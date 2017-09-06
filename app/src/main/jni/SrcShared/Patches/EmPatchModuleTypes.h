/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 PocketPyro, Inc.
	Portions Copyright (c) Palm, Inc.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef EmPatchModuleTypes_h
#define EmPatchModuleTypes_h

#include "EmPatchIf.h"			// HeadpatchProc, TailpatchProc, IEmPatchModule
#include "EmStructs.h"			// SystemCallContext


// ======================================================================
// At compile time, the list of functions we want to head- and tailpatch
// are stored in an array of ProtoPatchTableEntries.  At runtime, these
// compact arrays are expanded by PatchModule into a sparse array
// so that the function dispatch number can be used to look up the
// patch function.
// ======================================================================

struct ProtoPatchTableEntry
{
	uint16			fTrapWord;
	HeadpatchProc	fHeadpatch;
	TailpatchProc	fTailpatch;
};


// ===========================================================================
//		InstalledLibPatchEntry
// ===========================================================================
// Structure used to hold tail-patch information.

class InstalledLibPatchEntry
{
	public:
								InstalledLibPatchEntry (void) :
									fIsDirty (true),
									fTableP (NULL)
								{
								}

		bool					IsDirty (void) const
								{
									return fIsDirty;
								}

		void					SetDirty (bool dirty = true)
								{
									fIsDirty = dirty;
								}

		IEmPatchModule*			GetPatchTableP (void) const
								{
									return fTableP;
								}

		void					SetPatchTableP (IEmPatchModule* tableP)
								{
									fTableP = tableP;
								}

	private:
		bool					fIsDirty;
		IEmPatchModule*			fTableP;
};

typedef vector<InstalledLibPatchEntry> PatchedLibIndex;


// ===========================================================================
//		¥ TailpatchType
// ===========================================================================
// Structure used to hold tail-patch information.

struct TailpatchType
{
	SystemCallContext	fContext;
	int32 				fCount;
	TailpatchProc		fTailpatch;
};

typedef vector<TailpatchType>		TailPatchIndex;

#endif // EmPatchModuleTypes_h
