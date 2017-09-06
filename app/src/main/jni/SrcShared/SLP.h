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

#ifndef SLP_H_
#define SLP_H_

#include "EmPalmStructs.h"		// SlkPktHeaderType, SysPktBodyType, LAS
#include "EmTypes.h"			// ErrCode

class CSocket;

class SLP
{
	public:
								SLP 			(void);
								SLP 			(CSocket*);
								SLP 			(const SLP&);
								~SLP			(void);

		static void 			EventCallback	(CSocket* s, int event);
		ErrCode 				HandleDataReceived	(void);

		ErrCode 				HandleNewPacket (void);
		ErrCode 				SendPacket		(const void* body, long size);

		Bool					HavePacket		(void) const;
		long					GetPacketSize	(void) const;

		const EmProxySlkPktHeaderType&	Header	(void) const;
		const EmProxySysPktBodyType&	Body	(void) const;
		const EmProxySlkPktFooterType&	Footer	(void) const;

		EmProxySlkPktHeaderType&		Header	(void);
		EmProxySysPktBodyType&			Body	(void);
		EmProxySlkPktFooterType&		Footer	(void);

		void					DeferReply		(Bool);

		Bool					HasSocket		(CSocket* s) { return s == fSocket; }

	private:
		void					SetHeader		(void);
		void					SetBody			(void);
		void					SetFooter		(void);

		SlkPktHeaderChecksum	CalcHdrChecksum (SlkPktHeaderChecksum start,
												 UInt8* bufP, Int32 count);

		Bool					LogData 		(void);
		Bool					LogFlow 		(void);

	private:
		CSocket*				fSocket;

		EmProxySlkPktHeaderType	fHeader;
		EmProxySysPktBodyType	fBody;
		EmProxySlkPktFooterType	fFooter;

		Bool					fHavePacket;
		Bool					fSendReply;
};


#endif	// SPL_H_
