// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< UNISOCK.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      27-Dec-02    Marc Seter    * / [] \ *
//                          Last update:  27-Dec-02    Marc Seter    * GARRET *
//-----------------------------------------------------------------+-----------
// Provide a high-performance socket that can only transmit data between two
// threads within the same process.
//-----------------------------------------------------------------------------

#ifndef __PROCSOCK_H__
#define __PROCSOCK_H__

#include "sockio.h"

BEGIN_GOODS_NAMESPACE

class GOODS_DLL_EXPORT process_socket : public socket_t { 
 friend class async_event_manager; 
 protected: 
   process_socket* mAcceptNext;   // linked list of accepted connected sockets
   char*           mAddress;      // host address
   char*           mBuffer;       // incoming data buffer
   semaphore       mDataReadySem; // gets signalled when data can be read
   int             mErrorCode;    // error code of last failed operation 
   process_socket* mNext;         // global singly-linked list
   mutex           mObjectMutex;  // locks this entire object
   process_socket* mPeer;         // peer process_socket
   int             mPort;         // this socket's port
   int             mReadIndex;    // index into buffer where next read starts
   int             mWriteIndex;   // index into buffer where next write starts

   static process_socket* Mfirst;
   static int             MnextPort;
   static mutex           MstaticMutex;

   enum error_codes { 
     ok                  =  0,
     not_opened          = -1,
     bad_address         = -2,
     connection_failed   = -3,
     broken_pipe         = -4, 
     invalid_access_mode = -5,
     buffer_full         = -6
   };

   //--- protected utilities -----------------------------------
   void clear();
   int  compareRangeTo(int iStartThis, int iEndThis, int iThat);
   int  getRangesSize(int iStart, int iEnd);

   //--- static protected utilities ----------------------------
   static process_socket* FindForPort(int iPort);

 public: 
   //--- socket_t required overrides ---------------------------
   int       read(void* buf, size_t min_size, size_t max_size,time_t timeout);
   boolean   read(void* buf, size_t size);
   boolean   write(void const* buf, size_t size);

   boolean   is_ok(); 
   boolean   shutdown();
   boolean   close();
   void      clear_error();
   int       get_descriptor();
   void      get_error_text(char* buf, size_t buf_size);
   char*     get_peer_name(nat2 *oPort = NULL); 
   void      abort_input();

   socket_t* accept();
   boolean   cancel_accept();

   //--- static utilities --------------------------------------
   static process_socket* Connect(const char* iAddress, int max_attempts,
                                  time_t timeout, time_t connectTimeout);
   static process_socket* Create(const char* iLocalAddress);

   //--- constructors & destructor -----------------------------
   process_socket(const char* address);
   ~process_socket();
};

END_GOODS_NAMESPACE

#endif
