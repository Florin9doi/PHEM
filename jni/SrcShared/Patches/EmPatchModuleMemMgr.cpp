/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	Copyright (c) 2001 PocketPyro, Inc.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmPatchModuleSys.h"

#include "EmLowMem.h"			// EmLowMem_GetGlobal
#include "EmMemory.h"			// CEnableFullAccess
#include "EmPalmOS.h"			// ForgetStacksIn
#include "EmPalmFunction.h"		// FindFunctionName
#include "EmPalmHeap.h"			// EmPalmHeap::MemMgrInit, etc.
#include "EmPatchState.h"		// EnterMemMgr, ExitMemMgr,etc.
#include "EmSession.h"			// ScheduleDeferredError
#include "EmSubroutine.h"
#include "Hordes.h"				// gWarningHappened (!!! There's gotta be a better place for this!)
#include "Logging.h"			// LogAppendMsg
#include "Marshal.h"			// CALLED_SETUP
#include "MetaMemory.h"			// MetaMemory mark functions
#include "PreferenceMgr.h"		// ShouldContinue
#include "ROMStubs.h"			// MemHandleLock, MemHandleUnlock


// ======================================================================
//	Proto patch table for the system functions.  This array will be used
//	to create a sparse array at runtime.
// ======================================================================

ProtoPatchTableEntry	gProtoMemMgrPatchTable[] =
{
	#undef INSTALL_PATCH
	#define INSTALL_PATCH(fn) {sysTrap##fn, MemMgrHeadpatch::fn, MemMgrTailpatch::fn},

	FOR_EACH_MEMMGR_FUNCTION(INSTALL_PATCH)

	{0,	NULL,	NULL}
};


#ifdef FILL_BLOCKS
const int			kChunkNewValue		= 0x31;
const int			kChunkResizeValue	= 0x33;
const int			kChunkFreeValue		= 0x35;
const int			kHandleNewValue		= 0x71;
const int			kHandleResizeValue	= 0x73;
const int			kHandleFreeValue	= 0x75;
const int			kPtrNewValue		= 0x91;
const int			kPtrResizeValue		= 0x93;
const int			kPtrFreeValue		= 0x95;
#endif

static void			PrvRememberHeapAndPtr	(EmPalmHeap* h, emuptr p);
static EmPalmHeap*	PrvGetRememberedHeap	(emuptr p);

static void			PrvRememberChunk		(emuptr);
static void			PrvRememberHandle		(emuptr);
static void			PrvForgetChunk			(emuptr);
static void			PrvForgetHandle			(emuptr);
static void			PrvForgetAll			(UInt16 heapID, UInt16 ownerID);
static int			PrvCountLeaks			(UInt16 ownerID);
static void			PrvReportMemoryLeaks	(UInt16 ownerID);
static void			PrvReportLeakedChunk	(const EmTrackedChunk& tracked,
											 const EmPalmChunk& chunk);
static Bool			PrvLeakException		(const EmPalmChunk& chunk);


// Define a bunch of wrapper head- and tailpatches.
// These are defined via macros for those functions that don't have a
// complex implementation.

#define DEFINE_HEAD_PATCH(fn) CallROMType MemMgrHeadpatch::fn (void) { EmPatchState::EnterMemMgr (#fn); return kExecuteROM; }
#define DEFINE_TAIL_PATCH(fn) void MemMgrTailpatch::fn (void) { EmPatchState::ExitMemMgr (#fn); }

FOR_EACH_STUB_HEADPATCH_FUNCTION(DEFINE_HEAD_PATCH)
FOR_EACH_STUB_BOTHPATCH_FUNCTION(DEFINE_HEAD_PATCH)

