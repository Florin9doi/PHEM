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

#ifndef EmRegsVZVisorPrism_h
#define EmRegsVZVisorPrism_h

#include "EmRegsVZ.h"


class EmRegsVZVisorPrism : public EmRegsVZ
{
	public:
		virtual Bool			GetLCDScreenOn			(void);
		virtual Bool			GetLCDBacklightOn		(void);
		virtual Bool			GetLCDHasFrame			(void);
		virtual void			GetLCDBeginEnd			(emuptr& begin, emuptr& end);
		virtual void			GetLCDScanlines			(EmScreenUpdateInfo& info);
		virtual Bool			GetLineDriverState		(EmUARTDeviceType type);
		virtual EmUARTDeviceType	GetUARTDevice		(int uartNum);

		virtual uint8			GetPortInputValue		(int);
		virtual uint8			GetPortInternalValue	(int);
		virtual void			GetKeyInfo				(int* numRows, int* numCols,
														 uint16* keyMap, Bool* rows);
};

#endif	/* EmRegsVZVisorPrism_h */
