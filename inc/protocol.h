// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< PROTOCOL.H >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 17-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// This file defines protocol of client-server and server-server communication
//-------------------------------------------------------------------*--------*

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "convert.h"

BEGIN_GOODS_NAMESPACE

#define NIL_OPID   0x00000 
#define RAW_CPID   0x00001 
#define MIN_CPID   0x00002
#define MAX_CPID   0x0FFFF
#define ROOT_OPID  0x10000

// used in Smalltalk API
#define TAGGED_POINTER 0x8000

// used in bulk alloc request
#define ALLOC_ALIGNED 0x80000000

//
// Format of object reference in storage
//

struct dbs_reference_t {
#if PGSQL_ORM
	nat1 oid[6];
	nat1 cid[2];
#else
	nat1 store_id[2];   // prevent compiler from fields alignment
	nat1 object_id[4];
#endif
};

enum transaction_object_flags {
    tof_none       = 0,  
    tof_update     = 1, // request to update object
    tof_validate   = 2, // request to validate transaction object at server
    tof_unlock     = 4, // request to unlock transaction object at the end
                        // of transaction
    tof_unlock_exl = 8, // request to release exclusive lock at the end
                        // of transaction
    tof_closure    = 16,// object is part of closure: is referenced from explicitly requested object
	tof_change_metadata = 32,// transaction updating class definition
	tof_new		   = 64 // object was created in this transaction
};

enum load_object_flags { 
    lof_none     = 0,
    lof_copy     = 1, // request another copy of object
    lof_cluster  = 2, // enable sending cluster of objects in one message
    lof_auto     = 4, // use storage defaults
    lof_pin      = 8, // pin loaded objects in memory
	lof_bulk     = 16,// bulk load of object set
	lof_update   = 32 // load object for update
}; 


enum alloc_object_flags { 
    aof_none      = 0,
    aof_aligned   = 1, // align object on page boundary
    aof_clustered = 2  // allocate object near some other object
};

//
// Format of passing persistent object
//
struct dbs_object_header { 
    nat4  flags;     // transaction object flags
    nat4  opid;      // object identifier within store
    nat2  cpid;      // class identifier for store
    nat2  sid;       // index of store within database
    nat4  size;      // packed size of object in store

    char* body() { return (char*)this + sizeof(dbs_object_header); }

	opid_t get_opid() { return unpack4((char*)&this->opid); }
    void set_opid(opid_t opid) { pack4((char*)&this->opid, opid); }

    void set_cpid(cpid_t cpid) { pack2((char*)&this->cpid, cpid); }
    void set_size(size_t size) { pack4((char*)&this->size, (nat4)size); }
    void set_flags(nat4 flags) { pack4((char*)&this->flags, flags); }

	cpid_t get_cpid()          { return unpack2((char*)&this->cpid); }
    size_t get_size()          { return unpack4((char*)&this->size); }
    nat4   get_flags()         { return unpack4((char*)&this->flags); }
	
#if PGSQL_ORM
	objref_t get_ref()         { return MAKE_OBJREF(get_cpid(), ((objref_t)unpack2((char*)&this->sid) << 32) | get_opid()); }
	stid_t   get_sid()         { return 0; }
	void     set_sid(stid_t sid){}
	void     set_ref(objref_t oref) { set_opid((opid_t)GET_OID(oref)); pack2((char*)&this->sid, (nat2)(GET_OID(oref) >> 32)); }
#else
	objref_t get_ref()         { return get_opid(); }
	stid_t   get_sid()         { return unpack2((char*)&this->sid); }
	void     set_sid(stid_t sid){ pack2((char*)&this->sid, sid); }
	void     set_ref(objref_t oref) { set_opid((opid_t)oref); }
#endif
}; 


//
// Format of storing information about class in database
//

enum field_type { 
    fld_structure,
    fld_reference,
    fld_signed_integer, 
    fld_unsigned_integer, 
    fld_real,
    fld_string, 
    fld_raw_binary, 
    fld_last = 100
};

#define IS_SCALAR(f) \
((f) == fld_signed_integer || (f) == fld_unsigned_integer || (f) == fld_real)

struct dbs_field_descriptor { 
    nat2 type;       // type of field
    nat2 name;       // offset to name of field
    nat4 size;       // size of field
    nat4 n_items;    // number of components in array
    nat4 next;       // next field in structure

    boolean is_varying() { return n_items == 0; }

    void pack();
    void unpack();
};

struct dbs_class_descriptor { 
    nat4 fixed_size;
    nat4 varying_size;
    nat4 n_fixed_references;
    nat4 n_varying_references;

    nat4 n_fields;  // number of all fields in class (including subclasses)
    nat4 total_names_size; // total size of names of all fields 
    
    union { 
        dbs_field_descriptor fields[1];
        char                 names[1];
    };

    char* name() { 
        return &names[n_fields*sizeof(dbs_field_descriptor)]; 
    }

    size_t get_size() const { 
        return sizeof(dbs_class_descriptor) 
            + n_fields*sizeof(dbs_field_descriptor) 
            - sizeof(dbs_field_descriptor) 
            + total_names_size; 
    }

