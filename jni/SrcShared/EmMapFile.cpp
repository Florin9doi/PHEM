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
#include "EmMapFile.h"

#include "ChunkFile.h"			// Chunk, EmStreamChunk
#include "EmFileRef.h"			// EmFileRef
#include "EmStreamFile.h"		// EmStreamFile
#include "Miscellaneous.h"		// Strip

Bool
EmMapFile::Read (const EmFileRef& f, StringStringMap& mapData)
{
	try
	{
		EmStreamFile	fileStream (f, kOpenExistingForRead | kOpenText);	// Will throw if fnf
		return Read (fileStream, mapData);
	}
	catch (...)
	{
		return false;
	}

	return true;
}

Bool
EmMapFile::Read (EmStream& stream, StringStringMap& mapData)
{
	try
	{
		StringStringMap	theData;
		string			line;
		char			ch;
		ErrCode			err;

		do
		{
			err = stream.GetBytes (&ch, 1);

			if (ch == '\n' || ch == '\r' || err != 0)		// End of line or end of file.
			{
				// Skip leading WS
				line = Strip (line.c_str (), " \t", true, false);

				// Skip comment and empty lines
				if ((line[0] == ';' ) || (line[0] == '#' ) || (line[0] == '\0'))
				{
					line.erase ();
					continue;
				}

				// Everything else is data
				string::size_type	p = line.find ('=');
				if (p == string::npos)
				{
					line.erase ();
					continue;
				}

				// Split up the line into key/value sections and
				// add it to our hash table.
				string	key (line, 0, p);
				string	value (line, p + 1, line.size ());
				
				key		= Strip (key.c_str (), " \t", true, true);
				value	= Strip (value.c_str (), " \t", true, true);

				theData[key] = value;
				
				line.erase ();
			}
			else
			{
				line += ch;
			}
		}
		while (err == 0);

		mapData = theData;
	}
	catch (...)
	{
		return false;
	}

	return true;
}



Bool
EmMapFile::Write (const EmFileRef& f, const StringStringMap& mapData)
{
	try
	{
		// Get the data in fairly raw form.

		Chunk			chunk;
		EmStreamChunk	chunkStream (chunk);

		if (!Write (chunkStream, mapData))
			return false;

		// Now write that data to the file, letting the EmStreamFile
		// class sort out line endings.

		EmStreamFile	fileStream (f, kCreateOrEraseForUpdate | kOpenText,
								kFileCreatorCodeWarrior, kFileTypeText);	// Will throw if can't create
		return fileStream.PutBytes (chunk.GetPointer (), chunk.GetLength ());
	}
	catch (...)
	{
		return false;
	}

	return true;
}


Bool
EmMapFile::Write (EmStream& stream, const StringStringMap& mapData)
{
	StringStringMap::const_iterator	iter;

	for (iter = mapData.begin(); iter != mapData.end(); ++iter)
	{
		string	line = iter->first + "=" + iter->second + "\n";
		stream.PutBytes (line.c_str (), line.size ());
	}

	return true;
}
