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

// Resource IDs for strings.

//	1000 - 1999:	Message Templates
//		1000 - 1199:		Error messages
//		1200 - 1399:		Other messages
//		1400 - 1499:		Menu strings
//	2000 - xxxx:	Template Parts
//		2000 - 2199:		Misc parts
//		2200 - 2399:		Operations descriptions
//		2400 - 2599:		Error descriptions
//			2400 - 2499:		OS Errors (e.g., "the file was not found")
//			2500 - 2599:		User/app errors (e.g., "I don't understand that file")
//		2600 - 2799:		Recovery strings (e.g., "Reboot and try again")
//		2900 - 2964:		Exception descriptions (e.g., "bus error")
//		3000 - 4999:		Palm OS system function names
//		5000 - 6999:		Palm OS Library function names
//		7000 - xxxx:		Palm OS Error descriptions

// --------------------------------------------------------------------------------
//	Message Templates -- Error messages
// --------------------------------------------------------------------------------

#pragma mark Message Templates -- Error messages

#define kStr_OpError					1000
#define kStr_OpErrorRecover				1001

// Hardware Exceptions

#define kStr_ErrBusError				1010
#define kStr_ErrAddressError			1011
#define kStr_ErrIllegalInstruction		1012
#define kStr_ErrDivideByZero			1013
#define kStr_ErrCHKInstruction			1014
#define kStr_ErrTRAPVInstruction		1015
#define kStr_ErrPrivilegeViolation		1016
#define kStr_ErrTrace					1017
#define kStr_ErrATrap					1018
#define kStr_ErrFTrap					1019
#define kStr_ErrTRAPx					1020

// Special cases to hardware exceptions

#define kStr_ErrStorageHeap				1030
#define kStr_ErrNoDrawWindow			1031
#define kStr_ErrNoGlobals				1032
#define kStr_ErrSANE					1033
#define kStr_ErrTRAP0					1034
#define kStr_ErrTRAP8					1035

// Fatal Poser-detected errors

#define kStr_ErrStackOverflow			1040
#define kStr_ErrUnimplementedTrap		1041
#define kStr_ErrInvalidRefNum			1042
#define kStr_ErrCorruptedHeap			1043
#define kStr_ErrInvalidPC1				1044
#define kStr_ErrInvalidPC2				1045

// Non-fatal Poser-detected errors

#define kStr_ErrLowMemory				1050
#define kStr_ErrSystemGlobals			1051
#define kStr_ErrScreen					1052
#define kStr_ErrHardwareRegisters		1053
#define kStr_ErrROM						1054
#define kStr_ErrMemMgrStructures		1055
#define kStr_ErrMemMgrSemaphore			1056
#define kStr_ErrFreeChunk				1057
#define kStr_ErrUnlockedChunk			1058
#define kStr_ErrLowStack				1059
#define kStr_ErrStackFull				1060
#define kStr_ErrSizelessObject			1061
#define kStr_ErrOffscreenObject			1062
#define kStr_ErrFormAccess				1063
#define kStr_ErrFormObjectListAccess	1064
#define kStr_ErrFormObjectAccess		1065
#define kStr_ErrWindowAccess			1066
#define kStr_ErrBitmapAccess			1067
#define kStr_ErrProscribedFunction		1068
#define kStr_ErrStepSpy					1069
#define kStr_ErrWatchpoint				1070
#define kStr_ErrMemoryLeak				1071
#define kStr_ErrMemoryLeaks				1072

// Palm OS-detected errors

#define kStr_ErrSysFatalAlert			1080
#define kStr_ErrDbgMessage				1081

// Other errors

#define kStr_BadChecksum				1100
#define kStr_UnknownDeviceWarning		1101
#define kStr_UnknownDeviceError			1102
#define kStr_MissingSkins				1103
#define kStr_InconsistentDatabaseDates	1104
#define kStr_NULLDatabaseDate			1105
#define kStr_NeedHostFS					1106
#define kStr_InvalidAddressNotEven		1107
#define kStr_InvalidAddressNotInROMOrRAM 1108
#define kStr_CannotParseCondition		1109
#define kStr_UserNameTooLong			1110


// --------------------------------------------------------------------------------
//	Message Templates -- Other messages
// --------------------------------------------------------------------------------

