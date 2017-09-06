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

#ifndef _SOCKETMESSAGING_H_
#define _SOCKETMESSAGING_H_


struct sockaddr;


class CSocket
{
	public:
		enum { kConnected, kDataReceived, kDisconnected };
		typedef void (*EventCallback) (CSocket*, int event);

	public:
		static void 			Startup 			(void);
		static void 			Shutdown			(void);
		static ErrCode			IdleAll 			(void);

	public:
								CSocket 			(void);

		// Call Delete() instead of the destructor.  Our architecture
		// is such that sockets can delete themselves while IdleAll
		// is executing.  Doing that causes IdleAll to freak out,
		// so we have Delete() in order to defer the actual deletion.
		void					Delete				(void);
		Bool					Deleted				(void) const;

		virtual ErrCode 		Open				(void) = 0;
		virtual ErrCode 		Close				(void) = 0;
		virtual ErrCode 		Write				(const void* buffer, long amountToWrite, long* amtWritten) = 0;
		virtual ErrCode 		Read				(void* buffer, long sizeOfBuffer, long* amtRead) = 0;
		virtual Bool			HasUnreadData		(long timeout) = 0;
		virtual ErrCode 		Idle				(void) = 0;

		virtual Bool			ShortPacketHack 	(void);
		virtual Bool			ByteswapHack		(void);

	protected:
		virtual 				~CSocket			(void);

		static void				AddPending			(void);
		static void				DeletePending		(void);

	private:
		Bool					fDeleted;
};

class CTCPSocket : public CSocket
{
	public:
								CTCPSocket			(EventCallback, int port);
		virtual 				~CTCPSocket 		(void);

		virtual ErrCode 		Open				(void);
		virtual ErrCode 		Close				(void);
		virtual ErrCode 		Write				(const void* buffer, long amountToWrite, long* amtWritten);
		virtual ErrCode 		Read				(void* buffer, long sizeOfBuffer, long* amtRead);
		virtual Bool			HasUnreadData		(long timeout);
		virtual ErrCode 		Idle				(void);

		Bool					ConnectPending		(void);
		ErrCode 				AcceptConnection	(void);
		Bool					IsConnected 		(void);
		Bool					CheckConnection 	(void);

	protected:
		SOCKET					NewSocket			(void);
		sockaddr*				FillAddress 		(sockaddr* addr, bool forConnect);
		ErrCode					ErrorOccurred		(void);
		ErrCode					GetError			(void);

		EventCallback			fEventCallback;
		int						fPort;

		int						fSocketState;
		SOCKET					fListeningSocket;
		SOCKET					fConnectedSocket;
};

#endif /* _SOCKETMESSAGING_H_ */

