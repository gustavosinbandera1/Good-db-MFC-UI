// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CLIENT.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 23-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Client store interface. This store is responsible for tranferring          
// client requests to server and receiving server's replies.  
//-------------------------------------------------------------------*--------*

#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "sockio.h"

BEGIN_GOODS_NAMESPACE

class GOODS_DLL_EXPORT dbs_client_storage : public dbs_storage 
{ 
  protected:
    socket_t*       sock;

    semaphore       rcv_sem;  // enable request receiving
    semaphore       rep_sem;  // reply received
    mutex           snd_cs;   // sending in progress

    boolean         opened;
    boolean         closing;
    event           term_event; 

	dnm_buffer      snd_buf;
    dbs_request     reply;
    
    volatile int    waiting_reply;
    volatile boolean proceeding_invalidation;

    mutex           notify_cs;
    dnm_array<dbs_request> notify_buf;

    mutex           request_cs;

    mutex           send_msg_cs;
    dnm_array<dbs_request> send_msg_buf;

    virtual void    read(void* buf, size_t size);
    virtual void    write(void const* buf, size_t size);
    virtual boolean handle_communication_error();

    static void task_proc receiver(void*); 
    virtual void    receive_header();

    virtual void    send_receive(void const*  snd_buf, 
                                 size_t       snd_len,
                                 dbs_request& rcv_req, 
                                 int          exp_cmd);

    virtual void    send_receive(void const*  snd_buf, 
                                 size_t       snd_len,
                                 dbs_request& rcv_req, 
                                 int          exp_cmd,
                                 dnm_buffer&  rcv_buf);

    virtual void    send_notifications();

public:
	virtual void    bulk_allocate(size_t sizeBuf[], cpid_t cpidBuf[], size_t nAllocObjects,
								  objref_t opidBuf[], size_t nReservedOids, hnd_t clusterWith[]);
	virtual objref_t  allocate(cpid_t cpid, size_t size, int flags, objref_t clusterWith);
	virtual void    deallocate(objref_t opid);

	virtual boolean lock(objref_t opid, lck_t lck, int attr);
	virtual void    unlock(objref_t opid, lck_t lck);

    virtual void    get_class(cpid_t cpid, dnm_buffer& buf);
    virtual cpid_t  put_class(dbs_class_descriptor* desc);
    virtual void    change_class(cpid_t cpid, dbs_class_descriptor* desc);
                                            
    //
    // Load object from server into buffer. 
    //
	virtual void    load(objref_t* opid, int n_objects, int flags,
						 dnm_buffer& buf);
	virtual void    load(objref_t opid, int flags, dnm_buffer& buf);

	virtual void    query(objref_t& first_mbr, objref_t last_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members, dnm_buffer& buf);

	virtual void    query(objref_t owner, char const* table, char const* where, char const* order_by, nat4 limit, nat4 offset, dnm_buffer& buf);

    //
    // Send immediately a message to others clients. 
    //
    virtual void    send_message( int message);

    //
    // Schedule a message to others clients. 
    //
    virtual void    push_message( int message);

    //
    // Send all scheduleds messages to others clients. 
    //
    virtual void    send_messages();

    //
    // Inform server about state of object cache
    //
	virtual void    forget_object(objref_t opid);
	virtual void    throw_object(objref_t opid);

    //
    // Initiate transaction at server
    //
    virtual void    begin_transaction(dnm_buffer& buf); 

	virtual void    rollback_transaction();
    virtual boolean commit_coordinator_transaction(int     n_trans_servers, 
                                                   stid_t* trans_servers,
                                                   dnm_buffer& dnm, 
                                                   trid_t& tid);
    virtual void    commit_transaction(stid_t coordinator, 
                                       int n_trans_servers,
                                       stid_t* trans_servers,
                                       dnm_buffer& dnm, 
                                       trid_t tid);
    virtual boolean wait_global_transaction_completion();

    //
    // Establish connection with server, returns 'False' if connection failed
    //
	virtual boolean open(const char* server_connection_name, const char* login = NULL, const char* password = NULL, obj_storage* os = NULL);
    virtual void    close();

    virtual nat8    get_used_size();
    virtual void    add_user(char const* login, char const* password);
    virtual void    del_user(char const* login);

    virtual boolean start_gc();

	boolean convert_goods_database(char const* path, char const* name);

	int execute(char const* sql);

	/**
	* Get socket descriptor for this database connection
	* @return socket descriptor
	*/
	int get_socket();

	/**
	* Process PostgreSQL notification messages (if any)
	*/
	void process_notifications();

	/**
	* Wait notification event
	*/
	void wait_notification(event& e);

	void listen(hnd_t hnd, event& e);
	void unlisten(hnd_t hnd, event& e);

    dbs_client_storage(stid_t sid, dbs_application* app) 
    : dbs_storage(sid, app) {} 
    virtual~dbs_client_storage();
};
END_GOODS_NAMESPACE

#endif

