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
#include "SLP.h"

#include "Byteswapping.h"		// Canonical
#include "DebugMgr.h"			// Debug::HandleNewPacket
#include "EmErrCodes.h"			// kError_NoError
#include "EmException.h"		// EmExceptionReset
#include "EmPalmFunction.h"		// GetTrapName
#include "EmRPC.h"				// RPC::HandleNewPacket, slkSocketRPC
#include "EmSession.h" 			// EmSessionStopper
#include "Logging.h"			// LogAppendMsg
#include "SocketMessaging.h"	// CSocket::Write
#include "StringData.h" 		// kExceptionNames, kPacketNames

#define PacketName(command) (kPacketNames[command])

#define PRINTF	if (!this->LogFlow ()) ; else LogAppendMsg

static void PrvPrintHeader	(const EmAliasSlkPktHeaderType<LAS>& header);
static void PrvPrintHeader	(const EmProxySlkPktHeaderType& header);
static void PrvPrintBody	(const EmAliasSysPktBodyType<LAS>& body);
static void PrvPrintBody	(const EmProxySysPktBodyType& body);
static void PrvPrintFooter	(const EmAliasSlkPktFooterType<LAS>& footer);
static void PrvPrintFooter	(const EmProxySlkPktFooterType& footer);


/***********************************************************************
 *
 * FUNCTION:	SLP::SLP
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

SLP::SLP (void) :
	fSocket (NULL),
	fHeader (),
	fBody (),
	fFooter (),
	fHavePacket(false),
	fSendReply (true)
{
}


/***********************************************************************
 *
 * FUNCTION:	SLP::SLP
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

SLP::SLP (CSocket* s) :
	fSocket (s),
	fHeader (),
	fBody (),
	fFooter (),
	fHavePacket (false),
	fSendReply (true)
{
}


/***********************************************************************
 *
 * FUNCTION:	SLP::SLP
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

SLP::SLP (const SLP& other) :
	fSocket (other.fSocket),
	fHeader (other.fHeader),
	fBody (other.fBody),
	fFooter (other.fFooter),
	fHavePacket (other.fHavePacket),
	fSendReply (other.fSendReply)
{
	EmAssert (fSocket);
}


/***********************************************************************
 *
 * FUNCTION:	SLP::~SLP
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

SLP::~SLP (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	SLP::EventCallback
 *
 * DESCRIPTION: Standard callback for handling SLP packets.  If data
 *				is received, we stop the emulator thread and call
 *				HandleDataReceived to read the dispatch the packet.
 *				Nothing is done by default on connect and disconnect
 *				events.
 *
 * PARAMETERS:	s - the socket that connected, disconnected, or received
 *					some data.
 *
 *				event - a code indicating what happened.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void SLP::EventCallback (CSocket* s, int event)
{
	switch (event)
	{
		case CSocket::kConnected:
		{
			break;
		}

		case CSocket::kDataReceived:
		{
			ErrCode	result;
			do {
				SLP slp (s);
				result = slp.HandleDataReceived ();
			} while (result == errNone && s->HasUnreadData (500));
			break;
		}

		case CSocket::kDisconnected:
		{
//			if (s == fSocket)
//			{
//				fSocket = NULL;
//			}
			break;
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	SLP::HandleDataReceived
 *
 * DESCRIPTION: Called when received data is pending in the socket.
 *				Read that data, break it down into header, body, and
 *				footer sections, and call HandleNewPacket to dispatch
 *				it to the right sub-system.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	An error indicating what bad things happened.
 *
 ***********************************************************************/

