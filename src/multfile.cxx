// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< MULTFILE.CXX >--------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     15-Apr-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 25-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// File scattered to several segments
//-------------------------------------------------------------------*--------*

#include <stdio.h>
#include "stdinc.h"
#ifdef _WIN32
#pragma hdrstop
#endif
#include "multfile.h"

BEGIN_GOODS_NAMESPACE

#define MAX_SEGMENT_NAME_LEN 256
#define MB                   fsize_t(1024*1024)


void multifile::get_error_text(iop_status code,char* buf,size_t buf_size) const
{
    files->get_error_text(code, buf, buf_size);
}

file::iop_status multifile::set_position(fposi_t pos)
{
    cur_pos = pos;
    return ok;
}

file::iop_status multifile::get_position(fposi_t& pos)
{
    pos = cur_pos;
    return ok;
}

file::iop_status multifile::read(void* buf, size_t size)
{
    cs.enter();
    fposi_t pos = cur_pos;
    cur_pos += size;
    cs.leave();
    return read(pos, buf, size);
}

file::iop_status multifile::write(void const* buf, size_t size)
{
    cs.enter();
    fposi_t pos = cur_pos;
    cur_pos += size;
    cs.leave();
    return write(pos, buf, size);
}

file::iop_status multifile::read(fposi_t pos, void* buf, size_t size)
{
    int i = 0;
    int n  = n_files - 1;
    char* dst = (char*)buf; 

    while (True) { 
        if (pos < segments[i].size || i == n) { 
            if (pos + size <= segments[i].size || i == n) { 
                return files[i].read(pos, dst, size);
            } else {
                size_t segm_size = size_t(segments[i].size - pos);
                iop_status status = files[i++].read(pos, dst, segm_size);
                if (status == ok) { 
                    pos = 0;
                    dst += segm_size;
                    size -= segm_size;
                } else { 
                    return status;
                }
            }
        } else { 
            pos -= segments[i++].size;
        }
    }    
}

file::iop_status multifile::write(fposi_t pos, void const* buf, size_t size)
{
    int i = 0;
    int n  = n_files - 1;
    char* src = (char*)buf; 

    while (True) { 
        if (pos < segments[i].size || i == n) { 
            if (pos + size <= segments[i].size || i == n) { 
                return files[i].write(pos, src, size);
            } else {
                size_t segm_size = size_t(segments[i].size - pos);
                iop_status status = files[i++].write(pos, src, segm_size);
                if (status == ok) { 
                    pos = 0;
                    src += segm_size;
                    size -= segm_size;
                } else { 
                    return status;
                }
            }
        } else { 
            pos -= segments[i++].size;
        }
    }    
}

file::iop_status multifile::flush()
{
    for (int i = 0; i < n_files; i++) { 
        iop_status status = files[i].flush();
        if (status != ok) {
            return status; 
        }
    }
    return ok;
}

file::iop_status multifile::open(access_mode mode, int flags)
{
    if (descriptor_name != NULL) { 
        char buf[MAX_SEGMENT_NAME_LEN];
        FILE* f = fopen(descriptor_name, "r");
        if (!fgets(buf, sizeof(buf), f)) { 
            return bad_multifile_descriptor;
        }  
        if (sscanf(buf, "%d", &n_files) != 1 || n_files < 1) { 
            return bad_multifile_descriptor;
        }  
        files = NEW os_file[n_files]; 
        segments = NEW segment[n_files];
        for (int i = 0; i < n_files; i++) { 
            if (!fgets(buf, sizeof(buf), f)) { 
                return bad_multifile_descriptor;
            }  
            int seg_size, pos;
            if (sscanf(buf, "%d%n", &seg_size, &pos) != 1) { 
                return bad_multifile_descriptor;
            }
            while (buf[pos] == ' ' || buf[pos] == '\t') { 
                pos += 1;
            }
            int eol = int(strlen(buf));
            while (--eol > pos && (buf[eol] == '\r' || buf[eol] == '\n' || buf[eol] == ' '));
            if (eol <= pos) { 
                return bad_multifile_descriptor;
            }
            buf[++eol] = '\0';
            segments[i].size = seg_size*MB;
            segments[i].name = NEW char[eol-pos+1];
            memcpy((char*)segments[i].name, buf+pos, eol-pos+1);
            files[i].set_name(segments[i].name);
        }
    }
    total_size = 0;
    for (int i = 0; i < n_files; i++) { 
        iop_status status = files[i].open(mode, flags);
        if (status != ok) {
            while (--i >= 0) {
                files[i].close();
            } 
            return status;
        }
        total_size += segments[i].size;
    }
    cur_pos = 0;
    this->mode = mode;
    return ok;
}

file::iop_status multifile::close()
{
    for (int i = 0; i < n_files; i++) { 
        iop_status status = files[i].close();
        if (status != ok) { 
            return status;
        }
    }
    if (descriptor_name != NULL) { 
        for (int i = 0; i < n_files; i++) { 
            delete[] segments[i].name;
        }
        delete[] files;
        delete[] segments;
        files = NULL;
        segments = NULL;
    }
    return ok;
}

file::iop_status multifile::remove()
{
    for (int i = 0; i < n_files; i++) { 
        iop_status status = files[i].remove(); 
        if (status != ok) { 
            return status;
        }
    }
    return ok;
}

char const* multifile::get_name() const
{
    return files[0].get_name();
}

file::iop_status multifile::set_name(char const*) 
{
    for (int i = 0; i < n_files; i++) { 
        iop_status status = files[i].set_name(segments[i].name); 
        if (status != ok) { 
            return status;
        }
    }
    return ok;
}

file::iop_status multifile::get_size(fsize_t& size) const
{
    iop_status status = files[n_files-1].get_size(size);
    size += total_size - segments[n_files-1].size; 
    return status;
}

file::iop_status multifile::set_size(fsize_t new_size)
{
    if (new_size > total_size - segments[n_files-1].size) {
        return files[n_files-1].set_size
            (new_size - total_size + segments[n_files-1].size);
    } 
    return ok;
}
    
file* multifile::clone()
{
    return NEW multifile(n_files, segments); 
}

multifile::multifile(int n_segments, segment* segm)
{
    assert(n_segments > 0);
    segments = segm;
    n_files = n_segments;
    files = NEW os_file[n_files];
    descriptor_name = NULL; 
    for (int i = 0; i < n_segments; i++) { 
        files[i].set_name(segments[i].name); 
    }
}

multifile::multifile(char const* descriptor_file) 
{
    files = NULL;
    segments = NULL;
    descriptor_name = strdup(descriptor_file);
}

multifile::~multifile()
{
    delete[] files;
    delete[] descriptor_name;
}

void multifile::dump() {}

END_GOODS_NAMESPACE
