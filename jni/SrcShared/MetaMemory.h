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

#ifndef _METAMEMORY_H_
#define _METAMEMORY_H_

#include "EmMemory.h"			// EmMemGetMetaAddress
#include "EmPalmHeap.h"			// EmPalmHeap, EmPalmChunkList
#include "ErrorHandling.h"		// Errors::EAccessType


class MetaMemory
{
	public:
		static void				Initialize				(void);
		static void				Reset					(void);
		static void				Save					(SessionFile&);
		static void				Load					(SessionFile&);
		static void				Dispose					(void);

		// Called to mark and unmark some areas of memory.

		static void				MarkTotalAccess			(emuptr begin, emuptr end);

		static void				SetAccess				(emuptr begin, emuptr end, uint8 bits);

		static void				MarkLowMemory			(emuptr begin, emuptr end);
		static void				MarkSystemGlobals		(emuptr begin, emuptr end);
		static void				MarkHeapHeader			(emuptr begin, emuptr end);
		static void				MarkMPT					(emuptr begin, emuptr end);
		static void				MarkChunkHeader			(emuptr begin, emuptr end);
		static void				MarkChunkTrailer		(emuptr begin, emuptr end);
		static void				MarkLowStack			(emuptr begin, emuptr end);
		static void				MarkUnlockedChunk		(emuptr begin, emuptr end);
		static void				MarkFreeChunk			(emuptr begin, emuptr end);

		static void				MarkScreen				(emuptr begin, emuptr end);
		static void				UnmarkScreen			(emuptr begin, emuptr end);

		static void				MarkUIObject			(emuptr begin, emuptr end);
		static void				UnmarkUIObject			(emuptr begin, emuptr end);

		static void				MarkUIObjects			(void);
		static void				UnmarkUIObjects			(void);

		static void				MarkInstructionBreak	(emuptr opcodeLocation);
		static void				UnmarkInstructionBreak	(emuptr opcodeLocation);

		static void				MarkDataBreak			(emuptr begin, emuptr end);
		static void				UnmarkDataBreak			(emuptr begin, emuptr end);

		// Called when memory needs to be marked as initialized or not.

#if FOR_LATER
		static void				MarkUninitialized		(emuptr begin, emuptr end);
		static void				MoveUninitialized		(emuptr source, emuptr dest, uint32 size);
		static void				MarkInitialized			(emuptr begin, emuptr end);
		static void				MarkLongInitialized		(emuptr);	// Inlined, defined below
		static void				MarkWordInitialized		(emuptr);	// Inlined, defined below
		static void				MarkByteInitialized		(emuptr);	// Inlined, defined below
#endif

		// Called after a heap is created or changed.

		static void				Resync					(const EmPalmChunkList&);

		// Can a normal application access this memory location?
		// Forbidden locations are low-memory, system globals, screen
		// memory, memory manager structures, unlocked handles, and
		// uninitialized memory.

		static Bool				CanAppGetLong			(uint8*);	// Inlined, defined below
		static Bool				CanAppGetWord			(uint8*);	// Inlined, defined below
		static Bool				CanAppGetByte			(uint8*);	// Inlined, defined below

		static Bool				CanAppSetLong			(uint8*);	// Inlined, defined below
		static Bool				CanAppSetWord			(uint8*);	// Inlined, defined below
		static Bool				CanAppSetByte			(uint8*);	// Inlined, defined below

		// Can the non-Memory Manager parts of the system access this memory location?
		// Forbidden locations are low-memory, memory manager structures,
		// unlocked handles, and uninitialized memory.

		static Bool				CanSystemGetLong		(uint8*);	// Inlined, defined below
		static Bool				CanSystemGetWord		(uint8*);	// Inlined, defined below
		static Bool				CanSystemGetByte		(uint8*);	// Inlined, defined below

		static Bool				CanSystemSetLong		(uint8*);	// Inlined, defined below
		static Bool				CanSystemSetWord		(uint8*);	// Inlined, defined below
		static Bool				CanSystemSetByte		(uint8*);	// Inlined, defined below

		// Can the Memory Managers parts of the system access this memory location?
		// Forbidden locations are low-memory and uninitialized memory.

