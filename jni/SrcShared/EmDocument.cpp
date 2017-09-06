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
#include "EmDocument.h"

#include "EmApplication.h"		// SetDocument
#include "EmCommands.h"			// EmCommandID, kCommandSessionClose, etc.
#include "EmPatchState.h"		// EmPatchState::EvtGetEventCalled
#include "EmRPC.h"				// RPC::Idle
#include "EmSession.h"			// EmStopMethod
#include "EmWindow.h"			// EmWindow, gWindow
#include "ErrorHandling.h"		// Errors::ReportIfError
#include "Hordes.h"				// Hordes::PostLoad, Suspend, Step, Resume, Stop
#include "Platform.h"			// Platform::GetBoundDevice
#include "SocketMessaging.h"	// CSocket::IdleAll
#include "Startup.h"			// Startup::NewHorde
#include "Strings.r.h"			// kStr_CmdClose, etc.

#include "PHEMNativeIF.h"

#if HAS_PROFILING
#include "Profiling.h"			// ProfileStart, ProfileStop, ProfileDump
#endif

EmDocument* gDocument;

typedef void (EmDocument::*EmCommandFn)(EmCommandID);


static const struct
{
	EmCommandID		fCommandID;
	EmCommandFn		fFn;
	StrCode			fErrStrCode;
}
kCommand[] =
{
	{ kCommandSessionSave,		&EmDocument::DoSave,			kStr_CmdSave			},
	{ kCommandSessionSaveAs,	&EmDocument::DoSaveAs,			kStr_CmdSave			},
	{ kCommandSessionBound,		&EmDocument::DoSaveBound,		kStr_CmdSaveBound		},
	{ kCommandScreenSave,		&EmDocument::DoSaveScreen,		kStr_CmdSaveScreen		},
	{ kCommandSessionInfo,		&EmDocument::DoInfo,			kStr_CmdSessionInfo		},
	{ kCommandImportOther,		&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport0,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport1,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport2,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport3,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport4,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport5,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport6,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport7,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport8,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandImport9,			&EmDocument::DoImport,			kStr_CmdInstall			},
	{ kCommandExport,			&EmDocument::DoExport,			kStr_CmdInstall			},
	{ kCommandHotSync,			&EmDocument::DoHotSync,			kStr_CmdHotSync			},
	{ kCommandReset,			&EmDocument::DoReset,			kStr_CmdReset			},
	{ kCommandGremlinsNew,		&EmDocument::DoGremlinNew,		kStr_CmdGremlinNew		},
	{ kCommandGremlinsSuspend,	&EmDocument::DoGremlinSuspend,	kStr_CmdGremlinSuspend	},
	{ kCommandGremlinsStep,		&EmDocument::DoGremlinStep,		kStr_CmdGremlinStep		},
	{ kCommandGremlinsResume,	&EmDocument::DoGremlinResume,	kStr_CmdGremlinResume	},
	{ kCommandGremlinsStop,		&EmDocument::DoGremlinStop,		kStr_CmdGremlinStop		}

#if HAS_PROFILING
	,
	{ kCommandProfileStart,		&EmDocument::DoProfileStart,	kStr_CmdProfileStart	},
	{ kCommandProfileStop,		&EmDocument::DoProfileStop,		kStr_CmdProfileStop		},
	{ kCommandProfileDump,		&EmDocument::DoProfileDump,		kStr_CmdProfileDump		}
#endif
};


class EmActionNewHorde : public EmAction
{
	public:
								EmActionNewHorde (const HordeInfo& info) :
									EmAction (kStr_CmdGremlinNew),
									fInfo (info)
								{
								}

		virtual					~EmActionNewHorde (void)
								{
								}

		virtual void			Do (void)
								{
									EmSessionStopper	stopper (gSession, kStopOnSysCall);
									Hordes::New (fInfo);
								}

	private:
		HordeInfo				fInfo;
};


class EmActionDialog : public EmAction
{
	public:
								EmActionDialog (EmDlgThreadFn fn,
										const void* parms,
										EmDlgItemID& result) :
									EmAction (0),
									fDlgFn (fn),
									fDlgParms (parms),
									fDlgResult (result)
								{
								}

		virtual					~EmActionDialog (void) {}

		virtual void			Do (void);

