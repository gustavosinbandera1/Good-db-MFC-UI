// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< MONITOR.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      1-Oct-2002  K.A. Knizhnik  * / [] \ *
//                          Last update:  1-Oct-2002  K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Transaction monitor for GOODS clients. This monitor
// allows to replicate client requests to multiple servers
// and so provide fault tolerance. 
//-------------------------------------------------------------------*--------*


#ifndef _WIN32
#if defined(__sun) || defined(__SVR4)
#define mutex system_mutex
#endif 
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#undef mutex
#endif
#include <errno.h>

#include "monitor.h"
#include "protocol.h"

USE_GOODS_NAMESPACE

client_proxy::client_proxy(socket_t* client_sock, char** servers, int n_servers)
{
    server_socket = new socket_t*[n_servers];
    received_response = new int1[n_servers];
    lock_acknowledgement = new int1[n_servers];
    n_invalidations = new int8[n_servers];
    cmd_history = new command_history[CMD_HISTORY_SIZE];
    cmd_history_pos = 0;
    n_sent_invalidations = 0;
    client_socket = client_sock;
    this->n_servers = n_servers;


    expected_response = -1;
    client_cmd = -1;
    n_responses = 0;
    proceed_lock_request = false;
    memset(received_response, 0, n_servers);
    memset(n_invalidations, 0, n_servers*sizeof(int8));

    for (int i = 0; i < n_servers; i++) { 
	if (servers[i] == NULL) { 
	    server_socket[i] = NULL;
	} else { 
	    socket_t* sock = socket_t::connect(servers[i], socket_t::sock_global_domain, 1, 0);
	    if (!sock->is_ok()) { 
		msg_buf buf;
		sock->get_error_text(buf, sizeof buf);
		console::message(msg_error, "Failed to connect server '%s': %s\n", 
				 servers[i], buf);
		delete sock;
		sock = NULL;
	    }
	    server_socket[i] = sock;
	}
    }
}

