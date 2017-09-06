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

#ifndef EmRPC_h
#define EmRPC_h

#include "EmTypes.h"			// ErrCode
#include "HostControl.h"		// HostSignal

class SLP;
class CSocket;

#define slkSocketRPC			(slkSocketFirstDynamic + 10)
#define sysPktRPC2Cmd			0x70
#define sysPktRPC2Rsp			0xF0

class RPC
{
	public:
		static void 			Startup 			(void);
		static void 			Shutdown			(void);

		static void				Idle				(void);
		static void				SignalWaiters		(HostSignalType);

		static ErrCode			HandleNewPacket 	(SLP&);

		static Bool				HandlingPacket		(void);
		static void				DeferCurrentPacket	(long timeout);

	private:
		static void 			EventCallback		(CSocket* s, int event);
		static void 			CreateNewListener	(void);
};

#endif	/* EmRPC_h */

