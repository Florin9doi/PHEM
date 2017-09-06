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

#ifndef _ROMSTUBS_H_
#define _ROMSTUBS_H_

void ClipboardAddItem (const ClipboardFormatType format, const void* ptr, UInt16 length);
MemHandle ClipboardGetItem (const ClipboardFormatType format, UInt16* length);

const Char* CtlGetLabel (const ControlType *controlP);

Err DlkDispatchRequest (DlkServerSessionPtr sessP);
Err DlkGetSyncInfo (UInt32* succSyncDateP, UInt32* lastSyncDateP,
			DlkSyncStateType* syncStateP, Char* nameBufP,
			Char* logBufP, Int32* logLenP);

Err DmCloseDatabase (DmOpenRef dbR);
Err DmCreateDatabase (UInt16 cardNo, const Char * const nameP, 
					UInt32 creator, UInt32 type, Boolean resDB);
Err DmDatabaseInfo (UInt16 cardNo, LocalID	dbID, Char* nameP,
					UInt16* attributesP, UInt16* versionP, UInt32* crDateP,
					UInt32* modDateP, UInt32* bckUpDateP,
					UInt32* modNumP, LocalID* appInfoIDP,
					LocalID* sortInfoIDP, UInt32* typeP,
					UInt32* creatorP);
Err DmDeleteDatabase (UInt16 cardNo, LocalID dbID);
LocalID DmFindDatabase (UInt16 cardNo, const Char* nameP);
MemHandle DmGet1Resource (DmResType type, DmResID id);
LocalID DmGetDatabase (UInt16 cardNo, UInt16 index);
Err DmGetLastErr (void);
Err	DmGetNextDatabaseByTypeCreator (Boolean newSearch, DmSearchStatePtr stateInfoP,
			 		UInt32 type, UInt32 creator, Boolean onlyLatestVers, 
			 		UInt16* cardNoP, LocalID* dbIDP);
MemHandle DmGetResource (DmResType type, DmResID id);
MemHandle DmGetResourceIndex (DmOpenRef dbP, UInt16 index);
MemHandle DmNewHandle (DmOpenRef dbR, UInt32 size);
MemHandle DmNewRecord (DmOpenRef dbR, UInt16* atP, UInt32 size);
MemHandle DmNewResource (DmOpenRef dbR, DmResType resType, DmResID resID, UInt32 size);
DmOpenRef DmNextOpenDatabase(DmOpenRef currentP);
UInt16 DmNumDatabases (UInt16 cardNo);
UInt16 DmNumRecords (DmOpenRef dbP);
UInt16 DmNumResources (DmOpenRef dbP);
DmOpenRef DmOpenDatabase (UInt16 cardNo, LocalID dbID, UInt16 mode);
Err DmOpenDatabaseInfo (DmOpenRef dbP, LocalID* dbIDP, 
					UInt16* openCountP, UInt16* modeP, UInt16* cardNoP,
					Boolean* resDBP);
Err DmRecordInfo (DmOpenRef dbP, UInt16 index,
					UInt16* attrP, UInt32* uniqueIDP, LocalID* chunkIDP);
Err DmReleaseRecord (DmOpenRef dbR, UInt16 index, Boolean dirty);
Err DmReleaseResource (MemHandle resourceH);
Err DmResourceInfo (DmOpenRef dbP, UInt16 index,
					DmResType* resTypeP, DmResID* resIDP,  
					LocalID* chunkLocalIDP);
MemHandle DmQueryRecord (DmOpenRef dbP, UInt16 index);
Err DmSetDatabaseInfo (UInt16 cardNo, LocalID dbID, const Char* nameP,
					UInt16* attributesP, UInt16* versionP, UInt32* crDateP,
					UInt32* modDateP, UInt32* bckUpDateP,
					UInt32* modNumP, LocalID* appInfoIDP,
					LocalID* sortInfoIDP, UInt32* typeP,
					UInt32* creatorP);
Err DmSetRecordInfo (DmOpenRef dbR, UInt16 index, UInt16* attrP, UInt32* uniqueIDP);
Err DmWrite (MemPtr recordP, UInt32 offset, const void * const srcP, UInt32 bytes);

void EvtAddEventToQueue (EventType* event);
Err EvtEnqueueKey (UInt16 ascii, UInt16 keycode, UInt16 modifiers);
Err EvtEnqueuePenPoint (PointType* ptP);
const PenBtnInfoType*	EvtGetPenBtnList(UInt16* numButtons);
Err EvtResetAutoOffTimer (void);
Err EvtWakeup (void);

Err ExgLibControl(UInt16 libRefNum, UInt16 op, void *valueP, UInt16 *valueLenP);

void FldGetAttributes (const FieldType* fld, const FieldAttrPtr attrP);
UInt16 FldGetInsPtPosition (const FieldType* fld);
UInt16 FldGetMaxChars (const FieldType* fld);
UInt16 FldGetTextLength (const FieldType* fld);
Char* FldGetTextPtr (FieldType* fldP);

Int16 FntLineHeight (void);
UInt8 FntSetFont (UInt8 fontId);

