// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< OBJMGR.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET   *     ?  *
// (Generic Object Oriented Database System)                        *   /\|  *
//                                                                   *  /  \  *
//                          Created:     18-Jan-97    K.A. Knizhnik * / [] \ *
//                          Last update: 14-Sep-97    K.A. Knizhnik * GARRET *
//------------------------------------------------------------------*--------*
// Object locking and updating manager
//------------------------------------------------------------------*--------*

#ifndef __OBJMGR_H__
#define __OBJMGR_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

class object_node;
class object_lock;
class object_instance;
class object_reference;
class object_access_manager; 

//
// Client application abstraction
//
class GOODS_DLL_EXPORT client_process : public l2elem { // list of clients 
  public: 
    const unsigned id;        // client identifier
    dbs_server*    server;
    long           req_count; // counter of client requests

    semaphore      lock_sem;  // semaphore for waiting lock requests

    boolean        terminated;  // client agent was terminated by server

    boolean        suspended;   // client is suspended due to lack of
                                // server resources

    long           n_instances; // number of object instances loded by clients
    long           n_invalid_instances; // number of deteriorated instances 
                                        // in clients caches

    l2elem         instances; // object instances cached by process
    object_lock*   locking;   // process <->> locked objects
    object_lock*   waiting;   // process <->  wait object to lock

    opid_t  firstReservedOpid;
    opid_t  lastReservedOpid;

    void mark_reserved_objects();

    //
    // Insert invalidation request for object instance to client
    // notification queue.
    //
    virtual void   invalidate(opid_t opid) = 0; 

    //
    // Send invalidation signals (from notification queue) to client
    //
    virtual void   notify() = 0;

    //
    // Break connection with client process
    //
    virtual void   disconnect()= 0;

    virtual char*  get_name() = 0;
    
    //
    // Send reply to client 
    //
    virtual void write(void const* buf, size_t size) = 0;

    client_process(dbs_server* server, int client_id) 
    : id(client_id)
    { 
        this->server = server; 
        req_count = 0;
        n_instances = 0;
        n_invalid_instances = 0;
        locking = waiting = NULL; 
        terminated = False;
        suspended = False;
        firstReservedOpid = lastReservedOpid = 0;
    }
    virtual ~client_process();
};

enum cis_state { // state of client instance of object
    cis_none,    // client has no instance of object
    cis_valid,   // client has valid instance of object
    cis_invalid, // client has invalid instance of object
    cis_new,     // new object was allocated but not yet stored by client
    cis_thrown   // client threw away instance of object 
}; 

class GOODS_DLL_EXPORT object_access_manager { 
  public: 
    // 
    // Try to lock object. This function returns 'False' if resource is
    // bloked by another process with incompatible lock and
    // either lock attribute is lckattr_nowait or 
    // timeout for lock waiting is expired. 
    //
    virtual boolean lock_object(opid_t  opid, 
                                lck_t   lck, 
                                int     attr, 
				boolean send_ack,
                                lck_t& prev_lck,// Mode of lock previosly 
                                // granted to client for this object 
                                client_process* proc) = 0;

    virtual void    unlock_object(opid_t          opid, 
                                  lck_t           lck, 
                                  client_process* proc) = 0;

    //
    // Provide information about client object instance state
    //
    virtual cis_state get_object_state(opid_t opid, client_process* proc) = 0;

    //
    // Client now has instance of specified object. Modification of object 
    // is prohibited until 'release_object' method will be called.  
    //
    virtual void  load_object(opid_t opid, int flags, client_process* proc)=0;
    //
    // Object is access by GC. Modification of object is prohibited until
    // 'release_object' method will be called.
    //
    virtual void  scan_object(opid_t opid) = 0;
    //
    // Release object. 
    //
    virtual void  release_object(opid_t opid) = 0;

    //
    // Client allocate new object
    //
    virtual void  new_object(opid_t opid, client_process* proc) = 0;
    //
    // Mark object as been updated in transaction
    //
    virtual void  modify_object(opid_t opid) = 0;
    //
    // Save old references and update object instance 
    //
    virtual void  write_object(opid_t opid, cpid_t new_cpid, fposi_t new_pos,
                               size_t new_size, void* new_body, 
                               client_process* proc) = 0; 

    //
    // Client no more has instance of specified object
    //
    virtual void  throw_object(opid_t opid, client_process* proc) = 0; 
    //
    // Client no more has reference to specified object
    //
    virtual void  forget_object(opid_t opid, client_process* proc) = 0; 

