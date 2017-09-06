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

#ifndef EmDlg_h
#define EmDlg_h

#include "EmFileImport.h"		// EmFileImportMethod
#include "EmFileRef.h"			// EmFileRefList
#include "EmTransport.h"		// EmTransport
#include "EmTransportSerial.h"	// EmTransportSerial::BaudList
#include "EmTypes.h"			// EmResetType
#include "EmStructs.h"			// DatabaseInfoList
#include "Logging.h"			// FOR_EACH_LOG_PREF, FOR_EACH_REPORT_PREF

/*
	EmDlg fulfills two purposes:

	* 	Provides a "repository" for all dialogs the emulator shows. 
		Each dialog is invoked by calling an EmDlg entry point.  This
		entry point takes an optional set of parameters, shows and
		handles all interaction with the dialog, and returns the
		EmDlgItemID of the dialog item used to close the dialog (e.g.,
		OK, Cancel, etc.).

	* 	Provides a cross-platform interface for manipulating dialogs
		and their items.

	For the most part, clients of EmDlg use it to open and handle dialogs,
	and EmDlg itself uses the cross-platform part in order to update
	text, item states, etc., of the dialog.

	To add a new dialog:

	*	Define a new EmDlgID.  New ids should be added to the end of
		the current list of IDs.  They should not be renumbered. 
		Dialogs on some platforms (in particular, the Mac) are dependant
		on the numbers assigned to the EmDlgIDs, so don't renumber them
		without also renumbering all Mac dialogs and dialog items.

	*	Define new EmDlgItemIDs for all the items in the dialog.  All
		dialog items need to have unique IDs assigned to them -- even
		across all dialogs -- so use the DLG_BASE macro to establish a
		little number space for them based on the EmDlgID of the dialog
		they're in.

	*	Define a new entry point.  This entry point should have a name
		along the lines of Do<Something> (e.g., DoHordeNew,
		DoDatabaseExport). This entry point can take any number of
		parameters you like.

	*	Implement this entry point by calling EmDlg::RunDialog, passing
		to it the EmDlgID of the dialog box to show, a pointer to a
		callback procedure, and a pointer to any custom data to pass to
		the callback procedure (usually a pointer to a parameter block
		containing the parameters passed to the entry point).

	*	Implement the callback procedure.  The callback procedure is
		responsible for initializing the dialog and its elements,
		for handling clicks on the dialog items, and for finalizing and
		closing the dialog.  A template for a callback procedure is kept
		at the bottom of EmDlg.cpp.

	*	The callback procedure is passed a reference to an
		EmDlgContext.  This structure contains information about the
		dialog and why the callback procedure is being called.  The
		first thing the callback should do is examine the fCommandID
		field.  This field is of type EmDlgCmdID, and can be one of the
		following values:

		* kDlgCmdInit			Passed to the callback procedure once to
								initialize the dialog and its elements.

		* kDlgCmdIdle			Passed to the callback procedure several
								times a second to allow the dialog to idle.
								Sent to the callback only if it indicates
								that it wants idle time (by setting the
								fNeedsIdle field when initialized).

		* kDlgCmdItemSelected	Passed to the callback procedure any time
								a dialog item is selected.

		* kDlgCmdDestroy		Passed to the callback procedure once when
								the dialog is being destroyed.  The dialog
								may choose to perform any last minute
								cleanup here.

	*	If the command is kDlgCmdItemSelected, then the callback
		procedure should examine the fItemID field.  This indicates what
		dialog item was selected.

	*	The callback procedure returns an EmDlgFnResult.  This can be
		one of the following values:

		* kDlgResultContinue	Keep the dialog open--we're not done yet.

		* kDlgResultClose		Close the dialog.  Either the user pressed
								a button to close the dialog, or the dialog
								has some sort of auto-close feature, or an
								error occurred.

	*	Define your platform-specific dialogs.  In general, create your
		dialogs as you would naturally on that platform (e.g., use
		Constructor on the Mac, VC++'s dialog editor on Windows, or
		fluid if using FLTK on Unix).  However, to hook those dialogs up
		to the cross-platform mechanism, the following additional steps
		need to be taken:

		Macintosh
		---------
		*	The dialog's 'ppob' resource ID should be kDialogIDBase
			(2000) plus the EmDlgID of the dialog.

		*	The dialog items should have IDs (and, if applicable,
			message values) of kDialogItemIDBase (5000) plus their
			EmDlgItemID.

		Unix (FLTK)
		-----------
		*	Name the function that creates the dialog along the lines
			of "PrvMake<DialogName>".  In PrvMakeDialog() in
			EmDlgFltk.cpp, add a case statement to call this function.

		*	When creating widgets in fluid, include a line like the
			following in the "code" sections:

				::PrvSetWidgetID (o, kDlgItemOK);

		Windows
		-------
		*	Create the dialog and items using any resource symbols you
			care to define.  Note that the values for your resource
			symbols must be unique from those for all other dialog
			items.  The Emulator will enforce this with checks in
			PrvFromDlgItemID and PrvToDlgItemID (two internal routines
			used when creating and manipulating dialogs -- these checks
			are made only in debug builds).

		*	In EmDlgWin.cpp, map the symbols you've defined to the
			cross-platform symbols by adding the appropriate entries to
			kDlgIDMap and kDlgItemIDMap.
*/

class EmROMTransfer;


enum EmDlgCmdID
{
	kDlgCmdNone					= 0,

