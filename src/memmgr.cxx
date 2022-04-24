// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< MEMMGR.CXX >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 12-Nov-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Server store memory manager
//-------------------------------------------------------------------*--------*

#include "server.h"

#include <map>

BEGIN_GOODS_NAMESPACE

//
// This parameter specifies size of GC window. Bigger value increases sweep
// process speed but also increases client responce delays.
//
#define SWEEP_WINDOW_SIZE 4096U

#define MIN_FILE_SIZE 1024

#ifndef OLD_BACKUP_FORMAT_COMPATIBILITY
#define OLD_BACKUP_FORMAT_COMPATIBILITY 1
#endif

//
// Macros to work with GC object map
//
#define GC_MAP_POS(o)      ((o) >> 5)

#define GC_MAP_BIT(o)      (1 << ((o) & 31))

#define GC_MAP_SIZE(o)     GC_MAP_POS((o)+31)

#define GC_MAP_LAST()      (gc_map_size << 5)

#define WITHIN_GC_MAP(o)   (GC_MAP_POS(o) < gc_map_size)

#define GC_MARKED(o)       (gc_map[GC_MAP_POS(o)] & GC_MAP_BIT(o))

#define GC_MARK(o)         gc_map[GC_MAP_POS(o)] |= GC_MAP_BIT(o)

memory_manager::~memory_manager() {}

file_memory_manager::~file_memory_manager() {}


//
// Bitmap file memory manager
//

boolean bitmap_file_memory_manager::open(dbs_server* server)
{
    msg_buf buf;
    file::iop_status status = map->open(file::fa_readwrite,
                                        file::fo_random|file::fo_create);
    if (status == file::ok) {
        fsize_t file_size;
        status = map->get_size(file_size);
        if (status != file::ok) {
            map->get_error_text(status, buf, sizeof buf);
                console::message(msg_error, "Failed to get file size: %s\n", buf);
            return False;
        }
        size_t size = size_t(file_size);
        page_bits = server->pool_mgr->get_page_bits();
        area_beg = (nat1*)map->get_mmap_addr();
        area_end = area_beg + DOALIGN(size, 8);
        blob_bitmap_cur = 0;
        aligned_bitmap_cur = 0;
        non_blob_bitmap_cur = 0; 
        blob_allocated = 0;
        non_blob_allocated = 0;
        first_free_pos = 0;
        opened = True;
        n_clustered = 0;
        total_clustered = 0;
        total_aligned = 0;
        total_aligned_pages = 0;
        n_cluster_faults = 0;
        initialized = False;
        return True;
    } else {
        map->get_error_text(status, buf, sizeof buf);
        console::message(msg_error, "Failed to open map file: %s\n", buf);
    }
    return False;
}

void bitmap_file_memory_manager::close()
{
    if (opened) {
        extend_cs.enter();
        file::iop_status status = map->close();
        if (status != file::ok) {
            msg_buf buf;
            map->get_error_text(status, buf, sizeof buf);
            console::error("Failed to close mmaped file: %s\n", buf);
        }
        extend_cs.leave();
    }
    opened = False;
}

void bitmap_file_memory_manager::flush()
{
    extend_cs.enter();
    file::iop_status status = map->flush();
    if (status != file::ok) {
        msg_buf buf;
        map->get_error_text(status, buf, sizeof buf);
        console::error("Failed to sync mmaped file: %s\n", buf);
    }
    extend_cs.leave();
}

void bitmap_file_memory_manager::set_file_size_limit(fsize_t file_size_limit)
{
    max_file_size = file_size_limit;
}

void bitmap_file_memory_manager::set_extension_quantum(fsize_t quantum)
{
    extension_quantum = quantum;
}

void bitmap_file_memory_manager::set_blob_size(size_t max_blob_size)
{
    blob_size = max_blob_size;
}

void bitmap_file_memory_manager::set_blob_offset(size_t blob_offset)
{
    blob_bitmap_beg = (blob_offset + MEMORY_ALLOC_QUANT*8 - 1) >> (MEMORY_ALLOC_QUANT_LOG + 3);
}

void bitmap_file_memory_manager::compactify(fposi_t new_used_size)
{
    size_t n_bytes = (size_t)(new_used_size >> (MEMORY_ALLOC_QUANT_LOG+3));
    memset(area_beg, 0xFF, n_bytes);
    int n_bits = (int)(new_used_size >> MEMORY_ALLOC_QUANT_LOG) & 7;
    area_beg[n_bytes] = (1 << n_bits) - 1;
    non_blob_bitmap_cur = n_bytes;
    memset(area_beg + n_bytes + 1, 0, size_t(area_end - area_beg - n_bytes - 1));
    flush();
}

inline void bitmap_file_memory_manager::update_bitmap_position(size_t size, fposi_t pos, bitmap_file_memory_manager::alloc_kind kind)
{
    if (extension_quantum != 0) { 
        if (kind == alk_blob) { 
            if ((blob_allocated += size) >= extension_quantum) { 
                blob_allocated = 0;
                blob_bitmap_cur = 0;
                return;
            } 
        } else { 
            if ((non_blob_allocated += size) >= extension_quantum) { 
                non_blob_allocated = 0;
                nat1* beg = area_beg;
                nat1* end = area_end;
                nat1* cur = beg + size_t(first_free_pos >> (MEMORY_ALLOC_QUANT_LOG+3));
                while (cur < end && *cur == 0xFF) cur += 1;
                first_free_pos = fposi_t(size_t(cur - beg)) << (MEMORY_ALLOC_QUANT_LOG+3);
                non_blob_bitmap_cur = cur - beg;
                aligned_bitmap_cur = 0;
                return;
            } 
        }
    }                   
    size_t bitmap_cur = size_t((pos + size) >> (MEMORY_ALLOC_QUANT_LOG + 3));
    if (kind == alk_blob) {
        blob_bitmap_cur = bitmap_cur;
    } else { 
        size_t step = size_t(1) << (page_bits - MEMORY_ALLOC_QUANT_LOG - 3);
        if (kind == alk_page) {
            aligned_bitmap_cur = DOALIGN(bitmap_cur, step);
        } else {
            non_blob_bitmap_cur = bitmap_cur;
        }
        if (non_blob_bitmap_cur - aligned_bitmap_cur < step) { 
            non_blob_bitmap_cur = aligned_bitmap_cur + step;
        }
    }            
}

