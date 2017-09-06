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

#ifndef _SESSIONFILE_H_
#define _SESSIONFILE_H_

#include "ChunkFile.h"			// ChunkFile
#include "EmDevice.h"			// EmDevice
#include "EmStructs.h"			// Configuration, RGBType
#include "Platform.h"			// Platform

struct HwrJerryPLDType;
struct HwrM68328Type;
struct HwrM68EZ328Type;
struct HwrM68VZ328Type;
struct HwrM68SZ328Type;
struct regstruct;
struct SED1375RegsType;
struct SED1376RegsType;

class SessionFile
{
	public:
		typedef uint32 BugFixes;
		enum BugFix
		{
			kBugByteswappedStructs = 1	// Fixes bug where SED1375RegsType, HwrDBallEZType, HwrDBallType were
										// stored in host-endian format.
		};

	public:
								SessionFile				(ChunkFile& f);
								~SessionFile			(void);

		// ---------- Reading ----------

		Bool					ReadDevice				(EmDevice&);

		Bool					ReadROMFileReference	(EmFileRef&);

		Bool					ReadDBallRegs			(regstruct&);
		Bool					ReadHwrDBallType		(HwrM68328Type&);
		Bool					ReadHwrDBallEZType		(HwrM68EZ328Type&);
		Bool					ReadHwrDBallVZType		(HwrM68VZ328Type&);
		Bool					ReadDBallState			(Chunk& chunk) { return fFile.ReadChunk (kDBState, chunk); }
		Bool					ReadDBallEZState		(Chunk& chunk) { return fFile.ReadChunk (kDBEZState, chunk); }
		Bool					ReadDBallVZState		(Chunk& chunk) { return fFile.ReadChunk (kDBEZState, chunk); }

		Bool					ReadSED1375RegsType		(SED1375RegsType&);
		Bool					ReadSED1375Image		(void*);
		Bool					ReadSED1375Palette		(uint16 [256]);

		Bool					ReadSED1376RegsType		(SED1376RegsType&);
		Bool					ReadSED1376Palette		(RGBType [256]);

		Bool					ReadMediaQRegsType		(Chunk& chunk) { return fFile.ReadChunk (kMediaQRegs, chunk); }
		Bool					ReadMediaQImage			(void*);
		Bool					ReadMediaQPalette		(RGBType [256]);

		Bool					ReadPLDRegsType			(HwrJerryPLDType&);

		Bool					ReadGremlinInfo			(Chunk& chunk) { return fFile.ReadChunk (kGremlinInfo, chunk); }
		Bool					ReadGremlinHistory		(Chunk& chunk) { return this->ReadChunk (kGremlinHistory, chunk, kGzipCompression); }
		Bool					ReadDebugInfo			(Chunk& chunk) { return fFile.ReadChunk (kDebugInfo, chunk); }
		Bool					ReadMetaInfo			(Chunk& chunk) { return fFile.ReadChunk (kMetaInfo, chunk); }
		Bool					ReadPatchInfo			(Chunk& chunk) { return fFile.ReadChunk (kPatchInfo, chunk); }
		Bool					ReadProfileInfo			(Chunk& chunk) { return fFile.ReadChunk (kProfileInfo, chunk); }
		Bool					ReadLoggingInfo			(Chunk& chunk) { return fFile.ReadChunk (kLoggingInfo, chunk); }
		Bool					ReadStackInfo			(Chunk& chunk) { return fFile.ReadChunk (kStackInfo, chunk); }
		Bool					ReadHeapInfo			(Chunk& chunk) { return fFile.ReadChunk (kHeapInfo, chunk); }

		Bool					ReadPlatformInfo		(Chunk& chunk) { return fFile.ReadChunk (kPlatformInfo, chunk); }
		Bool					ReadPlatformInfoMac		(Chunk& chunk) { return fFile.ReadChunk (kPlatformMac, chunk); }
		Bool					ReadPlatformInfoWin		(Chunk& chunk) { return fFile.ReadChunk (kPlatformWindows, chunk); }
		Bool					ReadPlatformInfoUnix	(Chunk& chunk) { return fFile.ReadChunk (kPlatformUnix, chunk); }

		Bool					ReadRAMImage			(void*);
		Bool					ReadMetaRAMImage		(void*);
		Bool					ReadMetaROMImage		(void*);

