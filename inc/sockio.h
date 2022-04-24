// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< SOCKIO.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 23-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Socket abstraction
//-------------------------------------------------------------------*--------*

#ifndef __SOCKIO_H__
#define __SOCKIO_H__

#include <time.h>
#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

#ifndef DEFAULT_CONNECT_MAX_ATTEMPTS
#define DEFAULT_CONNECT_MAX_ATTEMPTS 3 //[MC]
#endif

#ifndef DEFAULT_RECONNECT_TIMEOUT
#define DEFAULT_RECONNECT_TIMEOUT    10 //[MC] 
#endif

#ifndef DEFAULT_LISTEN_QUEUE_SIZE
#define DEFAULT_LISTEN_QUEUE_SIZE    5
#endif

#ifndef LINGER_TIME
#define LINGER_TIME                  10
#endif

#define WAIT_FOREVER                 ((time_t)-1)

class big_body;

//
// Abstract socket interface
//
class GOODS_DLL_EXPORT socket_t { 
  public: 
    typedef void    (*bigBodyFn)(const char *iHeader, nat8 iSoFar, nat8 iTotal);
    typedef boolean (*progressFn)(nat4 iSoFar, nat4 iTotal);

    virtual int       read(void* buf, size_t min_size, size_t max_size,
                           time_t timeout = WAIT_FOREVER) = 0;
    virtual boolean   read(void* buf, size_t size) = 0;
    virtual boolean   write(void const* buf, size_t size) = 0;

    virtual boolean   is_ok() = 0; 
    virtual void      clear_error() = 0;
    virtual void      get_error_text(char* buf, size_t buf_size) = 0;

    //
    // This method is called by server to accept client connection
    //
    virtual socket_t* accept() = 0;

    //
    // Cancel accept operation and close socket
    //
    virtual boolean   cancel_accept() = 0;

    //
    // Call this method to abort another thread's call to this object's read()
    //
    virtual void abort_input() = 0;

    //
    // Shutdown socket: prohibite write and read operations on socket
    //
    virtual boolean   shutdown() = 0;

    //
    // Close socket
    //
    virtual boolean   close() = 0;

    //
    // Get socket peer name.
    // name is created using new char[]. If perr name can not be obtained NULL
    // is returned and errcode is set.
    //
    virtual char*     get_peer_name(nat2 *oPort = NULL) = 0;

    //
    // Create client socket connected to local or global server socket
    //
    enum socket_domain { 
        sock_any_domain,   // domain is chosen automatically
        sock_local_domain, // local domain (i.e. Unix domain socket) 
        sock_global_domain // global domain (i.e. INET sockets) 
    };

    static nat8       kBrokenBigBody;
    static bigBodyFn  MbigBodyFn;
    static progressFn MprogressFn;
    static time_t     MprogressInterval;

    static socket_t*  connect(char const* address, 
                              socket_domain domain = sock_any_domain, 
                              int max_attempts = DEFAULT_CONNECT_MAX_ATTEMPTS,
                              time_t timeout = DEFAULT_RECONNECT_TIMEOUT,
                              time_t connectTimeout = WAIT_FOREVER);
    
    //
    // Create local domain socket
    //
    static socket_t*  create_local(char const* address,
                                   int listen_queue_size = 
                                       DEFAULT_LISTEN_QUEUE_SIZE);

    //
    // Create global domain socket 
    //
    static socket_t*  create_global(char const* address,
                                   int listen_queue_size = 
                                       DEFAULT_LISTEN_QUEUE_SIZE);

    //
    // Copy the server name (and port) from the specified URL.
    //
    static boolean GetServerFromURL(const char* iURL, char* oServer);

    //
    // Get web content from a HTTP/1.1 or greater web server.
    //
    static char *GetURL(const char* iURL, int &oLength, int iRedirects = 0,
                        char** oHeader = NULL, const char* iAddToHeader = NULL);

    char *getURL(const char* iServer, const char* iURL, int &oLength,
                 int iRedirects = 0, char** oHeader = NULL,
                 const char* iAddToHeader = NULL);