static nat1 const first_hole_size [] = {
    8,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
    5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0
};
static nat1 const last_hole_size [] = {
    8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
static nat1 const max_hole_size [] = {
    8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    5,4,3,3,2,2,2,2,3,2,2,2,2,2,2,2,4,3,2,2,2,2,2,2,3,2,2,2,2,2,2,2,
    6,5,4,4,3,3,3,3,3,2,2,2,2,2,2,2,4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
    5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
    7,6,5,5,4,4,4,4,3,3,3,3,3,3,3,3,4,3,2,2,2,2,2,2,3,2,2,2,2,2,2,2,
    5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
    6,5,4,4,3,3,3,3,3,2,2,2,2,2,2,2,4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
    5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,0
};
static nat1 const max_hole_offset [] = {
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,0,1,5,5,5,5,5,5,0,5,5,5,5,5,5,5,
    0,1,2,2,0,3,3,3,0,1,6,6,0,6,6,6,0,1,2,2,0,6,6,6,0,1,6,6,0,6,6,6,
    0,1,2,2,3,3,3,3,0,1,4,4,0,4,4,4,0,1,2,2,0,1,0,3,0,1,0,2,0,1,0,5,
    0,1,2,2,0,3,3,3,0,1,0,2,0,1,0,4,0,1,2,2,0,1,0,3,0,1,0,2,0,1,0,7,
    0,1,2,2,3,3,3,3,0,4,4,4,4,4,4,4,0,1,2,2,0,5,5,5,0,1,5,5,0,5,5,5,
    0,1,2,2,0,3,3,3,0,1,0,2,0,1,0,4,0,1,2,2,0,1,0,3,0,1,0,2,0,1,0,6,
    0,1,2,2,3,3,3,3,0,1,4,4,0,4,4,4,0,1,2,2,0,1,0,3,0,1,0,2,0,1,0,5,
    0,1,2,2,0,3,3,3,0,1,0,2,0,1,0,4,0,1,2,2,0,1,0,3,0,1,0,2,0,1,0,0
};


inline boolean bitmap_file_memory_manager::find_hole(size_t size, int obj_bit_size, fposi_t& pos, nat1* cur, nat1* end)
{
    int hole_bit_size = 0;
    fposi_t quant_no; // offset to object in allocation quants
    while (cur < end) {
        int byte = *cur;
        if (hole_bit_size + first_hole_size[byte] >= obj_bit_size) {
            *cur |= (1 << (obj_bit_size - hole_bit_size)) - 1;
            quant_no = fposi_t(size_t(cur - area_beg))*8 - hole_bit_size;
            pos = quant_no << MEMORY_ALLOC_QUANT_LOG;
            if (hole_bit_size != 0) {
                while ((hole_bit_size -= 8) > 0) {
                    *--cur = 0xFF;
                }
                *(cur-1) |= ~((1 << -hole_bit_size) - 1);
            }
            return True;
        } else if (max_hole_size[byte] >= obj_bit_size) {
            int hole_bit_offset = max_hole_offset[byte];
            *cur |= ((1 << obj_bit_size) - 1) << hole_bit_offset;
            quant_no = fposi_t(size_t(cur - area_beg))*8 + hole_bit_offset;
            pos = quant_no << MEMORY_ALLOC_QUANT_LOG;
            return True;
        }
        hole_bit_size = (last_hole_size[byte] == 8)
            ? hole_bit_size+8 : last_hole_size[byte];
        cur += 1;
    }
    return False;
}


inline boolean bitmap_file_memory_manager::find_aligned_hole(size_t size, int obj_bit_size, fposi_t& pos, nat1* cur, nat1* end)
{
    size_t step = size_t(1) << (page_bits - MEMORY_ALLOC_QUANT_LOG - 3);
    int hole_bit_size = 0;
    fposi_t quant_no; // offset to object in allocation quants
    nat1* beg = area_beg;
    while (cur < end) {
        int byte = *cur;
        if (hole_bit_size + first_hole_size[byte] >= obj_bit_size) {
            *cur |= (1 << (obj_bit_size - hole_bit_size)) - 1;
            quant_no = fposi_t(size_t(cur - beg))*8 - hole_bit_size;
            if (hole_bit_size != 0) {
                while ((hole_bit_size -= 8) > 0) {
                    *--cur = 0xFF;
                }
                *(cur-1) |= ~((1 << -hole_bit_size) - 1);
            }
            pos = quant_no << MEMORY_ALLOC_QUANT_LOG;
            return True;
        } else if (byte != 0) { 
            hole_bit_size = 0;
            cur = beg + DOALIGN(cur - beg + 1, step);
        } else {
            hole_bit_size += 8;
            cur += 1;
        }
    }
    return False;
}


boolean bitmap_file_memory_manager::alloc(size_t size, fposi_t& pos, int flags)
{
    nat1* beg = area_beg;
    nat1* end = area_end;
    nat1* cur;
    alloc_kind kind;
    if (max_file_size != 0
        && area_beg+size_t(max_file_size >> (MEMORY_ALLOC_QUANT_LOG+3)) < end)
    {
        end = area_beg + size_t(max_file_size >> (MEMORY_ALLOC_QUANT_LOG+3));
    }
    int obj_bit_size = int((size + MEMORY_ALLOC_QUANT - 1) >> MEMORY_ALLOC_QUANT_LOG);
    if (blob_size != 0 && size >= blob_size) { 
        kind = alk_blob;
        beg += blob_bitmap_beg;
        cur = beg + blob_bitmap_cur;
        if (find_hole(size, obj_bit_size, pos, cur, end)) {
            update_bitmap_position(size, pos, alk_blob);
            return True;
        }
        blob_allocated = 0;
        internal_assert(cur <= end);
        if (find_hole(size, obj_bit_size, pos, beg, cur)) {
            update_bitmap_position(size, pos, alk_blob);
            return True;
        }
    } else {
        if (flags & aof_clustered) {
            fposi_t cluster_pos = (pos >> page_bits) << page_bits;
            size_t cluster_offs = (size_t)(cluster_pos >> (MEMORY_ALLOC_QUANT_LOG+3));
            size_t step = size_t(1) << (page_bits - MEMORY_ALLOC_QUANT_LOG - 3);
            cur = beg + cluster_offs;
            internal_assert(cur <= end);
            if (find_hole(size, obj_bit_size, pos, cur, cur + step)) { 
                non_blob_allocated += size;
                total_clustered += DOALIGN(size, MEMORY_ALLOC_QUANT);
                n_clustered += 1;
                return True;                
            }
            n_cluster_faults += 1;
        } 
        if (flags & aof_aligned) {
            total_aligned += DOALIGN(size, MEMORY_ALLOC_QUANT);
            total_aligned_pages += DOALIGN(size, size_t(1) << page_bits);
            kind = alk_page;
            cur = beg + aligned_bitmap_cur;
            internal_assert(cur <= end);
            if (find_aligned_hole(size, obj_bit_size, pos, cur, end)) {
                update_bitmap_position(size, pos, alk_page);
                return True;
            }
            non_blob_allocated = 0;
            if (find_aligned_hole(size, obj_bit_size, pos, beg, cur)) { 
                update_bitmap_position(size, pos, alk_page);
                return True;
            }
        } else {         
            kind = alk_object;
            cur = beg + non_blob_bitmap_cur;
            if (find_hole(size, obj_bit_size, pos, cur, end)) { 
                update_bitmap_position(size, pos, alk_object);
                return True;
            }
            non_blob_allocated = 0;
            internal_assert(cur <= end);
            end = cur;
            cur = beg + size_t(first_free_pos >> (MEMORY_ALLOC_QUANT_LOG+3));
            while (cur < end && *cur == 0xFF) cur += 1;
            first_free_pos = fposi_t(size_t(cur - beg)) << (MEMORY_ALLOC_QUANT_LOG+3);
            if (find_hole(size, obj_bit_size, pos, cur, end)) { 
                update_bitmap_position(size, pos, alk_object);
                return True;
            }
        }
    }

    fposi_t          quant_no; // offset to object in allocation quants
    size_t           old_size, new_size, max_size;
    file::iop_status status;

    old_size = size_t(area_end - area_beg);
    new_size = old_size + ((obj_bit_size + 7) >> 3);
    max_size = old_size*2;

    if (max_file_size != 0) {
        if ((fsize_t(new_size) << (MEMORY_ALLOC_QUANT_LOG+3)) > max_file_size){
            return False;
        }
        if (max_size > new_size) {
            if ((fsize_t(old_size) << (MEMORY_ALLOC_QUANT_LOG+4))>max_file_size)
            {
                max_size = size_t(max_file_size >> (MEMORY_ALLOC_QUANT_LOG+3));
            }
        }
    }
    new_size = DOALIGN(new_size, 8);
    extend_cs.enter();
    status = map->set_size(new_size, max_size);
    if (status != file::ok) {
        msg_buf buf;
        map->get_error_text(status, buf, sizeof buf);
        console::error("Failed to reallocate memory map: %s\n", buf);
    }
    quant_no = fposi_t(old_size)*8;
    area_beg = (nat1*)map->get_mmap_addr();
    area_end = area_beg + map->get_mmap_size();
    cur = area_beg + old_size;
    while ((obj_bit_size -= 8) > 0) {
        *cur++ = 0xFF;
    }
    *cur = (1 << (obj_bit_size+8)) - 1;
    pos = quant_no << MEMORY_ALLOC_QUANT_LOG;
    update_bitmap_position(size, pos, kind);
    extend_cs.leave();
    return True;
}


void bitmap_file_memory_manager::clean()
{
    memset(area_beg, 0, size_t(area_end - area_beg));
    first_free_pos = 0;
}

void bitmap_file_memory_manager::dealloc(fposi_t pos, size_t size)
{
    int offs = -int(pos) & (MEMORY_ALLOC_QUANT-1); // offset from end of quant
    if (size > size_t(offs)) {
        pos += offs;
        if (pos < first_free_pos) {
            first_free_pos = pos;
        }
        fposi_t quant_no = pos >> MEMORY_ALLOC_QUANT_LOG;
        int obj_bit_size = int((size - offs + MEMORY_ALLOC_QUANT - 1) >> MEMORY_ALLOC_QUANT_LOG);
        nat1* p = area_beg + size_t(quant_no >> 3);
        int   bit_offs = (int)quant_no & 7;

        if (obj_bit_size > 8 - bit_offs) {
            obj_bit_size -= 8 - bit_offs;
            *p &= (1 << bit_offs) - 1;
            while ((obj_bit_size -= 8) > 0) {
                *++p = 0;
            }
            p[1] &= ~((1 << (obj_bit_size + 8)) - 1);
        } else {
            *p &= ~(((1 << obj_bit_size) - 1) << bit_offs);
        }
    }
}

void bitmap_file_memory_manager::gc_finished()
{
     non_blob_bitmap_cur = (first_free_pos >> (MEMORY_ALLOC_QUANT_LOG+3));
}

boolean bitmap_file_memory_manager::do_realloc(size_t   size,
                                               fposi_t& pos,
                                               size_t   old_size)
{
    int obj_new_bit_size =
        int((size + MEMORY_ALLOC_QUANT - 1) >> MEMORY_ALLOC_QUANT_LOG);
    int obj_old_bit_size =
        int((old_size + MEMORY_ALLOC_QUANT - 1) >> MEMORY_ALLOC_QUANT_LOG);

    return (obj_new_bit_size > obj_old_bit_size)
        ? alloc(size, pos, aof_none) : True;
}

void bitmap_file_memory_manager::confirm_alloc(size_t size, fposi_t pos)
{
    int obj_bit_size = int((size + MEMORY_ALLOC_QUANT-1) >> MEMORY_ALLOC_QUANT_LOG);

    fposi_t quant_no = pos >> MEMORY_ALLOC_QUANT_LOG;
    nat1* p = area_beg + size_t(quant_no >> 3);
    int bit_offs = (int)quant_no & 7;

    if (obj_bit_size > 8 - bit_offs) {
        obj_bit_size -= 8 - bit_offs;
        *p |=  ~((1 << bit_offs) - 1);
        while ((obj_bit_size -= 8) > 0) {
            *++p = 0xFF;
        }
        p[1] |= (1 << (obj_bit_size + 8)) - 1;
    } else {
        *p |= ((1 << obj_bit_size) - 1) << bit_offs;
    }
}

int bitmap_file_memory_manager::check_location(fposi_t pos, size_t size)
{
    int obj_bit_size = int((size + MEMORY_ALLOC_QUANT-1) >> MEMORY_ALLOC_QUANT_LOG);

    fposi_t quant_no = pos >> MEMORY_ALLOC_QUANT_LOG;
    nat1* p = area_beg + size_t(quant_no >> 3);
    int bit_offs = (int)quant_no & 7;
    nat1 mask, res;

    if (obj_bit_size > 8 - bit_offs) {
        obj_bit_size -= 8 - bit_offs;
        mask = ~((1 << bit_offs) - 1);
        res = *p & mask;
        if (res != 0) {
            if (res != mask) return -1;
            while ((obj_bit_size -= 8) > 0) {
                if (*++p != 0xFF) return -1;
            }
            mask = (1 << (obj_bit_size + 8)) - 1;
            return (p[1] & mask) == mask ? 1 : -1;
        } else {
            while ((obj_bit_size -= 8) > 0) {
                // fixed memory access violation bug on database restore
                if (++p >= area_end || *p != 0) return -1;
            }
            return (p[1] & ((1 << (obj_bit_size + 8)) - 1)) ? -1 : 0;
        }
    } else {
        mask = ((1 << obj_bit_size) - 1) << bit_offs;
        res = *p & mask;
        return (res == 0) ? 0 : (res == mask) ? 1 : -1;
    }
}

void bitmap_file_memory_manager::initialize() {}

void bitmap_file_memory_manager::dump(char*)
{
    register nat1* cur = area_beg;
    register nat1* end = area_end;
    int hole_bit_size = 0;
    int n_holes = 0;
    long all_holes_size = 0;
    long max_hole_size = 0;
    int n_holes_of_size[64];
    memset(n_holes_of_size, 0, sizeof(n_holes_of_size));
    while (end > cur && *--end == 0);
    while (cur <= end) {
        int byte = *cur++;
        if (byte == 0) { 
            hole_bit_size += 8;
        } else { 
            int i = 8;
            do { 
                while ((byte & 1) == 0) { 
                    byte >>= 1;
                    hole_bit_size += 1;
                    i -= 1;
                }
                if (hole_bit_size != 0) { 
                    if (hole_bit_size > max_hole_size) { 
                        max_hole_size = hole_bit_size;
                    }
                    all_holes_size += hole_bit_size;
                    int log2 = 0;
                    while ((hole_bit_size >>= 1) != 0) { 
                        log2 += 1;
                    }
                    n_holes_of_size[log2] += 1;
                    n_holes += 1;
                }
                do { 
                    i -= 1;
                } while (((byte >>= 1) & 1) != 0);
            } while (byte != 0);
            hole_bit_size = i;
        }
    }
    if (hole_bit_size != 0) { 
        if (hole_bit_size > max_hole_size) { 
            max_hole_size = hole_bit_size;
        }
        all_holes_size += hole_bit_size;
        int log2 = 0;
        while ((hole_bit_size >>= 1) != 0) { 
            log2 += 1;
        }
        n_holes_of_size[log2] += 1;
        n_holes += 1;
    }


    console::output("Memory map size: %ld, allocated memory %" INT8_FORMAT "u, used memory %" INT8_FORMAT 
                    "u, max hole size %" INT8_FORMAT "u, average hole size %" INT8_FORMAT "u, number of holes %u\n"
                    "Clustered objects: %ld, cluster faults: %ld, total clustered: %ld, total aligned objects %ld, total aligned pages %ld\n", 
                    size_t(area_end - area_beg),
                    fsize_t(size_t(end - area_beg + 1))*8*MEMORY_ALLOC_QUANT, 
                    fsize_t(size_t(end - area_beg + 1))*8*MEMORY_ALLOC_QUANT - fsize_t(all_holes_size)*MEMORY_ALLOC_QUANT,
                    fsize_t(max_hole_size)*MEMORY_ALLOC_QUANT,
                    n_holes != 0 ? fsize_t(all_holes_size)*MEMORY_ALLOC_QUANT / n_holes : 0, n_holes, 
                    n_clustered, n_cluster_faults, total_clustered, total_aligned, total_aligned_pages);
    fsize_t size = MEMORY_ALLOC_QUANT;
    for (int i = 0; i < 64; i++) { 
        if (n_holes_of_size[i] != 0) { 
            console::output("Number of holes of size [%" INT8_FORMAT "u, %" INT8_FORMAT "u] = %d\n", 
                            size, (size << 1) - 1, n_holes_of_size[i]);
        }
        size <<= 1;
    }
}

static boolean do_file_backup(char* base, size_t size, file& backup_file, const char* what)
{
    msg_buf buf;
    file::iop_status status;
    nat8 pksize = size;
    pack8(pksize);
    status = backup_file.write(&pksize, sizeof pksize);
    if (status == file::ok) {
        status = backup_file.write(base, size);
    }
    if (status != file::ok) {
        backup_file.get_error_text(status, buf, sizeof buf);
        console::message(msg_error|msg_time,
                         "Failed to copy %s information to backup file: %s\n",
                         what, buf);
        return False;
    }
    return True;
}


static boolean do_file_restore(mmap_file& dbs_file, file& backup_file, const char* what)
{
    msg_buf buf;
    file::iop_status status;
    nat8 size;
#if OLD_BACKUP_FORMAT_COMPATIBILITY
    nat4 size_low, size_high;
    status = backup_file.read(&size_high, sizeof(size_high));    
    if (status != file::ok) {
        backup_file.get_error_text(status, buf, sizeof buf);
        console::message(msg_error,
                         "Failed to read %s information from backup file:"
                         " %s\n", what, buf);
        return False;
    }
    unpack4(size_high);
    if (size_high < MIN_FILE_SIZE) { // 8-byte timestamp
        status = backup_file.read(&size_low, sizeof(size_low));    
        if (status != file::ok) {
            backup_file.get_error_text(status, buf, sizeof buf);
            console::message(msg_error,
                             "Failed to read %s information from backup file:"
                             " %s\n", what, buf);
            return False;
        }
        unpack4(size_low);
        size = ((nat8)size_high << 32) | size_low;
    } else { 
        size = size_high;
    }
#else
    status = backup_file.read(&size, sizeof(size));    
    if (status != file::ok) {
        backup_file.get_error_text(status, buf, sizeof buf);
        console::message(msg_error,
                         "Failed to read %s information from backup file:"
                         " %s\n", what, buf);
        return False;
    }
    unpack8(size);
#endif
    status = dbs_file.open(file::fa_readwrite,
                           file::fo_create|file::fo_truncate);
    if (status != file::ok) {
        dbs_file.get_error_text(status, buf, sizeof buf);
        console::message(msg_error,
                         "Failed to open %s file: %s\n", what, buf);
        return False;
    }
    if (dbs_file.set_size(size) != file::ok) {
        dbs_file.get_error_text(status, buf, sizeof buf);
        console::message(msg_error,
                         "Failed to expand %s file: %s\n", what, buf);
        dbs_file.close();
        return False;
    }

    status = backup_file.read(dbs_file.get_mmap_addr(), size);
    if (status != file::ok) {
        backup_file.get_error_text(status, buf, sizeof buf);
        console::message(msg_error,
                         "Failed to read %s information from backup file:"
                         " %s\n", what, buf);
        dbs_file.close();
        return False;
    }
    dbs_file.close();
    return True;
}


boolean bitmap_file_memory_manager::backup(file& backup_file)
{
    extend_cs.enter();
    boolean result = opened && do_file_backup((char*)area_beg,
                                              area_end-area_beg,
                                              backup_file, "memory mapping");
    extend_cs.leave();
    return result;
}


boolean bitmap_file_memory_manager::restore(file& backup_file)
{
    return do_file_restore(*map, backup_file, "memory mapping");
}

fsize_t bitmap_file_memory_manager::get_storage_size()
{
    register nat4* p = (nat4*)area_end; // area_end is aligned to 8
    register nat4* begin = (nat4*)area_beg;
    while (p != begin && *--p == 0);
    fsize_t size =
        fsize_t((char*)(p+1) - (char*)begin) << (3+MEMORY_ALLOC_QUANT_LOG);
    return size;
}


size_t bitmap_file_memory_manager::get_allocation_quantum()
{
    return MEMORY_ALLOC_QUANT;
}

void bitmap_file_memory_manager::stop_backup() {}

//
// Grabage collector grey references set implementation
//

#define GC_HASH_SIZE 1024u

inline opid_t grey_references_set::get()
{
    if (n_grey_refs == 0) {
        return 0;
    }
    refs_delta -= 1;
    n_grey_refs -= 1;
    if (!queue.is_empty()) {
        return queue.get();
    }
    int i = curr_pos;
    while (True) {
        gc_page **pp, *p;
        for (pp = &page_hash[i]; (p = *pp) != NULL; pp = &p->next) {
            if (p->page_no >= base_page) {
                if (p->page_no >= base_page + GC_HASH_SIZE) {
                    if (rewind || p->page_no < next_base) {
                        rewind = False;
                        next_base = p->page_no;
                    }
                    break;
                }
                grey_reference* ref = p->refs;
                opid_t opid = ref->opid;
                if ((p->refs = ref->next) == NULL) {
                    *pp = p->next;
                    page_pool.dealloc(p);
                }
                refs_pool.dealloc(ref);
                curr_pos = i;
                return opid;
            }
        }
        if (++i == GC_HASH_SIZE) {
            i = 0;
            if (rewind) {
                base_page = 0;
            } else {
                base_page = next_base & ~(GC_HASH_SIZE-1);
                rewind = True;
            }
        }
    }
}

inline void grey_references_set::put(opid_t opid, fposi_t page_no)
{
    n_grey_refs += 1;
    if (++refs_delta > max_set_extension) {
        queue.put(opid);
    } else {
        gc_page *p, **pp = &page_hash[page_no % GC_HASH_SIZE];
        while ((p = *pp) != NULL && p->page_no < page_no) {
            pp = &p->next;
        }
        if (p == NULL || p->page_no != page_no) {
            gc_page* pg = (gc_page*)page_pool.alloc();
            pg->next = p;
            *pp = pg;
            pg->refs = NULL;
            pg->page_no = page_no;
            p = pg;
        }
        grey_reference* ref = (grey_reference*)refs_pool.alloc();
        ref->opid = opid;
        ref->next = p->refs;
        p->refs = ref;
        if (page_no >= base_page && (rewind || page_no < next_base)) {
            rewind = False;
            next_base = page_no;
        }
    }
}

inline void grey_references_set::reset()
{
    queue.make_empty();
    gc_page *pg, *next_page;
    grey_reference *ref, *next_ref;
    for (unsigned i = 0; i < GC_HASH_SIZE; i++) {
        for (pg = page_hash[i]; pg != NULL; pg = next_page) {
            next_page = pg->next;
            for (ref = pg->refs; ref != NULL; ref = next_ref) {
                next_ref = ref->next;
                refs_pool.dealloc(ref);
            }
            page_pool.dealloc(pg);
        }
        page_hash[i] = NULL;
    }
    refs_delta = max_set_extension > 0 ? INT_MIN : 0;
    n_grey_refs = 0;
    curr_pos = 0;
    base_page = 0;
    rewind = True;
}

inline void grey_references_set::put_root()
{
    queue.put(ROOT_OPID);
    n_grey_refs += 1;
    if (max_set_extension > 0) {
        refs_delta = 1;
    } else {
        refs_delta += 1;
    }
}

inline grey_references_set::grey_references_set(size_t max_extension)
: refs_pool(sizeof(grey_reference)),
  page_pool(sizeof(gc_page))
{
    max_set_extension = nat4(max_extension);
    page_hash = new gc_page*[GC_HASH_SIZE];
    memset(page_hash, 0, GC_HASH_SIZE*sizeof(gc_page*));
}


//
// Memory manager
//
//[MC] We disabled the garbage collector. There is a bug.  
const static bool gc_disabled = true;

void gc_memory_manager::gc_initiate()
{
	//[MC]
	if(gc_disabled)
		return;

    gc_allocated = 0;
    if (!shutdown_flag && !(gc_state & gcs_active)) {
        if (server->id == GC_COORDINATOR) {
            gc_cs.enter();
            if (!(gc_state & gcs_prepare)) {
                gc_state = gcs_prepare;
                gc_term_event.reset();
		if (gc_background) { 
		    task::create(start_garbage_collection, this,
				 task::pri_background);
		} else {
		    gc_mark();
		}
	    }
            gc_cs.leave();
        } else {
            dbs_request req;
            req.gc_sync.cmd = dbs_request::cmd_gcsync;
            req.gc_sync.fun = gcc_init;
            server->send(GC_COORDINATOR, &req);
        }
    }
}


void gc_memory_manager::disconnect_client(client_process* proc)
{
    if (proc->firstReservedOpid != 0) {
        cs.enter();
        dbs_handle* hp = index_beg;
        hp[proc->lastReservedOpid].set_next(hp->get_next());
        hp->set_next(proc->firstReservedOpid);
        cs.leave();
    }
}

void gc_memory_manager::mark_reserved_objects(client_process* proc)
{
    //
    // Call only from sweep method with mutex locked, so no extra locking is needed
    //
    opid_t opid = proc->firstReservedOpid;
    if (opid != 0) {
        dbs_handle* hp = index_beg;
        opid_t last = proc->lastReservedOpid;
        while (True) {
            GC_MARK(opid);
            if (opid == last) {
                break;
            }
            opid = hp[opid].get_next();
        }
    }
}

boolean gc_memory_manager::try_alloc(fposi_t& pos, size_t size, int flags, client_process* proc)
{
    while (!fmm->alloc(size, pos, flags)) {
        if (!shutdown_flag) {
            if (!gc_wait_flag) {
                server->message(msg_error|msg_time, 
                                "Limitation for data file size is exhausted\n");
                gc_wait_event.reset();
            }
	    gc_wait_flag += 1;
            gc_initiate();
            time_t t = time(NULL);
            proc->suspended = True;
            gc_wait_event.wait();
	    gc_wait_flag -= 1;
            proc->suspended = False;
            t = time(NULL) - t;
            if (t > max_gc_delay) {
                max_gc_delay = t;
            }
            if (!shutdown_flag) {
                continue;
            }
        }
        return False;
    }
    return True;
}

void gc_memory_manager::bulk_alloc(nat4* sizeBuf, nat2* cpidBuf, size_t nObjects,
                                   nat4* opidBuf, size_t nReserved, nat4* nearBuf, int flags, client_process* proc)
{
    size_t i;
    fposi_t pos;
    opid_t opid = proc->firstReservedOpid;
    dbs_handle* hp = index_beg;

    cs.enter();
    n_requests += 1;
    if (flags & aof_clustered) { 
        for (i = 0; i < nObjects; i++) {
            opid_t next = hp[opid].get_next();
            size_t size = sizeBuf[i];
            flags = aof_none;
            if (size & ALLOC_ALIGNED) {
                size &= ~ALLOC_ALIGNED;
                flags |= aof_aligned;
            }
            opid_t cluster_opid = nearBuf[i];
            if (cluster_opid != 0) { 
                pos = hp[cluster_opid].get_pos();
                flags |= aof_clustered;
            }
            if (!try_alloc(pos, size, flags, proc)) { 
                cs.leave();
                return;
            }
            hp[opid].set(cpidBuf[i], pos, nat4(size));
            gc_allocated += size;
            total_used_space += size;
            if (opid == proc->lastReservedOpid) {
                assert(i == nObjects-1);
                proc->lastReservedOpid = 0;
                opid = 0;
            } else {
                opid = next;
            }
        }                
    } else { 
        size_t page_size = size_t(1) << page_bits;
        size_t total_size = 0;
        size_t unalignedOffs = 0;
        for (i = 0; i < nObjects; i++) {
            size_t alignment = MEMORY_ALLOC_QUANT;
            size_t size = sizeBuf[i];
            if (size & ALLOC_ALIGNED) { 
                alignment = page_size;
                size &= ~ALLOC_ALIGNED;
                unalignedOffs += DOALIGN(size, alignment);
            }
            total_size += DOALIGN(size, alignment);
        }
        if (!try_alloc(pos, total_size, flags, proc)) { 
            cs.leave();
            return;
        }
        size_t alignedOffs = 0;
        for (i = 0; i < nObjects; i++) {
            opid_t next = hp[opid].get_next();
            size_t size = sizeBuf[i];
            if (size & ALLOC_ALIGNED) {
                size &= ~ALLOC_ALIGNED;
                hp[opid].set(cpidBuf[i], pos + alignedOffs, nat4(size));
                size = DOALIGN(size, page_size);
                gc_allocated += size;
                total_used_space += size;
                alignedOffs += size;
            } else {
                hp[opid].set(cpidBuf[i], pos + unalignedOffs, nat4(size));
                size = DOALIGN(size, MEMORY_ALLOC_QUANT);
                gc_allocated += size;
                total_used_space += size;
                unalignedOffs += size;
            }
            if (opid == proc->lastReservedOpid) {
                assert(i == nObjects-1);
                proc->lastReservedOpid = 0;
                opid = 0;
            } else {
                opid = next;
            }
        }
    }
    proc->firstReservedOpid = opid;
    if (nReserved > 0) {
      try_again:
        hp = index_beg;
        opid = hp->get_next();
        opid_t first_opid = opid;
        opid_t last_opid = index_end - index_beg;
        for (i = 0; opid > ROOT_OPID && opid < last_opid && hp[opid].is_free() && i < nReserved; i++) {
            opidBuf[i] = opid;
            opid = hp[opid].get_next();
        }
        if (i == nReserved) {
            hp->set_next(opid);
        } else {
            //
            // List of free handles is not up-to-date:
            // get handles from the end of area.
            //
            if (opid != 0) {
                server->message(msg_error|msg_time,
                                 "Allocation list is not up-to-date: opid=%x, "
                                 "max_opid=%x, last_opid=%x\nNot to worry; "
                                 "I'll re-build it.\n",
                                 opid, max_opid, last_opid);
                rebuild_free_list = True;
                hp->set_next(0);
            }
            opid = max_opid+1 < last_opid && hp[max_opid+1].is_free()
                ? max_opid+1 : last_opid;
            //
            // Now 'opid' garantly not belongs to the GC sweep window and
            // points to free cell.
            //
            if (opid + nReserved - 1 > max_opid) {
                if (n_objects_limit != 0 && opid >= n_objects_limit) {
                    if (!shutdown_flag) {
                        if (!gc_wait_flag) {
                            server->message(msg_error|msg_time, 
                                             "Limitation for number of objects in "
                                             "storage (%u) is exhausted\n",
                                             n_objects_limit);
			    gc_wait_event.reset();
                        }
 			gc_wait_flag += 1;
                        gc_initiate();
                        time_t t = time(NULL);
                        proc->suspended = True;
			gc_wait_event.wait();
			gc_wait_flag -= 1;
                        proc->suspended = False;
                        t = time(NULL) - t;
                        if (t > max_gc_delay) {
                            max_gc_delay = t;
                        }
                        if (!shutdown_flag) goto try_again;
                    }
                    cs.leave();
                    return;
                }
                max_opid = opid_t(opid + nReserved - 1);
            }
            if (opid + nReserved > last_opid) {
                extend_cs.enter();
                file::iop_status status = index->set_size((opid + nReserved)*sizeof(dbs_handle), 
                                                          index->get_mmap_size()*2);
                if (status != file::ok) {
                    msg_buf buf;
                    index->get_error_text(status, buf, sizeof buf);
                    console::error("Failed to extend index: %s\n", buf);
                }
                index_beg = hp = (dbs_handle*)index->get_mmap_addr();
                index_end = (dbs_handle*)index->get_mmap_addr() + index->get_mmap_size()/sizeof(dbs_handle);
                extend_cs.leave();
            }
            first_opid = opid;
            for (i = 0; i < nReserved; i++, opid++) {
                opidBuf[i] = opid;
                hp[opid].set_next(opid+1);
            }
        }
        for (i = 0; i < nReserved; i++) {
            opid = opidBuf[i];
            if (gc_state & gcs_sweep) {
                gc_mark_object(opid);
            }
            server->obj_mgr->new_object(opid, proc);
        }
        if (proc->lastReservedOpid == 0) {
            proc->lastReservedOpid = opidBuf[nReserved-1];
        }
        hp[opidBuf[nReserved-1]].set_next(proc->firstReservedOpid);
        proc->firstReservedOpid = first_opid;
    }
    if ((gc_watermark != 0 && gc_allocated >= gc_watermark)
	|| (gc_used_watermark != 0 && total_used_space >= gc_used_watermark)) 
    {
        gc_initiate();
    }
    cs.leave();
}


opid_t gc_memory_manager::alloc(cpid_t cpid, size_t size, int flags, opid_t cluster_with, 
                                client_process* proc)
{
    assert(cpid >= MIN_CPID);
    cs.enter();
    n_requests += 1;
  try_again:
    dbs_handle* hp = index_beg;
    opid_t opid = hp->get_next();
    opid_t last_opid = index_end - index_beg;
    if (opid <= ROOT_OPID || opid >= last_opid || !hp[opid].is_free()) {
        //
        // List of free handles is not up-to-date:
        // get handles from the end of area.
        //
        if (opid != 0) {
            server->message(msg_error|msg_time,
                             "Allocation list is not up-to-date: opid=%x, "
                             "max_opid=%x, last_opid=%x\nNot to worry; I'll "
                             "re-build it.\n",
                             opid, max_opid, last_opid);
            rebuild_free_list = True;
            hp->set_next(0);
        }
        opid = max_opid+1 < last_opid && hp[max_opid+1].is_free()
            ? max_opid+1 : last_opid;
        //
        // Now 'opid' garantly not belongs to the GC sweep window and
        // points to free cell.
        //
        if (opid > max_opid) {
            if (n_objects_limit != 0 && opid >= n_objects_limit) {
                if (!shutdown_flag) {
                    if (!gc_wait_flag) {
                        server->message(msg_error|msg_time, 
                                         "Limitation for number of objects in "
                                         "storage (%u) is exhausted\n",
                                         n_objects_limit);
			gc_wait_event.reset();
                    }
		    gc_wait_flag += 1;
                    gc_initiate();
                    time_t t = time(NULL);
                    proc->suspended = True;
                    gc_wait_event.wait();
		    gc_wait_flag -= 1;
                    proc->suspended = False;
                    t = time(NULL) - t;
                    if (t > max_gc_delay) {
                        max_gc_delay = t;
                    }
                    if (!shutdown_flag) goto try_again;
                }
                cs.leave();
                return 0;
            }
        }
    } else {
        hp->set_next(hp[opid].get_next());
    }
    if (opid > max_opid) {
        max_opid = opid;
    }

    if (opid == last_opid) {
        extend_cs.enter();
        file::iop_status status = index->set_size((opid+1)*sizeof(dbs_handle),
                                                  index->get_mmap_size()*2);
        if (status != file::ok) {
            msg_buf buf;
            index->get_error_text(status, buf, sizeof buf);
            console::error("Failed to extend index: %s\n", buf);
        }
        index_beg = hp = (dbs_handle*)index->get_mmap_addr();
        index_end = (dbs_handle*)index->get_mmap_addr() + index->get_mmap_size()/sizeof(dbs_handle);
        extend_cs.leave();
    }

    fposi_t pos = size == 0 ? INVALID_OBJECT_POSITION : EXTERNAL_BLOB_POSITION;
    if (size != 0 && !server->class_mgr->is_external_blob(cpid)) {
        if (flags & aof_clustered) { 
            if (cluster_with != 0) { 
                internal_assert(cluster_with < last_opid);
                pos = hp[cluster_with].get_pos();
            } else { 
                flags &= ~aof_clustered;
            }
        }
        if (!fmm->alloc(size, pos, flags)) {
            if (!shutdown_flag) {
                if (!gc_wait_flag) {
                    server->message(msg_error|msg_time, 
                                    "Limitation for data file size is exhausted\n");
                    gc_wait_event.reset();
                }
                //
                // Return allocated descriptor back to free list
                //
                hp[opid].set_next(hp->get_next());
                hp->set_next(opid);
                
                gc_wait_flag += 1;
                gc_initiate();
                time_t t = time(NULL);
                proc->suspended = True;
                gc_wait_event.wait();
                gc_wait_flag -= 1;
                proc->suspended = False;
                t = time(NULL) - t;
                if (t > max_gc_delay) {
                    max_gc_delay = t;
                }
                if (!shutdown_flag) goto try_again;
            }
            cs.leave();
            return 0;
        }
    }
    hp[opid].set(cpid, pos, nat4(size));
            
    if (gc_state & gcs_sweep) {
        gc_mark_object(opid);
    }
    server->obj_mgr->new_object(opid, proc);
    
    total_used_space += size;
    
    gc_allocated += size;
    if ((gc_watermark != 0 && gc_allocated >= gc_watermark)
        || (gc_used_watermark != 0 && total_used_space >= gc_used_watermark)) 
    {
        gc_initiate();
    }
    cs.leave();
    return opid;
}

fposi_t gc_memory_manager::get_pos(opid_t opid)
{
    cs.enter();
    n_requests += 1;
    dbs_handle* hp = &index_beg[opid];
    internal_assert(hp < index_end);
    fposi_t pos = hp->get_pos();
    cs.leave();
    return pos;
}

size_t gc_memory_manager::get_size(opid_t opid)
{
    cs.enter();
    n_requests += 1;
    dbs_handle* hp = &index_beg[opid];
    internal_assert(hp < index_end);
    size_t size = hp->get_size();
    cs.leave();
    return size;
}

cpid_t gc_memory_manager::get_cpid(opid_t opid)
{
    cs.enter();
    n_requests += 1;
    dbs_handle* hp = &index_beg[opid];
    internal_assert(hp < index_end);
    cpid_t cpid = hp->get_cpid();
    cs.leave();
    return cpid;
}

void gc_memory_manager::get_handle(opid_t opid, dbs_handle& hnd)
{
    cs.enter();
    n_requests += 1;
    dbs_handle* hp = &index_beg[opid];
    if (hp >= index_end) {
        hnd.mark_as_free();
    } else {
        hnd = *hp;
    }
    cs.leave();
}

void gc_memory_manager::dealloc(opid_t opid)
{
    cs.enter();
    dbs_handle* hp = &index_beg[opid];
    size_t size = hp->get_size();
    cpid_t cpid = hp->get_cpid();
    if (server->class_mgr->is_external_blob(cpid)) {
        server->remove_external_blob(opid);
    } else if (size != 0) {
        fmm->dealloc(hp->get_pos(), size);
    }
    hp->mark_as_free();
    if (opid > ROOT_OPID && !rebuild_free_list) {
        hp->set_next(index_beg->get_next());
        index_beg->set_next(opid);
    }
    cs.leave();
}

boolean gc_memory_manager::do_realloc(fposi_t& pos,
                                      size_t   old_size,
                                      size_t   new_size,
                                      client_process* proc)
{
    cs.enter();
    boolean proceeding = True;
    while (proceeding && !fmm->do_realloc(new_size, pos, old_size)){
        if (shutdown_flag) {
            cs.leave();
            return False;
        }
        if (!gc_wait_flag) {
            server->message(msg_error|msg_time, 
                            "Limitation for data file size is exhausted\n");
	    gc_wait_event.reset();
        }
	gc_wait_flag += 1;
        gc_initiate();
        time_t t = time(NULL);
        if (proc != NULL) {
            proc->suspended = True;
        }
        gc_wait_event.wait();
	gc_wait_flag -= 1; 
	if (proc != NULL) {
            proc->suspended = False;
        }
        t = time(NULL) - t;
        if (t > max_gc_delay) {
            max_gc_delay = t;
        }
        proceeding = !shutdown_flag;
    }
    if (new_size > old_size) { 
        total_used_space += new_size - old_size;
    }
    cs.leave();
    return proceeding;
}


void gc_memory_manager::undo_realloc(opid_t opid,
                                     size_t new_size, fposi_t new_pos)
{
    cs.enter();
    if (opid == 0) {
        fmm->dealloc(new_pos, new_size);
    } else {
        dbs_handle* hp = &index_beg[opid];
        internal_assert(hp < index_end);
        size_t old_size = hp->get_size();
        fposi_t old_pos = hp->get_pos();
        if (new_size > old_size && new_pos != old_pos) {
            fmm->dealloc(new_pos, new_size);
        }
    }
    cs.leave();
}

void gc_memory_manager::update_handle(opid_t opid, cpid_t new_cpid,
                                      size_t new_size, fposi_t new_pos)
{
    cs.enter();
    n_requests += 1;
    dbs_handle* hp = &index_beg[opid];
    internal_assert(hp < index_end);
    size_t old_size = hp->get_size();
    fposi_t old_pos = hp->get_pos();
    cpid_t old_cpid = hp->get_cpid();

    if (old_cpid != new_cpid || old_size != new_size || old_pos != new_pos) {
        internal_assert(new_cpid == opid || old_cpid != 0);
        if (old_size != 0) {
            if (old_pos != new_pos) {
                fmm->dealloc(old_pos, old_size);
            } else if (new_size < old_size) {
                fmm->dealloc(new_pos + new_size, old_size - new_size);
            }
        }
        if (new_cpid > max_cpid) {
            max_cpid = new_cpid;
        }
        hp->set(new_cpid, new_pos, nat4(new_size));
    }
    cs.leave();
}

//
// Following two methods are called only while recovery so
// intertask synchronization is not necessary
//
void gc_memory_manager::confirm_alloc(opid_t opid, cpid_t cpid, size_t size,
                                      fposi_t pos)
{
    dbs_handle* hp = &index_beg[opid];
    n_requests += 1;
    if (hp >= index_end) {
        file::iop_status status = index->set_size((opid+1)*sizeof(dbs_handle), index->get_mmap_size()*2);
        if (status != file::ok) {
            msg_buf buf;
            index->get_error_text(status, buf, sizeof buf);
            console::error("Failed to extand index: %s\n", buf);
        }
        index_beg = (dbs_handle*)index->get_mmap_addr();
        index_end = (dbs_handle*)index->get_mmap_addr() + index->get_mmap_size()/sizeof(dbs_handle);
        hp = &index_beg[opid];
    }
    if (size != 0 && (opid == cpid || pos != EXTERNAL_BLOB_POSITION)) {
        fmm->confirm_alloc(size, pos);
    }
    if (opid > max_opid) {
        max_opid = opid;
    }
    if (cpid > max_cpid) {
        max_cpid = cpid;
    }
    hp->set(cpid, pos + dbs_handle::recovered_object_flag, nat4(size));
}

void gc_memory_manager::revoke_object(opid_t old_opid, fposi_t old_pos, size_t old_size, 
                                      opid_t new_opid, fposi_t new_pos, size_t new_size)
{
    dbs_handle* hp = index_beg;
    SERVER_TRACE_MSG((msg_important|msg_object|msg_warning, "Revoke object %x, position %" INT8_FORMAT "u, index position %" INT8_FORMAT "u, size %lu with object %x, position %" INT8_FORMAT "u, size %lu\n",
                      old_opid, old_pos, hp[old_opid].get_pos(), (unsigned long)old_size,
                      new_opid, new_pos, (unsigned long)new_size));
    if (old_opid != new_opid && hp[old_opid].get_pos() == old_pos) {
        internal_assert(hp[old_opid].get_size() == old_size);
        hp[old_opid].mark_as_free();
        hp[old_opid].set_next(hp->get_next());
        hp->set_next(old_opid);
    }
    if (old_pos != new_pos || old_size > new_size) { 
        fmm->dealloc(old_pos, old_size);
    }
}

void gc_memory_manager::check_reference_consistency()
{
    opid_t last_opid = index_end - index_beg;
    assert(max_opid < last_opid);
    dbs_handle* hp = index_beg;
    for (opid_t opid = ROOT_OPID; opid <= max_opid; opid += 1) {
        if (!hp[opid].is_free()) {
            size_t size = hp[opid].get_size();
            cpid_t cpid = hp[opid].get_cpid();
            fposi_t pos = hp[opid].get_pos();
            assert(size == 0 || fmm->check_location(pos, size) == 1);
            int n_refs = server->class_mgr->get_number_of_references(cpid,
                                                                     size);
            assert(n_refs >= 0);
            if (n_refs != 0) {
                char* p = gc_buf.put(n_refs*sizeof(dbs_reference_t));
                server->pool_mgr->read(pos, p, gc_buf.size());
                while (--n_refs >= 0) {
					objref_t ropid;
                    stid_t rsid;
                    p = unpackref(rsid, ropid, p);
                    if ((rsid & TAGGED_POINTER) == 0) {
                        assert(rsid < n_servers
                               && (ropid <= max_opid
                                   || (ropid < last_opid && hp[ropid].is_free())));
                    }
                }
            }
        }
    }
}

gc_memory_manager::gc_memory_manager(mmap_file& index_file,
                                     mmap_file& map_file,
				     boolean gc_background, 
                                     time_t gc_init_timeout,
                                     size_t gc_init_alloc_size,
                                     fsize_t gc_init_used_size,
                                     time_t gc_init_idle_period,
                                     size_t gc_init_min_allocated,
                                     size_t gc_grey_set_threshold,
                                     fsize_t max_file_size,
                                     size_t max_objects,
                                     time_t gc_response_timeout,
                                     fsize_t extension_quantum,
                                     size_t blob_size,
                                     fsize_t blob_offset)
: gc_wait_event(cs), gc_sem(gc_cs), gc_grey_set(gc_grey_set_threshold)
{
    fmm = new bitmap_file_memory_manager(map_file, max_file_size, extension_quantum, blob_size, blob_offset);
    fmm_created = True;
    index = &index_file;
    server = NULL;
    gc_watermark = gc_init_alloc_size;
    gc_used_watermark = gc_init_used_size;
    this->gc_background = gc_background;
    this->gc_init_timeout = gc_init_timeout;
    this->gc_response_timeout = gc_response_timeout;
    this->gc_init_idle_period = gc_init_idle_period;
    this->gc_init_min_allocated = gc_init_min_allocated;
    n_objects_limit = max_objects;
    gc_wait_flag = 0;
    opened = False;
    n_profiled_classes = 0;
    profile_instances = NULL;
    profile_size = NULL;
}

gc_memory_manager::gc_memory_manager(mmap_file& index_file,
                                     file_memory_manager& fmm,
				     boolean gc_background, 
                                     time_t gc_init_timeout,
                                     size_t gc_init_alloc_size,
                                     fsize_t gc_init_used_size,
                                     time_t gc_init_idle_period,
                                     size_t gc_init_min_allocated,
                                     size_t gc_grey_set_threshold,
                                     size_t max_objects,
                                     time_t gc_response_timeout)
: gc_wait_event(cs), gc_sem(gc_cs), gc_grey_set(gc_grey_set_threshold)
{
    this->fmm = &fmm;
    fmm_created = False;
    index = &index_file;
    server = NULL;
    gc_watermark = gc_init_alloc_size;
    gc_used_watermark = gc_init_used_size;
    this->gc_background = gc_background;
    this->gc_init_timeout = gc_init_timeout;
    this->gc_response_timeout = gc_response_timeout;
    this->gc_init_idle_period = gc_init_idle_period;
    this->gc_init_min_allocated = gc_init_min_allocated;
    n_objects_limit = max_objects;
    gc_wait_flag = 0;
    opened = False;
    n_profiled_classes = 0;
    profile_instances = NULL;
    profile_size = NULL;
}

void gc_memory_manager::set_file_size_limit(fsize_t max_file_size)
{
    fmm->set_file_size_limit(max_file_size);
}

void gc_memory_manager::set_extension_quantum(fsize_t quantum)
{
    fmm->set_extension_quantum(quantum);
}

void gc_memory_manager::set_blob_size(size_t blob_size)
{
    fmm->set_blob_size(blob_size);
}

void gc_memory_manager::set_blob_offset(size_t blob_offset)
{
    fmm->set_blob_offset(blob_offset);
}

void gc_memory_manager::set_objects_limit(size_t max_objects)
{
    n_objects_limit = max_objects;
}


#define DEFAULT_INDEX_SIZE (256*1024)

boolean gc_memory_manager::open(dbs_server* server)
{
    opened = False;

    if (fmm->open(server)) {
        msg_buf buf;
        file::iop_status status = index->open(file::fa_readwrite,
                                              file::fo_random|file::fo_create);
        if (status == file::ok) {
            fsize_t index_size;
            status = index->get_size(index_size);
            if (status != file::ok) {
                index->get_error_text(status, buf, sizeof buf);
                server->message(msg_error, 
                                 "Failed to get index size: %s\n", buf);
                fmm->close();
            } else {
                this->server = server;

                if (index_size <= (ROOT_OPID+1)*sizeof(dbs_handle)) {
                    index_size = DEFAULT_INDEX_SIZE*sizeof(dbs_handle);
                    status = index->set_size(index_size);
                    if (status != file::ok) {
                        index->get_error_text(status, buf, sizeof buf);
                        server->message(msg_error, 
                                         "Failed to extend index file: %s\n",
                                         buf);
                        fmm->close();
                        return False;
                    }
                }
                opened = True;
                index_beg = (dbs_handle*)index->get_mmap_addr();
                index_end = index_beg + size_t(index_size) / sizeof(dbs_handle);

                max_opid = index_beg->get_size();
                max_cpid = index_beg->get_cpid();

                n_used_objects = 0;
                total_used_space = index_beg[1].get_pos();
                max_gc_delay = 0;

                gc_max_time = 0;
                gc_total_time = 0;
                n_completed_gc = 0;

                rebuild_free_list = False;
                shutdown_flag = False;

                page_bits = server->pool_mgr->get_page_bits();
                internal_assert(page_bits >= MEMORY_ALLOC_QUANT_LOG + 3);

                gc_timestamp = gc_last_timestamp = 0;

                n_servers = server->get_number_of_servers();

                if (server->id == GC_COORDINATOR) {
                    gc_refs_matrix = new extern_references*[n_servers];
                    for (int i = 0; i < n_servers; i++) {
                        gc_refs_matrix[i] = new extern_references[n_servers];
                    }
                    gc_extern_references = gc_refs_matrix[GC_COORDINATOR];
                } else {
                    gc_extern_references = new extern_references[n_servers];
                }
                gc_export_buf = new export_buffer[n_servers];

                gc_map_size = GC_MAP_SIZE(index_end - index_beg);
                gc_map = new nat4[gc_map_size];

                gc_state = 0;
                gc_allocated = 0;
                n_requests = 0;
            }
        } else {
            index->get_error_text(status, buf, sizeof buf);
            server->message(msg_error,
                             "Failed to open index file: %s\n", buf);
            fmm->close();
        }
    }
    return opened;
}

void gc_memory_manager::close()
{
    cs.enter();
    if (opened) {
        extend_cs.enter(); // wait backup completion
        if (server->id == GC_COORDINATOR) {
            for (int i = n_servers; -- i >= 0; delete[] gc_refs_matrix[i]);
            delete[] gc_refs_matrix;
        } else {
            delete[] gc_extern_references;
        }
        delete[] gc_export_buf;
        delete[] gc_map;

        fmm->close();
        if (index_beg->get_size() != max_opid) {
            index_beg->set_size(max_opid); // avoid unnecessary modification
        }
        if (index_beg->get_cpid() != max_cpid) {
            index_beg->set_cpid(max_cpid); // avoid unnecessary modification
        }
        if (index_beg[1].get_pos() != total_used_space) {
            index_beg[1].set_pos(total_used_space); // avoid unnecessary modification
        }
        file::iop_status status = index->close();
        if (status != file::ok) {
            msg_buf buf;
            index->get_error_text(status, buf, sizeof buf);
            console::error("Failed to close index file: %s\n", buf);
        }
        server = NULL;
        opened = False;
        extend_cs.leave();
    }
    cs.leave();
}

void gc_memory_manager::flush()
{
    cs.enter();
    extend_cs.enter();
    if (index_beg->get_size() != max_opid) {
        index_beg->set_size(max_opid); // avoid unnecessary modification
    }
    if (index_beg->get_cpid() != max_cpid) {
        index_beg->set_cpid(max_cpid); // avoid unnecessary modification
    }
    if (index_beg[1].get_pos() != total_used_space) {
        index_beg[1].set_pos(total_used_space); // avoid unnecessary modification
    }
    cs.leave();
    file::iop_status status = index->flush();
    if (status != file::ok) {
        msg_buf buf;
        index->get_error_text(status, buf, sizeof buf);
        console::error("Failed to sync index to file: %s\n", buf);
    }
    extend_cs.leave();
    fmm->flush();
}


gc_memory_manager::~gc_memory_manager()
{
    if (fmm_created) {
        delete fmm;
    }
    delete[] profile_instances;
    delete[] profile_size;
}

void task_proc gc_memory_manager::start_garbage_collection(void* arg)
{
    ((gc_memory_manager*)arg)->gc_mark();
}

//
// This function is executed at coordinator to check if all
// servers had finished local mark stage. If for all  i, j
//   gc_refs_matrix[i][j].import == gc_refs_matrix[j][i].export
// then mark stage is globally finished.
//
inline boolean gc_memory_manager::gc_global_mark_finished()
{
    int n = n_servers;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (gc_refs_matrix[i][j].n_import != gc_refs_matrix[j][i].n_export)
            {
                return False;
            }
        }
    }
    gc_state = (gc_state & ~(gcs_abort|gcs_mark)) | gcs_sweep;
    return True;
}

