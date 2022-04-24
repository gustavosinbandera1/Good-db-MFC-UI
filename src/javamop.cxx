// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< JAVAMOP.CXX >---------------------------------------------------*--------*
// JavaMop                    Version 1.02       (c) 1998  GARRET    *     ?  *
// (Metaobject Protocol for Java)                                    *   /\|  *
//                                                                   *  /  \  *
//                          Created:      1-Oct-98    K.A. Knizhnik  * / [] \ *
//                          Last update: 19-Jan-99    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

#include "javamop.h"

#if defined(_WIN32) && !defined(__MINGW32__)
#pragma warning(disable:4996)
#endif

//#define USE_TEMPORARY_VARIABLES 1
//#define JUMP_TO_THE_END         1

static int const vbm_instruction_length[] = {
#define JAVA_INSN(code, mnem, len, push, pop) len,
#include "javamop.d"
0
};

static int const vbm_instruction_stack_push[] = {
#define JAVA_INSN(code, mnem, len, push, pop) push,
#include "javamop.d"
0
};

static int const vbm_instruction_stack_pop[] = {
#define JAVA_INSN(code, mnem, len, push, pop) pop,
#include "javamop.d"
0
};


unsigned string_hash_function(byte* p) { 
    unsigned h = 0, g;
    while(*p) { 
        h = (h << 4) + *p++;
        if ((g = h & 0xF0000000) != 0) { 
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

int number_of_parameters(utf_string const& str)
{
    char* s = (char*)str.data;
    assert(*s == '(');
    int n = 0;
    while (*++s != ')') { 
        switch (*s) {
          case 'J':
          case 'D':
            n += 2;
            break;
          case '[':
            while (*++s == '[');
            if (*s == 'L') { 
                while (*++s != ';');
            }
            n += 1;
            break;
          case 'L':
            while (*++s != ';');
            // no break
          default:
            n += 1;
        }
    }
    return n;
}


int size_of_type(utf_string const& str)
{
    char* s = (char*)str.data;
    switch (*s++) { 
      case '(':
        while (*s++ != ')'); 
        return *s == 'V' ? 0 : (*s == 'J' || *s == 'D') ? 2 : 1;
      case 'J':
      case 'D':
        return 2;
      default:
        return 1;
    }
}

class_file* class_file::class_list;
class_file* class_file::hash_table[CLASS_HASH_TABLE_SIZE];
int class_file::n_mop_classes;
class_file* class_file::mop_root;
constant** class_file::constant_pool_extension;
char* metaobject_name = "Metaobject";
char* metaobject_type = "LMetaobject;";
char* metaobject_cons = "(LMetaobject;)V";
byte class_file::code_buffer[MAX_CODE_SIZE];
word class_file::relocation_table[MAX_CODE_SIZE];
bool make_public;
bool use_class_list;

class_file* parse_class_file(const char* name, int len, byte* buf, bool jar) 
{ 
    unsigned magic = unpack4(buf); 
    if (magic != 0xCAFEBABE) { 
        static bool reported;
        if (jar && !reported && len > 6 && (strncmp(name + len - 6, ".class", 6) == 0 
                                            || strncmp(name + len - 6, ".CLASS", 6) == 0))
        {  
            
            printf("JavaMOP is not able to preprocess compressed JAR file, please run jar with -c0f options\n");
            reported = true;
        }
        return NULL;
    }
    return new class_file(name, len, buf, jar);
}

void load_file(char* file_name, int recursive = 0)
{
#ifdef _WIN32
    HANDLE dir;
    char dir_path[MAX_PATH];
    _WIN32_FIND_DATAA file_data;
    for (char* p = file_name; *p != '\0'; p++) { 
        if (*p == '/') *p = '\\';
    }
    if (recursive != 0) { 
        sprintf(dir_path, "%s\\*", file_name);
        if ((dir=FindFirstFileA(dir_path, &file_data)) != INVALID_HANDLE_VALUE)
        {
            file_name = dir_path; 
        } 
    } else { 
        if (strcmp(file_name, "..") == 0 || strcmp(file_name, ".") == 0) { 
            load_file(file_name, 1);
            return;
        }
        if ((dir=FindFirstFileA(file_name, &file_data)) == INVALID_HANDLE_VALUE)
        { 
            fprintf(stderr, "Failed to open file '%s'\n", file_name);
            return;
        }
    }
    if (dir != INVALID_HANDLE_VALUE) {
        do {
            if (!recursive || *file_data.cFileName != '.') { 
                char file_path[MAX_PATH];
                char* file_dir = strrchr(file_name, '\\');
                char* file_name_with_path;
                if (file_dir != NULL) { 
                    int dir_len = file_dir - file_name + 1;
                    memcpy(file_path, file_name, dir_len);
                    strcpy(file_path+dir_len, file_data.cFileName);
                    file_name_with_path = file_path;
                } else { 
                    file_name_with_path = file_data.cFileName;
                }
                load_file(file_name_with_path, recursive+1);
            }
        } while (FindNextFileA(dir, &file_data));
        FindClose(dir);
        return;
    }
#else
    DIR* dir = opendir(file_name);
    if (dir != NULL) { 
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) { 
            if (*entry->d_name != '.') { 
                char full_name[MAX_PATH];
                sprintf(full_name, "%s/%s", file_name, entry->d_name);
                load_file(full_name, 2);
            }
        } 
        closedir(dir);
        return;
    }
#endif
    if (recursive >= 2) { 
        int len = strlen(file_name); 
        if (len < 6 || (strcmp(file_name + len - 6, ".class") != 0 &&
                        strcmp(file_name + len - 6, ".CLASS") != 0))
        {
            return;
        }
    }

    FILE* f = fopen(file_name, "rb");
    if (f == NULL) { 
        fprintf(stderr, "Failed to open file '%s'\n", file_name);
        return;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    byte* buffer = new byte[file_size];
    if (fread(buffer, file_size, 1, f) != 1) { 
        fprintf(stderr, "Failed to read file '%s'\n", file_name);
        fclose(f);
        delete[] buffer;
        return;
    }
    fclose(f);

    const char* suf = file_name + strlen(file_name) - 4;

    if (suf > file_name 
        && (strcmp(suf, ".zip") == 0 || strcmp(suf, ".ZIP") == 0 ||
            strcmp(suf, ".jar") == 0 || strcmp(suf, ".JAR") == 0)) 
    { 
        // extract files from ZIP archive
        if (file_size < ECREC_SIZE+4) { 
            fprintf(stderr, "Bad format of ZIP file '%s'\n", file_name);
            return;
        }
        byte* hdr = buffer + file_size - (ECREC_SIZE+4);
        int count = unpack2_le(hdr + TOTAL_ENTRIES_CENTRAL_DIR);
        int dir_size = unpack4_le(hdr + SIZE_CENTRAL_DIRECTORY);
        byte* directory = hdr - dir_size;
        byte* dp = directory;

        while (--count >= 0) {
            int uncompressed_size = unpack4_le(dp+4+C_UNCOMPRESSED_SIZE);
            int filename_length = unpack2_le(dp+4+C_FILENAME_LENGTH);
            int cextra_length = unpack2_le(dp+4+C_EXTRA_FIELD_LENGTH);

            if ((dp-directory)+filename_length+CREC_SIZE+4 > dir_size) { 
                fprintf(stderr, "Bad format of ZIP file '%s'\n", file_name);
                break;
            }
                
            int local_header_offset = 
                unpack4_le(dp+4+C_RELATIVE_OFFSET_LOCAL_HEADER);
            byte* local_header = buffer + local_header_offset;

            if (memcmp(local_header+1, LOCAL_HDR_SIG, 3) != 0) {
                fprintf(stderr, "Bad format of ZIP file '%s'\n", file_name);
                break;
            }
                
            int file_start = local_header_offset + (LREC_SIZE+4) 
                + unpack2_le(local_header+4+L_FILENAME_LENGTH)
                + unpack2_le(local_header+4+L_EXTRA_FIELD_LENGTH);

            int filename_offset = CREC_SIZE+4;

            if (uncompressed_size != 0) { 
                parse_class_file((char*)dp+filename_offset,
                                 filename_length, buffer+file_start, true);
            }
            dp += filename_offset + filename_length + cextra_length; 
        }
    } else { 
        parse_class_file(file_name, strlen(file_name), buffer, false);
    }
}

method_desc* class_file::find_method(utf_string const& name, 
                                     utf_string const& desc)
{
    for (method_desc* mth = first_method; mth != NULL; mth = mth->next) { 
        if (mth->name == name && mth->desc == desc) {
            return mth;
        }
    }
    if (superclass != NULL) { 
        return superclass->find_method(name, desc);
    }
    return NULL;
}

field_desc* class_file::find_field(utf_string const& name)
{
    for (field_desc* fd = field_list; fd != NULL; fd = fd->next) { 
        if (fd->name == name) {
            return fd;
        }
    }
    if (superclass != NULL) { 
        return superclass->find_field(name);
    }
    return NULL;
}


class_file::class_file(const char* name, int name_len, byte* fp, bool jar)
{
    int i;

    buffer = fp;
    mop = NULL;
    is_archive = jar;
    file_name = new char[name_len+1];
    sprintf(file_name, "%.*s", name_len, name);

    constants = fp += 8;
    constant_pool_count = unpack2(fp);
    fp += 2;
    constant_pool = new constant*[constant_pool_count];
    memset(constant_pool, 0, sizeof(constant*)*constant_pool_count);
    for (i = 1; i < constant_pool_count; i++) { 
        switch (*fp) { 
          case c_utf8:
            constant_pool[i] = new const_utf8(fp);
            fp += 3 + ((const_utf8*)constant_pool[i])->str.len;
            break;
          case c_integer:
          case c_float:
            fp += 5;
            break;
          case c_long:
          case c_double:
            fp += 9;
            i += 1;
            break;
          case c_class:
            constant_pool[i] = new const_class(fp);
            fp += 3;
            break;
          case c_string:
            fp += 3;
            break;
          case c_field_ref:
          case c_method_ref:
          case c_interface_method_ref:
            constant_pool[i] = new const_ref(fp);
            fp += 5;
            break;
          case c_name_and_type:
            constant_pool[i] = new const_name_and_type(fp);
            fp += 5;
            break;
        }
    }       
    attributes = fp;
    int access_flags = unpack2(fp);
    fp += 2;    
    if (access_flags & f_interface) { 
        // do not proceed interfaces
        return;
    }
    this_class = unpack2(fp);
    fp += 2;
    super_class = unpack2(fp);
    fp += 2;
    int interfaces_count = unpack2(fp);
    fp += 2 + 2*interfaces_count;
    
    this_class_name = ((const_class*)constant_pool[this_class])->name;
    class_name = &((const_utf8*)constant_pool[this_class_name])->str;
    marked = class_spec::search(class_name->as_asciz());
    super_class_name = NULL;
    superclass = NULL;
    if (super_class != 0) { 
        super_class_name = 
            &((const_utf8*)constant_pool
              [((const_class*)constant_pool[super_class])->name])->str;
    }
    int fields_count = unpack2(fp);
    fp += 2;
    
    field_list = NULL;
    while (--fields_count >= 0) {
        int access_flags = unpack2(fp); 
        if (make_public) { 
            access_flags = (access_flags&~(f_private|f_protected)) | f_public;
            pack2(fp, access_flags);
        }
        fp += 2;
        int name_index = unpack2(fp); fp += 4;
        int attr_count = unpack2(fp); fp += 2;
        const_utf8* field_name = (const_utf8*)constant_pool[name_index];

        field_list = new field_desc(field_name->str, access_flags, field_list);

        if (!(access_flags & f_static)) { 
            if (field_name->str == "metaobject") { 
                mop_root = mop = this;
                n_mop_classes += 1;
            }
        }
        while (--attr_count >= 0) { 
            int attr_len = unpack4(fp+2); 
            fp += 6 + attr_len;
        }
    }
    methods = fp;
    next = class_list;
    class_list = this;
    unsigned h = class_name->hash() % CLASS_HASH_TABLE_SIZE;
    collision_chain = hash_table[h];
    hash_table[h] = this;

    code_attr_name = 0;
    int methods_count = unpack2(fp); fp += 2;
    first_method = last_method = NULL;

    while (--methods_count >= 0) { 
        int access_flags = unpack2(fp); fp += 2;
        int name_index = unpack2(fp); fp += 2;
        int desc_index = unpack2(fp); fp += 2;
        int attr_count = unpack2(fp); fp += 2;
        
        const_utf8* mth_name = (const_utf8*)constant_pool[name_index];
        const_utf8* mth_desc = (const_utf8*)constant_pool[desc_index];
        
        method_desc* mth = NULL; 
        if (!(access_flags & f_static)) { 
            mth = new method_desc(mth_name->str, mth_desc->str);
            if (last_method != NULL) { 
                last_method->next = mth;
            } else { 
                first_method = mth;
            }
            last_method = mth;              
        }
            
        while (--attr_count >= 0) {
            int attr_name = unpack2(fp); fp += 2;
            int attr_len = unpack4(fp); fp += 4;
            if (((const_utf8*)constant_pool[attr_name])->str == "Code") { 
                code_attr_name = attr_name;
                if (mth != NULL) { 
                    mth->code = fp+8;
                    mth->code_length = unpack4(fp+4);
                }
            } 
            fp += attr_len;
        }
    } 
}


void class_file::prepare() 
{
    method_desc* mth;
    utf_string* super_name = super_class_name;
    bool add_mop_base = true;
    
    if (marked && is_archive) { 
        fprintf(stderr, "Can not preprocess class '%s' specified in MOP classes "
                " list since it is placed in archive\n", class_name->as_asciz());
        add_mop_base = false;
    }
    if (super_name != NULL) { 
        superclass =  find(super_name);
        do { 
            class_file* super = find(super_name);
            if (super != NULL) { 
                for (mth = super->first_method; mth != NULL; mth = mth->next) {
                    if (mth->redefinition == NULL) { 
                        mth->redefinition = find_method(mth->name, mth->desc);
                    }
                }
                if (mop == NULL) { 
                    mop = super->mop;
                    if (mop == NULL && marked && add_mop_base && !super->marked && *super_name != "java/lang/Object")
                    { 
                        fprintf(stderr, "Class '%s' was specifed in MOP classes "
                                "list while its base class '%s' not.\n",
                                class_name->as_asciz(), super_name->as_asciz());
                        add_mop_base = false;
                    }
                }
                super_name = super->super_class_name;
            } else { 
                if (marked && *super_name != "java/lang/Object") { 
                    fprintf(stderr, "Base class '%s' is unknown for class '%s' "
                            "specifed in MOP classes list\n", 
                            super_name->as_asciz(), class_name->as_asciz());
                    add_mop_base = false;
                }
                break;
            } 
        } while (super_name != NULL);
    }
    if (marked && add_mop_base) { 
        mop = mop_root;
    }  
    for (mth = first_method; mth != NULL; mth = mth->next) { 
        if (mth->code != NULL) { 
            preprocess_code(1, code_buffer, mth->code, mth, true, 
                            mth->code_length, 0);
        } 
    }
}


class_file* class_file::find(utf_string* name) 
{
    for (class_file* cf = hash_table[name->hash() % CLASS_HASH_TABLE_SIZE];
         cf != NULL; 
         cf = cf->collision_chain)
    {
        if (*name == *cf->class_name) { 
            return cf;
        }
    }
    return NULL;
}



void class_file::preprocess_file()
{
    int i;
    byte *cp, *fp = constants;
    byte buf[64];

    const_utf8* magic = (const_utf8*)constant_pool[constant_pool_count-1];
    if (is_archive || 
        (magic != NULL && magic->tag == c_utf8 && magic->str == JAVAMOP_STAMP))
    {
        return; // file was already preprocesed
    }

    FILE* f = fopen(file_name, "wb");
    if (f == NULL) { 
        fprintf(stderr, "Failed to open file '%s' for writing\n", file_name);
        return;
    }
    fwrite(buffer, constants - buffer, 1, f);

    for (class_file* cf = class_list; cf != NULL; cf = cf->next) { 
        cf->mop_var_index = 0;
    }
    int n_extra_consts = 0;
    constant_pool_extension[n_extra_consts++] = new const_utf8("metaobject");
    constant_pool_extension[n_extra_consts++] = 
        new const_utf8(metaobject_type);
    int metaobject_nm_index = constant_pool_count + n_extra_consts;
    constant_pool_extension[n_extra_consts++] = 
        new const_name_and_type(metaobject_nm_index-2, metaobject_nm_index-1); 

    constant_pool_extension[n_extra_consts++] = 
        new const_utf8(metaobject_name);
    int metaobject_index = constant_pool_count + n_extra_consts;
    constant_pool_extension[n_extra_consts++] = 
        new const_class(metaobject_index-1);
    
    constant_pool_extension[n_extra_consts] = new const_utf8("preDaemon");
    constant_pool_extension[n_extra_consts+1] = 
        new const_utf8("(Ljava/lang/Object;I)V");
    constant_pool_extension[n_extra_consts+2] = 
        new const_name_and_type(constant_pool_count + n_extra_consts,
                                constant_pool_count + n_extra_consts+1);
    n_extra_consts += 3;
    mop_pre_index = constant_pool_count + n_extra_consts;
    constant_pool_extension[n_extra_consts++] = 
        new const_ref(c_method_ref, metaobject_index, mop_pre_index-1);
    
    constant_pool_extension[n_extra_consts] = 
        new const_utf8("postDaemon");
    constant_pool_extension[n_extra_consts+1] = 
        new const_utf8("(Ljava/lang/Object;IZ)V");
    constant_pool_extension[n_extra_consts+2] = 
        new const_name_and_type(constant_pool_count + n_extra_consts,
                                constant_pool_count + n_extra_consts+1);
    n_extra_consts += 3;
    mop_post_index = constant_pool_count + n_extra_consts;
    constant_pool_extension[n_extra_consts++] = 
        new const_ref(c_method_ref, metaobject_index, mop_post_index-1);
    
    int constructor_name = 0;
    int constructor_desc = 0;
    int superclass_constructor = 0;

    if (mop != NULL) { 
        int mop_class_index = this_class;
        constructor_name = constant_pool_count+n_extra_consts;
        constant_pool_extension[n_extra_consts++] = new const_utf8("<init>");
        constructor_desc = constant_pool_count+n_extra_consts;
        constant_pool_extension[n_extra_consts++] = 
            new const_utf8(metaobject_cons);
        if (mop != this) { 
            constant_pool_extension[n_extra_consts++] = 
                new const_utf8(mop->class_name);
            mop_class_index = constant_pool_count + n_extra_consts;
            constant_pool_extension[n_extra_consts++] = 
                new const_class(mop_class_index-1);

            if (marked && *super_class_name == "java/lang/Object") { 
                constant_pool_extension[n_extra_consts++] = 
                    new const_utf8(mop_root->class_name);
                super_class = constant_pool_count + n_extra_consts;
                constant_pool_extension[n_extra_consts++] = 
                    new const_class(super_class-1);
                pack2(attributes+4, super_class);
                int default_constructor_desc = 
                    constant_pool_count+n_extra_consts;
                constant_pool_extension[n_extra_consts++] = 
                    new const_utf8("()V");
                int name_and_type = constant_pool_count+n_extra_consts;
                constant_pool_extension[n_extra_consts++] = 
                    new const_name_and_type(constructor_name, 
                                            default_constructor_desc);
                superclass_default_constructor = 
                    constant_pool_count + n_extra_consts;
                constant_pool_extension[n_extra_consts++] = 
                    new const_ref(c_method_ref, super_class, name_and_type);
            }
        }
        mop->mop_var_index = constant_pool_count + n_extra_consts;
        constant_pool_extension[n_extra_consts++] = 
            new const_ref(c_field_ref, mop_class_index, metaobject_nm_index);

        if (code_attr_name == 0) { 
            code_attr_name = constant_pool_count+n_extra_consts;
            constant_pool_extension[n_extra_consts++] = 
                new const_utf8("Code");
        }
        int constructor_name_and_type = constant_pool_count+n_extra_consts;
        constant_pool_extension[n_extra_consts++] = 
            new const_name_and_type(constructor_name, constructor_desc);

        int super = super_class;
        if (super == 0) { 
            constant_pool_extension[n_extra_consts++] = 
                new const_utf8("Ljava/lang/Object;");
            super = constant_pool_count + n_extra_consts;
            constant_pool_extension[n_extra_consts++] = 
                new const_class(super-1);
        }
        superclass_constructor = constant_pool_count + n_extra_consts;
        constant_pool_extension[n_extra_consts++] = 
            new const_ref(c_method_ref, super, constructor_name_and_type);
    }
    for (i = 1; i < constant_pool_count; i++) { 
        const_ref* ref = (const_ref*)constant_pool[i];
        if (ref != NULL && (ref->tag==c_field_ref || ref->tag==c_method_ref)) {
            const_class* cls = (const_class*)constant_pool[ref->cls];
            const_utf8* cls_name = (const_utf8*)constant_pool[cls->name];
            class_file* cf = find(&cls_name->str);
            if (cf != NULL && cf->mop != NULL) { 
                if (cf->mop->mop_var_index == 0) { 
                    constant_pool_extension[n_extra_consts++] = 
                        new const_utf8(cf->mop->class_name);
                    int mop_class_index = 
                        constant_pool_count + n_extra_consts;
                    constant_pool_extension[n_extra_consts++] = 
                        new const_class(mop_class_index-1);
                    cf->mop->mop_var_index = 
                        constant_pool_count + n_extra_consts;
                    constant_pool_extension[n_extra_consts++] = 
                        new const_ref(c_field_ref, mop_class_index, 
                                      metaobject_nm_index);
                }
                ref->mop_var_index = cf->mop->mop_var_index;
            }
        }
    }       
    constant_pool_extension[n_extra_consts++] = new const_utf8(JAVAMOP_STAMP);
   
    pack2(buf, constant_pool_count + n_extra_consts);
    fwrite(buf, 2, 1, f); // new size of constant pool
    // old part of constant pool
    fwrite(constants+2, attributes-constants-2, 1, f);

    for (i = 0; i < n_extra_consts; i++) {
        constant* c = constant_pool_extension[i];
        buf[0] = c->tag;
        switch (c->tag) { 
          case c_class:
            pack2(buf+1, ((const_class*)c)->name);
            fwrite(buf, 3, 1, f);
            break;
          case c_utf8:
            pack2(buf+1, ((const_utf8*)c)->str.len);
            fwrite(buf, 3, 1, f);
            fwrite(((const_utf8*)c)->str.data, ((const_utf8*)c)->str.len,1,f);
            break;
          case c_field_ref:
          case c_method_ref:
            pack2(buf+1, ((const_ref*)c)->cls);
            pack2(buf+3, ((const_ref*)c)->name_and_type);
            fwrite(buf, 5, 1, f);
            break;
          case c_name_and_type:
            pack2(buf+1, ((const_name_and_type*)c)->name);
            pack2(buf+3, ((const_name_and_type*)c)->desc);
            fwrite(buf, 5, 1, f);
            break;
          default:
            assert(false);
        }
        delete c;
    }
    if (make_public) { 
        int access_flags = unpack2(attributes);
        pack2(attributes, access_flags | f_public);
    }

    fwrite(attributes, methods-attributes, 1, f);

    fp = methods;
    int methods_count = unpack2(fp);
    if (mop) {
        cp = buf;
        cp = pack2(cp, methods_count+1);
        cp = pack2(cp, f_public);
        cp = pack2(cp, constructor_name);
        cp = pack2(cp, constructor_desc);
        cp = pack2(cp, 1); // number of attributes
        cp = pack2(cp, code_attr_name);
        cp = pack4(cp, 18);
        cp = pack2(cp, 2); // max stack
        cp = pack2(cp, 2); // max locals
        cp = pack4(cp, 6); // code length
        *cp++ = aload_0;
        *cp++ = aload_1;
        *cp++ = invokespecial;
        cp = pack2(cp, superclass_constructor);
        *cp++ = vreturn;
        cp = pack2(cp, 0); // exception table length
        cp = pack2(cp, 0); // code attributes
        fwrite(buf, cp-buf, 1, f);
    } else { 
        fwrite(fp, 2, 1, f);
    } 
    fp += 2;
    method_desc* mth = first_method;

    while (--methods_count >= 0) { 
        int access_flags = unpack2(fp);
        int attr_count = unpack2(fp+6); 
        bool generate_mop = mop != NULL && !(access_flags & f_static);
        fwrite(fp, 8, 1, f);
        fp += 8;
        
        while (--attr_count >= 0) { 
            int attr_name = unpack2(fp);
            int attr_len = unpack4(fp+2); 
            const_utf8* attr = (const_utf8*)constant_pool[attr_name];
            if (attr->str == "Code") { 
                fp += 6;
                int max_stack = unpack2(fp); fp += 2; 
                int max_locals = unpack2(fp); fp += 2;
                int code_length = unpack4(fp); fp += 4;
                int new_code_length = preprocess_code(2, code_buffer+14, fp, 
                                                      (access_flags & f_static)
                                                       ? (method_desc*)0 : mth,
                                                      generate_mop,
                                                      code_length, max_locals);
                assert(new_code_length+14 < MAX_CODE_SIZE);

                cp = pack2(code_buffer, attr_name);
                cp = pack4(cp, attr_len + new_code_length - code_length + 
                           (generate_mop && finally_start+1 < finally_end ? 8 : 0));
                cp = pack2(cp, max_stack+8);
#ifdef USE_TEMPORARY_VARIABLES 
#ifdef JUMP_TO_THE_END
                cp = pack2(cp, max_locals+9);
#else
                cp = pack2(cp, max_locals+3);
#endif
#else
                cp = pack2(cp, max_locals+1);
#endif
                cp = pack4(cp, new_code_length);
                cp += new_code_length;

                fwrite(code_buffer, cp-code_buffer, 1, f);

                fp += code_length;
                cp = fp;
                int exception_table_length = unpack2(fp); 
                if (generate_mop && finally_start+1 < finally_end) { 
                    pack2(fp, exception_table_length + 1);
                }
                fp += 2;
                while (--exception_table_length >= 0) { 
                    fp = pack2(fp, relocation_table[unpack2(fp)]);
                    fp = pack2(fp, relocation_table[unpack2(fp)]);
                    fp = pack2(fp, relocation_table[unpack2(fp)]);
                    fp += 2;
                }
                fwrite(cp, fp-cp, 1, f);

                if (generate_mop && finally_start+1 < finally_end) { 
                    pack2(buf, finally_start);
                    pack2(buf+2, finally_end);
                    pack2(buf+4, finally_handler);
                    pack2(buf+6, 0);
                    fwrite(buf, 8, 1, f);
                }

                int method_attr_count = unpack2(fp); 
                fwrite(fp, 2, 1, f);
                fp += 2;        

                while (--method_attr_count >= 0) { 
                    cp = fp;
                    int mth_attr_name = unpack2(fp); fp += 2;
                    int mth_attr_len = unpack4(fp); fp += 4;
                    const_utf8* attr = 
                        (const_utf8*)constant_pool[mth_attr_name];
                    if (attr->str == "LineNumberTable") {
                        int table_length = unpack2(fp); fp += 2;
                        while (--table_length >= 0) { 
                            pack2(fp, relocation_table[unpack2(fp)]);
                            fp += 4;
                        }
                    } else if (attr->str == "LocalVariableTable") { 
                        int table_length = unpack2(fp); fp += 2;
                        while (--table_length >= 0) { 
                            int old_start_pc = unpack2(fp);
                            int new_start_pc =  relocation_table[old_start_pc];
                            fp = pack2(fp, new_start_pc);
                            pack2(fp,relocation_table[old_start_pc+unpack2(fp)]
                                     - new_start_pc);
                            fp += 8;
                        }
                    } else { 
                        fp += mth_attr_len;
                    }
                    fwrite(cp, mth_attr_len+6, 1, f);
                }
            } else { 
                fwrite(fp, attr_len+6, 1, f);
                fp += attr_len + 6;
            }
        }
        if (!(access_flags & f_static)) { 
            mth = mth->next;
        }
    }
    
    int attr_count = unpack2(fp); 
    fwrite(fp, 2, 1, f);
    fp += 2;
    while (--attr_count >= 0) { 
        int attr_len = unpack4(fp+2);
        fwrite(fp, attr_len+6, 1, f);
        fp += attr_len + 6;
    }
    fclose(f);
}


inline byte* istore_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = istore_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = istore;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = istore;
        ip = pack2(ip, var_index);
    }
    return ip;
}

inline byte* iload_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = iload_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = iload;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = iload;
        ip = pack2(ip, var_index);
    }
    return ip;
}

