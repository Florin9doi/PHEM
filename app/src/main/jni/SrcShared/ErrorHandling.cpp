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
#include "ErrorHandling.h"

#include "DebugMgr.h"			// Debug::EnterDebugger
#include "EmApplication.h"		// ScheduleQuit
#include "EmBankDRAM.h"			// EmBankDRAM::ValidAddress
#include "EmBankSRAM.h"			// EmBankSRAM::ValidAddress
#include "EmCPU.h"				// gCPU
#include "EmCPU68K.h"			// gCPU68K, kException_SoftBreak
#include "EmDlg.h"				// kDebugReset, kErrorAlert, kContinueDebugReset, kCautionAlert
#include "EmErrCodes.h"			// kError_OutOfMemory, ConvertFromStdCError
#include "EmEventOutput.h"		// ErrorOccurred
#include "EmEventPlayback.h"	// RecordErrorEvent
#include "EmException.h"		// EmExceptionEnterDebugger
#include "EmMinimize.h"			// EmMinimize::IsOn, NextMinimizer
#include "EmPalmFunction.h"		// gLibErrorBase
#include "EmPalmOS.h"			// GenerateStackCrawl
#include "EmPalmStructs.h"		// EmAliasWindowType
#include "EmPatchState.h"		// EmPatchState::GetCurrentAppInfo
#include "EmSession.h"			// gSession
#include "Hordes.h"				// Hordes::IsOn, RecordErrorStats
#include "Logging.h"			// LogWrite, ReportFoo functions
#include "MetaMemory.h"			// MetaMemory
#include "Miscellaneous.h"		// LaunchCmdToString, SystemCallContext, ReplaceString
#include "Platform.h"			// GetString, CommonDialog
#include "PreferenceMgr.h"		// Preference
#include "Strings.r.h"			// kStr_ values


// ===========================================================================
//		¥ Errors
// ===========================================================================

#define FIELDS(type, name, fns)	\
	{			\
		0,		\
		EmAlias##type<PAS>::offsetof_##name (),	\
		#name,	\
		fns,	\
		NULL	\
	}

#define SUB_FIELDS(type, name, sub_table)	\
	{						\
		1,					\
		EmAlias##type<PAS>::offsetof_##name (),	\
		#name,				\
		NULL,				\
		sub_table			\
	}

#define END_OF_FIELDS	{2, 0, NULL, NULL, NULL }


struct EmFieldLookup
{
	int						fType;			// 0 == FIELDS, 1 == SUB_FIELDS
	size_t					fOffset;		// Offset of the field
	const char*				fFieldName;		// Name of the field

	// Used for FIELDS.
	const char*				fFunction;		// Name(s) of accessor function(s)

	// Used for SUB_FIELDS.
	const EmFieldLookup*	fNextTable;		// pointer to sub-struct table
};


/*
	BitmapType
		Int16				width;
		Int16				height;
		UInt16				rowBytes;
		BitmapFlagsType		flags;
		UInt8				pixelSize;			// bits/pixel
		UInt8				version;			// version of bitmap. This is vers 2
		UInt16	 			nextDepthOffset;	// # of DWords to next BitmapType
												//  from beginnning of this one
		UInt8				transparentIndex;	// v2 only, if flags.hasTransparency is true,
												// index number of transparent color
		UInt8				compressionType;	// v2 only, if flags.compressed is true, this is
												// the type, see BitmapCompressionType

		UInt16	 			reserved;			// for future use, must be zero!

		// [colorTableType] pixels | pixels*
												// If hasColorTable != 0, we have:
												//   ColorTableType followed by pixels. 
												// If hasColorTable == 0:
												//   this is the start of the pixels
												// if indirect != 0 bits are stored indirectly.
												//   the address of bits is stored here
												//   In some cases the ColorTableType will
												//   have 0 entries and be 2 bytes long.
*/

static const EmFieldLookup kBitmapTypeV2Table[] =
{
	FIELDS(BitmapTypeV2, width,				"BmpGetDimensions"),
	FIELDS(BitmapTypeV2, height,			"BmpGetDimensions"),
	FIELDS(BitmapTypeV2, rowBytes,			"BmpGetDimensions"),
	FIELDS(BitmapTypeV2, flags,				NULL),
	FIELDS(BitmapTypeV2, pixelSize,			"BmpGetBitDepth"),
	FIELDS(BitmapTypeV2, version,			NULL),
	FIELDS(BitmapTypeV2, nextDepthOffset,	"BmpGetNextBitmap"),
	FIELDS(BitmapTypeV2, transparentIndex,	NULL),
	FIELDS(BitmapTypeV2, compressionType,	NULL),
	FIELDS(BitmapTypeV2, reserved,			NULL),
	END_OF_FIELDS
};


static const EmFieldLookup kBitmapTypeV3Table[] =
{
	FIELDS(BitmapTypeV3, width,				"BmpGetDimensions"),
	FIELDS(BitmapTypeV3, height,			"BmpGetDimensions"),
	FIELDS(BitmapTypeV3, rowBytes,			"BmpGetDimensions"),
	FIELDS(BitmapTypeV3, flags,				NULL),
	FIELDS(BitmapTypeV3, pixelSize,			"BmpGetBitDepth"),
	FIELDS(BitmapTypeV3, version,			NULL),
	FIELDS(BitmapTypeV3, size,				NULL),
	FIELDS(BitmapTypeV3, pixelFormat,		NULL),
	FIELDS(BitmapTypeV3, unused,			NULL),
	FIELDS(BitmapTypeV3, compressionType,	NULL),
	FIELDS(BitmapTypeV3, density,			NULL),
	FIELDS(BitmapTypeV3, transparentValue,	NULL),
	FIELDS(BitmapTypeV3, nextDepthOffset,	"BmpGetNextBitmap"),
	END_OF_FIELDS
};


/*
	WindowType
		Coord				displayWidthV20;	// WinGetDisplayExtent
		Coord				displayHeightV20;	// WinGetDisplayExtent
		void *				displayAddrV20;		// BmpGetBits (aka BltGetBitsAddr) [3.5]
		WindowFlagsType		windowFlags;
								format			//
								offscreen		//
								modal			// WinModal
								focusable		//
								enabled			// WinEnableWindow, WinDisableWindow
								visible			//
								dialog			//
								freeBitmap		//
		RectangleType		windowBounds;		// WinGetWindowFrameRect, WinGetFramesRectangle, WinSetBounds
		AbsRectType			clippingBounds;		// WinSetClip, WinResetClip
		BitmapPtr			bitmapP;			// WinGetBitmap [3.5]
		FrameBitsType   	frameType;			//
		DrawStateType *		drawStateP;			// WinSetForeColor, WinSetBackColor, WinSetTextColor, WinSetForeColorRGB, WinSetBackColorRGB, WinSetTextColorRGB, WinGetPattern, WinSetPattern, WinGetPatternType, WinSetPatternType, WinSetDrawMode, WinSetUnderlineMode
		struct WindowType *	nextWindow;			//
*/

static const EmFieldLookup kWindowTypeTable[] =
{
	FIELDS(WindowType, displayWidthV20, "WinGetDisplayExtent"),
	FIELDS(WindowType, displayHeightV20, "WinGetDisplayExtent"),
	FIELDS(WindowType, displayAddrV20, "BmpGetBits"),
	FIELDS(WindowType, windowFlags, "WinModal, WinEnableWindow, WinDisableWindow"),
	FIELDS(WindowType, windowBounds, "WinGetWindowFrameRect, WinGetFramesRectangle, WinSetBounds"),
	FIELDS(WindowType, clippingBounds, "WinSetClip, WinResetClip"),
	FIELDS(WindowType, bitmapP, "WinGetBitmap"),
	FIELDS(WindowType, frameType, NULL),
	FIELDS(WindowType, drawStateP, "WinSetForeColor, WinSetBackColor, WinSetTextColor, WinSetForeColorRGB, WinSetBackColorRGB, WinSetTextColorRGB, WinGetPattern, WinSetPattern, WinGetPatternType, WinSetPatternType, WinSetDrawMode, WinSetUnderlineMode"),
	FIELDS(WindowType, nextWindow, NULL),
	END_OF_FIELDS
};


/*
	FormType
		WindowType				window;
		UInt16					formId;
		FormAttrType			attr;
									usable			:1;	// Set if part of ui 
									enabled			:1;	// Set if interactable (not grayed out)
									visible			:1;	// Set if drawn, used internally
									dirty			:1;	// Set if dialog has been modified
									saveBehind		:1;	// Set if bits behind form are save when form ids drawn
									graffitiShift	:1;	// Set if graffiti shift indicator is supported
									globalsAvailable:1; // Set by Palm OS if globals are available for the
														// form event handler
									doingDialog		:1;	// FrmDoDialog is using for nested event loop
									exitDialog		:1;	// tells FrmDoDialog to bail out and stop using this form
									reserved		:7;	// pad to 16
									reserved2;			// FormAttrType now explicitly 32-bits wide.
		WinHandle				bitsBehindForm;
		FormEventHandlerType*	handler;
		UInt16					focus;
		UInt16					defaultButton;
		UInt16					helpRscId;
		UInt16					menuRscId;
		UInt16					numObjects;
		FormObjListType*		objects;
*/

static const EmFieldLookup kFormTypeTable[] =
{
	SUB_FIELDS(FormType, window, kWindowTypeTable),
	FIELDS(FormType, formId, NULL),
	FIELDS(FormType, attr, NULL),
	FIELDS(FormType, bitsBehindForm, NULL),
	FIELDS(FormType, handler, NULL),
	FIELDS(FormType, focus, NULL),
	FIELDS(FormType, defaultButton, NULL),
	FIELDS(FormType, helpRscId, NULL),
	FIELDS(FormType, menuRscId, NULL),
	FIELDS(FormType, numObjects, NULL),
	FIELDS(FormType, objects, NULL),
	END_OF_FIELDS
};


/*
	FormObjListType
		FormObjectKind		objectType;
		UInt8				reserved;
		FormObjectType		object;
*/

static const EmFieldLookup kFormObjListTypeTable[] =
{
	FIELDS(FormObjListType, objectType, NULL),
	FIELDS(FormObjListType, reserved, NULL),
	FIELDS(FormObjListType, object, NULL),
	END_OF_FIELDS
};


/*
	FieldType
		UInt16				id;
		RectangleType		rect;
		FieldAttrType		attr;
		Char*				text;					// pointer to the start of text string 
		MemHandle			textHandle;				// block the contains the text string
		LineInfoPtr			lines;
		UInt16				textLen;
		UInt16				textBlockSize;
		UInt16				maxChars;
		UInt16				selFirstPos;
		UInt16				selLastPos;
		UInt16				insPtXPos;
		UInt16				insPtYPos;
		FontID				fontID;
		UInt8 				maxVisibleLines;
*/

static const EmFieldLookup kFieldTypeTable[] =
{
	FIELDS(FieldType, id, NULL),
	FIELDS(FieldType, rect, NULL),
	FIELDS(FieldType, attr, NULL),
	FIELDS(FieldType, text, NULL),
	FIELDS(FieldType, textHandle, NULL),
	FIELDS(FieldType, lines, NULL),
	FIELDS(FieldType, textLen, NULL),
	FIELDS(FieldType, textBlockSize, NULL),
	FIELDS(FieldType, maxChars, NULL),
	FIELDS(FieldType, selFirstPos, NULL),
	FIELDS(FieldType, selLastPos, NULL),
	FIELDS(FieldType, insPtXPos, NULL),
	FIELDS(FieldType, insPtYPos, NULL),
	FIELDS(FieldType, fontID, NULL),
	FIELDS(FieldType, maxVisibleLines, NULL),
	END_OF_FIELDS
};