inline boolean gc_memory_manager::gc_finish()
{
    int i, n = n_servers;
    gc_cs.enter();
    if (server->id == GC_COORDINATOR) {
        while(!(gc_state & (gcs_cont|gcs_abort)) && !gc_global_mark_finished())
        {
            gc_sem.wait();
        }
        if (gc_state & (gcs_cont|gcs_abort)) {
            gc_cs.leave();
            return False;
        }
        gc_cs.leave();
        dbs_request req;
        req.gc_sync.cmd = dbs_request::cmd_gcsync;
        req.gc_sync.fun = gcc_sweep;
        for (i = 0; i < n; i++) {
            if (i != server->id) {
                server->send(i, &req);
            }
        }
        return True;
    } else {
        while (!(gc_state & (gcs_cont|gcs_abort|gcs_sweep))) {
            dbs_request* reqp = (dbs_request*)gc_buf.put(sizeof(dbs_request)
                                + sizeof(extern_references)*n);
            reqp->gc_sync.cmd = dbs_request::cmd_gcsync;
            reqp->gc_sync.fun = gcc_finish;
            reqp->gc_sync.len = n*2;
            extern_references* rp = (extern_references*)(reqp+1);
            memcpy(rp, gc_extern_references, sizeof(extern_references)*n);
            for (i = 0; i < n; i++) {
                rp[i].pack();
            }
            gc_cs.leave();
            server->send(GC_COORDINATOR, reqp, sizeof(extern_references)*n);
            gc_cs.enter();
            if (!gc_sem.wait_with_timeout(gc_response_timeout)) {
                gc_state |= gcs_abort;
            }
        }
        gc_cs.leave();
        return (gc_state & gcs_sweep) != 0;
    }
}

