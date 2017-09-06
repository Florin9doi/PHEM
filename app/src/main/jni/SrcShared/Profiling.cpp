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
#include "Profiling.h"

#include "EmHAL.h"				// GetSystemClockFrequency
#include "EmMemory.h"			// EmMemCheckAddress, EmMemGet16
#include "EmPalmFunction.h"		// FindFunctionName, GetTrapName
#include "EmStreamFile.h"		// EmStreamFile
#include "Miscellaneous.h"		// IsSystemTrap, StMemory
#include "Platform.h"			// Platform::Debugger
#include "Strings.r.h"			// kStr_ values
#include "UAE.h"				// m68k_areg, m68k_dreg, regs, m68k_getpc, get_iword


/*
	P.S.  Here are some notes on interpreting the output

	Times are all theoretically in milliseconds.  Internally, Poser is faking
	a clock by counting reads, writes, wait states, and extra cycles for many
	68K instructions.  I'd estimate this count is correct to within a few percent,
	and I have a list of known incorrect instructions I'm working on getting more
	accurate.  (Notable issues: JSR, RTS, RTE fall one read short each, for 6 or 8
	cycles each depending on ROM or RAM, ANDSR, ORSR, and other "SR" instructions
	are 2 reads short, for 12 or 16 cycles.)

	Anyway, the cycle counts are stored as 64-bit integers in the profiler output,
	and a multiplier is applied to scale the result to milliseconds based on a
	58.xxx MHz clock.  So a number like 5661.518 is a bit over 5 1/2 seconds, and
	1.255 is a bit over 1 1/4 milliseconds.  (In theory at least, I'm still
	validating the data, if you see anything that strikes you as inaccurate, please
	tell me about it!)

	The function names mostly come from the ROM.Map file, but there are a few
	special cases:

	"functions" is a top-level cover that includes all regular function calls
	"interrupts" is a top-level node that includes all interrupts (except the ones
				Poser has patched out)
	"partial" means profiling was started in the middle of the function, so we
				don't have the address of the fn and consequently don't have a name.
	"overflow" is a lump of all functions called when we're out of space to track
				more unique calls, where unique means called from the same path to
				the "root" of the call tree.
	"unknown" is a function for which no name could be found.  Many functions in
				.prc files show up as unknown.

	The rest of the names all take the form "Name $address.x" where:
	   Name- is the name of the function or trap.
	   address-For regular functions, the 4 byte address.
 		   -For traps, the 2 byte trap number.
		   -For interrupts, the 1 byte interrupt number.
	   x- debugging info, where the name comes from. 
		't' = trap names table built in Poser,
		'm'= the ROM.Map file
		'd'=the symbol built into the ROM by the compiler
		'i'=invalid address flag (usually due to POSERs implementation internals)

	The other columns are defined as follows.  Note MINIMUM and STACK SPACE are
	NOT what you expect:

	count				- the number of times the functions was called.
	only				- time spent in the function, not counting child fns or interrupts.
	% (by only)			- percentage of total profiling time spent in this fn/call.
	+Children			- time spent in the function including child fns, but not
						  including interrupts
	% (by +Children)	- percentage of total profiling time spent in the fn and its kids
	Average				- "Only" divided by "count"
	Maximum				- the maximum time (in msec) spent in any 1 call to the fn.
	Minimum				- NOT WHAT YOU EXPECT.  This is actually the time spent handling
						  interrupts for calls to that particlular instance of that fn.
						  Due to the way the "Summary" is calculated, this number won't
						  be correct in summary views.
	Stack space			- NOT WHAT YOU EXPECT.  More of a trap/interrupt counter plus
						  some debug info.  The number in that field for a particular
						  fn entry is incremented by 1 every time the fn is interrupted,
						  by 10000 if the fn call is made by a faked up RTS instead of
						  a JSR, and 1000 if the function was executing when an RTS
						  occurred that didn't return to it's called but instead returned
						  to some fn farther up the call chain.  Again, this will only
						  be useful in the detail view, since the summary does some
						  computation on it.


	Please respond to palm-dev-forum@ls.palm.com
	To:	palm-dev-forum@ls.palm.com
	Subject:	Re: RE: Emulator profiling...

	At 7:14 PM -0700 10/6/98, Kenichi Okuyama wrote:
	>I'll be pleased if you can give us any good information source
	>beside source code itself, about POSE's profiler. Like, byte
	>alignment size( is it 4? or 8 on Mac... I don't know about Mac
	>really ), etc. Those informations, we can't get them from source
	>code itself, and since I don't have Mac, I can't get them from
	>project file for Mac, you see.

	I don't think alignment matters much at all for virtually all of the structs.

	The only ones where alignment will be important are the ones written to be
	compatible with the MW profiler.  These are ProfFileHeader and FnCallRecord
	in Profiling.cpp.  All the elements of those structs are either 4 or 8
	bytes wide.

	The MW profiler file consists of 3 sections.  The first section is a
	header, incompletely described by ProfFileHeader in Profiling.cpp.

	The second section is a big tree of function call blocks.  Each block is
	described by the FnCallRecord struct.  The tree is maintained via two
	pointers in each record, called 'kid' and 'sib', which contain array
	indexes.  kid points to the first child of a given node, sib points to a
	given node's next sibling.  So to enumerate all the kids for a given node
	you go to that node's kid, then follow the sib links from kid to kid until
	you get a sib value of -1, which is the end of the sibling list.  The other
	fields in the function call record are pretty much self explanitory.  (Note
	however that the POSER profiler uses cyclesMax and stackUsed for other
	things!  They track trap dispatcher overhead right now.)

	The 3rd section contains the names of all the functions, stored in a string
	table.  The string table is just a concatenation of all the name strings,
	seperated only by the terminating nulls, with each unique name appearing
	only once.  The FnCallRecord's address field stores the offset of the start
	of that function's name into the string table.  (It's sort of a normalized
	char *)  That structure is a byte stream, there is no other alignment.  The
	function FindOrAddString is used to build the string table when dumping a
	profiler file.

	>Also, if possible, what kind of information is kept in that profile
	>file, like function name, call count, running time, etc.

	FnCallRecord should pretty much explain that.  The only other interesting
	thing to note is that the data isn't stored on a function by function basis.
	Rather, it's stored by unique occurrences of a function.  That is, say
	function A calls function C, and function B also calls function C.  There
	will be two nodes in the function tree (two records) for function C, one
	storing information from calls from function A, and a second storing
	information from calls from function B.  Recursive functions will create a
	whole bunch of nodes, one for each level of recursion.  ..but that's
	necessary to properly create the call tree that the MW profiler displays.

	...the MW profiler's "summary" mode does a lot of work!  It has to go
	through the whole tree and collect the data for each node that represents a
	given function call and summarize it.  That was one of the reasons I chose
	to try to output MW profiler files!

	I think if I had to make a windows profiler, I'd start with the ProfileDump
	function in Profiling.cpp.  You could change that routine to take the data
	collected and output whatever you wanted.

	You might look at ProfilePrint, which does a text dump of the gathered
	profile data.  RecursivePrintBlock will give you a good start on something
	that translates the MW function records into some other data format.

					--Bob


	Please respond to palm-dev-forum@3com.com
	To:	palm-dev-forum@ls.3com.com
	Subject:	POSER Profiling under Windows

	The #1 item on my to-do list post-Palm Computing Platform Developer
	Conference was to update the Windows emulator to enable profiling.  I'm
	happy to say this is done (more or less), and a build is now available.
	It's 2.1d23, and you can download it at
	http://palm.3com.com/devzone/pose/seed.html

	There still is no nice viewer for Windows.  Instead, the emulator now
	outputs both the MW Profiler format file and a tab delimited text file
	containing the same information.  Until a nice viewer exists for Windows,
	you can open the text file in a text editor or spreadsheet app and get at
	the profiling info.

	For convenience, the text file output is pre-sorted by the "time in
	function and all it's children" category.

	For inconvenience, all levels of function calls (down into the kernel and
	beyond) are displayed.  Typically this is WAY too much information.  The
	only advice I can offer is a strategy for getting rid of it:

	The first column contains the nesting level of the function -- it defines
	the "tree".  So if there's too much detail inside a particular function
	call, you can note the number in the fist column, then search downward for
	an equal number.   The intervening lines (often very many) are sub-calls
	from that first call, and you might want to simply delete them to make the
	output more readable.

	That is, say you get something like this:

	1	foo			...
	2	bar			...
	3	baz			...
	4	cj_fdkjqwfd	...
	5	kp_sdlsvbnf	...
	4	cj_aslkqwsd	...
	5	lkaflds		...
	6	cj_sdldsfdl	...
	7	tm_eouas	...
	6	cj_werofs	...
	3	qux			...

	If you're only interested in where function foo is spending it's time, you
	can note that baz and qux are both sub-calls, and everything in-between
	them are sub-sub-calls that baz made.  Anyway, just deleting the lines with
	nesting level 4 and above will considerably clean up the output and make it
	easier to read.

					--Bob

*/

// sizes of buffers to use
#define AVGNAMELENGTH			  64
#define MAXNESTEDINTERRUPTS		   8

// max length of routine name (C++ mangled names can be this long!)
#define	MAX_ROUTINE_NAME		 256

// shared values exported via Profiling.h
int64	gClockCycles;
int64	gReadCycles;
int64	gWriteCycles;

