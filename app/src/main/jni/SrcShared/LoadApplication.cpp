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
#include "LoadApplication.h"

#include "EmErrCodes.h"			// kError_OutOfMemory
#include "EmMemory.h"			// EmMem_memcpy
#include "EmPalmStructs.h"		// RecordEntryType, RsrcEntryType, etc.
#include "EmStreamFile.h"		// EmStreamFile
#include "ErrorHandling.h"		// Errors::ThrowIfPalmError
#include "Miscellaneous.h"		// StMemory
#include "ROMStubs.h"			// DmFindDatabase, DmGetLastErr, DmDatabaseInfo, DmOpenDatabase, ...
#include "Strings.r.h"			// kStr_ values


static Bool PrvIsResources (UInt16 attributes)
{
	return (attributes & dmHdrAttrResDB);
}


/************************************************************
 * Export a resource or record database as a normal data file in 
 *	Pilot format.
 *************************************************************/

static void PrvMyShlExportAsPilotFile(EmStreamFile& fh, UInt16 cardNo, const char* nameP)
{
	DmOpenRef	dbP = 0;
	Err 		err;
	UInt16		numRecords;
	int 		size;
	long		offset;
	int 		i;

	try
	{
		//------------------------------------------------------------
		// Locate the Database and see what kind of database it is
		//------------------------------------------------------------

		LocalID	dbID;
		dbID = ::DmFindDatabase (cardNo, (Char*) nameP);
		if (!dbID)
			Errors::ThrowIfPalmError (::DmGetLastErr ());

		UInt16		attributes, version;
		UInt32		crDate, modDate, bckUpDate;
		UInt32		type, creator;
		UInt32		modNum;
		LocalID		appInfoID, sortInfoID;
		const int	kGapSize = 2;

		err = ::DmDatabaseInfo (cardNo, dbID, 0,
					&attributes, &version, &crDate,
					&modDate, &bckUpDate,
					&modNum, &appInfoID,
					&sortInfoID, &type,
					&creator);
		Errors::ThrowIfPalmError(err);

		dbP = ::DmOpenDatabase (cardNo, dbID, dmModeReadOnly);
		if (!dbP)
			Errors::ThrowIfPalmError (::DmGetLastErr ());
		
		//------------------------------------------------------------
		// Write out a resource database
		//------------------------------------------------------------
		if (::PrvIsResources (attributes))
		{
			DmResType	resType;
			DmResID		resID;
			UInt32		resSize;

			numRecords = ::DmNumRecords(dbP);

			size = EmAliasDatabaseHdrType<LAS>::GetSize () + numRecords * EmAliasRsrcEntryType<LAS>::GetSize () + kGapSize;
			StMemory	outP (size, true);

			// Fill in header
			EmAliasDatabaseHdrType<LAS>	hdr (outP.Get ());

			strcpy ((char*) hdr.name.GetPtr (), nameP);
			hdr.attributes			= attributes;
			hdr.version				= version;
			hdr.creationDate		= crDate;
			hdr.modificationDate	= modDate;
			hdr.lastBackupDate		= bckUpDate;
			hdr.modificationNumber	= modNum;
			hdr.appInfoID			= 0;
			hdr.sortInfoID			= 0;
			hdr.type				= type;
			hdr.creator				= creator;

			hdr.recordList.nextRecordListID	= 0;
			hdr.recordList.numRecords		= numRecords;

			// Get the size of the appInfo and sort Info if they exist
			offset = size;
			MemHandle	appInfoH=0, sortInfoH=0;
			UInt32		appInfoSize=0, sortInfoSize=0;
			if (appInfoID)
			{
				hdr.appInfoID = offset;
				appInfoH = (MemHandle) ::MemLocalIDToGlobal (appInfoID, cardNo);
				if (!appInfoH)
				{
					Errors::ThrowIfPalmError (-1);
				}
				appInfoSize = ::MemHandleSize (appInfoH);
				offset += appInfoSize;
			}

			if (sortInfoID)
			{
				hdr.sortInfoID = offset;
				sortInfoH = (MemHandle) ::MemLocalIDToGlobal (sortInfoID, cardNo);
				if (!sortInfoH)
				{
					Errors::ThrowIfPalmError (-1);
				}
				sortInfoSize = ::MemHandleSize (sortInfoH);
				offset += sortInfoSize;
			}


			// Fill in the info on each resource into the header
			offset = size;
			for (i=0; i < numRecords; i++)
			{
				EmAliasRsrcEntryType<LAS>	entry (hdr.recordList.resources[i]);

				::DmResourceInfo (dbP, i, &resType, &resID, NULL);
				MemHandle	resH = ::DmGetResourceIndex (dbP, i);
				if (resH)
				{
					resSize = ::MemHandleSize (resH);
					::DmReleaseResource (resH);
				}
				else
				{
					resSize = 0;
				}

				entry.type			= resType;
				entry.id			= resID;
				entry.localChunkID	= offset;
				
				offset += resSize;
			}
			
			// Clear out gap
			if (kGapSize > 0)
				memset (outP.Get() + (size - kGapSize), 0, kGapSize);
			
			// Write out  entry table
			fh.PutBytes (outP.Get (), size);
			
			// Write out the appInfo followed by sortInfo, if they exist
			if (appInfoID && appInfoSize)
			{
				UInt32		srcP;
				StMemory	outP (appInfoSize);
				srcP = (UInt32) ::MemHandleLock (appInfoH);
				EmMem_memcpy ((void*) outP.Get(), srcP, appInfoSize);
				::MemPtrUnlock ((MemPtr) srcP);
				fh.PutBytes (outP.Get (), appInfoSize);
			}

			if (sortInfoID && sortInfoSize)
			{
				UInt32		srcP;
				StMemory	outP (sortInfoSize);
				srcP = (UInt32) ::MemHandleLock (sortInfoH);
				EmMem_memcpy ((void*) outP.Get(), srcP, sortInfoSize);
				::MemPtrUnlock ((MemPtr) srcP);
				fh.PutBytes (outP.Get (), sortInfoSize);
			}

			// Write out each resource
			for (i=0; i < numRecords; i++)
			{
				MemHandle	srcResH;
				LocalID		resChunkID;
				
				::DmResourceInfo (dbP, i, &resType, &resID, &resChunkID);

				if (resChunkID)
				{
					UInt32	srcP;
					srcResH = ::DmGetResourceIndex (dbP, i);
					if (!srcResH)
					{
						Errors::ThrowIfPalmError (-1);
					}
					resSize = ::MemHandleSize (srcResH);

					StMemory	outP (resSize);
					srcP = (UInt32) ::MemHandleLock (srcResH);
					EmMem_memcpy ((void*) outP.Get (), srcP, resSize);
					::MemPtrUnlock ((MemPtr) srcP);
					fh.PutBytes (outP.Get (), resSize);

					::DmReleaseResource (srcResH);
				}
			}
		} // Resource database

		//------------------------------------------------------------
		// Write out a records database
		//------------------------------------------------------------
		else
		{
			UInt16		attr;
			UInt32		uniqueID;
			MemHandle	srcH;

			numRecords = ::DmNumRecords (dbP);

			size = EmAliasDatabaseHdrType<LAS>::GetSize () + numRecords * EmAliasRecordEntryType<LAS>::GetSize () + kGapSize;
			StMemory	outP(size);

			// Fill in header
			EmAliasDatabaseHdrType<LAS>	hdr (outP.Get ());

			strcpy ((char*) hdr.name.GetPtr (), nameP);
			hdr.attributes			= attributes;
			hdr.version				= version;
			hdr.creationDate		= crDate;
			hdr.modificationDate	= modDate;
			hdr.lastBackupDate		= bckUpDate;
			hdr.modificationNumber	= modNum;
			hdr.appInfoID			= 0;
			hdr.sortInfoID			= 0;
			hdr.type				= type;
			hdr.creator				= creator;

			hdr.recordList.nextRecordListID	= 0;
			hdr.recordList.numRecords		= numRecords;


			// Get the size of the appInfo and sort Info if they exist
			offset = size;
			MemHandle	appInfoH=0, sortInfoH=0;
			UInt32		appInfoSize=0, sortInfoSize=0;
			if (appInfoID)
			{
				hdr.appInfoID = offset;
				appInfoH = (MemHandle) ::MemLocalIDToGlobal (appInfoID, cardNo);
				if (!appInfoH)
				{
					Errors::ThrowIfPalmError (-1);
				}
				appInfoSize = ::MemHandleSize (appInfoH);
				offset += appInfoSize;
			}

			if (sortInfoID)
			{
				hdr.sortInfoID = offset;
				sortInfoH = (MemHandle) ::MemLocalIDToGlobal (sortInfoID, cardNo);
				if (!sortInfoH)
				{
					Errors::ThrowIfPalmError (-1);
				}
				sortInfoSize = ::MemHandleSize (sortInfoH);
				offset += sortInfoSize;
			}


			// Fill in the info on each resource into the header
			for (i=0; i < numRecords; i++)
			{
				EmAliasRecordEntryType<LAS>	entry (hdr.recordList.records[i]);

				err = ::DmRecordInfo (dbP, i, &attr, &uniqueID, 0);
				if (err)
				{
					Errors::ThrowIfPalmError (-1);
				}

				srcH = ::DmQueryRecord (dbP, i);

				entry.localChunkID	= offset;
				entry.attributes	= attr;
				entry.uniqueID[0]	= (uniqueID >> 16) & 0x00FF;
				entry.uniqueID[1]	= (uniqueID >> 8) & 0x00FF;
				entry.uniqueID[2]	= uniqueID	& 0x00FF;

				if (srcH)
					offset += ::MemHandleSize (srcH);
			}

			// Clear out gap
			if (kGapSize > 0)
				memset (outP.Get() + (size - kGapSize), 0, kGapSize);

			// Write out  entry table
			fh.PutBytes (outP.Get (), size);

			// Write out the appInfo followed by sortInfo, if they exist
			if (appInfoID && appInfoSize)
			{
				UInt32		srcP;
				StMemory	outP (appInfoSize);
				srcP = (UInt32) ::MemHandleLock (appInfoH);
				EmMem_memcpy ((void*) outP.Get(), srcP, appInfoSize);
				::MemPtrUnlock ((MemPtr) srcP);
				fh.PutBytes (outP.Get (), appInfoSize);
			}

			if (sortInfoID && sortInfoSize)
			{
				UInt32		srcP;
				StMemory	outP (sortInfoSize);
				srcP = (UInt32) ::MemHandleLock (sortInfoH);
				EmMem_memcpy ((void*) outP.Get(), srcP, sortInfoSize);
				::MemPtrUnlock ((MemPtr) srcP);
				fh.PutBytes (outP.Get (), sortInfoSize);
			}

			// Write out each record
			for (i=0; i<numRecords; i++)
			{
				UInt32		recSize;

				err = ::DmRecordInfo (dbP, i, &attr, &uniqueID, 0);
				if (err)
				{
					Errors::ThrowIfPalmError (-1);
				}

				srcH = ::DmQueryRecord (dbP, i);

				if (srcH)
				{
					UInt32	srcP;

					recSize = ::MemHandleSize(srcH);
					StMemory	outP (recSize);
					srcP = (UInt32) ::MemHandleLock (srcH);
					EmMem_memcpy ((void*) outP.Get(), srcP, recSize);
					::MemPtrUnlock ((MemPtr) srcP);
					fh.PutBytes (outP.Get (), recSize);
				}
			}
		}

		// Clean up
		::DmCloseDatabase (dbP);
	}
	catch (...)
	{
		if (dbP)
			::DmCloseDatabase (dbP);

		throw;
	}
}


// ---------------------------------------------------------------------------
//		¥ SavePalmFile
// ---------------------------------------------------------------------------
// Saves a Palm OS program or database file to a file.

void SavePalmFile (EmStreamFile& appFile, UInt16 cardNo, const char* databaseName)
{
	::PrvMyShlExportAsPilotFile (appFile, cardNo, databaseName);
}