void gc_memory_manager::gc_initialize()
{
    int i, j, n = n_servers;

    gc_cs.enter();
    if (server->id == GC_COORDINATOR) {
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                gc_refs_matrix[i][j].n_export = 0;
                gc_refs_matrix[i][j].n_import = 0;
            }
            //
            // Let import != export until row will be received from the server
            //
            gc_refs_matrix[i][i].n_export = 1;
        }
        gc_refs_matrix[GC_COORDINATOR][GC_COORDINATOR].n_export = 0;
    } else {
        for (i = 0; i < n; i++) {
            gc_extern_references[i].n_import = 0;
            gc_extern_references[i].n_export = 0;
        }
    }

    for (i = 0; i < n; i++) {
        gc_export_buf[i].curr = gc_export_buf[i].used = 0;
    }
    gc_grey_set.reset();
    memset(gc_map, 0, gc_map_size*sizeof(*gc_map));
    gc_cs.leave();
}


boolean gc_memory_manager::gc_start()
{
	//[MC]
	if(gc_disabled)
		return False;

    int i, n = n_servers;
    dbs_request req;
    req.gc_sync.cmd = dbs_request::cmd_gcsync;

    gc_cs.enter();
    if (n > 1) {
        req.gc_sync.fun = gcc_prepare;
        do {
            time_t current_time = time(NULL);
            if (current_time == gc_last_timestamp) {
                current_time += 1;
            }
            gc_last_timestamp = gc_timestamp = current_time;
            n_ready_to_gc_servers = 1; // I am ready
            gc_cs.leave();
            for (i = 0; i < n; i++) {
                if (i != GC_COORDINATOR) {
                    req.gc_sync.timestamp = current_time;
                    server->send(i, &req);
                }
            }
            gc_cs.enter();
            gc_sem.wait_with_timeout(gc_init_timeout);
        } while (!(gc_state & gcs_abort) && n_ready_to_gc_servers < n_servers);

        gc_timestamp = 0;
        SERVER_TRACE_MSG((msg_gc, "Prepare for GC: %d servers ready, state %x\n", 
                   n_ready_to_gc_servers, gc_state));
        if ((gc_state & gcs_abort) || n_ready_to_gc_servers < n_servers) {
            gc_state = 0;
            gc_cs.leave();
            req.gc_sync.fun = gcc_cancel;
            for (i = 0; i < n; i++) {
                if (i != GC_COORDINATOR) {
                    server->send(i, &req);
                }
            }
            return False;
        }
    }
    gc_state = (gc_state & ~gcs_prepare) | gcs_active | gcs_mark | gcs_cont;
    gc_cs.leave();
    server->obj_mgr->set_save_version_mode(True);
    req.gc_sync.fun = gcc_mark;
    for (i = 0; i < n; i++) {
        if (i != GC_COORDINATOR) {
            server->send(i, &req);
        }
    }
    return True;
}

