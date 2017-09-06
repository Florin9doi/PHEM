//				Package : omnithread
// omnithread/posix.cc		Created : 7/94 tjr
//
//    Copyright (C) 1994-1999 AT&T Laboratories Cambridge
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
// Implementation of OMNI thread abstraction for posix threads
//
// The source below tests for the definition of the macros:
//     PthreadDraftVersion
//     PthreadSupportThreadPriority
//     NoNanoSleep
//     NeedPthreadInit
//
// As different draft versions of the pthread standard P1003.4a/P1003.1c
// define slightly different APIs, the macro 'PthreadDraftVersion'
// identifies the draft version supported by this particular platform.
//
// Some unix variants do not support thread priority unless a real-time
// kernel option is installed. The macro 'PthreadSupportThreadPriority',
// if defined, enables the use of thread priority. If it is not defined,
// setting or changing thread priority will be silently ignored.
//
// nanosleep() is defined in Posix P1003.4 since Draft 9 (?).
// Not all platforms support this standard. The macro 'NoNanoSleep'
// identifies platform that don't.
//

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <omnithread.h>

// Android debugging
#if 0
#include <jni.h>
#include <android/log.h>

#define  LOG_TAG    "omnithread"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#endif


#if (defined(__GLIBC__) && __GLIBC__ >= 2)
// typedef of struct timeval and gettimeofday();
#include <sys/time.h>
#include <unistd.h>
#endif

#if defined(__linux__) && defined(_MIT_POSIX_THREADS)
#include <pthread/mit/sys/timers.h>
#endif

#if defined(__irix__) && defined(PthreadSupportThreadPriority)
#if _POSIX_THREAD_PRIORITY_SCHEDULING
#include <sched.h>
#endif
#endif

#if defined(__QNXNTO__) && defined(PthreadSupportThreadPriority)
#if _POSIX_THREAD_PRIORITY_SCHEDULING
#include <sched.h>
#endif
#endif

#define DB(x) // x
//#include <iostream.h> or #include <iostream> if DB is on.

#if (PthreadDraftVersion <= 6)
#define ERRNO(x) (((x) != 0) ? (errno) : 0)
#ifdef __VMS
// pthread_setprio returns old priority on success (draft version 4:
// OpenVms version < 7)
#define THROW_ERRORS(x) { if ((x) == -1) throw omni_thread_fatal(errno); }
#else
#define THROW_ERRORS(x) { if ((x) != 0) throw omni_thread_fatal(errno); }
#endif
#else
#define ERRNO(x) (x)
#define THROW_ERRORS(x) { int rc = (x); \
			  if (rc != 0) throw omni_thread_fatal(rc); }
#endif


///////////////////////////////////////////////////////////////////////////
//
// Mutex
//
///////////////////////////////////////////////////////////////////////////


omni_mutex::omni_mutex(void)
{
#if (PthreadDraftVersion == 4)
    THROW_ERRORS(pthread_mutex_init(&posix_mutex, pthread_mutexattr_default));
#else
    THROW_ERRORS(pthread_mutex_init(&posix_mutex, 0));
#endif
}

omni_mutex::~omni_mutex(void)
{
    THROW_ERRORS(pthread_mutex_destroy(&posix_mutex));
}

void
omni_mutex::lock(void)
{
#ifdef __QNXNTO__
  int err = pthread_mutex_lock(&posix_mutex);
  if ((err != EOK) && (err != EDEADLK))
  {
    DB(cerr << "omni_mutex::lock err=" << err);
    THROW_ERRORS(err);
  }
#else
    //LOGI("Locking mutex.");
    THROW_ERRORS(pthread_mutex_lock(&posix_mutex));
#endif
}

void
omni_mutex::unlock(void)
{
#ifdef __QNXNTO__
  int err = pthread_mutex_unlock(&posix_mutex);
  if ((err != EOK) && (err != EPERM))
  {
    DB(cerr << "omni_mutex::unlock err=" << err);
    THROW_ERRORS(err);
  }
#else
    THROW_ERRORS(pthread_mutex_unlock(&posix_mutex));
    //LOGI("Mutex unlocked.");
#endif
}



///////////////////////////////////////////////////////////////////////////
//
// Condition variable
//
///////////////////////////////////////////////////////////////////////////


