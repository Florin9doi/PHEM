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
#include "SessionFile.h"

#include "Byteswapping.h"		// Canonical
#include "EmErrCodes.h"			// kError_InvalidDevice
#include "EmPalmStructs.h"		// EmProxySED1376RegsType
#include "EmStreamFile.h"		// EmStreamFile
#include "ErrorHandling.h"		// Errors::Throw
#include "Miscellaneous.h"		// StMemory, RunLengthEncode, GzipEncode, etc.
#include "UAE.h"				// regstruct


/***********************************************************************
 *
 * FUNCTION:	SessionFile constructor
 *
 * DESCRIPTION:	Initialize the SessionFile object.  Sets the fFile data
 *				member to refer to the given ChunkFile (which must
 *				exist for the life of the SessionFile).
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

SessionFile::SessionFile (ChunkFile& f) :
	fFile (f),
	fCanReload (false),
	fCfg (),
	fReadBugFixes (false),
	fChangedBugFixes (false),
	fBugFixes (0)
{
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile destructor
 *
 * DESCRIPTION:	Releases SessionFile resources.  Currently does nothing.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

SessionFile::~SessionFile (void)
{
	if (fChangedBugFixes)
	{
		this->WriteBugFixes (fBugFixes);
	}
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadROMFileReference
 *
 * DESCRIPTION:	Read a reference to a paired ROM file from the session
 *				file.  For robustness, several approaches are used to
 *				find a ROM file.  Because these approaches are platform-
 *				specific, most of the work is off-loaded to the Platform
 *				sub-system.  If the file reference can be read, it's
 *				recorded in the configuration record to be used the next
 *				time a new session is created.
 *
 * PARAMETERS:	f - reference to EmFileRef to receive the read value.
 *
 * RETURNED:	True if a value could be found, false otherwise.
 *
 ***********************************************************************/

