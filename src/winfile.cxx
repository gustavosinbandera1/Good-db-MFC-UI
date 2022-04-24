// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< WINFILE.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 25-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// File implementation for Windows
//-------------------------------------------------------------------*--------*

#include "stdinc.h"
#pragma hdrstop
#include <io.h>
#include "mmapfile.h"

BEGIN_GOODS_NAMESPACE

#define BUFFERED_WRITE_THROUGH 1

#define BAD_POS 0xFFFFFFFF // returned by SetFilePointer and GetFileSize

#define MMAP_EXTEND_QUANTUM     64*1024*1024      /* 64Mb */
#define MMAP_DOUBLE_THRESHOLD 1024*1024*1024      /* 1Gb */

static unsigned int win_access_mode[] = {
    GENERIC_READ, GENERIC_WRITE, GENERIC_READ | GENERIC_WRITE 
};

static unsigned int win_open_flags[] = {
    OPEN_EXISTING, TRUNCATE_EXISTING, OPEN_ALWAYS, CREATE_ALWAYS
};

static unsigned int win_open_attrs[] = {
    FILE_FLAG_SEQUENTIAL_SCAN,
    FILE_FLAG_SEQUENTIAL_SCAN|FILE_FLAG_WRITE_THROUGH, 
    FILE_FLAG_RANDOM_ACCESS,
    FILE_FLAG_RANDOM_ACCESS|FILE_FLAG_WRITE_THROUGH,
    FILE_FLAG_NO_BUFFERING|FILE_FLAG_SEQUENTIAL_SCAN, 
    FILE_FLAG_NO_BUFFERING|FILE_FLAG_SEQUENTIAL_SCAN|FILE_FLAG_WRITE_THROUGH, 
    FILE_FLAG_NO_BUFFERING|FILE_FLAG_RANDOM_ACCESS,
    FILE_FLAG_NO_BUFFERING|FILE_FLAG_RANDOM_ACCESS|FILE_FLAG_WRITE_THROUGH    
};

static unsigned int win_page_access[] = {
    PAGE_READONLY, PAGE_READWRITE, PAGE_READWRITE 
};

static unsigned int win_map_access[] = {
    FILE_MAP_READ, FILE_MAP_WRITE, FILE_MAP_ALL_ACCESS 
};

static file::iop_status get_system_error()
{
    int error = GetLastError();
    switch (error) { 
      case ERROR_HANDLE_EOF:
        return file::end_of_file;
      case NO_ERROR:
        return file::ok;
    }
    return file::iop_status(error);
} 


#define BUFFER_BLOCKS 2 // number of blocks in synchronous write buffer
const size_t MAX_CHUNK_SIZE = 0x1000000;

class win_file_specific { 
  protected: 
    mutex   cs;
    
    char*   buffer[2];
    event   syscall_completion_event[2];
    event   operation_completion_event[2];
    size_t  buf_size;
    size_t  buf_used[2];
    int     bi;       // index of used buffer
    int     rc[2];    // operation completion codes
    boolean ready[2]; // operation completion indicator
    boolean write_through;

    int     n_writes;        // total number of syncronous writes
    int     n_merged_writes; // total number of merged writes
    int     n_par_writes;    // current number of parallel write operations
    int     max_par_writes;  // maximal number of parallel write operations
    
  public:    
    boolean winnt;