		static Bool				CanMemMgrGetLong		(uint8*);	// Inlined, defined below
		static Bool				CanMemMgrGetWord		(uint8*);	// Inlined, defined below
		static Bool				CanMemMgrGetByte		(uint8*);	// Inlined, defined below

		static Bool				CanMemMgrSetLong		(uint8*);	// Inlined, defined below
		static Bool				CanMemMgrSetWord		(uint8*);	// Inlined, defined below
		static Bool				CanMemMgrSetByte		(uint8*);	// Inlined, defined below

		static Errors::EAccessType
								GetWhatHappened			(emuptr address, long size, Bool forRead);
		static Errors::EAccessType
								AllowForBugs			(emuptr address, long size, Bool forRead, Errors::EAccessType);
		static void				CheckUIObjectAccess		(emuptr address, size_t size, Bool forRead,
														 Bool& isInUIObject, Bool& butItsOK);
		static Bool				InRAMOSComponent		(emuptr pc);
		static void				ChunkUnlocked			(emuptr addr);

		static void				RegisterBitmapHandle	(MemHandle);
		static void				RegisterBitmapPointer	(MemPtr);
		static Bool				IsBitmapHandle			(MemHandle);
		static Bool				IsBitmapPointer			(MemPtr);
		static void				UnregisterBitmapHandle	(MemHandle);
		static void				UnregisterBitmapPointer	(MemPtr);

		// Accessors for getting ranges of memory.

		static emuptr			GetLowMemoryBegin		(void);
		static emuptr			GetLowMemoryEnd			(void);
		static emuptr			GetSysGlobalsBegin		(void);
		static emuptr			GetSysGlobalsEnd		(void);
		static emuptr			GetHeapHdrBegin			(UInt16 heapID);
		static emuptr			GetHeapHdrEnd			(UInt16 heapID);

		// Predicates to help determine what kind of access error
		// just occurred.

		static Bool				IsLowMemory				(emuptr testAddress, uint32 size);
		static Bool				IsSystemGlobal			(emuptr testAddress, uint32 size);
		static Bool				IsScreenBuffer			(emuptr testAddress, uint32 size);
		static Bool				IsMemMgrData			(emuptr testAddress, uint32 size);
		static Bool				IsUnlockedChunk			(emuptr testAddress, uint32 size);
		static Bool				IsFreeChunk				(emuptr testAddress, uint32 size);
		static Bool				IsUninitialized			(emuptr testAddress, uint32 size);
		static Bool				IsStack					(emuptr testAddress, uint32 size);
		static Bool				IsLowStack				(emuptr testAddress, uint32 size);
		static Bool				IsAllocatedChunk		(emuptr testAddress, uint32 size);

		static Bool				IsScreenBuffer8			(uint8* metaAddress);	// Inlined, defined below
		static Bool				IsScreenBuffer16		(uint8* metaAddress);	// Inlined, defined below
		static Bool				IsScreenBuffer32		(uint8* metaAddress);	// Inlined, defined below
		static Bool				IsScreenBuffer			(uint8* metaAddress, uint32 size);

		static Bool				IsCPUBreak				(emuptr opcodeLocation);
		static Bool				IsCPUBreak				(uint8* metaLocation);

	private:
		struct ChunkCheck
		{
			emuptr	testAddress;
			uint32	size;
			Bool	result;
		};

		struct WhatHappenedData
		{
			Errors::EAccessType	result;
			emuptr				address;
			uint32				size;
			Bool				forRead;
		};

		static void				MarkRange				(emuptr start, emuptr end, uint8 v);
		static void				UnmarkRange				(emuptr start, emuptr end, uint8 v);
		static void				MarkUnmarkRange			(emuptr start, emuptr end,
														 uint8 andValue, uint8 orValue);

		static void				SyncOneChunk			(const EmPalmChunk& chunk);

