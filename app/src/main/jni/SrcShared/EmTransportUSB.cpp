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
#include "EmTransportUSB.h"

#include "EmErrCodes.h"			// kError_CommOpen
#include "Logging.h"			// LogSerial


//EmTransportUSB::OpenPortList	EmTransportUSB::fgOpenPorts;

#define PRINTF	if (!LogSerial ()) ; else LogAppendMsg


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportUSB::EmTransportUSB (void) :
	fHost (NULL),
	fConfig (),
	fCommEstablished (false)
{
	this->HostConstruct ();
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	desc - descriptor information used when opening
 *					the USB port.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportUSB::EmTransportUSB (const EmTransportDescriptor& /* desc */) :
	fHost (NULL),
	fConfig (),
	fCommEstablished (false)
{
	ConfigUSB	config;

	this->HostConstruct ();
	this->SetConfig (config);
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	config - configuration information used when opening
 *					the USB port.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportUSB::EmTransportUSB (const ConfigUSB& config) :
	fHost (NULL),
	fConfig (),
	fCommEstablished (false)
{
	this->HostConstruct ();
	this->SetConfig (config);
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB d'tor
 *
 * DESCRIPTION:	Destructor.  Delete our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportUSB::~EmTransportUSB (void)
{
	this->Close ();
	this->HostDestruct ();
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::Open
 *
 * DESCRIPTION:	Open the transport using the information provided
 *				either in the constructor or with SetConfig.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportUSB::Open (void)
{
	PRINTF ("EmTransportUSB::Open...");

	// Exit if communications have already been established.

	if (fCommEstablished)
	{
		PRINTF ("EmTransportUSB::Open: USB port already open...leaving...");
		return kError_CommOpen;
	}

//	EmAssert (fgOpenPorts.find (fConfig.fPort) == fgOpenPorts.end ());

	ErrCode	err = this->HostOpen ();

	if (!err)
		err = this->HostSetConfig (fConfig);

	if (err)
	{
		this->HostClose ();
	}
	else
	{
		fCommEstablished = true;
//		fgOpenPorts[fConfig.fPort] = this;
	}

	if (err)
		PRINTF ("EmTransportUSB::Open: err = %ld", err);

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::Close
 *
 * DESCRIPTION:	Close the transport.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportUSB::Close (void)
{
	if (!fCommEstablished)
	{
		PRINTF ("EmTransportUSB::Close: USB port not open...leaving...");
		return kError_CommNotOpen;
	}

	fCommEstablished = false;
//	fgOpenPorts.erase (fConfig.fPort);

	ErrCode	err = this->HostClose ();

	if (err)
		PRINTF ("EmTransportUSB::Close: err = %ld", err);

	PRINTF ("EmTransportUSB::Close: now closed...");

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::Read
 *
 * DESCRIPTION:	Read up to the given number of bytes, storing them in
 *				the given buffer.
 *
 * PARAMETERS:	len - maximum number of bytes to read.
 *				data - buffer to receive the bytes.
 *
 * RETURNED:	0 if no error.  The number of bytes actually read is
 *				returned in len if there was no error.
 *
 ***********************************************************************/

ErrCode EmTransportUSB::Read (long& len, void* data)
{
	PRINTF ("EmTransportUSB::Read...");

	if (!fCommEstablished)
	{
		PRINTF ("EmTransportUSB::Read: port not open, leaving");
		return kError_CommNotOpen;
	}

	ErrCode	err = this->HostRead (len, data);

	if (err)
		PRINTF ("EmTransportUSB::Read: err = %ld", err);
	else
		if (LogSerialData ())
			LogAppendData (data, len, "EmTransportUSB::Read: reading %ld bytes.", len);
		else
			PRINTF ("EmTransportUSB::Read: reading %ld bytes", len);

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::Write
 *
 * DESCRIPTION:	Write up the the given number of bytes, using the data
 *				in the given buffer.
 *
 * PARAMETERS:	len - number of bytes in the buffer.
 *				data - buffer containing the bytes.
 *
 * RETURNED:	0 if no error.  The number of bytes actually written is
 *				returned in len if there was no error.
 *
 ***********************************************************************/

ErrCode EmTransportUSB::Write (long& len, const void* data)
{
	PRINTF ("EmTransportUSB::Write...");

	if (!fCommEstablished)
	{
		PRINTF ("EmTransportUSB::Write: port not open, leaving");
		return kError_CommNotOpen;
	}

	ErrCode	err = this->HostWrite (len, data);

	if (err)
		PRINTF ("EmTransportUSB::Write: err = %ld", err);
	else
		if (LogSerialData ())
			LogAppendData (data, len, "EmTransportUSB::Write: writing %ld bytes.", len);
		else
			PRINTF ("EmTransportUSB::Write: writing %ld bytes", len);

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::CanRead
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

Bool EmTransportUSB::CanRead (void)
{
	return this->HostCanRead ();
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::CanWrite
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

Bool EmTransportUSB::CanWrite (void)
{
	return this->HostCanRead ();
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::BytesInBuffer
 *
 * DESCRIPTION:	Returns the number of bytes that can be read with the
 *				Read method.  Note that bytes may be received in
 *				between the time BytesInBuffer is called and the time
 *				Read is called, so calling the latter with the result
 *				of the former is not guaranteed to fetch all received
 *				and buffered bytes.
 *
 * PARAMETERS:	minBytes - try to buffer at least this many bytes.
 *					Return when we have this many bytes buffered, or
 *					until some small timeout has occurred.
 *
 * RETURNED:	Number of bytes that can be read.
 *
 ***********************************************************************/

long EmTransportUSB::BytesInBuffer (long minBytes)
{
	if (!fCommEstablished)
		return 0;

	return this->HostBytesInBuffer (minBytes);
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::GetSpecificName
 *
 * DESCRIPTION:	Returns the port name, or host address, depending on the
 *				transport in question.
 *
 * PARAMETERS:	
 *
 * RETURNED:	string, appropriate to the transport in question.
 *
 ***********************************************************************/
 
 string EmTransportUSB::GetSpecificName (void)
 {
 	return "USB";
 }


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::SetConfig
 *
 * DESCRIPTION:	Set the configuration to be used when opening the port,
 *				or to reconfigure a currently open port.
 *
 * PARAMETERS:	config - the configuration to use.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportUSB::SetConfig (const ConfigUSB& config)
{
	PRINTF ("EmTransportUSB::SetConfig...");

	if (config == fConfig)
	{
		PRINTF ("EmTransportUSB::SetConfig: Config unchanged, so not setting settings...");
		return errNone;
	}

	fConfig = config;

	// Exit if communications have not been established.

	if (!fCommEstablished)
	{
		PRINTF ("EmTransportUSB::SetConfig: USB port closed, so not setting settings...");
		return kError_CommNotOpen;
	}

	ErrCode	err = this->HostSetConfig (fConfig);

	if (err)
		PRINTF ("EmTransportUSB::SetConfig: err = %ld", err);

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::GetConfig
 *
 * DESCRIPTION:	Return the configuration specified in the constructor
 *				or in the last call to SetConfig.
 *
 * PARAMETERS:	config - config object to receive the settings.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportUSB::GetConfig (ConfigUSB& config)
{
	config = fConfig;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::GetTransport
 *
 * DESCRIPTION:	Return any transport object currently using the port
 *				specified in the given configuration.
 *
 * PARAMETERS:	config - The configuration object containing information
 *					on a port in which we're interested.  All or some
 *					of the information in this object is used when
 *					searching for a transport object already utilizing
 *					the port.
 *
 * RETURNED:	Any found transport object.  May be NULL.
 *
 ***********************************************************************/

EmTransportUSB* EmTransportUSB::GetTransport (const ConfigUSB& /*config*/)
{
#if 0
	OpenPortList::iterator	iter = fgOpenPorts.find (config.fPort);

	if (iter == fgOpenPorts.end ())
		return NULL;

	return iter->second;
#endif

	return NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::GetDescriptorList
 *
 * DESCRIPTION:	Return the list of USB ports on this computer.  Used
 *				to prepare a menu of USB port choices.
 *
 * PARAMETERS:	nameList - port names are added to this list.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportUSB::GetDescriptorList (EmTransportDescriptorList& descList)
{
	descList.clear ();

	if (EmTransportUSB::HostHasUSB ())
	{
		descList.push_back (EmTransportDescriptor (kTransportUSB));
	}
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::ConfigUSB c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportUSB::ConfigUSB::ConfigUSB ()
{
}
			

/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::ConfigUSB d'tor
 *
 * DESCRIPTION:	Destructor.  Delete our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportUSB::ConfigUSB::~ConfigUSB (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::ConfigUSB::NewTransport
 *
 * DESCRIPTION:	Create a new transport object based on the configuration
 *				information in this object.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The new transport object.
 *
 ***********************************************************************/

EmTransport* EmTransportUSB::ConfigUSB::NewTransport (void)
{
	return new EmTransportUSB (*this);
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::ConfigUSB::GetTransport
 *
 * DESCRIPTION:	Return any transport object currently using the port
 *				specified in the given configuration.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Any found transport object.  May be NULL.
 *
 ***********************************************************************/

EmTransport* EmTransportUSB::ConfigUSB::GetTransport (void)
{
	return EmTransportUSB::GetTransport (*this);
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportUSB::ConfigUSB::operator==
 *
 * DESCRIPTION:	Compare two Config objects to each other
 *
 * PARAMETERS:	other - the object to compare "this" to.
 *
 * RETURNED:	True if the objects are equivalent.
 *
 ***********************************************************************/

bool EmTransportUSB::ConfigUSB::operator==(const ConfigUSB& /*other*/) const
{
	return true;
}
