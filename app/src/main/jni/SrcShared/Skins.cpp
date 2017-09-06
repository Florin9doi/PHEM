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
#include "Skins.h"

#include "ChunkFile.h"			// Chunk
#include "EmApplication.h"		// gApplication
#include "EmFileRef.h"			// EmFileRef
#include "EmMapFile.h"			// EmMapFile
#include "EmSession.h"			// gSession
#include "EmStreamFile.h"		// EmStreamFile, kOpenExistingForRead
#include "Miscellaneous.h"		// StartsWith
#include "Platform.h"			// _stricmp
#include "PreferenceMgr.h"		// Preference
#include "Strings.r.h"			// kStr_MissingSkins

#include <algorithm>			// find()

struct ButtonBounds
{
	SkinElementType	fButton;
	EmRect			fBounds;
};
typedef vector<ButtonBounds>	ButtonBoundsList;
 
struct ButtonBoundsX
{
	SkinElementType	fButton;
	RectangleType	fBounds;
};
 
struct Skinfo
{
	Skinfo () :
		fSkinFile (),
		fName (),
		fImageName1x (),
		fImageName2x (),
		fDevices (),
		fButtons ()
		{}

	EmFileRef			fSkinFile;
	SkinName			fName;
	string				fImageName1x;
	string				fImageName2x;
	RGBType				fBackgroundColor;
	RGBType				fHighlightColor;
	EmDeviceList		fDevices;
	ButtonBoundsList	fButtons;
};
typedef vector<Skinfo>	SkinList;


static EmDevice			gCurrentDevice;
static Skinfo			gCurrentSkin;
static ScaleType		gCurrentScale;

static void				PrvBuildSkinList	(SkinList&);
static void				PrvGetSkins			(const EmDevice&, SkinList& results);
static Bool				PrvGetNamedSkin		(const EmDevice&, const SkinName& name, Skinfo& result);
static void				PrvGetGenericSkin	(Skinfo& skin);
static void				PrvGetDefaultSkin	(const EmDevice&, Skinfo& skin);
static void				PrvSetSkin			(const EmDevice&, const Skinfo&, ScaleType scale);
static EmRect			PrvGetTouchscreen	(void);
static SkinElementType	PrvTestPoint		(const EmPoint&, int outset);
static SkinName			PrvGetSkinName		(const EmDevice& device);


static const char* kElementNames[] =
{
	"PowerButton",
	"UpButton",
	"DownButton",
	"App1Button",
	"App2Button",
	"App3Button",
	"App4Button",
	"CradleButton",
	"Antenna",
	"ContrastButton",

		// Symbol-specific
	"TriggerLeft",
	"TriggerCenter",
	"TriggerRight",
	"UpButtonLeft",
	"UpButtonRight",
	"DownButtonLeft",
	"DownButtonRight",

	"Touchscreen",
	"LCD",
	"LED"
};


static const char	kGenericSkinName[] = "Generic";

#if 0 //AndroidTODO: conditionalize on Android compile
static ButtonBoundsX	kGenericButtons [] =
{
	{ kElement_PowerButton,		{ {   1, 274 }, {  16,  24 } } },
	{ kElement_UpButton,		{ {  96, 272 }, {  32,  16 } } },
	{ kElement_DownButton,		{ {  96, 293 }, {  32,  16 } } },
	{ kElement_App1Button,		{ {  23, 270 }, {  32,  32 } } },
	{ kElement_App2Button,		{ {  60, 270 }, {  32,  32 } } },
	{ kElement_App3Button,		{ { 131, 270 }, {  32,  32 } } },
	{ kElement_App4Button,		{ { 168, 270 }, {  32,  32 } } },
	{ kElement_CradleButton,	{ {   0,   0 }, {   0,   0 } } },
	{ kElement_Antenna,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_ContrastButton,	{ {   0,   0 }, {   0,   0 } } },
	{ kElement_Touchscreen,		{ {  32,  32 }, { 160, 220 } } },
	{ kElement_LCD,			{ {  32,  32 }, { 160, 160 } } },
	{ kElement_LED,			{ {   1, 274 }, {  16,  24 } } }
};
#else
static ButtonBoundsX	kGenericButtons [] =
{
	{ kElement_PowerButton,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_UpButton,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_DownButton,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_App1Button,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_App2Button,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_App3Button,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_App4Button,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_CradleButton,	{ {   0,   0 }, {   0,   0 } } },
	{ kElement_Antenna,		{ {   0,   0 }, {   0,   0 } } },
	{ kElement_ContrastButton,	{ {   0,   0 }, {   0,   0 } } },
	{ kElement_Touchscreen,		{ {   3,   3 }, { 160, 220 } } },
	{ kElement_LCD,			{ {   3,   3 }, { 160, 160 } } },
	{ kElement_LED,			{ {   0,   0 }, {   0,   0 } } }
};
#endif

