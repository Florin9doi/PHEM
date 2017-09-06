/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	Copyright (c) 2001 PocketPyro, Inc.

	Portions Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmPatchState.h"

#include "EmApplication.h"		// ScheduleQuit
#include "EmMemory.h"			// EmMem_strcpy
#include "Miscellaneous.h"		// SysTrapIndex, SystemCallContext
#include "PreferenceMgr.h"		// Preference, kPrefKeyReportMemMgrLeaks
#include "ROMStubs.h"

/* Update for recent GCC */
#include <cstddef>

EmStream& operator >> (EmStream& inStream, EmStackFrame& outInfo);
EmStream& operator << (EmStream& inStream, const EmStackFrame& inInfo);
EmStream& operator >> (EmStream& inStream, EmTrackedChunk& outInfo);
EmStream& operator << (EmStream& inStream, const EmTrackedChunk& inInfo);


// STATIC class data:

EmPatchStateData EmPatchState::fgData;


Err EmPatchState::Initialize (void)
{
	// Add a notification for MemMgrLeaks

	gPrefs->AddNotification (&EmPatchState::MemMgrLeaksPrefsChanged, kPrefKeyReportMemMgrLeaks);

	return errNone;
}


Err EmPatchState::Dispose (void)
{
	fgData.fUIInitialized		= false;
	fgData.fUIReset				= false;
	fgData.fHeapInitialized		= false;
	fgData.fEvtGetEventCalled	= false;
	fgData.fEncoding			= charEncodingUnknown;
		
	return errNone;
}


Err EmPatchState::Reset (void)
{
	fgData.fMemMgrCount				= 0;
	fgData.fMemSemaphoreCount		= 0;
	fgData.fMemSemaphoreReserveTime	= 0;
	fgData.fResizeOrigSize 			= 0;
	fgData.fHeapID 					= 0;

	fgData.fRememberedHeaps.clear ();
	fgData.fTrackedChunks.clear ();

	fgData.fLastEvtTrap				= 0;
	fgData.fOSVersion				= kOSUndeterminedVersion;
	
	fgData.fEvtGetEventCalled		= false;
	fgData.fEncoding				= charEncodingUnknown;
	fgData.fSysBinarySearchCount	= 0;

	fgData.fUIInitialized			= false;
	fgData.fUIReset					= false;
	fgData.fHeapInitialized			= false;


	SetQuitApp (0, 0);
	SetSwitchApp (0, 0);

	// Clear out everything we know about the current applications.

	fgData.fCurAppInfo.clear ();

	// Cache the preference about checking for memory manager leaks.

	Preference<Bool>	pref (kPrefKeyReportMemMgrLeaks);
	fgData.fMemMgrLeaks = pref.Loaded () && *pref;

	return errNone;
}



Err EmPatchState::Save (EmStreamChunk &s, long streamFmtVer, PersistencePhase pass)
{
	UNUSED_PARAM (streamFmtVer);
	
	if (pass == PSPersistAll || pass == PSPersistStep1)
	{
		s << fgData.fUIInitialized;
		s << fgData.fHeapInitialized;
		s << fgData.fSysBinarySearchCount;

		s << fgData.fMemMgrCount;
		s << fgData.fMemSemaphoreCount;
		s << fgData.fMemSemaphoreReserveTime;
		s << fgData.fResizeOrigSize;
		s << fgData.fHeapID;

		s << fgData.fNextAppCardNo;
		s << fgData.fNextAppDbID;
		s << (long) 0;	// was gQuitStage;

		SaveCurAppInfo (s);
	}

	if (pass == PSPersistAll || pass == PSPersistStep2)
	{
		s << fgData.fLastEvtTrap;
		s << fgData.fOSVersion;

		// Added in version 2.

		s << false;	// was: gJapanese

		// Added in version 3.

		s << fgData.fEncoding;

		// Added in version 4.

		s << fgData.fUIReset;

		// Added in version 5

		s << fgData.fTrackedChunks;
	}

	return errNone;
}
	

