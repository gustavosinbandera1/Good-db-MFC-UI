// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< TRANSMGR.CXX >-------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET   *     ?  *
// (Generic Object Oriented Database System)                        *   /\|  *
//                                                                   *  /  \  *
//                          Created:     18-Jan-97    K.A. Knizhnik * / [] \ *
//                          Last update: 12-Nov-98    K.A. Knizhnik * GARRET *
//------------------------------------------------------------------*--------*
// Transaction manager
//------------------------------------------------------------------*--------*

#include "server.h"

BEGIN_GOODS_NAMESPACE

server_transaction_manager::~server_transaction_manager() {}

dbs_transaction_header* dbs_transaction_header::pack()
{
    pack4(seqno);
    pack4(tid);
    pack4(coordinator);
    pack4(size);
    pack4(crc);
    return this;
}

dbs_transaction_header* dbs_transaction_header::unpack()
{
    unpack4(seqno);
    unpack4(tid);
    unpack4(coordinator);
    unpack4(size);
    unpack4(crc);
    return this;
}

//
// Transaction control block
//

void trans_cntl_block::receive_reply(trans_cntl_block_chain& tcb_hdr,
                                     stid_t                  sid,
                                     stid_t                  coordinator,
                                     trid_t                  tid,
                                     boolean                 result)
{
    tcb_hdr.cs.enter();
    for (trans_cntl_block *bp = tcb_hdr.chain; bp != NULL; bp = bp->next) {
        if (bp->tid == tid && bp->coordinator == coordinator) {
            bp->result &= result;
            bp->responder[bp->n_replies] = sid;
            if (++bp->n_replies == bp->n_responders) {
                bp->reply_sem.signal();
            }
            tcb_hdr.cs.leave();
            return;
        }
    }
    TRACE_MSG((msg_error|msg_time,
               "Unexpected %s notification from server %u to "
               "coordinator %u for transaction %lu\n",
               result ? "confirm" : "abort", sid, coordinator, tid));
    tcb_hdr.cs.leave();
}

//
// If server sends 'get_status' request to coordinator, then check if
// coordinator is still waiting for response from this server
// at first stage of transaction commit protocol. If so, treat
// 'get_status' request as acknowledgement from this server to commit
// transaction.
//
boolean trans_cntl_block::find_active_transaction(trans_cntl_block_chain& tcb_hdr,
                                                  stid_t sid, stid_t coordinator,
                                                  trid_t tid)
{
    tcb_hdr.cs.enter();
    for (trans_cntl_block *bp = tcb_hdr.chain; bp != NULL; bp = bp->next) {
        if (bp->tid == tid && bp->coordinator == coordinator)
        {
            int n = bp->n_replies;
            while (--n >= 0 && bp->responder[n] != sid);
            if (n < 0) {
                bp->responder[bp->n_replies] = sid;
                if (++bp->n_replies == bp->n_responders) {
                    bp->reply_sem.signal();
                }
            }
            tcb_hdr.cs.leave();
            return True;
        }
    }
    tcb_hdr.cs.leave();
    return False;
}

//
// If server sends 'checkpoint' request to coordinator, then check if
// coordinator is still waiting for response from this server
// at first stage of transaction commit protocol. If so, transaction
// should be aborted since failed server didn't save local part in it's log.
//
void trans_cntl_block::abort_transactions(trans_cntl_block_chain& tcb_hdr,
                                          stid_t sid, stid_t coordinator)
{
    tcb_hdr.cs.enter();
    for (trans_cntl_block *bp = tcb_hdr.chain; bp != NULL; bp = bp->next) {
        if (bp->coordinator == coordinator && bp->participant(sid)) {
            bp->result = False;
            bp->reply_sem.signal();
        }
    }
    tcb_hdr.cs.leave();
}

void trans_cntl_block::cancel_all_transactions(trans_cntl_block_chain& tcb_hdr,
                                               stid_t self)

{
    tcb_hdr.cs.enter();
    for (trans_cntl_block *bp = tcb_hdr.chain; bp != NULL; bp = bp->next) {
        if (bp->coordinator == self) {
            bp->result = False;
            bp->reply_sem.signal();
        }
    }
    tcb_hdr.cs.leave();
}

trans_cntl_block::poll_status trans_cntl_block::wait_replies(time_t timeout)
{
    if (reply_sem.wait_with_timeout(timeout)) {
        return result ? ts_confirmed : ts_refused;
    }
    return ts_timeout;
}

//
// This function broadcasts tm_refuse or tm_confirm request to all
// servers (except coordinator) participated in transaction.
//

void trans_cntl_block::broadcast_request(dbs_server* server,
                                         dbs_request const& req)
{
    tcb_hdr.cs.enter();
    int n = n_replies;
    n_replies = 0; // prepare for following receive
    for (int i = 0; i < n; i++) {
        dbs_request r = req;
        server->send(responder[i], &r);
    }
    tcb_hdr.cs.leave();
}


trans_cntl_block::trans_cntl_block(trans_cntl_block_chain& hdr,
                                   trid_t tid, stid_t coordinator,
                                   int n_servers, stid_t* servers,
                                   int n_responders)
: tcb_hdr(hdr)
{
    this->tid = tid;
    this->coordinator = coordinator;
    this->n_responders = n_responders;
    this->n_servers = n_servers;
    this->servers = servers;
    result = True;
    n_replies = 0;

    tcb_hdr.cs.enter();
    next = tcb_hdr.chain;
    tcb_hdr.chain = this;
    tcb_hdr.cs.leave();
}

trans_cntl_block::~trans_cntl_block()
{
    tcb_hdr.cs.enter();
    trans_cntl_block** bpp;
    for (bpp = &tcb_hdr.chain; *bpp != this; bpp = &(*bpp)->next);
    *bpp = next;
    tcb_hdr.cs.leave();
}

//
// Transaction log manager
//

void log_transaction_manager::complete_transaction(size_t size)
{
    if (size != 0) {
        cs.enter();
        committed_log_size += size;
        //
        // If after decrement n_active_transactions == 1 then only transaction
        // initiated checkpoint process is active now.
        //
        n_active_transactions -= 1;
        if (n_active_transactions <= 1 && checkpoint_in_progress) {
            checkpoint_sem.signal();
        }
        if (backup_wait_flag) {
            backup_sem.signal();
        }
        if (replication_wait_flag) {
            replication_sem.signal();
        }
        cs.leave();
    }
}

void log_transaction_manager::abort_transaction(dbs_transaction_object_header*
                                                toh, trans_obj_info* tp,
                                                int n_trans_objects,
                                                int n_modified_objects,
                                                client_process* client)
{
    int i;
    for (i = 0; i < n_modified_objects; i++) {
        fposi_t pos = toh->get_pos();
        opid_t opid = tp[i].opid;
        size_t size = tp[i].size;
        server->mem_mgr->undo_realloc(opid, size, pos);
        server->obj_mgr->release_object(opid);
        toh = (dbs_transaction_object_header*)((char*)toh+sizeof(*toh)+size);
    }
    for (i = 0; i < n_trans_objects; i++) {
        if (tp[i].flags & tof_validate) {
            server->obj_mgr->unlock_object(tp[i].opid,
                                           tp[i].plock,
                                           client);
        }
    }
}

struct trans_obj_redir {
    int                            i;
    fposi_t                        pos;
    dbs_transaction_object_header* toh;
};

static int compare_obj_offs(void const* a, void const* b)
{
 return ((trans_obj_redir*)a)->pos < ((trans_obj_redir*)b)->pos ? -1
  : ((trans_obj_redir*)a)->pos == ((trans_obj_redir*)b)->pos ? 0 : 1;
}