boolean client_proxy::reply(int server_id, transaction_monitor* monitor)
{
    int i, size;
    buf.put(sizeof(dbs_request));
    dbs_request* resp = (dbs_request*)&buf;
    if (server_socket[server_id] == NULL) { 
	return True;
    } 
    if (monitor->servers[server_id] != NULL) { 
	if (!server_socket[server_id]->read(resp, sizeof(dbs_request))) { 
	    monitor->on_server_fault(server_id, server_socket[server_id], server_connection_failure);
	    monitor->remove_channel(this, server_id);
	} else { 
	    //printf("Client %p receive response %d\n", this, resp->cmd);
	    cmd_history[cmd_history_pos % CMD_HISTORY_SIZE].server_id = server_id;
	    cmd_history[cmd_history_pos++ % CMD_HISTORY_SIZE].cmd = resp->cmd;
	    switch (resp->cmd) { 
	      case dbs_request::cmd_ack_lock:
		if (proceed_lock_request) { 
		    lock_acknowledgement[server_id] = True;
		    i = n_servers; 
		    while (--i >= 0 
			   && (monitor->servers[i] == NULL 
			       || server_socket[i] == NULL 
			       || lock_acknowledgement[i]));
		    if (i < 0) { 
			broadcast_completed = True;
			proceed_lock_request = False;
		    }
		    return True;
		} else { 
		    monitor->on_server_fault(server_id, server_socket[server_id], 
					     unexpected_server_response);
		    monitor->remove_channel(this, server_id);
		}
		break;
	      case dbs_request::cmd_bye:
		monitor->remove_channel(this, server_id);
		i = n_servers; 
		while (--i >= 0 && (monitor->servers[i] == NULL || server_socket[i] == NULL));
		if (i < 0) { 
		    // no more active servers
		    client_socket->write(resp, sizeof(dbs_request));
		    return False;
		}
		break;
	      case dbs_request::cmd_invalidate:
		resp->unpack();
		i = resp->object.extra;
		if (i > 0) { 
		    size = i*sizeof(dbs_request);
		    if (!server_socket[server_id]->read(buf.append(size), size)) { 
			monitor->on_server_fault(server_id, server_socket[server_id], 
						 server_connection_failure);
			monitor->remove_channel(this, server_id);
			resp = NULL;
		    }
		}
		if (resp != NULL) { 
		    if (n_invalidations[server_id] + i + 1 > n_sent_invalidations) { 
			resp = (dbs_request*)&buf;
			size = (int)(n_invalidations[server_id] + i + 1 - n_sent_invalidations)*sizeof(dbs_request);
			if (n_invalidations[server_id] != n_sent_invalidations) { 
			    assert(n_invalidations[server_id] < n_sent_invalidations);
			    resp += n_sent_invalidations - n_invalidations[server_id];
			    resp->unpack();
			    resp->object.extra = (int)(i - n_sent_invalidations + n_invalidations[server_id]);
			}
			resp->pack();
			if (!client_socket->write(resp, size)) { 
			    return False;
			}
			n_sent_invalidations = n_invalidations[server_id] + i + 1;
		    }
		    n_invalidations[server_id] += i + 1;
		    return True;
		}
		break;
	      default:
		if (received_response[server_id]
		    || (resp->cmd != expected_response &&
			(resp->cmd != dbs_request::cmd_refused 
			 || expected_response != dbs_request::cmd_ok)))
		{
		    monitor->on_server_fault(server_id, server_socket[server_id], 
					     unexpected_server_response);
		    monitor->remove_channel(this, server_id);
		} else { 
		    resp->unpack();
		    switch(resp->cmd) { 
		      case dbs_request::cmd_classdesc:  // server send class descriptor requested by cpid
		      case dbs_request::cmd_object:     // server sent object to client
		      case dbs_request::cmd_opids:      // object identifiers reserved by server for the client
			size = resp->result.size;
			if (!server_socket[server_id]->read(buf.append(size), size)) { 
			    monitor->on_server_fault(server_id, server_socket[server_id], 
						     server_connection_failure);
			    monitor->remove_channel(this, server_id);
			} else { 
			    received_response[server_id] = True;
			}
			break;
		      case dbs_request::cmd_classid:    // class identifier returned to client by server
		      case dbs_request::cmd_lockresult: // result of applying lock request at server
		      case dbs_request::cmd_transresult:// coordinator returns transaction status to client
		      case dbs_request::cmd_ok:         // successful authorization 
		      case dbs_request::cmd_ack:        // acknowledgement
		      case dbs_request::cmd_location:   // server returns address of allocated object
		      case dbs_request::cmd_refused:    // authorization procedure is failed at server
			received_response[server_id] = True;
			break;
		      default:
			assert(False);
		    }		
		    if (received_response[server_id]) { 
			if (n_responses > 0) { 
			    if (buf.size() != cmp_buf[0].size()
				|| memcmp(&cmp_buf[0], &buf, buf.size()) != 0)
			    {
				if (n_different_responses == 1) { 
				    cmp_buf[1].put(buf.size());
				    memcpy(&cmp_buf[1], &buf, buf.size());
				    n_different_responses = 2;
				    if (n_responses > 1) { 
					bad_response = 1;
				    }
				    response_from_server[1] = server_id;
				} else { 
				    if (bad_response == 1 
					|| buf.size() != cmp_buf[1].size() 
					|| memcmp(&cmp_buf[1], &buf, buf.size()) != 0)
				    {
					monitor->on_server_fault(-1, NULL,
								 unable_to_choose_correct_server_response);
					return False;
				    }
				    bad_response = 0;
				}
			    } else if (n_different_responses == 2) { 
				if (bad_response == 0) { 
				    monitor->on_server_fault(-1, NULL,
							     unable_to_choose_correct_server_response);
				    return False;
				}
				bad_response = 1;
			    }
			} else { 
			    cmp_buf[0].put(buf.size());
			    memcpy(&cmp_buf[0], &buf, buf.size());
			    n_different_responses = 1;
			    response_from_server[0] = server_id;
			}
			n_responses += 1;
		    }
		}
	    }
	}
    } else {
	monitor->remove_channel(this, server_id);
    }
    if (proceed_lock_request) { 
	i = n_servers; 
	while (--i >= 0 
	       && (monitor->servers[i] == NULL 
		   || server_socket[i] == NULL 
		   || lock_acknowledgement[i]));
	if (i < 0) { 
	    broadcast_completed = True;
	    proceed_lock_request = False;
	}
    } 
    i = n_servers; 
    while (--i >= 0 
	   && (monitor->servers[i] == NULL || server_socket[i] == NULL || received_response[i]));
    if (i < 0) {
	// receive all expected responses
	int resp_size;
	expected_response = -1;
	if (n_responses == 0) { 
	    monitor->on_server_fault(-1, NULL, no_active_servers);
	    return False;
	}
	if (n_different_responses > 1) {
	    if (bad_response < 0) { 
		monitor->on_server_fault(-1, NULL,
					 unable_to_choose_correct_server_response);
		return False;
	    } else { 
		int sid = response_from_server[bad_response]; 
		monitor->on_server_fault(sid, server_socket[sid], 
					 corrupted_server_response);
		monitor->remove_channel(this, sid);
		resp = (dbs_request*)&cmp_buf[bad_response^1];
		resp_size = (int)cmp_buf[bad_response^1].size();
	    }
	} else { 
	    resp_size = (int)cmp_buf[0].size();
	    resp = (dbs_request*)&cmp_buf[0];
	}
	n_responses = 0;
	memset(received_response, 0, n_servers);
	broadcast_completed = True;
	if (resp->cmd != dbs_request::cmd_ack) { 
	    resp->pack();
	    if (!client_socket->write(resp, resp_size)) { 
		return False;
	    }
	}
	return resp->cmd != dbs_request::cmd_refused;
    }
    return True;
}

