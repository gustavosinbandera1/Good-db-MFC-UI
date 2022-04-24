// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< UNISOCK.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      8-Feb-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 18-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Unix sockets  
//-------------------------------------------------------------------*--------*

#if defined(__sun) || defined(__SVR4)
#define mutex system_mutex
#endif 

#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

extern "C" {
#include <netdb.h>
}
#undef mutex

#include "stdinc.h"
#include "procsock.h"
#include "unisock.h"

#include <signal.h>

#ifdef COOPERATIVE_MULTITASKING
#include "async.h"
#endif

#if defined(_AIX)
    typedef size_t  socklen_t;
#elif !defined(__linux__) && !defined(__sun) && !defined(__svr4__) && !(defined(__FreeBSD__) && __FreeBSD__ > 3)
    typedef int     socklen_t;
#endif


BEGIN_GOODS_NAMESPACE

#define MAX_HOST_NAME     256

char* unix_socket::unix_socket_dir = "/tmp/";

class unix_socket_library { 
  public: 
    unix_socket_library() { 
        static struct sigaction sigpipe_ignore; 
        sigpipe_ignore.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &sigpipe_ignore, NULL);
    }
};

static unix_socket_library unisock_lib;

int unix_socket::get_descriptor() 
{
    return (int)fd;
}

boolean unix_socket::open(int listen_queue_size)
{
    char hostname[MAX_HOST_NAME];
    unsigned short port;
    char* p;

    assert(address != NULL);

    if ((p = strchr(address, ':')) == NULL 
        || unsigned(p - address) >= sizeof(hostname) 
        || sscanf(p+1, "%hu", &port) != 1) 
    {
        errcode = bad_address;
        return False;
    }
    memcpy(hostname, address, p - address);
    hostname[p - address] = '\0';
    assert(size_t(p - address) < sizeof(hostname));

    create_file = False; 
    union { 
        sockaddr    sock;
        sockaddr_in sock_inet;
        char        name[MAX_HOST_NAME];
    } u;
    int len;

    if (domain == sock_local_domain) { 
        u.sock.sa_family = AF_UNIX;

        assert(strlen(unix_socket_dir) + strlen(address) 
               < MAX_HOST_NAME - offsetof(sockaddr,sa_data)); 
        
        len = offsetof(sockaddr,sa_data) + 
            sprintf(u.name + offsetof(sockaddr,sa_data), "%s%s", unix_socket_dir, address);

        unlink(u.sock.sa_data); // remove file if existed
        create_file = True; 
    } else {
        u.sock_inet.sin_family = AF_INET;
        if (*hostname && stricmp(hostname, "localhost") != 0) {
            struct hostent* hp;  // entry in hosts table
            if ((hp = gethostbyname(hostname)) == NULL 
                || hp->h_addrtype != AF_INET) 
            {
                errcode = bad_address;
                return False;
            }
            memcpy(&u.sock_inet.sin_addr, hp->h_addr, 
                   sizeof u.sock_inet.sin_addr);
        } else {
            u.sock_inet.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        u.sock_inet.sin_port = htons(port);
        len = sizeof(sockaddr_in);        
    } 
    if ((fd = socket(u.sock.sa_family, SOCK_STREAM, 0)) < 0) { 
        errcode = errno;
        return False;
    }
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof on);

    if (bind(fd, &u.sock, len) < 0) {
        errcode = errno;
        ::close(fd);
        return False;
    }
    if (listen(fd, listen_queue_size) < 0) {
        errcode = errno;
        ::close(fd);
        return False;
    }
    errcode = ok;
    state = ss_open;
    return True;
}


char* unix_socket::get_peer_name(nat2* oPort)
{
    if (state != ss_open) { 
        errcode = not_opened;
        return NULL;
    }
    struct sockaddr_in insock;
    socklen_t len = sizeof(insock);
    if (getpeername(fd, (struct sockaddr*)&insock, &len) != 0) { 
        errcode = errno;
        return NULL;
    }
    if (oPort != NULL) {
        *oPort = htons(insock.sin_port);
    }
    char* addr = inet_ntoa(insock.sin_addr);
    if (addr == NULL) { 
        errcode = errno;
        return NULL;
    }
    char* addr_copy = new char[strlen(addr)+1];
    strcpy(addr_copy, addr);
    errcode = ok;
    return addr_copy;
}