inline byte* lstore_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = lstore_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = lstore;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = lstore;
        ip = pack2(ip, var_index);
    }
    return ip;
}

inline byte* lload_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = lload_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = lload;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = lload;
        ip = pack2(ip, var_index);
    }
    return ip;
}

inline byte* fstore_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = fstore_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = fstore;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = fstore;
        ip = pack2(ip, var_index);
    }
    return ip;
}

inline byte* fload_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = fload_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = fload;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = fload;
        ip = pack2(ip, var_index);
    }
    return ip;
}

inline byte* dstore_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = dstore_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = dstore;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = dstore;
        ip = pack2(ip, var_index);
    }
    return ip;
}

inline byte* dload_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = dload_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = dload;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = dload;
        ip = pack2(ip, var_index);
    }
    return ip;
}


inline byte* astore_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = astore_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = astore;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = astore;
        ip = pack2(ip, var_index);
    }
    return ip;
}

inline byte* aload_local(byte* ip, int var_index) { 
    if (var_index <= 3) { 
        *ip++ = aload_0+var_index;
    } else if (var_index < 256) { 
        *ip++ = aload;
        *ip++ = var_index;
    } else {
        *ip++ = wide;
        *ip++ = aload;
        ip = pack2(ip, var_index);
    }
    return ip;
}