omni_condition::omni_condition(omni_mutex* m) : mutex(m)
{
#if (PthreadDraftVersion == 4)
    THROW_ERRORS(pthread_cond_init(&posix_cond, pthread_condattr_default));
#else
    THROW_ERRORS(pthread_cond_init(&posix_cond, 0));
#endif
}

omni_condition::~omni_condition(void)
{
    THROW_ERRORS(pthread_cond_destroy(&posix_cond));
}

void
omni_condition::wait(void)
{
    //LOGI("Waiting on condition.");
    THROW_ERRORS(pthread_cond_wait(&posix_cond, &mutex->posix_mutex));
}

int
omni_condition::timedwait(unsigned long secs, unsigned long nanosecs)
{
    timespec rqts = { secs, nanosecs };

again:
    int rc = ERRNO(pthread_cond_timedwait(&posix_cond,
					  &mutex->posix_mutex, &rqts));
    if (rc == 0)
	return 1;

#if (PthreadDraftVersion <= 6)
    if (rc == EAGAIN)
	return 0;
#endif

#if defined(__GLIBC__)
    // Some versions of the glibc 2.0.x produces this errno when the 
    // program is debugged under gdb. Straightly speaking this is non-posix
    // compliant. We catch this here to make debugging possible.
    if (rc == EINTR)
      goto again;
#endif

    if (rc == ETIMEDOUT)
	return 0;

    throw omni_thread_fatal(rc);
}

void
omni_condition::signal(void)
{
    THROW_ERRORS(pthread_cond_signal(&posix_cond));
}

void
omni_condition::broadcast(void)
{
    //LOGI("Broadcasting condition.");
    THROW_ERRORS(pthread_cond_broadcast(&posix_cond));
}



///////////////////////////////////////////////////////////////////////////
//
// Counting semaphore
//
///////////////////////////////////////////////////////////////////////////


omni_semaphore::omni_semaphore(unsigned int initial) : c(&m)
{
    value = initial;
}

omni_semaphore::~omni_semaphore(void)
{
}

void
omni_semaphore::wait(void)
{
    omni_mutex_lock l(m);

    while (value == 0)
	c.wait();

    value--;
}

int
omni_semaphore::trywait(void)
{
    omni_mutex_lock l(m);

    if (value == 0)
	return 0;

    value--;
    return 1;
}

void
omni_semaphore::post(void)
{
    {
        omni_mutex_lock l(m);
        value++;
    }

    c.signal();
}



///////////////////////////////////////////////////////////////////////////
//
// Thread
//
///////////////////////////////////////////////////////////////////////////


//
// static variables
//

int omni_thread::init_t::count = 0;

omni_thread* omni_thread::root_thread;
omni_mutex* omni_thread::next_id_mutex;
int omni_thread::next_id = 0;

static pthread_key_t self_key;

#ifdef PthreadSupportThreadPriority
static int lowest_priority;
static int normal_priority;
static int highest_priority;
#endif


//
// Initialisation function (gets called before any user code).
//

