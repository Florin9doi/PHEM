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

#ifndef _PROFILING_H_
#define _PROFILING_H_

#include "sysconfig.h"			// STATIC_INLINE

// wait states vary with hardware

// !!! Yow, this stuff is all wrong!  This sets up values at *compile* time.
// We need them to change at *run* time.

#define PILOT		1
#define	PALM_PILOT	2
#define PALM_III	3
#define PALM_V		5		// also PALM_IIIx
#define PALM_VII	7
#define PALM_VII_EZ	8
#define	AUSTIN		9


#define PALM_HARDWARE_TYPE PALM_III



#if PALM_HARDWARE_TYPE == PILOT

#define WAITSTATES_ROM			2
#define WAITSTATES_SRAM			4	
#define WAITSTATES_DRAM			4	
#define WAITSTATES_REGISTERS	0
#define WAITSTATES_SED1375		4
#define WAITSTATES_PLD			0	// !!! What's this value?
#define WAITSTATES_DUMMYBANK	99	// hopefully won't be used!


#elif PALM_HARDWARE_TYPE == PALM_PILOT

#define WAITSTATES_ROM			2
#define WAITSTATES_SRAM			4	
#define WAITSTATES_DRAM			4	
#define WAITSTATES_REGISTERS	0
#define WAITSTATES_SED1375		4
#define WAITSTATES_PLD			0	// !!! What's this value?
#define WAITSTATES_DUMMYBANK	99	// hopefully won't be used!


#elif PALM_HARDWARE_TYPE == PALM_III

#define WAITSTATES_ROM			2
#define WAITSTATES_SRAM			4	// according to Wayne Harrington
#define WAITSTATES_DRAM			4	// according to Wayne Harrington
#define WAITSTATES_REGISTERS	0
#define WAITSTATES_SED1375		4
#define WAITSTATES_PLD			0	// !!! What's this value?
#define WAITSTATES_DUMMYBANK	99	// hopefully won't be used!

#elif PALM_HARDWARE_TYPE == PALM_V

#define WAITSTATES_ROM			1
#define WAITSTATES_SRAM			1	
#define WAITSTATES_DRAM			1	
#define WAITSTATES_REGISTERS	0
#define WAITSTATES_SED1375		4
#define WAITSTATES_PLD			0	// !!! What's this value?
#define WAITSTATES_DUMMYBANK	99	// hopefully won't be used!


#elif PALM_HARDWARE_TYPE == PALM_VII

#define WAITSTATES_ROM			1
#define WAITSTATES_SRAM			2	// not sure about this yet	
#define WAITSTATES_DRAM			2	// not sure about this yet
#define WAITSTATES_REGISTERS	0
#define WAITSTATES_SED1375		4
#define WAITSTATES_PLD			0	// !!! What's this value?
#define WAITSTATES_DUMMYBANK	99	// hopefully won't be used!

#elif PALM_HARDWARE_TYPE == AUSTIN

#define WAITSTATES_ROM			1
#define WAITSTATES_SRAM			1	
#define WAITSTATES_DRAM			1	
#define WAITSTATES_REGISTERS	0
#define WAITSTATES_SED1375		4
#define WAITSTATES_PLD			0	// !!! What's this value?
#define WAITSTATES_DUMMYBANK	99	// hopefully won't be used!

#elif PALM_HARDWARE_TYPE == PALM_VII_EZ

#define WAITSTATES_ROM			1
#define WAITSTATES_SRAM			1	
#define WAITSTATES_DRAM			1	
#define WAITSTATES_REGISTERS	0
#define WAITSTATES_SED1375		4
#define WAITSTATES_PLD			0	// !!! What's this value?
#define WAITSTATES_DUMMYBANK	99	// hopefully won't be used!

#endif	// PALM_HARDWARE_TYPE




// Standard macros for incrementing our counters

#define CYCLE_GETLONG(waitstates)						\
	if (!(gProfilingEnabled && gProfilingCounted)) ;	\
	else ProfileIncrementRead (2, waitstates)

#define CYCLE_GETWORD(waitstates)						\
	if (!(gProfilingEnabled && gProfilingCounted)) ;	\
	else ProfileIncrementRead (1, waitstates)

#define CYCLE_GETBYTE(waitstates)						\
	if (!(gProfilingEnabled && gProfilingCounted)) ;	\
	else ProfileIncrementRead (1, waitstates)


#define CYCLE_PUTLONG(waitstates)						\
	if (!(gProfilingEnabled && gProfilingCounted)) ;	\
	else ProfileIncrementWrite (2, waitstates)

#define CYCLE_PUTWORD(waitstates)						\
	if (!(gProfilingEnabled && gProfilingCounted)) ;	\
	else ProfileIncrementWrite (1, waitstates)

#define CYCLE_PUTBYTE(waitstates)						\
	if (!(gProfilingEnabled && gProfilingCounted)) ;	\
	else ProfileIncrementWrite (1, waitstates)


	// Declare these before including UAE.h, as UAE.h refers
	// to them (well, gProfilingEnabled at least).

