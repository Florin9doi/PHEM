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

#ifndef SYSTEMPACKET_H_
#define SYSTEMPACKET_H_

#include "EmTypes.h"			// ErrCode

class SLP;

class SystemPacket
{
	public:
		static ErrCode			SendState			(SLP&);
		static ErrCode			ReadMem 			(SLP&);
		static ErrCode			WriteMem			(SLP&);
		static ErrCode			SendRoutineName 	(SLP&);
		static ErrCode			ReadRegs			(SLP&);
		static ErrCode			WriteRegs			(SLP&);
		static ErrCode			Continue			(SLP&);
		static ErrCode			RPC 				(SLP&);
		static ErrCode			RPC2 				(SLP&);
		static ErrCode			GetBreakpoints		(SLP&);
		static ErrCode			SetBreakpoints		(SLP&);
		static ErrCode			ToggleBreak 		(SLP&);
		static ErrCode			GetTrapBreaks		(SLP&);
		static ErrCode			SetTrapBreaks		(SLP&);
		static ErrCode			Find				(SLP&);
		static ErrCode			GetTrapConditions	(SLP&);
		static ErrCode			SetTrapConditions	(SLP&);

		static ErrCode			SendMessage			(SLP&, const char*);

	private:
		static ErrCode			SendResponse		(SLP&, UInt8 code);
		static ErrCode			SendPacket			(SLP&, const void* body, long bodySize);

		static void 			GetRegs 			(M68KRegsType&);
		static void 			SetRegs 			(const M68KRegsType&);
};

#endif	// SYSTEMPACKET_H_