long	gReadMismatch;		// debug
long	gWriteMismatch;		// debug

int		gProfilingEnabled;
int		gProfilingOn;
int		gProfilingCounted;
int		gProfilingDetailed;

emuptr	gProfilingEnterAddress;
emuptr	gProfilingReturnAddress;
emuptr	gProfilingExitAddress;

// Internal stuff

int64	gCyclesCounted;		// cycles actually counted against functions

static int	gMaxCalls;
static int	gMaxDepth;

#define PROFILE_ONE_FN		0	// set to 1 to profile on a particular fn enter/exit
#define FNTOPROFILE			sysTrapDmGetNextDatabaseByTypeCreator

#define NOADDRESS			(0x00000000)
#define ROOTADDRESS			(0xFFFFFFFF)
#define INTERRUPTADDRESS	(0xFFFFFFFE)
#define OVERFLOWADDRESS		(0xFFFFFFFD)
#define NORECORD			(-1)

/*
	The profile output file is composed of three sections. A header section
	of 0x200 bytes, then a section of FnCallRecord structures containing an
	array of function data, followed by a string table containing the names
	of each function. The functions are represented in a tree, where each
	node represents a given function when called from a particular path to
	the root of the tree. That is, if A calls C and B calls C, then there
	will be at least 2 records in the tree representing calls to C, once
	when called from A and another when called from B. I say at least 2
	because A and B themselves may be called from multiple places, and each
	unique version of A and B will also have a unique child node representing C.

	Header

	The header section contains things like the number of functions in the
	function data array, and offset to the start of the string table, the
	size of the string table, and the record number (array index) of the
	root node in the function tree.
*/

struct ProfFileHeader
{
	Int32	proF;			// 'proF'
	Int32	version;		// 0x00040002
	Int32	fnCount;		// number of unique fns (records) in log
	Int32	four;			// 0x00000004

	Int32	zeros1;			// 0x00000000
	Int32	zeros2;			// 0x00000000
	Int32	unknown;		// 0xB141A3A9	- maybe timebase data
	Int32	recordsSize;	// size of header plus size of data (or offset to string table)
	
	Int32	stringTableSize;// size of string table in bytes
	int64	overhead;		// count for overhead
	Int32	rootRec;		// record number of root of tree
	
	Int32	sixtyfour1;		// 0x00000064
	Int32	sixtyfour2;		// 0x00000064
	Int32	countsPerTime;  // translation between counts at nodes and integers in column
							// 0x00FD0000 = 16.580608 MHz with display in seconds
							// 0x000040C4 = 16.580608 MHz with display in milliseconds
	Int32	oddstuff0;		// seems like it can be 0, set by profiler tool itself
	
	Int32	oddstuff1;		// seems like it can be 0, set by profiler tool itself
	Int32	oddstuff2;		// seems like it can be 0, set by profiler tool itself
	Int32	oddstuff3;		// seems like it can be 0, set by profiler tool itself
	Int32	oddstuff4;		// seems like it can be 0, set by profiler tool itself
};
const int kProfFileHeaderSize = 0x200;	// bytes (gregs)

/*
	Function Array

	Following the header (starting at offset 0x200 in the file) is an array
	of header.fnCount structs as defined below. Each struct represents a unique
	node in the tree. Most of these values are straightfoward, and appear in
	the results view representing what you expect. The sib and kid records
	determine the relationship to other functions in the tree. Children of a
	given node are found by looking at the node's kid field to find the first
	child, then look at the child's sib field to find the adjacent nodes which
	are all considered children of the first kid's parent. If the sib or kid
	contain -1 then there are no sibling or children nodes.
*/

struct FnCallRecord
{
	emuptr	address;		// address of fn, also offset from start of name table to this fn's name
	Int32	entries;		// times function was called
	int64	cyclesSelf;		// profiling data for this fn alone 
	int64	cyclesPlusKids;	// profiling data for this fn with kids
	int64	cyclesMin;		// profiling data for this fn alone, min
	int64	cyclesMax;		// profiling data for this fn alone, max
	Int32	sib;			// record number of sib, -1 for no sibs
	Int32	kid; 			// record number of kid, -1 for no kids
	Int32	stackUsed;		// bytes of stack used by fn, we use it to count unmatched returns
};
const int kFnCallRecordSize = 52;	// bytes (gregs)

struct FnStackRecord
{
	int32	call;						// fn data block for fn being called
	emuptr	returnAddress;				// return address aka (SP) to calling fn
	int64	cyclesAtEntry;				// cycle count when fn was called
	int64	cyclesAtInterrupt;			// cycle count when fn was interrupted
	int64	cyclesInKids;				// number of cycles spent in subroutines and interrupts
	int64	cyclesInInterrupts;			// number of cycles spent in interrupts for this fn alone
	int64	cyclesInInterruptsInKids;	// how much of cyclesInKids is interrupts
	int16	opcode;						// cached opcode for instruction profiling
};

/*

	To put this all together graphically, let's assume that an application
	starts off and calls FuncA, FuncB, and FuncC, in that order.  As well,
	FuncC calls FuncA.  At the point where FuncC calls FuncA, our "calls"
	and "gCallStack" structures look as follows:
	
		gCallStack
			0: PilotMain
			1: FuncC
			2: FuncA
			
		gCallTree (as flat array)
			0: Root record
			1: PilotMain (called from Root)
			2: FuncA (called from PilotMain)
			3: FuncB (called from PilotMain)
			4: FuncC (called from PilotMain)
			5: FuncA (called from FuncC)

		gCallTree (as tree)

				PilotMain
				  | (kid)
				  V
				FuncA -----> FuncB -----> FuncC			
					  (sib)        (sib)   | (kid)
										   V
										  FuncA (gCallTree[5], not gCallTree[2])
*/

// call tree
static FnCallRecord*	gCallTree = NULL;
static int32			gFirstFreeCallRec;
static int32			gRootRecord;
static int32			gExceptionRecord;
static int32			gOverflowRecord;

// call stack (interrupts and calls in interrupts on same stack)
static FnStackRecord*	gCallStack = NULL;
static int				gCallStackSP;

// interrupt tracking, interrupt stack is gCallStackSP when interrupt happened
static int				gInterruptDepth;
static int				gInterruptStack[MAXNESTEDINTERRUPTS];
static int64			gInterruptCount;
static int64			gInterruptCycles;

static unsigned long	gMissedCount;			// debug
static unsigned long	gExtraPopCount;			// debug
static int				gInterruptMismatch;		// debug

// for detailed (instruction level) profiling
static int				gInInstruction;
static FILE*			gProfilingDetailLog;
static int64			gClockCyclesLast;
static int64			gClockCyclesSaved;
static int64			gReadCyclesSaved;
static int64			gWriteCyclesSaved;
static emuptr			gDetailStartAddr;
static emuptr			gDetailStopAddr;



//---------------------------------------------------------------------
// Get function name out of debug symbols compiler produces
//---------------------------------------------------------------------

struct ROMMapRecord
{
	emuptr	address;
	char*	name;
};
ROMMapRecord*	gROMMap = NULL;
int				gROMMapEnd = 0;


// ¥¥¥ÊDOLATER ¥¥¥
// We need to handle shared library routines, which are called by
// trap dispatcher.  They start at 0x0000A800

#define kOpcode_ADD		0x0697	// ADD.L X, (A7)
#define kOpcode_LINK	0x4E50
#define kOpcode_RTE		0x4E73
#define kOpcode_RTD		0x4E74
#define kOpcode_RTS		0x4E75
#define kOpcode_JMP		0x4ED0

#define FIRSTTRAP 0x0000A000UL
#define LASTTRAP (FIRSTTRAP + 0x0001000)

#define LAST_EXCEPTION	0x100

#define IS_TRAP_16(addr)			(((addr) >= FIRSTTRAP) && ((addr) < LASTTRAP))
#define IS_TRAP_32(addr)			IS_TRAP_16((addr) >> 16)
#define PACK_TRAP_INFO(trap, extra)	((((uint32) (trap)) << 16) | ((uint16) (extra)))
#define TRAP_NUM(field)				(((field) >> 16) & 0x0FFFF)
#define TRAP_EXTRA(field)			((field) & 0x0FFFF)

// Creates unique file names for Profile output files

string CreateFileNameForProfile (Bool forText);


uint32	gCyclesPerSecond;

uint32 inline PrvGetCyclesPerSecond (void)
{
	if (gCyclesPerSecond == 0)
	{
		gCyclesPerSecond = EmHAL::GetSystemClockFrequency ();
	}

	return gCyclesPerSecond;
}

uint32 inline PrvGetCyclesPerMillisecond (void)
{
	return PrvGetCyclesPerSecond () / 1000;
}

// ---------------------------------------------------------------------------
//		¥ GetRoutineName
// ---------------------------------------------------------------------------
// GetRoutineName turns an address (or interrupt number or trap number) into a
// string representation for that function. It attempts to use the ROM.map
// data to find the function name, and if no match is found it steps through
// the instructions found at the address indicated until it finds the end of
// the function, then looks for the debug symbol. If the address was to a
// function in a code resource and the resource was moved, then the name will
// be incorrect. GetRoutineName is completely responsible for turning an
// address into a name, and it will always return some string, even if passed
// an invalid address.

