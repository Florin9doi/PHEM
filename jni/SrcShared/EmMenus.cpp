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
#include "EmMenus.h"

#include "EmApplication.h"		// IsBoundPartially
#include "EmFileRef.h"			// EmFileRefList
#include "EmPatchState.h"		// EmPatchState::UIInitialized
#include "EmSession.h"			// gSession
#include "Hordes.h"				// Hordes::CanNew, etc.
#include "Platform.h"			// Platform::GetString
#include "PreferenceMgr.h"		// gEmuPrefs
#include "Profiling.h"			// ProfileCanStart, etc.
#include "Strings.r.h"			// kStr_Menu...

// ---------------------------------------------------------------------------

#pragma mark Forward Declarations

static void		PrvInitializeMenus	(Bool alternateLayout);
static void		PrvAddMenuItem		(EmMenuItemList&, EmCommandID);
static void		PrvSetSubMenu		(EmMenuItemList&, EmCommandID, EmMenuItemList&);
static void		PrvUpdateMruMenus	(const EmFileRefList&, EmCommandID, EmMenuItemList&);
static void		PrvCollectFiles		(EmFileRefList&, EmFileRefList&);
static Bool		PrvGetItemStatus	(const EmMenuItem& item);
static void		PrvPrefsChanged		(PrefKeyType key, void*);
static EmMenuID	PrvPreferredMenuID	(EmMenuID);

// ---------------------------------------------------------------------------

#pragma mark -
#pragma mark Private Globals

static EmMenu	gMenuMenubar;
static EmMenu	gMenuPartiallyBoundMenubar;
static EmMenu	gMenuFullyBoundMenubar;

static EmMenu	gMenuPopupMenu;
static EmMenu	gMenuPartiallyBoundPopupMenu;
static EmMenu	gMenuFullyBoundPopupMenu;

static unsigned long	gChangeCount;

// ---------------------------------------------------------------------------
// This array is accessed by index using the values in EmMenuItemID.  Thus,
// any changes in this array need corresponding changes in that list.

struct EmPrvMenuItem
{
	EmCommandID		fCommandID;
#if 1
	StrCode			fTitle;
#else
	const char*		fTitle;
#endif
};

#define __________	kCommandDivider

/*
	Keyboard Equivalents

	Mac OS X reserves certain keyboard combinations as equivalents to menu
	commands; these shortcuts affect all applications. Even if your application
	doesnÕt support all the combinations shown in Table 8-3, donÕt use any of
	them for another function.

		Table 8-3 Reserved and recommended keyboard equivalents

		Menu				Keys		Command
		------------------------------------------------
		Application			Command-H*	Hide
		Application			Command-Q*	Quit
		................................................
		Window				Command-M*	Minimize
		................................................
		File				Command-N*	New
		File				Command-O	Open
		File				Command-W	Close
		File				Command-S	Save
		File				Command-P	Print
		................................................
		Edit				Command-Z	Undo
		Edit				Command-X	Cut
		Edit				Command-C	Copy
		Edit				Command-V	Paste
		Edit				Command-A	Select All
		................................................
		Format				Command-T	Open Font dialog
		------------------------------------------------
		* These combinations are reserved by the system; the others are
		recommended.

	Some applications use other common keyboard equivalents, as shown in Table 8-4.
	If you choose not to implement these functions in your product, you may use these
	equivalents for other actions, although that could make your application less
	intuitive for users accustomed to the combinations shown here.

		Table 8-4 Common keyboard equivalents that are not reserved

		Menu				Keys		Command
		------------------------------------------------
		Edit (recommended)	Command-F	Find
		Edit (recommended)	Command-G	Find Again
		................................................
		Format				Command-B	Bold
		Format				Command-I	Italic
		Format				Command-U	Underline
		------------------------------------------------

	Hmmm...that leaves: D, E, J, K, L, R, and Y for anything else!

		A |	
		B |	Breakpoints
		C |	Copy
		D |	Download ROM
		E |	Error Handling
		F |	
		G |	
		H |	HotSync
		I |	Import Database
		J |	
		K |	Skins
		L |	Logging Options
		M |	Save Screen
		N |	New Session
		O |	Open Session
		P |	
		Q |	Quit
		R |	Reset
		S |	Save Session
		T |	Tracing Options
		U |	
		V |	Paste
		W |	Close Session
		X |	Cut
		Y |	
		Z |	Undo
		/ | Preferences
		\ | Debuging Options
		[ | Profile Start
		] | Profile Stop

	===============================
	Accelerator key mappings
	===============================

		--+-----------------------------------------------------------------------------+--
		  |	Top level	Reset	Open	Import	Settings	Gremlins	Profile	Edit	|
		--+-----------------------------------------------------------------------------+--
		A |	Save As																		| A
		B |	Save Bound							Breakpoints								| B
		C |	Close		Cold													Copy	| C
		D |	Export		Debug					Debugging				Dump			| D
		E |	Settings							Error Handling							| E
		F |	Profile																		| F
		G |	Gremlins																	| G
		H |	HotSync								HostFS									| H
		I |	Install																		| I
		J |																				| J
		K |																				| K
		L |										Logging									| L
		M |													Minimize					| M
		N |	New			Normal								New							| N
		O |	Open				Other	Other											| O
		P |	Properties										Stop		Stop	Paste	| P
		Q |	Quit																		| Q
		R |	Reset											Resume				Clear	| R
		S |	Save								Skins		Step		Start			| S
		T |	Transfer ROM						Tracing							Cut		| T
		U |																		Undo	| U
		V |	Save Screen																	| V
		W |																				| W
		X |	Exit																		| X
		Y |													Replay						| Y
		Z |																				| Z
		--+-----------------------------------------------------------------------------+--
*/

