// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< TRANSMGR.H >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET   *     ?  *
// (Generic Object Oriented Database System)                        *   /\|  *
//                                                                   *  /  \  *
//                          Created:     18-Jan-97    K.A. Knizhnik * / [] \ *
//                          Last update: 12-Nov-98    K.A. Knizhnik * GARRET *
//------------------------------------------------------------------*--------*
// Transaction manager
//------------------------------------------------------------------*--------*

#ifndef __TRANSMGR_H__
#define __TRANSMGR_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

class dbs_server; 
class client_process; 

//
// This header is written to transaction log before each object. 
// It contains four components: class identifier, position in file, 
// object size and object persistent indentifier. Size of this structure 
// (16 byte) is equal to size of 'dbs_object_header' structure. 
// This convention is used to avoid extra copying of objects in transaction 
// buffer. Instances of this objects are unaligned.
//
class GOODS_DLL_EXPORT dbs_transaction_object_header : public dbs_handle { 
  protected: 
    nat4   opid;
    
  public: 
    opid_t get_opid() { 
        return unpack4((char*)&opid); 
    }
    void   set(cpid_t cpid, fposi_t pos, size_t size, opid_t opid) { 
        pack2((char*)&this->cpid, cpid);
        pack2((char*)&segm, nat8_high_part(pos));
        pack4((char*)&offs, nat8_low_part(pos));
        pack4((char*)&this->size, nat4(size)); 
        pack4((char*)&this->opid, opid); 
    }
}; 

//
// Transaction header is written before transaction body
//

struct dbs_transaction_header {
    nat4 seqno;       // log sequence number
    nat4 tid;         // global transaction identifer (0 if local)
    nat4 coordinator; // transaction coordinator identifier 
    nat4 size;        // total size of transaction (without header)
    nat4 crc;         // CRC for transaction body, filled by trans.manager
    
    enum replication_commands { // special values of the "size" field of transaction header are used
                                // as repliation commands
        REPL_CHECKPOINT = 0x00000000, 
        REPL_SHUTDOWN   = 0xFFFFFFFF, 
        REPL_ACK_NEEDED = 0x80000000 // receive acknowledgement is needed
    };


    dbs_object_header* body() const { // body of transaction
        return (dbs_object_header*)((char*)this + sizeof(*this));
    } 

    dbs_transaction_header* pack();
    dbs_transaction_header* unpack();
}; 
    


//
// Transaction manager abstraction
//

class GOODS_DLL_EXPORT server_transaction_manager { 
  public: 
    // 
    // Complete transaction. The argument 'hdr' points to the header
    // of transaction followed by transaction body.
    // Transaction header is unpacked (hst format) while body of transaction 
    // is packed in network format. Method returns result of transaction 
    // completion: 
    // True if transaction is committed and flase if transaction is aborted. 
    //
    virtual boolean do_transaction(int n_servers, 
                                   dbs_transaction_header* hdr, 
                                   client_process* client) = 0; 
    //
    // Protocol dependent synchronization request
    //
    virtual void  tm_sync(stid_t sender, dbs_request const& req) = 0;

    virtual trid_t get_global_transaction_identifier() = 0;

    virtual void  dump(const char* what) = 0;

    //
    // Set transaction log size limit. After reaching this size
    // checkpoint procedure is started and log is truncated. 
    //
    virtual void  set_log_size_limit(fsize_t log_limit) = 0;

    //
    // Set transaction log size limit when backup is in progress. 
    // When this limit is reached and backup is not yet completed, then all new transactions
    // will be blocked until the end of backup
    //
    virtual void  set_log_size_limit_for_backup(fsize_t log_limit) = 0;

    //
    // Specify initial size of transaction log file. Preextending of
    // log file can significantly reduce time of synchronous write operations
    // to the transaction log, because in this case OS has not to update
    // file metainformation (file size). So total system transaction
    // performance will be imporved.
    //
    virtual void  set_preallocated_log_size(fsize_t init_log_size) = 0;

    //
    // Enable or diable writes to the transaction log.
    // This method can be used to temporary switch-off logging when for example
    // bulk load to the database is performed
    //
    virtual void    control_logging(boolean enabled) = 0;

    //
    // Set timeout of waiting by coordinator of global transaction
    // responces from other servers participated in transaction
    //
    virtual void  set_global_transaction_commit_timeout(time_t timeout) = 0;

