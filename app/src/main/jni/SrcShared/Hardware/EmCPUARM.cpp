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
#include "EmCPUARM.h"

#include "EmSession.h"			// ExecuteSpecial

/*
	This file interfaces between the Palm OS Emulator and the embedded
	instance of the ARMulator.

	This version of the ARMulator comes from gdb 5.0, which in turn was
	derived from the ARM6 ARMulator from ARM, Inc.  ARM apparently no
	longer makes public source-code to their version of ARMulator
	available, so the version here is probably just a distant cousin
	of that version.

	The ARMulator here comes with the following files:

		* armcopro.c

			ARM coprocessor emulation

		* armemu.c

			Main ARM processor emulation.  It can be compiled for either
			26-bit or 32-bit modes.

		* arminit.c

			Contains routines to perform one-time initialization of
			ARMulator, create an emulator instance, select a processor,
			reset a processor, run one or more instructions, or trigger
			an abort.

		* armos.c

			(Something to do with knowledge of an ARM-based OS)

		* armrdi.c

			Implements the ARM RDI (Remote Debugger Interface).  All
			functions are exported via an RDI 1.0 RDIProcVec table
			(armul_rdi).

		* armsupp.c

			Support routines for common actions like determining addressing
			modes, reading and writing bytes, reading and writing registers, etc.

		* armvirt.c

			A simple memory emulator that maps in RAM pages on demand.

		* bag.c

			A simple dictionary data structure.  Used by kid.c to manage
			breakpoints.  (!!!Why are they managed there and not in armrdi?)

		* communicate.c

			Utility routines to read and write bytes, words, and strings
			over a socket.

		* gdbhost.c

			Interface routines between gdb and ARMulator.  ARMulator calls
			these functions to invoke gdb console facilities.

		* kid.c

			Infinite loops that listens on a socket for ADP packets.  It
			reads these and invokes the appropriate RDI functions in armrdi.c.

		* main.c

			Front-end application that sits between gdb and ARMulator, intercepting
			packets and passing them on.  (!!! It's not clear to me why this
			intermediary -- implemented in parent.c -- is required.)

		* parent.c

			Implements the loop that listens for packets from either gdb
			or ARMulator and forwards them on to the other.

		* thumbemu.c

			Emulator of the Thumb sub-cell.  Translates Thumb opcodes into
			ARM opcodes, and then invokes the ARM opcode emulator.

		* wrapper.c

			ARMulator wrapper that allows it to be incorporated into the gdb
			Simulator system.  This includes implementing functions that
			respond to the gdb set of remote debugger commands.

	When incorporated into the gdb Simulator system, only the files armcopro.c,
	armemu.c, arminit.c, armos.c, armsupp.c, armvirt.c, bag.c thumbemu.c, and
	wrapper.c seem to be used.  (!!! Why bag.c, when it's only used by kid.c?)
*/

#if 0

extern "C"
{
#include "armdefs.h"
#include "armemu.h"
#include "arm/dbg_rdi.h"			// RDIProcVec
#include "arm/dbg_hif.h"			// Dbg_HostosInterface (struct)
#include "arm/dbg_conf.h"			// Dbg_ConfigBlock, Dbg_HostosInterface (typdef)
}

extern "C"
{

int stop_simulator;		// Referenced in ARMul_Emulate32 loop

int mumkid[2] = {1, 2};			// Referenced in kid.c
int kidmum[2] = {3, 4};			// Referenced in kid.c, armrdi.c

/* RDI interface */
extern const struct RDIProcVec armul_rdi;

void ARMul_CheckAfterCycle (void);
void ARMul_SetSession (void* session);

void EmCPUARM_CheckAfterCycle (void* session)
{
	((EmSession*) session)->ExecuteSpecial (false);
}


/***************************************************************************\
*            Time for the Operating System to initialise itself.            *
\***************************************************************************/

// Referenced in RDI_open in armrdi.c

unsigned
ARMul_OSInit (ARMul_State * state)
{
  return (TRUE);
}

// Referenced in RDI_close in armrdi.c

void
ARMul_OSExit (ARMul_State * state)
{
}

/***************************************************************************\
*                  Return the last Operating System Error.                  *
\***************************************************************************/

// Referenced in RDI_info in armrdi.c

ARMword ARMul_OSLastErrorP (ARMul_State * state)
{
  return 0;
}

/***************************************************************************\
* The emulator calls this routine when a SWI instruction is encuntered. The *
* parameter passed is the SWI number (lower 24 bits of the instruction).    *
\***************************************************************************/

// Referenced in "case 0x7f" of ARMul_Emulate32 in armemu.c
// Referenced in "case 0xf0..0xff" of ARMul_Emulate32 in armemu.c

unsigned
ARMul_OSHandleSWI (ARMul_State * state, ARMword number)
{
	return (FALSE);
}

/***************************************************************************\
* The emulator calls this routine when an Exception occurs.  The second     *
* parameter is the address of the relevant exception vector.  Returning     *
* FALSE from this routine causes the trap to be taken, TRUE causes it to    *
* be ignored (so set state->Emulate to FALSE!).                             *
\***************************************************************************/

// Referenced in ARMul_Abort in arminit.c

unsigned
ARMul_OSException (ARMul_State * state, ARMword vector, ARMword pc)
{
	return (FALSE);
}

}

