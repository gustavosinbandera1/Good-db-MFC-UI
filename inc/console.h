// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CONSOLE.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update:  1-Mar-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Handle information and error messages at virtual console.
//-------------------------------------------------------------------*--------*

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#if DEBUG_LEVEL == DEBUG_TRACE
#define TRACE_MSG(x) console::message x
#define SERVER_TRACE_MSG(x) server->message x
#else
#define TRACE_MSG(x)
#define SERVER_TRACE_MSG(x)
#endif

#include <time.h>
#include "goodsdlx.h"
#include "task.h"

BEGIN_GOODS_NAMESPACE

enum console_message_classes {
    msg_time      = 0x001, // include timestamp in message
    msg_error     = 0x002, // messages about some non-fatal errors
    msg_notify    = 0x004, // notification messages
    msg_login     = 0x008, // messages about clients login/logout
    msg_warning   = 0x010, // messages about some possibly incorrect operations
    msg_important = 0x020, // trace messages about some important events
    msg_object    = 0x040, // trace operations with object instances
    msg_locking   = 0x080, // object lock related tracing
    msg_request   = 0x100, // trace requests received by server
    msg_gc        = 0x200, // trace garbage collector activity
    msg_output    = 0x400, // message which will be always shown

	// [MC] : used for logging custom messages
	msg_custom	  = 0x100000,	// messages used to log internal events

    msg_all       = ~0,    // output all messages
    msg_none      = 0      // ignore all messages
};

class GOODS_DLL_EXPORT console {
  protected:
    FILE* log;
    mutex log_mutex;

    enum output_type {
        con_data,
        con_message,
        con_error
    };

    virtual void output_data(output_type, const char* msg, va_list args);
    virtual boolean input_data(char* buf, size_t buf_size);
    virtual boolean input_data(char* buf, size_t buf_size, time_t timeout);

  public:
    static console* active_console;
    static int      trace_mask;

    virtual void log_data(const char* msg, va_list args);

    //
    // Output data at console.
    //

    static void output(const char* msg, ...);
    void out_put(const char* msg, ...);
    void out_data(const char* msg, va_list args) {
        output_data(con_data, msg, args);
    }

    //
    // Output data to the logfile only.
    //

    static void to_log(const char* msg, ...);
    //
    // Output message of class belonging to trace_mask at console
    //
    static void message(int message_class_mask, const char* msg, ...);
    static void vmessage(int message_class_mask, const char* msg, va_list args);
    void put_message(int message_class_mask, const char* msg, va_list args);

    //
    // Output message at console and terminate program.
    //
    static void error(const char* msg, ...);

    //
    // Input information from console
    //
    static boolean input(char* buf, size_t buf_size);
    static boolean input(char* buf, size_t buf_size, time_t timeout);

    static void use_log_file(FILE* log, boolean auto_close = True);

    console();
    virtual ~console() {};
};


class GOODS_DLL_EXPORT server_console: public console {
public:
    int      server_trace_mask;
    virtual void message(const char *name, stid_t id, int message_class_mask, const char* msg, va_list args);
};

END_GOODS_NAMESPACE

#endif



