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

#ifndef EmPalmOS_h
#define EmPalmOS_h

#include "EmCPU68K.h"			// ExceptionNumber
#include "EmStructs.h"			// EmStackFrameList
#include "PreferenceMgr.h"		// PrefKeyType

#pragma mark StackRange

struct StackRange
{
	StackRange () :
		fTop (0),
		fLowWaterMark (0),
		fBottom (0)
		{}
	StackRange (const StackRange& other) :
		fTop (other.fTop),
		fLowWaterMark (other.fLowWaterMark),
		fBottom (other.fBottom)
		{}
	StackRange (emuptr bottom, emuptr top) :
		fTop (top),
		fLowWaterMark (top),
		fBottom (bottom)
		{}

	bool operator==(const StackRange& other) const
		{ return fBottom == other.fBottom; }

	bool operator!=(const StackRange& other) const
		{ return fBottom != other.fBottom; }

	bool operator<(const StackRange& other) const
		{ return fBottom < other.fBottom; }

	bool operator>(const StackRange& other) const
		{ return fBottom > other.fBottom; }

	// (Order these this way so that they look correct in a debugger)

	emuptr	fTop;
	emuptr	fLowWaterMark;
	emuptr	fBottom;
};

typedef vector<StackRange> StackList;

class EmStream;
EmStream& operator << (EmStream&, const StackRange&);
EmStream& operator >> (EmStream&, StackRange&);

class EmPalmChunk;
class SessionFile;

class EmPalmOS
{
	public:
		static void				Initialize					(void);
		static void				Reset						(void);
		static void				Save						(SessionFile&);
		static void				Load						(SessionFile&);
		static void				Dispose						(void);

	public:
		static void 			CheckStackPointerAssignment	(void);
		static void 			CheckStackPointerDecrement	(void);
		static void 			CheckKernelStack			(void);
		static void 			RememberStackChunk			(const EmPalmChunk&);
		static void 			RememberBootStack			(void);
		static void 			RememberKernelStack			(void);
		static void 			RememberStackRange			(const StackRange&);
		static void				SetCurrentStack				(const StackRange&);
		static void 			ForgetStack					(emuptr);
		static void 			ForgetStacksIn				(emuptr, uint32);
		static StackRange		GetBootStack				(void);
		static Bool				IsInStack					(emuptr);
		static Bool				IsInStackBlock				(emuptr);
		static void				GenerateStackCrawl			(EmStackFrameList& frameList);
		static void				PrefsChanged				(PrefKeyType, void*);

	protected:
		static Bool				HandleTrap15				(ExceptionNumber);
		static Bool				HandleJSR					(emuptr oldPC, emuptr dest);
		static Bool				HandleJSR_Ind				(emuptr oldPC, emuptr dest);
		static void				HandleLINK					(int linkSize);
		static Bool				HandleRTE					(emuptr dest);
		static Bool				HandleRTS					(emuptr dest);
		static void				HandleNewPC					(emuptr dest);
		static void				HandleNewSP					(EmStackChangeType);

	private:
		static Bool				HandleSystemCall			(Bool fromTrap);
};

#endif /* EmPalmOS_h */

