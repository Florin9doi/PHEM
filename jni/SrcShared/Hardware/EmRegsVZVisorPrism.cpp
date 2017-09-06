/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmRegsVZVisorPrism.h"
#include "EmRegsVZPrv.h"

#include "EmRegsSED1376.h"		// SED1376 video controller register access

#include "EmSession.h"			// GetDevice
#include "EmDevice.h"			// HardwareID


// Prism pin definitions from Handspring.  Mostly the same as a Palm V,
// but with some pins changed and moved around.


/************************************************************************
 * Port B Bit settings
 ************************************************************************/
#define hwrVisorPrismPortBLCDVccOff			0x08	// (L) LCD Vcc
#define hwrVisorPrismPortBLCDAdjOn			0x40	// (H) LCD Contrast Voltage


/************************************************************************
 * Port C Bit settings
 ************************************************************************/
#define hwrVisorPrismPortCLCDEnableOn		0x80 	// (H) LCD Enable


/************************************************************************
 * Port D Bit settings
 ************************************************************************/
#define hwrVisorPrismPortDKbdCol0On			0x01 	// (H) Keyboard Column 0
#define hwrVisorPrismPortDKbdCol1On			0x02 	// (H) Keyboard Column 1
#define hwrVisorPrismPortDKbdCol2On			0x04 	// (H) Keyboard Column 2

// Not here!  See hwrVisorPrismPortKKbdRow2
// #define hwrVisorPrismPortDKbdRow2			0x08 	// (H) Keyboard Row 2

#define hwrVisorPrismPortDDock1IrqOff		0x10 	// (L) Dock1 interrupt
#define hwrVisorPrismPortDSlotIrqOff		0x20 	// (L) Slot Interrupt
#define hwrVisorPrismPortDUsbIrqOff			0x40 	// (L) USB Interrupt
#define hwrVisorPrismPortDCardInstIrqOff	0x80 	// (L) Card Installed Interrupt

#define hwrVisorPrismPortDKeyBits			0x07 	// (H) All Keyboard Columns



/************************************************************************
 * Port E Bit settings
 ************************************************************************/
#define hwrVisorPrismPortESerialTxD			0x20	// ( ) Serial transmit line
#define hwrVisorPrismPortESlotResetOff		0x40 	// (L) Slot reset
#define hwrVisorPrismPortEDock2Off			0x80 	// (L) Dock2 input


/************************************************************************
 * Port F Bit settings
 ************************************************************************/
#define hwrVisorPrismPortFContrast			0x01	// ( ) LCD Contrast PWM output
#define hwrVisorPrismPortFIREnableOff		0x04	// (L) Shutdown IR
#define hwrVisorPrismPortFUsbCsOff			0x80	// (L) USB chip select


/************************************************************************
 * Port G Bit settings
 ************************************************************************/
#define hwrVisorPrismPortGBacklightOff		0x01	// (L) Backlight on/off
#define hwrVisorPrismPortGUSBSuspend		0x02	// (X) USB Suspend bit - Output low for active
													//                     - Input for suspend
#define hwrVisorPrismPortGPowerFailIrqOff	0x04	// (L) Power Fail IRQ
#define hwrVisorPrismPortGKbdRow0 			0x08	// (H) Keyboard Row 0
#define hwrVisorPrismPortGKbdRow1 			0x10	// (H) Keyboard Row 1
#define hwrVisorPrismPortGAdcCsOff			0x20	// (L) A/D Select


/************************************************************************
 * Port J Bit settings
 ************************************************************************/
#define	hwrVisorPrismPortJEepromDi			0x01	// (H) EEPROM Data in
#define	hwrVisorPrismPortJEepromDo			0x02	// ( ) EEPROM Data out
#define	hwrVisorPrismPortJEepromSkOn		0x04	// (H) EEPROM Clock
#define	hwrVisorPrismPortJEepromCsOn		0x08	// (H) EEPROM Chip Select
#define	hwrVisorPrismPortJUnused4			0x10	// (H) unused GPIO
#define	hwrVisorPrismPortJUnused5			0x20	// (H) unused GPIO
#define	hwrVisorPrismPortJUnused6			0x40	// (H) unused GPIO
#define	hwrVisorPrismPortJUnused7			0x80	// (H) unused GPIO


/************************************************************************
 * Port K Bit settings
 ************************************************************************/
#define hwrVisorPrismPortKBrightnessPWMOff  0x01    // (L) Brightness PWM output
#define hwrVisorPrismPortKKbdRow2           0x02    // (H) Keyboard Row 2
#define hwrVisorPrismPortKUnused2           0x04    // (X) unused GPIO
#define hwrVisorPrismPortKIREnableBOff      0x08    // (L) Shutdown IR on DVT and later

#define	hwrVisorPrismPortKLcdResetOff		0x10	// (L) LCD reset
#define hwrVisorPrismPortKIREnableAOff		0x20	// (L) Shutdown IR on pre-DVT
#define hwrVisorPrismPortKUnused6			0x40	// (H) unused GPIO
#define	hwrVisorPrismPortKUsbResetOff		0x80	// (L) USB reset


/************************************************************************
 * Port M Bit settings
 ************************************************************************/



/************************************************************************
 * Hard key settings
 ************************************************************************/

const int		kNumButtonRows = 3;
const int		kNumButtonCols = 3;

const uint16	kButtonMap[kNumButtonRows][kNumButtonCols] =
{
	{ keyBitHard1,	keyBitHard2,	keyBitHard3 },
	{ keyBitPageUp,	keyBitPageDown,	0 },
	{ keyBitPower,	keyBitHard4,	0 }
};



// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetLCDScreenOn
// ---------------------------------------------------------------------------

