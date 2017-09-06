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
#include "EmRegsEZVisor.h"
#include "EmRegsVZVisorEdge.h"
#include "EmRegsVZPrv.h"


// Implementation of register-handling for Visor Edge.  Visor Edge is derived
// from Visor Platinum with the following differences:  
//  - key setup is identical to Visor Prism
//  - "LCD Enable On" pin is in a different location
//  - backlight pin is active high and in different place


/************************************************************************
 * Port B Bit settings
 ************************************************************************/
#define	hwrVisorEdgePortBLCDVccOff		0x08	// (L) LCD Vcc 
#define	hwrVisorEdgePortBLCDAdjOn		0x40	// (H) LCD Contrast Voltage


/************************************************************************
 * Port C Bit settings
 ************************************************************************/


/************************************************************************
 * Port D Bit settings
 ************************************************************************/
#define hwrVisorEdgePortDKbdCol0On		0x01 	// (H) Keyboard Column 0
#define hwrVisorEdgePortDKbdCol1On		0x02 	// (H) Keyboard Column 1
#define hwrVisorEdgePortDKbdCol2On		0x04 	// (H) Keyboard Column 2

#define hwrVisorEdgePortDExtPowerOn     0x08    // (H) External power indicator
									
#define hwrVisorEdgePortDDock1IrqOff	0x10 	// (L) Dock1 interrupt
#define hwrVisorEdgePortDSlotIrqOff		0x20 	// (L) Slot Interrupt
#define hwrVisorEdgePortDUsbIrqOff		0x40 	// (L) USB Interrupt
#define hwrVisorEdgePortDCardInstIrqOff	0x80 	// (L) Card Installed Interrupt

#define	hwrVisorEdgePortDKeyBits		0x07 	// (H) All Keyboard Columns


/************************************************************************
 * Port E Bit settings
 ************************************************************************/
#define	hwrVisorEdgePortESerialTxD		0x20	// ( ) Serial transmit line
#define hwrVisorEdgePortESlotResetOff	0x40 	// (L) Slot reset
#define hwrVisorEdgePortEDock2Off		0x80 	// (L) Dock2 input


/************************************************************************
 * Port F Bit settings
 ************************************************************************/
#define	hwrVisorEdgePortFContrast		0x01	// ( ) LCD Contrast PWM output
#define hwrVisorEdgePortFIREnableOff	0x04	// (L) Shutdown IR
#define hwrVisorEdgePortFUsbCsOff		0x80	// (L) USB chip select


/************************************************************************
 * Port G Bit settings
 ************************************************************************/
#define hwrVisorEdgePortGUnused0			0x01	// ( ) unused GPIO (after reset)
#define hwrVisorEdgePortGUsbSuspend			0x02	// (X) USB Suspend bit - Output low for active
													//                     - Input for suspend
#define	hwrVisorEdgePortGPowerFailIrqOff	0x04	// (L) Power Fail IRQ
#define hwrVisorEdgePortGKbdRow0 			0x08	// (H) Keyboard Row 0	
#define hwrVisorEdgePortGKbdRow1 			0x10	// (H) Keyboard Row 1	
#define hwrVisorEdgePortGAdcCsOff			0x20	// (L) A/D Select


/************************************************************************
 * Port J Bit settings
 ************************************************************************/
#define hwrVisorEdgePortJUnused0		0x01	// (X) unused GPIO
#define hwrVisorEdgePortJUnused1		0x02	// (X) unused GPIO
#define hwrVisorEdgePortJUnused2		0x04	// (X) unused GPIO
#define hwrVisorEdgePortJUnused3		0x08	// (X) unused GPIO
#define hwrVisorEdgePortJSerial2RxD		0x10	// ( ) Serial (UART2) recieve line
#define hwrVisorEdgePortJSerial2TxD		0x20	// ( ) Serial (UART2) transmit line
#define hwrVisorEdgePortJSerial2Rts		0x40	// ( ) Serial (UART2) RTS
#define hwrVisorEdgePortJSerial2Cts		0x80	// ( ) Serial (UART2) CTS


/************************************************************************
 * Port K Bit settings
 ************************************************************************/
#define hwrVisorEdgePortKUnused0        0x01    // (X) unused GPIO
#define hwrVisorEdgePortKKbdRow2        0x02    // (H) Keyboard Row 2
#define hwrVisorEdgePortKUnused2        0x04    // (X) unused GPIO
#define hwrVisorEdgePortKUnused3        0x08    // (X) unused GPIO

#define	hwrVisorEdgePortKLedOn			0x10	// (L) Turn on LED
#define hwrVisorEdgePortKLCDEnableOn	0x20	// (L) Shutdown IR
#define hwrVisorEdgePortKBacklightOn	0x40	// (H) Backlight on/off
#define	hwrVisorEdgePortKUsbResetOff	0x80	// (L) USB reset


/************************************************************************
 * Port M Bit settings
 ************************************************************************/

#define hwrVisorEdgePortMChargerOff			0x01    // (L) Assert (drive low) to enable 
													//     charger. Deassert for 1ms
													//     to restart charger
#define hwrVisorEdgePortMChargerOn			0x04    // (H) Assert (drive high) to enable 
													//     charger. Deassert for 1ms
													//     to restart charger


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorEdge::GetLCDScreenOn
// ---------------------------------------------------------------------------

Bool EmRegsVZVisorEdge::GetLCDScreenOn (void)
{
	return (READ_REGISTER (portKData) & hwrVisorEdgePortKLCDEnableOn) != 0;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorEdge::GetLCDBacklightOn
// ---------------------------------------------------------------------------

Bool EmRegsVZVisorEdge::GetLCDBacklightOn (void)
{
	return (READ_REGISTER (portKData) & hwrVisorEdgePortKBacklightOn) != 0;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsVZVisorEdge::GetKeyInfo
// ---------------------------------------------------------------------------

void EmRegsVZVisorEdge::GetKeyInfo (int* numRows, int* numCols,
									uint16* keyMap, Bool* rows)
{
	*numRows = kNumButtonRows;
	*numCols = kNumButtonCols;

	memcpy (keyMap, kVisorButtonMap, sizeof (kVisorButtonMap));

	// Determine what row is being asked for.

	UInt8	portKDir	= READ_REGISTER (portKDir);
	UInt8	portKData	= READ_REGISTER (portKData);

	UInt8	portGDir	= READ_REGISTER (portGDir);
	UInt8	portGData	= READ_REGISTER (portGData);

	rows[0]	= (portGDir & hwrVisorEdgePortGKbdRow0)   != 0 && (portGData & hwrVisorEdgePortGKbdRow0) == 0;
	rows[1]	= (portGDir & hwrVisorEdgePortGKbdRow1)   != 0 && (portGData & hwrVisorEdgePortGKbdRow1) == 0;
	rows[2]	= (portKDir & hwrVisorEdgePortKKbdRow2)   != 0 && (portKData & hwrVisorEdgePortKKbdRow2) == 0;
}
