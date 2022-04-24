// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< DATABASE.CXX >--------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 16-Sep-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Application specific database interface
//-------------------------------------------------------------------*--------*

#include "goods.h"
#ifdef _WIN32
#pragma hdrstop
#endif
#include "client.h"
#include "dbscls.h"

BEGIN_GOODS_NAMESPACE

#define DNM_BUFFER_WATERMARK 1024*1024


//
// All object storage methods are called from metaobjects
// with global mutex locked. Before locking storage critical section
// global mutex should be unlocked
//


//
// This function is called while transaction is commited 
// to assign persistent class identifier to object which is made persistent
// by this transaction. 
//
cpid_t obj_storage::get_cpid_by_descriptor(class_descriptor* desc)
{
    cpid_t cpid = cpid_table[desc->ctid]; 
    if (cpid == 0) { 
        object_monitor::unlock_global(); // wait response from server
        cpid = storage->put_class(desc->dbs_desc); 
        object_monitor::lock_global();
        cpid_table[desc->ctid] = cpid; 
        descriptor_table[cpid] = desc;
    }
    return cpid; 
}

//
// Called by smart pointers to load object instance from server 
// to client's object cache. Global mutex is locked but should be unlocked
// to make possible for other threads to continue execution.
// Before returning global lock should be reset. 
//
void obj_storage::load(hnd_t hnd, int flags)
{
  load(&hnd, 1, flags);
}

void obj_storage::query(result_set_cursor& cursor, objref_t owner, char const* table, char const* where, char const* order_by, nat4 limit, nat4 offset)
{
    critical_section guard(cs);
    assert(opened);

	storage->query(owner, table, where, order_by, limit, offset, loadBuf);

    cursor.result.clear();

    object_monitor::lock_global();

    dbs_object_header* hdr = (dbs_object_header*)&loadBuf; 
    dbs_object_header* end = (dbs_object_header*)((char*)hdr + loadBuf.size()); 
    while (hdr != end) { 
        cpid_t cpid = hdr->get_cpid(); 
        assert(cpid != 0); // object was found at server
        class_descriptor* desc = (cpid == RAW_CPID) 
            ? &storage_root::self_class : descriptor_table[cpid];
        
        if (desc == NULL) { 
            object_monitor::unlock_global();
            dnm_buffer cls_buf;
            storage->get_class(cpid, cls_buf);
            assert(cls_buf.size() != 0);
            dbs_class_descriptor* dbs_desc = 
                (dbs_class_descriptor*)&cls_buf;
            dbs_desc->unpack();
            object_monitor::lock_global();
            desc = class_descriptor::find(dbs_desc->name());
            assert(desc != NULL);
            if (*desc->dbs_desc != *dbs_desc) { 
                //
                // Class was changed
                //
#if PGSQL_ORM
				// Schema evolution is done using alter table
				object_monitor::unlock_global();
				storage->change_class(cpid, desc->dbs_desc);
				object_monitor::lock_global();
				descriptor_table[cpid] = desc;
				cpid_table[desc->ctid] = cpid;
#else 		
				object_monitor::unlock_global();
                cpid_t new_cpid = storage->put_class(desc->dbs_desc);
                object_monitor::lock_global();
                descriptor_table[new_cpid] = desc;
                cpid_table[desc->ctid] = new_cpid;
                desc = NEW class_descriptor(new_cpid, desc, dbs_desc);
                descriptor_table[cpid] = desc;
#endif
			}
			else {
                descriptor_table[cpid] = desc;
                cpid_table[desc->ctid] = cpid;
            }
		}
		objref_t id = hdr->get_ref();
        hnd_t hnd = object_handle::create_persistent_reference(this, id, 0);
		if (!IS_VALID_OBJECT(hnd->obj)) { 
			desc->unpack(hdr, hnd, lof_bulk);
		}
		cursor.result.push(anyref(hnd->obj));
		hdr = (dbs_object_header*)((char*)hdr + sizeof(dbs_object_header) + hdr->get_size());
	}

	object_monitor::unlock_global();
	loadBuf.truncate(DNM_BUFFER_WATERMARK);
}

