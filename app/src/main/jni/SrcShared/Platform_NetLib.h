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

#ifndef _PLATFORM_NETLIB_H_
#define _PLATFORM_NETLIB_H_

#include "SessionFile.h"		// SessionFile

class Platform_NetLib
{
	public:
		static void Initialize	(void);
		static void Reset		(void);
		static void Save		(SessionFile&);
		static void Load		(SessionFile&);
		static void Dispose		(void);

		static Bool				Redirecting (void);

		static Err				Open (UInt16 libRefNum, UInt16* netIFErrsP);
		static Err				OpenConfig (UInt16 libRefNum, UInt16 configIndex, UInt32 openFlags, UInt16* netIFErrsP);
		static Err				Close (UInt16 libRefNum, UInt16 immediate);
		static Err				Sleep (UInt16 libRefNum);
		static Err				Wake (UInt16 libRefNum);
		static Err				FinishCloseWait(UInt16 libRefNum);
		static Err				OpenIfCloseWait(UInt16 libRefNum);
		static Err				OpenCount (UInt16 refNum, UInt16* countP);
		static Err				HandlePowerOff (UInt16 refNum, EventPtr eventP);
		static Err				ConnectionRefresh(UInt16 refNum, Boolean refresh, 
									Boolean* allInterfacesUpP, UInt16* netIFErrP);
		static NetSocketRef		SocketOpen(UInt16 libRefNum, NetSocketAddrEnum domain, 
									NetSocketTypeEnum type, Int16 protocol, Int32 timeout, 
									Err* errP);
		static Int16			SocketClose(UInt16 libRefNum, NetSocketRef socket, Int32 timeout, 
									Err* errP);
		static Int16			SocketOptionSet(UInt16 libRefNum, NetSocketRef socket,
									NetSocketOptLevelEnum level, NetSocketOptEnum option, 
									MemPtr optValueP, UInt16 optValueLen,
									Int32 timeout, Err* errP);
		static Int16			SocketOptionGet(UInt16 libRefNum, NetSocketRef socket,
									NetSocketOptLevelEnum level, NetSocketOptEnum option,
									MemPtr optValueP, UInt16* optValueLenP,
									Int32 timeout, Err* errP);
		static Int16			SocketBind(UInt16 libRefNum, NetSocketRef socket,
									NetSocketAddrType* sockAddrP, Int16 addrLen, Int32 timeout, 
									Err* errP);
		static Int16			SocketConnect(UInt16 libRefNum, NetSocketRef socket,
									NetSocketAddrType* sockAddrP, Int16 addrLen, Int32 timeout, 
									Err* errP);
		static Int16			SocketListen(UInt16 libRefNum, NetSocketRef socket,
									UInt16 queueLen, Int32 timeout, Err* errP);
		static Int16			SocketAccept(UInt16 libRefNum, NetSocketRef socket,
									NetSocketAddrType* sockAddrP, Int16* addrLenP, Int32 timeout,
									Err* errP);
		static Int16			SocketShutdown(UInt16 libRefNum, NetSocketRef socket, 
									Int16 /*NetSocketDirEnum*/ direction, Int32 timeout, Err* errP);
		static Int16			SocketAddr(UInt16 libRefNum, NetSocketRef socketRef,
									NetSocketAddrType* locAddrP, Int16* locAddrLenP, 
									NetSocketAddrType* remAddrP, Int16* remAddrLenP, 
									Int32 timeout, Err* errP);
		static Int16			SendPB(UInt16 libRefNum, NetSocketRef socket,
									NetIOParamType* pbP, UInt16 flags, Int32 timeout, Err* errP);
		static Int16			Send(UInt16 libRefNum, NetSocketRef socket,
									const MemPtr bufP, UInt16 bufLen, UInt16 flags,
									MemPtr toAddrP, UInt16 toLen, Int32 timeout, Err* errP);
		static Int16			ReceivePB(UInt16 libRefNum, NetSocketRef socket,
									NetIOParamType* pbP, UInt16 flags, Int32 timeout, Err* errP);
		static Int16			Receive(UInt16 libRefNum, NetSocketRef socket,
									MemPtr bufP, UInt16 bufLen, UInt16 flags, 
									MemPtr fromAddrP, UInt16* fromLenP, Int32 timeout, Err* errP);
		static Int16			DmReceive(UInt16 libRefNum, NetSocketRef socket,
									MemPtr recordP, UInt32 recordOffset, UInt16 rcvLen, UInt16 flags, 
									MemPtr fromAddrP, UInt16* fromLenP, Int32 timeout, Err* errP);
		static NetHostInfoPtr	GetHostByName(UInt16 libRefNum, Char* nameP, 
									NetHostInfoBufPtr bufP, Int32 timeout, Err* errP);
		static NetHostInfoPtr	GetHostByAddr(UInt16 libRefNum, UInt8* addrP, UInt16 len, UInt16 type,
									NetHostInfoBufPtr bufP, Int32 timeout, Err* errP);
		static NetServInfoPtr	GetServByName(UInt16 libRefNum, Char* servNameP, 
									Char* protoNameP, NetServInfoBufPtr bufP, 
									Int32 timeout, Err* errP);
		static Int16			GetMailExchangeByName(UInt16 libRefNum, Char* mailNameP, 
									UInt16 maxEntries, 
									Char hostNames[][netDNSMaxDomainName+1], UInt16 priorities[], 
									Int32 timeout, Err* errP);
		static Int16			Select(UInt16 libRefNum, UInt16 width, NetFDSetType* readFDs, 
									NetFDSetType* writeFDs, NetFDSetType* exceptFDs,
									Int32 timeout, Err* errP);
};

#endif	// _PLATFORM_NETLIB_H_
