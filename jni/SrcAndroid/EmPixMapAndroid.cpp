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
#include "EmPixMapAndroid.h"

#include "PHEMNativeIF.h"


// ---------------------------------------------------------------------------
//		¥ ConvertPixMapToHost
// ---------------------------------------------------------------------------

void ConvertPixMapToHost(const EmPixMap& src, void* dest,
				int firstLine, int lastLine, Bool scale)
{
	// Determine a lot of the values we'll need.

	int			factor		= scale ? 2 : 1;
#if 0
        PHEM_Log_Msg("ConvertPixMapToHost");
        PHEM_Log_Place(factor);
        PHEM_Log_Place(firstLine);
        PHEM_Log_Place(lastLine);
#endif

	EmPoint			factorPoint	= EmPoint (factor, factor);

	EmPoint			srcSize		= src.GetSize ();
#if 0
        PHEM_Log_Place(srcSize.fX);
        PHEM_Log_Place(srcSize.fY);
#endif
	EmPixMapRowBytes	destRowBytes	= srcSize.fX * 2 * factor;
#if 0
        PHEM_Log_Msg("RowBytes:");
        PHEM_Log_Place(destRowBytes);
#endif

	// Finally, copy the bits, converting to 16-bit 565 format along the way.

	EmPixMap	wrapper;

	wrapper.SetSize (srcSize * factorPoint);
	wrapper.SetFormat (kPixMapFormat16RGB565);
	wrapper.SetRowBytes (destRowBytes);
	wrapper.SetColorTable (src.GetColorTable ());
	wrapper.SetBits (dest);

	EmRect	srcBounds (0, firstLine, srcSize.fX, lastLine);
	EmRect	destBounds (srcBounds * factorPoint);

	EmPixMap::CopyRect (wrapper, src, destBounds, srcBounds);
}
