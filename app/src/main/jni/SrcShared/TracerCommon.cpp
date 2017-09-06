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

#include "EmCommon.h"
#include "TracerCommon.h"

#include "PreferenceMgr.h"


// ---------------------------------------------------------------------------
//		¥ TracerBase::TracerBase
// ---------------------------------------------------------------------------

TracerBase::TracerBase(void)
{
	tracerRefCounter = 0;
}

// ---------------------------------------------------------------------------
//		¥ TracerBase::~TracerBase
// ---------------------------------------------------------------------------

TracerBase::~TracerBase(void)
{
}
	

// ---------------------------------------------------------------------------
//		¥ TracerBase::GetTracerTypeCount
// ---------------------------------------------------------------------------

unsigned short TracerBase::GetTracerTypeCount (void)
{
	return supportedTypesCount;
}


// ---------------------------------------------------------------------------
//		¥ TracerBase::GetTracerTypeInfo
// ---------------------------------------------------------------------------

TracerTypeInfo* TracerBase::GetTracerTypeInfo (unsigned short index)
{
	if (index < 1 || index > supportedTypesCount)
	{
		return 0;
	}

	return &supportedTypesTable[index-1]; // Types are numbered starting from 1
}

// ---------------------------------------------------------------------------
//		¥ TracerBase::GetCurrentTracerTypeInfo
// ---------------------------------------------------------------------------

TracerTypeInfo* TracerBase::GetCurrentTracerTypeInfo (void)
{
	return GetTracerTypeInfo (runningTracerType);
}


// ---------------------------------------------------------------------------
//		¥ TracerBase::GetCurrentTracerTypeIndex
// ---------------------------------------------------------------------------

unsigned short TracerBase::GetCurrentTracerTypeIndex (void)
{
	return runningTracerType;
}


// ---------------------------------------------------------------------------
//		¥ TracerBase::GetCapsToken
// ---------------------------------------------------------------------------

void TracerBase::GetCapsToken (char* src, char* tag, char* dst, size_t dstSize)
{
	int copiedBytes = 0;
	
	src = strstr(src,tag);

	if (!src) 
	{
		*dst= 0;
		return;
	}
	
	src += strlen(tag);

	while (*src != ',' && *src != 0 && copiedBytes < dstSize-1)
	{
		*dst++ = *src++;
		copiedBytes++;
	}

	*dst= 0;
}

// ---------------------------------------------------------------------------
//		¥ TracerBase::LoadTracerTypeList
// ---------------------------------------------------------------------------

void TracerBase::LoadTracerTypeList (void)
{
	char	tracerCaps[4096];
	size_t	bufferLen = 4096;
	char	*cursor;
	int		i;
	char	buff[2];
	
	// Get last used Type from prefs
	Preference<string> prefLastTracerType (kPrefKeyLastTracerType);
	string lastTypeName = *prefLastTracerType;

	supportedTypesCount = 0;
	supportedTypesTable = 0;

	// Ask PalmTrace for supported tracer Types
	// The expected result is a set of strings terminated by a double 0. Here is a possible one:
	// "Type=tcp,name=Palm Reporter,paramdescr=Target:,paramdefval=localhost,autoconnect=1"
	GetTracerCapabilities (tracerCaps, &bufferLen);

	// The string can't be void
	cursor = tracerCaps;
	if (*cursor == 0)
	{
		return;
	}
	
	while (cursor && *cursor)
	{
		supportedTypesCount++;
		
		// Seek next 0 ; the end marker is a double zero
		cursor += strlen(cursor)+1;
	}
	
	if (supportedTypesCount > 0)
	{
		cursor = tracerCaps;
		supportedTypesTable = new TracerTypeInfo [supportedTypesCount];

		// Set up a tracer Type table in memory
		for (i=0; i<supportedTypesCount; i++)
		{
			GetCapsToken (cursor, "kind=",			supportedTypesTable[i].type,		sizeof(supportedTypesTable[0].type));
			GetCapsToken (cursor, "name=",			supportedTypesTable[i].friendlyName,sizeof(supportedTypesTable[0].friendlyName));
			GetCapsToken (cursor, "paramdescr=",	supportedTypesTable[i].paramDescr,	sizeof(supportedTypesTable[0].paramDescr));
			GetCapsToken (cursor, "paramdefval=",	supportedTypesTable[i].paramDefVal,	sizeof(supportedTypesTable[0].paramDefVal));
			
			GetCapsToken (cursor, "autoconnect=", buff, 2);
			supportedTypesTable[i].autoConnectSupport =  (buff[0] == '1');

			strcpy (supportedTypesTable[i].paramCurVal, supportedTypesTable[i].paramDefVal);
			supportedTypesTable[i].autoConnectCurState = supportedTypesTable[i].autoConnectSupport;
			
			// Seek next string
			cursor = strchr (cursor,0) +1;
		}
	}

	// Set current tracer Type to the last used one
	SetCurrentTracerTypeIndex (LoadTracerPrefs ());
}


