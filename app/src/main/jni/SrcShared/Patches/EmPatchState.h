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

#ifndef EmPatchState_h
#define EmPatchState_h

#include "ChunkFile.h"
#include "EmPalmHeap.h"
#include "EmPatchModuleTypes.h"
#include "PreferenceMgr.h"		// PrefKeyType

#include <vector>
#include <map>


const UInt32	kOSUndeterminedVersion = ~0;


typedef map<emuptr, EmPalmHeap*>	EmHeapMap;



// Structure used to hold information about the currently
// running Palm OS application (as recorded when SysAppStartup
// is called).

struct EmuAppInfo
{
	Int16		fCmd;		// launch code
	DmOpenRef	fDB;
	UInt16		fCardNo;
	LocalID		fDBID;
	UInt16		fMemOwnerID;
	emuptr		fStackP;
	emuptr		fStackEndP;
	long		fStackSize;
	char		fName[dmDBNameLength];
	char		fVersion[256];	// <gulp> I hope this is big enough...
};

typedef vector<EmuAppInfo>	EmuAppInfoList;


// Structure used to keep track of the context in which
// memory chunks were allocated.

struct EmTrackedChunk
{
	emuptr				ptr;
	bool				isHandle;
	EmStackFrameList	stackCrawl;
};

typedef list<EmTrackedChunk> EmTrackedChunkList;


struct EmPatchStateData
{
	public:
		EmPatchStateData () : 
			fUIInitialized (false),
			fUIReset (false),

			fEvtGetEventCalled (false),
			fAutoAcceptBeamDialogs (false),
			fTimeToQuit (false),

			fLastEvtTrap (0),
			fOSVersion (0),
			fEncoding (0),

			fSysBinarySearchCount (0),

			fQuitAppCardNo (0),
			fQuitAppDbID (0),
			fNextAppCardNo (0),
			fNextAppDbID (0),

			fHeapInitialized (false),
			fMemMgrCount (0),
			fMemSemaphoreCount (0),
			fMemSemaphoreReserveTime (0),
			fResizeOrigSize (0),
			fHeapID (0)
		{
		}

		bool 					fUIInitialized; 
		bool 					fUIReset;
		bool					fEvtGetEventCalled;
		bool					fAutoAcceptBeamDialogs;
		bool					fTimeToQuit;

		uint16					fLastEvtTrap;
		uint32					fOSVersion;
		uint32					fEncoding;

		long 					fSysBinarySearchCount;

		uint16 					fQuitAppCardNo;
		LocalID					fQuitAppDbID;
		uint16 					fNextAppCardNo;
		LocalID					fNextAppDbID;

		EmuAppInfoList			fCurAppInfo;

		bool 					fHeapInitialized;

		long 					fMemMgrCount;
		Bool					fMemMgrLeaks;
		long 					fMemSemaphoreCount;
		unsigned long			fMemSemaphoreReserveTime;

		uint32					fResizeOrigSize;
		uint16 					fHeapID;

		EmHeapMap				fRememberedHeaps;

		EmTrackedChunkList		fTrackedChunks;
};



class EmPatchState
{
	public:
		static Err				Initialize	(void);
		static Err				Dispose		(void);
		static Err				Reset		(void);

		enum PersistencePhase
		{
			PSPersistAll,		// save/load all
			PSPersistStep1,		// Backward compatability step 1
			PSPersistStep2		// Backward compatability step 2
		};

		static Err				Save					(EmStreamChunk &s, long version = 1, PersistencePhase pass = PSPersistAll);
		static Err				Load					(EmStreamChunk &s, long version = 1, PersistencePhase pass = PSPersistAll);

		static Err				SaveCurAppInfo			(EmStreamChunk &s);
		static Err				LoadCurAppInfo			(EmStreamChunk &s);

		static uint16			GetLastEvtTrap			(void);
		static EmuAppInfo		GetCurrentAppInfo		(void);
		static EmuAppInfo		GetRootAppInfo			(void);


