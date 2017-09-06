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

#ifndef _ATRAPS_H_
#define _ATRAPS_H_

#include "Miscellaneous.h"		// StMemoryMapper
#include "UAE.h"				// regstruct

class SessionFile;

class ATrap
{
	public:
								ATrap			(void);
								~ATrap			(void);

		void					Call			(uint16 trapWord);

		void					PushByte		(uint8 iByte);
		void					PushWord		(uint16 iWord);
		void					PushLong		(uint32 iLong);

		void					SetNewDReg		(int regNum, uint32 value);
		void					SetNewAReg		(int regNum, uint32 value);

		uint32					GetD0			(void);
		uint32					GetA0			(void);

	protected:

		void					DoCall			(uint16 trapWord);

		char*					GetStackBase	(void);

	private:
		regstruct				fOldRegisters;
		regstruct				fNewRegisters;

//		static const int		kStackSize = 4096;	// VC++ is a bit medieval here...
		enum { kStackSize = 4096 };
		char					fStack[kStackSize + 3];

		StMemoryMapper			fEmulatedStackMapper;
};

#endif /* _ATRAPS_H_ */

