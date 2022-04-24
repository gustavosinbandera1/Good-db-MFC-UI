// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< SERVER.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET   *     ?  *
// (Generic Object Oriented Database System)                        *   /\|  *
//                                                                  *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik * / [] \ *
//                          Last update: 14-Sep-97    K.A. Knizhnik * GARRET *
//------------------------------------------------------------------*--------*
// Database storage server
//------------------------------------------------------------------*--------*

#include "server.h"
#include "query.h"

BEGIN_GOODS_NAMESPACE

//
// Authentication manager
//

#define PASSWD_BACKUP_MAGIC 0xCafeDeadDeedFeedLL
#define RANDOM_SEED 2011
#define MAX_BACKUP_INC_FILE_LINE_SIZE 256

inline char* encrypt_password(char* encrypted_password, char const* decrypted_password) { 
    char* dst =  encrypted_password;
    char const* src =  decrypted_password;
    *dst++ = '*';
    *dst++ = '*';
    *dst++ = '*';
    nat8 key = RANDOM_SEED;
    char ch;
    int len = 0;
    while ((ch = *src++) != 0) { 
        key = (3141592621u*key + 2718281829u) % 1000000007u;
        ch ^= (char)key;
        *dst++ = 'A' + ((ch >> 4) & 0xF);
        *dst++ = 'K' + (ch & 0xF);
        len += 1;
    }
    while (len & 7) { 
        key = (3141592621u*key + 2718281829u) % 1000000007u;
        ch = key;
        *dst++ = 'A' + ((ch >> 4) & 0xF);
        *dst++ = 'K' + (ch & 0xF);
        len += 1;
    }
    *dst = '\0';
    return encrypted_password;
}

inline char* decrypt_password(char* password)
{
    nat8 key = RANDOM_SEED;
    char* dst = password;
    char* src = password + 3;
    char ch;
    do { 
        ch = ((src[0] - 'A') << 4) | ((src[1] - 'K') & 0xFF);
        key = (3141592621u*key + 2718281829u) % 1000000007u;
        ch ^= (char)key;
        *dst++ = ch;
        src += 2;
    } while (ch != 0);
    return password;
}
    
        
simple_authentication_manager::login_entry::login_entry(char const* login, char const* password, login_entry* chain, boolean  encrypted_password)
{
    this->login = strdup(login);
    this->password = strdup(password);
    this->encrypted_password = encrypted_password;
    next = chain;
}

simple_authentication_manager::login_entry::~login_entry()
{
    delete[] login;
    delete[] password;
}

simple_authentication_manager::simple_authentication_manager(char const* file)
{
    memset(hash_table, 0, sizeof(hash_table));
    file_path = strdup(file);
	//[MC] This is a virtual method. shouldn't be called from a constructor
    //reload();		
}

boolean simple_authentication_manager::restore(file& backup_file) 
{
    nat8 magic;
    fposi_t pos;
    file::iop_status rc = backup_file.get_position(pos); // save poistion in the file
    if (rc != file::ok) { 
        return False;
    }
    rc = backup_file.read(&magic, sizeof magic);
    if (rc == file::end_of_file) {  // end of backup file: old backup format without passwd file
        return True;
    }
    unpack8(magic);
    if (magic != PASSWD_BACKUP_MAGIC) {  // magic doesn't match: old backup format without passwd file
        return backup_file.set_position(pos) == file::ok; // restore position
    }
    nat4 file_size;
    rc = backup_file.read(&file_size, sizeof file_size);
    if (rc != file::ok) { 
        return False;
    }
    unpack4(file_size);
    if ((int4)file_size >= 0) { 
        FILE* f = fopen(file_path, "wb");
        if (f == NULL) { 
            return False;
        }
        char* buf = NEW char[file_size];
        rc = backup_file.read(buf, file_size);
        int size = int(fwrite(buf, 1, file_size, f));
        delete[] buf;
        if (size != file_size) { 
			fclose(f);
            return False;
        }
        fclose(f);
        reload();
    }
    return True;
}

boolean simple_authentication_manager::backup(file& backup_file) 
{
    nat8 magic = PASSWD_BACKUP_MAGIC;
    pack8(magic);
    file::iop_status rc = backup_file.write(&magic, sizeof magic);
    if (rc != file::ok) { 
        return False;
    }    
    nat4 file_size = ~0;
    if (file_exists) { 
        FILE* f = fopen(file_path, "rb");
        if (f != NULL) { 
            fseek(f, 0, SEEK_END);
            int size = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* buf = NEW char[size];
            file_size = nat4(fread(buf, 1, size, f));
            if (size != file_size) { 
                delete[] buf;
				fclose(f);
                return False;
            }
            pack4(file_size);
            rc = backup_file.write(&file_size, sizeof file_size);
            if (rc != file::ok) { 
                delete[] buf;
				fclose(f);
                return False;
            }
            rc = backup_file.write(buf, size);
            delete[] buf;
            if (rc != file::ok) { 
				fclose(f);
                return False;
            }
            fclose(f);
            return True;
        }
    }
    pack4(file_size);
    return backup_file.write(&file_size, sizeof file_size) == file::ok;
}    

void simple_authentication_manager::clean()
{
    for (int i = 0; i < LOGIN_HASH_TABLE_SIZE; i++) { 
        login_entry *next, *entry;
        for (entry = hash_table[i]; entry != NULL; entry = next) { 
            next = entry->next;
            delete entry;
        }
        hash_table[i] = NULL;
    }
}

simple_authentication_manager::~simple_authentication_manager()
{
    clean();
    delete[] file_path;
}


boolean simple_authentication_manager::authenticate(char const* login, char const* password)
{
    int h = string_hash_function(login) % LOGIN_HASH_TABLE_SIZE;
    if (strlen(login) >= MAX_LOGIN_NAME || strlen(password) >= MAX_LOGIN_NAME) { 
        return False;
    }
    if (!file_exists) { 
        return True;
    }
    for (login_entry* entry = hash_table[h]; entry != NULL; entry = entry->next) { 
        if (strcmp(login, entry->login) == 0) { 
            return strcmp(password, entry->password) == 0;
        }
    }
    return False;
}

void simple_authentication_manager::add_user(char const* login, char const* password, boolean encrypted_password)
{
    int h = string_hash_function(login) % LOGIN_HASH_TABLE_SIZE;
    for (login_entry* entry = hash_table[h]; entry != NULL; entry = entry->next) { 
        if (strcmp(login, entry->login) == 0) { 
            delete[] entry->password;
            entry->encrypted_password = encrypted_password;
            entry->password = strdup(password);
            save();
            return;
        }
    }
    hash_table[h] = NEW login_entry(login, password, hash_table[h], encrypted_password);
    FILE* f = fopen(file_path, "a");
    if (f == NULL) { 
        console::message(msg_error|msg_time, "Failed to open login file %s\n", file_path);
    } else { 
        if (encrypted_password) { 
            char encrypted_password[MAX_LOGIN_NAME*2+1];
            fprintf(f, "%s %s\n", login, encrypt_password(encrypted_password, password));
        } else { 
            fprintf(f, "%s %s\n", login, password);
        }
        file_exists = True;
        fclose(f);
    }
}

boolean simple_authentication_manager::del_user(char const* login)
{
    int h = string_hash_function(login) % LOGIN_HASH_TABLE_SIZE;
    login_entry **epp = &hash_table[h], *ep;
    while ((ep = *epp) != NULL) { 
        if (strcmp(login, ep->login) == 0) { 
            *epp = ep->next;
            delete ep;
            save();
            return True;
        }
        epp = &ep->next;
    }
    return False;
}
 
void simple_authentication_manager::save()
{
    FILE* f = fopen(file_path, "w");
    if (f == NULL) { 
        console::message(msg_error|msg_time, "Failed to open login file %s\n", file_path);
    } else { 
        for (int i = 0; i < LOGIN_HASH_TABLE_SIZE; i++) { 
            for (login_entry* entry = hash_table[i]; entry != NULL; entry = entry->next) { 
                if (entry->encrypted_password) { 
                    char encrypted_password[MAX_LOGIN_NAME*2+1];
                    fprintf(f, "%s %s\n", entry->login, encrypt_password(encrypted_password, entry->password));
                } else { 
                    fprintf(f, "%s %s\n", entry->login, entry->password);
                }
            }
        }
        file_exists = True;
        fclose(f);
    }
}

void simple_authentication_manager::reload()
{
    clean();
    FILE* f = fopen(file_path, "r");
    if (f == NULL) {  
        file_exists = False;
        console::message(msg_error|msg_time, "Failed to open login file %s\n", file_path);
    } else { 
        char login[MAX_LOGIN_NAME];
        char password[MAX_LOGIN_NAME*2+1];
        while (fscanf(f, "%s%s", login, password) == 2) { 
            int h = string_hash_function(login) % LOGIN_HASH_TABLE_SIZE;
            boolean encrypted_password = False;
            if (strncmp(password, "***", 3) == 0) { 
                decrypt_password(password);
                encrypted_password = True;
            }
            hash_table[h] = NEW login_entry(login, password, hash_table[h], encrypted_password);
        }
        file_exists = True;
        fclose(f);
    }
}

