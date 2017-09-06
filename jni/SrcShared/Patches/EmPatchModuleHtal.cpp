/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	Copyright (c) 2001 PocketPyro, Inc.
	All rights reserved.
	  
	Portions Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmPatchModuleHtal.h"

#include "EmSubroutine.h"
#include "Marshal.h"


class HtalLibHeadpatch
{
	public:
		static CallROMType		HtalLibSendReply	(void);
};


EmPatchModuleHtal::EmPatchModuleHtal (void) :
	EmPatchModule ("~Htal")
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPatchModuleHtal::GetHeadpatch
 *
 * DESCRIPTION:	Special overide of GetHeadpatch to return the same function for any
 *				index.
 *
 * PARAMETERS:	index	trap index to locate patch for
 *				procP	patch procedure returned.
 *
 * RETURNED:	patchErrNone
 *
 ***********************************************************************/


Err EmPatchModuleHtal::GetHeadpatch (uint16 /* index */, HeadpatchProc& procP)
{
	procP = HtalLibHeadpatch::HtalLibSendReply;

	return kPatchErrNone;
}


/***********************************************************************
 *
 * FUNCTION:	HtalLibHeadpatch::HtalLibSendReply
 *
 * DESCRIPTION:	Ohhh...I'm going to Programmer Hell for this one...
 *				We call DlkDispatchRequest to install the user name in
 *				our UIInitialize patch.  DlkDispatchRequest will
 *				eventually call HtalLibSendReply to return a result
 *				code.  Well, we haven't fired up the Htal library, and
 *				wouldn't want it to send a response even if we had.
 *				Therefore, I'll subvert the whole process by setting
 *				the HTAL library refNum passed in to the Desktop Link
 *				Manager to an invalid value.  I'll look for this
 *				value in the SysTrap handling code and no-op the call
 *				by calling this stub.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType HtalLibHeadpatch::HtalLibSendReply (void)
{
	CALLED_SETUP ("Err", "void");

	PUT_RESULT_VAL (Err, errNone);

	return kSkipROM;
}
