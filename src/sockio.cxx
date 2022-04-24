// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
/**< SOCKIO.CXX >*************************************************************
   GOODS                     Version 1.0         (c) 2000  SETER   |
   (Generic Object Oriented Database System)                       |   *  /\
                                                                   |_/\__/  \_
                          Created:     01-May-00    Marc Seter     |/  \/    \
                          Last update: 09-Jun-00    Marc Seter     |Marc Seter
-------------------------------------------------------------------+-----------
Provide convenience functions for manipulating socket connections, including
the ability to retrieve web content from an HTTP/1.1 or greater server.
 *****************************************************************************/

#include "stdinc.h"
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "stdinc.h"
#include "bigbody.h"
#include "osfile.h"
#include "sockio.h"

BEGIN_GOODS_NAMESPACE

nat8 socket_t::kBrokenBigBody = CONST64(0xFFFFffffFFFFffff);


static const int  kDefChunkLen      = 16 * 1024;
static const nat4 kMaxServerNameLen = 200;

socket_t::bigBodyFn  socket_t::MbigBodyFn        = NULL;
socket_t::progressFn socket_t::MprogressFn       = NULL;
time_t               socket_t::MprogressInterval = WAIT_FOREVER;

socket_t::socket_t()
{ 
    state = ss_close;
    mProgressFn = MprogressFn;
    mProgressInterval = MprogressInterval;
}

/*****************************************************************************
GetServerFromURL - get the server name (and port) from the specified URL
-------------------------------------------------------------------------------
This function copies the name of the server, and the port number, from the
specified URL.  If a port is not specified, :80 (the default port for HTTP
operations) is appended to the server name.  If an error occurs, False is
returned; otherwise True is returned.

iURL    specifies a fully-qualified URL (i.e., "http://www.asktux.com/index.
        html") from which the server name is to be copied
oServer points to a string into which this function will copy the server name
        and port; given the above URL example, this string would contain
        "www.asktux.com:80"
 *****************************************************************************/

boolean socket_t::GetServerFromURL(const char* iURL, char* oServer)
{
    const char *port;
    const char *s;

    if(strincmp(iURL, "https", 5) == 0)
        port = ":443";
    else
        port = ":80";

    while(*iURL && (*iURL != ':'))
        iURL++;
    if(!*iURL)
        return False;
    iURL++;
    while(*iURL && (*iURL == '/'))
        iURL++;
    s = iURL++;
    while(*iURL && (*iURL != '/'))
        iURL++;
    strncpy(oServer, s, iURL - s);
    oServer[iURL - s] = 0;
    if(!strstr(oServer, ":"))
        strcat(oServer, port);

    return True;
}


/*****************************************************************************
GetURL - get a buffer containing the HTTP content at the specified URL
-------------------------------------------------------------------------------
This function retrieves the HTTP content from the specified URL, and returns
a buffer containing the content.  The returned buffer must be freed by the
caller.  If an error occurs, NULL is returned.

iURL           specifies a fully-qualified URL used to retrieve the HTTP
               content; for example, "http://www.asktux.com/index.html"
oLength        refers to an integer into which this function copies the length
               of the buffer to which a pointer is returned
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAddToHeader   if non-NULL, specifies additional HTTP request header lines.
               Each line in this string must end with "\r\n" (including the
               last line).  Helpful if the caller needs to include cookies in
               the HTTP request, keep the HTTP connection alive, etc.
 *****************************************************************************/

char *socket_t::GetURL(const char* iURL, int &oLength, int iRedirects,
 char** oHeader, const char* iAddToHeader)
{
    return ToURL("GET", iURL, NULL, 0, oLength, iRedirects, oHeader);
}


/*****************************************************************************
getURL - get a buffer containing the HTTP content at the specified URL
-------------------------------------------------------------------------------
This function retrieves the HTTP content from the specified URL, and returns
a buffer containing the content.  The returned buffer must be freed by the
caller.  If an error occurs, NULL is returned.

iServer        specifies the HTTP server from which HTTP content is to be
               retrieved; for example, "www.asktux.com"
iURL           specifies the file or resource to retrieve from the HTTP
               server; for example, "/index.html"
oLength        refers to an integer into which this function copies the length
               of the buffer to which a pointer is returned
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAddToHeader   if non-NULL, specifies additional HTTP request header lines.
               Each line in this string must end with "\r\n" (including the
               last line).  Helpful if the caller needs to include cookies in
               the HTTP request, keep the HTTP connection alive, etc.
 *****************************************************************************/

