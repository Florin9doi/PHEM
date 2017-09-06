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
#include "ChunkFile.h"

#include "Byteswapping.h"		// Canonical
#include "Platform.h"			// ReallocMemory

#include <string.h>

/***********************************************************************
 *
 * FUNCTION:	ChunkFile constructor
 *
 * DESCRIPTION:	Initialize the ChunkFile object.  Sets the fStream data
 *				member to refer to the given EmStream (which must
 *				exist for the life of the ChunkFile).
 *
 * PARAMETERS:	s - EmStream to utilize for read/write operations.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

ChunkFile::ChunkFile (EmStream& s) :
	fStream (s)
{
}


/***********************************************************************
 *
 * FUNCTION:	ChunkFile destructor
 *
 * DESCRIPTION:	Releases ChunkFile resources.  Currently does nothing.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

ChunkFile::~ChunkFile (void)
{
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	ChunkFile::FindChunk
 *
 * DESCRIPTION:	Find the requested chunk.  If successful, the file
 *				marker will be pointing to the chunk data which can
 *				then be read with ReadChunk.
 *
 * PARAMETERS:	targetTag - the tag of the chunk to find.
 *
 * RETURNED:	Size of the chunk, in bytes.  If the chunk can't be
 *				found, kChunkNotFound is returned.
 *
 ***********************************************************************/

long ChunkFile::FindChunk (Tag targetTag)
{
	long	len = kChunkNotFound;
	long	fileOffset = 0;
	long	fileLength = fStream.GetLength ();

	fStream.SetMarker (fileOffset, kStreamFromStart);

	while (fileOffset < fileLength)
	{
		Tag		chunkTag;
		long	chunkLen;

		fStream.GetBytes (&chunkTag, sizeof (chunkTag));
		fStream.GetBytes (&chunkLen, sizeof (chunkLen));

		Canonical (chunkTag);
		Canonical (chunkLen);

		if (chunkTag == targetTag)
		{
			len = chunkLen;
			break;
		}

		fStream.SetMarker (chunkLen, kStreamFromMarker);
		fileOffset += sizeof (chunkTag) + sizeof (chunkLen) + chunkLen;
	}

	return len;
}


/***********************************************************************
 *
 * FUNCTION:	ChunkFile::ReadChunk
 *
 * DESCRIPTION:	Find the requested chunk.  If successful, the file
 *				marker will be pointing to the chunk data which can
 *				then be read with ReadChunk.
 *
 * PARAMETERS:	targetTag - the tag of the chunk to find.
 *
 * RETURNED:	Size of the chunk, in bytes.  If the chunk can't be
 *				found, kChunkNotFound is returned.
 *
 ***********************************************************************/

