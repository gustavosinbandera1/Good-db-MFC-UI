// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< W32SOCK.CXX >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      8-May-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 19-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Windows sockets  
//-------------------------------------------------------------------*--------*

#include "stdinc.h"
#ifndef __MINGW32__
#pragma hdrstop
#endif
#include "procsock.h"
#include "w32sock.h"
#include <algorithm>

BEGIN_GOODS_NAMESPACE

#define MAX_HOST_NAME         256
#define MILLISECOND           1000

// Winsock2 has a very elusive but(t) ugly BUG that this constant works around.
// Specifically, if you ask recv() to fill a buffer > 1MB, you can frequently
// receive -1 from recv() with WSAGetLastError() returning "WSAENOBUFS".
// Restricting the number of bytes you request from recv() increases the number
// of calls to recv(), but it prevents this problem from happening.
static const int kMaxRecvSize = 100 * 1024;

static HANDLE WatchDogMutex;

// Use local implementation of windows sockets only when address is specified as "localhost"
//[MC] We are not using the local Windows socket implementation
//#define RESTRICTED_USE_OF_LOCAL_SOCKETS 1

#if (defined(_MSC_VER) && _MSC_VER < 1300) || defined(__GNUC__)
#define MEMORY_BARRIER_NOT_DEFINED true
#endif 

#ifdef MEMORY_BARRIER_NOT_DEFINED

#ifdef __GNUC__

#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#define MemoryBarrier() __sync_synchronize()
#else
inline void MemoryBarrier() { 
    asm volatile("mfence":::"memory");
}
#endif

#else

#if defined(_M_AMD64)
#define MemoryBarrier __faststorefence
#elif defined(_M_IA64)
#define MemoryBarrier __mf
#elif defined(_M_IX86) || defined(_M_X64)
inline void MemoryBarrier() {
    LONG Barrier;
    __asm {
        xchg Barrier, eax
    }
}
#else
static mutex barrierMutex;
inline void MemoryBarrier() { critical_section cs(barrierMutex); }
#endif
#endif
#endif

#undef max
#undef min

class win_socket_library { 
  public:
    SYSTEM_INFO sinfo;
    
    win_socket_library() 
    { 
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(1, 1), &wsa) != 0) {
            fprintf(stderr,"Failed to initialize windows sockets: %d\n",
                    WSAGetLastError());
        }
        //
        // This mutex is used to recognize process termination
        //
        WatchDogMutex = CreateMutex(NULL, TRUE, NULL);
#ifdef DEBUG_LOCAL_SOCKETS
        fprintf(stderr, "!!!! Create watchdog mutex %p in thread %p\n", WatchDogMutex, task::current());
#endif
        GetSystemInfo(&sinfo);  
    }

    void SetWatchDogThread()
    {
        CloseHandle(WatchDogMutex);
        WatchDogMutex = CreateMutex(NULL, TRUE, NULL);
    }

    ~win_socket_library() {
        WSACleanup();
    }
};

static win_socket_library ws32_lib;
#ifdef SET_SDDL_DACL
#define _WIN32_WINNT 0x0500
#include <sddl.h>

class dbMySecurityDesciptor { 
    bool createMyDACL( )
    {
        // Define the SDDL for the DACL. This example sets 
        // the following access:
        //     Built-in guests are denied all access.
        //     Anonymous logon is denied all access.
        //     Authenticated users are allowed read/write/execute access.
        //     Administrators are allowed full control.
        // Modify these values as needed to generate the proper
        // DACL for your application. 
        TCHAR * szSD = TEXT("D:")       // Discretionary ACL
            TEXT("(D;OICI;GA;;;BG)")     // Deny access to built-in guests
            TEXT("(D;OICI;GA;;;AN)")     // Deny access to anonymous logon
            TEXT("(A;OICI;GRGWGX;;;AU)") // Allow read/write/execute to authenticated users
            TEXT("(A;OICI;GA;;;BA)");    // Allow full control to administrators
        
        return ConvertStringSecurityDescriptorToSecurityDescriptor(
            szSD,
            SDDL_REVISION_1,
            &(sa.lpSecurityDescriptor),
            NULL);
    }

  public:
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES sa; 

    dbMySecurityDesciptor()
    { 
	 sa.nLength = sizeof(SECURITY_ATTRIBUTES);
         sa.bInheritHandle = FALSE;  

         // Call function to set the DACL. The DACL
         // is set in the SECURITY_ATTRIBUTES 
         // lpSecurityDescriptor member.
         createMyDACL();
    }

    ~dbMySecurityDesciptor()
    {
        // Free the memory allocated for the SECURITY_DESCRIPTOR.
        LocalFree(sa.lpSecurityDescriptor);
    }
    
    static dbMySecurityDesciptor instance;
};
dbMySecurityDesciptor dbMySecurityDesciptor::instance;
#define GOODS_SECURITY_ATTRIBUTES &dbMySecurityDesciptor::instance.sa

#elif defined(SET_NULL_DACL)
class dbNullSecurityDesciptor { 
  public:
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES sa; 

