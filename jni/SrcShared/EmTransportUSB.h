/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef EmTransportUSB_h
#define EmTransportUSB_h

#include "EmTransport.h"

#include <map>
#include <string>
#include <vector>

class EmHostTransportUSB;

class EmTransportUSB : public EmTransport
{
	public:
		typedef string	PortName;
		typedef vector<PortName>		PortNameList;

		// Note: this used to be named "Config", but that runs
		// afoul of a bug in VC++ (see KB Q143082).
		struct ConfigUSB : public EmTransport::Config
		{
									ConfigUSB ();
			virtual					~ConfigUSB (void);

			virtual EmTransport*	NewTransport	(void);
			virtual EmTransport*	GetTransport	(void);

			bool operator==(const ConfigUSB& other) const;
		};

//		typedef map<PortName, EmTransportUSB*>	OpenPortList;

	public:
								EmTransportUSB			(void);
								EmTransportUSB			(const EmTransportDescriptor&);
								EmTransportUSB			(const ConfigUSB&);
		virtual					~EmTransportUSB			(void);

		virtual ErrCode			Open					(void);
		virtual ErrCode			Close					(void);

		virtual ErrCode			Read					(long&, void*);
		virtual ErrCode			Write					(long&, const void*);

		virtual Bool			CanRead					(void);
		virtual Bool			CanWrite				(void);
		virtual long			BytesInBuffer			(long minBytes);
		virtual string			GetSpecificName			(void);

		ErrCode					SetConfig				(const ConfigUSB&);
		void					GetConfig				(ConfigUSB&);

		static EmTransportUSB*	GetTransport			(const ConfigUSB&);

		static void				GetDescriptorList		(EmTransportDescriptorList&);

	private:
		static Bool				HostHasUSB				(void);

		void					HostConstruct			(void);
		void					HostDestruct			(void);

		ErrCode					HostOpen				(void);
		ErrCode					HostClose				(void);

		ErrCode					HostRead				(long&, void*);
		ErrCode					HostWrite				(long&, const void*);

		Bool					HostCanRead				(void);
		Bool					HostCanWrite			(void);
		long					HostBytesInBuffer		(long minBytes);

		ErrCode					HostSetConfig			(const ConfigUSB&);

		EmHostTransportUSB*		fHost;
		ConfigUSB				fConfig;
		Bool					fCommEstablished;

//		static OpenPortList		fgOpenPorts;
};

#endif /* EmTransportUSB_h */
