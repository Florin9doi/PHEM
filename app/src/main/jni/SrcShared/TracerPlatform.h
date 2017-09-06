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

#ifndef _TRACER_PLATFORM_H_
#define _TRACER_PLATFORM_H_

#include "EmCommon.h"
#include "TracerCommon.h"

class Tracer : public TracerBase
{
public:
						Tracer	(void);
	virtual				~Tracer	(void);
	
	// Tracing subsystem lifecycle
	virtual void		Initialize	(void);
	virtual void		Dispose		(void);

	// HostTrace* counterparts
	virtual void		InitOutputPort	(void);		
	virtual void		CloseOutputPort	(void);		
	virtual void		OutputVT		(unsigned short errModule, const char* formatString, va_list args);
	virtual void		OutputVTL		(unsigned short errModule, const char* formatString, va_list args);
	virtual void		OutputB			(unsigned short errModule, const void* buffer, size_t bufferLen);

	// User feedback
	virtual void			GetLibraryVersionString	(char* buffer, size_t bufferLen);
	virtual unsigned long	GetLibraryVersionNumber (void);
	virtual long			GetConnectionStatus		(void);
	virtual bool			IsLibraryLoaded			(void);

	// Connection control
	virtual void		StopTracer					(void);
	virtual void		GetTracerCapabilities		(char* buffer, size_t* bufferLen);
	virtual void		SetCurrentTracerTypeIndex	(unsigned short tracerType, Bool paramChange = false);

#if PLATFORM_WINDOWS
	// Connection control
	virtual void		CheckPeerStatus 			(void);
#endif

};

extern Tracer	gTracer;

#endif // _TRACER_PLATFORM_H_