void obj_storage::query(result_set_cursor& cursor, objref_t& first_mbr, objref_t last_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members)
{
	critical_section guard(cs);
	assert(opened);

	storage->query(first_mbr, last_mbr, query, buf_size, flags, max_members, loadBuf);

	cursor.result.clear();

	object_monitor::lock_global();

	dbs_object_header* hdr = (dbs_object_header*)&loadBuf;
	dbs_object_header* end = (dbs_object_header*)((char*)hdr + loadBuf.size());
	while (hdr != end) {
		cpid_t cpid = hdr->get_cpid();
		assert(cpid != 0); // object was found at server
		class_descriptor* desc = (cpid == RAW_CPID)
			? &storage_root::self_class : descriptor_table[cpid];

		if (desc == NULL) {
			object_monitor::unlock_global();
			dnm_buffer cls_buf;
			storage->get_class(cpid, cls_buf);
			assert(cls_buf.size() != 0);
			dbs_class_descriptor* dbs_desc =
				(dbs_class_descriptor*)&cls_buf;
			dbs_desc->unpack();
			object_monitor::lock_global();
			desc = class_descriptor::find(dbs_desc->name());
			assert(desc != NULL);
			if (*desc->dbs_desc != *dbs_desc) {
				//
				// Class was changed
				//
#if PGSQL_ORM
				// Schema evolution is done using alter table
				object_monitor::unlock_global();
				storage->change_class(cpid, desc->dbs_desc);
				object_monitor::lock_global();
				descriptor_table[cpid] = desc;
				cpid_table[desc->ctid] = cpid;
#else 		
				object_monitor::unlock_global();
				cpid_t new_cpid = storage->put_class(desc->dbs_desc);
				object_monitor::lock_global();
				descriptor_table[new_cpid] = desc;
				cpid_table[desc->ctid] = new_cpid;
				desc = NEW class_descriptor(new_cpid, desc, dbs_desc);
				descriptor_table[cpid] = desc;
#endif
			}
			else {
				descriptor_table[cpid] = desc;
				cpid_table[desc->ctid] = cpid;
			}
		}
		objref_t id = hdr->get_ref();
		hnd_t hnd = object_handle::create_persistent_reference(this, id, 0);
		if (!IS_VALID_OBJECT(hnd->obj)) {
			desc->unpack(hdr, hnd, lof_bulk);
		}
        if (!(hdr->get_flags() & tof_closure)) {             
            cursor.result.push(anyref(hnd->obj));
        }
        hdr = (dbs_object_header*)((char*)hdr + sizeof(dbs_object_header) + hdr->get_size()); 
    } 

    object_monitor::unlock_global();
    loadBuf.truncate(DNM_BUFFER_WATERMARK);
}

boolean obj_storage::convert_goods_database(char const* path, char const* name)
{
	return storage->convert_goods_database(path, name);
}

int obj_storage::execute(char const* sql)
{
	return storage->execute(sql);
}

int obj_storage::get_socket()
{
	return storage->get_socket();
}

void obj_storage::process_notifications()
{
	storage->process_notifications();
}

void obj_storage::wait_notification(event& e)
{
	storage->wait_notification(e);
}

void obj_storage::listen(hnd_t hnd, event& e)
{
	storage->listen(hnd, e);
}

void obj_storage::unlisten(hnd_t hnd, event& e)
{
	storage->unlisten(hnd, e);
}

