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

// ---------------------------------------------------------------------------
#pragma mark ===== Includes
// ---------------------------------------------------------------------------

#include "EmCommon.h"
#include "DebugMgr.h"

#include "EmCPU68K.h"			// gCPU68K
#include "EmErrCodes.h"			// kError_NoError
#include "EmException.h"		// EmExceptionReset
#include "EmHAL.h"				// EmHAL
#include "EmLowMem.h"			// LowMem_SetGlobal, LowMem_GetGlobal
#include "EmMemory.h"			// CEnableFullAccess
#include "EmPalmFunction.h"		// SysTrapIndex, IsSystemTrap
#include "EmPatchState.h"		// EmPatchState::UIInitialized
#include "EmSession.h"			// EmSessionStopper, SuspendByDebugger
#include "ErrorHandling.h"		// ReportUnhandledException
#include "Logging.h"			// gErrLog
#include "MetaMemory.h"			// MetaMemory::MarkInstructionBreak
#include "Platform.h"			// Platform::SendPacket
#include "ROMStubs.h"			// FtrSet, FtrUnregister
#include "SessionFile.h"		// SessionFile
#include "SLP.h"				// SLP::EventCallback
#include "SocketMessaging.h"	// CSocket::Write
#include "Strings.r.h"			// kStr_ values
#include "SystemPacket.h"		// SystemPacket::
#include "UAE.h"				// m68k_areg, m68k_dreg, etc.

#include "ctype.h"				// isspace, isdigit, isxdigit


#define PRINTF	if (!LogHLDebugger ()) ; else LogAppendMsg


// ---------------------------------------------------------------------------
#pragma mark ===== Types
// ---------------------------------------------------------------------------

struct NamedConditionType
{
	const char*	name;
	compareFun	function;
};


// ---------------------------------------------------------------------------
#pragma mark ===== Functions
// ---------------------------------------------------------------------------

static uint32		PrvGetAddressRegister	(int num);
static uint32		PrvGetDataRegister		(int num);
static Bool			PrvBPEquals				(uint32 a, uint32 b);
static Bool			PrvBPNotEquals			(uint32 a, uint32 b);
static Bool			PrvBPGreater			(uint32 a, uint32 b);
static Bool			PrvBPGreaterOrEqual		(uint32 a, uint32 b);
static Bool			PrvBPLess				(uint32 a, uint32 b);
static Bool			PrvBPLessOrEqual		(uint32 a, uint32 b);
static const char*	PrvSkipWhite			(const char* p);

static Bool			PrvParseDecimal			(const char **ps, int *i);
static Bool			PrvParseUnsigned		(const char **ps, uint32 *u);


// ---------------------------------------------------------------------------
#pragma mark ===== Constants
// ---------------------------------------------------------------------------

#define kOpcode_ADD		0x0697	// ADD.L X, (A7)
#define kOpcode_LINK	0x4E50
#define kOpcode_RTE		0x4E73
#define kOpcode_RTD		0x4E74
#define kOpcode_RTS		0x4E75
#define kOpcode_JMP		0x4ED0


// ---------------------------------------------------------------------------
#pragma mark ===== Variables
// ---------------------------------------------------------------------------

// ----- Saved variables -----------------------------------------------------

DebugGlobalsType		gDebuggerGlobals;


// ----- UnSaved variables ---------------------------------------------------

static NamedConditionType kConditions[] =
{
	{"==",	&PrvBPEquals},
	{"!=",	&PrvBPNotEquals},
	{">",	&PrvBPGreater},
	{">=",	&PrvBPGreaterOrEqual},
	{"<",	&PrvBPLess},
	{"<=",	&PrvBPLessOrEqual},
	{NULL,	NULL}
};

emuptr	gExceptionAddress;
int		gExceptionSize;
Bool	gExceptionForRead;


// If we're listening for a debugger connection, the first three sockets
// contain references to those listening entities.	As soon as a connection
// is made, we set gConnectedDebugSocket and NULL out the other three,
// deleting any listening sockets that are no longer needed.

static CSocket*			gDebuggerSocket1;	 // TCP socket on user-defined port
static CSocket*			gDebuggerSocket2;	 // TCP socket on port 2000
static CSocket*			gDebuggerSocket3;	 // Platform-specific socket
static CTCPSocket*		gConnectedDebugSocket;


#pragma mark -

// ===========================================================================
//		¥ Debug
// ===========================================================================

/***********************************************************************
 *
 * FUNCTION:	Debug::Startup
 *
 * DESCRIPTION: Create all the sockets we'll need for the application
 *				and start them listening for clients.  Call this once
 *				at application startup.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::Startup (void)
{
	Debug::CreateListeningSockets ();
}


/***********************************************************************
 *
 * FUNCTION:	Debug::Shutdown
 *
 * DESCRIPTION: Delete the sockets we use.	CSocket::Shutdown would do
 *				this anyway, but it's a good idea to do it here, too,
 *				so as to drop the references in our global variables.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::Shutdown (void)
{
	Debug::DeleteListeningSockets ();

	if (gConnectedDebugSocket)
	{
		gConnectedDebugSocket->Close ();
		delete gConnectedDebugSocket;
		gConnectedDebugSocket = NULL;
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::ConnectedToTCPDebugger
 *
 * DESCRIPTION: Return whether or not we're connected to a debugger (or
 *				reasonably think so; we don't check for aborted
 *				or broken connections).  This function is called during
 *				Palm OS bootup to determine if the 'gdbS' feature needs
 *				to be installed.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if we were connected to a debugger the last time
 *				we checked.
 *
 ***********************************************************************/

Bool Debug::ConnectedToTCPDebugger (void)
{
	return Debug::GetTCPDebuggerSocket () != NULL;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::GetTCPDebuggerSocket
 *
 * DESCRIPTION: Return the socket that's connected to the debugger (if
 *				any).
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The pointer to the CSocket object connected to an
 *				external debugger.	Returns NULL if not connected to
 *				a debugger.
 *
 ***********************************************************************/

CTCPSocket* Debug::GetTCPDebuggerSocket (void)
{
	return gConnectedDebugSocket;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::GetDebuggerSocket
 *
 * DESCRIPTION: Return the socket that's connected to the debugger (if
 *				any).
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The pointer to the CSocket object connected to an
 *				external debugger.	Returns NULL if not connected to
 *				a debugger.
 *
 ***********************************************************************/

