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

#ifndef _ERRORHANDLING_H_
#define _ERRORHANDLING_H_

#include <vector>
#include <string>

#include "EmCPU68K.h"			// ExceptionNumber
#include "EmDlg.h"				// EmCommonDialogFlags
#include "EmRect.h"				// ExceptionNumber


class SessionFile;
struct SystemCallContext;


// ParamList contains a sequence of key/value pairs.  The even-numbered
// strings are the keys, and the odd-numbered strings are the values.
// The key/value pairs are used in string template substitution.  When
// substitution needs to take place, the keys and values are pulled
// out of the list one pair at a time.  The template string is then
// traversed, with each key being replaced by its value.
//
// Keys are typically of the form "%text" (with the "%" showing up in
// both the key text and the string template text), but could actually
// be in any form that is unique by convention.
//
// To create a parameter list, do something like the following:
//
//		ParamList	paramList;
//		paramList.push_back (string ("%foo"));
//		paramList.push_back (string ("bar"));
//		... repeat as necessary ...

typedef vector<string>	ParamList;


// Values passed to ReportInvalidPC.

enum
{
	kUnmappedAddress,
	kNotInCodeSegment,
	kOddAddress
};

class Errors
{
	public:
		static void				Initialize			(void);
		static void				Reset				(void);
		static void				Save				(SessionFile&);
		static void				Load				(SessionFile&);
		static void				Dispose				(void);


		// These three functions check for the indicated error condition.  If
		// there is an error, they display a "Could not foo because bar." message
		// in a dialog with an OK button.  If an optional recovery string is
		// provided, the message is "Could no foo because bar. Do this." message.
		// If "throwAfter" is true, Errors::Scram is called after the dialog
		// is dismissed.
		//
		// All of these functions bottleneck through DoDialog.
		//
		// "operation" and "recovery" are "kPart_Foo" values.
		// "error" is Mac or Windows error number, or a kError_Foo value.
		// "err" is a Palm OS error number.
		// "p" is a pointer to be tested.

		static void				ReportIfError		(StrCode operation, ErrCode error, StrCode recovery = 0, Bool throwAfter = true);
		static void				ReportIfPalmError	(StrCode operation, Err err, StrCode recovery = 0, Bool throwAfter = true);
		static void				ReportIfNULL		(StrCode operation, void* p, StrCode recovery = 0, Bool throwAfter = true);


		// Report that the indicated error condition occurred.  Messages are generally
		// of the form "<Application> <version> just tried to <foo>. <More explanatory
		// text>. Report this to the program author."

			// Hardware Exceptions

		static void				ReportErrBusError			(emuptr address, long size, Bool forRead);
		static void				ReportErrAddressError		(emuptr address, long size, Bool forRead);
		static void				ReportErrIllegalInstruction	(uint16 opcode);
		static void				ReportErrDivideByZero		(void);
		static void				ReportErrCHKInstruction		(void);
		static void				ReportErrTRAPVInstruction	(void);
		static void				ReportErrPrivilegeViolation	(uint16 opcode);
		static void				ReportErrTrace				(void);
		static void				ReportErrATrap				(uint16 opcode);
		static void				ReportErrFTrap				(uint16 opcode);
		static void				ReportErrTRAPx				(int exceptionNumber);

			// Special cases to hardware exceptions

		static void				ReportErrStorageHeap		(emuptr address, long size, Bool forRead);
		static void				ReportErrNoDrawWindow		(emuptr address, long size, Bool forRead);
		static void				ReportErrNoGlobals			(emuptr address, long size, Bool forRead);
		static void				ReportErrSANE				(void);
		static void				ReportErrTRAP0				(void);
		static void				ReportErrTRAP8				(void);

			// Fatal Poser-detected errors

		static void				ReportErrStackOverflow		(void);
		static void				ReportErrUnimplementedTrap	(const SystemCallContext&);
		static void				ReportErrInvalidRefNum		(const SystemCallContext&);
		static void				ReportErrCorruptedHeap		(ErrCode, emuptr);
		static void				ReportErrInvalidPC			(emuptr address, int reason);

