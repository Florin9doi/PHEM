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
#include "Platform_NetLib.h"

#include "PreferenceMgr.h"		// Preference
#include "Byteswapping.h"		// Canonical
#include "Logging.h"			// LogAppendMsg
#include "Miscellaneous.h"		// StMemory
#include "Platform.h"			// AllocateMemory
#include "ROMStubs.h"			// NetLibConfigMakeActive


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
#include <netinet/tcp.h>		// TCP_NODELAY
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
#include <cstddef>
#endif

#if defined(__svr4__)
#include <sys/filio.h>			// FIONBIO
#endif

#define PRINTF	if (!LogNetLib ()) ; else LogAppendMsg

// ===== COPIED FROM NETPRV.H =====

// Max # of open built-in sockets at one time.
#define		netMaxNumSockets	4								// Max # of sockets
#define		netMinSocketRefNum	(sysFileDescNet)				// lowest libRefNum
#define		netMaxSocketRefNum	(sysFileDescNet+netMaxNumSockets-1) // highest libRefNum

// The sockets maintained by NetLib plug-ins are numbered above the built-in
// sockets.
#define		netMaxNumPISockets	4						// Max # of Plug-In sockets
#define		netMinPISocketRefNum	(netMaxSocketRefNum+1)					// lowest libRefNum
#define		netMaxPISocketRefNum (netMinPISocketRefNum+netMaxNumPISockets-1)


// A NetSocketRef is the index into an array holding the pointers to
// socket data.  Because NetSocketRefs should not be zero, the refNums
// actually start at sysFileDescNet (which happens to be 1).
//
// The NetLib headers are sensitive to this numbering.  In particular,
// netFDSet, et al, assume a certain number range in order to set bits
// in the NetFDSetType object.
//
// Therefore, we can't just return the results of the socket() function as
// the result of NetLibSocketOpen().  Though an opaque type, the range of
// values returned by socket() is outside the range allowed by netFDSet.
// So, we have to map the SOCKET returned by socket() to something allowed
// by NetLib.  We store the SOCKETs in the array below, and return the
// index of the SOCKET, just as NetLib would.

SOCKET	gSockets[netMaxNumSockets] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };


// The following are copied from WinSock2.h.  We don't include that file
// here because it can't be included in the same scope as WinSock.h, which
// Windows.h pulls in.

#define SD_RECEIVE		0x00
#define SD_SEND			0x01
#define SD_BOTH			0x02

#if PLATFORM_WINDOWS
extern WORD	gWinSockVersion;
#endif

static long	gOpenCount;
static UInt16	gCurConfigIndex;

static Bool	NetLibToSocketsRef			(const NetSocketRef				inSocket,
										 SOCKET&						outSocket);

static Bool	RememberSocketsRef			(const SOCKET					inSocket,
										 NetSocketRef&					outSocket);
static Bool	ForgetSocketsRef			(NetSocketRef					sRef);

static Bool	NetLibToSocketsDomain		(const NetSocketAddrEnum		inDomain,
										 u_short&						outDomain);

static Bool	NetLibToSocketsType			(const NetSocketTypeEnum		inType,
										 int&							outType);
static Bool	NetLibToSocketsProtocol		(const Int16					inProtocol,
										 int&							outProtocol);
static Bool	NetLibToSocketsDirection	(const Int16					inDirection,
										 int&							outDirection);
static Bool NetLibToSocketsFlags		(const UInt16					inFlags,
										 int&							outFlags);

static Bool	NetLibToSocketsOptions		(const NetSocketOptLevelEnum	inOptLevel,
										 const NetSocketOptEnum			inOptName,
										 const MemPtr					inOptVal,
										 const UInt16					inOptValLen,
										 int&							outOptLevel,
										 int&							outOptName,
										 char*&							outOptVal,
										 socklen_t&						outOptValLen);
static Bool	SocketsToNetLibOptions		(const int						inOptLevel,
										 const int						inOptName,
										 /*const*/ char*&				inOptVal,
										 const int						inOptValLen,
										 NetSocketOptLevelEnum			outOptLevel,
										 NetSocketOptEnum				outOptName,
										 MemPtr							outOptVal,
										 UInt16							outOptValLen);

static Bool	NetLibToSocketsAddr			(const NetSocketAddrType&		inAddr,
										 const Int16					inAddrLen,
										 sockaddr&						outAddr,
										 int							outAddrLen);
static Bool	SocketsToNetLibAddr			(const sockaddr&				inAddr,
										 int							inAddrLen,
										 NetSocketAddrType&				outAddr,
										 Int16							outAddrLen);

static Bool SocketsToNetLibHostEnt		(const hostent&					inHostEnt,
										 NetHostInfoBufType&			outHostEnt);
static Bool SocketsToNetLibServEnt		(const servent&					inServEnt,
										 NetServInfoBufType&			outServEnt);

static Bool NetLibToSocketsFDSet		(const NetFDSetType*			inFDSet,
										 UInt16							inWidth,
										 fd_set*&						outFSSet,
										 int&							outWidth);
static Bool SocketsToNetLibFDSet		(const fd_set*					inFDSet,
										 int							inWidth,
										 NetFDSetType*					outFSSet,
										 UInt16&						outWidth);

static Bool NetLibToSocketsScalarOption (const MemPtr					inOptVal,
										 const UInt16					inOptValLen,
										 char*&							outOptVal,
										 socklen_t&						outOptValLen);

static Bool SocketsToNetLibScalarOption (/*const*/ char*&				inOptVal,
										 const int						inOptValLen,
										 MemPtr							outOptVal,
										 UInt16							outOptValLen);

static uint32	PrvGetError (void);
static Err		PrvGetTranslatedError (void);
#if PLATFORM_MAC
static Err		PrvGetTranslatedHError (void);
#endif
static Err		PrvTranslateError (uint32 err);



/***********************************************************************
 *
 * FUNCTION:	Platform_NetLib::Initialize
 *
 * DESCRIPTION:	NetLib redirection subsystem initialization routine
 *
 * PARAMETERS:	
 *
 * RETURNED:	
 *
 ***********************************************************************/

void Platform_NetLib::Initialize	(void)
{
}


/***********************************************************************
 *
 * FUNCTION:	Platform_NetLib::Reset
 *
 * DESCRIPTION:	NetLib redirection subsystem reset routine
 *
 * PARAMETERS:	
 *
 * RETURNED:	
 *
 ***********************************************************************/

void Platform_NetLib::Reset		(void)
{
	Dispose();
}


/***********************************************************************
 *
 * FUNCTION:	Platform_NetLib::Save
 *
 * DESCRIPTION:	NetLib redirection subsystem state saving routine
 *
 * PARAMETERS:	
 *
 * RETURNED:	
 *
 ***********************************************************************/

void Platform_NetLib::Save		(SessionFile&)
{
}


/***********************************************************************
 *
 * FUNCTION:	Platform_NetLib::Load
 *
 * DESCRIPTION:	NetLib redirection subsystem state loading routine
 *
 * PARAMETERS:	
 *
 * RETURNED:	
 *
 ***********************************************************************/

void Platform_NetLib::Load		(SessionFile&)
{
}


/***********************************************************************
 *
 * FUNCTION:	Platform_NetLib::Dispose
 *
 * DESCRIPTION:	NetLib redirection subsystem cleanup routine
 *
 * PARAMETERS:	
 *
 * RETURNED:	
 *
 ***********************************************************************/

void Platform_NetLib::Dispose		(void)
{
	for (int ii = 0; ii < netMaxNumSockets; ++ii)
	{
		if (gSockets[ii] != INVALID_SOCKET)
		{
			closesocket (gSockets[ii]);
			gSockets[ii] = INVALID_SOCKET;
		}
	}

	gOpenCount = 0;
}


Bool Platform_NetLib::Redirecting (void)
{
	Preference<bool>	pref (kPrefKeyRedirectNetLib);
	return *pref;
}


//--------------------------------------------------
// Library initialization, shutdown, sleep and wake
//--------------------------------------------------

Err Platform_NetLib::Open(UInt16 libRefNum, UInt16* netIFErrsP)
{
	UNUSED_PARAM(libRefNum)

	// Init return error 
	*netIFErrsP = 0;

#if PLATFORM_WINDOWS
	if (gWinSockVersion == 0)
	{
		return netErrInternal;
	}
#endif

	if (gOpenCount > 0)
	{
		++gOpenCount;
		return netErrAlreadyOpen;
	}

	++gOpenCount;
	return errNone;
}

Err Platform_NetLib::OpenConfig(UInt16 libRefNum, UInt16 configIndex, UInt32 openFlags,
						UInt16* netIFErrP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(openFlags)

	Err		err = errNone;
	Boolean needRefresh = false;

	// Init return error 
	*netIFErrP = errNone;

	//-----------------------------------------------------------------
	// We now have a "lock" on the globals and have prevented all other tasks
	//  from initiating an open or close until we release the lock.
	// Now, see what the current state of the library is and take the
	//  appropriate action
	// If it's already open make sure it's a compatible configuration,
	//  if not, it's an error
	// If we're in the closeWait state see if it's
	//  a compatible configuration, if it is just bump it's open
	//  count and return. If it's not, finish the close before we
	//  change the configuration
	//-----------------------------------------------------------------
	if (gOpenCount > 0)
	{
		// If we can use the current state...
		if (configIndex == netConfigIndexCurSettings || configIndex == gCurConfigIndex)
		{
			gOpenCount++;
			if (gOpenCount > 1)
				err = netErrAlreadyOpen;

			// Refresh connections if necessary 
			needRefresh = true;
			goto Exit;
		}

		err = netErrAlreadyOpenWithOtherConfig;
		goto Exit;
	}


	//-----------------------------------------------------------------
	// Activate the new configuration. This routine will initialize
	//  the config if it's one of our pre-defined ones that we
	//  know how to initialize and it's the first time we've used it. 
	//
	// It will also save the current configuration, update the 
	//  curConfigIndex field of the config table with the new config
	//  index, and  set the restoreConfig index to the old index
	//  so that PrvClose() in the protocol task can restore the 
	//  current configuration when it closes the NetLib. 
	//-----------------------------------------------------------------
	if (configIndex != netConfigIndexCurSettings)
	{
		err = NetLibConfigMakeActive (libRefNum, configIndex);
			// !!! Doesn't do the restoreOldConfigOnClose stuff...
		if (err)
			goto Exit;

		gCurConfigIndex = configIndex;
	}

	
	//-----------------------------------------------------------------
	// Do the guts of the open. This routine will leave the globals
	//  locked. If it's successful, it will return the new "realGlobals"
	//  ptr. If not, it will leave the shadow globals
	//-----------------------------------------------------------------
	err = Platform_NetLib::Open (libRefNum, netIFErrP);

	
	//-----------------------------------------------------------------
	// Exit now..
	//-----------------------------------------------------------------
Exit:

	// See if we should refresh any connections
	if (!err && needRefresh)
	{
		Boolean	allIFsUp;
		err = Platform_NetLib::ConnectionRefresh (libRefNum, true, &allIFsUp, netIFErrP);
	}

	return err;
}

Err Platform_NetLib::Close(UInt16 libRefNum, UInt16 immediate)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(immediate)

	if (gOpenCount <= 0)
	{
		return netErrNotOpen;
	}

	--gOpenCount;

	return errNone;
}


Err Platform_NetLib::Sleep(UInt16 libRefNum)
{
	UNUSED_PARAM(libRefNum)

	// stub. Do Nothing
	return errNone;
}


Err Platform_NetLib::Wake(UInt16 libRefNum)
{
	UNUSED_PARAM(libRefNum)

	// stub. Do Nothing
	return errNone;
}


