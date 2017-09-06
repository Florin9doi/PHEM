/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1999-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "Hordes.h"				// class Hordes

#include "CGremlins.h"			// Gremlins
#include "CGremlinsStubs.h"		// StubAppGremlinsOff
#include "EmApplication.h"		// ScheduleQuit
#include "EmEventPlayback.h"	// SaveEvents, LoadEvents, Clear, RecordEvents
#include "EmMapFile.h"			// EmMapFile::Write, etc.
#include "EmMinimize.h"			// EmMinimize::IsDone
#include "EmPatchState.h"		// EmPatchState::UIInitialized
#include "EmSession.h"			// gSession, ScheduleResumeHordesFromFile
#include "EmStreamFile.h"		// kCreateOrOpenForWrite
#include "ErrorHandling.h"		// Errors::ThrowIfPalmError
#include "Logging.h"			// LogStartNew, etc.
#include "Platform.h"			// Platform::GetMilliseconds
#include "PreferenceMgr.h"		// Preference, gEmuPrefs
#include "ROMStubs.h"			// EvtWakeup
#include "SessionFile.h"		// Chunk, EmStreamChunk
#include "Startup.h"			// HordeQuitWhenDone
#include "StringConversions.h"	// ToString, FromString;
#include "Strings.r.h"			// kStr_CmdOpen, etc.
#include "SystemMgr.h"			// sysGetROMVerMajor

#include <math.h>				// sqrt
#include <time.h>				// time, localtime
/* Update for GCC 4 */
#include <string.h>


////////////////////////////////////////////////////////////////////////////////////////
// HORDES CONSTANTS

static const int	MAXGREMLINS	= 999;


////////////////////////////////////////////////////////////////////////////////////////
// HORDES STATIC DATA

static	Gremlins	gTheGremlin;
static	int32		gGremlinStartNumber;
static	int32		gGremlinStopNumber;
static	int32		gSwitchDepth;
static	int32		gMaxDepth;
		int32		gGremlinSaveFrequency;
DatabaseInfoList	gGremlinAppList;
static	int32		gCurrentGremlin;
static	int32		gCurrentDepth;
static	bool		gIsOn;
static	uint32		gStartTime;
static	uint32		gStopTime;
static	EmGremlinThreadInfo		gGremlinHaltedInError[MAXGREMLINS + 1];
static	EmDirRef	gHomeForHordesFiles;

static Bool			gForceNewHordesDirectory;
static EmDirRef		gGremlinDir;

Bool				gWarningHappened;
Bool				gErrorHappened;


////////////////////////////////////////////////////////////////////////////////////////
// HORDES METHODS

