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
#include "EmRegsASICSymbol1700.h"

// Given a register (specified by its field name), return its address
// in emulated space.

#define addressof(reg)				\
	(this->GetAddressStart () + fRegs.offsetof_##reg ())


// Macro to help the installation of handlers for a register.

#define INSTALL_HANDLER(read, write, reg)			\
	this->SetHandler (	(ReadFunction) &EmRegsASICSymbol1700::read,		\
						(WriteFunction) &EmRegsASICSymbol1700::write,		\
						addressof (reg),			\
						fRegs.reg.GetSize ())


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::EmRegsASICSymbol1700
// ---------------------------------------------------------------------------

EmRegsASICSymbol1700::EmRegsASICSymbol1700 (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::~EmRegsASICSymbol1700
// ---------------------------------------------------------------------------

EmRegsASICSymbol1700::~EmRegsASICSymbol1700 (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::Initialize
// ---------------------------------------------------------------------------

void EmRegsASICSymbol1700::Initialize (void)
{
	EmRegs::Initialize ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::Reset
// ---------------------------------------------------------------------------

void EmRegsASICSymbol1700::Reset (Bool hardwareReset)
{
	EmRegs::Reset (hardwareReset);

	memset (fRegs.GetPtr (), 0, fRegs.GetSize ());
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::Save
// ---------------------------------------------------------------------------

void EmRegsASICSymbol1700::Save (SessionFile& f)
{
	EmRegs::Save (f);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::Load
// ---------------------------------------------------------------------------

void EmRegsASICSymbol1700::Load (SessionFile& f)
{
	EmRegs::Load (f);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::Dispose
// ---------------------------------------------------------------------------

void EmRegsASICSymbol1700::Dispose (void)
{
	EmRegs::Dispose ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::SetSubBankHandlers
// ---------------------------------------------------------------------------

void EmRegsASICSymbol1700::SetSubBankHandlers (void)
{
	// Install base handlers.

	EmRegs::SetSubBankHandlers ();

	// Now add standard/specialized handers for the defined registers.

		// Install the first three by hand instead of using our
		// INSTALL_HANDLER macro.  That macro doesn't support the
		// installation of a field that's an array.
	this->SetHandler (	(ReadFunction) &EmRegsASICSymbol1700::StdReadBE,		\
						(WriteFunction) &EmRegsASICSymbol1700::StdWriteBE,		\
						addressof (S24IO),				\
						64);
	this->SetHandler (	(ReadFunction) &EmRegsASICSymbol1700::StdReadBE,		\
						(WriteFunction) &EmRegsASICSymbol1700::StdWriteBE,		\
						addressof (S24Attribute),		\
						32);
	this->SetHandler (	(ReadFunction) &EmRegsASICSymbol1700::StdReadBE,		\
						(WriteFunction) &EmRegsASICSymbol1700::StdWriteBE,		\
						addressof (UART8251MacroSelect),\
						4);

//	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				S24IO);
//	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				S24Attribute);
//	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				UART8251MacroSelect);
	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				ScannerDecoderControl);
	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				Control);
	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				GPIOData);
	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				GPIODirection);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::GetRealAddress
// ---------------------------------------------------------------------------

uint8* EmRegsASICSymbol1700::GetRealAddress (emuptr address)
{
	return (uint8*) fRegs.GetPtr () + address - this->GetAddressStart ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::GetAddressStart
// ---------------------------------------------------------------------------

emuptr EmRegsASICSymbol1700::GetAddressStart (void)
{
	return 0x11000000;
}


// ---------------------------------------------------------------------------
//		¥ EmRegsASICSymbol1700::GetAddressRange
// ---------------------------------------------------------------------------

uint32 EmRegsASICSymbol1700::GetAddressRange (void)
{
	return fRegs.GetSize ();
}
