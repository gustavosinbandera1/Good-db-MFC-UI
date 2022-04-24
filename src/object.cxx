// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< OBJECT.CXX >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 26-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Base class for all GOODS classes
//-------------------------------------------------------------------*--------*

#include "goods.h"

BEGIN_GOODS_NAMESPACE

#define INIT_OBJECT_HANDLE_HASH_TABLE_SIZE 1997

static const size_t prime_numbers[] = {
    17,             /* 0 */
    37,             /* 1 */
    79,             /* 2 */
    163,            /* 3 */
    331,            /* 4 */
    673,            /* 5 */
    1361,           /* 6 */
    2729,           /* 7 */
    5471,           /* 8 */
    10949,          /* 9 */
    21911,          /* 10 */
    43853,          /* 11 */
    87719,          /* 12 */
    175447,         /* 13 */
    350899,         /* 14 */
    701819,         /* 15 */
    1403641,        /* 16 */
    2807303,        /* 17 */
    5614657,        /* 18 */
    11229331,       /* 19 */
    22458671,       /* 20 */
    44917381,       /* 21 */
    89834777,       /* 22 */
    179669557,      /* 23 */
    359339171,      /* 24 */
    718678369,      /* 25 */
    1437356741,     /* 26 */
    2147483647      /* 27 (largest signed int prime) */
};

class object_handle_hash_table 
{ 
  private:
    hnd_t* table;
    size_t size_log;
    size_t size;
    size_t used;
    size_t threshold;
    double load_factor;

  public:
    object_handle_hash_table(size_t init_size, double load_factor) { 
        size_t i;
        for (i = 0; prime_numbers[i] < init_size; i++); 
        size_log = i;
        size = prime_numbers[i];
        this->load_factor = load_factor;
        threshold = (size_t)(size*load_factor);
        table = new hnd_t[size];
        memset(table, 0, size*sizeof(hnd_t));
        used = 0;
    }

    ~object_handle_hash_table() { 
        delete[] table;
    }
	
	hnd_t find(obj_storage* storage, objref_t opid) {
		for (hnd_t hnd = table[GET_OID(opid) % size]; hnd != 0; hnd = hnd->next) {
			if (GET_OID(hnd->opid) == GET_OID(opid) && hnd->storage == storage) {
				return hnd;
			}
		}
		return 0;
	}

#ifdef _DEBUG
    void dump() { 
        for (size_t i = 0; i < size; i++) { 
            for (hnd_t hnd = table[i]; hnd != 0; hnd = hnd->next) { 
                hnd->dump();
            }
        }
    }
#endif

    void remove(hnd_t hnd) { 
		used -= 1;
		hnd_t hp, *hpp = &table[GET_OID(hnd->opid) % size];
        while ((hp = *hpp) != hnd) { 
            hpp = &hp->next;
        }
        *hpp = hnd->next;
    }

    void add(hnd_t hnd) { 
        if (++used > threshold) { 
            size_t new_size = prime_numbers[++size_log];
            hnd_t* new_table = new hnd_t[new_size];
            memset(new_table, 0, new_size*sizeof(hnd_t));
            for (size_t i = 0; i < size; i++) {
                hnd_t curr, next;
                for (curr = table[i]; curr != 0; curr = next) {
					next = curr->next;
					size_t h = GET_OID(curr->opid) % new_size;
                    curr->next = new_table[h];
                    new_table[h] = curr;
                }
            }
            delete[] table;
            table = new_table;
            size = new_size;
            threshold = (size_t)(new_size * load_factor);        
		}
		size_t h = GET_OID(hnd->opid) % size;
        hnd->next = table[h];
        table[h] = hnd;        
    }
};

   


//
// Object header index management
//

#define INIT_INDEX_TABLE_SIZE   (64*1024)

dnm_object_pool object_handle::handle_pool(sizeof(object_handle), False);
static object_handle_hash_table hnd_hash_table(INIT_OBJECT_HANDLE_HASH_TABLE_SIZE, 1.4);

void object_handle::cleanup_object_pool()
{
    handle_pool.toggle_cleanup(True);
}

hnd_t object_handle::find(obj_storage* storage, objref_t opid)
{
    return hnd_hash_table.find(storage, opid);
}

hnd_t object_handle::allocate(object* obj, obj_storage* storage)
{
    object_monitor::lock_global();
    hnd_t hnd = (hnd_t)handle_pool.alloc();
    hnd->storage = storage;
    hnd->obj = obj;
    hnd->access = 0;
    hnd->opid = 0;
    object_monitor::unlock_global();
    return hnd;
}

void object_handle::deallocate(hnd_t hnd) 
{
    internal_assert(hnd->access == 0);
    if (IS_VALID_OBJECT(hnd->obj)) { 
        if (!(hnd->obj->state & 
              (object_header::persistent|object_header::removed))) 
        { 
            //
            // Destructor of 'object' will call 'deallocate' method
            // once again but with obj == THROWN_OBJECT or obj == NULL
            //
            hnd->obj->remove_object();
        }
        return;
    } else if (hnd->obj == THROWN_OBJECT) { 
        internal_assert(hnd->opid != 0); 
        hnd->storage->forget_object(hnd->opid); 
    }
    if (hnd->opid != 0) { 
        deassign_persistent_oid(hnd); 
    }
    handle_pool.dealloc(hnd);
}

