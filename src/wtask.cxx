// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< WTASK.CXX >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      5-Apr-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 10-Jun-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Tasking implemented for Win32
//-------------------------------------------------------------------*--------*

#include "stdinc.h"
#pragma  hdrstop
#include "task.h"

BEGIN_GOODS_NAMESPACE

//
// Task 
//

task* task::create(fptr f, void* arg, priority pri, size_t stack_size)
{ 
    DWORD threadid;
    HANDLE h = CreateThread(NULL, stack_size, 
                            LPTHREAD_START_ROUTINE(f), arg,
                            CREATE_SUSPENDED, &threadid);
    if (h == NULL) { 
        console::error("CreateThread failed with error code=%d\n", GetLastError());
        return NULL;
    }
    SetThreadPriority(h, THREAD_PRIORITY_LOWEST + 
                (THREAD_PRIORITY_HIGHEST - THREAD_PRIORITY_LOWEST) 
                * (pri - pri_background) 
                / (pri_realtime - pri_background));
    ResumeThread(h);
    CloseHandle(h);
    return (task*)(size_t)threadid; 
}
 
GOODS_DLL_EXPORT int task_internals::tlsIndex;

class task_internels_initialize : public task_internals { 
  public:
    task_internels_initialize() { 
        tlsIndex = TlsAlloc();
    }
    ~task_internels_initialize() { 
        TlsFree(tlsIndex);
    }
};

static task_internels_initialize initializer;

void task::initialize(size_t) {}

void task::reschedule() {}

task* task::current() 
{ 
    return (task*)(size_t)GetCurrentThreadId();
}

void task::exit() 
{
    ExitThread(0);
} 

void task::sleep(time_t sec)
{
    Sleep(sec*1000);
}

END_GOODS_NAMESPACE
