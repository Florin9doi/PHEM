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
#include "EmExgMgr.h"

#include "EmBankMapped.h"		// EmBankMapped
#include "EmCPU.h"				// Emulator::SetBreakReason (kBreak_TaskFinished)
#include "EmFileImport.h"		// SetResult, SetDone
#include "EmMemory.h"			// EmMem_strncpy, EmMem_memset
#include "EmPalmStructs.h"		// SysLibTblEntryType, ExgGoToType, ExgSocketType
#include "EmSession.h"			// gSession, SuspendByTimeout
#include "EmStream.h"			// EmStream
#include "EmStreamFile.h"		// EmStreamFile
#include "ROMStubs.h"			// SysLibTblEntry


// ======================================================================
//	Globals
// ======================================================================

#define exgLibCtlGetTitle		1					// get title for Exg dialogs
#define exgLibCtlSpecificOp		0x8000				// start of range for library specific control codes

const UInt16	kHostExgLibInitSocket		= exgLibCtlSpecificOp | 0;
const UInt16	kHostExgLibFreeSocket		= exgLibCtlSpecificOp | 1;
const UInt16	kHostExgLibBeamCompleted	= exgLibCtlSpecificOp | 2;

#define exgMemError	 			(exgErrorClass | 1)
#define exgErrStackInit 		(exgErrorClass | 2)  // stack could not initialize
#define exgErrUserCancel 		(exgErrorClass | 3)
#define exgErrNoReceiver 		(exgErrorClass | 4)	// receiver device not found
#define exgErrNoKnownTarget		(exgErrorClass | 5)	// can't find a target app
#define exgErrTargetMissing		(exgErrorClass | 6)  // target app is known but missing
#define exgErrNotAllowed		(exgErrorClass | 7)  // operation not allowed
#define exgErrBadData			(exgErrorClass | 8)  // internal data was not valid
#define exgErrAppError			(exgErrorClass | 9)  // generic application error
#define exgErrUnknown			(exgErrorClass | 10) // unknown general error
#define exgErrDeviceFull		(exgErrorClass | 11) // device is full
#define exgErrDisconnected		(exgErrorClass | 12) // link disconnected
#define exgErrNotFound			(exgErrorClass | 13) // requested object not found
#define exgErrBadParam			(exgErrorClass | 14) // bad parameter to call
#define exgErrNotSupported		(exgErrorClass | 15) // operation not supported by this library
#define exgErrDeviceBusy		(exgErrorClass | 16) // device is busy
#define exgErrBadLibrary		(exgErrorClass | 17) // bad or missing ExgLibrary


/***********************************************************************
 *
 * CLASS:		EmExgMgr
 *
 * DESCRIPTION:	This class represents an Exchange Manager driver
 *		library.  It works in conjunction with an actual stub library
 *		installed by Poser into the current session when the session
 *		is created.  Using HostControl callback functions, the stub
 *		library calls into methods in EmExgMgr subclasses.  We thus
 *		create subclasses in order to provide specific library
 *		functionality.
 *
 *		EmExgMgr doesn't do anything itself except define an interface.
 *		All useful functionality is provided by subclasses.
 *
 *		Besides forwarding all calls ExgLib calls back to Poser via
 *		HostControl functions, the stub library does the following:
 *
 *		* Looks for irGotDataChr characters in the event stream and
 *			starts a beam process when it finds one.
 *
 *		* Sends a kHostExgLibInitSocket control code to the host
 *			driver to fill out the ExgSocketType data structure.
 *
 *		* Calls ExgNotifyReceive.  Note that having the stub library
 *			make this call is very important.  If the host driver
 *			were to call ExgNotifyReceive in its implementation of
 *			ExgLibHandleEvent, then control would not be returned back
 *			to us until ExgNotifyReceive had completed.  And without
 *			control, we can't update progress dialogs or look for
 *			clicks on Cancel buttons.
 *
 *		* Sends a kHostExgLibFreeSocket control code to clean up anything
 *			set up by the kHostExgLibInitSocket code.
 *
 *		* Sends a kHostExgLibBeamCompleted control code to the host
 *			driver in order to let it know that it can now remove any
 *			progress dialogs and/or display error messages.
 *
 * PROBLEMS:
 *
 *		This module needs to take into account a number of problems
 *		in order to work:
 *
 *		* There is a bug in Launcher 3.0 - 3.5 where it tries to send
 *		  sysAppLaunchCmdSyncNotify to anything received that's not a
 *		  PQA.  We have a patch on SysAppLaunch to deal with this.
 *
 *		* We have to make sure no UI will appear.  We have patches on
 *		  FrmCustomAlert and ExgDoDialog to make sure this doesn't happen.
 *
 *		* Palm VII's aren't even registered to recieve PQAs!
 *
 ***********************************************************************/

