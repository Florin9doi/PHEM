/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	Copyright (c) 2001 PocketPyro, Inc.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmPatchModuleNetLib.h"

#include "EmMemory.h"			// CEnableFullAccess
#include "EmSubroutine.h"
#include "Logging.h"			// LogAppendMsg
#include "Marshal.h"			// PARAM_VAL, etc.
#include "Platform_NetLib.h"	// Platform_NetLib

#if PLATFORM_UNIX || PLATFORM_MAC
#include <netinet/in.h>			// ntohs, ntohs
#endif


// ======================================================================
//	Patches for NetLib functions
// ======================================================================

class NetLibHeadpatch
{
	public:
		static CallROMType		NetLibOpen					(void);
		static CallROMType		NetLibClose					(void);
		static CallROMType		NetLibSleep					(void);
		static CallROMType		NetLibWake					(void);
		static CallROMType		NetLibAddrINToA				(void);
		static CallROMType		NetLibAddrAToIN				(void);
		static CallROMType		NetLibSocketOpen			(void);
		static CallROMType		NetLibSocketClose			(void);
		static CallROMType		NetLibSocketOptionSet		(void);
		static CallROMType		NetLibSocketOptionGet		(void);
		static CallROMType		NetLibSocketBind			(void);
		static CallROMType		NetLibSocketConnect			(void);
		static CallROMType		NetLibSocketListen			(void);
		static CallROMType		NetLibSocketAccept			(void);
		static CallROMType		NetLibSocketShutdown		(void);
		static CallROMType		NetLibSendPB				(void);
		static CallROMType		NetLibSend					(void);
		static CallROMType		NetLibReceivePB				(void);
		static CallROMType		NetLibReceive				(void);
		static CallROMType		NetLibDmReceive				(void);
		static CallROMType		NetLibSelect				(void);
		static CallROMType		NetLibMaster				(void);
		static CallROMType		NetLibGetHostByName			(void);
		static CallROMType		NetLibSettingGet			(void);
		static CallROMType		NetLibSettingSet			(void);
		static CallROMType		NetLibIFAttach				(void);
		static CallROMType		NetLibIFDetach				(void);
		static CallROMType		NetLibIFGet					(void);
		static CallROMType		NetLibIFSettingGet			(void);
		static CallROMType		NetLibIFSettingSet			(void);
		static CallROMType		NetLibIFUp					(void);
		static CallROMType		NetLibIFDown				(void);
		static CallROMType		NetLibGetHostByAddr			(void);
		static CallROMType		NetLibGetServByName			(void);
		static CallROMType		NetLibSocketAddr			(void);
		static CallROMType		NetLibFinishCloseWait		(void);
		static CallROMType		NetLibGetMailExchangeByName	(void);
		static CallROMType		NetLibOpenCount				(void);
		static CallROMType		NetLibTracePrintF			(void);
		static CallROMType		NetLibTracePutS				(void);
		static CallROMType		NetLibOpenIfCloseWait		(void);
		static CallROMType		NetLibHandlePowerOff		(void);
		static CallROMType		NetLibConnectionRefresh		(void);
		static CallROMType		NetLibOpenConfig			(void);
		static CallROMType		NetLibConfigMakeActive		(void);
		static CallROMType		NetLibConfigList			(void);
		static CallROMType		NetLibConfigIndexFromName	(void);
		static CallROMType		NetLibConfigDelete			(void);
		static CallROMType		NetLibConfigSaveAs			(void);
		static CallROMType		NetLibConfigRename			(void);
		static CallROMType		NetLibConfigAliasSet		(void);
		static CallROMType		NetLibConfigAliasGet		(void);
};


class NetLibTailpatch
{
	public:
		static void		NetLibOpen					(void);
		static void		NetLibClose					(void);
		static void		NetLibSleep					(void);
		static void		NetLibWake					(void);
		static void		NetLibAddrINToA				(void);
		static void		NetLibAddrAToIN				(void);
		static void		NetLibSocketOpen			(void);
		static void		NetLibSocketClose			(void);
		static void		NetLibSocketOptionSet		(void);
		static void		NetLibSocketOptionGet		(void);
		static void		NetLibSocketBind			(void);
		static void		NetLibSocketConnect			(void);
		static void		NetLibSocketListen			(void);
		static void		NetLibSocketAccept			(void);
		static void		NetLibSocketShutdown		(void);
		static void		NetLibSendPB				(void);
		static void		NetLibSend					(void);
		static void		NetLibReceivePB				(void);
		static void		NetLibReceive				(void);
		static void		NetLibDmReceive				(void);
		static void		NetLibSelect				(void);
		static void		NetLibMaster				(void);
		static void		NetLibGetHostByName			(void);
		static void		NetLibSettingGet			(void);
		static void		NetLibSettingSet			(void);
		static void		NetLibIFAttach				(void);
		static void		NetLibIFDetach				(void);
		static void		NetLibIFGet					(void);
		static void		NetLibIFSettingGet			(void);
		static void		NetLibIFSettingSet			(void);
		static void		NetLibIFUp					(void);
		static void		NetLibIFDown				(void);
		static void		NetLibGetHostByAddr			(void);
		static void		NetLibGetServByName			(void);
		static void		NetLibSocketAddr			(void);
		static void		NetLibFinishCloseWait		(void);
		static void		NetLibGetMailExchangeByName	(void);
		static void		NetLibOpenCount				(void);
		static void		NetLibTracePrintF			(void);
		static void		NetLibTracePutS				(void);
		static void		NetLibOpenIfCloseWait		(void);
		static void		NetLibHandlePowerOff		(void);
		static void		NetLibConnectionRefresh		(void);
		static void		NetLibOpenConfig			(void);
		static void		NetLibConfigMakeActive		(void);
		static void		NetLibConfigList			(void);
		static void		NetLibConfigIndexFromName	(void);
		static void		NetLibConfigDelete			(void);
		static void		NetLibConfigSaveAs			(void);
		static void		NetLibConfigRename			(void);
		static void		NetLibConfigAliasSet		(void);
		static void		NetLibConfigAliasGet		(void);
};


// ======================================================================
//	Proto patch table for NetLib functions.  This array will be used
//	to create a sparse array at runtime.
// ======================================================================

