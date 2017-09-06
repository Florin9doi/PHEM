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

#ifndef EmRegsSED1375_h
#define EmRegsSED1375_h

#include "EmHAL.h"				// EmHALHandler
#include "EmPalmStructs.h"
#include "EmRegs.h"
#include "EmStructs.h"			// RGBList

class SessionFile;
class EmScreenUpdateInfo;


class EmRegsSED1375 : public EmRegs, public EmHALHandler
{
	public:
								EmRegsSED1375		(emuptr baseRegsAddr,
													 emuptr baseVideoAddr);
		virtual					~EmRegsSED1375		(void);

			// EmRegs overrides
		virtual void			Initialize			(void);
		virtual void			Reset				(Bool hardwareReset);
		virtual void			Save				(SessionFile&);
		virtual void			Load				(SessionFile&);
		virtual void			Dispose				(void);

		virtual void			SetSubBankHandlers	(void);
		virtual uint8*			GetRealAddress		(emuptr address);
		virtual emuptr			GetAddressStart		(void);
		virtual uint32			GetAddressRange		(void);

			// EmHAL overrides
		virtual Bool			GetLCDScreenOn		(void);
		virtual Bool			GetLCDBacklightOn	(void);
		virtual Bool			GetLCDHasFrame		(void);
		virtual void			GetLCDBeginEnd		(emuptr& begin, emuptr& end);
		virtual void			GetLCDScanlines		(EmScreenUpdateInfo& info);

	private:
		uint32					vertNonDisplayRead		(emuptr address, int size);
		uint32					lookUpTableDataRead		(emuptr address, int size);

		void 					invalidateWrite			(emuptr address, int size, uint32 value);
		void 					lookUpTableAddressWrite (emuptr address, int size, uint32 value);
		void					lookUpTableDataWrite	(emuptr address, int size, uint32 value);

	private:
		void					PrvGetPalette		(RGBList& thePalette);

	private:
		emuptr					fBaseRegsAddr;
		emuptr					fBaseVideoAddr;
		EmProxySED1375RegsType	fRegs;
		uint16					fClutData[256];
};

#endif	/* EmRegsSED1375_h */