char *socket_t::getURL(const char* iServer, const char* iURL, int &oLength,
 int iRedirects, char** oHeader, const char* iAddToHeader)
{
    return toURL("GET", iServer, iURL, NULL, 0, oLength, iRedirects, oHeader,
                 iAddToHeader);
}


/*****************************************************************************
PostToURL - post HTTP data and get the buffer containing the HTTP response
-------------------------------------------------------------------------------
This function posts data to a specified URL, and retrieves a buffer containing
the HTTP content response from the post operation.  The returned buffer must be
freed by the caller.  If an error occurs, NULL is returned.

iURL           specifies a fully-qualified URL used to which the POST operation
               is performed; for example,
               "http://www.asktux.com/cgi-bin/register.pl"
iContent       points to the buffer of HTTP data to be posted
iContentLength specified the size of the `iContent' buffer, in bytes
oLength        refers to an integer into which this function copies the length
               of the buffer (to which a pointer is returned)
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAddToHeader   if non-NULL, specifies additional HTTP request header lines.
               Each line in this string must end with "\r\n" (including the
               last line).  Helpful if the caller needs to include cookies in
               the HTTP request, keep the HTTP connection alive, etc.
 *****************************************************************************/

char *socket_t::PostToURL(const char* iURL, const char *iContent,
 int iContentLength, int &oLength, int iRedirects, char** oHeader,
 const char* iAddToHeader)
{
    return ToURL("POST", iURL, iContent, iContentLength, oLength, iRedirects,
                 oHeader);
}


/*****************************************************************************
postToURL - post HTTP data and get the buffer containing the HTTP response
-------------------------------------------------------------------------------
This function posts HTTP content to the specified URL, and returns
a buffer containing the response.  The returned buffer must be freed by the
caller.  If an error occurs, NULL is returned.

iServer        specifies the HTTP server to which HTTP content is to be posted;
               for example, "www.asktux.com"
iURL           specifies the URL to which to post on the HTTP server; for
               example, "/cgi-bin/register.pl"
oLength        refers to an integer into which this function copies the length
               of the buffer to which a pointer is returned
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAddToHeader   if non-NULL, specifies additional HTTP request header lines.
               Each line in this string must end with "\r\n" (including the
               last line).  Helpful if the caller needs to include cookies in
               the HTTP request, keep the HTTP connection alive, etc.
 *****************************************************************************/

char *socket_t::postToURL(const char* iServer, const char* iURL,
 const char *iContent, int iContentLength, int &oLength, int iRedirects,
 char** oHeader, const char* iAddToHeader)
{
    return toURL("POST", iServer, iURL, iContent, iContentLength, oLength,
                 iRedirects, oHeader, iAddToHeader);
}


/*****************************************************************************
PutToURL - put HTTP data and get the buffer containing the HTTP response
-------------------------------------------------------------------------------
This function puts data to a specified URL, and retrieves a buffer containing
the HTTP content response from the put operation.  The returned buffer must be
freed by the caller.  If an error occurs, NULL is returned.

iURL           specifies a fully-qualified URL used to which the PUT operation
               is performed; for example,
               "http://www.asktux.com/cgi-bin/register.pl"
iContent       points to the buffer of HTTP data to be put
iContentLength specified the size of the `iContent' buffer, in bytes
oLength        refers to an integer into which this function copies the length
               of the buffer (to which a pointer is returned)
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAddToHeader   if non-NULL, specifies additional HTTP request header lines.
               Each line in this string must end with "\r\n" (including the
               last line).  Helpful if the caller needs to include cookies in
               the HTTP request, keep the HTTP connection alive, etc.
 *****************************************************************************/

char *socket_t::PutToURL(const char* iURL, const char *iContent,
 int iContentLength, int &oLength, int iRedirects, char** oHeader,
 const char* iAddToHeader)
{
    return ToURL("PUT", iURL, iContent, iContentLength, oLength, iRedirects,
                 oHeader);
}


