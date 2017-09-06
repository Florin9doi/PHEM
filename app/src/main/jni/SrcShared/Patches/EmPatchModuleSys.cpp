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

#include "CGremlinsStubs.h" 	// StubAppEnqueueKey
#include "DebugMgr.h"			// Debug::ConnectedToTCPDebugger
#include "EmFileImport.h"		// InstallExgMgrLib
#include "EmEventOutput.h"		// EmEventOutput::PoppingUpForm
#include "EmEventPlayback.h"	// EmEventPlayback::ReplayingEvents
#include "EmLowMem.h"			// EmLowMem::GetEvtMgrIdle, EmLowMem::TrapExists, EmLowMem_SetGlobal, EmLowMem_GetGlobal
#include "EmMemory.h"			// CEnableFullAccess, EmMem_memcpy, EmMem_strcpy, EmMem_strcmp
#include "EmPalmFunction.h"		// InEggOfInfiniteWisdom
#include "EmPalmOS.h"			// EmPalmOS::RememberStackRange
#include "EmPalmStructs.h"		// EmAliasErr
#include "EmPatchMgr.h"			// PuppetString
#include "EmPatchModule.h"		// IntlMgrAvailable
#include "EmPatchState.h"
#include "EmRPC.h"			// RPC::SignalWaiters
#include "EmSession.h"			// GetDevice
#include "EmApplication.h"		// Ticks per second
#include "EmSubroutine.h"
#include "ErrorHandling.h"		// Errors::SysFatalAlert
#include "Hordes.h"				// Hordes::IsOn, Hordes::PostFakeEvent, Hordes::CanSwitchToApp
#include "HostControlPrv.h" 	// HandleHostControlCall
#include "Logging.h"			// LogEvtAddEventToQueue, etc.
#include "Marshal.h"			// CALLED_GET_PARAM_STR
#include "MetaMemory.h" 		// MetaMemory mark functions
#include "Miscellaneous.h"		// SetHotSyncUserName, DateToDays, SystemCallContext
#include "Platform.h"			// Platform::SndDoCmd, GetString
#include "PreferenceMgr.h"		// Preference (kPrefKeyUserName)
#include "Profiling.h"			// StDisableAllProfiling
#include "ROMStubs.h"			// FtrSet, FtrUnregister, EvtWakeup, ...
#include "SessionFile.h"		// SessionFile
#include "SLP.h"				// SLP
#include "Startup.h"			// Startup::GetAutoLoads
#include "Strings.r.h"			// kStr_ values
#include "SystemPacket.h"		// SystemPacket::SendMessage

#include <vector>				// vector

#if PLATFORM_MAC
#include <errno.h>				// ENOENT, errno
#include <sys/types.h>			// u_short, ssize_t, etc.
#include <sys/socket.h>			// sockaddr
#include <sys/errno.h>			// Needed for error translation.
#include <sys/time.h>			// fd_set
#include <netdb.h>				// hostent
#include <unistd.h>				// close
#include <sys/filio.h>			// FIONBIO
#include <sys/ioctl.h>			// ioctl
#include <netinet/in.h>			// sockaddr_in
#include <netinet/tcp.h>		// TCP_NODELAY
#include <arpa/inet.h>			// inet_ntoa
#endif

#if PLATFORM_UNIX
#include <errno.h>				// ENOENT, errno
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>			// timeval
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>				// close
#include <arpa/inet.h>			// inet_ntoa
#endif

#include "PalmPack.h"
#define NON_PORTABLE
#include "HwrROMToken.h"		// hwrROMTokenIrda
#undef NON_PORTABLE
#include "PalmPackPop.h"


// ======================================================================
//	Global Interfaces
// ======================================================================


// ======================================================================
//	Proto patch table for the system functions.  This array will be used
//	to create a sparse array at runtime.
// ======================================================================

ProtoPatchTableEntry	gProtoSysPatchTable[] =
{
	{sysTrapBmpCreate,				NULL, 									SysTailpatch::BmpCreate},
	{sysTrapBmpDelete,				NULL, 									SysTailpatch::BmpDelete},
	{sysTrapClipboardGetItem, 		SysHeadpatch::ClipboardGetItem,			NULL},
	{sysTrapDbgMessage, 			SysHeadpatch::DbgMessage,				NULL},
	{sysTrapClipboardAddItem,		NULL,									SysTailpatch::ClipboardAddItem},
	{sysTrapClipboardAppendItem,	NULL,									SysTailpatch::ClipboardAppendItem},
	{sysTrapDmCloseDatabase, 		SysHeadpatch::DmCloseDatabase,			NULL},
	{sysTrapDmInit, 				SysHeadpatch::DmInit,					NULL},
	{sysTrapDmGet1Resource, 		NULL,									SysTailpatch::DmGet1Resource},
	{sysTrapDmGetResource, 			NULL,									SysTailpatch::DmGetResource},
	{sysTrapErrDisplayFileLineMsg,	SysHeadpatch::ErrDisplayFileLineMsg,	NULL},
	{sysTrapEvtAddEventToQueue, 	SysHeadpatch::EvtAddEventToQueue,		NULL},
	{sysTrapEvtAddUniqueEventToQueue,SysHeadpatch::EvtAddUniqueEventToQueue,NULL},
	{sysTrapEvtEnqueueKey,			SysHeadpatch::EvtEnqueueKey,			NULL},
	{sysTrapEvtEnqueuePenPoint, 	SysHeadpatch::EvtEnqueuePenPoint,		NULL},
	{sysTrapEvtGetEvent,			SysHeadpatch::RecordTrapNumber, 		SysTailpatch::EvtGetEvent},
	{sysTrapEvtGetPen,				SysHeadpatch::RecordTrapNumber, 		SysTailpatch::EvtGetPen},
	{sysTrapEvtGetSysEvent, 		NULL,									SysTailpatch::EvtGetSysEvent},
	{sysTrapEvtSysEventAvail,		NULL,									SysTailpatch::EvtSysEventAvail},
	{sysTrapExgReceive,				NULL,									SysTailpatch::ExgReceive},
	{sysTrapExgSend,				NULL,									SysTailpatch::ExgSend},
	{sysTrapExgDoDialog,			SysHeadpatch::ExgDoDialog,				NULL},
	{sysTrapFrmCustomAlert, 		SysHeadpatch::FrmCustomAlert,			NULL},
	{sysTrapFrmDrawForm,			SysHeadpatch::FrmDrawForm,				NULL},
	{sysTrapFrmPopupForm,			SysHeadpatch::FrmPopupForm,				NULL},
	{sysTrapFtrInit,				NULL,									SysTailpatch::FtrInit},
	{sysTrapFtrSet,					NULL,									SysTailpatch::FtrSet},
	{sysTrapHostControl,			SysHeadpatch::HostControl,				NULL},
	{sysTrapHwrBatteryLevel,		SysHeadpatch::HwrBatteryLevel,			NULL},
	{sysTrapHwrBattery,				SysHeadpatch::HwrBattery,				NULL},
	{sysTrapHwrDockStatus,			SysHeadpatch::HwrDockStatus,			NULL},
	{sysTrapHwrGetROMToken, 		SysHeadpatch::HwrGetROMToken,			NULL},
	{sysTrapHwrMemReadable, 		NULL,						SysTailpatch::HwrMemReadable},
	{sysTrapHwrSleep,				SysHeadpatch::HwrSleep, 		SysTailpatch::HwrSleep},
	{sysTrapPenOpen,				SysHeadpatch::PenOpen,			NULL},
	{sysTrapPrefSetAppPreferences,	SysHeadpatch::PrefSetAppPreferences,	NULL},
	{sysTrapSndDoCmd,				SysHeadpatch::SndDoCmd,			NULL},
	{sysTrapSysAppExit, 			SysHeadpatch::SysAppExit,			NULL},
	{sysTrapSysAppLaunch,			SysHeadpatch::SysAppLaunch, 			NULL},
	{sysTrapSysAppStartup,			NULL,						SysTailpatch::SysAppStartup},
	{sysTrapSysBinarySearch,		SysHeadpatch::SysBinarySearch,			SysTailpatch::SysBinarySearch},
	{sysTrapSysEvGroupWait, 		SysHeadpatch::SysEvGroupWait,			NULL},
	{sysTrapSysFatalAlert,			SysHeadpatch::SysFatalAlert,			NULL},
	{sysTrapSysLaunchConsole,		SysHeadpatch::SysLaunchConsole, 		NULL},
	{sysTrapSysSemaphoreWait,		SysHeadpatch::SysSemaphoreWait, 		NULL},
	{sysTrapSysTaskCreate,			NULL,						SysTailpatch::SysTaskCreate},
//	{sysTrapSysTicksPerSecond,		SysHeadpatch::SysTicksPerSecond, 		NULL},
	{sysTrapSysUIAppSwitch, 		SysHeadpatch::SysUIAppSwitch,			NULL},
	{sysTrapTblHandleEvent,			SysHeadpatch::TblHandleEvent,			SysTailpatch::TblHandleEvent},
	{sysTrapTimInit,				NULL,									SysTailpatch::TimInit},
	{sysTrapUIInitialize,			NULL,									SysTailpatch::UIInitialize},
	{sysTrapUIReset,				NULL,									SysTailpatch::UIReset},

	{sysTrapCtlNewControl,			SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapFldNewField,			SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapFrmInitForm,			SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapFrmNewBitmap,			SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapFrmNewGadget,			SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapFrmNewGsi,				SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapFrmNewLabel,			SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapLstNewList,				SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapWinAddWindow,			SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},
	{sysTrapWinRemoveWindow,		SysHeadpatch::UnmarkUIObjects,			SysTailpatch::MarkUIObjects},

	{0, 							NULL,									NULL}
};


extern ProtoPatchTableEntry	gProtoMemMgrPatchTable[];





// ===========================================================================
//		¥
// ===========================================================================

// ======================================================================
//	Globals and constants
// ======================================================================


static long	gSaveDrawStateStackLevel;

static Bool	gDontPatchClipboardAddItem;
static Bool	gDontPatchClipboardGetItem;


// ======================================================================
//	Private functions
// ======================================================================

static long	PrvGetDrawStateStackLevel	(void);
static void	PrvCopyPalmClipboardToHost	(void);
static void	PrvCopyHostClipboardToPalm	(void);

void	PrvAutoload					(void);	// Also called in PostLoad
void	PrvSetCurrentDate			(void);	// Also called in PostLoad


#pragma mark -



// ===========================================================================
//		¥ EmPatchModuleSys
// ===========================================================================

