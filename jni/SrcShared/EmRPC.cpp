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
#include "EmRPC.h"

#include "EmPalmStructs.h"		// SysPktRPCParamType, SysPktRPCType, SysPktRPC2Type
#include "EmSession.h"			// SuspendByExternal
#include "HostControl.h"		// hostErrTimeout
#include "Logging.h"			// LogAppendMsg
#include "Miscellaneous.h"		// CountBits
#include "Platform.h"			// Platform::GetMilliseconds
#include "PreferenceMgr.h"		// Preference
#include "EmMemory.h"			// CEnableFullAccess
#include "SLP.h"				// SLP::EventCallback
#include "SocketMessaging.h"	// CTCPSocket
#include "SystemPacket.h"		// SystemPacket::


struct SLPTimeout
{
	// Default constructor for the creation of STL collections.
	// CodeWarrior needed this; VC++ didn't.
	SLPTimeout () :
		fSLP (),
		fStart (0),
		fTimeout (0)
	{
	}

	SLPTimeout (SLP& slp, long timeout) :
		fSLP (slp),
		fStart (Platform::GetMilliseconds ()),
		fTimeout (timeout)
	{
	}

	SLP		fSLP;
	uint32	fStart;
	uint32	fTimeout;
};

typedef vector<SLPTimeout>	SLPTimeoutList;

static SLPTimeoutList		gSLPTimeouts;
static SLP*					gCurrentPacket;
static omni_mutex			gMutex;

#define PRINTF	if (true) ; else LogAppendMsg

