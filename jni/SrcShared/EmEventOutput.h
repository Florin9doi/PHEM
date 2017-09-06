/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef EmEventOutput_h
#define EmEventOutput_h

#include "EmEventPlayback.h"	// EmRecordedEvent
#include "EmStructs.h"			// StringList

#include <string>				// string
#include <strstream>			// strstream


/*
	EmEventOutput tries to write out a human-readable list of steps from a 
	Gremlin event list.  This process has two parts: gathering information 
	while the gremlin events are being played back, and running through the 
	event list and outputting the steps.

	The information-gathering happens when EmEventPlayback::ReplayGetEvent
	or ReplayGetPen calls EmEventOutput::GetEventInfo, right after a gremlin
	event has been posted. GetEventInfo gets info about the posted event, 
	or about a previous event if necessary. This info is stored in the 
	gremlin event structure for later use.

	The step outputting part currently happens when an event minimization 
	completes, and EmMinimize::MinimizationComplete calles 
	EmEventOutput::OutputEvents.

	This system is accessed from the following locations:

		* EmEventPlayback	: Reports that events are being inserted into the
							  system, and that EmEventOutput may want to gather
							  additional contextual information about the event
							  to be used when later logging the event.  To do
							  this, EmEventPlayback calls GetEventInfo, passing
							  in the event being played back.

		* EmMinimize		: Turns information gather on and off.
							  Triggers final output of events when minimization
							  is completed.

		* SysHeadpatch::FrmPopupForm
							: Report that a form is popping up.


	Output is according to this grammar:
	------------------------------------

	All steps are assumed to be numbered starting at one and incrementing by
	one.

	bugReport ::==
		appStart
		events*
		crash

	appStart ::==
		Start "appName" appVersion on a deviceName running Palm OS osVersion.

	events ::==
		tap | write | startApp |
		ejectCard # NYI

	ranking ::==
		first | second | third | nth | last
		# counted top to bottom, left to right

	formObjectType ::==
		button | trigger | selector | checkbox |
		pushbutton | gadget | field | list | table

	changeForm ::==
		to go to { the "formName" view | a new view }
		to open { the "formName" dialog | a new dialog }
		to popup a dialog |

	tapStart ::==
		Tap the "buttonName" {button | trigger | selector | checkbox} |
		Tap the {ranking} {formObjectType} {from the top} |
		Tap in the {ranking} field {from the top} {after the text "fieldText" | at the beginning} |
		Tap the list item "itemName" |
		Tap the {ranking} list item |
		Tap the {ranking} list {from the top}, in the "itemName" item |
		Tap the {ranking} list {from the top}, in the {ranking} item |
		Tap the {ranking} list {from the top} at x, y |
		Tap in the {ranking} table {from the top} at column n row n |
		Tap in the {ranking} table {from the top} at x, y |
		Tap the scroll {up | down} button | # NYI
		Tap the "menuName \ menuItem"  menu item | # NYI
		Tap in the title | # NYI
		Tap at x, y

	tap ::==
		tapStart {changeForm}.

	key ::==
		<backspace> | 
		<tab> | 
		<new line> |
		<carriage return> |
		<delete> |
		c | # normal characters from 0x20 - 0x7E
		<nnn> | # extended characters from 0x80 - 0xFF

	keyButton ::==
		page up | page down |
		left arrow | right arrow | up arrow | down arrow |
		prev field | next field |
		menu |
		command |
		launch |
		keyboard |
		find |
		calc |
		...

	keyAppEvent ::==
		low battery |
		auto off |
		exgtest |
		send data |
		ir receive |
		...

	write ::==
		Write "key+". | # combine adjacent keystrokes if possible.
		Tap the keyButton button. |
		Application receives a keyAppEvent event. |
		Write character nnn. # for key events with no other description

	startApp ::==
		Switch to "appName appVersion" from "oldAppName oldAppVersion". | 
		Switch to any other app. | # if the crash happens before the app exits
		Relaunch "appName appVersion".

	crash ::==
		Crash with error "errorDescription".
		stackCrawl
*/

// As events are replayed, we are called to gather contextual information
// about that event.  We use that contextual information when preparing
// out English output.  The event being replayed is stored along with bits
// and pieces of contextual information in the following structure.

struct EmEventInfo
{
	EmRecordedEvent			event;

	string					newFormText;
	StringList				stackCrawl;
	string					text;
	FormObjectKind			objKind;
	UInt16					objID;
	Int16					rank;
	Int16					row;
	Int16					column;
	ControlStyleType		style;

	// Silkscreen button info
	WChar					asciiCode;
	UInt16					keyCode;
	UInt16					modifiers;
};

typedef vector<EmEventInfo>	EmEventInfoList;

class EmEventOutput
{
	// Interface for gathering event info:

	public:
		static void				StartGatheringInfo		(void);
		static Bool				IsGatheringInfo			(void);
		static void				GatherInfo				(Bool);

		// Called by EmEventPlayback when returning a new event to be played back.

		static void				GetEventInfo			(const EmRecordedEvent&);

		// Called by SysHeadpatch::FrmPopupForm to report that a new form is
		// being popped up.

		static void				PoppingUpForm			(void);

		// Called by Errors::HandleDialog to report that an error occurred
		// and report the text.

		static void				ErrorOccurred			(const string&);

	private:
		static void				GetAwaitedEventInfo		(void);
		static void				GetPenEventInfo			(EmEventInfo&);
		static void				GetAppSwitchEventInfo	(EmEventInfo&);

	// Interface for outputting:

	public:
		// Called by Minimization when minimization is done and the accumulated
		// information needs to be written out.

		static void				OutputEvents			(strstream&);

		// Utility routine to list a stack crawl.  Exported here so that we
		// can share it with EmMinimize, which uses it when reporting that
		// it failed to produce an error on the last run.

		static void				OutputStack				(strstream&, const StringList&);

	private:
		static void				OutputStartStep			(strstream&);
		static void				OutputKeyEventStep		(EmEventInfoList::iterator&, strstream&);
		static void				OutputPenEventStep		(const EmEventInfo&, strstream&);
		static void				OutputAppSwitchEventStep(const EmEventInfo&, strstream&);
		static void				OutputErrorEvent		(const EmEventInfo&, strstream&);

	// Globals:

	private:
		static EmEventInfoList	fgEventInfo;
		static Bool				fgIsGatheringInfo;
		static int				fgCounter;
		static UInt16			fgPreviousFormID;
		static Bool				fgEventAwaitingInfo;
		static Bool				fgWaitForPenUp;
		static StringList		fgLastStackCrawl;
};

#endif // EmEventOutput_h