#pragma mark Message Templates -- Other messages

// Loading/saving

#define kStr_SaveSessionAs				1200
#define kStr_LoadSession				1201
#define kStr_LoadSessionForReplay		1202
#define kStr_LoadSessionForMinimize		1203
#define kStr_SaveScreenAs				1204
#define kStr_ImportFile					1205
#define kStr_SaveBeforeClosing			1206
#define kStr_SaveBeforeQuitting			1207

#define kStr_GremlinNumber				1210
#define kStr_GremlinXofYEvents			1211
#define kStr_GremlinXEvents				1212
#define kStr_GremlinElapsedTime			1213

#define kStr_MinimizePassNumber			1220
#define kStr_MinimizeXofYEvents			1221
#define kStr_MinimizeElapsedTime		1222
#define kStr_MinimizeRange				1223
#define kStr_MinimizeDiscarded			1224

#define kStr_AppAndVers					1230
#define kStr_XofYFiles					1231
#define kStr_GremlinStarted				1232
#define kStr_GremlinEnded				1233
#define kStr_EntireDevice				1234
#define kStr_ErrDisplayMessage			1235
#define kStr_UnknownFatalError			1236
#define kStr_EmulatorOff				1237
#define kStr_LogFileSize				1238
#define kStr_InternalErrorException		1239
#define kStr_InternalErrorMessage		1240
#define kStr_WillNowReset				1241
#define kStr_CommPortError				1242
#define kStr_SocketsError				1243	
#define kStr_MustTurnOnLogging			1244
#define kStr_MustTurnOnShowDialog		1245

#define kStr_ROMTransferInstructions	1250	// ... 1258


// --------------------------------------------------------------------------------
//	Message Templates -- Menu strings
// --------------------------------------------------------------------------------

#define kStr_MenuSessionNew				1400
#define kStr_MenuSessionOpenOther		1401
#define kStr_MenuSessionClose			1402
#define kStr_MenuSessionSave			1403
#define kStr_MenuSessionSaveAs			1404
#define kStr_MenuSessionBound			1405
#define kStr_MenuScreenSave				1406
#define kStr_MenuSessionInfo			1407

#define kStr_MenuImportOther			1410
#define kStr_MenuExport					1411
#define kStr_MenuHotSync				1412
#define kStr_MenuReset					1413
#define kStr_MenuDownloadROM			1414

#define kStr_MenuUndo					1420
#define kStr_MenuCut					1421
#define kStr_MenuCopy					1422
#define kStr_MenuPaste					1423
#define kStr_MenuClear					1424

#define kStr_MenuPreferences			1430
#define kStr_MenuLogging				1431
#define kStr_MenuDebugging				1432
#define kStr_MenuErrorHandling			1433
#define kStr_MenuTracing				1434
#define kStr_MenuSkins					1435
#define kStr_MenuHostFS					1436
#define kStr_MenuBreakpoints			1437

#define kStr_MenuGremlinsNew			1440
#define kStr_MenuGremlinsSuspend		1441
#define kStr_MenuGremlinsStep			1442
#define kStr_MenuGremlinsResume			1443
#define kStr_MenuGremlinsStop			1444
#define kStr_MenuEventReplay			1445
#define kStr_MenuEventMinimize			1446

#if HAS_PROFILING

#define kStr_MenuProfileStart			1450
#define kStr_MenuProfileStop			1451
#define kStr_MenuProfileDump			1452

#endif

#define kStr_MenuAbout					1460
#define kStr_MenuQuit					1461
#define kStr_MenuFile					1462
#define kStr_MenuEdit					1463
#define kStr_MenuGremlins				1464
#define kStr_MenuProfile				1465
#define kStr_MenuOpen					1466
#define kStr_MenuImport					1467
#define kStr_MenuSettings				1468
#define kStr_MenuEmpty					1469
#define kStr_MenuBlank					1470


// --------------------------------------------------------------------------------
//	Template Parts -- Misc parts
// --------------------------------------------------------------------------------

#pragma mark Template Parts -- Misc parts

#define kStr_AppName				2000
#define kStr_Untitled				2001
#define kStr_DefaultROMName			2002
#define kStr_ProfileResults			2003
#define kStr_Autoload				2004
#define kStr_Autorun				2005
#define kStr_AutorunAndQuit			2006

