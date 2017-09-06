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

#ifndef EmPatchModuleNetLib_h
#define EmPatchModuleNetLib_h

#include "EmPatchModule.h"

class EmPatchModuleNetLib : public EmPatchModule
{
	public:
								EmPatchModuleNetLib (void);
		virtual					~EmPatchModuleNetLib (void) {}
};

#endif // EmPatchModuleNetLib_h