/*****************************************************************************
putToURL - put HTTP data and get the buffer containing the HTTP response
-------------------------------------------------------------------------------
This function puts HTTP content to the specified URL, and returns
a buffer containing the response.  The returned buffer must be freed by the
caller.  If an error occurs, NULL is returned.

iServer      specifies the HTTP server to which HTTP content is to be put;
             for example, "www.asktux.com"
iURL         specifies the URL to which to put on the HTTP server; for example,
             "/cgi-bin/register.pl"
oLength      refers to an integer into which this function copies the length of
             the buffer to which a pointer is returned
iRedirects   specifies the number of HTTP re-directs this function should
             automatically follow; this is a good number to limit (ten is a
             good number) in case some foolish webmaster makes an infinite loop
             out of HTTP redirects
oHeader      if non-NULL, points to a pointer that will point to the HTTP
             header associated with the retrieved content.  This buffer must be
             freed by the caller.
iAddToHeader if non-NULL, specifies additional HTTP request header lines.
             Each line in this string must end with "\r\n" (including the
             last line).  Helpful if the caller needs to include cookies in
             the HTTP request, keep the HTTP connection alive, etc.
 *****************************************************************************/

char *socket_t::putToURL(const char* iServer, const char* iURL,
 const char *iContent, int iContentLength, int &oLength, int iRedirects,
 char** oHeader, const char* iAddToHeader)
{
    return toURL("PUT", iServer, iURL, iContent, iContentLength, oLength,
                 iRedirects, oHeader, iAddToHeader);
}


/*****************************************************************************
ToURL - perform any HTTP method to a HTTP/1.1 or greater web server
-------------------------------------------------------------------------------
This function performs the specified HTTP method using the optional specified
content to the specified URL, and returns a buffer containing the response.
The returned buffer must be freed by the caller.  If an error occurs, NULL is
returned.

iMethod        specifies the HTTP method (usually GET, POST, or PUT)
iURL           specifies a fully-qualified URL used to which the PUT operation
               is performed; for example,
               "http://www.asktux.com/cgi-bin/register.pl"
iContent       if non-NULL, specifies the content that will be sent to the web
               server immediately after the HTTP header is sent
iContentLength specifies the number of bytes in the buffer specified by the
               `iContent' argument
oLength        refers to an integer into which this function copies the length
               of the buffer to which a pointer is returned
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAddToHeader   if non-NULL, specifies additional HTTP request header lines.
               Each line in this string must end with "\r\n" (including the
               last line).  Helpful if the caller needs to include cookies in
               the HTTP request, keep the HTTP connection alive, etc.
 *****************************************************************************/

char *socket_t::ToURL(const char* iMethod, const char* iURL,
 const char *iContent, int iContentLength, int &oLength, int iRedirects,
 char** oHeader, const char* iAddToHeader)
{
    char       *result;
    char        server[kMaxServerNameLen];
    socket_t   *sock;

    if (oHeader) {
        *oHeader = NULL;
    }
    if(!GetServerFromURL(iURL, server))
        return NULL;
    if(!(sock = socket_t::connect(server)))
        return NULL;

    int slashCount = 3;
    while(slashCount > 0) {
        if(*iURL == '/')
            slashCount--;
        if(slashCount > 0)
            iURL++;
    }

    result = sock->toURL(iMethod, server, iURL, iContent, iContentLength,
                         oLength, iRedirects, oHeader, iAddToHeader);
    sock->close();
    delete sock;

    return result;
}


/*****************************************************************************
toURL - perform any HTTP method to a HTTP/1.1 or greater web server
-------------------------------------------------------------------------------
This function performs the specified HTTP method using the optional specified
content to the specified URL, and returns a buffer containing the response.
The returned buffer must be freed by the caller.  If an error occurs, NULL is
returned.

iMethod        specifies the HTTP method (usually GET, POST, or PUT)
iServer        specifies the HTTP server to which the HTTP request will be
               sent; for example, "www.asktux.com"
iURL           specifies the URL against which the method will be performed;
               for example, "/cgi-bin/register.pl"
iContent       if non-NULL, specifies the content that will be sent to the web
               server immediately after the HTTP header is sent
iContentLength specifies the number of bytes in the buffer specified by the
               `iContent' argument
oLength        refers to an integer into which this function copies the length
               of the buffer to which a pointer is returned
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAddToHeader   if non-NULL, specifies additional HTTP request header lines.
               Each line in this string must end with "\r\n" (including the
               last line).  Helpful if the caller needs to include cookies in
               the HTTP request, keep the HTTP connection alive, etc.
 *****************************************************************************/