// Llamagraphics, Inc:  Rather than passing in a buffer of unknown length,
// GetRoutineName now returns a statically allocated string that's good
// until the next time GetRoutineName is called.  This eliminates the need
// for dynamically allocating space in the heap for this string, and makes
// it easier to ensure that the buffer isn't overrun.

static char * GetRoutineName (emuptr addr)
{
	// Llamagraphics, Inc:  IMPORTANT!!!
	// C++ routine names stored in MacsBug format can take a full 256 characters
	// to store.  Any text that we append in addition to the MacsBug name must be
	// allowed for in the buffer size.
	static char buffer[256 + 18];
	emuptr startAddr;
	
	// if it's a dummy function header, say so
	if (addr == NOADDRESS)
	{
		strcpy(buffer, Platform::GetString (kStr_ProfPartial).c_str ());
		return buffer;
	}

	if (addr == ROOTADDRESS)
	{
		strcpy(buffer, Platform::GetString (kStr_ProfFunctions).c_str ());
		return buffer;
	}

	if (addr == INTERRUPTADDRESS)
	{
		strcpy(buffer, Platform::GetString (kStr_ProfInterrupts).c_str ());
		return buffer;
	}

	if (addr == OVERFLOWADDRESS)
	{
		strcpy(buffer, Platform::GetString (kStr_ProfOverflow).c_str ());
		return buffer;
	}

	if (addr < LAST_EXCEPTION)
	{
		sprintf(buffer, Platform::GetString (kStr_ProfInterruptX).c_str (), addr);
		return buffer;
	}

	// check for traps
	if (IS_TRAP_32(addr))
	{
		sprintf(buffer, Platform::GetString (kStr_ProfTrapNameAddress).c_str (),
					::GetTrapName (TRAP_NUM(addr), TRAP_EXTRA(addr), true), TRAP_NUM(addr));
		return buffer;
	}

	// look up address in the ROM map
	if (gROMMap != NULL)
	{
		int i = 0;
		// ¥¥¥ÊDOLATER ¥¥¥  use binary search, since gROMMap is sorted by address
		while (i < gROMMapEnd && addr > gROMMap[i].address)
			i++;
		if (i < gROMMapEnd && addr == gROMMap[i].address)
		{
			sprintf(buffer, Platform::GetString (kStr_ProfROMNameAddress).c_str (), gROMMap[i].name, addr);
			return buffer;
		}
	}

	// if not in the map, try to get the symbol out of the ROM itself
	// Llamagraphics, Inc:  Pass in the size of the buffer to FindFunctionName,
	// which prevents it from truncating the name at 31 characters.
	::FindFunctionName (addr, buffer, &startAddr, NULL, sizeof(buffer));

	if (strlen (buffer) == 0)
	{
		// no symbol in ROM or invalid address, just output address
		sprintf (buffer, Platform::GetString (kStr_ProfUnknownName).c_str (), addr);
	}
	else
	{
		if (startAddr != addr)
			sprintf(&buffer[strlen(buffer)], "+$%04lX", addr-startAddr);

#if 0
	// Removed this for now.  From Catherine White (cewhite@llamagraphics.com):
	//
	//	By the way, including the 12 char absolute address in square brackets
	//	interferes with the MW Profiler feature that unmangles C++ function
	//	names. In our copy of the Emulator we've chosen to eliminate the absolute
	//	address because we prefer to see the unmangled names in the profiler. It
	//	makes it easier to see what is going on.

		// Hack the address onto the end
		sprintf(&buffer[strlen(buffer)], " [$%08lX]", addr);
#endif
	}
	
	return buffer;
}





//---------------------------------------------------------------------
// Converting addresses to human readable names
//---------------------------------------------------------------------

char*	gStringTable = NULL;
int		gStringTableEnd;
int		gStringTableCapacity;


// ---------------------------------------------------------------------------
//		¥ InitStringTable
// ---------------------------------------------------------------------------
// Allocate the string table

static void InitStringTable (void)
{
	// Llamagraphics, Inc:  Since the gStringTable can now grow dynamically,
	// the initial value for gStringTableCapacity isn't so crucial as it once
	// was.  AVGNAMELENGTH * MAXUNIQUEFNS may be overkill.

	gStringTableCapacity = AVGNAMELENGTH * MAXUNIQUEFNS;
	gStringTable = (char*) Platform::AllocateMemory (gStringTableCapacity);
	gStringTableEnd = 0;
}


// ---------------------------------------------------------------------------
//		¥ CleanupStringTable
// ---------------------------------------------------------------------------

static void CleanupStringTable (void)
{
	Platform::DisposeMemory (gStringTable);
}


// ---------------------------------------------------------------------------
//		¥ FindOrAddString
// ---------------------------------------------------------------------------

static int FindOrAddString (const char* newS)
{
	int	offset = 0;
	while (offset < gStringTableEnd)
	{
		if (strcmp (&gStringTable[offset], newS) == 0)
			return offset;
		else
			offset += strlen (&gStringTable[offset]) + 1;
	}
	
	// Llamagraphics, Inc:  Added code to automatically increase the size of
	// the gStringTable as needed.  This is part of the solution for handling
	// long mangled C++ function names.

	gStringTableEnd = offset + strlen(newS) + 1;
	if (gStringTableEnd > gStringTableCapacity)
	{
		// We need to increase the capacity of the gStringTable.  Note that
		// even though we've modified gStringTableEnd, offset contains the
		// previous value of gStringTableEnd.

		gStringTableCapacity = (gStringTableEnd * 8) / 5;	// a moderately bigger number
		char* newTable = (char*) Platform::AllocateMemory (gStringTableCapacity);
		memcpy (newTable, gStringTable, offset);
		Platform::DisposeMemory (gStringTable);
		gStringTable = newTable;
	}

	strcpy (&gStringTable[offset], newS);
	return offset;
}


// ---------------------------------------------------------------------------
//		¥ LinearAddressToStrings
// ---------------------------------------------------------------------------

// Llamagraphics, Inc:  This routine used to be recursive, but we rewrote
// it to be linear because the recursive version was blowing the stack
// on the Macintosh.

static void LinearAddressToStrings (void)
{
	for (int32 i = 0; i < gFirstFreeCallRec; ++i)
	{
		// Not all addresses are valid. For instance, I've seen the value
		// 0x011dec94 in gCallTree[x].address, which is either the address of the
		// TRAP $C instruction used to regain control in ATrap::DoCall, or
		// December 11, 1994.
		//  ...but that's OK, GetRoutineName handles that (calls EmMemCheckAddress)

		gCallTree[i].address = FindOrAddString (GetRoutineName (gCallTree[i].address));
	}
}


// ---------------------------------------------------------------------------
//		¥ CheckTree
// ---------------------------------------------------------------------------

#if 0

// Llamagraphics, Inc:  We wrote this routine to sanity check the calls
// tree and make sure that the kid and sib links were consistant.  It
// didn't turn up any problems, but might come in handy in the future.
// It did help us understand that recursing on siblings is a bad idea...

static void CheckTree (void)
{
	EmAssert (gFirstFreeCallRec <= gMaxCalls);
	
	bool*	isUsed	= new bool[gMaxCalls];
	int*	pending	= new int[gMaxDepth];
	
	EmAssert (isUsed != 0);
	EmAssert (pending != 0);
	
	for (int i = 0; i < gMaxCalls; ++i)
		isUsed[i] = false;

	for (int i = 0; i < gMaxDepth; ++i)
		pending[i] = NORECORD;
	
	int depth = 0;
	int maxDepth = 0;
	int recordsProcessed = 0;
	
	// To start with, just the root node is pending
	pending[depth++] = gRootRecord;
	while (depth > 0)
	{
		// Pop off the current record
		int current = pending[--depth];
		
		// If the current record is NORECORD, then we're done
		// at this level and can continue popping to the next level.
		if (current == NORECORD)
			continue;

		EmAssert ((current >= 0) && (current < gFirstFreeCallRec));
		
		++recordsProcessed;
		
		// Make sure that this is the first time that we've processed
		// this record.

		EmAssert (!isUsed[current]);
		isUsed[current] = true;
		
		// If we have a sibling, push it on the stack.
		if (gCallTree[current].sib != NORECORD)
		{
			EmAssert (depth < gMaxDepth);
			pending[depth++] = gCallTree[current].sib;
			if (depth > maxDepth)
				maxDepth = depth;
		}
		
		// If we have a kid, push it on the stack.
		if (gCallTree[current].kid != NORECORD)
		{
			EmAssert (depth < gMaxDepth);
			pending[depth++] = gCallTree[current].kid;
			if (depth > maxDepth)
				maxDepth = depth;
		}
	}
	
	// Make sure that all of the records were used.
	for (int i = 0; i < gFirstFreeCallRec; ++i)
	{
		EmAssert (isUsed[i]);
	}

	EmAssert (recordsProcessed == gFirstFreeCallRec);
	
	delete [] isUsed;
	delete [] pending;
}
			
#endif

//---------------------------------------------------------------------
// debugging routine to print profiling stuff to a log file
//---------------------------------------------------------------------

#if PLATFORM_WINDOWS
#define LINE_FORMAT_SPEC "%d\t%d\t%d\t%s\t%ld\t%I64d\t%.3f\t%.1f\t%I64d\t%.3f\t%.1f\t%.3f\t%.3f\t%.3f\t%ld\n"
#else	// MAC
#define LINE_FORMAT_SPEC "%d\t%d\t%d\t%s\t%ld\t%lld\t%.3f\t%.1f\t%lld\t%.3f\t%.1f\t%.3f\t%.3f\t%.3f\t%ld\n"
#endif