boolean  unix_socket::is_ok()
{
    return errcode == ok;
}

void unix_socket::get_error_text(char* buf, size_t buf_size)
{
    char* msg; 
    switch(errcode) { 
      case ok:
        msg = "ok";
        break;
      case not_opened:
        msg = "socket not opened";
        break;
      case bad_address: 
        msg = "bad address";
        break;
      case connection_failed: 
        msg = "exceed limit of attempts of connection to server";
        break;
      case broken_pipe:
        msg = "connection is broken";
        break; 
      case invalid_access_mode:
        msg = "invalid access mode";
        break;
      default: 
        msg = strerror(errcode);
    }
    strncpy(buf, msg, buf_size);
}

#ifdef COOPERATIVE_MULTITASKING

/*****************************************************************************\
wait_input - wait until input is available or the socket is no longer open
-------------------------------------------------------------------------------
This function suspends the calling thread's execution until input is available
for reading on this object's socket, or the socket is no longer open.

Return Value:
 TRUE is returned if the socket is open and input is available for reading
 FALSE is returned if the socket is no longer open
\*****************************************************************************/

boolean unix_socket::wait_input()
{
    async_event_manager::attach_input_channel(this);
    input_sem.wait();
    async_event_manager::detach_input_channel(this);
    return state == ss_open;
}


/*****************************************************************************\
wait_input_with_timeout - wait those seconds or until input is available
-------------------------------------------------------------------------------
This function suspends the calling thread's execution for the specified number
of seconds or until input is available for reading on this object's socket.

Return Value:
 If "check_timeout_expired" is set and input is available, TRUE is returned.
 If "check_timeout_expired" is set and the timeout occurred, FALSE is returned.
 If "check_timeout_expired" is clear and the socket is open, TRUE is returned.
 If "check_timeout_expired" is clear and the socket is dead, FALSE is returned.

Arguments:
 timeout                specifies the number of seconds this function should
                        wait for input to be available for reading.
 check_timeout_expired  specifies if this function's return value should
                        indicate socket status or input availability; FALSE
                        sets this function's return value to behave like
                        wait_input()
\*****************************************************************************/

boolean unix_socket::wait_input_with_timeout(time_t timeout, 
                                             boolean check_timeout_expired)
{
    async_event_manager::attach_input_channel(this);
    boolean input_is_available = input_sem.wait_with_timeout(timeout);
    async_event_manager::detach_input_channel(this);
    return (check_timeout_expired) ? input_is_available : (state == ss_open);
}

boolean unix_socket::wait_output()
{
    async_event_manager::attach_output_channel(this);
    output_sem.wait();
    async_event_manager::detach_output_channel(this);
    return state == ss_open;
}

boolean unix_socket::wait_output_with_timeout(time_t timeout,
                                              boolean check_timeout_expired)
{
    async_event_manager::attach_output_channel(this);
    boolean timeout_expired = output_sem.wait_with_timeout(timeout);
    async_event_manager::detach_output_channel(this);
    return (check_timeout_expired) ? timeout_expired : (state == ss_open);
}
#endif


/*****************************************************************************\
abort_input - abort the current (or next) input operation
-------------------------------------------------------------------------------
This function will abort the current (or next) input operation being performed
by this object.  This function was designed to provide a thread with the
ability to cause another thread stuck in a read operation to bail.
\*****************************************************************************/

void unix_socket::abort_input()
{
#ifdef COOPERATIVE_MULTITASKING
    input_sem.signal();
#else
 // TODO: implement this functionality for pre-emptive multi-tasking
 //       configuration & for MS-Windows configuration.
#endif
}


socket_t* unix_socket::accept()
{
    int s;

    if (state != ss_open) { 
        errcode = not_opened;
        return NULL;
    }

#ifdef COOPERATIVE_MULTITASKING
    if (!wait_input()) { // may be socket was closed while waiting for input
        errcode = not_opened;
        return NULL;
    }
#endif


    while((s = ::accept(fd, NULL, NULL )) < 0 && errno == EINTR);

    if (s < 0) { 
        errcode = errno;
        return NULL;
    } else if (state != ss_open) {
        errcode = not_opened;
        return NULL;
    } else { 
        static struct linger l = {1, LINGER_TIME};
#ifdef COOPERATIVE_MULTITASKING
        if (fcntl(s, F_SETFL, O_NONBLOCK) != 0) { 
            errcode = invalid_access_mode; 
            ::close(s);
            return NULL; 
        }
#endif
        if (domain == sock_global_domain) { 
            int enabled = 1;
            if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&enabled, 
                           sizeof enabled) != 0)
            {
                errcode = errno;
                ::close(s);        
                return NULL;
            }
        }
        if (setsockopt(s, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof l) != 0) { 
            errcode = invalid_access_mode; 
            ::close(s);
            return NULL; 
        }
        errcode = ok;
        return new unix_socket(s); 
    }
}