		static void				GWH_ExamineHeap			(const EmPalmHeap& heap,
														 WhatHappenedData& info);
		static void				GWH_ExamineChunk		(const EmPalmChunk& chunk,
														 WhatHappenedData& info);
		/*
			Memory is laid out as follows:

				+-------------------+
				| Low Memory		|	Off Limits to everyone except IRQ handlers, SysSleep, and SysDoze
				+-------------------+
				|					|
				| System Globals	|	Off Limits to applications
				|					|
				+-------------------+
				| Dynamic Heap		|
				|+-----------------+|
				|| Heap Header	   ||	Off Limits to applications and non-MemMgr system
				|+-----------------+|
				|| Chunks		   ||	Off Limits to apps and non-MemMgr system if unlocked
				|+-----------------+|
				|| Free chunk	   ||	Off Limits to applications and non-MemMgr system
				|+-----------------+|
				|| Chunks		   ||
				|+-----------------+|
				|| Chunks		   ||
				|+-----------------+|
				|| Chunks		   ||
				|+-----------------+|
				|| Stack Chunk	   ||	Off Limits to everyone if below A7
				|+-----------------+|
				|| Screen Chunk	   ||	Off Limits to applications
				|+-----------------+|
				+-------------------+

			Any memory location can be marked as "uninitialized", which means that
			it can be written to but not read from.  An exception to this would be
			the parts of the memory manager that move around blocks (which may
			contain uninitialized sections).
		*/

		enum
		{
			kNoAppAccess		= 0x0001,
			kNoSystemAccess		= 0x0002,
			kNoMemMgrAccess		= 0x0004,
			kUnusedMeta1		= 0x0008,
			kStackBuffer		= 0x0010,	// Stack buffer; check to see if below-SP access is made.
			kScreenBuffer		= 0x0020,	// Screen buffer; update host screen if these bytes are changed.
			kInstructionBreak	= 0x0040,	// Halt CPU emulation and check to see why.
			kDataBreak			= 0x0080,	// Halt CPU emulation and check to see why.

			kLowMemoryBits		= kNoAppAccess | kNoSystemAccess | kNoMemMgrAccess,
			kGlobalsBits		= kNoAppAccess,
			kMPTBits			= kNoAppAccess,
			kMemStructBits		= kNoAppAccess | kNoSystemAccess,
			kLowStackBits		= kNoAppAccess | kNoSystemAccess | kNoMemMgrAccess,
			kFreeChunkBits		= kNoAppAccess | kNoSystemAccess,
			kUnlockedChunkBits	= kNoAppAccess | kNoSystemAccess,
			kScreenBits			= kNoAppAccess | kScreenBuffer,
			kUIObjectBits		= kNoAppAccess,
			kAccessBitMask		= kNoAppAccess | kNoSystemAccess | kNoMemMgrAccess,
			kFreeAccessBits		= 0
		};
};


// Macros to take a byte of bits and produce an
// 8-bit, 16-bit, or 32-bit version of it.

#define META_BITS_32(bits)		\
	(((uint32) (bits)) << 24) |	\
	(((uint32) (bits)) << 16) |	\
	(((uint32) (bits)) <<  8) |	\
	(((uint32) (bits)))

#define META_BITS_16(bits)		\
	(((uint16) (bits)) <<  8) |	\
	(((uint16) (bits)))

#define META_BITS_8(bits)		\
	(((uint8) (bits)))

// Macros to fetch the appropriate 8-bit, 16-bit, or
// 32-bit value from meta-memory.
//
// Fetch the values with byte operations in order to
// support CPUs that don't allow, say, 16-bit fetches
// on odd boundaries or 32-bit fetches on odd 16-bit
// boundaries.

#define META_VALUE_32(p)					\
	((((uint32)((uint8*) p)[0]) << 24) |	\
	 (((uint32)((uint8*) p)[1]) << 16) |	\
	 (((uint32)((uint8*) p)[2]) <<  8) |	\
	 (((uint32)((uint8*) p)[3])))

#define META_VALUE_16(p)					\
	((((uint16)((uint8*) p)[0]) << 8) |		\
	 (((uint16)((uint8*) p)[1])))

#define META_VALUE_8(p)						\
	(*(uint8*) p)

// Macros to define CanAppGetLong, CanAppSetLong,
// CanSystemGetLong, CanSystemSetLong, etc.

#define DEFINE_FUNCTIONS(kind, bits)							\
																\
inline Bool MetaMemory::Can##kind##Long (uint8* metaAddress)	\
{																\
	const uint32	kMask = META_BITS_32 (bits);				\
																\
	return (META_VALUE_32 (metaAddress) & kMask) == 0;			\
}																\
																\
