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


#ifndef EmDlgAndroid_h
#define EmDlgAndroid_h

#include "EmDlg.h"

#if 0
// For EmDlgFltkFactory.cpp
class Fl_Widget;
void PrvSetWidgetID (Fl_Widget* o, EmDlgItemID id);

// For fltk_main.cpp
void HandleDialogs (void);
void CloseAllDialogs (void);

// Fl_Push_Button is a button that can be configured as the Default
// Button (draws with the Return Arrow and responds to the Return
// key), Cancel Button (responds to the Escape Key), or a Normal
// Button (no special drawing or keyboard handling).

#include <FL/Fl_Return_Button.H>

class Fl_Push_Button : public Fl_Return_Button
{
	public:
		Fl_Push_Button (int x, int y, int w, int h, const char* l = NULL) :
			Fl_Return_Button (x, y, w, h, l),
			fType (kNormal)
		{}

		virtual int				handle				(int);

		void					SetDefaultButton	(void) { fType = kDefault; }
		void					SetCancelButton		(void) { fType = kCancel; }
		void					SetNormalButton		(void) { fType = kNormal; }

	protected:
		virtual void			draw				(void);

	private:
		enum
		{
			kNormal,
			kDefault,
			kCancel
		};

		int						fType;
};

#endif

class PHEM_Widget_Info {
  public:
  EmDlgItemID item_id;
  int visible;
  int enabled;
  int is_default;
  int is_cancel;
  long value;
  char *label;
};

class PHEM_Dialog {
  public:
  PHEM_Widget_Info widgets[5];
};

/*
kDlgItemCmnButton1
kDlgItemCmnButton2
kDlgItemCmnButton3
kDlgItemCmnText
kDlgItemOK
kDlgItemCancel
kDlgItemRstSoft
kDlgItemRstHard
kDlgItemRstDebug
kDlgItemRstNoExt
*/

#endif	/* EmDlgAndroid_h */