//
// Communication
//

time_t communication::ping_interval = 0;

boolean communication::handle_communication_error()
{
    cs.enter();
    if (connected) {
        msg_buf buf;
        sock->get_error_text(buf, sizeof buf);
        console::message(msg_error|msg_time,
                         "Connection with '%s' is lost: %s\n", name, buf);
        connected = False;
    }
    cs.leave();
    return False;
}

boolean communication::read(void* buf, size_t size)
{
    if (ping_interval != 0) {
        nat1* p = (nat1 *)buf;
        while (size > 0) {
            int result = sock->read(p, size, size, ping_interval);
            if ((result < 0) && !handle_communication_error()) {
                disconnect();
                return true;
            } else if (result == 0) {
                return false;
            } else if ((size_t)result <= size) {
                size -= result;
                p += result;
            }
        }
        return true;
    }

    if (!sock->read(buf, size) &&
        !handle_communication_error())
    {
        disconnect();
    }

    return true;
}

void communication::write(void const* buf, size_t size)
{
	// [MC]
	// -- boolean added to avoid deadlock that occured when disconnect was being called from inside the critical section
	bool must_disconnect = false;
	
    ((dbs_request*)buf)->pack();
    cs.enter();
    if (connected) {
        if (!sock->write(buf, size) &&
            !handle_communication_error())
        {
			must_disconnect = true;
        }
    }
    cs.leave();

	if (must_disconnect)
		disconnect();
}

void task_proc communication::receive(void* arg)
{
    ((communication*)arg)->poll();
}

void communication::connect(socket_t* connection, char* connection_name,
                            boolean fork)
{
    sock = connection;
    strcpy(name, connection_name);
    connected = True;
    receiver = fork ? create_receiver(receive, this) : task::current();
}

void communication::disconnect()
{
    cs.enter();
    if (connected) {
        console::message(msg_login|msg_time, "Disconnect '%s'\n", name);
        if (sock != NULL) {
            dbs_request req;
	    memset(&req, 0, sizeof req);
            req.cmd = dbs_request::cmd_bye;
            sock->write(&req, sizeof req);
        }
        connected = False;
    }
    if (sock != NULL) {
        sock->shutdown();
    }

    if (task::current() == receiver) {
        TRACE_MSG((msg_important, "Server agent '%s' terminated\n", name));
        delete sock;
        sock = NULL;
        if (reconnect_flag) {
            shutdown_event.signal();
        }
        receiver = NULL;
        cs.leave();
        remove();
        task::exit();
    } else {
        cs.leave();
    }
}

communication::~communication()
{
    delete sock;
}

//
// Client agent
//


void client_agent::invalidate(opid_t opid)
{
    dbs_request req;
    req.object.cmd = dbs_request::cmd_invalidate;
    req.object.opid = opid;
    req.object.extra = 0;
    TRACE_MSG((msg_object,
               "Push invalidation signal for object %x to client '%s'\n",
               opid, name));
    notify_cs.enter();
    notify_buf.push(req);
    notify_cs.leave();
}

void client_agent::notify()
{
    notify_cs.enter();
    int n = int(notify_buf.size());
    if (n > 0) {
        dbs_request* req = &notify_buf;
        TRACE_MSG((msg_object,
                   "Send invalidation signals for to client '%s': %x",
                   name, req->object.opid));
        req->object.extra = n - 1;
        while (--n > 0) {
            req += 1;
            TRACE_MSG((msg_object, ",%x", req->object.opid));
            req->pack();
        }
        communication::write(&notify_buf,
                             notify_buf.size()*sizeof(dbs_request));
        notify_buf.change_size(0);
        TRACE_MSG((msg_object, "\n"));
    }
    notify_cs.leave();
}

void client_agent::send_messages( dnm_array<dbs_request>& msg)
{
    notify_cs.enter();
    dbs_request *rqst = &msg;
	communication::write( rqst, msg.size()*sizeof(dbs_request));
    rqst->unpack();
    notify_cs.leave();
}

void client_agent::write(void const* buf, size_t size)
{
    //
    // Make sure that all invalidations messages generated during
    // handling of client request will be send to the client before
    // response to the request
    //
    notify();
    communication::write(buf, size);
}

#define MAX_FAILED_ATTEMPTS 1

void client_agent::preload_set(dbs_request const& req)
{
    dbs_handle hnd;
    opid_t owner_opid = req.object.opid;
    server->obj_mgr->load_object(owner_opid, req.object.flags, this);
    server->mem_mgr->get_handle(owner_opid, hnd);
    size_t size = hnd.get_size();
    cpid_t cpid = hnd.get_cpid();
    assert(size != 0 && cpid != 0);

    stid_t self_sid = server->id;
    dbs_object_header *hpd = (dbs_object_header*)buf.append(sizeof(dbs_object_header)+size);
    hpd->set_opid( owner_opid);
    hpd->set_size(size);
    hpd->set_cpid(cpid);
    
    int n_refs =  server->class_mgr->get_number_of_references(cpid, size);
    assert(n_refs >= 3); 
        
    server->pool_mgr->read(hnd.get_pos(), hpd->body(), size);
    server->obj_mgr->release_object(owner_opid);

	objref_t next_opid;
    stid_t next_sid;
    unpackref(next_sid, next_opid, hpd->body());
    
    while (true) { 
        if (next_sid != self_sid || next_opid == 0) { 
            break;
        }
        server->obj_mgr->load_object(next_opid, lof_none, this);
        server->mem_mgr->get_handle(next_opid, hnd);
        size = hnd.get_size();
        cpid = hnd.get_cpid();
        assert(size != 0 && cpid != 0);

        hpd = (dbs_object_header*)buf.append(sizeof(dbs_object_header)+size);
        hpd->set_opid(next_opid);
        hpd->set_size(size);
        hpd->set_cpid(cpid);
    
        n_refs = server->class_mgr->get_number_of_references(cpid, size);
        assert(n_refs >= 4); 
        
        server->pool_mgr->read(hnd.get_pos(), hpd->body(), size);
        server->obj_mgr->release_object(next_opid);

        char* p = hpd->body();
        p = unpackref(next_sid, next_opid, p);
        p += OBJECT_REF_SIZE; // prev
        p += OBJECT_REF_SIZE; // owner
        
		objref_t obj_opid;
        stid_t obj_sid;
        p = unpackref(obj_sid, obj_opid, p); // obj

        if (obj_sid == self_sid) { 
            server->obj_mgr->load_object(obj_opid, lof_none, this);
            server->mem_mgr->get_handle(obj_opid, hnd);
            size = hnd.get_size();
            cpid = hnd.get_cpid();
            hpd = (dbs_object_header*)buf.append(sizeof(dbs_object_header)+size);
            hpd->set_opid(obj_opid);
            hpd->set_size(size);
            hpd->set_cpid(cpid);
            if (cpid != obj_opid && server->class_mgr->is_external_blob(cpid)) { 
                server->read_external_blob(obj_opid, hpd->body(), size);
            } else if (size != 0) {
                server->pool_mgr->read(hnd.get_pos(), hpd->body(), size);
            }
            server->obj_mgr->release_object(obj_opid);
        }
    }
    dbs_request* reply = (dbs_request*)&buf;
    reply->result.cmd = dbs_request::cmd_object;
    reply->result.size = nat4(buf.size() - sizeof(dbs_request));
	write(reply, buf.size());
}


