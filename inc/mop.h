// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< MOP.H >---------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 17-Nov-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Metaobjects are used to specify synchronization policy for GOODS objects
//-------------------------------------------------------------------*--------*

#ifndef __MOP_H__
#define __MOP_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

//
// Methods of metaobject are called with global mutex locked so
// their execution is not concurrent
//

typedef void (*abort_hook_t)(metaobject*);

enum transaction_isolation_level {
    PER_PROCESS_TRANSACTION,
    PER_THREAD_TRANSACTION,
    PER_CONNECTION_TRANSACTION
};

class GOODS_DLL_EXPORT metaobject {
    friend class cache_manager;
    friend class transaction_manager;
  public:
    virtual ~metaobject() {};

    //
    // Make transient object persistent as a result of
    // referencing to it from persistent object
    //
    virtual void make_persistent(hnd_t hnd, hnd_t parent_hnd) = 0;

	//
	// Make transient object persistent by explicit specification of storge
	//
	virtual void make_persistent(hnd_t hnd, obj_storage* storage) = 0;

    //
    // This method is called by object destructor and performs cleanup of
    // deleted object.
    //
    virtual void destroy(hnd_t hnd) = 0;

    //
    // Intertasking object locking
    //
    virtual void lock(hnd_t hnd, lck_t lck) = 0;
    virtual void unlock(hnd_t hnd) = 0;

    //
    // Intertasking synchronization primitives
    //
    virtual void wait(hnd_t hnd) = 0;
    virtual boolean wait(hnd_t hnd, time_t timeout) = 0;
    virtual void notify(hnd_t hnd) = 0;

    //
    // Access to object
    //
    virtual void begin_read(hnd_t hnd, boolean for_update) = 0;
    virtual void end_read(hnd_t hnd) = 0;

    virtual void begin_write(hnd_t hnd) = 0;
    virtual void end_write(hnd_t hnd) = 0;
	virtual void end_write_transient(hnd_t hnd);

    //
    // Instance of object is deteriorated
    //
    virtual void invalidate(hnd_t hnd) = 0;

    //
    // Signal event when persistent object is modified by another client
    //
    virtual void signal_on_modification(hnd_t hnd, event& e) = 0;

    //
    // Remove signal handler
    //
    virtual void remove_signal_handler(hnd_t hnd, event& e) = 0;

    //
    // Insert persistent object to object cache. Replacement policy
    // for objects should be implemented by this method
    //
    virtual void put_in_cache(hnd_t hnd) = 0;
    //
    // Prevent object from replacement algorithm activity.
    //
    virtual void get_from_cache(hnd_t hnd) = 0;
    //
    // Pin object in memory
    //
    virtual void pin_object(hnd_t hnd) = 0;
    //
    // Unpin object
    //
    virtual void unpin_object(hnd_t hnd) = 0;

    //
    // Specify boundaries of nested transaction. Parent transaction will
    // not be commited until all nested transactions are finished.
    //
	virtual void begin_transaction() = 0;
	virtual void end_transaction(const database* dbs) = 0;

    //
    // Transaction management
    //
    enum abort_reason { aborted_by_server, aborted_by_user };

	virtual void commit_transaction(const database* dbs) = 0;
    virtual void abort_transaction(const database* dbs,
                                   abort_reason reason = aborted_by_user) = 0;

    //
    // This methods are called after transaction completion
    // (normal or abnormal) for each object involved in transaction
    //
    virtual void commit_object_changes(hnd_t hnd) = 0;
    virtual void abort_object_changes(hnd_t hnd, abort_reason reason) = 0;

    //
    // This method is called for all object from transaction list
    // when no changes were made within transaction
    //
    virtual void release_transaction_object(hnd_t hnd) = 0;

    //
    // Specify flags for transaction object
    //
    virtual int get_transaction_object_flags(hnd_t hnd) = 0;

    //
    // Primary cache size (limit0) should be greater then maximal cluster size
    // otherwise cluster can never be loaded
    //
    virtual void set_cache_limit(size_t limit0, size_t limit1) = 0;