boolean log_transaction_manager::do_transaction(int n_servers,
                                                dbs_transaction_header* trans,
                                                client_process* client)
{
    msg_buf buf;
    size_t trans_size  = trans->size;
    stid_t coordinator = trans->coordinator;

    dbs_transaction_object_header* toh;

    int i, n_modified_objects = 0;

    dnm_array<trans_obj_info> tobj;
    stid_t trans_servers[MAX_TRANS_SERVERS];
    dbs_request req;

    dbs_object_header* hdr = trans->body();
    if (n_servers > 1) {
        trans_size -= n_servers*sizeof(stid_t);
        char* p = (char*)hdr + trans_size;
        for (i = 0; i < n_servers; i++) {
            p = unpack2((char*)&trans_servers[i], p);
        }
    } else {
        trans_servers[0] = coordinator;
    }
    dbs_object_header* end = (dbs_object_header*)((char*)hdr + trans_size);

    trid_t tid = (coordinator == server->id)
                ? (n_servers > 1)
                  ? get_global_transaction_identifier()
                  : 0 // local transaction
                : trans->tid;

    trans_cntl_block tcb(tcb_hdr, tid, coordinator,
                         n_servers, trans_servers,
                         (coordinator == server->id) ? n_servers-1 : 1);

    if (shutdown_flag) {
        return False;
    }

    if (n_servers > 1 && coordinator == server->id) {
        //
        // I am coordinator of global transaction: return assigned
        // transaction identifier to client.
        //
        req.cmd = dbs_request::cmd_transresult;
        req.result.status = True;
        req.result.tid = tid;
        client->write(&req, sizeof req);
    }
    trans->tid = tid;

    //
    // First pass: lock and validate objects
    //
    size_t cluster_size = 0;
    for (i = 0; hdr < end; i++) {
        opid_t opid = hdr->get_ref();
        int flags = hdr->get_flags();
        size_t size = hdr->get_size();
        cpid_t cpid = hdr->get_cpid();
        if (!((opid >= MIN_CPID && opid <= MAX_CPID && opid == cpid && (flags & tof_change_metadata))
              || (opid >= ROOT_OPID && cpid >= MIN_CPID))            
            || ((flags & tof_update) == 0 && size != 0)
#if CHECK_LEVEL >= 1
            || !server->mem_mgr->verify_object(opid, cpid)
#endif
           )
        { 
            server->message(msg_error|msg_time, 
                            "Invalid object %x (cpid=%x) in transaction\n", opid, cpid);
            while (--i >= 0) {
                if (tobj[i].flags & tof_validate) {
                    server->obj_mgr->unlock_object(tobj[i].opid,
                                                   tobj[i].plock,
                                                   client);
                }
            }
            return False;
        }
#if CHECK_LEVEL >= 2
        int n_refs = (opid != cpid && size >= sizeof(dbs_reference_t))
            ? server->class_mgr->get_number_of_references(cpid, size) : 0;
        char* p = (char*)hdr + sizeof(*hdr);
        while (--n_refs >= 0) {
            opid_t ropid;
            stid_t rsid;
            p = unpackref(rsid, ropid, p);
            if (rsid >= server->get_number_of_servers()
                || (rsid == server->id && !server->mem_mgr->verify_reference(ropid)))
            {
                server->message(msg_error|msg_time, 
                                "Invalid reference field %x:%x in object %x (cpid=%x) in transaction\n", 
                                rsid, ropid, opid, cpid);
                while (--i >= 0) {
                    if (tobj[i].flags & tof_validate) {
                        server->obj_mgr->unlock_object(tobj[i].opid,
                                                       tobj[i].plock,
                                                       client);
                    }
                }
                return False;
            }
        }
#endif

        tobj[i].opid = opid;
        tobj[i].flags = flags;
        tobj[i].size = nat4(size);
        tobj[i].cpid = cpid;
        if (flags & tof_validate) {
            if (!server->obj_mgr->lock_object(opid,
                (flags & tof_update) ? lck_exclusive : lck_shared,
                lckattr_nowait, False, tobj[i].plock, client))
            {
                SERVER_TRACE_MSG((msg_important|msg_locking|msg_warning, "Failed to set %s lock "
                           "on object %x of client '%s'\n",
                           (flags & tof_update) ? "exclusive" : "shared",
                           opid, client->get_name()));
                while (--i >= 0) {
                    if (tobj[i].flags & tof_validate) {
                        server->obj_mgr->unlock_object(tobj[i].opid,
                                                       tobj[i].plock,
                                                       client);
                    }
                }
                return False;
            }
            if (server->obj_mgr->get_object_state(opid, client) == cis_invalid)
            {
                SERVER_TRACE_MSG((msg_important|msg_locking|msg_warning,
                           "Uptodate check failed for object %x "
                           "of client '%s'\n",
                           opid, client->get_name()));
                while (i >= 0) {
                    if (tobj[i].flags & tof_validate) {
                        server->obj_mgr->unlock_object(tobj[i].opid,
                                                       tobj[i].plock,
                                                       client);
                    }
                    i -= 1;
                }
                return False;
            }
        }
        if (flags & tof_update) {
            n_modified_objects += 1;
            if ((opid == cpid || !server->class_mgr->is_external_blob(cpid)) && size <= dynamic_reclustering_limit) {
                cluster_size += DOALIGN(size, allocation_quantum);
            }
        } else {
            assert(size == 0);
			///////////////////////////////////////////
			//We have to verify if the object has an invalid size and
			//in this case we abort the transaction because this operation
			//can shut down the DB Server. With this solution the client sending
			//the transaction will be aborted but the DB Server continues its operation
			//
			//[MC] Added validation to ensure that the transaction is no accepted for size non zero
			if(size)
			{
				TRACE_MSG((msg_error|msg_important|msg_object, 
					   "Invalid object %x included in transaction"
					   "of client '%s'\n",
					   opid, client->get_name()));

				while (i >= 0) { 
					if (tobj[i].flags & tof_validate) { 
						server->obj_mgr->unlock_object(tobj[i].opid, 
									   tobj[i].plock, 
									   client); 
					}
					i--;
				}
				return False;				
			}
			///////////////////////////////////////////

            trans_size -= sizeof(dbs_object_header);
        }
        hdr = (dbs_object_header*)((char*)hdr + sizeof(*hdr) + size);
    }

    //
    // Second pass: allocate space for extended objects and replace headers
    //
    toh = (dbs_transaction_object_header*)trans->body();
    trans_obj_info* tp = &tobj;
    int n = (int)tobj.size();
    fposi_t cluster_pos = 0;
    if (cluster_size > 0) {
        if (!server->mem_mgr->do_realloc(cluster_pos, 0, cluster_size, client)) {
            SERVER_TRACE_MSG((msg_object|msg_important|msg_warning,
                       "Failed to allocate cluster of size %lu\n",
                       cluster_size));
            abort_transaction((dbs_transaction_object_header*)trans->body(),
                              tp, n, 0, client);
            if (client != NULL) {
                client->disconnect();
            }
        }
    }
    for (i = 0; i < n_modified_objects; i++) {
        opid_t opid = tp[i].opid;
        opid_t cpid = tp[i].cpid;
        size_t size = tp[i].size;
        fposi_t pos = EXTERNAL_BLOB_POSITION;

        server->obj_mgr->modify_object(opid);

        if (opid == cpid || !server->class_mgr->is_external_blob(cpid)) {
            if (size <= dynamic_reclustering_limit) {
                pos = cluster_pos;
                size_t aligned_object_size = DOALIGN(size, allocation_quantum);
                cluster_pos += aligned_object_size;
                cluster_size -= aligned_object_size;
            } else {
                dbs_handle hnd;
                server->mem_mgr->get_handle(opid, hnd);
                pos = hnd.get_pos();
                size_t old_size = hnd.get_size();
                if (size > old_size) {
                    if (!server->mem_mgr->do_realloc(pos, old_size, size, client)) {
                        SERVER_TRACE_MSG((msg_object|msg_important|msg_warning,
                                          "Failed to reallocate object "
                                          "from size %lu to %lu\n",
                                          old_size, size));
                        server->obj_mgr->release_object(opid);
                        if (cluster_size > 0) {
                            server->mem_mgr->undo_realloc(0, cluster_size,
                                                          cluster_pos);
                        }
                        abort_transaction((dbs_transaction_object_header*)
                                          trans->body(), tp, n, i, client);
                        if (client != NULL) {
                            client->disconnect();
                        }
                    }
                }
            }
        }
        toh->set(cpid, pos, size, opid);
        toh = (dbs_transaction_object_header*)((char*)toh+sizeof(*toh)+size);
    }
    size_t trans_log_size = 0; // size of transaction in log

    if (trans_size != 0 && logging_enabled) {
        trans->crc = calculate_crc(trans->body(), int(trans_size));
        trans->size = nat4(trans_size); // size now includes only modified objects

        trans_log_size = sizeof(dbs_transaction_header) + trans_size;

        checkpoint_cs.enter();
        cs.enter();
        n_active_transactions += 1;

        //
        // Check available space in log file.
        // Reserve space for log termination record (for raw partitions).
        //
        if (allocated_log_size + sizeof(dbs_transaction_header)
            + trans_log_size > get_log_limit())
        {
            if (n_active_transactions != 1) {
                checkpoint_in_progress = True;
                if (permanent_backup) {
                    server->message(msg_error|msg_time,
                                     "Checkpoint is blocked until permanent "
                                     "backup completion\n");
                }
                checkpoint_sem.wait();
                checkpoint_in_progress = False;
            }
            if (n_active_transactions == 1
                && allocated_log_size + sizeof(dbs_transaction_header)
                + trans_log_size > get_log_limit())
            {
                server->pool_mgr->flush();
                server->mem_mgr->flush();
                if (replication_node_name != NULL) { 
                    replication_cs.enter();
                    if (replication_socket != NULL) { 
                        dbs_transaction_header hdr;
                        hdr.size = dbs_transaction_header::REPL_CHECKPOINT;
                        if (!replication_socket->write(&hdr, sizeof hdr)) { 
                            replication_socket->get_error_text(buf, sizeof buf);
                            server->message(msg_error|msg_time,
                                            "Failed to initiate checkpoint at standby node: %s\n", buf);
                            replication_socket = NULL;
                            delete replication_socket;
                        } else { 
                            int1 status;
                            if (!replication_socket->read(&status, sizeof status)) { 
                                replication_socket->get_error_text(buf, sizeof buf);
                                server->message(msg_error|msg_time,
                                                "Connection with standby node is failed during checkpoint: %s\n", 
                                                buf);
                                replication_socket = NULL;
                                delete replication_socket;
                            } else if (status != 0) { 
                                server->message(msg_error|msg_time, 
                                                "Checkpoint failed at standby node with status %d\n", 
                                                status);
                                replication_socket = NULL;
                                delete replication_socket;
                            } else {
                                checkpoint();
                            }
                        }
                    }
                    replication_cs.leave();
                } else { 
                    checkpoint();
                }
                if (checkpoint_in_schedule) {
                    checkpoint_init_sem.signal();
                }
            }
        }
        allocated_log_size += trans_log_size;
        if (allocated_log_size >= backup_start_log_size) {
            backup_start_log_size = MAX_FSIZE;
            backup_start_sem.signal();
        }
        cs.leave();
        checkpoint_cs.leave();

        trans->seqno = seqno;
        trans->pack();
        
        boolean wait_notification = False;
        if (replication_node_name != NULL) { 
            replication_cs.enter();
            if (replication_socket != NULL) { 
                assert(n_servers == 1); // global transaction can not be replicated
                nat4 ack_flag = dbs_transaction_header::REPL_ACK_NEEDED;
                pack4(ack_flag);
                trans->size += ack_flag;
                if (!replication_socket->write(trans, trans_log_size)) { 
                    replication_socket->get_error_text(buf, sizeof buf);
                    server->message(msg_error|msg_time, "Connection with standby node is broken: %s\n", buf);
                    delete replication_socket;
                    replication_socket = NULL;
                } else { 
                    n_not_confirmed_transactions += 1; 
                    wait_notification = True;
                }
                trans->size -= ack_flag;
            }
            replication_cs.leave();
        }

        file::iop_status status = log.write(trans, trans_log_size);
        if (status != file::ok) {
            log.get_error_text(status, buf, sizeof buf);
            console::error("Failed to write transaction to the log: %s\n",
                           buf);
        }
 
        if (wait_notification) { 
            replication_cs.enter();
            if (replication_socket != NULL) { 
                if (n_not_confirmed_transactions > 0) { 
                    dnm_array<int1> trans_status(n_not_confirmed_transactions);                
                    int1* tsp = &trans_status;
                    if (!replication_socket->read(tsp, n_not_confirmed_transactions)) { 
                        replication_socket->get_error_text(buf, sizeof buf);
                        server->message(msg_error|msg_time, "Connection with standby node is broken: %s\n", buf);
                        delete replication_socket;
                        replication_socket = NULL;
                    } else { 
                        for (int i = (int)n_not_confirmed_transactions; --i >= 0; tsp++) { 
                            if (*tsp != 0) { 
                                server->message(msg_error|msg_time, "Standby node failed to commit transaction, status %d\n", 
                                                *tsp);
                                delete replication_socket;
                                replication_socket = NULL;
                                break;
                            }
                        }
                        n_not_confirmed_transactions = 0;
                    }
                }
            }
            replication_cs.leave();
        }
    }
    if (server->id == coordinator) { // I am coordinator
        if (n_servers > 1) { // Global transaction
            trans_cntl_block::poll_status status =
                tcb.wait_replies(commit_timeout);

            req.tm_sync.cmd = dbs_request::cmd_tmsync;
            req.tm_sync.sid = coordinator;
            req.tm_sync.tid = tid;
            if (status != trans_cntl_block::ts_confirmed) {
                server->message(msg_error|msg_time, "Global transaction %x "
                                 "is aborted\n", tid);
                abort_transaction((dbs_transaction_object_header*)trans->body()
                                  , tp, n, n_modified_objects, client);
                complete_transaction(trans_log_size);
                req.tm_sync.fun = tm_refuse;
                tcb.broadcast_request(server, req);
                return False;
            } else {
                history.set_transaction_status(tid, True);
                req.tm_sync.fun = tm_confirm;
                tcb.broadcast_request(server, req);
            }
        }
    } else { // contact coordinator
        req.tm_sync.cmd = dbs_request::cmd_tmsync;
        req.tm_sync.fun = tm_confirm;
        req.tm_sync.sid = coordinator;
        req.tm_sync.tid = tid;
        server->send(coordinator, &req);
        trans_cntl_block::poll_status status =
            tcb.wait_replies(get_status_timeout);

        while (status == trans_cntl_block::ts_timeout) {
            server->message(msg_error|msg_time,
                             "Trying to obtain status of global transaction %u"
                             " from coordinator %u\n", tid, coordinator);
            req.tm_sync.cmd = dbs_request::cmd_tmsync;
            req.tm_sync.fun = tm_get_status;
            req.tm_sync.tid = tid;
            req.tm_sync.sid = coordinator;
            server->send(coordinator, &req);
            status = tcb.wait_replies(get_status_timeout);
        }
        if (status != trans_cntl_block::ts_confirmed) {
            server->message(msg_error|msg_time, "Global transaction %x is "
                             "aborted by coordinator\n", tid);
            abort_transaction((dbs_transaction_object_header*)trans->body(),
                              tp, n, n_modified_objects, client);
            complete_transaction(trans_log_size);
            return False;
        }
    }
    //
    // Complete transaction
    //
    toh = (dbs_transaction_object_header*)trans->body();
    dnm_array<trans_obj_redir> rd_buf(n_modified_objects);
    trans_obj_redir* rd = &rd_buf;
    for (i = 0; i < n_modified_objects; i++) {
        size_t size = tp[i].size;
        rd[i].i = i;
        rd[i].pos = toh->get_pos();
        rd[i].toh = toh;
        toh = (dbs_transaction_object_header*)((char*)toh+sizeof(*toh)+size);
    }
    qsort(rd, n_modified_objects, sizeof(trans_obj_redir), &compare_obj_offs);
    for (i = 0; i < n_modified_objects; i++) {
        int k = rd[i].i;
        opid_t opid = tp[k].opid;
        cpid_t cpid = tp[k].cpid;
        size_t size = tp[k].size;
        toh = rd[i].toh;
        fposi_t pos = toh->get_pos();
        server->obj_mgr->write_object(opid, cpid, pos, size,
                                      (char*)toh + sizeof(*toh),
                                      client);
        server->mem_mgr->update_handle(opid, cpid, size, pos);
        toh = (dbs_transaction_object_header*)((char*)toh+sizeof(*toh)+size);
    }
    server->notify_clients();

    for (i = 0; i < n_modified_objects; i++) {
        server->obj_mgr->release_object(tp[i].opid);
    }

    for (i = n; --i >= 0;) {
        if (tp[i].flags & tof_validate) {
            server->obj_mgr->unlock_object(tp[i].opid, tp[i].plock, client);
        }
    }
    for (i = n; --i >= 0;) {
        if (tp[i].flags & tof_unlock) {
            server->obj_mgr->unlock_object(tp[i].opid, lck_none, client);
        } else if (tp[i].flags & tof_unlock_exl) {
            server->obj_mgr->unlock_object(tp[i].opid, lck_shared, client);
        }
    }
    complete_transaction(trans_log_size);
    return True;
}



