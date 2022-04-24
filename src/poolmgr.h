// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< POOLMGR.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     18-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 24-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Manager of pages pool to access server file
//-------------------------------------------------------------------*--------*

#ifndef __POOLMGR_H__
#define __POOLMGR_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

//
// Abstraction of pool manager
//

class dbs_server;

class GOODS_DLL_EXPORT pool_manager { 
  public: 
    virtual void    read(fposi_t pos, void* buf, size_t size) = 0;
    virtual void    write(fposi_t pos, void const* buf, size_t size) = 0;

    virtual int     get_page_bits() = 0; // log2(page_size)
    
    //
    // Flush all modified pages on disk
    // 
    virtual void    flush() = 0;

    //
    // Truncate database file
    //
    virtual void    truncate(fsize_t size) = 0;

    //
    // Check if page with such address is in cache
    //
    virtual boolean in_cache(fposi_t pos) = 0;

    virtual void    dump(char* what) = 0;

    virtual boolean backup(file& backup_file, page_timestamp_t last_backuped_page_timestamp) = 0;
    virtual void    stop_backup() = 0;
    virtual boolean restore(file& backup_file) = 0;

    virtual void    initialize() = 0;
    virtual void    shutdown() = 0;

    virtual boolean open(dbs_server*) = 0;
    virtual void    close() = 0;

    virtual page_timestamp_t get_last_page_timestamp() = 0;

    virtual ~pool_manager();
};
    
//
// Implementation of pool manager
//

class GOODS_DLL_EXPORT page_header : public l2elem { // list for LRU discipline
  public:
    page_header* collision_chain;
    fposi_t      page_pos;    
    char*        page_data;

    enum page_state { 
        dirty=1,  // page was modified
        busy=2,   // page is reading from file
        wait=4    // some tasks are waiting for this page
    };
    int          used; // Number of tasks working with page. 
                       // Page can't be replace by LRU discipline until used=0
    int          state; 

    page_header() { state = 0; }
};

#define PAGE_POOL_HASH_TABLE_SIZE  10007
#define DEFAULT_PAGE_POOL_SIZE     1024
#define FILE_READ_BLOCK            1024*1024

class GOODS_DLL_EXPORT page_pool_manager : public pool_manager { 
  protected:
    mutex        cs; 
    semaphorex   lru_sem;   // semaphore to wait for available pages
    int          n_lru_blocked;
    file*        page_file;
    fsize_t      file_size;

    dbs_server*  server;

    page_header* hash_table[PAGE_POOL_HASH_TABLE_SIZE];
    l2elem       lru;  // list of page_headers sorted in sorted 
                       // by access counter to support LRU discipline
    page_header* free; // list of free pages

    char*         pages_data; 
    page_header*  pages;
    page_header** pages_ptr;

    size_t       pool_size; // number of pages in pool
    size_t       page_size;
    size_t       page_bits; // log2(page_size)

    eventex      busy_event; // event used to prevent concurrent read of page from file

    boolean      opened;
    boolean      waiting_busy;

    mutex        backup_cs;
    mutex        inc_cs;

    unsigned     n_page_reads;
    unsigned     n_page_writes;
    unsigned     n_page_faults;

    mmap_file*   incremental_map;
    page_timestamp_t last_page_timestamp;

    char*        cipher; 
    char*        encrypt_buffer; 

    unsigned     hash_function(fposi_t pos);

    void         save_page(page_header* ph);
    
    void         mark_dirty_page(fposi_t pos);

    enum access_mode { 
        pg_read,   // page is not modified
        pg_modify, // part of page is modified
        pg_write   // complete page is modified 
                   // (it is not necessary to read page from disk)
    };
    
    page_header*    get(fposi_t pos, access_mode); 

    void            release(page_header* ph);

    void crypt(char* pg);

  public:
    virtual void    read(fposi_t pos, void* buf, size_t size);
    virtual void    write(fposi_t pos, void const* buf, size_t size);
    virtual void    flush();
    virtual void    truncate(fsize_t size);
    virtual boolean in_cache(fposi_t pos);
    virtual boolean open(dbs_server*);
    virtual void    close();

    virtual int     get_page_bits(); // log2(page_size)

    virtual void    dump(char* what);

    virtual void    initialize();
    virtual void    shutdown();

    virtual boolean backup(file& backup_file, page_timestamp_t last_backuped_page_timestamp);
    virtual void    stop_backup();
    virtual boolean restore(file& backup_file);

    virtual page_timestamp_t get_last_page_timestamp();

    page_pool_manager(file& page_file, 
                      size_t pool_size = DEFAULT_PAGE_POOL_SIZE,
                      int page_size = 0, // If page_size == 0 then system dependent disk block size is used for pool page size
                      char const* cipher_key = NULL,  // Encryption cipher key
                      char const* incremental_map_file_name = NULL);

    virtual~page_pool_manager();
};

END_GOODS_NAMESPACE

#endif

