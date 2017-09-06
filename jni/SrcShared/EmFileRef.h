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

#ifndef EmFileRef_h
#define EmFileRef_h

#include "EmDirRef.h"			// EmDirRef
#include <vector>

enum EmFileCreator
{
	kFileCreatorNone,
	kFileCreatorEmulator,
	kFileCreatorInstaller,
	kFileCreatorTeachText,
	kFileCreatorCodeWarrior,
	kFileCreatorCodeWarriorProfiler,
	
	kFileCreatorLast
};
typedef vector<EmFileCreator>	EmFileCreatorList;

enum EmFileType
{
	kFileTypeNone,
	kFileTypeApplication,
	kFileTypeROM,
	kFileTypeSession,
	kFileTypeEvents,
	kFileTypePreference,
	kFileTypePalmApp,
	kFileTypePalmDB,
	kFileTypePalmQA,
	kFileTypeText,
	kFileTypePicture,
	kFileTypeSkin,
	kFileTypeProfile,
	kFileTypePalmAll,
	kFileTypeAll,
	
	kFileTypeLast
};

enum EmFileAttr
{
	kFileAttrReadOnly=1,
	kFileAttrHidden=2,
	kFileAttrSystem=4,
	
	kFileAttrLast
};

typedef vector<EmFileType>		EmFileTypeList;

class EmFileRef
{
	public:
								EmFileRef		(void);
								EmFileRef		(const EmFileRef&);
								EmFileRef		(const char*);
								EmFileRef		(const string&);
								EmFileRef		(const EmDirRef&, const char*);
								EmFileRef		(const EmDirRef&, const string&);
#if PLATFORM_MAC
								EmFileRef		(const unsigned char*);
								EmFileRef		(const FSSpec&);
								EmFileRef		(AliasHandle);
#endif
								~EmFileRef		(void);

		EmFileRef&				operator=		(const EmFileRef&);

		Bool					IsSpecified		(void) const;
		Bool					Exists			(void) const;
		void					Delete			(void) const;

		Bool					IsType			(EmFileType) const;
		void					SetCreatorAndType(EmFileCreator creator,
												 EmFileType fileType) const;
		int						GetAttr			(int * attr) const;
		int						SetAttr			(int attr) const;

		string					GetName			(void) const;
		EmDirRef				GetParent		(void) const;
		string					GetFullPath		(void) const;

		bool					operator==		(const EmFileRef&) const;
		bool					operator!=		(const EmFileRef&) const;
		bool					operator<		(const EmFileRef&) const;
		bool					operator>		(const EmFileRef&) const;

		bool					FromPrefString	(const string&);
		string					ToPrefString	(void) const;

		static void				SetEmulatorRef	(const EmFileRef&);
		static EmFileRef		GetEmulatorRef	(void);

#if PLATFORM_MAC
	public:
		// Needed in order to convert an EmFileRef into something
		// that PowerPlant or Mac OS can deal with.

		FSSpec					GetFSSpec		(void) const;
		AliasHandle				GetAlias		(void) const;

	private:
		void					UpdateAlias		(void);
		void					UpdateSpec		(void);

		Bool					IsType			(OSType type, const char* suffix) const;

		AliasHandle				fFileAlias;
		FSSpec					fFileSpec;
#endif

#if PLATFORM_WINDOWS || PLATFORM_UNIX
	private:
		void					MaybePrependCurrentDirectory (void);
		void					MaybeResolveLink	(void);
		void					MaybeNormalize		(void);

	private:
		string					fFilePath;
#endif
};

#endif	/* EmFileRef_h */