    int write(HANDLE fd, const void* src, size_t size) {  
        DWORD written_bytes;
        if (size == 0) { 
            return file::ok;
        }
        if (!write_through || size > (buf_size >> 1)) { 
            n_writes += write_through;
            while (size != 0) { 
                size_t chunk = size > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : size;
                if (!WriteFile(fd, src, DWORD(chunk), &written_bytes, NULL)) { 
                    return get_system_error();
                }
                if (written_bytes != (DWORD)chunk) { 
                    return  file::end_of_file ;
                }
                size -= chunk;
                src = (char*)src + chunk;
            }
            return file::ok;
        } else { 
            cs.enter();
            while (buf_used[bi] + size > buf_size) { 
                cs.leave();
                operation_completion_event[bi].wait();
                cs.enter();
            }
            int i = bi;
            if (buf_used[i] == 0) { 
                buf_used[i] = size;
                operation_completion_event[i].reset();
                syscall_completion_event[i].reset();
                ready[i] = False;
                n_par_writes = 0;
                n_writes += 1;

                if (!ready[1-i]) {
                    cs.leave();
                    operation_completion_event[1-i].wait();
                    cs.enter();
                    internal_assert(ready[1-i]);
                }                   
                internal_assert(i == bi && buf_used[1-i] == 0);

                if (n_par_writes > 0) { 
                    memcpy(buffer[i], src, size); 
                    src = buffer[i];
                    if (n_par_writes > max_par_writes) { 
                        max_par_writes = n_par_writes;
                    }
                    n_merged_writes += n_par_writes;
                }
                bi = 1-i;
                size_t total_size = buf_used[i];
                buf_used[i] -= size;
                cs.leave();
                int result = WriteFile(fd, src, DWORD(total_size),
                                       &written_bytes, NULL) 
                    ? total_size == written_bytes 
                      ? file::ok : file::end_of_file  
                    : get_system_error();
                if (total_size == size) { 
                    cs.enter();
                    ready[i] = True;
                    operation_completion_event[i].signal();
                    cs.leave();
                } else { 
                    rc[i] = result;
                    syscall_completion_event[i].signal();
                }
                return result;
            } else { 
                memcpy(buffer[i] + buf_used[i], src, size);
                buf_used[i] += size;
                n_par_writes += 1;
                cs.leave();
                syscall_completion_event[i].wait();
                cs.enter();
                int result = rc[i];
                if ((buf_used[i] -= size) == 0) { 
                    ready[i] = True;
                    operation_completion_event[i].signal();
                }                   
                cs.leave();
                return result;
            }
        }
    }
    void reset(boolean synchronous) { 
        bi = 0;
#ifdef BUFFERED_WRITE_THROUGH
        write_through = synchronous;
#else
        write_through = False;
#endif
        ready[1] = True;
        buf_used[0] = buf_used[1] = 0;
    }
    //
    // Provide atomic direct access to file (seek + write) for Windows 95
    //
    void lock() { cs.enter(); }
    void unlock() { cs.leave(); }
    
    void dump() { 
        if (n_writes != 0) { 
            console::output("Number of synchronous writes: %d\n"
                            "Average number of parallel requests: %6.3lf\n"
                            "Maximal number of parallel requests: %d\n", 
                            n_writes, 
                            double(n_writes + n_merged_writes)/n_writes, 
                            max_par_writes+1);
        }
    }

    win_file_specific() { 
        OSVERSIONINFO osinfo;
        osinfo.dwOSVersionInfoSize = sizeof osinfo;
        GetVersionEx(&osinfo);
        winnt = (osinfo.dwPlatformId == VER_PLATFORM_WIN32_NT);
        buf_size = os_file::get_disk_block_size() * BUFFER_BLOCKS;
        buffer[0] = (char*)os_file::allocate_disk_buffer(buf_size*2);
        buffer[1] = buffer[0] + buf_size;
        max_par_writes = 0;
        n_writes = 0;
        n_merged_writes = 0;
    }
    ~win_file_specific() {
        os_file::free_disk_buffer(buffer[0]);
    }
};

file::iop_status os_file::set_position(fposi_t pos)
{
    if (opened) { 
        LONG high_pos = nat8_high_part(pos); 
        if (SetFilePointer(fd, nat8_low_part(pos), &high_pos, FILE_BEGIN)
            == BAD_POS)
        {
            return get_system_error();
        }
        return ok;
    }
    return not_opened; 
}

file::iop_status os_file::get_position(fposi_t& pos)
{
    if (opened) { 
        LONG  high_pos = 0;
        DWORD low_pos = SetFilePointer(fd, 0, &high_pos, FILE_CURRENT);
        pos = cons_nat8(high_pos, low_pos);     
        if (low_pos == BAD_POS) {
            return get_system_error();
        }
        return ok;
    }
    return not_opened; 
} 

file::iop_status os_file::read(void* buf, size_t size)
{  
    if (opened) { 
        DWORD read_bytes;
        return ReadFile(fd, buf, DWORD(size), &read_bytes, NULL)
            ? size == read_bytes ? ok : end_of_file
            : get_system_error();
    } else {
        return not_opened;
    }
}

file::iop_status os_file::write(void const* buf, size_t size)
{  
    if (opened) {  
        return iop_status
            (((win_file_specific*)os_specific)->write(fd, buf, size));
    } else { 
        return not_opened;
    }
}

