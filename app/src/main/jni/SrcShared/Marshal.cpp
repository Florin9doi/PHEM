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
#include "Marshal.h"

#include "EmPalmStructs.h"		// EmAliasHostGremlinInfoType, etc.
#include "EmPatchState.h"		// EmPatchState::OSMajorVersion
#include "HostControl.h"		// HostGremlinInfoType, etc.
#include "ROMStubs.h"			// NetHToNL, NetHToNS

#if PLATFORM_UNIX || PLATFORM_MAC
#include <netinet/in.h>			// ntohl, ntohs
#endif

// -------------------------
// ----- Binary buffer -----
// -------------------------

void* Marshal::GetBuffer (emuptr p, long len)
{
	void*	result = NULL;

	if (p)
	{
		result = Platform::AllocateMemory (len);

		if (result)
		{
			EmMem_memcpy (result, p, len);
		}
	}

	return result;
}

#if 0	// inline
template <class T>
void Marshal::PutBuffer (emuptr p, const T*& buf, long len)
{
	if (p)
	{
		EmMem_memcpy (p, (void*) buf, len);
		Platform::DisposeMemory ((void*) buf);
	}
}
#endif


#pragma mark -

// ---------------------
// ----- DlkServerSessionType -----
// ---------------------

void Marshal::GetDlkServerSessionType (emuptr p, DlkServerSessionType& dest)
{
	memset (&dest, 0, sizeof (DlkServerSessionType));

	if (p)
	{
		EmAliasDlkServerSessionType<PAS>	src(p);

		dest.htalLibRefNum		= src.htalLibRefNum;
		dest.maxHtalXferSize	= src.maxHtalXferSize;
		dest.eventProcP			= (DlkEventProcPtr) (emuptr) src.eventProcP;
		dest.eventRef			= src.eventRef;
		dest.canProcP			= (DlkUserCanProcPtr) (emuptr) src.canProcP;
		dest.canRef				= src.canRef;
		dest.condFilterH		= (MemHandle) (emuptr) src.condFilterH;
		dest.dlkDBID			= src.dlkDBID;
		dest.reserved1			= src.reserved1;
		dest.dbR				= (DmOpenRef) (emuptr) src.dbR;
		dest.cardNo				= src.cardNo;
		dest.dbCreator			= src.dbCreator;
	//	dest.dbName				= src.dbName;
		dest.dbOpenMode			= src.dbOpenMode;
		dest.created			= src.created;
		dest.isResDB			= src.isResDB;
		dest.ramBased			= src.ramBased;
		dest.readOnly			= src.readOnly;
		dest.dbLocalID			= src.dbLocalID;
		dest.initialModNum		= src.initialModNum;
		dest.curRecIndex		= src.curRecIndex;
	//	dest.creatorList		= src.creatorList;
		dest.syncState			= src.syncState;
		dest.complete			= src.complete;
		dest.conduitOpened		= src.conduitOpened;
		dest.logCleared			= src.logCleared;
		dest.resetPending		= src.resetPending;
		dest.gotCommand			= src.gotCommand;
		dest.cmdTID				= src.cmdTID;
		dest.reserved2			= src.reserved2;
		dest.cmdLen				= src.cmdLen;
		dest.cmdP				= (void *) (emuptr) src.cmdP;
		dest.cmdH				= (MemHandle) (emuptr) src.cmdH;
		dest.wStateFlags		= src.wStateFlags;
	//	dest.dbSearchState		= src.dbSearchState;
	}
}


void Marshal::PutDlkServerSessionType (emuptr p, const DlkServerSessionType& src)
{
	if (p)
	{
		EmAliasDlkServerSessionType<PAS>	dest(p);

		dest.htalLibRefNum		= src.htalLibRefNum;
		dest.maxHtalXferSize	= src.maxHtalXferSize;
		dest.eventProcP			= (emuptr) (DlkEventProcPtr) src.eventProcP;
		dest.eventRef			= src.eventRef;
		dest.canProcP			= (emuptr) (DlkUserCanProcPtr) src.canProcP;
		dest.canRef				= src.canRef;
		dest.condFilterH		= (emuptr) (MemHandle) src.condFilterH;
		dest.dlkDBID			= src.dlkDBID;
		dest.reserved1			= src.reserved1;
		dest.dbR				= (emuptr) (DmOpenRef) src.dbR;
		dest.cardNo				= src.cardNo;
		dest.dbCreator			= src.dbCreator;
	//	dest.dbName				= src.dbName;
		dest.dbOpenMode			= src.dbOpenMode;
		dest.created			= src.created;
		dest.isResDB			= src.isResDB;
		dest.ramBased			= src.ramBased;
		dest.readOnly			= src.readOnly;
		dest.dbLocalID			= src.dbLocalID;
		dest.initialModNum		= src.initialModNum;
		dest.curRecIndex		= src.curRecIndex;
	//	dest.creatorList		= src.creatorList;
		dest.syncState			= src.syncState;
		dest.complete			= src.complete;
		dest.conduitOpened		= src.conduitOpened;
		dest.logCleared			= src.logCleared;
		dest.resetPending		= src.resetPending;
		dest.gotCommand			= src.gotCommand;
		dest.cmdTID				= src.cmdTID;
		dest.reserved2			= src.reserved2;
		dest.cmdLen				= src.cmdLen;
		dest.cmdP				= (emuptr) (void *) src.cmdP;
		dest.cmdH				= (emuptr) (MemHandle) src.cmdH;
		dest.wStateFlags		= src.wStateFlags;
	//	dest.dbSearchState		= src.dbSearchState;
	}
}


#pragma mark -

// ---------------------
// ----- DmSearchStateType -----
// ---------------------

void Marshal::GetDmSearchStateType (emuptr p, DmSearchStateType& dest)
{
	memset (&dest, 0, sizeof (DmSearchStateType));

	if (p)
	{
		EmAliasDmSearchStateType<PAS>	src(p);

		for (int ii = 0; ii < 8; ++ii)
		{
			dest.info[ii] = src.info[ii];
		}
	}
}


void Marshal::PutDmSearchStateType (emuptr p, const DmSearchStateType& src)
{
	if (p)
	{
		EmAliasDmSearchStateType<PAS>	dest(p);

		for (int ii = 0; ii < 8; ++ii)
		{
			dest.info[ii] = src.info[ii];
		}
	}
}


#pragma mark -

// ---------------------
// ----- EventType -----
// ---------------------

