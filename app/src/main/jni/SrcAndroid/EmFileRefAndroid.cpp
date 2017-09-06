/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmFileRefAndroid.h"

#include "Miscellaneous.h"		// EndsWith
#include "Platform.h"			// stricmp
#include "PHEMNativeIF.h"

#include <errno.h>				// ENOENT
#include <sys/stat.h>
#include <unistd.h>

/* Update for GCC 4 */
#include <string.h>


static const char*	kExtension[] =
{
	NULL,		// kFileTypeNone,
	NULL,		// kFileTypeApplication
	".rom",		// kFileTypeROM,
	".psf",		// kFileTypeSession,
	".pev",		// kFileTypeEvents,
	".ini",		// kFileTypePreference,
	".prc",		// kFileTypePalmApp,
	".pdb",		// kFileTypePalmDB,
	".pqa",		// kFileTypePalmQA,
	".txt",		// kFileTypeText,
	NULL,		// kFileTypePicture,
	".skin",	// kFileTypeSkin,
	".prof",	// kFileTypeProfile,
	NULL,		// kFileTypePalmAll,
	NULL		// kFileTypeAll
};


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::EmFileRef
 *
 * DESCRIPTION:	Various ways to make a file reference.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

EmFileRef::EmFileRef (void) :
	fFilePath ()
{
	COMPILE_TIME_ASSERT(countof (kExtension) == kFileTypeLast);
}


EmFileRef::EmFileRef (const EmFileRef& other) :
	fFilePath (other.fFilePath)
{
}


EmFileRef::EmFileRef (const char* path) :
	fFilePath (path)
{
	this->MaybePrependCurrentDirectory ();
	this->MaybeNormalize ();
}


EmFileRef::EmFileRef (const string& path) :
	fFilePath (path)
{
	this->MaybePrependCurrentDirectory ();
	this->MaybeNormalize ();
}


EmFileRef::EmFileRef (const EmDirRef& parent, const char* path) :
	fFilePath (parent.GetFullPath () + path)
{
	this->MaybeNormalize ();
}


