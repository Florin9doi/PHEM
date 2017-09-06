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

#ifndef EmMapFile_h
#define EmMapFile_h

#include "EmStructs.h"		// StringStringMap

class EmFileRef;
class EmStream;

class EmMapFile
{
	public:

		static Bool		Read		(	const EmFileRef& f,
										StringStringMap& mapData );
		static Bool		Read		(	EmStream& stream,
										StringStringMap& mapData );

		static Bool		Write		(	const EmFileRef& f,
										const StringStringMap& mapData );
		static Bool		Write		(	EmStream& stream,
										const StringStringMap& mapData );
};

#endif	/* EmMapFile_h */
