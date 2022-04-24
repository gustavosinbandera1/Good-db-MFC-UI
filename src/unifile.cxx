// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< UNIFILE.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 17-Feb-99    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// File implementation for Unix
//-------------------------------------------------------------------*--------*

#if defined(__sun) || defined(__SVR4)
#define mutex system_mutex
#endif 

#define _LARGEFILE64_SOURCE 1 // access to files greater than 2Gb in Solaris
#define _LARGE_FILE_API     1 // access to files greater than 2Gb in AIX

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/shm.h>
#if defined(COOPERATIVE_MULTITASKING) && defined(MERGE_IO_REQUESTS)
#include <sys/socket.h>
#endif

#undef mutex

#include "stdinc.h"
#include "osfile.h"
#include "mmapfile.h"
#include "support.h"

static int unix_access_mode[] = { O_RDONLY, O_WRONLY, O_RDWR };

#ifndef O_SYNC
#define O_SYNC  O_FSYNC
#endif

#ifndef O_DSYNC
#define O_DSYNC O_SYNC
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#define MMAP_EXTEND_QUANTUM     64*1024*1024      /* 64Mb */
#define MMAP_DOUBLE_THRESHOLD 1024*1024*1024      /* 1Gb */


const size_t MAX_CHUNK_SIZE = 0x10000000;

static int unix_open_flags[] = { 
    0, O_TRUNC, O_CREAT, O_CREAT|O_TRUNC, 
    O_DSYNC, O_DSYNC|O_TRUNC, O_DSYNC|O_CREAT, O_DSYNC|O_CREAT|O_TRUNC, 
};

//
// Use pread/pwrite functions to perform atomic positioning and read/write
// operations with file
//
#if defined(__sun) || defined(__SVR4) || defined(_AIX43)
#define CONCURRENT_IO 1
#define getpagesize() sysconf(_SC_PAGESIZE)
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__CYGWIN__)
#define CONCURRENT_IO 1
#define NO_LARGEFILE64_FUNCTIONS_DEFINED
#endif

#ifdef NO_LARGEFILE64_FUNCTIONS_DEFINED
#define lseek64(fd, pos, whence) lseek(fd, pos, whence)
#define pread64(fd, buf, size, pos) pread(fd, buf, size, pos);
#define pwrite64(fd, buf, size, pos) pwrite(fd, buf, size, pos)
#define stat64 stat
#define fstat64(fd, fs) fstat(fd, fs)
#define ftruncate64(fd, size) ftruncate(fd, size)
#endif

//
// Merging synchronous write requests can significuntly increase system 
// perofrmance. The main idea is to merge all synchronous write requests 
// issued while previous syncronous write request is not completed.
// This can be done by copying them to the buffer or creating vector
// for writev (if supported). Two buffers (or vectors) are used in 
// cyclic way: while one is used for write system call another is 
// filled with new requests. 
//
#define MERGE_IO_REQUESTS 1


//
// At some operating systems it is possible to obtain performance improvement
// by alignment write operation position and data size to operating system 
// block size. This is definitly True for Digital Unix 4.0 (UFS) and 
// False for Linux. Don't know about other operating systems.
//
//#define SYNC_WRITE_ALIGNED_BLOCKS 1

#if defined(COOPERATIVE_MULTITASKING) 
//
// Perform synchronous write operations in parallel by means of 
// separate write process.
//
#if defined(MERGE_IO_REQUESTS)

#include "unisock.h"

BEGIN_GOODS_NAMESPACE

#define SHMEM_BUF_SIZE (64*1024)

class unix_file_specific { 
  protected: 
    semaphore    writer_sem;  // sempahore to wait completion of write opration
    unix_socket* writer_sock; // socket to receive write completion replies
    int          writer_pipe; // pipe end to send requests to writer

    boolean      writer_started; // writer process forked
    boolean      busy;        // buffer is occupier with long request
    boolean      wait_flag;   // waiting queue is not empty
    event        busy_event;  // event to wait until buffer is availbale