#define MAXNESTING	80


// ---------------------------------------------------------------------------
//		¥ RecursivePrintBlock
// ---------------------------------------------------------------------------

static void RecursivePrintBlock(FILE* resultsLog, int i, int depth, int parent)
{
	while (i != NORECORD)
	{
		// Use statics in this function to reduce stack frame size
		// from 272 bytes to 160 bytes.  Printing each number by itself
		// instead of with one long formatting string further reduces
		// the stack frame size to 96 bytes.
		//
		// And, yes, these efforts are important.  We were easly using
		// 190K of stack space, blowing the space allocated on the Mac.

		static double cyclesSelfms;
		static double cyclesSelfpct;
		static double cyclesKidsms;
		static double cyclesKidspct;
		
		cyclesSelfms	= gCallTree[i].cyclesSelf / PrvGetCyclesPerMillisecond ();
		cyclesSelfpct	= gCallTree[i].cyclesSelf / gCyclesCounted * 100;
		cyclesKidsms	= gCallTree[i].cyclesPlusKids / PrvGetCyclesPerMillisecond ();
		cyclesKidspct	= gCallTree[i].cyclesPlusKids / gCyclesCounted * 100;
		
		static double temp1;
		static double temp2;
		static double temp3;
		
		temp1 = (double) gCallTree[i].cyclesSelf / (double)gCallTree[i].entries / (double)PrvGetCyclesPerMillisecond ();
		temp2 = (double) gCallTree[i].cyclesMax / (double)PrvGetCyclesPerMillisecond ();
		temp3 = (double) gCallTree[i].cyclesMin / (double)PrvGetCyclesPerMillisecond ();

		fprintf (resultsLog, "%d", i);
		fprintf (resultsLog, "\t%d", parent);
		fprintf (resultsLog, "\t%d", depth);
		fprintf (resultsLog, "\t%s", GetRoutineName (gCallTree[i].address));
		fprintf (resultsLog, "\t%ld", gCallTree[i].entries);
		fprintf (resultsLog, "\t%lld", gCallTree[i].cyclesSelf);
		fprintf (resultsLog, "\t%.3f", cyclesSelfms);
		fprintf (resultsLog, "\t%.1f", cyclesSelfpct);
		fprintf (resultsLog, "\t%lld", gCallTree[i].cyclesPlusKids);
		fprintf (resultsLog, "\t%.3f", cyclesKidsms);
		fprintf (resultsLog, "\t%.1f", cyclesKidspct);
		fprintf (resultsLog, "\t%.3f", temp1);
		fprintf (resultsLog, "\t%.3f", temp2);
		fprintf (resultsLog, "\t%.3f", temp3);
		fprintf (resultsLog, "\t%ld\n", gCallTree[i].stackUsed);

		RecursivePrintBlock (resultsLog, gCallTree[i].kid, depth + 1, i);

		// Was:
		//
		//		RecursivePrintBlock(resultsLog, gCallTree[i].sib, depth, parent);
		//
		// Recoded to manually force tail-recursion.  Doing this bumped
		// the stack frame size back up to 112 bytes, but hopefully this is
		// greatly offset by not recursing as much.

		i = gCallTree[i].sib;
	}
}


// ---------------------------------------------------------------------------
//		¥ PrintBlock
// ---------------------------------------------------------------------------

static void PrintBlock(FILE *resultsLog, int i)
{
	RecursivePrintBlock (resultsLog, i, 0, NORECORD);
}


// ---------------------------------------------------------------------------
//		¥ RecursiveSortKids
// ---------------------------------------------------------------------------

static void RecursiveSortKids(int parent)
{
	if (parent != NORECORD)
	{
		int i = gCallTree[parent].kid;
		int iprev = NORECORD;

		while (i != NORECORD)
		{
			// sort the kids of each node
			RecursiveSortKids(i);
						
			// start at root, examine each node until insertion point is found
			int j = gCallTree[parent].kid;
			int jprev = NORECORD;
			
			while (	j != NORECORD &&
					j != i &&
					gCallTree[i].cyclesPlusKids < gCallTree[j].cyclesPlusKids)
			{
				jprev = j;
				j = gCallTree[j].sib;
			}
			
			// assuming inserting eariler in list, update pointers.
			if (j != NORECORD)
			{
				if (j == i)
				{
					// no need to insert, since it's in the right place, so just go on
					iprev = i;
				}
				else
				{
					// moving i, so first remove i from list
					if (iprev == NORECORD) Platform::Debugger();	// should never happen, i replacing itself!
					gCallTree[iprev].sib = gCallTree[i].sib;
					
					// insert i before j
					if (jprev == NORECORD)
					{
						gCallTree[i].sib = gCallTree[parent].kid;
						gCallTree[parent].kid = i;
					}
					else
					{
						gCallTree[i].sib = gCallTree[jprev].sib;
						gCallTree[jprev].sib = i;
					}
				}
			}

			// go on to next node
			i = gCallTree[iprev].sib;
		}
			
	}
}


//---------------------------------------------------------------------
// Call stack and call record management routines
//---------------------------------------------------------------------

// ---------------------------------------------------------------------------
//		¥ PopCallStackFn
// ---------------------------------------------------------------------------
// PopCallStackFn is used when a function or interrupt is being exited from.
// It updates the nodes in the tree to properly track call counts and time
// spent in interrupts and in children. It is called both during normal
// function or interrupt exit and by cleanup code that's trying to keep the
// profiler's call stack in sync with the CPU's call stack.

static int64 PopCallStackFn(Boolean normalReturn)
{
	EmAssert (gCallStackSP >= 0);

	// cyclesThisCall includes all interrupts and kids
	int64 cyclesThisCall = gClockCycles - gCallStack[gCallStackSP].cyclesAtEntry;
	
	// cyclesInMe does not include interrupts or kids
	int64 cyclesInMe = cyclesThisCall - gCallStack[gCallStackSP].cyclesInKids;
	
	int64 totalCyclesInInterrupts = gCallStack[gCallStackSP].cyclesInInterrupts +
									gCallStack[gCallStackSP].cyclesInInterruptsInKids;
	
	int32 exiter = gCallStack[gCallStackSP].call;

	gCallTree[exiter].entries++;

	gCallTree[exiter].cyclesPlusKids = gCallTree[exiter].cyclesPlusKids + cyclesThisCall;
	gCallTree[exiter].cyclesPlusKids = gCallTree[exiter].cyclesPlusKids - totalCyclesInInterrupts;

	gCallTree[exiter].cyclesSelf = gCallTree[exiter].cyclesSelf + cyclesInMe;

	// using cyclesMin to count time in interrupt handlers
	gCallTree[exiter].cyclesMin = gCallTree[exiter].cyclesMin +
								gCallStack[gCallStackSP].cyclesInInterrupts;

	if ((cyclesInMe - gCallTree[exiter].cyclesMax) > 0)
		gCallTree[exiter].cyclesMax = cyclesInMe;
	
	if (!normalReturn)
		gCallTree[exiter].stackUsed += 10000;

	--gCallStackSP;
	if (gCallStackSP >= 0) {
		gCallStack[gCallStackSP].cyclesInKids = gCallStack[gCallStackSP].cyclesInKids +
												cyclesThisCall;
		gCallStack[gCallStackSP].cyclesInInterruptsInKids =
							gCallStack[gCallStackSP].cyclesInInterruptsInKids +
									 totalCyclesInInterrupts;
	}
	
	return cyclesThisCall;
}


// ---------------------------------------------------------------------------
//		¥ FindOrAddCall
// ---------------------------------------------------------------------------
// FindOrAddCall is used when a function or interrupt is being entered. It
// looks to see if the function has prevously been called from the current
// function or interrupt, and if so returns the existing record. If not, a
// new record is allocated, initialized, and plugged into the tree.

static int FindOrAddCall (int head, emuptr address)
{
	int newR = NORECORD;
	int current = NORECORD;
	int prev = NORECORD;

	if (head == gOverflowRecord)
		return gOverflowRecord;

	if (head == NORECORD)
	{
		newR = gFirstFreeCallRec++;	// head is empty, just create record
	}
	else
	{
		// look for existing
		current = head;
		while (current != NORECORD && gCallTree[current].address != address)
		{
			// Because of the "head == NORECORD" test above, we are guaranteed
			// to enter the loop body at least once, thus initializing "prev".
			// This is important, because we use "prev" later.

			prev = current;
			current = gCallTree[current].sib;
		}

		if (current != NORECORD)	// also returns if current == gOverflowRecord, good!
			return current;

		newR = gFirstFreeCallRec++;
		if (newR >= gMaxCalls)
			return gOverflowRecord;

		gCallTree[prev].sib = newR;
	}

	if (newR >= gMaxCalls)
		return gOverflowRecord;

	EmAssert (	address == ROOTADDRESS ||
				address == INTERRUPTADDRESS ||
				address == OVERFLOWADDRESS ||
				IS_TRAP_32(address) ||
				EmMemCheckAddress (address, 2));

	// fill record
	gCallTree[newR].sib				= NORECORD;
	gCallTree[newR].kid				= NORECORD;
	gCallTree[newR].address			= address;
	gCallTree[newR].entries			= 0;
	gCallTree[newR].cyclesSelf		= 0;
	gCallTree[newR].cyclesPlusKids	= 0;
	gCallTree[newR].cyclesMin		= 0;	// 0xFFFFFFFFFFFFFFFF;
	gCallTree[newR].cyclesMax		= 0;
	gCallTree[newR].stackUsed		= 0;

	return newR;
}