void log_transaction_manager::tm_sync(stid_t sender, dbs_request const& req)
{
    static const char* tm_sync_requests[] = {
        "tm_confirm", "tm_refuse", "tm_checkpoint", "tm_get_status"
    };

    SERVER_TRACE_MSG((msg_request, "tm_sync request from server %d, tid = %lu: %s\n",
               sender, req.tm_sync.tid, tm_sync_requests[req.tm_sync.fun]));

    if (!opened) return;

    switch (req.tm_sync.fun) {
      case tm_confirm:
        trans_cntl_block::receive_reply(tcb_hdr, sender,
                                        req.tm_sync.sid,
                                        req.tm_sync.tid,
                                        True);
        break;
      case tm_refuse:
        trans_cntl_block::receive_reply(tcb_hdr, sender,
                                        req.tm_sync.sid,
                                        req.tm_sync.tid,
                                        False);
        break;
      case tm_checkpoint:
        // Abort uncommitted transactions where this server was participated
        trans_cntl_block::abort_transactions(tcb_hdr, sender, server->id);
        break;
      case tm_get_status:
        //
        // Check if transaction is still active. In this case
        // coordinator will send transaction status to the server latter when
        // transaction will be completed.
        if (!trans_cntl_block::find_active_transaction(tcb_hdr, sender,
                                                       req.tm_sync.sid,
                                                       req.tm_sync.tid))
        {
            dbs_request rep;
            rep.tm_sync.cmd = dbs_request::cmd_tmsync;
            rep.tm_sync.fun =
                history.get_transaction_status(req.tm_sync.tid)
                ? tm_confirm : tm_refuse;
            rep.tm_sync.sid = server->id;
            rep.tm_sync.tid = req.tm_sync.tid;
            server->send(sender, &rep);
        }
        break;
      default:
        server->message(msg_error|msg_time,
                         "Illegal request to transaction manager: %d\n",
                         req.tm_sync.fun);
    }
}

