// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< MULTFILE.H >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     15-Apr-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 25-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// File scattered to several segments
//-------------------------------------------------------------------*--------*

#ifndef __MULTFILE_H__
#define __MULTFILE_H__


#include "osfile.h"

BEGIN_GOODS_NAMESPACE

//
// File consisting of several physical file segments
//

class multifile : public file { 
    struct segment {
        const char* name;
        fsize_t     size; 
    };
  protected:
    segment* segments;
    os_file* files;
    int      n_files;
    mutex    cs;
    fsize_t  total_size; // total size of all segments
    fposi_t  cur_pos; 
    char*    descriptor_name;

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
    
    virtual file*       clone();

    virtual void        dump();

    multifile(int n_segments, segment* segments);
    multifile(char const* descriptor_file);
    virtual ~multifile();
};
END_GOODS_NAMESPACE

#endif

