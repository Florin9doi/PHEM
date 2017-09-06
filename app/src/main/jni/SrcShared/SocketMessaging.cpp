/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "SocketMessaging.h"

#include "EmException.h"		// EmExceptionReset
#include "EmSession.h"			// EmSessionStopper
#include "Logging.h"			// LogAppendMsg

#include <algorithm>			// find()

#if PLATFORM_MAC
#include <errno.h>				// ENOENT, errno
#include <sys/types.h>			// u_short, ssize_t, etc.
#include <sys/socket.h>			// sockaddr
#include <sys/errno.h>			// Needed for error translation.
#include <sys/time.h>			// fd_set
#include <netdb.h>				// hostent
#include <unistd.h>				// close
#include <sys/filio.h>			// FIONBIO
#include <sys/ioctl.h>			// ioctl
#include <netinet/in.h>			// sockaddr_in
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
/* Update for GCC 4 */
#include <string.h>
#endif

// ---------------------------------------------------------------------------
//		¥ Stuff
// ---------------------------------------------------------------------------

#define PRINTF	if (!LogLLDebugger ()) ; else LogAppendMsg


// ---------------------------------------------------------------------------
//		¥ CSocket
// ---------------------------------------------------------------------------

typedef vector<CSocket*>	SocketList;
static SocketList			gSockets;
static SocketList			gSocketsToBeAdded;


/***********************************************************************
 *
 * FUNCTION:	CSocket::Startup
 *
 * DESCRIPTION: Create all the sockets we'll need for the application
 *				and start them listening for clients.  Call this once
 *				at application startup.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void CSocket::Startup (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	CSocket::Shutdown
 *
 * DESCRIPTION: Close all the sockets we've created.  Call this once at
 *				application shutdown.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void CSocket::Shutdown (void)
{
	// Add any sockets waiting to be added to gSockets.

	CSocket::AddPending ();

	SocketList::iterator	 iter = gSockets.begin ();
	while (iter != gSockets.end ())
	{
		CSocket* s = *iter;

		if (!s->Deleted())
		{
			s->Close ();
			s->Delete();
		}

		++iter;
	}

	CSocket::DeletePending ();

	EmAssert (gSockets.size() == 0);
	EmAssert (gSocketsToBeAdded.size() == 0);
}


/***********************************************************************
 *
 * FUNCTION:	CSocket::IdleAll
 *
 * DESCRIPTION: Idle all of the created sockets.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode CSocket::IdleAll (void)
{
	// Prevent recursion
	static Boolean	inIdleAll;
	if (inIdleAll)
		return errNone;

	inIdleAll = true;

//	PRINTF ("CSocket::IdleAll -- calling AddPending (1)...");
	CSocket::AddPending ();

//	PRINTF ("CSocket::IdleAll -- calling DeletePending (1)...");
	CSocket::DeletePending ();

	// Iterate over all the (non-deleted) sockets and Idle them.

	ErrCode					err = errNone;
	SocketList::iterator	iter = gSockets.begin ();

	while (iter != gSockets.end ())
	{
		if (!(*iter)->Deleted ())
		{
//			PRINTF ("CSocket::IdleAll -- Idling (0x%08X)...", (*iter));
			err = (*iter)->Idle ();
			if (err != errNone)
				break;
		}

		++iter;
	}

//	PRINTF ("CSocket::IdleAll -- calling AddPending (2)...");
	CSocket::AddPending ();

//	PRINTF ("CSocket::IdleAll -- calling DeletePending (2)...");
	CSocket::DeletePending ();

	inIdleAll = false;

	return err;
}


/***********************************************************************
 *
 * FUNCTION:    CSocket::AddPending
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void CSocket::AddPending (void)
{
	SocketList::iterator	iter = gSocketsToBeAdded.begin ();
	while (iter != gSocketsToBeAdded.end ())
	{
		gSockets.push_back (*iter);
		++iter;
	}

	gSocketsToBeAdded.clear();
}


/***********************************************************************
 *
 * FUNCTION:    CSocket::DeletePending
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/


void CSocket::DeletePending (void)
{
	// Delete any sockets pending to be deleted.

	SocketList::iterator	iter = gSockets.begin ();

	while (iter != gSockets.end ())
	{
		if ((*iter)->Deleted ())
		{
			delete *iter;
			iter = gSockets.erase(iter);
			continue;
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	CSocket::CSocket
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

CSocket::CSocket (void) :
	fDeleted (false)
{
	gSocketsToBeAdded.push_back (this);
}


/***********************************************************************
 *
 * FUNCTION:	CSocket::~CSocket
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

CSocket::~CSocket (void)
{
	EmAssert (fDeleted);
}


/***********************************************************************
 *
 * FUNCTION:    CSocket::Delete
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void CSocket::Delete (void)
{
	PRINTF ("CSocket(0x%08X)::Delete...", this);
//	EmAssert (!fDeleted);
	fDeleted = true;
}


/***********************************************************************
 *
 * FUNCTION:    CSocket::Deleted
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

Bool CSocket::Deleted (void) const
{
	return fDeleted;
}


/***********************************************************************
 *
 * FUNCTION:	CSocket::ShortPacketHack
 *
 * DESCRIPTION: In general, we should be using the full Serial Link
 *				Protocol format, which includes a header, body, and
 *				footer.  However, for stupid reasons, I was too lazy
 *				to implement the protocol for all transports, and
 *				omitted the footer and didn't fill in the header
 *				checksum field.  The SLP sub-system needs to know if
 *				the guy at the other end of this socket is expecting
 *				the full protocol or just the abbreviated one.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True for full packets (checksums and footers).
 *
 ***********************************************************************/

