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
#include "Platform.h"

#include "ErrorHandling.h"		// Errors::ThrowIfNULL
#include "Miscellaneous.h"		// StMemory
//#include "PreferenceMgr.h"
#include "ResStrings.h"
#include "SessionFile.h"
#include "Strings.r.h"			// kStr_ ...

#include <errno.h>				// EPERM, ENOENT, etc.
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>			// mkdir
#include <time.h>
#include <ctype.h>

#include "omnithread.h"			// omni_mutex
#include "PHEMNativeIF.h"			// omni_mutex

/* Update for GCC 4 */
#include <stdlib.h>
#include <string.h>

// ===========================================================================
//		¥ Globals
// ===========================================================================

ByteList		gClipboardDataPalm;
ByteList		gClipboardDataHost;
omni_mutex		gClipboardMutex;
omni_condition	gClipboardCondition (&gClipboardMutex);
Bool			gClipboardHaveOutgoingData;
Bool			gClipboardNeedIncomingData;
Bool			gClipboardPendingIncomingData;
Bool			gClipboardHaveIncomingData;

long long PrvGetMicroseconds (void)
{
	struct timeval tv;
	gettimeofday (&tv, NULL);

	long long usecs = ((long long) tv.tv_sec) * 1000000ULL + tv.tv_usec;

	return usecs;
}


// ===========================================================================
//		¥ Platform
// ===========================================================================

#ifndef __QNXNTO__
// Compare lexigraphically two strings

int _stricmp( const char *s1, const char *s2 )
{
	return strcasecmp( s1, s2 );
}

int _strnicmp( const char *s1, const char *s2, int n )
{
	return strncasecmp( s1, s2, n );
}

char* _strdup( const char *s )
{
	return strdup( s );
}
#endif


// ---------------------------------------------------------------------------
//		¥ Platform::Initialize
// ---------------------------------------------------------------------------
// Initializes platform-dependent stuff.

void Platform::Initialize( void )
{
}


// ---------------------------------------------------------------------------
//		¥ Platform::Reset
// ---------------------------------------------------------------------------

void Platform::Reset( void )
{
}


// ---------------------------------------------------------------------------
//		¥ Platform::Save
// ---------------------------------------------------------------------------

void Platform::Save(SessionFile&)
{
}


// ---------------------------------------------------------------------------
//		¥ Platform::Load
// ---------------------------------------------------------------------------

void Platform::Load(SessionFile&)
{
}


// ---------------------------------------------------------------------------
//		¥ Platform::Dispose
// ---------------------------------------------------------------------------

void Platform::Dispose( void )
{
}


// ---------------------------------------------------------------------------
//		¥ Platform::GetString
// ---------------------------------------------------------------------------

string Platform::GetString( StrCode id )
{
	const char* str = _ResGetString (id);
	if (str)
		return string (str);

	char	buffer[20];
	sprintf (buffer, "%ld", (long) id);
	return string ("<missing string ") + buffer + ">";
}


// ---------------------------------------------------------------------------
//		¥ Platform::GetIDForError
// ---------------------------------------------------------------------------