void log_transaction_manager::initialize()
{
    initialized = True;
    if (checkpoint_period != 0) {
        checkpoint_term_event.reset();
        task::create(start_checkpoint_process, this);
    }
}

boolean log_transaction_manager::open(dbs_server* server)
{
    msg_buf buf;
    this->server = server;
    allocation_quantum = server->mem_mgr->get_allocation_quantum();

    if (!history.open()) {
        return False;
    }

    file::iop_status status = log.open(file::fa_readwrite,
                                       sync_log_write
                                       ? file::fo_sync|file::fo_create
                                       : file::fo_create);
    if (status != file::ok) {
        log.get_error_text(status, buf, sizeof buf);
        server->message(msg_error, "Failed to open log file: %s\n", buf);
        history.close();
        return False;
    }
    fsize_t size = 0;
    size_t n_recovered_transactions = 0;
    restored_object_tree = NULL;
    restored_object_tree_depth = 0;

    opened = True;
    recovery_after_crash = False;
    if ((backup_log_file == NULL
         || recovery(*backup_log_file, size, n_recovered_transactions))
        && recovery(log, size, n_recovered_transactions))
    {
        if (recovery_after_crash) {
            server->message(msg_notify, "Perform recovery after crash\n");
            if (n_recovered_transactions > 0) {
                server->message(msg_notify, "Recover %lu transactions\n",
                                 n_recovered_transactions);
                restored_object_pool.reset();
            }
            server->mem_mgr->recovery(1);
            if (permanent_backup) {
                status = log.set_position(size);
                if (status != file::ok) {
                    log.get_error_text(status, buf, sizeof buf);
                    console::error("Failed to set position in log file: %s\n",
                                   buf);
                }
                allocated_log_size = committed_log_size = size;
            } else {
                server->pool_mgr->flush();
                server->mem_mgr->flush();
                if (replication_node_name == NULL) { 
                    allocated_log_size = committed_log_size = 0;
                    checkpoint();
                } else { 
                    allocated_log_size = committed_log_size = size;
                }
           }
        } else {
            if (replication_node_name == NULL || size == 0) { 
                allocated_log_size = committed_log_size = 0;
                checkpoint();
            } else { 
                allocated_log_size = committed_log_size = size;
            }
        }
        notify_coordinators();
        checkpoint_in_progress = False;
        n_active_transactions =
            permanent_backup ? 1 : 0; // avoid checkpoints before end of backup
        backup_wait_flag = False;
        replication_wait_flag = False;
        backup_in_schedule = False;
        backup_in_progress = False;
        backup_start_log_size = MAX_FSIZE;
        shutdown_flag = False;
        initialized = False;
        backup_log_file = NULL;
        last_checkpoint_time = 0;
        last_backup_time = 0;
        checkpoint_in_schedule = False;
        return True;
    } else {
        backup_log_file = NULL;
        opened = False;
        log.close();
        history.close();
        return False;
    }
}

void task_proc log_transaction_manager::start_checkpoint_process(void* arg)
{
    ((log_transaction_manager*)arg)->checkpoint_process();
}


void log_transaction_manager::checkpoint_process()
{
    do {
        if (checkpoint_period != 0) {
            checkpoint_in_schedule = True;
            if (checkpoint_init_sem.wait_with_timeout(checkpoint_period)) {
                checkpoint_in_schedule = False;
                continue;
            }
            checkpoint_in_schedule = False;
        }
        checkpoint_cs.enter();
        cs.enter();
        while (n_active_transactions != 0) {
            checkpoint_in_progress = True;
            checkpoint_sem.wait();
            checkpoint_in_progress = False;
        }
        server->pool_mgr->flush();
        server->mem_mgr->flush();
        checkpoint();
        cs.leave();
        checkpoint_cs.leave();
    } while (checkpoint_period != 0);

    checkpoint_term_event.signal();
}


void log_transaction_manager::set_checkpoint_period(time_t period)
{
    if (checkpoint_period == 0) {
        //
        // No checkpoint process was activated
        //
        if (period == 0) {
            checkpoint_process();
        } else {
            checkpoint_period = period;
            checkpoint_term_event.reset();
            task::create(start_checkpoint_process, this);
        }
    } else {
        checkpoint_period = period;
        checkpoint_init_sem.signal();
        if (period == 0) {
            checkpoint_term_event.wait();
        }
    }
}

void log_transaction_manager::set_dynamic_reclustering_limit(size_t max_size)
{
    dynamic_reclustering_limit = max_size;
}

void log_transaction_manager::shutdown()
{
    shutdown_flag = True;
    if (checkpoint_period != 0) {
        checkpoint_period = 0;
        checkpoint_init_sem.signal();
        checkpoint_term_event.wait();
    }
    if (server != NULL) {
        trans_cntl_block::cancel_all_transactions(tcb_hdr, server->id);
    }
    if (backup_in_schedule) {
        backup_start_sem.signal();
    }
}

void log_transaction_manager::checkpoint()
{
    msg_buf buf;
    file::iop_status status;
    internal_assert(allocated_log_size == committed_log_size);

    status = log.set_size(preallocated_log_size);
    if (status != file::ok) {
        log.get_error_text(status, buf, sizeof buf);
        server->message(msg_error, "Log truncation failed: %s\n", buf);
    }
    status = log.set_position(0);
    if (status != file::ok) {
        log.get_error_text(status, buf, sizeof buf);
        console::error("Log set position failed: %s\n", buf);
    }
    log_header lh;
    lh.last_tid = ++seqno;
    lh.normal_completion = !opened; // if checkpoint() is called from close
    lh.pack();
    status = log.write(&lh, sizeof lh);
    if (status != file::ok) {
        log.get_error_text(status, buf, sizeof buf);
        console::error("Log initialization failed: %s\n", buf);
    }
    if (!sync_log_write) { 
        log.flush();
    }
    committed_log_size = allocated_log_size = sizeof lh;
    server->message(msg_notify|msg_time, "Checkpoint %d finished%s\n", seqno,
                     opened ? "" : " (server shutdown)");
    last_checkpoint_time = time(NULL);
}

void log_transaction_manager::notify_coordinators()
{
    for (int i = server->id; --i >= 0; ) {
        dbs_request req;
        req.tm_sync.cmd = dbs_request::cmd_tmsync;
        req.tm_sync.fun = tm_checkpoint;
        req.tm_sync.seqno = seqno;
        server->send(i, &req);
    }
}

//Check if a transaction can be redone
//verify that the class persistent id of the object is in range
//[MC] Added extra validation to macke sure all objects in transaction have the correct cpid
static boolean can_redo_transaction(dnm_buffer& buf)
{
    char* src = &buf;
    char* end = src + buf.size(); 

    while (src < end) 
	{ 
		dbs_transaction_object_header* hdr = 
			(dbs_transaction_object_header*)src; 
		cpid_t cpid = hdr->get_cpid();
		size_t size = hdr->get_size();
		src += sizeof *hdr; 
		src += size;
		if(!(cpid >= MIN_CPID && cpid <= MAX_CPID))
			return false;
    }
    internal_assert(src == end);
	return src == end;
}

