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
#include "EmThreadSafeQueue.h"


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue
// ---------------------------------------------------------------------------

template <class T>
EmThreadSafeQueue<T>::EmThreadSafeQueue (int maxSize) :
	fMutex (),
#if HAS_OMNI_THREAD
	fAvailable (&fMutex),
#endif
	fContainer (),
	fMaxSize (maxSize)
{
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue
// ---------------------------------------------------------------------------

template <class T>
EmThreadSafeQueue<T>::~EmThreadSafeQueue (void)
{
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue::Put
// ---------------------------------------------------------------------------

template <class T>
void EmThreadSafeQueue<T>::Put (const T& value)
{
	omni_mutex_lock	lock (fMutex);

	EmAssert (fMaxSize == 0 || (long) fContainer.size () < fMaxSize);

	fContainer.push_back (value);

#if HAS_OMNI_THREAD
	// Tell clients that there may be new data in the buffer.
	fAvailable.signal ();
#endif
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue::Get
// ---------------------------------------------------------------------------

template <class T>
T EmThreadSafeQueue<T>::Get (void)
{
	omni_mutex_lock	lock (fMutex);

	// Make sure there's something in the queue (this shouldn't happen,
	// because the caller should always call GetUsed before Get).

	EmAssert (fContainer.size () > 0);

	T	result = fContainer[0];
	fContainer.pop_front ();

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue::Peek
// ---------------------------------------------------------------------------

template <class T>
T EmThreadSafeQueue<T>::Peek (void)
{
	omni_mutex_lock	lock (fMutex);

	// Make sure there's something in the queue (this shouldn't happen,
	// because the caller should always call GetUsed before Get).

	EmAssert (fContainer.size () > 0);

	T	result = fContainer[0];

	return result;
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue::GetFree
// ---------------------------------------------------------------------------

template <class T>
int EmThreadSafeQueue<T>::GetFree (void)
{
	omni_mutex_lock	lock (fMutex);

	return fMaxSize - fContainer.size ();
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue::GetUsed
// ---------------------------------------------------------------------------

template <class T>
int EmThreadSafeQueue<T>::GetUsed (void)
{
	omni_mutex_lock	lock (fMutex);

	return fContainer.size ();
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue::WaitForDataAvailable
// ---------------------------------------------------------------------------

template <class T>
Bool EmThreadSafeQueue<T>::WaitForDataAvailable (long timeoutms)
{
#if HAS_OMNI_THREAD

	// calc absolute time to wait until:
	unsigned long	sec;
	unsigned long	nanosec;
	omni_thread::get_time (&sec, &nanosec, 0, timeoutms * 1000);

	return fAvailable.timedwait (sec, nanosec) == 1;

#else

	UNUSED_PARAM (timeoutms)
	return fContainer.size () > 0;

#endif
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue::Clear
// ---------------------------------------------------------------------------

template <class T>
void EmThreadSafeQueue<T>::Clear (void)
{
	omni_mutex_lock	lock (fMutex);

	fContainer.clear ();
}


// ---------------------------------------------------------------------------
//		¥ EmThreadSafeQueue::GetMaxSize
// ---------------------------------------------------------------------------

template <class T>
int EmThreadSafeQueue<T>::GetMaxSize (void)
{
	return fMaxSize;
}

// Instantiate the ones we want.

#include "EmSession.h"			// uint8 (Byte), EmButtonEvent, EmKeyEvent, EmPenEvent

template class EmThreadSafeQueue<uint8>;
template class EmThreadSafeQueue<EmButtonEvent>;
template class EmThreadSafeQueue<EmKeyEvent>;
template class EmThreadSafeQueue<EmPenEvent>;