int Platform::GetIDForError( ErrCode error )
{
	switch (error)
	{
		// From /usr/include/asm/errno.h

	case EPERM:			break;		// 1	/* Operation not permitted */
	case ENOENT:		return kStr_FileNotFound;		// 2	/* No such file or directory */
	case ESRCH:			break;		// 3	/* No such process */
	case EINTR:			break;		// 4	/* Interrupted system call */
	case EIO:			return kStr_IOError;			// 5	/* I/O error */
	case ENXIO:			break;		// 6	/* No such device or address */
	case E2BIG:			break;		// 7	/* Arg list too long */
	case ENOEXEC:		break;		// 8	/* Exec format error */
	case EBADF:			break;		// 9	/* Bad file number */
	case ECHILD:		break;		// 10	/* No child processes */
	case EAGAIN:		break;		// 11	/* Try again */
	case ENOMEM:		return kStr_MemFull;			// 12	/* Out of memory */
	case EACCES:		return kStr_SerialPortBusy;		// 13	/* Permission denied */
	case EFAULT:		break;		// 14	/* Bad address */
	case ENOTBLK:		break;		// 15	/* Block device required */
	case EBUSY:			return kStr_FileBusy;			// 16	/* Device or resource busy */
	case EEXIST:		return kStr_DuplicateFileName;	// 17	/* File exists */
	case EXDEV:			break;		// 18	/* Cross-device link */
	case ENODEV:		return kStr_DiskMissing;		// 19	/* No such device */
	case ENOTDIR:		break;		// 20	/* Not a directory */
	case EISDIR:		break;		// 21	/* Is a directory */
	case EINVAL:		break;		// 22	/* Invalid argument */
	case ENFILE:		break;		// 23	/* File table overflow */
	case EMFILE:		return kStr_TooManyFilesOpen;	// 24	/* Too many open files */
	case ENOTTY:		break;		// 25	/* Not a typewriter */
	case ETXTBSY:		return kStr_FileBusy;			// 26	/* Text file busy */
	case EFBIG:			break;		// 27	/* File too large */
	case ENOSPC:		return kStr_DiskFull;			// 28	/* No space left on device */
	case ESPIPE:		break;		// 29	/* Illegal seek */
	case EROFS:			return kStr_DiskWriteProtected;	// 30	/* Read-only file system */
	case EMLINK:		break;		// 31	/* Too many links */
	case EPIPE:			break;		// 32	/* Broken pipe */
	case EDOM:			break;		// 33	/* Math argument out of domain of func */
	case ERANGE:		break;		// 34	/* Math result not representable */
	case EDEADLK:		break;		// 35	/* Resource deadlock would occur */
	case ENAMETOOLONG:	return kStr_BadFileName;		// 36	/* File name too long */

#if 0
		// Comment out this whole block.  We don't map them to any specific
		// error messages, and by commenting them out, we protect ourselves
		// against any Unixen that don't define them.

	case ENOLCK:		break;		// 37	/* No record locks available */
	case ENOSYS:		break;		// 38	/* Function not implemented */
	case ENOTEMPTY:		break;		// 39	/* Directory not empty */
	case ELOOP:			break;		// 40	/* Too many symbolic links encountered */
//	case EWOULDBLOCK:	break;		// EAGAIN	/* Operation would block */
	case ENOMSG:		break;		// 42	/* No message of desired type */
	case EIDRM:			break;		// 43	/* Identifier removed */
	case ECHRNG:		break;		// 44	/* Channel number out of range */
	case EL2NSYNC:		break;		// 45	/* Level 2 not synchronized */
	case EL3HLT:		break;		// 46	/* Level 3 halted */
	case EL3RST:		break;		// 47	/* Level 3 reset */
	case ELNRNG:		break;		// 48	/* Link number out of range */
	case EUNATCH:		break;		// 49	/* Protocol driver not attached */
	case ENOCSI:		break;		// 50	/* No CSI structure available */
	case EL2HLT:		break;		// 51	/* Level 2 halted */
	case EBADE:			break;		// 52	/* Invalid exchange */
	case EBADR:			break;		// 53	/* Invalid request descriptor */
	case EXFULL:		break;		// 54	/* Exchange full */
	case ENOANO:		break;		// 55	/* No anode */
	case EBADRQC:		break;		// 56	/* Invalid request code */
	case EBADSLT:		break;		// 57	/* Invalid slot */

//	case EDEADLOCK:		break;		// EDEADLK

	case EBFONT:		break;		// 59	/* Bad font file format */
	case ENOSTR:		break;		// 60	/* Device not a stream */
	case ENODATA:		break;		// 61	/* No data available */
	case ETIME:			break;		// 62	/* Timer expired */
	case ENOSR:			break;		// 63	/* Out of streams resources */
	case ENONET:		break;		// 64	/* Machine is not on the network */
	case ENOPKG:		break;		// 65	/* Package not installed */
	case EREMOTE:		break;		// 66	/* Object is remote */
	case ENOLINK:		break;		// 67	/* Link has been severed */
	case EADV:			break;		// 68	/* Advertise error */
	case ESRMNT:		break;		// 69	/* Srmount error */
	case ECOMM:			break;		// 70	/* Communication error on send */
	case EPROTO:		break;		// 71	/* Protocol error */
	case EMULTIHOP:		break;		// 72	/* Multihop attempted */
	case EDOTDOT:		break;		// 73	/* RFS specific error */
	case EBADMSG:		break;		// 74	/* Not a data message */
	case EOVERFLOW:		break;		// 75	/* Value too large for defined data type */
	case ENOTUNIQ:		break;		// 76	/* Name not unique on network */
	case EBADFD:		break;		// 77	/* File descriptor in bad state */
	case EREMCHG:		break;		// 78	/* Remote address changed */
	case ELIBACC:		break;		// 79	/* Can not access a needed shared library */
	case ELIBBAD:		break;		// 80	/* Accessing a corrupted shared library */
	case ELIBSCN:		break;		// 81	/* .lib section in a.out corrupted */
	case ELIBMAX:		break;		// 82	/* Attempting to link in too many shared libraries */
	case ELIBEXEC:		break;		// 83	/* Cannot exec a shared library directly */
	case EILSEQ:		break;		// 84	/* Illegal byte sequence */
	case ERESTART:		break;		// 85	/* Interrupted system call should be restarted */
	case ESTRPIPE:		break;		// 86	/* Streams pipe error */
	case EUSERS:		break;		// 87	/* Too many users */
	case ENOTSOCK:		break;		// 88	/* Socket operation on non-socket */
	case EDESTADDRREQ:	break;		// 89	/* Destination address required */
	case EMSGSIZE:		break;		// 90	/* Message too long */
	case EPROTOTYPE:	break;		// 91	/* Protocol wrong type for socket */
	case ENOPROTOOPT:	break;		// 92	/* Protocol not available */
	case EPROTONOSUPPORT:	break;		// 93	/* Protocol not supported */
	case ESOCKTNOSUPPORT:	break;		// 94	/* Socket type not supported */
	case EOPNOTSUPP:	break;		// 95	/* Operation not supported on transport endpoint */
	case EPFNOSUPPORT:	break;		// 96	/* Protocol family not supported */
	case EAFNOSUPPORT:	break;		// 97	/* Address family not supported by protocol */
	case EADDRINUSE:	break;		// 98	/* Address already in use */
	case EADDRNOTAVAIL:	break;		// 99	/* Cannot assign requested address */
	case ENETDOWN:		break;		// 100	/* Network is down */
	case ENETUNREACH:	break;		// 101	/* Network is unreachable */
	case ENETRESET:		break;		// 102	/* Network dropped connection because of reset */
	case ECONNABORTED:	break;		// 103	/* Software caused connection abort */
	case ECONNRESET:	break;		// 104	/* Connection reset by peer */
	case ENOBUFS:		break;		// 105	/* No buffer space available */
	case EISCONN:		break;		// 106	/* Transport endpoint is already connected */
	case ENOTCONN:		break;		// 107	/* Transport endpoint is not connected */
	case ESHUTDOWN:		break;		// 108	/* Cannot send after transport endpoint shutdown */
	case ETOOMANYREFS:	break;		// 109	/* Too many references: cannot splice */
	case ETIMEDOUT:		break;		// 110	/* Connection timed out */
	case ECONNREFUSED:	break;		// 111	/* Connection refused */
	case EHOSTDOWN:		break;		// 112	/* Host is down */
	case EHOSTUNREACH:	break;		// 113	/* No route to host */
	case EALREADY:		break;		// 114	/* Operation already in progress */
	case EINPROGRESS:	break;		// 115	/* Operation now in progress */
	case ESTALE:		break;		// 116	/* Stale NFS file handle */
	case EUCLEAN:		break;		// 117	/* Structure needs cleaning */
	case ENOTNAM:		break;		// 118	/* Not a XENIX named type file */
	case ENAVAIL:		break;		// 119	/* No XENIX semaphores available */
	case EISNAM:		break;		// 120	/* Is a named type file */
	case EREMOTEIO:		break;		// 121	/* Remote I/O error */
	case EDQUOT:		break;		// 122	/* Quota exceeded */

	case ENOMEDIUM:	break;		// 123	/* No medium found */
	case EMEDIUMTYPE:	break;		// 124	/* Wrong medium type */
#endif
	}

	return 0;
}


