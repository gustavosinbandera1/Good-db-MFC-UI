// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< JAVAMOP.H >-----------------------------------------------------*--------*
// JavaMop                    Version 1.02       (c) 1998  GARRET    *     ?  *
// (Metaobject Protocol for Java)                                    *   /\|  *
//                                                                   *  /  \  *
//                          Created:      1-Oct-98    K.A. Knizhnik  * / [] \ *
//                          Last update: 16-Nov-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*

#ifndef __JAVAMOP_H__
#define __JAVAMOP_H__

#define JAVAMOP_STAMP "JAVA_MOP_GENERATOR_v1"

#define bool    int
#define true    (1)
#define false   (0)

typedef unsigned char  byte;
typedef unsigned short word;

enum vbm_instruction_code { 
#define JAVA_INSN(code, mnem, len, push, pop) mnem,
#include "javamop.d"
last_insn
};
    

#define MAX_STACK         256
#define MAX_CLASS_NAME    256
#define EMPTY_CONSTRUCTOR 5 // length of empty constructor byte code

inline int unpack2(byte* s) { 
    return (s[0] << 8) + s[1]; 
}

inline int unpack4(byte* s) { 
    return (((((s[0] << 8) + s[1]) << 8) + s[2]) << 8) + s[3];
} 

inline int unpack2_le(byte* s) { 
    return (s[1] << 8) + s[0]; 
}

inline int unpack4_le(byte* s) { 
    return (((((s[3] << 8) + s[2]) << 8) + s[1]) << 8) + s[0];
} 

inline byte* pack2(byte* d, int val) { 
    *d++ = byte(val >> 8);
    *d++ = byte(val);
    return d;
}

inline byte* pack4(byte* d, int val) { 
    *d++ = byte(val >> 24);
    *d++ = byte(val >> 16);
    *d++ = byte(val >> 8);
    *d++ = byte(val);
    return d;
}

extern unsigned string_hash_function(byte* p);

class utf_string {
  public:
    int   len;
    byte* data;

    bool operator == (utf_string const& str) const { 
        return len == str.len && memcmp(data, str.data, len) == 0; 
    }
    bool operator != (utf_string const& str) const { 
        return len != str.len || memcmp(data, str.data, len) != 0; 
    }
    bool operator == (const char* str) const { 
        return strcmp((char*)data, str) == 0; 
    }
    bool operator != (const char* str) const { 
        return strcmp((char*)data, str) != 0; 
    }
    unsigned hash() const { 
        return string_hash_function(data);
    }

    utf_string(int length, byte* str) { 
        len = length;
        data = new byte[length+1];
        memcpy(data, str, length);
        data[length] = 0;
    }
    utf_string(const char* str) { 
        len = strlen(str);
        data = (byte*)str;
    }
    utf_string(utf_string* s) { 
        len = s->len;
        data = s->data;
    }
    char* as_asciz() const { return (char*)data; }
};

enum const_types { 
    c_none,
    c_utf8,
    c_reserver,
    c_integer,
    c_float,
    c_long,
    c_double, 
    c_class,
    c_string,
    c_field_ref,
    c_method_ref,
    c_interface_method_ref,
    c_name_and_type
};

enum access_flags { 
    f_public       = 0x01,
    f_private      = 0x02,
    f_protected    = 0x04,
    f_static       = 0x08,
    f_final        = 0x10,
    f_synchronized = 0x20,
    f_volatile     = 0x40,
    f_transient    = 0x80,
    f_interface    = 0x200,
    f_abstract     = 0x400
};

class constant { 
  public: 
    int tag;
    constant(byte* p) { tag = *p; }
    constant(int tag) { this->tag = tag; }
};

class const_utf8 : public constant {
  public:
    utf_string str;
    const_utf8(byte* p) : constant(p), str(unpack2(p+1), p+3) {}
    const_utf8(const char* s) : constant(c_utf8), str(s) {}
    const_utf8(utf_string* s) : constant(c_utf8), str(s) {}
};

class const_class : public constant{
  public: 
    int name;
    const_class(byte* p) : constant(p) {
        name = unpack2(p+1);
    }
    const_class(int name) : constant(c_class) { 
        this->name = name;
    }
};

class const_ref : public constant {
  public:
    int cls;
    int name_and_type;

    int mop_var_index; // index in constant pool of metaobject filed reference

    const_ref(byte* p) : constant(p) {
        cls = unpack2(p+1);
        name_and_type = unpack2(p+3);
        mop_var_index = 0;
    }
    const_ref(int tag, int cls, int name_and_type) : constant(tag) { 
        this->cls = cls;
        this->name_and_type = name_and_type;
    }
};


