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
#include "EmPalmFunction.h"

#include "EmBankROM.h"			// EmBankROM::GetMemoryStart
#include "EmLowMem.h"			// LowMem_GetGlobal
#include "EmMemory.h"			// CEnableFullAccess, EmMem_strcpy, EmMem_memcmp
#include "EmPalmHeap.h"			// EmPalmHeap
#include "EmPatchState.h"		// EmPatchState::OSMajorVersion
#include "Miscellaneous.h"		// FindFunctionName
#include "Platform.h"			// Platform::GetString
#include "Strings.r.h"			// kStr_INetLibTrapBase

#include <ctype.h>				// isalnum, toupper

const UInt16	kMagicRefNum	= 0x666;	// See comments in HtalLibSendReply.

const uint16	kOpcode_ADD 	= 0x0697;	// ADD.L X, (A7)
const uint16	kOpcode_LINK	= 0x4E50;
const uint16	kOpcode_RTE 	= 0x4E73;
const uint16	kOpcode_RTD 	= 0x4E74;
const uint16	kOpcode_RTS 	= 0x4E75;
const uint16	kOpcode_JMP 	= 0x4ED0;

const uint16	kOpcode_CMPW	= 0xB47C;		// CMP.W	#$xxxx,D2
const uint16	kOpcode_ADDW	= 0xD442;		// ADD.W	D2,D2
const uint32	kOpcode_JMPPC	= 0x4EFB2002;	// JMP		*+$0004(D2.W)
const uint16	kOpcode_JMPREL	= 0x4EFA;		// JMP		*+$xxxx
const uint16	kOpcode_MOVEL	= 0x2038;		// MOVE.L	$xxxxxxxx,D0

// This table should match up with gResourceBases in ErrorHandling.cpp.
static const char* gPalmOSLibraries [] =
{
	"INet.lib",
	"IrDA Library", // Also includes Exchange Lib
	"Security.lib",
	"Web.lib",
//	serIrCommLibNameP,	// ???SerIrCommLib.h doesn't declare the functions as SYSTRAPs!
	"Debugger Comm",
	"IrSerial Library",
	"Net.lib",
	"PADHTAL.lib",
	"Rail I/O Library",
	"RMP NetPlugIn.lib",
	"Serial Lib68681",
	"Serial Library",
	"TCPHTAL.lib",

	NULL
};


const uint8	kCerticomMemCpyPattern [] =
{
	0x20, 0x6F, 0x00, 0x04,	// 080B3CE2 MOVEA.L $0004(A7),A0          | 206F 0004
	0x22, 0x6F, 0x00, 0x08,	// 080B3CE6 MOVEA.L $0008(A7),A1          | 226F 0008
	0x30, 0x2F, 0x00, 0x0C,	// 080B3CEA MOVE.W  $000C(A7),D0          | 302F 000C
	0x60, 0x02,				// 080B3CEE BRA.S   *+$0004    ; 080B3CF2 | 6002
	0x10, 0xD9,				// 080B3CF0 MOVE.B  (A1)+,(A0)+           | 10D9
	0x51, 0xC8, 0xFF, 0xFC,	// 080B3CF2 DBF     D0,*-$0002 ; 080B3CF0 | 51C8 FFFC
	0x4E, 0x75				// 080B3CF6 RTS                           | 4E75
};


class EmFunctionRange
{
	public:
								EmFunctionRange	(const char* functionName);

		Bool					InRange			(emuptr = gCPU->GetPC ());
		void					Reset			(void);

		Bool					HasRange		(void) { return fBegin != EmMemNULL; }

	private:
		void					GetRange		(emuptr addr);

		const char*				fName;
		emuptr					fBegin;
		emuptr					fEnd;

		EmFunctionRange*		fNext;
		static EmFunctionRange*	fgRoot;
};

EmFunctionRange*	EmFunctionRange::fgRoot;

// Define a bunch of EmFunctionRange objects to search for and
// cache function ranges.

#define DEFINE_OBJECT(fn_name)	\
	static EmFunctionRange	g##fn_name (#fn_name);

FOR_EACH_FUNCTION(DEFINE_OBJECT)

// Define all the InFoo functions declared in EmPalmFunction.h.

#define DEFINE_FUNCTION(fn_name)	\
	Bool In##fn_name (emuptr addr) { return g##fn_name.InRange (addr); }

FOR_EACH_FUNCTION(DEFINE_FUNCTION)


static string	PrvGetShortName (const char*, int len);


/***********************************************************************
 *
 * FUNCTION:	EmFunctionRangeInit
 *
 * DESCRIPTION:	Reset all our objects that track where certain Palm
 *				functions exist in memory.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmPalmFunctionInit (void)
{
#define RESET_OBJECT(fn_name)	\
	g##fn_name.Reset ();

FOR_EACH_FUNCTION(RESET_OBJECT)
}


/***********************************************************************
 *
 * FUNCTION:	EmFunctionRange::EmFunctionRange
 *
 * DESCRIPTION:	Create the object.  Initialize all data members.
 *
 * PARAMETERS:	functionName - name of function to track.  Storage is
 *					not owned by this object, so clients should make
 *					sure it exists for the life of this object.  The
 *					function name is typically provided as a string
 *					constant in global space.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmFunctionRange::EmFunctionRange (const char* functionName) :
	fName (functionName),
	fBegin (EmMemNULL),
	fEnd (EmMemNULL),
	fNext (fgRoot)
{
	fgRoot = this;
}


/***********************************************************************
 *
 * FUNCTION:	EmFunctionRange::InRange
 *
 * DESCRIPTION:	Determine whether or not the given address is within
 *				the function managed by this object.
 *
 * PARAMETERS:	addr - address to test.
 *
 * RETURNED:	True if so.
 *
 ***********************************************************************/