file::iop_status os_file::read(fposi_t pos, void* buf, size_t size) 
{
    if (opened) { 
        DWORD read_bytes;
        if (((win_file_specific*)os_specific)->winnt) { 
            OVERLAPPED Overlapped;
            Overlapped.Offset = nat8_low_part(pos);
            Overlapped.OffsetHigh = nat8_high_part(pos);
            Overlapped.hEvent = NULL;
            return ReadFile(fd, buf, DWORD(size), &read_bytes, &Overlapped)
                ? size == read_bytes ? ok : end_of_file
                : get_system_error(); 
        } else { 
            ((win_file_specific*)os_specific)->lock();
            LONG high_pos = nat8_high_part(pos); 
            if (SetFilePointer(fd, nat8_low_part(pos), &high_pos, FILE_BEGIN)
                == BAD_POS)
            {
                iop_status rc = get_system_error();
                if (rc != ok) {
                    ((win_file_specific*)os_specific)->unlock();
                    return rc;
                }
            }
            boolean success = ReadFile(fd, buf, DWORD(size), &read_bytes, NULL);
            ((win_file_specific*)os_specific)->unlock();
            return success ? size == read_bytes ? ok : end_of_file
                : get_system_error();
        }
    } else { 
        return not_opened;
    }
}

file::iop_status os_file::write(fposi_t pos, void const* buf, size_t size) 
{
    if (opened) { 
        DWORD written_bytes;
        if (((win_file_specific*)os_specific)->winnt) {
            OVERLAPPED Overlapped;
            Overlapped.Offset = nat8_low_part(pos);
            Overlapped.OffsetHigh = nat8_high_part(pos);
            Overlapped.hEvent = NULL;
            return WriteFile(fd, buf, DWORD(size), &written_bytes, &Overlapped)
                ? size == written_bytes ? ok : end_of_file
                : get_system_error(); 
        } else { 
            ((win_file_specific*)os_specific)->lock();
            LONG high_pos = nat8_high_part(pos); 
            if (SetFilePointer(fd, nat8_low_part(pos), &high_pos, FILE_BEGIN)
                == BAD_POS)
            {
                iop_status rc = get_system_error();
                if (rc != ok) {
                    ((win_file_specific*)os_specific)->unlock();
                    return rc;
                }
            }
            boolean success = WriteFile(fd, buf, DWORD(size), &written_bytes, NULL);
            ((win_file_specific*)os_specific)->unlock();
            return success ? size == written_bytes ? ok : end_of_file
                : get_system_error();
        }
    } else { 
        return not_opened;
    }
}

file::iop_status os_file::close()
{
    if (opened) { 
        opened = False;
        return CloseHandle(fd) ? ok : get_system_error();
    } 
    return ok;
}

file::iop_status os_file::open(access_mode mode, int flags)
{
//[MC] We don't use OS cache in 64bit server. The user can select a bigger page pool in 64bit
//	   in 32bit the maximum pool size is around 1 GB, so OS buffering is allowed	
#if defined(_WIN64)
	unsigned int write_flags = fo_sync | fo_random | fo_directio;	// for 64-bits	
#else
	unsigned int write_flags = fo_sync | fo_random;					// for 32-bits
#endif

    close();
    assert(name != NULL); 
    fd = CreateFileA(name, win_access_mode[mode], (mode == fa_read) 
                    ? FILE_SHARE_READ|FILE_SHARE_WRITE : FILE_SHARE_READ, 
                    NULL, win_open_flags[flags & (fo_truncate|fo_create)],
                    win_open_attrs[(flags & write_flags) >> 2],
                    NULL); //directio makes the file access slower under windows comented out by Magaya team
    this->mode = mode;
    this->flags = flags;
    if (fd == INVALID_HANDLE_VALUE) {
        return get_system_error();
    } else { 
        opened = True;
        ((win_file_specific*)os_specific)->reset((flags & fo_sync) != 0);
        return ok;
    }
}

file::iop_status os_file::remove()
{
    close();
    return DeleteFileA(name) 
        ? ok : get_system_error();
}

char const* os_file::get_name() const 
{ 
    return name;
}

file::iop_status os_file::set_name(char const* new_name) 
{
    iop_status status = close();
    if (status == ok) { 
        if (new_name == NULL) { 
            if (name != NULL) { 
                free(name);
            }
            name = NULL; 
        } else { 
            if (name != NULL) { 
                if (strcmp(name, new_name) != 0) { 
                    if (MoveFileA(name, new_name)) {
                        free(name);
                        name = strdup(new_name);
                    } else {
                        return get_system_error();
                    }
                }
            } else { 
                name = strdup(new_name);
            }
        }
    }
    return status;
}

