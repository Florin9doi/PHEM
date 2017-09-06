/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmEventPlayback.h"

#include "CGremlinsStubs.h"		// StubAppEnqueueKey, StubAppEnqueuePt
#include "EmEventOutput.h"		// GetEventInfo
#include "EmMemory.h"			// EmMem_strlen, EmMem_strcpy
#include "EmMinimize.h"			// EmMinimize::IsOn
#include "EmPalmStructs.h"		// EmAliasControlType
#include "EmStreamFile.h"		// EmStreamFile
#include "Logging.h"			// LogAppendMsg
#include "ROMStubs.h"			// EvtResetAutoOffTimer
#include "SessionFile.h"		// SessionFile

#include <ctype.h>				// isprint

#if _DEBUG
#define LOG_PLAYBACK	1
#else
#define LOG_PLAYBACK	0
#endif

#define PRINTF	if (!LOG_PLAYBACK) ; else LogAppendMsg


EmStream&	operator << (EmStream&, const EmRecordedEvent&);
EmStream&	operator >> (EmStream&, EmRecordedEvent&);

EmStream&	operator << (EmStream&, const Chunk&);
EmStream&	operator >> (EmStream&, Chunk&);


// List of events that we've recorded or re-read from a session file.

Chunk								EmEventPlayback::fgEvents;
EmRecordedEventFilter				EmEventPlayback::fgMask;
Bool								EmEventPlayback::fgRecording;
Bool								EmEventPlayback::fgReplaying;
EmEventPlayback::EmIterationState	EmEventPlayback::fgIterationState;
EmEventPlayback::EmIterationState	EmEventPlayback::fgPrevIterationState;

enum EmStoredEventType
{
	// These values are written to external files, and so should not
	// be changed.  New values should be added to the end, and current
	// values should not be deleted.

	kStoredKeyEvent,					// 7 bytes
	kStoredKeyEventCompressed,			// 2 byte

	kStoredPenEvent,					// 5 bytes
	kStoredPenEventCompressedUp,		// 1 bytes
	kStoredPenEventCompressedX,			// 4 bytes
	kStoredPenEventCompressedY,			// 4 bytes
	kStoredPenEventCompressedXY,		// 3 bytes

	kStoredAppSwitchEvent,				// 13 bytes
	kStoredAppSwitchEventCompressed,	// N/A

	kStoredNullEvent,					// 1 bytes
	kStoredNullEventCompressed,			// N/A

	kStoredErrorEvent,					// 5 bytes
	kStoredErrorEventCompressed			// N/A
};


static inline Bool PrvIsPenEvent (const EmRecordedEvent& event)
{
	return event.eType == kRecordedPenEvent;
}

static inline Bool PrvIsPenUp (const PointType& pt)
{
	return pt.x == -1 && pt.y == -1;
}

static inline Bool PrvIsPenDown (const PointType& pt)
{
	return !::PrvIsPenUp (pt);
}

static inline Bool PrvIsPenUp (const EmRecordedEvent& event)
{
	return ::PrvIsPenEvent (event) && ::PrvIsPenUp (event.penEvent.coords);
}

