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
#include "EmPatchModuleMap.h"

#include "EmStructs.h"
#include "Miscellaneous.h"



/***********************************************************************
 *
 * FUNCTION:	EmPatchModuleMap::EmPatchModuleMap
 *
 * DESCRIPTION: EmPatchModuleMap constructor
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

EmPatchModuleMap::EmPatchModuleMap()
{
}



/***********************************************************************
 *
 * FUNCTION:	EmPatchModuleMap::DoAll
 *
 * DESCRIPTION: Repeat the given operation for all modules in the PatchMap
 *
 * PARAMETERS:	todo	the operation to be performed
 *
 * RETURNED:	patchErrNone
 *
 ***********************************************************************/

Err EmPatchModuleMap::DoAll(EPmOperation todo, IEmPatchContainer *conainerIP)
{
	Err ret = kPatchErrNone;
	
	SimplePatchModuleMap::iterator	iter;
	for (iter = fModMap.begin(); iter != fModMap.end(); ++iter)
	{
		switch (todo)
		{
			case EPMOpInitializeAll:
				iter->second->Initialize(*conainerIP);
				break;
			case EPMOpResetAll:
				iter->second->Reset();
				break;
			case EPMOpDisposeAll:
				iter->second->Dispose();
				break;
			case EPMOpClearAll:
				iter->second->Clear();
				break;
			case EPMOpLoadAll:
				iter->second->Load();
				break;
			default:
				break;
		}
	}

	return ret;
}

		
// ==============================================================================
// *  interface implementations
// ==============================================================================
// ==============================================================================
// *  BEGIN IEmAllPatchModules
// ==============================================================================

/***********************************************************************
 *
 * FUNCTION:	EmPatchModuleMap::GetModuleByName
 *
 * DESCRIPTION: Locates a PatchModule in the collection, based on the given 
 *				unique PatchModule name.
 *
 * PARAMETERS:	nameStr		name of the PatchModule to be located
 *				moduleIP	IEmPatchModule interface for the given PatchModule
 *
 * RETURNED:	patchErrNone
 *
 ***********************************************************************/

Err EmPatchModuleMap::GetModuleByName (const string &nameStr, IEmPatchModule *& moduleIP)
{
	moduleIP = NULL;
	SimplePatchModuleMap::iterator iter = fModMap.find(nameStr);

	if (iter != fModMap.end())
		moduleIP = iter->second;
	
	return kPatchErrNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmPatchModuleMap::AddModule
 *
 * DESCRIPTION: Add a PatchModule to the EmPatchModuleMap collection
 *
 * PARAMETERS:	moduleIP	address of the IEmPatchModule interface for
 *					the PatchModule to be added.
 *
 * RETURNED:	patchErrNone
 *
 ***********************************************************************/

Err EmPatchModuleMap::AddModule (IEmPatchModule *moduleIP)
{
	if (moduleIP != NULL)
	{
		fModMap.insert(SimplePatchModuleMap::value_type(moduleIP->GetName(), moduleIP));
	}
		
	return kPatchErrNone;
}
	
// ==============================================================================
// *  END IEmPatchModuleMap
// ==============================================================================