omni_thread::init_t::init_t(void)
{
    if (count++ != 0)	// only do it once however many objects get created.
	return;

    DB(cerr << "omni_thread::init: posix 1003.4a/1003.1c (draft "
       << PthreadDraftVersion << ") implementation initialising\n");

#ifdef NeedPthreadInit

    pthread_init();

#endif

#if (PthreadDraftVersion == 4)
    THROW_ERRORS(pthread_keycreate(&self_key, NULL));
#else
    THROW_ERRORS(pthread_key_create(&self_key, NULL));
#endif

#ifdef PthreadSupportThreadPriority

#if defined(__osf1__) && defined(__alpha__) || defined(__VMS)

    lowest_priority = PRI_OTHER_MIN;
    highest_priority = PRI_OTHER_MAX;

#elif defined(__hpux__)

    lowest_priority = PRI_OTHER_MIN;
    highest_priority = PRI_OTHER_MAX;

#elif defined(__sunos__) && (__OSVERSION__ == 5)

    // a bug in pthread_attr_setschedparam means lowest priority is 1 not 0

    lowest_priority  = 1;
    highest_priority = 3;

#else

    lowest_priority = sched_get_priority_min(SCHED_FIFO);
    highest_priority = sched_get_priority_max(SCHED_FIFO);

#endif

    switch (highest_priority - lowest_priority) {

    case 0:
    case 1:
	normal_priority = lowest_priority;
	break;

    default:
	normal_priority = lowest_priority + 1;
	break;
    }

#endif   /* PthreadSupportThreadPriority */

    next_id_mutex = new omni_mutex;

    //
    // Create object for this (i.e. initial) thread.
    //

    root_thread = new omni_thread;

    root_thread->_state = STATE_RUNNING;

    root_thread->posix_thread = pthread_self ();

    DB(cerr << "initial thread " << root_thread->id() << endl);

    THROW_ERRORS(pthread_setspecific(self_key, (void*) root_thread));

#ifdef PthreadSupportThreadPriority

#if (PthreadDraftVersion == 4)

    THROW_ERRORS(pthread_setprio(root_thread->posix_thread,
				 posix_priority(PRIORITY_NORMAL)));

#elif (PthreadDraftVersion == 6)

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    THROW_ERRORS(pthread_attr_setprio(&attr, posix_priority(PRIORITY_NORMAL)));

    THROW_ERRORS(pthread_setschedattr(root_thread->posix_thread, attr));

#else

    struct sched_param sparam;

    sparam.sched_priority = posix_priority(PRIORITY_NORMAL);

    THROW_ERRORS(pthread_setschedparam(root_thread->posix_thread, SCHED_OTHER, &sparam));

#endif   /* PthreadDraftVersion */

#endif   /* PthreadSupportThreadPriority */
}

omni_thread::init_t::~init_t (void)
{
    if (--count != 0)	// only do it once however many objects get created.
	return;

    delete root_thread;
    delete next_id_mutex;
}


//
// Wrapper for thread creation.
//

extern "C" void* 
omni_thread_wrapper(void* ptr)
{
    omni_thread* me = (omni_thread*)ptr;

    DB(cerr << "omni_thread_wrapper: thread " << me->id()
       << " started\n");

    THROW_ERRORS(pthread_setspecific(self_key, me));

    //
    // Now invoke the thread function with the given argument.
    //

    if (me->fn_void != NULL) {
	(*me->fn_void)(me->thread_arg);
	omni_thread::exit();
    }

    if (me->fn_ret != NULL) {
	void* return_value = (*me->fn_ret)(me->thread_arg);
	omni_thread::exit(return_value);
    }

    if (me->detached) {
	me->run(me->thread_arg);
	omni_thread::exit();
    } else {
	void* return_value = me->run_undetached(me->thread_arg);
	omni_thread::exit(return_value);
    }

    // should never get here.

    return NULL;
}


//
// Constructors for omni_thread - set up the thread object but don't
// start it running.
//

// construct a detached thread running a given function.

omni_thread::omni_thread(void (*fn)(void*), void* arg, priority_t pri)
{
    common_constructor(arg, pri, 1);
    fn_void = fn;
    fn_ret = NULL;
}

// construct an undetached thread running a given function.

omni_thread::omni_thread(void* (*fn)(void*), void* arg, priority_t pri)
{
    common_constructor(arg, pri, 0);
    fn_void = NULL;
    fn_ret = fn;
}

// construct a thread which will run either run() or run_undetached().

omni_thread::omni_thread(void* arg, priority_t pri)
{
    common_constructor(arg, pri, 1);
    fn_void = NULL;
    fn_ret = NULL;
}

// common part of all constructors.

void
omni_thread::common_constructor(void* arg, priority_t pri, int det)
{
    _state = STATE_NEW;
    _priority = pri;

    next_id_mutex->lock();
    _id = next_id++;
    next_id_mutex->unlock();

    thread_arg = arg;
    detached = det;	// may be altered in start_undetached()

    // posix_thread is set up in initialisation routine or start().
}


//
// Destructor for omni_thread.
//

omni_thread::~omni_thread(void)
{
    DB(cerr << "destructor called for thread " << id() << endl);
}


//
// Start the thread
//

