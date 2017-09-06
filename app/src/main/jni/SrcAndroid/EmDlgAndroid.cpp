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

#include "ErrorHandling.h"		// ParamList
#include "Miscellaneous.h"		// ReplaceString, FormatString
#include "Platform.h"			// Platform::GetString
#include "Strings.r.h"			// kStr_
#include "EmSession.h"			// ResetEmulator... AndroidTODO get rid of

#include "EmDlgAndroid.h"
#include "PHEMNativeIF.h"

#include <algorithm>	// find
#include <list>
#include <map>
#include <string>
#include <utility>		// pair
#include <queue>

#if 0
typedef string			FilterList;

static FilterList		PrvConvertTypeList (const EmFileTypeList&);
static void				PrvAddFilter (FilterList& filter, const char* pattern);
static Fl_Window*		PrvMakeDialog (EmDlgID);
static Bool				PrvInitializeDialog (EmDlgFn, void* data, EmDlgID, Fl_Window*);
static void				PrvIdleCallback (void* data);
static string			PrvBreakLine (Fl_Input_*, const char*);
#endif


// -----------------------------------------------------------------------------
// helper routines for establishing z-order independent ids for widgets.

typedef map<EmDlgItemID, PHEM_Widget_Info *> ID2WidgetType;
typedef map<PHEM_Widget_Info *, EmDlgItemID> Widget2IDType;

ID2WidgetType gID2Widget;
Widget2IDType gWidget2ID;

PHEM_Widget_Info* PrvFindWidgetByID (EmDlgItemID id)
{
	ID2WidgetType::iterator iter = gID2Widget.find (id);
	if (iter == gID2Widget.end ())
		return NULL;

	return iter->second;
}

EmDlgItemID PrvFindIDByWidget (PHEM_Widget_Info* o)
{
	Widget2IDType::iterator iter = gWidget2ID.find (o);
	if (iter == gWidget2ID.end ())
		return kDlgItemNone;

	return iter->second;
}

void PrvSetWidgetID (PHEM_Widget_Info* o, EmDlgItemID id)
{
	gID2Widget[id] = o;
	gWidget2ID[o] = id;
}

void PrvClearWidgetID (EmDlgItemID id)
{
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (id);

	if (o != NULL)
	{
		gID2Widget.erase (id);
		gWidget2ID.erase (o);
	}
}

void PrvClearWidgetID (PHEM_Widget_Info* o)
{
	EmDlgItemID id = ::PrvFindIDByWidget (o);

	if (id != kDlgItemNone)
	{
		gID2Widget.erase (id);
		gWidget2ID.erase (o);
	}
}

void PrvClearAllWidgetIDs (void)
{
	gID2Widget.clear ();
	gWidget2ID.clear ();
}

#if 0
void PrvClearDlgWidgetIDs (Fl_Group* w)
{
	// Clear self.
	::PrvClearWidgetID (w);

	// Clear children.
	for (int ii = 0; ii < w->children (); ++ii)
	{
		Fl_Widget* child = w->child (ii);

		Fl_Group* g = dynamic_cast<Fl_Group*>(child);
		if (g)
		{
			::PrvClearDlgWidgetIDs (g);
		}
		else
		{
			::PrvClearWidgetID (child);
		}
	}
}


// Define data structures and routines for keeping track of
// strings that are associated with widgets.  We need to keep
// track of what strings have been assigned to which widgets
// because FLTK widgets don't *own* the labels assigned to
// them.  They expect the caller to own the storage.  However,
// that's not the model EmDlg uses; it assumes that the widgets
// own the storage (as is the case in PowerPlant and with
// Windows).
//
// So when we set the label of a widget, we keep ownership
// of the string by storing it in a private collection.  This
// collection is a "list" object.  I chose that one so that
// strings currently in the collection are undisturbed when
// new strings are added.  Widgets keep a pointer to the
// characters owned by the string object.  If that object
// moves around as the result of adding new string objects,
// it might invalidate the pointer owned by the widget.

typedef pair<Fl_Widget*, string>		WidgetString;
typedef list<WidgetString>				WidgetStringList;

WidgetStringList gWidgetStrings;

const char* PrvSetWidgetString (Fl_Widget* o, const string& s)
{
	// First see if there's already an entry for this widget.

	WidgetStringList::iterator iter = gWidgetStrings.end ();
	while (iter != gWidgetStrings.end ())
	{
		if (iter->first == o)
		{
			// Widget is already in the collection.
			// Replace the string it owns.

			iter->second = s;
			return iter->second.c_str ();
		}

		++iter;
	}

	// Widget is not in the collection, so add an entry for it.

	gWidgetStrings.push_back (WidgetString (o, s));
	return (--(gWidgetStrings.end()))->second.c_str ();
}

void PrvClearWidgetString (Fl_Widget* o)
{
	WidgetStringList::iterator iter = gWidgetStrings.end ();
	while (iter != gWidgetStrings.end ())
	{
		if (iter->first == o)
		{
			gWidgetStrings.erase (iter);
			return;
		}

		++iter;
	}
}

void PrvClearAllWidgetStrings (void)
{
	gWidgetStrings.clear ();
}

void PrvClearDlgWidgetStrings (Fl_Group* w)
{
	for (int ii = 0; ii < w->children (); ++ii)
	{
		Fl_Widget* child = w->child (ii);
		::PrvClearWidgetString (child);

		Fl_Group* g = dynamic_cast<Fl_Group*>(child);
		if (g)
		{
			::PrvClearDlgWidgetStrings (g);
		}
	}
}

typedef pair<EmDlgContext*, EmDlgItemID>	EmWidget;

typedef vector<EmWidget>	EmWidgetList;
EmWidgetList				gWidgetList;

typedef vector<Fl_Window*>	Fl_Window_List;
Fl_Window_List				gDialogList;


EmWidget PrvPopWidget (void)
{
	EmWidget result ((EmDlgContext*) NULL, kDlgItemNone);

	if (!gWidgetList.empty ())
	{
		result = gWidgetList.front (); // Note, this is "top()" on Windows, maybe others...
		gWidgetList.erase (gWidgetList.begin ());
	}

	return result;
}


void PrvPushWidget (EmDlgContext* context, EmDlgItemID itemID)
{
	EmWidget widget (context, itemID);
	gWidgetList.push_back (widget);
}


void PrvClearQueue (EmDlgContext* context)
{
	if (!context)
	{
		gWidgetList.clear ();
	}
	else
	{
		EmWidgetList::iterator	iter = gWidgetList.begin ();

		while (iter != gWidgetList.end ())
		{
			if (context == iter->first)
			{
				EmWidgetList::size_type delta = iter - gWidgetList.begin ();
				gWidgetList.erase (iter);
				iter = gWidgetList.begin () + delta;

				continue;
			}

			++iter;
		}
	}
}


EmDlgItemID PrvFindCancelItem (Fl_Group* w)
{
	EmDlgItemID result = kDlgItemNone;

	for (int ii = 0; result == kDlgItemNone && ii < w->children (); ++ii)
	{
		Fl_Widget* child = w->child (ii);

		Fl_Group* g = dynamic_cast<Fl_Group*>(child);
		Fl_Button* b = dynamic_cast<Fl_Button*>(child);
		if (g)
		{
			result = ::PrvFindCancelItem (g);
		}
		else if (b)
		{
			if (b->shortcut () == FL_Escape)
			{
				result = ::PrvFindIDByWidget (b);
			}
		}
	}

	return result;
}