void client_agent::execute_query(dbs_request const& req)
{
    Runtime runtime(server, this, req.query.flags);
	Query::Expression* condition = NULL;
	opid_t end_member_opid = 0;
	read(&end_member_opid, sizeof end_member_opid);
    if (req.query.query_len != 0) { 
        char* query = (char*)buf.put(req.query.query_len + 1);        
        read(query, req.query.query_len);
        query[req.query.query_len] = '\0';
	
		//[MC] do not compile empty query
		if(*query != '\0')
		{
			Query::Compiler compiler(&runtime, query);
			try { 
				condition = compiler.compile();
			} 
			catch (QueryServerException const& x) { 
				char resp_buf[sizeof(dbs_request) + MAX_ERROR_MESSAGE_LENGTH];
				dbs_request* reply = (dbs_request*)resp_buf;        
				char* msg = (char*)(reply + 1);
				size_t msg_size = x.getMessage(msg);
				TRACE_MSG((msg_important, "Bad query \"%s\": %s\n", query, msg));
				reply->result.cmd = dbs_request::cmd_query_result;
				reply->result.status = False;
				reply->result.size = (nat4)msg_size + 1; // include terminator character
				reply->result.tid = 0;
				write(reply, sizeof(dbs_request) + msg_size + 1);
				return;
			}
		}
    }
    dnm_buffer mbr_buf;
    dbs_handle hnd;
	objref_t member_opid = req.query.member_opid;
    size_t max_buf_size = req.query.max_buf_size;
    size_t max_members = req.query.max_members;
    size_t next_field_offs = (req.query.flags & qf_backward) ? OBJECT_REF_SIZE : 0;
    bool closure = (req.query.flags & qf_closure) != 0;
    bool include_members = (req.query.flags & qf_include_members) != 0;

	//[MC] set the buffer size if valid
	buf.put(max_buf_size != UINT_MAX ? max_buf_size : 0); // reserve enough space in buffer
    buf.put(sizeof(dbs_request));
    
	//[MC] we never pass the set_owner opid, we just pass the first member to be processed in the query
    size_t size = 0;
    cpid_t cpid = 0;      
    stid_t self_sid = server->id;    
    int n_refs = 0;    
    stid_t next_sid = 0;
	objref_t next_opid = member_opid;
    
    for (size_t i = 0; buf.size() < max_buf_size && i < max_members; i++) { 
        if (next_sid != self_sid || next_opid == 0) {
            next_opid = 0; // no more members. [MC] we use next_opid for next iteration
            break;
        }
        member_opid = next_opid;
        server->obj_mgr->load_object(next_opid, lof_none, this);
        server->mem_mgr->get_handle(next_opid, hnd);
        size = hnd.get_size();
        cpid = hnd.get_cpid();
        assert(size != 0 && cpid != 0);
        
        mbr_buf.put(size);

        n_refs = server->class_mgr->get_number_of_references(cpid, size);
        assert(n_refs >= 4); 
        
        server->pool_mgr->read(hnd.get_pos(), &mbr_buf, size);
        server->obj_mgr->release_object(next_opid);
        
        char* p = &mbr_buf;

		//[MC] 
		// -- used to rollback the buffer by the size of the member if the query condition is not matched
		size_t member_size_in_buffer = 0;

        if (include_members) { 
			member_size_in_buffer = sizeof(dbs_object_header) + size;
			dbs_object_header* hpd = (dbs_object_header*)buf.append(member_size_in_buffer);
            hpd->set_flags(tof_none);
            hpd->set_opid(next_opid);
            hpd->set_size(size);
            hpd->set_cpid(cpid);
            memcpy(hpd->body(), p, size);
        }

        p = unpackref(next_sid, next_opid, p + next_field_offs);
        p += OBJECT_REF_SIZE - next_field_offs; // prev
        p += OBJECT_REF_SIZE; // owner
        
		objref_t obj_opid;
        stid_t obj_sid;
        unpackref(obj_sid, obj_opid, p); // obj
        
        if (obj_sid == self_sid) { 
            server->mem_mgr->get_handle(obj_opid, hnd);                
            server->obj_mgr->load_object(obj_opid, lof_none, this);
            size = hnd.get_size();
            cpid = hnd.get_cpid();
            dbs_object_header* hpd = (dbs_object_header*)buf.append(sizeof(dbs_object_header)+size);
            hpd->set_flags(include_members ? tof_closure : tof_none);
            hpd->set_opid(obj_opid);
            hpd->set_size(size);
            hpd->set_cpid(cpid);
            server->pool_mgr->read(hnd.get_pos(), hpd->body(), size);
            server->obj_mgr->release_object(obj_opid);
            if (condition != NULL) { 
                runtime.mark();
                runtime.unpackObject(hpd->body(), size, cpid);
                bool matched = false;
                try { 
                    matched = condition->evaluateCond(&runtime);
                } catch (QueryServerException const&) {}
                if (!matched) { 
                    buf.cut(sizeof(dbs_object_header)+size); // remove object from buffer
					//[MC]
					buf.cut(member_size_in_buffer); // remove member from buffer
                } else {
                    if (closure) { 
                        for (Runtime::RefObject* ref = runtime.getReferencedObjects(); ref != NULL; ref = ref->next) { 
                            dbs_object_header* hpd = (dbs_object_header*)buf.append(sizeof(dbs_object_header)+ref->size);
                            hpd->set_flags(tof_closure);
                            hpd->set_opid(ref->opid);
                            hpd->set_size(ref->size);
                            hpd->set_cpid(ref->cpid);
                            memcpy(hpd->body(), ref->body, ref->size);
                        }
                    }
                }
                runtime.reset();
            }
        }

		//exit loop if this is the ending member which must be processed
        if (member_opid == end_member_opid) {
            next_opid = 0; 
            break;
        }
    }
    dbs_request* reply = (dbs_request*)&buf;
    reply->result.cmd = dbs_request::cmd_query_result;
    reply->result.size = nat4(buf.size() - sizeof(dbs_request));
    reply->result.tid = next_opid; 	//[MC] return the next member to be processed
    reply->result.status = True;
	write(reply, buf.size());
}

void client_agent::send_object(dbs_request const& req)
{
    dbs_handle hnd;
	opid_t opid;
    int n, n_objects = req.object.extra + 1;
    dbs_request *reqp;
    dbs_object_header *hps, *hpd;
    dnm_array<dbs_request> req_buf;

    buf.put(cluster_size); // reserve enough space in buffer
    buf.put(sizeof(dbs_request));

    if (n_objects > 1) {
        req_buf.change_size(n_objects);
        reqp = &req_buf;
        read(reqp+1, (n_objects-1)*sizeof(dbs_request));
        for (n = 1; n < n_objects; n++) {
            reqp[n].unpack();
            if (reqp[n].cmd != dbs_request::cmd_load) {
                console::message(msg_error|msg_time,
                                 "Invalid load request %d from client '%s'\n",
                                 reqp[n].cmd, name);
                n_objects = n;
            }
        }
        *reqp = req;
    } else {
        if (req.object.flags & lof_bulk) {
            preload_set(req);
            return;
        } 
        reqp = (dbs_request*)&req;
    }

    for (n = 0; n < n_objects; n++) {
        opid = reqp[n].object.opid;

        server->obj_mgr->load_object(opid, reqp[n].object.flags, this);
        server->mem_mgr->get_handle(opid, hnd);
        size_t size = hnd.get_size();
        cpid_t cpid = hnd.get_cpid();

        hpd = (dbs_object_header*)buf.append(sizeof(dbs_object_header)+size);
        hpd->set_opid(opid);
        hpd->set_size(size);
        hpd->set_cpid(cpid);

        if (opid != cpid && server->class_mgr->is_external_blob(cpid)) { 
            server->read_external_blob(opid, hpd->body(), size);
        } else if (size != 0) {
            server->pool_mgr->read(hnd.get_pos(), hpd->body(), size);
        }
        server->obj_mgr->release_object(opid);
        if (cpid == 0) {
            console::message(msg_error|msg_time,
                             "Client '%s' request unexisted object %x\n",
                             name, opid);
            server->obj_mgr->forget_object(opid, this);
        }
    }
    size_t msg_size = buf.size();
    hps = (dbs_object_header*)(&buf + sizeof(dbs_request));
    hpd = (dbs_object_header*)(&buf + msg_size);
    int n_failed_attempts = 0;
    int n_requested_objects = n_objects;
    for (n = 0; n < n_objects && msg_size < cluster_size; n++) {
        if (n >= n_requested_objects || (reqp[n].object.flags & lof_cluster) != 0) {
            cpid_t cpid = hps->get_cpid();
            size_t size = hps->get_size();
            if (size == 0) {
                hps = (dbs_object_header*)((char*)hps + sizeof(dbs_object_header));
                continue;
            }
            int n_refs = server->class_mgr->get_number_of_references(cpid, size);
            internal_assert(n_refs >= 0);
            char* p = hps->body();

            while (--n_refs >= 0) {
				objref_t co_opid;
                stid_t co_sid;
                p = unpackref(co_sid, co_opid, p);
                if (co_opid != 0 && co_sid == server->id &&
                    server->obj_mgr->get_object_state(co_opid, this) == cis_none)
                {
                    server->obj_mgr->load_object(co_opid, lof_none, this);
                    server->mem_mgr->get_handle(co_opid, hnd);
                    size_t co_size = hnd.get_size();
                    if (co_size != 0 && msg_size + co_size
                        + sizeof(dbs_object_header) <= cluster_size)
                    {
                        cpid_t co_cpid = hnd.get_cpid();
                        hpd->set_opid(co_opid);
                        hpd->set_size(co_size);
                        hpd->set_cpid(co_cpid);
                        if (co_opid != co_cpid && server->class_mgr->is_external_blob(co_cpid)) { 
                            server->read_external_blob(co_opid, hpd->body(), co_size);
                        } else { 
                            server->pool_mgr->read(hnd.get_pos(), hpd->body(), co_size);
                        }
                        hpd = (dbs_object_header*)(hpd->body() + co_size);
                        msg_size += co_size + sizeof(dbs_object_header);
                        server->obj_mgr->release_object(co_opid);
                        n_objects += 1;
                    } else {
                        server->obj_mgr->forget_object(co_opid, this);
                        server->obj_mgr->release_object(co_opid);
                        //
                        // Continue construction of closure only if we have
                        // enough space in the buffer
                        //
                        if (co_size != 0
                            && ++n_failed_attempts >= MAX_FAILED_ATTEMPTS)
                        {
                            n = n_objects;
                            break;
                        }
                    }
                }
            }
        }
    }
    dbs_request* reply = (dbs_request*)&buf;
    reply->result.cmd = dbs_request::cmd_object;
    reply->result.size = nat4(msg_size - sizeof(dbs_request));
	write(reply, msg_size);
}

void client_agent::disconnect()
{
    terminated = True;
    lock_sem.signal();
    communication::disconnect();
}