    //
    // Mark objects intances of which are loaded by clients. 
    // First time this function is called before mark stage of GC to mark
    // (by means of gc_follow_reference) objects loaded by clients.  
    // Second time this function is called before sweep stage of GC to mark
    // (by means of gc_mark_object) all objects created during GC.
    //
    virtual void  mark_loaded_objects(int pass) = 0; 

    //
    // This mwthod enables or disables saving versions of
    // all modified abkects till the end of garbage collection.
    //
    virtual void  set_save_version_mode(boolean enabled) = 0;

    //
    // Release all resources occupied by disconnected client process
    //
    virtual void  disconnect_client(client_process* proc) = 0;

    virtual void  dump(client_process* proc, const char* what) = 0;
    virtual void  dump(const char* what) = 0;

    //
    // Set timeout for waiting lock request granting
    //
    virtual void  set_lock_timeout(time_t timeout) = 0;

    //
    // Get number of objects controlled by this manager (loaded or locked by client)
    //
    virtual int   get_number_of_objects() = 0;

    virtual boolean open(dbs_server*) = 0;
    virtual void  initialize() = 0;
    virtual void  shutdown() = 0;
    virtual void  close() = 0;

    virtual ~object_access_manager();
};

class GOODS_DLL_EXPORT object_instance : public l2elem { // process <->> cached instance
  public: 
    object_instance* next_proc;   // object <->> cached instances
    client_process*  proc; 
    opid_t           opid; 
    cis_state        state; 

    void* operator new(size_t, void* addr) { return addr; }    
    ~object_instance() { unlink(); }
}; 
    
class GOODS_DLL_EXPORT object_lock { 
  public: 
    object_lock*     next_proc;  // object <->> locking/waiting processes  
    object_lock*     next_obj;   // process <->> locked object
    object_lock*     prev_obj;   // process <->> locked object
    client_process*  proc;
    object_node*     obj; 
    nat1             mode;
    nat1             attr; 

    void* operator new(size_t, void* addr) { return addr; }    
};

//
// Class used for saving references which were changed by transaction
// Information about this references is stored until all client
// having deteriorated versions of object with this references
// throw away instance of the object from thier caches. 
//
class GOODS_DLL_EXPORT object_reference { 
  public: 
    object_reference* next; 
    opid_t            opid; 
    stid_t            sid;

    void* operator new(size_t, void* addr) { return addr; }
    
    object_reference(stid_t sid, opid_t opid) { 
        this->sid = sid; 
        this->opid = opid; 
    }
    object_reference() {}
};

const size_t OBJECT_REF_SIZE = 6;

class GOODS_DLL_EXPORT object_node { 
  public: 
    object_node*      next;      // hash table collision chain
    object_lock*      waiting;   // object <->> waiting for lock processes
    object_lock*      locking;   // object <->> locking processes
    object_instance*  instances; // object <->> client's object instances

    opid_t            opid;

    nat2              n_invalid_instances;

    nat2              queue_len; // number if tasks waiting for object
    nat2              n_readers; // number of reader accessing object
    nat1              is_locked; // mutator locks the object

    nat1              state;     // object state
    enum { 
        pinned  = 0x01, // object node can't be remoced until end of GC
        scanned = 0x02, // object was acanned by GC
        raw     = 0x04  // object was not yet initialized
    };

    object_reference* saved_references; // saved references of deteriorated
                                        // object instances

    void* operator new(size_t, void* addr) { return addr; }

    object_node(opid_t opid) { 
        this->opid = opid; 
        next = NULL; 
        waiting = locking = NULL; 
        instances = NULL; 
        n_invalid_instances = 0;
        saved_references = NULL; 
        n_readers = 0;
        is_locked = False;;
        queue_len = 0;
        state = 0;
    } 
};



#define OBJECT_HASH_TABLE_INIT_SIZE 64*1024
#define REFERENCE_BUFFER_SIZE  1024
//[MC]
#define DEFAULT_LOCK_TIMEOUT   60000 //60 minutes

class object_hash_table { 
  public:
    object_node** table;
    int size;
    int used;

    inline object_node* get(opid_t opid) { 
        for (object_node* node = table[opid & (size-1)]; node != NULL; node = node->next) { 
            if (node->opid == opid) { 
                return node;
            }
        }
        return NULL;
    }