void
omni_thread::start(void)
{
    omni_mutex_lock l(mutex);

    if (_state != STATE_NEW)
	throw omni_thread_invalid();

    pthread_attr_t attr;

#if (PthreadDraftVersion == 4)
    pthread_attr_create(&attr);
#else
    pthread_attr_init(&attr);
#endif

#if (PthreadDraftVersion == 8)
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_UNDETACHED);
#endif

#ifdef PthreadSupportThreadPriority

#if (PthreadDraftVersion <= 6)

    THROW_ERRORS(pthread_attr_setprio(&attr, posix_priority(_priority)));

#else

    struct sched_param sparam;

    sparam.sched_priority = posix_priority(_priority);

    THROW_ERRORS(pthread_attr_setschedparam(&attr, &sparam));

#endif	/* PthreadDraftVersion */

#endif	/* PthreadSupportThreadPriority */

#if defined(__osf1__) && defined(__alpha__) || defined(__VMS)

    // omniORB requires a larger stack size than the default (21120)
    // on OSF/1

    THROW_ERRORS(pthread_attr_setstacksize(&attr, 32768));

#endif	/* __osf1__ && __alpha__ */

#if (PthreadDraftVersion == 4)
    THROW_ERRORS(pthread_create(&posix_thread, attr, omni_thread_wrapper,
				(void*)this));
    pthread_attr_delete(&attr);
#else
    THROW_ERRORS(pthread_create(&posix_thread, &attr, omni_thread_wrapper,
				(void*)this));
    pthread_attr_destroy(&attr);
#endif

    _state = STATE_RUNNING;

    if (detached) {

#if (PthreadDraftVersion <= 6)
	THROW_ERRORS(pthread_detach(&posix_thread));
#else
	THROW_ERRORS(pthread_detach(posix_thread));
#endif
    }
}


//
// Start a thread which will run the member function run_undetached().
//

void
omni_thread::start_undetached(void)
{
    if ((fn_void != NULL) || (fn_ret != NULL))
	throw omni_thread_invalid();

    detached = 0;
    start();
}


//
// join - simply check error conditions & call pthread_join.
//

void
omni_thread::join(void** status)
{
    mutex.lock();

    if ((_state != STATE_RUNNING) && (_state != STATE_TERMINATED)) {
	mutex.unlock();
	throw omni_thread_invalid();
    }

    mutex.unlock();

    if (this == self())
	throw omni_thread_invalid();

    if (detached)
	throw omni_thread_invalid();

    DB(cerr << "omni_thread::join: doing pthread_join\n");

    THROW_ERRORS(pthread_join(posix_thread, status));

    DB(cerr << "omni_thread::join: pthread_join succeeded\n");

#if (PthreadDraftVersion == 4)
    // With draft 4 pthreads implementations (HPUX 10.x and
    // Digital Unix 3.2), have to detach the thread after 
    // join. If not, the storage for the thread will not be
    // be reclaimed.
    THROW_ERRORS(pthread_detach(&posix_thread));
#endif

    delete this;
}


//
// Change this thread's priority.
//

void
omni_thread::set_priority(priority_t pri)
{
    omni_mutex_lock l(mutex);

    if (_state != STATE_RUNNING)
	throw omni_thread_invalid();

    _priority = pri;

#ifdef PthreadSupportThreadPriority

#if (PthreadDraftVersion == 4)

    THROW_ERRORS(pthread_setprio(posix_thread, posix_priority(pri)));

#elif (PthreadDraftVersion == 6)

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    THROW_ERRORS(pthread_attr_setprio(&attr, posix_priority(pri)));

    THROW_ERRORS(pthread_setschedattr(posix_thread, attr));

#else

    struct sched_param sparam;

    sparam.sched_priority = posix_priority(pri);

    THROW_ERRORS(pthread_setschedparam(posix_thread, SCHED_OTHER, &sparam));

#endif   /* PthreadDraftVersion */

#endif   /* PthreadSupportThreadPriority */
}


//
// create - construct a new thread object and start it running.  Returns thread
// object if successful, null pointer if not.
//

// detached version

omni_thread*
omni_thread::create(void (*fn)(void*), void* arg, priority_t pri)
{
    omni_thread* t = new omni_thread(fn, arg, pri);

    t->start();

    return t;
}

// undetached version

