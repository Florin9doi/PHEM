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

#ifndef __GREMLINSSTUBS_H__
#define __GREMLINSSTUBS_H__

#ifdef __cplusplus
extern "C" {
#endif


extern void	StubAppEnqueuePt(const PointType* pen);

extern void StubAppEnqueueKey (UInt16 ascii, UInt16 keycode, UInt16 modifiers);

extern void StubAppGremlinsOn (void);

extern void StubAppGremlinsOff (void);

extern void StubViewDrawLine (int xStart, int yStart, int xEnd, int yEnd);

extern void StubViewDrawPixel (int xPos, int yPos);


#ifdef __cplusplus 
}
#endif

#endif	// __GREMLINSSTUBS_H__
