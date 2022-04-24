//-< PROCSOCK.CXX >------------------------------------------------+-----------
// GOODS                     Version 1.0         (c) 2002  SETER   |
// (Generic Object Oriented Database System)                       |   *  /\v
//                                                                 |_/\__/  \__
//                        Created:     27-Dec-02    Marc Seter     |/  \/    \_
//                        Last update: 27-Dec-02    Marc Seter     |Marc Seter
//-----------------------------------------------------------------+-----------
// Provide a high-performance socket that can only transmit data between two
// threads within the same process.
//-----------------------------------------------------------------------------

#include "stdinc.h"
#include <stdlib.h>
#include <string.h>
#include "procsock.h"

BEGIN_GOODS_NAMESPACE

// This constant controls the size of this socket's buffer.  Currently, this
// is not only the amount of memory an instance uses, it also controls how much
// data can be queued for reading by this socket's peer before additional
// writes to this socket will fail (with the error code EAGAIN).
static const int kBufferSize = 64 * 1024;


// This static class variable provides access to the global singly-linked list
// of all process_sockets in this process.
process_socket* process_socket::Mfirst = NULL;

// This static class variable tracks the next port that will most likely be
// available.
static const int kFirstPort = 1024;
static const int kLastPort  = 65535;

int process_socket::MnextPort = kFirstPort;


// This static constant restricts the number of characters in a local address,
// as passed to process_socket::Create().  You may want to reduce this if your
// thread stack size is small.
static const int kMaxLocalAddressLen = 256;


// This static class variable makes access to this class' static variables
// threadsafe.
mutex process_socket::MstaticMutex;


/*****************************************************************************
process_socket - address constructor
 *****************************************************************************/

process_socket::process_socket(const char* address) :
 socket_t(),
 mAcceptNext(NULL),
 mAddress(NULL),
 mBuffer(NULL),
 mDataReadySem(),
 mErrorCode(process_socket::ok),
 mNext(NULL),
 mObjectMutex(),
 mPeer(NULL),
 mPort(0),
 mReadIndex(0),
 mWriteIndex(0)
{
 // Initialize this socket's data buffer.
 mBuffer = new char[kBufferSize];

 // Add this socket to the global list.
 critical_section cs(MstaticMutex);
 mNext  = Mfirst;
 Mfirst = this;

 // Remember this socket's port.
 const char* port = strstr(address, ":");
 if(port) {
   port++;
   mPort = atol(port);
 }
}


/*****************************************************************************
~process_socket - destructor
 *****************************************************************************/

process_socket::~process_socket()
{
 close();
 if(mAddress) {
   delete[] mAddress;
   mAddress = NULL;
 }
 if(mBuffer) {
   delete[] mBuffer;
   mBuffer = NULL;
 }

 // Remove this socket from the global list.
 critical_section cs(MstaticMutex);
 if(Mfirst == this) {
   Mfirst = mNext;
 } else {
   process_socket* prev = Mfirst;
   while(prev && (prev->mNext != this)) {
     prev = prev->mNext;
   }
   if(prev) {
     prev->mNext = mNext;
   }
 }
}


/*****************************************************************************
accept - accept incoming socket connections
-------------------------------------------------------------------------------
This function waits until a socket connect()s to this socket's port, then
returns a pointer to the connected socket.  When this socket is shut down,
this function returns NULL.
 *****************************************************************************/

socket_t* process_socket::accept()
{
 process_socket* sock = NULL;

 mDataReadySem.wait();
 if(mAcceptNext != NULL) {
   critical_section cs(mObjectMutex);
   sock = mAcceptNext;
   mAcceptNext = mAcceptNext->mAcceptNext;
 }
 return sock;
}


/*****************************************************************************
abort_input - abort the current (or next) input operation
-------------------------------------------------------------------------------
This function will abort the current (or next) input operation being performed
by this object.  This function was designed to provide a thread with the
ability to cause another thread stuck in a read operation to bail.
 *****************************************************************************/

void process_socket::abort_input(void)
{
 // TODO: implement for in-process socket.
}


/*****************************************************************************
cancel_accept - close a socket that's currently accepting incoming connections
 *****************************************************************************/