    dbNullSecurityDesciptor() { 
	InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE; 
	sa.lpSecurityDescriptor = &sd;
    }
    
    static dbNullSecurityDesciptor instance;
};
dbNullSecurityDesciptor dbNullSecurityDesciptor::instance;
#define GOODS_SECURITY_ATTRIBUTES &dbNullSecurityDesciptor::instance.sa
#else    
#define GOODS_SECURITY_ATTRIBUTES NULL
#endif


void GOODS_DLL_EXPORT set_watchdog_thread()
{
     ws32_lib.SetWatchDogThread(); 
}

int win_socket::get_descriptor() 
{
    return (int)s;
}

boolean win_socket::open(int listen_queue_size)
{
    unsigned short port;
    char* p;
    char hostname[MAX_HOST_NAME];

    assert(address != NULL);

    if ((p = strchr(address, ':')) == NULL 
        || sscanf(p+1, "%hu", &port) != 1) 
    {
        errcode = bad_address;
        return False;
    }
    memcpy(hostname, address, p - address);
    hostname[p - address] = '\0';

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) { 
        errcode = WSAGetLastError();
        return False;
    }
    struct sockaddr_in insock;
    insock.sin_family = AF_INET;
    if (*hostname && stricmp(hostname, "localhost") != 0) {
        struct hostent* hp;  // entry in hosts table
        if ((hp = gethostbyname(hostname)) == NULL 
            || hp->h_addrtype != AF_INET) 
        {
            errcode = bad_address;
            return False;
        }
        memcpy(&insock.sin_addr, hp->h_addr, sizeof insock.sin_addr);
    } else {
        insock.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    insock.sin_port = htons(port);

    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof on);
    
    if (bind(s, (sockaddr*)&insock, sizeof(insock)) != 0) { 
        errcode = WSAGetLastError();
        closesocket(s);
        return False;
    }
    if (listen(s, listen_queue_size) != 0) {
        errcode = WSAGetLastError();
        closesocket(s);
        return False;
    } 
    errcode = ok;
    state = ss_open;
    return True;
}


char* win_socket::get_peer_name(nat2* oPort)
{
    if (state != ss_open) { 
        errcode = not_opened;
        return NULL;
    }
    struct sockaddr_in insock;
    int len = sizeof(insock);
    if (getpeername(s, (struct sockaddr*)&insock, &len) != 0) { 
        errcode = WSAGetLastError();
        return NULL;
    }
    if (oPort != NULL) {
        *oPort = htons(insock.sin_port);
    }    
    char* addr = inet_ntoa(insock.sin_addr);
    if (addr == NULL) { 
        errcode = WSAGetLastError();
        return NULL;
    }
    char* addr_copy = new char[strlen(addr)+1];
    strcpy(addr_copy, addr);
    errcode = ok;
    return addr_copy;
}

boolean win_socket::is_ok()
{
    return errcode == ok;
}

void win_socket::get_error_text(char* buf, size_t buf_size)
{
    int   len;
    const char* msg; 
    char  msgbuf[64];

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
        len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                             NULL,
                             errcode,
                             0,
                             buf,
                             DWORD(buf_size),
                             NULL);
        if (len == 0) { 
            sprintf(msgbuf, "unknown error code %u", errcode);
            msg = msgbuf;
        } else { 
            return;
        }
    }
    strncpy(buf, msg, buf_size);
}

/*****************************************************************************\
abort_input - abort the current (or next) input operation
-------------------------------------------------------------------------------
This function will abort the current (or next) input operation being performed
by this object.  This function was designed to provide a thread with the
ability to cause another thread stuck in a read operation to bail.
\*****************************************************************************/

void win_socket::abort_input(void)
{
     // TODO: implement for MS-Windows.
}


socket_t* win_socket::accept()
{
    if (state != ss_open) { 
        errcode = not_opened;
        return NULL;
    }

    SOCKET new_sock = ::accept(s, NULL, NULL );

    if (new_sock == INVALID_SOCKET) { 
        errcode = WSAGetLastError();
        return NULL;
    } else { 
        static struct linger l = {1, LINGER_TIME};
        if (setsockopt(new_sock, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof l) != 0) { 
            errcode = invalid_access_mode; 
            closesocket(new_sock);
            return NULL; 
        }
        int enabled = 1;
        if (setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&enabled, 
                       sizeof enabled) != 0)
        {
            errcode = WSAGetLastError();
            closesocket(new_sock);      
            return NULL;
        }
        errcode = ok;
        return new win_socket(new_sock); 
    }
}

boolean win_socket::cancel_accept() 
{
    state = ss_shutdown;
    delete socket_t::connect(address, sock_global_domain, 1, 0);
    return close();
}    


