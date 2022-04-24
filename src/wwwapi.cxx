// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< WWWAPI.CPP >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     27-Mar-99    K.A. Knizhnik  * / [] \ *
//                          Last update:  1-Jul-99    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Implementation of WWWapi class
//-------------------------------------------------------------------*--------*

#include "goods.h"
#include "bigbody.h"
#include "support.h"
#include "wwwapi.h"
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

BEGIN_GOODS_NAMESPACE

const size_t init_reply_buffer_size = 4*1024;
const size_t kMaxHostNameLen = 255;

// By default, WWWconnection reply buffers are not sized to exceed this
// amount (1MB).  GOODS server applications can set their own maximum bound.
size_t MmaxReplyBufSize = 1024 * 1024;

#define ERROR_TEXT(x) \
"HTTP/1.1 " x "\r\n\
Connection: close\r\n\r\n\
<HTML><HEAD><TITLE>Invalid request to the database</TITLE>\r\n\
</HEAD><BODY>\n\r\
<H1>" x "</H1>\n\r\
</BODY></HTML>\r\n\r\n"


mime_typer *HTTPapi::Mmime_typer = NULL;

mime_typer::mime_typer()
{}

mime_typer::~mime_typer()
{}

/*****************************************************************************\
getType - get the MIME type of the specified file
\*****************************************************************************/

const char* mime_typer::getType(const char *iFile)
{
    int   fileLen = (int)strlen(iFile);
    
    if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".gif") == 0))
        return "image/gif";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".jpg") == 0))
        return "image/jpeg";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".txt") == 0))
        return "text/plain";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".pdf") == 0))
        return "application/pdf";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".png") == 0))
        return "image/png";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".exe") == 0))
        return "application/octet-stream";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".xls") == 0))
        return "application/vnd.ms-excel";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".doc") == 0))
        return "application/msword";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".swf") == 0))
        return "application/x-shockwave-flash";
    else if((fileLen > 4) && (stricmp(&(iFile[fileLen - 4]), ".htm") == 0))
        return "text/html";
    else if((fileLen > 5) && (stricmp(&(iFile[fileLen - 5]), ".html") == 0))
        return "text/html";
    else if((fileLen > 6) && (stricmp(&(iFile[fileLen - 6]), ".htmlt") == 0))
        return "text/html";
    return "application/octet-stream";
}



WWWconnection::WWWconnection()
{
    memset(hash_table, 0, sizeof hash_table);
    sock = NULL;
    reply_buf = new char[init_reply_buffer_size];
    reply_buf_size = init_reply_buffer_size;
    free_pairs = NULL;
    peer = NULL;
    userData = NULL;
}

WWWconnection::~WWWconnection()
{
    reset();
    name_value_pair *nvp, *next;
    for (nvp = free_pairs; nvp != NULL; nvp = next) { 
        next = nvp->next;
        delete nvp;
    }
    delete[] reply_buf;
    delete[] peer;
}


inline char* WWWconnection::extendBuffer(size_t inc)
{
    if (reply_buf_used + inc + 1 >= reply_buf_size) { 
        reply_buf_size = reply_buf_size*2 > reply_buf_used + inc + 1
            ? reply_buf_size*2 : reply_buf_used + inc + 1;
        char* new_buf = new char[reply_buf_size];
        memcpy(new_buf, reply_buf, reply_buf_used);
        delete[] reply_buf;
        reply_buf = new_buf;
    }     
    reply_buf_used += inc;
    return reply_buf;
}

boolean WWWconnection::terminatedBy(char const* str) const
{
    size_t len = strlen(str);
    if (len > reply_buf_used - 4) { 
        return False;
    }
    return memcmp(reply_buf + reply_buf_used - len, str, len) == 0;
}

WWWconnection& WWWconnection::append(char const* str) 
{
    int pos = (int)reply_buf_used;
    char* dst = extendBuffer(strlen(str));
    unsigned char ch;
    switch (encoding) {
    case TAG:
        strcpy(dst + pos, str);
        encoding = HTML;
        break;
    case HTML:
        encoding = TAG;
        while (True) { 
            switch(ch = *str++) { 
            case '<':
                dst = extendBuffer(3);
                dst[pos++] = '&';
                dst[pos++] = 'l';
                dst[pos++] = 't';
                dst[pos++] = ';';
                break;
            case '>':
                dst = extendBuffer(3);
                dst[pos++] = '&';
                dst[pos++] = 'g';
                dst[pos++] = 't';
                dst[pos++] = ';';
                break;
            case '&':
                dst = extendBuffer(4);
                dst[pos++] = '&';
                dst[pos++] = 'a';
                dst[pos++] = 'm';
                dst[pos++] = 'p';
                dst[pos++] = ';';
                break;
            case '"':
                dst = extendBuffer(5);
                dst[pos++] = '&';
                dst[pos++] = 'q';
                dst[pos++] = 'u';
                dst[pos++] = 'o';
                dst[pos++] = 't';
                dst[pos++] = ';';
                break;
            case '\0':
                dst[pos] = '\0';
                return *this;
            default:
                dst[pos++] = ch;
            }
        }
        break;
    case URL:
        encoding = TAG;
        while (True) { 
            ch = *str++;
            if (ch == '\0') { 
                dst[pos] = '\0';
                return *this;
            } else if (ch == ' ') { 
                dst[pos++] = '+';
            } else if (!isalnum(ch)) { 
                dst = extendBuffer(2);
                dst[pos++] = '%';
                dst[pos++] = (ch >> 4) >= 10 
                    ? (ch >> 4) + 'A' - 10 : (ch >> 4) + '0';
                dst[pos++] = (ch & 0xF) >= 10
                    ? (ch & 0xF) + 'A' - 10 : (ch & 0xF) + '0';
            } else { 
                dst[pos++] = ch;
            }
        }
    }
    return *this;
}

WWWconnection& WWWconnection::append(char const* buf, nat4 iLength)
{
    int pos = (int)reply_buf_used;
    char* dst = extendBuffer(iLength);
    memcpy(dst + pos, buf, iLength);
    return *this;
}