// ---------------------------------------------------------------------------
//		¥ Platform::GetIDForRecovery
// ---------------------------------------------------------------------------

int Platform::GetIDForRecovery( ErrCode error )
{
	return 0;
}


// ---------------------------------------------------------------------------
//		¥ Platform::GetShortVersionString
// ---------------------------------------------------------------------------
// Returns a short version string.	The format of the string is:
//
//		#.# (.#) ([dab]#)
//
//		# = one or more numeric digits
//		. = literal '.'
//		Patterns in parentheses are optional
//		Patterns in brackets mean "one of these characters"
//		Spaces are shown above for clarity; they do not appear in the string
//
// Valid strings would be: 2.1d7, 2.1.1b14, 2.99, 2.1.1

string Platform::GetShortVersionString( void )
{
	return string("3.5");
}


/***********************************************************************
 *
 * FUNCTION:	Platform::CopyToClipboard
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Platform::CopyToClipboard (const ByteList& palmChars,
								const ByteList& hostChars)
{
        PHEM_Log_Msg("Platform::CopyToClipboard");
	ByteList	palmChars2 (palmChars);
	ByteList	hostChars2 (hostChars);

	// See if any mapping needs to be done.

	if (hostChars2.size () > 0 && palmChars2.size () == 0)
	{
		Platform::RemapHostToPalmChars (hostChars2, palmChars2);
	}
	else if (palmChars2.size () > 0 && hostChars2.size () == 0)
	{
		Platform::RemapPalmToHostChars (palmChars2, hostChars2);
	}

	omni_mutex_lock lock (gClipboardMutex);

	gClipboardDataPalm = palmChars2;
	gClipboardDataHost = hostChars2;

	gClipboardHaveOutgoingData = true;

        char *tmp_buf = (char *)calloc(sizeof(char), gClipboardDataHost.size()+1);
        ByteList::const_iterator iter = gClipboardDataHost.begin();
        int i=0;
        while (iter != gClipboardDataHost.end()) {
           tmp_buf[i++] = *iter++;
        }
        
        PHEM_Log_Msg("Calling PHEM_Set_Host_Clip");
        PHEM_Set_Host_Clip(tmp_buf);
        free(tmp_buf);
}


/***********************************************************************
 *
 * FUNCTION:	Platform::CopyFromClipboard
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Platform::CopyFromClipboard (ByteList& palmChars,
								  ByteList& hostChars)
{
        PHEM_Log_Msg("Platform::CopyFromClipboard");
	omni_mutex_lock lock (gClipboardMutex);

        // Copy the data to the host data clipboard.  The Palm-specific
        // clipboard will remain empty, and the host data will get
        // convert to it on demand.

	gClipboardHaveIncomingData = true;
#if 0
	gClipboardNeedIncomingData = true;
	gClipboardHaveIncomingData = false;

	while (!gClipboardHaveIncomingData)
		gClipboardCondition.wait ();

#else
        gClipboardDataPalm.clear();
        gClipboardDataHost.clear();

        PHEM_Log_Msg("Calling PHEM_Get_Host_Clip");
        const char *host_clip = PHEM_Get_Host_Clip();
        if (host_clip) {
          PHEM_Log_Msg(host_clip);
          copy (host_clip, host_clip + strlen(host_clip), 
            back_inserter (gClipboardDataHost));
        } else {
          PHEM_Log_Msg("Null host clip");
        }
#endif

	// Copy the data to our outgoing lists.

	palmChars = gClipboardDataPalm;
	hostChars = gClipboardDataHost;

	// See if any mapping needs to be done.

	if (hostChars.size () > 0 && palmChars.size () == 0)
	{
		Platform::RemapHostToPalmChars (hostChars, palmChars);
	}
	else if (palmChars.size () > 0 && hostChars.size () == 0)
	{
		Platform::RemapPalmToHostChars (palmChars, hostChars);
	}
}


/***********************************************************************
 *
 * FUNCTION:	Platform::RemapHostToPalmChars
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Platform::RemapHostToPalmChars	(const ByteList& hostChars,
									 ByteList& palmChars)
{
	// Converting line endings is all we do for now.

	ByteList::const_iterator	iter = hostChars.begin ();
	while (iter != hostChars.end ())
	{
		uint8	ch = *iter++;

		if (ch == 0x0A)
		{
			palmChars.push_back (chrLineFeed);
		}
		else
		{
			palmChars.push_back (ch);
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	Platform::RemapHostToPalmChars
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Platform::RemapPalmToHostChars	(const ByteList& palmChars,
									 ByteList& hostChars)
{
	// Converting line endings is all we do for now.

	ByteList::const_iterator	iter = palmChars.begin ();
	while (iter != palmChars.end ())
	{
		uint8	ch = *iter++;

		if (ch == chrLineFeed)
		{
			hostChars.push_back (0x0A);
		}
		else
		{
			hostChars.push_back (ch);
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	Platform::PinToScreen
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	True if the rectangle was changed.
 *
 ***********************************************************************/