boolean win_socket::connect(int max_attempts, time_t timeout, time_t)
{
    char hostname[MAX_HOST_NAME];
    char *p;
    unsigned short port;

    assert(address != NULL);

    if ((p = strchr(address, ':')) == NULL 
        || size_t(p - address) >= sizeof(hostname) 
        || sscanf(p+1, "%hu", &port) != 1) 
    {
        errcode = bad_address;
        return False;
    }
    memcpy(hostname, address, p - address);
    hostname[p - address] = '\0';

    struct sockaddr_in insock;  // inet socket address
    struct hostent*    hp;      // entry in hosts table

    if ((hp = gethostbyname(hostname)) == NULL || hp->h_addrtype != AF_INET) {
        errcode = bad_address;
        return False;
    }
    insock.sin_family = AF_INET;
    insock.sin_port = htons(port);
    
    while (True) {
        for (int i = 0; hp->h_addr_list[i] != NULL; i++) { 
            memcpy(&insock.sin_addr, hp->h_addr_list[i],
                   sizeof insock.sin_addr);
            if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) { 
                errcode = WSAGetLastError();
                return False;
            }
            if (::connect(s, (sockaddr*)&insock, sizeof insock) != 0) { 
                errcode = WSAGetLastError();
                closesocket(s);
                if (errcode != WSAECONNREFUSED) {
                    return False;
                }
            } else {
                int enabled = 1;
                if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&enabled, 
                               sizeof enabled) != 0)
                {
                    errcode = WSAGetLastError();
                    closesocket(s);     
                    return False;
                }
                enabled = 1;
				if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*)&enabled,
                               sizeof enabled) != 0)
                {
                    errcode = WSAGetLastError();
                    closesocket(s);     
                    return False;
                }
                errcode = ok;
                state = ss_open;
                return True;
            }
        }
        if (--max_attempts > 0) {  
            Sleep(timeout*MILLISECOND);
        } else { 
            errcode = connection_failed;
            return False;
        }
    }
}