/*
	ControlType
		UInt16				id;
		RectangleType		bounds;
		Char*				text;
		ControlAttrType		attr;
		ControlStyleType	style;
		FontID				font;
		UInt8				group;
		UInt8				reserved;
*/

static const EmFieldLookup kControlTypeTable[] =
{
	FIELDS(ControlType, id, NULL),
	FIELDS(ControlType, bounds, NULL),
	FIELDS(ControlType, text, NULL),
	FIELDS(ControlType, attr, NULL),
	FIELDS(ControlType, style, NULL),
	FIELDS(ControlType, font, NULL),
	FIELDS(ControlType, group, NULL),
	FIELDS(ControlType, reserved, NULL),
	END_OF_FIELDS
};


/*
	GraphicControlType
		UInt16				id;
		RectangleType		bounds;
		DmResID				bitmapID;			// overlays text in ControlType
		DmResID				selectedBitmapID;	// overlays text in ControlType
		ControlAttrType		attr;
		ControlStyleType	style;
		FontID				unused;
		UInt8				group;
		UInt8				reserved;
*/

static const EmFieldLookup kGraphicControlTypeTable[] =
{
	FIELDS(GraphicControlType, id, NULL),
	FIELDS(GraphicControlType, bounds, NULL),
	FIELDS(GraphicControlType, bitmapID, NULL),
	FIELDS(GraphicControlType, selectedBitmapID, NULL),
	FIELDS(GraphicControlType, attr, NULL),
	FIELDS(GraphicControlType, style, NULL),
	FIELDS(GraphicControlType, unused, NULL),
	FIELDS(GraphicControlType, group, NULL),
	FIELDS(GraphicControlType, reserved, NULL),
	END_OF_FIELDS
};


/*
	SliderControlType
		UInt16				id;
		RectangleType		bounds;
		DmResID				thumbID;		// overlays text in ControlType
		DmResID				backgroundID;	// overlays text in ControlType
		ControlAttrType		attr;			// graphical *is* set
		ControlStyleType	style;			// must be sliderCtl or repeatingSliderCtl
		UInt8				reserved;
		Int16				minValue;
		Int16				maxValue;
		Int16				pageSize;
		Int16				value;
		MemPtr				activeSliderP;
*/

static const EmFieldLookup kSliderControlTypeTable[] =
{
	FIELDS(SliderControlType, id, NULL),
	FIELDS(SliderControlType, bounds, NULL),
	FIELDS(SliderControlType, thumbID, NULL),
	FIELDS(SliderControlType, backgroundID, NULL),
	FIELDS(SliderControlType, attr, NULL),
	FIELDS(SliderControlType, style, NULL),
	FIELDS(SliderControlType, reserved, NULL),
	FIELDS(SliderControlType, minValue, NULL),
	FIELDS(SliderControlType, maxValue, NULL),
	FIELDS(SliderControlType, pageSize, NULL),
	FIELDS(SliderControlType, value, NULL),
	FIELDS(SliderControlType, activeSliderP, NULL),
	END_OF_FIELDS
};


/*
	ListType
		UInt16				id;
		RectangleType		bounds;
		ListAttrType		attr;
		Char**				itemsText;
		Int16				numItems;			// number of choices in the list
		Int16				currentItem;		// currently display choice
		Int16				topItem;			// top item visible when poped up
		FontID				font;				// font used to draw list
		UInt8				reserved;
		WinHandle			popupWin;			// used only by popup lists
		ListDrawDataFuncPtr	drawItemsCallback;	// 0 indicates no function
*/

static const EmFieldLookup kListTypeTable[] =
{
	FIELDS(ListType, id, NULL),
	FIELDS(ListType, bounds, NULL),
	FIELDS(ListType, attr, NULL),
	FIELDS(ListType, itemsText, NULL),
	FIELDS(ListType, numItems, NULL),
	FIELDS(ListType, currentItem, NULL),
	FIELDS(ListType, topItem, NULL),
	FIELDS(ListType, font, NULL),
	FIELDS(ListType, reserved, NULL),
	FIELDS(ListType, popupWin, NULL),
	FIELDS(ListType, drawItemsCallback, NULL),
	END_OF_FIELDS
};


/*
	TableType
		UInt16					id;
		RectangleType			bounds;
		TableAttrType			attr;
		Int16					numColumns;
		Int16					numRows;
		Int16					currentRow;
		Int16					currentColumn;
		Int16					topRow;
		TableColumnAttrType*	columnAttrs;
		TableRowAttrType*		rowAttrs;
		TableItemPtr			items;
		FieldType				currentField;
*/

static const EmFieldLookup kTableTypeTable[] =
{
	FIELDS(TableType, id, NULL),
	FIELDS(TableType, bounds, NULL),
	FIELDS(TableType, attr, NULL),
	FIELDS(TableType, numColumns, NULL),
	FIELDS(TableType, numRows, NULL),
	FIELDS(TableType, currentRow, NULL),
	FIELDS(TableType, currentColumn, NULL),
	FIELDS(TableType, topRow, NULL),
	FIELDS(TableType, columnAttrs, NULL),
	FIELDS(TableType, rowAttrs, NULL),
	FIELDS(TableType, items, NULL),
	SUB_FIELDS(TableType, currentField, kFieldTypeTable),
	END_OF_FIELDS
};


/*
	FormBitmapType
		FormObjAttrType	attr;
		PointType		pos;
		UInt16			rscID;
*/

static const EmFieldLookup kFormBitmapTypeTable[] =
{
	FIELDS(FormBitmapType, attr, NULL),
	FIELDS(FormBitmapType, pos, NULL),
	FIELDS(FormBitmapType, rscID, NULL),
	END_OF_FIELDS
};


/*
	FormLineType
		FormObjAttrType	attr;
		PointType		point1;
		PointType		point2;
*/

static const EmFieldLookup kFormLineTypeTable[] =
{
	FIELDS(FormLineType, attr, NULL),
	FIELDS(FormLineType, point1, NULL),
	FIELDS(FormLineType, point2, NULL),
	END_OF_FIELDS
};


/*
	FormFrameType
		UInt16			id;
		FormObjAttrType	attr;
		RectangleType	rect;
		UInt16			frameType;
*/

static const EmFieldLookup kFormFrameTypeTable[] =
{
	FIELDS(FormFrameType, id, NULL),
	FIELDS(FormFrameType, attr, NULL),
	FIELDS(FormFrameType, rect, NULL),
	FIELDS(FormFrameType, frameType, NULL),
	END_OF_FIELDS
};


/*
	FormRectangleType
		FormObjAttrType	attr;
		RectangleType	rect;
*/

static const EmFieldLookup kFormRectangleTypeTable[] =
{
	FIELDS(FormRectangleType, attr, NULL),
	FIELDS(FormRectangleType, rect, NULL),
	END_OF_FIELDS
};


/*
	FormLabelType
		UInt16			id;
		PointType		pos;
		FormObjAttrType	attr;
		FontID			fontID;
		UInt8 			reserved;
		Char*			text;
*/

static const EmFieldLookup kFormLabelTypeTable[] =
{
	FIELDS(FormLabelType, id, NULL),
	FIELDS(FormLabelType, pos, NULL),
	FIELDS(FormLabelType, attr, NULL),
	FIELDS(FormLabelType, fontID, NULL),
	FIELDS(FormLabelType, reserved, NULL),
	FIELDS(FormLabelType, text, NULL),
	END_OF_FIELDS
};


/*
	FormTitleType
		RectangleType	rect;
		Char *			text;
*/

static const EmFieldLookup kFormTitleTypeTable[] =
{
	FIELDS(FormTitleType, rect, NULL),
	FIELDS(FormTitleType, text, NULL),
	END_OF_FIELDS
};


/*
	FormPopupType
		UInt16		controlID;
		UInt16		listID;
*/

static const EmFieldLookup kFormPopupTypeTable[] =
{
	FIELDS(FormPopupType, controlID, NULL),
	FIELDS(FormPopupType, listID, NULL),
	END_OF_FIELDS
};


/*
	FrmGraffitiStateType
		PointType	pos;
*/

static const EmFieldLookup kFrmGraffitiStateTypeTable[] =
{
	FIELDS(FrmGraffitiStateType, pos, NULL),
	END_OF_FIELDS
};


/*
	FormGadgetType
		UInt16					id;
		FormGadgetAttrType		attr;
		RectangleType			rect;
		const void*				data;
		FormGadgetHandlerType*	handler;
*/

static const EmFieldLookup kFormGadgetTypeTable[] =
{
	FIELDS(FormGadgetType, id, NULL),
	FIELDS(FormGadgetType, attr, NULL),
	FIELDS(FormGadgetType, rect, NULL),
	FIELDS(FormGadgetType, data, NULL),
	FIELDS(FormGadgetType, handler, NULL),
	END_OF_FIELDS
};


/*
	ScrollBarType
		RectangleType		bounds;
		UInt16				id;
		ScrollBarAttrType	attr;
		Int16				value;
		Int16				minValue;
		Int16				maxValue;
		Int16				pageSize;
		Int16				penPosInCar;
		Int16				savePos;
*/

static const EmFieldLookup kScrollBarTypeTable[] =
{
	FIELDS(ScrollBarType, bounds, NULL),
	FIELDS(ScrollBarType, id, NULL),
	FIELDS(ScrollBarType, attr, NULL),
	FIELDS(ScrollBarType, value, NULL),
	FIELDS(ScrollBarType, minValue, NULL),
	FIELDS(ScrollBarType, maxValue, NULL),
	FIELDS(ScrollBarType, pageSize, NULL),
	FIELDS(ScrollBarType, penPosInCar, NULL),
	FIELDS(ScrollBarType, savePos, NULL),
	END_OF_FIELDS
};


void PrvLookupField (const EmFieldLookup* table, size_t offset,
					 const char*& fieldName, const char*& function);
string PrvGetProscribedReason (const SystemCallContext& context);


static ParamList	gUserParameters;


// This table should match up with gPalmOSLibraries in EmPalmFunction.cpp.
static const long	gResourceBases [] =
{
	kStr_INetLibTrapBase,
	kStr_IrLibTrapBase, // Also includes Exchange Lib
	kStr_SecLibTrapBase,
	kStr_WebLibTrapBase,
//	kStr_serIrCommLibTrapBase,	// ???SerIrCommLib.h doesn't declare the functions as SYSTRAPs!
	kStr_SerLibTrapBase,
	kStr_SerLibTrapBase,
	kStr_NetLibTrapBase,
	kStr_HTALLibTrapBase,
	kStr_RailLibTrapBase,
	kStr_NPILibTrapBase,
	kStr_SerLibTrapBase,
	kStr_SerLibTrapBase,
	kStr_HTALLibTrapBase
};


#pragma mark -

#define kFatal				1
#define	kEnterDebuggerFirst	2