void object_handle::remove_from_storage(hnd_t hnd) 
{ 
    hnd->storage->deallocate(hnd->opid); 
    deassign_persistent_oid(hnd);
}

void object_handle::assign_persistent_oid(hnd_t hnd, objref_t opid)
{
    hnd->opid = opid;
    hnd_hash_table.add(hnd);
    hnd->storage->add_reference();
}
    
void object_handle::deassign_persistent_oid(hnd_t hnd)
{
    hnd_hash_table.remove(hnd);
    hnd->opid = 0;
    hnd->storage->remove_reference();
}


#ifdef _DEBUG
/*****************************************************************************\
dump - dump this handle's object class type & memory address
-------------------------------------------------------------------------------
This function prints this handle's object class type & memory address, as well
as the number of references to this object, the object's storage and object ID.
\*****************************************************************************/
   
void object_handle::dump(void)
{
    char         storageID[20];
    const char*  type;
   
    if (obj != NULL) {
        type = obj->cls.name;
    } else {
        type = "n/a";
    }
    if (storage != NULL) {
        sprintf(storageID, "%04lx", (long)storage->id);
    } else {
        sprintf(storageID, "none");
	}
#ifndef _WIN64
	console::output("Object %s:%08x at (%08x) of class \"%s\" "
					"has %ld references.\n", storageID, (opid_t)opid, (nat4)obj, type,
                    access);
#endif
}


/*****************************************************************************\
dumpCachedDBhandles - print class type & memory address of each handle's object
-------------------------------------------------------------------------------
This function prints the class type, reference count, and memory address of
each cached database object referenced by an object handle.  The output is sent
to the default console output.
\*****************************************************************************/
   
void object_handle::dumpCachedDBhandles(void)
{
    hnd_hash_table.dump();
}
#endif


void object_handle::make_persistent(hnd_t hnd, obj_storage* parent_storage)
{
    object* obj = hnd->obj;
    if (hnd->storage != NULL) { 
        assert(hnd->storage->db == parent_storage->db);
    } else { 
        hnd->storage = parent_storage;
    }
    class_descriptor* desc = &obj->cls; 
    obj->cpid = hnd->storage->get_cpid_by_descriptor(desc);
    int flags = aof_none;
    if ((desc->class_attr & class_descriptor::cls_aligned) || (obj->state & object_header::clustered)) { 
        flags |= aof_aligned;
    }
    if (obj->cluster != 0) { 
        flags |= aof_clustered;
        if (obj->cluster->opid == 0) { 
            make_persistent(obj->cluster, parent_storage);
            assert(obj->cluster->opid != 0);
        }
    }
	objref_t opid = hnd->storage->allocate(obj->cpid, desc->packed_size((char*)obj, obj->size), flags, obj->cluster);
	assign_persistent_oid(hnd, opid);
	obj->state |= object_header::created;
} 
    

hnd_t object_handle::create_persistent_reference(obj_storage* storage,
												 objref_t opid, int n_refs)
{ 
    hnd_t hnd = find(storage, opid);
    if (hnd == 0) {
        hnd = allocate(NULL, storage);
        assign_persistent_oid(hnd, opid);
    } 
    hnd->access += n_refs;
    return hnd;
}

//
// Object control methods
//

boolean object_header::on_write_conflict() 
{ 
    console::error("Write conflict for object %x of class \"%s\"\n",
                   hnd, ((object*)this)->cls.name);
    return False;
}

boolean object_header::on_lock_failed(lck_t) { return False; } 

void object_header::on_loading() {}

void object_header::remove_object() 
{ 
    state |= removed;
    delete_object(); 
}

void object_header::delete_object()
{
    ::delete this;
}

object_header::~object_header() {}

//
// Object methods
//

metaobject* object::default_mop; 

#ifdef PROHIBIT_EXPLICIT_DEALLOCATION  
void  object::operator delete(void* ptr) { delete_object(); }
#endif

#ifdef CHECK_FOR_MEMORY_LEAKS
void*  object::operator new(size_t size, int block_type, char const* file, unsigned int line) { 
    size = DOALIGN(size, 8);
    char* ptr = new (_NORMAL_BLOCK, file, line) char[size]; 
    memset(ptr, 0, size);
    return ptr;
}
#else
void*  object::operator new(size_t size) { 
    size = DOALIGN(size, 8);
    char* ptr = (char *)::operator new(size);
    memset(ptr, 0, size);
    return ptr;
}
#endif
void*  object::operator new(size_t, class_descriptor& desc, size_t varying_length) {
    size_t size = DOALIGN(desc.fixed_size+desc.varying_size*varying_length,
                          8);
    char* ptr = (char *)::operator new(size);
    memset(ptr, 0, size);
    return ptr;
}

object* object::become(object* new_obj)
{
    object_monitor::lock_global();
    assert(new_obj->state == 0 && new_obj->n_invokations == 0); 

