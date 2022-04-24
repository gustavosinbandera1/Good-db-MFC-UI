// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< POOLMGR.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     18-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 29-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Pool of pages used to access server file
//-------------------------------------------------------------------*--------*

#include "server.h"

BEGIN_GOODS_NAMESPACE

#define INCREMENTAL_MAP_INIT_SIZE (1024*1024)
#define INCREMENTAL_BACKUP_FLAG   ((nat8)1 << 63)
#define INIT_PAGE_TIMESTAMP       1

pool_manager::~pool_manager() {}

inline void page_pool_manager::crypt(char* pg)
{
    for (size_t i = 0, n = page_size; i < n; i++) { 
        pg[i] ^= cipher[i];
    }
}


inline unsigned page_pool_manager::hash_function(fposi_t pos)
{
    return (nat4(pos >> page_bits) ^ nat4(pos >> 32))
           % PAGE_POOL_HASH_TABLE_SIZE;
}

inline void page_pool_manager::save_page(page_header* ph)
{
    file::iop_status status;
    if (cipher == NULL) { 
        cs.leave();
        status = page_file->write(ph->page_pos, ph->page_data, page_size);
        cs.enter();
    } else { 
        memcpy(encrypt_buffer, ph->page_data, page_size);
        crypt(encrypt_buffer);
        status = page_file->write(ph->page_pos, encrypt_buffer, page_size);
    }
    ph->state &= ~page_header::dirty;
    if (status != file::ok) {
        msg_buf buf;
        page_file->get_error_text(status, buf, sizeof buf);
        console::error("Failed to write page to file: %s\n", buf);
    } else if (ph->page_pos + page_size > file_size) {
        file_size = ph->page_pos + page_size;
    }
}

void page_pool_manager::release(page_header* ph)
{
    cs.enter();
    ph->state &= ~page_header::busy;
    if (ph->state & page_header::wait) {
        ph->state &= ~page_header::wait;
        waiting_busy = False;
        busy_event.signal();
    }
    if (--ph->used == 0) {
        ph->link_after(&lru);
        if (n_lru_blocked) {
            n_lru_blocked -= 1;
            lru_sem.signal();
        }
    }
    cs.leave();
}

inline void page_pool_manager::mark_dirty_page(fposi_t pos)
{
    if (incremental_map != NULL) { 
        inc_cs.enter();
        size_t page_no = pos >> page_bits;
        if (incremental_map->get_mmap_size() <= page_no*sizeof(page_timestamp_t)) { 
            file::iop_status status = incremental_map->set_size((page_no+1)*sizeof(page_timestamp_t), incremental_map->get_mmap_size()*2); 
            if (status != file::ok) {
                msg_buf buf;
                incremental_map->get_error_text(status, buf, sizeof buf);
                console::error("Failed to extend incremental map: %s\n", buf);
            }
        }
        ((page_timestamp_t*)incremental_map->get_mmap_addr())[page_no] = ++last_page_timestamp;
        inc_cs.leave();
    }
}