void client_agent::remove()
{
    server->obj_mgr->disconnect_client(this);
    server->mem_mgr->disconnect_client(this);
    server->remove_client_agent(this);
}

void client_agent::handle_notifications(int n_requests)
{
    if (n_requests > 0) {
        dnm_array<dbs_request> buf;
        buf.change_size(n_requests);
        dbs_request* rp = &buf;
        read(rp, n_requests*sizeof(dbs_request));
        while (--n_requests >= 0) {
            rp->unpack();
            switch (rp->cmd) {
              case dbs_request::cmd_forget:
                server->obj_mgr->forget_object(rp->object.opid, this);
                break;

              case dbs_request::cmd_throw:
                server->obj_mgr->throw_object(rp->object.opid, this);
                break;

              default:
                console::message(msg_error|msg_time,
                                 "Invalid notification %d from client '%s'\n",
                                 rp->cmd, name);
            }
            rp += 1;
        }
    }
}

void client_agent::poll()
{
    dbs_transaction_header* trans;
    dbs_class_descriptor* desc;
    dbs_request req, *reqp;
    lck_t       lck;
    
    while(connected) {
        if (!read(&req, sizeof req)) {
            req.cmd = dbs_request::cmd_ping;
            write(&req, sizeof(req));
            continue;
        }
        req.unpack();
        TRACE_MSG((msg_request, "Receive request %d from client '%s'\n",
                   req.cmd, name));
        req_count += 1;
	boolean ack_needed = False;
        switch (req.cmd) {
 	  case 	dbs_request::cmd_user_msg:
 	    send_msg_cs.enter();
 	    send_msg_buf.change_size( 0);
 	    send_msg_buf.push( req);
 	    if( req.object.extra > 0 ) {
 	      send_msg_buf.change_size( req.object.extra + 1);
 	      dbs_request *rp = &send_msg_buf;
 	      rp += 1;
 	      read( rp, req.object.extra*sizeof(dbs_request));
 	    }
 	    server->send_messages( this, send_msg_buf);
 	    send_msg_cs.leave();
 	    break;
          case dbs_request::cmd_load:
            send_object(req);
            break;

          case dbs_request::cmd_getclass:
            desc = server->class_mgr->get_and_lock_class(req.clsdesc.cpid,
                                                         this);
            if (desc != NULL) {
                size_t len = desc->get_size();
                reqp = (dbs_request*)buf.put(sizeof(dbs_request) + len);
                reqp->result.cmd = dbs_request::cmd_classdesc;
                reqp->result.size = nat4(len);
                dbs_class_descriptor* dst_desc =
                    (dbs_class_descriptor*)(reqp+1);
                memcpy(dst_desc, desc, len);
                server->class_mgr->unlock_class(req.clsdesc.cpid);
				//[MC] Changed output from msg_important to msg_none
                TRACE_MSG((msg_none, "Send class '%s' to client\n",
                           dst_desc->name()));
                dst_desc->pack();
                write(reqp, sizeof(dbs_request)+len);
            } else {
                req.result.cmd = dbs_request::cmd_classdesc;
                req.result.size = 0;
                console::message(msg_error|msg_time,
                                 "Class with descriptor %x is not found\n",
                                  req.clsdesc.cpid);
                write(&req, sizeof req);
            }
            break;

          case dbs_request::cmd_query:
            execute_query(req);
            break;

          case dbs_request::cmd_putclass:
            desc = (dbs_class_descriptor*)buf.put(req.clsdesc.size);
            read(desc, req.clsdesc.size);
            desc->unpack();
            req.clsdesc.cmd = dbs_request::cmd_classid;
            req.clsdesc.cpid = server->class_mgr->put_class(desc, this);
            write(&req, sizeof req);
            break;

          case dbs_request::cmd_modclass_ack:
	    ack_needed = True;
	    // no break
          case dbs_request::cmd_modclass:
            desc = (dbs_class_descriptor*)buf.put(req.clsdesc.size);
            desc->unpack();
            read(desc, req.clsdesc.size);
            server->class_mgr->modify_class(req.clsdesc.cpid, desc, this);
            break;

          case dbs_request::cmd_lock_ack:
	    ack_needed = True;
	    // no break
          case dbs_request::cmd_lock:
            req.result.status =
                server->obj_mgr->lock_object(req.lock.opid,
                                             lck_t(req.lock.type),
                                             req.lock.attr,
					     ack_needed, 
                                             lck, this);
	    ack_needed = False;
            req.result.cmd = dbs_request::cmd_lockresult;
            write(&req, sizeof req);
            break;

          case dbs_request::cmd_unlock_ack:
	    ack_needed = True;
	    // no break
          case dbs_request::cmd_unlock:
            server->obj_mgr->unlock_object(req.lock.opid,
                                           lck_t(req.lock.type), this);
            break;

          case dbs_request::cmd_transaction:
            trans = (dbs_transaction_header*)
                buf.put(sizeof(dbs_transaction_header) + req.trans.size);
            read(trans->body(), req.trans.size);
            req.cmd = dbs_request::cmd_transresult;
            if (calculate_crc(trans->body(), req.trans.size) != req.trans.crc) {
                req.result.status = False;
                server->message(msg_error|msg_time, "CRC check failed for transaction from client %s\n", name);
            } else { 
                trans->coordinator = server->id;
                trans->size = req.trans.size;
                req.result.status =
                    server->trans_mgr->do_transaction(req.trans.n_servers, trans, this);
            }
            write(&req, sizeof req);
            break;

          case dbs_request::cmd_subtransact:
            trans = (dbs_transaction_header*)
                buf.put(sizeof(dbs_transaction_header) + req.trans.size);
            read(trans->body(), req.trans.size);
            req.cmd = dbs_request::cmd_transresult;
            if (calculate_crc(trans->body(), req.trans.size) != req.trans.crc) {
                req.result.status = False;
                server->message(msg_error|msg_time, "CRC check failed for transaction from client %s\n", name);
            } else { 
                trans->coordinator = req.trans.coordinator;
                trans->size = req.trans.size;
                trans->tid = req.trans.tid;
                req.result.status =
                    server->trans_mgr->do_transaction(req.trans.n_servers,trans,this);
            }
            write(&req, sizeof req);
            break;

          case dbs_request::cmd_alloc:
            req.object.opid = server->mem_mgr->alloc(req.alloc.cpid,
                                                     req.alloc.size,
                                                     req.alloc.flags,
                                                     req.alloc.cluster,
                                                     this);
            if (req.object.opid == 0) {
                TRACE_MSG((msg_object|msg_important|msg_warning,
                           "Failed to allocate object of size %u\n",
                           req.alloc.size));
                disconnect();
            }
            TRACE_MSG((msg_object, "Allocate object %x\n", req.object.opid));
            req.cmd = dbs_request::cmd_location;
            write(&req, sizeof req);
            break;

          case dbs_request::cmd_bulk_alloc:
            {
                size_t i, n = req.bulk_alloc.n_objects;
                size_t elem_size = (req.bulk_alloc.flags & aof_clustered) ? 10 : 6;

                buf.put((n*elem_size < req.bulk_alloc.reserved*4 ? req.bulk_alloc.reserved*4 : n*elem_size) + sizeof(dbs_request));
                char* p = &buf;
                if (n != 0) {
                    read(p, n*elem_size);
                }
                nat4* p4 = (nat4*)p;
                for (i = 0; i < n; i++) {
                    unpack4(*p4++);
                }
                if (req.bulk_alloc.flags & aof_clustered) { 
                    for (i = 0; i < n; i++) {
                        unpack4(*p4++);
                    }
                }
                nat2* p2 = (nat2*)p4;
                for (i = 0; i < n; i++) {
                    unpack2(*p2++);
                }
                server->mem_mgr->bulk_alloc((nat4*)p, (nat2*)p4, n, (nat4*)(p + sizeof(dbs_request)),
                                            req.bulk_alloc.reserved, p4 - n, 
                                            req.bulk_alloc.flags, this);
                p4 = (nat4*)(p + sizeof(dbs_request));
                dbs_request* response = (dbs_request*)p;
                response->cmd = dbs_request::cmd_opids;
                n = req.bulk_alloc.reserved;
                response->result.size = nat4(n*4);
                for (i = 0; i < n; i++) {
                    pack4(*p4++);
                }
                write(response, sizeof(dbs_request) + n*4);
                break;
            }
          case dbs_request::cmd_free_ack:
	    ack_needed = True;
	    // no break
          case dbs_request::cmd_free:
            server->mem_mgr->dealloc(req.object.opid);
            server->obj_mgr->forget_object(req.object.opid, this);
            break;

          case dbs_request::cmd_forget_ack:
	    ack_needed = True;
	    // no break
          case dbs_request::cmd_forget:
            server->obj_mgr->forget_object(req.object.opid, this);
            handle_notifications(req.object.extra);
            break;

          case dbs_request::cmd_throw_ack:
	    ack_needed = True;
	    // no break
          case dbs_request::cmd_throw:
            server->obj_mgr->throw_object(req.object.opid, this);
            handle_notifications(req.object.extra);
            break;

          case dbs_request::cmd_logout:
            console::message(msg_login|msg_time,
                             "Client '%s' send logout request\n", name);
            disconnect();
            break;

          case dbs_request::cmd_add_user:
          {
              char* login = NEW char[req.any.arg3];
              read(login, req.any.arg3);
              char* password = NEW char[req.any.arg4];
              read(password, req.any.arg4);
              server->add_user(login, password, true);
              delete[] login;
              delete[] password;
              break;
          }
          case dbs_request::cmd_del_user:
          {
              char* login = NEW char[req.any.arg3];
              read(login, req.any.arg3);
              server->del_user(login);
              delete[] login;
              break;
          }


          case dbs_request::cmd_get_size:
            {
                fsize_t used_size = server->mem_mgr->get_used_size();
                req.cmd = dbs_request::cmd_location;
                req.any.arg3 = nat8_high_part(used_size);
                req.any.arg4 = nat8_low_part(used_size);
                write(&req, sizeof req);
                break;
            }

          case dbs_request::cmd_start_gc:
            console::message(msg_notify|msg_time, "Client %s send garbage "
                             "collection request\n", name);
            server->mem_mgr->start_gc();
            req.result.status = true;
            req.result.cmd = dbs_request::cmd_gc_started;
            write(&req, sizeof req);
            break;

          case dbs_request::cmd_ping_ack:
            console::message(msg_notify|msg_time,
                             "ping acknowledged by '%s'\n", name);
            break;

          default:
			{
				//[MC] Allow handling customized messages
				if (!pollex(req))
					console::message(msg_error|msg_time,
								 "Invalid request %d from client '%s'\n",
								 req.cmd, name);
			}
        }
	if (ack_needed) { 
	    req.cmd = dbs_request::cmd_ack;
	    write(&req, sizeof req);
	}
    }
    disconnect();
}

