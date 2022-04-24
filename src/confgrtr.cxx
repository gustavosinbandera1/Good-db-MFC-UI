// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CONFGRTR.CXX >--------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1999  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jun-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 28-Dec-99    M.S. Seter     * GARRET *
//-------------------------------------------------------------------*--------*
// This module contains configurator functions used to parse the database
// server's configuration file and apply the settings stored there.
//-------------------------------------------------------------------*--------*

#include "confgrtr.h"
#include "server.h"
#include "sockfile.h"
#include "tconsole.h"
#include "memmgr.h"
#include "multfile.h"

BEGIN_GOODS_NAMESPACE

unsigned init_map_file_size = 8*1024;/*Kb*/
unsigned init_index_file_size = 4*1024;/*Kb*/
// amount of memory after which allocation GC is initiated
unsigned gc_background = 1; /* enabled */
unsigned gc_init_timeout = 60; /*sec*/
unsigned gc_init_allocated = 1024; /*Kb*/
unsigned gc_init_used = 0; /*Kb*/
unsigned gc_init_idle_period = 0; /*sec (0 - disabled)*/
unsigned gc_init_min_allocated = 0; /*Kb*/
unsigned gc_response_timeout = 24*60*60; /*sec*/
unsigned gc_grey_set_threshold=1024; /* grey references */
unsigned max_data_file_size = 0; /*Kb, 0 - not limited*/
unsigned max_objects = 0; /* 0 - not limites */
unsigned extension_quantum = 0; /*Kb, 0 - not used*/
unsigned blob_size = 0; /*Kb, 0 - not used*/
unsigned blob_offset = 0; /*Kb, 0 - not used*/

unsigned sync_log_writes = True;
unsigned logging_enabled  = True;
unsigned permanent_backup = False;
unsigned trans_preallocated_log_size = 0;/*Kb*/
unsigned trans_max_log_size = 8*1024;/*Kb*/
unsigned trans_max_log_size_for_backup = 1024*1024*1024;/*Kb*/
unsigned trans_wait_timeout = 600; /*sec*/
unsigned trans_retry_timeout = 5; /*sec*/
unsigned checkpoint_period = 0; /*sec (0-disabled) */
unsigned dynamic_reclustering_limit = 0; /*disabled*/

unsigned lock_timeout = 600;

unsigned page_pool_size = 4096;
 
char*    admin_telnet_port;
char*    admin_password;
unsigned cluster_size = 512;
unsigned server_ping_interval = 0;

unsigned accept_remote_connections = True;
unsigned local_access_for_all = True;
unsigned include_credentials_in_backup = False;

socket_file* garcc_file         = NULL;
socket_t*    garcc_gateway      = NULL;
semaphore    garcc_has_shutdown;
semaphore    garcc_op_completed;
char*        garcc_port         = NULL;

char* index_file_name;
char* map_file_name;
char* data_file_name;
char* trans_file_name;
char* history_file_name;
char* incremental_map_file_name;
char* cipher_key;
char* passwd_file_path;
char* external_blob_directory;

char* replication_node_name;

boolean set_max_data_file_size(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_file_size_limit(fsize_t(val)*Kb);
    return True;        
}

boolean set_max_objects(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_objects_limit(val);
    return True;        
}

boolean set_gc_init_timeout(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_gc_init_timeout(time_t(val));
    return True;        
}

boolean set_gc_grey_set_threshold(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_gc_grey_set_extension_threshold(val);
    return True;        
}

boolean set_gc_response_timeout(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_gc_response_timeout(time_t(val));
    return True;        
}

boolean set_extension_quantum(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_extension_quantum(fsize_t(val)*Kb);
    return True;        
}

boolean set_blob_size(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_blob_size(val*Kb);
    return True;        
}

boolean set_blob_offset(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_blob_offset(val*Kb);
    return True;        
}

boolean set_gc_init_allocated(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_gc_init_alloc_size(val*Kb);
    return True;        
}

boolean set_gc_init_used(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_gc_init_used_size(fsize_t(val)*Kb);
    return True;        
}

boolean set_gc_init_idle_period(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_gc_init_idle_period(time_t(val));
    return True;        
}

boolean set_gc_init_min_allocated(dbs_server* srv, unsigned val)
{
    srv->mem_mgr->set_gc_init_min_allocated(val*Kb);
    return True;        
}

boolean set_permanent_backup(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->set_backup_type(val 
                                    ? server_transaction_manager::bck_permanent
                                    :  server_transaction_manager::bck_snapshot);
    return True;
}

boolean set_checkpoint_period(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->set_checkpoint_period(val);
    return True;
}

boolean control_logging(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->control_logging(val ? True : False);
    return True;
}

boolean set_max_log_size(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->set_log_size_limit(fsize_t(val)*Kb);        
    return True;
}

boolean set_max_log_size_for_backup(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->set_log_size_limit_for_backup(fsize_t(val)*Kb);        
    return True;
}

boolean set_preallocated_log_size(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->set_preallocated_log_size(fsize_t(val)*Kb); 
    return True;
}

boolean set_wait_timeout(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->set_global_transaction_commit_timeout(val);
    return True;
}

boolean set_retry_timeout(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->set_global_transaction_get_status_timeout(val);
    return True;
}

boolean set_dynamic_reclustering_limit(dbs_server* srv, unsigned val)
{
    srv->trans_mgr->set_dynamic_reclustering_limit(val);
    return True;
}

boolean set_lock_timeout(dbs_server* srv, unsigned val)
{
    srv->obj_mgr->set_lock_timeout(val);
    return True;
}

boolean set_cluster_size(dbs_server* srv, unsigned val)
{
    srv->set_object_cluster_size_limit(val);
    return True;
}

boolean set_ping_interval(dbs_server* srv, unsigned val)
{
    communication::ping_interval = server_ping_interval;
    return True;
}

boolean set_local_access_for_all(dbs_server* srv, unsigned val)
{
    srv->set_local_access_for_all(val != 0);
    return True;
}

boolean set_include_credentials_in_backup(dbs_server* srv, unsigned val)
{
    srv->set_include_credentials_in_backup(val != 0);
    return True;
}
    