	private:
		EmDlgThreadFn			fDlgFn;
		const void*				fDlgParms;
		EmDlgItemID&			fDlgResult;
};


void EmActionDialog::Do (void)
{
	// Show any error messages from the CPU thread.

	fDlgResult = EmDlg::RunDialog (fDlgFn, fDlgParms);

#if HAS_OMNI_THREAD
	EmAssert (gSession);
	gSession->UnblockDialog ();
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoNew
// ---------------------------------------------------------------------------
// Bottleneck for all document creation.  Creates a new document, associates
// it with the given application, creates a new window, creates a new session,
// and sets last configuration and last session preferences accordingly.
// On success, the new document is returned.  On failure, an exception is
// thrown.

EmDocument* EmDocument::DoNew (const Configuration& cfg)
{
	// Create the new document.

	EmDocument*	doc = EmDocument::HostCreateDocument ();

	try
	{
		// Create the new session.

		doc->fSession = new EmSession;
		doc->fSession->CreateNew (cfg);
		doc->fSession->CreateThread (false);	// !!! Created here, but destroyed in EmSession::~EmSession!

		// Save the newly created configuration for next time.

		{
			Preference<Configuration>	pref (kPrefKeyLastConfiguration);
			pref = doc->fSession->GetConfiguration ();
		}

		// Zap the last-saved session file, so that we give preference
		// to a New operation at startup and not an Open operation.

		{
			Preference<EmFileRef>	pref (kPrefKeyLastPSF);
			pref = EmFileRef();
		}

		// Set the ROM's file type, so that the icon looks right.

		cfg.fROMFile.SetCreatorAndType (kFileCreatorEmulator, kFileTypeROM);
	}
	catch (...)
	{
		delete doc;
		throw;
	}

	// If we got here, then everything worked.  Open a window on the session.

	doc->PrvOpenWindow ();

	return doc;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoOpen
// ---------------------------------------------------------------------------
// Bottleneck for all document opening.  Creates a new document, associates
// it with the given application, creates a new window, opens an old session,
// and sets last configuration and last session preferences accordingly.
// On success, the new document is returned.  On failure, an exception is
// thrown.

EmDocument* EmDocument::DoOpen (const EmFileRef& file)
{
	// Create the new document.

	Bool		hordesIsOn = false;
	EmDocument*	doc = EmDocument::HostCreateDocument ();

	try
	{
		// Open the old session.
                PHEM_Log_Msg("Opening old session.");

		doc->fSession = new EmSession;
		doc->fFile = file;					// !!! Actually, this is redundant with EmSession's.
		doc->fSession->CreateOld (file);

		// Get this here before we start up the thread.  If we were to first
		// start up the thread, it would proceed to run the Gremlin, which
		// may temporarily turn off the "Gremlins is running" flag (see
		// Hordes::SaveRootState), which would lead us to not open the
		// Gremlin Control Window.

		hordesIsOn = Hordes::IsOn ();

                PHEM_Log_Msg("CreateThread?");
		doc->fSession->CreateThread (false);	// !!! Created here, but destroyed in EmSession::~EmSession!

		// Patch up anything for Hordes.

		Hordes::PostLoad ();

		// Save the newly created configuration for next time.

                PHEM_Log_Msg("Saving to pref:last");
		{
			Preference<Configuration>	pref (kPrefKeyLastConfiguration);
			pref = doc->fSession->GetConfiguration ();
		}

		// Save the last-opened session file for next time.

                PHEM_Log_Msg("Saving file to pref:last");
		{
			Preference<EmFileRef>	pref (kPrefKeyLastPSF);
			pref = file;
		}

		// Update the session MRU list.
                PHEM_Log_Msg("Updating MRU");

		gEmuPrefs->UpdateRAMMRU (file);
	}
	catch (...)
	{
                PHEM_Log_Msg("Got Exception!!");
		delete doc;
		throw;
	}

	// If we got here, then everything worked.  Open a window on the session.
        PHEM_Log_Msg("Opening window");

	doc->PrvOpenWindow ();

	// If we loaded a session file with a Gremlin running, make sure
	// the Gremlin control window is open.  Do this after opening the
	// LCD window so that the Gremlin window is properly a child window
	// of the LCD window.

	if (hordesIsOn)
	{
		EmDlg::GremlinControlOpen ();
	}

        PHEM_Log_Msg("returning doc");
	return doc;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoNewBound
// ---------------------------------------------------------------------------

EmDocument* EmDocument::DoNewBound (void)
{
	EmAssert (gApplication->IsBoundPartially ());

	// Create the new document.

	EmDocument*	doc = EmDocument::HostCreateDocument ();

	try
	{
		// Create the new session.

		doc->fSession = new EmSession;
		doc->fSession->CreateBound ();
		doc->fSession->CreateThread (false);	// !!! Created here, but destroyed in EmSession::~EmSession!
	}
	catch (...)
	{
		delete doc;
		throw;
	}

	// If we got here, then everything worked.  Open a window on the session.

	doc->PrvOpenWindow ();

	return doc;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoOpenBound
// ---------------------------------------------------------------------------

EmDocument* EmDocument::DoOpenBound (void)
{
	EmAssert (gApplication->IsBoundFully ());

	EmDocument*	doc = EmDocument::HostCreateDocument ();

	try
	{
		doc->fSession = new EmSession;
		doc->fSession->CreateBound ();
		doc->fSession->CreateThread (false);	// !!! Created here, but destroyed in EmSession::~EmSession!
	}
	catch (...)
	{
		delete doc;
		throw;
	}

	// If we got here, then everything worked.  Open a window on the session.

	doc->PrvOpenWindow ();

	return doc;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocument::EmDocument
// ---------------------------------------------------------------------------
// Constructor.  Set our data members to NULL, and set the global application
// variable to point to us.

EmDocument::EmDocument (void) :
	EmActionHandler (),
	fSession (NULL),
	fFile ()
{
	EmAssert (gDocument == NULL);
	gDocument = this;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::~EmDocument
// ---------------------------------------------------------------------------
// Destructor.  Close the Gremlin Control window, disassociate us from the
// application object, clear the global document variable, and delete the
// session object.
//
// Note that we don't close the window object here.  We defer that to any
// document subclass in case it's not appropriate to close the window for
// this platform (for instance, the window stays open on Unix).

EmDocument::~EmDocument (void)
{
	EmDlg::GremlinControlClose ();

	EmAssert (gDocument == this);
	gDocument = NULL;

	delete fSession;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleCommand
// ---------------------------------------------------------------------------
// Handle a user command.  Normally the command is generated when the user
// makes a menu selection, but the command could really come from anywhere
// (for example, a toolbar with icon buttons, or from the dialog with the
// New, Open, Download, and Quit pushbuttons in it).
//
// This method examines the command, synchronizes with the CPU thread as
// necessary, executes the command, and catches any exceptions, showing them
// in an error dialog.
//
// This function is an EXCEPTION_CATCH_POINT.

Bool EmDocument::HandleCommand (EmCommandID commandID)
{
	// Find information on how this command should be handled.

	size_t ii;
	for (ii = 0; ii < countof (kCommand); ++ii)
	{
		if (kCommand[ii].fCommandID == commandID)
			break;
	}

	// If we couldn't find an entry for this command, assume that it's not
	// a command for the document, and return false indicating that we did
	// not handle the command.

	if (ii >= countof (kCommand))
	{
		return false;	// We did not handle this command.
	}

	// Execute the command.  Catch any exceptions and report them to the user.

	if (kCommand[ii].fFn)
	{
		try
		{
			(this->*(kCommand[ii].fFn)) (commandID);
		}
		catch (ErrCode errCode)
		{
			StrCode	operation = kCommand[ii].fErrStrCode;

			Errors::ReportIfError (operation, errCode, 0, false);
		}
	}

	return true;	// We handled this command.
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleKey
// ---------------------------------------------------------------------------
// Handle the user pressing a key on their keyboard.

void EmDocument::HandleKey (const EmKeyEvent& event)
{
	EmAssert (fSession);

	fSession->PostKeyEvent (event);
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleButton
// ---------------------------------------------------------------------------
// Handle the user pressing on a skin button with their mouse.

void EmDocument::HandleButton (SkinElementType theButton, Bool isDown)
{
	EmAssert (fSession);

	EmButtonEvent	event (theButton, isDown);
	fSession->PostButtonEvent (event);
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleIdle
// ---------------------------------------------------------------------------
// Perform any document-level idle time operations.

void EmDocument::HandleIdle (void)
{
	// Idle our sockets.

	CSocket::IdleAll ();

	// Idle any packets that are looking for signals.
	// That is, see if any have timed out by now.

	RPC::Idle ();

	// If we need to start a new Gremlin Horde, and the OS
	// is now booted enough to handle our mucking about as
	// we start it up, then start starting it up.

	if (EmPatchState::EvtGetEventCalled ())
	{
		HordeInfo	info;
		if (Startup::NewHorde (&info))
		{
			this->ScheduleNewHorde (info);
		}
	}

	// Pop off deferred actions and handle them.

	this->DoAll ();
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleSave
// ---------------------------------------------------------------------------
// Perform a standard Save operation.  If the document hasn't been saved
// before, perform a "Save As" operation.  Return whether or not the user
// cancelled the operation.  On failure, an exception is thrown.

Bool EmDocument::HandleSave (void)
{
	if (!fFile.IsSpecified ())
	{
		return this->HandleSaveAs ();
	}

	return this->HandleSaveTo (fFile);
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleSaveAs
// ---------------------------------------------------------------------------
// Perform a standard Save As operation, asking the user for a file name.
// Return whether or not the user cancelled the operation.  On failure,
// an exception is thrown.

Bool EmDocument::HandleSaveAs (void)
{
	EmFileRef	destRef;

	// Ask the user for a file to which to save the file.

	if (this->AskSaveSession (destRef))
	{
		// If they did not cancel, save the file.

		return this->HandleSaveTo (destRef);
	}

	// Return that we did not save the file.

	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleSaveTo
// ---------------------------------------------------------------------------
// Perform a save operation to the given file.  The given file may or may not
// already exist, but it must at least be specified.  The user will NOT be
// asked for a file name.  On success, the "last session" preference and the
// session MRU preference will be updated and this function will return true.
// On failure, an exception is thrown.
//
// This is the bottleneck routine for all document saving.  Note that this
// is different from "session saving", which is a lower-level operation that
// can occur while Gremlins is running.

Bool EmDocument::HandleSaveTo (const EmFileRef& destRef)
{
	EmAssert (destRef.IsSpecified ());

	// Suspend the session so that we can save its data.

        PHEM_Log_Msg("Stopping session...");
	EmSessionStopper	stopper (fSession, kStopNow);

	// Have the session save itself.

        PHEM_Log_Msg("Actually saving sesssion...");
	const Bool kUpdateFileRef = true;
	fSession->Save (destRef, kUpdateFileRef);

	// Remember that we are now associated with this file.

        PHEM_Log_Msg("Associating...");
	fFile = destRef;

	// Save the last-saved session file for next time.

        PHEM_Log_Msg("Saving pref...");
	Preference<EmFileRef>	pref (kPrefKeyLastPSF);
	pref = destRef;

	// Update the MRU menu.

	gEmuPrefs->UpdateRAMMRU (destRef);

	// Return that we saved the file.

        PHEM_Log_Msg("HandleSaveTo Done.");
	return true;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleClose
// ---------------------------------------------------------------------------
// Handle a request to close the document.  Get the user's preference with
// regards to saving the document before it's closed, and then call the
// version of HandleClose that takes the CloseActionType parameter.
//
// NOTE: this method can "delete this", so be careful not to access it after
// this method returns TRUE.

Bool EmDocument::HandleClose (Bool quitting)
{
	Preference<CloseActionType>	pref (kPrefKeyCloseAction);
	return this->HandleClose (*pref, quitting);
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::HandleClose
// ---------------------------------------------------------------------------
// Handle a request to close the document.  First, determine whether or not
// to save the document, based either on preference or on querying the user.
// Second, try to save the document if that's required.  Third, if the
// document was saved or the user didn't cancel the save operation, close
// the document by deleting it.  Finally, return whether or not the document
// was closed.
//
// NOTE: this method can "delete this", so be careful not to access it after
// this method returns TRUE.

Bool EmDocument::HandleClose (CloseActionType closeAction, Bool quitting)
{
	EmDlgItemID	answer;

	// If we have to ask, then ask.

	if (closeAction == kSaveAsk)
	{
		answer = this->AskSaveChanges (quitting);
	}

	// If we always save, then pretend we asked and the user said yes.

	else if (closeAction == kSaveAlways)
	{
		answer = kDlgItemYes;
	}

	// If we never save, then pretend we asked and the user said no.

	else // if (closeAction == kSaveNever)
	{
		answer = kDlgItemNo;
	}

	EmAssert (answer == kDlgItemYes || answer == kDlgItemNo || answer == kDlgItemCancel);

	Bool closeIt = true;

	// If we want to save the document, then save it.  If the save
	// succeeds, then set the flag to also close the document.

	if (answer == kDlgItemYes)
	{
		closeIt = this->HandleSave ();
	}

	// We don't want to save the document, and we no longer want
	// to close it, either.

	else if (answer == kDlgItemCancel)
	{
		closeIt = false;
	}

	// We didn't want to save the document, but we want to close
	// it anyway.

	else	// answer == kDlgItemNo
	{
		closeIt = true;
	}

	// Close the document by deleting it.

	if (closeIt)
	{
		delete this;
	}

	// Return whether or not we closed the document.  It may or may not
	// have been saved -- we're not saying which way with this response.

	return closeIt;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocument::ScheduleNewHorde
// ---------------------------------------------------------------------------

void EmDocument::ScheduleNewHorde (const HordeInfo& info)
{
	this->PostAction (new EmActionNewHorde (info));
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::ScheduleDialog
// ---------------------------------------------------------------------------

void EmDocument::ScheduleDialog (EmDlgThreadFn fn,
								 const void* parms,
								 EmDlgItemID& result)
{
	this->PostAction (new EmActionDialog (fn, parms, result));
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocument::DoSave
// ---------------------------------------------------------------------------
// Handle the Save menu item.

void EmDocument::DoSave (EmCommandID)
{
	this->HandleSave ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoSaveAs
// ---------------------------------------------------------------------------
// Handle the Save As menu item.

void EmDocument::DoSaveAs (EmCommandID)
{
	this->HandleSaveAs ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoSaveBound
// ---------------------------------------------------------------------------
// Handle the Save Bound menu item.

void EmDocument::DoSaveBound (EmCommandID)
{
	if (this->HostCanSaveBound ())
	{
		EmDlg::DoSaveBound ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoSaveScreen
// ---------------------------------------------------------------------------
// Handle the Save Screen menu item.

void EmDocument::DoSaveScreen (EmCommandID)
{
	EmFileRef		destRef;
	if (this->AskSaveScreen (destRef))
	{
		this->HostSaveScreen (destRef);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoInfo
// ---------------------------------------------------------------------------
// Handle the Session Info menu item.

void EmDocument::DoInfo (EmCommandID)
{
	EmDlg::DoSessionInfo ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoImport
// ---------------------------------------------------------------------------
// Handle the Install Application/Database menu items.  We handle all menu
// items in the sub-menu here.

void EmDocument::DoImport (EmCommandID commandID)
{
	EmFileRefList		fileList;

	// Ask the user for files to install.

	if (commandID == kCommandImportOther)
	{
		// If, when asking the user for a file to install, they press
		// cancel, then just return right now.

		if (!this->AskImportFiles (fileList))
		{
			return;
		}
	}

	// Get a file to install from the MRU list.

	else
	{
		int			index		= commandID - kCommandImport0;
		EmFileRef	importRef	= gEmuPrefs->GetIndPRCMRU (index);

		// If, for some reason, the returned file reference is not
		// specified, then just silently return right now.  If the
		// file doesn't exist, we'll let DoDatabaseImport report it
		// or throw an exception.

		if (!importRef.IsSpecified ())
		{
			return;
		}

		// Put file in list.

		fileList.push_back (importRef);
	}

	// Call the EmDlg routine that installs the file in tune with a
	// progress dialog.

	EmDlg::DoDatabaseImport (fileList, kMethodBest);
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoExport
// ---------------------------------------------------------------------------
// Handle the Export Database menu item.

void EmDocument::DoExport (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopOnSysCall);
	EmDlg::DoDatabaseExport ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoHotSync
// ---------------------------------------------------------------------------
// Handle the HotSync menu item by simulating a press on the cradle button.

void EmDocument::DoHotSync (EmCommandID)
{
	this->HandleButton (kElement_CradleButton, true);
	this->HandleButton (kElement_CradleButton, false);
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoReset
// ---------------------------------------------------------------------------
// Handle the Reset menu item.

void EmDocument::DoReset (EmCommandID)
{
	EmResetType	type;

	if (EmDlg::DoReset (type) == kDlgItemOK)
	{
		EmSessionStopper	stopper (fSession, kStopNow);
		fSession->Reset (type);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoGremlinNew
// ---------------------------------------------------------------------------
// Handle the New Gremlin menu item.

void EmDocument::DoGremlinNew (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopOnSysCall);
	EmDlg::DoHordeNew ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoGremlinSuspend
// ---------------------------------------------------------------------------
// Handle the Suspend Gremlin menu item.

void EmDocument::DoGremlinSuspend (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopOnSysCall);
	Hordes::Suspend ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoGremlinStep
// ---------------------------------------------------------------------------
// Handle the Step Gremlin menu item.

void EmDocument::DoGremlinStep (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopOnSysCall);
	Hordes::Step ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoGremlinResume
// ---------------------------------------------------------------------------
// Handle the Resume Gremlin menu item.

void EmDocument::DoGremlinResume (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopOnSysCall);
	Hordes::Resume ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoGremlinStop
// ---------------------------------------------------------------------------
// Handle the Stop Gremlin menu item.

void EmDocument::DoGremlinStop (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopNow);
	Hordes::Stop ();
}


#if HAS_PROFILING
// ---------------------------------------------------------------------------
//		¥ EmDocument::DoProfileStart
// ---------------------------------------------------------------------------
// Handle the Start Profile menu item.

void EmDocument::DoProfileStart (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopNow);
	::ProfileStart ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoProfileStop
// ---------------------------------------------------------------------------
// Handle the Stop Profile menu item.

void EmDocument::DoProfileStop (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopNow);
	::ProfileStop ();
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::DoProfileDump
// ---------------------------------------------------------------------------
// Handle the Dump Profile menu item.

void EmDocument::DoProfileDump (EmCommandID)
{
	EmSessionStopper	stopper (fSession, kStopNow);
	::ProfileDump (NULL);
}
#endif


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocument::AskNewSession
// ---------------------------------------------------------------------------
// Ask the user for new session configuration information.  If the user
// cancels, return false.  Otherwise, return true and store the new
// configuration information in cfg.  The cfg parameter is also used to
// provide the initial settings in the dialog box.

Bool EmDocument::AskNewSession (Configuration& cfg)
{
	EmAssert (!gApplication->IsBound ());

	// Ask the user for new configuration information.

	EmDlgItemID	item = EmDlg::DoSessionNew (cfg);

	// Return whether or not the user cancelled the operation.

	return item == kDlgItemOK;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::AskSaveSession
// ---------------------------------------------------------------------------
// Ask the user for a file to which to save the document.  If the user
// cancels, return false.  Otherwise, return true and store the specified
// file name in "ref".  The ref parameter also provides a default file name
// if it is specified.

Bool EmDocument::AskSaveSession (EmFileRef& ref)
{
	// Start by showing the user the directory in which they saved the
	// last session file.

	Preference<EmFileRef>	pref (kPrefKeyLastPSF);
	EmFileRef				lastSession = *pref;

	// Get other parameters used in presenting the dialog.

	string					prompt (Platform::GetString (kStr_SaveSessionAs));
	EmDirRef				defaultPath (lastSession.GetParent ());
	EmFileTypeList			filterList;
	string					defaultName (ref.GetName ());

	filterList.push_back (kFileTypeSession);
	filterList.push_back (kFileTypeAll);

	// Ask the user for a file name.

	EmDlgItemID	item = EmDlg::DoPutFile (
		ref, prompt, defaultPath, filterList, defaultName);

	// Return whether or not the user cancelled the operation.

	return item == kDlgItemOK;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::AskLoadSession
// ---------------------------------------------------------------------------
// Ask the user for a session document to load.  If the user cancels, return
// false.  Otherwise, return true and store the specified file name in "ref".
// The ref parameter also provides a default file name if it is specified.

Bool EmDocument::AskLoadSession (EmFileRef& ref, EmFileType type)
{
	EmAssert (!gApplication->IsBoundFully ());

	// Start by showing the user the directory in which they saved the
	// last session file.

	Preference<EmFileRef>	pref (kPrefKeyLastPSF);
	EmFileRef				lastSession = *pref;

	// Get other parameters used in presenting the dialog.

	string					prompt (Platform::GetString (kStr_LoadSession));
	EmDirRef				defaultPath (lastSession.GetParent ());
	EmFileTypeList			filterList;

	filterList.push_back (type);
	filterList.push_back (kFileTypeAll);

	// Ask the user for a file name.

	EmDlgItemID	item = EmDlg::DoGetFile (
		ref, prompt, defaultPath, filterList);

	// Return whether or not the user cancelled the operation.

	return item == kDlgItemOK;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::AskSaveChanges
// ---------------------------------------------------------------------------
// Ask the user if they want to save their changes to the session document.
// If the document had been saved before, ask the user if they want to save
// changes to the document with that name.  Otherwise, ask them if they want
// to save the "Untitled" document.  Also, ask them if they'd like to save
// the changes before "closing" or before "quitting".  Return the user's
// choice from the dialog: Yes, No, or Cancel.

EmDlgItemID EmDocument::AskSaveChanges (Bool quitting)
{
	string	name;

	if (fFile.IsSpecified ())
	{
		name = fFile.GetName ();
	}
	else
	{
		// DoSessionSave provides the kStr_Untitled string
		// if the given string is empty.
	}

	EmDlgItemID	item = EmDlg::DoSessionSave (name, quitting);

	EmAssert (item == kDlgItemYes || item == kDlgItemNo || item == kDlgItemCancel);

	return item;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::AskSaveScreen
// ---------------------------------------------------------------------------
// Ask the user for a file to which they want to save the screen contents.  If
// the user cancels, return false.  Otherwise, return true and store the
// specified file name in "ref".  The ref parameter also provides a default
// file name if it is specified.

Bool EmDocument::AskSaveScreen (EmFileRef& ref)
{
	// !!! Need to preserve the last directory to which a picture was
	// saved so that we can restore it here.

	// Get other parameters used in presenting the dialog.

	string			prompt (Platform::GetString (kStr_SaveScreenAs));
	EmDirRef		defaultPath;
	EmFileTypeList	filterList;
	string			defaultName;

	filterList.push_back (kFileTypePicture);
	filterList.push_back (kFileTypeAll);

	// Ask the user for a file name.

	EmDlgItemID		item = EmDlg::DoPutFile (
		ref, prompt, defaultPath, filterList, defaultName);

	// Return whether or not the user cancelled the operation.

	return item == kDlgItemOK;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::AskImportFiles
// ---------------------------------------------------------------------------
// Ask the user for files they want to import.  If the user cancels, return
// false.  Otherwise, return true and store the specified file names in
// "listRef".

Bool EmDocument::AskImportFiles (EmFileRefList& listRef)
{
	// Get other parameters used in presenting the dialog.

	string			prompt (Platform::GetString (kStr_ImportFile));
	EmDirRef		defaultPath;
	EmFileTypeList	filterList;

	filterList.push_back (kFileTypePalmApp);
	filterList.push_back (kFileTypePalmDB);
	filterList.push_back (kFileTypePalmQA);

	// Start by showing the user the directory from which they
	// last loaded a PRC/PDB/PQA file.

	EmFileRef		lastImport = gEmuPrefs->GetIndPRCMRU (0);
	if (lastImport.IsSpecified ())
	{
		defaultPath = lastImport.GetParent ();
	}

	// Ask the user for file names.

	EmDlgItemID		item = EmDlg::DoGetFileList (
		listRef, prompt, defaultPath, filterList);

	// Return whether or not the user cancelled the operation.

	return item == kDlgItemOK;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocument::HostCanSaveBound
// ---------------------------------------------------------------------------

Bool EmDocument::HostCanSaveBound (void)
{
	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmDocument::HostSaveScreen
// ---------------------------------------------------------------------------

void EmDocument::HostSaveScreen (const EmFileRef&)
{
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDocument::PrvOpenWindow
// ---------------------------------------------------------------------------
// Create the window for a new document.

void EmDocument::PrvOpenWindow (void)
{
	if (gWindow == NULL)
	{
		EmWindow::NewWindow ();
	}
	else
	{
		// If the window is already open, then at least reset it so that
		// it can get its new skin.

		gWindow->WindowReset ();
	}
}
