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

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include "Hordes.h"				// Hordes::IsOn
#include "Miscellaneous.h"		// StMemory
#include "PreferenceMgr.h"		// FOR_EACH_PREF
#include "omnithread.h"			// omni_mutex

#include <stdarg.h>				// va_list
#include <deque>				// deque

typedef deque<uint8>	ByteDeque;

class EmStreamFile;


class LogStreamInner
{
	// Non-multithread-safe version of LogStream.  LogStream
	// acquires a "logging mutex" and calls these functions.

	public:
								LogStreamInner	(const char* baseName);
								~LogStreamInner	(void);

		int						DumpHex			(const void*, long dataLen);
		int						VPrintf			(const char* fmt, va_list args, Bool timestamp = true);
		int						Write			(const void* buffer, long size, Bool timestamp = true);

		void					Clear			(void);

		long					GetLogSize		(void);
		void					SetLogSize		(long);

		void					EnsureNewFile	(void);
		void					DumpToFile		(void);

	private:
		void					DumpToFile			(EmStreamFile&, const char*, long size);
		EmFileRef				CreateFileReference	(void);
		void					Timestamp			(void);
		void					NewLine				(void);
		void					Append				(const char* buffer, long size);
		void					TrimLeading			(void);

		const char*				fBaseName;
		long					fFileIndex;

		ByteDeque				fBuffer;
		long					fBufferSize;
		Bool					fDiscarded;

		int32					fLastGremlinEventCounter;
		uint32					fLastTimestampTime;
		uint32					fBaseTimestampTime ;
		char					fLastTimestampString[30];
};

class LogStream
{
	public:
								LogStream		(const char* baseName);
								~LogStream		(void);

	// Multithread-safe interface for logging information.

		int						Printf			(const char* msg, ...);
		int						PrintfNoTime	(const char* msg, ...);
		int						DataPrintf		(const void*, long dataLen, const char* msg, ...);
		int						VPrintf			(const char* fmt, va_list args);
		int						Write			(const void* buffer, long size);

		void					Clear			(void);

		long					GetLogSize		(void);
		void					SetLogSize		(long);

		void					EnsureNewFile	(void);
		void					DumpToFile		(void);

	private:
		static void				PrefChanged			(PrefKeyType, PrefRefCon);	

	private:
		omni_mutex				fMutex;
		LogStreamInner			fInner;
};

void		LogEvtAddEventToQueue		(const EventType& event);
void		LogEvtAddUniqueEventToQueue	(const EventType& event, UInt32, Boolean);
void		LogEvtEnqueueKey			(UInt16 ascii, UInt16 keycode, UInt16 modifiers);
void		LogEvtEnqueuePenPoint		(const PointType&);

void		LogEvtGetEvent				(const EventType& event, Int32 timeout);
void		LogEvtGetPen				(Int16, Int16, Boolean);
void		LogEvtGetSysEvent			(const EventType& event, Int32 timeout);

LogStream*	LogGetStdLog				(void);
void		LogStartup					(void);
void		LogShutdown					(void);

#define LogAppendMsg		if (!LogGetStdLog ()) ; else LogGetStdLog ()->Printf
#define LogAppendMsgNoTime	if (!LogGetStdLog ()) ; else LogGetStdLog ()->PrintfNoTime
#define LogAppendData		if (!LogGetStdLog ()) ; else LogGetStdLog ()->DataPrintf
#define LogClear			if (!LogGetStdLog ()) ; else LogGetStdLog ()->Clear
#define LogDump				if (!LogGetStdLog ()) ; else LogGetStdLog ()->DumpToFile
#define LogEnsureNewFile	if (!LogGetStdLog ()) ; else LogGetStdLog ()->EnsureNewFile
#define LogStartNew			LogDump (); LogClear (); LogEnsureNewFile


// The LogFoo macros below are called all the time.  When implemented in
// terms of a function that instantiates a Preference<> object all the
// time, they can slow down the overall execution of Poser significantly.
// Therefore, the logging sub-system caches the relevent values and
// works from those, instead.

#define DECLARE_CACHED_PREF_KEYS(name, type, init) kCachedPrefKey##name,
enum
{
	FOR_EACH_PREF(DECLARE_CACHED_PREF_KEYS)
	kCachedPrefKeyDummy
};

