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

#ifndef EmStreamFile_h
#define EmStreamFile_h

#include "EmStream.h"			// EmStream

#include "EmFileRef.h"			// EmFileRef
#include "EmTypes.h"			// ErrCode

#include "stdio.h"				// FILE

enum
{
	kOpenExistingForRead,		//	"r"		Open an existing file for input.
	kCreateOrEraseForWrite,		//	"w"		Create a new file, or truncate an
								//			existing one, for output.
	kCreateOrOpenForWrite,		//	"a"		Create a new file, or append to an
								//			existing one, for output.

	kOpenExistingForUpdate,		//	"r+"	Open an existing file for update,
								//			starting at the beginning of the file.
	kCreateOrEraseForUpdate,	//	"w+"	Create a new file, or truncate an
								//			existing one, for update.
	kCreateOrOpenForUpdate,		//	"a+"	Create a new file, or append to an
								//			existing one, for update.

	kOpenTypeMask	= 63,
	kOpenText		= 64
};


class EmStreamFile : public EmStream
{
	public:
								EmStreamFile	(const EmFileRef&,
												 long			openMode,
												 EmFileCreator	creator = kFileCreatorNone,
												 EmFileType		fileType = kFileTypeNone);
		virtual					~EmStreamFile	(void);

		virtual void			SetMarker		(int32			inOffset,
												 StreamFromType	inFromWhere);
		virtual int32			GetMarker		(void) const;

		virtual void			SetLength		(int32			inLength);
		virtual int32			GetLength		(void) const;

		virtual ErrCode			PutBytes		(const void*	inBuffer,
												 int32			inByteCount);
		virtual ErrCode			GetBytes		(void*			outBuffer,
												 int32			inByteCount);

		EmFileRef				GetFileRef		(void) const
								{
									return fFileRef;
								}

	protected:
		void					Open			(const EmFileRef&,
												 long			openMode,
												 EmFileCreator	creator,
												 EmFileType		fileType);

		void					Close			(void);

		void					Throw			(int) const;
		void					SetFileNameParameter	(void) const;

	private:
			// Protect the copy constructor so that we don't
			// accidentally make copies (what does it mean to
			// copy a reference to an open file?).

								EmStreamFile	(const EmStreamFile&);

		EmFileRef				fFileRef;

#if USE_MAC_CALLS
		short					fRefNum;
		Bool					fTextMode;
#else
		FILE*					fStream;
#endif
};

#endif	/* EmStreamFile_h */
