// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< OBJMGR.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET   *     ?  *
// (Generic Object Oriented Database System)                        *   /\|  *
//                                                                   *  /  \  *
//                          Created:     18-Jan-97    K.A. Knizhnik * / [] \ *
//                          Last update: 28-Nov-97    K.A. Knizhnik * GARRET *
//------------------------------------------------------------------*--------*
// Object access manager
//------------------------------------------------------------------*--------*

#include "server.h"

BEGIN_GOODS_NAMESPACE

void client_process::mark_reserved_objects()
{
    server->mem_mgr->mark_reserved_objects(this);
}

client_process::~client_process() { unlink(); }

object_access_manager::~object_access_manager() {}

// GCC 3.3.3 and higher generate incorrect code if GOODS is build with full optimization
// I found two workarounds of this problem
// 1. Use -no-schedule-insns2 compiler option
// 2. Make object_manager::create_object_instance() non inline
// That is why I decide to make non-inline all methods originally declared as inline in this module  
#define INLINE_METHOD
//#define INLINE_METHOD inline

INLINE_METHOD object_node* object_manager::create_object_node(opid_t opid)
{
    object_node* node = new (object_node_pool.alloc()) object_node(opid);
    hash_table.put(node);
    return node;
}

INLINE_METHOD object_lock* object_manager::create_object_lock()
{
    return new (object_lock_pool.alloc()) object_lock;
}

INLINE_METHOD object_reference*
object_manager::create_object_reference(stid_t sid, opid_t opid) {
    return new (object_reference_pool.alloc()) object_reference(sid, opid);
}

object_instance* object_manager::create_object_instance()
{
    return new (object_instance_pool.alloc()) object_instance;

}

INLINE_METHOD void object_manager::remove_object_node(object_node* obj)
{
    hash_table.remove(obj);
    obj->~object_node();
    object_node_pool.dealloc(obj);
}

INLINE_METHOD void object_manager::remove_object_lock(object_lock* obj)
{
    obj->~object_lock();
    object_lock_pool.dealloc(obj);
}

INLINE_METHOD void object_manager::remove_object_reference(object_reference* obj)
{
    obj->~object_reference();
    object_reference_pool.dealloc(obj);
}

INLINE_METHOD void object_manager::remove_object_instance(object_instance* obj)
{
    obj->~object_instance();
    object_instance_pool.dealloc(obj);
}

INLINE_METHOD void object_manager::remove_old_references(object_node* op)
{
    refs_cs.enter();
    object_reference* ref = op->saved_references;
    while (ref != NULL) {
        object_reference* next = ref->next;
        remove_object_reference(ref);
        ref = next;
    }
    op->saved_references = NULL;
    refs_cs.leave();
}

INLINE_METHOD void object_manager::validate(object_node* op, object_instance* ip)
{
    if (ip->state == cis_invalid) {
        ip->proc->n_invalid_instances -= 1;
        if (--op->n_invalid_instances == 0
            && !(op->state & object_node::pinned))
        {
            remove_old_references(op);
        }
    }
    ip->state = cis_valid;
}

INLINE_METHOD void object_manager::invalidate(object_node* op, object_instance* ip)
{
    if (ip->state == cis_valid) {
        op->n_invalid_instances += 1;
        ip->state = cis_invalid;
        ip->proc->n_invalid_instances += 1;
        ip->proc->invalidate(op->opid);
    }
}


boolean object_manager::open(dbs_server* server)
{
    this->server = server;
    pinned_versions_mode = False;
    opened = True;
    return True;
}

void object_manager::close()
{
    cs.enter();
    opened = False;
    cs.leave();
}

//
// Precondition for next set/release lock methods: mutex cs is locked
//
INLINE_METHOD void object_manager::set_reader_lock(object_node* op)
{
    op->n_readers += 1;
    while (op->is_locked) {
        op->queue_len += 1;
        release_event.reset();
        release_event.wait();
        op->queue_len -= 1;
    }
}

INLINE_METHOD void object_manager::set_writer_lock(object_node* op)
{
    while (op->is_locked || op->n_readers != 0) {
        op->queue_len += 1;
        release_event.reset();
        release_event.wait();
        op->queue_len -= 1;
    }
    op->is_locked = True;
}