inline void gc_memory_manager::gc_extend_map()
{
    cs.enter();
    size_t new_size = GC_MAP_SIZE(index_end - index_beg);
    internal_assert(new_size > gc_map_size);
    nat4* new_map = new nat4[new_size];
    memcpy(new_map, gc_map, gc_map_size*sizeof(*gc_map));
    memset(new_map+gc_map_size, 0, (new_size - gc_map_size)*sizeof(*gc_map));
    delete[] gc_map;
    gc_map = new_map;
    gc_map_size = new_size;
    cs.leave();
}

inline void gc_memory_manager::gc_note_reference(stid_t sid, opid_t opid)
{
    if (sid == server->id) {
        if (!WITHIN_GC_MAP(opid) || !GC_MARKED(opid)) {
            if (!WITHIN_GC_MAP(opid)) {
                gc_extend_map();
                internal_assert(WITHIN_GC_MAP(opid));
            }
            dbs_handle hnd;
            extend_cs.enter();
            internal_assert(index_beg + opid < index_end);
            hnd = index_beg[opid];
            extend_cs.leave();
            gc_grey_set.put(opid, hnd.get_pos() >> page_bits);

            GC_MARK(opid);
            gc_state |= gcs_cont;
        }
    } else if (sid & TAGGED_POINTER) {
        // do nothing
    } else {
        export_buffer* bp = &gc_export_buf[sid];
        opid_t* op = bp->refs;
        opid_t* end = op + bp->used;
        pack4(opid);
        while (op < end && *op != opid) {
            op += 1;
        }
        if (op == end) {
            unsigned i = bp->curr;
            gc_extern_references[sid].n_export += 1;
            if (i == itemsof(bp->refs)) {
                gc_cs.leave();
                bp->req.gc_sync.cmd = dbs_request::cmd_gcsync;
                bp->req.gc_sync.fun = gcc_refs;
                bp->req.gc_sync.n_refs = i;
                server->send(sid, &bp->req, i*sizeof(opid_t));
                bp->curr = 0;
                gc_cs.enter();
            }
            bp->refs[bp->curr++] = opid;
            if (bp->used < bp->curr) {
                bp->used = bp->curr;
            }
        }
    }
}