	kDlgCmdInit					= 1,
	kDlgCmdIdle					= 2,
	kDlgCmdItemSelected			= 3,
	kDlgCmdDestroy				= 4,
	kDlgCmdPanelEnter			= 5,
	kDlgCmdPanelExit			= 6
};

enum EmDlgFnResult
{
	kDlgResultNone				= 0,

	kDlgResultContinue			= 1,
	kDlgResultClose				= 2
};

enum EmDlgID
{
	kDlgNone					= 0,

	kDlgAboutBox				= 1,
	kDlgSessionNew				= 2,
	kDlgSessionSave				= 3,
	kDlgHordeNew				= 4,
	kDlgDatabaseImport			= 5,
	kDlgDatabaseExport			= 6,
	kDlgROMTransferQuery		= 7,
	kDlgROMTransferProgress		= 8,
	kDlgGremlinControl			= 9,
	kDlgEditPreferences			= 10,
	kDlgEditLogging				= 11,
	kDlgEditDebugging			= 12,
	kDlgEditSkins				= 13,
	kDlgCommonDialog			= 14,
	kDlgSaveBound				= 15,
	kDlgEditHostFS				= 16,
	kDlgEditBreakpoints			= 17,
	kDlgEditCodeBreakpoint		= 18,
	kDlgEditTracingOptions		= 19,
	kDlgEditPreferencesFullyBound	= 20,
	kDlgReset					= 21,
	kDlgSessionInfo				= 22,
	kDlgGetSocketAddress		= 23,
	kDlgEditErrorHandling		= 24,
	kDlgMinimizeProgress		= 25
};

enum EmDlgPanelID
{
	kDlgPanelNone				= 0,

	kDlgPanelAbtMain			= 1,
	kDlgPanelAbtWindows			= 2,
	kDlgPanelAbtMac				= 3,
	kDlgPanelAbtUAE				= 4,
	kDlgPanelAbtQNX				= 5
};

#define DLG_BASE(dlgID)	((dlgID) * 100)
enum EmDlgItemID
{
	kDlgItemNone				= 0,

	kDlgItemOK					= 0x01,
	kDlgItemCancel				= 0x02,
	kDlgItemYes					= 0x03,
	kDlgItemNo					= 0x04,
	kDlgItemContinue			= 0x05,
	kDlgItemDebug				= 0x06,
	kDlgItemReset				= 0x07,
	kDlgItemNextGremlin			= 0x08,

	kDlgItemAbtAppName			= DLG_BASE(kDlgAboutBox) + 0,
	kDlgItemAbtURLPalmWeb		= DLG_BASE(kDlgAboutBox) + 1,
	kDlgItemAbtURLPalmMail		= DLG_BASE(kDlgAboutBox) + 2,
	kDlgItemAbtURLWindowsWeb	= DLG_BASE(kDlgAboutBox) + 3,
	kDlgItemAbtURLWindowsMail	= DLG_BASE(kDlgAboutBox) + 4,
	kDlgItemAbtURLMacWeb		= DLG_BASE(kDlgAboutBox) + 5,
	kDlgItemAbtURLMacMail		= DLG_BASE(kDlgAboutBox) + 6,
	kDlgItemAbtURLUAEWeb		= DLG_BASE(kDlgAboutBox) + 7,
	kDlgItemAbtURLUAEMail		= DLG_BASE(kDlgAboutBox) + 8,
	kDlgItemAbtURLQNXWeb		= DLG_BASE(kDlgAboutBox) + 9,
	kDlgItemAbtURLQNXMail		= DLG_BASE(kDlgAboutBox) + 10,

	kDlgItemNewDevice			= DLG_BASE(kDlgSessionNew) + 0,
	kDlgItemNewSkin				= DLG_BASE(kDlgSessionNew) + 1,
	kDlgItemNewMemorySize		= DLG_BASE(kDlgSessionNew) + 2,
	kDlgItemNewROM				= DLG_BASE(kDlgSessionNew) + 3,
	kDlgItemNewROMBrowse		= DLG_BASE(kDlgSessionNew) + 4,

	kDlgItemHrdAppList			= DLG_BASE(kDlgHordeNew) + 0,
	kDlgItemHrdStart			= DLG_BASE(kDlgHordeNew) + 1,
	kDlgItemHrdStop				= DLG_BASE(kDlgHordeNew) + 2,
	kDlgItemHrdCheckSwitch		= DLG_BASE(kDlgHordeNew) + 3,
	kDlgItemHrdCheckSave		= DLG_BASE(kDlgHordeNew) + 4,
	kDlgItemHrdCheckStop		= DLG_BASE(kDlgHordeNew) + 5,
	kDlgItemHrdDepthSwitch		= DLG_BASE(kDlgHordeNew) + 6,
	kDlgItemHrdDepthSave		= DLG_BASE(kDlgHordeNew) + 7,
	kDlgItemHrdDepthStop		= DLG_BASE(kDlgHordeNew) + 8,
	kDlgItemHrdLogging			= DLG_BASE(kDlgHordeNew) + 9,
	kDlgItemHrdFirstLaunchedApp	= DLG_BASE(kDlgHordeNew) + 10,
	kDlgItemHrdSelectAll		= DLG_BASE(kDlgHordeNew) + 11,
	kDlgItemHrdSelectNone		= DLG_BASE(kDlgHordeNew) + 12,

