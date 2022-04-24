// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
#ifndef __CONFGRTR_H__
#define __CONFGRTR_H__

#include "goodsdlx.h"
#include "stdinc.h"
#include "sockfile.h"

BEGIN_GOODS_NAMESPACE

class object_access_manager;
class pool_manager;
class class_manager;
class memory_manager;
class server_transaction_manager;
class dbs_server;
class storage_server;
class file;

#define GOODSRV_CFG_FILE_NAME  "goodsrv.cfg"

#define Kb (size_t)1024


GOODS_DLL_EXPORT extern unsigned init_map_file_size;
GOODS_DLL_EXPORT extern unsigned init_index_file_size;
// amount of memory after which allocation GC is initiated
GOODS_DLL_EXPORT extern unsigned gc_init_timeout;
GOODS_DLL_EXPORT extern unsigned gc_init_allocated;
GOODS_DLL_EXPORT extern unsigned gc_init_idle_period;
GOODS_DLL_EXPORT extern unsigned gc_init_min_allocated;
GOODS_DLL_EXPORT extern unsigned gc_response_timeout;
GOODS_DLL_EXPORT extern unsigned gc_grey_set_threshold;
GOODS_DLL_EXPORT extern unsigned max_data_file_size;
GOODS_DLL_EXPORT extern unsigned max_objects;

GOODS_DLL_EXPORT extern unsigned sync_log_writes;
GOODS_DLL_EXPORT extern unsigned permanent_backup;
GOODS_DLL_EXPORT extern unsigned trans_preallocated_log_size;
GOODS_DLL_EXPORT extern unsigned trans_max_log_size;
GOODS_DLL_EXPORT extern unsigned trans_wait_timeout;
GOODS_DLL_EXPORT extern unsigned trans_retry_timeout;
GOODS_DLL_EXPORT extern unsigned checkpoint_period;
GOODS_DLL_EXPORT extern unsigned dynamic_reclustering_limit;

GOODS_DLL_EXPORT extern unsigned lock_timeout;

GOODS_DLL_EXPORT extern unsigned page_pool_size;
 
GOODS_DLL_EXPORT extern char*    admin_telnet_port;
GOODS_DLL_EXPORT extern char*    admin_password;
GOODS_DLL_EXPORT extern unsigned cluster_size;

GOODS_DLL_EXPORT extern char* index_file_name;
GOODS_DLL_EXPORT extern char* map_file_name;
GOODS_DLL_EXPORT extern char* data_file_name;
GOODS_DLL_EXPORT extern char* trans_file_name;
GOODS_DLL_EXPORT extern char* history_file_name;
GOODS_DLL_EXPORT extern char* external_blob_directory;

// @PG: exported garcc interface 
GOODS_DLL_EXPORT extern socket_file* garcc_file;
GOODS_DLL_EXPORT extern socket_t*    garcc_gateway;
GOODS_DLL_EXPORT extern semaphore    garcc_has_shutdown;
GOODS_DLL_EXPORT extern semaphore    garcc_op_completed;
GOODS_DLL_EXPORT extern char*        garcc_port;


struct param_binding {
     char const* param_name;
     unsigned*   param_ivalue;
     char const* param_meaning; 
     boolean     (*set_param)(dbs_server* srv, unsigned val);
     char**      param_svalue;
};

GOODS_DLL_EXPORT extern param_binding goodsrv_params[];