void dbs_server::message(int message_class_mask, const char* msg, ...) const 
{
    va_list args;
    va_start (args, msg);
    if (sc != NULL) { 
        sc->message(get_name(), id, message_class_mask, msg, args);
    } else { 
        console::vmessage(message_class_mask, msg, args);
    }
    va_end (args);
}

void dbs_server::output(const char* msg, ...) const 
{
    va_list args;
    va_start (args, msg);
    if (sc != NULL)
        sc->out_data(msg, args);
    else
        console::active_console->out_data(msg, args);
        va_end (args);
}

char* client_agent::get_name() { return name; }

//
// Remote server agent
//

void server_agent::connect(socket_t* connection, char* name, boolean fork)
{
    //
    // connect() is called with fork == False from handshake() method
    // and with fork == True from open() and write() methods.
    // In write() mutex was already locked and it is not neccessary to lock
    // mutex when open() is executed. Nested lock should be avoided otherwise
    // shutdown_event.wait() will not unlock mutex.
    //
    if (!fork) cs.enter();
    SERVER_TRACE_MSG((msg_important, 
               "Connect '%s', fork = %d, connected = %d, receiver = %p\n",
               name, fork, connected, receiver));
    if (receiver != NULL) {
        reconnect_flag = True;
        shutdown_event.reset();
        sock->shutdown();
        shutdown_event.wait();
        reconnect_flag = False;
    }
    server->remote_server_connected(id);
    communication::connect(connection, name, fork);
    if (!fork) cs.leave();
}

void server_agent::write(void const* buf, size_t size)
{
    cs.enter();
    if (!connected) {
        if (id < server->id) {
            if (reconnect_flag) {
                while (reconnect_flag) {
                    shutdown_event.wait();
                }
                if (connected) {
                    communication::write(buf, size);
                }
            } else {
                socket_t* s =
                    socket_t::connect(name,socket_t::sock_any_domain, 1, 0);
                if (s->is_ok()) {
                    server->message(msg_notify|msg_login|msg_time, 
                                     "Reestablish connection with server "
                                     "'%s'\n", name);
                    if (handshake(s, server->id, server->get_name())) {
                        connect(s, name, True);
                        communication::write(buf, size);
                    } else {
                        delete s;
                    }
                } else {
                    delete s;
                }
            }
        }
    } else {
        communication::write(buf, size);
    }
    cs.leave();
}

void server_agent::remove()
{
    server->remove_server_agent(this);
}

void server_agent::poll()
{
    dbs_request req;

    while (connected) {
        if (!read(&req, sizeof req)) {
            req.cmd = dbs_request::cmd_ping;
            write(&req, sizeof(req));
            continue;
        }
        req.unpack();
        SERVER_TRACE_MSG((msg_request, "Receive request %d from server '%s'\n", 
                   req.cmd, name));
        switch (req.cmd) {
          case dbs_request::cmd_tmsync:
            server->trans_mgr->tm_sync(id, req);
            break;

          case dbs_request::cmd_gcsync:
            server->mem_mgr->gc_sync(id, req);
            break;

          case dbs_request::cmd_bye:
            console::message(msg_login|msg_notify|msg_time,
                             "Server '%s' send logout request\n", name);
            connected = False;
            disconnect();
            break;

          case dbs_request::cmd_ping:
            req.cmd = dbs_request::cmd_ping_ack;
            console::message(msg_notify|msg_time,
                "ping received from '%s'\n", name);
            write(&req, sizeof req);
            break;

          case dbs_request::cmd_ping_ack:
            console::message(msg_notify|msg_time,
                             "ping acknowledged by '%s'\n", name);
            break;

          default:
            console::message(msg_error|msg_time,
                             "Illegal request %d from server '%s'\n",
                             req.cmd, name);
        }
    }
    disconnect();
}

void server_agent::send(dbs_request* req, size_t body_len)
{
    write(req, sizeof(dbs_request) + body_len);
}

void server_agent::read_msg_body(void* buf, size_t size)
{
    read(buf, size);
}

boolean server_agent::handshake(socket_t* s, stid_t sid, char const* my_name)
{
    int len = int(strlen(my_name));
    assert(len < MAX_LOGIN_NAME);
    struct {
        dbs_request hdr;
        char        name[MAX_LOGIN_NAME];
    } snd_req;
    dbs_request rcv_req;

    snd_req.hdr.cmd = dbs_request::cmd_connect;
    snd_req.hdr.login.sid = sid;
    snd_req.hdr.login.name_len = len;
    memcpy(snd_req.name, my_name, len);
    snd_req.hdr.pack();
    if (s->write(&snd_req, sizeof(dbs_request) + len) &&
        s->read(&rcv_req, sizeof rcv_req))
    {
        if (rcv_req.cmd == dbs_request::cmd_ok) {
            return True;
        }
        server->message(msg_login|msg_error|msg_time, 
                         "Login is refused by server %d\n", id);
    } else {
        msg_buf buf;
        s->get_error_text(buf, sizeof buf);
        server->message(msg_login|msg_error|msg_time, 
                         "Handshake with server %d failed: %s\n", id, buf);
    }
    return False;
}

//
// Local storage server
//


void task_proc storage_server::start_handshake(void* arg)
{
    login_data* login = (login_data*)arg;
    login->server->handshake(login);
}


void storage_server::handshake(login_data* login)
{
	const auto msg_flag = console_message_classes::msg_important | msg_time;
    dbs_request req, reply;

	TRACE_MSG((msg_flag, "handshake: reading request from socket\n"));
	if (login->sock->read(&req, sizeof req)) {
        req.unpack();
	reply = req;
		if (req.cmd == dbs_request::cmd_synchronize) {
            trans_mgr->start_replication(login->sock, 
                                         cons_nat8(req.sync.log_offs_high, req.sync.log_offs_low));
            login->sock = NULL; // prevent deletion of replication socket
		} else if (req.cmd != dbs_request::cmd_login
            && req.cmd != dbs_request::cmd_connect)
        {
            console::message(msg_error|msg_time,
                             "Bad request received while handshake: %d\n",
                             req.cmd);
            reply.cmd = dbs_request::cmd_bye;
            login->sock->write(&reply, sizeof reply);
		} else if (req.login.name_len >= MAX_LOGIN_NAME) {
            console::message(msg_error|msg_time, "Login name too long: %d",
                             req.login.name_len);
            reply.cmd = dbs_request::cmd_bye;
            login->sock->write(&reply, sizeof reply);
		} else {

			TRACE_MSG((msg_flag, "handshake: reading login name from socket\n"));

            char name[MAX_LOGIN_NAME];
			if (login->sock->read(name, req.login.name_len)) {
                name[req.login.name_len] = '\0';
				if (req.cmd == dbs_request::cmd_login && !authenticate(req, name, login->is_remote_connection)) {
                    console::message(msg_login|msg_error|msg_time,
                                     "Authorization of %s '%s' failed\n",
                                     (req.cmd == dbs_request::cmd_connect)
                                     ? "server" : "client", name);
                    reply.cmd = dbs_request::cmd_refused;
		    reply.any.arg3 = login_refused_code;
		    reply.pack();
                    login->sock->write(&reply, sizeof reply);
				} else {
					TRACE_MSG((msg_flag, "handshake: sending server version response to client\n"));

                    reply.cmd = dbs_request::cmd_ok;
					//[MC] Return server version with login reply
					reply.any.arg5 = MAKELONG(nat4(((GOODS_VERSION) - nat4(GOODS_VERSION)) * 100), nat4(GOODS_VERSION));
					reply.pack();
                    login->sock->write(&reply, sizeof reply);
					if (req.cmd == dbs_request::cmd_connect) {
						if (req.login.sid == id) {
                            console::message(msg_error|msg_time,
                                             "Attempt to start duplicated "
                                             "server %d\n", id);
						} else if (req.login.sid >= n_servers) {
                            console::message(msg_error|msg_time,
                                             "Server %d is not in "
                                             "configuration file\n",
                                             req.login.sid);
						} else {
                            console::message(msg_login|msg_time,
                                             "Establish connection with"
                                             " server %d: \"%s\"\n",
                                             req.login.sid, name);
                            //
                            // Server agent "connect" method will wait until
                            // previous connection is terminated. So only
                            // one task per server_agent exists at each moment
                            // of time.
                            //
                            servers[req.login.sid]->connect(login->sock, name);
                            cs.enter();
                            login->sock = NULL; // do not delete
                            delete login;
							if (!opened && handshake_list.empty()) {
                                term_event.signal();
                            }
                            cs.leave();
                            servers[req.login.sid]->poll();
                            return;
                        }
					} else {
						TRACE_MSG((msg_flag, "handshake: create client agent\n"));

                        client_agent* client = create_client_agent();
                        console::message(msg_login|msg_time,
                                         "Open session for client '%s'\n",
                                         name);
                        cs.enter();
                        client->link_after(&clients);

						TRACE_MSG((msg_flag, "handshake: connect client agent\n"));
                        client->connect(login->sock, name);
                        login->sock = NULL; // do not delete
                        delete login;
						if (!opened && handshake_list.empty()) {
                            term_event.signal();
                        }
                        cs.leave();

						TRACE_MSG((msg_flag, "handshake: start client agent poll\n"));
                        client->poll();
                        return;
                    }
                }
            }
        }
	} else {
		if (opened) {
            msg_buf buf;
            login->sock->get_error_text(buf, sizeof buf);
            console::message(msg_error|msg_time,
                             "Handshake failed: %s\n", buf);
        }
    }
    cs.enter();
    delete login;
	if (!opened && handshake_list.empty()) {
        term_event.signal();
    }
    cs.leave();
}