int class_file::preprocess_code(int pass, byte* new_code, byte* old_code, 
                                method_desc* mth, bool generate_mop,
                                int code_length, int var_index)
{
    static byte forward_jumps[MAX_CODE_SIZE];
    byte* ip = new_code;

    enum { any, self };
    byte stack[MAX_STACK];
    int sp = 0;
    int pc = 0;
    int return_insn = 0;
    int len;

    if (generate_mop && mop != NULL && !mth->is_constructor) {
        *ip++ = iconst_0;
        ip = istore_local(ip, var_index);
        *ip++ = aload_0;
        *ip++ = getfield;
        ip = pack2(ip, mop->mop_var_index);
        *ip++ = aload_0;
        *ip++ = mth->mutator + iconst_0;
        *ip++ = invokevirtual;
        ip = pack2(ip, mop_pre_index);
    }
    
    finally_start = ip - new_code;

    memset(forward_jumps, 0, code_length);
    int basic_block = 1;
    int dirty_basic_block = 0;
    int n_news = 0;

    while (pc < code_length) { 
        const_ref* cref;
        const_name_and_type* nm;
        const_utf8* target_cls_name;
        const_utf8* name;
        const_utf8* type;
        const_class* cls;
        class_file*  target_class;

        if (forward_jumps[pc]) { 
            basic_block += 1;
        }

        relocation_table[pc] = ip - new_code;

        switch (old_code[pc]) { 
          case jsr:
          case ifeq:
          case ifne:
          case iflt:
          case ifge:
          case ifgt:
          case ifle:
          case if_icmpeq:
          case if_icmpne:
          case if_icmplt:
          case if_icmpge:
          case if_icmpgt:
          case if_icmple:
          case if_acmpeq:
          case if_acmpne:
          case ifnull:
          case ifnonnull:
          case goto_near:
            forward_jumps[pc + (short)unpack2(old_code+pc+1)] = 1;
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];
            basic_block += 1;
            sp = 0;
            break;
          case jsr_w:
          case goto_w:
            forward_jumps[pc + (int)unpack4(old_code+pc+1)] = 1;
            memcpy(ip, old_code+pc, 5);
            ip += 5;
            pc += 5;
            basic_block += 1;
            sp = 0;
            break;
          case tableswitch:
            {
                int old_pc = pc;
                ip[0] = old_code[pc];
                pc += 4 - (pc & 3);
                ip += 4 - ((ip - new_code) & 3);
                int low = unpack4(old_code + pc + 4); 
                int high = unpack4(old_code + pc + 8);
                memcpy(ip, old_code+pc, 12 + 4*(high-low+1));
                forward_jumps[old_pc + (int)unpack4(ip)] = 1;
                ip += 12;
                pc += 12 + 4*(high-low+1);
                while (low <= high) { 
                    forward_jumps[old_pc + (int)unpack4(ip)] = 1;
                    ip += 4;
                    low += 1;
                }
                if (--sp < 0) sp = 0;
                basic_block += 1;
                break;
            }
          case lookupswitch:
            { 
                int old_pc = pc;
                ip[0] = old_code[pc];
                pc += 4 - (pc & 3);
                ip += 4 - ((ip - new_code) & 3);
                int n_pairs = unpack4(old_code + pc + 4); 
                memcpy(ip, old_code+pc, 8+8*n_pairs);
                forward_jumps[old_pc + (int)unpack4(ip)] = 1;
                ip += 8;
                pc += 8 + 8*n_pairs;
                while (--n_pairs >= 0) { 
                    forward_jumps[old_pc + (int)unpack4(ip+4)] = 1;
                    ip += 8;
                }
                if (--sp < 0) sp = 0;
                basic_block += 1;
                break;
            }
          case aload_0:
            *ip++ = old_code[pc++];
            stack[sp++] = self;
            break;
          case ireturn:
          case freturn:
          case areturn:
          case dreturn:
          case lreturn:
          case vreturn:
            if (generate_mop && !mth->is_constructor) { 
                return_insn = old_code[pc];
#ifdef JUMP_TO_THE_END
                switch (return_insn) { 
                  case ireturn:
                    ip = istore_local(ip, var_index+3);
                    break;
                  case freturn:
                    ip = fstore_local(ip, var_index+8);
                    break;
                  case areturn:
                    ip = fstore_local(ip, var_index+1);
                    break;
                  case dreturn: 
                    ip = dstore_local(ip, var_index+4);
                    break;
                  case lreturn:
                    ip = lstore_local(ip, var_index+6);
                }
#endif
                if (pc+1 != code_length) { 
                    *ip++ = goto_near;
                    ip = pack2(ip, code_length - pc);
                }
            } else {
                *ip++ = old_code[pc];
            }
            sp = 0;
            pc += 1;
            basic_block += 1;
            break;
          case getstatic:
          case putstatic:
            cref = (const_ref*)constant_pool[unpack2(old_code+pc+1)];
            nm = (const_name_and_type*)constant_pool[cref->name_and_type];
            type = (const_utf8*)constant_pool[nm->desc];
            if (old_code[pc] == putstatic) { 
                sp -= size_of_type(type->str);
                if (sp < 0) sp = 0;
            } else { 
                stack[sp++] = any;
                if (size_of_type(type->str) == 2) stack[sp++] = any;
            } 
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];     
            break;
          case invokespecial:   
          case invokevirtual:   
          case invokeinterface:
            if (--sp < 0) sp = 0;
            // no break
          case invokestatic:
            cref = (const_ref*)constant_pool[unpack2(old_code+pc+1)];
            nm = (const_name_and_type*)constant_pool[cref->name_and_type];
            cls = (const_class*)constant_pool[cref->cls];
            type = (const_utf8*)constant_pool[nm->desc];
            name = (const_utf8*)constant_pool[nm->name];
            target_cls_name = (const_utf8*)constant_pool[cls->name];
            target_class = class_file::find(&target_cls_name->str);
            sp -= number_of_parameters(type->str);
            if (pass == 1 && old_code[pc] != invokestatic)
#if 0
                && sp >= 0 && stack[sp] == self) 
