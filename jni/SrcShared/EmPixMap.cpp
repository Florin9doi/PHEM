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
#include "EmPixMap.h"

#include "Platform.h"			// Platform::AllocateMemory

#include <string.h>

/*
	EmPixMap is a simple class for managing pixmaps (multi-bpp bitmap)
	in a cross-platform fashion.

	Poser makes use of pixmaps in the following ways:

		- Converting the LCD frame buffer to something that can be
			displayed by the host.
		- Converting the JPEG skin images to something that can be
			displayed by the host.
		- Executing the "Save Screen..." command.

	As such, EmPixMap does not try to be everything to everybody.  In
	particular, it does not support all possible bitdepths and pixel
	formats.  In general, it supports 1-, 8-, 24-, and 32-bit buffers,
	where the 32-bit buffer is always in ARGB format.  While there are
	routines that manage the conversion of 1-, 2-, and 4-bpp buffers into
	one of the above formats, these are in place solely for converting
	the LCD framebuffer into one of the standard EmPixMap formats.

	EmPixMap also has some utility routines that are useful for carrying
	some of Poser's facilities.  It can flip all of the scanlines in order
	to help interoperate with Windows BMPs; it can create an outline region
	that can be used to create a host window's border; etc.
*/


static uint32*	gConvert1To8;	// Used to convert a 4-bit nybble consisting of 4 1-bit pixels
								// to a 32-bit value containing 4 8-bit pixels.

static uint32*	gConvert2To8;	// Used to convert an 8-bit byte consisting of 4 2-bit pixels
								// to a 32-bit value containing 4 8-bit pixels.

static uint16*	gConvert4To8;	// Used to convert an 8-bit byte consisting of 2 4-bit pixels
								// to a 16-bit value containing 4 8-bit pixels.

struct ScanlineParms
{
	uint8*			fDestScanline;
	const uint8*	fSrcScanline;
	const RGBList*	fDestColors;
	const RGBList*	fSrcColors;
	EmCoord			fLeft;
	EmCoord			fRight;
};


// We have scanline converters that can go from bitdepth M to bitdepth N.
// The appropriate converter is chosen and stored in a variable of the
// following type.

typedef void (*ScanlineConverter) (const ScanlineParms&);


static void				PrvMakeMask		(void* dstPtr, void* srcPtr, long rowBytes, long width, long height);
static void				PrvAddToRegion	(EmRegion& region, int top, int left, int right);
static EmPixMapDepth	PrvGetDepth		(EmPixMapFormat);

#define DECLARE_CONVERTER(src_format, dest_format)				\
	static void PrvConvert##src_format##To##dest_format (const ScanlineParms&);


// Define FOR_EACH_FORMAT, a macro which iterates over all of
// pixmap formats we support.
//
// !!! Science project...how can I use this macro to iterate
// over *pairs* of formats in a generic way?  That is, I want
// be able to say something like:
//
//	#define FOR_EACH_FORMAT_PAIR(DO_TO_PAIR) ...
//
//	#define DECLARE_CONVERTER(format1, format2) ...
//
//	FOR_EACH_FORMAT_PAIR(DECLARE_CONVERTER)
//
// One restriction of this exercise is that I don't want to
// define another macro that explicitly lists the formats.
// That should be limited to FOR_EACH_FORMAT.

#define FOR_EACH_FORMAT(DO_TO_FORMAT)	\
	DO_TO_FORMAT (1)					\
	DO_TO_FORMAT (2)					\
	DO_TO_FORMAT (4)					\
	DO_TO_FORMAT (8)					\
	DO_TO_FORMAT (16RGB565)				\
	DO_TO_FORMAT (24RGB)				\
	DO_TO_FORMAT (24BGR)				\
	DO_TO_FORMAT (32ARGB)				\
	DO_TO_FORMAT (32ABGR)				\
	DO_TO_FORMAT (32RGBA)				\
	DO_TO_FORMAT (32BGRA)



// Define FOR_EACH_FORMAT_PAIR, which iterates over all of the src/dest
// format pairings.  We do this with a helper macro (FOR_EACH_FORMAT_PAIR_0)
// that allows us to multiply all the pairings together.

#define FOR_EACH_FORMAT_PAIR_0(DO_TO_PAIR, fmt)	\
	DO_TO_PAIR (fmt, 1)							\
	DO_TO_PAIR (fmt, 2)							\
	DO_TO_PAIR (fmt, 4)							\
	DO_TO_PAIR (fmt, 8)							\
	DO_TO_PAIR (fmt, 16RGB565)						\
	DO_TO_PAIR (fmt, 24RGB)						\
	DO_TO_PAIR (fmt, 24BGR)						\
	DO_TO_PAIR (fmt, 32ARGB)					\
	DO_TO_PAIR (fmt, 32ABGR)					\
	DO_TO_PAIR (fmt, 32RGBA)					\
	DO_TO_PAIR (fmt, 32BGRA)


#define FOR_EACH_FORMAT_PAIR(DO_TO_PAIR)		\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 1)		\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 2)		\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 4)		\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 8)		\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 16RGB565)	\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 24RGB)	\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 24BGR)	\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 32ARGB)	\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 32ABGR)	\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 32RGBA)	\
	FOR_EACH_FORMAT_PAIR_0 (DO_TO_PAIR, 32BGRA)


FOR_EACH_FORMAT_PAIR (DECLARE_CONVERTER)


// Macros used for accessing and converting/copying pixels.

// Note: we do 'bit extension' here - for example, if the 5-bit
// red value is 'vwxyz' then the 8-bit red value will be
// 'vwxyzvwx'. (In other words, we repeat the top three bits at
// the bottom.) If we didn't - if, say, we left those 'bottom' bits
// as zeros - it would darken the image. Similarly, if we set them
// to all ones, it would brighten the image. Repeating the bits
// keeps the color balance the same. (And note, a 16-bit all-ones
// value maps to a 24-bit all-ones, and similarly for all-zeroes.)
#define RED_16_565_MASK   0xF800
#define GREEN_16_565_MASK 0x7E0
#define BLUE_16_565_MASK  0x1F

#define GET_16RGB565(srcPtr, r, g, b, a)   \
        r = ((*(uint16 *)srcPtr) & RED_16_565_MASK) >> 8; \
        r = r | (r >> 5); \
        g = ((*(uint16 *)srcPtr) & GREEN_16_565_MASK) >> 3; \
        g = g | (g >> 6); \
        b = ((*(uint16 *)srcPtr) & BLUE_16_565_MASK) << 3; \
        b = b | (b >> 5); \
	a = 0; \
        srcPtr+=2;

#define GET_24RGB(srcPtr, r, g, b, a)	\
	r = *srcPtr++;						\
	g = *srcPtr++;						\
	b = *srcPtr++;						\
	a = 0;


#define GET_24BGR(srcPtr, r, g, b, a)	\
	b = *srcPtr++;						\
	g = *srcPtr++;						\
	r = *srcPtr++;						\
	a = 0;


#define GET_32ARGB(srcPtr, r, g, b, a)	\
	a = *srcPtr++;						\
	r = *srcPtr++;						\
	g = *srcPtr++;						\
	b = *srcPtr++;


#define GET_32ABGR(srcPtr, r, g, b, a)	\
	a = *srcPtr++;						\
	b = *srcPtr++;						\
	g = *srcPtr++;						\
	r = *srcPtr++;


#define GET_32RGBA(srcPtr, r, g, b, a)	\
	r = *srcPtr++;						\
	g = *srcPtr++;						\
	b = *srcPtr++;						\
	a = *srcPtr++;


#define GET_32BGRA(srcPtr, r, g, b, a)	\
	b = *srcPtr++;						\
	g = *srcPtr++;						\
	r = *srcPtr++;						\
	a = *srcPtr++;

#define PUT_16RGB565(destPtr, r, g, b, a) \
  *(uint16 *)destPtr = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3); \
  destPtr+=2;

#define PUT_24RGB(destPtr, r, g, b, a)	\
	*destPtr++ = r;						\
	*destPtr++ = g;						\
	*destPtr++ = b;


#define PUT_24BGR(destPtr, r, g, b, a)	\
	*destPtr++ = b;						\
	*destPtr++ = g;						\
	*destPtr++ = r;