static ProtoPatchTableEntry	gProtoNetLibPatchTable[] =
{
	{sysLibTrapOpen,				NetLibHeadpatch::NetLibOpen,					NetLibTailpatch::NetLibOpen},
	{sysLibTrapClose,				NetLibHeadpatch::NetLibClose,					NetLibTailpatch::NetLibClose},
	{sysLibTrapSleep,				NetLibHeadpatch::NetLibSleep,					NetLibTailpatch::NetLibSleep},
	{sysLibTrapWake,				NetLibHeadpatch::NetLibWake,					NetLibTailpatch::NetLibWake},
	{netLibTrapAddrINToA,			NetLibHeadpatch::NetLibAddrINToA,				NetLibTailpatch::NetLibAddrINToA},
	{netLibTrapAddrAToIN,			NetLibHeadpatch::NetLibAddrAToIN,				NetLibTailpatch::NetLibAddrAToIN},
	{netLibTrapSocketOpen,			NetLibHeadpatch::NetLibSocketOpen,				NetLibTailpatch::NetLibSocketOpen},
	{netLibTrapSocketClose,			NetLibHeadpatch::NetLibSocketClose,				NetLibTailpatch::NetLibSocketClose},
	{netLibTrapSocketOptionSet,		NetLibHeadpatch::NetLibSocketOptionSet,			NetLibTailpatch::NetLibSocketOptionSet},
	{netLibTrapSocketOptionGet,		NetLibHeadpatch::NetLibSocketOptionGet,			NetLibTailpatch::NetLibSocketOptionGet},
	{netLibTrapSocketBind,			NetLibHeadpatch::NetLibSocketBind,				NetLibTailpatch::NetLibSocketBind},
	{netLibTrapSocketConnect,		NetLibHeadpatch::NetLibSocketConnect,			NetLibTailpatch::NetLibSocketConnect},
	{netLibTrapSocketListen,		NetLibHeadpatch::NetLibSocketListen,			NetLibTailpatch::NetLibSocketListen},
	{netLibTrapSocketAccept,		NetLibHeadpatch::NetLibSocketAccept,			NetLibTailpatch::NetLibSocketAccept},
	{netLibTrapSocketShutdown,		NetLibHeadpatch::NetLibSocketShutdown,			NetLibTailpatch::NetLibSocketShutdown},
	{netLibTrapSendPB,				NetLibHeadpatch::NetLibSendPB,					NetLibTailpatch::NetLibSendPB},
	{netLibTrapSend,				NetLibHeadpatch::NetLibSend,					NetLibTailpatch::NetLibSend},
	{netLibTrapReceivePB,			NetLibHeadpatch::NetLibReceivePB,				NetLibTailpatch::NetLibReceivePB},
	{netLibTrapReceive,				NetLibHeadpatch::NetLibReceive,					NetLibTailpatch::NetLibReceive},
	{netLibTrapDmReceive,			NetLibHeadpatch::NetLibDmReceive,				NetLibTailpatch::NetLibDmReceive},
	{netLibTrapSelect,				NetLibHeadpatch::NetLibSelect,					NetLibTailpatch::NetLibSelect},
//	{netLibTrapPrefsGet,			NULL,											NULL},
//	{netLibTrapPrefsSet,			NULL,											NULL},
//	{netLibTrapDrvrWake,			NULL,											NULL},
//	{netLibTrapInterfacePtr,		NULL,											NULL},
	{netLibTrapMaster,				NetLibHeadpatch::NetLibMaster,					NetLibTailpatch::NetLibMaster},
	{netLibTrapGetHostByName,		NetLibHeadpatch::NetLibGetHostByName,			NetLibTailpatch::NetLibGetHostByName},
	{netLibTrapSettingGet,			NetLibHeadpatch::NetLibSettingGet,				NetLibTailpatch::NetLibSettingGet},
	{netLibTrapSettingSet,			NetLibHeadpatch::NetLibSettingSet,				NetLibTailpatch::NetLibSettingSet},
	{netLibTrapIFAttach,			NetLibHeadpatch::NetLibIFAttach,				NetLibTailpatch::NetLibIFAttach},
	{netLibTrapIFDetach,			NetLibHeadpatch::NetLibIFDetach,				NetLibTailpatch::NetLibIFDetach},
	{netLibTrapIFGet,				NetLibHeadpatch::NetLibIFGet,					NetLibTailpatch::NetLibIFGet},
	{netLibTrapIFSettingGet,		NetLibHeadpatch::NetLibIFSettingGet,			NetLibTailpatch::NetLibIFSettingGet},
	{netLibTrapIFSettingSet,		NetLibHeadpatch::NetLibIFSettingSet,			NetLibTailpatch::NetLibIFSettingSet},
	{netLibTrapIFUp,				NetLibHeadpatch::NetLibIFUp,					NetLibTailpatch::NetLibIFUp},
	{netLibTrapIFDown,				NetLibHeadpatch::NetLibIFDown,					NetLibTailpatch::NetLibIFDown},
//	{netLibTrapIFMediaUp,			NULL,											NULL},
//	{netLibTrapScriptExecute,		NULL,											NULL},
	{netLibTrapGetHostByAddr,		NetLibHeadpatch::NetLibGetHostByAddr,			NetLibTailpatch::NetLibGetHostByAddr},
	{netLibTrapGetServByName,		NetLibHeadpatch::NetLibGetServByName,			NetLibTailpatch::NetLibGetServByName},
	{netLibTrapSocketAddr,			NetLibHeadpatch::NetLibSocketAddr,				NetLibTailpatch::NetLibSocketAddr},
	{netLibTrapFinishCloseWait,		NetLibHeadpatch::NetLibFinishCloseWait,			NetLibTailpatch::NetLibFinishCloseWait},
	{netLibTrapGetMailExchangeByName,NetLibHeadpatch::NetLibGetMailExchangeByName,	NetLibTailpatch::NetLibGetMailExchangeByName},
//	{netLibTrapPrefsAppend,			NULL,											NULL},
//	{netLibTrapIFMediaDown,			NULL,											NULL},
	{netLibTrapOpenCount,			NetLibHeadpatch::NetLibOpenCount,				NetLibTailpatch::NetLibOpenCount},
	{netLibTrapTracePrintF,			NetLibHeadpatch::NetLibTracePrintF,				NetLibTailpatch::NetLibTracePrintF},
	{netLibTrapTracePutS,			NetLibHeadpatch::NetLibTracePutS,				NetLibTailpatch::NetLibTracePutS},
	{netLibTrapOpenIfCloseWait,		NetLibHeadpatch::NetLibOpenIfCloseWait,			NetLibTailpatch::NetLibOpenIfCloseWait},
	{netLibTrapHandlePowerOff,		NetLibHeadpatch::NetLibHandlePowerOff,			NetLibTailpatch::NetLibHandlePowerOff},
	{netLibTrapConnectionRefresh,	NetLibHeadpatch::NetLibConnectionRefresh,		NetLibTailpatch::NetLibConnectionRefresh},
//	{netLibTrapBitMove,				NULL,											NULL},
//	{netLibTrapBitPutFixed,			NULL,											NULL},
//	{netLibTrapBitGetFixed,			NULL,											NULL},
//	{netLibTrapBitPutUIntV,			NULL,											NULL},
//	{netLibTrapBitGetUIntV,			NULL,											NULL},
//	{netLibTrapBitPutIntV,			NULL,											NULL},
//	{netLibTrapBitGetIntV,			NULL,											NULL},
	{netLibOpenConfig,				NetLibHeadpatch::NetLibOpenConfig,				NetLibTailpatch::NetLibOpenConfig},
	{netLibConfigMakeActive,		NetLibHeadpatch::NetLibConfigMakeActive,		NetLibTailpatch::NetLibConfigMakeActive},
	{netLibConfigList,				NetLibHeadpatch::NetLibConfigList,				NetLibTailpatch::NetLibConfigList},
	{netLibConfigIndexFromName,		NetLibHeadpatch::NetLibConfigIndexFromName,		NetLibTailpatch::NetLibConfigIndexFromName},
	{netLibConfigDelete,			NetLibHeadpatch::NetLibConfigDelete,			NetLibTailpatch::NetLibConfigDelete},
	{netLibConfigSaveAs,			NetLibHeadpatch::NetLibConfigSaveAs,			NetLibTailpatch::NetLibConfigSaveAs},
	{netLibConfigRename,			NetLibHeadpatch::NetLibConfigRename,			NetLibTailpatch::NetLibConfigRename},
	{netLibConfigAliasSet,			NetLibHeadpatch::NetLibConfigAliasSet,			NetLibTailpatch::NetLibConfigAliasSet},
	{netLibConfigAliasGet,			NetLibHeadpatch::NetLibConfigAliasGet,			NetLibTailpatch::NetLibConfigAliasGet},

	{0,								NULL,											NULL}
};




// ======================================================================
//	Private functions
// ======================================================================

static const char*			PrvGetOptLevelString (NetSocketOptLevelEnum);
static const char*			PrvGetOptString (NetSocketOptLevelEnum, NetSocketOptEnum);
static const char*			PrvGetDottedIPString (const NetSocketAddrType& ipAddr);
static const char*			PrvGetPortString (const NetSocketAddrType& ipAddr);
static const char*			PrvGetErrorString (Err err);


#define PRINTF	if (!LogNetLib ()) ; else LogAppendMsg


#pragma mark -

// ===========================================================================
//		¥ EmPatchModuleNetLib
// ===========================================================================

