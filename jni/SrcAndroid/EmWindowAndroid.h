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

#ifndef EmWindowAndroid_h
#define EmWindowAndroid_h

#include "EmWindow.h"			// EmWindow
#include "PHEMNativeIF.h"		// PHEM_Event

//#include <vector>

//typedef vector<Fl_Menu_Item> Fl_Menu_Item_List;

class EmWindowAndroid : public EmWindow
{
	public:
						EmWindowAndroid(void);
		virtual				~EmWindowAndroid(void);
		int				handle(PHEM_Event*);

//		void					PopupMenu			(void);
//		void					GetHostMenu			(Fl_Menu_Item_List&);
//		void					DoMenuCommand		(const Fl_Menu_Item&);

//		void					CacheFlush			(void);
//		Fl_Image*				GetSkin				(void);

//	private:
//		virtual	void			draw				(void);
//		virtual int				handle				(int event);
//		static void				CloseCallback		(Fl_Widget*, void*);

	private:
		virtual void			HostWindowReset		(void);

		virtual void			HostMouseCapture	(void);
		virtual void			HostMouseRelease	(void);

		virtual void			HostDrawingBegin	(void);

		virtual void			HostWindowMoveBy	(const EmPoint&);
		virtual void			HostWindowMoveTo	(const EmPoint&);
		virtual EmRect			HostWindowBoundsGet	(void);
		virtual void			HostWindowCenter	(void);
		virtual void			HostWindowShow		(void);

		virtual void			HostRectFrame		(const EmRect&, const EmPoint&, const RGBType&);
		virtual void			HostOvalPaint		(const EmRect&, const RGBType&);

		virtual void			HostPaintCase		(const EmScreenUpdateInfo&);
		virtual void			HostPaintLCD		(const EmScreenUpdateInfo& info,
													 const EmRect& srcRect,
													 const EmRect& destRect,
													 Bool scaled);

		virtual void			HostGetDefaultSkin	(EmPixMap&, int scale);
		virtual EmPoint			HostGetCurrentMouse	(void);

	private:
                EmCoord				m_width;
                EmCoord				m_height;
//		Fl_Box*					fMessage;
//		Fl_Image*				fCachedSkin;
};

extern EmWindowAndroid* gHostWindow;

#endif	// EmWindowAndroid_h
