// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CLIENT.CXX >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 28-Oct-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Client storage interface. This store is responsible for transferring        
// client requests to server and receiving server's replies.  
//-------------------------------------------------------------------*--------*

#include "goods.h"
#ifdef _WIN32
#pragma hdrstop
#endif
#include "client.h"
#include "dbexcept.h"

BEGIN_GOODS_NAMESPACE

boolean dbs_client_storage::handle_communication_error() 
{
    msg_buf buf; 
    sock->get_error_text(buf, sizeof buf); 
    console::error("Communication with server failed: %s\n", buf);
    return False;
} 

void dbs_client_storage::read(void* buf, size_t size)
{ 
    internal_assert(sock != NULL);
    while (!sock->read(buf, size)) { 
        if (!handle_communication_error()) { 
            console::error("Read operation failed");
        }
    }
    num_bytes_in += size;

    transaction_manager* mgr;
    mgr = transaction_manager::get_thread_transaction_manager();
    if (mgr != NULL)
        mgr->num_bytes_in += size;
}

void dbs_client_storage::write(void const* buf, size_t size)
{ 
    internal_assert(sock != NULL);
    ((dbs_request*)buf)->pack();
    critical_section guard(snd_cs);
    if (opened) { 
        while (!sock->write(buf, size)) { 
            if (!handle_communication_error()) { 
                console::error("Write operation failed");
            }
        }
    }
    num_bytes_out += size;

    transaction_manager* mgr;
    mgr = transaction_manager::get_thread_transaction_manager();
    if (mgr != NULL)
        mgr->num_bytes_out += size;
}

void dbs_client_storage::send_notifications()
{
    critical_section guard(notify_cs);
    int n = (int)notify_buf.size();
    if (n > 0) { 
        dbs_request* req = &notify_buf;
        req->object.extra = n - 1;
        while (--n > 0) (++req)->pack();
        write(&notify_buf, notify_buf.size()*sizeof(dbs_request));
        notify_buf.change_size(0);
    } 
}

void dbs_client_storage::receive_header()
{
    dnm_array<dbs_request> buf;
    
    while (opened) {
        read(&reply, sizeof reply); 
        reply.unpack();
        switch (reply.cmd) { 
	  case dbs_request::cmd_user_msg:
	    application->receive_message( reply.any.arg3);
	    if( reply.object.extra > 0 ){ 
		int n = reply.object.extra;
		buf.change_size(n);
		dbs_request* rp = &buf;
		read(rp, n*sizeof(dbs_request));
		while (--n >= 0) {
		    rp->unpack();
		    internal_assert(rp->cmd == dbs_request::cmd_user_msg);
		    rp->object.extra = n;
		    application->receive_message( rp->any.arg3);
		    rp += 1;
		}
	    }
	    break;
          case dbs_request::cmd_invalidate:
            notify_cs.enter();
            proceeding_invalidation = True;
            notify_cs.leave();
            application->invalidate(id, reply.object.opid);
            if (reply.object.extra > 0) { 
                int n = reply.object.extra;
                buf.change_size(n);
                dbs_request* rp = &buf;
                read(rp, n*sizeof(dbs_request));
                while (--n >= 0) {
                    rp->unpack();
                    internal_assert(rp->cmd == dbs_request::cmd_invalidate);
                    application->invalidate(id, rp->object.opid);
                    rp += 1;
                }
            }
            notify_cs.enter();
            proceeding_invalidation = False;
            if (!waiting_reply) { 
                send_notifications();
            }
            notify_cs.leave();
            break;

          case dbs_request::cmd_ping:
            dbs_request snd_req;
            snd_req.cmd = dbs_request::cmd_ping_ack;
            write(&snd_req, sizeof snd_req);
            break;

          case dbs_request::cmd_bye: 
            opened = False;
            if (!closing) { 
                application->disconnected(id);
                break;
            }
            // no break
          default:
            rep_sem.signal();
            rcv_sem.wait();
        }
    }
    term_event.signal();
}