void PrvWidgetCallback (Fl_Widget* w, void* c)
{
//	EmDlgItemID id = ::PrvFindIDByWidget (w);
//	printf ("PrvWidgetCallback: id = 0x%08lX\n", (long) id);

	EmDlgContext*	context = (EmDlgContext*) c;
	EmAssert (context);

	EmDlgItemID		itemID = ::PrvFindIDByWidget (w);
	EmAssert (itemID != kDlgItemNone);

	::PrvPushWidget (context, itemID);
}


void PrvWindowCallback (Fl_Widget* w, void* c)
{
	EmDlgContext* context = (EmDlgContext*) c;
	EmAssert (context);

	Fl_Window* dlg = (Fl_Window*) context->fDlg;
	EmAssert (dlg);

	// Look for the item with the Escape shortcut.
	EmDlgItemID itemID = ::PrvFindCancelItem (dlg);

	// If one couldn't be found, for the kDlgItemCancel item
	if (itemID == kDlgItemNone)
	{
		itemID = kDlgItemCancel;
	}

	::PrvPushWidget (context, itemID);
}


void PrvInstallCallback (Fl_Widget* o, void* data)
{
	// Install callbacks only for our widgets.  We shouldn't
	// mess up any other callbacks -- they may do something
	// important (such as the callback installed into scrollbars
	// in an Fl_Scroll pane).

	if (::PrvFindIDByWidget (o) != kDlgItemNone)
	{
		o->callback (&::PrvWidgetCallback, data);
	}

	// Set the callback for the topmost window, too, so that we
	// get notification when it's closed.

	else
	{
		Fl_Window* w = dynamic_cast<Fl_Window*> (o);

		if (w)
		{
			o->callback (&::PrvWindowCallback, data);
		}
	}
}


void PrvInstallCallbacks (Fl_Group* w, EmDlgContext* context)
{
	// Set self.
	::PrvInstallCallback (w, context);

	// Set children.
	for (int ii = 0; ii < w->children (); ++ii)
	{
		Fl_Widget* child = w->child (ii);

		Fl_Group* g = dynamic_cast<Fl_Group*>(child);
		if (g)
		{
			::PrvInstallCallbacks (g, context);
		}
		else
		{
			::PrvInstallCallback (child, context);
		}
	}
}


void PrvAddDialog (Fl_Window* dlg)
{
	gDialogList.push_back (dlg);
}


void PrvRemoveDialog (Fl_Window* dlg)
{
	Fl_Window_List::iterator iter = find (gDialogList.begin (), gDialogList.end (), dlg);
	EmAssert (iter != gDialogList.end ());
	gDialogList.erase (iter);
}


void PrvDestroyDialog (EmDlgContext* context)
{
	EmAssert (context);

	Fl_Window* dlg = reinterpret_cast<Fl_Window*>(context->fDlg);
	EmAssert (dlg);

	context->Destroy ();
	EmDlg::HostStopIdling (*context);

	dlg->hide ();

	::PrvRemoveDialog (dlg);
	::PrvClearDlgWidgetIDs (dlg);
	::PrvClearDlgWidgetStrings (dlg);
	::PrvClearQueue (context);

	delete dlg;
	delete context;
}


// Process all events on our dialog widgets.  If dlg is non-NULL,
// look for events that close that dialog.  If the dialog is closed,
// return true and fill in dismissingItemID with the ID of the item
// that closed the dialog.

Bool PrvHandleDialogEvents (Fl_Window* dlg, EmDlgItemID& dismissingItemID)
{
	// Loop, getting dialog items that have been manipulated in some way.

	EmWidget w = ::PrvPopWidget ();

	EmDlgContext*	context = w.first;
	EmDlgItemID		itemID = w.second;

	while (context != NULL && itemID != kDlgItemNone)
	{
		// Handle the event on the dialog item, checking to see if the
		// dialog should be closed.

		if (context->Event (itemID) == kDlgResultClose)
		{
			// The dialog is closing.

			Fl_Window* closingDlg = reinterpret_cast<Fl_Window*>(context->fDlg);

			// See if we're interested that this dialog closed.

			Bool interested = dlg && (closingDlg == dlg);

			// Destroy the dialog.

			::PrvDestroyDialog (context);

			// If we're interested that this dialog closed, record the
			// item that closed it and return TRUE.

			if (interested)
			{
				dismissingItemID = itemID;

				return true;
			}
		}

		w = ::PrvPopWidget ();
		context = w.first;
		itemID = w.second;
	}

	return false;
}

#pragma mark -

void HandleDialogs (void)
{
	EmDlgItemID	dummyItemID;
	(void) ::PrvHandleDialogEvents (NULL, dummyItemID);
}


void HandleModalDialog (Fl_Window* dlg, EmDlgItemID& itemID)
{
	while (1)
	{
		// Handle any queued events.

		if (::PrvHandleDialogEvents (dlg, itemID))
			break;

		// Process any system events.

		Fl::wait ();

		// Check to see if an idle handler has asked to close the window.

		EmDlgContext* context = (EmDlgContext*) dlg->user_data ();
		EmAssert (context);

		if (context->fFnResult == kDlgResultClose)
		{
			::PrvDestroyDialog (context);
			break;
		}
	}
}


void CloseAllDialogs (void)
{
	Fl_Window_List::iterator iter = gDialogList.begin ();

	while (iter != gDialogList.end ())
	{
		EmDlgContext* context = (EmDlgContext*) (*iter)->user_data ();
		EmAssert (context);

		::PrvDestroyDialog (context);
		++iter;
	}

	gDialogList.clear ();
}


// -----------------------------------------------------------------------------
// callback used to close modal dialogs. modalResult is < 0 if not yet completed.
// set modalResult to be >=0 to complete dialog. Typically 0=cancel, 1=ok

static long modalResult;
void modalCallback(Fl_Return_Button*, void* result)
{
	modalResult = (long) result;
}


// -----------------------------------------------------------------------------
// modal dialog control loop

static int postModalDialog (Fl_Window* dlg)
{
	modalResult = -9999;
	dlg->show();

	// run our own event loop so that we have total control
	// of the UI thread
	for (;;)
	{
		Fl::wait ();
		if (modalResult != -9999)
			break;

		if (!dlg->shown ())
		{
			// user hit escape (cancel)
			modalResult = 0;
			break;
		}
	}

	// Clear out the default item queue.  Stuff that we clicked on
	// that didn't have a callback function will be in here.  The
	// items in that queue will be invalid after we close the window
	// if we don't clear it out.

	while (Fl::readqueue ())
		;

	dlg->hide ();

	return modalResult;
}


// -----------------------------------------------------------------------------
// URL handler callback. This works whether netscape is currently running or
// not.
//!TODO: work with other browsers

void openURL (Fl_Button* box, void*)
{
	char buffer[PATH_MAX];
	char url[PATH_MAX];

	box->labelcolor (FL_RED);

	// get the label, trim the opening '<' and closing '>'
	strcpy (url, &(box->label()[1]));
	url [strlen (url) - 1] = '\0';

#ifdef __QNXNTO__
	sprintf (buffer, "voyager -u %s &", url);
#else
	sprintf (buffer, "netscape -remote 'openURL(%s,new-window)' || netscape '%s' &", url, url);
#endif

	system (buffer);
}

#endif

