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
#include "EmBankDRAM.h"

#include "DebugMgr.h"			// Debug::CheckStepSpy
#include "EmBankSRAM.h"			// gRAMBank_Size, gRAM_Memory, gMemoryAccess
#include "EmCPU.h"				// GetSP
#include "EmCPU68K.h"			// gCPU68K
#include "EmHAL.h"				// EmHAL
#include "EmMemory.h"			// Memory::InitializeBanks, IsPCInRAM (implicitly, through META_CHECK)
#include "EmPalmFunction.h"		// InSysLaunch
#include "EmPalmOS.h"			// EmPalmOS::GetBootStack
#include "EmPatchState.h"		// META_CHECK calls EmPatchState::IsPCInMemMgr
#include "EmScreen.h"			// EmScreen::MarkDirty
#include "EmSession.h"			// gSession
#include "MetaMemory.h"			// MetaMemory
#include "Profiling.h"			// WAITSTATES_DRAM

#include <cstddef>

// ---------------------------------------------------------------------------
#pragma mark ===== Types
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
#pragma mark ===== Functions
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
#pragma mark ===== Constants
// ---------------------------------------------------------------------------

const uint32	kMemoryStart = 0x00000000;
const emuptr	kGlobalStart = offsetof (LowMemHdrType, globals);


// ---------------------------------------------------------------------------
#pragma mark ===== Variables
// ---------------------------------------------------------------------------

// ----- Saved variables -----------------------------------------------------

static uint32	gDynamicHeapSize;


// ----- UnSaved variables ---------------------------------------------------

static EmAddressBank	gAddressBank =
{
	EmBankDRAM::GetLong,
	EmBankDRAM::GetWord,
	EmBankDRAM::GetByte,
	EmBankDRAM::SetLong,
	EmBankDRAM::SetWord,
	EmBankDRAM::SetByte,
	EmBankDRAM::GetRealAddress,
	EmBankDRAM::ValidAddress,
	EmBankDRAM::GetMetaAddress,
	EmBankDRAM::AddOpcodeCycles
};


// ---------------------------------------------------------------------------
#pragma mark ===== Inlines
// ---------------------------------------------------------------------------

static inline int InlineValidAddress (emuptr address, size_t size)
{
	int	result = (address + size) <= gRAMBank_Size;

	return result;
}


static inline uint8* InlineGetRealAddress (emuptr address)
{
	return (uint8*) &(gRAM_Memory[address]);
}


static inline uint8* InlineGetMetaAddress (emuptr address)
{
	return (uint8*) &(gRAM_MetaMemory[address]);
}


static void PrvReportBelowStackPointerAccess (emuptr address, size_t size, Bool forRead)
{
	// Can be NULL during bootup.

	if (gStackLow == EmMemNULL)
		return;

	// SysLaunch clears out as much RAM as it can, including the area
	// below the stack pointer.

	if (::InSysLaunch ())
		return;

	// We can look in the stack for CJ_TAGFENCE, etc.

	if (CEnableFullAccess::AccessOK ())
		return;

	// If the access is in the boot stack, allow it.  There's a lot of memory
	// allocation going on below the stack pointer, such that it's not too
	// clear where the current "low end" of stack should be.

	StackRange	bootStack = EmPalmOS::GetBootStack ();
	if (address >= bootStack.fBottom && address < bootStack.fTop)
		return;

	gSession->ScheduleDeferredError (new EmDeferredErrLowStack (
		gStackLow, gCPU->GetSP (), gStackHigh,
		address, size, forRead));
}


static inline void PrvCheckBelowStackPointerAccess (emuptr address, size_t size, Bool forRead)
{
#if 1
	if (address < gCPU->GetSP () && address >= gStackLow)
	{
		::PrvReportBelowStackPointerAccess (address, size, forRead);
	}
#endif
}


static inline void PrvScreenCheck (uint8* metaAddress, emuptr address, size_t size)
{
#if defined (macintosh)

	if (size == 1 && !MetaMemory::IsScreenBuffer8 (metaAddress))
		return;

	if (size == 2 && !MetaMemory::IsScreenBuffer16 (metaAddress))
		return;

	if (size == 4 && !MetaMemory::IsScreenBuffer32 (metaAddress))
		return;

#else

	if (MetaMemory::IsScreenBuffer (metaAddress, size))

#endif

	{
		EmScreen::MarkDirty (address, size);
	}
}


