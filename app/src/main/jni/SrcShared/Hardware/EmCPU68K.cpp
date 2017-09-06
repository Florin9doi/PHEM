/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmCPU68K.h"

#include "Byteswapping.h"		// Canonical
#include "DebugMgr.h"			// gExceptionAddress, gExceptionSize, gExceptionForRead
#include "EmBankROM.h"			// EmBankROM::GetMemoryStart
#include "EmEventPlayback.h"	// EmEventPlayback::ReplayingEvents
#include "EmHAL.h"				// EmHAL::GetInterruptLevel
#include "EmMemory.h"			// CEnableFullAccess
#include "EmMinimize.h"			// IsOn
#include "EmSession.h"			// HandleInstructionBreak
#include "Logging.h"			// LogAppendMsg
#include "MetaMemory.h"			// IsCPUBreak
#include "Platform.h"			// GetMilliseconds
#include "SessionFile.h"		// WriteDBallRegs, etc.
#include "StringData.h"			// kExceptionNames
#include "UAE.h"				// cpuop_func, etc.

#include <algorithm>			// find
/* Update for recent GCC */
#include <cstddef>

#if __profile__
#include <Profiler.h>
#endif

#if defined (macintosh) && defined (_DEBUG) && 0
	#define HAS_DEAD_MANS_SWITCH	1
#else
	#define HAS_DEAD_MANS_SWITCH	0
#endif

// !!! Why is this here?  Shouldn't it be in Profiling.h?
#if HAS_PROFILING
perfRec perftbl[65536];
#endif

// Define our own flags for regs.spcflag.  Please do not let these
// overlap with UAE-defined flags (should not fall below 0x0002000)
// and avoid using the high bit just for safety.

#define SPCFLAG_END_OF_CYCLE	(0x40000000)


// Data needed by UAE.

int	areg_byteinc[] = { 1,1,1,1,1,1,1,2 };	// (normally in newcpu.c)
int	imm8_table[] = { 8,1,2,3,4,5,6,7 }; 	// (normally in newcpu.c)

int	movem_index1[256];						// (normally in newcpu.c)
int	movem_index2[256];						// (normally in newcpu.c)
int	movem_next[256];						// (normally in newcpu.c)

cpuop_func*	cpufunctbl[65536];				// (normally in newcpu.c)

uint16	last_op_for_exception_3;			/* Opcode of faulting instruction */
emuptr	last_addr_for_exception_3;			/* PC at fault time */
emuptr	last_fault_for_exception_3; 		/* Address that generated the exception */

struct regstruct	regs;					// (normally in newcpu.c)
struct flag_struct	regflags;				// (normally in support.c)


// These variables should strictly be in a sub-system that implements
// the stack overflow checking, etc.  However, for performance reasons,
// we need to expose them to UAE (see the CHECK_STACK_POINTER_ASSIGNMENT,
// et al macros), so define them here.
//
// Similar comments for the CheckKernelStack function.

uae_u32	gStackHigh;
uae_u32	gStackLowWarn;
uae_u32	gStackLow;
uae_u32	gKernelStackOverflowed;


// Definitions of the stack frames used in EmCPU68K::ProcessException.

#include "PalmPack.h"

struct ExceptionStackFrame1
{
	uint16 statusRegister;
	uint32 programCounter;
};

struct ExceptionStackFrame2
{
	uint16 functionCode;
	uint32 accessAddress;
	uint16 instructionRegister;
	uint16 statusRegister;
	uint32 programCounter;
};

#include "PalmPackPop.h"


EmCPU68K*	gCPU68K;


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::Cycle
// ---------------------------------------------------------------------------

// this guy is a macro instead of an inline function so that "counter" can
// be declared as a variable local to the calling function.  The resulting
// code can be more efficient if "counter" can be cached in a register
// instead of being a static or global variable.