    struct shmem_buf { 
        char data[SHMEM_BUF_SIZE];
        int  get_pos; // position in buffer to write data in file
        int  put_pos; // position in buffer to transfer data to writer process
        int  result;  // operation completion code
    }; 
    shmem_buf* buf;
    int        id;
    int        put_pos;
    int        get_pos;
    int        rc[2]; // result of operation
    event      e[2];  // cyclic buffer for signaling operation completion
    int        ei;    // index in cyclic buffer
    boolean    ready[2]; // previous operation completion flag

    int        n_writes;        // total number of syncronous writes
    int        n_write_reqs;    // total number of write requests
    int        n_par_writes;    // current number of parallel write operations
    int        max_par_writes;  // maximal number of parallel write operations
    
  public: 
    void start_writer(int fd) { 
        int cmd_pipe[2], reply_pipe[2];
        nat1 cmd;

        pipe(cmd_pipe);
        pipe(reply_pipe);
        id = shmget(IPC_PRIVATE, sizeof(shmem_buf), IPC_CREAT|0777);
        if (id < 0) { 
            console::error("Failed to create shared memory region\n");
        }
        if (fork() == 0) { 
            ::close(cmd_pipe[1]);
            ::close(reply_pipe[0]);
            shmem_buf* buf = (shmem_buf*)shmat(id, NULL, 0);
            if (buf == (shmem_buf*)-1) { 
                console::error("Failed to attach shared memory region\n");
            }
            while (read(cmd_pipe[0], &cmd, sizeof cmd) == sizeof cmd) {
                int size = 0;
                int rc;
                if (buf->get_pos < buf->put_pos) { 
                    size = buf->put_pos - buf->get_pos;
                    rc = ::write(fd, buf->data + buf->get_pos, size);
                } else { 
                    size = sizeof(buf->data) - buf->get_pos;
                    if (buf->put_pos == 0) { 
                        rc = ::write(fd, buf->data + buf->get_pos, size);
                    } else {
                        iovec iov[2];
                        iov[0].iov_base = buf->data + buf->get_pos;
                        iov[0].iov_len = size;
                        iov[1].iov_base = buf->data;
                        iov[1].iov_len = buf->put_pos;
                        size += buf->put_pos;
                        rc = writev(fd, iov, 2);
                    }
                }
                buf->result = (rc < 0) ? errno : (rc == size) 
                    ? (int)file::ok : (int)file::end_of_file;
                ::write(reply_pipe[1], &cmd, sizeof cmd);
            }
            shmdt((char*)buf);
            TRACE_MSG((msg_important, "Writer process terminated\n"));
            exit(0);
        } else {
            ::close(cmd_pipe[0]);
            ::close(reply_pipe[1]);
            buf = (shmem_buf*)shmat(id, NULL, 0);
            if (buf == (shmem_buf*)-1) { 
                console::error("Failed to attach shared memory region\n");
            }
            sleep(1); // give the child time to attach before removing region
            shmctl(id, IPC_RMID, NULL); // will be removed after detach
            writer_sock = new unix_socket(reply_pipe[0]);
            writer_pipe = cmd_pipe[1];
            writer_started = True;
        }
    }

    int write(int fd, const void* ptr, size_t size) {  
        nat1 cmd;
        char* src = (char*)ptr;

        if (size == 0) { 
            return file::ok;
        }
        if (!writer_started) { 
            start_writer(fd);
        }
        n_write_reqs += 1;
        while (busy) { 
            wait_flag = True;
            busy_event.reset();
            busy_event.wait();
        } 
        boolean wakeup = False;

        while (size > 0) { 
            int i = ei;
            int get_pos = this->get_pos;
            int put_pos = this->put_pos;
            size_t available = (get_pos <= put_pos) 
                ? SHMEM_BUF_SIZE - put_pos - (get_pos == 0)
                : get_pos - put_pos - 1;
            if (size > available) { 
                this->put_pos = put_pos + available;
                busy = True;
                memcpy(buf->data + put_pos, src, available);
                src += available;
                size -= available;
            } else { 
                this->put_pos = put_pos + size;
                memcpy(buf->data + put_pos, src, size);
                size = 0;
                wakeup = busy;
            }
            if (this->put_pos == SHMEM_BUF_SIZE) { 
                this->put_pos = 0;
            }
            if (n_par_writes++ == 0) {
                ready[i] = False;
                e[i].reset();
                if (wakeup) { 
                    busy = False;
                    if (wait_flag) { 
                        wait_flag = False;
                        busy_event.signal();
                    }
                }
                if (!ready[1-i]) { 
                    e[1-i].wait(); // wait completion of previous write 
                    internal_assert(ready[1-i]);
                }
                ei = 1 - i;
                n_writes += 1;
                if (n_par_writes > max_par_writes) { 
                    max_par_writes = n_par_writes;
                }
                n_par_writes = 0;

                // Initiate write operation
                buf->get_pos = put_pos;
                buf->put_pos = get_pos = this->put_pos;
                ::write(writer_pipe, &cmd, sizeof cmd);
                writer_sock->wait_input();
                writer_sock->read(&cmd, sizeof cmd);
                this->get_pos = get_pos;
                rc[i] = buf->result;
                ready[i] = True;
                e[i].signal();
            } else { 
                e[i].wait();
            }
            if (rc[i] != file::ok) { 
                return rc[i];
            }
        }
        return file::ok;
    }

