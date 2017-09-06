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

#ifndef EmApplication_h
#define EmApplication_h

#include "EmAction.h"			// EmActionHandler
#include "EmCommands.h"			// EmCommandID
#include "EmDevice.h"			// EmDevice
#include "EmFileRef.h"			// EmFileRef, EmFileRefList
#include "EmTypes.h"			// RAMSizeType

#if EMULATION_LEVEL == EMULATION_UNIX
#include <time.h>
#endif
/*
	EmApplication is an abstract class for handling Palm OS Emulator
	operations that don't require a document (or session) to be open or
	running.  That is, it deals with the commands that create or open
	documents, showing preferences and other dialogs, and quitting the
	application.  It is responsible for the following commands:

		* About Box
		* New Session
		* Open Session
		* Close Session
		* Quitting
		* Download ROM
		* All Preferences dialogs
		* Event Playback and Minimization

	EmApplication is mostly a cross-platform "code sharing" class.  It's
	not a true application class in that it doesn't deal with getting
	user events, menu selections, or other platform-specific operations.
	But once those events and selections are made, the EmApplication
	class can be used to *handle* those operations in a cross-platofrm
	way.  Thus, you will most likely create a sub-class of EmApplication
	that deals with user interaction and translate those into cross-
	platform operations.
*/

class Chunk;
class EmDocument;
struct Configuration;

class EmApplication : public EmActionHandler
{
	public:
								EmApplication		(void);
		virtual					~EmApplication		(void);

	public:
#if EMULATION_LEVEL == EMULATION_UNIX
            unsigned long last_ticks;// = 0;
            unsigned long max_tps;// = 0;
            unsigned long min_tps;// = 1000000;
            unsigned long avg_tps;
            unsigned long tps_count;// = 0;
            unsigned long tps_total;// = 0;
            struct timespec last_time;
#endif
            unsigned long ticks_per_second;// = sysTicksPerSecond;

	public:
		virtual Bool			Startup				(int argc, char** argv);
		virtual void			Shutdown			(void);

	public:
		void					HandleStartupActions(void);
		Bool					HandleCommand		(EmCommandID);
		void					HandleIdle			(void);

	public:
		Bool					GetTimeToQuit		(void)			{ return fQuit; }
		virtual void			SetTimeToQuit		(Bool q)		{ fQuit = q; }

	public:
		/*
			This ramp of functions is responsible for creating new sessions.
			They all return a pointer to any document they create.  Note that
			fDocument could be non-NULL and these functions return NULL if
			the user cancelled the closing of the current document.

			HandleNewFromUser	Presents the dialog asking the user for new
								configuration information.  Initial dialog
								settings can either be provided or fetched
								from preferences.  HandleNewFromConfig is
								called to perform the actual open.

			HandleNewFromPref	Creates a new session from the configuration
								information stored in the "last configuration"
								preference.  HandleNewFromConfig is called to
								perform the actual open.

			HandleNewFromROM	Presents the dialog asking the user for new
								configuration information.  Initial dialog
								settings are provided from the "last config-
								uration" preference and the provided ROM.
								HandleNewFromConfig is called to perform the
								actual open.

			HandleNewFromConfig	Creates a new session based on the given
								configuration information.  First validates
								the configuration and throws an exception if
								that fails.  Then attempts to close any current
								session, returning NULL if that operation is
								cancelled by the user.  Finally, creates the
								actual document.  This function is the bottle-
								neck for all document creation.
		*/

		EmDocument*				HandleNewFromUser	(const Configuration*);
		EmDocument*				HandleNewFromPrefs	(void);
		EmDocument*				HandleNewFromROM	(const EmFileRef&);
		EmDocument*				HandleNewFromConfig	(const Configuration&);
		EmDocument*				HandleNewBound		(void);

		/*
			This ramp of functions is responsible for opening old sessions.
			They all return a pointer to any document they open.  Note that
			fDocument could be non-NULL and these functions return NULL if
			the user cancelled the closing of the current document.

			HandleNewFromUser	Asks the user for a file to open. HandleOpenFromFile
								is called to perform the actual open.

			HandleOpenFromPrefs	Open the session file stored in the "last
								session file" preference.  HandleOpenFromFile
								is called to perform the actual open.

			HandleOpenFromFile	Opens the given file.  First attempts to close
								any current session, returning NULL if that
								operation is cancelled by the user.  Then opens
								the actual document.  This function is the
								bottleneck for all document opening.
		*/

