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
#include "EmRegsUSBVisor.h"

#include "EmSession.h"			// GetDevice
#include "EmHAL.h"				// EmHAL::GetROMSize
#include "EmBankROM.h"			// EmBankROM::GetMemoryStart


// Given a register (specified by its field name), return its address
// in emulated space.

#define addressof(reg)				\
	(this->GetAddressStart () + fRegs.offsetof_##reg ())


// Macro to help the installation of handlers for a register.

#define INSTALL_HANDLER(read, write, reg)			\
	this->SetHandler (	(ReadFunction) &EmRegsUSBVisor::read,		\
						(WriteFunction) &EmRegsUSBVisor::write,		\
						addressof (reg),			\
						fRegs.reg.GetSize ())


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::EmRegsUSBVisor
// ---------------------------------------------------------------------------

EmRegsUSBVisor::EmRegsUSBVisor (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::~EmRegsUSBVisor
// ---------------------------------------------------------------------------

EmRegsUSBVisor::~EmRegsUSBVisor (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::Initialize
// ---------------------------------------------------------------------------

void EmRegsUSBVisor::Initialize (void)
{
	EmRegs::Initialize ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::Reset
// ---------------------------------------------------------------------------

void EmRegsUSBVisor::Reset (Bool hardwareReset)
{
	EmRegs::Reset (hardwareReset);

	if (hardwareReset)
	{
		memset (fRegs.GetPtr (), 0, fRegs.GetSize ());
	}
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::Save
// ---------------------------------------------------------------------------

void EmRegsUSBVisor::Save (SessionFile& f)
{
	EmRegs::Save (f);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::Load
// ---------------------------------------------------------------------------

void EmRegsUSBVisor::Load (SessionFile& f)
{
	EmRegs::Load (f);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::Dispose
// ---------------------------------------------------------------------------

void EmRegsUSBVisor::Dispose (void)
{
	EmRegs::Dispose ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::SetSubBankHandlers
// ---------------------------------------------------------------------------

void EmRegsUSBVisor::SetSubBankHandlers (void)
{
	// Install base handlers.

	EmRegs::SetSubBankHandlers ();

	// Now add standard/specialized handers for the defined registers.

	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				data);
	INSTALL_HANDLER (StdReadBE,			StdWriteBE,				command);
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::GetRealAddress
// ---------------------------------------------------------------------------

uint8* EmRegsUSBVisor::GetRealAddress (emuptr address)
{
	return (uint8*) fRegs.GetPtr () + address - this->GetAddressStart ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::GetAddressStart
// ---------------------------------------------------------------------------

emuptr EmRegsUSBVisor::GetAddressStart (void)
{
	/* 
		Auto-detect USB hardware base address.  In the Visor, the USB hardware 
		is at CSA1, which resides just after the ROM (CSA0) in memory.  
	*/

	EmDevice device = gSession->GetDevice ();
	if (device.HardwareID () == 0x0a /*halModelIDVisorPrism*/)	// in Prism it is not at CSA1
		return (emuptr) 0x10800000L;

	return EmBankROM::GetMemoryStart() + EmHAL::GetROMSize ();
}


// ---------------------------------------------------------------------------
//		¥ EmRegsUSBVisor::GetAddressRange
// ---------------------------------------------------------------------------

uint32 EmRegsUSBVisor::GetAddressRange (void)
{
	return fRegs.GetSize ();
}