// This call forces the library to complete a close if it's
//  currently in the close-wait state. Returns 0 if library is closed,
//  Returns netErrFullyOpen if library is still open by some other task.

Err Platform_NetLib::FinishCloseWait(UInt16 libRefNum)
{
	UNUSED_PARAM(libRefNum)

	// stub. Do Nothing
	return errNone;
}


// This call is for use by the Network preference panel only. It
// causes the NetLib to fully open if it's currently in the close-wait
//  state. If it's not in the close wait state, it returns an error code

Err Platform_NetLib::OpenIfCloseWait(UInt16 libRefNum)
{
	UNUSED_PARAM(libRefNum)

	return netErrNotOpen;
}


// Get the open Count of the NetLib

Err Platform_NetLib::OpenCount(UInt16 libRefNum, UInt16* countP)
{
	UNUSED_PARAM(libRefNum)

	*countP = gOpenCount;
	return errNone;
}


// Give NetLib a chance to close the connection down in response
// to a power off event. Returns non-zero if power should not be
//  turned off. EventP points to the event that initiated the power off
//  which is either a keyDownEvent of the hardPowerChr or the autoOffChr.
// Don't include unless building for Viewer

Err Platform_NetLib::HandlePowerOff (UInt16 libRefNum, EventPtr eventP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(eventP)

	Err	err = Platform_NetLib::Close (libRefNum, true);

	// Don't let the fact that we're not open prevent us from powering off.
	if (err == netErrNotOpen)
		err = errNone;

	return err;
}


// Check status or try and reconnect any interfaces which have come down.
// This call can be made by applications when they suspect that an interface
// has come down (like PPP or SLIP). NOTE: This call can display UI
// (if 'refresh' is true) so it MUST be called from the UI task.

Err Platform_NetLib::ConnectionRefresh(UInt16 libRefNum,
										Boolean refresh,
										Boolean* allInterfacesUpP,
										UInt16* netIFErrP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(refresh)

	*allInterfacesUpP = true;
	*netIFErrP =0;
	return errNone;
}


//--------------------------------------------------
// Socket creation and option setting
//--------------------------------------------------

// Create a socket and return a refnum to it. Protocol is normally 0.
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

NetSocketRef Platform_NetLib::SocketOpen(UInt16 libRefNum,
										NetSocketAddrEnum socketDomain,
										NetSocketTypeEnum socketType,
										Int16 socketProtocol,
										Int32 timeout,
										Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	u_short	af;
	int		type;
	int		protocol;

	if (!NetLibToSocketsDomain (socketDomain, af) ||
		!NetLibToSocketsType (socketType, type) ||
		!NetLibToSocketsProtocol (socketProtocol, protocol))
	{
		*errP = netErrParamErr;
		return -1;
	}

	SOCKET	s = socket (af, type, protocol);

	if (s == INVALID_SOCKET)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	NetSocketRef	result;
	if (!RememberSocketsRef (s, result))
	{
		// !!! Need to close the socket.  But we should never
		// really get here...we allow as many sockets at NetLib
		// does.  I guess if NetLib raised the number of sockets
		// and we don't, then we'd get here, though...
		*errP = netErrNoMoreSockets;
		return -1;
	}

	*errP = 0;
	return result;
}


// Close a socket. 
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketClose(UInt16 libRefNum,
									NetSocketRef sRef,
									Int32 timeout,
									Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	SOCKET	s;
	if (!NetLibToSocketsRef (sRef, s))
	{
		*errP = netErrParamErr;
		return -1;
	}

	int	result = closesocket (s);

	// Drop our reference to this socket.
	// !!! Do this even if closesocket fails?

	ForgetSocketsRef (sRef);

	if (result)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	*errP = 0;
	return result;
}


// Set a socket option.  Level is usually netSocketOptLevelSocket. Option is one of
//  netSocketOptXXXXX. OptValueP is a pointer to the new value and optValueLen is
//  the length of the option value.
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketOptionSet(UInt16 libRefNum,
										NetSocketRef sRef,
										NetSocketOptLevelEnum optLevel,
										NetSocketOptEnum opt,
										MemPtr optValueP,
										UInt16 optValueLen,
										Int32 timeout,
										Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	int	result;

	if (optLevel == netSocketOptLevelSocket &&
		opt == netSocketOptSockNonBlocking)
	{
		SOCKET	s;
		if (!NetLibToSocketsRef (sRef, s))
		{
			*errP = netErrParamErr;
			return -1;
		}

#if PLATFORM_WINDOWS
		unsigned long	param;
#else
		long			param;
#endif

		if (optValueLen == 1)
			param = *(char*) optValueP;
		else if (optValueLen == 2)
			param = *(short*) optValueP;
		else if (optValueLen == 4)
			param = *(long*) optValueP;
		else
			param = 1;

		result = ioctlsocket (s, FIONBIO, &param);
	}
	else
	{
		SOCKET		s;
		int			level;
		int			optName;
		char*		optVal;
		socklen_t	optLen;

		if (!NetLibToSocketsRef (sRef, s) ||
			!NetLibToSocketsOptions (optLevel, opt, optValueP, optValueLen, level, optName, optVal, optLen))
		{
			*errP = netErrParamErr;
			return -1;
		}

		result = setsockopt (s, level, optName, optVal, optLen);

		Platform::DisposeMemory (optVal);
	}

	if (result)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	*errP = 0;
	return result;
}