INLINE_METHOD void object_manager::release_reader_lock(object_node* op)
{
    if (--op->n_readers == 0) {
        if (op->queue_len != 0) {
            release_event.signal();
        } else if (!op->instances && !op->waiting && !op->locking
                   && !(op->state & object_node::pinned))
        {
            remove_object_node(op);
        }
    }
}

INLINE_METHOD void object_manager::release_writer_lock(object_node* op)
{
    op->is_locked = False;
    if (op->queue_len != 0) {
        release_event.signal();
    } else if (!op->instances && !op->waiting && !op->locking
               && !(op->state & object_node::pinned))
    {
        remove_object_node(op);
    }
}

void object_manager::disconnect_client(client_process* proc)
{
	// [MC]
	TRACE_MSG((msg_important | msg_time, "Disconnecting client '%s'\n", proc->get_name()));
    cs.enter();
    if (proc->waiting) {
        unlock_object(proc->waiting->obj->opid, lck_none, proc);
    }
    while (proc->locking) {
        unlock_object(proc->locking->obj->opid, lck_none, proc);
    }
    while (!proc->instances.empty()) {
        object_instance **ipp, *ip = (object_instance*)proc->instances.next;
        opid_t opid = ip->opid;
        object_node *op = hash_table.get(opid);

        // Object is not changed so readers lock is enough
        // set_reader_lock(op);
        op->n_readers += 1;

        for (ipp = &op->instances; *ipp != ip; ipp = &(*ipp)->next_proc);
        validate(op, ip);
        *ipp = ip->next_proc;
        remove_object_instance(ip);

        release_reader_lock(op);
    }
	cs.leave();
	// [MC]
	TRACE_MSG((msg_important | msg_time, "Client '%s' disconnected\n", proc->get_name()));
}

void object_manager::abort_locker(object_node* op, client_process* proc)
{
    client_process* victim_proc = NULL;
    cs.enter();
    if (proc->waiting != NULL && op->locking != NULL) {
        object_lock *victim = NULL, *last_waiting = NULL;
        for (object_lock* lp = op->locking; lp != NULL; lp = lp->next_proc) {
            if (lp->proc->suspended) {
                server->message(msg_error|msg_time, 
                                 "Suspended process '%s' causes lock timeout "
                                 "expiration\n", lp->proc->get_name());
                cs.leave();
                return;
            }
            if (lp->proc->waiting != NULL) {
                last_waiting = lp;
            }
            victim = lp;
        }
        if (last_waiting != NULL) {
            victim = last_waiting;
        }
        if (!victim->proc->terminated) {
            victim_proc = victim->proc;
			//[MC] Added more detailed tracing
            server->message(msg_error|msg_time,
                             "Abort locker '%s' due to expiration of lock object %x"
							 " timeout. Killer '%s'\n", victim_proc->get_name(), op->opid, proc->get_name());
            dump(victim_proc, "");

			//[MC] Added even more detailed tracking
			dump(proc, "");

            victim_proc->terminated = True;
        }
    }
    cs.leave();
    if (victim_proc != NULL) { 
        victim_proc->disconnect();
    }
}

int object_manager::get_number_of_objects() 
{
    return hash_table.used;
}