void storage_server::accept(socket_t* gateway)
{
    msg_buf buf;
    while (opened) {
        socket_t* new_sock = gateway->accept();
        if (new_sock != NULL) {
            if (!opened) {
                delete new_sock;
                break;
            }
            login_data* login = new login_data(this, new_sock, gateway == global_gateway);
            cs.enter();
            login->link_after(&handshake_list);
            cs.leave();
            communication::create_receiver(start_handshake, login);
        } else {
            if (opened) {
                gateway->get_error_text(buf, sizeof buf);
                console::message(msg_error|msg_time,
                                 "Failed to accept socket: %s\n", buf);
            }
        }
    }
    cs.enter();
    if (--n_opened_gateways == 0) {
        term_event.signal();
    }
    cs.leave();
}

void storage_server::remove_client_agent(client_agent* agent)
{
    cs.enter();
    delete agent;
    if (!opened && clients.empty()) {
        term_event.signal();
    }
    cs.leave();
}

void storage_server::iterate_clients(void (client_process::*iterator)(void))
{
    cs.enter();
    client_process* cli = (client_process*)&clients;
    while ((cli = (client_process*)cli->next) != &clients) {
        (cli->*iterator)();
    }
    cs.leave();
}

void storage_server::remove_server_agent(server_agent* agent)
{
    cs.enter();
    TRACE_MSG((msg_important, "Server '%s' is now offline\n",
               agent->get_name()));
    n_online_remote_servers -= 1;
    if (!opened && n_online_remote_servers == 0) {
        term_event.signal();
    }
    cs.leave();
}

server_agent* storage_server::create_server_agent(int id)
{
    return new server_agent(this, id);
}

client_agent* storage_server::create_client_agent()
{
    return new client_agent(this, ++client_id, object_cluster_size_limit);
}

void storage_server::send(stid_t sid, dbs_request* req, size_t body_len)
{
    servers[sid]->send(req, body_len);
}

void storage_server::read_msg_body(stid_t sid, void* buf, size_t size)
{
    servers[sid]->read_msg_body(buf, size);
}

void task_proc storage_server::start_local_gatekeeper(void* arg)
{
    storage_server* server = (storage_server*)arg;
    server->accept(server->local_gateway);
}

void task_proc storage_server::start_global_gatekeeper(void* arg)
{
    storage_server* server = (storage_server*)arg;
    server->accept(server->global_gateway);
}

boolean storage_server::open(char const* database_configuration_file)
{
    int  i;
    socket_t* sock;
    char buf[MAX_CFG_FILE_LINE_SIZE];
    msg_buf err;

    FILE* cfg = fopen(database_configuration_file, "r");

    if (cfg == NULL) {
        console::message(msg_error,
                         "Failed to open database configuration file '%s'\n",
                         database_configuration_file);
        return False;
    }
    if (fgets(buf,sizeof buf,cfg) == NULL || sscanf(buf,"%d",&n_servers) != 1)
    {
        console::message(msg_error, "Bad format of configuration file '%s'\n",
                         database_configuration_file);
        fclose(cfg);
        return False;
    }
    if (id >= n_servers) {
        console::message(msg_error, "Can't open server %d since configuration"
                         "file contains only %d entries\n",
                         id, n_servers);
        fclose(cfg);
        return False;
    }

    servers = new server_agent*[n_servers];

    for (i = 0; i < n_servers; i++) {
        if (i != id) {
            servers[i] = create_server_agent(i);
        } else {
            servers[i] = NULL;
        }
    }
    *name = '\0';
    n_opened_gateways = 0;
    n_online_remote_servers = 0;
    backup_started = False;
    global_gateway = NULL;
    local_gateway = NULL;
    clients.prune();
    handshake_list.prune();

    while (fgets(buf, sizeof buf, cfg)) {
        char hostname[MAX_CFG_FILE_LINE_SIZE];
        int j;
        if (sscanf(buf, "%d:%s", &j, hostname) == 2 && j == id) {
	    if (*hostname == '"') { 
		char *dst = name, *src = hostname+1;
		while (*src != '\0') { 
		    if (*src != '\"') { 
			*dst++ = *src;
		    }
		    src += 1;
		}
		*dst = '\0';
	    } else { 
		strcpy(name, hostname);
	    }
            break;
        }
    }
    if (*name == '\0') {
        console::message(msg_error,
                         "Local server %d name is not defined in configuration"
                         " file '%s'\n", id, database_configuration_file);
        fclose(cfg);
        return False;
    }
    fseek(cfg, 0, 0); // seek to the beginning of the file

    while (fgets(buf, sizeof buf, cfg)) {
        char hostname[MAX_CFG_FILE_LINE_SIZE];
        int j;
        if (sscanf(buf, "%d:%s", &j, hostname) == 2) {
            if (strlen(hostname) >= MAX_LOGIN_NAME) {
                console::message(msg_error,
                                 "Server name too long: '%s'\n", hostname);
            } else {
                if (j < id) {
                    sock = socket_t::connect(hostname);
                    if (sock->is_ok()) {
                        if (servers[j]->handshake(sock, id, name)) {
                            servers[j]->connect(sock, hostname, True);
                            continue;
                        }
                    } else {
                        sock->get_error_text(err, sizeof err);
                        console::message(msg_error,
                                         "Failed to connect server '%s': %s\n",
                                         hostname, err);
                    }
                    delete sock;
                }
            }
        }
    }
    fclose(cfg);

    if (!pool_mgr->open(this)  ||
        !mem_mgr->open(this)   ||
        !obj_mgr->open(this)   ||
        !trans_mgr->open(this) ||
        !class_mgr->open(this))
    {
        close();
        return False;
    }
    opened = True;

    pool_mgr->initialize();
    mem_mgr->initialize();
    obj_mgr->initialize();
    trans_mgr->initialize();
    class_mgr->initialize();

    client_id = 0;
    char* gname = name;
    if(*gname == '!') {
	gname++;
    }
    if (accept_remote_connections) {
        global_gateway = socket_t::create_global(gname);
        if (global_gateway) {
            if (global_gateway->is_ok()) {
                n_opened_gateways = 1;
                task::create(start_global_gatekeeper, this);
            } else {
                global_gateway->get_error_text(err, sizeof err);
                console::message(msg_error,
                                 "Failed to open global accept socket: %s\n",err);
                delete global_gateway;
                global_gateway = NULL;
            }
        }
    }
    local_gateway = socket_t::create_local(name);
    if (local_gateway) {
        if (local_gateway->is_ok()) {
            cs.enter();
            n_opened_gateways += 1;
            cs.leave();
            task::create(start_local_gatekeeper, this);
        } else {
            local_gateway->get_error_text(err, sizeof err);
            console::message(msg_error,
                             "Failed to open local accept socket: %s\n", err);
            delete local_gateway;
            local_gateway = NULL;
        }
    }
    if (n_opened_gateways == 0) {
        console::message(msg_error, "Failed to create gateways\n");
        close();
        return False;
    }
    return True;
}