			// Non-fatal Poser-detected errors

		static void				ReportErrLowMemory			(emuptr address, long size, Bool forRead);
		static void				ReportErrSystemGlobals		(emuptr address, long size, Bool forRead);
		static void				ReportErrScreen				(emuptr address, long size, Bool forRead);
		static void				ReportErrHardwareRegisters	(emuptr address, long size, Bool forRead);
		static void				ReportErrROM				(emuptr address, long size, Bool forRead);
		static void				ReportErrMemMgrStructures	(emuptr address, long size, Bool forRead);
		static void				ReportErrMemMgrLeaks		(int leaks);
		static void				ReportErrMemMgrSemaphore	(void);
		static void				ReportErrFreeChunk			(emuptr address, long size, Bool forRead);
		static void				ReportErrUnlockedChunk		(emuptr address, long size, Bool forRead);
		static void				ReportErrLowStack			(emuptr stackLow, emuptr stackPointer, emuptr stackHigh, emuptr address, long size, Bool forRead);
		static void				ReportErrStackFull			(void);
		static void				ReportErrSizelessObject		(uint16 id, const EmRect&);
		static void				ReportErrOffscreenObject	(uint16 id, const EmRect&);
		static void				ReportErrFormAccess			(emuptr formAddress, emuptr address, long size, Bool forRead);
		static void				ReportErrFormObjectAccess	(emuptr objectAddress, emuptr formAddress, emuptr address, long size, Bool forRead);
		static void				ReportErrWindowAccess		(emuptr windowAddress, emuptr address, long size, Bool forRead);
		static void				ReportErrBitmapAccess		(emuptr bitmapAddress, emuptr address, long size, Bool forRead);
		static void				ReportErrProscribedFunction	(const SystemCallContext&);
		static void				ReportErrStepSpy			(emuptr writeAddress, int writeBytes, emuptr ssAddress, uint32 ssValue, uint32 newValue);
		static void				ReportErrWatchpoint			(emuptr writeAddress, int writeBytes, emuptr watchAddress, uint32 watchBytes);

			// Palm OS-detected errors

		static void				ReportErrSysFatalAlert		(const char* appMsg);
		static void				ReportErrDbgMessage			(const char* appMsg);

			// Helper functions for handling reporting of similar errors.

		static void				ReportErrAccessCommon		(StrCode, ExceptionNumber, int flags, emuptr address, long size, Bool forRead);
		static void				ReportErrObjectCommon		(StrCode, ExceptionNumber, int flags, uint16 id, const EmRect&);
		static void				ReportErrOpcodeCommon		(StrCode, ExceptionNumber, int flags, uint16 opcode);
		static void				ReportErrStackCommon		(StrCode, ExceptionNumber, int flags);
		static void				ReportErrCommon				(StrCode, ExceptionNumber, int flags);
		static EmDlgItemID		ReportErrCommPort			(string);
		static EmDlgItemID		ReportErrSockets			(string);

		enum
		{
			kFatal = 0x01,
			kEnterDebuggerFirst = 0x02
		};

		enum EAccessType
		{
			kOKAccess,

			kLowMemAccess,
			kGlobalVarAccess,
			kScreenAccess,
			kMemMgrAccess,
			kLowStackAccess,
			kFreeChunkAccess,
			kUnlockedChunkAccess,

			kUnknownAccess
		};


		// Add a user-supplied substitution rule.  Parameters are deleted after they
		// are used (by making a call to ReplaceParameters), so they need to be
		// re-established after every call to that function.

		static void				SetParameter		(const string& key, const string& value);
		static void				SetParameter		(const string& key, const char* value);
		static void				SetParameter		(const string& key, const unsigned char* value);
		static void				SetParameter		(const string& key, long);
		static void				ClearParameter		(const string& key);
		static void				ClearAllParameters	(void);
		static Bool				SetErrorParameters	(StrCode operation, ErrCode error, StrCode recovery);
		static void				SetStandardParameters	(void);