#endif
            { 
                if (target_class != NULL) { 
                    method_desc* callee = 
                        target_class->find_method(name->str, type->str);
                    if (callee != NULL) { 
                        mth->invocations = 
                            new invocation_desc(callee, 
                                                old_code[pc] != invokespecial,
                                                mth->invocations);
                    }
                }
            }
            if (sp < 0) sp = 0;
            switch (size_of_type(type->str)) { 
              case 2:
                stack[sp++] = any;
                // no break
              case 1:
                stack[sp++] = any;
            }           
            
            if (target_cls_name->str == metaobject_name) { 
                if (name->str == "mutator") {
                    if (generate_mop) { 
                        mth->mutator = 1;
                    } else { 
                        fprintf(stderr, "Metaobject.mutator() method invoked "
                                "from method not controled by metaobject\n");
                    }
                    pc += 3;
                    break;
                } 
                if (name->str == "modify") {
                    if (generate_mop) { 
                        mth->mutator = 1;
                        *ip++ = iconst_1;
                        ip = istore_local(ip, var_index);
                    }
                    pc += 3;
                    break;
                } 
            }
            len = vbm_instruction_length[old_code[pc]];
            memcpy(ip, old_code+pc, len);
            pc += len;
            ip += len;

            if (name->str == "<init>") { 
                if (marked && mop != NULL && mop != this
                    && target_cls_name->str == "java/lang/Object" && n_news == 0) 
                { 
                    pack2(ip-2, superclass_default_constructor);
                    assert(mth->is_constructor);
                    if (finally_start == 0) { 
                        finally_start = ip - new_code;
                    }
                } else if (target_class != NULL && target_class->mop != NULL) { 
                    if (n_news != 0) { 
#ifdef USE_TEMPORARY_VARIABLES
                        ip = astore_local(ip, var_index+1);
                        ip = aload_local(ip, var_index+1);
                        *ip++ = getfield;
                        ip = pack2(ip, target_class->mop->mop_var_index);
                        ip = aload_local(ip, var_index+1);
#else 
                        *ip++ = dup_x0;
                        *ip++ = getfield;
                        ip = pack2(ip, target_class->mop->mop_var_index);
                        *ip++ = swap; 
#endif
                        *ip++ = iconst_5; // MUTATOR+CONSTRUCTOR
                        *ip++ = iconst_1; // dirty
                        *ip++ = invokevirtual;
                        ip = pack2(ip, mop_post_index);
                        n_news -= 1;
                    } else {                
                        assert(mth->is_constructor);
                        if (finally_start == 0) { 
                            finally_start = ip - new_code;
                        }
                    }
                }
            }
            break;
          case getfield:
            cref = (const_ref*)constant_pool[unpack2(old_code+pc+1)];
            nm = (const_name_and_type*)constant_pool[cref->name_and_type];
            name = (const_utf8*)constant_pool[nm->name];
            type = (const_utf8*)constant_pool[nm->desc];
            if ((sp == 0 || stack[sp-1] != self || mth == NULL) 
                && name->str != "metaobject" && cref->mop_var_index != 0) 
            { 
#ifdef USE_TEMPORARY_VARIABLES
                ip = astore_local(ip, var_index+1); 
                ip = aload_local(ip, var_index+1);   // ... obj
                *ip++ = getfield;
                ip = pack2(ip, cref->mop_var_index); // ... mop
                ip = astore_local(ip, var_index+2); 
                ip = aload_local(ip, var_index+2);   // ... mop
                ip = aload_local(ip, var_index+1);   // ... mop obj
                *ip++ = iconst_2;                    // ... mop obj atr
                *ip++ = invokevirtual;               // ... 
                ip = pack2(ip, mop_pre_index);
                ip = aload_local(ip, var_index+1);   // ... obj
                *ip++ = getfield;                    // ... val 
                *ip++ = old_code[pc+1];
                *ip++ = old_code[pc+2];
                if (sp == 0) { 
                    stack[sp++] = any;
                } else { 
                    stack[sp-1] = any;
                }
                if (size_of_type(type->str) == 2) { 
                    stack[sp++] = any;
                }
                ip = aload_local(ip, var_index+2);   // ... val mop
                ip = aload_local(ip, var_index+1);   // ... val mop obj
                *ip++ = iconst_2;                    // ... val mop obj atr
                *ip++ = iconst_0;                    // ... val mop obj atr mod 
                *ip++ = invokevirtual;
                ip = pack2(ip, mop_post_index);      // ... val
#else
                *ip++ = dup_x0;        // ... obj obj
                *ip++ = dup_x0;        // ... obj obj obj
                *ip++ = getfield;
                ip = pack2(ip, cref->mop_var_index);
                *ip++ = dup_x2;        // ... mop obj obj mop
                *ip++ = swap;          // ... mop obj mop obj
                *ip++ = dup_x2;        // ... mop obj obj mop obj
                *ip++ = iconst_2;      // ... mop obj obj mop obj atr
                *ip++ = invokevirtual; // ... mop obj obj
                ip = pack2(ip, mop_pre_index);
                *ip++ = getfield;      // ... mop obj val [val2]
                *ip++ = old_code[pc+1];
                *ip++ = old_code[pc+2];
                if (sp == 0) { 
                    stack[sp++] = any;
                } else { 
                    stack[sp-1] = any;
                }
                if (size_of_type(type->str) == 2) { 
                    *ip++ = dup2_x2;   // ... val val2 mop obj val val2
                    *ip++ = pop2;      // ... val val2 mop obj
                    stack[sp++] = any;
                } else { 
                    *ip++ = dup_x2;    // ... val mop obj val 
                    *ip++ = pop;       // ... val mop obj
                }
                *ip++ = iconst_2;      // ... val mop obj atr
                *ip++ = iconst_0;      // ... val mop obj atr mod 
                *ip++ = invokevirtual;
                ip = pack2(ip, mop_post_index);
#endif
                pc += 3;
                break;
            } 
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];     
            if (sp == 0) { 
                stack[sp++] = any;
            } else { 
                stack[sp-1] = any;
            }
            if (size_of_type(type->str) == 2) stack[sp++] = any;
            break;
          case putfield:
            cref = (const_ref*)constant_pool[unpack2(old_code+pc+1)];
            nm = (const_name_and_type*)constant_pool[cref->name_and_type];
            name = (const_utf8*)constant_pool[nm->name];
            type = (const_utf8*)constant_pool[nm->desc];
            sp -= size_of_type(type->str);

            if (name->str != "metaobject") { 
                if (sp > 0 && stack[sp-1] == self) { 
                    cls = (const_class*)constant_pool[cref->cls];
                    target_cls_name = (const_utf8*)constant_pool[cls->name];
                    target_class = class_file::find(&target_cls_name->str);
                    field_desc* field = NULL; 
                    if (target_class != NULL) { 
                        field = target_class->find_field(name->str);
                    }                    
                    if (generate_mop && !mth->is_constructor 
                        && (field == NULL || (field->attr & f_transient) == 0))
                    { 
                        mth->mutator = 1;
                        if (basic_block != dirty_basic_block) { 
                            *ip++ = iconst_1;
                            ip = istore_local(ip, var_index);
                            dirty_basic_block = basic_block;
                        } 
                    }
                } else if (cref->mop_var_index != 0) { 
#ifdef USE_TEMPORARY_VARIABLES
                    if (size_of_type(type->str) == 2) { 
                        *ip++ = dup2_x1;                    // ... val val2 obj val val2
                        *ip++ = pop2;                       // ... val val2 obj
                        ip = astore_local(ip, var_index+1); // ... val val2
                        ip = aload_local(ip, var_index+1);  // ... val val2 obj
                        *ip++ = dup_x2;                     // ... obj val val2 obj 
                        *ip++ = pop;                        // ... obj val val2 
                    } else { 
                        *ip++ = swap;                       // ... val obj
                        ip = astore_local(ip, var_index+1); // ... val 
                        ip = aload_local(ip, var_index+1);  // ... val obj
                        *ip++ = swap;                       // ... obj val
                    }
                    ip = aload_local(ip, var_index+1);      // ... obj val obj 
                    *ip++ = getfield;                       // ... obj val mop 
                    ip = pack2(ip, cref->mop_var_index);
                    ip = astore_local(ip, var_index+2);     // ... obj val 
                    ip = aload_local(ip, var_index+2);      // ... obj val mop
                    ip = aload_local(ip, var_index+1);      // ... obj val mop obj
                    *ip++ = iconst_3;                       // ... obj val mop obj atr
                    *ip++ = invokevirtual;                  // ... obj val 
                    ip = pack2(ip, mop_pre_index);
                    *ip++ = putfield;                       // ...
                    *ip++ = old_code[pc+1];
                    *ip++ = old_code[pc+2];
                    ip = aload_local(ip, var_index+2);      // ... mop
                    ip = aload_local(ip, var_index+1);      // ... mop obj
#else               
                    if (size_of_type(type->str) == 2) { 
                        *ip++ = dup2_x1;   // ... val val2 obj val val2
                        *ip++ = pop2;      // ... val val2 obj
                        *ip++ = dup_x2;    // ... obj val val2 obj
                        *ip++ = dup_x0;    // ... obj val val2 obj obj
                        *ip++ = getfield;  // ... obj val val2 obj mop 
                        ip = pack2(ip, cref->mop_var_index);
                        *ip++ = swap;      // ... obj val val2 mop obj
                        *ip++ = dup_x1;    // ... obj val val2 obj mop obj
                        *ip++ = iconst_3;  // ... obj val val2 obj mop obj atr
                        *ip++ = invokevirtual; // ... obj val val2 obj
                        ip = pack2(ip, mop_pre_index);
                        *ip++ = dup_x2;    // ... obj obj val val2 obj
                        *ip++ = pop;       // ... obj obj val val2
                        *ip++ = putfield;  // ... obj
                        *ip++ = old_code[pc+1];
                        *ip++ = old_code[pc+2];
                        *ip++ = dup_x0;    // ... obj obj
                        *ip++ = getfield;  // ... obj mop 
                        ip = pack2(ip, cref->mop_var_index);
                        *ip++ = swap;      // ... mop obj
                    } else { 
                        *ip++ = swap;      // ... val obj
                        *ip++ = dup_x1;    // ... obj val obj
                        *ip++ = dup_x0;    // ... obj val obj obj
                        *ip++ = getfield;  // ... obj val obj mop 
                        ip = pack2(ip, cref->mop_var_index);
                        *ip++ = dup_x2;    // ... obj mop val obj mop
                        *ip++ = swap;      // ... obj mop val mop obj
                        *ip++ = dup_x2;    // ... obj mop obj val mop obj
                        *ip++ = iconst_3;  // ... obj mop obj val mop obj atr
                        *ip++ = invokevirtual; // ... obj mop obj val
                        ip = pack2(ip, mop_pre_index);
                        *ip++ = putfield;  // ... obj mop
                        *ip++ = old_code[pc+1];
                        *ip++ = old_code[pc+2];
                        *ip++ = swap;      // ... mop obj
                    }
#endif
                    *ip++ = iconst_3;      // ... mop obj atr
                    *ip++ = iconst_1;      // ... mop obj atr mod 
                    *ip++ = invokevirtual;
                    ip = pack2(ip, mop_post_index);
                    pc += 3;
                    if (--sp < 0) sp = 0;
                    break;
                }
            } 
            if (--sp < 0) sp = 0;
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];
            break;
          case multianewarray:
            if ((sp -= old_code[pc+3]) < 0) sp = 0;
            stack[sp++] = any;
            memcpy(ip, old_code+pc, 4);
            pc += 4;
            ip += 4;
            break;
          case wide:
            sp -= vbm_instruction_stack_pop[old_code[pc+1]];
            if (sp < 0) sp = 0;
            len = vbm_instruction_stack_push[old_code[pc+1]];
            while (--len >= 0) stack[sp++] = any;
            len = vbm_instruction_length[old_code[pc+1]]*2;
            memcpy(ip, old_code+pc, len);
            pc += len;
            ip += len;
            break;
          case dup_x0:
            if (sp > 0) {
                stack[sp] = stack[sp-1]; 
                sp += 1;
            }
            *ip++ = old_code[pc++];
            break;
          case dup_x1:
            if (sp > 1) { 
                stack[sp]   = stack[sp-1]; 
                stack[sp-1] = stack[sp-2]; 
                stack[sp-2] = stack[sp]; 
                sp += 1;
            } 
            *ip++ = old_code[pc++];
            break;
          case dup_x2:
            if (sp > 2) {
                stack[sp]   = stack[sp-1]; 
                stack[sp-1] = stack[sp-2]; 
                stack[sp-2] = stack[sp-3]; 
                stack[sp-3] = stack[sp]; 
                sp += 1;
            }
            *ip++ = old_code[pc++];
            break;
          case dup2_x0:
            if (sp > 1) { 
                stack[sp]   = stack[sp-2]; 
                stack[sp+1] = stack[sp-1]; 
                sp += 2;
            } 
            *ip++ = old_code[pc++];
            break;
          case dup2_x1:
            if (sp > 2) { 
                stack[sp+1] = stack[sp-1]; 
                stack[sp]   = stack[sp-2]; 
                stack[sp-1] = stack[sp-3]; 
                stack[sp-2] = stack[sp+1]; 
                stack[sp-3] = stack[sp]; 
                sp += 2;
            }
            *ip++ = old_code[pc++];
            break;
          case dup2_x2:
            if (sp > 3) { 
                stack[sp+1] = stack[sp-1]; 
                stack[sp]   = stack[sp-2]; 
                stack[sp-1] = stack[sp-3]; 
                stack[sp-2] = stack[sp-4]; 
                stack[sp-3] = stack[sp+1]; 
                stack[sp-4] = stack[sp]; 
                sp += 2;
            }
            *ip++ = old_code[pc++];
            break;
          case swap:
            if (sp > 1) { 
                byte tmp    = stack[sp-1]; 
                stack[sp-1] = stack[sp-2]; 
                stack[sp-2] = tmp;
            }
            *ip++ = old_code[pc++];
            break;          
          case anew:
            cls = (const_class*)constant_pool[unpack2(old_code+pc+1)];
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];
            *ip++ = old_code[pc++];
            target_cls_name = (const_utf8*)constant_pool[cls->name];
            target_class = class_file::find(&target_cls_name->str);
            if (target_class != NULL && target_class->mop != NULL) {
                n_news += 1;
                *ip++ = dup_x0;
            }
            stack[sp++] = any;
            break;
          case ret:
          case athrow:
            basic_block += 1;
            sp = 0;
            // no break
          default:
            sp -= vbm_instruction_stack_pop[old_code[pc]];
            if (sp < 0) sp = 0;
            len = vbm_instruction_stack_push[old_code[pc]];
            while (--len >= 0) stack[sp++] = any;
            len = vbm_instruction_length[old_code[pc]];
            memcpy(ip, old_code+pc, len);
            pc += len;
            ip += len;
        }
    }
    if (pass == 1) { 
        return 0;
    }
    relocation_table[code_length] = finally_end = ip - new_code;
    //
    // Adjust jump offsets
    //
    for (pc = 0; pc < code_length;) { 
        int new_pc = relocation_table[pc];
        ip = new_code + new_pc;
        switch (old_code[pc]) { 
          case jsr:
          case ifeq:
          case ifne:
          case iflt:
          case ifge:
          case ifgt:
          case ifle:
          case if_icmpeq:
          case if_icmpne:
          case if_icmplt:
          case if_icmpge:
          case if_icmpgt:
          case if_icmple:
          case if_acmpeq:
          case if_acmpne:
          case ifnull:
          case ifnonnull:
          case goto_near:
            pack2(ip+1, relocation_table[pc + (short)unpack2(ip+1)] - new_pc); 
            pc += 3;
            break;
          case jsr_w:
          case goto_w:
            pack4(ip+1, relocation_table[pc + (int)unpack4(ip+1)] - new_pc); 
            pc += 5;
            break;
          case ireturn:
          case freturn:
          case areturn:
          case dreturn:
          case lreturn:
          case vreturn:
            if (generate_mop && !mth->is_constructor && pc+1 != code_length) { 
#ifdef  JUMP_TO_THE_END  
                if ((unsigned)*ip - istore <= astore - istore) {
                    ip += 2;
                } else if ((unsigned)*ip - istore_0 <= astore_3 - istore_0) {
                    ip += 1;
                } else if (*ip == wide) { 
                    ip += 4;
                }
#endif
                assert(*ip == goto_near);
                pack2(ip+1, relocation_table[pc+(short)unpack2(ip+1)]-new_pc); 
            } 
            pc += 1;
            break;
          case tableswitch:
            { 
                int old_pc = pc;
                pc += 4 - (old_pc & 3);
                ip += 4 - (new_pc & 3);
                ip = pack4(ip, relocation_table[old_pc + (int)unpack4(ip)]
                               - new_pc);
                int low = unpack4(ip); 
                int high = unpack4(ip + 4);
                ip += 8;
                pc += 12 + 4*(high-low+1);
                while (low <= high) { 
                    ip = pack4(ip, relocation_table[old_pc + (int)unpack4(ip)] 
                                   - new_pc);
                    low += 1;
                }
                break;
            }
          case lookupswitch:
            { 
                int old_pc = pc;
                pc += 4 - (old_pc & 3);
                ip += 4 - (new_pc & 3);
                ip = pack4(ip, relocation_table[old_pc + (int)unpack4(ip)]
                           - new_pc);
                int n_pairs = unpack4(ip); 
                ip += 4;
                pc += 8 + 8*n_pairs;
                while (--n_pairs >= 0) { 
                    ip = pack4(ip+4,relocation_table[old_pc+(int)unpack4(ip+4)]
                                    - new_pc);
                }
                break;
            }
          case wide:
            pc += vbm_instruction_length[old_code[pc+1]]*2;
            break;
          default:
            pc += vbm_instruction_length[old_code[pc]];
        }
    }

    ip = new_code + finally_end;
    if (generate_mop) { 
#ifdef JUMP_TO_THE_END
        byte* goto_insn = NULL;
#endif
        if (!mth->is_constructor) { 
            *ip++ = aload_0;
            *ip++ = getfield;
            ip = pack2(ip, mop->mop_var_index);
            *ip++ = aload_0;
            *ip++ = mth->mutator + iconst_0;
            ip = iload_local(ip, var_index);
            *ip++ = invokevirtual;
            ip = pack2(ip, mop_post_index);
#ifdef JUMP_TO_THE_END
            goto_insn = ip;
            *ip++ = goto_near;
            ip += 2;
#else
            *ip++ = return_insn;
#endif
        } 
        if (finally_start+1 < finally_end) { 
            finally_handler = ip - new_code;
            *ip++ = aload_0;
            *ip++ = getfield;
            ip = pack2(ip, mop->mop_var_index);
            *ip++ = aload_0;
            *ip++ = bipush;
            *ip++ = mth->is_constructor 
                ? (8+4+1) // MUTATOR+CONSTRUCTOR+EXCEPTION
                : (8+mth->mutator); // EXCEPTION
            if (mth->is_constructor) { 
                *ip++ = iconst_1;
            } else { 
                ip = iload_local(ip, var_index);
            }
            *ip++ = invokevirtual;
            ip = pack2(ip, mop_post_index);
            *ip++ = athrow;
        }
#if JUMP_TO_THE_END
        if (goto_insn != NULL) { 
            pack2(goto_insn+1, ip - goto_insn);
            switch (return_insn) { 
              case ireturn:
                ip = iload_local(ip, var_index+3);
                break;
              case freturn:
                ip = fload_local(ip, var_index+8);
                break;
              case areturn:
                ip = fload_local(ip, var_index+1);
                break;
              case dreturn:     
                ip = dload_local(ip, var_index+4);
                break;
              case lreturn:
                ip = lload_local(ip, var_index+6);
            }
            *ip++ = return_insn;
        }
#endif
    }                                                 
    return ip - new_code;                                      
}


