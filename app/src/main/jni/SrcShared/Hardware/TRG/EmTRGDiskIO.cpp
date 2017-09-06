/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmTRGDiskIO.h"

/************************************************************************
 * This class handles the generic low level disk access.
 ************************************************************************/
#include <stdio.h>

#include "EmCommon.h"
#include "EmTRGDiskIO.h"

#define CFFILE_NAME   "trgdrv.dat"
#define SDFILE_NAME   "trgdrvsd.dat"

#define SECTOR_SIZE   512


EmTRGDiskIO::EmTRGDiskIO()
{
    m_driveNo = UNKNOWN_DRIVE;
}


EmTRGDiskIO::~EmTRGDiskIO()
{
}


char * EmTRGDiskIO::GetFilePath(int driveNo)
{
#if PLATFORM_WINDOWS
	static char tmp[MAX_PATH];

    if (driveNo == CF_DRIVE)
	    _snprintf(tmp, sizeof(tmp), "%s\\%s", getenv("WINDIR"), CFFILE_NAME);
    else
	    _snprintf(tmp, sizeof(tmp), "%s\\%s", getenv("WINDIR"), SDFILE_NAME);
	return(tmp);
#else
    if (driveNo == CF_DRIVE)
    	return CFFILE_NAME;
    else
    	return SDFILE_NAME;
#endif
}

int EmTRGDiskIO::Format(void)
{
    EmSector *buffer;
    uint32    num, lba;
    FILE      *fp;

   	fp = fopen(GetFilePath(m_driveNo), "wb");
    if (fp == NULL)
        return -1;

    buffer = new EmSector;
	num = m_currDisk.GetNumSectors(m_diskTypeID);
	for (lba=0; lba<num; lba++)
	{
		m_currDisk.GetSector(m_diskTypeID, lba, buffer);		                   

		// The most probable error condition is
		// attempting to write to a full drive ... it could also
		// be write-protected, or on a disconnected network drive.
		if (fwrite(buffer, SECTOR_SIZE, 1, fp) == 0)
        {
            delete buffer;
            fclose(fp);
			return -1;
        }
	}
    delete buffer;

	return 0;
}

int EmTRGDiskIO::Read(uint32 sectorNum, void *buffer)
{
    FILE *fp;

    if ((fp = fopen(GetFilePath(m_driveNo), "rb")) == NULL)
        return -1;

    if (fseek(fp, sectorNum * SECTOR_SIZE, SEEK_SET) == -1)
    {
        fclose(fp);
        return -1;
    }

    if (fread(buffer, SECTOR_SIZE, 1, fp) != 1)
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    return 0;
}

int EmTRGDiskIO::Write(uint32 sectorNum, void *buffer)
{
    FILE *fp;

    if ((fp = fopen(GetFilePath(m_driveNo), "r+b")) == NULL)
        return -1;

    if (fseek(fp, sectorNum * SECTOR_SIZE, SEEK_SET) == -1)
    {
        fclose(fp);
        return -1;
    }

    if (fwrite(buffer, SECTOR_SIZE, 1, fp) != 1)
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    return 0;
}


void EmTRGDiskIO::Initialize(EmDiskTypeID DiskTypeID, int driveNo)
{
    m_diskTypeID = DiskTypeID;
    m_driveNo = driveNo;
}

void EmTRGDiskIO::Dispose(void)
{
}

int EmTRGDiskIO::ReadSector(uint32 sectorNum, void *buffer)
{
    int retval;

    if ((retval = Read(sectorNum, buffer)) != 0)
    {
        Format();
        retval = Read(sectorNum, buffer);
    }

    return retval;
}



int EmTRGDiskIO::WriteSector(uint32 sectorNum, void *buffer)
{
    int retval;

    if ((retval = Write(sectorNum, buffer)) != 0)
    {
        Format();
        retval = Write(sectorNum, buffer);
    }

    return retval;
}
