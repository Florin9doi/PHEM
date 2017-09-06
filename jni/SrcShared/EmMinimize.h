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

#ifndef EmMinimize_h
#define EmMinimize_h

#include "EmStructs.h"			// GremlinEvent

#include "omnithread.h"			// omni_mutex

#include <vector>				// vector

class SessionFile;

class EmMinimize
{
	public:
		static void 			Initialize				(void);
		static void 			Reset					(void);
		static void 			Save					(SessionFile&);
		static void 			Load					(SessionFile&);
		static void 			Dispose 				(void);

		static void				Start					(void);
		static void				Stop					(void);
		static void				TurnOn					(Bool);
		static Bool				IsOn					(void);

		static void				NoErrorOccurred			(void);
		static void				ErrorOccurred			(void);

		static uint32			GetPassNumber			(void);
		static uint32			GetElapsedTime			(void);
		static void				GetCurrentRange			(uint32&, uint32&);
		static uint32			GetNumDiscardedEvents	(void);
		static uint32			GetNumInitialEvents		(void);

	private:
		static void				MinimizationPassComplete(void);
		static void				MinimizationComplete	(void);
		static void				SaveMinimalEvents		(void);
		static void				OutputEventsAsEnglish	(void);
		static Bool				MakeAnotherPass			(long oldNumEvents, long newNumEvents);
		static void				CurrentRange			(long& begin, long& end);
		static void				InitialLevel			(void);
		static Bool				SplitCurrentLevel		(void);
		static void				StartAgain				(void);
		static void				SplitAndStartAgain		(void);
		static void				DisableAndStartAgain	(void);
		static void				NextSubRange			(void);
		static void				LoadInitialState		(void);

	public:
		static void				RealLoadInitialState	(void);

	private:
		static void				LoadEvents				(void);
		static long				FindFirstError			(void);
		static void				GenerateStackCrawl		(StringList&);


	private:
		struct EmMinimizeLevel
		{
			long	fBegin;
			long	fEnd;

			// The following Boolean is used to indicate that the above range
			// has already been checked in some fashion and that we know or
			// strongly suspect that a needed event is in here somewhere.  The
			// practical effect of this is that when finish with one event
			// range and are popping to this event range (as opposed to
			// splitting the range we just finished and testing each of the
			// split ranges), then should we split this new range, too, or
			// should we test the whole thing first.  Knowing whether or not
			// to split a range that we're considering is important, since it
			// reduces the overall number of passes we have to make.
			//
			// fChecked is managed as follows: When the initial event ranges
			// are determined and pushed onto the fLevels stack, fChecked is
			// clear.  This means that each range is checked before
			// determining to keep it or split it and check it further.  If we
			// test a range and find that we don't need it, we discard that
			// range and don't consider it any further.  If, however, we find
			// that we need a range (because removing it cause the crash to go
			// away), then we set fChecked for that range, subdivide it, and
			// start checking the new sub-ranges.  When we create the new
			// sub-ranges, we copy the fChecked setting for the child ranges. 
			// For as long as we subdivide and retest subranges, we copy that
			// bit to the new sub ranges.
			//
			// However, as soon as we get down to the single event level and
			// determine that we need a particular event and can't subdivide
			// any longer, we walk up the stack and clear all fChecked
			// Booleans.  The idea here is that if there was just a single
			// event in the range that we needed to find and mark as
			// important, then we just found it.  We can no longer assume that
			// any other events in the initial range that we've been
			// subdividing are important, and so we need to clear the bit that
			// means that there's an important event within them.

			Bool	fChecked;
		};

		typedef vector<EmMinimizeLevel>	EmMinimizeLevelList;

		struct EmMinimizeState
		{
			EmMinimizeLevelList	fLevels;
		};

		static omni_mutex		fgMutex;
		static EmMinimizeState	fgState;
		static Bool				fgIsOn;
		static uint32			fgStartTime;
		static long				fgInitialNumberOfEvents;
		static long				fgDiscardedNumberOfEvents;
		static long				fgPassNumber;
		static Bool				fgPassEndedInError;
		static StringList		fgLastStackCrawl;
};

#endif // EmMinimize_h