#pragma mark -

//---------------------------------------------------------------------
// Main entry points, exported via Profiling.h
//---------------------------------------------------------------------

Bool ProfileCanInit (void)
{
	return !gProfilingEnabled;
}

Bool ProfileCanStart (void)
{
	return !gProfilingOn;
}

Bool ProfileCanStop (void)
{
	return gProfilingEnabled && gProfilingOn;
}

Bool ProfileCanDump (void)
{
	return gProfilingEnabled;
}


// ---------------------------------------------------------------------------
//		¥ ProfileInit
// ---------------------------------------------------------------------------
// ProfileInit allocates the stack and tree with the passed sizes, and
// initializes a bunch of other data structures.

void ProfileInit(int maxCalls, int maxDepth)
{
	EmAssert (::ProfileCanInit ());

	// initialize globals
	gClockCycles		= 0;
	gReadCycles			= 0;
	gWriteCycles		= 0;

	gReadMismatch		= 0;		// debug
	gWriteMismatch		= 0;		// debug

	gProfilingEnabled	= true;
	gProfilingOn		= false;
	gProfilingCounted	= false;
	gProfilingDetailed	= false;
	gInterruptCycles	= 0;
	gInterruptCount		= 0;
	gMaxCalls			= maxCalls;
	gMaxDepth			= maxDepth;

	gMissedCount		= 0;		// debug
	gExtraPopCount		= 0;		// debug
	gInterruptMismatch	= 0;		// debug

	// initialize call tree
	// Llamagraphics, Inc: Dispose of old gCallTree rather than calling Debugger()

	Platform::DisposeMemory (gCallTree);
	gCallTree = (FnCallRecord*) Platform::AllocateMemory (sizeof (FnCallRecord) * gMaxCalls);

	gFirstFreeCallRec	= 0;
	gExceptionRecord	= FindOrAddCall (NORECORD, INTERRUPTADDRESS);
	gOverflowRecord		= FindOrAddCall (NORECORD, OVERFLOWADDRESS);

	// initialize call stack
	// Llamagraphics, Inc: Dispose of old gCallStack rather than calling Debugger()

	Platform::DisposeMemory (gCallStack);
	gCallStack = (FnStackRecord*) Platform::AllocateMemory (sizeof (FnStackRecord) * gMaxDepth);

	gCallStackSP = 0;

	gCallStack[gCallStackSP].call = gRootRecord = FindOrAddCall (NORECORD, NOADDRESS);
	gCallStack[gCallStackSP].returnAddress = NOADDRESS;
	gCallStack[gCallStackSP].cyclesAtEntry = gClockCycles;
	gCallStack[gCallStackSP].cyclesInKids = 0;
	gCallStack[gCallStackSP].cyclesInInterrupts = 0;
	gCallStack[gCallStackSP].cyclesInInterruptsInKids = 0;

	gProfilingDetailLog = NULL;

	// ¥¥¥ for testing
	// ProfileDetailFn(0x10CA68A0, true);
}


// ---------------------------------------------------------------------------
//		¥ ProfileCleanup
// ---------------------------------------------------------------------------
// ProfileCleanup frees the data structures allocated in ProfileInit.

void ProfileCleanup()
{
	EmAssert (::ProfileCanDump ());

	gProfilingEnabled = false;

	Platform::DisposeMemory (gCallTree);
	Platform::DisposeMemory (gCallStack);

	if (gProfilingDetailLog != NULL)
		fclose (gProfilingDetailLog);
}


// ---------------------------------------------------------------------------
//		¥ ProfileStart
// ---------------------------------------------------------------------------
// ProfileStart turns on the profiling flags. Currently it assumes that it is
// not called from within an interrupt, often is not the case. When invoked
// from the POSER UI, the CPU is often currently processing an interrupt.

void ProfileStart()
{
	// If the system's not initialized, initialize it with
	// default settings.

	if (::ProfileCanInit ())
	{
		::ProfileInit (MAXFNCALLS, MAXDEPTH);
	}


	if (gCallStackSP != 0)			// debug check
		Platform::Debugger ();

	gProfilingOn	= true;
	gInterruptDepth	= -1;
	gInInstruction	= false;
}


// ---------------------------------------------------------------------------
//		¥ ProfileStop
// ---------------------------------------------------------------------------
// ProfileStop turns off collection of profiling data. If there are functions
// currently on the profiling stack, they are popped.

void ProfileStop()
{
	EmAssert (::ProfileCanStop ());

	// pop any functions on stack, updating cycles on the way up
	// stop when gCallStackSP == 0, which matches fake "root" fn

	while (gInterruptDepth >= 0)	// means we stopped profiling in an interrupt...
		ProfileInterruptExit (NOADDRESS);
		
	while (gCallStackSP > 0)
		PopCallStackFn (false);		// doesn't gather stats on the way out

	gProfilingOn = false;
}


// ---------------------------------------------------------------------------
//		¥ ProfilePrint
// ---------------------------------------------------------------------------

void ProfilePrint(const char* fileName)
{
	if (!gProfilingEnabled)
		Platform::Debugger ();

	if (gProfilingOn)
		return;
		
	gCyclesCounted = 	gCallTree[gRootRecord].cyclesPlusKids +
						gCallTree[gExceptionRecord].cyclesPlusKids +
						gCallTree[gOverflowRecord].cyclesPlusKids;
	
	string textFileName;
	if (fileName == NULL)
	{	
		textFileName = CreateFileNameForProfile (true);		// for text = true
		fileName = textFileName.c_str ();
	}

	FILE* resultsLog = fopen (fileName, "w");

	fputs ("index\tparent\tdepth\tfunction name\tcount\tonly cycles\tonly msec\tonly %\tplus kids cycles\tplus kids msec\tplus kids %\taverage msec\tmax msec\tinterrupt msec\tinterrupt count/debug\n", resultsLog);

	RecursiveSortKids (gRootRecord);
	RecursiveSortKids (gExceptionRecord);
	
	PrintBlock (resultsLog, gRootRecord);
	
	// in case ProfilePrint was called on its own, dump out exception and overflow nodes
	// (these will be sibs of gRootRecord if called from ProfileDump)

	if (gCallTree[gRootRecord].sib == NORECORD)
		PrintBlock (resultsLog, gExceptionRecord);

	if (gCallTree[gExceptionRecord].sib == NORECORD)
		PrintBlock (resultsLog, gOverflowRecord);


#if PLATFORM_WINDOWS
	fprintf (resultsLog, "\tcycles counted:\t\t%I64d\n", gCyclesCounted);
	fprintf (resultsLog, "\ttotal clocks:\t\t%I64d\n", gClockCycles);
	fprintf (resultsLog, "\ttotal reads:\t\t%I64d\n", gReadCycles);
	fprintf (resultsLog, "\ttotal writes:\t\t%I64d\n", gWriteCycles);
#else // MAC
	fprintf (resultsLog, "\tcycles counted:\t\t%lld\n", gCyclesCounted);
	fprintf (resultsLog, "\ttotal clocks:\t\t%lld\n", gClockCycles);
	fprintf (resultsLog, "\ttotal reads:\t\t%lld\n", gReadCycles);
	fprintf (resultsLog, "\ttotal writes:\t\t%lld\n", gWriteCycles);
#endif

	fprintf (resultsLog, "\treturn level mis-matches:\t\t%ld\n", gExtraPopCount);
	fprintf (resultsLog, "\tnon-matching returns:\t\t%ld\n", gMissedCount);

	fclose (resultsLog);
}


// ---------------------------------------------------------------------------
//		¥ ScanROMMapFile
// ---------------------------------------------------------------------------
// Process the next line of text from the ROM map file, and return true
// if we found a valid name/address pair.

static Boolean ScanROMMapFile(FILE* iROMMapFile, char* oRoutineName, emuptr* oAddr)
{
	// Read in the next line from the ROM map file, which looks like:
	// 	MenuGetVisible                     $10d06916
	// WARNING - the scan length for the name _must_ match MAX_ROUTINE_NAME

	int numArgs = fscanf (iROMMapFile, " %256s $%lx \n", oRoutineName, oAddr);

	if (numArgs != 2)
	{
		// See if we hit one of the Segment entries, which looks like:
		// Segment "launcherSeg2" size=$10d00e68

		if ((strcmp (oRoutineName, "Segment") == 0) && (numArgs == 1))
		{
			fscanf (iROMMapFile, " %*s size=$%*x \n");
		}
		else
		{
			// DOLATER kwk - this should display an error.
			Platform::Debugger ();
		}

		return false;
	}

	return true;
}


// ---------------------------------------------------------------------------
//		¥ ProfileDump
// ---------------------------------------------------------------------------
// ProfileDump reads in the ROM.map file, turns all the addresses in the call
// tree into real function names, generating the string table in the process,
// then dumps out the header, tree, and stringtable to disk. This operation is
// destructive to the function tree, and cannot be done more than once. (It
// should probably call ProfileCleanup to make sure this doesn't happen.)

