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

#ifndef _PREFERENCEMGR_H_
#define _PREFERENCEMGR_H_

#include "EmFileRef.h"		 	// EmFileRefList, EmFileRef
#include "EmTransport.h"		// EmTransportType
#include "EmHAL.h"				// EmUARTDeviceType
#include "Skins.h"				// SkinNameList

#include "omnithread.h" 		// omni_mutex

#include <stdio.h>				// FILE
#include <map>
#include <vector>

class EmTransport;

/*
	This file contains the routines for loading, saving, and accessing a collection
	of preferences/settings.

	All settings are accessed via the Preference class.  This is a templatized class
	that is specialized on the type of the setting and parameterized on the name of
	the setting.  This sounds nasty, but in practice is simple. For example, say you
	needed to access whether or not NetLib redirection was turned on.  You would use
	the following:

		Preference<bool>	pref(kPrefKeyRedirectNetLib);
		
		if (*pref)
		{
			// It's on.
		}

	Changing a value works similarly:

		Preference<DeviceType>	pref(kPrefKeyDeviceType);
		*pref = kDevicePalmIII;

	When the Preference object is destructed, it's new value is written back out to
	the Preference Manager.

	The Preference Manager is the "container" for all the preferences used by the
	application.  You can think of it as a map/dictionary/associative array.  You
	directly access the Preference Manager in order to create it, have it load the
	collection of preferences from disk, have it save them to disk, and to destroy
	it.  While there are GetPref and SetPref methods on this class, those are for
	the use of the Preference objects.

	All preferences are saved as text in the form:

		<key>=<value>

	<key> is any unique string.  It can consist of any text characters except for
	'='.  Keys can have subparts separated by '.', and can indicate array indices
	with "[#]".  For example:

		Scale=2
		GremlinInfo.fNumber=100
		PRC_MRU[0]=MyApplication.prc
		GremlinInfo.fAppList[0].name=AddressBook

	With the Preference class, it's possible to pull in all or part of the
	hierarchical information.  For instance:

		Preference<GremlinInfo>			pref("GremlinInfo");
		Preference<DatabaseInfoList>	pref("GremlinInfo.fAppList");
		Preference<DatabaseInfo>		pref("GremlinInfo.fAppList[0]");
		Preference<string>				pref("GremlinInfo.fAppList[0].name");
*/

typedef const char* PrefKeyType;

bool PrefKeysEqual (PrefKeyType, PrefKeyType);

class BasePreference
{
	public:
								BasePreference		(PrefKeyType name, bool = true);
								BasePreference		(long index, bool = true);
		virtual 				~BasePreference 	(void);

		bool					Loaded				(void)	{ return fLoaded; }

		void					Flush				(void)	{ this->Save (); }

	protected:
		void					Load				(void);
		void					Save				(void);

	protected:
		virtual bool			DoLoad				(void) = 0;
		virtual void			DoSave				(void) = 0;

	protected:
		string					fName;
		bool					fLoaded;
		bool					fChanged;
		bool					fAcquireLock;
};

template <class T>
class Preference : public BasePreference
{
	public:
								Preference			(PrefKeyType name, bool = true);
								Preference			(long index, bool = true);
		virtual 				~Preference 		(void);

	// I would *like* to have these operators.	That way, I could pass in a
	// "Preference<Foo>" any place that accepts a "const Foo&" as a parameter.
	// However, CodeWarrior seems to use the conversion operators here in place
	// that accept merely a "Foo&", which has lead to some problems.
//								operator const T&() const	{ return *GetValue (); }
//								operator const T*() const	{ return GetValue (); }
		const T&				operator*() const			{ return *GetValue (); }
		const T*				operator->() const			{ return GetValue (); }

		const T*				GetValue () const			{ EmAssert (fLoaded); return &fValue; }

		const T&				operator=(const T& rhs) 	{ fValue = rhs;
															  fLoaded = true;
															  fChanged = true;
															  this->Flush ();
															  return fValue; }

	protected:
		virtual bool			DoLoad				(void);
		virtual void			DoSave				(void);

	private:
		T						fValue;
};

typedef void*		PrefRefCon;
typedef void		(*PrefNotifyFunc)(PrefKeyType, PrefRefCon);
typedef StringList	PrefKeyList;