CSocket* Debug::GetDebuggerSocket (void)
{
	// Get the preferred (TCP) socket.

	CSocket* s = Debug::GetTCPDebuggerSocket ();

	// If we don't have one (we're not connected via such a socket),
	// try the backward-compatible, platform-specific socket.

	if (s == NULL)
		s = gDebuggerSocket3;

	// If we finally have a socket, return it.

	return s;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::Initialize
 *
 * DESCRIPTION: Standard initialization function.  Responsible for
 *				initializing this sub-system when a new session is
 *				created.  Will be followed by at least one call to
 *				Reset or Load.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

/*-----------------------------------------------------------------------------------*\

	Notes on side-stepping the ROM debugger:

		The debugger manipulates the following low-memory globals:
		
		Byte	dbgWasEntered;		// set true the first time debugger is entered
		Byte	dbgInDebugger;		// true if in debugger
		Byte	dbgTracing;			// tracing in debugger
		Ptr		dbgGlobalsP;		// pointer to dbgGlobals
		Ptr		dbgSerGlobalsP;		// pointer to Debugger Serial globals

		We set dbgWasEntered and dbgInDebugger in EnterDebugger.  We clear
		dbgInDebugger in ExitDebugger.	Both of these values are examined by
		other parts of the ROM.
		
		dbgTracing is set when the ROM debugger needs to single step.  It needs
		to single-step if there are breakpoints in the ROM, or if we need to
		execute over an installed breakpoint.  We have other ways to do these
		(we can actually install breakpoints into the ROM, and our CPU loop has
		a mechanism for fetching the opcode that is currently overwritten by
		a breakpoint), so we never have to set dbgTracing to true.

		dbgGlobalsP appears to be accessed only by the ROM debugger code.  Since
		we should intercept everything that would cause that code to be accessed,
		we shouldn't have to do anything with that block.

		dbgSerGlobalsP is initialized by DbgInit.  None of the ROM debugger code
		looks at it.  Only the serial code does.

	May want to patch the following traps:

		ConGetS
		ConPutS
		
		sysTrapDbgSrcMessage
		sysTrapDbgMessage
		sysTrapDbgGetMessage
		sysTrapDbgCommSettings

\*-----------------------------------------------------------------------------------*/

void Debug::Initialize (void)
{
	memset (&gDebuggerGlobals, 0, sizeof (gDebuggerGlobals));

	EmAssert (gSession);
	gSession->AddInstructionBreakHandlers (
				InstallInstructionBreaks,
				RemoveInstructionBreaks,
				HandleInstructionBreak);

	// Install functions that will cause the CPU loop to exit if we're
	// connected to an external debugger.

	EmAssert (gCPU68K);

	gCPU68K->InstallHookException (kException_BusErr,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_AddressErr,	Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_IllegalInstr,	Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_DivideByZero,	Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Chk,			Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap,			Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Privilege,	Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trace,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_ATrap,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_FTrap,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_AutoVec7,		Debug::BreakIfConnected);

	gCPU68K->InstallHookException (kException_SoftBreak,	Debug::BreakIfConnected);
		// Handspring implements this one.
//	gCPU68K->InstallHookException (kException_Trap1,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap2,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap3,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap4,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap5,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap6,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap7,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap9,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap10,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap11,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap13,		Debug::BreakIfConnected);
	gCPU68K->InstallHookException (kException_Trap14,		Debug::BreakIfConnected);

	// This one's a little different.  We want to exit the CPU loop
	// on a TRAP 8, but we want to do it conditionally.  If desired,
	// HandleTrap8 will cause the CPU loop to exit (leaving the PC just
	// after the TRAP instruction).	 Otherwise, it will treat the TRAP
	// like a NOP.

	gCPU68K->InstallHookException (kException_HardBreak,	Debug::HandleTrap8);

#if 0	// !!! TBD

	// Install Debugger Traps into trap table
	dispatchTableP[ SysTrapIndex (sysTrapDbgSrcMessage) ] = (Ptr)DbgMessage;
	dispatchTableP[ SysTrapIndex (sysTrapDbgMessage) ] = (Ptr)DbgMessage;
	dispatchTableP[ SysTrapIndex (sysTrapDbgGetMessage) ] = (Ptr)DbgGetMessage;
	dispatchTableP[ SysTrapIndex (sysTrapDbgCommSettings) ] = (Ptr)DbgCommSettings;

#endif
}


/***********************************************************************
 *
 * FUNCTION:	Debug::Reset
 *
 * DESCRIPTION: Standard reset function.  Sets the sub-system to a
 *				default state.	This occurs not only on a Reset (as
 *				from the menu item), but also when the sub-system
 *				is first initialized (Reset is called after Initialize)
 *				as well as when the system is re-loaded from an
 *				insufficient session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::Reset (void)
{
	gDebuggerGlobals.firstEntrance	= true;

	// Set flags that we're no longer in the debugger.  This is the same
	// as the last few steps of ExitDebugger.

	gDebuggerGlobals.excType = 0;
	EmLowMem_SetGlobal (dbgInDebugger, false);
}


/***********************************************************************
 *
 * FUNCTION:	Debug::Save
 *
 * DESCRIPTION: Standard save function.	 Saves any sub-system state to
 *				the given session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::Save (SessionFile& f)
{
	const long	kCurrentVersion = 1;

	Chunk			chunk;
	EmStreamChunk	s (chunk);

	s << kCurrentVersion;

	s << (bool) false;	// inDebugger
	s << gDebuggerGlobals.ignoreDbgBreaks;
	s << (bool) false;	// reEntered
	s << gDebuggerGlobals.firstEntrance;
	s << gDebuggerGlobals.stepSpy;
	s << gDebuggerGlobals.breakingOnATrap;
	s << gDebuggerGlobals.continueOverATrap;
	s << gDebuggerGlobals.continueOverBP;

	s << gDebuggerGlobals.checkTrapWordOnExit;
	s << gDebuggerGlobals.trapWord;
	s << gDebuggerGlobals.refNum;

	int ii;
	for (ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		s << (emuptr) gDebuggerGlobals.bp[ii].addr;
		s << gDebuggerGlobals.bp[ii].enabled;
		s << gDebuggerGlobals.bp[ii].installed;

		s << (uint16) 0;	// gDebuggerGlobals.bpOpcode[ii];

		if (gDebuggerGlobals.bpCondition[ii])
		{
			// Save only the source string; we can reconstruct
			// the rest of the fields from that.

			s << gDebuggerGlobals.bpCondition[ii]->source;
		}
		else
		{
			s << "";
		}
	}

	for (ii = 0; ii < dbgTotalTrapBreaks; ++ii)
	{
		s << gDebuggerGlobals.trapBreak[ii];
		s << gDebuggerGlobals.trapParam[ii];
	}

	s << gDebuggerGlobals.ssAddr;
	s << gDebuggerGlobals.ssValue;

	s << gDebuggerGlobals.excType;

	s << gDebuggerGlobals.watchEnabled;
	s << gDebuggerGlobals.watchAddr;
	s << gDebuggerGlobals.watchBytes;

	f.WriteDebugInfo (chunk);
}


/***********************************************************************
 *
 * FUNCTION:	Debug::Load
 *
 * DESCRIPTION: Standard load function.	 Loads any sub-system state
 *				from the given session file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::Load (SessionFile& f)
{
	// Delete any old state.

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		Debug::DeleteBreakpointCondition (ii);
	}

	// Load the new state.

	Chunk	chunk;
	if (f.ReadDebugInfo (chunk))
	{
		long			version;
		EmStreamChunk	s (chunk);

		bool			dummyBool;
		uint16			dummyShort;

		s >> version;

		if (version >= 1)
		{
			s >> dummyBool;		// inDebugger
			s >> gDebuggerGlobals.ignoreDbgBreaks;
			s >> dummyBool;		// reEntered
			s >> gDebuggerGlobals.firstEntrance;
			s >> gDebuggerGlobals.stepSpy;
			s >> gDebuggerGlobals.breakingOnATrap;
			s >> gDebuggerGlobals.continueOverATrap;
			s >> gDebuggerGlobals.continueOverBP;

			s >> gDebuggerGlobals.checkTrapWordOnExit;
			s >> gDebuggerGlobals.trapWord;
			s >> gDebuggerGlobals.refNum;

			int ii;
			for (ii = 0; ii < dbgTotalBreakpoints; ++ii)
			{
				emuptr temp;
				s >> temp; gDebuggerGlobals.bp[ii].addr = (MemPtr) temp;
				s >> gDebuggerGlobals.bp[ii].enabled;
				s >> gDebuggerGlobals.bp[ii].installed;
				
				s >> dummyShort;	// gDebuggerGlobals.bpOpcode[ii];

				string	source;
				s >> source;

				if (source.size () > 0)
				{
					BreakpointCondition*	bc = NewBreakpointCondition (source.c_str ());
					gDebuggerGlobals.bpCondition[ii] = bc;
				}
			}

			for (ii = 0; ii < dbgTotalTrapBreaks; ++ii)
			{
				s >> gDebuggerGlobals.trapBreak[ii];
				s >> gDebuggerGlobals.trapParam[ii];
			}

			s >> gDebuggerGlobals.ssAddr;
			s >> gDebuggerGlobals.ssValue;

			s >> gDebuggerGlobals.excType;

			s >> gDebuggerGlobals.watchEnabled;
			s >> gDebuggerGlobals.watchAddr;
			s >> gDebuggerGlobals.watchBytes;
		}
	}
	else
	{
		f.SetCanReload (false);
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::Dispose
 *
 * DESCRIPTION: Standard dispose function.	Completely release any
 *				resources acquired or allocated in Initialize and/or
 *				Load.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::Dispose (void)
{
	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		Debug::DeleteBreakpointCondition (ii);
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::HandleNewPacket
 *
 * DESCRIPTION: Completely handle a packet sent from an external
 *				debugger, setting any state and sending a reply if
 *				necessary.
 *
 * PARAMETERS:	slp - a reference to a SerialLink Protocol object that
 *					contains the packet information and the horse...uh,
 *					socket it rode in on.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode Debug::HandleNewPacket (SLP& slp)
{
	ErrCode result = kError_NoError;

	PRINTF ("Entering Debug::HandleNewPacket.");

	const EmProxySlkPktHeaderType&	header	= slp.Header ();
	const EmProxySysPktBodyType&	body	= slp.Body ();
//	const EmProxySlkPktFooterType&	footer	= slp.Footer ();

	EmAssert ((header.dest == slkSocketDebugger) || (header.dest == slkSocketConsole));

	// If the packet is for the debugger socket, then we pretty much need
	// to have the CPU stopped.  If not, when the PalmDebugger user executes
	// "g", the restored registers may not be what the machine expects.
	//
	// As a courtesy, we stop the CPU if we get a "state" packet.  That way,
	// the user can enter the debugger by typing "att" in the PalmDebugger and
	// doesn't have to fuss with shortcut-.1.
	//
	// Otherwise, we ignore the packet.

	if (header.dest == slkSocketDebugger)
	{
		EmSuspendState	state = gSession->GetSuspendState ();
		if (!state.fCounters.fSuspendByDebugger)
		{
			if (body.command == sysPktStateCmd)
			{
				// This will send the state packet as well as set our
				// "in the debugger" state.

				result = Debug::EnterDebugger (kException_SoftBreak, &slp);
				goto Exit;
			}

			PRINTF ("Packet is for the debugger, we're not in debug mode, and the packet is not a state command; leaving Debug::HandleNewPacket.");

			// This packet is for the debugger socket, we're not in the
			// debugger, and the packet is not a "state" command, so just
			// ignore this request.

			goto Exit;
		}
	}

	{
		CEnableFullAccess	munge;	// Remove blocks on memory access.

		// Goose the ROM, so it doesn't go to sleep while we're exchanging packets.
		// Do this by hand; we can't call ROM functions as subroutines while in
		// the debugger (we don't know what state the ROM is in).

		//	EvtResetAutoOffTimer ();
		EmLowMem_SetGlobal (sysAutoOffEvtTicks, EmLowMem_GetGlobal (hwrCurTicks));

		switch (body.command)
		{
			case sysPktStateCmd:
				result = SystemPacket::SendState (slp);
				break;

			case sysPktReadMemCmd:
				result = SystemPacket::ReadMem (slp);
				break;

			case sysPktWriteMemCmd:
				result = SystemPacket::WriteMem (slp);
				break;

			case sysPktSingleStepCmd:
				// I don't think there's anything to do here.  I think that
				// single-stepping is performed by the debugger setting the
				// trace bit in the SR and sending a sysPktContinueCmd.
				PRINTF ("   Not supported in this release.");
				break;

			case sysPktGetRtnNameCmd:
				result = SystemPacket::SendRoutineName (slp);
				break;

			case sysPktReadRegsCmd:
				result = SystemPacket::ReadRegs (slp);
				break;

			case sysPktWriteRegsCmd:
				result = SystemPacket::WriteRegs (slp);
				break;

			case sysPktContinueCmd:
				result = SystemPacket::Continue (slp);
				break;

			case sysPktRPCCmd:
				result = SystemPacket::RPC (slp);
				break;

			case sysPktGetBreakpointsCmd:
				result = SystemPacket::GetBreakpoints (slp);
				break;

			case sysPktSetBreakpointsCmd:
				result = SystemPacket::SetBreakpoints (slp);
				break;

//			case sysPktRemoteUIUpdCmd:
//				// Sent TO debugger; never received FROM debugger.
//				break;

//			case sysPktRemoteEvtCmd:
//				// Sent TO debugger; never received FROM debugger.
//				break;

			case sysPktDbgBreakToggleCmd:
				result = SystemPacket::ToggleBreak (slp);
				break;

			case sysPktFlashCmd:
				// Not supported in this release.
				PRINTF ("   Not supported in this release.");
				break;

			case sysPktCommCmd:
				// Not supported in this release.
				PRINTF ("   Not supported in this release.");
				break;

			case sysPktGetTrapBreaksCmd:
				result = SystemPacket::GetTrapBreaks (slp);
				break;

			case sysPktSetTrapBreaksCmd:
				result = SystemPacket::SetTrapBreaks (slp);
				break;

			case sysPktGremlinsCmd:
				// Not supported in this release.
				PRINTF ("   Not supported in this release.");
				break;

			case sysPktFindCmd:
				result = SystemPacket::Find (slp);
				break;

			case sysPktGetTrapConditionsCmd:
				result = SystemPacket::GetTrapConditions (slp);
				break;

			case sysPktSetTrapConditionsCmd:
				result = SystemPacket::SetTrapConditions (slp);
				break;

			case sysPktChecksumCmd:
				// Not supported in this release.
				PRINTF ("   Not supported in this release.");
				break;

			case sysPktExecFlashCmd:
				// Not supported in this release.
				PRINTF ("   Not supported in this release.");
				break;

			case sysPktRemoteMsgCmd:
				// Nothing for us to do.  With a serial link,
				// these are sent to clear the input port.
				PRINTF ("   Should not be *receiving these!");
				break;

#if 0
			case 0xFF:
			{
				// This is a special comand only for Poser

				char fn[256];
				strcpy (fn, (char*) &body.data[14]);

				EmFileRefList	fileList;
				fileList.push_back (fn);

				::LoadPalmFileList (fileList);
				break;
			}
#endif

			default:
				break;
		}
	}

 Exit:
	PRINTF ("Exiting Debug::HandleNewPacket.");

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::BreakIfConnected
 *
 * DESCRIPTION: Handle the given exception.  If we're connected to a
 *				debugger, we want to let it handle it, so change the CPU
 *				state and return TRUE (to say that we handled the
 *				execption).  Otherwise return false to perform default
 *				handling (which means handing it off to the emulated ROM).
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool Debug::BreakIfConnected (ExceptionNumber exceptionNumber)
{
	// Dump any logged information to a file.

	LogDump ();

	// If we're in the middle of calling a Palm OS subroutine, we're in
	// really bad shape.  Throw an exception that will eventually hard
	// reset the device.
	//
	// We can be nested for three reasons: the UI thread is making a
	// Palm OS call, the the UI thread is handling an RPC packet, or
	// the CPU thread is making a Palm OS call (probably as part of
	// a trap patch).  Let's examine these three cases.
	//
	// 1. The UI thread is making the Palm OS call.
	//
	//		TBD
	//
	// 2. The UI thread is handling an RPC packet.
	//
	//		In this case, the EmException object is thrown past the following:
	//
	//			Function						Action
	//			-------------------------		-----------------------------------
	//			Debug::BreakIfConnected			Throws the exception
	//			EmCPU68K::HandleException		-
	//			<<<Exception generator>>>		-
	//			EmCPU68K::Execute				-
	//			EmSession::CallCPU				-
	//			EmSession::ExecuteSubroutine	Unlock fSharedLock
	//			ATrap::DoCall					Add Trap Word and rethrow
	//			SystemPacket::RPC				Skips sending of response
	//			RPC::HandleNewPacket /			Clear gCurrentPacket
	//				Debug::HandleNewPacket		-
	//			SLP::HandleNewPacket			Reset the CPU before unstopping
	//			SLP::HandleDataReceived			-
	//			SLP::EventCallback				-
	//			-- one of the following --
	//			CMMFSocket::HandlePacket		Display error message
	//			CTCPSocket::Idle				Display error message
	//			CPPCSocket::HandlePacket		Record excpetion for display in UI thread.
	//
	// 3. The CPU thread is making the Palm OS call.
	//
	//		In this case, the EmException object is thrown past the following:
	//
	//			Function						Action
	//			-------------------------		-----------------------------------
	//			Debug::BreakIfConnected			Throws the exception
	//			EmCPU68K::HandleException		-
	//			<<<Exception generator>>>		-
	//			EmCPU68K::Execute				-
	//			EmSession::CallCPU				-
	//			EmSession::ExecuteSubroutine	Unlock fSharedLock
	//			ATrap::DoCall					Add Trap Word and rethrow
	//			ROMStubs function				-
	//			Trap Patch function				-
	//			Patches::HandlePatches			-
	//			Patches::HandleSystemCall		-
	//			EmPalmOS::HandleSystemCall		-
	//			-- either --
	//			EmPalmOS::HandleTrap15			-
	//			EmCPU68K::HandleException		-
	//			<<<Exception generator>>>		-
	//			-- or --
	//			EmPalmOS::HandleJSR_Ind			-
	//			cpuemu function					-
	//			-- end --
	//			EmCPU68K::Execute				-
	//			EmSession::CallCPU				-
	//			-- one of the following --
	//			EmSession::ExecuteIncremental	Display error message and Reset
	//			EmSession::Run					Display error message and Reset

	if (gSession->IsNested ())
	{
		EmExceptionReset	e (kResetSoft);
		e.SetException (exceptionNumber);
		throw e;
	}

	// If we entered due to a breakpoint (TRAP 0), backup the PC.

	emuptr	curpc = m68k_getpc ();
	if (exceptionNumber == kException_SoftBreak)
	{
		m68k_setpc (curpc - 2);
	}

	// Try entering the debugger.  If that fails, show a dialog,
	// giving the user Debug or Reset options.

	uint16	opcode	= EmMemGet16 (curpc & ~1);	// Protect against odd PC.

	switch (exceptionNumber)
	{
		case kException_BusErr:
			Errors::ReportErrBusError (gExceptionAddress, gExceptionSize, gExceptionForRead);
			break;

		case kException_AddressErr:
			Errors::ReportErrAddressError (gExceptionAddress, gExceptionSize, gExceptionForRead);
			break;

		case kException_IllegalInstr:
			Errors::ReportErrIllegalInstruction (opcode);
			break;

		case kException_DivideByZero:
			Errors::ReportErrDivideByZero ();
			break;

		case kException_Chk:
			Errors::ReportErrCHKInstruction ();
			break;

		case kException_Trap:
			Errors::ReportErrTRAPVInstruction ();
			break;

		case kException_Privilege:
			Errors::ReportErrPrivilegeViolation (opcode);
			break;

		case kException_Trace:
			Errors::ReportErrTrace ();
			break;

		case kException_ATrap:
			Errors::ReportErrATrap (opcode);
			break;

		case kException_FTrap:
			Errors::ReportErrFTrap (opcode);
			break;

		case kException_Trap0:
		case kException_Trap1:
		case kException_Trap2:
		case kException_Trap3:
		case kException_Trap4:
		case kException_Trap5:
		case kException_Trap6:
		case kException_Trap7:
		case kException_Trap8:
		case kException_Trap9:
		case kException_Trap10:
		case kException_Trap11:
		case kException_Trap12:
		case kException_Trap13:
		case kException_Trap14:
		case kException_Trap15:
			Errors::ReportErrTRAPx (exceptionNumber - kException_Trap0);
			break;

		default:
			EmAssert (false);
	}

	// If we got here, the user was allowed to click "Continue" for
	// some reason.  Say we handled the exception and muddle on.

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::HandleTrap8
 *
 * DESCRIPTION: The CPU just encountered a TRAP 8, which is what gets
 *				compiled into the developer's code when he "calls" the
 *				DbgBreak or DbgSrcBreak "function".  HandleTrap8
 *				determines whether or not this TRAP 8 should be handled
 *				as an exception (in which case we'd enter the debugger)
 *				or should be skipped like a NOP. If "ignoreDbgBreaks" is
 *				true, then merely return TRUE to say that we completely
 *				handled the exception.	Otherwise, change the CPU state
 *				to non-running before we return TRUE.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool Debug::HandleTrap8 (ExceptionNumber exceptionNumber)
{
	// Don't break on trapbreaks if we're calling the ROM internally.

	if (gSession->IsNested ())
		return true;

	if (gDebuggerGlobals.ignoreDbgBreaks)
	{
		return true;
	}

	return Debug::BreakIfConnected (exceptionNumber);
}


/***********************************************************************
 *
 * FUNCTION:	Debug::HandleSystemCall
 *
 * DESCRIPTION: An A-Trap is about to be executed.	This function is
 *				called to determine if we want to break on this A-Trap.
 *				If there are A-Trap breaks registered with the debugger,
 *				and if the "continueOverATrap" flag is not set (if it were set,
 *				that would mean that we'd just exited the debugger, and
 *				don't need to re-break on the A-Trap that caused us to
 *				enter the debugger), then scan the break table to see if
 *				the A-Trap we're about to execute is one we want to
 *				break on.
 *
 *				If it is, call Debug::EnterDebugger,
 *				which will cause the CPU to break at the end of this
 *				opcode.	 Also return TRUE, which tells the normal A-Trap
 *				handler that we completely handled this A-Trap, and that
 *				it doesn't need to do any trap dispatcher stuff.  Finally,
 *				back-up the PC so that it points to the A-Trap again so
 *				that when we resume execution we'll start at that A-Trap.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool Debug::HandleSystemCall (const SystemCallContext& context)
{
	// Don't break on trapbreaks if we're calling the ROM internally.

	if (gSession->IsNested ())
		return false;

	Bool	doBreak = false;

	if (!gDebuggerGlobals.continueOverATrap && gDebuggerGlobals.breakingOnATrap)
	{
		doBreak = Debug::MustBreakOnTrapSystemCall (context.fTrapWord, context.fExtra);

		// If we're supposed to break here, try entering the debugger.

		if (doBreak)
		{
			// Entering the debugger means trying to send a "state" packet
			// to the external debugger.  Make sure the PC we pass it is correct.

			emuptr	oldPC = m68k_getpc ();
			m68k_setpc (context.fPC);

			if (Debug::EnterDebugger (kException_SoftBreak, NULL) == kError_NoError)
			{
				// Check again on the way out to see if there's still
				// a breakpoint at this location.  If so, we need to
				// set *another* flag to make sure we skip over the
				// breakpoint.	Otherwise, we'd just break here over
				// and over again.

				gDebuggerGlobals.checkTrapWordOnExit = true;
				gDebuggerGlobals.trapWord = context.fTrapWord;
				gDebuggerGlobals.refNum = context.fExtra;
			}
			else
			{
				m68k_setpc (oldPC);
			}
		}
	}

	// Clear the flag telling us to inhibit the ATrapBreak check.

	gDebuggerGlobals.continueOverATrap = false;

	return doBreak;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::EnterDebugger
 *
 * DESCRIPTION: Put the emulator into "debug mode".  This pretty much
 *				consists of setting up a bunch of flags, figuring out
 *				why we entered the debugger, and telling the external
 *				debugger that we've entered debug mode and what the
 *				current CPU state is.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

//
//	Reasons for entering the debugger:
//
//		Single step:
//			reason == kException_Trace
//
//			Normal processing.
//
//		Step spy:
//			reason == kException_Trace
//
//			Normal processing.
//
//		User breakpoint (temporary or otherwise):
//			reason == kException_Trap0 + sysDbgBreakpointTrapNum
//
//			Normal processing.
//
//		Compiled breakpoint (DbgBreak, DbgSrcBreak):
//			reason == kException_Trap0 + sysDbgTrapNum
//
//			Ignore if DbgGlobalsType.ignoreDbgBreaks is true.
//			
//			Actually, now we catch this one a little earlier.  In
//			Software_ProcessException, we call an installed exception
//			handler that leads to calling Debug::HandleTrap8.  That
//			function will return TRUE if ignoreDbgBreaks is true, which
//			will effectively turn the TRAP 8 into a NOP.  Otherwise, it
//			returns false, which will lead to EnterDebugger being called
//			to handle the exception.
//
//		"A-Trap" break:
//			reason == kException_Trace
//
//			When looking for an A-Trap to break on, the debugger uses
//			a special trap dispatcher.	Called on every TRAP F, when it
//			finds a trap to break on, it sets DbgGlobalsType.breakingOnATrap,
//			sets the tracing SR bit, and then calls the original trap
//			dispatcher.
//
//			Because the trap dispatcher is run in supervisor mode, the
//			tracing bit doesn't do anything until it RTE's.
//
//			Normal processing.
//
//		Exception (bus error, address error, etc):
//			reason == exception code
//
//			Normal processing.
//
//	The debugger is silently entered if reason == kException_Trace.
//	Otherwise, it displays a message saying what exception occurred.

ErrCode Debug::EnterDebugger (ExceptionNumber reason, SLP* slp)
{
	PRINTF ("Entering Debug::EnterDebugger.");

	CEnableFullAccess	munge;	// Remove blocks on memory access.

	// Save the reason for our entering the debugger.

	gDebuggerGlobals.excType = (UInt16) (reason * 4);

	// Turn off sound in case it was on when the debugger was entered.

	EmHAL::TurnSoundOff ();

	// Send a state message packet notifying the host that we've entered.
	// If we can't send the packet (perhaps because there is no external
	// debugger listening) return false saying that we failed to enter
	// the debugger.  When we return false, we indicate that perhaps the
	// emulated ROM debugger should take over.
	//
	// Concurrency note: in the multi-threaded version of the emulator,
	// EnterDebugger will be called from one thread (the CPU thread),
	// while the UI thread will be listening for packets from the
	// external debugger.  When SendState sends the packet from this
	// thread, the external debugger could receive it and send us back
	// a message immediately.  The question is, is this OK?	 Are there
	// problems with the UI thread trying to debug us before the CPU
	// thread has had a chance to stop itself? Let's see: the UI thread
	// will receive the message, and start to handle it by telling the
	// CPU thread to stop.	That means that we should have a chance to
	// clean up (set our flags, remove breakpoints, etc.) and stop before
	// the UI thread tries to debug us.

	ErrCode result = kError_NoError;

	if (!slp)
	{
		CSocket*	debuggerSocket = Debug::GetDebuggerSocket ();
		if (debuggerSocket)
		{
			SLP	newSLP (debuggerSocket);
			result = SystemPacket::SendState (newSLP);
		}
		else
		{
			result = 1;	// !!! Need a not "connected to debugger" error!
		}
	}
	else
	{
		result = SystemPacket::SendState (*slp);
	}

	if (result == kError_NoError)
	{
		// Flag to the ROM that we're in the debugger. "dbgInDebugger" gets
		// cleared in ExitDebugger. "dbgWasDebugger" stays set to true.

		EmLowMem_SetGlobal (dbgWasEntered, true);
		EmLowMem_SetGlobal (dbgInDebugger, true);

		EmAssert (gSession);
		gSession->ScheduleSuspendException ();

		PRINTF ("Entered debug mode.");
	}
	else
	{
		PRINTF ("Failed to enter debug mode.");
	}

	PRINTF ("Exiting Debug::EnterDebugger.");

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::ExitDebugger
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode Debug::ExitDebugger (void)
{
	CEnableFullAccess	munge;	// Remove blocks on memory access.

	// If we're continuing, but we're on a breakpoint, set a boolean
	// that causes us to ignore the breakpoint when we hit it.

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		if (gDebuggerGlobals.bp[ii].enabled == false)
			continue;

		emuptr addr = (emuptr) gDebuggerGlobals.bp[ii].addr;
		if (!addr)
			continue;

		if (addr == m68k_getpc ())
			gDebuggerGlobals.continueOverBP = true;
	}

	// Check to see if we (a) stopped here because of a request
	// to break on a particular system call and (b) that we're
	// still requested to break on this call.  If so, set a flag
	// to skip over the next break-on-system-call.	Otherwise,
	// we'd just break at this location over and over again.

	if (gDebuggerGlobals.checkTrapWordOnExit)
	{
		gDebuggerGlobals.checkTrapWordOnExit = false;

		if (MustBreakOnTrapSystemCall (gDebuggerGlobals.trapWord,
									   gDebuggerGlobals.refNum))
		{
			// Set the flag that tells this function to not break
			// the next time it's entered.

			gDebuggerGlobals.continueOverATrap = true;
		}
	}

	// Set flags that we're no longer in the debugger.

	gDebuggerGlobals.excType = 0;
	EmLowMem_SetGlobal (dbgInDebugger, false);

	EmSuspendState	state = gSession->GetSuspendState ();
	state.fCounters.fSuspendByDebugger = 0;
	gSession->SetSuspendState (state);

	return kError_NoError;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::HandleInstructionBreak
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::HandleInstructionBreak (void)
{
	// Don't break on soft breakpoints if we're calling the ROM internally.

	if (gSession->IsNested ())
		return;

	// Don't break on soft breakpoints if we're exiting the debugger.

	if (gDebuggerGlobals.continueOverBP)
	{
		gDebuggerGlobals.continueOverBP = false;
		return;
	}

	Debug::ConditionalBreak ();
}


/***********************************************************************
 *
 * FUNCTION:	Debug::InstallInstructionBreaks
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::InstallInstructionBreaks (void)
{
	// Install the breakpoints.
	
	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		if (gDebuggerGlobals.bp[ii].enabled)
		{
			MetaMemory::MarkInstructionBreak ((emuptr) gDebuggerGlobals.bp[ii].addr);
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::RemoveInstructionBreaks
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::RemoveInstructionBreaks (void)
{
	// Remove the breakpoints.
	
	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		if (gDebuggerGlobals.bp[ii].enabled)
		{
			MetaMemory::UnmarkInstructionBreak ((emuptr) gDebuggerGlobals.bp[ii].addr);
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::NewBreakpointCondition
 *
 * DESCRIPTION: Create a new breakpoint condition by parsing the given
 *				source string.	Returns NULL on parse error.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

BreakpointCondition* Debug::NewBreakpointCondition (const char* sourceString)
{
	const char* source = sourceString;
	registerFun regType;
	int		regNum;
	Bool		indirect = false;
	uint32		indirectOffset;
	int		size = 4;

	compareFun	condition;
	int		value;

	source = PrvSkipWhite (source);
	if (!source)
		return NULL;

	if (isdigit (*source))
	{
		indirect = true;
		if (!PrvParseUnsigned (&source, &indirectOffset))
			return NULL;

		source = PrvSkipWhite (source);

		if (*source != '(')
			return NULL;

		++source;
		source = PrvSkipWhite (source);
	}

	switch (tolower (*source++))
	{
	case 'd':
		regType = PrvGetDataRegister;
		break;

	case 'a':
		regType = PrvGetAddressRegister;
		break;

	default:
		return NULL;
	}

	source = PrvSkipWhite (source);

	if (!PrvParseDecimal (&source, &regNum))
		return NULL;

	source = PrvSkipWhite (source);

	if (indirect)
	{
		if (*source != ')')
			return NULL;

		++source;
		source = PrvSkipWhite (source);
	}

	if (*source == '.')
	{
		++source;

		switch (*source)
		{
		case 'b': size = 1; break;
		case 'w': size = 2; break;
		case 'l': size = 4; break;
		default: return NULL;
		}

		++source;
		source = PrvSkipWhite (source);
	}

	condition = NULL;
	for (NamedConditionType* c = kConditions; c->name; ++c)
	{
		if (!strncmp (source, c->name, strlen (c->name)))
		{
			condition = c->function;
			source += strlen (c->name);
			break;
		}
	}

	if (!condition)
		return NULL;

	if (sscanf (source, "%i", &value) != 1)
		return NULL;

	BreakpointCondition* bc = new BreakpointCondition;

	bc->regType		= regType;
	bc->regNum			= regNum;
	bc->indirect		= indirect;
	bc->indirectOffset	= indirectOffset;
	bc->size			= size;

	bc->condition		= condition;
	bc->value			= value;
	bc->source			= _strdup (sourceString);

	return bc;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::SetBreakpoint
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::SetBreakpoint (int index, emuptr addr, BreakpointCondition* c)
{
	Debug::ClearBreakpoint (index);

	EmAssert (gSession);
	gSession->RemoveInstructionBreaks ();

	gDebuggerGlobals.bp[index].enabled		= true;
	gDebuggerGlobals.bp[index].installed	= false;
	gDebuggerGlobals.bp[index].addr			= (MemPtr) addr;
	gDebuggerGlobals.bpCondition[index]		= c;

	gSession->InstallInstructionBreaks ();
}


/***********************************************************************
 *
 * FUNCTION:	Debug::ClearBreakpoint
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::ClearBreakpoint (int index)
{
	EmAssert (gSession);
	gSession->RemoveInstructionBreaks ();

	gDebuggerGlobals.bp[index].enabled = false;
	gDebuggerGlobals.bp[index].addr = NULL;

	Debug::DeleteBreakpointCondition (index);

	gSession->InstallInstructionBreaks ();
}

/***********************************************************************
 *
 * FUNCTION:	Debug::DeleteBreakpointCondition
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::DeleteBreakpointCondition (int index)
{
	BreakpointCondition*&	cond = gDebuggerGlobals.bpCondition[index];

	if (cond)
	{
		if (cond->source)
		{
			free (cond->source);
		}

		delete cond;
		cond = NULL;
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::BreakpointInstalled
 *
 * DESCRIPTION: Return whether or not any breakpoints are installed.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	TRUE if so.
 *
 ***********************************************************************/

Bool Debug::BreakpointInstalled (void)
{
	// Install the breakpoints.
	
	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		if (gDebuggerGlobals.bp[ii].enabled)
		{
			return true;
		}
	}

	return false;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	Debug::MustBreakOnTrapSystemCall
 *
 * DESCRIPTION: Test the given trapWord (and optional refNum) to see
 *				if this is a combination that we expect to break on.
 *
 * PARAMETERS:	trapWord - the system function dispatch number (Axxx)
 *					to test.
 *
 *				refNum - the library reference number to test.	This
 *					value is used only if trapWord is in the range for
 *					library function calls.
 *
 * RETURNED:	True if we should break on this combination.
 *
 ***********************************************************************/

Bool Debug::MustBreakOnTrapSystemCall (uint16 trapWord, uint16 refNum)
{
	Bool	doBreak		= false;
	uint16	trapIndex	= ::SysTrapIndex (trapWord);

	// Do different compares for system traps and library traps.

	if (::IsSystemTrap (trapWord))
	{
		for (int ii = 0; ii < dbgTotalTrapBreaks; ++ii)
		{
			if (trapIndex == ::SysTrapIndex (gDebuggerGlobals.trapBreak[ii]))
			{
				doBreak = true;
				break;
			}
		}
	}
	else
	{
		for (int ii = 0; ii < dbgTotalTrapBreaks; ++ii)
		{
			if (trapIndex == ::SysTrapIndex (gDebuggerGlobals.trapBreak[ii]) &&
				refNum == gDebuggerGlobals.trapParam[ii])
			{
				doBreak = true;
				break;
			}
		}
	}

	return doBreak;
}


/***********************************************************************
 *
 * FUNCTION:	Debug::DoCheckStepSpy
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

void Debug::DoCheckStepSpy (emuptr writeAddress, int writeBytes)
{
	CEnableFullAccess	munge;

	uint32	newValue = EmMemGet32 (gDebuggerGlobals.ssAddr);
	if (newValue != gDebuggerGlobals.ssValue)
	{
		gSession->ScheduleDeferredError (new EmDeferredErrStepSpy (writeAddress, writeBytes,
			gDebuggerGlobals.ssAddr, gDebuggerGlobals.ssValue, newValue));
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::DoCheckWatchpoint
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

void Debug::DoCheckWatchpoint (emuptr writeAddress, int writeBytes)
{
	if (writeAddress < gDebuggerGlobals.watchAddr + gDebuggerGlobals.watchBytes &&
		writeAddress + writeBytes > gDebuggerGlobals.watchAddr)
	{
		gSession->ScheduleDeferredError (new EmDeferredErrWatchpoint (writeAddress, writeBytes,
			gDebuggerGlobals.watchAddr, gDebuggerGlobals.watchBytes));
	}
}


////// breakpoint conditions

uint32 PrvGetAddressRegister (int num)
{
	return m68k_areg (regs, num);
}

uint32 PrvGetDataRegister (int num)
{
	return m68k_dreg (regs, num);
}

Bool PrvBPEquals (uint32 a, uint32 b)
{
	return a == b;
}

Bool PrvBPNotEquals (uint32 a, uint32 b)
{
	return a != b;
}

// Comparisons are unsigned for now.  Would it be more useful if they were signed?

Bool PrvBPGreater (uint32 a, uint32 b)
{
	return a > b;
}

Bool PrvBPGreaterOrEqual (uint32 a, uint32 b)
{
	return a >= b;
}

Bool PrvBPLess (uint32 a, uint32 b)
{
	return a < b;
}

Bool PrvBPLessOrEqual (uint32 a, uint32 b)
{
	return a <= b;
}

const char* PrvSkipWhite (const char* p)
{
	while (p && isspace (*p))
		++p;

	return p;
}

/* Parse a signed decimal integer */

Bool PrvParseDecimal (const char **ps, int *i)
{
	const char *s = *ps;

	if (sscanf (s, "%d", i) != 1)
		return false;

	while (isdigit (*s))
		++s;

	*ps = s;

	return true;
}

/* Parse an unsigned integer which may be either decimal or hex (e.g. "0xabc")
 */
Bool PrvParseUnsigned (const char **ps, uint32 *u)
{
	const char *s = *ps;

	if (sscanf (s, "%li", u) != 1)
		return false;

	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))	/* hex */
	{
		s += 2;
		while (isxdigit (*s))
			++s;
	}
	else /* decimal */
	{
		while (isdigit (*s))
			++s;
	}

	*ps = s;

	return true;
}