#define PUT_32ARGB(destPtr, r, g, b, a)	\
	*destPtr++ = a;						\
	*destPtr++ = r;						\
	*destPtr++ = g;						\
	*destPtr++ = b;


#define PUT_32ABGR(destPtr, r, g, b, a)	\
	*destPtr++ = a;						\
	*destPtr++ = b;						\
	*destPtr++ = g;						\
	*destPtr++ = r;


#define PUT_32RGBA(destPtr, r, g, b, a)	\
	*destPtr++ = r;						\
	*destPtr++ = g;						\
	*destPtr++ = b;						\
	*destPtr++ = a;


#define PUT_32BGRA(destPtr, r, g, b, a)	\
	*destPtr++ = b;						\
	*destPtr++ = g;						\
	*destPtr++ = r;						\
	*destPtr++ = a;


#define CONVERT_PACKED_PIXEL(shift, mask, dest_format)					\
	if (xx++ >= right)													\
		break;															\
	{																	\
	const RGBType&	rgb = (*srcColors)[(bits >> (shift)) & (mask)];		\
	PUT_##dest_format(destPtr, rgb.fRed, rgb.fGreen, rgb.fBlue, 0)		\
	}


#define CONVERT_PACKED_BYTE_1(dest_format)		\
	CONVERT_PACKED_PIXEL(7, 0x01, dest_format)	\
	CONVERT_PACKED_PIXEL(6, 0x01, dest_format)	\
	CONVERT_PACKED_PIXEL(5, 0x01, dest_format)	\
	CONVERT_PACKED_PIXEL(4, 0x01, dest_format)	\
	CONVERT_PACKED_PIXEL(3, 0x01, dest_format)	\
	CONVERT_PACKED_PIXEL(2, 0x01, dest_format)	\
	CONVERT_PACKED_PIXEL(1, 0x01, dest_format)	\
	CONVERT_PACKED_PIXEL(0, 0x01, dest_format)


#define CONVERT_PACKED_BYTE_2(dest_format)		\
	CONVERT_PACKED_PIXEL(6, 0x03, dest_format)	\
	CONVERT_PACKED_PIXEL(4, 0x03, dest_format)	\
	CONVERT_PACKED_PIXEL(2, 0x03, dest_format)	\
	CONVERT_PACKED_PIXEL(0, 0x03, dest_format)


#define CONVERT_PACKED_BYTE_4(dest_format)		\
	CONVERT_PACKED_PIXEL(4, 0x0F, dest_format)	\
	CONVERT_PACKED_PIXEL(0, 0x0F, dest_format)


#define CONVERT_PACKED_BYTE_8(dest_format)								\
	if (xx++ >= right)													\
		break;															\
																		\
	const RGBType&	rgb = (*srcColors)[bits];							\
	PUT_##dest_format(destPtr, rgb.fRed, rgb.fGreen, rgb.fBlue, 0)


#define STD_NO_CONVERT(BPP)									\
	long	numBytes = ((parms.fRight * BPP) + 7) / 8;		\
	memcpy (parms.fDestScanline, parms.fSrcScanline, numBytes);


#define STD_DIRECT_CONVERT(src_format, dest_format)			\
	EmCoord			right	= parms.fRight;					\
	uint8*			destPtr	= parms.fDestScanline;			\
	const uint8*	srcPtr	= parms.fSrcScanline;			\
															\
	for (EmCoord xx = 0; xx < right; ++xx)					\
	{														\
		uint8	r, g, b, a;									\
		GET_##src_format(srcPtr, r, g, b, a)				\
		PUT_##dest_format(destPtr, r, g, b, a)				\
	}


#define STD_INDEX_TO_DIRECT_CONVERT(src_format, dest_format)	\
	EmCoord			right		= parms.fRight;				\
	uint8*			destPtr		= parms.fDestScanline;		\
	const uint8*	srcPtr		= parms.fSrcScanline;		\
	const RGBList*	srcColors	= parms.fSrcColors;			\
															\
	EmAssert (srcColors);										\
															\
	EmCoord xx = 0;											\
	while (1)												\
	{														\
		uint8	bits = *srcPtr++;							\
		CONVERT_PACKED_BYTE_##src_format(dest_format)		\
	}


#define STD_DIRECT_TO_1_CONVERT(src_format)					\
	EmCoord			right		= parms.fRight;				\
	uint8*			destPtr		= parms.fDestScanline;		\
	const uint8*	srcPtr		= parms.fSrcScanline;		\
															\
	uint8			bitMask		= 0x80;						\
	uint8			aByte		= 0;						\
															\
	for (EmCoord xx = 0; xx < right; ++xx)					\
	{														\
		uint8	r, g, b, a;									\
		GET_##src_format(srcPtr, r, g, b, a)				\
															\
		Bool	isDark1 = r < 0xC0;							\
		Bool	isDark2 = g < 0xC0;							\
		Bool	isDark3 = b < 0xC0;							\
															\
		if (isDark1 || isDark2 || isDark3)					\
		{													\
			/* Assumes white/black color table! */			\
			aByte |= bitMask;								\
		}													\
															\
		bitMask >>= 1;										\
		if (bitMask == 0)									\
		{													\
			*destPtr++	= aByte;							\
			bitMask		= 0x80;								\
			aByte		= 0;								\
		}													\
	}														\
															\
	/* Write out any partially filled out byte. */			\
															\
	if (bitMask != 0)										\
	{														\
		*destPtr++ = aByte;									\
	}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap constructor
 *
 * DESCRIPTION:	Intialize data members.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

EmPixMap::EmPixMap		(void) :
	fSize (0, 0),
	fFormat (kPixMapFormat1),
	fRowBytes (0),
	fColors (),
	fPixels (NULL),
	fPixelsOwned (true)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap copy constructor
 *
 * DESCRIPTION:	Create a new EmPixMap that is a clone of "other".
 *
 * PARAMETERS:	other - pixmap to make a copy of.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