enum
{
	MRU_COUNT = 9
};

class Preferences
{
	public:
								Preferences 		(void);
		virtual 				~Preferences		(void);

		virtual void			Load				(void);
		virtual void			Save				(void);

// This should work, but CW doesn't appear to like it.
//	private:
//		template <class T>	friend class Preference;
	public:
		bool					GetPref 			(const string& key, string& value);
		void					SetPref 			(const string& key, const string& value);
		void					DeletePref			(const string& key);

	public:
		void					PushPrefix			(const string& prefix);
		void					PushPrefix			(long index);
		void					PopPrefix			(void);

		string					ExpandKey			(const string& name);
		string					AppendName			(string key, const string& name);

	public:
		void					AddNotification		(PrefNotifyFunc, PrefKeyType, PrefRefCon = NULL);
		void					AddNotification		(PrefNotifyFunc, const PrefKeyList&, PrefRefCon = NULL);
		void					RemoveNotification	(PrefNotifyFunc);
		void					RemoveNotification	(PrefNotifyFunc, PrefKeyType);
		void					RemoveNotification	(PrefNotifyFunc, const PrefKeyList&);
		void					DoNotify			(const string& key);

	protected:
		virtual Bool			ReadPreferences		(StringStringMap&);
		virtual void			WritePreferences	(const StringStringMap&);
		virtual EmFileRef		GetPrefRef			(void);
		virtual void			WriteBanner 		(FILE*);
		virtual Bool			ReadBanner			(FILE*);
		virtual void			StripUnused 		(void);

	protected:
		typedef StringStringMap			PrefList;
		typedef PrefList::value_type	PrefPairType;
		typedef PrefList::iterator		iterator;

		PrefList				fPreferences;

		typedef StringList		PrefixType;
		PrefixType				fPrefixes;

		struct PrefNotifyType
		{
			PrefNotifyFunc		fFunc;
			PrefKeyList			fKeyList;
			PrefRefCon			fRefCon;
		};
		typedef vector<PrefNotifyType>	PrefNotifyList;

		PrefNotifyList			fNotifications;

	public:
		static omni_mutex		fgPrefsMutex;
};

extern Preferences* gPrefs;


class EmulatorPreferences : public Preferences
{
	public:
								EmulatorPreferences (void);
		virtual 				~EmulatorPreferences(void);

		virtual void			Load				(void);

		void					GetDatabaseMRU		(EmFileRefList&);
		void					GetSessionMRU		(EmFileRefList&);
		void					GetROMMRU			(EmFileRefList&);

		EmFileRef				GetIndPRCMRU		(int);
		EmFileRef				GetIndRAMMRU		(int);
		EmFileRef				GetIndROMMRU		(int);
		EmFileRef				GetIndMRU			(const EmFileRefList&, int);

		void					UpdatePRCMRU		(const EmFileRef&);
		void					UpdateRAMMRU		(const EmFileRef&);
		void					UpdateROMMRU		(const EmFileRef&);
		void					UpdateMRU			(EmFileRefList&, const EmFileRef&);

		void					RemovePRCMRU		(const EmFileRef&);
		void					RemoveRAMMRU		(const EmFileRef&);
		void					RemoveROMMRU		(const EmFileRef&);
		void					RemoveMRU			(EmFileRefList&, const EmFileRef&);

		void					SetTransports		(void);

		void					SetTransportForDevice	(EmUARTDeviceType, EmTransport*);
		EmTransport*			GetTransportForDevice	(EmUARTDeviceType);

		// Utility routines for determining what should happen in DoDialog.

		Bool					LogMessage			(Bool isFatal);
		Bool					ShouldQuit			(Bool isFatal);
		Bool					ShouldContinue		(Bool isFatal);
		Bool					ShouldNextGremlin	(Bool isFatal);

	protected:
		virtual void			WriteBanner 		(FILE*);
		virtual Bool			ReadBanner			(FILE*);
		virtual void			StripUnused 		(void);
		void					MigrateOldPrefs		(void);

		EmTransport*			fTransports[kUARTEnd];
};

extern EmulatorPreferences* gEmuPrefs;