		// Displays a dialog box using DoDialog, and handles the buttons
		// in a standard fashion.

		static void				HandleDialog		(StrCode messageID,
													 ExceptionNumber excNum,
													 EmCommonDialogFlags flags,
													 Bool enterDebuggerFirst);

		// Displays a dialog box with the given message and according to
		// the given flags.  Returns which button was clicked.

		static EmDlgItemID		DoDialog			(StrCode messageID, EmCommonDialogFlags);
		static EmDlgItemID		DoDialog			(const char* msg, EmCommonDialogFlags, StrCode messageID = -1);


		// Creates and returns a string based on the string template and
		// parameter list.

		static string			ReplaceParameters	(StrCode templateID);
		static string			ReplaceParameters	(StrCode templateID, ParamList& params);
		static string			ReplaceParameters	(const string&, ParamList& params);


		// Convert error numbers into string IDs that describe the error.

		static int				GetIDForError		(ErrCode error);
		static int				GetIDForRecovery	(ErrCode error);


		// Get the name and version of the current application.

		static void				GetAppName			(string& appNameUC,
													 string& appNameLC);
		static void				GetAppVersion		(string& appVersion);

		static Bool				LooksLikeA5Access	(emuptr address, long size, Bool forRead);


		// If an error condition is detected, throws an error number.
		// No message is reported.

		static void				Throw				(ErrCode error);
		static void				ThrowIfError		(ErrCode error);
		static void				ThrowIfPalmError	(Err error);
		static void				ThrowIfStdCError	(int error);
		static void				ThrowIfNULL			(void*);

		// Call this function as a "silent failure" bottleneck.
		// An exception will be thrown that will unwind the stack
		// to some top-level, but nothing will be reported.

		static void				Scram				(void);
};

class EmDeferredErr
{
	public:
								EmDeferredErr					(void);
		virtual					~EmDeferredErr					(void);

		virtual void			Do								(void) = 0;
};

class EmDeferredErrAccessCommon : public EmDeferredErr
{
	public:
								EmDeferredErrAccessCommon		(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrAccessCommon		(void);

	protected:
		emuptr					fAddress;
		long					fSize;
		Bool					fForRead;
};

class EmDeferredErrLowMemory : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrLowMemory			(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrLowMemory			(void);

		virtual void			Do								(void);
};

class EmDeferredErrSystemGlobals : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrSystemGlobals		(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrSystemGlobals		(void);

		virtual void			Do								(void);
};

class EmDeferredErrScreen : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrScreen				(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrScreen			(void);

		virtual void			Do								(void);
};

class EmDeferredErrHardwareRegisters : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrHardwareRegisters	(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrHardwareRegisters	(void);

		virtual void			Do								(void);
};

class EmDeferredErrROM : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrROM				(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrROM				(void);

		virtual void			Do								(void);
};

class EmDeferredErrMemMgrStructures : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrMemMgrStructures	(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrMemMgrStructures	(void);

		virtual void			Do								(void);
};

class EmDeferredErrMemMgrSemaphore : public EmDeferredErr
{
	public:
								EmDeferredErrMemMgrSemaphore	(void);
		virtual					~EmDeferredErrMemMgrSemaphore	(void);

		virtual void			Do								(void);
};

class EmDeferredErrFreeChunk : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrFreeChunk			(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrFreeChunk			(void);

		virtual void			Do								(void);
};

class EmDeferredErrUnlockedChunk : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrUnlockedChunk		(emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrUnlockedChunk		(void);

		virtual void			Do								(void);
};

class EmDeferredErrLowStack : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrLowStack			(emuptr stackLow, emuptr stackPointer, emuptr stackHigh, emuptr address, long size, Bool forRead);
		virtual					~EmDeferredErrLowStack			(void);