Bool PrvInitializeDialog (EmDlgFn fn, void* data, EmDlgID dlgID, void *dlg)
{
        EmDlgContext*           context = new EmDlgContext;
        if (!context)
        {
                //delete dlg;
                return false;
        }

        context->fFn            = fn;
//      context->fFnResult      = filled in after fFn is called;
        context->fDlg           = (EmDlgRef) dlg;
        context->fDlgID         = dlgID;
//      context->fPanelID       = filled in by subroutines;
//      context->fItemID        = filled in by subroutines;
//      context->fCommandID     = filled in by subroutines;
//      context->fNeedsIdle     = filled in by c'tor to false;
        context->fUserData      = data;

        // Initialize the dialog.  Delete the dialog if
        // initialization fails.
        if (context->Init() == kDlgResultClose) {
                return false;
        }

        return true;
}

/***********************************************************************
 *
 * FUNCTION:	EmDlg::HostRunGetFile
 *
 * DESCRIPTION:	Platform-specific routine for getting the name of a
 *				file from the user.  This file name is used to load
 *				the file.
 *
 * PARAMETERS:	typeList - types of files user is allowed to select.
 *				ref - selected file is returned here. Valid only if
 *					function result is kDlgItemOK.
 *
 * RETURNED:	ID of dialog item that dismissed the dialog.
 *
 ***********************************************************************/

