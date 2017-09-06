/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "HostControl.h"
#include "HostControlPrv.h"

#include "DebugMgr.h"			// gDebuggerGlobals
#include "EmApplication.h"		// gApplication, ScheduleQuit
#include "EmBankMapped.h"		// EmBankMapped::GetEmulatedAddress
#include "EmCPU68K.h"			// gCPU68K, gStackHigh, etc.
#include "EmDirRef.h"			// EmDirRefList
#include "EmDlg.h"				// DoGetFile, DoPutFile, DoGetDirectory
#include "EmDocument.h"			// gDocument, HostSaveScreen, ScheduleNewHorde
#include "EmErrCodes.h"			// kError_NoError
#include "EmExgMgr.h"			// EmExgMgr::GetExgMgr
#include "EmFileImport.h"		// EmFileImport::LoadPalmFileList
#include "EmFileRef.h"			// EmFileRefList
#include "EmMemory.h"			// EmMem_strlen, EmMem_strcpy
#include "EmPalmStructs.h"		// EmAliasErr
#include "EmPatchState.h"		// EmPatchState::UIInitialized
#include "EmRPC.h"				// RPC::HandlingPacket, RPC::DeferCurrentPacket
#include "EmSession.h"			// ResumeByExternal
#include "EmStreamFile.h"		// EmStreamFile
#include "EmStructs.h"			// StringList, ByteList
#include "Hordes.h"				// Hordes::IsOn
#include "LoadApplication.h"	// SavePalmFile
#include "Logging.h"			// LogFile
#include "Miscellaneous.h"		// GetDeviceTextList, GetMemoryTextList
#include "Platform.h"			// Platform::GetShortVersionString
#include "Profiling.h"			// ProfileInit, ProfileStart, ProfileStop, etc.
#include "ROMStubs.h"			// EvtWakeup
#include "Strings.r.h"			// kStr_ProfileResults

#include <ctype.h>				// isdigit

#if HAS_TRACER
#include "TracerPlatform.h"		// Tracer
#endif


#include "EmSubroutine.h"
#include "Marshal.h"

#include "PHEMNativeIF.h"

#define CALLED_GET_PARAM_FILE(name)					\
	CALLED_GET_PARAM_VAL (emuptr, name);			\
													\
	FILE*	fh	= PrvToFILE (name)


#if PLATFORM_WINDOWS

#include <direct.h>				// _mkdir, _rmdir
#include <sys/types.h>			// 
#include <sys/stat.h>			// struct _stat
#include <sys/utime.h>			// _utime, _utimbuf
#include <io.h>				// _open
#include <fcntl.h>			// O_RDWR
#include <time.h>			// asctime, clock, clock_t, etc.

typedef struct _stat	_stat_;
typedef struct _utimbuf _utimbuf_;

#else

#include "sys/stat.h"			// mkdir(???), stat
#include "unistd.h"				// rmdir
#include "fcntl.h"				// O_RDWR
#include "utime.h"				// utime
#include <time.h>				// asctime, clock, clock_t, etc.
#include "errno.h"				// EACCES
// RAI 2013 for HostGetFSSize and such
#include <sys/vfs.h>
#define statvfs statfs
#define fstatvfs fstatfs

/* Update for GCC 4 */
#include <stdlib.h>


#define _O_RDWR		O_RDWR

typedef struct stat		_stat_;
typedef struct utimbuf	_utimbuf_;

inline int	_mkdir (const char* path)
{
#if PLATFORM_MAC
	return mkdir (path);
#else
	return mkdir (path, 0777);
#endif
}

inline int	_rmdir (const char* path)
{
	return rmdir (path);
}

inline int	_stat (const char * path, struct stat * buf)
{
	return stat (path, buf);
}

inline int	_open (const char * path, int mode)
{
	return open (path, mode);
}

inline int	_chsize (int s, off_t offset)
{
	return ftruncate (s, offset);
}

inline int	_close (int s)
{
	return close (s);
}

inline int	_utime (const char * path, const utimbuf * times)
{
	return utime (path, times);
}

#endif

struct MyDIR
{
	EmDirRefList	fDirs;
	EmFileRefList	fFiles;

	EmDirRefList::iterator	fDirIter;
	EmFileRefList::iterator	fFileIter;

	int				fState;		// 0 = new, 1 = iterating dirs, 2 = iterating files, 3 = done
};


typedef void		(*HostHandler) (void);

static HostHandler	PrvHostGetHandler		(HostControlSelectorType selector);

static Bool 		PrvCollectParameters	(EmSubroutine& sub, const string& fmt,
											 ByteList& stackData, StringList& stringData);
static void 		PrvPushShort			(EmSubroutine& sub, ByteList& stackData);
static void 		PrvPushLong				(EmSubroutine& sub, ByteList& stackData);
static void 		PrvPushDouble			(EmSubroutine& sub, ByteList& stackData);
static void 		PrvPushLongDouble		(EmSubroutine& sub, ByteList& stackData);
static void 		PrvPushString			(EmSubroutine& sub, ByteList& stackData,
											 StringList& stringData);

static FILE*		PrvToFILE				(emuptr);

static void			PrvTmFromHostTm			(struct tm& dest, const HostTmType& src);
static void			PrvHostTmFromTm			(EmProxyHostTmType& dest, const struct tm& src);

static void			PrvMapAndReturn			(const void* p, long size, EmSubroutine& sub);
static void			PrvMapAndReturn			(const string& s, EmSubroutine& sub);
static void			PrvReturnString			(const char* p, EmSubroutine& sub);
static void			PrvReturnString			(const string& s, EmSubroutine& sub);

static void			PrvReleaseAllResources	(void);

static emuptr		PrvMalloc				(long size);
static emuptr		PrvRealloc				(emuptr p, long size);
static void			PrvFree					(emuptr p);


// Write to this "file" if you want to intertwine your
// output with that created by any host logging facilities
// (such as event logging).

#define hostLogFile ((HostFILEType*) -1)
#define hostLogFILE ((FILE*) -1)


inline int x_fclose (FILE* f)
{
	if (f == hostLogFILE)
	{
		return 0;
	}

	return fclose (f);
}

inline int x_feof (FILE* f)
{
	return feof (f);
}

inline int x_ferror (FILE* f)
{
	return ferror (f);
}

inline int x_fflush (FILE* f)
{
	if (f == hostLogFILE)
	{
		LogDump ();
		return 0;
	}

	return fflush (f);
}

inline int x_fgetc (FILE* f)
{
	if (f == hostLogFILE)
	{
		return EOF;
	}

	return fgetc (f);
}

inline char* x_fgets (char* s, int n, FILE* f)
{
	if (f == hostLogFILE)
	{
		return NULL;
	}

	return fgets (s, n, f);
}

inline int x_vfprintf (FILE* f, const char* fmt, va_list args)
{
	if (f == hostLogFILE)
	{
		return LogGetStdLog ()->VPrintf (fmt, args);
	}

	return vfprintf (f, fmt, args);
}

inline int x_fputc (int c, FILE* f)
{
	if (f == hostLogFILE)
	{
		return LogGetStdLog ()->Printf ("%c", c);
	}

	return fputc (c, f);
}

inline int x_fputs (const char* s, FILE* f)
{
	if (f == hostLogFILE)
	{
		return LogGetStdLog ()->Printf ("%s", s);
	}

	return fputs (s, f);
}

inline size_t x_fread (void* buffer, size_t size, size_t count, FILE* f)
{
	if (f == hostLogFILE)
	{
		return 0;
	}

	return fread (buffer, size, count, f);
}

inline int x_fseek (FILE* f, long offset, int origin)
{
	if (f == hostLogFILE)
	{
		return -1;
	}

	return fseek (f, offset, origin);
}

inline long x_ftell (FILE* f)
{
	if (f == hostLogFILE)
	{
		return -1;
	}

	return ftell (f);
}

inline size_t x_fwrite (const void* buffer, size_t size, size_t count, FILE* f)
{
	if (f == hostLogFILE)
	{
		return LogGetStdLog ()->Write (buffer, size * count);
	}

	return fwrite (buffer, size, count, f);
}

static int translate_err_no (int err_no)
{
	switch (err_no)
	{
		case 0:				return hostErrNone;
		case EACCES:
		case EPERM:			return hostErrPermissions;
		case ENOENT:		return hostErrFileNotFound;
		case EIO:			return hostErrDiskError;
		case EBADF:			return hostErrInvalidParameter;
		case ENOMEM:		return hostErrOutOfMemory;
		case EFAULT:		return hostErrInvalidParameter;
		case EEXIST:		return hostErrExists;
		case ENOTDIR:		return hostErrNotADirectory;
		case EISDIR:		return hostErrIsDirectory;
		case EINVAL:		return hostErrInvalidParameter;
		case ENFILE:
		case EMFILE:		return hostErrTooManyFiles;
		case EFBIG:			return hostErrFileTooBig;
		case ENOSPC:		return hostErrDiskFull;
		case EROFS:			return hostErrReadOnlyFS;
		case ENAMETOOLONG:	return hostErrFileNameTooLong;
		case ENOTEMPTY:		return hostErrDirNotEmpty;
		case ENOSYS:
		case ENODEV:		return hostErrOpNotAvailable;
		default:			return hostErrUnknownError;
	}
}

HostHandler			gHandlerTable [hostSelectorLastTrapNumber];

vector<FILE*>		gOpenFiles;
vector<MyDIR*>		gOpenDirs;
vector<void*>		gAllocatedBlocks;
HostDirEntType		gHostDirEnt;
string				gResultString;
EmProxyHostTmType	gGMTime;
EmProxyHostTmType	gLocalTime;


// ---------------------------------------------------------------------------
//		¥ HandleHostControlCall
// ---------------------------------------------------------------------------