    //
    // Set hook which will be called if transaction will be aborted by server.
    // Hook is called with global mutex locked and should not release this lock
    //
    virtual void set_abort_transaction_hook(abort_hook_t hook) = 0;

	//
	// Returns true if object is not persisted in the storage
	//
	virtual boolean is_transient() = 0;

  protected:
    inline static object_header* get_header(hnd_t hnd) {
        return (object_header*)hnd->obj;
    }

    //
    // Insert object in database transaction list
    //
    virtual void insert_in_transaction_list(hnd_t hnd,
                                            hnd_t parent_hnd = 0) = 0;
    //
    // Remove object from database transaction list
    //
    virtual void remove_from_transaction_list(hnd_t hnd) = 0;
};

//
// Loaded persistent objects are linked either in l2-list of cached object
// or in l2-list of objects participated in transaction
//
class GOODS_DLL_EXPORT object_l2_list {
  public:
    hnd_t head;
    hnd_t tail;

    void put_at_head(object_header* obj) {
        assert(obj->hnd->obj == obj);
        obj->next = head;
        obj->prev = 0;
        if (head != 0) {
            head->obj->prev = obj->hnd;
        } else {
            tail = obj->hnd;
        }
        head = obj->hnd;
    }

    void put_at_tail(object_header* obj) {
        obj->next = 0;
        obj->prev = tail;
        if (tail != 0) {
            tail->obj->next = obj->hnd;
        } else {
            head = obj->hnd;
        }
        tail = obj->hnd;
    }

    void put_after(object_header* after, object_header* obj) {
        obj->next = after->next;
        obj->prev = after->hnd;
        after->next = obj->hnd;
        if (obj->next != 0) {
            obj->next->obj->prev = obj->hnd;
        } else {
            tail = obj->hnd;
        }
    }

    void put_before(object_header* before, object_header* obj) {
        obj->next = before->hnd;
        obj->prev = before->prev;
        before->prev = obj->hnd;
        if (obj->prev != 0) {
            obj->prev->obj->next = obj->hnd;
        } else {
            head = obj->hnd;
        }
    }

    void unlink(object_header* obj) {
        if (obj->next) {
            obj->next->obj->prev = obj->prev;
        } else {
            tail = obj->prev;
        }
        if (obj->prev) {
            obj->prev->obj->next = obj->next;
        } else {
            head = obj->next;
        }
        obj->next = obj->prev = 0;
    }

    int4 find(object_header* obj) {
        int4  count = 0;
        boolean  found = false;
        hnd_t hnd   = head;
        while (!found && hnd) {
            if (hnd->obj == obj) {
                found = true;
            } else {
                hnd = hnd->obj->next;
                count++;
            }
        }
        return found ? count : -1;
    }

    boolean empty() { return head == 0; }

    void truncate() { head = tail = 0; }

    object_l2_list() { truncate(); }
};


//
// Class providing mutual exclusion for access to object.
// Since size of 'monitor' object is not very small,
// allocating monitor for each object will be space expansive.
// So "turnstile" of monitor objects is used for more space-effective monitor
// allocation.
//
class task_lock_manager;
class GOODS_DLL_EXPORT object_monitor { 
    friend  class monitor_turnstile;
    friend  class task_lock_manager;
  protected: 
    static mutex* global_cs; 

    hnd_t       hnd;
    task*       owner;
#ifdef MURSIW_SUPPORT
    eventex*    evt;
    int         n_readers;
    int         n_writers;
    int         n_blocked;
    int         waiting;

    void wait(task* t);
#else
    mutex*      cs;
    semaphorex* sem;
    int         busy;
    int         signaled;
    int         n_nested_locks;
#endif

  public:
    //
    // Synchronize accesses to object system
    //
    inline static void lock_global() {
        global_cs->enter();
    }

    inline static void unlock_global() {
        global_cs->leave();
    }

#ifdef MURSIW_SUPPORT
	//[MC] for multi-lock implementation
	boolean is_locked_by_other();
    void release();
#endif

    boolean is_locked();
    boolean is_busy();

    void lock(lck_t lck);
    void unlock();
    void wait();
    boolean wait(time_t timeout);
    void notify();
    void attach(hnd_t new_owner);
    void detach();


