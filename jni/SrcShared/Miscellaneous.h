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

#ifndef _MISCELLANEOUS_H_
#define _MISCELLANEOUS_H_

#include "EmStructs.h"			// DatabaseInfoList

class Chunk;
class EmRect;

class StMemory
{
	public:
		StMemory		(	char*	inPtr = NULL);

		StMemory		(	long	inSize,
							Bool	inClearBytes = false);

		~StMemory		();

		operator char*	()			{ return mPtr; }

		char*	Get		() const	{ return mPtr; }
		Bool	IsOwner	() const	{ return mIsOwner; }
		Bool	IsValid	() const	{ return (mPtr != NULL); }

		void	Adopt	(	char*	inPtr);
		char*	Release	() const;
		void	Dispose	();

	protected:
		char*			mPtr;
		mutable Bool	mIsOwner;

	private:
		StMemory	( const StMemory &inPointerBlock);
		StMemory&	operator = (const StMemory &inPointerBlock);
};


class StMemoryMapper
{
	public:
		StMemoryMapper	(const void* memory, long size);
		~StMemoryMapper	(void);

	private:
		const void*	fMemory;
};

class StWordSwapper
{
	public:
		StWordSwapper (void* memory, long length);
		~StWordSwapper (void);

	private:
		void*	fMemory;
		long	fLength;
};


// ================================================================================
//
//	EmValueChanger
//
//		Use EmValueChanger to temporarily change the value of a variable. The
//		constructor saves the old value and sets the new value. The destructor
//		restores the old value.
//
// ================================================================================

template <class T>
class EmValueChanger
{
	public:
								EmValueChanger(T& variable, T newValue) :
									fVariable(variable),
		  							fOrigValue(variable)
								{
									fVariable = newValue;
								}

								~EmValueChanger()
								{
									fVariable = fOrigValue;
								}
	
	private:
		T&						fVariable;
		T						fOrigValue;
};

void		ValidateFormObjects		(FormPtr frm);
void		CollectOKObjects		(FormPtr frm, vector<UInt16>& okObjects);

Bool		PinRectInRect			(EmRect& inner, const EmRect& outer);

const Bool	kAllDatabases		= false;
const Bool	kApplicationsOnly	= true;

void		GetDatabases			(DatabaseInfoList& appList, Bool applicationsOnly);

Bool		IsExecutable			(UInt32 dbType, UInt32 dbCreator, UInt16 dbAttrs);
Bool		IsVisible				(UInt32 dbType, UInt32 dbCreator, UInt16 dbAttrs);
void 		GetLoadableFileList 	(string directoryName, EmFileRefList& fileList);
void		GetFileContents			(const EmFileRef& file, Chunk& contents);

void		InstallCalibrationInfo	(void);
void		ResetCalibrationInfo	(void);
void		ResetClocks				(void);
void		SetHotSyncUserName		(const char*);

void		SeparateList			(StringList& stringList, string str, char delimiter);

void		RunLengthEncode			(void** srcPP, void** dstPP, long srcBytes, long dstBytes);
void		RunLengthDecode			(void** srcPP, void** dstPP, long srcBytes, long dstBytes);
long		RunLengthWorstSize		(long);

void		GzipEncode				(void** srcPP, void** dstPP, long srcBytes, long dstBytes);
void		GzipDecode				(void** srcPP, void** dstPP, long srcBytes, long dstBytes);
long		GzipWorstSize			(long);

int			CountBits				(uint32 v);
inline int	CountBits				(uint16 v) { return CountBits ((uint32) (uint16) v); }
inline int	CountBits				(uint8 v) { return CountBits ((uint32) (uint8) v); }

inline int	CountBits				(int32 v) { return CountBits ((uint32) (uint32) v); }
inline int	CountBits				(int16 v) { return CountBits ((uint32) (uint16) v); }
inline int	CountBits				(int8 v) { return CountBits ((uint32) (uint8) v); }

inline Bool	IsEven					(uint32 v) { return (v & 1) == 0; }
inline Bool	IsOdd					(uint32 v) { return (v & 1) != 0; }

uint32		NextPowerOf2			(uint32 x);
uint32		DateToDays				(uint32 year, uint32 month, uint32 day);

string		GetLibraryName			(uint16 refNum);

Bool		GetSystemCallContext	(emuptr, SystemCallContext&);

void		GetHostTime				(long* hour, long* min, long* sec);
void		GetHostDate				(long* year, long* month, long* day);

Bool		StartsWith				(const char* s, const char* pattern);
Bool		EndsWith				(const char* s, const char* pattern);
string		Strip					(const char* s, const char*, Bool leading, Bool trailing);
string		Strip					(const string& s, const char*, Bool leading, Bool trailing);
string		ReplaceString			(const string& source,
									 const string& pattern,
									 const string& replacement);
void		FormatInteger			(char* dest, uint32 integer);
string		FormatInteger			(uint32 integer);
string		FormatElapsedTime		(uint32 mSecs);
const char*	LaunchCmdToString		(UInt16 cmd);
void		StackCrawlStrings		(const EmStackFrameList& stackCrawl,
									 StringList& stackCrawlStrings);
string		StackCrawlString 		(const EmStackFrameList& stackCrawl,
									 long maxLen, Bool includeFrameSize,
									 emuptr oldStackLow);

typedef pair <RAMSizeType, string>	MemoryText;
typedef vector <MemoryText>			MemoryTextList;

void		GetMemoryTextList		(MemoryTextList& memoryList);

#endif	// _MISCELLANEOUS_H_