ErrCode SLP::HandleDataReceived (void)
{
	ErrCode result = errNone;

//	do {
		EmAssert (fSocket);

		long		amtRead;
		ErrCode 	err = fSocket->Read (	this->Header ().GetPtr (),
											this->Header ().GetSize (),
											&amtRead);

		if (err == errNone && amtRead == (long) this->Header ().GetSize ())
		{
			fHavePacket = true;

			if (this->Header ().bodySize > 0)
			{
				fSocket->Read (	this->Body ().GetPtr (),
								this->Header ().bodySize,
								NULL);
			}

			if (!fSocket->ShortPacketHack ())
			{
				fSocket->Read (	this->Footer ().GetPtr (),
								this->Footer ().GetSize (),
								NULL);
			}

			if (this->Header ().bodySize >= 2)
				result = this->HandleNewPacket ();
			else
				result = errNone;	// !!! Body is too small ... what to do?
		}
		else
		{
//			fSocket = NULL; // Disconnected?
			if (err == errNone)
			{
				result = 1;
			}
		}
//	} while (result == errNone && fSocket && fSocket->HasUnreadData (500));

	return result;
}

				
/***********************************************************************
 *
 * FUNCTION:	SLP::HandleNewPacket
 *
 * DESCRIPTION: Dispatch the packet information to the right sub-system.
 *
 * PARAMETERS:	header - contents of header part of message.
 *
 *				body - contents of body part of message.
 *
 *				footer - contents of footer part of message.  May
 *					not be valid with all sockets.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SLP::HandleNewPacket ()
{
	ErrCode result = kError_NoError;

	// Say that we're here.  Do this after setting gLastestHeader, as the PRINTF
	// macro looks at it.

	PRINTF ("Entering SLP::HandleNewPacket.");

	// Print the logging information if requested.

	if (this->LogData ())
	{
		// PrvPrintBody calls GetTrapName -> FindTrapName -> GetFunctionAddress ->
		// GetSysFunctionAddress -> LowMem::GetTrapAddress, which creates
		// a CEnableFullAccess object.  In order to create one of those outside
		// the CPU thread, we must stop the CPU thread first.
		EmSessionStopper	stopper (gSession, kStopNow);
		if (stopper.Stopped ())
		{
			PrvPrintHeader (this->Header ());
			PrvPrintBody (this->Body ());
			PrvPrintFooter (this->Footer ());
		}
	}
	else if (this->LogFlow ())
	{
		if (PacketName (this->Body().command))
			LogAppendMsg (" Received %s packet.", PacketName (this->Body().command));
		else
			LogAppendMsg (" Received unknown (0x%02X) packet.", (UInt8) this->Body().command);
	}

	// Dispatch the packet to the right sub-system.

	switch (this->Header().dest)
	{
		case slkSocketDebugger:
		{
			EmSessionStopper	stopper (gSession, kStopNow);
			if (stopper.Stopped ())
			{
				try
				{
					result = Debug::HandleNewPacket (*this);
				}
				catch (EmExceptionReset&)
				{
					gSession->Reset (kResetSoft);
					throw;
				}
			}
		}
		break;

		case slkSocketConsole:
		{
			EmSessionStopper	stopper (gSession, kStopOnSysCall);
			if (stopper.Stopped ())
			{
				try
				{
					result = Debug::HandleNewPacket (*this);
				}
				catch (EmExceptionReset&)
				{
					gSession->Reset (kResetSoft);
					throw;
				}
			}
		}
		break;

		case slkSocketRPC:
		{
			EmSessionStopper	stopper (gSession, kStopOnSysCall);
			if (stopper.Stopped ())
			{
				try
				{
					result = RPC::HandleNewPacket (*this);
				}
				catch (EmExceptionReset&)
				{
					gSession->Reset (kResetSoft);
					throw;
				}
			}
		}
		break;

		default:
			result = slkErrWrongDestSocket;
			PRINTF ("Unknown destination: %ld.", (long) this->Header ().dest);
			break;
	}

	PRINTF ("Exiting SLP::HandleNewPacket.");

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SLP::SendPacket
 *
 * DESCRIPTION: Sends the given packet to the external debugger.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode SLP::SendPacket (const void* bodyP, long bodySize)
{
	PRINTF ("Entering SLP::SendPacket.");

	if (!fSendReply)
	{
		PRINTF ("Not sending a reply because we were asked not to.");
		PRINTF ("Exiting SLP::SendPacket.");
		return errNone;
	}

	// Fold all the parts into a buffer: header, body, footer.

	long		totalSize = EmAliasSlkPktHeaderType<LAS>::GetSize () +
							bodySize + 
							EmAliasSlkPktFooterType<LAS>::GetSize ();
	StMemory	buffer (totalSize);
	char*		bufferP = (char*) buffer.Get ();

	EmAliasSlkPktHeaderType<LAS>	header	(&bufferP[0]);
	EmAliasSysPktBodyType<LAS>		body	(&bufferP[header.GetSize ()]);
	EmAliasSlkPktFooterType<LAS>	footer	(&bufferP[header.GetSize () + bodySize]);


	// Gen up a header.  If we're replying to a packet from an external
	// source, use the information from it to form a reply (socket
	// numbers and transaction ID).  Otherwise, stuff in some of our
	// own values.

	if (this->HavePacket())
	{
		header.signature1	= slkPktHeaderSignature1;
		header.signature2	= slkPktHeaderSignature2;
		header.dest			= this->Header().src;
		header.src			= this->Header().dest;
		header.type			= slkPktTypeSystem;
		header.bodySize		= bodySize;
		header.transId		= this->Header().transId;
		header.checksum		= 0;
	}
	else
	{
		header.signature1	= slkPktHeaderSignature1;
		header.signature2	= slkPktHeaderSignature2;
		header.dest			= slkSocketDebugger;	// !!! May want to parameterize these two.
		header.src			= slkSocketDebugger;	// !!! May want to parameterize these two.
		header.type			= slkPktTypeSystem;
		header.bodySize		= bodySize;
		header.transId		= 0;
		header.checksum		= 0;
	}

	// Compute and stuff the header checksum

	header.checksum	= SLP::CalcHdrChecksum (0,
						(UInt8*) header.GetPtr (),
						header.offsetof_checksum ());

	if (this->LogData ())
		PrvPrintHeader (header);

	// Copy in the packet body and byteswap it.

	memcpy (body.GetPtr (), bodyP, bodySize);

	if (this->LogData ())
	{
		PrvPrintBody (body);
	}
	else if (this->LogFlow ())
	{
		if (PacketName (body.command))
			LogAppendMsg (" Sending %s packet.", PacketName (body.command));
		else
			LogAppendMsg (" Sending unknown (0x%02X) packet.", (UInt8) body.command);
	}

	// Re-introduce byteswapping bugs if necessary for clients
	// that expect them.

	EmAssert (fSocket);
	if (fSocket->ByteswapHack ())
	{
		if (body.command == sysPktRPCRsp)
		{
			EmAliasSysPktRPCType<LAS>	rpc (body.GetPtr ());

			Canonical (*(uint32*) rpc.resultD0.GetPtr ());
			Canonical (*(uint32*) rpc.resultA0.GetPtr ());
		}
		else if (body.command == sysPktReadRegsRsp)
		{
			EmAliasSysPktReadRegsRspType<LAS>	regs (body.GetPtr ());
			uint32*	regsPtr = (uint32*) regs.reg.GetPtr ();

			Canonical (*regsPtr++);	// D0
			Canonical (*regsPtr++);	// D1
			Canonical (*regsPtr++);	// D2
			Canonical (*regsPtr++);	// D3
			Canonical (*regsPtr++);	// D4
			Canonical (*regsPtr++);	// D5
			Canonical (*regsPtr++);	// D6
			Canonical (*regsPtr++);	// D7
			Canonical (*regsPtr++);	// A0
			Canonical (*regsPtr++);	// A1
			Canonical (*regsPtr++);	// A2
			Canonical (*regsPtr++);	// A3
			Canonical (*regsPtr++);	// A4
			Canonical (*regsPtr++);	// A5
			Canonical (*regsPtr++);	// A6

			Canonical (*(uint32*) regs.reg.usp.GetPtr ());
			Canonical (*(uint32*) regs.reg.ssp.GetPtr ());
			Canonical (*(uint32*) regs.reg.pc.GetPtr ());
			Canonical (*(uint16*) regs.reg.sr.GetPtr ());
		}
	}


	// Calculate the footer checksum

	footer.crc16 = ::Crc16CalcBlock (header.GetPtr (), header.GetSize(), 0);
	footer.crc16 = ::Crc16CalcBlock (body.GetPtr (), bodySize, footer.crc16);
	if (this->LogData ())
		PrvPrintFooter (footer);


	// Send it to the external client.

	if (fSocket->ShortPacketHack ())
	{
		totalSize -= EmAliasSlkPktFooterType<LAS>::GetSize ();
	}

	long	amtWritten;
	ErrCode result = fSocket->Write (bufferP, totalSize, &amtWritten);

	PRINTF ("Exiting SLP::SendPacket.");

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SLP::HavePacket
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Bool SLP::HavePacket (void) const
{
	return fHavePacket;
}


/***********************************************************************
 *
 * FUNCTION:	SLP::GetPacketSize
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

long SLP::GetPacketSize (void) const
{
	EmAssert (this->HavePacket());

	return EmProxySlkPktHeaderType::GetSize () +
			this->Header ().bodySize +
			EmProxySlkPktFooterType::GetSize ();
}


/***********************************************************************
 *
 * FUNCTION:	SLP::Header
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

const EmProxySlkPktHeaderType& SLP::Header (void) const
{
	return fHeader;
}

EmProxySlkPktHeaderType& SLP::Header (void)
{
	return fHeader;
}


/***********************************************************************
 *
 * FUNCTION:	SLP::Body
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

const EmProxySysPktBodyType& SLP::Body (void) const
{
	return fBody;
}

EmProxySysPktBodyType& SLP::Body (void)
{
	return fBody;
}


/***********************************************************************
 *
 * FUNCTION:	SLP:: Footer
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

const EmProxySlkPktFooterType& SLP::Footer (void) const
{
	return fFooter;
}

EmProxySlkPktFooterType& SLP::Footer (void)
{
	return fFooter;
}


/***********************************************************************
 *
 * FUNCTION:    SLP::DeferReply
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void SLP::DeferReply (Bool v)
{
	fSendReply = !v;
}


/***********************************************************************
 *
 * FUNCTION:	SLP::CalcHdrChecksum
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

SlkPktHeaderChecksum SLP::CalcHdrChecksum (SlkPktHeaderChecksum start,
										   UInt8* bufP, Int32 count)
{
	do {
		start += *bufP++;
	} while (--count);

	return start;
}


/***********************************************************************
 *
 * FUNCTION:	SLP::LogFlow
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Bool SLP::LogFlow (void)
{
	UInt8	dest = slkSocketDebugger;
	if (this->HavePacket())
		dest = this->Header().dest;

	switch (dest)
	{
		case slkSocketDebugger:
		case slkSocketConsole:
			return LogHLDebugger ();

		case slkSocketRPC:
			return LogRPC ();

		default:
			break;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	SLP::LogData
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Bool SLP::LogData (void)
{
	UInt8	dest = slkSocketDebugger;
	if (this->HavePacket())
		dest = this->Header().dest;

	switch (dest)
	{
		case slkSocketDebugger:
		case slkSocketConsole:
			return LogHLDebuggerData ();

		case slkSocketRPC:
			return LogRPCData ();

		default:
			break;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	PrvPrintHeader
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void PrvPrintHeader (const EmAliasSlkPktHeaderType<LAS>& header)
{
	return;

	LogAppendMsg (" header.signature1   = 0x%04X",	(UInt16) header.signature1);
	LogAppendMsg (" header.signature2   = 0x%02X",	(UInt8) header.signature2);
	LogAppendMsg (" header.dest         = 0x%02X",	(UInt8) header.dest);
	LogAppendMsg (" header.src          = 0x%02X",	(UInt8) header.src);
	LogAppendMsg (" header.type         = 0x%02X",	(UInt8) header.type);
	LogAppendMsg (" header.bodySize     = 0x%04X",	(UInt16) header.bodySize);
	LogAppendMsg (" header.transId      = 0x%02X",	(UInt8) header.transId);
	LogAppendMsg (" header.checksum     = 0x%02X",	(SlkPktHeaderChecksum) header.checksum);
}


void PrvPrintHeader (const EmProxySlkPktHeaderType& header)
{
	return;

	EmAliasSlkPktHeaderType<LAS>	alias (header.GetPtr ());
	PrvPrintHeader (alias);
}


/***********************************************************************
 *
 * FUNCTION:	PrvPrintBody
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void PrvPrintBody (const EmAliasSysPktBodyType<LAS>& body)
{
	static UInt16 gLastNumReadBytes;

	if (PacketName (body.command))
	{
		LogAppendMsg (" body.command        = 0x%02X / %s.",
			(UInt8) body.command,
			PacketName (body.command));
	}
	else
	{
		LogAppendMsg (" body.command        = 0x%02X / <unknown command>.",
			(UInt8) body.command);
	}

	switch (body.command)
	{
		case sysPktStateCmd:
			break;
		case sysPktStateRsp:
		{
			EmAliasSysPktStateRspType<LAS> body2 (body.GetPtr ());
			int what = body2.exceptionId / 4;

			LogAppendMsg (" body.resetted       = %s",		(Boolean) body2.resetted ? "true" : "false");
			LogAppendMsg (" body.exceptionId    = 0x%02X / %s", what, kExceptionNames [what]);

			LogAppendMsg (" body.reg.d[0]       = 0x%08X",	(UInt32) body2.reg.d[0]);
			LogAppendMsg (" body.reg.d[1]       = 0x%08X",	(UInt32) body2.reg.d[1]);
			LogAppendMsg (" body.reg.d[2]       = 0x%08X",	(UInt32) body2.reg.d[2]);
			LogAppendMsg (" body.reg.d[3]       = 0x%08X",	(UInt32) body2.reg.d[3]);
			LogAppendMsg (" body.reg.d[4]       = 0x%08X",	(UInt32) body2.reg.d[4]);
			LogAppendMsg (" body.reg.d[5]       = 0x%08X",	(UInt32) body2.reg.d[5]);
			LogAppendMsg (" body.reg.d[6]       = 0x%08X",	(UInt32) body2.reg.d[6]);
			LogAppendMsg (" body.reg.d[7]       = 0x%08X",	(UInt32) body2.reg.d[7]);

			LogAppendMsg (" body.reg.a[0]       = 0x%08X",	(UInt32) body2.reg.a[0]);
			LogAppendMsg (" body.reg.a[1]       = 0x%08X",	(UInt32) body2.reg.a[1]);
			LogAppendMsg (" body.reg.a[2]       = 0x%08X",	(UInt32) body2.reg.a[2]);
			LogAppendMsg (" body.reg.a[3]       = 0x%08X",	(UInt32) body2.reg.a[3]);
			LogAppendMsg (" body.reg.a[4]       = 0x%08X",	(UInt32) body2.reg.a[4]);
			LogAppendMsg (" body.reg.a[5]       = 0x%08X",	(UInt32) body2.reg.a[5]);
			LogAppendMsg (" body.reg.a[6]       = 0x%08X",	(UInt32) body2.reg.a[6]);

			LogAppendMsg (" body.reg.usp        = 0x%08X",	(UInt32) body2.reg.usp);
			LogAppendMsg (" body.reg.ssp        = 0x%08X",	(UInt32) body2.reg.ssp);
			LogAppendMsg (" body.reg.pc         = 0x%08X",	(UInt32) body2.reg.pc);
			LogAppendMsg (" body.reg.sr         = 0x%04X",	(UInt16) body2.reg.sr);

			LogAppendMsg (" body.bp[0].addr     = 0x%08X",	(emuptr) body2.bp[0].addr);
			LogAppendMsg (" body.bp[1].addr     = 0x%08X",	(emuptr) body2.bp[1].addr);
			LogAppendMsg (" body.bp[2].addr     = 0x%08X",	(emuptr) body2.bp[2].addr);
			LogAppendMsg (" body.bp[3].addr     = 0x%08X",	(emuptr) body2.bp[3].addr);
			LogAppendMsg (" body.bp[4].addr     = 0x%08X",	(emuptr) body2.bp[4].addr);
			LogAppendMsg (" body.bp[5].addr     = 0x%08X",	(emuptr) body2.bp[5].addr);

			LogAppendMsg (" body.startAddr      = 0x%08X",	(emuptr) body2.startAddr);
			LogAppendMsg (" body.endAddr        = 0x%08X",	(emuptr) body2.endAddr);
			LogAppendMsg (" body.name           = %s",		(char*) body2.name.GetPtr ());
			LogAppendMsg (" body.trapTableRev   = %d",		(UInt8) body2.trapTableRev);
			break;
		}

		case sysPktReadMemCmd:
		{
			EmAliasSysPktReadMemCmdType<LAS>	body2 (body.GetPtr ());
			LogAppendMsg (" body.address        = 0x%08X",	(emuptr) body2.address);
			LogAppendMsg (" body.numBytes       = 0x%04X",	(UInt16) body2.numBytes);

			gLastNumReadBytes = body2.numBytes;
			break;
		}
		case sysPktReadMemRsp:
		{
			EmAliasSysPktReadMemRspType<LAS>	body2 (body.GetPtr ());
			LogAppendData (body2.data.GetPtr (), gLastNumReadBytes, " body.data           = ");
			break;
		}

		case sysPktWriteMemCmd:
		{
			EmAliasSysPktWriteMemCmdType<LAS>	body2 (body.GetPtr ());
			LogAppendMsg (" body.address        = 0x%08X",	(emuptr) body2.address);
			LogAppendMsg (" body.numBytes       = 0x%04X",	(UInt16) body2.numBytes);
			LogAppendData (body2.data.GetPtr (), body2.numBytes, "    body.data           = ");
			break;
		}
		case sysPktWriteMemRsp:
			break;

		case sysPktSingleStepCmd:
			break;
//		case sysPktSingleStepRsp:
//			break;

		case sysPktGetRtnNameCmd:
		{
			EmAliasSysPktRtnNameRspType<LAS>	body2 (body.GetPtr ());
			LogAppendMsg (" body.address        = 0x%08X",	(emuptr) body2.address);
			break;
		}
		case sysPktGetRtnNameRsp:
		{
			EmAliasSysPktRtnNameRspType<LAS>	body2 (body.GetPtr ());
			LogAppendMsg (" body.startAddr      = 0x%08X",	(emuptr) body2.startAddr);
			LogAppendMsg (" body.endAddr        = 0x%08X",	(emuptr) body2.endAddr);
			LogAppendMsg (" body.name           = %s",		(char*) body2.name.GetPtr ());
			break;
		}

		case sysPktReadRegsCmd:
			break;
		case sysPktReadRegsRsp:
			break;

		case sysPktWriteRegsCmd:
			break;
		case sysPktWriteRegsRsp:
			break;

		case sysPktContinueCmd:
			break;
//		case sysPktContinueRsp:
//			break;

		case sysPktRPCCmd:
		case sysPktRPCRsp:
		{
			EmAliasSysPktRPCType<LAS>	body2 (body.GetPtr ());
			LogAppendMsg (" body.trapWord       = 0x%04X / %s.", (UInt16) body2.trapWord,
													::GetTrapName (body2.trapWord));
			if (body.command == sysPktRPCRsp)
			{
				LogAppendMsg (" body.resultD0       = 0x%08X.",  (UInt32) body2.resultD0);
				LogAppendMsg (" body.resultA0       = 0x%08X.",  (UInt32) body2.resultA0);
			}

			LogAppendMsg (" body.numParams      = 0x%04X.", (UInt16) body2.numParams);

			void*	paramPtr	= body2.param.GetPtr ();

			for (UInt16 ii = 0; ii < body2.numParams; ++ii)
			{
				EmAliasSysPktRPCParamType<LAS> param (paramPtr);

				if (param.byRef)
				{
					LogAppendData (param.asByte.GetPtr (), param.size,
						"   body.param%d            = %d bytes of data.", ii, (UInt8) param.size);
				}
				else if (body.command == sysPktRPCCmd)
				{
					if (param.size == 1)
					{
						LogAppendMsg (" body.param%d            = 0x%02X.", ii, (UInt8) param.asByte);
					}
					else if (param.size == 2)
					{
						LogAppendMsg (" body.param%d            = 0x%04X.", ii, (UInt16) param.asShort);
					}
					else if (param.size == 4)
					{
						LogAppendMsg (" body.param%d            = 0x%08X.", ii, (UInt32) param.asLong);
					}
				}

				paramPtr = ((char*) param.asByte.GetPtr ()) + ((param.size + 1) & ~1);
			}
			break;
		}

		case sysPktGetBreakpointsCmd:
			break;
		case sysPktGetBreakpointsRsp:
			break;

//		case sysPktSetBreakpointsCmd:
//			break;
		case sysPktSetBreakpointsRsp:
			break;

		case sysPktRemoteUIUpdCmd:
			break;
//		case sysPktRemoteUIUpdRsp:
//			break;

		case sysPktRemoteEvtCmd:
			break;
//		case sysPktRemoteEvtRsp:
//			break;

//		case sysPktDbgBreakToggleCmd:
//			break;
		case sysPktDbgBreakToggleRsp:
			break;

		case sysPktFlashCmd:
			break;
		case sysPktFlashRsp:
			break;

		case sysPktCommCmd:
			break;
		case sysPktCommRsp:
			break;

		case sysPktGetTrapBreaksCmd:
			break;
		case sysPktGetTrapBreaksRsp:
			break;

		case sysPktSetTrapBreaksCmd:
			break;
		case sysPktSetTrapBreaksRsp:
			break;

		case sysPktGremlinsCmd:
			break;
//		case sysPktGremlinsRsp:
//			break;

		case sysPktFindCmd:
			break;
		case sysPktFindRsp:
			break;

		case sysPktGetTrapConditionsCmd:
			break;
		case sysPktGetTrapConditionsRsp:
			break;

		case sysPktSetTrapConditionsCmd:
			break;
		case sysPktSetTrapConditionsRsp:
			break;

		case sysPktChecksumCmd:
			break;
		case sysPktChecksumRsp:
			break;

		case sysPktExecFlashCmd:
			break;
		case sysPktExecFlashRsp:
			break;

		case sysPktRemoteMsgCmd:
			break;
//		case sysPktRemoteMsgRsp:
//			break;

		default:
			break;
	}
}


void PrvPrintBody (const EmProxySysPktBodyType& body)
{
	EmAliasSysPktBodyType<LAS>	alias (body.GetPtr ());
	PrvPrintBody (alias);
}


/***********************************************************************
 *
 * FUNCTION:	PrvPrintFooter
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void PrvPrintFooter (const EmAliasSlkPktFooterType<LAS>& footer)
{
	return;

	LogAppendMsg (" footer.crc16        = 0x%02X",	  (UInt16) footer.crc16);
}

void PrvPrintFooter (const EmProxySlkPktFooterType& footer)
{
	return;

	EmAliasSlkPktFooterType<LAS>	alias (footer.GetPtr ());
	PrvPrintFooter (alias);
}