void Marshal::GetEventType (emuptr p, EventType& dest)
{
	memset (&dest, 0, sizeof (EventType));

	if (p)
	{
		EmAliasEventType<PAS>	src(p);

		dest.eType		= src.eType;
		dest.penDown	= src.penDown;
		dest.screenX	= src.screenX;
		dest.screenY	= src.screenY;

		switch (dest.eType)
		{
			case nilEvent:
				break;

			case penDownEvent:
				dest.data.penUp.start.x			= src.data.penUp.start.x;
				dest.data.penUp.start.y			= src.data.penUp.start.y;
				dest.data.penUp.end.x			= src.data.penUp.end.x;
				dest.data.penUp.end.y			= src.data.penUp.end.y;
				break;

			case penUpEvent:
				dest.data.penUp.start.x			= src.data.penUp.start.x;
				dest.data.penUp.start.y			= src.data.penUp.start.y;
				dest.data.penUp.end.x			= src.data.penUp.end.x;
				dest.data.penUp.end.y			= src.data.penUp.end.y;
				break;

			case penMoveEvent:
				dest.data.penUp.start.x			= src.data.penUp.start.x;
				dest.data.penUp.start.y			= src.data.penUp.start.y;
				dest.data.penUp.end.x			= src.data.penUp.end.x;
				dest.data.penUp.end.y			= src.data.penUp.end.y;
				break;

			case keyDownEvent:
				dest.data.keyDown.chr			= src.data.keyDown.chr;
				dest.data.keyDown.keyCode		= src.data.keyDown.keyCode;
				dest.data.keyDown.modifiers		= src.data.keyDown.modifiers;
				break;

			case winEnterEvent:
				dest.data.winEnter.enterWindow	= (WinHandle) (emuptr) src.data.winEnter.enterWindow;
				dest.data.winEnter.exitWindow	= (WinHandle) (emuptr) src.data.winEnter.exitWindow;
				break;

			case winExitEvent:
				dest.data.winExit.enterWindow	= (WinHandle) (emuptr) src.data.winExit.enterWindow;
				dest.data.winExit.exitWindow	= (WinHandle) (emuptr) src.data.winExit.exitWindow;
				break;

			case ctlEnterEvent:
				dest.data.ctlEnter.controlID	= src.data.ctlEnter.controlID;
				dest.data.ctlEnter.pControl		= (struct ControlType*) (emuptr) src.data.ctlEnter.pControl;
				break;

			case ctlExitEvent:
				dest.data.ctlExit.controlID		= src.data.ctlExit.controlID;
				dest.data.ctlExit.pControl		= (struct ControlType*) (emuptr) src.data.ctlExit.pControl;
				break;

			case ctlSelectEvent:
				dest.data.ctlSelect.controlID	= src.data.ctlSelect.controlID;
				dest.data.ctlSelect.pControl	= (struct ControlType*) (emuptr) src.data.ctlSelect.pControl;
				dest.data.ctlSelect.on			= src.data.ctlSelect.on;
				break;

			case ctlRepeatEvent:
				dest.data.ctlRepeat.controlID	= src.data.ctlRepeat.controlID;
				dest.data.ctlRepeat.pControl	= (struct ControlType*) (emuptr) src.data.ctlRepeat.pControl;
				dest.data.ctlRepeat.time		= src.data.ctlRepeat.time;
				break;

			case lstEnterEvent:
				dest.data.lstEnter.listID		= src.data.lstEnter.listID;
				dest.data.lstEnter.pList		= (struct ListType*) (emuptr) src.data.lstEnter.pList;
				dest.data.lstEnter.selection	= src.data.lstEnter.selection;
				break;

			case lstSelectEvent:
				dest.data.lstSelect.listID		= src.data.lstSelect.listID;
				dest.data.lstSelect.pList		= (struct ListType*) (emuptr) src.data.lstSelect.pList;
				dest.data.lstSelect.selection	= src.data.lstSelect.selection;
				break;

			case lstExitEvent:
				dest.data.lstExit.listID		= src.data.lstExit.listID;
				dest.data.lstExit.pList			= (struct ListType*) (emuptr) src.data.lstExit.pList;
				break;

			case popSelectEvent:
				dest.data.popSelect.controlID			= src.data.popSelect.controlID;
				dest.data.popSelect.controlP			= (struct ControlType*) (emuptr) src.data.popSelect.controlP;
				dest.data.popSelect.listID				= src.data.popSelect.listID;
				dest.data.popSelect.listP				= (struct ListType*) (emuptr) src.data.popSelect.listP;
				dest.data.popSelect.selection			= src.data.popSelect.selection;
				dest.data.popSelect.priorSelection		= src.data.popSelect.priorSelection;
				break;

			case fldEnterEvent:
				dest.data.fldEnter.fieldID				= src.data.fldEnter.fieldID;
				dest.data.fldEnter.pField				= (struct FieldType*) (emuptr) src.data.fldEnter.pField;
				break;

			case fldHeightChangedEvent:
				dest.data.fldHeightChanged.fieldID		= src.data.fldHeightChanged.fieldID;
				dest.data.fldHeightChanged.pField		= (struct FieldType*) (emuptr) src.data.fldHeightChanged.pField;
				dest.data.fldHeightChanged.newHeight	= src.data.fldHeightChanged.newHeight;
				dest.data.fldHeightChanged.currentPos	= src.data.fldHeightChanged.currentPos;
				break;

			case fldChangedEvent:
				dest.data.fldChanged.fieldID	= src.data.fldChanged.fieldID;
				dest.data.fldChanged.pField		= (struct FieldType*) (emuptr) src.data.fldChanged.pField;
				break;

			case tblEnterEvent:
				dest.data.tblEnter.tableID		= src.data.tblEnter.tableID;
				dest.data.tblEnter.pTable		= (struct TableType*) (emuptr) src.data.tblEnter.pTable;
				dest.data.tblEnter.row			= src.data.tblEnter.row;
				dest.data.tblEnter.column		= src.data.tblEnter.column;
				break;

			case tblSelectEvent:
				dest.data.tblEnter.tableID		= src.data.tblEnter.tableID;
				dest.data.tblEnter.pTable		= (struct TableType*) (emuptr) src.data.tblEnter.pTable;
				dest.data.tblEnter.row			= src.data.tblEnter.row;
				dest.data.tblEnter.column		= src.data.tblEnter.column;
				break;

			case daySelectEvent:
				dest.data.daySelect.pSelector	= (struct DaySelectorType*) (emuptr) src.data.daySelect.pSelector;
				dest.data.daySelect.selection	= src.data.daySelect.selection;
				dest.data.daySelect.useThisDate	= src.data.daySelect.useThisDate;
				break;

			case menuEvent:
				dest.data.menu.itemID			= src.data.menu.itemID;
				break;

			case appStopEvent:
				break;

			case frmLoadEvent:
				dest.data.frmLoad.formID		= src.data.frmLoad.formID;
				break;

			case frmOpenEvent:
				dest.data.frmOpen.formID		= src.data.frmOpen.formID;
				break;

			case frmGotoEvent:
				dest.data.frmGoto.formID		= src.data.frmGoto.formID;
				dest.data.frmGoto.recordNum		= src.data.frmGoto.recordNum;
				dest.data.frmGoto.matchPos		= src.data.frmGoto.matchPos;
				dest.data.frmGoto.matchLen		= src.data.frmGoto.matchLen;
				dest.data.frmGoto.matchFieldNum	= src.data.frmGoto.matchFieldNum;
				dest.data.frmGoto.matchCustom	= src.data.frmGoto.matchCustom;
				break;

			case frmUpdateEvent:
				dest.data.frmUpdate.formID		= src.data.frmUpdate.formID;
				dest.data.frmUpdate.updateCode	= src.data.frmUpdate.updateCode;
				break;

			case frmSaveEvent:
				break;

			case frmCloseEvent:
				dest.data.frmClose.formID		= src.data.frmClose.formID;
				break;

			case frmTitleEnterEvent:
				dest.data.frmTitleEnter.formID	= src.data.frmTitleEnter.formID;
				break;

			case frmTitleSelectEvent:
				dest.data.frmTitleSelect.formID	= src.data.frmTitleSelect.formID;
				break;

			case tblExitEvent:
				dest.data.tblExit.tableID		= src.data.tblExit.tableID;
				dest.data.tblExit.pTable		= (struct TableType*) (emuptr) src.data.tblExit.pTable;
				dest.data.tblExit.row			= src.data.tblExit.row;
				dest.data.tblExit.column		= src.data.tblExit.column;
				break;

			case sclEnterEvent:
				dest.data.sclEnter.scrollBarID	= src.data.sclEnter.scrollBarID;
				dest.data.sclEnter.pScrollBar	= (struct ScrollBarType*) (emuptr) src.data.sclEnter.pScrollBar;
				break;

			case sclExitEvent:
				dest.data.sclExit.scrollBarID	= src.data.sclExit.scrollBarID;
				dest.data.sclExit.pScrollBar	= (struct ScrollBarType*) (emuptr) src.data.sclExit.pScrollBar;
				dest.data.sclExit.value			= src.data.sclExit.value;
				dest.data.sclExit.newValue		= src.data.sclExit.newValue;
				break;

			case sclRepeatEvent:
				dest.data.sclRepeat.scrollBarID	= src.data.sclRepeat.scrollBarID;
				dest.data.sclRepeat.pScrollBar	= (struct ScrollBarType*) (emuptr) src.data.sclRepeat.pScrollBar;
				dest.data.sclRepeat.value		= src.data.sclRepeat.value;
				dest.data.sclRepeat.newValue	= src.data.sclRepeat.newValue;
				dest.data.sclRepeat.time		= src.data.sclRepeat.time;
				break;

			case tsmConfirmEvent:
				dest.data.tsmConfirm.yomiText	= (Char*) (emuptr) src.data.tsmConfirm.yomiText;
				dest.data.tsmConfirm.formID		= src.data.tsmConfirm.formID;
				break;

			case tsmFepButtonEvent:
				dest.data.tsmFepButton.buttonID	= src.data.tsmFepButton.buttonID;
				break;

			case tsmFepModeEvent:
				dest.data.tsmFepMode.mode		= src.data.tsmFepMode.mode;
				break;

			case attnIndicatorEnterEvent:
				dest.data.attnIndicatorEnter.formID				= src.data.attnIndicatorEnter.formID;
				break;

			case attnIndicatorSelectEvent:
				dest.data.attnIndicatorSelect.formID			= src.data.attnIndicatorSelect.formID;
				break;

			case menuCmdBarOpenEvent:
				dest.data.menuCmdBarOpen.preventFieldButtons	= src.data.menuCmdBarOpen.preventFieldButtons;
				dest.data.menuCmdBarOpen.reserved				= src.data.menuCmdBarOpen.reserved;
				break;

			case menuOpenEvent:
				dest.data.menuOpen.menuRscID	= src.data.menuOpen.menuRscID;
				dest.data.menuOpen.cause		= src.data.menuOpen.cause;
				break;

			case menuCloseEvent:
				// Doesn't appear to be used.
				break;

			case frmGadgetEnterEvent:
				dest.data.gadgetEnter.gadgetID	= src.data.gadgetEnter.gadgetID;
				dest.data.gadgetEnter.gadgetP	= (struct FormGadgetType*) (emuptr) src.data.gadgetEnter.gadgetP;
				break;

			case frmGadgetMiscEvent:
				dest.data.gadgetMisc.gadgetID	= src.data.gadgetMisc.gadgetID;
				dest.data.gadgetMisc.gadgetP	= (struct FormGadgetType*) (emuptr) src.data.gadgetMisc.gadgetP;
				dest.data.gadgetMisc.selector	= src.data.gadgetMisc.selector;
				dest.data.gadgetMisc.dataP		= (void*) (emuptr) src.data.gadgetMisc.dataP;
				break;

			default:
				if ((dest.eType >= firstINetLibEvent &&
					 dest.eType < firstWebLibEvent + 0x100) ||
					dest.eType >= firstUserEvent)
				{
					// We don't know what's in here, so let's just blockmove it.

					EmMem_memcpy (	(void*) dest.data.generic.datum,
									src.data.generic.datum.GetPtr (),
									16);
				}
				else
				{
					EmAssert (false);
				}
		}
	}
}