    unix_file_specific() {
        writer_started = False;
        max_par_writes = 0;
        n_writes = 0;
        n_write_reqs = 0;
    }
    void reset() { 
        n_par_writes = 0;
        ei = 0;
        ready[1] = True;
        put_pos = get_pos = 0;
        busy = False;
        wait_flag = False;
    }
    void close() { 
        if (writer_started) { 
            delete writer_sock;
            ::close(writer_pipe);
            shmdt((char*)buf);
            shmctl(id, IPC_RMID, NULL); // removed after detach
            writer_started = False;
        }
    }

    void lock() {}
    void unlock() {}

    off_t seek(int fd, off_t pos) { return lseek(fd, pos, SEEK_SET); }

    void dump() { 
        if (n_writes > 0) { 
            console::output("Number of synchronous writes: %d\n"
                            "Average number of parallel requests: %6.3lf\n"
                            "Maximal number of parallel requests: %d\n", 
                            n_writes, 
                            double(n_write_reqs)/n_writes, 
                            max_par_writes);
        }
    }
};

#else

BEGIN_GOODS_NAMESPACE

class unix_file_specific { 
  public: 
    void lock() {}
    void unlock() {}

    void dump() {}
};

#endif

#else // Implementation of file for preemptive multitasking

BEGIN_GOODS_NAMESPACE

#if defined(MERGE_IO_REQUESTS)

#ifndef UIO_MAXIOV 
#define UIO_MAXIOV 64
#endif

class unix_file_specific { 
  protected: 
    mutex  cs;         // file critical section

    int    ready[2];   // write completion indicators 
    int    ei;         // index in event array 
    event  e[2];       // cyclic buffer for signaling write completion
    int    vi;         // index in vector array
    int    bi;         // index of used block
    off_t  pos;        // position in file
    char*  block[2];   // file blocks
    char*  zeros;      // block with zeros
    int    block_size; // file block size
    int    rc[2];      // write operation completion code
    
    int    n_writes;        // total number of syncronous writes
    int    n_merged_writes; // total number of merged writes
    int    n_par_writes;    // current number of parallel write operations
    int    max_par_writes;  // maximal number of parallel write operations
    
    typedef iovec iobuf[2][UIO_MAXIOV];
    iobuf iov;        // vectors of segments for writev

  public: 
    unix_file_specific() {
        block_size = getpagesize();
        block[0] = (char*)valloc(block_size*3);
        block[1] = block[0] + block_size;
        zeros = block[1] + block_size;
        memset(zeros, 0, block_size);
        max_par_writes = 0;
        n_writes = 0;
        n_merged_writes = 0;
    }
    void reset() { 
        ready[0] = True;
        ei = 0;
        vi = 0;
        bi = 0;
        pos = 0;
    }
    void close() {}
    ~unix_file_specific() { 
        free(block[0]);
    }

    off_t seek(int fd, off_t pos) { 
        int offs = int(pos) & (block_size-1); 
        off_t rc = lseek(fd, pos - offs, SEEK_SET); 
        reset();
        memset(block[0] + offs, 0, block_size - offs); 
        if (offs != 0 && rc == pos - offs) { 
            rc = read(fd, block[0], offs);
            if (rc == offs) { 
                rc = pos;
            }
        }
        return rc;
    }