	kDlgItemImpNumFiles			= DLG_BASE(kDlgDatabaseImport) + 0,
	kDlgItemImpProgress			= DLG_BASE(kDlgDatabaseImport) + 1,

	kDlgItemExpDbList			= DLG_BASE(kDlgDatabaseExport) + 0,

	kDlgItemDlqInstructions		= DLG_BASE(kDlgROMTransferQuery) + 0,
	kDlgItemDlqPortList			= DLG_BASE(kDlgROMTransferQuery) + 1,
	kDlgItemDlqBaudList			= DLG_BASE(kDlgROMTransferQuery) + 2,

	kDlgItemDlpMessage			= DLG_BASE(kDlgROMTransferProgress) + 0,
	kDlgItemDlpProgress			= DLG_BASE(kDlgROMTransferProgress) + 1,

	kDlgItemGrmNumber			= DLG_BASE(kDlgGremlinControl) + 0,
	kDlgItemGrmEventNumber		= DLG_BASE(kDlgGremlinControl) + 1,
	kDlgItemGrmElapsedTime		= DLG_BASE(kDlgGremlinControl) + 2,
	kDlgItemGrmStop				= DLG_BASE(kDlgGremlinControl) + 3,
	kDlgItemGrmResume			= DLG_BASE(kDlgGremlinControl) + 4,
	kDlgItemGrmStep				= DLG_BASE(kDlgGremlinControl) + 5,

	kDlgItemPrfRedirectSerial	= DLG_BASE(kDlgEditPreferences) + 0,
	kDlgItemPrfRedirectIR		= DLG_BASE(kDlgEditPreferences) + 1,
	kDlgItemPrfRedirectMystery	= DLG_BASE(kDlgEditPreferences) + 2,
	kDlgItemPrfRedirectNetLib	= DLG_BASE(kDlgEditPreferences) + 9,
	kDlgItemPrfEnableSound		= DLG_BASE(kDlgEditPreferences) + 10,
	kDlgItemPrfSaveAlways		= DLG_BASE(kDlgEditPreferences) + 11,
	kDlgItemPrfSaveAsk			= DLG_BASE(kDlgEditPreferences) + 12,
	kDlgItemPrfSaveNever		= DLG_BASE(kDlgEditPreferences) + 13,
	kDlgItemPrfUserName			= DLG_BASE(kDlgEditPreferences) + 14,

	kDlgItemLogNormal			= DLG_BASE(kDlgEditLogging) + 0,
	kDlgItemLogGremlins			= DLG_BASE(kDlgEditLogging) + 1,
	kDlgItemLogCheckBase		= DLG_BASE(kDlgEditLogging) + 10,
#undef DEFINE_BUTTON_ID
#define DEFINE_BUTTON_ID(name)	kDlgItemLog##name,
	FOR_EACH_LOG_PREF(DEFINE_BUTTON_ID)

	kDlgItemDbgDialogBeep		= DLG_BASE(kDlgEditDebugging) + 0,
	kDlgItemDbgCheckBase		= DLG_BASE(kDlgEditDebugging) + 10,
#undef DEFINE_BUTTON_ID
#define DEFINE_BUTTON_ID(name)	kDlgItemDbg##name,
	FOR_EACH_REPORT_PREF(DEFINE_BUTTON_ID)

	kDlgItemSknSkinList			= DLG_BASE(kDlgEditSkins) + 0,
	kDlgItemSknDoubleScale		= DLG_BASE(kDlgEditSkins) + 1,
	kDlgItemSknWhiteBackground	= DLG_BASE(kDlgEditSkins) + 2,
	kDlgItemSknDim				= DLG_BASE(kDlgEditSkins) + 3,
	kDlgItemSknRed				= DLG_BASE(kDlgEditSkins) + 4,
	kDlgItemSknGreen			= DLG_BASE(kDlgEditSkins) + 5,
	kDlgItemSknStayOnTop		= DLG_BASE(kDlgEditSkins) + 6,

	kDlgItemCmnText				= DLG_BASE(kDlgCommonDialog) + 0,
	kDlgItemCmnIconStop			= DLG_BASE(kDlgCommonDialog) + 1,
	kDlgItemCmnIconCaution		= DLG_BASE(kDlgCommonDialog) + 2,
	kDlgItemCmnIconNote			= DLG_BASE(kDlgCommonDialog) + 3,
	kDlgItemCmnButton1			= DLG_BASE(kDlgCommonDialog) + 4,
	kDlgItemCmnButton2			= DLG_BASE(kDlgCommonDialog) + 5,
	kDlgItemCmnButton3			= DLG_BASE(kDlgCommonDialog) + 6,
	kDlgItemCmnButtonCopy		= DLG_BASE(kDlgCommonDialog) + 7,

	kDlgItemBndSaveROM			= DLG_BASE(kDlgSaveBound) + 0,
	kDlgItemBndSaveRAM			= DLG_BASE(kDlgSaveBound) + 1,

	kDlgItemHfsList				= DLG_BASE(kDlgEditHostFS) + 0,
	kDlgItemHfsPath				= DLG_BASE(kDlgEditHostFS) + 1,
	kDlgItemHfsMounted			= DLG_BASE(kDlgEditHostFS) + 2,
	kDlgItemHfsBrowse			= DLG_BASE(kDlgEditHostFS) + 3,