EmPrvMenuItem	kPrvMenuItems[] =
{
	{ kCommandSessionNew,		kStr_MenuSessionNew },
	{ kCommandSessionOpenOther,	kStr_MenuSessionOpenOther },
	{ kCommandSessionOpen0,		kStr_MenuBlank },
	{ kCommandSessionOpen1,		kStr_MenuBlank },
	{ kCommandSessionOpen2,		kStr_MenuBlank },
	{ kCommandSessionOpen3,		kStr_MenuBlank },
	{ kCommandSessionOpen4,		kStr_MenuBlank },
	{ kCommandSessionOpen5,		kStr_MenuBlank },
	{ kCommandSessionOpen6,		kStr_MenuBlank },
	{ kCommandSessionOpen7,		kStr_MenuBlank },
	{ kCommandSessionOpen8,		kStr_MenuBlank },
	{ kCommandSessionOpen9,		kStr_MenuBlank },
	{ kCommandSessionClose,		kStr_MenuSessionClose },

	{ kCommandSessionSave,		kStr_MenuSessionSave },
	{ kCommandSessionSaveAs,	kStr_MenuSessionSaveAs },
	{ kCommandSessionBound,		kStr_MenuSessionBound },
	{ kCommandScreenSave,		kStr_MenuScreenSave },
	
	{ kCommandSessionInfo,		kStr_MenuSessionInfo },

	{ kCommandImportOther,		kStr_MenuImportOther },
	{ kCommandImport0,			kStr_MenuBlank },
	{ kCommandImport1,			kStr_MenuBlank },
	{ kCommandImport2,			kStr_MenuBlank },
	{ kCommandImport3,			kStr_MenuBlank },
	{ kCommandImport4,			kStr_MenuBlank },
	{ kCommandImport5,			kStr_MenuBlank },
	{ kCommandImport6,			kStr_MenuBlank },
	{ kCommandImport7,			kStr_MenuBlank },
	{ kCommandImport8,			kStr_MenuBlank },
	{ kCommandImport9,			kStr_MenuBlank },
	{ kCommandExport,			kStr_MenuExport },

	{ kCommandHotSync,			kStr_MenuHotSync },
	{ kCommandReset,			kStr_MenuReset },
	{ kCommandDownloadROM,		kStr_MenuDownloadROM },

	{ kCommandUndo,				kStr_MenuUndo },
	{ kCommandCut,				kStr_MenuCut },
	{ kCommandCopy,				kStr_MenuCopy },
	{ kCommandPaste,			kStr_MenuPaste },
	{ kCommandClear,			kStr_MenuClear },

	{ kCommandPreferences,		kStr_MenuPreferences },
	{ kCommandLogging,			kStr_MenuLogging },
	{ kCommandDebugging,		kStr_MenuDebugging },
	{ kCommandErrorHandling,	kStr_MenuErrorHandling },
#if HAS_TRACER
	{ kCommandTracing,			kStr_MenuTracing },
#endif
	{ kCommandSkins,			kStr_MenuSkins },
	{ kCommandHostFS,			kStr_MenuHostFS },
	{ kCommandBreakpoints,		kStr_MenuBreakpoints },

	{ kCommandGremlinsNew,		kStr_MenuGremlinsNew },
	{ kCommandGremlinsSuspend,	kStr_MenuGremlinsSuspend },
	{ kCommandGremlinsStep,		kStr_MenuGremlinsStep },
	{ kCommandGremlinsResume,	kStr_MenuGremlinsResume },
	{ kCommandGremlinsStop,		kStr_MenuGremlinsStop },
	{ kCommandEventReplay,		kStr_MenuEventReplay },
	{ kCommandEventMinimize,	kStr_MenuEventMinimize },

#if HAS_PROFILING
	{ kCommandProfileStart,		kStr_MenuProfileStart },
	{ kCommandProfileStop,		kStr_MenuProfileStop },
	{ kCommandProfileDump,		kStr_MenuProfileDump },
#endif

	{ kCommandAbout,			kStr_MenuAbout },
	{ kCommandQuit,				kStr_MenuQuit },

	{ kCommandFile,				kStr_MenuFile },
	{ kCommandEdit,				kStr_MenuEdit },
	{ kCommandGremlins,			kStr_MenuGremlins },
#if HAS_PROFILING
	{ kCommandProfile,			kStr_MenuProfile },
#endif

	{ kCommandOpen,				kStr_MenuOpen },
	{ kCommandImport,			kStr_MenuImport },
	{ kCommandSettings,			kStr_MenuSettings },

	{ __________,				kStr_MenuBlank }
};


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ MenuInitialize
// ---------------------------------------------------------------------------
// Create all of our menu hierarchies.