void Marshal::PutEventType (emuptr p, const EventType& src)
{
	if (p)
	{
		EmAliasEventType<PAS>	dest(p);

		dest.eType		= src.eType;
		dest.penDown	= src.penDown;
		dest.screenX	= src.screenX;
		dest.screenY	= src.screenY;

		switch (src.eType)
		{
			case nilEvent:
				break;

			case penDownEvent:
				dest.data.penUp.start.x			= src.data.penUp.start.x;
				dest.data.penUp.start.y			= src.data.penUp.start.y;
				dest.data.penUp.end.x			= src.data.penUp.end.x;
				dest.data.penUp.end.y			= src.data.penUp.end.y;
				break;

			case penUpEvent:
				dest.data.penUp.start.x			= src.data.penUp.start.x;
				dest.data.penUp.start.y			= src.data.penUp.start.y;
				dest.data.penUp.end.x			= src.data.penUp.end.x;
				dest.data.penUp.end.y			= src.data.penUp.end.y;
				break;

			case penMoveEvent:
				dest.data.penUp.start.x			= src.data.penUp.start.x;
				dest.data.penUp.start.y			= src.data.penUp.start.y;
				dest.data.penUp.end.x			= src.data.penUp.end.x;
				dest.data.penUp.end.y			= src.data.penUp.end.y;
				break;

			case keyDownEvent:
				dest.data.keyDown.chr			= src.data.keyDown.chr;
				dest.data.keyDown.keyCode		= src.data.keyDown.keyCode;
				dest.data.keyDown.modifiers		= src.data.keyDown.modifiers;
				break;

			case winEnterEvent:
				dest.data.winEnter.enterWindow	= (WinHandle) (emuptr) src.data.winEnter.enterWindow;
				dest.data.winEnter.exitWindow	= (WinHandle) (emuptr) src.data.winEnter.exitWindow;
				break;

			case winExitEvent:
				dest.data.winExit.enterWindow	= (WinHandle) (emuptr) src.data.winExit.enterWindow;
				dest.data.winExit.exitWindow	= (WinHandle) (emuptr) src.data.winExit.exitWindow;
				break;

			case ctlEnterEvent:
				dest.data.ctlEnter.controlID	= src.data.ctlEnter.controlID;
				dest.data.ctlEnter.pControl		= (struct ControlType*) (emuptr) src.data.ctlEnter.pControl;
				break;

			case ctlExitEvent:
				dest.data.ctlExit.controlID		= src.data.ctlExit.controlID;
				dest.data.ctlExit.pControl		= (struct ControlType*) (emuptr) src.data.ctlExit.pControl;
				break;

			case ctlSelectEvent:
				dest.data.ctlSelect.controlID	= src.data.ctlSelect.controlID;
				dest.data.ctlSelect.pControl	= (struct ControlType*) (emuptr) src.data.ctlSelect.pControl;
				dest.data.ctlSelect.on			= src.data.ctlSelect.on;
				break;

			case ctlRepeatEvent:
				dest.data.ctlRepeat.controlID	= src.data.ctlRepeat.controlID;
				dest.data.ctlRepeat.pControl	= (struct ControlType*) (emuptr) src.data.ctlRepeat.pControl;
				dest.data.ctlRepeat.time		= src.data.ctlRepeat.time;
				break;

			case lstEnterEvent:
				dest.data.lstEnter.listID		= src.data.lstEnter.listID;
				dest.data.lstEnter.pList		= (struct ListType*) (emuptr) src.data.lstEnter.pList;
				dest.data.lstEnter.selection	= src.data.lstEnter.selection;
				break;

			case lstSelectEvent:
				dest.data.lstSelect.listID		= src.data.lstSelect.listID;
				dest.data.lstSelect.pList		= (struct ListType*) (emuptr) src.data.lstSelect.pList;
				dest.data.lstSelect.selection	= src.data.lstSelect.selection;
				break;

			case lstExitEvent:
				dest.data.lstExit.listID		= src.data.lstExit.listID;
				dest.data.lstExit.pList			= (struct ListType*) (emuptr) src.data.lstExit.pList;
				break;

			case popSelectEvent:
				dest.data.popSelect.controlID			= src.data.popSelect.controlID;
				dest.data.popSelect.controlP			= (struct ControlType*) (emuptr) src.data.popSelect.controlP;
				dest.data.popSelect.listID				= src.data.popSelect.listID;
				dest.data.popSelect.listP				= (struct ListType*) (emuptr) src.data.popSelect.listP;
				dest.data.popSelect.selection			= src.data.popSelect.selection;
				dest.data.popSelect.priorSelection		= src.data.popSelect.priorSelection;
				break;

			case fldEnterEvent:
				dest.data.fldEnter.fieldID				= src.data.fldEnter.fieldID;
				dest.data.fldEnter.pField				= (struct FieldType*) (emuptr) src.data.fldEnter.pField;
				break;

			case fldHeightChangedEvent:
				dest.data.fldHeightChanged.fieldID		= src.data.fldHeightChanged.fieldID;
				dest.data.fldHeightChanged.pField		= (struct FieldType*) (emuptr) src.data.fldHeightChanged.pField;
				dest.data.fldHeightChanged.newHeight	= src.data.fldHeightChanged.newHeight;
				dest.data.fldHeightChanged.currentPos	= src.data.fldHeightChanged.currentPos;
				break;

			case fldChangedEvent:
				dest.data.fldChanged.fieldID	= src.data.fldChanged.fieldID;
				dest.data.fldChanged.pField		= (struct FieldType*) (emuptr) src.data.fldChanged.pField;
				break;

			case tblEnterEvent:
				dest.data.tblEnter.tableID		= src.data.tblEnter.tableID;
				dest.data.tblEnter.pTable		= (struct TableType*) (emuptr) src.data.tblEnter.pTable;
				dest.data.tblEnter.row			= src.data.tblEnter.row;
				dest.data.tblEnter.column		= src.data.tblEnter.column;
				break;

			case tblSelectEvent:
				dest.data.tblEnter.tableID		= src.data.tblEnter.tableID;
				dest.data.tblEnter.pTable		= (struct TableType*) (emuptr) src.data.tblEnter.pTable;
				dest.data.tblEnter.row			= src.data.tblEnter.row;
				dest.data.tblEnter.column		= src.data.tblEnter.column;
				break;

			case daySelectEvent:
				dest.data.daySelect.pSelector	= (struct DaySelectorType*) (emuptr) src.data.daySelect.pSelector;
				dest.data.daySelect.selection	= src.data.daySelect.selection;
				dest.data.daySelect.useThisDate	= src.data.daySelect.useThisDate;
				break;

			case menuEvent:
				dest.data.menu.itemID			= src.data.menu.itemID;
				break;

			case appStopEvent:
				break;

			case frmLoadEvent:
				dest.data.frmLoad.formID		= src.data.frmLoad.formID;
				break;

			case frmOpenEvent:
				dest.data.frmOpen.formID		= src.data.frmOpen.formID;
				break;

			case frmGotoEvent:
				dest.data.frmGoto.formID		= src.data.frmGoto.formID;
				dest.data.frmGoto.recordNum		= src.data.frmGoto.recordNum;
				dest.data.frmGoto.matchPos		= src.data.frmGoto.matchPos;
				dest.data.frmGoto.matchLen		= src.data.frmGoto.matchLen;
				dest.data.frmGoto.matchFieldNum	= src.data.frmGoto.matchFieldNum;
				dest.data.frmGoto.matchCustom	= src.data.frmGoto.matchCustom;
				break;

			case frmUpdateEvent:
				dest.data.frmUpdate.formID		= src.data.frmUpdate.formID;
				dest.data.frmUpdate.updateCode	= src.data.frmUpdate.updateCode;
				break;

			case frmSaveEvent:
				break;

			case frmCloseEvent:
				dest.data.frmClose.formID		= src.data.frmClose.formID;
				break;

			case frmTitleEnterEvent:
				dest.data.frmTitleEnter.formID	= src.data.frmTitleEnter.formID;
				break;

			case frmTitleSelectEvent:
				dest.data.frmTitleSelect.formID	= src.data.frmTitleSelect.formID;
				break;

			case tblExitEvent:
				dest.data.tblExit.tableID		= src.data.tblExit.tableID;
				dest.data.tblExit.pTable		= (struct TableType*) (emuptr) src.data.tblExit.pTable;
				dest.data.tblExit.row			= src.data.tblExit.row;
				dest.data.tblExit.column		= src.data.tblExit.column;
				break;

			case sclEnterEvent:
				dest.data.sclEnter.scrollBarID	= src.data.sclEnter.scrollBarID;
				dest.data.sclEnter.pScrollBar	= (struct ScrollBarType*) (emuptr) src.data.sclEnter.pScrollBar;
				break;

			case sclExitEvent:
				dest.data.sclExit.scrollBarID	= src.data.sclExit.scrollBarID;
				dest.data.sclExit.pScrollBar	= (struct ScrollBarType*) (emuptr) src.data.sclExit.pScrollBar;
				dest.data.sclExit.value			= src.data.sclExit.value;
				dest.data.sclExit.newValue		= src.data.sclExit.newValue;
				break;

			case sclRepeatEvent:
				dest.data.sclRepeat.scrollBarID	= src.data.sclRepeat.scrollBarID;
				dest.data.sclRepeat.pScrollBar	= (struct ScrollBarType*) (emuptr) src.data.sclRepeat.pScrollBar;
				dest.data.sclRepeat.value		= src.data.sclRepeat.value;
				dest.data.sclRepeat.newValue	= src.data.sclRepeat.newValue;
				dest.data.sclRepeat.time		= src.data.sclRepeat.time;
				break;

			case tsmConfirmEvent:
				dest.data.tsmConfirm.yomiText	= (Char*) (emuptr) src.data.tsmConfirm.yomiText;
				dest.data.tsmConfirm.formID		= src.data.tsmConfirm.formID;
				break;

			case tsmFepButtonEvent:
				dest.data.tsmFepButton.buttonID	= src.data.tsmFepButton.buttonID;
				break;

			case tsmFepModeEvent:
				dest.data.tsmFepMode.mode		= src.data.tsmFepMode.mode;
				break;

			case attnIndicatorEnterEvent:
				dest.data.attnIndicatorEnter.formID				= src.data.attnIndicatorEnter.formID;
				break;

			case attnIndicatorSelectEvent:
				dest.data.attnIndicatorSelect.formID			= src.data.attnIndicatorSelect.formID;
				break;

			case menuCmdBarOpenEvent:
				dest.data.menuCmdBarOpen.preventFieldButtons	= src.data.menuCmdBarOpen.preventFieldButtons;
				dest.data.menuCmdBarOpen.reserved				= src.data.menuCmdBarOpen.reserved;
				break;

			case menuOpenEvent:
				dest.data.menuOpen.menuRscID	= src.data.menuOpen.menuRscID;
				dest.data.menuOpen.cause		= src.data.menuOpen.cause;
				break;

			case menuCloseEvent:
				// Doesn't appear to be used.
				break;

			case frmGadgetEnterEvent:
				dest.data.gadgetEnter.gadgetID	= src.data.gadgetEnter.gadgetID;
				dest.data.gadgetEnter.gadgetP	= (struct FormGadgetType*) (emuptr) src.data.gadgetEnter.gadgetP;
				break;

			case frmGadgetMiscEvent:
				dest.data.gadgetMisc.gadgetID	= src.data.gadgetMisc.gadgetID;
				dest.data.gadgetMisc.gadgetP	= (struct FormGadgetType*) (emuptr) src.data.gadgetMisc.gadgetP;
				dest.data.gadgetMisc.selector	= src.data.gadgetMisc.selector;
				dest.data.gadgetMisc.dataP		= (void*) (emuptr) src.data.gadgetMisc.dataP;
				break;

			default:
				if ((src.eType >= firstINetLibEvent &&
					 src.eType < firstWebLibEvent + 0x100) ||
					src.eType >= firstUserEvent)
				{
					// We don't know what's in here, so let's just blockmove it.

					EmMem_memcpy (	dest.data.generic.datum.GetPtr (),
									(void*) src.data.generic.datum,
									16);
				}
				else
				{
					EmAssert (false);
				}
		}
	}
}