	kDlgItemBrkList				= DLG_BASE(kDlgEditBreakpoints) + 0,
	kDlgItemBrkButtonEdit		= DLG_BASE(kDlgEditBreakpoints) + 1,
	kDlgItemBrkButtonClear		= DLG_BASE(kDlgEditBreakpoints) + 2,
	kDlgItemBrkCheckEnabled		= DLG_BASE(kDlgEditBreakpoints) + 3,
	kDlgItemBrkStartAddress		= DLG_BASE(kDlgEditBreakpoints) + 4,
	kDlgItemBrkNumberOfBytes	= DLG_BASE(kDlgEditBreakpoints) + 5,

	kDlgItemBrkAddress			= DLG_BASE(kDlgEditCodeBreakpoint) + 0,
	kDlgItemBrkCondition		= DLG_BASE(kDlgEditCodeBreakpoint) + 1,

	kDlgItemTrcOutput			= DLG_BASE(kDlgEditTracingOptions) + 0,
	kDlgItemTrcTargetText		= DLG_BASE(kDlgEditTracingOptions) + 1,
	kDlgItemTrcTargetValue		= DLG_BASE(kDlgEditTracingOptions) + 2,
	kDlgItemTrcInfo				= DLG_BASE(kDlgEditTracingOptions) + 3,
	kDlgItemTrcAutoConnect		= DLG_BASE(kDlgEditTracingOptions) + 4,

	kDlgItemRstSoft				= DLG_BASE(kDlgReset) + 0,
	kDlgItemRstHard				= DLG_BASE(kDlgReset) + 1,
	kDlgItemRstDebug			= DLG_BASE(kDlgReset) + 2,
	kDlgItemRstNoExt			= DLG_BASE(kDlgReset) + 3,

	kDlgItemInfDeviceFld		= DLG_BASE(kDlgSessionInfo) + 0,
	kDlgItemInfRAMFld			= DLG_BASE(kDlgSessionInfo) + 1,
	kDlgItemInfROMFld			= DLG_BASE(kDlgSessionInfo) + 2,
	kDlgItemInfSessionFld		= DLG_BASE(kDlgSessionInfo) + 3,

	kDlgItemSocketAddress		= DLG_BASE(kDlgGetSocketAddress) + 0,

	kDlgItemErrWarningOff		= DLG_BASE(kDlgEditErrorHandling) + 0,
	kDlgItemErrErrorOff			= DLG_BASE(kDlgEditErrorHandling) + 1,
	kDlgItemErrWarningOn		= DLG_BASE(kDlgEditErrorHandling) + 2,
	kDlgItemErrErrorOn			= DLG_BASE(kDlgEditErrorHandling) + 3,

	kDlgItemMinPassNumber		= DLG_BASE(kDlgMinimizeProgress) + 0,
	kDlgItemMinEventNumber		= DLG_BASE(kDlgMinimizeProgress) + 1,
	kDlgItemMinElapsed			= DLG_BASE(kDlgMinimizeProgress) + 2,
	kDlgItemMinRange			= DLG_BASE(kDlgMinimizeProgress) + 3,
	kDlgItemMinDiscarded		= DLG_BASE(kDlgMinimizeProgress) + 4,
	kDlgItemMinProgress			= DLG_BASE(kDlgMinimizeProgress) + 5,

	kDlgItemLast
};


enum EmCommonDialogFlags
{
	// Each button is stored in an 8-bit field.  A button ID is
	// stored in the lower 4-bits, and the upper 4-bits are use
	// to hold flags indicating if the button is the default button,
	// if the button is enabled, or if the button is even visible.
	// There are three of these fields, filling 24 of the 32 bits of
	// the flags parameter.

	/*
		The various buttons have different positions depending on the platform
		being run on:

		Mac/Unix:

			+----------------------------------+
			| **                               |
			| **  Blah blah blah               |
			| **                               |
			|                                  |
			|                                  |
			|          Button3 Button2 Button1 |
			+----------------------------------+

		Windows:

			+----------------------------------+
			| **                       Button1 |
			| **  Blah blah blah       Button2 |
			| **                       Button3 |
			|                                  |
			|                                  |
			|                                  |
			+----------------------------------+
	*/

	kButtonMask			= 0x0F,

	kButtonDefault		= 0x10,
	kButtonEscape		= 0x20,
	kButtonVisible		= 0x40,
	kButtonEnabled		= 0x80,

	kButtonFieldShift	= 8,
	kButtonFieldMask	= 0x000000FF,

	// The following naming convention is used for the following values:
	//
	//	An all-upper name
	//		the button is the default button (and visible and enabled)
	//
	//	A first-upper name
	//		the button is visible and enabled.
	//
	//	An all-lower name
	//		the button is visible but disabled.

#define SET_BUTTON(p, x) (((long)(x) & kButtonFieldMask) << (kButtonFieldShift * (p)))
#define GET_BUTTON(p, x) (((x) >> (kButtonFieldShift * (p))) & kButtonFieldMask)

#define SET_BUTTON_DEFAULT(p, x)	SET_BUTTON(p, (x) | kButtonVisible | kButtonEnabled | kButtonDefault)
#define SET_BUTTON_ESCAPE(p, x)		SET_BUTTON(p, (x) | kButtonVisible | kButtonEnabled | kButtonEscape)
#define SET_BUTTON_STANDARD(p, x)	SET_BUTTON(p, (x) | kButtonVisible | kButtonEnabled)
#define SET_BUTTON_DISABLED(p, x)	SET_BUTTON(p, (x) | kButtonVisible)

	kDlgFlags_None					= 0,

