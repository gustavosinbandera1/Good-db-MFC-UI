// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< MOP.CXX >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     20-Feb-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 17-Nov-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Metaobjects are used to specify synchronization policy for GOODS objects
//-------------------------------------------------------------------*--------*


#include "goods.h"

BEGIN_GOODS_NAMESPACE

//
// Object monitor implementation used by basic_metaobject
//

mutex* object_monitor::global_cs = NEW mutex;

void object_monitor::cleanup_global_cs(void)
{
    delete global_cs;
}


#ifdef GOODS_SUPPORT_EXCEPTIONS
dbException::dbException()
{
    transaction_manager* mgr = transaction_manager::get();
    mgr->exception_count += 1;
}


dbException::~dbException()
{
    transaction_manager* mgr = transaction_manager::get();
    mgr->exception_count -= 1;
}
#endif


const size_t TASK_HASH_TABLE_SIZE = 1013;

class task_lock_manager
{
#ifdef DETECT_DEADLOCKS
  private:
    struct task_lock { 
        task* pretender;
        object_monitor* monitor;
        task_lock* next;
    };
    
    task_lock* free_chain;
    task_lock* hash_table[TASK_HASH_TABLE_SIZE];

    object_monitor* get_waiting_for(task* t) { 
        size_t h = size_t(t) % TASK_HASH_TABLE_SIZE;
        for (task_lock* tl = hash_table[h]; tl != NULL; tl = tl->next) { 
            if (tl->pretender == t) {
                return tl->monitor;
            }
        }
        return NULL;
    }

  public:
    void wait(task* t, object_monitor* m) { 
        object_monitor* monitor = m;    
        task* monitor_owner;
        while ((monitor_owner = monitor->owner) != NULL) { 
            if (monitor_owner == t) { 
                throw DeadlockException();
            } 
            monitor = get_waiting_for(monitor_owner);
            if (monitor == NULL || !monitor->waiting) { 
                break;
            }
        }
        task_lock* tl = free_chain;
        if (tl == NULL) { 
            tl = NEW task_lock();
        } else { 
            free_chain = tl->next;
        }
        size_t h = size_t(t) % TASK_HASH_TABLE_SIZE;
        tl->next = hash_table[h];
        hash_table[h] = tl;
        tl->pretender = t;
        tl->monitor = m;
    }

    void grant(task* t) { 
        size_t h = size_t(t) % TASK_HASH_TABLE_SIZE;
        task_lock *tl, **tpp;
        for (tpp = &hash_table[h]; (tl = *tpp) != NULL; tpp = &tl->next) { 
            if (tl->pretender == t) { 
                *tpp = tl->next;
                tl->next = free_chain;
                free_chain = tl;
                break;
            }
        }
    }
    
    task_lock_manager() { 
        free_chain = NULL;
        memset(hash_table, 0, sizeof(hash_table));
    }
    ~task_lock_manager() { 
        task_lock * tl, *next;
        for (tl = free_chain; tl != NULL; tl = next) { 
            next = tl->next;
            delete tl;
        }
    }
#else
  public:
    void wait(task* t, object_monitor* m) {}
    void grant(task*) {}
#endif
};

static task_lock_manager lock_manager;

#ifdef MURSIW_SUPPORT
//[MC] for multi-lock implementation
inline boolean object_monitor::is_locked_by_other()
{
	task *self = task::current();
	return n_readers + n_writers + n_blocked != 0 && self != owner;
}

inline boolean object_monitor::is_locked()
{
	return n_readers + n_writers + n_blocked != 0;
}

inline boolean object_monitor::is_busy()
{
	return n_readers + n_writers + n_blocked != 0;
}

inline void object_monitor::attach(hnd_t new_owner)
{
	internal_assert(!is_busy() && owner == NULL);
	if (hnd != 0 && IS_VALID_OBJECT(hnd->obj)) {
		hnd->obj->monitor = 0;
	}
	hnd = new_owner;
	if (evt == NULL) {
		evt = NEW eventex(*global_cs);
	}
}

inline void object_monitor::detach()
{
	internal_assert(!is_busy() && owner == NULL);
	hnd = 0;
}

inline void object_monitor::wait(task* t)
{
    waiting = true;
    lock_manager.wait(t, this);
    n_blocked += 1;
    evt->reset();
    evt->wait();
    n_blocked -= 1;
    lock_manager.grant(t);
}

inline void object_monitor::lock(lck_t lck)
{
    task* self = task::current();
    if (lck == lck_exclusive) {
        if (self != owner) {
            while (n_writers + n_readers != 0) {
                wait(self);
            }
            owner = self;
        } else {
            n_writers += n_readers;
            n_readers = 0;
        }
        n_writers += 1;
    } else {
        if (self != owner) {
            while (n_writers != 0) {
                wait(self);
            }
            if (n_readers++ == 0) {
                owner = self;
                if (lck == lck_temporary_exclusive) {
                    n_writers = 1;
                }
            } else {
                owner = NULL;
            }
        } else { 
            if (n_writers != 0) { 
                n_writers += 1;
            } else { 
                n_readers += 1;
            }
        }
    }
}


inline void object_monitor::release()
{
    if (n_writers == 1 && n_readers != 0) {
        n_writers = 0;
        if (waiting) {
            waiting = false;
            evt->signal();
        }
    }
}

inline void object_monitor::unlock()
{
    if (n_writers != 0) {
        internal_assert(owner != NULL/* && n_readers == 0*/); // n_readers can be non-zero for temporary exclusive lock
        if (--n_writers == 0) { 
            owner = NULL;
            if (waiting) {
                waiting = false;
                evt->signal();
            }
        }
    } else if (n_readers != 0) {
        if (--n_readers == 0) {
            owner = NULL;
            if (waiting) {
                waiting = false;
                evt->signal();
            }
        }
    }
}

inline object_monitor::object_monitor()
{
    n_readers = 0;
    n_writers = 0;
    n_blocked = 0;
    waiting = false;
    evt = NULL;
    hnd = 0;
    owner = NULL;
}

inline object_monitor::~object_monitor()
{
    delete evt;
}

inline void object_monitor::wait()
{
    assert(false);
}

inline boolean object_monitor::wait(time_t timeout)
{
    return false;
}

inline void object_monitor::notify()
{
    assert(false);
}

#else

inline boolean object_monitor::is_locked()
{
    return busy != 0;
}

inline boolean object_monitor::is_busy()
{
    return busy || signaled;
}

inline void object_monitor::attach(hnd_t new_owner)
{
    internal_assert(!busy && !signaled && owner == NULL && n_nested_locks==0);
    if (hnd != 0 && IS_VALID_OBJECT(hnd->obj)) {
        hnd->obj->monitor = 0;
    }
    hnd = new_owner;
}