static EmCommonDialogFlags PrvButtonFlags (int flags)
{
	return (flags & kFatal)
		? kDlgFlags_continue_DEBUG_Reset
		: kDlgFlags_Continue_DEBUG_Reset;
}

static Bool PrvEnterDebuggerFirst (int flags)
{
	return (flags & kEnterDebuggerFirst) ? true : false;
}

static string PrvAsHex4 (uint16 value)
{
	char	buffer[20];
	sprintf (buffer, "0x%04lX", (uint32) value);
	return string (buffer);
}

static string PrvAsHex8 (uint32 value)
{
	char	buffer[20];
	sprintf (buffer, "0x%08lX", value);
	return string (buffer);
}

static string PrvAsDecimal (int32 value)
{
	char	buffer[20];
	sprintf (buffer, "%ld", value);
	return string (buffer);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::Initialize
// ---------------------------------------------------------------------------

void Errors::Initialize (void)
{
}


// ---------------------------------------------------------------------------
//		¥ Errors::Reset
// ---------------------------------------------------------------------------

void Errors::Reset (void)
{
}


// ---------------------------------------------------------------------------
//		¥ Errors::Save
// ---------------------------------------------------------------------------

void Errors::Save (SessionFile&)
{
}


// ---------------------------------------------------------------------------
//		¥ Errors::Load
// ---------------------------------------------------------------------------

void Errors::Load (SessionFile&)
{
}


// ---------------------------------------------------------------------------
//		¥ Errors::Dispose
// ---------------------------------------------------------------------------

void Errors::Dispose (void)
{
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::ReportIfError
// ---------------------------------------------------------------------------
// Checks for the indicated error condition.  If there is an error, it
// displays a "Could not foo because bar." message in a dialog with an OK
// button.  If an optional recovery string is provided, the message is
// "Could no foo because bar.  Do this." message.  If "throwAfter" is true,
// Errors::Scram is called after the dialog is dismissed.
//
// "operation" and "recovery" are "kStr_Foo" values.
// "error" is Mac or Windows error number, or a kError_Foo value.

void Errors::ReportIfError (StrCode operation, ErrCode error, StrCode recovery, Bool throwAfter)
{
	if (error)
	{
		Errors::SetStandardParameters ();

		if (Errors::SetErrorParameters (operation, error, recovery))
		{
			Errors::DoDialog (kStr_OpErrorRecover, kDlgFlags_OK);
		}
		else
		{
			Errors::DoDialog (kStr_OpError, kDlgFlags_OK);
		}

		if (throwAfter)
		{
			Errors::Scram ();
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportIfPalmError
// ---------------------------------------------------------------------------
// Checks for the indicated error condition.  If there is an error, it
// displays a "Could not foo because bar." message in a dialog with an OK
// button.  If an optional recovery string is provided, the message is
// "Could no foo because bar.  Do this." message.  If "throwAfter" is true,
// Errors::Scram is called after the dialog is dismissed.
//
// "operation" and "recovery" are "kStr_Foo" values.
// "err" is a Palm OS error number.

void Errors::ReportIfPalmError (StrCode operation, Err err, StrCode recovery, Bool throwAfter)
{
	if (err)
	{
		Errors::ReportIfError (operation, ::ConvertFromPalmError (err), recovery, throwAfter);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportIfNULL
// ---------------------------------------------------------------------------
// Checks for the indicated error condition.  If there is an error, it
// displays a "Could not foo because bar." message in a dialog with an OK
// button.  If an optional recovery string is provided, the message is
// "Could no foo because bar.  Do this." message.  If "throwAfter" is true,
// Errors::Scram is called after the dialog is dismissed.
//
// "operation" and "recovery" are "kStr_Foo" values.
// "p" is a pointer to be tested.

void Errors::ReportIfNULL (StrCode operation, void* p, StrCode recovery, Bool throwAfter)
{
	if (p == NULL)
	{
		Errors::ReportIfError (operation, kError_OutOfMemory, recovery, throwAfter);
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrBusError
// ---------------------------------------------------------------------------

void Errors::ReportErrBusError (emuptr address, long size, Bool forRead)
{
	if (EmBankSRAM::ValidAddress (address, size))
	{
		Errors::ReportErrStorageHeap (address, size, forRead);
	}

	else if (address >= 0x80000000 && address < 0x80000000 + EmAliasWindowType<PAS>::GetSize ())
	{
		Errors::ReportErrNoDrawWindow (address, size, forRead);
	}

	else if (Errors::LooksLikeA5Access (address, size, forRead))
	{
		Errors::ReportErrNoGlobals (address, size, forRead);
	}

	else
	{
		Errors::ReportErrAccessCommon (
			kStr_ErrBusError,
			kException_BusErr,
			kFatal,
			address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrAddressError
// ---------------------------------------------------------------------------

void Errors::ReportErrAddressError (emuptr address, long size, Bool forRead)
{
	Errors::ReportErrAccessCommon (
		kStr_ErrAddressError,
		kException_AddressErr,
		kFatal,
		address, size, forRead);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrIllegalInstruction
// ---------------------------------------------------------------------------

void Errors::ReportErrIllegalInstruction (uint16 opcode)
{
	Errors::ReportErrOpcodeCommon (
		kStr_ErrIllegalInstruction,
		kException_IllegalInstr,
		kFatal,
		opcode);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrDivideByZero
// ---------------------------------------------------------------------------

void Errors::ReportErrDivideByZero (void)
{
	Errors::ReportErrCommon (kStr_ErrDivideByZero, kException_DivideByZero, kFatal);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrCHKInstruction
// ---------------------------------------------------------------------------

void Errors::ReportErrCHKInstruction (void)
{
	Errors::ReportErrCommon (kStr_ErrCHKInstruction, kException_Chk, kFatal);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrTRAPVInstruction
// ---------------------------------------------------------------------------

void Errors::ReportErrTRAPVInstruction (void)
{
	Errors::ReportErrCommon (kStr_ErrTRAPVInstruction, kException_Trap, kFatal);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrPrivilegeViolation
// ---------------------------------------------------------------------------

void Errors::ReportErrPrivilegeViolation (uint16 opcode)
{
	Errors::ReportErrOpcodeCommon (
		kStr_ErrPrivilegeViolation,
		kException_Privilege,
		kFatal,
		opcode);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrTrace
// ---------------------------------------------------------------------------

void Errors::ReportErrTrace (void)
{
	Errors::ReportErrCommon (kStr_ErrTrace, kException_Trace, kFatal | kEnterDebuggerFirst);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrATrap
// ---------------------------------------------------------------------------

void Errors::ReportErrATrap (uint16 opcode)
{
	if (opcode == 0xA9EB || opcode == 0xA9EC || opcode == 0xA9EE)
	{
		Errors::ReportErrSANE ();
	}

	else
	{
		Errors::ReportErrOpcodeCommon (kStr_ErrATrap, kException_ATrap, kFatal, opcode);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrFTrap
// ---------------------------------------------------------------------------

void Errors::ReportErrFTrap (uint16 opcode)
{
	Errors::ReportErrOpcodeCommon (kStr_ErrFTrap, kException_FTrap, kFatal, opcode);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrTRAPx
// ---------------------------------------------------------------------------

void Errors::ReportErrTRAPx (int trapNum)
{
	if (trapNum == 0)
	{
		Errors::ReportErrTRAP0 ();
	}

	else if (trapNum == 8)
	{
		Errors::ReportErrTRAP8 ();
	}

	else
	{
		// Set the %app message variable.

		Errors::SetStandardParameters ();

		// Set the %num message variable.

		Errors::SetParameter ("%num", trapNum);

		// Show the dialog.

		Errors::HandleDialog (
			kStr_ErrTRAPx,
			(ExceptionNumber) (kException_Trap0 + trapNum),
			kDlgFlags_continue_DEBUG_Reset, false);
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrStorageHeap
// ---------------------------------------------------------------------------

void Errors::ReportErrStorageHeap (emuptr address, long size, Bool forRead)
{
	Errors::ReportErrAccessCommon (
		kStr_ErrStorageHeap,
		kException_BusErr,
		kFatal,
		address, size, forRead);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrNoDrawWindow
// ---------------------------------------------------------------------------

void Errors::ReportErrNoDrawWindow (emuptr address, long size, Bool forRead)
{
	Errors::ReportErrAccessCommon (
		kStr_ErrNoDrawWindow,
		kException_BusErr,
		kFatal,
		address, size, forRead);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrNoGlobals
// ---------------------------------------------------------------------------

void Errors::ReportErrNoGlobals (emuptr address, long size, Bool forRead)
{
	// Set the %launch_code message variable.

	EmuAppInfo	appInfo		= EmPatchState::GetCurrentAppInfo ();
	const char*	launchStr	= ::LaunchCmdToString (appInfo.fCmd);

	Errors::SetParameter ("%launch_code", launchStr);

	Errors::ReportErrAccessCommon (
		kStr_ErrNoGlobals,
		kException_BusErr,
		kFatal,
		address, size, forRead);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrSANE
// ---------------------------------------------------------------------------

void Errors::ReportErrSANE (void)
{
	Errors::ReportErrCommon (kStr_ErrSANE, kException_ATrap, kFatal);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrTRAP0
// ---------------------------------------------------------------------------

void Errors::ReportErrTRAP0 (void)
{
	Errors::ReportErrCommon (kStr_ErrTRAP0, kException_Trap0, kFatal | kEnterDebuggerFirst);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrTRAP8
// ---------------------------------------------------------------------------

void Errors::ReportErrTRAP8 (void)
{
	Errors::ReportErrCommon (kStr_ErrTRAP8, kException_Trap8, kEnterDebuggerFirst);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrStackOverflow
// ---------------------------------------------------------------------------

void Errors::ReportErrStackOverflow (void)
{
	Errors::ReportErrStackCommon (kStr_ErrStackOverflow, kException_SoftBreak, kFatal);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrUnimplementedTrap
// ---------------------------------------------------------------------------

void Errors::ReportErrUnimplementedTrap (const SystemCallContext& context)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %trap_num message variable.

	string	asString (::PrvAsHex4 (context.fTrapWord));
	Errors::SetParameter ("%trap_num", asString.c_str ());

	// Get the trap name.  Look it up in our string resources first.

	uint32	errorBase = ::IsSystemTrap (context.fTrapWord)
		? kStr_SysTrapBase
		: gResourceBases[context.fLibIndex];

	string	trapName (Platform::GetString (errorBase + context.fTrapIndex));

	// If we couldn't find it, say that it's an unknown trap.

	if (trapName[0] == '<')	// Start of "<Missing string...>"
	{
		trapName = Platform::GetString (kStr_UnknownTrapNumber);
	}

	// Set the %trap_name message variable.

	Errors::SetParameter ("%trap_name", trapName);

	// Show the dialog.

	Errors::HandleDialog (kStr_ErrUnimplementedTrap, kException_SoftBreak, kDlgFlags_continue_DEBUG_Reset, false);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrInvalidRefNum
// ---------------------------------------------------------------------------

void Errors::ReportErrInvalidRefNum (const SystemCallContext& context)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %ref_num message variable.

	Errors::SetParameter ("%ref_num", context.fExtra);

	// Show the dialog.

	Errors::HandleDialog (kStr_ErrInvalidRefNum, kException_SoftBreak, kDlgFlags_continue_DEBUG_Reset, false);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrCorruptedHeap
// ---------------------------------------------------------------------------

void Errors::ReportErrCorruptedHeap (ErrCode corruptionType, emuptr chunkHdr)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %mem message variable.

	string	asString (::PrvAsHex8 (chunkHdr));
	Errors::SetParameter ("%mem", asString.c_str ());

	// Get the string that describes the type of corruption.

	int	strID = 0;

	switch (corruptionType)
	{
		case kError_CorruptedHeap_ChunkNotInHeap:
			strID = kStr_ChunkNotInHeap;
			break;

		case kError_CorruptedHeap_ChunkTooLarge:
			strID = kStr_ChunkTooLarge;
			break;

		case kError_CorruptedHeap_InvalidFlags:
			strID = kStr_InvalidFlags;
			break;

		case kError_CorruptedHeap_HOffsetNotInMPT:
			strID = kStr_HOffsetNotInMPT;
			break;

		case kError_CorruptedHeap_HOffsetNotBackPointing:
			strID = kStr_HOffsetNotBackPointing;
			break;

		case kError_CorruptedHeap_InvalidLockCount:
			strID = kStr_InvalidLockCount;
			break;

		default:
			EmAssert (false);
	}

	// Set the %corruption_type message variable.

	Errors::SetParameter ("%corruption_type", Platform::GetString (strID));

	// Show the dialog.

	Errors::HandleDialog (kStr_ErrCorruptedHeap, kException_SoftBreak, kDlgFlags_continue_debug_RESET, false);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrInvalidPC
// ---------------------------------------------------------------------------

void Errors::ReportErrInvalidPC (emuptr address, int reason)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %mem message variable.

	string	asString (::PrvAsHex8 (address));
	Errors::SetParameter ("%mem", asString.c_str ());

	// Get the string describing why the address is invalid.

	int	strID = 0;

	switch (reason)
	{
		case kUnmappedAddress:
			strID = kStr_UnmappedAddress;
			break;

		case kNotInCodeSegment:
			strID = kStr_NotInCodeSegment;
			break;

		case kOddAddress:
			strID = kStr_OddAddress;
			break;

		default:
			EmAssert (false);
	}

	// Set the %reason message variable.

	Errors::SetParameter ("%reason", Platform::GetString (strID));

	// Show the dialog.

	Errors::HandleDialog (kStr_ErrInvalidPC1, kException_SoftBreak, kDlgFlags_continue_DEBUG_Reset, false);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrLowMemory
// ---------------------------------------------------------------------------

void Errors::ReportErrLowMemory (emuptr address, long size, Bool forRead)
{
	if (::ReportLowMemoryAccess ())
	{
		Errors::ReportErrAccessCommon (kStr_ErrLowMemory, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrSystemGlobals
// ---------------------------------------------------------------------------

void Errors::ReportErrSystemGlobals (emuptr address, long size, Bool forRead)
{
	if (::ReportSystemGlobalAccess ())
	{
		Errors::ReportErrAccessCommon (kStr_ErrSystemGlobals, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrScreen
// ---------------------------------------------------------------------------

void Errors::ReportErrScreen (emuptr address, long size, Bool forRead)
{
	if (::ReportScreenAccess ())
	{
		Errors::ReportErrAccessCommon (kStr_ErrScreen, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrHardwareRegisters
// ---------------------------------------------------------------------------

void Errors::ReportErrHardwareRegisters (emuptr address, long size, Bool forRead)
{
	if (::ReportHardwareRegisterAccess ())
	{
		Errors::ReportErrAccessCommon (kStr_ErrHardwareRegisters, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrROM
// ---------------------------------------------------------------------------

void Errors::ReportErrROM (emuptr address, long size, Bool forRead)
{
	if (::ReportROMAccess ())
	{
		Errors::ReportErrAccessCommon (kStr_ErrROM, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrMemMgrStructures
// ---------------------------------------------------------------------------

void Errors::ReportErrMemMgrStructures (emuptr address, long size, Bool forRead)
{
	if (::ReportMemMgrDataAccess ())
	{
		Errors::ReportErrAccessCommon (kStr_ErrMemMgrStructures, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrMemMgrLeaks
// ---------------------------------------------------------------------------

void Errors::ReportErrMemMgrLeaks (int leaks)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %num_leaks message variable.

	Errors::SetParameter ("%num_leaks", leaks);

	// Show the dialog.

	StrCode	templateStrCode = leaks == 1 ? kStr_ErrMemoryLeak : kStr_ErrMemoryLeaks;
	Errors::HandleDialog (templateStrCode, kException_SoftBreak,
			kDlgFlags_Continue_DEBUG_Reset, false);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrMemMgrSemaphore
// ---------------------------------------------------------------------------

void Errors::ReportErrMemMgrSemaphore (void)
{
	if (::ReportMemMgrSemaphore ())
	{
		Errors::ReportErrCommon (kStr_ErrMemMgrSemaphore, kException_SoftBreak, 0);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrFreeChunk
// ---------------------------------------------------------------------------

void Errors::ReportErrFreeChunk (emuptr address, long size, Bool forRead)
{
	if (::ReportFreeChunkAccess ())
	{
		Errors::ReportErrAccessCommon (kStr_ErrFreeChunk, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrUnlockedChunk
// ---------------------------------------------------------------------------

void Errors::ReportErrUnlockedChunk (emuptr address, long size, Bool forRead)
{
	if (::ReportUnlockedChunkAccess ())
	{
		Errors::ReportErrAccessCommon (kStr_ErrUnlockedChunk, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrLowStack
// ---------------------------------------------------------------------------

void Errors::ReportErrLowStack (emuptr stackLow,
								emuptr stackPointer,
								emuptr stackHigh,
								emuptr address,
								long size,
								Bool forRead)

{
	if (::ReportLowStackAccess ())
	{
		// Set the %stack_low message variable.

		string	asString1 (::PrvAsHex8 (stackLow));
		Errors::SetParameter ("%stack_low", asString1.c_str ());

		// Set the %stack_pointer message variable.

		string	asString2 (::PrvAsHex8 (stackPointer));
		Errors::SetParameter ("%stack_pointer", asString2.c_str ());

		// Set the %stack_high message variable.

		string	asString3 (::PrvAsHex8 (stackHigh));
		Errors::SetParameter ("%stack_high", asString3.c_str ());

		Errors::ReportErrAccessCommon (kStr_ErrLowStack, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrStackFull
// ---------------------------------------------------------------------------

void Errors::ReportErrStackFull (void)
{
	if (::ReportStackAlmostOverflow ())
	{
		Errors::ReportErrStackCommon (kStr_ErrStackFull, kException_SoftBreak, 0);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrSizelessObject
// ---------------------------------------------------------------------------

void Errors::ReportErrSizelessObject (uint16 id, const EmRect& r)
{
	if (::ReportSizelessObject ())
	{
		Errors::ReportErrObjectCommon (kStr_ErrSizelessObject, kException_SoftBreak, 0, id, r);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrOffscreenObject
// ---------------------------------------------------------------------------

void Errors::ReportErrOffscreenObject (uint16 id, const EmRect& r)
{
	if (::ReportOffscreenObject ())
	{
		Errors::ReportErrObjectCommon (kStr_ErrOffscreenObject, kException_SoftBreak, 0, id, r);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrFormAccess
// ---------------------------------------------------------------------------

void Errors::ReportErrFormAccess (emuptr formAddress, emuptr address, long size, Bool forRead)
{
	if (::ReportUIMgrDataAccess ())
	{
		// Set the %form message variable.

		string	asString (::PrvAsHex8 (formAddress));
		Errors::SetParameter ("%form", asString.c_str ());

		// Set the %field and %function variables.

		EmAssert (address >= formAddress);

		size_t		offset		= address - formAddress;
		const char*	fieldName	= NULL;
		const char*	function	= NULL;


		if (offset < EmAliasFormType<PAS>::GetSize ())
		{
			// Set the %field message variable.

			::PrvLookupField (kFormTypeTable, offset, fieldName, function);

			Errors::SetParameter ("%field", fieldName);

			Errors::ReportErrAccessCommon (kStr_ErrFormAccess, kException_BusErr, 0, address, size, forRead);
		}
		else
		{
			CEnableFullAccess	munge;

			EmAliasFormType<PAS>	form (formAddress);

			emuptr	firstObject	= form.objects;
#ifndef NDEBUG
			uint16	numObjects	= form.numObjects;
			emuptr	lastObject	= firstObject + numObjects * EmAliasFormObjListType<PAS>::GetSize ();
#endif

			EmAssert (address >= firstObject && address < lastObject);

			size_t	listOffset	= address - firstObject;
			size_t	index		= listOffset / EmAliasFormObjListType<PAS>::GetSize ();
			size_t	fieldOffset	= listOffset % EmAliasFormObjListType<PAS>::GetSize ();

			// Set the %field message variable.

			::PrvLookupField (kFormObjListTypeTable, fieldOffset, fieldName, function);

			Errors::SetParameter ("%field", fieldName);

			// Set the %index message variable.

			string	asString (::PrvAsDecimal (index));
			Errors::SetParameter ("%index", asString.c_str ());

			Errors::ReportErrAccessCommon (kStr_ErrFormObjectListAccess, kException_BusErr, 0, address, size, forRead);
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrFormObjectAccess
// ---------------------------------------------------------------------------

void Errors::ReportErrFormObjectAccess (emuptr objectAddress, emuptr formAddress, emuptr address, long size, Bool forRead)
{
	if (::ReportUIMgrDataAccess ())
	{
		CEnableFullAccess	munge;

		EmAliasFormObjListType<PAS>	object (objectAddress);
		FormObjectKind				kind = object.objectType;
		emuptr						addr = object.object;

		string						typeString ("unknown");
		const EmFieldLookup*		table = NULL;

		switch (kind)
		{
			case frmFieldObj:			typeString = "frmFieldObj";			table = kFieldTypeTable;			break;
			case frmControlObj:			typeString = "frmControlObj";		table = kControlTypeTable;			break;
			case frmListObj:			typeString = "frmListObj";			table = kListTypeTable;				break;
			case frmTableObj:			typeString = "frmTableObj";			table = kTableTypeTable;			break;
			case frmBitmapObj:			typeString = "frmBitmapObj";		table = kFormBitmapTypeTable;		break;
			case frmLineObj:			typeString = "frmLineObj";			table = kFormLineTypeTable;			break;
			case frmFrameObj:			typeString = "frmFrameObj";			table = kFormFrameTypeTable;		break;
			case frmRectangleObj:		typeString = "frmRectangleObj";		table = kFormRectangleTypeTable;	break;
			case frmLabelObj:			typeString = "frmLabelObj";			table = kFormLabelTypeTable;		break;
			case frmTitleObj:			typeString = "frmTitleObj";			table = kFormTitleTypeTable;		break;
			case frmPopupObj:			typeString = "frmPopupObj";			table = kFormPopupTypeTable;		break;
			case frmGraffitiStateObj:	typeString = "frmGraffitiStateObj";	table = kFrmGraffitiStateTypeTable;	break;
			case frmGadgetObj:			typeString = "frmGadgetObj";		table = kFormGadgetTypeTable;		break;
			case frmScrollBarObj:		typeString = "frmScrollBarObj";		table = kScrollBarTypeTable;		break;
		}

		// Set the %object message variable.

		string	asString1 (::PrvAsHex8 (addr));
		Errors::SetParameter ("%object", asString1.c_str ());

		// Set the %field message variable.

		EmAssert (address >= addr);

		size_t		offset		= address - addr;
		const char*	fieldName	= NULL;
		const char*	function	= NULL;

		::PrvLookupField (table, offset, fieldName, function);

		Errors::SetParameter ("%field", fieldName);

		// Set the %type message variable.

		Errors::SetParameter ("%type", typeString.c_str ());

		// Set the %form message variable.

		string	asString2 (::PrvAsHex8 (formAddress));
		Errors::SetParameter ("%form", asString2.c_str ());

		Errors::ReportErrAccessCommon (kStr_ErrFormObjectAccess, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrWindowAccess
// ---------------------------------------------------------------------------

void Errors::ReportErrWindowAccess (emuptr windowAddress, emuptr address, long size, Bool forRead)
{
	if (::ReportUIMgrDataAccess ())
	{
		// Set the %window message variable.

		string	asString (::PrvAsHex8 (windowAddress));
		Errors::SetParameter ("%window", asString.c_str ());

		// Set the %field and %function variables.

		EmAssert (address >= windowAddress);

		size_t		offset		= address - windowAddress;
		const char*	fieldName	= NULL;
		const char*	function	= NULL;

		::PrvLookupField (kWindowTypeTable, offset, fieldName, function);

		Errors::SetParameter ("%field", fieldName);

		Errors::ReportErrAccessCommon (kStr_ErrWindowAccess, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrBitmapAccess
// ---------------------------------------------------------------------------

void Errors::ReportErrBitmapAccess (emuptr bitmapAddress, emuptr address, long size, Bool forRead)
{
	if (::ReportUIMgrDataAccess ())
	{
		// Set the %window message variable.

		string	asString (::PrvAsHex8 (bitmapAddress));
		Errors::SetParameter ("%bitmap", asString.c_str ());

		// Set the %field and %function variables.

		EmAssert (address >= bitmapAddress);

		size_t		offset		= address - bitmapAddress;
		const char*	fieldName	= NULL;
		const char*	function	= NULL;

		::PrvLookupField (kBitmapTypeV2Table, offset, fieldName, function);

		Errors::SetParameter ("%field", fieldName);

		Errors::ReportErrAccessCommon (kStr_ErrBitmapAccess, kException_BusErr, 0, address, size, forRead);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrProscribedFunction
// ---------------------------------------------------------------------------

void Errors::ReportErrProscribedFunction (const SystemCallContext& context)
{
	if (::ReportProscribedFunction ())
	{
		// Set the %app message variable.

		Errors::SetStandardParameters ();

		// Set the %function_name message variable.

		string	asString (::GetTrapName (context, true));
		Errors::SetParameter ("%function_name", asString.c_str ());

		// Set the %reason message variable.

		string	asString2 (::PrvGetProscribedReason (context));
		Errors::SetParameter ("%reason", asString2.c_str ());

		// Show the dialog.

		Errors::HandleDialog (kStr_ErrProscribedFunction, kException_SoftBreak,
			kDlgFlags_Continue_DEBUG_Reset, false);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrStepSpy
// ---------------------------------------------------------------------------

void Errors::ReportErrStepSpy (emuptr writeAddress,
							   int writeBytes,
							   emuptr ssAddress,
							   uint32 ssValue,
							   uint32 newValue)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %op message variable.

	string	operation (Platform::GetString (kStr_WroteTo));
	Errors::SetParameter ("%op", operation);

	// Set the %mem message variable.

	string	asString1 (::PrvAsHex8 (writeAddress));
	Errors::SetParameter ("%mem", asString1.c_str ());

	// Set the %write_bytes message variable.

	string	asString2 (::PrvAsDecimal (writeBytes));
	Errors::SetParameter ("%write_bytes", asString2.c_str ());

	// Set the %ss_address message variable.

	string	asString3 (::PrvAsHex8 (ssAddress));
	Errors::SetParameter ("%ss_address", asString3.c_str ());

	// Set the %old_value message variable.

	string	asString4 (::PrvAsHex8 (ssValue));
	Errors::SetParameter ("%old_value", asString4.c_str ());

	// Set the %new_value message variable.

	string	asString5 (::PrvAsHex8 (newValue));
	Errors::SetParameter ("%new_value", asString5.c_str ());

	// Show the dialog.

	Errors::HandleDialog (kStr_ErrStepSpy, kException_SoftBreak,
			kDlgFlags_Continue_DEBUG_Reset, false);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrWatchpoint
// ---------------------------------------------------------------------------

void Errors::ReportErrWatchpoint (emuptr writeAddress,
								  int writeBytes,
								  emuptr watchAddress,
								  uint32 watchBytes)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %op message variable.

	string	operation (Platform::GetString (kStr_WroteTo));
	Errors::SetParameter ("%op", operation);

	// Set the %mem message variable.

	string	asString1 (::PrvAsHex8 (writeAddress));
	Errors::SetParameter ("%mem", asString1.c_str ());

	// Set the %write_bytes message variable.

	string	asString2 (::PrvAsDecimal (writeBytes));
	Errors::SetParameter ("%write_bytes", asString2.c_str ());

	// Set the %watch_start message variable.

	string	asString3 (::PrvAsHex8 (watchAddress));
	Errors::SetParameter ("%watch_start", asString3.c_str ());

	// Set the %watch_end message variable.

	string	asString4 (::PrvAsHex8 (watchAddress + watchBytes));
	Errors::SetParameter ("%watch_end", asString4.c_str ());

	// Show the dialog.

	Errors::HandleDialog (kStr_ErrWatchpoint, kException_SoftBreak,
			kDlgFlags_Continue_DEBUG_Reset, false);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors:ReportErrSysFatalAlert
// ---------------------------------------------------------------------------

void Errors::ReportErrSysFatalAlert (const char* appMsg)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %message message variable.

	Errors::SetParameter ("%message", appMsg);

	// Show the dialog.

	Errors::HandleDialog (kStr_ErrSysFatalAlert, kException_SoftBreak,
							kDlgFlags_Continue_DEBUG_Reset, false);
}


// ---------------------------------------------------------------------------
//		¥ Errors:ReportErrDbgMessage
// ---------------------------------------------------------------------------

void Errors::ReportErrDbgMessage (const char* appMsg)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %message message variable.

	Errors::SetParameter ("%message", appMsg);

	// Show the dialog.

	Errors::HandleDialog (kStr_ErrDbgMessage, kException_SoftBreak,
			kDlgFlags_Continue_DEBUG_Reset, false);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrAccessCommon
// ---------------------------------------------------------------------------

void Errors::ReportErrAccessCommon (StrCode strIndex, ExceptionNumber excNum, int flags,
									emuptr address, long size, Bool forRead)
{
	UNUSED_PARAM(size);

	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %op message variable.

	string	operation (Platform::GetString (forRead ? kStr_ReadFrom : kStr_WroteTo));
	Errors::SetParameter ("%op", operation);

	// Set the %mem message variable.

	string	asString (::PrvAsHex8 (address));
	Errors::SetParameter ("%mem", asString.c_str ());

	// Show the dialog.

	Errors::HandleDialog (strIndex, excNum,
		::PrvButtonFlags (flags),
		::PrvEnterDebuggerFirst (flags));
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrObjectCommon
// ---------------------------------------------------------------------------

void Errors::ReportErrObjectCommon (StrCode strIndex, ExceptionNumber excNum, int flags, 
									uint16 id, const EmRect& r)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %obj_id message variable.

	{
		string	asString (::PrvAsDecimal (id));
		Errors::SetParameter ("%obj_id", asString.c_str ());
	}

	// Set the %left, %top, %right, and %bottom message variables.

	{
		string	asString (::PrvAsDecimal (r.fLeft));
		Errors::SetParameter ("%left", asString.c_str ());
	}

	{
		string	asString (::PrvAsDecimal (r.fTop));
		Errors::SetParameter ("%top", asString.c_str ());
	}

	{
		string	asString (::PrvAsDecimal (r.fRight));
		Errors::SetParameter ("%right", asString.c_str ());
	}

	{
		string	asString (::PrvAsDecimal (r.fBottom));
		Errors::SetParameter ("%bottom", asString.c_str ());
	}

	// Show the dialog.

	Errors::HandleDialog (strIndex, excNum,
		::PrvButtonFlags (flags),
		::PrvEnterDebuggerFirst (flags));
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrOpcodeCommon
// ---------------------------------------------------------------------------

void Errors::ReportErrOpcodeCommon (StrCode strIndex, ExceptionNumber excNum, int flags, uint16 opcode)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Set the %ins message variable.

	string	asString (::PrvAsHex4 (opcode));
	Errors::SetParameter ("%ins", asString.c_str ());

	// Show the dialog.

	Errors::HandleDialog (strIndex, excNum,
		::PrvButtonFlags (flags),
		::PrvEnterDebuggerFirst (flags));
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrStackCommon
// ---------------------------------------------------------------------------

void Errors::ReportErrStackCommon (StrCode strIndex, ExceptionNumber excNum, int flags)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Generate the stack crawl information.

	emuptr				oldStackLow = gCPU->GetSP ();
	EmStackFrameList	stackCrawl;
	EmPalmOS::GenerateStackCrawl (stackCrawl);

	string	stackCrawlString;
	
	stackCrawlString = ::StackCrawlString (stackCrawl, 200, true, oldStackLow);

	// Set the %sc message variable.

	Errors::SetParameter ("%sc", stackCrawlString.c_str ());

	// Show the dialog.

	Errors::HandleDialog (strIndex, excNum,
		::PrvButtonFlags (flags),
		::PrvEnterDebuggerFirst (flags));
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReportErrCommon
// ---------------------------------------------------------------------------

void Errors::ReportErrCommon (StrCode strIndex, ExceptionNumber excNum, int flags)
{
	// Set the %app message variable.

	Errors::SetStandardParameters ();

	// Show the dialog.

	Errors::HandleDialog (strIndex, excNum,
		::PrvButtonFlags (flags),
		::PrvEnterDebuggerFirst (flags));
}


// ---------------------------------------------------------------------------
//		¥ Errors:ReportErrCommPort
// ---------------------------------------------------------------------------

EmDlgItemID Errors::ReportErrCommPort (string errString)
{
	Errors::SetParameter ("%transport", errString);
	string commString = Errors::ReplaceParameters (kStr_CommPortError);

	EmDlgItemID	button = Errors::DoDialog (commString.c_str (),
							kDlgFlags_OK, -1);

	return button;
}


// ---------------------------------------------------------------------------
//		¥ Errors:ReportErrSockets
// ---------------------------------------------------------------------------

EmDlgItemID Errors::ReportErrSockets (string errString)
{
	Errors::SetParameter ("%transport", errString);
	string socketsString = Errors::ReplaceParameters (kStr_SocketsError);
	
	EmDlgItemID	button = Errors::DoDialog (socketsString.c_str (),
							kDlgFlags_OK, -1);

	return button;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::SetParameter
// ---------------------------------------------------------------------------
//	Add a user-supplied substitution rule. Parameters are deleted after they
//	are used (by making a call to ReportMessage), so they need to be
//	re-established after every call to that function.

void Errors::SetParameter (const string& key, const string& value)
{
	ClearParameter (key);
	gUserParameters.push_back (key);
	gUserParameters.push_back (value);
}

void Errors::SetParameter (const string& key, const char* value)
{
	SetParameter (key, string (value));
}

void Errors::SetParameter (const string& key, const unsigned char* value)
{
	SetParameter (key, string ((char*) &value[1], value[0]));
}

void Errors::SetParameter (const string& key, long value)
{
	char	buffer[20];
	sprintf (buffer, "%ld", value);

	SetParameter (key, string (buffer));
}


// ---------------------------------------------------------------------------
//		¥ Errors::ClearParameter
// ---------------------------------------------------------------------------
//	Remove a user-supplied substitution rule.

void Errors::ClearParameter (const string& key)
{
	long	index = (long) gUserParameters.size () - 2;
	
	while (index >= 0)
	{
		if (gUserParameters[index] == key)
		{
			gUserParameters.erase (gUserParameters.begin () + index,
									gUserParameters.begin () + index + 2);
			break;
		}

		index -= 2;
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ClearAllParameters
// ---------------------------------------------------------------------------
//	Remove a user-supplied substitution rule.

void Errors::ClearAllParameters (void)
{
	gUserParameters.clear ();
}


// ---------------------------------------------------------------------------
//		¥ Errors::SetErrorParameters
// ---------------------------------------------------------------------------

Bool Errors::SetErrorParameters (StrCode operation, ErrCode error, StrCode recovery)
{
	// Add %operation, %reason, and %recovery entries to our parameter
	// list in such a way that they appear at the start of the list
	// in that order. That way, if any of the replacement strings
	// contain parameters to be replaced, they can be replaced by
	// user-defined values.

	string s;

	// Add the error number as a possible substitution value.

	ErrCode	errCode = error;
	if (::IsPalmError (errCode))
	{
		errCode = ::ConvertToPalmError (errCode);

		// For Palm OS errors, try coming up with a description of the error.

		s = Platform::GetString (kStr_PalmOSErrorBase + errCode);

		if (s[0] == '<')
		{
			s = Platform::GetString (kStr_UnknownErrorCode);
		}

		Errors::SetParameter ("%error_desc", s);
	}

	char	errCodeString[20];
	sprintf (errCodeString, "0x%04X", (int) errCode);
	Errors::SetParameter ("%error", errCodeString);

	// If the caller didn't provide a recovery string ID, let's try to get
	// appropriate for this error code.

	if (recovery == 0)
	{
		recovery = Errors::GetIDForRecovery (error);
	}

	// If we (now) have a recovery string ID, use it to get the string
	// and add it to our parameter list.

	if (recovery)
	{
		s = Platform::GetString (recovery);
		gUserParameters.insert (gUserParameters.begin (), s);
		gUserParameters.insert (gUserParameters.begin (), "%recovery");
	}

	// Get a string for the error code provided.  If we don't have a canned
	// string, create a generic string that includes the error number.

	StrCode	errID = Errors::GetIDForError (error);
	s = Platform::GetString (errID);
	gUserParameters.insert (gUserParameters.begin (), s);
	gUserParameters.insert (gUserParameters.begin (), "%reason");

	// Finally, add the operation string to the parameter list.

	s = Platform::GetString (operation);
	gUserParameters.insert (gUserParameters.begin (), s);
	gUserParameters.insert (gUserParameters.begin (), "%operation");

	return recovery != 0;	// Return whether or not there's a recovery string in
							// the parameter list.  The caller will want to know
							// this so that it can provide the right template for it.
}


// ---------------------------------------------------------------------------
//		¥ Errors::SetStandardParameters
// ---------------------------------------------------------------------------
//	Set the following three parameters for text substitution:
//
//		%Application = current application name, or "The current application"
//		%application = current application name, or "the current application"
//		%version = current application version, or "(unknown version)"

void Errors::SetStandardParameters (void)
{
	string	appNameUC;
	string	appNameLC;
	string	version;

	Errors::GetAppName (appNameUC, appNameLC);
	Errors::GetAppVersion (version);

	Errors::SetParameter ("%App", appNameUC + " (" + version + ")");
	Errors::SetParameter ("%app", appNameLC + " (" + version + ")");
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::HandleDialog
// ---------------------------------------------------------------------------
//	Display the dialog, handling button selections in a standard fashion:
//
//		Continue:
//			Exit this function.  The caller will then be able to
//			continue without further incident.
//
//		Debug:
//			Attempt to enter the debugger.  If the attempt fails,
//			show the dialog again.
//
//		Reset:
//			Throw an exception that causes a reset at the top of
//			the CPU loop.
//
//		Next Gremlin:
//			Throw an exception that causes the next gremlin
//			to be loaded at the top of the CPU loop.

void Errors::HandleDialog (StrCode messageID,
						   ExceptionNumber excNum,
						   EmCommonDialogFlags flags,
						   Bool enterDebuggerFirst)
{
	// Expand the template here so that we can get the string in its final
	// form.  If we were to let Errors::DoDialog do the expansion, it would
	// form the string locally to itself, discarding the template parameters
	// in the process.  If we then need to loop because we were unable to
	// enter the debugger,  Errors::DoDialog would not be able to reform
	// the message because the template parameters are now gone.  So form
	// the message here and hold onto it.

	string	msgTemplate (Platform::GetString (messageID));
	string	msg (Errors::ReplaceParameters (msgTemplate, gUserParameters));

	// If this error occurred while nested (that is, while Poser itself
	// is calling into the ROM as a subroutine), then we can't really
	// recover from that.  At the very least, we don't need to bother
	// users with messages about our own mistakes.  So simply say
	// that an internal error occurred and that we're now about to reset.

	if (gSession && gSession->IsNested ())
	{
		EmExceptionReset	e (kResetSoft);
		e.SetMessage (msg.c_str ());
		throw e;
	}

	// Insert a note into the event stream that an error occurred.

	EmEventPlayback::RecordErrorEvent ();

	// If we reach this point, and minimization is taking place, then the
	// error was generated on purpose. Instead of warning the user, silently
	// switch to the next minimizer.

	if (EmMinimize::IsOn ())
	{
		if (LogGremlins ())
		{
			LogAppendMsg ("Calling EmMinimize::ErrorOccurred after encountering an error");
		}

		EmEventOutput::ErrorOccurred (msg);
		EmMinimize::ErrorOccurred ();

		return;
	}

	EmDlgItemID	button;

	do
	{
		if (enterDebuggerFirst)
		{
			button = kDlgItemDebug;
			enterDebuggerFirst = false;
		}
		else
		{
			button = Errors::DoDialog (msg.c_str (), flags, messageID);

			// If we show a dialog, then the user has already been told
			// what's gone wrong.  They don't need CodeWarrior to tell
			// them again.  Besides, CodeWarrior often puts up dialogs
			// with "missing" characters in them.  To inhibit CodeWarrior
			// from showing a dialog, we can change the exception to
			// a TRAP 0 exception.

			excNum = kException_SoftBreak;
		}

		if (button == kDlgItemDebug)
		{
			// If the user clicked on Debug, simulate a breakpoint in order to
			// get us into the debugger.

			if (Debug::EnterDebugger (excNum, NULL) == errNone)
			{
				EmExceptionEnterDebugger	e;
				throw e;
			}
		}

		else if (button == kDlgItemReset)
		{
			// Find out what kind of reset.

			EmResetType	type;
			if (EmDlg::DoReset (type) == kDlgItemOK)
			{
				EmExceptionReset	e (type);
				throw e;
			}
		}

		else if (button == kDlgItemNextGremlin)
		{
			EmExceptionNextGremlin	e;
			throw e;
		}
	} while (button != kDlgItemContinue);
}


// ---------------------------------------------------------------------------
//		¥ Errors::DoDialog
// ---------------------------------------------------------------------------
//	Displays a dialog box with the given message and according to
//	the given flags.  Returns which button was clicked.

EmDlgItemID Errors::DoDialog (StrCode messageID, EmCommonDialogFlags flags)
{
	string	msg (Platform::GetString (messageID));
	return Errors::DoDialog (msg.c_str (), flags, messageID);
}


// ---------------------------------------------------------------------------
//		¥ Errors::DoDialog
// ---------------------------------------------------------------------------
//	Displays a dialog box with the given message and according to
//	the given flags.  Returns which button was clicked.

EmDlgItemID Errors::DoDialog (const char* msg, EmCommonDialogFlags flags, StrCode messageID)
{
	string	msgStr (msg);
	msgStr = Errors::ReplaceParameters (msgStr, gUserParameters);

	// If this error occurred while nested (that is, while Poser itself
	// is calling into the ROM as a subroutine), then we can't really
	// recover from that.  At the very least, we don't need to bother
	// users with messages about our own mistakes.  So simply say
	// that an internal error occurred and that we're now about to reset.

	if (gSession && gSession->IsNested ())
	{
		EmExceptionReset	e (kResetSoft);
		e.SetMessage (msgStr.c_str ());
		throw e;
	}

	// See if this is a fatal or non-fatal message (also called "error"
	// or "warning").  The message is non-fatal if the first button
	// is a visible, enabled Continue button.

	uint8	button0	= GET_BUTTON (0, flags);
	Bool	isFatal	=	!((button0 & kButtonMask) == kDlgItemContinue &&
						(button0 & (kButtonVisible | kButtonEnabled)) == (kButtonVisible | kButtonEnabled));

	// Notify Hordes of the error so that it can keep stats.

	Hordes::RecordErrorStats (messageID);

	// Set flags governing Poser's return value.

	if (isFatal)
		gErrorHappened = true;
	else
		gWarningHappened = true;

	// If we are logging this kind of message, then we can further check
	// to see what kind of other actions to carry out.  Otherwise, we
	// display the error message.

	if (gEmuPrefs->LogMessage (isFatal))
	{
		// Log the error message.

		const char*	typeStr = isFatal ? "ERROR" : "WARNING";

		LogAppendMsg ("=== %s: ********************************************************************************", typeStr);
		LogAppendMsg ("=== %s: %s", typeStr, msgStr.c_str ());
		LogAppendMsg ("=== %s: ********************************************************************************", typeStr);

		LogDump ();

		// Determine what else we should do: quit the emulator,
		// continue on as if the user had pressed the Continue
		// button, switch to the next Gremlin in a Horde, or
		// display the message in a dialog.

		if (gEmuPrefs->ShouldQuit (isFatal))
		{
			EmAssert (gApplication);
			gApplication->ScheduleQuit ();

			if (Hordes::IsOn ())
			{
				return kDlgItemNextGremlin;
			}

			return kDlgItemContinue;
		}
		else if (gEmuPrefs->ShouldContinue (isFatal))
		{
			return kDlgItemContinue;
		}
		else if (gEmuPrefs->ShouldNextGremlin (isFatal))
		{
			return kDlgItemNextGremlin;
		}

		// ...else, drop through to show the dialog
	}

	// If we got here, it's either because logging is off for this
	// type of message (in which case, force the user to see the
	// message, or else they'd completely miss it), or their error
	// handling option said to show it in a dialog.

	return EmDlg::DoCommonDialog (msgStr, flags);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::ReplaceParameters
// ---------------------------------------------------------------------------
//	Take a string template (that is, a string containing text interspersed
//	with "parameters" (of the form "%parameterName") that need to be
//	replaced) and replace the parameters with their final values.

string Errors::ReplaceParameters (StrCode templateID)
{
	return Errors::ReplaceParameters (templateID, gUserParameters);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReplaceParameters
// ---------------------------------------------------------------------------
//	Take a string template (that is, a string containing text interspersed
//	with "parameters" (of the form "%parameterName") that need to be
//	replaced) and replace the parameters with their final values.

string Errors::ReplaceParameters (StrCode templateID, ParamList& params)
{
	string	templ = Platform::GetString (templateID);
	return Errors::ReplaceParameters (templ, params);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ReplaceParameters
// ---------------------------------------------------------------------------
//	Take a string template (that is, a string containing text interspersed
//	with "parameters" (of the form "%parameterName") that need to be
//	replaced) and replace the parameters with their final values.
//
//	Parameters are replaced one at a time, according to their position in
//	the ParamList.  The first parameter is first fully replaced in the
//	string template, in such a manner than recursion does not occur (that
//	is, if a parameter value itself include a parameter with the same
//	name, that embedded parameter is not also replaced).
//
//	The next parameter is then pulled off of ParamList and treated the
//	same way.  This time, any new parameters introduced during the previous
//	substitution pass can be replaced.
//
//	This approach allows for sequences like the following:
//
//	string template "Could not %operation because %reason."
//
//	parameter list:		"%operation"	"save the file Ò%extraÓ"
//						"%reason"		"the disk is full"
//						"%extra"		"FooBlitzky"
//
//	result: "Could not save the file ÒFooBlitzky" because the disk is full."
//
//	If the "%extra" in "save the file Ò%extraÓ" had been "%operation", it
//	would have stayed that way and never been replaced (unless "%operation"
//	appeared in the parameter list again).

string Errors::ReplaceParameters (const string& templ, ParamList& params)
{
	string	result (templ);

	ParamList::iterator	paramIter;

	for (paramIter = params.begin (); paramIter != params.end (); )
	{
		string	key		= *paramIter++;
		string	value	= *paramIter++;

		result = ::ReplaceString (result, key, value);
	}

	Errors::ClearAllParameters ();

	return result;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::Throw
// ---------------------------------------------------------------------------

#ifndef Throw_
#define Throw_(x) throw x
#endif

void Errors::Throw (ErrCode error)
{
	Throw_ (error);
}


// ---------------------------------------------------------------------------
//		¥ Errors::ThrowIfError
// ---------------------------------------------------------------------------

void Errors::ThrowIfError (ErrCode error)
{
	if (error)
	{
		Errors::Throw (error);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ThrowIfPalmError
// ---------------------------------------------------------------------------

void Errors::ThrowIfPalmError (Err error)
{
	if (error)
	{
		Errors::Throw (::ConvertFromPalmError (error));
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ThrowIfStdCError
// ---------------------------------------------------------------------------

void Errors::ThrowIfStdCError (int error)
{
	if (error)
	{
		Errors::Throw (::ConvertFromStdCError (error));
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::ThrowIfNULL
// ---------------------------------------------------------------------------

void Errors::ThrowIfNULL (void* p)
{
	if (!p)
	{
		Errors::Throw (kError_OutOfMemory);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::Scram
// ---------------------------------------------------------------------------

void Errors::Scram (void)
{
	Errors::Throw (kError_NoError);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::GetIDForError
// ---------------------------------------------------------------------------

int Errors::GetIDForError (ErrCode error)
{
	if (::IsPalmError (error))
	{
		Err	err = ::ConvertToPalmError (error);
		switch (err)
		{
			case dmErrDatabaseOpen:			return kStr_DmErrDatabaseOpen;
			case memErrNotEnoughSpace:		return kStr_MemErrNotEnoughSpace;
			default:						return kStr_GenericPalmError;
		}
	}
	else if (::IsStdCError (error))
	{
		int	status = ::ConvertToPalmError (error);
		switch (status)
		{
#if defined (ENOENT)
			case ENOENT:		return kStr_FileNotFound;
#endif

#if defined (ENOMEM)
			case ENOMEM:		return kStr_MemFull;
#endif

#if defined (EACCES)
			case EACCES:		return kStr_FileLocked;
#endif

#if defined (ECANCELED)
			case ECANCELED:		return kStr_UserCancel;
#endif

#if defined (EEXIST)
			case EEXIST:		return kStr_DuplicateFileName;
#endif

#if defined (ENFILE)
			case ENFILE:		return kStr_TooManyFilesOpen;
#endif

#if defined (EMFILE)
			case EMFILE:		return kStr_TooManyFilesOpen;
#endif

#if defined (ENOSPC)
			case ENOSPC:		return kStr_DiskFull;
#endif

#if defined (EROFS)
			case EROFS:			return kStr_DiskWriteProtected;
#endif
			default:			return kStr_GenericError;
		}
	}
	else if (::IsEmuError (error))
	{
		switch (error)
		{
			case kError_OutOfMemory:					return kStr_MemFull;

			case kError_BadROM:							return kStr_BadROM;
			case kError_WrongROMForType:				return kStr_WrongROMForType;
			case kError_UnsupportedROM:					return kStr_UnsupportedROM;
			case kError_InvalidDevice:					return kStr_InvalidDevice;
			case kError_InvalidSessionFile:				return kStr_InvalidSession;
			case kError_InvalidConfiguration:			return kStr_InvalidConfiguration;

			case kError_CantDownloadROM_BadBaudRate:	return kStr_GenericError;
			case kError_CantDownloadROM_SerialPortBusy:	return kStr_GenericError;
			case kError_CantDownloadROM_Generic:		return kStr_GenericError;

			case kError_OnlySameType:					return kStr_OnlySameType;
			case kError_OnlyOnePSF:						return kStr_OnlyOnePSF;
			case kError_OnlyOneROM:						return kStr_OnlyOneROM;
			case kError_UnknownType:					return kStr_UnknownType;

			case kError_BadDB_NameNotNULLTerminated:	return kStr_NameNotNULLTerminated;
			case kError_BadDB_NameNotPrintable:			return kStr_NameNotPrintable;
			case kError_BadDB_FileTooSmall:				return kStr_FileTooSmall;
			case kError_BadDB_nextRecordListIDNonZero:	return kStr_nextRecordListIDNonZero;
			case kError_BadDB_ResourceTooSmall:			return kStr_ResourceTooSmall;
			case kError_BadDB_RecordTooSmall:			return kStr_RecordTooSmall;
			case kError_BadDB_ResourceOutOfRange:		return kStr_ResourceOutOfRange;
			case kError_BadDB_RecordOutOfRange:			return kStr_RecordOutOfRange;
			case kError_BadDB_OverlappingResource:		return kStr_OverlappingResource;
			case kError_BadDB_OverlappingRecord:		return kStr_OverlappingRecord;
			case kError_BadDB_ResourceMemError:			return kStr_ResourceMemError;
			case kError_BadDB_RecordMemError:			return kStr_RecordMemError;
			case kError_BadDB_AppInfoMemError:			return kStr_AppInfoMemError;
			case kError_BadDB_DuplicateResource:		return kStr_DuplicateResource;

			default:									return kStr_GenericError;
		}
	}

	return Platform::GetIDForError (error);
}


// ---------------------------------------------------------------------------
//		¥ Errors::GetIDForRecovery
// ---------------------------------------------------------------------------

int Errors::GetIDForRecovery (ErrCode error)
{
	if (::IsPalmError (error))
	{
		switch (::ConvertToPalmError (error))
		{
			default:
				return 0;
		}
	}
	else if (::IsEmuError (error))
	{
		switch (error)
		{
			default:
				return 0;
		}
	}

	return Platform::GetIDForRecovery (error);
}


// ---------------------------------------------------------------------------
//		¥ Errors::GetAppName
// ---------------------------------------------------------------------------
// Get the current application's name.  If the name is not known, return
// "Unknown application" and "unknown application".

void Errors::GetAppName (string& appNameUC, string& appNameLC)
{
	EmuAppInfo	appInfo = EmPatchState::GetCurrentAppInfo ();

	if (strlen (appInfo.fName) > 0)
	{
		appNameUC = appInfo.fName;
		appNameLC = appInfo.fName;
	}
	else
	{
		appNameUC = Platform::GetString (kStr_CurrentAppUC);
		appNameLC = Platform::GetString (kStr_CurrentAppLC);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::GetAppVersion
// ---------------------------------------------------------------------------
// Get the current application's version.  If the version cannot be
// determined, return "(unknown version)".

void Errors::GetAppVersion (string& appVersion)
{
	EmuAppInfo	appInfo = EmPatchState::GetCurrentAppInfo ();

	if (strlen (appInfo.fVersion) > 0)
	{
		appVersion = appInfo.fVersion;
	}
	else
	{
		appVersion = Platform::GetString (kStr_UnknownVersion);
	}
}


// ---------------------------------------------------------------------------
//		¥ Errors::LooksLikeA5Access
// ---------------------------------------------------------------------------

Bool Errors::LooksLikeA5Access (emuptr address, long size, Bool forRead)
{
	UNUSED_PARAM (forRead);

	// The OS sets the high bit of the A5 register when calling PilotMain
	// with a launch code that doesn't allow global variable access.

	emuptr	A5 = gCPU68K->GetRegister (e68KRegID_A5);

	if ((address & 0x80000000) == 0 || (A5 & 0x80000000) == 0)
		return false;

	emuptr	strippedAddress	= address & 0x7FFFFFFF;
	emuptr	strippedA5		= A5 & 0x7FFFFFFF;

	// See if the stripped test address and the real A5 both reside in
	// the dynamic heap.

	if (!EmBankDRAM::ValidAddress (strippedA5, 1) ||
		!EmBankDRAM::ValidAddress (strippedAddress, size))
		return false;

	// Now see if they both point into the same heap.

	const EmPalmHeap*	heap1 = EmPalmHeap::GetHeapByPtr (strippedA5);
	const EmPalmHeap*	heap2 = EmPalmHeap::GetHeapByPtr (strippedAddress);

	if ((heap1 == NULL) || (heap2 == NULL) || (heap1 != heap2))
		return false;

	// Now see if they both point into the same chunk.

	const EmPalmChunk*	chunk1 = heap1->GetChunkBodyContaining (strippedA5);
	const EmPalmChunk*	chunk2 = heap1->GetChunkBodyContaining (strippedAddress);

	if ((chunk1 == NULL) || (chunk2 == NULL) || (chunk1 != chunk2))
		return false;

	// All tests pass, so report that it looks like an attempt
	// to utilize A5 in a verbotten situation.

	return true;
}


// ---------------------------------------------------------------------------
//		¥ PrvLookupField
// ---------------------------------------------------------------------------

void PrvLookupField (const EmFieldLookup* table, size_t offset,
					 const char*& fieldName, const char*& function)
{
	if (!table || table->fType == 2)
		return;

	fieldName = NULL;
	function = NULL;

	// Find the end of the table;

	const EmFieldLookup*	p = table;
	while (p->fType != 2)
		++p;

	// Now walk the table backwards, looking for the first (that is, last)
	// entry with an offset greater than or equal to the one we're looking for.

	while (--p >= table)
	{
		if (offset >= p->fOffset)
		{
			if (p->fType == 1)
			{
				const EmFieldLookup*	newTable	= p->fNextTable;
				size_t					newOffset	= offset - p->fOffset;

				::PrvLookupField (newTable, newOffset, fieldName, function);

				if (fieldName)
				{
					return;
				}
			}

			fieldName	= p->fFieldName;
			function	= p->fFunction;

			return;
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ PrvGetProscribedReason
// ---------------------------------------------------------------------------

string PrvGetProscribedReason (const SystemCallContext& context)
{
	string	result;
	int		reason = ::GetProscribedReason (context);

	switch (reason)
	{
		case kProscribedDocumentedSystemUseOnly:
		case kProscribedUndocumentedSystemUseOnly:
		case kProscribedKernelUseOnly:
		case kProscribedGhost:
		case kProscribedSystemUseOnlyAnyway:
		case kProscribedRare:
			result = Platform::GetString (kStr_SystemUseOnly);
			break;

		case kProscribedObsolete:
			result = Platform::GetString (kStr_Obsolete);
			break;

		default:
			EmAssert (false);
			break;
	}

	return result;
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErr
// ---------------------------------------------------------------------------

EmDeferredErr::EmDeferredErr (void)
{
}

EmDeferredErr::~EmDeferredErr (void)
{
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrAccessCommon
// ---------------------------------------------------------------------------

EmDeferredErrAccessCommon::EmDeferredErrAccessCommon (emuptr address, long size, Bool forRead) :
	EmDeferredErr (),
	fAddress (address),
	fSize (size),
	fForRead (forRead)
{
}

EmDeferredErrAccessCommon::~EmDeferredErrAccessCommon (void)
{
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrLowMemory
// ---------------------------------------------------------------------------

EmDeferredErrLowMemory::EmDeferredErrLowMemory (emuptr address, long size, Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead)
{
}

EmDeferredErrLowMemory::~EmDeferredErrLowMemory (void)
{
}

void EmDeferredErrLowMemory::Do (void)
{
	Errors::ReportErrLowMemory (fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrSystemGlobals
// ---------------------------------------------------------------------------

EmDeferredErrSystemGlobals::EmDeferredErrSystemGlobals (emuptr address, long size, Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead)
{
}

EmDeferredErrSystemGlobals::~EmDeferredErrSystemGlobals (void)
{
}

void EmDeferredErrSystemGlobals::Do (void)
{
	Errors::ReportErrSystemGlobals (fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrScreen
// ---------------------------------------------------------------------------

EmDeferredErrScreen::EmDeferredErrScreen (emuptr address, long size, Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead)
{
}

EmDeferredErrScreen::~EmDeferredErrScreen (void)
{
}

void EmDeferredErrScreen::Do (void)
{
	Errors::ReportErrScreen (fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrHardwareRegisters
// ---------------------------------------------------------------------------

EmDeferredErrHardwareRegisters::EmDeferredErrHardwareRegisters (emuptr address, long size, Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead)
{
}

EmDeferredErrHardwareRegisters::~EmDeferredErrHardwareRegisters (void)
{
}

void EmDeferredErrHardwareRegisters::Do (void)
{
	Errors::ReportErrHardwareRegisters (fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrROM
// ---------------------------------------------------------------------------

EmDeferredErrROM::EmDeferredErrROM (emuptr address, long size, Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead)
{
}

EmDeferredErrROM::~EmDeferredErrROM (void)
{
}

void EmDeferredErrROM::Do (void)
{
	Errors::ReportErrROM (fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrMemMgrStructures
// ---------------------------------------------------------------------------

EmDeferredErrMemMgrStructures::EmDeferredErrMemMgrStructures (emuptr address, long size, Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead)
{
}

EmDeferredErrMemMgrStructures::~EmDeferredErrMemMgrStructures (void)
{
}

void EmDeferredErrMemMgrStructures::Do (void)
{
	Errors::ReportErrMemMgrStructures (fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrMemMgrSemaphore
// ---------------------------------------------------------------------------

EmDeferredErrMemMgrSemaphore::EmDeferredErrMemMgrSemaphore (void) :
	EmDeferredErr ()
{
}

EmDeferredErrMemMgrSemaphore::~EmDeferredErrMemMgrSemaphore (void)
{
}

void EmDeferredErrMemMgrSemaphore::Do (void)
{
	Errors::ReportErrMemMgrSemaphore ();
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrFreeChunk
// ---------------------------------------------------------------------------

EmDeferredErrFreeChunk::EmDeferredErrFreeChunk (emuptr address, long size, Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead)
{
}

EmDeferredErrFreeChunk::~EmDeferredErrFreeChunk (void)
{
}

void EmDeferredErrFreeChunk::Do (void)
{
	Errors::ReportErrFreeChunk (fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrUnlockedChunk
// ---------------------------------------------------------------------------

EmDeferredErrUnlockedChunk::EmDeferredErrUnlockedChunk (emuptr address, long size, Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead)
{
}

EmDeferredErrUnlockedChunk::~EmDeferredErrUnlockedChunk (void)
{
}

void EmDeferredErrUnlockedChunk::Do (void)
{
	Errors::ReportErrUnlockedChunk (fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrLowStack
// ---------------------------------------------------------------------------

EmDeferredErrLowStack::EmDeferredErrLowStack (emuptr stackLow,
											  emuptr stackPointer,
											  emuptr stackHigh,
											  emuptr address,
											  long size,
											  Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead),
	fStackLow (stackLow),
	fStackPointer (stackPointer),
	fStackHigh (stackHigh)
{
}

EmDeferredErrLowStack::~EmDeferredErrLowStack (void)
{
}

void EmDeferredErrLowStack::Do (void)
{
	Errors::ReportErrLowStack (fStackLow, fStackPointer, fStackHigh,
		fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrStackFull
// ---------------------------------------------------------------------------

EmDeferredErrStackFull::EmDeferredErrStackFull (void) :
	EmDeferredErr ()
{
}

EmDeferredErrStackFull::~EmDeferredErrStackFull (void)
{
}

void EmDeferredErrStackFull::Do (void)
{
	Errors::ReportErrStackFull ();
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrObjectCommon
// ---------------------------------------------------------------------------

EmDeferredErrObjectCommon::EmDeferredErrObjectCommon (uint16 id, const EmRect& rect) :
	EmDeferredErr (),
	fID (id),
	fRect (rect)
{
}

EmDeferredErrObjectCommon::~EmDeferredErrObjectCommon (void)
{
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrSizelessObject
// ---------------------------------------------------------------------------

EmDeferredErrSizelessObject::EmDeferredErrSizelessObject (uint16 id, const EmRect& rect) :
	EmDeferredErrObjectCommon (id, rect)
{
}

EmDeferredErrSizelessObject::~EmDeferredErrSizelessObject (void)
{
}

void EmDeferredErrSizelessObject::Do (void)
{
	Errors::ReportErrSizelessObject (fID, fRect);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrOffscreenObject
// ---------------------------------------------------------------------------

EmDeferredErrOffscreenObject::EmDeferredErrOffscreenObject (uint16 id, const EmRect& rect) :
	EmDeferredErrObjectCommon (id, rect)
{
}

EmDeferredErrOffscreenObject::~EmDeferredErrOffscreenObject (void)
{
}

void EmDeferredErrOffscreenObject::Do (void)
{
	Errors::ReportErrOffscreenObject (fID, fRect);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrFormAccess
// ---------------------------------------------------------------------------

EmDeferredErrFormAccess::EmDeferredErrFormAccess (emuptr formAddress,
												  emuptr address,
												  long size,
												  Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead),
	fFormAddress (formAddress)
{
}

EmDeferredErrFormAccess::~EmDeferredErrFormAccess (void)
{
}

void EmDeferredErrFormAccess::Do (void)
{
	Errors::ReportErrFormAccess (fFormAddress, fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrFormObjectAccess
// ---------------------------------------------------------------------------

EmDeferredErrFormObjectAccess::EmDeferredErrFormObjectAccess (emuptr objectAddress,
															  emuptr formAddress,
															  emuptr address,
															  long size,
															  Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead),
	fObjectAddress (objectAddress),
	fFormAddress (formAddress)
{
}

EmDeferredErrFormObjectAccess::~EmDeferredErrFormObjectAccess (void)
{
}

void EmDeferredErrFormObjectAccess::Do (void)
{
	Errors::ReportErrFormObjectAccess (fObjectAddress, fFormAddress, fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrWindowAccess
// ---------------------------------------------------------------------------

EmDeferredErrWindowAccess::EmDeferredErrWindowAccess (emuptr windowAddress,
													  emuptr address,
													  long size,
													  Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead),
	fWindowAddress (windowAddress)
{
}

EmDeferredErrWindowAccess::~EmDeferredErrWindowAccess (void)
{
}

void EmDeferredErrWindowAccess::Do (void)
{
	Errors::ReportErrWindowAccess (fWindowAddress, fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrBitmapAccess
// ---------------------------------------------------------------------------

EmDeferredErrBitmapAccess::EmDeferredErrBitmapAccess (emuptr bitmapAddress,
													  emuptr address,
													  long size,
													  Bool forRead) :
	EmDeferredErrAccessCommon (address, size, forRead),
	fBitmapAddress (bitmapAddress)
{
}

EmDeferredErrBitmapAccess::~EmDeferredErrBitmapAccess (void)
{
}

void EmDeferredErrBitmapAccess::Do (void)
{
	Errors::ReportErrBitmapAccess (fBitmapAddress, fAddress, fSize, fForRead);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrProscribedFunction
// ---------------------------------------------------------------------------

EmDeferredErrProscribedFunction::EmDeferredErrProscribedFunction (const SystemCallContext& context) :
	EmDeferredErr (),
	fContext (context)
{
}

EmDeferredErrProscribedFunction::~EmDeferredErrProscribedFunction (void)
{
}

void EmDeferredErrProscribedFunction::Do (void)
{
	Errors::ReportErrProscribedFunction (fContext);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrStepSpy
// ---------------------------------------------------------------------------

EmDeferredErrStepSpy::EmDeferredErrStepSpy (emuptr writeAddress,
											 int writeBytes,
											 emuptr ssAddress,
											 uint32 ssValue,
											 uint32 newValue) :
	EmDeferredErr (),
	fWriteAddress (writeAddress),
	fWriteBytes (writeBytes),
	fSSAddress (ssAddress),
	fSSValue (ssValue),
	fNewValue (newValue)
{
}

EmDeferredErrStepSpy::~EmDeferredErrStepSpy (void)
{
}

void EmDeferredErrStepSpy::Do (void)
{
	Errors::ReportErrStepSpy (fWriteAddress, fWriteBytes, fSSAddress, fSSValue, fNewValue);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ EmDeferredErrWatchpoint
// ---------------------------------------------------------------------------

EmDeferredErrWatchpoint::EmDeferredErrWatchpoint (emuptr writeAddress,
												 int writeBytes,
												 emuptr watchAddress,
												 uint32 watchBytes) :
	EmDeferredErr (),
	fWriteAddress (writeAddress),
	fWriteBytes (writeBytes),
	fWatchAddress (watchAddress),
	fWatchBytes (watchBytes)
{
}

EmDeferredErrWatchpoint::~EmDeferredErrWatchpoint (void)
{
}

void EmDeferredErrWatchpoint::Do (void)
{
	Errors::ReportErrWatchpoint (fWriteAddress, fWriteBytes, fWatchAddress, fWatchBytes);
}