		static Err				AddAppInfo				(EmuAppInfo &newAppInfo);
		static Err				RemoveCurAppInfo		(void);

		static Err				SetLastEvtTrap			(uint16 lastEvtTrap);
		static Err				SetEvtGetEventCalled	(bool wasCalled);

		static Err				SetQuitApp				(UInt16 cardNo, LocalID dbID);
		static void				SetTimeToQuit			(void);

		static void				MemMgrLeaksPrefsChanged	(PrefKeyType, void*);

		static Err				CollectCurrentAppInfo	(emuptr appInfoP, EmuAppInfo &newAppInfo);


		static UInt32			OSVersion				(void);
		static void				SetOSVersion			(UInt32 version);
		static UInt32			OSMajorMinorVersion		(void);
		static UInt32			OSMajorVersion			(void);
		static UInt32			OSMinorVersion			(void);
		static void				SetSwitchApp			(UInt16 cardNo, LocalID dbID);
		static Bool				DatabaseInfoHasStackInfo(void);
		static Bool				HasWellBehavedMemSemaphoreUsage (void);


		// Bugs fixed in 3.0.
		static Bool				HasFindShowResultsBug	(void);
		static Bool				HasSysBinarySearchBug	(void);

		// Bugs fixed in 3.1
		static Bool				HasGrfProcessStrokeBug	(void);
		static Bool				HasMenuHandleEventBug	(void);

		// Bugs fixed in 3.2
		static Bool				HasBackspaceCharBug		(void);
		static Bool				HasDeletedStackBug		(void);
		static Bool				HasFindSaveFindStrBug	(void);
		static Bool				HasFldDeleteBug			(void);
		static Bool				HasFntDefineFontBug		(void);
		static Bool				HasNetPrvTaskMainBug	(void);
		static Bool				HasNetPrvSettingSetBug	(void);

		// Bugs fixed in 3.3
		static Bool				HasECValidateFieldBug	(void);

		// Bugs fixed in 3.5
		static Bool				HasConvertDepth1To2BWBug(void);

		// Bugs fixed in 4.0
		static Bool				HasSelectItemBug		(void);
		static Bool				HasSyncNotifyBug		(void);

		// Bugs fixed in ???
		static Bool				HasPrvDrawSliderControlBug	(void);


		static Bool				GetAutoAcceptBeamDialogs (void);
		static Bool				AutoAcceptBeamDialogs	(Bool newState);
		static UInt16			GetROMCharEncoding		(void);
		static Bool				EvtGetEventCalled		(void);
		static Bool				IsInSysBinarySearch		(void);
		static void				EnterSysBinarySearch	(void);
		static void				ExitSysBinarySearch		(void);
		static uint16			GetQuitAppCardNo		(void);
		static LocalID			GetQuitAppDbID			(void);
		static void				SetEncoding				(uint32 encoding);

		static Bool				IsPCInMemMgr			(void);
		static void				EnterMemMgr				(const char* fnName);
		static void				ExitMemMgr				(const char* fnName);
		static Bool				UIInitialized			(void);

		static void				SetUIInitialized		(Bool value = true);
		static void				SetUIReset				(Bool value = true);
		static void				SetHeapInitialized		(Bool value = true);

		static Bool				UIReset					(void);
		static Bool				HeapInitialized			(void);

		// inline functions

		static uint16 GetNextAppCardNo (void)
		{
			return fgData.fNextAppCardNo;
		}

		static LocalID GetNextAppDbID (void)
		{
			return fgData.fNextAppDbID;
		}

		static void SetNextAppCardNo (uint16 cardno)
		{
			fgData.fNextAppCardNo = cardno;
		}

		static void SetNextAppDbID (LocalID id)
		{
			fgData.fNextAppDbID = id;
		}

	public:
		static EmPatchStateData	fgData;
};

#endif // EmPatchState_h
