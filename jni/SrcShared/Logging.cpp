/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "Logging.h"

#include "EmApplication.h"		// gApplication, IsBound
#include "EmMemory.h"			// EmMemGet32, EmMemGet16, EmMem_strcpy, EmMem_strncat
#include "EmStreamFile.h"		// EmStreamFile
#include "Hordes.h"				// Hordes::IsOn, Hordes::EventCounter
#include "Platform.h"			// GetMilliseconds
#include "PreferenceMgr.h"		// Preference<>
#include "ROMStubs.h"			// FrmGetTitle, WinGetFirstWindow
#include "Strings.r.h"			// kStr_LogFileSize
#include "StringData.h"			// virtual key descriptions

#include <ctype.h>				// isprint
#include <cstddef>

//#define LOG_TO_TRACE

#ifdef LOG_TO_TRACE
#include "TracerPlatform.h"
#endif

static LogStream*	gStdLog;
uint8				gLogCache[kCachedPrefKeyDummy];


LogStream*	LogGetStdLog (void)
{
	return gStdLog;
}


static void PrvPrefsChanged (PrefKeyType key, void*)
{
#define UPDATE_ONE_PREF(name)								\
	if (::PrefKeysEqual (key, kPrefKey##name))				\
	{														\
		Preference<uint8>	pref(kPrefKey##name, false);	\
		gLogCache[kCachedPrefKey##name] = *pref;			\
	}

	FOR_EACH_SCALAR_PREF (UPDATE_ONE_PREF)
}


void LogStartup (void)
{
#define REGISTER_ONE_PREF(name)	\
	gPrefs->AddNotification (PrvPrefsChanged, kPrefKey##name);	\
	PrvPrefsChanged (kPrefKey##name, NULL);

#define HARDCODE_ONE_PREF(name)	\
	gLogCache[kCachedPrefKey##name] = 0;

	if (gApplication->IsBound ())
	{
		FOR_EACH_SCALAR_PREF (HARDCODE_ONE_PREF)
	}
	else
	{
		FOR_EACH_SCALAR_PREF (REGISTER_ONE_PREF)
	}

	EmAssert (gStdLog == NULL);
	gStdLog = new LogStream ("Log");
}


void LogShutdown (void)
{
#define UNREGISTER_ONE_PREF(name)	\
	gPrefs->RemoveNotification (PrvPrefsChanged);

	FOR_EACH_SCALAR_PREF (UNREGISTER_ONE_PREF)

	EmAssert (gStdLog != NULL);
	delete gStdLog;	// Dumps it to a file, too.
	gStdLog = NULL;
}


// ---------------------------------------------------------------------------
//		¥ CLASS LogStream
// ---------------------------------------------------------------------------

const long		kFindUniqueFile			= -1;
const uint32	kInvalidTimestamp		= (uint32) -1;
const int32		kInvalidGremlinCounter	= -2;
const long		kEventTextMaxLen		= 255;


/***********************************************************************
 *
 * FUNCTION:	LogStream::LogStream
 *
 * DESCRIPTION:	Constructor.
 *
 * PARAMETERS:	baseName - base name to use for the file the data gets
 *					written to.  This base name will be prepended with
 *					the path of the directory to write the file to, and
 *					appended with "%04d.txt", where %04d will be
 *					replaced with a number to make sure the file's
 *					name is unique.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

LogStream::LogStream (const char* baseName) :
	fMutex (),
	fInner (baseName)
{
	gPrefs->AddNotification (PrefChanged, kPrefKeyLogFileSize, this);
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::~LogStream
 *
 * DESCRIPTION:	Destructor.  Writes any buffered text to the file and
 *				closes the file.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

LogStream::~LogStream (void)
{
	gPrefs->RemoveNotification (PrefChanged);

	omni_mutex_lock lock (fMutex);
	fInner.DumpToFile ();
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::Printf
 *
 * DESCRIPTION:	A printf-like function for adding text to the log file.
 *				The text is preceded by a timestamp, and is suffixed
 *				with a newline.
 *
 * PARAMETERS:	fmt - a printf-like string for formatting the output
 *				text.
 *
 *				... - additional printf-like parameters.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

int LogStream::Printf (const char* fmt, ...)
{
	int		n;
	va_list	arg;

	omni_mutex_lock	lock (fMutex);

	va_start (arg, fmt);

	n = fInner.VPrintf (fmt, arg);

	va_end (arg);

//	fInner.DumpToFile ();
	return n;
}


int LogStream::PrintfNoTime (const char* fmt, ...)
{
	int		n;
	va_list	arg;

	omni_mutex_lock	lock (fMutex);

	va_start (arg, fmt);

	n = fInner.VPrintf (fmt, arg, false);

	va_end (arg);

//	fInner.DumpToFile ();
	return n;
}



/***********************************************************************
 *
 * FUNCTION:	LogStream::DataPrintf
 *
 * DESCRIPTION:	A printf-like function for adding text to the log file.
 *				The text is preceded by a timestamp, and is suffixed
 *				with a newline.
 *
 * PARAMETERS:	data - binary data to be included in the output
 *
 *				dataLen - length of binary data
 *
 *				fmt - a printf-like string for formatting the output
 *				text.
 *
 *				... - additional printf-like parameters.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

int LogStream::DataPrintf (const void* data, long dataLen, const char* fmt, ...)
{
	omni_mutex_lock	lock (fMutex);

	int		n;
	va_list	arg;

	va_start (arg, fmt);

	n = fInner.VPrintf (fmt, arg);

	// Dump the data nicely formatted

	n += fInner.DumpHex (data, dataLen);

	return n;
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::VPrintf
 *
 * DESCRIPTION:	A vprintf-like function for adding text to the log file.
 *
 * PARAMETERS:	fmt - a vprintf-like string for formatting the output
 *				text.
 *
 *				args - additional vprintf-like parameters.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

int LogStream::VPrintf (const char* fmt, va_list args)
{
	omni_mutex_lock	lock (fMutex);

	return fInner.VPrintf (fmt, args);
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::Write
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

int LogStream::Write (const void* buffer, long size)
{
	omni_mutex_lock	lock (fMutex);

	return fInner.Write (buffer, size);
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::Clear
 *
 * DESCRIPTION:	Clear any currently logged data
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStream::Clear (void)
{
	omni_mutex_lock	lock (fMutex);

	fInner.Clear ();
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::GetLogSize
 *
 * DESCRIPTION:	Returns the maximum amount of text to be written to
 *				the log file.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	The maximum size.
 *
 ***********************************************************************/

long LogStream::GetLogSize (void)
{
	omni_mutex_lock	lock (fMutex);

	return fInner.GetLogSize ();
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::SetLogSize
 *
 * DESCRIPTION:	Sets the maximum amount of text to be written to the
 *				log file.  Any currently logged data is lost.
 *
 * PARAMETERS:	size - the new maximum value.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStream::SetLogSize (long size)
{
	omni_mutex_lock	lock (fMutex);

	fInner.SetLogSize (size);
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::EnsureNewFile
 *
 * DESCRIPTION:	Ensure that the logged data is written to a new file the
 *				next time DumpToFile is called.  Otherwise, the data
 *				will be written to the same file it was written to the
 *				previous time DumpToFile was called.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStream::EnsureNewFile (void)
{
	omni_mutex_lock	lock (fMutex);

	fInner.EnsureNewFile ();
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::DumpToFile
 *
 * DESCRIPTION:	Dumps any buffered text to the log file, prepending
 *				a message saying that only the last <mumble> bytes
 *				of text are buffered.
 *
 *				If no data has been logged (or has been discarded with
 *				a call to Clear), nothing is written out and no file is
 *				created.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStream::DumpToFile (void)
{
	omni_mutex_lock	lock (fMutex);

	fInner.DumpToFile ();
}


/***********************************************************************
 *
 * FUNCTION:	LogStream::PrefChanged
 *
 * DESCRIPTION:	Outputs and EOL to the log stream.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStream::PrefChanged (PrefKeyType, PrefRefCon data)
{
	Preference<long>	size (kPrefKeyLogFileSize);
	((LogStream*) data)->SetLogSize (*size);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::LogStreamInner
 *
 * DESCRIPTION:	Constructor.
 *
 * PARAMETERS:	baseName - base name to use for the file the data gets
 *					written to.  This base name will be prepended with
 *					the path of the directory to write the file to, and
 *					appended with "%04d.txt", where %04d will be
 *					replaced with a number to make sure the file's
 *					name is unique.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

LogStreamInner::LogStreamInner (const char* baseName) :
	fBaseName (baseName),
	fFileIndex (kFindUniqueFile),
	fBuffer (),
	fBufferSize (0),
	fDiscarded (false),
	fLastGremlinEventCounter (kInvalidGremlinCounter),
	fLastTimestampTime (kInvalidTimestamp),
	fBaseTimestampTime (kInvalidTimestamp)
{
	Preference<long>	size (kPrefKeyLogFileSize);
	fBufferSize = *size;

#ifdef LOG_TO_TRACE
	gTracer.InitOutputPort ();
#endif
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::~LogStreamInner
 *
 * DESCRIPTION:	Destructor.  Writes any buffered text to the file and
 *				closes the file.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

LogStreamInner::~LogStreamInner (void)
{
#ifdef LOG_TO_TRACE
	gTracer.CloseOutputPort ();
#else
	this->DumpToFile ();
#endif
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::DumpHex
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

int LogStreamInner::DumpHex (const void* data, long dataLen)
{
	int n = 0;
	const uint8*	dataP = (const uint8*) data;

	if (dataP && dataLen)
	{
		for (long ii = 0; ii < dataLen; ii += 16)
		{
			char	text[16 * 4 + 4];	// 16 bytes * (3 for hex + 1 for ASCII) + 2 tabs + 1 space + 1 NULL
			char*	p = text;

			*p++ = '\t';

			// Print up to 16 bytes of hex on the left
			long	jj;
			for (jj = ii; jj < ii + 16 ; ++jj)
			{
				if (jj < dataLen)
					p += sprintf (p, "%02X ", dataP[jj]);
				else
					p += sprintf (p, "   ");

				if (jj == ii + 7)
					p += sprintf (p, " ");

				EmAssert (p - text < (ptrdiff_t) sizeof (text));
			}

			// Print the ascii on the right
			*p++ = '\t';
			for (jj = ii; jj < ii + 16 && jj < dataLen; ++jj)
			{
				char	c = dataP[jj];
				if (!isprint(c))
					c = '.';
				*p++ = c;

				EmAssert (p - text < (ptrdiff_t) sizeof (text));
			}

			EmAssert (p - text <= (ptrdiff_t) sizeof (text));

			this->Write (text, p - text);
		}
	}	

	return n;
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::VPrintf
 *
 * DESCRIPTION:	A vprintf-like function for adding text to the log file.
 *
 * PARAMETERS:	fmt - a vprintf-like string for formatting the output
 *				text.
 *
 *				args - additional vprintf-like parameters.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

int LogStreamInner::VPrintf (const char* fmt, va_list args, Bool timestamp)
{
	char	buffer[2000];

	int n = vsprintf (buffer, fmt, args);

	// debug check, watch for buffer overflows here
	if (n < 0 || n >= (int) sizeof (buffer))
	{
		Platform::Debugger();
	}

	this->Write (buffer, n, timestamp);

	return n;
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::Write
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

int LogStreamInner::Write (const void* buffer, long size, Bool timestamp)
{
	if (timestamp)
		this->Timestamp ();

	this->Append ((const char*) buffer, size);
	this->NewLine ();

	return size;
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::Clear
 *
 * DESCRIPTION:	Clear any currently logged data
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::Clear (void)
{
	fBuffer.clear ();
	fBaseTimestampTime = kInvalidTimestamp;
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::GetLogSize
 *
 * DESCRIPTION:	Returns the maximum amount of text to be written to
 *				the log file.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	The maximum size.
 *
 ***********************************************************************/

long LogStreamInner::GetLogSize (void)
{
	return fBufferSize;
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::SetLogSize
 *
 * DESCRIPTION:	Sets the maximum amount of text to be written to the
 *				log file.  Any currently logged data is lost.
 *
 * PARAMETERS:	size - the new maximum value.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::SetLogSize (long size)
{
	fBufferSize = size;

	this->TrimLeading ();
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::EnsureNewFile
 *
 * DESCRIPTION:	Ensure that the logged data is written to a new file the
 *				next time DumpToFile is called.  Otherwise, the data
 *				will be written to the same file it was written to the
 *				previous time DumpToFile was called.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::EnsureNewFile (void)
{
	fFileIndex = kFindUniqueFile;
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::DumpToFile
 *
 * DESCRIPTION:	Dumps any buffered text to the log file, prepending
 *				a message saying that only the last <mumble> bytes
 *				of text are buffered.
 *
 *				If no data has been logged (or has been discarded with
 *				a call to Clear), nothing is written out and no file is
 *				created.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::DumpToFile (void)
{
	if (fBuffer.size () == 0)
		return;

	// Open the output stream.  No need to open it as a "text" file;
	// our Dump method does any host-specific conversion.

	EmFileRef		ref = this->CreateFileReference ();
	EmStreamFile	stream (ref, kCreateOrEraseForUpdate,
							kFileCreatorCodeWarrior, kFileTypeText);

	// If we lost data off the front print a message saying that only
	// the last portion of the text is begin saved/dumped.

	if (fDiscarded)
	{
		char	buffer[200];
		string	templ = Platform::GetString (kStr_LogFileSize);
		sprintf (buffer, templ.c_str (), fBufferSize / 1024L);
		this->DumpToFile (stream, buffer, strlen (buffer));
	}

	// Dump the text.

	const int	kChunkSize = 1 * 1024L * 1024L;
	StMemory	chunk (kChunkSize);
	ByteDeque::iterator	iter = fBuffer.begin ();

	while (iter != fBuffer.end ())
	{
		long	amtToCopy = kChunkSize;
		long	amtLeft = fBuffer.end () - iter;

		if (amtToCopy > amtLeft)
		{
			amtToCopy = amtLeft;
		}

		copy (iter, iter + amtToCopy, chunk.Get ());

		this->DumpToFile (stream, chunk.Get (), amtToCopy);

		iter += amtToCopy;
	}
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::DumpToFile
 *
 * DESCRIPTION:	Dumps the given text to the log file, converting any
 *				EOL characters along the way.
 *
 * PARAMETERS:	f - open file to write the text to.
 *
 *				s - text to write.
 *
 *				size - number of characters to write (the input text
 *					is not necessarily NULL terminated).
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::DumpToFile (EmStreamFile& f, const char* s, long size)
{
	StMemory	converted;
	long		convertedLength;

	Platform::ToHostEOL (converted, convertedLength, s, size);

	f.PutBytes (converted.Get (), convertedLength);
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::CreateFileReference
 *
 * DESCRIPTION:	Creates a file reference based on the base file name
 *				passed in to the constructor and the current fFileIndex.
 *				If fFileIndex is kFindUniqueFile, this routine attempts
 *				to find a file index that results in a new file being
 *				created.  Otherwise, the current fFileIndex is used to
 *				either open an existing file or create a new one.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	The desired EmFileRef.
 *
 ***********************************************************************/

EmFileRef LogStreamInner::CreateFileReference (void)
{
	EmFileRef	result;
	char		buffer[32];

	// Figure out where to put the log file.  If a Gremlin Horde is
	// running, then put the log file in the directory created to
	// hold Gremlin output files.  Otherwise, use the directory the
	// use specified in the preferences.  If no such directory was
	// specified, use the Emulator's directory.

	Preference<EmDirRef>	logDirPref (kPrefKeyLogDefaultDir);

	EmDirRef	defaultDir	= *logDirPref;
	EmDirRef	poserDir	= EmDirRef::GetEmulatorDirectory ();
	EmDirRef	gremlinDir	= Hordes::GetGremlinDirectory ();
	EmDirRef	logDir;

	if (Hordes::IsOn ())
	{
		logDir = gremlinDir;
	}
	else if (defaultDir.Create (), defaultDir.Exists ())
	{
		logDir = defaultDir;
	}
	else
	{
		logDir = poserDir;
	}

	// If being forced to write to a new file, look for an unused
	// file name.

	if (fFileIndex == kFindUniqueFile)
	{
		fFileIndex = 0;

		do
		{
			++fFileIndex;

			sprintf (buffer, "%s_%04ld.txt", fBaseName, fFileIndex);

			result = EmFileRef (logDir, buffer);
		}
		while (result.IsSpecified () && result.Exists ());
	}

	// Otherwise, use the previously-used file name.

	else
	{
		sprintf (buffer, "%s_%04ld.txt", fBaseName, fFileIndex);

		result = EmFileRef (logDir, buffer);
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::Timestamp
 *
 * DESCRIPTION:	Outputs a timestamp to the log stream.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::Timestamp (void)
{
	Bool	reformat = false;
	uint32	now = Platform::GetMilliseconds ();

	// This may be a case of pre-optimization, but we try to keep around
	// a formatted timestamp string for as long as possible.  If either
	// the time changes or the Gremlin event number changes, we force
	// the regeneration of the cached timestamp string.

	if (fLastTimestampTime != now)
		reformat = true;

	if (!reformat && Hordes::IsOn () && fLastGremlinEventCounter != Hordes::EventCounter ())
		reformat = true;

	if (reformat)
	{
		fLastTimestampTime = now;

		// We try to print out logged data with timestamps that are
		// relative to the first event recorded.

		if (fBaseTimestampTime == kInvalidTimestamp)
			fBaseTimestampTime = now;

		now -= fBaseTimestampTime;

		// If a Gremlin is running, use a formatting string that includes
		// the event number.  Otherwise, use a format string that omits it.

		if (Hordes::IsOn ())
		{
			fLastGremlinEventCounter = Hordes::EventCounter ();
			sprintf (fLastTimestampString, "%ld.%03ld (%ld):\t", now / 1000, now % 1000, fLastGremlinEventCounter);
		}
		else
		{
			sprintf (fLastTimestampString, "%ld.%03ld:\t", now / 1000, now % 1000);
		}
	}

	this->Append (fLastTimestampString, strlen (fLastTimestampString));
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::NewLine
 *
 * DESCRIPTION:	Outputs and EOL to the log stream.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::NewLine (void)
{
	this->Append ("\n", 1);
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::Append
 *
 * DESCRIPTION:	Generic function for adding text (actually, any kind of
 *				unformatted data) to the output stream.  If the amount
 *				of text in the buffer exceeds the maximum specified
 *				amount, any old text is deleted.  This function is
 *				the bottleneck for all such functions in this class.
 *
 * PARAMETERS:	buffer - pointer to the text to be added.
 *
 *				size - length of the text (in bytes) to be added.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::Append (const char* buffer, long size)
{
	if (size != 0)
	{
#ifdef LOG_TO_TRACE
		// Convert the text to a string

		string s (buffer, size);

		// Write out the string, breaking it up manually at
		// '\n's (OutputVT doesn't know that's what we're using
		// for line endings).

		string::size_type	pos = s.find ('\n');

		while (pos != string::npos)
		{
			string	substr = s.substr (0, pos) + " ";
			gTracer.OutputVTL (0, substr.c_str (), (va_list) NULL);

			s = s.substr (pos + 1);
			pos = s.find ('\n');
		}

		// If there's anything left, write it out without the CR.

		if (s.size () > 0)
		{
			gTracer.OutputVT (0, s.c_str (), (va_list) NULL);
		}
#else
		copy (buffer, buffer + size, back_inserter (fBuffer));

		this->TrimLeading ();
#endif
	}
}


/***********************************************************************
 *
 * FUNCTION:	LogStreamInner::TrimLeading
 *
 * DESCRIPTION:	If the buffer has exceeded the maximum size we've
 *				established for it, drop any leading characters.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void LogStreamInner::TrimLeading (void)
{
	long	amtToDiscard = fBuffer.size () - fBufferSize;

	if (amtToDiscard > 0)
	{
		fDiscarded = true;
		fBuffer.erase (fBuffer.begin (), fBuffer.begin () + amtToDiscard);

		// Keep chopping up to the next '\n' so that we don't leave
		// any partial lines.

		while (fBuffer.front () != '\n')
		{
			fBuffer.pop_front ();
		}

		fBuffer.pop_front ();
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ StubEmFrmGetTitle
// ---------------------------------------------------------------------------
// Returns a pointer to the title string of a form.  Copied from Form.c.

static string StubEmFrmGetTitle (const FormPtr frm)
{
	const Char*	title = FrmGetTitle (frm);

	if (title)
	{
		char	buffer[256];
		EmMem_strcpy (buffer, (emuptr) title);
		return string (buffer);
	}

	return string ("Untitled");
}


// ---------------------------------------------------------------------------
//		¥ StubEmPrintFormID
// ---------------------------------------------------------------------------
// Displays the form resource id associated with the window passed.

static void StubEmPrintFormID (WinHandle winHandle, char* desc, char* eventText)
{
	emuptr winPtr;
	emuptr exitWinPtr;

	// Allow access to WindowType fields windowFlags and nextWindow.

	CEnableFullAccess	munge;
	
	if (winHandle)
		{
		exitWinPtr = (emuptr) WinGetWindowPointer (winHandle);
		
		// Check if the handle is still valid.  If the form has been deleted 
		// then we can't dereference the window pointer.
		
		// Search the window list for the pointer.
		winHandle = WinGetFirstWindow ();
		while (winHandle)
			{
			winPtr = (emuptr) WinGetWindowPointer (winHandle);
			if (winPtr == exitWinPtr)
				break;
			
			winHandle = (WinHandle) EmMemGet32 (winPtr + offsetof (WindowType, nextWindow));
			}
		
		
		if (winHandle && /*winPtr->windowFlags.dialog*/
			((EmMemGet16 (winPtr + offsetof (WindowType, windowFlags)) & 0x0200) != 0))
			{
			string	title = StubEmFrmGetTitle((FormPtr) winPtr);
			if (!title.empty())
				sprintf (&eventText[strlen(eventText)],"%s: \"%s\"", desc, title.c_str());
			else
				sprintf (&eventText[strlen(eventText)],"%s ID: %ld", desc, /*frm->formId*/
						EmMemGet16 (winPtr + offsetof (FormType, formId)));
			}
		}
}



#define irObCloseChr 0x01FB			// to shut down Obex from background thread




static const char* StubEmKeyDescription (Int16 key)
{
	unsigned int	index;

	index = key - vchrLowBattery;

	if (index < gVirtualKeyDescriptionsCount)
		return kVirtualKeyDescriptions [index];

	index = key - irObCloseChr;

	if (index < gHardKeyDescriptionsCount)
		return kHardKeyDescriptions [index];

	return "";
}


// ---------------------------------------------------------------------------
//		¥ PrvGetEventText
// ---------------------------------------------------------------------------
// Displays the passed event in the emulator's event tracewindow if it is
// active.

static Bool PrvGetEventText(const EventType* eventP, char* eventText)
{
	long curLen = strlen (eventText);
	eventText += curLen;

	switch (eventP->eType)
	{
		case nilEvent:
			return false;
			//sprintf(eventText,"nilEvent");
			//break;
		
		case penDownEvent:
			sprintf(eventText,"penDownEvent    X:%d   Y:%d", 
					eventP->screenX, eventP->screenY);
			break;

		case penUpEvent:
			strcpy(eventText,"penUpEvent");
			sprintf(eventText,"penUpEvent      X:%d   Y:%d", 
					eventP->screenX, eventP->screenY);
			break;

		case penMoveEvent:
			strcpy(eventText,"penMoveEvent");
			sprintf(eventText,"penMoveEvent    X:%d   Y:%d", 
					eventP->screenX, eventP->screenY);					
			break;

		case keyDownEvent:
			if ((eventP->data.keyDown.chr < 0x0100) && isprint (eventP->data.keyDown.chr))
			{
				sprintf(eventText,"keyDownEvent    Key:'%c' 0x%02X%s,  Modifiers: 0x%04X", 
						(char) eventP->data.keyDown.chr, eventP->data.keyDown.chr,  
						StubEmKeyDescription(eventP->data.keyDown.chr),
						eventP->data.keyDown.modifiers);
			}
			else
			{
				sprintf(eventText,"keyDownEvent    Key:0x%02X%s,  Modifiers: 0x%04X", 
						eventP->data.keyDown.chr,  
						StubEmKeyDescription(eventP->data.keyDown.chr),
						eventP->data.keyDown.modifiers);
			}
			break;

		case winEnterEvent:
			sprintf(eventText,"winEnterEvent   Enter: %p   Exit: %p", 
					eventP->data.winEnter.enterWindow, eventP->data.winEnter.exitWindow);		
			StubEmPrintFormID (eventP->data.winEnter.enterWindow, "  Enter Form", eventText);
			StubEmPrintFormID (eventP->data.winEnter.exitWindow, "  Exit Form", eventText);
			break;

		case winExitEvent:
			sprintf(eventText,"winExitEvent    Enter: %p   Exit: %p", 
					eventP->data.winExit.enterWindow, eventP->data.winExit.exitWindow);
			StubEmPrintFormID (eventP->data.winExit.enterWindow, "  Enter Form", eventText);
			StubEmPrintFormID (eventP->data.winExit.exitWindow, "  Exit Form", eventText);
			break;

		case ctlEnterEvent:
			sprintf(eventText,"ctlEnterEvent   ID: %u", 
					eventP->data.ctlEnter.controlID);
			break;

		case ctlSelectEvent:
			sprintf(eventText,"ctlSelectEvent  ID: %u   On: %u", 
					eventP->data.ctlSelect.controlID, eventP->data.ctlSelect.on);					
			break;

		case ctlRepeatEvent:
			sprintf(eventText,"ctlRepeatEvent  ID: %u   Time: %lu", 
					eventP->data.ctlRepeat.controlID, eventP->data.ctlRepeat.time);					
			break;

		case ctlExitEvent:
			sprintf(eventText,"ctlExitEvent");
			break;

		case lstEnterEvent:
			sprintf(eventText,"lstEnterEvent   ID: %u   Item: %u", 
					eventP->data.lstEnter.listID, eventP->data.lstEnter.selection);			
			break;

		case lstSelectEvent:
			sprintf(eventText,"lstSelectEvent  ID: %u   Item: %u", 
					eventP->data.lstSelect.listID, eventP->data.lstSelect.selection);
			break;

		case lstExitEvent:
			sprintf(eventText,"lstExitEvent    ID: %u", 
					eventP->data.lstExit.listID);
			break;

		case popSelectEvent:
			sprintf(eventText,"popSelectEvent  CtlID: %u  ListID: %u  Item: %u", 
					eventP->data.popSelect.controlID, eventP->data.popSelect.listID,
					eventP->data.popSelect.selection);
			break;

		case fldEnterEvent:
			sprintf(eventText,"fldEnterEvent   ID: %u", 
					eventP->data.fldEnter.fieldID);
			break;

		case fldHeightChangedEvent:
			sprintf(eventText,"fldHeightChangedEvent  ID: %u  Height: %u  Pos: %u", 
					eventP->data.fldHeightChanged.fieldID, 
					eventP->data.fldHeightChanged.newHeight,
					eventP->data.fldHeightChanged.currentPos);
			break;

		case fldChangedEvent:
			sprintf(eventText,"fldChangedEvent  ID: %u", 
					eventP->data.fldChanged.fieldID);
			break;

		case tblEnterEvent:
			sprintf(eventText,"tblEnterEvent   ID: %u   Row: %u  Col: %u",
					eventP->data.tblEnter.tableID,
					eventP->data.tblEnter.row,
					eventP->data.tblEnter.column);
			break;

		case tblSelectEvent:
			sprintf(eventText,"tblSelectEvent  ID: %u   Row: %u  Col: %u", 
					eventP->data.tblSelect.tableID, 
					eventP->data.tblSelect.row, 
					eventP->data.tblSelect.column);					
			break;

		case tblExitEvent:
			sprintf(eventText,"tblExitEvent  ID: %u   Row: %u  Col: %u", 
					eventP->data.tblExit.tableID, 
					eventP->data.tblExit.row, 
					eventP->data.tblExit.column);					
			break;
		
		case daySelectEvent:
			strcpy(eventText,"daySelectEvent");
			break;

		case menuEvent:
			sprintf(eventText,"menuEvent       ItemID: %u", 
					eventP->data.menu.itemID);
			break;

		case appStopEvent:
			strcpy(eventText,"appStopEvent");
			break;

		case frmLoadEvent:
			sprintf(eventText,"frmLoadEvent    ID: %u", 
					eventP->data.frmOpen.formID);
			break;

		case frmOpenEvent:
			sprintf(eventText,"frmOpenEvent    ID: %u", 
					eventP->data.frmOpen.formID);
			break;

		case frmGotoEvent:
			sprintf(eventText,"frmGotoEvent    ID: %u  Record: %u  Field: %u", 
					eventP->data.frmGoto.formID,
					eventP->data.frmGoto.recordNum,
					eventP->data.frmGoto.matchFieldNum);
			break;

		case frmUpdateEvent:
			sprintf(eventText,"frmUpdateEvent    ID: %u", 
					eventP->data.frmUpdate.formID);
			break;

		case frmSaveEvent:
			sprintf(eventText,"frmSaveEvent");
			break;

		case frmCloseEvent:
			sprintf(eventText,"frmCloseEvent   ID: %u", 
					eventP->data.frmClose.formID);
			break;

		case frmTitleEnterEvent:
			sprintf(eventText,"frmTitleEnterEvent   ID: %u", 
					eventP->data.frmTitleEnter.formID);
			break;

		case frmTitleSelectEvent:
			sprintf(eventText,"frmTitleSelectEvent   ID: %u", 
					eventP->data.frmTitleSelect.formID);
			break;

		case sclEnterEvent:
			sprintf(eventText,"sclEnterEvent   ID: %u", 
					eventP->data.sclEnter.scrollBarID);
			break;

		case sclRepeatEvent:
			sprintf(eventText,"sclRepeatEvent   ID: %u  Value: %u,  New value: %u", 
					eventP->data.sclRepeat.scrollBarID,
					eventP->data.sclRepeat.value,
					eventP->data.sclRepeat.newValue);
			break;

		case sclExitEvent:
			sprintf(eventText,"sclExitEvent   ID: %u", 
					eventP->data.sclExit.scrollBarID);
			break;

		case tsmConfirmEvent:
			curLen += sprintf(eventText,"tsmConfirmEvent   ID: %u  Text: ", 
					eventP->data.tsmConfirm.formID);
			EmMem_strncat(eventText, (emuptr)eventP->data.tsmConfirm.yomiText, kEventTextMaxLen - curLen);
			eventText[kEventTextMaxLen] = 0;	// Make sure we're terminated
			break;
			
		case tsmFepButtonEvent:
			sprintf(eventText,"tsmFepButtonEvent   ID: %u", 
					eventP->data.tsmFepButton.buttonID);
			break;
		
		case tsmFepModeEvent:
			sprintf(eventText,"tsmFepModeEvent   ID: %u", 
					eventP->data.tsmFepMode.mode);
			break;

		case menuCmdBarOpenEvent:
			sprintf(eventText,"menuCmdBarOpenEvent	preventFieldButtons: %u",
				eventP->data.menuCmdBarOpen.preventFieldButtons);
			break;
				
		case menuOpenEvent:
			sprintf(eventText,"menuOpenEvent   RscID:%u, cause:%u",
				eventP->data.menuOpen.menuRscID,
				eventP->data.menuOpen.cause);
			break;
				
		case menuCloseEvent:
			sprintf(eventText,"menuCloseEvent");
			break;

		case frmGadgetEnterEvent:
			sprintf(eventText,"frmGadgetEnterEvent   RscID:%u, gadget:0x%08lX",
					eventP->data.gadgetEnter.gadgetID,
					(unsigned long) eventP->data.gadgetEnter.gadgetP);
			break;

		case frmGadgetMiscEvent:
			sprintf(eventText,"frmGadgetMiscEvent   ID:%u, gadget:0x%08lX, selector:%u",
					eventP->data.gadgetMisc.gadgetID,
					(unsigned long) eventP->data.gadgetMisc.gadgetP,
					eventP->data.gadgetMisc.selector);
			break;

		default:
			if (eventP->eType >= firstINetLibEvent && eventP->eType < firstWebLibEvent)
			{
				sprintf(eventText, "NetLib event #%u", eventP->eType);
			}
			else if (eventP->eType >= firstWebLibEvent && eventP->eType < firstWebLibEvent + 0x0100)
			{
				sprintf(eventText, "WebLib event #%u", eventP->eType);
			}
			else if (eventP->eType >= firstUserEvent)
			{
				sprintf(eventText, "Application event #%u", eventP->eType);
			}
			else
			{
				sprintf(eventText,"Unknown Event!   Event->eType #: %u",
						eventP->eType);
			}
			break;
	}

	return true;
}

// ---------------------------------------------------------------------------
//		¥ LogEvtAddEventToQueue
// ---------------------------------------------------------------------------

void LogEvtAddEventToQueue (const EventType& event)
{
	if (LogEnqueuedEvents ())
	{
		// Get the text for this event.  If there is such text, log it.

		char	eventText[kEventTextMaxLen] = " -> EvtAddEventToQueue: ";
		if (PrvGetEventText (&event, eventText))
		{
			LogAppendMsg (eventText);
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ LogEvtAddUniqueEventToQueue
// ---------------------------------------------------------------------------

void LogEvtAddUniqueEventToQueue (const EventType& event, UInt32, Boolean)
{
	if (LogEnqueuedEvents ())
	{
		// Get the text for this event.  If there is such text, log it.

		char	eventText[kEventTextMaxLen] = " -> EvtAddUniqueEventToQueue: ";
		if (PrvGetEventText (&event, eventText))
		{
			LogAppendMsg (eventText);
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ LogEvtEnqueueKey
// ---------------------------------------------------------------------------

void LogEvtEnqueueKey (UInt16 ascii, UInt16 keycode, UInt16 modifiers)
{
	if (LogEnqueuedEvents ())
	{
		if ((ascii < 0x0100) && isprint (ascii))
		{
			LogAppendMsg (" -> EvtEnqueueKey: ascii = '%c' 0x%04X, keycode = 0x%04X, modifiers = 0x%04X.",
					(char) ascii, ascii, keycode, modifiers);
		}
		else
		{
			LogAppendMsg (" -> EvtEnqueueKey: ascii = 0x%04X, keycode = 0x%04X, modifiers = 0x%04X.",
					ascii, keycode, modifiers);
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ LogEvtEnqueuePenPoint
// ---------------------------------------------------------------------------

void LogEvtEnqueuePenPoint (const PointType& pt)
{
	if (LogEnqueuedEvents ())
	{
		LogAppendMsg (" -> EvtEnqueuePenPoint: pen->x=%d, pen->y=%d.", pt.x, pt.y);
	}
}


// ---------------------------------------------------------------------------
//		¥ LogEvtGetEvent
// ---------------------------------------------------------------------------

void LogEvtGetEvent (const EventType& event, Int32 timeout)
{
	UNUSED_PARAM(timeout)

	if (LogDequeuedEvents ())
	{
		// Get the text for this event.  If there is such text, log it.

		char	eventText[kEventTextMaxLen] = "<-  EvtGetEvent: ";
		if (PrvGetEventText (&event, eventText))
		{
			LogAppendMsg (eventText);
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ LogEvtGetPen
// ---------------------------------------------------------------------------

void LogEvtGetPen (Int16 screenX, Int16 screenY, Boolean penDown)
{
	if (LogDequeuedEvents ())
	{
		static Int16	lastScreenX = -2;
		static Int16	lastScreenY = -2;
		static Boolean	lastPenDown = false;
		static long		numCollapsedEvents;

		if (screenX != lastScreenX ||
			screenY != lastScreenY ||
			penDown != lastPenDown)
		{
			lastScreenX = screenX;
			lastScreenY = screenY;
			lastPenDown = penDown;

			numCollapsedEvents = 0;

			LogAppendMsg ("<-  EvtGetPen: screenX=%d, screenY=%d, penDown=%d.",
					(int) screenX, (int) screenY, (int) penDown);
		}
		else
		{
			++numCollapsedEvents;
			if (numCollapsedEvents == 1)
				LogAppendMsg ("<-  EvtGetPen: <<<eliding identical events>>>.");
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ LogEvtGetSysEvent
// ---------------------------------------------------------------------------

void LogEvtGetSysEvent (const EventType& event, Int32 timeout)
{
	UNUSED_PARAM(timeout)

	if (LogDequeuedEvents ())
	{
		// Get the text for this event.  If there is such text, log it.

		char	eventText[kEventTextMaxLen] = "<-  EvtGetSysEvent: ";
		if (PrvGetEventText (&event, eventText))
		{
			LogAppendMsg (eventText);
		}
	}
}

