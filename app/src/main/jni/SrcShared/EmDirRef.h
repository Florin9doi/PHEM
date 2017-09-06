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

#ifndef EmDirRef_h
#define EmDirRef_h

#include <string>
#include <vector>

class EmFileRef;
class EmDirRef;

typedef vector<EmFileRef>	EmFileRefList;
typedef vector<EmDirRef>	EmDirRefList;

class EmDirRef
{
	public:
								EmDirRef		(void);
								EmDirRef		(const EmDirRef&);
								EmDirRef		(const char*);
								EmDirRef		(const string&);
								EmDirRef		(const EmDirRef&, const char*);
								EmDirRef		(const EmDirRef&, const string&);
#if PLATFORM_MAC
								EmDirRef		(const unsigned char*);
								EmDirRef		(const FSSpec&);
								EmDirRef		(AliasHandle);
#endif
								~EmDirRef		(void);

		EmDirRef&				operator=		(const EmDirRef&);

		Bool					IsSpecified		(void) const;
		Bool					Exists			(void) const;
		void					Create			(void) const;

		string					GetName			(void) const;
		EmDirRef				GetParent		(void) const;
		string					GetFullPath		(void) const;

		void					GetChildren		(EmFileRefList*, EmDirRefList*) const;

		bool					operator==		(const EmDirRef&) const;
		bool					operator!=		(const EmDirRef&) const;
		bool					operator<		(const EmDirRef&) const;
		bool					operator>		(const EmDirRef&) const;

		bool					FromPrefString	(const string&);
		string					ToPrefString	(void) const;

		static EmDirRef			GetEmulatorDirectory	(void);
		static EmDirRef			GetPrefsDirectory		(void);

#if PLATFORM_MAC
	public:
		// Needed in order to convert an EmDirRef into something
		// that PowerPlant or Mac OS can deal with.

		FSSpec					GetFSSpec		(void) const;
		AliasHandle				GetAlias		(void) const;

	private:
		void					UpdateAlias		(void);
		void					UpdateSpec		(void);

		AliasHandle				fDirAlias;
		FSSpec					fDirSpec;
#endif

#if PLATFORM_WINDOWS || PLATFORM_UNIX
	private:
		void					MaybeAppendSlash	(void);
		void					MaybeResolveLink	(void);

	private:
		string					fDirPath;
#endif
};

#endif	/* EmDirRef_h */