void dbs_client_storage::send_receive(void const*  snd_buf, 
                                      size_t       snd_len,
                                      dbs_request& rcv_req,
                                      int          exp_cmd)
{
    notify_cs.enter();
    if (waiting_reply++ == 0) { 
        send_notifications();
    }
    notify_cs.leave();
    {
        critical_section cs(request_cs);
        if (snd_buf != NULL) {      
            write(snd_buf, snd_len);
        } 
        rep_sem.wait();
        rcv_req = reply;
        assert(rcv_req.cmd == exp_cmd);
        rcv_sem.signal();    
    }
    notify_cs.enter();
    if (--waiting_reply == 0) { 
        send_notifications();
    }
    notify_cs.leave();
}

void dbs_client_storage::send_receive(void const*  snd_buf, 
                                      size_t       snd_len,
                                      dbs_request& rcv_req,
                                      int          exp_cmd,
                                      dnm_buffer&  rcv_buf)
{
    notify_cs.enter();
    if (waiting_reply++ == 0) { 
        send_notifications();
    }
    notify_cs.leave();
    {
        critical_section cs(request_cs);
        if (snd_buf != NULL) {      
            write(snd_buf, snd_len);
        } 
        rep_sem.wait();
        rcv_req = reply;
        assert(rcv_req.cmd == exp_cmd);
	    rcv_buf.put(rcv_req.result.size);
	    if (rcv_req.result.size != 0) { 
		read(&rcv_buf, rcv_req.result.size);
	    } 
        rcv_sem.signal();    
    }
    notify_cs.enter();
    if (--waiting_reply == 0) { 
        send_notifications();    
    }
    notify_cs.leave();
}

void task_proc dbs_client_storage::receiver(void* arg) 
{
    ((dbs_client_storage*)arg)->receive_header();
} 

//---------------------------------------------------------


objref_t dbs_client_storage::allocate(cpid_t cpid, size_t size, int flags, objref_t clusterWith)
{
    dbs_request snd_req, rcv_req; 
    snd_req.cmd = dbs_request::cmd_alloc;
    snd_req.alloc.cpid = cpid;
    snd_req.alloc.size = (nat4)size;
	snd_req.alloc.flags = (nat1)flags;
	snd_req.alloc.cluster = (opid_t)clusterWith;
    send_receive(&snd_req, sizeof snd_req, 
                 rcv_req, dbs_request::cmd_location);
    return rcv_req.object.opid; 
} 

void dbs_client_storage::bulk_allocate(size_t sizeBuf[], cpid_t cpidBuf[], size_t nAllocObjects,
									   objref_t opidBuf[], size_t nReservedOids, hnd_t clusterWith[])
{
    size_t i;
    int flags = aof_none;
    for (i = 0; i < nAllocObjects; i++) { 
        if (clusterWith[i]) { 
            flags |= aof_clustered;
            break;
        }
    }
    size_t size = sizeof(dbs_request) + nAllocObjects*((flags & aof_clustered) ? 10 : 6); 
	dbs_request* req = (dbs_request*)snd_buf.put(size);
    dbs_request  reply;
    char* dst = (char*)(req+1);
    for (i = 0; i < nAllocObjects; i++) { 
        if (sizeBuf[i] & ALLOC_ALIGNED) {
            flags |= aof_aligned;
        }
        dst = pack4(dst, (nat4)sizeBuf[i]);
    }
    if (flags & aof_clustered) { 
        for (i = 0; i < nAllocObjects; i++) { 
            dst = pack4(dst, clusterWith[i] ? clusterWith[i]->opid : 0);
        }
    }
    for (i = 0; i < nAllocObjects; i++) { 
        dst = pack2(dst, cpidBuf[i]);
    }
    req->cmd = dbs_request::cmd_bulk_alloc;
    req->bulk_alloc.flags = (nat1)flags;
    req->bulk_alloc.n_objects = (nat4)nAllocObjects;
    req->bulk_alloc.reserved = (nat4)nReservedOids;
	send_receive(req, sizeof(dbs_request) + nAllocObjects*6,
				 reply, dbs_request::cmd_opids, snd_buf);
	char* src = &snd_buf;
    for (i = 0; i < nReservedOids; i++) { 
        opidBuf[i] = unpack4(src);
        src += 4;
    }
}