EmPixMap::EmPixMap		(const EmPixMap& other) :
	fSize (other.fSize),
	fFormat (other.fFormat),
	fRowBytes (other.fRowBytes),
	fColors (other.fColors),
	fPixels (NULL),
	fPixelsOwned (true)
{
	this->CopyPixelBuffer (other);
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap destructor
 *
 * DESCRIPTION:	Reclaim resources.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

EmPixMap::~EmPixMap		(void)
{
	if (fPixelsOwned)
	{
		Platform::DisposeMemory (fPixels);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap assignment operator
 *
 * DESCRIPTION:	Make "*this" look like "other".
 *
 * PARAMETERS:	other - source pixmap.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

EmPixMap&
EmPixMap::operator=		(const EmPixMap& other)
{
	if (&other != this)
	{
		fSize		= other.fSize;
		fFormat		= other.fFormat;
		fRowBytes	= other.fRowBytes;
		fColors		= other.fColors;

		this->CopyPixelBuffer (other);
	}

	return *this;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::GetSize
 *
 * DESCRIPTION:	Return the size of the image.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	EmPoint containing the image size.
 *
 ***********************************************************************/

EmPoint
EmPixMap::GetSize		(void) const
{
	return fSize;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::SetSize
 *
 * DESCRIPTION:	Set the size of the image.  Any old image is discarded
 *				and a new buffer is allocated.
 *
 * PARAMETERS:	p - new image size.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::SetSize		(const EmPoint& p)
{
	if (fSize != p)
	{
		fSize = p;
		fRowBytes = this->DetermineRowBytes ();
		this->InvalidateBuffer ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::GetDepth
 *
 * DESCRIPTION:	Return the image depth (bits per pixel).
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The image depth.
 *
 ***********************************************************************/

EmPixMapDepth
EmPixMap::GetDepth		(void) const
{
	return ::PrvGetDepth (fFormat);
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::GetFormat
 *
 * DESCRIPTION:	Return the format of the pixels.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The pixel format.
 *
 ***********************************************************************/

EmPixMapFormat
EmPixMap::GetFormat		(void) const
{
	return fFormat;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::SetFormat
 *
 * DESCRIPTION:	Set the bitdepth of the image.  Any old image is
 *				discarded and a new buffer is allocated.
 *
 * PARAMETERS:	d - new bith depth.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::SetFormat	(EmPixMapFormat f)
{
	EmPixMapDepth	oldDepth = this->GetDepth ();

	fFormat = f;

	if (oldDepth != this->GetDepth ())
	{
		fRowBytes = this->DetermineRowBytes ();
		this->InvalidateBuffer ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::GetRowBytes
 *
 * DESCRIPTION:	Return the number of bytes in a scanline.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The number of bytes in a scanline.
 *
 ***********************************************************************/

EmPixMapRowBytes
EmPixMap::GetRowBytes		(void) const
{
	return fRowBytes;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::SetRowBytes
 *
 * DESCRIPTION:	Set the number of bytes in a scanline.  Any old image is
 *				discarded and a new buffer is allocated.  In general,
 *				you should not have to call this function; a rowbytes
 *				value is determined based on the width and depth of the
 *				image.  You should call SetRowBytes only under
 *				exceptional circumstances to override the default.
 *
 * PARAMETERS:	b - new rowbytes value.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::SetRowBytes		(EmPixMapRowBytes b)
{
	if (fRowBytes != b)
	{
		fRowBytes = b;
		this->InvalidateBuffer ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::GetColorTable
 *
 * DESCRIPTION:	Return a *CONST REFERENCE* to the color table used by
 *				the image.  The color table will be empty for direct
 *				pixmaps (where the depth is > 8).  In general, the
 *				number of entries in the table will be (1 << depth),
 *				but nothing in EmPixMap itself relies on this.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Reference to the color table.
 *
 ***********************************************************************/

const RGBList&
EmPixMap::GetColorTable	(void) const
{
	return fColors;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::SetColorTable
 *
 * DESCRIPTION:	Set the color table to be used for indexed pixmaps
 *				(where the depth is <= 8).  It is up to the caller to
 *				make sure enough entries are supplied; EmPixMap assumes
 *				that there are.  Also, no color mapping is performed
 *				(that is, no effort is made to change the pixels in the
 *				buffer such that they end up pointing to the same or
 *				similar RGB values).
 *
 * PARAMETERS:	c - the new color table.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::SetColorTable	(const RGBList& c)
{
	fColors = c;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::GetBits
 *
 * DESCRIPTION:	Return a pointer to the pixel buffer.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Pointer to the pixel buffer.
 *
 ***********************************************************************/

const void*
EmPixMap::GetBits			(void) const
{
	this->UpdateBuffer ();
	return &fPixels[0];
}

void*
EmPixMap::GetBits			(void)
{
	this->UpdateBuffer ();
	return &fPixels[0];
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::SetBits
 *
 * DESCRIPTION:	Set the pixels to be used by the pixmap.  Note that the
 *				pixels are NOT OWNED, nor are they copied!  This
 *				function is used to wrap up EXISTING pixels from some
 *				other source (such as a host pixmap).
 *
 * PARAMETERS:	bits - bits to adopt.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::SetBits			(void* bits)
{
	this->InvalidateBuffer ();
	fPixels = (uint8*) bits;
	fPixelsOwned = false;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::CreateMask
 *
 * DESCRIPTION:	Change dest to be a 1-bpp pixmap containing a mask for
 *				the "this" pixmap.  The mask will contain black pixels
 *				for the parts inside the mask, and white pixels for the
 *				parts outside the mask.  The mask created is similar to
 *				that used by the Macintosh Finder to define the boundary
 *				of an icon.
 *
 * PARAMETERS:	dest - pixmap that will receive the mask.  The caller
 *					does not need to prepare this pixmap in any way; it
 *					gets completely reinitialized by this function.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmPixMap::CreateMask (EmPixMap& dest) const
{
	// Create a simple black & white color table.

	RGBList		colors;
	colors.push_back (RGBType (255, 255, 255));		// 0 = white
	colors.push_back (RGBType (0, 0, 0));			// 1 = black

	// Convert (*this) into a 1-bpp bitmap, putting the result in src.

	EmPixMap	src;
	src.SetSize (this->GetSize ());
	src.SetFormat (kPixMapFormat1);
	src.SetColorTable (colors);

	EmRect	rect (EmPoint (0, 0), this->GetSize());

	EmPixMap::CopyRect (src, *this, rect, rect);

	// Initialize the destination bitmap.

	dest.SetSize (this->GetSize ());
	dest.SetFormat (kPixMapFormat1);
	dest.SetColorTable (colors);

	// Call low-level routine to create the mask.

	::PrvMakeMask (dest.GetBits (), src.GetBits (), src.GetRowBytes (),
		src.GetSize().fX, src.GetSize().fY);
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::CreateRegion
 *
 * DESCRIPTION:	Scan-convert a 1-bpp bitmap into a region.  Zero bits
 *				are outside the region and one bits are inside it.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The bitmap converted into a region.
 *
 ***********************************************************************/

EmRegion
EmPixMap::CreateRegion	(void) const
{
	EmRegion			region;

	uint8*				bits		= (uint8*) this->GetBits ();
	EmCoord				width		= this->GetSize().fX;
	EmCoord				height		= this->GetSize().fY;
	EmPixMapRowBytes	rowBytes	= this->GetRowBytes ();

	for (EmCoord yy = 0; yy < height; ++yy)
	{
		Bool	wasInRegion = false;
		long	xStart = 0;
		long	xEnd = 0;

		uint8*	cBits = bits + yy * rowBytes;

		uint8	aByte = 0;
		uint8	bitMask = 0;

		for (long xx = 0; xx < width; ++xx, bitMask >>= 1)
		{
			if (bitMask == 0)
			{
				bitMask = 0x80;
				aByte = *cBits++;
			}

			Bool	nowInRegion = (aByte & bitMask) != 0;

			// Entering a region?

			if (nowInRegion && !wasInRegion)
			{
				// Start new "in region" scan

				xStart = xx;
			}

			// Exiting a region?

			else if (!nowInRegion && wasInRegion)
			{
				// Close off "in region" scan.

				xEnd = xx;
				::PrvAddToRegion (region, yy, xStart, xEnd);
			}

			wasInRegion = nowInRegion;
		}

		// End of scan line. Check to see if the region scan is still
		// open.  If so, close it.

		if (wasInRegion)
		{
			xEnd = width;
			::PrvAddToRegion (region, yy, xStart, xEnd);
		}
	}

	return region;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::ChangeTone
 *
 * DESCRIPTION:	Adjust the pixmap in the given line range by the given
 *				percentage.
 *
 * PARAMETERS:	amount - amount by which the image should be adjusted.
 *					100			= White
 *					1...99		= Lighten
 *					0			= No change
 *					-1...-99	= Darken
 *					-100		= Black
 *
 *				firstLine - first scanline in the image to alter.
 *
 *				lastLine - last scanline in the image to alter plus 1.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

inline void PrvAdjustPixel_ChangeTone (uint8& r, uint8& g, uint8& b, int32 amount)
{
	if (amount >= 0)
	{
		// Lighten the value.
		//
		// If amount == 0, rhs == 0, leaving value unchanged.
		//
		// If amount == 255, rhs == 255 - (value * 255 / 255),
		// == 255 - value, leaving value at 255.

		r += (amount - (r * amount / 255));
		g += (amount - (g * amount / 255));
		b += (amount - (b * amount / 255));
	}
	else
	{
		// Darken the value.
		//
		// If amount == 0, rhs == 0, leaving value unchanged.
		//
		// If amount == -255, rhs == value * -255 / 255 == -value, leaving value at zero.

		r += (r * amount / 255);
		g += (g * amount / 255);
		b += (b * amount / 255);
	}
}


void
EmPixMap::ChangeTone (int32 percent, EmCoord firstLine, EmCoord lastLine)
{
	EmAssert (percent >= -100);
	EmAssert (percent <= 100);

	EmPixMapDepth	depth		= this->GetDepth ();
	EmPixMapFormat	format		= this->GetFormat ();
	int32			amount		= (percent * 255) / 100;

	if (firstLine == -1)
		firstLine = 0;

	if (lastLine == -1)
		lastLine = this->GetSize ().fY;

	// If the image has a color table, alter that.

	if (depth <= 8)
	{
		RGBList::iterator	iter = fColors.begin ();
		while (iter != fColors.end ())
		{
			// Munge the RGB values directly.  You might be tempted to
			// convert the RGB values to HSV values, alter the V
			// parameter, and then convert back.  However, that gives
			// pretty much the same results.

			RGBType&	rgb	= *iter;

			::PrvAdjustPixel_ChangeTone (rgb.fRed, rgb.fGreen, rgb.fBlue, amount);

			++iter;
		}
	}

	// If the image is direct, alter that.

	else
	{
		uint8*				image		= (uint8*) this->GetBits ();
		EmPixMapRowBytes	rowBytes	= this->GetRowBytes ();
		EmCoord				width		= this->GetSize ().fX;

		for (int yy = firstLine; yy < lastLine; ++yy)
		{
			uint8*	scanline	= image + rowBytes * yy;
			uint8*	pixPtr		= scanline;

			for (int xx = 0; xx < width; ++xx)
			{
				uint8	r, g, b, a;

				switch (format)
				{
					case kPixMapFormat16RGB565:	GET_16RGB565 (pixPtr, r, g, b, a);		break;
					case kPixMapFormat24RGB:	GET_24RGB (pixPtr, r, g, b, a);		break;
					case kPixMapFormat24BGR:	GET_24BGR (pixPtr, r, g, b, a);		break;
					case kPixMapFormat32ARGB:	GET_32ARGB (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32ABGR:	GET_32ABGR (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32RGBA:	GET_32RGBA (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32BGRA:	GET_32BGRA (pixPtr, r, g, b, a);	break;
					default:	EmAssert (false);	r = g = b = a = 0;
				}

				// Munge the RGB values directly.  You might be tempted to
				// convert the RGB values to HSV values, alter the V
				// parameter, and then convert back.  However, that gives
				// pretty much the same results.

				::PrvAdjustPixel_ChangeTone (r, g, b, amount);

				switch (format)
				{
					case kPixMapFormat16RGB565:	pixPtr -= 2;	PUT_16RGB565 (pixPtr, r, g, b, a);		break;
					case kPixMapFormat24RGB:	pixPtr -= 3;	PUT_24RGB (pixPtr, r, g, b, a);		break;
					case kPixMapFormat24BGR:	pixPtr -= 3;	PUT_24BGR (pixPtr, r, g, b, a);		break;
					case kPixMapFormat32ARGB:	pixPtr -= 4;	PUT_32ARGB (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32ABGR:	pixPtr -= 4;	PUT_32ABGR (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32RGBA:	pixPtr -= 4;	PUT_32RGBA (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32BGRA:	pixPtr -= 4;	PUT_32BGRA (pixPtr, r, g, b, a);	break;
					default:	EmAssert (false);
				}
			}
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::ConvertToColor
 *
 * DESCRIPTION:	Adjust the pixmap in the given line range by the given
 *				percentage.
 *
 * PARAMETERS:	type - color to which the pixmap should be converted.
 *					100			= White
 *					1...99		= Lighten
 *					0			= No change
 *					-1...-99	= Darken
 *					-100		= Black
 *
 *				firstLine - first scanline in the image to alter.
 *
 *				lastLine - last scanline in the image to alter plus 1.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

inline void PrvAdjustPixel_ConvertToColor (uint8& r, uint8& g, uint8& b, int type)
{
	// First convert to grayscale.

	uint8	gray = (uint8) ((r * 0.2990) + (g * 0.5870) + (b * 0.1140));

	// The assign the 8-bit grayscale value accordingly.

	switch (type)
	{
		case 0:
			r = gray;
			g = gray;
			b = gray;
			break;

		case 1:
			r = gray;
			g = 0;
			b = 0;
			break;

		case 2:
			r = 0;
			g = gray;
			b = 0;
			break;

		case 3:
			r = 0;
			g = 0;
			b = gray;
			break;
	}
}


void
EmPixMap::ConvertToColor (int type, EmCoord firstLine, EmCoord lastLine)
{
	EmAssert (type >= 0);
	EmAssert (type <= 3);

	EmPixMapDepth	depth		= this->GetDepth ();
	EmPixMapFormat	format		= this->GetFormat ();

	if (firstLine == -1)
		firstLine = 0;

	if (lastLine == -1)
		lastLine = this->GetSize ().fY;

	// If the image has a color table, alter that.

	if (depth <= 8)
	{
		RGBList::iterator	iter = fColors.begin ();
		while (iter != fColors.end ())
		{
			// Munge the RGB values directly.

			RGBType&	rgb	= *iter;

			::PrvAdjustPixel_ConvertToColor (rgb.fRed, rgb.fGreen, rgb.fBlue, type);

			++iter;
		}
	}

	// If the image is direct, alter that.

	else
	{
		uint8*				image		= (uint8*) this->GetBits ();
		EmPixMapRowBytes	rowBytes	= this->GetRowBytes ();
		EmCoord				width		= this->GetSize ().fX;

		for (int yy = firstLine; yy < lastLine; ++yy)
		{
			uint8*	scanline	= image + rowBytes * yy;
			uint8*	pixPtr		= scanline;

			for (int xx = 0; xx < width; ++xx)
			{
				uint8	r, g, b, a;

				switch (format)
				{
					case kPixMapFormat16RGB565:	GET_16RGB565 (pixPtr, r, g, b, a);		break;
					case kPixMapFormat24RGB:	GET_24RGB (pixPtr, r, g, b, a);		break;
					case kPixMapFormat24BGR:	GET_24BGR (pixPtr, r, g, b, a);		break;
					case kPixMapFormat32ARGB:	GET_32ARGB (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32ABGR:	GET_32ABGR (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32RGBA:	GET_32RGBA (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32BGRA:	GET_32BGRA (pixPtr, r, g, b, a);	break;
					default:	EmAssert (false);	r = g = b = a = 0;
				}

				// Munge the RGB values directly.

				::PrvAdjustPixel_ConvertToColor (r, g, b, type);

				switch (format)
				{
					case kPixMapFormat16RGB565:	pixPtr -= 2;	PUT_16RGB565 (pixPtr, r, g, b, a);		break;
					case kPixMapFormat24RGB:	pixPtr -= 3;	PUT_24RGB (pixPtr, r, g, b, a);		break;
					case kPixMapFormat24BGR:	pixPtr -= 3;	PUT_24BGR (pixPtr, r, g, b, a);		break;
					case kPixMapFormat32ARGB:	pixPtr -= 4;	PUT_32ARGB (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32ABGR:	pixPtr -= 4;	PUT_32ABGR (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32RGBA:	pixPtr -= 4;	PUT_32RGBA (pixPtr, r, g, b, a);	break;
					case kPixMapFormat32BGRA:	pixPtr -= 4;	PUT_32BGRA (pixPtr, r, g, b, a);	break;
					default:	EmAssert (false);
				}
			}
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::FlipScanlines
 *
 * DESCRIPTION:	Invert the image in the pixel buffer, swapping all the
 *				pixels around the x-axis horizontally bisecting the image.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::FlipScanlines	(void)
{
	long	height		= fSize.fY;
	long	rowBytes	= fRowBytes;
	uint8*	topLine		= (uint8*) this->GetBits ();
	uint8*	bottomLine	= topLine + (height - 1) * rowBytes;

	void*	tempBuffer	= Platform::AllocateMemory (rowBytes);

	while (bottomLine > topLine)
	{
		memcpy (tempBuffer, topLine, rowBytes);
		memcpy (topLine, bottomLine, rowBytes);
		memcpy (bottomLine, tempBuffer, rowBytes);

		topLine += rowBytes;
		bottomLine -= rowBytes;
	}

	Platform::DisposeMemory (tempBuffer);
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::ConvertToFormat
 *
 * DESCRIPTION:	Convert "*this" to be a pixmap of the given depth,
 *				preserving the pixels in the current buffer.
 *
 * PARAMETERS:	destDepth - the new depth of the pixmap.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::ConvertToFormat	(EmPixMapFormat destFormat)
{
	// Copy our pixels to a new pixmap, converting along the way.

	EmPixMap	dest;
	dest.SetSize (this->GetSize ());
	dest.SetFormat (destFormat);
	dest.SetColorTable (this->GetColorTable ());

	EmRect	rect (EmPoint (0, 0), this->GetSize());

	EmPixMap::CopyRect (dest, *this, rect, rect);

	// Now copy those newly-converted pixels back to us.

	this->SetFormat (destFormat);
	this->CopyPixelBuffer (dest);
}


/***********************************************************************
 *
 * FUNCTION:	CopyRect
 *
 * DESCRIPTION:	Copy pixels from src pixmap to dest pixmap, copying
 *				from srcRect to destRect, converting and scaling along
 *				the way.
 *
 *				RESTRICTIONS: Currently, no translation of pixels can
 *					occur.  Additionally, the only scaling allowed
 *					is scaling up by a factor of two.  Finally, no
 *					depth conversions will take place if the process
 *					requires an inverse table lookup.
 *
 * PARAMETERS:	dest - pixmap to receive the pixels.
 *
 *				src - pixmap to provide the pixels.
 *
 *				destRect - area within dest to where the pixels will be
 *					copied.
 *
 *				srcRect - area within src from where the pixels will be
 *					copied.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::CopyRect	(EmPixMap& dest, const EmPixMap& src,
					 const EmRect& destRect, const EmRect& srcRect)
{
	// Validate the parameters; we don't support everything just yet.

	EmAssert (destRect.fLeft == 0);
	EmAssert (srcRect.fLeft == 0);

	EmAssert (destRect.fRight == dest.GetSize().fX);
	EmAssert (srcRect.fRight == src.GetSize().fX);

	EmAssert (srcRect.fRight	== destRect.fRight	|| srcRect.fRight * 2	== destRect.fRight);
	EmAssert (srcRect.fTop	== destRect.fTop	|| srcRect.fTop * 2		== destRect.fTop);
	EmAssert (srcRect.fBottom	== destRect.fBottom	|| srcRect.fBottom * 2	== destRect.fBottom);

	// Gather data common to all conversions.

	EmPixMapRowBytes	destRowBytes	= dest.GetRowBytes ();
	EmPixMapRowBytes	srcRowBytes		= src.GetRowBytes ();

	EmCoord				top				= srcRect.fTop;
	EmCoord				bottom			= srcRect.fBottom;

	ScanlineParms		parms;

	parms.fDestScanline		= ((uint8*) dest.GetBits ()) + top * destRowBytes;
	parms.fSrcScanline		= ((uint8*) src.GetBits ()) + top * srcRowBytes;

	parms.fDestColors		= dest.GetDepth () <= 8 ? &dest.GetColorTable () : NULL;
	parms.fSrcColors		= src.GetDepth () <= 8 ? &src.GetColorTable () : NULL;

	parms.fLeft				= srcRect.fLeft;
	parms.fRight			= srcRect.fRight;


	// Determine what scanline converter to use.

	ScanlineConverter	conv = NULL;
	long	srcDestFormat = (((uint32) src.GetFormat ()) << 16) | (uint32) dest.GetFormat ();

	switch (srcDestFormat)
	{

#define CONVERTER_CASE(src_format, dest_format)					\
		case (((uint32) kPixMapFormat##src_format) << 16) | (uint32) kPixMapFormat##dest_format:	\
			conv = &::PrvConvert##src_format##To##dest_format;	\
			break;

		FOR_EACH_FORMAT_PAIR (CONVERTER_CASE)
	}

	EmAssert (conv);

	// Create any tables we (might) need.

	::Get1To8Table ();
	::Get2To8Table ();
	::Get4To8Table ();

	// Do the conversion.

	for (EmCoord yy = top; yy < bottom; ++yy)
	{
		conv (parms);

		parms.fDestScanline += destRowBytes;
		parms.fSrcScanline += srcRowBytes;
	}

	if (destRect != srcRect)
	{
		// Now scale up.  The pixels have been copied in 8-, 16-, 24- or
		// 32-bit format to the upper-left of the dest pixmap.  Expand
		// that guy both horizontally and vertically.  In order to not
		// smash the pixels already there (we're converting the dest
		// pixmap "in place"), scale from bottom to top and right to left.

		EmPixMapDepth	destDepth			= dest.GetDepth ();
		uint8*			destBasePtr			= (uint8*) dest.GetBits ();
		long			halfDestRowBytes	= destRowBytes / 2;
		uint8*			srcLinePtr			= destBasePtr + bottom * destRowBytes + halfDestRowBytes;
		uint8*			destLinePtr			= destBasePtr + bottom * destRowBytes * 2 + destRowBytes;

		for (EmCoord yy = top; yy < bottom; ++yy)
		{
			srcLinePtr -= destRowBytes;
			destLinePtr -= destRowBytes + destRowBytes;

			uint8*	srcPtr		= srcLinePtr;
			uint8*	destPtr1	= destLinePtr;
			uint8*	destPtr2	= destPtr1 + destRowBytes;
			int		xx			= halfDestRowBytes;

			if (destDepth == 8)
			{
				while ((xx -= 1) >= 0)
				{
					uint8	c1	= *--srcPtr;

					*--destPtr1 = *--destPtr2 = c1;
					*--destPtr1 = *--destPtr2 = c1;
				}
			}
			else if (destDepth == 16)
			{
				while ((xx -= 2) >= 0)
				{
					uint8	c1	= *--srcPtr;
					uint8	c2	= *--srcPtr;

					*--destPtr1 = *--destPtr2 = c1;
					*--destPtr1 = *--destPtr2 = c2;

					*--destPtr1 = *--destPtr2 = c1;
					*--destPtr1 = *--destPtr2 = c2;
				}
                        }
			else if (destDepth == 24)
			{
				while ((xx -= 3) >= 0)
				{
					uint8	c1	= *--srcPtr;
					uint8	c2	= *--srcPtr;
					uint8	c3	= *--srcPtr;

					*--destPtr1 = *--destPtr2 = c1;
					*--destPtr1 = *--destPtr2 = c2;
					*--destPtr1 = *--destPtr2 = c3;

					*--destPtr1 = *--destPtr2 = c1;
					*--destPtr1 = *--destPtr2 = c2;
					*--destPtr1 = *--destPtr2 = c3;
				}
			}
			else if (destDepth == 32)
			{
				while ((xx -= 4) >= 0)
				{
					uint8	c1	= *--srcPtr;
					uint8	c2	= *--srcPtr;
					uint8	c3	= *--srcPtr;
					uint8	c4	= *--srcPtr;

					*--destPtr1 = *--destPtr2 = c1;
					*--destPtr1 = *--destPtr2 = c2;
					*--destPtr1 = *--destPtr2 = c3;
					*--destPtr1 = *--destPtr2 = c4;

					*--destPtr1 = *--destPtr2 = c1;
					*--destPtr1 = *--destPtr2 = c2;
					*--destPtr1 = *--destPtr2 = c3;
					*--destPtr1 = *--destPtr2 = c4;
				}
			}
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::DetermineRowBytes
 *
 * DESCRIPTION:	Determine a rowbytes value appropriate for the current
 *				pixel depth and image width.  Currently, the value
 *				returned is the minimum number of bytes required,
 *				rounded up to a multiple of four.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Suggested row bytes value.
 *
 ***********************************************************************/

EmPixMapRowBytes
EmPixMap::DetermineRowBytes	(void) const
{
	// RowBytes rounded up to 32-bit boundaries.

	long			width = fSize.fX;
	EmPixMapDepth	depth = this->GetDepth ();

	return ((width * depth + 31) & ~31) / 8;
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::InvalidateBuffer
 *
 * DESCRIPTION:	Mark the current pixel buffer as invalid.  The next time
 *				the buffer is accessed, it will be regenerated.  This
 *				function is used after SetBounds, SetDepth, or SetRowBytes
 *				is called.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::InvalidateBuffer (void)
{
	if (fPixelsOwned)
	{
		Platform::DisposeMemory (fPixels);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::UpdateBuffer
 *
 * DESCRIPTION:	Allocate a new image buffer if the old one has been
 *				marked invalid by InvalidateBuffer.  Called by GetBits
 *				before returning the address of the buffer.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::UpdateBuffer (void) const
{
	if (!fPixels)
	{
		long	neededSize = fSize.fY * fRowBytes;
		const_cast <EmPixMap*> (this)->fPixels =
			(uint8*) Platform::AllocateMemory (neededSize);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPixMap::CopyPixelBuffer
 *
 * DESCRIPTION:	Utility function to copy the pixels of one pixmap to
 *				another.  Used as a helper function in the copy
 *				constructor and assignment operator.
 *
 * PARAMETERS:	other - source pixmap.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
EmPixMap::CopyPixelBuffer (const EmPixMap& other)
{
	this->InvalidateBuffer ();

	memcpy (this->GetBits (), other.GetBits (), fSize.fY * fRowBytes);
}


/***********************************************************************
 *
 * FUNCTION:	Get1To8Table
 *
 * DESCRIPTION:	Return a table that can be used to convert 1-bpp pixels
 *				into 8-bpp pixels.  The table is 16 entries long, and
 *				so is indexed with a full nybble.  The result is a
 *				32-bit value, where each of the 4 1-bpp pixels have been
 *				converted to 4 8-bpp pixels.
 *
 *				Nybbles of the form 0b0000wxyz are converted to:
 *
 *					0b0000000w0000000x0000000y0000000z
 *
 *				Note that the 32-bit value returned is already
 *				byteswapped so that it appears correctly in memory when
 *				stored on Little-Endian architectures.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Pointer to the table.
 *
 ***********************************************************************/

uint32* Get1To8Table (void)
{
	if (!gConvert1To8)
	{
		const int	kTableSize = 16;
		gConvert1To8 = (uint32*) Platform::AllocateMemory (sizeof (*gConvert1To8) * kTableSize);

		for (int ii = 0; ii < kTableSize; ++ii)
		{
			uint32 pixels =	((ii & 0x08) << (24 - 3)) |
							((ii & 0x04) << (16 - 2)) |
							((ii & 0x02) <<  (8 - 1)) |
							((ii & 0x01) <<  (0 - 0));

			gConvert1To8[ii] = pixels;
		}
	}

	return gConvert1To8;
}


/***********************************************************************
 *
 * FUNCTION:	Get2To8Table
 *
 * DESCRIPTION:	Return a table that can be used to convert 1-bpp pixels
 *				into 8-bpp pixels.  The table is 256 entries long, and
 *				so is indexed with a full byte.  The result is a
 *				32-bit value, where each of the 4 2-bpp pixels have been
 *				converted to 4 8-bpp pixels.
 *
 *				Bytes of the form 0bwwxxyyzz are converted to:
 *
 *					0b000000ww000000xx000000yy000000zz
 *
 *				Note that the 32-bit value returned is already
 *				byteswapped so that it appears correctly in memory when
 *				stored on Little-Endian architectures.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Pointer to the table.
 *
 ***********************************************************************/

uint32* Get2To8Table (void)
{
	if (!gConvert2To8)
	{
		const int	kTableSize = 256;
		gConvert2To8 = (uint32*) Platform::AllocateMemory (sizeof (*gConvert2To8) * kTableSize);

		for (int ii = 0; ii < kTableSize; ++ii)
		{
			uint32 pixels =	((ii & 0xC0) << (24 - 6)) |
							((ii & 0x30) << (16 - 4)) |
							((ii & 0x0C) <<  (8 - 2)) |
							((ii & 0x03) <<  (0 - 0));

			gConvert2To8[ii] = pixels;
		}
	}

	return gConvert2To8;
}


/***********************************************************************
 *
 * FUNCTION:	Get4To8Table
 *
 * DESCRIPTION:	Return a table that can be used to convert 4-bpp pixels
 *				into 8-bpp pixels.  The table is 256 entries long, and
 *				so is indexed with a full byte.  The result is a
 *				16-bit value, where each of the 2 4-bpp pixels have been
 *				converted to 2 8-bpp pixels.
 *
 *				Bytes of the form 0bxxxxyyyy are converted to:
 *
 *					0b000xxxx0000yyyy
 *
 *				Note that the 16-bit value returned is already
 *				byteswapped so that it appears correctly in memory when
 *				stored on Little-Endian architectures.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Pointer to the table.
 *
 ***********************************************************************/

uint16* Get4To8Table (void)
{
	if (!gConvert4To8)
	{
		const int	kTableSize = 256;		
		gConvert4To8 = (uint16*) Platform::AllocateMemory (sizeof (*gConvert4To8) * kTableSize);

		for (int ii = 0; ii < kTableSize; ++ii)
		{
			uint16 pixels =	((ii & 0xF0) << (8 - 4)) |
							((ii & 0x0F) << (0 - 0));

			gConvert4To8[ii] = pixels;
		}
	}

	return gConvert4To8;
}


/***********************************************************************
 *
 * FUNCTION:	PrvMakeMask
 *
 * DESCRIPTION:	Calculate a mask for the given 1-bpp image.  The mask
 *				contains 1's inside the image, and 0's outside the
 *				image (where the image is defined as the closed outer
 *				boundary of 1 pixels in the source).  Thus:
 *
 *						src				dest
 *					...........		...........
 *					.....*.....		.....*.....
 *					....*.*....		....***....
 *					...*...*...	=>	...*****...
 *					....*.*....		....***....
 *					.....*.....		.....*.....
 *					...........		...........
 *
 *				...where .'s are 0's, and *'s are 1's.
 *
 *				The algorithm is interesting.  It starts by initializing
 *				the dest bitmap to all black.  It then creates white
 *				sentinel pixels all around the src bitmap.  These
 *				sentinels are not actually inserted into the bitmap,
 *				but are logically and effectively there.  The sentinels
 *				are then used to start a process of "smearing", where
 *				white pixels are smeared left, right, up, and down
 *				until they reach black pixels in the src.  This process
 *				continues until no changes are made in the destination.
 *
 *				I can't find any reference to this algorithm on the
 *				net.  This implementation is based on an algorithm
 *				described to me by Jerry(?) Harris while I was at
 *				Taligent.
 *
 * PARAMETERS:	dstPtr - buffer to hold the generated mask.  Caller is
 *					responsible for making the buffer is large enough.
 *
 *				srcPtr - pointer to the input 1-bpp image.
 *
 *				rowBytes - number of bytes in a row (scanline).
 *
 *				width - width of the source image, in pixels.
 *
 *				height - height of the source image, in pixels.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

static void PrvMakeMask (void* dstPtr, void* srcPtr, long rowBytes,
							long /*width*/, long height)
{
	uint8*	src			= (uint8*) srcPtr;
	uint8*	dest		= (uint8*) dstPtr;
	uint8*	prev;
	uint8*	prev0;
	uint8	srcVal, destVal, prevDestVal, newDestVal;
	long 	absRowBytes	= rowBytes;
	long 	xx, yy;
	long 	valChanged, anyChange;
	long	pass		= 0;
	uint8	edge		= 0;	// Sentry for the left and right edges.

	// Set the destination bitmap to black (1's)

	memset (dest, ~0, rowBytes * height);

	// Allocate a scanline buffer on the stack.  This scanline
	// acts as a sentry at the top and bottom of the dest buffer.

	prev0 = (uint8*) Platform::AllocateMemory (rowBytes);
	memset (prev0, edge, rowBytes);

	do {	// Loop while we're still effectively smearing.
		++pass;
		prev = prev0;
		anyChange = false;
		yy = height;

		do {	// Loop through all the scanlines.

			// Smear down and to the right.

			prevDestVal = edge;
			xx = absRowBytes;

			do {	// Loop through all the bytes in this line.
				destVal = *dest;

				if (destVal != 0)	// If it's not already all-white, let's see if we can mess with it.
				{
					srcVal = *src;

					// The following code effectively pushes down any 0's from
					// the previous line, then adds any 1's from the source.

					destVal = (destVal & *prev) | srcVal;

					do {
						// Shift destVal to the right.	The upper bit of the
						// new value gets filled in with the lower bit of the
						// previous destVal.

						newDestVal = (destVal >> 1) | (prevDestVal << 7);

						// AND the shifted destVal with the pre-shifted destVal.
						// This has the effect of smearing any 0's to the right.

						newDestVal &= destVal;

						// OR in the corresponding byte from the source.  This
						// has the effect of stopping the smear when it hits a 1.

						newDestVal |= srcVal;

						// See if the destVal we're working on has changed.  If
						// so, keep smearing to the right.

						valChanged = (newDestVal != destVal);
						if (valChanged)
						{
							destVal = newDestVal;
						}
					} while (valChanged);

					// If this destVal has changed, record the fact and write
					// out the new value.

					if (*dest != destVal)
					{
						anyChange = true;
						*dest = destVal;
					}
				}

				// Move to the next bytes in this line.

				++src;
				++prev;
				++dest;
				prevDestVal = destVal;
			} while (--xx != 0);

			// Smear down and to the left.	Same logic as before.

			prevDestVal = edge;
			xx = absRowBytes;
			do {
				--src;
				--prev;
				--dest;
				destVal = *dest;

				if (destVal != 0)
				{
					srcVal = *src;
					destVal = (destVal & *prev) | srcVal;

					do {
						newDestVal = (destVal << 1) | (prevDestVal >> 7);
						newDestVal &= destVal;
						newDestVal |= srcVal;
						valChanged = (newDestVal != destVal);
						if (valChanged)
							destVal = newDestVal;
					} while (valChanged);

					if (*dest != destVal)
					{
						anyChange = true;
						*dest = destVal;
					}
				}

				prevDestVal = destVal;
			} while (--xx != 0);

			// Move to the next scanlines.	Our movement is either
			// up or down, depending on whether rowBytes is negative
			// or not.

			src += rowBytes;
			prev = dest;
			dest += rowBytes;
		} while (--yy != 0);

		// Switch direction, adjust ptrs, and loop until no change.
		// Make sure we do at least one up and one down pass, though.

		rowBytes = -rowBytes;
		src += rowBytes;
		dest += rowBytes;
	} while (anyChange || (pass < 2));

	Platform::DisposeMemory (prev0);
}


/***********************************************************************
 *
 * FUNCTION:	PrvAddToRegion
 *
 * DESCRIPTION:	Utility function that adds a rectangle with the given
 *				bounds to the given region.  If the given region is
 *				empty, the region is *set* to the rectangle.
 *
 * PARAMETERS:	region - region to update (or initialize).
 *
 *				top, left, right - coordinates of the rectangle (where
 *					the bottom is top + 1).
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void PrvAddToRegion (EmRegion& region, int top, int left, int right)
{
	if (region.IsEmpty ())
	{
		region = EmRegion (EmRect (left, top, right, top + 1));
	}
	else
	{
		EmRegion	tempRegion (EmRect (left, top, right, top + 1));
		region.UnionWith (tempRegion);
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetDepth
 *
 * DESCRIPTION:	Return the depth implied by the given format
 *
 * PARAMETERS:	f - format determining the depth.
 *
 * RETURNED:	The depth implied by the format.
 *
 ***********************************************************************/

EmPixMapDepth PrvGetDepth (EmPixMapFormat f)
{
	static const uint8 kDepthArray[] = 
		{ 1, 2, 4, 8, 16, 16, 16, 16, 24, 24, 32, 32, 32, 32 };

	EmAssert (f >= 0 && f < kPixMapFormatLast);
	COMPILE_TIME_ASSERT (countof (kDepthArray) == kPixMapFormatLast);

	return kDepthArray [f];
}


/***********************************************************************
 *
 * FUNCTION:	PrvConvertMToN
 *
 * DESCRIPTION:	Scanline converters.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

#pragma mark -

void PrvConvert1To1 (const ScanlineParms& parms)
{
	STD_NO_CONVERT(1)
}

void PrvConvert1To2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert1To4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert1To8 (const ScanlineParms& parms)
{
	// Note, this conversion assumes that the first two entries of
	// the destination color table contains the same colors as the
	// first two entries in the source color table.

	EmCoord			right	= parms.fRight;
	uint8*			destPtr	= parms.fDestScanline;
	const uint8*	srcPtr	= parms.fSrcScanline;

	for (EmCoord xx = 0; xx < right; )
	{
		// Get 8 pixels.

		uint8	bits = *srcPtr++;

		// Convert each nybble (which contains four 1-bit pixels) to
		// a 32-bit value containing four 8-bit pixels.

		uint32	p1 = gConvert1To8[(bits >>  4) & 0x0F];
		uint32	p2 = gConvert1To8[(bits >>  0) & 0x0F];

		if (xx++ < right)
			*destPtr++ = p1 >> 24;
		if (xx++ < right)
			*destPtr++ = p1 >> 16;
		if (xx++ < right)
			*destPtr++ = p1 >>  8;
		if (xx++ < right)
			*destPtr++ = p1 >>  0;

		if (xx++ < right)
			*destPtr++ = p2 >> 24;
		if (xx++ < right)
			*destPtr++ = p2 >> 16;
		if (xx++ < right)
			*destPtr++ = p2 >>  8;
		if (xx++ < right)
			*destPtr++ = p2 >>  0;
	}
}

void PrvConvert1To16RGB565 (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(1, 16RGB565)
}

void PrvConvert1To24RGB (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(1, 24RGB)
}

void PrvConvert1To24BGR (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(1, 24BGR)
}

void PrvConvert1To32ARGB (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(1, 32ARGB)
}

void PrvConvert1To32ABGR (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(1, 32ABGR)
}

void PrvConvert1To32RGBA (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(1, 32RGBA)
}

void PrvConvert1To32BGRA (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(1, 32BGRA)
}

#pragma mark -

void PrvConvert2To1 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert2To2 (const ScanlineParms& parms)
{
	STD_NO_CONVERT(2)
}

void PrvConvert2To4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert2To8 (const ScanlineParms& parms)
{
	// Note, this conversion assumes that the first four entries of
	// the destination color table contains the same colors as the
	// first four entries in the source color table.

	EmCoord			right	= parms.fRight;
	uint8*			destPtr	= parms.fDestScanline;
	const uint8*	srcPtr	= parms.fSrcScanline;

	for (EmCoord xx = 0; xx < right; )
	{
		// Get 4 pixels.

		uint8	bits = *srcPtr++;

		// Convert the byte (which contains four 2-bit pixels) to
		// a 32-bit value containing four 8-bit pixels.

		uint32	p = gConvert2To8[bits];

		if (xx++ < right)
			*destPtr++ = p >> 24;
		if (xx++ < right)
			*destPtr++ = p >> 16;
		if (xx++ < right)
			*destPtr++ = p >>  8;
		if (xx++ < right)
			*destPtr++ = p >>  0;
	}
}

void PrvConvert2To16RGB565 (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(2, 16RGB565)
}

void PrvConvert2To24RGB (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(2, 24RGB)
}

void PrvConvert2To24BGR (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(2, 24BGR)
}

void PrvConvert2To32ARGB (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(2, 32ARGB)
}

void PrvConvert2To32ABGR (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(2, 32ABGR)
}

void PrvConvert2To32RGBA (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(2, 32RGBA)
}

void PrvConvert2To32BGRA (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(2, 32BGRA)
}

#pragma mark -

void PrvConvert4To1 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert4To2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert4To4 (const ScanlineParms& parms)
{
	STD_NO_CONVERT(4)
}

void PrvConvert4To8 (const ScanlineParms& parms)
{
	// Note, this conversion assumes that the first 16 entries of
	// the destination color table contains the same colors as the
	// first 16 entries in the source color table.

	EmCoord			right	= parms.fRight;
	uint8*			destPtr	= parms.fDestScanline;
	const uint8*	srcPtr	= parms.fSrcScanline;

	for (EmCoord xx = 0; xx < right; )
	{
		// Get 2 pixels.

		uint8	bits = *srcPtr++;

		// Convert the byte (which contains two 4-bit pixels) to
		// a 16-bit value containing two 8-bit pixels.

		uint16	p = gConvert4To8[bits];

		if (xx++ < right)
			*destPtr++ = p >> 8;
		if (xx++ < right)
			*destPtr++ = p >> 0;
	}
}

void PrvConvert4To16RGB565 (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(4, 16RGB565)
}

void PrvConvert4To24RGB (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(4, 24RGB)
}

void PrvConvert4To24BGR (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(4, 24BGR)
}

void PrvConvert4To32ARGB (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(4, 32ARGB)
}

void PrvConvert4To32ABGR (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(4, 32ABGR)
}

void PrvConvert4To32RGBA (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(4, 32RGBA)
}

void PrvConvert4To32BGRA (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(4, 32BGRA)
}

#pragma mark -

void PrvConvert8To1 (const ScanlineParms& parms)
{
	EmCoord			right		= parms.fRight;
	uint8*			destPtr		= parms.fDestScanline;
	const uint8*	srcPtr		= parms.fSrcScanline;
	const RGBList*	srcColors	= parms.fSrcColors;

	EmAssert (srcColors);

	uint8			bitMask		= 0x80;
	uint8			aByte		= 0;

	for (EmCoord xx = 0; xx < right; ++xx)
	{
		// Get the RGB value.

		uint8			pixel = *srcPtr++;
		const RGBType&	rgb = (*srcColors)[pixel];

		// See if the current pixel is dark.

		Bool	isDark1 = rgb.fRed		< 0xC0;
		Bool	isDark2 = rgb.fGreen	< 0xC0;
		Bool	isDark3 = rgb.fBlue		< 0xC0;

		if (isDark1 || isDark2 || isDark3)
		{
			aByte |= bitMask;	// !!! Assumes white/black color table
		}

		bitMask >>= 1;
		if (bitMask == 0)
		{
			*destPtr++ = aByte;
			bitMask = 0x80;
			aByte = 0;
		}
	}

	// Write out any partially filled out byte.

	if (bitMask != 0)
	{
		*destPtr++ = aByte;
	}
}

void PrvConvert8To2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert8To4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert8To8 (const ScanlineParms& parms)
{
	STD_NO_CONVERT(8)
}

void PrvConvert8To16RGB565 (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(8, 16RGB565)
}

void PrvConvert8To24RGB (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(8, 24RGB)
}

void PrvConvert8To24BGR (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(8, 24BGR)
}

void PrvConvert8To32ARGB (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(8, 32ARGB)
}

void PrvConvert8To32ABGR (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(8, 32ABGR)
}

void PrvConvert8To32RGBA (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(8, 32RGBA)
}

void PrvConvert8To32BGRA (const ScanlineParms& parms)
{
	STD_INDEX_TO_DIRECT_CONVERT(8, 32BGRA)
}

#pragma mark -

void PrvConvert16RGB565To1 (const ScanlineParms& parms)
{
	STD_DIRECT_TO_1_CONVERT(16RGB565)
}

void PrvConvert16RGB565To2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert16RGB565To4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert16RGB565To8 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert16RGB565To16RGB565 (const ScanlineParms& parms)
{
	STD_NO_CONVERT(16)
}

void PrvConvert16RGB565To24RGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(16RGB565, 24RGB)
}

void PrvConvert16RGB565To24BGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(16RGB565, 24BGR)
}

void PrvConvert16RGB565To32ARGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(16RGB565, 32ARGB)
}

void PrvConvert16RGB565To32ABGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(16RGB565, 32ABGR )
}

void PrvConvert16RGB565To32RGBA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(16RGB565, 32RGBA)
}

void PrvConvert16RGB565To32BGRA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(16RGB565, 32BGRA)
}

#pragma mark -

void PrvConvert24RGBTo1 (const ScanlineParms& parms)
{
	STD_DIRECT_TO_1_CONVERT(24RGB)
}

void PrvConvert24RGBTo2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert24RGBTo4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert24RGBTo8 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert24RGBTo16RGB565 (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24RGB, 16RGB565)
}

void PrvConvert24RGBTo24RGB (const ScanlineParms& parms)
{
	STD_NO_CONVERT(24)
}

void PrvConvert24RGBTo24BGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24RGB, 24BGR)
}

void PrvConvert24RGBTo32ARGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24RGB, 32ARGB)
}

void PrvConvert24RGBTo32ABGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24RGB, 32ABGR )
}

void PrvConvert24RGBTo32RGBA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24RGB, 32RGBA)
}

void PrvConvert24RGBTo32BGRA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24RGB, 32BGRA)
}

#pragma mark -

void PrvConvert24BGRTo1 (const ScanlineParms& parms)
{
	STD_DIRECT_TO_1_CONVERT(24BGR)
}

void PrvConvert24BGRTo2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert24BGRTo4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert24BGRTo8 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert24BGRTo16RGB565 (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24BGR, 16RGB565)
}

void PrvConvert24BGRTo24RGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24BGR, 24RGB)
}

void PrvConvert24BGRTo24BGR (const ScanlineParms& parms)
{
	STD_NO_CONVERT(24)
}

void PrvConvert24BGRTo32ARGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24BGR, 32ARGB)
}

void PrvConvert24BGRTo32ABGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24BGR, 32ABGR)
}

void PrvConvert24BGRTo32RGBA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24BGR, 32RGBA )
}

void PrvConvert24BGRTo32BGRA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(24BGR, 32BGRA)
}

#pragma mark -

void PrvConvert32ARGBTo1 (const ScanlineParms& parms)
{
	STD_DIRECT_TO_1_CONVERT(32ARGB)
}

void PrvConvert32ARGBTo2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32ARGBTo4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32ARGBTo8 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32ARGBTo16RGB565 (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ARGB, 16RGB565)
}

void PrvConvert32ARGBTo24RGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ARGB, 24RGB)
}

void PrvConvert32ARGBTo24BGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ARGB, 24BGR)
}

void PrvConvert32ARGBTo32ARGB (const ScanlineParms& parms)
{
	STD_NO_CONVERT(32)
}

void PrvConvert32ARGBTo32ABGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ARGB, 32ABGR)
}

void PrvConvert32ARGBTo32RGBA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ARGB, 32RGBA)
}

void PrvConvert32ARGBTo32BGRA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ARGB, 32BGRA)
}

#pragma mark -

void PrvConvert32ABGRTo1 (const ScanlineParms& parms)
{
	STD_DIRECT_TO_1_CONVERT(32ABGR)
}

void PrvConvert32ABGRTo2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32ABGRTo4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32ABGRTo8 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32ABGRTo16RGB565 (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ABGR, 16RGB565 )
}

void PrvConvert32ABGRTo24RGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ABGR, 24RGB )
}

void PrvConvert32ABGRTo24BGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ABGR, 24BGR)
}

void PrvConvert32ABGRTo32ARGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ABGR, 32ARGB)
}

void PrvConvert32ABGRTo32ABGR (const ScanlineParms& parms)
{
	STD_NO_CONVERT(32)
}

void PrvConvert32ABGRTo32RGBA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ABGR, 32RGBA)
}

void PrvConvert32ABGRTo32BGRA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32ABGR, 32BGRA)
}

#pragma mark -

void PrvConvert32RGBATo1 (const ScanlineParms& parms)
{
	STD_DIRECT_TO_1_CONVERT(32RGBA)
}

void PrvConvert32RGBATo2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32RGBATo4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32RGBATo8 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32RGBATo16RGB565 (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32RGBA, 16RGB565)
}

void PrvConvert32RGBATo24RGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32RGBA, 24RGB)
}

void PrvConvert32RGBATo24BGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32RGBA, 24BGR)
}

void PrvConvert32RGBATo32ARGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32RGBA, 32ARGB)
}

void PrvConvert32RGBATo32ABGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32RGBA, 32ABGR)
}

void PrvConvert32RGBATo32RGBA (const ScanlineParms& parms)
{
	STD_NO_CONVERT(32)
}

void PrvConvert32RGBATo32BGRA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32RGBA, 32BGRA)
}

#pragma mark -

void PrvConvert32BGRATo1 (const ScanlineParms& parms)
{
	STD_DIRECT_TO_1_CONVERT(32BGRA)
}

void PrvConvert32BGRATo2 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32BGRATo4 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32BGRATo8 (const ScanlineParms& /*parms*/)
{
	EmAssert (false);
}

void PrvConvert32BGRATo16RGB565 (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32BGRA, 16RGB565)
}

void PrvConvert32BGRATo24RGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32BGRA, 24RGB)
}

void PrvConvert32BGRATo24BGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32BGRA, 24BGR)
}

void PrvConvert32BGRATo32ARGB (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32BGRA, 32ARGB)
}

void PrvConvert32BGRATo32ABGR (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32BGRA, 32ABGR)
}

void PrvConvert32BGRATo32RGBA (const ScanlineParms& parms)
{
	STD_DIRECT_CONVERT(32BGRA, 32RGBA)
}

void PrvConvert32BGRATo32BGRA (const ScanlineParms& parms)
{
	STD_NO_CONVERT(32)
}
