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
#include "EmRegsSED1375.h"

#include "Byteswapping.h"		// Canonical
#include "EmMemory.h"			// EmMem_memcpy
#include "EmPixMap.h"			// SetSize, SetRowBytes, etc.
#include "EmScreen.h"			// EmScreen::InvalidateAll
#include "Miscellaneous.h"		// StWordSwapper
#include "SessionFile.h"		// WriteSED1375RegsType


// Given a register (specified by its field name), return its address
// in emulated space.

#define addressof(reg)				\
	(this->GetAddressStart () + fRegs.offsetof_##reg ())


// Macro to help the installation of handlers for a register.

#define INSTALL_HANDLER(read, write, reg)			\
	this->SetHandler (	(ReadFunction) &EmRegsSED1375::read,		\
						(WriteFunction) &EmRegsSED1375::write,		\
						addressof (reg),			\
						fRegs.reg.GetSize ())


#define kCLUTColorIndexMask 	0xf000
#define kCLUTColorsMask 		0x0fff

#define kCLUTRedMask			0x0f00
#define kCLUTGreenMask			0x00f0
#define kCLUTBlueMask			0x000f

#define kCLUTIndexRed			0x4000
#define kCLUTIndexGreen 		0x2000
#define kCLUTIndexBlue			0x1000



// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::EmRegsSED1375
// ---------------------------------------------------------------------------

EmRegsSED1375::EmRegsSED1375 (emuptr baseRegsAddr, emuptr baseVideoAddr) :
	fBaseRegsAddr (baseRegsAddr),
	fBaseVideoAddr (baseVideoAddr),
	fRegs ()
{
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::~EmRegsSED1375
// ---------------------------------------------------------------------------

EmRegsSED1375::~EmRegsSED1375 (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::Initialize
// ---------------------------------------------------------------------------

void EmRegsSED1375::Initialize (void)
{
	EmRegs::Initialize ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::Reset
// ---------------------------------------------------------------------------

void EmRegsSED1375::Reset (Bool hardwareReset)
{
	EmRegs::Reset (hardwareReset);

	if (hardwareReset)
	{
		memset (fRegs.GetPtr (), 0, fRegs.GetSize ());

//		EmAssert ((sed1375ProductCodeExpected | sed1375RevisionCodeExpected) == 0x24);
		fRegs.productRevisionCode = 0x24;
	}
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::Save
// ---------------------------------------------------------------------------

void EmRegsSED1375::Save (SessionFile& f)
{
	EmRegs::Save (f);

	f.WriteSED1375RegsType (*(SED1375RegsType*) fRegs.GetPtr ());
	f.FixBug (SessionFile::kBugByteswappedStructs);

	StWordSwapper	swapper1 (fClutData, sizeof (fClutData));
	f.WriteSED1375Palette (fClutData);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::Load
// ---------------------------------------------------------------------------

void EmRegsSED1375::Load (SessionFile& f)
{
	EmRegs::Load (f);

	// Read in the SED registers.

	if (f.ReadSED1375RegsType (*(SED1375RegsType*) fRegs.GetPtr ()))
	{
		// The Windows version of Poser 2.1d29 and earlier did not write
		// out structs in the correct format.  The fields of the struct
		// were written out in Little-Endian format, not Big-Endian.  To
		// address this problem, the bug has been fixed, and a new field
		// is added to the file format indicating that the bug has been
		// fixed.  With the new field (the "bug bit"), Poser can identify
		// old files from new files and read them in accordingly.
		// 
		// With the bug fixed, the .psf files should now be interchangeable
		// across platforms (modulo other bugs...).

		if (!f.IncludesBugFix (SessionFile::kBugByteswappedStructs))
		{
			Canonical (*(SED1375RegsType*) fRegs.GetPtr ());
		}
	}
	else
	{
		f.SetCanReload (false);
	}

	// Read in the LCD palette, and then byteswap it.

	if (f.ReadSED1375Palette (fClutData))
	{
		::ByteswapWords (fClutData, sizeof (fClutData));
	}
	else
	{
		f.SetCanReload (false);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::Dispose
// ---------------------------------------------------------------------------

void EmRegsSED1375::Dispose (void)
{
	EmRegs::Dispose ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::SetSubBankHandlers
// ---------------------------------------------------------------------------

void EmRegsSED1375::SetSubBankHandlers (void)
{
	// Install base handlers.

	EmRegs::SetSubBankHandlers ();

	// Now add standard/specialized handers for the defined registers.

	INSTALL_HANDLER (StdReadBE,				NullWrite,					productRevisionCode);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					mode0);
	INSTALL_HANDLER (StdReadBE,				invalidateWrite,			mode1);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					mode2);
	INSTALL_HANDLER (StdReadBE,				invalidateWrite,			horizontalPanelSize);
	INSTALL_HANDLER (StdReadBE,				invalidateWrite,			verticalPanelSizeLSB);
	INSTALL_HANDLER (StdReadBE,				invalidateWrite,			verticalPanelSizeMSB);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					FPLineStartPosition);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					horizontalNonDisplayPeriod);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					FPFRAMEStartPosition);
	INSTALL_HANDLER (vertNonDisplayRead,	StdWriteBE,					verticalNonDisplayPeriod);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					MODRate);
	INSTALL_HANDLER (StdReadBE,				invalidateWrite,			screen1StartAddressLSB);
	INSTALL_HANDLER (StdReadBE,				invalidateWrite,			screen1StartAddressMSB);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					screen2StartAddressLSB);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					screen2StartAddressMSB);
	INSTALL_HANDLER (StdReadBE,				invalidateWrite,			screen1StartAddressMSBit);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					memoryAddressOffset);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					screen1VerticalSizeLSB);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					screen1VerticalSizeMSB);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					unused1);
	INSTALL_HANDLER (StdReadBE,				lookUpTableAddressWrite,	lookUpTableAddress);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					unused2);
	INSTALL_HANDLER (lookUpTableDataRead,	lookUpTableDataWrite,		lookUpTableData);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					GPIOConfigurationControl);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					GPIOStatusControl);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					scratchPad);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					portraitMode);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					lineByteCountRegister);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					unused3);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					unused4);
	INSTALL_HANDLER (StdReadBE,				StdWriteBE,					unused5);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::GetRealAddress