char *socket_t::toURL(const char* iMethod, const char* iServer,
 const char* iURL, const char *iContent, int iContentLength, int &oLength,
 int iRedirects, char** oHeader, const char* iAddToHeader)
{
    const size_t inputBufferSize = 16*1024;
    char buf[inputBufferSize];
    char contentLen[80];
    const char *closeConnection = "Connection: close\r\n";

    contentLen[0] = 0;
    if (iContent) {
        sprintf(contentLen, "Content-Length: %d\r\n", iContentLength);
    }

    if (iAddToHeader) {
        if (strstr(iAddToHeader, "\r\nConnection:")
        || (strncmp(iAddToHeader, "Connection:", 11) == 0)) {
            closeConnection = "";
        }
    }

    sprintf(buf, "%s %s HTTP/1.1\r\nHost: %s\r\n%s%s",
            iMethod, iURL, iServer, contentLen, closeConnection);
    if (iAddToHeader) {
        strcat(buf, iAddToHeader);
    }
    strcat(buf, "\r\n");
    if(!write(buf, strlen(buf))) {
        return NULL;
    }
    if(iContent && !write(iContent, iContentLength)) {
        return NULL;
    }

    return readHTTPcontent(buf, inputBufferSize, oLength, iRedirects, oHeader);
}


/*****************************************************************************
readHTTPbody - read HTTP body content that follows the specified HTTP header
-------------------------------------------------------------------------------
This function reads the HTTP body content that follows the specified HTTP
header, and returns a buffer containing the entire body contents.  If no
body content follows the header, NULL is returned.  The length of the returned
buffer is stored in the `oLength' argument.

ioBuf          specifies a buffer that can be used while content is being read;
               this function assumes that this buffer contains the HTTP header,
               formatted as a NULL-terminated string (the remainder of the
               buffer is used by this function to retrieve the HTTP body
               content)
iBufSize       specifies the size of the `ioBuf' buffer, in bytes
iSize          specifies the number of bytes read thus far into the specified
               buffer; because of the nature of TCP/IP socket reading, this is
               usually more than the size of the HTTP header (possibly
               including all of the HTTP body content)
iBody          points within the specified buffer past the HTTP header to the
               beginning of the HTTP body content
oLength        refers to an integer into which this function copies the
               length of the buffer to which a pointer is returned
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAllowFreeForm if TRUE, this function will attempt to retrieve HTTP content
               even if the header does not contain the HTTP/1.1 content
               information (Content-length or chunked); set this to TRUE if
               you're retrieving content from a stupid Microsoft IIS/4.0 web
               server
oBigPost       optional; if specified and the size of the HTTP body being read
               exceeds big_body::Mthreshold, the HTTP body's data is stored in
               a temporary file by the big_body object, oLength is set to 0 and
               NULL is returned

If specified, this socket_t object's mProgressFn function will be called
periodically to indicate progress and provide the opportunity to abort the
transfer.  See setProgressFn() for more information on this optional feature.
 *****************************************************************************/