/*
	The FOR_EACH_PREF macro is used to manage the preferences keys.  Preferences
	are accessed via keys passed to the Preference constructor.  These keys have
	the symbolic form:

		kPrefKey<Label>

	The macro below is used to declare the keys used to access the settings, define
	the keys, initialize the settings to default values.
*/

#define FOR_EACH_PREF(DO_TO_PREF)								\
	FOR_EACH_UNINIT_PREF(DO_TO_PREF)							\
	FOR_EACH_INIT_PREF(DO_TO_PREF)

#define FOR_EACH_UNINIT_PREF(DO_TO_PREF)						\
	DO_TO_PREF(WindowLocation,	unused, unused)					\
	DO_TO_PREF(GCWLocation,		unused, unused)					\
	DO_TO_PREF(MPWLocation,		unused, unused)					\
	DO_TO_PREF(BackgroundColor,	unused, unused) 				\
	DO_TO_PREF(HighlightColor,	unused, unused)


#define FOR_EACH_INIT_PREF(DO_TO_PREF)											\
	DO_TO_PREF(RedirectNetLib,		bool,				(true))					\
	DO_TO_PREF(EnableSounds,		bool,				(true))				\
	DO_TO_PREF(CloseAction,			CloseActionType,	(kSaveNever))				\
	DO_TO_PREF(UserName,			string,				("PHEM"))	\
																				\
	DO_TO_PREF(PortSerial,			EmTransportDescriptor,	(kTransportNull))	\
	DO_TO_PREF(PortIR,				EmTransportDescriptor,	(kTransportNull))	\
	DO_TO_PREF(PortMystery,			EmTransportDescriptor,	(kTransportNull))	\
	DO_TO_PREF(PortDownload,		EmTransportDescriptor,	(kTransportNull))	\
																				\
	DO_TO_PREF(PortSerialSocket,	string,				(""))					\
	DO_TO_PREF(PortIRSocket,		string,				(""))					\
																				\
	DO_TO_PREF(ReportFreeChunkAccess,			bool,	(false))					\
	DO_TO_PREF(ReportHardwareRegisterAccess,	bool,	(false))					\
	DO_TO_PREF(ReportLowMemoryAccess,			bool,	(false))					\
	DO_TO_PREF(ReportLowStackAccess,			bool,	(false))					\
	DO_TO_PREF(ReportMemMgrDataAccess,			bool,	(false))					\
	DO_TO_PREF(ReportMemMgrLeaks,				bool,	(false))					\
	DO_TO_PREF(ReportMemMgrSemaphore,			bool,	(false))					\
	DO_TO_PREF(ReportOffscreenObject,			bool,	(false))					\
	DO_TO_PREF(ReportOverlayErrors,				bool,	(false))					\
	DO_TO_PREF(ReportProscribedFunction,		bool,	(false))					\
	DO_TO_PREF(ReportROMAccess,					bool,	(false))					\
	DO_TO_PREF(ReportScreenAccess,				bool,	(false))					\
	DO_TO_PREF(ReportSizelessObject,			bool,	(false))					\
	DO_TO_PREF(ReportStackAlmostOverflow,		bool,	(false))					\
	DO_TO_PREF(ReportStrictIntlChecks,			bool,	(false))					\
	DO_TO_PREF(ReportSystemGlobalAccess,		bool,	(false))					\
	DO_TO_PREF(ReportUIMgrDataAccess,			bool,	(false))					\
	DO_TO_PREF(ReportUnlockedChunkAccess,		bool,	(false))					\
																				\
	DO_TO_PREF(ReportLockedRecords,				bool,	(false))				\
	DO_TO_PREF(ReportSysFatalAlert,				bool,	(false))					\
																				\
	DO_TO_PREF(InterceptSysFatalAlert,			bool,	(true))					\
	DO_TO_PREF(DialogBeep,						bool,	(false))				\
																				\
	DO_TO_PREF(LogErrorMessages,	uint8,				(2))					\
	DO_TO_PREF(LogWarningMessages,	uint8,				(2))					\
	DO_TO_PREF(LogGremlins,			uint8,				(0))					\
	DO_TO_PREF(LogCPUOpcodes,		uint8,				(0))					\
	DO_TO_PREF(LogEnqueuedEvents,	uint8,				(0))					\
	DO_TO_PREF(LogDequeuedEvents,	uint8,				(0))					\
	DO_TO_PREF(LogSystemCalls,		uint8,				(0))					\
	DO_TO_PREF(LogApplicationCalls,	uint8,				(0))					\
	DO_TO_PREF(LogSerial,			uint8,				(0))					\
	DO_TO_PREF(LogSerialData,		uint8,				(0))					\
	DO_TO_PREF(LogNetLib,			uint8,				(0))					\
	DO_TO_PREF(LogNetLibData,		uint8,				(0))					\
	DO_TO_PREF(LogExgMgr,			uint8,				(0))					\
	DO_TO_PREF(LogExgMgrData,		uint8,				(0))					\
	DO_TO_PREF(LogHLDebugger,		uint8,				(0))					\
	DO_TO_PREF(LogHLDebuggerData,	uint8,				(0))					\
	DO_TO_PREF(LogLLDebugger,		uint8,				(0))					\
	DO_TO_PREF(LogLLDebuggerData,	uint8,				(0))					\
	DO_TO_PREF(LogRPC,				uint8,				(0))					\
	DO_TO_PREF(LogRPCData,			uint8,				(0))					\
																				\
	DO_TO_PREF(LogFileSize,			long,				(1 * 1024L * 1024L))	\
	DO_TO_PREF(LogDefaultDir,		EmDirRef,			())						\
																				\
	DO_TO_PREF(DebuggerSocketPort,	long,				(6414))					\
	DO_TO_PREF(RPCSocketPort,		long,				(6415))					\
																				\
	DO_TO_PREF(WarnAboutSkinsDir,	bool,				(false))					\
																				\
	DO_TO_PREF(AskAboutStartMenu,	bool,				(true))					\
	DO_TO_PREF(StartMenuItem,		EmFileRef,			())						\
																				\
	DO_TO_PREF(FillNewBlocks,		bool,				(false))				\
	DO_TO_PREF(FillResizedBlocks,	bool,				(false))				\
	DO_TO_PREF(FillDisposedBlocks,	bool,				(false))				\
	DO_TO_PREF(FillStack,			bool,				(false))				\
																				\
	DO_TO_PREF(LastConfiguration,	Configuration,		(EmDevice ("PalmIII"), 1024, EmFileRef()))	\
																				\
	DO_TO_PREF(GremlinInfo,			GremlinInfo,		())						\
	DO_TO_PREF(HordeInfo,			HordeInfo,			())						\
																				\
	DO_TO_PREF(LastPSF,				EmFileRef,			())						\
																				\
	DO_TO_PREF(ROM_MRU,				EmFileRefList,		())						\
	DO_TO_PREF(PRC_MRU,				EmFileRefList,		())						\
	DO_TO_PREF(PSF_MRU,				EmFileRefList,		())						\
																				\
	DO_TO_PREF(Skins,				SkinNameList,		())						\
	DO_TO_PREF(Scale,				ScaleType,			(2))					\
	DO_TO_PREF(DimWhenInactive,		bool,				(true))					\
	DO_TO_PREF(ShowDebugMode,		bool,				(false))					\
	DO_TO_PREF(ShowGremlinMode,		bool,				(false))					\
	DO_TO_PREF(StayOnTop,			bool,				(false))				\
																				\
	DO_TO_PREF(WarningOff,			EmErrorHandlingOption,	(kShow))			\
	DO_TO_PREF(ErrorOff,			EmErrorHandlingOption,	(kShow))			\
	DO_TO_PREF(WarningOn,			EmErrorHandlingOption,	(kShow))			\
	DO_TO_PREF(ErrorOn,				EmErrorHandlingOption,	(kShow))			\
																				\
	DO_TO_PREF(LastTracerType,		string,				(""))					\
	DO_TO_PREF(TracerTypes,			string,				(""))					\
																				\
	DO_TO_PREF(FfsHome,				string,				(""))					\
																				\
	DO_TO_PREF(SlotList,			SlotInfoList,		())						\


// Declare all the keys
#define DECLARE_PREF_KEYS(name, type, init) extern PrefKeyType kPrefKey##name;
FOR_EACH_PREF(DECLARE_PREF_KEYS)

#endif	// _PREFERENCEMGR_H_