void gc_memory_manager::gc_follow_reference(stid_t sid, opid_t opid)
{
    gc_cs.enter();
    gc_note_reference(sid, opid);
    gc_cs.leave();
}

//
// Scan object references.
// This function is called with gc_cs locked
//
inline void gc_memory_manager::gc_scan_object(opid_t opid)
{
    if (!WITHIN_GC_MAP(opid)) {
        gc_extend_map();
        internal_assert(WITHIN_GC_MAP(opid));
    }
    gc_cs.leave();
    server->obj_mgr->scan_object(opid);

    dbs_handle hnd;
    extend_cs.enter();
    internal_assert(index_beg + opid < index_end);
    hnd = index_beg[opid];
    extend_cs.leave();

    cpid_t cpid = hnd.get_cpid();
    GC_MARK(cpid); // only GC thread can mark class objects

    size_t size = hnd.get_size();
    int n_refs = (opid != cpid && size >= sizeof(dbs_reference_t))
        ? server->class_mgr->get_number_of_references(cpid, size) : 0;
    internal_assert(n_refs >= 0);

    char* p = NULL;
    if (n_refs != 0) {
        p = gc_buf.put(n_refs*sizeof(dbs_reference_t));
        server->pool_mgr->read(hnd.get_pos(), p, gc_buf.size());
    }
    server->obj_mgr->release_object(opid);

    gc_cs.enter();
    while (--n_refs >= 0) {
		objref_t ropid;
        stid_t rsid;
        p = unpackref(rsid, ropid, p);
        if (ropid != 0) {
            gc_note_reference(rsid, ropid);
        }
    }
}

//
// This function is called at sweep phase with cs mutex locked
//
void gc_memory_manager::gc_mark_object(opid_t opid)
{
    if (WITHIN_GC_MAP(opid)) {
        GC_MARK(opid);
    }
}

void gc_memory_manager::gc_mark()
{
	//[MC]
    if(gc_disabled)
		return;

    SERVER_TRACE_MSG((msg_gc|msg_time, "Prepare to start GC\n"));

    if (server->id == GC_COORDINATOR) {
        gc_initialize();
        if (!gc_start()) {
            gc_term_event.signal();
            return;
        }
    }
    time_t gc_start_time = time(NULL);
    SERVER_TRACE_MSG((msg_gc|msg_time, "Start mark phase of garbage collector\n"));

    gc_cs.enter();
    cs.enter();
    server->obj_mgr->mark_loaded_objects(1);
    cs.leave();
    gc_grey_set.put_root();

    while (!(gc_state & gcs_abort)) {
        opid_t opid = gc_grey_set.get();
        if (opid != 0) {
            gc_scan_object(opid);
        } else {
            gc_state &= ~gcs_cont;
            gc_cs.leave();
            for (int i = n_servers; --i >= 0;) {
                export_buffer* bp = &gc_export_buf[i];
                if (bp->curr > 0) {
                    bp->req.gc_sync.cmd = dbs_request::cmd_gcsync;
                    bp->req.gc_sync.fun = gcc_refs;
                    bp->req.gc_sync.n_refs = bp->curr;
                    server->send(i, &bp->req, bp->curr*sizeof(opid_t));
                    bp->curr = 0;
                }
            }
            if (gc_finish()) {
                gc_sweep();
                fmm->gc_finished();
                time_t gc_time = time(NULL) - gc_start_time;
                if (gc_time > gc_max_time) {
                    gc_max_time = gc_time;
                }
                gc_total_time += gc_time;
                n_completed_gc += 1;
                gc_cs.enter();
                break;
            } else {
                gc_cs.enter();
            }
        }
    }
    server->obj_mgr->set_save_version_mode(False);
    SERVER_TRACE_MSG((msg_gc, "Finish GC\n"));
    gc_state = 0;
    gc_term_event.signal();
    gc_cs.leave();
    cs.enter();
    if (gc_wait_flag != 0) {
        gc_wait_event.signal();
    }
    cs.leave();
}



void gc_memory_manager::gc_sweep()
{
    cs.enter();
    internal_assert(gc_state & gcs_sweep);
    SERVER_TRACE_MSG((msg_gc, "Start sweep phase of garbage collector\n"));
    server->obj_mgr->mark_loaded_objects(2);
#if 0
    //
    // Reserved objects are immediatly incuded in list of object instances for
    // the client process. So there is no need to warry about protecting them from GC.
    // But if server->obj_mgr->new_object is called only when object is actually allocated
    // (not only reserved) then the following call is needed.
    //
    server->iterate_clients(&client_process::mark_reserved_objects);
#endif
    opid_t last = max_opid+1;
    if (last > opid_t(index_end - index_beg)) {
        last = opid_t(index_end - index_beg);
    }
    max_opid = last-1;
    if (last > GC_MAP_LAST()) {
        last = opid_t(GC_MAP_LAST());
    }
    cs.leave();

    int max_profiled_cpid = max_cpid+1;
    if (max_profiled_cpid > n_profiled_classes) {
        max_profiled_cpid *= 2;
        delete[] profile_instances;
        delete[] profile_size;
        profile_instances = new size_t[max_profiled_cpid];
        profile_size = new nat8[max_profiled_cpid];
        n_profiled_classes = max_profiled_cpid;
    }   
    memset(profile_instances, 0, max_profiled_cpid*sizeof(size_t));
    memset(profile_size, 0, max_profiled_cpid*sizeof(nat8));

    boolean reconstruction = rebuild_free_list;
    size_t  used_objects = 0;
    fsize_t used_space = 0;

    register nat4* gc_map = this->gc_map;
    register opid_t opid = ROOT_OPID;
    register dbs_handle* hp;

    GC_MARK(ROOT_OPID);

    while (opid < last) {
        cs.enter();
        dbs_handle head;
        register opid_t end = last < opid + SWEEP_WINDOW_SIZE
                            ? last : opid + SWEEP_WINDOW_SIZE;
        register dbs_handle* chain = &head;
        hp = index_beg;

        do {
            if (!GC_MARKED(opid)) {
                if (!hp[opid].is_free()) {
                    size_t size = hp[opid].get_size();
                    if (size != 0) {
                        SERVER_TRACE_MSG((msg_gc, "Free object %x, size = %x\n", 
                                   opid, size));
                        if (server->class_mgr->is_external_blob(hp[opid].get_cpid())) {
                            server->remove_external_blob(opid);
                        } else {
                            fmm->dealloc(hp[opid].get_pos(), size);
                        }
                    }
                    hp[opid].mark_as_free();
#if CHECK_LEVEL < 3
                    chain->set_next(opid);
                    chain = &hp[opid];
#endif
                } else if (reconstruction) {
                    chain->set_next(opid);
                    chain = &hp[opid];
                }
            } else {
                size_t obj_size = hp[opid].get_size();
                cpid_t obj_cpid = hp[opid].get_cpid();
                used_space += obj_size;
                used_objects += 1;
                if (obj_cpid < max_profiled_cpid) { 
                    profile_instances[obj_cpid] += 1;
                    profile_size[obj_cpid] += obj_size;
                }
            }
        } while (++opid < end);

        if (chain != &head) {
            chain->set_next(hp->get_next());
            hp->set_next(head.get_next());
        }
        cs.leave();
    }
    cs.enter();
    int last_cpid = max_cpid < MAX_CPID ? max_cpid : MAX_CPID;
    hp = index_beg;
    for (int cpid = MIN_CPID; cpid <= last_cpid; cpid++) {
        if (!GC_MARKED(cpid)) {
            if (!hp[cpid].is_free()) {
                server->class_mgr->remove(cpid);
                max_cpid = cpid;
            }
        } else {
            max_cpid = cpid;
            used_objects += 1;
            used_space += hp[cpid].get_size();
        }
    }
    n_used_objects = used_objects;
    total_used_space = used_space;
    if (reconstruction) {
        rebuild_free_list = False;
    }
    cs.leave();
}

void gc_memory_manager::recovery(int stage)
{
    if (stage == 0) {
        fmm->clean();
        total_used_space = 0;
        n_used_objects = 0;
    } else {
        max_cpid = 0;
        max_opid = ROOT_OPID;
        register dbs_handle* hp = index_beg;
        register opid_t opid, last_opid = index_end - index_beg;

        for (opid = MIN_CPID; opid < last_opid; opid++) {
            if (!hp[opid].is_free()) {
                if (opid <= MAX_CPID && opid > max_cpid) {
                    max_cpid = opid;
                } else if (opid > max_opid) {
                    max_opid = opid;
                }
                if (!hp[opid].is_recovered_object()) {
                    fposi_t pos = hp[opid].get_pos();
                    size_t size = hp[opid].get_size();
                    if (pos == INVALID_OBJECT_POSITION || size == 0) { 
                        hp[opid].mark_as_free();
                        hp[opid].set_next(hp->get_next());
                        hp->set_next(opid);
                    } else if (pos == EXTERNAL_BLOB_POSITION) { // external BLOB
                        n_used_objects += 1;
                    } else if (fmm->check_location(pos, size) == 0) {
                        n_used_objects += 1;
                        total_used_space += size;
                        fmm->confirm_alloc(size, pos);
                    } else {
                        server->message(msg_error|msg_time, "Horror!  Object "
                                        "overlap detected.  Freeing object "
                                        "%lu to prevent further damage.", opid);
                        hp[opid].mark_as_free();
                        hp[opid].set_next(hp->get_next());
                        hp->set_next(opid);
                    }
                } else {
                    hp[opid].clear_recovered_flag();
                }
            }
        }
        index_beg->set_size(max_opid);  
        index_beg->set_cpid(max_cpid);
        if (max_opid < last_opid) { 
            memset(&hp[max_opid+1], 0, (last_opid-max_opid-1)*sizeof(dbs_handle));
        }
    }
}

