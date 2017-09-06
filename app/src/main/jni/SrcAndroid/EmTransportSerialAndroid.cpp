/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmTransportSerialAndroid.h"

#include "Logging.h"			// LogSerial
#include "Platform.h"			// Platform::AllocateMemory

#include <errno.h>				// errno
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>				// open(), close()
#include <termios.h>			// struct termios

#include "PHEMNativeIF.h"

#define PRINTF	if (!LogSerial ()) ; else LogAppendMsg


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostConstruct
 *
 * DESCRIPTION:	Construct platform-specific objects/data.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The platform-specific serial object.
 *
 ***********************************************************************/

void EmTransportSerial::HostConstruct (void)
{
	fHost = new EmHostTransportSerial;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostDestruct
 *
 * DESCRIPTION:	Destroy platform-specific objects/data.
 *
 * PARAMETERS:	hostData - The platform-specific serial object.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSerial::HostDestruct (void)
{
	delete fHost;
	fHost = NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostOpen
 *
 * DESCRIPTION:	Open the serial port in a platform-specific fashion.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportSerial::HostOpen (void)
{
	ErrCode	err = fHost->OpenCommPort (fConfig);

	if (!err)
		err = fHost->CreateCommThreads (fConfig);

	if (err) {
             PHEM_Log_Msg("HostOpen problem making threads.");
		this->HostClose ();
        } else {
             PHEM_Log_Msg("HostOpen err:");
             PHEM_Log_Place(err);
        }
	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostClose
 *
 * DESCRIPTION:	Close the serial port in a platform-specific fashion.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportSerial::HostClose (void)
{
	ErrCode err;

	err = fHost->DestroyCommThreads ();
	err = fHost->CloseCommPort ();

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostRead
 *
 * DESCRIPTION:	Read bytes from the port in a platform-specific fashion.
 *
 * PARAMETERS:	len - maximum number of bytes to read.
 *				data - buffer to receive the bytes.
 *
 * RETURNED:	0 if no error.  The number of bytes actually read is
 *				returned in len if there was no error.
 *
 ***********************************************************************/

ErrCode EmTransportSerial::HostRead (long& len, void* data)
{
	fHost->GetIncomingData (data, len);

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostWrite
 *
 * DESCRIPTION:	Write bytes to the port in a platform-specific fashion.
 *
 * PARAMETERS:	len - number of bytes in the buffer.
 *				data - buffer containing the bytes.
 *
 * RETURNED:	0 if no error.  The number of bytes actually written is
 *				returned in len if there was no error.
 *
 ***********************************************************************/

ErrCode EmTransportSerial::HostWrite (long& len, const void* data)
{
	fHost->PutOutgoingData (data, len);

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostBytesInBuffer
 *
 * DESCRIPTION:	Returns the number of bytes that can be read with the
 *				Read method.  Note that bytes may be received in
 *				between the time BytesInBuffer is called and the time
 *				Read is called, so calling the latter with the result
 *				of the former is not guaranteed to fetch all received
 *				and buffered bytes.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Number of bytes that can be read.
 *
 ***********************************************************************/

long EmTransportSerial::HostBytesInBuffer (long /*minBytes*/)
{
	return fHost->IncomingDataSize ();
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostSetConfig
 *
 * DESCRIPTION:	Configure the serial port in a platform-specific
 *				fashion.  The port is assumed to be open.
 *
 * PARAMETERS:	config - configuration information.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportSerial::HostSetConfig (const ConfigSerial& config)
{
	PRINTF ("EmTransportSerial::HostSetConfig: Setting settings.");

	ErrCode	err = errNone;

	struct termios		io;

        // For the null device (passes GPS data from Android), just pretend it's all good.
        if (fHost->fCommHandle < 0) {
          PHEM_Log_Msg("HostSetConfig, returning early.");
	  return err;
        } else {
          PHEM_Log_Msg("HostSetConfig, normal:");
          PHEM_Log_Place(fHost->fCommHandle);
        }

	// Get the current settings.

	if (tcgetattr (fHost->fCommHandle, &io) == -1)		// macro for ioctl (..., TCGETA, ...) call
	{
		err = errno;
		return err;
	}

	// One article on the net ("Serial Programming Guide for POSIX Compliant
	// Operating Systems", <http://www.easysw.com/~mike/serial/serial.html>)
	// recommends to *always* set these.

	io.c_cflag |= (CREAD | CLOCAL);

	// An execllent article on serial programming under UNIX ("Linux Serial Port
	// Programming Mini-Howto") says to turn off these for "raw" (as opposed to
	// "canonical") mode.

	io.c_lflag &= ~(ICANON | ECHO | ISIG);

	// The UNIX Programming FAQ (<www://www.faqs.org/faqs/unix-faq/programmer/faq/>)
	// recommends just setting all the c_iflags and c_oflags to zero.

	io.c_iflag = io.c_oflag = 0;

	// Set the baud

	int hostBaud = fHost->GetBaud (config.fBaud);
	cfsetospeed (&io, hostBaud);
	cfsetispeed (&io, hostBaud);

	// Set the parity

	if (config.fParity == EmTransportSerial::kNoParity)
	{
		io.c_cflag &= ~PARENB;
	}
	else
	{
		io.c_cflag |= PARENB;

		if (config.fParity == EmTransportSerial::kOddParity)
		{
			io.c_cflag |= PARODD;
		}
		else
		{
			io.c_cflag &= ~PARODD;
		}
	}

	// Set the data bits

	io.c_cflag &= ~CSIZE;
	io.c_cflag |= fHost->GetDataBits (config.fDataBits);

	// Set the stop bits

	if (config.fStopBits == 2)
	{
		io.c_cflag |= CSTOPB;
	}
	else
	{
		io.c_cflag &= ~CSTOPB;
	}


	// Set the hardware handshaking
	//
	// Note that I used to have the stuff in the #if 0.
	// However, on the Windows side, Olivier Naudan argues:
	//
	//	But I don't agree with: dcb.fRtsControl  = config.fHwrHandshake ?
	//		RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
	//	As hardware overrun can't be emulated, because of Poser timing that
	//	can't be accurate, I believe that fRtsControl should always be set to
	//	RTS_CONTROL_HANDSHAKE. So bytes will always arrive succesfully to host,
	//	and only software overrun could happen if PalmOS does not control the
	//	emulated RTS.

	/*
		Additional commentary

		From Alexandre Duret-Lutz <aduret@enst.fr>:

			I have a Unix application which connect to a serial device, and
			configure the serial line with the following parameter.

			  ...
			  int fd = open ("/dev/ttyS0", O_RDWR);
			  ...
			  tty.c_cflag = CS8 | B9600 | CLOCAL | CREAD;  // No CRTSCTS.
			  ...
			  tcsetattr (fd, TCSANOW, &tty);
			  ...

			I also have the same application adapted to the Palm, which
			connect to the same device, and initialize the line with

			  ...
			  UInt32 serFlags = srmSettingsFlagStopBits1 | srmSettingsFlagBitsPerChar8;
			  UInt16 serFlagsLen = sizeof serFlags;
			  ...
			  SrmOpen (serPortCradlePort, 9600, &port);
			  ...
			  SrmControl (port, srmCtlSetFlags, &serFlags, &serFlagsLen);
			  ....

			Both applications work fine.  But while the latter work on the
			Palm it doesn't work in Pose.  (The data seems to be sent but the
			serial device never answer and my appl eventually timeout on
			SrmReceive.)

			Since 3.0a8, Pose is turning CRTSCTS on for all serial
			connections, I beleive this is killing me.  Indeed if I change
			this behavior back to how it was in 3.0a7 my appl runs all fine.

			--- SrcUnix/EmTransportSerialUnix.cpp.old	Wed Apr 11 14:29:45 2001
			+++ SrcUnix/EmTransportSerialUnix.cpp	Wed Apr 11 14:30:00 2001
			@@ -283,7 +283,7 @@
				//	and only software overrun could happen if PalmOS does not control the
				//	emulated RTS.

			-#if 0
			+#if 1
				if (config.fHwrHandshake)
				{
					io.c_cflag |= CRTSCTS;


			I guess this might be related to Dave Sours' recent postings, 
			as his appl broke when he switched from 3.0a7 and 3.1.
			-- 
			Alexandre Duret-Lutz

		To which Olivier replies:

			Keith,

			Apparently, the change in Unix UART emulation is not exactly equivallent
			to the Windows one. The change I proposed activates RTS handshaking
			only, not CTS handshaking, whereas the change in Unix version activate
			both (io.c_cflag |= CRTSCTS)

			If opposite device does not handle flow control, or if the serial cable
			is not completely wired, the CTS handshaking MUST be deactivated.
			Otherwise, the host (Windows or Unix) will never send bytes to the other
			device, because CTS will always be deasserted.

			On the other hand, RTS handshaking can be activated safely on the host.
			This signal is taken into account by the other device, or simply ignored
			if it does not handle RTS handshaking.

			So on the Unix version, the flag must also be set to RTS-only, if
			possible. Otherwise, just revert the change.

			One point is not clear to me. The developper says:

				"The data seems to be sent but the serial device never answer and my
				appl eventually timeout on SrmReceive."

			In fact, since the other device does not handle flow control, the data
			must not be sent due to CTS deasserted. As a result, the SrmReceive
			never get an answer.

			To check this, you may ask this developper to use a hardware serial spy
			(a box with blinking LEDs indicating signal states) and check if the TD
			(Transmit Data) led blink. It shouldn't. If it does, my guess is wrong.
			Another option is to use a software serial spy. On Windows NT, I use
			"PortMon". It shows low-level calls to NT serial driver.

			BTW I proposed to always activate RTS handshaking on the host because in
			the past some other tasks running may slow Poser up to the point of an
			overrun. With this change, it does not occur any more if the other
			device handle flow control, and I think it is safe.
			I can't imagine a device that does not handle flow control, but that
			still rely on a particular state of CTS/RTS.

			Moreover, software overrun can still occur in the emulated UART if
			PalmOS does not deassert RTS. Hardware overrun can't be emulated because
			of intermediate buffering on the host and hazardous timing.

			To help developpers understand the change, I propose the following text:

				"dcb.fRtsControl  = RTS_CONTROL_HANDSHAKE;

				Whatever state of emulated UART is, we always activate RTS flow control
				on the host. This would prevent host buffer overrun, in case the host is
				busy and in case the opposite device handle RTS handshaking. This is
				safe because if the opposite device does not handle flow control, it
				will just ignore that signal.

				However, we activate CTS handshaking only if IGNORE_CTS register is not
				set. If the opposite device does not handle CTS and if we rely on CTS
				state, data will never be sent."

			To conclude, try using CTS-only flag on Unix version. If the CTS-only
			flag does not exist, just revert the change. Also check the Macintosh
			version. Ask the developper to investigate to check if data are really
			sent or not. They shouldn't. 

			Hope this helps :-)

		All of that is the long-winded explanation of why CRTSCTS is deasserted
		when hardware handshaking is off.
	*/

#if 1
	if (config.fHwrHandshake)
	{
		io.c_cflag |= CRTSCTS;
	}
	else
	{
		io.c_cflag &= ~CRTSCTS;
	}
#else
	io.c_cflag |= CRTSCTS;
#endif

	// Write out the changed settings.
	if (tcsetattr (fHost->fCommHandle, TCSANOW, &io) == -1)		// macro for ioctl (TCSETS, &io);
	{
		err = errno;
		return err;
	}

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostSetRTS
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSerial::HostSetRTS (RTSControl /*state*/)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostSetDTR
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSerial::HostSetDTR (Bool /*state*/)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostSetBreak
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSerial::HostSetBreak (Bool /*state*/)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostGetCTS
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Bool EmTransportSerial::HostGetCTS (void)
{
	return false;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostGetDSR
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Bool EmTransportSerial::HostGetDSR (void)
{
	return false;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostGetPortNameList
 *
 * DESCRIPTION:	Return the list of serial ports on this computer.  Used
 *				to prepare a menu of serial port choices.
 *
 * PARAMETERS:	nameList - port names are added to this list.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSerial::HostGetPortNameList (PortNameList& results)
{
	results.clear ();

#ifdef __QNXNTO__
	results.push_back ("/dev/ser1");
	results.push_back ("/dev/ser2");
#else
/*
        // Don't really have these on Android
	results.push_back ("/dev/ttyS0");
	results.push_back ("/dev/ttyS1");
	results.push_back ("/dev/ttyS2");
	results.push_back ("/dev/ttyS3");
*/
        // Might have these on Android
	results.push_back ("/dev/ttyUSB0");
	results.push_back ("/dev/ttyUSB1");
#endif
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSerial::HostGetSerialBaudList
 *
 * DESCRIPTION:	Return the list of baud rates support by this computer.
 *				Used to prepare a menu of baud rate choices.
 *
 * PARAMETERS:	baudList - baud rates are added to this list.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSerial::HostGetSerialBaudList (BaudList& results)
{
	long	maxBaud = 115200;	// ::PrvGetMaxBaudRate ()? How to
								// determine that on Unix?.

	switch (maxBaud)
	{
		case 115200:	results.push_back (115200);
		case 57600:		results.push_back (57600);
		case 38400:		results.push_back (38400);
		case 19200:		results.push_back (19200);
		case 9600:		results.push_back (9600);
	}
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmHostTransportSerial::EmHostTransportSerial (void) :
	fReadThread (NULL),
	fWriteThread (NULL),
	fCommHandle (0),
	fCommSignalPipeA (0),
	fCommSignalPipeB (0),
	fTimeToQuit (false),
	fDataMutex (),
	fDataCondition (&fDataMutex),
	fReadMutex (),
	fReadBuffer (),
	fWriteMutex (),
	fWriteBuffer ()
{
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial d'tor
 *
 * DESCRIPTION:	Destructor.  Delete our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmHostTransportSerial::~EmHostTransportSerial (void)
{
	EmAssert (fReadThread == NULL);
	EmAssert (fWriteThread == NULL);
	EmAssert (fCommHandle == 0);
	EmAssert (fCommSignalPipeA == 0);
	EmAssert (fCommSignalPipeB == 0);
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::OpenCommPort
 *
 * DESCRIPTION:	Open the serial port.
 *
 * PARAMETERS:	config - data block describing which port to use.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmHostTransportSerial::OpenCommPort (const EmTransportSerial::ConfigSerial& config)
{
	EmTransportSerial::PortName	portName = config.fPort;

	PRINTF ("EmTransportSerial::HostOpen: attempting to open port \"%s\"",
			portName.c_str());

        PHEM_Log_Msg("OpenCommPort:");
        PHEM_Log_Msg(portName.c_str());
	if (!portName.empty ()) {
		PRINTF ("EmTransportSerial::HostOpen: Opening serial port...");

		// An execllent article on serial programming under UNIX ("Linux Serial Port
		// Programming Mini-Howto") says to set the following flags in the open call.
		// The O_NDELAY is so that you can open the serial port without having DCD
		// asserted.  I'm not sure what the O_NOCTTY is for.
                if (portName.compare("/@")) {
                  PHEM_Log_Msg("OpenCommPort normal open.");
		  fCommHandle = open(portName.c_str (), O_RDWR | O_NOCTTY | O_NDELAY);

 		  if (fCommHandle <= 0) {
			fCommHandle = 0;

                     PHEM_Log_Msg("OpenCommPort returning err.");
			return errno;
		  }
               } else {
                 // It's the special fake serial dev that we use to pass GPS strings
		 PRINTF ("EmTransportSerial::HostOpen: 'Opening' null (passthrough) device...");
                 PHEM_Log_Msg("OpenCommPort fake open.");
                 fCommHandle = -1; // has to be nonzero
               }
	} else {
		PRINTF ("EmTransportSerial::HostOpen: No port selected in the Properties dialog box...");
                PHEM_Log_Msg("OpenCommPort no name?");
		return -1;	// !!! better error number
	}

        PHEM_Log_Msg("OpenCommPort returning no error.");
	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::CreateCommThreads
 *
 * DESCRIPTION:	Create the threads that asynchronously read from and
 *				write to the serial port.
 *
 * PARAMETERS:	config - data block describing which port to use.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmHostTransportSerial::CreateCommThreads (const EmTransportSerial::ConfigSerial& /*config*/)
{
	if (fCommHandle) {
		PRINTF ("EmTransportSerial::HostOpen: Creating serial port handler threads...");

		// Create the pipe used to communicate with CommRead.

		int filedes[] = { 0, 0 };
		if (pipe (filedes) == 0)
		{
			fCommSignalPipeA = filedes[0];	// for reading
			fCommSignalPipeB = filedes[1];	// for writing
		}

		// Create the threads and start them up.

		fTimeToQuit = false;
		fReadThread = omni_thread::create (CommRead, this);
		fWriteThread = omni_thread::create (CommWrite, this);
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::DestroyCommThreads
 *
 * DESCRIPTION:	Shutdown and destroy the comm threads.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmHostTransportSerial::DestroyCommThreads (void)
{
	// If never created, nothing to destroy.

	if (!fCommSignalPipeA)
		return errNone;

	// Signal the threads to quit.

	fDataMutex.lock ();

	fTimeToQuit = true;

	int dummy = 0;
	write (fCommSignalPipeB, &dummy, sizeof (dummy));		// Signals CommRead.

	fDataCondition.broadcast ();	// Signals CommWrite.
	fDataMutex.unlock ();

	// Wait for the threads to quit.

	if (fReadThread)
	{
		fReadThread->join (NULL);
		fWriteThread->join (NULL);
	}

	// Thread objects delete themselves, so set our references to NULL.

	fReadThread = NULL;
	fWriteThread = NULL;

	// Close the signal pipe.

	close (fCommSignalPipeA);
	close (fCommSignalPipeB);

	fCommSignalPipeA = fCommSignalPipeB = 0;

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::CloseCommPort
 *
 * DESCRIPTION:	Close the comm port.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmHostTransportSerial::CloseCommPort (void)
{
        if (fCommHandle > 0) {
	  (void) close (fCommHandle);
        }

	fCommHandle = 0;

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::PutIncomingData
 *
 * DESCRIPTION:	Thread-safe method for adding data to the queue that
 *				holds data read from the serial port.
 *
 * PARAMETERS:	data - pointer to the read data.
 *				len - number of bytes pointed to by "data".
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmHostTransportSerial::PutIncomingData	(const void* data, long& len)
{
	if (len == 0)
		return;

	omni_mutex_lock lock (fReadMutex);

	char*	begin = (char*) data;
	char*	end = begin + len;
	while (begin < end)
		fReadBuffer.push_back (*begin++);
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::GetIncomingData
 *
 * DESCRIPTION:	Thread-safe method for getting data from the queue
 *				holding data read from the serial port.
 *
 * PARAMETERS:	data - pointer to buffer to receive data.
 *				len - on input, number of bytes available in "data".
 *					On exit, number of bytes written to "data".
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmHostTransportSerial::GetIncomingData	(void* data, long& len)
{
	omni_mutex_lock lock (fReadMutex);

	if (len > (long) fReadBuffer.size ())
		len = (long) fReadBuffer.size ();

	char*	p = (char*) data;
	deque<char>::iterator	begin = fReadBuffer.begin ();
	deque<char>::iterator	end = begin + len;

	copy (begin, end, p);
	fReadBuffer.erase (begin, end);
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::IncomingDataSize
 *
 * DESCRIPTION:	Thread-safe method returning the number of bytes in the
 *				read queue.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Number of bytes in the read queue.
 *
 ***********************************************************************/

long EmHostTransportSerial::IncomingDataSize (void)
{
	omni_mutex_lock lock (fReadMutex);

	return fReadBuffer.size ();
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::PutOutgoingData
 *
 * DESCRIPTION:	Thread-safe method for adding data to the queue that
 *				holds data to be written to the serial port.
 *
 * PARAMETERS:	data - pointer to the read data.
 *				len - number of bytes pointed to by "data".
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmHostTransportSerial::PutOutgoingData	(const void* data, long& len)
{
	if (len == 0)
		return;

	omni_mutex_lock lock (fWriteMutex);

	char*	begin = (char*) data;
	char*	end = begin + len;
	while (begin < end)
		fWriteBuffer.push_back (*begin++);

	// Wake up CommWrite.

	fDataMutex.lock ();
	fDataCondition.broadcast ();
	fDataMutex.unlock ();
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::GetOutgoingData
 *
 * DESCRIPTION:	Thread-safe method for getting data from the queue
 *				holding data to be written to the serial port.
 *
 * PARAMETERS:	data - pointer to buffer to receive data.
 *				len - on input, number of bytes available in "data".
 *					On exit, number of bytes written to "data".
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmHostTransportSerial::GetOutgoingData	(void* data, long& len)
{
	omni_mutex_lock lock (fWriteMutex);

	if (len > (long) fWriteBuffer.size ())
		len = (long) fWriteBuffer.size ();

	char*	p = (char*) data;
	deque<char>::iterator	begin = fWriteBuffer.begin ();
	deque<char>::iterator	end = begin + len;

	copy (begin, end, p);
	fWriteBuffer.erase (begin, end);
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::OutgoingDataSize
 *
 * DESCRIPTION:	Thread-safe method returning the number of bytes in the
 *				write queue.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Number of bytes in the read queue.
 *
 ***********************************************************************/

long EmHostTransportSerial::OutgoingDataSize (void)
{
	omni_mutex_lock lock (fWriteMutex);

	return fWriteBuffer.size ();
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::CommRead
 *
 * DESCRIPTION:	This function sits in its own thread, waiting for data
 *				to show up in the serial port.	If data arrives, this
 *				function plucks it out and stores it in a thread-safe
 *				queue.  It quits when it detects that the comm handle
 *				has been deleted.
 *
 * PARAMETERS:	data - pointer to owning EmHostTransportSerial.
 *
 * RETURNED:	Thread status.
 *
 ***********************************************************************/

void* EmHostTransportSerial::CommRead (void* data)
{
	EmHostTransportSerial*	This = (EmHostTransportSerial*) data;

	PRINTF ("CommRead starting.");

	while (!This->fTimeToQuit) {
		int		status, fd1, fd2, maxfd;
		fd_set	read_fds;
		FD_ZERO (&read_fds);

		fd1 = This->fCommHandle;
		fd2 = This->fCommSignalPipeA;

                if (This->fCommHandle > 0) {
		  maxfd = max (fd1, fd2);
		  FD_SET (fd1, &read_fds);
		  FD_SET (fd2, &read_fds);
                } else {
                  // Don't wait on that FD, it's not even really a file descriptor
		  maxfd = fd2;
		  FD_SET (fd2, &read_fds);
                }

		status = select (maxfd + 1, &read_fds, NULL, NULL, NULL);

		if (This->fTimeToQuit) {
			break;
                }

		if (status > 0)	// data available
		{
			if ((This->fCommHandle > 0) && FD_ISSET (fd1, &read_fds)) {
				char	buf[1024];
				int		len = 1024;
				len = read (fd1, buf, len);

				if (len == 0)
					break; // port closed

				// Log the data.
				if (LogSerialData ())
					LogAppendData (buf, len, "EmHostTransportSerial::CommRead: Received data:");
				else
					PRINTF ("EmHostTransportSerial::CommRead: Received %ld serial bytes.", len);

				// Add the data to the EmHostTransportSerial object's buffer.
				long	n = (long) len;
				This->PutIncomingData (buf, n);
			}
		}
	}

	PRINTF ("CommRead exitting.");

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::CommWrite
 *
 * DESCRIPTION:	This function sits in its own thread, waiting for data
 *				to show up in the write queue.	If data arrives, this
 *				function plucks it out and sends it out to the port.  It
 *				quits when it detects that the comm handle has been
 *				deleted.
 *
 * PARAMETERS:	data - pointer to owning EmHostTransportSerial.
 *
 * RETURNED:	Thread status.
 *
 ***********************************************************************/

void* EmHostTransportSerial::CommWrite (void* data)
{
	EmHostTransportSerial*	This = (EmHostTransportSerial*) data;

	PRINTF ("CommWrite starting.");

	omni_mutex_lock lock (This->fDataMutex);

	while (!This->fTimeToQuit) {
		This->fDataCondition.wait ();

		// It's the EmHostTransportSerial object telling us to quit.
		if (This->fTimeToQuit)
			break;

                if (This->fCommHandle > 0) {
		  // Get the data to write.

		  long	len = This->OutgoingDataSize ();

		  // If there really wasn't any, go back to sleep.

		  if (len == 0) {
			continue;
                  }

		  // Get the data.

		  void*	buf = Platform::AllocateMemory (len);
		  This->GetOutgoingData (buf, len);

		  // Log the data.

		  if (LogSerialData ()) {
			LogAppendData (buf, len, "EmHostTransportSerial::CommWrite: Transmitted data:");
		  } else {
			PRINTF ("EmHostTransportSerial::CommWrite: Transmitted %ld serial bytes.", len);
                  }

		  // Write the data.

		  ::write (This->fCommHandle, buf, len);

		  // Dispose of the data.

		  Platform::DisposeMemory (buf);
                } else {
		  PRINTF ("EmHostTransportSerial::CommWrite: Null write to emulated port.");
                }
	}

	PRINTF ("CommWrite exitting.");

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::GetBaud
 *
 * DESCRIPTION:	Map a baud rate into the Mac OS constant that represents
 *				that rate in a SerSettings call.
 *
 * PARAMETERS:	baud - raw baud rate.
 *
 * RETURNED:	Unix constant that represents that rate.
 *
 ***********************************************************************/

int EmHostTransportSerial::GetBaud (EmTransportSerial::Baud baud)
{
	switch (baud)
	{
#if defined (B150)
		case    150:		PRINTF ("	Baud = 150");		return B150;
#endif

#if defined (B300)
		case    300:		PRINTF ("	Baud = 300");		return B300;
#endif

#if defined (B600)
		case    600:		PRINTF ("	Baud = 600");		return B600;
#endif

#if defined (B1200)
		case   1200:		PRINTF ("	Baud = 1200");		return B1200;
#endif

#if defined (B1800)
		case   1800:		PRINTF ("	Baud = 1800");		return B1800;
#endif

#if defined (B2400)
		case   2400:		PRINTF ("	Baud = 2400");		return B2400;
#endif

#if defined (B4800)
		case   4800:		PRINTF ("	Baud = 4800");		return B4800;
#endif

#if defined (B9600)
		case   9600:		PRINTF ("	Baud = 9600");		return B9600;
#endif

#if defined (B19200)
		case  19200:		PRINTF ("	Baud = 19200");		return B19200;
#endif

#if defined (B38400)
		case  38400:		PRINTF ("	Baud = 38400");		return B38400;
#endif

#if defined (B57600)
		case  57600:		PRINTF ("	Baud = 57600");		return B57600;
#endif

#if defined (B115200)
		case 115200:		PRINTF ("	Baud = 115200");	return B115200;
#endif

#if defined (B230400)
		case 230400:		PRINTF ("	Baud = 230400");	return B230400;
#endif
	}

	PRINTF ("	Unknown Baud value: %ld.", (long) baud);

	// Not necessarily invalid.  The UART often has invalid baud values while
	// it's being initialized.   It's also equally valid for a Palm application
	// to attempt to set the  baud to something the underlying OS doesn't support.

	//	EmAssert (false);

	return baud;
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportSerial::GetDataBits
 *
 * DESCRIPTION:	Map a dataBits value into the Mac OS constant that
 *				represents that value in a SerSettings call.
 *
 * PARAMETERS:	dataBits - raw data bits value.
 *
 * RETURNED:	Unix constant that represents that dataBits value.
 *
 ***********************************************************************/

int EmHostTransportSerial::GetDataBits (EmTransportSerial::DataBits dataBits)
{
	switch (dataBits)
	{
		case 5:				PRINTF ("	dataBits = 5");		return CS5;
		case 6:				PRINTF ("	dataBits = 6");		return CS6;
		case 7:				PRINTF ("	dataBits = 7");		return CS7;
		case 8:				PRINTF ("	dataBits = 8");		return CS8;
	}

	PRINTF ("	Unknown DataBits value.");
	EmAssert (false);
	return 0;
}

