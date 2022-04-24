// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< SERVER.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET   *     ?  *
// (Generic Object Oriented Database System)                        *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik * / [] \ *
//                          Last update: 14-Sep-97    K.A. Knizhnik * GARRET *
//------------------------------------------------------------------*--------*
// Database storage server
//------------------------------------------------------------------*--------*

#ifndef __SERVER_H__
#define __SERVER_H__

#include "stdinc.h"
#include "protocol.h"
#include "osfile.h"
#include "mmapfile.h"
#include "sockio.h"

#include "poolmgr.h"
#include "objmgr.h"
#include "memmgr.h"
#include "transmgr.h"
#include "classmgr.h"
#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

//
// Storage server abstraction
//

class server_agent;
class client_agent;

class GOODS_DLL_EXPORT dbs_server {
  public:
    const stid_t           id;

    boolean                opened;         // server is openned
    boolean                backup_started; // set by 'backup_start',
                                           // cleared by 'backup_stop'
    time_t                 backup_start_delay;
    fsize_t                backup_start_log_size;
    typedef void (*backup_finish_callback)(dbs_server& server,
                                           file& backup_file,
                                           boolean status);

    object_access_manager* obj_mgr;
    pool_manager*          pool_mgr;
    class_manager*         class_mgr;
    memory_manager*        mem_mgr;
    server_transaction_manager*   trans_mgr;

    int login_refused_code;

    //
    // Clients are assigned successive identifiers. This method
    // returns identifier of client which was connected longest time
    // ago, i.e. client having smallest identifier.
    //
    virtual unsigned get_oldest_client_id() = 0;

    //
    // Set limitation on object cluster size which can be sent to client
    //
    virtual void set_object_cluster_size_limit(size_t cluster_size) = 0;

    virtual unsigned get_number_of_servers() const = 0;
    virtual char const* get_name() const = 0;

    virtual void set_local_access_for_all(boolean enabled) = 0;
    virtual void set_include_credentials_in_backup(boolean enabled) = 0;

    virtual void    remote_server_connected(stid_t sid) = 0;

    virtual server_agent* create_server_agent(int id) = 0;
    virtual client_agent* create_client_agent() = 0;
    virtual void    remove_client_agent(client_agent* agent) = 0;
    virtual void    remove_server_agent(server_agent* agent) = 0;

    virtual void    iterate_clients(void (client_process::*)(void)) = 0;

    //
    // Send request message to remote server. Message consists of fixed part
    // (class dbs_request) and 'body_len' bytes of message body.
    // Header of message is automatically in place converted to universal
    // representation.
    //
    virtual void    send(stid_t sid, dbs_request* req,
                         size_t body_len = 0) = 0;

    //
    // Read message body. This function should be called from
    // 'gc_sync' or 'tm_sync' methods to read body part of message.
    //
    virtual void    read_msg_body(stid_t sid, void* buf, size_t size) = 0;

    //
    // Dump current server state. 'What' paramter specifies list of properties
    // which should be dumped. Properties can be separated by any separators.
    //
    virtual void    dump(char* what) = 0;

    //
    // Initiate online backup of storage files .
    //
    virtual void    start_backup(file&   backup_file,
                                 time_t  backup_start_delay = 0,
                                 fsize_t backup_start_log_size = 0,
                                 backup_finish_callback backup_callback = NULL) = 0;
    virtual void    stop_backup() = 0;


    //
    // Compactify database file
    //
    virtual void    compactify() = 0;

    //
    // Offline storage restore. Storage should be closed before
    // calling this method. If storage is succefully restored,
    // method "open" is called to open the server.
    //
	//[MC]
    virtual boolean restore(file& backup_file,
                            const char* database_configuration_file, int end_backup_no = -1) = 0;


    //
    // Notify clients about modified objects
    //
    virtual void    notify_clients() = 0;