void gc_memory_manager::gc_sync(stid_t sid, dbs_request const& req)
{
    dbs_request rep;
    dnm_array<nat4> buf;
    static const char* gc_sync_requests[] = {
        "gcc_init", "gcc_prepare", "gcc_ready", "gcc_busy", "gcc_cancel",
        "gcc_mark", "gcc_refs", "gcc_abort", "gcc_finish", "gcc_sweep"
    };

    SERVER_TRACE_MSG((msg_request, "gc_sync request from server %d: %s, state = %x\n",
               sid, gc_sync_requests[req.gc_sync.fun], gc_state));

    switch (req.gc_sync.fun) {
      case gcc_init:
        internal_assert(server->id == GC_COORDINATOR);
        cs.enter();
        gc_initiate();
        cs.leave();
        break;

      case gcc_mark:
        gc_cs.enter();
        if (!shutdown_flag && (gc_state & gcs_prepare)) {
            gc_state =
                (gc_state & ~gcs_prepare) | gcs_active | gcs_mark | gcs_cont;
            gc_term_event.reset();
            task::create(start_garbage_collection, this, task::pri_background);
        } else {
            server->message(msg_error|msg_gc|msg_time, 
                "Receive unexpected GC start mark phase request\n");
        }
        gc_cs.leave();
        break;

      case gcc_abort:
        assert(sid == GC_COORDINATOR);
        gc_cs.enter();
        if (gc_state & gcs_active) {
            gc_state |= gcs_abort;
            server->message(msg_error|msg_gc|msg_time,
                             "GC process aborted by server\n");
            if ((gc_state & (gcs_mark|gcs_cont)) == gcs_mark) {
                gc_sem.signal();
            }
        } else {
            server->message(msg_error|msg_gc|msg_time, 
                "Receive unexpected GC abort request\n");
        }
        gc_cs.leave();
        break;

      case gcc_sweep:
        gc_cs.enter();
        if (gc_state == (gcs_active|gcs_mark)) {
            gc_state = (gc_state & ~gcs_mark) | gcs_sweep;
            gc_sem.signal();
        } else {
            server->message(msg_error|msg_gc|msg_time, 
                "Receive unexpected start sweep phase request, GC state %x\n",
                             gc_state);
        }
        gc_cs.leave();
        break;

      case gcc_refs:
        buf.change_size(req.gc_sync.n_refs);
        server->read_msg_body(sid, &buf, req.gc_sync.n_refs*sizeof(opid_t));
        gc_cs.enter();
        if ((gc_state & (gcs_active|gcs_prepare)) && !(gc_state & gcs_sweep)) {
            int n = req.gc_sync.n_refs;
            assert (n <= GC_REFS_BUF_SIZE);
            opid_t* opp = &buf;
            int prev_state = gc_state;
            gc_extern_references[sid].n_import += n;
            while (--n >= 0) {
                opid_t opid = *opp++;
                unpack4(opid);
                gc_note_reference(server->id, opid);
            }
            if ((prev_state & (gcs_mark|gcs_cont)) == gcs_mark) {
                gc_sem.signal();
            }
        } else {
            server->message(msg_gc|msg_error|msg_time,
                             "Receving gcc_refs message from server %u "
                             "when GC is not active\n", sid);
        }
        gc_cs.leave();
        break;

      case gcc_finish:
        internal_assert(server->id == GC_COORDINATOR);
        buf.change_size(req.gc_sync.len);
        server->read_msg_body(sid, &buf, req.gc_sync.len*sizeof(opid_t));
        gc_cs.enter();
        if (gc_state & gcs_active) {
            int n = n_servers;
            assert((int)req.gc_sync.len == n*2);
            extern_references* dst = gc_refs_matrix[sid];
            extern_references* src = (extern_references*)&buf;
            while (--n >= 0) {
                *dst = *src++;
                dst->unpack();
                dst += 1;
            }
            gc_sem.signal();
        } else {
            server->message(msg_gc|msg_error|msg_time,
                             "Receving gcc_finish message from server %u "
                             "when GC is not active\n", sid);
        }
        gc_cs.leave();
        break;

      case gcc_prepare:
        assert(sid == GC_COORDINATOR);
        rep.gc_sync.cmd = dbs_request::cmd_gcsync;
        gc_cs.enter();
        if (!opened || !initialized || (gc_state & gcs_active)) {
            rep.gc_sync.fun = gcc_busy;
        } else {
            rep.gc_sync.fun = gcc_ready;
            gc_state = gcs_prepare;
            gc_initialize();
            server->obj_mgr->set_save_version_mode(True);
        }
        gc_cs.leave();
        rep.gc_sync.timestamp = req.gc_sync.timestamp;
        server->send(GC_COORDINATOR, &rep);
        break;

      case gcc_busy:
        assert(server->id == GC_COORDINATOR);
        gc_cs.enter();
        if (gc_timestamp == time_t(req.gc_sync.timestamp)) {
            internal_assert(gc_state & gcs_prepare);
            gc_state |= gcs_abort;
            gc_timestamp = 0; // ignore all other responces
            gc_sem.signal();
        }
        gc_cs.leave();
        break;

      case gcc_ready:
        assert(server->id == GC_COORDINATOR);
        gc_cs.enter();
        if (gc_timestamp == time_t(req.gc_sync.timestamp)) {
            internal_assert(gc_state & gcs_prepare);
            if (++n_ready_to_gc_servers == n_servers) {
                gc_sem.signal();
            }
        }
        gc_cs.leave();
        break;

      case gcc_cancel:
        assert(sid == GC_COORDINATOR);
        gc_cs.enter();
        if ((gc_state & (gcs_prepare|gcs_active)) == gcs_prepare) {
            gc_state &= ~gcs_prepare;
            server->obj_mgr->set_save_version_mode(False);
        }
        gc_cs.leave();
        break;

      default:
        server->message(msg_error|msg_time, 
                         "Illegal request to memory manager: %d\n",
                         req.gc_sync.fun);
    }
}

//
// Abort method can be called to terminate GC either by "close" method
// when storage is closed or by receiveing request from GC coordinator or
// (if server is coordinator itself) when some of remote servers
// is reconnected (it means that current GC process can not be finished).
//
void gc_memory_manager::gc_abort(boolean wait)
{
    gc_cs.enter();
    if (opened && (gc_state & (gcs_active|gcs_prepare))) {
        if (server->id != GC_COORDINATOR && (gc_state & gcs_prepare)) {
            gc_state &= ~gcs_prepare;
            server->obj_mgr->set_save_version_mode(False);
        } else {
            if (!(gc_state & gcs_sweep)) {
                gc_state |= gcs_abort;
                if (!(gc_state & gcs_cont)) {
                    gc_sem.signal();
                }
                if (server->id == GC_COORDINATOR) {
                    dbs_request req;
                    req.gc_sync.cmd = dbs_request::cmd_gcsync;
                    req.gc_sync.fun = gcc_abort;
                    gc_cs.leave();
                    for (int i = 0; i < n_servers; i++) {
                        if (i != GC_COORDINATOR) {
                            server->send(i, &req);
                        }
                    }
                    gc_cs.enter();
                }
            }
            if (wait) {
                gc_cs.leave();
                gc_term_event.wait(); // Wait GC thread termination
                gc_cs.enter();
            }
        }
    }
    gc_cs.leave();
}

void gc_memory_manager::gc_init_process()
{
    while (gc_init_idle_period) {
        long proceed_requests = n_requests;
        if (gc_init_sem.wait_with_timeout(gc_init_idle_period)) {
            continue;
        }
        if (n_requests == proceed_requests) {
            if (gc_allocated > gc_init_min_allocated) {
                cs.enter();
                gc_initiate();
                cs.leave();
            }
        } else {
            n_requests = proceed_requests;
        }
    }
    gc_init_task_event.signal();
}

void gc_memory_manager::set_gc_init_alloc_size(size_t watermark)
{
    gc_watermark = watermark;
}

void gc_memory_manager::set_gc_init_used_size(fsize_t watermark)
{
    gc_used_watermark = watermark;
}

void gc_memory_manager::set_gc_init_timeout(time_t timeout)
{
    gc_init_timeout = timeout;
}

void gc_memory_manager::set_gc_grey_set_extension_threshold(size_t max_extension)
{
    gc_grey_set.max_set_extension = (int)max_extension;
}

void gc_memory_manager::set_gc_init_min_allocated(size_t min_allocated)
{
    gc_init_min_allocated = min_allocated;
}

void gc_memory_manager::set_gc_init_idle_period(time_t period)
{
    if (gc_init_idle_period != 0) {
        gc_init_idle_period = period;
        gc_init_sem.signal();
        if (period == 0) {
            gc_init_task_event.wait();
        }
    } else if (period != 0) {
        gc_init_idle_period = period;
        gc_init_task_event.reset();
        task::create(start_gc_init_process, this);
    }
}


void gc_memory_manager::set_gc_response_timeout(time_t timeout)
{
    gc_response_timeout = timeout;
}

void task_proc gc_memory_manager::start_gc_init_process(void* arg)
{
    ((gc_memory_manager*)arg)->gc_init_process();
}

void gc_memory_manager::initialize()
{
    dbs_handle* root = &index_beg[ROOT_OPID];
    if (root->is_free()) {
        root->set(RAW_CPID, 0, 0);
    }
    if (max_opid < ROOT_OPID) {
        max_opid = ROOT_OPID;
    }
    initialized = True;
    if (gc_init_idle_period != 0) {
        gc_init_task_event.reset();
        task::create(start_gc_init_process, this);
    }
}

void gc_memory_manager::shutdown()
{
    cs.enter();
    shutdown_flag = True;
    if (gc_wait_flag != 0) {
        gc_wait_event.signal();
    }
    cs.leave();
    if (opened && gc_init_idle_period != 0) {
        gc_init_idle_period = 0;
        gc_init_sem.signal();
        gc_init_task_event.wait();
    }
    gc_abort();
}

void gc_memory_manager::dump(char* what)
{
    cs.enter();
    fmm->dump(what);
    cs.leave();
    console::output("Object index size: %lu\n", index_end - index_beg);
    console::output("Allocated space since last GC: %lu\n", gc_allocated);
    if (n_used_objects != 0) {
        console::output("Number of used objects in storage: %lu\n",
                        n_used_objects);
        console::output("Total used space in storage: %" INT8_FORMAT "u\n",
                        total_used_space);
    }
    if (max_gc_delay != 0) {
        console::output("Maximal application delay due to GC: %u (sec)\n",
                        max_gc_delay);
    }
    if (gc_state & gcs_sweep) {
        console::output("Sweep stage of garbage collector is in progress\n");
    } else if (gc_state & gcs_cont) {
        console::output("Mark stage of garbage collector is in progress\n");
        console::output("Number of references in GC grey set: %u\n",
                        gc_grey_set.size());
    } else if (gc_state & gcs_mark) {
        console::output("Mark stage of garbage collector locally finished\n");
    } else if (gc_state & gcs_prepare) {
        console::output("Prepare to start GC\n");
    } else if (gc_state & gcs_active) {
        console::output("Garbage collector is active\n");
    }
    if (n_completed_gc > 0) {
        console::output("Maximal time of GC completion: %u (sec)\n"
                        "Total time spent in GC: %u (sec)\n"
                        "Average time of GC completion: %lf (sec)\n"
                        "Number of completed GC: %lu\n",
                        gc_max_time,
                        gc_total_time,
                        (double)gc_total_time / n_completed_gc,
                        n_completed_gc);
        if (!(gc_state & gcs_cont)) {
            console::output("Profile of classes:\n");
            char class_name_buf[256];
            for (int i = n_profiled_classes; --i >= 0;) { 
                size_t n_instances = profile_instances[i];
                if (n_instances != 0) { 
                    console::output("%s\t%lu instances, %" INT8_FORMAT "u total size, %" INT8_FORMAT "u average size\n", 
                                    server->class_mgr->get_class_name(i, class_name_buf, sizeof(class_name_buf)), 
                                    n_instances, profile_size[i], profile_size[i] / n_instances);
                }
            }
        }
    }
}

boolean gc_memory_manager::backup(file& backup_file)
{
    extend_cs.enter();
    boolean result=opened && do_file_backup((char*)index_beg,
                                            (char*)index_end-(char*)index_beg,
                                            backup_file, "object index");
    extend_cs.leave();
    return result && fmm->backup(backup_file);
}


boolean gc_memory_manager::restore(file& backup_file)
{
    return do_file_restore(*index, backup_file, "object index")
        && fmm->restore(backup_file);
}

void gc_memory_manager::stop_backup()
{
    fmm->stop_backup();
}


fsize_t gc_memory_manager::get_storage_size()
{
    cs.enter();
    fsize_t size = fmm->get_storage_size();
    cs.leave();
    return size;
}

fsize_t gc_memory_manager::get_used_size()
{
    return total_used_space;
}

size_t gc_memory_manager::get_allocation_quantum()
{
    return fmm->get_allocation_quantum();
}

struct db_object_header { 
    opid_t  opid;
    fposi_t pos;
};

static int compare_offs(void const* a, void const* b)
{
    return ((db_object_header*)a)->pos < ((db_object_header*)b)->pos ? -1 
        : ((db_object_header*)a)->pos == ((db_object_header*)b)->pos ? 0 : 1;
}