void MenuInitialize (Bool alternateLayout)
{
	::PrvInitializeMenus (alternateLayout);

	gChangeCount	= 1;

	gPrefs->AddNotification (&::PrvPrefsChanged, kPrefKeyPSF_MRU);
	gPrefs->AddNotification (&::PrvPrefsChanged, kPrefKeyPRC_MRU);
}


// ---------------------------------------------------------------------------
//		¥ MenuFindMenu
// ---------------------------------------------------------------------------
// Find the top-level menu with the given menu ID and return a pointer to it.
// If the menu ID is unknown, this function returns NULL.

EmMenu* MenuFindMenu (EmMenuID menuID)
{
	EmMenu*	menu = NULL;

	menuID = ::PrvPreferredMenuID (menuID);

	switch (menuID)
	{
		case kMenuMenubarFull:				menu = &gMenuMenubar;					break;
		case kMenuMenubarPartiallyBound:	menu = NULL;							break;
		case kMenuMenubarFullyBound:		menu = NULL;							break;

		case kMenuPopupMenuFull:			menu = &gMenuPopupMenu;					break;
		case kMenuPopupMenuPartiallyBound:	menu = &gMenuPartiallyBoundPopupMenu;	break;
		case kMenuPopupMenuFullyBound:		menu = &gMenuFullyBoundPopupMenu;		break;

		default:
			EmAssert (false);
	}

	return menu;
}


// ---------------------------------------------------------------------------
//		¥ MenuFindMenuItemByCommandID
// ---------------------------------------------------------------------------
// Find the menu item in the given menu (or menu hierarchy if recurse is true)
// that has the given command ID.  If more than one menu item has that ID, the
// first one found is returned.  If no menu item has that ID, return NULL.

EmMenuItem* MenuFindMenuItemByCommandID (EmMenuItemList& menu,
										 EmCommandID cmd,
										 Bool recurse)
{
	EmMenuItemList::iterator	iter = menu.begin ();
	while (iter != menu.end ())
	{
		if (iter->GetCommand () == cmd)
		{
			return &(*iter);
		}

		if (recurse)
		{
			// Check the children by recursing.

			EmMenuItemList&	subMenu = iter->GetChildren ();
			EmMenuItem*	result = ::MenuFindMenuItemByCommandID (subMenu, cmd, recurse);
			if (result)
				return result;
		}

		++iter;
	}

	return NULL;
}


// ---------------------------------------------------------------------------
//		¥ MenuFindMenuContainingCommandID
// ---------------------------------------------------------------------------
// Return the menu containing the menu item that has the given command ID.
// If no such menu item can be found, return NULL.

EmMenuItemList* MenuFindMenuContainingCommandID (EmMenuItemList& menu, EmCommandID cmd)
{
	// Iterate over the given menu, looking for an item with the given command.

	EmMenuItemList::iterator	iter = menu.begin ();
	while (iter != menu.end ())
	{
		// If we found the item, return the menu it's in.

		if (iter->GetCommand () == cmd)
			return &menu;

		// Check the children by recursing.

		EmMenuItemList&	subMenu = iter->GetChildren ();
		EmMenuItemList*	result = ::MenuFindMenuContainingCommandID (subMenu, cmd);
		if (result)
			return result;

		// Move to the next item in the menu.

		++iter;
	}

	// If we reach here, we couldn't find it.  Indicate that by returning NULL.

	return NULL;
}


// ---------------------------------------------------------------------------
//		¥ MenuUpdateMruMenus
// ---------------------------------------------------------------------------
// Update the MRU menu items according to the current preferences.

void MenuUpdateMruMenus (EmMenu& menu)
{
	if (gChangeCount != menu.GetChangeCount ())
	{
		menu.SetChangeCount (gChangeCount);

		EmFileRefList	session, database;
		::PrvCollectFiles (session, database);

		::PrvUpdateMruMenus (session, kCommandSessionOpenOther, menu);
		::PrvUpdateMruMenus (database, kCommandImportOther, menu);
	}
}


// ---------------------------------------------------------------------------
//		¥ MenuUpdateMenuItemStatus
// ---------------------------------------------------------------------------