void ProfileDump (const char* fileName)
{
	// If profiling is occurring, stop it.

	if (::ProfileCanStop ())
	{
		::ProfileStop ();
	}

	EmAssert (::ProfileCanDump ());

	// Zero this out so that it can be refetched and recached for the
	// current processor.

	gCyclesPerSecond = 0;

	string mwfFileName;
	if (fileName == NULL)
	{
		mwfFileName = CreateFileNameForProfile (false);
		fileName = mwfFileName.c_str ();		// for text = false
	}

	long romRoutineNameBytes = 0;
	long romRoutineNames = 0;
	char romRoutineName[MAX_ROUTINE_NAME + 1];
	emuptr addr;
	
	// DOLATER kwk - try to open <rom name>.map in same folder as current ROM file.
	// If that fails, go for "ROM.map" in same folder as Poser.
	FILE* romMapFile = fopen("ROM.map", "r");
	
	// If we have a ROM map, count the number of routines and the space required
	// to save all of the routine names. Note that previously the max rom function
	// names was set to 3000, but currently we've got 5808 names in a 4.0 build.
	if (romMapFile)
	{
		while (!feof (romMapFile))
		{
			if (ScanROMMapFile(romMapFile, romRoutineName, &addr))
			{
				romRoutineNames += 1;
				romRoutineNameBytes += strlen(romRoutineName) + 1;
			}
		}

		rewind(romMapFile);
	}

	// Read in the names & addresses from the ROM map file
	if (gROMMap != NULL)
		Platform::Debugger();
	gROMMap = (ROMMapRecord*)Platform::AllocateMemory(sizeof(ROMMapRecord) * romRoutineNames);
	gROMMapEnd = 0;
	
	StMemory names (romRoutineNameBytes);
	char *namesEnd = names.Get();

	if (romMapFile)
	{
		while (!feof (romMapFile))
		{
			if (ScanROMMapFile (romMapFile, romRoutineName, &addr))
			{
				if (gROMMapEnd >= romRoutineNames)
				{
					Platform::Debugger ();
				}
				
				gROMMap[gROMMapEnd].address = addr;
				gROMMap[gROMMapEnd++].name = namesEnd;
				strcpy(namesEnd, romRoutineName);
				namesEnd += strlen(romRoutineName) + 1;
			}
		}
		
		fclose (romMapFile);
	}

	// fix up trees a bit (sum cycle counts for root nodes)

	gCallTree[gRootRecord].address = ROOTADDRESS;
	int current = gCallTree[gRootRecord].kid;
	while (current != NORECORD)
	{
		gCallTree[gRootRecord].cyclesPlusKids += gCallTree[current].cyclesPlusKids;
		current = gCallTree[current].sib;
	}

	current = gCallTree[gExceptionRecord].kid;
	while (current != NORECORD)
	{
		gCallTree[gExceptionRecord].cyclesPlusKids += gCallTree[current].cyclesPlusKids;
		current = gCallTree[current].sib;
	}

	// compute total cycles counted

	gCyclesCounted =	gCallTree[gRootRecord].cyclesPlusKids +
						gCallTree[gExceptionRecord].cyclesPlusKids +
						gCallTree[gOverflowRecord].cyclesPlusKids;
	
	// debugging

	if (gCallTree[gRootRecord].sib != NORECORD ||
		gCallTree[gExceptionRecord].sib != NORECORD ||
		gCallTree[gOverflowRecord].sib != NORECORD ||
		gCallTree[gOverflowRecord].kid != NORECORD)
	{
		Platform::Debugger ();
	}

	// fix up top-level records
	gCallTree[gRootRecord].sib = gExceptionRecord;
	gCallTree[gExceptionRecord].sib = gOverflowRecord;

	// free pointer could be really large...
	if (gFirstFreeCallRec > gMaxCalls)
		gFirstFreeCallRec = gMaxCalls;

	// dump out a plain text file too
	char	textName[256];
	strcpy (textName, fileName);
	if (::EndsWith (textName, ".mwp"))
		textName[strlen (textName) - 4] = 0;
	strcat (textName, ".txt");
	EmFileRef textRef (textName);
	ProfilePrint (textRef.GetFullPath().c_str());

	// munge all the addresses to produce the string table
	InitStringTable();

	// Llamagraphics, Inc:  Replaced RecursiveAddressToString() with
	// LinearAddressToString(), since the recursive version was blowing
	// the stack on the Macintosh.
	LinearAddressToStrings();

	// do a little cleanup now
	Platform::DisposeMemory (gROMMap);

	// create the header blocks
	ProfFileHeader header;

	header.proF = 'proF';
	header.version = 0x00040002;
	header.fnCount = gFirstFreeCallRec;
	header.four = 0x00000004;

	header.zeros1 = 0x00000000;
	header.zeros2 = 0x00000000;
	header.unknown = 0x00000000;

//	header.recordsSize = sizeof(ProfFileHeader) + gFirstFreeCallRec * sizeof(FnCallRecord);
	header.recordsSize = kProfFileHeaderSize + gFirstFreeCallRec * kFnCallRecordSize; // gregs
	header.stringTableSize = gStringTableEnd;
	header.overhead = gClockCycles - gCyclesCounted;
	header.rootRec = gRootRecord;

	header.sixtyfour1 = 0x00000064;
	header.sixtyfour2 = 0x00000064;
	header.countsPerTime = PrvGetCyclesPerSecond ()/100;	// times will be shown in milliseconds

	header.oddstuff0 = 0x00000000;
	header.oddstuff1 = 0x00000000;
	header.oddstuff2 = 0x00000000;
	header.oddstuff3 = 0x00000000;
	header.oddstuff4 = 0x00000000;

	char	profName[256];
	strcpy (profName, fileName);
	if (!::EndsWith (profName, ".mwp"))
		strcat (profName, ".mwp");

	EmFileRef		profRef (profName);
	EmStreamFile	stream (profRef, kCreateOrEraseForUpdate,
							kFileCreatorCodeWarriorProfiler, kFileTypeProfile);

//	stream.PutBytes (&header, sizeof(ProfFileHeader));
//	stream.PutBytes (gCallTree, sizeof(FnCallRecord) * gFirstFreeCallRec);

	// We can't just write sizeof(...) with a pointer to
	// the structure because the structure alignment is
	// different for different platforms / architectures, etc.
	// The Metrowerks Profiler format is big endian.

	// Write out header

	stream << header.proF;
	stream << header.version;
	stream << header.fnCount;
	stream << header.four;

	stream << header.zeros1;
	stream << header.zeros2;
	stream << header.unknown;

	stream << header.recordsSize;
	stream << header.stringTableSize;
	stream << header.overhead;
	stream << header.rootRec;

	stream << header.sixtyfour1;
	stream << header.sixtyfour2;
	stream << header.countsPerTime;

	stream << header.oddstuff0;
	stream << header.oddstuff1;
	stream << header.oddstuff2;
	stream << header.oddstuff3;
	stream << header.oddstuff4;

	while (stream.GetMarker () != kProfFileHeaderSize)
		stream << (uint8) 0;

	// Write out the function records

	for (int ii = 0; ii < gFirstFreeCallRec; ++ii)
	{
		stream << gCallTree[ii].address;
		stream << gCallTree[ii].entries;

		stream << gCallTree[ii].cyclesSelf;
		stream << gCallTree[ii].cyclesPlusKids;
		stream << gCallTree[ii].cyclesMin;
		stream << gCallTree[ii].cyclesMax;

		stream << gCallTree[ii].sib;
		stream << gCallTree[ii].kid;
		stream << gCallTree[ii].stackUsed;
	}

	// Write out the string table.

	stream.PutBytes (gStringTable, gStringTableEnd);

	// cleanup

	::CleanupStringTable ();
	
	// Llamagraphics, Inc:  After dumping, the gCallTree[i].address slots are
	// offsets into the string table rather than machine addresses.  This
	// means that the profiling information can't continue to be used, so
	// let's dispose of it now and remove the danger of it being misinterpreted.

	::ProfileCleanup ();
}



#pragma mark -

// ---------------------------------------------------------------------------
//		¥ ProfileFnEnter
// ---------------------------------------------------------------------------
// ProfileFnEnter is called by the emulator when a JSR is executed, and at a
// couple of other places. It creates a record for the function (or reuses an
// existing record), and updates the profiler's call stack.
//
// ProfileFnEnter also supports a special profiling mode. If the profiler is
// built with PROFILE_ONE_FN on, then it's assumed that whenever profiling has
// been initialized, you always want to start profiling when a specified
// function is entered, and stop profiling when that function is exited. That
// way you get profiling information for a particular function no matter who
// it's called from. In this case, ProfileFnEnter may be called before
// ProfileStart has been called, and if it detects that the target function
// has been entered it will start profiling at that point.