boolean unix_socket::cancel_accept() 
{
#ifdef COOPERATIVE_MULTITASKING
    return close();
#else
    // Wakeup listener
    state = ss_shutdown;
    delete socket_t::connect(address, domain, 1, 0);
    return close();
#endif
}    


boolean unix_socket::connect(int max_attempts, time_t timeout,
                             time_t connectTimeout)
{
    int   rc;
    char* p;
    struct utsname local_host;
    char hostname[MAX_HOST_NAME];
    unsigned short port;

    assert(address != NULL);

    if ((p = strchr(address, ':')) == NULL 
        || unsigned(p - address) >= sizeof(hostname) 
        || sscanf(p+1, "%hu", &port) != 1) {
        errcode = bad_address;
        return False;
    }
    memcpy(hostname, address, p - address);
    hostname[p - address] = '\0';
    assert(size_t(p - address) < sizeof(hostname));
    
    create_file = False; 
    uname(&local_host);

    if (domain == sock_local_domain || (domain == sock_any_domain && 
        (stricmp(hostname, local_host.nodename) == 0
         || stricmp(hostname, "localhost") == 0))) {
        // connect UNIX socket
        union { 
            sockaddr sock;
            char     name[MAX_HOST_NAME];
        } u;
        u.sock.sa_family = AF_UNIX;

        assert(strlen(unix_socket_dir) + strlen(address) 
               < MAX_HOST_NAME - offsetof(sockaddr,sa_data)); 
 
        int len = offsetof(sockaddr,sa_data) +
            sprintf(u.name + offsetof(sockaddr,sa_data), "%s%s", unix_socket_dir, address);
        
        while (True) {
            if ((fd = socket(u.sock.sa_family, SOCK_STREAM, 0)) < 0) { 
                errcode = errno;
                return False;
            }
#ifdef COOPERATIVE_MULTITASKING
            if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0) { 
                errcode = invalid_access_mode; 
                ::close(fd);
                return False; 
            }
#endif
            do { 
#ifdef COOPERATIVE_MULTITASKING
                async_event_manager::attach_output_channel(this);
#endif
                rc = ::connect(fd, &u.sock, len);
#ifdef COOPERATIVE_MULTITASKING
                if (rc < 0 && errno == EINPROGRESS)
                {
                    socklen_t errcodeSize = sizeof(errcode);
                    if (connectTimeout == WAIT_FOREVER)
                        output_sem.wait();
                    else if (!output_sem.wait_with_timeout(connectTimeout))
                    {
                        async_event_manager::detach_output_channel(this);
                        errcode = connection_failed;
                        return False;
                    }
                    getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode,
                        &errcodeSize);
                    if (errcode == 0)
                        rc = 0;
                }
                async_event_manager::detach_output_channel(this);
#endif
            } while (rc < 0 && errno == EINTR);
            
            if (rc < 0) { 
                errcode = errno;
                ::close(fd);
                if (errcode == ENOENT || errcode == ECONNREFUSED) {
                    if (--max_attempts > 0) { 
#ifndef COOPERATIVE_MULTITASKING
                        task::sleep(timeout);
#endif
                    } else { 
                        break;
                    }
                } else {
                    return False;
                }
            } else {
                errcode = ok;
                state = ss_open;
                return True;
            }
        }
    } else { 
        sockaddr_in sock_inet;
        struct hostent* hp;  // entry in hosts table

        if ((hp=gethostbyname(hostname)) == NULL || hp->h_addrtype != AF_INET)
        {
            errcode = bad_address;
            return False;
        }
        sock_inet.sin_family = AF_INET;  
        sock_inet.sin_port = htons(port);
        
        while (True) {
            for (int i = 0; hp->h_addr_list[i] != NULL; i++) { 
                memcpy(&sock_inet.sin_addr, hp->h_addr_list[i],
                       sizeof sock_inet.sin_addr);
                if ((fd = socket(sock_inet.sin_family, SOCK_STREAM, 0)) < 0) { 
                    errcode = errno;
                    return False;
                }
#ifdef COOPERATIVE_MULTITASKING
                    if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0) { 
                        errcode = invalid_access_mode; 
                        ::close(fd);
                        return False; 
                    }
#endif
                do { 
#ifdef COOPERATIVE_MULTITASKING
                    async_event_manager::attach_output_channel(this);
#endif
                    rc = ::connect(fd,(sockaddr*)&sock_inet,sizeof(sock_inet));
#ifdef COOPERATIVE_MULTITASKING
                    if (rc < 0 && errno == EINPROGRESS)
                    {
                        socklen_t errcodeSize = sizeof(errcode);
                        if (connectTimeout == WAIT_FOREVER)
                            output_sem.wait();
                        else if (!output_sem.wait_with_timeout(connectTimeout)) {
                            async_event_manager::detach_output_channel(this);
                            errcode = connection_failed;
                            return False;
                        }
                        getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode,
                                   &errcodeSize);
                        if (errcode == 0)
                            rc = 0;
                    }
                    async_event_manager::detach_output_channel(this);
#endif
                } while (rc < 0 && errno == EINTR);
                
                if (rc < 0) { 
                    errcode = errno;
                    ::close(fd);
                    if (errcode != ENOENT && errcode != ECONNREFUSED) {
                        return False;
                    }
                } else {
                    int enabled = 1;
                    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, 
                                   (char*)&enabled, sizeof enabled) != 0)
                    {
                        errcode = errno;
                        ::close(fd);        
                        return False;
                    }
                    errcode = ok;
                    state = ss_open;
                    return True;
                }
            }
            if (--max_attempts > 0) { 
#ifndef COOPERATIVE_MULTITASKING
                task::sleep(timeout);
#endif
            } else { 
                break;
            }
        }
    }
    errcode = connection_failed;
    return False;
}