void MenuUpdateMenuItemStatus (EmMenuItemList& menu)
{
	EmMenuItemList::iterator iter = menu.begin ();
	while (iter != menu.end ())
	{
		EmMenuItemList&	children = iter->GetChildren ();

		if (children.size () > 0)
		{
			iter->SetIsActive (true);
			::MenuUpdateMenuItemStatus (children);
		}
		else
		{
			iter->SetIsActive (::PrvGetItemStatus (*iter));
		}

		++iter;
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ PrvInitializeMenus
// ---------------------------------------------------------------------------
// Create all of our menu hierarchies.

void PrvInitializeMenus (Bool alternateLayout)
{
	// Create the menus.  Create all the distinct menus first.
	// We'll build up the hierarchy later.
	
	// Create the "File" sub-menu.

	EmMenuItemList	subMenuFile;
	::PrvAddMenuItem (subMenuFile, kCommandSessionNew);
	::PrvAddMenuItem (subMenuFile, kCommandOpen);
	::PrvAddMenuItem (subMenuFile, kCommandSessionClose);
	::PrvAddMenuItem (subMenuFile, __________);
	::PrvAddMenuItem (subMenuFile, kCommandSessionSave);
	::PrvAddMenuItem (subMenuFile, kCommandSessionSaveAs);
	::PrvAddMenuItem (subMenuFile, kCommandSessionBound);
	::PrvAddMenuItem (subMenuFile, kCommandScreenSave);
	::PrvAddMenuItem (subMenuFile, kCommandSessionInfo);
	::PrvAddMenuItem (subMenuFile, __________);
	::PrvAddMenuItem (subMenuFile, kCommandImport);
	::PrvAddMenuItem (subMenuFile, kCommandExport);
	::PrvAddMenuItem (subMenuFile, kCommandHotSync);
	::PrvAddMenuItem (subMenuFile, kCommandReset);
	::PrvAddMenuItem (subMenuFile, kCommandDownloadROM);
	
	if (!alternateLayout)
	{
		::PrvAddMenuItem (subMenuFile, __________);
		::PrvAddMenuItem (subMenuFile, kCommandQuit);
	}

	// Create the "Edit" sub-menu.

	EmMenuItemList	subMenuEdit;
	::PrvAddMenuItem (subMenuEdit, kCommandUndo);
	::PrvAddMenuItem (subMenuEdit, __________);
	::PrvAddMenuItem (subMenuEdit, kCommandCut);
	::PrvAddMenuItem (subMenuEdit, kCommandCopy);
	::PrvAddMenuItem (subMenuEdit, kCommandPaste);
	::PrvAddMenuItem (subMenuEdit, kCommandClear);

	::PrvAddMenuItem (subMenuEdit, __________);
	// Strictly speaking, we should not include the Preferences
	// menu item when using the Aqua interface.  Apple prefers
	// that that menu item be in the "application" menu under
	// the About menu item.  However, we have many Preferenc-like
	// menu items.  It would be nice to have them all in one
	// place.  However, I don't know how to put them all into
	// the "application" menu, so I'll keep them all in the Edit
	// menu.  But there will also be the standard Preferences
	// menu item in the "application" menu, which will do the
	// same thing as the Preferences menu item in the Edit menu.
#if (UNIVERSAL_INTERFACES_VERSION >= 0x0340)
//	if (!alternateLayout)
#endif
	{
		::PrvAddMenuItem (subMenuEdit, kCommandPreferences);
	}
	::PrvAddMenuItem (subMenuEdit, kCommandLogging);
	::PrvAddMenuItem (subMenuEdit, kCommandDebugging);
	::PrvAddMenuItem (subMenuEdit, kCommandErrorHandling);
#if HAS_TRACER
	::PrvAddMenuItem (subMenuEdit, kCommandTracing);
#endif
	::PrvAddMenuItem (subMenuEdit, kCommandSkins);
	::PrvAddMenuItem (subMenuEdit, kCommandHostFS);
	::PrvAddMenuItem (subMenuEdit, kCommandBreakpoints);

	// Create the "Gremlins" sub-menu.

	EmMenuItemList	subMenuGremlins;
	::PrvAddMenuItem (subMenuGremlins, kCommandGremlinsNew);
	::PrvAddMenuItem (subMenuGremlins, __________);
//	::PrvAddMenuItem (subMenuGremlins, kCommandGremlinsSuspend);
	::PrvAddMenuItem (subMenuGremlins, kCommandGremlinsStep);
	::PrvAddMenuItem (subMenuGremlins, kCommandGremlinsResume);
	::PrvAddMenuItem (subMenuGremlins, kCommandGremlinsStop);
	::PrvAddMenuItem (subMenuGremlins, __________);
	::PrvAddMenuItem (subMenuGremlins, kCommandEventReplay);
	::PrvAddMenuItem (subMenuGremlins, kCommandEventMinimize);

	// Create the "Profile" sub-menu.

#if HAS_PROFILING
	EmMenuItemList	subMenuProfile;
	::PrvAddMenuItem (subMenuProfile, kCommandProfileStart);
	::PrvAddMenuItem (subMenuProfile, kCommandProfileStop);
	::PrvAddMenuItem (subMenuProfile, __________);
	::PrvAddMenuItem (subMenuProfile, kCommandProfileDump);
#endif

	// Create the "Open Session" sub-menu.

	EmMenuItemList	subMenuOpen;
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpenOther);
	::PrvAddMenuItem (subMenuOpen, __________);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen0);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen1);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen2);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen3);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen4);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen5);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen6);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen7);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen8);
	::PrvAddMenuItem (subMenuOpen, kCommandSessionOpen9);

	// Create the "Install Database" sub-menu.

	EmMenuItemList	subMenuImport;
	::PrvAddMenuItem (subMenuImport, kCommandImportOther);
	::PrvAddMenuItem (subMenuImport, __________);
	::PrvAddMenuItem (subMenuImport, kCommandImport0);
	::PrvAddMenuItem (subMenuImport, kCommandImport1);
	::PrvAddMenuItem (subMenuImport, kCommandImport2);
	::PrvAddMenuItem (subMenuImport, kCommandImport3);
	::PrvAddMenuItem (subMenuImport, kCommandImport4);
	::PrvAddMenuItem (subMenuImport, kCommandImport5);
	::PrvAddMenuItem (subMenuImport, kCommandImport6);
	::PrvAddMenuItem (subMenuImport, kCommandImport7);
	::PrvAddMenuItem (subMenuImport, kCommandImport8);
	::PrvAddMenuItem (subMenuImport, kCommandImport9);

	// Create the "Settings" sub-menu.

	EmMenuItemList	subMenuSettings;
	::PrvAddMenuItem (subMenuSettings, kCommandPreferences);
	::PrvAddMenuItem (subMenuSettings, kCommandLogging);
	::PrvAddMenuItem (subMenuSettings, kCommandDebugging);
	::PrvAddMenuItem (subMenuSettings, kCommandErrorHandling);
