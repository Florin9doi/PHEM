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
#include "EmStreamFile.h"

#include "EmErrCodes.h"			// ConvertFromStdCError
#include "ErrorHandling.h"		// Errors::ThrowIfStdCError

#include <errno.h>				// EBADF, errno


// ===========================================================================
//		¥ EmStreamFile
//
//		A EmStreamFile is a lightweight reference to an open file.  The class's
//		constructor attempts to open (or create) the file based on the input
//		parameters.	 The class's destructor closes the file.
//
//		Once a file is open, member functions can be used to operate on the
//		file (read from it, write to it, etc.).
//
//		FileHandles can be copied, but no reference counting is performed.
//		Thus, after the first EmStreamFile is deleted, copies of it will
//		contain invalid file references.
//		
// ===========================================================================

/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::EmStreamFile
 *
 * DESCRIPTION:	EmStreamFile constructor. Opens and/or creates the
 *				file according to the input parameters.
 *
 * PARAMETERS:	ref - reference to the file to create/open.
 *
 *				openMode - flags describing how to open/create the file
 *
 *				creator - creator value to assign to the file if it's
 *					created (only used on Mac).
 *
 *				fileType - file type value to assign to the file if it's
 *					created (only used on the Mac).
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

EmStreamFile::EmStreamFile (const EmFileRef& ref,
							long openMode,
							EmFileCreator creator,
							EmFileType fileType) :
	EmStream (),
	fFileRef (ref),
#if USE_MAC_CALLS
	fRefNum (0)
#else
	fStream (NULL)