inline Bool MetaMemory::Can##kind##Word (uint8* metaAddress)	\
{																\
	const uint16	kMask = META_BITS_16 (bits);				\
																\
	return (META_VALUE_16 (metaAddress) & kMask) == 0;			\
}																\
																\
inline Bool MetaMemory::Can##kind##Byte (uint8* metaAddress)	\
{																\
	const uint8		kMask = META_BITS_8 (bits);					\
																\
	return (META_VALUE_8 (metaAddress) & kMask) == 0;			\
}


#if FOR_LATER
DEFINE_FUNCTIONS(AppGet, kNoAppAccess | kUninitialized)
DEFINE_FUNCTIONS(AppSet, kNoAppAccess)

DEFINE_FUNCTIONS(SystemGet, kNoSystemAccess | kUninitialized)
DEFINE_FUNCTIONS(SystemSet, kNoSystemAccess)

DEFINE_FUNCTIONS(MemMgrGet, kNoMemMgrAccess | kUninitialized)
DEFINE_FUNCTIONS(MemMgrSet, kNoMemMgrAccess)
#else
DEFINE_FUNCTIONS(AppGet, kNoAppAccess)
DEFINE_FUNCTIONS(AppSet, kNoAppAccess)

DEFINE_FUNCTIONS(SystemGet, kNoSystemAccess)
DEFINE_FUNCTIONS(SystemSet, kNoSystemAccess)

DEFINE_FUNCTIONS(MemMgrGet, kNoMemMgrAccess)
DEFINE_FUNCTIONS(MemMgrSet, kNoMemMgrAccess)
#endif


#if FOR_LATER
inline void MetaMemory::MarkLongInitialized (emuptr p)
{
	const uint32	kMask	=	META_BITS_32 (kUninitialized);

	META_VALUE_32 (p) &= ~kMask;
}
#endif

#if FOR_LATER
inline void MetaMemory::MarkWordInitialized (emuptr p)
{
	const uint16	kMask	=	META_BITS_16 (kUninitialized);

	META_VALUE_16 (p) &= ~kMask;
}
#endif

#if FOR_LATER
inline void MetaMemory::MarkByteInitialized (emuptr p)
{
	const uint8	kMask	=	META_BITS_8 (kUninitialized);

	META_VALUE_8 (p) &= ~kMask;
}
#endif


inline Bool MetaMemory::IsScreenBuffer (uint8* metaAddress, uint32 size)
{
	if (size == 1)
	{
		const uint8 kMask = META_BITS_8 (kScreenBuffer);

		return (META_VALUE_8 (metaAddress) & kMask) != 0;
	}
	else if (size == 2)
	{
		const uint16 kMask = META_BITS_16 (kScreenBuffer);

		return (META_VALUE_16 (metaAddress) & kMask) != 0;
	}
	else if (size == 4)
	{
		const uint32 kMask = META_BITS_32 (kScreenBuffer);

		return (META_VALUE_32 (metaAddress) & kMask) != 0;
	}

	const uint8 kMask = META_BITS_8 (kScreenBuffer);

	for (uint32 ii = 0; ii < size; ++ii)
	{
		if (((*(uint8*) (metaAddress + ii)) & kMask) != 0)
		{
			return true;
		}
	}

	return false;
}

inline Bool MetaMemory::IsScreenBuffer8 (uint8* metaAddress)
{
	const uint8 kMask = META_BITS_8 (kScreenBuffer);

	return (META_VALUE_8 (metaAddress) & kMask) != 0;
}

inline Bool MetaMemory::IsScreenBuffer16 (uint8* metaAddress)
{
	const uint16 kMask = META_BITS_16 (kScreenBuffer);

	return (META_VALUE_16 (metaAddress) & kMask) != 0;
}

inline Bool MetaMemory::IsScreenBuffer32 (uint8* metaAddress)
{
	const uint32 kMask = META_BITS_32 (kScreenBuffer);

	return (META_VALUE_32 (metaAddress) & kMask) != 0;
}


inline Bool MetaMemory::IsCPUBreak (emuptr opcodeLocation)
{
	EmAssert ((opcodeLocation & 1) == 0);
	return IsCPUBreak (EmMemGetMetaAddress (opcodeLocation));
}


inline Bool MetaMemory::IsCPUBreak (uint8* metaLocation)
{
	return ((*metaLocation) & kInstructionBreak) != 0;
}