void class_file::preprocessing()
{    
    class_file* cp;
    method_desc* mp;
    invocation_desc* ip;

    if (n_mop_classes == 0) { 
        fprintf(stderr, "No metaclasses found\n");
        return;
    }
        
    if (use_class_list && n_mop_classes != 1) { 
        fprintf(stderr, "Exactly one class with 'metaobject' instance field "
                "should be specified when list of MOP classes is used\n");
    }

    for (cp = class_list; cp != NULL; cp = cp->next) { 
        cp->prepare();
    }

    //
    // Propagate mutator attribute
    //
    bool propagated;
    do { 
        propagated = false;
        for (cp = class_list; cp != NULL; cp = cp->next) { 
            for (mp = cp->first_method; mp != NULL; mp = mp->next) {
                if (!mp->mutator) { 
                    if (!mp->redef_mutator && mp->redefinition != NULL
                        && mp->redefinition->mutator) 
                    { 
                        mp->redef_mutator = true;
                        propagated = true;
                    }
                    for (ip = mp->invocations; ip != NULL; ip = ip->next) { 
                        if (ip->callee->mutator ||
                            (ip->is_virtual && ip->callee->redef_mutator)) 
                        { 
                            propagated = true;
                            mp->mutator = 1;
                            break;
                        }
                    }
                }
            }
        }
    } while (propagated);

    constant_pool_extension = new constant*[n_mop_classes*3+64];

    for (cp = class_list; cp != NULL; cp = cp->next) { 
        cp->preprocess_file();
    }
}