page_header* page_pool_manager::get(fposi_t page_pos, access_mode mode)
{
    cs.enter();
 
    //const auto msg_flag = console_message_classes::msg_custom | msg_time;
 
    //TRACE_MSG((msg_flag, "page_pool_manager: enter method\n"));
 
    unsigned h = hash_function(page_pos);
    page_header* ph;
 
    if (mode == pg_read) {
        n_page_reads += 1;
    } else {
        n_page_writes += 1;
    }
 
search_page:
    //TRACE_MSG((msg_flag, "page_pool_manager: search page\n"));
    for (ph = hash_table[h]; ph != NULL; ph = ph->collision_chain) {
        if (ph->page_pos == page_pos) {
            if (ph->used++ == 0) {
                ph->unlink();
            }
            while (ph->state & page_header::busy) {
                ph->state |= page_header::wait;
                if (!waiting_busy) {    
                    waiting_busy = True;
                    busy_event.reset();
                }
                //TRACE_MSG((msg_flag, "page_pool_manager: busy_event - before wait\n"));
                busy_event.wait();
                //TRACE_MSG((msg_flag, "page_pool_manager: busy_event - after wait\n"));
            }
            if (mode != pg_read) {
                ph->state |= page_header::dirty;
                mark_dirty_page(page_pos);
            }
            cs.leave();
 
            //TRACE_MSG((msg_flag, "page_pool_manager: exit method (1)\n"));
            return ph;
        }
    }
    //
    // Allocate new page
    //
    n_page_faults += 1;
    ph = free;
    if (ph == NULL) {
        if ((ph = (page_header*)lru.prev) == &lru) {
            n_lru_blocked += 1;
            //TRACE_MSG((msg_flag, "page_pool_manager: lru_sem - before wait\n"));
            lru_sem.wait();
            //TRACE_MSG((msg_flag, "page_pool_manager: lru_sem - after wait\n"));
            goto search_page;
        }
        internal_assert(ph->used == 0);
        ph->unlink();
        if (ph->state & page_header::dirty) {
            //TRACE_MSG((msg_flag, "page_pool_manager: ph->state & page_header::dirty condition met\n"));
            ph->state |= page_header::busy;
            ph->used = 1;
            save_page(ph);
            ph->state &= ~page_header::busy;
            ph->used -= 1;
            if (ph->state & page_header::wait) {
                //TRACE_MSG((msg_flag, "page_pool_manager: ph->state & page_header::wait condition met\n"));
                //
                // While we are saving page new request to this page
                // arrive so try to choose another page for replacing...
                //
                internal_assert(ph->used != 0);
                ph->state &= ~page_header::wait;
                waiting_busy = False;
                busy_event.signal();
 
                //TRACE_MSG((msg_flag, "page_pool_manager: to search_page (1)\n"));
                goto search_page;
            }
            internal_assert(ph->used == 0);
            ph->next = free;
            free = ph;
            page_header** php = &hash_table[hash_function(ph->page_pos)];
            while (*php != ph) {
                internal_assert(*php != NULL);
                php = &(*php)->collision_chain;
            }
            *php = ph->collision_chain;
 
            //TRACE_MSG((msg_flag, "page_pool_manager: to search_page (2)\n"));
            goto search_page;
        }
        page_header** php = &hash_table[hash_function(ph->page_pos)];
        while (*php != ph) {
            internal_assert(*php != NULL);
            php = &(*php)->collision_chain;
        }
        *php = ph->collision_chain;
    } else {
        free = (page_header*)free->next;
    }
 
    ph->collision_chain = hash_table[h];
    hash_table[h] = ph;
    ph->page_pos = page_pos;
    ph->state = (mode == pg_read)
        ? page_header::busy : (page_header::busy | page_header::dirty);
    ph->used = 1;
    if (mode != pg_read) { 
        mark_dirty_page(page_pos);
    }
    if (mode != pg_write && ph->page_pos < file_size) {
        msg_buf buf;
        size_t available_size = ph->page_pos + page_size <= file_size
            ? page_size : size_t(file_size - ph->page_pos);
        cs.leave();
        file::iop_status status = page_file->read(ph->page_pos, ph->page_data,
                                                  available_size);
        if (status == file::end_of_file) {
            cs.enter();
            status = page_file->get_size(file_size);
            if (status != file::ok) {
                page_file->get_error_text(status, buf, sizeof buf);
                console::error("Failed to read page beyond end of file: %s\n",
                               buf);
            }
            cs.leave();
        } else {
            if (status != file::ok) {
                page_file->get_error_text(status, buf, sizeof buf);
                console::error("Failed to read page from file: %s\n", buf);
            } else if (cipher != NULL) { 
                crypt(ph->page_data);
            }
        }
    } else {
        cs.leave();
    }
 
    //TRACE_MSG((msg_flag, "page_pool_manager: exit method (2)\n"));
    return ph;
}

void page_pool_manager::read(fposi_t pos, void* buf, size_t size)
{
    char* dst = (char*)buf;
    unsigned offs = unsigned(pos & (page_size-1));
    fposi_t page_pos = pos - offs;

    while (True) {
        page_header* ph = get(page_pos, pg_read);
        size_t available = page_size - offs;
        if (available < size) {
            memcpy(dst, ph->page_data + offs, available);
            dst += available;
            size -= available;
            offs = 0;
            page_pos += page_size;
            release(ph);
        } else {
            memcpy(dst, ph->page_data + offs, size);
            release(ph);
            break;
        }
    }
}