		virtual void			Do								(void);

	private:
		emuptr					fStackLow;
		emuptr					fStackPointer;
		emuptr					fStackHigh;
};

class EmDeferredErrStackFull : public EmDeferredErr
{
	public:
								EmDeferredErrStackFull			(void);
		virtual					~EmDeferredErrStackFull			(void);

		virtual void			Do								(void);
};

class EmDeferredErrObjectCommon : public EmDeferredErr
{
	public:
								EmDeferredErrObjectCommon		(uint16 id, const EmRect&);
		virtual					~EmDeferredErrObjectCommon		(void);

	protected:
		uint16					fID;
		EmRect					fRect;
};

class EmDeferredErrSizelessObject : public EmDeferredErrObjectCommon
{
	public:
								EmDeferredErrSizelessObject		(uint16 id, const EmRect&);
		virtual					~EmDeferredErrSizelessObject	(void);

		virtual void			Do								(void);
};

class EmDeferredErrOffscreenObject : public EmDeferredErrObjectCommon
{
	public:
								EmDeferredErrOffscreenObject	(uint16 id, const EmRect&);
		virtual					~EmDeferredErrOffscreenObject	(void);

		virtual void			Do								(void);
};

class EmDeferredErrFormAccess : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrFormAccess			(emuptr formAddress,
																 emuptr address,
																 long size,
																 Bool forRead);
		virtual					~EmDeferredErrFormAccess		(void);

		virtual void			Do								(void);

	protected:
		emuptr					fFormAddress;
};

class EmDeferredErrFormObjectAccess : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrFormObjectAccess	(emuptr objectAddress,
																 emuptr formAddress,
																 emuptr address,
																 long size,
																 Bool forRead);
		virtual					~EmDeferredErrFormObjectAccess	(void);

		virtual void			Do								(void);

	protected:
		emuptr					fObjectAddress;
		emuptr					fFormAddress;
};

class EmDeferredErrWindowAccess : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrWindowAccess		(emuptr windowAddress,
																 emuptr address,
																 long size,
																 Bool forRead);
		virtual					~EmDeferredErrWindowAccess		(void);

		virtual void			Do								(void);

		emuptr					fWindowAddress;
};

class EmDeferredErrBitmapAccess : public EmDeferredErrAccessCommon
{
	public:
								EmDeferredErrBitmapAccess		(emuptr bitmapAddress,
																 emuptr address,
																 long size,
																 Bool forRead);
		virtual					~EmDeferredErrBitmapAccess		(void);

		virtual void			Do								(void);

		emuptr					fBitmapAddress;
};

class EmDeferredErrProscribedFunction : public EmDeferredErr
{
	public:
								EmDeferredErrProscribedFunction	(const SystemCallContext&);
		virtual					~EmDeferredErrProscribedFunction(void);

		virtual void			Do								(void);

		SystemCallContext		fContext;
};

class EmDeferredErrStepSpy : public EmDeferredErr
{
	public:
								EmDeferredErrStepSpy			(emuptr writeAddress,
																 int writeBytes,
																 emuptr ssAddress,
																 uint32 ssValue,
																 uint32 newValue);
		virtual					~EmDeferredErrStepSpy			(void);

		virtual void			Do								(void);

	protected:
		emuptr					fWriteAddress;
		int						fWriteBytes;
		emuptr					fSSAddress;
		uint32					fSSValue;
		uint32					fNewValue;
};

class EmDeferredErrWatchpoint : public EmDeferredErr
{
	public:
								EmDeferredErrWatchpoint			(emuptr writeAddress,
																 int writeBytes,
																 emuptr watchAddress,
																 uint32 watchBytes);
		virtual					~EmDeferredErrWatchpoint		(void);

		virtual void			Do								(void);

	protected:
		emuptr					fWriteAddress;
		int						fWriteBytes;
		emuptr					fWatchAddress;
		uint32					fWatchBytes;
};

#endif /* _ERRORHANDLING_H_ */

