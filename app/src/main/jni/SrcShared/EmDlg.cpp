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
#include "EmDlg.h"

#include "DebugMgr.h"			// gDebuggerGlobals
#include "EmApplication.h"		// gApplication, BindPoser
#include "EmBankDRAM.h"			// EmBankDRAM::ValidAddress
#include "EmBankROM.h"			// EmBankROM::ValidAddress
#include "EmBankSRAM.h"			// EmBankSRAM::ValidAddress
#include "EmEventPlayback.h"	// GetCurrentEvent, GetNumEvents
#include "EmFileImport.h"		// EmFileImport
#include "EmMinimize.h"			// EmMinimize::Stop
#include "EmPatchState.h"		// EmPatchState::UIInitialized
#include "EmROMTransfer.h"		// EmROMTransfer
#include "EmROMReader.h"		// EmROMReader
#include "EmSession.h"			// gSession
#include "EmStreamFile.h"		// EmStreamFile
#include "EmTransportSerial.h"	// PortNameList, GetSerialPortNameList, etc.
#include "EmTransportSocket.h"	// PortNameList, GetPortNameList, etc.
#include "EmTransportUSB.h"		// EmTransportUSB
#include "ErrorHandling.h"		// Errors::SetParameter
#include "Hordes.h"				// Hordes::New
#include "LoadApplication.h"	// SavePalmFile
#include "Logging.h"			// FOR_EACH_LOG_PREF
#include "Miscellaneous.h"		// MemoryTextList, GetMemoryTextList
#include "Platform.h"			// GetString, GetMilliseconds, Beep, CopyToClipboard
#include "PreferenceMgr.h"		// Preference, gEmuPrefs
#include "ROMStubs.h"			// FSCustomControl, SysLibFind, IntlSetStrictChecks
#include "Strings.r.h"			// kStr_AppName
#include "Skins.h"				// SkinNameList

#if HAS_TRACER
#include "TracerPlatform.h"		// gTracer
#endif

#include "algorithm"			// find, remove_if, unary_function<>

#include "PHEMNativeIF.h"

#if !PLATFORM_UNIX
static void		PrvAppendDescriptors	(EmTransportDescriptorList& menuItems,
										 const EmTransportDescriptorList& rawItems);
static void		PrvAppendDescriptors	(EmTransportDescriptorList& menuItems,
										 const string& rawItem);
static string	PrvGetMenuItemText		(EmDlgID whichMenu,
										 const EmTransportDescriptor& item);
static void		PrvAppendMenuItems		(EmDlgRef dlg, EmDlgItemID dlgItem,
										 const EmTransportDescriptorList& menuItems,
										 const EmTransportDescriptor& pref,
										 const string& socketAddr = string());
#endif	// !PLATFORM_UNIX


/***********************************************************************
 *
 * FUNCTION:	DoGetFile
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgItemID EmDlg::DoGetFile (	EmFileRef& result,
								const string& prompt,
								const EmDirRef& defaultPath,
								const EmFileTypeList& filterList)
{
	DoGetFileParameters	parameters (result, prompt, defaultPath, filterList);

	return EmDlg::RunDialog (EmDlg::HostRunGetFile, &parameters);
}


/***********************************************************************
 *
 * FUNCTION:	DoGetFileList
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgItemID EmDlg::DoGetFileList (	EmFileRefList& results,
									const string& prompt,
									const EmDirRef& defaultPath,
									const EmFileTypeList& filterList)
{
	DoGetFileListParameters	parameters (results, prompt, defaultPath, filterList);

	return EmDlg::RunDialog (EmDlg::HostRunGetFileList, &parameters);
}


/***********************************************************************
 *
 * FUNCTION:	DoPutFile
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgItemID EmDlg::DoPutFile (	EmFileRef& result,
								const string& prompt,
								const EmDirRef& defaultPath,
								const EmFileTypeList& filterList,
								const string& defaultName)
{
	DoPutFileParameters	parameters (result, prompt, defaultPath, filterList, defaultName);

	return EmDlg::RunDialog (EmDlg::HostRunPutFile, &parameters);
}


/***********************************************************************
 *
 * FUNCTION:	DoGetDirectory
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgItemID EmDlg::DoGetDirectory (	EmDirRef& result,
									const string& prompt,
									const EmDirRef& defaultPath)
{
	DoGetDirectoryParameters	parameters (result, prompt, defaultPath);

	return EmDlg::RunDialog (EmDlg::HostRunGetDirectory, &parameters);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoAboutBox
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgItemID EmDlg::DoAboutBox (void)
{
	return EmDlg::RunDialog (EmDlg::HostRunAboutBox, NULL);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoSessionNew
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct SessionNewData
{
	Configuration*			cfg;
	Configuration			fWorkingCfg;
	EmDeviceList			fDevices;
};


void EmDlg::PrvBuildROMMenu (const EmDlgContext& context)
{
	EmDlgRef		dlg		= context.fDlg;
	SessionNewData&	data	= *(SessionNewData*) context.fUserData;
	Configuration&	cfg		= data.fWorkingCfg;

	EmDlg::ClearMenu (dlg, kDlgItemNewROM);

	EmDlg::AppendToMenu (dlg, kDlgItemNewROM, Platform::GetString (kStr_OtherMRU));
	EmDlg::AppendToMenu (dlg, kDlgItemNewROM, std::string());

	EmFileRefList	romList;
	gEmuPrefs->GetROMMRU (romList);

	if (romList.size () == 0)
	{
		EmDlg::AppendToMenu (dlg, kDlgItemNewROM, Platform::GetString (kStr_EmptyMRU));
		EmDlg::DisableMenuItem (dlg, kDlgItemNewROM, 2);
		EmDlg::SetItemValue (dlg, kDlgItemNewROM, 2);
		cfg.fROMFile = EmFileRef ();
	}
	else
	{
		int 	menuID		= 2;
		Bool	selected	= false;

		EmFileRefList::iterator	iter = romList.begin ();
		while (iter != romList.end ())
		{
			EmDlg::AppendToMenu (dlg, kDlgItemNewROM, iter->GetName());

			if (cfg.fROMFile == *iter)
			{
				EmDlg::SetItemValue (dlg, kDlgItemNewROM, menuID);
				selected = true;
			}

			++iter;
			++menuID;
		}

		//
		// If we couldn't find the last-used ROM, try to find
		// one that we can use.
		//

		if (!selected) 
		{
			EmFileRefList::iterator	iter = romList.begin ();
			while (iter != romList.end ())
			{
				if (EmDlg::PrvCanUseROMFile (*iter) != kROMFileUnknown)
				{
					EmDlg::SetItemValue (dlg, kDlgItemNewROM, 2 + (iter - romList.begin ()));
					cfg.fROMFile = *iter;
					return;
				}

				++iter;
			}

			//
			// If we *still* can't find a ROM image to use, just
			// zap the whole list and try again with an empty one.
			//

			Preference<EmFileRefList> pref (kPrefKeyROM_MRU);
			pref = EmFileRefList ();
			EmDlg::PrvBuildROMMenu (context);
		}
	}
}


struct PrvSupportsROM : unary_function<EmDevice&, bool>
{
	PrvSupportsROM(EmROMReader& inROM) : ROM(inROM) {}	
	bool operator()(EmDevice& item)
	{
		return !item.SupportsROM(ROM);
	}

private:
	const EmROMReader& ROM;
};


void EmDlg::PrvBuildDeviceMenu (const EmDlgContext& context)
{
	EmDlgRef				dlg			= context.fDlg;
	SessionNewData&			data		= *(SessionNewData*) context.fUserData;
	Configuration&			cfg			= data.fWorkingCfg;

	EmDeviceList			devices		= EmDevice::GetDeviceList ();

	EmDeviceList::iterator	iter		= devices.begin();
	EmDeviceList::iterator	devices_end	= devices.end();
	int 					menuID		= 0;
	Bool					selected	= false;
	unsigned int			version		= 0;

	PrvFilterDeviceList(cfg.fROMFile, devices, devices_end, version);
	iter = devices.begin();

	if (iter == devices_end)
	{
		/* We filtered out too many things, so reset to displaying all */
		devices_end = devices.end();
	}

	data.fDevices = EmDeviceList(iter, devices_end);

	EmDlg::ClearMenu (dlg, kDlgItemNewDevice);

	while (iter != devices_end)
	{
		EmDlg::AppendToMenu (dlg, kDlgItemNewDevice, iter->GetMenuString ());

		if (cfg.fDevice == *iter)
		{
			EmDlg::SetItemValue (dlg, kDlgItemNewDevice, menuID);
			selected = true;
		}

		++iter;
		++menuID;
	}

	if (!selected)
	{
		EmDlg::SetItemValue (dlg, kDlgItemNewDevice, menuID-1);
		cfg.fDevice = data.fDevices [menuID-1];

		/* Rely on caller to (always) invoke PrvBuildSkinMenu */
	}
}


void EmDlg::PrvBuildSkinMenu (const EmDlgContext& context)
{
	EmDlgRef				dlg			= context.fDlg;
	SessionNewData&			data		= *(SessionNewData*) context.fUserData;
	Configuration&			cfg			= data.fWorkingCfg;

	SkinNameList			skins;
	::SkinGetSkinNames (cfg.fDevice, skins);
	SkinName				chosenSkin	= ::SkinGetSkinName (cfg.fDevice);

	SkinNameList::iterator	iter		= skins.begin ();
	int 					menuID		= 0;

	EmDlg::ClearMenu (dlg, kDlgItemNewSkin);

	while (iter != skins.end())
	{
		EmDlg::AppendToMenu (dlg, kDlgItemNewSkin, *iter);

		if (chosenSkin == *iter)
		{
			EmDlg::SetItemValue (dlg, kDlgItemNewSkin, menuID);
		}

		++iter;
		++menuID;
	}
}


void EmDlg::PrvBuildMemorySizeMenu (const EmDlgContext& context)
{
	EmDlgRef					dlg			= context.fDlg;
	SessionNewData&				data		= *(SessionNewData*) context.fUserData;
	Configuration&				cfg			= data.fWorkingCfg;

	MemoryTextList				sizes;
	::GetMemoryTextList (sizes);
	EmDlg::PrvFilterMemorySizes (sizes, cfg);

	MemoryTextList::iterator	iter		= sizes.begin();
	int							menuID		= 0;
	Bool						selected	= false;

	EmDlg::ClearMenu (dlg, kDlgItemNewMemorySize);

	while (iter != sizes.end())
	{
		EmDlg::AppendToMenu (dlg, kDlgItemNewMemorySize, iter->second);

		if (cfg.fRAMSize == iter->first)
		{
			EmDlg::SetItemValue (dlg, kDlgItemNewMemorySize, menuID);
			selected = true;
		}

		++iter;
		++menuID;
	}

	if (!selected)
	{
		EmDlg::SetItemValue (dlg, kDlgItemNewMemorySize, 0);
		cfg.fRAMSize = sizes[0].first;
	}
}


void EmDlg::PrvNewSessionSetOKButton (const EmDlgContext& context)
{
	EmDlgRef		dlg		= context.fDlg;
	SessionNewData&	data	= *(SessionNewData*) context.fUserData;
	Configuration&	cfg		= data.fWorkingCfg;
	EmFileRef&		rom		= cfg.fROMFile;

	EmDlg::EnableDisableItem (dlg, kDlgItemOK, rom.IsSpecified ());
}


void EmDlg::PrvFilterDeviceList(const EmFileRef& romFile, EmDeviceList& devices,
							EmDeviceList::iterator& devices_end, unsigned int& version)
{
	devices_end = devices.end ();
	version = 0;

	if (romFile.IsSpecified ())
	{
		try
		{
			EmStreamFile	hROM(romFile, kOpenExistingForRead);
			StMemory    	romImage (hROM.GetLength ());

			hROM.GetBytes (romImage.Get (), hROM.GetLength ());

			EmROMReader ROM(romImage.Get (), hROM.GetLength ());

			if (ROM.AcquireCardHeader ())
			{
				version = ROM.GetCardVersion ();

				if (version < 5)
				{
					ROM.AcquireROMHeap ();
					ROM.AcquireDatabases ();
					ROM.AcquireFeatures ();
					ROM.AcquireSplashDB ();
				}
			}

			devices_end = remove_if (devices.begin (), devices.end (),
				PrvSupportsROM (ROM));
		}
		catch (ErrCode)
		{
			/* On any of our errors, don't remove any devices */
		}
	}
}


void EmDlg::PrvFilterMemorySizes (MemoryTextList& sizes, const Configuration& cfg)
{
	RAMSizeType					minSize	= cfg.fDevice.MinRAMSize ();
	MemoryTextList::iterator	iter	= sizes.begin();

	while (iter != sizes.end())
	{
		if (iter->first < minSize)
		{
			sizes.erase (iter);
			iter = sizes.begin ();
			continue;
		}

		++iter;
	}
}


//
// Check out a ROM, and see if we can find any devices for it.  If
// so, return TRUE.  If not and the card header is < 5, warn the
// user but return TRUE anyway.  Otherwise, if we can't identify
// the ROM and it has a v5 card header or greater, show an error
// message and return FALSE.
//

EmROMFileStatus EmDlg::PrvCanUseROMFile (EmFileRef& testRef)
{
	if (!testRef.Exists ())
		return kROMFileUnknown;

	EmDeviceList			devices = EmDevice::GetDeviceList ();
	EmDeviceList::iterator	devices_end;
	unsigned int			version;

	EmDlg::PrvFilterDeviceList (testRef, devices, devices_end, version);

	if (devices_end == devices.begin ())
	{
		// No devices matched

		if (version < 5)
		{
			return kROMFileDubious;
		}

		return kROMFileUnknown;
	}

	return kROMFileOK;
}


