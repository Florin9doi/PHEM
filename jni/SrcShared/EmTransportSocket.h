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

#ifndef EmTransportSocket_h
#define EmTransportSocket_h

#include "EmTransport.h"
#include "SocketMessaging.h"

#include <deque>
#include <map>
#include <vector>
#include <string>

#include "omnithread.h"			// omni_mutex



class EmTransportSocket;

class CTCPClientSocket : public CTCPSocket
{
	public:
								CTCPClientSocket		(EventCallback fn, string targetHost, int targetPort, EmTransportSocket* transport);
		virtual 				~CTCPClientSocket 	(void);
		
		sockaddr*				FillAddress			(sockaddr* addr);
		virtual ErrCode 		Open				(void);
		ErrCode 				OpenInServerMode	(void);
		EmTransportSocket*		GetOwner			(void);
		sockaddr*				FillLocalAddress	(sockaddr* addr);

	private:
		string					fTargetHost;
		EmTransportSocket*		fTransport;	// Owner
};

class EmTransportSocket : public EmTransport
{
	public:
		typedef string	PortName;
		typedef vector<PortName>		PortNameList;

		struct ConfigSocket : public EmTransport::Config
		{
									ConfigSocket	(void);
			virtual					~ConfigSocket	(void);

			virtual EmTransport*	NewTransport	(void);
			virtual EmTransport*	GetTransport	(void);

			bool operator==(const ConfigSocket& other) const;

			string					fTargetHost;
			string					fTargetPort;
		};

		typedef map<PortName, EmTransportSocket*>	OpenPortList;

	public:
								EmTransportSocket		(void);
								EmTransportSocket		(const EmTransportDescriptor&);
								EmTransportSocket		(const ConfigSocket&);
		virtual					~EmTransportSocket		(void);

		virtual ErrCode			Open					(void);
		virtual ErrCode			Close					(void);

		virtual ErrCode			Read					(long&, void*);
		virtual ErrCode			Write					(long&, const void*);

		virtual Bool			CanRead					(void);
		virtual Bool			CanWrite				(void);
		virtual long			BytesInBuffer			(long minBytes);
		virtual string			GetSpecificName			(void);

		static EmTransportSocket*	GetTransport		(const ConfigSocket&);
		static void				GetDescriptorList		(EmTransportDescriptorList&);

		// ErrCode					OpenCommPort			(const EmTransportSocket::ConfigSocket&);
		ErrCode					OpenCommPortListen		(const EmTransportSocket::ConfigSocket&);
		ErrCode					OpenCommPortConnect		(const EmTransportSocket::ConfigSocket&);
		ErrCode					CloseCommPort			(void);
		ErrCode					CloseCommPortConnect	(void);
		ErrCode					CloseCommPortListen		(void);

		// Manage data coming in the host socket.
		void					PutIncomingData			(const void*, long&);
		void					GetIncomingData			(void*, long&);
		long					IncomingDataSize		(void);

		// Manage data going out the host socket.
		ErrCode					PutOutgoingData			(const void*, long&);
		long					OutgoingDataSize		(void);

		static void				EventCallBack			(CSocket* s, int event);

	public:

		omni_mutex				fReadMutex;
		deque<char>				fReadBuffer;

		// CTCPClientSocket*		fDataSocket;
		CTCPClientSocket*		fDataConnectSocket;
		CTCPClientSocket*		fDataListenSocket;

	private:

		ConfigSocket			fConfig;
		Bool					fCommEstablished;

		static OpenPortList		fgOpenPorts;
};

#endif /* EmTransportSocket_h */