int win_socket::read(void* buf, size_t min_size, size_t max_size, 
                     time_t timeout)
{ 
    size_t size = 0;
    time_t start = 0;
    if (state != ss_open) { 
        errcode = not_opened;
        return -1;
    }
    if (timeout != WAIT_FOREVER) { 
        start = time(NULL); 
    }

    do { 
        int rc;
        if (timeout != WAIT_FOREVER) { 
            fd_set events;
            struct timeval tm;
            FD_ZERO(&events);
            FD_SET(s, &events);
            tm.tv_sec = timeout;
            tm.tv_usec = 0;
            rc = select(s+1, &events, NULL, NULL, &tm);
            if (rc < 0) { 
                errcode = errno;
                return -1;
            }
            if (rc == 0) {
                return int(size);
            }
            time_t now = time(NULL);
            timeout = start + timeout >= now ? timeout + start - now : 0;  
        }
        int recvSize = int((max_size - size > (size_t)kMaxRecvSize) ? kMaxRecvSize : max_size - size);
        rc = recv(s, (char*)buf + size, recvSize, 0);
        if (rc < 0) { 
            errcode = WSAGetLastError();
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
        
boolean win_socket::read(void* buf, size_t size)
{ 
    if (state != ss_open) { 
        errcode = not_opened;
        return False;
    }

	//[MC] Read data using the specified socket buffer size
	static size_t recv_size = (0);
	if(recv_size == 0)
	{
		DWORD socket_buffer_length = 0;
		int optlen = sizeof(socket_buffer_length);
		if (getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&socket_buffer_length, (int*)&optlen) == SOCKET_ERROR)
		{
			socket_buffer_length = 8 * 1024;
	}

		recv_size = socket_buffer_length;
	}

	size_t r_size = 0;
	while(r_size < size)
	{
		int part = int(std::min(std::min(size - r_size, recv_size), size_t(std::numeric_limits<int>::max())));
		r_size += part;

		do { 
			int rc = recv(s, (char*)buf, part, 0);
			if (rc < 0) { 
				errcode = WSAGetLastError();
				return False;
			} else if (rc == 0) {
				errcode = broken_pipe;
				return False; 
			} else { 
				buf = (char*)buf + rc; 
				part -= rc; 
			}
		} while (part > 0);
	}

    return True;
}
        

boolean win_socket::write(void const* buf, size_t size)
{ 
    if (state != ss_open) { 
        errcode = not_opened;
        return False;
    }
	//[MC] Write data using the specified socket buffer size
	static size_t send_size = (0);
	if(send_size == 0)
	{
		DWORD socket_buffer_length = 0;
		int optlen = sizeof(socket_buffer_length);
		if (getsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&socket_buffer_length, (int*)&optlen) == SOCKET_ERROR)
		{
			socket_buffer_length = 8*1024;
		}

		send_size = socket_buffer_length;
	}

	size_t w_size = 0;
	while(w_size < size)
	{
		int part = int(std::min(std::min(size - w_size, send_size), size_t(std::numeric_limits<int>::max())));
		w_size += part;

		do { 
			int rc = send(s, (char*)buf, part, 0);
			if (rc < 0) { 
				errcode = WSAGetLastError();
				if (errcode == WSAENOBUFS) { 
					Sleep(100);
					continue;
				}
				return False;
			} else if (rc == 0) {
				errcode = broken_pipe;
				return False; 
			} else {
				errcode = ok;
				buf = (char*)buf + rc; 
				part -= rc; 
			}
		} while (part > 0);
	}
    
    return True;
}
        
boolean win_socket::shutdown()
{
    if (state == ss_open) { 
        state = ss_shutdown;
        int rc = ::shutdown(s, 2);
        if (rc != 0) {
            errcode = WSAGetLastError();
            return False;
        } 
    } 
    errcode = ok;
    return True;
}


void win_socket::clear_error()
{
    errcode = ok;
}

boolean win_socket::close()
{
    if (state != ss_close) { 
        state = ss_close;
        if (closesocket(s) == 0) { 
            errcode = ok;
            return True;
        } else { 
            errcode = WSAGetLastError();
            return False;
        }
    }
    return True;
}

win_socket::~win_socket()
{
    delete[] address;
    close();
}

win_socket::win_socket(const char* addr)
{ 
    address = new char[strlen(addr) + 1]; 
    strcpy(address, addr);
    errcode = ok;
    s = INVALID_SOCKET;
}

win_socket::win_socket(SOCKET new_sock) 
{ 
    s = new_sock; 
    address = NULL; 
    state = ss_open;
    errcode = ok;
}

socket_t* socket_t::create_local(char const* address, int listen_queue_size)
{
    if (address[0] == '!') {
	return new process_socket(address);
    }
    local_win_socket* sock = new local_win_socket(address);
    sock->open(listen_queue_size);
    return sock;
}

socket_t* socket_t::create_global(char const* address, int listen_queue_size)
{
    win_socket* sock = new win_socket(address);
    sock->open(listen_queue_size); 
    return sock;
}

socket_t* socket_t::connect(char const* address, 
                            socket_domain domain, 
                            int max_attempts,
                            time_t timeout,
                            time_t connectTimeout)
{
#ifdef RESTRICTED_USE_OF_LOCAL_SOCKETS
    char   hostname[MAX_HOST_NAME];
#endif

    size_t hostname_len;
    char const*  port;
    
    if (address[0] == '!') {
        return process_socket::Connect(address, max_attempts, timeout,
                                       connectTimeout);
    }

    if (domain == sock_local_domain 
        || (domain == sock_any_domain 
            && ((port = strchr(address, ':')) == NULL 
                || ((hostname_len = port - address) == 9 
                    && strincmp(address, "localhost", hostname_len) == 0)
#ifdef RESTRICTED_USE_OF_LOCAL_SOCKETS
                || (gethostname(hostname, sizeof hostname) == 0 
                    && strlen(hostname) == hostname_len 
                    && strincmp(address, hostname, hostname_len) == 0)
#endif
		)))
     {
        local_win_socket* s = new local_win_socket(address);
        s->connect(max_attempts, timeout, connectTimeout); 
        return s;
     } else { 
        win_socket* s = new win_socket(address);
        s->connect(max_attempts, timeout, connectTimeout); 
        return s;
    }  
}
    
static process_name_function current_process_name_function = 0;
void set_process_name_function( process_name_function func) {
    current_process_name_function = func;
} 

char const* get_process_name() 
{ 
    if( !current_process_name_function ) {
	static char name[MAX_HOST_NAME+8];
	gethostname(name, MAX_HOST_NAME); 
	sprintf(name + strlen(name), ":%x:%p", (int)GetCurrentProcessId(),  task::current());
	return name;
    } else {
	return current_process_name_function();
    }
}

//
// Local windows sockets
//

//xmmintrin.h
int local_win_socket::read(void* buf, size_t min_size, size_t max_size, 
                           time_t timeout)
{
    time_t start = 0;
    char* dst = (char*)buf;
    size_t size = 0;
    Error = ok;
    if (timeout != WAIT_FOREVER) { 
        start = time(NULL); 
        timeout *= 1000; // convert seconds to miliseconds
    }
    while (size < min_size && state == ss_open) {       
        RcvBuf->RcvWaitFlag = true;
        MemoryBarrier();
        size_t begin = RcvBuf->DataBeg;
        size_t end = RcvBuf->DataEnd;
        size_t rcv_size = (begin <= end)
            ? end - begin : sizeof(RcvBuf->Data) - begin;
        if (rcv_size > 0) { 
            RcvBuf->RcvWaitFlag = false;
            if (rcv_size >= max_size) { 
                memcpy(dst, &RcvBuf->Data[begin], max_size);
                begin += max_size;
                size += max_size;
            } else { 
                memcpy(dst, &RcvBuf->Data[begin], rcv_size);
                begin += rcv_size;
                dst += rcv_size;
                size += rcv_size;
                max_size -= rcv_size;
            } 
            RcvBuf->DataBeg = int((begin == sizeof(RcvBuf->Data)) ? 0 : begin);
            MemoryBarrier();
            if (RcvBuf->SndWaitFlag) { 
                SetEvent(Signal[RTR]);
            }           
        } else {
            HANDLE h[2];
            h[0] = Signal[RD];
            h[1] = Mutex;
            int rc = WaitForMultipleObjects(nObjects, h, false, timeout);
            RcvBuf->RcvWaitFlag = false;
            if (rc != WAIT_OBJECT_0) {
                if (rc == WAIT_OBJECT_0+1 || rc == WAIT_ABANDONED+1) { 
#ifdef DEBUG_LOCAL_SOCKETS
                    fprintf(stderr, "!!!! local_win_socket::read: watchdog mutex released\n");
#endif
                    Error = broken_pipe;
                    ReleaseMutex(Mutex);
                } else if (rc == WAIT_TIMEOUT) { 
                    return (int)size;
                } else { 
                    Error = GetLastError();
#ifdef DEBUG_LOCAL_SOCKETS
                    fprintf(stderr, "!!!! local_win_socket::read: %d\n", Error);
#endif
                }
                return -1;
            }
            if (timeout != WAIT_FOREVER) { 
                time_t now = time(NULL);
                timeout = timeout >= (now - start)*1000 
                    ? timeout - (now - start)*1000 : 0;  
            }
        }
    }                   
    return size < min_size ? -1 : (int)size;
}

boolean local_win_socket::read(void* buf, size_t size)
{
    char* dst = (char*)buf;
    Error = ok;
    while (size > 0 && state == ss_open) {      
        RcvBuf->RcvWaitFlag = True;
        MemoryBarrier();
        size_t begin = RcvBuf->DataBeg;
        size_t end = RcvBuf->DataEnd;
        size_t rcv_size = (begin <= end)
            ? end - begin : sizeof(RcvBuf->Data) - begin;
        if (rcv_size > 0) { 
            RcvBuf->RcvWaitFlag = False;
            if (rcv_size >= size) { 
                memcpy(dst, &RcvBuf->Data[begin], size);
                begin += size;
                size = 0;
            } else { 
                memcpy(dst, &RcvBuf->Data[begin], rcv_size);
                begin += rcv_size;
                dst += rcv_size;
                size -= rcv_size;
            } 
            RcvBuf->DataBeg = int((begin == sizeof(RcvBuf->Data)) ? 0 : begin);
            MemoryBarrier();
            if (RcvBuf->SndWaitFlag) { 
                SetEvent(Signal[RTR]);
            }           
        } else {
            HANDLE h[2];
            h[0] = Signal[RD];
            h[1] = Mutex;
            int rc = WaitForMultipleObjects(nObjects, h, FALSE, INFINITE);
            RcvBuf->RcvWaitFlag = False;
            if (rc != WAIT_OBJECT_0) {
                if (rc == WAIT_OBJECT_0+1 || rc == WAIT_ABANDONED+1) { 
#ifdef DEBUG_LOCAL_SOCKETS
                    fprintf(stderr, "!!!! local_win_socket::read: watchdog mutex released\n");
#endif
                    Error = broken_pipe;
                    ReleaseMutex(Mutex);
                } else { 
                    Error = GetLastError();
#ifdef DEBUG_LOCAL_SOCKETS
                    fprintf(stderr, "!!!! local_win_socket::read: %d\n", Error);
#endif
                }
                return False;
            }
        }
    }                   
    return size == 0;
}

boolean local_win_socket::write(const void* buf, size_t size)
{
    char* src = (char*)buf;
    Error = ok;
    while (size > 0 && state == ss_open) {      
        SndBuf->SndWaitFlag = True;
        MemoryBarrier();
        size_t begin = SndBuf->DataBeg;
        size_t end = SndBuf->DataEnd;
        size_t snd_size = (begin <= end) 
            ? sizeof(SndBuf->Data) - end - (begin == 0)
            : begin - end - 1;
        if (snd_size > 0) { 
            SndBuf->SndWaitFlag = False;
            if (snd_size >= size) { 
                memcpy(&SndBuf->Data[end], src, size);
                end += size;
                size = 0;
            } else { 
                memcpy(&SndBuf->Data[end], src, snd_size);
                end += snd_size;
                src += snd_size;
                size -= snd_size;
            } 
            SndBuf->DataEnd = int((end == sizeof(SndBuf->Data)) ? 0 : end);
            MemoryBarrier();
            if (SndBuf->RcvWaitFlag) { 
                SetEvent(Signal[TD]);
            }           
        } else {
            HANDLE h[2];
            h[0] = Signal[RTT];
            h[1] = Mutex;
            int rc = WaitForMultipleObjects(nObjects, h, FALSE, INFINITE);
            SndBuf->SndWaitFlag = False;
            if (rc != WAIT_OBJECT_0) {
                if (rc == WAIT_OBJECT_0+1 || rc == WAIT_ABANDONED+1) { 
#ifdef DEBUG_LOCAL_SOCKETS
                    fprintf(stderr, "!!!! local_win_socket::write: watchdog mutex released\n");
#endif
                    Error = broken_pipe;
                    ReleaseMutex(Mutex);
                } else { 
                    Error = GetLastError();
#ifdef DEBUG_LOCAL_SOCKETS
                    fprintf(stderr, "!!!! local_win_socket::write: %d\n", Error);
#endif
                }       
                return False;
            }
        }
    }                           
    return size == 0;
}

#define MAX_ADDRESS_LEN 64

local_win_socket::local_win_socket(const char* address)
{
    Name = new char[strlen(address) + 1]; 
    strcpy(Name, address);
    Error = not_opened;
    Mutex = NULL;
}
 
boolean local_win_socket::open(int)
{
    char buf[MAX_ADDRESS_LEN];  
    int  i;

    for (i = RD; i <= RTT; i++) {  
        sprintf(buf, "%s.%c", Name, i + '0');
        Signal[i] = CreateEventA(GOODS_SECURITY_ATTRIBUTES, False, False, buf);
        if (GetLastError() == ERROR_ALREADY_EXISTS) { 
            WaitForSingleObject(Signal[i], 0);
        }
        if (!Signal[i]) {
            Error = GetLastError();
            while (--i >= 0) { 
                CloseHandle(Signal[i]);
            }
            return False;
        }       
    }
    sprintf(buf, "%s.shr", Name);
    BufHnd = CreateFileMappingA(INVALID_HANDLE_VALUE, GOODS_SECURITY_ATTRIBUTES, PAGE_READWRITE,
                               0, sizeof(socket_buf)*2, buf);
    if (!BufHnd) {
        Error = GetLastError();
        for (i = RD; i <= RTT; i++) {  
            CloseHandle(Signal[i]);
        }
        return False;
    }
    RcvBuf = (socket_buf*)MapViewOfFile(BufHnd, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!RcvBuf) {
        Error = GetLastError();
        CloseHandle(BufHnd);
        for (i = RD; i <= RTT; i++) {  
            CloseHandle(Signal[i]);
        }
        return False;
    }   
    SndBuf = RcvBuf+1;
    RcvBuf->DataBeg = RcvBuf->DataEnd = 0;
    SndBuf->DataBeg = SndBuf->DataEnd = 0;       
    Error = ok;
    state = ss_open;
    return True;
}

local_win_socket::local_win_socket()
{
    int i;
    BufHnd = NULL;
    Mutex = NULL; 
    Name = NULL;

    for (i = RD; i <= RTT; i++) {  
        Signal[i] = CreateEventA(GOODS_SECURITY_ATTRIBUTES, False, False, NULL);
        if (!Signal[i]) {
            Error = GetLastError();
            while (--i >= 0) { 
                CloseHandle(Signal[i]);
            }
            return;
        }       
    }
    // create anonymous shared memory section
    BufHnd = CreateFileMappingA(INVALID_HANDLE_VALUE, GOODS_SECURITY_ATTRIBUTES, PAGE_READWRITE,
                               0, sizeof(socket_buf)*2, NULL);
    if (!BufHnd) {
        Error = GetLastError();
        for (i = RD; i <= RTT; i++) {  
            CloseHandle(Signal[i]);
        }
        return;
    }
    RcvBuf = (socket_buf*)MapViewOfFile(BufHnd, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!RcvBuf) {
        Error = GetLastError();
        CloseHandle(BufHnd);
        for (i = RD; i <= RTT; i++) {  
            CloseHandle(Signal[i]);
        }
        BufHnd = NULL;
        return;
    }   
    SndBuf = RcvBuf+1;
    RcvBuf->DataBeg = RcvBuf->DataEnd = 0;
    SndBuf->DataBeg = SndBuf->DataEnd = 0;       
    Error = ok;
    state = ss_open;
}

local_win_socket::~local_win_socket()
{
    close();
    delete[] Name;
}       

/*****************************************************************************\
abort_input - abort the current (or next) input operation
-------------------------------------------------------------------------------
This function will abort the current (or next) input operation being performed
by this object.  This function was designed to provide a thread with the
ability to cause another thread stuck in a read operation to bail.
\*****************************************************************************/

void local_win_socket::abort_input(void)
{
    // TODO: implement for MS-Windows.
}

socket_t* local_win_socket::accept()
{   
    HANDLE h[2];

    if (state != ss_open) {     
        return NULL;
    }
                    
    connect_data* cdp = (connect_data*)SndBuf->Data;
    cdp->Pid = GetCurrentProcessId();
    cdp->Mutex = WatchDogMutex;
    while (True) { 
        SetEvent(Signal[RTR]);
        int rc = WaitForSingleObject(Signal[RD], ACCEPT_TIMEOUT);
        if (rc == WAIT_OBJECT_0) {
            if (state != ss_open) { 
                Error = not_opened;
                return NULL;
            }
            Error = ok;
            break;
        } else if (rc != WAIT_TIMEOUT) { 
            Error = GetLastError();
            return NULL;
        }
    }
    local_win_socket* sock = new local_win_socket();
    sock->Mutex = ((connect_data*)RcvBuf->Data)->Mutex;
    accept_data* adp = (accept_data*)SndBuf->Data;
    adp->BufHnd = sock->BufHnd;
    for (int i = RD; i <= RTT; i++) { 
        adp->Signal[(i + TD - RD) & RTT] = sock->Signal[i]; 
    }
    SetEvent(Signal[TD]);
    h[0] = Signal[RD];
    h[1] = sock->Mutex;
    sock->nObjects = (sock->Mutex == INVALID_HANDLE_VALUE) ? 1 : 2;
    int rc = WaitForMultipleObjects(sock->nObjects, h, FALSE, INFINITE);
    if (rc != WAIT_OBJECT_0) {
        if (rc == WAIT_OBJECT_0+1 || rc == WAIT_ABANDONED+1) { 
            Error = broken_pipe;
            ReleaseMutex(Mutex);
        } else { 
            Error = GetLastError();
        }       
        delete sock;
        return NULL;
    }    
    return sock;
}

boolean local_win_socket::cancel_accept() 
{
    state = ss_shutdown;
    SetEvent(Signal[RD]);
    SetEvent(Signal[RTT]);
    return True;
}    

char* local_win_socket::get_peer_name(nat2*)
{
    if (state != ss_open) { 
        Error = not_opened;
        return NULL;
    }
    const char* addr = "127.0.0.1";
    char* addr_copy = new char[strlen(addr)+1];
    strcpy(addr_copy, addr);
    Error = ok;
    return addr_copy;
}

boolean local_win_socket::is_ok()
{
    return !Error;
}

void local_win_socket::clear_error()
{
    Error = ok;
}

boolean local_win_socket::close()
{
    if (state != ss_close) {            
        state = ss_close;
        if (Mutex) { 
            CloseHandle(Mutex);
        }
        for (int i = RD; i <= RTT; i++) { 
            CloseHandle(Signal[i]);
        }
        UnmapViewOfFile(RcvBuf < SndBuf ? RcvBuf : SndBuf);
        CloseHandle(BufHnd);    
        Error = not_opened;
    }
    return True;
}

void local_win_socket::get_error_text(char* buf, size_t buf_size)
{
    switch (Error) { 
      case ok:
        strncpy(buf, "ok", buf_size);
        break;
      case not_opened:
        strncpy(buf, "socket not opened", buf_size);
        break;
      case broken_pipe:
        strncpy(buf, "connection is broken", buf_size);
        break;
      case timeout_expired:
        strncpy(buf, "connection timeout expired", buf_size);
        break;
      default:  
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL,
                       Error,
                       0,
                       buf,
                       DWORD(buf_size),
                       NULL);
    }
}