EmDlgFnResult EmDlg::PrvSessionNew (EmDlgContext& context)
{
	EmDlgRef		dlg = context.fDlg;
	SessionNewData&	data = *(SessionNewData*) context.fUserData;
	Configuration&	cfg = data.fWorkingCfg;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			//
			// Make a copy of the configuration that we update as the
			// user makes dialog selections.
			//

			cfg = *(data.cfg);

			//
			// If a file is usable, make sure it is in the MRU list.
			//

			if (EmDlg::PrvCanUseROMFile (cfg.fROMFile) != kROMFileUnknown)
			{
				gEmuPrefs->UpdateROMMRU (cfg.fROMFile);
			}

			// If a file is not usable, then zap our reference to it.
			// PrvBuildROMMenu will then try to find an alternate.
			//

			else
			{
				cfg.fROMFile = EmFileRef ();
			}

			//
			// Build our menus.
			//

			EmDlg::PrvBuildROMMenu (context);
			EmDlg::PrvBuildDeviceMenu (context);
			EmDlg::PrvBuildSkinMenu (context);
			EmDlg::PrvBuildMemorySizeMenu (context);
			EmDlg::PrvNewSessionSetOKButton (context);
			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					*(data.cfg) = cfg;

					SkinNameList	skins;
					::SkinGetSkinNames (cfg.fDevice, skins);
					long	menuID = EmDlg::GetItemValue (dlg, kDlgItemNewSkin);

					::SkinSetSkinName (cfg.fDevice, skins[menuID]);

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemNewDevice:
				{
					EmDeviceList::size_type	index	= EmDlg::GetItemValue (dlg, kDlgItemNewDevice);

					cfg.fDevice = data.fDevices [index];

					EmDlg::PrvBuildSkinMenu (context);
					EmDlg::PrvBuildMemorySizeMenu (context);
					break;
				}

				case kDlgItemNewSkin:
				{
					break;
				}

				case kDlgItemNewMemorySize:
				{
					MemoryTextList	sizes;
					::GetMemoryTextList (sizes);
					EmDlg::PrvFilterMemorySizes (sizes, cfg);

					MemoryTextList::size_type	index = EmDlg::GetItemValue (dlg, kDlgItemNewMemorySize);

					cfg.fRAMSize = sizes [index].first;
					break;
				}

				case kDlgItemNewROMBrowse:
					break;

				case kDlgItemNewROM:
				{
					EmFileTypeList	typeList;
					typeList.push_back (kFileTypeROM);

					int			romStatus = kROMFileUnknown;
					EmFileRef	romFileRef;
					EmDirRef	romDirRef (cfg.fROMFile.GetParent ());

					//
					// See what menu item was selected: "Other..." or a
					// ROM from the MRU list.
					//

					int			index = EmDlg::GetItemValue (dlg, kDlgItemNewROM);

					//
					// User selected from MRU list.
					//

					if (index >= 2) 
					{
						romFileRef = gEmuPrefs->GetIndROMMRU (index - 2);
					}

					//
					// Ask user for new ROM.
					//

					else
					{
						if (EmDlg::DoGetFile (romFileRef, "Choose ROM file:",
										romDirRef, typeList) == kDlgItemOK)
						{
						}

						// If the user cancelled from the dialog, restore the
						// previously selected ROM.

						else
						{
							romFileRef = cfg.fROMFile;
						}
					}

					// Respond to the ROM selection.

					romStatus = EmDlg::PrvCanUseROMFile (romFileRef);

					if (romStatus == kROMFileDubious)
					{
						Errors::DoDialog (kStr_UnknownDeviceWarning, kDlgFlags_OK);
					}
					else if (romStatus == kROMFileUnknown)
					{
						Errors::DoDialog (kStr_UnknownDeviceError, kDlgFlags_OK);
					}

					//
					// If we can identify the ROM or we want to go ahead and
					// use it anyway, update our dialog items.
					//

					if (romStatus != kROMFileUnknown)
					{
						cfg.fROMFile = romFileRef;
						gEmuPrefs->UpdateROMMRU (cfg.fROMFile);
					}

					EmDlg::PrvNewSessionSetOKButton (context);

					EmDlg::PrvBuildROMMenu (context);
					EmDlg::PrvBuildDeviceMenu (context);
					EmDlg::PrvBuildSkinMenu (context);
					EmDlg::PrvBuildMemorySizeMenu (context);

					break;
				}

				default:
					EmAssert (false);
			}	// switch (context.fItemID)

			break;
		}	// case kDlgCmdItemSelected

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}	// switch (command)

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoSessionNew (Configuration& cfg)
{
	SessionNewData	data;
	data.cfg = &cfg;

	return EmDlg::RunDialog (EmDlg::PrvSessionNew, &data, kDlgSessionNew);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoSessionSave
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgItemID	EmDlg::DoSessionSave (const string& docName, Bool quitting)
{
	string	appName = Platform::GetString (kStr_AppName);
	string	docName2 (docName);

	if (docName2.empty ())
		docName2 = Platform::GetString (kStr_Untitled);

	DoSessionSaveParameters	parameters (appName, docName2, quitting);

	EmDlgItemID	result = EmDlg::RunDialog (EmDlg::HostRunSessionSave, &parameters);

	EmAssert (result == kDlgItemYes || result == kDlgItemNo || result == kDlgItemCancel);

	return result;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoHordeNew
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct HordeNewData
{
	HordeInfo		info;
	EmDatabaseList	appList;
};


UInt32 EmDlg::PrvSelectedAppNameToIndex (EmDatabaseList list, const string& appName)
{
	if (appName.empty ())
		return 0;

	uint32 index = 0;

	EmDatabaseList::iterator	iter = list.begin ();

	while (iter != list.end ())
	{
		if (strcmp ((*iter).name, appName.c_str ()) == 0)
			return index;

		index += 1;
		iter++;
	}

	return 0;
}


DatabaseInfo EmDlg::PrvSelectedIndexToApp (EmDatabaseList appList, uint32 index)
{
	return appList[index];
}


void EmDlg::PrvSelectedList (EmDatabaseList selectedApps, StringList &selectedAppStrings)
{
	selectedAppStrings.clear ();

	EmDatabaseList::iterator iter = selectedApps.begin ();

	while (iter != selectedApps.end ())
	{
		selectedAppStrings.push_back ((*iter).name);

		++iter;
	}
}


EmDlgFnResult EmDlg::PrvHordeNew (EmDlgContext& context)
{
	EmDlgRef		dlg = context.fDlg;
	HordeNewData&	data = *(HordeNewData*) context.fUserData;
	HordeInfo&		info = data.info;

	// These variables and iterators are used in multiple case statements,
	// so I'm putting them here both so that the compiler will not complain,
	// and so that they can be declared once, so that their names won't be
	// confusing.

	Bool	selected;
	EmDatabaseList	selectedApps;
	StringList		selectedAppStrings;
	EmDlgListIndexList	items;
	EmDlgListIndexList::iterator	iter1;
	EmDatabaseList::iterator	iter2;
	UInt32 currentSelection;

	// Initialize

	selectedApps.clear ();

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			// Get Horde preference settings.

			Preference<HordeInfo>	pref (kPrefKeyHordeInfo);
			info = *pref;

			// Install the settings into the dialog box items.

			EmDlg::SetItemValue (dlg, kDlgItemHrdStart,			info.fStartNumber);
			EmDlg::SetItemValue (dlg, kDlgItemHrdStop,			info.fStopNumber);
			EmDlg::SetItemValue (dlg, kDlgItemHrdDepthSwitch,	info.fDepthSwitch);
			EmDlg::SetItemValue (dlg, kDlgItemHrdDepthSave,		info.fDepthSave);
			EmDlg::SetItemValue (dlg, kDlgItemHrdDepthStop,		info.fDepthStop);
			EmDlg::SetItemValue (dlg, kDlgItemHrdCheckSwitch,	info.fCanSwitch);
			EmDlg::SetItemValue (dlg, kDlgItemHrdCheckSave,		info.fCanSave);
			EmDlg::SetItemValue (dlg, kDlgItemHrdCheckStop,		info.fCanStop);

			// Get the list of applications.

			::GetDatabases (data.appList, kApplicationsOnly);

			// Insert the names of the applications into the list.

			selected = false;
			EmDatabaseList::iterator	iter = data.appList.begin();
			while (iter != data.appList.end())
			{
				DatabaseInfo&	thisDB = *iter;

				// Add the current item.

				EmDlg::AppendToList (dlg, kDlgItemHrdAppList, thisDB.name);

				// If the item we just added is in our "selected" list,
				// then select it, and add it to our list of selected
				// apps (from which the first to be run will be selected
				// by the user).

				EmDatabaseList::iterator	begin = info.fAppList.begin ();
				EmDatabaseList::iterator	end = info.fAppList.end ();

				if (find (begin, end, thisDB) != end)
				{
					selected = true;
					int index = iter - data.appList.begin();
					EmDlg::SelectListItem (dlg, kDlgItemHrdAppList, index);
					selectedApps.push_back (thisDB);
				}

				++iter;
			}

			// If nothing's selected, then select something (the first item).

			if (!selected)
			{
				EmDlg::SelectListItem (dlg, kDlgItemHrdAppList, 0);
			}

			// Set up the selected apps menu from the string list we populated
			// above. Default to the first app on the list (as was done previously,
			// before this menu existed.

			EmDlg::PrvSelectedList (selectedApps, selectedAppStrings);
			currentSelection = EmDlg::PrvSelectedAppNameToIndex (selectedApps, info.fFirstLaunchedAppName);
			EmDlg::ClearMenu (dlg, kDlgItemHrdFirstLaunchedApp);
			EmDlg::AppendToMenu (dlg, kDlgItemHrdFirstLaunchedApp, selectedAppStrings);
			EmDlg::SetItemValue (dlg, kDlgItemHrdFirstLaunchedApp, currentSelection);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					info.fStartNumber	= EmDlg::GetItemValue (dlg, kDlgItemHrdStart);
					info.fStopNumber	= EmDlg::GetItemValue (dlg, kDlgItemHrdStop);
					info.fDepthSwitch	= EmDlg::GetItemValue (dlg, kDlgItemHrdDepthSwitch);
					info.fDepthSave		= EmDlg::GetItemValue (dlg, kDlgItemHrdDepthSave);
					info.fDepthStop		= EmDlg::GetItemValue (dlg, kDlgItemHrdDepthStop);
					info.fCanSwitch		= EmDlg::GetItemValue (dlg, kDlgItemHrdCheckSwitch);
					info.fCanSave		= EmDlg::GetItemValue (dlg, kDlgItemHrdCheckSave);
					info.fCanStop		= EmDlg::GetItemValue (dlg, kDlgItemHrdCheckStop);

					// Get the indexes for the selected items.

					EmDlg::GetSelectedItems (dlg, kDlgItemHrdAppList, items);

					// first make a dummy app list, unordered, so that we can figure out
					// which application was selected.

					DatabaseInfoList	dummyAppList;
					dummyAppList.clear ();
					EmDlgListIndexList::iterator dummyIter = items.begin ();
					
					while (dummyIter != items.end ())
					{
						DatabaseInfo	dummyDbInfo = data.appList[*dummyIter];

						dummyAppList.push_back (dummyDbInfo);

						++dummyIter;
					}

					DatabaseInfo	dbInfoStartApp = EmDlg::PrvSelectedIndexToApp (dummyAppList,
						EmDlg::GetItemValue (dlg, kDlgItemHrdFirstLaunchedApp));
					info.fFirstLaunchedAppName	= dbInfoStartApp.name;

					// push the app to be launched first onto the stack then
					// use the indexes to get the actual items, and add those
					// items to the "result" container.

					info.fAppList.clear ();
					info.fAppList.push_back (dbInfoStartApp);

					EmDlgListIndexList::iterator iter = items.begin ();

					while (iter != items.end ())
					{
						DatabaseInfo	dbInfo = data.appList[*iter];

						// don't add the starting app twice, however harmless that may be

						if (strcmp (dbInfo.name, dbInfoStartApp.name) != 0)
						{
							info.fAppList.push_back (dbInfo);
						}

						++iter;
					}

					// Transfer the new fields to the old fields for Hordes to use.

					info.NewToOld ();

					// Save the preferences

					Preference<HordeInfo>	pref (kPrefKeyHordeInfo);
					pref = info;

					// Startup up the gremlin sub-system.

					Hordes::New (info);

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemHrdAppList:
				case kDlgItemHrdFirstLaunchedApp:

					// First, clean out the list of selected applications,
					// or, alternately, the list of applications that will
					// be run.

					info.fAppList.clear ();
			
					// Get the indices of the selected items.

					EmDlg::GetSelectedItems (dlg, kDlgItemHrdAppList, items);
					
					// Use the indices to get the actual items, and add those
					// items to the "result" container.

					iter1 = items.begin ();
					while (iter1 != items.end())
					{
						DatabaseInfo	dbInfo = data.appList[*iter1];
						info.fAppList.push_back (dbInfo);
						++iter1;
					}
					
					// Now that we have a current fAppList, let's update our popup
					// menu for selecting the first launched app.

					// Initialize the string list that the pop-up menu will use
					// to populate itself, then iterate through the list of applications.

					selectedApps.clear ();

					selected = false;
					iter2 = data.appList.begin();
					while (iter2 != data.appList.end())
					{
						DatabaseInfo&	thisDB = *iter2;

						// If the item we just looked at is in our "selected" list,
						// then select it, and add it to our list of selected
						// apps (from which the first to be run will be selected
						// by the user).

						EmDatabaseList::iterator	begin = info.fAppList.begin ();
						EmDatabaseList::iterator	end = info.fAppList.end ();

						int index = iter2 - data.appList.begin();

						if (find (begin, end, thisDB) != end)
						{
							selected = true;
							EmDlg::SelectListItem (dlg, kDlgItemHrdAppList, index);
							selectedApps.push_back (thisDB);
						}

						++iter2;
					}

					// Set up the selected apps menu from the string list we populated
					// above. Default to the first app on the list (as was done previously,
					// before this menu existed.

					EmDlg::PrvSelectedList (selectedApps, selectedAppStrings);

					if (context.fItemID == kDlgItemHrdFirstLaunchedApp)
					{
						info.fFirstLaunchedAppName = selectedAppStrings
							[EmDlg::GetItemValue (dlg, kDlgItemHrdFirstLaunchedApp)];
					}
					else	// kDlgItemHrdAppList
					{
						currentSelection = EmDlg::PrvSelectedAppNameToIndex (selectedApps, info.fFirstLaunchedAppName);
						EmDlg::ClearMenu (dlg, kDlgItemHrdFirstLaunchedApp);
						EmDlg::AppendToMenu (dlg, kDlgItemHrdFirstLaunchedApp, selectedAppStrings);
						EmDlg::SetItemValue (dlg, kDlgItemHrdFirstLaunchedApp, currentSelection);
					}

					break;

				case kDlgItemHrdStart:
				case kDlgItemHrdStop:
				case kDlgItemHrdCheckSwitch:
				case kDlgItemHrdCheckSave:
				case kDlgItemHrdCheckStop:
				case kDlgItemHrdDepthSwitch:
				case kDlgItemHrdDepthSave:
				case kDlgItemHrdDepthStop:
					break;

				case kDlgItemHrdSelectAll:

					// Initialize the app list that the pop-up menu will use
					// to populate itself, then iterate through the list of applications.

					selectedApps.clear ();

					selected = false;
					iter2 = data.appList.begin ();
					while (iter2 != data.appList.end ())
					{
						DatabaseInfo&	thisDB = *iter2;

						// Regardless of whether this item is really selected or
						// not, select it, since we are Selecting All here.

						int index = iter2 - data.appList.begin ();

						selected = true;
						EmDlg::SelectListItem (dlg, kDlgItemHrdAppList, index);
						selectedApps.push_back (thisDB);

						++iter2;
					}

					// Set up the selected apps menu from the string list we populated
					// above. Default to the first app on the list (as was done previously,
					// before this menu existed.

					EmDlg::PrvSelectedList (selectedApps, selectedAppStrings);
					currentSelection = EmDlg::PrvSelectedAppNameToIndex (selectedApps, info.fFirstLaunchedAppName);
					EmDlg::ClearMenu (dlg, kDlgItemHrdFirstLaunchedApp);
					EmDlg::AppendToMenu (dlg, kDlgItemHrdFirstLaunchedApp, selectedAppStrings);
					EmDlg::SetItemValue (dlg, kDlgItemHrdFirstLaunchedApp, currentSelection);
					break;

				case kDlgItemHrdSelectNone:

					// Initialize the app list that the pop-up menu will use
					// to populate itself, then iterate through the list of applications.

					selectedApps.clear ();

					iter2 = data.appList.begin ();
					while (iter2 != data.appList.end ())
					{
						// Regardless of whether this item is really selected or
						// not, unselect it, since we are Selecting None here.

						int index = iter2 - data.appList.begin ();
						selected = false;
						EmDlg::UnselectListItem (dlg, kDlgItemHrdAppList, index);

						++iter2;
					}

					EmDlg::SelectListItem (dlg, kDlgItemHrdAppList, 0);

					// This set of braces is here to keep the compiler from complaining
					// about initializing within a case

					{
						DatabaseInfo&	thisDB = *(data.appList.begin ());
						selectedApps.push_back (thisDB);
					}

					// Set up the selected apps menu from the string list we populated
					// above. Default to the first app on the list (as was done previously,
					// before this menu existed.

					EmDlg::PrvSelectedList (selectedApps, selectedAppStrings);

					// Update our first launched app pref

					info.fFirstLaunchedAppName = selectedAppStrings[0];

					EmDlg::ClearMenu (dlg, kDlgItemHrdFirstLaunchedApp);
					EmDlg::AppendToMenu (dlg, kDlgItemHrdFirstLaunchedApp, selectedAppStrings);
					EmDlg::SetItemValue (dlg, kDlgItemHrdFirstLaunchedApp, 0);

					break;

				case kDlgItemHrdLogging:
					EmDlg::DoEditLoggingOptions (kGremlinLogging);
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoHordeNew (void)
{
	HordeNewData	data;

	return EmDlg::RunDialog (EmDlg::PrvHordeNew, &data, kDlgHordeNew);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoDatabaseImport
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct DatabaseImportData
{
	EmFileRefList		fFiles;
	EmFileImportMethod	fMethod;
	EmStreamFile*		fStream;
	EmFileImport*		fImporter;
	long				fProgressCurrent;	// Progress into the current file
	long				fProgressBase;		// Progress accumulated by previous files
	long				fProgressMax;		// Progress max value.
	long				fCurrentFile;		// Current file we're installing.
	long				fDoneStart;
};


EmDlgFnResult EmDlg::PrvDatabaseImport (EmDlgContext& context)
{
	EmDlgRef			dlg = context.fDlg;
	DatabaseImportData&	data = *(DatabaseImportData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			data.fStream		= NULL;
			data.fImporter		= NULL;

			try
			{
				data.fCurrentFile		= 0;
				data.fStream			= new EmStreamFile (data.fFiles[data.fCurrentFile], kOpenExistingForRead);
				data.fImporter			= new EmFileImport (*data.fStream, data.fMethod);
				data.fProgressCurrent	= -1;
				data.fProgressBase		= 0;
				data.fProgressMax		= EmFileImport::CalculateProgressMax (data.fFiles, data.fMethod);
				data.fDoneStart		= 0;

				long	remainingFiles	= data.fFiles.size () - data.fCurrentFile;
				string	curAppName		= data.fFiles[data.fCurrentFile].GetName ();

				EmDlg::SetItemValue	(dlg, kDlgItemImpNumFiles,		remainingFiles);
//				EmDlg::SetItemText	(dlg, kDlgItemImpCurAppName,	curAppName);
			}
			catch (...)
			{
				delete data.fImporter;
				delete data.fStream;

				data.fStream		= NULL;
				data.fImporter		= NULL;
			}

			context.fNeedsIdle = true;
			break;
		}

		case kDlgCmdIdle:
		{
			if (data.fImporter)
			{
				// Prevent against recursion (can occur if we need to display an error dialog).

				static Bool	inIdle;
				if (inIdle)
					break;

				inIdle = true;

				// Install a little bit of the file.

				ErrCode	err = data.fImporter->Continue ();

				// If an error occurred, display a dialog.

				if (err)
				{
					string	curAppName		= data.fFiles[data.fCurrentFile].GetName ();
					Errors::SetParameter ("%filename", curAppName);
					Errors::ReportIfError (kStr_CmdInstall, err, 0, false);
				}

				// Everything went OK.  Get the current progress amount.
				// If it changed, update the dialog.
				else
				{
					long	progressCurrent	= data.fImporter->GetProgress ();
					long	progressMax		= data.fProgressMax;

					if (data.fProgressCurrent != progressCurrent)
					{
						data.fProgressCurrent = progressCurrent;

						progressCurrent += data.fProgressBase;

						// Scale progressMax to < 32K (progress control on Windows
						// prefers it that way).

						long	divider = progressMax / (32 * 1024L) + 1;

						progressMax /= divider;
						progressCurrent /= divider;

						long	remainingFiles	= data.fFiles.size () - data.fCurrentFile;
						string	curAppName		= data.fFiles[data.fCurrentFile].GetName ();

						EmDlg::SetItemValue	(dlg, kDlgItemImpNumFiles,		remainingFiles);
//						EmDlg::SetItemText	(dlg, kDlgItemImpCurAppName,	curAppName);
						EmDlg::SetItemMax	(dlg, kDlgItemImpProgress,		progressMax);
						EmDlg::SetItemValue	(dlg, kDlgItemImpProgress,		progressCurrent);
					}
				}

				// If we're done with this file, clean up, and prepare for any
				// subsequent files.

				if (data.fImporter->Done ())
				{
					gEmuPrefs->UpdatePRCMRU (data.fFiles[data.fCurrentFile]);

					data.fProgressCurrent = -1;
					data.fProgressBase += data.fImporter->GetProgress ();

					delete data.fImporter;
					data.fImporter = NULL;

					delete data.fStream;
					data.fStream = NULL;

					data.fCurrentFile++;
					if (data.fCurrentFile < (long) data.fFiles.size ())
					{
						try
						{
							data.fStream	= new EmStreamFile (data.fFiles[data.fCurrentFile], kOpenExistingForRead);
							data.fImporter	= new EmFileImport (*data.fStream, data.fMethod);
						}
						catch (...)
						{
							delete data.fImporter;
							delete data.fStream;

							data.fStream		= NULL;
							data.fImporter		= NULL;
						}
					}
					else
					{
						// There are no more files to install.  Initialize our
						// counter used to delay the closing of the dialog.

						data.fDoneStart = Platform::GetMilliseconds ();
					}
				}

				inIdle = false;
			}

			// After we're done installing all files, wait a little bit in order to
			// see the completed progress dialog.

			else if (Platform::GetMilliseconds () - data.fDoneStart > 500)
			{
				return kDlgResultClose;
			}

			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// Fall thru...
				}

				case kDlgItemCancel:
				{
					if (context.fItemID == kDlgItemCancel)
						data.fImporter->Cancel ();

					delete data.fImporter;
					data.fImporter = NULL;

					return kDlgResultClose;
				}

				case kDlgItemImpNumFiles:
//				case kDlgItemImpCurAppName:
				case kDlgItemImpProgress:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoDatabaseImport (const EmFileRefList& files, EmFileImportMethod method)
{
	// Validate the list of files.

	EmFileRefList					newFileList;
	EmFileRefList::const_iterator	iter = files.begin ();

	while (iter != files.end ())
	{
		if (iter->IsSpecified () && iter->Exists ())
		{
			newFileList.push_back (*iter);
		}
		else
		{
			// !!! Report that the file doesn't exist.
		}

		++iter;
	}

	if (newFileList.size () <= 0)
	{
		return kDlgItemOK;
	}

	// Stop the session so that the files can be safely loaded.

	EmAssert (gSession);
	EmSessionStopper	stopper (gSession, kStopOnSysCall);
	if (!stopper.Stopped ())
		return kDlgItemCancel;

	DatabaseImportData	data;

	data.fFiles		= newFileList;
	data.fMethod	= method;

	return EmDlg::RunDialog (EmDlg::PrvDatabaseImport, &data, kDlgDatabaseImport);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoDatabaseExport
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct ExportDatbaseData
{
	EmDatabaseList	fAllDatabases;
};


Bool EmDlg::PrvExportFile (const DatabaseInfo& db)
{
	UInt16			dbAttributes;
	UInt32			dbType;
	UInt32			dbCreator;
	ErrCode			err = ::DmDatabaseInfo (db.cardNo, db.dbID, NULL,
											&dbAttributes, NULL, NULL,
											NULL, NULL, NULL, NULL,
											NULL, &dbType, &dbCreator);
	Errors::ThrowIfPalmError (err);

	// Figure out the file type.

	EmFileType		type = kFileTypeNone;

	if ((dbAttributes & dmHdrAttrResDB) != 0)
	{
		type = kFileTypePalmApp;
	}
	else
	{
		if (dbCreator == sysFileCClipper)
			type = kFileTypePalmQA;
		else
			type = kFileTypePalmDB;
	}

	EmFileRef		result;
	string			prompt ("Save as...");
	EmDirRef		defaultPath;
	EmFileTypeList	filterList;
	string			defaultName (db.dbName);

	string			extension = type == kFileTypePalmApp ? ".prc" :
								type == kFileTypePalmQA ? ".pqa" : ".pdb";

	if (!::EndsWith (defaultName.c_str (), extension.c_str ()))
	{
		defaultName += extension;
	}

	filterList.push_back (type);
	filterList.push_back (kFileTypePalmAll);
	filterList.push_back (kFileTypeAll);

	EmDlgItemID	item = EmDlg::DoPutFile (	result,
											prompt,
											defaultPath,
											filterList,
											defaultName);

	if (item == kDlgItemOK)
	{
		EmStreamFile	stream (result, kCreateOrEraseForUpdate,
								kFileCreatorInstaller, type);

		::SavePalmFile (stream, db.cardNo, db.dbName);
	}

	return (item == kDlgItemOK);
}


EmDlgFnResult EmDlg::PrvDatabaseExport (EmDlgContext& context)
{
	EmDlgRef			dlg = context.fDlg;
	ExportDatbaseData&	data = *(ExportDatbaseData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			// Get the list of installed databases.

			::GetDatabases (data.fAllDatabases, kAllDatabases);

			// Add then names of the databases to the list.

			EmDatabaseList::iterator	iter = data.fAllDatabases.begin();
			while (iter != data.fAllDatabases.end())
			{
				string	itemText (iter->dbName);

				// If the database name is different from any user-visible
				// name, then use both of them.

				if (strcmp (iter->dbName, iter->name) != 0)
				{
					itemText += " (";
					itemText += iter->name;
					itemText += ")";
				}

				EmDlg::AppendToList (dlg, kDlgItemExpDbList, itemText.c_str ());
				++iter;
			}
			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// Get the indexes for the selected items.

					EmDlgListIndexList	items;
					EmDlg::GetSelectedItems(dlg, kDlgItemExpDbList, items);

					// Export each item.

					Bool	cancel = false;
					EmDlgListIndexList::iterator	iter = items.begin ();
					while (!cancel && iter != items.end ())
					{
						DatabaseInfo&	db = data.fAllDatabases[*iter];

						cancel = !EmDlg::PrvExportFile (db);

						++iter;
					}
					
					if (cancel)
						break;

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemExpDbList:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoDatabaseExport (void)
{
	ExportDatbaseData	data;

	return EmDlg::RunDialog (EmDlg::PrvDatabaseExport, &data, kDlgDatabaseExport);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoReset
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgFnResult EmDlg::PrvReset (EmDlgContext& context)
{
	EmDlgRef		dlg = context.fDlg;
	EmResetType&	choice = *(EmResetType*) context.fUserData;

        PHEM_Log_Msg("In EmDlg::PrvReset");
	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
                        PHEM_Log_Msg("Doing init.");
			EmDlg::SetItemValue (dlg, kDlgItemRstSoft, 1);
                        PHEM_Log_Msg("Finishing init.");
			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
                                        PHEM_Log_Msg("Got OK");
					if (EmDlg::GetItemValue (dlg, kDlgItemRstSoft))
					{
                                                PHEM_Log_Msg("Doing soft");
						choice = kResetSoft;
					}
					else if (EmDlg::GetItemValue (dlg, kDlgItemRstHard))
					{
                                                PHEM_Log_Msg("Doing hard");
						choice = kResetHard;
					}
					else
					{
                                                PHEM_Log_Msg("Doing debug");
						EmAssert (EmDlg::GetItemValue (dlg, kDlgItemRstDebug));
						choice = kResetDebug;
					}

					if (EmDlg::GetItemValue (dlg, kDlgItemRstNoExt) != 0)
					{
                                                PHEM_Log_Msg("Doing no reset");
						choice = (EmResetType) ((int) choice | kResetNoExt);
					}
					// Fall thru...
				}

				case kDlgItemCancel:
				{
                                        PHEM_Log_Msg("ResultClose");
					return kDlgResultClose;
				}

				case kDlgItemRstSoft:
				case kDlgItemRstHard:
				case kDlgItemRstDebug:
				case kDlgItemRstNoExt:
                                        PHEM_Log_Msg("Got item ID:");
                                        PHEM_Log_Place(context.fItemID);
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
                        PHEM_Log_Msg("Destroy?");
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoReset (EmResetType& type)
{
	return EmDlg::RunDialog (EmDlg::PrvReset, &type, kDlgReset);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoROMTransferQuery
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct ROMTransferQueryData
{
#if !PLATFORM_UNIX
	EmTransportDescriptorList	fPortNameList;
#endif
	EmTransport::Config**		fConfig;
};


#if !PLATFORM_UNIX
void EmDlg::PrvGetDlqPortItemList (EmTransportDescriptorList& menuItems)
{
	EmTransportDescriptorList	descListSerial;
	EmTransportDescriptorList	descListUSB;

	EmTransportSerial::GetDescriptorList (descListSerial);
	EmTransportUSB::GetDescriptorList (descListUSB);

	::PrvAppendDescriptors (menuItems, descListSerial);
	::PrvAppendDescriptors (menuItems, descListUSB);
}
#endif


void EmDlg::PrvConvertBaudListToStrings (const EmTransportSerial::BaudList& baudList,
									StringList& baudStrList)
{
	EmTransportSerial::BaudList::const_iterator	iter = baudList.begin();
	while (iter != baudList.end())
	{
		char	buffer[20];
		::FormatInteger (buffer, *iter);
		strcat (buffer, " bps");
		baudStrList.push_back (buffer);
		++iter;
	}
}

EmDlgFnResult EmDlg::PrvROMTransferQuery (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	ROMTransferQueryData&	data = *(ROMTransferQueryData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			// On Unix, the port is specified in an edit text item.
			// On other platforms, we present a list of ports in a menu.

			// !!! TBD: restore port setting from a preference.

#if PLATFORM_UNIX
			EmDlg::SetItemText (dlg, kDlgItemDlqPortList, "/dev/ttyUSB0");
#else
			Preference<string>	pref (kPrefKeyPortDownload);
			::PrvAppendMenuItems (dlg, kDlgItemDlqPortList, data.fPortNameList, *pref);
#endif

			EmTransportSerial::BaudList		baudList;
			EmTransportSerial::GetSerialBaudList (baudList);

			StringList		baudStrList;
			EmDlg::PrvConvertBaudListToStrings (baudList, baudStrList);

			EmDlg::AppendToMenu (dlg, kDlgItemDlqBaudList, baudStrList);
			EmDlg::SetItemValue (dlg, kDlgItemDlqBaudList, 0);		// 115K baud

			// Load the instructions string.  Load it in 9 parts,
			// as the entire string is greater than:
			//
			//	Rez can handle on the Mac.
			//	MSDev's string editor on Windows.
			//	The ANSI-allowed string length (for Unix).

			string	ins;
			for (int ii = 0; ii < 9; ++ii)
			{
				string	temp = Platform::GetString (kStr_ROMTransferInstructions + ii);
				ins += temp;
			}

			EmDlg::SetItemText (dlg, kDlgItemDlqInstructions, ins);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// Get the selected settings.

#if PLATFORM_UNIX
					string					portName	= EmDlg::GetItemText (dlg, kDlgItemDlqPortList);
					EmTransportDescriptor	portDesc (portName);
#else
					long					portNum		= EmDlg::GetItemValue (dlg, kDlgItemDlqPortList);
					EmTransportDescriptor	portDesc	= data.fPortNameList [portNum];
					string					portName	= portDesc.GetSchemeSpecific ();
#endif

					EmTransportType transportType;	
					transportType = portDesc.GetType ();

					switch (transportType)
					{
						case kTransportSerial:
						{
							long	baudNum = EmDlg::GetItemValue (dlg, kDlgItemDlqBaudList);

							EmTransportSerial::BaudList		baudList;
							EmTransportSerial::GetSerialBaudList (baudList);

							EmTransportSerial::ConfigSerial*	serConfig = new EmTransportSerial::ConfigSerial;

							serConfig->fPort			= portName;
							serConfig->fBaud			= baudList [baudNum];
							serConfig->fParity			= EmTransportSerial::kNoParity;
							serConfig->fStopBits		= 1;
							serConfig->fDataBits		= 8;
							serConfig->fHwrHandshake	= true;

							*data.fConfig = serConfig;

							break;
						}

						case kTransportUSB:
						{
							EmTransportUSB::ConfigUSB*	usbConfig = new EmTransportUSB::ConfigUSB;

							*data.fConfig = usbConfig;

							break;
						}

						case kTransportSocket:
						case kTransportNull:
							break;

						case kTransportUnknown:
							EmAssert (false);
							break;
					}

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemDlqInstructions:
				case kDlgItemDlqPortList:
				case kDlgItemDlqBaudList:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoROMTransferQuery (EmTransport::Config*& config)
{
	config = NULL;

	ROMTransferQueryData	data;
	data.fConfig = &config;

#if !PLATFORM_UNIX
	// Generate the list here, and do it just once.  It's important to
	// do it just once, as the list of ports could possibly change between
	// the time they're added to the menu and the time one is chosen to
	// be returned.  This could happen, for example, on the Mac where
	// opening a USB connection creates a new "virtual" serial port.

	EmDlg::PrvGetDlqPortItemList (data.fPortNameList);
#endif

	return EmDlg::RunDialog (EmDlg::PrvROMTransferQuery, &data, kDlgROMTransferQuery);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoROMTransferProgress
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgFnResult EmDlg::PrvROMTransferProgress (EmDlgContext& context)
{
	EmDlgRef		dlg = context.fDlg;
	EmROMTransfer&	transferer = *(EmROMTransfer*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			context.fNeedsIdle = true;
			break;
		}

		case kDlgCmdIdle:
		{
			try
			{
				if (!transferer.Continue (dlg))
					return kDlgResultClose;
			}
			catch (ErrCode /*err*/)
			{
				// !!! Should report error code?
				return kDlgResultClose;
			}

			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// Fall thru...
				}

				case kDlgItemCancel:
				{
					transferer.Abort (dlg);
					return kDlgResultClose;
				}

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoROMTransferProgress (EmROMTransfer& transferer)
{
	return EmDlg::RunDialog (EmDlg::PrvROMTransferProgress, &transferer, kDlgROMTransferProgress);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditPreferences
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct EditPreferencesData
{
	EmTransportDescriptor		prefPortSerial;			// Selected menu item for serial
	EmTransportDescriptor		prefPortIR;				// Selected menu item for IR
	EmTransportDescriptor		prefPortMystery;		// Selected menu item for Mystery
	bool						prefRedirectNetLib;
	bool						prefEnableSounds;
	CloseActionType				prefCloseAction;
	string						prefUserName;

#if !PLATFORM_UNIX
	string						prefPortSerialSocket;	// Socket address for serial
	string						prefPortIRSocket;		// Socket address for serial

	EmTransportDescriptorList	menuItemsSerial;		// Built-up menu for serial
	EmTransportDescriptorList	menuItemsIR;			// Built-up menu for IR
	EmTransportDescriptorList	menuItemsMystery;		// Built-up menu for Mystery
#endif
};


void EmDlg::PrvEditPrefToDialog (EmDlgContext& context)
{
	EmDlgRef				dlg		= context.fDlg;
	EditPreferencesData&	data	= *(EditPreferencesData*) context.fUserData;

#if !PLATFORM_UNIX

	// ------------------------------------------------------------
	// Set up the port menus.
	// ------------------------------------------------------------

	EmDlg::ClearMenu (dlg, kDlgItemPrfRedirectSerial);
	EmDlg::ClearMenu (dlg, kDlgItemPrfRedirectIR);
	EmDlg::ClearMenu (dlg, kDlgItemPrfRedirectMystery);

	::PrvAppendMenuItems (dlg, kDlgItemPrfRedirectSerial,	data.menuItemsSerial,	data.prefPortSerial,	data.prefPortSerialSocket);
	::PrvAppendMenuItems (dlg, kDlgItemPrfRedirectIR,		data.menuItemsIR,		data.prefPortIR,		data.prefPortIRSocket);
	::PrvAppendMenuItems (dlg, kDlgItemPrfRedirectMystery,	data.menuItemsMystery,	data.prefPortMystery);

	// IR emulation appears to be consistantly flaky no matter what's
	// used for the underlying transport (local TCP, serial, etc.).
	// So always disable it for now, until we can figure out why it's
	// flakey and can work around it.

	EmDlg::DisableItem (dlg, kDlgItemPrfRedirectIR);

#else

	// ------------------------------------------------------------
	// Set up the port text items.
	// ------------------------------------------------------------

	EmDlg::SetItemText (dlg, kDlgItemPrfRedirectSerial,		data.prefPortSerial.GetMenuName ());
	EmDlg::SetItemText (dlg, kDlgItemPrfRedirectIR,			data.prefPortIR.GetMenuName ());
	EmDlg::SetItemText (dlg, kDlgItemPrfRedirectMystery,	data.prefPortMystery.GetMenuName ());

#endif

	// ------------------------------------------------------------
	// Set up the Redirect NetLib and Enable Sounds checkboxes.
	// ------------------------------------------------------------

	EmDlg::SetItemValue (dlg, kDlgItemPrfRedirectNetLib, data.prefRedirectNetLib);
	EmDlg::SetItemValue (dlg, kDlgItemPrfEnableSound, data.prefEnableSounds);

	// ------------------------------------------------------------
	// Set up the Close Action radio buttons.
	// ------------------------------------------------------------

	if (data.prefCloseAction == kSaveAlways)
	{
		EmDlg::SetItemValue (dlg, kDlgItemPrfSaveAlways, 1);
	}
	else if (data.prefCloseAction == kSaveAsk)
	{
		EmDlg::SetItemValue (dlg, kDlgItemPrfSaveAsk, 1);
	}
	else
	{
		EmDlg::SetItemValue (dlg, kDlgItemPrfSaveNever, 1);
	}

	// ------------------------------------------------------------
	// Set up the HotSync User Name edit text item.
	// ------------------------------------------------------------

	EmDlg::SetItemText (dlg, kDlgItemPrfUserName, data.prefUserName);
}


void EmDlg::PrvEditPrefFromDialog (EmDlgContext& context)
{
	EmDlgRef				dlg		= context.fDlg;
	EditPreferencesData&	data	= *(EditPreferencesData*) context.fUserData;

#if !PLATFORM_UNIX

	// ------------------------------------------------------------
	// Save the port settings.
	// ------------------------------------------------------------

	data.prefPortSerial		= data.menuItemsSerial	[EmDlg::GetItemValue (dlg, kDlgItemPrfRedirectSerial)];
	data.prefPortIR			= data.menuItemsIR		[EmDlg::GetItemValue (dlg, kDlgItemPrfRedirectIR)];
	data.prefPortMystery	= data.menuItemsMystery	[EmDlg::GetItemValue (dlg, kDlgItemPrfRedirectMystery)];

#else

	// ------------------------------------------------------------
	// Save the port settings.
	// ------------------------------------------------------------

	data.prefPortSerial		= EmDlg::GetItemText (dlg, kDlgItemPrfRedirectSerial);
	data.prefPortIR			= EmDlg::GetItemText (dlg, kDlgItemPrfRedirectIR);
	data.prefPortMystery	= EmDlg::GetItemText (dlg, kDlgItemPrfRedirectMystery);

#endif

	// ------------------------------------------------------------
	// Save the Redirect NetLib and  Enable Sounds settings.
	// ------------------------------------------------------------

	data.prefRedirectNetLib	= EmDlg::GetItemValue (dlg, kDlgItemPrfRedirectNetLib) != 0;
	data.prefEnableSounds	= EmDlg::GetItemValue (dlg, kDlgItemPrfEnableSound) != 0;

	// ------------------------------------------------------------
	// Save the Close Action settings.
	// ------------------------------------------------------------

	if (EmDlg::GetItemValue (dlg, kDlgItemPrfSaveAlways) != 0)
	{
		data.prefCloseAction = kSaveAlways;
	}
	else if (EmDlg::GetItemValue (dlg, kDlgItemPrfSaveAsk) != 0)
	{
		data.prefCloseAction = kSaveAsk;
	}
	else
	{
		data.prefCloseAction = kSaveNever;
	}

	// ------------------------------------------------------------
	// Save the HotSync User Name setting.
	// ------------------------------------------------------------

	data.prefUserName = EmDlg::GetItemText (dlg, kDlgItemPrfUserName);
}


#if !PLATFORM_UNIX

// Build up descriptors lists for each menu.

void EmDlg::PrvBuildDescriptorLists (EmDlgContext& context)
{
	EditPreferencesData&	data = *(EditPreferencesData*) context.fUserData;

	EmTransportDescriptorList	descListNull;
	EmTransportDescriptorList	descListSerial;
	EmTransportDescriptorList	descListSocket;

	EmTransportNull::GetDescriptorList (descListNull);
	EmTransportSerial::GetDescriptorList (descListSerial);
	EmTransportSocket::GetDescriptorList (descListSocket);

	data.menuItemsSerial.clear ();
	data.menuItemsIR.clear ();
	data.menuItemsMystery.clear ();

	::PrvAppendDescriptors (data.menuItemsSerial, descListNull);
	::PrvAppendDescriptors (data.menuItemsSerial, descListSerial);
	::PrvAppendDescriptors (data.menuItemsSerial, descListSocket);

	::PrvAppendDescriptors (data.menuItemsIR, descListNull);
	::PrvAppendDescriptors (data.menuItemsIR, descListSerial);
	::PrvAppendDescriptors (data.menuItemsIR, descListSocket);

	::PrvAppendDescriptors (data.menuItemsMystery, descListNull);
	::PrvAppendDescriptors (data.menuItemsMystery, descListSerial);

	// Find the socket descriptors, and add the socket-specific information.

	{
		EmTransportDescriptorList::iterator	iter = data.menuItemsSerial.begin ();
		while (iter != data.menuItemsSerial.end ())
		{
			if (iter->GetType () == kTransportSocket)
			{
				*iter = EmTransportDescriptor (iter->GetScheme () + ":" + data.prefPortSerialSocket);
				break;
			}

			++iter;
		}
	}

	{
		EmTransportDescriptorList::iterator	iter = data.menuItemsIR.begin ();
		while (iter != data.menuItemsIR.end ())
		{
			if (iter->GetType () == kTransportSocket)
			{
				*iter = EmTransportDescriptor (iter->GetScheme () + ":" + data.prefPortIRSocket);
				break;
			}

			++iter;
		}
	}
}

#endif	// !UNIX


// Transfer the preferences into local variables so that we can
// change them with impunity.  Also build up some menu lists.

void EmDlg::PrvGetEditPreferences (EmDlgContext& context)
{
	EditPreferencesData&	data = *(EditPreferencesData*) context.fUserData;

	Preference<EmTransportDescriptor>	prefPortSerial		(kPrefKeyPortSerial);
	Preference<EmTransportDescriptor>	prefPortIR			(kPrefKeyPortIR);
	Preference<EmTransportDescriptor>	prefPortMystery		(kPrefKeyPortMystery);
	Preference<bool>					prefRedirectNetLib	(kPrefKeyRedirectNetLib);
	Preference<bool>					prefEnableSounds	(kPrefKeyEnableSounds);
	Preference<CloseActionType>			prefCloseAction		(kPrefKeyCloseAction);
	Preference<string>					prefUserName		(kPrefKeyUserName);

	Preference<string>					prefPortSerialSocket(kPrefKeyPortSerialSocket);
	Preference<string>					prefPortIRSocket	(kPrefKeyPortIRSocket);

	data.prefPortSerial			= *prefPortSerial;
	data.prefPortIR				= *prefPortIR;
	data.prefPortMystery		= *prefPortMystery;
	data.prefRedirectNetLib		= *prefRedirectNetLib;
	data.prefEnableSounds		= *prefEnableSounds;
	data.prefCloseAction		= *prefCloseAction;
	data.prefUserName			= *prefUserName;

#if !PLATFORM_UNIX
	data.prefPortSerialSocket	= *prefPortSerialSocket;
	data.prefPortIRSocket		= *prefPortIRSocket;

	EmDlg::PrvBuildDescriptorLists (context);
#endif
}


// Transfer our update preferences back to the Preferences system.

void EmDlg::PrvPutEditPreferences (EmDlgContext& context)
{
	EditPreferencesData&	data = *(EditPreferencesData*) context.fUserData;

	Preference<EmTransportDescriptor>	prefPortSerial		(kPrefKeyPortSerial);
	Preference<EmTransportDescriptor>	prefPortIR			(kPrefKeyPortIR);
	Preference<EmTransportDescriptor>	prefPortMystery		(kPrefKeyPortMystery);
	Preference<bool>					prefRedirectNetLib	(kPrefKeyRedirectNetLib);
	Preference<bool>					prefEnableSounds	(kPrefKeyEnableSounds);
	Preference<CloseActionType>			prefCloseAction		(kPrefKeyCloseAction);
	Preference<string>					prefUserName		(kPrefKeyUserName);

	Preference<string>					prefPortSerialSocket(kPrefKeyPortSerialSocket);
	Preference<string>					prefPortIRSocket	(kPrefKeyPortIRSocket);

	prefPortSerial			= data.prefPortSerial;
	prefPortIR				= data.prefPortIR;
	prefPortMystery			= data.prefPortMystery;
	prefRedirectNetLib		= data.prefRedirectNetLib;
	prefEnableSounds		= data.prefEnableSounds;
	prefCloseAction			= data.prefCloseAction;
	prefUserName			= data.prefUserName;

#if !PLATFORM_UNIX
	prefPortSerialSocket	= data.prefPortSerialSocket;
	prefPortIRSocket		= data.prefPortIRSocket;
#endif
}


Bool EmDlg::PrvEditPreferencesValidate (EmDlgContext& context)
{
	EditPreferencesData&	data	= *(EditPreferencesData*) context.fUserData;

	if (data.prefUserName.size () > dlkMaxUserNameLength)
	{
		EmDlg::DoCommonDialog (kStr_UserNameTooLong, kDlgFlags_OK);
		return false;
	}

	return true;
}


#if !PLATFORM_UNIX
// Fetch an IP address from the user, store it in the appropriate
// field, and rebuild the menus.

void EmDlg::PrvGetPrefSocketAddress (EmDlgContext& context)
{
	EmAssert ((context.fItemID == kDlgItemPrfRedirectSerial) ||
		(context.fItemID == kDlgItemPrfRedirectIR));

	EmDlgRef				dlg		= context.fDlg;
	EditPreferencesData&	data	= *(EditPreferencesData*) context.fUserData;

	// Move current settings into our local data.

	EmDlg::PrvEditPrefFromDialog (context);

	// Figure out which descriptor was selected.

	string					address;
	EmTransportDescriptor	selectedDesc;

	if (context.fItemID == kDlgItemPrfRedirectSerial)
	{
		address			= data.prefPortSerialSocket;
		selectedDesc	= data.menuItemsSerial [EmDlg::GetItemValue (dlg, context.fItemID)];
	}
	else if (context.fItemID == kDlgItemPrfRedirectIR)
	{
		address			= data.prefPortIRSocket;
		selectedDesc	= data.menuItemsIR [EmDlg::GetItemValue (dlg, context.fItemID)];
	}

	// If it was for the socket menu item, get the address.

	if (selectedDesc.GetType () == kTransportSocket)
	{
		EmDlgItemID	item = EmDlg::DoGetSocketAddress (address);

		if (item == kDlgItemOK)
		{
			EmTransportDescriptor	prefDesc = EmTransportDescriptor (
				selectedDesc.GetScheme () + ":" + address);

			if (context.fItemID == kDlgItemPrfRedirectSerial)
			{
				data.prefPortSerial			= prefDesc;
				data.prefPortSerialSocket	= address;
			}
			else if (context.fItemID == kDlgItemPrfRedirectIR)
			{
				data.prefPortIR			= prefDesc;
				data.prefPortIRSocket	= address;
			}

			EmDlg::PrvBuildDescriptorLists (context);
			EmDlg::PrvEditPrefToDialog (context);
		}
	}
}
#endif

EmDlgFnResult EmDlg::PrvEditPreferences (EmDlgContext& context)
{
	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			EmDlg::PrvGetEditPreferences (context);
			EmDlg::PrvEditPrefToDialog (context);
			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// Make sure all the preferences are something we can live with.

					EmDlg::PrvEditPrefFromDialog (context);
					if (!EmDlg::PrvEditPreferencesValidate (context))
						break;

					// Save the preferences.

					EmDlg::PrvPutEditPreferences (context);

					// Update the emulated environment with the
					// new preferences.

					if (gSession && EmPatchState::UIInitialized ())
					{
						EmSessionStopper stopper (gSession, kStopOnSysCall);
						if (stopper.Stopped ())
						{
							EditPreferencesData&	data = *(EditPreferencesData*) context.fUserData;
							::SetHotSyncUserName (data.prefUserName.c_str ());

							// Update the transports used for UART emulation.

							gEmuPrefs->SetTransports ();
						}
					}

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemPrfRedirectSerial:
				case kDlgItemPrfRedirectIR:
#if !PLATFORM_UNIX
					EmDlg::PrvGetPrefSocketAddress (context);
#endif
					break;

				case kDlgItemPrfRedirectMystery:
				case kDlgItemPrfRedirectNetLib:
				case kDlgItemPrfEnableSound:
				case kDlgItemPrfSaveAlways:
				case kDlgItemPrfSaveAsk:
				case kDlgItemPrfSaveNever:
				case kDlgItemPrfUserName:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditPreferences (void)
{
	EditPreferencesData	data;

	EmDlgID	dlgID = gApplication->IsBoundFully ()
		? kDlgEditPreferencesFullyBound
		: kDlgEditPreferences;

	return EmDlg::RunDialog (EmDlg::PrvEditPreferences, &data, dlgID);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditLoggingOptions
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct EditLoggingOptionsData
{
	LoggingType		fActiveSet;

	// Define a "uint8" variable for each of the logging options we
	// support.  The name of each variable is of the form "f<name>",
	// where "<name>" is what appear in the FOR_EACH_LOG_PREF macro.

#undef DEFINE_STORAGE
#define DEFINE_STORAGE(name)	uint8	f##name;
	FOR_EACH_LOG_PREF (DEFINE_STORAGE)
};


void EmDlg::PrvFetchLoggingPrefs (EmDlgContext& context)
{
	// Transfer all of the logging values from the preferences
	// to our local storage.

	EditLoggingOptionsData&	data = *(EditLoggingOptionsData*) context.fUserData;

	#undef GET_PREF
	#define GET_PREF(name)							\
		{											\
			Preference<uint8> pref(kPrefKey##name);	\
			data.f##name = *pref;					\
		}

	FOR_EACH_LOG_PREF (GET_PREF)
}


void EmDlg::PrvInstallLoggingPrefs (EmDlgContext& context)
{
	EmSessionStopper	stopper (gSession, kStopNow);

	// Transfer all of the logging values from our local
	// storages to the preferences system.

	EditLoggingOptionsData&	data = *(EditLoggingOptionsData*) context.fUserData;

	#undef SET_PREF
	#define SET_PREF(name)							\
		{											\
			Preference<uint8> pref(kPrefKey##name);	\
			pref = data.f##name;					\
		}

	FOR_EACH_LOG_PREF (SET_PREF)
}


void EmDlg::PrvLoggingPrefsToButtons (EmDlgContext& context)
{
	// Set the buttons in the current panel.

	EditLoggingOptionsData&	data = *(EditLoggingOptionsData*) context.fUserData;
	int	bitmask = data.fActiveSet;

	#undef TO_BUTTON
	#define TO_BUTTON(name)	\
		EmDlg::SetItemValue (context.fDlg, kDlgItemLog##name, (data.f##name & bitmask) != 0);

	FOR_EACH_LOG_PREF (TO_BUTTON)
}


void EmDlg::PrvLoggingPrefsFromButtons (EmDlgContext& context)
{
	// Update our local preference values from the buttons in the current panel.

	EditLoggingOptionsData&	data = *(EditLoggingOptionsData*) context.fUserData;
	int	bitmask = data.fActiveSet;

	#undef FROM_BUTTON
	#define FROM_BUTTON(name)											\
		if (EmDlg::GetItemValue (context.fDlg, kDlgItemLog##name) != 0)	\
			data.f##name |= bitmask;									\
		else															\
			data.f##name &= ~bitmask;

	FOR_EACH_LOG_PREF (FROM_BUTTON)
}


EmDlgFnResult EmDlg::PrvEditLoggingOptions (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditLoggingOptionsData&	data = *(EditLoggingOptionsData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			EmDlg::PrvFetchLoggingPrefs (context);
			EmDlg::PrvLoggingPrefsToButtons (context);

			if (data.fActiveSet == kNormalLogging)
				EmDlg::SetItemValue (dlg, kDlgItemLogNormal, 1);
			else
				EmDlg::SetItemValue (dlg, kDlgItemLogGremlins, 1);

			// Disable unsupported options.

			EmDlg::DisableItem (dlg, kDlgItemLogLogCPUOpcodes);
			EmDlg::DisableItem (dlg, kDlgItemLogLogApplicationCalls);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					EmDlg::PrvLoggingPrefsFromButtons (context);
					EmDlg::PrvInstallLoggingPrefs (context);

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemLogNormal:
				case kDlgItemLogGremlins:
					EmDlg::PrvLoggingPrefsFromButtons (context);
					if (context.fItemID == kDlgItemLogNormal)
						data.fActiveSet = kNormalLogging;
					else
						data.fActiveSet = kGremlinLogging;
					EmDlg::PrvLoggingPrefsToButtons (context);
					break;

				#undef DUMMY_CASE
				#define DUMMY_CASE(name)	case kDlgItemLog##name:
				FOR_EACH_LOG_PREF (DUMMY_CASE)
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditLoggingOptions (LoggingType initialSet)
{
	EditLoggingOptionsData	data;

	data.fActiveSet	= initialSet;

	return EmDlg::RunDialog (EmDlg::PrvEditLoggingOptions, &data, kDlgEditLogging);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditDebuggingOptions
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct EditDebuggingOptionsData
{
	// Define a "Bool" variable for each of the debugging options we
	// support.  The name of each variable is of the form "f<name>",
	// where "<name>" is what appear in the FOR_EACH_REPORT_PREF macro.

#undef DEFINE_STORAGE
#define DEFINE_STORAGE(name)	Bool	f##name;
	FOR_EACH_REPORT_PREF (DEFINE_STORAGE)

	DEFINE_STORAGE(DialogBeep)
};


void EmDlg::PrvFetchDebuggingPrefs (EmDlgContext& context)
{
	// Transfer all of the debugging values from the preferences
	// to our local storage.

	EditDebuggingOptionsData&	data = *(EditDebuggingOptionsData*) context.fUserData;

	#undef GET_PREF
	#define GET_PREF(name)							\
		{											\
			Preference<Bool> pref(kPrefKey##name);	\
			data.f##name = *pref;					\
		}

	FOR_EACH_REPORT_PREF (GET_PREF)

	GET_PREF(DialogBeep)
}


void EmDlg::PrvInstallDebuggingPrefs (EmDlgContext& context)
{
	EmSessionStopper	stopper (gSession, kStopNow);

	// Transfer all of the debugging values from our local
	// storages to the preferences system.

	EditDebuggingOptionsData&	data = *(EditDebuggingOptionsData*) context.fUserData;

	#undef SET_PREF
	#define SET_PREF(name)							\
		{											\
			Preference<Bool> pref(kPrefKey##name);	\
			pref = data.f##name;					\
		}

	FOR_EACH_REPORT_PREF (SET_PREF)


	SET_PREF(DialogBeep)
}


void EmDlg::PrvDebuggingPrefsToButtons (EmDlgContext& context)
{
	// Set the buttons in the current panel.

	EditDebuggingOptionsData&	data = *(EditDebuggingOptionsData*) context.fUserData;

	#undef TO_BUTTON
	#define TO_BUTTON(name)	\
		EmDlg::SetItemValue (context.fDlg, kDlgItemDbg##name, data.f##name);

	FOR_EACH_REPORT_PREF (TO_BUTTON)

	TO_BUTTON(DialogBeep)
}


void EmDlg::PrvDebuggingPrefsFromButtons (EmDlgContext& context)
{
	// Update our local preference values from the buttons in the current panel.

	EditDebuggingOptionsData&	data = *(EditDebuggingOptionsData*) context.fUserData;

	#undef FROM_BUTTON
	#define FROM_BUTTON(name)	\
		data.f##name = EmDlg::GetItemValue (context.fDlg, kDlgItemDbg##name) != 0;

	FOR_EACH_REPORT_PREF (FROM_BUTTON)

	FROM_BUTTON(DialogBeep)
}


EmDlgFnResult EmDlg::PrvEditDebuggingOptions (EmDlgContext& context)
{
//	EmDlgRef					dlg = context.fDlg;
//	EditDebuggingOptionsData&	data = *(EditDebuggingOptionsData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			EmDlg::PrvFetchDebuggingPrefs (context);
			EmDlg::PrvDebuggingPrefsToButtons (context);
			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					EmDlg::PrvDebuggingPrefsFromButtons (context);
					EmDlg::PrvInstallDebuggingPrefs (context);

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				#undef DUMMY_CASE
				#define DUMMY_CASE(name)	case kDlgItemDbg##name:
				FOR_EACH_REPORT_PREF (DUMMY_CASE)

				DUMMY_CASE(DialogBeep)
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditDebuggingOptions (void)
{
	EditDebuggingOptionsData	data;

	return EmDlg::RunDialog (EmDlg::PrvEditDebuggingOptions, &data, kDlgEditDebugging);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditDebuggingOptions
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

// Map from an EmErrorHandlingOption to the menu string for it.

struct
{
	EmErrorHandlingOption	fOption;
	StrCode					fString;
} kMenuItemText[] =
{
	{ kShow,		kStr_ShowInDialog			},
	{ kContinue,	kStr_AutomaticallyContinue	},
	{ kQuit,		kStr_AutomaticallyQuit		},
	{ kSwitch,		kStr_NextGremlin			}
};


// Struct representing a single menu item.

struct EmErrorHandlingMenuItemBundle
{
	EmErrorHandlingOption	fOption;
};


// Menu items for the four menus in the dialog box.

EmErrorHandlingMenuItemBundle kMenuItems1[] =
{
	{ kShow },
	{ kContinue }
};


EmErrorHandlingMenuItemBundle kMenuItems2[] =
{
	{ kShow },
	{ kQuit }
};


EmErrorHandlingMenuItemBundle kMenuItems3[] =
{
	{ kShow },
	{ kContinue },
	{ kSwitch }
};


EmErrorHandlingMenuItemBundle kMenuItems4[] =
{
	{ kShow },
	{ kQuit },
	{ kSwitch }
};


// Struct containing all information for a single menu in the dialog box.

struct EmErrorHandlingMenuBundle
{
	EmErrorHandlingMenuItemBundle*	fMenuItems;		// Menu items
	size_t							fNumItems;		// Number of menu items
	EmDlgItemID						fDlgItem;		// Dialog item to get these items
	PrefKeyType						fPrefKey;		// Preference to get selected option
	PrefKeyType						fLogPrefKey;	// Logging option to turn on
	LoggingType						fLogType;		// Flag in the logging option to turn on
	Bool							fEnableLogging;	// True if logging option should be turned on
};

// Information for the Gremlins Off / On Warning menu.

EmErrorHandlingMenuBundle	gMenu1 =
{
	kMenuItems1,
	countof (kMenuItems1),
	kDlgItemErrWarningOff,
	kPrefKeyWarningOff,
	kPrefKeyLogWarningMessages,
	kNormalLogging
};

// Information for the Gremlins Off / On Error menu.

EmErrorHandlingMenuBundle	gMenu2 =
{
	kMenuItems2,
	countof (kMenuItems2),
	kDlgItemErrErrorOff,
	kPrefKeyErrorOff,
	kPrefKeyLogErrorMessages,
	kNormalLogging
};

// Information for the Gremlins On / On Warning menu.

EmErrorHandlingMenuBundle	gMenu3 =
{
	kMenuItems3,
	countof (kMenuItems3),
	kDlgItemErrWarningOn,
	kPrefKeyWarningOn,
	kPrefKeyLogWarningMessages,
	kGremlinLogging
};

// Information for the Gremlins On / On Error menu.

EmErrorHandlingMenuBundle	gMenu4 =
{
	kMenuItems4,
	countof (kMenuItems4),
	kDlgItemErrErrorOn,
	kPrefKeyErrorOn,
	kPrefKeyLogErrorMessages,
	kGremlinLogging
};


// Return the text of the menu item for the given error handling option.
// Makes use of the kMenuItemText map above.

string EmDlg::PrvMenuItemText (EmErrorHandlingOption item)
{
	for (size_t ii = 0; ii < countof (kMenuItemText); ++ii)
	{
		if (kMenuItemText[ii].fOption == item)
		{
			return Platform::GetString (kMenuItemText[ii].fString);
		}
	}

	EmAssert (false);

	return string();
}


// Build up the menu items in the dialog, based on the information in
// the given EmErrorHandlingMenuBundle.  Results are stored in "items".

void EmDlg::PrvBuildMenu (	EmErrorHandlingMenuBundle&	menu,
							StringList&					items)
{
	size_t	numItems = menu.fNumItems;

	for (size_t ii = 0; ii < numItems; ++ii)
	{
		items.push_back (EmDlg::PrvMenuItemText (menu.fMenuItems[ii].fOption));
	}
};


// Find the menu item index in the given menu corresponding to the
// given EmErrorHandlingOption.  This function is used when selecting
// the initial menu item based on the current preference settings.

long EmDlg::PrvFindIndex (	EmErrorHandlingMenuBundle&	menu,
							EmErrorHandlingOption		toFind)
{
	size_t	numItems = menu.fNumItems;

	for (size_t ii = 0; ii < numItems; ++ii)
	{
		if (menu.fMenuItems[ii].fOption == toFind)
			return (long) ii;
	}

	return -1;
}


// Build up the menu items in the dialog for the given menu, and select
// the initial menu item based on the current preference setting.

void EmDlg::PrvErrorHandlingToDialog (	EmDlgContext&				context,
										EmErrorHandlingMenuBundle&	menu)
{
	EmDlgRef	dlg = context.fDlg;

	StringList	menuItemsText;
	EmDlg::PrvBuildMenu (menu, menuItemsText);

	Preference<EmErrorHandlingOption>	pref (menu.fPrefKey);
	long index = EmDlg::PrvFindIndex (menu, *pref);
	if (index < 0)
		index = 0;

	EmDlg::AppendToMenu (dlg, menu.fDlgItem, menuItemsText);
	EmDlg::SetItemValue (dlg, menu.fDlgItem, index);
}


// Take the currently selected item and use it to establish a new
// preference setting.

void EmDlg::PrvErrorHandlingFromDialog (EmDlgContext&				context,
										EmErrorHandlingMenuBundle&	menu)
{
	EmDlgRef	dlg		= context.fDlg;
	long		index	= EmDlg::GetItemValue (dlg, menu.fDlgItem);

	EmAssert (index >= 0);
	EmAssert (index < (long) menu.fNumItems);

	Preference<EmErrorHandlingOption>	pref (menu.fPrefKey);
	pref = menu.fMenuItems[index].fOption;
}


// Ask the user if they'd like us to change the Logging Option settings
// for them as appropriate.

EmDlgItemID EmDlg::PrvAskChangeLogging (void)
{
	string		msg		= Platform::GetString (kStr_MustTurnOnLogging);
	EmDlgItemID	result	= EmDlg::DoCommonDialog (msg, kDlgFlags_YES_No);

	return result;
}


// Check a single menu item selection, seeing if a corresponding change
// in the logging options needs to be made.

void EmDlg::PrvCheckSetting (	EmDlgContext&				context,
								EmErrorHandlingMenuBundle&	menu)
{
	Preference<uint8>		pref (menu.fLogPrefKey);

	EmDlgRef				dlg			= context.fDlg;
	long					index		= EmDlg::GetItemValue (dlg, menu.fDlgItem);
	EmErrorHandlingOption	option		= menu.fMenuItems[index].fOption;

	Bool					needLogging	= option != kShow;
	Bool					haveLogging	= (*pref & menu.fLogType) != 0;

	menu.fEnableLogging = needLogging && !haveLogging;
}


// Check all menus to see if any of them require a change in the logging options.

Bool EmDlg::PrvCheckSettings (EmDlgContext& context)
{
	EmDlg::PrvCheckSetting (context, gMenu1);
	EmDlg::PrvCheckSetting (context, gMenu2);
	EmDlg::PrvCheckSetting (context, gMenu3);
	EmDlg::PrvCheckSetting (context, gMenu4);

	return
		gMenu1.fEnableLogging ||
		gMenu2.fEnableLogging ||
		gMenu3.fEnableLogging ||
		gMenu4.fEnableLogging;
}


// Enable a single logging option corresponding to the given menu,
// if appropriate.

void EmDlg::PrvEnableLoggingOption (EmErrorHandlingMenuBundle& menu)
{
	if (menu.fEnableLogging)
	{
		Preference<uint8>	pref (menu.fLogPrefKey);
		pref = *pref | menu.fLogType;
	}
}


// Enable all necessary logging options.

void EmDlg::PrvEnableLoggingOptions (void)
{
	EmSessionStopper	stopper (gSession, kStopNow);

	EmDlg::PrvEnableLoggingOption (gMenu1);
	EmDlg::PrvEnableLoggingOption (gMenu2);
	EmDlg::PrvEnableLoggingOption (gMenu3);
	EmDlg::PrvEnableLoggingOption (gMenu4);
}


EmDlgFnResult EmDlg::PrvErrorHandling (EmDlgContext& context)
{
	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			EmDlg::PrvErrorHandlingToDialog (context, gMenu1);
			EmDlg::PrvErrorHandlingToDialog (context, gMenu2);
			EmDlg::PrvErrorHandlingToDialog (context, gMenu3);
			EmDlg::PrvErrorHandlingToDialog (context, gMenu4);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					if (EmDlg::PrvCheckSettings (context))
					{
						if (EmDlg::PrvAskChangeLogging () == kDlgItemNo)
						{
							// Don't commit the changes; don't close the dialog.
							return kDlgResultContinue;
						}

						EmDlg::PrvEnableLoggingOptions ();
					}

					EmSessionStopper	stopper (gSession, kStopNow);

					EmDlg::PrvErrorHandlingFromDialog (context, gMenu1);
					EmDlg::PrvErrorHandlingFromDialog (context, gMenu2);
					EmDlg::PrvErrorHandlingFromDialog (context, gMenu3);
					EmDlg::PrvErrorHandlingFromDialog (context, gMenu4);
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemErrWarningOff:
				case kDlgItemErrErrorOff:
				case kDlgItemErrWarningOn:
				case kDlgItemErrErrorOn:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditErrorHandling (void)
{
	return EmDlg::RunDialog (EmDlg::PrvErrorHandling, NULL, kDlgEditErrorHandling);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditSkins
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct EditSkinsData
{
	EmDevice		fDevice;
	SkinNameList	fSkins;
};


EmDlgFnResult EmDlg::PrvEditSkins (EmDlgContext& context)
{
	EmDlgRef		dlg = context.fDlg;
	EditSkinsData&	data = *(EditSkinsData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			// Get the skin the user has chosen.

			SkinName	prefSkin = ::SkinGetSkinName (data.fDevice);

			// Add each skin in the list to the UI element.

			Bool		selected = false;
			int			index = 0;
			SkinNameList::iterator	skinIter = data.fSkins.begin();

			while (skinIter != data.fSkins.end())
			{
				EmDlg::AppendToList (dlg, kDlgItemSknSkinList, *skinIter);

				// Select the currently-used skin.

				if (*skinIter == prefSkin)
				{
					EmDlg::SelectListItem (dlg, kDlgItemSknSkinList, index);
					selected = true;
				}

				++index;
				++skinIter;
			}

			// Ensure that *something* was selected.

			if (!selected)
			{
				EmDlg::SelectListItem (dlg, kDlgItemSknSkinList, 0);
			}

			// Set up the checkboxes

			{
				Preference<ScaleType>	pref (kPrefKeyScale);
				EmDlg::SetItemValue (dlg, kDlgItemSknDoubleScale, (*pref == 2) ? 1 : 0);
			}

			{
				Preference<RGBType>		pref (kPrefKeyBackgroundColor);
				EmDlg::SetItemValue (dlg, kDlgItemSknWhiteBackground, pref.Loaded () ? 1 : 0);
			}

			{
				Preference<bool>  pref (kPrefKeyDimWhenInactive);
				EmDlg::SetItemValue (dlg, kDlgItemSknDim, *pref ? 1 : 0);
			}

			{
				Preference<bool>  pref (kPrefKeyShowDebugMode);
				EmDlg::SetItemValue (dlg, kDlgItemSknRed, *pref ? 1 : 0);
			}

			{
				Preference<bool>  pref (kPrefKeyShowGremlinMode);
				EmDlg::SetItemValue (dlg, kDlgItemSknGreen, *pref ? 1 : 0);
			}

			{
				Preference<bool>  pref (kPrefKeyStayOnTop);
				EmDlg::SetItemValue (dlg, kDlgItemSknStayOnTop, *pref ? 1 : 0);
			}

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// Update the skin choice.

					// Get the selected skin.

					EmDlgListIndex	index = EmDlg::GetSelectedItem (dlg, kDlgItemSknSkinList);
					EmAssert (index != kDlgItemListNone);

					// Update the preferences.

					::SkinSetSkinName (data.fDevice, data.fSkins[index]);

					// Update the scale.

					Preference<ScaleType>	prefScale (kPrefKeyScale);
					prefScale = (GetItemValue (dlg, kDlgItemSknDoubleScale) != 0) ? 2 : 1;

					// Update the background color.

					if (EmDlg::GetItemValue (dlg, kDlgItemSknWhiteBackground) != 0)
					{
						RGBType				color (0xFF, 0xFF, 0xFF);
						Preference<RGBType>	prefBackgroundColor (kPrefKeyBackgroundColor);
						prefBackgroundColor = color;
					}
					else
					{
						gPrefs->DeletePref (kPrefKeyBackgroundColor);
					}

					// Update "Dim When Inactive" setting.

					{
						Preference<bool>	pref (kPrefKeyDimWhenInactive);
						pref = (GetItemValue (dlg, kDlgItemSknDim) != 0) ? true : false;
					}

					// Update "Show Debug Mode" setting.

					{
						Preference<bool>	pref (kPrefKeyShowDebugMode);
						pref = (GetItemValue (dlg, kDlgItemSknRed) != 0) ? true : false;
					}

					// Update "Show Gremlin Mode" setting.

					{
						Preference<bool>	pref (kPrefKeyShowGremlinMode);
						pref = (GetItemValue (dlg, kDlgItemSknGreen) != 0) ? true : false;
					}

					// Update the stay on top setting.

					{
						Preference<bool>	pref (kPrefKeyStayOnTop);
						pref = (GetItemValue (dlg, kDlgItemSknStayOnTop) != 0) ? true : false;
					}

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemSknSkinList:
				case kDlgItemSknDoubleScale:
				case kDlgItemSknWhiteBackground:
				case kDlgItemSknDim:
				case kDlgItemSknRed:
				case kDlgItemSknGreen:
				case kDlgItemSknStayOnTop:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditSkins (void)
{
	Preference<Configuration>	prefConfiguration (kPrefKeyLastConfiguration);
	Configuration				cfg = *prefConfiguration;

	EditSkinsData	data;
	data.fDevice = cfg.fDevice;

	::SkinGetSkinNames (data.fDevice, data.fSkins);

	return EmDlg::RunDialog (EmDlg::PrvEditSkins, &data, kDlgEditSkins);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditHostFSOptions
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct EditHostFSOptionsData
{
	SlotInfoList	fWorkingInfo;
};

const int kMaxVolumes = 8;

void EmDlg::PrvEditHostFSOptionsOK (EmDlgContext& context)
{
	EditHostFSOptionsData&		data = *(EditHostFSOptionsData*) context.fUserData;

	EmSessionStopper	stopper (gSession, kStopOnSysCall);

	Preference<SlotInfoList>	pref (kPrefKeySlotList);
	SlotInfoList				origList = *pref;
	pref = data.fWorkingInfo;

	// For any that changed, mount/unmount the cards.

	UInt16	refNum;
	Err		err = ::SysLibFind ("HostFS Library", &refNum);

	if (err != errNone || refNum == sysInvalidRefNum)
	{
		EmDlg::DoCommonDialog (kStr_NeedHostFS, kDlgFlags_OK);
		return;
	}

	// We've made sure that we have the HostFS library,
	// so it's OK to try to notify it about changes.

	// Iterate over all the items before the user changed
	// them in the Options dialog.

	SlotInfoList::iterator	iter1 = origList.begin ();
	while (iter1 != origList.end ())
	{
		// Iterate over all the items *after* the user changed
		// them in the dialog.

		SlotInfoList::iterator	iter2 = data.fWorkingInfo.begin ();
		while (iter2 != data.fWorkingInfo.end ())
		{
			// Compare one item from the "before" list to one item
			// in the "after" list.  If they're for the same volume,
			// we will want to compare them further.

			if (iter1->fSlotNumber == iter2->fSlotNumber)
			{
				// The volume in this slot either used to be mounted
				// and now is not, or didn't used to be mounted but
				// now is.  We'll need to tell the HostFS library.

				if (iter1->fSlotOccupied != iter2->fSlotOccupied)
				{
					const UInt32	kCreator	= 'pose';
					const UInt16	kMounted	= 1;
					const UInt16	kUnmounted	= 0;

					UInt16	selector = iter2->fSlotOccupied ? kMounted : kUnmounted;
					void*	cardNum = (void*) iter1->fSlotNumber;

					// Note, in order to make this call, the CPU should be stopped
					// in the UI task.  That's because mounting and unmounting can
					// send out notification.  If the notification is sent out while
					// the current Palm OS task is not the UI task, then the
					// notification manager calls SysTaskWait.  This will switch
					// to another task if it can, and prime a timer to re-awake
					// the background task if not.  However, this timer is based
					// on an interrupt going off, and while we're calling into the
					// ROM, interrupts are turned off, leading to a hang.
					// Therefore, is is imperative that we call this function
					// while the UI task is the current task.

					::FSCustomControl (refNum, kCreator, selector, cardNum, NULL);
				}

				break;
			}

			++iter2;
		}

		++iter1;
	}
}


EmDlgFnResult EmDlg::PrvEditHostFSOptions (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditHostFSOptionsData&	data = *(EditHostFSOptionsData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			// Get the current preferences.

			Preference<SlotInfoList>	pref (kPrefKeySlotList);
			data.fWorkingInfo = *pref;

			// Make sure there are at least kMaxVolume entries.  If not, then
			// fill in the missing ones.

			long	curSize = data.fWorkingInfo.size ();
			while (curSize < kMaxVolumes)
			{
				SlotInfoType	info;
				info.fSlotNumber = curSize + 1;
				info.fSlotOccupied = false;
				data.fWorkingInfo.push_back (info);

				curSize++;
			}

			// Install the volumes into the list.

			EmDlg::ClearList (dlg, kDlgItemHfsList);

			for (int ii = 0; ii < kMaxVolumes; ++ii)
			{
				EmDlg::AppendToList (dlg, kDlgItemHfsList, string (1, (char) ('1' + ii)));
			}

			// Select one of the volumes.

			EmDlg::SelectListItem (dlg, kDlgItemHfsList, 0);

			// Call ourselves to make sure that the other dialog
			// items are properly initialized in light of the
			// currently selected item.

			EmDlgContext	subContext (context);
			subContext.fCommandID = kDlgCmdItemSelected;
			subContext.fItemID = kDlgItemHfsList;
			EmDlg::PrvEditHostFSOptions (subContext);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// User pressed OK, save the changes.

					EmDlg::PrvEditHostFSOptionsOK (context);

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}
				
				case kDlgItemHfsList:
				{
					// User changed the currently selected card.

					EmDlgListIndex	item = EmDlg::GetSelectedItem (dlg, kDlgItemHfsList);
					if (item >= 0)
					{
						SlotInfoType&	info = data.fWorkingInfo[item];

						// Get the path to the card's root.  If empty, display
						// a default message so that the user doesn't just see
						// a blank pane.

						string	fullPath (info.fSlotRoot.GetFullPath ());
						if (fullPath.empty ())
							fullPath = "<Not selected>";

						// Set the root path text and the button that says if
						// it's mounted or not.

						EmDlg::SetItemText (dlg, kDlgItemHfsPath, fullPath);
						EmDlg::SetItemValue (dlg, kDlgItemHfsMounted, info.fSlotOccupied);
					}
					break;
				}

				case kDlgItemHfsPath:
					break;

				case kDlgItemHfsMounted:
				{
					// User toggled the checkbox that says whether or not the
					// card is mounted.  Save the new setting.

					EmDlgListIndex	item = EmDlg::GetSelectedItem (dlg, kDlgItemHfsList);
					if (item >= 0)
					{
						SlotInfoType&	info = data.fWorkingInfo[item];

						info.fSlotOccupied = EmDlg::GetItemValue (dlg, kDlgItemHfsMounted);
					}
					break;
				}

				case kDlgItemHfsBrowse:
				{
					// User clicked on the Browse button.  Bring up a
					// "Get Directory" dialog.

					EmDlgListIndex	item = EmDlg::GetSelectedItem (dlg, kDlgItemHfsList);
					if (item >= 0)
					{
						SlotInfoType&	info = data.fWorkingInfo[item];

						EmDirRef	result;
						string		prompt ("Select a directory:");
						EmDirRef	defaultPath (info.fSlotRoot);

						if (EmDlg::DoGetDirectory (result, prompt, defaultPath) == kDlgItemOK)
						{
							info.fSlotRoot = result;
							EmDlg::SetItemText (dlg, kDlgItemHfsPath, result.GetFullPath ());
						}
					}
					break;
				}

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditHostFSOptions (void)
{
	EditHostFSOptionsData	data;

	return EmDlg::RunDialog (EmDlg::PrvEditHostFSOptions, &data, kDlgEditHostFS);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditBreakpoints
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct EditCodeBreakpointData
{
	Bool	fEnabled;
	emuptr	fAddress;
	string	fCondition;
};


struct EditBreakpointsData
{
	EditCodeBreakpointData	fCodeBreakpoints[dbgTotalBreakpoints];
};


// Enable or disable the Edit and Clear buttons.

void EmDlg::PrvEnableCodeBreakpointControls (EmDlgContext& context, bool enable)
{
	EmDlgRef	dlg = context.fDlg;

	EmDlg::EnableDisableItem (dlg, kDlgItemBrkButtonEdit, enable);
	EmDlg::EnableDisableItem (dlg, kDlgItemBrkButtonClear, enable);
}


// Enable or disable the Start Address and Number Of Bytes edit items.

void EmDlg::PrvEnableDataBreakpointControls (EmDlgContext& context, bool enable)
{
	EmDlgRef	dlg = context.fDlg;

	EmDlg::EnableDisableItem (dlg, kDlgItemBrkStartAddress, enable);
	EmDlg::EnableDisableItem (dlg, kDlgItemBrkNumberOfBytes, enable);
}


// Install the current set of code breakpoints into the list item.

void EmDlg::PrvRefreshCodeBreakpointList (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditBreakpointsData&	data = *(EditBreakpointsData*) context.fUserData;

	EmDlgListIndex	selected = EmDlg::GetSelectedItem (dlg, kDlgItemBrkList);

	EmDlg::ClearList (dlg, kDlgItemBrkList);

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		char text[256];

		if (data.fCodeBreakpoints[ii].fEnabled)
		{
			sprintf (text, "at 0x%08lX", data.fCodeBreakpoints[ii].fAddress);
			string	condition = data.fCodeBreakpoints[ii].fCondition;
			if (!condition.empty ())
			{
				strcat (text, " when ");
				strcat (text, condition.c_str ());
			}
		}
		else
		{
			sprintf (text, "(#%d - not set)", ii);
		}

		EmDlg::AppendToList (dlg, kDlgItemBrkList, text);
	}

	if (selected != kDlgItemListNone)
	{
		EmDlg::SelectListItem (dlg, kDlgItemBrkList, selected);
	}
}


// Copy the breakpoint information from the Debugger globals.

void EmDlg::PrvGetCodeBreakpoints (EmDlgContext& context)
{
	EditBreakpointsData&	data = *(EditBreakpointsData*) context.fUserData;

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		data.fCodeBreakpoints[ii].fEnabled		= gDebuggerGlobals.bp[ii].enabled;
		data.fCodeBreakpoints[ii].fAddress		= (emuptr) gDebuggerGlobals.bp[ii].addr;
		data.fCodeBreakpoints[ii].fCondition	= "";

		BreakpointCondition*	c = gDebuggerGlobals.bpCondition[ii];
		if (c)
		{
			data.fCodeBreakpoints[ii].fCondition = c->source;
		}
	}
}


// Install the breakpoint information into the Debugger globals.

void EmDlg::PrvSetCodeBreakpoints (EmDlgContext& context)
{
	EditBreakpointsData&	data = *(EditBreakpointsData*) context.fUserData;

	for (int ii = 0; ii < dbgTotalBreakpoints; ++ii)
	{
		if (data.fCodeBreakpoints[ii].fEnabled)
		{
			BreakpointCondition*	c = NULL;

			string	condition = data.fCodeBreakpoints[ii].fCondition;
			if (!condition.empty ())
			{
				c = Debug::NewBreakpointCondition (condition.c_str ());
			}

			Debug::SetBreakpoint (ii, data.fCodeBreakpoints[ii].fAddress, c);
		}
		else
		{
			Debug::ClearBreakpoint (ii);
		}
	}
}


EmDlgFnResult EmDlg::PrvEditBreakpoints (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditBreakpointsData&	data = *(EditBreakpointsData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			// Set up the list of code breakpoints.

			EmDlg::PrvGetCodeBreakpoints (context);
			EmDlg::PrvRefreshCodeBreakpointList (context);

			// Initially no breakpoint is selected in the list, so the "edit" and
			// "clear" buttons should be disabled.

			EmDlg::PrvEnableCodeBreakpointControls (context, false);

			// Set up the data breakpoint items

			char	text[256];

			EmDlg::SetItemValue (dlg, kDlgItemBrkCheckEnabled,
				gDebuggerGlobals.watchEnabled ? 1 : 0);

			sprintf (text, "0x%08lX", gDebuggerGlobals.watchAddr);
			EmDlg::SetItemText (dlg, kDlgItemBrkStartAddress, text);

			sprintf (text, "0x%08lX", gDebuggerGlobals.watchBytes);
			EmDlg::SetItemText (dlg, kDlgItemBrkNumberOfBytes, text);

			EmDlg::PrvEnableDataBreakpointControls (context, gDebuggerGlobals.watchEnabled);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// User pressed OK, save the changes.

					EmSessionStopper	stopper (gSession, kStopNow);

					// Set the code breakpoints

					EmDlg::PrvSetCodeBreakpoints (context);


					// Set data breakpoint

					gDebuggerGlobals.watchEnabled = EmDlg::GetItemValue (dlg, kDlgItemBrkCheckEnabled) != 0;

					if (gDebuggerGlobals.watchEnabled)
					{
						gDebuggerGlobals.watchAddr = EmDlg::GetItemValue (dlg, kDlgItemBrkStartAddress);
						gDebuggerGlobals.watchBytes = EmDlg::GetItemValue (dlg, kDlgItemBrkNumberOfBytes);
					}

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}
				
				case kDlgItemBrkList:
				{
					EmDlgListIndex	selected = EmDlg::GetSelectedItem (dlg, kDlgItemBrkList);
					EmDlg::PrvEnableCodeBreakpointControls (context, selected != kDlgItemListNone);
					break;
				}

				case kDlgItemBrkButtonEdit:
				{
					EmDlgListIndex	selected = EmDlg::GetSelectedItem (dlg, kDlgItemBrkList);
					if (selected != kDlgItemListNone)
					{
						EmDlg::DoEditCodeBreakpoint (data.fCodeBreakpoints[selected]);
						EmDlg::PrvRefreshCodeBreakpointList (context);
					}
					break;
				}

				case kDlgItemBrkButtonClear:
				{
					EmDlgListIndex	selected = EmDlg::GetSelectedItem (dlg, kDlgItemBrkList);
					if (selected != kDlgItemListNone)
					{
						data.fCodeBreakpoints[selected].fEnabled = false;
						EmDlg::PrvRefreshCodeBreakpointList (context);
					}
					break;
				}

				case kDlgItemBrkCheckEnabled:
				{
					long	enabled = EmDlg::GetItemValue (dlg, kDlgItemBrkCheckEnabled);
					EmDlg::PrvEnableDataBreakpointControls (context, enabled != 0);
					break;
				}

				case kDlgItemBrkStartAddress:
				{
					break;
				}

				case kDlgItemBrkNumberOfBytes:
				{
					break;
				}

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditBreakpoints (void)
{
	EditBreakpointsData	data;

	return EmDlg::RunDialog (EmDlg::PrvEditBreakpoints, &data, kDlgEditBreakpoints);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditCodeBreakpoint
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgFnResult EmDlg::PrvEditCodeBreakpoint (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditCodeBreakpointData&	data = *(EditCodeBreakpointData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			char	text[256];

			sprintf (text, "0x%08lX", data.fAddress);
			EmDlg::SetItemText (dlg, kDlgItemBrkAddress, text);

			EmDlg::SetItemText (dlg, kDlgItemBrkCondition, data.fCondition);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// User pressed OK, save the changes.
					char	text[256];

					data.fAddress	= EmDlg::GetItemValue (dlg, kDlgItemBrkAddress);
					data.fCondition	= EmDlg::GetItemText (dlg, kDlgItemBrkCondition);

					if (data.fAddress & 1)
					{
						string	formatStr = Platform::GetString (kStr_InvalidAddressNotEven);
						string	addressStr = EmDlg::GetItemText (dlg, kDlgItemBrkAddress);
						sprintf (text, formatStr.c_str (), addressStr.c_str ());
						EmDlg::DoCommonDialog (text, kDlgFlags_OK);
						break;
					}

					if (!EmBankROM::ValidAddress (data.fAddress, 2) &&
						!EmBankSRAM::ValidAddress (data.fAddress, 2) &&
						!EmBankDRAM::ValidAddress (data.fAddress, 2))
					{
						string	formatStr = Platform::GetString (kStr_InvalidAddressNotInROMOrRAM);
						string	addressStr = EmDlg::GetItemText (dlg, kDlgItemBrkAddress);
						sprintf (text, formatStr.c_str (), addressStr.c_str ());
						EmDlg::DoCommonDialog (text, kDlgFlags_OK);
						break;
					}

					if (!data.fCondition.empty ())
					{
						BreakpointCondition* c = NULL;

						c = Debug::NewBreakpointCondition (data.fCondition.c_str ());
						if (!c)
						{
							string	formatStr = Platform::GetString (kStr_CannotParseCondition);
							sprintf (text, formatStr.c_str (), data.fCondition.c_str ());
							EmDlg::DoCommonDialog (text, kDlgFlags_OK);
							break;
						}

						delete c;
					}

					data.fEnabled = true;

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}
				
				case kDlgItemBrkAddress:
				case kDlgItemBrkCondition:
				{
					break;
				}

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditCodeBreakpoint (EditCodeBreakpointData& data)
{
	return EmDlg::RunDialog (EmDlg::PrvEditCodeBreakpoint, &data, kDlgEditCodeBreakpoint);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoEditCodeBreakpoint
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

#if HAS_TRACER
struct EditTracingOptionsData
{
	unsigned short	fTracerType;
};


void EmDlg::PrvPopTracerSettings (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditTracingOptionsData&	data = *(EditTracingOptionsData*) context.fUserData;

	// Propagate settings from controls to internal table

	if (data.fTracerType)
	{
		TracerTypeInfo*	info = gTracer.GetTracerTypeInfo (data.fTracerType);
		EmAssert (info);

		string	value = EmDlg::GetItemText (dlg, kDlgItemTrcTargetValue);
		value = value.substr (0, sizeof (info->paramTmpVal) - 1);
		strcpy (info->paramTmpVal, value.c_str ());

		info->autoConnectTmpState = EmDlg::GetItemValue (dlg, kDlgItemTrcAutoConnect) != 0;
	}
}


void EmDlg::PrvPushTracerSettings (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditTracingOptionsData&	data = *(EditTracingOptionsData*) context.fUserData;

	// Propagate settings from internal table to controls
	if (data.fTracerType == 0)
	{
		EmDlg::SetItemText (dlg, kDlgItemTrcTargetValue, "");
		EmDlg::SetItemText (dlg, kDlgItemTrcTargetText, "");

		EmDlg::DisableItem (dlg, kDlgItemTrcTargetValue);
		EmDlg::DisableItem (dlg, kDlgItemTrcTargetText);

		EmDlg::SetItemValue (dlg, kDlgItemTrcAutoConnect, 0);
		EmDlg::DisableItem (dlg, kDlgItemTrcAutoConnect);
	}
	else
	{
		TracerTypeInfo*	info = gTracer.GetTracerTypeInfo (data.fTracerType);

		EmDlg::SetItemText (dlg, kDlgItemTrcTargetValue, info->paramTmpVal);
		EmDlg::SetItemText (dlg, kDlgItemTrcTargetText, info->paramDescr);

		EmDlg::EnableItem (dlg, kDlgItemTrcTargetValue);
		EmDlg::EnableItem (dlg, kDlgItemTrcTargetText);

		if (info->autoConnectSupport)
		{
			EmDlg::SetItemValue (dlg, kDlgItemTrcAutoConnect, info->autoConnectTmpState);
			EmDlg::EnableItem (dlg, kDlgItemTrcAutoConnect);
		}
		else
		{
			EmDlg::SetItemValue (dlg, kDlgItemTrcAutoConnect, 0);
			EmDlg::DisableItem (dlg, kDlgItemTrcAutoConnect);
		}
	}
}


EmDlgFnResult EmDlg::PrvEditTracingOptions (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditTracingOptionsData&	data = *(EditTracingOptionsData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			// Copy prefs states to internal states

			unsigned short ii;

			for (ii = 1; ii <= gTracer.GetTracerTypeCount (); ++ii)
			{
				TracerTypeInfo*	info = gTracer.GetTracerTypeInfo (ii);
				EmAssert(info);

				strcpy (info->paramTmpVal, info->paramCurVal);
				info->autoConnectTmpState = info->autoConnectCurState;
			}

			// Get the currently selected tracer type.

			data.fTracerType = gTracer.GetCurrentTracerTypeIndex ();

			// Build up the menu of tracer types and select the current one.

			EmDlg::AppendToMenu (dlg, kDlgItemTrcOutput, "None (discards traces)");

			for (ii = 1; ii <= gTracer.GetTracerTypeCount (); ++ii)
			{
				TracerTypeInfo*	info = gTracer.GetTracerTypeInfo (ii);
				EmAssert (info);

				EmDlg::AppendToMenu (dlg, kDlgItemTrcOutput, info->friendlyName);
			}

			EmDlg::SetItemValue (dlg, kDlgItemTrcOutput, data.fTracerType);

			// Install settings for this tracer type.

			EmDlg::PrvPushTracerSettings (context);

			// Show the version information.

			char	version[100];
			gTracer.GetLibraryVersionString(version, 100);
			EmDlg::SetItemText (dlg, kDlgItemTrcInfo, version);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					Bool	parameterValueChanged = false;

					// If the user made a selection...

					if (data.fTracerType)
					{
						// Commit the current settings for the current tracer type.

						EmDlg::PrvPopTracerSettings (context);

						// Get the pointer to the current tracer info.

						TracerTypeInfo*	info = gTracer.GetTracerTypeInfo (data.fTracerType);
						EmAssert (info);

						// If the tracer data changed, make it permanent.

						if (strcmp (info->paramCurVal, info->paramTmpVal) != 0)
						{
							parameterValueChanged = true;
							strcpy (info->paramCurVal, info->paramTmpVal);
						}

						info->autoConnectCurState = info->autoConnectTmpState;
					}

					// Make the selected tracer the current one.

					gTracer.SetCurrentTracerTypeIndex (data.fTracerType, parameterValueChanged);

					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemTrcOutput:
				{
					// Save the current settings for the old tracer type.
					EmDlg::PrvPopTracerSettings (context);

					// Get the new tracer type.
					data.fTracerType = EmDlg::GetItemValue (dlg, kDlgItemTrcOutput);

					// Install the settings for the new tracer type.
					EmDlg::PrvPushTracerSettings (context);

					break;
				}

				case kDlgItemTrcTargetText:
				case kDlgItemTrcTargetValue:
				case kDlgItemTrcInfo:
				{
					break;
				}

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoEditTracingOptions (void)
{
	EditTracingOptionsData	data;

	return EmDlg::RunDialog (EmDlg::PrvEditTracingOptions, &data, kDlgEditTracingOptions);
}
#endif


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoCommonDialog
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

struct EditCommonDialogData
{
	const char*			fMessage;
	EmCommonDialogFlags	fFlags;
	uint32				fFirstBeepTime;
	uint32				fLastBeepTime;
};


struct ButtonState
{
	ButtonState (void) :
		fTextID (0),
		fDefault (false),
		fCancel (false),
		fEnabled (false)
	{
	}

	ButtonState (StrCode textID, Bool def, Bool cancel, Bool enabled) :
		fTextID (textID),
		fDefault (def),
		fCancel (cancel),
		fEnabled (enabled)
	{
	}

	StrCode		fTextID;
	Bool		fDefault;
	Bool		fCancel;
	Bool		fEnabled;
};


EmDlgFnResult EmDlg::PrvCommonDialog (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	EditCommonDialogData&	data = *(EditCommonDialogData*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			// Get the right icon.  They all start out as invisible, so
			// show the right one.

			EmDlgItemID	iconID = kDlgItemCmnIconCaution;

#if 0
#pragma message ("Fix me")
			{
				switch (data.fFlags & kAlertMask)
				{
					case kNoteAlert:	iconID = kDlgItemCmnIconNote;		break;
					case kCautionAlert: iconID = kDlgItemCmnIconCaution;	break;
					case kErrorAlert:	iconID = kDlgItemCmnIconStop;		break;
				}
			}
#endif

			EmDlg::ShowItem (dlg, iconID);

			// Sort out the button situation.

			// Determine what the buttons should say and how they should look.

			ButtonState	buttons[3];
			{
				for (int ii = 0, buttonIndex = 0; ii < 3; ++ii)
				{
					int	flags = GET_BUTTON (ii, data.fFlags);
					if (flags & kButtonVisible)
					{
						StrCode	kStringCodes[] =
						{
							0,
							kStr_OK,
							kStr_Cancel,
							kStr_Yes,
							kStr_No,
							kStr_Continue,
							kStr_Debug,
							kStr_Reset,
							0, 0, 0, 0, 0, 0, 0, 0
						};

						ButtonState	newButton (
							kStringCodes[flags & kButtonMask],
							(flags & kButtonDefault) != 0,
							(flags & kButtonEscape) != 0,
							(flags & kButtonEnabled) != 0);

						buttons[buttonIndex] = newButton;

						++buttonIndex;
					}
				}
			}

			// Make the UI elements match what we determined above.

			for (int ii = 0; ii < 3; ++ii)
			{
				EmDlgItemID	itemID = (EmDlgItemID) (kDlgItemCmnButton1 + ii);

				if (buttons[ii].fTextID)
				{
					// The button is visible.  Set its text and enable/disable it.

					EmDlg::SetItemText (dlg, itemID, buttons[ii].fTextID);
					EmDlg::EnableDisableItem (dlg, itemID, buttons[ii].fEnabled);

					if (buttons[ii].fDefault)
					{
						EmDlg::SetDlgDefaultButton (context, itemID);
					}

					if (buttons[ii].fCancel)
					{
						EmDlg::SetDlgCancelButton (context, itemID);
					}
				}
				else
				{
					// The button is invisible.  Hide it, and move the others over.

					EmDlg::HideItem (dlg, itemID);
				}
			}

			// Measure the text, resize the box it goes into, and set the text.

			EmRect	r		= EmDlg::GetItemBounds (dlg, kDlgItemCmnText);
			int		height	= EmDlg::GetTextHeight (dlg, kDlgItemCmnText, data.fMessage);
			int		delta	= height - r.Height ();

			if (delta > 0)
			{
#if PLATFORM_WINDOWS
				// With PowerPlant and FLTK, resizing the window will resize
				// and move around the elements for us.

				r.fBottom += delta;
				EmDlg::SetItemBounds (dlg, kDlgItemCmnText, r);
#endif

				r = EmDlg::GetDlgBounds (dlg);
				r.fBottom += delta;
				EmDlg::SetDlgBounds (dlg, r);
			}

			EmDlg::SetItemText (dlg, kDlgItemCmnText, data.fMessage);

			// Set us up to idle so that we can beep.

			Preference<Bool>	pref (kPrefKeyDialogBeep);

			if (*pref)
			{
				context.fNeedsIdle = true;
				data.fFirstBeepTime = data.fLastBeepTime = Platform::GetMilliseconds ();
			}

			break;
		}

		case kDlgCmdIdle:
		{
			const uint32	kBeepTimeout	= 60 * 1024L;	// 60 seconds
			const uint32	kBeepInterval	= 2 * 1024L;	// 2 seconds
			uint32			curTime = Platform::GetMilliseconds ();
			uint32			delta1 = curTime - data.fFirstBeepTime;
			uint32			delta2 = curTime - data.fLastBeepTime;

			if (delta1 < kBeepTimeout && delta2 > kBeepInterval)
			{
				data.fLastBeepTime = curTime;
				Platform::Beep ();
			}

			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemCmnButton1:
				case kDlgItemCmnButton2:
				case kDlgItemCmnButton3:
				{
					return kDlgResultClose;
				}

				case kDlgItemCmnButtonCopy:
				{
					string		text = EmDlg::GetItemText (context.fDlg, kDlgItemCmnText);
					ByteList	hostChars;
					ByteList	palmChars;

					copy (text.begin (), text.end (), back_inserter (hostChars));

					Platform::CopyToClipboard (palmChars, hostChars);
					break;
				}

				case kDlgItemCmnText:
				case kDlgItemCmnIconStop:
				case kDlgItemCmnIconCaution:
				case kDlgItemCmnIconNote:
					break;

				case kDlgItemOK:
					EmAssert (false);
					break;

				case kDlgItemCancel:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoCommonDialog (StrCode msg,
								   EmCommonDialogFlags flags)
{
	string	str (Platform::GetString (msg));
	return EmDlg::DoCommonDialog (str, flags);
}


EmDlgItemID EmDlg::DoCommonDialog (const char* msg,
								   EmCommonDialogFlags flags)
{
	EditCommonDialogData	data;

	data.fMessage	= msg;
	data.fFlags		= flags;

	EmDlgItemID	result;
	EmDlgItemID	button = EmDlg::RunDialog (EmDlg::PrvCommonDialog, &data, kDlgCommonDialog);

	switch (button)
	{
		case kDlgItemCmnButton1:
			result = (EmDlgItemID) (GET_BUTTON (0, flags) & kButtonMask);
			break;

		case kDlgItemCmnButton2:
			result = (EmDlgItemID) (GET_BUTTON (1, flags) & kButtonMask);
			break;

		case kDlgItemCmnButton3:
			result = (EmDlgItemID) (GET_BUTTON (2, flags) & kButtonMask);
			break;

		default:
			EmAssert (false);
			result = (EmDlgItemID) 0; // prevent compiler warnings.
	}

	return result;
}


EmDlgItemID EmDlg::DoCommonDialog (const string& msg,
								   EmCommonDialogFlags flags)
{
	return EmDlg::DoCommonDialog (msg.c_str (), flags);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoSaveBound
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgFnResult EmDlg::PrvSaveBound (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			EmDlg::DisableItem (dlg, kDlgItemOK);
			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					Bool			fullSave = EmDlg::GetItemValue (dlg, kDlgItemBndSaveRAM) != 0;
					EmFileRef		destFile;
					EmFileTypeList	filterList (1, kFileTypeApplication);

#if PLATFORM_MAC
					string			newName ("Emulator_Bound");
#elif PLATFORM_UNIX
					string			newName ("pose_bound");
#elif PLATFORM_WINDOWS
					string			newName ("Emulator_Bound.exe");
#else
#error "Unsupported platform"
#endif

					if (EmDlg::DoPutFile (destFile, "Save new emulator",
						EmDirRef::GetEmulatorDirectory (), filterList, newName) == kDlgItemOK)
					{
						EmAssert (gApplication);
						gApplication->BindPoser (fullSave, destFile);
					}
					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemBndSaveROM:
				case kDlgItemBndSaveRAM:
					EmDlg::EnableItem (dlg, kDlgItemOK);
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoSaveBound (void)
{
	return EmDlg::RunDialog (EmDlg::PrvSaveBound, NULL, kDlgSaveBound);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmDlgContext::SessionInfo
 *
 * DESCRIPTION:	Called when the Session Info menu item is selected.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	nothing
 *
 ***********************************************************************/

EmDlgFnResult EmDlg::PrvSessionInfo (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;
	
	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			Preference<Configuration>	pref1 (kPrefKeyLastConfiguration);
			Preference<EmFileRef>		pref2 (kPrefKeyLastPSF);

			Configuration	cfg			= *pref1;
			EmDevice		device		= cfg.fDevice;
			string			deviceStr	= device.GetMenuString ();
			RAMSizeType		ramSize		= cfg.fRAMSize;
			EmFileRef		romFile		= cfg.fROMFile;
			string			romFileStr	= romFile.GetFullPath ();
			EmFileRef		sessionRef	= *pref2;
			string			sessionPath	= sessionRef.GetFullPath ();

			if (sessionPath.empty ())
				sessionPath = "<Not selected>";

			EmDlg::SetItemText (dlg, kDlgItemInfDeviceFld, deviceStr);
			EmDlg::SetItemValue (dlg, kDlgItemInfRAMFld, ramSize);
			EmDlg::SetItemText (dlg, kDlgItemInfROMFld, romFileStr);
			EmDlg::SetItemText (dlg, kDlgItemInfSessionFld, sessionPath);

			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				case kDlgItemCancel:
					return kDlgResultClose;

				case kDlgItemInfDeviceFld:
				case kDlgItemInfRAMFld:
				case kDlgItemInfROMFld:
				case kDlgItemInfSessionFld:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoSessionInfo (void)
{
	return EmDlg::RunDialog (EmDlg::PrvSessionInfo, NULL, kDlgSessionInfo);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	DoGetSocketAddress
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgFnResult EmDlg::PrvGetSocketAddress (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			EmDlg::SetItemText (dlg, kDlgItemSocketAddress, *(string*) context.fUserData);
			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					*(string*) context.fUserData = EmDlg::GetItemText (dlg, kDlgItemSocketAddress);
					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemSocketAddress:
					break;

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


EmDlgItemID EmDlg::DoGetSocketAddress (string& addr)
{
	return EmDlg::RunDialog (EmDlg::PrvGetSocketAddress, &addr, kDlgGetSocketAddress);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	GremlinControl
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgRef	gGremlinControl;

// Update the message "Gremlin #%gremlin_number".

void EmDlg::PrvGrmUpdateGremlinNumber (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	int32		number		= Hordes::GremlinNumber ();
	string		numberStr	= ::FormatInteger (number);

	Errors::ClearAllParameters ();
	Errors::SetParameter ("%gremlin_number", numberStr);

	string	text = Errors::ReplaceParameters (kStr_GremlinNumber);
	EmDlg::SetItemText (dlg, kDlgItemGrmNumber, text);
}


// Update the message "Event %current_event of %last_event" or
// "Event %current_event".

void EmDlg::PrvGrmUpdateEventNumber (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	// Update the "Event #: xxx of yyy" message.

	int32		counter		= Hordes::EventCounter ();
	int32		limit		= Hordes::EventLimit ();

	string		counterStr	= ::FormatInteger (counter);
	string		limitStr	= ::FormatInteger (limit);

	Errors::ClearAllParameters ();
	Errors::SetParameter ("%current_event", counterStr);
	Errors::SetParameter ("%last_event", limitStr);

	// If there is a max event number (indicated by a non -1 value
	// for Hordes::EventLimit), display a string including that
	// value.  Otherwise, the number of events to post is unlimited.
	// In that case, we just show the current event number.

	string	text = Errors::ReplaceParameters (
										limit > 0
										? kStr_GremlinXofYEvents
										: kStr_GremlinXEvents);
	EmDlg::SetItemText (dlg, kDlgItemGrmEventNumber, text);
}


// Update the message "Elapsed Time: %elapsed_time"

void EmDlg::PrvGrmUpdateElapsedTime (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	uint32		elapsed		= Hordes::ElapsedMilliseconds ();
	string		elapsedStr	= ::FormatElapsedTime (elapsed);

	Errors::ClearAllParameters ();
	Errors::SetParameter ("%elapsed_time", elapsedStr);

	string	text = Errors::ReplaceParameters (kStr_GremlinElapsedTime);
	EmDlg::SetItemText (dlg, kDlgItemGrmElapsedTime, text);
}


void EmDlg::PrvGrmUpdateAll (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	EmDlg::PrvGrmUpdateGremlinNumber (context);
	EmDlg::PrvGrmUpdateEventNumber (context);
	EmDlg::PrvGrmUpdateElapsedTime (context);

	// Update the buttons.

	EmDlg::EnableDisableItem (dlg, kDlgItemGrmStop, Hordes::CanStop ());
	EmDlg::EnableDisableItem (dlg, kDlgItemGrmResume, Hordes::CanResume ());
	EmDlg::EnableDisableItem (dlg, kDlgItemGrmStep, Hordes::CanStep ());
}


EmDlgFnResult EmDlg::PrvGremlinControl (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			Preference<PointType>	pref (kPrefKeyGCWLocation);

			if (pref.Loaded ())
			{
				EmDlg::MoveDlgTo (dlg, pref->x, pref->y);
				EmDlg::EnsureDlgOnscreen (dlg);
			}
			else
			{
				EmDlg::CenterDlg (dlg);
			}

			// Set a timer so that we can periodically update the displayed values.

			context.fNeedsIdle = true;

			// Set the initial contents of the dialog.

			EmDlg::PrvGrmUpdateAll (context);

			break;
		}

		case kDlgCmdIdle:
		{
			static UInt32	lastIdle = 0;

			UInt32			curIdle = Platform::GetMilliseconds ();

			if (curIdle - lastIdle > 500)
			{
				lastIdle = curIdle;

				EmDlg::PrvGrmUpdateAll (context);
			}

			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				case kDlgItemGrmStop:
				{
					EmSessionStopper	stopper (gSession, kStopNow);
					if (stopper.Stopped())
					{
						Hordes::Stop ();
					}
					break;
				}

				case kDlgItemGrmResume:
				{
					// Resuming attempts to make Palm OS calls (for instance,
					// to reset the pen calibration), so make sure we're stopped
					// at a good place.

					EmSessionStopper	stopper (gSession, kStopOnSysCall);
					if (stopper.Stopped())
					{
						Hordes::Resume ();
					}
					break;
				}

				case kDlgItemGrmStep:
				{
					EmSessionStopper	stopper (gSession, kStopOnSysCall);
					if (stopper.Stopped())
					{
						Hordes::Step ();
					}
					break;
				}

				case kDlgItemGrmNumber:
				case kDlgItemGrmEventNumber:
				case kDlgItemGrmElapsedTime:

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
		{
			EmRect	bounds = EmDlg::GetDlgBounds (dlg);

			Preference<EmPoint>	pref (kPrefKeyGCWLocation);
			pref = bounds.TopLeft ();

			gGremlinControl = NULL;

			break;	// Return value is ignored for destroy
		}

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


void EmDlg::GremlinControlOpen (void)
{
	if (!gGremlinControl)
	{
		gGremlinControl = EmDlg::DialogOpen (EmDlg::PrvGremlinControl, NULL, kDlgGremlinControl);
	}
	else
	{
		// !!! Bring to front
	}
}


void EmDlg::GremlinControlClose (void)
{
	if (gGremlinControl)
	{
		EmDlg::DialogClose (gGremlinControl);
	}
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	MinimizeControl
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgRef	gMinimizeProgress;


// Update the message "Pass #%pass_number".

void EmDlg::PrvMinUpdatePassNumber (EmDlgContext& context)
{
	EmDlgRef	dlg			= context.fDlg;

	uint32		pass		= EmMinimize::GetPassNumber ();
	string		passStr		= ::FormatInteger (pass);

	if (pass == 0)
	{
		passStr = "LAST";
	}

	Errors::ClearAllParameters ();
	Errors::SetParameter ("%pass_number", passStr);

	string	text = Errors::ReplaceParameters (kStr_MinimizePassNumber);
	EmDlg::SetItemText (dlg, kDlgItemMinPassNumber, text);
}


// Update the message "Event %current_event of %last_event".
//
// "last_event" is the last event number of the current pass.
// "current_event" is between one and that number, inclusive.

void EmDlg::PrvMinUpdateEventNumber (EmDlgContext& context)
{
	EmDlgRef	dlg			= context.fDlg;

	uint32		current		= EmEventPlayback::GetCurrentEvent ();
	string		currentStr	= ::FormatInteger (current);

	uint32		total		= EmEventPlayback::GetNumEvents ();
	string		totalStr	= ::FormatInteger (total);

	Errors::ClearAllParameters ();
	Errors::SetParameter ("%current_event", currentStr);
	Errors::SetParameter ("%last_event", totalStr);

	string	text = Errors::ReplaceParameters (kStr_MinimizeXofYEvents);
	EmDlg::SetItemText (dlg, kDlgItemMinEventNumber, text);
}


// Update the message "Elapsed Time: %elapsed_time".

void EmDlg::PrvMinUpdateElapsed (EmDlgContext& context)
{
	EmDlgRef	dlg			= context.fDlg;

	uint32		elapsed		= EmMinimize::GetElapsedTime ();
	string		elapsedStr	= ::FormatElapsedTime (elapsed);

	Errors::ClearAllParameters ();
	Errors::SetParameter ("%elapsed_time", elapsedStr);

	string	text = Errors::ReplaceParameters (kStr_MinimizeElapsedTime);
	EmDlg::SetItemText (dlg, kDlgItemMinElapsed, text);
}


// Update the message "Excluding range %first_event to %last_event".
//
// When formatting the last event number, subtract one to hide the
// fact that we work on a half-open interval.  Then add one to both
// the begin and end to convert a 0-based range to a 1-based range
// (so that the first event is reported as 1 instead of zero).

void EmDlg::PrvMinUpdateRange (EmDlgContext& context)
{
	EmDlgRef	dlg			= context.fDlg;

	uint32		begin, end;
	EmMinimize::GetCurrentRange (begin, end);

	string		beginStr	= ::FormatInteger (begin + 1);
	string		endStr		= ::FormatInteger (end);

	Errors::ClearAllParameters ();
	Errors::SetParameter ("%first_event", beginStr);
	Errors::SetParameter ("%last_event", endStr);

	string	text = Errors::ReplaceParameters (kStr_MinimizeRange);
	EmDlg::SetItemText (dlg, kDlgItemMinRange, text);
}


// Update the message "Discarded %num_discarded_events of %num_total_events events".
//
// "num_total_events" is the initial number of events at pass 1.
// "num_discarded_events" is between zero and that number.

void EmDlg::PrvMinUpdateDiscarded (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	uint32		discarded		= EmMinimize::GetNumDiscardedEvents ();
	string		discardedStr	= ::FormatInteger (discarded);

	uint32		total			= EmMinimize::GetNumInitialEvents ();
	string		totalStr		= ::FormatInteger (total);

	Errors::ClearAllParameters ();
	Errors::SetParameter ("%num_discarded_events", discardedStr);
	Errors::SetParameter ("%num_total_events", totalStr);

	string	text = Errors::ReplaceParameters (kStr_MinimizeDiscarded);
	EmDlg::SetItemText (dlg, kDlgItemMinDiscarded, text);
}


// Update the progress bar.
//
// Max value of the progress bar is the current maximum event for this pass.
// Current value of the bar is the lower end of the range we are currently
// examining for exclusion.  Given the way we walk through the ranges of
// events to exclude, this will give a jerky yet increasing progress bar.

void EmDlg::PrvMinUpdateProgress (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	uint32		begin, end;
	EmMinimize::GetCurrentRange (begin, end);

	uint32		total = EmEventPlayback::GetNumEvents ();

	// Make sure "total" is less than 32K (that's the most that the standard
	// Win32 progress bar can handle).

	int	divider = (total / 32768) + 1;

	total /= divider;
	begin /= divider;
	end /= divider;

	EmDlg::SetItemMax (dlg, kDlgItemMinProgress, total);
	EmDlg::SetItemValue (dlg, kDlgItemMinProgress, begin);
}


// Update all elements of the dialog box.

void EmDlg::PrvMinUpdateAll (EmDlgContext& context)
{
	EmDlg::PrvMinUpdatePassNumber (context);
	EmDlg::PrvMinUpdateEventNumber (context);
	EmDlg::PrvMinUpdateElapsed (context);
	EmDlg::PrvMinUpdateRange (context);
	EmDlg::PrvMinUpdateDiscarded (context);
	EmDlg::PrvMinUpdateProgress (context);
}


EmDlgFnResult EmDlg::PrvMinimizeProgress (EmDlgContext& context)
{
	EmDlgRef	dlg = context.fDlg;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			Preference<PointType>	pref (kPrefKeyMPWLocation);

			if (pref.Loaded ())
			{
				EmDlg::MoveDlgTo (dlg, pref->x, pref->y);
				EmDlg::EnsureDlgOnscreen (dlg);
			}
			else
			{
				EmDlg::CenterDlg (dlg);
			}

			// Set a timer so that we can periodically update the displayed values.

			context.fNeedsIdle = true;

			// Set the initial contents of the dialog.

			EmDlg::PrvMinUpdateAll (context);

			break;
		}

		case kDlgCmdIdle:
		{
			if (!EmMinimize::IsOn ())
			{
				return kDlgResultClose;
			}

			static UInt32	lastIdle = 0;

			UInt32			curIdle = Platform::GetMilliseconds ();

			if (curIdle - lastIdle > 500)
			{
				lastIdle = curIdle;

				EmDlg::PrvMinUpdateAll (context);
			}

			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemCancel:
				{
					EmMinimize::Stop ();
					return kDlgResultClose;
				}

				case kDlgItemMinRange:
				case kDlgItemMinElapsed:
				case kDlgItemMinDiscarded:
				case kDlgItemMinProgress:

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
		{
			EmRect	bounds = EmDlg::GetDlgBounds (dlg);

			Preference<EmPoint>	pref (kPrefKeyMPWLocation);
			pref = bounds.TopLeft ();

			gMinimizeProgress = NULL;

			break;	// Return value is ignored for destroy
		}

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}


void EmDlg::MinimizeProgressOpen (void)
{
	if (!gMinimizeProgress)
	{
		gMinimizeProgress = EmDlg::DialogOpen (EmDlg::PrvMinimizeProgress, NULL, kDlgMinimizeProgress);
	}
	else
	{
		// !!! Bring to front
	}
}


void EmDlg::MinimizeProgressClose (void)
{
	if (gMinimizeProgress)
	{
		EmDlg::DialogClose (gMinimizeProgress);
	}
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	RunDialog
 *
 * DESCRIPTION:	Common routine that all dialog clients go through.
 *				This function checks to see if we are in the UI thread
 *				or not.  If not, it forwards the call to the session
 *				so that it can block and wait for the UI thread to
 *				pick up and handle the call.  If we are already in the
 *				UI thread, then call the Host routine that displays
 *				and drives the dialog.
 *
 * PARAMETERS:	fn - the custom dialog handler
 *				userData - custom data passed back to the dialog handler.
 *				dlgID - ID of dialog to create.
 *
 * RETURNED:	ID of dialog item that dismissed the dialog.
 *
 ***********************************************************************/
 
EmDlgItemID EmDlg::RunDialog (EmDlgFn fn, void* userData, EmDlgID dlgID)
{
	RunDialogParameters	parameters (fn, userData, dlgID);
	return EmDlg::RunDialog (EmDlg::HostRunDialog, &parameters);
}


/***********************************************************************
 *
 * FUNCTION:	RunDialog
 *
 * DESCRIPTION:	Common routine that all dialog clients go through.
 *				This function checks to see if we are in the UI thread
 *				or not.  If not, it forwards the call to the session
 *				so that it can block and wait for the UI thread to
 *				pick up and handle the call.  If we are already in the
 *				UI thread, then call the Host routine that displays
 *				and drives the dialog.
 *
 * PARAMETERS:	fn - the custom dialog handler
 *				userData - custom data passed back to the dialog handler.
 *				dlgID - ID of dialog to create.
 *
 * RETURNED:	ID of dialog item that dismissed the dialog.
 *
 ***********************************************************************/
 
EmDlgItemID EmDlg::RunDialog (EmDlgThreadFn fn, const void* parameters)
{
#if HAS_OMNI_THREAD
	if (gSession && gSession->InCPUThread ())
	{
		return gSession->BlockOnDialog (fn, parameters);
	}
#else
	if (gSession && gSession->GetSessionState () == kRunning)
	{
		return gSession->BlockOnDialog (fn, parameters);
	}
#endif

	return fn (parameters);
}


/***********************************************************************
 *
 * FUNCTION:	DialogOpen
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	fn - the custom dialog handler
 *				userData - custom data passed back to the dialog handler.
 *				dlgID - ID of dialog to create.
 *
 * RETURNED:	.
 *
 ***********************************************************************/
 
EmDlgRef EmDlg::DialogOpen (EmDlgFn fn, void* data, EmDlgID dlgID)
{
	return EmDlg::HostDialogOpen (fn, data, dlgID);
}


/***********************************************************************
 *
 * FUNCTION:	DialogClose
 *
 * DESCRIPTION:	.
 *
 * PARAMETERS:	.
 *
 * RETURNED:	.
 *
 ***********************************************************************/
 
void EmDlg::DialogClose (EmDlgRef dlg)
{
	EmDlg::HostDialogClose (dlg);
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmDlg::MoveDlgTo
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::MoveDlgTo (EmDlgRef dlg, EmCoord x, EmCoord y)
{
	EmDlg::MoveDlgTo (dlg, EmPoint (x, y));
}


void EmDlg::MoveDlgTo (EmDlgRef dlg, const EmPoint& pt)
{
	EmRect	bounds = EmDlg::GetDlgBounds (dlg);
	bounds += (pt - bounds.TopLeft ());
	EmDlg::SetDlgBounds (dlg, bounds);
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::EnsureDlgOnscreen
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::EnsureDlgOnscreen (EmDlgRef dlg)
{
	EmRect	bounds = EmDlg::GetDlgBounds (dlg);

	if (Platform::PinToScreen (bounds))
	{
		EmDlg::SetDlgBounds (dlg, bounds);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SetItemText
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SetItemText (EmDlgRef dlg, EmDlgItemID item, StrCode strCode)
{
	string	str = Platform::GetString (strCode);
	EmDlg::SetItemText (dlg, item, str);
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SetItemText
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SetItemText (EmDlgRef dlg, EmDlgItemID item, const char* str)
{
	EmDlg::SetItemText (dlg, item, string (str));
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::EnableDisableItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::EnableDisableItem (EmDlgRef dlg, EmDlgItemID item, Bool enable)
{
	if (enable)
		EmDlg::EnableItem (dlg, item);
	else
		EmDlg::DisableItem (dlg, item);
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::ShowHideItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::ShowHideItem (EmDlgRef dlg, EmDlgItemID item, Bool show)
{
	if (show)
		EmDlg::ShowItem (dlg, item);
	else
		EmDlg::HideItem (dlg, item);
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::AppendToMenu
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::AppendToMenu (EmDlgRef dlg, EmDlgItemID item, const string& text)
{
	StringList	strList;
	strList.push_back (text);

	EmDlg::AppendToMenu (dlg, item, strList);
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::AppendToList
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::AppendToList (EmDlgRef dlg, EmDlgItemID item, const string& text)
{
	StringList	strList;
	strList.push_back (text);

	EmDlg::AppendToList (dlg, item, strList);
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::SelectListItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::SelectListItem (EmDlgRef dlg, EmDlgItemID item, EmDlgListIndex listItem)
{
	EmDlgListIndexList	itemList;
	itemList.push_back (listItem);

	EmDlg::SelectListItems (dlg, item, itemList);
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::UnselectListItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

void EmDlg::UnselectListItem (EmDlgRef dlg, EmDlgItemID item, EmDlgListIndex listItem)
{
	EmDlgListIndexList	itemList;
	itemList.push_back (listItem);

	EmDlg::UnselectListItems (dlg, item, itemList);
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::GetSelectedItem
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgListIndex EmDlg::GetSelectedItem (EmDlgRef dlg, EmDlgItemID item)
{
	EmDlgListIndexList	itemList;
	EmDlg::GetSelectedItems (dlg, item, itemList);

	if (itemList.size () > 0)
	{
		return itemList[0];
	}

	return kDlgItemListNone;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlg::StringToLong
 *
 * DESCRIPTION:	
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

Bool EmDlg::StringToLong (const char* s, long* num)
{
	if (sscanf(s, "%li", num) == 1)
		return true;

	return false;
}


#pragma mark -

/***********************************************************************
 *
 * FUNCTION:	EmDlgContext c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgContext::EmDlgContext (void) :
	fFn (NULL),
	fFnResult (kDlgResultNone),
	fDlg (NULL),
	fDlgID (kDlgNone),
	fPanelID (kDlgPanelNone),
	fItemID (kDlgItemNone),
	fCommandID (kDlgCmdNone),
	fNeedsIdle (false),
	fUserData (NULL),
	fDefaultItem (kDlgItemNone),
	fCancelItem (kDlgItemNone)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmDlgContext c'tor
 *
 * DESCRIPTION:	Constructor.  Initialize our data members.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	Nothing
 *
 ***********************************************************************/

EmDlgContext::EmDlgContext (const EmDlgContext& other) :
	fFn (other.fFn),
	fFnResult (other.fFnResult),
	fDlg (other.fDlg),
	fDlgID (other.fDlgID),
	fPanelID (other.fPanelID),
	fItemID (other.fItemID),
	fCommandID (other.fCommandID),
	fNeedsIdle (other.fNeedsIdle),
	fUserData (other.fUserData),
	fDefaultItem (other.fDefaultItem),
	fCancelItem (other.fCancelItem)
{
}


/***********************************************************************
 *
 * FUNCTION:	EmDlgContext::Init
 *
 * DESCRIPTION:	Call the custom dialog handler to initialize the dialog.
 *				If the dialog says that it wants idle time, then start
 *				idleing it.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The result returned by the dialog handler.
 *
 ***********************************************************************/

EmDlgFnResult EmDlgContext::Init (void)
{
	fFnResult	= kDlgResultNone;

	if (fFn)
	{
		fItemID		= kDlgItemNone;
		fCommandID	= kDlgCmdInit;

		fFnResult	= fFn (*this);

		if (fNeedsIdle)
			EmDlg::HostStartIdling (*this);
	}

	return fFnResult;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlgContext::Idle
 *
 * DESCRIPTION:	Call the custom dialog handler to idle.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The result returned by the dialog handler.
 *
 ***********************************************************************/

EmDlgFnResult EmDlgContext::Idle (void)
{
	fFnResult	= kDlgResultNone;

	if (fFn)
	{
		fItemID		= kDlgItemNone;
		fCommandID	= kDlgCmdIdle;

		fFnResult	= fFn (*this);
	}

	return fFnResult;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlgContext::Event
 *
 * DESCRIPTION:	Call the custom dialog handler to handle an event.
 *
 * PARAMETERS:	item - dialog item that was selected.
 *
 * RETURNED:	The result returned by the dialog handler.
 *
 ***********************************************************************/

EmDlgFnResult EmDlgContext::Event (EmDlgItemID itemID)
{
	fFnResult	= kDlgResultNone;

	if (fFn)
	{
		fItemID		= itemID;
		fCommandID	= kDlgCmdItemSelected;

		fFnResult	= fFn (*this);
	}

	return fFnResult;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlgContext::Destroy
 *
 * DESCRIPTION:	Call the custom dialog handler to tell it that the
 *				dialog is being destroyed.
 *
 * PARAMETERS:	None.
 *
 * RETURNED:	Nothing.
 *
 ***********************************************************************/

void EmDlgContext::Destroy (void)
{
	if (fFn)
	{
		fItemID		= kDlgItemNone;
		fCommandID	= kDlgCmdDestroy;

		fFnResult	= fFn (*this);
	}
}


/***********************************************************************
 *
 * FUNCTION:	EmDlgContext::PanelEnter
 *
 * DESCRIPTION:	Call the custom dialog handler to idle.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The result returned by the dialog handler.
 *
 ***********************************************************************/

EmDlgFnResult EmDlgContext::PanelEnter (void)
{	
	fItemID		= kDlgItemNone;
	fCommandID	= kDlgCmdPanelEnter;

	fFnResult	= fFn (*this);
	return fFnResult;
}


/***********************************************************************
 *
 * FUNCTION:	EmDlgContext::PanelExit
 *
 * DESCRIPTION:	Call the custom dialog handler to idle.
 *
 * PARAMETERS:	None
 *
 * RETURNED:	The result returned by the dialog handler.
 *
 ***********************************************************************/

EmDlgFnResult EmDlgContext::PanelExit (void)
{	
	fItemID		= kDlgItemNone;
	fCommandID	= kDlgCmdPanelExit;

	fFnResult	= fFn (*this);
	return fFnResult;
}


#pragma mark -

#if !PLATFORM_UNIX
// Append one set of descriptors to another set, separating them if
// necessary with an empty descriptor.

static void PrvAppendDescriptors (EmTransportDescriptorList& menuItems,
								  const EmTransportDescriptorList& rawItems)
{
	// If there are items to be added and items already added,
	// insert a divider first.

	if (menuItems.size () > 0 && rawItems.size () > 0)
	{
		menuItems.push_back (EmTransportDescriptor ());
	}

	// Add the menu items to the menu.

	std::copy (rawItems.begin (), rawItems.end (), back_inserter (menuItems));
}


// Append one descriptors to a set, separating it if
// necessary with an empty descriptor.

static void PrvAppendDescriptors (EmTransportDescriptorList& menuItems,
								  const string& rawItem)
{
	EmTransportDescriptorList	rawItems;
	rawItems.push_back (EmTransportDescriptor (rawItem));

	::PrvAppendDescriptors (rawItems, menuItems);
}


// Return the text that should be put into the menu item.

static string PrvGetMenuItemText (EmDlgItemID whichMenu,
								  const EmTransportDescriptor& item,
								  const string& socketAddr)
{
	// Get the text to add to the menu.

	string	itemText = item.GetMenuName ();

	// If this menu item is for a socket, see if we'd like to
	// augment the text for it with socket/port info.

	if (item.GetType () == kTransportSocket)
	{
		if (whichMenu == kDlgItemPrfRedirectSerial || whichMenu == kDlgItemPrfRedirectSerial)
		{
			// If the user has selected a socket for their serial or IR
			// port, then add the socket address/port, as well as a "..."
			// so that they can bring up a dialog allowing them to specify
			// that information.

			if (socketAddr.size () > 0)
			{
				itemText += " (" + socketAddr + ")";
			}

			itemText += "...";
		}
	}

	return itemText;
}


// Append the given descriptors to the indicated menu.  Empty descriptors
// are turned into divider menu items.  If the descriptor in "pref" is
// found, then select that descriptor in the menu.  Otherwise, select the
// first item in the menu.

static void PrvAppendMenuItems (EmDlgRef dlg, EmDlgItemID dlgItem,
								const EmTransportDescriptorList& menuItems,
								const EmTransportDescriptor& pref,
								const string& socketAddr)
{
	Bool	selected = false;

	// Iterate over all the items in "menuItems".

	EmTransportDescriptorList::const_iterator	iter = menuItems.begin ();
	while (iter != menuItems.end ())
	{
		// If this is descriptor is of a known type, add it to the menu as an item.

		if (iter->GetType () != kTransportUnknown)
		{
			// Add the menu item to the menu.

			string	itemText = ::PrvGetMenuItemText (dlgItem, *iter, socketAddr);
			EmDlg::AppendToMenu (dlg, dlgItem, itemText);

			// If this is the item the user has selected, then select it
			// in the menu.

			if (iter->GetType () == kTransportSocket)
			{
				if (pref.GetType () == kTransportSocket)
				{
					EmDlg::SetItemValue (dlg, dlgItem, iter - menuItems.begin ());
					selected = true;
				}
			}
			else if (*iter == pref)
			{
				EmDlg::SetItemValue (dlg, dlgItem, iter - menuItems.begin ());
				selected = true;
			}
		}

		// If this descriptor is not of a known type, add it as a menu divider.

		else
		{
			EmDlg::AppendToMenu (dlg, dlgItem, "");
		}

		++iter;
	}

	// If we couldn't find a menu item to select,
	// pick a default.

	if (!selected)
	{
		EmDlg::SetItemValue (dlg, dlgItem, 0);
	}
}
#endif	// !PLATFORM_UNIX


#pragma mark -

#if 0
	// Template for callback function

EmDlgFnResult EmDlg::PrvCallback (EmDlgContext& context)
{
	EmDlgRef				dlg = context.fDlg;
	<type>&	data = *(<type>*) context.fUserData;

	switch (context.fCommandID)
	{
		case kDlgCmdInit:
		{
			break;
		}

		case kDlgCmdIdle:
		{
			break;
		}

		case kDlgCmdItemSelected:
		{
			switch (context.fItemID)
			{
				case kDlgItemOK:
				{
					// Fall thru...
				}

				case kDlgItemCancel:
				{
					return kDlgResultClose;
				}

				default:
					EmAssert (false);
			}

			break;
		}

		case kDlgCmdDestroy:
			break;

		case kDlgCmdPanelEnter:
		case kDlgCmdPanelExit:
		case kDlgCmdNone:
			EmAssert (false);
			break;
	}

	return kDlgResultContinue;
}
#endif