char* socket_t::readHTTPbody(char* ioBuf, int iBufSize, int iSize, char* iBody,
 int& oLength, int iRedirects, char** oHeader, boolean iAllowFreeForm,
 big_body *oBigPost)
{
    char* data   = NULL;
    nat8  length = 0;
    char* p      = iBody;

    char* lenptr       = stristr(ioBuf, "Content-Length: ");
    char* location     = stristr(ioBuf, "Location: ");
    char* xferEncoding = stristr(ioBuf, "Transfer-Encoding: ");

    if ((location != NULL) && (iRedirects > 0)) {
        char* url    = location + 10;
        char* urlEnd = url;

        while(*urlEnd && (*urlEnd != '\n') && (*urlEnd != '\r'))
            urlEnd++;
        *urlEnd = 0;
        return GetURL(url, oLength, iRedirects - 1, oHeader);
    }

    if (xferEncoding == NULL) {
        if (lenptr != NULL) {
            sscanf(lenptr+15, "%" INT8_FORMAT "u", &length);
            if(length == 0) {
                oLength = 0;
                return NULL;
            }
            if (oBigPost && length >= big_body::Mthreshold) {
                data = NULL;
                oLength = 0;
                oBigPost->take(readHTTPbodyToTmpFile(ioBuf, p, iSize - (p -
                               ioBuf), iBufSize - (p - ioBuf), length), ioBuf);
            } else {
                data = new char[(int)length];
                oLength = iSize - (p - ioBuf);
                if (oLength > (int)length) {
                    oLength = (int)length;
                }
                memcpy(data, p, oLength);
                mContentLength  = (int)length;
                mNumTransferred = 0;
                while(oLength < (int)length) {
                    int rc = read(data + oLength, 1, (int)length - oLength,
                                  mProgressInterval);
                    if(rc == -1) {
                        delete data;
                        return NULL;
                    }
                    oLength += rc;
                    if(!reportProgress(rc)) {
                        delete data;
                        return NULL;
                    }
                }
            }
        } else if (iAllowFreeForm &&
                   ((strncmp(ioBuf, "HTTP/1.1 200 ", 13) == 0) ||
                    (strncmp(ioBuf, "HTTP/1.0 200 ", 13) == 0))) {
            data = readFreeForm(p, iSize - (p - ioBuf), oLength);
        }
    } else if (strncmp(xferEncoding + 19, "chunked", 7) == 0) {
        mContentLength  = 0;
        mNumTransferred = 0;
        if (lenptr != NULL) {
            sscanf(lenptr+15, "%d", &mContentLength);
        }
#if 0 // Here's my code: do I need the typecasts?
        data = readChunks(p, oLength, iSize - ((nat4)p - (nat4)ioBuf),
                          iBufSize - ((nat4)p - (nat4)ioBuf));
#else
        data = readChunks(p, oLength, iSize - (p - ioBuf),
                          iBufSize - (p - ioBuf));
#endif
    }

    if (oHeader) {
        *oHeader = new char[p + 1 - ioBuf];
        strcpy(*oHeader, ioBuf);
    }

    return data;
}


char* socket_t::readHTTPbodyToTmpFile(char *iHeader, char *ioBuf, int iSize,
  int iBufSize, nat8 iLength)
{
    os_file f;
    nat8    length = iSize;

    if (!f.create_temp()) {
        return NULL;
    }

    if ((iSize > 0) && (f.write(ioBuf, iSize) != file::ok)) {
        f.remove();
        return NULL;
    }

    while(length < iLength) {
        int rc = read(ioBuf, 1, iBufSize, mProgressInterval);
        if((rc == -1) || (f.write(ioBuf, rc) != file::ok) ||
           !reportProgress(rc)) {
            if((rc == -1) && (MbigBodyFn != NULL)) {
                (*MbigBodyFn)(iHeader, kBrokenBigBody, kBrokenBigBody);
            }
            f.remove();
            return NULL;
        }
        length += rc;
        if(MbigBodyFn != NULL)
            (*MbigBodyFn)(iHeader, length, iLength);
    }

    return strdup(f.get_name());
}


/*****************************************************************************\
readHTTPcontent - get the web content response for a GET or POST
-------------------------------------------------------------------------------
This function reads from this socket, expecting the response HTTP content that
follows a GET or POST method operation.  The caller is responsible for freeing
the returned buffer.  If an error occurs, NULL is returned and `*oHeader' is
set to NULL.  Otherwise, a pointer to a buffer containing the response HTTP
content is returned (which may be NULL if no content is returned), and
`oHeader' is set to point to a buffer containing the HTTP header the server
sent for the response.

ioBuf          specifies a buffer that can be used while content is being read
iBufSize       specifies the size of the `ioBuf' buffer, in bytes
oLength        refers to an integer into which this function copies the
               length of the buffer to which a pointer is returned
iRedirects     specifies the number of HTTP re-directs this function should
               automatically follow; this is a good number to limit (ten is a
               good number) in case some foolish webmaster makes an infinite
               loop out of HTTP redirects
oHeader        if non-NULL, points to a pointer that will point to the HTTP
               header associated with the retrieved content.  This buffer must
               be freed by the caller.
iAllowFreeForm if TRUE, this function will attempt to retrieve HTTP content
               even if the header does not contain the HTTP/1.1 content
               information (Content-length or chunked); set this to TRUE if
               you're retrieving content from a stupid Microsoft IIS/4.0 web
               server
 *****************************************************************************/