int unix_socket::read(void* buf, size_t min_size, size_t max_size, 
                      time_t timeout)
{ 
    size_t size = 0;
    time_t end = 0;
    if (state != ss_open) { 
        errcode = not_opened;
        return -1;
    }
    if (timeout != WAIT_FOREVER) { 
        end = time(NULL) + timeout;
    }
    do { 
        ssize_t rc; 
#ifdef COOPERATIVE_MULTITASKING
        if (timeout != WAIT_FOREVER) { 
            if (!wait_input_with_timeout(timeout, true)) { 
                return size;
            }
            time_t now = time(NULL);
            timeout = (now <= end) ? end - now : 0;
        } else { 
            wait_input();
        }
        if (state != ss_open) { 
            errcode = not_opened;
            return -1;
        }
#else
        if (timeout != WAIT_FOREVER) { 
            fd_set events;
            struct timeval tm;
            FD_ZERO(&events);
            FD_SET(fd, &events);
            tm.tv_sec = timeout;
            tm.tv_usec = 0;
            while ((rc = select(fd+1, &events, NULL, NULL, &tm)) < 0 
                   && errno == EINTR);
            if (rc < 0) { 
                errcode = errno;
                return -1;
            }
            if (rc == 0) {
                return size;
            }
            time_t now = time(NULL);
            timeout = (now <= end) ? end - now : 0;
        }
#endif
        while ((rc = ::read(fd, (char*)buf + size, max_size - size)) < 0 
               && errno == EINTR); 
        if (rc < 0) { 
            errcode = errno;
            return -1;
        } else if (rc == 0) {
            errcode = broken_pipe;
            return -1; 
        } else { 
            size += rc; 
        }
    } while (size < min_size); 

    return (int)size;
}
        

boolean unix_socket::read(void* buf, size_t size)
{ 
    if (state != ss_open) { 
        errcode = not_opened;
        return False;
    }

    do { 
        ssize_t rc; 
        while ((rc = ::read(fd, buf, size)) < 0 && errno == EINTR); 
        if (rc < 0) { 
#ifdef COOPERATIVE_MULTITASKING
            if (errno == EWOULDBLOCK) { 
                if (!wait_input()) { 
                    errcode = not_opened;
                    return False;
                }
                continue;
            }
#endif
            errcode = errno;
            return False;
        } else if (rc == 0) {
            errcode = broken_pipe;
            return False; 
        } else { 
            buf = (char*)buf + rc; 
            size -= rc; 
        }
    } while (size != 0); 

    return True;
}
        

