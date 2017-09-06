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

#ifndef EmROMTransfer_h
#define EmROMTransfer_h

class EmTransport;

#include "EmDlg.h"				// EmDlgRef
#include "EmTypes.h"			// ErrCode
#include "Miscellaneous.h"		// StMemory

class EmROMTransfer
{
	public:
								EmROMTransfer		(EmTransport*);
								~EmROMTransfer		(void);

		static void				ROMTransfer			(void);

		Bool					Continue			(EmDlgRef);
		void					Abort				(EmDlgRef);

		long					Size				(void);
		void*					Data				(void);

	private:
		Bool					WaitForTransport	(EmDlgRef);
		Bool					ReadSomeData		(EmDlgRef);
		uint8					HandleNewBlock		(void);
		uint8					CheckForTimeouts	(void);
		void					BufferPendingData	(void);
		Bool					HaveValidBlock		(void);
		Bool					HaveEntireROM		(void);
		Bool					ValidXModemBlock	(const uint8* block);
		void	 				SendByte			(uint8 byte);
		void					UpdateProgress		(EmDlgRef, long, long, long);
		static void				ResetSerialPort		(EmTransport* oldPort,
													 EmTransport* serPort);

	private:
		int						fState;
		EmTransport*			fTransport;

		long					fROMSize;
		long					fROMRead;
		StMemory				fROMBuffer;

		Bool					fHaveFirstBlock;
		Bool					fHaveLastBlock;
		uint8					fLastValidBlock;

		long					fProgressCaption;
		long					fProgressValue;
		long					fProgressMax;
		uint32					fProgressLastUpdate;
		enum { kProgressTimeout = 300 };	// Milliseconds

		enum { kTimeout = 1 * 1000 };	// Milliseconds
		uint32					fTimeoutBase;

		enum { kXModemBlockSize = 1024L };
		enum { kBufferSize = 2 * kXModemBlockSize };
		uint8					fTempBuffer[kBufferSize];
		long					fTempBufferOffset;
};


#endif	/* EmROMTransfer_h */