void storage_server::close()
{
    cs.enter();
    opened = False;

    if (local_gateway != NULL) {
        local_gateway->cancel_accept();
    }
    if (global_gateway != NULL) {
        global_gateway->cancel_accept();
    }
    while (n_opened_gateways != 0) {
        TRACE_MSG((msg_important,
                   "Close server: number of opened gateways: %d\n",
                   n_opened_gateways));
        term_event.reset();
        term_event.wait();
    }

    login_data* login = (login_data*)handshake_list.next;
    while (login != &handshake_list) {
        login_data* next = (login_data*)login->next;
        login->sock->shutdown();
        login = next;
    }
    while (!handshake_list.empty()) {
        TRACE_MSG((msg_important,
                   "Close server: waiting pending handshakes\n"));
        term_event.reset();
        term_event.wait();
    }

    client_agent* client = (client_agent*)clients.next;
    while (client != &clients) {
        client_agent* next = (client_agent*)client->next;
        client->disconnect();
        client = next;
    }

    cs.leave();
    pool_mgr->shutdown();
    mem_mgr->shutdown();
    obj_mgr->shutdown();
    trans_mgr->shutdown();
    class_mgr->shutdown();
    cs.enter();

    while (!clients.empty()) {
        TRACE_MSG((msg_important,
                   "Close server: waiting until all clients will "
                   "be disconnected\n"));
        term_event.reset();
        term_event.wait();
    }

    if (backup_started) {
        console::message(msg_notify,
                         "Waiting for termination of current backup\n");
        backup_finished_event.reset();
        backup_finished_event.wait();
        console::message(msg_notify, "Backup terminated\n");
    }

    if (servers != NULL) {
        for (int i = 0; i < n_servers; i++) {
            if (i != id) {
                servers[i]->disconnect();
            }
        }
    }

    while (n_online_remote_servers != 0)
    {
        TRACE_MSG((msg_important, "Close server: disconnect remote servers, "
                   "n_online_remote_servers=%d\n", n_online_remote_servers));
        term_event.reset();
        term_event.wait();
    }
    cs.leave();
    //
    // At this moment all server threads are terminated and no new
    // one can be created since gateways are closed
    //
    TRACE_MSG(( msg_important, "Close managers...\n"));
    delete local_gateway;
    delete global_gateway;
    obj_mgr->close();
    class_mgr->close();
    mem_mgr->close();
    pool_mgr->close();
    trans_mgr->close();

    if (servers != NULL) {
        for (int i = 0; i < n_servers; i++) {
            if (i != id) {
                delete servers[i];
            }
        }
        delete[] servers;
        servers = NULL;
    }
    TRACE_MSG((msg_important, "Database shutdown completed...\n"));
}

unsigned storage_server::get_number_of_servers() const
{
    return n_servers;
}

const char* storage_server::get_name() const
{
    return name;
}

void storage_server::remote_server_connected(stid_t sid)
{
    //
    // Restart garbage collection each time some server is rebooted
    // and connected to coordinator or when server reestablish connection with
    // coordinator
    //
    if (id == GC_COORDINATOR || sid == GC_COORDINATOR) {
        mem_mgr->gc_abort(False);
    }
    cs.enter();
    n_online_remote_servers += 1;
    cs.leave();
}

unsigned storage_server::get_oldest_client_id()
{
    cs.enter();
    unsigned id = clients.empty() ? 0 : ((client_agent*)clients.prev)->id;
    cs.leave();
    return id;
}

void storage_server::dump(char* what)
{
    cs.enter();
    if (opened) {
        if (strstr(what, "version")) {
            console::output("GOODS version %3.2f\n", GOODS_VERSION);
        }
        if (strstr(what, "server")) {
            for (int i = 0; i < n_servers; i++) {
                if (i != id) {
                    if (servers[i] != NULL) {
                        console::output("Remote server %d: '%s' - %s\n",
                                         i, servers[i]->get_name(),
                                         servers[i]->is_online()
                                         ? "online" : "offline");
                    }
                } else {
                    console::output("Local server %d: '%s'\n", i, name);
                }
            }
            if (backup_started) {
                console::output("Active backup process to file '%s'\n",
                                backup_file->get_name());
            }
        }
        if (strstr(what, "client")) {
            console::output("Attached clients:\n");
            for (client_agent* agent = (client_agent*)clients.next;
                 agent != &clients;
                 agent = (client_agent*)agent->next)
            {
                obj_mgr->dump(agent, what);
            }
        }
        if (strstr(what, "transaction")) {
            trans_mgr->dump(what);
        }
        if (strstr(what, "memory")) {
            mem_mgr->dump(what);
            obj_mgr->dump(what);
            pool_mgr->dump(what);
        }
        if (strstr(what, "class")) {
            class_mgr->dump(what);
        }
    } else {
        console::output("Server not opened");
    }
    cs.leave();
}

void task_proc storage_server::start_backup_process(void* arg)
{
    ((storage_server*)arg)->backup();
}


void storage_server::compactify()
{
    mem_mgr->compactify();
}

static int get_timezone_offset()
{
	_TIME_ZONE_INFORMATION time_zone;
	DWORD rt = GetTimeZoneInformation(&time_zone);
	int time_diff = time_zone.Bias;
	if(rt == TIME_ZONE_ID_DAYLIGHT)
		time_diff += time_zone.DaylightBias;
	return time_diff;
}

//[MC] Customized the incremental backup functionality
boolean storage_server::backup()
{
    char const* backup_file_name = backup_file->get_name();
    int  backup_file_name_length = (int)strlen(backup_file_name);    
    nat4 backup_no = 1;
    page_timestamp_t last_page_timestamp = 0;
	int8 backup_date_time = 0;
	long time_diff = 0;
    bool incremental_backup = false;
    boolean result;
    if (backup_file_name_length > 3 && stricmp(backup_file_name+backup_file_name_length-4, ".inc") == 0) {
        char buf[MAX_BACKUP_INC_FILE_LINE_SIZE];
        last_page_timestamp = 1; // indicator of incremental backup
        FILE* inc = fopen(backup_file_name, "r");
        if (inc != NULL) {             
            while (fgets(buf, sizeof buf, inc) != NULL) { 
                nat4 last_backup_no;
                if (sscanf(
						buf, 
						"%u %" INT8_FORMAT "u %" INT8_FORMAT "u %d", 
						&last_backup_no, 
						&last_page_timestamp,
						&backup_date_time,
						&time_diff) < 3 || 
					last_backup_no != backup_no) {
                    console::message(msg_error|msg_time, "Corrupted incremental backup header file %s\n", backup_file_name);
                    fclose(inc);                    
                    return false;
                }
                backup_no += 1;
            }            
            fclose(inc);
        }
		
		static const char backup_prefix_full[] = "full";
		static const char backup_prefix_diff[] = "diff";
		
		tm t;
		time(&backup_date_time);
		localtime_s(&t, &backup_date_time);
		time_diff = get_timezone_offset();
		
        incremental_backup = true;
        char* backup_part_name = new char[MAX_PATH];
        sprintf(
			backup_part_name, 
			"%.*s-%s-%02u-%04u-%02u-%02uT%02u-%02u.bck", 
			backup_file_name_length - 4, backup_file_name,
			backup_no == 1 ? backup_prefix_full : backup_prefix_diff,
			backup_no,
			t.tm_year + 1900,
			t.tm_mon + 1,
			t.tm_mday,
			t.tm_hour,
			t.tm_min);
        os_file backup_part(backup_part_name);
        result = trans_mgr->backup(backup_part,
                                   backup_start_delay,
                                   backup_start_log_size,
                                   last_page_timestamp);
        delete[] backup_part_name;
    } else { 
        result = trans_mgr->backup(*backup_file,
                                   backup_start_delay,
                                   backup_start_log_size,
                                   last_page_timestamp);
    }
    cs.enter();
    backup_started = False;
    backup_finished_event.signal();
    if (incremental_backup) { // incremental backup
        FILE* inc = fopen(backup_file_name, "a");
        if (inc == NULL) {
            console::message(msg_error|msg_time, "Failed to append incremental backup header file %s\n", backup_file_name);
            cs.leave();
            return false;
        }
		
        fprintf(inc, "%u %" INT8_FORMAT "u %" INT8_FORMAT "u %d\n", backup_no, last_page_timestamp, backup_date_time, time_diff);
        fclose(inc);
    }   
    cs.leave();
    if (backup_callback) {
        (*backup_callback)(*this, *backup_file, result);
    }
    return result;
}

void storage_server::start_backup(file&   backup_file,
                                  time_t  backup_start_delay,
                                  fsize_t backup_start_log_size,
                                  backup_finish_callback backup_callback)
{
    cs.enter();
    if (backup_started || !opened) {
        cs.leave();
        return;
    }
    this->backup_file = &backup_file;
    this->backup_start_delay = backup_start_delay;
    this->backup_start_log_size = backup_start_log_size;
    this->backup_callback = backup_callback;
    backup_started = True;
    
    task::create(start_backup_process, this, task::pri_background);
    cs.leave();
}

void storage_server::stop_backup()
{
    cs.enter();
    assert(opened && backup_started);
    backup_started = False;
    trans_mgr->stop_backup();
    cs.leave();
}

