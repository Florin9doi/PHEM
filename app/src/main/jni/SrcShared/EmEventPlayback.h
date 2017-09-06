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

#ifndef EmEventPlayback_h
#define EmEventPlayback_h

#include "ChunkFile.h"			// Chunk

#include <limits.h>				// LONG_MAX
#include <vector>				// vector

/*
	EmEventPlayback is responsible for managing the events generated
	during a Gremlins run.  It doesn't generate the events themselves,
	but, one they're created, it has the following duties:

		* Recording them in a list so that they can be replayed later.
		* Replaying them later
		* Saving them to and loading them from a file.
		* Logging events for debugging.
		* Filtering the events so that not all of them get replayed.

	This system is accessed from the other following locations:

		* CGremlins			: record events as they are generated.
		* EmDocument		: start playback after loading.
		* EmMinimize		: set and filter events replayed.
		* EmPalmOS			: call Initialize, Reset, etc., methods.
		* EmPatchMgr		: reply events.
		* EmPatchModuleSys	: inhibit application switching.
		* EmSession			: inhibit user events during playback.
		* Hordes			: turn recording on/off; save and load events
							  as Gremlins are switched.
*/

class EmFileRef;
class SessionFile;

#ifdef _MSC_VER
	// VC++ doesn't appear to find the bool specialization when you
	// say just "vector<bool>", so let's use its typedef.
typedef _Bvector		EmRecordedEventFilter;
#else
typedef vector<bool>	EmRecordedEventFilter;
#endif

enum EmRecordedEventType
{
	// New items can be freely added to this list, and the list
	// can freely be re-ordered.

	kRecordedUnknownEvent = -1,
	kRecordedKeyEvent,
	kRecordedPenEvent,
	kRecordedAppSwitchEvent,
	kRecordedNullEvent,
	kRecordedErrorEvent
};

struct EmRecordedEvent
{
							EmRecordedEvent		(void);
							EmRecordedEvent		(const EmRecordedEvent&);
							~EmRecordedEvent	(void);

	EmRecordedEvent&		operator=			(const EmRecordedEvent&);

	EmRecordedEventType		eType;

	union
	{
		struct
		{
			WChar			ascii;
			UInt16			keycode;
			UInt16			modifiers;
		} keyEvent;

		struct
		{
			PointType		coords;
		} penEvent;

		struct
		{
			uint16			cardNo;
			uint32			dbID;
			uint16			oldCardNo;
			uint32			oldDbID;
		} appSwitchEvent;

		struct
		{
		} nullEvent;

		struct
		{
		} errorEvent;
	};
};



class EmEventPlayback
{
	public:
		static void 			Initialize			(void);
		static void 			Reset				(void);
		static void 			Save				(SessionFile&);
		static void 			Load				(SessionFile&);
		static void 			Dispose 			(void);

		static void 			SaveEvents			(SessionFile&);
		static void 			LoadEvents			(const EmFileRef&);
		static void 			LoadEvents			(SessionFile&);

		static void				RecordEvents		(Bool);
		static Bool				RecordingEvents		(void);
		static void				RecordKeyEvent		(WChar		ascii,
													 UInt16		keycode,
													 UInt16		modifiers);
		static void				RecordPenEvent		(const PointType&);
		static void				RecordSwitchEvent	(uint16		cardNo,
													 uint32		dbID,
													 uint16		oldCardNo,
													 uint32		oldDbID);
		static void				RecordNullEvent		(void);
		static void				RecordErrorEvent	(void);

		static void				Clear				(void);
		static void				CullEvents			(void);
		static long				CountEnabledEvents	(void);

		static long				GetCurrentEvent		(void);
		static long				GetNumEvents		(void);
		static long				CountNumEvents		(void);
		static void				GetEvent			(long, EmRecordedEvent&);

		static void				EnableEvents		(long begin = 0, long end = LONG_MAX);
		static void				DisableEvents		(long begin = 0, long end = LONG_MAX);

		static void				ReplayEvents		(Bool);
		static Bool				ReplayingEvents		(void);
		static Bool				ReplayGetEvent		(void);
		static Bool				ReplayGetPen		(void);

		static long				FindFirstError		(void);
		static void				LogEvents			(void);

	private:
		static void				RecordEvent			(const EmRecordedEvent&);
		static void				LogEvent			(const EmRecordedEvent&);
		static void				ResetPlayback		(void);
		static Bool				GetNextReplayEvent	(EmRecordedEvent&);

		static Bool				ReplayKeyEvent		(WChar		ascii,
													 UInt16		keycode,
													 UInt16		modifiers);
		static Bool				ReplayPenEvent		(const PointType&);
		static Bool				ReplaySwitchEvent	(uint16		cardNo,
													 uint32		dbID,
													 uint16		oldCardNo,
													 uint32		oldDbID);
		static Bool				ReplayNullEvent		(void);
		static Bool				ReplayErrorEvent	(void);

	private:
		static Chunk					fgEvents;
		static EmRecordedEventFilter	fgMask;
		static Bool						fgRecording;
		static Bool						fgReplaying;

		struct EmIterationState
		{
			EmIterationState (void) :
				fIndex (0),
				fOffset (0),
				fPenIsDown (false)
				{}

			long				fIndex;
			long				fOffset;
			Bool				fPenIsDown;
		};

		static EmIterationState	fgIterationState;
		static EmIterationState	fgPrevIterationState;
};

#endif	// EmEventPlayback_h