void WWWconnection::reset()
{
    reply_buf_used = 0;
    encoding = TAG;
    for (int i = itemsof(hash_table); --i >= 0;) { 
        name_value_pair *nvp, *next;
        for (nvp = hash_table[i]; nvp != NULL; nvp = next) { 
            next = nvp->next;
            nvp->next = free_pairs;
            free_pairs = nvp;
        }
        hash_table[i] = NULL;
    }
}

void WWWconnection::addPair(char const* name, char const* value)
{
    name_value_pair* nvp;
    if (free_pairs != NULL) { 
        nvp = free_pairs;
        free_pairs = nvp->next;
    } else { 
        nvp = new name_value_pair;
    }
    unsigned hash_code = string_hash_function(name);
    nvp->hash_code = hash_code;
    hash_code %= hash_table_size;
    nvp->next = hash_table[hash_code];
    hash_table[hash_code] = nvp;
    nvp->value = value;
    nvp->name = name;
}

char* WWWconnection::unpack(char* body, size_t length)
{
    char *src = body, *end = body + length;
    
    while (src < end) { 
        char* name = src;
        char ch; 
        char* dst = src;
        while (src < end && (ch = *src++) != '=') { 
            if (ch == '+') {
                ch = ' ';
            } else if (ch == '%') { 
                ch = ((src[0] >= 'A' ? src[0] - 'A'+ 10 : src[0] - '0') << 4) |
                    (src[1] >= 'A' ? src[1] - 'A'+ 10 : src[1] - '0');
                src += 2;
            }
            *dst++ = ch;
        }
        *dst = '\0';
        char* value = dst = src;
        while (src < end && (ch = *src++) != '&') { 
            if (ch == '+') {
                ch = ' ';
            } else if (ch == '%') { 
                ch = ((src[0] >= 'A' ? src[0] - 'A'+ 10 : src[0] - '0') << 4) |
                    (src[1] >= 'A' ? src[1] - 'A'+ 10 : src[1] - '0');
                src += 2;
            }
            *dst++ = ch;
        }
        *dst = '\0';
        addPair(name, value);
    }
    stub = get("stub");
    return get("page");
}


char* WWWconnection::get(char const* name, int n)
{
    unsigned hash_code = string_hash_function(name);
    name_value_pair* nvp;
    for (nvp = hash_table[hash_code % hash_table_size];
    nvp != NULL; 
    nvp = nvp->next)
    {
        if (nvp->hash_code == hash_code && strcmp(nvp->name, name) == 0) { 
            if (n == 0) { 
                return (char*)nvp->value;
            }
            n -= 1;
        }
    }
    return NULL;
}


/*****************************************************************************\
readHTTPbody - see socket_t::readHTTPbody()
\*****************************************************************************/

char* WWWconnection::readHTTPbody(char* ioBuf, int iBufSize, int iSize,
                                  char* iBody, int& oLength, int iRedirects, char** oHeader,
                                  boolean iAllowFreeForm, big_body *iBigPost)
{
    if (!sock) {
        return NULL;
    }
    return sock->readHTTPbody(ioBuf, iBufSize, iSize, iBody, oLength,
        iRedirects, oHeader, iAllowFreeForm, iBigPost);
}


/*****************************************************************************\
readHTTPcontent - see socket_t::readHTTPcontent()
\*****************************************************************************/

char* WWWconnection::readHTTPcontent(char* ioBuf, int iBufSize, int& oLength,
                                     int iRedirects, char** oHeader, boolean iAllowFreeForm)
{
    if (oHeader) {
        *oHeader = NULL;
    }
    if (!sock) {
        return NULL;
    }
    reset();
    return sock->readHTTPcontent(ioBuf, iBufSize, oLength, iRedirects, oHeader,
        iAllowFreeForm);
}


/*****************************************************************************\
readHTTPheader - read a pending HTTP header into the specified buffer
-------------------------------------------------------------------------------
This function reads the HTTP header currently queued for reading on this
connection.  If an error occurs and this function closes this connection's
socket, NULL is returned.  If the HTTP header contains a bad request or the
HTTP header read operation times out, the appropriate response is streamed
across the HTTP connection and a pointer to the buffer is returned (no header
is read).  Otherwise if the read succeeds, a pointer to the first byte of
HTTP content past the HTTP header is returned, ioSize is updated to the total
number of bytes read, and ioBuffer will contain the NULL-terminated HTTP
header.

Note that more characters may be read into the specified buffer than there are
in the HTTP header; the returned pointer points to the first character of HTTP
content past the header, and ioSize contains the total number of bytes read.
So the calculation (ioSize - <retval> - ioBuffer) will return the number of
HTTP content bytes this function read.

ioBuffer specifies a buffer into which this function reads the pending HTTP
         header
iBufSize specifies the size of the specified buffer, in bytes
ioSize   when this function completes, this value will contain the total number
         of bytes copied into the specified buffer (including the HTTP header
         and any HTTP content)
iTimeout specifies the socket's timeout value; if unspecified, this function
         will wait forever for the HTTP header to be read
\*****************************************************************************/

char* WWWconnection::readHTTPheader(char* ioBuffer, size_t iBufSize,
                                    size_t &ioSize, time_t iTimeout)
{
    char* p;
    
    reset();
    if (!sock) {
        return NULL;
    }
    p = sock->readHTTPheader(ioBuffer, iBufSize, ioSize, iTimeout);
    if (p == NULL) {
        delete sock;
        sock = NULL;
    }
    return p;
}


/*****************************************************************************\
reply - send the cached reply buffer
-------------------------------------------------------------------------------
This function sends the cached reply buffer to this connection's peer.  This
function optionally inserts the length of the content in the reply buffer into
the HTTP header in this object's cache.  This function also optionally shuts
down this connection's socket.

This function returns `true' if this connection's socket has been shut down;
otherwise `false' is returned.

keepAlive    specifies whether this connection's socket should be shut down
             after the buffer is sent
insertLength specifies whether the content length should be inserted into
             the HTTP header that is at the beginning of this connection's
             reply buffer; this should only be `true' if this object's
             writeHTTPheader() was used
\*****************************************************************************/

