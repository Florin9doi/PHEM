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
#include "ROMStubs.h"

#include "EmBankMapped.h"		// EmBankMapped::GetEmulatedAddress
#include "EmSubroutine.h"		// EmSubroutine
#include "Marshal.h"			// CALLER_PUT_PARAM_VAL
#include "Miscellaneous.h"		// StMemoryMapper


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Clipboard Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	trap patches that keep the Palm Clipboard in sync with the host clipboard.
// --------------------

void ClipboardAddItem (const ClipboardFormatType format, const void* ptr, UInt16 length)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "const ClipboardFormatType format, const void* ptr, UInt16 length");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (ClipboardFormatType, format);
	CALLER_PUT_PARAM_PTR (void, ptr, length, Marshal::kInput);
	CALLER_PUT_PARAM_VAL (UInt16, length);

	// Call the function.
	sub.Call (sysTrapClipboardAddItem);

	// Write back any "by ref" parameters.

	// Return the result.
}

// --------------------
// Called:
//
//	*	trap patches that keep the Palm Clipboard in sync with the host clipboard.
// --------------------

MemHandle ClipboardGetItem (const ClipboardFormatType format, UInt16* length)
{
	// Prepare the stack.
	CALLER_SETUP ("MemHandle", "const ClipboardFormatType format, UInt16* length");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (ClipboardFormatType, format);
	CALLER_PUT_PARAM_REF (UInt16, length, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapClipboardGetItem);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (length);

	// Return the result.
	RETURN_RESULT_PTR (MemHandle);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Control Manager functions
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//		¥ CtlGetLabel, used in Minimization
// ---------------------------------------------------------------------------

const Char* CtlGetLabel (const ControlType *controlP)
{
	// Prepare the stack.
	CALLER_SETUP ("const Char *", "const ControlType *controlP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (ControlType*, controlP);

	// Call the function.
	sub.Call (sysTrapCtlGetLabel);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (const Char*);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Desktop Link Server functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	by SetHotSyncUserName when the user name needs to be set (during
//		boot-up, after a .psf file has been loaded, and after the preferences
//		have changed.
// --------------------

Err DlkDispatchRequest (DlkServerSessionPtr sessP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "DlkServerSessionType* sessP");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (DlkServerSessionType, sessP, Marshal::kInput);

	// Call the function.
	sub.Call (sysTrapDlkDispatchRequest);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called
//
//	*	in Patches::PostLoad to get the user name so that it
//		can be used to establish the preference setting.
// --------------------

Err DlkGetSyncInfo (UInt32* succSyncDateP, UInt32* lastSyncDateP,
			DlkSyncStateType* syncStateP, Char* nameBufP,
			Char* logBufP, Int32* logLenP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
		"UInt32* succSyncDateP, UInt32* lastSyncDateP,"
		"DlkSyncStateType* syncStateP, Char* nameBufP,"
		"Char* logBufP, Int32* logLenP");

	// Set the parameters.
	Int32	logLen = logLenP ? *logLenP : 0;

	CALLER_PUT_PARAM_REF (UInt32, succSyncDateP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, lastSyncDateP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (DlkSyncStateType, syncStateP, Marshal::kInOut);
	CALLER_PUT_PARAM_PTR (Char, nameBufP, dlkUserNameBufSize, Marshal::kInOut);
	CALLER_PUT_PARAM_PTR (Char, logBufP, logLen, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (Int32, logLenP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapDlkGetSyncInfo);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (succSyncDateP);
	CALLER_GET_PARAM_REF (lastSyncDateP);
	CALLER_GET_PARAM_REF (syncStateP);
	CALLER_GET_PARAM_REF (logLenP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Data Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	from My_DmCreateDatabaseFromImage after a database was successfully
//		installed, or after an error occurs (different code paths).  This
//		function is called any time a .prc, .pdb, or .pqa file is installed.
//
//	*	from My_ShlExportAsPilotFile (same comments)
//
//	*	from AppGetExtraInfo after opening a resource database to get its name.
//		This function is called any time a list of databases is needed (for
//		example, in the New Gremlin and Export Database dialogs).
//
//	*	while booting up (InstallCalibrationInfo) or starting a Gremlin
//		(ResetCalibrationInfo) as part of the process of setting the
//		calibration info to an identity state.
// --------------------

Err DmCloseDatabase (DmOpenRef dbR)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "DmOpenRef dbR");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbR);

	// Call the function.
	sub.Call (sysTrapDmCloseDatabase);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called
//
//	*	from My_DmCreateDatabaseFromImage.  This function is called any
//		time a .prc, .pdb, or .pqa file is installed.
// --------------------

Err DmCreateDatabase (UInt16 cardNo, const Char * const nameP, 
					UInt32 creator, UInt32 type, Boolean resDB)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt16 cardNo, const Char * const nameP, "
					"UInt32 creator, UInt32 type, Boolean resDB");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_STR (Char, nameP);
	CALLER_PUT_PARAM_VAL (UInt32, creator);
	CALLER_PUT_PARAM_VAL (UInt32, type);
	CALLER_PUT_PARAM_VAL (Boolean, resDB);

	// Call the function.
	sub.Call (sysTrapDmCreateDatabase);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	when exporting a database (first to generate the list of databases
//		that can be exported, then again to later determine if the
//		database is a resource database or not).
//
//	*	when we need to switch to another application.  Mostly called when
//		starting a Gremlin on a particular application.
//
//	*	after an application has been launched, in order to collect information
//		on that application for later error reporting.
// --------------------

Err DmDatabaseInfo (UInt16 cardNo, LocalID	dbID, Char* nameP,
					UInt16* attributesP, UInt16* versionP, UInt32* crDateP,
					UInt32* modDateP, UInt32* bckUpDateP,
					UInt32* modNumP, LocalID* appInfoIDP,
					LocalID* sortInfoIDP, UInt32* typeP,
					UInt32* creatorP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
					"UInt16 cardNo, LocalID	dbID, Char* nameP,"
					"UInt16* attributesP, UInt16* versionP, UInt32* crDateP,"
					"UInt32* modDateP, UInt32* bckUpDateP,"
					"UInt32* modNumP, LocalID* appInfoIDP,"
					"LocalID* sortInfoIDP, UInt32* typeP,"
					"UInt32* creatorP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_VAL (LocalID, dbID);
	CALLER_PUT_PARAM_PTR (Char, nameP, dmDBNameLength, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt16, attributesP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt16, versionP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, crDateP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, modDateP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, bckUpDateP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, modNumP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (LocalID, appInfoIDP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (LocalID, sortInfoIDP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, typeP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, creatorP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapDmDatabaseInfo);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (attributesP);
	CALLER_GET_PARAM_REF (versionP);
	CALLER_GET_PARAM_REF (crDateP);
	CALLER_GET_PARAM_REF (modDateP);
	CALLER_GET_PARAM_REF (bckUpDateP);
	CALLER_GET_PARAM_REF (modNumP);
	CALLER_GET_PARAM_REF (appInfoIDP);
	CALLER_GET_PARAM_REF (sortInfoIDP);
	CALLER_GET_PARAM_REF (typeP);
	CALLER_GET_PARAM_REF (creatorP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	from LoadPalmFile to delete a previous version of a database
//		that we're trying to install.
//
//	*	from My_DmCreateDatabaseFromImage if the attempt to install a
//		database fails.
// --------------------

Err DmDeleteDatabase (UInt16 cardNo, LocalID dbID)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt16 cardNo, LocalID dbID");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_VAL (LocalID, dbID);

	// Call the function.
	sub.Call (sysTrapDmDeleteDatabase);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by the application install code (My_DmCreateDatabaseFromImage)
//		in order to get the dbID of the database it just created.
//
//	*	by the application install code (LoadPalmFile) to determine
//		if there's a previous instance of the application that needs
//		to be deleted.
//
//	*	by the application export code (My_ShlExportAsPilotFile) to find
//		the database to be exported.
//
//	*	by code that switches to an application installed from the
//		AutoRun directory.
// --------------------

LocalID DmFindDatabase (UInt16 cardNo, const Char* nameP)
{
	// Prepare the stack.
	CALLER_SETUP ("LocalID", "UInt16 cardNo, const Char* nameP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_STR (Char, nameP);

	// Call the function.
	sub.Call (sysTrapDmFindDatabase);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (LocalID);
}

// --------------------
// Called:
//
//	*	by AppGetExtraInfo to get the 'tAIN' resource.
//
//	*	by CollectCurrentAppInfo to get the 'tAIN' and 'tver' resources.
// --------------------

MemHandle DmGet1Resource (DmResType type, DmResID id)
{
	// Prepare the stack.
	CALLER_SETUP ("MemHandle", "DmResType type, DmResID id");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmResType, type);
	CALLER_PUT_PARAM_VAL (DmResID, id);

	// Call the function.
	sub.Call (sysTrapDmGet1Resource);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemHandle);
}

// --------------------
// Called:
//
//	*	by GetDatabases when iterating over all the databases in the system.
//		Called when generating a list of databases to display (e.g.,
//		in the New Gremlin and Export Database dialogs).
// --------------------

LocalID DmGetDatabase (UInt16 cardNo, UInt16 index)
{
	// Prepare the stack.
	CALLER_SETUP ("LocalID", "UInt16 cardNo, UInt16 index");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_VAL (UInt16, index);

	// Call the function.
	sub.Call (sysTrapDmGetDatabase);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (LocalID);
}

// --------------------
// Called:
//
//	*	by the application export code (My_ShlExportAsPilotFile) when failing
//		to find or open the requested database.
//
//	*	AppGetExtraInfo when failing to open an application database in
//		order to get its name.
// --------------------

Err DmGetLastErr (void)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapDmGetLastErr);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by Gremlins (Gremlins::New) when it needs to find the application
//		responsible for a "runnable" database.
//
//	*	same comments for Patches::SwitchToApp.
// --------------------

Err	DmGetNextDatabaseByTypeCreator (Boolean newSearch, DmSearchStatePtr stateInfoP,
			 		UInt32 type, UInt32 creator, Boolean onlyLatestVers, 
			 		UInt16* cardNoP, LocalID* dbIDP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
		"Boolean newSearch, DmSearchStatePtr stateInfoP, "
		"UInt32 type, UInt32 creator, Boolean onlyLatestVers, "
		"UInt16* cardNoP, LocalID* dbIDP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (Boolean, newSearch);
	CALLER_PUT_PARAM_REF (DmSearchStateType, stateInfoP, Marshal::kInOut);
	CALLER_PUT_PARAM_VAL (UInt32, type);
	CALLER_PUT_PARAM_VAL (UInt32, creator);
	CALLER_PUT_PARAM_VAL (Boolean, onlyLatestVers);
	CALLER_PUT_PARAM_REF (UInt16, cardNoP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (LocalID, dbIDP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapDmGetNextDatabaseByTypeCreator);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (stateInfoP);
	CALLER_GET_PARAM_REF (cardNoP);
	CALLER_GET_PARAM_REF (dbIDP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	while booting up (InstallCalibrationInfo) or starting a Gremlin
//		(ResetCalibrationInfo) as part of the process of setting the
//		calibration info to an identity state.
// --------------------

MemHandle DmGetResource (DmResType type, DmResID id)
{
	// Prepare the stack.
	CALLER_SETUP ("MemHandle", "DmResType type, DmResID id");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmResType, type);
	CALLER_PUT_PARAM_VAL (DmResID, id);

	// Call the function.
	sub.Call (sysTrapDmGetResource);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemHandle);
}

// --------------------
// Called:
//
//	*	by the application export code (My_ShlExportAsPilotFile)
//		when iterating over all the resources.
// --------------------

MemHandle DmGetResourceIndex (DmOpenRef dbP, UInt16 index)
{
	// Prepare the stack.
	CALLER_SETUP ("MemHandle", "DmOpenRef dbP, UInt16 index");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbP);
	CALLER_PUT_PARAM_VAL (UInt16, index);

	// Call the function.
	sub.Call (sysTrapDmGetResourceIndex);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemHandle);
}

// --------------------
// Called:
//
//	*	by the application install code (My_DmCreateDatabaseFromImage)
//		to create an app info block.
// --------------------

MemHandle DmNewHandle (DmOpenRef dbR, UInt32 size)
{
	// Prepare the stack.
	CALLER_SETUP ("MemHandle", "DmOpenRef dbR, UInt32 size");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbR);
	CALLER_PUT_PARAM_VAL (UInt32, size);

	// Call the function.
	sub.Call (sysTrapDmNewHandle);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemHandle);
}

// --------------------
// Called:
//
//	*	by the application install code (My_DmCreateDatabaseFromImage)
//		to create new records.
// --------------------

MemHandle DmNewRecord (DmOpenRef dbR, UInt16* atP, UInt32 size)
{
	// Prepare the stack.
	CALLER_SETUP ("MemHandle", "DmOpenRef dbR, UInt16* atP, UInt32 size");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbR);
	CALLER_PUT_PARAM_REF (UInt16, atP, Marshal::kInOut);
	CALLER_PUT_PARAM_VAL (UInt32, size);

	// Call the function.
	sub.Call (sysTrapDmNewRecord);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (atP);

	// Return the result.
	RETURN_RESULT_PTR (MemHandle);
}

// --------------------
// Called:
//
//	*	by the application install code (My_DmCreateDatabaseFromImage)
//		to create new resources.
//
//	*	while booting up (InstallCalibrationInfo)  as part of the process
//		of setting the calibration info to an identity state.
// --------------------

MemHandle DmNewResource (DmOpenRef dbR, DmResType resType, DmResID resID, UInt32 size)
{
	// Prepare the stack.
	CALLER_SETUP ("MemHandle", "DmOpenRef dbR, DmResType resType, DmResID resID, UInt32 size");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbR);
	CALLER_PUT_PARAM_VAL (DmResType, resType);
	CALLER_PUT_PARAM_VAL (DmResID, resID);
	CALLER_PUT_PARAM_VAL (UInt32, size);

	// Call the function.
	sub.Call (sysTrapDmNewResource);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemHandle);
}

// --------------------
// Called:
//
//	*	.
// --------------------

DmOpenRef DmNextOpenDatabase (DmOpenRef currentP)
{
	// Prepare the stack.
	CALLER_SETUP ("DmOpenRef", "DmOpenRef currentP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, currentP);

	// Call the function.
	sub.Call (sysTrapDmNextOpenDatabase);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (DmOpenRef);
}

// --------------------
// Called:
//
//	*	when iterating over all databases in the system (GetDatabases).
//		Called when generating a list of databases to display (e.g.,
//		in the New Gremlin and Export Database dialogs).
// --------------------

UInt16 DmNumDatabases (UInt16 cardNo)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "UInt16 cardNo");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);

	// Call the function.
	sub.Call (sysTrapDmNumDatabases);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by the application export code (My_ShlExportAsPilotFile)