EmFileRef::EmFileRef (const EmDirRef& parent, const string& path) :
	fFilePath (parent.GetFullPath () + path)
{
	this->MaybeNormalize ();
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::EmFileRef
 *
 * DESCRIPTION:	EmFileRef destructor.  Nothing special to do...
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

EmFileRef::~EmFileRef (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::operator=
 *
 * DESCRIPTION:	Assignment operator.  If "other" is not the same as
 *				the controlled object, copy the contents.
 *
 * PARAMETERS:	other - object to copy.
 *
 * RETURNED:	reference to self.
 *
 ***********************************************************************/

EmFileRef&
EmFileRef::operator= (const EmFileRef& other)
{
	if (&other != this)
	{
		fFilePath = other.fFilePath;
	}

	return *this;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::IsSpecified
 *
 * DESCRIPTION:	Returns whether or not the controlled object has been
 *				pointed to a (possibly non-existant) file, or if it's
 *				empty (that it, it was created with the default ctor).
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	True if the object points to a file.
 *
 ***********************************************************************/

Bool
EmFileRef::IsSpecified (void) const
{
	return !fFilePath.empty ();
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::Exists
 *
 * DESCRIPTION:	Returns whether or not the controlled object points to
 *				an existing file.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	True if the referenced file exists.
 *
 ***********************************************************************/

Bool
EmFileRef::Exists (void) const
{
        PHEM_Log_Msg("FileRef:Exists...");
	if (this->IsSpecified ())
	{
                PHEM_Log_Msg(fFilePath.c_str());
		struct stat buf;
		int result = stat (fFilePath.c_str (), &buf);

                PHEM_Log_Place(result);
		return result == 0;
	}

        PHEM_Log_Msg("not specified!");
	return false;
}


/***********************************************************************
 *
 * FUNCTION:    EmFileRef::IsType
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

Bool
EmFileRef::IsType (EmFileType type) const
{
	if (fFilePath.size () > 4 &&
		kExtension[type] != NULL &&
		::EndsWith (fFilePath.c_str (), kExtension[type]))
	{
		return true;
	}

	// Add special hacks for ROM files.
	if (type == kFileTypeROM && ::StartsWith (fFilePath.c_str(), "rom."))
	{
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::SetCreatorAndType
 *
 * DESCRIPTION: Set the Finder type and creator information of the
 *				managed file.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmFileRef::SetCreatorAndType (EmFileCreator creator, EmFileType fileType) const
{
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::GetAttr
 *
 * DESCRIPTION: Get basic file attributes of the managed file.
 *
 * PARAMETERS:	A pointer to an integer where the mode bits will be stored.
 *
 * RETURNED:	An integer containing an errno style error result, 0 for no error.
 *
 ***********************************************************************/

int
EmFileRef::GetAttr (int * mode) const
{
	EmAssert(mode);
	
	*mode = 0;
	
	if (!IsSpecified())
		return ENOENT;
	
	struct stat stat_buf;
	if (stat(GetFullPath().c_str(), &stat_buf))
		return errno;
	
	if ((stat_buf.st_mode & S_IWUSR) == 0)
		*mode |= kFileAttrReadOnly;
	
	return 0;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::SetAttr
 *
 * DESCRIPTION: Set basic file attributes of the managed file.
 *
 * PARAMETERS:	An integer containing bits from the EmFileAttr enum.
 *
 * RETURNED:	An integer containing an errno style error result, 0 for no error.
 *
 ***********************************************************************/

int
EmFileRef::SetAttr (int mode) const
{
	if (!IsSpecified())
		return ENOENT;

	struct stat stat_buf;
	if (stat(GetFullPath().c_str(), &stat_buf))
		return errno;
	
	stat_buf.st_mode &= ~S_IWUSR;
	if (!(mode & kFileAttrReadOnly))
		stat_buf.st_mode |= S_IWUSR;
		
	if (chmod(GetFullPath().c_str(), stat_buf.st_mode))
		return errno;
	
	return 0;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::GetName
 *
 * DESCRIPTION:	Returns the name of the referenced file.  Only the file
 *				*name* is returned, not the full path.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	A string containing the name.  If the file is not
 *				specified, an empty string is returned.  No checks are
 *				made to see if the file actually exists.
 *
 ***********************************************************************/

string
EmFileRef::GetName (void) const
{
	string	result;

	if (this->IsSpecified ())
	{
                PHEM_Log_Msg("FileRef::GetName");
		string::size_type pos = fFilePath.rfind ('/', string::npos);
		EmAssert (pos != string::npos);

		result = fFilePath.substr (pos + 1, string::npos);
	}
        else {
                PHEM_Log_Msg("FileRef::GetName File wasn't specified!");
        }

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::GetParent
 *
 * DESCRIPTION:	Returns an object representing the parent (or container)
 *				of the managed file.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	An object representing the file's parent.
 *
 ***********************************************************************/

EmDirRef
EmFileRef::GetParent (void) const
{
	EmDirRef	result;

	if (this->IsSpecified () && fFilePath != "/")
	{
		string::size_type pos = fFilePath.rfind ('/', string::npos);
		EmAssert (pos != string::npos);

		result = EmDirRef (fFilePath.substr (0, pos + 1));
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::GetFullPath
 *
 * DESCRIPTION:	Get a full (platform-specific) path to the object.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	An string representing the file's path.
 *
 ***********************************************************************/

string
EmFileRef::GetFullPath (void) const
{
	return fFilePath;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::operator==
 * FUNCTION:	EmFileRef::operator!=
 * FUNCTION:	EmFileRef::operator>
 * FUNCTION:	EmFileRef::operator<
 *
 * DESCRIPTION:	Bogus operators for wiggy VC++ compiler which won't let
 *				us instantiate STL containers without them.
 *
 * PARAMETERS:	other - object to compare ourself to.
 *
 * RETURNED:	True if the requested condition is true.  Comparisons
 *				are based on the file's full path.
 *
 ***********************************************************************/

bool
EmFileRef::operator== (const EmFileRef& other) const
{
	return _stricmp (fFilePath.c_str (), other.fFilePath.c_str ()) == 0;
}


bool
EmFileRef::operator!= (const EmFileRef& other) const
{
	return _stricmp (fFilePath.c_str (), other.fFilePath.c_str ()) != 0;
}


bool
EmFileRef::operator> (const EmFileRef& other) const
{
	return _stricmp (fFilePath.c_str (), other.fFilePath.c_str ()) < 0;
}


bool
EmFileRef::operator< (const EmFileRef& other) const
{
	return _stricmp (fFilePath.c_str (), other.fFilePath.c_str ()) > 0;
}


/***********************************************************************
 *
 * FUNCTION:	FromPrefString
 *
 * DESCRIPTION:	Initialize this object from the string containing a file
 *				reference stored in a preference file.
 *
 * PARAMETERS:	s - the string from the preference file
 *
 * RETURNED:	True if we were able to carry out the initialization.
 *				False otherwise.  Note that the string is NOT validated
 *				to see if it refers to an existing file.
 *
 ***********************************************************************/

bool
EmFileRef::FromPrefString (const string& s)
{
	fFilePath = s;

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	ToPrefString
 *
 * DESCRIPTION:	Produce a string that can be stored to a preference file
 *				and which can later be used to reproduce the current
 *				file reference object.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The string to be written to the preference file.
 *
 ***********************************************************************/

string
EmFileRef::ToPrefString (void) const
{
	return fFilePath;
}


/***********************************************************************
 *
 * FUNCTION:	MaybePrependCurrentDirectory
 *
 * DESCRIPTION:	Prepend the current working directory if the managed
 *				path is not a full path.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void
EmFileRef::MaybePrependCurrentDirectory (void)
{
	if (fFilePath[0] != '/')
	{
		size_t bufSize = 256;
		char* buffer = (char*) Platform::AllocateMemory (bufSize);
		while (getcwd (buffer, bufSize) == NULL)
		{
			if (errno != ERANGE)
				return;

			bufSize *= 2;
			buffer = (char*) Platform::ReallocMemory (buffer, bufSize);
		}

		size_t cwdLen = strlen (buffer);
		if (cwdLen)
		{
			if (buffer[cwdLen - 1] != '/')
				fFilePath = string (buffer) + "/" + fFilePath;
			else
				fFilePath = string (buffer) + fFilePath;
		}

		Platform::DisposeMemory (buffer);
	}
}


/***********************************************************************
 *
 * FUNCTION:	MaybeNormalize
 *
 * DESCRIPTION:	The routines we use to fetch a file from the user
 *				sometimes return full paths starting with double
 *				slashes.  While the file system allows that, it play
 *				havoc with our operator=().  Patch up full paths
 *				so that they don't start with '//'
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void
EmFileRef::MaybeNormalize (void)
{
	if (fFilePath.size () >= 2 &&
		fFilePath[0] == '/' &&
		fFilePath[1] == '/')
	{
		fFilePath.erase (fFilePath.begin ());
	}
}