boolean log_transaction_manager::recovery(file& log, fsize_t& size,
                                          size_t& n_recovered_transactions)
{
    file::iop_status rc;
    dnm_buffer redo_buf;
    dbs_transaction_header hdr;
    log_header lh;
    msg_buf buf;

    size = 0;
    seqno = 0;
    rc = log.read(&lh, sizeof lh);

    if (rc == file::ok) {
        lh.unpack();
        seqno = lh.last_tid;
        size = sizeof lh;
        if (!lh.normal_completion) {
            if (!recovery_after_crash) {
                server->mem_mgr->recovery(0);
                recovery_after_crash = True;
            }
            while ((rc=log.read(&hdr, sizeof(dbs_transaction_header))) == file::ok)
            {
                hdr.unpack();
                size_t trans_size = hdr.size;
                if (hdr.seqno != seqno || trans_size == 0 ||
                    trans_size > MAX_TRANSACTION_SIZE)
                {
                    server->message(msg_error,
                                     "Failed to recover transaction with seqno=%lu"
                                     ", size=%lu, current seqno=%lu\n",
                                     hdr.seqno, trans_size, seqno);
                    break; // ignore rest of log
                }
				//[MC] Changed from msg_important to msg_none
                server->message(msg_none,
                                 "Length of restored transaction: %ld bytes\n",
                                 trans_size);
                redo_buf.put(trans_size);
                if ((rc = log.read(&redo_buf, trans_size)) != file::ok
                    || calculate_crc(&redo_buf, int(trans_size)) != hdr.crc)
                {
                    if (rc != file::ok && rc != file::end_of_file) {
                        log.get_error_text(rc, buf, sizeof buf);
                        server->message(msg_error, "Log read failed: %s\n", buf);
                        return False;
                    }
                    if (rc == file::end_of_file) {
                        server->message(msg_error,
                                         "Transaction record was not completely "
                                         "written: ignoring rest of log file.\n");
                        rc = file::ok;
                    } else {
                        server->message(msg_error,
                                         "Checksum error: ignoring rest of log "
                                         "file...\n");
                    }
                    break;
                }
                size += sizeof(hdr) + trans_size;
                if (hdr.tid != 0) { // global transaction
                    if (hdr.coordinator == server->id) {
                        if (!history.get_transaction_status(hdr.tid)) {
                            server->message(msg_notify, "Skip globally aborted "
                                             "transaction %x\n", hdr.tid);
                            continue; // skip this transaction
                        }
                    } else {
                        trans_cntl_block tcb(tcb_hdr, hdr.tid, hdr.coordinator,
                                             0, NULL, 1);
                        trans_cntl_block::poll_status status;
                        do {
                            dbs_request req;
                            req.tm_sync.cmd = dbs_request::cmd_tmsync;
                            req.tm_sync.fun = tm_get_status;
                            req.tm_sync.tid = hdr.tid;
                            server->send(hdr.coordinator, &req);
                            status = tcb.wait_replies(get_status_timeout);
                            if (status == trans_cntl_block::ts_timeout) {
                                server->message(msg_error|msg_time,
                                                 "Coordinator is unreachable...\n");
                            }
                        } while (status == trans_cntl_block::ts_timeout);

                        if (status == trans_cntl_block::ts_refused) {
                            server->message(msg_notify, "Skip globally aborted "
                                             "transaction %x\n", hdr.tid);
                            continue; // skip this transaction
                        }
                        assert(status == trans_cntl_block::ts_confirmed);
                    }
                }
				//Check first if the transaction can be redone
				//This code was added to fix a bug when recovering
				//because the object cpid came out of range
				//[MC] Added extra validation when processing recovered transactions
				if(can_redo_transaction(redo_buf))
				{
					n_recovered_transactions += 1;
					redo_transaction(redo_buf);
				}

            }
        }
        if (rc != file::end_of_file && rc != file::ok) {
            log.get_error_text(rc, buf, sizeof buf);
            server->message(msg_error, "Log read failed: %s\n", buf);
            return False;
        } else if (rc != file::end_of_file) { 
            log.set_position(size);
        }
    }
    if (replication_node_name != NULL) { 
        return synchronize_state(size, n_recovered_transactions);
    }
    return True;
}


boolean log_transaction_manager:: synchronize_state(fsize_t& recovered_size, size_t& n_recovered_transactions)
{
    msg_buf buf;
    dbs_request req;
    dnm_buffer redo_buf;
    file::iop_status rc;
    dbs_transaction_header hdr;
    replication_socket = socket_t::connect(replication_node_name, socket_t::sock_any_domain, 1, 0);
    if (replication_socket == NULL || !replication_socket->is_ok()) { 
        server->message(msg_notify, "Replication node '%s' is not accessible, starting as primary node\n", 
                        replication_node_name);
        delete replication_socket;
        replication_socket = NULL;
        return True;
    }
    boolean status = True;
    server->message(msg_notify, "Standby node is connected to primary node '%s', log offset=%" INT8_FORMAT "d\n",
                    replication_node_name, recovered_size);
    req.sync.cmd = dbs_request::cmd_synchronize;
    req.sync.log_offs_high = nat8_high_part(recovered_size);
    req.sync.log_offs_low = nat8_low_part(recovered_size);
    req.pack();
    
    int n_replicated_transactions = 0;
    log_header lh;
    if (!replication_socket->write(&req, sizeof req)) { 
        replication_socket->get_error_text(buf, sizeof buf);
        server->message(msg_notify, "Failed to send request to primary node: %s\n", buf);
    } else { 
        if (recovered_size == 0 && !replication_socket->read(&lh, sizeof lh)) { 
            replication_socket->get_error_text(buf, sizeof buf);
            server->message(msg_notify, "Failed to read log header sent by primary node: %s\n", buf);
        } else { 
            if (recovered_size == 0) { 
                if ((rc = log.write(&lh, sizeof lh)) != file::ok) { 
                    log.get_error_text(rc, buf, sizeof buf);
                    console::error("Failed to write transaction to the log: %s\n", buf);
                }
                lh.unpack();
                recovered_size = sizeof lh;
                seqno = lh.last_tid;
            }
            while (True)  { 
                if (!replication_socket->read(&hdr, sizeof(dbs_transaction_header))) { 
                    replication_socket->get_error_text(buf, sizeof buf);
                    server->message(msg_notify, "Failed to read header of transaction sent by primary node: %s\n", buf);
                    break;
                }
                if (!recovery_after_crash) { 
                    server->mem_mgr->recovery(0);
                    recovery_after_crash = True;
                }
                hdr.unpack();
                size_t trans_size = hdr.size;
                if (trans_size == dbs_transaction_header::REPL_CHECKPOINT) { // checkpoint
                    server->pool_mgr->flush();
                    server->mem_mgr->flush();
                    checkpoint();
                    int1 checkpoint_status = 0;
                    if (!replication_socket->write(&checkpoint_status, sizeof checkpoint_status)) { 
                        replication_socket->get_error_text(buf, sizeof buf);
                        server->message(msg_notify, "Failed to send checkout status to primary node: %s\n", buf);
                        break;
                    }                
                } else if (trans_size == dbs_transaction_header::REPL_SHUTDOWN) { 
                    // normal primary node termination
                    server->message(msg_notify, "Primary node is terminated\n");            
                    if (n_recovered_transactions > 0) {
                        restored_object_pool.reset();
                    }
                    server->mem_mgr->recovery(1);
                    server->pool_mgr->flush();
                    server->mem_mgr->flush();
                    opened = False;
                    checkpoint();
                    status = False;
                    int1 checkpoint_status = 0;
                    replication_socket->write(&checkpoint_status, sizeof checkpoint_status);
                    break;
                } else {
                    boolean ack_needed = False;
                    if (trans_size & dbs_transaction_header::REPL_ACK_NEEDED) { 
                        ack_needed = True;
                        hdr.size = nat4(trans_size &= ~dbs_transaction_header::REPL_ACK_NEEDED);
                    }
                    if (hdr.seqno != seqno) { 
                        server->message(msg_notify, "Log sequence mismatch: primary node seqno=%u, transaction size %u, log offset=%" INT8_FORMAT "d, standby node seqno=%u\n",
                                        hdr.seqno, trans_size, recovered_size, seqno);
                        break;
                    }
                    assert (hdr.tid == 0); // global transactions can not be replicated
                    redo_buf.put(trans_size);
                    if (!replication_socket->read(&redo_buf, trans_size)) { 
                        replication_socket->get_error_text(buf, sizeof buf);
                        server->message(msg_notify, "Failed to read body of transaction sent by primary node: %s\n", buf);
                        break;
                    }
                    if (ack_needed) { 
                        int1 transaction_status = 0;
                        if (!replication_socket->write(&transaction_status, sizeof transaction_status)) { 
                            replication_socket->get_error_text(buf, sizeof buf);
                            server->message(msg_notify, "Failed to send transaction status to primary node: %s\n", 
                                            buf);
                            break;
                        }
                    }
                      
                    hdr.pack();
                    if ((rc = log.write(&hdr, sizeof(hdr))) != file::ok
                        || (rc = log.write(&redo_buf, trans_size)) != file::ok) 
                    { 
                        log.get_error_text(rc, buf, sizeof buf);
                        console::error("Failed to write transaction to the log: %s\n", buf);
                    }
                    recovered_size += sizeof(hdr) + trans_size;
                    n_recovered_transactions += 1;
                    n_replicated_transactions += 1;
                    redo_transaction(redo_buf);
                }
            }
        }
    }
    server->on_replication_master_crash();
    server->message(msg_notify, "Replay %d transaction from primary node\n", n_replicated_transactions);
    delete replication_socket;
    replication_socket = NULL;
    return status;
}