Bool ChunkFile::ReadChunk (int index, Tag& tag, Chunk& chunk)
{
	long	fileOffset = 0;
	long	fileLength = fStream.GetLength ();

	fStream.SetMarker (fileOffset, kStreamFromStart);

	while (fileOffset < fileLength)
	{
		// Read the chunk's tag and length.

		Tag		chunkTag;
		long	chunkLen;

		fStream.GetBytes (&chunkTag, sizeof (chunkTag));
		fStream.GetBytes (&chunkLen, sizeof (chunkLen));

		Canonical (chunkTag);
		Canonical (chunkLen);

		// If this is the chunk we're looking for, read it in.

		if (index == 0)
		{
			tag = chunkTag;
			chunk.SetLength (chunkLen);
			this->ReadChunk (chunkLen, chunk.GetPointer ());

			// Return that we found the chunk.

			return true;
		}

		// Move to the next chunk.

		fStream.SetMarker (chunkLen, kStreamFromMarker);
		fileOffset += sizeof (chunkTag) + sizeof (chunkLen) + chunkLen;
		--index;
	}

	// Return that we didn't find the chunk.

	return false;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	ChunkFile::ReadChunk
 *
 * DESCRIPTION:	Read a block of bytes into the given buffer.
 *
 * PARAMETERS:	tag - marker identifying the data to read.
 *
 *				chunk - object to contain the returned data.
 *
 * RETURNED:	True if the data was found and read in.
 *
 ***********************************************************************/

Bool ChunkFile::ReadChunk (Tag tag, Chunk& chunk)
{
	long	size = this->FindChunk (tag);

	if (size == kChunkNotFound)
		return false;

	chunk.SetLength (size);
	this->ReadChunk (size, chunk.GetPointer ());

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	ChunkFile::ReadInt
 *
 * DESCRIPTION:	Read an integer (scalar) from the file.
 *
 * PARAMETERS:	tag - marker of the value to be read.
 *
 *				val - reference to variable to receive the integer.
 *
 * RETURNED:	True if the value existed; false if the tag could not
 *				be found.
 *
 ***********************************************************************/

Bool ChunkFile::ReadInt (Tag tag, uint8& val)
{
	if (ChunkFile::FindChunk (tag) == kChunkNotFound)
		return false;

	ChunkFile::ReadChunk (sizeof (val), &val);
	Canonical (val);

	return true;
}


Bool ChunkFile::ReadInt (Tag tag, int8& val)
{
	if (ChunkFile::FindChunk (tag) == kChunkNotFound)
		return false;

	ChunkFile::ReadChunk (sizeof (val), &val);
	Canonical (val);

	return true;
}


Bool ChunkFile::ReadInt (Tag tag, uint16& val)
{
	if (ChunkFile::FindChunk (tag) == kChunkNotFound)
		return false;

	ChunkFile::ReadChunk (sizeof (val), &val);
	Canonical (val);

	return true;
}


Bool ChunkFile::ReadInt (Tag tag, int16& val)
{
	if (ChunkFile::FindChunk (tag) == kChunkNotFound)
		return false;

	ChunkFile::ReadChunk (sizeof (val), &val);
	Canonical (val);

	return true;
}


Bool ChunkFile::ReadInt (Tag tag, uint32& val)
{
	if (ChunkFile::FindChunk (tag) == kChunkNotFound)
		return false;

	ChunkFile::ReadChunk (sizeof (val), &val);
	Canonical (val);

	return true;
}


Bool ChunkFile::ReadInt (Tag tag, int32& val)
{
	if (ChunkFile::FindChunk (tag) == kChunkNotFound)
		return false;

	ChunkFile::ReadChunk (sizeof (val), &val);
	Canonical (val);

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	ChunkFile::ReadString
 *
 * DESCRIPTION:	Read a string from the file.
 *
 * PARAMETERS:	tag - marker of the string to be read.
 *
 *				s - reference to variable to receive the string.
 *
 * RETURNED:	True if the string existed; false if the tag could not
 *				be found.
 *
 ***********************************************************************/

Bool ChunkFile::ReadString (Tag tag, char* s)
{
	long	len = ChunkFile::FindChunk (tag);
	if (len == kChunkNotFound)
		return false;

	if (len > 0)
	{
		ChunkFile::ReadChunk (len, s);
	}

	s[len] = '\0';			// Null terminator

	return true;
}

Bool ChunkFile::ReadString (Tag tag, string& s)
{
	long	len = ChunkFile::FindChunk (tag);
	if (len == kChunkNotFound)
		return false;

	s.resize (len);
	if (len > 0)
	{
		ChunkFile::ReadChunk (len, &s[0]);
	}

	return true;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	ChunkFile::WriteChunk
 *
 * DESCRIPTION:	Write the given data to the file.  The data is marked
 *				with the given tag. No effort is made to find any
 *				existing chunks with the same tag and deleting them.
 *
 * PARAMETERS:	tag - marker for the data being written.
 *
 *				chunk - object containing the data to write.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void ChunkFile::WriteChunk (Tag tag, const Chunk& chunk)
{
	this->WriteChunk (tag, chunk.GetLength (), chunk.GetPointer ());
}


/***********************************************************************
 *
 * FUNCTION:	ChunkFile::WriteInt
 *
 * DESCRIPTION:	Write the given value to disk, marking it with the
 *				given tag.
 *
 * PARAMETERS:	tag - marker used to later retrieve the value.
 *
 *				val - integer to write to disk.  All integers are
 *					stored in Big Endian format, and are written in
 *					their native size (1, 2, or 4 bytes).
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void ChunkFile::WriteInt (Tag tag, uint8 val)
{
	Canonical (val);
	ChunkFile::WriteChunk (tag, sizeof (val), &val);
}


void ChunkFile::WriteInt (Tag tag, int8 val)
{
	Canonical (val);
	ChunkFile::WriteChunk (tag, sizeof (val), &val);
}


void ChunkFile::WriteInt (Tag tag, uint16 val)
{
	Canonical (val);
	ChunkFile::WriteChunk (tag, sizeof (val), &val);
}


void ChunkFile::WriteInt (Tag tag, int16 val)
{
	Canonical (val);
	ChunkFile::WriteChunk (tag, sizeof (val), &val);
}


void ChunkFile::WriteInt (Tag tag, uint32 val)
{
	Canonical (val);
	ChunkFile::WriteChunk (tag, sizeof (val), &val);
}


void ChunkFile::WriteInt (Tag tag, int32 val)
{
	Canonical (val);
	ChunkFile::WriteChunk (tag, sizeof (val), &val);
}


/***********************************************************************
 *
 * FUNCTION:	ChunkFile::WriteString
 *
 * DESCRIPTION:	Write the given string to disk, marking it with the
 *				given tag.
 *
 * PARAMETERS:	tag - marker used to later retrieve the value.
 *
 *				s - string to write to disk.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void ChunkFile::WriteString (Tag tag, const char* s)
{
	ChunkFile::WriteChunk (tag, strlen (s), s);
}

void ChunkFile::WriteString (Tag tag, const string& s)
{
	ChunkFile::WriteChunk (tag, s.size (), s.c_str ());
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	ChunkFile::ReadChunk
 *
 * DESCRIPTION:	Read a block of bytes into the given buffer
 *
 * PARAMETERS:	size - number of bytes to read.
 *
 *				data - buffer to write the bytes to.
 *
 * RETURNED:	Nothing (exceptions can be thrown).
 *
 ***********************************************************************/

void ChunkFile::ReadChunk (uint32 size, void* data)
{
	// Merely read the requested bytes starting at the current location.

	fStream.GetBytes (data, size);
}


/***********************************************************************
 *
 * FUNCTION:	ChunkFile::WriteChunk
 *
 * DESCRIPTION:	Write the given data to the file.  The data is marked
 *				with the given tag. No effort is made to find any
 *				existing chunks with the same tag and deleting them.
 *
 * PARAMETERS:	tag - marker for the data being written.
 *
 *				size - number of bytes to write.
 *
 *				data - pointer to buffer containing bytes to write.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void ChunkFile::WriteChunk (Tag tag, uint32 size, const void* data)
{
	// Write the 4-byte tag in Big Endian format.

	Canonical (tag);
	fStream.PutBytes (&tag, sizeof (tag));

	// Write the chunk size in Big Endian format.  Return the size
	// back to host format when done so that it can be used to write
	// the actual data.

	Canonical (size);
	fStream.PutBytes (&size, sizeof (size));
	Canonical (size);

	// Write the chunk data.

	fStream.PutBytes (data, size);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	ChunkFile::GetFile
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

EmStream& ChunkFile::GetStream (void) const
{
	return fStream;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	Chunk::Chunk
 *
 * DESCRIPTION:	Default constructor.  Creates a "chunk" that initially
 *				has no memory.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/
Chunk::Chunk (void) :
	fPtr (NULL),
	fUsedSize (0),
	fAllocatedSize (0)
{
}


/***********************************************************************
 *
 * FUNCTION:	Chunk::Chunk
 *
 * DESCRIPTION:	Parameterized constructor.  Creates a "chunk" with a
 *				block of memory with the given length.
 *
 * PARAMETERS:	inLength - number of bytes for the initial block of
 *					memory.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Chunk::Chunk (long inLength) :
	fPtr (NULL),
	fUsedSize (0),
	fAllocatedSize (0)
{
	SetLength (inLength);
}


/***********************************************************************
 *
 * FUNCTION:	Chunk::Chunk
 *
 * DESCRIPTION:	Copy constructor.  Creates a "chunk" containing a copy
 *				of the given data.
 *
 * PARAMETERS:	other - chunk containing the data to copy.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Chunk::Chunk (const Chunk& other) :
	fPtr (NULL),
	fUsedSize (0),
	fAllocatedSize (0)
{
	this->SetLength (other.GetLength ());
	memcpy (this->GetPointer (), other.GetPointer (), other.GetLength ());
}


/***********************************************************************
 *
 * FUNCTION:	Chunk::~Chunk
 *
 * DESCRIPTION:	Destructor.  Disposes of the owned block of memory.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Chunk::~Chunk (void)
{
	Platform::DisposeMemory (fPtr);
}


/***********************************************************************
 *
 * FUNCTION:	Chunk::operator=
 *
 * DESCRIPTION:	Assignment operator.  Makes the controlled chunk
 *				contain a copy of the data managed by the other chunk.
 *
 * PARAMETERS:	other - chunk containing the data to copy.
 *
 * RETURNED:	Referenced to the controlled object.
 *
 ***********************************************************************/

Chunk& Chunk::operator= (const Chunk& other)
{
	if (this != &other)
	{
		this->SetLength (other.GetLength ());
		memcpy (this->GetPointer (), other.GetPointer (), other.GetLength ());
	}

	return *this;
}


/***********************************************************************
 *
 * FUNCTION:	Chunk::GetPointer
 *
 * DESCRIPTION:	Returns a pointer to the owned block of memory.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	A pointer to the owned block of memory.
 *
 ***********************************************************************/

void* Chunk::GetPointer (void) const
{
	return fPtr;
}


/***********************************************************************
 *
 * FUNCTION:	Chunk::GetLength
 *
 * DESCRIPTION:	Returns the number of useable bytes in the block of
 *				memory.  The actual number of allocated bytes may be
 *				larger, but shouldn't be used.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The number of usable bytes in the chunk.
 *
 ***********************************************************************/

long Chunk::GetLength (void) const
{
	return fUsedSize;
}


/***********************************************************************
 *
 * FUNCTION:	Chunk::SetLength
 *
 * DESCRIPTION:	Set the number of useable bytes in the owned block of
 *				memory.  The number of bytes actually allocated may
 *				be greater than passed in.  After this call, any
 *				previous pointers returned by GetPointer may be
 *				invalid.
 *
 * PARAMETERS:	inLength - the number of useable bytes to make available.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Chunk::SetLength (long inLength)
{
	if (inLength > fAllocatedSize)
	{
		const long	kSlushFund = 100;
		long	newAllocatedLength = inLength + kSlushFund;
		fPtr = Platform::ReallocMemory (fPtr, newAllocatedLength);
		fAllocatedSize = newAllocatedLength;
	}

	// As a special case, allow the clean up of memory by setting the
	// length to zero.

	else if (inLength == 0)
	{
		Platform::DisposeMemory (fPtr);
		fAllocatedSize = 0;
	}

	fUsedSize = inLength;
}


// ===========================================================================
//	EmStreamChunk.cpp
// ===========================================================================
//
//	A Stream whose bytes are in a Chunk


// ---------------------------------------------------------------------------
//	¥ EmStreamChunk(Handle)					Parameterized Constructor [public]
// ---------------------------------------------------------------------------
//	Construct from an existing Chunk

EmStreamChunk::EmStreamChunk(Chunk* chunk) :
	EmStream (),
	fChunk (*chunk),
	fOwnedChunk (chunk)
{
	EmStream::SetLength (fChunk.GetLength ());
}


EmStreamChunk::EmStreamChunk(Chunk& chunk) :
	EmStream (),
	fChunk (chunk),
	fOwnedChunk (NULL)
{
	EmStream::SetLength (fChunk.GetLength ());
}


// ---------------------------------------------------------------------------
//	¥ ~EmStreamChunk						Destructor				  [public]
// ---------------------------------------------------------------------------

EmStreamChunk::~EmStreamChunk()
{
	delete fOwnedChunk;
}


// ---------------------------------------------------------------------------
//	¥ SetLength														  [public]
// ---------------------------------------------------------------------------
//	Set the length, in bytes, of the EmStreamChunk

void
EmStreamChunk::SetLength(
	int32	inLength)
{
	fChunk.SetLength (inLength);
	EmStream::SetLength (inLength);
}


// ---------------------------------------------------------------------------
//	¥ PutBytes
// ---------------------------------------------------------------------------
//	Write bytes from a buffer to a EmStreamChunk
//
//	Returns an error code and passes back the number of bytes actually
//	written, which may be less than the number requested if an error occurred.
//
//	Grows data Chunk if necessary.

ErrCode
EmStreamChunk::PutBytes(
	const void	*inBuffer,
	int32		ioByteCount)
{
	ErrCode	err = errNone;
	int32	endOfWrite = GetMarker () + ioByteCount;
	
	if (endOfWrite > GetLength ())	// Need to grow Chunk
	{
		SetLength (endOfWrite);
	}
									// Copy bytes into Chunk
	if (ioByteCount > 0)
	{
		memmove ((char*) fChunk.GetPointer () + GetMarker (), inBuffer, ioByteCount);
		SetMarker (ioByteCount, kStreamFromMarker);
	}
	
	return err;
}


// ---------------------------------------------------------------------------
//	¥ GetBytes
// ---------------------------------------------------------------------------
//	Read bytes from a EmStreamChunk to a buffer
//
//	Returns an error code and passes back the number of bytes actually
//	read, which may be less than the number requested if an error occurred.
//
//	Errors:
//		readErr		Attempt to read past the end of the EmStreamChunk

ErrCode
EmStreamChunk::GetBytes(
	void	*outBuffer,
	int32	ioByteCount)
{
	ErrCode	err = errNone;
									// Upper bound is number of bytes from
									//   marker to end
	if ((GetMarker () + ioByteCount) > GetLength ())
	{
		ioByteCount = GetLength () - GetMarker ();
		err = 1;
	}
									// Copy bytes from Handle into buffer

	memmove (outBuffer, (char*) fChunk.GetPointer () + GetMarker (), ioByteCount);
	SetMarker (ioByteCount, kStreamFromMarker);
	
	return err;
}
