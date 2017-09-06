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

#include "EmCommon.h"
#include "SystemPacket.h"

#include "ATraps.h"				// ATrap
#include "DebugMgr.h"			// Debug::
#include "EmBankMapped.h"		// EmBankMapped::GetEmulatedAddress
#include "EmCPU68K.h"			// gCPU68K->UpdateRegistersFromSR
#include "EmErrCodes.h"			// kError_NoError
#include "EmLowMem.h"			// EmLowMem_GetGlobal
#include "EmMemory.h"			// EmMem_memcpy
#include "EmPalmFunction.h"		// FindFunctionName
#include "EmPalmStructs.h"		// EmSysPktRPCType, etc
#include "EmRPC.h"				// slkSocketRPC
#include "EmSession.h"			// EmSession::Reset
#include "HostControl.h"		// hostSelectorWaitForIdle
#include "Logging.h"			// LogAppendMsg
#include "Platform.h"			// Platform::ExitDebugger
#include "SLP.h"				// SLP

#include <ctype.h>				// isalpha


/*
	This module implements the handling of all "serial link" packets.

	Serial Link packets can be sent to Poser via a number of mechanisms:

		* serial connections
		* TCP connections
		* PPC Toolbox connections (Macintosh)
		* Memory-mapped file connection (Windows)

	Regardless of how they're received, they get processed by SLP::HandleNewPacket.
	This function looks at the destination of the packet specified in the packet
	header.  Currently, there are three supported targets in Poser:

		* Debugger socket (also supported by Palm OS debugger nub)
		* Console socket (also supported by Palm OS debugger nub)
		* RPC socket (Poser specific)

	Before handing the packet off to a socket-specific packet handler,
	SLP::HandleNewPacket (which is running in the UI thread) synchronizes
	with the CPU thread.  For the debugger socket, it halts the thread
	at whatever spot it's in.  For the other sockets, it halts the thread
	the next time a system call is made.

	Poser's Debugger sub-system handles packets targeted to the debugger
	and console sockets.  Poser's RPC sub-system handles packets targeted
	to the RPC socket.

	The three sockets handle the following packets:

		Debugger
			sysPktStateCmd
			sysPktReadMemCmd
			sysPktWriteMemCmd
			sysPktGetRtnNameCmd
			sysPktReadRegsCmd
			sysPktWriteRegsCmd
			sysPktContinueCmd
			sysPktRPCCmd
			sysPktSetBreakpointsCmd
			sysPktDbgBreakToggleCmd
			sysPktGetTrapBreaksCmd
			sysPktSetTrapBreaksCmd
			sysPktFindCmd
			sysPktGetTrapConditionsCmd
			sysPktSetTrapConditionsCmd

		Console
			sysPktRPCCmd

		RPC
			sysPktReadMemCmd
			sysPktWriteMemCmd
			sysPktRPCCmd
			sysPktRPC2Cmd

	The Console and RPC sockets will always handle the packet they receive
	(assuming that the UI thread has first synchronized with the CPU thread
	on a system call first).  If the packet is sent to the Debugger socket,
	then the CPU must first be in "debug mode".  This debug mode is automatically
	entered if an error or exception has occurred and Poser has sent a "state"
	packet to the external debugger.  After that point, the external debugger
	can send any packet it wants, optionally exiting debug mode by sending
	a "Continue" packet, or perhaps sending an RPC packet to invoke the
	SysReset Palm OS function.

	An external debugger can also force the entry into "debug mode" by sending
	a "get state" request packet to the Debugger socket.  If the CPU is not in
	debug mode, Poser will enter debug mode as a courtesey.  Note that this is
	a Poser-specific extension to the way the Debugger socket works.  The Palm
	OS ROM doesn't work this way.

	All other packets sent to the Debugger socket are silently ignored if the
	CPU is not in "debug mode".

	------------------------------------------------------------------------------

	Crib notes on the supported packets:

	+- Called by Debugger
	|	+- Implemented
	|	|								Command
	|	|	Command name				Number		Sent to
	------- --------------------------- -----------	-----------------
	*	*	sysPktStateCmd				0x00		slkSocketDebugger	SysPktStateCmdType				SysPktStateRspType
	*	*	sysPktReadMemCmd			0x01		slkSocketDebugger	SysPktReadMemCmdType			SysPktReadMemRspType
	*	*	sysPktWriteMemCmd			0x02		slkSocketDebugger	SysPktWriteMemCmdType			SysPktWriteMemRspType
	*	.	sysPktSingleStepCmd			0x03
	*	*	sysPktGetRtnNameCmd			0x04		slkSocketDebugger	SysPktRtnNameCmdType			SysPktRtnNameRspType
	*	*	sysPktReadRegsCmd			0x05		slkSocketDebugger	SysPktReadRegsCmdType			SysPktReadRegsRspType
	*	*	sysPktWriteRegsCmd			0x06		slkSocketDebugger	SysPktWriteRegsCmdType			SysPktWriteRegsRspType
	*	*	sysPktContinueCmd			0x07		slkSocketDebugger	SysPktContinueCmdType			<no response>
	*	*	sysPktRPCCmd				0x0A		both*				SysPktRPCType
		*	sysPktGetBreakpointsCmd		0x0B							SysPktGetBreakpointsCmdType		SysPktGetBreakpointsRspType
	*	*	sysPktSetBreakpointsCmd		0x0C		slkSocketDebugger	SysPktSetBreakpointsCmdType		SysPktSetBreakpointsRspType
		.	sysPktRemoteUIUpdCmd		0x0C							SysPktRemoteUIUpdCmdType
		.	sysPktRemoteEvtCmd			0x0D							SysPktRemoteEvtCmdType
	*	*	sysPktDbgBreakToggleCmd		0x0D		slkSocketDebugger	SysPktDbgBreakToggleCmdType		SysPktDbgBreakToggleRspType
	*	.	sysPktFlashCmd				0x0E							SysPktFlashWriteType
	*	.	sysPktCommCmd				0x0F		both*				SysPktCommCmdType				SysPktCommRspType
	*	*	sysPktGetTrapBreaksCmd		0x10		slkSocketDebugger	SysPktGetTrapBreaksCmdType		SysPktGetTrapBreaksRspType
	*	*	sysPktSetTrapBreaksCmd		0x11		slkSocketDebugger	SysPktSetTrapBreaksCmdType		SysPktSetTrapBreaksRspType
		+	sysPktGremlinsCmd			0x12		slkSocketConsole	SysPktGremlinsCmdType
	*	*	sysPktFindCmd				0x13		slkSocketDebugger	SysPktFindCmdType				SysPktFindRspType
		*	sysPktGetTrapConditionsCmd	0x14		slkSocketDebugger	SysPktGetTrapConditionsCmdType	SysPktGetTrapConditionsRspType
		*	sysPktSetTrapConditionsCmd	0x15		slkSocketDebugger	SysPktSetTrapConditionsCmdType	SysPktSetTrapConditionsRspType
		.	sysPktChecksumCmd			0x16		slkSocketDebugger	SysPktChecksumType
		.	sysPktExecFlashCmd			0x17		slkSocketDebugger	sysPktExecFlashType
	*	*	sysPktRemoteMsgCmd			0x7f		both				SysPktRemoteMsgCmdType

	"both*" = if attached, sent to slkSocketDebugger, else slkSocketConsole
*/