/***********************************************************************
 *
 * FUNCTION:	EmPatchModuleSys::EmPatchModuleSys
 *
 * DESCRIPTION:	Constructor
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

EmPatchModuleSys::EmPatchModuleSys() :
	EmPatchModule ("~system", gProtoSysPatchTable, gProtoMemMgrPatchTable)
{
}


// ===========================================================================
//		¥ SysHeadpatch
// ===========================================================================

/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::RecordTrapNumber
 *
 * DESCRIPTION:	Record the trap we're executing for our patch to
 *				SysEvGroupWait later.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::RecordTrapNumber (void)
{
	EmAssert (gCPU);
	uint8* realMem = EmMemGetRealAddress (gCPU->GetPC ());

	EmAssert (EmMemDoGet16 (realMem - 2) == (m68kTrapInstr + sysDispatchTrapNum));

	EmPatchState::SetLastEvtTrap (EmMemDoGet16 (realMem));

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::ClipboardGetItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::ClipboardGetItem (void)
{
	if (!Hordes::IsOn () && !gDontPatchClipboardGetItem)
	{
		::PrvCopyHostClipboardToPalm ();
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::UnmarkUIObjects
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::UnmarkUIObjects (void)
{
	MetaMemory::UnmarkUIObjects ();

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::DbgMessage
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::DbgMessage (void)
{
	// void DbgMessage(const Char* aStr);

	CALLED_SETUP ("void", "const Char* aStr");

	CALLED_GET_PARAM_STR (Char, aStr);

	if (aStr != EmMemNULL)
	{
		// Get the string that's passed in.

		string		msgCopy	(aStr);
		CSocket*	debuggerSocket		= Debug::GetDebuggerSocket ();
		Bool		contactedDebugger	= false;

		// If we're connected to a debugger, try to send it the string.

		if (debuggerSocket)
		{
			SLP		slp (debuggerSocket);
			ErrCode	err = SystemPacket::SendMessage (slp, msgCopy.c_str ());
			if (!err)
			{
				contactedDebugger = true;
			}
		}

		// If that failed or if we're not connected to a debugger,
		// tell the user in one of our own dialogs.

		if (!contactedDebugger)
		{
			// Squelch these debugger messages.  Some functions in the ROM
			// (see Content.c and Progress.c) use DbgMessage for logging
			// instead of debugging.  I'm hoping this will get fixed after
			// Palm OS 4.0, so I'm only checking in versions before that.
			//
			// ("Transaction finished" actually appears only in 3.2 and
			//  3.3 ROMs, so that one's already taken care of.)
			//
			// Also note that I have the last character as "\x0A".  In
			// the ROM sources, this is "\n".  However, the value that
			// that turns into is compiler-dependent, so I'm using the
			// value that the Palm compiler is currently known to use.

			if ((msgCopy != "...Transaction cancelled\x0A" &&
				 msgCopy != "Transaction finished\x0A" &&
				 msgCopy != "c") ||
				EmPatchState::OSMajorMinorVersion () > 40)
			{
				Errors::ReportErrDbgMessage (msgCopy.c_str ());
			}
		}
	}

	return kSkipROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::DmCloseDatabase
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::DmCloseDatabase (void)
{
	// Err DmCloseDatabase(DmOpenRef dbR)

	CALLED_SETUP ("Err", "DmOpenRef dbR");

	CALLED_GET_PARAM_VAL (DmOpenRef, dbR);

	// Allow for a NULL reference.  Applications shouldn't be doing
	// this, but the test harness does in order to make sure the
	// OS recovers OK.  We need to check for it specially, because
	// even though DmCloseDatabase checks for and handles the NULL
	// reference, we're inserting a call to DmOpenDatabaseInfo here,
	// which does NOT handle the NULL reference (it will call
	// ErrDisplay, but subsequent code is not defensive and will crash).

	if (dbR == NULL)
		return kExecuteROM;

	UInt16	openCount;
	Boolean	resDB;

	Err	err = ::DmOpenDatabaseInfo (
		dbR,		// DmOpenRef dbR,
		NULL,		// LocalID* dbIDP,
		&openCount,	// UInt16 * openCountP,
		NULL,		// UInt16 * modeP,
		NULL,		// UInt16 * cardNoP,
		&resDB);	// Boolean * resDBP)

	if (!err && resDB && openCount == 1)
	{
		MemHandle	resource;
		UInt16		ii;
		UInt16		numResources = ::DmNumResources (dbR);

		for (ii = 0; ii < numResources; ++ii)
		{
			resource = ::DmGetResourceIndex (dbR, ii);
			MetaMemory::UnregisterBitmapHandle (resource);
		}

		// Search for any overlay databases and unregister any
		// bitmaps they may have, too.
		//
		// Overlays were added in Palm OS 3.5; don't check the
		// "openType" field in older OSes.

		if (EmPatchState::OSMajorMinorVersion () >= 35)
		{
			emuptr						dbAccessP = (emuptr) (DmOpenRef) dbR;
			EmAliasDmAccessType<PAS>	dbAccess (dbAccessP);

			// If this is a "base" database, look for a corresponding
			// overlay database.  Do this by iterating over the list
			// of open databases and finding the one in the list
			// preceding the base database we're closing.

			if (dbAccess.openType == openTypeBase)
			{
				// Get the first open database.

				DmOpenRef	olRef = ::DmNextOpenDatabase (NULL);

				// Iterate over the open databases until we get to the end
				// or until we meet up with then one we're closing.  The
				// overlay database will appear in the linked list *before*
				// the one we're closing, so if we get to that one without
				// finding the overlay, we never will find it.

				while (olRef && olRef != dbR)
				{
					// For each open database, see if it's an overlay type.
					// If it is, see if the "next" field points to the database
					// we're closing.

					emuptr						olAccessP = (emuptr) olRef;
					EmAliasDmAccessType<PAS>	olAccess (olAccessP);

					UInt8	openType	= olAccess.openType;
					emuptr	next		= olAccess.next;

					if (openType == openTypeOverlay && next == dbAccessP)
					{
						// OK, now that we're here, let's iterate over
						// the database and release any resources it
						// has, too.

						numResources = ::DmNumResources (olRef);
						for (ii = 0; ii < numResources; ++ii)
						{
							resource = ::DmGetResourceIndex (olRef, ii);
							MetaMemory::UnregisterBitmapHandle (resource);
						}

						break;	// Break out of the "while (olRef && olRef != dbR)..." loop.
					}

					// This either wasn't an overlay database, or it didn't
					// belong to the base database we were closing.  Move
					// on to the next database.

					olRef = ::DmNextOpenDatabase (olRef);
				}

				// Make sure we found the overlay.

				EmAssert (olRef && olRef != dbR);
			}
		}
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::DmInit
 *
 * DESCRIPTION:	After MemInit is called, we need to sync up with the
 *				initial state of the heap(s).  However, MemInit is not
 *				called via the trap table, so we can't easily tailpatch
 *				it.  DmInit is the first such function called after
 *				MemInit, so we headpatch *it* instead of tailpatching
 *				MemInit.
 *
 *				(Actually, MemHeapCompact is called as one of the last
 *				 things MemInit does which makes it an interesting
 *				 candidate for patching in order to sync up with the
 *				 heap state.  However, if we were to do a full sync on
 *				 that call, a full sync would occur on *every* call to
 *				 MemHeapCompact, which we don't really want to do.)
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::DmInit (void)
{
//	MetaMemory::SyncAllHeaps ();

	EmPatchState::SetHeapInitialized (true);

	// If they haven't been released by now, then release the boot keys.

	gSession->ReleaseBootKeys ();

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::ErrDisplayFileLineMsg
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

typedef Bool	(*VersionChecker)(void);
typedef Bool	(*FunctionChecker)(emuptr addr);

struct EmInvalidErrorMessageData
{
	VersionChecker	fVersionChecker;
	FunctionChecker	fFunctionChecker;
	const char*		fMessage;
};

EmInvalidErrorMessageData	kInvalidErrorMessages [] =
{
	{
		&EmPatchState::HasECValidateFieldBug,
		&::InECValidateField,
		"Invalid insertion point position"
	},
	{
		&EmPatchState::HasPrvDrawSliderControlBug,
		&::InPrvDrawSliderControl,
		"Background must be at least half as wide as slider."
	},
	{
		NULL,	// Don't care about the system version.
		&::InPrvFindMemoryLeaks,
		NULL	// Don't care about the actual text.
	}
};


CallROMType SysHeadpatch::ErrDisplayFileLineMsg (void)
{
	// void ErrDisplayFileLineMsg(CharPtr filename, UInt16 lineno, CharPtr msg)

	CALLED_SETUP ("void", "CharPtr filename, UInt16 lineno, CharPtr msg");

	CALLED_GET_PARAM_STR (Char, msg);

	{
		CEnableFullAccess	munge;	// Remove blocks on memory access.

		// Force this guy to true.	If it's false, ErrDisplayFileLineMsg will
		// just try to enter the debugger.

		UInt16	sysMiscFlags = EmLowMem_GetGlobal (sysMiscFlags);
		EmLowMem_SetGlobal (sysMiscFlags, sysMiscFlags | sysMiscFlagUIInitialized);

		// Clear this low-memory flag so that we force the dialog to display.
		// If this flag is true, ErrDisplayFileLineMsg will just try to enter
		// the debugger.

		EmLowMem_SetGlobal (dbgWasEntered, false);
	}

	// Some ROMs incorrectly display error messages via ErrDisplayFileLineMsg.
	// Check to see if we are running on a ROM version with one of those
	// erroneous messages, check to see that we are in the function that
	// displays the message, and check to see if the text of the message
	// handed to us is one of the incorrect ones.  If all conditions are
	// true, squelch the message.

	for (size_t ii = 0; ii < countof (kInvalidErrorMessages); ++ii)
	{
		EmAssert (gCPU);

		EmInvalidErrorMessageData*	d = &kInvalidErrorMessages[ii];

		if (
			(!d->fVersionChecker || d->fVersionChecker ()) &&
			(!d->fFunctionChecker || d->fFunctionChecker (gCPU->GetPC ())))
		{
			if (!d->fMessage || (strcmp (msg, d->fMessage) == 0))
			{
				return kSkipROM;
			}
		}
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::EvtAddEventToQueue
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::EvtAddEventToQueue (void)
{
	// void EvtAddEventToQueue (const EventPtr event)

	CALLED_SETUP ("void", "const EventPtr event");

	CALLED_GET_PARAM_REF (EventType, event, Marshal::kInput);

	LogEvtAddEventToQueue (*event);

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::EvtAddUniqueEventToQueue
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::EvtAddUniqueEventToQueue (void)
{
	// void EvtAddUniqueEventToQueue(const EventPtr eventP, const UInt32 id, const Boolean inPlace)

	CALLED_SETUP ("void", "const EventPtr eventP, const UInt32 id, const Boolean inPlace");

	CALLED_GET_PARAM_REF (EventType, eventP, Marshal::kInput);
	CALLED_GET_PARAM_VAL (UInt32, id);
	CALLED_GET_PARAM_VAL (Boolean, inPlace);

	LogEvtAddUniqueEventToQueue (*eventP, id, inPlace);

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::EvtEnqueueKey
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::EvtEnqueueKey (void)
{
	// Err EvtEnqueueKey(UInt16 ascii, UInt16 keycode, UInt16 modifiers)

	CALLED_SETUP ("Err", "UInt16 ascii, UInt16 keycode, UInt16 modifiers");

	CALLED_GET_PARAM_VAL (UInt16, ascii);
	CALLED_GET_PARAM_VAL (UInt16, keycode);
	CALLED_GET_PARAM_VAL (UInt16, modifiers);

	LogEvtEnqueueKey (ascii, keycode, modifiers);

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::EvtEnqueuePenPoint
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::EvtEnqueuePenPoint (void)
{
	// Err EvtEnqueuePenPoint(PointType* ptP)

	CALLED_SETUP ("Err", "PointType* ptP");

	CALLED_GET_PARAM_REF (PointType, ptP, Marshal::kInput);

	LogEvtEnqueuePenPoint (*ptP);

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::ExgDoDialog
 *
 * DESCRIPTION:	Always accept the beam if we're beaming it in via our
 *				special ExgMgr "driver".
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::ExgDoDialog (void)
{
	// Boolean ExgDoDialog(ExgSocketPtr socketP, ExgDialogInfoType *infoP, Err *errP)

	CALLED_SETUP ("Boolean", "ExgSocketPtr socketP, ExgDialogInfoType *infoP, Err *errP");

	CALLED_GET_PARAM_VAL (ExgSocketPtr, socketP);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInOut);

	if (!EmPatchState::GetAutoAcceptBeamDialogs())
		return kExecuteROM;

	if (!socketP)
		return kExecuteROM;

	CALLED_PUT_PARAM_REF (errP);
	PUT_RESULT_VAL (Boolean, true);

	return kSkipROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::FrmCustomAlert
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

#define replaceBeamAppAlert					2008
#define beamReplaceErrorAppIsRunning		2012

#define replaceBeamAppAlertYesButton		0

CallROMType SysHeadpatch::FrmCustomAlert (void)
{
	// UInt16 FrmCustomAlert (UInt16 alertId, const Char* s1, const Char* s2, const Char* s3)

	CALLED_SETUP ("UInt16", "UInt16 alertId, const Char* s1, const Char* s2, const Char* s3");

	CALLED_GET_PARAM_VAL (UInt16, alertId);

	if (!EmPatchState::GetAutoAcceptBeamDialogs())
		return kExecuteROM;

	// "App already exists, replace it?"

	if (alertId == replaceBeamAppAlert)
	{
		PUT_RESULT_VAL (UInt16, replaceBeamAppAlertYesButton);
		return kSkipROM;
	}

	// "App is running. Switch first."

	if (alertId == beamReplaceErrorAppIsRunning)
	{
		PUT_RESULT_VAL (UInt16, 0);	// Can be anything.
		return kSkipROM;
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::FrmDrawForm
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::FrmDrawForm (void)
{
	// void FrmDrawForm (const FormPtr frm)

	CALLED_SETUP ("void", "const FormPtr frm");

	CALLED_GET_PARAM_VAL (FormPtr, frm);

	::ValidateFormObjects (frm);

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::FrmPopupForm
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::FrmPopupForm (void)
{
	// void FrmPopupForm (UInt16 formId)

//	CALLED_SETUP ("void", "UInt16 formId");

//	CALLED_GET_PARAM_VAL (UInt16, formId);

	EmEventOutput::PoppingUpForm ();

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::HostControl
 *
 * DESCRIPTION:	This one's kind of odd.  Originally, there was
 *				SysGremlins, which was declared as follows:
 *
 *					UInt32 SysGremlins(GremlinFunctionType selector,
 *										GremlinParamsType *params)
 *
 *				Also originally, the only defined selector was
 *				GremlinIsOn.
 *
 *				Now, SysGremlins is extended to be SysHostControl,
 *				which allows the Palm environment to access host
 *				functions if it's actually running under the simulator
 *				or emulator.
 *
 *				Because of this extension, functions implemented via
 *				this trap are not limited to pushing a selector and
 *				parameter block on the stack. Now, they will all push
 *				on a selector, but what comes after is dependent on the
 *				selector.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::HostControl (void)
{
	return HandleHostControlCall ();
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::HwrBatteryLevel
 *
 * DESCRIPTION:	Return that the battery is always full.  HwrBatteryLevel
 *				is the bottleneck function called to determine the
 *				battery level.	By patching it this way, we don't have
 *				to emulate the hardware registers that report the
 *				battery level.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::HwrBatteryLevel (void)
{
	// On non-328 devices, we emulate the hardware that returns this
	// information, so we don't need to patch this function.

	// We don't currently emulate the Prism battery hardware, so we
	// allow the patch. Not sure if this is called by the Prism ROM,
	// but we patch it to be safe (better than dividing by zero).

	EmAssert (gSession);

	if (!gSession->GetDevice().Supports68328 () &&
		gSession->GetDevice().HardwareID () != 0x0a /*halModelIDVisorPrism*/)
	{
		return kExecuteROM;
	}

	// UInt16 HwrBatteryLevel(void)

	CALLED_SETUP ("UInt16", "void");

	PUT_RESULT_VAL (UInt16, 255);	// Hardcode a maximum level

	return kSkipROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::HwrBattery
 *
 * DESCRIPTION:	If queried for the battery level, return that the battery
 *				is always full.  This is equivalent to the HwrBatteryLevel
 *				patch, necessary because some newer devices call HwrBattery
 *				for this information. 
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 * History: 
 *   2000-05-10 Added by Bob Petersen 
 *
 ***********************************************************************/

