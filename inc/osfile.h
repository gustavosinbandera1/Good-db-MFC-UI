// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< OSFILE.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 15-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Interface to operation system dependent file implementation
//-------------------------------------------------------------------*--------*

#ifndef __OSFILE_H__
#define __OSFILE_H__

#include "file.h"
#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

//
// Operation system supported file
//

class GOODS_DLL_EXPORT os_file : public file { 
  protected:
    descriptor_t  fd; 
    char*         name;
    boolean       opened; // is file opened ?

    void*         os_specific; // pointer to os spewcific part of class

  public:                                              
    virtual void        get_error_text(iop_status code,  
                                       char* buf, size_t buf_size) const; 

    virtual iop_status  read(fposi_t pos, void* buf, size_t size);
    virtual iop_status  write(fposi_t pos, void const* buf, size_t size);

    virtual iop_status  get_position(fposi_t& pos); 
    virtual iop_status  set_position(fposi_t pos); 

    virtual iop_status  read(void* buf, size_t size);
    virtual iop_status  write(void const* buf, size_t size);

    virtual iop_status  open(access_mode mode, int flags);
    virtual iop_status  flush();
    virtual iop_status  close();
    virtual iop_status  remove(); 

    virtual char const* get_name() const;
    virtual iop_status  set_name(char const* new_name);

    virtual iop_status  get_size(fsize_t& size) const;
    virtual iop_status  set_size(fsize_t new_size);
    
    virtual file*       clone();

    virtual void        dump();

    //
    // Return file system system dependent size of disk block
    //
    static  size_t      get_disk_block_size();
    //
    // Allocate buffer for disk io operation aligned to disk block size
    //
    static  void*       allocate_disk_buffer(size_t size); 
    //
    // Free buffer allocated by "allocate_disk_buffer" function
    //
    static  void        free_disk_buffer(void* buf); 
    //
    // Create a file in the system temporary directory
    //
    bool create_temp(void);
    //
    // Copy a file into system temporary directory
    //
    iop_status copy_to_temp(const char *iSource, nat4 iBufSize = 64 * 1024);

    os_file(const char* name = NULL);
    ~os_file();
};

END_GOODS_NAMESPACE

#endif