#define sysPktBadFormatRsp		0xFF


#define ENTER_CODE(fnName, cmdType, rspType)		\
	PRINTF ("Entering SystemPacket::" fnName ".");	\
	EmAlias##cmdType<LAS>	packet (slp.Body ().GetPtr ())


#define ENTER_PACKET(fnName, cmdType, rspType)		\
	PRINTF ("Entering SystemPacket::" fnName ".");	\
	EmAlias##cmdType<LAS>	packet (slp.Body ().GetPtr ());	\
	EmProxy##rspType		response


#define EXIT_PACKET(fnName, code, bodySize)			\
	response.command = code;						\
	response._filler = 0;							\
	ErrCode result = SystemPacket::SendPacket (slp, response.GetPtr(), bodySize);	\
	PRINTF ("Exiting SystemPacket::" fnName ".");	\
	return result


#define EXIT_CODE(fnName, code)						\
	ErrCode result = SystemPacket::SendResponse (slp, code);	\
	PRINTF ("Exiting SystemPacket::" fnName ".");	\
	return result


#define PRINTF	if (!LogHLDebugger ()) ; else LogAppendMsg


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::SendState
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::SendState (SLP& slp)
{
	// Do this function entry by hand instead of using the ENTER_PACKET macro.
	// That macro will call SLP::Body, which returns a reference to the
	// packet body that was sent to us.  However, SendState (a) doesn't
	// need the body reference, so there's no need to set it up, and (b)
	// isn't necessarily called in response to a "SendState" command,
	// and so the reference may be NULL.

	PRINTF ("Entering SystemPacket::SendState.");
	EmProxySysPktStateRspType		response;

	// Get the resetted boolean.  This flag is true if the "device" has
	// been resetted since the last time we sent a packet.	The debugger
	// needs to know this in order to update its notion of the device's
	// state (like, for instance, if breakpoints are installed).

	response.resetted				= gDebuggerGlobals.firstEntrance;
	gDebuggerGlobals.firstEntrance	= false;

	// Get the exception id.

	response.exceptionId			= gDebuggerGlobals.excType;

	// Get the registers.

	M68KRegsType	reg;
	SystemPacket::GetRegs (reg);

	response.reg.d[0]	= reg.d[0];
	response.reg.d[1]	= reg.d[1];
	response.reg.d[2]	= reg.d[2];
	response.reg.d[3]	= reg.d[3];
	response.reg.d[4]	= reg.d[4];
	response.reg.d[5]	= reg.d[5];
	response.reg.d[6]	= reg.d[6];
	response.reg.d[7]	= reg.d[7];
	response.reg.a[0]	= reg.a[0];
	response.reg.a[1]	= reg.a[1];
	response.reg.a[2]	= reg.a[2];
	response.reg.a[3]	= reg.a[3];
	response.reg.a[4]	= reg.a[4];
	response.reg.a[5]	= reg.a[5];
	response.reg.a[6]	= reg.a[6];
	response.reg.usp	= reg.usp;
	response.reg.ssp	= reg.ssp;
	response.reg.pc		= reg.pc;
	response.reg.sr		= reg.sr;

	// Get the breakpoints.

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		response.bp[ii].addr		= gDebuggerGlobals.bp[ii].addr;
		response.bp[ii].enabled		= gDebuggerGlobals.bp[ii].enabled;
		response.bp[ii].installed	= gDebuggerGlobals.bp[ii].installed;
	}

	// Get some code beginning at PC.

	emuptr pc = m68k_getpc ();
	for (int jj = 0; jj < sysPktStateRspInstWords; ++jj)
	{
		// Do this as two fetches in case the PC is odd.
		response.inst[jj]	= EmMemGet8 (pc + 2 * jj + 0) * 256 +
							  EmMemGet8 (pc + 2 * jj + 1);
	}


	// Get the routine name and address range info.

	void*	startAddr;
	void*	endAddr;
	memset (response.name.GetPtr (), 0, sysPktMaxNameLen);
	::FindFunctionName (	pc & 0xFFFFFFFE, // Protect against odd addresses.
							(char*) response.name.GetPtr (),
							(emuptr*) &startAddr,
							(emuptr*) &endAddr);

	response.startAddr	= startAddr;
	response.endAddr	= endAddr;

	// Send the rev of the trap table.

	response.trapTableRev	= EmLowMem_GetGlobal (sysDispatchTableRev);

	EXIT_PACKET ("SendState", sysPktStateRsp, response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::ReadMem
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::ReadMem (SLP& slp)
{
	ENTER_PACKET ("ReadMem", SysPktReadMemCmdType, SysPktReadMemRspType);

	void*	dest = response.data.GetPtr ();
	emuptr	src = (emuptr) packet.address;
	UInt16	len = packet.numBytes;
	
	memset (dest, 0xFF, len);	// Clear buffer in case of failure.

	if (len > 0 && EmMemCheckAddress (src, 1) && EmMemCheckAddress (src + len - 1, 1))
	{
		EmMem_memcpy (dest, src, len);
	}

	EXIT_PACKET ("ReadMem", sysPktReadMemRsp,
		EmProxySysPktEmptyRspType::GetSize () + packet.numBytes);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::WriteMem
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::WriteMem (SLP& slp)
{
	ENTER_CODE ("WriteMem", SysPktWriteMemCmdType, SysPktWriteMemRspType);

	emuptr		dest = (emuptr) packet.address;
	const void*	src = packet.data.GetPtr ();
	UInt16		len = packet.numBytes;

	if (len > 0 && EmMemCheckAddress (dest, 1) && EmMemCheckAddress (dest + len - 1, 1))
	{
		EmMem_memcpy (dest, src, len);
	}

	// If we just altered low memory, recalculate the low-memory checksum.
	// Make sure we're on a ROM that has this field!  Determine this by
	// seeing that the address of sysLowMemChecksum is below the memCardInfo
	// fields that come after the FixedGlobals.
	//
	// !!! This chunk of code should be in some more generally accessible location.

	if (offsetof (LowMemType, fixed.globals.sysLowMemChecksum) < EmLowMem_GetGlobal (memCardInfoP))
	{
		if ((emuptr) packet.address < (emuptr) 0x100)
		{
			UInt32		checksum	= 0;
			emuptr		csP		= EmMemNULL;

			// First, calculate the checksum

			while (csP < (emuptr) 0x100)
			{
				UInt32	data = EmMemGet32 (csP);

				// Don't do these trap vectors since they change whenever the
				// debugger is set to break on any a-trap or breakpoint.

				if (csP == offsetof (M68KExcTableType, trapN[sysDispatchTrapNum]))
					data = 0;

				if (csP == offsetof (M68KExcTableType, trapN[sysDbgBreakpointTrapNum]))
					data = 0;

				if (csP == offsetof (M68KExcTableType, trace))
					data = 0;

				checksum += data;
				csP += 4;
			}

			// Save new checksum

			EmLowMem_SetGlobal (sysLowMemChecksum, checksum);
		}
	}

	EXIT_CODE ("WriteMem", sysPktWriteMemRsp);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::SendRoutineName
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::SendRoutineName (SLP& slp)
{
	ENTER_PACKET ("SendRoutineName", SysPktRtnNameCmdType, SysPktRtnNameRspType);

	emuptr	pc = ((emuptr) packet.address) & ~1;	// Protect against odd address.
	void*	startAddr;
	void*	endAddr;

	::FindFunctionName( pc, (char*) response.name.GetPtr (),
						(emuptr*) &startAddr,
						(emuptr*) &endAddr);

	response.address	= packet.address;
	response.startAddr	= startAddr;
	response.endAddr	= endAddr;

	EXIT_PACKET ("SendRoutineName", sysPktGetRtnNameRsp, response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::ReadRegs
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::ReadRegs (SLP& slp)
{
	ENTER_PACKET ("ReadRegs", SysPktReadRegsCmdType, SysPktReadRegsRspType);
	UNUSED_PARAM (packet)

	// Get the registers.

	M68KRegsType	reg;
	SystemPacket::GetRegs (reg);

	response.reg.d[0]	= reg.d[0];
	response.reg.d[1]	= reg.d[1];
	response.reg.d[2]	= reg.d[2];
	response.reg.d[3]	= reg.d[3];
	response.reg.d[4]	= reg.d[4];
	response.reg.d[5]	= reg.d[5];
	response.reg.d[6]	= reg.d[6];
	response.reg.d[7]	= reg.d[7];
	response.reg.a[0]	= reg.a[0];
	response.reg.a[1]	= reg.a[1];
	response.reg.a[2]	= reg.a[2];
	response.reg.a[3]	= reg.a[3];
	response.reg.a[4]	= reg.a[4];
	response.reg.a[5]	= reg.a[5];
	response.reg.a[6]	= reg.a[6];
	response.reg.usp	= reg.usp;
	response.reg.ssp	= reg.ssp;
	response.reg.pc		= reg.pc;
	response.reg.sr		= reg.sr;

	EXIT_PACKET ("ReadRegs", sysPktReadRegsRsp, response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::WriteRegs
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::WriteRegs (SLP& slp)
{
	ENTER_CODE ("WriteRegs", SysPktWriteRegsCmdType, SysPktWriteRegsRspType);

	M68KRegsType	reg;

	reg.d[0]	= packet.reg.d[0];
	reg.d[1]	= packet.reg.d[1];
	reg.d[2]	= packet.reg.d[2];
	reg.d[3]	= packet.reg.d[3];
	reg.d[4]	= packet.reg.d[4];
	reg.d[5]	= packet.reg.d[5];
	reg.d[6]	= packet.reg.d[6];
	reg.d[7]	= packet.reg.d[7];
	reg.a[0]	= packet.reg.a[0];
	reg.a[1]	= packet.reg.a[1];
	reg.a[2]	= packet.reg.a[2];
	reg.a[3]	= packet.reg.a[3];
	reg.a[4]	= packet.reg.a[4];
	reg.a[5]	= packet.reg.a[5];
	reg.a[6]	= packet.reg.a[6];
	reg.usp		= packet.reg.usp;
	reg.ssp		= packet.reg.ssp;
	reg.pc		= packet.reg.pc;
	reg.sr		= packet.reg.sr;

	SystemPacket::SetRegs (reg);

	EXIT_CODE ("ReadRegs", sysPktWriteRegsRsp);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::Continue
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::Continue (SLP& slp)
{
	ENTER_CODE ("Continue", SysPktContinueCmdType, SysPktContinueCmdType);

	M68KRegsType	reg;

	reg.d[0]	= packet.regs.d[0];
	reg.d[1]	= packet.regs.d[1];
	reg.d[2]	= packet.regs.d[2];
	reg.d[3]	= packet.regs.d[3];
	reg.d[4]	= packet.regs.d[4];
	reg.d[5]	= packet.regs.d[5];
	reg.d[6]	= packet.regs.d[6];
	reg.d[7]	= packet.regs.d[7];
	reg.a[0]	= packet.regs.a[0];
	reg.a[1]	= packet.regs.a[1];
	reg.a[2]	= packet.regs.a[2];
	reg.a[3]	= packet.regs.a[3];
	reg.a[4]	= packet.regs.a[4];
	reg.a[5]	= packet.regs.a[5];
	reg.a[6]	= packet.regs.a[6];
	reg.usp		= packet.regs.usp;
	reg.ssp		= packet.regs.ssp;
	reg.pc		= packet.regs.pc;
	reg.sr		= packet.regs.sr;

	SystemPacket::SetRegs (reg);


	gDebuggerGlobals.stepSpy	= packet.stepSpy != 0;
	gDebuggerGlobals.ssAddr		= packet.ssAddr;
	// ssCount is ignored?
	gDebuggerGlobals.ssValue	= packet.ssCheckSum;

	ErrCode result = Debug::ExitDebugger ();

	// Perform any platform-specific actions.

	if (result == errNone)
	{
		Platform::ExitDebugger ();
	}

	// No reply...

	PRINTF ("Exiting SystemPacket::Continue.");

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::RPC
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::RPC (SLP& slp)
{
	ENTER_PACKET ("RPC", SysPktRPCType, SysPktRPCType);

	// Handle calls to SysReset specially, since that function never
	// returns.  Do this *before* creating the ATrap object, as that
	// object's d'tor will attempt to restore the PC to what it was
	// when the c'tor was run.

	if (packet.trapWord == sysTrapSysReset)
	{
#if 0
		emuptr	sysResetAddr = EmLowMem::GetTrapAddress (sysTrapSysReset);
		EmAssert (sysResetAddr);

		m68k_setpc (sysResetAddr);

		// Exit trace mode.  On an actual device, debugger packets (including
		// the final call to SysReset) are handled at interrupt time when
		// trace mode is turned off.  However, on Poser, trace mode may be
		// left on by any previous single-step commands since we handle
		// debugger packets "between cycles" and not at interrupt time.

		regs.t1 = regs.t0 = regs.m = 0;
		regs.spcflags &= ~(SPCFLAG_TRACE | SPCFLAG_DOTRACE);
#else
		// Call EmSession::Reset instead of doing the above in order
		// to more cleanly exit not only trace mode, but debug mode
		// and any other modes I didn't think of.
		EmAssert (gSession);
		gSession->Reset (kResetSoft);
#endif

		return kError_NoError;
	}


	ATrap	trap;

	// Map in the memory pointed to by the reference parameters
	StMemoryMapper	mapper (packet.GetPtr (), sysPktMaxBodySize);


	void*	paramStart	= packet.param.GetPtr ();
	void*	paramPtr	= paramStart;

	for (UInt16 ii = 0; ii < packet.numParams; ++ii)
	{
		EmAliasSysPktRPCParamType<LAS>	param (paramPtr);

		if (param.byRef)
		{
			trap.PushLong (EmBankMapped::GetEmulatedAddress (param.asByte.GetPtr ()));
		}
		else
		{
			if (param.size == 1)
			{
				trap.PushByte (param.asByte);
			}
			else if (param.size == 2)
			{
				trap.PushWord (param.asShort);
			}
			else if (param.size == 4)
			{
				trap.PushLong (param.asLong);
			}
		}

		paramPtr = ((char*) param.asByte.GetPtr ()) + ((param.size + 1) & ~1);
	}


	// Call the trap.

	trap.Call (packet.trapWord);

	memcpy (response.GetPtr (), packet.GetPtr (), sysPktMaxBodySize);

	response.resultD0	= trap.GetD0 ();
	response.resultA0	= trap.GetA0 ();

	long numBytes = ((char*) paramPtr) - ((char*) packet.GetPtr ());	// or get from header...

	EXIT_PACKET ("RPC", sysPktRPCRsp, numBytes);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::RPC2
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::RPC2 (SLP& slp)
{
	ENTER_PACKET ("RPC2", SysPktRPC2Type, SysPktRPC2Type);

	// Handle calls to SysReset specially, since that function never
	// returns.  Do this *before* creating the ATrap object, as that
	// object's d'tor will attempt to restore the PC to what it was
	// when the c'tor was run.

	if (packet.trapWord == sysTrapSysReset)
	{
#if 0
		emuptr	sysResetAddr = EmLowMem::GetTrapAddress (sysTrapSysReset);
		EmAssert (sysResetAddr);

		m68k_setpc (sysResetAddr);

		// Exit trace mode.  On an actual device, debugger packets (including
		// the final call to SysReset) are handled at interrupt time when
		// trace mode is turned off.  However, on Poser, trace mode may be
		// left on by any previous single-step commands since we handle
		// debugger packets "between cycles" and not at interrupt time.

		regs.t1 = regs.t0 = regs.m = 0;
		regs.spcflags &= ~(SPCFLAG_TRACE | SPCFLAG_DOTRACE);
#else
		// Call EmSession::Reset instead of doing the above in order
		// to more cleanly exit not only trace mode, but debug mode
		// and any other modes I didn't think of.
		EmAssert (gSession);
		gSession->Reset (kResetSoft);
#endif

		return kError_NoError;
	}


	ATrap	trap;

	// Map in the memory pointed to by the reference parameters
	StMemoryMapper	mapper (packet.GetPtr (), sysPktMaxBodySize);


	// Extract any D-registers from the packet.

	UInt32*	regsP	= (UInt32*) packet.Regs.GetPtr ();
	int		mask	= 0x01;
	int		regNum	= 0;

	while (mask < 0x100)
	{
		if ((mask & packet.DRegMask) != 0)
		{
			EmAliasUInt32<LAS> reg (regsP++);
			trap.SetNewDReg (regNum, reg);
		}

		++regNum;
		mask <<= 1;
	}

	// Extract any A-registers from the packet.

	mask	= 0x01;
	regNum	= 0;

	while (mask < 0x100)
	{
		if ((mask & packet.ARegMask) != 0)
		{
			EmAliasUInt32<LAS> reg (regsP++);
			trap.SetNewAReg (regNum, reg);
		}

		++regNum;
		mask <<= 1;
	}

	// Extract any stack-based parameters from the packet.

	EmAliasUInt16<LAS> numParamsP (regsP);
	UInt16	numParams = numParamsP;

	void*	paramStart	= ((char*) regsP) + sizeof (UInt16);
	void*	paramPtr	= paramStart;

	for (UInt16 ii = 0; ii < numParams; ++ii)
	{
		EmAliasSysPktRPCParamType<LAS>	param (paramPtr);

		if (param.byRef)
		{
			trap.PushLong (EmBankMapped::GetEmulatedAddress (param.asByte.GetPtr ()));
		}
		else
		{
			if (param.size == 1)
			{
				trap.PushByte (param.asByte);
			}
			else if (param.size == 2)
			{
				trap.PushWord (param.asShort);
			}
			else if (param.size == 4)
			{
				trap.PushLong (param.asLong);
			}
		}

		paramPtr = ((char*) param.asByte.GetPtr ()) + ((param.size + 1) & ~1);
	}


	// Call the trap.

	trap.Call (packet.trapWord);

	memcpy (response.GetPtr (), packet.GetPtr (), sysPktMaxBodySize);

	response.resultD0			= trap.GetD0 ();
	response.resultA0			= trap.GetA0 ();
	response.resultException	= 0;

	long numBytes = ((char*) paramPtr) - ((char*) packet.GetPtr ());	// or get from header...

	EXIT_PACKET ("RPC2", sysPktRPC2Rsp, numBytes);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::GetBreakpoints
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::GetBreakpoints (SLP& slp)
{
	ENTER_PACKET ("GetBreakpoints", SysPktGetBreakpointsCmdType, SysPktGetBreakpointsRspType);
	UNUSED_PARAM (packet)

	// Get the breakpoints.

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		response.bp[ii].addr		= gDebuggerGlobals.bp[ii].addr;
		response.bp[ii].enabled		= gDebuggerGlobals.bp[ii].enabled;
		response.bp[ii].installed	= gDebuggerGlobals.bp[ii].installed;
	}

	EXIT_PACKET ("GetBreakpoints", sysPktGetBreakpointsRsp, response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::SetBreakpoints
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::SetBreakpoints (SLP& slp)
{
	ENTER_CODE ("SetBreakpoints", SysPktSetBreakpointsCmdType, SysPktSetBreakpointsRspType);

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		if (packet.bp[ii].enabled)
		{
			Debug::SetBreakpoint (ii, (emuptr) packet.bp[ii].addr, NULL);
		}
		else
		{
			Debug::ClearBreakpoint (ii);
		}
	}

	EXIT_CODE ("GetBreakpoints", sysPktSetBreakpointsRsp);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::ToggleBreak
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::ToggleBreak (SLP& slp)
{
	ENTER_PACKET ("ToggleBreak", SysPktDbgBreakToggleCmdType, SysPktDbgBreakToggleRspType);
	UNUSED_PARAM (packet)

	gDebuggerGlobals.ignoreDbgBreaks = !gDebuggerGlobals.ignoreDbgBreaks;

	response.newState	= gDebuggerGlobals.ignoreDbgBreaks;

	EXIT_PACKET ("ToggleBreak", sysPktDbgBreakToggleRsp, response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::GetTrapBreaks
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::GetTrapBreaks (SLP& slp)
{
	ENTER_PACKET ("GetTrapBreaks", SysPktGetTrapBreaksCmdType, SysPktGetTrapBreaksRspType);
	UNUSED_PARAM (packet)

	for (int ii = 0; ii < dbgTotalTrapBreaks; ++ii)
	{
		response.trapBP[ii] = gDebuggerGlobals.trapBreak[ii];
	}

	EXIT_PACKET ("GetTrapBreaks", sysPktGetTrapBreaksRsp, response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::SetTrapBreaks
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::SetTrapBreaks (SLP& slp)
{
	ENTER_CODE ("SetTrapBreaks", SysPktSetTrapBreaksCmdType, SysPktSetTrapBreaksRspType);

	for (int ii = 0; ii < dbgTotalTrapBreaks; ++ii)
	{
		gDebuggerGlobals.trapBreak[ii] = packet.trapBP[ii];
	}

	gDebuggerGlobals.breakingOnATrap = false;
	for (int jj = 0; jj < dbgTotalTrapBreaks; ++jj)
	{
		if (gDebuggerGlobals.trapBreak[jj])
		{
			gDebuggerGlobals.breakingOnATrap = true;
			break;
		}
	}

	EXIT_CODE ("SetTrapBreaks", sysPktSetTrapBreaksRsp);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::Find
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::Find (SLP& slp)
{
	ENTER_PACKET ("Find", SysPktFindCmdType, SysPktFindRspType);

	response.addr		= (UInt32) NULL;
	response.found		= false;
	
	{
		UInt8*	dataP = (UInt8*) packet.data.GetPtr ();
		UInt32	startP = packet.firstAddr;

		while (startP <= packet.lastAddr)
		{
			UInt16	ii;

			for (ii = 0; ii < packet.numBytes; ++ii)
			{
				UInt8	b1 = EmMemGet8 (startP + ii);
				UInt8	b2 = dataP[ii];

				// Handle caseless conversion

				if (packet.caseInsensitive)
				{
					if (isalpha (b1))
						b1 = tolower (b1);

					if (isalpha (b2))
						b2 = tolower (b2);
				}

				if (b1 != b2)			// if different -->
					break;
			}

			// Check if matched

			if (ii >= packet.numBytes)
			{
				response.addr	= startP;
				response.found	= true;
				break;
			}

			startP++;					// advance start of search pointer
		}
	}

	EXIT_PACKET ("Find", sysPktFindRsp, response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::GetTrapConditions
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::GetTrapConditions (SLP& slp)
{
	ENTER_PACKET ("GetTrapConditions", SysPktGetTrapConditionsCmdType, SysPktGetTrapConditionsRspType);
	UNUSED_PARAM (packet);

	for (int ii = 0; ii < dbgTotalTrapBreaks; ++ii)
	{
		response.trapParam[ii] = gDebuggerGlobals.trapParam[ii];
	}

	EXIT_PACKET ("GetTrapConditions", sysPktGetTrapConditionsRsp, response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::SetTrapConditions
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::SetTrapConditions (SLP& slp)
{
	ENTER_CODE ("SetTrapConditions", SysPktSetTrapConditionsCmdType, SysPktSetTrapConditionsRspType);

	for (int ii = 0; ii < dbgTotalTrapBreaks; ++ii)
	{
		gDebuggerGlobals.trapParam[ii] = packet.trapParam[ii];
	}

	EXIT_CODE ("SetTrapConditions", sysPktSetTrapConditionsRsp);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::SendMessage
 *
 * DESCRIPTION: Sends a text string to the external client.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::SendMessage (SLP& slp, const char* msg)
{
//	ENTER_PACKET ("SendMessage", SysPktBodyType, SysPktBodyType);

	// Do this function entry by hand instead of using the ENTER_PACKET macro.
	// That macro will call SLP::Body, which returns a reference to the
	// packet body that was sent to us.  However, SendMessage (a) doesn't
	// need the body reference, so there's no need to set it up, and (b)
	// isn't necessarily called in response to a "SendMessage" command,
	// and so the reference may be NULL.

	PRINTF ("Entering SystemPacket::SendMessage.");
	EmProxySysPktBodyType		response;

	size_t	len = strlen (msg) + 1;
	strcpy ((char*) response.data.GetPtr (), msg);

	EXIT_PACKET ("SendMessage", sysPktRemoteMsgCmd,
					EmProxySysPktEmptyRspType::GetSize () + len);
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::SendResponse
 *
 * DESCRIPTION: Sends a simple null-body message to the external debugger.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::SendResponse (SLP& slp, UInt8 code)
{
	EmProxySysPktEmptyRspType	response;

	response.command	= code;
	response._filler	= 0;

	return SystemPacket::SendPacket (slp, response.GetPtr (), response.GetSize ());
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::SendPacket
 *
 * DESCRIPTION: Sends the given packet to the external client.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SystemPacket::SendPacket (SLP& slp, const void* body, long bodySize)
{
	PRINTF ("Entering SystemPacket::SendPacket.");

	ErrCode result = slp.SendPacket (body, bodySize);

	PRINTF ("Exiting SystemPacket::SendPacket.");

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SystemPacket::GetRegs
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void SystemPacket::GetRegs (M68KRegsType& debuggerRegs)
{
	debuggerRegs.d[0]	= m68k_dreg (regs, 0);
	debuggerRegs.d[1]	= m68k_dreg (regs, 1);
	debuggerRegs.d[2]	= m68k_dreg (regs, 2);
	debuggerRegs.d[3]	= m68k_dreg (regs, 3);
	debuggerRegs.d[4]	= m68k_dreg (regs, 4);
	debuggerRegs.d[5]	= m68k_dreg (regs, 5);
	debuggerRegs.d[6]	= m68k_dreg (regs, 6);
	debuggerRegs.d[7]	= m68k_dreg (regs, 7);

	debuggerRegs.a[0]	= m68k_areg (regs, 0);
	debuggerRegs.a[1]	= m68k_areg (regs, 1);
	debuggerRegs.a[2]	= m68k_areg (regs, 2);
	debuggerRegs.a[3]	= m68k_areg (regs, 3);
	debuggerRegs.a[4]	= m68k_areg (regs, 4);
	debuggerRegs.a[5]	= m68k_areg (regs, 5);
	debuggerRegs.a[6]	= m68k_areg (regs, 6);
//	debuggerRegs.a[7]	= m68k_areg (regs, 7);

	// Update the usp, isp, and/or msp before returning it.
	// (from m68k_dumpstate() in newcpu.c)
	
	if (regs.s == 0)
		regs.usp = m68k_areg (regs, 7);
	else
		regs.isp = m68k_areg (regs, 7);

	// Make sure the Status Register is up-to-date before moving it
	// into the debugger register set.

	EmAssert (gCPU68K);
	gCPU68K->UpdateSRFromRegisters ();

	// Copy the remaining registers from the UAE register set to the
	// debugger's register set.

	debuggerRegs.usp	= regs.usp;
	debuggerRegs.ssp	= regs.isp;
	debuggerRegs.sr		= regs.sr;
	debuggerRegs.pc		= m68k_getpc ();
}


/***********************************************************************
 *
 * FUNCTION:	Debug::SetRegs
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void SystemPacket::SetRegs (const M68KRegsType& debuggerRegs)
{
	m68k_dreg (regs, 0) = debuggerRegs.d[0];
	m68k_dreg (regs, 1) = debuggerRegs.d[1];
	m68k_dreg (regs, 2) = debuggerRegs.d[2];
	m68k_dreg (regs, 3) = debuggerRegs.d[3];
	m68k_dreg (regs, 4) = debuggerRegs.d[4];
	m68k_dreg (regs, 5) = debuggerRegs.d[5];
	m68k_dreg (regs, 6) = debuggerRegs.d[6];
	m68k_dreg (regs, 7) = debuggerRegs.d[7];

	m68k_areg (regs, 0) = debuggerRegs.a[0];
	m68k_areg (regs, 1) = debuggerRegs.a[1];
	m68k_areg (regs, 2) = debuggerRegs.a[2];
	m68k_areg (regs, 3) = debuggerRegs.a[3];
	m68k_areg (regs, 4) = debuggerRegs.a[4];
	m68k_areg (regs, 5) = debuggerRegs.a[5];
	m68k_areg (regs, 6) = debuggerRegs.a[6];
//	m68k_areg (regs, 7) = debuggerRegs.a[7];

	regs.usp	= debuggerRegs.usp;
	regs.isp	= debuggerRegs.ssp;
	regs.sr		= debuggerRegs.sr;
	m68k_setpc (debuggerRegs.pc);

	// Make sure 's' bit is correct.  This will also set SPCFLAG_INT, and
	// if either the t0 or t1 bits are set, SPCFLAG_TRACE, too.

	EmAssert (gCPU68K);
	gCPU68K->UpdateRegistersFromSR ();

	// I think we need to do a little extra handling here of the TRACE
	// mode.  If the TRACE mode bit is set, UpdateRegistersFromSR will
	// set its SPCFLAG_TRACE flag, which tells the CPU loop to finish
	// executing the current instruction, and then set the SPCFLAG_DOTRACE
	// flag afterwards, which will then cause execution to stop after
	// the NEXT instruction.  We have a bit of an edge condition here
	// when we set the SPCFLAG_TRACE flag BETWEEN instructions.  Doing it
	// this way will cause the next instruction to be treated as the
	// "current" instruction, and so execution will stop after executing
	// the second opcode.  We want execution to stop with the first
	// opcode, so fix up the flags here.  This code is the same as in
	// Emulator::ExecuteSpecial for managing this stuff.

	if (regs.spcflags & SPCFLAG_TRACE)
	{
		regs.spcflags &= ~SPCFLAG_TRACE;
		regs.spcflags |= SPCFLAG_DOTRACE;
	}

	// Finally, update the stack pointer from the usp or ssp,
	// as appropriate.

	if (regs.s == 0)
		m68k_areg (regs, 7) = debuggerRegs.usp;
	else
		m68k_areg (regs, 7) = debuggerRegs.ssp;
}
