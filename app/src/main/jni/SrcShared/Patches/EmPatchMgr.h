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

#ifndef EmPatchMgr_h
#define EmPatchMgr_h

#include "EcmObject.h"
#include "EmPatchIf.h"
#include "PreferenceMgr.h"		// PrefKeyType

#include <vector>

class SessionFile;
struct SystemCallContext;

struct IEmPatchModule;


class EmPatchMgr : public EcmObject,
	ecm_implements IEmPatchContainer,
	ecm_implements IEmPatchDllTempHacks
{
	public:

	ECM_CLASS_IF_LIST_BEGIN(EmPatchMgr, EcmObject)
		ECM_CLASS_IF(kEmPatchContainerIfn, IEmPatchContainer)
		ECM_CLASS_IF(kEmPatchDllTempHacksIfn, IEmPatchDllTempHacks)
	ECM_CLASS_IF_LIST_END(EmPatchMgr, EcmObject)

		
		
// ==============================================================================
// *  BEGIN IEmPatchContainer
// ==============================================================================

// ==============================================================================
// *  END IEmPatchContainer
// ==============================================================================

// ==============================================================================
// *  BEGIN IEmPatchDllTempHacks
// ==============================================================================

	virtual Err GetGlobalMemBanks(void** membanksPP);
	virtual Err GetGlobalRegs(void** regsPP);

// ==============================================================================
// *  END IEmPatchDllTempHacks
// ==============================================================================

		static void				Initialize				(void);
		static void				Reset					(void);
		static void				Save					(SessionFile&);
		static void				Load					(SessionFile&);
		static void				Dispose					(void);

		static void				PostLoad				(void);

		static CallROMType		HandleSystemCall		(const SystemCallContext&);

		static void				HandleInstructionBreak	(void);
		static void				InstallInstructionBreaks(void);
		static void				RemoveInstructionBreaks	(void);

		static void				GetPatches				(const SystemCallContext&,
														 HeadpatchProc& hp,
														 TailpatchProc& tp);

		static CallROMType		HandlePatches			(const SystemCallContext&,
														 HeadpatchProc hp,
														 TailpatchProc tp);

		static IEmPatchModule*	GetLibPatchTable		(uint16 refNum);

		static CallROMType		CallHeadpatch			(HeadpatchProc, bool noProfiling = true);
		static void				CallTailpatch			(TailpatchProc, bool noProfiling = true);

		static Err				SwitchToApp				(UInt16 cardNo, LocalID dbID);

		static void				PuppetString			(CallROMType& callROM,
														 Bool& clearTimeout);

		static Bool				IntlMgrAvailable		(void);

	private:
		static void				SetupForTailpatch		(TailpatchProc tp,
														 const SystemCallContext&);
		static TailpatchProc	RecoverFromTailpatch	(emuptr oldpc);
};

#endif /* EmPatchMgr_h */