#define kStr_ReadFrom				2010
#define kStr_WroteTo				2011
#define kStr_WrittenTo				2012

#define kStr_CurrentAppUC			2020
#define kStr_CurrentAppLC			2021
#define kStr_UnknownVersion			2022

#define kStr_OK						2030
#define kStr_Cancel					2031
#define kStr_Yes					2032
#define kStr_No						2033
#define kStr_Continue				2034
#define kStr_Debug					2035
#define kStr_Reset					2036

#define kStr_Browse					2040
#define kStr_EmptyMRU				2041
#define kStr_OtherMRU				2042

#define kStr_ProfPartial			2050
#define kStr_ProfFunctions			2051
#define kStr_ProfInterrupts			2052
#define kStr_ProfOverflow			2053
#define kStr_ProfInterruptX			2054
#define kStr_ProfTrapNameAddress	2055
#define kStr_ProfROMNameAddress		2056
#define kStr_ProfDebugNameAddress	2057
#define kStr_ProfUnknownName		2058

#define kStr_UnknownTrapNumber		2060
#define kStr_UnknownErrorCode		2061

#define kStr_NoPort					2070

#define kStr_ChunkNotInHeap			2080
#define kStr_ChunkTooLarge			2081
#define kStr_InvalidFlags			2082
#define kStr_HOffsetNotInMPT		2083
#define kStr_HOffsetNotBackPointing	2084
#define kStr_InvalidLockCount		2085

#define kStr_UnmappedAddress		2090
#define kStr_NotInCodeSegment		2091
#define kStr_OddAddress				2092

#define kStr_Waiting				2100
#define kStr_Transferring			2101

// Reasons for why the PC was changed

#define kStr_WhenRTS				2110
#define kStr_WhenRTE				2111
#define kStr_WhenJSR				2112
#define kStr_WhenException			2113
#define kStr_WhenSysCall			2114


#define kStr_ShowInDialog			2120
#define kStr_AutomaticallyContinue	2121
#define kStr_AutomaticallyQuit		2122
#define kStr_NextGremlin			2123


// --------------------------------------------------------------------------------
//	Template Parts -- Operations descriptions
// --------------------------------------------------------------------------------

#pragma mark Template Parts -- Operations descriptions

#define kStr_GenericOperation		2200

#define kStr_CmdAbout				2210

#define kStr_CmdNew					2220
#define kStr_CmdOpen				2221
#define kStr_CmdClose				2222
#define kStr_CmdSave				2223
#define kStr_CmdSaveBound			2224
#define kStr_CmdSaveScreen			2225
#define kStr_CmdSessionInfo			2226
#define kStr_CmdInstall				2227
#define kStr_CmdInstallMany			2228
#define kStr_CmdExportDatabase		2229
#define kStr_CmdHotSync				2230
#define kStr_CmdReset				2231
#define kStr_CmdTransferROM			2232
#define kStr_CmdQuit				2233

#define kStr_CmdPreferences			2240
#define kStr_CmdLoggingOptions		2241
#define kStr_CmdDebugOptions		2242
#define kStr_CmdErrorHandling		2243
#define kStr_CmdSkins				2244
#define kStr_CmdBreakpoints			2245
#define kStr_CmdTracingOptions		2246
#define kStr_CmdHostFSOptions		2247

#define kStr_CmdGremlinNew			2250
#define kStr_CmdGremlinSuspend		2251
#define kStr_CmdGremlinStep			2252
#define kStr_CmdGremlinResume		2253
#define kStr_CmdGremlinStop			2254
#define kStr_CmdEventReplay			2255
#define kStr_CmdEventMinimize		2256

#define kStr_CmdProfileStart		2260
#define kStr_CmdProfileStop			2261
#define kStr_CmdProfileDump			2262

#define kStr_ResizeWindow			2270
#define kStr_EnterKey				2271
#define kStr_EnterPen				2272
#define kStr_PressButton			2273
#define kStr_OpenFiles				2274
#define kStr_LoadPreferences		2275
#define kStr_OpenTheSerialPort		2276

#define kStr_CmdTracingSuspend		2280
#define kStr_CmdTracingResume		2281
#define kStr_CmdTracingReconnect	2282