boolean process_socket::cancel_accept()
{
    critical_section cs(mObjectMutex);
    mAcceptNext = NULL;
    mDataReadySem.signal();
    return true;
}


/*****************************************************************************
clear - initialize this object to its "not connected" state
-------------------------------------------------------------------------------
This function initialize this object to its "not connected" state.  Note that
this function does not lock this object's mutex; this function assumes that
the caller is handling thread synchronization (usually by locking the mutex).
 *****************************************************************************/

void process_socket::clear()
{
 if(mAddress) {
   delete[] mAddress;
   mAddress = NULL;
 }
 mErrorCode  = ok;
 mPeer       = NULL;
 mReadIndex  = 0;
 mWriteIndex = 0;
}


/*****************************************************************************
clear_error - forget the last error that this object experienced
 *****************************************************************************/

void process_socket::clear_error()
{
 mErrorCode = ok;
}


/*****************************************************************************
get_descriptor - get this object socket descriptor
-------------------------------------------------------------------------------
This required override function returns an invalid socket descriptor.  Local
process sockets don't have real socket descriptors - they use semaphores and
memory buffers for socket communication.  That's what makes them go fast. ;)
 *****************************************************************************/

int process_socket::get_descriptor()
{
 return -1;
}


/*****************************************************************************
close - close this socket, disconnecting it from its peer
-------------------------------------------------------------------------------
This function disconnects this socket from its peer.  If the sockets close
successfully or if the socket wasn't connected in the first place, true is
returned.  Otherwise an error code is set and false is returned.  Use
get_error_text() to retrieve the reason why the failure occurred. 
 *****************************************************************************/

boolean process_socket::close()
{
 if(mPeer) {
   critical_section cs(mObjectMutex);
   critical_section pcs(mPeer->mObjectMutex);
   mPeer->clear();
   clear();
 }
 return true;
}


/*****************************************************************************
compareRangeTo - determine where that index is relative to that range
-------------------------------------------------------------------------------
This function returns one of two values:

-n if the specified index (iThat) falls before the specified range
   [iStartThis .. iEndThis)
 n if the specified index (iThat) falls within the specified range
   [iStartThis .. iEndThis)

In both cases, "n" specifies the relative distance between the specified index
and the starting position of the specified range.

Note that this function treats the range as a range of bytes in a circular
buffer of size "kBufferSize".  So if (iStartThis > iEndThis), the range wraps
across the end of the buffer and back to the buffer's beginning.
 *****************************************************************************/

int process_socket::compareRangeTo(int iStartThis, int iEndThis, int iThat)
{
 if(iEndThis >= iStartThis) {
   if((iThat >= iStartThis) && (iThat < iEndThis)) {
     return iThat - iStartThis;
   } else if(iThat < iStartThis) {
     return iThat - iStartThis;
   }
   return iThat - kBufferSize - iStartThis;
 } else if(iThat >= iStartThis) {
   return iThat - iStartThis;
 } else if(iThat < iEndThis) {
   return (kBufferSize - iStartThis) + iThat;
 }
 return iThat - iStartThis;
}


/*****************************************************************************
Connect - create a socket and connect it to that address
-------------------------------------------------------------------------------
This function creates a socket and connects it to the specified address, which
should be the address of a socket that's accept()ing incoming connections.  If
an error occurs, NULL is returned.  Otherwise a pointer to the created and
connected socket is returned.
 *****************************************************************************/

process_socket* process_socket::Connect(const char* iAddress, int max_attempts,
 time_t timeout, time_t connectTimeout)
{
 critical_section cs(MstaticMutex);

 // Find the socket with the specified address.
 const char* acceptingPort = strstr(iAddress, ":");
 if (acceptingPort == NULL) {
   return NULL;
 }
 process_socket* acceptingSock = FindForPort(atol(++acceptingPort));
 while(!acceptingSock && (max_attempts-- > 0)) {
   int t = timeout;
   t += (connectTimeout != WAIT_FOREVER) ? connectTimeout : 0;
   while(!acceptingSock && (t-- > 0)) {
     MstaticMutex.leave();
     task::sleep(1);
     MstaticMutex.enter();
     acceptingSock = FindForPort(atol(acceptingPort));
   }
 }
 if(!acceptingSock) {
   return NULL;
 }

 // Create a socket for both sides of the connection.
 process_socket* thatSock = Create(iAddress);
 process_socket* thisSock = Create(iAddress);

 // Now connect the sockets.
 thatSock->mPeer = thisSock;
 thisSock->mPeer = thatSock;

 // Make the accepting socket accept the new connection.
 critical_section acs(acceptingSock->mObjectMutex);
 thatSock->mAcceptNext = acceptingSock->mAcceptNext;
 acceptingSock->mAcceptNext = thatSock;
 acceptingSock->mDataReadySem.signal();

 return thisSock;
}