void page_pool_manager::write(fposi_t pos, void const* buf, size_t size)
{
    char* src = (char*)buf;
    unsigned offs = unsigned(pos & (page_size-1));
    fposi_t page_pos = pos - offs;

    while (True) {
        size_t available = page_size - offs;
        page_header* ph = get(page_pos,
                              (offs == 0 && size >= page_size)
                               ? pg_write : pg_modify);
        if (available < size) {
            memcpy(ph->page_data + offs, src, available);
            src += available;
            size -= available;
            offs = 0;
            page_pos += page_size;
            internal_assert(ph->state & page_header::dirty);
            release(ph);
        } else {
            memcpy(ph->page_data + offs, src, size);
            internal_assert(ph->state & page_header::dirty);
            release(ph);
            break;
        }
    }
}

boolean page_pool_manager::in_cache(fposi_t pos)
{
    unsigned offs = unsigned(pos & (page_size-1));
    fposi_t page_pos = pos - offs;
    page_header *ph = hash_table[hash_function(page_pos)];

    while (ph != NULL && ph->page_pos != page_pos) {
        ph = ph->collision_chain;
    }
    return ph != NULL;
}


void page_pool_manager::truncate(fsize_t size) 
{
    page_file->set_size(DOALIGN(size, (fsize_t)page_size));
    if (incremental_map != NULL) { 
        incremental_map->close();
        // truncate incremental map
        file::iop_status status = incremental_map->open(file::fa_readwrite, file::fo_random|file::fo_create|file::fo_truncate);
        if (status != file::ok) {
            msg_buf buf;
            page_file->get_error_text(status, buf, sizeof buf);
            server->message(msg_error, "Failed to open incremental map file: %s\n", buf);
        }
    }
}

static int cmp_page_offs(void const* p, void const* q) 
{ 
    return (*(page_header**)p)->page_pos < (*(page_header**)q)->page_pos
	? -1 : (*(page_header**)p)->page_pos == (*(page_header**)q)->page_pos ? 0 : 1;
}

void page_pool_manager::flush()
{
    cs.enter();
    int n = int(pool_size);
    qsort(pages_ptr, n, sizeof(page_header*), &cmp_page_offs);

    for (int i = 0; i < n; i++) {
	page_header* ph = pages_ptr[i];
        if (ph->state & page_header::dirty) {
            if (ph->used++ == 0) {
                ph->unlink();
            }
            if (ph->state & page_header::busy) {
                do { 
                    ph->state |= page_header::wait;
                    if (!waiting_busy) {    
                        waiting_busy = True;
                        busy_event.reset();
                    }
                    busy_event.wait();
                } while (ph->state & page_header::busy);
                if (!(ph->state & page_header::dirty)) {
                    // page was already saved
                    continue;
                }
            }
            save_page(ph);
            if (--ph->used == 0) {
                ph->link_after(&lru);
            }
            if (n_lru_blocked) {
                n_lru_blocked -= 1;
                lru_sem.signal();
            }
        }
    }
    cs.leave();

    if (page_file->flush() != file::ok) {
        console::error("Failed to flush page pool to disk\n");
    }
}