    int write(int fd, const void* buf, int size) {  
        int offs;
        if (size == 0) { 
            return file::ok;
        }
        cs.enter();
        while (size_t(vi) >= UIO_MAXIOV-1) { 
            cs.leave();
            e[ei].wait();
            cs.enter();
        }           
        int i = ei;
        char* bp = block[bi];
        if (vi == 0) { 
            i ^= 1;
#if defined(SYNC_WRITE_ALIGNED_BLOCKS)
            offs = pos & (block_size-1); 
            if (offs + size <= block_size) { 
                iov[i][0].iov_base = bp;
                iov[i][0].iov_len = offs + size;
                memcpy(bp + offs, buf, size);
            } else { 
                if (offs != 0) { 
                    iov[i][0].iov_base = bp;
                    iov[i][0].iov_len = offs;
                    iov[i][1].iov_base = (char*)buf;
                    iov[i][1].iov_len = size;
                    vi += 1;
                } else { 
                    iov[i][0].iov_base = (char*)buf;
                    iov[i][0].iov_len = size;
                }
            }
            pos += size;
#else
            iov[i][0].iov_base = (char*)buf;
            iov[i][0].iov_len = size;
#endif
            vi += 1;
            ei = i;
            ready[i] = False;
            e[i].reset();
            n_par_writes = 0;
            n_writes += 1;

            if (!ready[1-i]) { 
                cs.leave();
                e[1-i].wait();
                cs.enter();
                internal_assert(ready[1-i]);
            }
            int count = vi; 
            vi = 0;
            internal_assert(ei == i);
#if defined(SYNC_WRITE_ALIGNED_BLOCKS)
            offs = pos & (block_size - 1);
            if (iov[i][count-1].iov_base != bp) {
                bi ^= 1;
                bp = block[bi];
                memset(bp + offs, 0, block_size - offs); 
                if (offs != 0) { 
                    int len, j = count, tail = offs;
                    while ((len = iov[i][--j].iov_len) < tail) { 
                        tail -= len;
                        memcpy(bp + tail, iov[i][j].iov_base, len);
                    } 
                    iov[i][count].iov_base = zeros;
                    iov[i][count].iov_len = block_size - offs; 
                    count += 1;
                } 
            } else if (offs != 0) { 
                iov[i][count-1].iov_len += block_size - offs; 
            }
#else
            offs = 0;
#endif
            if (n_par_writes > max_par_writes) { 
                max_par_writes = n_par_writes;
            }
            n_merged_writes += n_par_writes;
            ssize_t total_size = 0;
            for (int j = count; --j >= 0; total_size += iov[i][j].iov_len);
            cs.leave();
            int result = writev(fd, iov[i], count); 
            if (result >= 0 && offs != 0) { 
                lseek(fd, -block_size, SEEK_CUR);
            }
            cs.enter();
            if (result < 0) { 
                rc[i] = errno;
            } else { 
                rc[i] = (total_size == result) ? file::ok : file::end_of_file;
            }
            ready[i] = True;
            e[i].signal();
            cs.leave();
            return rc[i];
        } else {
            if ((unsigned long)((char*)iov[i][vi-1].iov_base
                                + iov[i][vi-1].iov_len + size - block[bi]) 
                <= (unsigned long)block_size)
            {
                internal_assert(vi == 1);
                memcpy((char*)iov[i][0].iov_base+iov[i][0].iov_len, buf, size);
                iov[i][0].iov_len += size; 
            } else {
                iov[i][vi].iov_base = (char*)buf;
                iov[i][vi].iov_len = size;
                vi += 1;
            } 
            n_par_writes += 1;
            pos += size;
            internal_assert (!ready[i]); 
            cs.leave();
            e[i].wait();
            return rc[i];
        }
    }
    
    void dump() { 
        if (n_writes > 0) { 
            console::output("Number of synchronous writes: %d\n"
                            "Average number of parallel requests: %6.3lf\n"
                            "Maximal number of parallel requests: %d\n", 
                            n_writes, 
                            double(n_writes + n_merged_writes)/n_writes, 
                            max_par_writes+1);
        }
    }
    void  lock() { cs.enter(); }
    void  unlock() { cs.leave(); }
};