Bool EmRegsVZVisorPrism::GetLCDScreenOn (void)
{
	// Override the Dragonball version and let the SED 1376 handle it.

	return EmHALHandler::GetLCDScreenOn ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetLCDBacklightOn
// ---------------------------------------------------------------------------

Bool EmRegsVZVisorPrism::GetLCDBacklightOn (void)
{
	// Override the Dragonball version and let the SED 1376 handle it.

	return EmHALHandler::GetLCDBacklightOn ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetLCDHasFrame
// ---------------------------------------------------------------------------

Bool EmRegsVZVisorPrism::GetLCDHasFrame (void)
{
	// Override the Dragonball version and let the SED 1376 handle it.

	return EmHALHandler::GetLCDHasFrame ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetLCDBeginEnd
// ---------------------------------------------------------------------------

void EmRegsVZVisorPrism::GetLCDBeginEnd (emuptr& begin, emuptr& end)
{
	// Override the Dragonball version and let the SED 1376 handle it.

	EmHALHandler::GetLCDBeginEnd (begin, end);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetLCDScanlines
// ---------------------------------------------------------------------------

void EmRegsVZVisorPrism::GetLCDScanlines (EmScreenUpdateInfo& info)
{
	// Override the Dragonball version and let the SED 1376 handle it.

	EmHALHandler::GetLCDScanlines (info);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetLineDriverState
// ---------------------------------------------------------------------------
// Return whether or not the line drivers for the given object are open or
// closed.
//
//	DOLATER BP:  this way of detecting SerialPortOn may be wrong.  
//	See PalmIIIc or PalmV.  It may impact Visor emulation as well

Bool EmRegsVZVisorPrism::GetLineDriverState (EmUARTDeviceType type)
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
//		¥ EmRegsVZVisorPrism::GetUARTDevice
// ---------------------------------------------------------------------------
// Return what sort of device is hooked up to the given UART.

EmUARTDeviceType EmRegsVZVisorPrism::GetUARTDevice (int /*uartNum*/)
{
	Bool	serEnabled	= this->GetLineDriverState (kUARTSerial);
	Bool	irEnabled	= this->GetLineDriverState (kUARTIR);

	// It's probably an error to have them both enabled at the same
	// time.  !!! TBD: make this an error message.

	EmAssert (!(serEnabled && irEnabled));

	// !!! Which UART are they using?

//	if (uartNum == ???)
	{
		if (serEnabled)
			return kUARTSerial;

		if (irEnabled)
			return kUARTIR;
	}

	return kUARTNone;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetPortInputValue
//
//	Returns the GPIO value of the given port.
// ---------------------------------------------------------------------------

uint8 EmRegsVZVisorPrism::GetPortInputValue (int port)
{
	uint8	result = EmRegsVZ::GetPortInputValue (port);

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
			EmDevice	device	= gSession->GetDevice ();
			result |= (~device.HardwareID ()) & modelInPins;  // <chg 24-Apr-2000 BP>  expects inverse output
		}
		else
		{
			// The pin for detecting "in cradle" is hwrVisorPrismPortEDock2Off.  It should be low
			// if in the serial cradle.  If it is high, we assume USB.
			//			-- Bob Petersen

			result &= ~hwrVisorPrismPortEDock2Off;
		}
	}
	else if (port == 'G')
	{
		// Make sure that hwrVisorPrismPortGPowerFailIrqOff is set.  If it's clear,
		// the battery code will make the device go to sleep immediately.

		result |= hwrVisorPrismPortGPowerFailIrqOff;
	}

	else if (port == 'J')
	{
		// Make sure this bit is set, which means the EEPROM has finished being read. 
		// Otherwise we will loop indefinitely as the OS polls this port. 

		result |= hwrVisorPrismPortJEepromDo;
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetPortInternalValue
// 
//	Returns the "dedicated" pin value of the given port.
// ---------------------------------------------------------------------------

uint8 EmRegsVZVisorPrism::GetPortInternalValue (int port)
{
	uint8	result = EmRegsVZ::GetPortInternalValue (port);

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

		result |= hwrVisorPrismPortDCardInstIrqOff;

		// Ensure that bit hwrVisorPrismPortDDock1IrqOff is set.  If it's clear, HotSync
		// will sync via the modem instead of the serial port.

		result |= hwrVisorPrismPortDDock1IrqOff;
	}
	else if (port == 'G')
	{
		// Make sure that hwrVisorPrismPortGPowerFailIrqOff is set.  If it's clear,
		// the battery code will make the device go to sleep immediately.

		result |= hwrVisorPrismPortGPowerFailIrqOff;
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorPrism::GetKeyInfo
// ---------------------------------------------------------------------------

void EmRegsVZVisorPrism::GetKeyInfo (int* numRows, int* numCols,
								uint16* keyMap, Bool* rows)
{
	*numRows = kNumButtonRows;
	*numCols = kNumButtonCols;

	memcpy (keyMap, kButtonMap, sizeof (kButtonMap));

	// Determine what row is being asked for.

	UInt8	portKDirVZ	= READ_REGISTER (portKDir);
	UInt8	portKDataVZ	= READ_REGISTER (portKData);

	UInt8	portGDir	= READ_REGISTER (portGDir);
	UInt8	portGData	= READ_REGISTER (portGData);

	rows[0]	= (portGDir & hwrVisorPrismPortGKbdRow0)   != 0 && (portGData & hwrVisorPrismPortGKbdRow0) == 0;
	rows[1]	= (portGDir & hwrVisorPrismPortGKbdRow1)   != 0 && (portGData & hwrVisorPrismPortGKbdRow1) == 0;
	rows[2]	= (portKDirVZ & hwrVisorPrismPortKKbdRow2) != 0 && (portKDataVZ & hwrVisorPrismPortKKbdRow2) == 0;
}