		Bool					ReadBugFixes			(BugFixes&);

		// ---------- Writing ----------

		void					WriteDevice				(const EmDevice&);

		void					WriteROMFileReference	(const EmFileRef&);

		void					WriteDBallRegs			(const regstruct&);
		void					WriteHwrDBallType		(const HwrM68328Type&);
		void					WriteHwrDBallEZType		(const HwrM68EZ328Type&);
		void					WriteHwrDBallVZType		(const HwrM68VZ328Type&);
		void					WriteDBallState			(const Chunk& chunk) { fFile.WriteChunk (kDBState, chunk); }
		void					WriteDBallEZState		(const Chunk& chunk) { fFile.WriteChunk (kDBEZState, chunk); }
		void					WriteDBallVZState		(const Chunk& chunk) { fFile.WriteChunk (kDBEZState, chunk); }

		void					WriteSED1375RegsType	(const SED1375RegsType&);
		void					WriteSED1375Image		(const void*, uint32);
		void					WriteSED1375Palette		(const uint16 [256]);

		void					WriteSED1376RegsType	(const SED1376RegsType&);
		void					WriteSED1376Palette		(const RGBType [256]);

		void					WriteMediaQRegsType		(const Chunk& chunk) { fFile.WriteChunk (kMediaQRegs, chunk); }
		void					WriteMediaQImage		(const void*, uint32);
		void					WriteMediaQPalette		(const RGBType [256]);

		void					WritePLDRegsType		(const HwrJerryPLDType&);

		void					WriteGremlinInfo		(const Chunk& chunk) { fFile.WriteChunk (kGremlinInfo, chunk); }
		void					WriteGremlinHistory		(const Chunk& chunk) { this->WriteChunk (kGremlinHistory, chunk, kGzipCompression); }
		void					WriteDebugInfo			(const Chunk& chunk) { fFile.WriteChunk (kDebugInfo, chunk); }
		void					WriteMetaInfo			(const Chunk& chunk) { fFile.WriteChunk (kMetaInfo, chunk); }
		void					WritePatchInfo			(const Chunk& chunk) { fFile.WriteChunk (kPatchInfo, chunk); }
		void					WriteProfileInfo		(const Chunk& chunk) { fFile.WriteChunk (kProfileInfo, chunk); }
		void					WriteLoggingInfo		(const Chunk& chunk) { fFile.WriteChunk (kLoggingInfo, chunk); }
		void					WriteStackInfo			(const Chunk& chunk) { fFile.WriteChunk (kStackInfo, chunk); }
		void					WriteHeapInfo			(const Chunk& chunk) { fFile.WriteChunk (kHeapInfo, chunk); }

		void					WritePlatformInfo		(const Chunk& chunk) { fFile.WriteChunk (kPlatformInfo, chunk); }
		void					WritePlatformInfoMac	(const Chunk& chunk) { fFile.WriteChunk (kPlatformMac, chunk); }
		void					WritePlatformInfoWin	(const Chunk& chunk) { fFile.WriteChunk (kPlatformWindows, chunk); }
		void					WritePlatformInfoUnix	(const Chunk& chunk) { fFile.WriteChunk (kPlatformUnix, chunk); }

		void					WriteRAMImage			(const void*, uint32);
		void					WriteMetaRAMImage		(const void*, uint32);
		void					WriteMetaROMImage		(const void*, uint32);

		void					WriteBugFixes			(const BugFixes&);

		// ---------- Other ----------

		Bool					ReadConfiguration		(Configuration&);
		long					GetRAMImageSize			(void);

		// As information is saved to the file, certain parts are recorded
		// here.  That way, this information can be save to the preference
		// file/registry so that newly created sessions can be based on the
		// most recently used settings.

		Configuration			GetConfiguration		(void) { return fCfg; }

		void					SetCanReload			(Bool);
		Bool					GetCanReload			(void);

		void					FixBug					(BugFix);
		Bool					IncludesBugFix			(BugFix);

	private:
		enum CompressionType
		{
			kNoCompression,
			kRLECompression,
			kGzipCompression
		};

		Bool					ReadChunk				(ChunkFile::Tag tag,
														 void*,
														 CompressionType);

		Bool					ReadChunk				(ChunkFile::Tag tag,
														 Chunk& chunk,
														 CompressionType);