/***********************************************************************
 *
 * FUNCTION:	EmPatchModuleNetLib::EmPatchModuleNetLib
 *
 * DESCRIPTION:	Constructor
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

EmPatchModuleNetLib::EmPatchModuleNetLib() :
	EmPatchModule ("Net.lib", gProtoNetLibPatchTable)
{
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibOpen
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibOpen (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibOpen");

	//	Err	NetLibOpen (UInt16 libRefNum, UInt16 *netIFErrsP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 *netIFErrsP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_REF (UInt16, netIFErrsP, Marshal::kOutput);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::Open (libRefNum, netIFErrsP);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (netIFErrsP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibOpen (void)
{
	//	Err	NetLibOpen (UInt16 libRefNum, UInt16 *netIFErrsP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 *netIFErrsP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_REF (UInt16, netIFErrsP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*netIFErrsP = %s (0x%04X)", PrvGetErrorString (*netIFErrsP), (long) *netIFErrsP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibClose
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibClose (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibClose");

	//	Err NetLibClose (UInt16 libRefNum, UInt16 immediate)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 immediate");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (UInt16, immediate);

	// Examine the parameters
	PRINTF ("\timmediate = 0x%08X", (long) immediate);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::Close (libRefNum, immediate);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibClose (void)
{
	//	Err NetLibClose (UInt16 libRefNum, UInt16 immediate)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 immediate");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (UInt16, immediate);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSleep
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSleep (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSleep");

	//	Err NetLibSleep (UInt16 libRefNum)

	CALLED_SETUP ("Err", "UInt16 libRefNum");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::Sleep (libRefNum);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}


void NetLibTailpatch::NetLibSleep (void)
{
	//	Err NetLibSleep (UInt16 libRefNum)

	CALLED_SETUP ("Err", "UInt16 libRefNum");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibWake
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibWake (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibWake");

	//	Err NetLibWake (UInt16 libRefNum)

	CALLED_SETUP ("Err", "UInt16 libRefNum");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::Wake (libRefNum);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibWake (void)
{
	//	Err NetLibWake (UInt16 libRefNum)

	CALLED_SETUP ("Err", "UInt16 libRefNum");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibAddrINToA
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibAddrINToA (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibAddrINToA");

	//	Char * NetLibAddrINToA(UInt16 libRefNum, NetIPAddr inet, Char *spaceP)

	CALLED_SETUP ("Char*", "UInt16 libRefNum, NetIPAddr inet, Char *spaceP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibAddrINToA (void)
{
	//	Char * NetLibAddrINToA(UInt16 libRefNum, NetIPAddr inet, Char *spaceP)

	CALLED_SETUP ("Char*", "UInt16 libRefNum, NetIPAddr inet, Char *spaceP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_PTR ();

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibAddrAToIN
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibAddrAToIN (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibAddrAToIN");

	//	NetIPAddr NetLibAddrAToIN(UInt16 libRefNum, Char *a)

	CALLED_SETUP ("NetIPAddr", "UInt16 libRefNum, Char *a");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibAddrAToIN (void)
{
	//	NetIPAddr NetLibAddrAToIN(UInt16 libRefNum, Char *a)

	CALLED_SETUP ("NetIPAddr", "UInt16 libRefNum, Char *a");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (NetIPAddr);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketOpen
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketOpen (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketOpen");

	//	NetSocketRef	NetLibSocketOpen(UInt16 libRefNum, NetSocketAddrEnum domain, 
	//						NetSocketTypeEnum type, Int16 protocol, Int32 timeout, 
	//						Err *errP)

	CALLED_SETUP ("NetSocketRef", "UInt16 libRefNum, NetSocketAddrEnum domain, "
							"NetSocketTypeEnum type, Int16 protocol, Int32 timeout, "
							"Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketAddrEnum, domain);
	CALLED_GET_PARAM_VAL (NetSocketTypeEnum, type);
	CALLED_GET_PARAM_VAL (Int16, protocol);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tdomain = 0x%08X", (long) domain);
	PRINTF ("\ttype = 0x%08X", (long) type);
	PRINTF ("\tprotocol = 0x%08X", (long) protocol);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		NetSocketRef	result = Platform_NetLib::SocketOpen (libRefNum,
			(NetSocketAddrEnum) (UInt8) domain, (NetSocketTypeEnum) (UInt8) type,
			protocol, timeout, errP);
		PUT_RESULT_VAL (NetSocketRef, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketOpen (void)
{
	CALLED_SETUP ("NetSocketRef", "UInt16 libRefNum, NetSocketAddrEnum domain, "
							"NetSocketTypeEnum type, Int16 protocol, Int32 timeout, "
							"Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketAddrEnum, domain);
//	CALLED_GET_PARAM_VAL (NetSocketTypeEnum, type);
//	CALLED_GET_PARAM_VAL (Int16, protocol);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (NetSocketRef);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketClose
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketClose (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketClose");

	//	Int16 NetLibSocketClose(UInt16 libRefNum, NetSocketRef socket, Int32 timeout, 
	//						Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketClose (libRefNum,
			socket, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketClose (void)
{
	//	Int16 NetLibSocketClose(UInt16 libRefNum, NetSocketRef socket, Int32 timeout, 
	//						Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketOptionSet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketOptionSet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketOptionSet");

	//	Int16 NetLibSocketOptionSet(UInt16 libRefNum, NetSocketRef socket,
	//						UInt16 /*NetSocketOptLevelEnum*/ level, UInt16 /*NetSocketOptEnum*/ option, 
	//						void *optValueP, UInt16 optValueLen,
	//						Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"UInt16 level, UInt16 option, "
					"void *optValueP, UInt16 optValueLen,"
					"Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt16, level);
	CALLED_GET_PARAM_VAL (UInt16, option);
	CALLED_GET_PARAM_VAL (UInt16, optValueLen);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	CALLED_GET_PARAM_PTR (void, optValueP, optValueLen, Marshal::kInput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\tlevel = %s (0x%08X)", PrvGetOptLevelString ((NetSocketOptLevelEnum) (UInt16) level), (long) (UInt16) level);
	PRINTF ("\toption = %s (0x%08X)", PrvGetOptString ((NetSocketOptLevelEnum) (UInt16) level, (NetSocketOptEnum) (UInt16) option), (long) (UInt16) option);
	PRINTF ("\toptValueP = 0x%08X", (long) (void*) optValueP);
	PRINTF ("\toptValueLen = 0x%08X", (long) optValueLen);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketOptionSet (libRefNum,
			socket,	(NetSocketOptLevelEnum) (UInt16) level,
			(NetSocketOptEnum) (UInt16) option, optValueP,
			optValueLen, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (optValueP);
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketOptionSet (void)
{
	//	Int16 NetLibSocketOptionSet(UInt16 libRefNum, NetSocketRef socket,
	//						UInt16 /*NetSocketOptLevelEnum*/ level, UInt16 /*NetSocketOptEnum*/ option, 
	//						void *optValueP, UInt16 optValueLen,
	//						Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"UInt16 level, UInt16 option, "
					"void *optValueP, UInt16 optValueLen,"
					"Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_VAL (UInt16, level);
//	CALLED_GET_PARAM_VAL (UInt16, option);
	CALLED_GET_PARAM_VAL (UInt16, optValueLen);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	CALLED_GET_PARAM_PTR (void, optValueP, optValueLen, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketOptionGet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketOptionGet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketOptionGet");

	//	Int16 NetLibSocketOptionGet(UInt16 libRefNum, NetSocketRef socket,
	//						UInt16 /*NetSocketOptLevelEnum*/ level, UInt16 /*NetSocketOptEnum*/ option,
	//						void *optValueP, UInt16 *optValueLenP,
	//						Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"UInt16 level, UInt16 option,"
					"void *optValueP, UInt16 *optValueLenP,"
					"Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt16, level);
	CALLED_GET_PARAM_VAL (UInt16, option);
	CALLED_GET_PARAM_REF (UInt16, optValueLenP, Marshal::kInOut);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	CALLED_GET_PARAM_PTR (void, optValueP, *optValueLenP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\tlevel = 0x%08X", (long) level);
	PRINTF ("\toption = 0x%08X", (long) option);
	PRINTF ("\toptValueP = 0x%08X", (long) (void*) optValueP);
	PRINTF ("\toptValueLenP = 0x%08X", (long) *optValueLenP);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketOptionGet (libRefNum,
			socket, (NetSocketOptLevelEnum) (UInt16) level,
			(NetSocketOptEnum) (UInt16) option, optValueP,
			optValueLenP, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (optValueP);
		CALLED_PUT_PARAM_REF (optValueLenP);
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketOptionGet (void)
{
	//	Int16 NetLibSocketOptionGet(UInt16 libRefNum, NetSocketRef socket,
	//						UInt16 /*NetSocketOptLevelEnum*/ level, UInt16 /*NetSocketOptEnum*/ option,
	//						void *optValueP, UInt16 *optValueLenP,
	//						Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"UInt16 level, UInt16 option,"
					"void *optValueP, UInt16 *optValueLenP,"
					"Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_VAL (UInt16, level);
//	CALLED_GET_PARAM_VAL (UInt16, option);
	CALLED_GET_PARAM_REF (UInt16, optValueLenP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	CALLED_GET_PARAM_PTR (void, optValueP, *optValueLenP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
	PRINTF ("\t*optValueLenP = 0x%08X", (long) *optValueLenP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketBind
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketBind (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketBind");

	//	Int16 NetLibSocketBind(UInt16 libRefNum, NetSocketRef socket,
	//						NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, 
	//						Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, "
					"Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_REF (NetSocketAddrType, sockAddrP, Marshal::kInput);
	CALLED_GET_PARAM_VAL (Int16, addrLen);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\tsockAddr.family = 0x%04X", (uint16) (*sockAddrP).family);
	PRINTF ("\tsockAddr.port = %s", PrvGetPortString (*sockAddrP));
	PRINTF ("\tsockAddr.address = %s", PrvGetDottedIPString (*sockAddrP));
	PRINTF ("\taddrLen = 0x%08X", (long) addrLen);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketBind (libRefNum, socket,
			sockAddrP, addrLen, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		if (result == 0)
		{
			CALLED_PUT_PARAM_REF (sockAddrP);
		}
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketBind (void)
{
	//	Int16 NetLibSocketBind(UInt16 libRefNum, NetSocketRef socket,
	//						NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, 
	//						Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, "
					"Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_REF (NetSocketAddrType, sockAddrP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int16, addrLen);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketConnect
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketConnect (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketConnect");

	//	Int16 NetLibSocketConnect(UInt16 libRefNum, NetSocketRef socket,
	//						NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, 
	//						Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, "
					"Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_REF (NetSocketAddrType, sockAddrP, Marshal::kInput);
	CALLED_GET_PARAM_VAL (Int16, addrLen);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\tsockAddr.family = 0x%04X", (uint16) (*sockAddrP).family);
	PRINTF ("\tsockAddr.port = %s", PrvGetPortString (*sockAddrP));
	PRINTF ("\tsockAddr.address = %s", PrvGetDottedIPString (*sockAddrP));
	PRINTF ("\taddrLen = 0x%08X", (long) addrLen);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketConnect (libRefNum,
			socket, sockAddrP, addrLen, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		if (result == 0)
		{
			CALLED_PUT_PARAM_REF (sockAddrP);
		}
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketConnect (void)
{
	//	Int16 NetLibSocketConnect(UInt16 libRefNum, NetSocketRef socket,
	//						NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, 
	//						Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetSocketAddrType *sockAddrP, Int16 addrLen, Int32 timeout, "
					"Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_REF (NetSocketAddrType, sockAddrP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int16, addrLen);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketListen
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketListen (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketListen");

	//	Int16 NetLibSocketListen(UInt16 libRefNum, NetSocketRef socket,
	//						UInt16	queueLen, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"UInt16	queueLen, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt16, queueLen);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\tqueueLen = 0x%08X", (long) queueLen);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketListen (libRefNum,
			socket, queueLen, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketListen (void)
{
	//	Int16 NetLibSocketListen(UInt16 libRefNum, NetSocketRef socket,
	//						UInt16	queueLen, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"UInt16	queueLen, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_VAL (UInt16, queueLen);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketAccept
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketAccept (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketAccept");

	//	Int16 NetLibSocketAccept(UInt16 libRefNum, NetSocketRef socket,
	//						NetSocketAddrType *sockAddrP, Int16 *addrLenP, Int32 timeout,
	//						Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetSocketAddrType *sockAddrP, Int16 *addrLenP, Int32 timeout,"
					"Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_REF (NetSocketAddrType, sockAddrP, Marshal::kOutput);
	CALLED_GET_PARAM_REF (Int16, addrLenP, Marshal::kInOut);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\taddrLen = 0x%08X", (long) *addrLenP);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketAccept (libRefNum,
			socket, sockAddrP, addrLenP, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		if (result >= 0)
		{
			CALLED_PUT_PARAM_REF (sockAddrP);
			CALLED_PUT_PARAM_REF (addrLenP);
		}
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketAccept (void)
{
	//	Int16 NetLibSocketAccept(UInt16 libRefNum, NetSocketRef socket,
	//						NetSocketAddrType *sockAddrP, Int16 *addrLenP, Int32 timeout,
	//						Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetSocketAddrType *sockAddrP, Int16 *addrLenP, Int32 timeout,"
					"Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_REF (NetSocketAddrType, sockAddrP, Marshal::kInput);
	CALLED_GET_PARAM_REF (Int16, addrLenP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
	PRINTF ("\tsockAddr.family = 0x%04X", (uint16) (*sockAddrP).family);
	PRINTF ("\tsockAddr.port = %s", PrvGetPortString (*sockAddrP));
	PRINTF ("\tsockAddr.address = %s", PrvGetDottedIPString (*sockAddrP));
	PRINTF ("\taddrLen = 0x%08X", (long) *addrLenP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketShutdown
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketShutdown (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketShutdown");

	//	Int16 NetLibSocketShutdown(UInt16 libRefNum, NetSocketRef socket, 
	//						Int16 /*NetSocketDirEnum*/ direction, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket, "
					"Int16 direction, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (Int16, direction);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\tdirection = 0x%08X", (long) direction);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketShutdown (libRefNum,
			socket, direction, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketShutdown (void)
{
	//	Int16 NetLibSocketShutdown(UInt16 libRefNum, NetSocketRef socket, 
	//						Int16 /*NetSocketDirEnum*/ direction, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket, "
					"Int16 direction, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_VAL (Int16, direction);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSendPB
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSendPB (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSendPB");

	// Int16 NetLibSendPB(UInt16 libRefNum, NetSocketRef socket,
	//						NetIOParamType *pbP, UInt16 flags, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetIOParamType *pbP, UInt16 flags, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_REF (NetIOParamType, pbP, Marshal::kInput);
	CALLED_GET_PARAM_VAL (UInt16, flags);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\tflags = 0x%08X", (long) flags);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SendPB (libRefNum,
			socket, pbP, flags, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (pbP);
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSendPB (void)
{
	// Int16 NetLibSendPB(UInt16 libRefNum, NetSocketRef socket,
	//						NetIOParamType *pbP, UInt16 flags, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetIOParamType *pbP, UInt16 flags, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_REF (NetIOParamType, pbP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (UInt16, flags);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSend
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSend (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSend");

	//	Int16 NetLibSend(UInt16 libRefNum, NetSocketRef socket,
	//						void *bufP, UInt16 bufLen, UInt16 flags,
	//						void *toAddrP, UInt16 toLen, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"void *bufP, UInt16 bufLen, UInt16 flags,"
					"void *toAddrP, UInt16 toLen, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt16, bufLen);
	CALLED_GET_PARAM_VAL (UInt16, flags);
	CALLED_GET_PARAM_REF (NetSocketAddrType, toAddrP, Marshal::kInput);
	CALLED_GET_PARAM_VAL (UInt16, toLen);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	CALLED_GET_PARAM_PTR (void, bufP, bufLen, Marshal::kInput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
//	PRINTF ("\tbufP = 0x%08X", (long) bufP);
	PRINTF ("\tbufLen = 0x%08X", (long) bufLen);
	PRINTF ("\tflags = 0x%08X", (long) flags);
//	PRINTF ("\ttoAddrP = 0x%08X", (long) toAddrP);
	PRINTF ("\ttoLen = 0x%08X", (long) toLen);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::Send (libRefNum, socket,
			bufP, bufLen, flags, toAddrP, toLen, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (bufP);
		CALLED_PUT_PARAM_REF (toAddrP);
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSend (void)
{
	//	Int16 NetLibSend(UInt16 libRefNum, NetSocketRef socket,
	//						void *bufP, UInt16 bufLen, UInt16 flags,
	//						void *toAddrP, UInt16 toLen, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"void *bufP, UInt16 bufLen, UInt16 flags,"
					"void *toAddrP, UInt16 toLen, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt16, bufLen);
//	CALLED_GET_PARAM_VAL (UInt16, flags);
//	CALLED_GET_PARAM_REF (NetSocketAddrType, toAddrP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (UInt16, toLen);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	CALLED_GET_PARAM_PTR (void, bufP, bufLen, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibReceivePB
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibReceivePB (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibReceivePB");

	//	Int16 NetLibReceivePB(UInt16 libRefNum, NetSocketRef socket,
	//						NetIOParamType *pbP, UInt16 flags, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetIOParamType *pbP, UInt16 flags, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_REF (NetIOParamType, pbP, Marshal::kInOut);
	CALLED_GET_PARAM_VAL (UInt16, flags);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
//	PRINTF ("\tpbP = 0x%08X", (long) pbP);
	PRINTF ("\tflags = 0x%08X", (long) flags);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::ReceivePB (libRefNum,
			socket, pbP, flags, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (pbP);
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibReceivePB (void)
{
	//	Int16 NetLibReceivePB(UInt16 libRefNum, NetSocketRef socket,
	//						NetIOParamType *pbP, UInt16 flags, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetIOParamType *pbP, UInt16 flags, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
//	CALLED_GET_PARAM_REF (NetIOParamType, pbP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (UInt16, flags);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibReceive
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibReceive (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibReceive");

	//	Int16 NetLibReceive(UInt16 libRefNum, NetSocketRef socket,
	//						void *bufP, UInt16 bufLen, UInt16 flags, 
	//						void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"void *bufP, UInt16 bufLen, UInt16 flags, "
					"void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt16, bufLen);
	CALLED_GET_PARAM_VAL (UInt16, flags);
	CALLED_GET_PARAM_REF (NetSocketAddrType, fromAddrP, Marshal::kOutput);
	CALLED_GET_PARAM_REF (UInt16, fromLenP, Marshal::kInOut);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	CALLED_GET_PARAM_PTR (void, bufP, bufLen, Marshal::kInOut);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
//	PRINTF ("\tbufP = 0x%08X", (long) bufP);
	PRINTF ("\tbufLen = 0x%08X", (long) bufLen);
	PRINTF ("\tflags = 0x%08X", (long) flags);
//	PRINTF ("\tfromAddrP = 0x%08X", (long) fromAddrP);
	PRINTF ("\tfromLen = 0x%08X", (long) *fromLenP);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::Receive (libRefNum,
			socket, bufP, bufLen, flags, fromAddrP, fromLenP, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (bufP);
		CALLED_PUT_PARAM_REF (fromAddrP);
		CALLED_PUT_PARAM_REF (fromLenP);
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibReceive (void)
{
	//	Int16 NetLibReceive(UInt16 libRefNum, NetSocketRef socket,
	//						void *bufP, UInt16 bufLen, UInt16 flags, 
	//						void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"void *bufP, UInt16 bufLen, UInt16 flags, "
					"void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt16, bufLen);
//	CALLED_GET_PARAM_VAL (UInt16, flags);
//	CALLED_GET_PARAM_REF (NetSocketAddrType, fromAddrP, Marshal::kInput);
//	CALLED_GET_PARAM_REF (UInt16, fromLenP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	CALLED_GET_PARAM_PTR (void, bufP, bufLen, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);

	if (LogNetLibData () && (((long) *errP) == 0) && (result > 0))
	{
		LogAppendData (bufP, result, "Received Data");
	}
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibDmReceive
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibDmReceive (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibDmReceive");

	//	Int16 NetLibDmReceive(UInt16 libRefNum, NetSocketRef socket,
	//						void *recordP, UInt32 recordOffset, UInt16 rcvLen, UInt16 flags, 
	//						void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"void *recordP, UInt32 recordOffset, UInt16 rcvLen, UInt16 flags, "
					"void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt32, recordOffset);
	CALLED_GET_PARAM_VAL (UInt16, rcvLen);
	CALLED_GET_PARAM_VAL (UInt16, flags);
	CALLED_GET_PARAM_REF (NetSocketAddrType, fromAddrP, Marshal::kOutput);
	CALLED_GET_PARAM_REF (UInt16, fromLenP, Marshal::kInOut);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	CALLED_GET_PARAM_PTR (void, recordP, rcvLen + recordOffset, Marshal::kInOut);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		CEnableFullAccess	munge;	// Remove blocks on memory access.

		// Call the host function.
		Int16	result = Platform_NetLib::DmReceive (libRefNum,
			socket, recordP, recordOffset, rcvLen, flags,
			fromAddrP, fromLenP, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (recordP);
		CALLED_PUT_PARAM_REF (fromAddrP);
		CALLED_PUT_PARAM_REF (fromLenP);
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibDmReceive (void)
{
	//	Int16 NetLibDmReceive(UInt16 libRefNum, NetSocketRef socket,
	//						void *recordP, UInt32 recordOffset, UInt16 rcvLen, UInt16 flags, 
	//						void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"void *recordP, UInt32 recordOffset, UInt16 rcvLen, UInt16 flags, "
					"void *fromAddrP, UInt16 *fromLenP, Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_VAL (UInt32, recordOffset);
	CALLED_GET_PARAM_VAL (UInt16, rcvLen);
//	CALLED_GET_PARAM_VAL (UInt16, flags);
//	CALLED_GET_PARAM_REF (NetSocketAddrType, fromAddrP, Marshal::kInput);
//	CALLED_GET_PARAM_REF (UInt16, fromLenP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	CALLED_GET_PARAM_PTR (void, recordP, recordOffset + rcvLen, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSelect
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSelect (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSelect");

	//	Int16 NetLibSelect(UInt16 libRefNum, UInt16 width, NetFDSetType *readFDs, 
	//						NetFDSetType *writeFDs, NetFDSetType *exceptFDs,
	//						Int32	timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, UInt16 width, NetFDSetType *readFDs, "
					"NetFDSetType *writeFDs, NetFDSetType *exceptFDs,"
					"Int32	timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (UInt16, width);
	CALLED_GET_PARAM_REF (NetFDSetType, readFDs, Marshal::kInOut);
	CALLED_GET_PARAM_REF (NetFDSetType, writeFDs, Marshal::kInOut);
	CALLED_GET_PARAM_REF (NetFDSetType, exceptFDs, Marshal::kInOut);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\twidth = 0x%08X", (long) width);
	PRINTF ("\treadFDs = 0x%08X", (long) *readFDs);
	PRINTF ("\twriteFDs = 0x%08X", (long) *writeFDs);
	PRINTF ("\texceptFDs = 0x%08X", (long) *exceptFDs);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::Select (libRefNum,
			width, readFDs, writeFDs, exceptFDs, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (readFDs);
		CALLED_PUT_PARAM_REF (writeFDs);
		CALLED_PUT_PARAM_REF (exceptFDs);
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSelect (void)
{
	//	Int16 NetLibSelect(UInt16 libRefNum, UInt16 width, NetFDSetType *readFDs, 
	//						NetFDSetType *writeFDs, NetFDSetType *exceptFDs,
	//						Int32	timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, UInt16 width, NetFDSetType *readFDs, "
					"NetFDSetType *writeFDs, NetFDSetType *exceptFDs,"
					"Int32	timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (UInt16, width);
	CALLED_GET_PARAM_REF (NetFDSetType, readFDs, Marshal::kInput);
	CALLED_GET_PARAM_REF (NetFDSetType, writeFDs, Marshal::kInput);
	CALLED_GET_PARAM_REF (NetFDSetType, exceptFDs, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
	PRINTF ("\treadFDs = 0x%08X", (long) *readFDs);
	PRINTF ("\twriteFDs = 0x%08X", (long) *writeFDs);
	PRINTF ("\texceptFDs = 0x%08X", (long) *exceptFDs);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibMaster
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibMaster (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibMaster");

	//	Err NetLibMaster(UInt16 libRefNum, UInt16 cmd, NetMasterPBPtr pbP,
	//					Int32 timeout)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 cmd, NetMasterPBPtr pbP,"
					"Int32 timeout");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibMaster (void)
{
	//	Err NetLibMaster(UInt16 libRefNum, UInt16 cmd, NetMasterPBPtr pbP,
	//					Int32 timeout)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 cmd, NetMasterPBPtr pbP,"
					"Int32 timeout");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibGetHostByName
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibGetHostByName (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibGetHostByName");

	//	NetHostInfoPtr NetLibGetHostByName(UInt16 libRefNum, Char *nameP, 
	//						NetHostInfoBufPtr bufP, Int32	timeout, Err *errP)

	CALLED_SETUP ("NetHostInfoPtr", "UInt16 libRefNum, Char *nameP, "
							"NetHostInfoBufPtr bufP, Int32	timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_STR (Char, nameP);
	CALLED_GET_PARAM_REF (NetHostInfoBufType, bufP, Marshal::kOutput);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tnameP = %s", (Char*) nameP);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		NetHostInfoPtr	p = Platform_NetLib::GetHostByName (libRefNum,
			nameP, bufP, timeout, errP);
		emuptr	result = p ? (emuptr) bufP + offsetof (NetHostInfoBufType, hostInfo) : EmMemNULL;
		PUT_RESULT_VAL (emuptr, result);

		// Return any pass-by-reference values.
			// bufP is a complex type with internal pointers and such.  We
			// *can't* copy the contents back into emulated memory if they
			// are uninitialized.  Check for errors before attempting that.
		if (result != EmMemNULL)
		{
			CALLED_PUT_PARAM_REF (nameP);
			CALLED_PUT_PARAM_REF (bufP);
		}
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibGetHostByName (void)
{
	//	NetHostInfoPtr NetLibGetHostByName(UInt16 libRefNum, Char *nameP, 
	//						NetHostInfoBufPtr bufP, Int32	timeout, Err *errP)

	CALLED_SETUP ("NetHostInfoPtr", "UInt16 libRefNum, Char *nameP, "
							"NetHostInfoBufPtr bufP, Int32	timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_STR (Char, nameP);
//	CALLED_GET_PARAM_REF (NetHostInfoBufType, bufP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_PTR ();

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);

	NetHostInfoBufType	hostInfo;
	Marshal::GetNetHostInfoBufType (result, hostInfo);
//	PRINTF ("\t\tname = %s", hostInfo.name);
//	PRINTF ("\t\taddress = 0x%08X", hostInfo.address[0]);

	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSettingGet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSettingGet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSettingGet");

	//	Err NetLibSettingGet(UInt16 libRefNum,
	//						UInt16 /*NetSettingEnum*/ setting, void *valueP, UInt16 *valueLenP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 setting, void *valueP, UInt16 *valueLenP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSettingGet (void)
{
	//	Err NetLibSettingGet(UInt16 libRefNum,
	//						UInt16 /*NetSettingEnum*/ setting, void *valueP, UInt16 *valueLenP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 setting, void *valueP, UInt16 *valueLenP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSettingSet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSettingSet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSettingSet");

	//	Err NetLibSettingSet(UInt16 libRefNum, 
	//						UInt16 /*NetSettingEnum*/ setting, void *valueP, UInt16 valueLen)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 setting, void *valueP, UInt16 valueLen");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSettingSet (void)
{
	//	Err NetLibSettingSet(UInt16 libRefNum, 
	//						UInt16 /*NetSettingEnum*/ setting, void *valueP, UInt16 valueLen)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 setting, void *valueP, UInt16 valueLen");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibIFAttach
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibIFAttach (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibIFAttach");

	//	Err NetLibIFAttach(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						Int32 timeout)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance, Int32 timeout");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibIFAttach (void)
{
	//	Err NetLibIFAttach(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						Int32 timeout)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance, Int32 timeout");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibIFDetach
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibIFDetach (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibIFDetach");

	//	Err NetLibIFDetach(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						Int32 timeout)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,"
					"Int32 timeout");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibIFDetach (void)
{
	//	Err NetLibIFDetach(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						Int32 timeout)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,"
					"Int32 timeout");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibIFGet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibIFGet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibIFGet");

	//	Err NetLibIFGet(UInt16 libRefNum, UInt16 index, UInt32 *ifCreatorP, 
	//							UInt16 *ifInstanceP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 index, UInt32 *ifCreatorP, "
					"UInt16 *ifInstanceP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibIFGet (void)
{
	//	Err NetLibIFGet(UInt16 libRefNum, UInt16 index, UInt32 *ifCreatorP, 
	//							UInt16 *ifInstanceP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 index, UInt32 *ifCreatorP, "
					"UInt16 *ifInstanceP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibIFSettingGet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibIFSettingGet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibIFSettingGet");

	//	Err NetLibIFSettingGet(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						UInt16 /*NetIFSettingEnum*/ setting, void *valueP, UInt16 *valueLenP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,"
					"UInt16 setting, void *valueP, UInt16 *valueLenP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibIFSettingGet (void)
{
	//	Err NetLibIFSettingGet(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						UInt16 /*NetIFSettingEnum*/ setting, void *valueP, UInt16 *valueLenP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,"
					"UInt16 setting, void *valueP, UInt16 *valueLenP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibIFSettingSet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibIFSettingSet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibIFSettingSet");

	//	Err NetLibIFSettingSet(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						UInt16 /*NetIFSettingEnum*/ setting, void *valueP, UInt16 valueLen)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,"
					"UInt16 setting, void *valueP, UInt16 valueLen");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibIFSettingSet (void)
{
	//	Err NetLibIFSettingSet(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						UInt16 /*NetIFSettingEnum*/ setting, void *valueP, UInt16 valueLen)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,"
					"UInt16 setting, void *valueP, UInt16 valueLen");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibIFUp
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibIFUp (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibIFUp");

	//	Err NetLibIFUp(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibIFUp (void)
{
	//	Err NetLibIFUp(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibIFDown
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibIFDown (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibIFDown");

	//	Err NetLibIFDown(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						Int32 timeout)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance, Int32 timeout");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibIFDown (void)
{
	//	Err NetLibIFDown(UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance,
	//						Int32 timeout)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt32 ifCreator, UInt16 ifInstance, Int32 timeout");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibGetHostByAddr
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibGetHostByAddr (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibGetHostByAddr");

	//	NetHostInfoPtr NetLibGetHostByAddr(UInt16 libRefNum, UInt8 *addrP, UInt16 len, UInt16 type,
	//						NetHostInfoBufPtr bufP, Int32	timeout, Err *errP)

	CALLED_SETUP ("NetHostInfoPtr", "UInt16 libRefNum, UInt8 *addrP, UInt16 len, UInt16 type,"
							"NetHostInfoBufPtr bufP, Int32	timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (UInt16, len);
	CALLED_GET_PARAM_PTR (UInt8, addrP, len, Marshal::kInput);
	CALLED_GET_PARAM_VAL (UInt16, type);
	CALLED_GET_PARAM_REF (NetHostInfoBufType, bufP, Marshal::kOutput);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		NetHostInfoPtr	p	= Platform_NetLib::GetHostByAddr (libRefNum,
			(UInt8*) addrP, len, type, bufP, timeout, errP);
		emuptr	result = p ? (emuptr) bufP + offsetof (NetHostInfoBufType, hostInfo) : EmMemNULL;
		PUT_RESULT_VAL (emuptr, result);

		// Return any pass-by-reference values.
			// bufP is a complex type with internal pointers and such.  We
			// *can't* copy the contents back into emulated memory if they
			// are uninitialized.  Check for errors before attempting that.
		if (result != EmMemNULL)
		{
			CALLED_PUT_PARAM_REF (addrP);
			CALLED_PUT_PARAM_REF (bufP);
		}
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibGetHostByAddr (void)
{
	//	NetHostInfoPtr NetLibGetHostByAddr(UInt16 libRefNum, UInt8 *addrP, UInt16 len, UInt16 type,
	//						NetHostInfoBufPtr bufP, Int32	timeout, Err *errP)

	CALLED_SETUP ("NetHostInfoPtr", "UInt16 libRefNum, UInt8 *addrP, UInt16 len, UInt16 type,"
							"NetHostInfoBufPtr bufP, Int32	timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_REF (NetSocketAddrType, addrP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (UInt16, len);
//	CALLED_GET_PARAM_VAL (UInt16, type);
//	CALLED_GET_PARAM_REF (NetHostInfoBufType, bufP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_PTR ();

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibGetServByName
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibGetServByName (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibGetServByName");

	//	NetServInfoPtr	NetLibGetServByName(UInt16 libRefNum, const Char *servNameP, 
	//						const Char *protoNameP,  NetServInfoBufPtr bufP, 
	//						Int32	timeout, Err *errP)

	CALLED_SETUP ("NetServInfoPtr", "UInt16 libRefNum, const Char *servNameP, "
							"const Char *protoNameP,  NetServInfoBufPtr bufP, "
							"Int32	timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_STR (Char, servNameP);
	CALLED_GET_PARAM_STR (Char, protoNameP);
	CALLED_GET_PARAM_REF (NetServInfoBufType, bufP, Marshal::kOutput);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tservNameP = %s", (Char*) servNameP);
	PRINTF ("\tprotoNameP = %s", (Char*) protoNameP);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		NetServInfoPtr	p	= Platform_NetLib::GetServByName (libRefNum,
			servNameP, protoNameP, bufP, timeout, errP);
		emuptr	result = p ? (emuptr) bufP + offsetof (NetServInfoBufType, servInfo) : EmMemNULL;
		PUT_RESULT_VAL (emuptr, result);

		// Return any pass-by-reference values.
			// bufP is a complex type with internal pointers and such.  We
			// *can't* copy the contents back into emulated memory if they
			// are uninitialized.  Check for errors before attempting that.
		if (result != EmMemNULL)
		{
			CALLED_PUT_PARAM_REF (servNameP);
			CALLED_PUT_PARAM_REF (protoNameP);
			CALLED_PUT_PARAM_REF (bufP);
		}
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibGetServByName (void)
{
	//	NetServInfoPtr	NetLibGetServByName(UInt16 libRefNum, const Char *servNameP, 
	//						const Char *protoNameP,  NetServInfoBufPtr bufP, 
	//						Int32	timeout, Err *errP)

	CALLED_SETUP ("NetServInfoPtr", "UInt16 libRefNum, const Char *servNameP, "
							"const Char *protoNameP,  NetServInfoBufPtr bufP, "
							"Int32	timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_STR (Char, servNameP);
	CALLED_GET_PARAM_STR (Char, protoNameP);
//	CALLED_GET_PARAM_REF (NetServInfoBufType, bufP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_PTR ();

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);

	NetServInfoBufType	servInfo;
	Marshal::GetNetServInfoBufType (result, servInfo);
//	PRINTF ("\t\tname = %s", servInfo.name);
//	PRINTF ("\t\tport = 0x%04X", (UInt16) servInfo.servInfo.port);
//	PRINTF ("\t\tprotoName = %s", servInfo.protoName);

	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibSocketAddr
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibSocketAddr (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibSocketAddr");

	//	Int16 NetLibSocketAddr(UInt16 libRefNum, NetSocketRef socket,
	//						NetSocketAddrType *locAddrP, Int16 *locAddrLenP, 
	//						NetSocketAddrType *remAddrP, Int16 *remAddrLenP, 
	//						Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetSocketAddrType *locAddrP, Int16 *locAddrLenP, "
					"NetSocketAddrType *remAddrP, Int16 *remAddrLenP, "
					"Int32 timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_REF (NetSocketAddrType, locAddrP, Marshal::kOutput);
	CALLED_GET_PARAM_REF (Int16, locAddrLenP, Marshal::kInOut);
	CALLED_GET_PARAM_REF (NetSocketAddrType, remAddrP, Marshal::kOutput);
	CALLED_GET_PARAM_REF (Int16, remAddrLenP, Marshal::kInOut);
	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tsocket = 0x%08X", (long) socket);
	PRINTF ("\tlocAddrLen = 0x%08X", (long) *locAddrLenP);
	PRINTF ("\tremAddrLen = 0x%08X", (long) *remAddrLenP);
	PRINTF ("\ttimeout = 0x%08X", (long) timeout);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Int16	result = Platform_NetLib::SocketAddr (libRefNum,
			socket, locAddrP, locAddrLenP, remAddrP, remAddrLenP, timeout, errP);
		PUT_RESULT_VAL (Int16, result);

		// Return any pass-by-reference values.
		if (result == 0)
		{
			CALLED_PUT_PARAM_REF (locAddrP);
			CALLED_PUT_PARAM_REF (locAddrLenP);
			CALLED_PUT_PARAM_REF (remAddrP);
			CALLED_PUT_PARAM_REF (remAddrLenP);
		}
		CALLED_PUT_PARAM_REF (errP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibSocketAddr (void)
{
	//	Int16 NetLibSocketAddr(UInt16 libRefNum, NetSocketRef socket,
	//						NetSocketAddrType *locAddrP, Int16 *locAddrLenP, 
	//						NetSocketAddrType *remAddrP, Int16 *remAddrLenP, 
	//						Int32 timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, NetSocketRef socket,"
					"NetSocketAddrType *locAddrP, Int16 *locAddrLenP, "
					"NetSocketAddrType *remAddrP, Int16 *remAddrLenP, "
					"Int32 timeout, Err *errP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (NetSocketRef, socket);
	CALLED_GET_PARAM_REF (NetSocketAddrType, locAddrP, Marshal::kInput);
	CALLED_GET_PARAM_REF (Int16, locAddrLenP, Marshal::kInput);
	CALLED_GET_PARAM_REF (NetSocketAddrType, remAddrP, Marshal::kInput);
	CALLED_GET_PARAM_REF (Int16, remAddrLenP, Marshal::kInput);
//	CALLED_GET_PARAM_VAL (Int32, timeout);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
	PRINTF ("\tlocAddr.family = 0x%04X", (uint16) (*locAddrP).family);
	PRINTF ("\tlocAddr.port = %s", PrvGetPortString (*locAddrP));
	PRINTF ("\tlocAddr.address = %s", PrvGetDottedIPString (*locAddrP));
	PRINTF ("\tlocAddrLen = 0x%08X", (long) *locAddrLenP);
	PRINTF ("\tremAddr.family = 0x%04X", (uint16) (*remAddrP).family);
	PRINTF ("\tremAddr.port = %s", PrvGetPortString (*remAddrP));
	PRINTF ("\tremAddr.address = %s", PrvGetDottedIPString (*remAddrP));
	PRINTF ("\tremAddrLen = 0x%08X", (long) *remAddrLenP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibFinishCloseWait
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibFinishCloseWait (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibFinishCloseWait");

	//	Err NetLibFinishCloseWait(UInt16 libRefNum)

	CALLED_SETUP ("Err", "UInt16 libRefNum");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::FinishCloseWait (libRefNum);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibFinishCloseWait (void)
{
	//	Err NetLibFinishCloseWait(UInt16 libRefNum)

	CALLED_SETUP ("Err", "UInt16 libRefNum");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibGetMailExchangeByName
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibGetMailExchangeByName (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibGetMailExchangeByName");

	//	Int16 NetLibGetMailExchangeByName(UInt16 libRefNum, Char *mailNameP, 
	//						UInt16 maxEntries, 
	//						Char hostNames[][netDNSMaxDomainName+1], UInt16 priorities[], 
	//						Int32	timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, Char *mailNameP, "
					"UInt16 maxEntries, "
					"Char** hostNames, UInt16* priorities, "
					"Int32	timeout, Err *errP");

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibGetMailExchangeByName (void)
{
	//	Int16 NetLibGetMailExchangeByName(UInt16 libRefNum, Char *mailNameP, 
	//						UInt16 maxEntries, 
	//						Char hostNames[][netDNSMaxDomainName+1], UInt16 priorities[], 
	//						Int32	timeout, Err *errP)

	CALLED_SETUP ("Int16", "UInt16 libRefNum, Char *mailNameP, "
					"UInt16 maxEntries, "
					"Char** hostNames, UInt16* priorities, "
					"Int32	timeout, Err *errP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Int16);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*errP = %s (0x%04X)", PrvGetErrorString (*errP), (long) *errP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibOpenCount
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibOpenCount (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibOpenCount");

	//	Err NetLibOpenCount (UInt16 libRefNum, UInt16 *countP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 *countP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_REF (UInt16, countP, Marshal::kOutput);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::OpenCount (libRefNum, countP);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (countP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibOpenCount (void)
{
	//	Err NetLibOpenCount (UInt16 libRefNum, UInt16 *countP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 *countP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_REF (UInt16, countP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\tcount = 0x%08X", (long) *countP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibTracePrintF
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibTracePrintF (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibTracePrintF");

	//	Err NetLibTracePrintF(UInt16 libRefNum, Char *formatStr, ...)

	CALLED_SETUP ("Err", "UInt16 libRefNum, Char *formatStr");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibTracePrintF (void)
{
	//	Err NetLibTracePrintF(UInt16 libRefNum, Char *formatStr, ...)

	CALLED_SETUP ("Err", "UInt16 libRefNum, Char *formatStr");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibTracePutS
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibTracePutS (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibTracePutS");

	// Err NetLibTracePutS(UInt16 libRefNum, Char *strP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, Char *strP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibTracePutS (void)
{
	// Err NetLibTracePutS(UInt16 libRefNum, Char *strP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, Char *strP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibOpenIfCloseWait
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibOpenIfCloseWait (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibOpenIfCloseWait");

	//	Err NetLibOpenIfCloseWait(UInt16 libRefNum)

	CALLED_SETUP ("Err", "UInt16 libRefNum");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::OpenIfCloseWait (libRefNum);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibOpenIfCloseWait (void)
{
	//	Err NetLibOpenIfCloseWait(UInt16 libRefNum)

	CALLED_SETUP ("Err", "UInt16 libRefNum");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibHandlePowerOff
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibHandlePowerOff (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibHandlePowerOff");

	//	Err NetLibHandlePowerOff (UInt16 libRefNum, SysEventType *eventP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, SysEventType *eventP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_REF (EventType, eventP, Marshal::kInput);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::HandlePowerOff (libRefNum, eventP);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (eventP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibHandlePowerOff (void)
{
	//	Err NetLibHandlePowerOff (UInt16 libRefNum, SysEventType *eventP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, SysEventType *eventP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_REF (EventType, eventP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConnectionRefresh
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConnectionRefresh (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConnectionRefresh");

	//	Err NetLibConnectionRefresh(UInt16 libRefNum, Boolean refresh, 
	//						UInt8 *allInterfacesUpP, UInt16 *netIFErrP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, Boolean refresh, "
					"UInt8 *allInterfacesUpP, UInt16 *netIFErrP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (Boolean, refresh);
	CALLED_GET_PARAM_REF (Boolean, allInterfacesUpP, Marshal::kOutput);
	CALLED_GET_PARAM_REF (UInt16, netIFErrP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\trefresh = 0x%08X", (long) refresh);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::ConnectionRefresh (libRefNum,
			refresh, allInterfacesUpP, netIFErrP);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (allInterfacesUpP);
		CALLED_PUT_PARAM_REF (netIFErrP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConnectionRefresh (void)
{
	//	Err NetLibConnectionRefresh(UInt16 libRefNum, Boolean refresh, 
	//						UInt8 *allInterfacesUpP, UInt16 *netIFErrP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, Boolean refresh, "
					"UInt8 *allInterfacesUpP, UInt16 *netIFErrP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (Boolean, refresh);
	CALLED_GET_PARAM_REF (Boolean, allInterfacesUpP, Marshal::kInput);
	CALLED_GET_PARAM_REF (UInt16, netIFErrP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*netIFErrP = %s (0x%04X)", PrvGetErrorString (*netIFErrP), (long) *netIFErrP);
	PRINTF ("\t*allInterfacesUpP = 0x%08X", (long) *allInterfacesUpP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibOpenConfig
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibOpenConfig (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibOpenConfig");

	//	Err NetLibOpenConfig( UInt16 libRefNum, UInt16 configIndex, UInt32 openFlags,
	//						UInt16 *netIFErrP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 configIndex, UInt32 openFlags,"
					"UInt16 *netIFErrP");

	// Get the stack-based parameters.
	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (UInt16, configIndex);
	CALLED_GET_PARAM_VAL (UInt32, openFlags);
	CALLED_GET_PARAM_REF (UInt16, netIFErrP, Marshal::kOutput);

	// Examine the parameters
	PRINTF ("\tconfigIndex = 0x%08X", (long) configIndex);
	PRINTF ("\topenFlags = 0x%08X", (long) openFlags);

	if (Platform_NetLib::Redirecting ())
	{
		PRINTF ("\t-->Executing host version");

		// Call the host function.
		Err	result = Platform_NetLib::OpenConfig (libRefNum,
			configIndex, openFlags, netIFErrP);
		PUT_RESULT_VAL (Err, result);

		// Return any pass-by-reference values.
		CALLED_PUT_PARAM_REF (netIFErrP);

		return kSkipROM;
	}

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibOpenConfig (void)
{
	//	Err NetLibOpenConfig( UInt16 libRefNum, UInt16 configIndex, UInt32 openFlags,
	//						UInt16 *netIFErrP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 configIndex, UInt32 openFlags,"
					"UInt16 *netIFErrP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
//	CALLED_GET_PARAM_VAL (UInt16, configIndex);
//	CALLED_GET_PARAM_VAL (UInt16, openFlags);
	CALLED_GET_PARAM_REF (UInt16, netIFErrP, Marshal::kInput);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
	PRINTF ("\t*netIFErrP = %s (0x%04X)", PrvGetErrorString (*netIFErrP), (long) *netIFErrP);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConfigMakeActive
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConfigMakeActive (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConfigMakeActive");

	//	Err NetLibConfigMakeActive( UInt16 libRefNum, UInt16 configIndex)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 configIndex");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConfigMakeActive (void)
{
	//	Err NetLibConfigMakeActive( UInt16 libRefNum, UInt16 configIndex)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 configIndex");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConfigList
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConfigList (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConfigList");

	//	Err NetLibConfigList( UInt16 libRefNum, NetConfigNameType nameArray[],
	//						UInt16 *arrayEntriesP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, NetConfigNameType* nameArray,"
					"UInt16 *arrayEntriesP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConfigList (void)
{
	//	Err NetLibConfigList( UInt16 libRefNum, NetConfigNameType nameArray[],
	//						UInt16 *arrayEntriesP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, NetConfigNameType* nameArray,"
					"UInt16 *arrayEntriesP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConfigIndexFromName
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConfigIndexFromName (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConfigIndexFromName");

	//	Err NetLibConfigIndexFromName( UInt16 libRefNum, NetConfigNamePtr nameP,
	//						UInt16 *indexP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, NetConfigNamePtr nameP, UInt16 *indexP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConfigIndexFromName (void)
{
	//	Err NetLibConfigIndexFromName( UInt16 libRefNum, NetConfigNamePtr nameP,
	//						UInt16 *indexP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, NetConfigNamePtr nameP, UInt16 *indexP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConfigDelete
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConfigDelete (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConfigDelete");

	//	Err NetLibConfigDelete( UInt16 libRefNum, UInt16 index)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 index");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConfigDelete (void)
{
	//	Err NetLibConfigDelete( UInt16 libRefNum, UInt16 index)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 index");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConfigSaveAs
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConfigSaveAs (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConfigSaveAs");

	//	Err NetLibConfigSaveAs( UInt16 libRefNum, NetConfigNamePtr nameP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, NetConfigNamePtr nameP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConfigSaveAs (void)
{
	//	Err NetLibConfigSaveAs( UInt16 libRefNum, NetConfigNamePtr nameP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, NetConfigNamePtr nameP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConfigRename
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConfigRename (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConfigRename");

	//	Err NetLibConfigRename( UInt16 libRefNum, UInt16 index,
	//						NetConfigNamePtr newNameP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 index, NetConfigNamePtr newNameP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConfigRename (void)
{
	//	Err NetLibConfigRename( UInt16 libRefNum, UInt16 index,
	//						NetConfigNamePtr newNameP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 index, NetConfigNamePtr newNameP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConfigAliasSet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConfigAliasSet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConfigAliasSet");

	//	Err NetLibConfigAliasSet( UInt16 libRefNum, UInt16 configIndex,
	//						UInt16 aliasToIndex)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 configIndex, UInt16 aliasToIndex");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConfigAliasSet (void)
{
	//	Err NetLibConfigAliasSet( UInt16 libRefNum, UInt16 configIndex,
	//						UInt16 aliasToIndex)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 configIndex, UInt16 aliasToIndex");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	NetLibHeadpatch::NetLibConfigAliasGet
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

CallROMType NetLibHeadpatch::NetLibConfigAliasGet (void)
{
	PRINTF ("----------------------------------------");
	PRINTF ("NetLibConfigAliasGet");

	//	Err NetLibConfigAliasGet( UInt16 libRefNum, UInt16 aliasIndex,
	//						UInt16 *indexP, Boolean *isAnotherAliasP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 aliasIndex, UInt16 *indexP, Boolean *isAnotherAliasP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Examine the parameters

	PRINTF ("\t-->Executing ROM version");
	return kExecuteROM;
}

void NetLibTailpatch::NetLibConfigAliasGet (void)
{
	//	Err NetLibConfigAliasGet( UInt16 libRefNum, UInt16 aliasIndex,
	//						UInt16 *indexP, Boolean *isAnotherAliasP)

	CALLED_SETUP ("Err", "UInt16 libRefNum, UInt16 aliasIndex, UInt16 *indexP, Boolean *isAnotherAliasP");

	// Get the stack-based parameters.
//	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Get the result.
	GET_RESULT_VAL (Err);

	// Examine the results.
	PRINTF ("\tResult = 0x%08X", result);
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetOptLevelString
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

#define DOCASE(name) case name: return #name;

const char* PrvGetOptLevelString (NetSocketOptLevelEnum level)
{
	switch (level)
	{
		DOCASE (netSocketOptLevelIP)
		DOCASE (netSocketOptLevelTCP)
		DOCASE (netSocketOptLevelSocket)
	}

	return "Unknown NetSocketOptLevelEnum";
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetOptString
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

const char* PrvGetOptString (NetSocketOptLevelEnum level, NetSocketOptEnum opt)
{
	switch (level)
	{
		case netSocketOptLevelIP:
			switch (opt)
			{
				DOCASE (netSocketOptIPOptions)

//				case netSocketOptTCPNoDelay:		== 1 (netSocketOptIPOptions)
				case netSocketOptTCPMaxSeg:

//				case netSocketOptSockDebug:			== 1 (netSocketOptIPOptions)
//				case netSocketOptSockAcceptConn:	== 2 (netSocketOptTCPMaxSeg)
				case netSocketOptSockReuseAddr:
				case netSocketOptSockKeepAlive:
				case netSocketOptSockDontRoute:
				case netSocketOptSockBroadcast:
				case netSocketOptSockUseLoopback:
				case netSocketOptSockLinger:
				case netSocketOptSockOOBInLine:
				case netSocketOptSockSndBufSize:
				case netSocketOptSockRcvBufSize:
				case netSocketOptSockSndLowWater:
				case netSocketOptSockRcvLowWater:
				case netSocketOptSockSndTimeout:
				case netSocketOptSockRcvTimeout:
				case netSocketOptSockErrorStatus:
				case netSocketOptSockSocketType:
				case netSocketOptSockNonBlocking:
				case netSocketOptSockRequireErrClear:
				case netSocketOptSockMultiPktAddr:
					break;
			}
			break;

		case netSocketOptLevelTCP:
			switch (opt)
			{
//				case netSocketOptIPOptions:			== 1 (netSocketOptTCPNoDelay)
//					break;

				DOCASE (netSocketOptTCPNoDelay)
				DOCASE (netSocketOptTCPMaxSeg)

//				case netSocketOptSockDebug:			== 1 (netSocketOptTCPNoDelay)
//				case netSocketOptSockAcceptConn:	== 2 (netSocketOptTCPMaxSeg)
				case netSocketOptSockReuseAddr:
				case netSocketOptSockKeepAlive:
				case netSocketOptSockDontRoute:
				case netSocketOptSockBroadcast:
				case netSocketOptSockUseLoopback:
				case netSocketOptSockLinger:
				case netSocketOptSockOOBInLine:
				case netSocketOptSockSndBufSize:
				case netSocketOptSockRcvBufSize:
				case netSocketOptSockSndLowWater:
				case netSocketOptSockRcvLowWater:
				case netSocketOptSockSndTimeout:
				case netSocketOptSockRcvTimeout:
				case netSocketOptSockErrorStatus:
				case netSocketOptSockSocketType:
				case netSocketOptSockNonBlocking:
				case netSocketOptSockRequireErrClear:
				case netSocketOptSockMultiPktAddr:
					break;
			}
			break;

		case netSocketOptLevelSocket:
			switch (opt)
			{
//				case netSocketOptIPOptions:
//				case netSocketTCPNoDelay:
//				case netSocketTCPMaxSeg:
//					break;

				DOCASE (netSocketOptSockDebug)
				DOCASE (netSocketOptSockAcceptConn)
				DOCASE (netSocketOptSockReuseAddr)
				DOCASE (netSocketOptSockKeepAlive)
				DOCASE (netSocketOptSockDontRoute)
				DOCASE (netSocketOptSockBroadcast)
				DOCASE (netSocketOptSockUseLoopback)
				DOCASE (netSocketOptSockLinger)
				DOCASE (netSocketOptSockOOBInLine)

				DOCASE (netSocketOptSockSndBufSize)
				DOCASE (netSocketOptSockRcvBufSize)
				DOCASE (netSocketOptSockSndLowWater)
				DOCASE (netSocketOptSockRcvLowWater)
				DOCASE (netSocketOptSockSndTimeout)
				DOCASE (netSocketOptSockRcvTimeout)
				DOCASE (netSocketOptSockErrorStatus)
				DOCASE (netSocketOptSockSocketType)

				DOCASE (netSocketOptSockNonBlocking)
				DOCASE (netSocketOptSockRequireErrClear)
				DOCASE (netSocketOptSockMultiPktAddr)
			}
			break;
	}

	return "Unknown NetSocketOptEnum";
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetDottedIPString
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

const char* PrvGetDottedIPString (const NetSocketAddrType& addr)
{
	NetSocketAddrINType	ipAddr = (const NetSocketAddrINType&) addr;
	long				tempIP = ntohl (ipAddr.addr);

	static char			dottedIPStr[20];

	sprintf (dottedIPStr, "%ld.%ld.%ld.%ld",
		((tempIP >> 24) & 0xFF),
		((tempIP >> 16) & 0xFF),
		((tempIP >> 8) & 0xFF),
		((tempIP) & 0xFF));

	return dottedIPStr;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetPortString
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

const char* PrvGetPortString (const NetSocketAddrType& addr)
{
	NetSocketAddrINType	ipAddr = (const NetSocketAddrINType&) addr;
	UInt16				port = ntohs (ipAddr.port);

	static char			portStr[10];

	sprintf (portStr, "0x%04X", port);

	return portStr;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetErrorString
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

const char* PrvGetErrorString (Err err)
{
	switch (err)
	{
		DOCASE (errNone)
		DOCASE (netErrAlreadyOpen)
		DOCASE (netErrNotOpen)
		DOCASE (netErrStillOpen)
		DOCASE (netErrParamErr)
		DOCASE (netErrNoMoreSockets)
		DOCASE (netErrOutOfResources)
		DOCASE (netErrOutOfMemory)
		DOCASE (netErrSocketNotOpen)
		DOCASE (netErrSocketBusy)
		DOCASE (netErrMessageTooBig)
		DOCASE (netErrSocketNotConnected)
		DOCASE (netErrNoInterfaces)
		DOCASE (netErrBufTooSmall)
		DOCASE (netErrUnimplemented)
		DOCASE (netErrPortInUse)
		DOCASE (netErrQuietTimeNotElapsed)
		DOCASE (netErrInternal)
		DOCASE (netErrTimeout)
		DOCASE (netErrSocketAlreadyConnected)
		DOCASE (netErrSocketClosedByRemote)
		DOCASE (netErrOutOfCmdBlocks)
		DOCASE (netErrWrongSocketType)
		DOCASE (netErrSocketNotListening)
		DOCASE (netErrUnknownSetting)
		DOCASE (netErrInvalidSettingSize)
		DOCASE (netErrPrefNotFound)
		DOCASE (netErrInvalidInterface)
		DOCASE (netErrInterfaceNotFound)
		DOCASE (netErrTooManyInterfaces)
		DOCASE (netErrBufWrongSize)
		DOCASE (netErrUserCancel)
		DOCASE (netErrBadScript)
		DOCASE (netErrNoSocket)
		DOCASE (netErrSocketRcvBufFull)
		DOCASE (netErrNoPendingConnect)
		DOCASE (netErrUnexpectedCmd)
		DOCASE (netErrNoTCB)
		DOCASE (netErrNilRemoteWindowSize)
		DOCASE (netErrNoTimerProc)
		DOCASE (netErrSocketInputShutdown)
		DOCASE (netErrCmdBlockNotCheckedOut)
		DOCASE (netErrCmdNotDone)
		DOCASE (netErrUnknownProtocol)
		DOCASE (netErrUnknownService)
		DOCASE (netErrUnreachableDest)
		DOCASE (netErrReadOnlySetting)
		DOCASE (netErrWouldBlock)
		DOCASE (netErrAlreadyInProgress)
		DOCASE (netErrPPPTimeout)
		DOCASE (netErrPPPBroughtDown)
		DOCASE (netErrAuthFailure)
		DOCASE (netErrPPPAddressRefused)
		DOCASE (netErrDNSNameTooLong)
		DOCASE (netErrDNSBadName)
		DOCASE (netErrDNSBadArgs)
		DOCASE (netErrDNSLabelTooLong)
		DOCASE (netErrDNSAllocationFailure)
		DOCASE (netErrDNSTimeout)
		DOCASE (netErrDNSUnreachable)
		DOCASE (netErrDNSFormat)
		DOCASE (netErrDNSServerFailure)
		DOCASE (netErrDNSNonexistantName)
		DOCASE (netErrDNSNIY)
		DOCASE (netErrDNSRefused)
		DOCASE (netErrDNSImpossible)
		DOCASE (netErrDNSNoRRS)
		DOCASE (netErrDNSAborted)
		DOCASE (netErrDNSBadProtocol)
		DOCASE (netErrDNSTruncated)
		DOCASE (netErrDNSNoRecursion)
		DOCASE (netErrDNSIrrelevant)
		DOCASE (netErrDNSNotInLocalCache)
		DOCASE (netErrDNSNoPort)
		DOCASE (netErrIPCantFragment)
		DOCASE (netErrIPNoRoute)
		DOCASE (netErrIPNoSrc)
		DOCASE (netErrIPNoDst)
		DOCASE (netErrIPktOverflow)
		DOCASE (netErrTooManyTCPConnections)
		DOCASE (netErrNoDNSServers)
		DOCASE (netErrInterfaceDown)
		DOCASE (netErrNoChannel)
		DOCASE (netErrDieState)
		DOCASE (netErrReturnedInMail)
		DOCASE (netErrReturnedNoTransfer)
		DOCASE (netErrReturnedIllegal)
		DOCASE (netErrReturnedCongest)
		DOCASE (netErrReturnedError)
		DOCASE (netErrReturnedBusy)
		DOCASE (netErrGMANState)
		DOCASE (netErrQuitOnTxFail)
		DOCASE (netErrFlexListFull)
		DOCASE (netErrSenderMAN)
		DOCASE (netErrIllegalType)
		DOCASE (netErrIllegalState)
		DOCASE (netErrIllegalFlags)
		DOCASE (netErrIllegalSendlist)
		DOCASE (netErrIllegalMPAKLength)
		DOCASE (netErrIllegalAddressee)
		DOCASE (netErrIllegalPacketClass)
		DOCASE (netErrBufferLength)
		DOCASE (netErrNiCdLowBattery)
		DOCASE (netErrRFinterfaceFatal)
		DOCASE (netErrIllegalLogout)
		DOCASE (netErrAAARadioLoad)
		DOCASE (netErrAntennaDown)
		DOCASE (netErrNiCdCharging)
		DOCASE (netErrAntennaWentDown)
		DOCASE (netErrNotActivated)
		DOCASE (netErrRadioTemp)
		DOCASE (netErrConfigNotFound)
		DOCASE (netErrConfigCantDelete)
		DOCASE (netErrConfigTooMany)
		DOCASE (netErrConfigBadName)
		DOCASE (netErrConfigNotAlias)
		DOCASE (netErrConfigCantPointToAlias)
		DOCASE (netErrConfigEmpty)
		DOCASE (netErrAlreadyOpenWithOtherConfig)
		DOCASE (netErrConfigAliasErr)
		DOCASE (netErrNoMultiPktAddr)
		DOCASE (netErrOutOfPackets)
		DOCASE (netErrMultiPktAddrReset)
		DOCASE (netErrStaleMultiPktAddr)
	}

	return "Unknown Error Code";
}