void ProfileFnEnter(emuptr destAddress, emuptr returnAddress)
{
	if (!gProfilingOn)
	{
#if PROFILE_ONE_FN
		if (address == FNTOPROFILE)
			ProfileStart ();
		else
			return;
#else
		return;
#endif
	}

	if (gInInstruction)
		ProfileInstructionExit(NOADDRESS);

	// If destAddress contains a trapword, save the trapword along
	// with some other information that might be useful (like the
	// library reference number if we're calling a library function).
	if (IS_TRAP_16(destAddress))
	{
		if (IsSystemTrap (destAddress))
		{
			uint16	trapWord = 0xA000 | SysTrapIndex (destAddress);

			if (trapWord == sysTrapIntlDispatch		||
				trapWord == sysTrapOmDispatch			||
				trapWord == sysTrapTsmDispatch		||
				trapWord == sysTrapFlpDispatch		||
				trapWord == sysTrapFlpEmDispatch		||
				trapWord == sysTrapSerialDispatch)
			{
				destAddress = PACK_TRAP_INFO(destAddress, m68k_dreg (regs, 2));
			}
			else
			{
				destAddress = PACK_TRAP_INFO(destAddress, 0);
			}
		}
		else
		{
			destAddress = PACK_TRAP_INFO(destAddress, EmMemGet16 (m68k_areg (regs, 7)));
		}
	}
	
	// Check for jumping to the IntlDispatch routine dispatcher. If so, then record
	// the address of the routine that we're going to be dispatching to, versus the
	// dispatcher itself.
	
	// DOLATER kwk - also do this for other dispatched traps (see above for the list)
	// Perhaps it would make more sense to set state when we hit one of these traps,
	// and then when this routine gets called (as a result of handling the RTE),
	// we could be smarter and more efficient about knowing that a different address
	// needed to be calculated.

	else
	{
		emuptr realDestAddress = GetIntlDispatchAddress (destAddress, m68k_dreg (regs, 2));
		if (realDestAddress != EmMemNULL)
		{
			destAddress = realDestAddress;
		}
	}

	// get the caller fn 
	int caller = gCallStack[gCallStackSP].call;
	
	// debug check, watch for overflow
	if (gCallStackSP >= gMaxDepth-1)
		Platform::Debugger();

	// push record for callee
	gCallStack[++gCallStackSP].returnAddress = returnAddress;
	gCallStack[gCallStackSP].cyclesAtEntry = gClockCycles;
	gCallStack[gCallStackSP].cyclesInKids = 0;
	gCallStack[gCallStackSP].cyclesInInterrupts = 0;
	gCallStack[gCallStackSP].cyclesInInterruptsInKids = 0;
	gCallStack[gCallStackSP].call = FindOrAddCall (gCallTree[caller].kid, destAddress);
	
	// check if this was first kid and update parent
	if (gCallTree[caller].kid == NORECORD && gCallStack[gCallStackSP].call != gOverflowRecord)
		gCallTree[caller].kid = gCallStack[gCallStackSP].call;
}


// ---------------------------------------------------------------------------
//		¥ ProfileFnExit
// ---------------------------------------------------------------------------
// ProfileFnExit is called by the emulator when an RTS is executed The Palm OS
// does some funny things such that there is not always an RTS for every JSR.
// To handle this, ProfileFnExit has to be smart, so it keeps the return
// address when a function is called and compares it to the destination
// address for the RTS. Most of the time they match, and everything is fine,
// but when they don't one of two things may be happening: either a function
// is being called not by using JSR, but rather by pushing the callee's
// address on the stack then executing an RTS, or a RTS has been skipped and
// the stack has been cleaned up by the executing code (the kernel does this.)
//
// If the return address is not what we expected based on the profiling stack,
// then the first thing we do is walk up the stack looking to see if the
// address matches some higher function. If it does, the stack is popped up to
// the function we expect, and the intervening functions are flagged as
// improperly exited. If no match was found, we assume it's a long jump
// disguised as an RTS, and call ProfileFnEnter instead.
//
// The profiler's call stack can (commonly) be popped to empty. This happens
// when the function that was executing when ProfileStart was called exits. If
// profiling is started from the POSER UI, this happens very often, so we do
// the best we can: create a new fake "root" for the call tree, representing
// the function we're returning to, and continue on. If profiling was started
// by the PROFILE_ONE_FN trick, then a pop to empty is considered exiting the
// target function, and profiling is turned off.

void ProfileFnExit(emuptr returnAddress, emuptr oldAddress)
{
	if (!gProfilingOn)
		return;

	if (gInInstruction)
		ProfileInstructionExit(NOADDRESS);

	// sanity check, try to make sure the returns match the calls we think they do
	if (gCallStack[gCallStackSP].returnAddress != NOADDRESS
	&& gCallStack[gCallStackSP].returnAddress != returnAddress)
	{
		int trySP = gCallStackSP;
		while (--trySP >= 0)
			if (returnAddress == gCallStack[trySP].returnAddress)
				break;

		if (trySP >= 0)
		{
			// found a match on stack, so pop up to it
			while (gCallStackSP > trySP)
			{
				gExtraPopCount++;		// debug, count extra pops
				PopCallStackFn(false);
			}
		}
		else if (gCallStackSP != 0)
		// could be a longjump disguised as an RTS, so push instead
		{
			ProfileFnEnter(returnAddress, oldAddress);
			gCallTree[gCallStack[gCallStackSP].call].stackUsed += 100000;	// mark it as wierdly called
			gMissedCount++;				// debug, count unmatched returns
			return;
		}
	}

	PopCallStackFn(true);

#if PROFILE_ONE_FN
	// if we started on entering a function, stop when it exits
	if (gCallStackSP == 0)
		ProfileStop();
#else		
	// or an alternate thing to do if we pop too far, re-root and continue
	if (gCallStackSP < 0)
	{
		// uh oh, we must have exited the fn we started profiling in!
		// make a new root to handle the new enclosing function
		gRootRecord = FindOrAddCall (NORECORD, NOADDRESS);
		if (gRootRecord == gOverflowRecord)
			Platform::Debugger();
		gCallTree[gRootRecord].kid = gCallStack[0].call;	// old root
		gCallStack[0].returnAddress = NOADDRESS;
		gCallStack[0].call = gRootRecord;
		// gCallStack[0].cyclesAtEntry = ... don't change, it's the time profiling was started
		gCallStack[0].cyclesInKids = gClockCycles - gCallStack[0].cyclesAtEntry;
		gCallStack[0].cyclesInInterrupts = 0;
		gCallStack[0].cyclesInInterruptsInKids = 0;
		gCallStackSP = 0;
	}
#endif
}


// ---------------------------------------------------------------------------
//		¥ ProfileInterruptEnter
// ---------------------------------------------------------------------------
// ProfileInterruptEnter does almost the same thing as ProfileFnEnter. The
// difference is that the current call is added to the "interrupt" root rather
// than the currently executing function, and the profile's call stack pointer
// is pushed onto the profiler's interrupt stack.

void ProfileInterruptEnter(int32 iException, emuptr returnAddress)
{
	if (!gProfilingOn)
		return;

	if (gInInstruction)
		ProfileInstructionExit(NOADDRESS);

	// get the caller fn 
	gCallStack[gCallStackSP].cyclesAtInterrupt = gClockCycles;
	gInterruptCount++;
	gInterruptStack[++gInterruptDepth] = gCallStackSP;	// mark the fn that was interrupted

	// debug check, watch for overflow
	if (gCallStackSP >= gMaxDepth-1)
		Platform::Debugger();

	// push record for callee
	gCallStack[++gCallStackSP].returnAddress = returnAddress;
	gCallStack[gCallStackSP].cyclesAtEntry = gClockCycles;
	gCallStack[gCallStackSP].cyclesInKids = 0;
	gCallStack[gCallStackSP].cyclesInInterrupts = 0;
	gCallStack[gCallStackSP].cyclesInInterruptsInKids = 0;
	gCallStack[gCallStackSP].call = FindOrAddCall (gCallTree[gExceptionRecord].kid, iException);
	// check if this was first exception and update gExceptionRecord
	if (gCallTree[gExceptionRecord].kid == NORECORD && gCallStack[gCallStackSP].call != gOverflowRecord)
		gCallTree[gExceptionRecord].kid = gCallStack[gCallStackSP].call;
}


// ---------------------------------------------------------------------------
//		¥ ProfileInterruptExit
// ---------------------------------------------------------------------------
// ProfileInterruptExit works much like ProfileFnExit, the calls stack's
// record for the interrupt "function" is popped. Unlike with RTS, RTE is
// reliably matched with an interrupt exit, so it's not necessary to do the
// funny call stack searching. However, when the RTE executes it's possible
// that there may still be some unmatched calls (JSRs) that were executed as
// part of processing the interrupt. (The kernel does this regularly.) So we
// make sure to pop all the functions off the profiler's call stack until we
// get to the one indicated by the profiler's interrupt stack.
//
// The Palm OS trap handler performs trap calls by pushing stuff on the CPU
// stack so that the RTE will jump to the real function (and then later be
// able to do an RTS from that fn to get back to the code that executed the
// TRAP interrupt.) To handle this, the RTE destination is compared to the PC
// that was saved when the interrupt happened, and if they addresses don't
// match then ProfileFnEnter is called to track the "fake" function entry.