inline void object_monitor::detach()
{
    internal_assert(busy == 0 && owner == NULL && n_nested_locks==0);
    hnd = 0;
    if (signaled) {
        signaled = 0;
        delete sem;
        sem = NULL;
    }
}

//
// 'lock' and 'unlock' are executed with global mutex locked.
// To avoid deadlock first release this lock when object is locked
// by other user.
//
inline void object_monitor::lock(lck_t)
{
    task* self = task::current();
    if (busy++ == 0) {
        cs->enter(); // should not wait
    } else if (owner != self) { 
		lock_manager.wait(self, this);
        unlock_global(); 
        cs->enter();
        lock_global();
        assert(owner == NULL);
        lock_manager.grant(self);
    }
    owner = self;
    n_nested_locks += 1;
}

inline void object_monitor::unlock()
{
    busy -= 1;
    if (--n_nested_locks == 0) {
        owner = NULL;
        cs->leave();
    }
}


inline void object_monitor::wait()
{
    if (sem == NULL) {
        sem = NEW semaphorex(*cs);
    }
    task* self = owner;
    int n_locks = n_nested_locks;
    n_nested_locks = 0;
    owner = NULL;
    unlock_global();
    sem->wait();
    lock_global();
    internal_assert(signaled > 0);
    owner = self;
    n_nested_locks = n_locks;
    signaled -= 1;
}

inline boolean object_monitor::wait(time_t timeout)
{
    if (sem == NULL) {
        sem = NEW semaphorex(*cs);
    }
    task* self = owner;
    int n_locks = n_nested_locks;
    n_nested_locks = 0;
    owner = NULL;
    unlock_global();
    if (sem->wait_with_timeout(timeout)) {
        lock_global();
        internal_assert(signaled > 0);
        owner = self;
        n_nested_locks = n_locks;
        signaled -= 1;
        return True;
    }
    lock_global();
    owner = self;
    n_nested_locks = n_locks;
    return False;
}

inline void object_monitor::notify()
{
    signaled += 1;
    if (sem == NULL) {
        sem = NEW semaphorex(*cs);
    }
    sem->signal();
}

inline object_monitor::object_monitor()
{
    busy = 0;
    signaled = 0;
    sem = NULL; // lazy semaphore creation
    hnd = 0;
    n_nested_locks = 0;
    cs = NEW mutex();
    owner = NULL;
}

inline object_monitor::~object_monitor()
{
    delete sem;
    delete cs;
}

#endif

//
// Turnstile of object monitors used by basic_metaobject
//

int monitor_turnstile::attach_monitor(hnd_t hnd)
{
    object_monitor* mp;
    int n = size;
    int i;
    for (i = 0; i < n; i++) {
        int j = (i + pos) % n;
        mp = turnstile[j];
        if (mp == NULL || !mp->is_busy()) {
            if (mp == NULL) {
                turnstile[j] = mp = NEW object_monitor();
            }
            mp->attach(hnd);
            return pos = j+1;
        }
    }
    object_monitor** new_turnstile = NEW object_monitor*[n * 2];
    memcpy(new_turnstile, turnstile, n*sizeof(object_monitor*));
    pos = n;
    size = n *= 2;
    for (i = pos; i < n; i++) {
        new_turnstile[i] = NULL;
    }
    delete[] turnstile;
    new_turnstile[pos] = mp = NEW object_monitor();
    turnstile = new_turnstile;
    mp->attach(hnd);
    return ++pos;
}

inline boolean monitor_turnstile::is_locked(int monitor)
{
    return monitor > 0 && turnstile[monitor-1]->is_locked();
}

inline void monitor_turnstile::lock(int monitor, lck_t lck)
{
    turnstile[monitor-1]->lock(lck);
}

inline void monitor_turnstile::unlock(int monitor)
{
    turnstile[monitor-1]->unlock();
}

#ifdef MURSIW_SUPPORT
//[MC] for multi-lock implementation
boolean monitor_turnstile::is_locked_by_other(int monitor)
{
    return monitor > 0 && turnstile[monitor-1]->is_locked_by_other();
}

inline void monitor_turnstile::release(int monitor)
{
    turnstile[monitor-1]->release();
}
#endif


inline void monitor_turnstile::wait(int monitor)
{
    turnstile[monitor-1]->wait();
}

inline boolean monitor_turnstile::wait(int monitor, time_t timeout)
{
    return turnstile[monitor-1]->wait(timeout);
}

inline void monitor_turnstile::notify(int monitor)
{
    turnstile[monitor-1]->notify();
}

inline void monitor_turnstile::detach_monitor(int monitor)
{
    turnstile[monitor-1]->detach();
}

monitor_turnstile::monitor_turnstile(int init_size)
{
    size = init_size;
    pos = 0;
    turnstile = NEW object_monitor*[init_size];
    for (int i = 0; i < init_size; i++) {
        turnstile[i] = NULL;
    }
}

monitor_turnstile::~monitor_turnstile() {
    for (int i = 0; i < size; i++) {
        delete turnstile[i];
    }
    delete[] turnstile;
}

//
// Cache manager
//

#define DEFAULT_CACHE_LIMIT     (1024*1024)

cache_manager cache_manager::instance;
transaction_manager transaction_manager::default_manager;
transaction_isolation_level transaction_manager::isolation_level = PER_PROCESS_TRANSACTION;

static void default_abort_hook(metaobject*)
{
    console::error("Transaction is aborted by server\n");
}

void cache_manager::invalidate_cache()
{
	hnd_t hnd, next;
	object_monitor::lock_global();
	for (int i = 0; i < 2; i++) {
		for (hnd = lru[i].head; hnd != NULL; hnd = next) {
			object_header* obj = metaobject::get_header(hnd);
			next = obj->next;
			if (obj->state & object_header::persistent) {
				hnd->obj->mop->invalidate(hnd);
			}
		}
	}
	object_monitor::unlock_global();
}

void cache_manager::cleanup_cache(obj_storage* storage)
{
    hnd_t hnd, next;
    for (int i = 0; i < 2; i++) {
        for (hnd = lru[i].head; hnd != NULL; hnd = next) {
            object_header* obj = metaobject::get_header(hnd);
            next = obj->next;
            if (hnd->storage == storage) {
                hnd->access = 0;
                obj->remove_object();
            }
        }
    }
    for (hnd = pin_list.head; hnd != NULL; hnd = next) {
        object_header* obj = metaobject::get_header(hnd);
        next = obj->next;
        if (hnd->storage == storage) {
            hnd->access = 0;
            obj->remove_object();
        }
    }
}