/*****************************************************************************
Create - create a socket, bind it to that address but give it an unused port
-------------------------------------------------------------------------------
This function creates a process_socket and binds it to an unused port at the
specified address.  Note that the specified address should include a port
specification, but it will be ignored by this function (i.e., it should specify
an address like "!10.0.0.10:9007", but the "9007" will be ignored).
 *****************************************************************************/

process_socket* process_socket::Create(const char* iLocalAddress)
{
 critical_section cs(MstaticMutex);
 char             thisAddress[kMaxLocalAddressLen];
 char*            thisPort;

 // Find an unused port.
 int tries = 0;
 while(FindForPort(MnextPort) != NULL) {
   MnextPort++;
   if(MnextPort > kLastPort) {
     MnextPort = kFirstPort;
   }
   if(++tries > kLastPort - kFirstPort) {
     return NULL;
   }
 }

 // Create a socket, using the unused port.
 strcpy(thisAddress, iLocalAddress);
 thisPort = strstr(thisAddress, ":");
 if(!thisPort) {
   return NULL;
 }
 sprintf(++thisPort, "%d", MnextPort++);
 if(MnextPort > kLastPort) {
   MnextPort = kFirstPort;
 }
 return new process_socket(thisAddress);
}


/*****************************************************************************
FindForPort - find the process_socket connected to that port
-------------------------------------------------------------------------------
This function returns a pointer to the process_socket that's connected to the
specified port, or NULL if there isn't one.
 *****************************************************************************/

process_socket* process_socket::FindForPort(int iPort)
{
 critical_section cs(MstaticMutex);
 process_socket* sock = Mfirst;
 while(sock && (sock->mPort != iPort)) {
   sock = sock->mNext;
 }
 return sock;
}


/*****************************************************************************
get_error_text - copy a string describing the last error that occurred to that
-------------------------------------------------------------------------------
This function copies a string describing the last error that occurred to the
specified buffer.  No more than the specified number of bytes of the error
message are copied.
 *****************************************************************************/

void process_socket::get_error_text(char* buf, size_t buf_size)
{
 const char* msg;
 switch(mErrorCode) {
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
     msg = strerror(mErrorCode);
 }
 strncpy(buf, msg, buf_size);
}


/*****************************************************************************
get_peer_name - get the name of this socket's peer connection
-------------------------------------------------------------------------------
This function returns a pointer to a string that describes this socket's peer
IP address and port.  Note that this function is not threadsafe.
 *****************************************************************************/

char* process_socket::get_peer_name(nat2*)
{
	static char empty_str[] = "";
	if (!mPeer || !mPeer->mAddress) {
		return empty_str;
	}
	return mPeer->mAddress;
}


/*****************************************************************************
is_ok - determine if this socket is okay
 *****************************************************************************/

boolean process_socket::is_ok()
{
 return (mErrorCode == ok);
}


/*****************************************************************************
getRangesSize - get the number of bytes in that range, given a circular buffer
 *****************************************************************************/

int process_socket::getRangesSize(int iStart, int iEnd)
{
 // Easy case for a circular buffer - size the contiguous byte range.
 if(iEnd >= iStart)
   return (iEnd - iStart);

 // Otherwise the buffer wraps across the end of the buffer - it's split into
 // two pieces.
 return ((kBufferSize - iStart) + iEnd);
}


/*****************************************************************************
read - read (min <= n <= max) bytes into that buffer, timing out after that
-------------------------------------------------------------------------------
This function copies up to "max_size" bytes but not fewer than "min_size" bytes
into the specified buffer from this socket, timing out after "timeout" seconds.
If an error occurs, -1 is returned.  Otherwise the number of bytes read is
returned.  If the operation times out, the number of bytes read so far is
returned.
 *****************************************************************************/