#define META_CHECK(metaAddress, address, op, size, forRead)		\
do {															\
	if (Memory::IsPCInRAM ())									\
	{															\
		if (!MetaMemory::CanApp##op (metaAddress))				\
		{														\
			if (!MetaMemory::InRAMOSComponent (gCPU->GetPC ()))	\
			{													\
				ProbableCause (address, sizeof (size), forRead);\
			}													\
			else if (!MetaMemory::CanSystem##op (metaAddress))	\
			{													\
				ProbableCause (address, sizeof (size), forRead);\
			}													\
		}														\
	}															\
	else if (EmPatchState::IsPCInMemMgr ())						\
	{															\
		if (!MetaMemory::CanMemMgr##op (metaAddress))			\
		{														\
			ProbableCause (address, sizeof (size), forRead);	\
		}														\
	}															\
	else														\
	{															\
		if (!MetaMemory::CanSystem##op (metaAddress))			\
		{														\
			ProbableCause (address, sizeof (size), forRead);	\
		}														\
	}															\
} while (0)


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkTotalAccess
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkTotalAccess (emuptr begin, emuptr end)
{
	UnmarkRange (begin, end, kAccessBitMask);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::SetAccess
// ---------------------------------------------------------------------------

inline void MetaMemory::SetAccess (emuptr begin, emuptr end, uint8 bits)
{
	MarkUnmarkRange (begin, end, ~kAccessBitMask, bits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkLowMemory
// ---------------------------------------------------------------------------
//	Mark the "low memory" range of memory (the first 256 bytes of
//	memory that hold exception vectors).

inline void MetaMemory::MarkLowMemory (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kLowMemoryBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkSystemGlobals
// ---------------------------------------------------------------------------
//	Mark the "system globals" range of memory.  This range holds the
//	global variables used by the Palm OS, as well as the jump table.

inline void MetaMemory::MarkSystemGlobals (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kGlobalsBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkHeapHeader
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkHeapHeader (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kMemStructBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkMPT
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkMPT (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kMPTBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkChunkHeader
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkChunkHeader (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kMemStructBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkChunkTrailer
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkChunkTrailer (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kMemStructBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkLowStack
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkLowStack (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kLowStackBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkUnlockedChunk
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkUnlockedChunk (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kUnlockedChunkBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkFreeChunk
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkFreeChunk (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kFreeChunkBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkScreen
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkScreen (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kScreenBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::UnmarkScreen
// ---------------------------------------------------------------------------

inline void MetaMemory::UnmarkScreen (emuptr begin, emuptr end)
{
	MarkTotalAccess (begin, end);
	UnmarkRange (begin, end, kScreenBuffer);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkUIObject
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkUIObject (emuptr begin, emuptr end)
{
	SetAccess (begin, end, kUIObjectBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::UnmarkUIObject
// ---------------------------------------------------------------------------

inline void MetaMemory::UnmarkUIObject (emuptr begin, emuptr end)
{
	MarkTotalAccess (begin, end);
//	UnmarkRange (begin, end, kUIObjectBits);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkInstructionBreak
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkInstructionBreak (emuptr opcodeLocation)
{
	EmAssert ((opcodeLocation & 1) == 0);

	uint8*	ptr = EmMemGetMetaAddress (opcodeLocation);

	*ptr |= kInstructionBreak;
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::UnmarkInstructionBreak
// ---------------------------------------------------------------------------

inline void MetaMemory::UnmarkInstructionBreak (emuptr opcodeLocation)
{
	EmAssert ((opcodeLocation & 1) == 0);

	uint8*	ptr = EmMemGetMetaAddress (opcodeLocation);

	*ptr &= ~kInstructionBreak;
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::MarkDataBreak
// ---------------------------------------------------------------------------

inline void MetaMemory::MarkDataBreak (emuptr begin, emuptr end)
{
	MarkRange (begin, end, kDataBreak);
}


// ---------------------------------------------------------------------------
//		¥ MetaMemory::UnmarkDataBreak
// ---------------------------------------------------------------------------

inline void MetaMemory::UnmarkDataBreak (emuptr begin, emuptr end)
{
	UnmarkRange (begin, end, kDataBreak);
}


#endif /* _METAMEMORY_H_ */