static RGBType	kGenericBackgroundColor	(0x7B, 0x8C, 0x5A);
static RGBType	kGenericHighlightColor	(0x64, 0xF0, 0xDC);


/***********************************************************************
 *
 * FUNCTION:    SkinGetSkinName
 *
 * DESCRIPTION: Get the name of the user-chosen skin for the given
 *				device type.
 *
 * PARAMETERS:  type - the device type for which we need the name
 *					of the skin to use.
 *
 * RETURNED:    The skin name.  At the very least, this will be the
 *				name of some default skin if the user hasn't made a
 *				choice or if the chosen skin is invalid.
 *
 ***********************************************************************/

SkinName SkinGetSkinName (const EmDevice& device)
{
	// Get the chosen skin for this device.

	SkinName	name = ::PrvGetSkinName (device);

	// If the name is empty or invalid, chose a default skin.

	if (!::SkinValidSkin (device, name))
	{
		name = ::SkinGetDefaultSkinName (device);
	}

	return name;
}


/***********************************************************************
 *
 * FUNCTION:    SkinGetDefaultSkinName
 *
 * DESCRIPTION: Get the name of the user-chosen skin for the given
 *				device type.
 *
 * PARAMETERS:  type - the device type for which we need the name
 *					of the skin to use.
 *
 * RETURNED:    The skin name.  At the very least, this will be the
 *				name of some default skin if the user hasn't made a
 *				choice or if the chosen skin is invalid.
 *
 ***********************************************************************/

SkinName SkinGetDefaultSkinName (const EmDevice& device)
{
	Skinfo	skin;
	::PrvGetDefaultSkin (device, skin);
	return skin.fName;
}