void dbs_client_storage::deallocate(objref_t opid)
{ 
    dbs_request snd_req; 
	snd_req.cmd = dbs_request::cmd_free;
	snd_req.object.opid = (opid_t)opid;
    write(&snd_req, sizeof snd_req);
} 

boolean dbs_client_storage::lock(objref_t opid, lck_t lck, int attr)
{
    dbs_request snd_req, rcv_req; 
    snd_req.cmd = dbs_request::cmd_lock;
    snd_req.lock.type = lck;
	snd_req.lock.attr = attr;
	snd_req.lock.opid = (opid_t)opid;
    send_receive(&snd_req, sizeof snd_req, 
                 rcv_req, dbs_request::cmd_lockresult);
    return rcv_req.result.status; 
}


void dbs_client_storage::unlock(objref_t opid, lck_t lck)
{
    dbs_request snd_req; 
    snd_req.cmd = dbs_request::cmd_unlock;
	snd_req.lock.type = lck;
	snd_req.lock.opid = (opid_t)opid;
    write(&snd_req, sizeof snd_req);
}

void dbs_client_storage::get_class(cpid_t cpid, dnm_buffer& buf)
{
    dbs_request snd_req, rcv_req;  
    snd_req.cmd = dbs_request::cmd_getclass;
    snd_req.clsdesc.cpid = cpid;
    send_receive(&snd_req, sizeof snd_req, 
                 rcv_req, dbs_request::cmd_classdesc, buf);
}

cpid_t dbs_client_storage::put_class(dbs_class_descriptor* dbs_desc)
{
    dbs_request rcv_req;
    size_t dbs_desc_size = dbs_desc->get_size();

	dbs_request* put_class_req
		= (dbs_request*)snd_buf.put(sizeof(dbs_request) + dbs_desc_size);

    put_class_req->cmd = dbs_request::cmd_putclass;
    put_class_req->clsdesc.size = (nat4)dbs_desc_size;
    memcpy(put_class_req+1, dbs_desc, dbs_desc_size);
    ((dbs_class_descriptor*)(put_class_req+1))->pack(); 
    send_receive(put_class_req, sizeof(dbs_request)+dbs_desc_size,
                 rcv_req, dbs_request::cmd_classid);
    return rcv_req.clsdesc.cpid;
}

void dbs_client_storage::change_class(cpid_t cpid, 
                                      dbs_class_descriptor* dbs_desc)
{
    size_t dbs_desc_size = dbs_desc->get_size();

	dbs_request* put_class_req
		= (dbs_request*)snd_buf.put(sizeof(dbs_request) + dbs_desc_size);

    put_class_req->cmd = dbs_request::cmd_modclass;
    put_class_req->clsdesc.cpid = cpid;
    put_class_req->clsdesc.size = (nat4)dbs_desc_size;
    memcpy(put_class_req+1, dbs_desc, dbs_desc_size);
    ((dbs_class_descriptor*)(put_class_req+1))->pack(); 
    write(put_class_req, sizeof(dbs_request) + dbs_desc_size);
}

//
// Send a message to others clients. 
//
void dbs_client_storage::send_message( int message) {
    dbs_request req;
    req.cmd = dbs_request::cmd_user_msg;
    req.any.arg3 = message;
    req.object.extra = 0;
    write( &req, sizeof( req));
}

//
// Schedule a message to others clients. 
//
void dbs_client_storage::push_message( int message) {
  critical_section guard(send_msg_cs);
  
  dbs_request req;
  req.cmd = dbs_request::cmd_user_msg;
  req.any.arg3 = message;
  req.object.extra = 0;

  if( send_msg_buf.size() ) req.pack();
  send_msg_buf.push( req);
}

//
// Send all scheduleds messages to others clients. 
//
void dbs_client_storage::send_messages() {
  critical_section guard(send_msg_cs);

  if( send_msg_buf.size() == 0 ) return;

  dbs_request* req = &send_msg_buf;
  req->object.extra = (nat4)(send_msg_buf.size() - 1);
  write( &send_msg_buf, send_msg_buf.size()*sizeof(dbs_request));
  send_msg_buf.change_size( 0);
}

