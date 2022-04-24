// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< MEMMGR.H >------------------------------------------------------*--------*
// GOODS                     Version 2.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 12-Nov-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Server store memory manager 
//-------------------------------------------------------------------*--------*

#ifndef __MEMMGR_H__
#define __MEMMGR_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

class dbs_handle; 
class dbs_server;
class client_process;

//
// Storage memory manager abstraction
//

class GOODS_DLL_EXPORT memory_manager {
  public:
    //
    // Allocate new object in store
    //
    virtual opid_t  alloc(cpid_t cpid, size_t size, int flags, opid_t cluster_with,
                          client_process* proc) = 0;
    //
    // Allocate group of objects with assigned OIDs and reserve new OIDs
    //
    virtual void    bulk_alloc(nat4* sizeBuf, nat2* cpidBuf, size_t nObjects, 
                               nat4* opidBuf, size_t nReserved, nat4* nearBuf, int flags, client_process* proc) = 0;
    //
    // Deallocate new object in store
    //
    virtual void    dealloc(opid_t opid) = 0;

    //
    // Get position of object in store file by object identifier
    //
    virtual fposi_t get_pos(opid_t opid) = 0;  
    //
    // Get size of object 
    //
    virtual size_t  get_size(opid_t opid) = 0;  
    //
    // Get class indentifier of object
    //
    virtual cpid_t  get_cpid(opid_t opid) = 0;
    //
    // Get object handle
    //
    virtual void    get_handle(opid_t opid, dbs_handle& hnd) = 0;

    //
    // Get total allocated size (i.e. size of storage). As far as storage can
    // use raw device, it is not possible to find size of storage without
    // this method.
    //
    virtual fsize_t get_storage_size() = 0; 


    //
    // Get total size of aall used object.
    //
    virtual fsize_t get_used_size() = 0; 

    //
    // Get size in bytes of minimal chunk of memory allocation 
    // (allocation granularity). So size allocated for each object is
    // equal to object size aligned to allocation quantum boundary.
    //
    virtual size_t  get_allocation_quantum() = 0;

    //
    // Backup and restore of memory allocation information
    //
    virtual boolean backup(file& backup_file) = 0;
    virtual void    stop_backup() = 0;
    virtual boolean restore(file& backup_file) = 0; 

    //
    // Reovery of memory allocator structures after fault.
    // This function is called twice: first time with parameter stage = 0 
    // before objects will be restored from the log and second time
    // with stage = 1 after restoring all transactions from the log.
    //
    virtual void    recovery(int stage) = 0;

    //
    // Compactify database file
    //
    virtual void    compactify() = 0;

    //
    // Force garbage collection
    //
    virtual void    start_gc() = 0;

    //
    // Log objects with bad references
    //
    virtual void    create_scavenge_task() = 0;
    virtual void    scavenge() = 0;
    virtual void    check_idx_integrity() = 0;

    //
    // Restore or nullify data within an object
    //
    virtual void    close_object_backup() = 0;
    virtual void    set_object_backup(const char* backup_odb_file) = 0;
    virtual void    extract_object_from_backup(const char* arg) = 0;
    virtual void    zero_out_object_data(const char* arg) = 0;
    virtual void    log_object_offset(const char* arg) = 0;

    //
    // Allocate new space for expanded object (if neccesary)
    // This method don't release any memory
    //
    virtual boolean do_realloc(fposi_t& pos, size_t old_size, size_t new_size,
                            client_process* proc) = 0;
    //
    // This method is called when transaction is aborted to eliminate
    // effect of realloc method. If opid == 0, then just free new_size
    // bytes at new_pos location. 
    //
    virtual void    undo_realloc(opid_t opid, 
                                 size_t new_size, fposi_t new_pos) = 0;
    //
    // Update object handle. 
    //
    virtual void    update_handle(opid_t opid, cpid_t new_cpid,
                                  size_t new_size, fposi_t new_pos) = 0;
    //
    // This method is called when server performs recovery after failure
    // for all objects of committed transactions 
    // (after last syncronization point) 
    //
    virtual void    confirm_alloc(opid_t opid, cpid_t cpid, size_t size,
                                  fposi_t pos) = 0; 


