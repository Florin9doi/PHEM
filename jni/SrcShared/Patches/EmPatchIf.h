/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 PocketPyro, Inc.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */


#ifndef EmPatchIf_h
#define EmPatchIf_h

#include "EcmIf.h"


enum
{
	kPatchErrNone,
	kPatchErrNotImplemented,
	kPatchErrInvalidIndex
};


enum CallROMType
{
	kExecuteROM,
	kSkipROM
};


// Function types for head- and Tailpatch functions.

typedef	CallROMType	(*HeadpatchProc)(void);
typedef	void		(*TailpatchProc)(void);


// ==============================================================================
// *  BEGIN IEmPatchLoader
// ==============================================================================

const EcmIfName kEmPatchLoaderIfn = "loader.patchmodule.i.ecm";

ecm_interface IEmPatchLoader : ecm_extends IEcmComponent
{
	virtual Err InitializePL	(void) = 0;
	virtual Err ResetPL			(void) = 0;
	virtual Err DisposePL		(void) = 0;
	
	virtual Err ClearPL			(void) = 0;
	virtual Err LoadPL			(void) = 0;

	virtual Err LoadAllModules	(void) = 0;
	virtual Err LoadModule		(const string& url) = 0;
};

// ==============================================================================
// *  END IEmPatchLoader
// ==============================================================================
	


// ==============================================================================
// *  BEGIN IEmPatchContainer
// ==============================================================================

const EcmIfName kEmPatchContainerIfn = "container.patchmodule.i.ecm";

ecm_interface IEmPatchContainer : ecm_extends IEcmContainer
{
};


// ==============================================================================
// *  END IEmPatchContainer
// ==============================================================================



// ==============================================================================
// *  BEGIN IEmPatchDllTempHacks
// ==============================================================================

const EcmIfName kEmPatchDllTempHacksIfn = "hacks.dll.patchmodule.i.ecm";

ecm_interface IEmPatchDllTempHacks : ecm_extends IEcmComponent
{
	virtual Err GetGlobalMemBanks(void** membanksPP)=0;
	virtual Err GetGlobalRegs(void** regsPP)=0;
};

// ==============================================================================
// *  END IEmPatchDllTempHacks
// ==============================================================================




// ===========================================================================
//		IEmPatchModule interface exposed by all patch modules.
//			this is how all patch modules appear to the patching sub-system
// ===========================================================================

const EcmIfName kEmPatchModuleIfn = "patchmodule.i.ecm";

ecm_interface IEmPatchModule : ecm_extends IEcmComponent
{
	virtual Err Initialize(IEmPatchContainer &containerIP) = 0;
	virtual Err Reset() = 0;
	virtual Err Dispose() = 0;
	
	virtual Err Clear () = 0;
	virtual Err Load () = 0;

	virtual const string &GetName() = 0;

	virtual Err GetHeadpatch (uint16 index, HeadpatchProc &procP) = 0;
	virtual Err GetTailpatch (uint16 index, TailpatchProc &procP) = 0;
};




// ===========================================================================
//		IEmPatchModuleMap interface supporting a collection of PatchModules
//			Basically there is one component which maintains the list of all 
//			installed patch modules, and it is accessed using IEmPatchModuleMap
// ===========================================================================

const EcmIfName kEmPatchModuleMapIfn = "map.patchmodule.i.ecm";

ecm_interface IEmPatchModuleMap : ecm_extends IEcmComponent
{
	virtual Err InitializeAll(IEmPatchContainer &containerI) = 0;
	virtual Err ResetAll() = 0;
	virtual Err DisposeAll() = 0;
	
	virtual Err ClearAll () = 0;
	virtual Err LoadAll () = 0;

	virtual Err AddModule (IEmPatchModule *moduleIP) = 0;
	
	virtual Err GetModuleByName (const string &nameStr, IEmPatchModule *& moduleIP) = 0;
};

#endif // EmPatchIf_h