    hnd_t new_hnd = new_obj->hnd; 
        
    new_obj->monitor = monitor;
    new_obj->hnd = hnd; 
    new_obj->next = next; 
    new_obj->prev = prev; 
    new_obj->cluster = cluster; 
    new_obj->state = state; 
    new_obj->n_invokations = n_invokations; 

    hnd->obj = new_obj; 

    state = 0;
    monitor = 0; 
    n_invokations = 0;
    next = prev = 0;
    cluster = NULL;
    if (new_hnd != 0 && new_hnd != hnd) { 
        //
        // Swap objects 
        //
        internal_assert(new_hnd->obj == new_obj);
        hnd = new_hnd; 
        hnd->obj = this;
        if (hnd->access == 0) { 
            remove_object();
        }
    } else {
        hnd = 0;
        remove_object();
    }
    object_monitor::unlock_global();
    return new_obj;
}

object* object::load_last_version()
{
    object_monitor::lock_global();
    hnd_t copy_hnd = object_handle::allocate(NULL);
    copy_hnd->storage = hnd->storage; 
    copy_hnd->opid = hnd->opid;
    copy_hnd->obj = COPIED_OBJECT; 
    hnd->storage->load(copy_hnd, lof_copy);
    object_monitor::unlock_global();
    return copy_hnd->obj; 
} 

void object::begin_transaction()
{
    object_monitor::lock_global();
    mop->begin_transaction(); 
    object_monitor::unlock_global();
}

void object::abort_transaction()
{
    object_monitor::lock_global();
    mop->abort_transaction(get_database()); 
    object_monitor::unlock_global();
}

void object::end_transaction()
{
	object_monitor::lock_global();
	mop->end_transaction(get_database());
    object_monitor::unlock_global();
}

boolean object::is_abstract_root() const
{ 
    return False; 
}

void object::signal_on_modification(event& e) const
{
    object_monitor::lock_global();
    mop->signal_on_modification(hnd, e);
    object_monitor::unlock_global();
}

void object::remove_signal_handler(event& e) const
{
    object_monitor::lock_global();
    mop->remove_signal_handler(hnd, e);
    object_monitor::unlock_global();
}

void object::cluster_with(object_ref const& r, boolean chained) 
{
    assert(hnd != 0);
    if (hnd->opid == 0 && cluster == NULL) { // object not yet persistent or clustered
        cluster = r.get_handle(); 
        hnd->storage = cluster->storage;
        if (chained) { 
            state |= clustered;
        }
        if (cluster->obj != NULL) {
            cluster->obj->state |= clustered;
        }
    }
}

void object::attach_to_storage(database const* db, int sid) const
{
    assert(hnd != 0);
    assert(hnd->opid == 0); // object not yet persistent
    assert(sid < db->n_storages);
    hnd->storage = db->storages[sid];
}

void object::wait() const
{
    object_monitor::lock_global();
    mop->wait(hnd);
    object_monitor::unlock_global();
}

boolean object::wait(time_t timeout) const
{
    object_monitor::lock_global();
    boolean result = mop->wait(hnd, timeout);
    object_monitor::unlock_global();
    return result;
}

void object::notify() const
{
    object_monitor::lock_global();
    mop->notify(hnd);
    object_monitor::unlock_global();    
}

database* object::get_database() const
{
	return hnd->storage ? hnd->storage->db : (database*)0;
}

 
object::~object() 
{ 
    object_monitor::lock_global();
    if (cls.n_varying_references > 0) { 
        cls.destroy_varying_part_references(this); 
    }
    if (hnd != 0) { 
        //
        // We can remove object when there are no references to it or it is
        // persistent object which can be loaded on demand from database
        //
        assert(n_invokations == 0 && (hnd->access == 0 || hnd->opid != 0)); 

        mop->destroy(hnd);

        if (state & persistent) { 
            hnd->obj = THROWN_OBJECT;
            if (hnd->access != 0) { 
                hnd->storage->throw_object(hnd->opid); 
            } 
        } else { 
            hnd->obj = NULL;
        }
        if (hnd->access == 0) { 
            object_handle::deallocate(hnd);
        }
    }
    object_monitor::unlock_global();
}

#if GOODS_RUNTIME_TYPE_CHECKING && !defined(__GOODS_MFCDLL)
template<>
class_descriptor& classof(object const*) { return object::self_class; }
#endif

field_descriptor& object::describe_components() { return NO_FIELDS; }

object* object::constructor(hnd_t hnd, class_descriptor& desc, size_t)
{ 
    return NEW object(hnd, desc, 0); 
}

//[MC] -- change static member self_class to function to avoid race condition
class_descriptor& object::get_self_class()
{
	static class_descriptor _self_class("object", sizeof(object), &optimistic_scheme, &object::constructor, &((object*)0)->object::describe_components(), NULL);
	return _self_class;
}

boolean storage_root::is_abstract_root() const
{ 
    return True; 
}

field_descriptor& storage_root::describe_components() { return NO_FIELDS; }

REGISTER_EX(storage_root, object, pessimistic_exclusive_scheme, class_descriptor::cls_non_relational);

END_GOODS_NAMESPACE
