// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< WWWAPI.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     27-Mar-99    K.A. Knizhnik  * / [] \ *
//                          Last update:  1-Jul-99    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// API for creating Internet applications 
//-------------------------------------------------------------------*--------*

#ifndef __WWWAPI_H__
#define __WWWAPI_H__

#include "goodsdlx.h"
#include "stdinc.h"
#include "sockio.h"

BEGIN_GOODS_NAMESPACE

class big_body;

enum WWWencodingType { 
    TAG  = 0, // HTML tags (no conversion)
    HTML = 1, // replace ('<','>','"','&') with (&lt; &gt; &amp; &qout;)
    URL  = 2  // replace spaces with '+', and other special characters with %XX
};
//
// Automatic state shifts after each append operation:
//   TAG->HTML
//   HTML->TAG
//   URL->TAG
//


class GOODS_DLL_EXPORT mime_typer {
  public:
           mime_typer();
  virtual ~mime_typer();

  virtual const char* getType(const char* iFile);
};


class GOODS_DLL_EXPORT WWWconnection {  
    friend class WWWapi;
    friend class CGIapi;
    friend class HTTPapi;
    friend class QueueManager;
    
  public:
    void* userData;
    typedef boolean (*handler)(WWWconnection& con);
    
    //
    // Limit the maximum size a to which connection's reply buffer is sized.
    //
    static size_t MmaxReplyBufSize;
    //
    // Append string to reply buffer
    //
    WWWconnection& append(char const* str);
    
    //
    // Append binary content to the reply buffer
    //
    WWWconnection& append(char const* buf, nat4 iLength);
    
    WWWconnection& operator << (char const* str) { 
        return append(str);
    }
    
    void setEncoding(WWWencodingType type) { encoding = type; }
    
    WWWconnection& operator << (WWWencodingType type) { 
        setEncoding(type);
        return *this;
    }
    WWWconnection& operator << (int value) { 
        char buf[32];
        sprintf(buf, "%d", value);
        return append(buf);
    }
    
    char* getStub() { return stub; }
    
    char* getAddress() { return address; }
    
    char* getPeer() { return peer; }
    
    //
    // Compare content of the string with the end of the reply buffer
    //
    boolean terminatedBy(char const* str) const;
    
    //
    // Get value of variable from request string. If name is not present in 
    // string NULL is returned. Parameter 'n' can be used to get n-th
    // value of variable for multiple selection slot. Zero value of n 
    // corresponds to the first variable's value, 1 - to the second,...
    // When no more values are available NULL is returned.
    //
    char* get(char const* name, int n = 0);
    
    //
    // Associatte value with name
    //
    void addPair(char const* name, char const* value);
    
    //
    // Read HTTP body content that follows the specified HTTP header; this
    // is usually called immediately after calling readHTTPheader()
    //
    char* readHTTPbody(char* ioBuf, int iBufSize, int iSize, char* iBody,
                       int& oLength, int iRedirects, char** oHeader, 
                       boolean iAllowFreeForm, big_body *iBigPost = NULL);
    
    //
    // Read a pending HTTP header into the specified buffer.
    //
    char* readHTTPheader(char* ioBuffer, size_t iBufSize, size_t &ioSize,
                         time_t iTimeout = WAIT_FOREVER);
    
    //
    // Get the incoming web request.
    //
    char* readHTTPcontent(char* ioBuf, int iBufSize, int& oLength,
                          int iRedirects, char** oHeader,
                          boolean iAllowFreeForm = true);
    
    //
    // Send the cached reply buffer to the connection's peer.  Returns true if
    // the connection's socket has been shut down.
    //
    boolean reply(boolean keepAlive, bool insertLength = false);
    
    //
    // Write the top portion of an HTTP header, with room to insert the
    // content length.
    //
    void writeHTTPheader(bool iKeepAlive);
    
    
    
    WWWconnection();
    ~WWWconnection();
  protected: 
    enum { hash_table_size = 1013 };
    socket_t*   sock;
    char*       reply_buf;
    size_t      reply_buf_size;
    size_t      reply_buf_used;
    char*       stub;
    char*       address;
    char*       peer;
    WWWconnection*  next;
    WWWencodingType encoding;    
   
    struct name_value_pair { 
        name_value_pair* next;
        char const*      name;
        char const*      value;
        unsigned         hash_code;
    };
    
    name_value_pair* hash_table[hash_table_size];
    name_value_pair* free_pairs;
    
    char* extendBuffer(size_t inc);
    
    
    //
    // Deallocate all resources hold by connection. It is not possible to 
    // call get_value() or reply() method after this. Method reset()
    // is implicitly called by WWWapi::get() method.
    //
    void reset();
    
    //
    // Unpack requests paramters
    //
    char* unpack(char* body, size_t body_length);
};


class GOODS_DLL_EXPORT WWWapi { 
  public:
    struct dispatcher { 
        char const*         page;
        WWWconnection::handler func;
        // filled by contracutor of WWWapi
        unsigned            hash_code;
        dispatcher*         collision_chain;
    };
    