	kDlgFlags_OK					= SET_BUTTON_DEFAULT	(0, kDlgItemOK),

	kDlgFlags_CANCEL				= SET_BUTTON_DEFAULT	(0, kDlgItemCancel),

	kDlgFlags_OK_Cancel				= SET_BUTTON_DEFAULT	(0, kDlgItemOK)
									| SET_BUTTON_ESCAPE		(1, kDlgItemCancel),

	kDlgFlags_Ok_CANCEL				= SET_BUTTON_STANDARD	(0, kDlgItemOK)
									| SET_BUTTON_DEFAULT	(1, kDlgItemCancel),

	kDlgFlags_YES_No				= SET_BUTTON_DEFAULT	(0, kDlgItemYes)
									| SET_BUTTON_ESCAPE		(1, kDlgItemNo),

	kDlgFlags_Yes_NO				= SET_BUTTON_STANDARD	(0, kDlgItemYes)
									| SET_BUTTON_DEFAULT	(1, kDlgItemNo),

	kDlgFlags_Continue_DEBUG_Reset	= SET_BUTTON_STANDARD	(0, kDlgItemContinue)
									| SET_BUTTON_DEFAULT	(1, kDlgItemDebug)
									| SET_BUTTON_STANDARD	(2, kDlgItemReset),

	kDlgFlags_continue_DEBUG_Reset	= SET_BUTTON_DISABLED	(0, kDlgItemContinue)
									| SET_BUTTON_DEFAULT	(1, kDlgItemDebug)
									| SET_BUTTON_STANDARD	(2, kDlgItemReset),

	kDlgFlags_continue_debug_RESET	= SET_BUTTON_DISABLED	(0, kDlgItemContinue)
									| SET_BUTTON_DISABLED	(1, kDlgItemDebug)
									| SET_BUTTON_DEFAULT	(2, kDlgItemReset)
};

typedef vector<EmDlgItemID>		EmDlgItemIDList;

typedef long					EmDlgItemIndex;
typedef vector<EmDlgItemIndex>	EmDlgItemIndexList;

typedef long					EmDlgListIndex;	// Zero-based
typedef vector<EmDlgListIndex>	EmDlgListIndexList;
const EmDlgListIndex			kDlgItemListNone = -1;

struct EmDlgContext;

typedef EmDlgItemID				(*EmDlgThreadFn)(const void*);
typedef EmDlgFnResult			(*EmDlgFn)(EmDlgContext&);
typedef void*					EmDlgRef;

enum EmROMFileStatus
{
	kROMFileOK,
	kROMFileDubious,
	kROMFileUnknown
};

struct EmDlgContext
{
						EmDlgContext (void);
						EmDlgContext (const EmDlgContext&);

	EmDlgFnResult		Init ();
	EmDlgFnResult		Event (EmDlgItemID);
	EmDlgFnResult		Idle ();
	void				Destroy ();
	EmDlgFnResult		PanelEnter ();
	EmDlgFnResult		PanelExit ();

	EmDlgFn				fFn;
	EmDlgFnResult		fFnResult;
	EmDlgRef			fDlg;
	EmDlgID				fDlgID;
	EmDlgPanelID		fPanelID;
	EmDlgItemID			fItemID;
	EmDlgCmdID			fCommandID;
	bool				fNeedsIdle;
	void*				fUserData;

	EmDlgItemID			fDefaultItem;
	EmDlgItemID			fCancelItem;
};

struct DoGetFileParameters
{
	DoGetFileParameters (EmFileRef& result,
						 const string& prompt,
						 const EmDirRef& defaultPath,
						 const EmFileTypeList& filterList) :
		fResult (result),
		fPrompt (prompt),
		fDefaultPath (defaultPath),
		fFilterList (filterList)
	{
	}

	EmFileRef&				fResult;
	const string&			fPrompt;
	const EmDirRef&			fDefaultPath;
	const EmFileTypeList&	fFilterList;
};

struct DoGetFileListParameters
{
	DoGetFileListParameters (EmFileRefList& results,
							 const string& prompt,
							 const EmDirRef& defaultPath,
							 const EmFileTypeList& filterList) :
		fResults (results),
		fPrompt (prompt),
		fDefaultPath (defaultPath),
		fFilterList (filterList)
	{
	}

	EmFileRefList&			fResults;
	const string&			fPrompt;
	const EmDirRef&			fDefaultPath;
	const EmFileTypeList&	fFilterList;
};

struct DoPutFileParameters
{
	DoPutFileParameters (EmFileRef& result,
						 const string& prompt,
						 const EmDirRef& defaultPath,
						 const EmFileTypeList& filterList,
						 const string& defaultName) :
		fResult (result),
		fPrompt (prompt),
		fDefaultPath (defaultPath),
		fFilterList (filterList),
		fDefaultName (defaultName)
	{
	}

	EmFileRef&				fResult;
	const string&			fPrompt;
	const EmDirRef&			fDefaultPath;
	const EmFileTypeList&	fFilterList;
	const string&			fDefaultName;
};

struct DoGetDirectoryParameters
{
	DoGetDirectoryParameters (EmDirRef& result,
							  const string& prompt,
							  const EmDirRef& defaultPath) :
		fResult (result),
		fPrompt (prompt),
		fDefaultPath (defaultPath)
	{
	}

	EmDirRef&				fResult;
	const string&			fPrompt;
	const EmDirRef&			fDefaultPath;
};