EmDlgItemID EmDlg::HostRunGetFile (const void* parameters)
{
#if 0
	EmAssert (parameters);
	DoGetFileParameters&	data = *(DoGetFileParameters*) parameters;

	FilterList filter = ::PrvConvertTypeList (data.fFilterList);

	FileChooser chooser (data.fDefaultPath.IsSpecified () ? data.fDefaultPath.GetFullPath ().c_str () : NULL,
						 filter.c_str (), FileChooser::SINGLE, data.fPrompt.c_str ());

	chooser.show ();

	while (chooser.shown ())
		Fl::wait ();

	long count = chooser.count ();
	if (count == 0)
		return kDlgItemCancel;

	// Get a EmFileRef to the given file

	data.fResult = EmFileRef (chooser.value(1));

	return kDlgItemOK;
#else
     // Not used on Android
	return kDlgItemCancel;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::HostRunGetFileList
 *
 * DESCRIPTION:	Platform-specific routine for getting the name of a
 *				file from the user.  This file name is used to load
 *				the file.
 *
 * PARAMETERS:	typeList - types of files user is allowed to select.
 *				ref - selected file is returned here. Valid only if
 *					function result is kDlgItemOK.
 *
 * RETURNED:	ID of dialog item that dismissed the dialog.
 *
 ***********************************************************************/

EmDlgItemID EmDlg::HostRunGetFileList (const void* parameters)
{
#if 0
	EmAssert (parameters);
	DoGetFileListParameters&	data = *(DoGetFileListParameters*) parameters;

	FilterList filter = ::PrvConvertTypeList (data.fFilterList);

	FileChooser chooser (data.fDefaultPath.IsSpecified () ? data.fDefaultPath.GetFullPath ().c_str () : NULL,
						 filter.c_str (), FileChooser::MULTI, data.fPrompt.c_str ());

	chooser.show ();

	while (chooser.shown ())
		Fl::wait ();

	long count = chooser.count ();
	if (count == 0)
		return kDlgItemCancel;

	for (long ii = 1; ii <= count; ++ii)
	{
		data.fResults.push_back (EmFileRef (chooser.value (ii)));
	}

	return kDlgItemOK;
#else
     // Not used on Android
	return kDlgItemCancel;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::HostRunPutFile
 *
 * DESCRIPTION:	Platform-specific routine for getting the name of a
 *				file from the user.  This file name is used to save
 *				the file.
 *
 * PARAMETERS:	defName - default name for the file to be saved.
 *				ref - selected file is returned here. Valid only if
 *					function result is kDlgItemOK.
 *
 * RETURNED:	ID of dialog item that dismissed the dialog.
 *
 ***********************************************************************/

EmDlgItemID EmDlg::HostRunPutFile (const void* parameters)
{
#if 0
	EmAssert (parameters);
	DoPutFileParameters&	data = *(DoPutFileParameters*) parameters;

	FilterList filter = ::PrvConvertTypeList (data.fFilterList);

	FileChooser chooser (data.fDefaultPath.IsSpecified () ? data.fDefaultPath.GetFullPath ().c_str () : NULL,
						 filter.c_str (), FileChooser::CREATE, data.fPrompt.c_str ());

	chooser.show ();

	while (chooser.shown ())
		Fl::wait ();

	long count = chooser.count ();
	if (count == 0)
		return kDlgItemCancel;

	// Get a EmFileRef to the given file

	data.fResult = EmFileRef (chooser.value (1));

	return kDlgItemOK;
#else
     // Not used on Android
	return kDlgItemCancel;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::HostRunGetDirectory
 *
 * DESCRIPTION:	Platform-specific routine for getting the name of a
 *				file from the user.  This file name is used to load
 *				the file.
 *
 * PARAMETERS:	typeList - types of files user is allowed to select.
 *				ref - selected file is returned here. Valid only if
 *					function result is kDlgItemOK.
 *
 * RETURNED:	ID of dialog item that dismissed the dialog.
 *
 ***********************************************************************/

EmDlgItemID EmDlg::HostRunGetDirectory (const void* parameters)
{
#if 0
	EmAssert (parameters);
	DoGetDirectoryParameters&	data = *(DoGetDirectoryParameters*) parameters;

	FileChooser chooser (data.fDefaultPath.IsSpecified () ? data.fDefaultPath.GetFullPath ().c_str () : NULL,
						 "nEveRmAtCh*", FileChooser::DIRECTORY, data.fPrompt.c_str ());

	chooser.show ();

	while (chooser.shown ())
		Fl::wait ();

	long count = chooser.count ();
	if (count == 0)
		return kDlgItemCancel;

	// Get a EmFileRef to the given file

	data.fResult = EmDirRef (chooser.value (1));

	return kDlgItemOK;
#else
     // Not used on Android
	return kDlgItemCancel;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	HostRunSessionSave
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgItemID	EmDlg::HostRunSessionSave (const void* parameters)
{
//AndroidTODO ?
#if 0
	EmAssert (parameters);
	DoSessionSaveParameters&	data = *(DoSessionSaveParameters*) parameters;

	string saveChanges (Platform::GetString (kStr_SaveBeforeClosing));

	ParamList paramList;
	paramList.push_back (string ("%AppName"));
	paramList.push_back (data.fAppName);
	paramList.push_back (string ("%DocName"));
	paramList.push_back (data.fDocName);

	string msg = Errors::ReplaceParameters (saveChanges, paramList);

	int result = fl_choice (msg.c_str(),
							Platform::GetString (kStr_Cancel).c_str(),
							Platform::GetString (kStr_Yes).c_str(),
							Platform::GetString (kStr_No).c_str() );

	if (result == 1) // yes
		return kDlgItemYes;

	else if (result == 2) // no
		return kDlgItemNo;

	// result == 0
	return kDlgItemCancel;
#else
     // Not used on Android
	return kDlgItemCancel;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	HostRunAboutBox
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgItemID	EmDlg::HostRunAboutBox (const void* parameters)
{
#if 0
	Fl_Window* aboutWin = ::PrvMakeDialog (kDlgAboutBox);
	postModalDialog (aboutWin);
	delete aboutWin;

	return kDlgItemOK;
#else
     // Not used on Android
	return kDlgItemOK;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	HostRunDialog
 *
 * DESCRIPTION:	Common routine that handles the creation of a dialog,
 *				initializes it (via the dialog handler), fetches events,
 *				handles events (via the dialog handler), and closes
 *				the dialog.
 *
 * PARAMETERS:	fn - the custom dialog handler
 *				userData - custom data passed back to the dialog handler.
 *				dlgID - ID of dialog to create.
 *
 * RETURNED:	ID of dialog item that dismissed the dialog.
 *
 ***********************************************************************/

void PrvInitPHEMWidget(PHEM_Widget_Info *widget, EmDlgItemID item_id, char *text) {
  PHEM_Log_Msg("Initializing widget:");
  PHEM_Log_Place(item_id);
  PHEM_Log_Msg(text);

  // default vals
  widget->enabled = 1;
  widget->visible = 1;
  widget->value = 0;

  // supplied vals
  widget->item_id = item_id;
  widget->label = text;

  // add to map
  PrvSetWidgetID (widget, item_id);
}

PHEM_Dialog *PrvMakeCommonDialog() {
  PHEM_Dialog *dlg = new PHEM_Dialog;

  if (NULL == dlg) {
    PHEM_Log_Msg("Unable to allocate dialog!");
    return NULL;
  } else {
    PHEM_Log_Msg("Setting up common dialog.");
  }
  int i;
  for (i=0; i<4; i++) {
    dlg->widgets[i].item_id = kDlgItemNone;

    dlg->widgets[i].enabled = 0;
    dlg->widgets[i].visible = 0;
    dlg->widgets[i].is_default = 0;
    dlg->widgets[i].is_cancel = 0;
  }

  PrvInitPHEMWidget(&(dlg->widgets[0]), kDlgItemCmnButton1, "");
  PrvInitPHEMWidget(&(dlg->widgets[1]), kDlgItemCmnButton2, "");
  PrvInitPHEMWidget(&(dlg->widgets[2]), kDlgItemCmnButton3, "");
  PrvInitPHEMWidget(&(dlg->widgets[3]), kDlgItemCmnText, "");
  //PrvInitPHEMWidget(&(dlg->widgets[4]), kDlgItemNone, "");

  return dlg;
}

PHEM_Dialog *PrvMakeResetDialog() {
  PHEM_Dialog *dlg = new PHEM_Dialog;

  PHEM_Log_Msg("Make Reset dialog.");
  if (NULL == dlg) {
    PHEM_Log_Msg("Unable to allocate dialog!");
    return NULL;
  } else {
    PHEM_Log_Msg("Setting up reset dialog.");
  }
  int i;
  for (i=0; i<3; i++) {
    dlg->widgets[i].item_id = kDlgItemNone;

    dlg->widgets[i].enabled = 0;
    dlg->widgets[i].visible = 0;
    dlg->widgets[i].is_default = 0;
    dlg->widgets[i].is_cancel = 0;
    dlg->widgets[i].value = 0;
    dlg->widgets[i].label = "";
  }

  PrvInitPHEMWidget(&(dlg->widgets[0]), kDlgItemRstSoft, "Soft Reset");
  PrvInitPHEMWidget(&(dlg->widgets[1]), kDlgItemRstHard, "Hard Reset");
  PrvInitPHEMWidget(&(dlg->widgets[2]), kDlgItemRstNoExt, "No-Extension Reset");

  return dlg;
}

struct EditCommonDialogData
{
        const char*                     fMessage;
        EmCommonDialogFlags     fFlags;
        uint32                          fFirstBeepTime;
        uint32                          fLastBeepTime;
};

EmDlgItemID EmDlg::HostRunDialog (const void* parameters)
{
	EmAssert (parameters);
	RunDialogParameters&	data = *(RunDialogParameters*) parameters;

        static unsigned int invocations = 0;

	EmDlgItemID	itemID = kDlgItemNone;
        PHEM_Dialog *dlg;

        switch (data.fDlgID) {
        case kDlgCommonDialog:
          // Create, initialize dialog
          dlg = PrvMakeCommonDialog();
	  if (::PrvInitializeDialog(data.fFn, data.fUserData, data.fDlgID, (void *)dlg)) {
            // Run it.
            {
              EditCommonDialogData&   userd = *(EditCommonDialogData*) data.fUserData;
              PHEM_Log_Msg(userd.fMessage);
              PHEM_Log_Hex(userd.fFlags);
            }
            itemID = PHEM_Do_Common_Dialog(dlg);
          } else {
            delete dlg;
          }
          break;
        case kDlgReset:
          // Create, initialize dialog
          dlg = PrvMakeResetDialog();
	  if (::PrvInitializeDialog(data.fFn, data.fUserData, data.fDlgID, (void *)dlg)) {
            // Run it.
            PHEM_Log_Msg("Calling phem_do_reset_dialog.");
            itemID = PHEM_Do_Reset_Dialog(dlg);
            EmDlg::SetItemValue((EmDlgRef)dlg, itemID, (long)1);

            // Rest of reset code expects 'kDlgItemOK'...
            itemID = kDlgItemOK;
          } else {
            PHEM_Log_Msg("Reset dialog initialization failure!");
            delete dlg;
          }
          break;
        default:
          PHEM_Log_Msg("Unimplemented dialog:");
          PHEM_Log_Place(data.fDlgID);
          // not implemented
          // AndroidTODO: this is unsatisfactory.
          break;
        }

        // Return the result.
        return itemID;
#if 0
	// Create the dialog.
	Fl_Window* dlg = ::PrvMakeDialog (data.fDlgID);
	if (!dlg)
		return kDlgItemNone;

	// Initialize the dialog.
	if (!::PrvInitializeDialog (data.fFn, data.fUserData, data.fDlgID, dlg))
		return kDlgItemNone;

	// Handle the dialog.
	EmDlgItemID	itemID;

	::HandleModalDialog (dlg, itemID);

	// Return the item that dismissed the dialog.
	return itemID;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	HostDialogOpen
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

EmDlgRef EmDlg::HostDialogOpen (EmDlgFn fn, void* data, EmDlgID dlgID)
{
     PHEM_Log_Msg("HostDialogOpen");
#if 0
	// Create the dialog.
	Fl_Window* dlg = ::PrvMakeDialog (dlgID);
	if (!dlg)
		return (EmDlgRef) NULL;

	// Initialize the dialog.
	if (!::PrvInitializeDialog (fn, data, dlgID, dlg))
		return (EmDlgRef) NULL;

	// Return the dialog.
	return (EmDlgRef) dlg;
#else
     // Not used on Android
	return (EmDlgRef) NULL;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	HostDialogClose
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

void EmDlg::HostDialogClose (EmDlgRef dlg)
{
     PHEM_Log_Msg("HostDialogClose");
#if 0
	Fl_Window* o = reinterpret_cast<Fl_Window*>(dlg);
	if (!o)
		return;

	EmDlgContext* context = (EmDlgContext*) o->user_data ();
	EmAssert (context);

	::PrvDestroyDialog (context);
#endif
}


/***********************************************************************
 *
 * FUNCTION:	HostStartIdling
 *
 * DESCRIPTION:	Queue up our idle callback function with FLTK.
 *
 * PARAMETERS:	context - context data to be provided to the callback
 *					function.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmDlg::HostStartIdling (EmDlgContext& context)
{
     PHEM_Log_Msg("HostStartIdling");
//AndroidTODO ?
#if 0
	Fl::add_timeout (.01, &::PrvIdleCallback, &context);
#endif
}


/***********************************************************************
 *
 * FUNCTION:	HostStopIdling
 *
 * DESCRIPTION:	Remove our idle callback function from FLTK.
 *
 * PARAMETERS:	context - context data to be provided to the callback
 *					function.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmDlg::HostStopIdling (EmDlgContext& context)
{
     PHEM_Log_Msg("HostStopIdling");
//AndroidTODO ?
#if 0
	Fl::remove_timeout (&::PrvIdleCallback, &context);
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SetDlgBounds
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SetDlgBounds (EmDlgRef dlg, const EmRect& bounds)
{
#if 0
	Fl_Window* o = reinterpret_cast<Fl_Window*>(dlg);
	if (!o)
		return;

	EmRect	oldBounds = EmDlg::GetDlgBounds (dlg);

	if (oldBounds != bounds)
	{
		o->resize (bounds.fLeft, bounds.fTop,
			bounds.Width (), bounds.Height ());
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::CenterDlg
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::CenterDlg (EmDlgRef dlg)
{
#if 0
	Fl_Window* o = reinterpret_cast<Fl_Window*>(dlg);
	if (!o)
		return;

	o->position (
		(Fl::w () - o->w ()) / 2,
		(Fl::h () - o->h ()) / 3);
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::GetDlgBounds
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmRect EmDlg::GetDlgBounds (EmDlgRef dlg)
{
	EmRect	result (0, 0, 0, 0);
#if 0

	Fl_Window* o = reinterpret_cast<Fl_Window*>(dlg);
	if (!o)
		return result;

	result.Set (o->x(), o->y(), o->x() + o->w(), o->y() + o->h());

	return result;
#else
    //Not used on Android
	return result;
#endif
}


void EmDlg::SetDlgDefaultButton (EmDlgContext& context, EmDlgItemID item)
{
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
	if (!o) {
	  return;
        }

	o->is_default = 1;
}


void EmDlg::SetDlgCancelButton (EmDlgContext& context, EmDlgItemID item)
{
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
	if (!o) {
	  return;
        }

	o->is_cancel = 1;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SetItemMin
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SetItemMin (EmDlgRef dlg, EmDlgItemID item, long minValue)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Valuator* valuator = dynamic_cast<Fl_Valuator*> (o);
	if (valuator)
		valuator->minimum (minValue);
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SetItemValue
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SetItemValue (EmDlgRef dlg, EmDlgItemID item, long value)
{
        PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
        if (!o) {
          return;
        }

        o->value = value;
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	// ...............................................................
	// Includes: Fl_Button, Fl_Light_Button, Fl_Check_Button,
	// Fl_Radio_Light_Button, Fl_Round_Button, Fl_Radio_Round_Button,
	// Fl_Repeat_Button, Fl_Radio_Buttion, Fl_Toggle_Button
	// ...............................................................

	Fl_Button* button = dynamic_cast<Fl_Button*> (o);
	if (button)
		button->value (value);

	// ...............................................................
	// Includes: Fl_Choice, Fl_Menu_Bar, Fl_Menu_Button
	// ...............................................................

	Fl_Menu_* menu = dynamic_cast<Fl_Menu_*> (o);
	if (menu)
	{
		// Prevent out-of-range values.

		if (!menu->menu ())
			return;

		if (value > menu->size () - 1)
			value = menu->size () - 1;
		else if (value < 0)
			value = 0;

		// Adjust for the fact that FLTK treats divider lines as
		// an attribute of the preceding menu item, while our model
		// is to treat them as seperate menu items.

		for (int ii = 0; ii < value; ++ii)
		{
			if (menu->menu ()[ii].flags & FL_MENU_DIVIDER)
			{
				--value;
			}
		}

		menu->value (value);
		menu->redraw ();
	}

	// ...............................................................
	// Includes: Fl_Browser, Fl_Hold_Browser, Fl_Multi_Browser,
	// Fl_Select_Browser (but not Fl_Browser_)
	// ...............................................................

	Fl_Browser* browser = dynamic_cast<Fl_Browser*> (o);
	if (browser)
		browser->value (value);

	// ...............................................................
	// Includes: Fl_Adjuster, Fl_Counter, Fl_Dial, Fl_Roller,
	// Fl_Scrollber, Fl_Slider, Fl_Value_Input, Fl_Value_Slider,
	// Fl_Value_Output
	// ...............................................................

	Fl_Valuator* valuator = dynamic_cast<Fl_Valuator*> (o);
	if (valuator)
		valuator->value (value);

	// ...............................................................
	// Includes: Fl_Input_, Fl_Input, Fl_Float_Input, Fl_Int_Input,
	// Fl_Multiline_Input, Fl_Secret_Input, Fl_Output and
	// Fl_Multiline_Output
	// ...............................................................

	Fl_Input_* input = dynamic_cast<Fl_Input_*> (o);
	if (input)
	{
		char buffer[20];
		sprintf (buffer, "%ld", (long) value);
		EmDlg::SetItemText (dlg, item, buffer);
	}

	// ...............................................................
	// Includes: Fl_Box
	// ...............................................................

	Fl_Box* box = dynamic_cast<Fl_Box*> (o);
	if (box)
	{
		char	buffer[20];
		::FormatInteger (buffer, value);
		EmDlg::SetItemText (dlg, item, buffer);
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SetItemMax
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SetItemMax (EmDlgRef dlg, EmDlgItemID item, long maxValue)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	// ...............................................................
	// Includes: Fl_Adjuster, Fl_Counter, Fl_Dial, Fl_Roller,
	// Fl_Scrollber, Fl_Slider, Fl_Value_Input, Fl_Value_Slider,
	// Fl_Value_Output
	// ...............................................................

	Fl_Valuator* valuator = dynamic_cast<Fl_Valuator*> (o);
	if (valuator)
		valuator->maximum (maxValue);
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SetItemBounds
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SetItemBounds (EmDlgRef dlg, EmDlgItemID item, const EmRect& bounds)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	EmRect	oldBounds = EmDlg::GetItemBounds (dlg, item);

	if (oldBounds != bounds)
	{
		o->resize (bounds.fLeft, bounds.fTop,
			bounds.Width (), bounds.Height ());
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SetItemText
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

char *PrvCopyStringContents(string str) {
   int label_len = str.length();
   char *buffer = (char *)calloc(sizeof(char), label_len+1);
   strncpy(buffer, str.c_str(), label_len);
   return buffer;
}

void EmDlg::SetItemText (EmDlgRef dlg, EmDlgItemID item, string str)
{
        //PHEM_Log_Msg("SetItemText");
        //PHEM_Log_Place(item);
        //PHEM_Log_Msg(str.c_str());
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
	if (!o) {
          PHEM_Log_Msg("SetItemText, couldn't find widget!");
	  return;
        }

        /*if (o->label) {
          PHEM_Log_Msg("Existing label:");
          PHEM_Log_Msg(o->label);
        }*/
	o->label = PrvCopyStringContents(str);
        /*if (o->label) {
          PHEM_Log_Msg("Label set to:");
          PHEM_Log_Msg(o->label);
        }*/
#if 0
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

		// Setting the text of other items is tricky.  That would be
		// done by calling Fl_Widget::label.  However, that function
		// does not copy the text -- just the text pointer.  That
		// means the string object (or whatever is holding the storage)
		// must exist after this function is called.

		// Additionally, it appears that we have to hide and then show
		// the widget in order to get it to update properly.  Merely
		// calling redraw will not erase the old text in boxes like
		// the file counter in the "Import Database" dialog.

		const char* s = ::PrvSetWidgetString (o, str);

		if (o->visible_r ())
		{
			o->hide ();
			o->label (s);
			o->show ();
		}
		else
		{
			o->label (s);
		}
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::GetItemValue
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

long EmDlg::GetItemValue (EmDlgRef dlg, EmDlgItemID item)
{
  //PHEM_Log_Msg("GetItemValue");
  //PHEM_Log_Place(item);

  PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
  if (!o) {
    return -1;
  }

  return o->value;
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return 0;

	// ...............................................................
	// Includes: Fl_Button, Fl_Light_Button, Fl_Check_Button,
	// Fl_Radio_Light_Button, Fl_Round_Button, Fl_Radio_Round_Button,
	// Fl_Repeat_Button, Fl_Radio_Buttion, Fl_Toggle_Button
	// ...............................................................

	Fl_Button* button = dynamic_cast<Fl_Button*> (o);
	if (button)
		return button->value ();

	// ...............................................................
	// Includes: Fl_Choice, Fl_Menu_Bar, Fl_Menu_Button
	// ...............................................................

	Fl_Menu_* menu = dynamic_cast<Fl_Menu_*> (o);
	if (menu)
	{
		if (menu->mvalue () == NULL)
			return -1;

		long	result = menu->value();
		long	add = 0;

		// Adjust for the fact that FLTK treats divider lines as
		// an attribute of the preceding menu item, while our model
		// is to treat them as seperate menu items.

		for (int ii = 0; ii < result; ++ii)
		{
			if (menu->menu()[ii].flags & FL_MENU_DIVIDER)
			{
				add++;
			}
		}

		return result + add;
	}

	// ...............................................................
	// Includes: Fl_Browser (but not Fl_Browser_)
	// ...............................................................

	Fl_Browser* browser = dynamic_cast<Fl_Browser*> (o);
	if (browser)
		return browser->value ();

	// ...............................................................
	// Includes: Fl_Adjuster, Fl_Counter, Fl_Dial, Fl_Roller,
	// Fl_Scrollber, Fl_Slider, Fl_Value_Input, Fl_Value_Slider,
	// Fl_Value_Output
	// ...............................................................

	Fl_Valuator* valuator = dynamic_cast<Fl_Valuator*> (o);
	if (valuator)
		return (long) valuator->value ();		// !!! Really is a "double"!

	// ...............................................................
	// Includes: Fl_Input_, Fl_Input, Fl_Float_Input, Fl_Int_Input,
	// Fl_Multiline_Input, Fl_Secret_Input, Fl_Output and
	// Fl_Multiline_Output
	// ...............................................................

	Fl_Input_* input = dynamic_cast<Fl_Input_*> (o);
	if (input)
	{
		const char* text = input->value ();
		long value = 0;
		sscanf (text, "%ld", &value);
		return value;
	}

	return 0;
#else
     // Not used on Android
	return 0;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::GetItemBounds
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmRect EmDlg::GetItemBounds (EmDlgRef dlg, EmDlgItemID item)
{
	EmRect	result (0, 0, 0, 0);
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return result;

	result.Set (o->x(), o->y(), o->x() + o->w(), o->y() + o->h());

	return result;
#else
    //Not used on Android
	return result;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::GetItemText
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

string EmDlg::GetItemText (EmDlgRef dlg, EmDlgItemID item)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return string ();

	Fl_Input_* in = dynamic_cast<Fl_Input_*> (o);
	if (in)
		return string (in->value ());

	return string (o->label ());
#else
   // Not used on Android
   return string ();
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::EnableItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::EnableItem (EmDlgRef dlg, EmDlgItemID item)
{
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
	if (!o) {
	  return;
        }

	o->enabled = 1;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::DisableItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::DisableItem (EmDlgRef dlg, EmDlgItemID item)
{
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
	if (!o) {
	  return;
        }

	o->enabled = 0;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::ShowItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::ShowItem (EmDlgRef dlg, EmDlgItemID item)
{
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
	if (!o) {
	  return;
        }

	o->visible = 1;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::HideItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::HideItem (EmDlgRef dlg, EmDlgItemID item)
{
	PHEM_Widget_Info* o = ::PrvFindWidgetByID (item);
	if (!o) {
	  return;
        }

	o->visible = 0;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::ClearMenu
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::ClearMenu (EmDlgRef dlg, EmDlgItemID item)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Menu_* menu = dynamic_cast<Fl_Menu_*> (o);
	EmAssert (menu);

	menu->clear ();
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::AppendToMenu
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::AppendToMenu (EmDlgRef dlg, EmDlgItemID item, const StringList& strList)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Menu_* menu = dynamic_cast<Fl_Menu_*> (o);
	EmAssert (menu);

	StringList::const_iterator iter = strList.begin ();
	while (iter != strList.end ())
	{
		if (iter->empty ())
		{
			menu->mode (menu->size () - 2, FL_MENU_DIVIDER);
		}
		else
		{
			menu->add (iter->c_str (), 0, NULL, NULL, 0);
		}

		++iter;
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::ClearList
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::ClearList (EmDlgRef dlg, EmDlgItemID item)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Browser* b = dynamic_cast<Fl_Browser*> (o);
	EmAssert (b);

	b->clear ();
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::EnableMenuItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::EnableMenuItem (EmDlgRef dlg, EmDlgItemID item, long menuItem)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Menu_* menu = dynamic_cast<Fl_Menu_*> (o);
	EmAssert (menu);
	EmAssert (menu->menu ());
	EmAssert (menuItem >= 0);
	EmAssert (menuItem < menu->size ());

	// Adjust for the fact that FLTK treats divider lines as
	// an attribute of the preceding menu item, while our model
	// is to treat them as seperate menu items.

	for (int ii = 0; ii < menuItem; ++ii)
	{
		if (menu->menu () [ii].flags & FL_MENU_DIVIDER)
		{
			--menuItem;
		}
	}

	/* DOLATER - kja: This won't work properly if we try to change a divider */
	menu->mode (menuItem, 0);
	menu->redraw ();
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::DisableMenuItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::DisableMenuItem (EmDlgRef dlg, EmDlgItemID item, long menuItem)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Menu_* menu = dynamic_cast<Fl_Menu_*> (o);
	EmAssert (menu);
	EmAssert (menu->menu ());
	EmAssert (menuItem >= 0);
	EmAssert (menuItem < menu->value ());

	// Adjust for the fact that FLTK treats divider lines as
	// an attribute of the preceding menu item, while our model
	// is to treat them as seperate menu items.

	for (int ii = 0; ii < menuItem; ++ii)
	{
		if (menu->menu () [ii].flags & FL_MENU_DIVIDER)
		{
			--menuItem;
		}
	}

	/* DOLATER - kja: This won't work properly if we try to change a divider */
	menu->mode (menuItem, FL_MENU_INACTIVE);
	menu->redraw ();
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::AppendToList
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::AppendToList (EmDlgRef dlg, EmDlgItemID item, const StringList& strList)
{
//AndroidTODO ?
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Browser* b = dynamic_cast<Fl_Browser*> (o);
	EmAssert (b);

	StringList::const_iterator iter = strList.begin ();
	while (iter != strList.end ())
	{
		b->add (iter->c_str (), NULL);
		++iter;
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SelectListItems
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SelectListItems (EmDlgRef dlg, EmDlgItemID item, const EmDlgItemIndexList& itemList)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Browser* b = dynamic_cast<Fl_Browser*> (o);
	EmAssert (b);

	EmDlgItemIndexList::const_iterator iter = itemList.begin ();
	while (iter != itemList.end ())
	{
		b->select (*iter + 1, 1);
		++iter;
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::UnselectListItems
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::UnselectListItems (EmDlgRef dlg, EmDlgItemID item, const EmDlgListIndexList& itemList)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Browser* b = dynamic_cast<Fl_Browser*> (o);
	EmAssert (b);

	EmDlgItemIndexList::const_iterator iter = itemList.begin ();
	while (iter != itemList.end ())
	{
		b->select (*iter + 1, 0);
		++iter;
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::GetSelectedItems
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::GetSelectedItems (EmDlgRef dlg, EmDlgItemID item, EmDlgItemIndexList& itemList)
{
#if 0
	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return;

	Fl_Browser* b = dynamic_cast<Fl_Browser*> (o);
	EmAssert (b);

	for (int ii = 1; ii <= b->size (); ++ii)
	{
		if (b->selected (ii))
		{
			itemList.push_back (ii - 1);
		}
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::GetTextHeight
 *
 * DESCRIPTION:	Determine the height the text would be if fitted into
 *				the given item.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	New text height
 *
 ***********************************************************************/

int EmDlg::GetTextHeight (EmDlgRef dlg, EmDlgItemID item, const string& s)
{
	int result = 0;
#if 0

	Fl_Widget* o = ::PrvFindWidgetByID (item);
	if (!o)
		return result;

	EmRect	bounds = EmDlg::GetItemBounds (dlg, item);
	int		width = bounds.Width ();

	Fl_Input_* i = dynamic_cast<Fl_Input_*>(o);

	if (i)
	{
		// Note that Fl_Output::draw insets the width of the widget
		// area when drawing.  We need to account for that here if we want
		// our text to be measured the same way it's laid out.

		Fl_Boxtype b = i->box () ? i->box () : FL_DOWN_BOX;
		width -= Fl::box_dw (b) + 6;

		// FIXME: The Fl_Label structure is not documented, and its layout
		// has changed in the past.  These two measurements should be
		// rewritten to use fl_measure or perhaps o->measure_label somehow.

		Fl_Label label =
		{
			s.c_str (),
#ifndef HAVE_LEGACY_FL_LABEL
			NULL,
			NULL,
#endif
			FL_NORMAL_LABEL,
			i->textfont (),
			i->textsize (),
			i->textcolor ()
		};

		label.measure (width, result);

		result += Fl::box_dh (b);
	}
	else
	{
		// Note that Fl_Widget::draw_label insets the width of the widget
		// area when drawing.  We need to account for that here if we want
		// our text to be measured the same way it's laid out.

		width -= Fl::box_dw (o->box ());

		if ((o->w () > 11) && (o->align () & (FL_ALIGN_LEFT | FL_ALIGN_RIGHT)))
		{
			width -= 6;
		}

		Fl_Label label =
		{
			s.c_str (),
#ifndef HAVE_LEGACY_FL_LABEL
			NULL,
			NULL,
#endif
			o->labeltype (),
			o->labelfont (),
			o->labelsize (),
			o->labelcolor ()
		};

		label.measure (width, result);
	}

	return result;
#else
     PHEM_Log_Msg("GetTextHeight");
   // Not used on Android
	return result;
#endif
}

#if 0

/***********************************************************************
 *
 * FUNCTION:	PrvConvertTypeList
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

FilterList PrvConvertTypeList (const EmFileTypeList& typeList)
{
	FilterList	filter;

	filter += "{";

	EmFileTypeList::const_iterator	iter = typeList.begin ();

	while (iter != typeList.end ())
	{
		switch (*iter)
		{
			case kFileTypeNone:
				break;

			case kFileTypeApplication:
				break;

			case kFileTypeROM:
				::PrvAddFilter (filter, "*.[Rr][Oo][Mm]");
				break;

			case kFileTypeSession:
				::PrvAddFilter (filter, "*.[Pp][Ss][Ff]");
				break;

			case kFileTypeEvents:
				::PrvAddFilter (filter, "*.[Pp][Ee][Vv]");
				break;

			case kFileTypePreference:
				break;

			case kFileTypePalmApp:
				::PrvAddFilter (filter, "*.[Pp][Rr][Cc]");
				break;

			case kFileTypePalmDB:
				::PrvAddFilter (filter, "*.[Pp][Dd][Bb]");
				break;

			case kFileTypePalmQA:
				::PrvAddFilter (filter, "*.[Pp][Qq][Aa]");
				break;

			case kFileTypeText:
				::PrvAddFilter (filter, "*.[Tt][Xx][Tt]");
				break;

			case kFileTypePicture:
				::PrvAddFilter (filter, "*.[Pp][Pp][Mm]");
				break;

			case kFileTypeSkin:
				::PrvAddFilter (filter, "*.[Ss][Kk][Ii][Nn]");
				break;

			case kFileTypeProfile:
				break;

			case kFileTypePalmAll:
				::PrvAddFilter (filter, "*.[Pp][Rr][Cc]|*.[Pp][Dd][Bb]|*.[Pp][Qq][Aa]");
				break;

			case kFileTypeAll:
				::PrvAddFilter (filter, "*");
				break;

			case kFileTypeLast:
				EmAssert (false);
				break;
		}

		++iter;
	}

	filter += "}";

	return filter;
}


/***********************************************************************
 *
 * FUNCTION:	PrvAddFilter
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void PrvAddFilter (FilterList& filter, const char* pattern)
{
	if (filter.size() != 1)
	{
		filter += "|";
	}

	filter += pattern;
}


/***********************************************************************
 *
 * FUNCTION:	PrvMakeDialog
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Fl_Window* PrvMakeDialog (EmDlgID id)
{
	Fl_Window* w = NULL;

	switch (id)
	{
		case kDlgNone:
			break;

		case kDlgAboutBox:
			w = ::PrvMakeAboutDialog ();
			break;

		case kDlgSessionNew:
			w = ::PrvMakeSessionNewDialog ();
			break;

		case kDlgSessionSave:
			break;

		case kDlgHordeNew:
			w = ::PrvMakeNewHordeDialog ();
			break;

		case kDlgDatabaseImport:
			w = ::PrvMakeDatabaseImportDialog ();
			break;

		case kDlgDatabaseExport:
			w = ::PrvMakeDatabaseExportDialog ();
			break;

		case kDlgROMTransferQuery:
			w = ::PrvMakeROMTransferQueryDialog ();
			break;

		case kDlgROMTransferProgress:
			w = ::PrvMakeROMTransferProgressDialog ();
			break;

		case kDlgGremlinControl:
			w = ::PrvMakeGremlinControlDialog ();
			break;

		case kDlgEditPreferences:
			w = ::PrvMakeEditPreferencesDialog ();			
			break;

		case kDlgEditLogging:
			w = ::PrvMakeEditLoggingOptionsDialog ();
			break;

		case kDlgEditDebugging:
			w = ::PrvMakeEditDebuggingOptionsDialog ();
			break;

		case kDlgEditSkins:
			w = ::PrvMakeEditSkinsDialog ();
			break;

		case kDlgEditHostFS:
			w = ::PrvMakeHostFSOptionsDialog ();
			break;

		case kDlgCommonDialog:
			w = ::PrvMakeCommonDialog ();
			break;

		case kDlgSaveBound:
			break;

		case kDlgEditBreakpoints:
			w = ::PrvMakeEditBreakpointsDialog ();
			break;

		case kDlgEditCodeBreakpoint:
			w = ::PrvMakeEditCodeBreakpointDialog ();
			break;

		case kDlgEditTracingOptions:
			w = ::PrvMakeEditTracingOptionsDialog ();
			break;

		case kDlgEditPreferencesFullyBound:
			break;

		case kDlgReset:
			w = ::PrvMakeResetDialog ();
			break;

		case kDlgSessionInfo:
			w = ::PrvMakeSessionInfoDialog ();
			break;

		case kDlgGetSocketAddress:
//			w = ::PrvMakeGetSocketAddressDialog ();
			break;

		case kDlgEditErrorHandling:
			w = ::PrvMakeEditErrorHandlingDialog ();
			break;

		case kDlgMinimizeProgress:
			w = ::PrvMakeMinimizeDialog ();
			break;
	}

	return w;
}


Bool PrvInitializeDialog (EmDlgFn fn, void* data, EmDlgID dlgID, Fl_Window* dlg)
{
	EmDlgContext*		context = new EmDlgContext;
	if (!context)
	{
		delete dlg;
		return false;
	}

	context->fFn		= fn;
//	context->fFnResult	= filled in after fFn is called;
	context->fDlg		= (EmDlgRef) dlg;
	context->fDlgID		= dlgID;
//	context->fPanelID	= filled in by subroutines;
//	context->fItemID	= filled in by subroutines;
//	context->fCommandID	= filled in by subroutines;
//	context->fNeedsIdle	= filled in by c'tor to false;
	context->fUserData	= data;

	// Center the dialog.
	EmDlg::CenterDlg (context->fDlg);

	// Install callbacks for all the items we handle.
	::PrvInstallCallbacks (dlg, context);

	// Show the dialog.  Note that the positioning of this call is
	// important.  The call to EmDlgContext::Init may result in the
	// dialog's init handler trying to position the dialog.  If it
	// does that *before* the dialog is shown, some FLTK/X interaction
	// causes the window to appear down and to the right by an amount
	// equal to the top and left margins of the window frame.  Showing
	// the dialog first appears to fix that problem.
	dlg->show();

	// Initialize the dialog.  Delete the dialog if
	// initialization fails.
	if (context->Init () == kDlgResultClose)
	{
		::PrvDestroyDialog (context);
		return false;
	}

	// Add this dialog to our internal list.
	::PrvAddDialog (dlg);

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	PrvIdleCallback
 *
 * DESCRIPTION:	Function called by FLTK when our timeout function has
 *				timed out.  Here we call the custom dialog handler.  If
 *				it doesn't indicate that it's time to close the dialog,
 *				requeue the timeout function.
 *
 * PARAMETERS:	data - callback data.  We store the pointer to the
 *					current dialog context here.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void PrvIdleCallback (void* data)
{
	EmAssert (data);
	EmDlgContext& context = *(EmDlgContext*) data;

	// Call the idle function.  The result is stored in context.fFnResult
	// as well as returned from the function.  If we need to continue,
	// then requeue the idle function.

	if (context.Idle () != kDlgResultClose && context.fNeedsIdle)
	{
		EmDlg::HostStartIdling (context);
	}
}


// Copied from fl_draw.cxx, since they don't let us get to it directly.

#define MAXBUF 1024

// Copy p to buf, replacing unprintable characters with ^X and \nnn
// Stop at a newline of if MAXBUF characters written to buffer.
// Also word-wrap if width exceeds maxw.
// Returns a pointer to the start of the next line of caharcters.
// Sets n to the number of characters put into the buffer.
// Sets width to the width of the string in the current font.

static const char*
expand(const char* from, char* buf, double maxw, int& n, double &width, int wrap) {

  char* o = buf;
  char* e = buf+(MAXBUF-4);
//  underline_at = 0;
  char* word_end = o;
  const char* word_start = from;
  double w = 0;

  const char* p = from;
  for (;; p++) {

    int c = *p & 255;

    if (!c || c == ' ' || c == '\n') {
      // test for word-wrap:
      if (word_start < p && wrap) {
	double newwidth = w + fl_width(word_end, o-word_end);
	if (word_end > buf && newwidth > maxw) { // break before this word
	  o = word_end;
	  p = word_start;
	  break;
	}
	word_end = o;
	w = newwidth;
      }
      if (!c) break;
      else if (c == '\n') {p++; break;}
      word_start = p+1;
    }

    if (o > e) break; // don't overflow buffer

    if (c == '\t') {
      for (c = (o-buf)%8; c<8 && o<e; c++) *o++ = ' ';

#if 0
    } else if (c == '&' && fl_draw_shortcut && *(p+1)) {
      if (*(p+1) == '&') {p++; *o++ = '&';}
      else if (fl_draw_shortcut != 2) underline_at = o;
#endif
    } else if (c < ' ' || c == 127) { // ^X
      *o++ = '^';
      *o++ = c ^ 0x40;

   /*
    * mike@easysw.com - The following escaping code causes problems when
    * using the PostScript ISOLatin1 and WinANSI encodings, because these
    * map to I18N characters...
    */
#if 0
    } else if (c >= 128 && c < 0xA0) { // \nnn
      *o++ = '\\';
      *o++ = (c>>6)+'0';
      *o++ = ((c>>3)&7)+'0';
      *o++ = (c&7)+'0';
#endif /* 0 */

    } else if (c == 0xA0) { // non-breaking space
      *o++ = ' ';

    } else {
      *o++ = c;

    }
  }

  width = w + fl_width(word_end, o-word_end);
  *o = 0;
  n = o-buf;
  return p;
}

string PrvBreakLine (Fl_Input_* in, const char* str)
{
	string result;

	// This function follows the outline of fl_measure.

	// Set the font so that we can measure the text's width.

	fl_font (in->textfont (), in->textsize ());

	// Get the width of the box in which the text will be
	// laid out.  Fl_Output fudge by box_dw() + 6, so we
	// must do that, too.

	int			max_width = in->w ();
	Fl_Boxtype	b = in->box () ? in->box () : FL_DOWN_BOX;

	max_width -= Fl::box_dw (b) + 6;	

	// Break up the string, adding it back together in
	// "result", with the lines seperated by CR's.

	if (str)
	{
		const char* p;
		const char* e;
		char buf[MAXBUF];
		int buflen;
		double width;

		for (p = str; *p; p = e)
		{
			e = expand (p, buf, max_width, buflen, width, true);
			result += buf;
			if (*e)
				result += '\n';
		}
	}

	return result;
}

void Fl_Push_Button::draw (void)
{
	if (fType == kDefault)
	{
		Fl_Return_Button::draw ();
	}
	else
	{
		Fl_Button::draw ();
	}
}


int Fl_Push_Button::handle (int event)
{
	if (fType == kDefault)
	{
		return Fl_Return_Button::handle (event);
	}

	if (event == FL_SHORTCUT)
	{
		if (fType == kCancel && Fl::event_key () == FL_Escape)
		{
			do_callback ();
			return 1;
		}
	}

	return Fl_Button::handle (event);
}
#endif
