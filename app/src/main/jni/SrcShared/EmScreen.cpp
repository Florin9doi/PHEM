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
#include "EmScreen.h"

#include "EmHAL.h"				// EmHAL:: GetLCDBeginEnd
#include "EmMemory.h"			// CEnableFullAccess
#include "MetaMemory.h"			// MetaMemory::MarkScreen


static emuptr	gScreenDirtyLow;
static emuptr	gScreenDirtyHigh;

static emuptr	gScreenBegin;
static emuptr	gScreenEnd;


/***********************************************************************
 *
 * FUNCTION:	EmScreen::Initialize
 *
 * DESCRIPTION: Standard initialization function.  Responsible for
 *				initializing this sub-system when a new session is
 *				created.  Will be followed by at least one call to
 *				Reset or Load.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmScreen::Initialize (void)
{
	gScreenDirtyLow		= EmMemEOM;
	gScreenDirtyHigh 	= EmMemNULL;

	gScreenBegin		= EmMemNULL;
	gScreenEnd			= EmMemNULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmScreen::Reset
 *
 * DESCRIPTION: Standard reset function.  Sets the sub-system to a
 *				default state.	This occurs not only on a Reset (as
 *				from the menu item), but also when the sub-system
 *				is first initialized (Reset is called after Initialize)
 *				as well as when the system is re-loaded from an
 *				insufficient session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmScreen::Reset (void)
{
	gScreenDirtyLow		= EmMemNULL;
	gScreenDirtyHigh	= EmMemEOM;

	gScreenBegin		= EmMemNULL;
	gScreenEnd			= EmMemNULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmScreen::Save
 *
 * DESCRIPTION: Standard save function.  Saves any sub-system state to
 *				the given session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmScreen::Save (SessionFile&)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmScreen::Load
 *
 * DESCRIPTION: Standard load function.  Loads any sub-system state
 *				from the given session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmScreen::Load (SessionFile&)
{
	gScreenDirtyLow		= EmMemNULL;
	gScreenDirtyHigh	= EmMemEOM;

	EmHAL::GetLCDBeginEnd (gScreenBegin, gScreenEnd);
}


/***********************************************************************
 *
 * FUNCTION:	EmScreen::Dispose
 *
 * DESCRIPTION: Standard dispose function.	Completely release any
 *				resources acquired or allocated in Initialize and/or
 *				Load.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmScreen::Dispose (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmScreen::MarkDirty
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmScreen::MarkDirty (emuptr address, uint32 size)
{
	/*	
		SUBTLE BUG NOTE:  Both of these tests need to be performed.
		Originally, I had this as an if (...) ... else if (...) ... After
		all, how could a given address be above the high- water mark if it
		was below the low-water mark?  "Obviously", it couldn't, so I didn't
		see the need to test against the high-water mark if we were below
		the low-water mark.

		Well, duh, that assumption was false.  After the LCD buffer is
		copied to the screen, we set the low-water mark to the end of the
		buffer and the high-water mark to the beginning of the buffer.  If
		the screen is modified in such a way that each pixel affected
		appears lower in memory than any previous pixel (as happens when we
		scroll a document down), then we always entered the low-water mark
		of the code below; we never entered the high-water mark of the code.
		Thus, the high-water mark stayed set to the beginning of the
		buffer, giving us a NULL update range.
	*/

	if (address < gScreenDirtyLow)
	{
		gScreenDirtyLow = address;
	}

	if (address + size > gScreenDirtyHigh)
	{
		gScreenDirtyHigh = address + size;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmScreen::InvalidateAll
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmScreen::InvalidateAll (void)
{
	gScreenDirtyLow		= EmMemNULL;
	gScreenDirtyHigh	= EmMemEOM;

	emuptr newScreenBegin;
	emuptr newScreenEnd;
	EmHAL::GetLCDBeginEnd (newScreenBegin, newScreenEnd);

	if (newScreenBegin != gScreenBegin || newScreenEnd != gScreenEnd)
	{
		MetaMemory::UnmarkScreen (gScreenBegin, gScreenEnd);

		gScreenBegin	= newScreenBegin;
		gScreenEnd		= newScreenEnd;

		MetaMemory::MarkScreen (gScreenBegin, gScreenEnd);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmScreen::GetBits
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool EmScreen::GetBits (EmScreenUpdateInfo& info)
{
	// Always return whether or not the LCD is on.

	info.fLCDOn			= EmHAL::GetLCDScreenOn ();

	// Get the first and last scanlines to update.	This is a *big* win
	// when running Gremlins.  Without this check, a typical test ran
	// 240 seconds.  With the check, the test ran 170 seconds.

	// Get the screen begin and end.  We'll be clipping against this
	// range (just in case screenLow and/or screenHigh got out of whack,
	// which might happen in a multi-threaded system and we aren't using
	// synchronization objects).

	emuptr screenBegin;
	emuptr screenEnd;
	EmHAL::GetLCDBeginEnd (screenBegin, screenEnd);

	// Get the range of bytes affected, clipping to the range of the
	// screen.  From this information, we can determine the first and
	// last affected scanlines.

	info.fScreenLow		= max (gScreenDirtyLow, screenBegin);
	info.fScreenHigh	= min (gScreenDirtyHigh, screenEnd);

	// Reset gScreenDirtyLow/High with sentinel values so that they can
	// be munged again by EmScreen::MarkDirty.

	gScreenDirtyLow		= EmMemEOM;
	gScreenDirtyHigh	= EmMemNULL;

	// If no lines need to be updated, we can return now.

	if (info.fScreenLow >= info.fScreenHigh)
	{
		return false;
	}

	// Get the current state of things.  Do this only if the LCD is on.
	// If the LCD is off, we may not be able to get the contents of its
	// frame buffer (the bus to the buffer may be disabled).

	if (info.fLCDOn)
	{
		CEnableFullAccess	munge;	// Remove blocks on memory access.

		EmHAL::GetLCDScanlines (info);
	}

	return true;
}