    //
    // This method is called when server performs recovery after failure
    // for objects which are restored from the log but latter replaced
    // with other objects (i.e. they were garbage collected).
    //
    virtual void    revoke_object(opid_t old_opid, fposi_t old_pos, size_t old_size,
                                  opid_t new_opid, fposi_t new_pos, size_t new_size) = 0;

    //
    // Check consistency of all object references in storages.
    // This method can be called after crash recovery to check
    // storage files consistency.
    //
    virtual void    check_reference_consistency() = 0;

    //
    // Perform GC inter-server syncronization. Synchronization protocol 
    // protocol is specific for concrete memory_allocators.
    //
    virtual void    gc_sync(stid_t sid, dbs_request const& req) = 0;

    //
    // Request to GC coordinator to abort current garbage collection.
    // This method can be called when fault of some server 
    // is detected. 
    //
    virtual void    gc_abort(boolean wait = True) = 0;

    //
    // Mark object 
    //
    virtual void    gc_mark_object(opid_t opid) = 0;
    //
    // Mark all objects reachable following specified reference
    //
    virtual void    gc_follow_reference(stid_t sid, opid_t opid) = 0; 

    //
    // Flush memory manager structures to disk
    // 
    virtual void    flush() = 0;

    //
    // Initialize storage
    //
    virtual void    initialize() = 0;

    //
    // This method is called before close and should stop 
    // manager activity related with other managers.
    //
    virtual void    shutdown() = 0;

    //
    // Release all resources occupied by disconnected client process
    //
    virtual void    disconnect_client(client_process* proc) = 0;

    virtual void    mark_reserved_objects(client_process* proc) = 0;
    
    virtual void    set_file_size_limit(fsize_t) = 0;
    virtual void    set_objects_limit(size_t max_objects) = 0;

    virtual void    set_extension_quantum(fsize_t) = 0;
    virtual void    set_blob_size(size_t) = 0;
    virtual void    set_blob_offset(size_t) = 0;

    //
    // Specify maximal extension of GC grey references set. When grey 
    // references set is extended by more than specified number of references,
    // then optimization of order of taking references from grey set
    // (improving references locality) is disabled and breadth first order of 
    // object reference graph traversal is used. 
    //
    virtual void   set_gc_grey_set_extension_threshold(size_t max_extension)=0;

    //
    // Set timeout of wainting by coordinators responses from servers
    // to initiate new GC process. 
    //
    virtual void    set_gc_init_timeout(time_t timeout) = 0;

    //
    // Set garbage collection period 
    // (size of allocated memory after which garbage collection is started)
    //
    virtual void    set_gc_init_alloc_size(size_t warermark) = 0;        
    
    //
    // Set garbage collection period (size of used memory in the database 
    // after reaching which garbage collection is started)
    //
    virtual void    set_gc_init_used_size(fsize_t warermark) = 0;        
    
    //
    // Set idle period after which GC will be initiated.
    // If period is 0 feture is disabled. 
    //
    virtual void    set_gc_init_idle_period(time_t period) = 0;
    
    //
    // Set minimal size of allocated memory to start GC (in idle state)
    //
    virtual void    set_gc_init_min_allocated(size_t min_allocated) = 0;

    //
    // Timeout for waiting acknowledgment from GC coordinator for 
    // to finish mark stage and perform sweep stage of GC. If no responce
    // will be received from GC coordinator within this period, GC will
    // be aborted at this server. 
    //
    virtual void    set_gc_response_timeout(time_t timeout) = 0;

    //
    // Perform verification of specified OPID and CPID
    //
    virtual boolean verify_object(opid_t opid, cpid_t cpid) = 0;
    virtual boolean verify_reference(opid_t opid) = 0;

    virtual void    dump(char* what) = 0;

    virtual boolean open(dbs_server*) = 0; 
    virtual void    close() = 0;

    virtual ~memory_manager();
}; 

//
// This class describes component of storage object index. 
// First cell (index 0) is not used for any valid object descriptor.
// Cells from MIN_CPID till MAX_CPID are used for mapping class descriptors.
// Cells greater than MAX_CPID are used to indexing objects. 
// Free cells are linked by 'offs' field in L1 list with header in first cell.
// Field 'size' of first cell contains maximal indentifier of allocated object 
// in storage. 
//
class GOODS_DLL_EXPORT dbs_handle {
  protected:
    nat2 cpid;
    nat2 segm;
    nat4 offs;
    nat4 size;

