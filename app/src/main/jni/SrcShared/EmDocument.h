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

#ifndef EmDocument_h
#define EmDocument_h

#include "EmAction.h"			// EmAction
#include "EmCommands.h"			// EmCommandID
#include "EmDlg.h"				// EmDlgItemID
#include "EmFileRef.h"			// EmFileRef
#include "Skins.h"				// SkinElementType

class EmApplication;
class EmSession;
class EmWindow;

struct EmKeyEvent;

/*
	EmDocument is responsible for handling commands operations related to
	an open and running document. It is responsible for the following:

		* Creating new documents
		* Loading old documents
		* Saving documents (possibly asking for a (new) name).
		* Closing documents
		* Handling document-specific commands:
			* Save
			* Save As
			* Save Bound
			* Save Screen
			* Session Info
			* Import Database
			* Export Database
			* HotSync
			* Reset
			* Undo/Cut/Copy/Paste/Clear
			* Gremlins (New/Stop/Resume/Step)
			* Profiling (Start/Stop/Dump)
		* Migrating key and buttons events.

	EmDocument is mostly a cross-platform "code sharing" class.  It's
	not a true document class in that it doesn't deal with getting
	user events, menu selections, or other platform-specific operations.
	But once those events and selections are made, the EmDocument
	class can be used to *handle* those operations in a cross-platofrm
	way.  Thus, you will most likely create a sub-class of EmDocument
	that deals with user interaction and translate those into cross-
	platform operations.

	EmDocument is owned by the EmApplication.  In turn, EmDocument owns
	the EmWindow in which it displays its stuff.
*/


class EmDocument : public EmActionHandler
{
	public:
		/*
			Bottlenecks for creating and opening documents.
		*/
		static EmDocument*		DoNew				(const Configuration&);
		static EmDocument*		DoOpen				(const EmFileRef&);

		static EmDocument*		DoNewBound			(void);
		static EmDocument*		DoOpenBound			(void);

	protected:
								EmDocument			(void);
		virtual					~EmDocument			(void);

	public:
		Bool					HandleCommand		(EmCommandID);
		void					HandleKey			(const EmKeyEvent&);
		void					HandleButton		(SkinElementType, Bool isDown);
		virtual void			HandleIdle			(void);

	public:
		EmSession*				GetSession			(void) const { return fSession; }
		EmFileRef				GetFileRef			(void) const { return fFile; }

	public:
		/*
			This ramp of functions is responsible for saving sessions.
			They all return a Boolean indicating whether or not the
			session was actually saved.  On failure, they throw exceptions.

			HandleSave		Standard Save operation.  If the document has
							been saved before, calls through to HandleSaveTo.
							Otherwise, calls through to HandleSaveAs.

			HandleSaveAs	Standard Save As operation.  Asks the user for a
							file to which to save the document.  If the user
							doesn't cancel, call through to HandleSaveTo.

			HandleSaveTo	Bottleneck for all save operations.  Assumes the
							given file reference is valid; the file may or
							may not already exist.  On success, the document
							remembers that it is associated with the file it
							just saved itself to and updates the "last session"
							and "session MRU" preferences.
		*/

		Bool					HandleSave			(void);
		Bool					HandleSaveAs		(void);
		Bool					HandleSaveTo		(const EmFileRef&);

		Bool					HandleClose			(Bool quitting);	// Deletes self!
		Bool					HandleClose			(CloseActionType, Bool quitting);	// Deletes self!

	public:
		// The following functions schedule actions for the document
		// to take the next chance it gets (at idle time).

		void					ScheduleNewHorde	(const HordeInfo&);
		void					ScheduleDialog		(EmDlgThreadFn fn,
													 const void* parms,
													 EmDlgItemID& result);

	public:
		// Command handling functions, called from HandleCommand.
		//
		// I'd like these to be private, but I build up a static table containing
		// references to these functions, thus requiring them to be public.
		void					DoSave				(EmCommandID);
		void					DoSaveAs			(EmCommandID);
		void					DoSaveBound			(EmCommandID);
		void					DoSaveScreen		(EmCommandID);
		void					DoInfo				(EmCommandID);
		void					DoImport			(EmCommandID);
		void					DoExport			(EmCommandID);
		void					DoHotSync			(EmCommandID);
		void					DoReset				(EmCommandID);

		void					DoGremlinNew		(EmCommandID);
		void					DoGremlinSuspend	(EmCommandID);
		void					DoGremlinStep		(EmCommandID);
		void					DoGremlinResume		(EmCommandID);
		void					DoGremlinStop		(EmCommandID);

#if HAS_PROFILING
		void					DoProfileStart		(EmCommandID);
		void					DoProfileStop		(EmCommandID);
		void					DoProfileDump		(EmCommandID);
#endif

	public:
		// I'd like these to be private, but they are currently needed
		// by the "What To Do" dialog.
		static Bool				AskNewSession		(Configuration&);
		static Bool				AskSaveSession		(EmFileRef&);
		static Bool				AskLoadSession		(EmFileRef&, EmFileType);

	public:
		// I'd like these to be private, but CEmulatorDoc::AskSaveChanges
		// needs access to AskSaveChanges.
		EmDlgItemID				AskSaveChanges		(Bool quitting);
		Bool					AskSaveScreen		(EmFileRef&);
		Bool					AskImportFiles		(EmFileRefList&);

	public:
		// I'd like these to be private, but at least one part of Poser
		// needs access to HostSaveScreen.
		static EmDocument*		HostCreateDocument	(void);
		virtual Bool			HostCanSaveBound	(void);
		virtual void			HostSaveScreen		(const EmFileRef&);

	private:
		void					PrvOpenWindow		(void);

	protected:
		EmSession*				fSession;
		EmFileRef				fFile;
};

extern EmDocument* gDocument;

#endif	// EmDocument_h
