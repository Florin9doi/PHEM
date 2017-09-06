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

#ifndef EmThreadSafeQueue_h
#define EmThreadSafeQueue_h

#include "omnithread.h"

#include <deque>

template <class T>
class EmThreadSafeQueue
{
	public:
								EmThreadSafeQueue		(int maxSize = 0);
								~EmThreadSafeQueue		(void);

		void					Put 					(const T&);
		T						Get 					(void);
		T						Peek 					(void);
		int 					GetUsed					(void);
		int 					GetFree					(void);
		Bool					WaitForDataAvailable	(long timeoutms);

		void					Clear					(void);
		int						GetMaxSize				(void);

	private:
		omni_mutex				fMutex;
#if HAS_OMNI_THREAD
		omni_condition			fAvailable;
#endif
		deque<T>				fContainer;
		int						fMaxSize;
};

typedef EmThreadSafeQueue<uint8>	EmByteQueue;

#endif	// EmThreadSafeQueue_h