void ProfileInterruptExit(emuptr returnAddress)
{
	int32 needsFakeCall = NOADDRESS;

	if (!gProfilingOn)
		return;

	if (gInInstruction)
		ProfileInstructionExit(NOADDRESS);

	if (gInterruptDepth < 0)		// means we started profiling in an interrupt... oh well
		return;

	if (gCallStackSP <= gInterruptStack[gInterruptDepth])
		Platform::Debugger();

	while (gCallStackSP > gInterruptStack[gInterruptDepth]+1)
	{
		// mismatched jsr/rts set inside interrupt handler, deal with it
		gExtraPopCount++;		// debug, count extra pops
		PopCallStackFn(false);
	}

	// sanity check, try to make sure the returns match the calls we think they do
	if (gCallStack[gCallStackSP].returnAddress != returnAddress)
	{
		// trap handler is sneaky, and pushes stuff on the stack so that the RTE jumps
		// to the implementation of the trap.  Detect this by noticing the RTE address didn't
		// match and generating a fake function call.
		if (gCallTree[gCallStack[gCallStackSP].call].address == 0x2F)
			needsFakeCall = gCallStack[gCallStackSP].returnAddress + 2;	// +2 to skip trap word
		else
			gInterruptMismatch++;
	}

	// pop the interrupt block
	int64 cyclesThisCall = PopCallStackFn(true);
	EmAssert(gCallStackSP >= 0);

	gInterruptDepth--;
	gInterruptCycles += cyclesThisCall;

	// store interrupt time so we can subtract it later from the cycles for the function itself
	gCallStack[gCallStackSP].cyclesInInterrupts += cyclesThisCall;
	gCallTree[gCallStack[gCallStackSP].call].stackUsed += 1;	// count other interrupts this way

	// trap interrupt
	if (needsFakeCall != NOADDRESS)
		ProfileFnEnter(returnAddress, needsFakeCall);
}



void ProfileDetailFn(emuptr addr, int logInstructions)
{
	::FindFunctionName(addr, NULL, &gDetailStartAddr, &gDetailStopAddr, 0);

	gProfilingDetailed = true;
	
	if (logInstructions && gProfilingDetailLog == NULL)
	{
		gProfilingDetailLog = fopen("ProfilingDetail.txt", "w");
		fputs("PC\topcode\tinstruction:\tclocks\treads\twrites\ttotal:\tclocks\treads\twrites\t", gProfilingDetailLog);
	}
}



void ProfileInstructionEnter(emuptr instructionAddress)
{
	if (!gProfilingOn)
		return;

	if (instructionAddress < gDetailStartAddr ||
		 instructionAddress > gDetailStopAddr)
		return;

	// get the caller fn 
	int caller = gCallStack[gCallStackSP].call;
	
	// debug check, watch for overflow
	if (gCallStackSP >= gMaxDepth-1)
		Platform::Debugger();

	// push record for instruction
	gCallStack[++gCallStackSP].returnAddress = instructionAddress;
	gCallStack[gCallStackSP].cyclesAtEntry = gClockCycles;
	gCallStack[gCallStackSP].cyclesInKids = 0;
	gCallStack[gCallStackSP].cyclesInInterrupts = 0;
	gCallStack[gCallStackSP].cyclesInInterruptsInKids = 0;
	gCallStack[gCallStackSP].call = FindOrAddCall (gCallTree[caller].kid, instructionAddress);
	gCallStack[gCallStackSP].opcode = get_iword(0);
	
	// check if this was first kid and update parent
	if (gCallTree[caller].kid == NORECORD && gCallStack[gCallStackSP].call != gOverflowRecord)
		gCallTree[caller].kid = gCallStack[gCallStackSP].call;
	
	gClockCyclesSaved = gClockCycles;
	gReadCyclesSaved = gReadCycles;
	gWriteCyclesSaved = gWriteCycles;
	
	gInInstruction = true;
}


void ProfileInstructionExit(emuptr instructionAddress)
{
	if (!gProfilingOn)
		return;
	
	if (!gInInstruction)
		return;

	if (instructionAddress == NOADDRESS)
		instructionAddress = gCallStack[gCallStackSP].returnAddress;
		
	// sanity check, try to make sure the returns match the calls we think they do
	if (gCallStack[gCallStackSP].returnAddress != instructionAddress)
		Platform::Debugger();

	if (gProfilingDetailLog != NULL)
	{
		char hackBuffer[512];
		sprintf(hackBuffer,
			"$%08lX\t$%04X\t \t%lld\t%lld\t%lld\t \t%lld\t%lld\t%lld\n",
				instructionAddress,
				gCallStack[gCallStackSP].opcode,
				gClockCycles - gClockCyclesSaved,
				gReadCycles - gReadCyclesSaved,
				gWriteCycles - gWriteCyclesSaved,
				gClockCycles,
				gReadCycles,
				gWriteCycles);
		
		if ((gClockCyclesLast - gClockCyclesSaved) != 0)
			fputs("\n", gProfilingDetailLog);
		
		if (instructionAddress == gDetailStartAddr)
			fputs("\t-->\tJSR\n", gProfilingDetailLog);
			
		fputs(hackBuffer, gProfilingDetailLog);

	if (gCallStack[gCallStackSP].opcode  == kOpcode_RTS)	// RTS
		fputs("\t<--\tRTS\n", gProfilingDetailLog);
			
		gClockCyclesLast = gClockCycles;
	}
	
	PopCallStackFn(true);
	
	gInInstruction = false;
}



#pragma mark -

void ProfileTest()
{
	ProfileInit(15,5);
	ProfileStart();
	ProfileIncrementClock(110 * PrvGetCyclesPerSecond ());
	
	ProfileInterruptExit(0x55555555);
	ProfileIncrementClock(110 * PrvGetCyclesPerSecond ());
	
	ProfileFnEnter(0xAAAAAAAA, 0);	// new
		ProfileIncrementClock(100 * PrvGetCyclesPerSecond ());
		ProfileFnEnter(0xBBBBBBBB, 0);	// new kid
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnEnter(0xCCCCCCCC, 0);		// new kid
				ProfileIncrementClock(1 * PrvGetCyclesPerSecond ());
				ProfileFnExit(0, 0);
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnEnter(0xDDDDDDDD, 0);		// new sib at end
				ProfileIncrementClock(1 * PrvGetCyclesPerSecond ());
				ProfileFnExit(0, 0);
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnEnter(0xCCCCCCCC, 0);		// same kid
				ProfileIncrementClock(1 * PrvGetCyclesPerSecond ());
				ProfileFnExit(0, 0);
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnExit(0, 0);
		ProfileIncrementClock(100 * PrvGetCyclesPerSecond ());
		ProfileFnEnter(0xBBBBBBBB, 0);	// same kid
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnEnter(0xCCCCCCCC, 0);		// new kid
				ProfileIncrementClock(1 * PrvGetCyclesPerSecond ());
				ProfileFnExit(0, 0);
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnExit(0, 0);
		ProfileIncrementClock(100 * PrvGetCyclesPerSecond ());
		ProfileFnEnter(0xDDDDDDDD, 0);	// new sib at end
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnEnter(0xCCCCCCCC, 0);		// new kid
				ProfileIncrementClock(1 * PrvGetCyclesPerSecond ());
				ProfileFnExit(0, 0);
	
	ProfileIncrementClock(120 * PrvGetCyclesPerSecond ());
	ProfileInterruptEnter(0x11111111, 0);	// interrupt
		ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
		ProfileFnEnter(0x22222222, 0);		// new kid
			ProfileIncrementClock(1 * PrvGetCyclesPerSecond ());
			ProfileFnExit(0, 0);
		ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
		ProfileInterruptExit(0);

			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnExit(0, 0);
		ProfileIncrementClock(100 * PrvGetCyclesPerSecond ());
		ProfileFnEnter(0xCCCCCCCC, 0);	// new kid in middle
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnExit(0, 0);
		ProfileIncrementClock(100 * PrvGetCyclesPerSecond ());
		ProfileFnEnter(0xAAAAAAAA, 0);	// new kid at beginning
			ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());
			ProfileFnExit(0, 0);
		ProfileIncrementClock(100 * PrvGetCyclesPerSecond ());
		ProfileFnExit(0, 0);
	ProfileIncrementClock(100 * PrvGetCyclesPerSecond ());
	// ProfileFnExit(0);
	// ProfileIncrementClock(100 * PrvGetCyclesPerSecond ());
	// ProfileFnExit(0);

	ProfileInterruptEnter(0x99999999, 0);	// interrupt
	ProfileIncrementClock(10 * PrvGetCyclesPerSecond ());

	ProfileStop();
	ProfileDump(NULL);
	ProfileCleanup();
}

#if _DEBUG

void ProfileIncrementClock(unsigned long by)
{
	gClockCycles += by;
}

void ProfileIncrementRead(int reads, int waitstates)
{
	gClockCycles += reads * (4 + waitstates);
	gReadCycles += reads;
}

void ProfileIncrementWrite(int writes, int waitstates)
{
	gClockCycles += writes * (4 + waitstates);
	gWriteCycles += writes;
}

#endif


/***********************************************************************
 *
 * FUNCTION:	CreateFileNameForProfile
 *
 * DESCRIPTION:	Creates a file reference based on the base file name
 *				passed in to the constructor. A new file reference is
 *				always created.
 *
 * PARAMETERS:	none
 *
 * RETURNED:	nothing, but fileName is set properly as a side effect
 *
 ***********************************************************************/

string CreateFileNameForProfile (Bool forText)
{
	EmFileRef	result;
	char		buffer[32];

	EmDirRef	poserDir = EmDirRef::GetEmulatorDirectory ();
	
	// Look for an unused file name.

	UInt32 fileIndex = 0;

	do
	{
		++fileIndex;

		if (forText)
			sprintf (buffer, "%s_%04ld.txt", "Profile Results", fileIndex);
		else
			sprintf (buffer, "%s_%04ld.mwp", "Profile Results", fileIndex);

		result = EmFileRef (poserDir, buffer);
	}
	while (result.IsSpecified () && result.Exists ());

	return buffer;
}