  public:
    enum { recovered_object_flag = 1 };

    cpid_t get_cpid() {
        return unpack2((char*)&cpid);
    }
    void set_cpid(cpid_t cpid) { 
        this->cpid = cpid;
        pack2(this->cpid);
    }

    nat4 get_size() { 
        return unpack4((char*)&size);
    }
    void set_size(nat4 size) { 
        this->size = size;
        pack4(this->size);
    }

    nat8 get_pos() { 
        return cons_nat8(unpack2((char*)&segm), 
                         ~recovered_object_flag & unpack4((char*)&offs));
    }
    
    void set_pos(nat8 pos) { 
        segm = nat8_high_part(pos);
        pack2(segm);
        offs = nat8_low_part(pos);
        pack4(offs);
    }
        
    boolean is_recovered_object() const { 
        return unpack4((char*)&offs) & recovered_object_flag; 
    }

    void clear_recovered_flag() { 
        unpack4(offs);
        offs &= ~recovered_object_flag;
        pack4(offs);
    }

    boolean is_free() { 
        return cpid == 0; // cpid of object always not zero
    }
    //
    // This two methods are used only for free cells
    //
    opid_t get_next() {
        return unpack4((char*)&offs);
    }
    void   set_next(opid_t opid) {
        offs = opid; 
        pack4(offs); 
    }

    void set(cpid_t cpid, nat8 pos, nat4 size) {
        this->cpid = cpid;
        pack2(this->cpid);
        segm = nat8_high_part(pos);
        pack2(segm);
        offs = nat8_low_part(pos);
        pack4(offs);
        this->size = size; 
        pack4(this->size); 
    }

    void mark_as_free() {
        cpid = 0;
        size = 0;
    }
};

//
// File memory manager abstraction
//

class GOODS_DLL_EXPORT file_memory_manager {
  public:
    //
    // Allocate space for new object in storage. If size of file exceeds 
    // limitation for file size then False is return and space not allocated.
    //
    virtual boolean alloc(size_t size, fposi_t& pos, int flags) = 0;

    //
    // Allocate new space for object if it is impossible to extend
    // current location. If size of file exceeds limitation for file
    // size then False is return and space not allocated.
    //
    virtual boolean do_realloc(size_t size, fposi_t& pos, size_t old_size) = 0;

    //
    // This method can be called for part of object (object truncation),
    // so it should be ready that 'pos' parameter doesn't point to 
    // the beginning of object. 
    //
    virtual void    dealloc(fposi_t pos, size_t size) = 0;

    //
    // This method is called by object memory manager when garbage collection 
    // is finished. File memory manager can change state to force reusing of
    // recently deallocated memory.
    //
    virtual void    gc_finished() = 0;

    //
    // Mark space occupied by object as been used (this method is called
    // only while database recovery procedure).
    //
    virtual void    confirm_alloc(size_t size, fposi_t pos) = 0;
 
    //
    // Remove all allocation information. Space for used objects will
    // be reallocated by "confirm_alloc". This method is called by GC
    // after recovery from fault to peroform complete, non-incremental GC. 
    // 
    virtual void    clean() = 0;

    //
    // Compactify database file
    //
    virtual void    compactify(fposi_t new_used_size) = 0;

    //
    // Check if object memory is properly allocated. This function returns 
    // 0  if object memory is not marked as occupied,
    // 1  if object memory is marked as occupied,
    // -1 if object memory is partly marked as used. 
    //
    virtual int     check_location(fposi_t pos, size_t size) = 0;

    //
    // Synchronize contents of memory file buffers with disks
    //
    virtual void    flush() = 0;

    virtual void    dump(char* what) = 0;

    virtual void    initialize() = 0;

    virtual boolean backup(file& backup_file) = 0;
    virtual void    stop_backup() = 0;
    virtual boolean restore(file& backup_file) = 0;
    
    virtual fsize_t get_storage_size() = 0;

    virtual size_t  get_allocation_quantum() = 0;

    virtual void    set_file_size_limit(fsize_t) = 0;
    
    virtual void    set_extension_quantum(fsize_t) = 0;
    virtual void    set_blob_size(size_t) = 0;
    virtual void    set_blob_offset(size_t) = 0;

    virtual boolean open(dbs_server*) = 0;
    virtual void    close() = 0;