Bool EmFunctionRange::InRange (emuptr addr)
{
	// It's not in our range if it's in someone else's.
	// This is an optimization.

	static Bool	walkingChain;

	if (!walkingChain)
	{
		walkingChain = true;

		EmFunctionRange*	range = fgRoot;
		while (range)
		{
			if (range != this && range->HasRange () && range->InRange (addr))
			{
				walkingChain = false;
				return false;
			}

			range = range->fNext;
		}

		walkingChain = false;
	}

	// If we haven't determine the range of the function for which we're
	// responsible, see if given address is in that function.  If so,
	// remember the range of that function.

	if (!this->HasRange ())
	{
		this->GetRange (addr);
	}

	// If we know the range of the function for which we are responsible,
	// test the address against that range.

	if (this->HasRange () && (addr >= fBegin) && (addr < fEnd))
	{
		// We may not want to cache the range if it's in RAM.

		EmMemGetFunc	func = EmMemGetBank(addr).lget;
		if ((func != &EmBankROM::GetLong) && (func != &EmBankFlash::GetLong))
		{
			this->Reset ();
		}

		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	EmFunctionRange::Reset
 *
 * DESCRIPTION:	If we've determined and remembered where the managed
 *				function exists, forget that information.  This method
 *				is typically called so that this object can be reused
 *				after a new ROM image has been loaded.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmFunctionRange::Reset (void)
{
	fBegin	= EmMemNULL;
	fEnd	= EmMemNULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmFunctionRange::GetRange
 *
 * DESCRIPTION:	Find the name of the function containing the given
 *				address.  If it's our managed function, remember the
 *				range of the function.
 *
 * PARAMETERS:	addr - probe address.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmFunctionRange::GetRange (emuptr addr)
{
	char	name[80];
	emuptr	startAddr;
	emuptr	endAddr;

	::FindFunctionName (addr, name, &startAddr, &endAddr, 80);

	string	shortName = ::PrvGetShortName (fName, 8);
	
	// See if the function has the name as specified in fName.

	if (strncmp (name, fName, 79) == 0)
	{
		fBegin = startAddr;
		fEnd = endAddr;
	}

	// If not, see if the function name is the same as the "fixed 8"
	// format of the Macsbug name.

	else if (strcmp (name, shortName.c_str ()) == 0)
	{
		fBegin = startAddr;
		fEnd = endAddr;
	}

	// Check to see if it looks like this is the function the Certicom
	// Encryption library uses to copy "random" data.  There's no
	// Macsbug name, so we have to go with some other signature.

	else if (strcmp ("_CerticomMemCpy", fName) == 0)
	{
		if (endAddr - startAddr != 0x16)
			return;

		if (EmMem_memcmp (startAddr, (void*) kCerticomMemCpyPattern, endAddr - startAddr) != 0)
			return;

		fBegin = startAddr;
		fEnd = endAddr;
	}
}




/***********************************************************************
 *
 * FUNCTION:	PrvProscribedFunction
 *
 * DESCRIPTION: Return whether or not the user should be warned about
 *				calling this function.
 *
 * PARAMETERS:	context - information about the system call context.
 *
 * RETURNED:	True if this function is proscribed and should not be
 *				called; false if it's OK for applications to call it.
 *
 ***********************************************************************/
// *****************************************************************
// * New Serial Manager trap selectors
// *****************************************************************

// The numbering of these #defines *MUST* match the order in SerialMgr.c

#define sysSerialInstall				0
#define sysSerialOpen					1
#define sysSerialOpenBkgnd				2
#define sysSerialClose					3
#define sysSerialSleep					4
#define sysSerialWake					5
#define sysSerialGetDeviceCount			6
#define sysSerialGetDeviceInfo			7
#define sysSerialGetStatus				8
#define sysSerialClearErr				9
#define sysSerialControl				10
#define sysSerialSend					11
#define sysSerialSendWait				12
#define sysSerialSendCheck				13
#define sysSerialSendFlush				14
#define sysSerialReceive				15
#define sysSerialReceiveWait			16
#define sysSerialReceiveCheck			17
#define sysSerialReceiveFlush			18
#define sysSerialSetRcvBuffer			19
#define sysSerialRcvWindowOpen			20
#define sysSerialRcvWindowClose			21
#define sysSerialSetWakeupHandler		22
#define sysSerialPrimeWakeupHandler		23
#define sysSerialOpenV4					24
#define sysSerialOpenBkgndV4			25
#define sysSerialCustomControl			26


// Selectors for routines found in the international manager. The order
// of these selectors MUST match the jump table in IntlDispatch.c.

#define intlIntlInit					0
#define intlTxtByteAttr					1
#define intlTxtCharAttr					2
#define intlTxtCharXAttr				3
#define intlTxtCharSize					4
#define intlTxtGetPreviousChar			5
#define intlTxtGetNextChar				6
#define intlTxtGetChar					7
#define intlTxtSetNextChar				8
#define intlTxtCharBounds				9
#define intlTxtPrepFindString			10
#define intlTxtFindString				11
#define intlTxtReplaceStr				12
#define intlTxtWordBounds				13
#define intlTxtCharEncoding				14
#define intlTxtStrEncoding				15
#define intlTxtEncodingName				16
#define intlTxtMaxEncoding				17
#define intlTxtTransliterate			18
#define intlTxtCharIsValid				19
#define intlTxtCompare					20
#define intlTxtCaselessCompare			21
#define intlTxtCharWidth				22
#define intlTxtGetTruncationOffset		23
#define intlIntlGetRoutineAddress		24
#define intlIntlHandleEvent				25	// New for Palm OS 3.5
#define intlTxtParamString				26
#define intlTxtConvertEncodingV35		27	// Patched for Palm OS 3.5.2
#define intlTxtConvertEncoding			28	// New for Palm OS 4.0
#define intlIntlSetRoutineAddress		29
#define intlTxtGetWordWrapOffset		30
#define intlTxtNameToEncoding			31	
#define	intlIntlStrictChecks			32


// Selectors used for getting to the right Overlay Manager routine via
// the OmDispatch trap.

#define omInit							0
#define omOpenOverlayDatabase			1
#define omLocaleToOverlayDBName			2
#define omOverlayDBNameToLocale			3
#define omGetCurrentLocale				4
#define omGetIndexedLocale				5
#define omGetSystemLocale				6
#define omSetSystemLocale				7
#define omGetRoutineAddress				8
#define omGetNextSystemLocale			9


struct EmProscribedFunction
{
	uint16	fTrapWord;
	int		fReason;
};


static const EmProscribedFunction kProscribedFunctionTrapWords [] =
{
	// Entries marked with "//*" are supported in PACE because Palm
	// apps, test apps, or Palm Debugger needs them, but developers
	// should probably still be warned that they should not be using
	// them.  Additionally, the Fpl functions are marked this way
	// because many applications call them, but we'd like to warn
	// them to use the Flp functions.

/*
	6	Appendix B - Unsupported Palm OS APIs

		The following is a list of Palm OS traps that are not supported
		by the Timulator.  The list is broken into a number of different
		groups (which more or less explains why the trap was not implemented).

	6.1.1	Documented 'System Use Only' Traps

		These traps are documented as "System Use Only" in the "Palm OS
		Reference Manual".
*/

	{ sysTrapAlmAlarmCallback,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapAlmCancelAll,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapAlmDisplayAlarm,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapAlmEnableNotification,		kProscribedDocumentedSystemUseOnly },	//* supported
	{ sysTrapAlmInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapAlmTimeChange,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapDmInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapEvtDequeueKeyEvent,		kProscribedDocumentedSystemUseOnly },
	{ sysTrapEvtGetSysEvent,			kProscribedDocumentedSystemUseOnly },	//* pseudo supported
	{ sysTrapEvtInitialize,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapEvtSetKeyQueuePtr,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapEvtSetPenQueuePtr,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapEvtSysInit,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapExgInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapFrmAddSpaceForObject,		kProscribedDocumentedSystemUseOnly },
	{ sysTrapFtrInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapGrfFree,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapGrfInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapInsPtCheckBlink,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapInsPtInitialize,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemCardFormat,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemHandleFlags,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemHandleOwner,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemHandleResetLock,		kProscribedDocumentedSystemUseOnly },	//* supported for release ROMs
	{ sysTrapMemHeapFreeByOwnerID,		kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemHeapInit,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemInitHeapTable,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemKernelInit,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemPtrFlags,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemPtrOwner,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemPtrResetLock,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemStoreInit,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapMemStoreSetInfo,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapPenClose,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapPenGetRawPen,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapPenOpen,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapPenRawToScreen,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapPenScreenToRaw,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrCompressScanLine,		kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrCopyRectangle,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrDeCompressScanLine,		kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrDrawChars,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrDrawNotify,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrLineRoutine,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrRectangleRoutine,		kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrScreenInfo,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapScrSendUpdateArea,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapSlkProcessRPC,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapSlkSysPktDefaultResponse,	kProscribedDocumentedSystemUseOnly },
	{ sysTrapSndInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapSysBatteryDialog,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapSysColdBoot,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapSysDoze,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapSysInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapSysLaunchConsole,			kProscribedDocumentedSystemUseOnly },	//* supported
	{ sysTrapSysNewOwnerID,				kProscribedDocumentedSystemUseOnly },
//	{ sysTrapSysReserved10Trap1,		kProscribedDocumentedSystemUseOnly },
//	{ sysTrapSysReserved31Trap1,		kProscribedDocumentedSystemUseOnly },
	{ sysTrapSysSemaphoreSet,			kProscribedDocumentedSystemUseOnly },
	{ sysTrapSysUILaunch,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapSysWantEvent,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapTimInit,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapUIInitialize,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapUIReset,					kProscribedDocumentedSystemUseOnly },
	{ sysTrapWinAddWindow,				kProscribedDocumentedSystemUseOnly },
	{ sysTrapWinRemoveWindow,			kProscribedDocumentedSystemUseOnly },

/*
	6.1.2	Undocumented 'System Use Only' or 'HAL Use Only' Traps

		These traps are routines in the HAL, documented in headers to be
		called by Palm OS only, or I have spoken with the authors of those
		traps who identified them as internal traps.
*/

	{ sysTrapAttnAllowClose,					kProscribedUndocumentedSystemUseOnly },	//* supported
	{ sysTrapAttnDoEmergencySpecialEffects,		kProscribedUndocumentedSystemUseOnly },
	{ sysTrapAttnEffectOfEvent,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapAttnEnableNotification,			kProscribedUndocumentedSystemUseOnly },	//* supported
	{ sysTrapAttnHandleEvent,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapAttnIndicatorAllow,				kProscribedUndocumentedSystemUseOnly },	//* supported
	{ sysTrapAttnIndicatorAllowed,				kProscribedUndocumentedSystemUseOnly },	//* supported
	{ sysTrapAttnIndicatorCheckBlink,			kProscribedUndocumentedSystemUseOnly },
	{ sysTrapAttnIndicatorGetBlinkPattern,		kProscribedUndocumentedSystemUseOnly },	//* supported
	{ sysTrapAttnIndicatorSetBlinkPattern,		kProscribedUndocumentedSystemUseOnly },	//* supported
	{ sysTrapAttnIndicatorTicksTillNextBlink,	kProscribedUndocumentedSystemUseOnly },
	{ sysTrapAttnInitialize,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltCopyRectangle,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltDrawChars,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltFindIndexes,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltGetPixel,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltLineRoutine,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltPaintPixel,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltPaintPixels,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltRectangleRoutine,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltRoundedRectangle,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapBltRoundedRectangleFill,			kProscribedUndocumentedSystemUseOnly },
	{ sysTrapDayHandleEvent,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapDbgControl,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapDbgSerDrvClose,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapDbgSerDrvControl,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapDbgSerDrvOpen,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapDbgSerDrvReadChar,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapDbgSerDrvStatus,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapDbgSerDrvWriteChar,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapFlashInit,							kProscribedUndocumentedSystemUseOnly },
	{ sysTrapFntPrvGetFontList,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrBacklightV33,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrBattery,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrBatteryLevel,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrCalcDynamicHeapSize,			kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrCursorV33,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrCustom,							kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDebuggerEnter,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDebuggerExit,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDebugSelect,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDisplayDoze,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDisplayDrawBootScreen,			kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDisplayInit,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDisplayPalette,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDisplaySleep,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDisplayWake,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDockSignals,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrDockStatus,						kProscribedUndocumentedSystemUseOnly },	//* supported
	{ sysTrapHwrDoze,							kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrFlashWrite,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrGetRAMMapping,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrGetSilkscreenID,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrIdentifyFeatures,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrInterruptsInit,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrIRQ1Handler,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrIRQ2Handler,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrIRQ3Handler,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrIRQ4Handler,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrIRQ5Handler,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrIRQ6Handler,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrLCDBaseAddrV33,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrLCDContrastV33,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrLCDGetDepthV33,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrModelInitStage2,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrModelInitStage3,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrModelSpecificInit,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrNVPrefGet,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrNVPrefSet,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrPluggedIn,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrPostDebugInit,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrPreDebugInit,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrResetNMI,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrResetPWM,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrSetCPUDutyCycle,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrSetSystemClock,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrSleep,							kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrSoundOff,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrSoundOn,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrTimerInit,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapHwrWake,							kProscribedUndocumentedSystemUseOnly },
	{ sysTrapKeyBootKeys,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapKeyHandleInterrupt,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapKeyInit,							kProscribedUndocumentedSystemUseOnly },
	{ sysTrapMemHeapPtr,						kProscribedUndocumentedSystemUseOnly },	//* supported (PalmDebugger)
	{ sysTrapMemStoreSearch,					kProscribedUndocumentedSystemUseOnly },
//	{ sysTrapOEMDispatch2,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapPalmPrivate3,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrCompress,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrDecompress,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrGetColortable,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrGetGrayPat,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrPalette,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrScreenInit,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrScreenLock,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrScreenUnlock,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapScrUpdateScreenBitmap,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapSndInterruptSmfIrregardless,		kProscribedUndocumentedSystemUseOnly },
	{ sysTrapSndPlaySmfIrregardless,			kProscribedUndocumentedSystemUseOnly },
	{ sysTrapSndPlaySmfResourceIrregardless,	kProscribedUndocumentedSystemUseOnly },
	{ sysTrapSysFatalAlertInit,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapSysKernelClockTick,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapSysNotifyBroadcastFromInterrupt,	kProscribedUndocumentedSystemUseOnly },
	{ sysTrapSysNotifyInit,						kProscribedUndocumentedSystemUseOnly },
//	{ sysTrapSysReserved30Trap1,				kProscribedUndocumentedSystemUseOnly },
//	{ sysTrapSysReserved30Trap2,				kProscribedUndocumentedSystemUseOnly },
	{ sysTrapSysUnimplemented,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapTimGetAlarm,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapTimSetAlarm,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapUIColorInit,						kProscribedUndocumentedSystemUseOnly },
	{ sysTrapWinGetFirstWindow,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapWinMoveWindowAddr,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapWinPrvInitCanvas,					kProscribedUndocumentedSystemUseOnly },
	{ sysTrapWinScreenInit,						kProscribedUndocumentedSystemUseOnly },

/*
	6.1.3	Kernel Traps

		These traps are not implemented because 68K applications do not have
		access to the kernel APIs.
*/

	{ sysTrapSysEvGroupCreate,			kProscribedKernelUseOnly },
	{ sysTrapSysEvGroupRead,			kProscribedKernelUseOnly },
	{ sysTrapSysEvGroupSignal,			kProscribedKernelUseOnly },
	{ sysTrapSysEvGroupWait,			kProscribedKernelUseOnly },
	{ sysTrapSysKernelInfo,				kProscribedKernelUseOnly },
	{ sysTrapSysMailboxCreate,			kProscribedKernelUseOnly },
	{ sysTrapSysMailboxDelete,			kProscribedKernelUseOnly },
	{ sysTrapSysMailboxFlush,			kProscribedKernelUseOnly },
	{ sysTrapSysMailboxSend,			kProscribedKernelUseOnly },
	{ sysTrapSysMailboxWait,			kProscribedKernelUseOnly },
	{ sysTrapSysResSemaphoreCreate,		kProscribedKernelUseOnly },
	{ sysTrapSysResSemaphoreDelete,		kProscribedKernelUseOnly },
	{ sysTrapSysResSemaphoreRelease,	kProscribedKernelUseOnly },
	{ sysTrapSysResSemaphoreReserve,	kProscribedKernelUseOnly },
	{ sysTrapSysSemaphoreCreate,		kProscribedKernelUseOnly },
	{ sysTrapSysSemaphoreDelete,		kProscribedKernelUseOnly },
	{ sysTrapSysSemaphoreSignal,		kProscribedKernelUseOnly },
	{ sysTrapSysSemaphoreWait,			kProscribedKernelUseOnly },
	{ sysTrapSysTaskCreate,				kProscribedKernelUseOnly },
	{ sysTrapSysTaskDelete,				kProscribedKernelUseOnly },
	{ sysTrapSysTaskID,					kProscribedKernelUseOnly },
	{ sysTrapSysTaskResume,				kProscribedKernelUseOnly },
	{ sysTrapSysTaskSetTermProc,		kProscribedKernelUseOnly },
	{ sysTrapSysTaskSuspend,			kProscribedKernelUseOnly },
	{ sysTrapSysTaskSwitching,			kProscribedKernelUseOnly },
	{ sysTrapSysTaskTrigger,			kProscribedKernelUseOnly },
	{ sysTrapSysTaskUserInfoPtr,		kProscribedKernelUseOnly },
	{ sysTrapSysTaskWait,				kProscribedKernelUseOnly },
	{ sysTrapSysTaskWaitClr,			kProscribedKernelUseOnly },
	{ sysTrapSysTaskWake,				kProscribedKernelUseOnly },
	{ sysTrapSysTimerCreate,			kProscribedKernelUseOnly },
	{ sysTrapSysTimerDelete,			kProscribedKernelUseOnly },
	{ sysTrapSysTimerRead,				kProscribedKernelUseOnly },
	{ sysTrapSysTimerWrite,				kProscribedKernelUseOnly },
	{ sysTrapSysTranslateKernelErr,		kProscribedKernelUseOnly },

/*
	6.1.4	Obsolete Traps

		These traps are not implemented because they are obsolete Palm OS
		1.0 traps (or an esoteric obsolete trap such as WiCmdV32).
*/

	{ sysTrapFplAdd,			kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplAToF,			kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplBase10Info,		kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplDiv,			kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplFloatToLong,	kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplFloatToULong,	kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplFToA,			kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplLongToFloat,	kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplMul,			kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapFplSub,			kProscribedObsolete },	//* supported (release ROM only)
	{ sysTrapWiCmdV32,			kProscribedObsolete },

/*
	6.1.5	Ghost Traps

		These traps were never implemented in Palm OS (although they appear
		in CoreTraps.h), but they are listed for completeness.  One of the
		traps in this group, MenuEraseMenu, was implemented but should be
		unknown to developers, as it does not appear in any public header file.
*/

	{ sysTrapClipboardCheckIfItemExist,	kProscribedGhost },
	{ sysTrapCtlValidatePointer,		kProscribedGhost },
	{ sysTrapFrmSetCategoryTrigger,		kProscribedGhost },
	{ sysTrapFrmSetLabel,				kProscribedGhost },
	{ sysTrapMenuEraseMenu,				kProscribedGhost },
	{ sysTrapSysUICleanup,				kProscribedGhost },
	{ sysTrapWinDrawArc,				kProscribedGhost },
	{ sysTrapWinDrawPolygon,			kProscribedGhost },
	{ sysTrapWinEraseArc,				kProscribedGhost },
	{ sysTrapWinErasePolygon,			kProscribedGhost },
	{ sysTrapWinFillArc,				kProscribedGhost },
	{ sysTrapWinFillPolygon,			kProscribedGhost },
	{ sysTrapWinInvertArc,				kProscribedGhost },
	{ sysTrapWinInvertPolygon,			kProscribedGhost },
	{ sysTrapWinPaintArc,				kProscribedGhost },
	{ sysTrapWinPaintPolygon,			kProscribedGhost },

/*
	6.1.6	Unimplemented NOP Traps

		These traps should not be called by applications.  Some third-party
		applications call these traps and it is safer to treat them as NOPs
		for backwards compatibility.
*/

	{ sysTrapFplFree,				kProscribedSystemUseOnlyAnyway },	//* supported (release ROM only)
	{ sysTrapFplInit,				kProscribedSystemUseOnlyAnyway },	//* supported (release ROM only)
	{ sysTrapHwrTimerSleep,			kProscribedSystemUseOnlyAnyway },
	{ sysTrapHwrTimerWake,			kProscribedSystemUseOnlyAnyway },
	{ sysTrapPenSleep,				kProscribedSystemUseOnlyAnyway },
	{ sysTrapPenWake,				kProscribedSystemUseOnlyAnyway },
	{ sysTrapSerReceiveISP,			kProscribedSystemUseOnlyAnyway },
//	{ sysTrapSrmSleep,				kProscribedSystemUseOnlyAnyway },	// sysTrapSerialDispatch
//	{ sysTrapSrmWake,				kProscribedSystemUseOnlyAnyway },	// sysTrapSerialDispatch
	{ sysTrapSysDisableInts,		kProscribedSystemUseOnlyAnyway },
	{ sysTrapSysRestoreStatus,		kProscribedSystemUseOnlyAnyway },
	{ sysTrapTimHandleInterrupt,	kProscribedSystemUseOnlyAnyway },
	{ sysTrapTimSleep,				kProscribedSystemUseOnlyAnyway },
	{ sysTrapTimWake,				kProscribedSystemUseOnlyAnyway },
	{ sysTrapWinDisableWindow,		kProscribedSystemUseOnlyAnyway },
	{ sysTrapWinEnableWindow,		kProscribedSystemUseOnlyAnyway },
	{ sysTrapWinInitializeWindow,	kProscribedSystemUseOnlyAnyway },

/*
	6.1.7	Unimplemented Rare Traps

		These traps all seem like traps that are only used internally by
		Palm OS or by serial drivers or by OEM extensions, etc.  In other
		words, these are traps that an application would not use.
*/

	{ sysTrapConGetS,						kProscribedRare },
	{ sysTrapConPutS,						kProscribedRare },
	{ sysTrapDayDrawDays,					kProscribedRare },
	{ sysTrapDayDrawDaySelector,			kProscribedRare },
	{ sysTrapDbgCommSettings,				kProscribedRare },
	{ sysTrapDbgGetMessage,					kProscribedRare },
//	{ sysTrapDlkControl,					kProscribedRare },	//* supports sysAppLaunchCmdHandleSyncCallApp
	{ sysTrapDlkDispatchRequest,			kProscribedRare },	//* sort of, returns dlkErrNoSession err
	{ sysTrapDlkStartServer,				kProscribedRare },	//* sort of, returns dlkErrNoSession err
	{ sysTrapDmMoveOpenDBContext,			kProscribedRare },
	{ sysTrapDmOpenDBWithLocale,			kProscribedRare },
	{ sysTrapFlashCompress,					kProscribedRare },
	{ sysTrapFlashErase,					kProscribedRare },
	{ sysTrapFlashProgram,					kProscribedRare },
	{ sysTrapMemGetRomNVParams,				kProscribedRare },
	{ sysTrapMemNVParams,					kProscribedRare },
	{ sysTrapResLoadForm,					kProscribedRare },
	{ sysTrapSlkSetSocketListener,			kProscribedRare },
	{ sysTrapSysNotifyDatabaseAdded,		kProscribedRare },
	{ sysTrapSysNotifyDatabaseRemoved,		kProscribedRare },
	{ sysTrapSysSetTrapAddress,				kProscribedRare }
};

static Bool	gProscribedTrapInitialized;
static Bool	gProscribedTrap[sysNumTraps];

Bool ProscribedFunction (const SystemCallContext& context)
{
	// Build up our table that allows this function to operate quickly.
	// We turn a table of the system function dispatch numbers we want
	// to warn about into a sparse array of booleans, where the index
	// into the table is the trap index number, and each entry says
	// whether or not to warn about the call.

	if (!gProscribedTrapInitialized)
	{
		gProscribedTrapInitialized = true;

		for (size_t ii = 0; ii < countof (kProscribedFunctionTrapWords); ++ii)
		{
			gProscribedTrap[::SysTrapIndex (kProscribedFunctionTrapWords[ii].fTrapWord)] = true;
		}
	}

	// Handle system functions (as opposed to library functions).

	if (::IsSystemTrap (context.fTrapWord))
	{
		// If this trap number is not on our list, let it pass.

		if (gProscribedTrap[context.fTrapIndex])
			return true;

		// Some system functions are sub-dispatched with a sub-dispatch
		// number in register D2.  We can find this number in context.fExtra.
		// Look at the combination to see if they represent proscribed
		// functions.

		if (context.fTrapWord == sysTrapSerialDispatch)
		{
			if (context.fExtra == sysSerialSleep ||
				context.fExtra == sysSerialWake)
			{
				return true;
			}
		}
	}

	// Looks like an OK function, so don't warn.

	return false;
}

int GetProscribedReason (const SystemCallContext& context)
{
	// Handle system functions (as opposed to library functions).

	if (::IsSystemTrap (context.fTrapWord))
	{
		// If this trap number is on our list, return the reason.

		for (size_t ii = 0; ii < countof (kProscribedFunctionTrapWords); ++ii)
		{
			if (kProscribedFunctionTrapWords[ii].fTrapWord == context.fTrapWord)
			{
				return kProscribedFunctionTrapWords[ii].fReason;
			}
		}

		// Some system functions are sub-dispatched with a sub-dispatch
		// number in register D2.  We can find this number in context.fExtra.
		// Look at the combination to see if they represent proscribed
		// functions.

		if (context.fTrapWord == sysTrapSerialDispatch)
		{
			if (context.fExtra == sysSerialSleep ||
				context.fExtra == sysSerialWake)
			{
				return kProscribedSystemUseOnlyAnyway;
			}
		}
	}

	EmAssert (false);

	return 0;
}


/***********************************************************************
 *
 * FUNCTION:	GetFunctionAddress
 *
 * DESCRIPTION:	Determines the address of a system function,
 *
 * PARAMETERS:	trapWord - the dispatch number used to invoke the
 *					function(the number after the TRAP $F opocde)
 *
 *				extra - an optional extra parameter.  This parameter can
 *					be something like a library reference number, or a
 *					register to be used in a sub-dispatcher.
 *
 *				digDeep - a boolean saying how hard we should look for
 *					the function address.  If false, we just return the
 *					address generated by the ROM TrapDispatcher
 *					function.  Otherwise, for certain special ROM
 *					routines (like the Floating Point Manager entry
 *					point), we look deeper.
 *
 * RETURNED:	The function's address, or EmMemNULL if it could not
 *				be determined.
 *
 ***********************************************************************/

emuptr GetFunctionAddress (uint16 trapWord, uint32 extra, Bool digDeep)
{
	// Ensure that it's in canonical format.

	trapWord = ::SysTrapIndex (trapWord) | sysTrapBase;

	// If it's a regular system function (0xA000-0xA7FFF), handle it.

	if (::IsSystemTrap (trapWord))
		return ::GetSysFunctionAddress (trapWord, extra, digDeep);

	// Assume it's a library function call.

	return ::GetLibFunctionAddress (trapWord, (UInt16) extra, digDeep);
}


/***********************************************************************
 *
 * FUNCTION:	GetLibFunctionAddress
 *
 * DESCRIPTION:	Determines the address of a library function,
 *
 * PARAMETERS:	trapWord - the dispatch number used to invoke the
 *					function(the number after the TRAP $F opocde)
 *
 *				extra - an optional extra parameter.  This parameter can
 *					be something like a library reference number, or a
 *					register to be used in a sub-dispatcher.
 *
 *				digDeep - a boolean saying how hard we should look for
 *					the function address.  If false, we just return the
 *					address generated by the ROM TrapDispatcher
 *					function.  Otherwise, for certain special ROM
 *					routines (like the Floating Point Manager entry
 *					point), we look deeper.
 *
 * RETURNED:	The function's address, or EmMemNULL if it could not
 *				be determined.
 *
 ***********************************************************************/

emuptr GetLibFunctionAddress (uint16 trapWord, UInt16 refNum, Bool digDeep)
{
	if (refNum == sysInvalidRefNum || refNum == 0)
		throw EmInvalidRefNumException (0);

	CEnableFullAccess	munge;	// Remove blocks on memory access.

	/*
		The System Library Table (sysLibTableP) is an array of
		sysLibTableEntries entries.  Each entry has the following
		format:

			Ptr*		dispatchTblP;	// pointer to library dispatch table
			void*		globalsP;		// Library globals
			LocalID 	dbID;			// database id of the library
			VoidPtr 	codeRscH;		// library code resource handle for RAM-based libraries

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

	uint32	sysLibTableP		= EmLowMem_GetGlobal (sysLibTableP);
	UInt16	sysLibTableEntries	= EmLowMem_GetGlobal (sysLibTableEntries);

	if (sysLibTableP == EmMemNULL)
	{
		// !!! No library table!
		EmAssert (false);
		return EmMemNULL;
	}

	if (refNum >= sysLibTableEntries)
	{
		// See comments in HtalLibHeadpatch::HtalLibSendReply.
		if (refNum == kMagicRefNum)
		{
			return 1;
		}

		// RefNum out of range.

		throw EmInvalidRefNumException (0, sysLibTableEntries);
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

	// Validate the dispatch number.  See if the library is one that
	// we know about.  If so, compare the dispatch number against
	// the list of known dispatch number limits.
	
	// The first entry in the table is always the offset from the
	// start of the table to the library name.	Use this information
	// get the library name.

	int16 	offset = EmMemGet16 (dispatchTblP + ::LibTrapIndex (sysLibTrapName) * 2);
	emuptr 	libNameP = dispatchTblP + offset;
	char	libName[256];
	EmMem_strcpy (libName, libNameP);

	// Iterate over our list of Palm OS-supplied libraries to see
	// if this is one of them.	If so, the library is known to follow
	// a format that allows us to determine the size of the table.

	const char**	knownLibNameP = gPalmOSLibraries;

	while (*knownLibNameP)
	{
		if (strcmp (*knownLibNameP, libName) != 0)
		{
			knownLibNameP++;
			continue;
		}

		// OK, it's one of our libraries.  Now get the *second*
		// entry in the table.	This is the offset from the start
		// of the table to the entry point for the standard
		// "library open" function.  All Palm OS libraries follow
		// the convention where this offset points to a JMP
		// instruction immediately following the function table.
		// We can use this information to calculate the size of
		// the table.

		offset = EmMemGet16 (dispatchTblP + LibTrapIndex (sysLibTrapOpen) * 2);

		if (::LibTrapIndex (trapWord) < (offset / 2))
		{
			// It's a known library, and the offset is in range,
			// so break out of here so that we can get one with
			// actually calling the function.
			break;
		}

		// It's a known library, but the offset is out of range,
		// so report a problem.

		throw EmUnimplementedFunctionException (knownLibNameP - gPalmOSLibraries);
	}

	// Either we didn't know about that library, or we did and the
	// version test passed.  So go ahead and jump.

	offset = EmMemGet16 (dispatchTblP + LibTrapIndex (trapWord) * 2);
	emuptr result = dispatchTblP + offset;

	if (digDeep && EmMemGet16 (result) == kOpcode_JMPREL)
	{
		result += 2;
		result += (int16) EmMemGet16 (result);
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	GetSysFunctionAddress
 *
 * DESCRIPTION:	Determines the address of a non-library function,
 *
 * PARAMETERS:	trapWord - the dispatch number used to invoke the
 *					function(the number after the TRAP $F opocde)
 *
 *				extra - an optional extra parameter.  This parameter can
 *					be something like a library reference number, or a
 *					register to be used in a sub-dispatcher.
 *
 *				digDeep - a boolean saying how hard we should look for
 *					the function address.  If false, we just return the
 *					address generated by the ROM TrapDispatcher
 *					function.  Otherwise, for certain special ROM
 *					routines (like the Floating Point Manager entry
 *					point), we look deeper.
 *
 * RETURNED:	The function's address, or EmMemNULL if it could not
 *				be determined.
 *
 ***********************************************************************/

emuptr GetSysFunctionAddress (uint16 trapWord, uint32 extra, Bool digDeep)
{
	emuptr unimplementedAddress	= EmLowMem::GetTrapAddress (sysTrapSysUnimplemented);
	emuptr result				= EmLowMem::GetTrapAddress (trapWord);

	if (result == EmMemNULL ||
		((result == unimplementedAddress) && (trapWord != sysTrapSysUnimplemented)))
	{
		// sysTrapHostControl is always implemented, so make sure we
		// don't return a NULL address.

		if (::SysTrapIndex (trapWord) == ::SysTrapIndex (sysTrapHostControl))
		{
			return 0x12345678;
		}

		throw EmUnimplementedFunctionException ();
	}

	if (digDeep)
	{
		emuptr result2 = EmMemNULL;

		if (trapWord == sysTrapIntlDispatch)
		{
			result2 = ::GetIntlDispatchAddress (result, extra);
		}

		else if (
			trapWord == sysTrapOmDispatch		||
			trapWord == sysTrapTsmDispatch		||
			trapWord == sysTrapFlpDispatch 		||
			trapWord == sysTrapSerialDispatch)
		{
			result2 = ::GetStdDispatchAddress (result, extra);
		}

		else if (trapWord == sysTrapFlpEmDispatch)
		{
			result2 = ::GetFlpEmDispatchAddress (result, extra);
		}

		if (result2)
			result = result2;
	}

	if (!result)
		throw EmUnimplementedFunctionException ();

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	GetStdDispatchAddress
 *
 * DESCRIPTION:	Determine the address of a function that is reached
 *				via a sub-dispatcher.
 *
 * PARAMETERS:	entryPt - the entry point of the sub-dispatch function.
 *
 *				regD2 - sub-dispatch selector.
 *
 * RETURNED:	The function's address, or EmMemNULL if it could not
 *				be determined.
 *
 ***********************************************************************/

emuptr GetStdDispatchAddress (emuptr entryPt, uint32 regD2)
{
	CEnableFullAccess	munge;	// Remove blocks on memory access.

	/*
		The standard dispatch routine code looks like this...

			+$0000	10D5D7FE  *CMP.W	 #$0018,D2		| B47C 0018
			+$0004	10D5D802   BHI.S	 @error 		| 626C
			+$0006	10D5D804   ADD.W	 D2,D2			| D442
			+$0008	10D5D806   ADD.W	 D2,D2			| D442
			+$000A	10D5D808   JMP		 *+$0004(D2.W)	| 4EFB 2002

			+$000E	10D5D80C   JMP		 function1		| 4EFA F732
			+$0012	10D5D810   JMP		 function2		| 4EFA 0AB8
			...
	*/

	// check for expected opcodes

	if (EmMemGet16 (entryPt + 0x00) != kOpcode_CMPW ||
		EmMemGet16 (entryPt + 0x06) != kOpcode_ADDW ||
		EmMemGet16 (entryPt + 0x08) != kOpcode_ADDW ||
		EmMemGet32 (entryPt + 0x0A) != kOpcode_JMPPC)
	{
		return EmMemNULL;
	}

	// load the max selector value, and compare to D2.W

	uint16 maxSelector = EmMemGet16 (entryPt + 2);

	if ((regD2 & 0x0000FFFF) > maxSelector)
	{
		return EmMemNULL;
	}

	// point to appropriate JMP xxx instruction.

	emuptr result = entryPt + 14 + (regD2 * 4);

	if (EmMemGet16 (result) != kOpcode_JMPREL)
	{
		return EmMemNULL;
	}

	result += 2;
	result += (int16) EmMemGet16 (result);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	GetFlpEmDispatchAddress
 *
 * DESCRIPTION:	Determine the address of a function that is reached
 *				via a sub-dispatcher.
 *
 * PARAMETERS:	entryPt - the entry point of the sub-dispatch function.
 *
 *				regD2 - sub-dispatch selector.
 *
 * RETURNED:	The function's address, or EmMemNULL if it could not
 *				be determined.
 *
 ***********************************************************************/

emuptr GetFlpEmDispatchAddress (emuptr entryPt, uint32 regD2)
{
	CEnableFullAccess	munge;	// Remove blocks on memory access.

	/*
		The FlpEm dispatch routine code looks like this...

			+$0000	10D5D804   ADD.W	 D2,D2				| D442
			+$0002	10D5D806   ADD.W	 D2,D2				| D442
			+$0004	10D5D808   JMP		 *+$0004(D2.W)		| 4EFB 2002

			+$0008	10D5D80C   JMP		 _fp_round			| 4EFA F732
			+$000A	10D5D810   JMP		 _fp_get_fpscr		| 4EFA 0AB8
			...
	*/

	// check for expected opcodes

	if (EmMemGet16 (entryPt + 0x00) != kOpcode_ADDW ||
		EmMemGet16 (entryPt + 0x02) != kOpcode_ADDW ||
		EmMemGet32 (entryPt + 0x04) != kOpcode_JMPPC)
	{
		return EmMemNULL;
	}

	// point to appropriate JMP xxx instruction.

	emuptr result = entryPt + 8 + (regD2 * 4);

	if (EmMemGet16 (result) != kOpcode_JMPREL)
	{
		return EmMemNULL;
	}

	result += 2;
	result += (int16) EmMemGet16 (result);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	GetIntlDispatchAddress
 *
 * DESCRIPTION:	Determine the address of an IntlDispatch function that
 *				is reached via a sub-dispatcher.
 *
 * PARAMETERS:	entryPt - the entry point of the sub-dispatch function.
 *
 *				regD2 - sub-dispatch selector.
 *
 * RETURNED:	The function's address, or EmMemNULL if it could not
 *				be determined.
 *
 ***********************************************************************/

emuptr GetIntlDispatchAddress (emuptr entryPt, uint32 regD2)
{
	CEnableFullAccess	munge;	// Remove blocks on memory access.

	/*
		The original IntlDispatch routine code looks like this...

			+$0000	10D5D7FE  *CMP.W	 #$0018,D2		| B47C 0018
			+$0004	10D5D802   BHI.S	 @error 		| 626C
			+$0006	10D5D804   ADD.W	 D2,D2			| D442
			+$0008	10D5D806   ADD.W	 D2,D2			| D442
			+$000A	10D5D808   JMP		 *+$0004(D2.W)	| 4EFB 2002

			+$000E	10D5D80C   JMP		 IntlInit		| 4EFA F732
			+$0012	10D5D810   JMP		 TxtByteAttr	| 4EFA 0AB8
			...

		In 4.0, the IntlDispatch routine code looks like this...

			+$0000	10C1AE18   CMP.W     #$001B,D2			| B47C 001B 
			+$0004	10C1AE1C   BHI.S     @error				| 6212 
			+$0006	10C1AE1E   ADD.W     D2,D2				| D442 
			+$0008	10C1AE20   ADD.W     D2,D2				| D442 
			+$000A	10C1AE22   MOVE.L    $00000350,D0		| 2038 0350 
			+$000E	10C1AE26   BEQ.S     @standard			| 670E
			
			+$0010	10C1AE28   MOVE.L    D0,A0				| 2040 
			+$0012	10C1AE2A   MOVE.L    $00(A0,D2.W),A0	| 2070 2000 
			+$0016	10C1AE2E   JMP       (A0)				| 4ED0
			
			@error:
			+$0018	10C1AE30   MOVE.W    D2,-(A7)			| 3F02 
			+$001A	10C1AE32   JMP       selectorError		| 4EFA 0086 
			
			@standard:
			+$001E	10C1AE36   JMP       *+$0004(D2.W)		| 4EFB 2002
			
			+$0022	10C1AE3A   JMP       IntlInit			| 4EFA 0260 
			+$0022	10C1AE3E   JMP       TxtByteAttr		| 4EFA EB60 
			...
	*/

	// check for expected opcodes

	Bool	newDispatch;

	if (EmMemGet16 (entryPt + 0x00) != kOpcode_CMPW ||
		EmMemGet16 (entryPt + 0x06) != kOpcode_ADDW ||
		EmMemGet16 (entryPt + 0x08) != kOpcode_ADDW)
	{
		return EmMemNULL;
	}

	if (EmMemGet16 (entryPt + 0x0A) == kOpcode_JMPPC)
	{
		newDispatch = false;
	}
	else if (EmMemGet16 (entryPt + 0x0A) == kOpcode_MOVEL)
	{
		newDispatch = true;
	}
	else
	{
		return EmMemNULL;
	}


	// load the max selector value, and compare to D2.W

	uint16 maxSelector = EmMemGet16 (entryPt + 2);

	if ((regD2 & 0x0000FFFF) > maxSelector)
	{
		return EmMemNULL;
	}

	emuptr result;
	if (newDispatch)
	{
		// figure out if low memory pointer exists.
		emuptr lowMemPtr = EmMemGet16 (entryPt + 0x0C);
		result = EmMemGet32 (lowMemPtr);

		if (result == EmMemNULL)
		{
			newDispatch = false;
			// point to start of dispatch table.
			result = entryPt + 34;
		}
	}
	else
	{
		// point to start of dispatch table.
		result = entryPt + 14;
	}

	if (newDispatch)
	{
		// result points to table of routine addresses.
		result = EmMemGet32 (result + (regD2 * 4));
	}
	else
	{
		// point to appropriate JMP xxx instruction.
		result += (regD2 * 4);
	
		if (EmMemGet16 (result) != kOpcode_JMPREL)
		{
			return EmMemNULL;
		}
	
		result += 2;
		result += (int16) EmMemGet16 (result);
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	GetTrapName
 *
 * DESCRIPTION:	Returns a pointer to a string appropriate for
 *				identifying a function.  If the function has a Macsbug
 *				name, that name is returned.  If the function is a
 *				known Palm OS function, that name is looked up in a
 *				string resource list.  Otherwise, a default "unknown"
 *				string is returned.
 *
 * PARAMETERS:	context - structure describing the function.
 *
 *				trapWord - the dispatch number used to invoke the
 *					function(the number after the TRAP $F opocde).
 *
 *				extra - an optional extra parameter.  This parameter can
 *					be something like a library reference number, or a
 *					register to be used in a sub-dispatcher.
 *
 *				digDeep - a boolean saying how hard we should look for
 *					the function address.  If false, we just return the
 *					address generated by the ROM TrapDispatcher
 *					function.  Otherwise, for certain special ROM
 *					routines (like the Floating Point Manager entry
 *					point), we look deeper.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

char* GetTrapName (const SystemCallContext& context, Bool digDeep)
{
	return ::GetTrapName (context.fTrapWord, context.fExtra, digDeep);
}


char* GetTrapName (uint16 trapWord, uint32 extra, Bool digDeep)
{
	static char name[sysPktMaxNameLen];

	try
	{
		name[0] = 0;
		::FindTrapName (trapWord, name, extra, digDeep);
	}
	catch (...)
	{
	}

	if (strlen (name) == 0)
	{
		strcpy (name, ">>> Unknown function name <<<");

		if (::IsSystemTrap (trapWord))
		{
			string	knownName = Platform::GetString (kStr_SysTrapBase + SysTrapIndex (trapWord));
			if (knownName[0] != '<')
			{
				strcpy (name, knownName.c_str ());
			}
		}
	}

	return name;
}


/***********************************************************************
 *
 * FUNCTION:	FindTrapName
 *
 * DESCRIPTION:	Finds the Macsbug name for the given function.	If the
 *				name cannot be found, an empty string is returned.
 *
 * PARAMETERS:	trapWord - the dispatch number used to invoke the
 *					function(the number after the TRAP $F opocde)
 *
 *				nameP - buffer to receive the function name.
 *
 *				extra - an optional extra parameter.  This parameter can
 *					be something like a library reference number, or a
 *					register to be used in a sub-dispatcher.
 *
 *				digDeep - a boolean saying how hard we should look for
 *					the function address.  If false, we just return the
 *					address generated by the ROM TrapDispatcher
 *					function.  Otherwise, for certain special ROM
 *					routines (like the Floating Point Manager entry
 *					point), we look deeper.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void FindTrapName (uint16 trapWord, char* nameP, uint32 extra, Bool digDeep)
{
	emuptr addr = ::GetFunctionAddress (trapWord, extra, digDeep);

	if (addr)
		::FindFunctionName (addr, nameP);
	else
		nameP[0] = '\0';
}


/***********************************************************************
 *
 * FUNCTION:	FindFunctionName
 *
 * DESCRIPTION:	Returns information about the function containing the
 *				the given memory address, including the function's
 *				start (the LINK instruction, usually), the function's
 *				end (just after the RTS, usually), and the function's
 *				name (that follows the function).
 *
 * PARAMETERS:	addr - address contained within the function.
 *
 *				nameP - storage for the returned function name.  Must
 *					be at least 32 characters.
 *
 *				startAddrP - storage for the returned function start.
 *
 *				endAddrP - storage for the returned function end.
 *
 *				nameCapacity - bytes of storage available at nameP
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/
 
// Llamagraphics, Inc:	Added nameCapacity argument so that callers
// can retrieve more than 31 character function names.	The default
// capacity is 32, so callers that don't provide the extra argument
// will get the same result as before.

void FindFunctionName (emuptr addr, char* nameP, 
			emuptr* startAddrP, emuptr* endAddrP,
			long nameCapacity)
{
	// Get the start address only if requested.

	if (startAddrP)
		*startAddrP = ::FindFunctionStart (addr);

	// Get the end address if requested or if we need it to
	// get the Macsbug name.

	if (nameP || endAddrP)
	{
		emuptr endAddr = ::FindFunctionEnd (addr);

		// Return the end address if requested.

		if (endAddrP)
			*endAddrP = endAddr;

		// Return the Macsbug name if requested.

		if (nameP)
		{
			if (endAddr)
				::GetMacsbugInfo (endAddr, nameP, nameCapacity, NULL);
			else
				nameP[0] = '\0';
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	FindFunctionStart
 *
 * DESCRIPTION:	Find the start of the function containing the given
 *				address.  The start of the function is determined
 *				by looking backwards for the end of the previous
 *				function and then scooting forward to the beginning
 *				of this function.
 *
 * PARAMETERS:	addr - the probe address.
 *
 * RETURNED:	Start of the function.  EmMemNULL if not found.
 *
 ***********************************************************************/

emuptr FindFunctionStart (emuptr addr)
{
	emuptr	beginAddr = addr - 0x02000;	// Set a default value.

	// Try finding the distance from the given address to the beginning
	// of the chunk it's in, and use that as maxLength.

	const EmPalmHeap*	heap = EmPalmHeap::GetHeapByPtr (addr);
	if (heap)
	{
		const EmPalmChunk*	chunk = heap->GetChunkContaining (addr);
		if (chunk)
		{
			beginAddr = chunk->BodyStart ();
		}
	}

	while ((addr -= 2) >= beginAddr)
	{
		// Make sure the address is valid.

		if (!EmMemCheckAddress (addr, 2))
		{
			return EmMemNULL;
		}

		if (::EndOfFunctionSequence (addr))
		{
			addr += 2;	// skip past the final RTS (or whatever).

			// Skip over the Macsbug name (and any constant data).

			::GetMacsbugInfo (addr, NULL, 0, &addr);

			return addr;
		}
	}

	// !!! TBD: if we got here, it's because we couldn't find the
	// EOF sequence of a previous function.  That might be because
	// the function we're probing is the first in the chunk.  In
	// that case, return chunk->Start() or chunk->Start() + 2 as
	// the beginning of the function.  I'm not doing that now because
	// I'm not sure which is right, or if there are other conditions
	// to take into account.

	return EmMemNULL;
}


/***********************************************************************
 *
 * FUNCTION:	FindFunctionEnd
 *
 * DESCRIPTION:	Find the end of the function containing the given
 *				address.  The end of the function is determined by
 *				testing the given address for an end-of-function
 *				sequence, and then scooting forward by two bytes
 *				if the test fails.
 *
 * PARAMETERS:	addr - the probe address.
 *
 * RETURNED:	End of the function.  EmMemNULL if not found.
 *
 ***********************************************************************/

emuptr FindFunctionEnd (emuptr addr)
{
	emuptr	endAddr = addr + 0x02000;	// Set a default value.

	// Try finding the distance from the given address to the end
	// of the chunk it's in, and use that as maxLength.

	const EmPalmHeap*	heap = EmPalmHeap::GetHeapByPtr (addr);
	if (heap)
	{
		const EmPalmChunk*	chunk = heap->GetChunkContaining (addr);
		if (chunk)
		{
			endAddr = chunk->BodyEnd ();
		}
	}

	while (addr < endAddr)
	{
		// Make sure the address is valid.

		if (!EmMemCheckAddress (addr, 2))
		{
			return EmMemNULL;
		}

		if (::EndOfFunctionSequence (addr))
		{
			return addr + 2;
		}

		addr += 2;
	}

	return EmMemNULL;
}


/***********************************************************************
 *
 * FUNCTION:	EndOfFunctionSequence
 *
 * DESCRIPTION:	Test the given memory location to see if it contains
 *				a valid-looking end of function sequence.  A valid
 *				end of function sequence is an RTE, JMP (A0), RTD, or
 *				RTS (as long as it doesn't look like the sequnce
 *				CodeWarrior uses for 32-bit jumps).
 *
 * PARAMETERS:	addr - memory location to test.
 *
 * RETURNED:	True if it looks like this is an end of function
 *				sequence.
 *
 ***********************************************************************/

Bool EndOfFunctionSequence (emuptr addr)
{
	uint16 opcode = EmMemGet16 (addr);

	// Look for the special 32-bit relative jumps which
	// CodeWarrior puts in. These are of the following form:
	//
	// +$0022  10CD3C2A   PEA		*+$0010 		; 10CD3C3A		| 487A 000E
	// +$0026  10CD3C2E   PEA		*+$0006 		; 10CD3C34		| 487A 0004
	// +$002A  10CD3C32   ADDI.L	#$00000CD2,(A7) ; '....'		| 0697 0000 0CD2
	// +$0030  10CD3C38   RTS										| 4E75

	if (opcode == kOpcode_RTS &&
		EmMemGet16 (addr - 6) != kOpcode_ADD)
	{
		return true;
	}

	if (opcode == kOpcode_RTE ||
		opcode == kOpcode_JMP ||	// JMP (A0)
		opcode == kOpcode_RTD)
	{
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	GetMacsbugInfo
 *
 * DESCRIPTION:	Return any eof-of-function information.  This includes
 *				the (validated) Macsbug name and the address following
 *				all end-of-function information (presumably the start
 *				of the next function).
 *
 * PARAMETERS:	eof - pointer to the end of the function (that is, the
 *					byte after the eof sequence).
 *
 *				name - pointer to buffer to receive the name.
 *
 *				nameCapacity - size of buffer pointed to by "name".
 *
 *				sof - receives the start of the next function.
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

void GetMacsbugInfo (emuptr eof, char* name, long nameCapacity, emuptr* sof)
{
	uint8	length;
	Bool	isFixed;
	emuptr	namePtr;

	::MacsbugNameLength (eof, &length, &isFixed, &namePtr);

	// Determine the address of the following function.  Start by
	// determining the address past the end of the string.

	if (sof)
	{
		*sof = namePtr + length;

		if (isFixed)
		{
			// Assert that we're word aligned.

			EmAssert ((*sof & 0x01) == 0);
		}
		else
		{
			// Make sure the address is word aligned.

			*sof = (*sof + 1) & ~0x01;

			// Get the size of any following constant data and add that to
			// the address we return as the start of the next function.
			// Make sure the address remains word aligned.

			uint16	constDataSize = EmMemGet16 (*sof);
			*sof = (*sof + constDataSize + 2 + 1) & ~0x01;
		}
	}

	// Copy the string, validating it as we go.  Note that we have
	// to go through this loop even if "name" is NULL so that we
	// can validate all of the characters.

	char*	dest = name;
	while (length--)
	{
		uint8	ch = EmMemGet8 (namePtr++) & 0x7F;

		// If we're dealing with fixed format and we've encountered
		// a space, make sure that all remaining characters are
		// also spaces.

		if (isFixed && ch == ' ')
		{
			while (length--)
			{
				ch = EmMemGet8 (namePtr++) & 0x7F;

				if (ch != ' ')
				{
					goto Error;
				}
			}

			break;
		}

		if (!::ValidMacsbugChar (ch))
		{
			goto Error;
		}

		// If there's room in the output buffer, store the character.
		// Do it this way (as opposed to altering "length" if it's
		// larger than nameCapacity) so that we can test all characters.

		if (dest && ((dest - name) < (nameCapacity - 1)))
		{
			*dest++ = (char) ch;
		}
	}

	// Terminate the string.

	if (dest)
		*dest = 0;

	return;

Error:
	// All bets are off.

	if (name)
		*name = 0;

	if (sof)
		*sof = eof;
}


/***********************************************************************
 *
 * FUNCTION:	MascsbugNameLength
 *
 * DESCRIPTION:	Returns the length of the Macsbug symbol starting at
 *				the given memory location.
 *
 *				Note that NO validation of the symbol is performed.
 *				That is, if there is NO symbol after the function and
 *				the word immediately after the end of the function is
 *				the first opcode of the following function, the
 *				result of this function is meaningless.
 *
 * PARAMETERS:	addr - start of the Macsbug symbol.  Should contain
 *					the symbol's length byte or bytes.
 *
 *				length - receives the length of the Macsbug symbol.
 *
 *				isFixed - receives a boolean indicating if this name
 *					is in "fixed" or "variable" format.  If the former,
 *					the name is padded with spaces which need to be
 *					accounted for.	If the latter, spaces are not
 *					allowed at all.  Also, variable-length names are
 *					followed by a word containing the length of a
 *					chunk of "local data".
 *
 *				newAddr - receives the address of the first character
 *					of the Macsbug name.  This address is addr, addr+1,
 *					or addr+2, depending on the symbol format.
 *
 * RETURNED:	The symbol length (zero on error).
 *
 ***********************************************************************/

void MacsbugNameLength (emuptr addr, uint8* length, Bool* isFixed, emuptr* namePtr)
{
	/*
		The Macsbug name can be in one of three forms:

			Variable length:
				The first byte is in the range $80 to $9F and is a length
				in the range 0 to $1F. The high order bit must be set.  A
				length of 0 implies the second byte is the actual length
				in the range $01 thru $FF. The length byte(s) and name
				may be an odd number of bytes. However, the data after
				the name is word aligned.
		
			Fixed length 8:
				The first byte is in the range $20 to $7F and is an ASCII
				character.  The high order bit may be set but is not
				required to be.
		
			Fixed length 16:
				The first byte is in the range $20 to $7F and is an ASCII
				character.  The high order bit may be set but is not
				required to be.  The high order bit in the second byte is
				required to be set.  This distinguishes the two types of
				fixed names.
	*/

	*length	= EmMemGet8 (addr);

	// If < 0x20, there is no name.

	if (*length < 0x20)
	{
		*length = 0;
		*isFixed = true;
	}
	else
	{
		// If < 0x20 (after stripping), it is in variable-length format.
		// If >= 0x20 (after stripping), it is in 8- or 16-byte fixed format.

		*length &= 0x7F;
		*isFixed = (*length >= 0x20);

		if (*isFixed)
		{
			if (EmMemGet8 (addr + 1) < 0x80)
				*length = 8;
			else
				*length = 16;
		}
		else
		{
			addr++;

			if (*length == 0)
				*length = EmMemGet8 (addr++);
		}
	}

	*namePtr = addr;
}


/***********************************************************************
 *
 * FUNCTION:	ValidMacsbugChar
 *
 * DESCRIPTION:	Returns whether or not the given character is a
 *				character in a valid Macsbug symbol.  Valid characters
 *				are: [a-zA-Z0-9_%.]
 *
 * PARAMETERS:	ch - the character to test
 *
 * RETURNED:	True if the character is valid.
 *
 ***********************************************************************/

Bool ValidMacsbugChar (uint8 ch)
{
	static Bool initialized = false;
	static Bool validChar[128];

	if (!initialized)
	{
		initialized = true;

		memset (validChar, false, sizeof (validChar));

		validChar ['_'] = true;
		validChar ['%'] = true;
		validChar ['.'] = true;

		uint8	ii;

		for (ii = 'a'; ii <= 'z'; ++ii)
			validChar [ii] = true;

		for (ii = 'A'; ii <= 'Z'; ++ii)
			validChar [ii] = true;

		for (ii = '0'; ii <= '9'; ++ii)
			validChar [ii] = true;
	}

	EmAssert (ch < 128);

	return validChar [ch];
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetShortName
 *
 * DESCRIPTION:	Returns whether or not the given character is a
 *				character in a valid Macsbug symbol.  Valid characters
 *				are: [a-zA-Z0-9_%.]
 *
 * PARAMETERS:	ch - the character to test
 *
 * RETURNED:	True if the character is valid.
 *
 ***********************************************************************/

string PrvGetShortName (const char* srcName, int destLen)
{
	string	result (destLen, ' ');	// Create space-filled string of right length.

	int	iDest = 0;
	int	iSrc = 0;
	int	srcLen = strlen (srcName);

	while (iDest < destLen && iSrc < srcLen)
	{
		char	c = srcName[iSrc++];
		if (isalnum (c))
		{
			result[iDest++] = toupper (c);
		}
	}

	return result;
}
