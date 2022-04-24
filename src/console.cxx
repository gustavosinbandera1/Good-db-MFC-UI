// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CONSOLE.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 26-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Handle information and error messages at virtual console.
//-------------------------------------------------------------------*--------*

#include "stdinc.h"
#include "sockio.h"
#include <fcntl.h>

BEGIN_GOODS_NAMESPACE

GOODS_DLL_EXPORT console* console::active_console;
int console::trace_mask = msg_important|msg_error|msg_notify|msg_login;
static console default_console;

console::console ()
{
    log = NULL;
    if (active_console == NULL || active_console == &default_console) {
        active_console = this;
    }
}

void console::to_log(const char* msg, ...)
{
    va_list args;
    va_start (args, msg);
    assert(active_console != NULL);
    active_console->log_data(msg, args);
    va_end (args);
}

void console::output(const char* msg, ...)
{
    va_list args;
    va_start (args, msg);
    assert(active_console != NULL);
    active_console->output_data(con_data, msg, args);
    va_end (args);
}

void console::out_put(const char* msg, ...)
{
    va_list args;
    va_start (args, msg);
    output_data(con_data, msg, args);
    va_end (args);
}

void console::message(int message_class_mask, const char* msg, ...)
{
    if ((trace_mask | msg_output) & message_class_mask & ~msg_time) {
        va_list args;
        va_start (args, msg);
        assert(active_console != NULL);
        active_console->put_message(message_class_mask, msg, args);
        va_end (args);
        }
}

void console::vmessage(int message_class_mask, const char* msg, va_list args)
{
    if ((trace_mask | msg_output) & message_class_mask & ~msg_time) {
        assert(active_console != NULL);
        active_console->put_message(message_class_mask, msg, args);
        }
}

void server_console::message(const char *name, stid_t id, int message_class_mask, const char* msg, va_list args)
{
        if ((server_trace_mask | msg_output) & message_class_mask & ~msg_time) {
            char format[256];
            if (name && *name) {
                if (strlen(name) + 8 + strlen(msg) > sizeof format) {
                    out_put("%s#%d ", name, id);
                    put_message(message_class_mask, msg, args);
                }
                else {
                    sprintf(format, "%s#%d %s", name, id, msg);
                    put_message(message_class_mask, format, args);
                }
            }
            else
                put_message(message_class_mask, msg, args);
        }
}

void console::put_message(int message_class_mask, const char* msg, va_list args)
{
        if (message_class_mask & msg_time) {
            static const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                     "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
            char format[256];
            time_t aclock;
            time(&aclock);
            struct tm* tp = localtime(&aclock);
            if (8 + 1 + 9 + 3 + strlen(msg) > sizeof format) {
                out_put("%02u:%02u.%02u %02u-%s-%u: ",
                       tp->tm_hour, tp->tm_min, tp->tm_sec,
                       tp->tm_mday, months[tp->tm_mon], 1900+tp->tm_year);
                output_data(con_message, msg, args);
            } else {
                sprintf(format, "%02u:%02u.%02u %02u-%s-%u: %s",
                        tp->tm_hour, tp->tm_min, tp->tm_sec,
                        tp->tm_mday, months[tp->tm_mon], 1900+tp->tm_year,
                        msg);
                output_data(con_message, format, args);
            }
        } else {
            output_data(con_message, msg, args);
        }
}

void console::error(const char* msg, ...)
{
    va_list args;
    va_start (args, msg);
    assert(active_console != NULL);
    active_console->output_data(con_error, msg, args);
    va_end (args);
#ifndef ERROR_DONT_ABORT
    abort();
#endif
}

boolean console::input(char* buf, size_t buf_size)
{
    return input(buf, buf_size, WAIT_FOREVER);
}

boolean console::input(char* buf, size_t buf_size, time_t timeout)
{
    return active_console->input_data(buf, buf_size, timeout);
}

void console::use_log_file(FILE* log, boolean auto_close)
{
    if (auto_close && active_console->log != NULL) {
        fclose(active_console->log);
    }
    active_console->log = log;
}

void console::output_data(output_type, const char* msg, va_list args)
{
    critical_section cs(log_mutex);
    vfprintf(stdout, msg, args);
    log_data(msg, args);
}

void console::log_data(const char* msg, va_list args)
{
    if (log != NULL) {
        critical_section cs(log_mutex);
        vfprintf(log, msg, args);
        fflush(log);
    }
}

END_GOODS_NAMESPACE

#ifdef COOPERATIVE_MULTITASKING
#include <unistd.h>
#include "unisock.h"

BEGIN_GOODS_NAMESPACE

boolean console::input_data(char* dst, size_t dst_size)
{
    return input_data(dst, dst_size, WAIT_FOREVER);
}

boolean console::input_data(char* dst, size_t dst_size, time_t timeout)
{
    static unix_socket stdin_socket(0);
    static char  buf[4096];
    static char* bp = buf;
    static int   used;

    if (dst_size == 0) {
        return False;
    }
    int   src_size = used;
    char* src = bp;

    fflush(stdout);

    while (True) {
        while (src_size > 0 && dst_size > 1) {
            src_size -= 1;
            dst_size -= 1;
            if ((*dst++ = *src++) == '\n') {
                *dst = '\0';
                bp = src;
                used = src_size;
                return True;
            }
        }
        if (dst_size == 1) {
            *dst++ = '\0';
            bp = src;
            used = src_size;
            return True;
        } else {
            int rc = stdin_socket.read(buf, 1, sizeof buf, timeout);
            if (rc > 0) {
                src_size = rc;
                src = buf;
            } else {
                used = 0;
                bp = buf;
                return False;
            }
        }
    }
}

END_GOODS_NAMESPACE

#else

BEGIN_GOODS_NAMESPACE

boolean console::input_data(char* buf, size_t buf_size)
{
    return input_data(buf, buf_size, WAIT_FOREVER);
}

boolean console::input_data(char* buf, size_t buf_size, time_t timeout)
{
#ifdef _WIN32
    return fgets(buf, int(buf_size), stdin) != NULL;
#else
    if (timeout == WAIT_FOREVER) {
        return fgets(buf, buf_size, stdin) != NULL;
    } else {
        long    original = (long)fcntl(0, F_GETFL);
        boolean dataRead = false;

        fcntl(0, F_SETFL, O_NONBLOCK);
        while (!dataRead && (timeout > 0)) {
            if (fgets(buf, buf_size, stdin) != NULL) {
                dataRead = true;
            } else {
                task::sleep(1);
                timeout--;
            }
        }
        fcntl(0, F_SETFL, original);
        return dataRead;
    }
#endif
}

END_GOODS_NAMESPACE

#endif


