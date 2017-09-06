/* -*- mode: C++; tab-width: 4 -*- */
// ===========================================================================
//	EmStream.cpp			   ©1993-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================
//
//	Abstract class for reading/writing an ordered sequence of bytes

#include "EmCommon.h"
#include "EmStream.h"

#include "Byteswapping.h"		// Canonical

/* Update for GCC 4 */
#include <string.h>

#pragma mark --- Construction & Destruction ---

// ---------------------------------------------------------------------------
//	¥ EmStream
// ---------------------------------------------------------------------------
//	Default Constructor

EmStream::EmStream (void) :
	mMarker (0),
	mLength (0)
{
}


// ---------------------------------------------------------------------------
//	¥ ~EmStream
// ---------------------------------------------------------------------------
//	Destructor

EmStream::~EmStream (void)
{
}


#pragma mark --- Accessors ---

// ---------------------------------------------------------------------------
//	¥ SetMarker
// ---------------------------------------------------------------------------
//	Place the Read/Write Marker at an offset from a specified position
//
//	inFromWhere can be streamFrom_Start, streamFrom_End, or streamFrom_Marker

void
EmStream::SetMarker (
	int32			inOffset,
	StreamFromType	inFromWhere)
{
	int32	newMarker = mMarker;

	switch (inFromWhere)
	{
		case kStreamFromStart:
			newMarker = inOffset;
			break;

		case kStreamFromEnd:
			newMarker = this->GetLength () - inOffset;
			break;

		case kStreamFromMarker:
			newMarker += inOffset;
			break;
	}

	if (newMarker < 0)		// marker must be between 0 and Length, inclusive
	{
		newMarker = 0;
	}
	else if (newMarker > GetLength ())
	{
		newMarker = this->GetLength ();
	}

	mMarker = newMarker;
}


// ---------------------------------------------------------------------------
//	¥ GetMarker
// ---------------------------------------------------------------------------
//	Return the Read/Write Marker position
//
//	Position is a byte offset from the start of the Stream

int32
EmStream::GetMarker (void) const
{
	return mMarker;
}


// ---------------------------------------------------------------------------
//	¥ SetLength
// ---------------------------------------------------------------------------
//	Set the length, in bytes, of the Stream

void
EmStream::SetLength(
	int32	inLength)
{
	int32	oldLength = this->GetLength ();
	mLength = inLength;
										// If making Stream shorter, call
										//   SetMarker to make sure that
	if (oldLength > inLength)			//   marker is not past the end
	{
		this->SetMarker (this->GetMarker (), kStreamFromStart);
	}
}


// ---------------------------------------------------------------------------
//	¥ GetLength
// ---------------------------------------------------------------------------
//	Return the length, in bytes, of the Stream

int32
EmStream::GetLength (void) const
{
	return mLength;
}

#pragma mark --- Low-Level I/O ---

// ---------------------------------------------------------------------------
//	¥ PutBytes
// ---------------------------------------------------------------------------
//	Write bytes from a buffer to a Stream
//
//	Returns an error code and passes back the number of bytes actually
//	written, which may be less than the number requested if an error occurred.
//
//	Subclasses must override this function to support writing.
//
//	NOTE: You should not throw an Exception out of this function.

ErrCode
EmStream::PutBytes (
	const void*	inBuffer,
	int32		ioByteCount)
{
	UNUSED_PARAM(inBuffer)
	UNUSED_PARAM(ioByteCount)

	return 1;
}


EmStream&
EmStream::operator <<		(const char* inString)
{
	WriteCString (inString);
	return (*this);
}

EmStream&
EmStream::operator <<		(const string& inString)
{
	WriteString (inString);
	return (*this);
}

EmStream&
EmStream::operator <<		(int8 inNum)
{
	Canonical (inNum);
	PutBytes (&inNum, sizeof (inNum));
	return (*this);
}

EmStream&
EmStream::operator <<		(uint8 inNum)
{
	Canonical (inNum);
	PutBytes (&inNum, sizeof (inNum));
	return (*this);
}

EmStream&
EmStream::operator <<		(char inChar)
{
	Canonical (inChar);
	PutBytes (&inChar, sizeof (inChar));
	return (*this);
}

EmStream&
EmStream::operator <<		(int16 inNum)
{
	Canonical (inNum);
	PutBytes (&inNum, sizeof (inNum));
	return (*this);
}

EmStream&
EmStream::operator <<		(uint16 inNum)
{
	Canonical (inNum);
	PutBytes (&inNum, sizeof (inNum));
	return (*this);
}