class_spec* class_spec::chain;

bool class_spec::search(char* name) { 
    for (class_spec* spec = chain; spec != NULL; spec = spec->next) { 
        char *p = name, *q = spec->name;
        while (*p == *q || (*p == '/' && *q == '.')) { 
            if (*p == '\0') return true;
            p += 1;
            q += 1;
        }
        if (*q == '*') return true;
    }
    return false;
}

void class_spec::add(char* name) { 
    class_spec* spec = (class_spec*)new char[sizeof(class_spec)+strlen(name)];
    strcpy(spec->name, name);
    spec->next = chain;
    chain = spec;
}


void mark_classes(char* file_name) { 
    char class_name[MAX_CLASS_NAME];
    FILE* f = fopen(file_name, "r");
    if (f == NULL) { 
        fprintf(stderr, "Failed to open file '%s'\n", file_name);
        return;
    }
    use_class_list = true;
    while (fscanf(f, "%s", class_name) == 1) { 
        class_spec::add(class_name); 
    }
    fclose(f);
}


int main(int argc, char* argv[])
{
    int i;
    if (argc == 1) { 
        fprintf(stderr, "JAVAMOP: Java metaobject protocol preprocessor\n"
                "Usage: javamop [-package NAME] [-classes FILE] [-public] {CLASS...}\n");
        return EXIT_FAILURE;
    }   
                                    
    for (i = 1; i < argc; i++) { 
        char* file_name = argv[i];
        if (*file_name == '-') { 
            if (strcmp(file_name, "-package") == 0) { 
                if (++i < argc) { 
                    char* mop_package_name = argv[i];
                    metaobject_type = new char[strlen(mop_package_name) + 14];
                    metaobject_name = new char[strlen(mop_package_name) + 12];
                    metaobject_cons = new char[strlen(mop_package_name) + 17];
                    sprintf(metaobject_name,"%s/Metaobject", mop_package_name);
                    sprintf(metaobject_type,"L%s/Metaobject;",
                            mop_package_name);
                    sprintf(metaobject_cons,"(L%s/Metaobject;)V", 
                            mop_package_name);
                } else { 
                    fprintf(stderr, "Value for option -protocol should be "
                            "specified\n");
                } 
            } else if (strcmp(file_name, "-classes") == 0) {
                if (++i < argc) { 
                    mark_classes(argv[i]);
                } else { 
                    fprintf(stderr, "File name should be specified for -classes "
                            "option\n");
                } 
            } else if (strcmp(file_name, "-public") == 0) {
                make_public = true;
            } else { 
                fprintf(stderr, "Unknown option: %s\n",  file_name);
            }
            continue;
        }
        load_file(file_name);
    }
    class_file::preprocessing();
    return EXIT_SUCCESS;
}