#if HAS_TRACER
	::PrvAddMenuItem (subMenuSettings, kCommandTracing);
#endif
	::PrvAddMenuItem (subMenuSettings, kCommandSkins);
	::PrvAddMenuItem (subMenuSettings, kCommandHostFS);
	::PrvAddMenuItem (subMenuSettings, kCommandBreakpoints);

	// Create the Full menubar.

	::PrvAddMenuItem (gMenuMenubar, kCommandFile);
	::PrvAddMenuItem (gMenuMenubar, kCommandEdit);
	::PrvAddMenuItem (gMenuMenubar, kCommandGremlins);
#if HAS_PROFILING
	::PrvAddMenuItem (gMenuMenubar, kCommandProfile);
#endif

	// Create the Full popup menu.

	::PrvAddMenuItem (gMenuPopupMenu, kCommandQuit);
	::PrvAddMenuItem (gMenuPopupMenu, __________);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandSessionNew);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandOpen);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandSessionClose);
	::PrvAddMenuItem (gMenuPopupMenu, __________);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandSessionSave);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandSessionSaveAs);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandSessionBound);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandScreenSave);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandSessionInfo);
	::PrvAddMenuItem (gMenuPopupMenu, __________);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandImport);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandExport);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandHotSync);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandReset);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandDownloadROM);
	::PrvAddMenuItem (gMenuPopupMenu, __________);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandGremlins);
#if HAS_PROFILING
	::PrvAddMenuItem (gMenuPopupMenu, kCommandProfile);
#endif
	::PrvAddMenuItem (gMenuPopupMenu, kCommandSettings);
	::PrvAddMenuItem (gMenuPopupMenu, __________);
	::PrvAddMenuItem (gMenuPopupMenu, kCommandAbout);

	// Create the Partially-Bound popup menu.
	// We remove:
	//		Close
	//		Save Bound
	//		Download ROM
	//		Gremlins
	//		Profile
	//		All settings bu Preferences and Skins

	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandQuit);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandSessionNew);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandOpen);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandSessionSave);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandSessionSaveAs);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandScreenSave);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandSessionInfo);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandImport);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandExport);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandHotSync);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandReset);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandPreferences);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandSkins);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuPartiallyBoundPopupMenu, kCommandAbout);

	// Create the Fully-Bound popup menu.
	// We remove Partially Bound items plus:
	//		SessionNew
	//		Open
	//		SessionSave
	//		SessionSaveAs
	//		SessionInfo
	//		Export

	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, kCommandQuit);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, kCommandScreenSave);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, kCommandImport);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, kCommandHotSync);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, kCommandReset);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, kCommandPreferences);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, kCommandSkins);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, __________);
	::PrvAddMenuItem (gMenuFullyBoundPopupMenu, kCommandAbout);

	// Set sub-menus.

	::PrvSetSubMenu (subMenuFile, kCommandOpen, subMenuOpen);
	::PrvSetSubMenu (subMenuFile, kCommandImport, subMenuImport);

	::PrvSetSubMenu (gMenuMenubar, kCommandFile, subMenuFile);
	::PrvSetSubMenu (gMenuMenubar, kCommandEdit, subMenuEdit);
	::PrvSetSubMenu (gMenuMenubar, kCommandGremlins, subMenuGremlins);
#if HAS_PROFILING
	::PrvSetSubMenu (gMenuMenubar, kCommandProfile, subMenuProfile);
#endif

	::PrvSetSubMenu (gMenuPopupMenu, kCommandOpen, subMenuOpen);
	::PrvSetSubMenu (gMenuPopupMenu, kCommandImport, subMenuImport);
	::PrvSetSubMenu (gMenuPopupMenu, kCommandGremlins, subMenuGremlins);