boolean client_proxy::broadcast(transaction_monitor* monitor)
{
    int i;
    size_t size;

    buf.put(sizeof(dbs_request));
    dbs_request* req = (dbs_request*)&buf;
    
    if (!client_socket->read(req, sizeof(dbs_request))) { 
	return False;
    }
    req->unpack();
    int expected_response = dbs_request::cmd_ack;
    client_cmd = req->cmd;
    cmd_history[cmd_history_pos % CMD_HISTORY_SIZE].server_id = -1;
    cmd_history[cmd_history_pos++ % CMD_HISTORY_SIZE].cmd = req->cmd;
    //printf("Client %p receive command %d\n", this, req->cmd);
    switch (req->cmd) { 
      case dbs_request::cmd_load:       // client wants to receive object from server
	size = req->object.extra*sizeof(dbs_request); 
	if (size != 0 && !client_socket->read(buf.append(size), size)) { 
	    return False;
	}
	expected_response = dbs_request::cmd_object;
	break;
      case dbs_request::cmd_forget:     // client remove reference to loaded object
	req->cmd = dbs_request::cmd_forget_ack;
	size = req->object.extra*sizeof(dbs_request); 
	if (size != 0 && !client_socket->read(buf.append(size), size)) { 
	    return False;
	}
	break;
      case dbs_request::cmd_throw:      // client throw away instance of object from cache
	req->cmd = dbs_request::cmd_throw_ack;
	size = req->object.extra*sizeof(dbs_request); 
	if (size != 0 && !client_socket->read(buf.append(size), size)) {
	    return False;
	}
	break;
      case dbs_request::cmd_getclass:   // get information about class from server
	expected_response = dbs_request::cmd_classdesc;
	break;
      case dbs_request::cmd_putclass:   // register new class at server 
	size = req->clsdesc.size;
	if (!client_socket->read(buf.append(size), size)) { 
	    return False;
	}
	expected_response = dbs_request::cmd_classid;
	break;
      case dbs_request::cmd_modclass:   // modify existed class at server
	req->cmd = dbs_request::cmd_modclass_ack;
	size = req->clsdesc.size;
	if (!client_socket->read(buf.append(size), size)) { 
	    return False;
	}
	break;
      case dbs_request::cmd_lock:       // request to server from client to lock object
	req->cmd = dbs_request::cmd_lock_ack;
	expected_response = dbs_request::cmd_lockresult;
	memset(lock_acknowledgement, 0, n_servers);
	proceed_lock_request = true;
	break;
      case dbs_request::cmd_unlock:     // request to server from client to unlock object
	req->cmd = dbs_request::cmd_unlock_ack;
	break;
      case dbs_request::cmd_transaction:// client sends transaction to coordinator
      case dbs_request::cmd_ztransaction:// client sends transaction to coordinator
	expected_response = dbs_request::cmd_transresult;
	size = req->trans.size;
	if (!client_socket->read(buf.append(size), size)) { 
	    return False;
	}
	break;	
      case dbs_request::cmd_subtransact:// client sends local part of transaction to server
      case dbs_request::cmd_zsubtransact:// client sends local part of transaction to server
	size = req->trans.size;
	if (!client_socket->read(buf.append(size), size)) { 
	    return False;
	}
	break;
      case dbs_request::cmd_login:      // client login at server 
	size = req->login.name_len;
	if (!client_socket->read(buf.append(size), size)) { 
	    return False;
	}
	expected_response = dbs_request::cmd_ok;
	break;
      case dbs_request::cmd_logout:     // client finish the session
	break;	
      case dbs_request::cmd_alloc:      // client request server to allocate object 
	expected_response = dbs_request::cmd_location;
	break;
      case dbs_request::cmd_free:       // client request to free object        
	req->cmd = dbs_request::cmd_free_ack;
	break;
      case dbs_request::cmd_bulk_alloc: // client request server to allocate several objects and reserve OIDs 
	size = req->alloc.size*6;
	if (!client_socket->read(buf.append(size), size)) {
	    return False;
	}
	expected_response = dbs_request::cmd_opids;
	break;
      case dbs_request::cmd_get_size:   // get storage size
	expected_response = dbs_request::cmd_location;
	break;
      default:
	return False;
    }
    req = (dbs_request*)&buf;
    req->pack();
    for (i = 0; i < n_servers; i++) { 
	if (server_socket[i] != NULL) { 
	    if (monitor->servers[i] == NULL) { 
		monitor->remove_channel(this, i);
	    } else { 
		if (!server_socket[i]->write(&buf, buf.size())) { 
		    monitor->on_server_fault(i, server_socket[i], server_connection_failure);
		    monitor->remove_channel(this, i);
		}
	    }
	}
    }
    if (req->cmd == dbs_request::cmd_logout) { 
	req->cmd = dbs_request::cmd_bye;
	client_socket->write(req, sizeof(dbs_request));
	return False;
    }
    n_different_responses = 0;
    n_responses = 0;
    bad_response = -1;
    memset(received_response, 0, n_servers);
    this->expected_response = expected_response;
    broadcast_completed = False;
    do { 
	if (!monitor->select_server_responses(this)) {
	    return False;
	}
    } while (!broadcast_completed);

    return True;
}

