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
#include "EmWindow.h"

#include "EmHAL.h"				// EmHAL::GetVibrateOn
#include "EmJPEG.h"				// JPEGToPixMap
#include "EmPixMap.h"			// EmPixMap
#include "EmQuantizer.h"		// EmQuantizer
#include "EmRegion.h"			// EmRegion
#include "EmScreen.h"			// EmScreenUpdateInfo
#include "EmSession.h"			// PostPenEvent, PostButtonEvent, etc.
#include "EmStream.h"			// delete imageStream
#include "Platform.h"			// Platform::PinToScreen

#include "PHEMNativeIF.h"

EmWindow*	gWindow;

// ---------------------------------------------------------------------------
//		¥ EmWindow::EmWindow
// ---------------------------------------------------------------------------
// Constructor.  Perform simple data member initialization.  The heavy stuff
// occurs in WindowCreate.

EmWindow::EmWindow (void) :
	fSkinBase (),
	fSkinCurrent (),
	fSkinColors (),
	fSkinRegion (),
	fPrevLCDColors (),
	fCurrentButton (kElement_None),
	fNeedWindowReset (false),
	fNeedWindowInvalidate (false),
	fOldLCDOn (false),
	fOldBacklightOn (false),
	fOldLEDState (0),
	fWiggled (false),
	fActive (true),
	fDebugMode (false),
	fGremlinMode (false)
{
	EmAssert (gWindow == NULL);
	gWindow = this;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::~EmWindow
// ---------------------------------------------------------------------------
// Dispose of the window and all its data.  Unregister for notification, save
// the window location, and destroy the host window object.

EmWindow::~EmWindow (void)
{
	EmAssert (gWindow == this);
	gWindow = NULL;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PreDestroy
// ---------------------------------------------------------------------------
// Carry out actions to be performed just before the window is closed.  These
// actions would normally be performed in EmWindow's destructor, except that
// the host window is already deleted by then.  Therefore, each sub-class of
// EmWindow needs to call this in *its* destructor before destroying the host
// window.

void EmWindow::PreDestroy (void)
{
	gPrefs->RemoveNotification (EmWindow::PrefsChangedCB);

	// Save the window location.

	EmRect	rect (this->HostWindowBoundsGet ());

	if (!rect.IsEmpty ())
	{
		Preference <EmPoint>	pref (kPrefKeyWindowLocation);
		pref = rect.TopLeft ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::WindowInit
// ---------------------------------------------------------------------------
// Create the LCD window, along with any host data and host window objects.
// Set the window's skin, register for any notification regarding the window's
// appearance, reposition the window to where it should live, ensure that
// the window is still on the screen, and then finally make the window
// visible.
//
// This function should be called immediately after the EmWindow object is
// created.

void EmWindow::WindowInit (void)
{
	// Establish the window's skin.

	this->WindowReset ();

	// Install notification callbacks for when the skin changes.

	gPrefs->AddNotification (&EmWindow::PrefsChangedCB, kPrefKeySkins, this);
	gPrefs->AddNotification (&EmWindow::PrefsChangedCB, kPrefKeyScale, this);
	gPrefs->AddNotification (&EmWindow::PrefsChangedCB, kPrefKeyDimWhenInactive, this);
	gPrefs->AddNotification (&EmWindow::PrefsChangedCB, kPrefKeyShowDebugMode, this);
	gPrefs->AddNotification (&EmWindow::PrefsChangedCB, kPrefKeyShowGremlinMode, this);
	gPrefs->AddNotification (&EmWindow::PrefsChangedCB, kPrefKeyBackgroundColor, this);
	gPrefs->AddNotification (&EmWindow::PrefsChangedCB, kPrefKeyStayOnTop, this);

	// Restore the window location.

	Preference<EmPoint>	pref (kPrefKeyWindowLocation);

	if (pref.Loaded ())
	{
		this->HostWindowMoveTo (*pref);
	}

	// No previously saved position, so center it.

	else
	{
		this->HostWindowCenter ();
	}

	// Ensure the window is on the screen.

	EmRect	rect (this->HostWindowBoundsGet ());

	if (Platform::PinToScreen (rect))
	{
		this->HostWindowMoveTo (rect.TopLeft ());
	}

	// Show the window.

	this->HostWindowShow ();

	// Make the window stay on top if it is set as such.

	this->HostWindowStayOnTop ();
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::WindowReset
// ---------------------------------------------------------------------------

void EmWindow::WindowReset (void)
{
	// Can only set the skin if we have a session running, so that we can
	// query it for its configuration.

	if (!gSession)
		return;

	// Configure our Skin engine.

	::SkinSetSkin ();

	// Get the specified skin, or the default skin if the specified
	// one cannot be found.

	if (!this->GetSkin (fSkinBase))
	{
		this->GetDefaultSkin (fSkinBase);
	}

	// Create a one-bpp mask of the skin.

	EmPixMap	mask;
	fSkinBase.CreateMask (mask);

	// Convert it to a region.

	fSkinRegion = mask.CreateRegion ();

	// Clear our color caches.  They'll get filled again on demand.

	for (int ii = 0; ii < (long) countof (fSkinColors); ++ii)
	{
		fSkinColors[ii].clear ();
	}

	// Clear the image of the skin, altered for its mode.  This image
	// will get regenerated on demand.

	fSkinCurrent = EmPixMap ();

	// Clear out the previously saved LCD colors.  This will force the
	// any host palettes to be regenerated, because they'll think the
	// Palm OS LCD palette has changed.

	fPrevLCDColors.clear ();

	// Now convert this all to platform-specific data structures.
	// This will also resize the window.

	this->HostWindowReset ();
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HandlePenEvent
// ---------------------------------------------------------------------------

void EmWindow::HandlePenEvent (const EmPoint& where, Bool down)
{
	// Find out what part of the case we clicked on.
        if (down) { 
          PHEM_Log_Msg("HandlePenEvent, down");
        } else {
          PHEM_Log_Msg("HandlePenEvent, up");
        }
        PHEM_Log_Place(where.fX);
        PHEM_Log_Place(where.fY);

	SkinElementType	what;

	// If the mouse button is down and we aren't currently tracking
	// anything, then find out what the user clicked on.

	if (down && (fCurrentButton == kElement_None))
	{
		what = ::SkinTestPoint (where);
                PHEM_Log_Place(what);

		if ((what != kElement_Frame) && (what != kElement_None))
		{
			this->HostMouseCapture ();
		}
	}

	// If the pen is up, or if we were already in the progress of tracking
	// something, then stick with it.

	else
	{
		what = fCurrentButton;
	}

	// If we clicked in the touchscreen area (which includes the LCD area
	// and the silkscreen area), start the mechanism for tracking the mouse.

	if (what == kElement_Touchscreen)
	{
		EmPoint		whereLCD = ::SkinWindowToTouchscreen (where);
		EmPenEvent	event (whereLCD, down);

		EmAssert (gSession);
		gSession->PostPenEvent (event);
	}

	// If we clicked in the frame, start dragging the window.

	else if (what == kElement_Frame)
	{
		if (down)
		{
			this->HostWindowDrag ();

			// We normally record the mouse button going up in
			// EventMouseUp.  However, that method is not called
			// after ClickInDrag, so say the mouse button is up here.

			down = false;
		}
	}

	// Otherwise, if we clicked on a button, start tracking it.

	else if (what != kElement_None)
	{
		EmButtonEvent	event (what, down);

		EmAssert (gSession);
		gSession->PostButtonEvent (event);
	}

	// Set or clear what we're tracking.

	if (down)
		fCurrentButton = what;
	else
		fCurrentButton = kElement_None;

	if (fCurrentButton == kElement_None)
	{
		this->HostMouseRelease ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HandleUpdate
// ---------------------------------------------------------------------------
// Called to handle update events.

void EmWindow::HandleUpdate (void)
{
	this->HostUpdateBegin ();

	this->PaintScreen (true, true);

	this->HostUpdateEnd ();
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HandlePalette
// ---------------------------------------------------------------------------
// Called when we need to tell the OS what our preferred palette is.  Usually
// called when window orders change.

void EmWindow::HandlePalette (void)
{
	this->HostPalette ();
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HandleDisplayChange
// ---------------------------------------------------------------------------
// Called when the display depth changes.

void EmWindow::HandleDisplayChange (void)
{
	this->HostDisplayChange ();
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HandleActivate
// ---------------------------------------------------------------------------
// Called when the display depth changes.

void EmWindow::HandleActivate (Bool active)
{
	if (fActive != active)
	{
		fActive = active;
		this->HostWindowReset ();

		fSkinCurrent = EmPixMap ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HandleDebugMode
// ---------------------------------------------------------------------------
// Called when the display depth changes.

void EmWindow::HandleDebugMode (Bool debugMode)
{
	if (fDebugMode != debugMode)
	{
		fDebugMode = debugMode;
		this->HostWindowReset ();

		fSkinCurrent = EmPixMap ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HandleGremlinMode
// ---------------------------------------------------------------------------
// Called when the display depth changes.

void EmWindow::HandleGremlinMode (Bool gremlinMode)
{
	if (fGremlinMode != gremlinMode)
	{
		fGremlinMode = gremlinMode;
		this->HostWindowReset ();

		fSkinCurrent = EmPixMap ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HandleIdle
// ---------------------------------------------------------------------------
// Called several times a second to perform idle time events such as
// refreshing the LCD area and vibrating the window.

void EmWindow::HandleIdle (void)
{
	// Get the current mouse position.
        //PHEM_Log_Msg("Window:HandleIdle...");
#if 0
// Not sure what the purpose of this is. Seems to just make trouble,
// generating extra presses on scroll bars and such.
	EmPoint	where = this->HostGetCurrentMouse ();

	// Track the pen if it's down.

	if (fCurrentButton == kElement_Touchscreen)
	{
           //PHEM_Log_Msg("In touchscreen, generating extra event...");
	  this->HandlePenEvent (where, true);
	}
#endif
	// Refresh the LCD area.

        //PHEM_Log_Msg("HostDrawingBegin.");
	this->HostDrawingBegin ();

	if (fNeedWindowReset)
	{
                PHEM_Log_Msg("WindowReset.");
		this->WindowReset ();
	}
	else if (fNeedWindowInvalidate)
	{
                //PHEM_Log_Msg("WindowInvalidate.");
		this->PaintScreen (false, true);
	}
	else
	{
                //PHEM_Log_Msg("!WindowInvalidate.");
		this->PaintScreen (false, false);
	}

	fNeedWindowReset		= false;
	fNeedWindowInvalidate	= false;

        //PHEM_Log_Msg("HostDrawingEnd.");
	this->HostDrawingEnd ();


#if 0
// On Android, we actually have a vibration motor
	// Do the Wiggle Walk.

	{
		const int	kWiggleOffset = 2;

		EmSessionStopper	stopper (gSession, kStopNow);
                PHEM_Log_Msg("Wiggle?");

		if (stopper.Stopped ())
		{
			if (EmHAL::GetVibrateOn ())
			{
                                PHEM_Log_Msg("Wiggling.");
				if (!fWiggled)
				{
					fWiggled = true;
					this->HostWindowMoveBy (EmPoint (kWiggleOffset, 0));
				}
				else
				{
					fWiggled = false;
					this->HostWindowMoveBy (EmPoint (-kWiggleOffset, 0));
				}
			}

			// If the vibrator just went off, then put the window
			// back to where it was.

			else if (fWiggled)
			{
				fWiggled = false;
				this->HostWindowMoveBy (EmPoint (-kWiggleOffset, 0));
			}
		}
	}
#else
       // Android hosts have vibration motors, usually.
       {
          EmSessionStopper stopper (gSession, kStopNow);

          if (stopper.Stopped ()) {
            if (EmHAL::GetVibrateOn()) {
              if (!fWiggled) {
                fWiggled = true;
                PHEM_Begin_Vibration();
              }
            } else {
              if (fWiggled) {
                fWiggled = false;
                PHEM_End_Vibration();
              }
            }
          }
       }
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetLCDContents
// ---------------------------------------------------------------------------
// Return the full contents of the LCD area.  Used for screenshots.

void EmWindow::GetLCDContents (EmScreenUpdateInfo& info)
{
        PHEM_Log_Msg("GetLCDContents");
	EmSessionStopper	stopper (gSession, kStopNow);

	if (stopper.Stopped ())
	{
                PHEM_Log_Msg("All");
		EmScreen::InvalidateAll ();
                PHEM_Log_Msg("GetBits");
		EmScreen::GetBits (info);
                PHEM_Log_Msg("All");
		EmScreen::InvalidateAll ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PaintScreen
// ---------------------------------------------------------------------------
// Paint the entire window.  Could be called in response to an update event
// or as part of our regular idle time handling.

void EmWindow::PaintScreen (Bool drawCase, Bool wholeLCD)
{
	EmScreenUpdateInfo	info;
	Bool				bufferDirty;
	Bool				drawFrame = false;
	Bool				drawLED = false;
	Bool				drawLCD = false;
	Bool				lcdOn;
	Bool				backlightOn;
	uint16				ledState;

	{
                //PHEM_Log_Msg("PaintScreen");
		// Pause the CPU so that we can get the current hardware state.

		EmSessionStopper	stopper (gSession, kStopNow);

		if (!stopper.Stopped ())
			return;

		// Determine if we have to force the redrawing of the background.
		// We have to do that if the LCD or LEDs have turned off.

		lcdOn		= EmHAL::GetLCDScreenOn ();
		backlightOn	= EmHAL::GetLCDBacklightOn ();
		ledState	= EmHAL::GetLEDState ();

		if (!lcdOn && fOldLCDOn)
		{
			drawCase = true;
		}

		if (!ledState && fOldLEDState)
		{
			drawCase = true;
		}

		// Determine if we have to draw the LCD or LED.  We'd have to do that
		// if their state has changed or if we had to draw the background.

		if (drawCase || (lcdOn != fOldLCDOn) || (backlightOn != fOldBacklightOn))
		{
			drawLCD		= lcdOn;
		}

		if (drawCase || (lcdOn != fOldLCDOn))
		{
			drawFrame	= lcdOn && EmHAL::GetLCDHasFrame ();
		}

                //PHEM_Log_Msg("some vars set");
		if (drawCase || (ledState != fOldLEDState))
		{
			drawLED		= ledState != 0;
                        PHEM_Enable_LED(drawLED);
		}

		fOldLCDOn		= lcdOn;
		fOldBacklightOn	= backlightOn;
		fOldLEDState	= ledState;

		// If we're going to be drawing the whole LCD, then invalidate
		// the entire screen.

                //PHEM_Log_Msg("Checking invalidate");
		if (wholeLCD || drawLCD)
		{
			EmScreen::InvalidateAll ();
		}

		// Get the current LCD state.

                //PHEM_Log_Msg("GetBits");
		bufferDirty = EmScreen::GetBits (info);

		// If the buffer was dirty, that's another reason to draw the LCD.

		if (bufferDirty)
		{
			drawLCD		= lcdOn;
		}

		// If there is no width or height to the buffer, then that's a
		// great reason to NOT draw the LCD.

		EmPoint	size = info.fImage.GetSize ();
		if (size.fX == 0 || size.fY == 0)
		{
			drawLCD		= false;
		}
	}

                //PHEM_Log_Msg("Set up palette");
	// Set up the graphics palette.

	Bool	drawAnything = drawCase || drawFrame || drawLED || drawLCD;

	if (drawAnything)
	{
		this->HostPaletteSet (info);
	}


	// Draw the case.

                //PHEM_Log_Msg("Drawcase");
	if (drawCase)
	{
		this->PaintCase (info);
	}

	// Draw any LCD frame.

                //PHEM_Log_Msg("Frame");
	if (drawFrame)
	{
		this->PaintLCDFrame (info);
	}

	// Draw any LEDs.
                //PHEM_Log_Msg("LED");

	if (drawLED)
	{
		this->PaintLED (ledState);
	}

	// Draw the LCD area.

                //PHEM_Log_Msg("LCD");
	if (drawLCD)
	{
		this->PaintLCD (info);
	}

#if 0
	// Draw frames around the buttons.	This is handy when determining their
	// locations initially.

	if (drawCase)
	{
		this->PaintButtonFrames ();
	}
#endif

	// Restore the palette that was changed in HostSetPalette.
                //PHEM_Log_Msg("Restore palette");

	if (drawAnything)
	{
		this->HostPaletteRestore ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PaintCase
// ---------------------------------------------------------------------------
// Draw the skin.

void EmWindow::PaintCase (const EmScreenUpdateInfo& info)
{
	this->HostPaintCase (info);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PaintLCDFrame
// ---------------------------------------------------------------------------
// Draw any frame around the LCD area required in order to faithfully
// emulate the appearance of the real LCD.

void EmWindow::PaintLCDFrame (const EmScreenUpdateInfo&)
{
	RGBType	backlightColor	(0xff, 0xff, 0xff);
	EmRect	lcdRect			(this->GetLCDBounds ());
	EmPoint	penSize			(2, 2);

	penSize = ::SkinScaleUp (penSize);

	lcdRect.Inset (-penSize.fX, -penSize.fY);

	this->HostRectFrame (lcdRect, penSize, backlightColor);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PaintLCD
// ---------------------------------------------------------------------------
// Draw the LCD portion of the window.

void EmWindow::PaintLCD (const EmScreenUpdateInfo& info)
{
	// Get the bounds of the LCD area.

	EmRect	lcdRect = this->GetLCDBounds ();

	// Limit the vertical range to the lines that have changed.

	EmRect	destRect = ::SkinScaleDown (lcdRect);

	destRect.fBottom	= destRect.fTop + info.fLastLine;
	destRect.fTop		= destRect.fTop + info.fFirstLine;

	destRect = ::SkinScaleUp (destRect);

	// Get the bounds of the area we'll be blitting from.

	EmRect	srcRect (destRect);
	srcRect -= lcdRect.TopLeft ();

	// Determine if the image needs to be scaled.

	EmPoint	before (1, 1);
	EmPoint	after = ::SkinScaleUp (before);

	// Setup is done. Let the host-specific routines handle the rest.

	this->HostPaintLCD (info, srcRect, destRect, before.fX != after.fX);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PaintLED
// ---------------------------------------------------------------------------

void EmWindow::PaintLED (uint16 ledState)
{
	// Get the color for the LED.

	RGBType	ledColor;

	if ((ledState & (kLEDRed | kLEDGreen)) == (kLEDRed | kLEDGreen))
	{
		ledColor = RGBType (255, 128, 0);	// Orange
	}
	else if ((ledState & kLEDRed) == kLEDRed)
	{
		ledColor = RGBType (255, 0, 0);		// Red
	}
	else /* green */
	{
		ledColor = RGBType (0, 255, 0);		// Green
	}

#if 0
	EmRect	bounds (this->GetLEDBounds ());

	this->HostOvalPaint (bounds, ledColor);
#else
  PHEM_Set_LED(ledColor);
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PaintButtonFrames
// ---------------------------------------------------------------------------
// Draw frames around the buttons.	This is handy when determining their
// locations initially.

void EmWindow::PaintButtonFrames (void)
{
	EmPoint			penSize (1, 1);
	RGBType			color (0, 0, 0);

	int				index = 0;
	SkinElementType	type;
	EmRect			bounds;

	while (::SkinGetElementInfo (index, type, bounds))
	{
		this->HostRectFrame (bounds, penSize, color);

		++index;
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PrefsChangedCB
// ---------------------------------------------------------------------------
// Respond to a preference change.  This version of the function retrieves
// the EmWindow object pointer and calls a non-static member function of the
// object.

void EmWindow::PrefsChangedCB (PrefKeyType key, void* data)
{
        PHEM_Log_Msg("PrefsChangedCB called.");
	EmAssert (data);

	EmWindow* lcd = static_cast<EmWindow*>(data);
	lcd->PrefsChanged (key);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::PrefsChanged
// ---------------------------------------------------------------------------
// Respond to a preference change.  If the skin or display size has changed,
// we need to rebuild our skin image.  If the background color preference
// has changed, then just flag that the LCD area needs to be redrawn.

void EmWindow::PrefsChanged (PrefKeyType key)
{
	if (::PrefKeysEqual (key, kPrefKeyScale) ||
		::PrefKeysEqual (key, kPrefKeySkins) ||
		::PrefKeysEqual (key, kPrefKeyShowDebugMode) ||
		::PrefKeysEqual (key, kPrefKeyShowGremlinMode) ||
		::PrefKeysEqual (key, kPrefKeyBackgroundColor))
	{
		fNeedWindowReset = true;
	}
	else if (::PrefKeysEqual (key, kPrefKeyBackgroundColor))
	{
		fNeedWindowInvalidate = true;
	}
	else if (::PrefKeysEqual (key, kPrefKeyStayOnTop))
	{
		this->HostWindowStayOnTop ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetLCDBounds
// ---------------------------------------------------------------------------
// Get the bounds for the LCD.

EmRect EmWindow::GetLCDBounds (void)
{
	int				ii = 0;
	SkinElementType	type = kElement_None;
	EmRect			bounds (10, 10, 170, 170);

	while (::SkinGetElementInfo (ii, type, bounds))
	{
		if (type == kElement_LCD)
		{
			break;
		}

		++ii;
	}

	return bounds;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetLEDBounds
// ---------------------------------------------------------------------------
// Get the bounds for the LED.  Put it in the power button.

EmRect EmWindow::GetLEDBounds (void)
{
	int				ii = 0;
	SkinElementType	type = kElement_None;
	Bool			found = false;
	EmRect			bounds (10, 10, 20, 20);

	while (::SkinGetElementInfo (ii++, type, bounds))
	{
		if (type == kElement_LED)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		ii = 0;
		while (::SkinGetElementInfo (ii++, type, bounds))
		{
			if (type == kElement_PowerButton)
			{
				// Create a square centered in the power button.

				if (bounds.Width () > bounds.Height ())
				{
					bounds.fLeft	= bounds.fLeft + (bounds.Width () - bounds.Height ()) / 2;
					bounds.fRight	= bounds.fLeft + bounds.Height ();
				}
				else
				{
					bounds.fTop		= bounds.fTop + (bounds.Height () - bounds.Width ()) / 2;
					bounds.fBottom	= bounds.fTop + bounds.Width ();
				}

				// Inset it a little -- looks better this way.

				bounds.Inset (2, 2);

				break;
			}
		}
	}

	return bounds;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetSkin
// ---------------------------------------------------------------------------
// Get the currently selected skin as a PixMap.

Bool EmWindow::GetSkin (EmPixMap& pixMap)
{
	EmStream*	imageStream = ::SkinGetSkinStream ();

	if (!imageStream)
		return false;

	// Turn the JPEG image into BMP format.

	::JPEGToPixMap (*imageStream, pixMap);

	// Free up the resource info.

	delete imageStream;
	imageStream = NULL;

	return true;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetDefaultSkin
// ---------------------------------------------------------------------------
// Get the default, built-in skin as a PixMap.

void EmWindow::GetDefaultSkin (EmPixMap& pixMap)
{
	Preference<ScaleType>	scalePref (kPrefKeyScale);

        PHEM_Log_Msg("GetDefaultSkin");
        PHEM_Log_Place(*scalePref);
	EmAssert ((*scalePref == 1) || (*scalePref == 2));

	this->HostGetDefaultSkin (pixMap, *scalePref);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmWindow::PrevLCDColorsChanged
// ---------------------------------------------------------------------------

Bool EmWindow::PrevLCDColorsChanged (const RGBList& newLCDColors)
{
	// If the incoming image does not have a color table, then it's a
	// direct-pixel image.  It can have any set of colors in it, so
	// we'll have to create an 8-bit palette for it no matter what.
	//
	// If the incoming image's color table has a different size than
	// the previously saved color table, then we'll need to regenerate
	// our cached palette.

	if (newLCDColors.size () == 0 ||
		newLCDColors.size () != fPrevLCDColors.size ())
	{
		return true;
	}

	// The incoming color table size is the same as the previously saved
	// color table size, and neither of them are zero.  Check the contents
	// of the two tables to see if they've changed.

	for (size_t ii = 0; ii < newLCDColors.size (); ++ii)
	{
		if (newLCDColors[ii] != fPrevLCDColors[ii])
		{
			return true;
		}
	}

	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::SaveLCDColors
// ---------------------------------------------------------------------------

void EmWindow::SaveLCDColors (const RGBList& newLCDColors)
{
	fPrevLCDColors = newLCDColors;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetSystemColors
// ---------------------------------------------------------------------------

void EmWindow::GetSystemColors (const EmScreenUpdateInfo& info, RGBList& colors)
{
	// If this is a 1, 2, or 4 bit LCD, then we can use the whole
	// palette from it along with our beefy skin color table.

	if (info.fImage.GetDepth () <= 4)
	{
		colors = info.fImage.GetColorTable ();
		colors.insert (colors.end (),
			this->GetCurrentSkinColors (false).begin (),
			this->GetCurrentSkinColors (false).end ());
	}

	// If this is an 8-bit image, then we have an interesting problem.  We
	// want to generate a palette that we can cache until the Palm OS
	// palette changes.  The palette we generate shouldn't be based on the
	// current image, as that can change and use different colors without
	// the Palm OS palette changing.  So if we were to take that route,
	// the palette we generated would be based on the handful of colors on
	// the screen and not what's in the palette.  It would be nice to use
	// the Palm OS palette itself, but there are 256 colors in there, and
	// we need to cut that down a little so that we can share time with
	// the skin.  Therefore, we run the Palm OS palette itself through the
	// color quantizer itself to generate a nice subset of the palette.
	// Since we have to merge 84 or so colors together, and since all
	// colors in the Palm OS palette are weighted evenly, we don't want
	// those 84 merged colors to appear at one end of the spectrum or the
	// other.  Therefore, we'll randomize the Palm OS palette before
	// offering its colors to the quantizer.

	else if (info.fImage.GetDepth () == 8)
	{
		srand (1);	// Randomize it consistantly...  :-)

		colors = info.fImage.GetColorTable ();

		for (size_t ii = 0; ii < colors.size (); ++ii)
		{
			size_t	jj = rand () % colors.size ();

			swap (colors[ii], colors[jj]);
		}

		EmQuantizer q (172, 6);
		q.ProcessColorTable (colors);
		q.GetColorTable (colors);

		colors.insert (colors.end (),
			this->GetCurrentSkinColors (true).begin (),
			this->GetCurrentSkinColors (true).end ());
	}

	// Otherwise, if this is a 16 or 24 bit LCD, then we have to
	// get the best 172 colors and combine that with our 64-entry
	// skin color table.

	else
	{
		EmQuantizer q (172, 6);
		q.ProcessImage (info.fImage);
		q.GetColorTable (colors);

		colors.insert (colors.end (),
			this->GetCurrentSkinColors (true).begin (),
			this->GetCurrentSkinColors (true).end ());
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::QuantizeSkinColors
// ---------------------------------------------------------------------------

void EmWindow::QuantizeSkinColors (const EmPixMap& skin, RGBList& colors, Bool polite)
{
	if (polite)
	{
		EmQuantizer q (64, 6);	// Leaves 256 - 64 - 20 = 172 for LCD
		q.ProcessImage (skin);
		q.GetColorTable (colors);
	}
	else
	{
		EmQuantizer q (220, 6);	// Leaves 256 - 220 - 20 = 16 for LCD
		q.ProcessImage (skin);
		q.GetColorTable (colors);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetCurrentSkin
// ---------------------------------------------------------------------------

const EmPixMap& EmWindow::GetCurrentSkin (void)
{
	if (fSkinCurrent.GetSize () == EmPoint (0, 0))
	{
		fSkinCurrent = fSkinBase;

		Preference<bool>	prefDimWhenInactive (kPrefKeyDimWhenInactive);
		Preference<bool>	prefShowDebugMode (kPrefKeyShowDebugMode);
		Preference<bool>	prefShowGremlinMode (kPrefKeyShowGremlinMode);

		if (!fActive && *prefDimWhenInactive)
			fSkinCurrent.ChangeTone (40);

		if (fDebugMode && *prefShowDebugMode)
			fSkinCurrent.ConvertToColor (1);

		else if (fGremlinMode && *prefShowGremlinMode)
			fSkinCurrent.ConvertToColor (2);
	}

	return fSkinCurrent;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetCurrentSkinColors
// ---------------------------------------------------------------------------

const RGBList& EmWindow::GetCurrentSkinColors (Bool polite)
{
	int	index = 0;

	if (fDebugMode)
		index |= 0x0001;

	else if (fGremlinMode)
		index |= 0x0002;

	if (fActive)
		index |= 0x0004;

	if (polite)
		index |= 0x0008;

	RGBList&	result = fSkinColors[index];

	if (result.empty ())
	{
		const EmPixMap&	skin = this->GetCurrentSkin ();

		this->QuantizeSkinColors (skin, result, polite);
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::GetCurrentSkinRegion
// ---------------------------------------------------------------------------

const EmRegion& EmWindow::GetCurrentSkinRegion (void)
{
	return fSkinRegion;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmWindow::HostWindowReset
// ---------------------------------------------------------------------------
// Update the window's appearance due to a skin change.

void EmWindow::HostWindowReset (void)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostMouseCapture
// ---------------------------------------------------------------------------
// Capture the mouse so that all mouse events get sent to this window.

void EmWindow::HostMouseCapture (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostMouseRelease
// ---------------------------------------------------------------------------
// Release the mouse so that mouse events get sent to the window the
// cursor is over.

void EmWindow::HostMouseRelease (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostUpdateBegin
// ---------------------------------------------------------------------------
// Prepare the host window object for updating.

void EmWindow::HostUpdateBegin (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostUpdateEnd
// ---------------------------------------------------------------------------
// Finalize the host window object after it's been updated.

void EmWindow::HostUpdateEnd (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostDrawingBegin
// ---------------------------------------------------------------------------
// Prepare the host window object for drawing outside of an update event.

void EmWindow::HostDrawingBegin (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostDrawingEnd
// ---------------------------------------------------------------------------
// Finalize the host window object after drawing outside of an update event.

void EmWindow::HostDrawingEnd (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostPaletteSet
// ---------------------------------------------------------------------------
// Establish the palette to be used for drawing.

void EmWindow::HostPaletteSet (const EmScreenUpdateInfo&)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostPaletteRestore
// ---------------------------------------------------------------------------
// Clean up after HostPaletteSet.

void EmWindow::HostPaletteRestore (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostWindowMoveBy
// ---------------------------------------------------------------------------
// Move the host window object by the given offset.

void EmWindow::HostWindowMoveBy (const EmPoint&)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostWindowMoveTo
// ---------------------------------------------------------------------------
// Move the host window object to the given location.

void EmWindow::HostWindowMoveTo (const EmPoint&)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostWindowBoundsGet
// ---------------------------------------------------------------------------
// Get the global bounds of the host window object.

EmRect EmWindow::HostWindowBoundsGet (void)
{
	EmAssert (false);
	return EmRect (0, 0, 0, 0);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostWindowCenter
// ---------------------------------------------------------------------------
// Center the window to the main display.

void EmWindow::HostWindowCenter (void)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostWindowShow
// ---------------------------------------------------------------------------
// Make the host window object visible.

void EmWindow::HostWindowShow (void)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostWindowStayOnTop
// ---------------------------------------------------------------------------
// Make the host window stay on top.

void EmWindow::HostWindowStayOnTop (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostWindowDrag
// ---------------------------------------------------------------------------
// The user has clicked in a region of the host window object that causes
// the window to be dragged.  Drag the window around.

void EmWindow::HostWindowDrag (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostRectFrame
// ---------------------------------------------------------------------------
// Draw a rectangle frame with the given width in the given color.

void EmWindow::HostRectFrame (const EmRect&, const EmPoint&, const RGBType&)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostOvalPaint
// ---------------------------------------------------------------------------
// Fill an oval with the given color.

void EmWindow::HostOvalPaint (const EmRect&, const RGBType&)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostPaintCase
// ---------------------------------------------------------------------------
// Draw the skin.

void EmWindow::HostPaintCase (const EmScreenUpdateInfo&)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostPaintLCD
// ---------------------------------------------------------------------------
// Draw the LCD area.

void EmWindow::HostPaintLCD (const EmScreenUpdateInfo&, const EmRect&,
						  const EmRect&, Bool)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostPalette
// ---------------------------------------------------------------------------
// Tell the system about the palette we want to use.  Called when Windows
// is updating its system palette.

void EmWindow::HostPalette (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostDisplayChange
// ---------------------------------------------------------------------------
// Respond to the display's bit depth changing.  All we do here is flush our
// caches of the skin information.  It will get regenerated when it's needed.

void EmWindow::HostDisplayChange (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostGetDefaultSkin
// ---------------------------------------------------------------------------
// Get the default (built-in) skin image.

void EmWindow::HostGetDefaultSkin (EmPixMap&, int)
{
	EmAssert (false);
}


// ---------------------------------------------------------------------------
//		¥ EmWindow::HostGetCurrentMouse
// ---------------------------------------------------------------------------
// Get the current mouse location.

EmPoint EmWindow::HostGetCurrentMouse (void)
{
	EmAssert (false);
	return EmPoint (0, 0);
}
