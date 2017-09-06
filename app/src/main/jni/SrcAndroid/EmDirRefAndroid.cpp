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

#include "EmFileRef.h"
#include "Platform.h"
#include "PHEMNativeIF.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

/* Update for GCC 4 */
#include <stdlib.h>
#include <string.h>


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::EmDirRef
 *
 * DESCRIPTION:	Various ways to make a directory reference.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

EmDirRef::EmDirRef (void) :
	fDirPath ()
{
}


EmDirRef::EmDirRef (const EmDirRef& other) :
	fDirPath (other.fDirPath)
{
}


EmDirRef::EmDirRef (const char* path) :
	fDirPath (path)
{
	this->MaybeAppendSlash ();
}


EmDirRef::EmDirRef (const string& path) :
	fDirPath (path)
{
	this->MaybeAppendSlash ();
}


EmDirRef::EmDirRef (const EmDirRef& parent, const char* path) :
	fDirPath (parent.GetFullPath () + path)
{
	this->MaybeAppendSlash ();
}


EmDirRef::EmDirRef (const EmDirRef& parent, const string& path) :
	fDirPath (parent.GetFullPath () + path)
{
	this->MaybeAppendSlash ();
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::EmDirRef
 *
 * DESCRIPTION:	EmDirRef destructor.  Nothing special to do...
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

EmDirRef::~EmDirRef (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::operator=
 *
 * DESCRIPTION:	Assignment operator.  If "other" is not the same as
 *				the controlled object, copy the contents.
 *
 * PARAMETERS:	other - object to copy.
 *
 * RETURNED:	reference to self.
 *
 ***********************************************************************/

EmDirRef&
EmDirRef::operator= (const EmDirRef& other)
{
	if (&other != this)
	{
		fDirPath = other.fDirPath;
	}

	return *this;
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::IsSpecified
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
EmDirRef::IsSpecified (void) const
{
	return !fDirPath.empty ();
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::Exists
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
EmDirRef::Exists (void) const
{
	if (this->IsSpecified ())
	{
		DIR* dir = opendir (fDirPath.c_str ());
		if (dir)
		{
			closedir (dir);
			return true;
		}
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::Create
 *
 * DESCRIPTION:	Attempt to create the managed directory.  Does nothing
 *				if the directory already exists.  Throws an exception
 *				if the attempt to create the directory fails.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmDirRef::Create (void) const
{
	if (!this->Exists () && this->IsSpecified ())
	{
		// Make sure all directories down to us are created, too.

		EmDirRef	parent = this->GetParent ();
		parent.Create ();

		if (mkdir (fDirPath.c_str (), 0777) != 0)
		{
			// !!! throw...
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::GetName
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
EmDirRef::GetName (void) const
{
	string	result;

	if (this->IsSpecified () && fDirPath != "/")
	{
		// Make a copy of the path, and chop off the trailing '\'
		// in order to get _splitpath to think the thing at the
		// end is a file name.

		string	dirPath (fDirPath);
		dirPath.resize (dirPath.size () - 1);

		string::size_type pos = dirPath.rfind ('/', string::npos);
		EmAssert (pos != string::npos);

		result = dirPath.substr (pos + 1, string::npos);
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::GetParent
 *
 * DESCRIPTION:	Returns an object representing the parent (or container)
 *				of the managed file.  If the managed file is the root
 *				directory, returns an unspecified EmDirRef.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	An object representing the file's parent.
 *
 ***********************************************************************/

EmDirRef
EmDirRef::GetParent (void) const
{
	EmDirRef	result;

	if (this->IsSpecified () && fDirPath != "/")
	{
		// Make a copy of the path, and chop off the trailing '\'
		// in order to get _splitpath to think the thing at the
		// end is a file name.

		string	dirPath (fDirPath);
		dirPath.resize (dirPath.size () - 1);

		string::size_type pos = dirPath.rfind ('/', string::npos);
		EmAssert (pos != string::npos);

		result = EmDirRef (dirPath.substr (0, pos + 1));
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::GetFullPath
 *
 * DESCRIPTION:	Get a full (platform-specific) path to the object.  The
 *				path is canonicalized in that it will always have a
 *				trailing slash.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	An string representing the file's path.
 *
 ***********************************************************************/

string
EmDirRef::GetFullPath (void) const
{
	return fDirPath;
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::GetChildren
 *
 * DESCRIPTION:	Get a full (platform-specific) path to the object.  The
 *				path is canonicalized in that it will always have a
 *				trailing slash.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	An string representing the file's path.
 *
 ***********************************************************************/

void
EmDirRef::GetChildren (EmFileRefList* fileList, EmDirRefList* dirList) const
{
	DIR* dir = opendir (fDirPath.c_str ());
	if (dir)
	{
		struct dirent* ent;
		while ((ent = readdir (dir)) != NULL)
		{
			if (strcmp (ent->d_name, ".") == 0)
				continue;

			if (strcmp (ent->d_name, "..") == 0)
				continue;

			string full_path (fDirPath + ent->d_name);
			struct stat buf;
			stat (full_path.c_str (), &buf);

			if (S_ISDIR (buf.st_mode))
			{
				if (dirList)
					dirList->push_back (EmDirRef (*this, ent->d_name));
			}
			else
			{
				if (fileList)
					fileList->push_back (EmFileRef (*this, ent->d_name));
			}
		}

		closedir (dir);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::operator==
 * FUNCTION:	EmDirRef::operator!=
 * FUNCTION:	EmDirRef::operator>
 * FUNCTION:	EmDirRef::operator<
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
EmDirRef::operator== (const EmDirRef& other) const
{
	return _stricmp (fDirPath.c_str (), other.fDirPath.c_str ()) == 0;
}


bool
EmDirRef::operator!= (const EmDirRef& other) const
{
	return _stricmp (fDirPath.c_str (), other.fDirPath.c_str ()) != 0;
}


bool
EmDirRef::operator> (const EmDirRef& other) const
{
	return _stricmp (fDirPath.c_str (), other.fDirPath.c_str ()) < 0;
}


bool
EmDirRef::operator< (const EmDirRef& other) const
{
	return _stricmp (fDirPath.c_str (), other.fDirPath.c_str ()) > 0;
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
EmDirRef::FromPrefString (const string& s)
{
	fDirPath = s;
	this->MaybeAppendSlash ();

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
EmDirRef::ToPrefString (void) const
{
	return fDirPath;
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::GetEmulatorDirectory
 *
 * DESCRIPTION:	Return an EmDirRef for Poser's directory.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The desired EmDirRef.
 *
 ***********************************************************************/

EmDirRef
EmDirRef::GetEmulatorDirectory (void)
{
// User-defined path, default our data dir on SD
  return EmDirRef (PHEM_Get_Base_Dir());
}


/***********************************************************************
 *
 * FUNCTION:	EmDirRef::GetPrefsDirectory
 *
 * DESCRIPTION:	Return an EmDirRef for Poser's preferences directory.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The desired EmDirRef.
 *
 ***********************************************************************/

EmDirRef
EmDirRef::GetPrefsDirectory (void)
{
	return GetEmulatorDirectory ();
}


/***********************************************************************
 *
 * FUNCTION:	MaybeAppendSlash
 *
 * DESCRIPTION:	Append a trailing slash to the full path if there isn't
 *				one already there.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void
EmDirRef::MaybeAppendSlash (void)
{
	if (this->IsSpecified () && fDirPath[fDirPath.size () - 1] != '/')
	{
		fDirPath += '/';
	}
}