Bool SessionFile::ReadROMFileReference (EmFileRef& f)
{
	// Look for a ROM reference using a platform-specific method.

	if (Platform::ReadROMFileReference (fFile, f))
		return true;

	// If a path can't be found, look for a simple ROM name.

	string	name;
	if (fFile.ReadString (SessionFile::kROMNameTag, name))
	{
		// First, look in the same directory as the session file.

		try
		{
			// Get the stream this ChunkFile is using and see if it's
			// a file-based stream.

			EmStream&		stream = fFile.GetStream ();
			EmStreamFile&	fileStream = dynamic_cast<EmStreamFile&> (stream);

			// If so, get the directory this file is in.

			EmFileRef		sessionRef = fileStream.GetFileRef ();
			EmDirRef		sessionParent = sessionRef.GetParent ();

			f = EmFileRef (sessionParent, name);

			if (f.Exists ())
				return true;
		}
		catch (...)	// Exception thrown if dynamic_cast fails.
		{
		}

		// If not there, look in the same directory as Poser itself.

		EmDirRef	emulatorDir = EmDirRef::GetEmulatorDirectory ();

		// Return this reference, even if the file doesn't exist.
		// That way, we can at least form error messages.

		f = EmFileRef (emulatorDir, name);
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadRAMImage
 *
 * DESCRIPTION:	Read the RAM image from the session file.  Attempts to
 *				read a number of versions of the image, including
 *				compressed and uncompressed versions.  If the image can
 *				be read, its size is recorded in the configuration
 *				record to be used the next time a new session is created.
 *
 * PARAMETERS:	image - reference to the pointer to receive the address
 *					of the RAM image.
 *
 *				size - reference to the integer to receive the size of
 *					the RAM image.
 *
 * RETURNED:	True if the image was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadRAMImage (void* image)
{
	Bool	result = this->ReadChunk (kRAMDataTag, image, kGzipCompression);

	if (!result)
		result = this->ReadChunk (kRLERAMDataTag, image, kRLECompression);

	if (!result)
		result = this->ReadChunk (kUncompRAMDataTag, image, kNoCompression);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadMetaRAMImage
 *
 * DESCRIPTION:	Read the MetaRAM image from the session file.  Attempts
 *				to read a number of versions of the image, including
 *				compressed and uncompressed versions.
 *
 * PARAMETERS:	image - reference to the pointer to receive the address
 *					of the MetaRAM image.
 *
 *				size - reference to the integer to receive the size of
 *					the MetaRAM image.
 *
 * RETURNED:	True if the image was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadMetaRAMImage (void* image)
{
	Bool	result = this->ReadChunk (kMetaRAMDataTag, image, kGzipCompression);

	if (!result)
		result = this->ReadChunk (kRLEMetaRAMDataTag, image, kRLECompression);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadMetaROMImage
 *
 * DESCRIPTION:	Read the MetaROM image from the session file.  Attempts
 *				to read a number of versions of the image, including
 *				compressed and uncompressed versions.
 *
 * PARAMETERS:	image - reference to the pointer to receive the address
 *					of the MetaROM image.
 *
 *				size - reference to the integer to receive the size of
 *					the MetaROM image.
 *
 * RETURNED:	True if the image was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadMetaROMImage (void* image)
{
	Bool	result = this->ReadChunk (kMetaROMDataTag, image, kGzipCompression);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadHwrDBallType
 *
 * DESCRIPTION:	Read the Dragonball hardware registers from the
 *				session file.
 *
 * PARAMETERS:	hwRegs - reference to the struct to receive the register
 *					data.
 *
 * RETURNED:	True if the data was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadHwrDBallType (HwrM68328Type& hwRegs)
{
	Chunk	chunk;
	if (fFile.ReadChunk (kDBRegs, chunk))
	{
		memcpy (&hwRegs, chunk.GetPointer (), sizeof (hwRegs));
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadHwrDBallEZType
 *
 * DESCRIPTION:	Read the DragonballEZ hardware registers from the
 *				session file.
 *
 * PARAMETERS:	hwRegs - reference to the struct to receive the register
 *					data.
 *
 * RETURNED:	True if the data was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadHwrDBallEZType (HwrM68EZ328Type& hwRegs)
{
	Chunk	chunk;
	if (fFile.ReadChunk (kDBEZRegs, chunk))
	{
		memcpy (&hwRegs, chunk.GetPointer (), sizeof (hwRegs));
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadHwrDBallVZType
 *
 * DESCRIPTION:	Read the DragonballVZ hardware registers from the
 *				session file.
 *
 * PARAMETERS:	hwRegs - reference to the struct to receive the register
 *					data.
 *
 * RETURNED:	True if the data was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadHwrDBallVZType (HwrM68VZ328Type& hwRegs)
{
	Chunk	chunk;
	if (fFile.ReadChunk (kDBVZRegs, chunk))
	{
		memcpy (&hwRegs, chunk.GetPointer (), sizeof (hwRegs));
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadHwrDBallSZType
 *
 * DESCRIPTION:	Read the DragonballSZ hardware registers from the
 *				session file.
 *
 * PARAMETERS:	hwRegs - reference to the struct to receive the register
 *					data.
 *
 * RETURNED:	True if the data was found and could be read in.
 *
 ***********************************************************************/



/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadDBallRegs
 *
 * DESCRIPTION:	Read the Dragonball CPU registers structure from the
 *				session file.
 *
 * PARAMETERS:	cpuRegs - reference to the struct to receive the register
 *					data.
 *
 * RETURNED:	True if the data was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadDBallRegs (regstruct& cpuRegs)
{
	Chunk	chunk;
	if (fFile.ReadChunk (kCPURegs, chunk))
	{
		memcpy (&cpuRegs, chunk.GetPointer (), sizeof (cpuRegs));
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadDeviceType
 *
 * DESCRIPTION:	Read the id of the device to emulate.  If the id can be
 *				read, it's recorded in the configuration record to be
 *				used the next time a new session is created.
 *
 * PARAMETERS:	v - reference to the EmDevice to receive the read value.
 *
 * RETURNED:	True if the value was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadDevice (EmDevice& v)
{
	v = EmDevice ();

	// Look up the device by the string.

	string	deviceTypeString;

	if (fFile.ReadString (kDeviceTypeString, deviceTypeString))
	{
		v = EmDevice (deviceTypeString);
	}

	return v.Supported ();
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadBugFixes
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	v - reference to the BugFixes to receive the read value.
 *
 * RETURNED:	True if the value was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadBugFixes (BugFixes& v)
{
	uint32	bits;
	Bool	result = fFile.ReadInt (kBugsTag, bits);

	if (result)
	{
		v = (BugFixes) bits;
	}

	return result;
}


Bool SessionFile::ReadSED1375RegsType (SED1375RegsType& sedRegs)
{
	Chunk	chunk;
	if (fFile.ReadChunk (kSED1375Regs, chunk))
	{
		memcpy (&sedRegs, chunk.GetPointer (), sizeof (sedRegs));
		return true;
	}

	return false;
}


Bool SessionFile::ReadSED1375Image (void* image)
{
	Bool	result = this->ReadChunk (kSED1375Image, image, kGzipCompression);

	return result;
}


Bool SessionFile::ReadSED1375Palette (uint16 palette[256])
{
	Chunk	chunk;
	if (fFile.ReadChunk (kSED1375Palette, chunk))
	{
		// Note: "sizeof (palette)" gives 4, not 512.
		int	size = 256 * sizeof (palette[0]);
		memcpy (palette, chunk.GetPointer (), size);
		return true;
	}

	return false;
}


Bool SessionFile::ReadSED1376RegsType (SED1376RegsType& sedRegs)
{
	Chunk	chunk;
	if (fFile.ReadChunk (kSED1376Regs, chunk))
	{
		memcpy (&sedRegs, chunk.GetPointer (), EmProxySED1376RegsType::GetSize ());
		return true;
	}

	return false;
}


Bool SessionFile::ReadSED1376Palette (RGBType palette[256])
{
	Chunk	chunk;
	if (fFile.ReadChunk (kSED1376Palette, chunk))
	{
		// Note: "sizeof (palette)" gives 4, not 512.
		int	size = 256 * sizeof (palette[0]);
		memcpy (palette, chunk.GetPointer (), size);
		return true;
	}

	return false;
}


// SessionFile::ReadMediaQRegsType is defined in SessionFile.h


Bool SessionFile::ReadMediaQImage (void* image)
{
	Bool	result = this->ReadChunk (kMediaQImage, image, kGzipCompression);

	return result;
}


Bool SessionFile::ReadMediaQPalette (RGBType palette[256])
{
	Chunk	chunk;
	if (fFile.ReadChunk (kMediaQPalette, chunk))
	{
		// Note: "sizeof (palette)" gives 4, not 512.
		int size = 256 * sizeof (palette[0]);
		memcpy (palette, chunk.GetPointer (), size);
		return true;
	}

	return false;
}


Bool SessionFile::ReadPLDRegsType (HwrJerryPLDType& pldRegs)
{
	Chunk	chunk;
	if (fFile.ReadChunk (kPLDRegs, chunk))
	{
		memcpy (&pldRegs, chunk.GetPointer (), sizeof (pldRegs));
		return true;
	}

	return false;
}


Bool SessionFile::ReadConfiguration (Configuration& cfg)
{
	if (!this->ReadDevice (cfg.fDevice))
		return false;

	if (!this->ReadROMFileReference (cfg.fROMFile))
		return false;

	cfg.fRAMSize = this->GetRAMImageSize ();
	if (cfg.fRAMSize == ChunkFile::kChunkNotFound)
		return false;

	cfg.fRAMSize /= 1024;

	return true;
}


long SessionFile::GetRAMImageSize (void)
{
	long	numBytes;

	Chunk	chunk;
	if (fFile.ReadChunk (kRAMDataTag, chunk) || fFile.ReadChunk (kRLERAMDataTag, chunk))
	{
		EmStreamChunk	s (chunk);
		s >> numBytes;
	}
	else
	{
		numBytes = fFile.FindChunk (kUncompRAMDataTag);
	}

	return numBytes;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteROMFileReference
 *
 * DESCRIPTION:	Write a reference to the ROM file to use for this
 *				session.  For robustness, the reference is written out
 *				in several different ways.  Because the way the file is
 *				later looked up is platform-dependent, most of the work
 *				is off-loaded to the Platform sub-system.
 *
 * PARAMETERS:	f - reference to the EmFileRef for the ROM file.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteROMFileReference (const EmFileRef& f)
{
	// Save a ROM reference using a platform-specific method.

	Platform::WriteROMFileReference (fFile, f);

	// Save the name of the ROM file.

	string	romFileName = f.GetName ();
	fFile.WriteString (SessionFile::kROMNameTag, romFileName);

	// Remember that we were using this ROM file most recently.

	fCfg.fROMFile = f;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteRAMImage
 *
 * DESCRIPTION:	Write the given data as the RAM image for the session
 *				file.  The data is written using the default compression.
 *
 * PARAMETERS:	image - pointer to the data to be written.  No munging
 *					of this data is performed; it is expected that any
 *					byteswapping has already taken place.
 *
 *				size - number of bytes in the image.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteRAMImage (const void* image, uint32 size)
{
	this->WriteChunk (kRAMDataTag, size, image, kGzipCompression);
	fCfg.fRAMSize = size / 1024;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteMetaRAMImage
 *
 * DESCRIPTION:	Write the given data as the MetaRAM image for the session
 *				file.  The data is written using the default compression.
 *
 * PARAMETERS:	image - pointer to the data to be written.  No munging
 *					of this data is performed; it is expected that any
 *					byteswapping has already taken place.
 *
 *				size - number of bytes in the image.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteMetaRAMImage (const void* image, uint32 size)
{
	this->WriteChunk (kMetaRAMDataTag, size, image, kGzipCompression);
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteMetaROMImage
 *
 * DESCRIPTION:	Write the given data as the MetaROM image for the session
 *				file.  The data is written using the default compression.
 *
 * PARAMETERS:	image - pointer to the data to be written.  No munging
 *					of this data is performed; it is expected that any
 *					byteswapping has already taken place.
 *
 *				size - number of bytes in the image.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteMetaROMImage (const void* image, uint32 size)
{
	this->WriteChunk (kMetaROMDataTag, size, image, kGzipCompression);
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteHwrDBallType
 *
 * DESCRIPTION:	Write the Dragonball hardware registers to the session
 *				file.
 *
 * PARAMETERS:	hwRegs - the DB registers to write to disk.  No munging
 *					of this data is performed; it is expected that any
 *					byteswapping has already taken place.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteHwrDBallType (const HwrM68328Type& hwRegs)
{
	fFile.WriteChunk (kDBRegs, sizeof (hwRegs), &hwRegs);
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteHwrDBallEZType
 *
 * DESCRIPTION:	Write the Dragonball EZ hardware registers to the session
 *				file.
 *
 * PARAMETERS:	hwRegs - the DB registers to write to disk.  No munging
 *					of this data is performed; it is expected that any
 *					byteswapping has already taken place.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteHwrDBallEZType (const HwrM68EZ328Type& hwRegs)
{
	fFile.WriteChunk (kDBEZRegs, sizeof (hwRegs), &hwRegs);
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteHwrDBallVZType
 *
 * DESCRIPTION:	Write the Dragonball VZ hardware registers to the session
 *				file.
 *
 * PARAMETERS:	hwRegs - the DB registers to write to disk.  No munging
 *					of this data is performed; it is expected that any
 *					byteswapping has already taken place.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteHwrDBallVZType (const HwrM68VZ328Type& hwRegs)
{
	fFile.WriteChunk (kDBVZRegs, sizeof (hwRegs), &hwRegs);
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteHwrDBallSZType
 *
 * DESCRIPTION:	Write the Dragonball SZ hardware registers to the session
 *				file.
 *
 * PARAMETERS:	hwRegs - the DB registers to write to disk.  No munging
 *					of this data is performed; it is expected that any
 *					byteswapping has already taken place.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/



/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteDBallRegs
 *
 * DESCRIPTION:	Write the Dragonball CPU registers to the session file.
 *
 * PARAMETERS:	cpuRegs - the registers to write to disk. No munging
 *					of this data is performed; it is expected that any
 *					byteswapping has already taken place.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteDBallRegs (const regstruct& cpuRegs)
{
	fFile.WriteChunk (kCPURegs, sizeof (cpuRegs), &cpuRegs);
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteDeviceType
 *
 * DESCRIPTION:	Write the id of the device to emulate.
 *
 * PARAMETERS:	v - id of the device to emulate.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteDevice (const EmDevice& v)
{
	fFile.WriteString (kDeviceTypeString, v.GetIDString ());

	// Remember that we were using this device most recently.

	fCfg.fDevice = v;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteBugFixes
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	v - flags of fixed bugs.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteBugFixes (const BugFixes& v)
{
	fFile.WriteInt (kBugsTag, (uint32) v);
}


void SessionFile::WriteSED1375RegsType (const SED1375RegsType& sedRegs)
{
	fFile.WriteChunk (kSED1375Regs, sizeof (sedRegs), &sedRegs);
}

void SessionFile::WriteSED1375Image (const void* image, uint32 size)
{
	this->WriteChunk (kSED1375Image, size, image, kGzipCompression);
}

void SessionFile::WriteSED1375Palette (const uint16 palette[256])
{
	// Note: "sizeof (palette)" gives 4, not 512.
	int	size = 256 * sizeof (palette[0]);
	fFile.WriteChunk (kSED1375Palette, size, palette);
}

void SessionFile::WriteSED1376RegsType (const SED1376RegsType& sedRegs)
{
	fFile.WriteChunk (kSED1376Regs, EmProxySED1376RegsType::GetSize (), &sedRegs);
}

void SessionFile::WriteSED1376Palette (const RGBType palette[256])
{
	// Note: "sizeof (palette)" gives 4, not 512.
	int	size = 256 * sizeof (palette[0]);
	fFile.WriteChunk (kSED1376Palette, size, palette);
}

// SessionFile::WriteMediaQRegsType is defined in SessionFile.h

void SessionFile::WriteMediaQImage (const void* image, uint32 size)
{
	this->WriteChunk (kMediaQImage, size, image, kGzipCompression);
}

void SessionFile::WriteMediaQPalette (const RGBType palette[256])
{
	// Note: "sizeof (palette)" gives 4, not 512.
	int size = 256 * sizeof (palette[0]);
	fFile.WriteChunk (kMediaQPalette, size, palette);
}

void SessionFile::WritePLDRegsType (const HwrJerryPLDType& pldRegs)
{
	fFile.WriteChunk (kPLDRegs, sizeof (pldRegs), &pldRegs);
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::SetCanReload
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void SessionFile::SetCanReload (Bool val)
{
	fCanReload = val;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::GetCanReload
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool SessionFile::GetCanReload (void)
{
	return fCanReload;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::FixBug
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void SessionFile::FixBug (BugFix val)
{
	fBugFixes |= val;
	fChangedBugFixes = true;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::IncludesBugFix
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool SessionFile::IncludesBugFix (BugFix val)
{
	if (!fReadBugFixes)
	{
		this->ReadBugFixes (fBugFixes);
		fReadBugFixes = true;
	}

	return (fBugFixes & val) != 0;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::ReadChunk
 *
 * DESCRIPTION:	Read an arbitrary chunk of data, utilizing the given
 *				type of compression, returning a pointer to the read
 *				data and the size of the data.
 *
 * PARAMETERS:	tag - marker identifying the data to be read.
 *
 *				size - reference to the integer to receive the number
 *					of bytes in the data.  This size may not be the same
 *					as the size of the chunk if the chunk is compressed.
 *
 *				image - reference to the pointer to receive the address
 *					of the read data.
 *
 *				compType - type of compression used for this chunk.  The
 *					caller is expected to know what kind of compression
 *					is being used; the compression type is not stored
 *					along with the chunk.
 *
 * RETURNED:	True if the image was found and could be read in.
 *
 ***********************************************************************/

Bool SessionFile::ReadChunk (ChunkFile::Tag tag, void* image,
							 CompressionType compType)
{
	// Get the size of the chunk.

	long	chunkSize = fFile.FindChunk (tag);
	if (chunkSize == ChunkFile::kChunkNotFound)
	{
		return false;
	}

	if (chunkSize)
	{
		// If no compression is being used, just read the data.

		if (compType == kNoCompression)
		{
			fFile.ReadChunk (chunkSize, image);
		}

		// The data is compressed.

		else
		{
			// Use the chunk size to create a buffer.

			StMemory	packedImage (chunkSize);

			// Read the chunk into memory.

			fFile.ReadChunk (chunkSize, packedImage.Get ());

			// The size of the unpacked image is stored as the first 4 bytes
			// of the chunk.

			long		unpackedSize = *(long*) packedImage.Get ();
			Canonical (unpackedSize);

			// Get pointers to the source (packed) data and
			// destination (unpacked) data.

			void*	src = packedImage.Get () + sizeof (long);
			void*	dest = image;

			// Decompress the data into the dest buffer.

			if (compType == kGzipCompression)
				::GzipDecode (&src, &dest, chunkSize - sizeof (long), unpackedSize);
			else
				::RunLengthDecode (&src, &dest, chunkSize - sizeof (long), unpackedSize);
		}
	}
	
	return true;
}


Bool SessionFile::ReadChunk (ChunkFile::Tag tag, Chunk& chunk,
							 CompressionType compType)
{
	// Get the size of the chunk.

	long	chunkSize = fFile.FindChunk (tag);
	if (chunkSize == ChunkFile::kChunkNotFound)
	{
		return false;
	}

	if (chunkSize)
	{
		// If no compression is being used, just read the data.

		if (compType == kNoCompression)
		{
			chunk.SetLength (chunkSize);
			fFile.ReadChunk (chunkSize, chunk.GetPointer ());
		}

		// The data is compressed.

		else
		{
			// Use the chunk size to create a buffer.

			StMemory	packedImage (chunkSize);

			// Read the chunk into memory.

			fFile.ReadChunk (chunkSize, packedImage.Get ());

			// The size of the unpacked image is stored as the first 4 bytes
			// of the chunk.

			long		unpackedSize = *(long*) packedImage.Get ();
			Canonical (unpackedSize);

			chunk.SetLength (unpackedSize);

			// Get pointers to the source (packed) data and
			// destination (unpacked) data.

			void*	src = packedImage.Get () + sizeof (long);
			void*	dest = chunk.GetPointer ();

			// Decompress the data into the dest buffer.

			if (compType == kGzipCompression)
				::GzipDecode (&src, &dest, chunkSize - sizeof (long), unpackedSize);
			else
				::RunLengthDecode (&src, &dest, chunkSize - sizeof (long), unpackedSize);
		}
	}

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	SessionFile::WriteChunk
 *
 * DESCRIPTION:	Write an arbitrary chunk of data, utilizing the given
 *				type of compression.
 *
 * PARAMETERS:	tag - marker used to later retrieve the data.
 *
 *				size - number of bytes in the image to write.
 *
 *				image - pointer to the image to write.
 *
 *				compType - type of compression to use.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SessionFile::WriteChunk (ChunkFile::Tag tag, uint32 size,
				const void* image, CompressionType compType)
{
	// No compression to be used; just write the data out as-is.

	if (compType == kNoCompression)
	{
		fFile.WriteChunk (tag, size, image);
	}

	// Use some form of compression.

	else
	{
		// Get the worst-case size for the compressed data.

		long		worstPackedSize = sizeof (long) +
						((compType == kGzipCompression)
							? ::GzipWorstSize (size)
							: ::RunLengthWorstSize (size));

		// Create a new buffer to hold the compressed data.

		StMemory	packedImage (worstPackedSize);

		// Write out the uncompressed size of the data as the first
		// 4 bytes of the chunk.

		Canonical (size);
		*(long*) packedImage.Get () = size;
		Canonical (size);

		// Get pointers to the source (unpacked) data and
		// destination (packed) data.

		void*	src = (void*) image;
		void*	dest = packedImage.Get () + sizeof (long);

		// Compress the data.

		if (compType == kGzipCompression)
			::GzipEncode (&src, &dest, size, worstPackedSize);
		else
			::RunLengthEncode (&src, &dest, size, worstPackedSize);

		// Calculate the compressed size of the data.

		long	packedSize = (char*) dest - (char*) packedImage;

		// Write the compressed data to the file.

		fFile.WriteChunk (tag, packedSize, packedImage.Get ());
	}
}

void SessionFile::WriteChunk (ChunkFile::Tag tag, const Chunk& chunk, CompressionType compType)
{
	this->WriteChunk (tag, chunk.GetLength (), chunk.GetPointer (), compType);
}