/***********************************************************************
 *
 * FUNCTION:	SkinGetSkinNames
 *
 * DESCRIPTION:	Get the list of names of available skins for the given
 *				device.
 *
 * PARAMETERS:	type - the device for which the list of skins should
 *					be returned.  If kDeviceUnspecified, return all
 *					skins for all devices.
 *
 *				results - receives the list of skin names.  Any skin
 *					names are *added* to this list; the list is not
 *					cleared out first.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SkinGetSkinNames (const EmDevice& device, SkinNameList& results)
{
	// Always push on the name of the default skin first.

	results.push_back (kGenericSkinName);

	// Get the names of any custom skins for the device and
	// add those, too.

	SkinList	skins;
	::PrvGetSkins (device, skins);

	SkinList::iterator	iter = skins.begin ();
	while (iter != skins.end ())
	{
		results.push_back (iter->fName);

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:	SkinSetSkin
 *
 * DESCRIPTION:	Establish the skin to use, based on the current settings.
 *				All other funtions in this module will then work within
 *				the context of the specified skin.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void SkinSetSkin (void)
{
	EmAssert (gSession);

	Configuration			cfg = gSession->GetConfiguration ();
	SkinName				skinName = ::PrvGetSkinName (cfg.fDevice);
	Preference<ScaleType>	scalePref (kPrefKeyScale);

	::SkinSetSkin (cfg.fDevice, *scalePref, skinName);
}


void SkinSetSkin (const EmDevice& device, ScaleType scale, const SkinName& name)
{
	Skinfo		skin;

	if (!::PrvGetNamedSkin (device, name, skin))
	{
		::PrvGetDefaultSkin (device, skin);
	}

	::PrvSetSkin (device, skin, scale);
}


void SkinSetSkinName (const EmDevice& device, const SkinName& name)
{
	string	idString (device.GetIDString ());
	string	prefKey (kPrefKeySkins + ("." + idString));

	Preference<SkinName>	prefSkin (prefKey.c_str ());
	prefSkin = name;
}


/***********************************************************************
 *
 * FUNCTION:	SkinGetSkinfoFile
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmFileRef SkinGetSkinfoFile (void)
{
	return gCurrentSkin.fSkinFile;
}


/***********************************************************************
 *
 * FUNCTION:	SkinGetSkinFile
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmFileRef SkinGetSkinFile (void)
{
	return ::SkinGetSkinFile (gCurrentScale);
}


EmFileRef SkinGetSkinFile (ScaleType scale)
{
	EmFileRef		skinfoFile = ::SkinGetSkinfoFile ();

	if (!skinfoFile.IsSpecified ())
	{
		return EmFileRef ();
	}

	EmAssert (!gApplication->SkinfoResourcePresent ());
	EmAssert (!gApplication->Skin1xResourcePresent ());
	EmAssert (!gApplication->Skin2xResourcePresent ());

	string			name;

	if (scale == 1)
		name = gCurrentSkin.fImageName1x.c_str();
	else
		name = gCurrentSkin.fImageName2x.c_str();

	EmDirRef		skinDir = skinfoFile.GetParent ();
	EmFileRef		skinImage (skinDir, name);

	// If the skin file doesn't exist, try looking for it
	// in a Mac or Windows sub-directory.  This helps development
	// of the emulator, as images for different platforms are
	// stored in directories with those names.  This sub-directory
	// looking shouldn't be needed for the image archive made
	// available to developers.

	if (!skinImage.Exists ())
	{
#if PLATFORM_MAC
		EmDirRef	skinSubDir (skinDir, "Mac");
#else
		EmDirRef	skinSubDir (skinDir, "Windows");
#endif

		skinImage = EmFileRef (skinSubDir, name);
		if (!skinImage.Exists ())
		{
			EmFileRef temp;
			return (temp);
		}
	}

	return skinImage;
}


/***********************************************************************
 *
 * FUNCTION:	SkinGetSkinStream
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmStream* SkinGetSkinStream (void)
{
	return ::SkinGetSkinStream (gCurrentScale);
}


EmStream* SkinGetSkinStream (ScaleType scale)
{
	EmStream*	result = NULL;

	if (gCurrentSkin.fName == kGenericSkinName)
		return result;

	if (gApplication->IsBound ())
	{
		// If we're bound, open up a stream on the resource data.

		Bool	haveRes;
		Chunk*	chunk = new Chunk;

		if (scale == 1)
			haveRes = gApplication->GetSkin1xResource (*chunk);
		else
			haveRes = gApplication->GetSkin2xResource (*chunk);

		if (haveRes)
			result = new EmStreamChunk (chunk);
		else
			delete chunk;
	}
	else
	{
		EmFileRef	file = ::SkinGetSkinFile (scale);

		if (file.Exists ())
		{
			// If we're not bound, try opening a stream on the file.

			result = new EmStreamFile (file, kOpenExistingForRead);
		}
	}

	if (!result)
	{
		// If we can't get the image, revert to the default image.
		// !!! Should probably also remove the skin from the skin list.

		Skinfo	skin;
		::PrvGetDefaultSkin (gCurrentDevice, skin);
		::PrvSetSkin (gCurrentDevice, skin, scale);
	}

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SkinValidSkin
 *
 * DESCRIPTION:	Returns whether the given device has a skin with the
 *				given name.
 *
 * PARAMETERS:	type - the device type.
 *
 *				skinName - the skin name.
 *
 * RETURNED:	True if the given device has a skin with the given name.
 *				False otherwise.
 *
 ***********************************************************************/

