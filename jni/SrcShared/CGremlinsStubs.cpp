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
#include "CGremlinsStubs.h"

#include "Platform.h"			// Platform::ViewDrawLine
#include "ROMStubs.h"			// EvtEnqueuePenPoint, EvtEnqueueKey


// ---------------------------------------------------------------------------
//		¥ StubEnqueuePt
// ---------------------------------------------------------------------------
// This is a stub routine that called the application object's method that
// enqueues a point.

void	StubAppEnqueuePt(const PointType* pen)
{
	// Make a copy of the point, as we may be munging it.

	PointType	pt = *pen;

	// Enqueue the new pen position. We must "reverse" correct it because the
	// Event Manager assumes that all points enqueued are raw digitizer points.

	if (pt.x != -1 || pt.y != -1)
	{
		(void) ::PenScreenToRaw(&pt);
	}

	::EvtEnqueuePenPoint(&pt);
}


// ---------------------------------------------------------------------------
//		¥ StubAppEnqueueKey
// ---------------------------------------------------------------------------
// This is a stub routine that called the Application object's method that
// enqueues a key.

void StubAppEnqueueKey (UInt16 ascii, UInt16 keycode, UInt16 modifiers)
{
	switch (ascii)
	{
		// This translates some control characters into system chars.
		// Following is a list of such system chars and whether or
		// now we support them specially.
		//
		//	* = Handled here
		//	! = Handled elsewhere
		//	
		//	*	vchrLowBattery				0x0101		// Display low battery dialog
		//		vchrEnterDebugger			0x0102		// Enter Debugger
		//	*	vchrNextField				0x0103		// Go to next field in form
		//		vchrStartConsole			0x0104		// Startup console task
		//
		//	*	vchrMenu					0x0105		// Ctl-A
		//	*	vchrCommand					0x0106		// Ctl-C
		//	*	vchrConfirm					0x0107		// Ctl-D
		//	*	vchrLaunch					0x0108		// Ctl-E
		//	*	vchrKeyboard				0x0109		// Ctl-F popup the keyboard in appropriate mode
		//		vchrFind					0x010A
		//		vchrCalc					0x010B
		//	*	vchrPrevField				0x010C
		//		vchrAlarm					0x010D		// sent before displaying an alarm
		//		vchrRonamatic				0x010E		// stroke from graffiti area to top half of screen
		//		vchrGraffitiReference		0x010F		// popup the Graffiti reference
		//		vchrKeyboardAlpha			0x0110		// popup the keyboard in alpha mode
		//		vchrKeyboardNumeric			0x0111		// popup the keyboard in number mode
		//		vchrLock					0x0112		// switch to the Security app and lock the device
		//	*	vchrBacklight				0x0113		// toggle state of backlight
		//	*	vchrAutoOff					0x0114		// power off due to inactivity timer
		// Added for PalmOS 3.0
		//		vchrExgTest					0x0115		// put exchange Manager into test mode (&.t)
		//		vchrSendData				0x0116		// Send data if possible
		//		vchrIrReceive				0x0117		// Initiate an Ir receive manually (&.i)
		// Added for PalmOS 3.1
		//		vchrTsm1					0x0118		// Text Services silk-screen button
		//		vchrTsm2					0x0119		// Text Services silk-screen button
		//		vchrTsm3					0x011A		// Text Services silk-screen button
		//		vchrTsm4					0x011B		// Text Services silk-screen button	
		// Added for PalmOS 3.2
		//		vchrRadioCoverageOK			0x011C		// Radio coverage check successful
		//		vchrRadioCoverageFail		0x011D		// Radio coverage check failure
		//		vchrPowerOff				0x011E		// Posted after autoOffChr or hardPowerChr
		//												// to put system to sleep with SysSleep.
		// Added for PalmOS 3.5
		//		vchrResumeSleep				0x011F		// Posted by NotifyMgr clients after they
		//												// have deferred a sleep request in order 
		//												// to resume it.
		//		vchrLateWakeup				0x0120		// Posted by the system after waking up
		//												// to broadcast a late wakeup notification.
		//												// FOR SYSTEM USE ONLY
		//		vchrTsmMode					0x0121		// Posted by TSM to trigger mode change.
		//		vchrBrightness				0x0122		// Activates brightness adjust dialog
		//	*	vchrContrast				0x0123		// Activates contrast adjust dialog
		//		vchrExgIntData				0x01FF		// Exchange Manager wakeup event
		//
		// The application launching buttons generate the following
		// key codes and will also set the commandKeyMask bit in the 
		// modifiers field
		//		vchrHardKeyMin				0x0200
		//		vchrHardKeyMax				0x02FF		// 256 hard keys
		//
		//	!	vchrHard1					0x0204
		//	!	vchrHard2					0x0205
		//	!	vchrHard3					0x0206
		//	!	vchrHard4					0x0207
		//	!	vchrHardPower				0x0208
		//		vchrHardCradle				0x0209		// Button on cradle pressed
		//		vchrHardCradle2				0x020A		// Button on cradle pressed and hwrDockInGeneric1
		//												// input on dock asserted (low).
		//		vchrHardContrast			0x020B		// Sumo's Contrast button
		//		vchrHardAntenna				0x020C		// Eleven's Antenna switch
		//		vchrHardBrightness			0x020D		// Hypothetical Brightness button

		case 0x01:	// control-A
			ascii = vchrMenu;
			modifiers = commandKeyMask;
			break;
		case 0x02:	// control-B
			ascii = vchrLowBattery;
			modifiers = commandKeyMask;
			break;			
		case 0x03:	// control-C
			ascii = vchrCommand;
			modifiers = commandKeyMask;
			break;
		case 0x04:	// control-D
			ascii = vchrConfirm;
			modifiers = commandKeyMask;
			break;
		case 0x05:	// control-E
			ascii = vchrLaunch;
			modifiers = commandKeyMask;
			break;
		case 0x06:	// control-F
			ascii = vchrKeyboard;
			modifiers = commandKeyMask;
			break;
		case 0x0D:	// control-M
			ascii = chrLineFeed;
			break;
		case 0x0E:	// control-N
			ascii = vchrNextField;
			modifiers = commandKeyMask;
			break;			
		case 0x10:	// control-P
			ascii = vchrPrevField;
			modifiers = commandKeyMask;
			break;			
		case 0x13:	// control-S
			ascii = vchrAutoOff;
			modifiers = commandKeyMask;
			break;			
		case 0x14:	// control-T
			ascii = vchrHardContrast;
			modifiers = commandKeyMask;
			break;			
		case 0x15:	// control-U
			ascii = vchrBacklight;
			modifiers = commandKeyMask;
			break;
	}


#if 0	// Turning this bit off for now.  It never worked because (a) the values
		// assigned to "status" were incorrect (they should be correct now) and
		// (b) they only worked for non-EZ devices.  EZ devices kind of ignore
		// the "status" parameter and probe the keyboard settings directly,
		// something we don't handle at this point of control.  So I'm turning
		// this off and just posting the ascii keys.  Since the old method
		// never worked, then there's no issue with the Key Manager no longer
		// doing the right thing.

	// If this is one of the hard keys, send it through the Key Manager so that
	//  it can do the right things concering double-taps, etc.
	if ((modifiers & commandKeyMask) &&
			(ascii > hardKeyMin || (ascii == pageUpChr) || (ascii == pageDownChr)))
	{

		DWord		status = 0;

		switch (ascii)
		{
			case hardPowerChr: 		status = hwr328IntLoInt0;		break;
			case pageUpChr: 		status = hwr328IntLoInt1;		break;
			case pageDownChr: 		status = hwr328IntLoInt2;		break;
			case hard1Chr: 			status = hwr328IntLoInt3;		break;
			case hard2Chr: 			status = hwr328IntLoInt4;		break;
			case hard3Chr: 			status = hwr328IntLoInt5;		break;
			case hard4Chr: 			status = hwr328IntLoInt6;		break;
			case hardCradleChr: 	status = hwr328IntHiIRQ1 << 16;	break;
			case hardAntennaChr: 	status = hwr328IntHiIRQ2 << 16;	break;
		}

		// I'm dubious that this is the correct thing to do in the emulator.
		// This is an interrupt routine that we're calling directly.  But
		// what if a "real" interrupt is triggered from some other source
		// while this call is executing (for instance, the user clicks on
		// one of the "keyboard buttons", thus generating a keyboard
		// interrupt)?  It's possible that the two will stomp on each other.
		// The best thing to do is to post events the emulator way.
		// However, I'm hard-pressed to do that.  That would require calling
		// Hardware_ButtonEvent twice (once to signal the keyboard
		// interrupt, and another to clear it), but I don't know how to give
		// the processor time in between to respond to the first call.

		if (status != 0)
			::KeyHandleInterrupt (false, status);
	}

	// For other keys, enqueue them directly.
	else
#endif
	{
		::EvtEnqueueKey (ascii, keycode, modifiers);
	}
}


// ---------------------------------------------------------------------------
//		¥ StubAppGremlinsOn
// ---------------------------------------------------------------------------
// Stub routine that update the Gremlins menu an the global variable the
// keeps track of the ????

void StubAppGremlinsOn (void)
{
	// Called from Gremlins::Initialize.
}


// ---------------------------------------------------------------------------
//		¥ StubAppGremlinsOff
// ---------------------------------------------------------------------------
// Stub routine that update the Gremlins menu an the global variable the
// keeps track of the ????

void StubAppGremlinsOff (void)
{
	// Called by Gremlins when counter > until.
}


// ---------------------------------------------------------------------------
//		¥ StubViewDrawLine
// ---------------------------------------------------------------------------
// This is a stub routine that called the View object's method that draw
// a line.

void StubViewDrawLine (int xStart, int yStart, int xEnd, int yEnd)
{
	Platform::ViewDrawLine (xStart, yStart, xEnd, yEnd);
}


// ---------------------------------------------------------------------------
//		¥ StubViewDrawPixel
// ---------------------------------------------------------------------------
// This is a stub routine that called the View object's method that draw
// a pixel.

void StubViewDrawPixel (int xPos, int yPos)
{
	Platform::ViewDrawPixel (xPos, yPos);
}