#pragma mark -

// ===========================================================================
//		¥ DRAM Bank Accessors
// ===========================================================================
// These functions provide fetch and store access to the emulator's random
// access memory.

/***********************************************************************
 *
 * FUNCTION:	EmBankDRAM::Initialize
 *
 * DESCRIPTION: Standard initialization function.  Responsible for
 *				initializing this sub-system when a new session is
 *				created.  Will be followed by at least one call to
 *				Reset or Load.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmBankDRAM::Initialize (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmBankDRAM::Reset
 *
 * DESCRIPTION:	Standard reset function.  Sets the sub-system to a
 *				default state.  This occurs not only on a Reset (as
 *				from the menu item), but also when the sub-system
 *				is first initialized (Reset is called after Initialize)
 *				as well as when the system is re-loaded from an
 *				insufficient session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmBankDRAM::Reset (Bool /*hardwareReset*/)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmBankDRAM::Save
 *
 * DESCRIPTION:	Standard save function.  Saves any sub-system state to
 *				the given session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmBankDRAM::Save (SessionFile&)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmBankDRAM::Load
 *
 * DESCRIPTION:	Standard load function.  Loads any sub-system state
 *				from the given session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmBankDRAM::Load (SessionFile&)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmBankDRAM::Dispose
 *
 * DESCRIPTION:	Standard dispose function.  Completely release any
 *				resources acquired or allocated in Initialize and/or
 *				Load.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmBankDRAM::Dispose (void)
{
}