file::iop_status os_file::get_size(fsize_t& size) const
{
    if (opened) { 
        DWORD high_size;
        DWORD low_size = GetFileSize(fd, &high_size);
        size = cons_nat8(high_size, low_size);
        if (low_size == BAD_POS) {
            return get_system_error();
        }
        return ok;
    } else {
        return not_opened;
    }
}

file::iop_status os_file::set_size(fsize_t size)
{
    if (opened) { 
        LONG high_part = nat8_high_part(size); 
        if (SetFilePointer(fd, nat8_low_part(size), &high_part, FILE_BEGIN)
            != BAD_POS || get_system_error() == ok)
        {
            if (SetEndOfFile(fd)) {
                return ok;
            }
        }
        return get_system_error();
    } else {
        return not_opened;
    }
}

file::iop_status os_file::flush()
{
    if (opened) { 
        return FlushFileBuffers(fd) ? ok : get_system_error();
    } else {
        return not_opened;
    }
}

void os_file::get_error_text(iop_status code, char* buf, size_t buf_size) const
{
    int len;
    switch (code) { 
      case ok:
        strncpy(buf, "Ok", buf_size);
        break;
      case not_opened:
        strncpy(buf, "file not opened", buf_size);
        break;
      case end_of_file:
        strncpy(buf, "operation not completly finished", buf_size);
        break;
      default:
        len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                             NULL,
                             code,
                             0,
                             buf,
                             DWORD(buf_size),
                             NULL);
        if (len == 0) { 
            char errcode[64];
            sprintf(errcode, "unknown error code %u", code);
            strncpy(buf, errcode, buf_size);
        }
    }
}

size_t os_file::get_disk_block_size()
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwPageSize;
}

