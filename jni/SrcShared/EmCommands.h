/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef EmCommands_h
#define EmCommands_h

enum EmCommandID
{
	kCommandNone,

	kCommandFirst = 1000,	// PowerPlant reserves all up to 999

	kCommandSessionNew = kCommandFirst,
	kCommandSessionOpenOther,
	kCommandSessionOpen0,
	kCommandSessionOpen1,
	kCommandSessionOpen2,
	kCommandSessionOpen3,
	kCommandSessionOpen4,
	kCommandSessionOpen5,
	kCommandSessionOpen6,
	kCommandSessionOpen7,
	kCommandSessionOpen8,
	kCommandSessionOpen9,
	kCommandSessionClose,

	kCommandSessionSave,
	kCommandSessionSaveAs,
	kCommandSessionBound,
	kCommandScreenSave,
	kCommandSessionInfo,

	kCommandImportOther,
	kCommandImport0,
	kCommandImport1,
	kCommandImport2,
	kCommandImport3,
	kCommandImport4,
	kCommandImport5,
	kCommandImport6,
	kCommandImport7,
	kCommandImport8,
	kCommandImport9,
	kCommandExport,

	kCommandHotSync,
	kCommandReset,
	kCommandDownloadROM,

	kCommandUndo,
	kCommandCut,
	kCommandCopy,
	kCommandPaste,
	kCommandClear,

	kCommandPreferences,
	kCommandLogging,
	kCommandDebugging,
	kCommandErrorHandling,
#if HAS_TRACER
	kCommandTracing,
#endif
	kCommandSkins,
	kCommandHostFS,
	kCommandBreakpoints,

	kCommandGremlinsNew,
	kCommandGremlinsSuspend,
	kCommandGremlinsStep,
	kCommandGremlinsResume,
	kCommandGremlinsStop,

	kCommandEventReplay,
	kCommandEventMinimize,

#if HAS_PROFILING
	kCommandProfileStart,
	kCommandProfileStop,
	kCommandProfileDump,
#endif

	kCommandAbout,
	kCommandQuit,

	kCommandEmpty,

	kCommandFile,
	kCommandEdit,
	kCommandGremlins,
#if HAS_PROFILING
	kCommandProfile,
#endif

	kCommandRoot,
	kCommandOpen,
	kCommandImport,
	kCommandSettings,

	kCommandDivider
};

DEFINE_SCALAR_MODIFIERS (EmCommandID)

#endif	// EmCommands_h
