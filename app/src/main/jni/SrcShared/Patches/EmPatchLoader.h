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


#ifndef EmPatchLoader_h
#define EmPatchLoader_h

#include "EcmObject.h"
#include "EmPatchIf.h"


// ===========================================================================
//		EmPatchLoader
// ===========================================================================

class EmPatchLoader : public EcmObject,
	ecm_implements IEmPatchLoader
{
	public:

// ==============================================================================
// *  constructors
// ==============================================================================

		EmPatchLoader()
		{
		}


// ==============================================================================
// *  interface implementations
// ==============================================================================
// ==============================================================================
// *  BEGIN IEmPatchLoader
// ==============================================================================

		virtual Err InitializePL();
		virtual Err ResetPL();
		virtual Err DisposePL();
		virtual Err ClearPL();
		virtual Err LoadPL();

		virtual Err LoadAllModules();
		virtual Err LoadModule(const string &url);

// ==============================================================================
// *  END IEmPatchLoader
// ==============================================================================


// ==============================================================================
// *  BEGIN IEmPatchContainer
// ==============================================================================


// ==============================================================================
// *  END IEmPatchContainer
// ==============================================================================

	private:
};


#endif // EmPatchLoader_h