    virtual ~file_memory_manager();
};

// 
// File memory manager implementation
//

#define MEMORY_ALLOC_QUANT_LOG 5
#define MEMORY_ALLOC_QUANT     (1 << MEMORY_ALLOC_QUANT_LOG)
#define INVALID_OBJECT_POSITION 0xEEEEEEEEu // not valid oposition because not aligned in allocation quant, but has to be even because of recovered_object_flag
#define EXTERNAL_BLOB_POSITION 0xB0BAB0BAu // not valid oposition because not aligned in allocation quant, but has to be even because of recovered_object_flag


class GOODS_DLL_EXPORT bitmap_file_memory_manager : public file_memory_manager {
  protected:
    mmap_file* map;
    nat1*      area_beg;  // start of mmaped area
    nat1*      area_end;  // en of mmaped area
    fposi_t    first_free_pos; // first free position in file

    mutex      extend_cs; // critical section for extending bitmap
    fsize_t    max_file_size;
    size_t     blob_size;
    size_t     blob_bitmap_beg;
    size_t     blob_bitmap_cur;
    size_t     non_blob_bitmap_cur;
    size_t     aligned_bitmap_cur;
    size_t     page_bits;
    fsize_t    extension_quantum;
    fsize_t    blob_allocated;
    fsize_t    non_blob_allocated;
    size_t     n_clustered;
    size_t     n_cluster_faults;
    size_t     total_clustered;
    size_t     total_aligned;
    size_t     total_aligned_pages;
    boolean    opened;
    boolean    initialized;


    enum alloc_kind { 
        alk_blob,
        alk_page,
        alk_object
    };

    void    update_bitmap_position(size_t size, fposi_t pos, alloc_kind kind);
    boolean find_hole(size_t size, int obj_bit_size, fposi_t& pos, nat1* cur, nat1* end);
    boolean find_aligned_hole(size_t size, int obj_bit_size, fposi_t& pos, nat1* cur, nat1* end);

    
  public: 
    virtual boolean alloc(size_t size, fposi_t& pos, int flags);
    virtual boolean do_realloc(size_t new_size, fposi_t& pos, size_t old_size);
    virtual void    dealloc(fposi_t pos, size_t size);
    virtual void    confirm_alloc(size_t size, fposi_t pos);
    virtual int     check_location(fposi_t pos, size_t size);

    virtual void    gc_finished();

    virtual void    flush();

    virtual void    clean();

    virtual void    initialize();

    virtual boolean open(dbs_server*);
    virtual void    close();

    virtual void    dump(char* what);

    virtual boolean backup(file& backup_file);
    virtual void    stop_backup();
    virtual boolean restore(file& backup_file);
    
    virtual fsize_t get_storage_size();

    virtual size_t  get_allocation_quantum();

    virtual void    set_file_size_limit(fsize_t);
    virtual void    set_extension_quantum(fsize_t);
    virtual void    set_blob_size(size_t);
    virtual void    set_blob_offset(size_t);    

    virtual void    compactify(fposi_t new_used_size);

    bitmap_file_memory_manager(mmap_file& file, fsize_t file_size_limit=0,
                               fsize_t extension_quant = 0,
                               size_t min_blob_size = 0,
                               fsize_t blob_offset = 0) 
    { 
        map = &file; 
        max_file_size = file_size_limit;
        opened = False; 
        extension_quantum = extension_quant;
        blob_size = min_blob_size;
        set_blob_offset(blob_offset);
    }
};


//
// Storage memory manager implementation
//

#define DEFAULT_GC_INIT_ALLOC_SIZE  (1024*1024)
#define DEFAULT_GC_INIT_USED_SIZE   0
#define DEFAULT_GC_INIT_TIMEOUT     60
#define DEFAULT_GC_RESPONSE_TIMEOUT (24*60*60) // one day 
#define DEFAULT_GC_GREY_SET_LIMIT   1024
#define GC_COORDINATOR              0
#define GC_REFS_BUF_SIZE            1024

struct extern_references {
    nat4 n_import; 
    nat4 n_export;
    void pack() { pack4(n_import); pack4(n_export); }
    void unpack() { unpack4(n_import); unpack4(n_export); }
};

