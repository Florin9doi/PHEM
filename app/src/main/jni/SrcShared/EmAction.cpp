/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmAction.h"

#include "ErrorHandling.h"		// Errors::ReportIfError
#include "Strings.r.h"			// kStr_GenericOperation


// ---------------------------------------------------------------------------
//		¥ EmActionComposite::EmActionComposite
// ---------------------------------------------------------------------------

EmActionComposite::EmActionComposite (void) :
	EmAction (kStr_GenericOperation)
{
}


// ---------------------------------------------------------------------------
//		¥ EmActionComposite::~EmActionComposite
// ---------------------------------------------------------------------------

EmActionComposite::~EmActionComposite (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmActionComposite::Do
// ---------------------------------------------------------------------------

void EmActionComposite::Do (void)
{
	EmActionList::iterator	iter = fActions.begin ();

	while (iter != fActions.end ())
	{
		(*iter)->Do ();
		++iter;
	}
}


// ---------------------------------------------------------------------------
//		¥ EmActionComposite::AddAction
// ---------------------------------------------------------------------------

void EmActionComposite::AddAction (EmAction* action)
{
	fActions.push_back (action);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmActionHandler::EmActionHandler
// ---------------------------------------------------------------------------

EmActionHandler::EmActionHandler (void) :
	fActions ()
{
}


// ---------------------------------------------------------------------------
//		¥ EmActionHandler::~EmActionHandler
// ---------------------------------------------------------------------------

EmActionHandler::~EmActionHandler (void)
{
	this->DeleteAll ();
}


// ---------------------------------------------------------------------------
//		¥ EmActionHandler::PostAction
// ---------------------------------------------------------------------------

void EmActionHandler::PostAction (EmAction* action)
{
	omni_mutex_lock	lock (fMutex);

	fActions.push_back (action);
}


// ---------------------------------------------------------------------------
//		¥ EmActionHandler::GetNextAction
// ---------------------------------------------------------------------------

EmAction* EmActionHandler::GetNextAction (void)
{
	omni_mutex_lock	lock (fMutex);

	EmAction*	result = NULL;

	if (!fActions.empty ())
	{
		result = fActions.front ();
		fActions.erase (fActions.begin ());
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmActionHandler::GetNextAction
// ---------------------------------------------------------------------------
//
// This function is an EXCEPTION_CATCH_POINT.

void EmActionHandler::DoAll (void)
{
	EmAction*	action = NULL;
	while ((action = this->GetNextAction ()) != NULL)
	{
		try
		{
			action->Do ();
		}
		catch (ErrCode errCode)
		{
			this->DeleteAll ();

			StrCode	operation = action->GetDescription ();

			Errors::ReportIfError (operation, errCode, 0, false);
		}

		delete action;
	}
}


// ---------------------------------------------------------------------------
//		¥ EmActionHandler::DeleteAll
// ---------------------------------------------------------------------------

void EmActionHandler::DeleteAll (void)
{
	omni_mutex_lock	lock (fMutex);

	EmActionList::iterator	iter = fActions.begin ();

	while (iter != fActions.end ())
	{
		delete *iter;
		++iter;
	}

	fActions.clear ();
}