    object_monitor();
    ~object_monitor();

    static void cleanup_global_cs(void);
};

#define INIT_TURNSTILE_SIZE 1024

class GOODS_DLL_EXPORT monitor_turnstile {
  private:
    int              size;
    int              pos;
    object_monitor** turnstile;

  public:
    boolean is_locked(int monitor);
    int     attach_monitor(hnd_t hnd);
    void    lock(int monitor, lck_t lck);
    void    unlock(int monitor);
    void    wait(int monitor);
    boolean wait(int monitor, time_t timeout);
    void    notify(int monitor);
    void    detach_monitor(int monitor);

#ifdef MURSIW_SUPPORT
	//[MC] for multi-lock implementation
	boolean is_locked_by_other(int monitor);
    void release(int monitor);
#endif

    monitor_turnstile(int init_size = INIT_TURNSTILE_SIZE);
    ~monitor_turnstile();
};

class GOODS_DLL_EXPORT transaction_manager_extension {
public:
	virtual ~transaction_manager_extension() {}
};

class GOODS_DLL_EXPORT transaction_manager {
  public:
    //
    // List of objects accessed within current transaction
    //
    object_l2_list trans_objects;

    event          trans_commit_event;
    boolean        trans_commit_wait_flag;
	boolean        trans_commit_in_progress;
	boolean        force_commit;

    unsigned       n_nested_transactions;
    int            exception_count;
    abort_hook_t   abort_hook;

    nat8           num_bytes_in;
    nat8           num_bytes_out;

	transaction_manager_extension* extension;

    static void set_isolation_level(transaction_isolation_level level);

    static  transaction_manager* get_thread_transaction_manager() {
        return (transaction_manager*)task::get_task_specific();
    }

    static  transaction_manager* get() {
        if (isolation_level == PER_PROCESS_TRANSACTION) {
            return &default_manager;
        }
        transaction_manager* mng = get_thread_transaction_manager();
        if (mng == NULL && isolation_level == PER_CONNECTION_TRANSACTION) {
            return &default_manager;
        }
        assert(mng != NULL); // attach was perfromed
        return mng;
    }

    void attach() {
        task::set_task_specific(this);
    }

    void detach() {
        task::set_task_specific(NULL);
    }

    transaction_manager();
    virtual ~transaction_manager();

    virtual void end_transaction();

    void clear_num_bytes_in_out() { num_bytes_in = 0; num_bytes_out = 0; }
    nat8 get_num_bytes_in() { return num_bytes_in; }
    nat8 get_num_bytes_out()   { return num_bytes_out;   }

    static transaction_isolation_level isolation_level;
    static transaction_manager default_manager;
};

class GOODS_DLL_EXPORT cache_manager {
  public:
    //
    // Lists of cached objects sorted in LRU order
    // Two separate list are used:
    //  one for objects accessed one or zero times
    //  and one for objects accessed more than once(with 'useful' bit in state)
    // Objects from first queue are replaced first.
    object_l2_list lru[2];

    //
    // List of pinned objects
    //
    object_l2_list pin_list;

    //
    // Total size of loaded objects.
    // Sizes are calculated separately for objects accessed 0 or 1 times and
    // for objects acccessed more than once.
    //
    size_t         used_cache_size[2];
    //
    // Limit for total size of objects in cache
    // Limits are specified separately for objects accessed 0 or 1 times and
    // for objects acccessed more than once.
    //
    size_t         cache_size_limit[2];

    //
    // Remove all objects from specified storage from the cache
    //
    void cleanup_cache(obj_storage* storage);

	//
	// Invalidate all cached objects
	//
	void invalidate_cache();

    static cache_manager instance;

    cache_manager();
    virtual ~cache_manager();
};

//
// Metaobject supporting basic transaction and cache management functionality
//

class GOODS_DLL_EXPORT basic_metaobject : public metaobject {
  protected:
    static monitor_turnstile turnstile;
    static mutex transaction_mutex;

    struct notify_item {
        notify_item* chain;
        hnd_t        hnd;
        event&       e;
        notify_item(hnd_t id, event& evt, notify_item* list)
        : chain(list), hnd(id), e(evt) {}
    };
    static notify_item* notification_hash[];

