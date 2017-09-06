/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef EMFILEIMPORT_H
#define EMFILEIMPORT_H

#include "EmDirRef.h"			// EmFileRefList
#include "EmTypes.h"			// ErrCode

class EmExgMgr;
class EmExgMgrImportWrapper;
class EmStream;

enum EmFileImportMethod
{
	kMethodBest,
	kMethodHomebrew,
	kMethodExgMgr
};


class EmFileImport
{
	public:
								EmFileImport			(EmStream& stream,
														 EmFileImportMethod method);
								~EmFileImport			(void);

		static ErrCode			LoadPalmFile			(const void*, uint32, EmFileImportMethod, LocalID&);
		static ErrCode			LoadPalmFileList		(const EmFileRefList&, EmFileImportMethod, vector<LocalID>&);

		static ErrCode			InstallExgMgrLib		(void);
		static Bool				CanUseExgMgr			(void);

		// Interface for the dialog to display its stuff

		ErrCode					Continue				(void);
		ErrCode					Cancel					(void);
		Bool					Done					(void);
		long					GetProgress				(void);
		LocalID					GetLocalID				(void);

		void					SetResult				(Err);
		void					SetResult				(ErrCode);
		void					SetDone					(void);

		static long				CalculateProgressMax	(const EmFileRefList&, EmFileImportMethod method);

	private:
		void					IncrementalInstall		(void);

		void					ExgMgrInstallStart		(void);
		void					ExgMgrInstallMiddle		(void);
		void					ExgMgrInstallEnd		(void);
		void					ExgMgrInstallCancel		(void);

		void					HomeBrewInstallStart	(void);
		void					HomeBrewInstallMiddle	(void);
		void					HomeBrewInstallEnd		(void);
		void					HomeBrewInstallCancel	(void);

		void					ValidateStream			(void);
		void					DeleteCurrentDatabase	(void);

	private:
		Bool					fUsingExgMgr;
		Bool					fGotoWhenDone;
		long					fState;
		ErrCode					fError;
		EmStream&				fStream;

		// Fields for ExgMgr installer
		EmExgMgr*				fExgMgrStream;
		EmExgMgrImportWrapper*	fExgMgrImport;
		Bool					fOldAcceptBeamState;

		// Fields for homebrew installer
		void*					fFileBuffer;
		long					fFileBufferSize;
		LocalID					fDBID;
		UInt16					fCardNo;
		DmOpenRef				fOpenID;
		long					fCurrentEntry;
};

#endif /* EMFILEIMPORT_H */