#else // not MERGE_IO_REQUESTS

class unix_file_specific { 
  protected: 
    mutex  cs;         // file critical section
  public:
    void  lock() { cs.enter(); }
    void  unlock() { cs.leave(); }

    void  dump() {}
};

#endif
#endif

file::iop_status os_file::set_position(fposi_t pos)
{
    if (opened) { 
	int8 rc;
#ifdef MERGE_IO_REQUESTS
        if (flags & fo_sync) { 
	    off_t off = (off_t)pos;
	    assert(fposi_t(off) == pos); // position was not truncated
            rc = ((unix_file_specific*)os_specific)->seek(fd, off);
        } else { 
            rc = lseek64(fd, pos, SEEK_SET);
        }
#else
        rc = lseek64(fd, pos, SEEK_SET);
#endif
        if (rc < 0) {  
            return iop_status(errno);
        } else if ((fposi_t)rc != pos) { 
            return end_of_file;
        } 
        return ok;
    }
    return not_opened; 
} 

file::iop_status os_file::get_position(fposi_t& pos)
{
    if (opened) { 
        int8 rc = lseek64(fd, 0, SEEK_CUR);
        if (rc < 0) {  
            return iop_status(errno);
        } else { 
            pos = rc;
            return ok;
        } 
    }
    return not_opened;     
}



file::iop_status os_file::read(void* buf, size_t size)
{  
    if (!opened) { 
        return not_opened;
    }
    char* dst = (char*)buf;
    while (size > 0) {
        size_t chunk = (size < MAX_CHUNK_SIZE) ? size : MAX_CHUNK_SIZE;
        ssize_t rc = ::read(fd, dst, chunk);
        if (rc < 0) { 
            return iop_status(errno);
        } else if (size_t(rc) != chunk) { 
            return end_of_file;
        } else { 
            dst += rc;
            size -= rc;
        }
    }
    return ok;
}

file::iop_status os_file::write(void const* buf, size_t size)
{  
    if (!opened) { 
        return not_opened;
    }
#ifdef MERGE_IO_REQUESTS
    if (flags & fo_sync) { 
        return iop_status(((unix_file_specific*)os_specific)->write(fd, buf, size));
    }
#endif
    char const* src = (char const*)buf;
    while (size > 0) {
        size_t chunk = (size < MAX_CHUNK_SIZE) ? size : MAX_CHUNK_SIZE;
        ssize_t rc = ::write(fd, src, chunk);
        if (rc < 0) { 
            return iop_status(errno);
        } else if (size_t(rc) != chunk) { 
            return end_of_file;
        } else { 
            src += rc;
            size -= rc;
        }
    }
    return ok;
}

file::iop_status os_file::read(fposi_t pos, void* buf, size_t size) 
{
    if (opened) { 
#ifdef CONCURRENT_IO
        ssize_t rc = pread64(fd, buf, size, pos);
        if (rc < 0) { 
            return iop_status(errno);
        } else if (size_t(rc) != size) { 
            return end_of_file;
        } else { 
            return ok;
        }
#else
        ((unix_file_specific*)os_specific)->lock();
        int8 rc = lseek64(fd, pos, SEEK_SET);
        if (rc < 0) {  
            ((unix_file_specific*)os_specific)->unlock();
            return iop_status(errno);
        } else if ((fposi_t)rc != pos) { 
            ((unix_file_specific*)os_specific)->unlock();
            return end_of_file;
        } 
        rc = ::read(fd, buf, size);
        ((unix_file_specific*)os_specific)->unlock();
        if (rc < 0) { 
            return iop_status(errno);
        } else if (size_t(rc) != size) { 
            return end_of_file;
        } else { 
            return ok;
        } 
#endif
    } else { 
        return not_opened;
    }
}