CallROMType HandleHostControlCall (void)
{
	CALLED_SETUP ("UInt4", "HostControlSelectorType selector");

	CALLED_GET_PARAM_VAL (HostControlSelectorType, selector);

        PHEM_Log_Msg("HostControl selector:");
        PHEM_Log_Place(selector);
	HostHandler	fn = PrvHostGetHandler (selector);

	if (fn)
	{
                PHEM_Log_Msg("Calling function...");
		fn ();
	}
	else
	{
		// !!! Display an "unknown function" error message.
                PHEM_Log_Msg("Yikes! Unknown function!");
	}

	return kSkipROM;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostGetHostVersion
// ---------------------------------------------------------------------------

static void _HostGetHostVersion (void)
{
	// long HostGetHostVersion (void)

	CALLED_SETUP_HC ("Int32", "void");

	enum { kMajor, kMinor, kFix, kBuild };

	int major		= 0;
	int minor		= 0;
	int fix 		= 0;
	int stage		= sysROMStageRelease;
	int buildNum	= 0;
	int state		= kMajor;

	string				version (Platform::GetShortVersionString ());
	string::iterator	iter;

	for (iter = version.begin (); iter != version.end (); ++iter)
	{
		char	ch = *iter;

		switch (state)
		{
			case kMajor:
				if (isdigit (ch))
					major = major * 10 + ch - '0';
				else if (ch == '.')
					state = kMinor;
				else
					goto VersionParseDone;
				break;

			case kMinor:
			case kFix:
				if (isdigit (ch))
				{
					if (state == kMinor)
						minor = minor * 10 + ch - '0';
					else
						fix = fix * 10 + ch - '0';
				}
				else if (ch == '.')
				{
					if (state == kMinor)
						state = kFix;
					else
						goto VersionParseDone;
				}
				else if (ch == 'd')
				{
					stage = sysROMStageDevelopment;
					state = kBuild;
				}
				else if (ch == 'a')
				{
					stage = sysROMStageAlpha;
					state = kBuild;
				}
				else if (ch == 'b')
				{
					stage = sysROMStageBeta;
					state = kBuild;
				}
				else
					goto VersionParseDone;
				break;

			case kBuild:
				if (isdigit (ch))
					buildNum = buildNum * 10 + ch - '0';
				else
					goto VersionParseDone;
				break;
		}
	}

VersionParseDone:
	
	// Return the result.

	Int32	result = sysMakeROMVersion (major, minor, fix, stage, buildNum);
	PUT_RESULT_VAL (Int32, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostGetHostID
// ---------------------------------------------------------------------------

static void _HostGetHostID (void)
{
	// HostIDType HostGetHostID (void)

	CALLED_SETUP_HC ("HostIDType", "void");

	// Return the result.

	PUT_RESULT_VAL (HostIDType, hostIDPalmOSEmulator);
}


// ---------------------------------------------------------------------------
//		¥ _HostGetHostPlatform
// ---------------------------------------------------------------------------

static void _HostGetHostPlatform (void)
{
	// HostPlatformType HostGetHostPlatform (void)

	CALLED_SETUP_HC ("HostPlatformType", "void");

	// Return the result.

#if PLATFORM_WINDOWS
	PUT_RESULT_VAL (HostPlatformType, hostPlatformWindows);
#elif PLATFORM_MAC
	PUT_RESULT_VAL (HostPlatformType, hostPlatformMacintosh);
#elif PLATFORM_UNIX
	PUT_RESULT_VAL (HostPlatformType, hostPlatformUnix);
#else
	#error "Unsupported platform"
#endif
}


// ---------------------------------------------------------------------------
//		¥ _HostIsSelectorImplemented
// ---------------------------------------------------------------------------

static void _HostIsSelectorImplemented (void)
{
	// HostBoolType HostIsSelectorImplemented (long selector)

	CALLED_SETUP_HC ("HostBoolType", "long selector");

	CALLED_GET_PARAM_VAL (long, selector);

	HostHandler fn = PrvHostGetHandler ((HostControlSelectorType) selector);

	// Return the result.

	PUT_RESULT_VAL (HostBoolType, (fn != NULL));
}


// ---------------------------------------------------------------------------
//		¥ _HostGestalt
// ---------------------------------------------------------------------------

static void _HostGestalt (void)
{
	// HostErrType HostGestalt (long gestSel, long* response)

	CALLED_SETUP_HC ("HostErrType", "long gestSel, long* response");

	// Return the result.

	PUT_RESULT_VAL (HostErrType, hostErrUnknownGestaltSelector);
}


// ---------------------------------------------------------------------------
//		¥ _HostIsCallingTrap
// ---------------------------------------------------------------------------

static void _HostIsCallingTrap (void)
{
	// HostBoolType HostIsCallingTrap (void)

	CALLED_SETUP_HC ("HostBoolType", "void");

	// Return the result.

	PUT_RESULT_VAL (HostBoolType, gSession->IsNested ());
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostProfileInit
// ---------------------------------------------------------------------------
#if HAS_PROFILING

static void _HostProfileInit (void)
{
	// HostErrType HostProfileInit (long maxCalls, long maxDepth)

	CALLED_SETUP_HC ("HostErrType", "long maxCalls, long maxDepth");

	if (!::ProfileCanInit ())
	{
		PUT_RESULT_VAL (HostErrType, hostErrProfilingNotReady);
		return;
	}

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (long, maxCalls);
	CALLED_GET_PARAM_VAL (long, maxDepth);

	// Call the function.

	::ProfileInit (maxCalls, maxDepth);

	// Return the result.

	PUT_RESULT_VAL (HostErrType, hostErrNone);
}

#endif


// ---------------------------------------------------------------------------
//		¥ _HostProfileStart
// ---------------------------------------------------------------------------
#if HAS_PROFILING

static void _HostProfileStart (void)
{
	// HostErrType HostProfileStart (void)

	CALLED_SETUP_HC ("HostErrType", "void");

	if (!::ProfileCanStart ())
	{
		PUT_RESULT_VAL (HostErrType, hostErrProfilingNotReady);
		return;
	}

	// Call the function.

	::ProfileStart ();

	// Return the result.

	PUT_RESULT_VAL (HostErrType, hostErrNone);
}

#endif


// ---------------------------------------------------------------------------
//		¥ _HostProfileStop
// ---------------------------------------------------------------------------
#if HAS_PROFILING

static void _HostProfileStop (void)
{
	// HostErrType HostProfileStop (void)

	CALLED_SETUP_HC ("HostErrType", "void");

	if (!::ProfileCanStop ())
	{
		PUT_RESULT_VAL (HostErrType, hostErrProfilingNotReady);
		return;
	}

	// Call the function.

	::ProfileStop ();

	// Return the result.

	PUT_RESULT_VAL (HostErrType, hostErrNone);
}

#endif


// ---------------------------------------------------------------------------
//		¥ _HostProfileDump
// ---------------------------------------------------------------------------
#if HAS_PROFILING

static void _HostProfileDump (void)
{
	// HostErrType HostProfileDump (const char* filenameP)

	CALLED_SETUP_HC ("HostErrType", "const char* filenameP");

	if (!::ProfileCanDump ())
	{
		PUT_RESULT_VAL (HostErrType, hostErrProfilingNotReady);
		return;
	}

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, filenameP);

	// Call the function.

	::ProfileDump (filenameP);

	// Return the result.

	PUT_RESULT_VAL (HostErrType, hostErrNone);
}

#endif


// ---------------------------------------------------------------------------
//		¥ _HostProfileCleanup
// ---------------------------------------------------------------------------
#if HAS_PROFILING

static void _HostProfileCleanup (void)
{
	// HostErrType HostProfileCleanup (void)

	CALLED_SETUP_HC ("HostErrType", "void");

	if (!::ProfileCanInit ())
	{
		PUT_RESULT_VAL (HostErrType, hostErrProfilingNotReady);
		return;
	}

	// Get the caller's parameters.

	// Call the function.

	// ProfileCleanup is now performed at the end of a dump.
//	::ProfileCleanup ();

	// Return the result.

	PUT_RESULT_VAL (HostErrType, hostErrNone);
}

#endif


// ---------------------------------------------------------------------------
//		¥ _HostProfileDetailFn
// ---------------------------------------------------------------------------
#if HAS_PROFILING

static void _HostProfileDetailFn (void)
{
	// HostErrType _HostProfileDetailFn (void* addr, HostBoolType logDetails)

	CALLED_SETUP_HC ("HostErrType", "void* addr, HostBoolType logDetails");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (emuptr, addr);
	CALLED_GET_PARAM_VAL (HostBoolType, logDetails);

	// Call the function.

	ProfileDetailFn (addr, logDetails);

	// Return the result.

	PUT_RESULT_VAL (HostErrType, hostErrNone);
}

#endif


// ---------------------------------------------------------------------------
//		¥ _HostProfileGetCycles
// ---------------------------------------------------------------------------
#if HAS_PROFILING

static void _HostProfileGetCycles (void)
{
	// long	HostProfileGetCycles(void)

	CALLED_SETUP_HC ("long", "void");

	// Get the caller's parameters.

	// Return the result.

	PUT_RESULT_VAL (long, gClockCycles);
}

#endif


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostErrNo
// ---------------------------------------------------------------------------

static void _HostErrNo (void)
{
	// long HostErrNo (void)

	CALLED_SETUP_HC ("long", "void");

	// Get the caller's parameters.

	// Return the result.

	PUT_RESULT_VAL (long, translate_err_no (errno));
}


#pragma mark -


// ---------------------------------------------------------------------------
//		¥ _HostFClose
// ---------------------------------------------------------------------------

static void _HostFClose (void)
{
	// long HostFClose (HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, EOF);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	int 	result = x_fclose (fh);

	vector<FILE*>::iterator	iter = gOpenFiles.begin ();
	while (iter != gOpenFiles.end ())
	{
		if (*iter == fh)
		{
			gOpenFiles.erase (iter);
			break;
		}

		++iter;
	}

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFEOF
// ---------------------------------------------------------------------------

static void _HostFEOF (void)
{
	// long HostFEOF (HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, 1);	// At end of file (right choice?)
		return;
	}

	// Call the function.

	int 	result = x_feof (fh);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFError
// ---------------------------------------------------------------------------

static void _HostFError (void)
{
	// long HostFError (HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, hostErrInvalidParameter);
		return;
	}

	// Call the function.

	int 	result = x_ferror (fh);

	// Return the result.

	PUT_RESULT_VAL (long, translate_err_no (result));
}


// ---------------------------------------------------------------------------
//		¥ _HostFFlush
// ---------------------------------------------------------------------------

static void _HostFFlush (void)
{
	// long HostFFlush (HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, EOF);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	int 	result = x_fflush (fh);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFGetC
// ---------------------------------------------------------------------------

static void _HostFGetC (void)
{
	// long HostFGetC (HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, EOF);	// No file, no data...
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	int 	result = x_fgetc (fh);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFGetPos
// ---------------------------------------------------------------------------

static void _HostFGetPos (void)
{
	// long HostFGetPos (HostFILEType* fileP, long* posP)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP, long* posP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);
	CALLED_GET_PARAM_REF (long, posP, Marshal::kInput);

	// Check the parameters.

	if (!fh || posP == EmMemNULL)
	{
		PUT_RESULT_VAL (long, 1);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	long	pos = x_ftell (fh);

	// If the function succeeded, return the position in
	// the memory location pointed to by "posP".

	if (pos >= 0)	// success
	{
		*posP = pos;
		CALLED_PUT_PARAM_REF (posP);
	}

	// Return the result.

	PUT_RESULT_VAL (long, (pos == -1 ? 1 : 0));
}


// ---------------------------------------------------------------------------
//		¥ _HostFGetS
// ---------------------------------------------------------------------------

static void _HostFGetS (void)
{
	// char* HostFGetS (char* s, long n, HostFILEType* fileP)

	CALLED_SETUP_HC ("char*", "char* s, long n, HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (uint32, n);
	CALLED_GET_PARAM_PTR (char, s, n, Marshal::kOutput);
	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh || s == EmMemNULL)
	{
		PUT_RESULT_VAL (emuptr, EmMemNULL);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	emuptr	returnVal = EmMemNULL;

	if (n > 0)
	{
		char*	result = x_fgets (s, (int) n, fh);

		// If we were able to read the string, copy it into the
		// user's buffer (using EmMem_strcpy to take care of real
		// <-> emulated memory mapping.  If the read failed,
		// return NULL.

		if (result != NULL)
		{
			CALLED_PUT_PARAM_REF (s);

			returnVal = (emuptr) s;
		}
	}

	// Return the result.

	PUT_RESULT_VAL (emuptr, returnVal);
}


// ---------------------------------------------------------------------------
//		¥ _HostFOpen
// ---------------------------------------------------------------------------

static void _HostFOpen (void)
{
	// HostFILEType* HostFOpen (const char* name, const char* mode)

	CALLED_SETUP_HC ("HostFILEType*", "const char* name, const char* mode");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, name);
	CALLED_GET_PARAM_STR (char, mode);

	// Check the parameters.

	if (name == EmMemNULL || mode == EmMemNULL)
	{
		PUT_RESULT_VAL (emuptr, EmMemNULL);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

#if PLATFORM_UNIX
        // RAI 2013
        // Note: HostFS assumes that, if a directory is opened, HostFOpen will fail.
        // Thus, on Unix, we need to check if the specified file is a directory.
        FILE *result = NULL;
        struct stat stat_buf;

        stat(name, &stat_buf);

        if (S_ISDIR(stat_buf.st_mode)) {
          errno = EISDIR; // HostFS.prc expects this
        } else  {
          result = fopen (name, mode);
          if (result)
          {
                gOpenFiles.push_back (result);
          }
        }
#else
	FILE*	result = fopen (name, mode);
#endif

	if (result)
	{
		gOpenFiles.push_back (result);
	}

	// Return the result.

	PUT_RESULT_VAL (emuptr, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFPrintF
// ---------------------------------------------------------------------------

static void _HostFPrintF (void)
{
	// long HostFPrintF (HostFILEType* fileP, const char* fmt, ...)

	CALLED_SETUP_STDARG_HC ("long", "HostFILEType* fileP, const char* fmt");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);
	CALLED_GET_PARAM_STR (char, fmt);

	// Check the parameters.

	if (!fh || fmt == EmMemNULL)
	{
		PUT_RESULT_VAL (long, EOF);
		errno = hostErrInvalidParameter;
		return;
	}

	// Collect the specified parameters. We need to make copies of everything
	// so that it's in the right endian order and to reverse any effects
	// of wordswapping.  The values specified on the stack (the integers,
	// chars, doubles, pointers, etc.) get converted and placed in stackData.
	// The data pointed to by the pointers gets converted and placed in stringData.

	ByteList	stackData;
	StringList stringData;

	if (!::PrvCollectParameters (sub, string (fmt), stackData, stringData))
	{
		PUT_RESULT_VAL (long, EOF);
		errno = hostErrInvalidParameter;
		return;
	}

	// Write everything out to the file using vfprintf.

        // RAI 2013: Casting to va_list is a no-no. There simply is no portable
        // way to do it. It's kind of a miracle it ever worked on as many platforms
        // and compilers as it did. AMD64 and ARM are examples of platforms where
        // it does *not* work.
        //
        // On the bright side, I can't find any evidence of anyone actually
        // calling this function.
        //
	//int 	result = x_vfprintf (fh, fmt, (va_list) &stackData[0]);
	int 	result = 0;

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFPutC
// ---------------------------------------------------------------------------

static void _HostFPutC (void)
{
	// long HostFPutC (long c, HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "long c, HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (long, c);
	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, EOF);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	int 	result = x_fputc ((int) c, fh);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFPutS
// ---------------------------------------------------------------------------

static void _HostFPutS (void)
{
	// long HostFPutS (const char* s, HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "const char* s, HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, s);
	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh || s == EmMemNULL)
	{
		PUT_RESULT_VAL (long, EOF);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	int 	result = x_fputs (s, fh);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFRead
// ---------------------------------------------------------------------------

static void _HostFRead (void)
{
	// long HostFRead (void* buffer, long size, long count, HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "void* buffer, long size, long count, HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (long, size);
	CALLED_GET_PARAM_VAL (long, count);
	CALLED_GET_PARAM_PTR (void, buffer, size * count, Marshal::kOutput);
	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh || buffer == EmMemNULL)
	{
		PUT_RESULT_VAL (long, 0);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	size_t	result = x_fread (buffer, size, count, fh);

	// If the read succeeded, copy the data into the user's buffer.

	if (result)
	{
		CALLED_PUT_PARAM_REF (buffer);
	}

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFReopen
// ---------------------------------------------------------------------------

#if 0
static void _HostFReopen (void)
{
}
#endif


// ---------------------------------------------------------------------------
//		¥ _HostFScanF
// ---------------------------------------------------------------------------

#if 0
static void _HostFScanF (void)
{
}
#endif


// ---------------------------------------------------------------------------
//		¥ _HostFSeek
// ---------------------------------------------------------------------------

static void _HostFSeek (void)
{
	// long HostFSeek (HostFILEType* fileP, long offset, long origin)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP, long offset, long origin");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);
	CALLED_GET_PARAM_VAL (long, offset);
	CALLED_GET_PARAM_VAL (long, origin);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, -1);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	int 	result	= x_fseek (fh, offset, (int) origin);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFSetPos
// ---------------------------------------------------------------------------

static void _HostFSetPos (void)
{
	// long HostFSetPos (HostFILEType* fileP, long* posP)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP, long* posP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);
	CALLED_GET_PARAM_REF (long, posP, Marshal::kInput);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, 1);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	int 	result = x_fseek (fh, *posP, SEEK_SET);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFTell
// ---------------------------------------------------------------------------

static void _HostFTell (void)
{
	// long HostFTell (HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh)
	{
		PUT_RESULT_VAL (long, -1);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	long	result = x_ftell (fh);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFWrite
// ---------------------------------------------------------------------------

static void _HostFWrite (void)
{
	// long HostFWrite (const void* buffer, long size, long count, HostFILEType* fileP)

	CALLED_SETUP_HC ("long", "const void* buffer, long size, long count, HostFILEType* fileP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (long, size);
	CALLED_GET_PARAM_VAL (long, count);
	CALLED_GET_PARAM_PTR (void, buffer, size * count, Marshal::kInput);
	CALLED_GET_PARAM_FILE (fileP);

	// Check the parameters.

	if (!fh || buffer == EmMemNULL)
	{
		PUT_RESULT_VAL (long, 0);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	size_t	result = x_fwrite (buffer, size, count, fh);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostRemove
// ---------------------------------------------------------------------------

static void _HostRemove (void)
{
	// long HostRemove(const char* nameP)

	CALLED_SETUP_HC ("long", "const char* nameP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);

	// Call the function.

	int	result = remove (nameP);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostRename
// ---------------------------------------------------------------------------

static void _HostRename (void)
{
	// long HostRename(const char* oldNameP, const char* newNameP)

	CALLED_SETUP_HC ("long", "const char* oldNameP, const char* newNameP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, oldNameP);
	CALLED_GET_PARAM_STR (char, newNameP);

	// Call the function.

	int	result = rename (oldNameP, newNameP);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostTmpFile
// ---------------------------------------------------------------------------

static void _HostTmpFile (void)
{
	// HostFILEType* HostTmpFile (void)

	CALLED_SETUP_HC ("HostFILEType*", "void");

	// Get the caller's parameters.

	// Call the function.

	FILE*		result = tmpfile ();

	// Return the result.

	PUT_RESULT_VAL (emuptr, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostTmpNam
// ---------------------------------------------------------------------------

static void _HostTmpNam (void)
{
	// char* HostTmpNam (char* nameP)

	CALLED_SETUP_HC ("char*", "char* nameP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_PTR (char, nameP, HOST_NAME_MAX, Marshal::kOutput);

	// Check the parameters.

	// Call the function.

#if HAVE_MKSTEMP

	// Try to find a good home for this file...

	char*	result = NULL;
	char*	env = NULL;
	char	temp_buff[HOST_NAME_MAX];

	if (!env)
		env = getenv ("TMPDIR");

	if (!env)
		env = getenv ("TEMP");

#if defined (P_tmpdir)
	if (!env)
		env = P_tmpdir;
#else
	if (!env)
		env = "/tmp";
#endif

	strcpy (temp_buff, env);
	strcat (temp_buff, "/pose.XXXXXX");

	// Create the temporary file name.

	int fd = mkstemp (temp_buff);

	// If that succeeded, close the file that was created
	// and remember the file name.

	if (fd > 0 && !close (fd))
	{
		result = temp_buff;

		if (nameP != EmMemNULL)
		{
			strcpy ((char*) nameP, temp_buff);
		}
	}

#else

	char*	result = tmpnam (nameP);

#endif

	// Return the result.

	if (!result)
	{
		PUT_RESULT_VAL (emuptr, EmMemNULL);
	}
	else
	{
		if (nameP != EmMemNULL)
		{
			CALLED_PUT_PARAM_REF (nameP);
			PUT_RESULT_VAL (emuptr, nameP);
		}
		else
		{
			::PrvReturnString (result, sub);
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ _HostGetEnv
// ---------------------------------------------------------------------------

static void _HostGetEnv (void)
{
	// char* HostGetEnv (const char* nameP)

	CALLED_SETUP_HC ("char*", "char* nameP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);

	// Check the parameters.

	if (nameP == EmMemNULL)
	{
		PUT_RESULT_VAL (emuptr, EmMemNULL);
		errno = hostErrInvalidParameter;
		return;
	}

	// Call the function.

	char*	value = getenv (nameP);

	// Return the result.

	::PrvReturnString (value, sub);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostMalloc
// ---------------------------------------------------------------------------

static void _HostMalloc (void)
{
	// void* HostMalloc(long size)

	CALLED_SETUP_HC ("void*", "long size");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (long, size);

	// Call the function.

	emuptr	result = ::PrvMalloc (size);

	// Return the result.

	PUT_RESULT_VAL (emuptr, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostRealloc
// ---------------------------------------------------------------------------

static void _HostRealloc (void)
{
	// void* HostRealloc(void* p, long size)

	CALLED_SETUP_HC ("void*", "void* p, long size");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (emuptr, p);
	CALLED_GET_PARAM_VAL (long, size);

	// Call the function.

	emuptr	result;

	if (!p)
	{
		result = ::PrvMalloc (size);
	}
	else if (!size)
	{
		::PrvFree (p);
		result = EmMemNULL;
	}
	else
	{
		result = ::PrvRealloc (p, size);
	}

	// Return the result.

	PUT_RESULT_VAL (emuptr, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostFree
// ---------------------------------------------------------------------------

static void _HostFree (void)
{
	// void HostFree(void* p)

	CALLED_SETUP_HC ("void", "void* p");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (emuptr, p);

	// Call the function.

	::PrvFree (p);
}


// ---------------------------------------------------------------------------
//		¥ _HostAscTime
// ---------------------------------------------------------------------------

static void _HostAscTime (void)
{
	// char* HostAscTime(const HostTmType* hisTmP)

	CALLED_SETUP_HC ("char*", "HostTmType* hisTmP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_REF (HostTmType, hisTmP, Marshal::kInput);

	struct tm	myTm;

	::PrvTmFromHostTm (myTm, *hisTmP);

	// Call the function.

	char*	result = asctime (&myTm);

	// Return the result.

	::PrvReturnString (result, sub);
}


// ---------------------------------------------------------------------------
//		¥ _HostClock
// ---------------------------------------------------------------------------

static void _HostClock (void)
{
	// HostClockType HostClock(void)

	CALLED_SETUP_HC ("HostClockType", "void");

	// Get the caller's parameters.

	// Call the function.

	clock_t	result = clock ();

	// Return the result.

	PUT_RESULT_VAL (HostClockType, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostCTime
// ---------------------------------------------------------------------------

static void _HostCTime (void)
{
	// char* HostCTime(const HostTimeType*)

	CALLED_SETUP_HC ("char*", "HostTimeType* timeP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_REF (HostTimeType, timeP, Marshal::kInput);

	time_t	myTime = *timeP;

	// Call the function.

	char*	result = ctime (&myTime);

	// Return the result.

	::PrvReturnString (result, sub);
}


// ---------------------------------------------------------------------------
//		¥ _HostDiffTime
// ---------------------------------------------------------------------------

#if 0
static void _HostDiffTime (void)
{
}
#endif


// ---------------------------------------------------------------------------
//		¥ _HostGMTime
// ---------------------------------------------------------------------------

static void _HostGMTime (void)
{
	// HostTmType* HostGMTime(const HostTimeType* timeP)

	CALLED_SETUP_HC ("void*", "HostTimeType* timeP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_REF (HostTimeType, timeP, Marshal::kInput);

	time_t	myTime = *timeP;

	// Call the function.

	struct tm*	result = gmtime (&myTime);

	::PrvHostTmFromTm (gGMTime, *result);

	// Return the result.

	::PrvMapAndReturn (&gGMTime, sizeof (gGMTime), sub);
}


// ---------------------------------------------------------------------------
//		¥ _HostLocalTime
// ---------------------------------------------------------------------------

static void _HostLocalTime (void)
{
	// HostTmType* HostLocalTime(const HostTimeType* timeP)

	CALLED_SETUP_HC ("HostTmType*", "HostTimeType* timeP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_REF (HostTimeType, timeP, Marshal::kInput);

	time_t	myTime = *timeP;

	// Call the function.

	struct tm*	result = localtime (&myTime);

	::PrvHostTmFromTm (gLocalTime, *result);

	// Return the result.

	::PrvMapAndReturn (&gLocalTime, sizeof (gLocalTime), sub);
}


// ---------------------------------------------------------------------------
//		¥ _HostMkTime
// ---------------------------------------------------------------------------

static void _HostMkTime (void)
{
	// HostTimeType HostMkTime(HostTmType* timeP)

	CALLED_SETUP_HC ("HostTimeType", "HostTmType* timeP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_REF (HostTmType, timeP, Marshal::kInput);

	struct tm	myTm;

	::PrvTmFromHostTm (myTm, *timeP);

	// Call the function.

	time_t	result = mktime (&myTm);

	// Return the result.

	PUT_RESULT_VAL (HostTimeType, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostStrFTime
// ---------------------------------------------------------------------------

static void _HostStrFTime (void)
{
	// HostSizeType HostStrFTime(char*, HostSizeType, const char*, const HostTmType*)

	CALLED_SETUP_HC ("HostSizeType", "char* strDest, HostSizeType maxsize, const char* format,"
							"const HostTmType* timeP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (HostSizeType, maxsize);
	CALLED_GET_PARAM_PTR (char, strDest, maxsize, Marshal::kOutput);
	CALLED_GET_PARAM_STR (char, format);
	CALLED_GET_PARAM_REF (HostTmType, timeP, Marshal::kInput);

	if (strDest == EmMemNULL || format == EmMemNULL || timeP == EmMemNULL)
	{
		PUT_RESULT_VAL (HostSizeType, 0);
		return;
	}

	struct tm	myTime;

	::PrvTmFromHostTm (myTime, *timeP);

	// Call the function.

	size_t	result = strftime (strDest, maxsize, format, &myTime);

	CALLED_PUT_PARAM_REF (strDest);

	// Return the result.

	PUT_RESULT_VAL (HostSizeType, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostTime
// ---------------------------------------------------------------------------

static void _HostTime (void)
{
	// HostTimeType HostTime(HostTimeType*)

	CALLED_SETUP_HC ("HostTimeType", "HostTimeType* timeP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_REF (HostTimeType, timeP, Marshal::kOutput);

	// Call the function.

	time_t	result2;
	time_t	result = time (&result2);

	if (timeP)
	{
		*timeP = result2;
		CALLED_PUT_PARAM_REF (timeP);
	}

	// Return the result.

	PUT_RESULT_VAL (HostTimeType, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostMkDir
// ---------------------------------------------------------------------------

static void _HostMkDir (void)
{
	// long HostMkDir (const char* nameP)

	CALLED_SETUP_HC ("long", "char* nameP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);

	// Call the function.

	int	result = _mkdir (nameP);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostRmDir
// ---------------------------------------------------------------------------

static void _HostRmDir (void)
{
	// long HostRmDir (const char* nameP)

	CALLED_SETUP_HC ("long", "char* nameP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);

	// Call the function.

	int	result = _rmdir (nameP);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostOpenDir
// ---------------------------------------------------------------------------

static void _HostOpenDir (void)
{
	// HostDIRType* HostOpenDir(const char*)

	CALLED_SETUP_HC ("HostDIRType*", "char* nameP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);

	// See if the directory exists.

	EmDirRef	dirRef (nameP);
	if (!dirRef.Exists ())
	{
		PUT_RESULT_VAL (emuptr, EmMemNULL);
		return;
	}

	// Create a new MyDIR object to pass back as the HostDIRType.

	MyDIR*	dir = new MyDIR;
	if (!dir)
	{
		PUT_RESULT_VAL (emuptr, EmMemNULL);
		return;
	}

	// Initialize the MyDIR with the child entries.

	dirRef.GetChildren (&dir->fFiles, &dir->fDirs);
	dir->fState = 0;

	// Remember this on our list of open directories and
	// pass it back to the user.

	gOpenDirs.push_back (dir);

	// Return the result.

	PUT_RESULT_VAL (emuptr, dir);
}


// ---------------------------------------------------------------------------
//		¥ _HostReadDir
// ---------------------------------------------------------------------------

static void _HostReadDir(void)
{
	// HostDirEntType* HostReadDir(HostDIRType* dirP)

	CALLED_SETUP_HC ("HostDIRType*", "HostDIRType* dirP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (emuptr, dirP);

	MyDIR*	dir = (MyDIR*) (emuptr) dirP;

	// Make sure dir is valid.

	vector<MyDIR*>::iterator	iter = gOpenDirs.begin ();
	while (iter != gOpenDirs.end ())
	{
		// It appears to be on our list, so let's use it.
		if (*iter == dir)
			goto FoundIt;

		++iter;
	}

	// hostDir was not on our list.  Return NULL.

	PUT_RESULT_VAL (emuptr, EmMemNULL);
	return;

FoundIt:

	// Initialize our result in case of failure.

	gHostDirEnt.d_name[0] = 0;

	// Get the next dir or file, as the case may be.

	switch (dir->fState)
	{
		// Just starting out.  Initialize the directory iterator
		// and fall through to the code that uses it.

		case 0:
			dir->fDirIter = dir->fDirs.begin ();
			dir->fState = 1;
			// Fall thru

		// Iterating over directories; get the next one.  If there
		// are no more, start iterating over files.

		case 1:
			if (dir->fDirIter != dir->fDirs.end ())
			{
				strcpy (gHostDirEnt.d_name, dir->fDirIter->GetName().c_str());
				++dir->fDirIter;
				break;
			}

			dir->fFileIter = dir->fFiles.begin ();
			dir->fState = 2;
			// Fall thru

		// Iterating over files; get the next one.  If there
		// are no more, stop iterating.

		case 2:
			if (dir->fFileIter != dir->fFiles.end ())
			{
				strcpy (gHostDirEnt.d_name, dir->fFileIter->GetName().c_str());
				++dir->fFileIter;
				break;
			}
			dir->fState = 3;

		// No longer iterating. Just return NULL.

		case 3:
			PUT_RESULT_VAL (emuptr, EmMemNULL);
			break;
	}

	// If we're returning a value in gHostDirEnt, make
	// sure it's mapped into emulated space.

	if (gHostDirEnt.d_name[0] != 0)
	{
		emuptr	result = EmBankMapped::GetEmulatedAddress (&gHostDirEnt);
		if (result == EmMemNULL)
		{
			EmBankMapped::MapPhysicalMemory (&gHostDirEnt, sizeof (gHostDirEnt));
			result = EmBankMapped::GetEmulatedAddress (&gHostDirEnt);
		}

		PUT_RESULT_VAL (emuptr, result);
	}
}


// ---------------------------------------------------------------------------
//		¥ _HostCloseDir
// ---------------------------------------------------------------------------

static void _HostCloseDir (void)
{
	// long HostCloseDir(HostDIRType*)

	CALLED_SETUP_HC ("long", "HostDIRType* dirP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (emuptr, dirP);

	MyDIR*	dir = (MyDIR*) (emuptr) dirP;

	// Make sure dir is valid.

	vector<MyDIR*>::iterator	iter = gOpenDirs.begin ();
	while (iter != gOpenDirs.end ())
	{
		if (*iter == dir)
		{
			// It's on our list.  Remove it from the list and delete it.

			gOpenDirs.erase (iter);
			delete dir;
			break;
		}

		++iter;
	}

	// Unmap any mapped gHostDirEnt if we're not iterating over any
	// other directories.

	if (gOpenDirs.size () == 0)
	{
		EmBankMapped::UnmapPhysicalMemory (&gHostDirEnt);
	}

	// Return no error (should we return an error if DIR was not found?)

	PUT_RESULT_VAL (long, 0);
}


// ---------------------------------------------------------------------------
//		¥ _HostStat
// ---------------------------------------------------------------------------

static void _HostStat(void)
{
	// long	HostStat(const char*, HostStatType*)

	CALLED_SETUP_HC ("long", "const char* nameP, HostStatType* statP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);
	CALLED_GET_PARAM_REF (HostStatType, statP, Marshal::kOutput);

	// Check the parameters.

	if (nameP == EmMemNULL || statP == EmMemNULL)
	{
		PUT_RESULT_VAL (long, hostErrInvalidParameter);
		return;
	}

	_stat_	local_stat;
	int	result = _stat (nameP, &local_stat);

	if (result == 0)
	{
		(*statP).st_dev_		= local_stat.st_dev;
		(*statP).st_ino_		= local_stat.st_ino;
		(*statP).st_mode_		= local_stat.st_mode;
		(*statP).st_nlink_		= local_stat.st_nlink;
		(*statP).st_uid_		= local_stat.st_uid;
		(*statP).st_gid_		= local_stat.st_gid;
		(*statP).st_rdev_		= local_stat.st_rdev;
		(*statP).st_atime_		= local_stat.st_atime;
		(*statP).st_mtime_		= local_stat.st_mtime;
		(*statP).st_ctime_		= local_stat.st_ctime;
		(*statP).st_size_		= local_stat.st_size;
#if PLATFORM_WINDOWS
		(*statP).st_blksize_	= 0;
		(*statP).st_blocks_		= 0;
		(*statP).st_flags_		= 0;
#elif PLATFORM_MAC
		(*statP).st_blksize_	= local_stat.st_blksize;
		(*statP).st_blocks_		= local_stat.st_blocks;
		(*statP).st_flags_		= local_stat.st_flags;
#else
		(*statP).st_blksize_	= local_stat.st_blksize;
		(*statP).st_blocks_		= local_stat.st_blocks;
		(*statP).st_flags_		= 0;
#endif

		CALLED_PUT_PARAM_REF (statP);
	}

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostTruncate
// ---------------------------------------------------------------------------

static void _HostTruncate (void)
{
	// long HostTruncate(const char*, long)

	CALLED_SETUP_HC ("long", "char* nameP, long size");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);
	CALLED_GET_PARAM_VAL (long, size);

	// Check the parameters.

	if (nameP == EmMemNULL || size < 0)
	{
		PUT_RESULT_VAL (long, hostErrInvalidParameter);
		return;
	}

	// Make the call

	int	result;

	int	fd = _open (nameP, _O_RDWR);
	if (fd)
	{
		result = _chsize (fd, size);
		_close (fd);
	}
	else
	{
		result = errno;
	}

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostUTime
// ---------------------------------------------------------------------------

static void _HostUTime (void)
{
	// long HostUTime (const char*, HostUTimeType*)

	CALLED_SETUP_HC ("long", "char* nameP, HostUTimeType* timeP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);
	CALLED_GET_PARAM_REF (HostUTimeType, timeP, Marshal::kOutput);

	if (nameP == EmMemNULL || timeP == EmMemNULL)
	{
		PUT_RESULT_VAL (long, -1);
		errno = hostErrInvalidParameter;
		return;
	}

	_utimbuf_	buf;
	_utimbuf_*	bufP = NULL;

	if (timeP)
	{
//		buf.crtime	= (*timeP).crtime_;
		buf.actime	= (*timeP).actime_;
		buf.modtime	= (*timeP).modtime_;

		bufP = &buf;
	}

	// Make the call

	int	result = _utime (nameP, bufP);

	// Return the result.

	PUT_RESULT_VAL (long, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostGetFileAttr
// ---------------------------------------------------------------------------

static void _HostGetFileAttr(void)
{
	// long	HostGetFileAttr(const char*, unsigned long * attr)

	CALLED_SETUP_HC ("long", "char* nameP, unsigned long * attrP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);
	CALLED_GET_PARAM_REF (unsigned long, attrP, Marshal::kOutput);

	// Check the parameters.

	if (nameP == EmMemNULL || attrP == EmMemNULL)
	{
		PUT_RESULT_VAL (long, hostErrInvalidParameter);
		return;
	}

	// Make the call

	EmFileRef fRef (nameP);

	int attr = 0;
	errno = fRef.GetAttr (&attr);

	// Return the result.

	if (errno != 0)
	{
		PUT_RESULT_VAL (long, -1);
		return;
	}

	*attrP = attr;
	CALLED_PUT_PARAM_REF (attrP);

	PUT_RESULT_VAL (long, hostErrNone);
}


// ---------------------------------------------------------------------------
//		¥ _HostSetFileAttr
// ---------------------------------------------------------------------------

static void _HostSetFileAttr(void)
{
	// long	HostSetFileAttr(const char*, unsigned long attr)

	CALLED_SETUP_HC ("long", "char* nameP, unsigned long attr");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, nameP);
	CALLED_GET_PARAM_VAL (unsigned long, attr);

	// Check the parameters.

	if (nameP == EmMemNULL)
	{
		PUT_RESULT_VAL (long, hostErrInvalidParameter);
		return;
	}
	
	// Make the call

	EmFileRef fRef(nameP);
	
	errno = fRef.SetAttr(attr);
	
	// Return the result.

	if (errno != 0)
	{
		PUT_RESULT_VAL (long, -1);
		return;
	}

	PUT_RESULT_VAL (long, hostErrNone);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostGremlinIsRunning
// ---------------------------------------------------------------------------

static void _HostGremlinIsRunning (void)
{
	// HostBoolType HostGremlinIsRunning (void)

	CALLED_SETUP_HC ("HostBoolType", "void");

	PUT_RESULT_VAL (HostBoolType, (Hordes::IsOn () ? 1 : 0));
}


// ---------------------------------------------------------------------------
//		¥ _HostGremlinNumber
// ---------------------------------------------------------------------------

static void _HostGremlinNumber (void)
{
	// long HostGremlinNumber (void)

	CALLED_SETUP_HC ("long", "void");

	PUT_RESULT_VAL (long, Hordes::GremlinNumber ());
}


// ---------------------------------------------------------------------------
//		¥ _HostGremlinCounter
// ---------------------------------------------------------------------------

static void _HostGremlinCounter (void)
{
	// long HostGremlinCounter (void)

	CALLED_SETUP_HC ("long", "void");

	PUT_RESULT_VAL (long, Hordes::EventCounter ());
}


// ---------------------------------------------------------------------------
//		¥ _HostGremlinLimit
// ---------------------------------------------------------------------------

static void _HostGremlinLimit (void)
{
	// long HostGremlinLimit (void)

	CALLED_SETUP_HC ("long", "void");

	PUT_RESULT_VAL (long, Hordes::EventLimit ());
}


// ---------------------------------------------------------------------------
//		¥ _HostGremlinNew
// ---------------------------------------------------------------------------

static void _HostGremlinNew (void)
{
	// HostErrType HostGremlinNew (const HostGremlinInfoType*)

	CALLED_SETUP_HC ("HostErrType", "const HostGremlinInfoType* infoP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_REF (HostGremlinInfoType, infoP, Marshal::kInput);

	// Get the easy parts.

	HordeInfo	info;

	info.fStartNumber		= (*infoP).fFirstGremlin;
	info.fStopNumber		= (*infoP).fLastGremlin;
	info.fSaveFrequency		= (*infoP).fSaveFrequency;
	info.fSwitchDepth		= (*infoP).fSwitchDepth;
	info.fMaxDepth 			= (*infoP).fMaxDepth;

	info.OldToNew ();	// Transfer the old fields to the new fields.

	// Get the list of installed applications so that we can compare it to
	// the list of applications requested by the caller.

	DatabaseInfoList	installedAppList;
	::GetDatabases (installedAppList, kApplicationsOnly);

	// Get the list of applications requested by the caller.  Break up the
	// string into a list of individual names, and check to see if the whole
	// thing was preceded by a '-'.

	string		appNames ((*infoP).fAppNames);

	Bool		exclude = false;
	if (appNames[0] == '-')
	{
		exclude = true;
		appNames = appNames.substr(1);
	}

	StringList	requestedAppList;
	::SeparateList (requestedAppList, appNames, ',');

	// For each requested application, see if it's installed in the device.
	// If so, add it to the list of applications Gremlins should be run on.

	StringList::iterator	iter1 = requestedAppList.begin();
	while (iter1 != requestedAppList.end())
	{
		// Get the application info based on the given name.

		DatabaseInfoList::iterator	iter2 = installedAppList.begin ();
		while (iter2 != installedAppList.end ())
		{
			Bool	addIt;

			if (exclude)
				addIt = *iter1 != iter2->name;
			else
				addIt = *iter1 == iter2->name;

			if (addIt)
			{
				info.fAppList.push_back (*iter2);
			}

			++iter2;
		}

		++iter1;
	}

	// Start up the gremlin sub-system.

	EmAssert (gDocument);
	gDocument->ScheduleNewHorde (info);

	// Return the result.

	PUT_RESULT_VAL (HostErrType, errNone);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostImportFile
// ---------------------------------------------------------------------------

static void _HostImportFile (void)
{
	// HostErrType HostImportFile (const char* fileName, long cardNum)

	CALLED_SETUP_HC ("HostErrType", "const char* fileName, long cardNum");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, fileName);
//	CALLED_GET_PARAM_VAL (long, cardNum);

	// Check the parameters.

	if (fileName == NULL)
	{
		PUT_RESULT_VAL (HostErrType, hostErrInvalidParameter);
		return;
	}

	// Make the call

	ErrCode result = kError_NoError;
	try
	{
		EmFileRef		fileRef (fileName);
		EmFileRefList	fileList;
		fileList.push_back (fileRef);

		vector<LocalID> idList;
		EmFileImport::LoadPalmFileList (fileList, kMethodBest, idList);
	}
	catch (ErrCode errCode)
	{
		result = errCode;
	}

	// Return the result.

	PUT_RESULT_VAL (HostErrType, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostImportFileWithID
// ---------------------------------------------------------------------------

static void _HostImportFileWithID (void)
{
	// HostErrType HostImportFileWithID (const char* fileName, long cardNum, LocalID *newIDP)

	CALLED_SETUP_HC ("HostErrType", "const char* fileName, long cardNum, LocalID *newIDP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, fileName);
//	CALLED_GET_PARAM_VAL (long, cardNum);
	CALLED_GET_PARAM_REF (LocalID, newIDP, Marshal::kOutput);

	// Check the parameters.

	if (fileName == NULL)
	{
		PUT_RESULT_VAL (HostErrType, hostErrInvalidParameter);
		return;
	}

	// Make the call

	ErrCode result = kError_NoError;
	*newIDP = 0;

	try
	{
		EmFileRef		fileRef (fileName);
		EmFileRefList	fileList;
		fileList.push_back (fileRef);

		vector<LocalID> idList;
		EmFileImport::LoadPalmFileList (fileList, kMethodBest, idList);

		EmAssert (idList.size () == fileList.size ());

		*newIDP = idList.front ();
	}
	catch (ErrCode errCode)
	{
		result = errCode;
	}

	// Return the result.

	CALLED_PUT_PARAM_REF(newIDP);
	PUT_RESULT_VAL (HostErrType, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExportFile
// ---------------------------------------------------------------------------

static void _HostExportFile (void)
{
	// HostErrType HostExportFile (const char* dbNameP, long cardNum, const char* fileNameP)

	CALLED_SETUP_HC ("HostErrType", "const char* fileName, long cardNum, const char* dbName");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, fileName);
	CALLED_GET_PARAM_VAL (long, cardNum);
	CALLED_GET_PARAM_STR (char, dbName);

	// Check the parameters.

	if (fileName == NULL || dbName == NULL)
	{
		PUT_RESULT_VAL (HostErrType, hostErrInvalidParameter);
		return;
	}

	// Make the call

	ErrCode result = kError_NoError;
	try
	{
		EmFileRef		ref (fileName);
		EmStreamFile	stream (ref, kCreateOrEraseForUpdate,
								kFileCreatorInstaller, kFileTypePalmApp);
		::SavePalmFile (stream, cardNum, dbName);
	}
	catch (ErrCode errCode)
	{
		result = errCode;
	}

	// Return the result.

	PUT_RESULT_VAL (HostErrType, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostSaveScreen
// ---------------------------------------------------------------------------

static void _HostSaveScreen (void)
{
	// HostErrType HostSaveScreen (const char* fileNameP)

	CALLED_SETUP_HC ("HostErrType", "char* fileNameP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, fileNameP);

	// Check the parameters.

	if (fileNameP == NULL)
	{
		PUT_RESULT_VAL (HostErrType, hostErrInvalidParameter);
		return;
	}

	// Make the call

	ErrCode result = kError_NoError;
	try
	{
		EmFileRef	ref (fileNameP);

		EmAssert (gDocument);
		gDocument->HostSaveScreen (ref);
	}
	catch (ErrCode errCode)
	{
		result = errCode;
	}

	// Return the result.

	PUT_RESULT_VAL (HostErrType, result);
}


#pragma mark -

#define exgErrBadData			(exgErrorClass | 8)  // internal data was not valid

// ---------------------------------------------------------------------------
//		¥ _HostExgLibOpen
// ---------------------------------------------------------------------------

static void _HostExgLibOpen (void)
{
	// Err ExgLibOpen (UInt16 libRefNum)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibOpen (libRefNum);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibClose
// ---------------------------------------------------------------------------

static void _HostExgLibClose (void)
{
	// Err ExgLibClose (UInt16 libRefNum)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibClose (libRefNum);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibSleep
// ---------------------------------------------------------------------------

static void _HostExgLibSleep (void)
{
	// Err ExgLibSleep (UInt16 libRefNum)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibSleep (libRefNum);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibWake
// ---------------------------------------------------------------------------

static void _HostExgLibWake (void)
{
	// Err ExgLibWake (UInt16 libRefNum)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibWake (libRefNum);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibHandleEvent
// ---------------------------------------------------------------------------

static void _HostExgLibHandleEvent (void)
{
	// Boolean ExgLibHandleEvent (UInt16 libRefNum, void* eventP)

	CALLED_SETUP_HC ("Boolean", "UInt16 libRefNum, void* eventP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, eventP);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Boolean	result = exgMgr->ExgLibHandleEvent (libRefNum, eventP);

	// Return the result.

	PUT_RESULT_VAL (Boolean, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibConnect
// ---------------------------------------------------------------------------

static void _HostExgLibConnect (void)
{
	// Err ExgLibConnect (UInt16 libRefNum, void* exgSocketP)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum, void* exgSocketP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, exgSocketP);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibConnect (libRefNum, exgSocketP);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibAccept
// ---------------------------------------------------------------------------

static void _HostExgLibAccept (void)
{
	// Err ExgLibAccept (UInt16 libRefNum, void* exgSocketP)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum, void* exgSocketP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, exgSocketP);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibAccept (libRefNum, exgSocketP);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibDisconnect
// ---------------------------------------------------------------------------

static void _HostExgLibDisconnect (void)
{
	// Err ExgLibDisconnect (UInt16 libRefNum, void* exgSocketP, Err error)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum, void* exgSocketP, Err error");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, exgSocketP);
	CALLED_GET_PARAM_VAL (Err, error);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibDisconnect (libRefNum, exgSocketP, error);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibPut
// ---------------------------------------------------------------------------

static void _HostExgLibPut (void)
{
	// Err ExgLibPut (UInt16 libRefNum, void* exgSocketP)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum, void* exgSocketP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, exgSocketP);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibPut (libRefNum, exgSocketP);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibGet
// ---------------------------------------------------------------------------

static void _HostExgLibGet (void)
{
	// Err ExgLibGet (UInt16 libRefNum, void* exgSocketP)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum, void* exgSocketP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, exgSocketP);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibGet (libRefNum, exgSocketP);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibSend
// ---------------------------------------------------------------------------

static void _HostExgLibSend (void)
{
	// UInt32 ExgLibSend (UInt16 libRefNum, void* exgSocketP,
	//			const void* const bufP, const UInt32 bufLen, Err* errP)

	CALLED_SETUP_HC ("UInt32", "UInt16 libRefNum, void* exgSocketP,"
				"const void* const bufP, const UInt32 bufLen, Err* errP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, exgSocketP);
	CALLED_GET_PARAM_VAL (UInt32, bufLen);
	CALLED_GET_PARAM_PTR (void, bufP, bufLen, Marshal::kInput);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		*errP = exgErrBadData;
		PUT_RESULT_VAL (UInt32, 0);
		return;
	}

	// Make the call

	UInt32	result = exgMgr->ExgLibSend (libRefNum, exgSocketP, bufP, bufLen, errP);

	// Return the result.

	CALLED_PUT_PARAM_REF (errP);

	PUT_RESULT_VAL (UInt32, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibReceive
// ---------------------------------------------------------------------------

static void _HostExgLibReceive (void)
{
	// UInt32 ExgLibReceive (UInt16 libRefNum, void* exgSocketP,
	//			void* bufP, const UInt32 bufLen, Err* errP)

	CALLED_SETUP_HC ("UInt32", "UInt16 libRefNum, void* exgSocketP,"
				"const void* const bufP, const UInt32 bufLen, Err* errP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, exgSocketP);
	CALLED_GET_PARAM_VAL (UInt32, bufLen);
	CALLED_GET_PARAM_PTR (void, bufP, bufLen, Marshal::kOutput);
	CALLED_GET_PARAM_REF (Err, errP, Marshal::kOutput);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		*errP = exgErrBadData;
		PUT_RESULT_VAL (UInt32, 0);
		return;
	}

	// Make the call

	UInt32	result = exgMgr->ExgLibReceive (libRefNum, exgSocketP, bufP, bufLen, errP);

	// Return the result.

	CALLED_PUT_PARAM_REF (bufP);
	CALLED_PUT_PARAM_REF (errP);

	PUT_RESULT_VAL (UInt32, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibControl
// ---------------------------------------------------------------------------

static void _HostExgLibControl (void)
{
	// Err ExgLibControl (UInt16 libRefNum, UInt16 op,
	//							 void* valueP, UInt16* valueLenP)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum, UInt16 op, void* valueP, UInt16* valueLenP");

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (UInt16, op);
	CALLED_GET_PARAM_VAL (emuptr, valueP);
	CALLED_GET_PARAM_VAL (emuptr, valueLenP);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibControl (libRefNum, op, valueP, valueLenP);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


// ---------------------------------------------------------------------------
//		¥ _HostExgLibRequest
// ---------------------------------------------------------------------------

static void _HostExgLibRequest (void)
{
	// Err ExgLibRequest (UInt16 libRefNum, void* exgSocketP)

	CALLED_SETUP_HC ("Err", "UInt16 libRefNum, void* exgSocketP");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, libRefNum);
	CALLED_GET_PARAM_VAL (emuptr, exgSocketP);

	// Check the parameters.

	EmExgMgr*	exgMgr	= EmExgMgr::GetExgMgr (libRefNum);
	if (!exgMgr)
	{
		PUT_RESULT_VAL (Err, exgErrBadData);
		return;
	}

	// Make the call

	Err	result = exgMgr->ExgLibRequest (libRefNum, exgSocketP);

	// Return the result.

	PUT_RESULT_VAL (Err, result);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostGetPreference
// ---------------------------------------------------------------------------

static void _HostGetPreference (void)
{
	// HostBoolType HostGetPreference (const char* key, char* value);

	CALLED_SETUP_HC ("HostBoolType", "const char* key, char* value");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, key);
	CALLED_GET_PARAM_VAL (emuptr, value);

	string	keyStr ((const char*) key);

	if (keyStr == "ReportIntlStrictChecks")
	{
		keyStr = "ReportStrictIntlChecks";
	}

	Preference<string>	pref (keyStr.c_str ());

	PUT_RESULT_VAL (HostBoolType, 0);

	if (pref.Loaded ())
	{
		EmMem_strcpy ((emuptr) value, pref->c_str ());
		PUT_RESULT_VAL (HostBoolType, 1);
	}
}


// ---------------------------------------------------------------------------
//		¥ _HostSet1Preference
// ---------------------------------------------------------------------------

static void _HostSetPreference (void)
{
	// void HostSetPreference (const char*, const char*);

	CALLED_SETUP_HC ("HostBoolType", "const char* key, char* value");

	// Get the caller's parameters.

	CALLED_GET_PARAM_STR (char, key);
	CALLED_GET_PARAM_STR (char, value);

	string	keyStr ((const char*) key);

	if (keyStr == "ReportIntlStrictChecks")
	{
		keyStr = "ReportStrictIntlChecks";
	}

	Preference<string>	pref (keyStr.c_str ());
	pref = string (value);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostLogFile
// ---------------------------------------------------------------------------

static void _HostLogFile (void)
{
	// HostFILEType* HostLogFile (void)

	CALLED_SETUP_HC ("HostFILEType", "void");

	PUT_RESULT_VAL (emuptr, hostLogFile);
}


// ---------------------------------------------------------------------------
//		¥ _HostSetLogFileSize
// ---------------------------------------------------------------------------

static void _HostSetLogFileSize (void)
{
	// void HostSetLogFileSize (long newSize)

	CALLED_SETUP_HC ("void", "long newSize");

	CALLED_GET_PARAM_VAL (long, newSize);

	Preference<long>	logFileSize (kPrefKeyLogFileSize);
	logFileSize = (long) newSize;
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostSessionCreate
// ---------------------------------------------------------------------------

#if 0
static void _HostSessionCreate (void)
{
	// HostErrType HostSessionCreate(const char* device, long ramSize, const char* romPath)

	struct StackFrame
	{
		const char*	device;
		long		ramSize;
		const char*	romPath;
	};

	string		deviceStr	= ToString (GET_PARAMETER (device));
	RAMSizeType	ramSize		= GET_PARAMETER (ramSize);
	string		romPathStr	= ToString (GET_PARAMETER (romPath));

	// See if it's OK to create a session.

	if (Platform::SessionRunning())
	{
		PUT_RESULT_VAL (HostErrType, hostErrSessionNotRunning);
		return;
	}

	// Convert the device string into a DeviceType

	DeviceType						device = kDeviceUnspecified;
	EmDeviceMenuItemList			devices = EmDevice::GetMenuItemList ();
	EmDeviceMenuItemList::iterator	deviceIter = devices.begin();

	while (deviceIter != devices.end())
	{
		if (_stricmp (deviceStr.c_str(), deviceIter->second.c_str()) == 0)
		{
			device = deviceIter->first;
			break;
		}
	}

	if (device == kDeviceUnspecified)
	{
		PUT_RESULT_VAL (HostErrType, hostErrInvalidDeviceType);
		return;
	}

	// Validate the RAM size

	Bool			sizeOK = false;
	MemoryTextList	sizes;
	::GetMemoryTextList (sizes);

	MemoryTextList::iterator	sizeIter = sizes.begin();
	while (sizeIter != sizes.end())
	{
		if (ramSize == sizeIter->first)
		{
			sizeOK = true;
			break;
		}
	}

	if (!sizeOK)
	{
		PUT_RESULT_VAL (HostErrType, hostErrInvalidRAMSize);
		return;
	}

	// Convert the ROM file string into a EmFileRef.

	EmFileRef	romRef(romPathStr);
	if (!romRef.Exists())
	{
		PUT_RESULT_VAL (HostErrType, hostErrFileNotFound);
		return;
	}

	// Kick this all off.

	Configuration	cfg (device, ramSize, romRef);
	EmAssert (gApplication);
	gApplication->ScheduleCreateSession (cfg);

	PUT_RESULT_VAL (HostErrType, errNone);
}
#endif


// ---------------------------------------------------------------------------
//		¥ _HostSessionOpen
// ---------------------------------------------------------------------------

#if 0
static void _HostSessionOpen (void)
{
	// HostErrType HostSessionOpen (const char* psfFileName);

	struct StackFrame
	{
		const char*	psfFileName;
	};

	string	psfFileName	= ToString (GET_PARAMETER (psfFileName));

	// See if it's OK to open a session.

	if (Platform::SessionRunning())
	{
		PUT_RESULT_VAL (HostErrType, hostErrSessionNotRunning);
		return;
	}

	// Validate the file to open.

	EmFileRef	psfFileRef(psfFileName);
	if (!psfFileRef.Exists())
	{
		PUT_RESULT_VAL (HostErrType, hostErrFileNotFound);
		return;
	}

	// Kick this all off.

	EmAssert (gApplication);
	gApplication->ScheduleOpenSession (psfFileRef);

	PUT_RESULT_VAL (HostErrType, errNone);
}
#endif


// ---------------------------------------------------------------------------
//		¥ _HostSessionSave
// ---------------------------------------------------------------------------

static void _HostSessionSave (void)
{
	// HostBoolType HostSessionSave (const char* saveFileName)

	CALLED_SETUP_HC ("HostBoolType", "const char* saveFileName");

	CALLED_GET_PARAM_STR (char, saveFileName);

	// See if it's OK to close a session.

	if (!gSession)
	{
		// !!! Oops. Returning an error code as a Boolean...
		PUT_RESULT_VAL (HostBoolType, hostErrSessionNotRunning);
		return;
	}

	EmFileRef	saveFileRef (saveFileName);

	// Saving will save the PC at the current instruction, which saves the state.  
	// Upon resumption, the current instruction will cause a save again.  This 
	// will repeat infinitely.  So advance the PC to the following instruction
	// before saving.

	// First, get the address of the function that got us here.
	// If the system call is being made by a TRAP $F, the PC has already
	// been bumped past the opcode.  If being made with a JSR via the
	// SYS_TRAP_FAST macro, the PC has not been adjusted.
	//
	// !!! Note that the following is 68K-specific!

	emuptr	startPC = gCPU->GetPC ();
	if ((EmMemGet16 (startPC) & 0xF000) == 0xA000)
	{
		startPC -= 2;	// Back us up to the TRAP $F
	}

	SystemCallContext	context;
	if (GetSystemCallContext (startPC, context))
	{
		gCPU->SetPC (context.fNextPC);

		// Set the return value to something else so that the code that is restored can distinguish
		// the saved case from not saved case.

		PUT_RESULT_VAL (HostBoolType, true);

		gSession->Save (saveFileRef, false);
	}

	PUT_RESULT_VAL (HostBoolType, false);
}


// ---------------------------------------------------------------------------
//		¥ _HostSessionClose
// ---------------------------------------------------------------------------

static void _HostSessionClose (void)
{
	// HostErrType HostSessionClose (const char* saveFileName)

	CALLED_SETUP_HC ("HostErrType", "const char* saveFileName");

	CALLED_GET_PARAM_STR (char, saveFileName);

	// See if it's OK to close a session.

	if (!gSession)
	{
		PUT_RESULT_VAL (HostErrType, hostErrSessionNotRunning);
		return;
	}

	EmFileRef	saveFileRef(saveFileName);

	// Kick this all off.

	gApplication->ScheduleSessionClose (saveFileRef);

	// Return the result.

	PUT_RESULT_VAL (HostErrType, errNone);
}


// ---------------------------------------------------------------------------
//		¥ _HostSessionQuit
// ---------------------------------------------------------------------------

static void _HostSessionQuit (void)
{
	// HostErrType HostSessionQuit (void)

	CALLED_SETUP_HC ("HostErrType", "void");

	// See if it's OK to quit Poser.

	if (gSession)
	{
//		PUT_RESULT_VAL (HostErrType, hostErrSessionRunning);
//		return;
	}

	// Kick this all off.

	gApplication->ScheduleQuit ();

	// Return the result.

	PUT_RESULT_VAL (HostErrType, errNone);
}


// ---------------------------------------------------------------------------
//		¥ _HostSignalSend
// ---------------------------------------------------------------------------
// Called by anyone wanting to send a signal to any waiting scripts.

static void _HostSignalSend (void)
{
	// HostErrType HostSignalSend (HostSignalType signalNumber)

	CALLED_SETUP_HC ("HostErrType", "HostSignalType signalNumber");

	CALLED_GET_PARAM_VAL (HostSignalType, signalNumber);

	RPC::SignalWaiters (signalNumber);

	// Return the result.

	PUT_RESULT_VAL (HostErrType, errNone);
}


// ---------------------------------------------------------------------------
//		¥ _HostSignalWait
// ---------------------------------------------------------------------------
// Called by scripts that want to get a signal sent from HostSignalSend.

static void _HostSignalWait (void)
{
	// HostErrType HostSignalWait (long timeout)

	CALLED_SETUP_HC ("HostErrType", "long timeout");

	CALLED_GET_PARAM_VAL (long, timeout);

	// Unblock the CPU thread if it's suspended from a previous
	// HostSignalSend call.

	EmAssert (gSession);
	gSession->ScheduleResumeExternal ();

	if (RPC::HandlingPacket ())
	{
		RPC::DeferCurrentPacket (timeout);

		if (EmPatchState::UIInitialized ())
		{
			::EvtWakeup ();	// Wake up the process in case the caller is looking
							// for an idle event (which would never otherwise
							// happen if EvtGetEvent has already been called and
							// is blocking).
		}
	}

	// Return the result.

	PUT_RESULT_VAL (HostErrType, errNone);
}


// ---------------------------------------------------------------------------
//		¥ _HostSignalResume
// ---------------------------------------------------------------------------
// Called by scripts to restart the emulator after it has sent a signal and
// then suspended itself.

static void _HostSignalResume (void)
{
	// HostErrType HostSignalResume (void)

	CALLED_SETUP_HC ("HostErrType", "void");

	EmAssert (gSession);
	gSession->ScheduleResumeExternal ();

	// Return the result.

	PUT_RESULT_VAL (HostErrType, errNone);
}


#pragma mark -

#if HAS_TRACER
// ---------------------------------------------------------------------------
//		¥ _HostTraceInit
// ---------------------------------------------------------------------------

static void _HostTraceInit (void)
{	
	gTracer.InitOutputPort ();
}


// ---------------------------------------------------------------------------
//		¥ _HostTraceClose
// ---------------------------------------------------------------------------

static void _HostTraceClose (void)
{
	gTracer.CloseOutputPort ();
}


// ---------------------------------------------------------------------------
//		¥ _HostTraceOutputT
// ---------------------------------------------------------------------------

static void _HostTraceOutputT (void)
{
	// void HostTraceOutputT (unsigned short, const char*, ...)

	CALLED_SETUP_STDARG_HC ("void", "UInt16 module, const char* fmt");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, module);
	CALLED_GET_PARAM_STR (char, fmt);

	// Check the parameters.

	if (fmt == NULL)
	{
		errno = hostErrInvalidParameter;
		return;
	}

	// Collect the specified parameters. We need to make copies of everything
	// so that it's in the right endian order and to reverse any effects
	// of wordswapping.  The values specified on the stack (the integers,
	// chars, doubles, pointers, etc.) get converted and placed in stackData.
	// The data pointed to by the pointers gets converted and placed in stringData.

	ByteList	stackData;
	StringList stringData;

	if (!::PrvCollectParameters (sub, string (fmt), stackData, stringData))
	{
		errno = hostErrInvalidParameter;
		return;
	}

	// Write everything out

	gTracer.OutputVT( module, fmt, (va_list) &stackData[0]);
}


// ---------------------------------------------------------------------------
//		¥ _HostTraceOutputTL
// ---------------------------------------------------------------------------

static void _HostTraceOutputTL (void)
{
	// void HostTraceOutputTL (unsigned short, const char*, ...)

	CALLED_SETUP_STDARG_HC ("void", "UInt16 module, const char* fmt");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, module);
	CALLED_GET_PARAM_STR (char, fmt);

	// Check the parameters.

	if (fmt == NULL)
	{
		errno = hostErrInvalidParameter;
		return;
	}

	// Collect the specified parameters. We need to make copies of everything
	// so that it's in the right endian order and to reverse any effects
	// of wordswapping.  The values specified on the stack (the integers,
	// chars, doubles, pointers, etc.) get converted and placed in stackData.
	// The data pointed to by the pointers gets converted and placed in stringData.

	ByteList	stackData;
	StringList stringData;

	if (!::PrvCollectParameters (sub, string (fmt), stackData, stringData))
	{
		errno = hostErrInvalidParameter;
		return;
	}

	// Write everything out

	gTracer.OutputVTL( module, fmt, (va_list) &stackData[0]);
}


// ---------------------------------------------------------------------------
//		¥ _HostOutputVT
// ---------------------------------------------------------------------------

static void _HostTraceOutputVT (void)
{
	// void HostTraceOutputVT (unsigned short module, const char* fmt, char* va_addr)

	CALLED_SETUP_STDARG_HC ("void", "UInt16 module, const char* fmt, const char* va_addr");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, module);
	CALLED_GET_PARAM_STR (char, fmt);
	CALLED_GET_PARAM_VAL (emuptr, va_addr);

	// Check the parameters.

	if (fmt == NULL)
	{
		errno = hostErrInvalidParameter;
		return;
	}

	// We get the parameters from va_addr, not the stack, so create a new
	// EmSubroutine object to access them.

	EmSubroutine	sub2;
	sub2.PrepareStack (va_addr);

	// Collect the specified parameters. We need to make copies of everything
	// so that it's in the right endian order and to reverse any effects
	// of wordswapping.  The values specified on the stack (the integers,
	// chars, doubles, pointers, etc.) get converted and placed in stackData.
	// The data pointed to by the pointers gets converted and placed in stringData.

	ByteList	stackData;
	StringList	stringData;

	if (!::PrvCollectParameters (sub2, string (fmt), stackData, stringData))
	{
		errno = hostErrInvalidParameter;
		return;
	}

	// Write everything out

	gTracer.OutputVT( module, fmt, (va_list) &stackData[0]);
}


// ---------------------------------------------------------------------------
//		¥ _HostOutputVTL
// ---------------------------------------------------------------------------

static void _HostTraceOutputVTL (void)
{
	// void HostTraceOutputVTL (unsigned short module, const char* fmt, char* va_addr)

	CALLED_SETUP_STDARG_HC ("void", "UInt16 module, const char* fmt, const char* va_addr");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, module);
	CALLED_GET_PARAM_STR (char, fmt);
	CALLED_GET_PARAM_VAL (emuptr, va_addr);

	// Check the parameters.

	if (fmt == NULL)
	{
		errno = hostErrInvalidParameter;
		return;
	}

	// We get the parameters from va_addr, not the stack, so create a new
	// EmSubroutine object to access them.

	EmSubroutine	sub2;
	sub2.PrepareStack (va_addr);

	// Collect the specified parameters. We need to make copies of everything
	// so that it's in the right endian order and to reverse any effects
	// of wordswapping.  The values specified on the stack (the integers,
	// chars, doubles, pointers, etc.) get converted and placed in stackData.
	// The data pointed to by the pointers gets converted and placed in stringData.

	ByteList	stackData;
	StringList	stringData;

	if (!::PrvCollectParameters (sub2, string (fmt), stackData, stringData))
	{
		errno = hostErrInvalidParameter;
		return;
	}

	// Write everything out

	gTracer.OutputVTL( module, fmt, (va_list) &stackData[0]);
}


// ---------------------------------------------------------------------------
//		¥ _HostOutputB
// ---------------------------------------------------------------------------

static void _HostTraceOutputB (void)
{
	// void HostTraceOutputB (unsigned short, const void*, HostSizeType)

	CALLED_SETUP_HC ("void", "UInt16 module, const void* buf, HostSizeType length");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt16, module);
	CALLED_GET_PARAM_VAL (HostSizeType, length);
	CALLED_GET_PARAM_PTR (void, buf, length, Marshal::kInput);

	// Check the parameters.

	if (buf == NULL || length == 0)
	{
		errno = hostErrInvalidParameter;
		return;
	}

	gTracer.OutputB (module, buf, length);
}

#endif	// HAS_TRACER


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostDbgSetDataBreak
// ---------------------------------------------------------------------------

static void _HostDbgSetDataBreak (void)
{
	// HostErr HostDbgSetDataBreak (UInt32 addr, UInt32 size)
	
	CALLED_SETUP_HC ("HostErr", "UInt32 addr, UInt32 size");

	// Get the caller's parameters.

	CALLED_GET_PARAM_VAL (UInt32, addr);
	CALLED_GET_PARAM_VAL (UInt32, size);

	// Check the parameters.

	if (!addr || !size)
	{
		PUT_RESULT_VAL (HostErrType, hostErrInvalidParameter);
		return;
	}

	// Set data breakpoint.

	gDebuggerGlobals.watchEnabled = true;

	if (gDebuggerGlobals.watchEnabled)
	{
		gDebuggerGlobals.watchAddr = addr;
		gDebuggerGlobals.watchBytes = size;
	}

	// Return the result.

	PUT_RESULT_VAL (HostErrType, errNone);
}


// ---------------------------------------------------------------------------
//		¥ _HostDbgClearDataBreak
// ---------------------------------------------------------------------------

static void _HostDbgClearDataBreak (void)
{
	// HostErr HostDbgClearDataBreak (void)

	CALLED_SETUP_HC ("HostErr", "void");

	// Get the caller's parameters.

	// Check the parameters.

	// Set data breakpoint

	gDebuggerGlobals.watchEnabled = false;

	if (!gDebuggerGlobals.watchEnabled)
	{
		gDebuggerGlobals.watchAddr = 0;
		gDebuggerGlobals.watchBytes = 0;
	}

	// Return the result.

	PUT_RESULT_VAL (HostErrType, errNone);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostSlotMax
// ---------------------------------------------------------------------------

static void _HostSlotMax (void)
{
	// long HostSlotMax(void)
        PHEM_Log_Msg("HostSlotMax()");

	CALLED_SETUP_HC ("long", "void");

	Int32	maxSlotNo = 0;

	Preference<SlotInfoList>	slots (kPrefKeySlotList);

	SlotInfoList::const_iterator	iter = (*slots).begin ();
	while (iter != (*slots).end ())
	{
		if (maxSlotNo < iter->fSlotNumber)
		{
			maxSlotNo = iter->fSlotNumber;
		}

		++iter;
	}
        PHEM_Log_Place(maxSlotNo);
	PUT_RESULT_VAL (long, maxSlotNo);
}


// ---------------------------------------------------------------------------
//		¥ _HostSlotRoot
// ---------------------------------------------------------------------------

static void _HostSlotRoot (void)
{
	// const char* HostSlotRoot(long slotNo)
        PHEM_Log_Msg("HostSlotRoot()");

	CALLED_SETUP_HC ("char*", "long slotNo");

	CALLED_GET_PARAM_VAL (long, slotNo);

	PUT_RESULT_VAL (emuptr, EmMemNULL);

	Preference<SlotInfoList>	slots (kPrefKeySlotList);

	SlotInfoList::const_iterator	iter = (*slots).begin ();
	while (iter != (*slots).end ())
	{
		if (slotNo == iter->fSlotNumber)
		{
			if (iter->fSlotOccupied)
			{
                            PHEM_Log_Msg("Root of slot:");
                            PHEM_Log_Msg(iter->fSlotRoot.GetFullPath().c_str());
				::PrvReturnString (iter->fSlotRoot.GetFullPath (), sub);
			}

			break;
		}

		++iter;
	}
}


// ---------------------------------------------------------------------------
//		¥ _HostSlotHasCard
// ---------------------------------------------------------------------------

static void _HostSlotHasCard (void)
{
	// HostBoolType HostSlotHasCard(long slotNo)
        PHEM_Log_Msg("HostSlotHasCard()");

	CALLED_SETUP_HC ("HostBoolType", "long slotNo");

	CALLED_GET_PARAM_VAL (long, slotNo);

	PUT_RESULT_VAL (HostBoolType, false);

	Preference<SlotInfoList>	slots (kPrefKeySlotList);

	SlotInfoList::const_iterator	iter = (*slots).begin ();
	while (iter != (*slots).end ())
	{
		if (slotNo == iter->fSlotNumber)
		{
                        PHEM_Log_Msg("SlotHasCard:");
                        PHEM_Log_Place(iter->fSlotOccupied);
			PUT_RESULT_VAL (HostBoolType, iter->fSlotOccupied);
			break;
		}

		++iter;
	}
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ _HostGetFile
// ---------------------------------------------------------------------------

static void _HostGetFile (void)
{
	// const char* HostGetFile(const char* prompt, const char* defaultDirName)

	CALLED_SETUP_HC ("char*", "const char* prompt, const char* defaultDirName");

	CALLED_GET_PARAM_STR (char, prompt);
	CALLED_GET_PARAM_STR (char, defaultDirName);

	PUT_RESULT_VAL (emuptr, EmMemNULL);

	EmDirRef		defaultDir (defaultDirName);

	EmFileRef		result;
	EmFileTypeList	filterList (1, kFileTypeAll);

	if (EmDlg::DoGetFile (result, string (prompt), defaultDir, filterList) == kDlgItemOK)
	{
		::PrvReturnString (result.GetFullPath (), sub);
	}
}


// ---------------------------------------------------------------------------
//		¥ _HostPutFile
// ---------------------------------------------------------------------------

static void _HostPutFile (void)
{
	// const char* HostPutFile(const char* prompt, const char* defaultDirName, const char* defaultName)

	CALLED_SETUP_HC ("char*", "const char* prompt, const char* defaultDirName, const char* defaultName");

	CALLED_GET_PARAM_STR (char, prompt);
	CALLED_GET_PARAM_STR (char, defaultDirName);
	CALLED_GET_PARAM_STR (char, defaultName);

	PUT_RESULT_VAL (emuptr, EmMemNULL);

	EmDirRef		defaultDir (defaultDirName);

	EmFileRef		result;
	EmFileTypeList	filterList (1, kFileTypeAll);

	if (EmDlg::DoPutFile (result, string (prompt), defaultDir,
							filterList, string (defaultName)) == kDlgItemOK)
	{
		::PrvReturnString (result.GetFullPath (), sub);
	}
}


// ---------------------------------------------------------------------------
//		¥ _HostGetDirectory
// ---------------------------------------------------------------------------

static void _HostGetDirectory (void)
{
	// const char* HostGetDirectory(const char* prompt, const char* defaultDirName)

	CALLED_SETUP_HC ("char*", "const char* prompt, const char* defaultDirName");

	CALLED_GET_PARAM_STR (char, prompt);
	CALLED_GET_PARAM_STR (char, defaultDirName);

	PUT_RESULT_VAL (emuptr, EmMemNULL);

	EmDirRef		defaultDir (defaultDirName);

	EmDirRef		result;

	if (EmDlg::DoGetDirectory (result, string (prompt), defaultDir) == kDlgItemOK)
	{
		::PrvReturnString (result.GetFullPath (), sub);
	}
}

#pragma mark -

// RAI 2013: Add in support for getting something possibly resembling an
// accurate size of HostFS volumes - HostGetVolSize and HostGetVolFree
// Note: Theoretically, a UInt32 could describe 4GB. But in practice,
// not all Palm file-management software actually supports the full
// range. FileZ in particular apparently uses a signed Int32, so it
// maxes out at 2GB.
//
// In practice, 2GB is practically infinite for a Palm anyway.

#define ALMOST_TWO_GB (LONG_MAX-1)
#define TWO_GB_MINUS_1K (LONG_MAX-1024)
// ---------------------------------------------------------------------------
//		¥ _HostGetVolSize
// ---------------------------------------------------------------------------

static void _HostGetVolSize(void)
{
	// long HostGetVolSize(char *path)
        PHEM_Log_Msg("HostGetVolSize:");

	CALLED_SETUP_HC ("long", "const char* path");

	CALLED_GET_PARAM_STR (char, path);

        struct statvfs the_stats;

        PHEM_Log_Msg(path);
        if (statvfs(path, &the_stats)) {
          PHEM_Log_Msg("Host get vol size failure!");
          PUT_RESULT_VAL (UInt32, 0);
        }

        long long vol_size = the_stats.f_frsize * the_stats.f_blocks;

        // Clamp the max at 2GB, all most Palm software will comprehend
        if (vol_size > ALMOST_TWO_GB) {
          vol_size = ALMOST_TWO_GB;
        }
        PHEM_Log_Place((UInt32)vol_size);
        PUT_RESULT_VAL (UInt32, (UInt32)vol_size);
}

// ---------------------------------------------------------------------------
//		¥ _HostGetVolFree
// ---------------------------------------------------------------------------

static void _HostGetVolFree(void)
{
	// long HostGetVolFree(char *path)
        PHEM_Log_Msg("HostGetVolFree:");

	CALLED_SETUP_HC ("long", "const char* path");

	CALLED_GET_PARAM_STR (char, path);

        struct statvfs the_stats;

        PHEM_Log_Msg(path);
        if (statvfs(path, &the_stats)) {
          PHEM_Log_Msg("Host get vol free failure!");
          PUT_RESULT_VAL (long, 0);
        }

        long long vol_free = the_stats.f_bavail * the_stats.f_bsize;

        // Clamp the max at about 2GB, all most Palm software will comprehend
        if (vol_free > TWO_GB_MINUS_1K) {
          vol_free = TWO_GB_MINUS_1K;
        }
        PHEM_Log_Place((UInt32)vol_free);
        PUT_RESULT_VAL (UInt32, (UInt32)vol_free);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ PrvHostGetHandler
// ---------------------------------------------------------------------------

HostHandler PrvHostGetHandler (HostControlSelectorType selector)
{
	HostHandler fn = NULL;

	// Hack for GremlinIsOn; see comments at head of HostTraps.h.

	if ((selector & 0xFF00) == 0)
	{
		selector = hostSelectorGremlinIsRunning;
	}

	if (selector < hostSelectorLastTrapNumber)
	{
		fn = gHandlerTable [selector];
	}

	return fn;
}


// ---------------------------------------------------------------------------
//		¥ PrvCollectParameters
// ---------------------------------------------------------------------------

Bool PrvCollectParameters (EmSubroutine& sub, const string& fmt, ByteList& stackData, StringList& stringData)
{
	// Start parsing up the format string.

	string::const_iterator	iter;

	for (iter = fmt.begin (); iter != fmt.end ();)
	{
		char	ch = *iter++;

		/*
			Format specification:

				% ? 12 .4 h d
				| |  |  | | |
				| |  |  | | +-- conversion letter
				| |  |  | +---- size modifier (l, L, or h)
				| |  |  +------ precision
				| |  +--------- minimum width field
				| +------------ flags (one or more of +, -, #, 0, or space)
				+-------------- start of specification
		*/

		if (ch == '%')
		{
			if (iter == fmt.end ())
				return false;

			ch = *iter++;

			// Skip over any flags.

			while (ch == '+' || ch == '-' || ch == ' ' || ch == '#' || ch == '0')
			{
				if (iter == fmt.end ())
					return false;

				ch = *iter++;
			}

			// Skip over any minimum width field.

			if (ch == '*')
			{
				if (iter == fmt.end ())
					return false;

				ch = *iter++;
			}
			else
			{
				while (ch >= '0' && ch <= '9')
				{
					if (iter == fmt.end ())
						return false;

					ch = *iter++;
				}
			}

			// Skip over any precision.

			if (ch == '.')
			{
				if (iter == fmt.end ())
					return false;

				ch = *iter++;

				while (ch >= '0' && ch <= '9')
				{
					if (iter == fmt.end ())
						return false;

					ch = *iter++;
				}
			}

			// Get any size modifier.

			enum { kSizeNone, kSizeLongInt, kSizeLongDouble, kSizeShortInt };

			int mod = kSizeNone;

			if (ch == 'l')
				mod = kSizeLongInt;
			else if (ch == 'L')
				mod = kSizeLongDouble;
			else if (ch == 'h')
				mod = kSizeShortInt;

			// If there was a modifier, it's been handled,
			// so skip over it.

			if (mod != kSizeNone)
			{
				if (iter == fmt.end ())
					return false;

				ch = *iter++;
			}

			switch (ch)
			{
				case 'd':
				case 'i':
				case 'u':
				case 'o':
				case 'x':
				case 'X':
					// int, short, or long
					if (mod == kSizeNone || mod == kSizeShortInt)
						PrvPushShort (sub, stackData);
					else
						PrvPushLong (sub, stackData);
					break;

				case 'f':
				case 'e':
				case 'E':
				case 'g':
				case 'G':
					// double or long double
					if (mod == kSizeNone)
						PrvPushDouble (sub, stackData);
					else
						PrvPushLongDouble (sub, stackData);
					break;

				case 'c':
					// int or wint_t
					if (mod == kSizeNone)
						PrvPushShort (sub, stackData);
#if defined (_MSC_VER)
					else if (sizeof (wint_t) == 2)
						PrvPushShort (sub, stackData);
#endif
					else
						PrvPushLong (sub, stackData);
					break;

				case 's':
					PrvPushString (sub, stackData, stringData);
					break;

				case 'p':
					PrvPushLong (sub, stackData);
					break;

				case 'n':
					// pointer
					// !!! Not supported for now...
					return false;

				case '%':
					// none
					break;

				default:
					// Bad conversion...
					return false;
			}
		}
	}

	return true;
}


// ---------------------------------------------------------------------------
//		¥ PrvPushShort
// ---------------------------------------------------------------------------

void PrvPushShort (EmSubroutine& sub, ByteList& stackData)
{
	// Read a 2-byte int from the caller's stack, and push it
	// onto our stack as a 4-byte int.

	char	paramName[20];
	sprintf (paramName, "param%d", (int) stackData.size ());

	char	decl[20];
	sprintf (decl, "UInt16 %s", paramName);

	sub.AddParam (decl);

	UInt16	value;
	sub.GetParamVal (paramName, value);

	ByteList::size_type	oldSize = stackData.size ();
	stackData.insert (stackData.end (), sizeof (int), 0);	// Make space for an "int"

	*(int*) &stackData[oldSize] = value;
}


// ---------------------------------------------------------------------------
//		¥ PrvPushLong
// ---------------------------------------------------------------------------

void PrvPushLong (EmSubroutine& sub, ByteList& stackData)
{
	// Read a 4-byte long int from the caller's stack, and push it
	// onto our stack as a 4-byte long int.

	char	paramName[20];
	sprintf (paramName, "param%d", (int) stackData.size ());

	char	decl[20];
	sprintf (decl, "UInt32 %s", paramName);

	sub.AddParam (decl);

	UInt32	value;
	sub.GetParamVal (paramName, value);

	ByteList::size_type	oldSize = stackData.size ();
	stackData.insert (stackData.end (), sizeof (long), 0);	// Make space for a "long int"

	*(long*) &stackData[oldSize] = value;
}


// ---------------------------------------------------------------------------
//		¥ PrvPushDouble
// ---------------------------------------------------------------------------

void PrvPushDouble (EmSubroutine& sub, ByteList& stackData)
{
	UNUSED_PARAM(sub)
	UNUSED_PARAM(stackData)
}


// ---------------------------------------------------------------------------
//		¥ PrvPushLongDouble
// ---------------------------------------------------------------------------

void PrvPushLongDouble (EmSubroutine& sub, ByteList& stackData)
{
	UNUSED_PARAM(sub)
	UNUSED_PARAM(stackData)
}


// ---------------------------------------------------------------------------
//		¥ PrvPushString
// ---------------------------------------------------------------------------

void PrvPushString (EmSubroutine& sub, ByteList& stackData, StringList& stringData)
{
	// Get the string pointer and clone the string into a new string object.

	char	paramName[20];
	sprintf (paramName, "param%d", (int) stackData.size ());

	char	decl[20];
	sprintf (decl, "char* %s", paramName);

	sub.AddParam (decl);

	emuptr	stringPtr;
	sub.GetParamVal (paramName, stringPtr);

	string	strCopy;
	size_t	strLen = EmMem_strlen (stringPtr);

	if (strLen > 0)
	{
		strCopy.resize (strLen);
		EmMem_strcpy (&strCopy[0], stringPtr);
	}

	// Add this string to the string array.

	stringData.push_back (strCopy);

	// In the stack data byte array, add a pointer to the string. 

	ByteList::size_type	oldSize = stackData.size ();
	stackData.insert (stackData.end (), sizeof (char*), 0); // Make space for a "char*"
	*(const char**) &stackData[oldSize] = (*(stringData.end () - 1)).c_str ();
}


// ---------------------------------------------------------------------------
//		¥ PrvToFILE
// ---------------------------------------------------------------------------

FILE* PrvToFILE (emuptr f)
{
	if ((HostFILEType*) f == hostLogFile)
	{
		return hostLogFILE;
	}

	return (FILE*) f;
}


// ---------------------------------------------------------------------------
//		¥ PrvTmFromHostTm
// ---------------------------------------------------------------------------

void PrvTmFromHostTm (struct tm& dest, const HostTmType& src)
{
	dest.tm_sec		= src.tm_sec_;
	dest.tm_min		= src.tm_min_;
	dest.tm_hour	= src.tm_hour_;
	dest.tm_mday	= src.tm_mday_;
	dest.tm_mon		= src.tm_mon_;
	dest.tm_year	= src.tm_year_;
	dest.tm_wday	= src.tm_wday_;
	dest.tm_yday	= src.tm_yday_;
	dest.tm_isdst	= src.tm_isdst_;
}


// ---------------------------------------------------------------------------
//		¥ PrvHostTmFromTm
// ---------------------------------------------------------------------------

void PrvHostTmFromTm (EmProxyHostTmType& dest, const struct tm& src)
{
	dest.tm_sec_	= src.tm_sec;
	dest.tm_min_	= src.tm_min;
	dest.tm_hour_	= src.tm_hour;
	dest.tm_mday_	= src.tm_mday;
	dest.tm_mon_	= src.tm_mon;
	dest.tm_year_	= src.tm_year;
	dest.tm_wday_	= src.tm_wday;
	dest.tm_yday_	= src.tm_yday;
	dest.tm_isdst_	= src.tm_isdst;
}


// ---------------------------------------------------------------------------
//		¥ PrvMapAndReturn
// ---------------------------------------------------------------------------

void PrvMapAndReturn (const void* p, long size, EmSubroutine& sub)
{
	emuptr	result = EmBankMapped::GetEmulatedAddress (p);

	if (result == EmMemNULL)
	{
		EmBankMapped::MapPhysicalMemory (p, size);
		result = EmBankMapped::GetEmulatedAddress (p);
	}

	PUT_RESULT_VAL (emuptr, result);
}


// ---------------------------------------------------------------------------
//		¥ PrvMapAndReturn
// ---------------------------------------------------------------------------

void PrvMapAndReturn (const string& s, EmSubroutine& sub)
{
	::PrvMapAndReturn (s.c_str (), s.size () + 1, sub);
}


// ---------------------------------------------------------------------------
//		¥ PrvReturnString
// ---------------------------------------------------------------------------

void PrvReturnString (const char* p, EmSubroutine& sub)
{
	EmBankMapped::UnmapPhysicalMemory (gResultString.c_str ());

	if (p)
	{
		gResultString = p;
		::PrvMapAndReturn (gResultString, sub);
	}
	else
	{
		gResultString.erase ();
		PUT_RESULT_VAL (emuptr, EmMemNULL);
	}
}


// ---------------------------------------------------------------------------
//		¥ PrvReturnString
// ---------------------------------------------------------------------------

void PrvReturnString (const string& s, EmSubroutine& sub)
{
	EmBankMapped::UnmapPhysicalMemory (gResultString.c_str ());

	gResultString = s;

	::PrvMapAndReturn (gResultString, sub);
}


#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Host::Initialize
// ---------------------------------------------------------------------------

void Host::Initialize	(void)
{
	memset (gHandlerTable, 0, sizeof (gHandlerTable));

	gHandlerTable [hostSelectorGetHostVersion]			= _HostGetHostVersion;
	gHandlerTable [hostSelectorGetHostID]				= _HostGetHostID;
	gHandlerTable [hostSelectorGetHostPlatform]			= _HostGetHostPlatform;
	gHandlerTable [hostSelectorIsSelectorImplemented]	= _HostIsSelectorImplemented;
	gHandlerTable [hostSelectorGestalt] 				= _HostGestalt;
	gHandlerTable [hostSelectorIsCallingTrap]			= _HostIsCallingTrap;

#if HAS_PROFILING
	gHandlerTable [hostSelectorProfileInit]				= _HostProfileInit;
	gHandlerTable [hostSelectorProfileStart]			= _HostProfileStart;
	gHandlerTable [hostSelectorProfileStop]				= _HostProfileStop;
	gHandlerTable [hostSelectorProfileDump]				= _HostProfileDump;
	gHandlerTable [hostSelectorProfileCleanup]			= _HostProfileCleanup;
	gHandlerTable [hostSelectorProfileDetailFn]			= _HostProfileDetailFn;
	gHandlerTable [hostSelectorProfileGetCycles]		= _HostProfileGetCycles;
#endif

	gHandlerTable [hostSelectorErrNo]					= _HostErrNo;

	gHandlerTable [hostSelectorFClose]					= _HostFClose;
	gHandlerTable [hostSelectorFEOF]					= _HostFEOF;
	gHandlerTable [hostSelectorFError]					= _HostFError;
	gHandlerTable [hostSelectorFFlush]					= _HostFFlush;
	gHandlerTable [hostSelectorFGetC]					= _HostFGetC;
	gHandlerTable [hostSelectorFGetPos]					= _HostFGetPos;
	gHandlerTable [hostSelectorFGetS]					= _HostFGetS;
	gHandlerTable [hostSelectorFOpen]					= _HostFOpen;
	gHandlerTable [hostSelectorFPrintF]					= _HostFPrintF;
	gHandlerTable [hostSelectorFPutC]					= _HostFPutC;
	gHandlerTable [hostSelectorFPutS]					= _HostFPutS;
	gHandlerTable [hostSelectorFRead]					= _HostFRead;
	gHandlerTable [hostSelectorRemove]					= _HostRemove;
	gHandlerTable [hostSelectorRename]					= _HostRename;
//	gHandlerTable [hostSelectorFReopen]					= _HostFReopen;
//	gHandlerTable [hostSelectorFScanF]					= _HostFScanF;
	gHandlerTable [hostSelectorFSeek]					= _HostFSeek;
	gHandlerTable [hostSelectorFSetPos]					= _HostFSetPos;
	gHandlerTable [hostSelectorFTell]					= _HostFTell;
	gHandlerTable [hostSelectorFWrite]					= _HostFWrite;
	gHandlerTable [hostSelectorTmpFile]					= _HostTmpFile;
	gHandlerTable [hostSelectorTmpNam]					= _HostTmpNam;
	gHandlerTable [hostSelectorGetEnv]					= _HostGetEnv;

	gHandlerTable [hostSelectorMalloc]					= _HostMalloc;
	gHandlerTable [hostSelectorRealloc]					= _HostRealloc;
	gHandlerTable [hostSelectorFree]					= _HostFree;

	gHandlerTable [hostSelectorAscTime]					= _HostAscTime;
	gHandlerTable [hostSelectorClock]					= _HostClock;
	gHandlerTable [hostSelectorCTime]					= _HostCTime;
//	gHandlerTable [hostSelectorDiffTime]				= _HostDiffTime;
	gHandlerTable [hostSelectorGMTime]					= _HostGMTime;
	gHandlerTable [hostSelectorLocalTime]				= _HostLocalTime;
	gHandlerTable [hostSelectorMkTime]					= _HostMkTime;
	gHandlerTable [hostSelectorStrFTime]				= _HostStrFTime;
	gHandlerTable [hostSelectorTime]					= _HostTime;

	gHandlerTable [hostSelectorMkDir]					= _HostMkDir;
	gHandlerTable [hostSelectorRmDir]					= _HostRmDir;
	gHandlerTable [hostSelectorOpenDir]					= _HostOpenDir;
	gHandlerTable [hostSelectorReadDir]					= _HostReadDir;
	gHandlerTable [hostSelectorCloseDir]				= _HostCloseDir;

//	gHandlerTable [hostSelectorFStat]					= _HostFStat;
	gHandlerTable [hostSelectorStat]					= _HostStat;
	
	gHandlerTable [hostSelectorGetFileAttr]				= _HostGetFileAttr;
	gHandlerTable [hostSelectorSetFileAttr]				= _HostSetFileAttr;

//	gHandlerTable [hostSelectorFTruncate]				= _HostFTruncate;
	gHandlerTable [hostSelectorTruncate]				= _HostTruncate;

	gHandlerTable [hostSelectorUTime]					= _HostUTime;

	gHandlerTable [hostSelectorGremlinIsRunning]		= _HostGremlinIsRunning;
	gHandlerTable [hostSelectorGremlinNumber]			= _HostGremlinNumber;
	gHandlerTable [hostSelectorGremlinCounter]			= _HostGremlinCounter;
	gHandlerTable [hostSelectorGremlinLimit]			= _HostGremlinLimit;
	gHandlerTable [hostSelectorGremlinNew]				= _HostGremlinNew;

	gHandlerTable [hostSelectorImportFile]				= _HostImportFile;
	gHandlerTable [hostSelectorExportFile]				= _HostExportFile;
	gHandlerTable [hostSelectorSaveScreen]				= _HostSaveScreen;
	gHandlerTable [hostSelectorImportFileWithID]		= _HostImportFileWithID;

	gHandlerTable [hostSelectorExgLibOpen]				= _HostExgLibOpen;
	gHandlerTable [hostSelectorExgLibClose]				= _HostExgLibClose;
	gHandlerTable [hostSelectorExgLibSleep]				= _HostExgLibSleep;
	gHandlerTable [hostSelectorExgLibWake]				= _HostExgLibWake;
	gHandlerTable [hostSelectorExgLibHandleEvent]		= _HostExgLibHandleEvent;
	gHandlerTable [hostSelectorExgLibConnect]			= _HostExgLibConnect;
	gHandlerTable [hostSelectorExgLibAccept]			= _HostExgLibAccept;
	gHandlerTable [hostSelectorExgLibDisconnect]		= _HostExgLibDisconnect;
	gHandlerTable [hostSelectorExgLibPut]				= _HostExgLibPut;
	gHandlerTable [hostSelectorExgLibGet]				= _HostExgLibGet;
	gHandlerTable [hostSelectorExgLibSend]				= _HostExgLibSend;
	gHandlerTable [hostSelectorExgLibReceive]			= _HostExgLibReceive;
	gHandlerTable [hostSelectorExgLibControl]			= _HostExgLibControl;
	gHandlerTable [hostSelectorExgLibRequest]			= _HostExgLibRequest;

	gHandlerTable [hostSelectorGetPreference]			= _HostGetPreference;
	gHandlerTable [hostSelectorSetPreference]			= _HostSetPreference;

	gHandlerTable [hostSelectorLogFile]					= _HostLogFile;
	gHandlerTable [hostSelectorSetLogFileSize]			= _HostSetLogFileSize;

//	gHandlerTable [hostSelectorSessionCreate]			= _HostSessionCreate;
//	gHandlerTable [hostSelectorSessionOpen]				= _HostSessionOpen;
	gHandlerTable [hostSelectorSessionSave]				= _HostSessionSave;
	gHandlerTable [hostSelectorSessionClose]			= _HostSessionClose;
	gHandlerTable [hostSelectorSessionQuit]				= _HostSessionQuit;
	gHandlerTable [hostSelectorSignalSend]				= _HostSignalSend;
	gHandlerTable [hostSelectorSignalWait]				= _HostSignalWait;
	gHandlerTable [hostSelectorSignalResume]			= _HostSignalResume;

#if HAS_TRACER
	gHandlerTable [hostSelectorTraceInit]				= _HostTraceInit;
	gHandlerTable [hostSelectorTraceClose]				= _HostTraceClose;
	gHandlerTable [hostSelectorTraceOutputT]			= _HostTraceOutputT;
	gHandlerTable [hostSelectorTraceOutputTL]			= _HostTraceOutputTL;
	gHandlerTable [hostSelectorTraceOutputVT]			= _HostTraceOutputVT;
	gHandlerTable [hostSelectorTraceOutputVTL]			= _HostTraceOutputVTL;
	gHandlerTable [hostSelectorTraceOutputB]			= _HostTraceOutputB;
#endif

	gHandlerTable [hostSelectorDbgSetDataBreak]			= _HostDbgSetDataBreak;
	gHandlerTable [hostSelectorDbgClearDataBreak]		= _HostDbgClearDataBreak;

	gHandlerTable [hostSelectorSlotMax]					= _HostSlotMax;
	gHandlerTable [hostSelectorSlotRoot]				= _HostSlotRoot;
	gHandlerTable [hostSelectorSlotHasCard]				= _HostSlotHasCard;

	gHandlerTable [hostSelectorGetFile]					= _HostGetFile;
	gHandlerTable [hostSelectorPutFile]					= _HostPutFile;
	gHandlerTable [hostSelectorGetDirectory]			= _HostGetDirectory;

	gHandlerTable [hostSelectorHostGetVolSize]			= _HostGetVolSize;
	gHandlerTable [hostSelectorHostGetVolFree]			= _HostGetVolFree;
}


// ---------------------------------------------------------------------------
//		¥ Host::Reset
// ---------------------------------------------------------------------------

void Host::Reset		(void)
{
	::PrvReleaseAllResources ();
}


// ---------------------------------------------------------------------------
//		¥ Host::Save
// ---------------------------------------------------------------------------

void Host::Save		(SessionFile&)
{
}


// ---------------------------------------------------------------------------
//		¥ Host::Load
// ---------------------------------------------------------------------------

void Host::Load		(SessionFile&)
{
}


// ---------------------------------------------------------------------------
//		¥ Host::Dispose
// ---------------------------------------------------------------------------

void Host::Dispose	(void)
{
	::PrvReleaseAllResources ();
}


// ---------------------------------------------------------------------------
//		¥ PrvReleaseAllResources
// ---------------------------------------------------------------------------

void PrvReleaseAllResources (void)
{
	// Close all open files.

	{
		vector<FILE*>::iterator	iter = gOpenFiles.begin ();
		while (iter != gOpenFiles.end ())
		{
			fclose (*iter);
			++iter;
		}

		gOpenFiles.clear ();
	}

	// Close all open directories.

	{
		vector<MyDIR*>::iterator	iter = gOpenDirs.begin ();
		while (iter != gOpenDirs.end ())
		{
			delete *iter;
			++iter;
		}

		gOpenDirs.clear ();

		// Unmap any mapped gHostDirEnt.

		EmBankMapped::UnmapPhysicalMemory (&gHostDirEnt);
	}

	// Release all allocated memory.

	{
		vector<void*>::iterator	iter = gAllocatedBlocks.begin ();
		while (iter != gAllocatedBlocks.end ())
		{
			EmBankMapped::UnmapPhysicalMemory (*iter);
			Platform::DisposeMemory (*iter);
			++iter;
		}

		gAllocatedBlocks.clear ();
	}

	// Unmap misc memory.

	EmBankMapped::UnmapPhysicalMemory (gResultString.c_str ());
	EmBankMapped::UnmapPhysicalMemory (&gGMTime);
	EmBankMapped::UnmapPhysicalMemory (&gLocalTime);
}


// ---------------------------------------------------------------------------
//		¥ PrvMalloc
// ---------------------------------------------------------------------------

emuptr PrvMalloc (long size)
{
	// Prepare the return value.

	emuptr	result = EmMemNULL;

	// Call the function.

	void*	newPtr = NULL;
	
	try
	{
		newPtr = Platform::AllocateMemory (size);
	}
	catch (...)
	{
	}

	// If the call worked, remember the pointer, map
	// it into emulated space, and return the mapped
	// pointer to the caller.

	if (newPtr)
	{
		gAllocatedBlocks.push_back (newPtr);
		EmBankMapped::MapPhysicalMemory (newPtr, size);

		result = EmBankMapped::GetEmulatedAddress (newPtr);
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ PrvRealloc
// ---------------------------------------------------------------------------

emuptr PrvRealloc (emuptr p, long size)
{
	// Prepare the return value.

	emuptr	result = EmMemNULL;

	// Recover the real pointer

	void*	oldPtr = EmBankMapped::GetRealAddress (p);
	if (oldPtr)
	{
		// Find the saved pointer in our list so that we can
		// remove it.

		vector<void*>::iterator	iter = gAllocatedBlocks.begin ();
		while (iter != gAllocatedBlocks.end ())
		{
			if (*iter == oldPtr)
			{
				// Found the saved pointer.  Now try the realloc.

				void*	newPtr = NULL;
				
				try
				{
					newPtr = Platform::ReallocMemory (oldPtr, size);
				}
				catch (...)
				{
				}

				// If that worked, then do the bookkeeping.

				if (newPtr)
				{
					gAllocatedBlocks.erase (iter);
					gAllocatedBlocks.push_back (newPtr);

					EmBankMapped::UnmapPhysicalMemory (oldPtr);
					EmBankMapped::MapPhysicalMemory (newPtr, size);

					result = EmBankMapped::GetEmulatedAddress (newPtr);
				}
				else
				{
					// Could not realloc!  Just return NULL
					// (already stored in "result").
				}

				return result;
			}

			++iter;
		}

		EmAssert (false);
		// !!! Handle not finding saved pointer
	}
	else
	{
		EmAssert (false);
		// !!! Handle not finding real address.
	}

	return result;
}


// ---------------------------------------------------------------------------
//		¥ PrvFree
// ---------------------------------------------------------------------------

void PrvFree (emuptr p)
{
	// Check the parameters.

	if (p)
	{
		// Recover the real pointer

		void*	oldPtr = EmBankMapped::GetRealAddress (p);
		if (oldPtr)
		{
			// Find the saved pointer in our list so that we can
			// remove it.

			vector<void*>::iterator	iter = gAllocatedBlocks.begin ();
			while (iter != gAllocatedBlocks.end ())
			{
				if (*iter == oldPtr)
				{
					Platform::DisposeMemory (oldPtr);

					EmBankMapped::UnmapPhysicalMemory (*iter);
					gAllocatedBlocks.erase (iter);

					return;
				}

				++iter;
			}

			EmAssert (false);
			// !!! Handle not finding saved pointer
		}
		else
		{
			EmAssert (false);
			// !!! Handle not finding real address.
		}
	}
}