    //
    // Post web content to a HTTP/1.1 or greater web server.
    //
    static char *PostToURL(const char* iURL, const char *iContent,
                           int iContentLength, int &oLength,
                           int iRedirects = 0, char** oHeader = NULL,
                           const char* iAddToHeader = NULL);

    char *postToURL(const char* iServer, const char* iURL,
                    const char *iContent, int iContentLength, int &oLength,
                    int iRedirects = 0, char** oHeader = NULL,
                    const char* iAddToHeader = NULL);

    //
    // Put web content to a HTTP/1.1 or greater web server.
    //
    static char *PutToURL(const char* iURL, const char *iContent,
                           int iContentLength, int &oLength,
                           int iRedirects = 0, char** oHeader = NULL,
                           const char* iAddToHeader = NULL);

    char *putToURL(const char* iServer, const char* iURL,
                   const char *iContent, int iContentLength, int &oLength,
                   int iRedirects = 0, char** oHeader = NULL,
                   const char* iAddToHeader = NULL);

    //
    // Perform any arbitrary HTTP method to a HTTP/1.1 or greater web server.
    //
    static char *ToURL(const char *iMethod, const char* iURL,
                       const char *iContent,
                       int iContentLength, int &oLength,
                       int iRedirects = 0, char** oHeader = NULL,
                       const char* iAddToHeader = NULL);

    char *toURL(const char *iMethod, const char* iServer, const char* iURL,
                const char *iContent, int iContentLength, int &oLength,
                int iRedirects = 0, char** oHeader = NULL,
                const char* iAddToHeader = NULL);

    //
    // Read HTTP body content that follows the specified HTTP header; this
    // is usually called immediately after calling readHTTPheader()
    //
    char* readHTTPbody(char* ioBuf, int iBufSize, int iSize, char* iBody,
                       int& oLength, int iRedirects, char** oHeader,
                       boolean iAllowFreeForm, big_body *oBigPost = NULL);

    //
    // Get the web content after sending a GET or a POST.
    //
    char* readHTTPcontent(char* ioBuf, int iBufSize, int& oLength,
                          int iRedirects, char** oHeader,
                          boolean iAllowFreeForm = true);

    //
    // Read a pending HTTP header into the specified buffer.
    //
    char* readHTTPheader(char* ioBuffer, size_t iBufSize, size_t &ioSize,
                         time_t iTimeout = WAIT_FOREVER);

    //
    // Report progress during an HTTP transfer; used internally.
    //
    boolean reportProgress(nat4 iSteps);

    //
    // Set the function to be called periodically during HTTP requests.
    //
    void setProgressFn(progressFn iFn);

    //
    // Set the maximum number of seconds that will pass between progress
    // function calls.
    //
    void setProgressInterval(time_t iSeconds);

    //
    // Get socket file descriptor.  Used by socket_t subclasses to obtain the
    // file descriptor from an instance of unisock or w32sock.
    //
    virtual int get_descriptor() = 0;

    socket_t();
    virtual ~socket_t() {}

  protected:
    enum { ss_open, ss_shutdown, ss_close } state;
    nat4                                    mContentLength;
    nat4                                    mNumTransferred;
    progressFn                              mProgressFn;
    time_t                                  mProgressInterval;

    //
    // Get web content from a page serving HTTP chunks.
    //
    char *readChunks(char *ioBuf, int &oLength, size_t iSize,
                     size_t iBufferSize);

    //
    // Get web content from a server that doesn't report content length.
    //
    char* readFreeForm(char* iFirstBuf, int iFirstBufLen, int &oLength);

    //
    // Get (potentially huge) HTTP content directly into a temporary file.
    //
    char* readHTTPbodyToTmpFile(char *iHeader, char *ioBuf, int iSize,
                                int iBufSize, nat8 iLength);
};

// 
// Return current host name + identifier of current process
//
GOODS_DLL_EXPORT extern char const* get_process_name(); 

typedef char const* (*process_name_function)();
GOODS_DLL_EXPORT extern void set_process_name_function( process_name_function); 


//
// Used by w32sock to setup watch dog mutex
//
GOODS_DLL_EXPORT extern void set_watchdog_thread();


END_GOODS_NAMESPACE

#endif