    //
    // This virtual methods can be used for metaobject specific
    // transaction object lists. Method 'commit_transaction'
    // tries to commit transactions in all databases.
    // Methos "abort_transaction" abort transaction in concrete database.
    //
	virtual void commit_transaction(const database* dbs, transaction_manager* mng);
	virtual void abort_transaction(const database* dbs, transaction_manager* mng, abort_reason reason);

    virtual void insert_in_transaction_list(hnd_t hnd,
                                            hnd_t parent_hnd = 0);
    virtual void remove_from_transaction_list(hnd_t hnd);

    virtual void reload_object(hnd_t hnd);
    virtual void update_object(hnd_t hnd);

	virtual void end_write_transient(hnd_t hnd);

  public:
#ifdef MURSIW_SUPPORT
    virtual void release(hnd_t hnd);
#endif

    virtual void destroy(hnd_t hnd);

    virtual void lock(hnd_t hnd, lck_t lck);
    virtual void unlock(hnd_t hnd);

    virtual void wait(hnd_t hnd);
    virtual boolean wait(hnd_t hnd, time_t timeout);
    virtual void notify(hnd_t hnd);

	virtual void make_persistent(hnd_t hnd, hnd_t parent_hnd);
	virtual void make_persistent(hnd_t hnd, obj_storage* storage);

    virtual void invalidate(hnd_t hnd);

    virtual void signal_on_modification(hnd_t hnd, event& e);
    virtual void remove_signal_handler(hnd_t hnd, event& e);

    virtual void put_in_cache(hnd_t hnd);
    virtual void get_from_cache(hnd_t hnd);

    virtual void pin_object(hnd_t hnd);
    virtual void unpin_object(hnd_t hnd);

	virtual void commit_transaction(const database* dbs);
    virtual void abort_transaction(const database* dbs, abort_reason reason);

    virtual void set_cache_limit(size_t limit0, size_t limit1);
    virtual void set_abort_transaction_hook(abort_hook_t hook);

	virtual boolean is_transient();

    virtual ~basic_metaobject() {};
};


//
// Optimistic approach: objects are not locked before access, instead
// of this modified object instances are checked at server while transaction
// commit
//
class GOODS_DLL_EXPORT optimistic_metaobject : public basic_metaobject {
  public:
    virtual void begin_read(hnd_t hnd, boolean for_update);
    virtual void end_read(hnd_t hnd);

    virtual void begin_write(hnd_t hnd);
    virtual void end_write(hnd_t hnd);

	virtual void begin_transaction();
	virtual void end_transaction(const database* dbs);

    optimistic_metaobject(lck_t intertask_lock = lck_exclusive) {
        intertask_read_lock = intertask_lock;
    }
    virtual ~optimistic_metaobject() {};

  protected:
    virtual void load(hnd_t hnd, lck_t lck, boolean hold);
    virtual int  get_transaction_object_flags(hnd_t hnd);

    virtual void commit_object_changes(hnd_t hnd);
    virtual void abort_object_changes(hnd_t hnd, abort_reason reason);

    virtual void release_transaction_object(hnd_t hnd);

    lck_t intertask_read_lock;
};

GOODS_DLL_EXPORT extern optimistic_metaobject
    optimistic_scheme;

GOODS_DLL_EXPORT extern optimistic_metaobject
    optimistic_concurrent_read_scheme;

//
// Optimistic approach providing repatable read:
// While transaction commit server checks uptodate status not only of
// modified objects but of all objects accessed within transaction
//
class GOODS_DLL_EXPORT optimistic_repeatable_read_metaobject :
 public optimistic_metaobject {
  public:
    virtual void begin_read(hnd_t hnd, boolean for_update);

    optimistic_repeatable_read_metaobject(lck_t intertask_lock = lck_exclusive)
        :  optimistic_metaobject(intertask_lock) {}
    virtual ~optimistic_repeatable_read_metaobject() {};
};

GOODS_DLL_EXPORT extern optimistic_repeatable_read_metaobject
    optimistic_repeatable_read_scheme;