void log_transaction_manager::start_replication(socket_t* replication_socket, fsize_t log_offset)
{
    critical_section CS(replication_cs);
    msg_buf buf;
    file::iop_status status;
    if (this->replication_socket != NULL) { 
        server->message(msg_error|msg_time, "Unexpected connection from standby server\n");
        delete replication_socket;
        return;
    }
    file* log_file = log.clone();
    status = log_file->open(file::fa_read, 0);
    if (status != file::ok) { 
        log_file->get_error_text(status, buf, sizeof buf);
        server->message(msg_error|msg_time, "Failed to open log file: %s\n", buf);
        delete replication_socket;
        return;
    }
    status = log_file->set_position(log_offset);
    if (status != file::ok) {
        log_file->get_error_text(status, buf, sizeof buf);
        server->message(msg_error|msg_time, "Failed to set log on the specfied position %ld: %s\n", 
                        (long)log_offset, buf);
        delete replication_socket;
        return;
    }
        
    while (True) { 
        cs.enter();
        if (log_offset >= allocated_log_size) {
            //
            // no more transactions in the log to be replicated
            //
            n_not_confirmed_transactions = 0;
            this->replication_socket = replication_socket;
            cs.leave();
            log_file->close();
            delete log_file;
            break;
        }
        size_t replication_log_quant;
        while (True) {
            replication_log_quant = (committed_log_size - log_offset > sizeof(replication_buf))
                ? sizeof(replication_buf)
                 : size_t(committed_log_size - log_offset);
            if (replication_log_quant == 0) {
                replication_wait_flag = True;
                replication_sem.wait();
                replication_wait_flag = False;
            } else {
                break;
            }
        }
        cs.leave();
        status = log_file->read(replication_buf, replication_log_quant);
        if (status != file::ok) {
            log_file->get_error_text(status, buf, sizeof buf);
            server->message(msg_error|msg_time,
                            "Failed to read log file: %s\n", buf);
            delete replication_socket;
            return;
        } else { 
            if (!replication_socket->write(replication_buf, replication_log_quant)) { 
                replication_socket->get_error_text(buf, sizeof buf);
                server->message(msg_error|msg_time,
                                "Failed to send transaction to standby client: %s\n", buf);
                delete replication_socket;
                return;
            }
            log_offset += replication_log_quant;
        }
    }
}

void log_transaction_manager::close()
{
    cs.enter();    
    if (opened) {        
        opened = False;
        if (replication_node_name != NULL) { 
            replication_cs.enter();
            if (replication_socket != NULL) { 
                msg_buf buf;
                dbs_transaction_header hdr;
                hdr.size = dbs_transaction_header::REPL_SHUTDOWN;
                if (!replication_socket->write(&hdr, sizeof hdr)) { 
                    replication_socket->get_error_text(buf, sizeof buf);
                    server->message(msg_notify, "Failed to send shutdown request to standby node: %s\n", buf);
                } else { 
                    int1 status;
                    if (!replication_socket->read(&status, sizeof status)) { 
                        replication_socket->get_error_text(buf, sizeof buf);
                        server->message(msg_notify, "Failed to redeive shutdown status from primary node: %s\n", buf);
                    } else if (status != 0) { 
                        server->message(msg_notify, "Standby node shutdown failed with status: %d\n", status);
                    } else { 
                        checkpoint();
                    }
                }                
                delete replication_socket;
                replication_socket = NULL;
            }                
            replication_cs.leave();
        } else { 
            checkpoint();
        }
        log.close();
        history.close();
        backup_log_file = NULL;
    }
    cs.leave();
}

inline void log_transaction_manager::intersect(restored_object* rp,
                                               opid_t  opid,
                                               fposi_t pos,
                                               size_t  size)
{
    if (rp->opid != 0
        && (opid != rp->opid || size != rp->size || pos != rp->pos)
        && rp->pos < pos + size && pos < rp->pos + rp->size)
    {
        server->mem_mgr->revoke_object(rp->opid, rp->pos, rp->size, opid, pos, size);
        rp->opid = 0;
    }
}

void log_transaction_manager::revoke_overlapped_objects(restored_object* rp,
                                                        opid_t  opid,
                                                        fposi_t pos,
                                                        size_t  size)
{
    intersect(rp, opid, pos, size);
    if (rp->left) {
        revoke_overlapped_objects(rp->left, opid, pos, size);
    }
    if (rp->right) {
        revoke_overlapped_objects(rp->right, opid, pos, size);
    }
}


void log_transaction_manager::find_overlapped_objects(opid_t opid,
                                                      fposi_t pos,
                                                      size_t size)
{
    static nat8 pow2[] = {
        CONST64(0x0000000000000001), CONST64(0x0000000000000002),
        CONST64(0x0000000000000004), CONST64(0x0000000000000008),
        CONST64(0x0000000000000010), CONST64(0x0000000000000020),
        CONST64(0x0000000000000040), CONST64(0x0000000000000080),
        CONST64(0x0000000000000100), CONST64(0x0000000000000200),
        CONST64(0x0000000000000400), CONST64(0x0000000000000800),
        CONST64(0x0000000000001000), CONST64(0x0000000000002000),
        CONST64(0x0000000000004000), CONST64(0x0000000000008000),
        CONST64(0x0000000000010000), CONST64(0x0000000000020000),
        CONST64(0x0000000000040000), CONST64(0x0000000000080000),
        CONST64(0x0000000000100000), CONST64(0x0000000000200000),
        CONST64(0x0000000000400000), CONST64(0x0000000000800000),
        CONST64(0x0000000001000000), CONST64(0x0000000002000000),
        CONST64(0x0000000004000000), CONST64(0x0000000008000000),
        CONST64(0x0000000010000000), CONST64(0x0000000020000000),
        CONST64(0x0000000040000000), CONST64(0x0000000080000000),
        CONST64(0x0000000100000000), CONST64(0x0000000200000000),
        CONST64(0x0000000400000000), CONST64(0x0000000800000000),
        CONST64(0x0000001000000000), CONST64(0x0000002000000000),
        CONST64(0x0000004000000000), CONST64(0x0000008000000000),
        CONST64(0x0000010000000000), CONST64(0x0000020000000000),
        CONST64(0x0000040000000000), CONST64(0x0000080000000000),
        CONST64(0x0000100000000000), CONST64(0x0000200000000000),
        CONST64(0x0000400000000000), CONST64(0x0000800000000000),
        CONST64(0x0001000000000000), CONST64(0x0002000000000000),
        CONST64(0x0004000000000000), CONST64(0x0008000000000000),
        CONST64(0x0010000000000000), CONST64(0x0020000000000000),
        CONST64(0x0040000000000000), CONST64(0x0080000000000000),
        CONST64(0x0100000000000000), CONST64(0x0200000000000000),
        CONST64(0x0400000000000000), CONST64(0x0800000000000000),
        CONST64(0x1000000000000000), CONST64(0x2000000000000000),
        CONST64(0x4000000000000000), CONST64(0x8000000000000000)
    };
    int i;
    fposi_t end = pos + size - 1;
    for (i = 0; end >= pow2[i]; i++);

    restored_object** rpp = &restored_object_tree;
    int n = restored_object_tree_depth;
    if (i > n) {
        restored_object_tree_depth = i;
        if (*rpp != NULL) {
            do {
                restored_object* rp =
                    new (restored_object_pool) restored_object;
                rp->left = *rpp;
                rp->right = NULL;
                rp->opid = 0;
                *rpp = rp;
            } while (++n < i);
        }
    } else {
        i = n;
    }
    while (True) {
        restored_object* rp = *rpp;
        if (rp == NULL) {
            *rpp = rp = new (restored_object_pool) restored_object;
            rp->left = NULL;
            rp->right = NULL;
            rp->opid = 0;
        } else {
            intersect(rp, opid, pos, size);
        }
        i -= 1;
        if (i >= 0 && (pos & pow2[i]) != 0) {
            rpp = &rp->right;
        } else if (i >= 0 && (end & pow2[i]) == 0) {
            rpp = &rp->left;
        } else {
            rp->opid = opid;
            rp->size = size;
            rp->pos = pos;
            restored_object* lp = rp->left;
            int j = i;
            while (lp != NULL && (pos & pow2[--j]) != 0) {
                intersect(lp, opid, pos, size);
                lp = lp->right;
            }
            if (lp != NULL) {
                revoke_overlapped_objects(lp, opid, pos, size);
            }
            rp = rp->right;
            while (rp != NULL && (end & pow2[--i]) == 0) {
                intersect(rp, opid, pos, size);
                rp = rp->left;
            }
            if (rp != NULL) {
                revoke_overlapped_objects(rp, opid, pos, size);
            }
            break;
        }
    }
}