char *socket_t::readHTTPcontent(char *ioBuf, int iBufSize, int& oLength,
 int iRedirects, char** oHeader, boolean iAllowFreeForm)
{
    char*  data = NULL;
    char*  p;
    size_t size = 0;

    if (oHeader) {
        *oHeader = NULL;
    }

    if (!(p = readHTTPheader(ioBuf, iBufSize, size, mProgressInterval))) {
        return NULL;
    }

    data = readHTTPbody(ioBuf, iBufSize, int(size), p, oLength, iRedirects,
                        oHeader, iAllowFreeForm);

    return data;
}


/*****************************************************************************
readHTTPheader - read a pending HTTP header into the specified buffer
-------------------------------------------------------------------------------
This function reads the HTTP header currently queued for reading on this
socket.  If an error occurs, NULL is returned.  If the HTTP header contains a
bad request or the HTTP header read operation times out, the appropriate
response is streamed across the socket and a pointer to the buffer is returned
(no header is read).  Otherwise if the read succeeds, a pointer to the first
byte of HTTP content past the HTTP header is returned, ioSize is updated to
the total number of bytes read, and ioBuffer will contain the NULL-terminated
HTTP header.

If the socket timeout is reached, this function returns NULL.  (If a progress
function is set and the timeout is reached, the progress function will be
called.  The progress function can then choose whether or not to abort the
entire socket read operation by returning FALSE to this function).

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
iTimeout if this socket has no progress function set, this value specifies the
         socket's timeout value (in seconds) - if unspecified, this function
         will wait forever for the HTTP header to be read
         if this socket has a progress function set, this value specified the
         maximum number of seconds between calls to the progress function
 *****************************************************************************/

char* socket_t::readHTTPheader(char* ioBuf, size_t iBufSize, size_t &ioSize,
 time_t iTimeout)
{
    nat4   countCR    = 0;
    nat4   countLF    = 0;
    time_t end;
    time_t now;
    char*  p          = ioBuf;
    bool   pastHeader = false;
    int    rc;

    if (iTimeout != WAIT_FOREVER) {
        time(&now);
        end = now + iTimeout;
    }
    ioSize = 0;
    while (!pastHeader) {
        // Read as much into our buffer as the server is willing to provide.
        // Because of transmission lag, there may only be a portion of the
        // content available for reading at this time.
        if (p == ioBuf + ioSize) {
            rc = read(ioBuf + ioSize, 1, iBufSize - ioSize - 1, iTimeout);
            // If a socket read error occurred, bail.
            if (rc < 0) {
                return NULL;
            }
            // If the socket timeout was reached and there's no progress
            // function to handle timeouts, bail.
            if ((mProgressFn == NULL) && (iTimeout != WAIT_FOREVER)) {
                time(&now);
                if (now >= end) {
                    return NULL;
                }
                iTimeout = end - now;
            }            
            ioSize += rc;

            // We're going to report progress, but since we're reading the
            // HTTP header and not the actual content we won't report that
            // content was read.
            if(!reportProgress(0)) {
                return NULL;
            }
        }
        ioBuf[ioSize] = '\0';

        // Determine if we've read the entire HTTP header yet or if we have
        // to go for another read() call.
        while (!pastHeader && (p < ioBuf + ioSize)) {
            if (*p == '\n') {
                countLF++;
            } else if (*p == '\r') {
                countCR++;
            } else if ((*p != ' ') && (*p != '\t')) {
                countLF = 0;
                countCR = 0;
            }

            if ((countCR == 2) && (countLF == 0)) {
                pastHeader = true;
            } else if ((countCR == 0) && (countLF == 2)) {
                pastHeader = true;
            } else if ((countCR == 2) && (countLF == 2)) {
                pastHeader = true;
            }

            p++;
        }
    }
    char* q = p - 1;
    while (((*q == '\n') || (*q == '\r') || (*q == ' ') || (*q == '\t')) &&
           (q > ioBuf)) {
      q--;
    }
    *(q+1) = '\n';
    *(q+2) = '\0';
    return p;
}