GOODS_DLL_EXPORT extern optimistic_repeatable_read_metaobject
    optimistic_concurrent_repeatable_read_scheme;


//
// Metaobjects providing modification correctness (changes can't be lost)
// Conflicts are also possible in that scheme since readonly accessed
// objects are not locked. Conflict can happen if invalidated object
// is accessed for modification and it is impossible to reload this object
// because there are some active invokations of methods for this object.
//
class GOODS_DLL_EXPORT pessimistic_metaobject : public optimistic_metaobject {
  protected:
    int lck_attr; // attributes of locking (lck_nowait,...)

    virtual boolean storage_lock(hnd_t hnd, lck_t lck);

    //
    // This function is called when modification method was called from
    // readonly method and object found to be deteriorated after locking.
    // So it is impossible to update object instance (as there are active
    // invokations for this object). If this method returns 'True'
    // then version of object in databsae will be overwritten by
    // local object instance after commite of transaction.
    // If method returns 'False' transaction will be aborted.
    //
    virtual boolean solve_write_conflict(hnd_t hnd);

    //
    // This method is called when lock request is failed
    //
    virtual boolean solve_lock_conflict(hnd_t hnd, lck_t lck);

    virtual int     get_transaction_object_flags(hnd_t hnd);

    virtual void    release_transaction_object(hnd_t hnd);

    virtual void    commit_object_changes(hnd_t hnd);
    virtual void    abort_object_changes(hnd_t hnd, abort_reason reason);

  public:
    virtual void    begin_write(hnd_t hnd);
    virtual void    begin_read(hnd_t hnd, boolean for_update);
   
    pessimistic_metaobject(int attr = 0, lck_t intertask_read_lock_mode = lck_exclusive) 
        : optimistic_metaobject(intertask_read_lock_mode)
    {
        lck_attr = attr;
    }
    virtual ~pessimistic_metaobject() {};

    //
    // This flag controls whether read-lock -> write-lock promotions will
    // cause the database client to crash.  This is valuable for detecting code
    // that will infrequently cause write conflicts.
    //
    // The default is "false" - roll the dice and hope the timing isn't right
    // to cause write conflicts.
    //
    static boolean abort_on_read_lock_promotion;
};

GOODS_DLL_EXPORT extern pessimistic_metaobject
    pessimistic_scheme;

GOODS_DLL_EXPORT extern pessimistic_metaobject
    pessimistic_concurrent_read_scheme;

GOODS_DLL_EXPORT extern pessimistic_metaobject
    nowait_pessimistic_scheme;


//
// Lazy pessimistic metaobject. This metaobject combines
// characterstics of both optimistic and pessimistic protocols.
// Lock is set after object has been modified.
//

class GOODS_DLL_EXPORT lazy_pessimistic_metaobject :
 public pessimistic_metaobject {
  protected:
    virtual boolean solve_write_conflict(hnd_t hnd);

  public:
    virtual void begin_write(hnd_t hnd);
    virtual void end_write(hnd_t hnd);

    lazy_pessimistic_metaobject(int attr = 0, lck_t intertask_read_lock_mode = lck_exclusive)
        : pessimistic_metaobject(attr, intertask_read_lock_mode) {}
    virtual ~lazy_pessimistic_metaobject() {};
};

GOODS_DLL_EXPORT extern lazy_pessimistic_metaobject
    lazy_pessimistic_scheme;

GOODS_DLL_EXPORT extern lazy_pessimistic_metaobject
    nowait_lazy_pessimistic_scheme;


//
// Pessimistic metaobject enforing repeatable read policy
//

class GOODS_DLL_EXPORT pessimistic_repeatable_read_metaobject :
 public pessimistic_metaobject {
  protected:
    lck_t read_lock;
  public:
    virtual void begin_read(hnd_t hnd, boolean for_update);

    pessimistic_repeatable_read_metaobject(int attr = 0,
                                           lck_t global_read_lock_mode = lck_shared,
                                           lck_t intertask_read_lock_mode = lck_exclusive)
        : pessimistic_metaobject(attr, intertask_read_lock_mode) { this->read_lock = global_read_lock_mode; }
    virtual ~pessimistic_repeatable_read_metaobject() {};
};