client_proxy::~client_proxy()
{
    delete client_socket;
    for (int i = n_servers; --i >= 0;) { 
	delete server_socket[i];
    }
    delete[] server_socket;
    delete[] received_response;
    delete[] lock_acknowledgement;
    delete[] n_invalidations;
    delete[] cmd_history;
}

void task_proc transaction_monitor::accept_task(void* arg)
{
    transaction_monitor* monitor = (transaction_monitor*)arg;
    monitor->accept();
}

void task_proc transaction_monitor::select_task(void* arg)
{
    transaction_monitor* monitor = (transaction_monitor*)arg;
    monitor->select();
}

void transaction_monitor::add_channel(client_proxy* proxy, int server_id) 
{
    cs.enter();
    int desc = server_id < 0 
	? proxy->client_socket->get_descriptor()
	: proxy->server_socket[server_id]->get_descriptor();

    if (desc >= n_channels) { 
	int i, n = n_channels;
	channel_descriptor* new_channels = new channel_descriptor[desc*2];
	for (i = 0; i < n; i++) { 
	    new_channels[i] = channels[i];
	}
	for (n = desc*2; i < n; i++) {
	    new_channels[i].proxy = NULL;
	}
	delete[] channels;
	channels = new_channels;
	n_channels = n;
    }
    if (desc >= n_descriptors) { 
	n_descriptors = desc+1;
    }
    channels[desc].proxy = proxy;
    channels[desc].server_id = server_id;
    FD_SET(desc, &descriptors);
    if (server_id < 0) { 
	if (n_clients == 0) { 
	    activate_select_task.signal();
	}
	n_clients += 1;
    } else { 
	FD_SET(desc, &server_descriptors);
    }
    cs.leave();
}

void transaction_monitor::remove_channel(client_proxy* proxy, int server_id)
{
    unsigned desc;
    cs.enter();
    if (server_id >= 0) { 
	desc = proxy->server_socket[server_id]->get_descriptor();
	delete proxy->server_socket[server_id];
	proxy->server_socket[server_id] = NULL;	
	FD_CLR(desc, &server_descriptors);
    } else { 
	for (int i = n_servers; --i >= 0;) { 
	    if (proxy->server_socket[i] != NULL) { 
		desc = proxy->server_socket[i]->get_descriptor();
		FD_CLR(desc, &descriptors);
		FD_CLR(desc, &server_descriptors);
		channels[desc].proxy = NULL;
	    }
	}
	n_clients -= 1;
	desc = proxy->client_socket->get_descriptor();
	delete proxy;
    }
    FD_CLR(desc, &descriptors);
    channels[desc].proxy = NULL;
    cs.leave();
}


