// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< ASYNC.H >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 18-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Asyncronous event manager for cooperative multitasking in Unix 
//-------------------------------------------------------------------*--------*

#ifndef __ASYNC_H__
#define __ASYNC_H__

#if defined(__FreeBSD__)
#include <sys/types.h>
#endif

#include "goodsdlx.h"
#include "stdinc.h"
#include "unisock.h"

BEGIN_GOODS_NAMESPACE

class GOODS_DLL_EXPORT async_event_manager { 
  private:
    static unsigned n_desc;
    static fd_set input_desc; 
    static fd_set output_desc; 
    
    static time_t select_timeout; 

    static dnm_array<unix_socket*> sockets; // attached sockets
    static l2elem active_timers; 

  public: 
    static void add_timer(ctimer* tmr);

    static void attach_input_channel(unix_socket* s);
    static void detach_input_channel(unix_socket* s);
    static void attach_output_channel(unix_socket* s);
    static void detach_output_channel(unix_socket* s);

    static void select(boolean wait);
}; 

END_GOODS_NAMESPACE

#endif