//		in order to loop over all the records/resources in a database.
// --------------------

UInt16 DmNumRecords (DmOpenRef dbP)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "DmOpenRef dbP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbP);

	// Call the function.
	sub.Call (sysTrapDmNumRecords);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	.
// --------------------
UInt16 DmNumResources (DmOpenRef dbP)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "DmOpenRef dbP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbP);

	// Call the function.
	sub.Call (sysTrapDmNumResources);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by the application install code (My_DmCreateDatabaseFromImage)
//		so that new records/resources can be added to the new database.
//
//	*	by the application export code (My_ShlExportAsPilotFile)
//		so that records/resources can be retrieved and written out.
//
//	*	by AppGetExtraInfo to get information out of an application's
//		'tAIN' resource.
// --------------------

DmOpenRef DmOpenDatabase (UInt16 cardNo, LocalID dbID, UInt16 mode)
{
	// Prepare the stack.
	CALLER_SETUP ("DmOpenRef", "UInt16 cardNo, LocalID dbID, UInt16 mode");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_VAL (LocalID, dbID);
	CALLER_PUT_PARAM_VAL (UInt16, mode);

	// Call the function.
	sub.Call (sysTrapDmOpenDatabase);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (DmOpenRef);
}

// --------------------
// Called:
//
//	*	after an application has been launched (CollectCurrentAppInfo)
//		to get the app's dbID and card number.
// --------------------

