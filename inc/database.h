// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< DATABASE.H >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 16-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Application specific database interface
//-------------------------------------------------------------------*--------*

#ifndef __DATABASE_H__
#define __DATABASE_H__

#include "storage.h"
#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

class class_descriptor; 
class object_ref; 
class cache_manager;
class transaction_manager;
class database; 
class object; 
class object_handle; 
class basic_metaobject;
class result_set_cursor;

//
// This class obtain application dependent interface with database storage. 
// It is responsible for: 
//   1) synchronization of request to database storage
//   2) mapping between database and application class descriptors
//   3) convertion of loaded and stored objects
//   4) checking storage availablity 
//
class GOODS_DLL_EXPORT obj_storage : public dbs_application { 
    friend class database; 
    friend class object; 
    friend class object_handle; 
    friend class basic_metaobject;

  protected: 
    dnm_array<class_descriptor*> descriptor_table;
    dnm_array<cpid_t>            cpid_table;
    mutex                        cs; 

    //
    // Number of references to objects from this storage
    // The 'storage' object can be removed when there are no more references
    // to it from any object (n_references == 0)
    //
    long              n_references; 
    dnm_buffer        loadBuf; 
    dnm_buffer        transBuf; 
    boolean           opened;

	objref_t*         alloc_opid_buf;
    hnd_t*            alloc_near_buf;
    size_t*           alloc_size_buf;
    cpid_t*           alloc_cpid_buf;
    volatile size_t   alloc_buf_pos;
    volatile size_t   alloc_buf_size;

  public: 
    database* const   db; 
    const stid_t      id;
	dbs_storage*      storage;
 
    void   add_reference() { 
        n_references += 1; 
    }

    void   remove_reference() { 
        if (--n_references == 0) { 
            delete this; 
        }
    }
    
    boolean is_opened() {
        return opened;
    }

    //
    // Send a user message to others clients. 
    //
    virtual void    send_message( int message);

    //
    // Schedule a user message to others clients. 
    //
    virtual void    push_message( int message);

    //
    // Send all scheduleds user messages to others clients. 
    //
    virtual void    send_messages();

    //
    // called by server to receive a user message. 
    //
    virtual void    receive_message( int message);

    //
    // Method called by server to invalidate object instance
    //
	virtual void invalidate(stid_t sid, objref_t opid);

    //
    // Method called by server to notify about server disconnection
    //
    virtual void disconnected(stid_t sid); 

    //
    // Authorization procedure fails at server sid
    // 
    virtual void login_refused(stid_t sid);

    //
    // Inform server that client no more has reference to specified object 
    //
	void    forget_object(objref_t opid) {
        if (opened) { 
            storage->forget_object(opid); 
        }
    }

    //
    // Inform server that client no more has instance of specified object
    //
	void    throw_object(objref_t opid) {
        if (opened) { 
            storage->throw_object(opid); 
        }
    }
    
    //
    // Get server class identifier for specified application class
    //
    cpid_t  get_cpid_by_descriptor(class_descriptor* desc);

    //
    // Allocate object at server
    //
	objref_t  allocate(cpid_t cpid, size_t size, int flags, hnd_t cluster_with);

    //
    // Deallocate object at server
    //
	void    deallocate(objref_t opid);

    //
    // Download object from server. This method is called with
    // global lock set, release lock while waiting for object and
    // set it again after receiving object from server
    //
    void    load(hnd_t hnd, int flags); 

    //
    // Download objects from server. This method is called with
    // global lock set, release lock while waiting for objects and
    // set again after receiving object from server.
    // Use this method to pre-load a group of objects in one
    // database request - much faster than one at a time.
    //
    void    load(hnd_t *hnds, int num_hnds, int flags);

    //
    // Query objects from server
    // 
	void    query(result_set_cursor& cursor, objref_t& first_mbr, objref_t last_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members);

	void    query(result_set_cursor& cursor, objref_t owner, char const* table, char const* where, char const* order_by, nat4 limit, nat4 offset);

    //
    // Set new lock or upgrade an existed one (from shared to exclusive)
    //
	boolean lock(objref_t opid, lck_t lck, int attr);
    //
    // Remove or downgrade lock. 
    //
	void    unlock(objref_t opid, lck_t lck) { storage->unlock(opid, lck); }
    
