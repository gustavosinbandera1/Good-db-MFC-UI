// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< PTASK.H >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      2-Mar-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 19-Feb-99    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Tasking implemented using Posix pthreads 
//-------------------------------------------------------------------*--------*

#ifndef __PTASK_H__
#define __PTASK_H__

#include "goodsdlx.h"

#if defined(__linux__)
#include <sys/time.h>
#endif

#if defined(__FreeBSD__)
#include <sched.h>
#endif
#include <pthread.h>

BEGIN_GOODS_NAMESPACE

#if defined(__osf__)
#define PTHREAD_GET_EXPIRATION_NP 1
// I had problems with recursive mutexes at our version of Digital Uni
#undef PTHREAD_MUTEX_RECURSIVE_NP
#endif

class GOODS_DLL_EXPORT mutex_internals { 
    friend class eventex_internals;
    friend class semaphorex_internals;

  protected: 
    pthread_mutex_t cs;  
#ifdef PTHREAD_MUTEX_RECURSIVE_NP
    void enter() { pthread_mutex_lock(&cs); }
    void leave() { pthread_mutex_unlock(&cs); }
#else
    int       count;
    pthread_t owner;
    void enter() 
    { 
        pthread_t self = pthread_self();
        if (owner != self) { 
            pthread_mutex_lock(&cs); 
            owner = self;
        }
        count += 1;
    }
    void leave() 
    {
        assert(pthread_self() == owner);
        if (--count == 0) {
            owner = 0;
            pthread_mutex_unlock(&cs);
        } 
    }
#endif

    mutex_internals();  
    ~mutex_internals();  
};

class GOODS_DLL_EXPORT semaphore_internals { 
  protected: 
    pthread_mutex_t cs;
    pthread_cond_t  cond; 
    int             count; 

    void wait();
    void signal();
    boolean wait_with_timeout(time_t sec);

    descriptor_t get_handle() { return -1; }

    semaphore_internals(int init_count);  
    ~semaphore_internals();  
}; 

class GOODS_DLL_EXPORT semaphorex_internals { 
  protected: 
    mutex_internals& guard;
    pthread_cond_t   cond; 
    int              count; 

    void wait();
    void signal();
    boolean wait_with_timeout(time_t sec);

    semaphorex_internals(mutex_internals& cs, int init_count);  
    ~semaphorex_internals();  
}; 

class GOODS_DLL_EXPORT event_internals { 
  protected: 
    pthread_mutex_t cs;
    pthread_cond_t  cond; 
    int             signaled; 
    long            n_signals;

    void wait();
    boolean wait_with_timeout(time_t sec);
    void signal();
    void reset() { signaled = False; }

    descriptor_t get_handle() { return -1; }

    event_internals(boolean signaled);  
    ~event_internals();  
}; 

class GOODS_DLL_EXPORT eventex_internals { 
  protected: 
    mutex_internals& guard;
    pthread_cond_t   cond; 
    int              signaled; 
    long             n_signals;

    void wait();
    boolean wait_with_timeout(time_t sec);
    void signal();
    void reset() { signaled = False; }

    eventex_internals(mutex_internals& cs, boolean signaled);  
    ~eventex_internals();  
}; 

#define task_proc // qualifier for thread procedure

class GOODS_DLL_EXPORT task_internals { 
  protected: 
    static pthread_key_t thread_key;
    static pthread_key_t thread_specific_key;

    pthread_t      thread;
    pthread_attr_t thread_attr; 

    void*          arg; 
    void           (*f)(void* arg); 

    static void* create_thread(void* arg);
    static void  delete_thread(void* arg); 

#if 0
    void*          task_specific_data;

    static void* get_task_specific() {
        return ((task_internals*)pthread_getspecific(thread_key))->task_specific_data;    
    }


    static void  set_task_specific(void* data) { 
        ((task_internals*)pthread_getspecific(thread_key))->task_specific_data = data;
    }
#else
    static void* get_task_specific() {
        return pthread_getspecific(thread_specific_key);    
    }


    static void  set_task_specific(void* data) { 
        pthread_setspecific(thread_specific_key, data);
    }
#endif

    // currently only implemented for ctask model...
    nat8 get_profiling_usec(void) { return 0; }
    void start_profiling(void)    { }
    nat8 stop_profiling(void)     { return 0; }
}; 

END_GOODS_NAMESPACE

#endif
