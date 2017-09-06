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
#include "EmFileImport.h"

#include "EmCPU.h"				// Emulator::ExecuteUntilIdle
#include "EmDlg.h"				// EmDlg::DoCommonDialog
#include "EmErrCodes.h"			// kError_OutOfMemory, ConvertFromPalmError, etc.
#include "EmExgMgr.h"			// EmExgMgrStream, EmExgMgrImport
#include "EmLowMem.h"			// TrapExists
#include "EmPalmStructs.h"		// SysLibTblEntryType, RecordEntryType, RsrcEntryType, etc.
#include "EmPatchState.h"		// EmPatchState::AutoAcceptBeamDialogs
#include "EmSession.h"			// ExecuteUntilIdle, gSession
#include "EmStreamFile.h"		// EmStreamFile, kOpenExistingForRead
#include "ErrorHandling.h"		// Errors::SetParameter
#include "Miscellaneous.h"		// StMemoryMapper
#include "Platform.h"			// Platform::DisposeMemory
#include "ROMStubs.h"			// ExgLibControl, DmFindDatabase, EvtEnqueueKey, EvtWakeup, DmDeleteDatabase, DmCreateDatabase...
#include "Strings.r.h"			// kStr_CmdInstall

#include "PHEMNativeIF.h"

#include <algorithm>			// find
#include <stdio.h>				// sprintf
#include <time.h>				// strftime


/*
	This module is responsible for installing files containing Palm OS
	applications, databases, query applications, text data, vCals, and
	vCards.

	The entire installation process is handled by the EmFileImport
	class.  You can either create one of these and continually call its
	Continue method until either it returns an error or its Done method
	returns true.  Using this approach is handy if you need to do
	something else in-between calls to Continue (such as update a
	progress dialog).  If you don't need to do anything else (like
	handle UI stuff), you could instead just call the static method
	LoadPalmFileList.  This method installs all the given files and
	doesn't return until either an error occurs or the job is done.

	Files are installed by one of two methods: either by using the
	Exchange Manager, or by using low-level Database Manager routines.
	The Exchange Manager is used when possible, and is the only way
	that all supported file types can be installed.  If the Exchange
	Manager cannot be used (for example, we're emulating a Palm OS 2.0
	device, or our custom Exchange Manager driver isn't installed),
	only .prc, .pdb, and .pqa files can be installed.  Additionally,
	after the files are installed, any application that would like
	to know about those files (like the Launcher) are NOT notified,
	and may fail after the installation process.  Therefore, we always
	try to use the Exchange Manager if possible.

	In order to use the Exchange Manager, we have to install and make
	use of a small, custom driver library.  This library is closely
	integrated with Poser.  In order to kick of an install process,
	Poser makes an ExgLibControl call to our library.  The library
	then takes over, making the appropriate HostControl calls to
	read in the contents of files, to update Poser's progress
	indicator, and to report on the status of the install.  Poser
	merely sits back and updates the progress bar until the driver
	says it's done.

	If we're not using the Exchange Manager method, we instead use
	standard Database Manager calls to install the file.  That is,
	we call DmCreateDatabase, DmNewResource, DmWrite, etc., in
	order to convert the file into a database.
*/

const int		kInstallStart	= -1;
const int		kInstallMiddle	= -2;
const int		kInstallEnd		= -3;
const int		kInstallDone	= -4;

#define irGotDataChr 0x01FC		// to initiate NotifyReceive

const char		kHostExgLibName[]		= "HostExgLib";
const UInt32	kHostExgLibCreator		= 'HExg';

	// Forward declarations.
extern const uint8	kHostExgLib[];
extern const int	kHostExgLibSize;

static Bool		PrvDetermineMethod	(EmFileImportMethod method);
static Bool		PrvCanUseExgMgr		(void);
static Bool		PrvHasExgMgr		(void);
//static Bool		PrvHostExgLibLoaded	(void);
static Bool		PrvIsResources		(UInt16 attributes);
static Bool		PrvIsResources		(const EmAliasDatabaseHdrType<LAS>& hdr);
static UInt32	PrvGetEntrySize		(const EmAliasDatabaseHdrType<LAS>& hdr);
static void		PrvSetResourceTypeIDParameters (UInt32 resType, UInt16 resID);

static ErrCode	PrvValidateDatabase	(const EmAliasDatabaseHdrType<LAS>& hdr, UInt32 size);
static void		PrvSetDate			(const char* varName, uint32 seconds);
static void		PrvSetExgMgr		(void* mgr);

static UInt16	gHostExgLibRefNum;


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::EmFileImport
 *
 * DESCRIPTION:	Constructor.  Initializes our data members.
 *
 * PARAMETERS:	file - ref to file to install
 *				gotoWhenDone - if true, switch to the application that
 *					just received the file we installed and display it,
 *					if possible.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

EmFileImport::EmFileImport	(EmStream& stream, EmFileImportMethod method) :
	fUsingExgMgr (::PrvDetermineMethod (method)),
	fGotoWhenDone (false),
	fState (kInstallStart),
	fError (errNone),
	fStream (stream),

	fExgMgrStream (NULL),
	fExgMgrImport (NULL),
	fOldAcceptBeamState (EmPatchState::AutoAcceptBeamDialogs (true)),

	fFileBuffer (NULL),
	fFileBufferSize (0),
	fDBID (0),
	fCardNo (0),
	fOpenID (0),
	fCurrentEntry (0)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::~EmFileImport
 *
 * DESCRIPTION:	Destructor.  Clean up.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

