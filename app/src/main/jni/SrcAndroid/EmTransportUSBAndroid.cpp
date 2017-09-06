/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmTransportUSBAndroid.h"

#include "EmTransportSerial.h"	// EmTransportSerial
#include "Logging.h"			// LogSerial, LogAppendMsg

#define PRINTF	if (!LogSerial ()) ; else LogAppendMsg


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostHasUSB
 *
 * DESCRIPTION:	Return whether or not USB facilities are available.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if the host has a USB port and we can use it.
 *				False otherwise.
 *
 ***********************************************************************/

Bool EmTransportUSB::HostHasUSB (void)
{
	return false;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostConstruct
 *
 * DESCRIPTION:	Construct platform-specific objects/data.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The platform-specific serial object.
 *
 ***********************************************************************/

void EmTransportUSB::HostConstruct (void)
{
	fHost = new EmHostTransportUSB;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostDestruct
 *
 * DESCRIPTION:	Destroy platform-specific objects/data.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportUSB::HostDestruct (void)
{
	delete fHost;
	fHost = NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostOpen
 *
 * DESCRIPTION:	Open the serial port in a platform-specific fashion.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportUSB::HostOpen (void)
{
	fHost->fOpenLocally = true;
	
	fHost->UpdateOpenState ();

	return errNone;

}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostClose
 *
 * DESCRIPTION:	Close the serial port in a platform-specific fashion.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportUSB::HostClose (void)
{
	fHost->fOpenLocally = false;
	fHost->fOpenRemotely = false;

	ErrCode	err = fHost->fSerialTransport->Close ();

	return err;

}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostRead
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

ErrCode EmTransportUSB::HostRead (long& len, void* data)
{
	ErrCode	err = serErrTimeOut;
	len = 0;

	fHost->UpdateOpenState ();

	if (!fHost->FakeOpenConnection ())
	{
		err = fHost->fSerialTransport->Read (len, data);

		if (err)
		{
			fHost->fOpenRemotely = false;
			fHost->fSerialTransport->Close ();	// Close it
		}
	}

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostWrite
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

ErrCode EmTransportUSB::HostWrite (long& len, const void* data)
{
	ErrCode	err = errNone;
	len = 0;

	fHost->UpdateOpenState ();

	if (!fHost->FakeOpenConnection ())
	{
		err = fHost->fSerialTransport->Write (len, data);

		if (err)
		{
			fHost->fOpenRemotely = false;
			fHost->fSerialTransport->Close ();	// Close it
		}
	}

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostCanRead
 *
 * DESCRIPTION:	Return whether or not the transport is available for
 *				a read operation (that is, it's connected to another
 *				entity).  Does NOT indicate whether or not there are
 *				actually any bytes available to be read.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	True if so.
 *
 ***********************************************************************/

Bool EmTransportUSB::HostCanRead (void)
{
	return fHost->fOpenLocally && fHost->fOpenRemotely;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostCanWrite
 *
 * DESCRIPTION:	Return whether or not the transport is available for
 *				a write operation (that is, it's connected to another
 *				entity).  Does NOT indicate whether or not there is
 *				actually any room in the transport's internal buffer
 *				for the data being written.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	True if so.
 *
 ***********************************************************************/

Bool EmTransportUSB::HostCanWrite (void)
{
	return fHost->fOpenLocally && fHost->fOpenRemotely;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostBytesInBuffer
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

long EmTransportUSB::HostBytesInBuffer (long minBytes)
{
	long	bytesRead = 0;
	
	fHost->UpdateOpenState ();

	if (!fHost->FakeOpenConnection ())
	{
		try
		{
			bytesRead = fHost->fSerialTransport->BytesInBuffer (minBytes);
		}
		catch (ErrCode)
		{
			fHost->fOpenRemotely = false;
			fHost->fSerialTransport->Close ();
		}
	}

	return bytesRead;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::HostSetConfig
 *
 * DESCRIPTION:	Configure the serial port in a platform-specific
 *				fasion.  The port is assumed to be open.
 *
 * PARAMETERS:	config - configuration information.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportUSB::HostSetConfig (const ConfigUSB& /*config*/)
{
	ErrCode	result = errNone;

	return result;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmHostTransportUSB c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmHostTransportUSB::EmHostTransportUSB (void) :
	fOpenLocally (false),
	fOpenRemotely (false),
	fSerialTransport (NULL)
{
	EmTransportSerial::ConfigSerial	config;
	
	config.fPort = "USB";

	fSerialTransport = dynamic_cast<EmTransportSerial*> (config.NewTransport ());
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportUSB d'tor
 *
 * DESCRIPTION:	Destructor.  Delete our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmHostTransportUSB::~EmHostTransportUSB (void)
{
	delete fSerialTransport;
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportUSB::UpdateOpenState
 *
 * DESCRIPTION:	If we want the serial port transport open, and it's not
 *				open, then try opening it.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

void EmHostTransportUSB::UpdateOpenState (void)
{
	// If we need to open the USB driver and haven't done so, yet, then
	// try to do so.

	if (fOpenLocally &&							// Do we need the USB driver open?
		!fOpenRemotely &&						// Is it closed?
		fSerialTransport->Open () == errNone)	// Could we open it?
	{
		fOpenRemotely = true;					// Say it's open
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmHostTransportUSB::FakeOpenConnection
 *
 * DESCRIPTION:	If we've asked the driver to be open, but could not do
 *				so because the USB device is not online, then try to
 *				pretend that we're talking with a non-responsive serial
 *				device.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if HostOpen has been called, but the USB device is
 *				not yet online and we haven't been able to call Open on
 *				the underlying serial transport.  False otherwise.
 *
 ***********************************************************************/

Bool EmHostTransportUSB::FakeOpenConnection (void)
{
	return fOpenLocally && !fOpenRemotely;
}