GOODS_DLL_EXPORT extern pessimistic_repeatable_read_metaobject
    pessimistic_repeatable_read_scheme;

GOODS_DLL_EXPORT extern pessimistic_repeatable_read_metaobject
    pessimistic_exclusive_scheme;

GOODS_DLL_EXPORT extern pessimistic_repeatable_read_metaobject
    nowait_pessimistic_repeatable_read_scheme;

GOODS_DLL_EXPORT extern pessimistic_repeatable_read_metaobject
    nowait_pessimistic_exclusive_scheme;

GOODS_DLL_EXPORT extern pessimistic_repeatable_read_metaobject
    pessimistic_concurrent_repeatable_read_scheme;

//
// Pessimistic metaobject for child objects accessed always through parent
// object. So locking parent object effectivly syncronize access to child
// and no explicit child locking is required (parent and child
// should be located in the same storage, otherwise we can access
// deteriorated instance of child object).
//
class GOODS_DLL_EXPORT hierarchical_access_metaobject :
 public optimistic_metaobject {
  public:
    int  get_transaction_object_flags(hnd_t);

    virtual ~hierarchical_access_metaobject() {};
};

GOODS_DLL_EXPORT extern hierarchical_access_metaobject
    hierarchical_access_scheme;

//
// Transient metaobject allows you to define a subclass of a persistent-
// capable base class without having instances of the subclass participate
// in any database transactions.  By definition, the transient metaobject
// is completely incapable of affecting the database; as such, it cannot
// be used to persist objects in the database.
//
// This metaobject is useful when you have developed a really cool base
// class that is persistent-capable, you want to re-use functionality in
// that base class, and you don't want the existence or activation of the
// transient instance of your defined subclass to prevent database
// transactions from completing.
//
class GOODS_DLL_EXPORT transient_metaobject : public metaobject {
  public:
    transient_metaobject();
    virtual ~transient_metaobject() {};

	virtual void make_persistent(hnd_t hnd, hnd_t parent_hnd);
	virtual void make_persistent(hnd_t hnd, obj_storage* storage);
    virtual void destroy(hnd_t hnd);
    virtual void lock(hnd_t hnd, lck_t lck);
    virtual void unlock(hnd_t hnd);
    virtual void wait(hnd_t hnd);
    virtual boolean wait(hnd_t hnd, time_t timeout);
    virtual void notify(hnd_t hnd);
    virtual void begin_read(hnd_t hnd, boolean for_update);
    virtual void end_read(hnd_t hnd);
    virtual void begin_write(hnd_t hnd);
    virtual void end_write(hnd_t hnd);
    virtual void invalidate(hnd_t hnd);
    virtual void signal_on_modification(hnd_t hnd, event& e);
    virtual void remove_signal_handler(hnd_t hnd, event& e);
    virtual void put_in_cache(hnd_t hnd);
    virtual void get_from_cache(hnd_t hnd);
    virtual void pin_object(hnd_t hnd);
    virtual void unpin_object(hnd_t hnd);
	virtual void begin_transaction();
	virtual void end_transaction(const database* dbs);
	virtual void commit_transaction(const database* dbs);
    virtual void abort_transaction(const database* dbs,
                                   abort_reason reason = aborted_by_user);
    virtual void commit_object_changes(hnd_t hnd);
    virtual void abort_object_changes(hnd_t hnd, abort_reason reason);
    virtual void release_transaction_object(hnd_t hnd);
    virtual int get_transaction_object_flags(hnd_t hnd);

    virtual void set_cache_limit(size_t limit0, size_t limit1);
    virtual void set_abort_transaction_hook(abort_hook_t hook);

	virtual boolean is_transient();

  protected:
    virtual void insert_in_transaction_list(hnd_t hnd,
                                            hnd_t parent_hnd = 0);
    virtual void remove_from_transaction_list(hnd_t hnd);
};

GOODS_DLL_EXPORT extern transient_metaobject
    transient_scheme;

END_GOODS_NAMESPACE

#endif