cache_manager::cache_manager()
{
    cache_size_limit[0] = cache_size_limit[1] = DEFAULT_CACHE_LIMIT;
    used_cache_size[0] = used_cache_size[1] = 0;
}

void transaction_manager::set_isolation_level(transaction_isolation_level level)
{
    isolation_level = level;
}

transaction_manager::transaction_manager()
{
    trans_commit_wait_flag = False;
	trans_commit_in_progress = False;
	force_commit = False;
    abort_hook = default_manager.abort_hook == NULL ? default_abort_hook : default_manager.abort_hook;
    n_nested_transactions = 0;
    exception_count = 0;
    num_bytes_in = 0;
	num_bytes_out = 0;
	extension = NULL;
}

transaction_manager::~transaction_manager()
{
	delete extension;
}

void transaction_manager::end_transaction() { }

cache_manager::~cache_manager() { }

//
// Basic metaobject methods
//

#define NOTIFICATION_HASH_SIZE  113

monitor_turnstile basic_metaobject::turnstile;
mutex basic_metaobject::transaction_mutex;
basic_metaobject::notify_item* basic_metaobject::notification_hash[NOTIFICATION_HASH_SIZE];

void metaobject::end_write_transient(hnd_t hnd)
{
    return end_read(hnd);
}

void basic_metaobject::end_write_transient(hnd_t hnd)
{
#if DEBUG_LEVEL >= DEBUG_TRACE
    object_header* obj = get_header(hnd);
    obj->crc = calculate_crc(obj+1, hnd->obj->size - sizeof(object_header));
    obj->state |= object_header::verified;
#endif    
    return end_read(hnd);
}

void basic_metaobject::destroy(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    if ((obj->state & (object_header::persistent|object_header::fixed)) ==
        object_header::persistent)
    {
        get_from_cache(hnd);
    }
    if ((obj->state & object_header::pinned) != 0) {
        unpin_object(hnd);
    }
    if (obj->monitor != 0) {
        turnstile.detach_monitor(obj->monitor);
        obj->monitor = 0;
    }
}

void basic_metaobject::lock(hnd_t hnd, lck_t lck)
{
    object_header* obj = get_header(hnd);
    if (obj->monitor == 0) {
        obj->monitor = turnstile.attach_monitor(hnd);
    }
    turnstile.lock(obj->monitor, lck);
}

void basic_metaobject::unlock(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    internal_assert(obj->monitor != 0);
    turnstile.unlock(obj->monitor);
}

#ifdef MURSIW_SUPPORT
void basic_metaobject::release(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    internal_assert(obj->monitor != 0);
    turnstile.release(obj->monitor);
}
#endif

boolean basic_metaobject::is_transient()
{
	return False;
}

void basic_metaobject::wait(hnd_t hnd)
{
    assert(transaction_manager::isolation_level == PER_PROCESS_TRANSACTION);
    object_header* obj = get_header(hnd);
    internal_assert(obj->monitor != 0);
    turnstile.wait(obj->monitor);
}

boolean basic_metaobject::wait(hnd_t hnd, time_t timeout)
{
    assert(transaction_manager::isolation_level == PER_PROCESS_TRANSACTION);
    object_header* obj = get_header(hnd);
    internal_assert(obj->monitor != 0);
    return turnstile.wait(obj->monitor, timeout);
}

void basic_metaobject::notify(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    if (obj->monitor == 0) {
        obj->monitor = turnstile.attach_monitor(hnd);
    }
    turnstile.notify(obj->monitor);
}



void basic_metaobject::make_persistent(hnd_t hnd, hnd_t parent_hnd)
{
    obj_storage* storage = parent_hnd->storage;
    object_header* obj = get_header(hnd);
    if (hnd->storage != NULL) {
        assert(hnd->storage->db == storage->db);
        storage = hnd->storage;
    } else {
        hnd->storage = storage;
    }
    class_descriptor* desc = &hnd->obj->cls;
    obj->cpid = storage->get_cpid_by_descriptor(desc);

	objref_t opid;
    int flags = aof_none;
    if ((desc->class_attr & class_descriptor::cls_aligned) || (obj->state & object_header::clustered)) {
        flags |= aof_aligned;
    }
    if (obj->cluster != 0) {
        flags |= aof_clustered;
        if (obj->cluster->opid == 0) {
            make_persistent(obj->cluster, parent_hnd);
            assert(obj->cluster->opid != 0);
        }
    }
    opid = storage->allocate(obj->cpid, desc->packed_size((char*)obj, hnd->obj->size), flags, obj->cluster);

    object_handle::assign_persistent_oid(hnd, opid);

	assert(!(obj->state & object_header::modified));
	obj->state |= object_header::modified | object_header::created;
    insert_in_transaction_list(hnd, parent_hnd);
}

void basic_metaobject::make_persistent(hnd_t hnd, obj_storage* storage)
{
	object_header* obj = get_header(hnd);
	if (hnd->storage != NULL) {
		assert(hnd->storage->db == storage->db);
		storage = hnd->storage;
	}
	else {
		hnd->storage = storage;
	}
	class_descriptor* desc = &hnd->obj->cls;
	obj->cpid = storage->get_cpid_by_descriptor(desc);

	objref_t opid;
	int flags = aof_none;
	if ((desc->class_attr & class_descriptor::cls_aligned) || (obj->state & object_header::clustered)) {
		flags |= aof_aligned;
	}
	if (obj->cluster != 0) {
		flags |= aof_clustered;
		if (obj->cluster->opid == 0) {
			make_persistent(obj->cluster, storage);
			assert(obj->cluster->opid != 0);
		}
	}
	opid = storage->allocate(obj->cpid, desc->packed_size((char*)obj, hnd->obj->size), flags, obj->cluster);

	object_handle::assign_persistent_oid(hnd, opid);

	assert(!(obj->state & object_header::modified));
	obj->state |= object_header::modified | object_header::created;
	insert_in_transaction_list(hnd, NULL);
}

void basic_metaobject::pin_object(hnd_t hnd)
{
    object_header* obj = get_header(hnd);

	//[MC] Avoid setting an object as pinned if it is already in another cache list
	if(obj->next || obj->prev)
		return;

    obj->state |= object_header::fixed|object_header::pinned;
    cache_manager::instance.pin_list.put_at_head(obj);
}

void basic_metaobject::unpin_object(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    cache_manager::instance.pin_list.unlink(obj);
}

void basic_metaobject::put_in_cache(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    cache_manager* mng = &cache_manager::instance;
    int i = (obj->state & object_header::useful) ? 1 : 0;

	mng->used_cache_size[i] += hnd->obj->cls.packed_size((char*)obj, hnd->obj->size);

    obj->state &= ~object_header::fixed;
    if (!(obj->state & object_header::notifier)) {
        mng->lru[i].put_at_head(obj);
    }
    while (mng->used_cache_size[i] > mng->cache_size_limit[i]) {
        hnd_t victim = mng->lru[i].tail;
        if (victim != 0 && victim != hnd) {
            get_header(victim)->remove_object();
        } else {
            break;
        }
    }
}