/***********************************************************************
 *
 * FUNCTION:	Hordes::Initialize
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

void Hordes::Initialize (void)
{
	gTheGremlin.Reset ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::Reset
 *
 * DESCRIPTION:	Standard reset function.  Sets the sub-system to a
 *				default state.  This occurs not only on a Reset (as
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

void Hordes::Reset (void)
{
	EmDlg::GremlinControlClose ();
	Hordes::Stop ();
	gTheGremlin.Reset ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::Save
 *
 * DESCRIPTION:	Standard save function.  Saves any sub-system state to
 *				the given session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Hordes::Save (SessionFile& f)
{
	gTheGremlin.Save (f);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::Load
 *
 * DESCRIPTION:	Standard load function.  Loads any sub-system state
 *				from the given session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Hordes::Load (SessionFile& f)
{
	Bool fHordesOn = gTheGremlin.Load (f);
	Hordes::TurnOn (fHordesOn);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::Dispose
 *
 * DESCRIPTION:	Standard dispose function.  Completely release any
 *				resources acquired or allocated in Initialize and/or
 *				Load.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Hordes::Dispose (void)
{
	gTheGremlin.Reset ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::New
 *
 * DESCRIPTION: Starts a new Horde of Gremlins
 *
 * PARAMETERS:	HordeInfo& info - Horde initialization info
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::New(const HordeInfo& info)
{
	gGremlinStartNumber	= min(info.fStartNumber, info.fStopNumber);
	gGremlinStopNumber	= max(info.fStartNumber, info.fStopNumber);
	
	if (info.fSwitchDepth == -1 || gGremlinStartNumber == gGremlinStopNumber)
		gSwitchDepth = info.fMaxDepth;

	else
		gSwitchDepth = info.fSwitchDepth;
	
	gMaxDepth				= info.fMaxDepth;
	gGremlinSaveFrequency	= info.fSaveFrequency;
	gGremlinAppList			= info.fAppList;
	gCurrentDepth			= 0;
	gCurrentGremlin			= gGremlinStartNumber;

	if (gSwitchDepth == 0)
		gSwitchDepth = -1;

	if (gMaxDepth == 0)
		gMaxDepth = -1;

	for (int counter = 0; counter <= MAXGREMLINS; counter++)
	{
		gGremlinHaltedInError[counter].fHalted		= false;
		gGremlinHaltedInError[counter].fErrorEvent	= 0;
		gGremlinHaltedInError[counter].fMessageID	= -1;
	}

	GremlinInfo gremInfo;
	
	gremInfo.fNumber		= gCurrentGremlin;
	gremInfo.fSaveFrequency	= gGremlinSaveFrequency;
	gremInfo.fSteps			= (gMaxDepth == -1) ? gSwitchDepth : min (gSwitchDepth, gMaxDepth);
	gremInfo.fAppList		= gGremlinAppList;
	gremInfo.fFinal			= gMaxDepth;

	gStartTime = Platform::GetMilliseconds ();

	gTheGremlin.New (gremInfo);

	EmDlg::GremlinControlOpen ();

	Hordes::UseNewAutoSaveDirectory ();

	EmEventPlayback::Clear ();

	// When we save our root state, we want it to be saved with all
	// the correct GremlinInfo (per the 2.0 file format) but with
	// Gremlins turned OFF.

	Hordes::TurnOn (false);

	Hordes::SaveRootState ();

	Hordes::TurnOn (true);

	Hordes::StartLog ();

	LogAppendMsg ("New Gremlin #%ld started anew to %ld events",
					gremInfo.fNumber, gremInfo.fSteps);

	LogDump ();

	gWarningHappened	= false;
	gErrorHappened		= false;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::NewGremlin
 *
 * DESCRIPTION: Starts a new Horde of just one Gremlin --
 *				"classic Gremlins"
 *
 * PARAMETERS:	GremlinInfo& info - Gremlin initialization info
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::NewGremlin (const GremlinInfo &info)
{
	HordeInfo newHorde;

	newHorde.fStartNumber	= info.fNumber;
	newHorde.fStopNumber	= info.fNumber;
	newHorde.fSwitchDepth	= info.fSteps;
	newHorde.fMaxDepth		= info.fSteps;
	newHorde.fSaveFrequency	= info.fSaveFrequency;
	newHorde.fAppList		= info.fAppList;
	
	Hordes::New (newHorde);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::HStatus
 *
 * DESCRIPTION: Returns several pieces of status information about the
 *				currently running Gremlin in the Horde.
 *
 * PARAMETERS:	currentNumber - returns the current Gremlin number.
 *				currentStep - returns the current event number of the
 *					currently running Gremlin
 *				currentUntil - returns the current upper event bound of
 *					currently running Gremlin
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::HStatus (unsigned short *currentNumber, unsigned long *currentStep,
				unsigned long *currentUntil)
{
	gTheGremlin.GStatus (currentNumber, currentStep, currentUntil);
}


string
Hordes::GremlinsFlagsToString (void)
{
	string output;

	for (int ii = 0; ii < MAXGREMLINS; ++ii)
	{
		gGremlinHaltedInError[ii].fHalted ? output += "1" : output += "0";
	}

	return output;
}

void
Hordes::GremlinsFlagsFromString(string& inFlags)
{
	for (int ii = 0; ii < MAXGREMLINS; ++ii)
	{
		gGremlinHaltedInError[ii].fHalted = (inFlags.c_str()[ii] == '1');
	}
}

void
Hordes::SaveSearchProgress()
{
	StringStringMap	searchProgress;

	searchProgress["gGremlinStartNumber"]	= ::ToString (gGremlinStartNumber);
	searchProgress["gGremlinStopNumber"]	= ::ToString (gGremlinStopNumber);
	searchProgress["gSwitchDepth"]			= ::ToString (gSwitchDepth);
	searchProgress["gMaxDepth"]				= ::ToString (gMaxDepth);
	searchProgress["gGremlinSaveFrequency"]	= ::ToString (gGremlinSaveFrequency);
	searchProgress["gCurrentGremlin"]		= ::ToString (gCurrentGremlin);
	searchProgress["gCurrentDepth"]			= ::ToString (gCurrentDepth);
	searchProgress["gGremlinHaltedInError"]	= Hordes::GremlinsFlagsToString ();
	searchProgress["gStartTime"]			= ::ToString (gStartTime);
	searchProgress["gStopTime"]				= ::ToString (gStopTime);

	EmFileRef	searchFile = Hordes::SuggestFileRef (kHordeProgressFile);

	EmMapFile::Write (searchFile, searchProgress);
}


void
Hordes::ResumeSearchProgress (const EmFileRef& f)
{
	StringStringMap	searchProgress;

	EmMapFile::Read (f, searchProgress);

	::FromString (searchProgress["gGremlinStartNumber"],	gGremlinStartNumber);
	::FromString (searchProgress["gGremlinStopNumber"],		gGremlinStopNumber);
	::FromString (searchProgress["gSwitchDepth"],			gSwitchDepth);
	::FromString (searchProgress["gMaxDepth"],				gMaxDepth);
	::FromString (searchProgress["gGremlinSaveFrequency"],	gGremlinSaveFrequency);
	::FromString (searchProgress["gCurrentGremlin"],		gCurrentGremlin);
	::FromString (searchProgress["gCurrentDepth"],			gCurrentDepth);

	Hordes::GremlinsFlagsFromString (searchProgress["gGremlinHaltedInError"]);

	// Get, then patch up start and stop times.

	::FromString (searchProgress["gStartTime"],				gStartTime);
	::FromString (searchProgress["gStopTime"],				gStopTime);

	uint32	delta = gStopTime - gStartTime;
	gStopTime	= Platform::GetMilliseconds ();
	gStartTime	= gStopTime - delta;

//	gSession->ScheduleResumeHordesFromFile ();
}

Bool
Hordes::IsOn (void)
{
	return gIsOn;
}

Bool
Hordes::InSingleGremlinMode (void)
{
	return gGremlinStartNumber == gGremlinStopNumber;
}


Bool
Hordes::QuitWhenDone (void)
{
	if (Startup::HordeQuitWhenDone ())
		return true;

	return false;
}


Bool
Hordes::CanNew (void)
{
	return !EmMinimize::IsOn () && EmPatchState::UIInitialized ();
}


Bool
Hordes::CanSuspend (void)
{
	return !EmMinimize::IsOn ();
}


Bool
Hordes::CanStep (void)
{
	return (gTheGremlin.IsInitialized () && !gIsOn && !EmMinimize::IsOn ());
}


Bool
Hordes::CanResume (void)
{
	return (gTheGremlin.IsInitialized () && !gIsOn && !EmMinimize::IsOn ());
}


Bool
Hordes::CanStop (void)
{
	return (gIsOn && !EmMinimize::IsOn ());
}


int32
Hordes::GremlinNumber (void)
{
	return gCurrentGremlin;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::TurnOn
 *
 * DESCRIPTION: Turns Hordes on or off.
 *
 * PARAMETERS:	fHordesOn - specifies if Hordes should be on or off.
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::TurnOn (Bool hordesOn)
{
	gIsOn = (hordesOn != false);
	EmEventPlayback::RecordEvents (gIsOn);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::EventCounter
 *
 * DESCRIPTION: Returns the current event count of the currently running
 *				Gremlin
 *
 * PARAMETERS:	none
 *
 * RETURNED:	event count
 *
 ***********************************************************************/

