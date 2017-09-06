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
#include "EmROMTransfer.h"

#include "EmDlg.h"					// DoTransferROM
#include "EmStreamFile.h"			// EmStreamFile
#include "EmTransport.h"			// EmTransport
#include "EmTransportSerial.h"		// EmTransportSerial
#include "ErrorHandling.h"			// Errors::ThrowIfError
#include "Platform.h"				// Platform::GetMilliseconds
#include "Strings.r.h"				// kStr_Waiting

/* Update for GCC 4 */
#include <stdlib.h>
#include <string.h>


/*
	Notes on the XModem/YModem implementation used in this file:

	Basic XModem is dead simple:

		1. Sender waits for NAK from receiver.
		2. Sender sends SOH, block#, complement of block#
		3. Sender sends 128 bytes of data.
		3. Sender sends one-byte sum of those 128 bytes as checksum.
		4. Receiver sends ACK or NAK.
		5. If NAK, go back to step 2.
		6. If ACK, increment block# and go back to step 2.
		7. At end of file, sender sends EOT.
		8. Receiver acknowledges EOT with ACK.

	There is an XModem variant called Ymodem that sends an additional block at
	the start of the protocol - this block contains the file name and size,
	among other things.
*/


const int	kXModemBlockSize	= 1024;	// 1k-XModem variant
const int	kXModemBufferSize	= 1029;	// 1k-XModem variant
const char	kXModemSoh			= 1;	// start of block header
const char	kXModemEof			= 4;	// end of file signal
const char	kXModemAck			= 6;	// acknowledge
const char	kXModemNak			= 21;	// negative acknowledge (resend packet)
const char	kXModemCan			= 24;	// cancel
const char	kXModemNakCrc		= 'C';	// used instead of NAK for initial block