#pragma mark -

// -------------------------------
// ----- FieldAttrType -----
// -------------------------------

/*
	======== BITFIELD LAYOUT CHEAT-SHEET ========

	typedef struct {
		UInt16 usable			:1;
		UInt16 visible		:1;
		UInt16 editable		:1;
		UInt16 singleLine		:1;
		UInt16 hasFocus		:1;
		UInt16 dynamicSize	:1;
		UInt16 insPtVisible	:1;
		UInt16 dirty			:1;
		UInt16 underlined		:2;
		UInt16 justification	:2;
		UInt16 autoShift		:1;
		UInt16 hasScrollBar	:1;
		UInt16 numeric		:1;
	} FieldAttrType;

	// On the Mac:

	|---------- high byte ----------|---------- low byte -----------|

	 15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0  
	  7   6   5   4   3   2   1   0   7   6   5   4   3   2   1   0
	+---+---+---+---+---+---+---+---+-------+-------+---+---+---+---+
	| u | v | e | s | h | d | i | d |   u   |   j   | a | h | n | * |
	+---+---+---+---+---+---+---+---+-------+-------+---+---+---+---+


	// On Windows (in-register representation):

	|---------- high byte ----------|---------- low byte -----------|

	 15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0  
	  7   6   5   4   3   2   1   0   7   6   5   4   3   2   1   0
	+---+---+---+---+-------+-------+---+---+---+---+---+---+---+---+
	| * | n | h | a |   j   |   u   | d | i | d | h | s | e | v | u |
	+---+---+---+---+-------+-------+---+---+---+---+---+---+---+---+
*/

