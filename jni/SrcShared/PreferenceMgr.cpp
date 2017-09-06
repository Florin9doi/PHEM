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
#include "PreferenceMgr.h"

#include "EmCPU.h"				// gCPU
#include "EmHAL.h"				// EmHAL::GetLineDriverState
#include "EmMapFile.h"			// EmMapFile
#include "EmSession.h"			// EmSessionStopper
#include "EmTransportSerial.h"	// EmTransportSerial
#include "EmTransportSocket.h"	// EmTransportSocket
#include "EmTransportUSB.h"		// EmTransportUSB
#include "Platform.h"			// _stricmp
#include "StringConversions.h"	// ToString, FromString

#include <algorithm>			// find()
#include <ctype.h>				// isdigit
/* Update for GCC 4 */
#include <string.h>

Preferences*			gPrefs;
EmulatorPreferences*	gEmuPrefs;
omni_mutex				Preferences::fgPrefsMutex;


// Define all the keys

#define DEFINE_PREF_KEYS(name, type, init)	const char* kPrefKey##name = #name;
FOR_EACH_PREF(DEFINE_PREF_KEYS)

static void PrvFilterFileRefList (PrefKeyType key);
static EmTransportType PrvGetTransportTypeFromPortName (const char* portName);

static Bool PrvFirstBeginsWithSecond (const string& first, const string& second)
{
	// Return true if the first string starts with the second string, and
	// if the point where they both end in common is either the end of
	// the first string or a delimiter indicating that a sub-part follows.

	return
		(first.size () >= second.size ()) &&
		(memcmp (first.c_str (), second.c_str (), second.size ()) == 0) &&
		((first.size () == second.size ()) ||
		 (first[second.size ()] == '.') ||
		 (first[second.size ()] == '['));
}


// ----------------------------------------------------------------------
//	* BasePreference
//
//	Clients of the prefernces sub-system access the data via Preference
//	objects.  These objects are created from the Preference class,
//	which the client templatizes based on the data type in which they
//	want the data returned.
//
//	Preference descends from BasePreference, which implements generic
//	functionality.	If we were to put this functionality into Preference
//	itself, we'd have massive unnecessary code bloat.
// ----------------------------------------------------------------------

/***********************************************************************
 *
 * FUNCTION:	BasePreference constructor
 *
 * DESCRIPTION: Initializes all the data members.
 *
 * PARAMETERS:	name - name of the key used to fetch the data.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

BasePreference::BasePreference (PrefKeyType name, bool acquireLock) :
	fName (name),
	fLoaded (false),
	fChanged (false),
	fAcquireLock (acquireLock)
{
}

BasePreference::BasePreference (long index, bool acquireLock) :
	fName (::ToString (index)),
	fLoaded (false),
	fChanged (false),
	fAcquireLock (acquireLock)
{
}


/***********************************************************************
 *
 * FUNCTION:	BasePreference destructor
 *
 * DESCRIPTION: Destroys all the data members.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

BasePreference::~BasePreference (void)
{
}


/***********************************************************************
 *
 * FUNCTION:	BasePreference::Load
 *
 * DESCRIPTION: Lock the preference data and attempt to load the item
 *				specified by the key (specified to the constructor).
 *				If the data was able to be loaded (which pretty much
 *				means that we were able to convert it into the correct
 *				type, set fLoaded to true.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void BasePreference::Load (void)
{
	if (fAcquireLock)
	{
		// Lock this here.	In case DoLoad sets the prefix, we don't want
		// any other threads accidently using or changing that prefix.

		omni_mutex_lock lock (Preferences::fgPrefsMutex);

		if (this->DoLoad ())
		{
			fLoaded = true;
		}
	}
	else
	{
		if (this->DoLoad ())
		{
			fLoaded = true;
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	BasePreference::Save
 *
 * DESCRIPTION: If the data has been change (via operator=()), lock the
 *				preference data and store the entry back out to it.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void BasePreference::Save (void)
{
	if (fChanged)
	{
		// Lock this here.	In case DoSave sets the prefix, we don't want
		// any other threads accidently using or chaning that prefix.

		if (fAcquireLock)
		{
			omni_mutex_lock lock (Preferences::fgPrefsMutex);

			this->DoSave ();
			fChanged = false;
		}
		else
		{
			this->DoSave ();
			fChanged = false;
		}
	}
}


// ----------------------------------------------------------------------
//	* Preference<T>
//
//	Clients of the prefernces sub-system access the data via Preference
//	objects.  These objects are created from the Preference class,
//	which the client templatizes based on the data type in which they
//	want the data returned.
//
//	Once an object has been created, the preference value can be fetched
//	with the * operator:
//
//		Preference<bool>	pref(kPrefKeySomeSetting);
//		if (*pref)
//			...
//
//	The value can also be fetched with the -> operator:
//
//		Preference<RGBType> pref(kPrefKeySomeColor);
//		if (pref->fRed == 0 && pref->fGreen == 0 && pref->fBlue == 0)
//			...
//
//	The value can be changed by simply assigning to the Preference object:
//
//		Preference<string>	pref(kPrefKeySomeString);
//		pref = string ("new string value");
//
//	You can find out if there is a valid value for the desired preference
//	by calling Loaded():
//
//		Preference<bool>		pref1(kPrefKeySomeSetting);
//		pref1 = true;
//
//		Preference<GremlinInfo> pref2(kPrefKeySomeSetting);
//		if (pref2.Loaded ())
//			... fails because we can't convert a bool to a GremlinInfo...
//
//		Preference<bool>		pref3("bogus undefined key);
//		if (pref3.Loaded ())
//			... fails because we key was not found...
//
// ----------------------------------------------------------------------


/***********************************************************************
 *
 * FUNCTION:	Preference<T> constructor
 *
 * DESCRIPTION: Calls BasePreference to initialize the base members,
 *				then calls Load() to load the data.
 *
 * PARAMETERS:	name - name of the key used to fetch the data.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

template <class T>
Preference<T>::Preference (PrefKeyType name, bool acquireLock) :
	BasePreference (name, acquireLock),
	fValue (T())
{
	this->Load ();
}

template <class T>
Preference<T>::Preference (long index, bool acquireLock) :
	BasePreference (index, acquireLock),
	fValue (T())
{
	this->Load ();
}


	
/***********************************************************************
 *
 * FUNCTION:	Preference<T> destructor
 *
 * DESCRIPTION: Ensures that any changes are flushed back to the
 *				preference collection before destructing the base class.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

template <class T>
Preference<T>::~Preference (void)
{
	this->Save ();
}


/***********************************************************************
 *
 * FUNCTION:	Preference<T>::DoLoad
 *
 * DESCRIPTION: Virtual function responsible for the actual conversion
 *				of string information into natural data types.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	True if the function could be loaded and converted.
 *				False otherwise.
 *
 ***********************************************************************/