void log_transaction_manager::redo_transaction(dnm_buffer& buf)
{
    char* src = &buf;
    char* end = src + buf.size();

    while (src < end) {
        dbs_transaction_object_header* hdr =
            (dbs_transaction_object_header*)src;
        opid_t opid = hdr->get_opid();
        cpid_t cpid = hdr->get_cpid();
        size_t size = hdr->get_size();
        fposi_t pos = hdr->get_pos();
        src += sizeof *hdr;

        internal_assert(cpid >= MIN_CPID);
        if (size > 0) {
            if (opid != cpid && pos == EXTERNAL_BLOB_POSITION) { // server->class_mgr->is_external_blob() is not avaiable at recovery stage
                server->write_external_blob(opid, src, size);
            } else { 
                //
                // Find restored objects overridden by this object
                //
                find_overlapped_objects(opid, pos, size);
                server->pool_mgr->write(pos, src, size);
            }
        }
        server->mem_mgr->confirm_alloc(opid, cpid, size, pos);
        src += size;
    }
    internal_assert(src == end);
}

void log_transaction_manager::dump(const char* what)
{
    static const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    console::output("Transaction log size: %" INT8_FORMAT "u\n"
                    "Number of passed checkpoints: %u\n"
                    "Number of active transactions: %u\n",
                    allocated_log_size,
                    seqno, n_active_transactions);
    if (last_backup_time != 0) {
        struct tm* tp = localtime(&last_backup_time);
        console::output(
            "Last backup completion time %02u:%02u.%02u %02u-%s-%u\n",
            tp->tm_hour, tp->tm_min, tp->tm_sec,
            tp->tm_mday, months[tp->tm_mon], 1900 + tp->tm_year);
    }
    if (last_checkpoint_time != 0) {
        struct tm* tp = localtime(&last_checkpoint_time);
        console::output(
            "Last checkpoint completion time %02u:%02u.%02u %02u-%s-%u\n",
            tp->tm_hour, tp->tm_min, tp->tm_sec,
            tp->tm_mday, months[tp->tm_mon], 1900 + tp->tm_year);
    }
    log.dump();
    history.dump(what);
}

void log_transaction_manager::stop_backup()
{
    server->pool_mgr->stop_backup();
    server->mem_mgr->stop_backup();
    if (backup_in_schedule) {
        backup_start_sem.signal();
    }
    backup_sem.signal();
}

void log_transaction_manager::set_backup_type(backup_type online_backup_type)
{
    cs.enter();
    if (permanent_backup && online_backup_type == bck_snapshot) {
        permanent_backup = False;
        n_active_transactions -= 1;
        if (n_active_transactions == 1 && checkpoint_in_progress) {
            checkpoint_sem.signal();
        }
    } else if (!permanent_backup && online_backup_type == bck_permanent) {
        permanent_backup = True;
        n_active_transactions += 1;
    }
    cs.leave();
}


void log_transaction_manager::abort_backup(file* backup_file, file* log_file)
{
    cs.enter();
    backup_in_progress = False;
    n_active_transactions -= 1; 
    if (n_active_transactions <= 1 && checkpoint_in_progress) {
        checkpoint_sem.signal();
    }
    cs.leave();
    backup_file->close();
    delete log_file;
}

boolean log_transaction_manager::backup(file& backup_file,
                                        time_t start_backup_delay,
                                        fsize_t start_backup_log_size,
                                        page_timestamp_t& page_timestamp)
{
    msg_buf buf;
    file::iop_status status;

    backup_start_log_size = start_backup_log_size;

    //
    // Reset semaphore to non-signaled state
    //
    while (backup_start_sem.wait_with_timeout(0));

    backup_in_schedule = True;
    if (!server->backup_started) {
        backup_in_schedule = False;
        return False;
    }
    if (!shutdown_flag) {
        backup_start_sem.wait_with_timeout(start_backup_delay);
    }
    backup_in_schedule = False;

    if (!server->backup_started) {
        return False;
    }
    status = backup_file.open(file::fa_write,
                              file::fo_largefile|file::fo_truncate|file::fo_create);
    if (status != file::ok) {
        backup_file.get_error_text(status, buf, sizeof buf);
        server->message(msg_error|msg_time,
                         "Failed to open backup file: %s\n", buf);
        return False;
    }
    cs.enter();
    backup_in_progress = True;
    n_active_transactions += 1; // prevent checkpoints during backup
    cs.leave();
    if (!server->pool_mgr->backup(backup_file, page_timestamp)
        || !server->mem_mgr->backup(backup_file)
        || !server->save_credentials(backup_file))

    {
        abort_backup(&backup_file, NULL);
        return False;
    }
    file* log_file = log.clone();
    status = log_file->open(file::fa_read, 0);
    if (status != file::ok) {
        log_file->get_error_text(status, buf, sizeof buf);
        server->message(msg_error|msg_time,
                         "Failed to open log file: %s\n", buf);
        abort_backup(&backup_file, log_file);
        return False;
    }
    fposi_t pos = 0;
    fposi_t last_log_pos = committed_log_size; // high bound for snapshot

    while (server->backup_started) {
        size_t backup_log_quant = 0;

        if (permanent_backup) {
            cs.enter();
            if (pos == allocated_log_size) {
                //
                // Log is completely backuped now
                //
                status = backup_file.flush();
                if (status != file::ok) {
                    backup_file.get_error_text(status, buf, sizeof buf);
                    server->message(msg_error|msg_time,
                                     "Failed to flush backup file: %s\n", buf);
                }
                server->pool_mgr->flush();
                server->mem_mgr->flush();
                checkpoint();
                backup_in_progress = False;
                n_active_transactions -= 1;
                if (checkpoint_in_schedule) {
                    checkpoint_init_sem.signal();
                }

                if (permanent_backup) {
                    internal_assert(n_active_transactions == 1 ||
                      (n_active_transactions == 2 && checkpoint_in_progress));
                } else {
                    internal_assert(n_active_transactions == 0 ||
                      (n_active_transactions == 1 && checkpoint_in_progress));
                }
                if (checkpoint_in_progress) {
                    checkpoint_sem.signal();
                }
                page_timestamp = server->pool_mgr->get_last_page_timestamp();
                cs.leave();
                backup_file.close();
                log_file->close();
                delete log_file;
                last_backup_time = time(NULL);
                return True;
            } else {
                while (server->backup_started) {
                    backup_log_quant =
                        (committed_log_size - pos > sizeof(backup_buf))
                        ? sizeof(backup_buf)
                        : size_t(committed_log_size - pos);
                    if (backup_log_quant == 0) {
                        backup_wait_flag = True;
                        backup_sem.wait();
                        backup_wait_flag = False;
                    } else {
                        break;
                    }
                }
                cs.leave();
                if (!server->backup_started) {
                    break;
                }
            }
        } else {
            if (pos == last_log_pos) {
                status = backup_file.flush();
                if (status != file::ok) {
                    backup_file.get_error_text(status, buf, sizeof buf);
                    server->message(msg_error|msg_time,
                                     "Failed to flush backup file: %s\n", buf);
                }
                cs.enter();
                backup_in_progress = False;
                n_active_transactions -= 1; // allow checkpoints

                if (n_active_transactions <= 1 && checkpoint_in_progress) {
                    checkpoint_sem.signal();
                }
                page_timestamp = server->pool_mgr->get_last_page_timestamp();
                cs.leave();
                backup_file.close();
                log_file->close();
                delete log_file;
                last_backup_time = time(NULL);
                return True;
            } else {
                backup_log_quant =
                    (last_log_pos - pos > sizeof(backup_buf))
                    ? sizeof(backup_buf)
                    : size_t(last_log_pos - pos);
            }
        }
        status = log_file->read(backup_buf, backup_log_quant);
        if (status != file::ok) {
            backup_file.get_error_text(status, buf, sizeof buf);
            server->message(msg_error|msg_time,
                             "Failed to read log file: %s\n", buf);
            abort_backup(&backup_file, log_file);
            return False;
        }
        status = backup_file.write(backup_buf, backup_log_quant);
        if (status != file::ok) {
            backup_file.get_error_text(status, buf, sizeof buf);
            server->message(msg_error|msg_time,
                             "Failed to write log file: %s\n", buf);
            abort_backup(&backup_file, log_file);
            return False;
        }
        pos += backup_log_quant;
    }
    abort_backup(&backup_file, log_file);
    return False;
}