Bool Platform::PinToScreen (EmRect& r)
{
	// !!! TBD
	return false;
}


/***********************************************************************
 *
 * FUNCTION:	ToHostEOL
 *
 * DESCRIPTION:	Converts a string of characters into another string
 *				where the EOL sequence is consistant for files on the
 *				current platform.
 *
 * PARAMETERS:	dest - receives the result.	 The buffer is sized by
 *					this function, so the caller doesn't have to
 *					allocate any space itself.
 *
 *				destLen - receives the length of the resulting string.
 *
 *				src - pointer to input characters.
 *
 *				srcLen - number of characters pointed to by src.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Platform::ToHostEOL (	StMemory& dest, long& destLen,
				const char* src, long srcLen)
{
	char*	d = (char*) Platform::AllocateMemory (srcLen);
	char*	p = d;
	Bool	previousWas0x0D = false;

	for (long ii = 0; ii < srcLen; ++ii)
	{
		char	ch = src[ii];

		// Convert 0x0D to 0x0A.
		
		if (ch == 0x0D)
		{
			*p++ = 0x0A;
		}

		// If we're looking at a 0x0A that's part of
		// a 0x0D/0x0A, just swallow it.

		else if (ch == 0x0A && previousWas0x0D)
		{
			// Nothing
		}

		// Copy all other characters straight through.

		else
		{
			*p++ = ch;
		}

		previousWas0x0D = ch == 0x0D;
	}

	destLen = p - d;
	d = (char*) Platform::ReallocMemory (d, destLen);
	dest.Adopt (d);
}


// -----------------------------------------------------------------------------
// find the ROM file path embedded in the saved ram image

Bool Platform::ReadROMFileReference (ChunkFile& docFile, EmFileRef& f)
{
	// First look for a ROM file path.

	string	path;
	if (docFile.ReadString (SessionFile::kROMUnixPathTag, path))
	{
		f = EmFileRef (path);
		return true;
	}

	// If a path can't be found, look for a simple ROM name.

	string	name;
	if (docFile.ReadString (SessionFile::kROMNameTag, name))
	{
		// !!! TBD
	}

	return false;
}

void Platform::WriteROMFileReference (ChunkFile& docFile, const EmFileRef& f)
{
	docFile.WriteString (SessionFile::kROMUnixPathTag, f.GetFullPath ());
}


// ---------------------------------------------------------------------------
//		¥ Platform::Delay
// ---------------------------------------------------------------------------
// Delay for a time period appropriate for a sleeping 68328.

void Platform::Delay (void)
{
	// Delay 10 msecs.	Delaying by this amount pauses us 1/100 of a second,
	// which is the rate at which the device's tickcount counter increments.
	//
	// Wait on an event instead of just calling Sleep(10) so that another
	// thread can wake us up before our time.

	omni_thread::sleep( 0, 10000 ); // 10k nanoseconds = 1/100 sec
}


// ---------------------------------------------------------------------------
//		¥ Platform::CycleSlowly
// ---------------------------------------------------------------------------

void Platform::CycleSlowly (void)
{
	// Nothing to do in Unix.
}


// ---------------------------------------------------------------------------
//		¥ Platform::RealAllocateMemory
// ---------------------------------------------------------------------------

void* Platform::RealAllocateMemory (size_t size, Bool clear, const char*, int)
{
	void*	result;

	if (clear)
		result = calloc (size, 1);
	else
		result = malloc (size);

	Errors::ThrowIfNULL (result);

	return result;
}


// ---------------------------------------------------------------------------
//		¥ Platform::RealReallocMemory
// ---------------------------------------------------------------------------

void* Platform::RealReallocMemory (void* p, size_t size, const char*, int)
{
	void*	result = realloc (p, size);

	Errors::ThrowIfNULL (result);

	return result;
}


// ---------------------------------------------------------------------------
//		¥ Platform::RealDisposeMemory
// ---------------------------------------------------------------------------

void Platform::RealDisposeMemory (void* p)
{
	if (p)
	{
		free (p);
	}
}


/***********************************************************************
 *
 * FUNCTION:	Platform::ForceStartupScreen
 *
 * DESCRIPTION:	See if the user has requested that the Startup dialog
 *				be presented instead of attempting to use the latest
 *				session file or creation settings.
 *
 *				The current signal is to toggle the CAPSLOCK key.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	True if the user has signalled that the dialog should
 *				be presented.
 *
 ***********************************************************************/