template <class T>
bool Preference<T>::DoLoad (void)
{
	string	value;

	if (gPrefs->GetPref (fName, value) && ::FromString (value, fValue))
	{
		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	Preference<T>::DoSave
 *
 * DESCRIPTION: Virtual function responsible for the actual conversion
 *				of our natural data types into strings.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

template <class T>
void Preference<T>::DoSave (void)
{
	gPrefs->SetPref (fName, ::ToString (fValue));
}


// ----------------------------------------------------------------------
//	Instantiations of Preference class for all non-compound types.
// ----------------------------------------------------------------------

template class Preference<bool>;
template class Preference<char>;
template class Preference<signed char>;
template class Preference<unsigned char>;
template class Preference<signed short>;
template class Preference<unsigned short>;
template class Preference<signed int>;
template class Preference<unsigned int>;
template class Preference<signed long>;
template class Preference<unsigned long>;

//template class Preference<EmDirRef>;
template class Preference<EmFileRef>;
template class Preference<string>;

//template class Preference<Bool>;
template class Preference<CloseActionType>;
template class Preference<EmDevice>;
template class Preference<EmErrorHandlingOption>;

/* Templates needed for GCC 4 */
template class Preference<EmTransportDescriptor>;
template class Preference<EmDirRef>;

// ----------------------------------------------------------------------
//	Specializations of Preference class for compound types.
//
//		template Preference<Configuration>;
//		template Preference<DatabaseInfo>;
//		template Preference<DatabaseInfoList>;
//		template Preference<EmFileRefList>;
//		template Preference<GremlinInfo>;
//		template Preference<PointType>;
//		template Preference<EmPoint>;
//		template Preference<RGBType>;
//		template Preference<SkinNameList>;
// ----------------------------------------------------------------------

template <>
bool Preference<Configuration>::DoLoad (void)
{
	bool	loaded = false;
	string	value;

	gPrefs->PushPrefix (fName);

	if (gPrefs->GetPref ("fDeviceType", value)	&& ::FromString (value, fValue.fDevice) &&
		gPrefs->GetPref ("fRAMSize", value) 	&& ::FromString (value, fValue.fRAMSize) &&
		gPrefs->GetPref ("fROMFile", value) 	&& ::FromString (value, fValue.fROMFile))
	{
		loaded = true;
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<Configuration>::DoSave (void)
{
	gPrefs->PushPrefix (fName);

	gPrefs->SetPref ("fDeviceType", 	::ToString (fValue.fDevice));
	gPrefs->SetPref ("fRAMSize",		::ToString (fValue.fRAMSize));
	gPrefs->SetPref ("fROMFile",		::ToString (fValue.fROMFile));

	gPrefs->PopPrefix ();
}

// ----------------------------------------------------------------------

template <>
bool Preference<DatabaseInfo>::DoLoad (void)
{
	bool	loaded = false;
	string	value;

	gPrefs->PushPrefix (fName);

	if (gPrefs->GetPref ("creator", value)	&& ::FromString (value, fValue.creator) &&
		gPrefs->GetPref ("type", value) 	&& ::FromString (value, fValue.type) &&
		gPrefs->GetPref ("version", value)	&& ::FromString (value, fValue.version) &&
		gPrefs->GetPref ("dbID", value) 	&& ::FromString (value, fValue.dbID) &&
		gPrefs->GetPref ("cardNo", value)	&& ::FromString (value, fValue.cardNo) &&
		gPrefs->GetPref ("modDate", value)	&& ::FromString (value, fValue.modDate) &&
		gPrefs->GetPref ("dbAttrs", value)	&& ::FromString (value, fValue.dbAttrs) &&
		gPrefs->GetPref ("name", value) 	&& ::FromString (value, fValue.name))
	{
		loaded = true;

		// Workaround a bug in older Posers.  I forgot to write out the dbName,
		// so not all preference files have that field.  If it's there, read it.
		// Otherwise, use application name.
		//
		// !!! May instead want to set dbName to "\0" and try to fix it up later.
		if (gPrefs->GetPref ("dbName", value)	&& ::FromString (value, fValue.dbName))
		{
		}
		else
		{
			strcpy (fValue.dbName, fValue.name);
		}
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<DatabaseInfo>::DoSave (void)
{
	gPrefs->PushPrefix (fName);

	gPrefs->SetPref ("creator", 	::ToString (fValue.creator));
	gPrefs->SetPref ("type",		::ToString (fValue.type));
	gPrefs->SetPref ("version", 	::ToString (fValue.version));
	gPrefs->SetPref ("dbID",		::ToString (fValue.dbID));
	gPrefs->SetPref ("cardNo",		::ToString (fValue.cardNo));
	gPrefs->SetPref ("modDate", 	::ToString (fValue.modDate));
	gPrefs->SetPref ("dbAttrs", 	::ToString (fValue.dbAttrs));
	gPrefs->SetPref ("name",		::ToString (fValue.name));
	gPrefs->SetPref ("dbName",		::ToString (fValue.dbName));

	gPrefs->PopPrefix ();
}

// ----------------------------------------------------------------------

template <>
bool Preference<DatabaseInfoList>::DoLoad (void)
{
	bool	loaded = true;
	string	value;

	gPrefs->PushPrefix (fName);

	long	ii = 0;
	while (1)
	{
		Preference<DatabaseInfo> pref (ii++, false);
		if (!pref.Loaded ())
			break;

		fValue.push_back (*pref);
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<DatabaseInfoList>::DoSave (void)
{
	gPrefs->DeletePref (fName);

	gPrefs->PushPrefix (fName);

	long	ii = 0;
	DatabaseInfoList::iterator	iter = fValue.begin ();
	while (iter != fValue.end ())
	{
		Preference<DatabaseInfo> pref (ii++, false);
		pref = *iter;

		++iter;
	}

	gPrefs->PopPrefix ();
}

// ----------------------------------------------------------------------

template <>
bool Preference<EmFileRefList>::DoLoad (void)
{
	bool	loaded = true;
	gPrefs->PushPrefix (fName);

	long	ii = 0;
	while (1)
	{
		Preference<EmFileRef>	pref (ii++, false);
		if (!pref.Loaded ())
			break;

		fValue.push_back (*pref);
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<EmFileRefList>::DoSave (void)
{
	gPrefs->DeletePref (fName);

	gPrefs->PushPrefix (fName);

	long	ii = 0;
	EmFileRefList::iterator	iter = fValue.begin ();
	while (iter != fValue.end ())
	{
		Preference<EmFileRef>	pref (ii++, false);
		pref = *iter;

		++iter;
	}

	gPrefs->PopPrefix ();
}

// ----------------------------------------------------------------------

template <>
bool Preference<GremlinInfo>::DoLoad (void)
{
	bool	loaded = false;
	string	value;

	gPrefs->PushPrefix (fName);

	if (gPrefs->GetPref ("fNumber", value)					&& ::FromString (value, fValue.fNumber) &&
		gPrefs->GetPref ("fSteps", value)					&& ::FromString (value, fValue.fSteps) &&
		gPrefs->GetPref ("fSaveFrequency", value)			&& ::FromString (value, fValue.fSaveFrequency))
	{
		loaded = true;
	}

	if (loaded)
	{
		Preference<DatabaseInfoList> pref ("fAppList", false);
		if (pref.Loaded ())
		{
			fValue.fAppList = *pref;
		}
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<GremlinInfo>::DoSave (void)
{
	gPrefs->PushPrefix (fName);

	gPrefs->SetPref ("fNumber", 				::ToString (fValue.fNumber));
	gPrefs->SetPref ("fSteps",					::ToString (fValue.fSteps));
	gPrefs->SetPref ("fSaveFrequency",			::ToString (fValue.fSaveFrequency));

	// Save the fAppList collection.
	{
		Preference<DatabaseInfoList> pref ("fAppList", false);
		pref = fValue.fAppList;
	}

	gPrefs->PopPrefix ();
}

// ----------------------------------------------------------------------

template <>
bool Preference<HordeInfo>::DoLoad (void)
{
	bool	loaded = false;
	string	value;

	gPrefs->PushPrefix (fName);

	// If the old fields exist, convert them to new fields and delete them.
	if (gPrefs->GetPref ("fDepthBound", value)			&& ::FromString (value, fValue.fSwitchDepth) &&
		gPrefs->GetPref ("fSaveFrequency", value)		&& ::FromString (value, fValue.fSaveFrequency) &&
		gPrefs->GetPref ("fMaxDepth", value)			&& ::FromString (value, fValue.fMaxDepth))
	{
		fValue.OldToNew ();	// Transfer the old fields to the new fields.

		gPrefs->SetPref ("fDepthSwitch",				::ToString (fValue.fDepthSwitch));
		gPrefs->SetPref ("fDepthSave",					::ToString (fValue.fDepthSave));
		gPrefs->SetPref ("fDepthStop",					::ToString (fValue.fDepthStop));
		gPrefs->SetPref ("fCanSwitch",					::ToString (fValue.fCanSwitch));
		gPrefs->SetPref ("fCanSave",					::ToString (fValue.fCanSave));
		gPrefs->SetPref ("fCanStop",					::ToString (fValue.fCanStop));

		gPrefs->DeletePref ("fDepthBound");
		gPrefs->DeletePref ("fSaveFrequency");
		gPrefs->DeletePref ("fMaxDepth");
	}

	if (gPrefs->GetPref ("fStartNumber", value)			&& ::FromString (value, fValue.fStartNumber) &&
		gPrefs->GetPref ("fStopNumber", value)			&& ::FromString (value, fValue.fStopNumber) &&
		gPrefs->GetPref ("fDepthSwitch", value)			&& ::FromString (value, fValue.fDepthSwitch) &&
		gPrefs->GetPref ("fDepthSave", value)			&& ::FromString (value, fValue.fDepthSave) &&
		gPrefs->GetPref ("fDepthStop", value)			&& ::FromString (value, fValue.fDepthStop) &&
		gPrefs->GetPref ("fCanSwitch", value)			&& ::FromString (value, fValue.fCanSwitch) &&
		gPrefs->GetPref ("fCanSave", value)				&& ::FromString (value, fValue.fCanSave) &&
		gPrefs->GetPref ("fCanStop", value)				&& ::FromString (value, fValue.fCanStop) &&
		gPrefs->GetPref ("fFirstLaunchedAppName", value)	&& ::FromString (value, fValue.fFirstLaunchedAppName))
	{
		loaded = true;
		fValue.NewToOld ();	// Transfer the new fields to the old fields.
	}

	if (loaded)
	{
		Preference<DatabaseInfoList> pref ("fAppList", false);
		if (pref.Loaded ())
		{
			fValue.fAppList = *pref;
		}
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<HordeInfo>::DoSave (void)
{
	gPrefs->PushPrefix (fName);

	gPrefs->SetPref ("fStartNumber", 				::ToString (fValue.fStartNumber));
	gPrefs->SetPref ("fStopNumber",					::ToString (fValue.fStopNumber));
	gPrefs->SetPref ("fDepthSwitch",				::ToString (fValue.fDepthSwitch));
	gPrefs->SetPref ("fDepthSave",					::ToString (fValue.fDepthSave));
	gPrefs->SetPref ("fDepthStop",					::ToString (fValue.fDepthStop));
	gPrefs->SetPref ("fCanSwitch",					::ToString (fValue.fCanSwitch));
	gPrefs->SetPref ("fCanSave",					::ToString (fValue.fCanSave));
	gPrefs->SetPref ("fCanStop",					::ToString (fValue.fCanStop));
	gPrefs->SetPref ("fFirstLaunchedAppName",		::ToString (fValue.fFirstLaunchedAppName));

	// Save the fAppList collection.
	{
		Preference<DatabaseInfoList> pref ("fAppList", false);
		pref = fValue.fAppList;
	}


	gPrefs->PopPrefix ();
}


// ----------------------------------------------------------------------

	// It's odd that we have to provide explicit specializations of the
	// constructor and destructor for Preference<PointType>.  I think it
	// has something to do with the fact that PointType doesn't have a
	// constructor of its own.

template <>
Preference<PointType>::Preference (PrefKeyType name, bool acquireLock) :
	BasePreference (name, acquireLock)
	//,	fValue ()
{
	this->Load ();
}

template <>
Preference<PointType>::Preference (long index, bool acquireLock) :
	BasePreference (index, acquireLock)
	//,	fValue ()
{
	this->Load ();
}

template <>
Preference<PointType>::~Preference (void)
{
	this->Save ();
}

template <>
bool Preference<PointType>::DoLoad (void)
{
	bool	loaded = false;
	string	value;

	gPrefs->PushPrefix (fName);

	if (gPrefs->GetPref ("x", value)	&& ::FromString (value, fValue.x) &&
		gPrefs->GetPref ("y", value)	&& ::FromString (value, fValue.y))
	{
		loaded = true;
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<PointType>::DoSave (void)
{
	gPrefs->PushPrefix (fName);

	gPrefs->SetPref ("x",	::ToString (fValue.x));
	gPrefs->SetPref ("y",	::ToString (fValue.y));

	gPrefs->PopPrefix ();
}

// ----------------------------------------------------------------------

	// It's odd that we have to provide explicit specializations of the
	// constructor and destructor for Preference<EmPoint>.

template <>
Preference<EmPoint>::Preference (PrefKeyType name, bool acquireLock) :
	BasePreference (name, acquireLock),
	fValue ()
{
	this->Load ();
}

template <>
Preference<EmPoint>::Preference (long index, bool acquireLock) :
	BasePreference (index, acquireLock),
	fValue ()
{
	this->Load ();
}

template <>
Preference<EmPoint>::~Preference (void)
{
	this->Save ();
}

template <>
bool Preference<EmPoint>::DoLoad (void)
{
	bool	loaded = false;
	string	value;

	gPrefs->PushPrefix (fName);

	if (gPrefs->GetPref ("x", value)	&& ::FromString (value, fValue.fX) &&
		gPrefs->GetPref ("y", value)	&& ::FromString (value, fValue.fY))
	{
		loaded = true;
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<EmPoint>::DoSave (void)
{
	gPrefs->PushPrefix (fName);

	gPrefs->SetPref ("x",	::ToString (fValue.fX));
	gPrefs->SetPref ("y",	::ToString (fValue.fY));

	gPrefs->PopPrefix ();
}

// ----------------------------------------------------------------------

template <>
Preference<RGBType>::Preference (PrefKeyType name, bool acquireLock) :
	BasePreference (name, acquireLock),
	fValue ()
{
	this->Load ();
}

template <>
Preference<RGBType>::Preference (long index, bool acquireLock) :
	BasePreference (index, acquireLock),
	fValue ()
{
	this->Load ();
}

template <>
Preference<RGBType>::~Preference (void)
{
	this->Save ();
}

template <>
bool Preference<RGBType>::DoLoad (void)
{
	bool	loaded = false;
	string	value;

	gPrefs->PushPrefix (fName);

	if (gPrefs->GetPref ("red", value)		&& ::FromString (value, fValue.fRed) &&
		gPrefs->GetPref ("green", value)	&& ::FromString (value, fValue.fGreen) &&
		gPrefs->GetPref ("blue", value) 	&& ::FromString (value, fValue.fBlue))
	{
		loaded = true;
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<RGBType>::DoSave (void)
{
	gPrefs->PushPrefix (fName);

	gPrefs->SetPref ("red", 	::ToString (fValue.fRed));
	gPrefs->SetPref ("green",	::ToString (fValue.fGreen));
	gPrefs->SetPref ("blue",	::ToString (fValue.fBlue));

	gPrefs->PopPrefix ();
}


// ----------------------------------------------------------------------
// Also SkinNameList and PortNameList.

template <>
void Preference<StringList>::DoSave (void)
{
//	gPrefs->DeletePref (fName);

	gPrefs->PushPrefix (fName);

	long	ii = 0;
	StringList::iterator	iter = fValue.begin ();
	while (iter != fValue.end ())
	{
		Preference<string>	pref (ii++, false);
		pref = *iter;

		++iter;
	}

	gPrefs->PopPrefix ();
}


template <>
bool Preference<StringList>::DoLoad (void)
{
	bool	loaded = true;
	gPrefs->PushPrefix (fName);

	long	ii = 0;
	while (1)
	{
		Preference<string>	pref (ii, false);
		if (!pref.Loaded ())
			break;

		fValue.push_back (*pref);

		++ii;
	}

	gPrefs->PopPrefix ();

	return loaded;
}

// ----------------------------------------------------------------------

template <>
Preference<SlotInfoType>::Preference (PrefKeyType name, bool acquireLock) :
	BasePreference (name, acquireLock),
	fValue ()
{
	this->Load ();
}

template <>
Preference<SlotInfoType>::Preference (long index, bool acquireLock) :
	BasePreference (index, acquireLock),
	fValue ()
{
	this->Load ();
}

template <>
Preference<SlotInfoType>::~Preference (void)
{
	this->Save ();
}

template <>
bool Preference<SlotInfoType>::DoLoad (void)
{
	bool	loaded = false;
	string	value;

	gPrefs->PushPrefix (fName);

	if (gPrefs->GetPref ("fSlotNumber", value)		&& ::FromString (value, fValue.fSlotNumber) &&
		gPrefs->GetPref ("fSlotOccupied", value)	&& ::FromString (value, fValue.fSlotOccupied) &&
		gPrefs->GetPref ("fSlotRoot", value)		&& ::FromString (value, fValue.fSlotRoot))
	{
		loaded = true;
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<SlotInfoType>::DoSave (void)
{
	gPrefs->PushPrefix (fName);

	gPrefs->SetPref ("fSlotNumber", 	::ToString (fValue.fSlotNumber));
	gPrefs->SetPref ("fSlotOccupied",	::ToString (fValue.fSlotOccupied));
	gPrefs->SetPref ("fSlotRoot",		::ToString (fValue.fSlotRoot));

	gPrefs->PopPrefix ();
}


// ----------------------------------------------------------------------

template <>
bool Preference<SlotInfoList>::DoLoad (void)
{
	bool	loaded = true;
	gPrefs->PushPrefix (fName);

	long	ii = 0;
	while (1)
	{
		Preference<SlotInfoType>	pref (ii++, false);
		if (!pref.Loaded ())
			break;

		fValue.push_back (*pref);
	}

	gPrefs->PopPrefix ();

	return loaded;
}

template <>
void Preference<SlotInfoList>::DoSave (void)
{
	gPrefs->DeletePref (fName);

	gPrefs->PushPrefix (fName);

	long	ii = 0;
	SlotInfoList::iterator	iter = fValue.begin ();
	while (iter != fValue.end ())
	{
		Preference<SlotInfoType>	pref (ii++, false);
		pref = *iter;

		++iter;
	}

	gPrefs->PopPrefix ();
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	Preferences::Preferences
 *
 * DESCRIPTION: Constructor.  Does nothing but construct data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Preferences::Preferences (void) :
	fPreferences ()
{
	if (gPrefs == NULL)
		gPrefs = this;
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::~Preferences
 *
 * DESCRIPTION: Destructor.  Does nothing but destruct data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Preferences::~Preferences (void)
{
	if (gPrefs == this)
		gPrefs = NULL;
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::Load
 *
 * DESCRIPTION: Loads the preferences from the storage medium.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::Load (void)
{
	StringStringMap	mapData;

	if (!this->ReadPreferences (mapData))
		return;

	StringStringMap::iterator	iter = mapData.begin ();
	while (iter != mapData.end ())
	{
		string	key = iter->first;
		string	value = iter->second;

		this->SetPref (key, value);

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::Save
 *
 * DESCRIPTION: Saves the preferences to the storage medium.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::Save (void)
{
	this->StripUnused ();

	this->WritePreferences (fPreferences);
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::GetPref
 *
 * DESCRIPTION: Returns the specified preference.  The given key is
 *				prepended with any prefixes, and the resulting key is
 *				used to look up the value.
 *
 * PARAMETERS:	key - the key used to look up the preference's value.
 *
 *				value - the found value (if any).
 *
 * RETURNED:	True if the key/value could be found, false otherwise.
 *
 ***********************************************************************/

bool Preferences::GetPref (const string& key, string& value)
{
	// Locking done in Preference::DoLoad.
//	omni_mutex_lock lock (fgPrefsMutex);

	string	fullKey = gPrefs->ExpandKey (key);

	iterator	iter = fPreferences.find (fullKey);
	if (iter == fPreferences.end ())
		return false;

	value = iter->second;
	return true;
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::SetPref
 *
 * DESCRIPTION: Store the given key/value pair into our collection.  If
 *				an entry with the given key already exists, its value
 *				is updated with the specified one.
 *
 *				If the value is changed or newly added, then send out
 *				notification to anyone registered for this preference.
 *
 * PARAMETERS:	key - the key used to look up the preference.
 *
 *				value - the value to associate with the key.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::SetPref (const string& key, const string& value)
{
	// Locking done in Preference::DoSave.
//	omni_mutex_lock lock (fgPrefsMutex);

	string	fullKey = gPrefs->ExpandKey (key);
	Bool	doNotify = false;

	iterator	iter = fPreferences.find(fullKey);
	if (iter == fPreferences.end ())
	{
		doNotify = true;
		fPreferences[fullKey] = value;
	}
	else
	{
		doNotify = (iter->second != value);
		iter->second = value;
	}

	if (doNotify)
	{
		this->DoNotify (fullKey);
	}
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::DeletePref
 *
 * DESCRIPTION: Delete a preference from our collection.  The delete
 *				operation works on the given key and all sub-keys (that
 *				is, all keys that have "key" as its prefix).
 *
 * PARAMETERS:	key - the key for the value or value to delete.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::DeletePref (const string& key)
{
	// Locking done in Preference::DoSave.
//	omni_mutex_lock lock (fgPrefsMutex);

	string	deleteKey = gPrefs->ExpandKey (key);

	// Iterate over all the entries in the map.

	iterator	iter = fPreferences.begin ();
	while (iter != fPreferences.end ())
	{
		// For each entry, get the key.

		string	thisKey (iter->first.c_str());

		if (::PrvFirstBeginsWithSecond (thisKey, deleteKey))
		{
			fPreferences.erase (iter);
			iter = fPreferences.begin ();
		}
		else
		{
			++iter;
		}
	}

	this->DoNotify (deleteKey);
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::PushPrefix
 *
 * DESCRIPTION: Push a prefix onto our internal stack.	These prefixes
 *				are used later to build a full key with which to fetch
 *				and store preference values.
 *
 * PARAMETERS:	prefix - the subkey to push onto our stack.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::PushPrefix (const string& prefix)
{
	// Locking done in Preference::DoSave/DoLoad.
//	omni_mutex_lock lock (fgPrefsMutex);

	fPrefixes.push_back (prefix);
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::PushPrefix
 *
 * DESCRIPTION: Push a prefix onto our internal stack.	These prefixes
 *				are used later to build a full key with which to fetch
 *				and store preference values.
 *
 * PARAMETERS:	index - the subkey to push onto our stack.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::PushPrefix (long index)
{
	this->PushPrefix (::ToString (index));
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::PopPrefix
 *
 * DESCRIPTION: Pop a prefix off of our internal stack.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::PopPrefix (void)
{
	// Locking done in Preference::DoSave/DoLoad.
//	omni_mutex_lock lock (fgPrefsMutex);

	fPrefixes.pop_back ();
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::ExpandKey
 *
 * DESCRIPTION: Prepend the given key with the prefixes pushed onto
 *				our stack.	Prefixes are prepended in such as way as to
 *				look like a "C" expression.  "Fields" are seperated with
 *				dots, and array subscripts are surround with brackets.
 *
 * PARAMETERS:	name - the part of the key that will appear at the
 *					very end of the expanded key.  If this name is
 *					empty, the expanded key will end with the last
 *					prefix pushed onto the stack.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

string Preferences::ExpandKey (const string& name)
{
	string	key;

	PrefixType::iterator	iter = fPrefixes.begin ();

	if (iter == fPrefixes.end ())
	{
		key = name;
	}
	else
	{
		key = *iter++;

		while (iter != fPrefixes.end ())
		{
			if (!iter->empty ())
				key = this->AppendName (key, *iter);
			
			++iter;
		}

		if (!name.empty ())
		{
			key = this->AppendName (key, name);
		}
	}

	return key;
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::AppendName
 *
 * DESCRIPTION: Append a single prefix to the full expanded key as it
 *				is built up from left to right.
 *
 * PARAMETERS:	key - the expanded key so far.
 *
 *				name - the subkey to append to it.
 *
 * RETURNED:	The catenated value. If name looks like a number, the
 *				catenated result is:
 *
 *					key[name]
 *
 *				Otherwise, we return:
 *
 *					key.name
 *
 ***********************************************************************/

string Preferences::AppendName (string key, const string& name)
{
	if (isdigit (name[0]))
	{
		key = key + "[" + name + "]";
	}
	else
	{
		key = key + "." + name;
	}
	
	return key;
}


/***********************************************************************
 *
 * FUNCTION:    Preferences::AddNotification
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void Preferences::AddNotification (PrefNotifyFunc fn, PrefKeyType key, PrefRefCon refCon)
{
	PrefKeyList	prefList;

	prefList.push_back (key);
	this->AddNotification (fn, prefList, refCon);
}


void Preferences::AddNotification (PrefNotifyFunc fn, const PrefKeyList& keyList, PrefRefCon refCon)
{
	PrefNotifyList::iterator	iter = fNotifications.begin ();
	while (iter != fNotifications.end ())
	{
		if (iter->fFunc == fn)
		{
			break;
		}

		++iter;
	}

	if (iter == fNotifications.end ())
	{
		PrefNotifyType	entry;

		entry.fFunc = fn;
		entry.fRefCon = refCon;

		fNotifications.push_back (entry);

		iter = fNotifications.end () - 1;
	}

	// Add the list of keys for which this function wants notification.
	// (Hmmm...doesn't check for duplicates...should it?)

	iter->fKeyList.insert (iter->fKeyList.end (), keyList.begin (), keyList.end ());
}


/***********************************************************************
 *
 * FUNCTION:    Preferences::RemoveNotification
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void Preferences::RemoveNotification (PrefNotifyFunc fn)
{
	PrefNotifyList::iterator	iter = fNotifications.begin ();
	while (iter != fNotifications.end ())
	{
		if (iter->fFunc == fn)
		{
			fNotifications.erase (iter);
			break;
		}

		++iter;
	}
}


void Preferences::RemoveNotification (PrefNotifyFunc fn, PrefKeyType key)
{
	PrefKeyList	prefList;

	prefList.push_back (key);
	this->RemoveNotification (fn, prefList);
}


void Preferences::RemoveNotification (PrefNotifyFunc /*fn*/, const PrefKeyList& /*keyList*/)
{
#if 0
	PrefNotifyList::iterator	iter = fNotifications.find(fn);

	if (iter != fNotifications.end ())
	{
		// !!! TBD
	}
#endif
}


/***********************************************************************
 *
 * FUNCTION:    DoNotify
 *
 * DESCRIPTION: DESCRIPTION
 *
 * PARAMETERS:  None
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void Preferences::DoNotify (const string& key)
{
	PrefNotifyList::iterator	listIter = fNotifications.begin ();
	while (listIter != fNotifications.end ())
	{
		// For each key in the list, see if it matches the specified key.

		PrefKeyList::iterator	keyIter = listIter->fKeyList.begin ();
		while (keyIter != listIter->fKeyList.end ())
		{
			if (::PrvFirstBeginsWithSecond (key, *keyIter))
			{
				// It does, so call the notification function.

				(listIter->fFunc) (keyIter->c_str (), listIter->fRefCon);
			}

			++keyIter;
		}


		++listIter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::ReadPreferences
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Bool Preferences::ReadPreferences (StringStringMap& prefData)
{
	EmFileRef		prefRef (this->GetPrefRef ());
	return EmMapFile::Read (prefRef, prefData);
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::WritePreferences
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::WritePreferences (const StringStringMap& prefData)
{
	EmFileRef	prefRef (this->GetPrefRef ());
	EmMapFile::Write (prefRef, prefData);
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::GetPrefRef
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmFileRef Preferences::GetPrefRef (void)
{
	EmDirRef	poserDir (EmDirRef::GetEmulatorDirectory ());
	EmDirRef	prefDir (EmDirRef::GetPrefsDirectory ());

#if PLATFORM_MAC

	string		name ("Palm OS Emulator Prefs");

#elif PLATFORM_UNIX

	string		name (".poserrc");

#elif PLATFORM_WINDOWS

	string		name ("Palm OS Emulator.ini");

#else

	#error "Undefined platform"

#endif

	EmFileRef	poserResult (poserDir, name);
	if (poserResult.Exists ())
		return poserResult;

	EmFileRef	prefResult (prefDir, name);

	return prefResult;
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::WriteBanner
 *
 * DESCRIPTION: Write a descriptive banner to the preference file.
 *				Useful so that people will know what the contents of
 *				the file are for (in case they can't glean that from
 *				the name of the file).
 *
 * PARAMETERS:	f - the open FILE to write the banner to.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::WriteBanner (FILE*)
{
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::ReadBanner
 *
 * DESCRIPTION: Read the banner in the given file, possibly validating
 *				it.
 *
 * PARAMETERS:	f - the open FILE containing the banner.
 *
 * RETURNED:	True if it looks like this is our file, false if it
 *				looks like this file contains something else.
 *
 ***********************************************************************/

Bool Preferences::ReadBanner (FILE*)
{
	return true;
}


/***********************************************************************
 *
 * FUNCTION:	Preferences::StripUnused
 *
 * DESCRIPTION: Called to remove any obsolete or otherwise stray
 *				settings from our collection.  Usually called some time
 *				before the updated settings are rewritten back to disk.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void Preferences::StripUnused (void)
{
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::EmulatorPreferences
 *
 * DESCRIPTION: Constructor.  Establishes default values for all the
 *				preferences we'll be using.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmulatorPreferences::EmulatorPreferences (void) :
	Preferences (),
	fTransports ()
{
	if (gEmuPrefs == NULL)
		gEmuPrefs = this;

	// Set up default values for all the keys.
	//
	// Warning: this method works implicitly off of "gPrefs", and so will
	// fail when trying to create additional preference objects.

	#define INIT_PREF_KEYS(name, type, init)		\
	{												\
		Preference<type>	pref (kPrefKey##name);	\
		pref = type init;							\
	}

	FOR_EACH_INIT_PREF(INIT_PREF_KEYS)

	for (EmUARTDeviceType ii = kUARTBegin; ii < kUARTEnd; ++ii)
	{
		fTransports[ii] = NULL;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::~EmulatorPreferences
 *
 * DESCRIPTION: Destructor.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmulatorPreferences::~EmulatorPreferences (void)
{
	for (EmUARTDeviceType ii = kUARTBegin; ii < kUARTEnd; ++ii)
	{
		delete fTransports[ii];
	}

	if (gEmuPrefs == this)
		gEmuPrefs = NULL;
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::Load
 *
 * DESCRIPTION: Install the correct set of logging preferences after the
 *				settings have been loaded from the file.  Also, make
 *				sure the list of skin preferences is large enough so
 *				that we can look up the skin for any device without
 *				having to check the array size all over the place.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmulatorPreferences::Load (void)
{
	Preferences::Load ();

	// Migrate over any old prefs to any new way of handling them.

	this->MigrateOldPrefs ();

	// Let's set up some default configuration in case there isn't
	// one in the prefs file (or it was invalid).

	Preference<Configuration>	prefConfig (kPrefKeyLastConfiguration);
	if (!prefConfig.Loaded())
	{
		prefConfig = Configuration (EmDevice ("PalmIII"), 1024, EmFileRef());
	}

	// Create the transports used by the UARTs.

	this->SetTransports ();
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::GetIndMRU
 *
 * DESCRIPTION: Return a PRC or PSF file from the corresponding list of
 *				files, based on the given index.
 *
 * PARAMETERS:	The index of the file to return.
 *
 * RETURNED:	The nth file in the list.  If index is off the end of
 *				the list, a non-specified EmFileRef (that is, one
 *				for which the IsSpecified method returns false) is
 *				returned.
 *
 ***********************************************************************/

void EmulatorPreferences::GetDatabaseMRU (EmFileRefList& files)
{
	Preference<EmFileRefList> pref (kPrefKeyPRC_MRU);
	files = *pref;
}


void EmulatorPreferences::GetSessionMRU (EmFileRefList& files)
{
	Preference<EmFileRefList> pref (kPrefKeyPSF_MRU);
	files = *pref;
}


void EmulatorPreferences::GetROMMRU (EmFileRefList& files)
{
	Preference<EmFileRefList> pref (kPrefKeyROM_MRU);
	files = *pref;
}


EmFileRef EmulatorPreferences::GetIndPRCMRU (int index)
{
	Preference<EmFileRefList> pref (kPrefKeyPRC_MRU);
	EmFileRefList mru = *pref;
	return this->GetIndMRU (mru, index);
}


EmFileRef EmulatorPreferences::GetIndRAMMRU (int index)
{
	Preference<EmFileRefList> pref (kPrefKeyPSF_MRU);
	EmFileRefList mru = *pref;
	return this->GetIndMRU (mru, index);
}


EmFileRef EmulatorPreferences::GetIndROMMRU (int index)
{
	Preference<EmFileRefList> pref (kPrefKeyROM_MRU);
	EmFileRefList mru = *pref;
	return this->GetIndMRU (mru, index);
}


EmFileRef EmulatorPreferences::GetIndMRU (const EmFileRefList& fileList, int index)
{
	if ((EmFileRefList::size_type) index < fileList.size ())
		return fileList [index];

	return EmFileRef();
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::UpdateMRU
 *
 * DESCRIPTION: Update the MRU list for PRC and PSF files.	The given
 *				file is inserted at the beginning of the list.	If
 *				the file previously existed in the list, it is removed
 *				from its old location.	The length of the list is
 *				limited to MRU_COUNT
 *
 * PARAMETERS:	newFile - the file to move to the beginning of the list.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmulatorPreferences::UpdatePRCMRU (const EmFileRef& newFile)
{
	Preference<EmFileRefList> pref (kPrefKeyPRC_MRU);
	EmFileRefList mru = *pref;
	this->UpdateMRU (mru, newFile);
	pref = mru;
}


void EmulatorPreferences::UpdateRAMMRU (const EmFileRef& newFile)
{
	Preference<EmFileRefList> pref (kPrefKeyPSF_MRU);
	EmFileRefList mru = *pref;
	this->UpdateMRU (mru, newFile);
	pref = mru;
}


void EmulatorPreferences::UpdateROMMRU (const EmFileRef& newFile)
{
	Preference<EmFileRefList> pref (kPrefKeyROM_MRU);
	EmFileRefList mru = *pref;
	this->UpdateMRU (mru, newFile);
	pref = mru;
}


void EmulatorPreferences::UpdateMRU (EmFileRefList& fileList, const EmFileRef& newFile)
{
	this->RemoveMRU (fileList, newFile);

	fileList.insert (fileList.begin (), newFile);

	while (fileList.size () > MRU_COUNT)
		fileList.pop_back ();
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::RemoveMRU
 *
 * DESCRIPTION: Remove the given PRC or PSF file from its MRU list.
 *
 * PARAMETERS:	oldFile - the file to remove.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmulatorPreferences::RemovePRCMRU (const EmFileRef& oldFile)
{
	Preference<EmFileRefList> pref (kPrefKeyPRC_MRU);
	EmFileRefList mru = *pref;
	this->RemoveMRU (mru, oldFile);
	pref = mru;
}


void EmulatorPreferences::RemoveRAMMRU (const EmFileRef& oldFile)
{
	Preference<EmFileRefList> pref (kPrefKeyPSF_MRU);
	EmFileRefList mru = *pref;
	this->RemoveMRU (mru, oldFile);
	pref = mru;
}


void EmulatorPreferences::RemoveROMMRU (const EmFileRef& oldFile)
{
	Preference<EmFileRefList> pref (kPrefKeyROM_MRU);
	EmFileRefList mru = *pref;
	this->RemoveMRU (mru, oldFile);
	pref = mru;
}


void EmulatorPreferences::RemoveMRU (EmFileRefList& fileList, const EmFileRef& oldFile)
{
	EmFileRefList::iterator	iter = find (fileList.begin (), fileList.end (), oldFile);

	if (iter != fileList.end ())
		fileList.erase (iter);
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::SetTransports
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

void EmulatorPreferences::SetTransports (void)
{
	{
		Preference<EmTransportDescriptor> pref (kPrefKeyPortSerial);	
		this->SetTransportForDevice (kUARTSerial, pref->CreateTransport ());
	}

	{
		Preference<EmTransportDescriptor> pref (kPrefKeyPortIR);	
		this->SetTransportForDevice (kUARTIR, pref->CreateTransport ());
	}

	{
		Preference<EmTransportDescriptor> pref (kPrefKeyPortMystery);	
		this->SetTransportForDevice (kUARTMystery, pref->CreateTransport ());
	}
}


void EmulatorPreferences::SetTransportForDevice	(EmUARTDeviceType type,
												 EmTransport* transport)
{
	EmSessionStopper	stopper (gSession, kStopNow);

	delete fTransports[type];
	fTransports[type] = transport;

	// If the transport exists and needs to be opened, open it.

	if (transport && gCPU && EmHAL::GetLineDriverState (type))
	{
		transport->Open ();
	}
}


EmTransport* EmulatorPreferences::GetTransportForDevice (EmUARTDeviceType type)
{
	return fTransports[type];
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Errors::LogMessage
// ---------------------------------------------------------------------------
// Return whether or not the message should be logged.

Bool EmulatorPreferences::LogMessage (Bool isFatal)
{
	{
		if (isFatal && LogErrorMessages ())
			return true;
	}

	{
		if (!isFatal && LogWarningMessages ())
			return true;
	}

	{
		Preference<Bool>	pref ("SilentRunning");
		if (pref.Loaded () && *pref)
			return true;
	}

	return false;
}


// ---------------------------------------------------------------------------
//		¥ PrvGetPrefKey
// ---------------------------------------------------------------------------
// Return the key for the preference that tells us to handle the error or
// warning in this situation.

static PrefKeyType PrvGetPrefKey (Bool isFatal)
{
	PrefKeyType	prefKey = NULL;

	if (Hordes::IsOn ())
	{
		prefKey = isFatal ? kPrefKeyErrorOn : kPrefKeyWarningOn;
	}
	else
	{
		prefKey = isFatal ? kPrefKeyErrorOff : kPrefKeyWarningOff;
	}

	return prefKey;
}


// ---------------------------------------------------------------------------
//		¥ EmulatorPreferences::ShouldQuit
// ---------------------------------------------------------------------------
// Return whether or not this error should cause us to quit.

Bool EmulatorPreferences::ShouldQuit (Bool isFatal)
{
	// If SilentRunning is set, then we quit on fatal errors.

	if (isFatal)
	{
		Preference<Bool>	pref ("SilentRunning");
		if (pref.Loaded () && *pref)
			return true;
	}

	// If the old ExitOnErrors preference is set, then we
	// quit on fatal errors.

	if (isFatal)
	{
		Preference<Bool>	pref ("ExitOnErrors");
		if (pref.Loaded () && *pref)
			return true;
	}

	// Otherwise, get the setting for this situation and
	// see if it tells us to quit.

	PrefKeyType	prefKey = ::PrvGetPrefKey (isFatal);
	Preference<EmErrorHandlingOption>	pref (prefKey);
	return (*pref == kQuit);
}


// ---------------------------------------------------------------------------
//		¥ EmulatorPreferences::ShouldContinue
// ---------------------------------------------------------------------------
// Return whether or not this error should be logged but not displayed.

Bool EmulatorPreferences::ShouldContinue (Bool isFatal)
{
	// If SilentRunning is set, then we continue on warnings.

	if (!isFatal)
	{
		Preference<Bool>	pref ("SilentRunning");
		if (pref.Loaded () && *pref)
			return true;
	}

	// If the old ContinueOnWarnings preference is set, then we
	// continue on warnings.

	if (!isFatal)
	{
		Preference<Bool>	pref ("ContinueOnWarnings");
		if (pref.Loaded () && *pref)
			return true;
	}

	// Otherwise, get the setting for this situation and
	// see if it tells us to quit.

	PrefKeyType	prefKey = ::PrvGetPrefKey (isFatal);
	Preference<EmErrorHandlingOption>	pref (prefKey);
	return (*pref == kContinue);
}


// ---------------------------------------------------------------------------
//		¥ EmulatorPreferences::ShouldNextGremlin
// ---------------------------------------------------------------------------
// Return whether or not this error should cause us to switch to the next
// Gremlin in a Horde.

Bool EmulatorPreferences::ShouldNextGremlin (Bool isFatal)
{
	// Otherwise, get the setting for this situation and
	// see if it tells us to quit.

	PrefKeyType	prefKey = ::PrvGetPrefKey (isFatal);
	Preference<EmErrorHandlingOption>	pref (prefKey);
	return (*pref == kSwitch);
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::WriteBanner
 *
 * DESCRIPTION: Write out a banner indicating that that is a Poser
 *				preference file.
 *
 * PARAMETERS:	f - the open FILE to write the banner to.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmulatorPreferences::WriteBanner (FILE* f)
{
	fprintf (f, "# Palm OS Emulator Preferences\n\n");
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::ReadBanner
 *
 * DESCRIPTION: Do nothing.
 *
 * PARAMETERS:	f - the open FILE containing the banner.
 *
 * RETURNED:	True if it looks like this is our file, false if it
 *				looks like this file contains something else.
 *
 ***********************************************************************/

Bool EmulatorPreferences::ReadBanner (FILE*)
{
	return true;
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::StripUnused
 *
 * DESCRIPTION: Called to remove any obsolete or otherwise stray
 *				settings from our collection.  Usually called some time
 *				before the updated settings are rewritten back to disk.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

static bool PrvCheckKey (PrefKeyType key)
{
	#define REMOVE_UNUSED(name, type, init) 		\
		if (::PrefKeysEqual (key, kPrefKey##name))	\
			return true;
	FOR_EACH_PREF(REMOVE_UNUSED)

	return false;
}

void EmulatorPreferences::StripUnused (void)
{
	// Iterate over all the entries in the map.

	iterator	iter = fPreferences.begin ();
	while (iter != fPreferences.end ())
	{
		// For each entry, get the key, and then just the root part of
		// the key (everything before the first '.' or '[', if any).

		string				keyRoot (iter->first);

		string::size_type	dotPos = keyRoot.find ('.');
		if (dotPos != string::npos)
			keyRoot.erase (dotPos);

		string::size_type	bracketPos = keyRoot.find ('[');
		if (bracketPos != string::npos)
			keyRoot.erase (bracketPos);

		// See if this key (root) is one that we still support.
		// If not, then remove it.

		if (!::PrvCheckKey (keyRoot.c_str ()))
		{
			// According to MSDN's documentation, erase is supposed to
			// return an iterator to the next element.	MSL doesn't
			// seem to follow that description.
//			iter = fPreferences.erase (iter);

			fPreferences.erase (iter);
			iter = fPreferences.begin ();
			continue;
		}

		// If this is one of the old Skins preferences, remove it.

		keyRoot = iter->first;
		bracketPos = keyRoot.find ('[');

		if (bracketPos != string::npos)
		{
			keyRoot.erase (bracketPos + 1);

			if (keyRoot == "Skins[")
			{
				fPreferences.erase (iter);
				iter = fPreferences.begin ();
				continue;
			}
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmulatorPreferences::MigrateOldPrefs
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmulatorPreferences::MigrateOldPrefs (void)
{
	// Migrate any old skin preferences to the current format.

	{
		const char*	kDeviceName[] = 
		{
			NULL,
			"Pilot",
			"Pilot",
			"PalmPilot",
			"PalmPilot",
			NULL,
			"PalmIII",
			"PalmVII",
			NULL,
			NULL,
			"PalmV",
			"PalmIIIx",
			NULL,
			"PalmIIIc",
			"PalmVIIEZ",
			"PalmIIIe",
			"PalmVx",
			"Symbol1700",
			"TRGpro",
			"Visor",
			"PalmM100",
			"PalmVIIx",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			"VZDevice",
			NULL,
			NULL,
			"VisorPrism",
			"VisorPlatinum",
			NULL
		};

		for (size_t index = 0; index < countof (kDeviceName); ++index)
		{
			if (kDeviceName[index])
			{
				char	oldPrefKey[20];
				sprintf (oldPrefKey, "%s[%d]", kPrefKeySkins, index);

				Preference<SkinName>	oldSkinPref (oldPrefKey);
				if (!oldSkinPref.Loaded ())
					break;

				char	newPrefKey[20];
				sprintf (newPrefKey, "%s.%s", kPrefKeySkins, kDeviceName[index]);

				Preference<SkinName>	newSkinPref (newPrefKey);
				newSkinPref = *oldSkinPref;
			}
		}
	}

	// Filter the MRU lists, removing references to items no longer there.

	::PrvFilterFileRefList (kPrefKeyPRC_MRU);
	::PrvFilterFileRefList (kPrefKeyPSF_MRU);
	::PrvFilterFileRefList (kPrefKeyROM_MRU);

	// If this is the first time Poser 3.3 is being run, then set the
	// logging options for errors and warnings to true in Gremlin mode.

	StringStringMap	mapData;

	if (this->ReadPreferences (mapData))
	{
		if (mapData.find ("DialogBeep") == mapData.end ())
		{
			Preference<uint8>	prefLogWarnings (kPrefKeyLogWarningMessages);
			Preference<uint8>	prefLogErrors (kPrefKeyLogErrorMessages);

			prefLogWarnings	= *prefLogWarnings | kGremlinLogging;
			prefLogErrors	= *prefLogErrors | kGremlinLogging;
		}
	}

	// If there's a CommPortList preference, migrate that over
	// to the CommPort preference.

	Preference<string>		prefCommPort ("CommPort");
	Preference<StringList>	prefCommPortList ("CommPortList");

	if (prefCommPortList.Loaded () && prefCommPortList->size () > 0)
	{
		prefCommPort = (*prefCommPortList)[0];
	}

	// Migrate forward old port redirection settings to new ones.
	// (Yes, this is done in addition to the migration done previously.
	// Each migration represents different changes in Poser's evolution.)

	// Convert CommPort, SerialTargetHost, and SerialTargetPort.

	{
		Preference<string>	oldHost ("SerialTargetHost");
		Preference<string>	oldPort ("SerialTargetPort");

		if (oldHost.Loaded ())
		{
			Preference<string>	newHost (kPrefKeyPortSerialSocket);

			newHost = *oldHost + ":" + *oldPort;
		}
	}

	{
		Preference<string>	oldPref ("CommPort");
		if (oldPref.Loaded ())
		{
			Preference<EmTransportDescriptor>	newPref (kPrefKeyPortSerial);
			EmTransportType	type = PrvGetTransportTypeFromPortName (oldPref->c_str ());

			if (type == kTransportSerial)
			{
				newPref = EmTransportDescriptor (kTransportSerial, *oldPref);
			}
			else if (type == kTransportSocket)
			{
				Preference<string>	newHost (kPrefKeyPortSerialSocket);

				newPref = EmTransportDescriptor (kTransportSocket, *newHost);
			}
			else
			{
				newPref = EmTransportDescriptor (kTransportNull);
			}
		}
	}

	// Convert IRPort, IRTargetHost, and IRTargetPort.

	{
		Preference<string>	oldHost ("IRTargetHost");
		Preference<string>	oldPort ("IRTargetPort");

		if (oldHost.Loaded ())
		{
			Preference<string>	newHost (kPrefKeyPortIRSocket);

			newHost = *oldHost + ":" + *oldPort;
		}
	}

	{
		Preference<string>	oldPref ("IRPort");
		if (oldPref.Loaded ())
		{
			Preference<EmTransportDescriptor>	newPref (kPrefKeyPortIR);
			EmTransportType	type = PrvGetTransportTypeFromPortName (oldPref->c_str ());

			if (type == kTransportSerial)
			{
				newPref = EmTransportDescriptor (kTransportSerial, *oldPref);
			}
			else if (type == kTransportSocket)
			{
				Preference<string>	newHost (kPrefKeyPortIRSocket);

				newPref = EmTransportDescriptor (kTransportSocket, *newHost);
			}
			else
			{
				newPref = EmTransportDescriptor (kTransportNull);
			}
		}
	}

	// Establish Mystery Port

	{
		Preference<EmTransportDescriptor>	pref (kPrefKeyPortMystery);

		if (!pref.Loaded ())
		{
			pref = EmTransportDescriptor (kTransportNull);
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvFilterFileRefList
 *
 * DESCRIPTION: Remove files from the given list that no longer appear
 *				to exist.
 *
 * PARAMETERS:	key - the key of the preference containing the list
 *					of files to check.
 *
 * RETURNED:	Nothing, but the preference is altered.
 *
 ***********************************************************************/

void PrvFilterFileRefList (PrefKeyType key)
{
	Preference<EmFileRefList>	pref (key);
	EmFileRefList				mruList = *pref;

	EmFileRefList::iterator		iter = mruList.begin ();

	while (iter != mruList.end ())
	{
		if (!iter->Exists ())
		{
			EmFileRefList::difference_type	index = iter - mruList.begin ();
			mruList.erase (iter);
			iter = mruList.begin () + index;
		}
		else
		{
			++iter;
		}
	}

	pref = mruList;
}


/**********************************************************************
 *
 * FUNCTION:    PrvGetTransportTypeFromPortName
 *
 * DESCRIPTION: .
 *
 * PARAMETERS:  .
 *
 * RETURNED:    .
 *
 ***********************************************************************/

EmTransportType PrvGetTransportTypeFromPortName (const char* portName)
{
#if PLATFORM_UNIX
	if (portName && strlen (portName) > 0)
	{
		if (portName[0] == '/')
		{
			return kTransportSerial;
		}

		return kTransportSocket;
	}
#else
	{
		EmTransportDescriptorList	names;
		EmTransportSerial::GetDescriptorList (names);
		EmTransportDescriptorList::iterator	iter = names.begin ();

		while (iter != names.end ())
		{
			if (!strcmp (portName, iter->GetSchemeSpecific ().c_str ()))
			{
				return kTransportSerial;
			}

			++iter;
		}
	}

	{
		EmTransportDescriptorList	names;
		EmTransportSocket::GetDescriptorList (names);
		EmTransportDescriptorList::iterator	iter = names.begin ();

		while (iter != names.end ())
		{
			if (!strcmp (portName, iter->GetSchemeSpecific ().c_str ()))
			{
				return kTransportSocket;
			}

			++iter;
		}
	}

	{
		EmTransportDescriptorList	names;
		EmTransportUSB::GetDescriptorList (names);
		EmTransportDescriptorList::iterator	iter = names.begin ();

		while (iter != names.end ())
		{
			if (!strcmp (portName, iter->GetSchemeSpecific ().c_str ()))
			{
				return kTransportUSB;
			}

			++iter;
		}
	}
#endif

	return kTransportNull;
}

bool PrefKeysEqual (PrefKeyType key1, PrefKeyType key2)
{
	return _stricmp (key1, key2) == 0;
}

// Force instantiation of some necessary templates.
template class Preference<GremlinInfo>;
template class Preference<HordeInfo>;
template class Preference<SlotInfoList>;
template class Preference<EmFileRefList>;
template class Preference<Configuration>;