//
// Buffer for collecting exported references 
//
struct export_buffer { 
    int         used; // total number of references in buffer
    int         curr; // number references waiting for been sent to server
    dbs_request req; 
    opid_t      refs[GC_REFS_BUF_SIZE];
};

struct grey_reference { 
    grey_reference* next;    
    opid_t          opid;
};

struct gc_page { 
    gc_page*        next;    
    grey_reference* refs;    
    fposi_t         page_no;
};

//
// This class is used to store grey references at mark stage of garbage 
// collection. To reduce disk IO, object references are sorted by their
// location at disk, so locality of references is enforced. But such 
// optimization can cause unlimited grey references set growth (for example 
// it can place in grey set all references contained in B-Tree). So we use this
// strategy only until grey references set is not extended by more than 
// some specified number of references, whereupon optimization is disabled 
// and breadth first order of references graph traversal 
// is used (for example for B-tree, breadth first order guarantee that 
// grey references set size will not exceed tree height*number of items at the
// page). 
//
class GOODS_DLL_EXPORT grey_references_set { 
    dnm_queue<opid_t> queue;
    int               refs_delta; // pushed - poped
    int               n_grey_refs;
    int               curr_pos;
    fposi_t           base_page;
    fposi_t           next_base;
    gc_page**         page_hash;
    dnm_object_pool   refs_pool;
    dnm_object_pool   page_pool;
    boolean           rewind;
    
  public: 
    int               max_set_extension;

    opid_t            get();
    void              put(opid_t opid, fposi_t page_no);
    void              reset();
    void              put_root();
    unsigned          size() { return n_grey_refs; }

    grey_references_set(size_t max_set_extension);
    ~grey_references_set() { delete[] page_hash; }
};




class GOODS_DLL_EXPORT gc_memory_manager : public memory_manager { 
  protected:
    mutex                cs; 
    file_memory_manager* fmm;  
    boolean              fmm_created;
    boolean              opened; 
    boolean              initialized;
    dbs_server*          server; 
    int                  n_servers; // number of servers in database


    mmap_file*           index;  // object index
    dbs_handle*          index_beg;
    dbs_handle*          index_end;
   
    //
    // Block clinets if limitation for file size or number of objects in
    // storage are reached. 
    //
    eventex              gc_wait_event;
    volatile boolean     gc_wait_flag;

    //
    // Limitation for number of objects in storage
    //
    size_t               n_objects_limit; 
    
    //
    // If list of free object descriptors is not up-to-date, 
    // then GC should perform complete rebuild of the list
    //
    boolean              rebuild_free_list; 
    
    boolean              shutdown_flag; // flag is set by shutdown() method 

    long                 n_requests; // number of requests proceed by manager

    //
    // Period of time to initiate GC if no reqests for memory managers
    // were issued within this period
    //
    time_t               gc_init_idle_period;
    size_t               gc_init_min_allocated;
    semaphore            gc_init_sem;
    event                gc_init_task_event;
    
    static void task_proc start_gc_init_process(void* arg);
    void                 gc_init_process();

    mutex                extend_cs; // critical section for extending index

    //
    // This two variables are calculated by GC and used only for information
    //
    size_t               n_used_objects;
    fsize_t              total_used_space; // total used space in storage

    //
    // Maximal delay of client application due to not enough speed of GC
    //
    time_t               max_gc_delay;

    //
    // This fields are used to reduce number of objects scanned by GC.
    // 
    cpid_t               max_cpid;      // maximal class identifier in storage
    opid_t               max_opid;      // maximal object identifier in storage

    enum gc_sync_cop { // garbage collector syncrobization operations
        gcc_init,   // request to coordinator to initiate GC
        gcc_prepare,// coordinator ask if all other servers are ready for GC
        gcc_ready,  // positive response to coordinator for 'gcc_prepare' 
        gcc_busy,   // negative response to coordinator for 'gcc_prepare' 
        gcc_cancel, // coordinator informs servers that GC will not be started
        gcc_mark,   // request from coordinator to perform mark phase of GC
        gcc_refs,   // message with extern references
        gcc_abort,  // request from coordinator to abort garbage collection 
        gcc_finish, // information to coordinator about finishing of mark phase
        gcc_sweep   // request from coordinator to perform sweep phase of GC 
    };