boolean object_manager::lock_object(opid_t opid, lck_t lck,
                                 int attr, boolean send_ack, lck_t& plck,
                                 client_process* proc)
{
    cs.enter();

    SERVER_TRACE_MSG((msg_locking, "Client '%s' try to lock object %x mode %x\n", 
               proc->get_name(), opid, lck));

    if (send_ack) { 
	dbs_request req;
	req.cmd = dbs_request::cmd_ack_lock;
	proc->write(&req, sizeof req);
    }
    boolean granted = True;
    object_node *op = hash_table.get(opid);
    object_lock *lp, **lpp;
    int n_exl_locks = 1;
    int n_shr_locks = 0;
    object_lock* prev_lock = NULL; // lock previously granted to this process

    if (op == NULL) {
        op = create_object_node(opid);
    }
    //
    // We use "honest" locking strategy: lock request will not be granted
    // if there is older waiting request for this object.
    // There is only one exception from this rule: we allow shared lock to
    // be upgraded to exclusive despite to the existence of waiting
    // lock requests which can't be granted.
    //
    if (op->waiting == NULL || lck == lck_exclusive) {
        n_exl_locks = 0;
        for (lp = op->locking; lp != NULL; lp = lp->next_proc) {
            if (lp->proc != proc) {
                if (lp->mode == lck_shared) {
                    n_shr_locks += 1;
                } else {
                    n_exl_locks += 1;
                }
            } else {
                internal_assert(prev_lock == NULL);
                prev_lock = lp;
            }
        }
    }
    if ((n_shr_locks == 0 || lck == lck_shared) && n_exl_locks == 0) {
        if (prev_lock != NULL) { // upgrade lock
            plck = (lck_t)prev_lock->mode;
            if ((int)prev_lock->mode < (int)lck) {
                prev_lock->mode = lck;
            }
            prev_lock->attr = attr;
            SERVER_TRACE_MSG((msg_locking, "Client '%s' upgrade lock of object %x"
                       " to mode %x\n", proc->get_name(), opid, lck));
        } else {
            lp = create_object_lock();
            lp->proc = proc;
            lp->mode = lck;
            lp->attr = attr;
            lp->obj = op;
            lp->next_proc = op->locking;
            op->locking = lp;
            lp->prev_obj = NULL;
            if ((lp->next_obj = proc->locking) != NULL) {
                lp->next_obj->prev_obj = lp;
            }
            proc->locking = lp;
            plck = lck_none;
            SERVER_TRACE_MSG((msg_locking, 
                       "Client '%s' granted lock of object %x in mode %x\n",
                       proc->get_name(), opid, lck));
        }
    } else { // will block
        if (attr & lckattr_nowait) {
            granted = False;
        } else {
            lp = create_object_lock();
            lp->proc = proc;
            lp->attr = attr;
            lp->mode = lck;
            lp->obj = op;
            for (lpp = &op->waiting; *lpp != NULL; lpp = &(*lpp)->next_proc);
            lp->next_proc = NULL;
            *lpp = lp;
            proc->waiting = lp;
            SERVER_TRACE_MSG((msg_locking, 
                      "Client '%s' waiting for object %x lock mode %x, "
                      "locked by process '%s' in mode %x, shared locks = %d\n",
                      proc->get_name(),opid,lck,op->locking->proc->get_name(),
                      op->locking->mode, n_shr_locks));
            cs.leave();
            while (!proc->lock_sem.wait_with_timeout(lock_timeout)) {
                abort_locker(op, proc);
            }
            if (proc->terminated) { // process was disconnected
                proc->disconnect(); // delete client agent
                return False;
            }
            SERVER_TRACE_MSG((msg_locking, 
                       "Client '%s' granted lock of object %x in mode %x\n",
                       proc->get_name(), opid, lck));
            return True;
        }
    }
    cs.leave();
    return granted;
}

