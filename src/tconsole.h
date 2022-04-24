// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< TCONSOLE.H >--------------------------------------------------+-----------
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

#ifndef __TCONSOLE_H__
#define __TCONSOLE_H__

#include "stdinc.h"
#include "console.h"
#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

class socket_t;

class GOODS_DLL_EXPORT telnet_console : public console
{
 protected :
   console  *mBaseConsole;
   socket_t *mSocket;

   virtual boolean input_data(char* buf, size_t buf_size);
   virtual boolean input_data(char* buf, size_t buf_size, time_t timeout);
   virtual void    output_data(output_type, const char* msg, va_list args);

 public :
            telnet_console(socket_t *ioSocket, console *ioBaseConsole);
   virtual ~telnet_console(void);
};

class GOODS_DLL_EXPORT log_console : public console
{
 protected :
   console  *mBaseConsole;

   virtual boolean input_data(char* buf, size_t buf_size);
   virtual boolean input_data(char* buf, size_t buf_size, time_t timeout);
   virtual void    output_data(output_type, const char* msg, va_list args);

 public :
   virtual void    log_data(const char* msg, va_list args);

            log_console(console *ioBaseConsole);
   virtual ~log_console(void);
};
END_GOODS_NAMESPACE

#endif
