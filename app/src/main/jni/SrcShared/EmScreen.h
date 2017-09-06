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

#ifndef EmScreen_h
#define EmScreen_h

#include "EmPixMap.h"			// EmPixMap

class SessionFile;

class EmScreenUpdateInfo
{
	public:
		// Output parameters.  Passed back to caller of
		// Screen::GetBits.  fLCDOn is always valid.  The
		// other fields are valid only if Screen::GetBits
		// returns true.

		EmPixMap	fImage;			// LCD image
		long		fFirstLine;		// First changed scanline
		long		fLastLine;		// Last changed scanline + 1
		long		fLeftMargin;	// If LCD is scrlled by some sub-byte amount,
									// this contains that amount.
		Bool		fLCDOn;			// True if LCD is on at all

		// Input parameters.  Set by Screen::GetBits and
		// passed to EmHAL::GetLCDScanlines.

		emuptr		fScreenLow;		// First dirty byte
		emuptr		fScreenHigh;	// Last dirty byte
};

class EmScreen
{
	public:
		static void 			Initialize			(void);
		static void 			Reset				(void);
		static void 			Save				(SessionFile&);
		static void 			Load				(SessionFile&);
		static void 			Dispose 			(void);

		static void 			MarkDirty			(emuptr address, uint32 size);
		static void 			InvalidateAll		(void);

		static Bool 			GetBits 			(EmScreenUpdateInfo&);
};

#endif	// EmScreen_h