#endif
{
	this->Open (ref, openMode, creator, fileType);
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::~EmStreamFile
 *
 * DESCRIPTION:	EmStreamFile destructor. Closes the file.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

EmStreamFile::~EmStreamFile (void)
{
	this->Close ();
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::SetMarker
 *
 * DESCRIPTION:	Set the read/write position within the file.
 *
 * PARAMETERS:	inOffset - stdio-style offset value.
 *
 *				inFromWhere - stdio-style mode value.
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

void			
EmStreamFile::SetMarker (int32			inOffset,
						 StreamFromType	inFromWhere)
{
	EmStream::SetMarker (inOffset, inFromWhere);

#if USE_MAC_CALLS
	if (fRefNum == 0)
	{
		this->Throw (EBADF);
	}

	int whence;

	if (inFromWhere == kStreamFromStart)
	{
		whence = fsFromStart;
	}
	else if (inFromWhere == kStreamFromEnd)
	{
		whence = fsFromLEOF;
	}
	else
	{
		EmAssert (inFromWhere == kStreamFromMarker);
		whence = fsFromMark;
	}

	OSErr	err = ::SetFPos (fRefNum, whence, inOffset);
	if (err)
	{
		Need to deal with this being a Mac error, not a Std C error.
		this->Throw (err);
	}
#else
	if (fStream == NULL)
	{
		this->Throw (EBADF);
	}

	int whence;

	if (inFromWhere == kStreamFromStart)
	{
		whence = SEEK_SET;
	}
	else if (inFromWhere == kStreamFromEnd)
	{
		whence = SEEK_END;
	}
	else
	{
		EmAssert (inFromWhere == kStreamFromMarker);
		whence = SEEK_CUR;
	}

	int err = fseek (fStream, inOffset, whence);
	if (err)
	{
		this->Throw (errno);
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::GetMarker
 *
 * DESCRIPTION:	Set the read/write position within the file.
 *
 * PARAMETERS:	inOffset - stdio-style offset value.
 *
 *				inFromWhere - stdio-style mode value.
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

int32			
EmStreamFile::GetMarker (void) const
{
#if USE_MAC_CALLS
	if (fRefNum == 0)
	{
		this->Throw (EBADF);
	}

	long	pos;
	OSErr	err = ::GetFPos (fRefNum, &pos);
	return pos;
#else
	if (fStream == NULL)
	{
		this->Throw (EBADF);
	}

	return ftell (fStream);
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::SetLength
 *
 * DESCRIPTION:	Set the length of the file.
 *
 * PARAMETERS:	inLength - the desired file length.
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

void			
EmStreamFile::SetLength (int32 inLength)
{
	EmStream::SetLength (inLength);

	// !!! Use BOOL SetEndOfFile (fHandle);
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::GetLength
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The length of the stream (file) in bytes.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

int32			
EmStreamFile::GetLength (void) const
{
#if USE_MAC_CALLS
	if (fRefNum == 0)
	{
		this->Throw (EBADF);
	}

	long	eof;
	OSErr	err = ::GetEOF (fRefNum, &eof);
	return eof;
#else
	if (fStream == NULL)
	{
		this->Throw (EBADF);
	}

	long	cur = ftell (fStream);
	fseek (fStream, 0, SEEK_END);

	long	length = ftell (fStream);
	fseek (fStream, cur, SEEK_SET);

	return length;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::PutBytes
 *
 * DESCRIPTION:	Write data to the file.
 *
 * PARAMETERS:	length - amount of data to write.
 *
 *				buffer - buffer from which data is retrieved
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

ErrCode			
EmStreamFile::PutBytes (const void*	inBuffer,
						int32		inByteCount)
{
#if USE_MAC_CALLS
	if (fRefNum == 0)
	{
		return EBADF;
	}

	if (!inBuffer)
	{
		return EINVAL;
	}

	long	count = inByteCount;
	OSErr	err = ::FSWrite (fRefNum, &count, inBuffer);

	return err;
#else
	if (fStream == NULL)
	{
		return EBADF;
	}

	if (!inBuffer)
	{
		return EINVAL;
	}

	fwrite (inBuffer, 1, inByteCount, fStream);
	if (ferror (fStream))
	{
		return ::ConvertFromStdCError (errno);
	}

	return 0;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::GetBytes
 *
 * DESCRIPTION:	Read data from the file.
 *
 * PARAMETERS:	length - amount of data to read.
 *
 *				buffer - buffer into which the data is place.
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

ErrCode			
EmStreamFile::GetBytes (void*	outBuffer,
						int32	inByteCount)
{
#if USE_MAC_CALLS
	if (fRefNum == 0)
	{
		return EBADF;
	}

	if (!outBuffer)
	{
		return EINVAL;
	}

	long	count = inByteCount;
	OSErr	err = ::FSRead (fRefNum, &count, outBuffer);

	if (!err && fTextMode)
	{
		char*	buffer = (char*) outBuffer;
		while (inByteCount--)
		{
			if (*buffer == '/n')
				*buffer = '/r';
			else if (*buffer == '/r')
				*buffer = '/n';

			++buffer;
		}
	}

	return err;
#else
	if (fStream == NULL)
	{
		return EBADF;
	}

	if (!outBuffer)
	{
		return EINVAL;
	}

	if (fread (outBuffer, 1, inByteCount, fStream) == 0)
	{
		return 1;	// !!! need better error code
	}

	if (ferror (fStream))
	{
		return ::ConvertFromStdCError (errno);
	}

	return 0;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::Open [protected]
 *
 * DESCRIPTION:	Opens and/or creates the given file based on the given
 *				parameters.	 This function is called from the ctor in
 *				order to do all the work.
 *
 * PARAMETERS:	ref - reference to the file to create/open.
 *
 *				openMode - flags describing how to open/create the file
 *
 *				creator - creator value to assign to the file if it's
 *					created (only used on Mac).
 *
 *				fileType - file type value to assign to the file if it's
 *					created (only used on the Mac).
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

void			
EmStreamFile::Open (const EmFileRef& ref, long openMode,
					EmFileCreator creator, EmFileType fileType)
{
#if USE_MAC_CALLS

	// Open/create the file.
	
	string	cName = ref.GetFullPath ();
	LStr255	name (cName.c_str ());
	OSErr	err;
	
	if ((openMode & kOpenTypeMask) == kCreateOrEraseForUpdate)
	{
		err = ::FSOpen (name, 0, &fRefNum);
		if (err == fnfErr)
		{
			err = ::Create (name, 0, creator, fileType);
			
			if (!err)
			{
				err = ::FSOpen (name, 0, &fRefNum);
			}
			
			if (!err)
			{
				err = ::SetEOF (fRefNum, 0);
			}
		}
	}
	else if ((openMode & kOpenTypeMask) == kOpenExistingForRead)
	{
		err = ::FSOpen (name, 0, &fRefNum);
	}

	// Check for errors.

	if (err)
	{
		fRefNum = 0;
		this->Throw (err);
	}
	
	fTextMode = (openMode & kOpenText) != 0;
#else
	char*	kModes[] = { "r", "w", "a", "r+", "w+", "a+" };
	string	mode (kModes[openMode & kOpenTypeMask]);

	if ((openMode & kOpenText) != 0)
		mode += 't';
	else
		mode += 'b';

	// Open/create the file.
	
	fStream = fopen (ref.GetFullPath ().c_str (), mode.c_str ());

	// Check for errors.

	if (fStream == NULL)
	{
		this->Throw (errno);
	}
#endif

	if (creator && fileType)
	{
		ref.SetCreatorAndType (creator, fileType);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::Close [protected]
 *
 * DESCRIPTION:	Closes the file.  Called from the dtor to do all the
 *				work.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	nothing.
 *
 *				If the operation fails, an exception is thrown.
 *
 ***********************************************************************/

void			
EmStreamFile::Close (void)
{
#if USE_MAC_CALLS
	if (fRefNum == 0)
	{
		this->Throw (EINVAL);
	}

	::FSClose (fRefNum);
	fRefNum = 0;
#else
	if (fStream == NULL)
	{
		this->Throw (EINVAL);
	}

	if (fclose (fStream))
	{
		fStream = NULL;
		this->Throw (errno);
	}

	fStream = NULL;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::Throw [protected]
 *
 * DESCRIPTION:	Bottleneck function for throwing an exception.	Makes
 *				sure that the file's name is installed as an error
 *				message parameter and then throws the exception.
 *
 * PARAMETERS:	err - Std C error code to throw.
 *
 * RETURNED:	never.
 *
 ***********************************************************************/

void			
EmStreamFile::Throw (int err) const
{
	this->SetFileNameParameter ();

	Errors::ThrowIfStdCError (err);
}


/***********************************************************************
 *
 * FUNCTION:	EmStreamFile::SetFileNameParameter [protected]
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void			
EmStreamFile::SetFileNameParameter (void) const
{
	string	name = fFileRef.GetName ();
	Errors::SetParameter ("%filename", name);
}