// --------------------------------------------------------------------------------
//	Template Parts -- Error descriptions -- OS Errors
// --------------------------------------------------------------------------------

#pragma mark Template Parts -- Error descriptions -- OS Errors

#define kStr_GenericError			2400
#define kStr_UserCancel				2401	// userCanceledErr
#define kStr_TimeoutError			2402	// kernelTimeoutErr

#define kStr_DirectoryFull			2410	// dirFulErr
#define kStr_DiskFull				2411	// dskFulErr
#define kStr_DiskMissing			2412	// nsvErr, volOffLinErr, volGoneErr
#define kStr_IOError				2413	// ioErr
#define kStr_BadFileName			2414	// bdNamErr
#define kStr_TooManyFilesOpen		2415	// tmfoErr
#define kStr_FileNotFound			2416	// fnfErr
#define kStr_DiskWriteProtected		2417	// wPrErr
#define kStr_FileLocked				2418	// fLckdErr, permErr
#define kStr_DiskLocked				2419	// vLckdErr
#define kStr_FileBusy				2420	// fBsyErr
#define kStr_DuplicateFileName		2421	// dupFNErr

#define kStr_MemFull				2430	// memFullErr, kError_OutOfMemory

#define kStr_SerialPortBusy			2440

#define kStr_GenericPalmError		2480
#define kStr_DmErrDatabaseOpen		2481	// dmErrDatabaseOpen
#define kStr_MemErrNotEnoughSpace	2482	// memErrNotEnoughSpace


// --------------------------------------------------------------------------------
//	Template Parts -- Error descriptions -- User/app errors
// --------------------------------------------------------------------------------

#pragma mark Template Parts -- Error descriptions -- User/app errors

#define kStr_OnlySameType			2500
#define kStr_OnlyOnePSF				2501
#define kStr_OnlyOneROM				2502
#define kStr_UnknownType			2503

#define kStr_OldFormat				2510
#define kStr_WrongROMForType		2511	// kError_WrongROMForType

#define kStr_BadROM					2520	// kError_BadROM
#define kStr_UnsupportedROM			2521	// kError_UnsupportedROM
#define kStr_InvalidDevice			2522	// kError_InvalidDevice
#define kStr_InvalidSession			2523	// kError_InvalidSessionFile
#define kStr_InvalidConfiguration	2524	// kError_InvalidConfiguration

#define kStr_NameNotNULLTerminated		2530
#define kStr_NameNotPrintable			2531
#define kStr_FileTooSmall				2532
#define kStr_nextRecordListIDNonZero	2533
#define kStr_ResourceTooSmall			2534
#define kStr_RecordTooSmall				2535
#define kStr_ResourceOutOfRange			2536
#define kStr_RecordOutOfRange			2537
#define kStr_OverlappingResource		2538
#define kStr_OverlappingRecord			2539
#define kStr_ResourceMemError			2540
#define kStr_RecordMemError				2541
#define kStr_AppInfoMemError			2542
#define kStr_DuplicateResource			2543

#define kStr_SystemUseOnly			2550
#define kStr_Obsolete				2551


// --------------------------------------------------------------------------------
//	Template Parts -- Recovery strings
// --------------------------------------------------------------------------------

#pragma mark Template Parts -- Recovery strings

#define kStr_DeleteSomeFiles		2610
#define kStr_UnlockTheDisk			2611
#define kStr_UnlockTheFile			2612

#define kStr_NeedMoreRAM			2620

#define kStr_TransferAROM			2630
#define kStr_DownloadAROM			2631


// --------------------------------------------------------------------------------
//	Template Parts -- Exception descriptions
// --------------------------------------------------------------------------------

#pragma mark Template Parts -- Exception descriptions

#define kStr_ExceptionBase			2900


// --------------------------------------------------------------------------------
//	Template Parts -- Palm OS function names
// --------------------------------------------------------------------------------

#pragma mark Template Parts -- Palm OS function names

#define kStr_SysTrapBase			3000