GOODS_DLL_EXPORT boolean set_max_data_file_size(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_max_objects(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_gc_init_timeout(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_gc_grey_set_threshold(dbs_server* srv,
                                                   unsigned val);
GOODS_DLL_EXPORT boolean set_gc_response_timeout(dbs_server* srv,
                                                 unsigned val);
GOODS_DLL_EXPORT boolean set_gc_init_allocated(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_gc_init_idle_period(dbs_server* srv,
                                                 unsigned val);
GOODS_DLL_EXPORT boolean set_gc_init_min_allocated(dbs_server* srv,
                                                   unsigned val);
GOODS_DLL_EXPORT boolean set_permanent_backup(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_checkpoint_period(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_max_log_size(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_preallocated_log_size(dbs_server* srv,
                                                   unsigned val);
GOODS_DLL_EXPORT boolean set_wait_timeout(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_retry_timeout(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_dynamic_reclustering_limit(dbs_server* srv,
                                                        unsigned val);
GOODS_DLL_EXPORT boolean set_lock_timeout(dbs_server* srv, unsigned val);
GOODS_DLL_EXPORT boolean set_cluster_size(dbs_server* srv, unsigned val);

union option_value { 
    unsigned ivalue;
    char*    svalue;
};

GOODS_DLL_EXPORT boolean parse_option(char* buf, param_binding* &param,
                                      option_value& u);
GOODS_DLL_EXPORT void read_goodsrv_configuration(const char* cfg_file_name);

#define SHOW_ALL "servers clients memory transaction classes version"

GOODS_DLL_EXPORT void on_backup_completion(dbs_server&, file& backup_file,
                                           boolean result);

struct trace_option { 
    const char* option;
    int         mask;
    const char* meaning;
};

GOODS_DLL_EXPORT extern trace_option trace_options_table[];

const int max_cmd_len = 256;

GOODS_DLL_EXPORT extern char      log_file_name[max_cmd_len];

GOODS_DLL_EXPORT extern char      monitor_options[max_cmd_len];
GOODS_DLL_EXPORT extern time_t    monitor_period;
GOODS_DLL_EXPORT extern semaphore monitor_sem;
GOODS_DLL_EXPORT extern event     monitor_term_event;

GOODS_DLL_EXPORT void task_proc monitor(void* arg);

GOODS_DLL_EXPORT boolean arg2int(char* arg, int& val);

GOODS_DLL_EXPORT boolean authenticate_user();

GOODS_DLL_EXPORT void administrator_dialogue(char* database_config_name,
                                             dbs_server& server);

GOODS_DLL_EXPORT void acceptAdminConnections(char* database_config_name,
                                             dbs_server& server);

GOODS_DLL_EXPORT int goodsrv(int argc, char **argv);

GOODS_DLL_EXPORT void start_goods_server(char const* storage_name, bool wait_for_startup = false );
																   //OR@13.06.2004
GOODS_DLL_EXPORT void stop_goods_server();


/**
 * Performs an online backup in case of embedded server.
 *
 * @author OR@07.06.2004
 * @param backup_file_name - full qualified path and name of backup file
 * @param notify_on_completion - if set to true, this method terminates after completion of backup
 *                               and returns the result of the backup. Makes online backup synchron!
 * @return true, if notify_on_completion is set to true and the backup was successful
 *         true (always), if notify_on_completion is set to false
 *         false, if notify_on_completion is set to true and the backup was not successful
 */
GOODS_DLL_EXPORT bool backup_odb( const char* backup_file_name, bool notify_on_completion = false );


/**
 * Performs a restore from a previous database backup.
 * @author OR@07.06.2004
 *
 * @param backup_file_name - full qualified path and name of backup file
 * @return true if restore was successful, else false
 * @precondition disconnect database client from server because server will stopped and restarted.
 * @postcondition re-connect databse client to server
 */
GOODS_DLL_EXPORT bool restore_odb( const char* backup_file_name );


/**
 * Defragments object index and database files.
 * @author OR@08.06.2004
 * @precondition disconnect database client from server because server will stopped and restarted.
 * @postcondition re-connect databse client to server
 */
GOODS_DLL_EXPORT void compactify_odb();



class GOODS_DLL_EXPORT confgrtr {
  public:
    confgrtr(void);
    virtual ~confgrtr(void);

    virtual void accept_garcc_connections(void);
    virtual int  administer(dbs_server& server, boolean batchmode);
    virtual int  serve(int argc, char **argv);

    virtual storage_server* create_server(
        stid_t                 sid,
        object_access_manager& omgr,                                      
        pool_manager&          pmgr,                                      
        class_manager&         cmgr,                                      
        memory_manager&        mmgr,                                      
        server_transaction_manager&   tmgr,
        size_t                 object_cluster_size = 0,
        boolean                accept_remote_connections = True,
        char const*            passwd_file_path = NULL,
        char const*            ext_blob_dir = NULL);
    
    virtual boolean destroy_server(storage_server *server);
    
    virtual boolean load_configuration(char const* goodsrv_cfg_file_name);
   
    
	/**
     * @author OR@07.06.2004
     */
    char* get_cfg_name() { return cfg_name; };


    /**
     * @author OR@07.06.2004
     */
    storage_server* get_pserver() { return pserver; };

  protected:
    char* cfg_name;
    char* srv_name;
    storage_server *pserver;
};

END_GOODS_NAMESPACE

#endif