/*****************************************************************************
reportProgress - report that more bytes have been transferred
-------------------------------------------------------------------------------
This function reports to the progress function that more bytes have been
transferred, if a progress function has been specified.  If the transfer
should be aborted, False is returned; otherwise this function returns True.
 *****************************************************************************/

boolean socket_t::reportProgress(nat4 iSteps)
{
 mNumTransferred += iSteps;
 if(!mProgressFn)
   return True;
 return (*mProgressFn)(mNumTransferred, mContentLength);
}


/*****************************************************************************
setProgressFn - set the function to be called periodically during HTTP requests
-------------------------------------------------------------------------------
This function sets the function to be called periodically during HTTP requests.
This function is passed two integers: the number of bytes that have been
transferred, and the total number of bytes to be transferred.  If the total
number of bytes cannot be determined, zero is passed as the total.

The progress function (to which a pointer is passed into setProgressFn()) is
also given the ability to abort the transfer.  If the progress function returns
False when it is called, this socket_t object abort whatever transfer is being
conducted and returns an error condition.  Otherwise the progress function
should return True if it wishes the socket_t to continue transferring data.

If progress should not be reported by this socket_t object, pass NULL into this
function (the default behaviour is not to report progress).
 *****************************************************************************/

void socket_t::setProgressFn(socket_t::progressFn iFn)
{
 mProgressFn = iFn;
}


/*****************************************************************************
setProgressInterval - set the max. # secs that can pass between progress calls
-------------------------------------------------------------------------------
This function sets the maximum number of seconds that this object will allow to
pass between calls to this object's progress function during read() operations.
Calling this function is only useful if this object's progress callback
function has been set (or the global socket_t::MprogressFn has been set).

iSeconds  specifies the maximum number of seconds between progress reports (via
          the function set using setProgressFn()); default value is
          WAIT_FOREVER, which causes progress to be reported only after
          progress has been made
 *****************************************************************************/

void socket_t::setProgressInterval(time_t iSeconds)
{
 mProgressInterval = iSeconds;
}


class HTTPchunk
{
 protected :
   char*      mData;
   nat4       mLength;
   HTTPchunk* mNext;

 public :
   HTTPchunk(nat4 iLength);
   virtual ~HTTPchunk(void);

   void copyTo(char* oDest);
   nat4 getLength(void);

   static nat4 ParseLength(char *iBuf);
   static HTTPchunk* Read(socket_t* ioSock, char* ioBuf, nat4 iSize,
                          nat4 iBufferSize, boolean &oError,
                          time_t iProgressInterval);
   static HTTPchunk* ReadFreeForm(socket_t* iSock);
};


HTTPchunk::HTTPchunk(nat4 iLength) :
 mData(NULL),
 mLength(iLength),
 mNext(NULL)
{
 mData = new char[iLength];
}

HTTPchunk::~HTTPchunk(void)
{
 if(mData)
   delete mData;
 if(mNext)
   delete mNext;
}

void HTTPchunk::copyTo(char* oDest)
{
 if(mData && mLength)
   memcpy(oDest, mData, mLength);
 if(mNext)
   mNext->copyTo(oDest + mLength);
}

nat4 HTTPchunk::getLength(void)
{
 return mLength + (mNext ? mNext->getLength() : 0);
}

nat4 HTTPchunk::ParseLength(char *iBuf)
{
     nat4 length = 0;

 while((*iBuf == '\n') || (*iBuf == '\r') || (*iBuf == ' '))
   iBuf++;

 while(*iBuf && (*iBuf != ' ')) {
   length <<= 4;
   if((*iBuf >= '0') && (*iBuf <= '9'))
     length += *iBuf - '0';
   else if((*iBuf >= 'a') && (*iBuf <= 'f'))
     length += *iBuf - 'a' + 10;
   else if((*iBuf >= 'A') && (*iBuf <= 'F'))
     length += *iBuf - 'F' + 10;
   else
     return 0xffffffff;
   iBuf++;
   }
 return length;
}

