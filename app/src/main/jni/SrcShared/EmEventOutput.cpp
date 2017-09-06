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

#include "EmCommon.h"
#include "EmEventOutput.h"

#include "EmMemory.h"			// EmMem_strcpy
#include "EmPalmOS.h"			// GenerateStackCrawl
#include "EmPalmStructs.h"		// EmAliasControlType
#include "EmPatchState.h"		// EmPatchState::GetCurrentAppInfo
#include "EmSession.h"			// gSession
#include "ErrorHandling.h"		// Errors::GetAppName, GetAppVersion
#include "Miscellaneous.h"		// StackCrawlStrings
#include "PreferenceMgr.h"		// Preference, gEmuPrefs
#include "ROMStubs.h"			// FtrGet


// Special rank positions:

#define kObjectRank_None -2
#define kObjectRank_Last -1

// PalmOS table font:

#define tableFont boldFont

// WindowFlagsType modal flag bit:

#define WindowTypeFlags_modal 0x2000

// Useful macros:

static inline Bool PrvIsPenEvent (const EmRecordedEvent& event)
{
	return event.eType == kRecordedPenEvent;
}

static inline Bool PrvIsPenUp (const PointType& pt)
{
	return pt.x == -1 && pt.y == -1;
}

static inline Bool PrvIsPenDown (const PointType& pt)
{
	return !::PrvIsPenUp (pt);
}

static inline Bool PrvIsPenUp (const EmRecordedEvent& event)
{
	return ::PrvIsPenEvent (event) && ::PrvIsPenUp (event.penEvent.coords);
}

static inline Bool PrvIsPenDown (const EmRecordedEvent& event)
{
	return ::PrvIsPenEvent (event) && ::PrvIsPenDown (event.penEvent.coords);
}


#define POINT_IN_RECT(X,Y,R) (X >= R.topLeft.x && \
							  Y >= R.topLeft.y && \
							  X <= R.topLeft.x + R.extent.x && \
							  Y <= R.topLeft.y + R.extent.y)

#define IS_EXTENDED(chrcode) (chrcode >= 0x0080 && chrcode <= 0x00FF)


// Global state variables:

EmEventInfoList	EmEventOutput::fgEventInfo;
Bool			EmEventOutput::fgIsGatheringInfo;
int				EmEventOutput::fgCounter;
UInt16			EmEventOutput::fgPreviousFormID;
Bool			EmEventOutput::fgEventAwaitingInfo;	// Set if tapped on field or list; cleared when we get the awaited information.
Bool			EmEventOutput::fgWaitForPenUp;


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::StartGatheringInfo
 *
 * DESCRIPTION: Reset our state in preparation to start gathering info.
 *				This should be called each time we start replaying an
 *				event list.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::StartGatheringInfo (void)
{
	fgEventInfo.clear ();
	fgCounter			= 0;
	fgPreviousFormID	= 0;
	fgEventAwaitingInfo	= false;
	fgWaitForPenUp		= false;

	EmEventOutput::GatherInfo (true);
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::IsGatheringInfo
 *
 * DESCRIPTION: Return whether or not we are currently gathering information.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if we are currently gathering info.
 *
 ***********************************************************************/

Bool EmEventOutput::IsGatheringInfo (void)
{
	return fgIsGatheringInfo;
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::GatherInfo
 *
 * DESCRIPTION: Turn information gathering on or off.
 *
 * PARAMETERS:	gatherInfo - whether or not we're turning on
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::GatherInfo (Bool gatherInfo)
{
	fgIsGatheringInfo = gatherInfo;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	PrvGetStringFromEmuMemory
 *
 * DESCRIPTION: Retrieve a string from emulator memory and store it in
 *				a string object.
 *
 * PARAMETERS:	emustr - pointer to the string in emulator memory
 *
 * RETURNED:	The string object.
 *
 ***********************************************************************/

static string PrvGetStringFromEmuMemory (emuptr emustr)
{
	string str;

	if (!emustr)
		return str;

	size_t len = EmMem_strlen (emustr);

	if (len)
	{
		char* temp = new char [len + 1];

		if (temp == NULL) 
		{
			// Failed to allocate space for temporary buffer.
			return str;
		}

		EmMem_strcpy (temp, emustr);

		str = temp;

		delete [] temp;
	}

	return str;
}


/***********************************************************************
 *
 * FUNCTION:	PrvClearEventData
 *
 * DESCRIPTION: Clear out information gathered and stored in an event
 *				without disturbing the event itself.
 *
 * PARAMETERS:	event - event to clear
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

static void PrvClearEventData (EmEventInfo& eventInfo)
{
	eventInfo.text.empty ();
	eventInfo.newFormText.empty ();
	eventInfo.stackCrawl.clear ();

	eventInfo.objKind	= (FormObjectKind) -1;
	eventInfo.objID		= 0;
	eventInfo.rank		= kObjectRank_None;
	eventInfo.row		= -1;
	eventInfo.column	= -1;
	eventInfo.style		= (ControlStyleType) -1;

	eventInfo.asciiCode	= 0;
	eventInfo.keyCode	= 0;
	eventInfo.modifiers	= 0;
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::GetEventInfo
 *
 * DESCRIPTION: Master function for retrieving information from the
 *				emulator related to gremlin events being posted.
 *
 * PARAMETERS:	event - the current event being replayed.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::GetEventInfo (const EmRecordedEvent& event)
{
	if (!fgIsGatheringInfo)
		return;

	CEnableFullAccess munge;	// Direct access to form.window.windowFlags.flags.

	// Clear current event data.

	EmEventInfo	eventInfo;
	::PrvClearEventData (eventInfo);

	eventInfo.event = event;

	// Get the active form and form id.
	// This may be incorrect for popup dialogs.

	FormType* frmP = ::FrmGetActiveForm ();
	
	UInt16 currentFormID = 0;

	if (frmP)
	{
		currentFormID = ::FrmGetFormId (frmP);
	}

	// If the active form has changed since last event:

	if ((currentFormID != fgPreviousFormID) && currentFormID)
	{
		// Cancel the event awaiting info, because it is no longer possible to get info.

		fgEventAwaitingInfo = false;

		if (currentFormID)
		{
			// If there was a previous event, associate the form change with it.

			if (fgEventInfo.size () > 0)
			{
				EmEventInfo&	prevEvent = fgEventInfo.back ();

				EmAliasFormType<PAS> form ((emuptr) frmP);

				// Get the new form's title.

				emuptr emuTitle = (emuptr)::FrmGetTitle (frmP);

				if (emuTitle)
				{
					string title = ::PrvGetStringFromEmuMemory (emuTitle);

					if (!title.empty ())
					{
						if (form.window.windowFlags.flags & WindowTypeFlags_modal)
						{
							prevEvent.newFormText = string("open the \"") + title + string("\" dialog");
						}
						else
						{
							prevEvent.newFormText = string("go to the \"") + title + string("\" view");
						}
					}
				}

				// If there is no title:

				if (prevEvent.newFormText.empty ())
				{
					if (form.window.windowFlags.flags & WindowTypeFlags_modal)
					{
						prevEvent.newFormText = "go to a new dialog";
					}
					else
					{
						prevEvent.newFormText += "go to a new view";
					}
				}
			}
		}
	}

	// A previous pen down event may have set the fgEventAwaitingInfo flag.
	// This flag means that we want to scope out the state of the system
	// later to find out what the pen event caused to happen.  We gather
	// that information on the next event after the ensuing pen up event.
	//
	// This will not work 100% of the time, because the event's target object
	// may have disappeared, the active form may have changed, or the
	// application may have done something to the object.

	if (fgEventAwaitingInfo)
	{
		// If we're waiting for a pen-up event, see if we found it.

		if (fgWaitForPenUp)
		{
			if (::PrvIsPenUp (eventInfo.event))
			{
				fgWaitForPenUp = false;
			}
		}
		else 
		{
			EmEventOutput::GetAwaitedEventInfo ();

			fgEventAwaitingInfo = false;
		}
	}

	// Get information about current event.
	// Skip pen up events.

	if (!::PrvIsPenUp (eventInfo.event))
	{
		switch (eventInfo.event.eType)
		{
			case kRecordedPenEvent:

				EmEventOutput::GetPenEventInfo (eventInfo);

				break;

			case kRecordedAppSwitchEvent:

				EmEventOutput::GetAppSwitchEventInfo (eventInfo);

				break;

			default:

				break;
		}
	}

	fgPreviousFormID = currentFormID;

	// For some reason, I sometimes see *two* sets of instructions
	// in the output -- one a duplicate of the other.  However, they
	// are all number consecutively, which implies that OutputEvents
	// is called just once, and that the doubling effect is because
	// two sets of events are in fgEventInfo.  Put a check here to
	// see if that's the case.

	if (fgEventInfo.size () > 0)
	{
		EmAssert (fgEventInfo.back ().event.eType != kRecordedErrorEvent);
	}

	fgEventInfo.push_back (eventInfo);
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::PoppingUpForm
 *
 * DESCRIPTION: Called by the FrmPopupForm headpatch to tell us that
 *				the application is popping up a dialog.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::PoppingUpForm (void)
{
	if (!fgIsGatheringInfo)
		return;

	// If there was a previous event, associate the form change with it.

	if (fgEventInfo.size () > 0)
	{
		fgEventInfo.back ().newFormText = "popup a dialog";
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::ErrorOccurred
 *
 * DESCRIPTION: Called by Errors::HandleDialog to tell us the text of
 *				the error that occurred.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::ErrorOccurred (const string& msg)
{
	if (!fgIsGatheringInfo)
		return;

	// Create a new event recording the information.

	EmEventInfo	eventInfo;

	eventInfo.event.eType = kRecordedErrorEvent;
	eventInfo.text = msg;

	// Capture the stack crawl.

	EmStackFrameList	stackCrawl;
	EmPalmOS::GenerateStackCrawl (stackCrawl);

	// Generate a full stack crawl.

	::StackCrawlStrings (stackCrawl, eventInfo.stackCrawl);

	// Save this information at the end of our collection.

	fgEventInfo.push_back (eventInfo);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	PrvObjectIsHigherRank
 *
 * DESCRIPTION: Determine if an object is ranked higher inside a form
 *				than another object. Ranking is top to bottom, then
 *				left to right.
 *
 * PARAMETERS:	r1 - bounds of object in question
 *
 *				r2 - bounds of object we're comparing to
 *
 * RETURNED:	True if object (r1) is higher-ranked.
 *
 ***********************************************************************/

static Bool PrvObjectIsHigherRank (RectangleType& r1, RectangleType& r2)
{
	if (r2.topLeft.y < r1.topLeft.y) 
		return true;

	if (r2.topLeft.y == r1.topLeft.y)
	{
		if (r2.topLeft.x < r1.topLeft.x)
			return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	PrvFindObjectRank
 *
 * DESCRIPTION: Find the rank number of an object inside a form.
 *
 * PARAMETERS:	frmP - pointer to form containing the object
 *
 *				objectIndices - vector array of object indexes in form
 *
 *				object1 - index of the object to rank
 *
 * RETURNED:	Rank value of the object.
 *
 ***********************************************************************/

static int16 PrvFindObjectRank (FormPtr frmP, vector<uint16>& objectIndices, uint16 object1)
{
	EmAssert (frmP);

	RectangleType r1;
	::FrmGetObjectBounds (frmP, object1, &r1);

	FormObjectKind objectKind1 = ::FrmGetObjectType (frmP, object1);

	int rank = 0;
	uint16 siblingCount = 0;

	vector<uint16>::iterator iter = objectIndices.begin ();

	while (iter != objectIndices.end ())
	{
		// Make sure object is not ourself

		if (*iter != object1)
		{
			// Make sure object is the same kind

			FormObjectKind objectKind2 = ::FrmGetObjectType (frmP, *iter);

			if (objectKind2 == objectKind1)
			{
				siblingCount++;

				RectangleType r2;
				::FrmGetObjectBounds (frmP, *iter, &r2);

				if (::PrvObjectIsHigherRank (r1, r2))
					rank++;
			}
		}

		++iter;
	}

	// No ranking if we are all alone.

	if (siblingCount == 0)
		return kObjectRank_None;

	// Special case for the last object in the form.

	else if (rank == siblingCount)	
		return kObjectRank_Last;

	// Else just use the rank number.

	return rank;
}


/***********************************************************************
 *
 * FUNCTION:    PrvPointInTableItem
 *
 * DESCRIPTION: This routine computes which row, column a point is in.
 *				It was adapted from the function 'PointInTableItem' in
 *				UI/src/Table.cpp by art and peter.
 *
 * PARAMETERS:	tableP - pointer to a table object
 *
 *				x - point's x coordinate
 *
 *				y - point's y coordinate
 *
 *				rrow - return the row
 *
 *				rcol - return the column
 *
 * RETURNED:	True if the point is in at item.
 *
 ***********************************************************************/

static Boolean PrvPointInTableItem (TableType* tableP, Coord x, Coord y, Int16& rrow, Int16& rcol)
{
	EmAssert (tableP);

	CEnableFullAccess	munge;

	Int16 row, col;
	Int16 numRows, numCols;
	RectangleType r;

	EmAliasTableType<PAS> table ((emuptr)tableP);

	RectangleType bounds;
	bounds.topLeft.x = table.bounds.topLeft.x;
	bounds.topLeft.y = table.bounds.topLeft.y;
	bounds.extent.x = table.bounds.extent.x;
	bounds.extent.y = table.bounds.extent.y;

	if (!POINT_IN_RECT(x, y, bounds))
		return false;

	r.topLeft.x = table.bounds.topLeft.x;
	r.topLeft.y = table.bounds.topLeft.y;

	numRows = table.numRows;
	for (row = table.topRow; row < numRows; row++)
	{
		// Is the point within the bounds of the row.

		r.extent.x = table.bounds.extent.x;
		r.extent.y = ::TblGetRowHeight (tableP, row);

		if (POINT_IN_RECT(x, y, r))
		{
			numCols = table.numColumns;
			for (col = 0; col < numCols; col++)
			{
				// Is the point within the bounds of the column

				r.extent.x = ::TblGetColumnWidth (tableP, col);
				r.extent.y = ::TblGetRowHeight (tableP, row);

				if (POINT_IN_RECT(x, y, r))
				{
					rrow = row;
					rcol = col;
					return true;
				}

				// Move to the next column.

				r.topLeft.x += r.extent.x + ::TblGetColumnSpacing (tableP, col);
			}

			// We were in a useable row, but we were not in any of the columns.

			return false;
		}

		// Move to next row.

		r.topLeft.x = table.bounds.topLeft.x;
		r.topLeft.y += ::TblGetRowHeight (tableP, row);
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::GetPenEventInfo
 *
 * DESCRIPTION: Get information about the current pen event. EventAwaitingInfo
 *				is set for events that need to retrieve info a little later.
 *
 * PARAMETERS:	event - the current event being replayed
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::GetPenEventInfo (EmEventInfo& eventInfo)
{
	EmAssert (eventInfo.event.eType == kRecordedPenEvent);
	EmAssert (fgIsGatheringInfo);

	CEnableFullAccess munge;

	// Get the pen location in display coordinates.

	EmPoint	displayPt (eventInfo.event.penEvent.coords);
	Int16	displayX = displayPt.fX;
	Int16	displayY = displayPt.fY;

	EmPoint	localPt	= displayPt;
	Int16	localX	= displayX;
	Int16	localY	= displayY;

	// Get the active form.

	FormPtr	frmP = ::FrmGetActiveForm ();

	// Get the objects in that form.

	vector<uint16> objectIndices;
	::CollectOKObjects (frmP, objectIndices);

	// Convert the pen location to local form coordinates.

	if (frmP)
	{
		WinHandle	oldDrawWin = ::WinSetDrawWindow (::FrmGetWindowHandle (frmP));
		::WinDisplayToWindowPt (&localX, &localY);
		::WinSetDrawWindow (oldDrawWin);

		localPt = EmPoint (localX, localY);
	}

	// Iterate over the objects, finding one we hit.

	vector<uint16>::iterator	iter = objectIndices.begin ();

	while (iter != objectIndices.end ())
	{
		RectangleType	r;
		::FrmGetObjectBounds (frmP, *iter, &r);

		EmRect	bounds (r);

		// Must expand bounds a little to match what PalmOS detects as tapping inside an object.

		bounds.fBottom += 2;
		bounds.fRight += 2;

		if (bounds.Contains (localPt))
		{
			break;
		}

		++iter;
	};

	// If we found a hit on a form object...

	if (iter != objectIndices.end ())
	{
		eventInfo.objID = *iter;

		// Get its type.
									  
		eventInfo.objKind = ::FrmGetObjectType (frmP, *iter);

		// Get its rank.

		eventInfo.rank = ::PrvFindObjectRank (frmP, objectIndices, *iter);

		// Get special info for object type.

		switch (eventInfo.objKind)
		{
			case frmFieldObj:
			{ 
				// Set up to retrieve info during the next event.

				eventInfo.text.empty ();
				fgEventAwaitingInfo = true;
				fgWaitForPenUp = false;

				break;
			}

			case frmControlObj:
			{
				emuptr	ctlP = (emuptr) ::FrmGetObjectPtr (frmP, *iter);

				if (ctlP)
				{
					EmAliasControlType<PAS>	control (ctlP);

					// Store control style.

					eventInfo.style = control.style;

					// Store text label if it exists.

					eventInfo.text = ::PrvGetStringFromEmuMemory (control.text);

					if (!eventInfo.text.empty ())
					{
						// Check if label is unique. If so, no need for rank.

						eventInfo.rank = kObjectRank_None;

						// TODO: Else rank it amongst peers with same label.
					}
				}

				break;
			}

			case frmListObj:
			{
				// Set up to retrieve info during the next event.

				eventInfo.text.empty ();
				eventInfo.row = -1;
				fgEventAwaitingInfo = true;
				fgWaitForPenUp = true;

				break;
			}

			case frmTableObj:
			{
				TableType* tableP = (TableType*)::FrmGetObjectPtr (frmP, *iter);

				if (tableP)
				{
					::PrvPointInTableItem (tableP, localX, localY,
						eventInfo.row, eventInfo.column);
				}

				break;
			}

			case frmLabelObj:

//				my ($label_id) = FrmGetObjectId ($form, $ii);
//				my ($address, $label) = FrmGetLabel($form, $label_id);
//				$line .= " \"$label\"";

//				event.text = "labeltext";
				break;

			case frmTitleObj:

//				my ($address, $title) = FrmGetTitle($form,);
//				$line .= " \"$title\"";

//				event.text = "titletext";
				break;

			default:
				break;	// Handle all other cases to keep gcc happy.
		}
	}

	// Otherwise, see if we hit in the silkscreen area.

	else 
	{
		UInt16						numButtons;
		const PenBtnInfoType*		buttonListP = EvtGetPenBtnList(&numButtons);
		EmAliasPenBtnInfoType<PAS>	buttonList ((emuptr) buttonListP);

		for (UInt16 buttonIndex = 0; buttonIndex < numButtons; ++buttonIndex)
		{
			EmAliasPenBtnInfoType<PAS>	button = buttonList[buttonIndex];

			RectangleType	buttonBounds;
			buttonBounds.topLeft.x	= button.boundsR.topLeft.x;
			buttonBounds.topLeft.y	= button.boundsR.topLeft.y;
			buttonBounds.extent.x	= button.boundsR.extent.x;
			buttonBounds.extent.y	= button.boundsR.extent.y;

			if (POINT_IN_RECT(displayX, displayY, buttonBounds))
			{
				eventInfo.asciiCode	= button.asciiCode;
				eventInfo.keyCode	= button.keyCode;
				eventInfo.modifiers	= button.modifiers;
				break;
			}
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::GetAppSwitchEventInfo
 *
 * DESCRIPTION: Get information about the current app switch event.
 *				In particular, any recorded "from application"
 *				information recorded in it is probably incorrect now,
 *				so let's generate some new information.
 *
 * PARAMETERS:	event - the current event being replayed
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::GetAppSwitchEventInfo (EmEventInfo& eventInfo)
{
	EmuAppInfo		currentApp = EmPatchState::GetCurrentAppInfo ();

	eventInfo.event.appSwitchEvent.oldCardNo	= currentApp.fCardNo;
	eventInfo.event.appSwitchEvent.oldDbID		= currentApp.fDBID;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	PrvFindInsertionPointString
 *
 * DESCRIPTION: Create a string that identifies where a field was tapped,
 *				using the position insertion point. Currently we just
 *				copy the contents of the field up to the insertion point.
 *
 * PARAMETERS:	emustr - contents of the field in emulator memory
 *
 *				pos - position of the insertion point
 *
 * RETURNED:	String identifying where the field was tapped.
 *
 ***********************************************************************/

static string PrvFindInsertionPointString (emuptr emustr, UInt16 pos)
{
	EmAssert (emustr);

	// Copy the text before pos from emulator memory into local temp buffer.

	char* temp = new char [pos + 1];
	if (temp == NULL)
	{
		// Failed to create temp buffer.
		return string ();
	}

	::EmMem_strncpy (temp, emustr, (size_t) pos);

	// Perhaps.. Scan backwards from pos until string up to pos is unique inside entire field string.
	// For now we'll just use the entire string up to pos.

	temp[pos] = 0;

	// Return result.

	string finalString = temp;

	delete [] temp;

	return finalString;
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::GetAwaitedEventInfo
 *
 * DESCRIPTION: Master function to get information from the emulator about 
 *				an event that has been waiting for information. This 
 *				mechanism is used to give the running application or OS 
 *				enough time to respond to a gremlin event before we try to 
 *				get information about it. This is used for finding where we 
 *				tapped inside a field, a list, or a table. After the gremlin 
 *				taps inside the target object, FldHandleEvent, LstHandleEvent, 
 *				or TblHandleEvent will compute the new insertion point or 
 *				selection due to that pen event. We then try to retrieve the 
 *				results of that computation. This method won't work if the
 *				application changes the target object before we have a chance
 *				to retrieve its info, or the object gets destroyed.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::GetAwaitedEventInfo (void)
{
	EmAssert (fgEventAwaitingInfo);
	EmAssert (fgIsGatheringInfo);

	CEnableFullAccess	munge;	// Remove blocks on memory access.

	// It is entirely possible that the active form has been destroyed or changed.

	FormType* frmP = ::FrmGetActiveForm ();

	if (frmP == NULL)
	{
		return;
	}

	// Look for an event that needs completing.

	Bool	foundAwaitedEvent = false;
	EmEventInfoList::reverse_iterator	iter = fgEventInfo.rbegin ();

	while (!foundAwaitedEvent && iter != fgEventInfo.rend ())
	{
		EmEventInfo&	awaitingEvent = *iter++;

		switch (awaitingEvent.event.eType)
		{
			case kRecordedPenEvent:
			{
				switch (awaitingEvent.objKind)
				{
					case frmFieldObj:
					{
						// Find the field text to tap after.

						FieldType* fldP = (FieldType*) ::FrmGetObjectPtr (frmP, awaitingEvent.objID);

						if (fldP)
						{
							emuptr txtP = (emuptr) ::FldGetTextPtr (fldP);

							if (txtP)
							{
								UInt16 pos = ::FldGetInsPtPosition (fldP);

								if (pos == 0)
								{
									// Tap at the beginning.

									awaitingEvent.text = " at the beginning";
								}
								else
								{
									// Tap after the text 'blah'.

									string aftertext = 
										::PrvFindInsertionPointString (txtP, pos);

									awaitingEvent.text = " after the text \"" + aftertext + "\"";
								}
							}
						}

						foundAwaitedEvent = true;

						break;
					}	// case frmFieldObj

					case frmListObj:
					{
						// Find the list item to tap on.

						ListType* lstP = (ListType*)::FrmGetObjectPtr (frmP, awaitingEvent.objID);

						if (lstP)
						{
							Int16 currentItem = ::LstGetSelection (lstP);
							
							// If there is a selection:

							if (currentItem != noListSelection)
							{
								// Use text if it exists, else use row number.

								emuptr textP = (emuptr) ::LstGetSelectionText (lstP, currentItem);

								if (textP && ::EmMem_strlen(textP) > 0)
								{
									// This may not be unique...
									awaitingEvent.text = ::PrvGetStringFromEmuMemory (textP);
								}
								else 
								{
									// Should this be relative to topItem?
									awaitingEvent.row = currentItem;
								}
							}
						}

						foundAwaitedEvent = true;

						break;
					}	// case frmListObj

					default:
					{
						// fgEventAwaitingInfo set inappropriately

						EmAssert (false);
					}
				}	// switch (objKind)

				break;
			}	// case pen event

			default:
			{
				// fgEventAwaitingInfo set inappropriately

				EmAssert (false);
			}
		}	// switch (eType)
	}	// while
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::OutputEvents
 *
 * DESCRIPTION: Master output function - write out a text description of
 *				a gremlin event list.
 *
 * PARAMETERS:	events - list of events to output
 *
 *				stream - string stream to write output into
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::OutputEvents (strstream& stream)
{
	// Start step counter from one. The current step number is not necessarily
	// the same as the current event index, because multiple events can go into 
	// a single step.

	fgCounter = 1;

	// Write out starting step.

	EmEventOutput::OutputStartStep (stream);

	// Iterate over all the events.

	EmEventInfoList::iterator	iter (fgEventInfo.begin ());

	while (iter != fgEventInfo.end ())
	{
		switch (iter->event.eType)
		{
			case kRecordedKeyEvent:

				EmEventOutput::OutputKeyEventStep (iter, stream);
				break;

			case kRecordedPenEvent:

				EmEventOutput::OutputPenEventStep (*iter, stream);
				break;

			case kRecordedAppSwitchEvent:
				
				EmEventOutput::OutputAppSwitchEventStep (*iter, stream);
				break;

			case kRecordedErrorEvent:

				EmEventOutput::OutputErrorEvent (*iter, stream);
				break;

			case kRecordedNullEvent:

				// No output for null events.
				break;

			default:

				stream << "Unknown event" << endl;
				EmAssert (false);
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::OutputStartStep
 *
 * DESCRIPTION: Write out the description of the starting step,
 *				ie launching the application being exercised.
 *
 * PARAMETERS:	stream - the output stream to write into
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::OutputStartStep (strstream& stream)
{
	stream << endl;
	stream << fgCounter << ". ";

	// Get and write out application name and version.

	string	appNameUC;
	string	appNameLC;
	string	appVersion;

	Errors::GetAppName (appNameUC, appNameLC);
	Errors::GetAppVersion (appVersion);	

	stream << "Start \"" << appNameLC << "\" " << appVersion << "";

	// Device name:

	Preference<Configuration>	pref (kPrefKeyLastConfiguration);

	Configuration	cfg				= *pref;
	EmDevice		device			= cfg.fDevice;
	string			deviceStr		= device.GetMenuString ();

	stream << " on a " << deviceStr;

	// ROM version:

	UInt32 romVersionData;
	::FtrGet (sysFileCSystem, sysFtrNumROMVersion, &romVersionData);

	UInt32 romVersionMajor = sysGetROMVerMajor (romVersionData);
	UInt32 romVersionMinor = sysGetROMVerMinor (romVersionData);

	stream 
		<< " running Palm OS " 
		<< romVersionMajor << "." << romVersionMinor 
		<< "." << endl;	

	fgCounter++;
}


#pragma mark -

enum KeyType
{
	kKeyType_Writeable,		// a normal character that can be entered by typing/graffiti
	kKeyType_Button,		// a button
	kKeyType_Event			// an event, not necessarily from user input
};


struct KeyDescription
{
	char *	description;
	uint16	chrcode;
	KeyType type;
};


// List of text descriptions for different kinds of character codes.
// Palm OS chr and vchr defines are in Core/System/Chars.h.
// Items followed by a '-' are not currently generated by gremlins.

static KeyDescription gKeyDescription [] = 
{
	{ "backspace",				chrBackspace,				kKeyType_Writeable },	// 0x0008
	{ "tab",					chrHorizontalTabulation,	kKeyType_Writeable },	// 0x0009
	{ "new line",				chrLineFeed,				kKeyType_Writeable },	// 0x000A
	{ "page up",				vchrPageUp,					kKeyType_Button },		// 0x000B
	{ "page down",				vchrPageDown,				kKeyType_Button },		// 0x000C
	{ "carriage return",		chrCarriageReturn,			kKeyType_Writeable },	// 0x000D
	{ "escape",					chrEscape,					kKeyType_Writeable },	// 0x001B -
	{ "left arrow",				chrLeftArrow,				kKeyType_Button },		// 0x001C
	{ "right arrow",			chrRightArrow,				kKeyType_Button },		// 0x001D
	{ "up arrow",				chrUpArrow,					kKeyType_Button },		// 0x001E
	{ "down arrow",				chrDownArrow,				kKeyType_Button },		// 0x001F
//	{ " ",						chrSpace,					kKeyType_Writeable },	// 0x0020

	// Everything here (0x0021 to 0x007E) is printable.

	{ "<delete>",				chrDelete,					kKeyType_Writeable },	// 0x007F

	// extended characters from 0x0080 to 0x00FF

	// Remaining are either commands or wide characters.

	{ "low battery",			vchrLowBattery,				kKeyType_Event },	// 0x0101
	{ "next field",				vchrNextField,				kKeyType_Button },	// 0x0103
	{ "Menu",					vchrMenu,					kKeyType_Button },	// 0x0105
	{ "command",				vchrCommand,				kKeyType_Button },	// 0x0106
	{ "Home",					vchrLaunch,					kKeyType_Button },	// 0x0108
	{ "Keyboard",				vchrKeyboard,				kKeyType_Button },	// 0x0109
	{ "Find",					vchrFind,					kKeyType_Button },	// 0x010A
	{ "Calculator",				vchrCalc,					kKeyType_Button },	// 0x010B -
	{ "prev field",				vchrPrevField,				kKeyType_Button },	// 0x010C

	// Currently, nothing beyond here is generated by gremlins.

	{ "Keyboard \"abc\"",		vchrKeyboardAlpha,			kKeyType_Button },	// 0x0110
	{ "Keyboard \"123\"",		vchrKeyboardNumeric,		kKeyType_Button },	// 0x0111
	{ "lock",					vchrLock,					kKeyType_Button },	// 0x0112
	{ "Backlight",				vchrBacklight,				kKeyType_Button },	// 0x0113
	{ "auto off",				vchrAutoOff,				kKeyType_Event },	// 0x0114 - This one is currently disabled.

	// PalmOS 3.0

	{ "exgtest",				vchrExgTest,				kKeyType_Event },	// 0x0115
	{ "send data",				vchrSendData,				kKeyType_Event },	// 0x0116
	{ "ir receive",				vchrIrReceive,				kKeyType_Event },	// 0x0117

	// PalmOS 3.1

	{ "tsm1",					vchrTsm1,					kKeyType_Event },	// 0x0118
	{ "tsm2",					vchrTsm2,					kKeyType_Event },	// 0x0119
	{ "tsm3",					vchrTsm3,					kKeyType_Event },	// 0x011A
	{ "tsm4",					vchrTsm4,					kKeyType_Event },	// 0x011B

	// PalmOS 3.2

	{ "radio coverage OK",		vchrRadioCoverageOK,		kKeyType_Event },	// 0x011C
	{ "radio coverage fail",	vchrRadioCoverageFail,		kKeyType_Event },	// 0x011D
	{ "power off",				vchrPowerOff,				kKeyType_Event },	// 0x011E

	// PalmOS 3.5

	{ "resume sleep",			vchrResumeSleep,			kKeyType_Event },	// 0x011F

	{ "late wakeup",			vchrLateWakeup,				kKeyType_Event },	// 0x0120

	{ "tsm mode",				vchrTsmMode,				kKeyType_Event },	// 0x0121
	{ "Brightness",				vchrBrightness,				kKeyType_Button },	// 0x0122
	{ "Contrast",				vchrContrast,				kKeyType_Button },	// 0x0123
	{ "exg int data",			vchrExgIntData,				kKeyType_Event },	// 0x01FF

	{ "hard 1 (Date Book)",		vchrHard1,					kKeyType_Button },	// 0x0204
	{ "hard 2 (Address Book)",	vchrHard2,					kKeyType_Button },	// 0x0205
	{ "hard 3 (ToDo List)",		vchrHard3,					kKeyType_Button },	// 0x0206
	{ "hard 4 (Note Pad)",		vchrHard4,					kKeyType_Button },	// 0x0207
	{ "hard power",				vchrHardPower,				kKeyType_Button },	// 0x0208
	{ "hard cradle",			vchrHardCradle,				kKeyType_Button },	// 0x0209
	{ "hard cradle 2",			vchrHardCradle2,			kKeyType_Button },	// 0x020A
	{ "hard contrast",			vchrHardContrast,			kKeyType_Button },	// 0x020B
	{ "hard antenna",			vchrHardAntenna,			kKeyType_Event },	// 0x020C
	{ "hard brightness",		vchrHardBrightness,			kKeyType_Button },	// 0x020D

	{ NULL,						0,							(KeyType) 0 }
};


/***********************************************************************
 *
 * FUNCTION:	PrvFindKeyDescription
 *
 * DESCRIPTION: Search for the text description of a character code 
 *				recieved in event.data.keyDown.ascii.
 *
 * PARAMETERS:	chrcode - the character code value to search for
 *
 * RETURNED:	pointer to a KeyDescription structure that describes
 *				the character code, or null if no description found.
 *
 ***********************************************************************/

static KeyDescription* PrvFindKeyDescription (uint16 chrcode)
{
	// Iterate over the key description list and find an element with 
	// the same key code.

	for (KeyDescription* kd = gKeyDescription; kd->description != NULL; kd++) 
	{
		if (kd->chrcode == chrcode) 
			return kd;
	}

	// Failed to find description.

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	PrvIsKeyWriteable
 *
 * DESCRIPTION: Determine whether a key event is writeable or not. By
 *				'writeable' we mean that the key event can be reproduced
 *				by a user by typing or graffiti, as in a normal printable
 *				text character. This routine is used to determine whether
 *				multiple key events can be merged together into a single
 *				string.
 *
 * PARAMETERS:	kd - the key description structure for this event, if it exists
 *
 *				chrcode - the character code, ie from event.data.keyDown.ascii
 *
 *				modifiers - the modifiers for this event
 *
 * RETURNED:	True if the key event is writeable.
 *
 ***********************************************************************/

static Bool PrvIsKeyWriteable (KeyDescription* kd, uint16 chrcode, uint16 modifiers)
{
	// If commandKeyMask is set, can't be writeable.

	if (modifiers & commandKeyMask)
		return false;

	// Everything above 0x0100 without commandKeyMask set is a writeable wide character..?

	if (chrcode >= 0x100)
		return true;

	// Remaining values are below 0x100 without commandKeyMask set.
	// If the key has a description, use it to determine writeability.

	if (kd) 
	{
		return kd->type == kKeyType_Writeable;
	}

	// If the key's character code is printable, we'll assume it's writeable.

	if (isprint (chrcode))
		return true;

	// If the key's character code is in the extended ascii range, we'll assume it's 
	// writeable. Problem with this is that it's not easy to enter an extended character 
	// into a Palm device..

	if (IS_EXTENDED(chrcode))
		return true;

	// Anything else (meaning values below 0x0080 without commandKeyMask set and 
	// no key description specified) assumed to be unwriteable.

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::OutputKeyEventStep
 *
 * DESCRIPTION: Write out the description of a key event. This routine
 *				handles key events that look like button presses or special
 *				event notifications (like LowBattery), as well as writeable
 *				characters. Successive writeable characters are merged
 *				together into a single string if possible.
 *
 * PARAMETERS:	events - the entire event list
 *
 *				iter - an iterator over the events list, currently
 *					located at a key event
 *
 *				stream - the output stream to write into
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::OutputKeyEventStep (EmEventInfoList::iterator& eventInfoIter, strstream& stream)
{
	EmAssert (eventInfoIter->event.eType == kRecordedKeyEvent);

	stream << fgCounter << ". ";

	KeyDescription* kd = ::PrvFindKeyDescription (eventInfoIter->event.keyEvent.ascii);

	// If this key event is writeable, then build up a string 
	// of characters to write.

	if (::PrvIsKeyWriteable (kd, eventInfoIter->event.keyEvent.ascii, eventInfoIter->event.keyEvent.modifiers))
	{
		string charString;

		// Scan forward until we find an event that doesn't involve writing,
		// or we hit the end of the list.

		do
		{
			// Add character onto string: Use description if available, else just
			// use the character itself.

			if (kd)
			{
				charString += string ("<") + kd->description + string (">");
			}

			else if (IS_EXTENDED (eventInfoIter->event.keyEvent.ascii))
			{
				// Put brackets around extended character number

				char temp[16];
				sprintf (temp, "<%3d>", (int) eventInfoIter->event.keyEvent.ascii);
				charString += temp;
			}

			else
			{
				charString += eventInfoIter->event.keyEvent.ascii;
			}

			++eventInfoIter;

			// Stop if this event is not a key event.

			if (eventInfoIter->event.eType != kRecordedKeyEvent) 
				break;

			// Stop if this key event is not writeable.

			kd = ::PrvFindKeyDescription (eventInfoIter->event.keyEvent.ascii);

			if (!::PrvIsKeyWriteable (kd, eventInfoIter->event.keyEvent.ascii, eventInfoIter->event.keyEvent.modifiers))
				break;

		} while (eventInfoIter != fgEventInfo.end ());

		// Undigest the event that caused us to break.

		--eventInfoIter;

		// Output the string.

		stream << "Write \"" << charString << "\"." << endl;
	}

	else
	{
		if (kd)
		{
			// Check if it's a button.

			if (kd->type == kKeyType_Button)
			{
				stream << "Tap the " << kd->description << " button." << endl;
			}

			// Check if it's an event.

			else if (kd->type == kKeyType_Event)
			{
				stream
					<< "Application receives a "
					<< kd->description << " event." << endl;
			}
		}

		else
		{
			stream
				<< "Write character "
				<< eventInfoIter->event.keyEvent.ascii << "." << endl;
		}
	}

	fgCounter++;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	PrvGetRankString
 *
 * DESCRIPTION: Return a text string to describe a rank number.
 *
 * PARAMETERS:	rank - the rank number to describe
 *
 * RETURNED:	String containing the rank description.
 *
 ***********************************************************************/

static string PrvGetRankString (int rank)
{
	string rankString;

	// Special cases:

	if		(rank == kObjectRank_Last)	rankString = "last";
	else if	(rank == 0)					rankString = "first";
	else if (rank == 1)					rankString = "second";
	else if (rank == 2)					rankString = "third";

	else
	{
		// Just use 'nth'.

		char temp[16];
		sprintf (temp, "%d", rank);
		rankString = temp + string("th");
	}

	return rankString;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetObjectKindString
 *
 * DESCRIPTION: Return a string describing the type of a form object.
 *
 * PARAMETERS:	objKind - the type of the object
 *
 *				style - the style (for control objects)
 *
 * RETURNED:	A string describing the object's type.
 *
 ***********************************************************************/

static string PrvGetObjectKindString (FormObjectKind objKind, ControlStyleType style)
{
	string kindString;

	switch (objKind)
	{
		case frmFieldObj:
			kindString = "field";
			break;

		case frmControlObj:

			switch (style)
			{
				case buttonCtl:
					kindString = "button";
					break;

				case pushButtonCtl:
					kindString = "pushbutton";
					break;

				case checkboxCtl:
					kindString = "checkbox";
					break;

				case popupTriggerCtl:
					kindString = "popup trigger";
					break;

				case selectorTriggerCtl:
					kindString = "selector trigger";
					break;

				case repeatingButtonCtl:
					kindString = "repeating button";
					break;

				case sliderCtl:
					kindString = "slider";
					break;

				case feedbackSliderCtl:
					kindString = "feedback slider";
					break;

				default:
					kindString = "control";
			}

			break;

		case frmListObj:
			kindString = "list";
			break;

		case frmTableObj:
			kindString = "table";
			break;

		case frmBitmapObj:
			kindString = "bitmap";
			break;

//			case frmLineObj:
//			case frmFrameObj:
//			case frmRectangleObj:

		case frmLabelObj:
			kindString = "label";
			break;

		case frmTitleObj:
			kindString = "title";
			break;

		case frmPopupObj:
			kindString = "popup";
			break;

		case frmGraffitiStateObj:
			kindString = "graffiti shift indicator";
			break;

		case frmGadgetObj:
			kindString = "gadget";
			break;

		case frmScrollBarObj:
			kindString = "scrollbar";
			break;

		default:
			kindString = "(unknown object type)";
	}

	return kindString;
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::OutputPenEventStep
 *
 * DESCRIPTION: Write out the description of a pen event. 'Lift stylus'
 *				events are ignored for now. 
 *
 * PARAMETERS:	event - the pen event to describe
 *
 *				stream - the output stream to write into
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::OutputPenEventStep (const EmEventInfo& eventInfo, strstream& stream)
{
	EmAssert (eventInfo.event.eType == kRecordedPenEvent);

	PointType		pen			= eventInfo.event.penEvent.coords;
	string			text		= eventInfo.text;
	FormObjectKind	kind		= eventInfo.objKind;
	Int16			rank		= eventInfo.rank;

	if (::PrvIsPenUp (pen))
	{
		// Skip lift stylus events.
		// stream << "Lift stylus." << endl;
	}
	else
	{
		stream << fgCounter << ". ";

		// Tap on silkscreen

		KeyDescription* kd;
		if (eventInfo.asciiCode != 0 &&
			(kd = ::PrvFindKeyDescription (eventInfo.asciiCode)) != NULL)
		{
			// Check if it's a button.

			if (kd->type == kKeyType_Button)
			{
				stream << "Tap the " << kd->description << " button";
			}

			// Check if it's an event.

			else if (kd->type == kKeyType_Event)
			{
				stream << "Application receives a " << kd->description << " event";
			}
		}

		// Tap somewhere.

		else if (kind == -1)
		{
			// Just output location to tap at.

			stream << "Tap at " << pen.x << ", " << pen.y;
		}
		else
		{
			// Command.

			if (   (kind == frmFieldObj) 
				|| (kind == frmTitleObj) 
				|| (kind == frmTableObj))
			{
				stream << "Tap in the ";
			}
			else
			{
				stream << "Tap the ";
			}

			// Ranking.

			Bool needRankPostfix = false;

			if (rank == kObjectRank_None)
			{
				// Output control label or nothing.

				if (!text.empty () && (kind == frmControlObj))
				{
					stream << "\"" << text << "\" ";
				}
			}
			else
			{
				// Output ranking.

				stream << ::PrvGetRankString (rank) << " ";

				if (rank > 1)
					needRankPostfix = true;
			}

			// Object kind.
			// Skip this for the special 'nth list item' case.

			if (!(kind == frmListObj &&
				  rank == kObjectRank_None && 
				  text.empty() &&
				  eventInfo.row != -1))
			{
				stream
					<< ::PrvGetObjectKindString (kind, eventInfo.style);
			}

			// Remainder of ranking.

			if (needRankPostfix)
				stream << " from the top";

			// Special object data.

			switch (kind)
			{
				case frmFieldObj:

					if (!text.empty ())
					{
						// After the text 'blah'.

						stream << text;
					}
					else
					{
						// Shouldn't need this because if event.text is empty then
						// the field is empty so it doesn't matter where you tap.
						// stream << " at " << pen.x << ", " << pen.y;
					}

					break;

				case frmListObj:

					// Output item text if we have it, else try the item number.

					if (!text.empty ())
					{
						if (rank == kObjectRank_None)
						{
							// Special case for forms with only one list.

							stream << " item \"" << text << "\"";
						}
						else
						{
							stream << ", in the \"" << text << "\" item";
						}
					}
					else if (eventInfo.row != -1)
					{
						if (rank == kObjectRank_None)
						{
							// Special case for forms with only one list.

							stream << ::PrvGetRankString (eventInfo.row) << " list item";
						}
						else
						{
							stream << ", in the " << ::PrvGetRankString (eventInfo.row) << " item";
						}
					}
					else
					{
						stream << " at " << pen.x << ", " << pen.y;
					}

					break;

				case frmTableObj:

					// Output column, row if we have it, else use x, y pixel position.

					if (eventInfo.row != -1)
					{
						stream 
							<< " at column " << eventInfo.column
							<< " row " << eventInfo.row;
					}
					else 
					{
						stream << " at " << pen.x << ", " << pen.y;
					}

					break;

				default:
					break;	// Handle all other cases to keep gcc happy.
			}
		}

		// Form change.

		if (!eventInfo.newFormText.empty ())
		{
			// "To go to the 'A' dialog" or "To open the 'B' view".

			stream << " to " << eventInfo.newFormText;
		}

		stream << "." << endl;

		fgCounter++;
	}
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::OutputAppSwitchEventStep
 *
 * DESCRIPTION: Write out the description of an appSwitch event.
 *
 * PARAMETERS:	event - the appSwitch event to describe
 *
 *				stream - the output stream to write into
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::OutputAppSwitchEventStep (const EmEventInfo& eventInfo, strstream& stream)
{
	EmAssert (eventInfo.event.eType == kRecordedAppSwitchEvent);

	stream << fgCounter << ". ";

	uint16	cardNo		= eventInfo.event.appSwitchEvent.cardNo;
	uint32	dbID		= eventInfo.event.appSwitchEvent.dbID;
	uint16	oldCardNo	= eventInfo.event.appSwitchEvent.oldCardNo;
	uint32	oldDbID		= eventInfo.event.appSwitchEvent.oldDbID;

	char	name [dmDBNameLength]		= { 0 };
	char	oldName [dmDBNameLength]	= { 0 };
	uint16  version;
	uint16	oldVersion;

	#define GET_NAME(x, y) x, NULL, y, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL

	::DmDatabaseInfo (cardNo, dbID, GET_NAME(name, &version));
	::DmDatabaseInfo (oldCardNo, oldDbID, GET_NAME(oldName, &oldVersion));

	if (strcmp (name, oldName) != 0)
	{
		stream << "Switch to \"" << name << "\" from \"" << oldName << "\"." << endl;
	}
	else
	{
		stream << "Relaunch \"" << name << "\"." << endl;
	}

	fgCounter++;
}


/***********************************************************************
 *
 * FUNCTION:	EmEventOutput::OutputErrorEvent
 *
 * DESCRIPTION: Write out the description of an error event.
 *
 * PARAMETERS:	event - the error event to describe
 *
 *				stream - the output stream to write into
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmEventOutput::OutputErrorEvent (const EmEventInfo& eventInfo, strstream& stream)
{
	EmAssert (eventInfo.event.eType == kRecordedErrorEvent);

	// Output error description.  Do this a character at
	// a time in order to convert line endings.

	stream << endl;
	stream << "Crash with error:" << endl << endl;

	{
		string::const_iterator	iter = eventInfo.text.begin ();
		while (iter != eventInfo.text.end ())
		{
			if (*iter == '\r' || *iter == '\n')
			{
				stream << endl;
			}
			else
			{
				stream << *iter;
			}

			++iter;
		}

		stream << endl << endl;
	}

	// Output the stack crawl.

	stream << "Function call stack:" << endl;

	EmEventOutput::OutputStack (stream, eventInfo.stackCrawl);

	stream << endl;
}


void EmEventOutput::OutputStack (strstream& stream, const StringList& stackCrawl)
{
	StringList::const_iterator	iter = stackCrawl.begin ();

	while (iter != stackCrawl.end ())
	{
		stream << "\t" << *iter << endl;
		++iter;
	}
}