Bool Platform::ForceStartupScreen (void)
{
	return false;
}


// ---------------------------------------------------------------------------
//		¥ Platform::StopOnResetKeyDown
// ---------------------------------------------------------------------------

Bool Platform::StopOnResetKeyDown( void )
{
	// Comment this out for now.  It seems that Windows doesn't always return
	// the expected result.	 That is, this function often returns TRUE even
	// though the Control is not down.
	//
	// Since this functionality is really only required by Palm OS engineers,
	// and since they're working on Macs, we don't really need this feature
	// in the Windows version now anyway.

//	return ::GetAsyncKeyState (VK_CONTROL) != 0;
	return false;
}


// ---------------------------------------------------------------------------
//		¥ Platform::CollectOptions
// ---------------------------------------------------------------------------

int Platform::CollectOptions (int argc, char** argv, int& errorArg, int (*cb)(int, char**, int&))
{
  int i=1, last_i=1;

  PHEM_Log_Msg("Plat:CollOpt...");
  PHEM_Log_Place(argc);
  if (argc) {
    PHEM_Log_Msg("arg:");
    PHEM_Log_Msg(argv[0]);
  }
  while (i < argc) {
    if (!cb(argc, argv, i)) {
      PHEM_Log_Msg("option not recognized!");
      PHEM_Log_Msg(argv[i]);
      return false;
    }
    if (i == last_i) {
     PHEM_Log_Msg("i not incremented!");
     PHEM_Log_Place(i);
     return false;
    } else {
     last_i = i;
    }
  }
#if 0
//AndroidTODO
	if (!Fl::args (argc, argv, errorArg, cb) || errorArg < argc - 1)
		return false;
#endif

        PHEM_Log_Msg("Plat:CollOpt returning true.");
	return true;
}


