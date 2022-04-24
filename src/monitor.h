// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< MONITOR.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      1-Oct-2002  K.A. Knizhnik  * / [] \ *
//                          Last update:  1-Oct-2002  K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Transaction monitor for GOODS clients. This monitor
// allows to replicate client requests to multiple servers
// and so provide fault tolerance. 
//-------------------------------------------------------------------*--------*

#ifndef __MONITOR_H__
#define __MONITOR_H__

#if defined(__FreeBSD__)
#include <sys/types.h>
#endif

#include "goodsdlx.h"
#include "stdinc.h"
#include "task.h"
#include "sockio.h"

BEGIN_GOODS_NAMESPACE

class transaction_monitor;

enum server_fault { 
    server_connection_failure, 
    no_active_servers, 
    corrupted_server_response,
    unexpected_server_response,
    unable_to_choose_correct_server_response
};

#define CMD_HISTORY_SIZE 1024
struct command_history {
    int1 cmd;
    int1 server_id;
};

class client_proxy { 
    friend class transaction_monitor;
  private:    
    socket_t*  client_socket;
    socket_t** server_socket;
    int        n_servers;
    int        expected_response;
    int1*      received_response;
    int1*      lock_acknowledgement;
    int8*      n_invalidations;
    int8       n_sent_invalidations;
    dnm_buffer buf;
    dnm_buffer cmp_buf[2];
    int        n_different_responses;
    int        n_responses;
    int        response_from_server[2];
    int        bad_response;
    int        client_cmd;
    boolean    proceed_lock_request;
    boolean    broadcast_completed;

    command_history* cmd_history;
    unsigned         cmd_history_pos;

  public:
    boolean broadcast(transaction_monitor* monitor);
    boolean reply(int server_id, transaction_monitor* monitor);
    client_proxy(socket_t* client_sock, char** servers, int n_server);
    ~client_proxy();
};

struct channel_descriptor { 
    client_proxy* proxy;
    int           server_id;
};

class transaction_monitor {
    friend class client_proxy;
  private:
    socket_t*            gateway;
    char**               servers;
    int                  n_servers;
    fd_set               server_descriptors;
    fd_set               descriptors;
    int                  n_descriptors;
    boolean              running;
    boolean              select_started;
    channel_descriptor*  channels;
    int                  n_channels;
    int                  n_clients;
    eventex              activate_select_task;
    event                accept_task_terminated;
    event                select_task_terminated;
    mutex                cs;

  public:
    transaction_monitor(char*  proxy, 
			char** servers, 
			int    n_servers);
    void start();
    void stop();
    void dump();

    virtual void on_server_fault(int server_id, socket_t* socket,
				 server_fault status);
    virtual ~transaction_monitor();

  protected:
    void accept();
    boolean select_server_responses(client_proxy* proxy);
    void select();

    void add_channel(client_proxy* proxy, int server_id);
    void remove_channel(client_proxy* proxy, int server_id);
    
    static void task_proc accept_task(void* arg);
    static void task_proc select_task(void* arg);
};

END_GOODS_NAMESPACE

#endif



