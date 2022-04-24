// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< TASK.H >--------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 23-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Tasking and syncronization mechanisms abstractions  
//-------------------------------------------------------------------*--------*

#ifndef __TASK_H__
#define __TASK_H__

#include <time.h>
#include "goodsdlx.h"

#if defined(_WIN32)
#include "wtask.h"
#elif defined(PTHREADS)
#include "ptask.h"
#else
#include "ctask.h"
#endif

BEGIN_GOODS_NAMESPACE

//
// Mutual exclusion: only one task can hold mutex at the same time.
// Task holding mutex can call 'enter' any number of times and 
// will hold mutex until correspondent number of 'leave' will be 
// executed. If task holding mutex is terminated mutex is automaticaly unlocked
//  
class GOODS_DLL_EXPORT mutex : private mutex_internals { 
    friend class eventex;
    friend class semaphorex;
  public: 
    void enter() { mutex_internals::enter(); }
    void leave() { mutex_internals::leave(); }
};

//
// Critical section. When object of this class is used as local 
// variable, mutex is locked by constructor at the beginning of the block 
// (entering critical section) and unlocked by destructor after existing from 
// the block (leaving critical section)
//
class GOODS_DLL_EXPORT critical_section { 
    mutex& cs;
  public:
    critical_section(mutex& guard) : cs(guard) { 
        cs.enter();
    }
    ~critical_section() { cs.leave(); }
};

//
// Standard sempaphore supporting wait (P) and signal (V) operations.
//
class GOODS_DLL_EXPORT semaphore : private semaphore_internals { 
  public: 
    //
    // Wait semaphore to be signaled 
    //
    void wait() { semaphore_internals::wait(); }
    //
    // Wait for signal for a specified period of time. 
    // If timeout expired before semaphore is signaled then 'False'
    // is returned. 
    //
    boolean wait_with_timeout(time_t sec) { 
        return semaphore_internals::wait_with_timeout(sec);
    }
    //
    // Wakeup one task waiting for semaphore. If no task is waiting
    // then value of semaphore is increased so that following wait will
    // not block the task. 
    // 
    void signal() { semaphore_internals::signal(); }
    
    //
    // This method works only with Win32 and can be used for wating for multiple objects
    // All other implementations will return -1
    //
    descriptor_t get_handle() { 
        return semaphore_internals::get_handle();
    }
    semaphore(int init_count = 0) : semaphore_internals(init_count) {}
};

//
// Semaphore with associated mutex
// 
class GOODS_DLL_EXPORT semaphorex : private semaphorex_internals { 
  public: 
    //
    // Wait semaphore to be signaled. Should be called with associated mutex 
    // locked. Before wait mutex is unlocked and will locked again before 
    // returning.
    //
    void wait() { semaphorex_internals::wait(); }
    //
    // Wait for signal for a specified period of time. 
    // If timeout expired before semaphore is signaled then 'False'
    // is returned. Should be called with associated mutex 
    // locked. Before wait mutex is unlocked and will locked again before 
    // returning.
    //
    boolean wait_with_timeout(time_t sec) { 
        return semaphorex_internals::wait_with_timeout(sec);
    }
    //
    // Wakeup one task waiting for semaphore. If no task is waiting
    // then value of semaphore is increased so that following wait will
    // not block the task. Should be called with associated mutex locked.
    // 
    void signal() { semaphorex_internals::signal(); }

    semaphorex(mutex& cs, int init_count = 0) 
    : semaphorex_internals(cs, init_count) {}
};

//
// Event with manual reset 
//
class GOODS_DLL_EXPORT event : private event_internals { 
  public: 
    //
    // Wait event to be signaled 
    //
    void wait() { event_internals::wait(); }
    //
    // Wait for signal for a specified period of time. 
    // If timeout expired before event is signaled then 'False' is returned. 
    //
    boolean wait_with_timeout(time_t sec) { 
        return event_internals::wait_with_timeout(sec);
    }
    //
    // Switch event object to signaled state
    //
    void signal() { event_internals::signal(); }
    //
    // Reset event to non-signaled state. 
    // 
    void reset() { event_internals::reset(); }

    descriptor_t get_handle() { 
        return event_internals::get_handle();
    }
    event(boolean signaled = False) : event_internals(signaled) {}
};

