// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
#ifndef __MMAPFILE_H__
#define __MMAPFILE_H__

#include "goodsdlx.h"
#include "osfile.h"

BEGIN_GOODS_NAMESPACE

class GOODS_DLL_EXPORT mmap_file : public os_file { 
  protected:
    descriptor_t md; 
    size_t       init_size; // initial size of memory map for created file
    size_t       mmap_size;
    char*        mmap_addr;

  public:
    char*              get_mmap_addr() const { return mmap_addr; }
    size_t             get_mmap_size() const { return mmap_size; }

    virtual iop_status get_size(fsize_t& size) const;
    virtual iop_status set_size(fsize_t new_size);
    virtual iop_status set_size(fsize_t min_size, fsize_t max_size);

    virtual iop_status flush();
    virtual iop_status close();
    virtual iop_status open(access_mode mode, int flags);

    virtual file*      clone();

    mmap_file(const char* name, size_t init_size) : os_file(name) {
        mmap_addr = NULL;
        this->init_size = init_size; 
    }
};

END_GOODS_NAMESPACE

#endif