void* os_file::allocate_disk_buffer(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

void  os_file::free_disk_buffer(void* buf)
{
    VirtualFree(buf, 0, MEM_RELEASE);
}

os_file::os_file(const char* name)
{
    os_specific = new win_file_specific;
    opened = False;
    this->name = name ? strdup(name) : (char*)0; 
}


os_file::~os_file() 
{ 
    close();
    delete (win_file_specific*)os_specific;
    free(name); 
}

file* os_file::clone()
{
    return NEW os_file(name); 
}

void os_file::dump() 
{
    ((win_file_specific*)os_specific)->dump();
}

bool os_file::create_temp(void)
{
    name = (char *)malloc(_MAX_PATH);
    GetTempPath(_MAX_PATH, name);
    strcat(name, "/");
    strcat(name, "GDS_XXXXXX");
    _mktemp(name);
    if (open(fa_write, fo_create | fo_largefile) != ok) {
        return false;
    }
    return true;
}

file::iop_status os_file::copy_to_temp(const char *iSource, nat4 iBufSize)
{
    os_file src(iSource);

    if(!create_temp() || (src.open(fa_read, fo_largefile) != ok)) {
        return not_opened;
    }

    dnm_buffer  buf;
    fsize_t     dstSize = 0;
    fsize_t     numRead;
    char       *p;
    fsize_t     srcSize;

    if(src.get_size(srcSize)) {
        return not_opened;
    }
    
    p = buf.put(iBufSize);
    while(dstSize < srcSize) {
        numRead = srcSize - dstSize;
        if(numRead > iBufSize)
            numRead = iBufSize;
        if((src.read(dstSize, p, numRead) != ok) || (write(p, numRead) != ok)) {
            return end_of_file;
        }
        dstSize += numRead;
    }

    src.close();
    return close();
}

//
// Mapped on memory file
// 

file::iop_status mmap_file::open(access_mode mode, int flags)
{ 
    iop_status status = os_file::open(mode, flags);
    if (status == ok) { 
        DWORD high_size;
        DWORD size = GetFileSize(fd, &high_size);
        if (size == BAD_POS && (status = get_system_error()) != ok) {
            os_file::close();
            return status;
        }
        //assert(high_size == 0);

		const size_t file_size = cons_nat8(high_size, size);

		mmap_size = (file_size < init_size) ? init_size : file_size;
		md = CreateFileMapping(fd, NULL,
                               win_page_access[mode],
                               nat8_high_part(mmap_size), nat8_low_part(mmap_size), NULL);
        if (md == NULL) { 
            status = get_system_error();
            os_file::close();
            return status;
        }
        mmap_addr = (char*)MapViewOfFile(md, win_map_access[mode], 0, 0, 0);
        if (mmap_addr == NULL) { 
            status = get_system_error();
            os_file::close();
            return status;
        }
		memset(mmap_addr + file_size, 0, mmap_size - file_size);
	}
    return status; 
}

file::iop_status mmap_file::close()
{
    if (opened) { 
        if (!UnmapViewOfFile(mmap_addr) || !CloseHandle(md)) { 
            return get_system_error();
        } 
        return os_file::close();
    }
    return ok;
}

file::iop_status mmap_file::set_size(fsize_t size)
{
    return set_size(size, size);
}

#define MIN_PAGE_SIZE 4096

file::iop_status mmap_file::set_size(fsize_t min_size, fsize_t max_size)
{
    if (opened) { 
        iop_status status;
        if (min_size > max_size) { 
            max_size = min_size;
        }
        size_t new_min_size = size_t(min_size);
        size_t new_max_size = size_t(max_size);
        assert(new_max_size == max_size); // no truncation 

        if (new_min_size > mmap_size || new_max_size < mmap_size) { 
            if (!UnmapViewOfFile(mmap_addr) || !CloseHandle(md)) { 
                return get_system_error();
            } 
            SYSTEM_INFO sysinfo;
            GetSystemInfo(&sysinfo);
            size_t page_size = sysinfo.dwPageSize;
            new_min_size = DOALIGN(new_min_size, page_size);
            new_max_size = DOALIGN(new_max_size, page_size);

            if (new_max_size > mmap_size
                && sizeof(void*) == 4 && new_max_size >= MMAP_DOUBLE_THRESHOLD                
                && new_min_size + MMAP_EXTEND_QUANTUM < new_max_size)
            {
                new_max_size = new_min_size + MMAP_EXTEND_QUANTUM;
            }
            if (new_max_size < mmap_size) { 
                status = os_file::set_size(new_max_size);
                if (status != ok) { 
                    return status;
                }
            }
            while ((md = CreateFileMapping(fd, NULL, 
                                           win_page_access[mode],
                                           nat8_high_part(new_max_size), nat8_low_part(new_max_size), NULL)) == NULL
                   || (mmap_addr = (char*)MapViewOfFile(md, win_map_access[mode], 
                                                        0, 0, 0)) == NULL)
            {
                if (new_max_size > new_min_size) { 
                    new_max_size = (new_max_size + new_min_size) >> 1;
                } else { 
                    status = get_system_error();
                    os_file::close();
                    return status;
                }
            }
            if (new_max_size > mmap_size) { 
                memset(mmap_addr+mmap_size, 0, new_max_size - mmap_size);
            }
            mmap_size = new_max_size;
        } 
        return ok;
    } else { 
        return not_opened;
    }
}

file::iop_status mmap_file::get_size(fsize_t& size) const
{
    if (opened) { 
        size = mmap_size;
        return ok;
    } else { 
        return not_opened;
    }
}

file::iop_status mmap_file::flush()
{
    if (opened) { 
        return FlushViewOfFile(mmap_addr, mmap_size)
            ? ok : get_system_error();
    } else { 
        return not_opened;
    }   
}

file* mmap_file::clone()
{
    return new mmap_file(name, init_size); 
}


// -- class sequential_buffered_file
//
//[MC] Use sequential_buffered_file when server is opened in maintenance mode. (Shrinking)
//	   To speed up the garbage collection process we open the file using buffering and sequential flags

#include "extrafiles.h"

file::iop_status sequential_buffered_file::open(access_mode mode, int flags)
{
	// -- force to open buffered and secuential
	mode = fa_readwrite;
	flags = 0;

	close();
	assert(name != NULL); 
	fd = CreateFileA(
				name, 
				GENERIC_READ | GENERIC_WRITE, 
				FILE_SHARE_READ, 
                NULL, 
				OPEN_EXISTING,
                FILE_FLAG_SEQUENTIAL_SCAN,
                NULL); 

	this->mode = mode;
	this->flags = 0;

	if (fd == INVALID_HANDLE_VALUE) 
	{
		return get_system_error();
	} 
	else 
	{ 
		opened = True;
		static_cast<win_file_specific*>(os_specific)->reset((flags & fo_sync) != 0);
		return ok;
	}
}


END_GOODS_NAMESPACE