void object_manager::unlock_object(opid_t opid, lck_t lck,
                                   client_process* proc)
{
    cs.enter();

    SERVER_TRACE_MSG((msg_locking, "Client '%s' unlock object %x\n", 
               proc->get_name(), opid));

    object_node *op = hash_table.get(opid);
    object_lock *lp = NULL, **lpp = NULL;

    if (op == NULL) { // Nothing to unlock
        cs.leave();
        return;
    }

    if (proc->waiting && proc->waiting->obj == op) {
        for (lpp=&op->waiting; (lp=*lpp)->proc != proc; lpp=&lp->next_proc);
    } else {
        for (lpp = &op->locking;
             (lp = *lpp) != NULL && lp->proc != proc;
             lpp = &lp->next_proc)
        {
            // Look through the list of granted locks
        }
    }
    if (lp == NULL) { // Nothing to unlock
        cs.leave();
        return;
    }
    //
    // We retry to grant waiting locks after removing even non-granted
    // lock since as FIFO discipline is used for locks so this lock
    // can prevent other locks from been satisfied
    //
    if (lck == lck_none) {
        *lpp = lp->next_proc;
        if (proc->waiting != lp) {
            if (lp->prev_obj == NULL) { 
                internal_assert(proc->locking == lp);
                proc->locking = lp->next_obj;
            } else { 
                lp->prev_obj->next_obj = lp->next_obj;
            }
            if (lp->next_obj != NULL) { 
                lp->next_obj->prev_obj = lp->prev_obj;
            }
        } else {
            proc->waiting = NULL;
        }
        remove_object_lock(lp);
    } else {
        lp->mode = lck;
    }

    if (!op->instances && !op->locking && !op->waiting
        && op->n_readers == 0 && !op->is_locked && op->queue_len == 0
        && !(op->state & object_node::pinned))
    {
        remove_object_node(op);
    } else if (op->waiting) {
        int n_shr_locks = 0;

        for (lp = op->locking; lp != NULL; lp = lp->next_proc) {
            if (lp->mode == lck_shared) {
                n_shr_locks += 1;
            } else {
                cs.leave();
                return;
            }
        }
        for (lpp = &op->waiting; (lp = *lpp) != NULL;) {
            if (lp->mode == lck_exclusive) {
                if (n_shr_locks == 0) {
                    *lpp = lp->next_proc;
                    lp->next_proc = op->locking;
                    op->locking = lp;
                    lp->prev_obj = NULL;
                    if ((lp->next_obj = lp->proc->locking) != NULL) { 
                        lp->next_obj->prev_obj = lp;
                    }
                    lp->proc->locking = lp;
                    lp->proc->waiting = NULL;
                    lp->proc->lock_sem.signal();
                } else if (n_shr_locks == 1 && lp->proc == op->locking->proc) {
                    *lpp = lp->next_proc;
                    op->locking->mode = lck_exclusive;
                    op->locking->attr = lp->attr;
                    remove_object_lock(lp);
                    lp = op->locking;
                    lp->proc->waiting = NULL;
                    lp->proc->lock_sem.signal();
                }
                break; // "honest policy": locks are granted in FIFO order
            } else {
                //
                // As there are no exclusive locks
                // then shared lock can be certanly granted
                //
                *lpp = lp->next_proc;
                lp->next_proc = op->locking;
                op->locking = lp;
                lp->prev_obj = NULL;
                if ((lp->next_obj = lp->proc->locking) != NULL) { 
                    lp->next_obj->prev_obj = lp;
                }
                lp->proc->locking = lp;
                lp->proc->waiting = NULL;
                lp->proc->lock_sem.signal();
                n_shr_locks += 1;
            }
        }
    }
    cs.leave();
}

cis_state object_manager::get_object_state(opid_t opid, client_process* proc)
{
    cs.enter();

    object_node *op = hash_table.get(opid);

    if (op != NULL) {
        for (object_instance* ip=op->instances; ip != NULL; ip=ip->next_proc) {
            if (ip->proc == proc) {
                cis_state state = ip->state;
                cs.leave();
                return state;
            }
        }
    }
    cs.leave();
    return cis_none;
}


void object_manager::load_object(opid_t opid, int lof_flags,
                                 client_process* proc)
{
    cs.enter();

    SERVER_TRACE_MSG((msg_object, "Client '%s' load object %x, flags = %x\n", 
               proc->get_name(), opid, lof_flags));

    object_node *op = hash_table.get(opid);
    if (op == NULL) {
        op = create_object_node(opid);
    }
    set_reader_lock(op);

    object_instance *ip;
    for (ip = op->instances; ip != NULL; ip = ip->next_proc) {
        if (ip->proc == proc) {
            if (!(lof_flags & lof_copy)) {
                validate(op, ip);
            }
            cs.leave();
            return;
        }
    }
    ip = create_object_instance();
    ip->next_proc = op->instances;
    op->instances = ip;
    ip->proc = proc;
    ip->opid = opid;
    ip->state = cis_valid;
    ip->link_after(&proc->instances);
    proc->n_instances += 1;
    cs.leave();
}

void object_manager::new_object(opid_t opid, client_process* proc)
{
    cs.enter();
    object_node *op = hash_table.get(opid);
    if (op == NULL) {
        op = create_object_node(opid);
    }
    object_instance *ip = create_object_instance();
    ip->next_proc = op->instances;
    op->instances = ip;
    op->state = object_node::raw;
    ip->proc = proc;
    ip->opid = opid;
    ip->state = cis_new;
    ip->link_after(&proc->instances);
    proc->n_instances += 1;
    cs.leave();
}