void obj_storage::load(hnd_t *hnds, int num_hnds, int flags)
{
    int i;

    dnm_buffer opidsBuffer;
	objref_t *opids = (objref_t *)opidsBuffer.append(num_hnds * sizeof(objref_t));

    int num_opids = 0;
    for (i = 0; i < num_hnds; i++) {
        object* obj = hnds[i]->obj;
        internal_assert(hnds[i]->opid != 0);
        int reloading = IS_VALID_OBJECT(obj) 
            ? (obj->state & object_header::reloading) : 0;
        opids[i] = reloading; // hack: use opidsBuffers for storing reloading flag
    }
    object_monitor::unlock_global();
    critical_section guard(cs);
    assert(opened);

    for (i = 0; i < num_hnds; i++) {
        int reloading = (int)opids[i];        
        opids[num_opids] = hnds[i]->opid; 
        if (!reloading && (flags & lof_bulk) == 0) {
            object_monitor::lock_global();
            if (!IS_VALID_OBJECT(hnds[i]->obj)) { 
                num_opids++;
            }
            object_monitor::unlock_global();
        } else {
            num_opids++;
        }
    }
    if (num_opids == 0) {
      // 
      // Objects are already loaded by concurrent thread
      //
      object_monitor::lock_global();
      return;
    }

    if (flags & lof_auto) { 
        flags = db->fetch_flags;
    }
    storage->load(opids, num_opids, flags, loadBuf);
    internal_assert(loadBuf.size() != 0);

    object_monitor::lock_global();

    dbs_object_header* hdr = (dbs_object_header*)&loadBuf; 
    dbs_object_header* end = (dbs_object_header*)((char*)hdr + loadBuf.size()); 
    do { 
        cpid_t cpid = hdr->get_cpid(); 
        assert(cpid != 0); // object was found at server
        class_descriptor* desc = (cpid == RAW_CPID) 
            ? &storage_root::self_class : descriptor_table[cpid];
        
        if (desc == NULL) { 
            object_monitor::unlock_global();
            dnm_buffer cls_buf;
            storage->get_class(cpid, cls_buf);
            assert(cls_buf.size() != 0);
            dbs_class_descriptor* dbs_desc = 
                (dbs_class_descriptor*)&cls_buf;
            dbs_desc->unpack();
            object_monitor::lock_global();
            desc = class_descriptor::find(dbs_desc->name());
            assert(desc != NULL);
            if (*desc->dbs_desc != *dbs_desc) { 
                //
                // Class was changed
                //
#if PGSQL_ORM
				// Schema evolution is done using alter table
				object_monitor::unlock_global();
				storage->change_class(cpid, desc->dbs_desc);
				object_monitor::lock_global();
				descriptor_table[cpid] = desc;
				cpid_table[desc->ctid] = cpid;
#else 		
				object_monitor::unlock_global();
                cpid_t new_cpid = storage->put_class(desc->dbs_desc);
                object_monitor::lock_global();
                descriptor_table[new_cpid] = desc;
                cpid_table[desc->ctid] = new_cpid;
                desc = NEW class_descriptor(new_cpid, desc, dbs_desc);
				descriptor_table[cpid] = desc;
#endif
            } else { 
                descriptor_table[cpid] = desc;
                cpid_table[desc->ctid] = cpid;
            }
		}
		objref_t id = hdr->get_ref();
        hnd_t loaded_hnd = NULL;
        for (i = 0; !loaded_hnd && i < num_hnds; i++) {
            loaded_hnd = (id == hnds[i]->opid) ? hnds[i] : NULL;
        }
        if (loaded_hnd == NULL) {
            desc->unpack(hdr, object_handle::create_persistent_reference(this, id, 0), flags);
        } else if ((flags & lof_bulk) == 0) {
            desc->unpack(hdr, loaded_hnd, flags);
        }
        hdr = (dbs_object_header*)((char*)hdr + sizeof(dbs_object_header) + 
                                   hdr->get_size()); 
    } while (hdr != end); 
    loadBuf.truncate(DNM_BUFFER_WATERMARK);
}
  
//
// Start transaction.
//  
void obj_storage::begin_transaction()
{ 
    storage->begin_transaction(transBuf);
}

//
// Appened objects to transaction buffer. 
// Global mutex should be locked.
//
void obj_storage::include_object_in_transaction(hnd_t hnd, int flags)
{
    dbs_object_header* hdr; 
    object*            obj = hnd->obj;

    if (obj->cpid == 0) { 
        //
        // Objects made persistent by 'become' operator may have no assigned 
        // persistent class indentifier.
        //
        obj->cpid = hnd->storage->get_cpid_by_descriptor(&obj->cls);
    }

    if (obj->cpid == RAW_CPID) { 
        flags &= ~tof_update;
    }
    if (flags & tof_update) { 
        class_descriptor* desc = descriptor_table[obj->cpid];
        size_t size = desc->packed_size((char*)obj, obj->size) + sizeof(dbs_object_header);
        hdr = (dbs_object_header*)transBuf.append(size); 
        desc->pack(hdr, hnd); 
    } else { 
        hdr = (dbs_object_header*)transBuf.append(sizeof(dbs_object_header)); 
		hdr->set_ref(hnd->opid);
        hdr->set_cpid(obj->cpid); 
        hdr->set_size(0);
    } 
    hdr->set_flags(flags); 
}

