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

#ifndef EmPatchModuleMap_h
#define EmPatchModuleMap_h

#include "EmPatchIf.h"			// IEmPatchModuleMap
#include "EmPatchModule.h"		// IEmPatchModule


typedef map<string, IEmPatchModule*> SimplePatchModuleMap;


// EmPatchModuleMap defines a mapped list of Patch Modules created

class EmPatchModuleMap : public EcmObject,
	ecm_implements IEmPatchModuleMap
{
	public:
// ==============================================================================
// *  constructors
// ==============================================================================
		EmPatchModuleMap();
		virtual ~EmPatchModuleMap() {}


// ==============================================================================
// *  Helper functions
// ==============================================================================
		enum EPmOperation
		{
			EPMOpInitializeAll,
			EPMOpResetAll,
			EPMOpDisposeAll,
			EPMOpClearAll,
			EPMOpLoadAll
		};

		Err DoAll(EPmOperation todo, IEmPatchContainer *IP = NULL);


// ==============================================================================
// *  interface implementations
// ==============================================================================
// ==============================================================================
// *  BEGIN IEmPatchModuleMap
// ==============================================================================
		Err InitializeAll(IEmPatchContainer &containerI)
		{
			return DoAll(EPMOpInitializeAll, &containerI);
		}
		
		Err ResetAll()
		{
			return DoAll(EPMOpResetAll);
		}

		Err DisposeAll()
		{
			return DoAll(EPMOpDisposeAll);
		}
		
		Err ClearAll()
		{
			return DoAll(EPMOpClearAll);
		}
		
		Err LoadAll()
		{
			return DoAll(EPMOpLoadAll);
		}

		Err AddModule (IEmPatchModule *moduleIP);

		Err GetModuleByName (const string &nameStr, IEmPatchModule *& moduleIP);

// ==============================================================================
// *  BEGIN IEmPatchModuleMap
// ==============================================================================
	
	private:
		SimplePatchModuleMap fModMap;
};


Err AddStaticPatchModules();


#endif // EmPatchModuleMap_h