Err DmOpenDatabaseInfo (DmOpenRef dbP, LocalID* dbIDP, 
					UInt16* openCountP, UInt16* modeP, UInt16* cardNoP,
					Boolean* resDBP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
		"DmOpenRef dbP, LocalID* dbIDP, "
		"UInt16* openCountP, UInt16* modeP, UInt16* cardNoP,"
		"Boolean* resDBP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbP);
	CALLER_PUT_PARAM_REF (LocalID, dbIDP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt16, openCountP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt16, modeP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt16, cardNoP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (Boolean, resDBP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapDmOpenDatabaseInfo);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (dbIDP);
	CALLER_GET_PARAM_REF (openCountP);
	CALLER_GET_PARAM_REF (modeP);
	CALLER_GET_PARAM_REF (cardNoP);
	CALLER_GET_PARAM_REF (resDBP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by the application export code (My_ShlExportAsPilotFile) in order
//		to get information about exported records.
// --------------------

Err DmRecordInfo (DmOpenRef dbP, UInt16 index,
					UInt16* attrP, UInt32* uniqueIDP, LocalID* chunkIDP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
		"DmOpenRef dbP, UInt16 index,"
		"UInt16* attrP, UInt32* uniqueIDP, LocalID* chunkIDP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbP);
	CALLER_PUT_PARAM_VAL (UInt16, index);
	CALLER_PUT_PARAM_REF (UInt16, attrP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, uniqueIDP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (LocalID, chunkIDP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapDmRecordInfo);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (attrP);
	CALLER_GET_PARAM_REF (uniqueIDP);
	CALLER_GET_PARAM_REF (chunkIDP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by the application install code (My_DmCreateDatabaseFromImage) to release
//		records created by DmNewRecord.
// --------------------

Err DmReleaseRecord (DmOpenRef dbR, UInt16 index, Boolean dirty)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "DmOpenRef dbR, UInt16 index, Boolean dirty");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbR);
	CALLER_PUT_PARAM_VAL (UInt16, index);
	CALLER_PUT_PARAM_VAL (Boolean, dirty);

	// Call the function.
	sub.Call (sysTrapDmReleaseRecord);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by all code that calls DmNewResource, DmGet1Resource, DmGetResource,
//		DmGetResourceIndex, etc.
// --------------------

Err DmReleaseResource (MemHandle resourceH)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "MemHandle resourceH");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemHandle, resourceH);

	// Call the function.
	sub.Call (sysTrapDmReleaseResource);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by application export code (My_ShlExportAsPilotFile) to get
//		information about the resources being written out.
// --------------------

Err DmResourceInfo (DmOpenRef dbP, UInt16 index,
					DmResType* resTypeP, DmResID* resIDP,  
					LocalID* chunkLocalIDP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
		"DmOpenRef dbP, UInt16 index,"
		"DmResType* resTypeP, DmResID* resIDP,"
		"LocalID* chunkLocalIDP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbP);
	CALLER_PUT_PARAM_VAL (UInt16, index);
	CALLER_PUT_PARAM_REF (DmResType, resTypeP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (DmResID, resIDP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (LocalID, chunkLocalIDP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapDmResourceInfo);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (resTypeP);
	CALLER_GET_PARAM_REF (resIDP);
	CALLER_GET_PARAM_REF (chunkLocalIDP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by application export code (My_ShlExportAsPilotFile) to get
//		information about the records being written out.
// --------------------

MemHandle DmQueryRecord (DmOpenRef dbP, UInt16 index)
{
	// Prepare the stack.
	CALLER_SETUP ("MemHandle", "DmOpenRef dbP, UInt16 index");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbP);
	CALLER_PUT_PARAM_VAL (UInt16, index);

	// Call the function.
	sub.Call (sysTrapDmQueryRecord);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemHandle);
}

// --------------------
// Called:
//
//	*	by application install code (My_DmCreateDatabaseFromImage) to
//		set the attributes of newly-created databases.
// --------------------

Err DmSetDatabaseInfo (UInt16 cardNo, LocalID dbID, const Char* nameP,
					UInt16* attributesP, UInt16* versionP, UInt32* crDateP,
					UInt32* modDateP, UInt32* bckUpDateP,
					UInt32* modNumP, LocalID* appInfoIDP,
					LocalID* sortInfoIDP, UInt32* typeP,
					UInt32* creatorP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
		"UInt16 cardNo, LocalID dbID, const Char* nameP,"
		"UInt16* attributesP, UInt16* versionP, UInt32* crDateP,"
		"UInt32* modDateP, UInt32* bckUpDateP,"
		"UInt32* modNumP, LocalID* appInfoIDP,"
		"LocalID* sortInfoIDP, UInt32* typeP,"
		"UInt32* creatorP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_VAL (LocalID, dbID);
	CALLER_PUT_PARAM_STR (Char, nameP);
	CALLER_PUT_PARAM_REF (UInt16, attributesP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (UInt16, versionP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (UInt32, crDateP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (UInt32, modDateP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (UInt32, bckUpDateP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (UInt32, modNumP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (LocalID, appInfoIDP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (LocalID, sortInfoIDP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (UInt32, typeP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (UInt32, creatorP, Marshal::kInput);

	// Call the function.
	sub.Call (sysTrapDmSetDatabaseInfo);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by application install code (My_DmCreateDatabaseFromImage) to
//		set the attributes of newly-created records.
// --------------------

Err DmSetRecordInfo (DmOpenRef dbR, UInt16 index, UInt16* attrP, UInt32* uniqueIDP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "DmOpenRef dbR, UInt16 index, UInt16* attrP, UInt32* uniqueIDP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (DmOpenRef, dbR);
	CALLER_PUT_PARAM_VAL (UInt16, index);
	CALLER_PUT_PARAM_REF (UInt16, attrP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (UInt32, uniqueIDP, Marshal::kInput);

	// Call the function.
	sub.Call (sysTrapDmSetRecordInfo);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by application install code (My_DmCreateDatabaseFromImage) to
//		copy data into the newly-created records and resources.
//
//	*	while booting up (InstallCalibrationInfo) as part of the process
//		of setting the calibration info to an identity state.
// --------------------

Err DmWrite (MemPtr recordP, UInt32 offset, const void * const srcP, UInt32 bytes)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "MemPtr recordP, UInt32 offset, const void * const srcP, UInt32 bytes");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemPtr, recordP);
	CALLER_PUT_PARAM_VAL (UInt32, offset);
	CALLER_PUT_PARAM_PTR (void, srcP, bytes, Marshal::kInput);
	CALLER_PUT_PARAM_VAL (UInt32, bytes);

	// Call the function.
	sub.Call (sysTrapDmWrite);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Event Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	by Gremlins app switch code to insert an app stop event
// --------------------

void EvtAddEventToQueue (EventType* event)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "EventType* event");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (EventType, event, Marshal::kInput);

	// Call the function.
	sub.Call (sysTrapEvtAddEventToQueue);

	// Write back any "by ref" parameters.

	// Return the result.
}

// --------------------
// Called:
//
//	*	by Gremlins and key event insertion code (StubAppEnqueueKey) to
//		insert a key event.
// --------------------

Err EvtEnqueueKey (UInt16 ascii, UInt16 keycode, UInt16 modifiers)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt16 ascii, UInt16 keycode, UInt16 modifiers");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, ascii);
	CALLER_PUT_PARAM_VAL (UInt16, keycode);
	CALLER_PUT_PARAM_VAL (UInt16, modifiers);

	// Call the function.
	sub.Call (sysTrapEvtEnqueueKey);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by Gremlins and pen event insertion code (StubEnqueuePt) to
//		insert a pen event.
// --------------------

Err EvtEnqueuePenPoint (PointType* ptP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "PointType* ptP");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (PointType, ptP, Marshal::kInput);

	// Call the function.
	sub.Call (sysTrapEvtEnqueuePenPoint);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by Gremlins code (FakeEventXY) in order to target silkscreen buttons.
// --------------------

const PenBtnInfoType*	EvtGetPenBtnList(UInt16* numButtons)
{
	// Prepare the stack.
	CALLER_SETUP ("const PenBtnInfoType*", "UInt16* numButtons");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (UInt16, numButtons, Marshal::kOutput);

	// Call the function.
	sub.Call (sysTrapEvtGetPenBtnList);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (numButtons);

	// Return the result.
	RETURN_RESULT_PTR (const PenBtnInfoType*);
}

// --------------------
// Called:
//
//	*	by Gremlins code (Gremlins::GetFakeEvent) to make sure the device
//		doesn't fall asleep, even though Gremlins is pumping in key events.
// --------------------

Err EvtResetAutoOffTimer (void)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapEvtResetAutoOffTimer);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by Gremlin code (Gremlins::New, Gremlins::Resume, Hordes::Step) to
//		wake a device out of snooze mode or a call to EvtGetEvent with an
//		infinite timeout.
//
//	*	by pen and key event insertion code to wake a device out of snooze
//		mode or a call to EvtGetEvent with an infinite timeout.
//
//	*	by HostSignalWait if the caller is looking for an idle event.
// --------------------

Err EvtWakeup (void)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapEvtWakeup);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}


#if 0
// ---------------------------------------------------------------------------
//		¥ Exchange Manager functions
// ---------------------------------------------------------------------------

#pragma mark -

typedef enum
{
	exgLibTrapHandleEvent = sysLibTrapCustom,
	exgLibTrapConnect,
	exgLibTrapAccept,
	exgLibTrapDisconnect,
	exgLibTrapPut,
	exgLibTrapGet,
	exgLibTrapSend,
	exgLibTrapReceive,
	exgLibTrapControl,	// <-- Needed for this
	exgLibReserved1,
	exgLibTrapLast
} ExgLibTrapNumberEnum;

Err 	ExgLibControl(UInt16 libRefNum, UInt16 op, void *valueP, UInt16 *valueLenP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt16 libRefNum, UInt16 op, emuptr valueP, emuptr valueLenP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, libRefNum);
	CALLER_PUT_PARAM_VAL (UInt16, op);
	CALLER_PUT_PARAM_VAL (emuptr, valueP);
	CALLER_PUT_PARAM_VAL (emuptr, valueLenP);

	sub.SetParamVal ("libRefNum", libRefNum);
	sub.SetParamVal ("op", op);
	sub.SetParamVal ("valueP", valueP);
	sub.SetParamVal ("valueLenP", valueLenP);

	sub.Call (exgLibTrapControl);

	Err result;
	sub.GetReturnVal (result);
	return result;
}
#endif

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Field Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	by Gremlin code (IsFocus, SpaceLeftInFocus) to see if a field
//		is editable.
// --------------------

void FldGetAttributes (const FieldType* fld, const FieldAttrPtr attrP)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "const FieldType* fld, const FieldAttrPtr attrP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FieldType*, fld);
	CALLER_PUT_PARAM_REF (FieldAttrType, attrP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapFldGetAttributes);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (attrP);

	// Return the result.
}

// --------------------
// Called:
//
//	*	by EventOutput to try to find out where the gremlin tapped inside a field.
// --------------------

UInt16 FldGetInsPtPosition (const FieldType* fld)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const FieldType* fld");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FieldType*, fld);

	// Call the function.
	sub.Call (sysTrapFldGetInsPtPosition);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by Gremlin code (SpaceLeftInFocus) to determine if more characters
//		should be jammed into a field.
// --------------------

UInt16 FldGetMaxChars (const FieldType* fld)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const FieldType* fld");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FieldType*, fld);

	// Call the function.
	sub.Call (sysTrapFldGetMaxChars);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by Gremlin code (SpaceLeftInFocus) to determine if more characters
//		should be jammed into a field.
// --------------------

UInt16 FldGetTextLength (const FieldType* fld)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const FieldType* fld");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FieldType*, fld);

	// Call the function.
	sub.Call (sysTrapFldGetTextLength);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by Minimization code to get the text of a field to beautify the output.
// --------------------

Char* FldGetTextPtr (FieldType* fldP)
{
	// Prepare the stack.
	CALLER_SETUP ("Char*", "FieldType* fldP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FieldType*, fldP);

	// Call the function.
	sub.Call (sysTrapFldGetTextPtr);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (Char*);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Font Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	by Gremlin code (FakeLocalMovement) to generate strokes based on
//		the font height.
// --------------------

Int16 FntLineHeight (void)
{
	// Prepare the stack.
	CALLER_SETUP ("Int16", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapFntLineHeight);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Int16);
}

// --------------------
// Called:
//
// --------------------

UInt8 FntSetFont (UInt8 fontId)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt8", "UInt8 fontId");

	// Set the parameters.

	CALLER_PUT_PARAM_VAL (UInt8, fontId);

	// Call the function.
	sub.Call (sysTrapFntSetFont);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL(UInt8);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Form Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	by Gremlin code (GetFocusObject) to determine if the active form
//		is the active window.
//
//	*	by Gremlin code (FakeEventXY) to see if there's a form that should
//		be targeted.
// --------------------

FormType* FrmGetActiveForm (void)
{
	// Prepare the stack.
	CALLER_SETUP ("FormType*", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapFrmGetActiveForm);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (FormType*);
}

// --------------------
// Called:
//
//	*	by Gremlin code (GetFocusObject) when trying to determine the
//		object that has the focus and that should get preferred attention.
// --------------------

UInt16 FrmGetFocus (const FormType* frm)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const FormType* frm");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);

	// Call the function.
	sub.Call (sysTrapFrmGetFocus);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	EmEventOutput::GetEventInfo
// --------------------

UInt16 FrmGetFormId (const FormType* frm)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const FormType* frm");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);

	// Call the function.
	sub.Call (sysTrapFrmGetFormId);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by Gremlin code (CollectOKObjects) in order to iterate over
//		all the objects in a form.
// --------------------

UInt16 FrmGetNumberOfObjects (const FormType* frm)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const FormType* frm");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);

	// Call the function.
	sub.Call (sysTrapFrmGetNumberOfObjects);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by Gremlin code (FakeEventXY) to generate a random point within
//		a form object.
//
//	*	by Gremlin code (CollectOKObjects) to get the bounds of an object
//		for validation.
// --------------------

void FrmGetObjectBounds (const FormType* frm, const UInt16 pObjIndex, const RectanglePtr r)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "const FormType* frm, const UInt16 pObjIndex, const RectanglePtr r");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);
	CALLER_PUT_PARAM_VAL (UInt16, pObjIndex);
	CALLER_PUT_PARAM_REF (RectangleType, r, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapFrmGetObjectBounds);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (r);

	// Return the result.
}

// --------------------
// Called:
//
//	*	by Gremlin code (CollectOKObjects) to get the ID of an object
//		for use in any error messages that might occur.
// --------------------

UInt16 FrmGetObjectId (const FormType* frm, const UInt16 objIndex)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const FormType* frm, const UInt16 objIndex");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);
	CALLER_PUT_PARAM_VAL (UInt16, objIndex);

	// Call the function.
	sub.Call (sysTrapFrmGetObjectId);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by minimization code, when replaying the events the final time,
//		in order to grab the control names.
// --------------------

UInt16 FrmGetObjectIndex (const FormType* formP, UInt16 objID)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const FormType* formP, UInt16 objID");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, formP);
	CALLER_PUT_PARAM_VAL (UInt16, objID);

	// Call the function.
	sub.Call (sysTrapFrmGetObjectIndex);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by Gremlin code (GetFocusObject) to get a pointer to a table object
//		so that we can call TblGetCurrentField on it.
//
//	*	by Gremlin code (CollectOKObjects) to get a pointer to control
//		and list objects so that we can validate them.
// --------------------

MemPtr FrmGetObjectPtr (const FormType* frm, const UInt16 objIndex)
{
	// Prepare the stack.
	CALLER_SETUP ("MemPtr", "const FormType* frm, const UInt16 objIndex");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);
	CALLER_PUT_PARAM_VAL (UInt16, objIndex);

	// Call the function.
	sub.Call (sysTrapFrmGetObjectPtr);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemPtr);
}

// --------------------
// Called:
//
//	*	by Gremlin code (GetFocusObject) to see if the focussed object
//		is a table that might have an embedded field.
//
//	*	by Gremlin code (CollectOKObjects) to see if the object we're
//		iterating over is one that we'd like to emulate a tap on.
// --------------------

FormObjectKind FrmGetObjectType (const FormType* frm, const UInt16 objIndex)
{
	// Prepare the stack.
	CALLER_SETUP ("FormObjectKind", "const FormType* frm, const UInt16 objIndex");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);
	CALLER_PUT_PARAM_VAL (UInt16, objIndex);

	// Call the function.
	sub.Call (sysTrapFrmGetObjectType);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (FormObjectKind);
}

// --------------------
// Called:
//
//	*	by logging code (StubEmFrmGetTitle) to get the form's title.
// --------------------

const Char* FrmGetTitle (const FormType* frm)
{
	// Prepare the stack.
	CALLER_SETUP ("const Char*", "const FormType* frm");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);

	// Call the function.
	sub.Call (sysTrapFrmGetTitle);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (const Char*);
}

// --------------------
// Called:
//
//	*	by Gremlin code (GetFocusObject, FakeEventXY) to determine if the active
//		form is the active window.
// --------------------

WinHandle FrmGetWindowHandle (const FormType* frm)
{
	// Prepare the stack.
	CALLER_SETUP ("WinHandle", "const FormType* frm");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (FormType*, frm);

	// Call the function.
	sub.Call (sysTrapFrmGetWindowHandle);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (WinHandle);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ File System functions
// ---------------------------------------------------------------------------

typedef enum {
	FSTrapLibAPIVersion = sysLibTrapCustom,
	FSTrapCustomControl,	// <-- Needed for this
	FSTrapFilesystemType,
	
	FSTrapFileCreate,
	FSTrapFileOpen,
	FSTrapFileClose,
	FSTrapFileRead,
	FSTrapFileWrite,
	FSTrapFileDelete,
	FSTrapFileRename,
	FSTrapFileSeek,
	FSTrapFileEOF,
	FSTrapFileTell,
	FSTrapFileResize,
	FSTrapFileAttributesGet,
	FSTrapFileAttributesSet,
	FSTrapFileDateGet,
	FSTrapFileDateSet,
	FSTrapFileSize,
	
	FSTrapDirCreate,
	FSTrapDirEntryEnumerate,
	
	FSTrapVolumeFormat,
	FSTrapVolumeMount,
	FSTrapVolumeUnmount,
	FSTrapVolumeInfo,
	FSTrapVolumeLabelGet,
	FSTrapVolumeLabelSet,
	FSTrapVolumeSize,
	
	FSMaxSelector = FSTrapVolumeSize,

	FSBigSelector = 0x7FFF	// Force FSLibSelector to be 16 bits.
} FSLibSelector;

Err FSCustomControl (UInt16 fsLibRefNum, UInt32 apiCreator, UInt16 apiSelector, 
					void *valueP, UInt16 *valueLenP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
		"UInt16 fsLibRefNum, UInt32 apiCreator, UInt16 apiSelector, "
		"emuptr valueP, UInt16 *valueLenP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, fsLibRefNum);
	CALLER_PUT_PARAM_VAL (UInt32, apiCreator);
	CALLER_PUT_PARAM_VAL (UInt16, apiSelector);
	CALLER_PUT_PARAM_VAL (emuptr, valueP);		// !!! Only works for our mount/unmount calls!
	CALLER_PUT_PARAM_REF (UInt16, valueLenP, Marshal::kInOut);

	// Call the function.
	sub.Call (FSTrapCustomControl);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (valueLenP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Feature Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	by Gremlin code (IntlMgrExists) to see if the Intenational Manager
//		exists before trying to call it.
//
//	*	by Gremlin code (Gremlins::GetFakeEvent) to see what language system
//		we're using (so we know whether to use the English or Japanese quotes).
//
//	*	during bootup (patch on FtrInit) to get the ROM version.
// --------------------

Err FtrGet (UInt32 creator, UInt16 featureNum, UInt32* valueP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt32 creator, UInt16 featureNum, UInt32* valueP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt32, creator);
	CALLER_PUT_PARAM_VAL (UInt16, featureNum);
	CALLER_PUT_PARAM_REF (UInt32, valueP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapFtrGet);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (valueP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	to install the 'pose' feature after creating or loading a session.
//
//	*	by Poser's debugger handling code (Debug::EventCallback) after a
//		connection to an external debugger is established.
//
//	*	after creating a new session and we are currently connected to a debugger.
//
//	*	after reloading a .psf file and we are currently connected to a debugger.
// --------------------

Err FtrSet (UInt32 creator, UInt16 featureNum, UInt32 newValue)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt32 creator, UInt16 featureNum, UInt32 newValue");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt32, creator);
	CALLER_PUT_PARAM_VAL (UInt16, featureNum);
	CALLER_PUT_PARAM_VAL (UInt32, newValue);

	// Call the function.
	sub.Call (sysTrapFtrSet);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by Poser's debugger handling code (Debug::EventCallback) when it
//		detects that the connection to an external debugger has been broken.
//
//	*	after reloading a .psf file and we are not currently connected to a debugger.
// --------------------

Err	FtrUnregister (UInt32 creator, UInt16 featureNum)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt32 creator, UInt16 featureNum");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt32, creator);
	CALLER_PUT_PARAM_VAL (UInt16, featureNum);

	// Call the function.
	sub.Call (sysTrapFtrUnregister);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ International Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	By dialog code, if the check is enabled.
// --------------------

Boolean IntlSetStrictChecks (Boolean iStrictChecks)
{
	// Prepare the stack.
	CALLER_SETUP ("Boolean", "Boolean iStrictChecks");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (Boolean, iStrictChecks);

	// Call the function.
	sub.CallSelector (sysTrapIntlDispatch, intlIntlStrictChecks);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Boolean);
}

// ---------------------------------------------------------------------------
//		¥ List Manager functions
// ---------------------------------------------------------------------------

#pragma mark -

// --------------------
// Called:
//
//	*	by PrvValidateFormObjectSize in Miscellaneous.cpp to determine if 
//		a zero-height list has any items.
// --------------------

Int16 LstGetNumberOfItems (const ListType* lst)
{
	// Prepare the stack.
	CALLER_SETUP ("Int16", "const ListType* lst");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (ListType*, lst);

	// Call the function.
	sub.Call (sysTrapLstGetNumberOfItems);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Int16);
}

// --------------------
// Called:
//
//	*	By EventOutput code to try to find out which
//      item in a list was tapped by a gremlin.
// --------------------

Int16 LstGetSelection (const ListType* lst)
{
	// Prepare the stack.
	CALLER_SETUP ("Int16", "const ListType* lst");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (ListType*, lst);

	// Call the function.
	sub.Call (sysTrapLstGetSelection);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Int16);
}

// --------------------
// Called:
//
//	*	By EventOutput code to try to find the
//      text of the item in a list that was tapped by a gremlin.
// --------------------

Char * LstGetSelectionText (const ListType *lst, Int16 itemNum)
{
	// Prepare the stack.
	CALLER_SETUP ("Char *", "const ListType* lst, Int16 itemNum");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (ListType*, lst);
	CALLER_PUT_PARAM_VAL (Int16, itemNum);

	// Call the function.
	sub.Call (sysTrapLstGetSelectionText);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (Char*);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Memory Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	in our patch to SysUIAppSwitch (called as MemPtrFree) to free up any
//		left over cmdPBP's.
// --------------------

Err MemChunkFree (MemPtr chunkDataP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "MemPtr chunkDataP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemPtr, chunkDataP);

	// Call the function.
	sub.Call (sysTrapMemChunkFree);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	during the application install process (My_DmCreateDatabaseFromImage)
//		in order to lock a database handle before calling DmWrite.
//
//	*	by our SysAppStartup patch (CollectCurrentAppInfo) to get information
//		from the 'tAIN' or 'tver' resources.
//
//	*	while booting up (InstallCalibrationInfo) or starting a Gremlin
//		(ResetCalibrationInfo) as part of the process of setting the
//		calibration info to an identity state.
// --------------------

MemPtr MemHandleLock (MemHandle h)
{
	// Prepare the stack.
	CALLER_SETUP ("MemPtr", "MemHandle h");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemHandle, h);

	// Call the function.
	sub.Call (sysTrapMemHandleLock);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemPtr);
}

// --------------------
// Called:
//
//	*	during the application export process (My_ShlExportAsPilotFile)
//		all over the place
// --------------------

UInt32 MemHandleSize (MemHandle h)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt32", "MemHandle h");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemHandle, h);

	// Call the function.
	sub.Call (sysTrapMemHandleSize);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt32);
}