param_binding goodsrv_params[] = { 
{"memmgr.init_map_file_size", &init_map_file_size,
 "initial size for memory map file"},
{"memmgr.init_index_file_size", &init_index_file_size,
 "initial size for object index file"},
{"memmgr.gc_background", &gc_background,
 "perform GC in background"},
{"memmgr.gc_init_timeout", &gc_init_timeout,
 "timeout for GC initiation", set_gc_init_timeout},
{"memmgr.gc_response_timeout", &gc_response_timeout,
 "timeout for waiting GC coordinator response", set_gc_response_timeout},
{"memmgr.gc_init_allocated", &gc_init_allocated,
 "size of allocated since last GC memory after which new GC iteration is started", set_gc_init_allocated},
{"memmgr.gc_init_used", &gc_init_used,
 "size of used memory in the database which GC is started", set_gc_init_used},
{"memmgr.gc_init_idle_period", &gc_init_idle_period,
 "idle period after which GC is initated", set_gc_init_idle_period},
{"memmgr.gc_init_min_allocated", &gc_init_min_allocated,
 "minimal allocated memory to initiate GC", set_gc_init_min_allocated},
{"memmgr.gc_grey_set_threshold", &gc_grey_set_threshold,
 "grey references set extension threshold", set_gc_grey_set_threshold},
{"memmgr.max_data_file_size", &max_data_file_size,
 "limitation for size of storage data file", set_max_data_file_size},
{"memmgr.max_objects", &max_objects,
 "limitation for number of objects in storage", set_max_objects},
{"memmgr.index_file_name", NULL,
 "name of object index file", NULL, &index_file_name},
{"memmgr.map_file_name", NULL,
 "name of memory mapping file", NULL, &map_file_name},
{"memmgr.external_blob_directory", NULL,
 "path to the diredtory for external blobs", NULL, &external_blob_directory},
{"memmgr.extension_quantum", &extension_quantum,
 "database extension quantum (Kb)", set_extension_quantum},
{"memmgr.blob_size", &blob_size,
 "minimal size of BLOB (Kb)", set_blob_size},
{"memmgr.blob_offset", &blob_offset,
 "Offset of allocation BLOB within storage (Kb)", set_blob_offset},


{"transmgr.logging_enabled", &logging_enabled,
 "disable or enable writes to transaction log", control_logging},
{"transmgr.sync_log_writes", &sync_log_writes,
 "synchronous transaction log write mode"},
{"transmgr.permanent_backup", &permanent_backup, 
 "permanent backup type (snapshot backup if 0)", set_permanent_backup},
{"transmgr.max_log_size", &trans_max_log_size, 
 "limitation for transaction log size", set_max_log_size},
{"transmgr.max_log_size_for_backup", &trans_max_log_size_for_backup, 
 "limitation for transaction log size during backup", set_max_log_size_for_backup},
{"transmgr.preallocated_log_size", &trans_preallocated_log_size, 
 "initial size of transaction log file", set_preallocated_log_size},
{"transmgr.checkpoint_period", &checkpoint_period, 
 "checkpoint initation period", set_checkpoint_period},
{"transmgr.wait_timeout", &trans_wait_timeout, 
 "timeout for committing global transaction", set_wait_timeout},
{"transmgr.retry_timeout", &trans_retry_timeout, 
 "timeout for requesting status of global transaction", set_retry_timeout},
{"transmgr.dynamic_reclustering_limit", &dynamic_reclustering_limit, 
 "maximal size of object to be reclustered", set_dynamic_reclustering_limit},
{"transmgr.log_file_name", NULL,
 "name of transaction log file", NULL, &trans_file_name},
{"transmgr.history_file_name", NULL,
 "name of global transaction history file", NULL, &history_file_name},
{"transmgr.replication_node", NULL,
 "address of replication node", NULL, &replication_node_name},

{"objmgr.lock_timeout", &lock_timeout, 
 "timeout for waiting lock granting", set_lock_timeout},

{"poolmgr.page_pool_size", &page_pool_size,
 "size of server page pool"},
{"poolmgr.data_file_name", NULL,
 "name of storage data file", NULL, &data_file_name},
{"poolmgr.incremental_map_file_name", NULL,
 "name of map files used by incremental backup", NULL, &incremental_map_file_name},
{"poolmgr.cipher_key", NULL,
 "Cippher key used to encrupt content of database file", NULL, &cipher_key},

{"server.admin_telnet_port", NULL,
  "administrator telnet hostname and port", NULL, &admin_telnet_port},
{"server.admin_password", NULL,
  "administrator password for login", NULL, &admin_password},
{"server.cluster_size", &cluster_size, 
  "limitation for object cluster size", set_cluster_size},

{"server.remote.connections", &accept_remote_connections,
  "accept remote connections"},
{"server.local_access_for_all", &local_access_for_all,
  "allow access to database from local node for all users", set_local_access_for_all},
{"server.include_credentials_in_backup", &include_credentials_in_backup,
  "store credentials information from passwd file in backup", set_include_credentials_in_backup},
{"server.passwd_file", NULL, 
  "path to the file with users credentials", NULL, &passwd_file_path},

{"server.garcc_port", NULL,
  "GOODS archiver port", NULL, &garcc_port},
{"server.ping_interval", &server_ping_interval,
  "max # seconds of socket inactivity", set_ping_interval},
{NULL}
};

boolean parse_option(char* buf, param_binding* &param, option_value& u)
{
    char* np = buf;
    while (*np == ' ' || *np == '\t') np += 1;
    char* p = np;
    if (*p != '#' && *p != '\n') { 
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '=')p++;
        if (*p == '\0') { 
            return False;
        }
        char* enp = p;
        if (*p++ != '=') { 
            while (*p != '=' && *p != '\0') p += 1;
            if (*p++ != '=') { 
                return False;
            }
        }  
        char c = *enp;
        *enp = '\0';
        for (param_binding* pb = goodsrv_params;
             pb->param_name != NULL;
             pb++)
        {
             if (stricmp(pb->param_name, np) == 0) 
             { 
                 *enp = c;
                 param = pb;
                 if (pb->param_ivalue != NULL) { 
                     if (sscanf(p, "%i", &u.ivalue) != 1) { 
                         return False;
                     }
                 } else { 
                     internal_assert(pb->param_svalue != NULL);
                     while (*p != '"') { 
                         if (*p == '\0') { 
                             return False;
                         }
                         p += 1;
                     }
                     char* name = ++p;
                     while (*p != '"') { 
                         if (*p == '\0') { 
                             return False;
                         }
                         p += 1;
                     }
                     int len = p - name;
                     u.svalue = new char[len+1];
                     memcpy(u.svalue, name, len);
                     u.svalue[len] = '\0';                   
                 }
                 return True;
             }
        }
        *enp = c;
        return False;
    }
    param = NULL;
    return True;
}