/***********************************************************************
 *
 * FUNCTION:    EmBankDRAM::SetBankHandlers
 *
 * DESCRIPTION: Set the bank handlers UAE uses to dispatch memory
 *				access operations.
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void EmBankDRAM::SetBankHandlers (void)
{
	// First few memory banks are managed by the functions in EmBankDRAM.

	gDynamicHeapSize = EmHAL::GetDynamicHeapSize ();
	
	if (gDynamicHeapSize > gRAMBank_Size)
		gDynamicHeapSize = gRAMBank_Size;

	uint32	sixtyFourK	= 64 * 1024L;
	uint32	numBanks	= (gDynamicHeapSize + sixtyFourK - 1) / sixtyFourK;

	Memory::InitializeBanks (	gAddressBank,
								EmMemBankIndex (kMemoryStart),
								numBanks);
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::GetLong
// ---------------------------------------------------------------------------

uint32 EmBankDRAM::GetLong (emuptr address)
{
	if (address > gDynamicHeapSize)
		return EmBankSRAM::GetLong (address);

#if (PROFILE_MEMORY)
	gMemoryAccess[kDRAMLongRead]++;
	if (address & 2)
		gMemoryAccess[kDRAMLongRead2]++;
#endif

	if (CHECK_FOR_ADDRESS_ERROR && (address & 1) != 0)
	{
		AddressError (address, sizeof (uint32), true);
	}

	register uint8*	metaAddress = InlineGetMetaAddress (address);
	META_CHECK (metaAddress, address, GetLong, uint32, true);

	::PrvCheckBelowStackPointerAccess (address, sizeof (uint32), true);

	if (VALIDATE_DRAM_GET &&
		gMemAccessFlags.fValidate_DRAMGet &&
		!InlineValidAddress (address, sizeof (uint32)))
	{
		InvalidAccess (address, sizeof (uint32), true);
	}

#if (HAS_PROFILING)
	CYCLE_GETLONG (WAITSTATES_DRAM);
#endif

	return EmMemDoGet32 (gRAM_Memory + address);
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::GetWord
// ---------------------------------------------------------------------------

uint32 EmBankDRAM::GetWord (emuptr address)
{
	if (address > gDynamicHeapSize)
		return EmBankSRAM::GetWord (address);

#if (PROFILE_MEMORY)
	gMemoryAccess[kDRAMWordRead]++;
#endif

	if (CHECK_FOR_ADDRESS_ERROR && (address & 1) != 0)
	{
		AddressError (address, sizeof (uint16), true);
	}

	register uint8*	metaAddress = InlineGetMetaAddress (address);
	META_CHECK (metaAddress, address, GetWord, uint16, true);

	::PrvCheckBelowStackPointerAccess (address, sizeof (uint16), true);

	if (VALIDATE_DRAM_GET &&
		gMemAccessFlags.fValidate_DRAMGet &&
		!InlineValidAddress (address, sizeof (uint16)))
	{
		InvalidAccess (address, sizeof (uint16), true);
	}

#if (HAS_PROFILING)
	CYCLE_GETWORD (WAITSTATES_DRAM);
#endif

	return EmMemDoGet16 (gRAM_Memory + address);
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::GetByte
// ---------------------------------------------------------------------------

uint32 EmBankDRAM::GetByte (emuptr address)
{
	if (address > gDynamicHeapSize)
		return EmBankSRAM::GetByte (address);

#if (PROFILE_MEMORY)
	gMemoryAccess[kDRAMByteRead]++;
#endif

	register uint8*	metaAddress = InlineGetMetaAddress (address);
	META_CHECK (metaAddress, address, GetByte, uint8, true);

	::PrvCheckBelowStackPointerAccess (address, sizeof (uint8), true);

	if (VALIDATE_DRAM_GET &&
		gMemAccessFlags.fValidate_DRAMGet &&
		!InlineValidAddress (address, sizeof (uint8)))
	{
		InvalidAccess (address, sizeof (uint8), true);
	}

#if (HAS_PROFILING)
	CYCLE_GETBYTE (WAITSTATES_DRAM);
#endif

	return EmMemDoGet8 (gRAM_Memory + address);
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::SetLong
// ---------------------------------------------------------------------------

void EmBankDRAM::SetLong (emuptr address, uint32 value)
{
	if (address > gDynamicHeapSize)
	{
		EmBankSRAM::SetLong (address, value);
		return;
	}

#if (PROFILE_MEMORY)
	gMemoryAccess[kDRAMLongWrite]++;
	if (address & 2)
		gMemoryAccess[kDRAMLongWrite2]++;
#endif

	if (CHECK_FOR_ADDRESS_ERROR && (address & 1) != 0)
	{
		AddressError (address, sizeof (uint32), false);
	}

	register uint8*	metaAddress = InlineGetMetaAddress (address);
	META_CHECK (metaAddress, address, SetLong, uint32, false);

	::PrvCheckBelowStackPointerAccess (address, sizeof (uint32), false);

	if (VALIDATE_DRAM_SET &&
		gMemAccessFlags.fValidate_DRAMSet &&
		!InlineValidAddress (address, sizeof (uint32)))
	{
		InvalidAccess (address, sizeof (uint32), false);
	}

	::PrvScreenCheck (metaAddress, address, sizeof (uint32));

#if (HAS_PROFILING)
	CYCLE_PUTLONG (WAITSTATES_DRAM);
#endif

	EmMemDoPut32 (gRAM_Memory + address, value);

#if FOR_LATER
	// Mark that this memory location can now be read from.

	MetaMemory::MarkLongInitialized (address);
#endif

	// See if any interesting memory locations have changed.  If so,
	// CheckStepSpy will report it.

	Debug::CheckStepSpy (address, sizeof (uint32));
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::SetWord
// ---------------------------------------------------------------------------

void EmBankDRAM::SetWord (emuptr address, uint32 value)
{
	if (address > gDynamicHeapSize)
	{
		EmBankSRAM::SetWord (address, value);
		return;
	}

#if (PROFILE_MEMORY)
	gMemoryAccess[kDRAMWordWrite]++;
#endif

	if (CHECK_FOR_ADDRESS_ERROR && (address & 1) != 0)
	{
		AddressError (address, sizeof (uint16), false);
	}

	register uint8*	metaAddress = InlineGetMetaAddress (address);
	META_CHECK (metaAddress, address, SetWord, uint16, false);

	::PrvCheckBelowStackPointerAccess (address, sizeof (uint16), false);

	if (VALIDATE_DRAM_SET &&
		gMemAccessFlags.fValidate_DRAMSet &&
		!InlineValidAddress (address, sizeof (uint16)))
	{
		InvalidAccess (address, sizeof (uint16), false);
	}

	::PrvScreenCheck (metaAddress, address, sizeof (uint16));

#if (HAS_PROFILING)
	CYCLE_PUTWORD (WAITSTATES_DRAM);
#endif

	EmMemDoPut16 (gRAM_Memory + address, value);

#if FOR_LATER
	// Mark that this memory location can now be read from.

	MetaMemory::MarkWordInitialized (address);
#endif

	// See if any interesting memory locations have changed.  If so,
	// CheckStepSpy will report it.

	Debug::CheckStepSpy (address, sizeof (uint16));
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::SetByte
// ---------------------------------------------------------------------------

void EmBankDRAM::SetByte (emuptr address, uint32 value)
{
	if (address > gDynamicHeapSize)
	{
		EmBankSRAM::SetByte (address, value);
		return;
	}

#if (PROFILE_MEMORY)
	gMemoryAccess[kDRAMByteWrite]++;
#endif

	register uint8*	metaAddress = InlineGetMetaAddress (address);
	META_CHECK (metaAddress, address, SetByte, uint8, false);

	::PrvCheckBelowStackPointerAccess (address, sizeof (uint8), false);

	if (VALIDATE_DRAM_SET &&
		gMemAccessFlags.fValidate_DRAMSet &&
		!InlineValidAddress (address, sizeof (uint8)))
	{
		InvalidAccess (address, sizeof (uint8), false);
	}

	::PrvScreenCheck (metaAddress, address, sizeof (uint8));

#if (HAS_PROFILING)
	CYCLE_PUTBYTE (WAITSTATES_DRAM);
#endif

	EmMemDoPut8 (gRAM_Memory + address, value);

#if FOR_LATER
	// Mark that this memory location can now be read from.

	MetaMemory::MarkByteInitialized (address);
#endif

	// See if any interesting memory locations have changed.  If so,
	// CheckStepSpy will report it.

	Debug::CheckStepSpy (address, sizeof (uint8));
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::ValidAddress
// ---------------------------------------------------------------------------

int EmBankDRAM::ValidAddress (emuptr address, uint32 size)
{
	int	result = InlineValidAddress (address, size);

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::GetRealAddress
// ---------------------------------------------------------------------------

uint8* EmBankDRAM::GetRealAddress (emuptr address)
{
	return InlineGetRealAddress (address);
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::GetMetaAddress
// ---------------------------------------------------------------------------

uint8* EmBankDRAM::GetMetaAddress (emuptr address)
{
	return InlineGetMetaAddress (address);
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::AddOpcodeCycles
// ---------------------------------------------------------------------------

void EmBankDRAM::AddOpcodeCycles (void)
{
#if (HAS_PROFILING)
	CYCLE_GETWORD (WAITSTATES_DRAM);
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::AddressError
// ---------------------------------------------------------------------------

void EmBankDRAM::AddressError (emuptr address, long size, Bool forRead)
{
	EmAssert (gCPU68K);
	gCPU68K->AddressError (address, size, forRead);
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::InvalidAccess
// ---------------------------------------------------------------------------

void EmBankDRAM::InvalidAccess (emuptr address, long size, Bool forRead)
{
	EmAssert (gCPU68K);
	gCPU68K->BusError (address, size, forRead);
}


// ---------------------------------------------------------------------------
//		¥ EmBankDRAM::ProbableCause
// ---------------------------------------------------------------------------

void EmBankDRAM::ProbableCause (emuptr address, long size, Bool forRead)
{
	EmAssert (gSession);

	Errors::EAccessType	whatHappened = MetaMemory::GetWhatHappened (address, size, forRead);

	switch (whatHappened)
	{
		case Errors::kOKAccess:
			break;

		case Errors::kLowMemAccess:
			gSession->ScheduleDeferredError (new EmDeferredErrLowMemory (address, size, forRead));
			break;

		case Errors::kGlobalVarAccess:
			gSession->ScheduleDeferredError (new EmDeferredErrSystemGlobals (address, size, forRead));
			break;

		case Errors::kScreenAccess:
			gSession->ScheduleDeferredError (new EmDeferredErrScreen (address, size, forRead));
			break;

		case Errors::kMemMgrAccess:
			gSession->ScheduleDeferredError (new EmDeferredErrMemMgrStructures (address, size, forRead));
			break;

		case Errors::kFreeChunkAccess:
			gSession->ScheduleDeferredError (new EmDeferredErrFreeChunk (address, size, forRead));
			break;

		case Errors::kUnlockedChunkAccess:
			gSession->ScheduleDeferredError (new EmDeferredErrUnlockedChunk (address, size, forRead));
			break;

		case Errors::kUnknownAccess:
		case Errors::kLowStackAccess:
			EmAssert (false);
			MetaMemory::GetWhatHappened (address, size, forRead);
	}
}
