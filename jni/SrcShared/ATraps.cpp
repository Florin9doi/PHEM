/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

// ---------------------------------------------------------------------------
#pragma mark ===== Includes
// ---------------------------------------------------------------------------

#include "EmCommon.h"
#include "ATraps.h"

#include "Byteswapping.h"		// Canonical
#include "EmBankMapped.h"		// EmBankMapped::GetEmulatedAddress
#include "EmCPU68K.h"			// GetRegisters
#include "EmException.h"		// EmExceptionReset
#include "EmPalmOS.h"			// EmPalmOS
#include "EmSession.h"			// gSession
#include "ErrorHandling.h"		// Errors::ReportError
#include "Miscellaneous.h"		// StMemoryMapper
#include "EmPalmFunction.h"		// GetTrapName
#include "Profiling.h"			// StDisableAllProfiling


// ---------------------------------------------------------------------------
#pragma mark ===== Types
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
#pragma mark ===== Functions
// ---------------------------------------------------------------------------

static Bool	PrvHandleTrap12 (ExceptionNumber);


// ---------------------------------------------------------------------------
#pragma mark ===== Constants
// ---------------------------------------------------------------------------

const uint16	kOpcode_ROMCall		= m68kTrapInstr + sysDispatchTrapNum;
const uint16	kOpcode_ATrapReturn	= m68kTrapInstr + kATrapReturnTrapNum;


// ---------------------------------------------------------------------------
#pragma mark ===== Variables
// ---------------------------------------------------------------------------

// ----- Saved variables -----------------------------------------------------

// ----- UnSaved variables ---------------------------------------------------


#pragma mark -

// ===========================================================================
//		¥ ATrap
// ===========================================================================

/***********************************************************************
 *
 * FUNCTION:	ATrap::ATrap
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ATrap::ATrap (void) :
#if (__GNUC__ < 2)
/* Can't call the default constructor because there isn't one defined */
/* on a struct as there is with a class under GCC 2.8.1 */
	fNewRegisters (),
