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

#include "EmCommon.h"
#include "EmPalmHeap.h"

#include "ChunkFile.h"			// Chunk, EmStreamChunk
#include "EmErrCodes.h"			// kError_CorruptedHeap_Foo
#include "EmMemory.h"			// CEnableFullAccess, EmMemGet32, EmMemGet16, EmMemGet8
#include "ErrorHandling.h"		// Errors::ReportErrCorruptedHeap
#include "ROMStubs.h"			// MemNumHeaps, MemHeapID, MemHeapPtr
#include "SessionFile.h"		// SessionFile

#include <stdio.h>				// sprintf
#include <cstddef>


// ===========================================================================
//		¥ EmPalmHeap
// ===========================================================================

EmPalmHeapList	EmPalmHeap::fgHeapList;


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::Initialize	[ STATIC ]
 *
 * DESCRIPTION: Standard initialization function.  Responsible for
 *				initializing this sub-system when a new session is
 *				created.  Will be followed by at least one call to
 *				Reset or Load.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::Initialize (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::Reset	[ STATIC ]
 *
 * DESCRIPTION:	Re-initializes the PalmHeap sub-system.  This function
 *				is called when the ROM is "rebooted" (which includes
 *				the initial boot-up after a ROM is loaded).
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::Reset (void)
{
	fgHeapList.clear ();
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::Save	[ STATIC ]
 *
 * DESCRIPTION:	Standard save function.  Saves any sub-system state to
 *				the given session file.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::Save (SessionFile& f)
{
	const long	kCurrentVersion = 1;

	Chunk			chunk;
	EmStreamChunk	s (chunk);

	s << kCurrentVersion;

	s << fgHeapList;

	f.WriteHeapInfo (chunk);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::Load	[ STATIC ]
 *
 * DESCRIPTION:	Standard load function.  Loads any sub-system state
 *				from the given session file.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::Load (SessionFile& f)
{
	Chunk	chunk;
	if (f.ReadHeapInfo (chunk))
	{
		long			version;
		EmStreamChunk	s (chunk);

		s >> version;

		if (version >= 1)
		{
			s >> fgHeapList;
		}
	}
	else
	{
		f.SetCanReload (false);	// Need to reboot
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::Dispose	[ STATIC ]
 *
 * DESCRIPTION:	Standard dispose function.  Completely release any
 *				resources acquired or allocated in Initialize and/or
 *				Load.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::Dispose (void)
{
	fgHeapList.clear ();
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetHeapListBegin	[ STATIC ]
 *
 * DESCRIPTION:	Return an iterator representing the beginning of our
 *				list of heap objects.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The iterator.
 *
 ***********************************************************************/

EmPalmHeapList::const_iterator EmPalmHeap::GetHeapListBegin (void)
{
	return fgHeapList.begin ();
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetHeapListEnd	[ STATIC ]
 *
 * DESCRIPTION:	Return an iterator representing the end of our
 *				list of heap objects.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The iterator.
 *
 ***********************************************************************/

EmPalmHeapList::const_iterator EmPalmHeap::GetHeapListEnd (void)
{
	return fgHeapList.end ();
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetHeapByID	[ STATIC ]
 *
 * DESCRIPTION:	Find a heap based on its ID.
 *
 * PARAMETERS:	heapID - the ID to test.
 *
 * RETURNED:	The pointer to the heap object.  If no heap object
 *				has that ID, NULL is returned.
 *
 ***********************************************************************/

const EmPalmHeap* EmPalmHeap::GetHeapByID (UInt16 heapID)
{
	EmPalmHeapList::iterator	iter = fgHeapList.begin ();

	while (iter != fgHeapList.end ())
	{
		if (iter->fHeapID == heapID)
		{
			return &*iter;
		}

		++iter;
	}

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetHeapByPtr	[ STATIC ]
 *
 * DESCRIPTION:	Find a heap based on a pointer.
 *
 * PARAMETERS:	p - the pointer to test.
 *
 * RETURNED:	The pointer to the heap object.  If no heap object
 *				contains the pointer, NULL is returned.
 *
 ***********************************************************************/

const EmPalmHeap* EmPalmHeap::GetHeapByPtr (emuptr p)
{
	return GetHeapByPtr ((MemPtr) p);
}

const EmPalmHeap* EmPalmHeap::GetHeapByPtr (MemPtr p)
{
	// !!! Could do a binary search here, if needed.

	EmPalmHeapList::iterator	iter = fgHeapList.begin ();

	while (iter != fgHeapList.end ())
	{
		if (iter->Contains ((emuptr) p))
		{
			return &*iter;
		}

		++iter;
	}

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetHeapByHdl	[ STATIC ]
 *
 * DESCRIPTION:	Find a heap based on a handle.
 *
 * PARAMETERS:	h - the handle to test.
 *
 * RETURNED:	The pointer to the heap object.  If no heap object
 *				contains the handle, NULL is returned.
 *
 *	!!! What to do when the handle and pointer aren't in the same heap?
 *
 ***********************************************************************/

const EmPalmHeap* EmPalmHeap::GetHeapByHdl (MemHandle h)
{
	MemPtr	p = DerefHandle (h);

	if (p)
		return GetHeapByPtr (p);

	return NULL;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::MemInitHeapTable	[ STATIC ]
 *
 * DESCRIPTION:	All new heaps have been created.  Create objects to
 *				represent those heaps and add them to our list of heaps.
 *
 * PARAMETERS:	Parameters passed to MemInitHeapTable
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::MemInitHeapTable (UInt16 cardNo)
{
	UInt16	numHeaps = ::MemNumHeaps (cardNo);

	for (UInt16 heapIndex = 0; heapIndex < numHeaps; ++heapIndex)
	{
		UInt16	heapID = ::MemHeapID (cardNo, heapIndex);

		// Add the heap, as long as it's not the dynamic heap.  During
		// bootup, the memory initialization sequence goes like:
		//
		//	if hard reset required:
		//		MemCardFormat
		//			lay out the card
		//			MemStoreInit
		//				for each heap
		//					MemHeapInit
		//	MemInit
		//		for each card:
		//			MemInitHeapTable
		//		for each dynamic heap:
		//			MemHeapInit
		//		for each RAM heap:
		//			Unlock all chunks
		//			Compact
		//
		// Which means that if there's no hard reset, MemHeapInit
		// has not been called on the dynamic heap at the time
		// MemInitHeapTable is called.  And since the dynamic heap
		// is currently in a corrupted state (because the boot stack
		// and initial LCD buffer have been whapped over it), we
		// can't perform the heap walk we'd normally do when adding
		// a heap object.

		if (heapID != 0)
			AddHeap (heapID);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::MemHeapInit	[ STATIC ]
 *
 * DESCRIPTION:	A new heap has been created.  Create an object to
 *				represent that heap and add it to our list of heaps.
 *
 * PARAMETERS:	Parameters passed to MemHeapInit
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::MemHeapInit (UInt16 heapID, Int16, Boolean)
{
	AddHeap (heapID);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::MemHeapCompact	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemHeapFreeByOwnerID	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemHeapScramble	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemChunkNew	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemChunkFree	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemPtrNew	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemPtrResize	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemHandleNew	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemHandleResize	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemHandleFree	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemLocalIDToLockedPtr	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemHandleLock	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemHandleUnlock	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemHandleResetLock	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemPtrResetLock	[ STATIC ]
 * FUNCTION:	EmPalmHeap::MemPtrUnlock	[ STATIC ]
 *
 * DESCRIPTION:	All of these functions alter the heap in some way.
 *				Resync our notion of the state of the heap with reality.
 *
 * PARAMETERS:	Parameters to the Memory Manager functions that altered
 *				the heap.
 *
 * RETURNED:	The chunks that cover the altered range of the heap
 *				are returned in the "delta" collection.
 *
 ***********************************************************************/

void EmPalmHeap::MemHeapCompact (UInt16 heapID, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByID (heapID));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemHeapFreeByOwnerID (UInt16 heapID, UInt16, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByID (heapID));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemHeapScramble (UInt16 heapID, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByID (heapID));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemChunkNew (UInt16 heapID, MemPtr p, UInt16 attr, EmPalmChunkList* delta)
{
	UNUSED_PARAM (p);
	UNUSED_PARAM (attr);

	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByID (heapID));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemChunkFree (EmPalmHeap* heap, EmPalmChunkList* delta)
{
//	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByPtr (p));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemPtrNew (MemPtr p, EmPalmChunkList* delta)
{
	UNUSED_PARAM (p);
//	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByPtr (p));
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByID (0));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemPtrResize (MemPtr p, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByPtr (p));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemHandleNew (MemHandle h, EmPalmChunkList* delta)
{
	UNUSED_PARAM (h);
//	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByHdl (h));
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByID (0));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemHandleResize (MemHandle h, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByHdl (h));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemHandleFree (EmPalmHeap* heap, EmPalmChunkList* delta)
{
//	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByHdl (h));

	if (heap)
		heap->ResyncAll (delta);
}

void EmPalmHeap::MemLocalIDToLockedPtr (MemPtr p, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByPtr (p));

	if (heap)
		heap->ResyncPtr (p, delta);
}

void EmPalmHeap::MemHandleLock (MemHandle h, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByHdl (h));

	if (heap)
		heap->ResyncHdl (h, delta);
}

void EmPalmHeap::MemHandleUnlock (MemHandle h, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByHdl (h));

	if (heap)
		heap->ResyncHdl (h, delta);
}

void EmPalmHeap::MemHandleResetLock (MemHandle h, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByHdl (h));

	if (heap)
		heap->ResyncHdl (h, delta);
}

void EmPalmHeap::MemPtrResetLock (MemPtr p, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByPtr (p));

	if (heap)
		heap->ResyncPtr (p, delta);
}

void EmPalmHeap::MemPtrUnlock (MemPtr p, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByPtr (p));

	if (heap)
		heap->ResyncPtr (p, delta);
}

void EmPalmHeap::MemPtrSetOwner (MemPtr p, EmPalmChunkList* delta)
{
	EmPalmHeap*	heap = const_cast <EmPalmHeap*> (GetHeapByPtr (p));

	if (heap)
		heap->ResyncPtr (p, delta);
}

void EmPalmHeap::ValidateAllHeaps (void)
{
	EmPalmHeapList::iterator	iter = fgHeapList.begin ();

	while (iter != fgHeapList.end ())
	{
		(*iter).Validate ();
		++iter;
	}
}


#pragma mark -


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::AddHeap	[ STATIC ]
 *
 * DESCRIPTION:	Add a heap based on its ID.
 *
 * PARAMETERS:	heapID - the ID of the heap to add.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmPalmHeap::AddHeap (UInt16 heapID)
{
	// Get information on the newly-created heap.

	EmPalmHeap	heap (heapID);

	// Find where to add this heap to our list of heaps.  We keep this
	// list sorted by heap location for easy lookup on that basis.

	EmPalmHeapList::iterator	iter = fgHeapList.begin ();

	while (iter != fgHeapList.end ())
	{
		// If we already had an entry for this heap, replace it.

		if (iter->Start () == heap.Start ())
		{
			*iter = heap;
			break;
		}

		// Found a place to insert this heap.

		if (iter->Start () > heap.Start ())
		{
			fgHeapList.insert (iter, heap);
			break;
		}

		++iter;
	}

	// If the new heap couldn't be inserted before a currently existing
	// heap, then add it to the end.

	if (iter == fgHeapList.end ())
	{
		fgHeapList.push_back (heap);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::DerefHandle	[ STATIC ]
 *
 * DESCRIPTION:	Dereferences a handle and returns the pointer
 *
 * PARAMETERS:	h - the handle to dereference
 *
 * RETURNED:	The pointer associated with the handle.
 *
 ***********************************************************************/

MemPtr EmPalmHeap::DerefHandle (MemHandle h)
{
	if (!h)
		return NULL;

	CEnableFullAccess	munge;	// Remove blocks on memory access.

	emuptr	pp = (emuptr) memHandleUnProtect(h);
	return (MemPtr) EmMemGet32 (pp);
}

/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::RecoverHandle	[ STATIC ]
 *
 * DESCRIPTION:	Finds a handle given a pointer.
 *
 *				Note that MemPtrRecoverHandle calls memHandleProtect on
 *				the result even if it's NULL; we don't do that here.
 *
 * PARAMETERS:	p - the pointer whose handle we want to find
 *
 * RETURNED:	The handle associated with the pointer.  NULL if the
 *				chunk was not allocated as a relocatable chunk or if
 *				it's a free chunk.
 *
 ***********************************************************************/

MemHandle EmPalmHeap::RecoverHandle (MemPtr p)
{
	MemHandle	h = NULL;

	if (p)
	{
		const EmPalmHeap*	heap = EmPalmHeap::GetHeapByPtr (p);

		if (heap)
		{
			const EmPalmChunk*	chunk = heap->GetChunkBodyContaining ((emuptr) p);

			if (chunk && !chunk->Free () && chunk->HOffset ())
			{
				h = (MemHandle) (((emuptr) p) - chunk->HOffset () * 2 - chunk->HeaderSize ());

#if _DEBUG
				CEnableFullAccess	munge;	// Remove blocks on memory access.
				EmAssert (EmMemGet32 ((emuptr) h) == (emuptr) p);
#endif

				h = memHandleProtect (h);
			}
		}
	}

	return h;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetHeapVersion	[ STATIC ]
 *
 * DESCRIPTION:	Determine and return the version number of the given
 *				heap.
 *
 * PARAMETERS:	heapPtr - a pointer to the header for the heap whose
 *					version is to be determined.
 *
 * RETURNED:	The version number of the heap.  Currently, these values
 *				are 1 (original heap format), 2 (allows for >64K heaps),
 *				and 3 (uses a free list for faster allocations).
 *
 ***********************************************************************/

long EmPalmHeap::GetHeapVersion (emuptr heapHdr)
{
	CEnableFullAccess	munge;	// Remove blocks on memory access.

	long	heapVersion = kVersionUnknown;

	// Get the heap version.  If bit 14 is set, it's a version 3 heap.
	// If bit 15 is set, it's a version 2 heap.  Otherwise, it's a
	// version 1 heap.

	uint16	flags = EmMemGet16 (heapHdr);
	
	if ((flags & memHeapFlagVers4) != 0)
	{
		heapVersion = kVersion4;	// free MP's on a list
	}
	else if ((flags & memHeapFlagVers3) != 0)
	{
		heapVersion = kVersion3;	// has free list
	}
	else if ((flags & memHeapFlagVers2) != 0)
	{
		heapVersion = kVersion2;	// > 64K
	}
	else
	{
		heapVersion = kVersion1;	// original
	}

	return heapVersion;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmHeap::EmPalmHeap (void) :
	fVersion (kVersionUnknown),
	fHeapID (-1),
	fHeapHdrStart (0),
	fHeapHdrSize (0),
	fFirstChunk (0),
	fFlags (0),
	fSize (0),
	fFirstFree (0),
	fChunkHdrSize (0),
	fChunkList (),
	fMPTList ()
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	heapID - Palm OS ID for the heap we're to track.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmHeap::EmPalmHeap (UInt16 heapID) :
	fVersion (kVersionUnknown),
	fHeapID (heapID),
	fHeapHdrStart (0),
	fHeapHdrSize (0),
	fFirstChunk (0),
	fFlags (0),
	fSize (0),
	fFirstFree (0),
	fChunkHdrSize (0),
	fChunkList (),
	fMPTList ()
{
	this->GetHeapHeaderInfo (heapID);
	this->ResyncAll (NULL);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 *				Note: this version of the constructor doesn't work too
 *				well, since it doesn't determine the Palm OS heap ID.
 *
 * PARAMETERS:	heapHdr - pointer to the heap header of the heap we're
 *					to track.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmHeap::EmPalmHeap (emuptr heapHdr) :
	fVersion (kVersionUnknown),
	fHeapID (-1),
	fHeapHdrStart (0),
	fHeapHdrSize (0),
	fFirstChunk (0),
	fFlags (0),
	fSize (0),
	fFirstFree (0),
	fChunkHdrSize (0),
	fChunkList (),
	fMPTList ()
{
	this->GetHeapHeaderInfo (heapHdr);
	this->ResyncAll (NULL);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap copy constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	other - EmPalmHeap object we're to clone.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmHeap::EmPalmHeap (const EmPalmHeap& other) :
	fVersion (other.fVersion),
	fHeapID (other.fHeapID),
	fHeapHdrStart (other.fHeapHdrStart),
	fHeapHdrSize (other.fHeapHdrSize),
	fFirstChunk (other.fFirstChunk),
	fFlags (other.fFlags),
	fSize (other.fSize),
	fFirstFree (other.fFirstFree),
	fChunkHdrSize (other.fChunkHdrSize),
	fChunkList (other.fChunkList),
	fMPTList (other.fMPTList)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::~EmPalmHeap
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmHeap::~EmPalmHeap (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetChunkListBegin
 *
 * DESCRIPTION:	Return an iterator representing the beginning of our
 *				list of chunk objects.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The iterator.
 *
 ***********************************************************************/

EmPalmChunkList::const_iterator EmPalmHeap::GetChunkListBegin (void) const
{
	return fChunkList.begin ();
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetChunkListEnd
 *
 * DESCRIPTION:	Return an iterator representing the end of our
 *				list of chunk objects.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The iterator.
 *
 ***********************************************************************/

EmPalmChunkList::const_iterator EmPalmHeap::GetChunkListEnd (void) const
{
	return fChunkList.end ();
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetChunkReferencedBy
 *
 * DESCRIPTION:	Iterate over our list of chunks, finding the one
 *				referenced by the given handle.
 *
 * PARAMETERS:	h - test handle
 *
 * RETURNED:	Pointer to chunk object containing the probe address.
 *				NULL if not found.
 *
 ***********************************************************************/

const EmPalmChunk* EmPalmHeap::GetChunkReferencedBy (MemHandle h) const
{
	// !!! Could do a binary search here, if needed.

	emuptr	p = (emuptr) EmPalmHeap::DerefHandle (h);

	ITERATE_CHUNKS (*this, iter, end)
	{
		if (iter->Contains (p))
		{
			return &*iter;
		}

		++iter;
	}

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetChunkContaining
 *
 * DESCRIPTION:	Iterate over our list of chunks, finding the one
 *				containing the given pointer.  The range includes the
 *				chunk header and trailer.
 *
 * PARAMETERS:	p - probe address
 *
 * RETURNED:	Pointer to chunk object containing the probe address.
 *				NULL if not found.
 *
 ***********************************************************************/

const EmPalmChunk* EmPalmHeap::GetChunkContaining (emuptr p) const
{
	// !!! Could do a binary search here, if needed.

	ITERATE_CHUNKS (*this, iter, end)
	{
		if (iter->Contains (p))
		{
			return &*iter;
		}

		++iter;
	}

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetChunkBodyContaining
 *
 * DESCRIPTION:	Iterate over our list of chunks, finding the one
 *				containing the given pointer.  The range does not
 *				include the chunk header or trailer.
 *
 * PARAMETERS:	p - probe address
 *
 * RETURNED:	Pointer to chunk object containing the probe address.
 *				NULL if not found.
 *
 ***********************************************************************/

const EmPalmChunk* EmPalmHeap::GetChunkBodyContaining (emuptr p) const
{
	// !!! Could do a binary search here, if needed.

	ITERATE_CHUNKS (*this, iter, end)
	{
		if (iter->BodyContains (p))
		{
			return &*iter;
		}

		++iter;
	}

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetMPTListBegin
 *
 * DESCRIPTION:	Return an iterator representing the beginning of our
 *				list of master pointer table objects.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The iterator.
 *
 ***********************************************************************/

EmPalmMPTList::const_iterator EmPalmHeap::GetMPTListBegin (void) const
{
	return fMPTList.begin ();
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetMPTListEnd
 *
 * DESCRIPTION:	Return an iterator representing the end of our
 *				list of master pointer table objects.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The iterator.
 *
 ***********************************************************************/

EmPalmMPTList::const_iterator EmPalmHeap::GetMPTListEnd (void) const
{
	return fMPTList.end ();
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::Tracked
 *
 * DESCRIPTION:	Return whether or not we should track the contents of
 *				this heap.  For performance reasons, we currently
 *				track only the dynamic heap.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	True if we track this heap's contents.
 *
 ***********************************************************************/

Bool EmPalmHeap::Tracked (void) const
{
	return this->Dynamic ();
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::ReadOnly
 *
 * DESCRIPTION:	Return whether or not this heap is read-only
 *
 * PARAMETERS:	None
 *
 * RETURNED:	True if so.
 *
 ***********************************************************************/

Bool EmPalmHeap::ReadOnly (void) const
{
	return (fFlags & memHeapFlagReadOnly) != 0;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::Dynamic
 *
 * DESCRIPTION:	Return whether or not this heap is the (a?) dynamic heap.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	True if so.
 *
 ***********************************************************************/

Bool EmPalmHeap::Dynamic (void) const
{
	// This is probably the wrong test.  I think there's a Palm OS call
	// that indicates which heap(s) is/are dynamic.

	return fHeapID == 0;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::Validate
 *
 * DESCRIPTION:	Check to see that our chunk collection matches what's
 *				actually there.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmPalmHeap::Validate (void)
{
	if (!this->Tracked ())
		return;

	EmPalmChunkList	newList;
	emuptr			chunkHdr = this->DataStart ();

	while (1)
	{
		// Get information about the current chunk.

		EmPalmChunk		chunk (*this, chunkHdr);

		// If the size is zero, we've reached the sentinel at the end.

		if (chunk.Size () == 0)
		{
			break;
		}

		// See if this chunk looks valid.  An exception is thrown if not.

		chunk.Validate (*this);

		// Push this guy onto our list.

		newList.push_back (chunk);

		// Go on to next chunk.

		chunkHdr += chunk.Size ();
	}

	EmPalmChunkList	delta;
	this->GenerateDeltas (newList, fChunkList, delta);
	EmAssert (delta.size () == 0);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetHeapHeaderInfo
 *
 * DESCRIPTION:	Scarf information out of the heap header.
 *
 * PARAMETERS:	heapID - the heapID of the heap to query.
 *
 * RETURNED:	Nothing, but many data members are filled in.
 *
 ***********************************************************************/

void EmPalmHeap::GetHeapHeaderInfo (UInt16 heapID)
{
	MemPtr	heapHdr = MemHeapPtr (heapID);
	if (heapHdr)
		this->GetHeapHeaderInfo ((emuptr) heapHdr);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GetHeapHeaderInfo
 *
 * DESCRIPTION:	Scarf up information about the managed heap.
 *
 * PARAMETERS:	heapHdr - the pointer to the heap to query.
 *
 * RETURNED:	Nothing, but many data members are filled in.
 *
 ***********************************************************************/

void EmPalmHeap::GetHeapHeaderInfo (emuptr heapHdr)
{
	CEnableFullAccess	munge;	// Remove blocks on memory access.


	// Save the pointer to the beginning of the heap.  This pointer
	// points to a struct, the definition of which changes depending
	// on the heap version.

	fHeapHdrStart	= heapHdr;


	// Get the heap version so we know the layout of the header.

	fVersion = GetHeapVersion (fHeapHdrStart);


	/*
		Heap layout

			flags:					// Always here, always 2 bytes
			size					// 2 bytes in V1, 4 otherwise
			firstFreeChunkOffset	// Only in V3
			mstrPtrTbl
				numEntries			// always 2 bytes long
				nextTblOffset		// 2 bytes in V1, 4 otherwise
				mstrP [numEntries]
			...chunks...
	*/


	switch (fVersion)
	{
		case kVersion1:
		{
			fFlags			= EmMemGet16 (fHeapHdrStart);
			fSize			= EmMemGet16 (fHeapHdrStart + offsetof (Mem1HeapHeaderType, size));
			fFirstFree		= EmMemNULL;
			fChunkHdrSize	= sizeof (Mem1ChunkHeaderType);
			fHeapHdrSize	= offsetof (Mem1HeapHeaderType, mstrPtrTbl);

			EmPalmMPT		mpt (*this, this->MptStart ());
			fFirstChunk		= this->MptStart () + mpt.Size ();

			// In version 1 heaps, a zero in the size field means 64K.

			if (fSize == 0)
			{
				fSize = 64 * 1024L;
			}
			break;
		}

		case kVersion2:
		{
			fFlags			= EmMemGet16 (fHeapHdrStart);
			fSize			= EmMemGet32 (fHeapHdrStart + offsetof (Mem2HeapHeaderType, size));
			fFirstFree		= EmMemNULL;
			fChunkHdrSize	= sizeof (MemChunkHeaderType);
			fHeapHdrSize	= offsetof (Mem2HeapHeaderType, mstrPtrTbl);

			EmPalmMPT		mpt (*this, this->MptStart ());
			fFirstChunk		= this->MptStart () + mpt.Size ();
			break;
		}

		case kVersion3:
		case kVersion4:
		{
			fFlags			= EmMemGet16 (fHeapHdrStart);
			fSize			= EmMemGet32 (fHeapHdrStart + offsetof (MemHeapHeaderType, size));
			fFirstFree		= EmMemGet32 (fHeapHdrStart + offsetof (MemHeapHeaderType, firstFreeChunkOffset));
			fChunkHdrSize	= sizeof (MemChunkHeaderType);
			fHeapHdrSize	= offsetof (MemHeapHeaderType, mstrPtrTbl);

			EmPalmMPT		mpt (*this, this->MptStart ());
			fFirstChunk		= this->MptStart () + mpt.Size ();
			break;
		}

		default:
			EmAssert (false);
			break;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::ResyncAll
 *
 * DESCRIPTION:	Resynchronize our notion about the contents of the heap
 *				this object wraps up.
 *
 * PARAMETERS:	delta - optional collection to receive the list of
 *					chunks that are different between the current and
 *					previous states of the heap.  This collection is
 *					used when remarking what parts of the heap can be
 *					accessed by different processes.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::ResyncAll (EmPalmChunkList* delta)
{
	this->ResyncMPTList ();
	this->ResyncChunkList (delta);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::ResyncMPTList
 *
 * DESCRIPTION:	Resynchronize our notion of what master pointer tables
 *				exist in this heap.  Does nothing if this heap is
 *				not a "tracked" heap (see the Tracked method).
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::ResyncMPTList (void)
{
	if (!this->Tracked ())
		return;

	emuptr	p = this->MptStart ();

	fMPTList.clear ();

	while (1)
	{
		EmPalmMPT	mpt (*this, p);

		// See if this table looks valid.  An exception is thrown if not.

		mpt.Validate (*this);

		fMPTList.push_back (mpt);

		if (mpt.NextTableOffset () == 0)
			break;

		p = this->fHeapHdrStart + mpt.NextTableOffset ();
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::ResyncChunkList
 *
 * DESCRIPTION:	Resynchronize our notion of what memory chunks
 *				exist in this heap.  Does nothing if this heap is
 *				not a "tracked" heap (see the Tracked method).
 *
 * PARAMETERS:	delta - optional collection to receive the list of
 *					chunks that are different between the current and
 *					previous states of the heap.  This collection is
 *					used when remarking what parts of the heap can be
 *					accessed by different processes.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::ResyncChunkList (EmPalmChunkList* delta)
{
	if (!this->Tracked ())
		return;

	EmPalmChunkList	oldList;

	if (delta)
		oldList = fChunkList;

	emuptr			chunkHdr = this->DataStart ();

	fChunkList.clear ();

	while (1)
	{
		// Get information about the current chunk.

		EmPalmChunk		chunk (*this, chunkHdr);

		// If the size is zero, we've reached the sentinel at the end.

		if (chunk.Size () == 0)
		{
			break;
		}

		// See if this chunk looks valid.  An exception is thrown if not.

		chunk.Validate (*this);

		// Push this guy onto our list.

		fChunkList.push_back (chunk);

		// Go on to next chunk.

		chunkHdr += chunk.Size ();
	}

	if (delta)
		this->GenerateDeltas (oldList, fChunkList, *delta);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::ResyncPtr
 *
 * DESCRIPTION:	Resynchronize information about a single particular
 *				chunk.  This routine works by assuming that (a) the
 *				chunk already exists in the chunk list and that (b)
 *				only minor changes have been made to the chunk (like
 *				its lockcount having been changed).  This routine
 *				should not be called if the chunk has been moved or
 *				resized, since by implication that means that more
 *				than one chunk has changed.
 *
 *				Does nothing if the heap is not "tracked".
 *
 * PARAMETERS:	p - pointer to the body of the chunk that changed.
 *
 *				delta - optional collection to receive the list of
 *					chunks that are different between the current and
 *					previous states of the heap.  This collection is
 *					used when remarking what parts of the heap can be
 *					accessed by different processes.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::ResyncPtr (MemPtr p, EmPalmChunkList* delta)
{
	if (!this->Tracked ())
		return;

	EmPalmChunkList::iterator	iter = fChunkList.begin ();

	emuptr	chunkStart = ((emuptr) p) - fChunkHdrSize;
	
	while (iter != fChunkList.end ())
	{
		if (iter->HeaderStart () == chunkStart)
		{
			*iter = EmPalmChunk (*this, chunkStart);
			
			if (delta)
				delta->push_back (*iter);

			return;
		}

		++iter;
	}

	EmAssert (false);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::ResyncHdl
 *
 * DESCRIPTION:	Resynchronize information about a single particular
 *				chunk.  This routine works by assuming that (a) the
 *				chunk already exists in the chunk list and that (b)
 *				only minor changes have been made to the chunk (like
 *				its lockcount having been changed).  This routine
 *				should not be called if the chunk has been moved or
 *				resized, since by implication that means that more
 *				than one chunk has changed.
 *
 *				Does nothing if the heap is not "tracked".
 *
 * PARAMETERS:	h - handle of the chunk that changed.
 *
 *				delta - optional collection to receive the list of
 *					chunks that are different between the current and
 *					previous states of the heap.  This collection is
 *					used when remarking what parts of the heap can be
 *					accessed by different processes.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::ResyncHdl (MemHandle h, EmPalmChunkList* delta)
{
	if (!this->Tracked ())
		return;

	MemPtr	p = DerefHandle (h);

	if (p)
		ResyncPtr (p, delta);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmHeap::GenerateDeltas
 *
 * DESCRIPTION:	Given the current heap state and a previous heap state,
 *				generate the list of chunks that have changed between
 *				the two states.
 *
 * PARAMETERS:	oldList - a previous heap state.
 *				newList - the current heap state.
 *				delta - container for changed chunks.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmHeap::GenerateDeltas (const EmPalmChunkList& oldList,
								 const EmPalmChunkList& newList,
								 EmPalmChunkList& delta)
{
	// Get iterators for the "before" and "after" chunk lists.

	bool							alreadyPushed = false;
	EmPalmChunkList::const_iterator	oldIter = oldList.begin ();
	EmPalmChunkList::const_iterator	newIter	= newList.begin ();

	// Clear out the containing receiving the results.

	delta.clear ();

	// Iterate over the "before" and "after" chunk lists until
	// we reach the end of one of them.

	while (oldIter != oldList.end () && newIter != newList.end ())
	{
		// If the two chunks currently being examined are the same,
		// then the heaps are in sync at this point; proceed to the
		// next chunks in both heap lists.

		if (oldIter->CompareForDelta (*newIter))
		{
			++oldIter;
			++newIter;
			alreadyPushed = false;
			continue;
		}

		// There is a difference between the two chunks.  If we haven't
		// already done so, record the difference by pushing the chunk
		// from the "after" chunk list onto "delta".

		if (!alreadyPushed)
		{
			delta.push_back (*newIter);
			alreadyPushed = true;
		}

		// Try to find where the heap sync up again.  If we increment
		// the "after" iterator, make sure we invalid the flag that
		// says we already added that chunk to the "delta" list.

		if (oldIter->HeaderStart () < newIter->HeaderStart ())
		{
			++oldIter;
		}
		else if (oldIter->HeaderStart () > newIter->HeaderStart ())
		{
			++newIter;
			alreadyPushed = false;
		}
		else
		{
			++oldIter;
			++newIter;
			alreadyPushed = false;
		}
	}

	// If we reached the end of the "before" list, any remaining
	// chunks on the "after" list are differences that need to be
	// added to the "delta" container.

	while (newIter != newList.end ())
	{
		delta.push_back (*newIter);
		++newIter;
	}
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmPalmMPT constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmMPT::EmPalmMPT (void) :
	fVersion (kVersionUnknown),
	fMptHdrStart (0),
	fMptHdrSize (0),
	fSize (0),
	fNumEntries (0),
	fNextTblOffset (0)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmMPT constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	heap - heap wrapper object containing this MPT.
 *				mptHdr - pointer to the master pointer table.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmMPT::EmPalmMPT (const EmPalmHeap& heap, emuptr mptHdr) :
	fVersion (kVersionUnknown),
	fMptHdrStart (0),
	fMptHdrSize (0),
	fSize (0),
	fNumEntries (0),
	fNextTblOffset (0)
{
	this->GetMPTInfo (heap, mptHdr);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmMPT copy constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	other - MPT object we're to clone.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmMPT::EmPalmMPT (const EmPalmMPT& other) :
	fVersion (other.fVersion),
	fMptHdrStart (other.fMptHdrStart),
	fMptHdrSize (other.fMptHdrSize),
	fSize (other.fSize),
	fNumEntries (other.fNumEntries),
	fNextTblOffset (other.fNextTblOffset)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmMPT::~EmPalmMPT
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmMPT::~EmPalmMPT (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmMPT::Validate
 *
 * DESCRIPTION:	Validate that the master pointer table looks valid.
 *
 * PARAMETERS:	heap - heap containing the master pointe table
 *
 * RETURNED:	Nothing (an exception is thrown on error).
 *
 ***********************************************************************/

void EmPalmMPT::Validate (const EmPalmHeap& /*heap*/) const
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmMPT::GetMPTInfo
 *
 * DESCRIPTION:	Scarf up information about the managed master pointer
 *				table.
 *
 * PARAMETERS:	heap - heap object containing this master pointer table.
 *				mptHdr - pointer to the master pointer table contained
 *					in the heap.
 *
 * RETURNED:	Nothing, but many data members are filled in.
 *
 ***********************************************************************/

void EmPalmMPT::GetMPTInfo (const EmPalmHeap& heap, emuptr mptHdr)
{
	CEnableFullAccess	munge;	// Remove blocks on memory access.

	switch (heap.Version ())
	{
		case EmPalmHeap::kVersion1:
			fVersion		= kVersion1;

			fMptHdrStart	= mptHdr;
			fMptHdrSize		= sizeof (Mem1MstrPtrTableType);

			fNumEntries		= EmMemGet16 (mptHdr + offsetof (Mem1MstrPtrTableType, numEntries));
			fNextTblOffset	= EmMemGet16 (mptHdr + offsetof (Mem1MstrPtrTableType, nextTblOffset));
			break;

		case EmPalmHeap::kVersion2:
		case EmPalmHeap::kVersion3:
		case EmPalmHeap::kVersion4:
			fVersion		= kVersion2;

			fMptHdrStart	= mptHdr;
			fMptHdrSize		= sizeof (MemMstrPtrTableType);

			fNumEntries		= EmMemGet16 (mptHdr + offsetof (MemMstrPtrTableType, numEntries));
			fNextTblOffset	= EmMemGet32 (mptHdr + offsetof (MemMstrPtrTableType, nextTblOffset));
			break;

		default:
			EmAssert (false);
			break;
	}

	fSize = fMptHdrSize + fNumEntries * sizeof (MemPtr);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmPalmChunk constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmChunk::EmPalmChunk (void) :
	fVersion (kVersionUnknown),
	fChunkHdrStart (0),
	fChunkHdrSize (0),
	fFree (0),
	fMoved (0),
	fUnused2 (0),
	fUnused3 (0),
	fSizeAdj (0),
	fSize (0),
	fLockCount (0),
	fOwner (0),
	fHOffset (0)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmChunk constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	heap - heap wrapper object containing this chunk.
 *				chunkHdr - pointer to the header of the chunk we're to
 *					wrap up.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmChunk::EmPalmChunk (const EmPalmHeap& heap, emuptr chunkHdr) :
	fVersion (kVersionUnknown),
	fChunkHdrStart (0),
	fChunkHdrSize (0),
	fFree (0),
	fMoved (0),
	fUnused2 (0),
	fUnused3 (0),
	fSizeAdj (0),
	fSize (0),
	fLockCount (0),
	fOwner (0),
	fHOffset (0)
{
	this->GetChunkInfo (heap, chunkHdr);
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmChunk copy constructor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	other - chunk object we're to clone.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmChunk::EmPalmChunk (const EmPalmChunk& other) :
	fVersion (other.fVersion),
	fChunkHdrStart (other.fChunkHdrStart),
	fChunkHdrSize (other.fChunkHdrSize),
	fFree (other.fFree),
	fMoved (other.fMoved),
	fUnused2 (other.fUnused2),
	fUnused3 (other.fUnused3),
	fSizeAdj (other.fSizeAdj),
	fSize (other.fSize),
	fLockCount (other.fLockCount),
	fOwner (other.fOwner),
	fHOffset (other.fHOffset)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmChunk destructor
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmPalmChunk::~EmPalmChunk (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmChunk::Validate
 *
 * DESCRIPTION:	Validate that the chunk looks valid.
 *
 * PARAMETERS:	heap - heap containing the master pointe table
 *
 * RETURNED:	Nothing (an exception is thrown on error).
 *
 ***********************************************************************/

void EmPalmChunk::Validate (const EmPalmHeap& heap) const
{
	try
	{
		// Make sure the chunk is in the heap (this should always be true).
		// !!! It's not...determine why and document.

		if (!heap.Contains (fChunkHdrStart))
		{
			throw (kError_CorruptedHeap_ChunkNotInHeap);
		}

		// Check the size.

		if (this->End () > heap.End ())
		{
			char	buffer[20];

			// !!! There's a problem here.  These variables are set before the
			// variable that uses them.  Thus, replacements based on these variables
			// are made before they're ready.  The result is that %chunk_size and
			// %chunk_max still appear in the final message.  I'm not sure how, but
			// this needs to be addressed and fixed...

			sprintf (buffer, "%0x08X", (int) this->Size ());
			Errors::SetParameter ("%chunk_size", buffer);

			sprintf (buffer, "%0x08X", (int) heap.Size ());
			Errors::SetParameter ("%chunk_max", buffer);

			throw (kError_CorruptedHeap_ChunkTooLarge);
		}

		// These bits should not be set.

		if (fUnused2 || fUnused3)
		{
			throw (kError_CorruptedHeap_InvalidFlags);
		}

		if (!fFree)
		{
			// If it's a movable chunk, validate that the handle offset
			// references the handle that points back to the chunk.

			if (fHOffset)
			{
				// Get the master pointer to this block.  If hOffset is bogus, then
				// this pointer may be bogus, too.

				emuptr	h = fChunkHdrStart - fHOffset * 2;

				// Make sure it is in a master pointer block.

				ITERATE_MPTS(heap, iter, end)
				{
					// See if "h" is in this array.  If so, break from the loop.

					if (iter->TableContains (h))
					{
						break;
					}
					
					++iter;
				}
				
				if (iter == end)
				{
					throw (kError_CorruptedHeap_HOffsetNotInMPT);
				}

				// "h" is in a master pointer table. Make sure "h" points back
				// to the block it's supposed to.

				CEnableFullAccess	munge;	// Remove blocks on memory access.

				if (EmMemGet32 (h) != this->BodyStart())
				{
					throw (kError_CorruptedHeap_HOffsetNotBackPointing);
				}
			}

			// If it's not a movable chunk, it must have a max lock count.

			else
			{
				if (fLockCount != memPtrLockCount)
				{
					throw (kError_CorruptedHeap_InvalidLockCount);
				}
			}
		}

		// !!! Walk the heap and make sure that fChunkHdrStart is found?
	}
	catch (ErrCode err)
	{
		// This will throw an EmExceptionReset object.

		Errors::ReportErrCorruptedHeap (err, this->fChunkHdrStart);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmChunk::CompareForDelta
 *
 * DESCRIPTION:	Compare two chunks to see if they're "close enough"
 *				when determining if something significant has changed.
 *
 * PARAMETERS:	lhs - chunk to compare "*this" to.
 *
 * RETURNED:	True if the two chunks are "equal enough".
 *
 ***********************************************************************/

Bool EmPalmChunk::CompareForDelta (const EmPalmChunk& lhs) const
{
	return
		fVersion			== lhs.fVersion			&&
		fChunkHdrStart		== lhs.fChunkHdrStart	&&
		fChunkHdrSize		== lhs.fChunkHdrSize	&&
		fFree				== lhs.fFree			&&
		fMoved				== lhs.fMoved			&&
//		fUnused2			== lhs.fUnused2			&&
//		fUnused3			== lhs.fUnused3			&&
		fSizeAdj			== lhs.fSizeAdj			&&
		fSize				== lhs.fSize			&&
		(fLockCount > 0)	== (lhs.fLockCount > 0)	&&
//		fOwner				== lhs.fOwner			&&
		fHOffset			== lhs.fHOffset;
}


/***********************************************************************
 *
 * FUNCTION:	EmPalmChunk::GetChunkInfo
 *
 * DESCRIPTION:	Scarf up information about the managed chunk.
 *
 * PARAMETERS:	heapHdr - the pointer to the heap to query.
 *				chunkHdr - pointer to the header of the chunk in the
 *					heap we're to query.
 *
 * RETURNED:	Nothing, but many data members are filled in.
 *
 ***********************************************************************/

void EmPalmChunk::GetChunkInfo (const EmPalmHeap& heap, emuptr chunkHdr)
{
	CEnableFullAccess	munge;	// Remove blocks on memory access.

	switch (heap.Version ())
	{
		case EmPalmHeap::kVersion1:
		{
			fVersion		= kVersion1;
			fChunkHdrStart	= chunkHdr;
			fChunkHdrSize	= sizeof (Mem1ChunkHeaderType);

			UInt16	size	= EmMemGet16 (chunkHdr + offsetof (Mem1ChunkHeaderType, size));

			if (size == 0)
			{
				// Leave now.  We're at the end of the heap.  If we're at the end of RAM,
				// too, attempts to access subsequent fields will cause a bus error.

				break;
			}

			UInt8	lockOwner	= EmMemGet8 (chunkHdr + offsetof (Mem1ChunkHeaderType, lockOwner));
			UInt8	flags		= EmMemGet8 (chunkHdr + offsetof (Mem1ChunkHeaderType, flags));
			Int16	hOffset		= EmMemGet16 (chunkHdr + offsetof (Mem1ChunkHeaderType, hOffset));

			fFree			= (flags & memChunkFlagFree) != 0;
			fMoved			= (flags & memChunkFlagUnused1) != 0;
			fUnused2		= (flags & memChunkFlagUnused2) != 0;
			fUnused3		= (flags & memChunkFlagUnused3) != 0;
			fSizeAdj		= (flags & mem1ChunkFlagSizeAdj);
			fSize			= size;
			fLockCount		= (lockOwner & mem1ChunkLockMask) >> 4;
			fOwner			= (lockOwner & mem1ChunkOwnerMask);
			fHOffset		= hOffset;

			break;
		}

		case EmPalmHeap::kVersion2:
		case EmPalmHeap::kVersion3:
		case EmPalmHeap::kVersion4:
		{
			fVersion		= kVersion2;
			fChunkHdrStart	= chunkHdr;
			fChunkHdrSize	= sizeof (MemChunkHeaderType);

			UInt32	part1	= EmMemGet32 (chunkHdr);
			UInt32	part2	= 0;

			if ((part1 & 0x00FFFFFF) != 0)
			{
				part2		= EmMemGet32 (chunkHdr + 4);
			}

			fFree			= (part1 & 0x80000000) != 0;
			fMoved			= (part1 & 0x40000000) != 0;
			fUnused2		= (part1 & 0x20000000) != 0;
			fUnused3		= (part1 & 0x10000000) != 0;
			fSizeAdj		= (part1 & 0x0F000000) >> 24;
			fSize			= (part1 & 0x00FFFFFF);
			fLockCount		= (part2 & 0xF0000000) >> 28;
			fOwner			= (part2 & 0x0F000000) >> 24;
			fHOffset		= (part2 & 0x00FFFFFF);

			// hOffset is a signed value; make sure we keep it that way
			// when we extract it by hand.

			if ((fHOffset & 0x00800000) != 0)
			{
				fHOffset |= 0xFF000000;
			}

			break;
		}

		default:
			EmAssert (false);
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ operator << (EmStream&, EmPalmChunk&)
// ---------------------------------------------------------------------------

EmStream& operator << (EmStream& s, const EmPalmChunk& chunk)
{
	const long	kVersion = 1;
	
	s << kVersion;

	s << chunk.fVersion;

	s << chunk.fChunkHdrStart;
	s << chunk.fChunkHdrSize;

	s << chunk.fFree;
	s << chunk.fMoved;
	s << chunk.fUnused2;
	s << chunk.fUnused3;
	s << chunk.fSizeAdj;
	s << chunk.fSize;

	s << chunk.fLockCount;
	s << chunk.fOwner;
	s << chunk.fHOffset;

	return s;
}

// ---------------------------------------------------------------------------
//		¥ operator >> (EmStream&, EmPalmChunk&)
// ---------------------------------------------------------------------------

EmStream& operator >> (EmStream& s, EmPalmChunk& chunk)
{
	long	version;
	s >> version;

	if (version >= 1)
	{
		s >> chunk.fVersion;

		s >> chunk.fChunkHdrStart;
		s >> chunk.fChunkHdrSize;

		s >> chunk.fFree;
		s >> chunk.fMoved;
		s >> chunk.fUnused2;
		s >> chunk.fUnused3;
		s >> chunk.fSizeAdj;
		s >> chunk.fSize;

		s >> chunk.fLockCount;
		s >> chunk.fOwner;
		s >> chunk.fHOffset;
	}

	return s;
}

// ---------------------------------------------------------------------------
//		¥ operator << (EmStream&, EmPalmMPT&)
// ---------------------------------------------------------------------------

EmStream& operator << (EmStream& s, const EmPalmMPT& mpt)
{
	const long	kVersion = 1;
	
	s << kVersion;

	s << mpt.fVersion;

	s << mpt.fMptHdrStart;
	s << mpt.fMptHdrSize;
	s << mpt.fSize;

	s << mpt.fNumEntries;
	s << mpt.fNextTblOffset;

	return s;
}

// ---------------------------------------------------------------------------
//		¥ operator >> (EmStream&, EmPalmMPT&)
// ---------------------------------------------------------------------------

EmStream& operator >> (EmStream& s, EmPalmMPT& mpt)
{
	long	version;
	s >> version;

	if (version >= 1)
	{
		s >> mpt.fVersion;

		s >> mpt.fMptHdrStart;
		s >> mpt.fMptHdrSize;
		s >> mpt.fSize;

		s >> mpt.fNumEntries;
		s >> mpt.fNextTblOffset;
	}

	return s;
}

// ---------------------------------------------------------------------------
//		¥ operator << (EmStream&, EmPalmHeap&)
// ---------------------------------------------------------------------------

EmStream& operator << (EmStream& s, const EmPalmHeap& heap)
{
	const long	kVersion = 1;
	
	s << kVersion;

	s << heap.fVersion;
	s << heap.fHeapID;

	s << heap.fHeapHdrStart;
	s << heap.fHeapHdrSize;
	s << heap.fFirstChunk;

	s << heap.fFlags;
	s << heap.fSize;
	s << heap.fFirstFree;
	s << heap.fChunkHdrSize;

	s << heap.fChunkList;
	s << heap.fMPTList;

	return s;
}

// ---------------------------------------------------------------------------
//		¥ operator >> (EmStream&, EmPalmHeap&)
// ---------------------------------------------------------------------------

EmStream& operator >> (EmStream& s, EmPalmHeap& heap)
{
	long	version;
	s >> version;

	if (version >= 1)
	{
		s >> heap.fVersion;
		s >> heap.fHeapID;

		s >> heap.fHeapHdrStart;
		s >> heap.fHeapHdrSize;
		s >> heap.fFirstChunk;

		s >> heap.fFlags;
		s >> heap.fSize;
		s >> heap.fFirstFree;
		s >> heap.fChunkHdrSize;

		s >> heap.fChunkList;
		s >> heap.fMPTList;
	}

	return s;
}