// --------------------
// Called:
//
//	*	during the application install process (My_DmCreateDatabaseFromImage)
//		to set the app info block field after the block's been created.
// --------------------

LocalID MemHandleToLocalID (MemHandle h)
{
	// Prepare the stack.
	CALLER_SETUP ("LocalID", "MemHandle h");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemHandle, h);

	// Call the function.
	sub.Call (sysTrapMemHandleToLocalID);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (LocalID);
}

// --------------------
// Called:
//
//	*	during the application install process (My_DmCreateDatabaseFromImage)
//		in order to unlock a database handle after copying the data
//		into it.
//
//	*	getting information about a database (AppGetExtraInfo) from its 'tAIN'
//		resource or app info block.
//
//	*	while booting up (InstallCalibrationInfo) or starting a Gremlin
//		(ResetCalibrationInfo) as part of the process of setting the
//		calibration info to an identity state.
// --------------------

Err MemHandleUnlock (MemHandle h)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "MemHandle h");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemHandle, h);

	// Call the function.
	sub.Call (sysTrapMemHandleUnlock);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	from our patch to MemInitHeapTable when walking all current
//		heaps to locate them all.
// --------------------

UInt16 MemHeapID (UInt16 cardNo, UInt16 heapIndex)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "UInt16 cardNo, UInt16 heapIndex");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_VAL (UInt16, heapIndex);

	// Call the function.
	sub.Call (sysTrapMemHeapID);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	by EmPalmHeap::GetHeapHeaderInfo (heapID) in order to get a heap