#define FieldAttrType_Mask_usable			0x0001
#define FieldAttrType_Mask_visible			0x0001
#define FieldAttrType_Mask_editable			0x0001
#define FieldAttrType_Mask_singleLine		0x0001
#define FieldAttrType_Mask_hasFocus			0x0001
#define FieldAttrType_Mask_dynamicSize		0x0001
#define FieldAttrType_Mask_insPtVisible		0x0001
#define FieldAttrType_Mask_dirty			0x0001
#define FieldAttrType_Mask_underlined		0x0003
#define FieldAttrType_Mask_justification	0x0003
#define FieldAttrType_Mask_autoShift		0x0001
#define FieldAttrType_Mask_hasScrollBar		0x0001
#define FieldAttrType_Mask_numeric			0x0001

#define FieldAttrType_Shift_usable			15
#define FieldAttrType_Shift_visible			14
#define FieldAttrType_Shift_editable		13
#define FieldAttrType_Shift_singleLine		12
#define FieldAttrType_Shift_hasFocus		11
#define FieldAttrType_Shift_dynamicSize		10
#define FieldAttrType_Shift_insPtVisible	9
#define FieldAttrType_Shift_dirty			8
#define FieldAttrType_Shift_underlined		7
#define FieldAttrType_Shift_justification	5
#define FieldAttrType_Shift_autoShift		3
#define FieldAttrType_Shift_hasScrollBar	2
#define FieldAttrType_Shift_numeric			1