    unsigned get_number_of_references(size_t size) const { 
        return n_varying_references 
            ? n_fixed_references + unsigned((size - fixed_size) / varying_size * n_varying_references)
            : n_fixed_references; 
    }

    unsigned get_varying_length(size_t size) const { 
        return varying_size ? unsigned((size - fixed_size) / varying_size) : 0;
    }

    dbs_class_descriptor* pack();
    dbs_class_descriptor* unpack();

    boolean operator == (dbs_class_descriptor const& dbs_desc) const;
    boolean operator != (dbs_class_descriptor const& dbs_desc) const { 
        return !(*this == dbs_desc); 
    }

    //
    // Create descriptor with specified number of fields and total names size
    //
    void* operator new(size_t size, size_t n_fields, size_t nm_size) {
        size += (n_fields-1)*sizeof(dbs_field_descriptor)+nm_size;
        return ::operator new(size);
    }
    //
    // Create descriptor with specified size
    //
    void* operator new(size_t, size_t size) { 
        return ::operator new(size);
    }

	void operator delete(void* p) {
		::operator delete(p);
	}

    dbs_class_descriptor* clone(); 
};

//
// Lock request types
// 
enum lck_t { 
    lck_none,      // No lock
    lck_shared,    // Prevent object from been modified or exclusively 
                   // locked by other processes
    lck_exclusive, // Prevent object from been modified or locked
                   // (shared or exclusive) by other processes
    lck_temporary_exclusive, // exclusive lock which is hold until release is performed and then downgraded to shared
    lck_update     // shared lock which can be upgraded to exclusive 
};

//
// Attributes of locks are represented as bits in bit mask
//
enum lckattr_t { 
    lckattr_no = 0,
    lckattr_nowait = 1 // Lock request will not block, if granting lock is
                       // impossible, request will be immediately refused 
};

#define MAX_LOGIN_NAME    255 // limitation for login name length
#define MAX_TRANS_SERVERS 255 // maximal number of servers participated in 
                              // global transaction

enum query_flags {
    qf_default         = 0,
    qf_backward        = 1, // backward traversal of set members
    qf_closure         = 2, // include in response all referenced object
    qf_insensitive     = 4, // case insensitive string comparison
    qf_include_members = 8  // Include the set_members together with the objects.
};    

struct dbs_request { 
    enum command_code { 
        cmd_load,       // client wants to receive object from server
        cmd_object,     // server sent object to client
        cmd_forget,     // client remove reference to loaded object
        cmd_throw,      // client throw away instance of object from cache

        cmd_invalidate, // server inform client about object modification

        cmd_getclass,   // get information about class from server
        cmd_classdesc,  // server send class descriptor requested by cpid

        cmd_putclass,   // register new class at server 
        cmd_classid,    // class identifier returned to client by server

        cmd_modclass,   // modify existed class at server

        cmd_lock,       // request to server from client to lock object
        cmd_lockresult, // result of applying lock request at server
        cmd_unlock,     // request to server from client to unlock object

        cmd_transaction,// client sends transaction to coordinator
        cmd_subtransact,// client sends local part of transaction to server
        cmd_transresult,// coordinator returns transaction status to client
        cmd_tmsync,     // interserver transaction synchronization 

        cmd_login,      // client login at server 
        cmd_logout,     // client finish the session
        cmd_connect,    // one server connects to other 
        cmd_bye,        // server notifies client or other server 
                        // about it's termination
        cmd_ok,         // successful authorization 
        cmd_refused,    // authorization procedure is failed at server

        cmd_alloc,      // client request server to allocate object 
        cmd_location,   // server returns address of allocated object
        cmd_free,       // client request to free object
        
        cmd_bulk_alloc, // client request server to allocate several objects and reserve OIDs 
        cmd_opids,      // object identifiers reserved by server for the client

        cmd_gcsync,     // interserver garbage collection synchronization 

        cmd_get_size,   // get storage size

        cmd_synchronize,// request from started standby server to primary server to synchronize its state

	cmd_user_msg,     // cmd_user_msg are used for clients who want to pass messages to others clients.

	// synchronous versions of some of methods above (methods with acknowledgement)
	cmd_forget_ack, 
	cmd_throw_ack, 
	cmd_modclass_ack, 
	cmd_lock_ack, 
	cmd_unlock_ack, 
	cmd_free_ack, 
	cmd_ack, // acknowledgement 
	cmd_ack_lock, // acknowledgement for lock request

        cmd_start_gc,   // db client's asking server to initiate garbage collection
        cmd_gc_started, // db server's response to client's start_gc request
        
        cmd_ping,       // db server's asking client or peer to respond
        cmd_ping_ack,   // db client or peer is responding
        
        cmd_add_user,   // register new login
        cmd_del_user,   // delete user
        
        cmd_query,      // execute query
        cmd_query_result, // query result

        //cmd_ztransaction,// client sends compressed transaction to coordinator
        //cmd_zsubtransact,// client sends compressed local part of transaction to server
		cmd_select,      // new query command
	cms_last
    };