FOR_EACH_STUB_TAILPATCH_FUNCTION(DEFINE_TAIL_PATCH)
FOR_EACH_STUB_BOTHPATCH_FUNCTION(DEFINE_TAIL_PATCH)


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	MemMgrHeadpatch::MemChunkFree
 *
 * DESCRIPTION:	If the user wants disposed blocks to be filled with a
 *				special value, handle that here.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType MemMgrHeadpatch::MemChunkFree (void)
{
	// Err MemChunkFree(MemPtr chunkDataP)

	EmPatchState::EnterMemMgr ("MemChunkFree");

	CALLED_SETUP ("Err", "MemPtr p");

	CALLED_GET_PARAM_VAL (MemPtr, p);

#ifdef FILL_BLOCKS

	gHeapID = ::MemPtrHeapID (p);

	if (p && gPrefs->FillDisposedBlocks ())
	{
		UInt32	size = ::MemPtrSize (p);

		CEnableFullAccess	munge;
		uae_memset (p, kChunkFreeValue, size);
	}

#endif

	::PrvForgetChunk ((emuptr) (MemPtr) p);

	// Remember what heap the chunk was in for later when
	// we need to resync with that heap.

	EmPalmHeap*	heap = const_cast<EmPalmHeap*>(EmPalmHeap::GetHeapByPtr (p));
	::PrvRememberHeapAndPtr (heap, (emuptr) (MemPtr) p);

	// In case this chunk contained a stack, forget all references
	// to that stack (or those stacks).

	if (heap)
	{
		const EmPalmChunk*	chunk = heap->GetChunkBodyContaining ((emuptr) (MemPtr) p);
		if (chunk)
		{
			EmPalmOS::ForgetStacksIn (chunk->BodyStart (), chunk->BodySize ());
		}
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrHeadpatch::MemHandleFree
 *
 * DESCRIPTION:	If the user wants disposed blocks to be filled with a
 *				special value, handle that here.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType MemMgrHeadpatch::MemHandleFree (void)
{
	// Err MemHandleFree(MemHandle h)

	EmPatchState::EnterMemMgr ("MemHandleFree");

	CALLED_SETUP ("Err", "MemHandle h");

	CALLED_GET_PARAM_VAL (MemHandle, h);

#ifdef FILL_BLOCKS

	gHeapID = ::MemHandleHeapID (h);

	if (h && gPrefs->FillDisposedBlocks ())
	{
		UInt32	size = ::MemHandleSize (h);
		if (p)
		{
			{
				CEnableFullAccess	munge;
				uae_memset (p, kHandleFreeValue, size);
			}
		}
	}

#endif

	::PrvForgetHandle ((emuptr) (MemHandle) h);

	// Remember what heap the chunk was in for later when
	// we need to resync with that heap.

	EmPalmHeap*	heap = const_cast<EmPalmHeap*>(EmPalmHeap::GetHeapByHdl (h));
	::PrvRememberHeapAndPtr (heap, (emuptr) (MemHandle) h);

	// In case this chunk contained a stack, forget all references
	// to that stack (or those stacks).

	if (heap)
	{
		const EmPalmChunk*	chunk = heap->GetChunkReferencedBy (h);
		if (chunk)
		{
			EmPalmOS::ForgetStacksIn (chunk->BodyStart (), chunk->BodySize ());
		}
	}

	if (h)
	{
		// In case this chunk contained system code, invalid our cache
		// of valid system code chunks.

		emuptr	p = (emuptr) EmPalmHeap::DerefHandle (h);
		MetaMemory::ChunkUnlocked (p);

		// In case this chunk held a bitmap, drop it from our list of
		// registered bitmaps.

		MetaMemory::UnregisterBitmapHandle (h);
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrHeadpatch::MemHandleUnlock
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType MemMgrHeadpatch::MemHandleUnlock (void)
{
	// Err MemHandleUnlock(MemHandle h) 

	EmPatchState::EnterMemMgr ("MemHandleUnlock");

	CALLED_SETUP ("Err", "MemHandle h");

	CALLED_GET_PARAM_VAL (MemHandle, h);

	// If this handle was for a bitmap resource, forget the pointer
	// we registered earlier.

	if (MetaMemory::IsBitmapHandle (h))
	{
		EmPalmHeap*	heap = const_cast<EmPalmHeap*>(EmPalmHeap::GetHeapByHdl (h));

		if (heap)
		{
			emuptr	p = (emuptr) EmPalmHeap::DerefHandle (h) - heap->ChunkHeaderSize ();

			EmPalmChunk	chunk (*heap, p);

			if (chunk.LockCount () == 1)
			{
				MetaMemory::UnregisterBitmapPointer ((MemPtr) p);
			}
		}
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrHeadpatch::MemPtrUnlock
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType MemMgrHeadpatch::MemPtrUnlock (void)
{
	// Err MemPtrUnlock(MemPtr p) 

	EmPatchState::EnterMemMgr ("MemPtrUnlock");

	CALLED_SETUP ("Err", "MemPtr p");

	CALLED_GET_PARAM_VAL (MemPtr, p);

	// If this handle was for a bitmap resource, forget the pointer
	// we registered earlier.

	if (MetaMemory::IsBitmapPointer (p))
	{
		MetaMemory::UnregisterBitmapPointer (p);
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrHeadpatch::MemSemaphoreRelease
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType MemMgrHeadpatch::MemSemaphoreRelease (void)
{
	// Err MemSemaphoreRelease (Boolean writeAccess)

	EmPatchState::EnterMemMgr ("MemSemaphoreRelease");

	CALLED_SETUP ("Err", "Boolean writeAccess");

	CALLED_GET_PARAM_VAL (Boolean, writeAccess);

	if (writeAccess
		&& --EmPatchState::fgData.fMemSemaphoreCount == 0
		&& EmPatchState::HasWellBehavedMemSemaphoreUsage ())
	{
		// Amount of time to allow before warning that the memory manager
		// semaphore has been held too long.  Note that this used to be
		// 60 seconds.  However, at current emulation speeds and the rate
		// at which the tickcount is incremented, it seems that the
		// Memory Manager itself can hold onto the semaphore longer than
		// this threshold (in really, really full heaps when calling
		// MemChunkNew, for example).  So let's double it.

		const unsigned long	kMaxElapsedSeconds	= 120 /* seconds */ * 100 /* ticks per second */;

		CEnableFullAccess	munge;

		unsigned long	curCount		= EmLowMem_GetGlobal (hwrCurTicks);
		unsigned long	elapsedTicks	= curCount - EmPatchState::fgData.fMemSemaphoreReserveTime;

		if (elapsedTicks > kMaxElapsedSeconds)
		{
			EmAssert (gSession);
			gSession->ScheduleDeferredError (new EmDeferredErrMemMgrSemaphore);
		}
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrHeadpatch::MemSemaphoreReserve
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType MemMgrHeadpatch::MemSemaphoreReserve (void)
{
	// Err MemSemaphoreReserve (Boolean writeAccess)

	EmPatchState::EnterMemMgr ("MemSemaphoreReserve");

	CALLED_SETUP ("Err", "Boolean writeAccess");

	CALLED_GET_PARAM_VAL (Boolean, writeAccess);

	if (writeAccess
		&& EmPatchState::fgData.fMemSemaphoreCount++ == 0
		&& EmPatchState::HasWellBehavedMemSemaphoreUsage ())
	{
		CEnableFullAccess	munge;
		EmPatchState::fgData.fMemSemaphoreReserveTime = EmLowMem_GetGlobal (hwrCurTicks);
	}

	return kExecuteROM;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemChunkFree
 *
 * DESCRIPTION:	If the user wants disposed blocks to be filled with a
 *				special value, handle that here.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemChunkFree (void)
{
	// Err MemChunkFree(MemPtr chunkDataP)

	CALLED_SETUP ("Err", "MemPtr p");

	CALLED_GET_PARAM_VAL (MemPtr, p);

	EmPalmHeap*	heap = ::PrvGetRememberedHeap ((emuptr) (MemPtr) p);

	EmPalmChunkList	delta;
	EmPalmHeap::MemChunkFree (heap, &delta);
	MetaMemory::Resync (delta);

	{
		CEnableFullAccess	munge;

		emuptr	w = EmLowMem_GetGlobal (uiGlobals.firstWindow);
		if (w == (emuptr) (MemPtr) p)
		{
			EmPatchState::SetUIReset (false);
		}
	}

	EmPatchState::ExitMemMgr ("MemChunkFree");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemChunkNew
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemChunkNew (void)
{
	// MemPtr MemChunkNew(UInt16 heapID, UInt32 size, UInt16 attr)

	CALLED_SETUP ("MemPtr", "UInt16 heapID, UInt32 size, UInt16 attr");

	CALLED_GET_PARAM_VAL (UInt16, heapID);
//	CALLED_GET_PARAM_VAL (UInt32, size);
	CALLED_GET_PARAM_VAL (UInt16, attr);
	GET_RESULT_PTR ();

#ifdef FILL_BLOCKS

	if (gPrefs->FillNewBlocks ())
	{
		if (c)
		{

			if (attr & memNewChunkFlagNonMovable)
			{
				CEnableFullAccess	munge;
				uae_memset (c, kChunkNewValue, size);
			}
			else
			{
				emuptr	p = (emuptr) ::MemHandleLock ((MemHandle) c);
				if (p)
				{
					{
						CEnableFullAccess	munge;
						uae_memset (p, kChunkNewValue, size);
					}

					::MemHandleUnlock ((MemHandle) c);
				}
			}
		}
	}

#endif

	EmPalmChunkList	delta;
	EmPalmHeap::MemChunkNew (heapID, (MemPtr) result, attr, &delta);
	MetaMemory::Resync (delta);

#define TRACK_BOOT_ALLOCATION 0
#if TRACK_BOOT_ALLOCATION
	if (attr & memNewChunkFlagNonMovable)
	{
		c = EmPalmHeap::DerefHandle (c);
	}

	const EmPalmHeap*	heap = EmPalmHeap::GetHeapByPtr ((MemPtr) result);
	if (heap)
	{
		const EmPalmChunk*	chunk = heap->GetChunkContaining (result);
		if (chunk)
		{
			LogAppendMsg ("Chunk allocated, loc = 0x%08X, size = 0x%08X",
				(int) chunk->Start (), (int) chunk->Size ());
		}
	}
#endif

	if (attr & memNewChunkFlagNonMovable)
	{
		::PrvRememberChunk (result);
	}

	else if (attr & memNewChunkFlagPreLock)
	{
		MemHandle	h = EmPalmHeap::RecoverHandle ((MemPtr) result);
		::PrvRememberHandle ((emuptr) h);
	}
	else
	{
		::PrvRememberHandle (result);
	}

	EmPatchState::ExitMemMgr ("MemChunkNew");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHandleFree
 *
 * DESCRIPTION:	If the user wants disposed blocks to be filled with a
 *				special value, handle that here.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHandleFree (void)
{
	// Err MemHandleFree(MemHandle h)

	CALLED_SETUP ("Err", "MemHandle h");

	CALLED_GET_PARAM_VAL (MemHandle, h);

	EmPalmHeap*	heap = ::PrvGetRememberedHeap ((emuptr) (MemHandle) h);

	EmPalmChunkList	delta;
	EmPalmHeap::MemHandleFree (heap, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemHandleFree");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHandleNew
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHandleNew (void)
{
	// MemHandle MemHandleNew(UInt32 size)

	CALLED_SETUP ("MemHandle", "UInt32 size");

	GET_RESULT_PTR ();


#ifdef FILL_BLOCKS

	if (result)
	{
		if (size && gPrefs->FillNewBlocks ())
		{
			emuptr	p = (emuptr) ::MemHandleLock ((MemHandle) result);
			if (p)
			{
				{
					CEnableFullAccess	munge;
					uae_memset (p, kHandleNewValue, size);
				}

				::MemHandleUnlock ((MemHandle) result);
			}
		}
	}

#endif

	EmPalmChunkList	delta;
	EmPalmHeap::MemHandleNew ((MemHandle) result, &delta);
	MetaMemory::Resync (delta);

#if TRACK_BOOT_ALLOCATION
	emuptr	c;

	if (1)
	{
		c = EmPalmHeap::DerefHandle (result);
	}

	const EmPalmHeap*	heap = EmPalmHeap::GetHeapByPtr ((MemPtr) c);
	if (heap)
	{
		const EmPalmChunk*	chunk = heap->GetChunkContaining (c);
		if (chunk)
		{
			LogAppendMsg ("Handle allocated, loc = 0x%08X, size = 0x%08X",
				(int) chunk->Start (), (int) chunk->Size ());
		}
	}
#endif

	::PrvRememberHandle (result);

	EmPatchState::ExitMemMgr ("MemHandleNew");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHandleLock
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHandleLock (void)
{
	// MemPtr MemHandleLock(MemHandle h) 

	CALLED_SETUP ("MemPtr", "MemHandle h");

	CALLED_GET_PARAM_VAL (MemHandle, h);

	EmPalmChunkList	delta;
	EmPalmHeap::MemHandleLock ((MemHandle) h, &delta);
	MetaMemory::Resync (delta);

	// If this handle was for a bitmap resource, remember the pointer
	// for fast checking later.

	if (MetaMemory::IsBitmapHandle (h))
	{
		GET_RESULT_PTR ();
		MetaMemory::RegisterBitmapPointer ((MemPtr) result);
	}

	EmPatchState::ExitMemMgr ("MemHandleLock");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHandleResetLock
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHandleResetLock (void)
{
	// Err MemHandleResetLock(MemHandle h) 

	CALLED_SETUP ("Err", "MemHandle h");

	CALLED_GET_PARAM_VAL (MemHandle, h);

	EmPalmChunkList	delta;
	EmPalmHeap::MemHandleResetLock ((MemHandle) h, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemHandleResetLock");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHandleResize
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHandleResize (void)
{
	// Err MemHandleResize(MemHandle h,  UInt32 newSize) 

	CALLED_SETUP ("Err", "MemHandle h,  UInt32 newSize");

	CALLED_GET_PARAM_VAL (MemHandle, h);

#ifdef FILL_BLOCKS

	if (h)
	{
		if (newSize > gResizeOrigSize && gPrefs->FillResizedBlocks ())
		{
			emuptr	p = (emuptr) ::MemHandleLock ((MemHandle) h);
			if (p)
			{
				{
					CEnableFullAccess	munge;
					uae_memset (p + gResizeOrigSize, kHandleResizeValue, newSize - gResizeOrigSize);
				}

				::MemHandleUnlock ((MemHandle) h);
			}
		}
	}

#endif

	EmPalmChunkList	delta;
	EmPalmHeap::MemHandleResize ((MemHandle) h, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemHandleResize");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHandleUnlock
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHandleUnlock (void)
{
	// Err MemHandleUnlock(MemHandle h) 

	CALLED_SETUP ("Err", "MemHandle h");

	CALLED_GET_PARAM_VAL (MemHandle, h);

	EmPalmChunkList	delta;
	EmPalmHeap::MemHandleUnlock ((MemHandle) h, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemHandleUnlock");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHeapCompact
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHeapCompact (void)
{
	// Err MemHeapCompact(UInt16 heapID)

	CALLED_SETUP ("Err", "UInt16 heapID");

	CALLED_GET_PARAM_VAL (UInt16, heapID);

	EmPalmChunkList	delta;
	EmPalmHeap::MemHeapCompact (heapID, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemHeapCompact");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHeapFreeByOwnerID
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

//	!!! This should probably walk the heap itself so that it can just mark
//	the chunks that are about to be freed.

void MemMgrTailpatch::MemHeapFreeByOwnerID (void)
{
	// Err MemHeapFreeByOwnerID(UInt16 heapID, UInt16 ownerID)

	CALLED_SETUP ("Err", "UInt16 heapID, UInt16 ownerID");

	CALLED_GET_PARAM_VAL (UInt16, heapID);
	CALLED_GET_PARAM_VAL (UInt16, ownerID);

	EmPalmChunkList	delta;
	EmPalmHeap::MemHeapFreeByOwnerID (heapID, ownerID, &delta);
	MetaMemory::Resync (delta);

	::PrvForgetAll (heapID, ownerID);

	EmPatchState::ExitMemMgr ("MemHeapFreeByOwnerID");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemInitHeapTable
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemInitHeapTable (void)
{
	// Err MemInitHeapTable(UInt16 cardNo)

	CALLED_SETUP ("Err", "UInt16 cardNo");

	CALLED_GET_PARAM_VAL (UInt16, cardNo);

	EmPalmHeap::MemInitHeapTable (cardNo);

	EmPatchState::ExitMemMgr ("MemInitHeapTable");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHeapInit
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHeapInit (void)
{
	// Err MemHeapInit(UInt16 heapID, Int16 numHandles, Boolean initContents)

	CALLED_SETUP ("Err", "UInt16 heapID, Int16 numHandles, Boolean initContents");

	CALLED_GET_PARAM_VAL (UInt16, heapID);
	CALLED_GET_PARAM_VAL (Int16, numHandles);
	CALLED_GET_PARAM_VAL (Boolean, initContents);

	EmPalmHeap::MemHeapInit (heapID, numHandles, initContents);

	EmPatchState::ExitMemMgr ("MemHeapInit");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemHeapScramble
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemHeapScramble (void)
{
	// Err MemHeapScramble(UInt16 heapID)

	CALLED_SETUP ("Err", "UInt16 heapID");

	CALLED_GET_PARAM_VAL (UInt16, heapID);

	EmPalmChunkList	delta;
	EmPalmHeap::MemHeapScramble (heapID, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemHeapScramble");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemKernelInit
 *
 * DESCRIPTION:	MemKernelInit is called just after AMXDriversInit, which
 *				is where the exception vectors are set up.  After those
 *				vectors are installed, there really shouldn't be any
 *				more memory accesses to that range of memory. Thus,
 *				memory access there is flagged as invalid.
 *
 *				While we're here, let's mark some other memory
 *				locations, too.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemKernelInit (void)
{
	MetaMemory::MarkLowMemory (		MetaMemory::GetLowMemoryBegin (),
									MetaMemory::GetLowMemoryEnd ());
	MetaMemory::MarkSystemGlobals (	MetaMemory::GetSysGlobalsBegin (),
									MetaMemory::GetSysGlobalsEnd ());
	MetaMemory::MarkHeapHeader (	MetaMemory::GetHeapHdrBegin (0),
									MetaMemory::GetHeapHdrEnd (0));

	// On the Visor, these are fair game.
	MetaMemory::MarkSystemGlobals (offsetof (M68KExcTableType, unassigned[12]),
								 offsetof (M68KExcTableType, unassigned[16]));

	EmPatchState::ExitMemMgr ("MemKernelInit");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemLocalIDToLockedPtr
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemLocalIDToLockedPtr (void)
{
	// MemPtr MemLocalIDToLockedPtr(LocalID local,  UInt16 cardNo)

	CALLED_SETUP ("MemPtr", "LocalID local,  UInt16 cardNo");

	GET_RESULT_PTR ();

	EmPalmChunkList	delta;
	EmPalmHeap::MemLocalIDToLockedPtr ((MemPtr) result, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemLocalIDToLockedPtr");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemPtrNew
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemPtrNew (void)
{
	// MemPtr MemPtrNew(UInt32 size) 

	CALLED_SETUP ("MemPtr", "UInt32 size");

	GET_RESULT_PTR ();

#ifdef FILL_BLOCKS

	if (result)
	{
		if (gPrefs->FillNewBlocks ())
		{
			CEnableFullAccess	munge;
			uae_memset (result, kPtrNewValue, size);
		}
	}

#endif

	EmPalmChunkList	delta;
	EmPalmHeap::MemPtrNew ((MemPtr) result, &delta);
	MetaMemory::Resync (delta);

	::PrvRememberChunk (result);

	EmPatchState::ExitMemMgr ("MemPtrNew");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemPtrResetLock
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemPtrResetLock (void)
{
	// Err MemPtrResetLock(MemPtr p) 

	CALLED_SETUP ("Err", "MemPtr p");

	CALLED_GET_PARAM_VAL (MemPtr, p);

	EmPalmChunkList	delta;
	EmPalmHeap::MemPtrResetLock (p, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemPtrResetLock");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemPtrResize
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemPtrSetOwner (void)
{
	// Err MemPtrSetOwner(MemPtr chunkDataP, UInt8 owner)

	CALLED_SETUP ("Err", "MemPtr p, UInt16 owner");

	CALLED_GET_PARAM_VAL (MemPtr, p);
//	CALLED_GET_PARAM_VAL (UInt16, owner);

	EmPalmChunkList	delta;
	EmPalmHeap::MemPtrSetOwner (p, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemPtrSetOwner");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemPtrResize
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemPtrResize (void)
{
	// Err MemPtrResize(MemPtr p, UInt32 newSize)

	CALLED_SETUP ("Err", "MemPtr p, UInt32 newSize");

	CALLED_GET_PARAM_VAL (MemPtr, p);

#ifdef FILL_BLOCKS

	if (p)
	{
		if (newSize > gResizeOrigSize && gPrefs->FillResizedBlocks ())
		{
			CEnableFullAccess	munge;
			uae_memset (p + gResizeOrigSize, kPtrResizeValue, newSize - gResizeOrigSize);
		}
	}

#endif

	EmPalmChunkList	delta;
	EmPalmHeap::MemPtrResize (p, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemPtrResize");
}


/***********************************************************************
 *
 * FUNCTION:	MemMgrTailpatch::MemPtrUnlock
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void MemMgrTailpatch::MemPtrUnlock (void)
{
	// Err MemPtrUnlock(MemPtr p) 

	CALLED_SETUP ("Err", "MemPtr p");

	CALLED_GET_PARAM_VAL (MemPtr, p);

	EmPalmChunkList	delta;
	EmPalmHeap::MemPtrUnlock (p, &delta);
	MetaMemory::Resync (delta);

	EmPatchState::ExitMemMgr ("MemPtrUnlock");
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	PrvRememberHeapAndPtr
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void PrvRememberHeapAndPtr (EmPalmHeap* h, emuptr p)
{
	EmAssert (EmPatchState::fgData.fRememberedHeaps.find (p) == EmPatchState::fgData.fRememberedHeaps.end ());

	EmPatchState::fgData.fRememberedHeaps[p] = h;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetRememberedHeap
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

EmPalmHeap* PrvGetRememberedHeap (emuptr p)
{
	EmPalmHeap*	result = NULL;

	map<emuptr, EmPalmHeap*>::iterator	iter = EmPatchState::fgData.fRememberedHeaps.find (p);
	if (iter != EmPatchState::fgData.fRememberedHeaps.end ())
	{
		result = iter->second;
		EmPatchState::fgData.fRememberedHeaps.erase (iter);
	}

	EmAssert (result);

	return result;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	PrvRememberChunk
 *
 * DESCRIPTION:	Remember the context in which the given pointer was
 *				allocated.  Called after MemPtrNew or MemChunkNew
 *				completes.
 *
 * PARAMETERS:	p - pointer returned from MemPtrNew or MemChunkNew.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void PrvRememberChunk (emuptr p)
{
	//
	// Track only allocations in the dynamic heap.
	//

	const EmPalmHeap*	heap = EmPalmHeap::GetHeapByPtr (p);

	if (!heap || heap->HeapID () != 0)
		return;

#ifdef _DEBUG

	//
	// Make sure that this pointer is not a handle in disguise.
	//

	MemHandle	h = EmPalmHeap::RecoverHandle ((MemPtr) p);

	if (h)
	{
		EmAssert (false);
	}

	//
	// Ensure this pointer is not already on our list.
	//

	EmTrackedChunkList::iterator	iter = EmPatchState::fgData.fTrackedChunks.begin ();
	while (iter != EmPatchState::fgData.fTrackedChunks.end ())
	{
		//
		// If we have recorded a handle, make sure that our handle
		// does not reference this newly allocated chunk.  That would
		// indicate that we have a stale handle in our list.
		//

		if (iter->isHandle)
		{
			emuptr	hDereffed = (emuptr) EmPalmHeap::DerefHandle ((MemHandle) iter->ptr);
			if (hDereffed == p)
			{
				EmAssert (false);
			}
		}

		//
		// If we have recorded a pointer, make sure that it doesn't
		// also point to the newly allocated chunk.  That would indicate
		// that we have a stale pointer in our list.
		//

		else
		{
			if (iter->ptr == p)
			{
				EmAssert (false);
			}
		}

		++iter;
	}
#endif

	//
	// Push an empty record onto our list.  We'll update it after
	// it's on the list in order to reduce any performance impact
	// due to copying the stack crawl.  Push the chunk at the front
	// of the list; the theory here is that recently added chunks
	// will be the ones most likely to be removed later, so adding
	// them to the front will speed up the removal process when
	// performing a forward traversal.
	//

	EmTrackedChunk	chunk;
	EmPatchState::fgData.fTrackedChunks.push_front (chunk);

	//
	// Get the pointer to the newly-added record.
	//

	EmTrackedChunkList::reference	newChunk = EmPatchState::fgData.fTrackedChunks.front ();

	//
	// Fill in the newly added record.
	//

	newChunk.ptr		= p;
	newChunk.isHandle	= false;

	EmPalmOS::GenerateStackCrawl (newChunk.stackCrawl);
}


/***********************************************************************
 *
 * FUNCTION:	PrvRememberHandle
 *
 * DESCRIPTION:	Remember the context in which the given handle was
 *				allocated.  Called after MemHandleNew or MemChunkNew
 *				completes.
 *
 * PARAMETERS:	h - handle returned from MemHandleNew or MemChunkNew.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void PrvRememberHandle (emuptr h)
{
	//
	// Track only allocations in the dynamic heap.
	//

	const EmPalmHeap*	heap = EmPalmHeap::GetHeapByHdl ((MemHandle) h);

	if (!heap || heap->HeapID () != 0)
		return;

#ifdef _DEBUG
	//
	// Ensure this handle is not already on our list.
	//

	EmTrackedChunkList::iterator	iter = EmPatchState::fgData.fTrackedChunks.begin ();
	while (iter != EmPatchState::fgData.fTrackedChunks.end ())
	{
		//
		// If we have recorded a handle, make sure it does not
		// match the newly allocated handle.  That would indicate
		// that we have a stale handle in our list.
		//

		if (iter->isHandle)
		{
			if (iter->ptr == h)
			{
				EmAssert (false);
			}
		}

		//
		// If we have recorded a pointer, make sure it is not
		// pointed to by the newly allocate handle.  That would
		// indicate that we have a stale pointer in our list.
		//

		else
		{
			emuptr hDereffed = (emuptr) EmPalmHeap::DerefHandle ((MemHandle) h);

			if (iter->ptr == hDereffed)
			{
				EmAssert (false);
			}
		}

		++iter;
	}
#endif

	//
	// Push an empty record onto our list.  We'll update it after
	// it's on the list in order to reduce any performance impact
	// due to copying the stack crawl.  Push the handle at the front
	// of the list; the theory here is that recently added chunks
	// will be the ones most likely to be removed later, so adding
	// them to the front will speed up the removal process when
	// performing a forward traversal.
	//

	EmTrackedChunk	chunk;
	EmPatchState::fgData.fTrackedChunks.push_front (chunk);

	//
	// Get the pointer to the newly-added record.
	//

	EmTrackedChunkList::reference	newChunk = EmPatchState::fgData.fTrackedChunks.front ();

	//
	// Fill in the newly added record.
	//

	newChunk.ptr		= h;
	newChunk.isHandle	= true;

	EmPalmOS::GenerateStackCrawl (newChunk.stackCrawl);
}


/***********************************************************************
 *
 * FUNCTION:	PrvForgetChunk
 *
 * DESCRIPTION:	Forget about the given chunk.  Also checks for pointers
 *				to relocatable blocks and calls PrvForgetHandle instead
 *				if appropriate.  Called in response to MemChunkFree.
 *
 * PARAMETERS:	p - handle passed to MemChunkFree.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void PrvForgetChunk (emuptr p)
{
	//
	// Deal with the goofwads who allocate by handle but
	// dispose by pointer.
	//

	MemHandle	h = EmPalmHeap::RecoverHandle ((MemPtr) p);

	if (h)
	{
		::PrvForgetHandle ((emuptr) h);
		return;
	}

	//
	// Look for the given chunk in our records.
	//

	EmTrackedChunkList::iterator	iter = EmPatchState::fgData.fTrackedChunks.begin ();
	while (iter != EmPatchState::fgData.fTrackedChunks.end ())
	{
		if (!iter->isHandle && iter->ptr == p)
		{
			//
			// If we've found it, erase it from our list and return.
			//

			EmPatchState::fgData.fTrackedChunks.erase (iter);
			return;
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvForgetHandle
 *
 * DESCRIPTION:	Forget about the given handle.  Called in response to
 *				MemHandleFree.
 *
 * PARAMETERS:	h - handle passed to MemHandleFree.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void PrvForgetHandle (emuptr h)
{
	//
	// Look for the given handle in our records.
	//

	EmTrackedChunkList::iterator	iter = EmPatchState::fgData.fTrackedChunks.begin ();
	while (iter != EmPatchState::fgData.fTrackedChunks.end ())
	{
		if (iter->isHandle && iter->ptr == h)
		{
			//
			// If we've found it, erase it from our list and return.
			//

			EmPatchState::fgData.fTrackedChunks.erase (iter);
			return;
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvForgetAll
 *
 * DESCRIPTION:	Forget about all allocated chunks in the given heap
 *				with the given owner ID.  Called in response to
 *				MemHeapFreeByOwnerID.
 *
 * PARAMETERS:	heapID - heap ID passed to MemHeapFreeByOwnerID.
 *
 *				ownerID - owner ID passed to MemHeapFreeByOwnerID.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void PrvForgetAll (UInt16 heapID, UInt16 ownerID)
{
	EmTrackedChunkList::iterator	iter = EmPatchState::fgData.fTrackedChunks.begin ();
	while (iter != EmPatchState::fgData.fTrackedChunks.end ())
	{
		const EmPalmHeap*	heap	= NULL;
		const EmPalmChunk*	chunk	= NULL;

		if (iter->isHandle)
		{
			heap = EmPalmHeap::GetHeapByHdl ((MemHandle) iter->ptr);

			if (heap && heap->HeapID () == heapID)
			{
				chunk = heap->GetChunkReferencedBy ((MemHandle) iter->ptr);
			}
		}
		else
		{
			heap = EmPalmHeap::GetHeapByPtr (iter->ptr);

			if (heap && heap->HeapID () == heapID)
			{
				chunk = heap->GetChunkBodyContaining (iter->ptr);
			}
		}

		if (chunk && chunk->Owner () == ownerID)
		{
			iter = EmPatchState::fgData.fTrackedChunks.erase (iter);
		}
		else
		{
			if (chunk)
				EmAssert (!chunk->Free ());

			++iter;
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvCountLeaks
 *
 * DESCRIPTION:	Find and report memory leaks.  Iterates over the
 *				allocated chunks, looking for ones with the given
 *				owner ID.
 *
 * PARAMETERS:	ownerID - owner ID of the application quitting.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

int PrvCountLeaks (UInt16 ownerID)
{
	int	leaks = 0;

	EmTrackedChunkList::iterator	iter = EmPatchState::fgData.fTrackedChunks.begin ();
	while (iter != EmPatchState::fgData.fTrackedChunks.end ())
	{
		const EmPalmHeap*	heap	= NULL;
		const EmPalmChunk*	chunk	= NULL;

		if (iter->isHandle)
		{
			heap = EmPalmHeap::GetHeapByHdl ((MemHandle) iter->ptr);

			if (heap)
			{
				chunk = heap->GetChunkReferencedBy ((MemHandle) iter->ptr);
			}
		}
		else
		{
			heap = EmPalmHeap::GetHeapByPtr (iter->ptr);

			if (heap)
			{
				chunk = heap->GetChunkBodyContaining (iter->ptr);
			}
		}

		if (chunk && chunk->Owner () == ownerID)
		{
			//
			// If this "leak" is not caused by the OS, count it.
			//

			if (!::PrvLeakException (*chunk))
			{
				++leaks;
			}
		}

		++iter;
	}

	return leaks;
}


/***********************************************************************
 *
 * FUNCTION:	PrvReportMemoryLeaks
 *
 * DESCRIPTION:	Find and report memory leaks.  Iterates over the
 *				allocated chunks, looking for ones with the given
 *				owner ID.
 *
 * PARAMETERS:	ownerID - owner ID of the application quitting.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void PrvReportMemoryLeaks (UInt16 ownerID)
{
	EmTrackedChunkList::iterator	iter = EmPatchState::fgData.fTrackedChunks.begin ();
	while (iter != EmPatchState::fgData.fTrackedChunks.end ())
	{
		const EmPalmHeap*	heap	= NULL;
		const EmPalmChunk*	chunk	= NULL;

		if (iter->isHandle)
		{
			heap = EmPalmHeap::GetHeapByHdl ((MemHandle) iter->ptr);

			if (heap)
			{
				chunk = heap->GetChunkReferencedBy ((MemHandle) iter->ptr);
			}
		}
		else
		{
			heap = EmPalmHeap::GetHeapByPtr (iter->ptr);

			if (heap)
			{
				chunk = heap->GetChunkBodyContaining (iter->ptr);
			}
		}

		if (chunk && chunk->Owner () == ownerID)
		{
			//
			// If this "leak" is not caused by the OS, report it.
			//

			if (!::PrvLeakException (*chunk))
			{
				if (iter != EmPatchState::fgData.fTrackedChunks.begin ())
				{
					LogAppendMsg ("--------------------------------------------------------");
				}

				::PrvReportLeakedChunk (*iter, *chunk);
			}
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvReportLeakedChunk
 *
 * DESCRIPTION:	Report information on the leaked chunk:
 *
 *					* Chunk address and size
 *					* Stack crawl of the context allocating the chunk
 *					* Contents of the chunk (truncated to 256 bytes).
 *
 * PARAMETERS:	tracked - context information about the chunk.
 *
 *				chunk - information on the leaked chunk.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void PrvReportLeakedChunk (const EmTrackedChunk& tracked, const EmPalmChunk& chunk)
{
	//
	// Generate a full stack crawl.
	//

	StringList	stackCrawlFunctions;

	EmStackFrameList::const_iterator	iter = tracked.stackCrawl.begin ();
	while (iter != tracked.stackCrawl.end ())
	{
		// Get the function name.

		char	funcName[256] = {0};
		::FindFunctionName (iter->fAddressInFunction, funcName, NULL, NULL, 255);

		// If we can't find the name, dummy one up.

		if (strlen (funcName) == 0)
		{
			sprintf (funcName, "<Unknown @ 0x%08lX>", iter->fAddressInFunction);
		}

		stackCrawlFunctions.push_back (string (funcName));

		++iter;
	}

	//
	// Get some data to dump
	//

	uint8	buffer[256];
	uint32	len = chunk.BodySize ();

	if (len > countof (buffer))
	{
		len = countof (buffer);
	}

	EmMem_memcpy ((void*) buffer, chunk.BodyStart (), len);

	//
	// Print the message.
	//
	//	* Chunk address and size
	//	* Stack crawl of the context allocating the chunk
	//	* Contents of the chunk (truncated to 256 bytes).
	//

	LogAppendMsg ("%s chunk leaked at 0x%08X, size = %ld",
		tracked.isHandle ? "Relocatable" : "Non-relocatable",
		chunk.BodyStart (), chunk.BodySize ());

	LogAppendMsg ("Chunk allocated by:");

	StringList::iterator	iter2 = stackCrawlFunctions.begin ();
	while (iter2 != stackCrawlFunctions.end ())
	{
		LogAppendMsg ("\t%s", iter2->c_str ());

		++iter2;
	}

	if (chunk.BodySize () <= countof (buffer))
		LogAppendData (buffer, len, "Chunk contents:");
	else
		LogAppendData (buffer, len, "Chunk contents (first %d bytes only):", countof (buffer));
}


/***********************************************************************
 *
 * FUNCTION:	PrvLeakException
 *
 * DESCRIPTION:	Return whether or not the given memory chunk should be
 *				quietly allowed as a leak.
 *
 * PARAMETERS:	chunk - information on the chunk to be tested.
 *
 * RETURNED:	TRUE if the chunk -- even though leaked -- should not
 *				be reported as a leak.  FALSE otherwise.
 *
 ***********************************************************************/

Bool PrvLeakException (const EmPalmChunk& chunk)
{
	// Up to Palm OS 3.2, UIReset (called from the application's
	// startup code) allocated the event queue and undo buffers
	// using the application's chunk ID.  In Palm OS 3.2, this
	// allocation was moved to UIInitialize, causing the blocks
	// to be marked with the system's chunk ID.
	//
	// At the same time, the call to InitWindowVariables (which
	// allocated the RootWindow) was moved from UIReset to
	// UIInitialize.

	if (EmPatchState::OSMajorMinorVersion () >= 32)
	{
		// Nothing
	}
	else if (EmPatchState::OSMajorMinorVersion () >= 31)
	{
		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV31.displayWindow))
			return true;

		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV31.eventQ))
			return true;

		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV31.undoGlobals.buffer))
			return true;
	}
	else if (EmPatchState::OSMajorMinorVersion () >= 30)
	{
		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV3.displayWindow))
			return true;

		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV3.eventQ))
			return true;

		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV3.undoGlobals.buffer))
			return true;
	}
	else if (EmPatchState::OSMajorMinorVersion () >= 20)
	{
		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV2.displayWindow))
			return true;

		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV2.eventQ))
			return true;

		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV2.undoGlobals.buffer))
			return true;
	}
	else	// OSMajorMinorVersion < 20
	{
		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV1.displayWindow))
			return true;

		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV1.eventQ))
			return true;

		if (chunk.BodyStart () == EmLowMem_GetGlobal (uiGlobalsV1.undoGlobals.buffer))
			return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	CheckForMemoryLeaks
 *
 * DESCRIPTION:	Entry point for checking for and reporting memory leaks.
 *				Called from SysAppExit patch.
 *
 * PARAMETERS:	memOwnerID - memory owner ID of the function quitting.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void CheckForMemoryLeaks (UInt32 memOwnerID)
{
	if (!EmPatchState::fgData.fMemMgrLeaks)
		return;

	CEnableFullAccess	munge;

	int	leaks = ::PrvCountLeaks (memOwnerID);

	if (!leaks)
		return;

	//
	// Found some leaks, so report them.
	//

	LogAppendMsg ("WARNING: ========================================================");
	LogAppendMsg ("WARNING: Memory Leaks");
	LogAppendMsg ("WARNING: ========================================================");
	LogAppendMsg ("WARNING: Begin Memory Leak Dump");
	LogAppendMsg ("WARNING: ========================================================");

	::PrvReportMemoryLeaks (memOwnerID);

	LogAppendMsg ("WARNING: ========================================================");
	LogAppendMsg ("WARNING: End Memory Leak Dump");
	LogAppendMsg ("WARNING: ========================================================");

	LogDump ();

	// Now tell the user about them.

	Errors::ReportErrMemMgrLeaks (leaks);
}
