//				Package : omnithread
// omnithread/nt.cc		Created : 6/95 tjr
//
//    Copyright (C) 1995-1999 AT&T Laboratories Cambridge
//
//    This file is part of the omnithread library
//
//    The omnithread library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  
//    02111-1307, USA
//

//
// Implementation of OMNI thread abstraction for NT threads
//

#include "EmCommon.h"
#include "omnithread.h"

#define DB(x) // x 
//#include <iostream.h> or #include <iostream> if DB is on.

///////////////////////////////////////////////////////////////////////////
//
// Mutex
//
///////////////////////////////////////////////////////////////////////////


omni_mutex::omni_mutex(void)
{
}

omni_mutex::~omni_mutex(void)
{
}

void
omni_mutex::lock(void)
{
}

void
omni_mutex::unlock(void)
{
}


#if 0
///////////////////////////////////////////////////////////////////////////
//
// Condition variable
//
///////////////////////////////////////////////////////////////////////////


omni_condition::omni_condition(omni_mutex* m) : mutex(m)
{
}


omni_condition::~omni_condition(void)
{
}


void
omni_condition::wait(void)
{
}


int
omni_condition::timedwait(unsigned long abs_sec, unsigned long abs_nsec)
{
#pragma unused (abs_sec, abs_nsec)

    return 1;
}


void
omni_condition::signal(void)
{
}


void
omni_condition::broadcast(void)
{
}



///////////////////////////////////////////////////////////////////////////
//
// Counting semaphore
//
///////////////////////////////////////////////////////////////////////////


omni_semaphore::omni_semaphore(unsigned int initial)
{
#pragma unused (initial)
}


omni_semaphore::~omni_semaphore(void)
{
}


void
omni_semaphore::wait(void)
{
}


int
omni_semaphore::trywait(void)
{
    return 0;
}


void
omni_semaphore::post(void)
{
}



///////////////////////////////////////////////////////////////////////////
//
// Thread
//
///////////////////////////////////////////////////////////////////////////
#endif	// 0

//
// Static variables
//

int omni_thread::init_t::count = 0;


//
// Initialisation function (gets called before any user code).
//

omni_thread::init_t::init_t(void)
{
}

omni_thread::init_t::~init_t(void)
{
}

#if 0
//
// Wrapper for thread creation.
//

void
omni_thread_wrapper(void* ptr)
{
#pragma unused (ptr)
}


//
// Constructors for omni_thread - set up the thread object but don't
// start it running.
//

// construct a detached thread running a given function.

omni_thread::omni_thread(void (*fn)(void*), void* arg, priority_t pri)
{
#pragma unused (fn, arg, pri)
}

// construct an undetached thread running a given function.

omni_thread::omni_thread(void* (*fn)(void*), void* arg, priority_t pri)
{
#pragma unused (fn, arg, pri)
}

// construct a thread which will run either run() or run_undetached().

omni_thread::omni_thread(void* arg, priority_t pri)
{
#pragma unused (arg, pri)
}

// common part of all constructors.

void
omni_thread::common_constructor(void* arg, priority_t pri, int det)
{
#pragma unused (arg, pri, det)
}


//
// Destructor for omni_thread.
//

omni_thread::~omni_thread(void)
{
}


//
// Start the thread
//

void
omni_thread::start(void)
{
}


//
// Start a thread which will run the member function run_undetached().
//

void
omni_thread::start_undetached(void)
{
}


//
// join - simply check error conditions & call WaitForSingleObject.
//

void
omni_thread::join(void** status)
{
#pragma unused (status)
}


//
// Change this thread's priority.
//

void
omni_thread::set_priority(priority_t pri)
{
#pragma unused (pri)
}


//
// create - construct a new thread object and start it running.  Returns thread
// object if successful, null pointer if not.
//

// detached version

omni_thread*
omni_thread::create(void (*fn)(void*), void* arg, priority_t pri)
{
#pragma unused (fn, arg, pri)
	return NULL;
}

// undetached version

omni_thread*
omni_thread::create(void* (*fn)(void*), void* arg, priority_t pri)
{
#pragma unused (fn, arg, pri)
	return NULL;
}


//
// exit() _must_ lock the mutex even in the case of a detached thread.  This is
// because a thread may run to completion before the thread that created it has
// had a chance to get out of start().  By locking the mutex we ensure that the
// creating thread must have reached the end of start() before we delete the
// thread object.  Of course, once the call to start() returns, the user can
// still incorrectly refer to the thread object, but that's their problem.
//

void
omni_thread::exit(void* return_value)
{
#pragma unused (return_value)
}


omni_thread*
omni_thread::self(void)
{
	return NULL;
}


void
omni_thread::yield(void)
{
}


void
omni_thread::sleep(unsigned long secs, unsigned long nanosecs)
{
#pragma unused (secs, nanosecs)
}


void
omni_thread::get_time(unsigned long* abs_sec, unsigned long* abs_nsec,
		      unsigned long rel_sec, unsigned long rel_nsec)
{
#pragma unused (abs_sec, abs_nsec, rel_sec, rel_nsec)
}

#endif	// 0