    inline void put(object_node* node) { 
        if (++used > size) { 
            int new_size = size*2;
            object_node** new_table = new object_node*[new_size];
            memset(new_table, 0, sizeof(object_node*)*new_size);
            
            for (int i = size; --i >= 0;) { 
                object_node* node = table[i]; 
                while (node != NULL) { 
                    object_node* next = node->next;
                    unsigned h = node->opid & (new_size-1);
                    node->next = new_table[h];
                    new_table[h] = node;
                    node = next;
                }
            }
            delete[] table;
            size = new_size;
            table = new_table;
        }
        unsigned h = node->opid & (size-1);
        node->next = table[h];
        table[h] = node;
    }

    inline void remove(object_node* node) { 
        object_node *op, **opp = &table[node->opid & (size-1)];
        while ((op = *opp) != node) { 
            opp = &op->next;
        }
        used -= 1;
        *opp = op->next;
    }
            

    object_hash_table() { 
        table = new object_node*[OBJECT_HASH_TABLE_INIT_SIZE];
        memset(table, 0,  sizeof(object_node*)*OBJECT_HASH_TABLE_INIT_SIZE);
        size = OBJECT_HASH_TABLE_INIT_SIZE;
        used = 0;
    }

    ~object_hash_table() { 
        delete[] table;
    }
};
     

class GOODS_DLL_EXPORT object_manager : public object_access_manager { 
  protected:
    boolean         opened; 
    dbs_server*     server; 

    dnm_object_pool object_node_pool;
    dnm_object_pool object_lock_pool;
    dnm_object_pool object_reference_pool;
    dnm_object_pool object_instance_pool;

    mutex           cs; 
    eventex         release_event;
    mutex           refs_cs; 
    
    boolean         pinned_versions_mode;

    dbs_reference_t refs_buf[REFERENCE_BUFFER_SIZE];
    
    object_hash_table hash_table;
    
    //
    // Abort process keeping lock on object for a very long time
    // Argument 'op' specifies objects been locked, and 'proc' - 
    // client process waiting for granting lock.
    //
    virtual void  abort_locker(object_node* op, client_process* proc);
    
    object_reference* save_old_version(object_node*      op, 
                                       cpid_t            new_cpid, 
                                       size_t            new_size, 
                                       void*             new_body,
                                       object_reference& chain);
    void  set_reader_lock(object_node* op);
    void  set_writer_lock(object_node* op);
    void  release_reader_lock(object_node* op);
    void  release_writer_lock(object_node* op);

    void              validate(object_node* op, object_instance* ip);
    void              invalidate(object_node* op, object_instance* ip);

    void              remove_old_references(object_node* op); 

    object_node*      create_object_node(opid_t opid);
    object_reference* create_object_reference(stid_t sid, opid_t opid);
    object_lock*      create_object_lock();
    object_instance*  create_object_instance();
    void              remove_object_node(object_node*);
    void              remove_object_lock(object_lock*);
    void              remove_object_reference(object_reference*);
    void              remove_object_instance(object_instance*);

  public:
    time_t        lock_timeout; 

    virtual boolean lock_object(opid_t          opid, 
                                lck_t           lck, 
                                int             attr, 
				boolean         send_ack,
                                lck_t&          prev_lck,
                                client_process* proc);

    virtual void  scan_object(opid_t opid);

    virtual void  release_object(opid_t opid);

    virtual void  unlock_object(opid_t opid, lck_t lck, 
                                client_process* proc);
    
    virtual cis_state get_object_state(opid_t opid, client_process* proc);

    virtual void  load_object(opid_t opid, int flags, client_process* proc);
 
    virtual void  new_object(opid_t opid, client_process* proc);

    virtual void  modify_object(opid_t opid);

    virtual void  write_object(opid_t opid, cpid_t new_cpid, fposi_t new_pos,
                                size_t new_size, void* new_body, 
                                client_process* proc); 

    virtual void  throw_object(opid_t opid, client_process* proc); 
    virtual void  forget_object(opid_t opid, client_process* proc); 

    virtual void  set_save_version_mode(boolean enabled);

    virtual void  mark_loaded_objects(int pass); 

    virtual void  disconnect_client(client_process* proc);

    virtual void  dump(client_process* proc, const char* what);
    virtual void  dump(const char* what);

    virtual void  set_lock_timeout(time_t timeout);

    virtual int   get_number_of_objects();

    virtual boolean  open(dbs_server*);
    virtual void  initialize();
    virtual void  shutdown();
    virtual void  close();

    object_manager(time_t lock_timeout = DEFAULT_LOCK_TIMEOUT); 
}; 
END_GOODS_NAMESPACE

#endif
