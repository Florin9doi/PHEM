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

#ifndef EmPatchModuleSys_h
#define EmPatchModuleSys_h

#include "EmPatchModule.h"

class EmPatchModuleSys : public EmPatchModule
{
	public:
		EmPatchModuleSys();
		virtual ~EmPatchModuleSys() {}
};


// ======================================================================
// PatchModule for system functions.
// ======================================================================

class SysHeadpatch
{
	public:
		static CallROMType		UnmarkUIObjects			(void); // CtlNewControl, FldNewField, FrmInitForm, FrmNewBitmap, FrmNewGadget, FrmNewGsi, FrmNewLabel, LstNewList, WinAddWindow, WinRemoveWindow
		static CallROMType		RecordTrapNumber		(void); // EvtGetEvent & EvtGetPen

		static CallROMType		ClipboardGetItem		(void);
		static CallROMType		DbgMessage				(void);
		static CallROMType		DmCloseDatabase			(void);
		static CallROMType		DmInit					(void);
		static CallROMType		ErrDisplayFileLineMsg	(void);
		static CallROMType		EvtAddEventToQueue		(void);
		static CallROMType		EvtAddUniqueEventToQueue(void);
		static CallROMType		EvtEnqueueKey			(void);
		static CallROMType		EvtEnqueuePenPoint		(void);
		static CallROMType		ExgDoDialog				(void);
		static CallROMType		FrmCustomAlert			(void);
		static CallROMType		FrmDrawForm				(void);
		static CallROMType		FrmPopupForm			(void);
		static CallROMType		HostControl 			(void);
		static CallROMType		HwrBatteryLevel 		(void);
		static CallROMType		HwrBattery		 		(void);
		static CallROMType		HwrDockStatus			(void);
		static CallROMType		HwrGetROMToken			(void);
		static CallROMType		HwrSleep				(void);
		static CallROMType		PenOpen 				(void);
		static CallROMType		PrefSetAppPreferences	(void);
		static CallROMType		SndDoCmd				(void);
		static CallROMType		SysAppExit				(void);
		static CallROMType		SysAppLaunch			(void);
		static CallROMType		SysBinarySearch 		(void);
		static CallROMType		SysEvGroupWait			(void);
		static CallROMType		SysFatalAlert			(void);
		static CallROMType		SysLaunchConsole		(void);
		static CallROMType		SysSemaphoreWait		(void);
		static CallROMType		SysTicksPerSecond		(void);
		static CallROMType		SysUIAppSwitch			(void);
		static CallROMType		TblHandleEvent			(void);
};

class SysTailpatch
{
	public:
		static void		MarkUIObjects			(void); // 	CtlNewControl, FldNewField, FrmInitForm, FrmNewBitmap, FrmNewGadget, FrmNewGsi, FrmNewLabel, LstNewList, WinAddWindow, WinRemoveWindow

		static void		BmpCreate				(void);
		static void		BmpDelete				(void);
		static void		ClipboardAddItem		(void);
		static void		ClipboardAppendItem		(void);
		static void		DmGet1Resource			(void);
		static void		DmGetResource			(void);
		static void 	EvtGetEvent 			(void);
		static void 	EvtGetPen				(void);
		static void 	EvtGetSysEvent			(void);
		static void		EvtSysEventAvail		(void);
		static void		ExgReceive				(void);
		static void		ExgSend					(void);
		static void 	FtrInit 				(void);
		static void 	FtrSet	 				(void);
		static void 	HwrMemReadable			(void);
		static void 	HwrSleep				(void);
		static void 	SysAppStartup			(void);
		static void 	SysBinarySearch 		(void);
		static void		SysTaskCreate			(void);
		static void 	TblHandleEvent	 		(void);
		static void 	TimInit 				(void);
		static void 	UIInitialize			(void);
		static void 	UIReset					(void);
};