#define CYCLE(sleeping)															\
{																				\
	/* Don't do anything if we're in the middle of an ATrap call.  We don't */	\
	/* need interrupts firing or tmr counters incrementing. */					\
																				\
	EmAssert (session);															\
	if (!session->IsNested ())													\
	{																			\
		/* Perform CPU-specific idling. */										\
																				\
		EmHAL::Cycle (sleeping);												\
																				\
		/* Perform expensive operations. */										\
																				\
		if (sleeping || ((++counter & 0x7FFF) == 0))							\
		{																		\
			this->CycleSlowly (sleeping);										\
		}																		\
	}																			\
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::EmCPU68K
// ---------------------------------------------------------------------------

EmCPU68K::EmCPU68K (EmSession* session) :
	EmCPU (session),
	fLastTraceAddress (EmMemNULL),
	fCycleCount (0),
//	fExceptionHandlers (),
	fHookJSR (),
	fHookJSR_Ind (),
	fHookLINK (),
	fHookRTE (),
	fHookRTS (),
	fHookNewPC (),
	fHookNewSP ()
#if REGISTER_HISTORY
	, fRegHistoryIndex (0)
//	, fRegHistory ()
#endif
#if EXCEPTION_HISTORY
	, fExceptionHistoryIndex (0)
//	, fExceptionHistory ()
#endif
{
	this->InitializeUAETables ();

	EmAssert (gCPU68K == NULL);
	gCPU68K = this;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::~EmCPU68K
// ---------------------------------------------------------------------------

EmCPU68K::~EmCPU68K (void)
{
	EmAssert (gCPU68K == this);
	gCPU68K = NULL;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmCPU68K::Reset
// ---------------------------------------------------------------------------

void EmCPU68K::Reset (Bool hardwareReset)
{
	fLastTraceAddress		= EmMemNULL;
	fCycleCount				= 0;

#if REGISTER_HISTORY
	fRegHistoryIndex		= 0;
#endif

#if EXCEPTION_HISTORY
	fExceptionHistoryIndex	= 0;
#endif

	gStackHigh				= EmMemEOM;
	gStackLowWarn			= EmMemNULL;
	gStackLow				= EmMemNULL;
	gKernelStackOverflowed	= false;

	if (hardwareReset)
	{
		// (taken from m68k_reset in newcpu.c)

		// !!! I think that we really need to emulate ROM appearing at NULL.
		// That would break our dependency on EmBankROM.

		emuptr romStart = EmBankROM::GetMemoryStart ();
		m68k_areg (regs, 7) = EmMemGet32 (romStart);
		m68k_setpc (EmMemGet32 (romStart + 4));

		// Note, on the 68K, the t0 and m flags must always be zero.

		regs.prefetch	= 0x0000;
		regs.kick_mask	= 0xF80000;	// (not a 68K register)
		regs.s			= 1;		// supervisor/user state
		regs.m			= 0;		// master/interrupt state
		regs.stopped	= 0;		// (not a 68K register)
		regs.t1 		= 0;		// 1 = trace on any instruction
		regs.t0 		= 0;		// 1 = trace on change of flow
		SET_ZFLG (0);
		SET_XFLG (0);
		SET_CFLG (0);
		SET_VFLG (0);
		SET_NFLG (0);
		regs.spcflags	= 0;		// (not a 68K register)
		regs.intmask	= 7;		// disable all interrupts
		regs.vbr = regs.sfc = regs.dfc = 0;
		regs.fpcr = regs.fpsr = regs.fpiar = 0;
	}

	Memory::CheckNewPC (m68k_getpc ());
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::Save
// ---------------------------------------------------------------------------

void EmCPU68K::Save (SessionFile& f)
{
	// Write out the CPU Registers

	regstruct	tempRegs;
	this->GetRegisters (tempRegs);

	Canonical (tempRegs);
	f.WriteDBallRegs (tempRegs);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::Load
// ---------------------------------------------------------------------------

void EmCPU68K::Load (SessionFile& f)
{
	// Read in the CPU Registers.

	regstruct	tempRegs;
	f.ReadDBallRegs (tempRegs);

	Canonical (tempRegs);
	this->SetRegisters (tempRegs);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmCPU68K::Execute
// ---------------------------------------------------------------------------

void EmCPU68K::Execute (void)
{
	// This function is the bottleneck for all 68K emulation.  It's
	// important that it run as quickly as possible.  To that end,
	// fine tune register allocation as much as we can by hand.

#if defined(__powerc) || defined(powerc) || \
	defined(__powerpc) || defined(powerpc) || \
	defined(__ppc__) || defined(ppc)

	register int			counter			= 0;
	register cpuop_func**	functable		= cpufunctbl;

	register uint8**		pc_p_p			= &regs.pc_p;
	register uint8**		pc_meta_oldp_p	= &regs.pc_meta_oldp;
	register uint8**		pc_oldp_p		= &regs.pc_oldp;
	register uint32*		spcflags_p		= &regs.spcflags;
	register EmSession*		session			= fSession;

	#define pc_p			(*pc_p_p)
	#define pc_meta_oldp	(*pc_meta_oldp_p)
	#define pc_oldp			(*pc_oldp_p)
	#define spcflags		(*spcflags_p)

#elif defined(_MSC_VER) && defined(_M_IX86)

	register int			counter			= 0;
	register cpuop_func**	functable		= cpufunctbl;

	#define pc_p			(regs.pc_p)
	#define pc_meta_oldp	(regs.pc_meta_oldp)
	#define pc_oldp			(regs.pc_oldp)
	#define spcflags		(regs.spcflags)
	#define session			(fSession)

#else

	register int			counter			= 0;
	register cpuop_func**	functable		= cpufunctbl;

	#define pc_p			(regs.pc_p)
	#define pc_meta_oldp	(regs.pc_meta_oldp)
	#define pc_oldp			(regs.pc_oldp)
	#define spcflags		(regs.spcflags)
	#define session			(fSession)

#endif

#if HAS_PROFILING_DEBUG
	UInt64	readCyclesSaved		= 0;
	UInt64	writeCyclesSaved	= 0;
	UInt64	clockCyclesSaved	= 0;
#endif

#if HAS_DEAD_MANS_SWITCH
	// -----------------------------------------------------------------------
	// Put in a little dead-man's switch. If this function doesn't exit for a
	// long time, let us get into the debugger.
	// -----------------------------------------------------------------------

	uint32	deadManNow;
	uint32	deadManStart = Platform::GetMilliseconds ();
#endif

	// -----------------------------------------------------------------------
	// Check for the stopped flag before entering the "execute an opcode"
	// section.  It could be that we last exited the loop while still in stop
	// mode, and we need to wind our way back down to that spot.
	// -----------------------------------------------------------------------

	if ((spcflags & SPCFLAG_STOP) != 0)
		goto StoppedLoop;

	while (1)
	{
#if REGISTER_HISTORY
		// -----------------------------------------------------------------------
		// Save the registers for the post-mortem, but don't record the
		// instructions we generate when calling the ROM as a subroutine.  We want
		// those to be as transparent as possible.	In particular, we don't want
		// any functions that we call as part of figuring out why a problem
		// occurred to knock the problem-causing registers off of our array.
		// -----------------------------------------------------------------------

//		if (!session->IsNested ())
		{
			++fRegHistoryIndex;
			fRegHistory[fRegHistoryIndex & (kRegHistorySize - 1)] = regs;
		}
#endif

#if HAS_DEAD_MANS_SWITCH
		// -----------------------------------------------------------------------
		// Put in a little dead-man's switch. If this function doesn't exit for a
		// long time, let us get into the debugger.
		// -----------------------------------------------------------------------

		deadManNow = Platform::GetMilliseconds ();
		if ((deadManNow - deadManStart) > 5000)
		{
			Platform::Debugger ();
		}
#endif

		// -----------------------------------------------------------------------
		// See if we need to halt CPU execution at this location.  We could need
		// to do this for several reasons, including hitting soft breakpoints or
		// needing to execute tailpatches.
		// -----------------------------------------------------------------------

		if (MetaMemory::IsCPUBreak (pc_meta_oldp + (pc_p - pc_oldp)))
		{
			EmAssert (session);
			session->HandleInstructionBreak ();
		}

#if HAS_PROFILING
		emuptr	pcStart;
		pcStart = m68k_getpc ();

		if (gProfilingEnabled)
		{
			// If detailed, log instruction here

			if (gProfilingDetailed)
			{
				ProfileInstructionEnter (pcStart);
			}

#if HAS_PROFILING_DEBUG
			readCyclesSaved		= gReadCycles;
			writeCyclesSaved	= gWriteCycles;
			clockCyclesSaved	= gClockCycles;
#endif

			// Turn gProfilingCounted on here so the opcode fetch below is counted.

			gProfilingCounted = true;
		}
#endif

		// =======================================================================
		// Execute the opcode.
		// -----------------------------------------------------------------------
		//	Interestingly, defining "opcode" as an EmOpcode68K (which is a uint32)
		//	instead of a uint16 (which is all you need to hold the opcode), makes
		//	a big difference in performance (at least, on Intel).  The former
		//	generates:
		//
		//		mov		edx, DWORD PTR _regs+92
		//		xor		eax, eax
		//		mov		ax, WORD PTR [edx]
		//		push	eax
		//		call	DWORD PTR [edi+eax*4]
		//
		//	while the latter generates:
		//
		//		mov		edx, DWORD PTR _regs+92
		//		mov		ax, WORD PTR [edx]
		//		and		eax, 65535		; 0000ffffH
		//		push	eax
		//		call	DWORD PTR [edi+eax*4]
		//
		//	This results in an amazing 5% performance difference.  What makes it
		//	even more amazing is that similar optimizations elsewhere don't seem
		//	to help.  For instance, in EmCPU68K::Cycle, there's the expression:
		//
		//		((++gCounter) & 0x7FFF == 0)
		//
		//	Changing gCounter to a uint16, or changing the expression to:
		//
		//		((uint16) ++gCounter) == 0)
		//
		//	both result in the period doubling, but also in better code
		//	generation.  However, neither gives better performance.  Additionally,
		//	I tried like heck to optimize the call to IsCPUBreak above.  I added
		//	a preflight check that would skip 70% of the calls to IsCPUBreak (as
		//	well as the calculation of the parameter passed to it) and re-
		//	organized the code so that branch prediction would work.  However,
		//	none of that resulted in better performance.
		// -----------------------------------------------------------------------
		EmOpcode68K	opcode;
	//	opcode = get_iword (0);
#if HAS_PROFILING
		if (gProfilingEnabled)
			get_word(regs.pc + ((char*) pc_p - (char*) pc_oldp));
#endif
		opcode = do_get_mem_word (pc_p);
		fCycleCount += (functable[opcode]) (opcode);
		// =======================================================================

#if HAS_PROFILING
		if (gProfilingEnabled)
		{
			// Add in the extra time taken to execute the instruction.

			ProfileIncrementClock (perftbl[opcode].extraCycles);

			// Detail (instruction level) profiling support

			if (gProfilingDetailed)
			{
				ProfileInstructionExit (pcStart);
			}

#if HAS_PROFILING_DEBUG
			// Validity check on EmMemory stuff.

			Boolean tryAgain = false;

			if (perftbl[opcode].readCycles != 0xFF &&
				gReadCycles - readCyclesSaved != perftbl[opcode].readCycles)
			{
				gReadMismatch += gReadCycles - readCyclesSaved - perftbl[opcode].readCycles;
			}

			if (perftbl[opcode].writeCycles != 0xFF &&
				gWriteCycles - writeCyclesSaved != perftbl[opcode].writeCycles)
			{
				gWriteMismatch += gWriteCycles - writeCyclesSaved - perftbl[opcode].writeCycles;
			}

			if (tryAgain)
			{
				(functable[opcode]) (opcode);
			}
#endif
		}
#endif

		// Perform periodic tasks.

		CYCLE (false);

StoppedLoop:

		// -----------------------------------------------------------------------
		// Handle special conditions.  NB: the code reached by calling
		// EmCPU68K::ExecuteSpecial used to be inline in this function.  Moving it
		// out (thus simplifying both EmCPU68K::Execute and EmCPU68K::ExecuteSpecial)
		// sped up the CPU loop by 9%!
		// -----------------------------------------------------------------------

		if (spcflags)
		{
			if (this->ExecuteSpecial ())
				break;
		}

#if HAS_PROFILING
		gProfilingCounted = false;
#endif
	}	// while (1)
	
#undef pc_p
#undef pc_meta_oldp
#undef pc_oldp
#undef spcflags
#undef session
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ExecuteSimple
// ---------------------------------------------------------------------------

/*
	This is the same loop with the following removed:
	
	* Comments
	* Profiling code
	* Profiling debugging code
	* Dead mans's check on Mac
	* Register history recording

	It's essentially what we get in Release/Non-profile builds.
*/

#if 0

void EmCPU68K::ExecuteSimple (void)
{
	if ((regs.spcflags & SPCFLAG_STOP) != 0)
		goto StoppedLoop;

	while (1)
	{
		if (MetaMemory::IsCPUBreak (regs.pc_meta_oldp + (regs.pc_p - regs.pc_oldp)))
		{
			this->HandleInstructionBreak ();
		}

		EmOpcode68K	opcode;
		opcode = get_iword (0);
		fCycleCount += cpufunctbl[opcode] (opcode);

		this->Cycle (false);

StoppedLoop:
		if (regs.spcflags)
		{
			if (this->ExecuteSpecial ())
				break;
		}
	}
}

#endif


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ExecuteSpecial
// ---------------------------------------------------------------------------

Bool EmCPU68K::ExecuteSpecial (void)
{
	// If we're making subroutine calls, then all we process are requests
	// to break from the CPU loop.  We don't want interrupts, tracing, etc.
	// getting in the way.

	EmAssert (fSession);
	if (fSession->IsNested () != 0)
	{
		return this->CheckForBreak ();
	}

	// Check for Reset first.  If this is set, don't do anything else.

	if ((regs.spcflags & SPCFLAG_END_OF_CYCLE))
	{		
		if (fSession->ExecuteSpecial (true))
			return true;
	}

	// Execute UAE spcflags-handling code (from do_specialties in newcpu.c).

	// If we're tracing, execute the trace vector.
	//
	// The check for SPCFLAG_STOP was added in Poser.  It's needed
	// if we're re-entering ExecuteStopped loop on the Mac after
	// exiting in order to handle events.

	if ((regs.spcflags & SPCFLAG_DOTRACE) && !(regs.spcflags & SPCFLAG_STOP))
	{
		this->ProcessException (kException_Trace);
	}

	if (regs.spcflags & SPCFLAG_STOP)
	{
		if (this->ExecuteStoppedLoop ())
		{
			regs.spcflags &= ~SPCFLAG_BRK;
			return true;
		}
	}

	// Do trace-mode stuff (do_trace from newcpu.c does more,
	// but it's only needed for CPU_LEVEL > 0)

	if (regs.spcflags & SPCFLAG_TRACE)
	{
		if (regs.t1)
		{
			fLastTraceAddress = m68k_getpc ();
			regs.spcflags &= ~SPCFLAG_TRACE;
			regs.spcflags |= SPCFLAG_DOTRACE;
		}
	}

	if ((regs.spcflags & SPCFLAG_DOINT) && !gKernelStackOverflowed)
	{
		int32 interruptLevel = EmHAL::GetInterruptLevel ();
		regs.spcflags &= ~SPCFLAG_DOINT;	// was ~(SPCFLAG_INT | SPCFLAG_DOINT) in Greg and Craig, but the latest UAE has this
		if ((interruptLevel != -1) && (interruptLevel > regs.intmask))
		{
			this->ProcessInterrupt (interruptLevel);
			regs.stopped = 0;
		}
	}

	if (regs.spcflags & SPCFLAG_INT)
	{
		regs.spcflags &= ~SPCFLAG_INT;
		regs.spcflags |= SPCFLAG_DOINT;
	}

	// Check for Poser end-of-cycle operations.  This is inserted
	// before the standard UAE check of the SPCFLAG_BRK flag.

	if ((regs.spcflags & SPCFLAG_END_OF_CYCLE))
	{
		regs.spcflags &= ~SPCFLAG_END_OF_CYCLE;
		
		if (fSession->ExecuteSpecial (false))
			return true;
	}

	return this->CheckForBreak ();
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ExecuteStoppedLoop
// ---------------------------------------------------------------------------

Bool EmCPU68K::ExecuteStoppedLoop (void)
{
	register EmSession*		session			= fSession;

	EmAssert (session);
	EmAssert (!session->IsNested ());
	EmAssert (regs.intmask < 7);

	// If we're running Gremlins and the device goes to sleep (as opposed
	// to just dozing), pretend the user clicked on the Power button).

	if (EmHAL::GetAsleep () && !session->HasButtonEvent ())
	{
		// Turns Hordes off for a moment, as pen events are rejected
		// when it's running!

		Bool	hordesWasOn = false;
		Bool	playbackWasOn = false;
		Bool	minimizationWasOn = false;

		if (Hordes::IsOn ())
		{
			Hordes::TurnOn (false);
			hordesWasOn = true;
		}

		if (EmEventPlayback::ReplayingEvents ())
		{
			EmEventPlayback::ReplayEvents (false);
			playbackWasOn = true;
		}

		if (EmMinimize::IsOn ())
		{
			EmMinimize::TurnOn (false);
			minimizationWasOn = true;
		}

		/*
			When posting the button down event for the Power button, include
			a flag that causes the event to be posted to the hardware NOW.
			Normally, EmSession employs a throttling mechanism that prevents
			button events from showing up in the hardware too quickly.  This
			throttling allows the time for the OS to wakeup and respond to
			the hardware interrupt.
			
			However, the throttling also leads to an incredibly subtle bug
			if it's used unconditionally.  Consider the following sequence
			of events:
			
			*	Gremlins creates a vchrAutoOff event
			*	The system responds to it and goes to sleep.
			*	Poser notices the device is asleep while running a Gremlin,
				and posts Power button down/up events.
			*	It's been more than 100msec since the last button event, so
				the button event is given to the hardware immediately.
			*	The system wakes up, notices that the Power key is down, and
				sets the flag that says it should stay awake.
			*	Gremlins starts posting events again.
			*	Within 100msecs, Gremlins generates another vchrAutoOff event.
			*	The devices goes to sleep again.  Somewhere in the process of
				dealing with the  vchrAutoOff event, the Power-button up event
				is popped off and handled.
			*	Poser posts Power button down/up events again.
			*	Because it's been within 100msecs of the last time a button
				event was popped off the queue, these events are held in the
				queue for a while.
			*	An RTC interrupt occurs.
			*	The device wakes up, notices that the Power is not down, and
				does NOT set the bit that says to stay awake.
			*	The devices goes back to sleep after processing the RTC
				interrupt.  Note that there are still two Power-button events
				in EmSession's button queue.
			*	Poser notices that the device is off, and posts *another*
				pair of Power-button down/up events.
			*	At this point, Poser is in a weird state, where there are too
				many events in the queue.  In fact, the situation will get
				worse, with more and more events piling up and the system
				deals with turning itself on and off in response to the pending
				button events.  The result is that the user sees the device
				flashing on and off.

			In order to avoid this problem, have the Power button posted to
			the hardware registers immediately, before an interrupt can get
			in the way.
			
			Note that there is another potential bug here.  We avoid it for
			now, but be careful not to introduce it later.  By passing "true"
			to PostButtonEvent, we can make the button event available to
			the hardware immediately.  However, the event is not forced onto
			the hardware.  Instead, the hardware polls for any pending button
			events at some reasonable rate.  That is, even if EmSession is
			prepared to say that there is a pending button event, EmCPU may
			not pick up on it until some time later based on its checking
			frequency.
			
			We avoid this problem for now because the hardware checks for
			button events in EmHAL::CycleSlowly, which is called
			unconditionally when the processor is moved out of the "stopped"
			state.
		*/

		if (hordesWasOn || playbackWasOn || minimizationWasOn)
		{
			EmButtonEvent	event (kElement_PowerButton, true);
			session->PostButtonEvent (event, true);

			event.fButtonIsDown = false;
			session->PostButtonEvent (event);
		}

		if (hordesWasOn)
		{
			Hordes::TurnOn (true);
		}

		if (playbackWasOn)
		{
			EmEventPlayback::ReplayEvents (true);
		}

		if (minimizationWasOn)
		{
			EmMinimize::TurnOn (true);
		}
	}

	int	counter = 0;

	// While the CPU is stopped (because a STOP instruction was
	// executed) do some idle tasks.

#if HAS_DEAD_MANS_SWITCH
	// -----------------------------------------------------------------------
	// Put in a little dead-man's switch. If this function doesn't
	// exit for a long time, let us get into the debugger.
	// -----------------------------------------------------------------------

	uint32 deadManStart = Platform::GetMilliseconds ();
#endif

	do {
#if HAS_DEAD_MANS_SWITCH
		// -----------------------------------------------------------------------
		// Put in a little dead-man's switch. If this function doesn't
		// exit for a long time, let us get into the debugger.
		// -----------------------------------------------------------------------

		uint32 deadManNow = Platform::GetMilliseconds ();
		if ((deadManNow - deadManStart) > 5000)
		{
			Platform::Debugger ();
		}
#endif

		// -----------------------------------------------------------------------
		// Slow down processing so that the timer used
		// to increment the tickcount doesn't run too quickly.
		// -----------------------------------------------------------------------

#if __profile__
	short	oldStatus = ProfilerGetStatus ();
	ProfilerSetStatus (false);
#endif

		Platform::Delay ();

#if __profile__
	ProfilerSetStatus (oldStatus);
#endif

		// Perform periodic tasks.

		CYCLE (true);

		// Process an interrupt (see if it's time to wake up).

		if (regs.spcflags & (SPCFLAG_INT | SPCFLAG_DOINT))
		{
			int32 interruptLevel = EmHAL::GetInterruptLevel ();

			regs.spcflags &= ~(SPCFLAG_INT | SPCFLAG_DOINT);

			if ((interruptLevel != -1) && (interruptLevel > regs.intmask))
			{
				this->ProcessInterrupt (interruptLevel);
				regs.stopped = 0;
				regs.spcflags &= ~SPCFLAG_STOP;
			}
		}

		if (this->CheckForBreak ())
		{
			return true;
		}
	} while (regs.spcflags & SPCFLAG_STOP);

	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::CycleSlowly
// ---------------------------------------------------------------------------

void EmCPU68K::CycleSlowly (Bool sleeping)
{
	EmHAL::CycleSlowly (sleeping);

	// Do some platform-specific stuff.

	Platform::CycleSlowly ();

#if HAS_OMNI_THREAD
	// Check to see if some external thread has asked us to quit.

	EmAssert (fSession);
	omni_mutex_lock	lock (fSession->fSharedLock);

	if (fSession->fSuspendState.fAllCounters)
	{
		this->CheckAfterCycle ();
	}
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::CheckAfterCycle
// ---------------------------------------------------------------------------

void EmCPU68K::CheckAfterCycle (void)
{
	regs.spcflags |= SPCFLAG_END_OF_CYCLE;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::GetPC
// ---------------------------------------------------------------------------

emuptr EmCPU68K::GetPC (void)
{
//	return this->GetRegister (e68KRegID_PC);
	return m68k_getpc ();
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::GetSP
// ---------------------------------------------------------------------------

emuptr EmCPU68K::GetSP (void)
{
//	return this->GetRegister (e68KRegID_SSP);
	return m68k_areg (regs, 7);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::GetRegister
// ---------------------------------------------------------------------------

uint32 EmCPU68K::GetRegister (int index)
{
	uint32	result = 0;

	switch (index)
	{
		case e68KRegID_D0:	result = m68k_dreg (regs, 0);	break;
		case e68KRegID_D1:	result = m68k_dreg (regs, 1);	break;
		case e68KRegID_D2:	result = m68k_dreg (regs, 2);	break;
		case e68KRegID_D3:	result = m68k_dreg (regs, 3);	break;
		case e68KRegID_D4:	result = m68k_dreg (regs, 4);	break;
		case e68KRegID_D5:	result = m68k_dreg (regs, 5);	break;
		case e68KRegID_D6:	result = m68k_dreg (regs, 6);	break;
		case e68KRegID_D7:	result = m68k_dreg (regs, 7);	break;
		case e68KRegID_A0:	result = m68k_areg (regs, 0);	break;
		case e68KRegID_A1:	result = m68k_areg (regs, 1);	break;
		case e68KRegID_A2:	result = m68k_areg (regs, 2);	break;
		case e68KRegID_A3:	result = m68k_areg (regs, 3);	break;
		case e68KRegID_A4:	result = m68k_areg (regs, 4);	break;
		case e68KRegID_A5:	result = m68k_areg (regs, 5);	break;
		case e68KRegID_A6:	result = m68k_areg (regs, 6);	break;
		case e68KRegID_USP:	result = m68k_areg (regs, 7);	break;
		case e68KRegID_SSP:	result = m68k_areg (regs, 7);	break;
		case e68KRegID_PC:	result = m68k_getpc ();			break;
		case e68KRegID_SR:
			this->UpdateSRFromRegisters ();
			result = regs.sr;
			break;

		default:
			EmAssert (false);
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::SetPC
// ---------------------------------------------------------------------------

void EmCPU68K::SetPC (emuptr newPC)
{
	this->SetRegister (e68KRegID_PC, newPC);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::SetSP
// ---------------------------------------------------------------------------

void EmCPU68K::SetSP (emuptr newPC)
{
	this->SetRegister (e68KRegID_SSP, newPC);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::SetRegister
// ---------------------------------------------------------------------------

void EmCPU68K::SetRegister (int index, uint32 val)
{
	switch (index)
	{
		case e68KRegID_D0:	m68k_dreg (regs, 0) = val;	break;
		case e68KRegID_D1:	m68k_dreg (regs, 1) = val;	break;
		case e68KRegID_D2:	m68k_dreg (regs, 2) = val;	break;
		case e68KRegID_D3:	m68k_dreg (regs, 3) = val;	break;
		case e68KRegID_D4:	m68k_dreg (regs, 4) = val;	break;
		case e68KRegID_D5:	m68k_dreg (regs, 5) = val;	break;
		case e68KRegID_D6:	m68k_dreg (regs, 6) = val;	break;
		case e68KRegID_D7:	m68k_dreg (regs, 7) = val;	break;
		case e68KRegID_A0:	m68k_areg (regs, 0) = val;	break;
		case e68KRegID_A1:	m68k_areg (regs, 1) = val;	break;
		case e68KRegID_A2:	m68k_areg (regs, 2) = val;	break;
		case e68KRegID_A3:	m68k_areg (regs, 3) = val;	break;
		case e68KRegID_A4:	m68k_areg (regs, 4) = val;	break;
		case e68KRegID_A5:	m68k_areg (regs, 5) = val;	break;
		case e68KRegID_A6:	m68k_areg (regs, 6) = val;	break;
		case e68KRegID_USP:	m68k_areg (regs, 7) = val;	break;
		case e68KRegID_SSP:	m68k_areg (regs, 7) = val;	break;
		case e68KRegID_PC:	m68k_setpc (val);			break;
		case e68KRegID_SR:
			regs.sr = val;
			this->UpdateRegistersFromSR ();
			break;

		default:
			EmAssert (false);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::Stopped
// ---------------------------------------------------------------------------
// Return whether or not the CPU itself is halted.  This is seperate from
// whether or not the session (that is, the thread emulating the CPU) is
// halted.

Bool EmCPU68K::Stopped (void)
{
	return regs.stopped;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::CheckForBreak
// ---------------------------------------------------------------------------
// Check to see if the conditions tell us to break from the CPU Execute loop.

Bool EmCPU68K::CheckForBreak (void)
{
	if ((regs.spcflags & SPCFLAG_BRK) != 0)
	{
		regs.spcflags &= ~SPCFLAG_BRK;
		return true;
	}

	EmAssert (fSession);
	return fSession->CheckForBreak ();
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ProcessInterrupt
// ---------------------------------------------------------------------------

void EmCPU68K::ProcessInterrupt (int32 interrupt)
{
	this->ProcessException ((ExceptionNumber) (EmHAL::GetInterruptBase () + interrupt));

	regs.intmask = interrupt;
	regs.spcflags |= SPCFLAG_INT;	// Check for higher-level interrupts
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ProcessException
// ---------------------------------------------------------------------------

void EmCPU68K::ProcessException (ExceptionNumber exception)
{
	// Make sure the Status Register is up-to-date.

	this->UpdateSRFromRegisters ();

#if EXCEPTION_HISTORY
	// Save the exception for the post-mortem

	fExceptionHistoryIndex++;

	long	index = fExceptionHistoryIndex & (kExceptionHistorySize - 1);
	fExceptionHistory[index].name = kExceptionNames[exception];

	fExceptionHistory[index].pc = m68k_getpc ();
	fExceptionHistory[index].sp = m68k_areg (regs, 7);
#endif

#if HAS_PROFILING
	// Don't count cycles spent in exception handlers against functions.

	// Get and remember the current PC here.  This is important to make the
	// profiling routines come out right.  If Poser decides to completely
	// handle the exception and profiling is on, then the handler will return
	// true (setting "handled" to true), and adjust the program counter.
	// Since "handled" is true, Poser calls ProfileInterruptExit, this time
	// with the current PC.  ProfileInterruptExit sees the adjusted PC, and
	// determines that the funky TRAP $F thing is going on, resulting in it
	// pushing a new function-call record on its stack.  However, since Poser
	// has completely handled the interrupt, we don't want that to happen.
	// All we want is to remove the entry recorded by ProfileInterruptEnter.
	// By saving the current PC value here and passing it to ProfileInterruptExit
	// later, we achieve that affect.  ProfileInterruptExit will record an
	// "interrupt mismatch", but I can live with that...

	emuptr curpc = m68k_getpc ();

	if (gProfilingEnabled)
	{
		ProfileInterruptEnter (exception, curpc);
	}
#endif

	// Let any custom exception handler have a go at it.  If it returns
	// true, it handled it completely, and we don't have anything else to do.
	//
	// By "handled it completely", we could mean one of two things.  (1)
	// that Poser completely replaced the system function and the ROM
	// function was skipped, or (2) Poser dealt with the trap dispatch
	// process, setting the PC to the function entry point.
	//
	// If profiling is on, (2) never occurs because we want to profile the
	// trap dispatcher itself.  Therefore, only (1) is possible.

	Bool							handled = false;
	Hook68KExceptionList&			fns = fExceptionHandlers[exception];
	Hook68KExceptionList::iterator	iter = fns.begin ();
	while (iter != fns.end ())
	{
		if ((*iter) (exception))
		{
			handled = true;
		}

		++iter;
	}

	if (handled)
	{
#if HAS_PROFILING
		if (gProfilingEnabled)
		{
			ProfileInterruptExit (curpc);
		}
#endif
		return;
	}

	// The following is vaguely modelled after Exception() from newcpu.c
	// (The call to MakeSR appears at the start of this method).

	// If not in supervisor mode, set the usp, restore A7 from the isp,
	// and transfer to supervisor mode.

	if (!regs.s)
	{
		regs.usp = m68k_areg (regs, 7);
		m68k_areg (regs, 7) = regs.isp;
		regs.s = 1;
	}

	// Set up the stack frame.
	// !!! If we're handling a trace exception, I think that fLastTraceAddress
	// comes into play here instead of m68k_getpc.
	// !!! Manage this with EmPalmStructs...

	if (exception == kException_BusErr || exception == kException_AddressErr)
	{
		COMPILE_TIME_ASSERT (sizeof (ExceptionStackFrame2) == 14);
		m68k_areg (regs, 7) -= sizeof (ExceptionStackFrame2);
		CHECK_STACK_POINTER_DECREMENT ();

		emuptr frame = m68k_areg (regs, 7);

		// Eh...Palm OS doesn't use these 3 anyway...
		EmMemPut16 (frame + offsetof (ExceptionStackFrame2, functionCode), 0);
		EmMemPut32 (frame + offsetof (ExceptionStackFrame2, accessAddress), 0);
		EmMemPut16 (frame + offsetof (ExceptionStackFrame2, instructionRegister), 0);

		EmMemPut16 (frame + offsetof (ExceptionStackFrame2, statusRegister), regs.sr);
		EmMemPut32 (frame + offsetof (ExceptionStackFrame2, programCounter), m68k_getpc ());
	}
	else
	{
		COMPILE_TIME_ASSERT (sizeof (ExceptionStackFrame1) == 6);
		m68k_areg (regs, 7) -= sizeof (ExceptionStackFrame1);
		CHECK_STACK_POINTER_DECREMENT ();

		emuptr frame = m68k_areg (regs, 7);

		EmMemPut16 (frame + offsetof (ExceptionStackFrame1, statusRegister), regs.sr);
		EmMemPut32 (frame + offsetof (ExceptionStackFrame1, programCounter), m68k_getpc ());
	}

	emuptr newpc;
	{
		CEnableFullAccess	munge;	// Remove blocks on memory access.

		// Get the exception handler address.
		newpc = EmMemGet32 (regs.vbr + 4 * exception);
	}

	// Check the exception handler address and jam it into the PC.

	this->CheckNewPC (newpc);
	m68k_setpc (newpc);

	// Turn tracing off.

	regs.t1 = regs.t0 = regs.m = 0;
	regs.spcflags &= ~(SPCFLAG_TRACE | SPCFLAG_DOTRACE);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ProcessIllegalInstruction
// ---------------------------------------------------------------------------

void EmCPU68K::ProcessIllegalInstruction (EmOpcode68K opcode)
{
	// This function is loosely based on op_illg in newcpu.c

	// Process an FTrap.

	if ((opcode & 0xF000) == 0xF000)
	{
		this->ProcessException (kException_FTrap);
	}

	// Process an ATrap.

	else if ((opcode & 0xF000) == 0xA000)
	{
		this->ProcessException (kException_ATrap);
	}

	// Process all other opcodes.

	else
	{
		this->ProcessException (kException_IllegalInstr);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ProcessJSR
// ---------------------------------------------------------------------------

int EmCPU68K::ProcessJSR (emuptr oldPC, emuptr dest)
{
	int	handled = false;

	Hook68KJSRList::iterator	iter = fHookJSR.begin ();
	while (iter != fHookJSR.end ())
	{
		if ((*iter) (oldPC, dest))
		{
			handled = true;
		}

		++iter;
	}

	return handled;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ProcessJSR_Ind
// ---------------------------------------------------------------------------

int EmCPU68K::ProcessJSR_Ind (emuptr oldPC, emuptr dest)
{
	int	handled = false;

	Hook68KJSR_IndList::iterator	iter = fHookJSR_Ind.begin ();
	while (iter != fHookJSR_Ind.end ())
	{
		if ((*iter) (oldPC, dest))
		{
			handled = true;
		}

		++iter;
	}

	return handled;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ProcessLINK
// ---------------------------------------------------------------------------

void EmCPU68K::ProcessLINK (int linkSize)
{
	Hook68KLINKList::iterator	iter = fHookLINK.begin ();
	while (iter != fHookLINK.end ())
	{
		(*iter) (linkSize);

		++iter;
	}
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ProcessRTE
// ---------------------------------------------------------------------------

int EmCPU68K::ProcessRTE (emuptr dest)
{
	int	handled = false;

	Hook68KRTEList::iterator	iter = fHookRTE.begin ();
	while (iter != fHookRTE.end ())
	{
		if ((*iter) (dest))
		{
			handled = true;
		}

		++iter;
	}

	return handled;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::ProcessRTS
// ---------------------------------------------------------------------------

int EmCPU68K::ProcessRTS (emuptr dest)
{
	int	handled = false;

	Hook68KRTSList::iterator	iter = fHookRTS.begin ();
	while (iter != fHookRTS.end ())
	{
		if ((*iter) (dest))
		{
			handled = true;
		}

		++iter;
	}

	return handled;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::CheckNewPC
// ---------------------------------------------------------------------------

void EmCPU68K::CheckNewPC (emuptr dest)
{
	Hook68KNewPCList::iterator	iter = fHookNewPC.begin ();
	while (iter != fHookNewPC.end ())
	{
		(*iter) (dest);

		++iter;
	}

	EmMemory::CheckNewPC (dest);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::CheckNewSP
// ---------------------------------------------------------------------------

void EmCPU68K::CheckNewSP (EmStackChangeType type)
{
	Hook68KNewSPList::iterator	iter = fHookNewSP.begin ();
	while (iter != fHookNewSP.end ())
	{
		(*iter) (type);

		++iter;
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmCPU68K::InstallHookException
//		¥ EmCPU68K::InstallHookJSR
//		¥ EmCPU68K::InstallHookJSR_Ind
//		¥ EmCPU68K::InstallHookLINK
//		¥ EmCPU68K::InstallHookRTE
//		¥ EmCPU68K::InstallHookRTS
//		¥ EmCPU68K::InstallHookNewPC
//		¥ EmCPU68K::InstallHookNewSP
// ---------------------------------------------------------------------------

void EmCPU68K::InstallHookException (ExceptionNumber exceptionNumber,
										Hook68KException fn)
{
	fExceptionHandlers[exceptionNumber].push_back (fn);
}


void EmCPU68K::InstallHookJSR (Hook68KJSR fn)
{
	fHookJSR.push_back (fn);
}


void EmCPU68K::InstallHookJSR_Ind (Hook68KJSR_Ind fn)
{
	fHookJSR_Ind.push_back (fn);
}


void EmCPU68K::InstallHookLINK (Hook68KLINK fn)
{
	fHookLINK.push_back (fn);
}


void EmCPU68K::InstallHookRTE (Hook68KRTE fn)
{
	fHookRTE.push_back (fn);
}


void EmCPU68K::InstallHookRTS (Hook68KRTS fn)
{
	fHookRTS.push_back (fn);
}


void EmCPU68K::InstallHookNewPC (Hook68KNewPC fn)
{
	fHookNewPC.push_back (fn);
}


void EmCPU68K::InstallHookNewSP (Hook68KNewSP fn)
{
	fHookNewSP.push_back (fn);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::RemoveHookException
//		¥ EmCPU68K::RemoveHookJSR
//		¥ EmCPU68K::RemoveHookJSR_Ind
//		¥ EmCPU68K::RemoveHookLINK
//		¥ EmCPU68K::RemoveHookRTE
//		¥ EmCPU68K::RemoveHookRTS
//		¥ EmCPU68K::RemoveHookNewPC
//		¥ EmCPU68K::RemoveHookNewSP
// ---------------------------------------------------------------------------

void EmCPU68K::RemoveHookException (ExceptionNumber exceptionNumber,
										Hook68KException fn)
{
	Hook68KExceptionList::iterator	iter = find (
							fExceptionHandlers[exceptionNumber].begin (),
							fExceptionHandlers[exceptionNumber].end (),
							fn);

	if (iter != fExceptionHandlers[exceptionNumber].end ())
	{
		fExceptionHandlers[exceptionNumber].erase (iter);
	}
}


void EmCPU68K::RemoveHookJSR (Hook68KJSR fn)
{
	Hook68KJSRList::iterator	iter = find (fHookJSR.begin (), fHookJSR.end (), fn);

	if (iter != fHookJSR.end ())
	{
		fHookJSR.erase (iter);
	}
}


void EmCPU68K::RemoveHookJSR_Ind (Hook68KJSR_Ind fn)
{
	Hook68KJSR_IndList::iterator	iter = find (fHookJSR_Ind.begin (), fHookJSR_Ind.end (), fn);

	if (iter != fHookJSR_Ind.end ())
	{
		fHookJSR_Ind.erase (iter);
	}
}


void EmCPU68K::RemoveHookLINK (Hook68KLINK fn)
{
	Hook68KLINKList::iterator	iter = find (fHookLINK.begin (), fHookLINK.end (), fn);

	if (iter != fHookLINK.end ())
	{
		fHookLINK.erase (iter);
	}
}


void EmCPU68K::RemoveHookRTE (Hook68KRTE fn)
{
	Hook68KRTEList::iterator	iter = find (fHookRTE.begin (), fHookRTE.end (), fn);

	if (iter != fHookRTE.end ())
	{
		fHookRTE.erase (iter);
	}
}


void EmCPU68K::RemoveHookRTS (Hook68KRTS fn)
{
	Hook68KRTSList::iterator	iter = find (fHookRTS.begin (), fHookRTS.end (), fn);

	if (iter != fHookRTS.end ())
	{
		fHookRTS.erase (iter);
	}
}


void EmCPU68K::RemoveHookNewPC (Hook68KNewPC fn)
{
	Hook68KNewPCList::iterator	iter = find (fHookNewPC.begin (), fHookNewPC.end (), fn);

	if (iter != fHookNewPC.end ())
	{
		fHookNewPC.erase (iter);
	}
}


void EmCPU68K::RemoveHookNewSP (Hook68KNewSP fn)
{
	Hook68KNewSPList::iterator	iter = find (fHookNewSP.begin (), fHookNewSP.end (), fn);

	if (iter != fHookNewSP.end ())
	{
		fHookNewSP.erase (iter);
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmCPU68K::GetRegisters
// ---------------------------------------------------------------------------

void EmCPU68K::GetRegisters (regstruct& registers)
{
	this->UpdateSRFromRegisters ();

	registers = regs;
	registers.pc = m68k_getpc ();
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::SetRegisters
// ---------------------------------------------------------------------------

void EmCPU68K::SetRegisters (regstruct& registers)
{
	regs = registers;
	this->UpdateRegistersFromSR ();

	m68k_setpc (registers.pc);

	this->CheckNewSP (kStackPointerChanged);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::UpdateSRFromRegisters
// ---------------------------------------------------------------------------
// Create a 16-bit status register value from the broken out fields.  In
// general, we keep the various fields separate for speed of access.  However,
// there are times when we need the packed 16-bit field.
//
// This function is called any time the 16-bit representation of the SR is
// needed:
//
//		on EORSR
//		on ORSR
//		on ANDSR
//		on MVSR2
//		on MV2SR
//		on RTR
//		when processing exceptions (EmCPU68K::ProcessException)
//
//		SystemPacket::GetRegs
//		EmCPU68K::GetRegisters

void EmCPU68K::UpdateSRFromRegisters (void)
{
	// (taken from MakeSR in newcpu.c)

	regs.sr = ((regs.t1 << 15) | (regs.t0 << 14)
		| (regs.s << 13) | (regs.m << 12) | (regs.intmask << 8)
		| (GET_XFLG << 4) | (GET_NFLG << 3) | (GET_ZFLG << 2) | (GET_VFLG << 1)
		| GET_CFLG);
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::UpdateRegistersFromSR
// ---------------------------------------------------------------------------
// Break out all of the fields from the 16-bit status register into their own
// separate variables.
//
// This function is called any time the SR has been update and needs to be
// re-expanded:
//
//		on EORSR
//		on ORSR
//		on ANDSR
//		on MV2SR
//		on STOP
//		on RTE
//		on RTR
//
//		SystemPacket::SetRegs
//		EmCPU68K::SetRegisters

void EmCPU68K::UpdateRegistersFromSR (void)
{
	// (taken from MakeFromSR in newcpu.c)
	
//	int oldm = regs.m;
	int olds = regs.s;
	
	regs.t1 = (regs.sr >> 15) & 1;
	regs.t0 = (regs.sr >> 14) & 1;
	regs.s = (regs.sr >> 13) & 1;
	regs.m = (regs.sr >> 12) & 1;
	regs.intmask = (regs.sr >> 8) & 7;

	SET_XFLG ((regs.sr >> 4) & 1);
	SET_NFLG ((regs.sr >> 3) & 1);
	SET_ZFLG ((regs.sr >> 2) & 1);
	SET_VFLG ((regs.sr >> 1) & 1);
	SET_CFLG (regs.sr & 1);

	if (olds != regs.s)
	{
		if (olds)
		{
			regs.isp = m68k_areg(regs, 7);
			m68k_areg(regs, 7) = regs.usp;
		}
		else
		{
			regs.usp = m68k_areg(regs, 7);
			m68k_areg(regs, 7) = regs.isp;
		}
	}
	
	regs.spcflags |= SPCFLAG_INT;

	if (regs.t1 || regs.t0)
	{
		regs.spcflags |= SPCFLAG_TRACE;
	}
	else
	{
		regs.spcflags &= ~(SPCFLAG_TRACE | SPCFLAG_DOTRACE);
	}
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::GetCycleCount
// ---------------------------------------------------------------------------

uint32 EmCPU68K::GetCycleCount (void)
{
	return fCycleCount;
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::BusError
// ---------------------------------------------------------------------------

void EmCPU68K::BusError (emuptr address, long size, Bool forRead)
{
	gExceptionAddress	= address;
	gExceptionSize		= size;
	gExceptionForRead	= forRead;

	this->ProcessException (kException_BusErr);

	EmAssert (false);	// Should never get this far.
}


// ---------------------------------------------------------------------------
//		¥ EmCPU68K::AddressError
// ---------------------------------------------------------------------------

void EmCPU68K::AddressError (emuptr address, long size, Bool forRead)
{
	gExceptionAddress	= address;
	gExceptionSize		= size;
	gExceptionForRead	= forRead;

	this->ProcessException (kException_AddressErr);

	EmAssert (false);	// Should never get this far.
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmCPU68K::InitializeUAETables
// ---------------------------------------------------------------------------

void EmCPU68K::InitializeUAETables (void)
{
	static int	initialized;

	// All of the stuff in this function needs to be done only once;
	// it doesn't need to be executed every time we create a new CPU.

	if (initialized)
		return;

	initialized = true;

	// Initialize some CPU-related tables
	// (This initialization code is taken from init_m68k in newcpu.c)

	int 	i, j;

	for (i = 0 ; i < 256 ; i++)
	{
		for (j = 0 ; j < 8 ; j++)
		{
			if (i & (1 << j))
			{
				break;
			}
		}

		movem_index1[i] = j;
		movem_index2[i] = 7-j;
		movem_next[i] = i & (~(1 << j));
	}

	read_table68k ();
	do_merges ();

	// The rest of this code is based on build_cpufunctbl in newcpu.c.

	unsigned long	opcode;
	struct cputbl*	tbl = op_smalltbl_3;

	for (opcode = 0; opcode < 65536; opcode++)
	{
		cpufunctbl[opcode] = op_illg;
	}

	for (i = 0; tbl[i].handler != NULL; i++)
	{
		if (!tbl[i].specific)
		{
			cpufunctbl[tbl[i].opcode] = tbl[i].handler;
#if HAS_PROFILING
			perftbl[tbl[i].opcode] = tbl[i].perf;
#endif
		}
	}

	for (opcode = 0; opcode < 65536; opcode++)
	{
		cpuop_func* f;

		if (table68k[opcode].mnemo == i_ILLG || table68k[opcode].clev > 0)
		{
			continue;
		}

		if (table68k[opcode].handler != -1)
		{
			f = cpufunctbl[table68k[opcode].handler];
			if (f == op_illg)
			{
				abort ();
			}

			cpufunctbl[opcode] = f;
#if HAS_PROFILING
			perftbl[opcode] = perftbl[table68k[opcode].handler];
#endif

		}
	}

	for (i = 0; tbl[i].handler != NULL; i++)
	{
		if (tbl[i].specific)
		{
			cpufunctbl[tbl[i].opcode] = tbl[i].handler;
#if HAS_PROFILING
			perftbl[tbl[i].opcode] = tbl[i].perf;
#endif
		}
	}

	// (hey readcpu doesn't free this guy!)

	Platform::DisposeMemory (table68k);
}