Bool SkinValidSkin (const EmDevice& device, const SkinName& skinName)
{
	SkinNameList	skins;
	::SkinGetSkinNames (device, skins);

	SkinNameList::iterator	iter = skins.begin ();
	while (iter != skins.end ())
	{
		if (*iter == skinName)
		{
			return true;
		}

		++iter;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	SkinGetBackgroundColor
 *
 * DESCRIPTION:	Return the default background color for the current
 *				skin.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The default background color.
 *
 ***********************************************************************/

RGBType		SkinGetBackgroundColor	(void)
{
	return gCurrentSkin.fBackgroundColor;
}


/***********************************************************************
 *
 * FUNCTION:	SkinGetHighlightColor
 *
 * DESCRIPTION:	Return the default highlight color for the current
 *				skin.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	The default highlight color.
 *
 ***********************************************************************/

RGBType		SkinGetHighlightColor	(void)
{
	return gCurrentSkin.fHighlightColor;
}


/***********************************************************************
 *
 * FUNCTION:	SkinTestPoint
 *
 * DESCRIPTION:	Tests the given point to see what skin element it's over.
 *
 * PARAMETERS:	pt - location in the window to test.
 *
 * RETURNED:	If the point is within an element, returns the id for
 *				that element.  If an element was just missed, returns
 *				kElement_None.  Otherwise, returns kElement_Frame.
 *
 ***********************************************************************/

SkinElementType	SkinTestPoint		(const EmPoint& pt)
{	
	// See if we hit an element.  PrvTestPoint will return either the
	// element hit or kElement_Frame if none were hit.  If an element
	// was hit, return it.

	SkinElementType	result = ::PrvTestPoint (pt, 0);

	if (result != kElement_Frame)
		return result;

	// Test again, this time allowing for some slop around the
	// elements.  If an element was hit this time, then we hit the
	// small "dead area" we allow around the elements.  In that case,
	// we want to return kElement_None.  Otherwise, if no element was
	// hit, signal that the frame was hit.

	result = ::PrvTestPoint (pt, 5);
	if (result != kElement_Frame)
		return kElement_None;

	return kElement_Frame;
}


/***********************************************************************
 *
 * FUNCTION:	SkinWindowToTouchscreen
 *
 * DESCRIPTION:	Convert a point from window coordinates to LCD
 *				coordinates.
 *
 * PARAMETERS:	pt - point in window coordinates to convert.
 *
 * RETURNED:	The point in LCD coordinates (where the topleft corner
 *				of the LCD is 0,0 and the scale is 1x).
 *
 ***********************************************************************/

EmPoint		SkinWindowToTouchscreen		(const EmPoint& pt)
{
	EmPoint	result	= pt;
	EmRect	r		= ::PrvGetTouchscreen ();

	result -= r.TopLeft ();

	if (result.fX < 0)
		result.fX = 0;

	if (result.fY < 0)
		result.fY = 0;

	if (result.fX >= r.Width ())
		result.fX = r.Width () - 1;

	if (result.fY >= r.Height ())
		result.fY = r.Height () - 1;

	result = ::SkinScaleDown (result);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SkinLCDToWindow
 *
 * DESCRIPTION:	Convert a point from LCD coordinates to window
 *				coordinates.
 *
 * PARAMETERS:	pt - point in LCD coordinates to convert.
 *
 * RETURNED:	The point in window coordinates (where the topleft
 *				corner of the window is 0,0 and the scale is the
 *				scale chosen by the user).
 *
 ***********************************************************************/

EmPoint		SkinTouchscreenToWindow		(const EmPoint& lcdPt)
{
	EmPoint	result	= lcdPt;
	EmRect	r		= ::PrvGetTouchscreen ();

	result += r.TopLeft ();

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SkinGetElementInfo
 *
 * DESCRIPTION:	Get information on the given element.
 *
 * PARAMETERS:	index - index of the element we're querying.
 *
 *				type - type of the element we've queried
 *
 *				bounds - bounds of the element we've queried.  The
 *					is scaled up if necessary.
 *
 * RETURNED:	TRUE if there was such an element.  FALSE if index is
 *				out of range.
 *
 ***********************************************************************/

Bool	SkinGetElementInfo	(int index, SkinElementType& type, EmRect& bounds)
{
	if (index < (int) gCurrentSkin.fButtons.size ())
	{
		type = gCurrentSkin.fButtons[index].fButton;
		bounds = gCurrentSkin.fButtons[index].fBounds;
		bounds = ::SkinScaleUp (bounds);

		return true;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:	SkinScaleDown
 *
 * DESCRIPTION:	Convert a point from 1x or 2x to just 1x.
 *
 * PARAMETERS:	pt - point to change.
 *
 * RETURNED:	Normalized point.
 *
 ***********************************************************************/

EmPoint	SkinScaleDown	(const EmPoint& pt)
{
	EmAssert (gCurrentScale > 0);

	EmPoint	result = pt;

	result /= EmPoint (gCurrentScale, gCurrentScale);

	return result;
}


EmRect	SkinScaleDown	(const EmRect& r)
{
	EmAssert (gCurrentScale > 0);

	EmRect	result = r;

	result /= EmPoint (gCurrentScale, gCurrentScale);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	SkinScaleUp
 *
 * DESCRIPTION:	Convert a point to 1x or 2x, depending on the scaling
 *				factor.
 *
 * PARAMETERS:	pt - point to change.
 *
 * RETURNED:	Denormalized point.
 *
 ***********************************************************************/

EmPoint	SkinScaleUp	(const EmPoint& pt)
{
	EmAssert (gCurrentScale > 0);

	EmPoint	result = pt;

	result *= EmPoint (gCurrentScale, gCurrentScale);

	return result;
}


EmRect	SkinScaleUp	(const EmRect& r)
{
	EmAssert (gCurrentScale > 0);

	EmRect	result = r;

	result *= EmPoint (gCurrentScale, gCurrentScale);

	return result;
}


#pragma mark -


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static int PrvGetNumber (const string& s)
{
	// Get the (stringized) component to convert.

	string	numString = ::Strip (s, " \t", true, true);

	// Try converting it from hex to numeric format.

	int		result;
	int		scanned = sscanf (numString.c_str (), "0x%X", &result);

	// If that failed, try converting it from decimal to numeric format.
	// We have to be careful about scanning "0".  That text will match
	// the "0" in "0x%X", causing it to return EOF, but not filling
	// in "result".

	if (scanned == 0 || scanned == EOF)
		scanned = sscanf (numString.c_str (), "%d", &result);

	// If that failed, throw an exception.

	if (scanned == 0 || scanned == EOF)
		throw 1;

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static uint8 PrvGetOneRGB (const StringList& rgbs, int component)
{
	// Convert the string to a number.

	int	result = ::PrvGetNumber (rgbs[component]);

	// If that failed, or if the resulting number is out of range,
	// throw an exception indicating an invalid input.

	if (result < 0 || result > 255)
		throw 1;

	return (uint8) result;
}


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static RGBType PrvGetRGB (const StringStringMap& entries, const char* key)
{
	RGBType	result;

	// Get the string containing the devices.

	string		rgbString = entries.find (key)->second;

	// Break it up into parts.

	StringList	rgbs;
	::SeparateList (rgbs, rgbString, ',');

	// Make sure we have the right number

	if (rgbs.size () != 3)
		throw 1;

	result.fRed		= ::PrvGetOneRGB (rgbs, 0);
	result.fGreen	= ::PrvGetOneRGB (rgbs, 1);
	result.fBlue	= ::PrvGetOneRGB (rgbs, 2);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static EmDeviceList PrvGetDeviceList (const StringStringMap& entries)
{
	EmDeviceList	result;

	// Get the string containing the devices.

	string		devicesString = entries.find ("Devices")->second;

	// Break it up into parts.

	StringList	deviceNames;
	::SeparateList (deviceNames, devicesString, ',');

	// Iterate over the given device names, and see if they're
	// supported or valid.

	StringList::iterator	iter = deviceNames.begin ();
	while (iter != deviceNames.end ())
	{
		// Get a device name, and remove any leading or trailing whitespace.

		string		deviceName = ::Strip (*iter, " \t", true, true);

		// Create a device object from it.

		EmDevice	device (deviceName);

		// If the name was accepted, add the device to the list.

		if (device.Supported ())
		{
			result.push_back (device);
		}

		++iter;
	}

	// If no valid devices were provided, throw an exception to signal
	// the caller that a completely invalid list was provided.

	if (result.size () == 0)
		throw 1;

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static SkinElementType PrvGetElementType (const string& s)
{
	// Get the element name and try to ID it.

	string	eltName = ::Strip (s, " \t", true, true);

	for (SkinElementType ii = kElement_First; ii < kElement_NumElements; ++ii)
	{
		if (kElementNames[ii] != NULL &&
			_stricmp (eltName.c_str (), kElementNames[ii]) == 0)
		{
			return ii;
		}
	}

	// If we couldn't determine the name, error out.

	throw 1;

	return kElement_None;	// (For the compiler)
}


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static ButtonBounds PrvGetOneElement (const string& s)
{
	ButtonBounds	result;

	// Break up the string into its parts, which should be an
	// element name followed by its bounds.

	StringList		parts;
	::SeparateList (parts, s, ',');

	// Make sure we have the right number of pieces.

	if (parts.size () != 5)
		throw 1;

	result.fButton = ::PrvGetElementType (parts[0]);

	// Get the bounds

	RectangleType	rect;
	rect.topLeft.x = ::PrvGetNumber (parts[1]);
	rect.topLeft.y = ::PrvGetNumber (parts[2]);
	rect.extent.x = ::PrvGetNumber (parts[3]);
	rect.extent.y = ::PrvGetNumber (parts[4]);

	result.fBounds = EmRect (rect);

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static ButtonBoundsList PrvGetElementList (const StringStringMap& entries)
{
	ButtonBoundsList	result;
	Bool				haveLCD = false;
	Bool				haveTouchScreen = false;

	// Iterate over all of the entries in "entries", looking for ones
	// that start with "Element".

	StringStringMap::const_iterator	iter = entries.begin ();
	while (iter != entries.end ())
	{
		if (::StartsWith (iter->first.c_str (), "Element"))
		{
			ButtonBounds	elt = ::PrvGetOneElement (iter->second);
			result.push_back (elt);

			if (elt.fButton == kElement_LCD)
				haveLCD = true;

			if (elt.fButton == kElement_Touchscreen)
				haveTouchScreen = true;
		}

		++iter;
	}

	if (!haveLCD || !haveTouchScreen)
		throw 1;

	return result;
}


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static void PrvAddSkin (SkinList& skins, StringStringMap& entries,
						const EmFileRef* file = NULL)
{
	// Check for required fields.

	if (entries.find ("Name")				== entries.end ()	||
		entries.find ("File1x")				== entries.end ()	||
		entries.find ("File2x")				== entries.end ()	||
		entries.find ("BackgroundColor")	== entries.end ()	||
		entries.find ("HighlightColor")		== entries.end ()	||
		entries.find ("Devices")			== entries.end ())
	{
		return;
	}

	// Now extract the values.

	try
	{
		Skinfo	skin;

		skin.fSkinFile			= file ? *file : EmFileRef();
		skin.fName				= entries["Name"];
		skin.fImageName1x		= entries["File1x"];
		skin.fImageName2x		= entries["File2x"];
		skin.fBackgroundColor	= ::PrvGetRGB (entries, "BackgroundColor");
		skin.fHighlightColor	= ::PrvGetRGB (entries, "HighlightColor");
		skin.fDevices			= ::PrvGetDeviceList (entries);
		skin.fButtons			= ::PrvGetElementList (entries);

		skins.push_back (skin);
	}
	catch (...)
	{
	}
}

static void PrvAddSkin (SkinList& skins, EmStream& skinStream)
{
	StringStringMap	entries;
	EmMapFile::Read (skinStream, entries);

	PrvAddSkin (skins, entries);
}

static void PrvAddSkin (SkinList& skins, const EmFileRef& skinFile)
{
	StringStringMap	entries;
	EmMapFile::Read (skinFile, entries);

	PrvAddSkin (skins, entries, &skinFile);
}


/***********************************************************************
 *
 * FUNCTION:	.
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

static void PrvScanForSkinFiles (SkinList& skins, const EmDirRef& skinDir)
{
	EmDirRefList	dirs;
	EmFileRefList	files;
	skinDir.GetChildren (&files, &dirs);

	// Look for any *.skin files in the given directory.
	{
		EmFileRefList::iterator	iter = files.begin ();
		while (iter != files.end ())
		{
			if (iter->IsType (kFileTypeSkin))
			{
				::PrvAddSkin (skins, *iter);
			}

			++iter;
		}
	}

	// Now recurse into sub-directories.
	{
		EmDirRefList::iterator	iter = dirs.begin ();
		while (iter != dirs.end ())
		{
			string	name = iter->GetName ();
			if (name.size () < 2 ||
				name[0] != '(' ||
				name[name.size () - 1] != ')')
			{
				::PrvScanForSkinFiles (skins, *iter);
			}

			++iter;
		}
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvBuildSkinList
 *
 * DESCRIPTION:	Create the full list of skins known to the emulator.
 *				This list of skins includes both the built-in ones and
 *				any found on disk.
 *
 * PARAMETERS:	skins - receives the list of skins.  The list of skins
 *					is *added* to this collection; it is not cleared
 *					out first.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void PrvBuildSkinList (SkinList& skins)
{
	// If we're a "bound" Poser, the skin list consists of the
	// skin we're bound to.

	if (gApplication->IsBound ())
	{
		Chunk	chunk;
		if (gApplication->GetSkinfoResource (chunk))
		{
			EmStreamChunk	stream (chunk);
			::PrvAddSkin (skins, stream);
		}

		return;
	}

	// Look for a sub-directory named "Skins".

	EmDirRef	scanDir (EmDirRef::GetEmulatorDirectory (), "Skins");

	if (!scanDir.Exists ())
		scanDir = EmDirRef (EmDirRef::GetEmulatorDirectory (), "skins");

#if PLATFORM_UNIX
	// On Unix, also look in the /usr/local/share/pose and /usr/share/pose directories.

	if (!scanDir.Exists ())
		scanDir = EmDirRef ("/usr/local/share/pose/Skins");

	if (!scanDir.Exists ())
		scanDir = EmDirRef ("/usr/local/share/pose/skins");

	if (!scanDir.Exists ())
		scanDir = EmDirRef ("/usr/share/pose/Skins");

	if (!scanDir.Exists ())
		scanDir = EmDirRef ("/usr/share/pose/skins");
#endif

	if (scanDir.Exists ())
	{
		::PrvScanForSkinFiles (skins, scanDir);
	}
}


#pragma mark -


/***********************************************************************
 *
 * FUNCTION:	PrvSetSkin
 *
 * DESCRIPTION:	Common low-level routine to setting the current skin.
 *
 * PARAMETERS:	skin - new skin.
 *
 *				scale - new scale.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void PrvSetSkin (const EmDevice& device, const Skinfo& skin, ScaleType scale)
{
	gCurrentDevice	= device;
	gCurrentSkin	= skin;
	gCurrentScale	= scale;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetSkins
 *
 * DESCRIPTION:	Get the list of available skins for the given device.
 *				Does not include the generic skin.
 *
 * PARAMETERS:	type - the device for which the list of skins should
 *					be returned.  If kDeviceUnspecified, return all
 *					skins for all devices.
 *
 *				results - receives the list of skins.  Any skins are
 *					*added* to this list; the list is not cleared out
 *					first.
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void PrvGetSkins (const EmDevice& device, SkinList& results)
{
	static SkinList	gFullSkinList;
	static Bool		gInitialized;

	if (!gInitialized)
	{
		gInitialized = true;
		::PrvBuildSkinList (gFullSkinList);
	}

	SkinList::iterator	iter = gFullSkinList.begin ();
	while (iter != gFullSkinList.end ())
	{
		if (!device.Supported () ||	// Add everything!
			find (iter->fDevices.begin (), iter->fDevices.end (), device) != iter->fDevices.end ())
		{
			results.push_back (*iter);
		}

		++iter;
	}
}


/***********************************************************************
 *
 * FUNCTION:    PrvGetNamedSkin
 *
 * DESCRIPTION: Find the Skinfo for the given device and that has the
 *				given name.
 *
 * PARAMETERS:  type - the device whose skin we're looking for.
 *
 *				name - the name of the skin to find.
 *
 *				result - reference to the Skinfo in which to place the
 *					found skin information, if any.
 *
 * RETURNED:    True if the skin could be found, false othewise.
 *
 ***********************************************************************/

Bool PrvGetNamedSkin (const EmDevice& device, const SkinName& name, Skinfo& result)
{
	if (name == kGenericSkinName)
	{
		::PrvGetGenericSkin (result);
		return true;
	}

	SkinList	skins;
	::PrvGetSkins (device, skins);

	SkinList::iterator	iter = skins.begin();
	while (iter != skins.end ())
	{
		if (iter->fName == name)
		{
			result = *iter;
			return true;
		}

		++iter;
	}

	return false;
}


/***********************************************************************
 *
 * FUNCTION:    PrvGetGenericSkin
 *
 * DESCRIPTION: Return skin information for the "generic" skin, the one
 *				that is built-in and can be used for any device if a
 *				custom one cannot be found.
 *
 * PARAMETERS:  skin - reference to the Skinfo in which to place the
 *					default skin information.
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void PrvGetGenericSkin (Skinfo& skin)
{
	skin.fDevices.clear ();
	skin.fButtons.clear ();

	skin.fName				= kGenericSkinName;
	skin.fImageName1x		= "";
	skin.fImageName2x		= "";
	skin.fBackgroundColor	= kGenericBackgroundColor;
	skin.fHighlightColor	= kGenericHighlightColor;

	for (size_t ii = 0; ii < countof (kGenericButtons); ++ii)
	{
		ButtonBoundsX	buttonX = kGenericButtons[ii];
		ButtonBounds	button;
		button.fButton = buttonX.fButton;
		button.fBounds = EmRect (buttonX.fBounds);
		skin.fButtons.push_back (button);
	}
}


/***********************************************************************
 *
 * FUNCTION:    PrvGetDefaultSkin
 *
 * DESCRIPTION: Return default skin information for the given device.
 *				This function returns information in the case that
 *				a skin with a desired name could not be found.
 *
 * PARAMETERS:  type - the device whose default skin information we want.
 *
 *				skin - reference to the Skinfo in which to place the
 *					default skin information.
 *
 * RETURNED:    Nothing
 *
 ***********************************************************************/

void PrvGetDefaultSkin (const EmDevice& device, Skinfo& skin)
{
	SkinList	skins;
	::PrvGetSkins (device, skins);

	if (skins.size () == 0)
	{
		::PrvGetGenericSkin (skin);
	}
	else
	{
		skin = skins[0];
	}
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetTouchscreen
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/

EmRect	PrvGetTouchscreen	(void)
{
	int	index = 0;
	SkinElementType	type;
	EmRect	bounds;

	while (::SkinGetElementInfo (index, type, bounds))
	{
		if (type == kElement_Touchscreen)
		{
			return bounds;
		}

		++index;
	}

	EmAssert (false);

	// Shut up the compiler
	return bounds;
}


/***********************************************************************
 *
 * FUNCTION:	PrvTestPoint
 *
 * DESCRIPTION:	Test the given point against all of the skin elements.
 *				An optional outset value can be provided which is
 *				used to modify the element bounds before they are
 *				tested.
 *
 * PARAMETERS:	pt - window location to test.
 *
 *				outset - outset value to apply to the bounds of all
 *					the skin elements.
 *
 * RETURNED:	If one contains the given point, return that skin
 *				element.  Otherwise, return kElement_Frame.
 *
 ***********************************************************************/

SkinElementType	PrvTestPoint (const EmPoint& pt, int outset)
{
	ButtonBoundsList::iterator	iter = gCurrentSkin.fButtons.begin ();
	while (iter != gCurrentSkin.fButtons.end ())
	{
		EmRect	bounds = iter->fBounds;
		bounds = ::SkinScaleUp (bounds);
		bounds.Inset (-outset, -outset);

		if (bounds.Contains (pt))
			return iter->fButton;

		++iter;
	}

	return kElement_Frame;
}


/***********************************************************************
 *
 * FUNCTION:	PrvGetSkinName
 *
 * DESCRIPTION:	Returns the name of the skin to use for the given
 *				device.  The preferences are first queried to see if
 *				there is a skin name registered under any of the
 *				ID strings by which this device is known ("Palm m100",
 *				"m100", "Calvin", etc.).  If not, then an empty skin
 *				name is returned.
 *
 * PARAMETERS:	device - the device object for which the skin is needed.
 *
 * RETURNED:	The name of the preferred skin for the device, or an
 *				empty skin name if one could not be found.
 *
 ***********************************************************************/

SkinName PrvGetSkinName (const EmDevice& device)
{
	SkinName				result;
	StringList				deviceNames = device.GetIDStrings ();
	StringList::iterator	iter = deviceNames.begin ();

	while (iter != deviceNames.end ())
	{
		string					prefKey (kPrefKeySkins + ("." + *iter));
		Preference<SkinName>	pref (prefKey.c_str ());

		if (pref.Loaded ())
		{
			result = *pref;
			break;
		}

		++iter;
	}

	return result;
}
