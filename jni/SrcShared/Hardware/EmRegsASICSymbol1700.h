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

#ifndef EmRegsASICSymbol1700_h
#define EmRegsASICSymbol1700_h

#include "EmPalmStructs.h"
#include "EmRegs.h"

class SessionFile;


class EmRegsASICSymbol1700 : public EmRegs
{
	public:
								EmRegsASICSymbol1700	(void);
		virtual					~EmRegsASICSymbol1700	(void);

		virtual void			Initialize			(void);
		virtual void			Reset				(Bool hardwareReset);
		virtual void			Save				(SessionFile&);
		virtual void			Load				(SessionFile&);
		virtual void			Dispose				(void);

		virtual void			SetSubBankHandlers	(void);
		virtual uint8*			GetRealAddress		(emuptr address);
		virtual emuptr			GetAddressStart		(void);
		virtual uint32			GetAddressRange		(void);

	private:
		EmProxyHwrSymbolASICType	fRegs;
};

#endif	/* EmRegsASICSymbol1700_h */