    enum { 
        gcs_none    = 0x00,
        gcs_prepare = 0x01, // garbage collection in prepare stage
        gcs_active  = 0x02, // garbage collection active
        gcs_mark    = 0x04, // mark phase in progress
        gcs_cont    = 0x08, // continue mark phase (grey references exist)
        gcs_sweep   = 0x10, // sweep phase in progress
        gcs_abort   = 0x20  // abort garbage collection
    }; 
    volatile int         gc_state; 
    
    size_t               gc_allocated;   // total since of memory allocated
                                         // since last garbage collection 
    size_t               gc_watermark;   // size of allocated memory after 
                                         // which garbage collection is started
    fsize_t              gc_used_watermark; // size of used memory in database
                                         // file after reaching which new GC 
                                         // iteration is started
    mutex                gc_cs;          // syncronize access to gc data
    semaphorex           gc_sem;         // wait responce sempahore
    event                gc_term_event;  // GC thread is terminated

    //
    // Array of extern reference counters exported and imported
    // from all other servers.
    //
    extern_references*   gc_extern_references;    

    //
    // This matrix is created only at GC coordinator.
    // Each row of this matrix contains copy of 'gc_extern_references' array
    // obtained from correspondent server with gcc_finish request. 
    // Servers from which 'gcc_finish' request was received can be 
    // destinguished by checking diaganal element of matrix. Initially
    // import != export for server itself.
    //
    extern_references**  gc_refs_matrix;

    //
    // Buffers for exported references, There is one buffer for each server. 
    //
    export_buffer*       gc_export_buf;

    grey_references_set  gc_grey_set;
    int                  page_bits;

    nat4*                gc_map;      // bitmap of object identifiers 
    size_t               gc_map_size; // allocated size of bitmap
    
    time_t               gc_timestamp;// timestamp set by coordinator
                                      // used to identify current GC process. 
    time_t               gc_last_timestamp; // used to make timestamp unique

    int                  n_ready_to_gc_servers;
    time_t               gc_init_timeout; 

    time_t               gc_max_time; // maximal time of GC completion
    time_t               gc_total_time;// total time spent in GC
    size_t               n_completed_gc;// number of GC since storage open

    boolean              gc_background; // perform garbage collection in background

    size_t*              profile_instances;
    nat8*                profile_size;
    int                  n_profiled_classes;

    //
    // Timeout for waiting acknowledgment from GC coordinator for 
    // to finish mark stage and perform sweep stage of GC. If no responce
    // will be received from GC coordinator within this period, GC will
    // be aborted at this server. 
    //
    time_t               gc_response_timeout;

    dnm_buffer           gc_buf;      // buffer for loading marked objects

    void                 gc_scan_object(opid_t opid); 
    void                 gc_extend_map();
    void                 gc_initialize();
    void                 gc_initiate();
    boolean              gc_start();
    boolean              gc_finish();
    boolean              gc_global_mark_finished();

    void                 gc_note_reference(stid_t sid, opid_t opid);

    static void task_proc start_garbage_collection(void* arg);    
    virtual void         gc_mark();    
    virtual void         gc_sweep();    

    boolean try_alloc(fposi_t& pos, size_t size, int flags, client_process* proc);
    boolean check_object_class(cpid_t old_cpid, cpid_t new_cpid);

  public:
    virtual fposi_t get_pos(opid_t opid);  
    virtual size_t  get_size(opid_t opid);  
    virtual cpid_t  get_cpid(opid_t opid);
    virtual void    get_handle(opid_t opid, dbs_handle& hnd);

    virtual opid_t  alloc(cpid_t cpid, size_t size, int flags, opid_t cluster_with, client_process* proc);
    virtual void    bulk_alloc(nat4* sizeBuf, nat2* cpidBuf, size_t nObjects, 
                               nat4* opidBuf, size_t nReserved, nat4* nearBuf, int flags, client_process* proc);
    virtual void    dealloc(opid_t opid);
    virtual boolean do_realloc(fposi_t& pos, size_t old_size, size_t new_size,
                            client_process* proc); 
    virtual void    undo_realloc(opid_t opid, size_t new_size,fposi_t new_pos);

    virtual void    update_handle(opid_t opid, cpid_t new_cpid, 
                                  size_t new_size, fposi_t new_pos);

    virtual void    confirm_alloc(opid_t opid, cpid_t cpid, size_t size,
                                  fposi_t pos); 
 