//		pointer that can be passed to EmPalmHeap::GetHeapHeaderInfo (ptr).
//		This version of GetHeapHeaderInfo is called from the version of
//		the EmPalmHeap constructor that taks a heap ID.  This version of
//		the constructor is called from our patches on MemInitHeapTable
//		and MemHeapInit.
// --------------------

MemPtr MemHeapPtr (UInt16 heapID)
{
	// Prepare the stack.
	CALLER_SETUP ("MemPtr", "UInt16 heapID");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, heapID);

	// Call the function.
	sub.Call (sysTrapMemHeapPtr);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemPtr);
}

// --------------------
// Called:
//
//	*	while getting an application's name (AppGetExtraInfo) from
//		the app info block -- we need to determine if the block
//		is a handle or not so that we know whether or not to lock it.
// --------------------

LocalIDKind MemLocalIDKind (LocalID local)
{
	// Prepare the stack.
	CALLER_SETUP ("LocalIDKind", "LocalID local");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (LocalID, local);

	// Call the function.
	sub.Call (sysTrapMemLocalIDKind);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (LocalIDKind);
}

// --------------------
// Called:
//
//	*	when exporting a database (My_ShlExportAsPilotFile) to get the
//		app info and sort info handles so that we can get their sizes.
//
//	*	while getting an application's name (AppGetExtraInfo) from the
//		app info block.
// --------------------