Err EmPatchState::Load (EmStreamChunk &s, long streamFmtVer, PersistencePhase pass)
{
	if (pass == PSPersistAll || pass == PSPersistStep1)
	{
		long unused;

		s >> fgData.fUIInitialized;
		s >> fgData.fHeapInitialized;

		SetEvtGetEventCalled (false);


		s >> fgData.fSysBinarySearchCount;

		s >> fgData.fMemMgrCount;
		s >> fgData.fMemSemaphoreCount;
		s >> fgData.fMemSemaphoreReserveTime;
		s >> fgData.fResizeOrigSize;
		s >> fgData.fHeapID;

		s >> fgData.fNextAppCardNo;
		s >> fgData.fNextAppDbID;
		s >> unused;	// was gQuitStage;

		EmPatchState::LoadCurAppInfo (s);
	}

	if (pass == PSPersistAll || pass == PSPersistStep2)
	{
		if (streamFmtVer >= 1)
		{
			s >> fgData.fLastEvtTrap;
			s >> fgData.fOSVersion;
		}

		if (streamFmtVer >= 2)
		{
			bool dummy;
			s >> dummy;	// was: gJapanese
		}

		if (streamFmtVer >= 3)
		{
			s >> fgData.fEncoding;
		}
		else
		{
			fgData.fEncoding = charEncodingUnknown;
		}

		if (streamFmtVer >= 4)
		{
			s >> fgData.fUIReset;
		}

		if (streamFmtVer >= 5)
		{
			s >> fgData.fTrackedChunks;
		}
	}

	// Cache the preference about checking for memory manager leaks.

	Preference<Bool>	pref (kPrefKeyReportMemMgrLeaks);
	fgData.fMemMgrLeaks = pref.Loaded () && *pref;

	return errNone;
}




Err EmPatchState::SaveCurAppInfo (EmStreamChunk &s)
{
	s << (long) fgData.fCurAppInfo.size ();

	EmuAppInfoList::iterator	iter;
	for (iter = fgData.fCurAppInfo.begin (); iter != fgData.fCurAppInfo.end (); ++iter)
	{
		s << iter->fCmd;
		s << (long) iter->fDB;
		s << iter->fCardNo;
		s << iter->fDBID;
		s << iter->fMemOwnerID;
		s << iter->fStackP;
		s << iter->fStackEndP;
		s << iter->fStackSize;
		s << iter->fName;
		s << iter->fVersion;
	}

	return errNone;
}

Err EmPatchState::LoadCurAppInfo (EmStreamChunk &s)
{
	fgData.fCurAppInfo.clear ();

	long	numApps;
	
	s >> numApps;

	long	ii;
	
	for (ii = 0; ii < numApps; ++ii)
	{
		EmuAppInfo	info;

		s >> info.fCmd;
		
		long		temp;

		s >> temp;	
		info.fDB = (DmOpenRef) temp;
		
		
		s >> info.fCardNo;
		s >> info.fDBID;
		s >> info.fMemOwnerID;
		s >> info.fStackP;
		s >> info.fStackEndP;
		s >> info.fStackSize;
		s >> info.fName;
		s >> info.fVersion;

		fgData.fCurAppInfo.push_back (info);
	}

	return errNone;
}

Err EmPatchState::AddAppInfo (EmuAppInfo &newAppInfo)
{
	fgData.fCurAppInfo.push_back (newAppInfo);
	return errNone;
}

Err EmPatchState::RemoveCurAppInfo (void)
{
	fgData.fCurAppInfo.pop_back ();
	return errNone;
}
	
	
	
Err EmPatchState::SetLastEvtTrap (uint16 lastEvtTrap)
{
	fgData.fLastEvtTrap = lastEvtTrap;
	
	return errNone;
}

uint16 EmPatchState::GetLastEvtTrap (void)
{
	return fgData.fLastEvtTrap;
}

Err EmPatchState::SetEvtGetEventCalled (bool wasCalled)
{
	fgData.fEvtGetEventCalled = wasCalled;

	return errNone;
}