void dbs_client_storage::query(objref_t owner, char const* table, char const* where, char const* order_by, nat4 limit, nat4 offset, dnm_buffer& buf)
{
    dnm_buffer snd_buf;
    dbs_request rcv_req;
	size_t query_len = strlen(table) + 3;
	if (where) {
		query_len += strlen(where);
	}
	if (order_by) {
		query_len += strlen(order_by);
	}
	dbs_request* snd_req = (dbs_request*)snd_buf.put(sizeof(dbs_request) + query_len);
	snd_req->cmd = dbs_request::cmd_select;
	snd_req->select.flags = 0;
	snd_req->select.query_len = (nat2)query_len;
	snd_req->select.owner = (opid_t)owner;
	snd_req->select.offset = offset;
	snd_req->select.limit = limit;
	char* dst = (char*)snd_req + 1;
	strcpy(dst, table);
	dst += strlen(table) + 1;
	if (where) {
		strcpy(dst, where);
		dst += strlen(where);
	}
	*dst++ = '\0';
	if (order_by) {
		strcpy(dst, order_by);
		dst += strlen(order_by);
	}
	send_receive(snd_req, sizeof(dbs_request) + sizeof(opid_t) + query_len, rcv_req, dbs_request::cmd_query_result, buf);
	if (!rcv_req.result.status) {
		throw QueryException(&buf);
	}
}

void dbs_client_storage::query(objref_t& first_mbr, objref_t last_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members, dnm_buffer& buf)
{
	dnm_buffer snd_buf;
	dbs_request rcv_req;
	size_t query_len = strlen(query);
	dbs_request* snd_req = (dbs_request*)snd_buf.put(sizeof(dbs_request) + query_len + sizeof(opid_t));
    snd_req->cmd = dbs_request::cmd_query;
    snd_req->query.flags = (nat1)flags;
    snd_req->query.query_len = (nat2)query_len;
	snd_req->query.member_opid = (opid_t)first_mbr;
    snd_req->query.max_buf_size = buf_size;
	snd_req->query.max_members = max_members;
	*(opid_t*)(snd_req + 1) = last_mbr;
	memcpy((opid_t*)(snd_req + 1) + 1, query, query_len);
	send_receive(snd_req, sizeof(dbs_request) + sizeof(opid_t) + query_len, rcv_req, dbs_request::cmd_query_result, buf);
	first_mbr = rcv_req.result.tid;
    if (!rcv_req.result.status) { 
        throw QueryException(&buf);
    }
}

void dbs_client_storage::load(objref_t opid, int flags, dnm_buffer& buf)
{
    dbs_request snd_req, rcv_req;
    snd_req.cmd = dbs_request::cmd_load;
    snd_req.object.flags = flags;
    snd_req.object.opid = opid;
    snd_req.object.extra = 0;
    send_receive(&snd_req, sizeof snd_req, 
                 rcv_req, dbs_request::cmd_object, buf);
}

void dbs_client_storage::load(objref_t* opp, int n_objects,
							  int flags, dnm_buffer& buf)
{
    dbs_request rcv_req;
    dnm_buffer snd_buf;
	if (n_objects > 0) {
		snd_buf.put(sizeof(dbs_request)*n_objects);
		dbs_request* rp = (dbs_request*)&snd_buf;
        rp->cmd = dbs_request::cmd_load;
		rp->object.flags = flags;
		rp->object.opid = (opid_t)*opp++;
        rp->object.extra = n_objects - 1;
        for (int i = 1; i < n_objects; i++) { 
            rp += 1;
            rp->cmd = dbs_request::cmd_load;
            rp->object.flags = flags;
            rp->object.opid = *opp++;
            rp->pack();
		}
		send_receive(&snd_buf, n_objects*sizeof(dbs_request),
                     rcv_req, dbs_request::cmd_object, buf);
    } else { 
        buf.put(0);
    }
}