transaction_monitor::transaction_monitor(char*  proxy, 
					 char** servers, 
					 int    n_servers)
: activate_select_task(cs)
{
    msg_buf err;
    this->n_servers = n_servers;
    this->servers = new char*[n_servers];
    for (int i = 0; i < n_servers; i++) { 
	this->servers[i] = strdup(servers[i]);
    }
    gateway = socket_t::create_global(proxy);
    if (gateway !=  NULL) {
        if (!gateway->is_ok()) {
            gateway->get_error_text(err, sizeof err);
            console::error("Failed to accept socket: %s\n", err);
            delete gateway;
            gateway = NULL;
        }
    }
    n_channels = 0;
    channels = NULL;
}


void transaction_monitor::start()
{
    running = True;
    n_clients = 0;
    FD_ZERO(&descriptors);
    FD_ZERO(&server_descriptors);
    n_descriptors = 0;
    accept_task_terminated.reset();
    select_task_terminated.reset();
    activate_select_task.reset();
    task::create(accept_task, this);
    task::create(select_task, this);
}

void transaction_monitor::stop()
{
    cs.enter();
    running = False;
    activate_select_task.signal();
    cs.leave();
    if (gateway != NULL) { 
	gateway->cancel_accept();	
    }
    accept_task_terminated.wait();
    select_task_terminated.wait();
    delete gateway;
    for (int i = 0; i < n_channels; i++) {
	if (channels[i].proxy != NULL && channels[i].server_id < 0) { 
	    delete channels[i].proxy;
	}
    }
}
    
transaction_monitor::~transaction_monitor()
{
    for (int i = 0; i < n_servers; i++) { 
	if (servers[i] != NULL) { 
	    free(servers[i]);
	}
    }
    delete[] servers;
    delete[] channels;
}


void transaction_monitor::accept()
{
    msg_buf buf;
    while (running) {
        socket_t* new_sock = gateway->accept();
        if (new_sock != NULL) {
            if (!running) {
                delete new_sock;
                break;
            }
	    client_proxy* proxy = new client_proxy(new_sock, servers, n_servers);
	    add_channel(proxy, -1);
	    for (int i = n_servers; --i >= 0;) {
		if (servers[i] != NULL && proxy->server_socket[i] != NULL) { 
		    add_channel(proxy, i);
		}
	    }
	    if (!select_started) { 
		select_started = True;
		task::create(select_task, this);
	    }
        } else {
            if (running) {
                gateway->get_error_text(buf, sizeof buf);
                console::message(msg_error|msg_time,
                                 "Failed to accept socket: %s\n", buf);
            }
        }
    }
    accept_task_terminated.signal();
}


boolean transaction_monitor::select_server_responses(client_proxy* client)
{
    fd_set readfds;
    struct timeval timeout;
    
    if (!running) { 
	return False;
    }
    cs.enter();
    readfds = server_descriptors;
    cs.leave();
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    int rc = ::select(n_descriptors, &readfds, NULL, NULL, &timeout);
    if (rc > 0) { 
	for (int i = 0; rc > 0; i++) { 
	    if (FD_ISSET(i, &readfds)) {
		client_proxy* proxy;
		int server_id;
		cs.enter();
		proxy = channels[i].proxy;
		server_id = channels[i].server_id;
		cs.leave();
		rc -= 1;
		if (proxy != NULL) { 
		    assert(server_id >= 0);
		    if (!proxy->reply(server_id, this)) { 
			if (proxy == client) { 
			    return False;
			} else { 
			    remove_channel(proxy, -1);
			}
		    }
		}
	    }
	} 
    }
    return True;
}