boolean WWWconnection::reply(boolean keepAlive, bool insertLength)
{
    if (insertLength) {
        char* body;
        char  buf[64];
        char prev_ch = 0;
        reply_buf[reply_buf_used] = '\0';
        body = strstr(reply_buf, "Content-Length: ");
        if (body != NULL) {
            body += 16;
            int length_pos = body - reply_buf;
            while ((*body != '\n' || prev_ch != '\n') &&
                (*body != '\r' || prev_ch != '\n') && 
                *body != '\0')
            {
                prev_ch = *body++;
            }
            if (*body == '\0') { 
                reset();
                append(ERROR_TEXT("HTTP/1.1 500 Internal server error"));
                if (sock) {
                    sock->write(reply_buf, reply_buf_used);
                }
                return false;
            }
            body += *body == '\n' ? 1 : 2;
            sprintf(buf, "%u", (unsigned)(reply_buf_used - (body - reply_buf)));
            memcpy(reply_buf + length_pos, buf, strlen(buf));
        }
    }
    
    if (sock) {
        sock->write(reply_buf, reply_buf_used);
    } else {
        return true;
    }
    if (!keepAlive) {
        delete sock;
        sock = NULL;
        return true;
    }
    return false;
}

/*****************************************************************************\
writeHTTPheader - write the top part of an HTTP header, with space for length
-------------------------------------------------------------------------------
This function writes the top portion of an HTTP header to the connection
buffer, with a blank space inserted so that the content length can be filled
in later by reply() (when we actually know how much content we'll be sending!)

iKeepAlive specifies whether the HTTP header should indicate to the client to
           keep the connection alive or shut it down
\*****************************************************************************/

void WWWconnection::writeHTTPheader(bool iKeepAlive)
{
    append("HTTP/1.1 200 OK\r\nContent-Length:       \r\n");
    append(iKeepAlive
        ? "Connection: Keep-Alive\r\n" 
        : "Connection: close\r\n");
}


//--------------------------------------------------


WWWapi::WWWapi(int n_handlers, dispatcher* dispatch_table)
{
    memset(hash_table, 0, sizeof hash_table);
    sock = NULL;
    address = NULL;
    dispatcher* disp = dispatch_table;
    while (--n_handlers >= 0) { 
        unsigned hash_code = string_hash_function(disp->page);
        disp->hash_code = hash_code;
        hash_code %= hash_table_size;
        disp->collision_chain = hash_table[hash_code];
        hash_table[hash_code] = disp;
        disp += 1;
    }
}

boolean WWWapi::open(char const* socket_address, 
                     socket_t::socket_domain domain, 
                     int listen_queue)
{
    if (sock != NULL) { 
        close();
    }
    address = new char[strlen(socket_address) + 1];
    strcpy(address, socket_address);
    sock = domain != socket_t::sock_global_domain 
        ? socket_t::create_local(socket_address, listen_queue)
        : socket_t::create_global(socket_address, listen_queue);
    return sock != NULL;
}




boolean WWWapi::connect(WWWconnection& con)
{
    assert(sock != NULL);
    con.reset();
    delete con.sock;
    con.sock = sock->accept();
    con.address = address;
    return con.sock != NULL;
}



void WWWapi::close()
{
    delete sock;
    delete[] address;
    sock = NULL;
}



boolean WWWapi::dispatch(WWWconnection& con, char* page)
{
    unsigned hash_code = string_hash_function(page);
    for (dispatcher* disp = hash_table[hash_code % hash_table_size];
    disp != NULL; 
    disp = disp->collision_chain)
    {
        if (disp->hash_code == hash_code && strcmp(disp->page, page) == 0)
        { 
            return disp->func(con);
        }
    }
    return True;
}



boolean CGIapi::serve(WWWconnection& con)
{
    nat4 length;
    con.reset();
    if ((size_t)con.sock->read(&length, sizeof length, sizeof length) 
        != sizeof(length)) 
    {
        return True;
    }
    int size = length - sizeof length;
    char* buf = new char[size];
    if (con.sock->read(buf, size, size) != size) {
        return True;
    }
    char* page = con.unpack(buf + buf[0], length - sizeof length - buf[0]);
    char* peer = con.get("peer");
    con.peer = new char[strlen(peer)+1];
    strcpy(con.peer, peer);
    boolean result = True;
    if (page != NULL)  { 
        con.extendBuffer(4);
        result = dispatch(con, page);
        *(int4*)con.reply_buf = (int)con.reply_buf_used;
        con.sock->write(con.reply_buf, con.reply_buf_used);    
    }
    delete con.sock;
    con.sock = NULL; // close connection
    return result;
}

#define HEX_DIGIT(ch) ((ch) >= 'a' ? ((ch) - 'a' + 10) : (ch) >= 'A' ? (((ch) - 'A' + 10)) : ((ch) - '0'))

void URL2ASCII(char* src)
{
    char* dst = src;
    char ch;
    while ((ch = *src++) != '\0') { 
        if (ch == '%') { 
            *dst++ = (HEX_DIGIT(src[0]) << 4) | HEX_DIGIT(src[1]);
            src += 2;
        } else if (ch == '+') {
            *dst++ = ' ';
        } else { 
            *dst++ = ch;
        }
    }
    *dst = '\0';
}