EmStream&
EmStream::operator <<		(int32 inNum)
{
	Canonical (inNum);
	PutBytes (&inNum, sizeof (inNum));
	return (*this);
}

EmStream&
EmStream::operator <<		(uint32 inNum)
{
	Canonical (inNum);
	PutBytes (&inNum, sizeof (inNum));
	return (*this);
}

EmStream&
EmStream::operator <<		(int64 inNum)
{
	Canonical (inNum);
	PutBytes (&inNum, sizeof (inNum));
	return (*this);
}

EmStream&
EmStream::operator <<		(uint64 inNum)
{
	Canonical (inNum);
	PutBytes (&inNum, sizeof (inNum));
	return (*this);
}

EmStream&
EmStream::operator <<		(bool inBool)
{
	Canonical (inBool);
	PutBytes (&inBool, sizeof (inBool));
	return (*this);
}

		
// ---------------------------------------------------------------------------
//	¥ GetBytes
// ---------------------------------------------------------------------------
//	Read bytes from a Stream to a buffer
//
//	Returns an error code and passes back the number of bytes actually
//	read, which may be less than the number requested if an error occurred.
//
//	Subclasses must override this function to support reading.
//
//	NOTE: You should not throw an Exception out of this function.

ErrCode
EmStream::GetBytes (
	void*	outBuffer,
	int32	ioByteCount)
{
	UNUSED_PARAM(outBuffer)
	UNUSED_PARAM(ioByteCount)

	return 1;
}


// ---------------------------------------------------------------------------
//	¥ PeekData
// ---------------------------------------------------------------------------
//	Read data from a Stream to a buffer, without moving the Marker
//
//	Return the number of bytes actually read, which may be less than the
//	number requested if an error occurred

int32
EmStream::PeekData (
	void*	outBuffer,
	int32	inByteCount)
{
	int32	currentMarker = this->GetMarker ();

	int32	bytesToPeek = inByteCount;
	this->GetBytes (outBuffer, bytesToPeek);

	this->SetMarker (currentMarker, kStreamFromStart);

	return bytesToPeek;
}

EmStream&
EmStream::operator >>		(char* outString)
{
	ReadCString (outString);
	return (*this);
}

EmStream&
EmStream::operator >>		(string& outString)
{
	ReadString (outString);
	return (*this);
}

EmStream&
EmStream::operator >>		(int8 &outNum)
{
	GetBytes (&outNum, sizeof (outNum));
	Canonical (outNum);
	return (*this);
}

EmStream&
EmStream::operator >>		(uint8 &outNum)
{
	GetBytes (&outNum, sizeof (outNum));
	Canonical (outNum);
	return (*this);
}

EmStream&
EmStream::operator >>		(char &outChar)
{
	GetBytes (&outChar, sizeof (outChar));
	Canonical (outChar);
	return (*this);
}

EmStream&
EmStream::operator >>		(int16 &outNum)
{
	GetBytes (&outNum, sizeof (outNum));
	Canonical (outNum);
	return (*this);
}

EmStream&
EmStream::operator >>		(uint16 &outNum)
{
	GetBytes (&outNum, sizeof (outNum));
	Canonical (outNum);
	return (*this);
}

EmStream&
EmStream::operator >>		(int32 &outNum)
{
	GetBytes (&outNum, sizeof (outNum));
	Canonical (outNum);
	return (*this);
}

EmStream&
EmStream::operator >>		(uint32 &outNum)
{
	GetBytes (&outNum, sizeof (outNum));
	Canonical (outNum);
	return (*this);
}

EmStream&
EmStream::operator >>		(int64 &outNum)
{
	GetBytes (&outNum, sizeof (outNum));
	Canonical (outNum);
	return (*this);
}

EmStream&
EmStream::operator >>		(uint64 &outNum)
{
	GetBytes (&outNum, sizeof (outNum));
	Canonical (outNum);
	return (*this);
}

EmStream&
EmStream::operator >>		(bool &outBool)
{
	GetBytes (&outBool, sizeof (outBool));
	Canonical (outBool);
	return (*this);
}


#pragma mark --- High-Level I/O ---

// ---------------------------------------------------------------------------
//	¥ WriteCString
// ---------------------------------------------------------------------------
//	Write a C string to a Stream
//
//	Returns the number of bytes written.

int32
EmStream::WriteCString (
	const char* inString)
{
	EmAssert (inString);

	int32	strLen = strlen (inString);

		// Write C string as a 4-byte count followed by the characters.
		// Do not write the null terminator.

	*this << strLen;
	this->PutBytes (inString, strLen);

	return (strLen + (int32) sizeof (strLen));
}