void read_goodsrv_configuration(const char* cfg_file_name)
{
    FILE* cfg = fopen(cfg_file_name, "r");
    if (cfg != NULL) { 
        char buf[MAX_CFG_FILE_LINE_SIZE];

        while (fgets(buf, sizeof buf, cfg) != NULL) { 
            option_value u;
            param_binding* pb;
            if (parse_option(buf, pb, u)) { 
                if (pb != NULL) {  
                    if (pb->param_ivalue != NULL) {
                        *pb->param_ivalue = u.ivalue;
                    } else { 
                        if (*pb->param_svalue != NULL) { 
                            delete[] *pb->param_svalue;
                        }
                        *pb->param_svalue = u.svalue;
                    }                   
                }
            } else { 
                console::output("Incorrect string in %s configuration file: %s", cfg_file_name, buf);
            }   
        }
        fclose(cfg);
    }
}

void validate_goodsrv_configuration(void)
{
    if (max_data_file_size > 0) {
        unsigned init_data_size_limit_in_Kb;
        unsigned max_data_size_limit_in_Kb;
        init_data_size_limit_in_Kb = init_map_file_size * 8 * MEMORY_ALLOC_QUANT;
        max_data_size_limit_in_Kb  = max_data_file_size;
        if (init_data_size_limit_in_Kb > max_data_size_limit_in_Kb) {
            console::message(msg_error, "Initial map size of %ld Kb allows an "
                             "initial data file of %ld Kb, which exceeds the maximal limit "
                             "of %ld Kb; using %ld Kb as the maximum instead.\n",
                             init_map_file_size, init_data_size_limit_in_Kb,
                             max_data_size_limit_in_Kb, init_data_size_limit_in_Kb,
                             max_data_size_limit_in_Kb);
        }
    }
}

void on_backup_completion(dbs_server&, file& backup_file, boolean result)
{
    console::message(msg_notify|msg_time, 
                     "Backup to file \"%s\" %s finished\n",
                     backup_file.get_name(), 
                     result ? "successfully" : "abnormally"); 
} 

trace_option trace_options_table[] = { 
    {"error",     msg_error, "messages about some non-fatal errors"},
    {"notify",    msg_notify, "notification messages"}, 
    {"login",     msg_login,   "messages about clients login/logout"},
    {"warning",   msg_warning, "messages about some incorrect operations"},
    {"important", msg_important, "messages about some important events"},
    {"object",    msg_object,"messages about operation with object instances"},
    {"locking",   msg_locking, "object lock related tracing"},
    {"request",   msg_request, "trace requests received by server"},
    {"gc",        msg_gc, "trace garbage collector activity"},
    {"all",       msg_all, "output all messages"},
    {"none",      msg_none, "ignore all messages"},
    {NULL}
};
    

char log_file_name[max_cmd_len];

char      monitor_options[max_cmd_len];
time_t    monitor_period;
semaphore monitor_sem;
event     monitor_term_event;