boolean page_pool_manager::open(dbs_server* server)
{
    opened = False;
    this->server = server;
    file::iop_status status = page_file->open(file::fa_readwrite,
                                              file::fo_largefile|file::fo_random|file::fo_directio);

    if (status != file::ok) {
        status = page_file->open(file::fa_readwrite,
                                 file::fo_largefile|file::fo_random|file::fo_create|file::fo_directio);
        file_size = 0;
    } else {
        // it is not so easy to calculate file size for raw partitions
        if (page_file->get_size(file_size) != file::ok) { 
            file_size = MAX_FSIZE;
        }
    }
    if (status == file::ok) {
        memset(hash_table, 0, sizeof hash_table);
        lru.prune();

        size_t n_used_pages = 0;
        if (file_size > 0 && file_size <= pool_size*page_size && cipher == NULL) { 
            file::iop_status rc = file::ok;
#ifdef _WIN32
            for (size_t pos = 0; pos < file_size; pos += FILE_READ_BLOCK) { 
                size_t size = pos + FILE_READ_BLOCK < file_size ? FILE_READ_BLOCK : (size_t)(file_size - pos);
                if ((rc = page_file->read(pos, pages_data + pos, size)) != file::ok) { 
                    break;
                }
            }
#else 
            rc = page_file->read(0, pages_data, (size_t)file_size);
#endif
            if (rc == file::ok) { 
                server->message(msg_notify, "Read the whole database size to page pool\n");
                n_used_pages = (size_t)(file_size/page_size);
                free = NULL;
                for (size_t i = 0; i < n_used_pages; i++) { 
                    page_header* ph = &pages[i];
                    fposi_t page_pos = i*page_size;
                    unsigned h = hash_function(page_pos);
                    ph->page_pos = page_pos;
                    ph->state = 0;
                    ph->used = 0;
                    ph->collision_chain = hash_table[h];
                    hash_table[h] = ph;
                    ph->link_after(&lru);
                }
            }
        }
        free = NULL;
        if (n_used_pages < pool_size) { 
            for (size_t i = n_used_pages+1; i < pool_size; i++) {
                pages[i-1].next = &pages[i];
                pages[i].state = 0;
            }
            pages[n_used_pages].state = 0;
            pages[pool_size-1].next = NULL;
            free = &pages[n_used_pages];
        }
        if (incremental_map) { 
            status = incremental_map->open(file::fa_readwrite, file::fo_random|file::fo_create);
            if (status != file::ok) {
                msg_buf buf;
                page_file->get_error_text(status, buf, sizeof buf);
                server->message(msg_error, "Failed to open incremental map file: %s\n", buf);
            }            
            last_page_timestamp = INIT_PAGE_TIMESTAMP;
            page_timestamp_t* beg = (page_timestamp_t*)incremental_map->get_mmap_addr();
            page_timestamp_t* end = (page_timestamp_t*)(incremental_map->get_mmap_addr() + incremental_map->get_mmap_size());
            while (beg < end) { 
                if (*beg > last_page_timestamp) { 
                    last_page_timestamp = *beg;
                }
                beg += 1;
            }
        }
        opened = True;
        n_page_writes = 0;
        n_page_reads = 0;
        n_page_faults = 0;
    } else {
        msg_buf buf;
        page_file->get_error_text(status, buf, sizeof buf);
        server->message(msg_error, "Failed to open database file: %s\n", buf);
    }
    return opened;
}

void page_pool_manager::close()
{
    cs.enter();

    if (opened) {
        backup_cs.enter(); // wait backup completion
        flush();
        file::iop_status status = page_file->close();
        if (status != file::ok) {
            msg_buf buf;
            page_file->get_error_text(status, buf, sizeof buf);
            console::error("Failed to close page file: %s\n", buf);
        }
        opened = False;
        backup_cs.leave();
        if (incremental_map) { 
            incremental_map->close();
        }
    }
    cs.leave();
}

void page_pool_manager::dump(char*)
{
    fsize_t size;
    if (page_file->get_size(size) == file::ok && size != 0) {
        console::output("Storage file size: %" INT8_FORMAT "u\n", size);
    }
    console::output("Page pool size: %lu\n", pool_size);
    console::output("Page pool access: %u reads, %u writes, %u page faults\n",
                    n_page_reads, n_page_writes, n_page_faults);
}

page_timestamp_t page_pool_manager::get_last_page_timestamp()
{
    return last_page_timestamp;
}