// ---------------------------------------------------------------------------

uint8* EmRegsSED1375::GetRealAddress (emuptr address)
{
	return (uint8*) fRegs.GetPtr () + address - this->GetAddressStart ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::GetAddressStart
// ---------------------------------------------------------------------------

emuptr EmRegsSED1375::GetAddressStart (void)
{
	return fBaseRegsAddr;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::GetAddressRange
// ---------------------------------------------------------------------------

uint32 EmRegsSED1375::GetAddressRange (void)
{
	return fRegs.GetSize ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::GetLCDScreenOn
// ---------------------------------------------------------------------------

Bool EmRegsSED1375::GetLCDScreenOn (void)
{
	return ((fRegs.mode1) & sed1375DisplayBlank) == 0;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::GetLCDBacklightOn
// ---------------------------------------------------------------------------

Bool EmRegsSED1375::GetLCDBacklightOn (void)
{
	return true;			// The Backlight is always on for these units.
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::GetLCDHasFrame
// ---------------------------------------------------------------------------

Bool EmRegsSED1375::GetLCDHasFrame (void)
{
	return true;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::GetLCDBeginEnd
// ---------------------------------------------------------------------------

void EmRegsSED1375::GetLCDBeginEnd (emuptr& begin, emuptr& end)
{
	// Get the screen metrics.

	int32	bpp			= 1 << ((fRegs.mode1 & sed1375BPPMask) >> sed1375BPPShift);
	int32	height		= ((fRegs.verticalPanelSizeMSB << 8) | fRegs.verticalPanelSizeLSB) + 1;
	int32	rowBytes	= (fRegs.horizontalPanelSize + 1) * bpp;
	uint32	offset		= (fRegs.screen1StartAddressMSBit	<< 17) |
						  (fRegs.screen1StartAddressMSB		<<  9) |
						  (fRegs.screen1StartAddressLSB		<<  1);
	emuptr	baseAddr	= fBaseVideoAddr + offset;

	begin = baseAddr;
	end = baseAddr + rowBytes * height;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::GetLCDScanlines
// ---------------------------------------------------------------------------

void EmRegsSED1375::GetLCDScanlines (EmScreenUpdateInfo& info)
{
	// Get the screen metrics.

	int32	bpp			= 1 << ((fRegs.mode1 & sed1375BPPMask) >> sed1375BPPShift);
	int32	width		= (fRegs.horizontalPanelSize + 1) * 8;
	int32	height		= ((fRegs.verticalPanelSizeMSB << 8) | fRegs.verticalPanelSizeLSB) + 1;
	int32	rowBytes	= (fRegs.horizontalPanelSize + 1) * bpp;
	uint32	offset		= (fRegs.screen1StartAddressMSBit	<< 17) |
						  (fRegs.screen1StartAddressMSB		<<  9) |
						  (fRegs.screen1StartAddressLSB		<<  1);
	emuptr	baseAddr	= fBaseVideoAddr + offset;

	info.fLeftMargin	= 0;

	EmPixMapFormat	format	=	bpp == 1 ? kPixMapFormat1 :
								bpp == 2 ? kPixMapFormat2 :
								bpp == 4 ? kPixMapFormat4 :
								kPixMapFormat8;

	RGBList	colorTable;
	this->PrvGetPalette (colorTable);

	// Set format, size, and color table of EmPixMap.

	info.fImage.SetSize			(EmPoint (width, height));
	info.fImage.SetFormat		(format);
	info.fImage.SetRowBytes		(rowBytes);
	info.fImage.SetColorTable	(colorTable);

	// Determine first and last scanlines to fetch, and fetch them.

	info.fFirstLine		= (info.fScreenLow - baseAddr) / rowBytes;
	info.fLastLine		= (info.fScreenHigh - baseAddr - 1) / rowBytes + 1;

	long	firstLineOffset	= info.fFirstLine * rowBytes;
	long	lastLineOffset	= info.fLastLine * rowBytes;

	EmMem_memcpy (
		(void*) ((uint8*) info.fImage.GetBits () + firstLineOffset),
		baseAddr + firstLineOffset,
		lastLineOffset - firstLineOffset);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::invalidateWrite
// ---------------------------------------------------------------------------

void EmRegsSED1375::invalidateWrite (emuptr address, int size, uint32 value)
{
	this->StdWriteBE (address, size, value);
	EmScreen::InvalidateAll ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::vertNonDisplayRead
// ---------------------------------------------------------------------------

uint32 EmRegsSED1375::vertNonDisplayRead (emuptr address, int size)
{
	UNUSED_PARAM(address)
	UNUSED_PARAM(size)

	// Always set the vertical non-display status high since in the real
	// hardware, the ROM will check this flag in order to write the CLUT
	// registers.

	return (fRegs.verticalNonDisplayPeriod) | sed1375VerticalNonDisplayStatus;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::lookUpTableAddressWrite
// ---------------------------------------------------------------------------

void EmRegsSED1375::lookUpTableAddressWrite (emuptr address, int size, uint32 value)
{
	this->StdWriteBE (address, size, value);

	value &= 0x0FF;

	fClutData[value] &= kCLUTColorsMask;				// Update the rgb index
	fClutData[value] |= kCLUTIndexRed;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::lookUpTableDataRead
// ---------------------------------------------------------------------------

uint32 EmRegsSED1375::lookUpTableDataRead (emuptr address, int size)
{
	EmAssert (size == 1);

	if (size != 1)
		return 0;			// Error case.

	uint8	clutIndex = (fRegs.lookUpTableAddress); 	// Get the LUT Addr.
	uint16	clutEntry = fClutData[clutIndex];			// Get the entry.
	uint8	colorData;

	if ((clutEntry & kCLUTIndexRed) != 0)
	{
		colorData = (uint8) ((clutEntry & kCLUTRedMask) >> 4); 	// Get the 4 bits of red.

		fClutData[clutIndex] =									// Update the next rgb index
			(fClutData[clutIndex] & kCLUTColorsMask) | kCLUTIndexGreen;
	}
	else if ((clutEntry & kCLUTIndexGreen) != 0)
	{
		colorData = (uint8) (clutEntry & kCLUTGreenMask);		// Get the 4 bits of green

		fClutData[clutIndex] =									// Update the next rgb index
			(fClutData[clutIndex] & kCLUTColorsMask) | kCLUTIndexBlue;
	}
	else
	{
		colorData = (uint8) ((clutEntry & kCLUTBlueMask) << 4);	// Get the 4 bits of blue.

		address = (emuptr) (addressof (lookUpTableAddress));
		EmRegsSED1375::lookUpTableAddressWrite (address, 1, (clutIndex + 1) & 0xFF);
	}

	return colorData;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::lookUpTableDataWrite
// ---------------------------------------------------------------------------

void EmRegsSED1375::lookUpTableDataWrite (emuptr address, int size, uint32 value)
{
	EmAssert (size == 1);

	if (size != 1)
		return; 		// Error case.

	uint8	clutIndex = (fRegs.lookUpTableAddress); 	// Get the LUT Addr.
	uint16	clutEntry = fClutData[clutIndex];			// Get the entry.

	uint8	newColor = (uint8) (value & 0x00F0);

	if (clutEntry & kCLUTIndexRed)
	{
		fClutData[clutIndex] &= ~kCLUTRedMask;			// Clear out old red bits.
		fClutData[clutIndex] |= newColor << 4;			// Save in new red bits.

		fClutData[clutIndex] =							// Update the rgb index
			(fClutData[clutIndex] & kCLUTColorsMask) | kCLUTIndexGreen;
	}
	else if (clutEntry & kCLUTIndexGreen)
	{
		fClutData[clutIndex] &= ~kCLUTGreenMask;		// Clear out old red bits.
		fClutData[clutIndex] |= newColor;				// Save in new green bits.

		fClutData[clutIndex] =							// Update the rgb index
			(fClutData[clutIndex] & kCLUTColorsMask) | kCLUTIndexBlue;
	}
	else
	{
		fClutData[clutIndex] &= ~kCLUTBlueMask; 		// Clear out old red bits.
		fClutData[clutIndex] |= newColor >> 4;			// Save in new blue bits.

		address = (emuptr) (addressof (lookUpTableAddress));
		EmRegsSED1375::lookUpTableAddressWrite (address, 1, (clutIndex + 1) & 0xFF);
	}

	EmScreen::InvalidateAll ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsSED1375::PrvGetPalette
// ---------------------------------------------------------------------------

void EmRegsSED1375::PrvGetPalette (RGBList& thePalette)
{
	int32	bpp			= 1 << ((fRegs.mode1 & sed1375BPPMask) >> sed1375BPPShift);
	int32 	numColors	= 1 << bpp;

	thePalette.resize (numColors);

	for (int ii = 0; ii < numColors; ++ii)
	{
		uint16	curEntry = fClutData[ii];
		uint8	color;

		color = (uint8) ((curEntry & kCLUTRedMask) >> 4);
		thePalette[ii].fRed = color + (color >> 4);

		color = (uint8) ((curEntry & kCLUTGreenMask) >> 0);
		thePalette[ii].fGreen = color + (color >> 4);

		color = (uint8) ((curEntry & kCLUTBlueMask) << 4);
		thePalette[ii].fBlue = color + (color >> 4);
	}
}