//*******************************************************************************
// Parse multipart data, retreive the name-value pairs and first
// file transfered. Search between start and end pointers in the buffer.
// Assume that data in the buffer is delimited by "bundary" string.
//*******************************************************************************
bool  HTTPapi::ParseMultipartData (WWWconnection& con,
                                   char *start, 
                                   char *end, 
                                   char *boundary,
                                   char **data, 
                                   int  &dataLength)
{
    int   bndlength = (int)strlen(boundary);
    char *bnd_end, *tptr, *pname, *pfilename, *tmp;

    *data = NULL; 
    dataLength = 0;

    while (start <= end) {
        bnd_end = BinaryPatternSearch (start, end, boundary, bndlength);
        if (!bnd_end) return False;
        if (bnd_end == start) {
            while (*start != '\r' && *start != '\n') {
                start++; if (start > end) return False;
            }
            start += 2;
        } else {
            bnd_end -= 2;
            //-------------------- get line
            tptr = start;
            while (*tptr != '\r' && *tptr != '\n') {
                tptr++; if (tptr > end) return False;
            }
            //------ look for data start
            tmp = findValueStart(start, bnd_end);
            //-------------------- search for name
            pname = extractTokenValue ("name", start, tptr);
            if (pname) {
                if (tmp) {
                    pfilename = extractTokenValue ("filename", start, tptr);
                    if (pfilename) {
                        if (*data == NULL) {
                            dataLength = bnd_end - tmp;
                            if (dataLength) {
                                *data = new char[dataLength];
                                memcpy(*data, tmp, dataLength);
                            }
                            con.addPair("filename", pfilename);
                        }
                    } else {
                        //-------- this is name - value pair
                        *bnd_end = '\0';
                        con.addPair(pname, tmp);
                    }
                }
            }
            //---------- go to next data
            start = bnd_end + bndlength + 4;
        }
    }
    return True;
}

//**********************************************************************************
// There is no limitations in data size when it is POSTed as "multipart/form-data"
// Function will read entire multipart/form-data buffer and call handlePUT function
// if no error occured.
//
// Parameters:
//      iHeader             - pointer to character string contains HTTP header
//      iHost               - host name, string.
//      iContentData        - pointer to contents data received
//      ContentDataLength   - size of content data in iBody buffer.
//
// Function returns non-zero value if successfull.
//**********************************************************************************
int HTTPapi::ReadMultipartData (WWWconnection& con,
                                char *iHeader, 
                                const char *iHost, 
                                char *iContentData,
                                int   ContentDataLength)
{
    char *tptr;
    char *bndry;
    char *startbound;
    char *filedata = NULL;
    int  dataLen;
    size_t bndrysz;

    //----------------------------------------------------------------
    // get boundary value and size for this multipart data
    //----------------------------------------------------------------
    bndry = stristr(iHeader, "boundary");
    if (!bndry) return False;
    bndry += 8;
    while (*bndry == ' ') bndry++;
    while (*bndry == '=') bndry++;
    while (*bndry == ' ') bndry++;
        tptr = bndry;
        while (*tptr != '\n' && *tptr != '\r' && *tptr != '\0' && *tptr != ' ') tptr ++;
    bndrysz = tptr - bndry;
    //----------------------------------------------------------------
    //  build start and end boundary strings
    //----------------------------------------------------------------
    startbound = new char[bndrysz + 6];
    strcpy (startbound, "--");
    memcpy (&startbound[2], bndry, bndrysz);
    startbound[bndrysz + 2] = '\0';
    bndrysz = strlen(startbound);

    //----------------------------------------------------------------
    // pasre multipart data
    //----------------------------------------------------------------
    ParseMultipartData (con, iContentData,
                        &iContentData[ContentDataLength - 1],
                        startbound, &filedata, dataLen);

    boolean res = handlePut(con, iHeader, iHost, filedata, dataLen);
    delete [] startbound;
    return res;
}