HTTPchunk* HTTPchunk::Read(socket_t* ioSock, char* ioBuf, nat4 iSize,
 nat4 iBufferSize, boolean &oError, time_t iProgressInterval)
{
     HTTPchunk* chunk;
     bool       foundLen = false;
     char       prev_ch = 0;
     char*      p;

 p = ioBuf;
 do {
   if(p == ioBuf + iSize) {
     int rc = ioSock->read(ioBuf + iSize, 1, iBufferSize - iSize - 1,
                           iProgressInterval);
     if(rc < 0) {
       oError = True;
       return NULL;
     }
     iSize += rc;
     if(!ioSock->reportProgress(rc)) {
       oError = True;
       return NULL;
     }
   }
   ioBuf[iSize] = '\0';
   while(*p != '\0' && (!foundLen || prev_ch != '\r' || *p != '\n')) {
     if (((*p >= '0') && (*p <= '9')) ||
         ((tolower(*p) >= 'a') && (tolower(*p) <= 'f'))) {
       foundLen = true;
     }
     prev_ch = *p++;
   }
 } while(*p == '\0' && p == ioBuf + iSize); // p now points to the content
 if(*p != '\n' || prev_ch != '\r') {
   oError = True;
   return NULL;
 }
 *(--p) = '\0';
 p += 2;

 nat4 length;
 if((length = ParseLength(ioBuf)) == 0)
   return NULL;
 if(length == 0xffffffff) {
   oError = True;
   return NULL;
 }

 chunk  = new HTTPchunk(length);
 iSize -= p - ioBuf;
 nat4 bite = iSize;
 if(bite >= length) {
   bite = length;
   memcpy(chunk->mData, p, bite);

   while((bite < iSize) && ((p[bite] == '\r') || (p[bite] == '\n')))
     bite++;
   iSize -= bite;
   memcpy(ioBuf, p + bite, iSize);
 } else {
   memcpy(chunk->mData, p, bite);
   iSize = 0;
 }

 while(bite < length) {
   int rc = ioSock->read(chunk->mData + bite, 1, length - bite,
                         iProgressInterval);
   if(rc == -1) {
     delete chunk;
     oError = True;
     return NULL;
   }
   bite += rc;
   if(!ioSock->reportProgress(rc)) {
     delete chunk;
     oError = True;
     return NULL;
   }
 }
 chunk->mNext = Read(ioSock, ioBuf, iSize, iBufferSize, oError,
                     iProgressInterval);
 return chunk;
}

HTTPchunk* HTTPchunk::ReadFreeForm(socket_t* iSock)
{
     HTTPchunk* chunk = new HTTPchunk(kDefChunkLen);
     int        dx, x = 0;
     HTTPchunk* lastChunk;

 do {
   dx = iSock->read(chunk->mData + x, 1, kDefChunkLen - x);
   if(dx <= 0) {
     if(x == 0) {
       delete chunk;
       return NULL;
     }
     lastChunk = new HTTPchunk(x);
     memcpy(lastChunk->mData, chunk->mData, x);
     delete chunk;
     return lastChunk;
   }
   x += dx;
   if(!iSock->reportProgress(dx)) {
     delete chunk;
     return NULL;
   }
 } while(x < kDefChunkLen);
 chunk->mNext = ReadFreeForm(iSock);
 return chunk;
}


char* socket_t::readChunks(char* ioBuf, int &oLength, size_t iSize,
 size_t iBufferSize)
{
     HTTPchunk* chunks;
     char*      data  = NULL;
     boolean    error = False;

     if(!(chunks = HTTPchunk::Read(this, ioBuf, int(iSize), nat4(iBufferSize), error, mProgressInterval)))
   {
   return NULL;
   }
 if(!error)
   {
   oLength = (int)chunks->getLength();
   data = new char[oLength];
   chunks->copyTo(data);
   }
 delete chunks;
 
 return data;
}


char* socket_t::readFreeForm(char* iFirstBuf, int iFirstBufLen, int &oLength)
{
     HTTPchunk* chunks;
     char*      data;

 mContentLength  = 0;
 mNumTransferred = 0;
 chunks = HTTPchunk::ReadFreeForm(this);
 oLength = iFirstBufLen;
 if(chunks)
   oLength += chunks->getLength();
 data = new char[oLength];
 memcpy(data, iFirstBuf, iFirstBufLen);
 if(chunks)
   {
   chunks->copyTo(&(data[iFirstBufLen]));
   delete chunks;
   }

 return data;
}

END_GOODS_NAMESPACE