void basic_metaobject::get_from_cache(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    cache_manager* mng = &cache_manager::instance;
	int i = (obj->state & object_header::useful) ? 1 : 0;
	mng->used_cache_size[i] -= hnd->obj->cls.packed_size((char*)obj, hnd->obj->size);
    if ((obj->state & object_header::accessed) && mng->lru[i].head != hnd) {
        obj->state |= object_header::useful;
    }
    obj->state |= object_header::fixed|object_header::accessed;
    if (!(obj->state & object_header::notifier)) {

#if 0 // Enable this code if you want to find out where your mis-use of    // {
      // per-thread transactions is corrupting a GOODS object cache.  If
      // a thread in your code creates a reference while attached to one
      // connection and then destroys that reference while attached to
      // another connection, you'll corrupt the cache (if the object is
      // the first or last item in the cache's object list.  Please note
      // that this code affects performance severely; you should only
      // enable it for debugging builds.
        assert(mng->lru[i].find(obj) >= 0);
#endif                                                                     // }

        mng->lru[i].unlink(obj);
    }
}

void basic_metaobject::insert_in_transaction_list(hnd_t hnd, hnd_t parent_hnd)
{
    transaction_manager* mng = transaction_manager::get();
    object_header* obj = get_header(hnd);
    assert((obj->state & object_header::in_trans) == 0);
    if ((obj->state & object_header::pinned) != 0) {
        unpin_object(hnd);
    }
    //
    // Prevent object involved in transaction from GC until end of transaction
    //
    hnd->access += 1;
    //
    // Group all modified objects together
    //
    if (parent_hnd) {
        mng->trans_objects.put_after(get_header(parent_hnd), obj);
    } else {
        if (obj->state & object_header::modified) {
            mng->trans_objects.put_at_head(obj); // modified objects first
        } else {
            mng->trans_objects.put_at_tail(obj);
        }
    }
    obj->state |= object_header::in_trans;
    if (transaction_manager::isolation_level != PER_PROCESS_TRANSACTION) {
        if (obj->monitor == 0) {
            obj->monitor = turnstile.attach_monitor(hnd);
        }
        turnstile.lock(obj->monitor, (obj->state & object_header::modified) ? lck_exclusive : lck_shared);
    }
}

void basic_metaobject::remove_from_transaction_list(hnd_t hnd)
{
    transaction_manager* mng = transaction_manager::get();
    object_header* obj = get_header(hnd);
    mng->trans_objects.unlink(obj);
    obj->state &= ~object_header::in_trans;
    hnd->access -= 1;
    if (transaction_manager::isolation_level != PER_PROCESS_TRANSACTION) {
        internal_assert(obj->monitor != 0);
        turnstile.unlock(obj->monitor);
    }
}

void basic_metaobject::update_object(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    internal_assert(!(obj->state & object_header::in_trans));
    if (obj->state & object_header::notifier) {
        notify_item *np, **npp;
        npp = &notification_hash[size_t(hnd) % itemsof(notification_hash)];
        while ((np = *npp) != NULL && np->hnd != hnd) {
            npp = &np->chain;
        }
        assert(np != NULL);
        np->e.signal();
        *npp = np->chain;
        delete np;
        obj->state = (obj->state & ~object_header::notifier) | object_header::fixed;
    }
    obj->remove_object();
}

void basic_metaobject::reload_object(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    internal_assert(obj->state & object_header::fixed);
    obj->state |= object_header::reloading;
    obj->state &= ~object_header::invalidated;
    hnd->storage->load(hnd, lof_none);
    obj = get_header(hnd);
    object_monitor::unlock_global();
    obj->on_loading();
    object_monitor::lock_global();
#if DEBUG_LEVEL >= DEBUG_TRACE
    obj->crc = calculate_crc(obj+1, hnd->obj->size - sizeof(object_header));
    obj->state |= object_header::verified;
#endif
}



void basic_metaobject::invalidate(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    /*
    internal_assert(transaction_manager::get()->trans_commit_in_progress ||
        !(obj->state & (object_header::exl_locked|object_header::shr_locked)));
    */
    /*
    if (!(obj->state & (object_header::in_trans|object_header::fixed))
        && !turnstile.is_locked(obj->monitor))
    */
    if (!(obj->state & object_header::in_trans)
        && obj->n_invokations == 0
        && !turnstile.is_locked(obj->monitor))
    {
        update_object(hnd);
    } else {
        obj->state |= object_header::invalidated;
    }
}

void basic_metaobject::commit_transaction(const database* dbs)
{
	commit_transaction(dbs, transaction_manager::get());
}


void basic_metaobject::abort_transaction(const database* dbs,
                                         abort_reason reason)
{
    abort_transaction(dbs, transaction_manager::get(), reason);
}