void obj_storage::rollback_transaction()
{
	storage->rollback_transaction();
}

//
// Commit local transaction or send part of global transaction
// to coordinator. 
//
boolean obj_storage::commit_coordinator_transaction(int n_trans_servers, 
                                                    stid_t* trans_servers, 
                                                    trid_t& tid)
{
    object_monitor::unlock_global();
    if (alloc_buf_pos > 0) { 
        storage->bulk_allocate(alloc_size_buf, alloc_cpid_buf, alloc_buf_pos, 
                               alloc_opid_buf, alloc_buf_pos, alloc_near_buf); 
        alloc_buf_pos = 0;
    }
    boolean committed = 
        storage->commit_coordinator_transaction(n_trans_servers, trans_servers,
                                                transBuf, tid);
    object_monitor::lock_global();
    transBuf.truncate(DNM_BUFFER_WATERMARK);
    return committed; 
} 

//
// Send part of global transaction to server. 
//
void obj_storage::commit_transaction(stid_t  coordinator, 
                                     int     n_trans_servers,
                                     stid_t* trans_servers, 
                                     trid_t  tid)
{
    object_monitor::unlock_global();
    if (alloc_buf_pos > 0) { 
        storage->bulk_allocate(alloc_size_buf, alloc_cpid_buf, alloc_buf_pos, 
                               alloc_opid_buf, alloc_buf_pos, alloc_near_buf); 
        alloc_buf_pos = 0;
    }
    storage->commit_transaction(coordinator, 
                                n_trans_servers, trans_servers, 
                                transBuf, tid);
    object_monitor::lock_global();
    transBuf.truncate(DNM_BUFFER_WATERMARK);
} 


//
// Wait for global transation completion status from coordinator.
//
boolean obj_storage::wait_global_transaction_completion()
{
    object_monitor::unlock_global();
    boolean committed = storage->wait_global_transaction_completion();
    object_monitor::lock_global();
    return committed; 
}

static boolean try_lock(mutex& cs, dbs_storage* storage, opid_t opid, lck_t lck, int attr)
{	
	auto start_at = time(nullptr);
	for (;;)
	{
		critical_section guard(cs);
		if (storage->lock(opid, lck, lckattr_nowait))
		{
			return True;
		}

		if (attr == lckattr_nowait)
		{
			return False;
		}

		if (time(nullptr) - start_at > 20) // 20 seconds
		{
			return False;
		}
	}
}

//
// Lock function is called by meaobject protocol methods.
// Object instance and global mutex are locked at this moment.
// Global mutex should be unlocked before sending request to server
// and locked again after receiving reply.
//
boolean obj_storage::lock(objref_t opid, lck_t lck, int attr)
{
    boolean result;
    internal_assert(lck == lck_shared || lck == lck_exclusive); 
    object_monitor::unlock_global();
    assert(opened);
    {
		//result = try_lock(cs, storage, opid, lck, attr);
        critical_section guard(cs);
        result = storage->lock(opid, lck, attr);
    }
    object_monitor::lock_global();
    return result;
}

