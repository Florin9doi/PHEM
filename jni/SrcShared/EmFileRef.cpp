/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmFileRef.h"

static EmFileRef	gEmulatorRef;

/***********************************************************************
 *
 * FUNCTION:	EmFileRef::SetEmulatorRef
 *
 * DESCRIPTION:	Set the ref corresponding to the emulator application.
 *
 * PARAMETERS:	ref -.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void EmFileRef::SetEmulatorRef	(const EmFileRef& ref)
{
	EmAssert (ref.IsSpecified ());
	EmAssert (!gEmulatorRef.IsSpecified ());

	gEmulatorRef = ref;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileRef::GetEmulatorRef
 *
 * DESCRIPTION:	Return the ref corresponding to the emulator application.
 *
 * PARAMETERS:	none.
 *
 * RETURNED:	ref - .
 *
 ***********************************************************************/

EmFileRef EmFileRef::GetEmulatorRef	(void)
{
	EmAssert (gEmulatorRef.IsSpecified ());

	return gEmulatorRef;
}