void basic_metaobject::commit_transaction(const database* dbs, transaction_manager* mng) {
    object_l2_list& trans_objects = mng->trans_objects;
	mng->trans_commit_in_progress = True;
	boolean has_something_to_commit = False;

    object_monitor::unlock_global();
    transaction_mutex.enter();
    object_monitor::lock_global();

  UpdatedObjectLoop:
    while (!trans_objects.empty()) {
        hnd_t hnd = trans_objects.head;

        if (!(get_header(hnd)->state & object_header::modified)) {
            //
            // No more modified objects (because modified objects
            // are always placed at the end of transaction list)
            //
            do {
                object_header* obj = get_header(hnd);
                hnd_t next = obj->next;
                internal_assert(!(obj->state & object_header::modified));
                hnd->obj->mop->release_transaction_object(hnd);
                hnd = next;
            } while (hnd != 0);
            break;
        }

        obj_storage*    coordinator = NULL;
        int             n_trans_servers = 0;
        database const* dbs = hnd->storage->db;
        dnm_array<obj_storage*> trans_servers;

        do {
            object_header* obj = get_header(hnd);
            if (hnd->storage->db == dbs) {
                if (obj->state & object_header::invalidated) {
                    abort_transaction(dbs, mng, aborted_by_server);
                    goto UpdatedObjectLoop;
                }
                stid_t sid = hnd->storage->id;
                if (trans_servers[sid] == NULL) {
                    hnd->storage->begin_transaction();
                    n_trans_servers += 1;
                    trans_servers[sid] = hnd->storage;
                }

                int flags = hnd->obj->mop->get_transaction_object_flags(hnd);

                if (obj->state & object_header::modified) {
                    flags |= tof_update;
                }
				if (obj->state & object_header::created) {
					flags |= tof_new;
				}
#if DEBUG_LEVEL >= DEBUG_TRACE
                if (obj->state & object_header::verified) {
                    assert(obj->crc == calculate_crc(obj+1, hnd->obj->size - sizeof(object_header)));
                }
#endif
                hnd->storage->include_object_in_transaction(hnd, flags);
                if (coordinator == NULL || coordinator->id > sid) {
                    coordinator = hnd->storage;
                }
            }
            hnd = obj->next;
        } while (hnd != 0);

        if (n_trans_servers != 0) {
            assert(n_trans_servers < MAX_TRANS_SERVERS);
            stid_t trans_sids[MAX_TRANS_SERVERS];
            obj_storage** server = &trans_servers;
            trid_t tid;
            int i, j, n = (int)trans_servers.size();

			has_something_to_commit = True;

            for (i = 0, j = 0; i < n; i++) {
                if (server[i] != NULL) {
                    trans_sids[j++] = server[i]->id;
                }
            }
            if (!coordinator->commit_coordinator_transaction(n_trans_servers,
                                                             trans_sids,
                                                             tid))
            {
                abort_transaction(dbs, mng, aborted_by_server);
                continue;
            } else { // transaction was successefully completed
                if (n_trans_servers > 1) { // global transaction
                    for (i = 0; i < n; i++) {
                        if (server[i] != NULL && server[i] != coordinator)
                        {
                            server[i]->commit_transaction(coordinator->id,
                                                          n_trans_servers,
                                                          trans_sids,
                                                          tid);
                        }
                    }
                    for (i = 0; i < n; i++) {
                        if (server[i] != NULL)
                        {
                            if (!server[i]->wait_global_transaction_completion()) {
                                abort_transaction(dbs, mng, aborted_by_server);
                                goto UpdatedObjectLoop;
                            }
                        }
                    }
                }
            }
            //
            // Transaction was successfully commited
            //
            hnd = trans_objects.head;
            while (hnd != 0) {
                object_header* obj = get_header(hnd);
                hnd_t next = obj->next;
                if (hnd->storage->db == dbs) {
                    hnd->obj->mop->commit_object_changes(hnd);
                }
                hnd = next;
            }
        }
    }
	if (!has_something_to_commit && dbs != NULL) {
		if (mng->force_commit) {
			stid_t sid = 0;
			trid_t tid;
			dbs->get_storage(0)->commit_coordinator_transaction(1, &sid, tid);
		}
		else {
			dbs->get_storage(0)->rollback_transaction();
		}
	}
	mng->trans_commit_in_progress = False;
	mng->force_commit = False;
    if (mng->trans_commit_wait_flag) {
        mng->trans_commit_event.signal();
        mng->trans_commit_wait_flag = False;
    }
    transaction_mutex.leave();
    mng->end_transaction();
}


void basic_metaobject::abort_transaction(const database* dbs, transaction_manager* mng, abort_reason reason)
{
    object_l2_list& trans_objects = mng->trans_objects;
	hnd_t hnd = trans_objects.head;
	dnm_array<obj_storage*> trans_servers;
	int n_trans_servers = 0;

    while (hnd != 0) {
        hnd_t next = get_header(hnd)->next;
        if (hnd->storage->db == dbs || dbs == NULL) {
			stid_t sid = hnd->storage->id;
			if (trans_servers[sid] == NULL) {
				trans_servers[sid] = hnd->storage;
				n_trans_servers += 1;
			}
            hnd->obj->mop->abort_object_changes(hnd, reason);
        }
        hnd = next;
    }
    if (mng->abort_hook != NULL && reason == aborted_by_server) {
        (*mng->abort_hook)(this);
    }
	if (n_trans_servers != 0) {
		size_t i, n = trans_servers.size();
		for (i = 0; i < n; i++) {
			if (trans_servers[i] != NULL) {
				trans_servers[i]->rollback_transaction();
			}
		}
	} else if (dbs != NULL) {
		 dbs->get_storage(0)->rollback_transaction();
	}
}

void basic_metaobject::set_cache_limit(size_t limit0, size_t limit1)
{
    cache_manager::instance.cache_size_limit[0] = limit0;
    cache_manager::instance.cache_size_limit[1] = limit1;
}

void basic_metaobject::set_abort_transaction_hook(abort_hook_t hook)
{
    transaction_manager::get()->abort_hook = hook;
}


void basic_metaobject::signal_on_modification(hnd_t hnd, event& e)
{
#if PGSQL_ORM
	hnd->storage->listen(hnd, e);
#else    
    object_header* obj = get_header(hnd);
    obj->state |= object_header::notifier;
    unsigned h = size_t(hnd) % itemsof(notification_hash);
	notification_hash[h] = NEW notify_item(hnd, e, notification_hash[h]);
#endif
}

void basic_metaobject::remove_signal_handler(hnd_t hnd, event& e)
{
#if PGSQL_ORM
	hnd->storage->unlisten(hnd, e);
#else    
    object_header* obj = get_header(hnd);
    if (obj->state & object_header::notifier) {
        unsigned h = size_t(hnd) % itemsof(notification_hash);
        notify_item *np, **npp;
        for (npp = &notification_hash[h]; (np = *npp) != NULL; npp = &np->chain) {
            if (&np->e == &e) {
                *npp = np->chain;
                delete np;
                if (notification_hash[h] == NULL) {
                    obj->state &= ~object_header::notifier;
                }
                return;
            }
        }
	}
#endif
}

//
// Optimistic metaobject
//

