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

#ifndef EmPalmHeap_h
#define EmPalmHeap_h

#include <vector>

class SessionFile;

class EmPalmChunk;
class EmPalmMPT;
class EmPalmHeap;

typedef vector<EmPalmChunk>		EmPalmChunkList;
typedef vector<EmPalmMPT>		EmPalmMPTList;
typedef vector<EmPalmHeap>		EmPalmHeapList;

class EmStream;
EmStream& operator << (EmStream&, const EmPalmChunk&);
EmStream& operator >> (EmStream&, EmPalmChunk&);

EmStream& operator << (EmStream&, const EmPalmMPT&);
EmStream& operator >> (EmStream&, EmPalmMPT&);

EmStream& operator << (EmStream&, const EmPalmHeap&);
EmStream& operator >> (EmStream&, EmPalmHeap&);


class EmPalmHeap
{
// ===== Static member functions =====

	public:

		static void				Initialize				(void);
		static void				Reset					(void);
		static void				Save					(SessionFile&);
		static void				Load					(SessionFile&);
		static void				Dispose					(void);

		static EmPalmHeapList::const_iterator
								GetHeapListBegin		(void);
		static EmPalmHeapList::const_iterator
								GetHeapListEnd			(void);

		static const EmPalmHeap*
								GetHeapByID				(UInt16);
		static const EmPalmHeap*
								GetHeapByPtr			(emuptr);
		static const EmPalmHeap*
								GetHeapByPtr			(MemPtr);
		static const EmPalmHeap*
								GetHeapByHdl			(MemHandle);

		static void				MemInitHeapTable		(UInt16 cardNo);
		static void				MemHeapInit				(UInt16 heapID,
														 Int16 numHandles,
														 Boolean initContents);
		static void				MemHeapCompact			(UInt16 heapID,
														 EmPalmChunkList* = NULL);
		static void				MemHeapFreeByOwnerID	(UInt16 heapID,
														 UInt16 ownerID,
														 EmPalmChunkList* = NULL);
		static void				MemHeapScramble			(UInt16 heapID,
														 EmPalmChunkList* = NULL);

		static void				MemChunkNew				(UInt16 heapID,
														 MemPtr,
														 UInt16 attributes,
														 EmPalmChunkList* = NULL);
		static void				MemChunkFree			(EmPalmHeap* heap,
														 EmPalmChunkList* = NULL);

		static void				MemPtrNew				(MemPtr,
														 EmPalmChunkList* = NULL);
		static void				MemPtrResize			(MemPtr,
														 EmPalmChunkList* = NULL);

		static void				MemHandleNew			(MemHandle,
														 EmPalmChunkList* = NULL);
		static void				MemHandleResize			(MemHandle,
														 EmPalmChunkList* = NULL);
		static void				MemHandleFree			(EmPalmHeap* heap,
														 EmPalmChunkList* = NULL);

		static void				MemLocalIDToLockedPtr	(MemPtr,
														 EmPalmChunkList* = NULL);
		static void				MemHandleLock			(MemHandle,
														 EmPalmChunkList* = NULL);
		static void				MemHandleUnlock			(MemHandle,
														 EmPalmChunkList* = NULL);
		static void				MemHandleResetLock		(MemHandle,
														 EmPalmChunkList* = NULL);
		static void				MemPtrResetLock			(MemPtr,
														 EmPalmChunkList* = NULL);
		static void				MemPtrUnlock			(MemPtr,
														 EmPalmChunkList* = NULL);
		static void				MemPtrSetOwner			(MemPtr,
														 EmPalmChunkList* = NULL);

		static void				ValidateAllHeaps		(void);

		static MemPtr			DerefHandle				(MemHandle h);
		static MemHandle		RecoverHandle			(MemPtr p);

	private:
		static void				AddHeap					(UInt16 heapID);
		static long				GetHeapVersion			(emuptr	heapHdr);

	private:
	
		static EmPalmHeapList	fgHeapList;


// ===== Object member functions =====

	public:

								EmPalmHeap				(void);
								EmPalmHeap				(UInt16 heapID);
								EmPalmHeap				(emuptr heapHdr);
								EmPalmHeap				(const EmPalmHeap&);
								~EmPalmHeap				(void);

		EmPalmChunkList::const_iterator
								GetChunkListBegin		(void) const;
		EmPalmChunkList::const_iterator
								GetChunkListEnd			(void) const;

		const EmPalmChunk*		GetChunkReferencedBy	(MemHandle) const;
		const EmPalmChunk*		GetChunkContaining		(emuptr) const;
		const EmPalmChunk*		GetChunkBodyContaining	(emuptr) const;

		EmPalmMPTList::const_iterator
								GetMPTListBegin			(void) const;
		EmPalmMPTList::const_iterator
								GetMPTListEnd			(void) const;

		Bool					Tracked					(void) const;
		Bool					ReadOnly				(void) const;
		Bool					Dynamic					(void) const;