// ---------------------------------------------------------------------------
//	¥ ReadCString
// ---------------------------------------------------------------------------
//	Read a C string from a Stream
//
//	Returns the number of bytes read

int32
EmStream::ReadCString (
	char* outString)
{
	EmAssert (outString);

		// C string is stored as a 4-byte count followed by the
		// characters. The null terminator is not stored and must
		// be added afterwards.

	int32	strLen;
	*this >> strLen;

	this->GetBytes (outString, strLen);
	outString[strLen] = '\0';			// Null terminator

	return (strLen + (int32) sizeof (int32));
}

// ---------------------------------------------------------------------------
//	¥ WriteString
// ---------------------------------------------------------------------------
//	Write a C string to a Stream
//
//	Returns the number of bytes written.

int32
EmStream::WriteString (
	const string&	inString)
{
	return this->WriteCString (inString.c_str ());
}


// ---------------------------------------------------------------------------
//	¥ ReadString
// ---------------------------------------------------------------------------
//	Read a C string from a Stream
//
//	Returns the number of bytes read

int32
EmStream::ReadString(
	string&			outString)
{
		// C string is stored as a 4-byte count followed by the
		// characters. The null terminator is not stored and must
		// be added afterwards.

	int32	strLen;
	*this >> strLen;

	outString.resize (strLen);
	if (strLen > 0)
	{
		GetBytes(&outString[0], strLen);
	}

	return (strLen + (int32) sizeof (int32));
}


// ===========================================================================
//	EmStreamBlock.cpp			   ©1993-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================
//
//	A Stream whose bytes are in a Chunk


// ---------------------------------------------------------------------------
//	¥ EmStreamBlock(Handle)					Parameterized Constructor [public]
// ---------------------------------------------------------------------------
//	Construct from an existing Chunk

EmStreamBlock::EmStreamBlock(void* data, int32 length) :
	EmStream (),
	fData (data)
{
	EmStream::SetLength (length);
}


// ---------------------------------------------------------------------------
//	¥ ~EmStreamBlock						Destructor				  [public]
// ---------------------------------------------------------------------------

EmStreamBlock::~EmStreamBlock()
{
}


// ---------------------------------------------------------------------------
//	¥ SetLength														  [public]
// ---------------------------------------------------------------------------
//	Set the length, in bytes, of the EmStreamBlock

void
EmStreamBlock::SetLength(
	int32	/*inLength*/)
{
	// Do nothing...can't change the length.
}


// ---------------------------------------------------------------------------
//	¥ PutBytes
// ---------------------------------------------------------------------------
//	Write bytes from a buffer to a EmStreamBlock
//
//	Returns an error code and passes back the number of bytes actually
//	written, which may be less than the number requested if an error occurred.
//
//	Grows data Chunk if necessary.

ErrCode
EmStreamBlock::PutBytes(
	const void*	inBuffer,
	int32		ioByteCount)
{
	ErrCode	err = errNone;
	int32	endOfWrite = this->GetMarker () + ioByteCount;
	
	if (endOfWrite > this->GetLength ())	// Need to grow Chunk
	{
		this->SetLength (endOfWrite);
	}
									// Copy bytes into Chunk
	if (ioByteCount > 0)
	{
		memmove ((char*) fData + this->GetMarker (), inBuffer, ioByteCount);
		this->SetMarker (ioByteCount, kStreamFromMarker);
	}
	
	return err;
}


// ---------------------------------------------------------------------------
//	¥ GetBytes
// ---------------------------------------------------------------------------
//	Read bytes from a EmStreamBlock to a buffer
//
//	Returns an error code and passes back the number of bytes actually
//	read, which may be less than the number requested if an error occurred.
//
//	Errors:
//		readErr		Attempt to read past the end of the EmStreamBlock

ErrCode
EmStreamBlock::GetBytes(
	void*	outBuffer,
	int32	ioByteCount)
{
	ErrCode	err = errNone;
									// Upper bound is number of bytes from
									//   marker to end
	if ((this->GetMarker () + ioByteCount) > this->GetLength ())
	{
		ioByteCount = this->GetLength () - this->GetMarker ();
		err = 1;
	}
									// Copy bytes from Handle into buffer

	memmove (outBuffer, (char*) fData + this->GetMarker (), ioByteCount);
	this->SetMarker (ioByteCount, kStreamFromMarker);
	
	return err;
}