// ---------------------------------------------------------------------------
//		¥ EmExgMgr::EmExgMgr
// ---------------------------------------------------------------------------
//	Constructor for abstract base class.  Does nothing.

EmExgMgr::EmExgMgr (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgr::~EmExgMgr
// ---------------------------------------------------------------------------
//	Destructor for abstract base class.  Does nothing.

EmExgMgr::~EmExgMgr (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgr::GetExgMgr
// ---------------------------------------------------------------------------
//	Static method for finding and returning the host-implemented Exchange
//	Manager driver associated with the given library or socket.  Our stub
//	library stores the pointer to the EmExgMgr subclass in its library
//	globals field, so we fetch it from there.

EmExgMgr* EmExgMgr::GetExgMgr (UInt16 libRefNum)
{
	emuptr	entryP = (emuptr) ::SysLibTblEntry (libRefNum);
	if (!entryP)
		return NULL;

	EmAliasSysLibTblEntryType<PAS>	entry (entryP);
	emuptr	globalsP = entry.globalsP;
	return (EmExgMgr*) globalsP;
}


#pragma mark -

/***********************************************************************
 *
 * CLASS:		EmExgMgrStream
 *
 * DESCRIPTION:	.
 *
 ***********************************************************************/

// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::EmExgMgrStream
// ---------------------------------------------------------------------------

EmExgMgrStream::EmExgMgrStream (EmStream& stream) :
	fStream (stream)
{
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::~EmExgMgrStream
// ---------------------------------------------------------------------------

EmExgMgrStream::~EmExgMgrStream (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibOpen
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibOpen (UInt16 /*libRefNum*/)
{
	return errNone;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibClose
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibClose (UInt16 /*libRefNum*/)
{
	return errNone;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibSleep
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibSleep (UInt16 /*libRefNum*/)
{
	return errNone;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibWake
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibWake (UInt16 /*libRefNum*/)
{
	return errNone;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibHandleEvent
// ---------------------------------------------------------------------------

Boolean EmExgMgrStream::ExgLibHandleEvent (UInt16 /*libRefNum*/, emuptr /*eventP*/)
{
	return false;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibConnect
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibConnect (UInt16 libRefNum, emuptr exgSocketP)
{
	// Same as ExgLibPut?  Applications seems to call ExgLibPut.
	// Additionally, IrLib's version of ExgLibConnect merely forwards
	// to ExgLibPut.

	return EmExgMgrStream::ExgLibPut (libRefNum, exgSocketP);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibAccept
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibAccept (UInt16 /*libRefNum*/, emuptr /*exgSocketP*/)
{
	fStream.SetMarker (0, kStreamFromStart);
	return errNone;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibDisconnect
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibDisconnect (UInt16 /*libRefNum*/, emuptr /*exgSocketP*/, Err error)
{
	return error;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibPut
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibPut (UInt16 /*libRefNum*/, emuptr /*exgSocketP*/)
{
	// Same as ExgLibConnect?  Applications seems to call ExgLibPut.
	// Additionally, IrLib's version of ExgLibConnect merely forwards
	// to ExgLibPut.

	return exgErrNotSupported;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibGet
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibGet (UInt16 /*libRefNum*/, emuptr /*exgSocketP*/)
{
	// What does this function do?  Nothing in the ROM seems to call
	// it, and the IrLib version merely returns zero.

	return exgErrNotSupported;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibSend
// ---------------------------------------------------------------------------

UInt32 EmExgMgrStream::ExgLibSend (UInt16 /*libRefNum*/, emuptr /*exgSocketP*/,
								 void* bufP, const UInt32 bufLen, Err* errP)
{
	if (bufLen > 0)
	{
		// Transfer the bytes.

		*errP = fStream.PutBytes (bufP, bufLen);
	}
	else
	{
		*errP = errNone;
	}

	return bufLen;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibReceive
// ---------------------------------------------------------------------------

UInt32 EmExgMgrStream::ExgLibReceive (UInt16 /*libRefNum*/, emuptr /*exgSocketP*/,
									void* bufP, const UInt32 bufLen, Err* errP)
{
	// Determine how many bytes to transfer.

	long	amountToCopy = bufLen;
	long	amountLeftInBuffer = fStream.GetLength () - fStream.GetMarker ();
	if (amountToCopy > amountLeftInBuffer)
		amountToCopy = amountLeftInBuffer;

	if (amountToCopy > 0)
	{
		// Transfer the bytes.

		*errP = fStream.GetBytes (bufP, amountToCopy);
	}
	else
	{
		*errP = errNone;
	}

	return amountToCopy;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibControl
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibControl (UInt16 libRefNum, UInt16 op,
								 emuptr valueP, emuptr valueLenP)
{
	Err	err = errNone;

	switch (op)
	{
		case exgLibCtlGetTitle:
			if (valueP && valueLenP)
			{
				EmAliasUInt16<PAS>	valueLen (valueLenP);
				EmMem_strncpy (valueP, "Poser Beam", valueLen);
			}
			else
			{
				err = exgErrBadParam;
			}
			break;

		case kHostExgLibInitSocket:
			if (valueP && valueLenP)
			{
				EmAliasUInt16<PAS>			valueLen (valueLenP);
				EmAliasExgSocketType<PAS>	socket (valueP);

				EmAssert ((UInt16) valueLen == socket.GetSize ());

				const UInt16	kLocalModeBit	= 0x8000;
//				const UInt16	kPacketModeBit	= 0x4000;
//				const UInt16	kNoGoToBit		= 0x2000;
				const UInt16	kNoStatusBit	= 0x1000;

				EmMem_memset (valueP, 0, valueLen);

				socket.libraryRef	= libRefNum;	// identifies the Exg library in use
//				socket.socketRef	= 0;			// used by Exg library to identify this connection
//				socket.target		= 0;			// Creator ID of application this is sent to
//				socket.count		= 0;			// # of objects in this connection (usually 1)
//				socket.length		= 0;			// # total byte count for all objects being sent (optional)
//				socket.time			= 0;			// last modified time of object (optional)
//				socket.appData		= 0;			// application specific info
//				socket.goToCreator	= 0;			// creator ID of app to launch with goto after receive
//				socket.goToParams;					// If launchCreator then this contains goto find info
//				socket.goToParams.dbCardNo		= 0;	// card number of the database	
//				socket.goToParams.dbID			= 0;	// LocalID of the database
//				socket.goToParams.recordNum		= 0;	// index of record that contain a match
//				socket.goToParams.uniqueID		= 0;	// postion in record of the match.
//				socket.goToParams.matchCustom	= 0;	// application specific info
				socket.flags		= kLocalModeBit | kNoStatusBit;		// system flags
//				socket.description	= EmMemNULL;		// text description of object (for user)
//				socket.type			= EmMemNULL;		// Mime type of object (optional)
//				socket.name			= EmMemNULL;		// name of object, generally a file name (optional)

				EmStreamFile*	s = dynamic_cast<EmStreamFile*> (&fStream);
				if (s)
				{
					fFileName = s->GetFileRef().GetName ();
					EmBankMapped::MapPhysicalMemory (fFileName.c_str (), fFileName.size () + 1);
					socket.name = EmBankMapped::GetEmulatedAddress (fFileName.c_str ());
				}
			}
			else
			{
				err = exgErrBadParam;
			}
			break;

		case kHostExgLibFreeSocket:
		{
			// Unmap file name and drop our reference to it.

#ifndef NDEBUG
			EmAliasUInt16<PAS>			valueLen (valueLenP);
#endif
			EmAliasExgSocketType<PAS>	socket (valueP);

			EmAssert ((UInt16) valueLen == socket.GetSize ());

			socket.name = EmMemNULL;
			EmBankMapped::UnmapPhysicalMemory (fFileName.c_str ());
			break;
		}

		default:
			err = exgErrNotSupported;		
	}

	return err;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrStream::ExgLibRequest
// ---------------------------------------------------------------------------

Err EmExgMgrStream::ExgLibRequest (UInt16 /*libRefNum*/, emuptr /*exgSocketP*/)
{
	return exgErrNotSupported;
}


#pragma mark -

/***********************************************************************
 *
 * CLASS:		EmExgMgrImportWrapper
 *
 * DESCRIPTION:	This class is used to wrap up other EmExgMgr
 *				implementations.  It adds the facility for communicating
 *				information back to the EmFileImport class (for instance
 *				progress information and whether or not the beam
 *				process is completed).  It also allows the beaming
 *				process to be aborted.
 *
 ***********************************************************************/

// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::EmExgMgrImportWrapper
// ---------------------------------------------------------------------------
//	Remember the wrapped object, as well as the EmFileImport object we
//	communicate with.  Initialize our "aborting" flag to false.

EmExgMgrImportWrapper::EmExgMgrImportWrapper (EmExgMgr& exgMgr, EmFileImport& importer) :
	fExgMgr (exgMgr),
	fImporter (importer),
	fAborting (false)
{
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::~EmExgMgrImportWrapper
// ---------------------------------------------------------------------------

EmExgMgrImportWrapper::~EmExgMgrImportWrapper (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::Cancel
// ---------------------------------------------------------------------------
//	Set our "aborting" flag to true.  This will cause our read and write
//	methods to return "user cancelled".

void EmExgMgrImportWrapper::Cancel (void)
{
	fAborting = true;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibOpen
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibOpen (UInt16 libRefNum)
{
	return fExgMgr.ExgLibOpen (libRefNum);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibClose
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibClose (UInt16 libRefNum)
{
	return fExgMgr.ExgLibClose (libRefNum);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibSleep
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibSleep (UInt16 libRefNum)
{
	return fExgMgr.ExgLibSleep (libRefNum);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibWake
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibWake (UInt16 libRefNum)
{
	return fExgMgr.ExgLibWake (libRefNum);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibHandleEvent
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Boolean EmExgMgrImportWrapper::ExgLibHandleEvent (UInt16 libRefNum, emuptr eventP)
{
	return fExgMgr.ExgLibHandleEvent (libRefNum, eventP);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibConnect
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibConnect (UInt16 libRefNum, emuptr exgSocketP)
{
	return fExgMgr.ExgLibConnect (libRefNum, exgSocketP);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibAccept
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibAccept (UInt16 libRefNum, emuptr exgSocketP)
{
	return fExgMgr.ExgLibAccept (libRefNum, exgSocketP);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibDisconnect
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibDisconnect (UInt16 libRefNum, emuptr exgSocketP, Err error)
{
	return fExgMgr.ExgLibDisconnect (libRefNum, exgSocketP, error);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibPut
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibPut (UInt16 libRefNum, emuptr exgSocketP)
{
	return fExgMgr.ExgLibPut (libRefNum, exgSocketP);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibGet
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibGet (UInt16 libRefNum, emuptr exgSocketP)
{
	return fExgMgr.ExgLibGet (libRefNum, exgSocketP);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibSend
// ---------------------------------------------------------------------------
//	If aborting, return "user cancelled".  Otherwise, call the wrapped method.

UInt32 EmExgMgrImportWrapper::ExgLibSend (UInt16 libRefNum, emuptr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP)
{
	if (fAborting)
	{
		*errP = exgErrUserCancel;
		return 0;
	}

	return fExgMgr.ExgLibSend (libRefNum, exgSocketP, bufP, bufLen, errP);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibReceive
// ---------------------------------------------------------------------------
//	If aborting, return "user cancelled".  Otherwise, call the wrapped method.
//	Also, set the emulator flag indicating that the emulation loop should
//	exit.  Doing this allows the UI task to regain control, update dialogs
//	and check for Stop button clicks.

UInt32 EmExgMgrImportWrapper::ExgLibReceive (UInt16 libRefNum, emuptr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP)
{
	if (fAborting)
	{
		*errP = exgErrUserCancel;
		return 0;
	}

	UInt32	result = fExgMgr.ExgLibReceive (libRefNum, exgSocketP, bufP, bufLen, errP);

	EmAssert (gSession);
	gSession->ScheduleSuspendTimeout ();

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibControl
// ---------------------------------------------------------------------------
//	See if the operation is "beam completed".  If so, let the EmFileImport
//	object know so that it can close the progress dialog and report any
//	errors.  In all cases, also call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibControl (UInt16 libRefNum, UInt16 op, emuptr valueP, emuptr valueLenP)
{
	if (op == kHostExgLibBeamCompleted)
	{
		EmAliasErr<PAS>			err (valueP);

		fImporter.SetResult ((Err) err);
		fImporter.SetDone ();
	}

	return fExgMgr.ExgLibControl (libRefNum, op, valueP, valueLenP);
}


// ---------------------------------------------------------------------------
//		¥ EmExgMgrImportWrapper::ExgLibRequest
// ---------------------------------------------------------------------------
//	Merely call the wrapped method.

Err EmExgMgrImportWrapper::ExgLibRequest (UInt16 libRefNum, emuptr exgSocketP)
{
	return fExgMgr.ExgLibRequest (libRefNum, exgSocketP);
}