class const_name_and_type : public constant {
  public:
    int name;
    int desc;

    const_name_and_type(byte* p) : constant(p) {
        name = unpack2(p+1);
        desc = unpack2(p+3);
    }
    const_name_and_type(int name, int desc) : constant(c_name_and_type) { 
        this->name = name;
        this->desc = desc;
    }
};

class method_desc;
class class_file;

class invocation_desc { 
  public:
    invocation_desc* next;
    method_desc*     callee;
    bool             is_virtual;

    invocation_desc(method_desc* mth, bool virt, invocation_desc* chain) { 
        callee = mth;
        next = chain;
        is_virtual = virt;
    }
};

class field_desc { 
  public:
   field_desc*       next;
   utf_string const& name;
   int               attr;

   field_desc(utf_string const& fld_name, int fld_attr, field_desc* chain)
     : next(chain), name(fld_name),  attr(fld_attr)
   {}
};
    

class method_desc { 
  public:
    method_desc*      next;
    invocation_desc*  invocations;
    method_desc*      redefinition;
    utf_string const& name;
    utf_string const& desc;
    bool              is_constructor;
  
    int               code_length;
    int               mutator;
    int               redef_mutator;
    byte*             code;

    method_desc(utf_string const& mth_name, utf_string const& mth_desc)
    : name(mth_name), desc(mth_desc)
    {
        code = NULL;
        next = NULL;
        is_constructor = (mth_name == "<init>");
        invocations = NULL;
        redefinition = NULL;
        mutator = 0;
        redef_mutator = 0;
    } 
};

#define CLASS_HASH_TABLE_SIZE 117
#define MAX_CODE_SIZE 0x10000

class class_file { 
  public:
    class_file*  next;
    class_file*  collision_chain;
    class_file*  superclass;
    static class_file* class_list;
    static class_file* hash_table[CLASS_HASH_TABLE_SIZE];

    static int   n_mop_classes;

    bool         is_archive;
    bool         marked;

    char*        file_name;
    utf_string*  class_name;
    utf_string*  super_class_name;
    int          this_class;
    int          super_class;
    int          this_class_name;
    int          code_attr_name;
    int          superclass_default_constructor;

    constant**   constant_pool;
    static constant** constant_pool_extension;
    int          constant_pool_count;

    static class_file* mop_root; 

    byte*        buffer;
    byte*        constants;
    byte*        methods;
    byte*        attributes;

    static byte  code_buffer[MAX_CODE_SIZE];
    static word  relocation_table[MAX_CODE_SIZE];
    
    method_desc* first_method;
    method_desc* last_method;
    
    field_desc*  field_list;

    int          finally_handler;
    int          finally_start;
    int          finally_end;

    int          mop_var_index; // index of metaobject variable in pool
    int          mop_pre_index; // index of preDaemon method constant in pool
    int          mop_post_index;// index of postDaeom method constant in pool
    class_file*  mop;

    void         prepare();
    void         preprocess_file();
    int          preprocess_code(int pass, byte* new_code, byte* old_code, 
                                method_desc* mth, bool generate_mop, 
                                int code_len, int max_locals);
    static void  preprocessing();

    method_desc* find_method(utf_string const& name, utf_string const& desc);
    field_desc* find_field(utf_string const& name);
    static class_file* find(utf_string* name);
    class_file(const char* file_name, int file_name_len, byte* fp, bool jar);
};

class class_spec { 
  protected: 
    class_spec* next;
    static class_spec* chain;
    char        name[1];

  public:
    static bool search(char* name);
    static void add(char* name);
};

//
// Constants for extracting zip file
//

#define LOCAL_HDR_SIG     "\113\003\004"   /*  bytes, sans "P" (so unzip */
#define LREC_SIZE     26    /* lengths of local file headers, central */
#define CREC_SIZE     42    /*  directory headers, and the end-of-    */
#define ECREC_SIZE    18    /*  central-dir record, respectively      */
#define TOTAL_ENTRIES_CENTRAL_DIR  10
#define SIZE_CENTRAL_DIRECTORY     12
#define C_UNCOMPRESSED_SIZE        20
#define C_FILENAME_LENGTH          24
#define C_EXTRA_FIELD_LENGTH       26
#define C_RELATIVE_OFFSET_LOCAL_HEADER    38
#define L_FILENAME_LENGTH                 22
#define L_EXTRA_FIELD_LENGTH              24


#endif
