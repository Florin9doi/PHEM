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

#ifndef EmPixMap_h
#define EmPixMap_h

#include "EmStructs.h"			// RGBList
#include "EmRegion.h"			// EmRegion

#include <vector>

typedef int	EmPixMapDepth;
typedef int	EmPixMapRowBytes;

enum EmPixMapFormat
{
	// If you change these values, be sure to update PrvGetDepth.

	kPixMapFormat1,
	kPixMapFormat2,
	kPixMapFormat4,
	kPixMapFormat8,
	kPixMapFormat16RGB555,
	kPixMapFormat16BGR555,
	kPixMapFormat16RGB565,
	kPixMapFormat16BGR565,
	kPixMapFormat24RGB,
	kPixMapFormat24BGR,
	kPixMapFormat32ARGB,
	kPixMapFormat32ABGR,
	kPixMapFormat32RGBA,
	kPixMapFormat32BGRA,

	kPixMapFormatLast
};


class EmPixMap
{
	public:
								EmPixMap		(void);
								EmPixMap		(const EmPixMap&);
								~EmPixMap		(void);

		EmPixMap&				operator=		(const EmPixMap&);

		EmPoint					GetSize			(void) const;
		void					SetSize			(const EmPoint&);

		EmPixMapDepth			GetDepth		(void) const;
		EmPixMapFormat			GetFormat		(void) const;
		void					SetFormat		(EmPixMapFormat);

		EmPixMapRowBytes		GetRowBytes		(void) const;
		void					SetRowBytes		(EmPixMapRowBytes);

		const RGBList&			GetColorTable	(void) const;
		void					SetColorTable	(const RGBList&);

		const void*				GetBits			(void) const;
		void*					GetBits			(void);
		void					SetBits			(void*);

		void					CreateMask		(EmPixMap& dest) const;
		EmRegion				CreateRegion	(void) const;

		void					ChangeTone		(int32 percent, EmCoord firstLine = -1, EmCoord lastLine = -1);
		void					ConvertToColor	(int type, EmCoord firstLine = -1, EmCoord lastLine = -1);
		void					FlipScanlines	(void);
		void					ConvertToFormat	(EmPixMapFormat);

		static void				CopyRect		(EmPixMap& dest, const EmPixMap& src,
												 const EmRect& destRect, const EmRect& srcRect);

	private:
		EmPixMapRowBytes		DetermineRowBytes	(void) const;
		void					InvalidateBuffer	(void);
		void					UpdateBuffer		(void) const;
		void					CopyPixelBuffer		(const EmPixMap& other);

	private:
		EmPoint					fSize;
		EmPixMapFormat			fFormat;
		EmPixMapRowBytes		fRowBytes;
		RGBList					fColors;
		uint8*					fPixels;
		Bool					fPixelsOwned;
};

uint32*	Get1To8Table (void);
uint32*	Get2To8Table (void);
uint16*	Get4To8Table (void);

#endif	// EmPixMap_h