boolean log_transaction_manager::restore(file& backup_file)
{
    file::iop_status status = backup_file.open(file::fa_read, file::fo_largefile);

    if (status != file::ok) {
        msg_buf buf;
        backup_file.get_error_text(status, buf, sizeof buf);
        server->message(msg_error|msg_time,
                         "Failed to open backup file: %s\n", buf);
        return False;
    }

    if (!server->pool_mgr->restore(backup_file) ||
        !server->mem_mgr->restore(backup_file) ||
        !server->restore_credentials(backup_file))
    {
        return False;
    }
    backup_log_file = &backup_file;
    return True;
}

void log_transaction_manager::set_log_size_limit(fsize_t log_limit)
{
    this->log_limit = log_limit;
}

void log_transaction_manager::set_log_size_limit_for_backup(fsize_t log_limit)
{
    this->log_limit_for_backup = log_limit;
}

void log_transaction_manager::set_preallocated_log_size(fsize_t init_log_size)
{
    preallocated_log_size = init_log_size;
}

void log_transaction_manager::set_global_transaction_commit_timeout(time_t
                                                                    timeout)
{
    commit_timeout = timeout;
}

void log_transaction_manager::set_global_transaction_get_status_timeout(time_t
                                                                      timeout)
{
    get_status_timeout = timeout;
}

void log_transaction_manager::control_logging(boolean enabled)
{
    logging_enabled = enabled;
}


log_transaction_manager::log_transaction_manager(
        file&           log_file,
        file&           history_file,
        char const*     replication_node_name, 
        boolean         sync_write,
        backup_type     online_backup_type,
        fsize_t         trans_log_limit,
        fsize_t         trans_log_limit_for_backup,
        fsize_t         init_log_size,
        time_t          checkpoint_init_period,
        time_t          wait_commit_timeout,
        time_t          retry_get_status_timeout,
        size_t          reclustering_limit, 
	boolean         logging_switched_on)
:
    log(log_file),
    history(history_file, sync_write),
    checkpoint_sem(cs),
    checkpoint_period(checkpoint_init_period),
    commit_timeout(wait_commit_timeout),
    get_status_timeout(retry_get_status_timeout),
    preallocated_log_size(init_log_size),
    log_limit(trans_log_limit),
    log_limit_for_backup(trans_log_limit_for_backup),
    sync_log_write(sync_write),
    backup_sem(cs),
    replication_sem(cs),
    dynamic_reclustering_limit(reclustering_limit),
    logging_enabled(logging_switched_on),
    restored_object_pool(sizeof(restored_object)) 
{
    opened = False;
    if (replication_node_name != NULL && *replication_node_name != '\0') { 
        this->replication_node_name = strdup(replication_node_name);
    } else { 
        this->replication_node_name = NULL;
    }
    n_not_confirmed_transactions = 0;
    replication_socket = NULL;
    backup_log_file = NULL;
    permanent_backup = (online_backup_type == bck_permanent);
}

log_transaction_manager::~log_transaction_manager()
{
    if (replication_node_name != NULL) { 
        free(replication_node_name);
    }
}

trid_t log_transaction_manager::get_global_transaction_identifier()
{
    return history.get_transaction_id();
}

//
// Global transaction history implementation
//

void global_transaction_history::write(trid_t tid, char mask)
{
    file::iop_status status = log.write(sizeof(log_header) + (tid >> 3),
                                        &mask, sizeof mask);
    if (status != file::ok) {
        msg_buf buf;
        log.get_error_text(status, buf, sizeof buf);
        console::error("Failed to write history file: %s\n", buf);
    }
}

char global_transaction_history::read(trid_t tid)
{
    char mask = 0;
    file::iop_status status = log.read(sizeof(log_header) + (tid >> 3),
                                       &mask, sizeof mask);
    if (status != file::ok && status != file::end_of_file) {
        msg_buf buf;
        log.get_error_text(status, buf, sizeof buf);
        console::error("Failed to read history file: %s\n", buf);
    }
    return mask;
}


global_transaction_history::global_transaction_history(file& hfile,
                                                       boolean sync_write)
: log(hfile),
  sync_log_write(sync_write)
{
}


boolean global_transaction_history::open()
{
    msg_buf buf;
    file::iop_status status =
        log.open(file::fa_readwrite,
                 sync_log_write ? file::fo_sync|file::fo_create
                                : file::fo_create);
    if (status != file::ok) {
        log.get_error_text(status, buf, sizeof buf);
        console::message(msg_error, "Failed to open history file: %s\n", buf);
        return False;
    }
    memset(tid_cache, 0, sizeof tid_cache);
    log_header lh;
    status = log.read(0, &lh, sizeof lh);
    if (status != file::ok && status != file::end_of_file) {
        log.get_error_text(status, buf, sizeof buf);
        log.close();
        console::message(msg_error, "Failed to read history file: %s\n", buf);
        return False;
    }
    if (status != file::end_of_file) {
        lh.unpack();
        last_assigned_tid = lh.last_tid;
        if (!lh.normal_completion) {
            lh.last_tid = last_assigned_tid += tid_sync_interval;
        }
    } else {
        lh.last_tid = last_assigned_tid = 0;
    }
    lh.normal_completion = False;
    lh.pack();
    status = log.write(0, &lh, sizeof lh);
    if (status != file::ok) {
        log.get_error_text(status, buf, sizeof buf);
        log.close();
        console::message(msg_error, "Failed to write history file: %s\n", buf);
        return False;
    }
    if (last_assigned_tid & 7) {
        status = log.read(sizeof(log_header) + (last_assigned_tid >> 3),
                          tid_cache, 1);
        if (status != file::ok && status != file::end_of_file) {
            log.get_error_text(status, buf, sizeof buf);
            log.close();
            console::message(msg_error,
                             "Failed to read history file: %s\n", buf);
            return False;
        }
    }
    first_tid_in_cache = last_assigned_tid & ~7;
    n_unsync_tids = 0;
    return True;
}

void global_transaction_history::close()
{
    cs.enter();
    log_header lh;
    lh.last_tid = last_assigned_tid;
    lh.normal_completion = True;
    lh.pack();
    file::iop_status status = log.write(0, &lh, sizeof lh);
    if (status != file::ok) {
        msg_buf buf;
        log.get_error_text(status, buf, sizeof buf);
        console::error("Failed to write history file: %s\n", buf);
    }
    log.close();
    cs.leave();
}

trid_t global_transaction_history::get_transaction_id()
{
    cs.enter();
    trid_t tid = ++last_assigned_tid;
    if (++n_unsync_tids == tid_sync_interval) {
        n_unsync_tids = 0;
        log_header lh;
        lh.last_tid = last_assigned_tid;
        lh.normal_completion = False;
        lh.pack();
        file::iop_status status = log.write(0, &lh, sizeof lh);
        if (status != file::ok) {
            msg_buf buf;
            log.get_error_text(status, buf, sizeof buf);
            console::error("Failed to write history file: %s\n", buf);
        }
    }
    assert(tid >= first_tid_in_cache);
    if (tid >= first_tid_in_cache + history_cache_size*8) {
        assert(tid == first_tid_in_cache + history_cache_size*8);
        first_tid_in_cache += 8;
        memcpy(tid_cache, tid_cache+1, history_cache_size-1);
        tid_cache[history_cache_size-1] = 0;
    }
    cs.leave();
    return tid;
}


boolean global_transaction_history::get_transaction_status(trid_t tid)
{
    cs.enter();
    char mask = 0;
    if (tid >= first_tid_in_cache
        && tid < first_tid_in_cache + history_cache_size*8)
    {
        mask = tid_cache[(tid - first_tid_in_cache) >> 3];
    } else {
        mask = read(tid);
    }
    console::message(msg_notify|msg_time,
                     "get global transaction history for tid=%lu -> %s\n",
                     tid, (mask & (1 << (tid & 7))) != 0
                           ? "committed" : "aborted");
    cs.leave();
    return (mask & (1 << (tid & 7))) != 0;
}

void global_transaction_history::set_transaction_status(trid_t tid,
                                                        boolean committed)
{
    cs.enter();
    assert(tid < first_tid_in_cache + history_cache_size*8);
    char mask , *pm;
    if (tid < first_tid_in_cache) {
        mask = read(tid);
        pm = &mask;
    } else {
        pm = &tid_cache[(tid - first_tid_in_cache) >> 3];
    }
    *pm |= committed << (tid & 7);
    write(tid, *pm);
    cs.leave();
}

void global_transaction_history::dump(const char*)
{
    console::output("Last global transaction identifier: %u\n",
                    last_assigned_tid);
}

END_GOODS_NAMESPACE