  protected:
    socket_t*   sock;
    char*       address;
    enum { hash_table_size = 113  };
    dispatcher* hash_table[hash_table_size];
        
    boolean dispatch(WWWconnection& con, char* page);
        
  public:
    WWWapi(int n_handlers, dispatcher* dispatch_table);
    virtual ~WWWapi() {};

    //
    // Bind and listen socket
    //
    boolean open(char const* socket_address = "localhost:80", 
                 socket_t::socket_domain domain = socket_t::sock_global_domain,
                 int listen_queue = DEFAULT_LISTEN_QUEUE_SIZE);
        
        
    //
    // Read and execute requests
    //
    virtual boolean serve(WWWconnection& con) = 0;
        
    //
    // Accept new connection by the socket
    //
    boolean connect(WWWconnection& con);
        
    //
    // Close socket
    // 
    void close();
};


//
// Interaction with WWW server by means of CGI protocol and CGIatub program
//
class GOODS_DLL_EXPORT CGIapi : public WWWapi { 
  public:
    virtual boolean serve(WWWconnection& con);
    
    CGIapi(int n_handlers, dispatcher* dispatch_table) 
        : WWWapi(n_handlers, dispatch_table) {}
    virtual ~CGIapi() {};
};


// 
// Built-in implementation of subset of HTTP protocol
//
class GOODS_DLL_EXPORT HTTPapi : public WWWapi { 
  protected:
    time_t             connectionHoldTimeout;
    boolean            keepConnectionAlive;
    static mime_typer *Mmime_typer;
    
    virtual boolean handlePut(WWWconnection& con, char* header, const char* host,
                              big_body& content);
    virtual boolean handlePut(WWWconnection& con, char* header, const char* host,
                              char* content, nat4 contentLength);
    virtual boolean handleRequest(WWWconnection& con, char* header, char* url,
                                  char* begin, char* end, const char* host,
                                  boolean& result);
    virtual boolean handleNonPageRequest(WWWconnection& con, char* header,
                                         char* url, char* begin, char* end, 
                                         const char* host, boolean& result);
    virtual void logPageServed(WWWconnection& con, char* header, char* file,
                               size_t file_size, nat2 http_code);


  public:
    virtual boolean serve(WWWconnection& con);
    virtual boolean servePage(WWWconnection& con, char* header, char* file,
                              char** err);

    static boolean ServePage(WWWconnection& con, char* header, char* file,
                             char** err, boolean keepConnectionAlive, 
                             size_t *o_file_size = NULL, 
                             nat2 *o_http_code = NULL);

    static void SetMimeTyper(mime_typer *iMimeTyper);

    static char*       GetHost(char* ioHeader);
    static nat4        GetHost(char* iHeader, char* oBuf, nat4 iBufLen);
    static const char* GetType(const char* iFile);
    
    //--------------------------------------------------------------------------
    //  Search binary data
    //--------------------------------------------------------------------------
    char *BinaryPatternSearch (char* iStart, char* iEnd, 
                               const char *iPattern, size_t iPatternSize);
    char *extractTokenValue (const char *Token, char *start, char *end);
    char *findValueStart(char *start, char *end);
    bool  ParseMultipartData (WWWconnection& con, char *start, char *end, char
                              *boundary, char **data, int &dataLength);
    //--------------------------------------------------------------------------
    //  Read "multipart/form-data" from POST request and call handlePUT
    //--------------------------------------------------------------------------
    int   ReadMultipartData (WWWconnection& con, char *iHeader, const char *iHost,
                             char *iContentData, int   ContentDataLength);
    HTTPapi(int n_handlers, dispatcher* dispatch_table, 
            boolean persistentConnections = false,
            time_t connectionHoldTimeoutSec = WAIT_FOREVER) 
        : WWWapi(n_handlers, dispatch_table) 
    {
        keepConnectionAlive = persistentConnections;
        connectionHoldTimeout = connectionHoldTimeoutSec;
    }

    virtual ~HTTPapi() {};
};




class GOODS_DLL_EXPORT QueueManager { 
    WWWconnection* connectionPool;
    WWWconnection* freeList;
    WWWconnection* waitList;
    mutex          cs;
    semaphorex     start;
    eventex        done;
    task**         threads;
    int            nThreads;
    WWWapi*        server;
    
    static void task_proc handleThread(void* arg);
    void handle();
    
  public:
    void stop();
    
    QueueManager(void);
    QueueManager(WWWapi& api, // WWWapi should be opened
                 int     nThreads = 8, 
                 int     connectionQueueLen = 64,
                 size_t  threadStackSize = task::normal_stack);
    
    ~QueueManager();
    
    void run(WWWapi& api, // WWWapi should be opened
             int     nThreads = 8, 
             int     connectionQueueLen = 64,
             size_t  threadStackSize = task::normal_stack);
};

GOODS_DLL_EXPORT void URL2ASCII(char* src);

END_GOODS_NAMESPACE

#endif