struct DoSessionSaveParameters
{
	DoSessionSaveParameters (const string& appName,
							 const string& docName,
							 Bool quitting) :
		fAppName (appName),
		fDocName (docName),
		fQuitting (quitting)
	{
	}

	const string&			fAppName;
	const string&			fDocName;
	Bool					fQuitting;
};

struct RunDialogParameters
{
	RunDialogParameters (EmDlgFn fn, void* userData, EmDlgID dlgID) :
		fFn (fn),
		fUserData (userData),
		fDlgID (dlgID)
	{
	}

	EmDlgFn					fFn;
	void*					fUserData;
	EmDlgID					fDlgID;
};

// !!! Needs to be moved elsewhere
typedef DatabaseInfoList	EmDatabaseList;
struct EditCodeBreakpointData;
struct EmErrorHandlingMenuBundle;

class EmDlg
{
	public:
		static EmDlgItemID		DoGetFile					(EmFileRef& result,
															 const string& prompt,
															 const EmDirRef& defaultPath,
															 const EmFileTypeList& filterList);
		static EmDlgItemID		DoGetFileList				(EmFileRefList& results,
															 const string& prompt,
															 const EmDirRef& defaultPath,
															 const EmFileTypeList& filterList);
		static EmDlgItemID		DoPutFile					(EmFileRef& result,
															 const string& prompt,
															 const EmDirRef& defaultPath,
															 const EmFileTypeList& filterList,
															 const string& defaultName);
		static EmDlgItemID		DoGetDirectory				(EmDirRef& result,
															 const string& prompt,
															 const EmDirRef& defaultPath);

		static EmDlgItemID		DoAboutBox					(void);

		static EmDlgItemID		DoSessionNew				(Configuration&);
		static EmDlgItemID		DoSessionSave				(const string& docName,
															 Bool quitting);

		static EmDlgItemID		DoHordeNew					(void);

		static EmDlgItemID		DoDatabaseImport			(const EmFileRefList&,
															 EmFileImportMethod method);
		static EmDlgItemID		DoDatabaseExport			(void);
		static EmDlgItemID		DoReset						(EmResetType&);
		static EmDlgItemID		DoROMTransferQuery			(EmTransport::Config*&);
		static EmDlgItemID		DoROMTransferProgress		(EmROMTransfer&);

		static EmDlgItemID		DoEditPreferences			(void);
		static EmDlgItemID		DoEditLoggingOptions		(LoggingType);
		static EmDlgItemID		DoEditDebuggingOptions		(void);
		static EmDlgItemID		DoEditErrorHandling			(void);
		static EmDlgItemID		DoEditSkins					(void);
		static EmDlgItemID		DoEditHostFSOptions			(void);
		static EmDlgItemID		DoEditBreakpoints			(void);
		static EmDlgItemID		DoEditCodeBreakpoint		(EditCodeBreakpointData&);
#if HAS_TRACER
		static EmDlgItemID		DoEditTracingOptions		(void);
#endif

		static EmDlgItemID		DoCommonDialog				(StrCode msg,
															 EmCommonDialogFlags);
		static EmDlgItemID		DoCommonDialog				(const char* msg,
															 EmCommonDialogFlags);
		static EmDlgItemID		DoCommonDialog				(const string& msg,
															 EmCommonDialogFlags);

		static EmDlgItemID		DoSaveBound					(void);
		static EmDlgItemID		DoSessionInfo				(void);
		static EmDlgItemID		DoGetSocketAddress			(string&);

		static void				GremlinControlOpen			(void);
		static void				GremlinControlClose			(void);

		static void				MinimizeProgressOpen		(void);
		static void				MinimizeProgressClose		(void);

		static EmDlgItemID		RunDialog					(EmDlgFn, void*, EmDlgID);
		static EmDlgItemID		RunDialog					(EmDlgThreadFn, const void* parameters);

		static EmDlgRef			DialogOpen					(EmDlgFn, void*, EmDlgID);
		static void				DialogClose					(EmDlgRef);

	private:
		static EmDlgItemID		HostRunGetFile				(const void* parameters);
		static EmDlgItemID		HostRunGetFileList			(const void* parameters);
		static EmDlgItemID		HostRunPutFile				(const void* parameters);
		static EmDlgItemID		HostRunGetDirectory			(const void* parameters);
		static EmDlgItemID		HostRunAboutBox				(const void* parameters);
		static EmDlgItemID		HostRunSessionSave			(const void* parameters);
		static EmDlgItemID		HostRunDialog				(const void* parameters);

		static EmDlgRef			HostDialogOpen				(EmDlgFn, void*, EmDlgID);
		static void				HostDialogClose				(EmDlgRef);

	public:	// public right now for PrvIdleCallback, PrvCreateDialog
		static void				HostDialogInit				(EmDlgContext&);
		static void				HostStartIdling				(EmDlgContext&);
		static void				HostStopIdling				(EmDlgContext&);

#if PLATFORM_WINDOWS
		static BOOL CALLBACK	PrvHostModalProc			(HWND hWnd,
															 UINT msg,
															 WPARAM wParam,
															 LPARAM lParam);
		static BOOL CALLBACK	PrvHostModelessProc			(HWND hWnd,
															 UINT msg,
															 WPARAM wParam,
															 LPARAM lParam);
#endif

	private:
		friend struct EmDlgContext;	// To call HostStartIdling

