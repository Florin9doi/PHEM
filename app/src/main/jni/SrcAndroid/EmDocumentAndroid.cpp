/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1999-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmDocumentAndroid.h"

#include "EmScreen.h"			// EmScreen

#include <stdio.h>				// fopen, fprintf, fwrite, fclose, FILE

/* Update for GCC 4 */
#include <stdlib.h>
#include <string.h>


EmDocumentAndroid*	gHostDocument;

// ---------------------------------------------------------------------------
//		¥ EmDocument::HostCreateDocument
// ---------------------------------------------------------------------------
// Create our document instance.  This is the one and only function that
// creates the document.  Being in a platform-specific file, it can create
// any subclass of EmDocument it likes (in particular, one specific to our
// platform).

EmDocument* EmDocument::HostCreateDocument (void)
{
	return new EmDocumentAndroid;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmDocumentAndroid::EmDocumentAndroid
 *
 * DESCRIPTION:	Constructor.  Sets the global host application variable
 *				to point to us.
 *
 * PARAMETERS:	None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

EmDocumentAndroid::EmDocumentAndroid (void) :
	EmDocument ()
{
	EmAssert (gHostDocument == NULL);
	gHostDocument = this;
}


/***********************************************************************
 *
 * FUNCTION:	EmDocumentAndroid::~EmDocumentAndroid
 *
 * DESCRIPTION:	Destructor.  Closes our window and sets the host
 *				application variable to NULL.
 *
 * PARAMETERS:	None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

EmDocumentAndroid::~EmDocumentAndroid (void)
{
	EmAssert (gHostDocument == this);
	gHostDocument = NULL;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocumentAndroid::HostSaveScreen
// ---------------------------------------------------------------------------
// Save the current contents of the LCD buffer to the given file.

void EmDocumentAndroid::HostSaveScreen (const EmFileRef& destRef)
{
#if 0
//AndroidTODO
	// Make sure the extension is right.

	string	fullPath = destRef.GetFullPath ();
	char*	fNameExt = (char*) malloc (fullPath.size () + 4);
	strcpy (fNameExt, fullPath.c_str ());
	fl_filename_setext (fNameExt, ".ppm");

	FILE* f = fopen (fNameExt, "wb");
	if (f)
	{
		EmScreen::InvalidateAll ();

		EmScreenUpdateInfo info;
		EmScreen::GetBits (info);

		EmScreen::InvalidateAll ();

		info.fImage.ConvertToFormat (kPixMapFormat24RGB);

		// PPM format is:
		//
		// File type tag
		// Width
		// Height
		// Max component value
		// Width * Height pixels
		//
		// The first items in the file are all text and
		// seperated by whitespace.  The array of pixels
		// is in text format if the file type is P3, and
		// is in binary format if the type is P6.

		EmPoint size = info.fImage.GetSize ();

		fprintf (f, "P6 %ld %ld 255\x0D", size.fX, size.fY);

		uint8* bits = (uint8*) info.fImage.GetBits ();
		for (long yy = 0; yy < size.fY; ++yy)
		{
			long rowBytes = info.fImage.GetRowBytes ();
			uint8* basePtr = bits + yy * rowBytes;

			fwrite (basePtr, 1, rowBytes, f);
		}

		fclose (f);
	}

	free (fNameExt);
#endif
}
