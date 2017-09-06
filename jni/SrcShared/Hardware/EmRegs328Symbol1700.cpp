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
#include "EmRegs328Symbol1700.h"
#include "EmRegs328Prv.h"

#define keyBitTrigLeft		0x0400
#define keyBitTrigCenter	0x0800
#define keyBitTrigRight		0x1000
#define keyBitPageUpLeft	keyBitPageUp
#define keyBitPageUpRight	keyBitPageDown
#define keyBitPageDownLeft	0x2000
#define keyBitPageDownRight	0x4000


#define	keyBitMask			0x0F	// Which bits in port D we use for keys

#define portDKbdCols		0xF0
#define portDKbdRows		0x0F

#define portDKbdCol0		0x10
#define portDKbdCol1		0x20
#define portDKbdCol2		0x40
#define portDKbdCol3		0x80

#define portDKbdRow0		0x01
#define portDKbdRow1		0x02
#define portDKbdRow2		0x04
#define portDKbdRow3		0x08


const int		kNumButtonRows = 4;
const int		kNumButtonCols = 4;

const uint16	kButtonMap[kNumButtonRows][kNumButtonCols] =
{
	{ keyBitPower,		keyBitTrigLeft,		keyBitTrigCenter,	keyBitTrigRight		},
	{ keyBitHard1,		keyBitHard2,		keyBitHard3,		keyBitHard4			},
	{ keyBitPageUpLeft,	keyBitPageUpRight,	keyBitPageDownLeft,	keyBitPageDownRight	},
	{ keyBitContrast,	0,					0,					0					}
};


// ---------------------------------------------------------------------------
//		¥ EmRegs328Symbol1700::GetKeyBits
// ---------------------------------------------------------------------------

uint8 EmRegs328Symbol1700::GetKeyBits (void)
{
	uint8	portDData = READ_REGISTER (portDData);

	Bool	cols[4];
	cols[0]	= (portDData & portDKbdCol0) == 0;
	cols[1]	= (portDData & portDKbdCol1) == 0;
	cols[2]	= (portDData & portDKbdCol2) == 0;
	cols[3]	= (portDData & portDKbdCol3) == 0;

	uint8	keyData = 0;

	// Walk the columns, looking for one that is requested.

	for (int col = 0; col < kNumButtonCols; ++col)
	{
		if (cols[col])
		{
			// Walk the rows.

			for (int row = 0; row < kNumButtonRows; ++row)
			{
				// Get the key corresponding to this row and column.
				// If we've recorded (in fgKeyBits) that this key is
				// pressed, then set its row bit.

				if ((fKeyBits & kButtonMap[row][col]) != 0)
				{
					keyData |= 1 << (col + 4);
					keyData |= 1 << row;
				}
			}
		}
	}

	return keyData;
}


// ---------------------------------------------------------------------------
//		¥ EmRegs328Symbol1700::ButtonToBits
// ---------------------------------------------------------------------------

uint16 EmRegs328Symbol1700::ButtonToBits (SkinElementType button)
{
	uint16 bitNumber = 0;
	switch (button)
	{
		default:
			bitNumber = EmRegs328PalmPilot::ButtonToBits (button);
			break;

		case kElement_TriggerLeft:		bitNumber = keyBitTrigLeft;			break;
		case kElement_TriggerCenter:	bitNumber = keyBitTrigCenter;		break;
		case kElement_TriggerRight:		bitNumber = keyBitTrigRight;		break;
		case kElement_UpButtonLeft:		bitNumber = keyBitPageUpLeft;		break;
		case kElement_UpButtonRight:	bitNumber = keyBitPageUpRight;		break;
		case kElement_DownButtonLeft:	bitNumber = keyBitPageDownLeft;		break;
		case kElement_DownButtonRight:	bitNumber = keyBitPageDownRight;	break;
	}

	return bitNumber;
}