    union { 
        nat1 cmd;
        struct { 
            nat1 cmd;
            nat1 arg1;
            nat2 arg2;
            nat4 arg3;
            nat4 arg4; 
            nat4 arg5; // can be used by application
        } any;
        
        struct {  
            nat1 cmd;
            nat1 arg1;
            nat2 flags;
            nat4 opid;
            nat4 extra; 
        } object;

        struct { 
            nat1 cmd;
            nat1 type;
            nat2 attr;
            nat4 opid;
        } lock;

        struct {
            nat1 cmd;
            nat1 n_servers; // number of servers participated in transaction
            nat2 coordinator; // coordinator of global transaction
            nat4 size;
            nat4 tid;
            nat4 crc;
        } trans;

        struct {
            nat1 cmd;
            nat1 n_servers; // number of servers participated in transaction
            nat2 coordinator; // coordinator of global transaction
            nat4 size;
            nat4 tid;
            nat4 orig_size; // decompressed size
        } ztrans; // compressed transaction

        struct { 
            nat1 cmd;
            nat1 flags; // alloc_object_flags
            nat2 cpid;
            nat4 size;
            nat4 cluster;
        } alloc;
 
        struct { 
            nat1 cmd;
            nat1 flags; // alloc_object_flags
            nat2 cpid;
            nat4 n_objects;
            nat4 reserved;
        } bulk_alloc;
 
        struct { 
            nat1 cmd;
            nat1 arg1;
            nat2 cpid;
            nat4 size;
        } clsdesc;

        struct { 
            nat1 cmd;
            nat1 arg1;
            nat2 sid;
            nat4 name_len;
        } login;

        struct { 
            nat1 cmd;
            nat1 flags;
            nat2 query_len;
            nat4 member_opid;
            nat4 max_buf_size;
            nat4 max_members;
        } query;

		struct {
			nat1 cmd;
			nat1 flags;
			nat2 query_len;
			nat4 owner;
			nat4 limit;
			nat4 offset;
		} select;

		struct {
			nat1 cmd;
			nat1 status;
			nat2 arg2;
			nat4 size;
			nat4 tid;
			nat4 orig_size;
		} result;

        struct { 
            nat1 cmd;
            nat1 fun;
            nat2 sid;
            union { 
                nat4 tid;
                nat4 seqno;
            };
        } tm_sync;

        struct { 
            nat1 cmd;
            nat1 fun;
            nat2 arg2;
            union { 
                nat4 len;
                nat4 n_refs;
                nat4 timestamp;
            };
        } gc_sync;

        struct { 
            nat1 cmd;
            nat1 arg1;
            nat2 arg2;
            nat4 log_offs_low;  // low part of offset of last recovered transaction at standby node
            nat4 log_offs_high; // high part of offset of last recovered transaction at standby node
        } sync;
    };

    // convert all fields to universal representation
    void pack() { 
        pack2(any.arg2);
        pack4(any.arg3);
        pack4(any.arg4);
        pack4(any.arg5);
    } 

    // convert all fields to local host representation
    void unpack() { 
        unpack2(any.arg2);
        unpack4(any.arg3);
        unpack4(any.arg4);
        unpack4(any.arg5);
    }
};

extern nat4 calculate_crc(void* buf, int size);

/* Request description:
+------------------+-----------+----------------+-----------------+-----------+
|   request code   |   arg1    |     arg2       |      arg3       |   arg4    |
+------------------+-----------+----------------+-----------------+-----------+
| cmd_load         |           |loading flags   |object identifier|           |
| cmd_object       |           |                |size of object(s)|           |
| cmd_forget       |           |                |object identifier|extra reqs.|
| cmd_throw        |           |                |object identifier|extra reqs.|
| cmd_lock         |lock type  |lock attributes |object identifier|           |
| cmd_unlock       |lock type  |                |object identifier|           |
| cmd_lockresult   |lock result|                |                 |           |
| cmd_invalidate   |           |                |object identifier|extra reqs.|
| cmd_getclass     |           |class identifier|                 |           |
| cmd_classdesc    |           |                |size of descrip. |           |
| cmd_putclass     |           |                |size of descrip. |           |
| cmd_classid      |           |class identifier|                 |           |
| cmd_modclass     |           |class identifier|size of descrip. |           |
| cmd_transaction  | n_servers |   coordinator  |transaction size |           |
| cmd_subtransact  | n_servers |   coordinator  |transaction size |    tid    |
| cmd_transresult  |tran.status|                |                 |    tid    |
| cmd_alloc        |           |class identifier| size of object  |           |
| cmd_location     |           |                |object identifier|           |
| cmd_free         |           |                |object identifier|           |
| cmd_tmsync       |I M P L E M E N T A T I O N    D E P E N D E N T          |
| cmd_gcsync       |I M P L E M E N T A T I O N    D E P E N D E N T          |
| cmd_login        |           |                | client name size|           |
| cmd_logout       |           |                |                 |           |
| cmd_connect      |           |   server id    | server name size|           |
| cmd_bye          |           |                |                 |           |
| cmd_refused      |           |                |                 |           |
| cmd_ok           |           |                |                 |           |
+------------------+-----------+----------------+-----------------+-----------+
*/

END_GOODS_NAMESPACE

#endif




