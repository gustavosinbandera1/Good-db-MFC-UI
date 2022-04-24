// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< TCONSOLE.CXX >------------------------------------------------+-----------
// GOODS                     Version 1.0         (c) 2000  SETER   |
// (Generic Object Oriented Database System)                       |   *  /\v
//                                                                 |_/\__/  \__
//                        Created:     18-Jan-00    Marc Seter     |/  \/    \_
//                        Last update: 18-Jan-00    Marc Seter     |Marc Seter
//-----------------------------------------------------------------+-----------
// Provide an interactive session for a database administrator using a telnet
// port.  It also provides a console override that simply logs output to a
// logfile.
//-----------------------------------------------------------------------------

#include "stdinc.h"
#include <assert.h>
#include "tconsole.h"
#include "sockio.h"

BEGIN_GOODS_NAMESPACE


static const long kMaxOutput = 8192;

/*****************************************************************************
telnet_console - expected constructor
 *****************************************************************************/

telnet_console::telnet_console(socket_t *ioSocket, console *ioBaseConsole) :
console(),
mBaseConsole(ioBaseConsole),
mSocket(ioSocket)
{
}


/*****************************************************************************
~telnet_console - destructor
 *****************************************************************************/

telnet_console::~telnet_console(void)
{
    if(mSocket)
        delete mSocket;
}


/*****************************************************************************
input_data - read the specified number of characters from this console
-------------------------------------------------------------------------------
This function reads a line of characters from this console, copying the
characters into the specified destination string until the specified number of
characters is copied or a newline is encountered.  If this function returns
True, the specified destination will contain a NULL-terminated string (the
newline is replaced with a NULL character).  Otherwise if False is returned,
nothing could be read or zero characters were requested (dst_size was 0).

dst      specifies a character buffer, into which the line read from this
         console is copied
dst_size specifies the maximum number of characters to copy into 'dst'
timeout  if not WAIT_FOREVER, specifies the number of seconds of inactivity
         that will cause this function to abort
 *****************************************************************************/

boolean telnet_console::input_data(char* dst, size_t dst_size)
{
    return input_data(dst, dst_size, WAIT_FOREVER);
}

boolean telnet_console::input_data(char* dst, size_t dst_size, time_t timeout)
{
    char    buf;
    boolean done = False;
    size_t  i    = 0;

    if(dst_size == 0)
        return False;

    while(!done)
    {
        if(mSocket->read(&buf, 1, 1, timeout) <= 0)
            return False;

        if(buf == '\n')
        {
            dst[i] = '\0';
            done = True;
        }
        else
        {
            dst[i++] = buf;
            done = (i >= dst_size);
        }
    }

    return True;
}


/*****************************************************************************
output_data - write the specified formatted string to this console
-------------------------------------------------------------------------------
This function writes the specified formatted string across this console's
telnet connection.  If the base console this console is temporarily replacing
has a log file, the specified formatted string is also written to the log file.
 *****************************************************************************/

void telnet_console::output_data(output_type, const char* msg, va_list args)
{
#ifdef va_copy
    va_list  bc_args = nullptr;
#endif
    char    *buffer;
    long     len;

    // After calling vsprintf, `args' is undefined.  So we need a copy.
#ifdef va_copy
    if(mBaseConsole) {
        va_copy(bc_args, args);
    }
#endif

    // Write the formatted string.
    assert(mSocket != NULL);
    if(!(buffer = new char[kMaxOutput]))
        return;
    len = vsprintf(buffer, msg, args);
    mSocket->write(buffer, len);

    // Also write to the base console's log file, if it exists.
#ifdef va_copy
    if(mBaseConsole)
        mBaseConsole->log_data(msg, bc_args);
#else
    if(mBaseConsole)
        mBaseConsole->log_data(msg, args);
#endif

    delete[] buffer;
}


/*****************************************************************************
log_console - expected constructor
 *****************************************************************************/

log_console::log_console(console *ioBaseConsole) :
mBaseConsole(ioBaseConsole)
{
}


/*****************************************************************************
~log_console - destructor
 *****************************************************************************/

log_console::~log_console(void)
{
    if(mBaseConsole)
        delete mBaseConsole;
}


/*****************************************************************************
input_data - do nothing, since this console should only be used for output
 *****************************************************************************/

boolean log_console::input_data(char* buf, size_t buf_size)
{
    return input_data(buf, buf_size, WAIT_FOREVER);
}

boolean log_console::input_data(char*, size_t, time_t timeout)
{
    // If this assertion fails, someone tried to obtain input from this console,
    // which is not allowed.  Logging consoles are used solely for output.
    assert(False);
    return False;
}


/*****************************************************************************
output_data - output data to the logfile only
 *****************************************************************************/

void log_console::output_data(output_type, const char* msg, va_list args)
{
    assert(mBaseConsole != NULL);
    mBaseConsole->log_data(msg, args);
}


/*****************************************************************************
log_data - output data to the logfile only
 *****************************************************************************/

void log_console::log_data(const char* msg, va_list args)
{
    assert(mBaseConsole != NULL);
    mBaseConsole->log_data(msg, args);
}

END_GOODS_NAMESPACE
