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

#ifndef _HORDES_H_
#define _HORDES_H_

#include "EmStructs.h"			// HordeInfo, DatabaseInfoList
#include "EmTypes.h"			// ErrCode
#include "CGremlins.h"			// GremlinEventType


enum HordeFileType
{
	kHordeProgressFile		= 0x00,
	kHordeRootFile			= 0x01,
	kHordeSuspendFile		= 0x02,
	kHordeEventFile			= 0x03,
	kHordeMinimalEventFile	= 0x04,
	kHordeAutoCurrentFile	= 0x05
};



///////////////////////////////////////////////////////////////////////////////////////
// HORDES CLASS

class SessionFile;

// Gremlins::Save, Gremlins::Load
extern	long				gGremlinSaveFrequency;
extern	DatabaseInfoList	gGremlinAppList;

extern	Bool				gWarningHappened;
extern	Bool				gErrorHappened;

class Hordes
{
	public:
		static void				Initialize	(void);
		static void				Reset		(void);
		static void				Save		(SessionFile&);
		static void				Load		(SessionFile&);
		static void				Dispose		(void);

		static void				New						(const HordeInfo& info);

		// Support to run a "horde" of just a single Gremlin -- for example, from a session
		// file load, or from a UI only supporting single Gremlins. "Gremlins classic":

		static void				NewGremlin				(const GremlinInfo& info);

		static void				SaveSearchProgress		(void);
		static void				ResumeSearchProgress	(const EmFileRef& f);
		static void				HStatus					(unsigned short* currentNumber,
														 unsigned long* currentStep,
														 unsigned long* currentUntil);
		
		static Bool				IsOn					(void);
		static Bool				InSingleGremlinMode		(void);
		static Bool				QuitWhenDone			(void);

		static Bool				CanNew					(void);
		static Bool				CanSuspend				(void);
		static Bool				CanStep					(void);
		static Bool				CanResume				(void);
		static Bool				CanStop					(void);

/*		static void				Status					(unsigned short* currentNumber,
														 unsigned long* currentStep,
														 unsigned long* currentUntil);*/

		static void				TurnOn					(Bool hordesOn);

		static int32			GremlinNumber			(void);
		static int32			EventCounter			(void);
		static int32			EventLimit				(void);

		static void				StopEventReached		(void);
		static void				ErrorEncountered		(void);
		static void				RecordErrorStats		(StrCode messageID = -1);

		static void 			Suspend					(void);
		static void 			Step					(void);
		static void				Resume					(void);
		static void				Stop					(void);

		static string			SuggestFileName			(HordeFileType category, uint32 num = 0);
		static EmFileRef		SuggestFileRef			(HordeFileType category, uint32 num = 0);

		static void				PostLoad				(void);

		static Bool				PostFakeEvent			(void);
		static void				PostFakePenEvent		(void);
		static Bool	 			SendCharsToType			(void);
		static void				BumpCounter				(void);

		static uint32			ElapsedMilliseconds		(void);

		static Bool				CanSwitchToApp			(UInt16 cardNo, LocalID dbID);

		static void				SetGremlinsHome			(const EmDirRef& gremlinsHome);
		static void				SetGremlinsHomeToDefault(void);
		static Bool				GetGremlinsHome			(EmDirRef& outPath);

		static void				AutoSaveState			(void);
		static void				SaveSuspendedState		(void);
		static void				SaveRootState			(void);
		static ErrCode			LoadRootState			(void);
		static ErrCode			LoadSuspendedState		(void);

		static void				LoadEvents				(void);
		static void				SaveEvents				(void);

		static void				StartGremlinFromLoadedRootState		(void);
		static void				StartGremlinFromLoadedSuspendedState(void);
		static void				SetGremlinStatePathFromControlFile	(EmFileRef& controlFile);

		static EmDirRef			GetGremlinDirectory		(void);
		static void 			UseNewAutoSaveDirectory (void);
		static DatabaseInfoList	GetAppList				(void);

		static string			TranslateErrorCode		(UInt32 errCode);

	private:
		static void				NextGremlin();
		static void				ProposeNextGremlin		(long& outNextGremlin,
														 long& outNextDepth,
														 long inFromGremlin,
														 long inFromDepth);
		static void				EndHordes				(void);

		static ErrCode			LoadState				(const EmFileRef& ref);

		static void				StartLog				(void);
		static string			GremlinsFlagsToString	(void);
		static void				GremlinsFlagsFromString	(string& inFlags);
		static void				ComputeStatistics		(int32 &min,
														 int32 &max,
														 int32 &avg,
														 int32 &stdDev,
														 int32 &smallErrorIndex);
		static void				GremlinReport			(void);
};

#endif /* _HORDES_H_ */