void optimistic_metaobject::load(hnd_t hnd, lck_t lck, boolean hold)
{
    object_header* obj = get_header(hnd);
    transaction_manager* mng = transaction_manager::get();
#if PGSQL_ORM
    if (mng->n_nested_transactions++ == 0 && hnd->storage != NULL) {
		hnd->storage->begin_transaction();
		obj = get_header(hnd);
		if (!IS_VALID_OBJECT(obj)) {
			internal_assert(hnd->storage != NULL); 
			hnd->storage->load(hnd, lof_none);
			obj = get_header(hnd);
			internal_assert(IS_VALID_OBJECT(obj));          
		}
    }
#else
	mng->n_nested_transactions += 1;
#endif
#ifdef GOODS_SUPPORT_EXCEPTIONS
    try {
#endif
    while (mng->trans_commit_in_progress) {
        mng->trans_commit_event.reset();
        mng->trans_commit_wait_flag = True;
        object_monitor::unlock_global();
        mng->trans_commit_event.wait();
        object_monitor::lock_global();
        obj = get_header(hnd);
        if (!IS_VALID_OBJECT(obj)) {
            internal_assert(hnd->storage != NULL);
            hnd->storage->load(hnd, lof_none);
            obj = get_header(hnd);
            internal_assert(IS_VALID_OBJECT(obj));
        }
    }
#ifdef GOODS_SUPPORT_EXCEPTIONS
    } catch(...) {
#if PGSQL_ORM
		if (--mng->n_nested_transactions == 0 && hnd->storage != NULL) {
			hnd->storage->rollback_transaction();
		}
#else
		mng->n_nested_transactions -= 1;
#endif
        throw;
    }
    try {
#endif
    lock(hnd, lck); // intertask locking
    obj = get_header(hnd);
    obj->n_invokations += 1;
    if ((obj->state & (object_header::persistent|object_header::fixed))
        == object_header::persistent)
    {
        get_from_cache(hnd);
    }
    if ((obj->state & (object_header::persistent|object_header::initialized))
        == object_header::persistent)
    {
        obj->state |= object_header::initialized;
        object_monitor::unlock_global();
        obj->on_loading();
        object_monitor::lock_global();
#if DEBUG_LEVEL >= DEBUG_TRACE
        obj->crc = calculate_crc(obj+1, hnd->obj->size - sizeof(object_header));
        obj->state |= object_header::verified;
#endif
    }
#ifdef GOODS_SUPPORT_EXCEPTIONS
    } catch(...) {
#if PGSQL_ORM
		if (--mng->n_nested_transactions == 0 && hnd->storage != NULL) {
			hnd->storage->rollback_transaction();
		}
#else
		mng->n_nested_transactions -= 1;
#endif
        obj->n_invokations -= 1;
        unlock(hnd);
#ifdef MURSIW_SUPPORT
        if (!hold) {
            release(hnd);
        }
#endif
        throw;
    }
#endif
#ifdef MURSIW_SUPPORT
    if (!hold) {
        release(hnd);
    }
#endif
}

void optimistic_metaobject::begin_read(hnd_t hnd, boolean for_update)
{
    load(hnd, for_update ? lck_exclusive : intertask_read_lock, False);
}

void optimistic_metaobject::end_read(hnd_t hnd)
{
    transaction_manager* mng = transaction_manager::get();
    unlock(hnd);
    object_header* obj = get_header(hnd);
    if (--obj->n_invokations == 0) {
        if ((obj->state & (object_header::in_trans|object_header::fixed|object_header::pinned))
            == object_header::fixed)
        {
            put_in_cache(hnd);
        }
        if ((obj->state & (object_header::in_trans|object_header::invalidated))
            == object_header::invalidated && !turnstile.is_locked(obj->monitor))
        {
            update_object(hnd);
        }
    }
    if (--mng->n_nested_transactions == 0) {
		database const* dbs = hnd->storage ? hnd->storage->db : NULL;
#ifdef GOODS_SUPPORT_EXCEPTIONS
        if (mng->exception_count != 0) {
			abort_transaction(dbs, aborted_by_user);
        } 
		else
#endif
			commit_transaction(dbs);
    }
}

void optimistic_metaobject::begin_write(hnd_t hnd)
{
    optimistic_metaobject::load(hnd, lck_exclusive, False);
    object_header* obj = get_header(hnd);
    if ((obj->state & (object_header::persistent|object_header::modified))
         == object_header::persistent)
    {
        obj->state |= object_header::modified;
        if (obj->state & object_header::in_trans) {
            //
            // Relink object in transaction list according to
            // the changed state of object (modified)
            //
            remove_from_transaction_list(hnd);
        }
        insert_in_transaction_list(hnd);
    }
}

void optimistic_metaobject::end_write(hnd_t hnd)
{
#if DEBUG_LEVEL >= DEBUG_TRACE
    object_header* obj = metaobject::get_header(hnd);
    obj->crc = calculate_crc(obj+1, hnd->obj->size - sizeof(object_header));
    obj->state |= object_header::verified;
#endif
    optimistic_metaobject::end_read(hnd);
}

void optimistic_metaobject::begin_transaction()
{
    transaction_manager* mng = transaction_manager::get();
    object_monitor::lock_global();
    mng->n_nested_transactions += 1;
    object_monitor::unlock_global();
}

void optimistic_metaobject::end_transaction(const database* dbs)
{
    transaction_manager* mng = transaction_manager::get();
    object_monitor::lock_global();
    assert(mng->n_nested_transactions > 0);
    if (--mng->n_nested_transactions == 0) {
        commit_transaction(dbs);
    }
    object_monitor::unlock_global();
}

int  optimistic_metaobject::get_transaction_object_flags(hnd_t)
{
    return tof_validate;
}

void optimistic_metaobject::commit_object_changes(hnd_t hnd)
{
    object_header* obj = get_header(hnd);

    remove_from_transaction_list(hnd);
    //
    // Setting of 'persistent', 'fixed', 'initialized' and 'accessed' flags is
    // necessary for transient object included in persistent closure
    //
    obj->state = object_header::persistent | object_header::fixed
		| object_header::initialized | object_header::accessed
		| (obj->state & ~(object_header::modified | object_header::created));

    if ((obj->state & object_header::pinned) == 0) {
        put_in_cache(hnd);
    } else {
        pin_object(hnd);
    }
    if ((obj->state & object_header::invalidated) && !turnstile.is_locked(obj->monitor)) {
        update_object(hnd);
    }
}


void optimistic_metaobject::abort_object_changes(hnd_t hnd, abort_reason)
{
    remove_from_transaction_list(hnd);

    object_header* obj = get_header(hnd);
    if (!(obj->state & object_header::persistent)) {
		obj->state &= ~(object_header::modified | object_header::created);
        object_handle::remove_from_storage(hnd);
    } else {
        if (obj->n_invokations == 0 && (obj->state & object_header::pinned) == 0) {
            put_in_cache(hnd);
        } else if ((obj->state & object_header::pinned) != 0) {
            pin_object(hnd);
        }
        if (obj->state & object_header::modified) {
            obj->state |= object_header::invalidated;
            obj->state &= ~object_header::modified;
		}
		obj->state &= ~object_header::created;
    }
    if (obj->n_invokations == 0
        && ((obj->state & object_header::invalidated)
            || (hnd->access == 0
                && !(obj->state & object_header::persistent))))
    {
        update_object(hnd);
    }
}

void optimistic_metaobject::release_transaction_object(hnd_t hnd)
{
    commit_object_changes(hnd);
}

optimistic_metaobject optimistic_scheme;

optimistic_metaobject optimistic_concurrent_read_scheme(lck_temporary_exclusive);

//
// Optimistic metaobject with repeatable read facility
//