    //
    // send a message from a client to others
    //
    virtual void    send_messages( client_agent *sender, dnm_array<dbs_request>& msg) = 0;
  
    //
    // Method performs login authentication. This method should return
    // 'True' value for valid clients (or servers) and 'False'
    // otherwise.
    //
    virtual boolean authenticate(dbs_request const& req, char* name, boolean is_remote_login) = 0;

    // 
    // Pass the reason of login failed. ( to be called from authenticate ( above) ).
    // ( the code will go thru the receive_message method of client application ).
    // 
    void set_refused_login_code( int code) { login_refused_code = code; }

    virtual boolean open(char const* database_configuration_file) = 0;
    virtual void    close() = 0;

    virtual boolean restore_credentials(file& backup_file) = 0;
    virtual boolean save_credentials(file& backup_file) = 0;
    virtual void update_credentials() = 0;
    virtual void add_user(char const* login, char const* password, boolean encrypted_password = False) = 0;
    virtual boolean del_user(char const* login) = 0;

    //
    // Methods for external BLOBs
    //
    virtual void write_external_blob(opid_t opid, void const* body, size_t size) = 0;
    virtual void read_external_blob(opid_t opid, void* body, size_t size) = 0;
    virtual void remove_external_blob(opid_t opid) = 0;

    //
    // This method should be overridden by programmer if he wants smae specific 
    // handling of primary node failure event. Thsi method is invoked when standby nodes
    // detects failure of primary nodes.
    //
    virtual void on_replication_master_crash() = 0;

    virtual void message(int message_class_mask, const char* msg, ...) const;
    virtual void output(const char* msg, ...) const;

    server_console *sc;

    dbs_server(stid_t sid, server_console *s = 0) : id(sid), sc(0) {
        opened = False;
        backup_started = False;
    }
    virtual ~dbs_server() {};
};

//
// Communication protocol
//

class GOODS_DLL_EXPORT communication {
  protected:
    char      name[MAX_LOGIN_NAME]; // connection name
    boolean   connected;
    socket_t* sock;
    mutex     cs;   // mutex for synchronization of concurrent writes
                    // and connect/disconnect operations
    task*     receiver;
    eventex   shutdown_event;
    boolean   reconnect_flag;

    virtual boolean  handle_communication_error();

    static void task_proc receive(void* arg);

  public:
    virtual void    write(void const* buf, size_t size);
    virtual boolean read(void* buf, size_t size);

    virtual void  poll() = 0;

    virtual void  remove() = 0;

    virtual void  connect(socket_t* connection, char* name, boolean fork=False);
    virtual void  disconnect();

    static task*  create_receiver(task::fptr f, void* arg) {
        return task::create(f, arg, task::pri_normal, task::small_stack);
    }

    static time_t ping_interval;

    communication() : shutdown_event(cs) {
        connected = False;
        *name = '\0';
        receiver = NULL;
        reconnect_flag = False;
        sock = NULL;
    }
    virtual~communication();
};

//
// Client agent
//

class GOODS_DLL_EXPORT client_agent : public client_process, public communication
{
  public:
    //
    // Insert invalidation signal in notification queue
    //
    virtual void invalidate(opid_t opid);

    //
    // Send notification requests to client
    //
    virtual void notify();

    //
    // Send a message to client
    //
    virtual void send_messages( dnm_array<dbs_request>& msg);
 
    //
    // Poll and serve client requests
    //
    virtual void poll();

	//
	// poll extensions
	//
	//[MC]
	virtual boolean pollex(dbs_request &req) {
		return True;
	};

    //
    // Disconnect client from server
    //
    virtual void disconnect();


    virtual void remove();

    //
    // Send object (or bulk of objects) to client
    //
    virtual void send_object(dbs_request const& req);

    //
    // Send all set members to the client
    //
    virtual void preload_set(dbs_request const& req);

    //
    // Filer set members
    //
    virtual void execute_query(dbs_request const& req);

    virtual char* get_name();