Bool BreakpointCondition::Evaluate (void)
{
	uint32 r = regType (regNum);

	if (indirect)
	{
		if (size == 4)
			r = EmMemGet32 (r + indirectOffset);
		else if (size == 2)
			r = EmMemGet16 (r + indirectOffset);
		else if (size == 1)
			r = EmMemGet8 (r + indirectOffset);
		else
			return false;
	}
	else
	{
		if (size == 2)
			r = (uint16) r;
		else if (size == 1)
			r = (uint8) r;
	}

	return condition (r, value);
}


/***********************************************************************
 *
 * FUNCTION:	Debug::ConditionalBreak
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::ConditionalBreak (void)
{
	MemPtr	bpAddress = (MemPtr) m68k_getpc ();

	// Loop over the breakpoints to see if we've hit one.

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		// This breakpoint is not enabled, so just skip over it.

		if (!gDebuggerGlobals.bp[ii].enabled)
			continue;

		// This breakpoint is not for this PC, so just skip over it.

		if (gDebuggerGlobals.bp[ii].addr != bpAddress)
			continue;

		// If there is a condition for this breakpoint but it evaluates
		// to false, just skip over the breakpoint.

		BreakpointCondition*	bc = gDebuggerGlobals.bpCondition[ii];

		if (bc && bc->Evaluate () == false)
			continue;

		// Clear temporary breakpoint if we hit it.

		if (bpAddress == gDebuggerGlobals.bp[dbgTempBPIndex].addr)
		{
			gDebuggerGlobals.bp[dbgTempBPIndex].enabled = false;
		}

		// Break into the debugger.  If we can't do that (for instance,
		// if no debugger is connected), just leave.

		Debug::EnterDebugger (kException_SoftBreak, NULL);
		break;
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::CreateListeningSockets
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

static void PrvFireUpSocket (CSocket*& s)
{
	if (s)
	{
		if (s->Open () != errNone)
		{
			s->Delete();
			s = NULL;
		}
	}
}

void Debug::CreateListeningSockets (void)
{
	Preference<long>	portPref (kPrefKeyDebuggerSocketPort);

	EmAssert (gDebuggerSocket1 == NULL);
	EmAssert (gDebuggerSocket2 == NULL);
	EmAssert (gDebuggerSocket3 == NULL);

	if (*portPref != 0)
	{
		gDebuggerSocket1 = new CTCPSocket (&Debug::EventCallback, *portPref);
		gDebuggerSocket2 = new CTCPSocket (&Debug::EventCallback, 2000);
	}

	gDebuggerSocket3 = Platform::CreateDebuggerSocket ();

	::PrvFireUpSocket (gDebuggerSocket1);
	::PrvFireUpSocket (gDebuggerSocket2);
	::PrvFireUpSocket (gDebuggerSocket3);
}


/***********************************************************************
 *
 * FUNCTION:	Debug::DeleteListeningSockets
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Debug::DeleteListeningSockets (void)
{
	if (gDebuggerSocket1)
	{
		gDebuggerSocket1->Close ();
		gDebuggerSocket1->Delete();
		gDebuggerSocket1 = NULL;
	}

	if (gDebuggerSocket2)
	{
		gDebuggerSocket2->Close ();
		gDebuggerSocket2->Delete();
		gDebuggerSocket2 = NULL;
	}

	if (gDebuggerSocket3)
	{
		gDebuggerSocket3->Close ();
		gDebuggerSocket3->Delete();
		gDebuggerSocket3 = NULL;
	}
}


/***********************************************************************
 *
 * FUNCTION:	Debug::EventCallback
 *
 * DESCRIPTION: Callback function for TCP-based debugger-related
 *				sockets.  This function takes care of installing and
 *				removing the 'gdbS' Feature (for gdb support), and
 *				forwards debugger packets to the Debug sub-system.
 *
 * PARAMETERS:	s - the socket that connected, disconnected, or received
 *					some data.
 *
 *				event - a code indicating what happened.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void Debug::EventCallback (CSocket* s, int event)
{
	switch (event)
	{
		case CSocket::kConnected:
		{
			EmAssert (gDebuggerSocket1 == s ||
					gDebuggerSocket2 == s ||
					gDebuggerSocket3 == s);
			EmAssert (gConnectedDebugSocket == NULL);

			// We've connected on one of the TCP sockets we were listening
			// on.	Delete the other listening sockets so that we don't
			// connect with it, too.  We actually delete the other
			// Sockets so that we don't accidentally start listening with
			// them again (our TCPSockets are pretty tenacious when it comes
			// to auto-starting the listening process).

			if (s == gDebuggerSocket1)
			{
				gConnectedDebugSocket = (CTCPSocket*) gDebuggerSocket1;
				gDebuggerSocket1 = NULL;
			}
			else if (s == gDebuggerSocket2)
			{
				gConnectedDebugSocket = (CTCPSocket*) gDebuggerSocket2;
				gDebuggerSocket2 = NULL;
			}
			else	// s == gDebuggerSocket3
			{
				gDebuggerSocket3 = NULL;
			}

			Debug::DeleteListeningSockets ();

			// If we're listening on a socket, install the 'gdbS' feature.	The
			// existance of this feature causes programs written the the prc tools
			// to enter the debugger  when they're launched.

			if (EmPatchState::UIInitialized ())
			{
				EmSessionStopper	stopper (gSession, kStopOnSysCall);
				if (stopper.Stopped ())
				{
					::FtrSet ('gdbS', 0, 0x12BEEF34);
				}
			}
			break;
		}

		case CSocket::kDataReceived:
		{
			break;
		}

		case CSocket::kDisconnected:
		{
			// Let's start listening for a new debugger connection.

			EmAssert (gDebuggerSocket1 == NULL);
			EmAssert (gDebuggerSocket2 == NULL);
			EmAssert (gDebuggerSocket3 == NULL);
			EmAssert (gConnectedDebugSocket == s);

			gConnectedDebugSocket = NULL;
			s->Delete();

			Debug::CreateListeningSockets ();

			if (EmPatchState::UIInitialized ())
			{
				EmSessionStopper	stopper (gSession, kStopOnSysCall);
				if (stopper.Stopped ())
				{
					::FtrUnregister ('gdbS', 0);
				}
			}

			break;
		}
	}

	SLP::EventCallback (s, event);
}