void optimistic_repeatable_read_metaobject::begin_read(hnd_t hnd, boolean for_update)
{
    optimistic_metaobject::begin_read(hnd, for_update);
    object_header* obj = get_header(hnd);
    if ((obj->state & (object_header::persistent|object_header::in_trans))
        == object_header::persistent)
    {
        insert_in_transaction_list(hnd);
    }
}

optimistic_repeatable_read_metaobject optimistic_repeatable_read_scheme;

optimistic_repeatable_read_metaobject optimistic_concurrent_repeatable_read_scheme(lck_temporary_exclusive);

//
// Pessimistic metaobject
//

boolean pessimistic_metaobject::abort_on_read_lock_promotion = false;

boolean pessimistic_metaobject::solve_write_conflict(hnd_t hnd)
{
	console::error("Write conflict for object %x of class \"%s\"\n",
				   (opid_t)hnd->opid, hnd->obj->cls.name);
    return False;
}

boolean pessimistic_metaobject::solve_lock_conflict(hnd_t hnd, lck_t lck)
{
    object_monitor::unlock_global();
    boolean solved = get_header(hnd)->on_lock_failed(lck);
    object_monitor::lock_global();
    return solved;
}

int pessimistic_metaobject::get_transaction_object_flags(hnd_t)
{
    return tof_unlock;
}

boolean pessimistic_metaobject::storage_lock(hnd_t hnd, lck_t lck)
{
    while (!hnd->storage->lock(hnd->opid, lck, lck_attr)) {
        if (!solve_lock_conflict(hnd, lck)) {
            return False;
        }
    }
    return True;
}

void pessimistic_metaobject::begin_write(hnd_t hnd)
{
    int was_modified = (get_header(hnd)->state | ~object_header::modified);

    optimistic_metaobject::begin_write(hnd);

    object_header* obj = get_header(hnd);
    if ((obj->state & (object_header::persistent|object_header::exl_locked))
        == object_header::persistent)
    {
        if (storage_lock(hnd, lck_exclusive)) {
            obj = get_header(hnd);

            if (abort_on_read_lock_promotion              &&
                !(obj->state & object_header::exl_locked) &&
                (obj->n_invokations != 1)) {
                // If this code is reached, this object has a read lock on it
                // and a write lock is being established.  THIS IS ILLEGAL -
                // you cannot promote a read lock to a write lock without
                // introducing the potential for a write lock conflict.
                // Examine your call stack and determine how you can release
                // the read lock before establishing the write lock.  Or just
                // establish a write lock in the first place - instead of
                // establishing a read lock.
                abort();
            }

            obj->state |= object_header::exl_locked;
            if (obj->state & object_header::invalidated) {
				if (obj->n_invokations == 1) {
                    reload_object(hnd);
                    internal_assert(!(get_header(hnd)->state
                                      & object_header::invalidated));
                } else {
                    if (!solve_write_conflict(hnd)) {
                        get_header(hnd)->state &= was_modified;
                        abort_transaction(hnd->storage->db,
                                          aborted_by_user);
                    } else {
                        get_header(hnd)->state &= ~object_header::invalidated;
                    }
                }
            }
        } else {
            get_header(hnd)->state &= was_modified;
            abort_transaction(hnd->storage->db, aborted_by_user);
        }
    }
}

void pessimistic_metaobject::begin_read(hnd_t hnd, boolean for_update)
{
    load(hnd, for_update ? lck_exclusive : intertask_read_lock, True);
    if (for_update) {
        object_header* obj = get_header(hnd);
        if (obj->state & object_header::persistent) {
            if (!(obj->state & object_header::exl_locked)) {
                if (storage_lock(hnd, lck_exclusive)) {
                    obj = get_header(hnd);
                    obj->state |= object_header::exl_locked;
                    if (obj->state & object_header::invalidated) {
                        //
                        // As far as object is always locked before access,
                        // there are no other invokations for invalidated object
                        //
                        reload_object(hnd);
                        obj = get_header(hnd);
                        internal_assert(!(obj->state&object_header::invalidated));
                    }
                    if (!(obj->state & object_header::in_trans)) {
#ifdef MURSIW_SUPPORT
                        release(hnd);
#endif
                        insert_in_transaction_list(hnd);
                        return;
                    }
                } else {
                    abort_transaction(hnd->storage->db, aborted_by_user);
                }
            }
        }
    }
#ifdef MURSIW_SUPPORT
    release(hnd);
#endif
}

void pessimistic_metaobject::commit_object_changes(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    // Locks are automatically remove when transactoin is committed
    obj->state &= ~(object_header::shr_locked|object_header::exl_locked);
    optimistic_metaobject::commit_object_changes(hnd);
}

void pessimistic_metaobject::abort_object_changes(hnd_t hnd,
                                                  abort_reason reason)
{
    object_header* obj = get_header(hnd);
    if (obj->state & (object_header::shr_locked|object_header::exl_locked)) {
        hnd->storage->unlock(hnd->opid, lck_none);
        obj->state &= ~(object_header::shr_locked|object_header::exl_locked);
    }
    optimistic_metaobject::abort_object_changes(hnd, reason);
}


void pessimistic_metaobject::release_transaction_object(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    if (obj->state & (object_header::exl_locked|object_header::shr_locked)) {
        hnd->storage->unlock(hnd->opid, lck_none);
        obj->state &= ~(object_header::shr_locked|object_header::exl_locked);
    }
    optimistic_metaobject::commit_object_changes(hnd);
}


pessimistic_metaobject pessimistic_scheme;

pessimistic_metaobject pessimistic_concurrent_read_scheme(0, lck_temporary_exclusive);

pessimistic_metaobject nowait_pessimistic_scheme(lckattr_nowait);

//
// Lazy pessimistic metaobject. This metaobject combines
// characterstics of both optimistic and pessimistic protocols.
// Lock is set after object has been modified.
//

boolean lazy_pessimistic_metaobject::solve_write_conflict(hnd_t hnd)
{
    object_monitor::unlock_global();
    boolean solved = get_header(hnd)->on_write_conflict();
    object_monitor::lock_global();
    return solved;
}

void lazy_pessimistic_metaobject::begin_write(hnd_t hnd)
{
    optimistic_metaobject::begin_write(hnd);
}

void lazy_pessimistic_metaobject::end_write(hnd_t hnd)
{
    object_header* obj = get_header(hnd);
    if ((obj->state & (object_header::persistent|object_header::modified)) ==
        (object_header::persistent|object_header::modified))
    {
        if (!(obj->state & object_header::exl_locked)) {
            if (storage_lock(hnd, lck_exclusive)) {
                obj = get_header(hnd);
                obj->state |= object_header::exl_locked;
                if (obj->state & object_header::invalidated) {
                    if (!solve_write_conflict(hnd)) {
                        abort_transaction(hnd->storage->db, aborted_by_user);
                        obj = get_header(hnd);
                    } else {
                        obj = get_header(hnd);
                        obj->state &= ~object_header::invalidated;
                    }
                }
            }
        } else {
            abort_transaction(hnd->storage->db, aborted_by_user);
            obj = get_header(hnd);
        }
    }
    optimistic_metaobject::end_write(hnd);
}