static inline Bool PrvIsPenDown (const EmRecordedEvent& event)
{
	return ::PrvIsPenEvent (event) && ::PrvIsPenDown (event.penEvent.coords);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::Initialize
// ---------------------------------------------------------------------------

void EmEventPlayback::Initialize (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::Reset
// ---------------------------------------------------------------------------

void EmEventPlayback::Reset (void)
{
	fgRecording		= false;
	fgReplaying		= false;

	EmEventPlayback::ResetPlayback ();
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::Save
// ---------------------------------------------------------------------------
// Save our state to the given file.  This method is called when a session
// file is being saved to disk.  A session file can be saved under the
// following circumstances:
//
//	* Save / Save As
//	* Minimization (after final minimizing)
//	* Gremlin switching
//
// For Save / Save As, we don't really care about saving events.  In fact,
// event recording shouldn't even be on.  (However, this may change in the
// future if we want to record human-generated events.)
//
// For Minimization, we want to save the final minimal set of events to a
// .pev file so that it can later be replayed.
//
// For Gremlins, we want to save events to a holding .pev file, seperate
// from the "resume" files Hordes creates.
//
// Based on these requirements, we don't really want to unconditionally save
// events when saving the session.  Instead, we'll save them as needed from
// the various sub-systems that need the events.

void EmEventPlayback::Save (SessionFile&)
{
//	EmEventPlayback::SaveEvents (f, fgEvents);
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::Load
// ---------------------------------------------------------------------------
// Load our state from the given file.  This method is called when a session
// file is being reloaded from disk.  A session file can be loaded under the
// following circumstances:
//
//	* Open
//	* Replay
//	* Minimize
//	* Gremlin switching
//
// For "Open", we don't really care about any events stored with the session.
// In fact, since session files with saved events are usually of a type that
// the Open menu item doesn't allow, we shouldn't get into this situation.
//
// When Replaying, we want to load the events so that we can replay them.
//
// When Minimizing, we want to load the events so that we can filter them --
// possibly many times -- and then start replaying them.  However, we want
// to load the events only once, so as not to affect any modifications we
// may have made to the event set as part of minimizing them.
//
// When Gremlin Switching, we want to load the events, but load them from
// a file different from the one holding the state.
//
// Based on these requirements, we don't really want to unconditionally load
// events when loading the session.  Instead, we'll load them as needed from
// the various sub-systems that need the events.

void EmEventPlayback::Load (SessionFile&)
{
//	EmEventPlayback::LoadEvents (f, fgEvents);
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::Dispose
// ---------------------------------------------------------------------------

void EmEventPlayback::Dispose (void)
{
	EmEventPlayback::Clear ();
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::SaveEvents
// ---------------------------------------------------------------------------
// Saves the given events to the given session file.  Saves all of the events
// in their own chunk.

void EmEventPlayback::SaveEvents (SessionFile& f)
{
	const uint32	kCurrentVersion = 1;
	Chunk			chunk;
	EmStreamChunk	s (chunk);

	s << kCurrentVersion;
	s << fgEvents;

	f.WriteGremlinHistory (chunk);

#if 0
	LogAppendMsg ("EmEventPlayback::SaveEvents: saved %d events", EmEventPlayback::GetNumEvents ());

	EmRecordedEventList::size_type	length = EmEventPlayback::GetNumEvents ();
	EmRecordedEventList::size_type	begin = length - 10;

//	if (begin < 0)
		begin = 0;

	for (EmRecordedEventList::size_type ii = begin; ii < length; ++ii)
	{
		LogAppendMsg ("%d:", ii);
		EmEventPlayback::LogEvent (events[ii]);
	}
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::LoadEvents
// ---------------------------------------------------------------------------
// Loads all of the events from a session file and returns them in the given
// collection.

void EmEventPlayback::LoadEvents (const EmFileRef& ref)
{
	EmStreamFile	stream (ref, kOpenExistingForRead);
	ChunkFile		chunkFile (stream);
	SessionFile		sessionFile (chunkFile);

	EmEventPlayback::LoadEvents (sessionFile);
}


void EmEventPlayback::LoadEvents (SessionFile& f)
{
	Chunk	chunk;

	fgEvents.SetLength (0);	// Clear the list in case of failure.

	if (f.ReadGremlinHistory (chunk))
	{
		uint32			version;
		EmStreamChunk	s (chunk);

		s >> version;
		s >> fgEvents;

		// Set the event mask to be the same size, with all events enabled.
		// (I'd use assign() here, but it's not support on my Linux's version
		// of STL.)
//		fgMask.assign (events.size (), true);
		fgMask = EmRecordedEventFilter (EmEventPlayback::CountNumEvents (), true);
	}

#if 0
	LogAppendMsg ("EmEventPlayback::LoadEvents: loaded %d events", EmEventPlayback::GetNumEvents ());

	// Iterate over all the events, counting them up.

	long			ii = 0;
	EmRecordedEvent	event;
	EmStreamChunk	s (fgEvents);

	while (s.GetMarker () < s.GetLength ())
	{
		s >> event;

		LogAppendMsg ("%d:", ii);
		EmEventPlayback::LogEvent (event);

		++ii;
	}
#endif
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::RecordEvents
// ---------------------------------------------------------------------------

void EmEventPlayback::RecordEvents (Bool record)
{
	fgRecording = record;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::RecordingEvents
// ---------------------------------------------------------------------------
// Return whether or not we're recording events.

Bool EmEventPlayback::RecordingEvents (void)
{
	return fgRecording;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::RecordCharEvent
// ---------------------------------------------------------------------------
// Record a character event, as passed to EvtEnqueueKey.

void EmEventPlayback::RecordKeyEvent (WChar		ascii,
									  UInt16	keycode,
									  UInt16	modifiers)
{
	EmRecordedEvent event;
	event.eType = kRecordedKeyEvent;

	event.keyEvent.ascii		= ascii;
	event.keyEvent.keycode		= keycode;
	event.keyEvent.modifiers	= modifiers;

	EmEventPlayback::RecordEvent (event);
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::RecordPenEvent
// ---------------------------------------------------------------------------
// Record a pen event, as passed to EvtEnqueuePen.

void EmEventPlayback::RecordPenEvent (const PointType& coords)
{
	EmAssert (::PrvIsPenUp (coords) || ((coords.x >= 0) && (coords.y >= 0)));

	EmRecordedEvent event;
	event.eType = kRecordedPenEvent;

	event.penEvent.coords	= coords;

	EmEventPlayback::RecordEvent (event);
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::RecordSwitchEvent
// ---------------------------------------------------------------------------
// Record a psuedo-event indicating that Gremlins is trying to switch to
// another application.

void EmEventPlayback::RecordSwitchEvent (uint16 cardNo,
										 uint32 dbID,
										 uint16 oldCardNo,
										 uint32 oldDbID)
{
	EmRecordedEvent event;
	event.eType = kRecordedAppSwitchEvent;

	event.appSwitchEvent.cardNo		= cardNo;
	event.appSwitchEvent.dbID		= dbID;
	event.appSwitchEvent.oldCardNo	= oldCardNo;
	event.appSwitchEvent.oldDbID	= oldDbID;

	EmEventPlayback::RecordEvent (event);
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::RecordNullEvent
// ---------------------------------------------------------------------------
// Record a NULL event.

void EmEventPlayback::RecordNullEvent (void)
{
	EmRecordedEvent event;
	event.eType = kRecordedNullEvent;

	EmEventPlayback::RecordEvent (event);
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::RecordErrorEvent
// ---------------------------------------------------------------------------
// Record a psuedo-event indicating that an error occurred.

void EmEventPlayback::RecordErrorEvent (void)
{
	EmRecordedEvent event;
	event.eType = kRecordedErrorEvent;

	EmEventPlayback::RecordEvent (event);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::Clear
// ---------------------------------------------------------------------------
// Clear all events that we've recorded so far.

void EmEventPlayback::Clear (void)
{
	fgEvents.SetLength (0);
	fgMask.clear ();
	fgRecording		= false;
	fgReplaying		= false;

	EmEventPlayback::ResetPlayback ();
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::CullEvents
// ---------------------------------------------------------------------------
// Remove all filtered events.

void EmEventPlayback::CullEvents (void)
{
	Chunk			newEvents;
	EmStreamChunk	s (newEvents);

	EmEventPlayback::ResetPlayback ();

	EmRecordedEvent	event;
	while (EmEventPlayback::GetNextReplayEvent (event))
	{
		s << event;

		// Keep track of pen up/down state so that GetNextReplayEvent
		// will filter properly.

		fgIterationState.fPenIsDown = ::PrvIsPenDown (event);
	}

	fgEvents = newEvents;

	// Set the event mask to be the same size, with all events enabled.
	// (I'd use assign() here, but it's not support on my Linux's version
	// of STL.)
//	fgMask.assign (fgEvents.size (), true);
	fgMask = EmRecordedEventFilter (EmEventPlayback::CountNumEvents (), true);
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::CountEnabledEvents
// ---------------------------------------------------------------------------
// If we were to play all events right now given the current enabled state,
// return how many events that would be.

long EmEventPlayback::CountEnabledEvents (void)
{
	long			result = 0;

	// We may be in the middle of a playback right now, so save the
	// playback state we'll be changing.

	EmValueChanger<EmIterationState>	oldIterationState (fgIterationState, EmIterationState ());

	// Iterate over all the events, counting them up.

	EmRecordedEvent	event;
	while (EmEventPlayback::GetNextReplayEvent (event))
	{
		++result;

		// Keep track of pen up/down state so that GetNextReplayEvent
		// will filter properly.

		fgIterationState.fPenIsDown = ::PrvIsPenDown (event);
	}

	return result;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::GetCurrentEvent
// ---------------------------------------------------------------------------
// Return the event number we've played back.  This will be it's index plus
// one.  That is, it's the index of the event following the one we most
// recently returned and presumably played.

long EmEventPlayback::GetCurrentEvent (void)
{
	return fgIterationState.fIndex;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::GetNumEvents
// ---------------------------------------------------------------------------
// Return how many events we've recorded.  If we're playing back, this count
// may include events that have been masked out.

long EmEventPlayback::GetNumEvents (void)
{
	return fgMask.size ();
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::CountNumEvents
// ---------------------------------------------------------------------------
// Return how many events we've recorded.

long EmEventPlayback::CountNumEvents (void)
{
	long			result = 0;

	// Iterate over all the events, counting them up.

	EmRecordedEvent	event;
	EmStreamChunk	s (fgEvents);

	while (s.GetMarker () < s.GetLength ())
	{
		s >> event;
		++result;
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::GetEvent
// ---------------------------------------------------------------------------
// Return an event on our list of recorded events.

void EmEventPlayback::GetEvent (long index, EmRecordedEvent& event)
{
	EmStreamChunk	s (fgEvents);

	while ((index-- >= 0) && (s.GetMarker () < s.GetLength ()))
	{
		s >> event;
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::EnableEvents
// ---------------------------------------------------------------------------

void EmEventPlayback::EnableEvents (long begin, long end)
{
	if (begin < 0)
		begin = 0;

	if (end > (long) fgMask.size ())
		end = (long) fgMask.size ();

	for (long ii = begin; ii < end; ++ii)
	{
		fgMask[ii] = true;
	}
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::DisableEvents
// ---------------------------------------------------------------------------

void EmEventPlayback::DisableEvents (long begin, long end)
{
	if (begin < 0)
		begin = 0;

	if (end > (long) fgMask.size ())
		end = (long) fgMask.size ();

	for (long ii = begin; ii < end; ++ii)
	{
		fgMask[ii] = false;
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplayEvents
// ---------------------------------------------------------------------------

void EmEventPlayback::ReplayEvents (Bool replay)
{
	fgReplaying = replay;

	if (replay)
	{
		EmEventPlayback::ResetPlayback ();
	}
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplayingEvents
// ---------------------------------------------------------------------------

Bool EmEventPlayback::ReplayingEvents (void)
{
	return fgReplaying;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplayGetEvent
// ---------------------------------------------------------------------------

Bool EmEventPlayback::ReplayGetEvent (void)
{
	EmAssert (EmEventPlayback::ReplayingEvents ());

	Bool	result = false;

	EmRecordedEvent	event;
	if (EmEventPlayback::GetNextReplayEvent (event))
	{
		// Keep track of pen up/down state so that GetNextReplayEvent
		// will filter properly.

		fgIterationState.fPenIsDown = ::PrvIsPenDown (event);

		switch (event.eType)
		{
			case kRecordedKeyEvent:

				result = EmEventPlayback::ReplayKeyEvent (
					event.keyEvent.ascii, event.keyEvent.keycode, event.keyEvent.modifiers);

				break;

			case kRecordedPenEvent:

				result = EmEventPlayback::ReplayPenEvent (
					event.penEvent.coords);

				break;

			case kRecordedAppSwitchEvent:

				result = EmEventPlayback::ReplaySwitchEvent (
					event.appSwitchEvent.cardNo, event.appSwitchEvent.dbID,
					event.appSwitchEvent.oldCardNo, event.appSwitchEvent.oldDbID);

				break;

			case kRecordedNullEvent:

				result = EmEventPlayback::ReplayNullEvent ();

				break;

			case kRecordedErrorEvent:

				result = EmEventPlayback::ReplayErrorEvent ();

				break;

			default:
				EmAssert (false);
				break;
		}

		// Get information on this event.

		EmEventOutput::GetEventInfo (event);
	}
	else
	{
		// No more events to return; turn ourself off.

		EmEventPlayback::ReplayEvents (false);

		// If minimization is running, tell it that we appear to have
		// run the gamut.

		if (EmMinimize::IsOn ())
		{
			EmMinimize::NoErrorOccurred ();
		}
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplayGetPen
// ---------------------------------------------------------------------------

Bool EmEventPlayback::ReplayGetPen (void)
{
	EmAssert (EmEventPlayback::ReplayingEvents ());

	EmRecordedEvent	event;
	Bool			haveEvent = EmEventPlayback::GetNextReplayEvent (event);

	// The Palm OS shouldn't be asking for the pen location if the
	// pen is up.  And if the pen is still down, there should be a
	// pen up event in here somewhere.  Therefore, GetNextReplayEvent
	// should not be returning false or a non-pen event.  But if it
	// does (and it *does* happen), fabricate a pen up.

	if (!haveEvent)
	{
		PRINTF ("EmEventPlayback::ReplayGetPen[%ld]: no more events; fabricating a pen up event",
			fgIterationState.fIndex - 1);

		haveEvent = true;
		event.eType = kRecordedPenEvent;
		event.penEvent.coords.x = -1;
		event.penEvent.coords.y = -1;

		fgIterationState.fPenIsDown = false;
	}
	else if (event.eType != kRecordedPenEvent)
	{
		PRINTF ("EmEventPlayback::ReplayGetPen[%ld]: next event wasn't a pen event; fabricating a pen up event",
			fgIterationState.fIndex - 1);

		EmAssert (fgPrevIterationState.fOffset != -1);

		fgIterationState = fgPrevIterationState;

		// Invalidate the previously saved iteration state so that
		// we can make sure we don't use it again.

		fgPrevIterationState.fOffset = -1;

		event.eType = kRecordedPenEvent;
		event.penEvent.coords.x = -1;
		event.penEvent.coords.y = -1;

		fgIterationState.fPenIsDown = false;
	}

	EmAssert (haveEvent);
	EmAssert (event.eType == kRecordedPenEvent);

	Bool	result = EmEventPlayback::ReplayPenEvent (event.penEvent.coords);

	// Get information on this event.

	EmEventOutput::GetEventInfo (event);

	return result;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::RecordEvent
// ---------------------------------------------------------------------------
// Add the given event to our list of events, as long as recording is on.

void EmEventPlayback::RecordEvent (const EmRecordedEvent& event)
{
	if (EmEventPlayback::RecordingEvents ())
	{
		EmStreamChunk	s (fgEvents);
		s.SetMarker (0, kStreamFromEnd);
		s << event;

		fgMask.push_back (true);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::FindFirstError
// ---------------------------------------------------------------------------
// Return the index of the first error event record.  Return -1 if one could
// not be found.

long EmEventPlayback::FindFirstError (void)
{
	long	result = 0;

	// Iterate over all the events, counting them up.

	EmRecordedEvent	event;
	EmStreamChunk	s (fgEvents);

	while (s.GetMarker () < s.GetLength ())
	{
		s >> event;

		if (event.eType == kRecordedErrorEvent)
		{
			return result;
		}

		++result;
	}

	return -1;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::LogEvents
// ---------------------------------------------------------------------------
// Print debug information about the given event to the Log file.

void EmEventPlayback::LogEvents (void)
{
	long			counter = 0;
	EmRecordedEvent	event;
	EmStreamChunk	s (fgEvents);

	while (s.GetMarker () < s.GetLength ())
	{
		s >> event;

		LogAppendMsg ("%d:", counter);
		EmEventPlayback::LogEvent (event);

		++counter;
	}
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::LogEvent
// ---------------------------------------------------------------------------
// Print debug information about the given event to the Log file.

void EmEventPlayback::LogEvent (const EmRecordedEvent& inEvent)
{
	static const char* kStrings[] =
	{
		"kGremlinKeyEvent",
		"kGremlinPenEvent",
		"kGremlinAppSwitchEvent",
		"kGremlinNullEvent",
		"kGremlinErrorEvent"
	};

	LogAppendMsg ("	eType			= %d (%s)", inEvent.eType, kStrings[inEvent.eType]);

	switch (inEvent.eType)
	{
		case kRecordedKeyEvent:

			// If it's a printable character, print it.  Otherwise,
			// show its numerical value.

			if ((inEvent.keyEvent.ascii < 0x0100) && isprint (inEvent.keyEvent.ascii))
				LogAppendMsg ("		ascii		= %d (%c)", inEvent.keyEvent.ascii, (char)  inEvent.keyEvent.ascii);
			else
				LogAppendMsg ("		ascii		= %d", inEvent.keyEvent.ascii);

			// If there's a keycode, print it.

			if (inEvent.keyEvent.keycode)
				LogAppendMsg ("		keycode		= %d", inEvent.keyEvent.keycode);

			// If there's a modifier, print it.

			if (inEvent.keyEvent.modifiers)
				LogAppendMsg ("		modifiers	= %d", inEvent.keyEvent.modifiers);
			
			break;

		case kRecordedPenEvent:

			// Print X, Y.

			LogAppendMsg ("		coords.x	= %d", inEvent.penEvent.coords.x);
			LogAppendMsg ("		coords.y	= %d", inEvent.penEvent.coords.y);

			break;

		case kRecordedAppSwitchEvent:

			LogAppendMsg ("		cardNo		= %d", inEvent.appSwitchEvent.cardNo);
			LogAppendMsg ("		dbID		= %d", inEvent.appSwitchEvent.dbID);
			LogAppendMsg ("		oldCardNo	= %d", inEvent.appSwitchEvent.oldCardNo);
			LogAppendMsg ("		oldDbID		= %d", inEvent.appSwitchEvent.oldDbID);

			break;

		case kRecordedNullEvent:

			break;

		case kRecordedErrorEvent:

			break;

		default:

			EmAssert (false);
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ResetPlayback
// ---------------------------------------------------------------------------

void EmEventPlayback::ResetPlayback (void)
{
	fgIterationState = EmIterationState ();

	// Invalidate the previously saved iteration state so that
	// we can make sure we don't use it.

	fgPrevIterationState.fOffset = -1;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::GetNextReplayEvent
// ---------------------------------------------------------------------------

/*
	The filtering of events is a little tricky.  The user can *request*
	that an event be filtered or not filtered during subsequent event
	playback.  However, this request does not need to be heeded by the
	actual playback function.  This disobedience is mostly noticable
	when dealing with pen events.  If the pen is down and the pen up
	is supposed to be filtered out, the pen event will be replayed
	anyway.  Similarly, if a pen up event is supposed to be returned but
	the pen is not down (because previous pen down events were filtered
	out), then event is not replayed.
*/

Bool EmEventPlayback::GetNextReplayEvent (EmRecordedEvent& event)
{
	EmStreamChunk	s (fgEvents);
	s.SetMarker (fgIterationState.fOffset, kStreamFromStart);

	// Save the current position so that we can push back to it
	// later if we have to (see EmEventPlayback::ReplayGetPen).

	fgPrevIterationState = fgIterationState;

	while (s.GetMarker () < s.GetLength ())
	{
		s >> event;

		Bool	eventEnabled	= fgMask[fgIterationState.fIndex];

		fgIterationState.fIndex++;
		fgIterationState.fOffset = s.GetMarker ();

		// If this is a pen up event:
		//
		//	* Discard it if the pen is already up.
		//
		//	* Return it if the pen is down, even if this event is
		//		masked out.

		if (::PrvIsPenUp (event))
		{
			if (!fgIterationState.fPenIsDown)
			{
				continue;
			}

			return true;
		}

		// If this is any event and it's not masked out, return it.

		if (eventEnabled)
		{
			// Make sure that the pen is not down if this is not a pen event.
			// The code above should make sure that we never get into this
			// state.
			//
			// Actually, it seems that Gremlins can generate a sequence of
			// pen down events followed by something other than a pen up
			// event.  So no longer make this assert.  Elsewhere in this
			// module, we'll have to make sure we set fPenIsDown to false
			// on non-pen events.

			/*
			if (!::PrvIsPenEvent (event) && fgIterationState.fPenIsDown)
			{
				EmAssert (false);
			}
			*/

			return true;
		}

		// This event is masked out -- move on to the next event.
	}

	// No event to return.

	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplayKeyEvent
// ---------------------------------------------------------------------------

Bool EmEventPlayback::ReplayKeyEvent (WChar		ascii,
									 UInt16		keycode,
									 UInt16		modifiers)
{
	PRINTF ("EmEventPlayback::ReplayKeyEvent[%ld]: playing back key %d, %d, %d",
		fgIterationState.fIndex - 1, ascii, keycode, modifiers);

	// EvtEnqueueKey doesn't reset the event timer.

	::EvtResetAutoOffTimer ();

	// Add the event to the event queue.

	::StubAppEnqueueKey (ascii, keycode, modifiers);

	// Return that we posted an event.

	return true;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplayPenEvent
// ---------------------------------------------------------------------------

Bool EmEventPlayback::ReplayPenEvent (const PointType& pt)
{
	PRINTF ("EmEventPlayback::ReplayPenEvent[%ld]: playing back pen %d, %d",
		fgIterationState.fIndex - 1, pt.x, pt.y);

	// Add the event to the event queue.

	::StubAppEnqueuePt (&pt);

	// Return that we posted an event.

	return true;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplaySwitchEvent
// ---------------------------------------------------------------------------

Bool EmEventPlayback::ReplaySwitchEvent (uint16		cardNo,
										 uint32		dbID,
										 uint16		/* oldCardNo */,
										 uint32		/* oldDbID */)
{
	PRINTF ("EmEventPlayback::ReplaySwitchEvent[%ld]: playing switch %d, %ld",
		fgIterationState.fIndex - 1, cardNo, dbID);

	// Switch to the indicated application.

	::SysUIAppSwitch (cardNo, dbID, sysAppLaunchCmdNormalLaunch, NULL);

	// Return that we did NOT post an event, resulting in the event
	// insertion mechanism posting a NULL event.

	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplayNullEvent
// ---------------------------------------------------------------------------

Bool EmEventPlayback::ReplayNullEvent (void)
{
	PRINTF ("EmEventPlayback::ReplayNullEvent[%ld]: playing NULL",
		fgIterationState.fIndex - 1);

	// Return that we did NOT post an event, resulting in the event
	// insertion mechanism posting a NULL event.

	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmEventPlayback::ReplayErrorEvent
// ---------------------------------------------------------------------------

Bool EmEventPlayback::ReplayErrorEvent (void)
{
	PRINTF ("EmEventPlayback::ReplayErrorEvent[%ld]: playing error",
		fgIterationState.fIndex - 1);

	// Return that we did NOT post an event, resulting in the event
	// insertion mechanism posting a NULL event.

	return false;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ operator <<
// ---------------------------------------------------------------------------
// Flatten an EmRecordedEvent to the stream.  Some events can be compressed by
// not writing out fields that are commonly zero/NULL, or by writing out only
// 8-bit values even though the field is a 16-bit type.

EmStream& operator << (EmStream& inStream, const EmRecordedEvent& inEvent)
{
	switch (inEvent.eType)
	{
		case kRecordedKeyEvent:

			if (inEvent.keyEvent.ascii < 256 &&
				inEvent.keyEvent.keycode == 0 &&
				inEvent.keyEvent.modifiers == 0)
			{
				inStream << (uint8) kStoredKeyEventCompressed;
				inStream << (uint8) inEvent.keyEvent.ascii;
			}
			else
			{
				inStream << (uint8) kStoredKeyEvent;
				inStream << inEvent.keyEvent.ascii;
				inStream << inEvent.keyEvent.keycode;
				inStream << inEvent.keyEvent.modifiers;
			}
			
			break;

		case kRecordedPenEvent:

			if (inEvent.penEvent.coords.x < 0 && inEvent.penEvent.coords.y < 0)
			{
				inStream << (uint8) kStoredPenEventCompressedUp;
			}
			else if (inEvent.penEvent.coords.x < 256 && inEvent.penEvent.coords.y < 256)
			{
				inStream << (uint8) kStoredPenEventCompressedXY;
				inStream << (uint8) inEvent.penEvent.coords.x;
				inStream << (uint8) inEvent.penEvent.coords.y;
			}
			else if (inEvent.penEvent.coords.x < 256)
			{
				inStream << (uint8) kStoredPenEventCompressedX;
				inStream << (uint8) inEvent.penEvent.coords.x;
				inStream << inEvent.penEvent.coords.y;
			}
			else if (inEvent.penEvent.coords.y < 256)
			{
				inStream << (uint8) kStoredPenEventCompressedX;
				inStream << inEvent.penEvent.coords.x;
				inStream << (uint8) inEvent.penEvent.coords.y;
			}
			else
			{
				inStream << (uint8) kStoredPenEvent;
				inStream << inEvent.penEvent.coords.x;
				inStream << inEvent.penEvent.coords.y;
			}

			break;

		case kRecordedAppSwitchEvent:

			inStream << (uint8) kStoredAppSwitchEvent;
			inStream << inEvent.appSwitchEvent.cardNo;
			inStream << inEvent.appSwitchEvent.dbID;
			inStream << inEvent.appSwitchEvent.oldCardNo;
			inStream << inEvent.appSwitchEvent.oldDbID;

			break;

		case kRecordedNullEvent:

			inStream << (uint8) kStoredNullEvent;

			break;

		case kRecordedErrorEvent:

			inStream << (uint8) kStoredErrorEvent;

			break;

		default:

			EmAssert (false);
	}

	return inStream;
}


// ---------------------------------------------------------------------------
//		¥ operator >>
// ---------------------------------------------------------------------------
// Resurrect an EmRecordedEvent from the stream.

EmStream& operator >> (EmStream& inStream, EmRecordedEvent& outEvent)
{
	uint8	temp;
	uint8	storedType;
	inStream >> storedType;

	switch (storedType)
	{
		case kStoredKeyEvent:

			outEvent.eType = kRecordedKeyEvent;

			inStream >> outEvent.keyEvent.ascii;
			inStream >> outEvent.keyEvent.keycode;
			inStream >> outEvent.keyEvent.modifiers;

			break;

		case kStoredKeyEventCompressed:

			outEvent.eType = kRecordedKeyEvent;

			inStream >> temp;
			outEvent.keyEvent.ascii		= temp;
			outEvent.keyEvent.keycode	= 0;
			outEvent.keyEvent.modifiers	= 0;

			break;

		case kStoredPenEvent:

			outEvent.eType				= kRecordedPenEvent;

			inStream >> outEvent.penEvent.coords.x;
			inStream >> outEvent.penEvent.coords.y;

			break;

		case kStoredPenEventCompressedUp:

			outEvent.eType				= kRecordedPenEvent;
			outEvent.penEvent.coords.x	= -1;
			outEvent.penEvent.coords.y	= -1;

			break;

		case kStoredPenEventCompressedX:

			outEvent.eType				= kRecordedPenEvent;

			inStream >> temp;
			outEvent.penEvent.coords.x	= temp;

			inStream >> outEvent.penEvent.coords.y;

			break;

		case kStoredPenEventCompressedY:

			outEvent.eType				= kRecordedPenEvent;

			inStream >> outEvent.penEvent.coords.x;

			inStream >> temp;
			outEvent.penEvent.coords.y	= temp;

			break;

		case kStoredPenEventCompressedXY:

			outEvent.eType				= kRecordedPenEvent;

			inStream >> temp;
			outEvent.penEvent.coords.x	= temp;

			inStream >> temp;
			outEvent.penEvent.coords.y	= temp;

			break;

		case kStoredAppSwitchEvent:

			outEvent.eType		= kRecordedAppSwitchEvent;

			inStream >> outEvent.appSwitchEvent.cardNo;
			inStream >> outEvent.appSwitchEvent.dbID;
			inStream >> outEvent.appSwitchEvent.oldCardNo;
			inStream >> outEvent.appSwitchEvent.oldDbID;

			break;

		case kStoredAppSwitchEventCompressed:

			EmAssert (false);

			break;

		case kStoredNullEvent:

			outEvent.eType		= kRecordedNullEvent;

			break;

		case kStoredNullEventCompressed:

			EmAssert (false);

			break;

		case kStoredErrorEvent:

			outEvent.eType		= kRecordedErrorEvent;

			break;

		case kStoredErrorEventCompressed:

			EmAssert (false);

			break;

		default:

			EmAssert (false);
	}

	return inStream;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ operator <<
// ---------------------------------------------------------------------------
// Flatten a Chunk to the stream.

EmStream&	operator << (EmStream& s, const Chunk& chunk)
{
	s << chunk.GetLength ();
	s.PutBytes (chunk.GetPointer (), chunk.GetLength ());
	return s;
}


// ---------------------------------------------------------------------------
//		¥ operator >>
// ---------------------------------------------------------------------------
// Resurrect a Chunk from the stream.

EmStream&	operator >> (EmStream& s, Chunk& chunk)
{
	long	len;

	s >> len;

	chunk.SetLength (len);
	s.GetBytes (chunk.GetPointer (), len);

	return s;
}


EmRecordedEvent::EmRecordedEvent (void) :
	eType (kRecordedUnknownEvent)
{
}

EmRecordedEvent::EmRecordedEvent (const EmRecordedEvent& other)
{
	*this = other;
}

EmRecordedEvent::~EmRecordedEvent (void)
{
}

EmRecordedEvent& EmRecordedEvent::operator= (const EmRecordedEvent& other)
{
	if (this != &other)
	{
		eType = other.eType;

		switch (eType)
		{
			case kRecordedUnknownEvent:
				EmAssert (false);
				break;

			case kRecordedKeyEvent:
				keyEvent.ascii				= other.keyEvent.ascii;
				keyEvent.keycode			= other.keyEvent.keycode;
				keyEvent.modifiers			= other.keyEvent.modifiers;
				break;

			case kRecordedPenEvent:
				penEvent.coords				= other.penEvent.coords;
				break;

			case kRecordedAppSwitchEvent:
				appSwitchEvent.cardNo		= other.appSwitchEvent.cardNo;
				appSwitchEvent.dbID			= other.appSwitchEvent.dbID;
				appSwitchEvent.oldCardNo	= other.appSwitchEvent.oldCardNo;
				appSwitchEvent.oldDbID		= other.appSwitchEvent.oldDbID;
				break;

			case kRecordedNullEvent:
				break;

			case kRecordedErrorEvent:
				break;
		}
	}

	return *this;
}
