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

#ifndef EmMenus_h
#define EmMenus_h

#include "EmCommands.h"			// EmCommandID

#include <string>
#include <vector>


/*
	This file defines the objects and routines for creating, managing,
	and manipulating menubars, menus, and menu items.

	Menu items (EmMenuItem) are the base object clients deal with.  A
	menu item is pretty much what you think it is: a distinct item in
	a menu.  Every menu item has the following properties:
	
		Title
			This is the text that appears on screen.  The title can also
			contain an '&' to indicate any mnemonic character, and can
			optionally be broken into two fields with a TAB character,
			with the second field containing a single character to be
			used as a shortcut or accelerator.  Example:
			
				"&New\tN"
		
		Shortcut
			This is the Cmd, Ctrl, or Alt shortcut character.  It can
			be specified along with the title (which makes associating
			the title and shortcut easy in a string resource) or
			separately via dedicated API.

		Command
			This is the value indicating what menu item was just selected
			and what action should be taken.  There is no rule that says
			all menu items should have unique command numbers, but it makes
			looking up menu items by their command number easier.

		Flags
			A set of Boolean flags.

			Active/Inactive
				Indicates whether the menu item is drawn normally and is
				selectable, or is drawn grayed out and is not selectable.
			
			Checked/Unchecked
				Indicates whether or not the menu item is drawn with a
				check mark next to it.

			Visible/Invisible
				Indicates whether or not the menu item appears in the
				host menu at all.

			Divider
				Menu items marked as dividers appear as dividers or
				separators in the host menu.  The are implicitly inactive
				and unchecked, implicitly have no children or shortcuts,
				and their titles are ignored.
		
		Children
			Each menu item can have zero or more children.  A menu item
			with children is called a hierarchical menu, and the children
			are grouped together in a sub-menu.  Hierarchical menus
			cannot be selected themselves (that is, highlighting one and
			dismissing the menu does not result in a command number being
			generated).

	A menu (EmMenu) is merely an ordered collection of menu items.

	A menubar is merely a menu where all the menu items are hierarchical.
*/

class EmMenuItem;
typedef vector<EmMenuItem>	EmMenuItemList;


// This class is pretty straightforward.  It's an example of what we
// called "structification" at Taligent (and were told not to use).
// The class is mostly data with getters and setters for that data.
// The only non-straightforward parts are:
//
//	*	Calling SetTitle will check to see if a shortcut character
//		is included.  If so, it is removed, stored as the shortcut
//		character, and the remaining string is stored as the title
//		without it.
//
//	*	GetChildren returns a non-const reference to the children.
//		Thus, you can modify the collection directly.  Of course,
//		you can also just replace whatever collection is there by
//		calling SetChildren.

class EmMenuItem
{
	public:
								EmMenuItem		(void);	// Creates a divider
								EmMenuItem		(const EmMenuItem&);
								EmMenuItem		(StrCode, EmCommandID);
								EmMenuItem		(const string&, EmCommandID);
								~EmMenuItem		(void);

		string					GetTitle		(void) const;
		char					GetShortcut		(void) const;
		EmCommandID				GetCommand		(void) const;
		EmMenuItemList&			GetChildren		(void);
		const EmMenuItemList&	GetChildren		(void) const;
		Bool					GetIsActive		(void) const;
		Bool					GetIsChecked	(void) const;
		Bool					GetIsDivider	(void) const;

		void					SetTitle		(const string&);
		void					SetShortcut		(char);
		void					SetCommand		(EmCommandID);
		void					SetChildren		(const EmMenuItemList&);
		void					SetIsActive		(Bool);
		void					SetIsChecked	(Bool);
		void					SetIsDivider	(Bool);

	private:
		string					ExtractTitle	(const string&) const;
		char					ExtractShortcut	(const string&) const;

	private:
		string					fTitle;
		char					fShortcut;
		EmCommandID				fCommand;
		Bool					fFlagActive;
		Bool					fFlagChecked;
		Bool					fFlagDivider;
		EmMenuItemList			fChildren;
};


// An EmMenu is a top-level menu.  It is the same as an EmMenuItemList,
// with the addition of a changecount used as a timestamp.  This
// changecount is used to determine if the menu needs to be updated in
// the face of any changes (such as the MRU lists changing).

class EmMenu : public EmMenuItemList
{
	public:
								EmMenu	(void) : EmMenuItemList () {}
								EmMenu	(const EmMenuItemList& o) : EmMenuItemList (o) {}
								~EmMenu	(void) {}

		unsigned long			GetChangeCount	(void)				{ return fChangeCount; };
		void					SetChangeCount	(unsigned long v)	{ fChangeCount = v; }

	private:
		unsigned long			fChangeCount;
};




enum EmMenuID
{
	kMenuNone,

	kMenuMenubarPreferred,
	kMenuMenubarFull,
	kMenuMenubarPartiallyBound,
	kMenuMenubarFullyBound,

	kMenuPopupMenuPreferred,
	kMenuPopupMenuFull,
	kMenuPopupMenuPartiallyBound,
	kMenuPopupMenuFullyBound
};

void			MenuInitialize					(Bool alternateLayout);
EmMenu*			MenuFindMenu					(EmMenuID);
EmMenuItem*		MenuFindMenuItemByCommandID		(EmMenuItemList&, EmCommandID, Bool recurse);
EmMenuItemList* MenuFindMenuContainingCommandID	(EmMenuItemList&, EmCommandID);
void			MenuUpdateMruMenus				(EmMenu&);
void			MenuUpdateMenuItemStatus		(EmMenuItemList&);

#endif	// EmMenus_h
