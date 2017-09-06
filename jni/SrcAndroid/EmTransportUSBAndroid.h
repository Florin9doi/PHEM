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

#ifndef EmTransportUSBAndroid_h
#define EmTransportUSBAndroid_h

#include "EmTransportUSB.h"

class EmTransportSerial;

class EmHostTransportUSB
{
	public:
								EmHostTransportUSB	(void);
								~EmHostTransportUSB	(void);

		void					UpdateOpenState		(void);
		Bool					FakeOpenConnection	(void);

	public:
		Bool					fOpenLocally;
		Bool					fOpenRemotely;
		EmTransportSerial*		fSerialTransport;
};

#endif /* EmTransportUSBAndroid_h */