	private:
		static EmDlgFnResult	PrvSessionNew				(EmDlgContext&);
		static EmDlgFnResult	PrvHordeNew					(EmDlgContext&);
		static EmDlgFnResult	PrvDatabaseImport			(EmDlgContext&);
		static EmDlgFnResult	PrvDatabaseExport			(EmDlgContext&);
		static EmDlgFnResult	PrvReset					(EmDlgContext&);
		static EmDlgFnResult	PrvROMTransferQuery			(EmDlgContext&);
		static EmDlgFnResult	PrvROMTransferProgress		(EmDlgContext&);
		static EmDlgFnResult	PrvEditPreferences			(EmDlgContext&);
		static EmDlgFnResult	PrvEditLoggingOptions		(EmDlgContext&);
		static EmDlgFnResult	PrvEditDebuggingOptions		(EmDlgContext&);
		static EmDlgFnResult	PrvErrorHandling			(EmDlgContext&);
		static EmDlgFnResult	PrvEditSkins				(EmDlgContext&);
		static EmDlgFnResult	PrvEditHostFSOptions		(EmDlgContext&);
		static EmDlgFnResult	PrvEditBreakpoints			(EmDlgContext&);
		static EmDlgFnResult	PrvEditCodeBreakpoint		(EmDlgContext&);
#if HAS_TRACER
		static EmDlgFnResult	PrvEditTracingOptions		(EmDlgContext&);
#endif
		static EmDlgFnResult	PrvCommonDialog				(EmDlgContext&);
		static EmDlgFnResult	PrvSaveBound				(EmDlgContext&);
		static EmDlgFnResult	PrvSessionInfo				(EmDlgContext&);
		static EmDlgFnResult	PrvGetSocketAddress			(EmDlgContext&);
		static EmDlgFnResult	PrvGremlinControl			(EmDlgContext&);
		static EmDlgFnResult	PrvMinimizeProgress			(EmDlgContext&);

		// DoSessionNew

		static void				PrvBuildROMMenu				(const EmDlgContext&);
		static void				PrvBuildDeviceMenu			(const EmDlgContext&);
		static void				PrvBuildSkinMenu			(const EmDlgContext&);
		static void				PrvBuildMemorySizeMenu		(const EmDlgContext&);
		static void				PrvNewSessionSetOKButton	(const EmDlgContext&);
		static void				PrvFilterDeviceList			(const EmFileRef& romFile,
															 EmDeviceList& devices,
															 EmDeviceList::iterator& devices_end,
															 unsigned int& version);
		static void				PrvFilterMemorySizes		(MemoryTextList& sizes,
															 const Configuration& cfg);
		static EmROMFileStatus	PrvCanUseROMFile			(EmFileRef& testRef);

		// DoHordeNew

		static UInt32			PrvSelectedAppNameToIndex	(EmDatabaseList list, const string& appName);
		static DatabaseInfo		PrvSelectedIndexToApp		(EmDatabaseList list, uint32 index);
		static void				PrvSelectedList				(EmDatabaseList selectedApps,
																StringList &selectedAppStrings);

		// DoDatabaseExport

		static Bool				PrvExportFile				(const DatabaseInfo& db);

		// DoROMTransferQuery

		static void				PrvGetDlqPortItemList		(EmTransportDescriptorList& menuItems);
		static void				PrvConvertBaudListToStrings	(const EmTransportSerial::BaudList& baudList,
															 StringList& baudStrList);

		// DoEditPreferences

		static void				PrvEditPrefToDialog			(EmDlgContext& context);
		static void				PrvEditPrefFromDialog		(EmDlgContext& context);
		static void				PrvBuildDescriptorLists		(EmDlgContext& context);
		static void				PrvGetEditPreferences		(EmDlgContext& context);
		static void				PrvPutEditPreferences		(EmDlgContext& context);
		static Bool				PrvEditPreferencesValidate	(EmDlgContext& context);
		static void				PrvGetPrefSocketAddress		(EmDlgContext& context);

		// DoEditLoggingOptions

		static void				PrvFetchLoggingPrefs		(EmDlgContext& context);
		static void				PrvInstallLoggingPrefs		(EmDlgContext& context);
		static void				PrvLoggingPrefsToButtons	(EmDlgContext& context);
		static void				PrvLoggingPrefsFromButtons	(EmDlgContext& context);

		// DoEditDebuggingOptions

		static void				PrvFetchDebuggingPrefs		(EmDlgContext& context);
		static void				PrvInstallDebuggingPrefs	(EmDlgContext& context);
		static void				PrvDebuggingPrefsToButtons	(EmDlgContext& context);
		static void				PrvDebuggingPrefsFromButtons (EmDlgContext& context);

		// DoEditErrorHandling

		static string			PrvMenuItemText				(EmErrorHandlingOption item);
		static void				PrvBuildMenu				(EmErrorHandlingMenuBundle&	menu,
															 StringList&				items);
		static long				PrvFindIndex				(EmErrorHandlingMenuBundle&	menu,
															 EmErrorHandlingOption		toFind);
		static void				PrvErrorHandlingToDialog	(EmDlgContext&				context,
															 EmErrorHandlingMenuBundle&	menu);
		static void				PrvErrorHandlingFromDialog	(EmDlgContext&				context,
															 EmErrorHandlingMenuBundle&	menu);
		static EmDlgItemID		PrvAskChangeLogging			(void);
		static void				PrvCheckSetting				(EmDlgContext&				context,
															 EmErrorHandlingMenuBundle&	menu);
		static Bool				PrvCheckSettings			(EmDlgContext& context);
		static void				PrvEnableLoggingOption		(EmErrorHandlingMenuBundle& menu);
		static void				PrvEnableLoggingOptions		(void);

