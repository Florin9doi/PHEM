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
#include "EmTRG.h"

#include "EmBankRegs.h"
#include "EmRegsEZTRGpro.h"
#include "EmRegs330CPLD.h"
#include "EmRegsVZHandEra330.h"
#include "EmHandEraCFBus.h"
#include "EmHandEraSDBus.h"
#include "EmTRGCF.h"
#include "EmTRGSD.h"


/***********************************************************************
 *
 * FUNCTION:    OEMCreateTRGRegObjs
 *
 * DESCRIPTION: This function is called by EmDevice::CreateRegs for TRG
 *              devices
 *
 * PARAMETERS:  'fHardwareSubID' specifies the device
 *
 * RETURNED:    None
 *
 ***********************************************************************/
void OEMCreateTRGRegObjs(long hardwareSubID)
{
    CFBusManager *          fCFBus;
    HandEra330PortManager * fPortMgr;

    switch (hardwareSubID)
    {
	    case hwrTRGproID :
        default :
            EmBankRegs::AddSubBank (new EmRegsEZTRGpro(&fCFBus));
  	        EmBankRegs::AddSubBank (new EmRegsCFMemCard(fCFBus));
            break;
        case hwrTRGproID + 1 :
	        EmBankRegs::AddSubBank (new EmRegsVZHandEra330(&fPortMgr));
	        EmBankRegs::AddSubBank (new EmRegs330CPLD(fPortMgr));
            EmBankRegs::AddSubBank (new EmRegsCFMemCard(&fPortMgr->CFBus));
            break;
        }
}