#if HAS_PROFILING
	::PrvSetSubMenu (gMenuPopupMenu, kCommandProfile, subMenuProfile);
#endif
	::PrvSetSubMenu (gMenuPopupMenu, kCommandSettings, subMenuSettings);

	::PrvSetSubMenu (gMenuPartiallyBoundPopupMenu, kCommandOpen, subMenuOpen);
	::PrvSetSubMenu (gMenuPartiallyBoundPopupMenu, kCommandImport, subMenuImport);

	::PrvSetSubMenu (gMenuFullyBoundPopupMenu, kCommandImport, subMenuImport);
}


// ---------------------------------------------------------------------------
//		¥ PrvAddMenuItem
// ---------------------------------------------------------------------------
// Look up the template for the given command ID and use it to create a new
// menu item, adding it to the given menu.

void PrvAddMenuItem (EmMenuItemList& menu, EmCommandID cmd)
{
	// Get the template for this menu item.

	EmPrvMenuItem*	prvMenuItem = NULL;

	for (size_t ii = 0; ii < countof (kPrvMenuItems); ++ii)
	{
		if (kPrvMenuItems[ii].fCommandID == cmd)
		{
			prvMenuItem = &kPrvMenuItems[ii];
			break;
		}
	}

	EmAssert (prvMenuItem);

	// Create the new menu item.

	EmMenuItem		menuItem (prvMenuItem->fTitle, prvMenuItem->fCommandID);

	// Add it to the menu.

	menu.push_back (menuItem);
}


// ---------------------------------------------------------------------------
//		¥ PrvSetSubMenu
// ---------------------------------------------------------------------------
// Find the menu item with the given command and set its children.

void PrvSetSubMenu (EmMenuItemList& menu, EmCommandID cmd, EmMenuItemList& subMenu)
{
	// Find the menu item with the given command ID.

	EmMenuItem* menuItem = ::MenuFindMenuItemByCommandID (menu, cmd, false);
	EmAssert (menuItem);

	// If we found it, set it's sub-menu to the given menu.
	
	if (menuItem)
	{
		menuItem->SetChildren (subMenu);
	}
}


// ---------------------------------------------------------------------------
//		¥ PrvUpdateMruMenus
// ---------------------------------------------------------------------------
// Update the MRU menu items according to the current preferences.

void PrvUpdateMruMenus (const EmFileRefList& files,
						 EmCommandID identifyingMenuItem,
						 EmMenuItemList& menu)
{
	EmMenuItemList*	subMenu = ::MenuFindMenuContainingCommandID (menu, identifyingMenuItem);
	if (!subMenu)
		return;

	// Clear out the old items.

	subMenu->erase (subMenu->begin () + 2, subMenu->end ());

	// Install the files from "files".

	if (files.size () > 0)
	{
		int								index = 1;
		EmCommandID						cmd = ++identifyingMenuItem;
		EmFileRefList::const_iterator	fileIter = files.begin ();

		while (fileIter != files.end ())
		{
			string		fileName (fileIter->GetFullPath ());
#if !PLATFORM_MAC
			// On Unix and Windows, the MRU items are prefixed with
			// numbers that serve as mnemonic keys.

			if (index <= 10)
			{
				char	buffer[4];
				sprintf (buffer, "&%d ", index < 10 ? index : 0);
				fileName = buffer + fileName;
			}
#endif

			EmMenuItem	newItem (fileName, cmd);
			subMenu->push_back (newItem);

			++fileIter;
			++cmd;
			++index;
		}
	}

	// If no files, set the first one to be "(Empty)" and inactive

	else
	{
		EmMenuItem	emptyItem (kStr_MenuEmpty, kCommandEmpty);
		emptyItem.SetIsActive (false);
		subMenu->push_back (emptyItem);
	}
}


// ---------------------------------------------------------------------------
//		¥ PrvCollectFiles
// ---------------------------------------------------------------------------
// Collect the MRU files from the preferences system.

void PrvCollectFiles (EmFileRefList& session, EmFileRefList& database)
{
	int				ii;

	// Get the MRU list of session files.

	for (ii = 0; ii < 10; ++ii)
	{
		EmFileRef	ref = gEmuPrefs->GetIndRAMMRU (ii);

		if (!ref.IsSpecified ())
			break;

		session.push_back (ref);
	}

	// Clear out that list, and get the list of database files.

	for (ii = 0; ii < 10; ++ii)
	{
		EmFileRef	ref = gEmuPrefs->GetIndPRCMRU (ii);

		if (!ref.IsSpecified ())
			break;

		database.push_back (ref);
	}
}


// ---------------------------------------------------------------------------
//		¥ PrvGetItemStatus
// ---------------------------------------------------------------------------
// Return whether the given menu item should be enabled or not.

