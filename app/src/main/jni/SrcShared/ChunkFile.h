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

#ifndef _CHUNKFILE_H_
#define _CHUNKFILE_H_

#include "EmStream.h"			// EmStream

class Chunk;


/*
	ChunkFile is a class for managing files containing tagged
	chunks of data.  It contain member functions for writing
	tagged data to a file, finding that data later, and reading
	the data back into user-specified buffers.  There are also
	member functions for writing common data types (numbers and
	strings) in a platform-independent fashion.
	
	There is no facility for *updating* chunk files.  That is,
	there is no way to modify the contents of a chunk that already
	exists in a a chunk file (regardless of whether or not the
	length of the chunk changes).
	
	The format of a chunk file is simple.  It's essentially a
	variable-length array of:
	
		tag: 4 bytes
		size: 4 bytes
		data: "size" bytes of data

	"tag", "size", and any integer values written with WriteInt
	are stored in Big Endian format.  Strings are stored without
	the NULL terminator.  Data is packed as tightly as possible;
	there's no word or longword alignment.
 */

class ChunkFile
{
	public:
								ChunkFile		(EmStream& s);
								~ChunkFile		(void);

		typedef uint32	Tag;

//		static const long		kChunkNotFound = -1;	// VC++ is a bit medieval here...
		enum { kChunkNotFound = -1 };
		long					FindChunk		(Tag tag);	// Returns chunk size

		Bool					ReadChunk		(int index, Tag& tag, Chunk&);
		Bool					ReadChunk		(Tag tag, Chunk&);
		void					ReadChunk		(uint32 size, void* data);
		Bool					ReadInt			(Tag tag, uint8&);
		Bool					ReadInt			(Tag tag, int8&);
		Bool					ReadInt			(Tag tag, uint16&);
		Bool					ReadInt			(Tag tag, int16&);
		Bool					ReadInt			(Tag tag, uint32&);
		Bool					ReadInt			(Tag tag, int32&);
		Bool					ReadString		(Tag tag, char*);
		Bool					ReadString		(Tag tag, string&);

		void					WriteChunk		(Tag tag, const Chunk&);
		void					WriteChunk		(Tag tag, uint32 size, const void* data);
		void					WriteInt		(Tag tag, uint8);
		void					WriteInt		(Tag tag, int8);
		void					WriteInt		(Tag tag, uint16);
		void					WriteInt		(Tag tag, int16);
		void					WriteInt		(Tag tag, uint32);
		void					WriteInt		(Tag tag, int32);
		void					WriteString		(Tag tag, const char*);
		void					WriteString		(Tag tag, const string&);

		EmStream&				GetStream		(void) const;

	private:
		EmStream&				fStream;
};


class Chunk
{
	public:
								Chunk			(void);
								Chunk			(long inLength);
								Chunk			(const Chunk&);
								~Chunk			(void);

		Chunk&					operator=		(const Chunk&);

		void*					GetPointer		(void) const;
		long					GetLength		(void) const;
		void					SetLength		(long inLength);

	private:
		void*					fPtr;
		long					fUsedSize;
		long					fAllocatedSize;
};


class EmStreamChunk : public EmStream
{
	public:
								EmStreamChunk	(Chunk*);
								EmStreamChunk	(Chunk&);
		virtual					~EmStreamChunk	(void);

		virtual void			SetLength		(int32			inLength);

		virtual ErrCode			PutBytes		(const void*	inBuffer,
												 int32			ioByteCount);
		virtual ErrCode			GetBytes		(void*			outBuffer,
												 int32			ioByteCount);

	private:
		Chunk&					fChunk;
		Chunk*					fOwnedChunk;
};


#endif	// _CHUNKFILE_H_