    virtual void    revoke_object(opid_t old_opid, fposi_t old_pos, size_t old_size,
                                  opid_t new_opid, fposi_t new_pos, size_t new_size);

    virtual void    check_reference_consistency();

    virtual void    flush();

    virtual void    gc_sync(stid_t sid, dbs_request const& req);
    virtual void    gc_abort(boolean wait = True);

    virtual void    gc_follow_reference(stid_t sid, opid_t opid); 
    virtual void    gc_mark_object(opid_t opid);

    virtual boolean open(dbs_server* server); 
    virtual void    initialize();
    virtual void    shutdown();
    virtual void    close();

    virtual void    compactify();
    virtual void    start_gc();
    virtual void    create_scavenge_task();
    virtual void    scavenge();
    virtual void    check_idx_integrity();
    virtual void    close_object_backup();
    virtual void    set_object_backup(const char* backup_odb);
    virtual void    extract_object_from_backup(const char* arg);
    virtual void    zero_out_object_data(const char* arg);
    virtual void    log_object_offset(const char* arg);
    boolean         get_dbs_handle(const char* arg, dbs_handle& o_hnd);

    static void task_proc start_scavenge(void* arg);

    virtual void    disconnect_client(client_process* proc);

    virtual void    mark_reserved_objects(client_process* proc);

    virtual void    dump(char* what);

    virtual boolean backup(file& backup_file);
    virtual void    stop_backup();
    virtual boolean restore(file& backup_file); 

    virtual void    recovery(int stage);

    virtual fsize_t get_storage_size(); 
    virtual fsize_t get_used_size(); 

    virtual size_t  get_allocation_quantum();

    virtual void    set_file_size_limit(fsize_t);

    virtual void    set_objects_limit(size_t max_objects);

    virtual void    set_gc_init_timeout(time_t timeout);

    virtual void    set_gc_init_alloc_size(size_t watermark);        

    virtual void    set_gc_init_used_size(fsize_t watermark);        

    virtual void    set_gc_init_idle_period(time_t period); 

    virtual void    set_gc_init_min_allocated(size_t min_allocated);

    virtual void    set_gc_response_timeout(time_t timeout);

    virtual void    set_gc_grey_set_extension_threshold(size_t max_extension);

    virtual void    set_extension_quantum(fsize_t quantum);
    virtual void    set_blob_size(size_t);
    virtual void    set_blob_offset(size_t);

    virtual boolean verify_object(opid_t opid, cpid_t cpid);
    virtual boolean verify_reference(opid_t opid);

    gc_memory_manager(mmap_file& index_file, mmap_file& map_file,
                      boolean gc_background = True, 
                      time_t gc_init_timeout = DEFAULT_GC_INIT_TIMEOUT,
                      size_t gc_init_alloc_size = DEFAULT_GC_INIT_ALLOC_SIZE,
                      fsize_t gc_init_used_size = DEFAULT_GC_INIT_USED_SIZE,
                      time_t gc_init_idle_period = 0, 
                      size_t gc_init_min_allocated = 0,
                      size_t gc_grey_set_threshold = DEFAULT_GC_GREY_SET_LIMIT, 
                      fsize_t max_file_size = 0, 
                      size_t max_objects = 0,
                      time_t gc_response_timeout = DEFAULT_GC_RESPONSE_TIMEOUT,
                      fsize_t extension_quantum = 0,
                      size_t blob_size = 0,
                      fsize_t blob_offset = 0);

    gc_memory_manager(mmap_file& index_file, file_memory_manager& fmm,
                      boolean gc_background = True, 
                      time_t gc_init_timeout = DEFAULT_GC_INIT_TIMEOUT,
                      size_t gc_init_alloc_size = DEFAULT_GC_INIT_ALLOC_SIZE,
                      fsize_t gc_init_used_size = DEFAULT_GC_INIT_USED_SIZE,
                      time_t gc_init_idle_period = 0, 
                      size_t gc_init_min_allocated = 0,
                      size_t gc_grey_set_threshold=DEFAULT_GC_GREY_SET_LIMIT, 
                      size_t max_objects = 0,
                      time_t gc_response_timeout=DEFAULT_GC_RESPONSE_TIMEOUT);
    
    virtual~gc_memory_manager();
}; 
END_GOODS_NAMESPACE

#endif


