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

#ifndef EmWindow_h
#define EmWindow_h

#include "EmPixMap.h"			// EmPixMap
#include "EmRegion.h"			// EmRegion
#include "EmStructs.h"			// RGBList
#include "PreferenceMgr.h"		// PrefKeyType

/*
	EmWindow is the cross-platform object representing the window that
	displays the emulation.  It is not the actual thing that appears on
	the monitor as a window, but merely deals with cross-platform
	aspects of handling it, such as updating its contents, changing
	skins, etc.

	EmWindow handles all the cross-platform issues.  It can deal with
	the upper-level parts of updating the LCD area, wiggling the window
	if the vibrator is going, changing skins, etc.

	EmWindow manages most of the platform-specific aspects by defining
	HostFoo methods, which are then defined differently on each
	platform.  These HostFoo methods are implemented in
	EmWindow<Host>.cpp.

	EmWindow<Host> is also defined in EmWindow<Host>.cpp, and holds any
	platform-specific data (such as the HWND on Windows, etc.).  It
	declares and implements any support functions needed on that
	particular platform, but which can't be declared in EmWindow because
	either not all platforms need those functions, or they involve
	platform-specific types.

	One creates a window by calling "window = new EmWindow<Host>;"
	followed by "window->WindowInit();".  Cross-platform code can then
	call high-level functions in EmWindow such as HandleUpdate and
	HandlePen.  Platform-specific code can perform platform-specific
	operations.  When it's time to close the window, call
	"window->WindowDispose();" followed by "delete window;"
*/

class EmPoint;
class EmScreenUpdateInfo;

class EmWindow
{
	public:
								EmWindow			(void);
		virtual					~EmWindow			(void);

		static EmWindow*		NewWindow			(void);

		void					WindowInit			(void);
		void					WindowReset			(void);

		void					HandlePenEvent		(const EmPoint&, Bool down);
		void					HandleUpdate		(void);
		void 					HandlePalette		(void);
		void					HandleDisplayChange	(void);
		void					HandleActivate		(Bool active);
		virtual void			HandleIdle			(void);

		void					HandleDebugMode		(Bool debugMode);
		void					HandleGremlinMode	(Bool gremlinMode);

		void					GetLCDContents		(EmScreenUpdateInfo& info);

	private:
		void					PaintScreen			(Bool drawCase, Bool always);
		void					PaintCase			(const EmScreenUpdateInfo& info);
		void					PaintLCDFrame		(const EmScreenUpdateInfo& info);
		void					PaintLCD			(const EmScreenUpdateInfo& info);
		void					PaintLED			(uint16 ledState);
		void					PaintButtonFrames	(void);

		static void				PrefsChangedCB		(PrefKeyType key, void* data);
		void					PrefsChanged		(PrefKeyType key);

		EmRect					GetLCDBounds		(void);
		EmRect					GetLEDBounds		(void);

		Bool					GetSkin				(EmPixMap&);
		void					GetDefaultSkin		(EmPixMap&);

	protected:
		void					PreDestroy			(void);
		Bool					PrevLCDColorsChanged(const RGBList& newLCDColors);
		void					SaveLCDColors		(const RGBList& newLCDColors);
		void					GetSystemColors		(const EmScreenUpdateInfo&,
													 RGBList&);
		void					QuantizeSkinColors	(const EmPixMap& skin,
													 RGBList& colors,
													 Bool polite);

		const EmPixMap&			GetCurrentSkin		(void);
		const RGBList&			GetCurrentSkinColors(Bool polite);
		const EmRegion&			GetCurrentSkinRegion(void);

	private:
		virtual void			HostWindowReset		(void) = 0;

		virtual void			HostMouseCapture	(void);
		virtual void			HostMouseRelease	(void);

		virtual void			HostUpdateBegin		(void);
		virtual void			HostUpdateEnd		(void);

		virtual void			HostDrawingBegin	(void);
		virtual void			HostDrawingEnd		(void);

		virtual void			HostPaletteSet		(const EmScreenUpdateInfo&);
		virtual void			HostPaletteRestore	(void);

		virtual void			HostWindowMoveBy	(const EmPoint&) = 0;
		virtual void			HostWindowMoveTo	(const EmPoint&) = 0;
		virtual EmRect			HostWindowBoundsGet	(void) = 0;
		virtual void			HostWindowCenter	(void) = 0;
		virtual void			HostWindowShow		(void) = 0;
		virtual void			HostWindowDrag		(void);
		virtual void			HostWindowStayOnTop	(void);

		virtual void			HostRectFrame		(const EmRect&, const EmPoint&, const RGBType&) = 0;
		virtual void			HostOvalPaint		(const EmRect&, const RGBType&) = 0;

		virtual void			HostPaintCase		(const EmScreenUpdateInfo&) = 0;
		virtual void			HostPaintLCD		(const EmScreenUpdateInfo& info,
													 const EmRect& srcRect,
													 const EmRect& destRect,
													 Bool scaled) = 0;

		virtual void			HostPalette			(void);
		virtual void			HostDisplayChange	(void);

		virtual void			HostGetDefaultSkin	(EmPixMap&, int scale) = 0;
		virtual EmPoint			HostGetCurrentMouse	(void) = 0;

	private:
		static EmWindow*		fgWindow;

		EmPixMap				fSkinBase;
		EmPixMap				fSkinCurrent;

		RGBList					fSkinColors[16];

		EmRegion				fSkinRegion;

		RGBList					fPrevLCDColors;

		SkinElementType			fCurrentButton;

		Bool					fNeedWindowReset;
		Bool					fNeedWindowInvalidate;

		Bool					fOldLCDOn;
		Bool					fOldBacklightOn;
		uint16					fOldLEDState;

		Bool					fWiggled;
		Bool					fActive;
		Bool					fDebugMode;
		Bool					fGremlinMode;
};

extern EmWindow*	gWindow;

#endif	// EmWindow_h