    client_agent(dbs_server* server, int client_id, size_t max_cluster_size)
    : client_process(server, client_id),
      cluster_size(max_cluster_size) {}

  protected:
    void handle_notifications(int n_requests);
    virtual void write(void const* buf, size_t size);
    void send_compressed_response(dbs_request* reply);

    size_t     cluster_size;
    dnm_buffer buf;
    dnm_buffer zbuf;

    mutex      notify_cs;
    dnm_array<dbs_request> notify_buf;

    mutex      send_msg_cs;
    dnm_array<dbs_request> send_msg_buf;    
};

//
// Remote server agent
//

class GOODS_DLL_EXPORT server_agent : public communication {
  protected:
    const stid_t id;
    dbs_server*  server;
    mutex        connect_cs;

  public:
    boolean      is_online() { return connected; }
    char*        get_name() { return name; }

    //
    // Make connection with another server by sending self indentifing
    // information to it and receive authorization result.
    // This methods is called before connect(), so communication class fields
    // are not initialized.
    //
    virtual boolean handshake(socket_t* sock, stid_t sid, char const* my_name);
    //
    // Poll and serve requests of remote server
    //
    virtual void poll();
    //
    // Write to remote server
    //
    virtual void write(void const* buf, size_t size);
    //
    // Remove server
    //
    virtual void remove();
    //
    // Send request to remote server
    //
    virtual void send(dbs_request* req, size_t body_len = 0);
    //
    //
    //
    virtual void connect(socket_t* connection, char* name, boolean fork=False);

    //
    // Read body of message send by remote server
    //
    virtual void read_msg_body(void* buf, size_t size);

    server_agent(dbs_server* server, stid_t sid) : id(sid) {
        this->server = server;
    }
};

//
// Login hash
// 
#define LOGIN_HASH_TABLE_SIZE 113

class authentication_manager 
{ 
  public:	
	//[MC] Added virtual destructor
	virtual ~authentication_manager() {}
	virtual boolean authenticate(char const* login, char const* password) = 0;
	virtual void    add_user(char const* login, char const* password, boolean encrypted_password = False) = 0;
	virtual boolean del_user(char const* login) = 0;
	virtual void    reload() = 0;
	virtual boolean backup(file& backup_file) = 0;
	virtual boolean restore(file& backup_file) = 0;
};

class simple_authentication_manager : public authentication_manager 
{
  //[MC]
  protected:
    struct login_entry {
        login_entry* next;
        char*        login;
        char*        password;
        boolean      encrypted_password;
        login_entry(char const* login, char const* password, login_entry* chain, boolean  encrypted_password);
        ~login_entry();
    };    
  public:
    simple_authentication_manager(char const* file);
    ~simple_authentication_manager();

    virtual boolean authenticate(char const* login, char const* password);
    virtual void reload();
    virtual void    add_user(char const* login, char const* password, boolean encrypted_password = False);
    virtual boolean del_user(char const* login);
    virtual boolean backup(file& backup_file);
    virtual boolean restore(file& backup_file);

  //[MC] Changed from private to protected 	
  protected:
    void clean();
	//[MC] Added virtual
    virtual void save();
    login_entry* hash_table[LOGIN_HASH_TABLE_SIZE];
    char const* file_path;
    boolean file_exists;
};
                            

//
// Local storage server
//

#define DEFAULT_CLUSTER_SIZE 512

class GOODS_DLL_EXPORT storage_server : public dbs_server
{
  protected:
    l2elem         clients;   // list if client agents
    server_agent** servers;   // array of server agents
    int            n_servers; // total number of servers in database
    mutex          cs;        // synchronize access to class components

    socket_t*      local_gateway;    // listening sockets
    socket_t*      global_gateway;

    int            n_online_remote_servers;
    int            n_opened_gateways;
    eventex        term_event; // event notifing about thread termination
    size_t         object_cluster_size_limit;