//
// This function construct list of references which were present in old
// version of object but no in new one. As far as finding extact difference
// of two sets of references require N^2 compare operations, then we make
// a decision to compare anly references located at the same position
// within object and near neighbours. So two common situations are handled:
// when references are not changed and when references are shifted
// (left or right) in array as a result of deleting or inserting element.
//
INLINE_METHOD object_reference*
object_manager::save_old_version(object_node*      op,
                                 cpid_t            new_cpid,
                                 size_t            new_size,
                                 void*             new_body,
                                 object_reference& chain)
{
    dbs_handle hnd;
    object_reference* rp = &chain;

    server->mem_mgr->get_handle(op->opid, hnd);
    cpid_t old_cpid = hnd.get_cpid();
    size_t old_size = hnd.get_size();

    int n_old_refs = server->class_mgr->get_number_of_references(old_cpid,
                                                                 old_size);
    internal_assert(n_old_refs >= 0);

    if (n_old_refs != 0) {
        int n_new_refs = server->class_mgr->get_number_of_references(new_cpid,
                                                                     new_size);
        internal_assert(n_new_refs >= 0);
        fposi_t old_pos = hnd.get_pos();
		char* new_refs = (char*)new_body;
		objref_t new_opid[2], old_opid;
        stid_t new_sid[2], old_sid;

        new_opid[0] = new_opid[1] = 0;
        new_sid[0] = new_sid[1] = 0;

        if (n_new_refs != 0) {
            n_new_refs -= 1;
            new_refs = unpackref(new_sid[1], new_opid[1], new_refs);
        }
        do {
            int n_refs = size_t(n_old_refs) < itemsof(refs_buf)
                ? n_old_refs : itemsof(refs_buf);
            n_old_refs -= n_refs;
            server->pool_mgr->read(old_pos, refs_buf,
                                   n_refs*sizeof(dbs_reference_t));
            old_pos += n_refs*sizeof(dbs_reference_t);

            char* old_refs = (char*)refs_buf;

            refs_cs.enter();
            do {
                old_refs = unpackref(old_sid, old_opid, old_refs);
                if ((new_opid[0] == old_opid && new_sid[0] == old_sid) ||
                    (new_opid[1] == old_opid && new_sid[1] == old_sid))
                {
                    if (n_new_refs != 0) {
                        n_new_refs -= 1;
                        new_opid[0] = new_opid[1];
                        new_sid[0] = new_sid[1];
                        new_refs=unpackref(new_sid[1], new_opid[1], new_refs);
                    }
                } else {
                    if (n_new_refs != 0) {
                        n_new_refs -= 1;
                        new_opid[0] = new_opid[1];
                        new_sid[0] = new_sid[1];
                        new_refs=unpackref(new_sid[1], new_opid[1], new_refs);
                        if (new_opid[1] == old_opid && new_sid[1] == old_sid) {
                            continue;
                        }
                    }
                    if (old_opid != 0 && (old_sid & TAGGED_POINTER) == 0) {
                        rp=rp->next = create_object_reference(old_sid, old_opid);
                    }
                }
            } while (--n_refs != 0);
            refs_cs.leave();
        } while (n_old_refs != 0);
    }
    return rp;
}

void object_manager::modify_object(opid_t opid)
{
    cs.enter();
    SERVER_TRACE_MSG((msg_object, "Modify object %x\n", opid));

    object_node *op = hash_table.get(opid);
    if (op == NULL) {
        op = create_object_node(opid);
    }
    set_writer_lock(op);
    cs.leave();
}