// ---------------------------------------------------------------------------
//		¥ Platform::PrintHelp
// ---------------------------------------------------------------------------

void Platform::PrintHelp (void)
{
#if 0
//AndroidTODO
	printf ("%s\n", Fl::help);
#endif
}


// ---------------------------------------------------------------------------
//		¥ Platform::GetMilliseconds
// ---------------------------------------------------------------------------

uint32 Platform::GetMilliseconds( void )
{
	long long usecs = ::PrvGetMicroseconds ();
	uint32   millis = (uint32) (usecs / 1000);

	return millis;
}


// ---------------------------------------------------------------------------
//		¥ Platform::CreateDebuggerSocket
// ---------------------------------------------------------------------------

CSocket* Platform::CreateDebuggerSocket (void)
{
	return NULL;
}


// ---------------------------------------------------------------------------
//		¥ Platform::ExitDebugger
// ---------------------------------------------------------------------------
//	Perform platform-specific operations when debug mode is exited.

void Platform::ExitDebugger( void )
{
}


// ---------------------------------------------------------------------------
//		¥ Platform::ViewDrawLine
// ---------------------------------------------------------------------------

void Platform::ViewDrawLine( int xStart, int yStart, int xEnd, int yEnd )
{
}


// ---------------------------------------------------------------------------
//		¥ Platform::ViewDrawPixel
// ---------------------------------------------------------------------------

void Platform::ViewDrawPixel( int xPos, int yPos )
{
}


static void PrvQueueNote (int frequency, int duration, int amplitude)
{
  //PHEM_Log_Msg("Platform::PrvQueueNote");
  //if (amplitude) {
    // Only send the sound on to the host if it actually is a sound that
    // can be, y'know, heard.
    PHEM_Queue_Sound(frequency, duration, amplitude);
  //} else {
    //PHEM_Log_Msg("PrvQueueNote: Not queueing silence.");
  //}
}


CallROMType Platform::SndDoCmd (SndCommandType& cmd)
{
  //PHEM_Log_Msg("Platform::SndDoCmd");
	switch (cmd.cmd)
	{
		case sndCmdFreqDurationAmp:
			PrvQueueNote (cmd.param1, cmd.param2, cmd.param3);
			break;

		case sndCmdNoteOn:
			return kExecuteROM;

		case sndCmdFrqOn:
			PrvQueueNote (cmd.param1, cmd.param2, cmd.param3);
			break;

		case sndCmdQuiet:
			return kExecuteROM;
	}

	return kSkipROM;
}

void Platform::StopSound (void)
{
}


void Platform::Beep (void)
{
  PHEM_Log_Msg("Beep");
  PHEM_Queue_Sound(880, 40, 64);
}