#ifdef __cplusplus
extern "C" {
#endif

// Public (HostControl) functions
extern Bool ProfileCanInit (void);
extern Bool ProfileCanStart (void);
extern Bool ProfileCanStop (void);
extern Bool ProfileCanDump (void);

extern void ProfileInit(int maxCalls, int maxDepth);
extern void ProfileStart();
extern void ProfileStop();

// note, ProfileDump destructively modifies the profile data,
// you must call ProfileCleanup after dumping, do not attempt
// to gather more profiling data.
extern void ProfileDump(const char* dumpFileName);

extern void ProfilePrint(const char* textFileName);
extern void ProfileCleanup();

extern void ProfileDetailFn(emuptr addr, int logInstructions);


// Private (Emulator Only) stuff

	// Indicates that the profiling system has been initialized
	// by calling ProfileInit.  Set back to false after ProfileCleanup.
	// If true, adds calls to ProfileIncrementClock throught the 68K
	// emuilator.  Also controls calls to ProfileFnEnter, ProfileFnExit,
	// ProfileInterruptEnter and ProfileInterruptExit, (but that seems
	// redundant with gProfilingOn).  Finally, enables cycle counting for
	// memory accesses (but only if gProfilingCounted is also true).
extern int gProfilingEnabled;

	// Set to false in ProfileInit and ProfileStop, set to true in ProfileStart.
	// If false, ProfileFnEnter, ProfileFnExit, ProfileInterruptEnter and
	// ProfileInterruptExit merely return.
extern int gProfilingOn;

	// Set to false in ProfileInit, set to true at the start of the loop in
	// Emulator::Execute and false at the end of the loop.
	// It controls whether or not memory access cycles are counted.  It
	// is always checked in conjunction with gProfilingEnabled; both have
	// to be true in order for memory access cycles to be counted.
extern int gProfilingCounted;


	// Set to false in ProfileInit, set to true by HostControl to enable
	// detailed profiling of a given address range (typically one function)
extern int gProfilingDetailed;

#ifdef __cplusplus

class StDisableAllProfiling
{
	public:
		StDisableAllProfiling () :
		  fOld (gProfilingEnabled)
		{
		  gProfilingEnabled = false;
		}

		~StDisableAllProfiling ()
		{
			gProfilingEnabled = fOld;
		}

	private:
		int	fOld;
};

class StDisableFunctionTracking
{
	public:
		StDisableFunctionTracking () :
		  fOld (gProfilingOn)
		{
		  gProfilingOn = false;
		}

		~StDisableFunctionTracking ()
		{
			gProfilingOn = fOld;
		}

	private:
		int	fOld;
};

class StDisableMemoryCycleCounting
{
	public:
		StDisableMemoryCycleCounting () :
		  fOld (gProfilingCounted)
		{
		  gProfilingCounted = false;
		}

		~StDisableMemoryCycleCounting ()
		{
			gProfilingCounted = fOld;
		}

	private:
		int	fOld;
};

#endif


#define DEFAULT_RESULTS_FILENAME	"Profile Results.mwp"
#define DEFAULT_RESULTS_TEXT_FILENAME	"Profile Results.txt"

#define HAS_PROFILING_DEBUG		0	// turn on code to help debug profiler


// еее normally passed in to ProfileInit, but here for CEmulatorApp.cpp
#define MAXFNCALLS		0x10000
#define MAXUNIQUEFNS	2500
#define MAXDEPTH		200



	// Total running clock cycles.  Includes opcode execution
	// time and memory fetch time.
extern int64 gClockCycles;

	// Portion of gClockCycles spent reading memory.
extern int64 gReadCycles;

	// Portion of gClockCycles spent writing memory.
extern int64 gWriteCycles;


#if _DEBUG

void ProfileIncrementClock(unsigned long by);
void ProfileIncrementRead(int reads, int waitstates);
void ProfileIncrementWrite(int writes, int waitstates);

#else

STATIC_INLINE void ProfileIncrementClock(unsigned long by)
{
	gClockCycles += by;
}

STATIC_INLINE void ProfileIncrementRead(int reads, int waitstates)
{
	gClockCycles += reads * (4 + waitstates);
	gReadCycles += reads;
}

STATIC_INLINE void ProfileIncrementWrite(int writes, int waitstates)
{
	gClockCycles += writes * (4 + waitstates);
	gWriteCycles += writes;
}
#endif


extern void ProfileFnEnter(emuptr destAddress, emuptr returnAddress);
extern void ProfileFnExit(emuptr returnAddress, emuptr oldAddress);

extern void ProfileInterruptEnter(int32 iException, emuptr returnAddress);
extern void ProfileInterruptExit(emuptr returnAddress);

extern void ProfileInstructionEnter(emuptr instructionAddress);
extern void ProfileInstructionExit(emuptr instructionAddress);

// debugging stuff

extern long gReadMismatch;
extern long gWriteMismatch;

extern void ProfileTest();


#ifdef __cplusplus
}
#endif


#endif /* _PROFILING_H_ */
  
  
