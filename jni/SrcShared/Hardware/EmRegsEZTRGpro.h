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

#ifndef EmRegsEZTRGpro_h
#define EmRegsEZTRGpro_h

#include "EmRegsEZ.h"
#include "EmHandEraCFBus.h"

// --------------------------------------------------------------------------
// This file is used to emulate the SPI controller ... the SPI performs
// double duty for the TRGpro ... normally, it controls the touch screen ...
// however, if a latch bit is set on Port D, it will use the SPI to
// control the keypad and bus-state
// --------------------------------------------------------------------------

#define SPIKeyRow0              0x01
#define SPIKeyRow1              0x02
#define SPIKeyRow2              0x04
#define SPIBusSwapOff           0x08
#define SPIBacklightOn          0x10
#define SPICardBufferOff        0x20
#define SPICardSlotPwrOff       0x40
#define SPIBusWidth8            0x80

#define TRGPRO_NUM_KEY_ROWS 3

typedef struct {
	uint16 Row[TRGPRO_NUM_KEY_ROWS];
} TrgProKeys;


class EmRegsEZTRGpro : public EmRegsEZ
{
	public:
								EmRegsEZTRGpro			(CFBusManager ** fBusManager);
		virtual					~EmRegsEZTRGpro			(void);

		virtual void			Initialize				(void);
		virtual Bool			GetLCDScreenOn			(void);
		virtual Bool			GetLCDBacklightOn		(void);
		virtual Bool			GetLineDriverState		(EmUARTDeviceType type);

		virtual uint8			GetPortInputValue		(int);
		virtual uint8			GetPortInternalValue	(int);
		virtual void			GetKeyInfo				(int* numRows, int* numCols,
														 uint16* keyMap, Bool* rows);
		virtual void			PortDataChanged			(int, uint8, uint8);
		void 				SetSubBankHandlers(void);
	private:
		void				spiWrite(emuptr address, int size, uint32 value);
		uint16				spiLatchedVal, spiUnlatchedVal;
        void				LatchSpi(void);
		

	protected:
		virtual EmSPISlave*		GetSPISlave				(void);

	private:
		EmSPISlave*				fSPISlaveADC;
        CFBusManager            CFBus;
        TrgProKeys              Keys;                          
        int                     bBacklightOn;
};

#endif	/* EmRegsEZTRGpro_h */
