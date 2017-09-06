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

#ifndef EmTransportSerial_h
#define EmTransportSerial_h

#include "EmTransport.h"

#include <map>
#include <string>
#include <vector>

class EmHostTransportSerial;

class EmTransportSerial : public EmTransport
{
	public:
		typedef string	PortName;
		typedef long	Baud;
		typedef long	StopBits;
		typedef long	DataBits;
		typedef long	HwrHandshake;

		typedef vector<PortName>	PortNameList;
		typedef vector<Baud>		BaudList;

		enum Parity
		{
			kNoParity,
			kOddParity,
			kEvenParity
		};

		enum RTSControl
		{
			kRTSOff,
			kRTSOn,
			kRTSAuto
		};

		// Note: this used to be named "Config", but that runs
		// afoul of a bug in VC++ (see KB Q143082).
		struct ConfigSerial : public EmTransport::Config
		{
									ConfigSerial	(void);
			virtual					~ConfigSerial	(void);

			virtual EmTransport*	NewTransport	(void);
			virtual EmTransport*	GetTransport	(void);

			bool operator==(const ConfigSerial& other) const;

			PortName		fPort;
			Baud			fBaud;
			Parity			fParity;
			StopBits		fStopBits;
			DataBits		fDataBits;
			HwrHandshake	fHwrHandshake;
		};

		typedef map<PortName, EmTransportSerial*>	OpenPortList;

	public:
								EmTransportSerial		(void);
								EmTransportSerial		(const EmTransportDescriptor&);
								EmTransportSerial		(const ConfigSerial&);
		virtual					~EmTransportSerial		(void);

		virtual ErrCode			Open					(void);
		virtual ErrCode			Close					(void);

		virtual ErrCode			Read					(long&, void*);
		virtual ErrCode			Write					(long&, const void*);

		virtual Bool			CanRead					(void);
		virtual Bool			CanWrite				(void);
		virtual long			BytesInBuffer			(long minBytes);
		virtual string			GetSpecificName			(void);

		ErrCode					SetConfig				(const ConfigSerial&);
		void					GetConfig				(ConfigSerial&);

		void					SetRTS					(RTSControl state);
		void					SetDTR					(Bool state);
		void					SetBreak				(Bool state);

		Bool					GetCTS					(void);
		Bool					GetDSR					(void);

		static EmTransportSerial*	GetTransport		(const ConfigSerial&);
		static void				GetDescriptorList		(EmTransportDescriptorList&);
		static void				GetSerialBaudList		(BaudList&);
                // RAI 2014: Moved from private so we can muck with it for GPS NMEA passthroug
		EmHostTransportSerial*	fHost;

	private:
		void					HostConstruct			(void);
		void					HostDestruct			(void);

		ErrCode					HostOpen				(void);
		ErrCode					HostClose				(void);

		ErrCode					HostRead				(long&, void*);
		ErrCode					HostWrite				(long&, const void*);
		long					HostBytesInBuffer		(long minBytes);

		ErrCode					HostSetConfig			(const ConfigSerial&);

		void					HostSetRTS				(RTSControl state);
		void					HostSetDTR				(Bool state);
		void					HostSetBreak			(Bool state);

		Bool					HostGetCTS				(void);
		Bool					HostGetDSR				(void);

		static void				HostGetPortNameList		(PortNameList&);
		static void				HostGetSerialBaudList	(BaudList&);

		//EmHostTransportSerial*	fHost;
		ConfigSerial			fConfig;
		Bool					fCommEstablished;

		static OpenPortList		fgOpenPorts;
};

#endif /* EmTransportSerial_h */