/* ===========================================================================
	The following macros are used to help us generate patches to all the
	Memory Manager functions.  We patch the entire Memory Manager in order
	to track when we are "in" the Memory Manager.  We want to know this so
	that we can extend special privileges to the Memory Manager when it
	accesses memory; the Memory Manager can touch more of RAM than any other
	executing code.
	
	Since our patches mostly just increment and decrement a counter, most of
	them are generated automatically with macros.  I call these "simple"
	head- and tailpatches.  However, we patch a few other the other Memory
	Manager functions for other reasons.  We can't let the macros generate
	simple patches for these functions and instead have to write them
	ourselves, making sure to perform the same incrementing and decrementing
	the simple functions would do.  I call these patches "complex" head- and
	tailpatches.
	
	Therefore, there are four "categories" of patchings: functions for which
	we want simple head- and tailpatches, functions for which we have a
	complex headpatch but need a simple tailpatch, functions for which we
	have a complex tailpatch but need a simple headpatch, and functions for
	which we already have complex head- and tailpatches and don't need
	simple patches.
	
	(Actually, there is a fifth category: Memory Manager functions that we
	don't want to treat as Memory Manager functions.  The functions in this
	category are MemSet, MemMove, and MemCmp.  We don't want to extend to
	these functions the same privileges that other Memory Manager functions
	get.)
	
	(And there's kind of a sixth category as well: non-Memory Manager
	functions that we want to treat as Memory Manager functions. For
	instance, DmWriteCheck grovels over Memory Manager data structures, so
	we have to give it Memory Manager function privileges. 
	PrvFindMemoryLeaks is also a function like this, but since it's not in
	the trap table, we can't patch it, and have to handle it differently. 
	See MetaMemory.cpp for this.)
	
	The macros below divide the set of Memory Manager functions into the
	four categories.  Any function on any of the four lists will
	automatically have generated for it declarations for head- and
	tailpatches.  Additionally, depending on which list it's on, it will
	optionally have simple head- and tailpatch functions generated for it.
=========================================================================== */
	
// List of functions for which we want to automatically generate simple
// head- and tailpatches.
//
// DmWriteCheck is on this list because it grovels over memory, making sure
// everything is OK before it writes to the storage heap.  ScrInit is on
// this list because it calls MemMove to move the contents of the temporary
// LCD buffer to the real one after it's been allocated; the source of this
// move is a faux buffer out in the middle of an unallocated block.

#define FOR_EACH_STUB_BOTHPATCH_FUNCTION(DO_TO_FUNCTION)	\
	DO_TO_FUNCTION(MemInit)					\
	DO_TO_FUNCTION(MemNumCards)				\
	DO_TO_FUNCTION(MemCardFormat)			\
	DO_TO_FUNCTION(MemCardInfo)				\
	DO_TO_FUNCTION(MemStoreInfo)			\
	DO_TO_FUNCTION(MemStoreSetInfo)			\
	DO_TO_FUNCTION(MemNumHeaps)				\
	DO_TO_FUNCTION(MemNumRAMHeaps)			\
	DO_TO_FUNCTION(MemHeapID)				\
	DO_TO_FUNCTION(MemHeapDynamic)			\
	DO_TO_FUNCTION(MemHeapFreeBytes)		\
	DO_TO_FUNCTION(MemHeapSize)				\
	DO_TO_FUNCTION(MemHeapFlags)			\
	DO_TO_FUNCTION(MemPtrRecoverHandle)		\
	DO_TO_FUNCTION(MemPtrFlags)				\
	DO_TO_FUNCTION(MemPtrSize)				\
	DO_TO_FUNCTION(MemPtrOwner)				\
	DO_TO_FUNCTION(MemPtrHeapID)			\
	DO_TO_FUNCTION(MemPtrDataStorage)		\
	DO_TO_FUNCTION(MemPtrCardNo)			\
	DO_TO_FUNCTION(MemPtrToLocalID)			\
	DO_TO_FUNCTION(MemHandleFlags)			\
	DO_TO_FUNCTION(MemHandleSize)			\
	DO_TO_FUNCTION(MemHandleOwner)			\
	DO_TO_FUNCTION(MemHandleLockCount)		\
	DO_TO_FUNCTION(MemHandleHeapID)			\
	DO_TO_FUNCTION(MemHandleDataStorage)	\
	DO_TO_FUNCTION(MemHandleCardNo)			\
	DO_TO_FUNCTION(MemHandleToLocalID)		\
	DO_TO_FUNCTION(MemHandleSetOwner)		\
	DO_TO_FUNCTION(MemLocalIDToGlobal)		\
	DO_TO_FUNCTION(MemLocalIDKind)			\
	DO_TO_FUNCTION(MemLocalIDToPtr)			\
	DO_TO_FUNCTION(MemDebugMode)			\
	DO_TO_FUNCTION(MemSetDebugMode)			\
	DO_TO_FUNCTION(MemHeapCheck)			\
	DO_TO_FUNCTION(MemHeapPtr)				\
	DO_TO_FUNCTION(MemStoreSearch)			\
	DO_TO_FUNCTION(MemStoreInit)			\
	DO_TO_FUNCTION(MemNVParams)				\
	DO_TO_FUNCTION(DmWriteCheck)			\
	DO_TO_FUNCTION(WinScreenInit)