//
// Event with associated mutex
//
class GOODS_DLL_EXPORT eventex : private eventex_internals { 
  public: 
    //
    // Wait event to be signaled. Associated mutex is released before wait 
    // and will be relocked before returning.
    //
    void wait() { eventex_internals::wait(); }
    //
    // Wait for signal for a specified period of time. Associated mutex 
    // is released before wait and will be relocked before returning. 
    // If timeout expired before event is signaled then 'False' is returned. 
    //
    boolean wait_with_timeout(time_t sec) { 
        return eventex_internals::wait_with_timeout(sec);
    }
    //
    // Switch event object to signaled state. Should be called with associated
    // mutex locked. 
    //
    void signal() { eventex_internals::signal(); }
    //
    // Reset event to non-signaled state. Should be called with associated 
    // mutex locked.
    // 
    void reset() { eventex_internals::reset(); }

    eventex(mutex& cs, boolean signaled = False) 
    : eventex_internals(cs, signaled) {}
};

class object_monitor;

//
// Task is a unit for scheduling excution. Schedulig can be either preemptive
// or cooperative. After task termination all mutexes hold by this task
// will be released
//
class GOODS_DLL_EXPORT task : private task_internals { 
    friend class task_internals;
  public: 
    typedef void (task_proc *fptr)(void* arg); 
    enum priority { 
        pri_background, 
        pri_low, 
        pri_normal, 
        pri_high, 
        pri_realtime 
    };
    enum { 
        min_stack    = 16*1024, 
        small_stack  = 64*1024, 
        normal_stack = 256*1024,
        big_stack    = 512*1024,
        huge_stack   = 1024*1024
    }; 
    //
    // Create new task. Pointer to task object returned by this function
    // can be used only for thread identification.
    //
    static task* create(fptr f, void* arg = NULL, priority pri = pri_normal, 
         size_t stack_size = normal_stack); 
    //
    // Initialize tasking. 
    //
    static void  initialize(size_t main_stack_size = normal_stack);
    //
    // Forse task rescheduling 
    //
    static void  reschedule();
    //
    // Current task will sleep during specified period
    //
    static void  sleep(time_t sec);
     //
    // Terminate current task
    //
    static void  exit();
    //
    // Get current task
    //
    static task* current();
    //
    // Get task specific data
    //
    static void* get_task_specific() { 
        return task_internals::get_task_specific();
    }
    //
    // Set task specific data
    //
    static void set_task_specific(void* data) { 
        task_internals::set_task_specific(data);
    }

    nat8 get_profiling_usec(void) {
        return task_internals::get_profiling_usec();
    }
    void start_profiling(void) {
        task_internals::start_profiling();
    }
    nat8 stop_profiling(void) {
        return task_internals::stop_profiling();
    }
}; 
    

template<class T>
class GOODS_DLL_EXPORT fifo_queue { 
  protected: 
    struct queue_end { 
        semaphorex       sem;
        volatile size_t  pos;
        volatile int     wait;

        queue_end(mutex& cs) : sem(cs) { pos = 0; wait = 0; }
    };
    mutex        cs; 
    queue_end    put, get;
    T*           buf;
    const size_t buf_size; 

  public: 
    boolean is_empty() const { return put.pos == get.pos; }
    boolean is_full() const { 
        return put.pos == get.pos-1 
            || (get.pos == 0 && put.pos == buf_size-1); 
    }
    fifo_queue& operator << (T const& elem) { 
        cs.enter();
        while (is_full()) { 
            put.wait += 1;
            put.sem.wait();
        }
        buf[put.pos] = elem;
        if (++put.pos == buf_size) put.pos = 0;
        if (get.wait != 0) { 
            get.sem.signal();
            get.wait -= 1;
        }
        cs.leave();
        return *this;
    }
    fifo_queue& operator >> (T& elem) { 
        cs.enter();
        while (is_empty()) { 
            get.wait += 1;
            get.sem.wait();
        }
        elem = buf[get.pos];
        if (++get.pos == buf_size) get.pos = 0;
        if (put.wait) { 
            put.sem.signal();
            put.wait -= 1;
        }
        cs.leave();
        return *this;
    }
    fifo_queue(size_t size) : put(cs), get(cs), buf_size(size) { 
        buf = NEW T[size];
    }
    ~fifo_queue() { delete[] buf; }
};
        
class GOODS_DLL_EXPORT barrier { 
  protected:
    mutex   cs;
    eventex e;
    int     count;

  public:
    void reset(int n) { 
        count = n;
        e.reset();
    }
    void reach() { 
        cs.enter();
        assert(count > 0);
        if (--count != 0) { 
            e.wait();
        } else { 
            e.signal();
        }
        cs.leave();
    }
    barrier() : e(cs) { count = 0; } 
};

END_GOODS_NAMESPACE

#endif