enum
{
	kWaitingForTransport,	// Waiting for transprt to come online
	kOpen					// Transport ready, first NakCrc sent
};


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::ROMTransfer
 *
 * DESCRIPTION:	Handle the entire process of downloading a ROM, from
 *				asking them for port/baud, to showing the progress
 *				dialog, to downloading the ROM, to saving it, and to
 *				handling the Cancel button.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmROMTransfer::ROMTransfer (void)
{
	// Run the dialog that sets up the download process.

	EmTransport::Config*	cfg;
	if (EmDlg::DoROMTransferQuery (cfg) == kDlgItemCancel)
		return;

	EmAssert (cfg != NULL);

	EmTransport*	oldTransport = NULL;
	EmTransport*	transport = NULL;

	try
	{
		// Close whatever might be on the selected port already.

		oldTransport = cfg->GetTransport ();
		if (oldTransport)
			Errors::ThrowIfError (oldTransport->Close ());

		// Open the port we want to use for downloading the ROM.

		transport = cfg->NewTransport ();
		Errors::ThrowIfError (transport->Open());

		// Create the ROM transfer object with this transport.

		EmROMTransfer	transferer (transport);

		// Run the progress window.

		EmDlgItemID	result = EmDlg::DoROMTransferProgress (transferer);

		if (result != kDlgItemCancel && transferer.HaveEntireROM ())
		{
			// Ask what name to save the ROM image to.

			EmFileRef		ref;
			EmFileTypeList	typeList (1, kFileTypeROM);
			if (EmDlg::DoPutFile (ref, "", "", typeList, "") == kDlgItemOK)
			{
				// Save the ROM image.

				EmStreamFile	stream (ref, kCreateOrEraseForUpdate,
										kFileCreatorEmulator, kFileTypeROM);

				stream.PutBytes (transferer.Data (), transferer.Size ());
			}
		}

		ResetSerialPort (oldTransport, transport);
	}
	catch (ErrCode errCode)
	{
		ResetSerialPort (oldTransport, transport);
		Errors::ReportIfError (kStr_CmdTransferROM, errCode, 0, false);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer c'tor
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	transport - transport object for low-level communications.
 *					We do not own it; the client deletes it.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmROMTransfer::EmROMTransfer (EmTransport* transport) :
	fState (kWaitingForTransport),
	fTransport (transport),
	fROMSize (0),
	fROMRead (0),
	fROMBuffer (),
	fHaveFirstBlock (false),
	fHaveLastBlock (false),
	fLastValidBlock ((uint8) -1),
	fProgressCaption (-1),
	fProgressValue (-1),
	fProgressMax (-1),
	fProgressLastUpdate (0),
//	fTempBuffer (),
	fTempBufferOffset (0)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer d'tor
 *
 * DESCRIPTION:	Destroy the object.  Delete all data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmROMTransfer::~EmROMTransfer (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::Continue
 *
 * DESCRIPTION:	Continually called to incrementally download a ROM.
 *
 * PARAMETERS:	dlg - reference to the progress dialog
 *
 * RETURNED:	True to continue downloading, false if done.
 *
 ***********************************************************************/

Bool EmROMTransfer::Continue (EmDlgRef dlg)
{
	switch (fState)
	{
		case kWaitingForTransport:
			return this->WaitForTransport (dlg);

		case kOpen:
			return this->ReadSomeData (dlg);
	}

	return false;	// Don't continue
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::Abort
 *
 * DESCRIPTION:	Abort the download.  Called when the user clicks on
 *				the Cancel button.
 *
 * PARAMETERS:	dlg - reference to the progress dialog
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmROMTransfer::Abort (EmDlgRef)
{
	// Nothing to do.  All memory is reclaimed in the destructor, and
	// the transport is closed and called by EmROMTransfer::ROMTransfer.
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::Size
 *
 * DESCRIPTION:	Return the size of the ROM.  Valid only if the ROM has
 *				been successfully downloaded (that is, Continue had
 *				been called until it finally returned false).
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Size of the ROM in bytes.
 *
 ***********************************************************************/

long EmROMTransfer::Size (void)
{
	return fROMSize;
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::Data
 *
 * DESCRIPTION:	Return a pointer to the ROM.  Valid only if the ROM has
 *				been successfully downloaded (that is, Continue had
 *				been called until it finally returned false).
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Pointer to the ROM image.
 *
 ***********************************************************************/

void* EmROMTransfer::Data (void)
{
	return fROMBuffer.Get();
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::WaitForTransport
 *
 * DESCRIPTION:	Called while waiting for the transport object to
 *				indicate that the transport is ready for sending
 *				data.  Until it is, we idle the progress indicator.
 *				When the transport is ready, prepare our state
 *				variables for downloading the ROM.
 *
 * PARAMETERS:	dlg - reference to the progress dialog
 *
 * RETURNED:	True to indicate that the Continue function should
 *				still be called.
 *
 ***********************************************************************/

Bool EmROMTransfer::WaitForTransport (EmDlgRef dlg)
{
	// Update the progress indicator.

	this->UpdateProgress (dlg, kStr_Waiting, 0, 0);

	if (fTransport->CanRead ())
	{
		// Start the transfer by sending kXModemNakCrc.

		this->SendByte (kXModemNakCrc);

		// Initialize our download state

		fROMSize			= 0;
		fROMRead			= 0;
		fHaveFirstBlock		= false;
		fHaveLastBlock		= false;
		fLastValidBlock		= (uint8) -1;
		fTimeoutBase		= Platform::GetMilliseconds ();
		fTempBufferOffset	= 0;

		// Switch over to the engine state

		fState = kOpen;
	}

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::ReadSomeData
 *
 * DESCRIPTION:	Called to incrementally download a ROM.  Reads data
 *				until we have enough for an XModem packet.  Examines
 *				the packet. If valid, adds it to our full ROM image and
 *				acks the packet.  If invalid, requests a resend.
 *
 * PARAMETERS:	dlg - reference to the progress dialog
 *
 * RETURNED:	True to indicate that the Continue function should
 *				still be called.  False if the entire ROM has now
 *				been downloaded.
 *
 ***********************************************************************/

Bool EmROMTransfer::ReadSomeData (EmDlgRef dlg)
{
	this->BufferPendingData ();

	uint8	ackChar;

	// If we have an entire new valid block by now, process it.

	if (this->HaveValidBlock ())
	{
		ackChar = this->HandleNewBlock ();
	}

	// EOF signal.

	else if (fTempBufferOffset > 0 && fTempBuffer[0] == kXModemEof)
	{
		ackChar = kXModemAck;
		fHaveLastBlock = true;
	}

	// Check for timeouts.

	else
	{
		// Returns:
		//
		//		kXModemNak		If timeout in middle of transfer
		//		kXModemNakCrc	If timeout and haven't started transfer, yet
		//		0				If no timeout

		ackChar = this->CheckForTimeouts ();
	}

	// Send the ack char and reset our timeout counter.

	if (ackChar)
	{
		this->SendByte (ackChar);
	}

	// Update the progress indicator.

	if (fHaveFirstBlock)
	{
		this->UpdateProgress (dlg, kStr_Transferring, fROMRead, fROMSize);
	}

	return !this->HaveEntireROM ();
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::HandleNewBlock
 *
 * DESCRIPTION:	A new ROM block has been downloaded and verified as
 *				valid.  Add it to our incrementally built ROM image
 *				and prepare for the next block.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The character with which to acknowledge the packet.
 *
 ***********************************************************************/

uint8 EmROMTransfer::HandleNewBlock (void)
{
	// If this is the first block, skip past the "file name" and
	// get the file (ROM) size.  Allocate a buffer to hold the image.

	uint8	receivedBlock = fTempBuffer[1];

	EmAssert (
		!fHaveFirstBlock ||
		fLastValidBlock == receivedBlock ||
		fLastValidBlock == (uint8) (receivedBlock - 1));

	if (!fHaveFirstBlock)
	{
		char* p = (char*) &fTempBuffer[3];
		p += strlen (p) + 1;
		fROMSize = atoi (p);
		fROMBuffer.Adopt ((char*) Platform::AllocateMemory (fROMSize));

		fHaveFirstBlock = true;
	}

	// Got a good block of data. Copy it into the ROM image.

	else if (fLastValidBlock != receivedBlock)
	{
		memcpy (fROMBuffer.Get () + fROMRead, &fTempBuffer[3], kXModemBlockSize);
		fROMRead += kXModemBlockSize;

#ifndef NDEBUG
		int	blocksRead = fROMRead / kXModemBlockSize;
		EmAssert ((blocksRead % 256) == receivedBlock);
		EmAssert (fROMRead <= fROMSize);
#endif
	}

	fLastValidBlock = receivedBlock;

	// Prepare the read buffer for the next block of data.

	fTempBufferOffset = 0;

	// Acknowledge this packet as good.

	return kXModemAck;
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::CheckForTimeouts
 *
 * DESCRIPTION:	Check to see if a certain amount of time has elapsed
 *				since we received a valid packet.  If it has, indicate
 *				that the sender resend the previous packet.
 *
 * PARAMETERS:	ackChar - the current ackChar the caller is considering
 *					sending back to the client.
 *
 * RETURNED:	The ackChar to *really* send back to the client.  If
 *				the timeout hasn't occurred, just send back what the
 *				caller sent us.  If it has timed out, return an ackChar
 *				based on whether we're in the middle of a download or
 *				just starting up.
 *
 ***********************************************************************/

uint8 EmROMTransfer::CheckForTimeouts (void)
{
	if (Platform::GetMilliseconds () - fTimeoutBase > kTimeout)
	{
		// We haven't received a good packet in some time.  Probably a dropped
		// character, or we're just starting the protocol.  If just starting, nak
		// with kXModemNakCrc 'C', else just use regular nak '\025'.

		fTempBufferOffset = 0;
		return fROMRead > 0 ? kXModemNak : kXModemNakCrc;
	}
	
	return 0;	// No timeout
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::BufferPendingData
 *
 * DESCRIPTION:	Transfer any data in the transport's buffer into our
 *				own private little buffer used to hold a single packet.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmROMTransfer::BufferPendingData (void)
{
	// Get some data.  Read as much as we can, but don't overflow
	// our local buffer.

	long	bytesInPort = fTransport->BytesInBuffer (kXModemBufferSize);

	if (bytesInPort > 0)
	{
		long	bytesToRead = min (bytesInPort, kBufferSize - fTempBufferOffset);
		ErrCode	err = fTransport->Read (bytesToRead, &fTempBuffer[fTempBufferOffset]);
		Errors::ThrowIfError (err);
		fTempBufferOffset += bytesToRead;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::HaveValidBlock
 *
 * DESCRIPTION:	Check to see if our little packet buffer now contains
 *				a full and valid packet
 *
 * PARAMETERS:	None
 *
 * RETURNED:	True if so.
 *
 ***********************************************************************/

Bool EmROMTransfer::HaveValidBlock (void)
{
	// Check to see whether we have enough data for a full block, and
	// that the block is valid, and it's the block number we're expecting

	Bool	haveEnoughData = fTempBufferOffset >= kXModemBufferSize;
	Bool	checksumValid = haveEnoughData && this->ValidXModemBlock (fTempBuffer);

	return haveEnoughData && checksumValid;
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::HaveEntireROM
 *
 * DESCRIPTION:	Return whether or now we have downloaded the entire ROM.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	True if so.
 *
 ***********************************************************************/

Bool EmROMTransfer::HaveEntireROM (void)
{
	return fHaveLastBlock;
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::ValidXModemBlock
 *
 * DESCRIPTION:	Validate a packet by doing a checksum and comparing it
 *				to the checksum that came with the packet.
 *
 * PARAMETERS:	block - the packet to checksum
 *
 * RETURNED:	True if valid.
 *
 ***********************************************************************/

Bool EmROMTransfer::ValidXModemBlock (const uint8* block)
{
	/*
	 * XModem-1k block layout is as follows:
	 *
	 * +--------------+
	 * | SOH = '\002' |
	 * +--------------+
	 * | Block number |
	 * +--------------+
	 * | Block compl. | Complement of block number
	 * +--------------+
	 * | 1024 bytes   |
	 * | of data	  |
	 * .	  . 	  .
	 * .	  . 	  .
	 * +--------------+
	 * | CRC hi byte  | CRC-16 of preceding 1024 bytes of data,
	 * +--------------+ plus two zero bytes
	 * | CRC lo byte  |
	 * +--------------+
	 */

	if (block[0] != kXModemSoh)
	{
		return false;
	}

	if ((block[1] ^ block[2]) != 0xFF)
	{
		return false;
	}

	uint16	calculatedCRC = Crc16CalcBlock ((void*) &block[3], kXModemBlockSize, 0);

	uint8	zeros[2] = {0, 0};
	calculatedCRC = Crc16CalcBlock (zeros, 2, calculatedCRC);

	uint16	xmittedCRC = (((uint16) (block[3 + kXModemBlockSize])) << 8) |
						block[3 + kXModemBlockSize + 1];

	return xmittedCRC == calculatedCRC;
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::SendByte
 *
 * DESCRIPTION:	Send a single byte to the client.
 *
 * PARAMETERS:	byte - the byte to send.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

#define CORRUPT_SENDS 0

#if CORRUPT_SENDS
static UInt32 PrvRange (UInt32 maxValue)
{
	static int initialized;
	if (!initialized)
	{
		initialized = true;
		srand (1);
	}

	return (rand () * maxValue) / (1UL + RAND_MAX);
}
#endif


void EmROMTransfer::SendByte (uint8 byte)
{
#if CORRUPT_SENDS
	// Drop a character.
	if (::PrvRange (100) < 5)
	{
		LogAppendMsg ("CORRUPTOR: dropping a character");
		fTimeoutBase = Platform::GetMilliseconds ();
		return;
	}

	// Corrupt a character.
	if (::PrvRange (100) < 5)
	{
		LogAppendMsg ("CORRUPTOR: changing a character");
		byte++;
	}
#endif

	long	len = 1;
	/*ErrCode	err =*/ fTransport->Write (len, &byte);

	// Any errors will just cause the retry/timeout mechanisms to kick in.
//	Errors::ThrowIfError (err);

	fTimeoutBase = Platform::GetMilliseconds ();
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::UpdateProgress
 *
 * DESCRIPTION:	Update the progress bar according to whether or not we
 *				have started the download process and, if so, how far
 *				along we are.  The progress information is updated only
 *				incrementally and only when it's been changed.
 *
 * PARAMETERS:	dlg - reference to progress dialog.
 *				caption - StrID of string for caption.
 *				value - number indicating how much of the ROM has been
 *					downloaded.
 *				max - number indicating how large the ROM image is.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmROMTransfer::UpdateProgress (EmDlgRef dlg, long caption, long value, long maxValue)
{
	if (fProgressCaption != caption)
	{
		fProgressCaption = caption;
		EmDlg::SetItemText (dlg, kDlgItemDlpMessage, caption);
	}

	uint32	now = Platform::GetMilliseconds ();
	uint32	delta = now - fProgressLastUpdate;
	bool	timeout = delta > kProgressTimeout;

	if (timeout)
	{
		// Divide values by 1024, since the Windows control deals only
		// with 16-bit values.  (This is fixed in later versions of
		// the progress control, but those aren't shipped with stock
		// Windows installations, yet.)

		if (fProgressMax != maxValue)
		{
			fProgressMax = maxValue;
			EmDlg::SetItemMax (dlg, kDlgItemDlpProgress, max (1L, maxValue / 1024));
		}

		if (fProgressValue != value)
		{
			fProgressValue = value;
			EmDlg::SetItemValue (dlg, kDlgItemDlpProgress, value / 1024);
		}

		fProgressLastUpdate = now;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmROMTransfer::ResetSerialPort
 *
 * DESCRIPTION:	We're done downloading the ROM (either successfully, or
 *				after an error or after the user Cancels).  Close up
 *				the transport object we were using and, if needed,
 *				restore the old transport object.
 *
 * PARAMETERS:	oldTransport - the transport object that was using the
 *					connection medium before we came along and usurped
 *					it.  Usually a serial port.
 *
 *				curTransport - the transport object used to download
 *					the ROM and that now needs to be closed.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmROMTransfer::ResetSerialPort (EmTransport* oldTransport, EmTransport* curTransport)
{
	// Close our stream.

	if (curTransport)
	{
		curTransport->Close ();
		delete curTransport;
	}

	// Reopen the stream used before we got in the way.

	if (oldTransport)
	{
		oldTransport->Open ();
	}
}