// List of functions for which we want to automatically
// generate simple headpatches only.

#define FOR_EACH_STUB_HEADPATCH_FUNCTION(DO_TO_FUNCTION)	\
	DO_TO_FUNCTION(MemKernelInit)			\
	DO_TO_FUNCTION(MemInitHeapTable)		\
	DO_TO_FUNCTION(MemHeapInit)				\
	DO_TO_FUNCTION(MemHeapCompact)			\
	DO_TO_FUNCTION(MemHeapFreeByOwnerID)	\
	DO_TO_FUNCTION(MemChunkNew)				\
	DO_TO_FUNCTION(MemPtrNew)				\
	DO_TO_FUNCTION(MemPtrSetOwner)			\
	DO_TO_FUNCTION(MemPtrResize)			\
	DO_TO_FUNCTION(MemPtrResetLock)			\
	DO_TO_FUNCTION(MemHandleNew)			\
	DO_TO_FUNCTION(MemHandleResize)			\
	DO_TO_FUNCTION(MemHandleLock)			\
	DO_TO_FUNCTION(MemHandleResetLock)		\
	DO_TO_FUNCTION(MemLocalIDToLockedPtr)	\
	DO_TO_FUNCTION(MemHeapScramble)

// List of functions for which we want to automatically
// generate simple tailpatches only.

#define FOR_EACH_STUB_TAILPATCH_FUNCTION(DO_TO_FUNCTION)	\
	DO_TO_FUNCTION(MemSemaphoreReserve)		\
	DO_TO_FUNCTION(MemSemaphoreRelease)

// List of functions for which we have compex head- and tailpatches.

#define FOR_EACH_STUB_NOPATCH_FUNCTION(DO_TO_FUNCTION)	\
	DO_TO_FUNCTION(MemChunkFree)			\
	DO_TO_FUNCTION(MemHandleFree)			\
	DO_TO_FUNCTION(MemHandleUnlock)			\
	DO_TO_FUNCTION(MemPtrUnlock)			\

// This macro contains _all_ memory manager functions.

#define FOR_EACH_MEMMGR_FUNCTION(DO_TO_FUNCTION)		\
	FOR_EACH_STUB_BOTHPATCH_FUNCTION(DO_TO_FUNCTION)	\
	FOR_EACH_STUB_HEADPATCH_FUNCTION(DO_TO_FUNCTION)	\
	FOR_EACH_STUB_TAILPATCH_FUNCTION(DO_TO_FUNCTION)	\
	FOR_EACH_STUB_NOPATCH_FUNCTION(DO_TO_FUNCTION)


class MemMgrHeadpatch
{
	public:
		// Declare headpatches for all the memory manager functions.

		#define DECLARE_HEAD_PATCH(fn) static CallROMType fn (void);
		FOR_EACH_MEMMGR_FUNCTION(DECLARE_HEAD_PATCH)
};


class MemMgrTailpatch
{
	public:
		// Declare tailpatches for all the memory manager functions.

		#define DECLARE_TAIL_PATCH(fn) static void fn (void);
		FOR_EACH_MEMMGR_FUNCTION(DECLARE_TAIL_PATCH)
};


void CheckForMemoryLeaks (UInt32 memOwnerID);


#endif // EmPatchModuleSys_h