MemPtr MemLocalIDToGlobal (LocalID local, UInt16 cardNo)
{
	// Prepare the stack.
	CALLER_SETUP ("MemPtr", "LocalID local, UInt16 cardNo");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (LocalID, local);
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);

	// Call the function.
	sub.Call (sysTrapMemLocalIDToGlobal);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemPtr);
}

// --------------------
// Called:
//
//	*	when iterating over all databases in the system (GetDatabases).
//		Called when generating a list of databases to display (e.g.,
//		in the New Gremlin and Export Database dialogs).
// --------------------

UInt16 MemNumCards (void)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapMemNumCards);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	while booting up on a patch to MemInitHeapTable to determine
//		how many heaps we need to track.
// --------------------

UInt16 MemNumHeaps (UInt16 cardNo)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "UInt16 cardNo");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);

	// Call the function.
	sub.Call (sysTrapMemNumHeaps);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	when booting up (PrvSetCurrentDate) to set the current date
//		stored in the non-volatile data section.
// --------------------

Err MemNVParams (Boolean set, SysNVParamsPtr paramsP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "Boolean set, SysNVParamsPtr paramsP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (Boolean, set);
	CALLER_PUT_PARAM_REF (SysNVParamsType, paramsP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapMemNVParams);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (paramsP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	by Gremlins (Gremlins::New) when it needs to create a cmdPBP  telling
//		 Clipper which PQA to launch.
//
//	*	same comments for Patches::SwitchToApp, called when a .pqa was in
//		AutoRun or AutoRunAndQuit.
// --------------------

MemPtr MemPtrNew (UInt32 size)
{
	// Prepare the stack.
	CALLER_SETUP ("MemPtr", "UInt32 size");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt32, size);

	// Call the function.
	sub.Call (sysTrapMemPtrNew);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (MemPtr);
}

// --------------------
// Called:
//
//	*	by Gremlins (Gremlins::New) when it needs to create a cmdPBP  telling
//		 Clipper which PQA to launch.
//
//	*	same comments for Patches::SwitchToApp, called when a .pqa was in
//		AutoRun or AutoRunAndQuit.
// --------------------

Err MemPtrSetOwner (MemPtr p, UInt16 owner)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "MemPtr p, UInt16 owner");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemPtr, p);
	CALLER_PUT_PARAM_VAL (UInt16, owner);

	// Call the function.
	sub.Call (sysTrapMemPtrSetOwner);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	while generate a list of databases and their names.  Some non-application
//		databases have special resources with this information; the resource
//		size needs to be checked to see if this information is included.
//
//	*	by our SysAppStartup patch (CollectCurrentAppInfo) to get the size of the stack.
// --------------------

UInt32 MemPtrSize (MemPtr p)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt32", "MemPtr p");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemPtr, p);

	// Call the function.
	sub.Call (sysTrapMemPtrSize);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt32);
}

// --------------------
// Called:
//
//	*	during the application export process (My_ShlExportAsPilotFile)
//		in order to unlock a database handle after copying the data
//		out of it.
// --------------------