boolean unix_socket::write(void const* buf, size_t size)
{ 
    if (state != ss_open) { 
        errcode = not_opened;
        return False;
    }
    
    do { 
        ssize_t rc; 
        while ((rc = ::write(fd, buf, size)) < 0 && errno == EINTR); 
        if (rc < 0) { 
#ifdef COOPERATIVE_MULTITASKING
            if (errno == EWOULDBLOCK) { 
                if (!wait_output()) { 
                    errcode = not_opened;
                    return False;
                }
                continue;
            }
#endif
            errcode = errno;
            return False;
        } else if (rc == 0) {
            errcode = broken_pipe;
            return False; 
        } else { 
            buf = (char*)buf + rc; 
            size -= rc; 
        }
    } while (size != 0); 

    //
    // errcode is not assigned 'ok' value beacuse write function 
    // can be called in parallel with other socket operations, so
    // we want to preserve old error code here.
    //
    return True;
}
        
void unix_socket::clear_error()
{
    errcode = ok;
}

boolean unix_socket::close()
{
    if (state != ss_close) {
        state = ss_close;
#ifdef COOPERATIVE_MULTITASKING
        async_event_manager::detach_input_channel(this);
        async_event_manager::detach_output_channel(this);
        input_sem.signal();
        output_sem.signal();
#endif
        if (::close(fd) == 0) {
            errcode = ok;
            return True;
        } else { 
            errcode = errno;
            return False;
        }
    }
    errcode = ok;
    return True;
}

boolean unix_socket::shutdown()
{
    if (state == ss_open) { 
        state = ss_shutdown;
#ifdef COOPERATIVE_MULTITASKING
        async_event_manager::detach_input_channel(this);
        async_event_manager::detach_output_channel(this);
        input_sem.signal();
        output_sem.signal();
#endif
        int rc = ::shutdown(fd, 2);
        if (rc != 0) { 
            errcode = errno;
            return False;
        } 
    } 
    return True;
}

unix_socket::~unix_socket()
{
    close();
    if (create_file) { 
        char name[MAX_HOST_NAME];
        sprintf(name, "%s%s", unix_socket_dir, address);
        assert(strlen(name) <= sizeof(name));
        unlink(name);
    }
    if (address != NULL) { 
        free(address); 
    }
}

unix_socket::unix_socket(const char* addr, socket_domain domain)
{ 
    address = strdup(addr); 
    this->domain = domain;
    create_file = False;
    errcode = ok;
}

unix_socket::unix_socket(int new_fd) 
{ 
    fd = new_fd; 
    address = NULL; 
    create_file = False;
    state = ss_open; 
    errcode = ok;
}

socket_t* socket_t::create_local(char const* address, int listen_queue_size)
{
    if (address[0] == '!') {
        return new process_socket(address);
    }
    unix_socket* sock = new unix_socket(address, sock_local_domain);
    sock->open(listen_queue_size); 
    return sock;
}

socket_t* socket_t::create_global(char const* address, int listen_queue_size)
{
    unix_socket* sock = new unix_socket(address, sock_global_domain);
    sock->open(listen_queue_size); 
    return sock;
}

socket_t* socket_t::connect(char const* address, 
                            socket_domain domain, 
                            int max_attempts, 
                            time_t timeout,
                            time_t connectTimeout)
{
    if (address[0] == '!') {
        return process_socket::Connect(address, max_attempts, timeout,
                                       connectTimeout);
    }
    unix_socket* sock = new unix_socket(address, domain);
    sock->connect(max_attempts, timeout, connectTimeout); 
    return sock;
}

static process_name_function current_process_name_function = 0;
void set_process_name_function( process_name_function func) {
    current_process_name_function = func;
} 
    
char const* get_process_name() 
{ 
    if( !current_process_name_function ){
    static char name[MAX_HOST_NAME+8];
    struct utsname local_host;
    uname(&local_host);
    sprintf(name, "%s:%d:%p", local_host.nodename, (int)getpid(), task::current());
    return name;
    } else {
        return current_process_name_function();
    }
}

END_GOODS_NAMESPACE
