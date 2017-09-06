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

#ifndef EmRegs328Symbol1700_h
#define EmRegs328Symbol1700_h

#include "EmRegs328PalmPilot.h"			// EmRegs328PalmPilot

class EmRegs328Symbol1700 : public EmRegs328PalmPilot
{
	public:
		virtual uint8			GetKeyBits				(void);
		virtual uint16			ButtonToBits			(SkinElementType);
};

#endif	/* EmRegs328Symbol1700_h */