lazy_pessimistic_metaobject lazy_pessimistic_scheme;

lazy_pessimistic_metaobject nowait_lazy_pessimistic_scheme(lckattr_nowait);



//
// Pessimistic repeatable read metaobject. No conflicts are possible in this
// scheme. However this scheme is not deadlock free and(or) significantly
// reduces concurrency.
//

void pessimistic_repeatable_read_metaobject::begin_read(hnd_t hnd, boolean for_update)
{
    load(hnd, for_update ? lck_exclusive : intertask_read_lock, True);
    object_header* obj = get_header(hnd);
    lck_t lck = for_update ? lck_exclusive : read_lock;

    if (obj->state & object_header::persistent) {
        if (!(obj->state & object_header::exl_locked)
            && (lck != lck_shared
                || !(obj->state & object_header::shr_locked)))
        {
            if (storage_lock(hnd, lck)) {
                obj = get_header(hnd);
                obj->state |= (lck == lck_shared)
                    ? object_header::shr_locked : object_header::exl_locked;
                if (obj->state & object_header::invalidated) {
                    //
                    // As far as object is always locked before access,
                    // there are no other invokations for invalidated object
                    //
                    reload_object(hnd);
                    obj = get_header(hnd);
                    internal_assert(!(obj->state&object_header::invalidated));
                }
                if (!(obj->state & object_header::in_trans)) {
#ifdef MURSIW_SUPPORT
                    release(hnd);
#endif
                    insert_in_transaction_list(hnd);
                    return;
                }
            } else {
                abort_transaction(hnd->storage->db, aborted_by_user);
            }
        }
    }
#ifdef MURSIW_SUPPORT
    release(hnd);
#endif
}


pessimistic_repeatable_read_metaobject
    pessimistic_repeatable_read_scheme;

pessimistic_repeatable_read_metaobject
    pessimistic_concurrent_repeatable_read_scheme(0, lck_shared, lck_temporary_exclusive);

pessimistic_repeatable_read_metaobject
    pessimistic_exclusive_scheme(0, lck_exclusive);

pessimistic_repeatable_read_metaobject
    nowait_pessimistic_repeatable_read_scheme(lckattr_nowait);

pessimistic_repeatable_read_metaobject
    nowait_pessimistic_exclusive_scheme(lckattr_nowait, lck_exclusive);

//
// Pessimistic metaobject for child objects accessed always through parent
// object. So locking parent object effectivly syncronize access to child
// and no explicit child locking is required (parent and child
// should be located in the same storage, otherwise we can access
// deteriorated instance of child object).
//

int hierarchical_access_metaobject::get_transaction_object_flags(hnd_t)
{
    return 0; // validation is not necessary
}

hierarchical_access_metaobject hierarchical_access_scheme;

transient_metaobject::transient_metaobject() {}

void transient_metaobject::abort_object_changes(hnd_t, abort_reason) {}
void transient_metaobject::abort_transaction(const database*, abort_reason) {}
void transient_metaobject::begin_read(hnd_t, boolean) {}
void transient_metaobject::begin_transaction() {}
void transient_metaobject::begin_write(hnd_t) {}
void transient_metaobject::commit_object_changes(hnd_t) {}
void transient_metaobject::commit_transaction(const database* dbs) {}
void transient_metaobject::destroy(hnd_t) {}
void transient_metaobject::end_read(hnd_t) {}
void transient_metaobject::end_transaction(const database* dbs) {}
void transient_metaobject::end_write(hnd_t) {}
int  transient_metaobject::get_transaction_object_flags(hnd_t) { return 0; }
void transient_metaobject::lock(hnd_t,lck_t) {}
void transient_metaobject::notify(hnd_t hnd) {}
void transient_metaobject::release_transaction_object(hnd_t) {}
void transient_metaobject::unlock(hnd_t) {}
void transient_metaobject::wait(hnd_t) {}
boolean transient_metaobject::wait(hnd_t, time_t) { return True; }
boolean transient_metaobject::is_transient() { return True; }

void transient_metaobject::make_persistent(hnd_t, hnd_t)
{
    // Object instances using the transient_metaobject should NEVER be
    // persisted to the database.  Referring to a transient object
    // instance from a persistance object is a programmer error.
    assert(False);
}

void transient_metaobject::make_persistent(hnd_t, obj_storage*)
{
	// Object instances using the transient_metaobject should NEVER be
	// persisted to the database.  Referring to a transient object
	// instance from a persistance object is a programmer error.
	assert(False);
}

void transient_metaobject::put_in_cache(hnd_t)
{
    // Object instances using the transient_metaobject should NEVER be
    // persisted to the database.  Referring to a transient object
    // instance from a persistance object is a programmer error.
    assert(False);
}

void transient_metaobject::get_from_cache(hnd_t)
{
    // Object instances using the transient_metaobject should NEVER be
    // persisted to the database.  Referring to a transient object
    // instance from a persistance object is a programmer error.
    assert(False);
}

void transient_metaobject::pin_object(hnd_t)
{
    // Object instances using the transient_metaobject should NEVER be
    // persisted to the database.  Referring to a transient object
    // instance from a persistance object is a programmer error.
    assert(False);
}

void transient_metaobject::unpin_object(hnd_t)
{
    // Object instances using the transient_metaobject should NEVER be
    // persisted to the database.  Referring to a transient object
    // instance from a persistance object is a programmer error.
    assert(False);
}

void transient_metaobject::insert_in_transaction_list(hnd_t, hnd_t)
{
//  hnd->access += 1;
}

void transient_metaobject::remove_from_transaction_list(hnd_t)
{
//  hnd->access -= 1;
}

void transient_metaobject::invalidate(hnd_t)
{
    // Since transient objects can only be accessed by a single process,
    // it should never be required (or possible) to invalidate them.
    assert(False);
}


void transient_metaobject::signal_on_modification(hnd_t, event&)
{
    // Since transient objects can only be accessed by a single process,
    // it should never be required (or possible) to notify anyone that
    // another process has modified the object.
    assert(False);
}

void transient_metaobject::remove_signal_handler(hnd_t, event&)
{
}

void transient_metaobject::set_cache_limit(size_t, size_t) {}
void transient_metaobject::set_abort_transaction_hook(abort_hook_t) {}

transient_metaobject transient_scheme;

END_GOODS_NAMESPACE