//*******************************************************************************
//  Serve request 
//*******************************************************************************
boolean HTTPapi::serve(WWWconnection& con)
{
    const size_t inputBufferSize = 16*1024;
    char buf[inputBufferSize];
    boolean result = False;
    size_t size = 0;
    
    con.peer = con.sock->get_peer_name();
    
    while (true) { 
        char* p = con.readHTTPheader(buf, inputBufferSize, size,
            connectionHoldTimeout);
        if (!p) {
            return false;
        } else if (p == buf) {
            break;
        }
        int length = INT_MAX;
        char* lenptr = stristr(buf, "Content-Length: ");
        boolean persistentConnection = stristr(buf, "Connection: Keep-Alive") != NULL;
        if (lenptr != NULL) { 
            sscanf(lenptr+15, "%d", &length);
        }
        if (strncmp(buf, "GET ", 4) == 0) { 
            char hostBuf[kMaxHostNameLen];
            const char *host = hostBuf;
            GetHost(buf, hostBuf, kMaxHostNameLen);
            char* file, *uri = buf;
            file = strchr(uri, '/');
            if (file == NULL) { 
                con.append(ERROR_TEXT("400 Bad Request"));
                break;
            }
            if (*++file == '/') { 
                if (*host != '\0') { 
                    host = file+1;        
                }
                file = strchr(uri, '/');
                if (file == NULL) { 
                    con.append(ERROR_TEXT("400 Bad Request"));
                    break;
                }        
                *file++ = '\0';
            }
            char* file_end = strchr(file, ' ');
            char index_html[] = "index.html";
            if (file_end == NULL) { 
                con.append(ERROR_TEXT("400 Bad Request"));
                break;
            }
            if (file_end == file) { 
                file = index_html;
            } else {
                *file_end = '\0';
            }
            char* params = strchr(file, '?');
            if (params != NULL) { 
                if (*host == '\0') { 					
                    host = "localhost";
                }
                if (!handleRequest(con, file_end+1, file, params+1, file_end,
                    host, result)) { 
                    delete con.sock;
                    con.sock = NULL;
                    return result;
                } 
            } else {                 
                char* err = NULL;
                if (servePage(con, file_end+1, file, &err)) {
                    return True;
                }
                if (err) {
                    con.append(err);
                    break;
                }
            }
        } else if (strncmp(buf, "POST ", 5) == 0) { 
            char hostBuf[kMaxHostNameLen];
            const char *host = hostBuf;
            GetHost(buf, hostBuf, kMaxHostNameLen);
            char* body = p;
            if (stristr(buf, "Content-type: multipart/form-data")) {
                //-----------------------------------------------------------
                // we must process multipart data differently.
                // simpliest way is to retreive all data between
                // boundary markers and handle request as a PUT
                //-----------------------------------------------------------
                big_body biggie;
                char*    data    = NULL;
                int      dataLen = 0;
                data = con.readHTTPbody(buf, (int)inputBufferSize, (int)size, body, dataLen,
                                         0, NULL, true, &biggie);
                if (data) {
                    if (!ReadMultipartData(con, buf, host, data, dataLen)) {
                        delete con.sock;
                        delete [] data;
                        con.sock = NULL;
                        return False;
                    }
                    delete [] data;
                } else if (biggie.hasData()) {
                    big_body_part *part = biggie.first();
                    while(part != biggie.end()) {
                        con.addPair(part->name(), part->value());
                        part = part->getNext();
                    }
                    if(!handlePut(con, buf, host, biggie)) {
                        break;
                    }
                }
            } else {
                
ScanNextPart:
            int n = length < buf + size - p
                ? length : (int)(buf + size - p);
                while (--n >= 0 && *p != '\r' && *p != '\n') {
                    p += 1;
                }
                if (n < 0 && p - body != length) {
                    if (size >= sizeof(buf) - 1) {
                        con.append(ERROR_TEXT("413 Request Entity Too Large"));
                        break;
                    }        
                    int rc = con.sock->read(p, 1, sizeof(buf) - size - 1,
                    connectionHoldTimeout);
                    if (rc < 0) { 
                        delete con.sock;
                        con.sock = NULL;
                        return True;
                    }
                    size += rc;
                    goto ScanNextPart;
                } else {
                    if (*host == '\0') {
                        host = "localhost";
                    }
                    if (!handleRequest(con, buf, NULL, body, p, host, result)) {
                        delete con.sock;
                        con.sock = NULL;
                        return result;
                    }
                    while (p < buf + size && (*p == '\n' || *p == '\r')) {
                        p += 1;
                        n -= 1;
                    }
                }
            }
        } else if (strncmp(buf, "PUT ", 4) == 0) {
            char* begin;
            char* data    = NULL;
            int   dataLen = 0;
            char* end;
            char* header;
            char  hostBuf[kMaxHostNameLen];
            char *host = hostBuf;
            
            header = new char[strlen(buf) + 1];
            strcpy(header, buf);
            GetHost(buf, host, kMaxHostNameLen);
            begin = strstr(header, "?");
            if (begin != NULL) {
                end = ++begin;
                while (*end && (*end != ' ')) end++;
                con.unpack(begin, end - begin);
            }
            data = con.readHTTPbody(buf, (int)inputBufferSize, (int)size, p, dataLen,
                0, NULL, true);
            if (!handlePut(con, buf, host, data, dataLen)) {
                delete header;
                delete con.sock;
                con.sock = NULL;
                return result;
            }
            delete header;
        } else { 
            con.append(ERROR_TEXT("405 Method not allowed"));
            break;
        }
        if (!persistentConnection) { 
            delete con.sock;
            con.sock = NULL;
            return True;
        }            
        if (p - buf < (long)size) {
            size -= p - buf;
            memcpy(buf, p, size);
        } else { 
            size = 0;
        }
    }
    con.append("Connection: close\r\n\r\n");
    if (con.sock != NULL) { 
        con.sock->write(con.reply_buf, con.reply_buf_used);
        delete con.sock;
        con.sock = NULL;
    }
    return True;
}

//*******************************************************************************
// Serch paterns in the input buffer, between iStart and iEnd pointers.
// Returns a pointer to first occurense of the iParern, or NULL,
// if patern not found.
//*******************************************************************************
char *HTTPapi::BinaryPatternSearch (char* iStart, char* iEnd, const char *iPattern, size_t iPatternSize)
{
    char *st = iStart, *end = iEnd, *tmp;
    size_t n;
    int found;
    //------------------------------------------------------------------------
    // go thru entire buffer, from start to end
    //------------------------------------------------------------------------
    while (st <= end) {
        //--------------------------------------------------------------------
        // find first byte
        //--------------------------------------------------------------------
        while (*st != iPattern[0]) {
            st++;
            //--------------------------------------------------------------------
            // we are past the end of buffer, return NULL
            //--------------------------------------------------------------------
            if (st > end) return NULL;
        }

        //--------------------------------------------------------------------
        // search for the rest of patern
        //--------------------------------------------------------------------
        n = 1;
        found = 1;
        tmp = st;

        while (n < iPatternSize) {
            //----------------------------------------------------------------
            // we are past the end of buffer, return NULL
            //----------------------------------------------------------------
            tmp++;
            if (tmp > end) {
                return NULL;
            }
            if (*tmp != iPattern[n]) {
                found = 0;
                break;
            }
            n++;
        }
        if (found) {
            return st;
        } else {
            st = tmp;
        }
    }
    return NULL;
}

/*****************************************************************************\
GetHost - get the name of the host identified by the specified HTTP header
-------------------------------------------------------------------------------
This function returns a pointer to a NULL-terminated string containing the
name of the host against which the specified HTTP header will conduct its
operation.  Note that this function modifies the specified string (inserting a
NULL character after the host name) and returns a pointer within the specified
string.

If no host attribute is found in the specified HTTP header, NULL is returned.

ioHeader points to a NULL-terminated string containing the HTTP header whose
host name is to be retrieved; this function modifies this string
\*****************************************************************************/

char* HTTPapi::GetHost(char* ioHeader)
{
    char* host    = stristr(ioHeader, "Host: ");
    if (host != NULL) { 
        char* q = host += 6;
        while (*q != '\n' && *q != '\r' && *q != '\0') q += 1;
        *q = '\0';
    }
    return host;
}

//*******************************************************************************
//  extract token value from binary buffer
//*******************************************************************************
char *HTTPapi::extractTokenValue (const char *Token, char *start, char *end)
{
    char *ptoken = BinaryPatternSearch (start, end, Token, strlen(Token));
    char *pend, *pvalue = NULL;
    if (ptoken) {
        ptoken += strlen(Token);
        while (*ptoken == ' ') { ptoken++; if (ptoken > end) return NULL; }
        while (*ptoken == '=') { ptoken++; if (ptoken > end) return NULL; }
        while (*ptoken == ' ') { ptoken++; if (ptoken > end) return NULL; }
        while (*ptoken == '"') { ptoken++; if (ptoken > end) return NULL; }
        pend = ptoken;
        while (*pend != '"' && *pend != ';' && *pend != '\n' && *pend != '\r') { 
            pend++; 
            if (pend > end) { 
                return NULL; 
            }
        }
        *pend = '\0';
        pvalue = ptoken;
    }
    return pvalue;
}

