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

#ifndef _CGREMLINS_H_
#define _CGREMLINS_H_

#include	"EmStream.h"
#include	"EmStructs.h"		// GremlinEventList

class SessionFile;


extern unsigned long int gGremlinNext;

class Gremlins
{
public:
				Gremlins();
				~Gremlins();

	void		New(const GremlinInfo& info);

	Boolean		IsInitialized() const;
	void		Initialize(UInt16 newNumber = -1, UInt32 untilStep = -1, UInt32 finalVal = -1);
	void		Reset (void);

	void		Save(SessionFile &f);
	Boolean		Load(SessionFile &f);

	void		GStatus(UInt16 *currentNumber, UInt32 *currentStep,
					UInt32 *currentUntil);
	Boolean		SetSeed(UInt32 newSeed);
	void		SetUntil(UInt32 newUntil);
	void		RestoreFinalUntil(void);

	void 		Step();
	void		Resume();
	void		Stop();

	Boolean		GetFakeEvent();
	void		GetPenMovement();
	Boolean 	SendCharsToType();

	void		BumpCounter();

	UInt32	GetStartTime()		{ return gremlinStartTime; }
	UInt32	GetStopTime()		{ return gremlinStopTime;  }


private:

	UInt16		keyProbabilitiesSum;	// The sum of all the key possiblities.
//	PointType	lastPoint;				// The previous point enqueued to TD.
	Int16		lastPointX, lastPointY;
	Boolean		lastPenDown;			// TRUE - last point caused a penDownEvent, FALSE - pen is up or already down.
	UInt16		number;					// the random seed (-1 means not started)
	UInt32		counter;				// The number of "events" produced by Gremlins.
	UInt32		until;					// The maximum number of "events" to produce.
	UInt32		finalUntil;				// As above, but not manipulated, as until is by Hordes
	UInt32		saveUntil;
	Boolean		inited;					// TRUE - Gremlins has been initialized, FALSE - not inited.
	Boolean		catchUp;				// TRUE - Event loop needs time to catch up, FALSE - event loop doesn't need time.
	Boolean		needPenUp;				// TRUE - A pen up need to be enqueued, FALSE - pen up doesn't need to be  enqueued.
	char		charsToType[2048];		// Chars to send to the emulator.

	UInt32		gremlinStartTime;
	UInt32		gremlinStopTime;
};


#ifdef __cplusplus
extern "C" {
#endif

extern void GremlinsSendEvent (void);

extern void GremlinsProcessPacket (void* pktBodyP);

#ifdef __cplusplus 
}
#endif


#endif /* _CGREMLINS_H_ */