void object_manager::write_object(opid_t  opid,
                                  cpid_t  new_cpid,
                                  fposi_t new_pos,
                                  size_t  new_size,
                                  void*   new_body,
                                  client_process* proc)
{
    object_reference  saved_refs_first;
    object_reference* saved_refs_last = &saved_refs_first;

    SERVER_TRACE_MSG((msg_object, "Client %s update object %x\n", 
               proc ? proc->get_name() : "administrator", opid));

    cs.enter();
    object_node* op = hash_table.get(opid);
    cs.leave();

    if (new_cpid != opid) { // not a class descriptor
        if ((op->instances != NULL
             && (op->instances->proc!=proc || op->instances->next_proc!=NULL))
            || (pinned_versions_mode
                && !(op->state & (object_node::scanned|object_node::raw))))
        {
            saved_refs_last = save_old_version(op, new_cpid, new_size,
                                               new_body, saved_refs_first);
        }
    }

    if (new_cpid != opid && server->class_mgr->is_external_blob(new_cpid)) { 
        server->write_external_blob(opid, new_body, new_size);
    } else { 
        server->pool_mgr->write(new_pos, new_body, new_size);
    }

    cs.enter();
    for (object_instance* ip = op->instances; ip != NULL; ip = ip->next_proc)
    {
        if (ip->proc == proc) {
            if (ip->state == cis_invalid) {
                server->message(msg_error|msg_time, 
                                 "Process %s modify invalid instance %x\n",
                                  proc->get_name(), opid);
            }
            validate(op, ip);
        } else {
            invalidate(op, ip);
        }
    }
    if (saved_refs_last != &saved_refs_first) {
        if (pinned_versions_mode && !(op->state & object_node::scanned)) {
            op->state |= object_node::pinned;
        }
        saved_refs_last->next = op->saved_references;
        op->saved_references = saved_refs_first.next;
    } else if (op->state & object_node::raw) {
        op->state &= ~object_node::raw;
        if (pinned_versions_mode) {
            op->state |= object_node::pinned;
        }
    }
    cs.leave();
}

void object_manager::scan_object(opid_t opid)
{
    cs.enter();
    object_node *op = hash_table.get(opid);
    if (op == NULL) {
        op = create_object_node(opid);
    }
    set_reader_lock(op);
    internal_assert(!(op->state & object_node::raw));
    op->state |= object_node::scanned;
    cs.leave();
    refs_cs.enter();
    for (object_reference* rp = op->saved_references;
         rp != NULL;
         rp = rp->next)
    {
        server->mem_mgr->gc_follow_reference(rp->sid, rp->opid);
    }
    refs_cs.leave();
    cs.enter();
    if (op->state & object_node::pinned) {
        op->state &= ~object_node::pinned;
        if (op->n_invalid_instances == 0) {
            remove_old_references(op);
        }
    }
    cs.leave();
}

void object_manager::release_object(opid_t opid)
{
    cs.enter();
    object_node *op = hash_table.get(opid);
    if (op->is_locked) { // I am writer
        release_writer_lock(op);
    } else { // I am reader
        release_reader_lock(op);
    }
    cs.leave();
}

void object_manager::throw_object(opid_t opid, client_process* proc)
{
    cs.enter();

    SERVER_TRACE_MSG((msg_object, "Client '%s' throw object %x\n", 
               proc->get_name(), opid));

    object_node *op = hash_table.get(opid);
    boolean is_raw_object = False;
    if (op != NULL) { 
        set_reader_lock(op);
        object_instance *ip;
        boolean is_raw_object = (op->state & object_node::raw) != 0;
        for (ip=op->instances; ip != NULL && ip->proc != proc; ip=ip->next_proc);
        
        if (ip != NULL) {
            internal_assert(is_raw_object == (ip->state == cis_new));
            validate(op, ip);
            ip->state = cis_thrown;
        }
        release_reader_lock(op);
    }
    cs.leave();

    if (is_raw_object) { 
        server->mem_mgr->dealloc(opid);
    }
}


void object_manager::forget_object(opid_t opid, client_process* proc)
{
    cs.enter();

    SERVER_TRACE_MSG((msg_object, "Client '%s' forget object %x\n", 
               proc->get_name(), opid));

    object_node *op = hash_table.get(opid);

    if (op != NULL) {
        object_instance *ip, **ipp;
        set_reader_lock(op);
        for (ipp = &op->instances;
             (ip = *ipp) != NULL && ip->proc != proc;
             ipp = &ip->next_proc);

        if (ip != NULL) {
            validate(op, ip);
            *ipp = ip->next_proc;
            remove_object_instance(ip);
        }
        release_reader_lock(op);
    }
    cs.leave();
}

