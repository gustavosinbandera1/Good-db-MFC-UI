// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< OBJECT.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 15-Sep-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Base class for all GOODS classes
//-------------------------------------------------------------------*--------*

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

//
// There are some special values of 'obj' field of object_handle are used
// in order to avoid extra 'state' field because size of object handle
// should be as small as possible.
//
// INVALIDATED_OBJECT is used when invalidate signal comes from server
//   while object loading (obj == NULL). When loading and unpacking
//   of object are finished, 'invalidated' bit would be set in object state
// THROWN_OBJECT is set when object instance is thrown away from object cache
//   When object handle will be released by garbage collector
//   'forget_object' request will be send to server if value of object
//   is not NULL
// COPIED_OBJECT is used to make transient copy of persistent object.
//
#define THROWN_OBJECT         ((object*)1)
#define COPIED_OBJECT         ((object*)2)
#define INVALIDATED_OBJECT    ((object*)3)

//
// Object pointer is considered valid iff it is not equal to NULL,
// THROWN_OBJECT, COPIED_OBJECT or INVALIDATED_OBJECT
//
#define IS_VALID_OBJECT(obj)  ((char*)(obj) > (char*)INVALIDATED_OBJECT)

class database;
class obj_storage; 
class class_descriptor;
class field_descriptor;
class object_ref;
class object;

class GOODS_DLL_EXPORT object_handle {
  protected: 
    static dnm_object_pool handle_pool;

public:
	static hnd_t   find(obj_storage* storage, objref_t opid);
    
    static hnd_t   allocate(object* obj, obj_storage* storage = NULL);
    static void    deallocate(hnd_t hnd);

	static void    assign_persistent_oid(hnd_t hnd, objref_t opid);
	static void    deassign_persistent_oid(hnd_t hnd);
	static hnd_t   create_persistent_reference(obj_storage* storage,
											   objref_t opid, int n_refs = 1);
    static void    cleanup_object_pool();
    
#ifdef _DEBUG
    static void    dumpCachedDBhandles(void);
#endif

    //
    // Make object persistent. Assign object to the storage
    // which was explicitly specified for this object or 
    // to the storage of object referenced this object (parent_storage)
    //
    static void    make_persistent(hnd_t hnd, obj_storage* parent_storage);
    
    static void    remove_from_storage(hnd_t hnd);
    
    static void    remove_reference(hnd_t hnd) { 
        if (hnd != 0 && --hnd->access <= 0) {
            deallocate(hnd);
        }
    }

  public: 
    hnd_t          next;    // hash table collision chain
	objref_t       opid;    // persistent object identifier in storage
    nat4           access;  // access counter
    object*        obj;     // pointer to object in memory
    obj_storage*   storage; // storage of persistent object

#ifdef _DEBUG
    void dump(void);
#endif
};

//
// Part of object controlled by metaobject
//
class GOODS_DLL_EXPORT object_header {
  public: 
    hnd_t  hnd;             // object transient indentifier 

    hnd_t  next;            // persistent object is linked into either l2-list
    hnd_t  prev;            // of cached object (sorted in LRU order) 
                            // or in list of object participated in transaction
    hnd_t  cluster;         // object with which this one should be clustered
    int    monitor;         // index of object monitor 

    cpid_t cpid;            // object class persistent identifier (in storage)
    
    nat2   n_invokations;   // number of methods recursivly called
                            // for this objects

    nat4   crc;             // object CRC
    int    state;           // state of object, mask of following bits: 
    enum { 
        useful      = 0x0001, // persistent object was accessed more than once
        persistent  = 0x0002, // object is persistent (has persistet object ID)
        fixed       = 0x0004, // persistent object is fixed in memory
        modified    = 0x0008, // object was modified 
        in_trans    = 0x0010, // persistent object involved in transaction
        exl_locked  = 0x0020, // exclusive lock for the transaction duration 
        shr_locked  = 0x0040, // shared lock for the transaction duration 
        initialized = 0x0080, // on_loading method is called for object
        invalidated = 0x0100, // object instance is deteriorated
        reloading   = 0x0200, // invalidated object is reloaded from server
        accessed    = 0x0400, // persitence object was previously accessed
        removed     = 0x0800, // object was already deleted
        notifier    = 0x1000, // notification should be done on deterioration
        pinned      = 0x2000, // object is pinned in memory
        clustered   = 0x4000, // object is intended to be clustered with other object (is used as parameter of cluster_with method)
		verified    = 0x8000,  // CRC is calculated for the object
		created		= 0x10000 // object was created by this transaction
    };
    
    //
    // This method is called when write conflict between different processes
    // takes place. Returning 'True' means that conflict is solved and
    // changes made by current process override changes made by another
    // process. Returning 'False' cause abort of transaction.
    //
    virtual boolean on_write_conflict(); 

    //
    // Locking of object is impossible since it is locked by another process. 
    // This method is used for nowaiting locking protocol.
    // Parameter 'lck' specify lock request which was not granted. 
    // If this method returns 'True' lock request will be send to server
    // once again. So metaobject will try to lock object until
    // lock will granted or 'on_object_lock_failed' method returns 'False'.
    // Returning 'False' cause abort of transaction.
    //
    virtual boolean on_lock_failed(lck_t lck);

    //
    // This method is called when persistent object is loaded from storage
    //
    virtual void on_loading();

    //
    // Call object destructor and free memory used by object
    //
    virtual void remove_object(); 