void task_proc monitor(void* arg)
{
    dbs_server* dbs = (dbs_server*)arg;
    while (monitor_period != 0) { 
        if (!monitor_sem.wait_with_timeout(monitor_period)) { 
            console::output("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
            console::message(msg_output|msg_time, "Monitoring: %s\n", 
                             monitor_options);
            dbs->dump(monitor_options);
        }
    }
    monitor_term_event.signal();
}


boolean arg2int(char* arg, int& val) 
{
    char c;
    return sscanf(arg, "%i %c", &val, &c) == 1; 
}

/*****************************************************************************\
authenticate_user() - Prompts user for password and verifies the given password
\*****************************************************************************/
boolean authenticate_user()
{
  char  buf[max_cmd_len];
  char  cmd[max_cmd_len];

  if (admin_password == NULL) {
    return True;
  }

  if (strlen(admin_telnet_port) == 0) {
    return False;
  }

  console::output("\nEnter password: ");
  if (!console::input(buf, sizeof buf)) {
    return False;
  }

  sscanf(buf, "%s", cmd);

  if (stricmp(cmd, admin_password) != 0) {
    console::output("Invalid password.\n");
    return False;
  }

  return True;
  
}

void administrator_dialogue(char* database_config_name, dbs_server& server, boolean batchmode)
{ 
    char buf[max_cmd_len];
    os_file* backup_file = NULL;
    int number_of_errors = 0;
    int number_of_input_errors = 0;

    console::message(msg_notify, server.opened
                     ? "server is up...\n"
                     : "server is DOWN\n");
    
    while(True) { 
        char  cmd[max_cmd_len];
        char  arg[3][max_cmd_len];
        int   arg_offs[2];
        int   n_args;

        console::output(">");
        if (!console::input(buf, sizeof buf)) { 
            if (strlen(admin_telnet_port) == 0) {
                if (batchmode) { 
                    while (1) { 
                        task::sleep(1000);
                    }
                }
                if (++number_of_input_errors > 1) {
                    break;
                } else { 
                    continue;
                }
            }
            return;
        }
        number_of_input_errors = 0;
        n_args = sscanf(buf, "%s%n%s%n%s%s", cmd, 
                        &arg_offs[0], arg[0], &arg_offs[1], arg[1], arg[2]);
        if (n_args <= 0) { 
            continue;
        }
        if (stricmp(cmd, "open") == 0) { 
            if (server.opened) { 
                console::output("Server is already opened\n");
            } else { 
                console::message(msg_notify, server.open(database_config_name)
                                 ? "GOODS server started...\n"
                                 : "Failed to start server\n");
            }
        } else if (stricmp(cmd, "close") == 0) { 
            if (server.opened) { 
                server.close();
            } else { 
                console::output("Server is not opened\n");
            }
        } else if (stricmp(cmd, "shutdown") == 0) { 
            break;
        } else if (stricmp(cmd, "exit") == 0) { 
            if (strlen(admin_telnet_port) == 0) {
                break;
            }
            return;
        } else if (stricmp(cmd, "logout") == 0) { 
            if (strlen(admin_telnet_port) == 0) {
                break;
            }
            return;
        } else if (stricmp(cmd, "add_user") == 0) { 
            if (n_args != 3) { 
                console::output("Usage: add_user login password\n");
            } else {
                server.add_user(arg[0], arg[1]);
            }
        } else if (stricmp(cmd, "del_user") == 0) { 
            if (n_args != 2) { 
                console::output("Usage: del_user login\n");
            } else {
                if (!server.del_user(arg[0])) { 
                    console::output("No such user\n");
                }
            }
        } else if (stricmp(cmd, "update_credentials") == 0) { 
            server.update_credentials();
        } else if (stricmp(cmd, "rename") == 0) { 
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else if (n_args < 3 || n_args > 4) { 
                console::output("Usage: raname old-class-name new-class-name\n");
            } else if (n_args == 3) {  
                console::output(server.class_mgr->rename_class(arg[0], arg[1])
                                ? "Class renamed\n" : "No such class\n");
            } else { 
                console::output(server.class_mgr->rename_class_component
                                (arg[0], arg[1], arg[2])
                                ? "Component renamed\n" 
                                : "No such class/component\n");
            }
        } else if (stricmp(cmd, "show") == 0) { 
            if (strstr(buf+arg_offs[0], "setting") != NULL) {
                for (param_binding* pb = goodsrv_params;
                     pb->param_name != NULL;
                     pb++)
                {
                    if (pb->param_ivalue != NULL) { 
                        console::output("%s = %u\n", 
                                        pb->param_name, *pb->param_ivalue);
                    } else { 
                        console::output("%s = \"%s\"\n", 
                                        pb->param_name, *pb->param_svalue);
                    }
                }
            } 
            if (server.opened) { 
                server.dump(n_args == 1 ? (char*)SHOW_ALL : buf + arg_offs[0]);
            } else { 
                console::output("Server is not opened\n");
            }
        } else if (stricmp(cmd, "monitor") == 0) { 
            int period;
            if (n_args < 2 || !arg2int(arg[0], period)) { 
                console::output("Monitoring period is not specified\n");
            } else { 
                if (n_args > 2) { 
                    strcpy(monitor_options, buf+arg_offs[1]);
                } else { 
                    strcpy(monitor_options, SHOW_ALL);
                }
                if (monitor_period == 0) { 
                    if (period != 0) {
                        monitor_period = period;
                        monitor_term_event.reset();
                        task::create(monitor, &server);
                    } 
                } else { 
                    monitor_period = period;
                    monitor_sem.signal();
                    if (monitor_period == 0) {
                        monitor_term_event.wait();
                    }
                }
            }
        } else if (strincmp(cmd, "close_extract_odb", 17) == 0) {
            server.mem_mgr->close_object_backup();
        } else if (strincmp(cmd, "set_extract_odb", 15) == 0) {
            server.mem_mgr->set_object_backup(arg[0]);
        } else if ((strincmp(cmd, "extract", 7) == 0) ||
                   (strcmp(cmd, "x") == 0)) {
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else { 
                server.mem_mgr->extract_object_from_backup(arg[0]);
            }
        } else if ((strincmp(cmd, "zero", 4) == 0) || (strcmp(cmd, "z") == 0)) {
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else { 
                server.mem_mgr->zero_out_object_data(arg[0]);
            }
        } else if (strincmp(cmd, "offset", 6) == 0) {
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else { 
                server.mem_mgr->log_object_offset(arg[0]);
            }
        } else if (strincmp(cmd, "scavenge", 8) == 0) { 
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else { 
                server.mem_mgr->create_scavenge_task();
            }
        } else if (strincmp(cmd, "checkidx", 8) == 0) { 
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else { 
                server.mem_mgr->check_idx_integrity();
            }
        } else if (strincmp(cmd, "startgc", 7) == 0) { 
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else { 
                server.mem_mgr->start_gc();
                console::output("Garbage collection process manually started.\n");
            }
        } else if (strincmp(cmd, "compactify", 6) == 0) { 
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else { 
                server.compactify();
                console::output("Database compactified, please restart the server\n");
            }
        } else if (strincmp(cmd, "backup", 6) == 0) { 
            if (!server.opened) { 
                console::output("Server is not opened\n");
            } else if (server.backup_started) { 
                console::output("Backup is already started\n");
            } else { 
                if (n_args >= 2) { 
                    int timeout = 0, log_size = 0;
                    int i = 1;
                    if (i+1 < n_args) { 
                        if (!arg2int(arg[i], timeout)) { 
                            console::output("Backup timeout value is not valid "
                                            "integer constant: '%s'\n", arg[i]);
                            continue;
                        }                
                        i += 1;
                        if (i+1 < n_args && !arg2int(arg[i], log_size)) { 
                            console::output("Backup log size threshold value is "
                                            "not valid integer constant: '%s'\n", 
                                            arg[i]);
                            continue;
                        }                
                    }   
                    delete backup_file;
                    backup_file = new os_file(arg[0]);
                    server.start_backup(*backup_file, timeout, 
                                        fsize_t(log_size)*Kb,
                                        on_backup_completion);
                    console::message(msg_notify|msg_time, "Backup started\n");

                } else { 
                    console::output("Name of backup file is not specified\n");
                }       
            }   
        } else if (stricmp(cmd, "stop") == 0) { 
            if (!server.backup_started) {
                console::output("No backup process is active\n");
            } else { 
                server.stop_backup();
            }
        } else if (stricmp(cmd, "restore") == 0) {  
            if (n_args == 2) { 
                if (server.opened) { 
                    server.close();
                }
                backup_file = new os_file(arg[0]);
                if (!server.restore(*backup_file, database_config_name)) { 
                    console::output("Database restore from backup file %s failed\n", arg[0]);
                    delete backup_file;
                    backup_file = NULL;
                    break;
                }
                delete backup_file;
                backup_file = NULL;
            } else { 
                console::output("Name of backup file is not specified\n");
            }
        } else if (stricmp(cmd, "log") == 0) {
            if (n_args >= 2)
            {
                *log_file_name = '\0';
                if (strcmp(arg[0], "-") == 0) { 
                    console::use_log_file(NULL);
                } else { 
                    FILE* f = fopen(arg[0], "w");
                    if (f == NULL) { 
                        console::output("Failed to open log file '%s'\n",
                                        arg[0]);
                    } else { 
                        strcpy(log_file_name, arg[0]);
                    }
                    console::use_log_file(f);
                }
            } else { 
                if (*log_file_name) { 
                    console::output("Write output to log file '%s'\n", 
                                     log_file_name);
                } else { 
                    console::output("No log file is used\n");
                }
            }
        } else if (stricmp(cmd, "trace") == 0) {
            int n_options = 0;
            int mask = 0;
            char* opt = buf + arg_offs[0];

            while (sscanf(opt, "%s%n", arg[0], arg_offs) == 1) { 
                trace_option* tp = trace_options_table;
                while (tp->option != NULL) { 
                    if (stricmp(arg[0], tp->option) == 0) { 
                        mask |= tp->mask;
                        break;
                    }
                    tp += 1;
                }
                if (tp->option == NULL) { 
                    console::output("Unknown trace option: '%s'\n", arg[0]);
                    break;
                }
                n_options += 1;
                opt += *arg_offs;
            }
            if (n_options == 0) { 
                if (console::trace_mask == 0) { 
                    console::output("All trace messages are ignored\n");
                } else { 
                    trace_option* tp = trace_options_table;
                    console::output("Trace following messages:");
                    while (tp->option != NULL) { 
                        if ((tp->mask & console::trace_mask)
                            && tp->mask != msg_all)
                        {
                            console::output(" %s", tp->option);
                        }
                        tp += 1;
                    }
                    console::output("\n");
                }
            } else { 
                console::trace_mask = mask;
            }
        } else if (stricmp(cmd, "set") == 0) {
            param_binding* pb;
            option_value u;
            if (parse_option(buf+arg_offs[0], pb, u) && pb != NULL) { 
                if (pb->set_param != NULL) { 
                   if (!(*pb->set_param)(&server, u.ivalue)) { 
                       console::output("Invalid value of parameter: %u\n",
                                       u.ivalue);
                   } else { 
                       *pb->param_ivalue = u.ivalue;
                   }
                } else {
                   console::output("This parameter should be set "
                                   "in configuration file\n");
                }
            } else { 
                console::output("Invalid set instruction: %s\n", 
                                buf+arg_offs[0]);
            }                   
        } else { 
            boolean help = stricmp(cmd, "help") == 0; 
            if (help || ++number_of_errors > 1) { 
                number_of_errors = 0;
                if (help && n_args >= 2) { 
                    if (stricmp(arg[0], "trace") == 0) {
                        console::output("\
Syntax: trace [TRACE_MESSAGE_CLASS]\n\
Description: Select classes of output messages:\n");
                        for (trace_option* tp = trace_options_table;
                             tp->option != NULL;
                             tp++)
                        {
                            console::output(" %s - %s\n", 
                                            tp->option, tp->meaning);
                        }
                        continue;
                    }
                    if (stricmp(arg[0], "set") == 0) {
                        console::output("\
Syntax: set PARAMETER = INTEGER_VALUE\n\
Description: Set one of the following parameters:\n");
                        for (param_binding* pb = goodsrv_params; 
                             pb->param_name != NULL;    
                             pb++)
                        { 
                            if (pb->set_param != NULL) { 
                                console::output(" %s - %s\n", 
                                                pb->param_name, 
                                                pb->param_meaning);
                            }
                        }
                        continue;
                    }
                    if (stricmp(arg[0], "show") == 0) {
                        console::output("\
Syntax: show [settings %s]\n\
Description: Show information about storage server current state.\n\
  By default all categories are shown.\n", SHOW_ALL);
                        continue;
                    } 
                    if (stricmp(arg[0], "monitor") == 0) {
                        console::output("\
Syntax: monitor PERIOD [%s]\n\
Description: Perform periodical monitoring of storage server state.\n\
  By default all categories are shown.\n", SHOW_ALL);
                        continue;
                    } 
                } 
                console::output("\
Commands:\n\
  help [COMMAND]                        print information about command(s)\n\
  open                                  open database\n\
  close                                 close database\n\
  logout                                terminate administrative session\n\
  exit                                  terminate administrative session\n\
  shutdown                              shutdown database server\n\
  show [CATEGORIES]                     show current server state\n\
  monitor PERIOD [CATEGORIES]           periodical monitoring of server state\n\
  backup FILE [TIME [LOG_SIZE]]         schedule online backup process\n\
  stop backup                           stop backup process\n\
  restore BACKUP_FILE                   restore database from the backup\n\
  trace [TRACE_MESSAGE_CLASS]           select classes of output messages\n\
  log [LOG_FILE_NAME|\"-\"]               set log file name\n\
  set PARAMETER = INTEGER_VALUE         set server parameters\n\
  rename OLD_CLASS_NAME NEW_CLASS_NAME  rename class in the storage\n\
  rename CLASS COMPONENT_PATH NEW_NAME  rename component of the class\n\
  startgc                               manually start garbage collection\n\
  update_credentials                    update_users credentials from specified passwd-file\n\
  add_user LOGIN PASSWORD               add new user\n\
  del_user LOGIN                        delete user\n\
  update_credentials                    update_users credentials from specified passwd-file\n\
  compactify                            defragment object index and\n\
                                        database files\n"
"\n\
Integrity check and extract from backups commands:\n\
  checkidx                              validate the integrity of this\n\
                                        database storage's object index\n\
                                        (.idx file).  Any problems detected\n\
                                        are logged.\n\
  scavenge                              look for objects whose broken\n\
                                        references will most certainly break\n\
                                        a garbage collection run.  Use this\n\
                                        command if you think your .odb file\n\
                                        suffered corruption (like if you\n\
                                        were running on a server with a bad\n\
                                        RAID controller).\n\
  offset STORAGEID:OBJECTID             show where the specified object is\n\
                                        located in the .odb file (offset &\n\
                                        size).\n\
  set_extract_odb FILE_NAME             open a backup .odb file, for use\n\
                                        with the \"extract\" command.\n\
  close_extract_odb                     close backup .odb file.\n\
  zero STORAGEID:OBJECTID               zero out data for specified object.\n\
  extract STORAGEID:OBJECTID            roll back the specified object to\n\
                                        its version stored in the backup\n\
                                        .odb file.\n\
  \n");
            } else { 
                console::output("Unknown command: %s\n", cmd);
                continue;
            }
        }
        number_of_errors = 0;
    }
    if (server.opened) { 
        server.close();
    }
    if (monitor_period != 0) {
        monitor_period = 0;
        monitor_sem.signal();
        monitor_term_event.wait();
    }
    delete backup_file;
    console::message(msg_notify, "GOODS server is terminated\n");
}


/*****************************************************************************\
acceptAdminConnections - server administration telnet sessions to the adm. port
-------------------------------------------------------------------------------
This function configures a TCP/IP port (specified in the .srv file by the
optional server.admin_telnet_port option) on this localhost to listen for
incoming administrative database telnet requests.  Once requested, this
function serves an administrative console session along the port.  This
function runs until the database is closed.
\*****************************************************************************/

void acceptAdminConnections(char* database_config_name, dbs_server& server)
{
     socket_t *adminGateway = NULL;

     // Listen for incoming administrator console session requests.
     adminGateway = socket_t::create_global(admin_telnet_port);
 if(!adminGateway || !adminGateway->is_ok())
   {
   fprintf(stderr, "Could not obtain database administration telnet port."
                 "Terminating.\n\n");
         return;
     }
     while (server.opened)  {
         socket_t *telnetSession = adminGateway->accept();       
         if (telnetSession) {
             if (!server.opened) { 
                 delete telnetSession;
             } else {
                 console *oldConsole = console::active_console;
                 telnet_console* tconsole = new telnet_console(telnetSession, oldConsole);
                 console::active_console = tconsole;
         if (authenticate_user()) {
             administrator_dialogue(database_config_name, server, False);
         }
                 console::active_console = oldConsole;
                 delete tconsole;
             }
         }
     }
}

/*****************************************************************************\
accept_garcc_connections - accept backup/restore connections on archiver port
-------------------------------------------------------------------------------
This function configures a TCP/IP port (specified in the .srv file by the
OPTIONAL Server.garcc_port option) on this localhost to listen for
incoming database backup or restore requests.  Once requested, this
function initiates a backup or restore process using the socket as the backup
or restore file.

Note that, before a restore operation can begin, you must "close" the database
storage server.  This provides a certain level of security - you can only start
a database restore if you have access to the database' administrative console.

If you enable the garcc server port, it's a very good idea to configure your
system's firewall so that only the remote backup/restore workstation can access
that port.  Otherwise anyone on the Internet who knows about this feature could
use the garcc client to obtain a backup of your database!
\*****************************************************************************/

static void on_garcc_backup_completion(dbs_server&, file&, boolean)
{
    console::message(msg_notify|msg_time, "garcc backup completed\n");
    garcc_op_completed.signal();
}

void task_proc start_garcc(void* i_confgrtr)
{
    ((confgrtr *)i_confgrtr)->accept_garcc_connections();
}

static boolean embedded_server;
static event   embedded_server_shutdown_event;
static event   embedded_server_terminated_event;

static confgrtr* pEmbeddedSvr = 0;          //OR@07.06.2004
static os_file* pEmbeddedSvrBackupFile = 0; //OR@07.06.2004
static boolean online_backup_completion_status;
event  online_backup_finished_evt;          //OR@08.06.2004
event  embedded_server_running;             //OR@13.06.2004

static void task_proc goods_server_main_thread(void* arg) 
{ 
    char* args[2];
    args[1] = (char*)arg;
    goodsrv(2, args);
}

void start_goods_server(char const* storage_name, bool wait_for_startup ) //OR@13.06.2004
{
    task::initialize();
    embedded_server = True;
    embedded_server_shutdown_event.reset();
    embedded_server_terminated_event.reset();
    embedded_server_running.reset(); //OR@13.06.2004

    task::create(goods_server_main_thread, (void*)storage_name, task::pri_normal, task::huge_stack);
    
    if( wait_for_startup ) //OR@13.06.2004
    {
        embedded_server_running.wait();
    }
}


void stop_goods_server()
{
    embedded_server_shutdown_event.signal();
    embedded_server_terminated_event.wait();
    
    //OR@07.06.2004
    if( pEmbeddedSvr )
    {
        delete pEmbeddedSvr;
        pEmbeddedSvr = 0;
    }

    if( pEmbeddedSvrBackupFile )
    {
        delete pEmbeddedSvrBackupFile;
        pEmbeddedSvrBackupFile = 0;
    }
}


int goodsrv(int argc, char **argv)
{
    //OR@07.06.2004
    if( embedded_server )
    {
        pEmbeddedSvr = new confgrtr();
        int retVal = pEmbeddedSvr->serve(argc, argv);
        embedded_server_terminated_event.signal();
        return retVal;
    }
    else
    {
        confgrtr configurator;
        return configurator.serve(argc, argv);
    }
}


static void on_embedded_backup_completion(dbs_server&, file& backup_file, boolean result)
{
    online_backup_completion_status = result;
    online_backup_finished_evt.signal();
} 

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
bool backup_odb( const char* backup_file_name, bool notify_on_completion /* = false */ )
{
    storage_server* pServer = pEmbeddedSvr->get_pserver();
    
    if ( !pServer->opened )
    { 
        return false;
    }
    else if ( pServer->backup_started )
    { 
        return false;
    }
    else
    { 
        if ( backup_file_name )
        { 
            int timeout = 0, log_size = 0;
            
            if( pEmbeddedSvrBackupFile )
            {
                delete pEmbeddedSvrBackupFile;
            }
            pEmbeddedSvrBackupFile = new os_file( backup_file_name );
            
            online_backup_finished_evt.reset();
            
            pServer->start_backup( *pEmbeddedSvrBackupFile,
                                   timeout,                 // no timeout
                                   fsize_t(log_size)*Kb,
                                   on_embedded_backup_completion);                     // no function call on completion

            // if 'notify_on_completion' is true we wait until the backup process 
            // has been finished to return the result
            if ( notify_on_completion )
            {
                online_backup_finished_evt.wait();
                return online_backup_completion_status;
            }
        }    
    }
    return true;  
}


/**
 * Performs a restore from a previous database backup.
 * @author OR@07.06.2004
 *
 * @param backup_file_name - full qualified path and name of backup file
 * @return true if restore was successful, else false
 * @precondition disconnect database client from server because server will stopped and restarted.
 * @postcondition re-connect databse client to server
 */
bool restore_odb( const char* backup_file_name )
{
    storage_server* pServer = pEmbeddedSvr->get_pserver();

    if ( pServer->opened )
    { 
        pServer->close();
    }
    if ( backup_file_name )
    { 
        if( pEmbeddedSvrBackupFile )
        {
            delete pEmbeddedSvrBackupFile;
        }
        pEmbeddedSvrBackupFile = new os_file( backup_file_name );
        return pServer->restore( *pEmbeddedSvrBackupFile, pEmbeddedSvr->get_cfg_name() );
    }

    return false;
}


/**
 * Defragments object index and database files.
 * @author OR@08.06.2004
 * @precondition disconnect database client from server because server will stopped and restarted.
 * @postcondition re-connect databse client to server
 */
void compactify_odb()
{
    storage_server* pServer = pEmbeddedSvr->get_pserver();

    if ( pServer->opened )
    { 
        ((dbs_server*)pServer)->compactify();
        pServer->close();
        pServer->open( pEmbeddedSvr->get_cfg_name() );
    }
}


confgrtr::confgrtr(void)  :
 cfg_name(NULL),
 srv_name(NULL),
 pserver(NULL)
{
}

void confgrtr::accept_garcc_connections(void)
{
    // Listen on the garcc socket for incoming archiver requests.
    garcc_gateway = socket_t::create_global(garcc_port);
    if (!garcc_gateway || !garcc_gateway->is_ok()) {
        console::message(msg_error|msg_time, "Could not obtain garcc archiver "
            "port, \"%s\".\n"
            "Networked backup/restore features are not available;\n"
            "GOODS will continue starting up without them.\n\n",
            garcc_port);
        if (garcc_gateway != NULL) {
            delete garcc_gateway;
        }
        garcc_has_shutdown.signal();
        return;
    }
    console::message(msg_notify|msg_time, "garcc waiting on port %s for "
        "backup or restore requests.\n", garcc_port);
    while (true) {
        socket_t *garcc_session = garcc_gateway->accept();
        if (garcc_session == NULL) {
            break;
        }

        char operation = 0;

        if (!garcc_session->read(&operation, 1)) {
            console::message(msg_error|msg_time, "garcc connection: could not "
                "read operation.");
            delete garcc_session;
            continue;
        }

        if (operation == 'b') {
            int timeout = 0, log_size = 0;

            console::message(msg_notify|msg_time, "garcc backup initiated\n");
            garcc_file = new socket_file(garcc_session);
            pserver->start_backup (*garcc_file,
                                   timeout,                 // no timeout
                                   fsize_t(log_size)*Kb,
                                   on_garcc_backup_completion);
        } else if (operation == 'r') {
            if (pserver->opened) {
                console::message(msg_error|msg_time, "garcc restore failed: "
                    "Restore is possible only for closed server.\n");
                delete garcc_session;
                continue;
            }
            console::message(msg_notify|msg_time, "garcc restore initiated\n");
            garcc_file = new socket_file(garcc_session);
            pserver->restore(*garcc_file, cfg_name);

        } else {
            console::message(msg_error|msg_time, "garcc connection: "
                "unrecognized operation %c received.\n", operation);
            delete garcc_session;
            continue;
        }
     }
    garcc_has_shutdown.signal();
}

storage_server* confgrtr::create_server(
                                        stid_t                 sid,
                                        object_access_manager& omgr,
                                        pool_manager&          pmgr,
                                        class_manager&         cmgr,
                                        memory_manager&        mmgr,
                                        server_transaction_manager&   tmgr,
                                        size_t                 object_cluster_size,
                                        boolean                accept_remote_connections,
                                        char const*            passwd_file_path,
                                        char const*            ext_blob_dir

    )
{
    authentication_manager* authenticator = passwd_file_path != NULL
        ? NEW simple_authentication_manager(passwd_file_path) : NULL;

    return new storage_server(sid, 
                              omgr, 
                              pmgr, 
                              cmgr, 
                              mmgr, 
                              tmgr,
                              object_cluster_size,
                              accept_remote_connections,
                              authenticator,
                              ext_blob_dir);
}
 
boolean confgrtr::destroy_server(storage_server *server)
{
    if (server != NULL) {
        if(server->opened) {
            server->close();
        }
        delete server;
        return True;
    }
    return False;
}

boolean is_multifile_descriptor(char const* file_name) 
{
    size_t len = strlen(file_name);
    return len > 4 && stricmp(file_name + len - 4, ".mfd") == 0;
}

boolean confgrtr::load_configuration(char const* cfg_file_name)
{
    read_goodsrv_configuration(cfg_file_name);
    read_goodsrv_configuration(srv_name);
    validate_goodsrv_configuration();

    communication::ping_interval = server_ping_interval;

    return True;
}

int confgrtr::serve(int argc, char **argv)
{
    int id = 0;
    boolean batchmode = False;
    char const* cfg_file_name = GOODSRV_CFG_FILE_NAME;

    while (argc > 2) {
        if (strcmp(argv[1], "-b") == 0) {
            batchmode = True;
            argc--; argv++;
        } else if (strcmp(argv[1], "-c") == 0) {
            cfg_file_name = argv[2];
            argc -= 2;
            argv += 2;
        } else if (strcmp(argv[1], "-p") == 0) {
            passwd_file_path = argv[2];
            argc -= 2;
            argv += 2;
        } else if (strcmp(argv[1], "-k") == 0) {
            cipher_key = argv[2];
            argc -= 2;
            argv += 2;
         } else { 
            break;
        }
    }
    if (argc < 2 || argc > 4) { 
        console::error("GOODS storage server.\n"
                       "Usage: goodsrv [-b] [-c config-file] [-p passwd-file] [-p cipher-key] <storage name> [<storage-id> [<trace-file-name>]]\n");
    }
    if (argc >= 3) { 
        if (sscanf(argv[2], "%d", &id) != 1) { 
            console::error("Bad storage identifier: '%s'\n", argv[2]);
        }
    }
    if (argc == 4 && strcmp(argv[3], "-") != 0) { 
        FILE* f = fopen(argv[3], "w");
        if (f == NULL) { 
            console::output("Failed to open log file '%s'\n", argv[3]);
        } else { 
            strncpy(log_file_name, argv[3], sizeof log_file_name);
            console::use_log_file(f);
        }
    }
    char* name = argv[1];
    char id_str[8];
    int len = int(strlen(name) + 5);
    *id_str = '\0';
    if (id != 0) { 
        len += sprintf(id_str, "%d", id);
    }

    cfg_name = new char[len];
    srv_name = new char[len];
    data_file_name = new char[len];
    trans_file_name = new char[len];
    history_file_name = new char[len];
    index_file_name = new char[len];
    map_file_name = new char[len];
    admin_telnet_port = new char[1];
    replication_node_name = new char[1];

    sprintf(cfg_name, "%s.cfg", name);        
    sprintf(srv_name, "%s%s.srv", name, id_str);
    sprintf(data_file_name, "%s%s.odb", name, id_str);
    sprintf(trans_file_name, "%s%s.log", name, id_str);
    sprintf(history_file_name, "%s%s.his", name, id_str);
    sprintf(map_file_name, "%s%s.map", name, id_str);
    sprintf(index_file_name, "%s%s.idx", name, id_str);
    *admin_telnet_port = '\0';
    *replication_node_name = '\0';

    load_configuration(cfg_file_name); // Load the configuration from goodsrv.cfg and database.srv 

    if (strlen(admin_telnet_port) > 0) {
        console::active_console = new log_console(console::active_console);
    }

    os_file   odb_file(data_file_name);
    multifile multi_file(data_file_name);
    os_file   log_file(trans_file_name);
    os_file   his_file(history_file_name);
    mmap_file idx_file(index_file_name, init_index_file_size*Kb);
    mmap_file map_file(map_file_name, init_map_file_size*Kb);

    object_manager    obj_mgr((time_t)lock_timeout);
    page_pool_manager pool_mgr(is_multifile_descriptor(data_file_name) ? (file&)multi_file : (file&)odb_file, 
                               page_pool_size, 
                               0, 
                               cipher_key, 
                               incremental_map_file_name);

    dbs_class_manager class_mgr;
    gc_memory_manager mem_mgr(idx_file, map_file, 
                              gc_background != 0, 
                              gc_init_timeout, 
                              gc_init_allocated*Kb,
                              fsize_t(gc_init_used)*Kb,
                              time_t(gc_init_idle_period), 
                              gc_init_min_allocated*Kb, 
                              gc_grey_set_threshold, 
                              fsize_t(max_data_file_size)*Kb, 
                              max_objects,
                              gc_response_timeout,
                              fsize_t(extension_quantum)*Kb,
                              blob_size*Kb,
                              blob_offset*Kb);

    log_transaction_manager trans_mgr(log_file, his_file, 
                                      replication_node_name,
                                      sync_log_writes,
                                      (server_transaction_manager::backup_type)
                                       permanent_backup, 
                                      fsize_t(trans_max_log_size)*Kb,
                                      fsize_t(trans_max_log_size_for_backup)*Kb,
                                      fsize_t(trans_preallocated_log_size)*Kb,
                                      time_t(checkpoint_period), 
                                      time_t(trans_wait_timeout), 
                                      time_t(trans_retry_timeout),
                                      dynamic_reclustering_limit,
                                      logging_enabled);

      pserver = create_server(id, 
                              obj_mgr, 
                              pool_mgr, 
                              class_mgr, 
                              mem_mgr, 
                              trans_mgr,
                              cluster_size,
                              accept_remote_connections,
                              passwd_file_path,
                              external_blob_directory);  // Create and register the storage server object 
      pserver->set_local_access_for_all(local_access_for_all);
      pserver->set_include_credentials_in_backup(include_credentials_in_backup);

      validate_goodsrv_configuration();
      int result = administer(*pserver, batchmode);
      destroy_server(pserver); // Deregister and Destroy it 
      pserver = NULL;
      return result;
}


int confgrtr::administer(dbs_server& server, boolean batchmode)
{
    boolean result = server.open(cfg_name);
    console::message(msg_notify, result
                     ? "GOODS server started...\n"
                     : "Failed to start server\n");
    if (batchmode && !result) {
        return EXIT_FAILURE;
    }
    if (embedded_server) {
        embedded_server_running.signal(); //OR@13.06.2004
        embedded_server_shutdown_event.wait();
        if (server.opened) { 
            server.close();
        }
        //embedded_server_terminated_event.signal();
    } else {
        if (garcc_port && (strlen(garcc_port) != 0)) {
            task::create(start_garcc, this,
                         task::pri_background, task::normal_stack);
        }

        if (strlen(admin_telnet_port) == 0) {
            administrator_dialogue(cfg_name, server, batchmode);
        } else {
            acceptAdminConnections(cfg_name, server);
        }

        if (garcc_gateway != NULL) {
            garcc_gateway->cancel_accept();
            garcc_has_shutdown.wait();
        }
    }
    return EXIT_SUCCESS;
}

confgrtr::~confgrtr(void)
{
    delete[] cfg_name;
    delete[] srv_name;
    delete[] data_file_name;
    delete[] trans_file_name;
    delete[] history_file_name;
    delete[] index_file_name;
    delete[] map_file_name;
    delete[] admin_telnet_port;
    delete[] replication_node_name;
}

END_GOODS_NAMESPACE