//[MC] Get backup time adjusted to local time
static void convert_backup_time_to_local(tm& t, time_t& backup_date_time, const int time_diff)
{
	gmtime_s(&t, &backup_date_time);
	t.tm_min -= time_diff;
	_mkgmtime(&t);
}

//[MC] Customize the restore of incremental backups. Allow to restore up to a specific backup in the sequence
boolean storage_server::restore(file& backup_file,
                                const char* database_config_file, int end_backup_no /*= -1*/)
{
    assert(!opened);

    char const* backup_file_name = backup_file.get_name();
    int  backup_file_name_length = (int)strlen(backup_file_name);    
    if (backup_file_name_length > 3 && stricmp(backup_file_name+backup_file_name_length-4, ".inc") == 0) {
        char buf[MAX_BACKUP_INC_FILE_LINE_SIZE];
        if (database_config_file == NULL) { 
            console::message(msg_error|msg_time, "To restore from incremental backup database configuration file should be specified\n");
            return false;
        }            
        FILE* inc = fopen(backup_file_name, "r");
        if (inc == NULL) {             
            console::message(msg_error|msg_time, "Failed to open backup header file %s\n", backup_file_name);
            return false;
        }
        page_timestamp_t last_page_timestamp = 1;
		int8 backup_date_time = 0;
		long time_diff = 0;
        nat4 last_backup_no = 0;
        while (fgets(buf, sizeof buf, inc) != NULL) {  
            nat4 backup_no;
            page_timestamp_t page_timestamp;
			int count = sscanf(buf, "%u %" INT8_FORMAT "u %" INT8_FORMAT "u %d", &backup_no, &page_timestamp, &backup_date_time, &time_diff);
            if ((count != 3 && count != 4) || last_backup_no+1 != backup_no || page_timestamp < last_page_timestamp) {
                console::message(msg_error|msg_time, "Corrupted incremental backup header file %s\n", backup_file_name);
                fclose(inc);                    
                return false;
            }

            last_backup_no = backup_no;;
            last_page_timestamp = page_timestamp;

			static const char backup_prefix_full[] = "full";
			static const char backup_prefix_diff[] = "diff";
		
			tm t;			
			if (count == 4)
				convert_backup_time_to_local(t, backup_date_time, time_diff);
			else
				localtime_s(&t, &backup_date_time);
			
			char* backup_part_name = new char[MAX_PATH];
			sprintf(
				backup_part_name, 
				"%.*s-%s-%02u-%04u-%02u-%02uT%02u-%02u.bck", 
				backup_file_name_length - 4, backup_file_name,
				backup_no == 1 ? backup_prefix_full : backup_prefix_diff,
				backup_no,
				t.tm_year + 1900,
				t.tm_mon + 1,
				t.tm_mday,
				t.tm_hour,
				t.tm_min);

            os_file backup_part(backup_part_name);
            delete[] backup_part_name;
            
            if (opened) { 
                close();
            }
            
            if (!trans_mgr->restore(backup_part) || !open(database_config_file)) {
                console::message(msg_error|msg_time, "Stop restoring from incremental backup on part %u\n", backup_no);
                fclose(inc);
                return false;
            }

			if (backup_no == end_backup_no) break;
        }            
        fclose(inc);
        return true;
    } else { // non incremental backup
        return trans_mgr->restore(backup_file)
            && (database_config_file == NULL || open(database_config_file));
    }
}

void storage_server::notify_clients()
{
    cs.enter();
    for (l2elem* agent = clients.next; agent != &clients; agent = agent->next)
    {
        ((client_agent*)agent)->notify();
    }
    cs.leave();
}

//
// send a message from a client to others
//
void storage_server::send_messages( client_agent *sender, dnm_array<dbs_request>& msg) {
  cs.enter();
  for (l2elem* agent = clients.next; agent != &clients; agent = agent->next)
    {
      if( agent == sender ) continue;
      ((client_agent*)agent)->send_messages( msg);
    }
  cs.leave();
}
 
void storage_server::set_object_cluster_size_limit(size_t cluster_size)
{
    object_cluster_size_limit = cluster_size;
}

boolean storage_server::restore_credentials(file& backup_file) 
{
    boolean rc = True;
    if (authenticator != NULL && include_credentials_in_backup) { 
        rc = authenticator->restore(backup_file);
    }
    return rc;
}

boolean storage_server::save_credentials(file& backup_file)
{
    boolean rc = True;
    if (authenticator != NULL && include_credentials_in_backup) { 
        cs.enter();
        rc = authenticator->backup(backup_file);
        cs.leave();
    }
    return rc;
}

void storage_server::update_credentials()
{
    if (authenticator != NULL) { 
        cs.enter();
        authenticator->reload();
        cs.leave();
    }
}

void storage_server::add_user(char const* login, char const* password, boolean encrypted_password)
{
    if (authenticator != NULL) { 
        cs.enter();
        authenticator->add_user(login, password, encrypted_password);
        cs.leave();
    }
}

boolean storage_server::del_user(char const* login)
{
    boolean rc = False;
    if (authenticator != NULL) { 
        cs.enter();
        rc = authenticator->del_user(login);
        cs.leave();
    }
    return rc;
}

boolean storage_server::authenticate(dbs_request const& req, char* name, boolean is_remote_login)
{
    boolean authenticated = True;
    if (authenticator != NULL && (!local_access_for_all || is_remote_login)) { 
        char* sep = strchr(name, ':');
        if (sep == NULL) { 
            authenticated = False;
        } else { 
            char* login = name;
            char* password = sep + 1;
            *sep = '\0';
            cs.enter();
            authenticated = authenticator->authenticate(login, password);
            cs.leave();
        }
    }
    return authenticated;
}

void storage_server::set_local_access_for_all(boolean enabled)
{
    local_access_for_all = enabled;
}

void storage_server::set_include_credentials_in_backup(boolean enabled)
{
    include_credentials_in_backup = enabled;
}

storage_server::storage_server(stid_t                      sid,
                               object_access_manager&      omgr,
                               pool_manager&               pmgr,
                               class_manager&              cmgr,
                               memory_manager&             mmgr,
                               server_transaction_manager& tmgr,
                               size_t                      object_cluster_size,
                               boolean                     remote_connections,
                               authentication_manager*     auth_mgr,
                               char const*                 ext_blob_dir)
    : dbs_server(sid), term_event(cs), backup_finished_event(cs), authenticator(auth_mgr), external_blob_directory(ext_blob_dir)
{
    servers = NULL;
    n_servers = 0;

    obj_mgr   = &omgr;
    mem_mgr   = &mmgr;
    pool_mgr  = &pmgr;
    class_mgr = &cmgr;
    trans_mgr = &tmgr;

    opened = False;
    local_gateway = global_gateway = NULL;
    object_cluster_size_limit = object_cluster_size;
    accept_remote_connections = remote_connections;
    login_refused_code = 0;
    local_access_for_all = False;
}


storage_server::~storage_server() 
{
    delete authenticator;
}

void storage_server::on_replication_master_crash() {}

file* storage_server::external_blob_file(opid_t opid)
{
    char file_name[MAX_CFG_FILE_LINE_SIZE];
    sprintf(file_name, "%s%x.blob", external_blob_directory, opid);
    return new os_file(file_name);
}

void storage_server::write_external_blob(opid_t opid, void const* body, size_t size)
{
    msg_buf buf;
    file* blob_file = external_blob_file(opid);
    file::iop_status status = blob_file->open(file::fa_write,
                                             file::fo_largefile|file::fo_truncate|file::fo_create);
    if (status != file::ok) {
        blob_file->get_error_text(status, buf, sizeof buf);
        message(msg_error|msg_time, "Failed to create external BLOB file: %s\n", buf);
    } else { 
        status = blob_file->write(body, size);
        if (status != file::ok) { 
            blob_file->get_error_text(status, buf, sizeof buf);
            message(msg_error|msg_time, "Failed to write external BLOB: %s\n", buf);
        }
    }
    delete blob_file;
}

void storage_server::read_external_blob(opid_t opid, void* body, size_t size)
{
    msg_buf buf;
    file* blob_file = external_blob_file(opid);
    file::iop_status status = blob_file->open(file::fa_read, file::fo_largefile);
    if (status != file::ok) {
        blob_file->get_error_text(status, buf, sizeof buf);
        message(msg_error|msg_time, "Failed to open external BLOB file: %s\n", buf);
    } else { 
        status = blob_file->read(body, size);
        if (status != file::ok) { 
            blob_file->get_error_text(status, buf, sizeof buf);
            message(msg_error|msg_time, "Failed to read external BLOB: %s\n", buf);
        }
    }
	
	//[MC] Reset content of external blob if file not found
	if (status != file::ok)
		memset(body, 0, size);

    delete blob_file;
}

void storage_server::remove_external_blob(opid_t opid)
{
    msg_buf buf;
    file* blob_file = external_blob_file(opid);
    file::iop_status status = blob_file->remove();
    if (status != file::ok) { 
        blob_file->get_error_text(status, buf, sizeof buf);
        message(msg_error|msg_time, "Failed to read external BLOB: %s\n", buf);
    }
    delete blob_file;
}


END_GOODS_NAMESPACE