    //
    // Transaction protocol. 
    // No other storage methods (except forget_object and throw_object)
    // can be called until transaction completion. This syncronization should
    // be done by metaobject protocol. 
    //
    void    begin_transaction(); 
	void    rollback_transaction();    
    void    include_object_in_transaction(hnd_t hnd, int flags);
    boolean commit_coordinator_transaction(int n_trans_servers,
                                           stid_t* trans_servers,
                                           trid_t& tid);
    void    commit_transaction(stid_t coordinator, 
                               int n_trans_servers,
                               stid_t* trans_servers,
                               trid_t tid);
    boolean wait_global_transaction_completion();

    boolean open(char const* connection_address, char const* login = NULL, char const* password = NULL);
    void    close();

    void    add_user(char const* login, char const* password);
    void    del_user(char const* login);

    nat8    get_used_size();

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
    
    obj_storage(database* dbs, stid_t sid); 
    virtual~obj_storage();
};

//
// Class 'database' is collection of storages.
// It's main responsibility is transaction handling. 
//
class GOODS_DLL_EXPORT database { 
  protected:
    friend class object;
    friend class obj_storage;
    friend class field_descriptor; 

    obj_storage**  storages;
    int            n_storages;

    boolean        opened; 
    mutex          cs;

    int            fetch_flags;
    size_t         alloc_buf_size;

  protected: 
    virtual dbs_storage* create_dbs_storage(stid_t sid) const;
    virtual obj_storage* create_obj_storage(stid_t sid);

    //
    // Handle of one of storage servers disconnection
    //
    virtual void    disconnected(stid_t sid);

    //
    // Handle authorization error
    //
    virtual void    login_refused(stid_t sid);
 
  public:
	obj_storage*    get_storage(stid_t sid) const {
        internal_assert(sid < n_storages);
        return storages[sid];
    }
  
    virtual boolean open(const char* database_configuration_file, char const* login = NULL, char const* password = NULL);
    virtual void    close();
    
    //
    // Enable fetching of object clusters. When enabled server will send to the client
    // not only the requested object, but also objects which are directly or indirectly
    // referenced from the requested object. Maximal size of the cluster is configured
    // at server. 
    //
    virtual void    enable_clustering(boolean enabled);

    //
    // Set size of object allocation buffer. Server will return to the client set of OIDs
    // which can be used by client. At the end of transaction all new objects will be allocated
    // in the database using one bulk alloc operations. 
    //
    virtual void    set_alloc_buffer_size(size_t size);

    //
    // Send a message to others clients. 
    //
    virtual void    send_message( int message);

    //
    // Schedule a user message to others clients. 
    //
    virtual void    push_message( int message);

    //
    // Send all scheduleds user messages to others clients. 
    //
    virtual void    send_messages();

    //
    // called by server to receive a user message. ( do nothing by default). 
    //
    virtual void    receive_message( int message);

    //
    // Attach current thread to the database connection
    //
    virtual void    attach(); 

    //
    // Detach current thread from the database connection
    //
    virtual void    detach(); 

    //
    // Get root object of specified storage
    //
    void get_root(object_ref& ref, stid_t sid = 0);

    //
    // Get a reference to an object from its object ID.
    //
	void get_object(object_ref& ref, objref_t opid, stid_t sid = 0);

    //
    // Get number of storages
    //
    int  get_number_of_storages() const { return n_storages; }

    nat8 get_used_size(stid_t sid = 0) { return get_storage(sid)->get_used_size(); }

    stid_t get_storage_with_minimal_size();

    void add_user(char const* login, char const* password);
    void del_user(char const* login);

    boolean start_gc();

    void clear_num_bytes_in_out(void);
    void clear_num_bytes_in_out(stid_t sid);
    nat8 get_num_bytes_from_db(void);
    nat8 get_num_bytes_from_storage(stid_t sid);
    nat8 get_num_bytes_from_transaction(void);
    nat8 get_num_bytes_to_db(void);
    nat8 get_num_bytes_to_storage(stid_t sid);
    nat8 get_num_bytes_to_transaction(void);

	/**
	* Convert database from GOODS to Postgres
	* @param path GOODS database directory path
	* @param name database name
	* @return true if database was successfully converterd, false otherwise
	*/
	boolean convert_goods_database(char const* path, char const* name);

	/**
	* Execute INSERT/UPDATE/DELETE or DDL statement
	* @param sql SQL statement to be executed at server
	* @return number of affected rows
	*/
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

    //
    // Remove all statically allocated objects.
    // This method may be used after database close to remove all statically allocated objects and so makes
    // avoid messages about memory leaks. 
    // Please notice, that you can call cleanup only when there are no active instances of GOODS database
    // and you can not establish new database connection after cleanup is performed.
    //
    static void cleanup();

    database();
    virtual ~database();
};    

END_GOODS_NAMESPACE

#endif
