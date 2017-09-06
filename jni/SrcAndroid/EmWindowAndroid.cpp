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
#include "EmWindowAndroid.h"

#include "EmApplication.h"		// gApplication
#include "EmCommands.h"			// EmCommandID
#include "EmDocument.h"			// EmDocument
#include "EmPixMapAndroid.h"		// ConvertPixMapToHost
#include "EmScreen.h"			// EmScreenUpdateInfo
#include "EmSession.h"			// EmKeyEvent, EmButtonEvent
#include "EmWindow.h"			// EmWindow
#include "Platform.h"			// Platform::AllocateMemory

//#include "EmMenusFltk.h"		// HostCreatePopupMenu

#include <ctype.h>				// isprint, isxdigit

/* Update for GCC 4 */
#include <string.h>

#include "PHEMNativeIF.h"

#include "DefaultSmall.xpm"
#include "DefaultLarge.xpm"

const int kDefaultWidth = 220;
const int kDefaultHeight = 330;


EmWindowAndroid* gHostWindow;

// ---------------------------------------------------------------------------
//		¥ EmWindow::NewWindow
// ---------------------------------------------------------------------------

EmWindow* EmWindow::NewWindow (void)
{
	// This is the type of window we should create.  However, on Unix, we
	// create one and only one window when we create the application.  This
	// method -- called by the document to create its window -- therefore
	// doesn't need to actually create a window.

	return new EmWindowAndroid;

	EmAssert (gHostWindow != NULL);
	return NULL;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::EmWindowAndroid
// ---------------------------------------------------------------------------

EmWindowAndroid::EmWindowAndroid(void) :
//	Fl_Window (kDefaultWidth, kDefaultHeight, "pose"),
//	fMessage (NULL),
//	fCachedSkin (NULL),
	EmWindow ()
{
	EmAssert (gHostWindow == NULL);
	gHostWindow = this;
#if 0
	this->box (FL_FLAT_BOX);
	this->color (fl_gray_ramp (FL_NUM_GRAY - 1));

	// Install a function to get called when the window is closed
	// via a WM_DELETE_WINDOW message.

	this->callback (&EmWindowAndroid::CloseCallback, NULL);

	// Ensure that the user can't resize this window.

	this->resizable (NULL);

	// Create the message to display when there's no session running.

	fMessage = new Fl_Box (0, 0, 200, 40,
						   "Right click on this window to show a menu of commands.");
	fMessage->box (FL_NO_BOX);
	fMessage->align (FL_ALIGN_CENTER | FL_ALIGN_WRAP | FL_ALIGN_INSIDE);

	this->end ();

	this->redraw (); // Redraw help message

	// Set the X-Windows window class.  Normally, this is done when
	// Fl_Window::show (argc, argv) is called (in main()).  However, the
	// EmWindow class gets in the way and automatically shows the host
	// window by calling Fl_Window::show().  The latter does not set the
	// window class.  And even when the other "show" method is called later,
	// the damage is done:  the X Windows window is already created with a
	// NULL window class.
	//
	// Setting the window class to the title of the window is a workaround
	// at best.  In previous versions of Poser, the window class would be
	// set to the name of the executable.  These should both be "pose",
	// but it's possible for them to be different.

	this->xclass (this->label ());
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::~EmWindowAndroid
// ---------------------------------------------------------------------------

EmWindowAndroid::~EmWindowAndroid (void)
{
#if 0
	this->PreDestroy ();

	// Get rid of the cached skin.

	this->CacheFlush ();
#endif
	EmAssert (gHostWindow == this);
	gHostWindow = NULL;
}


#if 0
// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::draw
// ---------------------------------------------------------------------------

void EmWindowAndroid::draw (void)
{
	if (gDocument)
	{
		this->HandleUpdate ();
	}
	else
	{
		fl_color (255, 255, 255);
		fl_rect (0, 0, this->w (), this->h ());

		fMessage->position (
			(this->w () - fMessage->w ()) / 2,
			(this->h () - fMessage->h ()) / 3);
		this->draw_child (*fMessage);
	}
}
#endif


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::handle
//  The event-handling code. Mapping keys is the complicated part.
// ---------------------------------------------------------------------------

int EmWindowAndroid::handle(PHEM_Event *evt) {

  //PHEM_Log_Msg("EmWindowAndroid::handle().");
  //PHEM_Log_Place(evt->x_pos);
  //PHEM_Log_Place(evt->y_pos);
  EmPoint where(evt->x_pos, evt->y_pos);

  switch (evt->type) {
    case PHEM_PEN_DOWN:
    case PHEM_PEN_MOVE:
	this->HandlePenEvent (where, true);
	return 1;
       
    case PHEM_PEN_UP:
	this->HandlePenEvent (where, false);
	return 1;

    case PHEM_KEY:
        // AndroidTODO
        break;
    default:
        PHEM_Log_Msg("Hit default in window:handle()!");
        break;
  }
  return 0;
}

#if 0
// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::handle
// ---------------------------------------------------------------------------

int EmWindowAndroid::handle (int event)
{
	EmPoint	where (Fl::event_x(), Fl::event_y());

	switch (event)
	{
		case FL_PUSH:
			if (Fl::event_button () == 3)
			{
				this->PopupMenu ();
				return 1;
			}
			// Fall through...

		case FL_DRAG:
			this->HandlePenEvent (where, true);
			return 1;

		case FL_RELEASE:
			this->HandlePenEvent (where, false);
			return 1;

/*
	These don't seem to get called when we want them to...

		case FL_ACTIVATE:
			this->HandleActivate (true);
			return 1;

		case FL_DEACTIVATE:
			this->HandleActivate (false);
			return 1;
*/

		case FL_FOCUS:
			this->HandleActivate (true);
			return 1;

		case FL_UNFOCUS:
			this->HandleActivate (false);
			return 1;

		case FL_SHORTCUT:
		{
			if (Fl::test_shortcut (FL_F + 10 | FL_SHIFT))
			{
				this->PopupMenu ();
				return 1;
			}

			Fl_Menu_Item_List hostMenu;
			this->GetHostMenu (hostMenu);

			const Fl_Menu_Item* selected = hostMenu[0].test_shortcut ();
			if (selected)
			{
				this->DoMenuCommand (*selected);
				return 1;
			}

			return 0;
		}

		case FL_KEYBOARD:
			if (Fl::event_state (FL_ALT | FL_META))
			{
				// Reserved for shortcuts
				return 0;
			}

			if (gDocument != NULL)
			{
				// Handle printable characters.

				if (strlen (Fl::event_text ()) > 0)
				{
					int key = (unsigned char) Fl::event_text()[0];
					EmKeyEvent event (key);
					// !!! Need to get modifiers
					gDocument->HandleKey (event);
					return 1;
				}

				// Handle all other characters.

				int c = Fl::event_key ();

				struct KeyConvert
				{
					int fEventKey;
					SkinElementType fButton;
					int fKey;
				};

				KeyConvert kConvert[] =
				{
					{ FL_Enter,		kElement_None, chrLineFeed },
					{ FL_KP_Enter,	kElement_None, chrLineFeed },
					{ FL_Left,		kElement_None, leftArrowChr },
					{ FL_Right,		kElement_None, rightArrowChr },
					{ FL_Up,		kElement_None, upArrowChr },
					{ FL_Down,		kElement_None, downArrowChr },
					{ FL_F + 1,		kElement_App1Button },
					{ FL_F + 2,		kElement_App2Button },
					{ FL_F + 3,		kElement_App3Button },
					{ FL_F + 4,		kElement_App4Button },
					{ FL_F + 9,		kElement_PowerButton },
					{ FL_Page_Up,	kElement_UpButton },
					{ FL_Page_Down,	kElement_DownButton }
				};

				for (size_t ii = 0; ii < countof (kConvert); ++ii)
				{
					if (c == kConvert[ii].fEventKey)
					{
						if (kConvert[ii].fButton != kElement_None)
						{
							gDocument->HandleButton (kConvert[ii].fButton, true);
							gDocument->HandleButton (kConvert[ii].fButton, false);
							return 1;
						}

						if (kConvert[ii].fKey)
						{
							EmKeyEvent event (kConvert[ii].fKey);
							// !!! Need to get modifiers
							gDocument->HandleKey (event);
							return 1;
						}
					}
				}

				if (c == FL_F + 10)
				{
					this->PopupMenu ();
					return 1;
				}

				if (c < 0x100)
				{
					EmKeyEvent event (c);
					// !!! Need to get modifiers
					gDocument->HandleKey (event);
					return 1;
				}
			}

			return 0;
	}

	return Fl_Window::handle (event);
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::PopupMenu
// ---------------------------------------------------------------------------

void EmWindowAndroid::PopupMenu (void)
{
	// Get the menu.

	Fl_Menu_Item_List hostMenu;
	this->GetHostMenu (hostMenu);

	const Fl_Menu_Item* item = hostMenu[0].popup (Fl::event_x (), Fl::event_y ());
	if (item)
	{
		this->DoMenuCommand (*item);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::GetHostMenu
// ---------------------------------------------------------------------------

void EmWindowAndroid::GetHostMenu (Fl_Menu_Item_List& hostMenu)
{
	EmMenu*	menu = ::MenuFindMenu (kMenuPopupMenuPreferred);
	EmAssert (menu);

	::MenuUpdateMruMenus (*menu);
	::MenuUpdateMenuItemStatus (*menu);
	::HostCreatePopupMenu (*menu, hostMenu);
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::DoMenuCommand
// ---------------------------------------------------------------------------

void EmWindowAndroid::DoMenuCommand (const Fl_Menu_Item& item)
{
	EmCommandID id = (EmCommandID) item.argument ();

	if (gDocument)
		if (gDocument->HandleCommand (id))
			return;

	if (gApplication)
		if (gApplication->HandleCommand (id))
			return;

	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::CacheFlush
// ---------------------------------------------------------------------------

void EmWindowAndroid::CacheFlush (void)
{
	delete fCachedSkin;
	fCachedSkin = NULL;
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::CacheFlush
// ---------------------------------------------------------------------------

Fl_Image* EmWindowAndroid::GetSkin (void)
{
	if (!fCachedSkin)
	{
		const EmPixMap& p = this->GetCurrentSkin ();
		EmPoint size = p.GetSize ();

		fCachedSkin = new Fl_RGB_Image ((uchar*) p.GetBits (), size.fX, size.fY,
							   3, p.GetRowBytes ());
	}

	EmAssert (fCachedSkin);

	return fCachedSkin;
}
#endif

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostWindowReset
// ---------------------------------------------------------------------------
// Update the window's appearance due to a skin change.

void EmWindowAndroid::HostWindowReset (void)
{
   // Change the window to accomodate the settings and bitmap.

   // Get the desired client size.
#if 0
   EmRect	newBounds = this->GetCurrentSkinRegion().Bounds ();
   m_width = newBounds.Width();
   m_height = newBounds.Height();
#else
   EmPoint	the_size = this->GetCurrentSkin().GetSize();
   m_width = the_size.fX;
   m_height = the_size.fY;
#endif

   // Protect against this function being called when there's
   // no established skin.

   if (m_width == 0) {
     m_width = kDefaultWidth;
   }

   if (m_height == 0) {
     m_height = kDefaultHeight;
   }

   // Resize the window.
   PHEM_Reset_Window(m_width, m_height);

   this->HandleUpdate ();
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostMouseCapture
// ---------------------------------------------------------------------------
// Capture the mouse so that all mouse events get sent to this window.

void EmWindowAndroid::HostMouseCapture (void)
{
#if 0
	Fl::grab (this);
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostMouseRelease
// ---------------------------------------------------------------------------
// Release the mouse so that mouse events get sent to the window the
// cursor is over.

void EmWindowAndroid::HostMouseRelease (void)
{
#if 0
	Fl::grab (NULL);
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostDrawingBegin
// ---------------------------------------------------------------------------
// Prepare the host window object for drawing outside of an update event.

void EmWindowAndroid::HostDrawingBegin (void)
{
#if 0
	this->make_current ();
#endif
	//PHEM_Log_Msg("HostDrawingBegin?");
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostWindowMoveBy
// ---------------------------------------------------------------------------
// Move the host window object by the given offset.

void EmWindowAndroid::HostWindowMoveBy (const EmPoint& offset)
{
// Not applicable to Android. We don't 'move' Activities.
#if 0
	this->HostWindowMoveTo (this->HostWindowBoundsGet ().TopLeft () + offset);
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostWindowMoveTo
// ---------------------------------------------------------------------------
// Move the host window object to the given location.

void EmWindowAndroid::HostWindowMoveTo (const EmPoint& loc)
{
// Not applicable to Android. We don't 'move' Activities.
#if 0
	this->position (loc.fX, loc.fY);
#endif
}


// Not needed on Android
// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostWindowBoundsGet
// ---------------------------------------------------------------------------
// Get the global bounds of the host window object.

EmRect EmWindowAndroid::HostWindowBoundsGet (void)
{
   if (0 == m_width) {
     m_width = kDefaultWidth;
   }
   if (0 == m_height) {
     m_height = kDefaultHeight;
   }
	return EmRect (0, 0, m_width, m_height);
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostWindowCenter
// ---------------------------------------------------------------------------
// Center the window to the main display.

void EmWindowAndroid::HostWindowCenter (void)
{
// Not applicable to Android. We don't 'move' Activities.
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostWindowShow
// ---------------------------------------------------------------------------
// Make the host window object visible.

void EmWindowAndroid::HostWindowShow (void)
{
// Redudant on Android - don't have multiple overlapping windows
#if 0
	this->show ();
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostRectFrame
// ---------------------------------------------------------------------------
// Draw a rectangle frame with the given width in the given color.

void EmWindowAndroid::HostRectFrame (const EmRect& r, const EmPoint& pen, const RGBType& color)
{
//AndroidTODO
#if 0
	EmRect r2 (r);
	fl_color (color.fRed, color.fGreen, color.fBlue);

	// !!! This could be changed to not assume a square pen, but since
	// we're kind of tied to that on Windows right now, that's the
	// assumption we'll make.

	for (EmCoord size = 0; size < pen.fX; ++size)
	{
		fl_rect (r2.fLeft, r2.fTop, r2.Width (), r2.Height ());
		r2.Inset (1, 1);
	}
#endif
  // Set the flag to tell the Android side to update the screen
  PHEM_Mark_Screen_Updated(4096, -1);
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostOvalPaint
// ---------------------------------------------------------------------------
// Fill an oval with the given color.

void EmWindowAndroid::HostOvalPaint (const EmRect& r, const RGBType& color)
{
//AndroidTODO
#if 0
	fl_color (color.fRed, color.fGreen, color.fBlue);
	fl_pie (r.fLeft, r.fTop, r.Width (), r.Height (), 0, 360);
#endif
  // Set the flag to tell the Android side to update the screen
  PHEM_Mark_Screen_Updated(4096, -1);
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostPaintCase
// ---------------------------------------------------------------------------
// Draw the skin.

void EmWindowAndroid::HostPaintCase (const EmScreenUpdateInfo&)
{
  PHEM_Log_Msg("HostPaintCase");
  // Get the current skin image, blit its pixels to the buffer
  const EmPixMap& p = this->GetCurrentSkin ();
  EmPoint size = p.GetSize ();

  PHEM_Log_Place(size.fY);
  ::ConvertPixMapToHost (p, PHEM_Get_Buffer(), 0, size.fY, false);

//  memcpy(PHEM_Get_Buffer(), (char *)p.GetBits(), size.fX * size.fY * 2); // 2 bytes per pixel

  // Set the flag to tell the Android side to update the screen
  PHEM_Mark_Screen_Updated(0, size.fY);
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostPaintLCD
// ---------------------------------------------------------------------------
// Draw the LCD area.  info contains the raw LCD data, including a partially
// updated fImage, and fFirstList and fLastLine which indicate the valid
// range of the image.  srcRect and destRect also indicate the range that
// needs to be updated, and have also been scaled appropriately.  scaled is
// true if we need to scale info.fImage during the process of converting it
// to a host pixmap.

void EmWindowAndroid::HostPaintLCD (const EmScreenUpdateInfo& info, const EmRect& srcRect,
						  const EmRect& destRect, Bool scaled)
{
	// Determine the buffer size and allocate it.
	// We assume that ConvertPixMapToHost is converting to 16-bit RGB 565.

        int i;
	int		rowBytes	= srcRect.fRight * 2; // 2 bytes per pixel
	int		lcd_bufferSize	= srcRect.fBottom * rowBytes;
	uint8 *	lcd_buffer		= (uint8 *) Platform::AllocateMemory (lcd_bufferSize);
        uint8 * saved_lcd_buffer;
        uint8 * target_buffer = PHEM_Get_Buffer();
        saved_lcd_buffer = lcd_buffer; // so we can free it

        const EmPixMap& p = this->GetCurrentSkin ();
        EmPoint size = p.GetSize ();
#if 0
        PHEM_Log_Msg("HostPaintLCD");
        if (scaled) {
          PHEM_Log_Msg("Scaled");
        } else {
          PHEM_Log_Msg("Unscaled");
        }

        PHEM_Log_Msg("SrcRect");
        PHEM_Log_Place(srcRect.fTop);
        PHEM_Log_Place(srcRect.fLeft);
        PHEM_Log_Place(srcRect.fRight);
        PHEM_Log_Place(srcRect.fBottom);
        PHEM_Log_Place(srcRect.Width());
        PHEM_Log_Place(srcRect.Height());

        PHEM_Log_Msg("DestRect");
        PHEM_Log_Place(destRect.fTop);
        PHEM_Log_Place(destRect.fLeft);
        PHEM_Log_Place(destRect.fRight);
        PHEM_Log_Place(destRect.fBottom);
        PHEM_Log_Place(destRect.Width());
        PHEM_Log_Place(destRect.Height());

        PHEM_Log_Msg("info");
        PHEM_Log_Place(info.fFirstLine);
        PHEM_Log_Place(info.fLastLine);

        PHEM_Log_Msg("skin size");
        PHEM_Log_Place(size.fX);
        PHEM_Log_Place(size.fY);
#endif

	// Convert the image, scaling along the way.

	::ConvertPixMapToHost (info.fImage, lcd_buffer,
				 info.fFirstLine, info.fLastLine, scaled);

	// Draw the converted image.

        // We need to copy line-by-line into the buffer.
        // Position the target_buffer pointer: (lines * bytes in a line) + (bytes inset)
        target_buffer += (destRect.fTop * size.fX * 2) + (destRect.fLeft * 2);
        lcd_buffer += srcRect.fTop * rowBytes;

        for (i=srcRect.fTop; i<srcRect.fBottom; i++) {
	  memcpy(target_buffer, lcd_buffer, rowBytes);
          // increment the pointers
          target_buffer += (size.fX * 2);
          lcd_buffer += rowBytes;
        }
	// Clean up.

	Platform::DisposeMemory (saved_lcd_buffer);
#if 0
  // Set the flag to tell the Android side to update the screen
  if (scaled) {
    PHEM_Mark_Screen_Updated(info.fFirstLine*2, info.fLastLine*2);
  } else {
    PHEM_Mark_Screen_Updated(info.fFirstLine, info.fLastLine);
  }
#else
  PHEM_Mark_Screen_Updated(destRect.fTop, destRect.fBottom);
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostGetDefaultSkin
// ---------------------------------------------------------------------------
// Get the default (built-in) skin image.

void EmWindowAndroid::HostGetDefaultSkin(EmPixMap& pixMap, int scale)
{
        PHEM_Log_Msg("HostGetDefaultSkin");
        PHEM_Log_Place(scale);
	char** xpm = (scale == 2) ? DefaultLarge : DefaultSmall;

	/*
		An XPM file is an array of strings composed of four sections:

				<Values>
				<Colors>
				<Pixels>
				<Extensions>

		Each string is composed of words separated by spaces.
	*/

	/*
		<Values> is a string containing four or six integers in base
		10 that correspond to width, height, number of colors, number
		of characters per pixel, and (optionally) hotspot location.
	*/

	int w = 0;
	int h = 0;
	int num_colors = 0;
	int cpp = 0;
	int hot_x = 0;
	int hot_y = 0;
	int i = sscanf (xpm [0], "%d %d %d %d %d %d", &w, &h, &num_colors,
					&cpp, &hot_x, &hot_y);

	EmAssert (i == 4);
	EmAssert (w > 0);
	EmAssert (h > 0);
	EmAssert (num_colors > 0);
	EmAssert (cpp == 1);
	EmAssert (hot_x == 0);
	EmAssert (hot_y == 0);

	/*
		<Colors> contains as many lines as there are colors.  Each string
		contains the following words:

			<color_code> {<key> <color>}+
	*/

	RGBType colorMap[0x80];

	for (int color_num = 0; color_num < num_colors; ++color_num)
	{
		const char*	this_line	= xpm [1 + color_num];
		int			color_code	= this_line[0];
		char		key[3]		= {0};
		char		color[8]	= {0};

		i = sscanf (this_line + 1 + 1, "%s %s", key, color);

		EmAssert (i == 2);
		EmAssert (strlen (key) == 1);
		EmAssert (strlen (color) == 7);
		EmAssert (key[0] == 'c');
		EmAssert (color[0] == '#');
		EmAssert (isxdigit (color[1]));
		EmAssert (isxdigit (color[2]));
		EmAssert (isxdigit (color[3]));
		EmAssert (isxdigit (color[4]));
		EmAssert (isxdigit (color[5]));
		EmAssert (isxdigit (color[6]));

		int r, g, b;
		i = sscanf (color, "#%2x%2x%2x", &r, &g, &b);

		EmAssert (i == 3);
		EmAssert (isprint (color_code));

		colorMap [color_code] = RGBType (r, g, b);
	}

	/*
		<Pixels> contains "h" lines, each containing cpp * "w"
		characters in them.  Each set of cpp characters maps to
		one of the colors in the <Colors> array.
	*/

	uint8* buffer = (uint8*) Platform::AllocateMemory (w * h * 3);
	uint8* dest = buffer;

	for (int yy = 0; yy < h; ++yy)
	{
		char* src = xpm [1 + num_colors + yy];

		for (int xx = 0; xx < w; ++xx)
		{
			int color_code = *src++;
			EmAssert (isprint (color_code));

			const RGBType& rgb = colorMap [color_code];

			*dest++ = rgb.fRed;
			*dest++ = rgb.fGreen;
			*dest++ = rgb.fBlue;
		}
	}

	EmAssert ((dest - buffer) <= (w * h * 3));

	// We now have the data in RGB format.  Wrap it up in a temporary
	// EmPixMap so that we can copy it into the result EmPixMap.

	EmPixMap	wrapper;

	wrapper.SetSize (EmPoint (w, h));
	wrapper.SetFormat (kPixMapFormat24RGB);
	wrapper.SetRowBytes (w * 3);
	wrapper.SetBits (buffer);

	// Copy the data to the destination.

        //Android: convert to Android buffer format
        PHEM_Log_Msg("Converting skin to 16RGB565");
        wrapper.ConvertToFormat(kPixMapFormat16RGB565);

	pixMap = wrapper;

	// Clean up.

	Platform::DisposeMemory (buffer);
}


// ---------------------------------------------------------------------------
//		¥ EmWindowAndroid::HostGetCurrentMouse
// ---------------------------------------------------------------------------
// Get the current mouse location.

EmPoint EmWindowAndroid::HostGetCurrentMouse (void)
{
return EmPoint(PHEM_mouse_x, PHEM_mouse_y);
}