void Marshal::GetFieldAttrType (emuptr p, FieldAttrType& dest)
{
	memset (&dest, 0, sizeof (FieldAttrType));

	if (p)
	{
		EmAliasUInt16<PAS>	src(p);

		UInt16	temp = src;

#undef COPY_FIELD
#define COPY_FIELD(name)	\
		dest.name = (temp >> FieldAttrType_Shift_##name) & FieldAttrType_Mask_##name

		COPY_FIELD (usable);
		COPY_FIELD (visible);
		COPY_FIELD (editable);
		COPY_FIELD (singleLine);
		COPY_FIELD (hasFocus);
		COPY_FIELD (dynamicSize);
		COPY_FIELD (insPtVisible);
		COPY_FIELD (dirty);
		COPY_FIELD (underlined);
		COPY_FIELD (justification);
		COPY_FIELD (autoShift);
		COPY_FIELD (hasScrollBar);
		COPY_FIELD (numeric);
	}
}


void Marshal::PutFieldAttrType (emuptr p, const FieldAttrType& src)
{
	if (p)
	{
		EmAliasUInt16<PAS>	dest(p);

		UInt16	temp = 0;

#undef COPY_FIELD
#define COPY_FIELD(name)	\
		temp |= (src.name << FieldAttrType_Shift_##name)

		COPY_FIELD (usable);
		COPY_FIELD (visible);
		COPY_FIELD (editable);
		COPY_FIELD (singleLine);
		COPY_FIELD (hasFocus);
		COPY_FIELD (dynamicSize);
		COPY_FIELD (insPtVisible);
		COPY_FIELD (dirty);
		COPY_FIELD (underlined);
		COPY_FIELD (justification);
		COPY_FIELD (autoShift);
		COPY_FIELD (hasScrollBar);
		COPY_FIELD (numeric);

		dest = temp;
	}
}


#pragma mark -

// -------------------------------
// ----- HostGremlinInfoType -----
// -------------------------------

void Marshal::GetHostGremlinInfoType (emuptr p, HostGremlinInfoType& dest)
{
	memset (&dest, 0, sizeof (HostGremlinInfoType));

	if (p)
	{
		EmAliasHostGremlinInfoType<PAS>	src(p);

		dest.fFirstGremlin	= src.fFirstGremlin;
		dest.fLastGremlin	= src.fLastGremlin;
		dest.fSaveFrequency	= src.fSaveFrequency;
		dest.fSwitchDepth	= src.fSwitchDepth;
		dest.fMaxDepth		= src.fMaxDepth;

		EmMem_strcpy (dest.fAppNames, src.fAppNames.GetPtr ());
	}
}


void Marshal::PutHostGremlinInfoType (emuptr p, const HostGremlinInfoType& src)
{
	if (p)
	{
		EmAliasHostGremlinInfoType<PAS>	dest(p);

		dest.fFirstGremlin	= src.fFirstGremlin;
		dest.fLastGremlin	= src.fLastGremlin;
		dest.fSaveFrequency	= src.fSaveFrequency;
		dest.fSwitchDepth	= src.fSwitchDepth;
		dest.fMaxDepth		= src.fMaxDepth;

		EmMem_strcpy (dest.fAppNames.GetPtr (), src.fAppNames);
	}
}


#pragma mark -

// ------------------------
// ----- HostStatType -----
// ------------------------

void Marshal::GetHostStatType (emuptr p, HostStatType& dest)
{
	memset (&dest, 0, sizeof (HostStatType));

	if (p)
	{
		EmAliasHostStatType<PAS>	src(p);

		dest.st_dev_		= src.st_dev_;
		dest.st_ino_		= src.st_ino_;
		dest.st_mode_		= src.st_mode_;
		dest.st_nlink_		= src.st_nlink_;
		dest.st_uid_		= src.st_uid_;
		dest.st_gid_		= src.st_gid_;
		dest.st_rdev_		= src.st_rdev_;
		dest.st_atime_		= src.st_atime_;
		dest.st_mtime_		= src.st_mtime_;
		dest.st_ctime_		= src.st_ctime_;
		dest.st_size_		= src.st_size_;
		dest.st_blksize_	= src.st_blksize_;
		dest.st_blocks_		= src.st_blocks_;
		dest.st_flags_		= src.st_flags_;
	}
}


void Marshal::PutHostStatType (emuptr p, const HostStatType& src)
{
	if (p)
	{
		EmAliasHostStatType<PAS>	dest(p);

		dest.st_dev_		= src.st_dev_;
		dest.st_ino_		= src.st_ino_;
		dest.st_mode_		= src.st_mode_;
		dest.st_nlink_		= src.st_nlink_;
		dest.st_uid_		= src.st_uid_;
		dest.st_gid_		= src.st_gid_;
		dest.st_rdev_		= src.st_rdev_;
		dest.st_atime_		= src.st_atime_;
		dest.st_mtime_		= src.st_mtime_;
		dest.st_ctime_		= src.st_ctime_;
		dest.st_size_		= src.st_size_;
		dest.st_blksize_	= src.st_blksize_;
		dest.st_blocks_		= src.st_blocks_;
		dest.st_flags_		= src.st_flags_;
	}
}


#pragma mark -

// ----------------------
// ----- HostTmType -----
// ----------------------

void Marshal::GetHostTmType (emuptr p, HostTmType& dest)
{
	memset (&dest, 0, sizeof (HostTmType));

	if (p)
	{
		EmAliasHostTmType<PAS>	src(p);

		dest.tm_sec_		= src.tm_sec_;
		dest.tm_min_		= src.tm_min_;
		dest.tm_hour_		= src.tm_hour_;
		dest.tm_mday_		= src.tm_mday_;
		dest.tm_mon_		= src.tm_mon_;
		dest.tm_year_		= src.tm_year_;
		dest.tm_wday_		= src.tm_wday_;
		dest.tm_yday_		= src.tm_yday_;
		dest.tm_isdst_		= src.tm_isdst_;
	}
}


void Marshal::PutHostTmType (emuptr p, const HostTmType& src)
{
	if (p)
	{
		EmAliasHostTmType<PAS>	dest(p);

		dest.tm_sec_		= src.tm_sec_;
		dest.tm_min_		= src.tm_min_;
		dest.tm_hour_		= src.tm_hour_;
		dest.tm_mday_		= src.tm_mday_;
		dest.tm_mon_		= src.tm_mon_;
		dest.tm_year_		= src.tm_year_;
		dest.tm_wday_		= src.tm_wday_;
		dest.tm_yday_		= src.tm_yday_;
		dest.tm_isdst_		= src.tm_isdst_;
	}
}


#pragma mark -

// -------------------------
// ----- HostUTimeType -----
// -------------------------

void Marshal::GetHostUTimeType (emuptr p, HostUTimeType& dest)
{
	memset (&dest, 0, sizeof (HostUTimeType));

	if (p)
	{
		EmAliasHostUTimeType<PAS>	src(p);

		dest.crtime_		= src.crtime_;
		dest.actime_		= src.actime_;
		dest.modtime_		= src.modtime_;
	}
}


void Marshal::PutHostUTimeType (emuptr p, const HostUTimeType& src)
{
	if (p)
	{
		EmAliasHostUTimeType<PAS>	dest(p);

		dest.crtime_		= src.crtime_;
		dest.actime_		= src.actime_;
		dest.modtime_		= src.modtime_;
	}
}


#pragma mark -

// -------------------------------
// ----- HwrBatCmdReadType -----
// -------------------------------

void Marshal::GetHwrBatCmdReadType (emuptr p, HwrBatCmdReadType& dest)
{
	memset (&dest, 0, sizeof (HwrBatCmdReadType));

	if (p)
	{
		EmAliasHwrBatCmdReadType<PAS>	src(p);

		dest.mVolts	= src.mVolts;
		dest.abs	= src.abs;
	}
}


void Marshal::PutHwrBatCmdReadType (emuptr p, const HwrBatCmdReadType& src)
{
	if (p)
	{
		EmAliasHwrBatCmdReadType<PAS>	dest(p);

		dest.mVolts	= src.mVolts;
		dest.abs	= src.abs;
	}
}


#pragma mark -

// -----------------------------
// ----- NetSocketAddrType -----
// -----------------------------

void Marshal::GetNetSocketAddrType (emuptr p, NetSocketAddrType& d)
{
	memset (&d, 0, sizeof (NetSocketAddrType));

	if (p)
	{
		EmAliasNetSocketAddrType<PAS>	sockAddr (p);

		switch (sockAddr.family)
		{
			case netSocketAddrRaw:
			{
				EmAliasNetSocketAddrRawType<PAS>	src (p);
				NetSocketAddrRawType&				dest = (NetSocketAddrRawType&) d;

				dest.family		= src.family;		// In HBO
				dest.ifInstance	= src.ifInstance;	// Unspecified order
				dest.ifCreator	= src.ifCreator;	// Unspecified order

				break;
			}

			case netSocketAddrINET:
			{
				EmAliasNetSocketAddrINType<PAS>		src (p);
				NetSocketAddrINType&				dest = (NetSocketAddrINType&) d;

				dest.family		= src.family;		// In HBO
				dest.port		= htons (NetNToHS ((UInt16) src.port));	// In NBO
				dest.addr		= htonl (NetNToHL ((UInt32) src.addr));	// In NBO

				break;
			}

			default:
			{
				// Do the best we can...
				EmAliasNetSocketAddrType<PAS>		src (p);
				NetSocketAddrType&					dest = (NetSocketAddrType&) d;

				dest.family		= src.family;		// In HBO

				EmMem_memcpy ((void*) &dest.data[0], src.data.GetPtr (), 14);
				break;
			}
		}
	}
}


void Marshal::PutNetSocketAddrType (emuptr p, const NetSocketAddrType& s)
{
	if (p)
	{
		switch (s.family)
		{
			case netSocketAddrRaw:
			{
				EmAliasNetSocketAddrRawType<PAS>	dest (p);
				NetSocketAddrRawType&				src = (NetSocketAddrRawType&) s;

				dest.family		= src.family;		// In HBO
				dest.ifInstance	= src.ifInstance;	// Unspecified order
				dest.ifCreator	= src.ifCreator;	// Unspecified order

				break;
			}

			case netSocketAddrINET:
			{
				EmAliasNetSocketAddrINType<PAS>		dest (p);
				NetSocketAddrINType&				src = (NetSocketAddrINType&) s;

				dest.family		= src.family;	// In HBO
				dest.port		= NetHToNS (ntohs (src.port));	// In NBO
				dest.addr		= NetHToNL (ntohl (src.addr));	// In NBO

				break;
			}

			default:
			{
				// Do the best we can...
				EmAliasNetSocketAddrType<PAS>		dest (p);
				NetSocketAddrType&					src = (NetSocketAddrType&) s;

				dest.family		= src.family;	// In HBO

				EmMem_memcpy (dest.data.GetPtr (), (void*) &src.data[0], 14);
				break;
			}
		}
	}
}


#pragma mark -

// --------------------------
// ----- NetIOParamType -----
// --------------------------

void Marshal::GetNetIOParamType (emuptr p, NetIOParamType& dest)
{
	memset (&dest, 0, sizeof (NetIOParamType));

	if (p)
	{
		EmAliasNetIOParamType<PAS>	src (p);

		// Copy the simple fields.

		dest.addrLen			= src.addrLen;
		dest.iovLen				= src.iovLen;
		dest.accessRightsLen	= src.accessRightsLen;

		// Copy the address field.

		dest.addrP				= (UInt8*) GetBuffer (src.addrP, src.addrLen);
		if (dest.addrP)
		{
			// The "family" field needs to be stored in HBO, not NBO.
			// The following sequence will assure that.

			EmAliasNetSocketAddrType<PAS>	addr (src.addrP);

			((NetSocketAddrType*) (dest.addrP))->family = addr.family;
		}

		// Copy the access rights field.

		dest.accessRights		= (UInt8*) GetBuffer (src.accessRights, dest.accessRightsLen);

		// Copy the i/o buffers.

		dest.iov				= (NetIOVecPtr) Platform::AllocateMemory (dest.iovLen * sizeof (NetIOVecType));
		emuptr iovP				= src.iov;

		for (UInt16 ii = 0; ii < dest.iovLen; ++ii)
		{
			EmAliasNetIOVecType<PAS>	iov (iovP);

			dest.iov[ii].bufLen	= iov.bufLen;
			dest.iov[ii].bufP	= (UInt8*) GetBuffer (iov.bufP, iov.bufLen);

			iovP += iov.GetSize ();
		}
	}
}


void Marshal::PutNetIOParamType (emuptr p, const NetIOParamType& src)
{
	if (p)
	{
		EmAliasNetIOParamType<PAS>	dest (p);

		// Copy the simple fields.

		dest.addrLen			= src.addrLen;
		dest.iovLen				= src.iovLen;
		dest.accessRightsLen	= src.accessRightsLen;

		// Copy the address field.

		PutBuffer (dest.addrP, src.addrP, src.addrLen);

		if (src.addrP)
		{
			// The "family" field needs to be stored in HBO, not NBO.
			// The following sequence will assure that.

			EmAliasNetSocketAddrType<PAS>	addr (dest.addrP);

			addr.family = ((NetSocketAddrType*) (src.addrP))->family;
		}

		// Copy the access rights field.

		PutBuffer (dest.accessRights, src.accessRights,	src.accessRightsLen);

		// Copy the i/o buffers.

		emuptr iovP	= dest.iov;

		for (UInt16 ii = 0; ii < src.iovLen; ++ii)
		{
			EmAliasNetIOVecType<PAS>	iov (iovP);

			iov.bufLen = src.iov[ii].bufLen;
			PutBuffer (iov.bufP, src.iov[ii].bufP, src.iov[ii].bufLen);

			iovP += iov.GetSize ();
		}

		void*	buffer = src.iov;
		Platform::DisposeMemory (buffer);
	}
}


#pragma mark -

// ------------------------------
// ----- NetHostInfoBufType -----
// ------------------------------

/*
	typedef struct
	{
		NetHostInfoType	hostInfo;
		{
			Char*			nameP; -------------------------+
			Char**			nameAliasesP;-------------------|---+
			UInt16			addrType;                       |   |
			UInt16			addrLen;                        |   |
			UInt8**			addrListP;----------------------|---|---+
		}                                                   |   |   |
											                |   |   |
		Char			name[netDNSMaxDomainName+1];   <----+   |   |
	                                                            |   |
		Char*			aliasList[netDNSMaxAliases+1];   <------+   |
		Char			aliases[netDNSMaxAliases][netDNSMaxDomainName+1];
	                                                                |
		NetIPAddr*		addressList[netDNSMaxAddresses];   <--------+
		NetIPAddr		address[netDNSMaxAddresses];

	} NetHostInfoBufType, *NetHostInfoBufPtr;
*/

void Marshal::GetNetHostInfoBufType (emuptr p, NetHostInfoBufType& netHostInfoBufType)
{
	memset (&netHostInfoBufType, 0, sizeof (NetHostInfoBufType));

	if (p)
	{
	}
}


void Marshal::PutNetHostInfoBufType (emuptr p, const NetHostInfoBufType& src)
{
	if (p)
	{
		EmAliasNetHostInfoBufType<PAS>	dest (p);

		// Copy the host name.

		dest.hostInfo.nameP = dest.name.GetPtr ();
		EmMem_strcpy ((emuptr) dest.hostInfo.nameP, src.name);

		// Copy the aliases.

		dest.hostInfo.nameAliasesP	= dest.aliasList.GetPtr ();

		Char**	srcNameAliasesP		= src.hostInfo.nameAliasesP;	// Ptr to src ptrs
		emuptr	destAliasList		= dest.aliasList.GetPtr ();		// Ptr to dest ptrs
		emuptr	destAliases			= dest.aliases.GetPtr ();		// Ptr to dest buffer

		while (*srcNameAliasesP)
		{
			EmMem_strcpy (destAliases, *srcNameAliasesP);

			EmAliasemuptr<PAS>	p (destAliasList);
			p = destAliases;

			destAliasList += sizeof (emuptr);
			destAliases += strlen (*srcNameAliasesP) + 1;
			srcNameAliasesP += 1;
		}

		EmAliasemuptr<PAS>	p1 (destAliasList);
		p1 = EmMemNULL;

		// Copy the easy fields.

		dest.hostInfo.addrType		= src.hostInfo.addrType;
		dest.hostInfo.addrLen		= src.hostInfo.addrLen;

		// Copy the address list.

		dest.hostInfo.addrListP	= dest.addressList.GetPtr ();

		NetIPAddr**	addrListP		= (NetIPAddr**) src.hostInfo.addrListP;
		emuptr		destAddressList	= dest.addressList.GetPtr ();
		emuptr		destAddress		= dest.address.GetPtr ();

		while (*addrListP)
		{
			// Copy them one at a time to sort out endian issues.  Just how this
			// is supposed to be done is not clear (NetLib documentation says that
			// the addresses are in HBO, while the WinSock header says that the
			// addresses are supplied in NBO but returned in HBO), but the following
			// seems to work.

			EmAliasNetIPAddr<PAS>	addr (destAddress);
			addr = ntohl (**addrListP);

			EmAliasemuptr<PAS>	p (destAddressList);
			p = destAddress;

			destAddressList += sizeof (emuptr);
			destAddress += sizeof (NetIPAddr);
			addrListP += 1;
		}

		EmAliasemuptr<PAS>	p2 (destAddressList);
		p2 = EmMemNULL;
	}
}


#pragma mark -

// ------------------------------
// ----- NetServInfoBufType -----
// ------------------------------

void Marshal::GetNetServInfoBufType (emuptr p, NetServInfoBufType& netServInfoBuf)
{
	memset (&netServInfoBuf, 0, sizeof (NetServInfoBufType));

	if (p)
	{
	}
}


void Marshal::PutNetServInfoBufType (emuptr p, const NetServInfoBufType& src)
{
	if (p)
	{
		EmAliasNetServInfoBufType<PAS>	dest (p);

		// Copy the server name.

		dest.servInfo.nameP = dest.name.GetPtr ();
		EmMem_strcpy ((emuptr) dest.servInfo.nameP, src.name);

		// Copy the aliases.

		dest.servInfo.nameAliasesP	= dest.aliasList.GetPtr ();

		Char**	srcNameAliasesP		= src.servInfo.nameAliasesP;	// Ptr to src ptrs
		emuptr	destAliasList		= dest.aliasList.GetPtr ();		// Ptr to dest ptrs
		emuptr	destAliases			= dest.aliases.GetPtr ();		// Ptr to dest buffer

		while (*srcNameAliasesP)
		{
			EmMem_strcpy (destAliases, *srcNameAliasesP);

			EmAliasemuptr<PAS>	p (destAliasList);
			p = destAliases;

			destAliasList += sizeof (emuptr);
			destAliases += strlen (*srcNameAliasesP) + 1;
			srcNameAliasesP += 1;
		}

		EmAliasemuptr<PAS>	p1 (destAliasList);
		p1 = EmMemNULL;

		// Copy the port.

		dest.servInfo.port		= NetHToNS (ntohs (src.servInfo.port));	// In NBO

		// Copy the proto name.

		dest.servInfo.protoP = dest.protoName.GetPtr ();
		EmMem_strcpy ((emuptr) dest.servInfo.protoP, src.protoName);
	}
}


#pragma mark -

// -------------------------------
// ----- PointType -----
// -------------------------------

void Marshal::GetPointType (emuptr p, PointType& dest)
{
	memset (&dest, 0, sizeof (PointType));

	if (p)
	{
		EmAliasPointType<PAS>	src(p);

		dest.x	= src.x;
		dest.y	= src.y;
	}
}


void Marshal::PutPointType (emuptr p, const PointType& src)
{
	if (p)
	{
		EmAliasPointType<PAS>	dest(p);

		dest.x	= src.x;
		dest.y	= src.y;
	}
}


#pragma mark -

// -------------------------------
// ----- RectangleType -----
// -------------------------------

void Marshal::GetRectangleType (emuptr p, RectangleType& dest)
{
	memset (&dest, 0, sizeof (RectangleType));

	if (p)
	{
		EmAliasRectangleType<PAS>	src(p);

		dest.topLeft.x	= src.topLeft.x;
		dest.topLeft.y	= src.topLeft.y;
		dest.extent.x	= src.extent.x;
		dest.extent.y	= src.extent.y;
	}
}


void Marshal::PutRectangleType (emuptr p, const RectangleType& src)
{
	if (p)
	{
		EmAliasRectangleType<PAS>	dest(p);

		dest.topLeft.x	= src.topLeft.x;
		dest.topLeft.y	= src.topLeft.y;
		dest.extent.x	= src.extent.x;
		dest.extent.y	= src.extent.y;
	}
}


#pragma mark -

// -------------------------------
// ----- SndCommandType -----
// -------------------------------

void Marshal::GetSndCommandType (emuptr p, SndCommandType& dest)
{
	memset (&dest, 0, sizeof (SndCommandType));

	if (p)
	{
		EmAliasSndCommandType<PAS>	src(p);

		dest.cmd		= src.cmd;
		dest.reserved	= src.reserved;
		dest.param1		= src.param1;
		dest.param2		= src.param2;
		dest.param3		= src.param3;
	}
}


void Marshal::PutSndCommandType (emuptr p, const SndCommandType& src)
{
	if (p)
	{
		EmAliasSndCommandType<PAS>	dest(p);

		dest.cmd		= src.cmd;
		dest.reserved	= src.reserved;
		dest.param1		= src.param1;
		dest.param2		= src.param2;
		dest.param3		= src.param3;
	}
}


#pragma mark -

// -------------------------------
// ----- SysAppInfoType -----
// -------------------------------

void Marshal::GetSysAppInfoType (emuptr p, SysAppInfoType& dest)
{
	memset (&dest, 0, sizeof (SysAppInfoType));

	if (p)
	{
		EmAliasSysAppInfoType<PAS>	src(p);

		dest.cmd			= src.cmd;
		dest.cmdPBP			= (MemPtr) (emuptr) src.cmdPBP;
		dest.launchFlags	= src.launchFlags;
		dest.taskID			= src.taskID;
		dest.codeH			= (MemHandle) (emuptr) src.codeH;
		dest.dbP			= (DmOpenRef) (emuptr) src.dbP;
		dest.stackP			= (UInt8*) (emuptr) src.stackP;
		dest.globalsChunkP	= (UInt8*) (emuptr) src.globalsChunkP;
		dest.memOwnerID		= src.memOwnerID;
		dest.dmAccessP		= (MemPtr) (emuptr) src.dmAccessP;
		dest.dmLastErr		= src.dmLastErr;
		dest.errExceptionP	= (MemPtr) (emuptr) src.errExceptionP;

		// PalmOS v3.0 fields begin here

		if (EmPatchState::OSMajorVersion () >= 3)
		{
			dest.a5Ptr			= (UInt8*) (emuptr) src.a5Ptr;
			dest.stackEndP		= (UInt8*) (emuptr) src.stackEndP;
			dest.globalEndP		= (UInt8*) (emuptr) src.globalEndP;
			dest.rootP			= (struct SysAppInfoType*) (emuptr) src.rootP;
			dest.extraP			= (MemPtr) (emuptr) src.extraP;
		}
	}
}


void Marshal::PutSysAppInfoType (emuptr p, const SysAppInfoType& src)
{
	if (p)
	{
		EmAliasSysAppInfoType<PAS>	dest(p);

		dest.cmd			= src.cmd;
		dest.cmdPBP			= (emuptr) src.cmdPBP;
		dest.launchFlags	= src.launchFlags;
		dest.taskID			= src.taskID;
		dest.codeH			= (emuptr) src.codeH;
		dest.dbP			= (emuptr) src.dbP;
		dest.stackP			= (emuptr) src.stackP;
		dest.globalsChunkP	= (emuptr) src.globalsChunkP;
		dest.memOwnerID		= src.memOwnerID;
		dest.dmAccessP		= (emuptr) src.dmAccessP;
		dest.dmLastErr		= src.dmLastErr;
		dest.errExceptionP	= (emuptr) src.errExceptionP;

		// PalmOS v3.0 fields begin here

		if (EmPatchState::OSMajorVersion () >= 3)
		{
			dest.a5Ptr			= (emuptr) src.a5Ptr;
			dest.stackEndP		= (emuptr) src.stackEndP;
			dest.globalEndP		= (emuptr) src.globalEndP;
			dest.rootP			= (emuptr) src.rootP;
			dest.extraP			= (emuptr) src.extraP;
		}
	}
}


#pragma mark -

// -------------------------------
// ----- SysNVParamsType -----
// -------------------------------

void Marshal::GetSysNVParamsType (emuptr p, SysNVParamsType& dest)
{
	memset (&dest, 0, sizeof (SysNVParamsType));

	if (p)
	{
		EmAliasSysNVParamsType<PAS>	src(p);

		dest.rtcHours				= src.rtcHours;
		dest.rtcHourMinSecCopy		= src.rtcHourMinSecCopy;
		dest.swrLCDContrastValue	= src.swrLCDContrastValue;
		dest.swrLCDBrightnessValue	= src.swrLCDBrightnessValue;
		dest.splashScreenPtr		= (void*) (emuptr) src.splashScreenPtr;
		dest.hardResetScreenPtr		= (void*) (emuptr) src.hardResetScreenPtr;
		dest.localeLanguage			= src.localeLanguage;
		dest.localeCountry			= src.localeCountry;
		dest.sysNVOEMStorage1		= src.sysNVOEMStorage1;
		dest.sysNVOEMStorage2		= src.sysNVOEMStorage2;
	}
}


void Marshal::PutSysNVParamsType (emuptr p, const SysNVParamsType& src)
{
	if (p)
	{
		EmAliasSysNVParamsType<PAS>	dest(p);

		dest.rtcHours				= src.rtcHours;
		dest.rtcHourMinSecCopy		= src.rtcHourMinSecCopy;
		dest.swrLCDContrastValue	= src.swrLCDContrastValue;
		dest.swrLCDBrightnessValue	= src.swrLCDBrightnessValue;
		dest.splashScreenPtr		= (emuptr) src.splashScreenPtr;
		dest.hardResetScreenPtr		= (emuptr) src.hardResetScreenPtr;
		dest.localeLanguage			= src.localeLanguage;
		dest.localeCountry			= src.localeCountry;
		dest.sysNVOEMStorage1		= src.sysNVOEMStorage1;
		dest.sysNVOEMStorage2		= src.sysNVOEMStorage2;
	}
}


#pragma mark -

// -------------------------------
// ----- SysKernelInfoType -----
// -------------------------------

void Marshal::GetSysKernelInfoType (emuptr p, SysKernelInfoType& dest)
{
	memset (&dest, 0, sizeof (SysKernelInfoType));

	if (p)
	{
		EmAliasSysKernelInfoType<PAS>	src(p);

		dest.selector				= src.selector;
		dest.reserved				= src.reserved;
		dest.id						= src.id;

		switch (dest.selector)
		{
			case sysKernelInfoSelCurTaskInfo:
			case sysKernelInfoSelTaskInfo:
				dest.param.task.id				= src.task.id;
				dest.param.task.nextID			= src.task.nextID;
				dest.param.task.tag				= src.task.tag;
				dest.param.task.status			= src.task.status;
				dest.param.task.timer			= src.task.timer;
				dest.param.task.timeSlice		= src.task.timeSlice;
				dest.param.task.priority		= src.task.priority;
				dest.param.task.attributes		= src.task.attributes;
				dest.param.task.pendingCalls	= src.task.pendingCalls;
				dest.param.task.senderTaskID	= src.task.senderTaskID;
				dest.param.task.msgExchangeID	= src.task.msgExchangeID;
				dest.param.task.tcbP			= src.task.tcbP;
				dest.param.task.stackP			= src.task.stackP;
				dest.param.task.stackStart		= src.task.stackStart;
				dest.param.task.stackSize		= src.task.stackSize;
				break;

			case sysKernelInfoSelSemaphoreInfo:
				dest.param.semaphore.id			= src.semaphore.id;
				dest.param.semaphore.nextID		= src.semaphore.nextID;
				dest.param.semaphore.tag		= src.semaphore.tag;
				dest.param.semaphore.initValue	= src.semaphore.initValue;
				dest.param.semaphore.curValue	= src.semaphore.curValue;
				dest.param.semaphore.nestLevel	= src.semaphore.nestLevel;
				dest.param.semaphore.ownerID	= src.semaphore.ownerID;
				break;

			case sysKernelInfoSelTimerInfo:
				dest.param.timer.id				= src.timer.id;
				dest.param.timer.nextID			= src.timer.nextID;
				dest.param.timer.tag			= src.timer.tag;
				dest.param.timer.ticksLeft		= src.timer.ticksLeft;
				dest.param.timer.period			= src.timer.period;
				dest.param.timer.proc			= src.timer.proc;
				break;

			default:
				EmAssert (false);
		}
	}
}


void Marshal::PutSysKernelInfoType (emuptr p, const SysKernelInfoType& src)
{
	if (p)
	{
		EmAliasSysKernelInfoType<PAS>	dest(p);

		dest.selector				= src.selector;
		dest.reserved				= src.reserved;
		dest.id						= src.id;

		switch (dest.selector)
		{
			case sysKernelInfoSelCurTaskInfo:
			case sysKernelInfoSelTaskInfo:
				dest.task.id				= src.param.task.id;
				dest.task.nextID			= src.param.task.nextID;
				dest.task.tag				= src.param.task.tag;
				dest.task.status			= src.param.task.status;
				dest.task.timer				= src.param.task.timer;
				dest.task.timeSlice			= src.param.task.timeSlice;
				dest.task.priority			= src.param.task.priority;
				dest.task.attributes		= src.param.task.attributes;
				dest.task.pendingCalls		= src.param.task.pendingCalls;
				dest.task.senderTaskID		= src.param.task.senderTaskID;
				dest.task.msgExchangeID		= src.param.task.msgExchangeID;
				dest.task.tcbP				= src.param.task.tcbP;
				dest.task.stackP			= src.param.task.stackP;
				dest.task.stackStart		= src.param.task.stackStart;
				dest.task.stackSize			= src.param.task.stackSize;
				break;

			case sysKernelInfoSelSemaphoreInfo:
				dest.semaphore.id			= src.param.semaphore.id;
				dest.semaphore.nextID		= src.param.semaphore.nextID;
				dest.semaphore.tag			= src.param.semaphore.tag;
				dest.semaphore.initValue	= src.param.semaphore.initValue;
				dest.semaphore.curValue		= src.param.semaphore.curValue;
				dest.semaphore.nestLevel	= src.param.semaphore.nestLevel;
				dest.semaphore.ownerID		= src.param.semaphore.ownerID;
				break;

			case sysKernelInfoSelTimerInfo:
				dest.timer.id				= src.param.timer.id;
				dest.timer.nextID			= src.param.timer.nextID;
				dest.timer.tag				= src.param.timer.tag;
				dest.timer.ticksLeft		= src.param.timer.ticksLeft;
				dest.timer.period			= src.param.timer.period;
				dest.timer.proc				= src.param.timer.proc;
				break;

			default:
				EmAssert (false);
		}
	}
}