objref_t obj_storage::allocate(cpid_t cpid, size_t size, int flags, hnd_t cluster_with)
{
    int buf_size = (int)db->alloc_buf_size;

    if (buf_size != 0) { 
        assert(opened);
        if (alloc_buf_size == 0) {
            object_monitor::unlock_global();
            {
                critical_section guard(cs);
                if (alloc_buf_size == 0) { 
					alloc_opid_buf = new objref_t[buf_size];
                    alloc_size_buf = new size_t[buf_size];
                    alloc_cpid_buf = new cpid_t[buf_size];
                    alloc_near_buf = new hnd_t[buf_size];
                    alloc_buf_pos = 0;
                    storage->bulk_allocate(alloc_size_buf, alloc_cpid_buf, 0, 
                                           alloc_opid_buf, buf_size, alloc_near_buf); 
                    alloc_buf_size = buf_size;
                }
            }
            object_monitor::lock_global();
        } 
        while (alloc_buf_pos == alloc_buf_size) { 
            object_monitor::unlock_global();
            {
                critical_section guard(cs);
                if (alloc_buf_pos == alloc_buf_size) { 
                    storage->bulk_allocate(alloc_size_buf, alloc_cpid_buf, alloc_buf_size, 
                                           alloc_opid_buf, alloc_buf_size, alloc_near_buf); 
                    alloc_buf_pos = 0;
                }
            }
            object_monitor::lock_global();
        }
        if (flags & aof_aligned) {
            size |= ALLOC_ALIGNED;
        }
        alloc_size_buf[alloc_buf_pos] = size;
        alloc_cpid_buf[alloc_buf_pos] = cpid;
        alloc_near_buf[alloc_buf_pos] = cluster_with;
        return alloc_opid_buf[alloc_buf_pos++];
    }
	object_monitor::unlock_global();
	objref_t opid;
    assert(opened);
    {
        critical_section guard(cs);
        opid = storage->allocate(cpid, size, flags, cluster_with ? cluster_with->opid : 0); 
    }
    object_monitor::lock_global();
    return opid; 
}

void obj_storage::deallocate(objref_t opid)
{
    for (int n = (int)alloc_buf_pos, i = n; --i >= 0;) { 
        if (alloc_opid_buf[i] == opid) { 
            alloc_cpid_buf[i] = 0;
            alloc_size_buf[i] = 0;
            while (n != 0 && alloc_cpid_buf[n-1] == 0) {
                n -= 1;
            }
            alloc_buf_pos = n;
            return;
        }
    }
    storage->deallocate(opid);    
}


nat8 obj_storage::get_used_size()
{ 
    nat8 size;
    assert(opened);
    {
        critical_section guard(cs);
        size = storage->get_used_size(); 
    }
    return size; 
}

void obj_storage::add_user(char const* login, char const* password)
{
    assert(opened);
    {
        critical_section guard(cs);
        storage->add_user(login, password);
    }
}

void obj_storage::del_user(char const* login)
{
    assert(opened);
    {
        critical_section guard(cs);
        storage->del_user(login);
    }
}

void obj_storage::disconnected(stid_t sid)
{
    db->disconnected(sid);
    opened = False;
}

void obj_storage::login_refused(stid_t sid) 
{
    db->login_refused(sid);
    opened = False;
}

void obj_storage::send_message( int message)
{
    storage->send_message( message);
}

void obj_storage::push_message( int message)
{
    storage->push_message( message);
}

void obj_storage::send_messages()
{
    storage->send_messages();
}

void obj_storage::receive_message( int message) {
  db->receive_message( message);
}

void obj_storage::invalidate(stid_t, objref_t opid)
{
    object_monitor::lock_global(); 
    hnd_t hnd = object_handle::find(this, opid);
    if (hnd != 0) { 
        if (IS_VALID_OBJECT(hnd->obj)) { 
            hnd->obj->mop->invalidate(hnd); 
        } else { 
            hnd->obj = INVALIDATED_OBJECT; 
        }
    }
    object_monitor::unlock_global(); 
}

boolean obj_storage::open(char const* connection_address, const char* login, const char* password)
{ 
    storage = db->create_dbs_storage(id); 
    n_references = 0;
	if (!storage->open(connection_address, login, password, this)) {
        delete storage; 
        storage = NULL; 
        return False;
    }
    opened = True;
    n_references = 1;
    return True; 
}
        
void obj_storage::close()
{
    boolean was_opened;
    {
        critical_section guard(cs);
        was_opened = opened;
        if (was_opened) { 
            opened = False;
            if (storage != NULL) { 
                storage->close();
            }
        }
    }
    if (was_opened) { 
        object_monitor::lock_global();
        cache_manager::instance.cleanup_cache(this);
        if (--n_references == 0) { 
            delete this; 
        }
        object_monitor::unlock_global();
    }
}

obj_storage::obj_storage(database* dbs, stid_t sid) 
: storage(NULL), db(dbs), id(sid) 
{
    alloc_buf_size = 0;
    alloc_buf_pos  = 0;
    alloc_opid_buf = NULL;    
    alloc_size_buf = NULL;    
    alloc_cpid_buf = NULL;    
    alloc_near_buf = NULL;
}

