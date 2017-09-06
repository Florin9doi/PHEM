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

#ifndef _STARTUP_H_
#define _STARTUP_H_

#include "EmFileRef.h"			// EmFileRefList
#include "EmStructs.h"			// StringList

#include <string>				// string
#include <map>					// multimap

struct Configuration;
struct HordeInfo;
class EmFileRef;

typedef StringStringMap				OptionList;
typedef multimap<string, string>	PreferenceList;

class Startup
{
	public:
		static Bool				DetermineStartupActions	(int argc, char** argv);

		static void				GetAutoLoads			(EmFileRefList&);
		static string			GetAutoRunApp			(void);

		// Getters...

		static Bool				AskWhatToDo				(void);
		static Bool				CreateSession			(Configuration&);
		static Bool				OpenSession				(EmFileRef&);
		static Bool				Minimize				(EmFileRef&);
		static Bool				NewHorde				(HordeInfo*);
		static Bool				HordeQuitWhenDone		(void);
		static Bool				MinimizeQuitWhenDone	(void);
		static Bool				CloseSession			(EmFileRef&);
		static Bool				QuitOnExit				(void);

	private:
		static void				PrvGetDatabaseInfosFromAppNames		(const StringList& names, DatabaseInfoList& results);
		static void				PrvError				(const char* msg);
		static void				PrvDontUnderstand		(const char* arg);
		static void				PrvMissingArgument		(const char* arg);
		static void				PrvInvalidRAMSize		(const char* arg);
		static void				PrvInvalidSkin			(const char* argSkin, const char* argDevice);
		static void				PrvInvalidPreference	(const char* pref);
		static void				PrvPrintHelp			(void);
		static int				PrvParseOneOption		(int argc, char** argv, int& argIndex);
		static Bool				PrvCollectOptions		(int argc, char** argv,
														 OptionList& options,
														 PreferenceList& prefs);
		static Bool				PrvConvertRAM			(const string& str, RAMSizeType& ramSize);
		static void				PrvParseFileList		(EmFileRefList& fileList, string option);
		static Bool				PrvHandleOpenSessionParameters		(OptionList& options);
		static Bool				PrvHandleCreateSessionParameters	(OptionList& options);
		static Bool				PrvHandleNewHordeParameters			(OptionList& options);
		static Bool				PrvHandleAutoLoadParameters			(OptionList& options);
		static Bool				PrvHandleSkinParameters				(OptionList& options);
		static Bool				PrvHandlePreferenceParameters		(PreferenceList& prefs);
		static Bool				PrvParseCommandLine		(int argc, char** argv);
		static void				PrvLookForAutoloads		(void);
		static void				PrvAppendFiles			(EmFileRefList& list1, const EmFileRefList& list2);
		static string			PrvTryGetApp			(const EmFileRefList& fileList);

	private:
		// Setters...

		static void				Clear					(void);
		static void				ScheduleAskWhatToDo		(void);
		static void				ScheduleCreateSession	(const Configuration&);
		static void				ScheduleOpenSession		(const EmFileRef&);
		static void				ScheduleMinimize		(const EmFileRef&);
		static void				ScheduleNewHorde		(const HordeInfo&, const StringList&);
		static void				ScheduleQuitOnExit		(void);
};

#endif	/* _STARTUP_H_ */