omni_thread*
omni_thread::create(void* (*fn)(void*), void* arg, priority_t pri)
{
    omni_thread* t = new omni_thread(fn, arg, pri);

    t->start();

    return t;
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
    omni_thread* me = self();

    if (me)
      {
	me->mutex.lock();

	me->_state = STATE_TERMINATED;

	me->mutex.unlock();

	DB(cerr << "omni_thread::exit: thread " << me->id() << " detached "
	   << me->detached << " return value " << return_value << endl);

	if (me->detached)
	  delete me;
      }
    else
      {
	DB(cerr << "omni_thread::exit: called with a non-omnithread. Exit quietly." << endl);
      }

    pthread_exit(return_value);
}


omni_thread*
omni_thread::self(void)
{
    omni_thread* me;

#if (PthreadDraftVersion <= 6)

    THROW_ERRORS(pthread_getspecific(self_key, (void**)&me));

#else

    me = (omni_thread *)pthread_getspecific(self_key);

#endif

    if (!me) {
      // This thread is not created by omni_thread::start because it
      // doesn't has a class omni_thread instance attached to its key.
      DB(cerr << "omni_thread::self: called with a non-ominthread. NULL is returned." << endl);
    }

    return me;
}


void
omni_thread::yield(void)
{
#if (PthreadDraftVersion == 6)

    pthread_yield(NULL);

#elif (PthreadDraftVersion < 9)

    pthread_yield();

#elif (__svr4__)

	thr_yield();

#else

    THROW_ERRORS(sched_yield());

#endif
}


void
omni_thread::sleep(unsigned long secs, unsigned long nanosecs)
{
    timespec rqts = { secs, nanosecs };

#ifndef NoNanoSleep

    if (nanosleep(&rqts, (timespec*)NULL) != 0)
	throw omni_thread_fatal(errno);
#else

#if defined(__osf1__) && defined(__alpha__) || defined(__hpux__) && (__OSVERSION__ == 10) || defined(__VMS) || defined(__SINIX__)

    if (pthread_delay_np(&rqts) != 0)
	throw omni_thread_fatal(errno);

#elif defined(__linux__) || defined(__aix__) || defined(__svr4__) || defined(__bsdi__)

    if (secs > 2000) {
	::sleep(secs);
    } else {
	usleep(secs * 1000000 + (nanosecs / 1000));
    }

#else

#error You need to define how to emulate nanosleep!
    throw omni_thread_invalid();

#endif
#endif	/* NoNanoSleep */
}


void
omni_thread::get_time(unsigned long* abs_sec, unsigned long* abs_nsec,
		      unsigned long rel_sec, unsigned long rel_nsec)
{
    timespec abs;

#if defined(__osf1__) && defined(__alpha__) || defined(__hpux__) && (__OSVERSION__ == 10) || defined(__VMS) || defined(__SINIX__)

    timespec rel;
    rel.tv_sec = rel_sec;
    rel.tv_nsec = rel_nsec;
    THROW_ERRORS(pthread_get_expiration_np(&rel, &abs));

#else

#if defined(__linux__) || defined(__aix__) || defined(__svr4__)

    struct timeval tv;
    gettimeofday(&tv, NULL); 
    abs.tv_sec = tv.tv_sec;
    abs.tv_nsec = tv.tv_usec * 1000;

#else	/* __linux__ || __aix__ */

    clock_gettime(CLOCK_REALTIME, &abs);

#endif	/* __linux__ || __aix__ */

    abs.tv_nsec += rel_nsec;
    abs.tv_sec += rel_sec + abs.tv_nsec / 1000000000;
    abs.tv_nsec = abs.tv_nsec % 1000000000;

#endif	/* __osf1__ && __alpha__ */

    *abs_sec = abs.tv_sec;
    *abs_nsec = abs.tv_nsec;
}


int
omni_thread::posix_priority(priority_t pri)
{
#ifdef PthreadSupportThreadPriority
    switch (pri) {

    case PRIORITY_LOW:
	return lowest_priority;

    case PRIORITY_NORMAL:
	return normal_priority;

    case PRIORITY_HIGH:
	return highest_priority;

    }
#endif

    throw omni_thread_invalid();
    return 0; /* keep compilers happy */
}