boolean local_win_socket::shutdown()
{
    if (state == ss_open) { 
        state = ss_shutdown;
        SetEvent(Signal[RD]);   
        SetEvent(Signal[RTT]);  
    }
    return True;
}

boolean local_win_socket::connect(int max_attempts, time_t timeout, time_t)
{
    char buf[MAX_ADDRESS_LEN];
    int  rc, i, error_code;
    HANDLE h[2];

    for (i = RD; i <= RTT; i++) {  
        sprintf(buf, "%s.%c", Name, ((i + TD - RD) & RTT) + '0');
        Signal[i] = CreateEventA(GOODS_SECURITY_ATTRIBUTES, False, False, buf);
        if (!Signal[i]) {
            Error = GetLastError();
            while (--i >= 0) { 
                CloseHandle(Signal[i]);
            }
            return False;
        }       
    }
    sprintf(buf, "%s.shr", Name);
    BufHnd = CreateFileMappingA(INVALID_HANDLE_VALUE, GOODS_SECURITY_ATTRIBUTES, PAGE_READWRITE,
                               0, sizeof(socket_buf)*2, buf);
    if (!BufHnd) {
        Error = GetLastError();
        for (i = RD; i <= RTT; i++) {  
            CloseHandle(Signal[i]);
        }
        return False;
    }
    SndBuf = (socket_buf*)MapViewOfFile(BufHnd, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!SndBuf) { 
        Error = GetLastError();
        for (i = RD; i <= RTT; i++) {  
            CloseHandle(Signal[i]);
        }
        CloseHandle(BufHnd);
        return False;
    }
    RcvBuf = SndBuf+1;
    state = ss_shutdown;
    Mutex = NULL;

    rc = WaitForSingleObject(Signal[RTT],timeout*max_attempts*MILLISECOND);
    if (rc != WAIT_OBJECT_0) {
        error_code = rc == WAIT_TIMEOUT ? timeout_expired : GetLastError();
        close();
        Error = error_code;
        return False;
    }
    connect_data* cdp = (connect_data*)RcvBuf->Data;
    HANDLE hServer = OpenProcess(STANDARD_RIGHTS_REQUIRED|PROCESS_DUP_HANDLE,
                                 FALSE, cdp->Pid);
//{ invoke "SeDebugPrivilege" if Process has it
    if (!hServer) {
        error_code = GetLastError();
        if (error_code != ERROR_ACCESS_DENIED) {
            close();
            Error = error_code;
            return False;
        }

        OSVERSIONINFO osvi = { 0 };

        // determine operating system version
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        GetVersionEx(&osvi);

        // nothing else to do if this is not Windows NT
        if (osvi.dwPlatformId != VER_PLATFORM_WIN32_NT) {
            SetLastError(ERROR_ACCESS_DENIED);
            error_code = ERROR_ACCESS_DENIED;
            close();
            Error = error_code; 
            return False;
        }

        // enable SE_DEBUG_NAME and try again
        TOKEN_PRIVILEGES Priv, PrivOld;
        DWORD cbPriv = sizeof(PrivOld);
        HANDLE hToken;

        // get current thread token 
        if (!OpenThreadToken(GetCurrentThread(), 
                             TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES,
                             FALSE, &hToken))
        {
            error_code = GetLastError();
            if (error_code != ERROR_NO_TOKEN) {
                close();
                Error = error_code; 
                return False;
            }

            // revert to the process token, if not impersonating
            if (!OpenProcessToken(GetCurrentProcess(),
                                  TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES,
                                  &hToken)) {
                error_code = GetLastError();
                close();
                Error = error_code;
                return False;
            }
        }

        internal_assert(ANYSIZE_ARRAY > 0);

        Priv.PrivilegeCount = 1;
        Priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        LookupPrivilegeValue(NULL, SE_DEBUG_NAME,
        &Priv.Privileges[0].Luid);

        // try to enable the privilege
        if (!AdjustTokenPrivileges(hToken, FALSE, &Priv, sizeof(Priv),
                                   &PrivOld, &cbPriv))
        {
            error_code = GetLastError();
            CloseHandle(hToken);
            close();
            Error = error_code;
            return False;
        }

        error_code = GetLastError();
        if(error_code == ERROR_NOT_ALL_ASSIGNED)
        {
            // the SE_DEBUG_NAME privilege is not in the caller's token
            CloseHandle(hToken);
            close();
            Error = error_code;
            return False;
        }

        // try to open process handle again
        hServer = OpenProcess(STANDARD_RIGHTS_REQUIRED |
                              PROCESS_DUP_HANDLE, FALSE, cdp->Pid);

        // restore original privilege state
        AdjustTokenPrivileges(hToken, FALSE, &PrivOld, sizeof(PrivOld),
                              NULL, NULL);

        CloseHandle(hToken);
    }
//}
    if (!hServer) { 
        error_code = GetLastError();
        close();
        Error = error_code;
        return False;
    }
    HANDLE hSelf = GetCurrentProcess();
    if (cdp->Pid == (int)GetCurrentProcessId()) {
        // communication within one process
        ((connect_data*)SndBuf->Data)->Mutex = INVALID_HANDLE_VALUE;
        nObjects = 1;
    } else { 
        nObjects = 2;
        if (!DuplicateHandle(hServer, cdp->Mutex, hSelf, &Mutex, 
                             0, FALSE, DUPLICATE_SAME_ACCESS) ||
            !DuplicateHandle(hSelf, WatchDogMutex, hServer, 
                             &((connect_data*)SndBuf->Data)->Mutex, 
                             0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                error_code = GetLastError();
                CloseHandle(hServer);
                close();
                Error = error_code;
                return False;
            }
    }
    SetEvent(Signal[TD]);
    h[0] = Signal[RD];
    h[1] = Mutex;
    rc = WaitForMultipleObjects(nObjects, h, FALSE, INFINITE);

    if (rc != WAIT_OBJECT_0) { 
        if (rc == WAIT_OBJECT_0+1 || rc == WAIT_ABANDONED+1) { 
            error_code = broken_pipe;
            ReleaseMutex(Mutex);
        } else { 
            error_code = GetLastError();
        }
        CloseHandle(hServer);
        close();
        Error = error_code;
        return False;
    }
    accept_data ad = *(accept_data*)RcvBuf->Data;

    SetEvent(Signal[TD]);
    for (i = RD; i <= RTT; i++) { 
        CloseHandle(Signal[i]);
    }
    UnmapViewOfFile(SndBuf);
    CloseHandle(BufHnd);        
    BufHnd = NULL;

    if (!DuplicateHandle(hServer, ad.BufHnd, hSelf, &BufHnd, 
                         0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        Error = GetLastError();
        CloseHandle(hServer);
        CloseHandle(Mutex); 
        return False;
    } else { 
        for (i = RD; i <= RTT; i++) { 
            if (!DuplicateHandle(hServer, ad.Signal[i], 
                                 hSelf, &Signal[i], 
                                 0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                Error = GetLastError();
                CloseHandle(hServer);
                CloseHandle(BufHnd); 
                CloseHandle(Mutex); 
                while (--i >= 0) CloseHandle(Signal[1]);
                return False;
            }
        }
    }
    CloseHandle(hServer);

    SndBuf = (socket_buf*)MapViewOfFile(BufHnd, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!SndBuf) { 
        Error = GetLastError();
        CloseHandle(BufHnd); 
        CloseHandle(Mutex); 
        for (i = RD; i <= RTT; i++) {  
            CloseHandle(Signal[i]);
        }
        return False;
    }
    RcvBuf = SndBuf+1;
    Error = ok;
    state = ss_open; 
    return True;
}

int local_win_socket::get_descriptor() 
{
    return -1;
}


END_GOODS_NAMESPACE