#define kStr_LibTrapBase			5000
#define kStr_HTALLibTrapBase		kStr_LibTrapBase + 100
#define kStr_INetLibTrapBase		kStr_LibTrapBase + 200
#define kStr_IrLibTrapBase			kStr_LibTrapBase + 300
#define kStr_NetLibTrapBase			kStr_LibTrapBase + 400
#define kStr_NPILibTrapBase			kStr_LibTrapBase + 500
#define kStr_RailLibTrapBase		kStr_LibTrapBase + 600
#define kStr_SecLibTrapBase			kStr_LibTrapBase + 700
#define kStr_SerLibTrapBase			kStr_LibTrapBase + 800
#define kStr_WebLibTrapBase			kStr_LibTrapBase + 900

// --------------------------------------------------------------------------------
//	Template Parts -- Error descriptions
// --------------------------------------------------------------------------------
#pragma mark Template Parts -- Palm OS Error descriptions

#define kStr_PalmOSErrorBase		7000

// Copied from SystemMgr.h -- needed for string ID assignments

/************************************************************
 * Error Classes for each manager
 *************************************************************/
#define	errNone						0x0000	// No error

#define	memErrorClass				0x0100	// Memory Manager
#define	dmErrorClass				0x0200	// Data Manager
#define	serErrorClass				0x0300	// Serial Manager
#define	slkErrorClass				0x0400	// Serial Link Manager
#define	sysErrorClass				0x0500	// System Manager
#define	fplErrorClass				0x0600	// Floating Point Library
#define	flpErrorClass				0x0680	// New Floating Point Library
#define	evtErrorClass				0x0700  	// System Event Manager
#define	sndErrorClass				0x0800  	// Sound Manager
#define	almErrorClass				0x0900  	// Alarm Manager
#define	timErrorClass				0x0A00  	// Time Manager
#define	penErrorClass				0x0B00  	// Pen Manager
#define	ftrErrorClass				0x0C00  	// Feature Manager
#define	cmpErrorClass				0x0D00  	// Connection Manager (HotSync)
#define	dlkErrorClass				0x0E00	// Desktop Link Manager
#define	padErrorClass				0x0F00	// PAD Manager
#define	grfErrorClass				0x1000	// Graffiti Manager
#define	mdmErrorClass				0x1100	// Modem Manager
#define	netErrorClass				0x1200	// Net Library
#define	htalErrorClass				0x1300	// HTAL Library
#define	inetErrorClass				0x1400	// INet Library
#define	exgErrorClass				0x1500	// Exg Manager
#define	fileErrorClass				0x1600	// File Stream Manager
#define	rfutErrorClass				0x1700	// RFUT Library
#define	txtErrorClass				0x1800	// Text Manager
#define	tsmErrorClass				0x1900	// Text Services Library
#define	webErrorClass				0x1A00	// Web Library
#define	secErrorClass				0x1B00	// Security Library
#define	emuErrorClass				0x1C00	// Emulator Control Manager
#define	flshErrorClass				0x1D00	// Flash Manager
#define	pwrErrorClass				0x1E00	// Power Manager
#define	cncErrorClass				0x1F00	// Connection Manager (Serial Communication)
#define	actvErrorClass				0x2000	// Activation application
#define	radioErrorClass			0x2100	// Radio Manager (Library)
#define	dispErrorClass				0x2200	// Display Driver Errors.
#define	bltErrorClass				0x2300	// Blitter Driver Errors.
#define	winErrorClass				0x2400	// Window manager.
#define	omErrorClass				0x2500	// Overlay Manager
#define	menuErrorClass				0x2600	// Menu Manager
#define	lz77ErrorClass				0x2700	// Lz77 Library
#define	smsErrorClass				0x2800	// Sms Library
#define	expErrorClass				0x2900	// Expansion Manager and Slot Driver Library
#define	vfsErrorClass				0x2A00	// Virtual Filesystem Manager and Filesystem library
#define	lmErrorClass				0x2B00	// Locale Manager
#define	intlErrorClass				0x2C00	// International Manager
#define pdiErrorClass				0x2D00	// PDI Library
#define	attnErrorClass				0x2E00	// Attention Manager
#define	telErrorClass				0x2F00	// Telephony Manager
#define hwrErrorClass				0x3000	// Hardware Manager (HAL)
#define	blthErrorClass				0x3100	// Bluetooth Library Error Class
#define	udaErrorClass				0x3200	// UDA Manager Error Class

#define  errInfoClass				0x7F00	// special class shows information w/o error code
#define	appErrorClass				0x8000	// Application-defined errors