//*******************************************************************************
//  find start value in multipart data
//*******************************************************************************
char *HTTPapi::findValueStart(char *start, char *end)
{
    nat4   nCR   = 0;
    nat4   nLF   = 0;
    while (start < end) {
        switch (*(start++)) {
        case '\n': nLF++; break;
        case '\r': nCR++; break;
        case ' ':  break;
        case '\t': break;
        default: nLF = nCR = 0;
        }
            if (((nCR == 2) && (nLF == 0))  || 
            ((nCR == 0) && (nLF == 2))  || 
            ((nCR == 2) && (nLF == 2))) 
                    return start;
    }
    return NULL;
}

/*****************************************************************************\
GetHost - get the name of the host identified by the specified HTTP header
-------------------------------------------------------------------------------
This function copies a NULL-terminated string into the specified buffer, a
string containing the name of the host against which the specified HTTP header
will conduct its operation.  Unlike the GetHost() implementation above, this
function does NOT modify the header from which the host name is copied.

The length of the host name is returned.  Note that this number may be larger
than the size of the buffer.

iHeader points to a NULL-terminated string containing the HTTP header whose
        host name is to be retrieved
oBuf    points to a buffer into which the host name (or as much of it that will
        fit) is copied; may be NULL if iBufLen is zero; unless this number is
        zero the copied string will always be NULL-terminated (even if the
        whole host name will not fit)
iBufLen specifies the size of the buffer (oBuf); if this number is smaller than
        the length of the host name, as much of the host name as will fit is
        copied into the buffer
\*****************************************************************************/

nat4 HTTPapi::GetHost(char* iHeader, char* oBuf, nat4 iBufLen)
{
    char* host = stristr(iHeader, "Host: ");
    nat4 hostLen = 0;
    if (host != NULL) { 
        const char* q = host += 6;
        while (*q != '\n' && *q != '\r' && *q != '\0') {
            if (hostLen < iBufLen) { 
                oBuf[hostLen] = *q;
            }
            hostLen += 1;
            q += 1;
        }
        hostLen = (nat4)(q - host);
        if (hostLen < iBufLen)
            oBuf[hostLen] = 0;
        else
            oBuf[iBufLen - 1] = 0;
    } else if (oBuf) {
        *oBuf = 0;
    }
    return hostLen;
}


/*****************************************************************************\
GetType - get the MIME type of the specified file
-------------------------------------------------------------------------------
This function allows the extension of the getType function, via mime_typer.
\*****************************************************************************/

const char* HTTPapi::GetType(const char *iFile)
{
    if(Mmime_typer != NULL) {
        return(Mmime_typer->getType(iFile));
    } else {
        mime_typer mime;
        return(mime.getType(iFile));
    }
}


/*****************************************************************************\
servePage - serve the specified file across the specified connection
-------------------------------------------------------------------------------
This function streams the specified file across the specified connection.

  Return Value:
  - If an error occurs, false is returned and `*err' will point to a string
  description of the error that occurred.
  - If the file is streamed successfully and the connection should not be kept
  alive, true is returned.
  - If the file is streamed successfully and the connection should be kept
  alive, false is returned and *err is NULL.

Arguments:
con    specifies the connection across which the specified file is to be
       streamed
header specifies the request's HTTP header
file   specifies the path to the file to be served across the connection
err    points to a character pointer which will be set to NULL if the file is
       streamed successfully or a string description of the error that occurred
       if the streaming fails
\*****************************************************************************/

boolean HTTPapi::servePage(WWWconnection& con, char* header, char* file,
                           char** err)
{
    nat2    http_code    = 200;
    size_t  file_size    = 0;
    boolean return_value;

    return_value = ServePage(con, header, file, err, keepConnectionAlive,
        &file_size, &http_code);
    logPageServed(con, header, file, file_size, http_code);
    return return_value;
}