Bool PrvGetItemStatus (const EmMenuItem& item)
{
	EmCommandID	id = item.GetCommand ();

	switch (id)
	{
		case kCommandNone:	return false;

		case kCommandSessionNew:		return true;
		case kCommandSessionOpenOther:	return true;
		case kCommandSessionOpen0:		return true;
		case kCommandSessionOpen1:		return true;
		case kCommandSessionOpen2:		return true;
		case kCommandSessionOpen3:		return true;
		case kCommandSessionOpen4:		return true;
		case kCommandSessionOpen5:		return true;
		case kCommandSessionOpen6:		return true;
		case kCommandSessionOpen7:		return true;
		case kCommandSessionOpen8:		return true;
		case kCommandSessionOpen9:		return true;
		case kCommandSessionClose:		return gSession != NULL;

		case kCommandSessionSave:		return gSession != NULL;
		case kCommandSessionSaveAs:		return gSession != NULL;
#if PLATFORM_WINDOWS
		case kCommandSessionBound:		return gSession != NULL;
#else
		case kCommandSessionBound:		return false;
#endif
		case kCommandScreenSave:		return gSession != NULL;
		case kCommandSessionInfo:		return gSession != NULL;

		case kCommandImportOther:		return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport0:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport1:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport2:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport3:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport4:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport5:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport6:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport7:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport8:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandImport9:			return gSession != NULL && EmPatchState::UIInitialized ();
		case kCommandExport:			return gSession != NULL && EmPatchState::UIInitialized ();

		case kCommandHotSync:			return gSession != NULL;
		case kCommandReset:				return gSession != NULL;
		case kCommandDownloadROM:		return true;

		case kCommandUndo:				return false;
		case kCommandCut:				return false;
		case kCommandCopy:				return false;
		case kCommandPaste:				return false;
		case kCommandClear:				return false;

		case kCommandPreferences:		return true;
		case kCommandLogging:			return true;
		case kCommandDebugging:			return true;
		case kCommandErrorHandling:		return true;
#if HAS_TRACER
		case kCommandTracing:			return true;
#endif
		case kCommandSkins:				return true;
		case kCommandHostFS:			return true;
		case kCommandBreakpoints:		return true;

		case kCommandGremlinsNew:		return gSession != NULL && Hordes::CanNew ();
		case kCommandGremlinsSuspend:	return gSession != NULL && Hordes::CanSuspend ();
		case kCommandGremlinsStep:		return gSession != NULL && Hordes::CanStep ();
		case kCommandGremlinsResume:	return gSession != NULL && Hordes::CanResume ();
		case kCommandGremlinsStop:		return gSession != NULL && Hordes::CanStop ();
		case kCommandEventReplay:		return true;
		case kCommandEventMinimize:		return true;

#if HAS_PROFILING
		case kCommandProfileStart:		return gSession != NULL && ::ProfileCanStart ();
		case kCommandProfileStop:		return gSession != NULL && ::ProfileCanStop ();
		case kCommandProfileDump:		return gSession != NULL && ::ProfileCanDump ();
#endif

		case kCommandAbout:				return true;
		case kCommandQuit:				return true;

		case kCommandEmpty:				return false;

		case kCommandFile:				return true;
		case kCommandEdit:				return true;
		case kCommandGremlins:			return true;
#if HAS_PROFILING
		case kCommandProfile:			return true;
#endif

		case kCommandRoot:				return true;
		case kCommandOpen:				return true;
		case kCommandImport:			return true;
		case kCommandSettings:			return true;

		case kCommandDivider:			return false;
	}

	return false;
}


// ---------------------------------------------------------------------------
//		¥ PrvGetItemStatus
// ---------------------------------------------------------------------------
// Respond to the preference change by invalidating one of our flags.  This
// flag will be inspected later when getting a menu to display.

void PrvPrefsChanged (PrefKeyType key, void*)
{
	if (strcmp (key, kPrefKeyPSF_MRU) == 0)
	{
		++gChangeCount;
	}
	else if (strcmp (key, kPrefKeyPRC_MRU) == 0)
	{
		++gChangeCount;
	}
}


// ---------------------------------------------------------------------------
//		¥ PrvPreferredMenuID
// ---------------------------------------------------------------------------
// Translate the given menu ID into the one we really should use.  For the
// most cases, that merely means returning the given ID.  However, for
// kMenuMenubarPreferred and kMenuPopupMenuPreferred, that means checking
// our "bound" state and returning the actual menu to use.

