// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< SOCKFILE.H >--------------------------------------------------+-----------
// GOODS                     Version 1.0         (c) 2006  SETER   |   *
// (Generic Object Oriented Database System)                       |      /\ v
//                                                                 |_/\__/  \__
//                        Created:     03-Jan-07    Marc Seter     |/  \/    \/ //                        Last update: 03-Jan-07    Marc Seter     |Marc Seter
//-----------------------------------------------------------------+-----------
// Provide a file subclass that streams data across a socket.  Really helpful
// when you want to backup or restore the database using an archive stored on
// a remote server.
//-----------------------------------------------------------------------------

#ifndef __SOCKFILE_H__
#define __SOCKFILE_H__

#include "goodsdlx.h"
#include "file.h"

BEGIN_GOODS_NAMESPACE

class socket_t;

class GOODS_DLL_EXPORT socket_file : public file { 
  public:
    virtual void        get_error_text(iop_status code,  
                                       char* buf, size_t buf_size) const; 

    virtual iop_status  read(fposi_t pos, void* buf, size_t size);
    virtual iop_status  write(fposi_t pos, void const* buf, size_t size);

    virtual iop_status  set_position(fposi_t pos); 
    virtual iop_status  get_position(fposi_t& pos); 

    virtual iop_status  read(void* buf, size_t size);
    virtual iop_status  write(void const* buf, size_t size);

    virtual iop_status  flush();

    virtual iop_status  open(access_mode mode, int flags);
    virtual iop_status  close();
    virtual iop_status  remove(); 

    virtual char const* get_name() const;
    virtual iop_status  set_name(char const* new_name);

    virtual iop_status  get_size(fsize_t& size) const;
    virtual iop_status  set_size(fsize_t new_size);
    
    //
    // Create copy of the original file control object. 
    // Cloned file object points to the same file as the original one 
    // but is not opened.
    // Original and cloned file objects are independent. 
    // 
    virtual file*       clone();

    virtual void        dump();

    socket_file(socket_t *io_socket);
    virtual ~socket_file();

  protected: 
    socket_t* m_socket;
};

END_GOODS_NAMESPACE

#endif