void dbs_client_storage::forget_object(objref_t opid)
{
    dbs_request snd_req;
	snd_req.cmd = dbs_request::cmd_forget;
	snd_req.object.opid = (opid_t)opid;
    snd_req.object.extra = 0;
    notify_cs.enter();
    if (proceeding_invalidation || waiting_reply) { 
        dbs_request* req = &notify_buf;
        int n = (int)notify_buf.size();
        while (--n >= 0) { 
            if (req->object.opid == opid) { 
                req->cmd = dbs_request::cmd_forget;
                notify_cs.leave();
                return;
            }
            req += 1;
        }
        notify_buf.push(snd_req);
    } else { 
        write(&snd_req, sizeof snd_req);
    }
    notify_cs.leave();
}

void dbs_client_storage::throw_object(objref_t opid)
{
    dbs_request snd_req;
	snd_req.cmd = dbs_request::cmd_throw;
	snd_req.object.opid = (opid_t)opid;
    snd_req.object.extra = 0;
    notify_cs.enter();
    if (proceeding_invalidation || waiting_reply) { 
        notify_buf.push(snd_req);
    } else { 
        write(&snd_req, sizeof snd_req);
    }
    notify_cs.leave();
}

void dbs_client_storage::begin_transaction(dnm_buffer& buf)
{
    buf.put(sizeof(dbs_request));
}

void dbs_client_storage::rollback_transaction()
{
}

boolean dbs_client_storage::commit_coordinator_transaction(int n_trans_servers,
                                                           stid_t*trans_servers,
                                                           dnm_buffer& buf, 
                                                           trid_t& tid)
{
    dbs_request rcv_req;
    if (n_trans_servers > 1) { 
        char* p = buf.append(n_trans_servers*sizeof(stid_t));
        for (int i = 0; i < n_trans_servers; i++) { 
            p = pack2(p, trans_servers[i]);
        }
    }
    dbs_request* trans_req = (dbs_request*)&buf;
    trans_req->cmd = dbs_request::cmd_transaction;
    trans_req->trans.n_servers = n_trans_servers;
	trans_req->trans.size = (nat4)(buf.size() - sizeof(dbs_request));
	trans_req->trans.crc = calculate_crc(trans_req+1, (int)(buf.size() - sizeof(dbs_request)));
	send_receive(trans_req, buf.size(),
		 rcv_req, dbs_request::cmd_transresult);
    if (rcv_req.result.status) { // committed ?
        tid = rcv_req.result.tid; 
    }
    return rcv_req.result.status; 
}

void dbs_client_storage::commit_transaction(stid_t      coordinator, 
                                            int         n_trans_servers,
                                            stid_t*     trans_servers,
                                            dnm_buffer& buf, 
                                            trid_t      tid)
{
    if (n_trans_servers > 1) { 
        char* p = buf.append(n_trans_servers*sizeof(stid_t));
        for (int i = 0; i < n_trans_servers; i++) { 
            p = pack2(p, trans_servers[i]);
        }
    }
    dbs_request* trans_req = (dbs_request*)&buf;
    trans_req->cmd = dbs_request::cmd_subtransact; 
    trans_req->trans.n_servers = n_trans_servers;
    trans_req->trans.coordinator = coordinator;
    trans_req->trans.size = (nat4)(buf.size() - sizeof(dbs_request));
    trans_req->trans.tid = tid;
	trans_req->trans.crc = calculate_crc(trans_req+1, (int)(buf.size() - sizeof(dbs_request)));
	write(trans_req, buf.size());
}

boolean dbs_client_storage::wait_global_transaction_completion()
{
    dbs_request rcv_req;
    send_receive(NULL, 0, rcv_req, dbs_request::cmd_transresult);
    return rcv_req.result.status;
}