extern uint8 gLogCache[];
inline Bool LogCommon(int key)
{
	if (key >= kCachedPrefKeyLogErrorMessages &&
		key <= kCachedPrefKeyLogRPCData)
	{
		if (Hordes::IsOn ())
			return (gLogCache[key] & kGremlinLogging) != 0;

		return (gLogCache[key] & kNormalLogging) != 0;
	}

	EmAssert (false);

	return 0;
}

inline Bool ReportCommon(int key)
{
	if (key >= kCachedPrefKeyReportFreeChunkAccess &&
		key <= kCachedPrefKeyReportUnlockedChunkAccess)
	{
		return gLogCache[key] != 0;
	}

	EmAssert (false);

	return 0;
}

#define FOR_EACH_LOG_PREF(DO_TO_LOG_PREF)	\
	DO_TO_LOG_PREF (LogErrorMessages)		\
	DO_TO_LOG_PREF (LogWarningMessages)		\
	DO_TO_LOG_PREF (LogGremlins)			\
	DO_TO_LOG_PREF (LogCPUOpcodes)			\
	DO_TO_LOG_PREF (LogEnqueuedEvents)		\
	DO_TO_LOG_PREF (LogDequeuedEvents)		\
	DO_TO_LOG_PREF (LogSystemCalls)			\
	DO_TO_LOG_PREF (LogApplicationCalls)	\
	DO_TO_LOG_PREF (LogSerial)				\
	DO_TO_LOG_PREF (LogSerialData)			\
	DO_TO_LOG_PREF (LogNetLib)				\
	DO_TO_LOG_PREF (LogNetLibData)			\
	DO_TO_LOG_PREF (LogExgMgr)				\
	DO_TO_LOG_PREF (LogExgMgrData)			\
	DO_TO_LOG_PREF (LogHLDebugger)			\
	DO_TO_LOG_PREF (LogHLDebuggerData)		\
	DO_TO_LOG_PREF (LogLLDebugger)			\
	DO_TO_LOG_PREF (LogLLDebuggerData)		\
	DO_TO_LOG_PREF (LogRPC)					\
	DO_TO_LOG_PREF (LogRPCData)

#define FOR_EACH_REPORT_PREF(DO_TO_REPORT_PREF)			\
	DO_TO_REPORT_PREF (ReportFreeChunkAccess) 			\
	DO_TO_REPORT_PREF (ReportHardwareRegisterAccess)	\
	DO_TO_REPORT_PREF (ReportLowMemoryAccess) 			\
	DO_TO_REPORT_PREF (ReportLowStackAccess)			\
	DO_TO_REPORT_PREF (ReportMemMgrDataAccess)			\
	DO_TO_REPORT_PREF (ReportMemMgrLeaks) 				\
	DO_TO_REPORT_PREF (ReportMemMgrSemaphore) 			\
	DO_TO_REPORT_PREF (ReportOffscreenObject)			\
	DO_TO_REPORT_PREF (ReportOverlayErrors)				\
	DO_TO_REPORT_PREF (ReportProscribedFunction)		\
	DO_TO_REPORT_PREF (ReportROMAccess)					\
	DO_TO_REPORT_PREF (ReportScreenAccess)				\
	DO_TO_REPORT_PREF (ReportSizelessObject)			\
	DO_TO_REPORT_PREF (ReportStackAlmostOverflow) 		\
	DO_TO_REPORT_PREF (ReportStrictIntlChecks)			\
	DO_TO_REPORT_PREF (ReportSystemGlobalAccess)		\
	DO_TO_REPORT_PREF (ReportUIMgrDataAccess)			\
	DO_TO_REPORT_PREF (ReportUnlockedChunkAccess)

#define FOR_EACH_SCALAR_PREF(DO_TO_SCALAR_PREF)			\
	FOR_EACH_LOG_PREF(DO_TO_SCALAR_PREF)				\
	FOR_EACH_REPORT_PREF(DO_TO_SCALAR_PREF)

#define CREATE_LOG_ACCESSOR(name)	\
	inline Bool name (void) { return LogCommon(kCachedPrefKey##name); }

#define CREATE_REPORT_ACCESSOR(name)	\
	inline Bool name (void) { return ReportCommon(kCachedPrefKey##name); }

FOR_EACH_LOG_PREF(CREATE_LOG_ACCESSOR)
FOR_EACH_REPORT_PREF(CREATE_REPORT_ACCESSOR)

#endif /* _LOGGING_H_ */
