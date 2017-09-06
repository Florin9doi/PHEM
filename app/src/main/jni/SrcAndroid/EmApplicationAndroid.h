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

#ifndef EmApplicationAndroid_h
#define EmApplicationAndroid_h

#include "EmApplication.h"		// EmApplication
#include "EmStructs.h"			// ByteList
#include "PreferenceMgr.h"              // EmulatorPrefs

#include "PHEMNativeIF.h"

class EmWindowAndroid;

class EmApplicationAndroid : public EmApplication
{
	public:
						EmApplicationAndroid		(void);
		virtual				~EmApplicationAndroid		(void);

	public:
                EmulatorPreferences             prefs;
		virtual Bool			Startup(int argc, char** argv);
		void				Run(void);
		virtual void			Shutdown(void);
		void				HandleIdle(void);
		void				HandleEvent(PHEM_Event *evt);

	private:
		Bool				PrvIdleClipboard		(void);
		static void			PrvClipboardPeriodic	(void* data);
                void				PrvCreateWindow(int argc, char** argv);

	private:
		EmWindowAndroid			*fAppWindow;
		ByteList			fClipboardData;
};

extern EmApplicationAndroid*	gHostApplication;

#endif	// EmApplicationAndroid_h