/***********************************************************************
 *
 * FUNCTION:	RPC::Startup
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

void RPC::Startup (void)
{
	RPC::CreateNewListener ();
}


/***********************************************************************
 *
 * FUNCTION:	RPC::Shutdown
 *
 * DESCRIPTION: Call this once at application shutdown.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void RPC::Shutdown (void)
{
}

/***********************************************************************
 *
 * FUNCTION:    RPC::Idle
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void RPC::Idle (void)
{
	omni_mutex_lock	lock (gMutex);

	uint32	now = Platform::GetMilliseconds ();

	SLPTimeoutList::iterator	iter = gSLPTimeouts.begin ();

	while (iter != gSLPTimeouts.end ())
	{
		SLP&	slp		= iter->fSLP;
		uint32	start	= iter->fStart;
		uint32	timeout	= iter->fTimeout;

		if (now - start > timeout)
		{
			PRINTF ("RPC::Idle: Timing out.");

			EmAssert (slp.HavePacket ());

			slp.DeferReply (false);

			EmProxySysPktBodyType&	response = slp.Body ();
			if (response.command == sysPktRPCCmd)
			{
				EmAliasSysPktRPCType<LAS>	response (slp.Body().GetPtr());

				response.command			= sysPktRPCRsp;
				response._filler			= 0;

				response.resultD0			= (UInt32) hostErrTimeout;
				response.resultA0			= 0;
			}
			else
			{
				EmAliasSysPktRPC2Type<LAS>	response (slp.Body().GetPtr());

				response.command			= sysPktRPC2Rsp;
				response._filler			= 0;

				response.resultD0			= (UInt32) hostErrTimeout;
				response.resultA0			= 0;
				response.resultException	= 0;
			}

			long	bodySize = slp.GetPacketSize () - (slp.Header().GetSize() + slp.Footer().GetSize());

			slp.SendPacket (response.GetPtr (), bodySize);

			iter = gSLPTimeouts.erase (iter);
			continue;
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:    RPC::SignalWaiters
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void RPC::SignalWaiters (HostSignalType signal)
{
	omni_mutex_lock	lock (gMutex);

	PRINTF ("RPC::SignalWaiters: Entering");

	Bool						signalledOne = false;
	SLPTimeoutList::iterator	iter = gSLPTimeouts.begin ();

	while (iter != gSLPTimeouts.end ())
	{
		PRINTF ("RPC::SignalWaiters: Signaling");

		SLP&	slp		= iter->fSLP;

		EmAssert (slp.HavePacket ());

		slp.DeferReply (false);

		EmProxySysPktBodyType&	response = slp.Body ();
		if (response.command == sysPktRPCCmd)
		{
			EmAliasSysPktRPCType<LAS>	response (slp.Body().GetPtr());

			response.command			= sysPktRPCRsp;
			response._filler			= 0;

			response.resultD0			= errNone;
			response.resultA0			= 0;

			EmAliasSysPktRPCParamType<LAS>	param (response.param.GetPtr ());

			param.asLong				= signal;
		}
		else
		{
			EmAliasSysPktRPC2Type<LAS>	response (slp.Body().GetPtr());

			response.command			= sysPktRPC2Rsp;
			response._filler			= 0;

			response.resultD0			= errNone;
			response.resultA0			= 0;
			response.resultException	= 0;

			int							numRegs		= ::CountBits(response.DRegMask) +
													  ::CountBits(response.ARegMask);
			UInt16*						numParams	= ((UInt16*) response.Regs.GetPtr ()) + numRegs;
			EmAliasSysPktRPCParamType<LAS>	param (numParams + 1);

			param.asLong				= signal;
		}

		long	bodySize = slp.GetPacketSize () - (slp.Header().GetSize() + slp.Footer().GetSize());

		ErrCode result = slp.SendPacket (response.GetPtr (), bodySize);

		if (result == errNone)
		{
			signalledOne = true;
		}

		++iter;
	}

	gSLPTimeouts.clear ();

	// gSession may be NULL if we're signally a hostSignalQuit.

	if (signalledOne && gSession)
	{
		gSession->ScheduleSuspendExternal ();
	}

	PRINTF ("RPC::SignalWaiters: Exiting");
}


/***********************************************************************
 *
 * FUNCTION:	RPC::HandleNewPacket
 *
 * DESCRIPTION: Completely handle a packet sent from an external
 *				client, setting any state and sending a reply if
 *				necessary.
 *
 * PARAMETERS:	slp - a reference to a SerialLink Protocol object that
 *					contains the packet information and the horse...uh,
 *					socket it rode in on.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode RPC::HandleNewPacket (SLP& slp)
{
	ErrCode result = errNone;

	EmAssert (slp.Header ().dest == slkSocketRPC);

	gCurrentPacket = &slp;

	CEnableFullAccess	munge;	// Remove blocks on memory access.

	try
	{
		switch (slp.Body ().command)
		{
			case sysPktReadMemCmd:
				result = SystemPacket::ReadMem (slp);
				break;

			case sysPktWriteMemCmd:
				result = SystemPacket::WriteMem (slp);
				break;

			case sysPktRPCCmd:
				result = SystemPacket::RPC (slp);
				break;

			case sysPktRPC2Cmd:
				result = SystemPacket::RPC2 (slp);
				break;

			default:
				break;
		}
	}
	catch (...)
	{
		gCurrentPacket = NULL;
		throw;
	}

	gCurrentPacket = NULL;

	return result;
}


/***********************************************************************
 *
 * FUNCTION:    RPC::HandlingPacket
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

Bool RPC::HandlingPacket (void)
{
	return gCurrentPacket != NULL;
}


/***********************************************************************
 *
 * FUNCTION:    RPC::DeferCurrentPacket
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void RPC::DeferCurrentPacket (long timeout)
{
	omni_mutex_lock	lock (gMutex);

	PRINTF ("RPC::DeferCurrentPacket: Entering");

	EmAssert (gCurrentPacket);

	gCurrentPacket->DeferReply (true);
	gSLPTimeouts.push_back (SLPTimeout (*gCurrentPacket, timeout));

	PRINTF ("RPC::DeferCurrentPacket: Exiting");
}


/***********************************************************************
 *
 * FUNCTION:	RPC::EventCallback
 *
 * DESCRIPTION: Callback function for RPC-related sockets.	When an RPC
 *				socket connects, we instantly create a new listener.
 *				When an RPC socket disconnects, we delete it.
 *
 * PARAMETERS:	s - the socket that connected, disconnected, or received
 *					some data.
 *
 *				event - a code indicating what happened.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void RPC::EventCallback (CSocket* s, int event)
{
	switch (event)
	{
		case CSocket::kConnected:
		{
			RPC::CreateNewListener ();
			break;
		}

		case CSocket::kDataReceived:
		{
			break;
		}

		case CSocket::kDisconnected:
		{
			// A socket just disconnected.  If we had anything on that
			// socket waiting for us to signal it, forget about it.

			SLPTimeoutList::iterator	iter = gSLPTimeouts.begin ();

			while (iter != gSLPTimeouts.end ())
			{
				if (iter->fSLP.HasSocket (s))
				{
					gSLPTimeouts.erase (iter);
					iter = gSLPTimeouts.begin ();
					continue;
				}

				++iter;
			}

			s->Delete ();
		}
	}

	SLP::EventCallback (s, event);
}


/***********************************************************************
 *
 * FUNCTION:	RPC::CreateNewListener
 *
 * DESCRIPTION: Create a new socket for listening for RPC clients.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void RPC::CreateNewListener (void)
{
	Preference<long>	portPref (kPrefKeyRPCSocketPort);

	if (*portPref != 0)
	{
		CSocket*	rpcSocket = new CTCPSocket (&RPC::EventCallback, *portPref);
		ErrCode		err = rpcSocket->Open ();
		if (err != errNone)
		{
			rpcSocket->Delete ();
			rpcSocket = NULL;
		}
	}
}