file::iop_status os_file::write(fposi_t pos, void const* buf, size_t size) 
{
    if (opened) { 
#ifdef CONCURRENT_IO
        ssize_t rc = pwrite64(fd, buf, size, pos);
        if (rc < 0) { 
            return iop_status(errno);
        } else if (size_t(rc) != size) { 
            return end_of_file;
        } else { 
            return ok;
        }
#else
        ((unix_file_specific*)os_specific)->lock();
        int8 lrc = lseek64(fd, pos, SEEK_SET);
        if (lrc < 0) {  
            ((unix_file_specific*)os_specific)->unlock();
            return iop_status(errno);
        } else if ((fposi_t)lrc != pos) { 
            ((unix_file_specific*)os_specific)->unlock();
            return end_of_file;
        } 
        int rc = ::write(fd, buf, size);
        ((unix_file_specific*)os_specific)->unlock();
        if (rc < 0) { 
            return iop_status(errno);
        } else if (size_t(rc) != size) { 
            return end_of_file;
        } else { 
            return ok;
        } 
#endif
    } else { 
        return not_opened;
    }
}

file::iop_status os_file::close()
{
    if (opened) { 
        opened = False;
#ifdef MERGE_IO_REQUESTS
        ((unix_file_specific*)os_specific)->close();
#endif
        if (::close(fd) != 0) { 
            return iop_status(errno);
        } 
    } 
    return ok;
}



file::iop_status os_file::open(access_mode mode, int flags) 
{
    close();
    assert(name != NULL); 
    opened = False;
    this->mode = mode;
    this->flags = flags;
    int attr = unix_access_mode[mode] |
	unix_open_flags[flags & (fo_truncate|fo_sync|fo_create)];
#if defined(_AIX43)
    if (flags & fo_directio) { 
	attr |= O_DIRECT;
    }
#endif
    if (flags & fo_largefile) { 
	attr |= O_LARGEFILE;
    }       
    fd = ::open(name, attr, 0666);
    if (fd < 0) {
        return iop_status(errno);
    }
#if defined(__sun)
    if (flags & fo_directio) { 
	directio(fd, DIRECTIO_ON);
    }
#endif	
    opened = True;
#ifdef MERGE_IO_REQUESTS
    ((unix_file_specific*)os_specific)->reset();
#endif
    //
    // Prevent concurrent access to file
    //
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLK, &fl) < 0 && (errno==EACCES || errno==EAGAIN)) { 
	return lock_error;
    }
    return ok;
}

file::iop_status os_file::remove()
{
    close();
    if (unlink(name) < 0) { 
        return iop_status(errno);
    } else { 
        return ok;
    }
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
                    if (rename(name, new_name) == 0) { 
                        free(name);
                        name = strdup(new_name);
                    } else {
                        return iop_status(errno);
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
        struct stat64 fs; 
        if (fstat64(fd, &fs) == 0) { 
            if (!S_ISREG(fs.st_mode)) {
                return end_of_file;
            }
            size = fs.st_size;
            return ok;
        } else { 
            return iop_status(errno);
        }
    } else {
        return not_opened;
    }
}

file::iop_status os_file::set_size(fsize_t size)
{
    if (opened) { 
#ifdef MERGE_IO_REQUESTS
        ((unix_file_specific*)os_specific)->reset();
#endif
        if (ftruncate64(fd, size) == 0) { 
            return ok;
        } else { 
            return iop_status(errno);
        }
    } else {
        return not_opened;
    }
}

file::iop_status os_file::flush()
{
    if (opened) { 
        if (fsync(fd) != 0) { 
            return iop_status(errno);
        }
        return ok;
    } else {
        return not_opened;
    }
}

void os_file::get_error_text(iop_status code, char* buf, size_t buf_size) const
{
    char* msg; 
    switch (code) { 
      case ok:
        msg = "Ok";
        break;
      case not_opened:
        msg = "file not opened";
        break;
      case end_of_file:
        msg = "operation not completly finished";
        break;
      case lock_error:
        msg = "file is used by another program";
        break;
      default:
        msg = strerror(code);
    }
    strncpy(buf, msg, buf_size);
}


size_t os_file::get_disk_block_size()
{
    return getpagesize();
}

void* os_file::allocate_disk_buffer(size_t size)
{
#if defined(__SYMBIAN_OS__) || defined(__MINGW32__) || defined(__CYGWIN__) || (defined(__QNX__) && !defined(__QNXNTO__))
    return malloc(size);
#else
    return valloc(size);
#endif
}

void  os_file::free_disk_buffer(void* buf)
{
    free(buf); 
}

os_file::os_file(const char* name)
{
    opened = False;
    this->name = name ? strdup(name) : (char*)0; 
    this->os_specific = new unix_file_specific;
}


