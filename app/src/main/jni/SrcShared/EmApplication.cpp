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
#include "EmApplication.h"

#include "EmCommands.h"			// EmCommandID
#include "EmDlg.h"				// EmDlg, DoEditPreferences, etc.
#include "EmDocument.h"			// EmDocument::AskNewSession, etc.
#include "EmErrCodes.h"			// kError_OnlySameType
#include "EmEventPlayback.h"	// EmEventPlayback::ReplayEvents
#include "EmMinimize.h"			// EmMinimize::Start
#include "EmPatchState.h"		// EmPatchState::IsTimeToQuit
#include "EmROMTransfer.h"		// EmROMTransfer::ROMTransfer
#include "EmSession.h"			// EmStopMethod
#include "EmTransport.h"		// EmTransport::CloseAllTransports
#include "EmTypes.h"			// StrCode
#include "EmWindow.h"			// gWindow
#include "ErrorHandling.h"		// Errors::ReportIfError
#include "HostControl.h"		// hostSignalQuit
#include "Startup.h"			// CreateSession, OpenSession, DetermineStartupActions
#include "Strings.r.h"			// kStr_CmdAbout, etc.

#include "DebugMgr.h"			// Debug::Startup
#include "EmDlg.h"				// DoCommonDialog
#include "EmRPC.h"				// RPC::Startup
#include "Logging.h"			// LogStartup
#include "SocketMessaging.h"	// CSocket::Startup

#include "EmLowMem.h" // EmLowMem_GetGlobal() - for checking tick speed
#include "PHEMNativeIF.h"

#if HAS_TRACER
#include "TracerPlatform.h"		// gTracer.Initialize();
#endif

typedef void (EmApplication::*EmCommandFn)(EmCommandID);


static const struct
{
	EmCommandID		fCommandID;
	EmCommandFn		fFn;
	StrCode			fErrStrCode;
}
kCommand[] =
{
	{ kCommandAbout,			&EmApplication::DoAbout,		kStr_CmdAbout			},
	{ kCommandSessionNew,		&EmApplication::DoNew,			kStr_CmdNew				},
	{ kCommandSessionOpenOther,	&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen0,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen1,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen2,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen3,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen4,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen5,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen6,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen7,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen8,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionOpen9,		&EmApplication::DoOpen,			kStr_CmdOpen			},
	{ kCommandSessionClose,		&EmApplication::DoClose,		kStr_CmdClose			},
	{ kCommandQuit,				&EmApplication::DoQuit,			kStr_CmdQuit			},

	{ kCommandDownloadROM,		&EmApplication::DoDownload,		kStr_CmdTransferROM		},

	{ kCommandPreferences,		&EmApplication::DoPreferences,	kStr_CmdPreferences		},
	{ kCommandLogging,			&EmApplication::DoLogging,		kStr_CmdLoggingOptions	},
	{ kCommandDebugging,		&EmApplication::DoDebugging,	kStr_CmdDebugOptions	},
	{ kCommandErrorHandling,	&EmApplication::DoErrorHandling,kStr_CmdErrorHandling	},
#if HAS_TRACER
	{ kCommandTracing,			&EmApplication::DoTracing,		kStr_CmdTracingOptions	},
#endif
	{ kCommandSkins,			&EmApplication::DoSkins,		kStr_CmdSkins			},
	{ kCommandHostFS,			&EmApplication::DoHostFS,		kStr_CmdHostFSOptions	},
	{ kCommandBreakpoints,		&EmApplication::DoBreakpoints,	kStr_CmdBreakpoints		},

	{ kCommandEventReplay,		&EmApplication::DoReplay,		kStr_CmdEventReplay		},
	{ kCommandEventMinimize,	&EmApplication::DoMinimize,		kStr_CmdEventMinimize	},

	{ kCommandEmpty,			&EmApplication::DoNothing,		0	},
	{ kCommandFile,				&EmApplication::DoNothing,		0	},
	{ kCommandEdit,				&EmApplication::DoNothing,		0	},
	{ kCommandGremlins,			&EmApplication::DoNothing,		0	},
#if HAS_PROFILING
	{ kCommandProfile,			&EmApplication::DoNothing,		0	},
#endif
	{ kCommandRoot,				&EmApplication::DoNothing,		0	},
	{ kCommandOpen,				&EmApplication::DoNothing,		0	},
	{ kCommandImport,			&EmApplication::DoNothing,		0	},
	{ kCommandSettings,			&EmApplication::DoNothing,		0	},
	{ kCommandDivider,			&EmApplication::DoNothing,		0	}
};