    //
    // Destruct this object
    //
    void delete_object();

    object_header(hnd_t hp) { 
        hnd = hp; 
        monitor = 0;
        cpid = 0;
        n_invokations = 0;
        state = 0;
        cluster = NULL;
    }
    virtual ~object_header();  
};


class GOODS_DLL_EXPORT object : protected object_header { 
    friend class class_descriptor;
    friend class object_ref;
    friend class object_l2_list;
    friend class object_monitor; 
    friend class object_handle; 
    friend class obj_storage;
    friend class metaobject; 

  protected:
#ifdef PROHIBIT_EXPLICIT_DEALLOCATION  
    //
    // Prohibite explicit deletion of object by user. GOODS is using
    // implicit memory deallocation algorith garbage collection and 
    // explicit deallocation is not possible. To check such bugs at compiler
    // time the following protected meoth is defined. Unfortunately,
    // some C++ compilers implicitly call delete method if exeption was raised
    // by new operator. This is whu by default this option is disabled.
    //
    void operator delete(void* ptr);
#endif
    
    //
    // Change 'obj' reference in object_handle to new object.  
    // New object should not be persistent and accessed by other
    // methods. All control information (state, locks...) from original object
    // will be copied to new the object. Metaobject of new object is preserved.
    // Method returns pointer to 'new_obj'.
    //
    object* become(object* new_obj); 

    //
    // This method can be used only for persistent objects. 
    // It loads from database last version of this object.
    // Object is loaded in client's memory as transient object
    // without assigned persistent identifier. 
    //
    object* load_last_version(); 

    //
    // Check if object is abstract storage root, i.e. root object
    // automatically allocated when storage is created
    //
    virtual boolean is_abstract_root() const; 

    //
    // Constructor for persistent objects loaded from database.
    //
    object(hnd_t hnd, class_descriptor& desc, size_t varying_length = 0) 
    : object_header(hnd), 
      cls(desc), 
      mop(default_mop ? default_mop : desc.mop), 
      size(unsigned(desc.fixed_size + desc.varying_size*varying_length))
    {}
    static object* constructor(hnd_t hnd, class_descriptor& desc, size_t); 
    
  public:
	  //[MC] -- change static member self_class to function to avoid race condition
    static class_descriptor&  self_class; 
    class_descriptor&        cls;  // object class(points to 'self_class')

    static metaobject*       default_mop; 
    metaobject*              mop;  // metaobject

    const  unsigned          size; // size of object in memory

    //
    // Create list of all class components descriptors
    //
    virtual field_descriptor& describe_components();

    //
    // Explicit control of transaction
    //
    void begin_transaction();
    void abort_transaction();
    void end_transaction();
    
    //
    // Default new operators
    //
#ifdef CHECK_FOR_MEMORY_LEAKS
    void* operator new(size_t size, int block_type, char const* file, unsigned int line);
#else
    void* operator new(size_t size);
#endif
    void* operator new(size_t, class_descriptor& desc, size_t varying_length);

    /**
     * Add object to the cluster: try to allocate new object together with specified object.
     * @param r cluster's owner: object to which cluster new object should be added. 
     * 'r' object may be not yet persisted: not assigned OID. In this case GOODS will allocate object 'r' before 'this' object.
     * @param chained whether this objects will also be used as target for clustering with other objects.
     * If this parameter is true and this object is failed to be clustered (allocated) together with referenced object, 
     * then it will be allocated aligned on page boundary to let other objects be allocated near it.
     * Otherwise it will be allocated in normal way (not preserving space for further append to the cluster).
     */
    void cluster_with(object_ref const& r, boolean chained = False);

    //
    // Attach object to the storage with specified identifier.
    // Object should not be already persistent.
    // Object will not be stored in the storage 
    // until it will be referenced by some other persistent object from 
    // the same database.
    //
    void attach_to_storage(database const* db, int sid) const;

    //
    // Signal event when persistent object is modified by other client
    //
    void signal_on_modification(event& e) const;

    //
    // Remove signal handler
    //
    void remove_signal_handler(event& e) const;

    hnd_t get_handle() const { return hnd; }

	database* get_database() const;

    //
    // Wait until object monitor is signaled.
    //
    void wait() const; 
    //
    // Wait for specified time until object monitor is signaled.
    // This method returns False if object is not signaled within 
    // specified timeout. If object is signaled before timeout expiration
    // 'True' is returned.
    //
    boolean wait(time_t timeout) const; 
    //
    // Switch object monitor to signaled state.
    //
    void notify() const;

    object(class_descriptor& desc, size_t varying_length = 0)
    : object_header(object_handle::allocate(this)), 
      cls(desc), 
      mop(default_mop ? default_mop : desc.mop), 
      size(unsigned(desc.fixed_size + desc.varying_size*varying_length)) {}

    object(class_descriptor& desc, metaobject* meta, size_t varying_length=0) 
    : object_header(object_handle::allocate(this)), 
      cls(desc), 
      mop(meta), 
      size(unsigned(desc.fixed_size + desc.varying_size*varying_length)) {}

    ~object();  
};

//
// Class for default storage root object
//

class GOODS_DLL_EXPORT storage_root : public object { 
  public: 
    virtual boolean is_abstract_root() const; 
    METACLASS_DECLARATIONS(storage_root, object);
};

END_GOODS_NAMESPACE

#endif