void gc_memory_manager::compactify()
{
    gc_cs.enter();
    cs.enter();

    // do garbage collection
    if (!(gc_state & gcs_prepare)) {
        gc_allocated = 0;
        gc_state = gcs_prepare;
        gc_mark();
    }

    register dbs_handle* hp = index_beg;
    register opid_t i, opid, last_opid = index_end - index_beg;

    db_object_header* hdr = new db_object_header[last_opid];
    assert(hdr != NULL);
    memset(hdr, 0, sizeof(db_object_header)*last_opid);
    
    opid_t new_opid = ROOT_OPID;
    opid_t max_opid = 0;
    for (opid = 1; opid < last_opid; opid++) {
        if (!hp[opid].is_free()) {
            hdr[opid].pos = hp[opid].get_pos();
            hdr[opid].opid = opid;
            max_opid = opid;
            if (opid >= ROOT_OPID) {
                new_opid += 1;
            }
        }
    }
    boolean compactify_index = false;
    opid_t* opid_map = NULL;
    if (server->get_number_of_servers() == 1 && new_opid*2 < max_opid) {
        if (server->obj_mgr->get_number_of_objects() != 0) { 
            console::message(msg_warning, "Could not perform index compactification when there are opened connections to the database");
        } else { 
            compactify_index = true;
        }
    }

    if (compactify_index) {
        opid_map = new opid_t[max_opid+1];
        for (opid = new_opid = ROOT_OPID; opid < last_opid; opid++) {
            if (!hp[opid].is_free()) {
                hp[new_opid] = hp[opid];
                hdr[opid].opid = new_opid;
                opid_map[opid] = new_opid;
                new_opid += 1;
            }
        }
        hp->set_size(new_opid-1);
        this->max_opid = new_opid-1;
        size_t index_size = index->get_mmap_size();
        while ((index_size >> 1)/sizeof(dbs_handle) > new_opid) { 
            index_size >>= 1;
        }
        file::iop_status status = index->set_size(index_size);
        if (status != file::ok) {
            msg_buf buf;
            index->get_error_text(status, buf, sizeof buf);
            console::error("Failed to truncate index: %s\n", buf);            
        }
        index_beg = hp = (dbs_handle*)index->get_mmap_addr();
        index_end = (dbs_handle*)index->get_mmap_addr() + index_size/sizeof(dbs_handle);
        memset(&hp[new_opid], 0, (char*)index_end - (char*)&hp[new_opid]);
    }

    qsort(hdr, last_opid, sizeof(db_object_header), &compare_offs);

    fposi_t curr_pos = 0;
    dnm_buffer buf;
    stid_t self_id = server->id;

    for (i = 0; i < last_opid; i++) {
        opid = hdr[i].opid;
        cpid_t cpid = hp[opid].get_cpid();
        if (opid != 0 && !hp[opid].is_free() && (opid == cpid || !server->class_mgr->is_external_blob(cpid))) {
            hp[opid].set_pos(curr_pos);
            size_t size = hp[opid].get_size();
            buf.put(size);
            char* obj = &buf;
            server->pool_mgr->read(hdr[i].pos, obj, size);
            if (compactify_index) {
                int n_refs = (opid != cpid && size >= sizeof(dbs_reference_t))
                    ? server->class_mgr->get_number_of_references(cpid, size) : 0;
                char* p = obj;
                while (--n_refs >= 0) {
					objref_t ropid;
                    stid_t rsid;
                    unpackref(rsid, ropid, p);
                    if (ropid != 0 && rsid == self_id) {                        
                        packref(p, self_id, opid_map[ropid]);
                    }
                    p += sizeof(dbs_reference_t);
                }
            }
            server->pool_mgr->write(curr_pos, obj, size);
            curr_pos += DOALIGN(size, MEMORY_ALLOC_QUANT);
        }
    }    
    fmm->compactify(curr_pos);
    server->pool_mgr->truncate(curr_pos);
    server->pool_mgr->flush();
    flush();
    cs.leave();    
    gc_cs.leave();
    delete[] opid_map;
    delete[] hdr; 
}

void gc_memory_manager::start_gc()
{
    gc_initiate();
}

void gc_memory_manager::create_scavenge_task()
{
    task::create(start_scavenge, this, task::pri_background);
}

void task_proc gc_memory_manager::start_scavenge(void* arg)
{
    ((gc_memory_manager*)arg)->scavenge();
}

//
// This neat function, called by the GOODS administrative console command
// "scavenge", looks for objects whose broken references will most certainly
// break a garbage collection run.  These objects could only get broken by
// some heinous bug like a memory bounds violation overwrite or a bad RAID
// controller.
//
void gc_memory_manager::scavenge()
{
    cs.enter();
    console::message(msg_notify|msg_time, "Scavenge initiated.\n");

    dnm_buffer  buf;
    dbs_handle  hnd;
    opid_t      last    = (index_end - index_beg);
    int         ns      = n_servers;
    opid_t      opid    = 0;
    nat1        percent = 0;
    stid_t      sid     = server->id;

    while (++opid < last) {
        hnd = index_beg[opid];
        if (!hnd.is_free()) {
            cpid_t cpid = hnd.get_cpid();
            size_t size = hnd.get_size();

            int n_refs = (opid != cpid && size >= sizeof(dbs_reference_t))
                ? server->class_mgr->get_number_of_references(cpid, size) : 0;

            char* p = NULL;
            if (n_refs != 0) {
                p = buf.put(n_refs*sizeof(dbs_reference_t));
                server->pool_mgr->read(hnd.get_pos(), p, buf.size());

                while (--n_refs >= 0) {
					objref_t ropid;
                    stid_t rsid;
                    p = unpackref(rsid, ropid, p);
                    if (((sid == rsid) && (ropid >= last))
                        || ((sid == rsid) && index_beg[ropid].is_free())
                        || (rsid >= ns)) {
                        console::message(msg_error|msg_time, "%x:%x refers to "
                            "nonexistent object %x:%x\n", sid, opid,
                            rsid, ropid);
                        n_refs = 0;
                    }
                }
            }
        }

        nat1 new_percent = (nat1)((nat8)opid * 100 / (nat8)last);
        if (percent < new_percent) {
            percent = new_percent;
            console::message(msg_notify|msg_time, "scavenge: %3u%% complete.\n",
                percent);
        }
    }
    console::message(msg_notify|msg_time, "Scavenge completed.\n");
    cs.leave();
}

static void check_idx_overlap(opid_t opid, dbs_handle& hnd, nat1* m)
{
    nat1 bit = 0x80 >> ((hnd.get_pos() / MEMORY_ALLOC_QUANT) % 8);

    m += (hnd.get_pos() / MEMORY_ALLOC_QUANT / 8);
    for (nat4 i = 0; i < hnd.get_size(); i += MEMORY_ALLOC_QUANT) {
        if (*m & bit) {
            console::message(msg_error|msg_time, "checkidx: object %x "
                             "overlaps another object!\n", opid);
        }
        *m |= bit;
        bit >>= 1;
        if (bit == 0)  {
            bit = 0x80;
            ++m;
        }
    }
}

//
// This neat little function validates the integrity of this database storage's
// object index (.idx file).  Any problems detected are logged.
//
// This algorithm:
//
// * validates each allocated object's class ID is valid;
// * validates that no two objects are indexed to overlap in the .odb file.
// * validates that the "free" list is intact;
//   not too big a deal if it's not; GOODS automatically rebuilds this list if
//   it detects a problem
//

void gc_memory_manager::check_idx_integrity()
{
    dbs_handle hnd;
    opid_t     last_opid = (opid_t)(index_end - index_beg);
    dnm_buffer freemap;
    cpid_t     max_cpid  = server->class_mgr->get_max_cpid();
    opid_t     max_opid  = (opid_t)index_beg[0].get_size();
    fsize_t    max_size  = get_storage_size();
    dnm_buffer memmap;
    opid_t     opid;

    cs.enter();
    console::message(msg_notify|msg_time, "checkidx initiated.\n");

    if (max_opid >= last_opid) {
        console::message(msg_error|msg_time, "checkidx: idx[0].size (max "
                         "opid) %d > .idx file size (%d).\n",
                         max_opid, last_opid);
    }

    // Make sure every object has a valid class ID, and that none overlap.
    nat1* m = (nat1*)memmap.put((max_size / 8) + (max_size % 8 != 0));
    memset(m, 0, memmap.allocated_size());
    for (opid = ROOT_OPID; opid <= max_opid; opid += 1) {
        hnd = index_beg[opid];
        if (hnd.is_free())
            continue;
        if (hnd.get_cpid() > max_cpid) {
            console::message(msg_error|msg_time, "checkidx: object %x entry "
                             "contains invalid class ID %d.\n", opid,
                             hnd.get_cpid());
        }
        if (hnd.get_pos() % MEMORY_ALLOC_QUANT != 0) {
            console::message(msg_error|msg_time, "checkidx: object %x at "
                             "offset %" INT8_FORMAT "u is not quantum aligned.\n",
                             opid, hnd.get_pos());
        }
        if (hnd.get_pos() >= max_size) {
            console::message(msg_error|msg_time, "checkidx: object %x offset %"
                              INT8_FORMAT "u is beyond the storage's capacity %"
                              INT8_FORMAT "u.\n", opid, hnd.get_pos(), max_size);
        } else if (hnd.get_pos() + hnd.get_size() > max_size) {
            console::message(msg_error|msg_time, "checkidx: object %x at "
                             "offset %" INT8_FORMAT "u of size %u is too large "
                             "to fit in the storage's capacity %" INT8_FORMAT 
                             "u.\n", opid, hnd.get_pos(), hnd.get_size(),
                             max_size);
        } else {
            check_idx_overlap(opid, hnd, m);
        }
    }

    // Make sure the "free" linked list is intact.
    freemap.put((last_opid / 8) + (last_opid % 8 != 0));
    opid = 0;
    do {
        hnd  = index_beg[opid];
        if (opid && hnd.get_size() != 0) {
            console::message(msg_error|msg_time, "checkidx: free object %x "
                             "has non-zero size: %u.\n", opid, hnd.get_size());
        }
        if (opid && hnd.get_cpid() != 0) {
            console::message(msg_error|msg_time, "checkidx: free object %x "
                             "has non-zero cpid: %u.\n", opid, hnd.get_cpid());
        }
        if ((opid_t)hnd.get_next() >= last_opid) {
            console::message(msg_error|msg_time, "checkidx: free object %x's"
                             "next index is bad: %x.\n", opid, hnd.get_pos());
            break;
        }
        opid = (opid_t)hnd.get_next();
    } while (opid != 0);

    console::message(msg_notify|msg_time, "checkidx completed.\n");
    cs.leave();
}

static os_file *backup_odb      = NULL;
static fsize_t  backup_odb_size = 0;

void gc_memory_manager::close_object_backup()
{
    if (backup_odb) {
        backup_odb->close();
        delete backup_odb;
        backup_odb = NULL;
    }
}

void gc_memory_manager::set_object_backup(const char* backup_odb_file)
{
    close_object_backup();
    os_file *f = backup_odb = new os_file(backup_odb_file);
    if ((f->open(f->fa_read, f->fo_random | f->fo_largefile) != f->ok) 
       || (f->get_size(backup_odb_size) != f->ok)) {
        console::message(msg_notify|msg_time, "Failed opening file \"%s\".\n",
                         backup_odb_file);
    }
}

boolean gc_memory_manager::get_dbs_handle(const char* arg, dbs_handle& o_hnd)
{
    unsigned int a, b;
    opid_t       opid;
    stid_t       sid;

    sscanf(arg, "%x:%x", &a, &b);
    sid  = (stid_t)a;
    opid = (opid_t)b;
    if (sid != server->id) {
        console::message(msg_notify|msg_time, "Storage ID mismatch: "
                         "%x != %x.\n", sid, server->id);
        return false;
    } else if (opid >= opid_t(index_end - index_beg)) {
        console::message(msg_notify|msg_time, "Object ID beyond max: "
                         "%x > %x.\n", opid, (index_end - index_beg));
        return false;
    }
    o_hnd = index_beg[opid];
    if (o_hnd.is_free()) {
        console::message(msg_notify|msg_time, "Cannot zero out free object "
                         "%x:%x.\n", sid, opid);
        return false;
    }
    return true;
}

void gc_memory_manager::extract_object_from_backup(const char* arg)
{
    dnm_buffer buf;
    dbs_handle hnd;

    if (!backup_odb) {
        console::message(msg_notify|msg_time, "First, specify a backup odb "
                         "file from which to extract objects.\n");
        return;
    }
    if (!get_dbs_handle(arg, hnd))  return;
    size_t size = hnd.get_size();
    fposi_t pos = hnd.get_pos();

    if (pos + size > backup_odb_size) {
        console::message(msg_notify|msg_time, "Object %s is not in "
                         "backup odb \"%s\"\n", arg, backup_odb->get_name());
        return;
    }
    char* p = buf.put(size);
    backup_odb->read(pos, p, size);
    server->pool_mgr->write(pos, p, size);
}

void gc_memory_manager::zero_out_object_data(const char* arg)
{
    dnm_buffer buf;
    dbs_handle hnd;

    if (!get_dbs_handle(arg, hnd))  return;
    size_t size = hnd.get_size();
    fposi_t pos = hnd.get_pos();
    char* p = buf.put(size);
    memset(p, 0, size);
    server->pool_mgr->write(pos, p, size);
}

void gc_memory_manager::log_object_offset(const char* arg)
{
    dbs_handle hnd;
    if (!get_dbs_handle(arg, hnd)) return;
    fposi_t pos = hnd.get_pos();
    size_t size = hnd.get_size();
    console::message(msg_notify|msg_time, "Object %s is at offset %"  INT8_FORMAT 
      "u, size %u.\n", arg, pos, size);
}

boolean gc_memory_manager::check_object_class(cpid_t old_cpid, cpid_t new_cpid)
{
    char name[2][256];
    return new_cpid == old_cpid || old_cpid == RAW_CPID || 
        strcmp(server->class_mgr->get_class_name(new_cpid, name[0], sizeof(name[0])), 
               server->class_mgr->get_class_name(old_cpid, name[1], sizeof(name[1]))) == 0;
}

boolean gc_memory_manager::verify_object(opid_t opid, cpid_t cpid)
{ 
    dbs_handle* hp = index_beg;
    return ((opid == cpid && ((hp[opid].get_cpid() == cpid && cpid <= max_cpid) || hp[opid].is_free()))
            || (cpid >= MIN_CPID && cpid <= max_cpid && opid >= ROOT_OPID && opid <= max_opid && !hp[opid].is_free() 
                && hp[cpid].get_cpid() == cpid && check_object_class(hp[opid].get_cpid(), cpid)));
}

boolean gc_memory_manager::verify_reference(opid_t opid)
{
    return opid == 0 || (opid >= ROOT_OPID && opid <= max_opid && !index_beg[opid].is_free());
}

END_GOODS_NAMESPACE