os_file::~os_file() 
{ 
    close();
    free(name); 
    delete (unix_file_specific*)os_specific;
}

file* os_file::clone()
{
    return new os_file(name); 
}

void os_file::dump() 
{
    ((unix_file_specific*)os_specific)->dump();
}

bool os_file::create_temp(void)
{
    close();
    if(name != NULL) {
        free(name);
    }
    name = strdup("/tmp/GDS_XXXXXX");
    if((fd = mkstemp(name)) < 0) {
        return false;
    }
    opened = true;
    flags = fo_largefile;

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

#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

#ifndef MAP_FILE
#define MAP_FILE (0)
#endif


file::iop_status mmap_file::open(access_mode mode, int flags) 
{ 
    iop_status status = os_file::open(mode, flags);
    if (status == ok) { 
        size_t size = lseek(fd, 0, SEEK_END); 
        if (size < init_size) {
            size = init_size; 
            if (ftruncate(fd, init_size) != ok) {
                status = iop_status(errno);
                os_file::close();
                return status;
            }
        }
        if (lseek(fd, 0, SEEK_SET) != 0) { 
            status = iop_status(errno);
            os_file::close();
            return status;
        }
        mmap_addr = (char*)mmap(NULL, size, 
                                mode == fa_read ? PROT_READ :  
                                mode == fa_write ? PROT_WRITE :  
                                PROT_READ|PROT_WRITE, 
                                MAP_FILE|MAP_SHARED, 
                                fd, 0);
        if (mmap_addr != (char*)MAP_FAILED) { 
            mmap_size = size;
            return ok;
        }
        mmap_addr = NULL; 
        status = iop_status(errno);
        os_file::close();
    }
    return status; 
}

file::iop_status mmap_file::close()
{
    if (opened) { 
        if (munmap(mmap_addr, mmap_size) != 0) { 
            return iop_status(errno);
        } 
        return os_file::close(); 
    }
    return ok;
}

file::iop_status mmap_file::set_size(fsize_t size)
{
    return set_size(size, size);
}

file::iop_status mmap_file::set_size(fsize_t min_size, fsize_t max_size)
{
    if (opened) { 
        if (min_size > max_size) { 
            max_size = min_size;
        }
        size_t new_min_size = size_t(min_size);
        size_t new_max_size = size_t(max_size);
        assert(new_max_size == max_size); // no truncation 

        if (new_max_size > mmap_size
            && sizeof(void*) == 4 && new_max_size >= MMAP_DOUBLE_THRESHOLD                
            && new_min_size + MMAP_EXTEND_QUANTUM < new_max_size)
        {
            new_max_size = new_min_size + MMAP_EXTEND_QUANTUM;
        }
        

        if (new_min_size > mmap_size || new_max_size < mmap_size) { 
            void* new_addr = NULL; 
            if (munmap(mmap_addr, mmap_size) != ok) { 
                return iop_status(errno);
            }
            int page_size = getpagesize();
            new_min_size = DOALIGN(new_min_size, page_size);
            new_max_size = DOALIGN(new_max_size, page_size);
            while (ftruncate(fd, new_max_size) != ok
                   || (new_addr = (char*)mmap(NULL, new_max_size, 
                                              mode == fa_read ? PROT_READ :  
                                              mode == fa_write ? PROT_WRITE :  
                                              PROT_READ|PROT_WRITE, 
                                              MAP_FILE|MAP_SHARED, 
                                              fd, 0)) == (char*)MAP_FAILED)
            {
                if (new_max_size > new_min_size) { 
                    new_max_size = (new_max_size + new_min_size) >> 1;
                } else { 
                    return iop_status(errno);
                }
            }
            mmap_addr = (char*)new_addr; 
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
#ifdef MS_SYNC
        if (msync(mmap_addr, mmap_size, MS_SYNC) == 0) { 
#else
        if (msync(mmap_addr, mmap_size) == 0) { 
#endif
            return ok;
        } else { 
            return iop_status(errno);
        }
    } else { 
        return not_opened;
    }   
}


file* mmap_file::clone()
{
    return new mmap_file(name, init_size); 
}


END_GOODS_NAMESPACE