		// DoEditHostFSOptions

		static void				PrvEditHostFSOptionsOK		(EmDlgContext& context);

		// DoEditBreakpoints

		static void				PrvEnableCodeBreakpointControls	(EmDlgContext& context, bool enable);
		static void				PrvEnableDataBreakpointControls	(EmDlgContext& context, bool enable);
		static void				PrvRefreshCodeBreakpointList	(EmDlgContext& context);
		static void				PrvGetCodeBreakpoints		(EmDlgContext& context);
		static void				PrvSetCodeBreakpoints		(EmDlgContext& context);

		// DoEditTracingOptions

#if HAS_TRACER
		static void				PrvPopTracerSettings		(EmDlgContext& context);
		static void				PrvPushTracerSettings		(EmDlgContext& context);
#endif

		// GremlinControl

		static void				PrvGrmUpdateGremlinNumber	(EmDlgContext& context);
		static void				PrvGrmUpdateEventNumber		(EmDlgContext& context);
		static void				PrvGrmUpdateElapsedTime		(EmDlgContext& context);
		static void				PrvGrmUpdateAll				(EmDlgContext& context);

		// Minimization Progress

		static void				PrvMinUpdatePassNumber		(EmDlgContext& context);
		static void				PrvMinUpdateEventNumber		(EmDlgContext& context);
		static void				PrvMinUpdateElapsed			(EmDlgContext& context);
		static void				PrvMinUpdateRange			(EmDlgContext& context);
		static void				PrvMinUpdateDiscarded		(EmDlgContext& context);
		static void				PrvMinUpdateProgress		(EmDlgContext& context);
		static void				PrvMinUpdateAll				(EmDlgContext& context);

	public:	// Most of these are really "Host" functions
		static void				SetDlgBounds				(EmDlgRef, const EmRect&);
		static EmRect			GetDlgBounds				(EmDlgRef);
		static void				MoveDlgTo					(EmDlgRef, EmCoord x, EmCoord y);
		static void				MoveDlgTo					(EmDlgRef, const EmPoint&);
		static void				CenterDlg					(EmDlgRef);
		static void				EnsureDlgOnscreen			(EmDlgRef);

		static void				SetDlgDefaultButton			(EmDlgContext&, EmDlgItemID);
		static void				SetDlgCancelButton			(EmDlgContext&, EmDlgItemID);

		static void				SetItemMin					(EmDlgRef, EmDlgItemID, long max);
		static void				SetItemValue				(EmDlgRef, EmDlgItemID, long value);
		static void				SetItemMax					(EmDlgRef, EmDlgItemID, long max);
		static void				SetItemBounds				(EmDlgRef, EmDlgItemID, const EmRect&);

		static void				SetItemText					(EmDlgRef, EmDlgItemID, StrCode);
		static void				SetItemText					(EmDlgRef, EmDlgItemID, const char*);
		static void				SetItemText					(EmDlgRef, EmDlgItemID, string);

		static long				GetItemMin					(EmDlgRef, EmDlgItemID);
		static long				GetItemValue				(EmDlgRef, EmDlgItemID);
		static long				GetItemMax					(EmDlgRef, EmDlgItemID);
		static EmRect			GetItemBounds				(EmDlgRef, EmDlgItemID);
		static string			GetItemText					(EmDlgRef, EmDlgItemID);

		static void				EnableItem					(EmDlgRef, EmDlgItemID);
		static void				DisableItem					(EmDlgRef, EmDlgItemID);
		static void				EnableDisableItem			(EmDlgRef, EmDlgItemID, Bool);

		static void				ShowItem					(EmDlgRef, EmDlgItemID);
		static void				HideItem					(EmDlgRef, EmDlgItemID);
		static void				ShowHideItem				(EmDlgRef, EmDlgItemID, Bool);

		static void				ClearMenu					(EmDlgRef, EmDlgItemID);
		static void				AppendToMenu				(EmDlgRef, EmDlgItemID, const string&);
		static void				AppendToMenu				(EmDlgRef, EmDlgItemID, const StringList&);
		static void				EnableMenuItem				(EmDlgRef, EmDlgItemID, long);
		static void				DisableMenuItem				(EmDlgRef, EmDlgItemID, long);

		static void				ClearList					(EmDlgRef, EmDlgItemID);
		static void				AppendToList				(EmDlgRef, EmDlgItemID, const string&);
		static void				AppendToList				(EmDlgRef, EmDlgItemID, const StringList&);
		static void				SelectListItem				(EmDlgRef, EmDlgItemID, EmDlgListIndex);
		static void				SelectListItems				(EmDlgRef, EmDlgItemID, const EmDlgListIndexList&);
		static void				UnselectListItem			(EmDlgRef, EmDlgItemID, EmDlgListIndex);
		static void				UnselectListItems			(EmDlgRef, EmDlgItemID, const EmDlgListIndexList&);
		static EmDlgListIndex	GetSelectedItem				(EmDlgRef, EmDlgItemID);
		static void				GetSelectedItems			(EmDlgRef, EmDlgItemID, EmDlgListIndexList&);

		static int				GetTextHeight				(EmDlgRef, EmDlgItemID, const string&);

		static Bool				StringToLong				(const char*, long*);
};

#endif	/* EmDlg_h */