EmApplication*	gApplication;

class EmActionSessionClose : public EmAction
{
	public:
								EmActionSessionClose (const EmFileRef& ref) :
									EmAction (kStr_CmdClose),
									fRef (ref)
								{
								}

		virtual					~EmActionSessionClose (void)
								{
								}

		virtual void			Do (void)
								{
									gApplication->HandleSessionClose (fRef);
								}

	private:
		EmFileRef				fRef;
};


class EmActionQuit : public EmAction
{
	public:
								EmActionQuit (void) :
									EmAction (kStr_CmdQuit)
								{
								}

		virtual					~EmActionQuit (void)
								{
								}

		virtual void			Do (void)
								{
									gApplication->HandleQuit ();
								}

	private:
};



// ---------------------------------------------------------------------------
//		¥ EmApplication::EmApplication
// ---------------------------------------------------------------------------
// Constructor.  Sets the document reference to NULL, sets the quit flag to
// false, and sets the global gApplication variable to point to us.

EmApplication::EmApplication (void) :
	EmActionHandler (),
	fQuit (false)
{
	EmAssert (gApplication == NULL);
	gApplication = this;
#if EMULATION_LEVEL == EMULATION_UNIX
        last_ticks = 0;
        ticks_per_second = 1000;
        max_tps = 0;
        min_tps = 1000000;
        avg_tps = sysTicksPerSecond;
        tps_count = 0;
        tps_total = 0;
        memset(&last_time, 0, sizeof(struct timespec));
#endif
        ticks_per_second = sysTicksPerSecond;
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::~EmApplication
// ---------------------------------------------------------------------------
// Destructor.  Makes sure the document has been closed, removes the reference
// to us in gApplication, and performs some necessary final cleanup.

EmApplication::~EmApplication (void)
{
	EmAssert (fQuit);

	EmAssert (gApplication == this);
	gApplication = NULL;

	EmTransport::CloseAllTransports ();
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::Startup
// ---------------------------------------------------------------------------
// Performs one-time startup initialization.

Bool EmApplication::Startup (int argc, char** argv)
{
	// Load our preferences.

        PHEM_Log_Msg("in App:Startup");
	gPrefs->Load ();

        PHEM_Log_Msg("Prefs loaded.");
	CSocket::Startup ();
	Debug::Startup ();	// Create our sockets
	RPC::Startup ();	// Create our sockets
        PHEM_Log_Msg("Sockets started.");

#if HAS_TRACER
	gTracer.Initialize ();
#endif

	LogStartup ();
        PHEM_Log_Msg("Logs started.");

	// Check to see if any skins were loaded. Report a possible problem if
	// not. Only warn the user once. Don't warn for bound Posers, which have
	// the only skin they need to use included as a resource.

	Preference<Bool>	pref (kPrefKeyWarnAboutSkinsDir);
	if (!this->IsBound () && *pref)
	{
		SkinNameList	names;
		SkinGetSkinNames (EmDevice (), names);

		if (names.size () <= 1)
		{
			EmDlg::DoCommonDialog (kStr_MissingSkins, kDlgFlags_OK);

			pref = false;
		}
	}

	// Determine what startup options the user has specified on the command
	// line.

        PHEM_Log_Msg("Calling DetermineStartupActions.");
	Bool result = Startup::DetermineStartupActions (argc, argv);

	if (!result)
	{
                PHEM_Log_Msg("SetTimeToQuit true!");
		this->SetTimeToQuit (true);
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::Shutdown
// ---------------------------------------------------------------------------
// Performs one-time shutdown operations.

void EmApplication::Shutdown (void)
{
	RPC::SignalWaiters (hostSignalQuit);

	Debug::Shutdown ();
	RPC::Shutdown ();
	CSocket::Shutdown ();

	LogShutdown ();

#if HAS_TRACER
	gTracer.Dispose ();
#endif

	// Save the preferences.

	gPrefs->Save ();
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleStartupActions
// ---------------------------------------------------------------------------
// Kick off any actions required when Poser starts up.  The rules for what
// needs to be done are defined by the Startup class, based on command line
// options, preferences, and the state of the CapsLock key.  Here, all we do
// is respond to the results of all those options and either open an old
// session, create a new one, or do nothing.
//
// !!! This function should probably be replaced with EmActions posted by
// Startup::DetermineStartupActions.
//
// This function is an EXCEPTION_CATCH_POINT.

void EmApplication::HandleStartupActions (void)
{
	EmFileRef		ref;
	Configuration	cfg;

	try
	{
                PHEM_Log_Place(100);
		if (this->IsBoundFully ())
		{
                        PHEM_Log_Msg("App:OpenBound");
			this->HandleOpenBound ();
		}
		else if (this->IsBoundPartially ())
		{
                        PHEM_Log_Msg("App:NewBound");
			this->HandleNewBound ();
		}
		else if (Startup::OpenSession (ref))
		{
                        PHEM_Log_Msg("App:OpenFromFile");
			this->HandleOpenFromFile (ref);
		}
		else if (Startup::CreateSession (cfg))
		{
                        PHEM_Log_Msg("App:NewFromConfig");
			this->HandleNewFromConfig (cfg);
		}
		else if (Startup::Minimize (ref))
		{
                        PHEM_Log_Msg("App:Minimize");
			this->HandleMinimize (ref);
		}
                PHEM_Log_Place(106);
	}
	catch (ErrCode errCode)
	{
               PHEM_Log_Msg("App:StartupError");
		Errors::ReportIfError (kStr_GenericOperation, errCode, 0, false);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleCommand
// ---------------------------------------------------------------------------
// Handle a user command.  Normally the command is generated when the user
// makes a menu selection, but the command could really come from anywhere
// (for example, a toolbar with icon buttons, or from the dialog with the New,
// Open, Download, and Quit pushbuttons in it).
//
// This method examines the command, synchronizes with the CPU thread as
// necessary, executes the command, and catches any exceptions, showing them
// in an error dialog.
//
// This function is an EXCEPTION_CATCH_POINT.

Bool EmApplication::HandleCommand (EmCommandID commandID)
{
	// Find information on how this command should be handled.

	size_t ii;
	for (ii = 0; ii < countof (kCommand); ++ii)
	{
		if (kCommand[ii].fCommandID == commandID)
			break;
	}

	// If we couldn't find an entry for this command, assume that it's not a
	// command for the application, and return false indicating that we did
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

#if EMULATION_LEVEL == EMULATION_UNIX
timespec spec_diff(timespec start, timespec end)
{
        timespec temp;
        if ((end.tv_nsec-start.tv_nsec)<0) {
                temp.tv_sec = end.tv_sec-start.tv_sec-1;
                temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
        } else {
                temp.tv_sec = end.tv_sec-start.tv_sec;
                temp.tv_nsec = end.tv_nsec-start.tv_nsec;
        }
        return temp;
}

void  EmApplication::Prv_Measure_Ticks_Per_Second() {
  unsigned long cur_ticks = EmLowMem_GetGlobal(hwrCurTicks);
  struct timespec cur_time;
  clock_gettime(CLOCK_MONOTONIC, &cur_time);

  // Wait until we have two samples to start calculating.
  if (last_time.tv_sec) {
    timespec diffy = spec_diff(last_time, cur_time);

    // Do the math.
    if (diffy.tv_sec == 0) {
      unsigned long tick_diff = cur_ticks - last_ticks;

      double temp_tps = (tick_diff * 1000000000.0) / diffy.tv_nsec;
      ticks_per_second = (unsigned long) (temp_tps + 0.5); // round up

      if (ticks_per_second > max_tps) {
         max_tps = ticks_per_second;
      }
      if (ticks_per_second < min_tps) {
         min_tps = ticks_per_second;
      }
      tps_count++;
      tps_total += ticks_per_second;
      avg_tps = tps_total/tps_count;
      //AndroidTODO: handle rollover.
      //LOGI("tps: %d min: %d max: %d avg: %d", ticks_per_second, min_tps, max_tps, tps_total/tps_count);
    } else {
      // This ain't right.
      //LOGI("Time diff is more than a second! %d %d", diffy.tv_sec, diffy.tv_nsec);
    }
  }

  last_time = cur_time;
  last_ticks = cur_ticks;
}
#else
void  EmApplication::Prv_Measure_Ticks_Per_Second() {
  // Other platforms have different timers. Too bad for them.
}
#endif


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleIdle
// ---------------------------------------------------------------------------
// Perform any application-level idle time operations.

void EmApplication::HandleIdle (void)
{
	// Don't let us recurse.  This could happen if, for example, we were
	// handling a debugger packet, an exception occurs, and Poser tries to
	// show an error message in a dialog.  That dialog would allow re-entry
	// into this function.

        //PHEM_Log_Msg("App:HandleIdle...");
	static Bool	inHandleIdle;

	if (inHandleIdle)
		return;

	EmValueChanger<Bool>	oldInHandleIdle (inHandleIdle, true);

	// Pop off deferred actions and handle them.

        //Prv_Measure_Ticks_Per_Second();
        //PHEM_Measure_Ticks(EmLowMem_GetGlobal(hwrCurTicks));

        //PHEM_Log_Msg("Calling DoAll.");
	this->DoAll ();

	// Idle the document.

	if (gDocument)
	{
                //PHEM_Log_Msg("Idle Document.");
		gDocument->HandleIdle ();
	}

	// Idle the window.

	if (gWindow)
	{
                //PHEM_Log_Msg("Idle Window.");
		gWindow->HandleIdle ();
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleNewFromUser
// ---------------------------------------------------------------------------
// Create a new session asking the user for configuration information.
// Starting configuration information can be passed in.  If it is not,
// configuration information from the prefs is used.

EmDocument* EmApplication::HandleNewFromUser (const Configuration* inCfg)
{
	EmAssert (!this->IsBound ());

	// Get the current configuration.

	Configuration	cfg;

	if (inCfg)
	{
		cfg = *inCfg;
	}
	else
	{
		Preference<Configuration>	pref (kPrefKeyLastConfiguration);
		cfg = *pref;
	}

	// Ask the user for missing information.

	if (!EmDocument::AskNewSession (cfg))
	{
		return NULL;
	}

	// Create the new document.

	return this->HandleNewFromConfig (cfg);
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleNew
// ---------------------------------------------------------------------------
// Create a new session from the current configuration information in the
// preferences.

EmDocument* EmApplication::HandleNewFromPrefs (void)
{
	EmAssert (!this->IsBound ());

	// Get the current configuration.

	Preference<Configuration>	pref (kPrefKeyLastConfiguration);
	Configuration				cfg = *pref;

	// Create the new document.

	return this->HandleNewFromConfig (cfg);
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleNewFromROM
// ---------------------------------------------------------------------------
// Create a new session from the given ROM.

EmDocument* EmApplication::HandleNewFromROM (const EmFileRef& romRef)
{
	EmAssert (!this->IsBound ());

	// Get the current (last specified) configuration.

	Preference<Configuration>	pref (kPrefKeyLastConfiguration);
	Configuration				cfg = *pref;

	// Update the ROM file setting with the one passed in to this function.

	cfg.fROMFile = romRef;

	// Create the new document.

	return this->HandleNewFromUser (&cfg);
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleNewFromConfig
// ---------------------------------------------------------------------------
// Create a new session from the given configuration, closing any previous
// session first.  An exception is thrown if the configuration is invalid.
//
// This function is the bottleneck for creating new sessions.

EmDocument* EmApplication::HandleNewFromConfig (const Configuration& cfg)
{
        PHEM_Log_Msg("HandleNewFromConfig...");
	EmAssert (!this->IsBound ());

        PHEM_Log_Msg("HandleNewFromConfig...");
	// Validate the configuration.

	if (!cfg.IsValid ())
	{
                PHEM_Log_Msg("Invalid config...");
                if (!cfg.fDevice.Supported()) {
                  PHEM_Log_Msg("Device not supported!");
                }

                if (cfg.fRAMSize <= 0) {
                  PHEM_Log_Msg("RAM <= 0!");
                }

                if (!cfg.fROMFile.IsSpecified()) {
                  PHEM_Log_Msg("ROM file not specified!");
                }

                if (!cfg.fROMFile.Exists()) {
                  PHEM_Log_Msg("ROM file doesn't exist!");
                }

                if (!cfg.fDevice.SupportsROM(cfg.fROMFile)) {
                  PHEM_Log_Msg("ROM file not supported!");
                }

		Errors::Throw (kError_InvalidConfiguration);
	}

	// Close any previous document.  If the user cancels, return NULL.

	if (!this->CloseDocument (false))
	{
                PHEM_Log_Msg("Failed to close old document!");
		return NULL;
	}

	// Create the new document.

        PHEM_Log_Msg("Creating new document.");
	EmAssert (gDocument == NULL);

	EmDocument::DoNew (cfg);

	return gDocument;
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleNewBound
// ---------------------------------------------------------------------------

EmDocument* EmApplication::HandleNewBound (void)
{
	EmAssert (this->IsBoundPartially ());

	if (!this->CloseDocument (false))
	{
		return NULL;
	}

	EmAssert (gDocument == NULL);

	EmDocument::DoNewBound ();

	return gDocument;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleOpenFromUser
// ---------------------------------------------------------------------------
// Open a previous session, asking the user to find it.

EmDocument* EmApplication::HandleOpenFromUser (EmFileType type)
{
	EmAssert (!this->IsBound ());

	EmFileRef	file;

	if (EmDocument::AskLoadSession (file, type))
	{
		return this->HandleOpenFromFile (file);
	}

	return NULL;
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleOpenFromPrefs
// ---------------------------------------------------------------------------
// Open a previous session, using information from the preferences.

EmDocument* EmApplication::HandleOpenFromPrefs (void)
{
	EmAssert (!this->IsBound ());

	Preference<EmFileRef>	pref (kPrefKeyLastPSF);
	EmFileRef	file = *pref;

	return this->HandleOpenFromFile (file);
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleOpenFromFile
// ---------------------------------------------------------------------------
// Open the given saved session, closing any previous session first.
//
// This function is the bottleneck for opening sessions.

EmDocument* EmApplication::HandleOpenFromFile (const EmFileRef& file)
{
	EmAssert (!this->IsBound ());

	// Close any previous document.  If the use cancels, return NULL.
        PHEM_Log_Msg("Closing previous doc...");
	if (!this->CloseDocument (false))
	{
		return NULL;
	}

	// Open the old document.

	EmAssert (gDocument == NULL);

        PHEM_Log_Msg("Opening new doc");
	EmDocument::DoOpen (file);

	return gDocument;
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleOpenBound
// ---------------------------------------------------------------------------

EmDocument* EmApplication::HandleOpenBound (void)
{
	EmAssert (this->IsBoundFully ());

	if (!this->CloseDocument (false))
	{
		return NULL;
	}

	EmAssert (gDocument == NULL);

	EmDocument::DoOpenBound ();

	return gDocument;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleFileList
// ---------------------------------------------------------------------------
// Grovel over the given list of files and determine what to do with them. The
// list can be any set of files that the emulator recognizes: session files,
// ROM files, PRC/PDB/PQA files, etc.  This function must figure out what
// files they are, validate them, and handle them (open an old session, create
// a new session, install files, etc.)
//
// This function is an EXCEPTION_CATCH_POINT.

void EmApplication::HandleFileList (const EmFileRefList& fileList)
{
	int	operation = kStr_GenericOperation;

	try
	{
		// Grovel over the file list, counting up how many of each kind of
		// file was passed to us.

		int 	prcCount = 0;
		int 	psfCount = 0;
		int 	romCount = 0;
		int 	otherCount = 0;

		EmFileRefList::const_iterator	iter = fileList.begin();
		while (iter != fileList.end())
		{
			// egcs 1.1.1 can't seem to handle iter->Method() here...
			if ((*iter).IsType (kFileTypePalmApp))		prcCount++;
			else if ((*iter).IsType (kFileTypePalmDB))	prcCount++;
			else if ((*iter).IsType (kFileTypePalmQA))	prcCount++;
			else if ((*iter).IsType (kFileTypeSession))	psfCount++;
			else if ((*iter).IsType (kFileTypeROM))		romCount++;
			else otherCount++;

			++iter;
		}

		// If we're able to use the Exchange Manager, let us beam in any file
		// type.  Later, we may want to do something like PrvFindTarget to see
		// if there's actually a handler installed for any given file type
		// before trying to beam it.  For now, let's just let the OS return an
		// error in that case.
		//
		// Note that the check of "otherCount" is a cheap way to ensure that a
		// session is currently running.  If no session were running, then the
		// call to CanUseExgMgr would crash, as it tries to call into the ROM.

		if (otherCount && EmFileImport::CanUseExgMgr ())
		{
			prcCount += otherCount;
			otherCount = 0;
		}

		// If we have a heterogeneous list, throw an exception.

		if ((prcCount > 0) + (psfCount > 0) + (romCount > 0) + (otherCount > 0) > 1)
		{
			Errors::Throw (kError_OnlySameType);
		}

		// If we have PRC/PDB/PQA files, install them.

		else if (prcCount > 0)
		{
			operation = prcCount == 1 ? kStr_CmdInstall : kStr_CmdInstallMany;
			EmDlg::DoDatabaseImport (fileList, kMethodBest);
		}

		// If we have a session file, open it.  If there's more than one, open
		// just the first one.

		else if (psfCount > 0)
		{
			operation = kStr_CmdOpen;

			if (!this->IsBoundFully ())
			{
				if (psfCount > 1)
				{
					Errors::Throw (kError_OnlyOnePSF);
				}
				else
				{
					this->HandleOpenFromFile (fileList[0]);
				}
			}
		}

		// If we have a ROM file, create a new session based on it.  If
		// there's more than one, utilize just the first one.

		else if (romCount > 0)
		{
			operation = kStr_CmdNew;

			if (!this->IsBound ())
			{
				if (romCount > 1)
				{
					Errors::Throw (kError_OnlyOneROM);
				}
				else
				{
					this->HandleNewFromROM (fileList[0]);
				}
			}
		}

		// If there's any other file type, throw an exception.

		else
		{
			Errors::Throw (kError_UnknownType);
		}
	}

	// Catch any exceptions and report them.

	catch (ErrCode errCode)
	{
		Errors::ReportIfError (kStr_GenericOperation, errCode, 0, false);
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleMinimize
// ---------------------------------------------------------------------------
// Open a session/event file and start the minimization process on it.  Called
// in response to Startup::ScheduleMinimize.

void EmApplication::HandleMinimize (const EmFileRef& ref)
{
	if (this->HandleOpenFromFile (ref))
	{
		EmMinimize::Start ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleSessionClose
// ---------------------------------------------------------------------------
// Close a session, writing it out to the given file.  If the file is not
// specified, just close the file without saving it.  Called in response to
// ScheduleSessionClose.

void EmApplication::HandleSessionClose (const EmFileRef& ref)
{
	if (gDocument)
	{
		if (ref.IsSpecified ())
		{
                        PHEM_Log_Msg("Calling HandleSaveTo().");
			gDocument->HandleSaveTo (ref);
		}

                PHEM_Log_Msg("Calling HandleClose().");
		gDocument->HandleClose (kSaveNever, false);
	}

        PHEM_Log_Msg("Returning from HandleSessionClose.");
	EmAssert (gDocument == NULL);
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::HandleQuit
// ---------------------------------------------------------------------------
// Quit the emulator, closing any session without saving it.  Called in
// response to ScheduleQuit.

void EmApplication::HandleQuit (void)
{
	if (gDocument)
	{
		gDocument->HandleClose (kSaveNever, true);
	}

	EmAssert (gDocument == NULL);

	this->SetTimeToQuit (true);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::ScheduleSessionClose
// ---------------------------------------------------------------------------
// Schedule the emulator to close the session to the given file at some safe
// point in the future.

void EmApplication::ScheduleSessionClose (const EmFileRef& ref)
{
	this->PostAction (new EmActionSessionClose (ref));
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::ScheduleQuit
// ---------------------------------------------------------------------------
// Schedule the emulator to quit at some safe point in the future.

void EmApplication::ScheduleQuit (void)
{
	this->PostAction (new EmActionQuit);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::DoAbout
// ---------------------------------------------------------------------------
// Show the All Important About Box.

void EmApplication::DoAbout (EmCommandID)
{
	EmDlg::DoAboutBox ();
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoNew
// ---------------------------------------------------------------------------
// Create a new document, asking the user for configuration information.

void EmApplication::DoNew (EmCommandID)
{
	EmAssert (!this->IsBoundFully ());

	if (this->IsBoundPartially ())
	{
		this->HandleNewBound ();
	}
	else 
	{
		this->HandleNewFromUser (NULL);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoOpen
// ---------------------------------------------------------------------------
// Open a previously saved document.  Either ask the user for a document, or
// fetch a document from the MRU list.

void EmApplication::DoOpen (EmCommandID commandID)
{
	EmAssert (!this->IsBound ());

	if (commandID == kCommandSessionOpenOther)
	{
		this->HandleOpenFromUser (kFileTypeSession);
	}
	else
	{
		int			index = commandID - kCommandSessionOpen0;
		EmFileRef	file = gEmuPrefs->GetIndRAMMRU (index);

		if (file.IsSpecified ())
		{
			this->HandleOpenFromFile (file);
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoClose
// ---------------------------------------------------------------------------
// Attempt to close the current document.

void EmApplication::DoClose (EmCommandID)
{
	this->CloseDocument (false);
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoQuit
// ---------------------------------------------------------------------------
// Attempt to close the current document and quit the application.

void EmApplication::DoQuit (EmCommandID)
{
	if (this->CloseDocument (true))
	{
		this->SetTimeToQuit (true);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoDownload
// ---------------------------------------------------------------------------
// Display the ROM Transfer dialog, and download the actual ROM if the user
// doesn't cancel the operation.

void EmApplication::DoDownload (EmCommandID)
{
	EmROMTransfer::ROMTransfer ();
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoPreferences
// ---------------------------------------------------------------------------
// Display the General Preferences dialog.

void EmApplication::DoPreferences (EmCommandID)
{
	EmDlg::DoEditPreferences ();
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoLogging
// ---------------------------------------------------------------------------
// Display the Logging Preferences dialog.

void EmApplication::DoLogging (EmCommandID)
{
	EmDlg::DoEditLoggingOptions (kNormalLogging);
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoDebugging
// ---------------------------------------------------------------------------
// Display the Debugging Preferences dialog.

void EmApplication::DoDebugging (EmCommandID)
{
	EmDlg::DoEditDebuggingOptions ();
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoErrorHandling
// ---------------------------------------------------------------------------
// Display the Error Handling Preferences dialog.

void EmApplication::DoErrorHandling (EmCommandID)
{
	EmDlg::DoEditErrorHandling ();
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoTracing
// ---------------------------------------------------------------------------
// Display the Tracing Preferences dialog.

#if HAS_TRACER
void EmApplication::DoTracing (EmCommandID)
{
	EmDlg::DoEditTracingOptions ();
}
#endif


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoSkins
// ---------------------------------------------------------------------------
// Display the Skins Preferences dialog.

void EmApplication::DoSkins (EmCommandID)
{
	EmDlg::DoEditSkins ();
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoHostFS
// ---------------------------------------------------------------------------
// Display the HostFS Preferences dialog.

void EmApplication::DoHostFS (EmCommandID)
{
	EmDlg::DoEditHostFSOptions ();
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoBreakpoints
// ---------------------------------------------------------------------------
// Display the Breakpoints Preferences dialog.

void EmApplication::DoBreakpoints (EmCommandID)
{
	EmDlg::DoEditBreakpoints ();
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoReplay
// ---------------------------------------------------------------------------
// Open a document specified by the user and start the Playback process.

void EmApplication::DoReplay (EmCommandID)
{
	if (this->HandleOpenFromUser (kFileTypeEvents))
	{
		EmAssert (gSession);
		EmEventPlayback::LoadEvents (gSession->GetFile ());
		EmEventPlayback::ReplayEvents (true);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoMinimize
// ---------------------------------------------------------------------------
// Open a document specified by the user and start the Minimization process.

void EmApplication::DoMinimize (EmCommandID)
{
	if (this->HandleOpenFromUser (kFileTypeEvents))
	{
		EmMinimize::Start ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::DoNothing
// ---------------------------------------------------------------------------
// Null operation.  Used for menu items that can be selected but that don't
// actually do anything.

void EmApplication::DoNothing (EmCommandID)
{
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::CloseDocument
// ---------------------------------------------------------------------------
// Handles the request to close a document.  This request can be denied: if
// the user is asked if they want to save the document, they can cancel the
// operation.  This function returns true if there is no running document when
// it exits (that is, the current document was closed or there was no document
// open to begin with).  Otherwise, it returns false to indicate that a
// document is still open.

Bool EmApplication::CloseDocument (Bool quitting)
{
	if (gDocument)
	{
		gDocument->HandleClose (quitting);
	}

	return gDocument == NULL;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmApplication::IsBound
// ---------------------------------------------------------------------------
//	Returns whether or not this application is bound in any sort of fashion.
//	We cache the result in case this function is called from time-critical
//	functions.

Bool EmApplication::IsBound (void)
{
	static int	result;

	if (result == 0)
	{
		result = this->ROMResourcePresent () ? 1 : -1;
	}

	return result > 0;
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::IsBoundPartially
// ---------------------------------------------------------------------------
//	Returns whether or not this application is partially bound.  Returns false
//	if fully bound or not bound.
//	We cache the result in case this function is called from time-critical
//	functions.

Bool EmApplication::IsBoundPartially (void)
{
	static int	result;

	if (result == 0)
	{
		result = (this->ROMResourcePresent () && !this->PSFResourcePresent ()) ? 1 : -1;
	}

	return result > 0;
}


// ---------------------------------------------------------------------------
//		¥ EmApplication::IsBoundFully
// ---------------------------------------------------------------------------
//	Returns whether or not this application is fully bound.  Return false if
//	partially bound or not bound.
//	We cache the result in case this function is called from time-critical
//	functions.

Bool EmApplication::IsBoundFully (void)
{
	static int	result;

	if (result == 0)
	{
		result = this->PSFResourcePresent () ? 1 : -1;
	}

	return result > 0;
}