    //
    // Set timeout for requesting status of global transaction from 
    // coordinator. 
    //
    virtual void  set_global_transaction_get_status_timeout(time_t timeout)=0;

    //
    // Set period of checkpoint process activations. If checkpoint
    // was started as a result of exceeding log size limit before
    // period timeout expiration, then timeout is reset to initial value.
    // Passing 0 as period value stops current checkpoint process or
    // forces immediate checkpoint if checkpoint process was not started.
    //
    virtual void  set_checkpoint_period(time_t period) = 0;

    enum backup_type { bck_snapshot, bck_permanent };
    virtual void  set_backup_type(backup_type) = 0;

    //
    // Enable or disable dynamic reclustering of objects. If dynamic
    // reclustering of objects is enabled, all objects modified in
    // transaction are sequentially written to new place in the storage
    // (with the assumption that objects modified in one transaction will 
    // be also accessed together in future). <TT>max_object_size</TT>
    // parameter specifies maximal size of object for dynamic reclustering.
    // Zero value of this parameter disables reclustering. 
    //
    virtual void    set_dynamic_reclustering_limit(size_t max_object_size) = 0;

    //
    // Initiate online backup of storage files. 
    // If snapshot backup type has been choosen then backup is terminated after
    // saving consistent state of database and checkpoints will 
    // be enabled. Otherwise if permanent backup type is used, then
    // backup terminates and forces checkpoint after saving all
    // records from transaction log. 
    //
    virtual boolean backup(file& backup_file,
                           time_t start_backup_delay,
                           fsize_t start_backup_log_size,
                           page_timestamp_t& page_timestmp) = 0;
    virtual void    stop_backup() = 0;

    virtual boolean restore(file& backup_file) = 0;

    virtual void    initialize() = 0;
    //
    // This method is called before close and should stop 
    // manager activity related with other managers.
    //
    virtual void    shutdown() = 0;


    //
    // Synchronize state of primary node with standby node.
    // Parameters:
    //   replication_socket - socket connected with standby node
    //   log_offset - offset in the transaction of of the last transaction receovered at standby node
    // Primary node will send to standby node all transaction whioch were committed later that transaction
    // with specified offset. 
    //
    virtual void    start_replication(socket_t* replication_socket, fsize_t log_offset) = 0;

    virtual boolean open(dbs_server* server) = 0;
    virtual void    close() = 0;

    virtual ~server_transaction_manager();
};

//
// Transaction manager implementation
//

class trans_cntl_block;

class GOODS_DLL_EXPORT trans_cntl_block_chain { 
  private: 
    friend class trans_cntl_block;
    trans_cntl_block* chain; 
    mutex             cs;     // mutex for synchronization accesses to chain
  public: 
    trans_cntl_block_chain() { chain = NULL; }
};

class GOODS_DLL_EXPORT trans_cntl_block {
  private:
    trans_cntl_block_chain& tcb_hdr;
    trans_cntl_block* next;   // chain of transaction control blocks

    trid_t       tid;         // transaction identifier
    stid_t       coordinator; // transaction coordinator
    stid_t*      servers;     // servers participated in transaction
    int          n_servers;   // number of servers participated in 
                              // global transaction
    
    int          n_responders;// number of server from which replies are 
                              // expected

    semaphore    reply_sem;   // this sempahoe is signaled when positive
                              // replies from all servers participated in 
                              // transaction are received or when negative
                              // reply is received from any server
    
    boolean      result;      // 'True' if all servers sent positive replies,

    boolean      participant(stid_t sid) { 
        int i = n_servers; 
        while (--i >= 0 && servers[i] != sid);
        return i >= 0;
    }

  public: 
    int          n_replies;   // number of received replies

    dnm_array<stid_t> responder;

    static void    receive_reply(trans_cntl_block_chain& tcb_hdr, 
                                 stid_t sid, stid_t coordinator, 
                                 trid_t tid, boolean result);
    
    static boolean find_active_transaction(trans_cntl_block_chain& tcb_hdr, 
                                           stid_t sid, stid_t coordinator, 
                                           trid_t tid);
    
    static void    abort_transactions(trans_cntl_block_chain& tcb_hdr, 
                                      stid_t sid, stid_t coordinator);
    
    static void    cancel_all_transactions(trans_cntl_block_chain& tcb_hdr,
                                           stid_t self);
                                         