boolean dbs_client_storage::open(const char* server_connection_name, const char* login, const char* password, obj_storage* os)
{
    sock = socket_t::connect(server_connection_name);
    if (!sock->is_ok()) { 
        msg_buf buf;
        sock->get_error_text(buf, sizeof buf);
        console::output("Failed to connect server '%s': %s\n", 
                         server_connection_name, buf);
        delete sock;
        sock = NULL; 
        return False;
    }
    opened = True;
    closing = False;
    waiting_reply = False;
    proceeding_invalidation = False;
    char login_name[MAX_LOGIN_NAME];
    size_t login_name_len;
    notify_buf.change_size(0);

    if (login != NULL) { 
        if (password == NULL) { 
            password = "";
        }
        login_name_len = sprintf(login_name, "%s:%s", login, password); 
    } else { 
        login_name_len = sprintf(login_name, get_process_name());
    }
	snd_buf.put(sizeof(dbs_request) + login_name_len);
	dbs_request* login_req = (dbs_request*)&snd_buf;
	login_req->cmd = dbs_request::cmd_login;
    login_req->login.name_len = (nat4)login_name_len;
    strcpy((char*)(login_req+1), login_name); 
    write(login_req, sizeof(dbs_request) + login_name_len); 
    dbs_request rcv_req;
    read(&rcv_req, sizeof rcv_req);
    rcv_req.unpack();
    assert(rcv_req.cmd == dbs_request::cmd_ok ||
           rcv_req.cmd == dbs_request::cmd_bye ||
           rcv_req.cmd == dbs_request::cmd_refused);
    switch (rcv_req.cmd) { 
      case dbs_request::cmd_bye: 
            opened = False;
            application->disconnected(id);
            return False;
      case dbs_request::cmd_refused: 
            opened = False;
	    application->receive_message( rcv_req.any.arg3);
            application->login_refused(id);
            return False;
    }          
    task::create(receiver, this, task::pri_high); 
    return True;
}       


void dbs_client_storage::close()
{
    if (sock != NULL && sock->is_ok()) { 
        dbs_request snd_req, rcv_req;
        snd_req.cmd = dbs_request::cmd_logout; 
        closing = True;
        send_receive(&snd_req, sizeof snd_req, rcv_req, dbs_request::cmd_bye);
        sock->close();
        term_event.wait();
    }
    delete sock;
}

nat8 dbs_client_storage::get_used_size() 
{
    dbs_request snd_req, rcv_req; 
    snd_req.cmd = dbs_request::cmd_get_size;
    send_receive(&snd_req, sizeof snd_req, 
                 rcv_req, dbs_request::cmd_location);
    return cons_nat8(rcv_req.any.arg3, rcv_req.any.arg4); 
}

void dbs_client_storage::add_user(char const* login, char const* password)
{
    dnm_buffer snd_buf;
    size_t req_len = sizeof(dbs_request) + strlen(login) + strlen(password) + 2;
	snd_buf.put(req_len);
	dbs_request* req = (dbs_request*)&snd_buf;
	req->cmd = dbs_request::cmd_add_user;
    req->any.arg3 = (nat4)strlen(login)+1;
    req->any.arg4 = (nat4)strlen(password)+1;
    memcpy(req+1, login, req->any.arg3);
    memcpy((char*)(req+1) +req->any.arg3 , password, req->any.arg4);
    write(req, req_len);
}

void dbs_client_storage::del_user(char const* login)
{
    dnm_buffer snd_buf;
    size_t req_len = sizeof(dbs_request) + strlen(login) + 1;
	snd_buf.put(req_len);
	dbs_request* req = (dbs_request*)&snd_buf;
	req->cmd = dbs_request::cmd_del_user;
    req->any.arg3 = (nat4)strlen(login)+1;
    memcpy(req+1, login, req->any.arg3);
    write(req, req_len);
}

boolean dbs_client_storage::start_gc()
{
    dbs_request snd_req, rcv_req; 
    snd_req.cmd = dbs_request::cmd_start_gc;
    send_receive(&snd_req, sizeof snd_req, 
                 rcv_req, dbs_request::cmd_gc_started);
    return rcv_req.result.status; 
}

dbs_client_storage::~dbs_client_storage()
{
}

boolean dbs_client_storage::convert_goods_database(char const* path, char const* name)
{
	return false;
}

int dbs_client_storage::execute(char const* sql)
{
	return 0;
}

int dbs_client_storage::get_socket()
{
	return -1;
}

void dbs_client_storage::process_notifications()
{
}

void dbs_client_storage::wait_notification(event& e)
{
	e.wait();
}

void dbs_client_storage::listen(hnd_t opid, event& e)
{}

void dbs_client_storage::unlisten(hnd_t opid, event& e)
{}

END_GOODS_NAMESPACE