    int            client_id; // generator of sequence numbers
                              // used to identify clients

    boolean        accept_remote_connections;

    char           name[MAX_LOGIN_NAME]; // address of server

    file*          backup_file;
    backup_finish_callback backup_callback;

    eventex        backup_finished_event;

    authentication_manager* authenticator;
    boolean        local_access_for_all;
    boolean        include_credentials_in_backup;

    char const*    external_blob_directory;
    
    class login_data : public l2elem {
      public:
        socket_t*       sock;
        storage_server* server;
        boolean         is_remote_connection;

        login_data(storage_server* server, socket_t* sock, boolean is_remote) {
            this->server = server;
            this->sock = sock;
            this->is_remote_connection = is_remote;
        }
        ~login_data() {
            unlink();
            delete sock;
        }
    };
    l2elem handshake_list; // list of accepted but not established connections

    static void task_proc start_local_gatekeeper(void* arg);
    static void task_proc start_global_gatekeeper(void* arg);
    static void task_proc start_backup_process(void* arg);
    static void task_proc start_handshake(void* arg);

    virtual void          accept(socket_t* gateway); // accept connections
    virtual void          handshake(login_data* login);

    virtual void          remote_server_connected(stid_t sid);

    virtual server_agent* create_server_agent(int id);
    virtual client_agent* create_client_agent();

    virtual boolean       backup();

    virtual void          compactify();

    virtual file* external_blob_file(opid_t opid);

  public:

    virtual void        notify_clients();

    //
    // send a message from a client to others
    //
    virtual void        send_messages( client_agent *sender, dnm_array<dbs_request>& msg);

    virtual void        send(stid_t sid, dbs_request* req, size_t body_len=0);

    virtual void        read_msg_body(stid_t sid, void* buf, size_t size);

    virtual unsigned    get_number_of_servers() const;

    virtual char const* get_name() const;

    virtual unsigned    get_oldest_client_id();

    virtual void        remove_client_agent(client_agent* agent);

    virtual void        set_object_cluster_size_limit(size_t cluster_size);

    virtual void        set_local_access_for_all(boolean enabled);
    virtual void        set_include_credentials_in_backup(boolean enabled);

    virtual void        remove_server_agent(server_agent* agent);

    virtual void        dump(char* what);

    virtual void        iterate_clients(void (client_process::*)(void));

    virtual void        start_backup(file&   backup_file,
                                     time_t  backup_start_delay = 0,
                                     fsize_t backup_start_log_size = 0,
                                     backup_finish_callback callback = NULL);
    virtual void        stop_backup();
	//[MC] Added end_backup_no parameter
    virtual boolean     restore(file& backup_file,
                             const char* database_configuration_file, int end_backup_no = -1);

    virtual boolean     authenticate(dbs_request const& req, char* name, boolean is_remote_login);
    virtual void        update_credentials();
    virtual boolean     restore_credentials(file& backup_file);
    virtual boolean     save_credentials(file& backup_file);
    virtual void        add_user(char const* login, char const* password, boolean encrypted_password = False);
    virtual boolean     del_user(char const* login);

    virtual boolean     open(char const* database_configuration_file);
    virtual void        close();

    virtual void        write_external_blob(opid_t opid, void const* body, size_t size);
    virtual void        read_external_blob(opid_t opid, void* body, size_t size);
    virtual void        remove_external_blob(opid_t opid);

    virtual void        on_replication_master_crash();

    virtual ~storage_server();

    storage_server(stid_t sid,
                   object_access_manager&,
                   pool_manager&,
                   class_manager&,
                   memory_manager&,
                   server_transaction_manager&,
                   size_t object_cluster_size = DEFAULT_CLUSTER_SIZE,
                   boolean  accept_remote_connections = True,
                   authentication_manager* authenticator = NULL,
                   char const* ext_blob_dir = NULL
        );
};
END_GOODS_NAMESPACE

#endif
