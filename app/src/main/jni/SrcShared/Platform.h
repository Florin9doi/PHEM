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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_


#ifdef __cplusplus
extern "C"
{
#endif

	// Define these wrapper functions for C modules.

void*	Platform_AllocateMemory (size_t size);
void*	Platform_ReallocMemory	(void* p, size_t size);
void	Platform_DisposeMemory	(void* p);

#ifdef __cplusplus
}	// extern "C"

	// Do all the rest of this for C++ studs.

#include "EmStructs.h"			// ByteList
#include "EmTypes.h"			// StrCode
#include "EmPatchIf.h"			// CallROMType

class ChunkFile;
class CSocket;
class EmFileRef;
class EmRect;
class SessionFile;
class StMemory;

// Globals

int _stricmp(const char *s1, const char *s2);
int _strnicmp(const char *s1, const char *s2, int n);
char* _strdup (const char* s);
char* _strlwr (char* s);

// Function prototypes.

class Platform
{
	public:

		static void 			Initialize				(void);
		static void 			Reset					(void);
		static void 			Save					(SessionFile&);
		static void 			Load					(SessionFile&);
		static void 			Dispose 				(void);

	// Resource-related functions

		static string			GetString				(StrCode id);
		static int				GetIDForError			(ErrCode error);
		static int				GetIDForRecovery		(ErrCode error);
	
		static string			GetShortVersionString	(void);

	// Clipboard-related functions

		static void				CopyToClipboard			(const ByteList& palmChars,
														 const ByteList& hostChars);
		static void				CopyFromClipboard		(ByteList& palmChars,
														 ByteList& hostChars);

		static uint16			RemapHostChar			(uint8 hostEncoding, uint8 deviceEncoding, uint16 hostChar);

		static void				RemapHostToPalmChars	(const ByteList& hostChars,
														 ByteList& palmChars);
		static void				RemapPalmToHostChars	(const ByteList& palmChars,
														 ByteList& hostChars);

	// Time-related functions

		static uint32			GetMilliseconds 		(void);

	// External debugger-related functions

		static CSocket* 		CreateDebuggerSocket	(void);
		static void 			ExitDebugger			(void);

	// Graphics-related functions

		static void 			ViewDrawLine			(int xStart, int yStart, int xEnd, int yEnd);
		static void 			ViewDrawPixel			(int xPos, int yPos);

	// Sound functions

		static CallROMType		SndDoCmd				(SndCommandType&);
		static void				StopSound				(void);
		static void				Beep					(void);

	// Whatever....
	
		static Bool				PinToScreen				(EmRect&);

		static void 			ToHostEOL				(StMemory& dest, long& destLen,
														 const char* src, long srcLen);


		static Bool 			ReadROMFileReference	(ChunkFile& docFile,
														 EmFileRef& f);
		static void 			WriteROMFileReference	(ChunkFile& docFile,
														 const EmFileRef& f);

		static void 			Delay					(void);
		static void 			CycleSlowly				(void);

		static void*			RealAllocateMemory	 	(size_t size, Bool clear, const char* file, int line);
		static void*			RealReallocMemory		(void* p, size_t size, const char* file, int line);
		template <class T>
		static void 			DisposeMemory			(T*& p)
								{
									// Normally, I don't put functions inline like
									// this.  However, if I try defining this function
									// in a separate body, VC++ complains that DisposeMemory
									// doesn't take one parameter.	Go figure...

									if (p)
									{
										RealDisposeMemory ((void*) p);
										p = NULL;
									}
								}
		static void 			RealDisposeMemory		(void* p);

			// Aliases for DisposeMemory, because I can never remember
			// what the real name is...
		template <class T>
		static void 			DeleteMemory			(T*& p) { DisposeMemory(p); }

		template <class T>
		static void 			FreeMemory				(T*& p) { DisposeMemory(p); }

		static Bool				ForceStartupScreen		(void);
		static Bool 			StopOnResetKeyDown		(void);

	// Parse up the command line in a platform-specific fashion.  In particular,
	// FLTK likes to take over the iteration so that it can scarf up any
	// standard X options.  We have to let *it* do the iteration in order to
	// get everything to work out right.  If we were to iterate over the
	// options and call the low-level FLTK "consume one option" function for
	// the options we don't know about, then when we later call fl::show to
	// show our main window (using the geometry, display, etc., information
	// from the command line), fl::show will try to parse the command line
	// again, fumbling on the options it doesn't know.  If we try to fool
	// fl::show by NULL-ing out the command line options it doesn't know about,
	// it will later crash when trying to call strlen(NULL).

		static Bool				CollectOptions			(int argc, char** argv, int& i, int (*cb)(int, char**, int&));
		static void				PrintHelp				(void);

		static void 			Debugger				(const char* = NULL);
};

#define AllocateMemory(size)	\
	RealAllocateMemory(size, false, __FILE__, __LINE__)

#define AllocateMemoryClear(size)	 \
	RealAllocateMemory(size, true, __FILE__, __LINE__)

#define ReallocMemory(p, size)	  \
	RealReallocMemory(p, size, __FILE__, __LINE__)


#if PLATFORM_MAC

	inline void Platform::Debugger (const char* text)
	{
		if (text)
		{
			LStr255	str (text);
			::DebugStr (str);
		}
		else
		{
			::Debugger ();
		}
	}

#endif

#if PLATFORM_WINDOWS

	inline void Platform::Debugger (const char*)
	{
		::DebugBreak ();
	}

#endif

#if PLATFORM_UNIX

#include <signal.h>

	inline void Platform::Debugger (const char*)
	{
		raise (SIGTRAP);
	}
	
#endif

#endif	// extern "C"

#endif /* _PLATFORM_H_ */
