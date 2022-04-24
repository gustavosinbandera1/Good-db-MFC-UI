// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< UNISOCK.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update:  7-Jan-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Unix socket 
//-------------------------------------------------------------------*--------*

#ifndef __UNISOCK_H__
#define __UNISOCK_H__

#include "sockio.h"

BEGIN_GOODS_NAMESPACE
class unix_socket : public socket_t { 
    friend class async_event_manager; 
  protected: 
    descriptor_t  fd; 
    int           errcode;     // error code of last failed operation 
    char*         address;     // host address
    socket_domain domain;      // Unix domain or INET socket
    boolean       create_file; // Unix domain sockets use files for connection

#ifdef COOPERATIVE_MULTITASKING
    semaphore     input_sem;
    semaphore     output_sem;  
#endif

    enum error_codes { 
        ok = 0,
        not_opened = -1,
        bad_address = -2,
        connection_failed = -3,
        broken_pipe = -4, 
        invalid_access_mode = -5
    };

  public: 
    //
    // Directory for Unix Domain socket files. This directory should be 
    // either empty or be terminated with "/". Dafault value is "/tmp/"
    //
    static char* unix_socket_dir; 

    boolean   open(int listen_queue_size);
    boolean   connect(int max_attempts, time_t timeout,
                      time_t connectTimeout = WAIT_FOREVER);

    int       read(void* buf, size_t min_size, size_t max_size,time_t timeout);
    boolean   read(void* buf, size_t size);
    boolean   write(void const* buf, size_t size);

    boolean   is_ok(); 
    boolean   shutdown();
    void      clear_error();
    boolean   close();
    void      get_error_text(char* buf, size_t buf_size);
    char*     get_peer_name(nat2 *oPort = NULL);

    virtual void abort_input(void);

    socket_t* accept();
    boolean   cancel_accept();
    
    int       get_descriptor();

#ifdef COOPERATIVE_MULTITASKING
    boolean   wait_input();
    boolean   wait_input_with_timeout(time_t timeout, 
                                      boolean check_timeout_expired = False);
    boolean   wait_output();
    boolean   wait_output_with_timeout(time_t timeout,
                                       boolean check_timeout_expired = False);
#endif

    unix_socket(const char* address, socket_domain domain); 
    unix_socket(int new_fd);

    ~unix_socket();
};

END_GOODS_NAMESPACE

#endif

