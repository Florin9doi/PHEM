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

// This file provides emulation at the register level for TRG's
// TRGpro handheld computer, which uses the EZ processor


#include "EmCommon.h"
#include "EmRegsEZTRGpro.h"
#include "EmRegsEZPrv.h"

#include "EmSPISlaveADS784x.h"	// EmSPISlaveADS784x

#include "PalmPack.h"
#define NON_PORTABLE
	#include "EZSumo/IncsPrv/HardwareEZ.h"			// hwrEZPortCLCDEnableOn, etc.
#undef NON_PORTABLE
#include "PalmPackPop.h"

/* Update for recent GCC */
#include <cstddef>

// TRGpro Defines
#define hwrEZPortDCFInit          0x40
#define hwrEZPortGDO_LATCH        0x08  // SPI DO_LATCH enable
#define hwrEZPortBLCDVccOff       0x02  // LCD VCC off
#define hwrEZPortD232EnableTRGpro 0x20  // RS 232 enable

const int		kNumButtonRows = 3;
const int		kNumButtonCols = 4;

const uint16	kButtonMap[kNumButtonRows][kNumButtonCols] =
{
	{ keyBitHard1,	keyBitHard2,	keyBitHard3,	keyBitHard4 },
	{ keyBitPageUp,	keyBitPageDown,	0,				0 },
	{ keyBitPower,	keyBitContrast,	keyBitHard2,	0 }
};

// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::EmRegsEZTRGpro
// ---------------------------------------------------------------------------

EmRegsEZTRGpro::EmRegsEZTRGpro (CFBusManager ** fBusMgr) :
	EmRegsEZ (),
	fSPISlaveADC (new EmSPISlaveADS784x (kChannelSet2))
{
	*fBusMgr = &CFBus;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::~EmRegsEZTRGpro
// ---------------------------------------------------------------------------

EmRegsEZTRGpro::~EmRegsEZTRGpro (void)
{
	delete fSPISlaveADC;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::Initialize
// ---------------------------------------------------------------------------

void EmRegsEZTRGpro::Initialize(void)
{
	bBacklightOn	= false;
	CFBus.bEnabled	= false;
	CFBus.Width		= kCFBusWidth16;
	CFBus.bSwapped	= false;
	EmRegsEZ::Initialize();
}

// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::GetLCDScreenOn
// ---------------------------------------------------------------------------

Bool EmRegsEZTRGpro::GetLCDScreenOn (void)
{
	return (READ_REGISTER (portCData) & hwrEZPortCLCDEnableOn) != 0;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::GetLCDBacklightOn
// ---------------------------------------------------------------------------

Bool EmRegsEZTRGpro::GetLCDBacklightOn (void)
{
	return (bBacklightOn);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::GetLineDriverState
// ---------------------------------------------------------------------------
// Return whether or not the line drivers for the given object are open or
// closed.

Bool EmRegsEZTRGpro::GetLineDriverState (EmUARTDeviceType type)
{
	if (type == kUARTSerial)
		return (READ_REGISTER (portDData) & hwrEZPortD232EnableTRGpro) != 0;

	if (type == kUARTIR)
		return (READ_REGISTER (portGData) & 0x10) == 0;

	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::GetPortInputValue
// ---------------------------------------------------------------------------
// Return the GPIO values for the pins on the port.  These values are used
// if the select pins are high.

uint8 EmRegsEZTRGpro::GetPortInputValue (int port)
{
	uint8	result = EmRegsEZ::GetPortInputValue (port);

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::GetPortInternalValue
// ---------------------------------------------------------------------------
// Return the dedicated values for the pins on the port.  These values are
// used if the select pins are low.

uint8 EmRegsEZTRGpro::GetPortInternalValue (int port)
{
	uint8	result = EmRegsEZ::GetPortInternalValue (port);

	if (port == 'D')
	{
		// Make sure that hwrEZPortDPowerFail is set.  If it's clear,
		// the battery code will make the device go to sleep immediately.

		result |= (hwrEZPortDCFInit | hwrEZPortDPowerFail);
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::GetKeyInfo
// ---------------------------------------------------------------------------

void EmRegsEZTRGpro::GetKeyInfo (int* numRows, int* numCols,
								uint16* keyMap, Bool* rows)
{
//      KeyRowsType keyRows;

        *numRows = kNumButtonRows;
        *numCols = kNumButtonCols;

        memcpy (keyMap, kButtonMap, sizeof (kButtonMap));

        // Determine what row is being asked for.
        rows[0] = Keys.Row[0];
        rows[1] = Keys.Row[1];
        rows[2] = Keys.Row[2];
}

void EmRegsEZTRGpro::LatchSpi(void)
{
    uint16 latched = spiUnlatchedVal;

    bBacklightOn  = ((latched & SPIBacklightOn) != 0);
    CFBus.bEnabled  = ((latched & (SPICardBufferOff |
                                 SPICardSlotPwrOff)) == 0);
    CFBus.bSwapped  = ((latched & SPIBusSwapOff) == 0);
    if (latched & SPIBusWidth8)
        CFBus.Width = kCFBusWidth8;
    else
        CFBus.Width = kCFBusWidth16;
    Keys.Row[0] = !(latched & SPIKeyRow0);
    Keys.Row[1] = !(latched & SPIKeyRow1);
    Keys.Row[2] = !(latched & SPIKeyRow2);
}

// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::GetKeyInfo
// ---------------------------------------------------------------------------

void EmRegsEZTRGpro::PortDataChanged (int port, uint8 oldValue, uint8 newValue)
{
	if (port == 'G')
	{
		// If the the DO_LATCH bit is set, then the recent value
		// written to the SPI controller is for us, and not
		// the touchscreen

		if (newValue & hwrEZPortGDO_LATCH)
            LatchSpi();
	}

	EmRegsEZ::PortDataChanged(port, oldValue, newValue);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::spiWrite
//
// We need to share SPI writes with the touchscreen ...
// ---------------------------------------------------------------------------

void EmRegsEZTRGpro::spiWrite(emuptr address, int size, uint32 value)
{
    spiUnlatchedVal = value;
	EmRegsEZ::spiMasterControlWrite (address, size, value);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::SetSubBankHandlers
// ---------------------------------------------------------------------------

void EmRegsEZTRGpro::SetSubBankHandlers(void)
{
	EmRegsEZ::SetSubBankHandlers();
	this->SetHandler((ReadFunction)&EmRegsEZTRGpro::StdRead,
                         (WriteFunction)&EmRegsEZTRGpro::spiWrite,
	                 addressof(spiMasterData),
	                 sizeof(f68EZ328Regs.spiMasterData));
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZTRGpro::GetSPISlave
// ---------------------------------------------------------------------------

EmSPISlave* EmRegsEZTRGpro::GetSPISlave (void)
{
	if ((READ_REGISTER (portGData) & hwrEZPortGADCOff) == 0)
	{
		return fSPISlaveADC;
	}

	return NULL;
}
