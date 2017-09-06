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

#ifndef EmRegsEZVisor_h
#define EmRegsEZVisor_h

#include "EmRegsEZ.h"

// Handspring modelID defines
// DOLATER BP:  move the general Handspring-specific stuff to another include file
#define   halModelChipIDEZ              0x00		  // 0000 0
#define   halModelChipIDVZ              0x08		  // 0000 1
//#define   halModelChipIDXZ            0x10		  // 0001 0  placeholder

#define	  halModelEZIndexLego			0x00		  //       000

#define	  halModelVZIndexVisorPlatinum	0x00      	  //       000
#define   halModelVZIndexVisorPrism		0x02		  //       010
#define   halModelVZIndexVisorEdge		0x01		  //       001


// Model IDs (combinations of the above)...
// ----------------------------------------
#define   halModelIDLego		  (halModelChipIDEZ | halModelEZIndexLego) // Lego (Visor)
#define   halModelIDVisorPlatinum (halModelChipIDVZ | halModelVZIndexVisorPlatinum)
#define   halModelIDVisorPrism	  (halModelChipIDVZ | halModelVZIndexVisorPrism)
#define   halModelIDVisorEdge	  (halModelChipIDVZ | halModelVZIndexVisorEdge)


// Visor pin definitions from Handspring.  Mostly the same as a Palm V,
// but with some pins changed and moved around.


/************************************************************************
 * Port B Bit settings
 ************************************************************************/
#define hwrLegoPortBLCDVccOff		0x08	// (L) LCD Vcc
#define hwrLegoPortBLCDAdjOn		0x40	// (H) LCD Contrast Voltage


/************************************************************************
 * Port C Bit settings
 ************************************************************************/
#define hwrLegoPortCLCDEnableOn		0x80 	// (H) LCD Enable


/************************************************************************
 * Port D Bit settings
 ************************************************************************/
#define hwrLegoPortDKbdCol0On		0x01 	// (H) Keyboard Column 0
#define hwrLegoPortDKbdCol1On		0x02 	// (H) Keyboard Column 1
#define hwrLegoPortDKbdCol2On		0x04 	// (H) Keyboard Column 2

#define hwrLegoPortDKbdRow2			0x08 	// (H) Keyboard Row 2

#define hwrLegoPortDDock1IrqOff		0x10 	// (L) Dock1 interrupt
#define hwrLegoPortDSlotIrqOff		0x20 	// (L) Slot Interrupt
#define hwrLegoPortDUsbIrqOff		0x40 	// (L) USB Interrupt
#define hwrLegoPortDCardInstIrqOff	0x80 	// (L) Card Installed Interrupt

#define hwrLegoPortDKeyBits			0x07 	// (H) All Keyboard Columns



/************************************************************************
 * Port E Bit settings
 ************************************************************************/
#define hwrLegoPortESerialTxD		0x20	// ( ) Serial transmit line
#define hwrLegoPortESlotResetOff	0x40 	// (L) Slot reset
#define hwrLegoPortEDock2Off		0x80 	// (L) Dock2 input


/************************************************************************
 * Port F Bit settings
 ************************************************************************/
#define hwrLegoPortFContrast		0x01	// ( ) LCD Contrast PWM output
#define hwrLegoPortFIREnableOff		0x04	// (L) Shutdown IR
#define hwrLegoPortFUsbCsOff		0x80	// (L) USB chip select


/************************************************************************
 * Port G Bit settings
 ************************************************************************/
#define hwrLegoPortGBacklightOff	0x01	// (L) Backlight on/off
#define hwrLegoPortGUSBSuspend		0x02	// (X) USB Suspend bit - Output low for active
											//                     - Input for suspend
#define hwrLegoPortGPowerFailIrqOff	0x04	// (L) Power Fail IRQ
#define hwrLegoPortGKbdRow0 		0x08	// (H) Keyboard Row 0
#define hwrLegoPortGKbdRow1 		0x10	// (H) Keyboard Row 1
#define hwrLegoPortGAdcCsOff		0x20	// (L) A/D Select


const int		kNumButtonRows = 3;
const int		kNumButtonCols = 3;

extern const uint16	kVisorButtonMap[kNumButtonRows][kNumButtonCols];


class EmRegsEZVisor : public EmRegsEZ
{
	public:
								EmRegsEZVisor			(void);
		virtual					~EmRegsEZVisor			(void);

		virtual Bool			GetLCDScreenOn			(void);
		virtual Bool			GetLCDBacklightOn		(void);
		virtual Bool			GetLineDriverState		(EmUARTDeviceType type);

		virtual uint8			GetPortInputValue		(int);
		virtual uint8			GetPortInternalValue	(int);
		virtual void			GetKeyInfo				(int* numRows, int* numCols,
														 uint16* keyMap, Bool* rows);

	protected:
		virtual EmSPISlave*		GetSPISlave				(void);

	private:
		EmSPISlave*				fSPISlaveADC;
};

#endif	/* EmRegsEZVisor_h */
