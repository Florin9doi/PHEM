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
#include "StringConversions.h"

#include "EmDirRef.h"			// EmDirRef
#include "EmFileRef.h"			// EmFileRef
#include "EmTransport.h"		// EmTransportDescriptor
#include "Platform.h"			// _stricmp

#include <ctype.h>				// isprint
#include <stdio.h>				// sscanf, sprintf
/* Update for GCC 4 */
#include <string.h>

// Handy macro for performing some common unsigned string compares.

#define CHECK_STRING(s1, s2, v) 		\
	if (_stricmp (s1.c_str(), s2) == 0) \
	{									\
		value = v;						\
		return true;					\
	}



// We convert a lot of numbers into strings; this value is the
// minimum buffer size to hold the converted string.

const int kMaxIntStringSize = 12;	// sign + 4xxxyyyzzz + NULL




// ----------------------------------------------------------------------
//	Our preferences routines need to convert our preference data to and
//	from string format (the format in which the data is stored on disk).
//	Following are a bunch of overloaded routines for converting strings
//	into all the data types we use for preferences.
// ----------------------------------------------------------------------

bool FromString (const string& s, bool& value)
{
	if (s == "0" || _stricmp (s.c_str (), "false") == 0)
		value = false;
	else
		value = true;

	return true;
}


bool FromString (const string& s, char& value)
{
	if (s.size () == 1 && isprint (s[0]))
	{
		value = s[0];
		return true;
	}

	return false;
}


bool FromString (const string& s, signed char& value)
{
	short	temp;
	int result = sscanf (s.c_str (), "%hd", &temp);
	if (result == 1)
		value = (signed char) temp;
	return result == 1;
}


bool FromString (const string& s, unsigned char& value)
{
	unsigned short	temp;
	int result = sscanf (s.c_str (), "%hu", &temp);
	if (result == 1)
		value = (unsigned char) temp;
	return result == 1;
}


bool FromString (const string& s, signed short& value)
{
	int result = sscanf (s.c_str (), "%hd", &value);
	return result == 1;
}


bool FromString (const string& s, unsigned short& value)
{
	int result = sscanf (s.c_str (), "%hu", &value);
	return result == 1;
}


bool FromString (const string& s, signed int& value)
{
	int result = sscanf (s.c_str (), "%d", &value);
	return result == 1;
}


bool FromString (const string& s, unsigned int& value)
{
	int result = sscanf (s.c_str (), "%u", &value);
	return result == 1;
}


bool FromString (const string& s, signed long& value)
{
	int result = sscanf (s.c_str (), "%ld", &value);
	return result == 1;
}


bool FromString (const string& s, unsigned long& value)
{
	int result = sscanf (s.c_str (), "%lu", &value);
	return result == 1;
}


bool FromString (const string& s, string& value)
{
	value = s;
	return true;
}


bool FromString (const string& s, char* value)
{
	strcpy (value, s.c_str ());
	return true;
}


/*
	// Seems to conflict with standard scalar types
bool FromString (const string& s, Bool& value)
{
	bool	temp;
	if (::FromString (s, temp))
	{
		value = (Bool) temp;
		return true;
	}
	
	return false;
}
*/


bool FromString (const string& s, CloseActionType& value)
{
	CHECK_STRING (s, "SaveAlways", kSaveAlways)
	CHECK_STRING (s, "SaveAsk", kSaveAsk)
	CHECK_STRING (s, "SaveNever", kSaveNever)

	return false;
}


bool FromString (const string& s, EmDevice& value)
{
	value = EmDevice (s);

	return value.Supported () != 0;
}


bool FromString (const string& s, EmDirRef& value)
{
	return value.FromPrefString (s);
}


bool FromString (const string& s, EmErrorHandlingOption& value)
{
	CHECK_STRING (s, "ShowDialog",	kShow)
	CHECK_STRING (s, "Continue",	kContinue)
	CHECK_STRING (s, "Quit",		kQuit)
	CHECK_STRING (s, "NextGremlin",	kSwitch)

	return false;
}


bool FromString (const string& s, EmFileRef& value)
{
	return value.FromPrefString (s);
}


bool FromString (const string& s, EmTransportDescriptor& value)
{
	value = EmTransportDescriptor (s);
	return true;
}


// ----------------------------------------------------------------------
//	Our preferences routine need to convert our preference data to and
//	from string format (the format in which the data is stored on disk).
//	Following are a bunch of overloaded routines for converting data
//	(in all the types we use for preferences) into strings.
// ----------------------------------------------------------------------

string ToString (bool value)
{
	if (value)
		return "1";
	return "0";
}


string ToString (char value)
{
	return string (1, value);
}


string ToString (signed char value)
{
	char	buffer[kMaxIntStringSize];
	sprintf (buffer, "%hd", (short) value);
	return buffer;
}


string ToString (unsigned char value)
{
	char	buffer[kMaxIntStringSize];
	sprintf (buffer, "%hu", (unsigned short) value);
	return buffer;
}


string ToString (signed short value)
{
	char	buffer[kMaxIntStringSize];
	sprintf (buffer, "%hd", value);
	return buffer;
}


string ToString (unsigned short value)
{
	char	buffer[kMaxIntStringSize];
	sprintf (buffer, "%hu", value);
	return buffer;
}


string ToString (signed int value)
{
	char	buffer[kMaxIntStringSize];
	sprintf (buffer, "%d", value);
	return buffer;
}


string ToString (unsigned int value)
{
	char	buffer[kMaxIntStringSize];
	sprintf (buffer, "%u", value);
	return buffer;
}


string ToString (signed long value)
{
	char	buffer[kMaxIntStringSize];
	sprintf (buffer, "%ld", value);
	return buffer;
}


string ToString (unsigned long value)
{
	char	buffer[kMaxIntStringSize];
	sprintf (buffer, "%lu", value);
	return buffer;
}


string ToString (const string& value)
{
	return value;
}


string ToString (const char* value)
{
	return string (value);
}


/*

 // Seems to conflict with standard scalar types
string ToString (Bool value)
{
	return ::ToString ((bool) value);
}

*/


string ToString (CloseActionType value)
{
	switch (value)
	{
		case kSaveAlways:		return "SaveAlways";
		case kSaveAsk:			return "SaveAsk";
		case kSaveNever:		return "SaveNever";
	}

	return "";
}


string ToString (const EmDevice& value)
{
	return value.GetIDString ();
}


string ToString (const EmDirRef& value)
{
	return value.ToPrefString ();
}


string ToString (EmErrorHandlingOption value)
{
	switch (value)
	{
		case kShow:			return "ShowDialog";
		case kContinue:		return "Continue";
		case kQuit:			return "Quit";
		case kSwitch:		return "NextGremlin";
	}

	return "";
}


string ToString (const EmFileRef& value)
{
	return value.ToPrefString ();
}


string ToString (const EmTransportDescriptor& value)
{
	return value.GetDescriptor ();
}