EmMenuID PrvPreferredMenuID (EmMenuID id)
{
	switch (id)
	{
		case kMenuMenubarPreferred:
			if (gApplication->IsBoundPartially ())	return kMenuMenubarPartiallyBound;
			if (gApplication->IsBoundFully ())		return kMenuMenubarPartiallyBound;
													return kMenuMenubarFull;
		case kMenuMenubarFull:						return kMenuMenubarFull;
		case kMenuMenubarPartiallyBound:			return kMenuMenubarPartiallyBound;
		case kMenuMenubarFullyBound:				return kMenuMenubarFullyBound;

		case kMenuPopupMenuPreferred:
			if (gApplication->IsBoundPartially ())	return kMenuPopupMenuPartiallyBound;
			if (gApplication->IsBoundFully ())		return kMenuPopupMenuFullyBound;
													return kMenuPopupMenuFull;
		case kMenuPopupMenuFull:					return kMenuPopupMenuFull;
		case kMenuPopupMenuPartiallyBound:			return kMenuPopupMenuPartiallyBound;
		case kMenuPopupMenuFullyBound:				return kMenuPopupMenuFullyBound;

		case kMenuNone:
			break;
	}

	return kMenuNone;
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmMenuItem::EmMenuItem
// ---------------------------------------------------------------------------

EmMenuItem::EmMenuItem (void) :
	fTitle			(),
	fShortcut		(0),
	fCommand		(kCommandNone),
	fFlagActive		(false),
	fFlagChecked	(false),
	fFlagDivider	(true),
	fChildren		()
{
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::EmMenuItem
// ---------------------------------------------------------------------------

EmMenuItem::EmMenuItem (const EmMenuItem& other) :
	fTitle			(other.fTitle),
	fShortcut		(other.fShortcut),
	fCommand		(other.fCommand),
	fFlagActive		(other.fFlagActive),
	fFlagChecked	(other.fFlagChecked),
	fFlagDivider	(other.fFlagDivider),
	fChildren		(other.fChildren)
{
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::EmMenuItem
// ---------------------------------------------------------------------------

EmMenuItem::EmMenuItem (StrCode str, EmCommandID cmd) :
	fTitle			(this->ExtractTitle (Platform::GetString (str))),
	fShortcut		(this->ExtractShortcut (Platform::GetString (str))),
	fCommand		(cmd),
	fFlagActive		(true),
	fFlagChecked	(false),
	fFlagDivider	(cmd == kCommandDivider),
	fChildren		()
{
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::EmMenuItem
// ---------------------------------------------------------------------------

EmMenuItem::EmMenuItem (const string& title, EmCommandID cmd) :
	fTitle			(this->ExtractTitle (title)),
	fShortcut		(this->ExtractShortcut (title)),
	fCommand		(cmd),
	fFlagActive		(true),
	fFlagChecked	(false),
	fFlagDivider	(cmd == kCommandDivider),
	fChildren		()
{
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::~EmMenuItem
// ---------------------------------------------------------------------------

EmMenuItem::~EmMenuItem (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::GetTitle
// ---------------------------------------------------------------------------

string EmMenuItem::GetTitle (void) const
{
	return fTitle;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::GetShortcut
// ---------------------------------------------------------------------------

char EmMenuItem::GetShortcut (void) const
{
	return fShortcut;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::GetCommand
// ---------------------------------------------------------------------------

EmCommandID EmMenuItem::GetCommand (void) const
{
	return fCommand;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::GetChildren
// ---------------------------------------------------------------------------

EmMenuItemList& EmMenuItem::GetChildren (void)
{
	return fChildren;
}

const EmMenuItemList& EmMenuItem::GetChildren (void) const
{
	return fChildren;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::GetIsActive
// ---------------------------------------------------------------------------

Bool EmMenuItem::GetIsActive (void) const
{
	return fFlagActive;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::GetIsChecked
// ---------------------------------------------------------------------------

Bool EmMenuItem::GetIsChecked (void) const
{
	return fFlagChecked;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::GetIsDivider
// ---------------------------------------------------------------------------

Bool EmMenuItem::GetIsDivider (void) const
{
	return fFlagDivider;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::SetTitle
// ---------------------------------------------------------------------------

void EmMenuItem::SetTitle (const string& title)
{
	fTitle = this->ExtractTitle (title);
	fShortcut = this->ExtractShortcut (title);
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::SetShortcut
// ---------------------------------------------------------------------------

void EmMenuItem::SetShortcut (char shortcut)
{
	fShortcut = shortcut;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::SetCommand
// ---------------------------------------------------------------------------

void EmMenuItem::SetCommand (EmCommandID command)
{
	fCommand = command;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::SetChildren
// ---------------------------------------------------------------------------

void EmMenuItem::SetChildren (const EmMenuItemList& children)
{
	fChildren = children;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::SetIsActive
// ---------------------------------------------------------------------------

void EmMenuItem::SetIsActive (Bool active)
{
	fFlagActive = active;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::SetIsChecked
// ---------------------------------------------------------------------------

void EmMenuItem::SetIsChecked (Bool checked)
{
	fFlagChecked = checked;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::SetIsDivider
// ---------------------------------------------------------------------------

void EmMenuItem::SetIsDivider (Bool divider)
{
	fFlagDivider = divider;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::ExtractTitle
// ---------------------------------------------------------------------------

string EmMenuItem::ExtractTitle (const string& title) const
{
	string	result (title);

	// Look for any TAB, indicating a shortcut character.

	string::size_type	tabPos = result.find ('\t');
	if (tabPos != string::npos)
	{
		result.erase (tabPos, 2);
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmMenuItem::ExtractShortcut
// ---------------------------------------------------------------------------

char EmMenuItem::ExtractShortcut (const string& title) const
{
	char	result (0);

	// Look for any TAB, indicating a shortcut character.

	string::size_type	tabPos = title.find ('\t');
	if (tabPos != string::npos)
	{
		result = title[tabPos + 1];
	}

	return result;
}
