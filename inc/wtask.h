// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< WTASK.H >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      5-Apr-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 23-Sep-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Tasking implementation for Win32
//-------------------------------------------------------------------*--------*

#ifndef __WTASK_H__
#define __WTASK_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

#define task_proc WINAPI

#define MAX_SEM_VALUE 1000000

class GOODS_DLL_EXPORT mutex_internals { 
    friend class eventex_internals;
    friend class semaphorex_internals;
  protected: 
    CRITICAL_SECTION cs;

    void enter() { EnterCriticalSection(&cs); }
    void leave() { LeaveCriticalSection(&cs); }

    mutex_internals() { InitializeCriticalSection(&cs); }
    ~mutex_internals() { DeleteCriticalSection(&cs); } 
};

class GOODS_DLL_EXPORT semaphore_internals { 
  protected: 
    HANDLE h;
    
    descriptor_t get_handle() { return h; }

    void wait() { WaitForSingleObject(h, INFINITE); }
    boolean wait_with_timeout(time_t sec) { 
        return WaitForSingleObject(h, sec*1000) == WAIT_OBJECT_0;
    }
    void signal() { ReleaseSemaphore(h, 1, NULL); }

    semaphore_internals(int init_count) { 
        h = CreateSemaphore(NULL, init_count, MAX_SEM_VALUE, NULL); 
    }
    ~semaphore_internals() { CloseHandle(h); }
};

class GOODS_DLL_EXPORT semaphorex_internals { 
  protected: 
    HANDLE h;
    CRITICAL_SECTION* cs;

    void wait() { 
        LeaveCriticalSection(cs);
        WaitForSingleObject(h, INFINITE); 
        EnterCriticalSection(cs);
    }
    boolean wait_with_timeout(time_t sec) { 
        LeaveCriticalSection(cs);
        int result = WaitForSingleObject(h, sec*1000);
        EnterCriticalSection(cs);
        return result == WAIT_OBJECT_0;
    }
    void signal() { ReleaseSemaphore(h, 1, NULL); }

    semaphorex_internals(mutex_internals& guard, int init_count) { 
        cs = &guard.cs;
        h = CreateSemaphore(NULL, init_count, MAX_SEM_VALUE, NULL); 
    }
    ~semaphorex_internals() { CloseHandle(h); }
};

class GOODS_DLL_EXPORT event_internals { 
  protected: 
    HANDLE h;

    descriptor_t get_handle() { return h; }

    void wait() { WaitForSingleObject(h, INFINITE); }

    boolean wait_with_timeout(time_t sec) { 
        return WaitForSingleObject(h, sec*1000) == WAIT_OBJECT_0;
    }

    void signal() { SetEvent(h); }

    void reset() { ResetEvent(h); }

    event_internals(boolean signaled) { 
        h = CreateEvent(NULL, True, signaled, NULL); 
    }
    ~event_internals() { CloseHandle(h); }
}; 

class GOODS_DLL_EXPORT eventex_internals { 
  protected: 
    HANDLE  h;
    HANDLE  s;
    int     wait_queue_len;
    boolean signaled;
    CRITICAL_SECTION* cs;

    void wait() { 
        if (!signaled) { 
            wait_queue_len += 1;
            LeaveCriticalSection(cs);
            WaitForSingleObject(h, INFINITE);
            ReleaseSemaphore(s, 1, NULL);
            EnterCriticalSection(cs);
        }
    }

    boolean wait_with_timeout(time_t sec) { 
        if (!signaled) { 
            wait_queue_len += 1;
            LeaveCriticalSection(cs);
            int result =  WaitForSingleObject(h, sec*1000);
            ReleaseSemaphore(s, 1, NULL);
            EnterCriticalSection(cs);
            return result == WAIT_OBJECT_0;
        }
        return True;
    }
        
    void signal() { // should be called with cs mutex locked
        SetEvent(h); 
        signaled = True;
        while (wait_queue_len != 0) { 
            WaitForSingleObject(s, INFINITE);
            wait_queue_len -= 1;
        } 
    }

    void reset() { // should be called with cs mutex locked
        signaled = False;
        ResetEvent(h); 
    }

    eventex_internals(mutex_internals& guard, boolean signaled)  { 
        wait_queue_len = 0;
        this->signaled = signaled;
        cs = &guard.cs;
        h = CreateEvent(NULL, True, signaled, NULL); 
        s = CreateSemaphore(NULL, 0, MAX_SEM_VALUE, NULL); 
    }
    ~eventex_internals() { 
        CloseHandle(h); 
        CloseHandle(s); 
    }
}; 

class GOODS_DLL_EXPORT task_internals { 
  protected:
    static int tlsIndex;
    static void* get_task_specific() { 
        return TlsGetValue(tlsIndex);
    }
    
    static void set_task_specific(void* data) { 
        TlsSetValue(tlsIndex, data);
    }

    // currently only implemented for ctask model...
    nat8 get_profiling_usec(void) { return 0; }
    void start_profiling(void)    { }
    nat8 stop_profiling(void)     { return 0; }
};


END_GOODS_NAMESPACE

#endif
