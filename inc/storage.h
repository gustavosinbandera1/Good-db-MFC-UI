// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< STORAGE.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 23-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Abstract database storage interface.  
//-------------------------------------------------------------------*--------*

#ifndef __STORAGE_H__
#define __STORAGE_H__

#include "goodsdlx.h"
#include "protocol.h"

BEGIN_GOODS_NAMESPACE

class obj_storage;

//
// This class is used to provide interface with application to storage. 
// Since storage is implemented in application independent way it
// requires an agent to handle servers requests 
//
class GOODS_DLL_EXPORT dbs_application { 
  public: 
    virtual ~dbs_application() {};

    //
    // Function which is called when server informs client
    // that object instance was changed. 
    //
	virtual void invalidate(stid_t sid, objref_t opid) = 0;
    //
    // Function is called when server is diconnected from client
    //
    virtual void disconnected(stid_t sid) = 0;

    //
    // Authorization procedure fails at server sid 
    //
    virtual void login_refused(stid_t sid) = 0;

    //
    // receive a user message
    //
    virtual void receive_message( int message) = 0;
};  

   
//
// Interface to database object storage. All methods waiting answer 
// from server should be called synchronously 
// (concurrent requests are not allowed)
//

class GOODS_DLL_EXPORT dbs_storage { 
  protected:
    //
    // Identifier of storage within database. 
    // 
    const stid_t    id;
    //
    // Interface with application
    //
    dbs_application* application;

    nat8 num_bytes_in;
    nat8 num_bytes_out;

public:
	virtual objref_t  allocate(cpid_t cpid, size_t size, int flags, objref_t clusterWith) = 0;
	virtual void    bulk_allocate(size_t sizeBuf[], cpid_t cpidBuf[], size_t nAllocObjects,
								  objref_t opidBuf[], size_t nReservedOids, hnd_t clusterWith[]) = 0;
	virtual void    deallocate(objref_t opid) = 0;

	virtual boolean lock(objref_t opid, lck_t lck, int attr) = 0;
	virtual void    unlock(objref_t opid, lck_t lck) = 0;
    
    //
    // Get class descriptor by class identifier.
    // Class descriptor is placed in the buffer supplied by application.
    // If there is no such class at server buf.size() is set to 0.
    //
    virtual void    get_class(cpid_t cpid, dnm_buffer& buf) = 0;
    //
    // Store class descriptor at server receive it's identifier
    //
    virtual cpid_t  put_class(dbs_class_descriptor* desc) = 0;
    //
    // Change existed class descriptor
    //
    virtual void    change_class(cpid_t cpid, dbs_class_descriptor* desc) = 0;
    //                                        
    // Send a message to others clients. 
    //
    virtual void    send_message( int message) = 0;
    //
    // Schedule a user message to others clients. 
    //
    virtual void    push_message( int message) = 0;
    //
    // Send all scheduleds user messages to others clients. 
    //
    virtual void    send_messages() = 0;
                                            
    //
    // Load object from server into the buffer supplied by application.
    // Before each object dbs_object_header structure is placed.
    // If there is no such object at server then "cpid" field of 
    // dbs_object_header is set to 0.
    //
	virtual void    load(objref_t* opid, int n_objects,
						 int flags, dnm_buffer& buf) = 0;

	virtual void    load(objref_t opid, int flags, dnm_buffer& buf) = 0;


	virtual void    query(objref_t& first_mbr, objref_t last_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members, dnm_buffer& buf) = 0;

	virtual void    query(objref_t owner, char const* table, char const* where, char const* order_by, nat4 limit, nat4 offset, dnm_buffer& buf) = 0;

    //
    // Inform server that client no more has reference to specified object
    // 
	virtual void    forget_object(objref_t opid) = 0;
    //
    // Inform server that client no more has instance of specified object
    //
	virtual void    throw_object(objref_t opid) = 0;

    //
    // Initiate transaction at server. Allocate place for transaction header
    // in buffer. All objects involved in transaction should be appended 
    // to this buffer. 
    //
    virtual void    begin_transaction(dnm_buffer& buf) = 0; 

	//
	// Rollback transaction in which this storage was involved
	//
	virtual void    rollback_transaction() = 0;

    //
    // Commit local transaction or part of global transaction 
    // at coordinator server. In latter case coordinator will return 
    // transaction indentifier which should be 
    // passed to all other servers participated in transaction.
    // If transaction is aborted by server "False" is returned. 
    //
    virtual boolean commit_coordinator_transaction(int n_trans_servers, 
                                                   stid_t* trans_servers,
                                                   dnm_buffer& buf, 
                                                   trid_t& tid) = 0;
    //
    // Commit local part of global transaction at server. 
    //
    virtual void    commit_transaction(stid_t coordinator, 
                                       int n_trans_servers,
                                       stid_t* trans_servers,
                                       dnm_buffer& buf, 
                                       trid_t tid) = 0;
    //
    // Wait completion of global transaction (request to coordinator)
    // If transaction is aborted by server "False" is returned. 
    //
    virtual boolean wait_global_transaction_completion() = 0;

    //
    // Establish connection with server, returns 'False' if connection failed
    //
	virtual boolean open(const char* server_connection_name, char const* login = NULL, char const* password = NULL, obj_storage* os = NULL) = 0;
	virtual void    close() = 0;

    virtual nat8    get_used_size() = 0;

    virtual boolean start_gc() = 0;

    virtual void    add_user(char const* login, char const* password) = 0;
    virtual void    del_user(char const* login) = 0;

    void clear_num_bytes_in_out() { num_bytes_in = 0; num_bytes_out = 0; }
    nat8 get_num_bytes_in() { return num_bytes_in; }
    nat8 get_num_bytes_out()   { return num_bytes_out;   }

	virtual boolean convert_goods_database(char const* path, char const* name) = 0;
	virtual int execute(char const* sql) = 0;

	/**
	* Get socket descriptor for this database connection
	* @return socket descriptor
	*/
	virtual int get_socket() = 0;

	/**
	* Process PostgreSQL notification messages (if any)
	*/
	virtual void process_notifications() = 0;

	/**
	* Wait notification event
	*/
	virtual void wait_notification(event& e) = 0;

	virtual void listen(hnd_t opid, event& e) = 0;
	virtual void unlisten(hnd_t hnd, event& e) = 0;

    dbs_storage(stid_t sid, dbs_application* app) 
    : id(sid), application(app), num_bytes_in(0), num_bytes_out(0) {}
    virtual ~dbs_storage() {}
};

END_GOODS_NAMESPACE

#endif