		void					WriteChunk				(ChunkFile::Tag tag,
														 uint32,
														 const void*,
														 CompressionType);

		void					WriteChunk				(ChunkFile::Tag tag,
														 const Chunk& chunk,
														 CompressionType);

		// These functions access kROMAliasTag, kROMNameTag, kROMPathTag
		friend Bool Platform::ReadROMFileReference (ChunkFile&, EmFileRef&);
		friend void Platform::WriteROMFileReference (ChunkFile&, const EmFileRef&);

		enum
		{
			kDeviceType			= 'DTyp',	// Device type (Pilot 1000, PalmPilot, Palm III, etc.)
			kDeviceTypeString	= 'DStr',	// Device type as string (preferred)

			kROMAliasTag		= 'ROMa',	// Macintosh alias
			kROMNameTag			= 'ROMn',	// Simple file name
			kROMWindowsPathTag	= 'ROMp',	// Full file path (Windows format)
			kROMUnixPathTag	   	= 'ROMu',	// Full file path (Unix format)

			kCPURegs			= 'Creg',	// CPU registers (D0-D7, A0-A7, PC, CCR, SR, etc.).
			kDBRegs				= 'DB  ',	// Memory-mapped registers struct.
			kDBEZRegs			= 'DBEZ',	// Memory-mapped registers struct.
			kDBVZRegs			= 'DBVZ',	// Memory-mapped registers struct.
			kDBState			= 'DSt8',	// Extra state stored outside of the memory-mapped registers struct.
			kDBEZState			= 'ESt8',	// Extra state stored outside of the memory-mapped registers struct.
			kDBVZState			= 'VSt8',	// Extra state stored outside of the memory-mapped registers struct.

			kSED1375Regs		= '5reg',	// Memory-mapped registers struct.
			kSED1375Image		= '5ram',	// LCD buffer memory.
			kSED1375Palette		= '5clt',	// LCD color lookup table.

			kSED1376Regs		= '6reg',	// Memory-mapped registers struct.
			kSED1376Palette		= '6clt',	// LCD color lookup table.

			kMediaQRegs			= 'MQrg',	// Memory-mapped registers struct.
			kMediaQImage		= 'MQim',	// LCD buffer memory.
			kMediaQPalette		= 'MQcl',	// LCD color lookup table.

			kPLDRegs			= 'pld ',	// Memory-mapped registers struct.

			kGremlinInfo		= 'grem',	// Gremlin state
			kGremlinHistory		= 'hist',	// Gremlin event history
			kDebugInfo			= 'dbug',	// Debug state
			kMetaInfo			= 'meta',	// MetaMemory state
			kPatchInfo			= 'ptch',	// Trappatch state
			kProfileInfo		= 'prof',	// Profiling state
			kLoggingInfo		= 'log ',	// Standard LogStream data
			kStackInfo			= 'stak',	// List of currently known stacks
			kHeapInfo			= 'heap',	// Heap state

			kPlatformInfo		= 'plat',	// Information managed by the Platform sub-system that has analogs on all platforms.
			kPlatformMac		= 'mac ',	// Mac-specific information
			kPlatformWindows	= 'wind',	// Windows-specific information
			kPlatformUnix		= 'unix',	// Unix-specific information

			kTimeDelta			= 'Time',	// Delta between the actual time and the time set by
											// the user via the General preference panel.

			kRAMDataTag			= 'zram',	// gzip compressed RAM image
			kMetaRAMDataTag		= 'zmrm',	// gzip compressed meta-RAM image
			kMetaROMDataTag		= 'zmro',	// gzip compressed meta-ROM image

			kBugsTag			= 'bugz',	// bit flags indicating bug fixes in file format
			
			kRLERAMDataTag		= 'cram',	// RLE compressed RAM image - obsolete
			kRLEMetaRAMDataTag	= 'mram',	// RLE compressed meta-RAM image - obsolete
			kUncompRAMDataTag	= 'ram '	// Uncompressed RAM image - obsolete
		};

	private:
		ChunkFile&				fFile;
		Bool					fCanReload;
		Configuration			fCfg;
		bool					fReadBugFixes;
		bool					fChangedBugFixes;
		BugFixes				fBugFixes;
};

#endif	// _SESSIONFILE_H_