// ---------------------------------------------------------------------------
//		¥ TracerBase::LoadTracesPrefs
// ---------------------------------------------------------------------------

unsigned short TracerBase::LoadTracerPrefs (void)
{
	int i;
	Preference<string> prefTracerTypes (kPrefKeyTracerTypes);
	char Type[ sizeof(supportedTypesTable[0].type) ];
	char paramCurVal[ sizeof(supportedTypesTable[0].paramCurVal) ];
	bool autoConnect;
	string s = *prefTracerTypes;
	const char* p= s.c_str ();
	
	p = strchr (p, '<');
	
	while (p)
	{
		// Skip <
		p++;
		
		// Extract Type name
		i = 0;
		while (isalnum(*p))
		{
			Type[i++] = *p;
			p++;
		}
		
		Type[i] = 0;

		// Skip ,
		p++;

		autoConnect = (*p == '1');

		// Skip value + ,
		p++;
		p++;

		// Extract parameter value
		i = 0;
		while (*p != '>')
		{
			paramCurVal[i++] = *p;
			p++;
		}
		paramCurVal[i] = 0;

		// Search Type in supported Types list
		for (i=0; i<supportedTypesCount; i++)
		{
			if (!strcmp (Type, supportedTypesTable[i].type))
			{
				strcpy(supportedTypesTable[i].paramCurVal, paramCurVal);
				supportedTypesTable[i].autoConnectCurState = autoConnect;
			}
		}

		p = strchr (p, '<');
	}

	// Try to find which is the index of the last used tracer Type
	Preference<string> prefLastTracerType (kPrefKeyLastTracerType);
	string lastTypeName = *prefLastTracerType;

	for (i=0; i<supportedTypesCount; i++)
	{
		if (lastTypeName == supportedTypesTable[i].type)
		{
			return i+1;
		}
	}

	// Use nil tracer Type
	return 0;
}


// ---------------------------------------------------------------------------
//		¥ TracerBase::SaveTracerPrefs
// ---------------------------------------------------------------------------

void TracerBase::SaveTracerPrefs (void)
{
	Preference<string> prefTracerTypes (kPrefKeyTracerTypes);
	Preference<string> prefLastTracerType (kPrefKeyLastTracerType);
	string s;
	TracerTypeInfo* t;
	int i;

	char buffer[ sizeof(supportedTypesTable[0].type) + sizeof(supportedTypesTable[0].paramCurVal) + 16 ];

	// Set last used Type in prefs
	if (TracerBase::GetCurrentTracerTypeIndex ())
	{
		s = TracerBase::GetCurrentTracerTypeInfo()->type;
	}
	else
	{
		s = "nil";
	}

	prefLastTracerType = s;

	// Save the current configuration of the tracers
	s = "";

	for (i=1; i<=TracerBase::GetTracerTypeCount(); i++)
	{
		t = TracerBase::GetTracerTypeInfo (i);

		sprintf (buffer, "<%s,%d,%s>", t->type, t->autoConnectCurState, t->paramCurVal);
		
		s += buffer;
	}
	
	prefTracerTypes = s;
}

// ---------------------------------------------------------------------------
//		¥ TracerBase::DisposeTracerTypeList
// ---------------------------------------------------------------------------

void TracerBase::DisposeTracerTypeList (void)
{
	SaveTracerPrefs ();

	delete supportedTypesTable;
	
	supportedTypesTable = 0;
	supportedTypesCount = 0;
}