//
// Destructor is called from close or remove_reference methods 
// with global mutex locked.
//
obj_storage::~obj_storage() 
{ 
    class_descriptor** dpp = &descriptor_table; 
    int n = (int)descriptor_table.size(); 
       
    while (--n >= 0) { 
        if (*dpp != NULL) { 
            //
            // remove class descriptors created only for loading 
            // instances of objects of modified classes 
            //
            (*dpp)->dealloc();
        }
        dpp += 1;
    }
    delete storage;
    delete[] alloc_near_buf;
    delete[] alloc_opid_buf;
    delete[] alloc_size_buf;
    delete[] alloc_cpid_buf;
} 

//
// Database (collection of storages)
//


boolean database::open(const char* database_configuration_file, const char* login, const char* password)
{
    char buf[MAX_CFG_FILE_LINE_SIZE];

    critical_section guard(cs);

    if (opened) { 
        return True;
    }

    opened = False; 

    FILE* cfg = fopen(database_configuration_file, "r");

    if (cfg == NULL) { 
        console::output("Failed to open database configuration file: '%s'\n", 
                        database_configuration_file);
        return False;
    }
    if (fgets(buf, sizeof buf, cfg) == NULL 
        || sscanf(buf, "%d", &n_storages) != 1)
    { 
        console::output("Bad format of configuration file '%s'\n",
                         database_configuration_file);
        fclose(cfg);
        return False;
    }
    storages = NEW obj_storage*[n_storages];
    memset(storages, 0, n_storages*sizeof(obj_storage*));

    opened = True;
    while (fgets(buf, sizeof buf, cfg)) { 
        int i;
        char hostname[MAX_CFG_FILE_LINE_SIZE];

        if (sscanf(buf, "%d:%s", &i, hostname) == 2) { 
            if (i < n_storages) { 
                if (storages[i] != NULL) { 
                    console::output("Duplicated entry in configuration file: "
                                    "%s", buf);
                }
                storages[i] = create_obj_storage(i);
				if (storages[i] != NULL) {
					if (!storages[i]->open("localhost:6110", login, password)) {
						fclose(cfg);
						close();
						return False;
					}
				}
				else {

					console::output("Storage is null ");
				}

				
            }
        }
    }
    fclose(cfg);
    attach();
    return True;
}

boolean database::convert_goods_database(char const* path, char const* name)
{
	return storages[0]->convert_goods_database(path, name);
}

int database::execute(char const* sql)
{
	return storages[0]->execute(sql);
}

int database::get_socket()
{
	return storages[0]->get_socket();
}

void database::process_notifications()
{
	storages[0]->process_notifications();
}

void database::wait_notification(event& e)
{
	storages[0]->wait_notification(e);
}

obj_storage* database::create_obj_storage(stid_t sid)
{
    return NEW obj_storage(this, sid);
}
   

dbs_storage* database::create_dbs_storage(stid_t sid) const
{
    return NEW dbs_client_storage(sid, storages[sid]);
}

void database::cleanup()
{
    object_handle::cleanup_object_pool();
    object_monitor::cleanup_global_cs();
}

void database::close()
{
    critical_section guard(cs);
    if (opened) { 
        for (int i = n_storages; -- i >= 0;) { 
            if (storages[i] != NULL) { 
                storages[i]->close();
            }
        }
        delete[] storages;
        opened = False; 
        detach();
    }
}

void database::get_object(object_ref& r, objref_t opid, stid_t sid)
{
    object_monitor::lock_global();
    assert(opened && sid < n_storages);
    obj_storage* storage = storages[sid];
    assert(storage != NULL);
    hnd_t hnd = 
        object_handle::create_persistent_reference(storage, opid);
    r.unlink();
    r.hnd = hnd;
    //
    // It is not necessary to load this object immediately
    // since root object can't never be removed and server
    // should not know that client has reference to this object
    //
    object_monitor::unlock_global();
}

void database::get_root(object_ref& r, stid_t sid)
{
    get_object(r, ROOT_OPID, sid);
}

