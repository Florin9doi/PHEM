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
#include "EmTransportSocket.h"

#include "EmErrCodes.h"			// kError_CommOpen, kError_CommNotOpen, kError_NoError
#include "Logging.h"			// LogSerial
#include "Platform.h"			// Platform::AllocateMemory

#if PLATFORM_MAC
#include <GUSIPOSIX.h>			// inet_addr
#endif

#if PLATFORM_UNIX
#include <errno.h>				// ENOENT, errno
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>			// timeval
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>				// close
#include <arpa/inet.h>			// inet_addr
#endif

/* Update for GCC 4 */
#include <stdlib.h>
#include <string.h>

#ifndef INADDR_NONE
#define INADDR_NONE		0xffffffff
#endif

#define PRINTF	if (!LogSerial ()) ; else LogAppendMsg

EmTransportSocket::OpenPortList	EmTransportSocket::fgOpenPorts;


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 * NOTES:		We used to only have a fDataSocket. Now, to allow for
 *				a listening socket to be opened before a connection is
 *				attempted, we need two sockets. Note that EmTransport
 *				Socket does not get its hands dirty with actual sockets.
 *				Thus, fDataListenSocket is not a socket in the traditional
 *				sense, and may be connected or not.
 *
 ***********************************************************************/

EmTransportSocket::EmTransportSocket (void) :
	fReadMutex (),
	fReadBuffer (),
	fDataConnectSocket (NULL),
	fDataListenSocket (NULL),
	fConfig (),
	fCommEstablished (false)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	desc - descriptor information used when opening
 *					the TCP port.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportSocket::EmTransportSocket (const EmTransportDescriptor& desc) :
	fReadMutex (),
	fReadBuffer (),
	fDataConnectSocket (NULL),
	fDataListenSocket (NULL),
	fConfig (),
	fCommEstablished (false)
{
	ConfigSocket	config;

	string				addr		= desc.GetSchemeSpecific ();
	string::size_type	colonPos	= addr.find (':');
	string::size_type	nonNumPos	= addr.find_first_not_of ("0123456789");

	// If there's a colon, assume a fully-specified address.

	if (colonPos != string::npos)
	{
		config.fTargetHost	= addr.substr (0, colonPos);
		config.fTargetPort	= addr.substr (colonPos + 1);
	}

	// If there's no colon, look for numbers.  If the entire
	// address is made up of numbers, assume it's a port number.

	else if (nonNumPos == string::npos)
	{
		config.fTargetPort	= addr;
	}

	// Otherwise, assume it's a host address with no port.

	else
	{
		config.fTargetHost	= addr;
	}

	fConfig = config;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	config - configuration information used when opening
 *					the TCP port.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportSocket::EmTransportSocket (const ConfigSocket& config) :
	fReadMutex (),
	fReadBuffer (),
	fDataConnectSocket (NULL),
	fDataListenSocket (NULL),
	fConfig (config),
	fCommEstablished (false)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket d'tor
 *
 * DESCRIPTION:	Destructor.  Delete our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportSocket::~EmTransportSocket (void)
{
	this->Close ();
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::Open
 *
 * DESCRIPTION:	Open the transport using the information provided
 *				either in the constructor or with SetConfig.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	0 if no error.
 *
 * NOTES:		Implements the new method of opening a listening socket,
 *				then opening a connection socket. This avoids the problem
 *				where both machines might try to connect, fail, and then
 *				each fall back to listening. However, this approach does
 *				not work in localhost mode, for obvious (upon reflection)
 *				reasons: if you start listening, then shout, you are
 *				going to hear yourself (or in this case, you're going to
 *				drop your listening socket once your connecting socket
 *				tells you that it has connected, not knowing that you have
 *				connected to yourself).
 *
 ***********************************************************************/

ErrCode EmTransportSocket::Open (void)
{
	PRINTF ("EmTransportSocket::Open...");

	// Exit if communications have already been established.

	if (fCommEstablished)
	{
		PRINTF ("EmTransportSocket::Open: TCP port already open...leaving...");
		return kError_CommOpen;
	}

	string registrationKey = "TCP:" + fConfig.fTargetHost + ":" + fConfig.fTargetPort;
	EmAssert (fgOpenPorts.find (registrationKey) == fgOpenPorts.end ());

	ErrCode err;

	if (fConfig.fTargetHost != "localhost")
	{
		err= OpenCommPortListen (fConfig);

		if (err)
		{
			PRINTF ("EmTransportSocket::Open: comm port closed due to error %ld on listening attempt", err);
			PRINTF ("EmTransportSocket::Open: closing listening socket");

			err = CloseCommPortListen ();
			fCommEstablished = false;

			if (err)
				PRINTF ("EmTransportSocket::Open: err = %ld when closing listen socket", err);
		}
		else if (fDataListenSocket->ConnectPending ())
		{
			PRINTF ("EmTransportSocket::Open: not attempting to connect due to a pending connection");
		}
		else
		{
			PRINTF ("EmTransportSocket::Open: listening socket opened properly");

			// set to true here because we are listening properly; not affected by whether this
			// next connect call fails
			fCommEstablished = true;

			err = OpenCommPortConnect (fConfig);
		}

		if (err == 0)
		{
			fCommEstablished = true;
			fgOpenPorts[registrationKey] = this;

			PRINTF ("EmTransportSocket::Open: successful connection, so closing listening socket");

			err = CloseCommPortListen ();
		}
		else
		{
			PRINTF ("EmTransportSocket::Open: err = %ld on connect attempt", err);
			PRINTF ("EmTransportSocket::Open: closing connect socket");

			err = CloseCommPortConnect ();
		}
	}
	else
	{
		PRINTF ("EmTransportSocket::Open: opening in localhost mode (old style)");

		err = OpenCommPortConnect (fConfig);

		if (err)
		{
			PRINTF ("EmTransportSocket::Open: err %ld in connect attempt", err);

			err = CloseCommPortConnect ();

			if (err)
				PRINTF ("EmTransportSocket::Open: err %ld in connect socket close", err);

			PRINTF ("EmTransportSocket::Open: falling back to listening");

			err = OpenCommPortListen (fConfig);

			PRINTF ("EmTransportSocket::Open: listening socket opened properly");
		}

		if (err == 0)
		{
			fCommEstablished = true;
			fgOpenPorts [registrationKey] = this;
		}
		else
		{
			PRINTF ("EmTransportSocket::Open: error %ld on listening attempt, closing listen socket", err);

			err = CloseCommPortListen ();
		}
	}

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::Close
 *
 * DESCRIPTION:	Close the transport.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportSocket::Close (void)
{
	PRINTF ("EmTransportSocket::Close...");

	if (!fCommEstablished)
	{
		PRINTF ("EmTransportSocket::Close: TCP port not open...leaving...");
		return kError_CommNotOpen;
	}

	fCommEstablished = false;

	string registrationKey = "TCP:" + fConfig.fTargetHost + ":" + fConfig.fTargetPort;
	
	fgOpenPorts.erase (registrationKey);

	ErrCode	err = CloseCommPort ();

	if (err)
		PRINTF ("EmTransportSocket::Close: err = %ld", err);

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::Read
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

ErrCode EmTransportSocket::Read (long& len, void* data)
{
	PRINTF ("EmTransportSocket::Read...");

	if (!fCommEstablished)
	{
		PRINTF ("EmTransportSocket::Read: port not open, leaving");
		return kError_CommNotOpen;
	}

	GetIncomingData (data, len);

	if (LogSerialData ())
		LogAppendData (data, len, "EmTransportSocket::Read: reading %ld bytes.", len);
	else
		PRINTF ("EmTransportSocket::Read: reading %ld bytes", len);

	return kError_NoError;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::Write
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

ErrCode EmTransportSocket::Write (long& len, const void* data)
{
	PRINTF ("EmTransportSocket::Write...");
	
	ErrCode err;

	if (!fCommEstablished)
	{
		PRINTF ("...EmTransportSocket::Write: port not open, leaving");
		return kError_CommNotOpen;
	}

	// Tracking errors here so that we can close the connection if some
	// mechanism farther down the food chain (in CTCPSocket) finds out
	// that the connection was dropped. This wasn't checked before, as
	// errors were ignored in PutOutgoingData, explaining the many
	// writes of 0 bytes (indicating a dropped connection) in log files.

	err = PutOutgoingData (data, len);

	if (err)
	{
		PRINTF ("...EmTransportSocket::Write: PutOutgoingData returned err %ld, closing connection", err);

		this->Close ();

		return kError_CommNotOpen;
	}

	if (LogSerialData ())
		LogAppendData (data, len, "EmTransportSocket::Write: writing %ld bytes.", len);
	else
		PRINTF ("EmTransportSocket::Write: writing %ld bytes", len);

	return kError_NoError;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::CanRead
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

Bool EmTransportSocket::CanRead (void)
{
	return fCommEstablished;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::CanWrite
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

Bool EmTransportSocket::CanWrite (void)
{
	return fCommEstablished;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::BytesInBuffer
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

long EmTransportSocket::BytesInBuffer (long /*minBytes*/)
{
	if (!fCommEstablished)
		return 0;

	return this->IncomingDataSize ();
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::GetSpecificName
 *
 * DESCRIPTION:	Returns the port name, or host address, depending on the
 *				transport in question.
 *
 * PARAMETERS:	
 *
 * RETURNED:	string, appropriate to the transport in question.
 *
 ***********************************************************************/
 
 string EmTransportSocket::GetSpecificName (void)
 {
 	string returnString = fConfig.fTargetHost + ":" + fConfig.fTargetPort;
 	return returnString;
 }


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::GetTransport
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

EmTransportSocket* EmTransportSocket::GetTransport (const ConfigSocket& config)
{
	string registrationKey = "TCP:" + config.fTargetHost + ":" + config.fTargetPort;
	OpenPortList::iterator	iter = fgOpenPorts.find (registrationKey);

	if (iter == fgOpenPorts.end ())
		return NULL;

	return iter->second;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::GetDescriptorList
 *
 * DESCRIPTION:	Return the list of TCP ports on this computer.  Used
 *				to prepare a menu of TCP port choices.
 *
 * PARAMETERS:	nameList - port names are added to this list.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSocket::GetDescriptorList (EmTransportDescriptorList& descList)
{
	descList.clear ();

	descList.push_back (EmTransportDescriptor (kTransportSocket));
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::OpenCommPortConnect
 *
 * DESCRIPTION:	Open the TCP port.
 *
 * PARAMETERS:	config - data block describing which port to use.
 *
 * RETURNED:	0 if no error.
 *
 * NOTES:		Implements a subset of OpenCommPort's functionality.
 *				In particular, it tries to connect, then returns.
 *				OpenCommPortListen implements the listening behavior.
 *				This change was made so that control over the behavior
 *				(whether connect first or listen first) could be
 *				in Open.
 *
 ***********************************************************************/

ErrCode EmTransportSocket::OpenCommPortConnect (const EmTransportSocket::ConfigSocket& config)
{
	ErrCode err;

	if (fDataConnectSocket)
		return errNone;

	fDataConnectSocket = new CTCPClientSocket (EmTransportSocket::EventCallBack,
		config.fTargetHost, atoi (config.fTargetPort.c_str ()), this);

	// Try establishing a connection to some peer already waiting on the
	// target host, on the target port

	err = fDataConnectSocket->Open ();

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::OpenCommPortListen
 *
 * DESCRIPTION:	Open the TCP port.
 *
 * PARAMETERS:	config - data block describing which port to use.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportSocket::OpenCommPortListen (const EmTransportSocket::ConfigSocket& config)
{
	ErrCode err;

	if (fDataListenSocket)
		return errNone;

	fDataListenSocket = new CTCPClientSocket (EmTransportSocket::EventCallBack,
		config.fTargetHost, atoi (config.fTargetPort.c_str ()), this);

	// Fall into server mode and start waiting

	err = fDataListenSocket->OpenInServerMode ();

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::EventCallBack
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/
void EmTransportSocket::EventCallBack (CSocket* s, int event)
{
	switch (event)
	{
		case CSocket::kConnected:
			break;

		case CSocket::kDataReceived:
			while (s->HasUnreadData(1))
			{
				long len = 1;
				char buf;

				s->Read(&buf, len, &len);

				if (len > 0)
				{
					// Log the data.
					if (LogSerialData ())
						LogAppendData (&buf, len, "EmTransportSocket::CommRead: Received data:");
					else
						PRINTF ("EmTransportSocket::CommRead: Received %ld TCP bytes.", len);

					// Add the data to the EmTransportSocket object's buffer.
					((CTCPClientSocket*)s)->GetOwner()->PutIncomingData (&buf, len);
				}
			}
			break;

		case CSocket::kDisconnected:
			break;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::CloseCommPort
 *
 * DESCRIPTION:	Close the comm port.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportSocket::CloseCommPort (void)
{
	// Must close each socket separately.

	if (fDataListenSocket)
	{
		fDataListenSocket->Close ();
		fDataListenSocket->Delete ();
		fDataListenSocket = NULL;
	}

	if (fDataConnectSocket)
	{
		fDataConnectSocket->Close ();
		fDataConnectSocket->Delete ();
		fDataConnectSocket = NULL;
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::CloseCommPortConnect
 *
 * DESCRIPTION:	Close the comm port.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportSocket::CloseCommPortConnect (void)
{
	if (fDataConnectSocket)
	{
		fDataConnectSocket->Close ();
		fDataConnectSocket->Delete ();
		fDataConnectSocket = NULL;
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::CloseCommPortListen
 *
 * DESCRIPTION:	Close the comm port.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	0 if no error.
 *
 ***********************************************************************/

ErrCode EmTransportSocket::CloseCommPortListen (void)
{
	if (fDataListenSocket)
	{
		fDataListenSocket->Close ();
		fDataListenSocket->Delete ();
		fDataListenSocket = NULL;
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::PutIncomingData
 *
 * DESCRIPTION:	Thread-safe method for adding data to the queue that
 *				holds data read from the TCP port.
 *
 * PARAMETERS:	data - pointer to the read data.
 *				len - number of bytes pointed to by "data".
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSocket::PutIncomingData	(const void* data, long& len)
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
 * FUNCTION:	EmTransportSocket::GetIncomingData
 *
 * DESCRIPTION:	Thread-safe method for getting data from the queue
 *				holding data read from the TCP port.
 *
 * PARAMETERS:	data - pointer to buffer to receive data.
 *				len - on input, number of bytes available in "data".
 *					On exit, number of bytes written to "data".
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmTransportSocket::GetIncomingData	(void* data, long& len)
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
 * FUNCTION:	EmTransportSocket::IncomingDataSize
 *
 * DESCRIPTION:	Thread-safe method returning the number of bytes in the
 *				read queue.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Number of bytes in the read queue.
 *
 ***********************************************************************/


long EmTransportSocket::IncomingDataSize (void)
{
	omni_mutex_lock lock (fReadMutex);

	return fReadBuffer.size ();
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::PutOutgoingData
 *
 * DESCRIPTION:	Thread-safe method for adding data to the queue that
 *				holds data to be written to the TCP port.
 *
 * PARAMETERS:	data - pointer to the read data.
 *				len - number of bytes pointed to by "data".
 *
 * RETURNED:	Nothing
 *
 * NOTES:		See the caveat in the constructor's comments about treating
 *				these "sockets" as real sockets. The actual control over
 *				sockets is down a level in CTCPSocket; thus, the listening
 *				socket at this level is, logically, where data should be
 *				written to. The connect socket is tried first, as the
 *				listening socket is closed on a successful connection.
 *
 ***********************************************************************/

ErrCode EmTransportSocket::PutOutgoingData	(const void* data, long& len)
{
	ErrCode err = kError_CommNotOpen;

	if (fDataListenSocket)
		err = fDataListenSocket->Write (data, len, &len);
	if (fDataConnectSocket)
		err = fDataConnectSocket->Write (data, len, &len);

	return err;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::ConfigSocket c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportSocket::ConfigSocket::ConfigSocket (void)
{
}
			

/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::ConfigSocket d'tor
 *
 * DESCRIPTION:	Destructor.  Delete our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmTransportSocket::ConfigSocket::~ConfigSocket (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::ConfigSocket::NewTransport
 *
 * DESCRIPTION:	Create a new transport object based on the configuration
 *				information in this object.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The new transport object.
 *
 ***********************************************************************/

EmTransport* EmTransportSocket::ConfigSocket::NewTransport (void)
{
	return new EmTransportSocket (*this);
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::ConfigSocket::GetTransport
 *
 * DESCRIPTION:	Return any transport object currently using the port
 *				specified in the given configuration.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Any found transport object.  May be NULL.
 *
 ***********************************************************************/

EmTransport* EmTransportSocket::ConfigSocket::GetTransport (void)
{
	return EmTransportSocket::GetTransport (*this);
}


/***********************************************************************
 *
 * FUNCTION:	EmTransportSocket::ConfigSocket::operator==
 *
 * DESCRIPTION:	Compare two Config objects to each other
 *
 * PARAMETERS:	other - the object to compare "this" to.
 *
 * RETURNED:	True if the objects are equivalent.
 *
 ***********************************************************************/

bool EmTransportSocket::ConfigSocket::operator==(const ConfigSocket& other) const
{
	return
			fTargetHost	== other.fTargetHost	&&
			fTargetPort	== other.fTargetPort;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	CTCPClientSocket::CTCPClientSocket
 *
 * DESCRIPTION:CTPClientSocket c'tor
 *
 * PARAMETERS: 
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

enum { kSocketState_Unconnected, kSocketState_Listening, kSocketState_Connected };

CTCPClientSocket::CTCPClientSocket (EventCallback fn, string targetHost, int targetPort, EmTransportSocket* transport) :
	CTCPSocket (fn, targetPort)
{
	fTargetHost = targetHost;
	fTransport = transport;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPClientSocket::~CTCPClientSocket
 *
 * DESCRIPTION:
 *
 * PARAMETERS:
 *
 * RETURNED:
 *
 ***********************************************************************/

CTCPClientSocket::~CTCPClientSocket ()
{
}


/***********************************************************************
 *
 * FUNCTION:	CTCPClientSocket::Open
 *
 * DESCRIPTION: Open a socket and try to establish a connection to a
 *					 TCP target. 
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	errNone if a connection has been established, or a
 *					nonzero error code otherwise
 *
 ***********************************************************************/

ErrCode CTCPClientSocket::Open (void)
{
	PRINTF ("CTCPClientSocket(0x%08X)::Open...", this);

	EmAssert (fSocketState == kSocketState_Unconnected);
	EmAssert (fConnectedSocket == INVALID_SOCKET);

	sockaddr	addr;
	int 		result;

	fConnectedSocket = this->NewSocket ();
	if (fConnectedSocket == INVALID_SOCKET)
	{
		PRINTF ("...NewSocket failed for connecting socket; result = %08X", this->GetError ());
		return this->ErrorOccurred ();
	}

	// Attempt to connect to that address (in case anyone is listening).
	result = connect (fConnectedSocket, this->FillAddress (&addr), sizeof(addr));
	if (result == 0)
	{
		PRINTF ("...connected!");

		fSocketState = kSocketState_Connected;
		return errNone;
	}
	// if the connection was unsuccessful, this should be logged as well
	else
	{
		PRINTF ("...unable to connect: %d", result);
	}

	closesocket (fConnectedSocket);
	fConnectedSocket = INVALID_SOCKET;

	return this->ErrorOccurred ();
}


/***********************************************************************
 *
 * FUNCTION:	CTCPClientSocket::OpenInServerMode
 *
 * DESCRIPTION: 
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	errNone if a connection has been established, or a
 *					nonzero error code otherwise
 *
 ***********************************************************************/

ErrCode CTCPClientSocket::OpenInServerMode (void)
{
	int 		result;
	sockaddr	addr;

	fListeningSocket = this->NewSocket ();
	if (fListeningSocket == INVALID_SOCKET)
	{
		PRINTF ("...NewSocket failed for listening socket; result = %08X", this->GetError ());
		return this->ErrorOccurred ();
	}

	result = bind (fListeningSocket, this->FillLocalAddress (&addr), sizeof(addr));
	if (result != 0)
	{
		PRINTF ("...bind failed; result = %08X", this->GetError ());
		return this->ErrorOccurred ();
	}

	// Start listening for a connection.

	result = listen (fListeningSocket, 1);
	if (result != 0)
	{
		PRINTF ("...listen failed; result = %08X", this->GetError ());
		return this->ErrorOccurred ();
	}

	PRINTF ("...listening for connection");

	fSocketState = kSocketState_Listening;

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPClientSocket::FillLocalAddress
 *
 * DESCRIPTION: Fill in a sockaddr data structure with values
 *					appropriate for connecting from the outside. This is an
 *					Internet address on the local machine.
 *
 * PARAMETERS:	addr - the sockaddr to fill in.
 *
 * RETURNED:	The same sockaddr passed in.
 *
 ***********************************************************************/

sockaddr* CTCPClientSocket::FillLocalAddress (sockaddr* addr)
{
	sockaddr_in*	addr_in = (sockaddr_in*) addr;

#ifdef HAVE_SIN_LEN
	addr_in->sin_len			= sizeof (*addr_in);
#endif
	addr_in->sin_family 		= AF_INET;
	addr_in->sin_port			= htons (fPort);
	addr_in->sin_addr.s_addr	= htonl (INADDR_ANY);

	return addr;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPClientSocket::GetOwner
 *
 * DESCRIPTION:This creates a link between a CTCPClientSocket object and
 *					a EmTransportSocket object. 
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Associated transport object, as passed to c'tor
 *
 ***********************************************************************/

EmTransportSocket* CTCPClientSocket::GetOwner (void)
{
	return fTransport;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPClientSocket::FillAddress
 *
 * DESCRIPTION: Fill in a sockaddr data structure with values
 *					appropriate for connecting from the outside. This is an
 *					Internet address on the local machine. 
 *
 * PARAMETERS:	addr - the sockaddr to fill in.
 *
 * RETURNED:	The same sockaddr passed in.
 *
 ***********************************************************************/

sockaddr* CTCPClientSocket::FillAddress (sockaddr* addr)
{
	PRINTF ("CTCPSocket(0x%08X)::FillAddress...", this);

	sockaddr_in*	addr_in = (sockaddr_in*) addr;
	const char*		name = fTargetHost.c_str ();
	unsigned long ip;

	// Check for common "localhost" case in order to avoid a name lookup on the Mac

	if (!_stricmp(name,"localhost"))
	{
		ip = htonl(INADDR_LOOPBACK);
	}
	else
	{
		// Try decoding a dotted ip address string
		ip = inet_addr(name);

		if (ip == INADDR_NONE)
		{
			hostent* entry;

			// Perform a DNS lookup
			entry = gethostbyname(name);

			if (entry)
			{
				ip = *(unsigned long*) entry->h_addr;
			}
		}
	}

#ifdef HAVE_SIN_LEN
	addr_in->sin_len			= sizeof (*addr_in);
#endif
	addr_in->sin_family 		= AF_INET;
	addr_in->sin_port			= htons (fPort);
	addr_in->sin_addr.s_addr	= ip;

	return addr;
}