		emuptr					Start					(void) const		{ return fHeapHdrStart; }
		emuptr					End						(void) const		{ return fHeapHdrStart + fSize; }
		uint32					Size					(void) const		{ return End () - Start (); }
		Bool					Contains				(emuptr p) const	{ return ((p >= Start ()) && (p < End ())); }

		emuptr					HeaderStart				(void) const		{ return fHeapHdrStart; }
		emuptr					HeaderEnd				(void) const		{ return fHeapHdrStart + fHeapHdrSize; }
		uint32					HeaderSize				(void) const		{ return HeaderEnd () - HeaderStart (); }
		Bool					HeaderContains			(emuptr p) const	{ return ((p >= HeaderStart ()) && (p < HeaderEnd ())); }

		emuptr					MptStart				(void) const		{ return fHeapHdrStart + fHeapHdrSize; }
		emuptr					MptEnd					(void) const		{ return fFirstChunk; }
		uint32					MptSize					(void) const		{ return MptEnd () - MptStart (); }
		Bool					MptContains				(emuptr p) const	{ return ((p >= MptStart ()) && (p < MptEnd ())); }

		emuptr					DataStart				(void) const		{ return fFirstChunk; }
		emuptr					DataEnd					(void) const		{ return fHeapHdrStart + fSize; }
		uint32					DataSize				(void) const		{ return DataEnd () - DataStart (); }
		Bool					DataContains			(emuptr p) const	{ return ((p >= DataStart ()) && (p < DataEnd ())); }

		enum
		{
			kVersionUnknown,
			kVersion1,	// 1 = original
			kVersion2,	// 2 = >64K
			kVersion3,	// 3 = has free chunk list
			kVersion4	// 4 = has free master pointer list
		};

		long					Version					(void) const		{ return fVersion; }
		long					HeapID					(void) const		{ return fHeapID; }
		long					ChunkHeaderSize			(void) const		{ return fChunkHdrSize; }

		void					Validate				(void);

	private:

		void					GetHeapHeaderInfo		(UInt16	heapID);
		void					GetHeapHeaderInfo		(emuptr	heapHdr);

		void					ResyncAll				(EmPalmChunkList* delta);
		void					ResyncMPTList			(void);
		void					ResyncChunkList			(EmPalmChunkList* delta);
		void					ResyncPtr				(MemPtr,
														 EmPalmChunkList* delta);
		void					ResyncHdl				(MemHandle,
														 EmPalmChunkList* delta);

		void					GenerateDeltas			(const EmPalmChunkList& oldList,
														 const EmPalmChunkList& newList,
														 EmPalmChunkList& delta);


	private:
		friend EmStream& operator << (EmStream&, const EmPalmHeap&);
		friend EmStream& operator >> (EmStream&, EmPalmHeap&);

		long					fVersion;
		long					fHeapID;

		emuptr					fHeapHdrStart;
		uint32					fHeapHdrSize;		// Up to the master pointer table struct
		emuptr					fFirstChunk;		// Just after the end of the master pointer table

		UInt16					fFlags;
		UInt32					fSize;
		UInt32					fFirstFree;
		UInt16					fChunkHdrSize;

		EmPalmChunkList			fChunkList;
		EmPalmMPTList			fMPTList;
};


// Handy macros for dealing with the rather unwieldy iteration functions.

#define ITERATE_HEAPS(iter1, iter2)	\
		EmPalmHeapList::const_iterator	iter1 = EmPalmHeap::GetHeapListBegin ();	\
		EmPalmHeapList::const_iterator	iter2 = EmPalmHeap::GetHeapListEnd ();		\
		while (iter1 != iter2)

#define ITERATE_MPTS(heap, iter1, iter2)	\
		EmPalmMPTList::const_iterator	iter1 = (heap).GetMPTListBegin ();		\
		EmPalmMPTList::const_iterator	iter2 = (heap).GetMPTListEnd ();		\
		while (iter1 != iter2)

#define ITERATE_CHUNKS(heap, iter1, iter2)	\
		EmPalmChunkList::const_iterator	iter1 = (heap).GetChunkListBegin ();	\
		EmPalmChunkList::const_iterator	iter2 = (heap).GetChunkListEnd ();		\
		while (iter1 != iter2)


class EmPalmMPT
{
	public:
								EmPalmMPT				(void);
								EmPalmMPT				(const EmPalmHeap&,
														 emuptr mptHdr);
								EmPalmMPT				(const EmPalmMPT&);
								~EmPalmMPT				(void);

		void					Validate				(const EmPalmHeap& heap) const;

		emuptr					Start					(void) const		{ return fMptHdrStart; }
		emuptr					End						(void) const		{ return fMptHdrStart + fSize; }
		uint32					Size					(void) const		{ return End () - Start (); }
		Bool					Contains				(emuptr p) const	{ return ((p >= Start ()) && (p < End ())); }