EmFileImport::~EmFileImport	(void)
{
	::PrvSetExgMgr (NULL);

	delete fExgMgrStream;
	delete fExgMgrImport;

	Platform::DisposeMemory (fFileBuffer);

	EmPatchState::AutoAcceptBeamDialogs (fOldAcceptBeamState);
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::LoadPalmFile
 *
 * DESCRIPTION:	Utility function to install a Palm application, given
 *				a buffer containing it.
 *
 * PARAMETERS:	data - pointer to the image.
 *				size - number of bytes in the image
 *
 * RETURNED:	result code
 *
 ***********************************************************************/

ErrCode EmFileImport::LoadPalmFile (const void* data, uint32 size,
									EmFileImportMethod method,
									LocalID &newID)
{
	ErrCode			err = errNone;
	EmStreamBlock	stream ((void*) data, size);
	EmFileImport	importer (stream, method);

	while (!importer.Done () && err == errNone)
	{
		err = importer.Continue ();
	}

	newID = importer.GetLocalID ();

	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::LoadPalmFileList
 *
 * DESCRIPTION:	Utility function to install a collection of files.  This
 *				is intended to be a faceless function; no UI is involved.
 *
 * PARAMETERS:	fileList - collection of files to install.
 *
 * RETURNED:	result code
 *
 ***********************************************************************/

ErrCode EmFileImport::LoadPalmFileList (const EmFileRefList& fileList,
										EmFileImportMethod method,
										vector<LocalID>& newIDList)
{
	ErrCode	err = errNone;

	newIDList.clear ();

        PHEM_Log_Msg("LoadPalmFileList starting.");
	EmFileRefList::const_iterator	iter = fileList.begin ();
        PHEM_Log_Msg("Starting loop.");
	while (iter != fileList.end ())
	{
                PHEM_Log_Msg("Getting stream.");
		EmStreamFile	stream (*iter, kOpenExistingForRead);
                PHEM_Log_Msg("Creating importer.");
		EmFileImport	importer (stream, method);

		while (!importer.Done () && err == errNone)
		{
                        PHEM_Log_Msg("Calling importer.Continue().");
			err = importer.Continue ();
		}

                PHEM_Log_Msg("Pushing back new LocalID.");
		newIDList.push_back (importer.GetLocalID ());

		++iter;
	}

        PHEM_Log_Msg("Done, err =");
        PHEM_Log_Place(err);
	return err;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::InstallExgMgrLib
 *
 * DESCRIPTION:	Install our Exchange Manager stub driver library if it
 *				looks OK to do so.  We install it if we're running
 *				OS 3.0 or later, and if the library is not already
 *				installed.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	result code
 *
 ***********************************************************************/

ErrCode EmFileImport::InstallExgMgrLib (void)
{
	// Don't install it if we don't have the Exchange Manager.

	if (!::PrvHasExgMgr ())
		return errNone;

	// Don't install it if it's already there.

	LocalID id = ::DmFindDatabase (0, kHostExgLibName);
	if (id != 0)
		return errNone;

	// OK to install it.

	return EmFileImport::LoadPalmFile (kHostExgLib, kHostExgLibSize, kMethodHomebrew, id);
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::CanUseExgMgr
 *
 * DESCRIPTION:	Return whether or not it looks like we will use the
 *				Exchange Manager when installing files.  Knowing this
 *				can be important upstream client who need to know
 *				what sorts of files to pass on to EmFileImport.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if we'll be using the ExgMgr.
 *
 ***********************************************************************/

Bool EmFileImport::CanUseExgMgr (void)
{
	return ::PrvCanUseExgMgr ();
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::Continue
 *
 * DESCRIPTION:	Install a small part of the application this object has
 *				been charged with installing.  Clients should continually
 *				call this method until either it returns a non-zero
 *				result or the Done method returns true.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	result code
 *
 ***********************************************************************/

ErrCode EmFileImport::Continue (void)
{
	// Install as much as we can in the allotted amount of time.

	const uint32	kIntervalTimeMSecs = 100;
	uint32			start = Platform::GetMilliseconds ();

	while (fState != kInstallDone &&
		(Platform::GetMilliseconds () - start) < kIntervalTimeMSecs)
	{
		this->IncrementalInstall ();

		if (fError != errNone)
		{
			fState = kInstallDone;
		}
	}

	return fError;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::Cancel
 *
 * DESCRIPTION:	Clean up any partially installed file.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	result code
 *
 ***********************************************************************/

ErrCode EmFileImport::Cancel (void)
{
	if (fUsingExgMgr)
	{
		this->ExgMgrInstallCancel ();
	}
	else
	{
		this->HomeBrewInstallCancel ();
	}

	fState = kInstallDone;

	return fError;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::Done
 *
 * DESCRIPTION:	Returns whether or not the file this object has been
 *				charged with installing has been fully installed.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if so.
 *
 ***********************************************************************/

Bool EmFileImport::Done (void)
{
	return fState == kInstallDone;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::GetProgress
 *
 * DESCRIPTION:	Return a value indicating how far through the install
 *				process we currently are.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	A value ranging from 0 to the value returned from
 *				CalculateProgressMax if it were fed a list consisting
 *				solely of the file we're installing now.
 *
 ***********************************************************************/

long EmFileImport::GetProgress (void)
{
	if (fUsingExgMgr)
	{
		return fStream.GetMarker ();
	}
	else
	{
		return fCurrentEntry;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::GetLocalID
 *
 * DESCRIPTION:	Return the LocalID of the database we just installed.
 *              This is only valid when the importer is "Done".
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	-1 if no database has been installed, otherwise a valid
 *				LocalID.
 *
 ***********************************************************************/

LocalID EmFileImport::GetLocalID (void)
{
	if (fUsingExgMgr)
	{
		return 0;
	}
	else
	{
		return fDBID;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::CalculateProgressMax
 *
 * DESCRIPTION:	Generate a value that can be used as a progress
 *				indicator's maximum value.  Note that the actual value
 *				returned is different depending on whether the Exchange
 *				or Database Manager is used, so don't try interpreting
 *				the result too closely.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	A progress indicator's maximum value.
 *
 ***********************************************************************/

long EmFileImport::CalculateProgressMax	(const EmFileRefList& files, EmFileImportMethod method)
{
	long	result = 0;
	Bool	useExgMgr = ::PrvDetermineMethod (method);

	EmFileRefList::const_iterator	iter = files.begin ();
	while (iter != files.end ())
	{
		EmStreamFile	file (*iter, kOpenExistingForRead);

		if (useExgMgr)
		{
			// Add up all the file sizes.

			result += file.GetLength ();
		}
		else
		{
			// Add up all the number of entries.

			EmProxyDatabaseHdrType	hdr;
			file.GetBytes (hdr.GetPtr (), hdr.GetSize ());
			result += hdr.recordList.numRecords;
		}

		++iter;
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::SetResult
 *
 * DESCRIPTION:	Called by external clients (notable the HostControl
 *				function called by our Exchange Manager driver) to set
 *				a result value.
 *
 * PARAMETERS:	err - result of the install process.  Can be errNone
 *					if no errors occurred.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::SetResult (Err err)
{
	this->SetResult (::ConvertFromPalmError (err));
}


void EmFileImport::SetResult (ErrCode err)
{
	fError = err;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::SetDone
 *
 * DESCRIPTION:	Called by external clients (notable the HostControl
 *				function called by our Exchange Manager driver) to set
 *				a result value.
 *
 * PARAMETERS:	err - result of the install process.  Can be errNone
 *					if no errors occurred.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::SetDone (void)
{
	fState = kInstallDone;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::IncrementalInstall
 *
 * DESCRIPTION:	Install a small part of the file.  Examines the current
 *				installation method and state and calls a function
 *				appropriate for the next stage.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::IncrementalInstall (void)
{
	if (fUsingExgMgr)
	{
		switch (fState)
		{
			case kInstallStart:		this->ExgMgrInstallStart ();	break;
			case kInstallMiddle:	this->ExgMgrInstallMiddle ();	break;
			case kInstallEnd:		this->ExgMgrInstallEnd ();		break;
			case kInstallDone:		break;
		}
	}
	else
	{
		switch (fState)
		{
			case kInstallStart:		this->HomeBrewInstallStart ();	break;
			case kInstallMiddle:	this->HomeBrewInstallMiddle ();	break;
			case kInstallEnd:		this->HomeBrewInstallEnd ();	break;
			case kInstallDone:		break;
		}
	}
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmFileImport::ExgMgrInstallStart
 *
 * DESCRIPTION:	Performs the first part of the installation process
 *				using the Exchange Manager.  This stage involves telling
 *				our driver what file to install, and then telling it
 *				to start installing.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::ExgMgrInstallStart (void)
{
	this->ValidateStream ();
	if (fError)
		return;

	// Create the stub ExgMgr driver callback object.

	EmAssert (gHostExgLibRefNum != 0);
	EmAssert (fExgMgrStream == NULL);
	EmAssert (fExgMgrImport == NULL);

	fStream.SetMarker (0, kStreamFromStart);
	fExgMgrStream = new EmExgMgrStream (fStream);
	fExgMgrImport = new EmExgMgrImportWrapper (*fExgMgrStream, *this);

	// Tell the library what host-based driver to use.

	::PrvSetExgMgr (fExgMgrImport);

	if (!fError)
	{
		// Kick off the install process.

		Err	err = ::EvtEnqueueKey (irGotDataChr, gHostExgLibRefNum, libEvtHookKeyMask);
		this->SetResult (err);

		err = ::EvtWakeup ();
	}

	fState = kInstallMiddle;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::ExgMgrInstallMiddle
 *
 * DESCRIPTION:	Performs any repeating part of the installation process
 *				using the Exchange Manager.  This stage merely involves
 *				giving the Emulator a chance to execute.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::ExgMgrInstallMiddle (void)
{
	EmAssert (gSession);
// !!!	gSession->ExecuteUntilIdle ();
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::ExgMgrInstallEnd
 *
 * DESCRIPTION:	Performs any final part of the installation process
 *				using the Exchange Manager.  There's nothing we need
 *				to do here.  The end of the installation process is
 *				signalled when our Exchange Manager driver calls
 *				SetResult via the HostControl functions.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::ExgMgrInstallEnd (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::ExgMgrInstallCancel
 *
 * DESCRIPTION:	Cancels the installation process using the Exchange
 *				Manager.  We do this by sending a message to our
 *				Exchange Manager driver.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::ExgMgrInstallCancel (void)
{
	// Tell the library to stop.

	fExgMgrImport->Cancel ();
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmFileImport::HomeBrewInstallStart
 *
 * DESCRIPTION:	Performs the first part of the installation process
 *				using the Database Manager.  This stage involves
 *				loading the file, scoping out its header, deleting any
 *				old databases with the same name, creating the new
 *				database, setting its attributes, and opening it.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::HomeBrewInstallStart (void)
{
	this->ValidateStream ();
	if (fError)
		return;

	Err	err = errNone;

	// Assume failure

	fCardNo	= 0;
	fDBID	= 0;
	fOpenID	= 0;

	// Get the file and read it into memory.
	// Set up a proxy to get to the header contents.

	EmAssert (fFileBuffer == NULL);

	fFileBufferSize = fStream.GetLength ();
	fFileBuffer		= Platform::AllocateMemory (fFileBufferSize);
	if (!fFileBuffer)
	{
		this->SetResult (kError_OutOfMemory);
		return;
	}

	fStream.SetMarker (0, kStreamFromStart);
	fStream.GetBytes (fFileBuffer, fFileBufferSize);

	EmAliasDatabaseHdrType<LAS>	hdr (fFileBuffer);

	// See if there's already a database with this name.
	// If the database already exists, delete it.

	LocalID tempID = ::DmFindDatabase (fCardNo, (Char*) hdr.name.GetPtr ());

	if (tempID != 0)
	{
		err = ::DmDeleteDatabase (fCardNo, tempID);

		if (err && err != dmErrROMBased)
			goto Error;
	}

	// Create the new database

	err = ::DmCreateDatabase (fCardNo, (Char*) hdr.name.GetPtr (),
								hdr.creator, hdr.type, ::PrvIsResources (hdr));
	if (err)
		goto Error;

	// Set the database info

	fDBID = ::DmFindDatabase (fCardNo, (Char*) hdr.name.GetPtr ());
	if (!fDBID)
	{
		err = ::DmGetLastErr ();
		goto Error;
	}

	{
		UInt16	attributes			= hdr.attributes;
		UInt16	version				= hdr.version;
		UInt32	creationDate		= hdr.creationDate;
		UInt32	modificationDate	= hdr.modificationDate;
		UInt32	lastBackupDate		= hdr.lastBackupDate;
		UInt32	modificationNumber	= hdr.modificationNumber;
		UInt32	type				= hdr.type;
		UInt32	creator				= hdr.creator;

		err = ::DmSetDatabaseInfo (fCardNo, fDBID, NULL,
					&attributes, &version, &creationDate,
					&modificationDate, &lastBackupDate,
					&modificationNumber, NULL,
					NULL, &type, &creator);

		if (err)
			goto Error;
	}

	// Open the new database

	fOpenID = ::DmOpenDatabase (fCardNo, fDBID, dmModeReadWrite);
	if (!fOpenID)
	{
		err = ::DmGetLastErr ();
		goto Error;
	}

	// If this is a record database and there's an appInfo block, add it.

	if (!::PrvIsResources (hdr))
	{
		if (hdr.appInfoID)
		{
			UInt32		recSize;
			MemHandle	newRecH;

			EmAliasRecordEntryType<LAS> recordEntry (hdr.recordList.records);

			if (hdr.sortInfoID)
			{
				recSize = (UInt32) hdr.sortInfoID - (UInt32) hdr.appInfoID;
			}
			else if (hdr.recordList.numRecords > 0)
			{
				recSize = (UInt32) recordEntry.localChunkID - (UInt32) hdr.appInfoID;
			}
			else
			{
				recSize = fFileBufferSize - (UInt32) hdr.appInfoID;
			}

			// Set the error parameters in case an error occurs.

			Errors::SetParameter ("%res_size", recSize);

			// Allocate the new record.

			newRecH = (MemHandle) ::DmNewHandle (fOpenID, recSize);
			if (!newRecH)
			{
				err = ::DmGetLastErr ();
				goto Error;
			}

			// Copy the data in.

			UInt8*	srcP = (UInt8*) hdr.GetPtr () + (UInt32) hdr.appInfoID;
			UInt8*	dstP = (UInt8*) ::MemHandleLock ((MemHandle) newRecH);
			::DmWrite (dstP, 0, srcP, recSize);
			::MemHandleUnlock ((MemHandle) newRecH);

			// Store it in the header.

			LocalID	appInfoID = ::MemHandleToLocalID ((MemHandle) newRecH);
			err = ::DmSetDatabaseInfo (fCardNo, fDBID, NULL, NULL, NULL, NULL,
									NULL, NULL, NULL, &appInfoID, NULL, NULL, NULL);

			if (err)
				goto Error;
		}
	}

	// If there are resources/records, start installing them.

	if (hdr.recordList.numRecords > 0)
		fState = kInstallMiddle;
	else
		fState = kInstallEnd;

	return;

Error:
	this->DeleteCurrentDatabase ();
	this->SetResult (err);
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::HomeBrewInstallMiddle
 *
 * DESCRIPTION:	Performs any repeating part of the installation process
 *				using the Exchange Manager.  This stage involves
 *				creating and installing the next resource or record
 *				in the file.  If this is a record database and this
 *				is the first call to this method, we also install
 *				the appInfo record.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::HomeBrewInstallMiddle (void)
{
	Err		err = errNone;

	UInt8*	srcP;
	UInt8*	dstP;

	EmAssert (fFileBuffer);
	EmAliasDatabaseHdrType<LAS>	hdr (fFileBuffer);

	//------------------------------------------------------------
	// If it's a resource database, add the resources
	//------------------------------------------------------------

	if (::PrvIsResources (hdr))
	{
		UInt32			resSize;
		MemHandle		newResH;

		EmAliasRsrcEntryType<LAS> rsrcEntry (hdr.recordList.resources[fCurrentEntry]);

		// Set the error parameters in case an error occurs.

		Errors::SetParameter ("%record_number", fCurrentEntry);
		::PrvSetResourceTypeIDParameters (rsrcEntry.type, rsrcEntry.id);

		// Calculate the size of the resource.

		if (fCurrentEntry < hdr.recordList.numRecords - 1)
		{
			resSize = (UInt32) rsrcEntry[1].localChunkID;
			resSize -= (UInt32) rsrcEntry.localChunkID;
		}
		else
		{
			resSize = fFileBufferSize - (UInt32) rsrcEntry.localChunkID;
		}

		// Set the error parameters in case an error occurs.

		Errors::SetParameter ("%res_size", resSize);

		// Allocate the new resource.

		newResH = (MemHandle) ::DmNewResource (fOpenID, rsrcEntry.type, rsrcEntry.id, resSize);
		if (!newResH)
		{
			err = ::DmGetLastErr ();
			if (err == dmErrMemError)
			{
				this->DeleteCurrentDatabase ();
				this->SetResult (kError_BadDB_ResourceMemError);
				return;
			}
			goto Error;
		}

		// Copy the data in.

		srcP = (UInt8*) hdr.GetPtr () + (UInt32) rsrcEntry.localChunkID;
		dstP = (UInt8*) ::MemHandleLock ((MemHandle) newResH);
		::DmWrite (dstP, 0, srcP, resSize);
		::MemHandleUnlock ((MemHandle) newResH);

		// Done with it.

		::DmReleaseResource ((MemHandle) newResH);
	}

	//------------------------------------------------------------
	// If it's a record database, add the records
	//------------------------------------------------------------

	else
	{
		UInt32			recSize;
		MemHandle		newRecH;

		EmAliasRecordEntryType<LAS> recordEntry (hdr.recordList.records[fCurrentEntry]);

		// Set the error parameters in case an error occurs.

		Errors::SetParameter ("%record_number", fCurrentEntry);

		// Calculate the size of the record.

		if (fCurrentEntry < hdr.recordList.numRecords - 1)
		{
			recSize = (UInt32) recordEntry[1].localChunkID;
			recSize -= (UInt32) recordEntry.localChunkID;
		}
		else
		{
			recSize = fFileBufferSize - (UInt32) recordEntry.localChunkID;
		}

		// Set the error parameters in case an error occurs.

		Errors::SetParameter ("%res_size", recSize);

		//	Only add non-nil records. When the default databases are created
		//	they sometimes have nil records if the user had to delete a
		//	record they didn't want.

		if (recSize)
		{
			// Allocate the new record.

			UInt16	index = 0xFFFF;
			newRecH = (MemHandle) ::DmNewRecord (fOpenID, &index, recSize);
			if (!newRecH)
			{
				err = ::DmGetLastErr ();
				if (err == dmErrMemError)
				{
					this->DeleteCurrentDatabase ();
					this->SetResult (kError_BadDB_RecordMemError);
					return;
				}
				goto Error;
			}

			// Copy the data in.

			srcP = (UInt8*) hdr.GetPtr () + (UInt32) recordEntry.localChunkID;
			dstP = (UInt8*) ::MemHandleLock ((MemHandle) newRecH);
			::DmWrite (dstP, 0, srcP, recSize);
			::MemHandleUnlock ((MemHandle) newRecH);

			// Set the attributes.

			UInt16	attr = recordEntry.attributes;
			UInt32	id = recordEntry.uniqueID[0];
			id = (id << 8) | recordEntry.uniqueID[1];
			id = (id << 8) | recordEntry.uniqueID[2];
			err = ::DmSetRecordInfo (fOpenID, index, &attr, &id);
			if (err)
				goto Error;

			// Done with it.

			::DmReleaseRecord (fOpenID, index, true);
		}
	}

	fCurrentEntry++;

	if (fCurrentEntry >= hdr.recordList.numRecords)
	{
		fState = kInstallEnd;
	}

	return;

Error:
	this->DeleteCurrentDatabase ();
	this->SetResult (err);
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::HomeBrewInstallEnd
 *
 * DESCRIPTION:	Performs any final part of the installation process
 *				using the Exchange Manager.  This stage involves
 *				setting some final information about the database and
 *				then closing it up.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::HomeBrewInstallEnd (void)
{
	EmAssert (fFileBuffer);
	EmAliasDatabaseHdrType<LAS>	hdr (fFileBuffer);

	// Fixup the modification # to match what was in the image.

	EmAssert (fDBID);
	UInt32	modificationNumber = hdr.modificationNumber;
	Err	err = ::DmSetDatabaseInfo (fCardNo, fDBID, NULL, NULL, NULL, NULL,
				NULL, NULL, &modificationNumber, NULL, NULL, NULL, NULL);

	if (err)
	{
		this->SetResult (err);
		return;
	}

	// Close up and drop our references.

	EmAssert (fOpenID);
	::DmCloseDatabase (fOpenID);

	fOpenID = 0;
//	fDBID = 0;	// Don't zap this, we return it in GetLocalID.

	fState = kInstallDone;
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::HomeBrewInstallCancel
 *
 * DESCRIPTION:	Cancels the installation process using the Database
 *				Manager.  This stage involves merely deleting the
 *				database we were in the middle of creating.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::HomeBrewInstallCancel (void)
{
	this->DeleteCurrentDatabase ();
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmFileImport::ValidateStream
 *
 * DESCRIPTION:	Preflight the installation of certain files.  If the
 *				stream contains a .prc, .pdb, or .pqa file, then make
 *				sure the contents are valid.  Otherwise, the OS might
 *				discover the error and call ErrDisplay.  We can't
 *				recover from that right now, so try to make sure it
 *				doesn't happen.
 *
 *				Note: in general, the EmFileImport class doesn't care
 *				what kind of stream it's using for input.  The stream
 *				could be a disk file, a memory buffer, or even a TCP
 *				socket.  For the most part, we don't care.  However,
 *				*this* function does.  In order to help determine
 *				what the stream contains (one of the files we can
 *				validate, or something else, like a text, vCard, or
 *				vCal file), we look at the file type.  Therefore,
 *				we try casting the base stream into an EmStreamFile.
 *				If that works, then we can check the file type.  If
 *				the cast fails, then we just assume the best and
 *				say the file can be installed.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::ValidateStream (void)
{
	// Only validate resource/record files.  Which, in turn, means
	// that we can only validate EmStreamFiles.  Anything else, well,
	// we hope they're OK.

	EmStreamFile*	fileStream = dynamic_cast <EmStreamFile*> (&fStream);
	if (fileStream == NULL)
		return;

	EmFileRef		fileRef = fileStream->GetFileRef ();
	if (fileRef.IsType (kFileTypePalmApp) ||
		fileRef.IsType (kFileTypePalmDB) ||
		fileRef.IsType (kFileTypePalmQA))
	{
		long		len = fStream.GetLength ();
		StMemory	buffer (len);
		void*		bufferP = buffer.Get ();

		fStream.SetMarker (0, kStreamFromStart);
		fStream.GetBytes (bufferP, len);

		EmAliasDatabaseHdrType<LAS>	hdr (bufferP);

		ErrCode	err = ::PrvValidateDatabase (hdr, len);
		this->SetResult (err);
	}

	// If we're installing something other than a resource/record
	// file, then we can only do that if we're using the Exchange
	// Manager

	else
	{
		if (!fUsingExgMgr)
		{
			this->SetResult (kError_UnknownType);
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmFileImport::DeleteCurrentDatabase
 *
 * DESCRIPTION:	Delete the database currently being installed.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmFileImport::DeleteCurrentDatabase (void)
{
	if (fOpenID)
	{
		::DmCloseDatabase (fOpenID);
		fOpenID = 0;
	}

	if (fDBID)
	{
		::DmDeleteDatabase (fCardNo, fDBID);
		fDBID = 0;
	}
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	PrvDetermineMethod
 *
 * DESCRIPTION:	Return whether or not we should use the Exchange
 *				Manager for installing the file.  Right now, we check
 *				only for existance of the Exchange Manager and whether
 *				or not our library is installed.  In the future, we may
 *				also check for valid file types and user preference
 *				settings.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if we'll be using the ExgMgr.
 *
 ***********************************************************************/

Bool PrvDetermineMethod (EmFileImportMethod method)
{
	return
		(method == kMethodExgMgr) ? true :
		(method == kMethodHomebrew) ? false :
		::PrvCanUseExgMgr ();
}


/***********************************************************************
 *
 * FUNCTION:	PrvCanUseExgMgr
 *
 * DESCRIPTION:	Return whether or not we should use the Exchange
 *				Manager for installing the file.  Right now, we check
 *				only for existance of the Exchange Manager and whether
 *				or not our library is installed.  In the future, we may
 *				also check for valid file types and user preference
 *				settings.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if we'll be using the ExgMgr.
 *
 ***********************************************************************/

Bool PrvCanUseExgMgr (void)
{
#if 0
	return ::PrvHasExgMgr () && ::PrvHostExgLibLoaded ();
#else
	return false;
#endif
}


/***********************************************************************
 *
 * FUNCTION:	PrvHasExgMgr
 *
 * DESCRIPTION:	Return whether or not the Exchange Manager is installed.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if it is.
 *
 ***********************************************************************/

Bool PrvHasExgMgr (void)
{
	return EmLowMem::TrapExists (sysTrapExgInit);
}


/***********************************************************************
 *
 * FUNCTION:	PrvHostExgLibLoaded
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

#if 0
Bool PrvHostExgLibLoaded (void)
{
	// Try finding the library.
	UInt16	libRefNum;
	Err		err = ::SysLibFind (kHostExgLibName, &libRefNum);

	// If we can't find it, try loading it.
	if (err == sysErrLibNotFound)
        err = ::SysLibLoad (sysFileTExgLib, kHostExgLibCreator, &libRefNum);

	if (!err)
	{
		gHostExgLibRefNum = libRefNum;
		return true;
	}

	return false;
}
#endif


/***********************************************************************
 *
 * FUNCTION:	PrvIsResources
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool PrvIsResources (UInt16 attributes)
{
	return (attributes & dmHdrAttrResDB);
}


/***********************************************************************
 *
 * FUNCTION:	PrvIsResources
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool PrvIsResources (const EmAliasDatabaseHdrType<LAS>& hdr)
{
	return ::PrvIsResources (hdr.attributes);
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetEntrySize
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

UInt32 PrvGetEntrySize (const EmAliasDatabaseHdrType<LAS>& hdr)
{
	if (::PrvIsResources (hdr))
		return EmAliasRsrcEntryType<LAS>::GetSize ();

	return EmAliasRecordEntryType<LAS>::GetSize ();
}


/***********************************************************************
 *
 * FUNCTION:	PrvSetResourceTypeIDParameters
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void PrvSetResourceTypeIDParameters (UInt32 resType, UInt16 resID)
{
	char	buffer[20];

	sprintf (buffer, "%c%c%c%c",
					(char) (resType >> 24),
					(char) (resType >> 16),
					(char) (resType >> 8),
					(char) (resType >> 0));
	Errors::SetParameter ("%res_type", buffer);

	Errors::SetParameter ("%res_id", resID);
}


/***********************************************************************
 *
 * FUNCTION:	PrvValidateDatabaseHeader
 *
 * DESCRIPTION:	Validates database header (to be used before any of the
 *				rest of the database is byteswapped or otherwise
 *				processed).
 *
 * PARAMETERS:	hdrP - pointer to database header
 *				bufferSize - size of the buffer holding the database
 *
 * RETURNED:	Err - error code
 *
 ***********************************************************************/

static ErrCode PrvValidateDatabaseHeader(const EmAliasDatabaseHdrType<LAS>& hdr, UInt32 bufferSize)
{
	// Make sure the database name is not too long.

	char*	namePtr	= (char*) hdr.name.GetPtr ();
	size_t	len		= strlen (namePtr);

	if (len >= dmDBNameLength)
	{
		char	databaseName[dmDBNameLength];
		memcpy (databaseName, namePtr, dmDBNameLength);

		databaseName[dmDBNameLength - 4] = '.';
		databaseName[dmDBNameLength - 3] = '.';
		databaseName[dmDBNameLength - 2] = '.';
		databaseName[dmDBNameLength - 1] = '\0';

		Errors::SetParameter ("%database_name", databaseName);
		return kError_BadDB_NameNotNULLTerminated;
	}

	// Check to see that name consists of printable characters

	for (size_t ii = 0; ii < len; ++ii)
	{
		uint8	ch = (uint8) namePtr[ii];

		if (ch < ' ' || ch > 0x7E)
		{
			Errors::SetParameter ("%database_name", namePtr);
			return kError_BadDB_NameNotPrintable;
		}
	}

	// Check that the modification date is not older than the creation date.
	// We don't care about this and neither does the Palm OS, but HotSync does.

	uint32	creationDate = hdr.creationDate;
	uint32	modificationDate = hdr.modificationDate;

	if (creationDate > modificationDate)
	{
		// Just show the problem here, but don't consider it fatal.
		::PrvSetDate ("%cr_date", hdr.creationDate);
		::PrvSetDate ("%mod_date", hdr.modificationDate);
		Errors::SetParameter ("%database_name", namePtr);
		string	msg = Errors::ReplaceParameters (kStr_InconsistentDatabaseDates);
		EmDlg::DoCommonDialog (msg, kDlgFlags_OK);
	}

	// Also check for zero creation dates.
	// We don't care about this and neither does the Palm OS, but HotSync does.

	else if (creationDate == 0)
	{
		// Just show the problem here, but don't consider it fatal.
		Errors::SetParameter ("%database_name", namePtr);
		string	msg = Errors::ReplaceParameters (kStr_NULLDatabaseDate);
		EmDlg::DoCommonDialog (msg, kDlgFlags_OK);
	}

	// Check that the resource/database records headers fit within the space
	// of the buffer.

	UInt32	endOfEntryArray = EmAliasDatabaseHdrType<LAS>::GetSize () +
								::PrvGetEntrySize (hdr) * hdr.recordList.numRecords;

	if (endOfEntryArray > bufferSize)
		return kError_BadDB_FileTooSmall;

	// If we got here, we're okay.

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	PrvValidateEntries
 *
 * DESCRIPTION: Validates entries in a database
 *
 * PARAMETERS:	hdrP - pointer to database header
 *				bufferSize - size of the buffer holding the database
 *
 * RETURND:		Err - error code
 *
 ***********************************************************************/

static ErrCode PrvValidateEntries(const EmAliasDatabaseHdrType<LAS>& hdr, UInt32 bufferSize)
{
	Bool		isResource	= ::PrvIsResources (hdr);

	// Establish the upper and lower ranges for localChunkIDs.	These IDs are
	// offsets from the beginning of the buffer/file, so they can't be larger
	// than the size of the buffer/file, nor can they be smaller than the
	// starting offset of the chunks.

	UInt32		upperRange	= bufferSize;
	UInt32		lowerRange	= EmAliasDatabaseHdrType<LAS>::GetSize () +
								::PrvGetEntrySize (hdr) * hdr.recordList.numRecords;

	// Create an array to keep track of the discovered resource type/ids.
	// This is so we can find duplicate entries.

	typedef pair<UInt32, UInt16>	ResTypeIDPair;
	vector<ResTypeIDPair>	resPairs;

	// nextRecordListID not supported for now.

	if (hdr.recordList.nextRecordListID != 0)
		return kError_BadDB_nextRecordListIDNonZero;

	for (UInt16 recordNumber = 0; recordNumber < hdr.recordList.numRecords; ++recordNumber)
	{
		LocalID 	itsLocalID;
		long		itsSize;	// Use a signed value to detect decreasing LocalIDs

		// Check for negative sizes and get the LocalID.
		// ---------------------------------------------
		// Though the code is similar for resources and for records, we have separate
		// blocks so that we can safely cast to different types to access the localChunkID
		// field.

		if (isResource)
		{
			EmAliasRsrcEntryType<LAS> rsrcEntry (hdr.recordList.resources[recordNumber]);

			// Set the error parameters in case an error occurs.

			::PrvSetResourceTypeIDParameters (rsrcEntry.type, rsrcEntry.id);

			// Check for duplicate resources.

			ResTypeIDPair	thisPair (rsrcEntry.type, rsrcEntry.id);

			if (find (resPairs.begin (), resPairs.end (), thisPair) != resPairs.end ())
				return kError_BadDB_DuplicateResource;

			resPairs.push_back (thisPair);

			// Get the size of the resource.

			if (recordNumber < hdr.recordList.numRecords - 1)
			{
				itsSize = (UInt32) rsrcEntry[1].localChunkID;
				itsSize -= (UInt32) rsrcEntry.localChunkID;
			}
			else
				itsSize = bufferSize - (UInt32) rsrcEntry.localChunkID;

			// Set the error parameters in case an error occurs.

			Errors::SetParameter ("%res_size", itsSize);

			// Check for negative sizes (zero is NOT okay for resources).

			if (itsSize <= 0)
				return kError_BadDB_ResourceTooSmall;

			itsLocalID = rsrcEntry.localChunkID;
		}
		else
		{
			EmAliasRecordEntryType<LAS> recordEntry (hdr.recordList.records[recordNumber]);

			// Set the error parameters in case an error occurs.

			Errors::SetParameter ("%record_number", recordNumber);

			if (recordNumber < hdr.recordList.numRecords - 1)
			{
				itsSize = (UInt32) recordEntry[1].localChunkID;
				itsSize -= (UInt32) recordEntry.localChunkID;
			}
			else
				itsSize = bufferSize - (UInt32) recordEntry.localChunkID;

			// Set the error parameters in case an error occurs.

			Errors::SetParameter ("%res_size", itsSize);

			// Check for negative sizes (zero is okay for records).

			if (itsSize < 0)
				return kError_BadDB_RecordTooSmall;

			itsLocalID = recordEntry.localChunkID;
		}

		// Test that the beginning and ending of the chunk are in range.

		if (itsLocalID < lowerRange || itsLocalID + itsSize > upperRange)
			return isResource
				? kError_BadDB_ResourceOutOfRange
				: kError_BadDB_RecordOutOfRange;
	}

	// If we got here, we're okay

	return errNone;
}


/***********************************************************************
 *
 * FUNCTION:	PrvValidateDatabase
 *
 * DESCRIPTION: 
 *
 * PARAMETERS:	hdrP - pointer to database header
 *				bufferSize - size of the buffer holding the database
 *
 * RETURNED:	Err - error code
 *
 ***********************************************************************/

static ErrCode PrvValidateDatabase (const EmAliasDatabaseHdrType<LAS>& hdr, UInt32 size)
{
	ErrCode err;

	// Make sure the file has enough of a header to validate.

	if (size < hdr.GetSize ())
	{
		err = kError_BadDB_FileTooSmall;
		goto Exit;
	}

	// Validate the header.

	err = ::PrvValidateDatabaseHeader (hdr, size);
	if (err != errNone)
		goto Exit;

	// Check out the record entries; make sure they're okay

	err = ::PrvValidateEntries (hdr, size);

Exit:
	if (!err)
		Errors::ClearAllParameters ();

	return err;
}

void PrvSetDate (const char* varName, uint32 seconds)
{
// this table tells you how many days since the last 'daysInFourYears' boundary.
// That is, the first 12 entries give you the days in previous months for a leap year,
// e.g. table entry 3 (March) is 60 because there are 31 days in January and 29 in February
// each successive 12 entries give you the days in previous months for that year plus the
// days in previous years.  That allows you to do just one table lookup with (year % 4) + month
// to find out how many days have gone by since the start of the previous leap year.
// (See DateToDays for how this is used.)
//
// There's one extra entry at the end, so that this gives you the days in a given month:
// 		daysInPrevMonths[(year%4)*12+month] - daysInPrevMonth[(year%4)*12+month-1]
//
// It's OK to use just one table for all this, because over our valid date range
// (1904 - 2032) all years divisible by 4 are leap years.

static const UInt16 daysInPrevMonths[] =
{
//		 +31		 +29		 +31		 +30		 +31			+30
	0, 			31,			60,			91,			121,		152,		// 1904, 1908, ...

//		 +31		 +31		 +30		 +31		 +30			+31
	182,		213,		244,		274,		305,		335,		// (leap years!)
												
//		   +31		 +28		 +31		 +30		 +31		+30
	366+0,		366+31,		366+59,		366+90,		366+120,	366+151,	// 1905, 1909, ...

//			+31		 +31		 +30		 +31		 +30		+31
	366+181,	366+212,	366+243, 	366+273,	366+304,	366+334,
													
	731+0,		731+31,		731+59,		731+90,		731+120,	731+151,	// 1906, 1910, ...
	731+181,	731+212,	731+243, 	731+273,	731+304,	731+334,
													
	1096+0,		1096+31,	1096+59,	1096+90,	1096+120,	1096+151,// 1907, 1911, ...
	1096+181,	1096+212,	1096+243,	1096+273,	1096+304,	1096+334,
	
	1461
};

	UInt32			days;
	UInt16			month;
	DateTimeType	dateTime;

	// Now convert from seconds to days.
	days = seconds / daysInSeconds;
	seconds = seconds % daysInSeconds;
	dateTime.weekDay = (days + 5) % daysInWeek;

	// get year to nearest leap year
	dateTime.year = firstYear + (days / daysInFourYears) * 4;

	// now figure out month/year within 4-year range
	days %= daysInFourYears;
	month = 0;
	while (days >= daysInPrevMonths[month])
		month++;
	month--;

	dateTime.year += month / monthsInYear;
	dateTime.month = (month % monthsInYear) + 1;
	dateTime.day = days - daysInPrevMonths[month] + 1;

	// now calc the time 
	dateTime.hour = seconds / hoursInSeconds;
	seconds = seconds % hoursInSeconds;
	dateTime.minute = seconds / minutesInSeconds;
	seconds = seconds % minutesInSeconds;
	dateTime.second = seconds;

	// Convert this into  text.

	char		buffer[200];
	struct tm	time;

	time.tm_sec		= dateTime.second;
	time.tm_min		= dateTime.minute;
	time.tm_hour	= dateTime.hour;
	time.tm_mday	= dateTime.day - 1;
	time.tm_mon		= dateTime.month - 1;
	time.tm_year	= dateTime.year - 1900;
	time.tm_wday	= dateTime.weekDay;
	time.tm_yday	= 0;
	time.tm_isdst	= -1;

	strftime (buffer, 200, "%B %d, %Y, %I:%M:%S %p", &time);
//	strftime (buffer, 200, "%c", &time);

	Errors::SetParameter (varName, buffer);
}


void PrvSetExgMgr (void* mgr)
{
	if (gHostExgLibRefNum)
	{
		emuptr	tblP = (emuptr) ::SysLibTblEntry (gHostExgLibRefNum);
		if (tblP)
		{
			EmAliasSysLibTblEntryType<PAS>	tbl (tblP);
			tbl.globalsP = (emuptr) mgr;
		}
	}
}

const uint8 kHostExgLib[] =
{
	0x48, 0x6F, 0x73, 0x74, 0x45, 0x78, 0x67, 0x4C, 0x69, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x00, 0x01, 0xB5, 0x85, 0x65, 0x02, 0xB5, 0x85, 0x65, 0x02, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0x78, 0x67, 0x6C,
	0x48, 0x45, 0x78, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x6C, 0x69,
	0x62, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00, 0x48, 0x7A, 0x00, 0x04, 0x06, 0x97,
	0x00, 0x00, 0x00, 0x06, 0x4E, 0x75, 0x2F, 0x0A, 0x2F, 0x03, 0x59, 0x4F, 0x24, 0x6F, 0x00, 0x12,
	0x48, 0x57, 0x3F, 0x3C, 0x00, 0x01, 0x2F, 0x3C, 0x70, 0x73, 0x79, 0x73, 0x4E, 0x4F, 0xA2, 0x7B,
	0x36, 0x00, 0x4F, 0xEF, 0x00, 0x0A, 0x66, 0x08, 0x0C, 0x97, 0x03, 0x00, 0x00, 0x00, 0x64, 0x06,
	0x30, 0x3C, 0x15, 0x02, 0x60, 0x0C, 0x4E, 0xBA, 0x02, 0x16, 0x24, 0x88, 0x42, 0xAA, 0x00, 0x04,
	0x70, 0x00, 0x58, 0x4F, 0x26, 0x1F, 0x24, 0x5F, 0x4E, 0x75, 0x3F, 0x2F, 0x00, 0x04, 0x3F, 0x3C,
	0x05, 0x80, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x54, 0x4F, 0x4E, 0x75, 0x3F, 0x2F, 0x00, 0x04,
	0x3F, 0x3C, 0x05, 0x81, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x54, 0x4F, 0x4E, 0x75, 0x3F, 0x2F,
	0x00, 0x04, 0x3F, 0x3C, 0x05, 0x83, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x54, 0x4F, 0x4E, 0x75,
	0x3F, 0x2F, 0x00, 0x04, 0x3F, 0x3C, 0x05, 0x82, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x54, 0x4F,
	0x4E, 0x75, 0x2F, 0x0A, 0x2F, 0x03, 0x59, 0x4F, 0x36, 0x2F, 0x00, 0x10, 0x24, 0x6F, 0x00, 0x12,
	0x0C, 0x52, 0x00, 0x04, 0x66, 0x10, 0x30, 0x2A, 0x00, 0x0C, 0x02, 0x40, 0x04, 0x00, 0x67, 0x06,
	0xB6, 0x6A, 0x00, 0x0A, 0x67, 0x06, 0x70, 0x00, 0x60, 0x00, 0x00, 0x9C, 0x0C, 0x6A, 0x01, 0xFC,
	0x00, 0x08, 0x66, 0x00, 0x00, 0x82, 0x3E, 0xBC, 0x00, 0x3C, 0x70, 0x00, 0x30, 0x17, 0x2F, 0x00,
	0x4E, 0x4F, 0xA0, 0x13, 0x24, 0x48, 0x20, 0x0A, 0x58, 0x4F, 0x66, 0x04, 0x70, 0x00, 0x60, 0x76,
	0x42, 0x67, 0x2F, 0x0A, 0x4E, 0x4F, 0xA0, 0x1B, 0x48, 0x6F, 0x00, 0x06, 0x2F, 0x0A, 0x3F, 0x3C,
	0x80, 0x00, 0x3F, 0x03, 0x4E, 0x4F, 0xA8, 0x0D, 0x3F, 0x40, 0x00, 0x14, 0x4A, 0x6F, 0x00, 0x14,
	0x4F, 0xEF, 0x00, 0x12, 0x66, 0x0C, 0x2F, 0x0A, 0x4E, 0x4F, 0xA3, 0x10, 0x3F, 0x40, 0x00, 0x06,
	0x58, 0x4F, 0x48, 0x57, 0x2F, 0x0A, 0x3F, 0x3C, 0x80, 0x01, 0x3F, 0x03, 0x4E, 0x4F, 0xA8, 0x0D,
	0x42, 0xA7, 0x48, 0x6F, 0x00, 0x12, 0x3F, 0x3C, 0x80, 0x02, 0x3F, 0x03, 0x4E, 0x4F, 0xA8, 0x0D,
	0x2F, 0x0A, 0x4E, 0x4F, 0xA0, 0x12, 0x4A, 0x6F, 0x00, 0x1E, 0x57, 0xC0, 0x44, 0x00, 0x48, 0x80,
	0x4F, 0xEF, 0x00, 0x20, 0x60, 0x12, 0x2F, 0x0A, 0x3F, 0x03, 0x3F, 0x3C, 0x05, 0x84, 0x4E, 0x4F,
	0xA3, 0x44, 0x54, 0x4F, 0x5C, 0x4F, 0x58, 0x4F, 0x26, 0x1F, 0x24, 0x5F, 0x4E, 0x75, 0x2F, 0x2F,
	0x00, 0x06, 0x3F, 0x2F, 0x00, 0x08, 0x3F, 0x3C, 0x05, 0x85, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F,
	0x5C, 0x4F, 0x4E, 0x75, 0x2F, 0x2F, 0x00, 0x06, 0x3F, 0x2F, 0x00, 0x08, 0x3F, 0x3C, 0x05, 0x86,
	0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x5C, 0x4F, 0x4E, 0x75, 0x3F, 0x2F, 0x00, 0x0A, 0x2F, 0x2F,
	0x00, 0x08, 0x3F, 0x2F, 0x00, 0x0A, 0x3F, 0x3C, 0x05, 0x87, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F,
	0x50, 0x4F, 0x4E, 0x75, 0x2F, 0x2F, 0x00, 0x06, 0x3F, 0x2F, 0x00, 0x08, 0x3F, 0x3C, 0x05, 0x89,
	0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x5C, 0x4F, 0x4E, 0x75, 0x2F, 0x2F, 0x00, 0x06, 0x3F, 0x2F,
	0x00, 0x08, 0x3F, 0x3C, 0x05, 0x88, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x5C, 0x4F, 0x4E, 0x75,
	0x2F, 0x2F, 0x00, 0x12, 0x2F, 0x2F, 0x00, 0x12, 0x2F, 0x2F, 0x00, 0x12, 0x2F, 0x2F, 0x00, 0x12,
	0x3F, 0x2F, 0x00, 0x14, 0x3F, 0x3C, 0x05, 0x8A, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x4F, 0xEF,
	0x00, 0x12, 0x4E, 0x75, 0x2F, 0x2F, 0x00, 0x12, 0x2F, 0x2F, 0x00, 0x12, 0x2F, 0x2F, 0x00, 0x12,
	0x2F, 0x2F, 0x00, 0x12, 0x3F, 0x2F, 0x00, 0x14, 0x3F, 0x3C, 0x05, 0x8B, 0x4E, 0x4F, 0xA3, 0x44,
	0x54, 0x4F, 0x4F, 0xEF, 0x00, 0x12, 0x4E, 0x75, 0x2F, 0x2F, 0x00, 0x0C, 0x2F, 0x2F, 0x00, 0x0C,
	0x3F, 0x2F, 0x00, 0x0E, 0x3F, 0x2F, 0x00, 0x0E, 0x3F, 0x3C, 0x05, 0x8C, 0x4E, 0x4F, 0xA3, 0x44,
	0x54, 0x4F, 0x4F, 0xEF, 0x00, 0x0C, 0x4E, 0x75, 0x2F, 0x2F, 0x00, 0x06, 0x3F, 0x2F, 0x00, 0x08,
	0x3F, 0x3C, 0x05, 0x8D, 0x4E, 0x4F, 0xA3, 0x44, 0x54, 0x4F, 0x5C, 0x4F, 0x4E, 0x75, 0x41, 0xFA,
	0x00, 0x04, 0x4E, 0x75, 0x00, 0x1E, 0x00, 0x2A, 0x00, 0x2E, 0x00, 0x32, 0x00, 0x36, 0x00, 0x3A,
	0x00, 0x3E, 0x00, 0x42, 0x00, 0x46, 0x00, 0x4A, 0x00, 0x4E, 0x00, 0x52, 0x00, 0x56, 0x00, 0x5A,
	0x00, 0x5E, 0x48, 0x6F, 0x73, 0x74, 0x45, 0x78, 0x67, 0x4C, 0x69, 0x62, 0x00, 0x00, 0x4E, 0xFA,
	0xFD, 0xCA, 0x4E, 0xFA, 0xFD, 0xD8, 0x4E, 0xFA, 0xFD, 0xF8, 0x4E, 0xFA, 0xFD, 0xE2, 0x4E, 0xFA,
	0xFE, 0x02, 0x4E, 0xFA, 0xFE, 0xCA, 0x4E, 0xFA, 0xFE, 0xDC, 0x4E, 0xFA, 0xFE, 0xEE, 0x4E, 0xFA,
	0xFF, 0x1A, 0x4E, 0xFA, 0xFF, 0x00, 0x4E, 0xFA, 0xFF, 0x28, 0x4E, 0xFA, 0xFF, 0x48, 0x4E, 0xFA,
	0xFF, 0x68, 0x4E, 0xFA, 0xFF, 0x84
};

const int kHostExgLibSize = sizeof (kHostExgLib);