Err MemPtrUnlock (MemPtr p)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "MemPtr p");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (MemPtr, p);

	// Call the function.
	sub.Call (sysTrapMemPtrUnlock);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Net Library functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	NetLib redirection code (Platform_NetLib::OpenConfig) to activate
//		the configuration passed into NetOpenConfig.  The process mirrors
//		that which goes on in NetLib itself.
// --------------------

Err NetLibConfigMakeActive (UInt16 refNum, UInt16 configIndex)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt16 refNum, UInt16 configIndex");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, refNum);
	CALLER_PUT_PARAM_VAL (UInt16, configIndex);

	// Call the function.
	sub.Call (netLibConfigMakeActive);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Pen Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	while starting a Gremlin (ResetCalibrationInfo) to reset the Pen
//		calibration information *after* the Pen manager has read the
//		preference file (that is, merely changing the preference data
//		won't work, since PenOpen has already read it).
// --------------------

Err PenCalibrate (PointType* digTopLeftP, PointType* digBotRightP,
					PointType* scrTopLeftP, PointType* scrBotRightP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", 
		"PointType* digTopLeftP, PointType* digBotRightP,"
		"PointType* scrTopLeftP, PointType* scrBotRightP");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (PointType, digTopLeftP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (PointType, digBotRightP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (PointType, scrTopLeftP, Marshal::kInput);
	CALLER_PUT_PARAM_REF (PointType, scrBotRightP, Marshal::kInput);

	// Call the function.
	sub.Call (sysTrapPenCalibrate);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	while booting up (InstallCalibrationInfo) or starting a Gremlin
//		(ResetCalibrationInfo) as part of the process of setting the
//		calibration info to an identity state.
// --------------------

Err	 PenRawToScreen(PointType* penP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "PointType* penP");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (PointType, penP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapPenRawToScreen);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (penP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	our EvtEnqueuePenPoint wrapper.
// --------------------

Err PenScreenToRaw (PointType* penP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "PointType* penP");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (PointType, penP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapPenScreenToRaw);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (penP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Peferences Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	while booting up (InstallCalibrationInfo) or starting a Gremlin
//		(ResetCalibrationInfo) as part of the process of setting the
//		calibration info to an identity state (1.0 devices only).
// --------------------

DmOpenRef PrefOpenPreferenceDBV10 (void)
{
	// Prepare the stack.
	CALLER_SETUP ("DmOpenRef", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapPrefOpenPreferenceDBV10);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (DmOpenRef);
}

// --------------------
// Called:
//
//	*	while booting up (InstallCalibrationInfo) or starting a Gremlin
//		(ResetCalibrationInfo) as part of the process of setting the
//		calibration info to an identity state (2.0 devices and later).
// --------------------

DmOpenRef PrefOpenPreferenceDB (Boolean saved)
{
	// Prepare the stack.
	CALLER_SETUP ("DmOpenRef", "Boolean saved");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (Boolean, saved);

	// Call the function.
	sub.Call (sysTrapPrefOpenPreferenceDB);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (DmOpenRef);
}

// --------------------
// Called:
//
//	*	while booting up in a tailpatch to UIInitialize to turn off
//		any auto-off values.
// --------------------

void PrefSetPreference (SystemPreferencesChoice choice, UInt32 value)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "SystemPreferencesChoice choice, UInt32 value");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (SystemPreferencesChoice, choice);
	CALLER_PUT_PARAM_VAL (UInt32, value);

	// Call the function.
	sub.Call (sysTrapPrefSetPreference);

	// Write back any "by ref" parameters.

	// Return the result.
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ System Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	in a headpatch to SysUIAppSwitch to see if the application we're
//		switching to is allowed (it may not be during Gremlins).
// --------------------

Err	SysCurAppDatabase (UInt16* cardNoP, LocalID* dbIDP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt16* cardNoP, LocalID* dbIDP");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (UInt16, cardNoP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (LocalID, dbIDP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapSysCurAppDatabase);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (cardNoP);
	CALLER_GET_PARAM_REF (dbIDP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	in the memory access checking code (GWH_ExamineChunk) to iterate
//		the tasks.  This is part of the hueristic to see if a task
//		that's being terminated is still trying to access its stack after
//		it's been deleted.
// --------------------

Err SysKernelInfo (MemPtr p)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "SysKernelInfoPtr p");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (SysKernelInfoType, p, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapSysKernelInfo);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (p);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	.
// --------------------

Err SysLibFind (const Char *nameP, UInt16 *refNumP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "const Char *nameP, UInt16 *refNumP");

	// Set the parameters.
	CALLER_PUT_PARAM_STR (Char, nameP);
	CALLER_PUT_PARAM_REF (UInt16, refNumP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapSysLibFind);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (refNumP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	.
// --------------------

Err SysLibLoad (UInt32 libType, UInt32 libCreator, UInt16 *refNumP)
{
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt32 libType, UInt32 libCreator, UInt16 *refNumP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt32, libType);
	CALLER_PUT_PARAM_VAL (UInt32, libCreator);
	CALLER_PUT_PARAM_REF (UInt16, refNumP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapSysLibLoad);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (refNumP);

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

// --------------------
// Called:
//
//	*	.
// --------------------

SysLibTblEntryPtr SysLibTblEntry (UInt16 refNum)
{
	// Prepare the stack.
	CALLER_SETUP ("SysLibTblEntryPtr", "UInt16 refNum");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, refNum);

	// Call the function.
	sub.Call (sysTrapSysLibTblEntry);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (SysLibTblEntryPtr);
}

// --------------------
// Called:
//
//	*	while booting up in a tailpatch to UIInitialize to turn off
//		any auto-off values.
// --------------------

UInt16 SysSetAutoOffTime (UInt16 seconds)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "UInt16 seconds");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, seconds);

	// Call the function.
	sub.Call (sysTrapSysSetAutoOffTime);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	By Gremlin code (Gremlins::New) to switch to an approved application
//		(either one on the approved list, or one that can "run" one on the
//		approved like, like a PQA).
//
//	*	By Patches::SwitchToApp to switch to a given application.  This
//		function is called after booting or loading a session and the user
//		has a file in the AutoRun or AutoRunAndQuit directories.
// --------------------

Err SysUIAppSwitch (UInt16 cardNo, LocalID dbID, UInt16 cmd, MemPtr cmdPBP)
{	
	// Prepare the stack.
	CALLER_SETUP ("Err", "UInt16 cardNo, LocalID dbID, UInt16 cmd, MemPtr cmdPBP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt16, cardNo);
	CALLER_PUT_PARAM_VAL (LocalID, dbID);
	CALLER_PUT_PARAM_VAL (UInt16, cmd);
	CALLER_PUT_PARAM_VAL (MemPtr, cmdPBP);

	// Call the function.
	sub.Call (sysTrapSysUIAppSwitch);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Err);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Table Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	By EventOutput code
// --------------------

Coord TblGetColumnSpacing (const TableType* tableP, Int16 column)
{
	// Prepare the stack.
	CALLER_SETUP ("Coord", "const TableType* tableP, Int16 column");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (TableType*, tableP);
	CALLER_PUT_PARAM_VAL (Int16, column);

	// Call the function.
	sub.Call (sysTrapTblGetColumnSpacing);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Coord);
}

// --------------------
// Called:
//
//	*	By EventOutput code
// --------------------

Coord TblGetColumnWidth (const TableType* tableP, Int16 column)
{
	// Prepare the stack.
	CALLER_SETUP ("Coord", "const TableType* tableP, Int16 column");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (TableType*, tableP);
	CALLER_PUT_PARAM_VAL (Int16, column);

	// Call the function.
	sub.Call (sysTrapTblGetColumnWidth);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Coord);
}

// --------------------
// Called:
//
//	*	By Gremlin code (GetFocusObject) to determine if there's a sub-field
//		in a table that needs to be targeted.
// --------------------

FieldPtr TblGetCurrentField (const TableType* table)
{
	// Prepare the stack.
	CALLER_SETUP ("FieldPtr", "const TableType* table");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (TableType*, table);

	// Call the function.
	sub.Call (sysTrapTblGetCurrentField);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (FieldPtr);
}

// --------------------
// Called:
//
//	*	By EventOutput code
// --------------------

Coord TblGetRowHeight (const TableType* tableP, Int16 row)
{
	// Prepare the stack.
	CALLER_SETUP ("Coord", "const TableType* tableP, Int16 row");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (TableType*, tableP);
	CALLER_PUT_PARAM_VAL (Int16, row);

	// Call the function.
	sub.Call (sysTrapTblGetRowHeight);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (Coord);
}

// --------------------
// Called:
//
//	*	By EventOutput code (GetPreviousEventInfo) to try to find out where
//      in the table was tapped by a gremlin.
// --------------------

Boolean TblGetSelection (const TableType* tableP, Int16* rowP, Int16* columnP)
{
	// Prepare the stack.
	CALLER_SETUP ("Boolean", "const TableType* tableP, Int16* rowP, Int16* columnP");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (TableType*, tableP);
	CALLER_PUT_PARAM_REF (Int16, rowP, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (Int16, columnP, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapTblGetSelection);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (rowP);
	CALLER_GET_PARAM_REF (columnP);

	// Return the result.
	RETURN_RESULT_VAL (Boolean);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Text Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	By Gremlin code (GetFakeEvent) when generating the percentage
//		that any character is randomly posted to the application.
// --------------------

UInt8 TxtByteAttr(UInt8 inByte)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt8", "UInt8 inByte");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (UInt8, inByte);

	// Call the function.
	sub.CallSelector (sysTrapIntlDispatch, intlTxtByteAttr);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_VAL (UInt8);
}

// --------------------
// Called:
//
//	*	By Gremlin code (GetFakeEvent) when generating the percentage
//		that any character is randomly posted to the application.
// --------------------

UInt16 TxtCharBounds (const Char* inText, UInt32 inOffset, UInt32* outStart, UInt32* outEnd)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const Char* inText, UInt32 inOffset, UInt32* outStart, UInt32* outEnd");

	// Set the parameters.
	CALLER_PUT_PARAM_PTR (Char, inText, inOffset + 4, Marshal::kInput);
	CALLER_PUT_PARAM_VAL (UInt32, inOffset);
	CALLER_PUT_PARAM_REF (UInt32, outStart, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (UInt32, outEnd, Marshal::kInOut);

	// Call the function.
	sub.CallSelector (sysTrapIntlDispatch, intlTxtCharBounds);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (outStart);
	CALLER_GET_PARAM_REF (outEnd);

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

// --------------------
// Called:
//
//	*	By Gremlin code (SendCharsToType) when iterating over the hardcoded
//		text to be posted to the application.
// --------------------

UInt16 TxtGetNextChar (const Char* inText, UInt32 inOffset, WChar* outChar)
{
	// Prepare the stack.
	CALLER_SETUP ("UInt16", "const Char* inText, UInt32 inOffset, WChar* outChar");

	// Set the parameters.
	CALLER_PUT_PARAM_PTR (Char, inText, inOffset + 4, Marshal::kInput);
	CALLER_PUT_PARAM_VAL (UInt32, inOffset);
	CALLER_PUT_PARAM_REF (WChar, outChar, Marshal::kInOut);

	// Call the function.
	sub.CallSelector (sysTrapIntlDispatch, intlTxtGetNextChar);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (outChar);

	// Return the result.
	RETURN_RESULT_VAL (UInt16);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Window Manager functions
// ---------------------------------------------------------------------------

// --------------------
// Called:
//
//	*	By Gremlins code (FakeEventXY) after picking a window object to click on.
// --------------------

void WinDisplayToWindowPt (Int16* extentX, Int16* extentY)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "Int16* extentX, Int16* extentY");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (Int16, extentX, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (Int16, extentY, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapWinDisplayToWindowPt);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (extentX);
	CALLER_GET_PARAM_REF (extentY);

	// Return the result.
}

// --------------------
// Called:
//
//	*	By Gremlin code (GetFocusObject) to make sure that the active window
//		is also the active form.
//
//	*	By Gremlin code (RandomWindowXY) to determine if there's a window
//		within which a pen event should be generated.
//
//	*	By Gremlin code (RandomWindowXY) to set the draw window before calling
//		WinGetWindowBounds.
// --------------------

WinHandle WinGetActiveWindow (void)
{
	// Prepare the stack.
	CALLER_SETUP ("WinHandle", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapWinGetActiveWindow);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (WinHandle);
}

// --------------------
// Called:
//
//	*	By Gremlin code (FakeLocalMovement) to clip random pen dragging
//		to the window's bounds.
//
//	*	By Gremlin code (RandomScreenXY) to generate a random pen event
//		within a window's bounds.
//
//	*	By Gremlin code (FakeEventXY) to clip a random pen event within
//		an object to the window's bounds.
//
//	*	By Gremlin code (CollectOKObjects) to check to see if all objects
//		are within the window's bounds.
// --------------------

void WinGetDisplayExtent (Int16* extentX, Int16* extentY)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "Int16* extentX, Int16* extentY");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (Int16, extentX, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (Int16, extentY, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapWinGetDisplayExtent);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (extentX);
	CALLER_GET_PARAM_REF (extentY);

	// Return the result.
}

// --------------------
// Called:
//
//	*	By logging code (StubEmPrintFormID) to walk the window list in
//		order to ensure that a WinHandle is valid before trying to get
//		its name/title.
// --------------------

WinHandle WinGetFirstWindow (void)
{
	// Prepare the stack.
	CALLER_SETUP ("WinHandle", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapWinGetFirstWindow);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (WinHandle);
}

// --------------------
// Called:
//
//	*	By Gremlins code (RandomWindowXY) before calling WinGetWindowBounds
//		while determining a random location to tap in the window.
//
//	!!! This is renamed to WinGetDrawWindowBounds in Bellagio.
// --------------------

void WinGetWindowBounds (RectanglePtr r)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "RectanglePtr r");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (RectangleType, r, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapWinGetDrawWindowBounds);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (r);

	// Return the result.
}