CallROMType SysHeadpatch::HwrBattery (void)
{
	// On non-328 devices, we emulate the hardware that returns this
	// information, so we don't need to patch this function.

	// We don't currently emulate the Prism battery hardware, so we
	// allow the patch.

	EmAssert (gSession);

	if (!gSession->GetDevice().Supports68328 () &&
		gSession->GetDevice().HardwareID () != 0x0a /*halModelIDVisorPrism*/)
	{
		return kExecuteROM;
	}

	//	Err		HwrBattery(UInt16 /* HwrBatterCmdEnum*/ cmd, void * cmdP)

	CALLED_SETUP ("Err", "UInt16 cmd, void * cmdP");

	CALLED_GET_PARAM_VAL (UInt16, cmd);

	if (cmd == 2 /* hwrBatteryCmdMainRead */)
	{
		CALLED_GET_PARAM_REF (HwrBatCmdReadType, cmdP, Marshal::kInOut);

		if (gSession->GetDevice().HardwareID () == 0x0a /*halModelIDVisorPrism*/)
		{
			// MAINTAINER'S NOTE: The following code and comment are pretty
			// much what I got from Handspring.  The comment says 4.2 volts,
			// and the code contains redundant assignments that first assign
			// 4.2 volts and then 4.0 volts.  I'm just keeping this in case
			// they really want it this way for some reason.

			// DOLATER: Battery voltage should differ by battery type.
			// 4.2 is returned to handle lithium ion batteries.  'mVolts'
			// is the value used by the Prism display, not 'abs'. 
			(*cmdP).mVolts = 4200;	// 4.2 volts
			(*cmdP).mVolts = 4000;	// 4.0 volts
		}
		else
		{
			(*cmdP).mVolts = 2500;	// 2.5 volts
		}

		(*cmdP).abs = 255;		// Hardcode a maximum level

		cmdP.Put ();

		PUT_RESULT_VAL (Err, errNone);
		return kSkipROM;
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::HwrDockStatus
 *
 * DESCRIPTION:	Always return hwrDockStatusUsingExternalPower.  We
 *				could fake this out by twiddling the right bits in the
 *				Dragonball and DragonballEZ emulation units, but those
 *				bits are different for almost every device.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::HwrDockStatus (void)
{
	// On non-328 devices, we emulate the hardware that returns this
	// information, so we don't need to patch this function.

	EmAssert (gSession);

	if (!gSession->GetDevice().Supports68328 ())
	{
		return kExecuteROM;
	}

	// hwrDockStatusState HwrDockStatus(void)
	//
	//	(added in Palm OS 3.1)
	//	(changed later to return UInt16)

	CALLED_SETUP ("UInt16", "void");

	// Old enumerated values from Hardware.h:
	//
	//		DockStatusNotDocked = 0,
	//		DockStatusInModem,
	//		DockStatusInCharger,
	//		DockStatusUnknown = 0xFF

	// New defines from HwrDock.h
#define	hwrDockStatusUndocked			0x0000	// nothing is attached
#define	hwrDockStatusModemAttached		0x0001	// some type of modem is attached
#define	hwrDockStatusDockAttached		0x0002	// some type of dock is attached
#define	hwrDockStatusUsingExternalPower	0x0004	// using some type of external power source
#define	hwrDockStatusCharging			0x0008	// internal power cells are recharging

	PUT_RESULT_VAL (UInt16, hwrDockStatusUsingExternalPower);

	return kSkipROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::HwrGetROMToken
 *
 * DESCRIPTION:	Patch this guy so that we never return the 'irda' token.
 *				We should take this out when some sort of IR support is
 *				added.
 *
 *				NOTE: This patch is useless for diverting the ROM. It
 *				calls HwrGetROMToken directly, which means that it will
 *				bypass this patch.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::HwrGetROMToken (void)
{
	//	Err HwrGetROMToken (UInt16 cardNo, UInt32 token, UInt8** dataP, UInt16* sizeP)

	CALLED_SETUP ("Err", "UInt16 cardNo, UInt32 token, UInt8** dataP, UInt16* sizeP");

	CALLED_GET_PARAM_VAL (UInt16, cardNo);
	CALLED_GET_PARAM_VAL (UInt32, token);
	CALLED_GET_PARAM_REF (UInt8Ptr, dataP, Marshal::kInOut);
	CALLED_GET_PARAM_REF (UInt16, sizeP, Marshal::kInOut);

	if (cardNo == 0 && token == hwrROMTokenIrda)
	{
		if (dataP)
		{
			*dataP = 0;
			dataP.Put ();
		}

		if (sizeP)
		{
			*sizeP = 0;
			sizeP.Put ();
		}

		PUT_RESULT_VAL (Err, ~0);	// token not found.

		return kSkipROM;
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::HwrSleep
 *
 * DESCRIPTION:	Record whether or not we are sleeping and update the
 *				boolean that determines if low-memory access is OK.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::HwrSleep (void)
{
	// void HwrSleep(Boolean untilReset, Boolean emergency)

	// HwrSleep changes the exception vectors for for the interrupts,
	// so temporarily unlock those.  We'll re-establish them in the
	// HwrSleep tailpatch.

	MetaMemory::MarkTotalAccess (offsetof (M68KExcTableType, busErr),
								 offsetof (M68KExcTableType, busErr) + sizeof (emuptr));

	MetaMemory::MarkTotalAccess (offsetof (M68KExcTableType, addressErr),
								 offsetof (M68KExcTableType, addressErr) + sizeof (emuptr));

	MetaMemory::MarkTotalAccess (offsetof (M68KExcTableType, illegalInstr),
								 offsetof (M68KExcTableType, illegalInstr) + sizeof (emuptr));

	MetaMemory::MarkTotalAccess (offsetof (M68KExcTableType, autoVec1),
								 offsetof (M68KExcTableType, trapN[0]));

	// On Palm OS 1.0, there was code in HwrSleep that would execute only if
	// memory location 0xC2 was set.  It looks like debugging code (that is,
	// Ron could change it from the debugger to try different things out),
	// but we still have to allow access to it.

	if (EmPatchState::OSMajorVersion () == 1)
	{
		MetaMemory::MarkTotalAccess (0x00C2, 0x00C3);
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::KeyCurrentState
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

// From Roger:
//
//	I was thinking a bit more about Gremlins and games yesterday.  Games which
//	use the buttons as input have been a special case for Gremlins because
//	Gremlins only faked events and pen points.	Normally games have to fake
//	their own button mask, and they get better results if have some buttons
//	held down more often than others.
//
//	Now I'm thinking different.  With Poser, the KeyCurrentState call should
//	be stolen when Gremlins is running.  All of the keys possible should have
//	their button bits on half the time.  This will allow users to test games.
//	By not tuning how often buttons should be held down, the testing process
//	will take longer to excerise all app functionality, but it's better than
//	now.  App developers can override the default Gremlin values with their
//	own for better results.
//
//	To successfully test this, A game like SubHunt should play on it's own for
//	a least a while.  HardBall is an example of a game which would really
//	benefit from recognizing Gremlins is running and tune itself to make the
//	testing more effective.  It should grant infinite balls until after the
//	last level is finished.
//
//	I actually think this is important enough to be for Acorn because it
//	enables users to test a large class of apps which otherwise can't.	I
//	think it's easy to implement.  Basically just have KeyCurrentState return
//	the int from the random number generator.  Each bit should be on about
//	half the time.
//
// Follow-up commentary: it turns out that this patch is not having the
// effect we hoped.  SubHunt was so overwhelmed with events from Gremlins
// that it rarely had the chance to call KeyCurrentState.  We're thinking
// of Gremlins backing off on posting events if the SysGetEvent sleep time
// is small (i.e., not "forever"), but we'll have to think about the impact
// on other apps first.

#if 0
CallROMType SysHeadpatch::KeyCurrentState (void)
{
	// UInt32 KeyCurrentState(void)

	CALLED_SETUP ("UInt32", "void");

	// Remove this for now.  Its current implementation uses the Std C Lib
	// random number generator, which is not in sync with the Gremlins RNG.

	if (Hordes::IsOn ())
	{
		// Let's try setting each bit 1/4 of the time.

		uint32 bits = rand () & rand ();

		PUT_RESULT_VAL (Uint32, bits);

		return kSkipROM;
	}

	return kExecuteROM;
}
#endif


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::PenOpen
 *
 * DESCRIPTION:	This is where the pen calibration information is read.
 *				Preflight this call to add the calibration information
 *				to the preferences database if it doesn't exist.  That
 *				way, we don't have to continually calibrate the screen
 *				when we boot up.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::PenOpen (void)
{
	// Err PenOpen(void)

	::InstallCalibrationInfo ();

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::PrefSetAppPreferences
 *
 * DESCRIPTION:	Change the default proxy server address.  If it looks
 *				like INet is establishing the old proxy server address,
 *				substitute the current one instead.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::PrefSetAppPreferences (void)
{
	// void PrefSetAppPreferences (UInt32 creator, UInt16 id, Int16 version, 
	//					const void *prefs, UInt16 prefsSize, Boolean saved)

	CALLED_SETUP ("void", "UInt32 creator, UInt16 id, Int16 version, "
					"const void *prefs, UInt16 prefsSize, Boolean saved");

	CALLED_GET_PARAM_VAL (UInt32, creator);
	CALLED_GET_PARAM_VAL (emuptr, prefs);
	CALLED_GET_PARAM_VAL (UInt16, prefsSize);
	CALLED_GET_PARAM_VAL (Boolean, saved);

#define inetCreator	'inet'

	if ((creator == inetCreator) && saved && (prefsSize >= 3 + 64))
	{
		emuptr proxyP = prefs + 3;

		if ((EmMem_strcmp (proxyP, "207.240.80.136") == 0) ||
			(EmMem_strcmp (proxyP, "209.247.202.106") == 0))
		{
			static string	currentIPAddress;

			// See if we've looked up the address, yet.

			if (currentIPAddress.empty ())
			{
				// If not, get the host information.

				struct hostent*	host = gethostbyname ("content-dev.palm.net");
				if (!(host && host->h_addrtype == AF_INET))
				{
					host = gethostbyname ("content-dev2.palm.net");
				}

				if (host && host->h_addrtype == AF_INET)
				{
					// If the address family looks like one we can handle,
					// get and use the first one.

					struct in_addr*	addr_in = (struct in_addr*) host->h_addr_list[0];
					if (addr_in)
					{
						char*	as_string = inet_ntoa (*addr_in);
						currentIPAddress = as_string;
					}
				}
			}

			// If we have the address cached NOW, use it to replace
			// the hardcoded addresses.

			if (!currentIPAddress.empty ())
			{
				EmMem_strcpy (proxyP, currentIPAddress.c_str ());
			}
		}
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SndDoCmd
 *
 * DESCRIPTION:	Intercept calls to the Sound Manager and generate the
 *				equivalent host commands.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SndDoCmd (void)
{
	Preference<bool>	pref (kPrefKeyEnableSounds);

	if (!*pref)
		return kExecuteROM;

	// Err SndDoCmd(void * chanP, SndCommandPtr cmdP, Boolean noWait)

	CALLED_SETUP ("Err", "void * chanP, SndCommandPtr cmdP, Boolean noWait");

	CALLED_GET_PARAM_REF (SndCommandType, cmdP, Marshal::kInput);

	PUT_RESULT_VAL (Err, errNone);	// Set default return value

	return Platform::SndDoCmd (*cmdP);
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysAppExit
 *
 * DESCRIPTION:	If the application calling SysAppExit was launched as a
 *				full application, then "forget" any information we have
 *				about it.  When the next application is launched, we'll
 *				collect information on it in SysAppStartup.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SysAppExit (void)
{
	// Err SysAppExit(SysAppInfoPtr appInfoP, MemPtr prevGlobalsP, MemPtr globalsP)

	CALLED_SETUP ("Err", "SysAppInfoPtr appInfoP, MemPtr prevGlobalsP, MemPtr globalsP");

	CALLED_GET_PARAM_REF (SysAppInfoType, appInfoP, Marshal::kInput);

	if (appInfoP == NULL)
		return kExecuteROM;

	Int16	cmd			= (*appInfoP).cmd;
	UInt16	memOwnerID	= (*appInfoP).memOwnerID;
	UInt16	launchFlags	= (*appInfoP).launchFlags;

	if (false /*LogLaunchCodes ()*/)
	{
		emuptr	dbP		= (emuptr) ((*appInfoP).dbP);
		emuptr	openP	= EmMemGet32 (dbP + offsetof (DmAccessType, openP));
		LocalID	dbID	= EmMemGet32 (openP + offsetof (DmOpenInfoType, hdrID));
		UInt16	cardNo	= EmMemGet16 (openP + offsetof (DmOpenInfoType, cardNo));

		char	name[dmDBNameLength] = {0};
		UInt32	type = 0;
		UInt32	creator = 0;

		/* Err	err =*/ ::DmDatabaseInfo (
				cardNo, dbID, name,
				NULL, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL,
				&type, &creator);

		LogAppendMsg ("SysAppExit called:");
		LogAppendMsg ("	cardNo:			%ld",		(long) cardNo);
		LogAppendMsg ("	dbID:			0x%08lX",	(long) dbID);
		LogAppendMsg ("		name:		%s",		name);
		LogAppendMsg ("		type:		%04lX",		type);
		LogAppendMsg ("		creator:	%04lX",		creator);
	}

	if (cmd == sysAppLaunchCmdNormalLaunch)
	{
		EmuAppInfo	appInfo;
		EmPatchState::CollectCurrentAppInfo ((emuptr) appInfoP, appInfo);

		if (appInfo.fCardNo == EmPatchState::GetQuitAppCardNo () && appInfo.fDBID == EmPatchState::GetQuitAppDbID ())
		{
			EmPatchState::SetTimeToQuit ();
		}
	}
	
	// Go through and dispose of any memory chunks allocated by this application.
	// <chg 5-8-98 RM> Changed test to: launchFlags & sysAppLaunchFlagNewGlobals
	//   from rootP==NULL so that it works correcty for sublaunched apps
	//   that get their own globals.

	if (memOwnerID && (launchFlags & sysAppLaunchFlagNewGlobals))
	{
		::CheckForMemoryLeaks (memOwnerID);
	}

	EmPatchState::RemoveCurAppInfo ();	// !!! should probably make sure appInfoP matches
										// the top guy on the stack.

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysAppLaunch
 *
 * DESCRIPTION:	Log information app launches and action codes.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SysAppLaunch (void)
{
	// Err SysAppLaunch(UInt16 cardNo, LocalID dbID, UInt16 launchFlags,
	//				UInt16 cmd, MemPtr cmdPBP, UInt32* resultP)

	CALLED_SETUP ("Err", "UInt16 cardNo, LocalID dbID, UInt16 launchFlags,"
					"UInt16 cmd, MemPtr cmdPBP, UInt32* resultP");

	CALLED_GET_PARAM_VAL (UInt16, cardNo);
	CALLED_GET_PARAM_VAL (LocalID, dbID);
	CALLED_GET_PARAM_VAL (UInt16, launchFlags);
	CALLED_GET_PARAM_VAL (UInt16, cmd);

	if (false /*LogLaunchCodes ()*/)
	{
		const char* launchStr = ::LaunchCmdToString (cmd);

		char	name[dmDBNameLength] = {0};
		UInt32	type = 0;
		UInt32	creator = 0;

		/* Err	err =*/ ::DmDatabaseInfo (
				cardNo, dbID, name,
				NULL, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL,
				&type, &creator);

		LogAppendMsg ("SysAppLaunch called:");
		LogAppendMsg ("	cardNo:			%ld",		(long) cardNo);
		LogAppendMsg ("	dbID:			0x%08lX",	(long) dbID);
		LogAppendMsg ("		name:		%s",		name);
		LogAppendMsg ("		type:		%04lX",		type);
		LogAppendMsg ("		creator:	%04lX",		creator);
		LogAppendMsg ("	launchFlags:	0x%08lX",	(long) launchFlags);
		LogAppendMsg ("	cmd:			%ld (%s)",	(long) cmd, launchStr);

#if 0
		switch (cmd)
		{
			case sysAppLaunchCmdNormalLaunch:
				// No parameter block
				break;

			case sysAppLaunchCmdFind:
			{
				FindParamsType	parms;

				LogAppendMsg ("	FindParamsType:");
				LogAppendMsg ("		dbAccesMode:		%ld",	(long) parms.dbAccesMode);
				LogAppendMsg ("		recordNum:			%ld",	(long) parms.recordNum);
				LogAppendMsg ("		more:				%ld",	(long) parms.more);
				LogAppendMsg ("		strAsTyped:			%ld",	(long) parms.strAsTyped);
				LogAppendMsg ("		strToFind:			%ld",	(long) parms.strToFind);
				LogAppendMsg ("		numMatches:			%ld",	(long) parms.numMatches);
				LogAppendMsg ("		lineNumber:			%ld",	(long) parms.lineNumber);
				LogAppendMsg ("		continuation:		%ld",	(long) parms.continuation);
				LogAppendMsg ("		searchedCaller:		%ld",	(long) parms.searchedCaller);
				LogAppendMsg ("		callerAppDbID:		%ld",	(long) parms.callerAppDbID);
				LogAppendMsg ("		callerAppCardNo:	%ld",	(long) parms.callerAppCardNo);
				LogAppendMsg ("		appDbID:			%ld",	(long) parms.appCardNo);
				LogAppendMsg ("		newSearch:			%ld",	(long) parms.newSearch);
				LogAppendMsg ("		searchState:		%ld",	(long) parms.searchState);
				LogAppendMsg ("		match:				%ld",	(long) parms.match);

				break;
			}

			case sysAppLaunchCmdGoTo:
				// GoToParamsType
				break;

			case sysAppLaunchCmdSyncNotify:
				// No parameter block
				break;

			case sysAppLaunchCmdTimeChange:
				// No parameter block
				break;

			case sysAppLaunchCmdSystemReset:
			{
				// SysAppLaunchCmdSystemResetType
				SysAppLaunchCmdSystemResetType	parms;
				LogAppendMsg ("	SysAppLaunchCmdSystemResetType:");
				LogAppendMsg ("		hardReset:			%s",	parms.hardReset ? "TRUE" : "FALSE");
				LogAppendMsg ("		createDefaultDB:	%s",	parms.createDefaultDB ? "TRUE" : "FALSE");
				break;
			}

			case sysAppLaunchCmdAlarmTriggered:
				// SysAlarmTriggeredParamType
				break;

			case sysAppLaunchCmdDisplayAlarm:
				// SysDisplayAlarmParamType
				break;

			case sysAppLaunchCmdCountryChange:
				// Not sent?
				break;

			case sysAppLaunchCmdSyncRequestLocal:
//			case sysAppLaunchCmdSyncRequest:
				// No parameter block (I think...)
				break;

			case sysAppLaunchCmdSaveData:
				// SysAppLaunchCmdSaveDataType
				break;

			case sysAppLaunchCmdInitDatabase:
				// SysAppLaunchCmdInitDatabaseType
				break;

			case sysAppLaunchCmdSyncCallApplicationV10:
				// SysAppLaunchCmdSyncCallApplicationTypeV10
				break;

			case sysAppLaunchCmdPanelCalledFromApp:
				// Panel specific?
				//	SvcCalledFromAppPBType
				//	NULL
				break;

			case sysAppLaunchCmdReturnFromPanel:
				// No parameter block
				break;

			case sysAppLaunchCmdLookup:
				// App-specific (see AppLaunchCmd.h)
				break;

			case sysAppLaunchCmdSystemLock:
				// No parameter block
				break;

			case sysAppLaunchCmdSyncRequestRemote:
				// No parameter block (I think...)
				break;

			case sysAppLaunchCmdHandleSyncCallApp:
				// SysAppLaunchCmdHandleSyncCallAppType
				break;

			case sysAppLaunchCmdAddRecord:
				// App-specific (see AppLaunchCmd.h)
				break;

			case sysSvcLaunchCmdSetServiceID:
				// ServiceIDType
				break;

			case sysSvcLaunchCmdGetServiceID:
				// ServiceIDType
				break;

			case sysSvcLaunchCmdGetServiceList:
				// serviceListType
				break;

			case sysSvcLaunchCmdGetServiceInfo:
				// serviceInfoType
				break;

			case sysAppLaunchCmdFailedAppNotify:
				// SysAppLaunchCmdFailedAppNotifyType
				break;

			case sysAppLaunchCmdEventHook:
				// EventType
				break;

			case sysAppLaunchCmdExgReceiveData:
				// ExgSocketType
				break;

			case sysAppLaunchCmdExgAskUser:
				// ExgAskParamType
				break;

			case sysDialLaunchCmdDial:
				// DialLaunchCmdDialType
				break;

			case sysDialLaunchCmdHangUp:
				// DialLaunchCmdDialType
				break;

			case sysSvcLaunchCmdGetQuickEditLabel:
				// SvcQuickEditLabelInfoType
				break;

			case sysAppLaunchCmdURLParams:
				// Part of the URL
				break;

			case sysAppLaunchCmdNotify:
				// SysNotifyParamType
				break;

			case sysAppLaunchCmdOpenDB:
				// SysAppLaunchCmdOpenDBType
				break;

			case sysAppLaunchCmdAntennaUp:
				// No parameter block
				break;

			case sysAppLaunchCmdGoToURL:
				// URL
				break;
		}
#endif
	}

	// Prevent Symbol applications from running.

	if (cmd == sysAppLaunchCmdSystemReset)
	{
		UInt32	type;
		UInt32	creator;
		/*Err		err =*/ ::DmDatabaseInfo (
							cardNo, dbID, NULL,
							NULL, NULL, NULL, NULL,
							NULL, NULL, NULL, NULL,
							&type, &creator);

		if (type == sysFileTApplication &&
			(creator == 'SSRA' || creator == 'SYRA'))
		{
			PUT_RESULT_VAL (Err, errNone);
			return kSkipROM;
		}
	}

	// Work around a bug in Launcher where it tries to send
	// sysAppLaunchCmdSyncNotify to any received files that
	// aren't PQAs.  This code is pretty much the same as
	// the fixed version of Launcher.

	static Bool recursing;

	if (!recursing && cmd == sysAppLaunchCmdSyncNotify && EmPatchState::HasSyncNotifyBug ())
	{
		UInt32	type;
		UInt32	creator;
		Err		err = ::DmDatabaseInfo (
							cardNo, dbID, NULL,
							NULL, NULL, NULL, NULL,
							NULL, NULL, NULL, NULL,
							&type, &creator);

		if (!err)
		{
			// If it's not an application, then get the application that owns it.

			UInt16	appCardNo = cardNo;
			LocalID	appDbID = dbID;

			if (type != sysFileTApplication)
			{
				DmSearchStateType	searchState;
				
				err = ::DmGetNextDatabaseByTypeCreator (true /*new search*/,
					&searchState, sysFileTApplication, creator, 
					true /*latest version*/, &appCardNo, &appDbID);
			}

			// If there's an error, pretend that everything went OK.

			if (err)
			{
				PUT_RESULT_VAL (Err, errNone);
				return kSkipROM;
			}

			// Otherwise, substitute the new cardNo and dbID for the
			// ones on the stack.

			sub.SetParamVal ("cardNo", appCardNo);
			sub.SetParamVal ("dbID", appDbID);
		}

		// Could not get information on the specified database.
		// Pass it off to the ROM as normal.
	}

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysBinarySearch
 *
 * DESCRIPTION:	There's a bug in pre-3.0 versions of SysBinarySearch
 *				that cause it to call the user callback function with a
 *				pointer just past the array to search.	Make a note of
 *				when we enter SysBinarySearch so that we can make
 *				allowances for that in MetaMemory's memory checking.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SysBinarySearch (void)
{
	EmPatchState::EnterSysBinarySearch ();

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysEvGroupWait
 *
 * DESCRIPTION:	We patch SysEvGroupWait as the mechanism for feeding
 *				the Palm OS new events.  See comments in
 *				EmPatchState::PuppetString.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SysEvGroupWait (void)
{
	// Only do this under 2.0 and later.  Under Palm OS 1.0, EvtGetSysEvent
	// called SysSemaphoreWait instead.  See our headpatch of that function
	// for a chunk of pretty similar code.

	if (EmPatchState::OSMajorVersion () == 1)
	{
		return kExecuteROM;
	}

	// Err SysEvGroupWait(UInt32 evID, UInt32 mask, UInt32 value, Int32 matchType,
	//						 Int32 timeout)

	CALLED_SETUP ("Err", "UInt32 evID, UInt32 mask, UInt32 value, Int32 matchType,"
							 "Int32 timeout");

	CALLED_GET_PARAM_VAL (Int32, timeout);

	CallROMType result;
	Bool		clearTimeout;

	EmPatchMgr::PuppetString (result, clearTimeout);

	// If timeout is infinite, the kernel wants 0.
	// If timeout is none, the kernel wants -1.

	if (clearTimeout && timeout == 0)
	{
		sub.SetParamVal ("timeout", (Int32) -1);
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysFatalAlert
 *
 * DESCRIPTION:	Intercept this and show the user a dialog.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SysFatalAlert (void)
{
	Preference<bool>	pref (kPrefKeyInterceptSysFatalAlert);
	if (!*pref)
	{
		// Palm OS will display a dialog with just a Reset button
		// in it.  So *always* turn off the Gremlin, as the user
		// won't be able to select "Continue".

		Hordes::Stop ();
		return kExecuteROM;
	}

	// UInt16 SysFatalAlert (const Char* msg)

	CALLED_SETUP ("UInt16", "const Char* msg");

	CALLED_GET_PARAM_STR (Char, msg);

	string	msgString;

	if (msg)
	{
		msgString = string (msg);
	}
	else
	{
		msgString = Platform::GetString (kStr_UnknownFatalError);
	}

	Errors::ReportErrSysFatalAlert (msgString.c_str ());

	// On Debug or Reset, an exception will be thrown past this point,
	// meaning that we don't have to return fatalEnterDebugger or fatalReset.
	// On the psuedo Next Gremlin, Hordes::ErrorEncountered will have been
	// called.  Otherwise, Continue and Next Gremlin end up coming here, and
	// we should return fatalDoNothing.

	PUT_RESULT_VAL (UInt16, fatalDoNothing);

	return kSkipROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysLaunchConsole
 *
 * DESCRIPTION:	Stub this function out so that it doesn't do anything.
 *				We completely handle the console in DebugMgr, so there's
 *				no need to get the ROM all heated up.  Also, there are
 *				problems with actually letting the ROM try to launch its
 *				console task, at least on the Mac.	It will try to open
 *				a serial port socket and do stuff with it.	That attempt
 *				will fail, as much of the serial port processing on the
 *				Mac is handled at idle time, and idle time processing is
 *				inhibited when handling debugger packets
 *				(SysLaunchConsole is usually called by a debugger via
 *				the RPC packet).  Since serial port processing doesn't
 *				occur, SysLaunchConsole hangs.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SysLaunchConsole (void)
{
	// Err SysLaunchConsole(void)

	CALLED_SETUP ("Err", "void");

	PUT_RESULT_VAL (Err, errNone);

	return kSkipROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysSemaphoreWait
 *
 * DESCRIPTION:	We patch SysSemaphoreWait as the mechanism for feeding
 *				the Palm OS new events. See comments in
 *				EmPatchState::PuppetString.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SysSemaphoreWait (void)
{
	// Only do this under 1.0.	Under Palm OS 2.0 and later, EvtGetSysEvent
	// calls SysEvGroupWait instead.  See our headpatch of that function
	// for a chunk of pretty similar code.

	if (EmPatchState::OSMajorVersion () != 1)
	{
		return kExecuteROM;
	}

	// Err SysSemaphoreWait(UInt32 smID, UInt32 priority, Int32 timeout)

	CALLED_SETUP ("Err", "UInt32 smID, UInt32 priority, Int32 timeout");

	CALLED_GET_PARAM_VAL (Int32, timeout);

	CallROMType result;
	Bool		clearTimeout;

	EmPatchMgr::PuppetString (result, clearTimeout);

	// If timeout is infinite, the kernel wants 0.
	// If timeout is none, the kernel wants -1.

	if (clearTimeout && timeout == 0)
	{
		sub.SetParamVal ("timeout", (Int32) -1);
	}

	return result;
}

#if 1
/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysTicksPerSecond
 *
 * DESCRIPTION:	SysTicksPerSecond is called from many places to calibrate
 *                              delays. On real hardware, the system runs at
 *				100 ticks per second, but the emulator usually runs
 *				significantly faster than that.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::SysTicksPerSecond(void)
{
	CALLED_SETUP ("UInt16", "void");

	PUT_RESULT_VAL (UInt16, sysTicksPerSecond);

	return kSkipROM;

}
#endif

/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::SysUIAppSwitch
 *
 * DESCRIPTION:	SysUIAppSwitch is called from the following locations
 *				for the given reasons.	When running Gremlins, we want
 *				to prevent SysUIAppSwitch from doing its job, which is
 *				to record information about the application to switch
 *				to and to then post an appStopEvent to the current
 *				application.
 *
 *				There are a couple of places where SysUIAppSwitch is
 *				called to quit and re-run the current application.
 *				Therefore, we want to stub out SysUIAppSwitch only when
 *				the application being switched to is not the currently
 *				running application.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

//	Places where this is called:
//
//	- LauncherMain.c (AppsViewSwitchApp) to launch new app.
//	- PrefApp (PilotMain) to launch a panel.
//	- SyncApp (prvLaunchExternalModule) to launch a "service owner"
//	- AppLaunchCmd.h (AppLaunchWithCommand) ???
//	- ModemPanel.h (ModemPanelLaunch) ???
//	- callApp.h (LaunchApp) ???
//	- ExgMgr.c (ExgNotifyReceive) to send a sysAppLaunchCmdGoTo message.
//	- GraffitiGlue.c (PrvLaunchGraffitiDemo) to launch Graffiti demo.
//	- Keyboard.c (PrvLaunchGraffitiDemo) to launch Graffiti demo.
//	- Launcher.c (LauncherFormHandleEvent) handles taps on launcher icons.
//	- SystemMgr.c (SysHandleEvent) to send sysAppLaunchCmdSystemLock
//	  in response to seeing a lockChr keyboard message.
//	- SystemMgr.c (SysHandleEvent) to switch apps in response to hard#Chr
//	  keyboard messages.
//	- Find.c (Find) to send sysAppLaunchCmdGoTo message.
//
//	- ButtonsPanel.c (ButtonsFormHandleEvent) switch to another panel.
//	- FormatsPanel.c (FormatsFormHandleEvent) switch to another panel.
//	- GeneralPanel.c (GeneralFormHandleEvent) switch to another panel.
//	- ModemPanel.c (ModemFormHandleEvent) switch to another panel.
//	- NetworkPanel.c (NetworkFormHandleEvent) switch to another panel.
//	- OwnerPanel.c (OwnerViewHandleEvent) switch to another panel.
//	- ShortCutsPanel.c (ShortCutFormHandleEvent) switch to another panel.

CallROMType SysHeadpatch::SysUIAppSwitch (void)
{
	// Err SysUIAppSwitch(UInt16 cardNo, LocalID dbID, UInt16 cmd, MemPtr cmdPBP)

	CALLED_SETUP ("Err", "UInt16 cardNo, LocalID dbID, UInt16 cmd, MemPtr cmdPBP");

	CALLED_GET_PARAM_VAL (UInt16, cardNo);
	CALLED_GET_PARAM_VAL (LocalID, dbID);
	CALLED_GET_PARAM_VAL (MemPtr, cmdPBP);

	if (false /*LogLaunchCodes ()*/)
	{
		char	name[dmDBNameLength] = {0};
		UInt32	type = 0;
		UInt32	creator = 0;

		/* Err	err =*/ ::DmDatabaseInfo (
				cardNo, dbID, name,
				NULL, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL,
				&type, &creator);

		LogAppendMsg ("SysUIAppSwitch called:");
		LogAppendMsg ("	cardNo:			%ld",		(long) cardNo);
		LogAppendMsg ("	dbID:			0x%08lX",	(long) dbID);
		LogAppendMsg ("		name:		%s",		name);
		LogAppendMsg ("		type:		%04lX",		type);
		LogAppendMsg ("		creator:	%04lX",		creator);
	}

	// We are headpatching SysUIAppSwitch; if we skip the ROM version, we
	// need to replicate at least this part of its functionality:
	//
	// If the last launch attempt failed, release the command parameter block, if
	//	any. When a launch succeeds, the UIAppShell will clear this global
	//	and free the chunk itself  when the app quits.
	{
		CEnableFullAccess	munge;	// Remove blocks on memory access.
		emuptr nextUIAppCmdPBP = EmLowMem_GetGlobal (nextUIAppCmdPBP);
		if (nextUIAppCmdPBP)
		{
			MemPtrFree((MemPtr) nextUIAppCmdPBP);
			EmLowMem_SetGlobal (nextUIAppCmdPBP, 0);
		}
	}

	// Get the current application card and db.  If we are attempting to switch
	// to the same app, allow it.

	UInt16	currentCardNo;
	LocalID currentDbID;
	Err 	err = SysCurAppDatabase (&currentCardNo, &currentDbID);

	// Got an error? Give up and let default handling occur.

	if (err != 0)
		return kExecuteROM;

	// Switching to current app; let default handling occur.

	if ((cardNo == currentCardNo) && (dbID == currentDbID))
		return kExecuteROM;

	// OK, we're switching to a different application.	If Gremlins is running
	// and we're trying to switch to an application that's not on its list,
	// then stub out SysUIAppSwitch to do nothing.

	if ((Hordes::IsOn () || EmEventPlayback::ReplayingEvents ()) &&
		!Hordes::CanSwitchToApp (cardNo, dbID))
	{
		// Store the incoming parameter block in nextUIAppCmdPBP, just
		// like the real version would.  This way, it will get cleaned
		// up by the code above, or by the return code in UIAppShell, or
		// by the real SysUIAppSwitch, or whatever.  By not freeing it
		// now, we preserve compatibility with any application that
		// expects the block to stay around for a while.

	//	CEnableFullAccess	munge;	// Remove blocks on memory access.
	//	EmLowMem_SetGlobal (nextUIAppCmdPBP, (MemPtr) cmdPBP);

		// Actually, let's not take that approach.  It looks to me that
		// it could lead to problems in UIAppShell.  When SysAppLaunch
		// returns, UIAppShell will try to free the block that previously
		// was in nextUIAppCmdPBP, which it had saved in a local variable.
		// So let's examing the following: UIAppShell launches an application,
		// saving the parameter block.  The next application tries calling
		// SysUIAppSwitch with a parameter block.  We save that parameter
		// block in nextUIAppCmdPBP and then keep the application running.
		// Then the application decides to quit for whatever reason.
		// UIAppShell will free any parameter it originally used to free
		// the application.  It then notices that the application just up
		// and quit, and so tries launching some default application (usually
		// the previous application).  However, in doing so, one of two
		// things can happen. (1) nextUIAppCmdPBP is set to NULL by UIAppShell
		// without first deleting any contents.  (2) The next application
		// is launched with its parameter block set to whatever is left over
		// in nextUIAppCmdPBP, which is now the parameter block to the application
		// we earlier prevented from running.
		//
		// Neither of those is correct, so let's see if we can just dump
		// the block now, and hope that no applications complain.

		MemPtr	p = (MemPtr) cmdPBP;
		if (p)
		{
			MemPtrFree (p);
		}

		PUT_RESULT_VAL (Err, errNone);
		return kSkipROM;
	}

	// Do the normal app switch.

	return kExecuteROM;
}


/***********************************************************************
 *
 * FUNCTION:	SysHeadpatch::TblHandleEvent
 *
 * DESCRIPTION:	See comments in SysTailpatch::TblHandleEvent.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType SysHeadpatch::TblHandleEvent (void)
{
	if (EmPatchState::HasSelectItemBug ())
	{
		gSaveDrawStateStackLevel = ::PrvGetDrawStateStackLevel ();
	}

	return kExecuteROM;
}


#pragma mark -

// ===========================================================================
//		¥ SysTailpatch
// ===========================================================================

/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::MarkUIObjects
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::MarkUIObjects (void)
{
	MetaMemory::MarkUIObjects ();
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::BmpCreate
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::BmpCreate (void)
{
	// BitmapType *BmpCreate (Coord width, Coord height, UInt8 depth, 
	//		ColorTableType *colortableP, UInt16 *error)

	CALLED_SETUP ("BitmapType*", "Coord width, Coord height, UInt8 depth"
		"ColorTableType *colortableP, UInt16 *error");

	GET_RESULT_PTR ();

	MetaMemory::RegisterBitmapPointer ((MemPtr) result);
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::BmpDelete
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::BmpDelete (void)
{
	// Err BmpDelete (BitmapType *bitmapP)

	CALLED_SETUP ("Err", "BitmapType *bitmapP");

	CALLED_GET_PARAM_VAL (MemPtr, bitmapP);

	MetaMemory::UnregisterBitmapPointer (bitmapP);
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::ClipboardAddItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::ClipboardAddItem (void)
{
	if (!Hordes::IsOn () && !gDontPatchClipboardAddItem)
	{
		::PrvCopyPalmClipboardToHost ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::ClipboardAppendItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::ClipboardAppendItem (void)
{
	if (!Hordes::IsOn () && !gDontPatchClipboardAddItem)
	{
		::PrvCopyPalmClipboardToHost ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::DmGet1Resource
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::DmGet1Resource (void)
{
	// MemHandle DmGet1Resource (DmResType type, DmResID id)

	CALLED_SETUP ("MemHandle", "DmResType type, DmResID id");

	CALLED_GET_PARAM_VAL (DmResType, type);
//	CALLED_GET_PARAM_VAL (DmResID, id);

#define iconType	'tAIB'
#define bitmapRsc 	'Tbmp'

	if (type == iconType || type == bitmapRsc)
	{
		GET_RESULT_PTR ();
		MetaMemory::RegisterBitmapHandle ((MemHandle) result);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::DmGetResource
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::DmGetResource (void)
{
	// MemHandle DmGetResource (DmResType type, DmResID id)

	CALLED_SETUP ("MemHandle", "DmResType type, DmResID id");

	CALLED_GET_PARAM_VAL (DmResType, type);
//	CALLED_GET_PARAM_VAL (DmResID, id);

	if (type == iconType || type == bitmapRsc)
	{
		GET_RESULT_PTR ();
		MetaMemory::RegisterBitmapHandle ((MemHandle) result);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::EvtGetEvent
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::EvtGetEvent (void)
{
	// void EvtGetEvent(const EventPtr eventP, Int32 timeout);

	CALLED_SETUP ("void", "const EventPtr eventP, Int32 timeout");

	CALLED_GET_PARAM_REF (EventType, eventP, Marshal::kInput);
	CALLED_GET_PARAM_VAL (Int32, timeout);

	LogEvtGetEvent (*eventP, timeout);

	EmPatchState::SetEvtGetEventCalled (true);
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::EvtGetPen
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::EvtGetPen (void)
{
	// void EvtGetPen(Int16 *pScreenX, Int16 *pScreenY, Boolean *pPenDown)

	CALLED_SETUP ("void", "Int16 *pScreenX, Int16 *pScreenY, Boolean *pPenDown");

	CALLED_GET_PARAM_REF (Int16, pScreenX, Marshal::kInput);
	CALLED_GET_PARAM_REF (Int16, pScreenY, Marshal::kInput);
	CALLED_GET_PARAM_REF (Boolean, pPenDown, Marshal::kInput);

	LogEvtGetPen (*pScreenX, *pScreenY, *pPenDown);
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::EvtGetSysEvent
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::EvtGetSysEvent (void)
{
	// void EvtGetSysEvent(EventPtr eventP, Int32 timeout)

	CALLED_SETUP ("void", "EventPtr eventP, Int32 timeout");

	CALLED_GET_PARAM_REF (EventType, eventP, Marshal::kInput);
	CALLED_GET_PARAM_VAL (Int32, timeout);

	LogEvtGetSysEvent (*eventP, timeout);
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::EvtSysEventAvail
 *
 * DESCRIPTION:	If there are no pending events in the real system,
 *				check to see if we have any pen events to insert.
 *				Set the function's return value to TRUE if so.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::EvtSysEventAvail (void)
{
	// Boolean EvtSysEventAvail(Boolean ignorePenUps)

	CALLED_SETUP ("Boolean", "Boolean ignorePenUps");

	CALLED_GET_PARAM_VAL (Boolean, ignorePenUps);
	GET_RESULT_VAL (Boolean);

	if (result == 0)
	{
		EmAssert (gSession);
		if (gSession->HasPenEvent ())
		{
			EmPenEvent	event = gSession->PeekPenEvent ();

			if (event.fPenIsDown || !ignorePenUps)
			{
				PUT_RESULT_VAL (Boolean, true);
			}
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::ExgReceive
 *
 * DESCRIPTION:	Log Exchange Manager data.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::ExgReceive (void)
{
	// UInt32 ExgReceive (ExgSocketPtr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP)

	CALLED_SETUP ("UInt32", "ExgSocketPtr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP");

	CALLED_GET_PARAM_VAL (UInt32, bufLen);
	CALLED_GET_PARAM_PTR (void, bufP, bufLen, Marshal::kInput);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);
	GET_RESULT_VAL (UInt32);

	if (LogExgMgr ())
	{
		LogAppendMsg ("ExgMgr: received %ld bytes, err = 0x%04X.", result, *errP);
	}
	else if (LogExgMgrData ())
	{
		LogAppendData ((void*) bufP, bufLen, "ExgMgr: received %ld bytes, err = 0x%04X.", result, *errP);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::ExgSend
 *
 * DESCRIPTION:	Log Exchange Manager data.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::ExgSend (void)
{
	// UInt32 ExgSend (ExgSocketPtr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP) = 0;

	CALLED_SETUP ("UInt32", "ExgSocketPtr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP");

	CALLED_GET_PARAM_VAL (UInt32, bufLen);
	CALLED_GET_PARAM_PTR (void, bufP, bufLen, Marshal::kInput);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);
	GET_RESULT_VAL (UInt32);

	if (LogExgMgr ())
	{
		LogAppendMsg ("ExgMgr: sent %ld bytes, err = 0x%04X.", result, *errP);
	}
	else if (LogExgMgrData ())
	{
		LogAppendData ((void*) bufP, bufLen, "ExgMgr: sent %ld bytes, err = 0x%04X.", result, *errP);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::FtrInit
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::FtrInit (void)
{
	// void FtrInit(void)

	// Get information about the current OS so that we know
	// what features are implemented (for those cases when we
	// can't make other tests, like above where we test for
	// the existance of a trap before calling it).

	// Read the version into a local variable; the ROM Stub facility
	// automatically maps local variables into Palm space so that ROM
	// functions can get to them.  If we were to pass &gOSVersion,
	// the DummyBank functions would complain about an invalid address.

	UInt32	value;
	Err 	err = ::FtrGet (sysFtrCreator, sysFtrNumROMVersion, &value);

	if (err == errNone)
	{
		EmPatchState::SetOSVersion(value);
	}
	else
	{
		EmPatchState::SetOSVersion(kOSUndeterminedVersion);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::FtrSet
 *
 * DESCRIPTION:	Look for calls indicating that we're running a
 *				non-Roman system
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::FtrSet (void)
{
	// Err FtrSet (UInt32 creator, UInt16 featureNum, UInt32 newValue)

	CALLED_SETUP ("Err", "UInt32 creator, UInt16 featureNum, UInt32 newValue");

	CALLED_GET_PARAM_VAL (UInt32, creator);
	CALLED_GET_PARAM_VAL (UInt16, featureNum);
	CALLED_GET_PARAM_VAL (UInt32, newValue);
	GET_RESULT_VAL (Err);

	// Look for calls indicating that we're running a non-Roman system.

	if (result == errNone)
	{
		if (creator == sysFtrCreator && featureNum == sysFtrNumEncoding)
		{
			EmPatchState::SetEncoding (newValue);
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::HwrMemReadable
 *
 * DESCRIPTION:	Patch this function so that it returns non-zero if the
 *				address is in the range of memory that we've mapped in
 *				to emulated space.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::HwrMemReadable (void)
{
	// UInt32 HwrMemReadable(MemPtr address)

	CALLED_SETUP ("UInt32", "MemPtr address");

	CALLED_GET_PARAM_VAL (emuptr, address);
	GET_RESULT_VAL (UInt32);

	if (result == 0)
	{
		void*	addrStart;
		uint32	addrLen;

		Memory::GetMappingInfo (address, &addrStart, &addrLen);

		PUT_RESULT_VAL (UInt32, addrLen);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::HwrSleep
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::HwrSleep (void)
{
	// void HwrSleep(Boolean untilReset, Boolean emergency)

	MetaMemory::MarkLowMemory (offsetof (M68KExcTableType, busErr),
							   offsetof (M68KExcTableType, busErr) + sizeof (emuptr));

	MetaMemory::MarkLowMemory (offsetof (M68KExcTableType, addressErr),
							   offsetof (M68KExcTableType, addressErr) + sizeof (emuptr));

	MetaMemory::MarkLowMemory (offsetof (M68KExcTableType, illegalInstr),
							   offsetof (M68KExcTableType, illegalInstr) + sizeof (emuptr));

	MetaMemory::MarkLowMemory (offsetof (M68KExcTableType, autoVec1),
							   offsetof (M68KExcTableType, trapN[0]));

	// On Palm OS 1.0, there was code in HwrSleep that would execute only if
	// memory location 0xC2 was set.  It looks like debugging code (that is,
	// Ron could change it from the debugger to try different things out),
	// but we still have to allow access to it.

	if (EmPatchState::OSMajorVersion () == 1)
	{
		MetaMemory::MarkLowMemory (0x00C2, 0x00C3);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::SysAppStartup
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::SysAppStartup (void)
{
	// Err SysAppStartup(SysAppInfoPtr* appInfoPP, MemPtr* prevGlobalsP, MemPtr* globalsPtrP)

	CALLED_SETUP ("Err", "SysAppInfoPtr* appInfoPP, MemPtr* prevGlobalsP, MemPtr* globalsPtrP");

	CALLED_GET_PARAM_REF (SysAppInfoPtr, appInfoPP, Marshal::kInput);
	GET_RESULT_VAL (Err);

	if (result != errNone)
		return;

	if (appInfoPP == EmMemNULL)
		return;

	emuptr appInfoP = EmMemGet32 (appInfoPP);
	if (!appInfoP)
		return;

	if (false /*LogLaunchCodes ()*/)
	{
		emuptr	dbP		= EmMemGet32 (appInfoP + offsetof (SysAppInfoType, dbP));
		emuptr	openP	= EmMemGet32 (dbP + offsetof (DmAccessType, openP));
		LocalID	dbID	= EmMemGet32 (openP + offsetof (DmOpenInfoType, hdrID));
		UInt16	cardNo	= EmMemGet16 (openP + offsetof (DmOpenInfoType, cardNo));

		char	name[dmDBNameLength] = {0};
		UInt32	type = 0;
		UInt32	creator = 0;

		/* Err	err =*/ ::DmDatabaseInfo (
				cardNo, dbID, name,
				NULL, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL,
				&type, &creator);

		LogAppendMsg ("SysAppStartup called:");
		LogAppendMsg ("	cardNo:			%ld",		(long) cardNo);
		LogAppendMsg ("	dbID:			0x%08lX",	(long) dbID);
		LogAppendMsg ("		name:		%s",		name);
		LogAppendMsg ("		type:		%04lX",		type);
		LogAppendMsg ("		creator:	%04lX",		creator);
	}

	Int16 cmd = EmMemGet16 (appInfoP + offsetof (SysAppInfoType, cmd));

	EmuAppInfo	newAppInfo;
	EmPatchState::CollectCurrentAppInfo (appInfoP, newAppInfo);

	newAppInfo.fCmd = cmd;
	EmPatchState::AddAppInfo(newAppInfo);

	if (cmd == sysAppLaunchCmdNormalLaunch)
	{
		// Clear the flags that tell us which warnings we've already issued.
		// We reset these flags any time a new application is launched.
		//
		// !!! Put these flags into EmuAppInfo?  That way, we can keep
		// separate sets for sub-launched applications.

//		Errors::ClearWarningFlags ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::SysBinarySearch
 *
 * DESCRIPTION:	There's a bug in pre-3.0 versions of SysBinarySearch
 *				that cause it to call the user callback function with a
 *				pointer just past the array to search.	Make a note of
 *				when we enter SysBinarySearch so that we can make
 *				allowances for that in MetaMemory's memory checking.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::SysBinarySearch (void)
{
	EmPatchState::ExitSysBinarySearch ();
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::SysTaskCreate
 *
 * DESCRIPTION:	Track calls to SysTaskCreate to get information on any
 *				stacks created.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::SysTaskCreate (void)
{
	// Err		SysTaskCreate(UInt32 * taskIDP, UInt32 * creator, ProcPtr codeP,
	//						MemPtr stackP, UInt32 stackSize, UInt32 attr, UInt32 priority,
	//						UInt32 tSlice)

	CALLED_SETUP ("Err", "UInt32 * taskIDP, UInt32 * creator, ProcPtr codeP,"
					"MemPtr stackP, UInt32 stackSize, UInt32 attr, UInt32 priority,"
					"UInt32 tSlice");

	CALLED_GET_PARAM_VAL (emuptr, stackP);
	CALLED_GET_PARAM_VAL (UInt32, stackSize);
	GET_RESULT_VAL (Err);

	if (result == errNone)
	{
		StackRange	range (stackP, stackP + stackSize);
		EmPalmOS::RememberStackRange (range);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::TblHandleEvent
 *
 * DESCRIPTION:	There's a bug in a private Table Manager function called
 *				SelectItem (that's the name of the function, not the
 *				bug).  Under certain circumstances, the function will
 *				exit early without popping the draw state that it pushed
 *				onto the stack at the start of the function.  This
 *				tailpatch remedies that problem.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::TblHandleEvent (void)
{
	if (EmPatchState::HasSelectItemBug ())
	{
		long	curDrawStateStackLevel = ::PrvGetDrawStateStackLevel ();
		long	levelDelta = curDrawStateStackLevel - gSaveDrawStateStackLevel;

		if (levelDelta == 1)
		{
			WinPopDrawState ();
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::TimInit
 *
 * DESCRIPTION:	TimInit is where a special boolean is set to trigger a
 *				bunch of RTC bug workarounds in the ROM (that is, there
 *				are RTC bugs in the Dragonball that the ROM needs to
 *				workaround).  We're not emulating those bugs, so turn
 *				off that boolean.  Otherwise, we'd have to add support
 *				for the 1-second, 1-minute, and 1-day RTC interrupts in
 *				order to get those workarounds to work.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::TimInit (void)
{
	// Err TimInit(void)

	// Turn off the RTC bug workaround flag.

	CEnableFullAccess	munge;

	emuptr	timGlobalsP = EmLowMem_GetGlobal (timGlobalsP);
	EmAliasTimGlobalsType<PAS>	timGlobals (timGlobalsP);

	if (timGlobals.rtcBugWorkaround == 0x01)
	{
		timGlobals.rtcBugWorkaround = 0;
	}

	// Set the current date.

	::PrvSetCurrentDate ();
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::UIInitialize
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::UIInitialize (void)
{
	// Void UIInitialize (void)

	EmPatchState::SetUIInitialized (true);

	// Reconfirm the strict intl checks setting, whether on or off.

	Preference<Bool> intlPref (kPrefKeyReportStrictIntlChecks);

	if (EmPatchMgr::IntlMgrAvailable ())
	{
		::IntlSetStrictChecks (*intlPref);
	}

	// Reconfirm the overlay checks setting, whether on or off.

	Preference<Bool> overlayPref (kPrefKeyReportOverlayErrors);
	(void) ::FtrSet (omFtrCreator, omFtrShowErrorsFlag, *overlayPref);

	// Prevent the device from going to sleep.

	::SysSetAutoOffTime (0);

	// Can't call PrefSetPreference on 1.0 devices....

	if (EmLowMem::TrapExists (sysTrapPrefSetPreference))
	{
		::PrefSetPreference (prefAutoOffDuration, 0);
	}

	// Install a 'pose' feature so that people can tell if they're running
	// under the emulator or not.  We *had* added a HostControl function
	// for this, but people can call that only under Poser or under 3.0
	// and later ROMs.	Which means that there's no way to tell under 2.0
	// and earlier ROMs when running on an actual device.
	//
	// Note that we do this here instead of in a tailpatch to FtrInit because
	// of a goofy inter-dependency.  FtrInit is called early in the boot process
	// before a valid stack has been allocated and switched to. During this time,
	// the ROM is using a range of memory that happens to reside in a free
	// memory chunk.  Routines in MetaMemory know about this and allow the use
	// of this unallocate range of memory.	However, in order to work, they need
	// to know the current stack pointer value.  If we were to call FtrSet from
	// the FtrInit tailpatch, the stack pointer will point to the stack set up
	// by the ATrap object, not the faux stack in use at the time of FtrInit.
	// Thus, the MetaMemory routines get confused and end up getting in a state
	// where accesses to the temporary OS stack are flagged as invalid.  Hence,
	// we defer adding these Features until much later in the boot process (here).

	::FtrSet ('pose', 0, 0);

	// If we're listening on a socket, install the 'gdbS' feature.	The
	// existance of this feature causes programs written with the prc tools
	// to enter the debugger when they're launched.

	if (Debug::ConnectedToTCPDebugger ())
	{
		::FtrSet ('gdbS', 0, 0x12BEEF34);
	}

	// Install the HotSync user-name.

	Preference<string>	userName (kPrefKeyUserName);
	::SetHotSyncUserName (userName->c_str ());

	// Auto-load any files in the Autoload[Foo] directories.

	::PrvAutoload ();

	// Install our ExgMgr driver

	// Don't do this for now.  We don't currently use it, and people get
	// confused seeing it show up in application lists, etc.

//	EmFileImport::InstallExgMgrLib ();
}


/***********************************************************************
 *
 * FUNCTION:	SysTailpatch::UIReset
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void SysTailpatch::UIReset (void)
{
	// Void UIReset (void)

	EmPatchState::SetUIReset (true);
}


#pragma mark -


/***********************************************************************
 *
 * FUNCTION:	PrvAutoload
 *
 * DESCRIPTION:	Install the files in the various Autoload directories.
 *				If there are any designated to be run, pick on to run.
 *				If the emulator should exit when the picked application
 *				quits, schedule it to do so.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void PrvAutoload (void)
{
	// Load all the files in one blow.

	EmFileRefList	fileList;
	Startup::GetAutoLoads (fileList);

	vector<LocalID> idList;
	EmFileImport::LoadPalmFileList (fileList, kMethodHomebrew, idList);

	// Get the application to switch to (if any);

	string	appToRun = Startup::GetAutoRunApp ();

	if (!appToRun.empty())
	{
		char	name[dmDBNameLength];
		strcpy (name, appToRun.c_str());

		UInt16	cardNo = 0;
		LocalID	dbID = ::DmFindDatabase (cardNo, name);

		if (dbID != 0)
		{
			EmPatchState::SetSwitchApp (cardNo, dbID);

			// If we're supposed to quit after running this application,
			// start that process in motion.

			if (Startup::QuitOnExit ())
			{
				EmPatchState::SetQuitApp (cardNo, dbID);
			}
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvSetCurrentDate
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void PrvSetCurrentDate (void)
{
	CEnableFullAccess	munge;

	// Get the current date.

	long	year, month, day;
	::GetHostDate (&year, &month, &day);

	// Convert it to days -- and then hourse -- since 1/1/1904

	UInt32	rtcHours = ::DateToDays (year, month, day) * 24;

	// Update the "hours adjustment" value to contain the current date.

	emuptr	timGlobalsP = EmLowMem_GetGlobal (timGlobalsP);
	EmAliasTimGlobalsType<PAS>	timGlobals (timGlobalsP);

	timGlobals.rtcHours = rtcHours;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetDrawStateStackLevel
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

long PrvGetDrawStateStackLevel (void)
{
	CEnableFullAccess	munge;
	UInt16				level = EmLowMem_GetGlobal (uiGlobals.gStateV3.drawStateIndex);
	return level;
}


/***********************************************************************
 *
 * FUNCTION:	PrvCopyPalmClipboardToHost
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void PrvCopyPalmClipboardToHost (void)
{
	gDontPatchClipboardGetItem = true;

	UInt16		length;
	MemHandle	dataHdl = ::ClipboardGetItem (clipboardText, &length);

	gDontPatchClipboardGetItem = false;

	if (length > 0)
	{
		MemPtr	dataPtr = ::MemHandleLock (dataHdl);
		if (dataPtr)
		{
			void*	dataCopy = Platform::AllocateMemory (length);
			if (dataCopy)
			{
				EmMem_memcpy (dataCopy, (emuptr) dataPtr, length);

				ByteList	palmChars;
				ByteList	hostChars;
				char*		charData = (char*) dataCopy;

				copy (charData, charData + length, back_inserter (palmChars));

				Platform::CopyToClipboard (palmChars, hostChars);
				Platform::DisposeMemory (dataCopy);
			}

			::MemHandleUnlock (dataHdl);
		}
	}
	else
	{
		ByteList	palmChars;
		ByteList	hostChars;

		Platform::CopyToClipboard (palmChars, hostChars);
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvCopyHostClipboardToPalm
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void PrvCopyHostClipboardToPalm (void)
{
	ByteList	palmChars;
	ByteList	hostChars;

	Platform::CopyFromClipboard (palmChars, hostChars);

	gDontPatchClipboardAddItem = true;

	if (palmChars.size () > 0)
	{
		// Limit us to cbdMaxTextLength characters so as to not
		// overburden the dynamic heap.  This is the same limit used by
		// the Field Manager does when calling ClipboardAddItem (except
		// that the Field Manager puts up a dialog saying that it won't
		// copy the text to the clipboard instead of truncating).

		if (palmChars.size () > cbdMaxTextLength)
		{
			palmChars.resize (cbdMaxTextLength);
		}

		// Set the Palm OS clipboard to the text data.

		::ClipboardAddItem (clipboardText, &palmChars[0], palmChars.size ());
	}
	else
	{
		::ClipboardAddItem (clipboardText, NULL, 0);
	}

	gDontPatchClipboardAddItem = false;
}
