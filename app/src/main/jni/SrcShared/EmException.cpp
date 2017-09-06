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
#include "EmException.h"

#include "EmPalmFunction.h"		// GetTrapName
#include "EmSession.h"			// gSession
#include "ErrorHandling.h"		// Errors::DoDialog
#include "Hordes.h"				// Hordes::Stop
#include "Platform.h"			// Platform::GetString
#include "Strings.r.h"			// kStr_InternalErrorException

#include <string.h>

// ---------------------------------------------------------------------------
//		¥ EmException::EmException
// ---------------------------------------------------------------------------

EmException::EmException (void) throw () :
	exception ()
{
}

	
// ---------------------------------------------------------------------------
//		¥ EmException::~EmException
// ---------------------------------------------------------------------------

EmException::~EmException (void) throw ()
{
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmExceptionTopLevelAction::EmExceptionTopLevelAction
// ---------------------------------------------------------------------------

EmExceptionTopLevelAction::EmExceptionTopLevelAction (void) throw () :
	EmException ()
{
}

	
// ---------------------------------------------------------------------------
//		¥ EmExceptionTopLevelAction::~EmExceptionTopLevelAction
// ---------------------------------------------------------------------------

EmExceptionTopLevelAction::~EmExceptionTopLevelAction (void) throw ()
{
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmExceptionEnterDebugger::EmExceptionEnterDebugger
// ---------------------------------------------------------------------------

EmExceptionEnterDebugger::EmExceptionEnterDebugger (void) throw () :
	EmExceptionTopLevelAction ()
{
}

	
// ---------------------------------------------------------------------------
//		¥ EmExceptionEnterDebugger::~EmExceptionEnterDebugger
// ---------------------------------------------------------------------------

EmExceptionEnterDebugger::~EmExceptionEnterDebugger (void) throw ()
{
}


// ---------------------------------------------------------------------------
//		¥ EmExceptionEnterDebugger::DoAction
// ---------------------------------------------------------------------------

void EmExceptionEnterDebugger::DoAction (void)
{
	// We've already entered debug mode.  The only thing left to do
	// is stop Gremlins.

	// !!! This should probably be changed so that the only thing
	// done at the point of error/exception is the sending of the
	// state packet to the debugger.  If that succeeds, this exception
	// is thrown and performs the real entry into debug mode.  Right
	// now, both those tasks are intertwined in Debug::EnterDebugger.

	// If we're entering the debugger while running a Gremlin, save the events.

	if (Hordes::IsOn ())
	{
		Hordes::SaveEvents ();
	}

	Hordes::Stop ();
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmExceptionReset::EmExceptionReset
// ---------------------------------------------------------------------------

EmExceptionReset::EmExceptionReset (EmResetType type) throw () :
	EmExceptionTopLevelAction (),
	fType (type),
	fWhat (),
	fMessage (),
	fException (0),
	fTrapWord (0)
{
}

	
// ---------------------------------------------------------------------------
//		¥ EmExceptionReset::~EmExceptionReset
// ---------------------------------------------------------------------------

EmExceptionReset::~EmExceptionReset (void) throw ()
{
}


// ---------------------------------------------------------------------------
//		¥ EmExceptionReset::DoAction
// ---------------------------------------------------------------------------

void EmExceptionReset::DoAction (void)
{
	EmAssert (gSession);
	gSession->Reset (fType);
}


// ---------------------------------------------------------------------------
//		¥ EmExceptionReset::what
// ---------------------------------------------------------------------------

const char* EmExceptionReset::what (void) const throw ()
{
	if (fWhat.empty ())
	{
		char	buffer[1000] = {0};

		string	trapName (::GetTrapName (fTrapWord));
		string	errTemplate;

		if (fException)
		{
			errTemplate = Platform::GetString (kStr_InternalErrorException);
			sprintf (buffer, errTemplate.c_str (), fException, trapName.c_str ());
		}
		else if (!fMessage.empty ())
		{
			errTemplate = Platform::GetString (kStr_InternalErrorMessage);
			sprintf (buffer, errTemplate.c_str (), fMessage.c_str (), trapName.c_str ());
		}

		if (strlen (buffer) > 0)
		{
			fWhat = buffer;

			fWhat += Platform::GetString (kStr_WillNowReset);
		}
	}

	return fWhat.c_str ();
}


// ---------------------------------------------------------------------------
//		¥ EmExceptionReset::Display
// ---------------------------------------------------------------------------

void EmExceptionReset::Display (void) const
{
	// Generate fWhat.

	this->what ();

	// If there's something there, show it.

	if (!fWhat.empty ())
	{
		Errors::DoDialog (this->what (), kDlgFlags_continue_debug_RESET, -1);
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmExceptionNextGremlin::EmExceptionNextGremlin
// ---------------------------------------------------------------------------

EmExceptionNextGremlin::EmExceptionNextGremlin (void) throw () :
	EmExceptionTopLevelAction ()
{
}

	
// ---------------------------------------------------------------------------
//		¥ EmExceptionNextGremlin::~EmExceptionNextGremlin
// ---------------------------------------------------------------------------

EmExceptionNextGremlin::~EmExceptionNextGremlin (void) throw ()
{
}


// ---------------------------------------------------------------------------
//		¥ EmExceptionNextGremlin::DoAction
// ---------------------------------------------------------------------------

void EmExceptionNextGremlin::DoAction (void)
{
	Hordes::ErrorEncountered ();

	// The previous session state has been safely tucked away in bed, and the
	// next session state has been scheduled to be loaded.  However, it will
	// not actually be loaded until after the next opcode is executed.  If
	// that opcode causes an error (and may in fact be the reason for
	// switching to a new Gremlin), then we'd end up executing it again.  We
	// don't want that, so let's run any queued "load next state" commands now. 

	EmAssert (gSession);
	gSession->ExecuteSpecial (false);
}