Err EmPatchState::SetQuitApp (UInt16 cardNo, LocalID dbID)
{
	fgData.fTimeToQuit		= false;
	fgData.fQuitAppCardNo	= cardNo;
	fgData.fQuitAppDbID		= dbID;

	return errNone;
}

void EmPatchState::SetTimeToQuit (void)
{
	gApplication->ScheduleQuit ();
}
	



// ---------------------------------------------------------------------------
//		¥ EmPatchState::MemMgrLeaksPrefsChanged
// ---------------------------------------------------------------------------
// Respond to a preference change.

void EmPatchState::MemMgrLeaksPrefsChanged (PrefKeyType, void*)
{
	Preference<Bool> pref (kPrefKeyReportMemMgrLeaks, false);
	fgData.fMemMgrLeaks = *pref;
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::OSVersion
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

UInt32 EmPatchState::OSVersion (void)
{
	EmAssert (fgData.fOSVersion != kOSUndeterminedVersion);

	return fgData.fOSVersion;
}

void EmPatchState::SetOSVersion (UInt32 version)
{
	fgData.fOSVersion = version;
}

/***********************************************************************
*
* FUNCTION:	EmPatchState::OSMajorMinorVersion
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

UInt32 EmPatchState::OSMajorMinorVersion (void)
{
	return OSMajorVersion () * 10 + OSMinorVersion ();
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::OSMajorVersion
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

UInt32 EmPatchState::OSMajorVersion (void)
{
	EmAssert (fgData.fOSVersion != kOSUndeterminedVersion);

	return sysGetROMVerMajor (fgData.fOSVersion);
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::OSMinorVersion
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

UInt32 EmPatchState::OSMinorVersion (void)
{
	EmAssert (fgData.fOSVersion != kOSUndeterminedVersion);

	return sysGetROMVerMinor (fgData.fOSVersion);
}


/***********************************************************************
 *
 * FUNCTION:	SetSwitchApp
 *
 * DESCRIPTION:	Sets an application or launchable document to switch to
 *				the next time the system can manage it.
 *
 * PARAMETERS:	cardNo - the card number of the app to switch to.
 *
 *				dbID - the database id of the app to switch to.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void EmPatchState::SetSwitchApp (UInt16 cardNo, LocalID dbID)
{
	fgData.fNextAppCardNo = cardNo;
	fgData.fNextAppDbID = dbID;
}


Bool EmPatchState::DatabaseInfoHasStackInfo (void)
{
	return OSMajorVersion () <= 2; 
}	// Changed in 3.0

	
/***********************************************************************
*
* FUNCTION:	EmPatchState::HasWellBehavedMemSemaphoreUsage
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

Bool EmPatchState::HasWellBehavedMemSemaphoreUsage (void)
{
	// Palm OS 3.0 and later should not be holding the memory manager
	// semaphore for longer than 1 minute.	I imagine that older ROMs
	// don't hold the semaphore for longer than this, but Roger still
	// suggested testing for 3.0.

	return fgData.fOSVersion != kOSUndeterminedVersion && OSMajorMinorVersion () >= 30;
}


// Bugs fixed in 3.0.
Bool EmPatchState::HasFindShowResultsBug (void)
{
	return OSMajorVersion () <= 2; 
}

Bool EmPatchState::HasSysBinarySearchBug (void)
{
	return OSMajorVersion () <= 2; 
}

// Bugs fixed in 3.1
Bool EmPatchState::HasGrfProcessStrokeBug (void)
{ 
	return OSMajorMinorVersion () <= 30; 
}

Bool EmPatchState::HasMenuHandleEventBug (void)
{
	return OSMajorMinorVersion () <= 30; 
}

// Bugs fixed in 3.2
Bool EmPatchState::HasBackspaceCharBug (void)
{
	return OSMajorMinorVersion () <= 31; 
}

Bool EmPatchState::HasDeletedStackBug (void)
{
	return OSMajorMinorVersion () <= 31; 
}

Bool EmPatchState::HasFindSaveFindStrBug (void)
{
	return OSMajorMinorVersion () <= 31; 
}

Bool EmPatchState::HasFldDeleteBug (void)
{
	return OSMajorMinorVersion () <= 31; 
}

Bool EmPatchState::HasFntDefineFontBug (void)
{
	return OSMajorMinorVersion () <= 31; 
}

Bool EmPatchState::HasNetPrvTaskMainBug (void)
{
	return OSMajorMinorVersion () <= 31; 
}

Bool EmPatchState::HasNetPrvSettingSetBug (void)
{
	return OSMajorMinorVersion () <= 31; 
}

// Bugs fixed in 3.3
Bool EmPatchState::HasECValidateFieldBug (void)
{
	return OSMajorMinorVersion () <= 32; 
}

// Bugs fixed in 3.5
Bool EmPatchState::HasConvertDepth1To2BWBug (void)
{ 
	return OSMajorMinorVersion () <= 33; 
}

// Bugs fixed in 4.0
Bool EmPatchState::HasSelectItemBug (void)
{ 
	return OSMajorMinorVersion () == 35; 
}

Bool EmPatchState::HasSyncNotifyBug (void)
{
	return OSMajorVersion () <= 3; 
}

// Bugs fixed in ???
Bool EmPatchState::HasPrvDrawSliderControlBug (void)
{ 
	return OSMajorMinorVersion () <= 40; 
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::GetAutoAcceptBeamDialogs
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

Bool EmPatchState::GetAutoAcceptBeamDialogs (void)
{
	return fgData.fAutoAcceptBeamDialogs;
}

	
/***********************************************************************
*
* FUNCTION:	EmPatchState::AutoAcceptBeamDialogs
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

Bool EmPatchState::AutoAcceptBeamDialogs (Bool newState)
{
	bool oldState = fgData.fAutoAcceptBeamDialogs;
	fgData.fAutoAcceptBeamDialogs = newState != 0;
	return oldState;
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::GetROMCharEncoding (was TurningJapanese)
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	Palm OS character encoding used by ROM.
*
***********************************************************************/

UInt16 EmPatchState::GetROMCharEncoding (void)
{
	return fgData.fEncoding;
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::EvtGetEventCalled
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

Bool EmPatchState::EvtGetEventCalled (void)
{
	return fgData.fEvtGetEventCalled;
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::IsInSysBinarySearch
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

Bool EmPatchState::IsInSysBinarySearch (void)
{
	return fgData.fSysBinarySearchCount > 0;
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::EnterSysBinarySearch
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

void EmPatchState::EnterSysBinarySearch (void)
{
	EmAssert (fgData.fSysBinarySearchCount < 10);
	++fgData.fSysBinarySearchCount;
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::ExitSysBinarySearch
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

void EmPatchState::ExitSysBinarySearch (void)
{
	--fgData.fSysBinarySearchCount;
	EmAssert (fgData.fSysBinarySearchCount >= 0);
}



uint16 EmPatchState::GetQuitAppCardNo (void)
{
	return fgData.fQuitAppCardNo;
}


LocalID EmPatchState::GetQuitAppDbID (void)
{
	return fgData.fQuitAppDbID;
}

void EmPatchState::SetEncoding (uint32 encoding)
{
	fgData.fEncoding = encoding;
}

Bool EmPatchState::IsPCInMemMgr (void)
{
	return fgData.fMemMgrCount > 0; 
}
	
/***********************************************************************
*
* FUNCTION:	EmPatchState::EnterMemMgr
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

void EmPatchState::EnterMemMgr (const char* fnName)
{
	UNUSED_PARAM (fnName)

	++fgData.fMemMgrCount;
	EmAssert (fgData.fMemMgrCount < 10);
}


/***********************************************************************
*
* FUNCTION:	EmPatchState::ExitMemMgr
*
* DESCRIPTION:	
*
* PARAMETERS:	none
*
* RETURNED:	nothing
*
***********************************************************************/

void EmPatchState::ExitMemMgr (const char* fnName)
{
	UNUSED_PARAM (fnName)

	--fgData.fMemMgrCount;
	EmAssert (fgData.fMemMgrCount >= 0);

	#if 0	// _DEBUG
	if (fgData.fMemMgrCount == 0)
	{
		EmPalmHeap::ValidateAllHeaps ();
	}
	#endif
}

Bool EmPatchState::UIInitialized (void)
{
	return fgData.fUIInitialized;
}

void EmPatchState::SetUIInitialized (Bool value)
{
	fgData.fUIInitialized = value != 0;
}

Bool EmPatchState::UIReset (void)
{
	return fgData.fUIReset;
}

void EmPatchState::SetUIReset (Bool value)
{
	fgData.fUIReset = value != 0;
}

Bool EmPatchState::HeapInitialized (void)
{
	return fgData.fHeapInitialized;
}

void EmPatchState::SetHeapInitialized (Bool value)
{
	fgData.fHeapInitialized = value != 0;
}


/***********************************************************************
 *
 * FUNCTION:	EmPatchState::CollectCurrentAppInfo
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Err EmPatchState::CollectCurrentAppInfo (emuptr appInfoP, EmuAppInfo &newAppInfo)
{
	memset (&newAppInfo, 0, sizeof (newAppInfo));

	// Scarf some information out of the app info block.

	newAppInfo.fDB			= (DmOpenRef) EmMemGet32 (appInfoP + offsetof (SysAppInfoType, dbP));
	newAppInfo.fStackP		= EmMemGet32 (appInfoP + offsetof (SysAppInfoType, stackP));
	newAppInfo.fMemOwnerID	= EmMemGet16 (appInfoP + offsetof (SysAppInfoType, memOwnerID));

	// Determine the current stack range.  Under Palm OS 3.0 and later, this information
	// is in the DatabaseInfo block.  Under earlier OSes, we only get the low-end of the stack
	// (that is, the address that was returned by MemPtrNew).  To get the high-end of
	// the stack, assume that stackP pointed to a chunk of memory allocated by MemPtrNew
	// and call MemPtrSize.

	if (DatabaseInfoHasStackInfo ())
	{
		if (newAppInfo.fStackP)
		{
			UInt32	stackSize = ::MemPtrSize ((MemPtr) newAppInfo.fStackP);
			if (stackSize)
			{
				newAppInfo.fStackEndP = newAppInfo.fStackP + stackSize;
			}
			else
			{
				newAppInfo.fStackEndP = EmMemNULL;
			}
		}
	}
	else
	{
		newAppInfo.fStackEndP = EmMemGet32 (appInfoP + offsetof (SysAppInfoType, stackEndP));
	}

	newAppInfo.fStackSize = newAppInfo.fStackEndP - newAppInfo.fStackP;

	// Remember the current application name and version information.  We use
	// this information when telling users that something has gone haywire.  Collect
	// this information now instead of later (on demand) as we can't be sure that
	// we can make the necessary DataMgr calls after an error occurs.
	//
	// If the database has a 'tAIN' resource, get the name from there.
	// Otherwise, use the database name.
	//
	// (Write the name into a temporary local variable.  The local variable is
	// on the stack, which will get "mapped" into the emulated address space so
	// that the emulated DmDatabaseInfo can get to it.)

	UInt16	cardNo;
	LocalID dbID;

	Err err = ::DmOpenDatabaseInfo (newAppInfo.fDB, &dbID, NULL, NULL, &cardNo, NULL);
	if (err)
		return err;

	newAppInfo.fCardNo = cardNo;
	newAppInfo.fDBID = dbID;

	char	appName[dmDBNameLength] = {0};
	char	appVersion[256] = {0};	// <gulp> I hope this is big enough...

//	DmOpenRef	dbP = DmOpenDatabase (cardNo, dbID, dmModeReadOnly);

//	if (dbP)
	{
		MemHandle	strH;

		// Get the app name from the 'tAIN' resource.

		strH = ::DmGet1Resource (ainRsc, ainID);
		if (strH)
		{
			emuptr strP = (emuptr) ::MemHandleLock (strH);
			EmMem_strcpy (appName, strP);
			::MemHandleUnlock (strH);
			::DmReleaseResource (strH);
		}

		// Get the version from the 'tver' resource, using ID's 1 and 1000

		strH = ::DmGet1Resource (verRsc, appVersionID);
		if (strH == NULL)
			strH = ::DmGet1Resource (verRsc, appVersionAlternateID);
		if (strH)
		{
			emuptr strP = (emuptr) ::MemHandleLock (strH);
			EmMem_strcpy (appVersion, strP);
			::MemHandleUnlock (strH);
			::DmReleaseResource (strH);
		}

//		::DmCloseDatabase (dbP);
	}

	if (appName[0] == 0)	// No 'tAIN' resource, so use database name
	{
		::DmDatabaseInfo (cardNo, dbID,
						appName,
						NULL, NULL, NULL, NULL, NULL,
						NULL, NULL, NULL, NULL, NULL);
	}

	// Copy the strings from the stack to their permanent homes.

	strcpy (newAppInfo.fName, appName);
	strcpy (newAppInfo.fVersion, appVersion);

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmPatchState::GetCurrentAppInfo
 *
 * DESCRIPTION:	Return information on the last application launched
 *				with SysAppLaunch (and that hasn't exited yet).
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/


EmuAppInfo EmPatchState::GetCurrentAppInfo (void)
{
	EmuAppInfo	result;
	memset (&result, 0, sizeof (result));

	if (fgData.fCurAppInfo.size () > 0)
	{
		result = *(fgData.fCurAppInfo.rbegin ());
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	EmPatchState::GetRootAppInfo
 *
 * DESCRIPTION:	Return information on the last application launched
 *				with SysAppLaunch and with the launch code of
 *				sysAppLaunchCmdNormalLaunch (and that hasn't exited yet).
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

EmuAppInfo EmPatchState::GetRootAppInfo (void)
{
	EmuAppInfo	result;
	memset (&result, 0, sizeof (result));

	EmuAppInfoList::reverse_iterator iter = fgData.fCurAppInfo.rbegin ();

	while (iter != fgData.fCurAppInfo.rend ())
	{
		if ((*iter).fCmd == sysAppLaunchCmdNormalLaunch)
		{
			result = *iter;
			break;
		}

		++iter;
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ operator >> (EmStream&, EmStackFrame&)
// ---------------------------------------------------------------------------

EmStream& operator >> (EmStream& inStream, EmStackFrame& outInfo)
{
	inStream >> outInfo.fAddressInFunction;
	inStream >> outInfo.fA6;

	return inStream;
}


// ---------------------------------------------------------------------------
//		¥ operator << (EmStream&, const EmStackFrame&)
// ---------------------------------------------------------------------------

EmStream& operator << (EmStream& inStream, const EmStackFrame& inInfo)
{
	inStream << inInfo.fAddressInFunction;
	inStream << inInfo.fA6;

	return inStream;
}


// ---------------------------------------------------------------------------
//		¥ operator >> (EmStream&, EmTrackedChunk&)
// ---------------------------------------------------------------------------

EmStream& operator >> (EmStream& inStream, EmTrackedChunk& outInfo)
{
	inStream >> outInfo.ptr;
	inStream >> outInfo.isHandle;
	inStream >> outInfo.stackCrawl;

	return inStream;
}


// ---------------------------------------------------------------------------
//		¥ operator << (EmStream&, const EmTrackedChunk&)
// ---------------------------------------------------------------------------

EmStream& operator << (EmStream& inStream, const EmTrackedChunk& inInfo)
{
	inStream << inInfo.ptr;
	inStream << inInfo.isHandle;
	inStream << inInfo.stackCrawl;

	return inStream;
}