    enum poll_status { ts_refused, ts_confirmed, ts_canceled, ts_timeout };

    poll_status    wait_replies(time_t timeout); 
    void           broadcast_request(dbs_server* server, 
                                     dbs_request const& req);
    
    trans_cntl_block(trans_cntl_block_chain& hdr,
                     trid_t tid, stid_t coordinator, 
                     int n_servers, stid_t* servers,
                     int n_responders); 
    ~trans_cntl_block();
}; 

//
// This class is responsible for support of global transation history
//

class GOODS_DLL_EXPORT global_transaction_history { 
  protected: 
    enum { 
        history_cache_size = 64,
        tid_sync_interval = 256
    };
    file&         log; 
    trid_t        last_assigned_tid;
    trid_t        first_tid_in_cache;
    char          tid_cache[history_cache_size];
    int           n_unsync_tids;
    mutex         cs;
    const boolean sync_log_write;   

    void write(trid_t tid, char c);     
    char read(trid_t tid);
        
  public: 
    trid_t  get_transaction_id();
    boolean get_transaction_status(trid_t  tid);
    void    set_transaction_status(trid_t  tid, boolean committed);
        
    void    dump(const char* what);
    void    recovery();
    boolean open();
    void    close(); 

    global_transaction_history(file& hfile, boolean sync_log_write);
};


struct log_header { 
    nat4 last_tid;
    nat4 normal_completion; // not zero if storage was normally closed

    void pack() { pack4(last_tid); }
    void unpack() { unpack4(last_tid); }
};

struct restored_object { 
    restored_object* left;
    restored_object* right;
    fposi_t          pos;
    size_t           size;
    opid_t           opid;

    void* operator new(size_t, dnm_object_pool& pool) { return pool.alloc(); }
};

#define DEFAULT_MAX_LOG_SIZE  (8*1024*1024) /* bytes */
#define DEFAULT_MAX_LOG_SIZE_FOR_BACKUP  ((fsize_t)1024*1024*1024*1024) /* bytes */
#define DEFAULT_WAIT_TIMEOUT  60          /* sec */
#define DEFAULT_RETRY_TIMEOUT 5           /* sec */
#define MAX_TRANSACTION_SIZE  (512*1024*1024)

#define BACKUP_BUF_SIZE       (16*1024)
#define REPLICATION_BUF_SIZE  (16*1024)

class GOODS_DLL_EXPORT log_transaction_manager : public server_transaction_manager { 
  protected: 
    file&         log;
    
    global_transaction_history history;

    mutex         cs;
    mutex         checkpoint_cs;
    semaphorex    checkpoint_sem;

    volatile boolean checkpoint_in_schedule;// checkpoint process is waiting
                                         // for checpoint_event signaling
    time_t        checkpoint_period;     // period of checkpoint activation
    semaphore     checkpoint_init_sem;   // semaphore for checkpoint activation
    event         checkpoint_term_event; // checpoint task termination event

    int           n_active_transactions;
    boolean       checkpoint_in_progress;

    dbs_server*   server; 
    trans_cntl_block_chain tcb_hdr;

    time_t        commit_timeout;// timeout for waiting replies by coordinator
    time_t        get_status_timeout;  // timeout for requesting transaction 
                                       // status from coordinator
    
    fsize_t       allocated_log_size;  // current reserved space in local log 
    fsize_t       committed_log_size;  // current committed space in local log 
    fsize_t       preallocated_log_size;// initial log size

    fsize_t       log_limit; // limitiation for maximal local log size
                             // afterwards checkpoint procedure is forced

    fsize_t       log_limit_for_backup; // limitiation for maximal local log size during backup

    const boolean sync_log_write; // synchronouse mode of writing to logs

    nat4          seqno;     // log sequence number: this number is incremented
                             // after each checkpoint

    boolean       opened;
    boolean       initialized;

    socket_t*     replication_socket;
    char*         replication_node_name;
    mutex         replication_cs;

    file*         backup_log_file;  
    semaphore     backup_start_sem; 
    semaphorex    backup_sem;
    volatile boolean backup_in_schedule;
    volatile boolean backup_in_progress;
    volatile boolean backup_wait_flag;

    semaphorex    replication_sem;
    volatile boolean replication_wait_flag;
    size_t        n_not_confirmed_transactions;
    
    boolean       permanent_backup; // perform permanenet backup of storage 
    fsize_t       backup_start_log_size;

