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

#ifndef EmPatchModule_h
#define EmPatchModule_h

#include "EcmObject.h"
#include "EmPatchModuleTypes.h"
#include "ChunkFile.h"
#include "Miscellaneous.h"
#include "SystemMgr.h"

#include <string>
#include <map>



// ===========================================================================
//		EmPatchModule
// ===========================================================================


const uint16 kMaxProtoTables = 5;

class EmPatchModule : public EcmObject,
	ecm_implements IEmPatchModule
{
	private:
// ==============================================================================
// *  illegal constructors
// ==============================================================================
		//don't let EmPatchModule be created using default constructor... must have a unique name.
		//
		EmPatchModule(){}

	public:

// ==============================================================================
// *  constructors
// ==============================================================================

		EmPatchModule(const char *nameCSP, ProtoPatchTableEntry *protoTable1P = NULL, ProtoPatchTableEntry *protoTable2P = NULL);
		virtual ~EmPatchModule (void) {};

// ==============================================================================
// *  interface implementations
// ==============================================================================
// ==============================================================================
// *  BEGIN IEmPatchModule
// ==============================================================================

		virtual Err Initialize(IEmPatchContainer &containerIP);

		virtual Err Reset();

		virtual Err Dispose();

		virtual Err Clear ();

		virtual Err Load();

		virtual const string &GetName();

		virtual Err GetHeadpatch (uint16 index, HeadpatchProc& procP);
		virtual Err GetTailpatch (uint16 index, TailpatchProc& procP);

// ==============================================================================
// *  END IEmPatchModule
// ==============================================================================


		void AddProtoPatchTable (ProtoPatchTableEntry protoPatchTable[], bool loadTable = false)
		{
			fProtoTables[fProtoTableCount++] = protoPatchTable;

			if (loadTable)
				Load();
		}


		void PrvLoadProtoPatchTable (uint16 index);

		bool operator == (const char *nameCSP) const
		{
			return (fName == nameCSP);
		}

		bool operator == (const string &nameStr) const
		{
			return (fName == nameStr);
		}
	
	private:
		string					fName;

		vector<HeadpatchProc>	fHeadpatches;
		vector<TailpatchProc>	fTailpatches;

		uint16					fProtoTableCount;
		uint16					fLoadedTableCount;

		IEmPatchContainer*		fContainerIP;

		ProtoPatchTableEntry*	fProtoTables[kMaxProtoTables];
};

#endif // EmPatchModule_h
