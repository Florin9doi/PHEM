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
#include "EmRegsEZVisor.h"
#include "EmRegsEZPrv.h"

#include "EmDevice.h"			// HardwareID
#include "EmSession.h"			// GetDevice
#include "EmSPISlaveADS784x.h"	// EmSPISlaveADS784x


const uint16	kVisorButtonMap[kNumButtonRows][kNumButtonCols] =
{
	{ keyBitHard1,	keyBitHard2,	keyBitHard3 },
	{ keyBitPageUp,	keyBitPageDown,	0 },
	{ keyBitPower,	keyBitHard4,	0 }
};


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::EmRegsEZVisor
// ---------------------------------------------------------------------------

EmRegsEZVisor::EmRegsEZVisor (void) :
	EmRegsEZ (),
	fSPISlaveADC (new EmSPISlaveADS784x (kChannelSet2))
{
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::~EmRegsEZVisor
// ---------------------------------------------------------------------------

EmRegsEZVisor::~EmRegsEZVisor (void)
{
	delete fSPISlaveADC;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::GetLCDScreenOn
// ---------------------------------------------------------------------------

Bool EmRegsEZVisor::GetLCDScreenOn (void)
{
	return (READ_REGISTER (portCData) & hwrLegoPortCLCDEnableOn) != 0;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::GetLCDBacklightOn
// ---------------------------------------------------------------------------

Bool EmRegsEZVisor::GetLCDBacklightOn (void)
{
	return (READ_REGISTER (portGData) & hwrLegoPortGBacklightOff) == 0;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::GetLineDriverState
// ---------------------------------------------------------------------------
// Return whether or not the line drivers for the given object are open or
// closed.

Bool EmRegsEZVisor::GetLineDriverState (EmUARTDeviceType type)
{
	if (type == kUARTSerial)
	{
		uint16	uControl = READ_REGISTER (uControl);
		uint16	uMisc = READ_REGISTER (uMisc);

		return (uControl & hwrEZ328UControlUARTEnable) != 0 &&
				(uMisc & hwrEZ328UMiscIRDAEn) == 0;
	}

	if (type == kUARTIR)
	{
		uint16	uControl = READ_REGISTER (uControl);
		uint16	uMisc = READ_REGISTER (uMisc);

		return (uControl & hwrEZ328UControlUARTEnable) != 0 &&
				(uMisc & hwrEZ328UMiscIRDAEn) != 0;
	}

	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::GetPortInputValue
// ---------------------------------------------------------------------------
// Return the GPIO values for the pins on the port.  These values are used
// if the select pins are high.

uint8 EmRegsEZVisor::GetPortInputValue (int port)
{
	uint8	result = EmRegsEZ::GetPortInputValue (port);

	if (port == 'E')
	{
		/*
			Support the Visor model ID.  From Bob Petersen at Handspring:

			The Visor ROM only works with 1 device so far. But, the Visor does have
			model detect pins so that future ROMs can work on multiple platforms. The
			model input pins are connected to port E, bits 0-2 (E[0-2]). To read them,
			you have to drive port G bit 2 low and enable the pullups on port E.
		*/

#define modelOutPin		0x04		// EMUIRQ (IRQ7)
#define modelInPins     0x07        // SPI pins on Visor

		UInt8	portGData		= READ_REGISTER (portGData);
		UInt8	portGDir		= READ_REGISTER (portGDir);
		UInt8	portGSelect		= READ_REGISTER (portGSelect);

		if (((portGData & modelOutPin) == 0) &&
			((portGDir & modelOutPin) == modelOutPin) &&
			((portGSelect & modelOutPin) == modelOutPin))
		{
			EmAssert (gSession);

			EmDevice	device	= gSession->GetDevice ();
			result |= (~device.HardwareID ()) & modelInPins;  // <chg 24-Apr-2000 BP>  expects inverse output
		}
		else
		{
			// The pin for detecting "in cradle" is hwrLegoPortEDock2Off.  It should be low
			// if in the serial cradle.  If it is high, we assume USB.
			//			-- Bob Petersen

			result &= ~hwrLegoPortEDock2Off;
		}
	}
	else if (port == 'G')
	{
		// Make sure that hwrLegoPortGPowerFailIrqOff is set.  If it's clear,
		// the battery code will make the device go to sleep immediately.

		result |= hwrLegoPortGPowerFailIrqOff;
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::GetPortInternalValue
// ---------------------------------------------------------------------------
// Return the dedicated values for the pins on the port.  These values are
// used if the select pins are low.

uint8 EmRegsEZVisor::GetPortInternalValue (int port)
{
	uint8	result = EmRegsEZ::GetPortInternalValue (port);

	if (port == 'D')
	{
		// Always set the high bit, indicating that there's no memory module
		// installed.  From Bob Petersen at Handspring:
		//
		//	This is a probe into the module memory, to check for a replacement ROM on a
		//	card that is plugged in.  The card is mapped to 0x20000000 and the card's
		//	ROM is at 0x28000000.  If it finds the card header (FEEDBEEF) and the card
		//	is plugged in that has a non-zero reset vector, the chip selects are changed
		//	so B0 addresses 10c00000, and then we jump to the reset vector and boot off
		//	the card.
		//
		//	This part of the boot code should only be called if it is detected that a
		//	card is currently plugged in (bit 7 of port D) and we aren't currently
		//	booting from the card.

		result |= hwrLegoPortDCardInstIrqOff;

		// Ensure that bit hwrLegoPortDDock1IrqOff is set.  If it's clear, HotSync
		// will sync via the modem instead of the serial port.

		result |= hwrLegoPortDDock1IrqOff;
	}
	else if (port == 'G')
	{
		// Make sure that hwrLegoPortGPowerFailIrqOff is set.  If it's clear,
		// the battery code will make the device go to sleep immediately.

		result |= hwrLegoPortGPowerFailIrqOff;
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::GetKeyInfo
// ---------------------------------------------------------------------------

void EmRegsEZVisor::GetKeyInfo (int* numRows, int* numCols,
								uint16* keyMap, Bool* rows)
{
	*numRows = kNumButtonRows;
	*numCols = kNumButtonCols;

	memcpy (keyMap, kVisorButtonMap, sizeof (kVisorButtonMap));

	// Determine what row is being asked for.

	UInt8	portDDir	= READ_REGISTER (portDDir);
	UInt8	portDData	= READ_REGISTER (portDData);

	UInt8	portGDir	= READ_REGISTER (portGDir);
	UInt8	portGData	= READ_REGISTER (portGData);

	rows[0]	= (portGDir & hwrLegoPortGKbdRow0) != 0 && (portGData & hwrLegoPortGKbdRow0) == 0;
	rows[1]	= (portGDir & hwrLegoPortGKbdRow1) != 0 && (portGData & hwrLegoPortGKbdRow1) == 0;
	rows[2]	= (portDDir & hwrLegoPortDKbdRow2) != 0 && (portDData & hwrLegoPortDKbdRow2) == 0;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsEZVisor::GetSPISlave
// ---------------------------------------------------------------------------

EmSPISlave* EmRegsEZVisor::GetSPISlave (void)
{
	if ((READ_REGISTER (portGData) & hwrLegoPortGAdcCsOff) == 0)
	{
		return fSPISlaveADC;
	}

	return NULL;
}