		emuptr					HeaderStart				(void) const		{ return fMptHdrStart; }
		emuptr					HeaderEnd				(void) const		{ return fMptHdrStart + fMptHdrSize; }
		uint32					HeaderSize				(void) const		{ return HeaderEnd () - HeaderStart (); }
		Bool					HeaderContains			(emuptr p) const	{ return ((p >= HeaderStart ()) && (p < HeaderEnd ())); }

		emuptr					TableStart				(void) const		{ return fMptHdrStart + fMptHdrSize; }
		emuptr					TableEnd				(void) const		{ return fMptHdrStart + fSize; }
		uint32					TableSize				(void) const		{ return TableEnd () - TableStart (); }
		Bool					TableContains			(emuptr p) const	{ return ((p >= TableStart ()) && (p < TableEnd ())); }

		enum
		{
			kVersionUnknown,
			kVersion1,	// 1 = original
			kVersion2	// 2 = >64K
		};

		long					Version					(void) const		{ return fVersion; }
		UInt32					NextTableOffset			(void) const		{ return fNextTblOffset; }

	private:
		void					GetMPTInfo				(const EmPalmHeap&,
														 emuptr mptHdr);

	private:
		friend EmStream& operator << (EmStream&, const EmPalmMPT&);
		friend EmStream& operator >> (EmStream&, EmPalmMPT&);

		long		fVersion;

		emuptr		fMptHdrStart;
		uint32		fMptHdrSize;			// Up to the array of master pointers
		uint32		fSize;					// Entire size

		UInt16		fNumEntries;
		UInt32		fNextTblOffset;
};


class EmPalmChunk
{
	public:
								EmPalmChunk				(void);
								EmPalmChunk				(const EmPalmHeap&,
														 emuptr chunkHdr);
								EmPalmChunk				(const EmPalmChunk&);
								~EmPalmChunk			(void);

		void					Validate				(const EmPalmHeap& heap) const;
		Bool					CompareForDelta			(const EmPalmChunk& lhs) const;

		emuptr					Start					(void) const		{ return fChunkHdrStart; }
		emuptr					End						(void) const		{ return fChunkHdrStart + fSize; }
		uint32					Size					(void) const		{ return End () - Start (); }
		Bool					Contains				(emuptr p) const	{ return ((p >= Start ()) && (p < End ())); }

		emuptr					HeaderStart				(void) const		{ return fChunkHdrStart; }
		emuptr					HeaderEnd				(void) const		{ return fChunkHdrStart + fChunkHdrSize; }
		uint32					HeaderSize				(void) const		{ return HeaderEnd () - HeaderStart (); }
		Bool					HeaderContains			(emuptr p) const	{ return ((p >= HeaderStart ()) && (p < HeaderEnd ())); }

		emuptr					BodyStart				(void) const		{ return fChunkHdrStart + fChunkHdrSize; }
		emuptr					BodyEnd					(void) const		{ return fChunkHdrStart + fSize - fSizeAdj; }
		uint32					BodySize				(void) const		{ return BodyEnd () - BodyStart (); }
		Bool					BodyContains			(emuptr p) const	{ return ((p >= BodyStart ()) && (p < BodyEnd ())); }

		emuptr					TrailerStart			(void) const		{ return fChunkHdrStart + fSize - fSizeAdj; }
		emuptr					TrailerEnd				(void) const		{ return fChunkHdrStart + fSize; }
		uint32					TrailerSize				(void) const		{ return TrailerEnd () - TrailerStart (); }
		Bool					TrailerContains			(emuptr p) const	{ return ((p >= TrailerStart ()) && (p < TrailerEnd ())); }

		enum
		{
			kVersionUnknown,
			kVersion1,	// 1 = original
			kVersion2	// 2 = >64K
		};

		long					Version					(void) const		{ return fVersion; }
		Bool					Free					(void) const		{ return fFree; }
		uint8					LockCount				(void) const		{ return fLockCount; }
		uint8					Owner					(void) const		{ return fOwner; }
		int32					HOffset					(void) const		{ return fHOffset; }

	private:
		void					GetChunkInfo			(const EmPalmHeap&,
														 emuptr chunkHdr);

	private:
		friend EmStream& operator << (EmStream&, const EmPalmChunk&);
		friend EmStream& operator >> (EmStream&, EmPalmChunk&);

		long		fVersion;

		emuptr		fChunkHdrStart;
		uint32		fChunkHdrSize;

		bool		fFree;				// set if free chunk
		bool		fMoved;				// used by MemHeapScramble
		bool		fUnused2;			// unused
		bool		fUnused3;			// unused
		uint8		fSizeAdj;			// size adjustment
		uint32		fSize;				// actual size of chunk

		uint8		fLockCount;			// lock count
		uint8		fOwner;				// owner ID
		int32		fHOffset;			// signed handle offset/2
										// used in free chunks to point to next free chunk

};

#endif /* EmPalmHeap_h */
