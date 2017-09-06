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
#include "Miscellaneous.h"

#include "Byteswapping.h"		// Canonical
#include "ChunkFile.h"			// Chunk::GetPointer
#include "EmBankMapped.h"		// EmBankMapped::GetEmulatedAddress
#include "EmErrCodes.h"			// kError_UnimplementedTrap
#include "EmHAL.h"				// EmHAL::ResetTimer, EmHAL::ResetRTC
#include "EmLowMem.h"			// EmLowMem_SetGlobal, EmLowMem_GetGlobal
#include "EmMemory.h"			// Memory::MapPhysicalMemory, EmMem_strcpy, EmMem_memcmp
#include "EmPalmFunction.h"		// GetFunctionAddress
#include "EmPatchState.h"		// EmPatchState::OSMajorVersion
#include "EmSession.h"			// ScheduleDeferredError
#include "EmStreamFile.h"		// EmStreamFile, kOpenExistingForRead
#include "ErrorHandling.h"		// Errors::Throw
#include "Logging.h"			// LogDump
#include "Platform.h"			// Platform::AllocateMemory
#include "ROMStubs.h"			// WinGetDisplayExtent, FrmGetNumberOfObjects, FrmGetObjectType, FrmGetObjectId, ...
#include "Strings.r.h"			// kStr_INetLibTrapBase, etc.
#include "UAE.h"				// m68k_dreg, etc.

#include <algorithm>			// sort()
#include <locale.h> 			// localeconv, lconv
#include <strstream>			// strstream
#include <time.h>				// time, localtime


extern "C" {
	// These are defined in machdep_maccess.h, too
#undef get_byte
#undef put_byte
#undef put_long
#include "gzip.h"
#include "lzw.h"

int (*write_buf_proc)(char *buf, unsigned size);

DECLARE(uch, inbuf,  INBUFSIZ +INBUF_EXTRA);
DECLARE(uch, outbuf, OUTBUFSIZ+OUTBUF_EXTRA);
DECLARE(ush, d_buf,  DIST_BUFSIZE);
DECLARE(uch, window, 2L*WSIZE);
DECLARE(ush, tab_prefix, 1L<<BITS);

int test = 0;		  /* test .gz file integrity */
int level = 1;		  /* compression level */
int exit_code = OK; 	 /* program exit code */
int verbose = 2;		/* be verbose (-v) */
int quiet = 0;			/* be quiet (-q) */

char ifname[] = "ifname";
char ofname[] = "ofname";
char* progname = "progname";

long bytes_in;			   /* number of input bytes */
long bytes_out; 		   /* number of output bytes */
int  ifd;				   /* input file descriptor */
int  ofd;				   /* output file descriptor */
unsigned insize;		   /* valid bytes in inbuf */
unsigned inptr; 		   /* index of next byte to be processed in inbuf */
unsigned outcnt;		   /* bytes in output buffer */

#include <setjmp.h>
jmp_buf env;

RETSIGTYPE abort_gzip()
{
	LogDump ();
	abort ();
//	longjmp (env, 1);
}

int my_fprintf (FILE*, const char* fmt, ...)
{
	int 	n;
	va_list arg;

	va_start (arg, fmt);
	n = LogGetStdLog ()->VPrintf (fmt, arg);
	va_end (arg);

	return n;
}

}	// extern "C"


extern "C"
{
	int PrvGzipReadProc 	(char* buf, unsigned size);
	int PrvGzipWriteProc	(char* buf, unsigned size);
}

static void*	gSrcP;
static void*	gDstP;
static long 	gSrcBytes;
static long 	gDstBytes;
static long 	gSrcOffset;
static long 	gDstOffset;

// ===========================================================================
//	¥ StMemory Class
// ===========================================================================
//	Constructor allocates the Ptr
//	Destructor disposes of the Ptr

StMemory::StMemory(
	char*	inPtr)
{
	mIsOwner = (inPtr != NULL);
	mPtr	 = inPtr;
}

StMemory::StMemory(
	long	inSize, 				// Bytes to allocate
	Bool	inClearBytes)			// Whether to clear all bytes to zero
{
	mIsOwner = true;

	if (inClearBytes)
		mPtr = (char*) Platform::AllocateMemoryClear (inSize);
	else
		mPtr = (char*) Platform::AllocateMemory (inSize);
}


StMemory::StMemory(
	const StMemory	&inPointerBlock)
{
	UNUSED_PARAM(inPointerBlock)
}


StMemory::~StMemory()
{
	Dispose ();
}


StMemory&
StMemory::operator = (
	const StMemory	&inPointerBlock)
{
	UNUSED_PARAM(inPointerBlock)

	return *this;
}


void
StMemory::Adopt(
	char* 	inPtr)
{
	if (inPtr != mPtr)
	{
		Dispose ();
	
		mIsOwner = (inPtr != NULL);
		mPtr	 = inPtr;
	}
}


char*
StMemory::Release() const
{
	mIsOwner = false;
	return mPtr;
}


void
StMemory::Dispose()
{
	if (mIsOwner && (mPtr != NULL))
	{
		Platform::DisposeMemory (mPtr);
	}
	
	mIsOwner = false;
	mPtr	 = NULL;
}



StMemoryMapper::StMemoryMapper (const void* memory, long size) :
	fMemory (memory)
{
	if (fMemory)
		Memory::MapPhysicalMemory (fMemory, size);
}

StMemoryMapper::~StMemoryMapper (void)
{
	if (fMemory)
		Memory::UnmapPhysicalMemory (fMemory);
}


void*	Platform_AllocateMemory (size_t size)
{
	return Platform::AllocateMemory (size);
}


void*	Platform_ReallocMemory	(void* p, size_t size)
{
	return Platform::ReallocMemory (p, size);
}


void	Platform_DisposeMemory	(void* p)
{
	Platform::DisposeMemory (p);
}


StWordSwapper::StWordSwapper (void* memory, long length) :
	fMemory (memory),
	fLength (length)
{
	::ByteswapWords (fMemory, fLength);
}

StWordSwapper::~StWordSwapper (void)
{
	::ByteswapWords (fMemory, fLength);
}


/***********************************************************************
 *
 * FUNCTION:	PrvFormObjectHasValidSize
 *
 * DESCRIPTION: Determine whether or not a form object's size is valid.
 *
 * PARAMETERS:	frm - form containing the object
 *
 *				objType - the object's type
 *
 *				objIndex - the object's index in the form
 *
 *				bounds - the bounding rectangle around the object
 *
 * RETURNED:	True if the object's size is valid.
 *
 ***********************************************************************/