		EmDocument*				HandleOpenFromUser	(EmFileType);
		EmDocument*				HandleOpenFromPrefs	(void);
		EmDocument*				HandleOpenFromFile	(const EmFileRef&);
		EmDocument*				HandleOpenBound		(void);

		/*
			Deal with figuring out what to do with the given files.  Parse
			them up, figure out what type(s) they are, and determine whether
			to open them, create new sessions with them, install them, etc.
		*/

		void					HandleFileList		(const EmFileRefList&);

		/*
			Open the given session/event file for minimization.
		*/

		void					HandleMinimize	(const EmFileRef&);

		/*
			Close a session, writing it out to the given file.  If the file
			is not specified, just close the file without saving it.  Called
			in response to ScheduleSessionClose.
		*/

		void					HandleSessionClose	(const EmFileRef&);

		/*
			Quit the emulator, closing any session without saving it.  Called
			in response to ScheduleQuit.
		*/

		void					HandleQuit			(void);

	public:
		// The following functions schedule actions for the application
		// to take the next chance it gets (at idle time).

		void					ScheduleSessionClose	(const EmFileRef&);
		void					ScheduleQuit			(void);	// Close Now and Quit

	public:
		// Command handling functions, called from HandleCommand.
		//
		// I'd like these to be private, but I build up a static table containing
		// references to these functions, thus requiring them to be public.

		void					DoAbout				(EmCommandID);
		void					DoNew				(EmCommandID);
		void					DoOpen				(EmCommandID);
		void					DoClose				(EmCommandID);
		void					DoQuit				(EmCommandID);

		void					DoDownload			(EmCommandID);

		void					DoPreferences		(EmCommandID);
		void					DoLogging			(EmCommandID);
		void					DoDebugging			(EmCommandID);
		void					DoErrorHandling		(EmCommandID);
#if HAS_TRACER
		void					DoTracing			(EmCommandID);
#endif
		void					DoSkins				(EmCommandID);
		void					DoHostFS			(EmCommandID);
		void					DoBreakpoints		(EmCommandID);

		void					DoReplay			(EmCommandID);
		void					DoMinimize			(EmCommandID);

		void					DoNothing			(EmCommandID);

	public:
		// Bound Poser facilities.

		Bool					IsBound					(void);
		Bool					IsBoundPartially		(void);
		Bool					IsBoundFully			(void);

		virtual void			BindPoser				(Bool /*fullSave*/,
														 const EmFileRef& /*dest*/) {}

		virtual Bool 			ROMResourcePresent		(void)		{ return false; }
		virtual Bool	 		GetROMResource			(Chunk&)	{ return false; }

		virtual Bool	 		PSFResourcePresent		(void)		{ return false; }
		virtual Bool 			GetPSFResource			(Chunk&)	{ return false; }

		virtual Bool	 		ConfigResourcePresent	(void)		{ return false; }
		virtual Bool 			GetConfigResource		(Chunk&)	{ return false; }

		virtual Bool 			SkinfoResourcePresent	(void)		{ return false; }
		virtual Bool 			GetSkinfoResource		(Chunk&)	{ return false; }

		virtual Bool	 		Skin1xResourcePresent	(void)		{ return false; }
		virtual Bool 			GetSkin1xResource		(Chunk&)	{ return false; }

		virtual Bool 			Skin2xResourcePresent	(void)		{ return false; }
		virtual Bool 			GetSkin2xResource		(Chunk&)	{ return false; }

		virtual EmDevice		GetBoundDevice			(void)		{ return EmDevice (); }
		virtual RAMSizeType		GetBoundRAMSize			(void)		{ return 0; }

	private:
		Bool					CloseDocument		(Bool quitting);
                void  Prv_Measure_Ticks_Per_Second();

	private:
		Bool					fQuit;
};

extern EmApplication*	gApplication;

#endif	// EmApplication_h