void object_manager::set_save_version_mode(boolean enabled)
{
    if (enabled) {
        pinned_versions_mode = True;
    } else {
        if (pinned_versions_mode) {
            cs.enter();
            pinned_versions_mode = False;
            for (size_t i = 0, n = hash_table.size; i < n; i++) {
                object_node *op = hash_table.table[i];
                while (op != NULL) { 
                    object_node* next = op->next;
                    op->state &= ~object_node::scanned;
                    if (op->state & object_node::pinned) {
                        op->state &= ~object_node::pinned;
                        if (op->n_invalid_instances == 0) {
                            remove_old_references(op);
                            if (!op->instances && !op->locking && !op->waiting
                                && op->n_readers == 0 && !op->is_locked
                                && op->queue_len == 0)
                            {
                                remove_object_node(op);
                            }
                        }
                    }
                    op = next;
                }
            }
            cs.leave();
        }
    }
}

void object_manager::mark_loaded_objects(int pass)
{
    cs.enter();
    if (pass == 1) { // this pass is performed before mark stage of GC.
        for (int i = 0, n = hash_table.size; i < n; i++) {
            for (object_node *op = hash_table.table[i]; op != NULL; op = op->next) {
                if (!(op->state & object_node::raw)) {
                    server->mem_mgr->gc_follow_reference(server->id, op->opid);
                }
            }
        }
    } else { // this pass is performed before sweep phase of GC
        pinned_versions_mode = False;
        for (int i = 0, n = hash_table.size; i < n; i++) {
            object_node *op = hash_table.table[i]; 
            while (op != NULL) {
                object_node* next = op->next; 
                if (!(op->state & object_node::scanned)) {
                    server->mem_mgr->gc_mark_object(op->opid);
                } else {
                    op->state &= ~object_node::scanned;
                }
                if (op->state & object_node::pinned) {
                    op->state &= ~object_node::pinned;
                    if (op->n_invalid_instances == 0) {
                        remove_old_references(op);
                        if (!op->instances && !op->locking && !op->waiting
                            && op->n_readers == 0 && !op->is_locked
                            && op->queue_len == 0)
                        {
                            remove_object_node(op);
                        }
                    }
                }
                op = next;
            }
        }
    }
    cs.leave();
}

void object_manager::initialize() {}

object_manager::object_manager(time_t timeout)
: object_node_pool(sizeof(object_node), True, 16*1024, 32),
  object_lock_pool(sizeof(object_lock)),
  object_reference_pool(sizeof(object_reference)),
  object_instance_pool(sizeof(object_instance)),
  release_event(cs)
{
    lock_timeout = timeout;
    opened = False;
}


void object_manager::dump(client_process* proc, const char*)
{
    cs.enter();
    console::output("Client \"%s\"%s, %lu proceed requests, "
                     "%ld loaded objects, %lu invalidated\n",
                    proc->get_name(),
                    proc->suspended ? " (suspended)" : "",
                    proc->req_count,
                    proc->n_instances, proc->n_invalid_instances);
    if (proc->waiting) {
        console::output("--- waiting for object %c:%x locked by:",
                         proc->waiting->mode == lck_shared ? 'S' : 'X',
                         proc->waiting->obj->opid);
        for (object_lock* lp = proc->waiting->obj->locking;
             lp != NULL; lp = lp->next_proc)
        {
            console::output(" \"%s\"", lp->proc->get_name());
        }
        console::output("\n");
    }
    if (proc->locking) {
        console::output("+++ locking objects:");
        for (object_lock* lp = proc->locking; lp != NULL; lp = lp->next_obj)
        {
            console::output(" %c:%x", lp->mode == lck_shared ? 'S' : 'X',
                             lp->obj->opid);
        }
        console::output("\n");
    }
    cs.leave();
}

void object_manager::dump(const char*)
{
    console::output("Memory usage: nodes=%u, locks=%u, references=%u, instances=%u\n",
                    (unsigned)object_node_pool.allocated_size(),
                    (unsigned)object_lock_pool.allocated_size(),
                    (unsigned)object_reference_pool.allocated_size(),
                    (unsigned)object_instance_pool.allocated_size());
}

void object_manager::set_lock_timeout(time_t timeout)
{
    lock_timeout = timeout;
}

void object_manager::shutdown() {}

END_GOODS_NAMESPACE