#endif


#pragma mark -

EmCPUARM*	gCPUARM;

// ---------------------------------------------------------------------------
//		¥ EmCPUARM::EmCPUARM
// ---------------------------------------------------------------------------

EmCPUARM::EmCPUARM (EmSession* session) :
	EmCPU (session)
{
	this->DoReset (true);

	EmAssert (gCPUARM == NULL);
	gCPUARM = this;
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::~EmCPUARM
// ---------------------------------------------------------------------------

EmCPUARM::~EmCPUARM (void)
{
#if 0
	/* int	i = */ armul_rdi.close ();
#endif

	EmAssert (gCPUARM == this);
	gCPUARM = NULL;
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::Reset
// ---------------------------------------------------------------------------

void EmCPUARM::Reset (Bool hardwareReset)
{
	if (hardwareReset)
	{
		this->DoReset (false);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::Save
// ---------------------------------------------------------------------------

void EmCPUARM::Save (SessionFile&)
{
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::Load
// ---------------------------------------------------------------------------

void EmCPUARM::Load (SessionFile&)
{
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::Execute
// ---------------------------------------------------------------------------

void EmCPUARM::Execute (void)
{
#if 0
	PointHandle	point;
	/* int	i = */ armul_rdi.execute (&point);
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::CheckAfterCycle
// ---------------------------------------------------------------------------

void EmCPUARM::CheckAfterCycle (void)
{
#if 0
	ARMul_CheckAfterCycle ();
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::GetPC
// ---------------------------------------------------------------------------

emuptr EmCPUARM::GetPC (void)
{
	return EmMemNULL;
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::GetSP
// ---------------------------------------------------------------------------

emuptr EmCPUARM::GetSP (void)
{
	return EmMemNULL;
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::GetRegister
// ---------------------------------------------------------------------------

uint32 EmCPUARM::GetRegister (int /*index*/)
{
	return 0;
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::SetPC
// ---------------------------------------------------------------------------

void EmCPUARM::SetPC (emuptr /*newPC*/)
{
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::SetSP
// ---------------------------------------------------------------------------

void EmCPUARM::SetSP (emuptr /*newPC*/)
{
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::SetRegister
// ---------------------------------------------------------------------------

void EmCPUARM::SetRegister (int /*index*/, uint32 /*val*/)
{
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::Stopped
// ---------------------------------------------------------------------------
// Return whether or not the CPU itself is halted.  This is seperate from
// whether or not the session (that is, the thread emulating the CPU) is
// halted.

Bool EmCPUARM::Stopped (void)
{
	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmCPUARM::DoReset
// ---------------------------------------------------------------------------

void EmCPUARM::DoReset (Bool /*cold*/)
{
#if 0
	// Taken from RDP_Start in kid.c

	Dbg_ConfigBlock		config;
	config.processor	= 0;
	config.memorysize	= 1024 * 1024L;
	config.bytesex		= RDISex_Little;

	Dbg_HostosInterface	hostif;

	hostif.dbgprint		= NULL;	// myprint;	// Called in ARMul_DebugPrint, ARMul_DebugPrint_i, which are called only if RDI_VERBOSE is defined.
	hostif.dbgpause		= NULL;	// mypause;	// Called in ARMul_DebugPause, which in turn is not called.
	hostif.dbgarg		= NULL;	// stdout;	// Passed to dbgprint and dbgpause.
	hostif.writec		= NULL;	// mywritec;// Not called
	hostif.readc		= NULL;	// myreadc;	// Not called
	hostif.write		= NULL;	// mywrite;	// Not called
	hostif.gets			= NULL;	// mygets;	// Not called
	hostif.hostosarg	= NULL;				// Not referenced
	hostif.reset		= NULL;	// mypause;	// Not referenced
	hostif.resetarg		= NULL;				// Not referenced

	/* int	i = */ armul_rdi.open (cold ? 0 : 1, &config, &hostif, NULL);

	ARMul_SetSession (fSession);
#endif
}
