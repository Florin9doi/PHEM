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

#ifndef EmTransportSerialAndroid_h
#define EmTransportSerialAndroid_h

#include "EmTransportSerial.h"

#include "omnithread.h"			// omni_mutex
#include <deque>				// deque


class EmHostTransportSerial
{
	public:
								EmHostTransportSerial (void);
								~EmHostTransportSerial (void);

		ErrCode					OpenCommPort		(const EmTransportSerial::ConfigSerial&);
		ErrCode					CreateCommThreads	(const EmTransportSerial::ConfigSerial&);
		ErrCode					DestroyCommThreads	(void);
		ErrCode					CloseCommPort		(void);

		// Manage data coming in the host serial port.
		void					PutIncomingData		(const void*, long&);
		void					GetIncomingData		(void*, long&);
		long					IncomingDataSize	(void);

		// Manage data going out the host serial port.
		void					PutOutgoingData		(const void*, long&);
		void					GetOutgoingData		(void*, long&);
		long					OutgoingDataSize	(void);

		static void*			CommRead			(void*);
		static void*			CommWrite			(void*);

		int						GetBaud				(EmTransportSerial::Baud);
		int						GetDataBits			(EmTransportSerial::DataBits);

	public:
		omni_thread*			fReadThread;
		omni_thread*			fWriteThread;

	   	int						fCommHandle;
		int						fCommSignalPipeA;
		int						fCommSignalPipeB;
		int						fTimeToQuit;
		omni_mutex				fDataMutex;
		omni_condition			fDataCondition;

		omni_mutex				fReadMutex;
		deque<char>				fReadBuffer;

		omni_mutex				fWriteMutex;
		deque<char>				fWriteBuffer;
};

#endif /* EmTransportSerialAndroid_h */