static Bool PrvFormObjectHasValidSize (FormPtr frm, FormObjectKind objType, UInt16 objIndex, 
									   RectangleType bounds)
{
	EmAssert (frm); 

	CEnableFullAccess	munge;	// Remove blocks on memory access.

	// Return valid right away if we have height and width

	if (bounds.extent.x > 0 &&
		bounds.extent.y > 0)
	{
		return true;
	}

	// Allow zero-sized gadgets and tables.  The former are often
	// used as dummy objects merely to hold references to custom
	// data.  The latter exist because there's no other way to
	// hide a table (there's no "usable" bit).

	if (objType == frmGadgetObj ||
		objType == frmTableObj)
	{
		return true;
	}

	// Allow zero-width (but not zero-height) popup triggers.

	if (objType == frmControlObj)
	{
		emuptr	ctrlPtr	= (emuptr) ::FrmGetObjectPtr (frm, objIndex);
		uint8	style	= EmMemGet8 (ctrlPtr + offsetof (ControlType, style));

		if (style == popupTriggerCtl)
		{
			if (bounds.extent.x == 0 &&
				bounds.extent.y > 0)
			{
				return true;
			}
		}
	}

	// Allow zero-height lists if the number of objects in them is zero.

	if (objType == frmListObj)
	{
		emuptr	listPtr = (emuptr) FrmGetObjectPtr (frm, objIndex);
		Int16	numItems = ::LstGetNumberOfItems ((ListType*)listPtr);

		if (numItems == 0)
		{
			if (bounds.extent.x > 0 &&
				bounds.extent.y == 0)
			{
				return true;
			}
		}
	}

	// Failed all the special cases.

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	PrvFormObjectIsOffscreen
 *
 * DESCRIPTION: Determine whether or not an object is off-screen.
 *
 * PARAMETERS:	bounds - the bounding rectangle of the object
 *
 *				winWidth - width of the window
 *
 *				winHeight - width of the window
 *
 * RETURNED:	True if the object is off-screen.
 *
 ***********************************************************************/

static Bool PrvFormObjectIsOffscreen (RectangleType bounds, Int16 winWidth, Int16 winHeight)
{
	// Ignore objects with a zero extent

	if (bounds.extent.x <= 0 ||
		bounds.extent.y <= 0) 
		return false;

	return (bounds.topLeft.x >= winWidth ||
			bounds.topLeft.y >= winHeight ||
			bounds.topLeft.x + bounds.extent.x <= 0 ||
			bounds.topLeft.y + bounds.extent.y <= 0);
}


/***********************************************************************
 *
 * FUNCTION:	PrvFormObjectIsUsable
 *
 * DESCRIPTION: Determines whether or not a form object is usable, ie 
 *				should be considered part of the UI.
 *
 * PARAMETERS:	frmP - form containing the object in question
 *
 *				index - index of the object
 *
 *				kind - type of the object
 *
 * RETURNED:	True if the object is usable.
 *
 ***********************************************************************/


#define ControlAttrType_usable			0x8000	// set if part of ui 


static Bool PrvFormObjectIsUsable (FormPtr frmP, uint16 index, FormObjectKind kind)
{
	EmAssert (frmP);

	CEnableFullAccess	munge;	// Remove blocks on memory access.

	emuptr objP = (emuptr)::FrmGetObjectPtr (frmP, index);

	if (objP == EmMemNULL)
	{
		return false;
	}

	switch (kind)
	{
		// Objects with special 'usable' flag:

		case frmFieldObj:
		{
			FieldAttrType attr;
			::FldGetAttributes ((FieldType*)objP, &attr);

			return attr.usable == true;
		}

		case frmControlObj:
		{
			EmAliasControlType<PAS>	control ((emuptr)objP);

			return control.attr.flags & ControlAttrType_usable;
		}

		case frmGadgetObj:
		{
			EmAliasFormGadgetType<PAS> gadget ((emuptr)objP);

			return gadget.attr.flags & ControlAttrType_usable;
		}

		case frmListObj:
		{
			EmAliasListType<PAS> list ((emuptr)objP);

			return list.attr.flags & ControlAttrType_usable;
		}

		case frmScrollBarObj:
		{
			EmAliasScrollBarType<PAS> scrollbar ((emuptr)objP);

			return scrollbar.attr.flags & ControlAttrType_usable;
		}

		// Objects assumed to be usable:

		case frmTableObj:
		case frmGraffitiStateObj:

			return true;

		// Objects assumed to be unusable:

		case frmBitmapObj:
		case frmLineObj:
		case frmFrameObj:
		case frmRectangleObj:
		case frmLabelObj:
		case frmPopupObj:
		case frmTitleObj:

			return false;
	}

	// Everything else:

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	ValidateFormObjects
 *
 * DESCRIPTION: Iterate over all the objects in a form and complain
 *				if we find one that is invalid in some way.
 *
 * PARAMETERS:	frm - the form to validate
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void ValidateFormObjects (FormPtr frm)
{
	if (!frm)
		return;

	Int16 winWidth, winHeight;
	::WinGetDisplayExtent (&winWidth, &winHeight);

	UInt16	numObjects = ::FrmGetNumberOfObjects (frm);
	for (UInt16 objIndex = 0; objIndex < numObjects; ++objIndex)
	{
		FormObjectKind	objType	= ::FrmGetObjectType (frm, objIndex);
		UInt16			objID	= ::FrmGetObjectId (frm, objIndex);
		
		switch (objType)
		{
			case frmBitmapObj:
			case frmLineObj:
			case frmFrameObj:
			case frmRectangleObj:
			case frmLabelObj:
			case frmTitleObj:
			case frmPopupObj:
				// do nothing for these
				break;
			
			default:
			{
				// Check for completely offscreen objects.
				// (The jury is still out on partially offscreen objects.)
				RectangleType	bounds;
				::FrmGetObjectBounds (frm, objIndex, &bounds);

				if (!::PrvFormObjectHasValidSize (frm, objType, objIndex, bounds))
				{
					// Report any errors.  For now, don't report errors on 1.0
					// devices.  They may not follow the rules, either.  In
					// particular, someone noticed that the Graffiti state
					// indicator has a size of 0,0.

					if (EmPatchState::OSMajorVersion () > 1)
					{
						EmAssert (gSession);
						gSession->ScheduleDeferredError (
							new EmDeferredErrSizelessObject (objID, bounds));
					}
				}
				else if (::PrvFormObjectIsOffscreen (bounds, winWidth, winHeight))
				{
					if (EmPatchState::OSMajorVersion () > 1)
					{
						EmAssert (gSession);
						gSession->ScheduleDeferredError (
							new EmDeferredErrOffscreenObject (objID, bounds));
					}
				}
			}
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	CollectOKObjects
 *
 * DESCRIPTION: Iterate over the objects in a form and make a list of
 *				the ones that are fair game for tapping on. Exclude
 *				objects that aren't interactive (ie a label), aren't
 *				usable, or aren't valid.
 *
 * PARAMETERS:	frm - the form in question
 *
 *				okObjects - vector list of objects that are deemed 'ok'.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void CollectOKObjects (FormPtr frm, vector<UInt16>& okObjects)
{
	if (!frm)
		return;

	Int16 winWidth, winHeight;
	::WinGetDisplayExtent (&winWidth, &winHeight);

	UInt16	numObjects = ::FrmGetNumberOfObjects (frm);
	for (UInt16 objIndex = 0; objIndex < numObjects; ++objIndex)
	{
		FormObjectKind	objType	= ::FrmGetObjectType (frm, objIndex);

		switch (objType)
		{
			case frmBitmapObj:
			case frmLineObj:
			case frmFrameObj:
			case frmRectangleObj:
			case frmLabelObj:
			case frmTitleObj:
			case frmPopupObj:
				// do nothing for these
				break;
			
			default:
			{
				RectangleType	bounds;
				::FrmGetObjectBounds (frm, objIndex, &bounds);

				if (!::PrvFormObjectHasValidSize (frm, objType, objIndex, bounds) ||
					 ::PrvFormObjectIsOffscreen (bounds, winWidth, winHeight) ||
					!::PrvFormObjectIsUsable (frm, objIndex, objType))
				{
					break;
				}
				else
				{
					okObjects.push_back (objIndex);
				}
				break;
			}
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ PinRectInRect
// ---------------------------------------------------------------------------

Bool PinRectInRect (EmRect& inner, const EmRect& outer)
{
	// Do this in a way such that if the incoming rectangle is
	// taller or wider than gdRect that we ensure we see the
	// top and left.

	Bool	result = false;

	if (inner.fBottom > outer.fBottom)
	{
		inner.Offset (0, outer.fBottom - inner.fBottom);	// Move it up
		result = true;
	}

	if (inner.fRight > outer.fRight)
	{
		inner.Offset (outer.fRight - inner.fRight, 0);		// Move it left
		result = true;
	}

	if (inner.fTop < outer.fTop)
	{
		inner.Offset (0, outer.fTop - inner.fTop);			// Move it down
		result = true;
	}

	if (inner.fLeft < outer.fLeft)
	{
		inner.Offset (outer.fLeft - inner.fLeft, 0);		// Move it right
		result = true;
	}

	return result;
}


// For "data" databases, we look to see if they have the following structure
// in their appInfo block. If so, we grab the icon and version string from here.
// WARNING: This structure contains variable size fields and must be generated
//	and parsed at programmatically.
#define 	dataAppInfoSignature 'lnch'
#define 	dataAppInfoVersion	3  // current version of header

#include "PalmPack.h"

struct DataAppInfoType
{
	UInt32			signature;					// must be 'lnch' (0x6C6E6338)
	UInt16			hdrVersion; 				// version of this header - must be 3 
	UInt16			encVersion; 				// encoder version
	UInt16			verStrWords;				// length of version string array that 
												//	follows in 16-bit words.
	//UInt8			verStr[verStrWords];		// 0 terminated version string with
												//	possible extra NULL byte at end for
												//	padding
														
	//--- The title is only present in version 2 or later
	UInt16			titleWords; 				// length of title string array that follows
												//	 in 16-bit words.
	//UInt8			title[titleWords];			// 0 terminated title string with possible
												//	extra NULL at end for padding. 


	UInt16			iconWords;					// length of icon data that follows in 16-bit
												//	 words.
	//UInt8			icon[iconWords];			// icon in "BitmapType" format with possible NULL
												//	byte at end for even UInt16 padding
	UInt16			smIconWords;				// length of small icon data that follows in
												//	 16-bit words
	//UInt8			smIcon[smIconWords];		// small icon in "BitmapType" format with
												// possible NULL byte at end for even UInt16
												//	padding
												//--------- Version 2 Fields ------------
};

// Size of appInfo block. 
#define 	dataAppInfoVersionSize (sizeof(DataAppInfoType))

#include "PalmPackPop.h"


/***********************************************************************
 *
 * FUNCTION:	IsExecutable
 *
 * DESCRIPTION: Returns whether or not the given database contains an
 *				application in which case we want to present only
 *				the database with the most recent version number (in
 *				case there's more than one database with this one's
 *				type and creator).
 *
 * PARAMETERS:	dbType - type of the database
 *
 *				dbCreator - creator signature of the database
 *
 *				dbAttrs - attributes of the database
 *
 * RETURNED:	True if this database is an executable.
 *
 ***********************************************************************/

Bool IsExecutable (UInt32 dbType, UInt32 dbCreator, UInt16 dbAttrs)
{
	UNUSED_PARAM(dbCreator)
	UNUSED_PARAM(dbAttrs)

	if (dbType == sysFileTApplication)
		return true;

	if (dbType == sysFileTPanel)
		return true;

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	IsVisible
 *
 * DESCRIPTION: Returns whether or not the given database represents
 *				an item we want to display.
 *
 * PARAMETERS:	dbType - type of the database
 *
 *				dbCreator - creator signature of the database
 *
 *				dbAttrs - attributes of the database
 *
 * RETURNED:	True if we should include this database in our list.
 *
 ***********************************************************************/

Bool IsVisible (UInt32 dbType, UInt32 dbCreator, UInt16 dbAttrs)
{
	UNUSED_PARAM(dbCreator)

	// Don't show anything concerning the Launcher
	// (That comment and the following commented out code was from the
	// Launcher application. I've take it out so that we can run
	// Gremlins over the Launcher).
//	if (dbCreator == sysFileCLauncher)
//		return false;

	// The following test can come and go.	Currently, it's here
	// so that things like Clipper don't show up in the list (just
	// as it doesn't show up in the Launcher).	However, there may
	// be time when we want to show it.  An example would be in
	// an experiemental version of the New Gremlin dialog that
	// separated apps and documents.  Selecting an app in one list
	// would display a list of its documents in the other list.  In
	// that case, we'd need to show clipper in order to be able to
	// display its documents.

	// OK, the test is now gone.  From Scott Johnson:
	//
	//		The New Gremlin list doesn't show apps with the dmHdrAttrHidden attribute.
	//		This is a problem for mine, which is a sort of runtime forms engine.  The
	//		runtime is a hidden app and the user-built apps are visible.  The user app
	//		launches by just doing an app switch to the runtime.  (Sort of a launchable
	//		database concept for pre-3.2 systems.)  To Gremlin this, both apps need to
	//		be selected in the New Gremlin list.  But the hidden one isn't shown.  Oops.


//	if (dbAttrs & dmHdrAttrHidden)
//		return false;

	if (dbAttrs & dmHdrAttrLaunchableData)
		return true;

	if (dbType == sysFileTApplication)
		return true;

	if (dbType == sysFileTPanel)
		return true;

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	GetLoadableFileList
 *
 * DESCRIPTION:	Scan the given directory for files that can be loaded
 *				into the Palm OS environment and add them to the
 *				given collection object.  Loadable files are PRCs,
 *				PDBs, and PQAs.
 *
 * PARAMETERS:	directoryName - name of the directory to search.
 *					This directory is assumed to be in the emulator's
 *					containing directory.
 *
 *				fileList - collection to receive the found files.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void GetLoadableFileList (string directoryName, EmFileRefList& fileList)
{
	// Get the application's directory.

	EmDirRef	poserDir = EmDirRef::GetEmulatorDirectory ();

	// Get the directory we're asked to look into.

	EmDirRef	searchDir (poserDir, directoryName);
	if (!searchDir.Exists ())
		return;

	// Get all of its children.

	EmFileRefList	children;
	searchDir.GetChildren (&children, NULL);

	// Filter for the types that we want.

	EmFileRefList::iterator	iter = children.begin ();
	while (iter != children.end ())
	{
		if (iter->IsType (kFileTypePalmApp) ||
			iter->IsType (kFileTypePalmDB) ||
			iter->IsType (kFileTypePalmQA))
		{
			fileList.push_back (*iter);
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	GetFileContents
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	
 *
 * RETURNED:	
 *
 ***********************************************************************/

void GetFileContents (const EmFileRef& file, Chunk& contents)
{
	EmStreamFile	stream (file, kOpenExistingForRead);
	int32			length = stream.GetLength ();

	contents.SetLength (length);

	stream.GetBytes (contents.GetPointer (), length);
}


/***********************************************************************
 *
 * FUNCTION:	AppGetExtraInfo
 *
 * DESCRIPTION: Returns additional information on the application.	This
 *				information is usually pretty expensive to get, so we
 *				defer getting it until as late as possible.
 *
 *				This function is derived from one in the Launcher with
 *				the same name.	That function returned a lot more
 *				information.  This one has been simplified to return
 *				only the application's (or special database's) name.
 *
 * PARAMETERS:	infoP - pointer to the DatabaseInfo struct for the application
 *				we need to get more information on.
 *
 * RETURNED:	Any errors encountered while processing this request.
 *				The requested information is returned back in the DatabaseInfo
 *				struct.
 *
 ***********************************************************************/

static Err AppGetExtraInfo (DatabaseInfo* infoP)
{
	Err err = errNone;

	infoP->name[0] = 0;

	//====================================================================
	// If it's a resource database, we must open it to get the appName
	//====================================================================
	if (infoP->dbAttrs & dmHdrAttrResDB)
	{
		DmOpenRef	appDB = NULL;
		MemHandle	strH;
		
		// Open database 
		appDB = DmOpenDatabase (infoP->cardNo, infoP->dbID, dmModeReadOnly);
		if (appDB == NULL)
		{
			err = DmGetLastErr ();
			goto Exit;
		}

		//...............................
		// Get app name if we don't already have it.
		//...............................
		strH = DmGet1Resource (ainRsc, ainID);
		
		// copy launcher name, if present
		if (strH != NULL)
		{
			emuptr strP = (emuptr) MemHandleLock (strH);
			EmMem_strcpy (infoP->name, strP);
			MemHandleUnlock (strH);
			DmReleaseResource (strH);
		}

		::DmCloseDatabase (appDB);
	} // if resource database

	//====================================================================
	// If it's a record database, we look in the appInfo block.
	//====================================================================
	else
	{
		LocalID 	appInfoID;
		MemHandle	appInfoH = 0;
		MemPtr 	appInfoP = 0;
		emuptr 	specialInfoP;
		emuptr 	bP;
		UInt16		verStrWords, titleWords;

		// Look for app info
		err = DmDatabaseInfo (infoP->cardNo, infoP->dbID, 0,
					0, 0, 0, 0, 0, 0, &appInfoID, 0, 0, 0);

		if (!err && appInfoID)
		{
			// Get handle (if RAM based) and ptr to app Info
			if (MemLocalIDKind (appInfoID) == memIDHandle)
			{
				appInfoH = (MemHandle) MemLocalIDToGlobal (appInfoID, infoP->cardNo);
				appInfoP = MemHandleLock (appInfoH);
			}
			else
			{
				appInfoP = MemLocalIDToGlobal(appInfoID, infoP->cardNo);
			}

			// See if this is the special launcher info and if so, get the icons
			//	out of that.
			specialInfoP = (emuptr) appInfoP;
			DataAppInfoType specialInfo;

			specialInfo.signature = EmMemGet32 (specialInfoP + offsetof (DataAppInfoType, signature));
			specialInfo.hdrVersion = EmMemGet16 (specialInfoP + offsetof (DataAppInfoType, hdrVersion));
			specialInfo.encVersion = EmMemGet16 (specialInfoP + offsetof (DataAppInfoType, encVersion));

			if (MemPtrSize (appInfoP) >= dataAppInfoVersionSize &&
				specialInfo.signature == dataAppInfoSignature &&
				specialInfo.hdrVersion >= dataAppInfoVersion)
			{
				// Get ptr to version string
				bP = specialInfoP + offsetof (DataAppInfoType, verStrWords);
				verStrWords = EmMemGet16 (bP);
				bP += sizeof(UInt16);
				bP += verStrWords * sizeof(UInt16);

				// Get ptr to name string
				titleWords = EmMemGet16 (bP);
				bP += sizeof(UInt16);
				if (titleWords)
				{
					EmMem_strcpy (infoP->name, bP);
				}
			} // If valid appInfo

			if (appInfoH)
			{
				MemHandleUnlock(appInfoH);
			}
		} // if (!err && appInfoID)
	} // Record Database. 

Exit:

	// If no luck getting the visible name, put in default
	if (infoP->name[0] == 0)
	{
		// Get DB name
		strcpy (infoP->name, infoP->dbName);
	}

	return err; 
}



/***********************************************************************
 *
 * FUNCTION:	AppCompareDataBaseNames
 *
 * DESCRIPTION: sort() callback function to sort entries by name.
 *
 * PARAMETERS:	a, b - references to two DatabaseInfo's to compare.
 *
 * RETURNED:	True if a should appear before b, false otherwise.
 *
 ***********************************************************************/

static bool AppCompareDataBaseNames (const DatabaseInfo& a, const DatabaseInfo& b)
{
	return _stricmp (a.name, b.name) < 0;
}


/***********************************************************************
 *
 * FUNCTION:	GetDatabases
 *
 * DESCRIPTION: Collects the list of entries that should be displayed
 *				in the New Gremlin dialog box.
 *
 *				This function is derived from the Launcher function
 *				AppCreateDataBaseList, as rewritten by Ron for the
 *				3.2 ROMs.
 *
 * PARAMETERS:	dbList -- collection into which we store the found
 *				DatabaseInfo entries.
 *
 * RETURNED:	nothing.
 *
 ***********************************************************************/

void GetDatabases (DatabaseInfoList& dbList, Bool applicationsOnly)
{
	UInt16			cardNo;
	UInt16			numCards;
	UInt16			numDBs;
	Int16			dbIndex;		// UInt16 results in a bug
	LocalID			dbID;
	Err				err = errNone;
	DatabaseInfo	dbInfo;
	Boolean			needToAddNewEntry;

	//=======================================================================
	// Cycle through all databases in the ROM and RAM and place them into our list.
	//=======================================================================
	numCards = ::MemNumCards ();
	for (cardNo = 0; cardNo < numCards; ++cardNo)
	{
		numDBs = ::DmNumDatabases (cardNo);

		//---------------------------------------------------------------
		// Loop through databases on this card, DmGetDatabase() returns ROM 
		// databases first, followed by RAM databases. 
		//---------------------------------------------------------------
		for (dbIndex = 0; dbIndex < numDBs; ++dbIndex)
		{
			//--------------------------------------------------------
			// Get info on the next database and see if it should be visible.
			//--------------------------------------------------------
			dbID = ::DmGetDatabase (cardNo, dbIndex);
			err = ::DmDatabaseInfo (
						cardNo,
						dbID,
						dbInfo.dbName,		/*nameP*/
						&dbInfo.dbAttrs,
						&dbInfo.version,
						NULL,				/*create date*/
						&dbInfo.modDate,
						NULL,				/*backup date*/
						NULL,				/*modNum*/
						NULL,				/*appInfoID*/
						NULL,				/*sortInfoID*/
						&dbInfo.type,
						&dbInfo.creator);

			Errors::ThrowIfPalmError (err);


			// If it's not supposed to be visible, skip it
			if (applicationsOnly && !::IsVisible (dbInfo.type, dbInfo.creator, dbInfo.dbAttrs))
			{
				continue;
			}

			//--------------------------------------------------------------
			// Save info on this database
			//--------------------------------------------------------------
			dbInfo.dbID = dbID;
			dbInfo.cardNo = cardNo;

			//--------------------------------------------------------------
			// If it's an executable, make sure it's the most recent version in our
			// list
			//--------------------------------------------------------------
			needToAddNewEntry = true;
			if (applicationsOnly && ::IsExecutable (dbInfo.type, dbInfo.creator, dbInfo.dbAttrs))
			{
				// Search for database of same type and creator and check version
				DatabaseInfoList::iterator	thisIter = dbList.begin ();
				while (thisIter != dbList.end ())
				{
					if ((*thisIter).type == dbInfo.type &&
						(*thisIter).creator == dbInfo.creator)
					{
						// If this new one is a newer or same version than the previous one, 
						// replace the previous entry. Checking for == version allows RAM
						// executables to override ROM ones. 
						if (dbInfo.version >= (*thisIter).version)
						{
							::AppGetExtraInfo (&dbInfo);
							*thisIter = dbInfo;
						}

						// Since there's already an item with this type/creator
						// already in the list, there's no need to add another one.
						needToAddNewEntry = false;

						break;
					}

					++thisIter;
				}
			}


			//--------------------------------------------------------------
			// If we still need to add this entry, do so now.
			//--------------------------------------------------------------
			if (needToAddNewEntry)
			{
				::AppGetExtraInfo (&dbInfo);
				dbList.push_back (dbInfo);
			}
		} // for (dbIndex = 0; dbIndex < numDBs; dbIndex++)
	} // for (cardNo = 0; cardNo < MemNumCards(); cardNo++)


	//===========================================================================
	// Sort the list by name
	//===========================================================================
	// Sort the databases by their name.
	sort (dbList.begin (), dbList.end (), AppCompareDataBaseNames);
}



/***********************************************************************
 *
 * FUNCTION:	InstallCalibrationInfo
 *
 * DESCRIPTION: Sets the pen calibration info to be "perfect": no
 *				translation or scaling.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void InstallCalibrationInfo (void)
{
	// Open the preferences database.  If the new version of PrefOpenPreferenceDB
	// exists, then call it.  Otherwise, call the old version.	We can't just
	// unconditionally call the old version, as that has a bug in the newer
	// ROMs that causes it to create the database incorrectly if it doesn't
	// already exist.

	DmOpenRef	dbP;
	if (EmLowMem::TrapExists (sysTrapPrefOpenPreferenceDB))
		dbP = ::PrefOpenPreferenceDB (false);
	else
		dbP = ::PrefOpenPreferenceDBV10 ();

	if (dbP)
	{
		// Get the calibration information.

		MemHandle	resourceH = ::DmGetResource (sysResTSysPref, sysResIDSysPrefCalibration);

		// If that information doesn't exist, go about creating it.

		if (!resourceH)
		{
			resourceH = ::DmNewResource (dbP, sysResTSysPref, sysResIDSysPrefCalibration,
						4 * sizeof(UInt16));
		}

		if (resourceH)
		{
			// Write in the calibration information.  The information has the
			// following format and values:
			//
			//		scaleX	: 256
			//		scaleY	: 256
			//		offsetX :	0
			//		offsetY :	0
			//
			// We encode that data here as a string of bytes to avoid endian problems.

			MemPtr resP = ::MemHandleLock (resourceH);

			unsigned char data[] = { 1, 0, 1, 0, 0, 0, 0, 0 };

			::DmWrite (resP, 0, data, 4 * sizeof (UInt16));

			::MemHandleUnlock (resourceH);
			::DmReleaseResource (resourceH);
		}

		::DmCloseDatabase (dbP);
	}
}


/***********************************************************************
 *
 * FUNCTION:	ResetCalibrationInfo
 *
 * DESCRIPTION: Sets the pen calibration info to be "perfect": no
 *				translation or scaling.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void ResetCalibrationInfo (void)
{
	// Reset the pen calibration info by calling PenCalibrate with the right
	// parameters.  Unfortunately, due presumably to division rounding errors,
	// doing this just once doesn't necessarily get the scaling and offset
	// values exactly.  However, making a few calls to PenCalibrate seems
	// to get us to home in on the perfect calibration values.

	Bool	perfect = false;

	for (int kk = 0; kk < 3 && !perfect; ++kk)
	{
		#define target0X	10				// top left
		#define target0Y	10
		#define target1X	(160-10)		// bottom right
		#define target1Y	(160-10)

		Err			err;
		PointType	digPoints[2];
		PointType	scrPoints[2];

		scrPoints[0].x = target0X;
		scrPoints[0].y = target0Y;
		scrPoints[1].x = target1X;
		scrPoints[1].y = target1Y;

		digPoints[0].x = 0x100 - target0X;
		digPoints[0].y = 0x100 - target0Y;
		digPoints[1].x = 0x100 - target1X;
		digPoints[1].y = 0x100 - target1Y;

		err = ::PenRawToScreen(&digPoints[0]);
		err = ::PenRawToScreen(&digPoints[1]);
		err = ::PenCalibrate(&digPoints[0], &digPoints[1], &scrPoints[0], &scrPoints[1]);

		DmOpenRef	dbP;
		if (EmLowMem::TrapExists (sysTrapPrefOpenPreferenceDB))
			dbP = ::PrefOpenPreferenceDB (false);
		else
			dbP = ::PrefOpenPreferenceDBV10 ();

		if (dbP)
		{
			MemHandle	resourceH = ::DmGetResource (sysResTSysPref, sysResIDSysPrefCalibration);

			if (resourceH)
			{
				MemPtr			resP = ::MemHandleLock (resourceH);
				unsigned char	perfect_pattern[] = { 1, 0, 1, 0, 0, 0, 0, 0 };

				perfect = (EmMem_memcmp ((void*) perfect_pattern, (emuptr) resP, 8) == 0);

				::MemHandleUnlock (resourceH);
				::DmReleaseResource (resourceH);
			}

			::DmCloseDatabase (dbP);
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	ResetClocks
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void ResetClocks (void)
{
	EmHAL::ResetTimer ();

	EmLowMem_SetGlobal (hwrCurTicks, 0);

	emuptr	sysKernelDataP = EmLowMem_GetGlobal (sysKernelDataP);
	EmMemPut32 (sysKernelDataP + 0x20, 0);

	EmHAL::ResetRTC ();
}


/***********************************************************************
 *
 * FUNCTION:	SetHotSyncUserName
 *
 * DESCRIPTION: Calls the Data Link Manager to set the user's HotSync
 *				name.  Many applications key off this name for things
 *				like copy protection, so we set this value when they
 *				boot up.
 *
 * PARAMETERS:	userNameP - the user's name.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

const UInt16	kMagicRefNum	= 0x666;	// See comments in HtalLibSendReply.

void SetHotSyncUserName (const char* userNameP)
{
	if (EmLowMem::GetTrapAddress (sysTrapDlkDispatchRequest) == EmMemNULL)
		return;

	if (!userNameP)
		return;

	size_t	userNameLen = strlen (userNameP) + 1;

	// If the name is too long, just return.  This should really only
	// happen if the user hand-edits the preference file to contain
	// a name that's too long.  The Preferences dialog box handler
	// checks as well, so the name shouldn't get too long from that path.

	if (userNameLen > dlkMaxUserNameLength + 1)
		return;

	// We need to prepare a command block for the DataLink Manager.
	// Define one large enough to hold all the data we'll pass in.
	//
	// The format of the data block is as follows:
	//
	//		[byte] DlpReqHeaderType.id			: Command request number (== dlpWriteUserInfo)
	//		[byte] DlpReqHeaderType.argc		: # of arguments for this command (== 1)
	//
	//		[byte] DlpTinyArgWrapperType.bID	: ID of first argument (== dlpWriteUserInfoReqArgID)
	//		[byte] DlpTinyArgWrapperType.bSize	: Size in bytes of first argument (== whatever)
	//
	//		[long] DlpWriteUserInfoReqHdrType.userID		: Not used here - set to zero
	//		[long] DlpWriteUserInfoReqHdrType.viewerID		: Not used here - set to zero
	//		[long] DlpWriteUserInfoReqHdrType.lastSyncPC	: Not used here - set to zero
	//		[8byt] DlpWriteUserInfoReqHdrType.lastSyncDate	: Not used here - set to zero
	//		[long] DlpWriteUserInfoReqHdrType.modFlags		: Bits saying what values are being set
	//		[byte] DlpWriteUserInfoReqHdrType.userNameLen	: Length of user name + NULL
	//
	//		[str ] userName

	char	buffer[ sizeof (DlpReqHeaderType) +
					sizeof (DlpTinyArgWrapperType) +
					sizeof (DlpWriteUserInfoReqHdrType) +
					dlpMaxUserNameSize] __attribute__((__aligned__(2)));

	// Get handy pointers to all of the above.
	DlpReqHeaderType*			reqHdr		= (DlpReqHeaderType*) buffer;
	DlpTinyArgWrapperType*		reqWrapper	= (DlpTinyArgWrapperType*) (((char*) reqHdr) + sizeof(DlpReqHeaderType));
	DlpWriteUserInfoReqHdrType* reqArgHdr	= (DlpWriteUserInfoReqHdrType*) (((char*) reqWrapper) + sizeof(DlpTinyArgWrapperType));
	char*						reqName		= ((char*) reqArgHdr) + sizeof (DlpWriteUserInfoReqHdrType);

	// Fill in request header
	reqHdr->id				= dlpWriteUserInfo;
	reqHdr->argc			= 1;

	// Fill in the request arg wrapper
	reqWrapper->bID 		= (UInt8) dlpWriteUserInfoReqArgID;
	reqWrapper->bSize		= (UInt8) (sizeof (*reqArgHdr) + userNameLen);

	// Fill in request arg header
	reqArgHdr->modFlags 	= dlpUserInfoModName;
	reqArgHdr->userNameLen	= userNameLen;

	// Copy in the user name.
	strcpy (reqName, userNameP);

	// Build up a session block to hold the command block.
	DlkServerSessionType	session;
	memset (&session, 0, sizeof (session));
	session.htalLibRefNum	= kMagicRefNum;	// See comments in HtalLibSendReply.
	session.gotCommand		= true;
	session.cmdLen			= sizeof (buffer);
	session.cmdP			= buffer;

	// For simplicity, byteswap here so that we don't have to reparse all
	// that above data in DlkDispatchRequest.

	Canonical (reqHdr->id);
	Canonical (reqHdr->argc);

	Canonical (reqWrapper->bID);
	Canonical (reqWrapper->bSize);

	Canonical (reqArgHdr->modFlags);
	Canonical (reqArgHdr->userNameLen);

	// Patch up cmdP and map in the buffer it points to.

	StMemoryMapper	mapper (session.cmdP, session.cmdLen);
	session.cmdP = (void*) EmBankMapped::GetEmulatedAddress (session.cmdP);

	// Finally, install the name.
	/*Err err =*/ DlkDispatchRequest (&session);

#if 0
	ULong	lastSyncDate;
	char	userName[dlkUserNameBufSize];
	Err 	err = DlkGetSyncInfo(0/*succSyncDateP*/, &lastSyncDate, 0/*syncStateP*/,
								userName, 0/*logBufP*/, 0/*logLenP*/);
#endif
}


/***********************************************************************
 *
 * FUNCTION:    SeparateList
 *
 * DESCRIPTION: Break up a comma-delimited list of items, returning the
 *				pieces in a StringList.
 *
 * PARAMETERS:  stringList - the StringList to receive the broken-up
 *					pieces of the comma-delimited list.  This collection
 *					is *not* first cleared out, so it's possible to add
 *					to the collection with this function.
 *
 *				str - the string containing the comma-delimited items.
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void SeparateList (StringList& stringList, string str, char delimiter)
{
	string::size_type	offset;

	while ((offset = str.find (delimiter)) != string::npos)
	{
		string	nextElement = str.substr (0, offset);
		str = str.substr (offset + 1);
		stringList.push_back (nextElement);
	}

	stringList.push_back (str);
}


/***********************************************************************
 *
 * FUNCTION:	RunLengthEncode
 *
 * DESCRIPTION: Pack data according to the scheme used in QuickDraw's
 *				PackBits routine.  This is the format, according to
 *				Macintosh Technote 1023:
 *
 *				The first byte is a flag-counter byte that specifies
 *				whether or not the following data is packed, and the
 *				number of bytes involved.
 *
 *				If this first byte is a negative number, the following
 *				data is packed and the number is a zero-based count of
 *				the number of times the data byte repeats when expanded.
 *				There is one data byte following the flag-counter byte
 *				in packed data; the byte after the data byte is the next
 *				flag-counter byte.
 *
 *				If the flag-counter byte is a positive number, then the
 *				following data is unpacked and the number is a zero-based
 *				count of the number of incompressible data bytes that
 *				follow. There are (flag-counter+1) data bytes following
 *				the flag-counter byte. The byte after the last data byte
 *				is the next flag-counter byte.
 *
 *				Consider the following example:
 *
 *				Unpacked data:
 *
 *					AA AA AA 80 00 2A AA AA AA AA 80 00
 *					2A 22 AA AA AA AA AA AA AA AA AA AA
 *
 *				After being packed by PackBits:
 *
 *					FE AA			; (-(-2)+1) = 3 bytes of the pattern $AA
 *					02 80 00 2A 	; (2)+1 = 3 bytes of discrete data
 *					FD AA			; (-(-3)+1) = 4 bytes of the pattern $AA
 *					03 80 00 2A 22	; (3)+1 = 4 bytes of discrete data
 *					F7 AA			; (-(-9)+1) = 10 bytes of the pattern $AA
 *
 *				or
 *
 *					FE AA 02 80 00 2A FD AA 03 80 00 2A 22 F7 AA
 *					*	  * 		  * 	*			   *
 *
 *				The bytes with the asterisk (*) under them are the
 *				flag-counter bytes. PackBits packs the data only when
 *				there are three or more consecutive bytes with the same
 *				data; otherwise it just copies the data byte for byte
 *				(and adds the count byte).
 *
 * PARAMETERS:	srcPP - pointer to the pointer to the source bytes.  The
 *					referenced pointer gets udpated to point past the
 *					last byte included the packed output.
 *
 *				dstPP - pointer to the pointer to the destination buffer.
 *					The referenced pointer gets updated to point past
 *					the last byte stored in the output buffer.
 *
 *				srcBytes - length of the buffer referenced by srcPP
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void RunLengthEncode (void** srcPP, void** dstPP, long srcBytes, long dstBytes)
{
	UNUSED_PARAM(dstBytes)

	enum { kBeginRun, kRepeatRun, kCopyRun };

	uint8* srcP = (uint8*) *srcPP;
	uint8* dstP = (uint8*) *dstPP;
	uint8* opP = NULL;
	long	sample[3] = { -1, -1, -1 }; // Type must be > uint8 so that it can hold -1.
	long	opCount = 0;
	long	state = kBeginRun;

	for (srcBytes += 1; srcBytes >= 0; --srcBytes)
	{
		sample[0] = sample[1];
		sample[1] = sample[2];
		sample[2] = -1;

		if (srcBytes > 1)
		{
			sample[2] = *srcP++;
		}

		switch (state)
		{
			case kBeginRun: // Determine whether or not to pack the bytes
				if (sample[2] == sample[0] && sample[2] == sample[1])
				{
					state		= kRepeatRun;
					opCount 	= -2;
				}
				else if (sample[0] != -1)
				{
					state		= kCopyRun;
					opCount 	= 0;
					opP 		= dstP++;
					*dstP++ 	= (uint8) sample[0];
				}
				break;

			case kRepeatRun:	// We're packing bytes
				if (sample[2] == sample[1])
				{
					--opCount;

					if (opCount > -127)
					{
						break;
					}

					sample[2]	= -1;
				}

				sample[1]	= -1;
				*dstP++ 	= (uint8) opCount;
				*dstP++ 	= (uint8) sample[0];
				state		= kBeginRun;
				break;

			case kCopyRun:	// We're copying bytes
				if (sample[0] != sample[1] || sample[0] != sample[2])
				{
					*dstP++ = (uint8) sample[0];
					++opCount;

					if (opCount >= 127)
					{
						*opP	= (uint8) opCount;
						state	= kBeginRun;
						break;
					}
				}
				else
				{
					*opP	= (uint8) opCount;
					state	= kRepeatRun;
					opCount = -2;
				}
				break;

			default:
				EmAssert (false);
				break;
		}
	}

	if (state == kCopyRun)
	{
		*opP = (uint8) opCount;
	}

	*srcPP = (void*) srcP;
	*dstPP = (void*) dstP;
}


/***********************************************************************
 *
 * FUNCTION:	RunLengthDecode
 *
 * DESCRIPTION: Decode the data packed by RunLengthEncode.
 *
 * PARAMETERS:	srcPP -
 *
 *				dstPP -
 *
 *				dstBytes -
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void RunLengthDecode (void** srcPP, void** dstPP, long srcBytes, long dstBytes)
{
	UNUSED_PARAM(srcBytes)

	int8* 	srcP	= (int8*) *srcPP;
	int8* 	dstP	= (int8*) *dstPP;
	int8* 	limitP	= dstP + dstBytes;

	while (dstP < limitP)
	{
		int op = *srcP++;
		if (op == -128)
		{
			// Nothing
		}
		else if (op >= 0)
		{
			int count = op + 1;
			do
			{
				*dstP++ = *srcP++;
			} while (--count);
		}
		else
		{
			int 	count = 1 - op;
			uint8	fillData = *srcP++;
			do
			{
				*dstP++ = fillData;
			} while (--count);
		}
	}

	*srcPP = (void*) srcP;
	*dstPP = (void*) dstP;
}


/***********************************************************************
 *
 * FUNCTION:	RunLengthWorstSize
 *
 * DESCRIPTION: Calculate the largest buffer needed when packing a
 *				buffer "srcBytes" long.  The algorithm is based on
 *				that found in Macintosh Technote 1023.
 *
 * PARAMETERS:	srcBytes - number of bytes in the buffer to be encoded.
 *
 * RETURNED:	Largest buffer size needed to encode source buffer.
 *
 ***********************************************************************/

long RunLengthWorstSize (long srcBytes)
{
	long	maxDestBytes = (srcBytes + (srcBytes + 126) / 127);

	return maxDestBytes;
}


/***********************************************************************
 *
 * FUNCTION:	GzipEncode
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	srcPP - pointer to the pointer to the source bytes.  The
 *					referenced pointer gets udpated to point past the
 *					last byte included the packed output.
 *
 *				dstPP - pointer to the pointer to the destination buffer.
 *					The referenced pointer gets updated to point past
 *					the last byte stored in the output buffer.
 *
 *				srcBytes - length of the buffer referenced by srcPP
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void GzipEncode (void** srcPP, void** dstPP, long srcBytes, long dstBytes)
{
	gSrcP		= *srcPP;
	gDstP		= *dstPP;
	gSrcBytes	= srcBytes;
	gDstBytes	= dstBytes;
	gSrcOffset	= 0;
	gDstOffset	= 0;

	bytes_in = srcBytes;	// (for gzip internal debugging)

	ush 	attr = 0;		   /* ascii/binary flag */
	ush 	deflate_flags = 0; /* pkzip -es, -en or -ex equivalent */
	int 	method;

	clear_bufs ();

	read_buf		= &::PrvGzipReadProc;
	write_buf_proc	= &::PrvGzipWriteProc;

	bi_init (NO_FILE);
	ct_init (&attr, &method);
	lm_init (level, &deflate_flags);

	deflate ();

	// Perform a put_byte(0) to pad out the
	// compressed buffer.  gzip apparently can skid off the
	// end of the compressed data when inflating it, so we need
	// an extra zero.

	put_byte (0);

	flush_outbuf ();

	*srcPP = ((char*) gSrcP) + gSrcOffset;
	*dstPP = ((char*) gDstP) + gDstOffset;
}


/***********************************************************************
 *
 * FUNCTION:	GzipDecode
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	srcPP -
 *
 *				dstPP -
 *
 *				dstBytes -
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void GzipDecode (void** srcPP, void** dstPP, long srcBytes, long dstBytes)
{
	gSrcP		= *srcPP;
	gDstP		= *dstPP;
	gSrcBytes	= srcBytes;
	gDstBytes	= dstBytes;
	gSrcOffset	= 0;
	gDstOffset	= 0;

	clear_bufs ();

	read_buf		= &::PrvGzipReadProc;
	write_buf_proc	= &::PrvGzipWriteProc;

	inflate ();

	*srcPP = ((char*) gSrcP) + gSrcOffset;
	*dstPP = ((char*) gDstP) + gDstOffset;
}


/***********************************************************************
 *
 * FUNCTION:	GzipWorstSize
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	srcBytes - number of bytes in the buffer to be encoded.
 *
 * RETURNED:	Largest buffer size needed to encode source buffer.
 *
 ***********************************************************************/

long GzipWorstSize (long srcBytes)
{
	long	maxDestBytes = srcBytes * 2;

	return maxDestBytes;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGzipReadProc
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

int PrvGzipReadProc (char* buf, unsigned size)
{
	if (gSrcOffset == gSrcBytes)
		return EOF;

	if (size > (unsigned) (gSrcBytes - gSrcOffset))
		size = gSrcBytes - gSrcOffset;

	if (size > 0)
	{
		memcpy (buf, ((char*) gSrcP) + gSrcOffset, size);
		gSrcOffset += size;
	}

	return size;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGzipWriteProc
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

int PrvGzipWriteProc (char* buf, unsigned size)
{
	if (gDstOffset == gDstBytes)
		return EOF;

	if (size > (unsigned) (gDstBytes - gDstOffset))
		size = gDstBytes - gDstOffset;

	if (size > 0)
	{
		memcpy (((char*) gDstP) + gDstOffset, buf, size);
		gDstOffset += size;
	}

	return size;
}


/***********************************************************************
 *
 * FUNCTION:	StackCrawlStrings
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

void StackCrawlStrings (const EmStackFrameList& stackCrawl, StringList& stackCrawlStrings)
{
	EmStackFrameList::const_iterator	iter = stackCrawl.begin ();
	while (iter != stackCrawl.end ())
	{
		// Get the function name.

		char	funcName[256] = {0};
		::FindFunctionName (iter->fAddressInFunction, funcName, NULL, NULL, 255);

		// If we can't find the name, dummy one up.

		if (strlen (funcName) == 0)
		{
			sprintf (funcName, "<Unknown @ 0x%08lX>", iter->fAddressInFunction);
		}

		stackCrawlStrings.push_back (string (funcName));

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	StackCrawlString
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

string StackCrawlString (const EmStackFrameList& stackCrawl, long maxLen, Bool includeFrameSize, emuptr oldStackLow)
{
	StringList	strings;
	::StackCrawlStrings (stackCrawl, strings);

	string stackCrawlString;

	EmStackFrameList::const_iterator	iter = stackCrawl.begin ();
	StringList::const_iterator			s_iter = strings.begin ();

	while (iter != stackCrawl.end ())
	{
		// Catenate the function name to the built-up string.

		if (iter != stackCrawl.begin ())
		{
			stackCrawlString += ", ";
		}

		stackCrawlString += *s_iter;

		if (includeFrameSize)
		{
			// Get the stack size used by the function.

			char	stackSize[20];
			sprintf (stackSize, "%ld", iter->fA6 - oldStackLow);

			stackCrawlString += string ("(") + string (stackSize) + ")";
		}

		// If the string looks long enough, stop.

		if (maxLen > 0 && (long) stackCrawlString.size () > maxLen)
		{
			stackCrawlString += "...";
			break;
		}

		oldStackLow = iter->fA6;

		++iter;
		++s_iter;
	}

	return stackCrawlString;
}

#pragma mark -

static int kBitCount[16] =
{
	0, 1, 1, 2,
	1, 2, 2, 3,
	1, 2, 2, 3,
	2, 3, 3, 4
};

int CountBits (uint32 v)
{
	return	kBitCount[ (v >> 0) & 0x0F ] +
			kBitCount[ (v >> 4) & 0x0F ] +
			kBitCount[ (v >> 8) & 0x0F ] +
			kBitCount[ (v >> 12) & 0x0F ];
}


/***********************************************************************
 *
 * FUNCTION:	NextPowerOf2
 *
 * DESCRIPTION: Calculates the next power of two above the given number
 *				If the given number is already a power of two, it is
 *				returned.
 *
 * PARAMETERS:	n - probe number
 *
 * RETURNED:	Next power of two above the probe number, or the number
 *				itself if it is a power of two.
 *
 ***********************************************************************/

//	Seven implementations!	No waiting!

uint32 NextPowerOf2 (uint32 n)
{
	// Smear down the upper 1 bit to all bits lower than it.

	uint32 n2 = n;

	n2 |= n2 >> 1;
	n2 |= n2 >> 2;
	n2 |= n2 >> 4;
	n2 |= n2 >> 8;
	n2 |= n2 >> 16;

	// Now use itself to clear all the lower bits.

	n2 &= ~(n2 >> 1);

	// If n2 ends up being the same as what we started with, keep it.
	// Otherwise, we need to bump it by a factor of two (round up).

	if (n2 != n)
		n2 <<= 1;

	return n2;
}


#if 0
uint32 NextPowerOf2 (uint32 n)
{
	uint32 startn = n;
	uint32 prevn = 0;

	while (n)		// Loop until we're out of bits
	{
		prevn = n;	// Remember what we're starting with.  When "n"
					// reaches zero, prevn will hold the previous value,
					// which will have a single bit in it.	Since we're
					// whacking off bits from the bottom, this will be
					// the highest bit.
		n &= n - 1; // Mask off the low bit
	}

	// If prevn ends up being the same as what we started with, keep it.
	// Otherwise, we need to bump it by a factor of two.

	if (prevn != startn)
		prevn <<= 1;

	return prevn;
}
#endif


#if 0
	// This was my own first attempt.  Pretty lame...

uint32 NextPowerOf2 (uint32 x)
{
	// Figure out the next power-of-2 higher than or equal to 'x'. We do
	// this by continually shifting x to the left until we get a '1' in
	// the upper bit. At the same time, we shift 0x80000000 to the right.
	// When we find that '1' in the upper bit, the shifted 0x80000000
	// pattern should hold our power-of-two.
	//
	// That approach is good for finding the next power-of-2 higher than
	// a given value, so in order to deal with the 'or equal to' part
	// of the task, we decrement x by 1 before embarking on this adventure.
	//
	// This function'll fail for x == 0 or x == 1, as well x > 0x80000000,
	// so handle those cases up front:

	if (x > 0x80000000)
		return -1;

	if (x == 0 || x == 1)
		return x;

	--x;

	unsigned long	result = 0x80000000;
	while (((x <<= 1) & 0x80000000) == 0)	// check for the highest set bit.
		result >>= 1;

	return result;
}
#endif

#if 0
	// This one was posted to the net.	Seems most reasonable.
	// Fails when n == 0.
uint32 HighBitNumber (uint32 n)
{
	uint32 i = (n & 0xffff0000) ? 16 : 0;

	if ((n >>= i) & 0xff00)
	{
		i |= 8;
		n >>= 8;
	}

	if (n & 0xf0)
	{
		i |= 4;
		n >>= 4;
	}

	if (n & 0xc)
	{
		i |= 2;
		n >>= 2;
	}

	return (i | (n >> 1));
}
#endif

#if 0
	// This one was posted to the net.	Uses a loop; not quite as effecient.
	// Seems pretty buggy, since it doesn't work for x == 0, 1, or 2.
uint32 HighBitNumber (uint32 x)
{
	unsigned long mask=2, numBits=1;
	while (mask < x)
	{
		mask += mask;
		numBits++;
	}

	return numBits;
}
#endif

#if 0
	// This one was posted to the net. Makes up to 5 comparisons, which is
	// more than the one we're using.
uint32 HighBitNumber (uint32 x)
{
#define hi_bit(n)\
	((n)>=1<<16?(n)>=1<<24?(n)>=1<<28?(n)>=1<<30?(n)>=1<<31?31:30:(n)>=1<<29?\
	29:28:(n)>=1<<26?(n)>=1<<27?27:26:(n)>=1<<25?25:24:(n)>=1<<20?(n)>=1<<22?\
	(n)>=1<<23?23:22:(n)>=1<<21?21:20:(n)>=1<<18?(n)>=1<<19?19:18:(n)>=1<<17?\
	17:16:(n)>=1<<8?(n)>=1<<12?(n)>=1<<14?(n)>=1<<15?15:14:(n)>=1<<13?13:12:(\
	n)>=1<<10?(n)>=1<<11?11:10:(n)>=1<<9?9:8:(n)>=1<<4?(n)>=1<<6?(n)>=1<<7?7:\
	6:(n)>=1<<5?5:4:(n)>=1<<2?(n)>=1<<3?3:2:(n)>=1<<1?1:(n)>=1<<0?0:-1)

	return hi_bit (x);
}
#endif

#if 0
	// This one was posted to the net (by the same guy posting that macro).
	// Pretty neat until that divide by 37.
uint32 HighBitNumber (uint32 x)
{
	static const int t[] =
	{
		-1,  0, 25,  1, 22, 26, 31,  2, 15, 23, 29, 27, 10, -1, 12,  3,  6, 16,
		-1, 24, 21, 30, 14, 28,  9, 11,  5, -1, 20, 13,  8,  4, 19,  7, 18, 17
	};

	return t[(n |= n >> 1, n |= n >> 2, n |= n >> 4, n |= n >> 8, n |= n >> 16) % 37];
}
#endif


/***********************************************************************
 *
 * FUNCTION:	DateToDays
 *
 * DESCRIPTION: Convert a year, month, and day into the number of days
 *				since 1/1/1904.
 *
 *				Parameters are not checked for valid dates, so it's
 *				possible to feed in things like March 35, 1958.  This
 *				function also assumes that year is at least 1904, and
 *				will only work up until 2040 or so.
 *
 * PARAMETERS:	year - full year
 *
 *				month - 1..12
 *
 *				day - 1..31
 *
 * RETURNED:	Number of days since 1/1/1904.
 *
 ***********************************************************************/

uint32 DateToDays (uint32 year, uint32 month, uint32 day)
{
	static const int	month2days[] =
	{
		  0,	 31,	 59,	 90,	120,	151,
		181,	212,	243,	273,	304,	334
	};


	// Normalize the values.

	year -= 1904;
	month -= 1;
	day -= 1;

	// Not counting any possible leap-day in the current year, figure out
	// the number of days between now and 1/1/1904.

	const uint32	kNumDaysInLeapCycle = 4 * 365 + 1;

	uint32 days =	day + month2days[month] +
					(year * kNumDaysInLeapCycle + 3) / 4;

	// Now add in this year's leap-day, if there is one.

	if ((month >= 2) && ((year & 3) == 0))
		days++;

	return days;
}


/***********************************************************************
 *
 * FUNCTION:	GetLibraryName
 *
 * DESCRIPTION: 
 *
 * PARAMETERS:	none
 *
 * RETURNED:	The libraries name, or an empty string if the library
 *				could not be found.
 *
 ***********************************************************************/

string GetLibraryName (uint16 refNum)
{
	if (refNum == sysInvalidRefNum)
		return string();

	CEnableFullAccess	munge;	// Remove blocks on memory access.

	/*
		The System Library Table (sysLibTableP) is an array of
		sysLibTableEntries entries.  Each entry has the following
		format:

			Ptr*		dispatchTblP;	// pointer to library dispatch table
			void*		globalsP;		// Library globals
			LocalID 	dbID;			// database id of the library
			MemPtr	 	codeRscH;		// library code resource handle for RAM-based libraries

		The latter two fields are present only in Palm OS 2.0 and
		later.	So our first steps are to (a) get the pointer to 
		the array, (b) make sure that the index into the array (the
		refNum passed as the first parameter to all library calls)
		is within range, (c) get a pointer to the right entry,
		taking into account the Palm OS version, and (d) getting the
		dispatchTblP field.

		The "library dispatch table" is an array of 16-bit offsets.  The
		values are all relative to the beginning of the table (dispatchTblP).
		The first entry in the array corresponds to the library name.  All
		subsequent entries are offsets to the various library functions,
		starting with the required four: sysLibTrapOpen, sysLibTrapClose,
		sysLibTrapSleep, and sysLibTrapWake.
	*/

	emuptr	sysLibTableP		= EmLowMem_GetGlobal (sysLibTableP);
	UInt16	sysLibTableEntries	= EmLowMem_GetGlobal (sysLibTableEntries);

	if (sysLibTableP == EmMemNULL)
	{
		// !!! No library table!
		EmAssert (false);
		return string();
	}

	if (refNum >= sysLibTableEntries)
	{
		if (refNum != 0x0666)
		{
			// !!! RefNum out of range!
			EmAssert (false);
		}

		return string();
	}

	emuptr libEntry;
	emuptr dispatchTblP;

	if (EmPatchState::OSMajorVersion () > 1)
	{
		libEntry		= sysLibTableP + refNum * sizeof (SysLibTblEntryType);
		dispatchTblP	= EmMemGet32 (libEntry + offsetof (SysLibTblEntryType, dispatchTblP));
	}
	else
	{
		libEntry		= sysLibTableP + refNum * sizeof (SysLibTblEntryTypeV10);
		dispatchTblP	= EmMemGet32 (libEntry + offsetof (SysLibTblEntryTypeV10, dispatchTblP));
	}

	// The first entry in the table is always the offset from the
	// start of the table to the library name.	Use this information
	// get the library name.

	int16 	offset = EmMemGet16 (dispatchTblP + LibTrapIndex (sysLibTrapName) * 2);
	emuptr 	libNameP = dispatchTblP + offset;

	char		libName[256];
	EmMem_strcpy (libName, libNameP);

	return string (libName);
}


/***********************************************************************
 *
 * FUNCTION:	GetSystemCallContext
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

Bool GetSystemCallContext (emuptr pc, SystemCallContext& context)
{
	context.fPC = pc;

	// Determine how the system function is being called.  There are two ways:
	//
	//		* Via SYS_TRAP macro:
	//
	//			TRAP	$F
	//			DC.W	$Axxx
	//
	//		* Via SYS_TRAP_FAST macro:
	//
	//			MOVE.L	struct(LowMemType.fixed.globals.sysDispatchTableP), A1
	//			MOVE.L	((trapNum-sysTrapBase)*4)(A1), A1
	//			JSR 	(A1)	; opcode == 0x4e91
	//
	// The PC is current pointing to either the TRAP $F or the JSR (A1),
	// so we can look at the opcode to determine how we got here.

	uint8* realMem 		= EmMemGetRealAddress (pc);
	uint16 opcode		= EmMemDoGet16 (realMem);

	context.fViaTrap 	= opcode == (m68kTrapInstr + sysDispatchTrapNum);
	context.fViaJsrA1	= opcode == (0x4e91);

	
	if (context.fViaTrap)
	{
		// Not all development systems generate the correct dispatch
		// numbers; some leave off the preceding "A".  Make sure it's
		// set so that we can recognize it as a trap dispatch number.
		// (This code is here specifically so that the profiling routines
		//	will work, which check for trap numbers masquerading as function
		//	addresses by checking to see if they are in the sysTrapBase range.)

		context.fTrapWord	= EmMemGet16 (pc + 2) | sysTrapBase;
		context.fNextPC 	= pc + 4;
	}
	else if (context.fViaJsrA1)
	{
		context.fTrapWord	= (EmMemGet16 (pc - 2) / 4) | sysTrapBase;
		context.fNextPC 	= pc + 2;
	}
	else
	{
		EmAssert (false);
		return false;
	}

	if (::IsSystemTrap (context.fTrapWord))
	{
		context.fTrapIndex	= SysTrapIndex (context.fTrapWord);
		context.fExtra		= m68k_dreg (regs, 2);
	}
	else
	{
		context.fTrapIndex	= LibTrapIndex (context.fTrapWord);
		context.fExtra		= EmMemGet16 (m68k_areg (regs, 7));
	}

	EmAssert ((context.fTrapWord >= sysTrapBase) && (context.fTrapWord < sysTrapBase + 0x1000));

	try
	{
		context.fError		= 0;
		context.fDestPC1	= ::GetFunctionAddress (context.fTrapWord, context.fExtra, false);
		context.fDestPC2	= ::GetFunctionAddress (context.fTrapWord, context.fExtra, true);
	}
	catch (EmUnimplementedFunctionException& e)
	{
		context.fDestPC1	= EmMemNULL;
		context.fDestPC2	= EmMemNULL;
		context.fError		= kError_UnimplementedTrap;
		context.fLibIndex	= e.fLibIndex;
		return false;
	}
	catch (EmInvalidRefNumException& e)
	{
		context.fDestPC1	= EmMemNULL;
		context.fDestPC2	= EmMemNULL;
		context.fError		= kError_InvalidLibraryRefNum;
		context.fLibIndex	= e.fLibIndex;
		return false;
	}

	return true;
}


/***********************************************************************
 *
 * FUNCTION:	GetHostTime
 *
 * DESCRIPTION: Returns the current time in hours, minutes, and seconds.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void GetHostTime (long* hour, long* min, long* sec)
{
	time_t t;
	struct tm tm;
	
	time (&t);
	tm = *localtime (&t);
	
	*hour = tm.tm_hour; 	// 0...23
	*min =	tm.tm_min;		// 0...59
	*sec =	tm.tm_sec;		// 0...59
}


/***********************************************************************
 *
 * FUNCTION:	GetHostDate
 *
 * DESCRIPTION: Returns years since 1900, month as 0-11, and day as 1-31
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void GetHostDate (long* year, long* month, long* day)
{
	time_t t;
	struct tm tm;
	
	time (&t);
	tm = *localtime (&t);
	
	*year =  tm.tm_year + 1900; 	// 1904...2040
	*month = tm.tm_mon + 1; 		// 1...12
	*day =	 tm.tm_mday;			// 1...31
}


/***********************************************************************
 *
 * FUNCTION:	StartsWith
 *
 * DESCRIPTION:	Determine if a string starts with the given pattern.
 *
 * PARAMETERS:	s - string to test.
 *
 *				p - pattern to test with.
 *
 * RETURNED:	True if "s" starts with "p".
 *
 ***********************************************************************/

Bool StartsWith (const char* s, const char* p)
{
	if (strlen (s) < strlen (p))
		return false;

	return (_strnicmp (s, p, strlen (p)) == 0);
}


/***********************************************************************
 *
 * FUNCTION:	EndsWith
 *
 * DESCRIPTION:	Determine if a string end with the given pattern.
 *
 * PARAMETERS:	s - string to test.
 *
 *				p - pattern to test with.
 *
 * RETURNED:	True if "s" ends with "p".
 *
 ***********************************************************************/

Bool EndsWith (const char* s, const char* p)
{
	if (strlen (s) < strlen (p))
		return false;

	const char* buffer = s + strlen(s) - strlen(p);
	return (_stricmp (buffer, p) == 0);
}


/***********************************************************************
 *
 * FUNCTION:	Strip
 *
 * DESCRIPTION:	Remove leading or trailing instances of the given
 *				character(s).
 *
 * PARAMETERS:	s - string to modify.
 *
 *				ch - character(s) to remove.
 *
 *				leading - true if leading characters should be removed.
 *
 *				trailing - true if trailing characters should be removed.
 *
 * RETURNED:	Modified string.
 *
 ***********************************************************************/

string Strip (const char* s, const char* ch, Bool leading, Bool trailing)
{
	string	result (s);
	string	chars (ch);

	if (leading)
	{
		// Iterate over the string, looking to see if the leading character
		// is in the given character set.

		string::iterator	iter = result.begin ();
		while (iter != result.end ())
		{
			if (chars.find (*iter) == string::npos)
				break;

			result.erase (iter);
			iter = result.begin ();
		}
	}

	if (trailing)
	{
		// Iterate over the string, looking to see if the trailing character
		// is in the given character set.

		string::reverse_iterator	iter = result.rbegin ();
		while (iter != result.rend ())
		{
			if (chars.find (*iter) == string::npos)
				break;

			result.erase (iter.base () - 1);
			iter = result.rbegin ();
		}
	}

	return result;
}
string Strip (const string& s, const char* ch, Bool leading, Bool trailing)
{
	return ::Strip (s.c_str (), ch, leading, trailing);
}

/***********************************************************************
 *
 * FUNCTION:	ReplaceString
 *
 * DESCRIPTION:	Replace all occurances of one string inside of a second
 *				string with a third string.
 *
 * PARAMETERS:	source - the string on which the replacements are to
 *					be made.
 *
 *				pattern - substring to search for.
 *
 *				replacement - string with which to replace the patterm.
 *
 * RETURNED:	The changed string.
 *
 ***********************************************************************/

string		ReplaceString	(const string& source,
							 const string& pattern,
							 const string& replacement)
{
	string				result (source);
	string::size_type	pos = 0;

	for (;;)
	{
		pos = result.find (pattern, pos);
		if (pos == string::npos)
			break;
		result.replace (pos, pattern.size (), replacement);
		pos += replacement.size ();
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	PrvInsertString
 *
 * DESCRIPTION:	Insert one string into the middle of another.  The
 *				buffer containing the string being modified should be
 *				large enough to hold the resulting string.
 *
 * PARAMETERS:	dest - buffer containing the string to receive the
 *					inserted text.
 *
 *				insertBefore - character index at which the inserted
 *					text should appear. Assumed to be [0, strlen(dest)].
 *
 *				src - text to be inserted.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

static void PrvInsertString (char* dest, int insertBefore, const char* src)
{
	size_t	destLen = strlen (dest);
	size_t	srcLen = strlen (src);

	// Move down the part of the string after the insertion point.	Don't
	// use strcpy or memcpy, as we're dealing with an overlapping range.
	// Add 1 to the range length to get the terminating NULL.

	memmove (dest + insertBefore + srcLen, dest + insertBefore, destLen - insertBefore + 1);

	// Insert the new string.  *Don't* copy the NULL!

	memmove (dest + insertBefore, src, srcLen);
}


/***********************************************************************
 *
 * FUNCTION:	FormatInteger
 *
 * DESCRIPTION:	Convert an integral value into a formatted string,
 *				adding thousands seperators as necessary.
 *
 * PARAMETERS:	dest - buffer for the outputted string
 *
 *				integer - input value
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void FormatInteger (char* dest, uint32 integer)
{
	sprintf (dest, "%ld", integer);

	// Get the thousands separator character(s).
#ifdef HAVE_LOCALECONV
	struct lconv*	locale_data = localeconv ();
	char*			thousands_sep = locale_data->thousands_sep;

	if (strlen (thousands_sep) == 0)
	{
		thousands_sep = ",";
	}
#else
	char*			thousands_sep = ",";
#endif

	// Insert the thousands separator(s).

		// Divide by three to get the number of 3 digit groupings remaining
		// (subtracting one to get the math to come out right)
		//
		//		 1 -> 0
		//		 2 -> 0
		//		 3 -> 0
		//		 4 -> 1
		//		 5 -> 1
		//		 6 -> 1
		//		 7 -> 2
		//		 8 -> 2
		//		 9 -> 2
		//		10 -> 3
		//
		//	etc...

	int numCommas = (strlen (dest) - 1) / 3;

		// Special case the stupid rule about not putting a comma
		// in a number like xxxx.

	if (strlen (dest) <= 4)
	{
		numCommas = 0;
	}

	for (int ii = 1; ii <= numCommas; ++ii)
	{
		// Back up four for every comma (skip past every ",xxx" pattern).

		::PrvInsertString (dest, strlen (dest) + 1 - (4 * ii), thousands_sep);
	}
}

string FormatInteger (uint32 integer)
{
	// Format the integer as a plain string.

	strstream	stream;
	
	stream << integer;

	string	result (stream.str (), stream.pcount ());

	// Unfreeze the stream, or else its storage will be leaked.

	stream.freeze (false);

	// Get the thousands separator character(s).
#ifdef HAVE_LOCALECONV
	struct lconv*	locale_data = localeconv ();
	char*			thousands_sep = locale_data->thousands_sep;

	if (strlen (thousands_sep) == 0)
	{
		thousands_sep = ",";
	}
#else
	char*			thousands_sep = ",";
#endif

	// Insert the thousands separator(s).

		// Divide by three to get the number of 3 digit groupings remaining
		// (subtracting one to get the math to come out right)
		//
		//		 1 -> 0
		//		 2 -> 0
		//		 3 -> 0
		//		 4 -> 1
		//		 5 -> 1
		//		 6 -> 1
		//		 7 -> 2
		//		 8 -> 2
		//		 9 -> 2
		//		10 -> 3
		//
		//	etc...

	int numCommas = (result.size () - 1) / 3;

		// Special case the stupid rule about not putting a comma
		// in a number like xxxx.

	if (result.size () <= 4)
	{
		numCommas = 0;
	}

	for (int ii = 1; ii <= numCommas; ++ii)
	{
		// Back up four for every comma (skip past every ",xxx" pattern).
		
		result.insert (result.size () + 1 - (4 * ii), thousands_sep);
	}

	return result;
}


string FormatElapsedTime (uint32 mSecs)
{
	// Get hours, minutes, and seconds.

	const long	kMillisecondsPerSecond	= 1000;
	const long	kSecondsPerMinute		= 60;
	const long	kMinutesPerHour 		= 60;

	const long	kMillisecondsPerMinute	= kMillisecondsPerSecond * kSecondsPerMinute;
	const long	kMillisecondsPerHour	= kMillisecondsPerMinute * kMinutesPerHour;

	long	hours	= mSecs / kMillisecondsPerHour;		mSecs -= hours * kMillisecondsPerHour;
	long	minutes	= mSecs / kMillisecondsPerMinute;	mSecs -= minutes * kMillisecondsPerMinute;
	long	seconds	= mSecs / kMillisecondsPerSecond;	mSecs -= seconds * kMillisecondsPerSecond;

	// Format them into a string.

	char	formattedTime[20];
	sprintf (formattedTime, "%ld:%02ld:%02ld", hours, minutes, seconds);

	return string (formattedTime);
}


/***********************************************************************
 *
 * FUNCTION:	LaunchCmdToString
 *
 * DESCRIPTION: Convert the given launch command (the command that's
 *				passed to PilotMain in a Palm OS application) into a
 *				text form suitable for displaying to a user.
 *
 * PARAMETERS:	cmd - the launch command (from SystemMgr.h)
 *
 * RETURNED:	A pointer to a string representing the command.  If
 *				the command is unrecognized (probably because this
 *				function is out-of-date and needs to be updated for
 *				newly-added commands), a default string containing
 *				the command number is returned.
 *
 ***********************************************************************/

const char* LaunchCmdToString (UInt16 cmd)
{
#undef DOCASE
#define DOCASE(name)	\
	case name:			\
		return #name;

	switch (cmd)
	{
		DOCASE (sysAppLaunchCmdNormalLaunch)
		DOCASE (sysAppLaunchCmdFind)
		DOCASE (sysAppLaunchCmdGoTo)
		DOCASE (sysAppLaunchCmdSyncNotify)
		DOCASE (sysAppLaunchCmdTimeChange)
		DOCASE (sysAppLaunchCmdSystemReset)
		DOCASE (sysAppLaunchCmdAlarmTriggered)
		DOCASE (sysAppLaunchCmdDisplayAlarm)
		DOCASE (sysAppLaunchCmdCountryChange)
		DOCASE (sysAppLaunchCmdSyncRequestLocal)
		DOCASE (sysAppLaunchCmdSaveData)
		DOCASE (sysAppLaunchCmdInitDatabase)
		DOCASE (sysAppLaunchCmdSyncCallApplicationV10)
		DOCASE (sysAppLaunchCmdPanelCalledFromApp)
		DOCASE (sysAppLaunchCmdReturnFromPanel)
		DOCASE (sysAppLaunchCmdLookup)
		DOCASE (sysAppLaunchCmdSystemLock)
		DOCASE (sysAppLaunchCmdSyncRequestRemote)
		DOCASE (sysAppLaunchCmdHandleSyncCallApp)
		DOCASE (sysAppLaunchCmdAddRecord)
		DOCASE (sysSvcLaunchCmdSetServiceID)
		DOCASE (sysSvcLaunchCmdGetServiceID)
		DOCASE (sysSvcLaunchCmdGetServiceList)
		DOCASE (sysSvcLaunchCmdGetServiceInfo)
		DOCASE (sysAppLaunchCmdFailedAppNotify)
		DOCASE (sysAppLaunchCmdEventHook)
		DOCASE (sysAppLaunchCmdExgReceiveData)
		DOCASE (sysAppLaunchCmdExgAskUser)
		DOCASE (sysDialLaunchCmdDial)
		DOCASE (sysDialLaunchCmdHangUp)
		DOCASE (sysSvcLaunchCmdGetQuickEditLabel)
		DOCASE (sysAppLaunchCmdURLParams)
		DOCASE (sysAppLaunchCmdNotify)
		DOCASE (sysAppLaunchCmdOpenDB)
		DOCASE (sysAppLaunchCmdAntennaUp)
		DOCASE (sysAppLaunchCmdGoToURL)
	}

	static char 	buffer[20];
	sprintf (buffer, "#%ld", (long) cmd);
	return buffer;
}


/***********************************************************************
 *
 * FUNCTION:    GetMemoryList
 *
 * DESCRIPTION: Return the list of items that should appear in the
 *				various listboxes and menus that allow the user to
 *				select a memory size.  The list is returned as a
 *				collection of pairs.  The first element of the pair is
 *				the memory size (in K), and the second element of the
 *				pair is the same thing in textform, suitable for
 *				displaying to the user.
 *
 * PARAMETERS:  memoryList - reference to the collection to receive
 *					the results.
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void GetMemoryTextList (MemoryTextList& memoryList)
{
	memoryList.push_back (make_pair (RAMSizeType (128), string ("128K")));
	memoryList.push_back (make_pair (RAMSizeType (256), string ("256K")));
	memoryList.push_back (make_pair (RAMSizeType (512), string ("512K")));
	memoryList.push_back (make_pair (RAMSizeType (1024), string ("1024K")));
	memoryList.push_back (make_pair (RAMSizeType (2048), string ("2048K")));
	memoryList.push_back (make_pair (RAMSizeType (4096), string ("4096K")));
	memoryList.push_back (make_pair (RAMSizeType (8192), string ("8192K")));
	memoryList.push_back (make_pair (RAMSizeType (16384), string ("16384K")));
}


/***********************************************************************
 *
 * FUNCTION:    MyAssertFailed
 *
 * DESCRIPTION: Called by the EmAssert macro if NDEBUG is defined.
 *				EmAssert and MyAssertFailed are moral replacements for
 *				the assert macro of the Standard C Library and the
 *				underlying function it calls.  However, those facilities
 *				don't necessarily work the way we'd like.  In
 *				particular, on Linux, they will abort the application.
 *				Sometimes, we'd like the ability to continue application
 *				execution.
 *
 * PARAMETERS:  exp - expression that evaluate to false.
 *				file - name of file containing the expression.
 *				line - line number in the file containing the expression.
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void MyAssertFailed (const char* expr, const char* file, unsigned int line)
{
	char	message[2000];

	sprintf (message, "Assertion Failure: Expression: \"%s\", File: %s, Line: %d",
				expr, file, line);

	LogDump ();

	Platform::Debugger (message);
}