boolean page_pool_manager::backup(file& backup_file, page_timestamp_t last_backuped_page_timestamp)
{
    fsize_t size;
    msg_buf  err;
    file::iop_status status;

    if (last_backuped_page_timestamp > last_page_timestamp) { 
        server->message(msg_error|msg_time,
                        "Incremental backup sequence can not be continued after restore\n");
        return False;
    }
    if (last_backuped_page_timestamp != 0 && incremental_map == NULL) { 
        server->message(msg_error|msg_time,
                        "Failed to perfrom incremental backup since incremental map file was not specified\n");
        return False;
    }
    backup_cs.enter(); // linger manager closing until backup completion
    if (!opened) {
        backup_cs.leave();
        return False;
    }

    if (page_file->get_size(size) != file::ok || (size == 0 && file_size != 0))
    {
        // file is located at raw partition
        size = server->mem_mgr->get_storage_size();
    }
    size = DOALIGN(size, (fsize_t)page_size);
    nat8 pksize = size;
    if (last_backuped_page_timestamp > INIT_PAGE_TIMESTAMP) { 
        pksize |= INCREMENTAL_BACKUP_FLAG;
    }
    pack8(pksize);
    status = backup_file.write(&pksize, sizeof pksize);
    if (status != file::ok) {
        backup_file.get_error_text(status, err, sizeof err);
        server->message(msg_error|msg_time,
                         "Failed to write backup file: %s\n", err);
        backup_cs.leave();
        return False;
    }
    char* encrypt_buf = NULL;
    if (cipher != NULL) { 
        encrypt_buf = NEW char[page_size];
    }
    for (fposi_t pos = 0; pos < size; pos += page_size) {
        if (!server->backup_started) {
            backup_cs.leave();
            delete[] encrypt_buf;
            return False;
        }
        size_t page_no = pos >> page_bits;
        if (last_backuped_page_timestamp != 0) { 
            page_timestamp_t page_timestamp = 0;
            inc_cs.enter();
            if (incremental_map->get_mmap_size() > page_no*sizeof(page_timestamp_t)) { 
                page_timestamp = ((page_timestamp_t*)incremental_map->get_mmap_addr())[page_no];
            } else { 
                file::iop_status status = incremental_map->set_size((page_no+1)*sizeof(page_timestamp_t), incremental_map->get_mmap_size()*2); 
                if (status != file::ok) {
                    msg_buf buf;
                    incremental_map->get_error_text(status, buf, sizeof buf);
                    console::error("Failed to extend incremental map: %s\n", buf);
                    inc_cs.leave();
                    backup_cs.leave();
                    delete[] encrypt_buf;
                    return False;
                }
            }
            if (page_timestamp == 0) { 
                ((page_timestamp_t*)incremental_map->get_mmap_addr())[page_no] = ++last_page_timestamp;
            }
            inc_cs.leave();
            if (page_timestamp-INIT_PAGE_TIMESTAMP < last_backuped_page_timestamp) { // 0 timestamp means that incremental map was created after this page was last saved
                continue;
            }
        }
        page_header* ph = get(pos, pg_read);
        if (last_backuped_page_timestamp > INIT_PAGE_TIMESTAMP) { // first part of incremental backup sequence is stored as full snapshot of the database
            fposi_t pg_pos = pos;
            pack8(pg_pos);
            status = backup_file.write(&pg_pos, sizeof(pg_pos));
            if (status != file::ok) {
                backup_file.get_error_text(status, err, sizeof err);
                server->message(msg_error|msg_time,
                                "Failed to write backup file: %s\n", err);
                backup_cs.leave();
                delete[] encrypt_buf;
                release(ph);
                return False;
            }                               
        }               
        if (cipher != NULL) { 
            memcpy(encrypt_buf, ph->page_data, page_size);
            crypt(encrypt_buf);
            status = backup_file.write(encrypt_buf, page_size);
        } else { 
            status = backup_file.write(ph->page_data, page_size);
        }
        release(ph);
        if (status != file::ok) {
            backup_file.get_error_text(status, err, sizeof err);
            server->message(msg_error|msg_time,
                            "Failed to write page to backup file: %s\n", err);
            backup_cs.leave();
            delete[] encrypt_buf;
            return False;
        }
        task::reschedule();
    }
    delete[] encrypt_buf;
    backup_cs.leave();

    if (last_backuped_page_timestamp > INIT_PAGE_TIMESTAMP) { // end mark
        status = backup_file.write(&pksize, sizeof pksize);
        if (status != file::ok) {
            backup_file.get_error_text(status, err, sizeof err);
            server->message(msg_error|msg_time,
                            "Failed to write backup file: %s\n", err);
            return False;
        }
    }
    return True;
}

void page_pool_manager::stop_backup() {}