boolean HTTPapi::ServePage(WWWconnection& con, char* header, char* file,
                           char** err, boolean keepConnectionAlive,
                           size_t *o_file_size, nat2 *o_http_code)
{
    FILE*       f        = NULL;
    const char* fileType = NULL;

    URL2ASCII(file);

    // If the specified path is to a directory, serve the index.html or
    // index.htm file within that directory.
    int statStat;
#ifdef _WINDOWS
    struct _stat isDirBuf;
    statStat = _stat(file, &isDirBuf);
#else
    struct stat isDirBuf;
    statStat = stat(file, &isDirBuf);
#endif
    if (statStat < 0) {
		static char _404[] = ERROR_TEXT("404 Not found");
        *err = _404;
        if (o_http_code != NULL) { 
            *o_http_code = 404;
        }
        return False;
    }
    if (isDirBuf.st_mode & S_IFDIR) {
        int   fileLen   = (int)strlen(file);
        char* indexFile = new char[fileLen + strlen("index.html") + 1];
        indexFile[0] = 0;
        if (fileLen > 0) {
            strcpy(indexFile, file);
            if (indexFile[fileLen - 1] != '/') {
                delete[] indexFile;
                con.append("HTTP/1.1 301 Moved Permanently\r\n");
                if(GetHost(header) != NULL) {
                    con.append("Location: http://");
                    con.append(GetHost(header));
                    con.append("/");
                    con.append(file);
                    con.append("/\r\n");
                }
                con.append("Connection: close\r\n\r\n");
                con << TAG;
				static char _301[] =
                    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
                    "<HTML><HEAD>\r\n"
                    "<TITLE>301 Moved Permanently</TITLE>\r\n"
                    "</HEAD><BODY>\r\n"
                    "<H1>Moved Permanently</H1>\r\n"
                    "The document has moved.\r\n"
                    "<HR>\r\n"
                    "</BODY></HTML>\r\n";

				*err = _301;
                if (o_http_code != NULL) { 
                    *o_http_code = 301;
                }
                return False;
            }
        }
        strcat(indexFile, "index.htm");
        f = fopen(indexFile, "rb");
        if (f == NULL) {
            strcat(indexFile, "l");
            f = fopen(indexFile, "rb");
        }
        fileType = GetType(indexFile);
        delete[] indexFile;
    } else {
        f = fopen(file, "rb");
        fileType = GetType(file);
    }

    if (f == NULL) { 
		static char _404[] = ERROR_TEXT("404 Not found");
        *err = _404;
        if (o_http_code != NULL) { 
            *o_http_code = 404;
        }
        return False;
    }
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    if(o_file_size != NULL) {
        *o_file_size = file_size;
    }
    fseek(f, 0, SEEK_SET);
    char reply[1024];
    sprintf(reply, "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\n"
            "Content-Type: %s\r\nConnection: %s\r\n\r\n", 
            (unsigned long)file_size, fileType,
            keepConnectionAlive ? "Keep-Alive" : "close");
    con.append(reply);
    if (!con.sock->write(con.reply_buf, con.reply_buf_used)) { 
        fclose(f);
        delete con.sock;
        con.sock = NULL;
        return True;
    }
    if (o_http_code != NULL) {
        *o_http_code = 200;
    }

    if ((file_size > con.reply_buf_size) &&
        (con.reply_buf_size < MmaxReplyBufSize)) {
        size_t newReplyBufSize = file_size;
        if (newReplyBufSize > MmaxReplyBufSize) {
            newReplyBufSize = MmaxReplyBufSize;
        }
        con.extendBuffer(newReplyBufSize - con.reply_buf_size);
    }

    size_t totalRead = 0;
    while (totalRead < file_size) {
        size_t numRead =  fread(con.reply_buf, 1, con.reply_buf_size, f);
        if (numRead <= 0) {
            fclose(f);
            con.reset();
			static char _500[] = ERROR_TEXT("500 Internal server error");
            *err = _500;
            if (o_http_code != NULL) {
                *o_http_code = 500;
            }
            return False;
        }
        if (!con.sock->write(con.reply_buf, numRead)) { 
            fclose(f);
            delete con.sock;
            con.sock = NULL;
            return True;
        }
        totalRead += numRead;
    }
    fclose(f);
    if (!keepConnectionAlive) {
        delete con.sock;
        con.sock = NULL;
        return True;
    }
    return False;
}


/*****************************************************************************\
SetMimeTyper - Set or clear static mime types
-------------------------------------------------------------------------------
This function allows a subclass to override the mime-types table.
A subclass passes a new mimeTyper instance to this function.
Passing NULL to this function clears the mime type.
\*****************************************************************************/

void HTTPapi::SetMimeTyper(mime_typer *iMimeTyper)
{
  Mmime_typer = iMimeTyper;
}


/*****************************************************************************\
handlePut - handle a PUT method
-------------------------------------------------------------------------------
This overridable function is called when an HTTP request arrives as a PUT
method.  Overrides of this function should stream whatever content they wish
into `con', do whatever they wish with the content buffer passed into this
function, then call `con.reply(true)', then `return keepConnectionAlive'.

This function is responsible for seeing that the buffer `content' is freed.

The default implementation indicates that this method is not allowed.

con           specifies the connection from which the HTTP request originated
              and to which any response should be streamed
header        points to a string containing the NULL-terminated header from the
              HTTP request
host          specifies the name of the server host to which the HTTP request
              was directed
content       points to an allocated buffer containing the body content of the
              HTTP PUT request; this function becomes the owner of this buffer
              (it is responsible for freeing the buffer or making sure someone
              frees it)
contentLength specifies the size (in bytes) of the body content buffer
\*****************************************************************************/

boolean HTTPapi::handlePut(WWWconnection& con, char* header, const char* host,
                           big_body& content)
{
    con.append("HTTP/1.1 405 Method not allowed\r\n");
    return con.reply(keepConnectionAlive);
}

boolean HTTPapi::handlePut(WWWconnection& con, char* header, const char* host,
                           char* content, nat4 contentLength)
{
    delete content;
    con.append("HTTP/1.1 405 Method not allowed\r\n");
    return con.reply(keepConnectionAlive);
}


/*****************************************************************************\
handleRequest - handle a GET with parameters or a POST
-------------------------------------------------------------------------------
This overridable function is called when an HTTP request arrives as a POST
method or as a GET method with parameters.  Overrides of this function should
stream whatever content they wish into `con', then call `con.reply(true)',
then `return keepConnectionAlive'.

The default implementation dispatches requests to a function in this object's
dispatcher table, based on the value of the `page' parameter.  An error is
reported across the connection if a corresponding page function is not found
or if an internal server error occurs.

con    specifies the connection from which the HTTP request originated and
       to which any response should be streamed
header points to a string containing the header from the HTTP request
url    points to the requested URL, or NULL if the request was made using the
       POST method
begin  points to the first character of the HTTP request parameter list
end    points to the last character of the HTTP request parameter list
host   specifies the name of the server host to which the HTTP request was
       directed
result currently unused; overrides should set to true before exiting
\*****************************************************************************/

boolean HTTPapi::handleRequest(WWWconnection& con, char* header, char* url,
                               char* begin, char* end, const char* host,
                               boolean& result)
{
    char buf[64];
    char ch = *end;
    char* page = con.unpack(begin, end - begin);
    if (page != NULL)  { 
        con.writeHTTPheader(keepConnectionAlive);
        sprintf(buf, "http://%s/cgistub", host);
        con.stub = buf;
        result = dispatch(con, page);
        con.reply(true, true);
        *end = ch;
        return result && keepConnectionAlive;
    } else { 
        *end = ch;
        return handleNonPageRequest(con, header, url, begin, end, host, result);
    }
}