    time_t        last_backup_time;
    time_t        last_checkpoint_time;

    size_t        dynamic_reclustering_limit; 
    size_t        allocation_quantum;

    boolean       shutdown_flag; // flag is set by shutdown() method 

    boolean       recovery_after_crash;
    boolean       logging_enabled;

    dnm_object_pool  restored_object_pool;
    restored_object* restored_object_tree;
    int              restored_object_tree_depth;

    void intersect(restored_object* rp, opid_t opid, fposi_t pos, size_t size);
    void find_overlapped_objects(opid_t opid, fposi_t pos, size_t size);
    void revoke_overlapped_objects(restored_object* rp, 
                                   opid_t opid, fposi_t pos, size_t size);

    char replication_buf[REPLICATION_BUF_SIZE];
    char backup_buf[BACKUP_BUF_SIZE];
    // 
    // Synchronization request code
    //
    enum tm_sync_codes { 
        tm_confirm,    // confirm request
        tm_refuse,     // refuse request
        tm_checkpoint, // inform coordinator about checpoint 
        tm_get_status  // get status of global transaction
    }; 

    struct trans_obj_info { 
        opid_t  opid;   
        nat4    size;
        nat2    flags;  // copied value of transaction object flags
        nat2    cpid;   // class indentifier
        lck_t   plock;  // object lock mode before trasaction 
    };

    
    fsize_t get_log_limit() { 
        return permanent_backup || backup_in_progress ? log_limit_for_backup : log_limit;
    }

    void complete_transaction(size_t size);

    void abort_transaction(dbs_transaction_object_header* toh, 
                           trans_obj_info* tp, int n,
                           int n_modifed_objects, 
                           client_process* client);
    
    boolean recovery(file& log_file, fsize_t& restored_from_log_size,
                           size_t& n_recovered_transactions);
    void redo_transaction(dnm_buffer& buf);

    boolean synchronize_state(fsize_t& recovered_size, size_t& n_recovered_transactions);

    void checkpoint();
    void checkpoint_process();

    void notify_coordinators();

    void abort_backup(file* backup_file, file* log_file);    

    static void task_proc start_checkpoint_process(void* arg);

  public: 
    virtual boolean do_transaction(int n_servers, 
                                 dbs_transaction_header* hdr,
                                 client_process* client); 

    virtual void    tm_sync(stid_t sender, dbs_request const& req);

    virtual trid_t  get_global_transaction_identifier();

    virtual void    dump(const char* what);

    virtual void    set_log_size_limit(fsize_t log_limit);

    virtual void    set_log_size_limit_for_backup(fsize_t log_limit);

    virtual void    set_preallocated_log_size(fsize_t init_log_size);

    virtual void    control_logging(boolean enabled);

    virtual void    set_global_transaction_commit_timeout(time_t timeout);

    virtual void    set_global_transaction_get_status_timeout(time_t timeout);

    virtual void    set_checkpoint_period(time_t period);

    virtual void    set_dynamic_reclustering_limit(size_t max_object_size);

    virtual boolean backup(file& backup_file,
                           time_t start_backup_delay,
                           fsize_t start_backup_log_size,
                           page_timestamp_t& page_timestmp);
    virtual boolean restore(file& backup_file);
    virtual void    stop_backup();

    virtual void    initialize();
    virtual void    shutdown();

    virtual boolean open(dbs_server* server);
    virtual void    close();

    virtual void    set_backup_type(backup_type);

    virtual void    start_replication(socket_t* replication_socket, fsize_t log_offset);

    log_transaction_manager(file& log_file, 
                            file& history_file,
                            char const* replication_node_name = NULL, 
                            boolean sync_log_write = True, 
                            backup_type online_backup_type = bck_snapshot, 
                            fsize_t log_limit = DEFAULT_MAX_LOG_SIZE,
                            fsize_t log_limit_for_backup = DEFAULT_MAX_LOG_SIZE_FOR_BACKUP,
                            fsize_t preallocated_log_size = 0,
                            time_t checkpoint_init_period = 0,
                            time_t wait_commit_timeout=DEFAULT_WAIT_TIMEOUT, 
                            time_t retry_get_status_timeout = 
                                                       DEFAULT_RETRY_TIMEOUT,
                            size_t reclustering_limit = 0, 
			    boolean logging_enabled = true);
    ~log_transaction_manager();
}; 
END_GOODS_NAMESPACE

#endif