int32
Hordes::EventCounter (void)
{
	unsigned short	number;
	unsigned long	step;
	unsigned long	until;

	Hordes::HStatus (&number, &step, &until);

	return step;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::EventLimit
 *
 * DESCRIPTION: Returns the current event limit of the currently running
 *				Gremlin
 *
 * PARAMETERS:	none
 *
 * RETURNED:	event limit
 *
 ***********************************************************************/

int32
Hordes::EventLimit(void)
{
	unsigned short	number;
	unsigned long	step;
	unsigned long	until;

	Hordes::HStatus (&number, &step, &until);

	return until;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::EndHordes
 *
 * DESCRIPTION: Ends Hordes, giving back control to user.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::EndHordes (void)
{
	// In an odd twist, logging doesn't work if Hordes is supposedly
	// turned off. So let's spoof that it's on; it will be turned off --
	// again -- below, when Hordes::Stop() is called.

	Hordes::TurnOn (true);

	if (!Hordes::InSingleGremlinMode ())
	{
		LogAppendMsg ("*************   Gremlin Horde ended at Gremlin #%ld\n", gGremlinStopNumber);
	}

	// It's time to print out some basic info:
	//	ROM version
	//	ROM file name
	//	Device name
	//	RAM size

	UInt32 romVersionData;
	::FtrGet (sysFileCSystem, sysFtrNumROMVersion, &romVersionData);

	UInt32 romVersionMajor = sysGetROMVerMajor (romVersionData);
	UInt32 romVersionMinor = sysGetROMVerMinor (romVersionData);

	Preference<Configuration>	pref (kPrefKeyLastConfiguration);
	Preference<EmFileRef>		pref2 (kPrefKeyLastPSF);

	Configuration	cfg				= *pref;
	EmDevice		device			= cfg.fDevice;
	string			deviceStr		= device.GetIDString ();
	RAMSizeType		ramSize			= cfg.fRAMSize;
	EmFileRef		romFile			= cfg.fROMFile;
	string			romFileStr		= romFile.GetFullPath ();
	EmFileRef		sessionFile		= *pref2;
	string			sessionFileStr	= sessionFile.GetFullPath ();

	if (sessionFileStr.empty ())
	{
		sessionFileStr = "<Not selected>";
	}

	LogAppendMsg ("*************   Device Info:");
	LogAppendMsg ("ROM version:             %d.%d", romVersionMajor, romVersionMinor);
	LogAppendMsg ("ROM file name:           %s", (char *) romFileStr.c_str ());
	LogAppendMsg ("Session file:			%s", (char *) sessionFileStr.c_str ());
	LogAppendMsg ("Device name:             %s", (char *) deviceStr.c_str ());
	LogAppendMsg ("RAM size:                %d KB\n", (long) ramSize);

	// Let's come up with some statistics from our new field in 
	// gGremlinHaltedInError.

	int32 min, max, avg, stdDev, smallErrorIndex;
	Hordes::ComputeStatistics (min, max, avg, stdDev, smallErrorIndex);

	LogAppendMsg ("*************   Error Occurrence Statistics:");
	LogAppendMsg ("");

	Hordes::GremlinReport ();

	// check for the sentinel value

	if (smallErrorIndex != 0x7FFFFFFF)
	{
		LogAppendMsg ("");
		LogAppendMsg ("Minimum Event:            %d", min);
		LogAppendMsg ("Maximum Event:            %d", max);
		LogAppendMsg ("Average Event:            %d", avg);
		LogAppendMsg ("Standard Deviation:       %d", stdDev);
		LogAppendMsg ("Overall Shortest Gremlin: #%d\n", smallErrorIndex);
	}
	else
	{
		LogAppendMsg ("No Gremlins found errors.\n");
	}

	LogDump ();

	Hordes::TurnOn (false);

	LogClear();
	EmEventPlayback::Clear ();

	if (!Hordes::InSingleGremlinMode ())
	{
		EmDlg::GremlinControlClose ();

		EmAssert (gSession);
		gSession->ScheduleLoadRootState ();
	}

	if (Hordes::QuitWhenDone ())
	{
		EmAssert (gApplication);
		gApplication->ScheduleQuit ();
	}
	else
	{
		gWarningHappened	= false;
		gErrorHappened		= false;
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::ProposeNextGremlin
 *
 * DESCRIPTION: Proposes the NEXT Gremlin # and corresponding search
 *				depth, FROM the state of the Hordes run specified by the
 *				input paramaters
 *
 * PARAMETERS:	outNextGremlin - passes back suggested next Gremlin
 *				outNextDepth - passes back next depth
 *				inFromGremlin - "current" Gremlin
 *				inFromGremlin - "current" depth
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::ProposeNextGremlin (long& outNextGremlin, long& outNextDepth,
							long inFromGremlin, long inFromDepth)
{
	outNextGremlin	= inFromGremlin + 1;
	outNextDepth	= inFromDepth;

	if (outNextGremlin == gGremlinStopNumber + 1)
	{
		outNextGremlin = gGremlinStartNumber;

		if (outNextDepth >= 0)
			++outNextDepth;
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::StartGremlinFromLoadedRootState
 *
 * DESCRIPTION: After the CPU loads the root state during an off-cycle,
 *				it calls this to indicate that Hordes is meant to start
 *				the current Gremlin, and that the Emulator state is ready
 *				for this.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::StartGremlinFromLoadedRootState (void)
{
	GremlinInfo gremInfo;

	gremInfo.fNumber		= gCurrentGremlin;
	gremInfo.fSaveFrequency	= gGremlinSaveFrequency;
	gremInfo.fSteps			= ((gMaxDepth == -1) ? gSwitchDepth : min(gSwitchDepth, gMaxDepth));
	gremInfo.fAppList		= gGremlinAppList;
	gremInfo.fFinal			= gMaxDepth;

	gTheGremlin.New (gremInfo);

	LogAppendMsg ("New Gremlin #%ld started from root state to %ld events",
					gCurrentGremlin, gremInfo.fSteps);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::StartGremlinFromLoadedSuspendedState
 *
 * DESCRIPTION: After the CPU loads the suspended state during an off-cycle,
 *				it calls this to indicate that Hordes is meant to resume
 *				the current Gremlin, and that the Emulator state is ready
 *				for this.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::StartGremlinFromLoadedSuspendedState (void)
{
	// We reset the Gremlin to go until the next occurence of the
	// depth-bound, or until gMaxDepth, whichever occurs first.

	long newUntil = gSwitchDepth * (gCurrentDepth + 1);

	if (gMaxDepth != -1)
	{
		newUntil = min (newUntil, gMaxDepth);
	}

	gTheGremlin.SetUntil (newUntil);

	LogAppendMsg("Resuming Gremlin #%ld to #%ld events",
		gCurrentGremlin, newUntil);

	Hordes::TurnOn(true);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::SetGremlinStatePathFromControlFile
 *
 * DESCRIPTION:	Given the file reference to a control file (actually *any*
 *				file in the Gremlins state path), set the Gremlins state
 *				path to the directory that contains this file.
 *
 * PARAMETERS:	controlFile - reference to the rootStateFile.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void
Hordes::SetGremlinStatePathFromControlFile (EmFileRef& controlFile)
{
	gGremlinDir = controlFile.GetParent ();
	gForceNewHordesDirectory = false;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::NextGremlin
 *
 * DESCRIPTION: Selects and runs the next Gremlin in the Horde, if there
 *				are unfinished Gremlins left. Otherwise, just loads
 *				the pre-Horde state and stops.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::NextGremlin (void)
{
	Hordes::Stop ();

	Hordes::SaveSearchProgress ();

	// Find the next Gremlin to run.

	long nextGremlin, nextDepth;
	
	// Keep looking until we find a Gremlin in the range which has
	// not halted in error.

	Hordes::ProposeNextGremlin (nextGremlin, nextDepth, gCurrentGremlin, gCurrentDepth);
	
	while (gGremlinHaltedInError[nextGremlin].fHalted) 
	{
		// All Gremlins halted in error when we are back at the current
		// Gremlin at the next depth. (We looped around without finding
		// the next Gremlin that didn't halt in error).

		if (nextGremlin == gCurrentGremlin && nextDepth >= gCurrentDepth + 1)
		{
			Hordes::EndHordes ();
			return;
		}

		Hordes::ProposeNextGremlin (nextGremlin, nextDepth, nextGremlin, nextDepth);
	}

	// Update our current location in the Gremlin search tree.

	gCurrentGremlin	= nextGremlin;
	gCurrentDepth	= nextDepth;

	// All the Gremlins have reached gMaxDepth when the depth exceeds the
	// depth necessary to reach gMaxDepth. Special case for
	// gMaxDepth = forever.

	if ( gMaxDepth != -1 &&
		( (gCurrentDepth > gMaxDepth / gSwitchDepth) ||
		  ( (gCurrentDepth == gMaxDepth / gSwitchDepth) && (gMaxDepth % gSwitchDepth == 0) ) ) )
	{
		Hordes::EndHordes();
		return;
	}

	// If the current depth is 0, we start at the root state.

	if (gCurrentDepth == 0)
	{
		EmAssert (gSession);
		gSession->ScheduleNextGremlinFromRootState ();
	}

	// Otherwise, we load the suspended state, which is where we begin to
	// resume the Gremlin

	else
	{
		EmAssert (gSession);
		gSession->ScheduleNextGremlinFromSuspendedState ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::ErrorEncountered
 *
 * DESCRIPTION: Called when an error condition has been encountered.
 *				This function saves the current state and starts in
 *				motion the machinery to start the next Gremlin in the
 *				Horde.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::ErrorEncountered (void)
{
	int32	errorGremlin	= Hordes::GremlinNumber ();
	int32	errorEvent		= Hordes::EventCounter () - 1;

	Hordes::AutoSaveState ();

	LogAppendMsg ("=== ERROR: Gremlin #%ld terminated in error at event #%ld\n",
		errorGremlin, errorEvent);

	LogDump ();

	// This is a fatal error; stop the execution of this Gremlin.

	gGremlinHaltedInError[errorGremlin].fHalted = true;

	// Save the events, now that it's terminated with an error event.

	Hordes::SaveEvents ();

	// Move to the next Gremlin.

//	if (!Hordes::InSingleGremlinMode ())
	{
		Hordes::NextGremlin ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::RecordErrorStats
 *
 * DESCRIPTION: Called when an error message needs to be displayed,
 *				either as a result of a hardware exception, an error
 *				condition detected by Poser, or the Palm application
 *				calling SysFatalAlert.  If appropriate, log the error
 *				message.
 *
 * PARAMETERS:	messageID - ID indicating what error occurred.  We pass
 *					that into here so that we can keep stats on the kinds
 *					of errors generated.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
Hordes::RecordErrorStats (StrCode messageID)
{
	if (Hordes::IsOn ())
	{
		// Update our gGremlinHaltedInError info if it was passed, and if
		// we haven't already assigned an error to this Gremlin. (This is
		// so that the first error, of possibly many if we are continuing
		// on errors, is the one that shows up in the statistics.)

		int32	errorGremlin	= Hordes::GremlinNumber();
		int32	errorEvent		= Hordes::EventCounter () - 1;

		// Only update this if we haven't already logged an error event for the first
		// error

		Bool firstErrForGremlin = gGremlinHaltedInError[errorGremlin].fMessageID == -1;

		if (firstErrForGremlin)
		{
			gGremlinHaltedInError[errorGremlin].fErrorEvent = errorEvent;

			// Now update the message id if a valid one was passed

			if (messageID != -1)
			{
				gGremlinHaltedInError[errorGremlin].fMessageID = (long) messageID;
			}
		}

		// Tell Minimization that a warning or error occurred.
		//
		// Do we ever get here?  RecordErrorStats is called from Errors::DoDialog.
		// Errors::DoDialog is called from Errors::HandleDialog, which calls
		// EmMinimize::ErrorOccurred and returns before calling Errors::DoDialog
		// if minimization is turned on.

		if (EmMinimize::IsOn ())
		{
			EmAssert (false);	// See if we get here.

			LogAppendMsg ("Calling EmMinimize::ErrorOccurred from Hordes::RecordErrorStats");
			EmMinimize::ErrorOccurred ();
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::StopEventReached
 *
 * DESCRIPTION: Message to Hordes indicating that a Gremlin has
 *				completed its last event. Saves a suspended state if
 *				we intend to resume this Gremlin in the future.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::StopEventReached()
{
	int32	gremlinNumber	= Hordes::GremlinNumber ();
	int32	stopEventNumber	= Hordes::EventLimit ();

	LogAppendMsg ("Gremlin #%ld finished successfully to event #%ld",
					gremlinNumber, stopEventNumber);

	if (stopEventNumber == gMaxDepth)
	{
//		LogAppendMsg ("********************************************************************************");
		LogAppendMsg ("*************   Gremlin #%ld successfully completed", gremlinNumber);
//		LogAppendMsg ("********************************************************************************");
	}

	LogDump ();

	// Save the events of the successful run.

	Hordes::SaveEvents ();

	// Move to the next Gremlin.

	Hordes::NextGremlin ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::Suspend
 *
 * DESCRIPTION: Suspends the currently running Gremlin.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::Suspend()
{
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::Step
 *
 * DESCRIPTION: "Steps" the currently running Gremlin.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::Step()
{
	if (!gIsOn)
	{
		Hordes::TurnOn (true);

		// Spoof info to look as if this was a single Gremlin run. This way,
		// stepping will work; if this is not done, then the next Gremlin is
		// launched, just as if a switching barrier was reached.

		gCurrentDepth = gMaxDepth;
		gGremlinStartNumber = gGremlinStopNumber = gCurrentGremlin;

		gTheGremlin.Step ();

		// Make sure the app's awake.  Normally, we post events on a patch to
		// SysEvGroupWait.  However, if the Palm device is already waiting,
		// then that trap will never get called.  By calling EvtWakeup now,
		// we'll wake up the Palm device from its nap.

		Errors::ThrowIfPalmError (EvtWakeup ());
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::Resume
 *
 * DESCRIPTION: Resumes the currently suspended Gremlin.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::Resume()
{
	if (!Hordes::IsOn ())
	{
		Hordes::TurnOn (true);
		
		gStartTime = Platform::GetMilliseconds () - (gStopTime - gStartTime);

		gTheGremlin.RestoreFinalUntil ();

		gTheGremlin.Resume ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::Stop
 *
 * DESCRIPTION: Suspends the currently running Gremlin.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::Stop (void)
{
	gStopTime = Platform::GetMilliseconds ();

	StubAppGremlinsOff ();

	gTheGremlin.Stop ();
}


/*****************************************************************************
 *
 * FUNCTION:	Hordes::SuggestFileName
 *
 * DESCRIPTION: This function is responsible for deciding about the names of
 *				files that are created during a horde run.  The file names 
 *				are assigned based on the requested file category. Some 
 *				file categories use gremlins data (event number, gremlin 
 *				number) to construct a unique file name.  The following 
 *				categories are used by the "Horde" class:
 *
 *					kHordeProgressFile
 *					kHordeRootFile
 *					kHordeSuspendFile
 *					kHordeAutoCurrentFile
 *
 *					kHordeSuspendFile	-	last file in a gremlin thread
 *
 * PARAMETERS:	file category
 *
 * RETURNED:	file name or an empty string when the input category is
 *				incorrect
 *
 *****************************************************************************/

string Hordes::SuggestFileName (HordeFileType category, uint32 num)
{
	static const char kStrSearchProgressFile[]	= "Gremlin_Search_Progress.dat";
	static const char kStrRootStateFile[]		= "Gremlin_Root_State.psf";
	static const char kStrSuspendedStateFile[]	= "Gremlin_%03ld_Suspended.psf";
	static const char kStrAutoSaveFile[]		= "Gremlin_%03ld_Event_%08ld.psf";
	static const char kStrEventFile[]			= "Gremlin_%03ld_Events.pev";
	static const char kStrMinimalEventFile []	= "Gremlin_%03ld_Interim_Event_File_%08ld.pev";

	char fileName[64];

	int32	gremlinNumber;
	int32	eventCounter;
	uint32	time;

	switch (category)
	{
		case kHordeProgressFile:

			strcpy (fileName, kStrSearchProgressFile);
			break;

		case kHordeRootFile:

			strcpy (fileName, kStrRootStateFile);
			break;

		case kHordeSuspendFile:

			gremlinNumber = Hordes::GremlinNumber ();
			sprintf (fileName, kStrSuspendedStateFile, gremlinNumber);
			break;

		case kHordeAutoCurrentFile:

			gremlinNumber = Hordes::GremlinNumber ();
			eventCounter = Hordes::EventCounter ();

			if (gGremlinSaveFrequency == 0)
			{
				sprintf (fileName, kStrAutoSaveFile, gremlinNumber, eventCounter);
			}
			else
			{
				eventCounter = (eventCounter / gGremlinSaveFrequency) * gGremlinSaveFrequency;
				sprintf (fileName, kStrAutoSaveFile, gremlinNumber, eventCounter);
			}
			break;

		case kHordeEventFile:

			gremlinNumber = Hordes::GremlinNumber ();
			sprintf (fileName, kStrEventFile, gremlinNumber);
			break;

		case kHordeMinimalEventFile:

			gremlinNumber = num;
			time = Platform::GetMilliseconds ();
			sprintf (fileName, kStrMinimalEventFile, gremlinNumber, time);
			break;

		default:

			*fileName = '\0';
			break;
	};

	return string (fileName);
}


/*****************************************************************************
 *
 * FUNCTION:	Hordes::SuggestFileRef
 *
 * DESCRIPTION: This function is responsible for deciding about the names of
 *				files that are created during a horde run.  The file names 
 *				are assigned based on the requested file category. Some 
 *				file categories use gremlins data (event number, gremlin 
 *				number) to construct a unique file name.  The following 
 *				categories are used by the "Horde" class:
 *
 *					kHordeProgressFile
 *					kHordeRootFile
 *					kHordeSuspendFile
 *					kHordeAutoCurrentFile
 *
 *					kHordeSuspendFile	-	last file in a gremlin thread
 *
 * PARAMETERS:	file category
 *
 * RETURNED:	file ref
 *
 *****************************************************************************/

EmFileRef Hordes::SuggestFileRef (HordeFileType category, uint32 num)
{
	EmFileRef	fileRef (
		Hordes::GetGremlinDirectory (),
		Hordes::SuggestFileName (category, num));

	return fileRef;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::PostLoad
 *
 * DESCRIPTION: Initializes a load that has taken place outside the
 *				control of the Hordes subsystem. For example, if the user
 *				has opened a session file manually. This is to set the
 *				Hordes state to play the Gremlin in the file as a "horde
 *				of one."
 *
 * PARAMETERS:	f - SessionFile to load from
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::PostLoad (void)
{
	// We can't just call NewGremlin with the GremlinInfo because
	// gTheGremlin.Load() has already restored the state of the Gremlin,
	// so we should not call gTheGremlin.New() on it.

	Preference<GremlinInfo>	pref (kPrefKeyGremlinInfo);
	GremlinInfo info = *pref;

	gGremlinStartNumber		= info.fNumber;
	gGremlinStopNumber		= info.fNumber;
	gCurrentGremlin			= info.fNumber;
	gSwitchDepth			= info.fSteps;
	gMaxDepth				= info.fSteps;
	gGremlinSaveFrequency	= info.fSaveFrequency;
	gGremlinAppList			= info.fAppList;
	gCurrentDepth			= 0;

	gStartTime	= gTheGremlin.GetStartTime ();
	gStopTime	= gTheGremlin.GetStopTime ();

	if (Hordes::IsOn())
	{
		Hordes::UseNewAutoSaveDirectory ();

		EmAssert (gSession);
		gSession->ScheduleSaveRootState ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::PostFakeEvent
 *
 * DESCRIPTION: Posts a fake event through the currently running Gremlin.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	TRUE if a key or point was enqueued, FALSE otherwise.
 *
 ***********************************************************************/

Bool
Hordes::PostFakeEvent (void)
{
	// check to see if the Gremlin has produced its max # of "events."

	if (Hordes::EventLimit() > 0 && Hordes::EventCounter () > Hordes::EventLimit ())
	{
		Hordes::StopEventReached ();
		return false;
	}

	Bool result = gTheGremlin.GetFakeEvent ();

	Hordes::BumpCounter ();

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::PostFakePenEvent
 *
 * DESCRIPTION: Posts a phony pen movement to through the currently
 *				running Gremlin
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::PostFakePenEvent (void)
{
	Hordes::BumpCounter ();
	gTheGremlin.GetPenMovement ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::SendCharsToType
 *
 * DESCRIPTION: Send a char to the Emulator if any are pending for the
 *				currently running Gremlin
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

Bool
Hordes::SendCharsToType (void)
{
	return gTheGremlin.SendCharsToType ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::ElapsedMilliseconds
 *
 * DESCRIPTION: Returns the elapsed time of the Horde
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

uint32
Hordes::ElapsedMilliseconds (void)
{
	if (gIsOn)
		return Platform::GetMilliseconds () - gStartTime;

	return gStopTime - gStartTime;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::BumpCounter
 *
 * DESCRIPTION: Bumps event counter of the currently running Gremlin
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::BumpCounter (void)
{
	gTheGremlin.BumpCounter ();

	if (gGremlinSaveFrequency != 0 &&
		(Hordes::EventCounter () % gGremlinSaveFrequency) == 0)
	{
		EmAssert (gSession);
		gSession->ScheduleAutoSaveState ();
	}

	if (Hordes::EventLimit () > 0 &&
		Hordes::EventLimit () == Hordes::EventCounter ())
	{	
		EmAssert (gSession);
		gSession->ScheduleSaveSuspendedState ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::CanSwitchToApp
 *
 * DESCRIPTION: Returns whether the Horde can switch to the given Palm
 *				app
 *
 * PARAMETERS:	cardNo   \ Palm application info
 *				dbID     /
 *
 * RETURNED:	TRUE if the designated Palm app can be run by the Horde
 *				FALSE otherwise
 *
 ***********************************************************************/

Bool
Hordes::CanSwitchToApp (UInt16 cardNo, LocalID dbID)
{
	if (gGremlinAppList.size () == 0)
		return true;

	DatabaseInfoList			appList = Hordes::GetAppList ();
	DatabaseInfoList::iterator	iter = appList.begin ();

	while (iter != appList.end ())
	{
		DatabaseInfo&	dbInfo = *iter++;

		if (dbInfo.cardNo == cardNo && dbInfo.dbID == dbID)
			return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::SetGremlinsHome
 *
 * DESCRIPTION: Indicates the directory in which the user would like
 *				his Horde files to collect.
 *
 * PARAMETERS:	gremlinsHome - the name of the directory
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
Hordes::SetGremlinsHome (const EmDirRef& gremlinsHome)
{
	gHomeForHordesFiles = gremlinsHome;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::SetGremlinsHomeToDefault
 *
 * DESCRIPTION: Indicates that the user would like his Horde files to
 *				collect in the default location.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
Hordes::SetGremlinsHomeToDefault (void)
{
	gHomeForHordesFiles = EmDirRef ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::GetGremlinsHome
 *
 * DESCRIPTION: Returns the directory that the user would like to house
 *				his Horde state files.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	TRUE if gHomeForHordesFiles is defined;
 *				FALSE otherwise (use default).
 *
 ***********************************************************************/

Bool
Hordes::GetGremlinsHome (EmDirRef& outPath)
{
	// If we don't have anything, default to Poser home.

	outPath = gHomeForHordesFiles;

	// Try to create the path if it doesn't exist.

	if (!gHomeForHordesFiles.Exists ())
	{
		gHomeForHordesFiles.Create ();
	}

	// Return whether or not we succeeded.

	return gHomeForHordesFiles.Exists ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::AutoSaveState
 *
 * DESCRIPTION: Creates a file reference to where the auto-saved state
 *				should be saved.  Then calls a platform-specific
 *				routine to do the actual saving.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
Hordes::AutoSaveState (void)
{
	EmFileRef	fileRef = Hordes::SuggestFileRef (kHordeAutoCurrentFile);

	EmAssert (gSession);
	gSession->Save (fileRef, false);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::SaveRootState
 *
 * DESCRIPTION: Creates a file reference to where the state
 *				should be saved.  Then calls a platform-specific
 *				routine to do the actual saving.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
Hordes::SaveRootState (void)
{
	EmFileRef	fileRef = Hordes::SuggestFileRef (kHordeRootFile);

	Bool hordesWasOn = Hordes::IsOn ();

	if (hordesWasOn != false)
		Hordes::TurnOn (false);

	EmAssert (gSession);
	gSession->Save (fileRef, false);

	Hordes::TurnOn (hordesWasOn);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::LoadState
 *
 * DESCRIPTION: Does the work of loading a state while Hordes is running.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode
Hordes::LoadState (const EmFileRef& ref)
{
	ErrCode returnedErrCode = errNone;

	try
	{
		EmAssert (gSession);
		gSession->Load (ref);
	}
	catch (ErrCode errCode)
	{
		Hordes::TurnOn (false);

		Errors::SetParameter ("%filename", ref.GetName ());
		Errors::ReportIfError (kStr_CmdOpen, errCode, 0, true);

		returnedErrCode = errCode;
	}

	return returnedErrCode;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::LoadRootState
 *
 * DESCRIPTION: Creates a file reference to where the root state
 *				should be loaded.  Then calls a
 *				routine to do the actual loading.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode
Hordes::LoadRootState (void)
{
	EmFileRef	fileRef = Hordes::SuggestFileRef (kHordeRootFile);

	ErrCode		result = Hordes::LoadState (fileRef);

	if (result == 0)
	{
		// There are no events to load, but we need to make sure we at
		// least clear out any old events.

		EmEventPlayback::Clear ();
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::SaveSuspendedState
 *
 * DESCRIPTION: Creates a file reference to where the suspended state
 *				should be saved.  Then calls a platform-specific
 *				routine to do the actual saving.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
Hordes::SaveSuspendedState (void)
{
	EmFileRef	fileRef = Hordes::SuggestFileRef (kHordeSuspendFile);

	gSession->Save (fileRef, false);

	// This sort of overloads the function, but right now, any time we
	// save the suspend state, we also want to save any recorded events.

	Hordes::SaveEvents ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::LoadSuspendedState
 *
 * DESCRIPTION: Creates a file reference to where the suspended state
 *				should be loaded.  Then calls a	routine to do the actual
 *				loading.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode
Hordes::LoadSuspendedState (void)
{
	EmFileRef	fileRef = Hordes::SuggestFileRef (kHordeSuspendFile);

	ErrCode		result = Hordes::LoadState (fileRef);

	if (result == 0)
	{
		// Load the events from the associated file holding them.  Do this
		// *after* Hordes::LoadState, as gSession->Load will try to load
		// events from *its* file, overwriting the ones in our holding file.
		//
		// Note also that we load events in LoadSuspendedState but not
		// LoadRootState, as there should not be any events associated
		// with the root state (none have been generated!).

		Hordes::LoadEvents ();
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::SaveEvents
 *
 * DESCRIPTION: Write out the current set of events to the designated
 *				event file.  This file contains the root (initial)
 *				Gremlin state, to which we append the events.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
Hordes::SaveEvents (void)
{
	EmFileRef		eventRef = Hordes::SuggestFileRef (kHordeEventFile);

	EmStreamFile	eventStream (eventRef, kCreateOrEraseForWrite,
						kFileCreatorEmulator, kFileTypeEvents);
	ChunkFile		eventChunkFile (eventStream);
	SessionFile		eventSessionFile (eventChunkFile);

	// Copy over the root state to the session file, first.

	EmFileRef		rootRef = Hordes::SuggestFileRef (kHordeRootFile);
	EmStreamFile	rootStream (rootRef, kOpenExistingForRead,
						kFileCreatorEmulator, kFileTypeEvents);

	{
		int32		length = rootStream.GetLength ();
		ByteList	buffer (length);
		rootStream.GetBytes (&buffer[0], length);
		eventStream.PutBytes (&buffer[0], length);
	}

	// Finally, write the events to the file.

	EmEventPlayback::SaveEvents (eventSessionFile);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::LoadEvents
 *
 * DESCRIPTION: Load the events from the current event file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void
Hordes::LoadEvents (void)
{
	EmFileRef		eventRef = Hordes::SuggestFileRef (kHordeEventFile);

	EmStreamFile	stream (eventRef, kOpenExistingForRead,
						kFileCreatorEmulator, kFileTypeEvents);
	ChunkFile		chunkFile (stream);
	SessionFile		eventFile (chunkFile);

	EmEventPlayback::LoadEvents (eventFile);
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::StartLog
 *
 * DESCRIPTION: Starts Hordes logging
 *
 * PARAMETERS:	none
 *
 * RETURNED:	none
 *
 ***********************************************************************/

void
Hordes::StartLog (void)
{
	LogClear ();
	LogStartNew ();

	LogAppendMsg ("********************************************************************************");
	LogAppendMsg ("*************   Gremlin Hordes started");
	LogAppendMsg ("********************************************************************************");
	LogAppendMsg ("Running Gremlins %ld to %ld", gGremlinStartNumber, gGremlinStopNumber);

	if (gSwitchDepth != -1)
		LogAppendMsg ("Will run each Gremlin %ld events at a time until all Gremlins have terminated in error", gSwitchDepth);

	else
		LogAppendMsg ("Will run each Gremlin until all Gremlins have terminated in error", gSwitchDepth);

	if (gMaxDepth != -1)
		LogAppendMsg ("or have reached a maximum of %ld events", gMaxDepth);

	LogAppendMsg ("********************************************************************************");

	LogDump ();
}


/***********************************************************************
 *
 * FUNCTION:	Hordes::GetGremlinDirectory
 *
 * DESCRIPTION:	Return an EmDirRef for directory where information
 *				about the current Gremlin is saved.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The desired EmDirRef.
 *
 ***********************************************************************/

EmDirRef
Hordes::GetGremlinDirectory (void)
{
	// If requested, create the directory to use.

	if (gForceNewHordesDirectory)
	{
		gForceNewHordesDirectory = false;

		time_t		now_time;
		time (&now_time);
		struct tm*	now_tm = localtime (&now_time);

		char buffer[30];
		strftime (buffer, countof (buffer), "%Y-%m-%d.%H-%M-%S", now_tm);	// 19 chars + NULL

		char	stateName[30];
		sprintf (stateName, "Gremlins.%s", buffer);

		EmAssert (strlen(stateName) <= 31);	// Max on Macs

		EmDirRef	homeDir;

		if (!Hordes::GetGremlinsHome (homeDir))
		{
			homeDir = EmDirRef::GetEmulatorDirectory ();
		}

		gGremlinDir = EmDirRef (homeDir, stateName);

		if (!gGremlinDir.Exists ())
		{
			try
			{
				gGremlinDir.Create ();
			}
			catch (...)
			{
				// !!! Put up some error message

				gGremlinDir = EmDirRef ();
			}
		}
	}

	// Otherwise, return the previously specified directory.

	return gGremlinDir;
}


// ---------------------------------------------------------------------------
//		¥ Hordes::UseNewAutoSaveDirectory
// ---------------------------------------------------------------------------

void Hordes::UseNewAutoSaveDirectory (void)
{
	gForceNewHordesDirectory = true;
}


// ---------------------------------------------------------------------------
//		¥ Hordes::ComputeStatistics
// ---------------------------------------------------------------------------

void Hordes::ComputeStatistics (int32 &min,
								int32 &max,
								int32 &avg,
								int32 &stdDev,
								int32 &smallErrorIndex)
{
	// I'm worried about overflow, but in practice this should be sufficiently large

	int32 sum = 0; 

	// this is the largest value possible with an int32; it is effectively infinity

	min = 0x7FFFFFFF;
	max = 0;

	// initialize this to a sentinel value

	smallErrorIndex = 0x7FFFFFFF;

	int32 numEventsToErr = 0;
	int32 counter = 0;
	int32 errorCounter = 0;

	EmAssert (gGremlinStartNumber <= gGremlinStopNumber);

	for (counter = gGremlinStartNumber; counter <= gGremlinStopNumber; counter++)
	{
		numEventsToErr = gGremlinHaltedInError[counter].fErrorEvent;
		sum += numEventsToErr;

		if (numEventsToErr > max)
		{
			max = numEventsToErr;
		}

		if (numEventsToErr < min && numEventsToErr > 0)
		{
			min = numEventsToErr;
		}

		if (numEventsToErr != 0)
		{
			errorCounter++;

			if ((smallErrorIndex == 0x7FFFFFFF) ||
				(numEventsToErr < gGremlinHaltedInError[smallErrorIndex].fErrorEvent))
			{
				smallErrorIndex = counter;
			}
		}
	}

	if (sum > 0 && errorCounter > 0)
	{
		avg = sum / errorCounter;
	}
	else
	{
		avg = 0;
		stdDev = 0;

		return;
	}

	// now to calculate the standard deviation

	int32 diffSquaredSum = 0;

	for (counter = gGremlinStartNumber; counter <= gGremlinStopNumber; counter++)
	{
		numEventsToErr = gGremlinHaltedInError[counter].fErrorEvent - avg;
		diffSquaredSum += (numEventsToErr * numEventsToErr);
	}

	// I'm assuming that MAXGREMLINS is not 0. Since it is defined, and
	// constant, I think this is a safe assumption.

	diffSquaredSum /= MAXGREMLINS;			// total - 1

	stdDev = (int32) sqrt (diffSquaredSum);

	return;
}


// ---------------------------------------------------------------------------
//		¥ Hordes::GremlinReport
// ---------------------------------------------------------------------------

void Hordes::GremlinReport (void)
{
	int32	counter					= 0;
	int32	lastSmallIndex			= 0;
	int32	numGremlinsWithError	= 0;
	int32	highestFrequency		= 1;

	// We want a table of sorts showing all of the errors encountered,
	// sorted by frequency. Errors that didn't crop up will be omitted,
	// as they are uninteresting. Alongside each error will be the count
	// of how many discrete Gremlins terminated with the error, and the
	// Gremlin number of the offender which terminated after the least
	// number of events. The errors that will be handled have their
	// constants defined in Strings.r.h

	// There are several interesting problems to deal with: the error
	// distribution, as compared to the number of Gremlins that were run,
	// is likely to be quite low; we would like to come up with the
	// Gremlin that terminates quickest, for each error; and we would like
	// to know which error is most prevalent.

	// According to Strings.r.h, error codes will be in the range {1000, 1199}.
	// Let's set up some offsets, and an array that will eventually hold
	// the error prevalence data.

	const int32		errorBase	= 1000;
	const int32		errorLast	= 1199;

	EmGremlinErrorFrequencyInfo errorCountArray [errorLast - errorBase + 1];

	// Let's initialize the array, using the offset notation that will be
	// used further on, just to be consistent. The first field will contain
	// the count of the error, the second will contain the Gremlin that
	// terminated quickest with the error.

	for (counter = errorBase; counter <= errorLast; counter++)
	{
		errorCountArray [counter - errorBase].fCount = 0;
		errorCountArray [counter - errorBase].fErrorFrequency = 0;

		// sentinel value, so that we know that we haven't seen this error before

		errorCountArray [counter - errorBase].fFirstErrantGremlinIndex = -1;
	}

	// Now let's walk through the gGremlinHaltedInError array, and for each
	// Gremlin that terminated in error, increment the error count for the
	// appropriate error

	int32	errorTypeNumber	= 0;
	int32	temp			= 0;

	EmAssert (gGremlinStartNumber <= gGremlinStopNumber);
	for (counter = gGremlinStartNumber; counter <= gGremlinStopNumber; counter++)
	{
		if (gGremlinHaltedInError[counter].fHalted != false)
		{
			errorTypeNumber = gGremlinHaltedInError[counter].fMessageID - errorBase;

			errorCountArray[errorTypeNumber].fErrorFrequency += 1;
			numGremlinsWithError += 1;

			// if we have not seen this error before, then this Gremlin's index
			// is, by definition, the index of the Gremlin which terminated first
			// with this error.

			lastSmallIndex = errorCountArray[errorTypeNumber].fFirstErrantGremlinIndex;

			if (lastSmallIndex == -1)
			{
				errorCountArray[errorTypeNumber].fCount = gGremlinHaltedInError[counter].fErrorEvent;
				errorCountArray[errorTypeNumber].fFirstErrantGremlinIndex = counter;
			}

			// otherwise, we have seen this error before, so check whether this
			// Gremlin terminated before the last frontrunner did.

			else
			{
				highestFrequency += 1;

				temp = gGremlinHaltedInError[counter].fErrorEvent;

				if (temp < gGremlinHaltedInError[lastSmallIndex].fErrorEvent)
				{
					errorCountArray[errorTypeNumber].fCount = temp;
					errorCountArray[errorTypeNumber].fFirstErrantGremlinIndex = counter;
				}
			}
		}
	}

	// So now we have an array filled with the number of times each error occurred,
	// and the # of the first-offending Gremlin. Now all that remains is to sort
	// the errors by their frequency, and then output our results. Some vital
	// information about the frequencies: their sum is numGremlinsWithError, and
	// the highest frequency of any single error is <= the number of Gremlins with
	// errors - 2 * the number of discrete errors that occurred multiple times.
	// With these constraints in mind, this following algorithm, ostensibly n^2
	// time, is in reality of managable time since n is constrained as above.
	// Why I am doing this is because I don't think that n is sufficiently large
	// to warrant the overhead of a quicksort, and because I don't feel like
	// writing a quicksort for a multi-dimensional array.

	if (numGremlinsWithError == 0)
	{
		return;
	}

	LogAppendMsg ("%d of %d Gremlins terminated in error.", numGremlinsWithError,
					gGremlinStopNumber - gGremlinStartNumber + 1);
	LogAppendMsg ("");
	LogAppendMsg ("Count               Error name                    Shortest Gremlin    Events");

	int32 errorNumber = 0;
	int32 frequency = 0;
	string str;

	// We only go down to 1, since we don't care about the errors that didn't
	// occur.

	for (frequency = highestFrequency;
		 frequency > 0;
		 frequency--)
	{
		for (errorNumber = errorBase; errorNumber <= errorLast; errorNumber++)
		{
			if (errorCountArray[errorNumber - errorBase].fErrorFrequency == frequency)
			{
				str = Hordes::TranslateErrorCode (errorNumber);
				LogAppendMsg ("%-4d                %-29s Gremlin #%-3d        %-4d",
					frequency,
					str.c_str (),
					errorCountArray[errorNumber - errorBase].fFirstErrantGremlinIndex,
					errorCountArray[errorNumber - errorBase].fCount);
			}
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ Hordes::GetAppList
// ---------------------------------------------------------------------------
// Return the of applications that can be run under this Gremlin.

DatabaseInfoList Hordes::GetAppList (void)
{
	// If it looks like some of the fields need to filled out,
	// then do that now (some versions of Poser saved only the
	// creator, type, and version fields, leaving the others blank
	// -- we have to put back that information now).

	if (gGremlinAppList.size () > 0 && gGremlinAppList.front ().dbID == 0)
	{
		// Get the list of applications.

		DatabaseInfoList	appList;
		::GetDatabases (appList, kApplicationsOnly);

		// Iterate over all the allowed Gremlins applications and all the
		// installed applications, and use information in the latter to
		// fill in the former.

		DatabaseInfoList::iterator	gremlin_iter = gGremlinAppList.begin ();
		while (gremlin_iter != gGremlinAppList.end ())
		{
			DatabaseInfoList::iterator	installed_iter = appList.begin ();
			while (installed_iter != appList.end ())
			{
				if (gremlin_iter->creator == installed_iter->creator &&
					gremlin_iter->type == installed_iter->type &&
					gremlin_iter->version == installed_iter->version)
				{
					*gremlin_iter = *installed_iter;
					break;
				}

				++installed_iter;
			}

			++gremlin_iter;
		}
	}

	return gGremlinAppList;
}


// ---------------------------------------------------------------------------
//		¥ Hordes::TranslateErrorCode
// ---------------------------------------------------------------------------

string Hordes::TranslateErrorCode (UInt32 errCode)
{
	switch (errCode)
	{
		case kStr_OpError:
			return "OpError";
			break;

		case kStr_OpErrorRecover:
			return "OpErrorRecover";
			break;

		case kStr_ErrBusError:
			return "ErrBusError";
			break;

		case kStr_ErrAddressError:
			return "ErrAddressError";
			break;

		case kStr_ErrIllegalInstruction:
			return "ErrIllegalInstruction";
			break;

		case kStr_ErrDivideByZero:
			return "ErrDivideByZero";
			break;

		case kStr_ErrCHKInstruction:
			return "ErrCHKInstruction";
			break;

		case kStr_ErrTRAPVInstruction:
			return "ErrTRAPVInstruction";
			break;

		case kStr_ErrPrivilegeViolation:
			return "ErrPrivilegeViolation";
			break;

		case kStr_ErrTrace:
			return "ErrTrace";
			break;

		case kStr_ErrATrap:
			return "ErrATrap";
			break;

		case kStr_ErrFTrap:
			return "ErrFTrap";
			break;

		case kStr_ErrTRAPx:
			return "ErrTRAPx";
			break;

		case kStr_ErrStorageHeap:
			return "ErrStorageHeap";
			break;

		case kStr_ErrNoDrawWindow:
			return "ErrNoDrawWindow";
			break;

		case kStr_ErrNoGlobals:
			return "ErrNoGlobals";
			break;

		case kStr_ErrSANE:
			return "ErrSANE";
			break;

		case kStr_ErrTRAP0:
			return "ErrTRAP0";
			break;

		case kStr_ErrTRAP8:
			return "ErrTRAP8";
			break;

		case kStr_ErrStackOverflow:
			return "ErrStackOverflow";
			break;

		case kStr_ErrUnimplementedTrap:
			return "ErrUnimplementedTrap";
			break;

		case kStr_ErrInvalidRefNum:
			return "ErrInvalidRefNum";
			break;

		case kStr_ErrCorruptedHeap:
			return "ErrCorruptedHeap";
			break;

		case kStr_ErrInvalidPC1:
			return "ErrInvalidPC1";
			break;

		case kStr_ErrInvalidPC2:
			return "ErrInvalidPC2";
			break;

		case kStr_ErrLowMemory:
			return "ErrLowMemory";
			break;

		case kStr_ErrSystemGlobals:
			return "ErrSystemGlobals";
			break;

		case kStr_ErrScreen:
			return "ErrScreen";
			break;

		case kStr_ErrHardwareRegisters:
			return "ErrHardwareRegisters";
			break;

		case kStr_ErrROM:
			return "ErrROM";
			break;

		case kStr_ErrMemMgrStructures:
			return "ErrMemMgrStructures";
			break;

		case kStr_ErrMemMgrSemaphore:
			return "ErrMemMgrSemaphore";
			break;

		case kStr_ErrFreeChunk:
			return "ErrFreeChunk";
			break;

		case kStr_ErrUnlockedChunk:
			return "ErrUnlockedChunk";
			break;

		case kStr_ErrLowStack:
			return "ErrLowStack";
			break;

		case kStr_ErrStackFull:
			return "ErrStackFull";
			break;

		case kStr_ErrSizelessObject:
			return "ErrSizelessObject";
			break;

		case kStr_ErrOffscreenObject:
			return "ErrOffscreenObject";
			break;

		case kStr_ErrFormAccess:
			return "ErrFormAccess";
			break;

		case kStr_ErrFormObjectListAccess:
			return "ErrFormObjectListAccess";
			break;

		case kStr_ErrFormObjectAccess:
			return "ErrFormObjectAccess";
			break;

		case kStr_ErrWindowAccess:
			return "ErrWindowAccess";
			break;

		case kStr_ErrBitmapAccess:
			return "ErrBitmapAccess";
			break;

		case kStr_ErrProscribedFunction:
			return "ErrProscribedFunction";
			break;

		case kStr_ErrStepSpy:
			return "ErrStepSpy";
			break;

		case kStr_ErrWatchpoint:
			return "ErrWatchpoint";
			break;

		case kStr_ErrSysFatalAlert:
			return "ErrSysFatalAlert";
			break;

		case kStr_ErrDbgMessage:
			return "ErrDbgMessage";
			break;

		case kStr_BadChecksum:
			return "BadChecksum";
			break;

		case kStr_UnknownDeviceWarning:
			return "UnknownDeviceWarning";
			break;

		case kStr_UnknownDeviceError:
			return "UnknownDeviceError";
			break;

		case kStr_MissingSkins:
			return "MissingSkins";
			break;

		case kStr_InconsistentDatabaseDates:
			return "InconsistentDatabaseDates";
			break;

		case kStr_NULLDatabaseDate:
			return "NULLDatabaseDate";
			break;

		case kStr_NeedHostFS:
			return "NeedHostFS";
			break;

		case kStr_InvalidAddressNotEven:
			return "InvalidAddressNotEven";
			break;

		case kStr_InvalidAddressNotInROMOrRAM:
			return "InvalidAddressNotInROMOrRAM";
			break;

		case kStr_CannotParseCondition:
			return "CannotParseCondition";
			break;

		case kStr_UserNameTooLong:
			return "UserNameTooLong";
			break;

		case kStr_ErrMemoryLeak:
			return "ErrMemoryLeak";
			break;

		case kStr_ErrMemoryLeaks:
			return "ErrMemoryLeaks";
			break;

		default:
			EmAssert (false);
	}

	return "";
}