// Get a socket option.
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketOptionGet(UInt16 libRefNum,
										NetSocketRef sRef,
										NetSocketOptLevelEnum optLevel,
										NetSocketOptEnum opt,
										MemPtr optValueP,
										UInt16* optValueLenP,
										Int32 timeout,
										Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	SOCKET		s;
	int			level;
	int			optName;
	char*		optVal;
	socklen_t	optLen = *optValueLenP;

	if (!NetLibToSocketsRef (sRef, s) ||
		!NetLibToSocketsOptions (optLevel, opt, optValueP, *optValueLenP, level, optName, optVal, optLen))
	{
		*errP = netErrParamErr;
		return -1;
	}

	int	result	= getsockopt (s, level, optName, optVal, &optLen);

	if (result)
	{
		Platform::DisposeMemory (optVal);
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	if (!SocketsToNetLibOptions (level, optName, optVal, optLen, optLevel, opt, optValueP, *optValueLenP))
	{
		*errP = netErrParamErr;
		return -1;
	}

	*errP = 0;
	return result;
}


//--------------------------------------------------
// Socket Control
//--------------------------------------------------

// Bind a source address and port number to a socket. This makes the
//  socket accept incoming packets destined for the given socket address.
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketBind(UInt16 libRefNum,
									NetSocketRef sRef,
									NetSocketAddrType* sockAddrP,
									Int16 addrLen,
									Int32 timeout,
									Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	SOCKET		s;
	sockaddr	name;
	int			namelen = sizeof (name);

	if (!NetLibToSocketsRef (sRef, s) ||
		!NetLibToSocketsAddr (*sockAddrP, addrLen, name, namelen))
	{
		*errP = netErrParamErr;
		return -1;
	}

	int	result	= bind (s, &name, namelen);

	if (result)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	*errP = 0;
	return result;
}


// Connect to a remote socket. For a stream based socket (i.e. TCP), this initiates
//  a 3-way handshake with the remote machine to establish a connection. For
//  non-stream based socket, this merely specifies a destination address and port
//  number for future outgoing packets from this socket.
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketConnect(UInt16 libRefNum,
										NetSocketRef sRef,
										NetSocketAddrType* sockAddrP,
										Int16 addrLen,
										Int32 timeout,
										Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	SOCKET		s;
	sockaddr	name;
	int			namelen = sizeof (name);

	if (!NetLibToSocketsRef (sRef, s) ||
		!NetLibToSocketsAddr (*sockAddrP, addrLen, name, namelen))
	{
		*errP = netErrParamErr;
		return -1;
	}

	int	result	= connect (s, &name, namelen);

	if (result)
	{
		// Work around Windows 2000 bug.  According to Mark Baysinger
		// (<mbaysing@qualcomm.com>):
		//
		//		Windows 2000 seems to return the wrong error code from connect() with
		//		non-blocking IO when the connection is already in progress.  (It returns
		//		WSAEINVAL instead of WSAEALREADY.)  This results in NetLibSocketConnect
		//		returning netErrParamErr instead of netErrSocketAlreadyConnected."
		//
		// However, according to the connect() documentation on MSDN:
		//
		//		Until the connection attempt completes on a nonblocking socket, all
		//		subsequent calls to connect on the same socket will fail with the error
		//		value WSAEALREADY, and WSAEISCONN when the connection completes
		//		successfully. Due to ambiguities the Windows Sockets Specification 1.1,
		//		error values returned from connect while a connection is already pending
		//		may vary among implementations. As a result, it is not recommended that
		//		applications use multiple calls to connect to detect connection
		//		completion. If they do, they must be prepared to handle WSAEINVAL and
		//		WSAEWOULDBLOCK error values the same way that they handle WSAEALREADY,
		//		to assure robust execution.
		//
		// So it looks like (a) there are other result codes to look for, and (b)
		// we shouldn't consider only Windows 2000.
		//
		// Further Notes:
		//
		// In the connect documentation, it says:
		//
		//		With a nonblocking socket, the connection attempt cannot be completed
		//		immediately. In this case, connect will return SOCKET_ERROR, and
		//		WSAGetLastError will return WSAEWOULDBLOCK. In this case, there are
		//		three possible scenarios: <snip>
		//
		// This means that WSAEWOULDBLOCK should not be translated to WSAEALREADY.


#if PLATFORM_WINDOWS
		uint32	err = ::PrvGetError ();

		if (err == WSAEINVAL /*|| err == WSAEWOULDBLOCK*/)
		{
			err = WSAEALREADY;
		}

		*errP = PrvTranslateError (err);
#else
		*errP = PrvGetTranslatedError ();
#endif
		return -1;
	}

	*errP = 0;
	return result;
}

// Makes a socket ready to accept incoming connection requests. The queueLen 
//  specifies the max number of pending connection requests that will be enqueued
//  while the server is busy handling other requests.
//  Only applies to stream based (i.e. TCP) sockets.
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketListen(UInt16 libRefNum,
										NetSocketRef sRef,
										UInt16 queueLen,
										Int32 timeout,
										Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	SOCKET	s;
	if (!NetLibToSocketsRef (sRef, s))
	{
		*errP = netErrParamErr;
		return -1;
	}

	int	result = listen (s, queueLen);

	if (result)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	*errP = 0;
	return result;
}

// Blocks the current process waiting for an incoming connection request. The socket
//  must have previously be put into listen mode through the NetLibSocketListen call.
//  On return, *sockAddrP will have the remote machines address and port number.
//  Only applies to stream based (i.e. TCP) sockets.
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketAccept(UInt16 libRefNum,
									NetSocketRef sRef,
									NetSocketAddrType* sockAddrP,
									Int16* addrLenP,
									Int32 timeout,
									Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	SOCKET		s;
	sockaddr	addr;
	socklen_t	addrlen = sizeof(addr);

	if (!NetLibToSocketsRef (sRef, s))
	{
		*errP = netErrParamErr;
		return -1;
	}

	int	new_s = accept (s, &addr, &addrlen);

	if (new_s == INVALID_SOCKET)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	NetSocketRef	result;
	if (!RememberSocketsRef (new_s, result))
	{
		// !!! Need to un-accept?
		*errP = netErrParamErr;
		return -1;
	}

	if (!SocketsToNetLibAddr (addr, addrlen, *sockAddrP, *addrLenP))
	{
		// !!! Need to un-accept?
		*errP = netErrParamErr;
		return -1;
	}

	*errP = 0;
	return (Int16) result;
}

// Shutdown a connection in one or both directions.  
//  Only applies to stream based (i.e. TCP) sockets.
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketShutdown(UInt16 libRefNum,
										NetSocketRef sRef,
										Int16 direction,
										Int32 timeout,
										Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	SOCKET	s;
	int		how;
	
	if (!NetLibToSocketsRef (sRef, s) ||
		!NetLibToSocketsDirection (direction, how))
	{
		*errP = netErrParamErr;
		return -1;
	}

	int	result = shutdown (s, how);

	if (result)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	*errP = 0;
	return result;
}


// Gets the local and remote addresses of a socket. Useful for TCP sockets that 
//  get dynamically bound at connect time. 
// Returns 0 on success, -1 on error. If error, *errP gets filled in with error code.

Int16 Platform_NetLib::SocketAddr(UInt16 libRefNum,
									NetSocketRef sRef,
									NetSocketAddrType* locAddrP,
									Int16* locAddrLenP,
									NetSocketAddrType* remAddrP,
									Int16* remAddrLenP,
									Int32 timeout,
									Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	int		result = 0;
	SOCKET	s;

	if (!NetLibToSocketsRef (sRef, s))
	{
		*errP = netErrParamErr;
		return -1;
	}

	if (locAddrP && locAddrLenP)
	{
		sockaddr	locName;
		socklen_t	locNamelen = sizeof (locName);

		result = getsockname (s, &locName, &locNamelen);

		// For unbound sockets, NetLib returns 0.0.0.0
		// !!! Assumes AF_INET family!

		if (result == -1 && ::PrvGetError () == WSAEINVAL)
		{
			result = errNone;

			sockaddr_in&	inLocName = (sockaddr_in&) locName;

			memset (&inLocName, 0, sizeof (inLocName));

			inLocName.sin_family = AF_INET;
			inLocName.sin_port = htons(0);
			inLocName.sin_addr.s_addr = htonl(INADDR_ANY);
		}

		if (result)
		{
			*errP = PrvGetTranslatedError ();
			return -1;
		}

		if (!SocketsToNetLibAddr (locName, locNamelen, *locAddrP, *locAddrLenP))
		{
			*errP = netErrParamErr;
			return -1;
		}
	}

	if (remAddrP && remAddrLenP)
	{
		sockaddr	remName;
		socklen_t	remNamelen = sizeof (remName);

		result = getpeername (s, &remName, &remNamelen);

		if (result)
		{
			*errP = PrvGetTranslatedError ();
			return -1;
		}

		if (!SocketsToNetLibAddr (remName, remNamelen, *remAddrP, *remAddrLenP))
		{
			*errP = netErrParamErr;
			return -1;
		}
	}

	*errP = 0;
	return result;
}


//--------------------------------------------------
// Sending and Receiving
//--------------------------------------------------
// Send data through a socket. The data is specified through the NetIOParamType
//  structure.
// Flags is one or more of netMsgFlagXXX.
// Returns # of bytes sent on success, or -1 on error. If error, *errP gets filled 
//  in with error code.

Int16 Platform_NetLib::SendPB(UInt16 libRefNum,
								NetSocketRef sRef,
								NetIOParamType* pbP,
								UInt16 sendFlags,
								Int32 timeout,
								Err* errP)
{
	UNUSED_PARAM(libRefNum)

	SOCKET	s;
	int		flags;
	int		result;

	if (!NetLibToSocketsRef (sRef, s) ||
		!NetLibToSocketsFlags (sendFlags, flags))
	{
		*errP = netErrParamErr;
		return -1;
	}

	// Wait till the socket is ready to send.

	if (timeout >= 0)
	{
		fd_set	hostWriteFDs;
		struct timeval hostTimeout;
		struct timeval *hTp = NULL;

		FD_ZERO(&hostWriteFDs);
		FD_SET(s, &hostWriteFDs);

		// Convert ticks to microseconds.

		const uint64	kMicrosecondsPerSecond	= 1000000;
		const uint64	kSysTicksPerSecond		= 100;	// !!! Should really call SysTicksPerSecond.

		uint64	usecs = timeout * kMicrosecondsPerSecond / kSysTicksPerSecond;

		hostTimeout.tv_sec = usecs / kMicrosecondsPerSecond;
		hostTimeout.tv_usec = usecs % kMicrosecondsPerSecond;
		hTp = &hostTimeout;

		result = select (s + 1, NULL, &hostWriteFDs, NULL, hTp);
		if (result == SOCKET_ERROR)
		{
			*errP = PrvGetTranslatedError ();
			return -1;
		}
		else if (!result)
		{
			*errP = netErrTimeout;
			return -1;
		}
	}

	// Collapse the scatter-write array into a single big buffer.

	UInt16	ii;
	long	bigBufferSize = 0;
	for (ii = 0; ii < pbP->iovLen; ++ii)
	{
		bigBufferSize += pbP->iov[ii].bufLen;
	}

	StMemory	bigBuffer (bigBufferSize);

	long	offset = 0;
	for (ii = 0; ii < pbP->iovLen; ++ii)
	{
		memcpy (bigBuffer.Get() + offset, pbP->iov[ii].bufP, pbP->iov[ii].bufLen);
		offset += pbP->iov[ii].bufLen;
	}

	// Determine if we need to set up a return address buffer.

	sockaddr	name;
	sockaddr*	nameP = NULL;
	socklen_t	namelen	= 0;

	if (pbP->addrP)
	{
		nameP	= &name;
		namelen	= sizeof (name);

		if (!NetLibToSocketsAddr (*(NetSocketAddrType*) pbP->addrP, pbP->addrLen, name, namelen))
		{
			*errP = netErrParamErr;
			return -1;
		}
	}

	// Make the send call.
	//
	// In previous versions of this function, we tried to make some determination
	// as to whether send() or sendto() needed to be called.  However,
	// according to the Windows sockets documentation, "On a connection-oriented
	// socket, the to and tolen parameters are ignored, making sento equivalent
	// to send".  And the GUSI sources back this up.  So we just use sendto in
	// all cases.

	result = sendto (s, bigBuffer.Get(), bigBufferSize, flags, nameP, namelen);

	// If there was an error, translate and return it.

	if (result == SOCKET_ERROR)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	*errP = 0;
	return result;
}


// Send data through a socket. The data to send is passed in a single buffer,
//  unlike NetLibSendPB. If toAddrP is not nil, the data will be sent to 
//  address *toAddrP.
// Flags is one or more of netMsgFlagXXX.
// Returns # of bytes sent on success, or -1 on error. If error, *errP gets filled 
//  in with error code.

Int16 Platform_NetLib::Send(UInt16 libRefNum,
							NetSocketRef sRef,
							const MemPtr bufP,
							UInt16 bufLen,
							UInt16 sendFlags,
							MemPtr toAddrP,
							UInt16 toLen,
							Int32 timeout,
							Err* errP)
{
	// Init the iov

	NetIOVecType	iov;
	iov.bufP = (UInt8*) bufP;
	iov.bufLen = bufLen;

	// Form a paramBlock for the Platform_NetLib::SendPB call

	NetIOParamType	pb;
	if (toLen)
	{
		pb.addrP = (UInt8*) toAddrP;
		pb.addrLen = toLen;
	}
	else
	{
		pb.addrP = 0;
		pb.addrLen = 0;
	}

	pb.iov = &iov;
	pb.iovLen = 1;

	// Do the call

	Int16	result = SendPB (libRefNum, sRef, &pb, sendFlags, timeout, errP);

	return result;
}


// Receive data from a socket. The data is gatthered into buffers specified in the 
//  NetIOParamType structure.
// Flags is one or more of netMsgFlagXXX.
// Timeout is max # of ticks to wait, or -1 for infinite, or 0 for none.
// Returns # of bytes received, or -1 on error. If error, *errP gets filled in 
//  with error code.

Int16 Platform_NetLib::ReceivePB(UInt16 libRefNum,
									NetSocketRef sRef,
									NetIOParamType* pbP,
									UInt16 rcvFlags,
									Int32 timeout,
									Err* errP)
{
	UNUSED_PARAM(libRefNum)

	SOCKET	s;
	int		flags;
	int		result;

	if (!NetLibToSocketsRef (sRef, s) ||
		!NetLibToSocketsFlags (rcvFlags, flags))
	{
		*errP = netErrParamErr;
		return -1;
	}

	// Wait till the socket is ready to receive.

	if (timeout >= 0)
	{
		fd_set	hostReadFDs;
		struct timeval hostTimeout;
		struct timeval *hTp = NULL;

		FD_ZERO(&hostReadFDs);
		FD_SET(s, &hostReadFDs);

		// Convert ticks to microseconds.

		const uint64	kMicrosecondsPerSecond	= 1000000;
		const uint64	kSysTicksPerSecond		= 100;	// !!! Should really call SysTicksPerSecond.

		uint64	usecs = timeout * kMicrosecondsPerSecond / kSysTicksPerSecond;

		hostTimeout.tv_sec = usecs / kMicrosecondsPerSecond;
		hostTimeout.tv_usec = usecs % kMicrosecondsPerSecond;
		hTp = &hostTimeout;

		result = select (s + 1, &hostReadFDs, NULL, NULL, hTp);
		if (result == SOCKET_ERROR)
		{
			*errP = PrvGetTranslatedError ();
			return -1;
		}
		else if (!result)
		{
			*errP = netErrTimeout;
			return -1;
		}
	}

	// Collapse the gather-read array into a single big buffer.

	// First, get the size for the big buffer.

	UInt16	ii;
	long	bigBufferSize = 0;
	for (ii = 0; ii < pbP->iovLen; ++ii)
	{
		bigBufferSize += pbP->iov[ii].bufLen;
	}

	// Second, allocate the buffer.

	StMemory	bigBuffer (bigBufferSize);

	// Third, copy the input buffer's contents to the big buffer.

	long	offset = 0;
	for (ii = 0; ii < pbP->iovLen; ++ii)
	{
		memcpy (bigBuffer.Get () + offset, pbP->iov[ii].bufP, pbP->iov[ii].bufLen);
		offset += pbP->iov[ii].bufLen;
	}

	// Determine if we need to set up a return address buffer.

	sockaddr	name;
	sockaddr*	nameP = NULL;
	socklen_t	namelen	= 0;

	if (pbP->addrP)
	{
		nameP	= &name;
		namelen	= sizeof (name);
	}

	// Make the receive call.
	//
	// In previous versions of this function, we tried to make some determination
	// as to whether recv() or recvfrom() needed to be called.  However,
	// according to my Linux man page, recv() is the same as recvfrom() but with
	// a NULL name parameter.  So we just use recvfrom in all cases.

	result = recvfrom (s, bigBuffer.Get (), bigBufferSize, flags, nameP, &namelen);

	// If there was an error, translate and return it.

	if (result == SOCKET_ERROR)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	// If we established a return address buffer, return the result
	// back to the caller.

	if (pbP->addrP)
	{
		if (!SocketsToNetLibAddr (name, namelen, *(NetSocketAddrType*) pbP->addrP, pbP->addrLen))
		{
			*errP = netErrParamErr;
			return -1;
		}
	}

	// Copy the chunks of the big buffer back into the iov array.

	offset = 0;
	for (ii = 0; ii < pbP->iovLen; ++ii)
	{
		memcpy (pbP->iov[ii].bufP, bigBuffer.Get () + offset, pbP->iov[ii].bufLen);
		offset += pbP->iov[ii].bufLen;
	}

	*errP = 0;
	return result;
}


// Receive data from a socket. The data is read into a single buffer, unlike
//  NetLibReceivePB. If fromAddrP is not nil, *fromLenP must be initialized to
//  the size of the buffer that fromAddrP points to and on exit *fromAddrP will
//  have the address of the sender in it.
// Flags is one or more of netMsgFlagXXX.
// Timeout is max # of ticks to wait, or -1 for infinite, or 0 for none.
// Returns # of bytes received, or -1 on error. If error, *errP gets filled in 
//  with error code.

Int16 Platform_NetLib::Receive(UInt16 libRefNum,
								NetSocketRef sRef,
								MemPtr bufP,
								UInt16 bufLen,
								UInt16 rcvFlags,
								MemPtr fromAddrP,
								UInt16* fromLenP,
								Int32 timeout,
								Err* errP)
{
	NetIOParamType		pb;
	NetIOVecType		iov;	

	// Assume no error
	*errP = 0;

	// If no buflen, return 0
	if (!bufLen)
		return 0;

	//-------------------------------------------------------------------------
	// Socket read
	// Form a paramBlock for the NetLibReceivePB call
	//-------------------------------------------------------------------------
	if (fromLenP)
	{
		pb.addrLen = *fromLenP;
		pb.addrP = (UInt8*) fromAddrP;
	}
	else
	{
		pb.addrLen = 0;
		pb.addrP = 0;
	}

	pb.iov = &iov;
	pb.iovLen = 1;

	// Init the iov
	iov.bufP = (UInt8*) bufP;
	iov.bufLen = bufLen;

	// Do the call
	Int16	result = ReceivePB (libRefNum, sRef, &pb, rcvFlags, timeout, errP);

	// Update the address length, if passed
	if (fromLenP)
		*fromLenP = pb.addrLen;

	return result;
}

// Receive data from a socket directly into a (write-protected) Data Manager 
//  record. 
// If fromAddrP is not nil, *fromLenP must be initialized to
//  the size of the buffer that fromAddrP points to and on exit *fromAddrP will
//  have the address of the sender in it.
// Flags is one or more of netMsgFlagXXX.
// Timeout is max # of ticks to wait, or -1 for infinite, or 0 for none.
// Returns # of bytes received, or -1 on error. If error, *errP gets filled in 
//  with error code.

Int16 Platform_NetLib::DmReceive(UInt16 libRefNum,
									NetSocketRef sRef,
									MemPtr recordP,
									UInt32 recordOffset,
									UInt16 rcvLen,
									UInt16 rcvFlags,
									MemPtr fromAddrP,
									UInt16* fromLenP,
									Int32 timeout,
									Err* errP)
{
	Int16	result = Receive (libRefNum, sRef, ((char*) recordP) + recordOffset,
								rcvLen, rcvFlags, fromAddrP, fromLenP, timeout, errP);

	return result;
}


//--------------------------------------------------
// Name Lookups
//--------------------------------------------------

NetHostInfoPtr Platform_NetLib::GetHostByName(UInt16 libRefNum,
												Char* nameP,
												NetHostInfoBufPtr bufP,
												Int32 timeout,
												Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	hostent*	pHostEnt = gethostbyname (nameP);

	if (!pHostEnt)
	{
#if PLATFORM_MAC
		*errP = PrvGetTranslatedHError ();
#else
		*errP = PrvGetTranslatedError ();
#endif
		return NULL;
	}

	if (!SocketsToNetLibHostEnt (*pHostEnt, *bufP))
	{
		*errP = netErrParamErr;
		return NULL;
	}

	*errP = 0;
	return &(bufP->hostInfo);
}


NetHostInfoPtr Platform_NetLib::GetHostByAddr(UInt16 libRefNum,
												UInt8* addrP,
												UInt16 len,
												UInt16 type,
												NetHostInfoBufPtr bufP,
												Int32 timeout,
												Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	hostent*	pHostEnt = gethostbyaddr ((const char*) addrP, len, type);

	if (!pHostEnt)
	{
#if PLATFORM_MAC
		*errP = PrvGetTranslatedHError ();
#else
		*errP = PrvGetTranslatedError ();
#endif
		return NULL;
	}

	if (!SocketsToNetLibHostEnt (*pHostEnt, *bufP))
	{
		*errP = netErrParamErr;
		return NULL;
	}

	*errP = 0;
	return &(bufP->hostInfo);
}


NetServInfoPtr Platform_NetLib::GetServByName(UInt16 libRefNum,
												Char* servNameP,
												Char* protoNameP,
												NetServInfoBufPtr bufP,
												Int32 timeout,
												Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(timeout)

	servent*	pServEnt = getservbyname (servNameP, protoNameP);

	if (!pServEnt)
	{
		*errP = PrvGetTranslatedError ();
		return NULL;
	}

	if (!SocketsToNetLibServEnt (*pServEnt, *bufP))
	{
		*errP = netErrParamErr;
		return NULL;
	}

	*errP = 0;
	return &(bufP->servInfo);
}


// Looks up a mail exchange name and returns a list of hostnames for it. Caller
//  must pass space for list of return names (hostNames), space for 
//  list of priorities for those hosts (priorities) and max # of names to 
//  return (maxEntries).
// Returns # of entries found, or -1 on error. If error, *errP gets filled in
//  with error code.

Int16 Platform_NetLib::GetMailExchangeByName(UInt16 libRefNum,
												Char* mailNameP,
												UInt16 maxEntries,
												Char hostNames[][netDNSMaxDomainName+1],
												UInt16 priorities[],
												Int32 timeout,
												Err* errP)
{
	UNUSED_PARAM(libRefNum)
	UNUSED_PARAM(mailNameP)
	UNUSED_PARAM(maxEntries)
	UNUSED_PARAM(hostNames)
	UNUSED_PARAM(priorities)
	UNUSED_PARAM(timeout)

	*errP = 0;
	// TODO
	return -1;
}


//--------------------------------------------------
// System level
//--------------------------------------------------

Int16 Platform_NetLib::Select(UInt16 libRefNum,
								UInt16 netWidth,
								NetFDSetType* netReadFDs,
								NetFDSetType* netWriteFDs,
								NetFDSetType* netExceptFDs,
								Int32 netTimeout,
								Err* errP)
{
	UNUSED_PARAM(libRefNum)

	fd_set	hostReadFDs;
	fd_set	hostWriteFDs;
	fd_set	hostExceptFDs;
	int		hostWidth = 0;

	fd_set*	hostReadFDsP = &hostReadFDs;
	fd_set*	hostWriteFDsP = &hostWriteFDs;
	fd_set*	hostExceptFDsP = &hostExceptFDs;

	if (!NetLibToSocketsFDSet (netReadFDs, netWidth, hostReadFDsP, hostWidth) ||
		!NetLibToSocketsFDSet (netWriteFDs, netWidth, hostWriteFDsP, hostWidth) ||
		!NetLibToSocketsFDSet (netExceptFDs, netWidth, hostExceptFDsP, hostWidth))
	{
		*errP = netErrParamErr;
		return -1;
	}

	timeval hostTimeout;
	timeval *hTp = NULL;
	if (netTimeout >= 0)
	{
		// Convert ticks (100ths of a second) to microseconds.

		hostTimeout.tv_sec = netTimeout / 100;
		hostTimeout.tv_usec = (netTimeout % 100) * 10000;
		hTp = &hostTimeout;
	}

	int	result = select (hostWidth, hostReadFDsP, hostWriteFDsP, hostExceptFDsP, hTp);

	netWidth = 0;
	if (!SocketsToNetLibFDSet (hostReadFDsP, hostWidth, netReadFDs, netWidth) ||
		!SocketsToNetLibFDSet (hostWriteFDsP, hostWidth, netWriteFDs, netWidth) ||
		!SocketsToNetLibFDSet (hostExceptFDsP, hostWidth, netExceptFDs, netWidth))
	{
		*errP = netErrParamErr;
		return -1;
	}

	if (result == SOCKET_ERROR)
	{
		*errP = PrvGetTranslatedError ();
		return -1;
	}

	*errP = 0;
	return result;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsRef
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool	NetLibToSocketsRef			(const NetSocketRef				inSocket,
									 SOCKET&						outSocket)
{
	if (inSocket < netMinSocketRefNum || inSocket > netMaxSocketRefNum)
		return false;

	outSocket = gSockets [inSocket - netMinSocketRefNum];

	if (outSocket == INVALID_SOCKET)
		return false;

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	RememberSocketsRef
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool	RememberSocketsRef			(const SOCKET					inSocket,
									 NetSocketRef&					outSocket)
{
	for (int ii = 0; ii < netMaxNumSockets; ++ii)
	{
		if (gSockets[ii] == INVALID_SOCKET)
		{
			gSockets[ii] = inSocket;
			outSocket = ii + netMinSocketRefNum;
			return true;
		}
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	ForgetSocketsRef
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool	ForgetSocketsRef			(NetSocketRef					sRef)
{
	if (sRef >= netMinSocketRefNum && sRef <= netMaxSocketRefNum)
	{
		gSockets [sRef - netMinSocketRefNum] = INVALID_SOCKET;
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsDomain
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool	NetLibToSocketsDomain	(const NetSocketAddrEnum	inDomain,
								 u_short&					outDomain)
{
	Bool result = true;

	switch (inDomain)
	{
		case netSocketAddrRaw:
			outDomain = AF_UNSPEC;
			break;

		case netSocketAddrINET:
			outDomain = AF_INET;
			break;

		default:
			result = false;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsType
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool NetLibToSocketsType (	const NetSocketTypeEnum	inType,
							int&					outType)
{
	Bool result = true;

	switch (inType)
	{
		case netSocketTypeStream:
			outType = SOCK_STREAM;
			break;

		case netSocketTypeDatagram:
			outType = SOCK_DGRAM;
			break;

		case netSocketTypeRaw:
			outType = SOCK_RAW;
			break;

		case netSocketTypeReliableMsg:
			outType = SOCK_RDM;
			break;

		default:
			result = false;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsProtocol
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool NetLibToSocketsProtocol (	const Int16	inProtocol,
								int&		outProtocol)
{
	Bool result = true;

	switch (inProtocol)
	{
		// This first case takes care of situations where applications
		// call NetLibOpenSocket, passing in zero for the protocol parameter.
		// Zero doesn't appear to be a valid value when looking at NetMgr.h
		// but zero *is* defined in WinSock.h, and applications *do* pass it in...
		//
		// Actually, the NetLib.c function comments say:
		//
		// *	protocol	- Ignored for datagram or stream sockets in the 
		// *				INET domain. If there were more than
		// *				one protocol that implements stream oriented services,
		// *				for example, this parameter would indicate which protocol 
		// *				to use.
		//
		// It goes on to describe how this parameter is really only used with
		// "raw" sockets in the INET domain.

		case 0:
			outProtocol = IPPROTO_IP;
			break;

		case netSocketProtoIPICMP:
			outProtocol = IPPROTO_ICMP;
			break;

		case netSocketProtoIPTCP:
			outProtocol = IPPROTO_TCP;
			break;

		case netSocketProtoIPUDP:
			outProtocol = IPPROTO_UDP;
			break;

		case netSocketProtoIPRAW:
			outProtocol = IPPROTO_RAW;
			break;

		default:
			result = false;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsDirection
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool NetLibToSocketsDirection (	const Int16	inDirection,
								int&		outDirection)
{
	Bool result = true;

	switch (inDirection)
	{
		case netSocketDirInput:
			outDirection = SD_RECEIVE;
			break;

		case netSocketDirOutput:
			outDirection = SD_SEND;
			break;

		case netSocketDirBoth:
			outDirection = SD_BOTH;
			break;

		default:
			result = false;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsFlags
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool NetLibToSocketsFlags (const UInt16 inFlags, int& outFlags)
{
	// Return false if there are any flags set other than the ones we know about.
	if ((inFlags & ~(netIOFlagOutOfBand | netIOFlagPeek | netIOFlagDontRoute)) != 0)
	{
		return false;
	}

	outFlags = 0;

	if ((inFlags & netIOFlagOutOfBand) != 0)
	{
		outFlags |= MSG_OOB;
	}

	if ((inFlags & netIOFlagPeek) != 0)
	{
		outFlags |= MSG_PEEK;
	}

	if ((inFlags & netIOFlagDontRoute) != 0)
	{
		outFlags |= MSG_DONTROUTE;
	}

	return true;
}

	
/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsOptions
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool NetLibToSocketsOptions (	const NetSocketOptLevelEnum	inOptLevel,
								const NetSocketOptEnum		inOptName,
								const MemPtr				inOptVal,
								const UInt16				inOptValLen,
								int&						outOptLevel,
								int&						outOptName,
								char*&						outOptVal,
								socklen_t&					outOptValLen)
{
	Bool	result = false;

	switch (inOptLevel)
	{
		case netSocketOptLevelIP:
			outOptLevel = IPPROTO_IP;
			switch (inOptName)
			{
#ifdef IP_OPTIONS
				case netSocketOptIPOptions:				// Not supported by WinSock 1.x
					outOptName = IP_OPTIONS;
					goto Common;
#endif
				default:	// Tell gcc to shut up about unhandled enumeration values.
					break;
			}
			break;

		case netSocketOptLevelTCP:
			outOptLevel = IPPROTO_TCP;
			switch (inOptName)
			{
#ifdef TCP_NODELAY
				case netSocketOptTCPNoDelay:
					outOptName = TCP_NODELAY;
					goto Common;
#endif

#ifdef TCP_MAXSEG
				case netSocketOptTCPMaxSeg:				// Not supported by WinSock
					outOptName = TCP_MAXSEG;
					goto Common;
#endif
				default:	// Tell gcc to shut up about unhandled enumeration values.
					break;
			}
			break;

		case netSocketOptLevelSocket:
			outOptLevel = SOL_SOCKET;
			switch (inOptName)
			{
#ifdef SO_DEBUG
				case netSocketOptSockDebug:
					outOptName = SO_DEBUG;
					goto Common;
#endif

#ifdef SO_ACCEPTCONN
				case netSocketOptSockAcceptConn:
					outOptName = SO_ACCEPTCONN;
					goto Common;
#endif

#ifdef SO_REUSEADDR
				case netSocketOptSockReuseAddr:
					outOptName = SO_REUSEADDR;
					goto Common;
#endif

#ifdef SO_KEEPALIVE
				case netSocketOptSockKeepAlive:
					outOptName = SO_KEEPALIVE;
					goto Common;
#endif

#ifdef SO_DONTROUTE
				case netSocketOptSockDontRoute:
					outOptName = SO_DONTROUTE;
					goto Common;
#endif

#ifdef SO_BROADCAST
				case netSocketOptSockBroadcast:
					outOptName = SO_BROADCAST;
					goto Common;
#endif

#ifdef SO_USELOOPBACK
				case netSocketOptSockUseLoopback:
					outOptName = SO_USELOOPBACK;
					goto Common;
#endif

#ifdef SO_LINGER
				case netSocketOptSockLinger:
					outOptName = SO_LINGER;
					if (inOptValLen == sizeof (NetSocketLingerType))
					{
						outOptValLen = sizeof (linger);
						outOptVal = (char*) Platform::AllocateMemory (outOptValLen);

						NetSocketLingerType	val = *(NetSocketLingerType*) inOptVal;

						Canonical (val.onOff);	// Byteswap on LE machines
						Canonical (val.time);	// Byteswap on LE machines

						((linger*) outOptVal)->l_onoff = val.onOff;
						((linger*) outOptVal)->l_linger = val.time;

						result = true;
					}
					break;
#endif

#ifdef SO_OOBINLINE
				case netSocketOptSockOOBInLine:
					outOptName = SO_OOBINLINE;
					goto Common;
#endif

#ifdef SO_SNDBUF
				case netSocketOptSockSndBufSize:
					outOptName = SO_SNDBUF;
					goto Common;
#endif

#ifdef SO_RCVBUF
				case netSocketOptSockRcvBufSize:
					outOptName = SO_RCVBUF;
					goto Common;
#endif

#ifdef SO_SNDLOWAT
				case netSocketOptSockSndLowWater:		// Not supported by WinSock
					outOptName = SO_SNDLOWAT;
					goto Common;
#endif

#ifdef SO_RCVLOWAT
				case netSocketOptSockRcvLowWater:		// Not supported by WinSock
					outOptName = SO_RCVLOWAT;
					goto Common;
#endif

#ifdef SO_SNDTIMEO
				case netSocketOptSockSndTimeout:		// Not supported by WinSock
					outOptName = SO_SNDTIMEO;
					goto Common;
#endif

#ifdef SO_RCVTIMEO
				case netSocketOptSockRcvTimeout:		// Not supported by WinSock
					outOptName = SO_RCVTIMEO;
					goto Common;
#endif

#ifdef SO_ERROR
				case netSocketOptSockErrorStatus:
					outOptName = SO_ERROR;
					goto Common;
#endif

#ifdef SO_TYPE
				case netSocketOptSockSocketType:
					outOptName = SO_TYPE;
					goto Common;
#endif

				default:	// Tell gcc to shut up about unhandled enumeration values.
					break;

Common:
					result = NetLibToSocketsScalarOption (inOptVal, inOptValLen, outOptVal, outOptValLen);
					break;

//				case netSocketOptSockNonBlocking:		// Palm OS Exclusive (special support added in SocketOptionSet)
//				case netSocketOptSockRequireErrClear:	// Palm OS Exclusive
//				case netSocketOptSockMultiPktAddr:		// Palm OS Exclusive
			}
			break;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SocketsToNetLibOptions
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool	SocketsToNetLibOptions		(const int				inOptLevel,
									 const int				inOptName,
									 /*const*/ char*&		inOptVal,
									 const int				inOptValLen,
									 NetSocketOptLevelEnum	outOptLevel,
									 NetSocketOptEnum		outOptName,
									 MemPtr					outOptVal,
									 UInt16					outOptValLen)
{
	UNUSED_PARAM(outOptLevel)
	UNUSED_PARAM(outOptName)

	Bool	result = false;

	switch (inOptLevel)
	{
		case IPPROTO_IP:
			switch (inOptName)
			{
#ifdef IP_OPTIONS
				case IP_OPTIONS:				// Not supported by WinSock 1.x
					goto Common;
#endif
			}
			break;

		case IPPROTO_TCP:
			switch (inOptName)
			{
#ifdef TCP_NODELAY
				case TCP_NODELAY:
					goto Common;
#endif

#ifdef TCP_MAXSEG
				case TCP_MAXSEG:				// Not supported by WinSock
					goto Common;
#endif
			}
			break;

		case SOL_SOCKET:
			switch (inOptName)
			{
#ifdef SO_ACCEPTCONN
				case SO_ACCEPTCONN:
					goto Common;
#endif

#ifdef SO_USELOOPBACK
				case SO_USELOOPBACK:
					goto Common;
#endif

#ifdef SO_DEBUG
				case SO_DEBUG:
					goto Common;
#endif

#ifdef SO_REUSEADDR
				case SO_REUSEADDR:
					goto Common;
#endif

#ifdef SO_KEEPALIVE
				case SO_KEEPALIVE:
					goto Common;
#endif

#ifdef SO_DONTROUTE
				case SO_DONTROUTE:
					goto Common;
#endif

#ifdef SO_BROADCAST
				case SO_BROADCAST:
					goto Common;
#endif

#ifdef SO_LINGER
				case SO_LINGER:
					if (inOptValLen == sizeof (linger))
					{
						linger	val = *(linger*) inOptVal;

						Canonical (val.l_onoff);	// Byteswap on LE machines
						Canonical (val.l_linger);	// Byteswap on LE machines

						((NetSocketLingerType*) outOptVal)->onOff = val.l_onoff;
						((NetSocketLingerType*) outOptVal)->time = val.l_linger;

						Platform::DisposeMemory (inOptVal);

						result = true;
					}
					break;
#endif

#ifdef SO_OOBINLINE
				case SO_OOBINLINE:
					goto Common;
#endif

#ifdef SO_SNDBUF
				case SO_SNDBUF:
					goto Common;
#endif

#ifdef SO_RCVBUF
				case SO_RCVBUF:
					goto Common;
#endif

#ifdef SO_SNDLOWAT
				case SO_SNDLOWAT:		// Not supported by WinSock
					goto Common;
#endif

#ifdef SO_RCVLOWAT
				case SO_RCVLOWAT:		// Not supported by WinSock
					goto Common;
#endif

#ifdef SO_SNDTIMEO
				case SO_SNDTIMEO:		// Not supported by WinSock
					goto Common;
#endif

#ifdef SO_RCVTIMEO
				case SO_RCVTIMEO:		// Not supported by WinSock
					goto Common;
#endif

#ifdef SO_ERROR
				case SO_ERROR:
					goto Common;
#endif

#ifdef SO_TYPE
				case SO_TYPE:
					goto Common;
#endif

//				case netSocketOptSockNonBlocking:		// Palm OS Exclusive (special support added in SocketOptionGet)
//				case netSocketOptSockRequireErrClear:	// Palm OS Exclusive
//				case netSocketOptSockMultiPktAddr:		// Palm OS Exclusive

Common:
					result = SocketsToNetLibScalarOption (inOptVal, inOptValLen, outOptVal, outOptValLen);
					break;
			}
			break;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsAddr
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool	NetLibToSocketsAddr			(const NetSocketAddrType&	inAddr,
									 const Int16				inAddrLen,
									 sockaddr&					outAddr,
									 int						outAddrLen)
{
	UNUSED_PARAM (inAddrLen)

	Bool result = true;

	switch (inAddr.family)
	{
		case netSocketAddrRaw:
			outAddr.sa_family	= AF_UNSPEC;
#if HAVE_SA_LEN
			outAddr.sa_len		= sizeof (sockaddr);
#endif
			memcpy (outAddr.sa_data, inAddr.data, outAddrLen - offsetof (sockaddr, sa_data));
			break;

		case netSocketAddrINET:
			((sockaddr_in&) outAddr).sin_family			= AF_INET;
#if HAVE_SA_LEN
			((sockaddr_in&) outAddr).sin_len			= sizeof (sockaddr_in);
#endif
			((sockaddr_in&) outAddr).sin_port			= ((NetSocketAddrINType&) inAddr).port;
			((sockaddr_in&) outAddr).sin_addr.s_addr	= ((NetSocketAddrINType&) inAddr).addr;
			break;

		default:
			result = false;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SocketsToNetLibAddr
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool	SocketsToNetLibAddr			(const sockaddr&	inAddr,
									 int				inAddrLen,
									 NetSocketAddrType&	outAddr,
									 Int16				outAddrLen)
{
	UNUSED_PARAM (inAddrLen)

	Bool result = true;

	switch (inAddr.sa_family)
	{
		case AF_UNSPEC:
			outAddr.family	= netSocketAddrRaw;
			memcpy (outAddr.data, inAddr.sa_data, outAddrLen - offsetof (NetSocketAddrType, data));
			break;

		case AF_INET:
			((NetSocketAddrINType&) outAddr).family	= netSocketAddrINET;
			((NetSocketAddrINType&) outAddr).port	= ((sockaddr_in&) inAddr).sin_port;
			((NetSocketAddrINType&) outAddr).addr	= ((sockaddr_in&) inAddr).sin_addr.s_addr;
			break;

		default:
			result = false;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SocketsToNetLibHostEnt
 *
 * DESCRIPTION:	Convert a sockets hostent to a NetLib NetHostInfoBufType
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool SocketsToNetLibHostEnt (	const hostent&			inHostEnt,
								NetHostInfoBufType&		outHostEnt)
{
	Bool	result = true;

	// hostent.h_name ->	NetHostInfoBufType.name
	//						NetHostInfoBufType.hostInfo.nameP
	strcpy (outHostEnt.name, inHostEnt.h_name);
	outHostEnt.hostInfo.nameP = outHostEnt.name;

	// hostent.h_aliases ->	NetHostInfoBufType.aliasList
	//						NetHostInfoBufType.aliases
	//						NetHostInfoBufType.hostInfo.nameAliasesP
	char*	curSrcAlias;
	char*	curDestAlias = outHostEnt.aliases[0];
	long	index = 0;
	while ((index < netDNSMaxAliases) &&
		((curSrcAlias = inHostEnt.h_aliases[index]) != NULL))
	{
		outHostEnt.aliasList[index] = curDestAlias;
		strcpy (curDestAlias, curSrcAlias);
		curDestAlias += strlen (curDestAlias) + 1;
		if ((((long) curDestAlias) & 1) != 0)
			++curDestAlias;
		++index;
	}

	outHostEnt.aliasList[index] = NULL;
	outHostEnt.hostInfo.nameAliasesP = outHostEnt.aliasList;

	// hostent.h_addrtype ->	NetHostInfoBufType.hostInfo.addrType
	outHostEnt.hostInfo.addrType = inHostEnt.h_addrtype;

	// hostent.h_length ->		NetHostInfoBufType.hostInfo.addrLen
	outHostEnt.hostInfo.addrLen = inHostEnt.h_length;

	// hostent.h_addr_list ->	NetHostInfoBufType.addressList
	//							NetHostInfoBufType.address
	//							NetHostInfoBufType.hostInfo.addrListP
	char*		curSrcAddr;
	NetIPAddr*	curDestAddr = outHostEnt.address;
	/*long*/	index = 0;
	while ((index < netDNSMaxAddresses) &&
		((curSrcAddr = inHostEnt.h_addr_list[index]) != NULL))
	{
		outHostEnt.addressList[index] = curDestAddr;
		memcpy (curDestAddr, curSrcAddr, inHostEnt.h_length);
		curDestAddr = (NetIPAddr*) (((char*) curDestAddr) + inHostEnt.h_length);
		++index;
	}

	// Comment from PrvDNSNameToAddressHandler in NetStack1.c:
	//
	// Long, long ago in a galaxy not so far away, Ron defined a struct
	// called NetHostInfoBufType.  Unfortunately, this struct has a bug
	// in it, but its too late to fix the problem, because 10 zillion
	// developers are already using this struct.  The problem is that
	// there can be up to netDNSMaxAddresses + 1 addresses in the array
	// of addresses referenced via the field addrListP, but he only
	// reserved space for netDNSMaxAddresses addresses.  The additional
	// address is needed to zero-terminate the list in the case when
	// the maximum number of DNS addresses are returned.  Oh well, too
	// late now.  The problem observed by many is that the null termination
	// was overwritten the following array, which happened to be the first
	// returned DNS address.  Many folks assume a minimum of at least one
	// address and just use the first, but if the first returned DNS address
	// is zero, then bad things happen (or nothing happen!).  So, the
	// so-so solution is to return only netDNSMaxAddresses - 1 entries
	// (using the last entry for the null termination), even when the
	// maximum really was returned.  Although, by now, most poor
	// developers have written their own DNS lookup routines and won't
	// get to use this lovely bug fix anyway.

	if (index == netDNSMaxAddresses)
		--index;

	outHostEnt.addressList[index] = 0;
	outHostEnt.hostInfo.addrListP = (UInt8**) &outHostEnt.addressList[0];

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SocketsToNetLibServEnt
 *
 * DESCRIPTION:	Convert a sockets servent to a NetLib NetServInfoBufType
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool SocketsToNetLibServEnt (	const servent&			inServEnt,
								NetServInfoBufType&		outServEnt)
{
	Bool	result = true;

	// servent.s_name ->	NetServInfoBufType.name
	//						NetServInfoBufType.servInfo.nameP
	strcpy (outServEnt.name, inServEnt.s_name);
	outServEnt.servInfo.nameP = outServEnt.name;

	// servent.s_aliases ->	NetServInfoBufType.aliasList
	//						NetServInfoBufType.aliases
	//						NetServInfoBufType.servInfo.nameAliasesP
	char*	curSrcAlias;
	char*	curDestAlias = outServEnt.aliases[0];
	long	index = 0;
	while ((curSrcAlias = inServEnt.s_aliases[index]) != NULL)
	{
		outServEnt.aliasList[index] = curDestAlias;
		strcpy (curDestAlias, curSrcAlias);
		curDestAlias += strlen (curDestAlias) + 1;
		if ((((long) curDestAlias) & 1) != 0)
			++curDestAlias;
		++index;

		if (index == netServMaxAliases)
			break;
	}

	outServEnt.aliasList[index] = NULL;
	outServEnt.servInfo.nameAliasesP = outServEnt.aliasList;

	// servent.s_port ->	NetServInfoButType.servInfo.port
	outServEnt.servInfo.port = inServEnt.s_port;

	// servent.s_proto ->	NetServInfoBufType.protoName
	//						NetServInfoBufType.servInfo.protoP
	strcpy (outServEnt.protoName, inServEnt.s_proto);
	outServEnt.servInfo.protoP = outServEnt.protoName;

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsFDSet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool NetLibToSocketsFDSet (	const NetFDSetType*	inFDSet,
							UInt16				inWidth,
							fd_set*&			outFSSet,
							int&				outWidth)
{
	Bool	result = true;
	fd_set*	finalSetPtr = NULL;

	if (inFDSet)
	{
		FD_ZERO (outFSSet);

		if (inWidth > netMaxNumSockets)
		{
			inWidth = netMaxNumSockets;
		}

		for (int ii = 0; ii < inWidth; ++ii)
		{
			NetSocketRef sRef = ii + netMinSocketRefNum;

			if (netFDIsSet (sRef, inFDSet))
			{
				SOCKET	s = gSockets [ii];
				if (s != INVALID_SOCKET)
				{
					FD_SET (s, outFSSet);
					finalSetPtr = outFSSet;

					outWidth = FD_SETSIZE;
				}
			}
		}
	}

	outFSSet = finalSetPtr;

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SocketsToNetLibFDSet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool SocketsToNetLibFDSet (	const fd_set*	inFDSet,
							int				inWidth,
							NetFDSetType*	outFSSet,
							UInt16&			outWidth)
{
	Bool	result = true;

	if (inFDSet && outFSSet)
	{
		netFDZero (outFSSet);

		if (inWidth > FD_SETSIZE)
		{
			inWidth = FD_SETSIZE;
		}

		for (int ii = 0; ii < netMaxNumSockets; ++ii)
		{
			SOCKET	s = gSockets[ii];

			if (s != INVALID_SOCKET && FD_ISSET (s, inFDSet))
			{
				netFDSet ((netMinSocketRefNum + ii), outFSSet);

				if (outWidth < ii)
				{
					outWidth = ii;
				}
			}
		}
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	NetLibToSocketsScalarOption
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool NetLibToSocketsScalarOption (	const MemPtr	inOptVal,
									const UInt16	inOptValLen,
									char*&			outOptVal,
									socklen_t&		outOptValLen)
{
	Bool	result = true;

	if (inOptVal)
	{
		uint32	val = 0;
		if (inOptValLen == sizeof (uint8))
		{
			uint8	temp = *(uint8*) inOptVal;
			Canonical (temp);					// Byteswap on LE machines
			val = temp;
		}
		else if (inOptValLen == sizeof (uint16))
		{
			uint16	temp = *(uint16*) inOptVal;
			Canonical (temp);					// Byteswap on LE machines
			val = temp;
		}
		else if (inOptValLen == sizeof (uint32))
		{
			uint32	temp = *(uint32*) inOptVal;
			Canonical (temp);					// Byteswap on LE machines
			val = temp;
		}
		else
		{
			result = false;
		}

		if (result)
		{
			outOptValLen = sizeof (uint32);
			outOptVal = (char*) Platform::AllocateMemory (outOptValLen);
			*(uint32*) outOptVal = val;
		}
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SocketsToNetLibScalarOption
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

Bool SocketsToNetLibScalarOption	(/*const*/ char*&	inOptVal,
									 const int			inOptValLen,
									 MemPtr				outOptVal,
									 UInt16				outOptValLen)
{
	Bool	result = true;

	if (inOptVal && outOptVal)
	{
		if (inOptValLen == outOptValLen)
		{
			if (inOptValLen == sizeof (uint8))
			{
				uint8	val = *(uint8*) inOptVal;
				Canonical (val);					// Byteswap on LE machines
				*(uint8*) outOptVal = val;
			}
			else if (inOptValLen == sizeof (uint16))
			{
				uint16	val = *(uint16*) inOptVal;
				Canonical (val);					// Byteswap on LE machines
				*(uint16*) outOptVal = val;
			}
			else if (inOptValLen == sizeof (uint32))
			{
				uint32	val = *(uint32*) inOptVal;
				Canonical (val);					// Byteswap on LE machines
				*(uint32*) outOptVal = val;
			}
			else
			{
				result = false;
			}
		}
		else
		{
			result = false;
		}
	}

	Platform::DisposeMemory (inOptVal);

	return result;
}

										 
/***********************************************************************
 *
 * FUNCTION:	PrvGetError
 *
 * DESCRIPTION:	Returns the most recent sockets error number.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	Sockets error number
 *
 ***********************************************************************/

uint32 PrvGetError (void)
{
#if PLATFORM_MAC || PLATFORM_UNIX
	uint32	err = errno;
#endif

#if PLATFORM_WINDOWS
	uint32	err = ::WSAGetLastError ();
#endif

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetTranslatedError
 *
 * DESCRIPTION:	Returns the most recent sockets error number translated
 *				into a NetLib error number.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	NetLib error number
 *
 ***********************************************************************/

Err PrvGetTranslatedError (void)
{
	uint32	err = ::PrvGetError ();

	return PrvTranslateError (err);
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetTranslatedHError
 *
 * DESCRIPTION:	Returns the most recent sockets error number translated
 *				into a NetLib error number.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	NetLib error number
 *
 ***********************************************************************/

#if PLATFORM_MAC
extern int h_errno;
const int kHErrNoBase = 30000;

Err PrvGetTranslatedHError (void)
{
	// For some reason (I don't know...maybe BSD sockets is like this, too),
	// GUSI returns errors from gethostbyname() and gethostbyaddr() in h_errno.

	uint32	err = h_errno + kHErrNoBase;

	return PrvTranslateError (err);
}
#endif

	
/***********************************************************************
 *
 * FUNCTION:	PrvTranslateError
 *
 * DESCRIPTION:	Convert a Windows Sockets error number to a NetLib error
 *				number.
 *
 * PARAMETERS:	err - Windows Sockets error number (probably from
 *				WSAGetLastError)
 *
 * RETURNED:	NetLib error number
 *
 ***********************************************************************/

Err PrvTranslateError (uint32 err)
{
	Err	result = netErrParamErr;	// Default error number

	switch (err)
	{
#if PLATFORM_MAC || PLATFORM_UNIX
//		case EPERM:				//	1		/* Operation not permitted */
//		case ENOENT:			//	2		/* No such file or directory */
//		case ESRCH:				//	3		/* No such process */
		case EINTR:				//	4		/* Interrupted system call */
			result = netErrUserCancel;
			break;

//		case EIO:				//	5		/* Input/output error */
//		case ENXIO:				//	6		/* Device not configured */
//		case E2BIG:				//	7		/* Argument list too long */
//		case ENOEXEC:			//	8		/* Exec format error */
//		case EBADF:				//	9		/* Bad file descriptor */
//		case ECHILD:			//	10		/* No child processes */
		case EDEADLK:			//	11		/* Resource deadlock avoided */
					/* 11 was EAGAIN */
			result = netErrWouldBlock;
			break;

		case ENOMEM:			//	12		/* Cannot allocate memory */
			result = netErrOutOfMemory;
			break;

		case EACCES:			//	13		/* Permission denied */
			result = netErrAuthFailure;
			break;

//		case EFAULT:			//	14		/* Bad address */
//		case ENOTBLK:			//	15		/* Block device required */
		case EBUSY:				//	16		/* Device busy */
			result = netErrSocketBusy;
			break;

//		case EEXIST:			//	17		/* File exists */
//		case EXDEV:				//	18		/* Cross-device link */
//		case ENODEV:			//	19		/* Operation not supported by device */
//		case ENOTDIR:			//	20		/* Not a directory */
//		case EISDIR:			//	21		/* Is a directory */
//		case EINVAL:			//	22		/* Invalid argument */
//		case ENFILE:			//	23		/* Too many open files in system */
//		case EMFILE:			//	24		/* Too many open files */
//		case ENOTTY:			//	25		/* Inappropriate ioctl for device */
//		case ETXTBSY:			//	26		/* Text file busy */
//		case EFBIG:				//	27		/* File too large */
//		case ENOSPC:			//	28		/* No space left on device */
//		case ESPIPE:			//	29		/* Illegal seek */
		case EROFS:				//	30		/* Read-only file system */
			result = netErrReadOnlySetting;
			break;

//		case EMLINK:			//	31		/* Too many links */
//		case EPIPE:				//	32		/* Broken pipe */
/* math software */
//		case EDOM:				//	33		/* Numerical argument out of domain */
//		case ERANGE:			//	34		/* Result too large */
		case EAGAIN:			//	35		/* Resource temporarily unavailable */
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:		//	EAGAIN	/* Operation would block */
#endif
		case EINPROGRESS:		//	36		/* Operation now in progress */
		//	result = netErrSocketBusy;
			result = netErrWouldBlock;
			break;

/* These are defined to be the same under QNX/Neutrino */
#if EALREADY != EBUSY
		case EALREADY:			//	37		/* Operation already in progress */
			result = netErrAlreadyInProgress;
			break;
#endif
/* ipc/network software -- argument errors */
		case ENOTSOCK:			//	38		/* Socket operation on non-socket */
			result = netErrNoSocket;
			break;

		case EDESTADDRREQ:		//	39		/* Destination address required */
			result = netErrIPNoDst;
			break;

		case EMSGSIZE:			//	40		/* Message too long */
			result = netErrMessageTooBig;
			break;

//		case EPROTOTYPE:		//	41		/* Protocol wrong type for socket */
		case ENOPROTOOPT:		//	42		/* Protocol not available */
			result = netErrUnknownProtocol;
			break;

		case EPROTONOSUPPORT:	//	43		/* Protocol not supported */
			result = netErrUnknownProtocol;
			break;

		case ESOCKTNOSUPPORT:	//	44		/* Socket type not supported */
			result = netErrWrongSocketType;
			break;

		case EOPNOTSUPP:		//	45		/* Operation not supported on socket */
			result = netErrWrongSocketType;
			break;

		case EPFNOSUPPORT:		//	46		/* Protocol family not supported */
			result = netErrUnknownService;
			break;

		case EAFNOSUPPORT:		//	47		/* Address family not supported by protocol family */
			result = netErrUnknownService;
			break;

		case EADDRINUSE:		//	48		/* Address already in use */
			result = netErrPortInUse;
			break;

		case EADDRNOTAVAIL:		//	49		/* Can't assign requested address */
			result = netErrPortInUse;
			break;

/* ipc/network software -- operational errors */
		case ENETDOWN:			//	50		/* Network is down */
			result = netErrUnreachableDest;
			break;

		case ENETUNREACH:		//	51		/* Network is unreachable */
			result = netErrNoInterfaces;
			break;

		case ENETRESET:			//	52		/* Network dropped connection on reset */
		case ECONNABORTED:		//	53		/* Software caused connection abort */
		case ECONNRESET:		//	54		/* Connection reset by peer */
			result = netErrSocketClosedByRemote;
			break;

		case ENOBUFS:			//	55		/* No buffer space available */
			result = netErrNoTCB;
			break;

		case EISCONN:			//	56		/* Socket is already connected */
			result = netErrSocketAlreadyConnected;
			break;

		case ENOTCONN:			//	57		/* Socket is not connected */
			result = netErrSocketNotConnected;
			break;

		case ESHUTDOWN:			//	58		/* Can't send after socket shutdown */
			result = netErrSocketNotOpen;
			break;

//		case ETOOMANYREFS:		//	59		/* Too many references: can't splice */
		case ETIMEDOUT:			//	60		/* Connection timed out */
			result = netErrTimeout;
			break;

		case ECONNREFUSED:		//	61		/* Connection refused */
			result = netErrTimeout;
			break;

//		case ELOOP:				//	62		/* Too many levels of symbolic links */
//		case ENAMETOOLONG:		//	63		/* File name too long */
		case EHOSTDOWN:			//	64		/* Host is down */
		case EHOSTUNREACH:		//	65		/* No route to host */
			result = netErrIPNoRoute;
			break;

//		case ENOTEMPTY:			//	66		/* Directory not empty */
//		case EPROCLIM:			//	67		/* Too many processes */
//		case EUSERS:			//	68		/* Too many users */
//		case EDQUOT:			//	69		/* Disc quota exceeded */

/* Network File System */
//		case ESTALE:			//	70		/* Stale NFS file handle */
//		case EREMOTE:			//	71		/* Too many levels of remote in path */
//		case EBADRPC:			//	72		/* RPC struct is bad */
//		case ERPCMISMATCH:		//	73		/* RPC version wrong */
//		case EPROGUNAVAIL:		//	74		/* RPC prog. not avail */
//		case EPROGMISMATCH:		//	75		/* Program version wrong */
//		case EPROCUNAVAIL:		//	76		/* Bad procedure for program */
//		case ENOLCK:			//	77		/* No locks available */
//		case ENOSYS:			//	78		/* Function not implemented */
//		case EFTYPE:			//	79		/* Inappropriate file type or format */
#endif	// PLATFORM_MAC || PLATFORM_UNIX

#if PLATFORM_MAC
		case HOST_NOT_FOUND + kHErrNoBase:	//	1		/* Authoritative Answer Host not found */
			result = netErrDNSUnreachable;
			break;

		case TRY_AGAIN + kHErrNoBase:		//	2		/* Non-Authoritive Host not found, or SERVERFAIL */
			result = netErrDNSServerFailure;
			break;

		case NO_RECOVERY + kHErrNoBase:		//	3		/* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
			result = netErrDNSRefused;
			break;

		case NO_DATA + kHErrNoBase:			//	4		/* Valid name, no data record of requested type */
//		case NO_ADDRESS + kHErrNoBase:		//	4		/* no address, look for MX record */
			result = netErrDNSNonexistantName;
			break;
#endif

#if PLATFORM_WINDOWS
		case WSAEACCES:
			// (10013)
			// Permission denied.
			// An attempt was made to access a socket in a way forbidden by its access permissions. An example is using a broadcast address for sendto without broadcast permission being set using setsockopt(SO_BROADCAST).
			result = netErrWrongSocketType;	// BEST I CAN THINK OF.
			break;

		case WSAEADDRINUSE:
			// (10048)
			// Address already in use.
			// Only one usage of each socket address (protocol/IP address/port) is normally permitted. This error occurs if an application attempts to bind a socket to an IP address/port that has already been used for an existing socket, or a socket that wasn't closed properly, or one that is still in the process of closing. For server applications that need to bind multiple sockets to the same port number, consider using setsockopt(SO_REUSEADDR). Client applications usually need not call bind at all - connect will choose an unused port automatically. When bind is called with a wild-card address (involving ADDR_ANY), a WSAEADDRINUSE error could be delayed until the specific address is "committed." This could happen with a call to other function later, including connect, listen, WSAConnect or WSAJoinLeaf.
			result = netErrPortInUse;
			break;

		case WSAEADDRNOTAVAIL:
			// (10049)
			// Cannot assign requested address.
			// The requested address is not valid in its context. Normally results from an attempt to bind to an address that is not valid for the local machine. This can also result from connect, sendto, WSAConnect, WSAJoinLeaf, or WSASendTo when the remote address or port is not valid for a remote machine (e.g. address or port 0).
			result = netErrParamErr;
			break;

		case WSAEAFNOSUPPORT:
			// (10047)
			// Address family not supported by protocol family.
			// An address incompatible with the requested protocol was used. All sockets are created with an associated "address family" (i.e. AF_INET for Internet Protocols) and a generic protocol type (i.e. SOCK_STREAM). This error will be returned if an incorrect protocol is explicitly requested in the socket call, or if an address of the wrong family is used for a socket, e.g. in sendto.
			result = netErrParamErr;
			break;

		case WSAEALREADY:
			// (10037)
			// Operation already in progress.
			// An operation was attempted on a non-blocking socket that already had an operation in progress - i.e. calling connect a second time on a non-blocking socket that is already connecting, or canceling an asynchronous request (WSAAsyncGetXbyY) that has already been canceled or completed.
			result = netErrAlreadyInProgress;
			break;

		case WSAECONNABORTED:
			// (10053)
			// Software caused connection abort.
			// An established connection was aborted by the software in your host machine, possibly due to a data transmission timeout or protocol error.
			result = netErrSocketClosedByRemote;
			break;

		case WSAECONNREFUSED:
			// (10061)
			// Connection refused.
			// No connection could be made because the target machine actively refused it. This usually results from trying to connect to a service that is inactive on the foreign host - i.e. one with no server application running.
			result = netErrTimeout;			// As near as I can tell, if a connection is refused, NetLib merely closes the connection and eventually times out.
			break;

		case WSAECONNRESET:
			// (10054)
			// Connection reset by peer.
			// A existing connection was forcibly closed by the remote host. This normally results if the peer application on the remote host is suddenly stopped, the host is rebooted, or the remote host used a "hard close" (see setsockopt for more information on the SO_LINGER option on the remote socket.) This error may also result if a connection was broken due to "keep-alive" activity detecting a failure while one or more operations are in progress. Operations that were in progress fail with WSAENETRESET. Subsequent operations fail with WSAECONNRESET.
			result = netErrSocketClosedByRemote;
			break;

		case WSAEDESTADDRREQ:
			// (10039)
			// Destination address required.
			// A required address was omitted from an operation on a socket. For example, this error will be returned if sendto is called with the remote address of ADDR_ANY.
			result = netErrParamErr;
			break;

		case WSAEFAULT:
			// (10014)
			// Bad address.
			// The system detected an invalid pointer address in attempting to use a pointer argument of a call. This error occurs if an application passes an invalid pointer value, or if the length of the buffer is too small. For instance, if the length of an argument which is a sockaddr is smaller than sizeof(sockaddr).
			result = netErrParamErr;
			break;

		case WSAEHOSTDOWN:
			// (10064)
			// Host is down.
			// A socket operation failed because the destination host was down. A socket operation encountered a dead host. Networking activity on the local host has not been initiated. These conditions are more likely to be indicated by the error WSAETIMEDOUT.
			result = netErrTimeout;
			break;

		case WSAEHOSTUNREACH:
			// (10065)
			// No route to host.
			// A socket operation was attempted to an unreachable host. See WSAENETUNREACH
			result = netErrUnreachableDest;
			break;

		case WSAEINPROGRESS:
			// (10036)
			// Operation now in progress.
			// A blocking operation is currently executing. Windows Sockets only allows a single blocking operation to be outstanding per task (or thread), and if any other function call is made (whether or not it references that or any other socket) the function fails with the WSAEINPROGRESS error.
			result = netErrAlreadyInProgress;
			break;

		case WSAEINTR:
			// (10004)
			// Interrupted function call.
			// A blocking operation was interrupted by a call to WSACancelBlockingCall.
			result = netErrUserCancel;
			break;

		case WSAEINVAL:
			// (10022)
			// Invalid argument.
			// Some invalid argument was supplied (for example, specifying an invalid level to the setsockopt function). In some instances, it also refers to the current state of the socket - for instance, calling accept on a socket that is not listening.
			result = netErrParamErr;
			break;

		case WSAEISCONN:
			// (10056)
			// Socket is already connected.
			// A connect request was made on an already connected socket. Some implementations also return this error if sendto is called on a connected SOCK_DGRAM socket (For SOCK_STREAM sockets, the to parameter in sendto is ignored), although other implementations treat this as a legal occurrence.
			result = netErrSocketAlreadyConnected;
			break;

		case WSAEMFILE:
			// (10024)
			// Too many open files.
			// Too many open sockets. Each implementation may have a maximum number of socket handles available, either globally, per process or per thread.
			result = netErrNoMoreSockets;
			break;

		case WSAEMSGSIZE:
			// (10040)
			// Message too long.
			// A message sent on a datagram socket was larger than the internal message buffer or some other network limit, or the buffer used to receive a datagram into was smaller than the datagram itself.
			result = netErrMessageTooBig;
			break;

		case WSAENETDOWN:
			// (10050)
			// Network is down.
			// A socket operation encountered a dead network. This could indicate a serious failure of the network system (i.e. the protocol stack that the WinSock DLL runs over), the network interface, or the local network itself.
			result = netErrUnreachableDest;
			break;

		case WSAENETRESET:
			// (10052)
			// Network dropped connection on reset.
			// The connection has been broken due to "keep-alive" activity detecting a failure while the operation was in progress. It can also be returned by setsockopt if an attempt is made to set SO_KEEPALIVE on a connection that has already failed.
			result = netErrSocketClosedByRemote;
			break;

		case WSAENETUNREACH:
			// (10051)
			// Network is unreachable.
			// A socket operation was attempted to an unreachable network. This usually means the local software knows no route to reach the remote host.
			result = netErrUnreachableDest;
			break;

		case WSAENOBUFS:
			// (10055)
			// No buffer space available.
			// An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full.
			result = netErrOutOfMemory;
			break;

		case WSAENOPROTOOPT:
			// (10042)
			// Bad protocol option.
			// An unknown, invalid or unsupported option or level was specified in a getsockopt or setsockopt call.
			result = netErrParamErr;
			break;

		case WSAENOTCONN:
			// (10057)
			// Socket is not connected.
			// A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using sendto) no address was supplied. Any other type of operation might also return this error - for example, setsockopt setting SO_KEEPALIVE if the connection has been reset.
			result = netErrSocketNotConnected;
			break;

		case WSAENOTSOCK:
			// (10038)
			// Socket operation on non-socket.
			// An operation was attempted on something that is not a socket. Either the socket handle parameter did not reference a valid socket, or for select, a member of an fd_set was not valid.
			result = netErrParamErr;
			break;

		case WSAEOPNOTSUPP:
			// (10045)
			// Operation not supported.
			// The attempted operation is not supported for the type of object referenced. Usually this occurs when a socket descriptor to a socket that cannot support this operation, for example, trying to accept a connection on a datagram socket.
			result = netErrWrongSocketType;
			break;

		case WSAEPFNOSUPPORT:
			// (10046)
			// Protocol family not supported.
			// The protocol family has not been configured into the system or no implementation for it exists. Has a slightly different meaning to WSAEAFNOSUPPORT, but is interchangeable in most cases, and all Windows Sockets functions that return one of these specify WSAEAFNOSUPPORT.
			result = netErrParamErr;
			break;

		case WSAEPROCLIM:
			// (10067)
			// Too many processes.
			// A Windows Sockets implementation may have a limit on the number of applications that may use it simultaneously. WSAStartup may fail with this error if the limit has been reached.
			result = netErrOutOfResources;
			break;

		case WSAEPROTONOSUPPORT:
			// (10043)
			// Protocol not supported.
			// The requested protocol has not been configured into the system, or no implementation for it exists. For example, a socket call requests a SOCK_DGRAM socket, but specifies a stream protocol.
			result = netErrUnknownProtocol;
			break;

		case WSAEPROTOTYPE:
			// (10041)
			// Protocol wrong type for socket.
			// A protocol was specified in the socket function call that does not support the semantics of the socket type requested. For example, the ARPA Internet UDP protocol cannot be specified with a socket type of SOCK_STREAM.
			result = netErrUnknownProtocol;
			break;

		case WSAESHUTDOWN:
			// (10058)
			// Cannot send after socket shutdown.
			// A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call. By calling shutdown a partial close of a socket is requested, which is a signal that sending or receiving or both has been discontinued.
			result = netErrNotOpen;
			break;

		case WSAESOCKTNOSUPPORT:
			// (10044)
			// Socket type not supported.
			// The support for the specified socket type does not exist in this address family. For example, the optional type SOCK_RAW might be selected in a socket call, and the implementation does not support SOCK_RAW sockets at all.
			result = netErrParamErr;
			break;

		case WSAETIMEDOUT:
			// (10060)
			// Connection timed out.
			// A connection attempt failed because the connected party did not properly respond after a period of time, or established connection failed because connected host has failed to respond.
			result = netErrTimeout;
			break;

//		case WSATYPE_NOT_FOUND:
			// (10109)
			// Class type not found.
			// The specified class was not found.

		case WSAEWOULDBLOCK:
			// (10035)
			// Resource temporarily unavailable.
			// This error is returned from operations on non-blocking sockets that cannot be completed immediately, for example recv when no data is queued to be read from the socket. It is a non-fatal error, and the operation should be retried later. It is normal for WSAEWOULDBLOCK to be reported as the result from calling connect on a non-blocking SOCK_STREAM socket, since some time must elapse for the connection to be established.
			result = netErrWouldBlock;
			break;

		case WSAHOST_NOT_FOUND:
			// (11001)
			// Host not found.
			// No such host is known. The name is not an official hostname or alias, or it cannot be found in the database(s) being queried. This error may also be returned for protocol and service queries, and means the specified name could not be found in the relevant database.
			result = netErrDNSNonexistantName;
			break;

//		case WSA_INVALID_HANDLE:
			// (OS dependent)
			// Specified event object handle is invalid.
			// An application attempts to use an event object, but the specified handle is not valid.

//		case WSA_INVALID_PARAMETER:
			// (OS dependent)
			// One or more parameters are invalid.
			// An application used a Windows Sockets function which directly maps to a Win32 function. The Win32 function is indicating a problem with one or more parameters.

//		case WSAINVALIDPROCTABLE:
			// (OS dependent)
			// Invalid procedure table from service provider.
			// A service provider returned a bogus proc table to WS2_32.DLL. (Usually caused by one or more of the function pointers being NULL.)

//		case WSAINVALIDPROVIDER:
			// (OS dependent)
			// Invalid service provider version number.
			// A service provider returned a version number other than 2.0.

//		case WSA_IO_INCOMPLETE:
			// (OS dependent)
			// Overlapped I/O event object not in signaled state.
			// The application has tried to determine the status of an overlapped operation which is not yet completed. Applications that use WSAGetOverlappedResult (with the fWait flag set to false) in a polling mode to determine when an overlapped operation has completed will get this error code until the operation is complete.

//		case WSA_IO_PENDING:
			// (OS dependent)
			// Overlapped operations will complete later.
			// The application has initiated an overlapped operation which cannot be completed immediately. A completion indication will be given at a later time when the operation has been completed.

//		case WSA_NOT_ENOUGH_MEMORY:
			// (OS dependent)
			// Insufficient memory available.
			// An application used a Windows Sockets function which directly maps to a Win32 function. The Win32 function is indicating a lack of required memory resources.

		case WSANOTINITIALISED:
			// (10093)
			// Successful WSAStartup not yet performed.
			// Either the application hasn't called WSAStartup or WSAStartup failed. The application may be accessing a socket which the current active task does not own (i.e. trying to share a socket between tasks), or WSACleanup has been called too many times.
			result = netErrNotOpen;
			break;

		case WSANO_DATA:
			// (11004)
			// Valid name, no data record of requested type.
			// The requested name is valid and was found in the database, but it does not have the correct associated data being resolved for. The usual example for this is a hostname -> address translation attempt (using gethostbyname or WSAAsyncGetHostByName) which uses the DNS (Domain Name Server), and an MX record is returned but no A record - indicating the host itself exists, but is not directly reachable.
			result = netErrDNSFormat;
			break;

		case WSANO_RECOVERY:
			// (11003)
			// This is a non-recoverable error.
			// This indicates some sort of non-recoverable error occurred during a database lookup. This may be because the database files (e.g. BSD-compatible HOSTS, SERVICES or PROTOCOLS files) could not be found, or a DNS request was returned by the server with a severe error.
			result = netErrInternal;
			break;

//		case WSAPROVIDERFAILEDINIT:
			// (OS dependent)
			// Unable to initialize a service provider.
			// Either a service provider's DLL could not be loaded (LoadLibrary failed) or the provider's WSPStartup/NSPStartup function failed.

//		case WSASYSCALLFAILURE:
			// (OS dependent)
			// System call failure.
			// Returned when a system call that should never fail does. For example, if a call to WaitForMultipleObjects fails or one of the registry functions fails trying to manipulate theprotocol/namespace catalogs.

		case WSASYSNOTREADY:
			// (10091)
			// Network subsystem is unavailable.
			// This error is returned by WSAStartup if the Windows Sockets implementation cannot function at this time because the underlying system it uses to provide network services is currently unavailable. Users should check:
			// * that the appropriate Windows Sockets DLL file is in the current path,
			// * that they are not trying to use more than one Windows Sockets implementation simultaneously. If there is more than one WINSOCK DLL on your system, be sure the first one in the path is appropriate for the network subsystem currently loaded.
			// * the Windows Sockets implementation documentation to be sure all necessary components are currently installed and configured correctly.
			result = netErrInternal;
			break;

		case WSATRY_AGAIN:
			// (11002)
			// Non-authoritative host not found.
			// This is usually a temporary error during hostname resolution and means that the local server did not receive a response from an authoritative server. A retry at some time later may be successful.
			result = netErrDNSTimeout;
			break;

		case WSAVERNOTSUPPORTED:
			// (10092)
			// WINSOCK.DLL version out of range.
			// The current Windows Sockets implementation does not support the Windows Sockets specification version requested by the application. Check that no old Windows Sockets DLL files are being accessed.
			result = netErrInternal;
			break;

		case WSAEDISCON:
			// (10094)
			// Graceful shutdown in progress.
			// Returned by WSARecv and WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence.
			result = netErrSocketNotConnected;
			break;

//		case WSA_OPERATION_ABORTED:
			// (OS dependent)
			// Overlapped operation aborted.
			// An overlapped operation was canceled due to the closure of the socket, or the execution of the SIO_FLUSH command in WSAIoctl.
#endif	// PLATFORM_WINDOWS
	}

	PRINTF ("PrvTranslateError: mapping host error code %ld (0x%08lX) to NetLib code %ld (0x%08lX).",
		err, err, result, result);

	return result;
}