void database::disconnected(stid_t sid)
{
    console::error("Server %d is disconnected\n", sid);
}

void database::login_refused(stid_t sid)
{
    //console::error("Authorization procedure fails at server %d\n", sid);
}

void database::add_user(char const* login, char const* password)
{
    for (int i = 0; i < n_storages; i++) { 
        storages[i]->add_user(login, password);
    }
}

void database::del_user(char const* login)
{
    for (int i = 0; i < n_storages; i++) { 
        storages[i]->del_user(login);
    }    
}

stid_t database::get_storage_with_minimal_size()
{
    if (n_storages != 0) { 
        stid_t min_sid = 0;
        nat8 min_size = get_used_size(0);
        for (int i = n_storages; --i > 0;) { 
            nat8 size = get_used_size(i);
            if (size < min_size) { 
                min_sid = i;
                min_size = size;
            }
        }
        return min_sid;
    }
    return (stid_t)-1;
}

void database::attach() 
{ 
    transaction_manager* mgr = transaction_manager::get_thread_transaction_manager();
    if (mgr == NULL) {
        mgr = new transaction_manager();
        mgr->attach();
    }
}

void database::detach() 
{ 
    transaction_manager* mgr = transaction_manager::get_thread_transaction_manager();
    if (mgr != NULL) {
        mgr->detach();
        delete mgr;
    }
}


database::database() 
{
    storages = NULL;
    n_storages = 0;
    opened = False; 
    fetch_flags = lof_none;
    alloc_buf_size = 0;
} 

void database::enable_clustering(boolean enabled)
{
    fetch_flags = enabled ? lof_cluster : lof_none;
}

void database::set_alloc_buffer_size(size_t size)
{
    alloc_buf_size = size;
}

void database::send_message( int message) 
{
    // Only one server need to receive the message :
    if( n_storages ) storages[0]->send_message( message);
}

void database::push_message( int message) {
    if( n_storages ) storages[0]->push_message( message);
}

void database::send_messages() {
    if( n_storages ) storages[0]->send_messages();
}

void database::receive_message( int message) {
    // to be reimplemented.
}

boolean database::start_gc()
{
    if (!opened || (storages == NULL) || (n_storages < 1)) {
        return false;
    }

    return storages[0]->storage->start_gc();
}

void database::clear_num_bytes_in_out(void)
{
    transaction_manager* mgr;

    mgr = transaction_manager::get_thread_transaction_manager();
    if (mgr != NULL)
        mgr->clear_num_bytes_in_out();

    for (stid_t sid = 0; sid < n_storages; sid++) {
        clear_num_bytes_in_out(sid);
    }
}

void database::clear_num_bytes_in_out(stid_t sid)
{
    if (sid < n_storages) {
        storages[sid]->storage->clear_num_bytes_in_out();
    }
}

nat8 database::get_num_bytes_from_db(void)
{
    nat8 total = 0;

    for (stid_t sid = 0; sid < n_storages; sid++) {
        total += get_num_bytes_from_storage(sid);
    }
    return total;
}

nat8 database::get_num_bytes_from_storage(stid_t sid)
{
    if (sid < n_storages) {
        return storages[sid]->storage->get_num_bytes_in();
    }
    return 0;
}

nat8 database::get_num_bytes_to_db(void)
{
    nat8 total = 0;

    for (stid_t sid = 0; sid < n_storages; sid++) {
        total += get_num_bytes_to_storage(sid);
    }
    return total;
}

nat8 database::get_num_bytes_to_storage(stid_t sid)
{
    if (sid < n_storages) {
        return storages[sid]->storage->get_num_bytes_out();
    }
    return 0;
}

nat8 database::get_num_bytes_from_transaction(void)
{
    transaction_manager* mgr;

    mgr = transaction_manager::get_thread_transaction_manager();
    if (mgr != NULL)
        return mgr->get_num_bytes_in();
    return 0;
}

nat8 database::get_num_bytes_to_transaction(void)
{
    transaction_manager* mgr;

    mgr = transaction_manager::get_thread_transaction_manager();
    if (mgr != NULL)
        return mgr->get_num_bytes_out();
    return 0;
}

database::~database() 
{
}    

END_GOODS_NAMESPACE
