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

#ifndef EmCommon_h
#define EmCommon_h

// Common header file included by all Palm OS Emulator files.

// ==================================================================================
// Configuration
// ==================================================================================

// Define some symbols that control how the Mac and PowerPlant headers are handled.
// We need to do this here and not in EmCommonMac.h, as including Switches.h and
// EmTypes.h could pull in some system headers before we have a chance to include
// EmCommonMac.h.

#define PP_Suppress_Notes_20			1
#define PP_Suppress_Notes_21			1
#define PP_Suppress_Notes_22			1

#define PP_Target_Carbon				1
#define PP_Target_Classic				(!PP_Target_Carbon)

#define PP_StdDialogs_Option			PP_StdDialogs_NavServicesOnly
#define PP_Obsolete_ThrowExceptionCode	1

#define TARGET_API_MAC_CARBON			PP_Target_Carbon
#define TARGET_API_MAC_OS8				PP_Target_Classic
#define OTCARBONAPPLICATION				PP_Target_Carbon


// Pull in switches that define our configuration.  We require that the following
// symbols be defined externally, but all the rest are defined in Switches.h.
//
//		_DEBUG or NDEBUG
//		HAS_PROFILING (0 or 1)
//		INCLUDE_SECRET_STUFF (0 or 1)

#include "Switches.h"

#include "EmTypes.h"

// ==================================================================================
// Some very handy macros.
// ==================================================================================

#define countof(a)	(sizeof(a) / sizeof (a[0]))

#define UNUSED_PARAM(x)	((void) x);

#define COMPILE_TIME_ASSERT(x)	do { char _dummy[(x) ? 1 : -1]; UNUSED_PARAM (_dummy)} while (0)

#define DEFINE_SCALAR_MODIFIERS(type)		\
		/* Pre-increment operator */		\
	inline type	operator++(type& x)			\
	{										\
		x = type (x + 1);					\
		return x;							\
	}										\
											\
		/* Post-increment operator */		\
	inline type	operator++(type& x, int)	\
	{										\
		type result = x;					\
		x = type (x + 1);					\
		return result;						\
	}										\
											\
		/* Pre-decrement operator */		\
	inline type	operator--(type& x)			\
	{										\
		x = type (x - 1);					\
		return x;							\
	}										\
											\
		/* Post-decrement operator */		\
	inline type	operator--(type& x, int)	\
	{										\
		type result = x;					\
		x = type (x - 1);					\
		return result;						\
	}


// ==================================================================================
// Platform-independent definitions
// ==================================================================================

//#include "EmTypes.h"
//#include "EmStructs.h"


// ==================================================================================
// Platform-specific definitions
// ==================================================================================

#if PLATFORM_MAC
	#include "EmCommonMac.h"
#endif

#if PLATFORM_UNIX
	#include "EmCommonUnix.h"
#endif

#if PLATFORM_WINDOWS
	#include "EmCommonWin.h"
#endif

using namespace std;

// Everyone gets EmAssert!

#include "EmAssert.h"


// Compile-time checks to see that the sizes are right.

extern char check1	[sizeof(int8) == 1 ? 1 : 0];
extern char check2	[sizeof(int16) == 2 ? 1 : 0];
extern char check3	[sizeof(int32) == 4 ? 1 : 0];
extern char check4	[sizeof(int64) == 8 ? 1 : 0];

extern char check5	[sizeof(uint8) == 1 ? 1 : 0];
extern char check6	[sizeof(uint16) == 2 ? 1 : 0];
extern char check7	[sizeof(uint32) == 4 ? 1 : 0];
extern char check8	[sizeof(uint64) == 8 ? 1 : 0];

extern char check9	[sizeof(emuptr) == 4 ? 1 : 0];


#endif	/* EmCommon_h */
