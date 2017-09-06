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

#ifndef _TRACER_COMMON_H_
#define _TRACER_COMMON_H_

#include <stdarg.h>				// va_list
#include <stdlib.h>				// size_t
#include "SessionFile.h"		// SessionFile


// Structures kept in memory
struct TracerTypeInfo
{
	char type			[8];
	char friendlyName	[32];
	char paramDescr		[32];
	char paramDefVal	[16];
	char paramCurVal	[256];
	char paramTmpVal	[256];
	bool autoConnectSupport;
	bool autoConnectCurState;
	bool autoConnectTmpState;
};

class TracerBase
{
public:
							TracerBase	(void);
	virtual					~TracerBase	(void);
	
	// Tracing subsystem lifecycle
	virtual void			Initialize	(void) = 0;
	virtual void 			Dispose		(void) = 0;
	
	// Tracer Type control
	virtual void			SetCurrentTracerTypeIndex	(unsigned short tracerType, Bool paramChange = false) = 0;
	virtual TracerTypeInfo*	GetCurrentTracerTypeInfo	(void);
	virtual TracerTypeInfo*	GetTracerTypeInfo			(unsigned short index);
	virtual unsigned short	GetTracerTypeCount			(void);
	virtual unsigned short	GetCurrentTracerTypeIndex	(void);

	// HostTrace* counterparts
	virtual void			InitOutputPort	(void) = 0;		
	virtual void			CloseOutputPort	(void) = 0;		
	virtual void			OutputVT		(unsigned short errModule, const char* formatString, va_list args) = 0;
	virtual void			OutputVTL		(unsigned short errModule, const char* formatString, va_list args) = 0;
	virtual void			OutputB			(unsigned short errModule, const void* buffer, size_t bufferLen) = 0;

	// User feedback
	virtual bool			IsLibraryLoaded			(void) = 0;
	virtual void			GetLibraryVersionString	(char* buffer, size_t bufferLen) = 0;
	virtual unsigned long	GetLibraryVersionNumber	(void) { return 0; };
	virtual long			GetConnectionStatus		(void) = 0;
	
	virtual void			LoadTracerTypeList		(void);
	virtual void			DisposeTracerTypeList	(void);

	virtual void			StopTracer				(void) = 0;

	virtual void			SaveTracerPrefs			(void);
	virtual unsigned short	LoadTracerPrefs			(void);

	virtual void 			GetTracerCapabilities	(char* buffer, size_t* bufferLen) = 0;
	virtual void			GetCapsToken			(char* src, char* tag, char* dst, size_t dstSize);

protected:
	unsigned long	tracerRefCounter;		// Number of referenced subsystem users
	unsigned short	supportedTypesCount;
	unsigned short	runningTracerType;
	unsigned short	previousTracerType;
	TracerTypeInfo*	supportedTypesTable;
};

#endif // _TRACER_COMMON_H_