// --------------------
// Called:
//
//	* In our tailpatch of TblHandleEvent to fix a Palm OS 3.5 bug.
// --------------------

void WinPopDrawState (void)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "void");

	// Set the parameters.

	// Call the function.
	sub.Call (sysTrapWinPopDrawState);

	// Write back any "by ref" parameters.

	// Return the result.
}

// --------------------
// Called:
//
// --------------------

WinHandle WinSetDrawWindow (WinHandle winHandle)
{
	// Prepare the stack.
	CALLER_SETUP ("WinHandle", "WinHandle winHandle");

	// Set the parameters.
	CALLER_PUT_PARAM_VAL (WinHandle, winHandle);

	// Call the function.
	sub.Call (sysTrapWinSetDrawWindow);

	// Write back any "by ref" parameters.

	// Return the result.
	RETURN_RESULT_PTR (WinHandle);
}

// --------------------
// Called:
//
//	*	By Gremlins code (FakeEventXY) after picking a window object to click on.
// --------------------

void WinWindowToDisplayPt (Int16* extentX, Int16* extentY)
{
	// Prepare the stack.
	CALLER_SETUP ("void", "Int16* extentX, Int16* extentY");

	// Set the parameters.
	CALLER_PUT_PARAM_REF (Int16, extentX, Marshal::kInOut);
	CALLER_PUT_PARAM_REF (Int16, extentY, Marshal::kInOut);

	// Call the function.
	sub.Call (sysTrapWinWindowToDisplayPt);

	// Write back any "by ref" parameters.
	CALLER_GET_PARAM_REF (extentX);
	CALLER_GET_PARAM_REF (extentY);

	// Return the result.
}