int process_socket::read(void* buf, size_t min_size, size_t max_size,
 time_t timeout)
{
 boolean done      = false;
 size_t  size      = 0;

 // Fail quickly if we're not connected.
 if(!mPeer) {
   mErrorCode = not_opened;
   return -1;
 }

 // Continue reading until we've read at least the minimum number of bytes.
 while(!done) {
   // Wait until there's something to read.
   if(getRangesSize(mReadIndex, mWriteIndex) == 0) {
     time_t startTime = time(NULL);
     if(timeout == WAIT_FOREVER) {
       mDataReadySem.wait();
     } else if(!mDataReadySem.wait_with_timeout(timeout)) {
         return int(size);
     }
     if(timeout != WAIT_FOREVER) {
       timeout -= time(NULL) - startTime;
     }
   }

   // Lock this socket object, since this operation will be fiddling with it.
   {
     critical_section cs(mObjectMutex);

     // Figure out how many bytes we can read.
     int numToRead = int(max_size - size);
     if(getRangesSize(mReadIndex, mWriteIndex) < numToRead) {
       numToRead = getRangesSize(mReadIndex, mWriteIndex);
     }

     // Update the count of bytes we've read and figure out if it's enough.
     size += numToRead;
     done = (size >= min_size);

     // Copy the byte range, splitting the copy in two if the range wraps
     // around the buffer.
     if(mReadIndex + numToRead > kBufferSize) {
       int len = kBufferSize - mReadIndex;
       memcpy(buf, &mBuffer[mReadIndex], len);
       mReadIndex  = 0;
       buf         = (void*)((char*)buf + len);
       numToRead  -= len;
     }

     // Copy the entire buffer, or the second half of the two-step copy.
     memcpy(buf, &mBuffer[mReadIndex], numToRead);
     mReadIndex += numToRead;
   }
 }
 return int(size);
}


/*****************************************************************************
read - read that number of bytes into that buffer
-------------------------------------------------------------------------------
This function reads the specified number of bytes from this socket and copies
them into the specified destination buffer.  If an error occurs, an error
code is set and false is returned.  Otherwise true is returned.
 *****************************************************************************/

boolean process_socket::read(void* buf, size_t size)
{
 return (read(buf, size, size, WAIT_FOREVER) == (int)size);
}


/*****************************************************************************
shutdown - shutdown this socket
-------------------------------------------------------------------------------
This function does the same thing as closing this socket.  Probably not 100%
emulation of a "real" socket, but it should do in a pinch.
 *****************************************************************************/

boolean process_socket::shutdown()
{
 return close();
}


/*****************************************************************************
write - send that data to this socket's peer
-------------------------------------------------------------------------------
This function sends the specified array of bytes to this socket's peer.  If an
error occurs, the error code is stored in this object and false is returned;
otherwise true is returned.
 *****************************************************************************/

boolean process_socket::write(void const* buf, size_t size)
{
 if(!mPeer) {
   mErrorCode = not_opened;
   return false;
 }
    
 // Lock the peer socket object, since this operation will be fiddling
 // with it.  It's good to be threadsafe.
 critical_section pcs(mPeer->mObjectMutex);

 // If the peer's socket hasn't been read and its incoming buffer will be
 // filled by this write, fail.
 if(compareRangeTo(mPeer->mWriteIndex,
                   int(mPeer->mWriteIndex + size % kBufferSize), mPeer->mReadIndex) > 0) {
     mErrorCode = buffer_full;
     return false;
 }

 // If this write will wrap over the end of the buffer, copy in two steps.
 if(mPeer->mWriteIndex + (int)size > kBufferSize) {
   int len = kBufferSize - mPeer->mWriteIndex;
   memcpy(&mPeer->mBuffer[mPeer->mWriteIndex], buf, len);
   mPeer->mWriteIndex = 0;
   buf   = (void*)((char*)buf + len);
   size -= len;
 }

 // Copy the entire buffer, or the second half of the two-step copy.
 memcpy(&mPeer->mBuffer[mPeer->mWriteIndex], buf, size);
 mPeer->mWriteIndex += int(size);
 mPeer->mDataReadySem.signal();

 return true;
}

END_GOODS_NAMESPACE