boolean page_pool_manager::restore(file& backup_file)
{
    nat8 size;
    msg_buf buf;
    file::iop_status status = backup_file.read(&size, sizeof size);

    if (status != file::ok) {
        backup_file.get_error_text(status, buf, sizeof buf);
        server->message(msg_error,
                         "Failed to read from backup file: %s\n", buf);
        return False;
    }
    unpack8(size);
    int open_flags =  file::fo_largefile|file::fo_create;
    if (!(size & INCREMENTAL_BACKUP_FLAG)) {
        open_flags |= file::fo_truncate;
    }
    status = page_file->open(file::fa_write, open_flags);
    if (status != file::ok) {
        page_file->get_error_text(status, buf, sizeof buf);
        server->message(msg_error,
                         "Failed to open storage data file: %s\n", buf);
        return False;
    }
    char* page = new char[page_size];
    if (size & INCREMENTAL_BACKUP_FLAG) { 
        while (true) { 
            fposi_t pos;
            status = backup_file.read(&pos, sizeof(pos));
            if (status != file::ok) {
                backup_file.get_error_text(status, buf, sizeof buf);
                server->message(msg_error,
                                "Failed to read page from backup file: %s\n",buf);
                page_file->close();
                delete page;
                return False;
            }
            unpack8(pos);
            if (pos == size) {
                break;
            }
            status = backup_file.read(page, page_size);
            if (status != file::ok) {
                backup_file.get_error_text(status, buf, sizeof buf);
                server->message(msg_error,
                                "Failed to read page from backup file: %s\n",buf);
                page_file->close();
                delete page;
                return False;
            }
            status = page_file->write(pos, page, page_size);
            if (status != file::ok) {
                page_file->get_error_text(status, buf, sizeof buf);
                server->message(msg_error,
                                "Failed to restore page from from backup file:"
                                " %s\n", buf);
                page_file->close();
                delete page;
                return False;
            }
        }
    } else { 
        if (incremental_map != NULL) { 
            // truncate incremental map
            status = incremental_map->open(file::fa_readwrite, file::fo_random|file::fo_create|file::fo_truncate);
            if (status == file::ok) {
                incremental_map->close();
            }
        }
        for (fposi_t pos = 0; pos < size; pos += page_size) {
            status = backup_file.read(page, pos + page_size <= size
                                      ? page_size : size_t(size - pos));
            if (status != file::ok) {
                backup_file.get_error_text(status, buf, sizeof buf);
                server->message(msg_error,
                                "Failed to read page from backup file: %s\n",buf);
                page_file->close();
                delete page;
                return False;
            }
            status = page_file->write(page, page_size);
            if (status != file::ok) {
                page_file->get_error_text(status, buf, sizeof buf);
                server->message(msg_error,
                                "Failed to restore page from from backup file:"
                                " %s\n", buf);
                page_file->close();
                delete page;
                return False;
            }
        }
    }
    page_file->close();
    delete page;
    return True;
}

int page_pool_manager::get_page_bits() { return int(page_bits); }

void page_pool_manager::initialize() {}

void page_pool_manager::shutdown() {}

page_pool_manager::page_pool_manager(file& page_file,
                                     size_t pool_size,
                                     int page_size,
                                     char const* cipher_key,
                                     char const* incremental_map_file_name)
    : lru_sem(cs), busy_event(cs)
{
    opened = False;
    waiting_busy = False;

    this->page_file = &page_file;
    this->pool_size = pool_size;
    n_lru_blocked = 0;

    if (page_size == 0) {
        page_size = int(os_file::get_disk_block_size());
    }
    assert(((page_size - 1) & page_size) == 0/* page_size is power of two*/);
    this->page_size = page_size;
    for (page_bits = 0; (1 << page_bits) < page_size; page_bits += 1);

    pages = new page_header[pool_size];
    pages_ptr = new page_header*[pool_size];
    char* p_data = (char*)os_file::allocate_disk_buffer(pool_size*page_size);
    assert(p_data != NULL);
    pages_data = p_data;

    for (size_t i = 0; i < pool_size; i++) {
        pages[i].page_data = p_data;
	pages_ptr[i] = &pages[i];
        p_data += page_size;
    }
    if (cipher_key != NULL) { 
        char state[256];
	for (int counter = 0; counter < 256; ++counter) { 
	    state[counter] = (char)counter;
        }
	int index1 = 0;
	int index2 = 0;
        int key_length = int(strlen(cipher_key));
	for (int counter = 0; counter < 256; ++counter) {
	    index2 = (cipher_key[index1] + state[counter] + index2) & 0xff;
	    char temp = state[counter];
	    state[counter] = state[index2];
	    state[index2] = temp;
	    index1 = (index1 + 1) % key_length;
        }
        int x = 0;
        int y = 0;
        cipher = NEW char[page_size];
        encrypt_buffer = NEW char[page_size];
        for (int i = 0; i < page_size; i++) {
            x = (x + 1) & 0xff;
            y = (y + state[x]) & 0xff;
            char temp = state[x];
            state[x] = state[y];
            state[y] = temp;
            cipher[i] = state[(state[x] + state[y]) & 0xff];
        }
    } else { 
        cipher = NULL;
        encrypt_buffer = NULL;
    } 
    incremental_map = incremental_map_file_name != NULL 
        ? NEW mmap_file(incremental_map_file_name, INCREMENTAL_MAP_INIT_SIZE) : NULL;       
}

page_pool_manager::~page_pool_manager()
{
    os_file::free_disk_buffer(pages_data);
    lru.prune();
    delete[] pages;
    delete[] pages_ptr;
    delete[] encrypt_buffer;
    delete[] cipher;
    delete   incremental_map;
}


END_GOODS_NAMESPACE