FormType* FrmGetActiveForm (void);
UInt16 FrmGetFocus (const FormType* frm);
UInt16 FrmGetFormId (const FormType* frm);
UInt16 FrmGetNumberOfObjects (const FormType* frm);
void FrmGetObjectBounds (const FormType* frm, const UInt16 pObjIndex, const RectanglePtr r);
UInt16 FrmGetObjectId (const FormType* frm, const UInt16 objIndex);
UInt16 FrmGetObjectIndex (const FormType* formP, UInt16 objID);
MemPtr FrmGetObjectPtr (const FormType* frm, const UInt16 objIndex);
FormObjectKind FrmGetObjectType (const FormType* frm, const UInt16 objIndex);
const Char* FrmGetTitle (const FormType* frm);
WinHandle FrmGetWindowHandle (const FormType* frm);

Err FSCustomControl(UInt16 fsLibRefNum, UInt32 apiCreator, UInt16 apiSelector, 
					void *valueP, UInt16 *valueLenP);

Err FtrGet (UInt32 creator, UInt16 featureNum, UInt32* valueP);
Err FtrSet (UInt32 creator, UInt16 featureNum, UInt32 newValue);
Err	FtrUnregister (UInt32 creator, UInt16 featureNum);

Boolean IntlSetStrictChecks (Boolean iStrictChecks);

UInt32 KeyHandleInterrupt(Boolean periodic, UInt32 status);

Int16 LstGetNumberOfItems (const ListType* lst);
Int16 LstGetSelection (const ListType* lst);
Char * LstGetSelectionText (const ListType *listP, Int16 itemNum);

Err MemChunkFree (MemPtr chunkDataP);
MemPtr MemHandleLock (MemHandle h);
UInt32 MemHandleSize (MemHandle h);
LocalID MemHandleToLocalID (MemHandle h);
Err MemHandleUnlock (MemHandle h);
UInt16 MemHeapID (UInt16 cardNo, UInt16 heapIndex);
MemPtr MemHeapPtr (UInt16 heapID);
LocalIDKind MemLocalIDKind (LocalID local);
MemPtr MemLocalIDToGlobal (LocalID local, UInt16 cardNo);
UInt16 MemNumCards (void);
UInt16 MemNumHeaps (UInt16 cardNo);
Err MemNVParams (Boolean set, SysNVParamsPtr paramsP);
MemPtr MemPtrNew (UInt32 size);
Err MemPtrSetOwner (MemPtr p, UInt16 owner);
UInt32 MemPtrSize (MemPtr p);
Err MemPtrUnlock (MemPtr p);

Err NetLibConfigMakeActive (UInt16 refNum, UInt16 configIndex);

Err PenCalibrate (PointType* digTopLeftP, PointType* digBotRightP,
					PointType* scrTopLeftP, PointType* scrBotRightP);
Err	 PenRawToScreen(PointType* penP);
Err PenScreenToRaw (PointType* penP);

DmOpenRef PrefOpenPreferenceDBV10 (void);
DmOpenRef PrefOpenPreferenceDB (Boolean saved);
void PrefSetPreference (SystemPreferencesChoice choice, UInt32 value);

Err	SysCurAppDatabase (UInt16* cardNoP, LocalID* dbIDP);
Err SysKernelInfo (MemPtr p);
Err SysLibFind (const Char *nameP, UInt16 *refNumP);
Err SysLibLoad (UInt32 libType, UInt32 libCreator, UInt16 *refNumP);
SysLibTblEntryPtr SysLibTblEntry (UInt16 refNum);
UInt16 SysSetAutoOffTime (UInt16 seconds);
Err SysUIAppSwitch (UInt16 cardNo, LocalID dbID, UInt16 cmd, MemPtr cmdPBP);

Coord TblGetColumnSpacing (const TableType* tableP, Int16 column);
Coord TblGetColumnWidth (const TableType* tableP, Int16 column);
FieldPtr TblGetCurrentField (const TableType* table);
Boolean TblGetSelection (const TableType* tableP, Int16* rowP, Int16* columnP);
Coord TblGetRowHeight (const TableType* tableP, Int16 row);

UInt8 TxtByteAttr(UInt8 inByte);
UInt16 TxtCharBounds (const Char* inText, UInt32 inOffset, UInt32* outStart, UInt32* outEnd);
UInt16 TxtGetNextChar (const Char* inText, UInt32 inOffset, WChar* outChar);

extern void WinDisplayToWindowPt (Int16* extentX, Int16* extentY);
WinHandle WinGetActiveWindow (void);
void WinGetDisplayExtent (Int16* extentX, Int16* extentY);
WinHandle WinGetFirstWindow (void);
void WinGetWindowBounds (RectanglePtr r);
void WinPopDrawState (void);
WinHandle WinSetDrawWindow (WinHandle winHandle);
void WinWindowToDisplayPt (Int16* extentX, Int16* extentY);

#define			MemPtrFree(	p) \
						MemChunkFree(p)

// convert host Int16 to network Int16
#define			NetHToNS(x) 	(x)	
					
// convert host long to network long
#define			NetHToNL(x) 	(x)	
					
// convert network Int16 to host Int16
#define			NetNToHS(x) 	(x)	
					
// convert network long to host long
#define			NetNToHL(x) 	(x)						



#endif /* _ROMSTUBS_H_ */