/*****************************************************************************\
handleNonPageRequest - handle a GET or a POST with parameters but no page attr.
-------------------------------------------------------------------------------
This overridable function is called when an HTTP request arrives as a POST
method or as a GET method with parameters, but no "page" parameter.  Overrides
of this function should stream whatever content they wish into `con', then call
`con.reply(true)', then `return keepConnectionAlive'.

The default implementation reports an error across the connection that a
corresponding page function was not found.

con    specifies the connection from which the HTTP request originated and
       to which any response should be streamed
header points to a string containing the header from the HTTP request
url    points to the requested URL, or NULL if the request was made using the
       POST method
begin  points to the first character of the HTTP request parameter list
end    points to the last character of the HTTP request parameter list
host   specifies the name of the server host to which the HTTP request was
       directed
result currently unused; overrides should set to true before exiting
\*****************************************************************************/

boolean HTTPapi::handleNonPageRequest(WWWconnection& con, char*, char*, char*,
                                      char*, const char*, boolean& result)
{
    con.append(ERROR_TEXT("406 Not acceptable"));
    result = True;
    con.sock->write(con.reply_buf, con.reply_buf_used);
    return False;
}


/*****************************************************************************\
logPageServed - provide subclasses the chance to log that a webpage was served
-------------------------------------------------------------------------------
This overridable function is called when the HTTPapi web server has
successfully served a requested file to a web client.  Subclasses may override
this function to log webpage requests for statistical analysis.

This default implementation does nothing - it simply serves as a placeholder.

con       specifies the connection across which the specified file was streamed
header    specifies the request's HTTP header
file      specifies the path to the file that was served across the connection
file_size specifies the size of the file (in bytes) that was served
http_code specifies the HTTP return code that either designates a successful
          HTTP request service (i.e., 200 for "200 OK") or an HTTP error code
          (i.e., 404 for "404 Not Found")
\*****************************************************************************/

void HTTPapi::logPageServed(WWWconnection& con, char* header, char* file,
                            size_t file_size, nat2 http_code)
{
}


//----------------------------------------------------

void task_proc QueueManager::handleThread(void* arg)
{
    ((QueueManager*)arg)->handle();
}


QueueManager::QueueManager(void)
    : connectionPool(NULL), start(cs), done(cs), threads(NULL)
{
}


QueueManager::QueueManager(WWWapi& api, 
                           int     nThreads, 
                           int     connectionQueueLen,
                           size_t  threadStackSize)
                           : start(cs), done(cs)
{
    run(api, nThreads, connectionQueueLen, threadStackSize);
}


void QueueManager::run(WWWapi& api, 
                       int     nThreads, 
                       int     connectionQueueLen,
                       size_t  threadStackSize)
{
    boolean connected = false;
    // Allocate the thread pool.  Each thread will enter the handle() function
    // and wait their turn to handle a connection.
    assert(nThreads >= 1 && connectionQueueLen >= 1);
    this->nThreads = nThreads;
    threads = new task*[nThreads];
    while (--nThreads >= 0) { 
        threads[nThreads] = task::create(handleThread, this, task::pri_normal,
            threadStackSize);
    }
    
    // Allocate the connection pool, and place each connection into the free
    // queue.
    connectionPool = new WWWconnection[connectionQueueLen];
    connectionPool[--connectionQueueLen].next = NULL;
    while (--connectionQueueLen >= 0) { 
        connectionPool[connectionQueueLen].next = 
            &connectionPool[connectionQueueLen+1];
    }
    freeList = connectionPool;
    waitList = NULL;
    
    // This loop grabs the next free connection, and waits for an incoming
    // request.  Once a request is received, the connection is placed in the
    // wait queue so a thread (waiting in handle()) can serve it.  The critical
    // section is used to maintain the integrity of the wait and free queues.
    server = &api;
    cs.enter();
    while (server != NULL) { 
        // Grab a connection from the free queue.  If there are no free
        // connections, wait until one of the handle() threads finishes serving
        // a connection's request.
        if (freeList == NULL) { 
            done.reset();
            done.wait();
            if (server == NULL) { 
                break;
            }
            assert(freeList != NULL);
        }
        WWWconnection* con = freeList;
        freeList = con->next;
        WWWapi* srv = server;
        cs.leave();
        
        // Wait for an incoming request.  If the server pointer is set to NULL
        // (presumably by another thread) then it's time to shut down the
        // QueueManager.
        connected = srv->connect(*con);
        if (server == NULL) {
            return;
        }
        
        // If the socket connected successfully, place the connection we
        // grabbed from the free queue into the wait queue and signal the next
        // thread waiting in handle() to serve the connection.
        
        cs.enter();
        if (connected) {
            con->next = waitList;
            waitList = con;
            start.signal();
        }
    }
    cs.leave();
}


void QueueManager::handle()
{
    // This critical section queues up the next thread to handle the next
    // connection inserted into the wait queue by the server thread running in
    // run().  One thread per connection.
    cs.enter();
    while (True) { 
        // Wait for the server thread (in run()) to queue a waiting connection.
        start.wait();
        
        // If server is set to NULL, then it's time to shut down the
        // QueueManager.
        WWWapi* api = server;
        if (api == NULL) { 
            break;
        }
        
        // Remove the next waiting connection from the wait queue.
        WWWconnection* con = waitList;
        assert(con != NULL);
        waitList = con->next;
        
        // Leaving this critical section allows the next thread to handle the
        // next connection in the wait queue.
        cs.leave();
        
        // Handle the connection's request.
        api->serve(*con);
        
        // Return the connection to the free queue.  The critical section is
        // used to maintain the integrity of the free queue.
        cs.enter();
        if (freeList == NULL) { 
            done.signal();
        }
        con->next = freeList;
        freeList = con;
    }
    cs.leave();
}


void QueueManager::stop() 
{
    cs.enter();
    server = NULL;
    while (--nThreads >= 0) { 
        start.signal();
    }
    done.signal();
    cs.leave();
}


QueueManager::~QueueManager()
{
    if(threads) {
        delete[] threads;
    }
    if(connectionPool) {
        delete[] connectionPool;
    }
}


END_GOODS_NAMESPACE