void transaction_monitor::select()
{
    fd_set readfds;
    struct timeval timeout;
    
    while (running) { 
	cs.enter();
	if (n_clients == 0) { 
	    activate_select_task.reset();
	    activate_select_task.wait();
	    if (!running) { 
		break;
	    }
	    assert(n_clients != 0);
	}
	readfds = descriptors;
	cs.leave();
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	int rc = ::select(n_descriptors, &readfds, NULL, NULL, &timeout);
	boolean client_response_proceeded = False;
	if (rc > 0) { 
	    for (int i = n_descriptors; --i >= 0;) { 
		if (FD_ISSET(i, &readfds)) {
		    client_proxy* proxy;
		    int server_id;
		    cs.enter();
		    proxy = channels[i].proxy;
		    server_id = channels[i].server_id;
		    cs.leave();
		    if (proxy != NULL && (!client_response_proceeded || server_id < 0)) { 
			boolean succeed;
			if (server_id < 0) { 
			    // client socket descriptor
			    succeed = proxy->broadcast(this);
			    client_response_proceeded = True;
			} else { 
			    succeed = proxy->reply(server_id, this);
			}
			if (!succeed) { 
			    remove_channel(proxy, -1);
			}
		    }
		}
	    }
	} 
#if 0
	else if (rc < 0 && errno == EBADF) {
	    FD_ZERO(&readfds);
	    for (int i = n_descriptors; --i >= 0;) { 
		if (FD_ISSET(i, &descriptors)) {
          static struct timeval nowait;
		    FD_SET(i, &readfds);
		    while ((rc = ::select(i+1, &readfds, NULL, NULL, &nowait)) < 0
			    && errno == EINTR);
		    if (rc < 0 && errno == EBADF) { 
			if (channels[i].proxy != NULL) { 
			    remove_channel(channels[i].proxy, channels[i].server_id);
			} else { 
			    FD_CLR((unsigned)i, &descriptors);
			}
		    }
		    FD_CLR((unsigned)i, &readfds);
		}
	    }
	}
#endif
    }
    select_task_terminated.signal();
}

void transaction_monitor::on_server_fault(int server_id, socket_t* sock, 
					  server_fault status)
{
    msg_buf msg;

    switch (status) {
      case server_connection_failure:
	sock->get_error_text(msg, sizeof msg);
	console::message(msg_error|msg_time, "Connection with server %s broken: %s\n",
			 servers[server_id], msg);
	free(servers[server_id]);
	servers[server_id] = NULL;
	break;
      case unexpected_server_response:
	console::error("Unexpected response received from server %s\n", 
		       servers[server_id]);
	free(servers[server_id]);
	servers[server_id] = NULL;
	break;
      case corrupted_server_response:
	console::message(msg_error, "Corrupted response received from server %s\n", 
		       servers[server_id]);
	free(servers[server_id]);
	servers[server_id] = NULL;
	break;
      case no_active_servers:
	console::error("No active servers\n");
      case unable_to_choose_correct_server_response:
	console::error("Unable to choose correct server response\n");
    }
}


void transaction_monitor::dump()
{
    cs.enter();
    int i, n;
    for (i = 0, n = 0; i < n_servers; i++) { 
	if (servers[i] != NULL) { 
	    n += 1;
	}
    }
    printf("Number of online servers: %d\n", n);
    printf("Number of attached clients: %d\n", n_clients);
    cs.leave();
}

int main(int argc, char** argv)
{
    console::output("Transaction monitor for GOODS\n");
    if (argc < 4) {
	console::output(
	    "Usage: \n" 
	    "    monitor MONITOR-HOST-ADDRESS (SERVER-HOST-ADDRESS)+\n"
	    "where\n"
	    "    HOST-ADDRESS is HOSTNAME:PORT pair\n"
	    "and at least two servers are specified\n\n");
	return 1;
    }
    transaction_monitor* monitor = new transaction_monitor(argv[1], argv+2, argc-2);
    boolean started = False;
    char buf[256];

    while (True) { 
	console::output("> ");
	if (!console::input(buf, sizeof buf)) { 
	    if (started) { 
		monitor->stop();
	    }
	    delete monitor;
	    return 1;
	}
	int len = (int)strlen(buf);
	if (len == 1) { 
	    continue;
	}
	buf[len-1] = '\0';
	if (stricmp(buf, "start") == 0) { 
	    if (started) { 
		console::output("Monitor already started");
	    } else { 
		monitor->start();
		started = True;
	    }
	} else if (stricmp(buf, "stop") == 0) { 
	    if (started) { 
		monitor->stop();
		started = False;
	    } else { 
		console::output("Monitor is not started");
	    }
	} else if (stricmp(buf, "show") == 0) { 
	    if (started) {
		monitor->dump();
	    } else { 
		console::output("Monitor is not started");
	    }		
	} else if (stricmp(buf, "exit") == 0) { 
	    if (started) {
		monitor->stop();
	    }
	    delete monitor;
	    return 0;
	} else { 
	    console::output("Valid commands: start, stop, exit, show\n");
	}
    }
}