Bool CSocket::ShortPacketHack (void)
{
	return false;
}


/***********************************************************************
 *
 * FUNCTION:	CSocket::ByteswapHack
 *
 * DESCRIPTION: In general, all data is sent in Big-Endian format.
 *				However, due to a bug, some fields in some packets
 *				didn't get byteswapped.  This bug was fixed when using
 *				TCP sockets as the transport medium, but people
 *				connecting via other methods already expect the broken
 *				format, so we have to still support that for them.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True for mis-byteswapped packets.
 *
 ***********************************************************************/

Bool CSocket::ByteswapHack (void)
{
	return false;
}


// ---------------------------------------------------------------------------
//		¥ CTCPSocket
// ---------------------------------------------------------------------------

enum { kSocketState_Unconnected, kSocketState_Listening, kSocketState_Connected };

/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::CTCPSocket
 *
 * DESCRIPTION: Creates and initializes the socket object.	Nothing else
 *				is attempted at this time (that is, no underlying
 *				SOCKET objects are created, no listening is performed,
 *				no connections are attempted).
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

CTCPSocket::CTCPSocket (EventCallback fn, int port) :
	fEventCallback (fn),
	fPort (port),
	fSocketState (kSocketState_Unconnected),
	fListeningSocket (INVALID_SOCKET),
	fConnectedSocket (INVALID_SOCKET)
{
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::~CTCPSocket
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

CTCPSocket::~CTCPSocket (void)
{
	EmAssert (fSocketState == kSocketState_Unconnected);
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::Open
 *
 * DESCRIPTION: Starts up the sockets mechanism for communicating with
 *				the debugger.  It first attempts to connect to the
 *				debugger, assuming that it's been started up and issued
 *				a listen().  If that fails, it creates a socket to
 *				listen on itself.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 *				On any error, the sub-system will be closed down.  All
 *				sockets will be closed and the state will be set to
 *				unconnected.
 *
 ***********************************************************************/

ErrCode CTCPSocket::Open (void)
{
	PRINTF ("CTCPSocket(0x%08X)::Open...", this);

	EmAssert (fSocketState == kSocketState_Unconnected);
	EmAssert (fListeningSocket == INVALID_SOCKET);
	EmAssert (fConnectedSocket == INVALID_SOCKET);

	int 		result;
	sockaddr	addr;

#if TRY_TO_CONNECT_BEFORE_LISTENING
	fConnectedSocket = this->NewSocket ();
	if (fConnectedSocket == INVALID_SOCKET)
	{
		PRINTF ("...NewSocket failed for connecting socket; result = %08X", this->GetError ());
		return this->ErrorOccurred ();
	}


	// Attempt to connect to that address (in case anyone is listening).

	result = connect (fConnectedSocket, this->FillAddress (&addr, true), sizeof(addr));
	if (result == 0)
	{
		PRINTF ("...connected!");

		fSocketState = kSocketState_Connected;
		return errNone;
	}

	// Didn't connect; attempt to bind to that address so that we
	// can listen for connections.

	closesocket (fConnectedSocket);
	fConnectedSocket = INVALID_SOCKET;
#endif

	fListeningSocket = this->NewSocket ();
	if (fListeningSocket == INVALID_SOCKET)
	{
		PRINTF ("...NewSocket failed for listening socket; result = %08X", this->GetError ());
		return this->ErrorOccurred ();
	}

	result = bind (fListeningSocket, this->FillAddress (&addr, false), sizeof(addr));
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
 * FUNCTION:	CTCPSocket::Close
 *
 * DESCRIPTION: Closes the sockets session.  Closes all sockets, sets
 *				our state to unconnected, and closes down the sockets
 *				library (on Windows).
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode CTCPSocket::Close (void)
{
	PRINTF ("CTCPSocket(0x%08X)::Close...", this);

	Bool	needDisconnectNotification = false;

	if (fConnectedSocket != INVALID_SOCKET)
	{
		closesocket (fConnectedSocket);
		fConnectedSocket = INVALID_SOCKET;
		PRINTF ("...closed connection port...");
		needDisconnectNotification = fSocketState == kSocketState_Connected;
	}

	if (fListeningSocket != INVALID_SOCKET)
	{
		closesocket (fListeningSocket);
		fListeningSocket = INVALID_SOCKET;
		PRINTF ("...closed listening port...");
	}

	fSocketState = kSocketState_Unconnected;

	// Tell the callback function that the socket is now disconnected
	// (and not even listening).  Send out this notification at this
	// point so that the callback function can put the socket back into
	// the listening state if it wants.

	if (needDisconnectNotification && fEventCallback)
	{
		fEventCallback (this, kDisconnected);
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::Idle
 *
 * DESCRIPTION: Perform idle-time maintenance on this socket.  Mostly
 *				this means looking for any newly-arrived data.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode CTCPSocket::Idle (void)
{
	if (this->HasUnreadData (0))
	{
		if (fEventCallback)
		{
			try
			{
				fEventCallback (this, kDataReceived);
			}
			catch (const EmExceptionReset& e)
			{
				EmSessionStopper	stopper (gSession, kStopNow);
				e.Display ();
			}
		}
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::Write
 *
 * DESCRIPTION: Writes data out to the buffer.
 *
 * PARAMETERS:	buffer - pointer to the data to be written.
 *
 *				amountToWrite - number of bytes in the buffer.
 *
 * RETURNED:	Nothing.
 *
 *				On any error, the sub-system will be closed down.  All
 *				sockets will be closed and the state will be set to
 *				unconnected.
 *
 ***********************************************************************/

ErrCode CTCPSocket::Write (const void* buffer, long amountToWrite, long* amtWritten)
{
	PRINTF ("CTCPSocket(0x%08X)::Write...", this);

	long dummy;
	if (!amtWritten)
		amtWritten = &dummy;

	*amtWritten = 0;

	if (this->CheckConnection ())
	{
		if (LogLLDebuggerData ())
			LogAppendData (buffer, amountToWrite, "...writing %ld bytes.", amountToWrite);
		else
			PRINTF ("...writing %ld bytes.", amountToWrite);

		while (amountToWrite)
		{
			int sentThisTime = send (fConnectedSocket,
									&((char*) buffer)[*amtWritten],
									amountToWrite, 0);

			if (sentThisTime == SOCKET_ERROR )
			{
				PRINTF ("...error calling send: %08X", this->GetError ());

				return this->ErrorOccurred ();
			}

			*amtWritten += sentThisTime;
			amountToWrite -= sentThisTime;
		}
	}
	else
	{
		PRINTF ("...no connection.");
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::Read
 *
 * DESCRIPTION: Read bytes from the socket.  At most sizeOfBuffer bytes
 *				are read.  It is assumed that HasUnreadData has already
 *				been called and returned true.
 *
 * PARAMETERS:	buffer - pointer to the buffer to put the data.
 *
 *				sizeOfBuffer - size of the buffer.
 *
 * RETURNED:	The actual number of bytes read..
 *
 *				On any error, the sub-system will be closed down.  All
 *				sockets will be closed and the state will be set to
 *				unconnected.
 *
 ***********************************************************************/

ErrCode CTCPSocket::Read (void* buffer, long sizeOfBuffer, long* amtRead)
{
	PRINTF ("CTCPSocket(0x%08X)::Read...", this);

	long dummy;
	if (!amtRead)
		amtRead = &dummy;

	*amtRead = 0;

	if (this->CheckConnection ())
	{
		while (*amtRead < sizeOfBuffer)
		{
			long	r = recv (fConnectedSocket, ((char*) buffer) + *amtRead, sizeOfBuffer - *amtRead, 0);

			// More from the sockets manual for the select() function:
			//
			// For connection-oriented sockets, readability can also indicate that
			// a request to close the socket has been received from the peer. If the
			// virtual circuit was closed gracefully, and all data was received, then
			// a recv will return immediately with zero bytes read. If the virtual
			// circuit was reset, then a recv will complete immediately with an error
			// code such as WSAECONNRESET. The presence of OOB data will be checked
			// if the socket option SO_OOBINLINE has been enabled (see setsockopt).

			if (r == 0 && *amtRead == 0)
			{
				// OK, the connection was closed on the other side.  Close it on
				// our side, too.

				PRINTF ("...found a closed connection.");
				this->Close ();
				return errNone;
			}

			if (r == SOCKET_ERROR)
			{
				PRINTF ("...error calling recv: %08X", this->GetError ());

				return this->ErrorOccurred ();
			}

			if (r > 0)
			{
				*amtRead += r;

				if (LogLLDebuggerData ())
					LogAppendData (((char*) buffer) + *amtRead - r, r,
								"...got %ld of %ld bytes of data.", *amtRead, sizeOfBuffer);
				else
					PRINTF ("...got %ld of %ld bytes of data.", *amtRead, sizeOfBuffer);
			}
		}
	}
	else
	{
		PRINTF ("...no connection.");
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::ConnectPending
 *
 * DESCRIPTION: Returns whether or not there is a pending connect.	This
 *				function can be called at any time.  It will only return
 *				true if there is a non-blocking listen issued with a
 *				pending connect that we haven't yet accepted.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if there is a pending connect.  If there is, the
 *				caller should next call CTCPSocket::AcceptConnection.
 *
 ***********************************************************************/

Bool CTCPSocket::ConnectPending (void)
{
	// From the sockets manual for the select() function:
	//
	// The parameter readfds identifies the sockets that are to be
	// checked for readability. If the socket is currently in the
	// listen state, it will be marked as readable if an incoming
	// connection request has been received such that an accept is
	// guaranteed to complete without blocking. For other sockets,
	// readability means that queued data is available for reading
	// such that a call to recv, WSARecv, WSARecvFrom, or recvfrom
	// is guaranteed not to block.

	Bool	havePendingConnect = false;

#if 0
	if (fSocketState == kSocketState_Unconnected)
	{
		PRINTF ("CTCPSocket(0x%08X)::ConnectPending...", this);
		PRINTF ("...starting to listen for connection.");

		this->Open ();
	}
#endif

	if (fSocketState == kSocketState_Listening)
	{
		EmAssert (fListeningSocket != INVALID_SOCKET);
		EmAssert (fConnectedSocket == INVALID_SOCKET);

		fd_set	readfds;

		FD_ZERO (&readfds);
		FD_SET (fListeningSocket, &readfds);
		
		timeval timeout = {0, 0};	// Don't wait at all

		int result = select (fListeningSocket + 1, &readfds, NULL, NULL, &timeout);

		if (result > 0 && FD_ISSET (fListeningSocket, &readfds))
		{
			PRINTF ("CTCPSocket(0x%08X)::ConnectPending...", this);
			PRINTF ("...have pending connection.");

			havePendingConnect = true;
		}
		else if (result == SOCKET_ERROR)
		{
			PRINTF ("CTCPSocket(0x%08X)::ConnectPending...", this);
			PRINTF ("...error calling select: %08X", this->GetError ());

			result = this->GetError ();
		}
		else if (result > 0)
		{
			PRINTF ("CTCPSocket(0x%08X)::ConnectPending...", this);
			PRINTF ("...fell through cases in kSocketState_Listening");
		}
	}

	return havePendingConnect;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::AcceptConnection
 *
 * DESCRIPTION: Accepts a pending connection.  Assumes that
 *				ConnectPending has already been called and returned
 *				true.  After this, the socket connection should be open
 *				for business.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 *				On any error, the sub-system will be closed down.  All
 *				sockets will be closed and the state will be set to
 *				unconnected.
 *
 ***********************************************************************/

ErrCode CTCPSocket::AcceptConnection (void)
{
	PRINTF ("CTCPSocket(0x%08X)::AcceptConnection...", this);

	EmAssert (fSocketState == kSocketState_Listening);
	EmAssert (fListeningSocket != INVALID_SOCKET);
	EmAssert (fConnectedSocket == INVALID_SOCKET);

	sockaddr	addr;
	socklen_t 	addr_len;

	memset (&addr, 0, sizeof (addr));
	addr_len = sizeof(addr);

	fConnectedSocket = accept (fListeningSocket, &addr, &addr_len);

	if (fConnectedSocket == INVALID_SOCKET)
	{
		PRINTF ("...error calling accept: %08X", this->GetError ());
		return this->ErrorOccurred ();
	}

	closesocket (fListeningSocket);
	fListeningSocket = INVALID_SOCKET;

	// Set the socket to not use the "Nagle delay algorithm" that clumps
	// batches of small writes together into one big send.

	protoent*	protocolEntity = getprotobyname ("tcp");
	if (!protocolEntity)
	{
		PRINTF ("...error calling getprotobyname: %08X", this->GetError ());
		return this->ErrorOccurred ();
	}

#ifdef TCP_NODELAY
	int tmp = 1;
	if (setsockopt (fConnectedSocket, protocolEntity->p_proto, TCP_NODELAY, (char*) &tmp, sizeof (tmp)))
	{
		PRINTF ("...error calling setsockopt: %08X", this->GetError ());
		return this->ErrorOccurred ();
	}
#endif

	PRINTF ("...accepted the connection.");

	fSocketState = kSocketState_Connected;

	if (fEventCallback)
	{
		fEventCallback (this, kConnected);
	}

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::IsConnected
 *
 * DESCRIPTION: Returns whether or not we have a live connection to an
 *				external debugger via a socket.  This function can be
 *				called at any time.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 *				On any error, the sub-system will be closed down.  All
 *				sockets will be closed and the state will be set to
 *				unconnected.
 *
 ***********************************************************************/

Bool CTCPSocket::IsConnected (void)
{
	// If we thought we were connected, see if we still are.

	if (fSocketState == kSocketState_Connected)
	{
		// From the sockets manual for the select() function:
		//
		// If a socket is processing a connect call (nonblocking), a socket is
		// writable if the connection establishment successfully completes. If
		// the socket is not processing a connect call, writability means a send,
		// sendto, or WSASendto are guaranteed to succeed.

		fd_set	writefds;

		FD_ZERO (&writefds);
		FD_SET (fConnectedSocket, &writefds);

		timeval timeout = {0, 0};	// Don't wait at all

		int 	result = select (fConnectedSocket+1, NULL, &writefds, NULL, &timeout);

		Bool	isConnected = result > 0 && FD_ISSET (fConnectedSocket, &writefds);

		// If we're no longer connected, update our internal state.

		if (!isConnected)
		{
			PRINTF ("CTCPSocket(0x%08X)::IsConnected...", this);
			PRINTF ("...found a broken connection, or error calling select: %08X", this->GetError ());

			this->ErrorOccurred ();
		}
	}

	return fSocketState == kSocketState_Connected;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::CheckConnection
 *
 * DESCRIPTION: Attempt to ensure that a connection is in place.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if a connection is in place.  False otherwise.
 *
 ***********************************************************************/

Bool CTCPSocket::CheckConnection (void)
{
	if (!this->IsConnected() && this->ConnectPending ())
	{
		this->AcceptConnection ();
	}

	return this->IsConnected ();
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::HasUnreadData
 *
 * DESCRIPTION: Returns whether or not there is data to be read from
 *				the socket.
 *
 * PARAMETERS:	msecs - time to wait in milliseconds for any data.
 *
 * RETURNED:	True if there is data to be read.  There is no way to
 *				get how many bytes are currently in the buffer.
 *
 *				On any error, the sub-system will be closed down.  All
 *				sockets will be closed and the state will be set to
 *				unconnected.
 *
 ***********************************************************************/

Bool CTCPSocket::HasUnreadData (long msecs)
{
	Bool	hasData = false;

	// From the sockets manual for the select() function:
	//
	// The parameter readfds identifies the sockets that are to be
	// checked for readability. If the socket is currently in the
	// listen state, it will be marked as readable if an incoming
	// connection request has been received such that an accept is
	// guaranteed to complete without blocking. For other sockets,
	// readability means that queued data is available for reading
	// such that a call to recv, WSARecv, WSARecvFrom, or recvfrom
	// is guaranteed not to block.

	if (this->CheckConnection ())
	{
		EmAssert (fSocketState == kSocketState_Connected);

		fd_set	readfds;

		FD_ZERO (&readfds);
		FD_SET (fConnectedSocket, &readfds);

		timeval timeout = {msecs / 1000, msecs * 1000};

		int result = select (fConnectedSocket+1, &readfds, NULL, NULL, &timeout);

		if (result > 0 && FD_ISSET (fConnectedSocket, &readfds))
		{
			PRINTF ("CTCPSocket(0x%08X)::HasUnreadData...", this);
			PRINTF ("...has pending data.");

			hasData = true;
		}
		else if (result == SOCKET_ERROR)
		{
			PRINTF ("CTCPSocket(0x%08X)::HasUnreadData...", this);
			PRINTF ("...error calling select: %08X", this->GetError ());

			this->ErrorOccurred ();
			return false;
		}
	}

	return hasData;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::NewSocket
 *
 * DESCRIPTION: Creates and returns a new socket configured the way we
 *				like it.  The way we like it is: reusable after it's
 *				closed, uses "keep alive" packets, and non-blocking.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The created socket, or INVALID_SOCKET on error.
 *
 *				On any error, the sub-system will be closed down.  All
 *				sockets will be closed and the state will be set to
 *				unconnected.
 *
 ***********************************************************************/

SOCKET CTCPSocket::NewSocket (void)
{
	PRINTF ("CTCPSocket(0x%08X)::NewSocket...", this);

	int 	tmp;
	int 	result;

	SOCKET	newSocket = socket (AF_INET, SOCK_STREAM, 0);
	if (newSocket == INVALID_SOCKET)
	{
		PRINTF ("...error calling socket: %08X", this->GetError ());
		goto Error;
	}

	// Allow rapid reuse of this port.

	tmp = 1;
	result = setsockopt (newSocket, SOL_SOCKET, SO_REUSEADDR, (char*) &tmp, sizeof(tmp));
	if (result != 0)
	{
		PRINTF ("...error calling setsockopt (SO_REUSEADDR): %08X", this->GetError ());
		goto Error;
	}

	// Enable TCP keep alive process.

	tmp = 1;
	result = setsockopt (newSocket, SOL_SOCKET, SO_KEEPALIVE, (char*) &tmp, sizeof(tmp));
	if (result != 0)
	{
		PRINTF ("...error calling setsockopt (SO_KEEPALIVE): %08X", this->GetError ());
		goto Error;
	}

	return newSocket;

Error:
	if (newSocket != INVALID_SOCKET)
	{
		closesocket (newSocket);
	}

	return INVALID_SOCKET;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::FillAddress
 *
 * DESCRIPTION: Fill in a sockaddr data structure with values
 *				appropriate for connecting.  For now, this is an
 *				Internet address on the local machine with port 2000.
 *
 * PARAMETERS:	addr - the sockaddr to fill in.
 *
 * RETURNED:	The same sockaddr passed in.
 *
 ***********************************************************************/

sockaddr* CTCPSocket::FillAddress (sockaddr* addr, bool forConnect)
{
	PRINTF ("CTCPSocket(0x%08X)::FillAddress...", this);

	sockaddr_in*	addr_in = (sockaddr_in*) addr;

#ifdef HAVE_SIN_LEN
	addr_in->sin_len			= sizeof (*addr_in);
#endif
	addr_in->sin_family 		= AF_INET;
	addr_in->sin_port			= htons (fPort);
	addr_in->sin_addr.s_addr	= htonl (forConnect ? INADDR_LOOPBACK : INADDR_ANY);

	return addr;
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::ErrorOccurred
 *
 * DESCRIPTION: Handles any internal errors.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

ErrCode CTCPSocket::ErrorOccurred (void)
{
	ErrCode result = this->GetError ();

	this->Close ();

	return result;	// Return the original error code.
}


/***********************************************************************
 *
 * FUNCTION:	CTCPSocket::GetError
 *
 * DESCRIPTION: Returns the error code for the last operation.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The error code as returned by WSAGetLastError (on
 *				Windows) or errno (on the Mac or UNIX).
 *
 ***********************************************************************/

ErrCode CTCPSocket::GetError (void)
{
#if defined (_WIN32)
	return (ErrCode) ::WSAGetLastError ();
#else
	return (ErrCode) errno;
#endif
}