#endif
	fEmulatedStackMapper (this->GetStackBase (), kStackSize)
{
	// Get the registers.

	EmAssert (gCPU68K);
	gCPU68K->GetRegisters (fOldRegisters);
	fNewRegisters = fOldRegisters;

	// Make sure the CPU is not stopped.  I suppose that we could force the CPU
	// to no longer be stopped, but I'd rather that the Palm OS itself woke up
	// first before we try making calls into it.  Therefore, anything making
	// an out-of-the-blue Palm OS call via this class (that is, a call outside
	// of the context of a Palm OS function head- or tailpatch) should first
	// bring the CPU to a halt by calling EmSession::ExecuteUntilSysCall first.

	EmAssert (fNewRegisters.stopped == 0);

	// Give ourselves our own private stack.  We'll want this in case
	// we're in the debugger and the stack pointer is hosed.

	m68k_areg (fNewRegisters, 7) = EmBankMapped::GetEmulatedAddress (
										this->GetStackBase () + kStackSize - 4);

	// Remember this as a stack so that our stack sniffer won't complain.

	char*	stackBase = this->GetStackBase ();
	StackRange	range (	EmBankMapped::GetEmulatedAddress (&stackBase[0]), 
						EmBankMapped::GetEmulatedAddress (&stackBase[kStackSize - 4]));
	EmPalmOS::RememberStackRange (range);
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::~ATrap
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ATrap::~ATrap (void)
{
	// Put things back the way they were.

	EmPalmOS::ForgetStack (EmBankMapped::GetEmulatedAddress (this->GetStackBase ()));

	EmAssert (gCPU68K);
	gCPU68K->SetRegisters (fOldRegisters);

	// Check to see if anything interesting was registered while we
	// were making the Palm OS subroutine call.  The "check after end
	// of cycle" bit may have gotten cleared when restoring the old
	// registers, so set it on the off chance that it was.  Doing this
	// is harmless if there really aren't any scheduled tasks.

	EmAssert (gSession);
	gCPU68K->CheckAfterCycle ();
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::Call
 *
 * DESCRIPTION:	Calls the given pseudo-ATrap.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void ATrap::Call (uint16 trapWord)
{
	EmAssert (trapWord != sysTrapSysReset);

	// Up until now, the registers in "regs" have been left alone.  If any
	// values were pushed on the stack, the stack position was reflected in
	// fNewRegisters.  Now's the time to move those values from fNewRegisters
	// to regs.

	EmAssert (gCPU68K);
	gCPU68K->SetRegisters (fNewRegisters);

	// Make the call.

	this->DoCall(trapWord);

	// Remember the resulting register values so that we can report them to
	// the user when they call GetD0 and/or GetA0.

	gCPU68K->GetRegisters (fNewRegisters);
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::PushByte
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void ATrap::PushByte (uint8 iByte)
{
	StDisableAllProfiling	stopper;

	m68k_areg (fNewRegisters, 7) -= 2;
	EmMemPut8 (m68k_areg (fNewRegisters, 7), iByte);
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::PushWord
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void ATrap::PushWord (uint16 iWord)
{
	StDisableAllProfiling	stopper;

	m68k_areg (fNewRegisters, 7) -= 2;
	EmMemPut16 (m68k_areg (fNewRegisters, 7), iWord);
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::PushLong
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void ATrap::PushLong (uint32 iLong)
{
	StDisableAllProfiling	stopper;

	m68k_areg (fNewRegisters, 7) -= 4;
	EmMemPut32 (m68k_areg (fNewRegisters, 7), iLong);
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::SetNewDReg
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void ATrap::SetNewDReg (int regNum, uint32 value)
{
	m68k_dreg (fNewRegisters, regNum) = value;
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::SetNewAReg
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void ATrap::SetNewAReg (int regNum, uint32 value)
{
	m68k_areg (fNewRegisters, regNum) = value;
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::GetD0
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

uint32 ATrap::GetD0 (void)
{
	return m68k_dreg (fNewRegisters, 0);
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::GetA0
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

uint32 ATrap::GetA0 (void)
{
	return m68k_areg (fNewRegisters, 0);
}


/***********************************************************************
 *
 * FUNCTION:	ATrap::DoCall
 *
 * DESCRIPTION:	Calls the pseudo-ATrap. When we return, D0 or A0 should
 *				hold the result, the parameters will still be on the
 *				stack with the SP pointing to them, and the PC will be
 *				restored to what it was before this function was called.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void ATrap::DoCall (uint16 trapWord)
{
	// Stop all profiling activities. Stop cycle counting and stop the
	// recording of function entries and exits.  We want our calls to
	// ROM functions to be as transparent as possible.

	StDisableAllProfiling	stopper;


	// Assert that the function we're trying to call is implemented.
	//
	// Oops...bad test...this doesn't work when we're calling a library.
	// Instead, since we now invoke ROM functions by creating a TRAP $F
	// sequence, we'll let our TRAP $F handler deal with validating the
	// function call (it does that anyway).

//	EmAssert (LowMem::GetTrapAddress (trapWord));

	// We call the ROM function by dummying up a sequence of 68xxx instructions
	// for it.  The sequence of instructions is:
	//
	//			TRAP	$F
	//			DC.W	<dispatch number>
	//			TRAP	$C
	//
	// The first two words invoke the function (calling any head- or tailpatches
	// along the way).  The third word allows the emulator to regain control
	// after the function has returned.
	//
	// Note: this gets a little ugly on little-endian machines.  The following
	// instructions are stored on the emulator's stack.  This memory is mapped
	// into the emulated address space in such a fashion that no byteswapping of
	// word or long values occurs.  Thus, we have to put the data into Big Endian
	// format when putting it into the array.
	//
	// However, opcodes are a special case.  They are optimized in the emulator
	// for fast access.  Opcodes are *always* fetched a word at a time in host-
	// endian order.  Thus, the opcodes below have to be stored in host-endian
	// order.  That's why there's no call to Canonical to put them into Big
	// Endian order.

	uint16	code[] = { kOpcode_ROMCall, trapWord, kOpcode_ATrapReturn };

	// Oh, OK, we do have to byteswap the trapWord.  Opcodes are fetched with
	// EmMemDoGet16, which always gets the value in host byte order.  The
	// trapWord is fetched with EmMemGet16, which gets values according to the
	// rules of the memory bank.  For the dummy bank, the defined byte order
	// is Big Endian.

	Canonical (code[1]);

	// Map in the code stub so that the emulation code can access it.

	StMemoryMapper	mapper (code, sizeof (code));

	// Prepare to handle the TRAP 12 exception.

	EmAssert (gCPU68K);
	gCPU68K->InstallHookException (kException_ATrapReturn, PrvHandleTrap12);

	// Point the PC to our code.

	emuptr	newPC = EmBankMapped::GetEmulatedAddress (code);
	m68k_setpc (newPC);

	// Execute until the next break.

	try
	{
		EmAssert (gSession);
		gSession->ExecuteSubroutine ();
	}
	catch (EmExceptionReset& e)
	{
		e.SetTrapWord (trapWord);

		// Remove the TRAP 12 exception handler.

		EmAssert (gCPU68K);
		gCPU68K->RemoveHookException (kException_ATrapReturn, PrvHandleTrap12);

		throw;
	}

	// Remove the TRAP 12 exception handler.

	EmAssert (gCPU68K);
	gCPU68K->RemoveHookException (kException_ATrapReturn, PrvHandleTrap12);
}


// ---------------------------------------------------------------------------
//		¥ ATrap::GetStackBase
// ---------------------------------------------------------------------------

char* ATrap::GetStackBase ()
{
	// Ensure that the stack is aligned to a longword address.

	uint32	stackBase = (uint32) fStack;

	stackBase += 3;
	stackBase &= ~3;

	return (char*) stackBase;
}


// ---------------------------------------------------------------------------
//		¥ PrvHandleTrap12
// ---------------------------------------------------------------------------

Bool PrvHandleTrap12 (ExceptionNumber)
{
	EmAssert (gSession);
	gSession->ScheduleSuspendSubroutineReturn ();

	return true;
}
