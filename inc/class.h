// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CLASS.H >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 31-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Runtime type information: class and fileds descriptors 
//-------------------------------------------------------------------*--------*

#ifndef __CLASS_H__
#define __CLASS_H__

#include "wstring.h"
#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

class object_ref; 
class metaobject; 

class raw_binary_t { 
  public:
    char const* get_data() const { 
        return data;
    }

    size_t get_size() const { 
        return size;
    }

    void set_size(size_t new_size) {
        if (new_size != size) {
            delete[] data;
            size = new_size;
            if (new_size != 0) { 
                data = new char[new_size];
            } else { 
                data = NULL;
            }                
        }
    }
    
    operator char*() { 
        return data;
    }

    char& operator [](int i) { 
        assert((unsigned)i < size);
        return data[i];
    }
    

    raw_binary_t() { 
        data = NULL;
        size = 0;
    }
    
    //[MC]
	raw_binary_t(char const* ptr, size_t len) {
        if (len > 0) { 
            data = new char[len];
            memcpy(data, ptr, len);
            size = len;
        } else { 
            data = NULL;
            size = 0;
        }
    }
    
    raw_binary_t& operator = (raw_binary_t const& src) { 
        set_size(src.size);
        if (size > 0) {
            memcpy(data, src.data, size);
        }
        return *this;
    }
    
    ~raw_binary_t() { 
        delete[] data;
    }

  private:
    friend class field_descriptor;

    char*  data;
    size_t size;
};

//[MC]
inline void assign_str(raw_binary_t &raw_binary, const char *str) 
{
	raw_binary = raw_binary_t(str, strlen(str) + 1);
}

enum field_flags {  
	fld_not_null = 1,
	fld_binary = 2,
	fld_indexed = 4
};

class GOODS_DLL_EXPORT field_descriptor : public l2elem { 
    friend class class_descriptor;
public:
    field_descriptor* components;     // components of field 
    const char*       name;           // name of field
    
    struct field_characteristics { 
        field_type        type;       // type of field (defined in protocol.h)
        int               size;       // size of field
        int               n_items;    // length of array 
        int               offs;       // offset to field 
    };
    field_characteristics loc;        // characteristics of local object field 
                                      // in volatile memory
    field_characteristics dbs;        // characteristics of object files 
                                      // in database

    int               n_refs;         // number of references in field
                                      // (this field is used only for database objects)
    int               flags;          // Extra flags associated with field
 
    //
    // Unpack field from database format to internal representation
    //
    static void unpack(field_descriptor* field, obj_storage* storage, 
                       char* dst, char* &src_refs, char* &src_bins); 
    //
    // Pack field to database format from internal representation
    // (this method also builds persistent closure of all referenced objects)
    //
    static void pack(field_descriptor* field, 
                     char* &dst_refs, char* &dst_bins, char* src,
                     hnd_t parent_hnd, field_descriptor* varying,
                     int varying_length);

    
    //
    // Calculated packed size of string components
    //
    static size_t calculate_strings_size(field_descriptor* field, char* src_obj, size_t size);
   

    //
    // Destroy references from varying part of object. 'src' pointes
    // to the second element of varying part and 'len' specify 
    // number of element in varying part minus one
    //
    void destroy_references(char* src, int len); 


    virtual ~field_descriptor();
  public:
    friend field_descriptor& describe_field(char const&);
    friend field_descriptor& describe_field(int1 const&);
    friend field_descriptor& describe_field(int2 const&);
    friend field_descriptor& describe_field(int4 const&);
    friend field_descriptor& describe_field(int8 const&);
    friend field_descriptor& describe_field(nat1 const&);
    friend field_descriptor& describe_field(nat2 const&);
    friend field_descriptor& describe_field(nat4 const&);
    friend field_descriptor& describe_field(nat8 const&);
    friend field_descriptor& describe_field(float const&);
    friend field_descriptor& describe_field(double const&);
    friend field_descriptor& describe_field(object_ref const&);
    friend field_descriptor& describe_field(wstring_t const&);
    friend field_descriptor& describe_field(raw_binary_t const&);

    void* operator new(size_t size);
    void  operator delete(void* p);

    field_descriptor& operator,(field_descriptor& next_field) { 
        next_field.link_after(prev); 
        return *this;
    }

    //
    // Construct field descriptor using parameters of field obtained by
    // one of the macros below
    //
    field_descriptor(const char* name, int size, int n_items, int offset, 
                     field_descriptor* componentns, int flags = 0); 
    //
    // Construct field descriptor for performing mapping between fields
    // when class format is changed
    //
    field_descriptor(field_descriptor const* new_field,
                     dbs_field_descriptor const* old_field);
};

#ifndef NO_BOOL_TYPE
inline field_descriptor& describe_field(bool const&) { 
    return *(field_descriptor*)fld_unsigned_integer; 
}
#endif

inline field_descriptor& describe_field(int1 const&) { 
    return *(field_descriptor*)fld_signed_integer; 
}
inline field_descriptor& describe_field(int2 const&) { 
    return *(field_descriptor*)fld_signed_integer; 
}
inline field_descriptor& describe_field(int4 const&) { 
    return *(field_descriptor*)fld_signed_integer; 
}
inline field_descriptor& describe_field(int8 const&) { 
    return *(field_descriptor*)fld_signed_integer; 
}
inline field_descriptor& describe_field(char const&) { 
    return *(field_descriptor*)fld_signed_integer; 
}
inline field_descriptor& describe_field(nat1 const&) { 
    return *(field_descriptor*)fld_unsigned_integer; 
}
inline field_descriptor& describe_field(nat2 const&) { 
    return *(field_descriptor*)fld_unsigned_integer; 
}
inline field_descriptor& describe_field(nat4 const&) { 
    return *(field_descriptor*)fld_unsigned_integer; 
}
inline field_descriptor& describe_field(nat8 const&) { 
    return *(field_descriptor*)fld_unsigned_integer; 
}
inline field_descriptor& describe_field(float const&) { 
    return *(field_descriptor*)fld_real; 
}
inline field_descriptor& describe_field(double const&) { 
    return *(field_descriptor*)fld_real; 
}
inline field_descriptor& describe_field(object_ref const&) { 
    return *(field_descriptor*)fld_reference; 
}
#ifdef _NATIVE_WCHAR_T_DEFINED 
inline field_descriptor& describe_field(wchar_t const&) { 
    return *(field_descriptor*)fld_unsigned_integer; 
}
#endif
inline field_descriptor& describe_field(wstring_t const&) { 
    return *(field_descriptor*)fld_string; 
}
inline field_descriptor& describe_field(raw_binary_t const&) { 
    return *(field_descriptor*)fld_raw_binary; 
}


#define NO_FIELDS (*(GOODS_NAMESPACE::field_descriptor*)0)

#ifdef USE_NAMESPACES
#define FIELD(x) \
(*new GOODS_NAMESPACE::field_descriptor(#x, sizeof(x), 1, (char*)&(x) - (char*)this, \
                       &describe_field(x)))

#define ARRAY(x) \
(*new GOODS_NAMESPACE::field_descriptor(#x, sizeof(*x), itemsof(x), (char*)(x) - (char*)this, \
                       &describe_field(*x)))

#define MATRIX(x) \
(*new GOODS_NAMESPACE::field_descriptor(#x, sizeof(*x), itemsof(x), (char*)(x) - (char*)this, \
                       new GOODS_NAMESPACE::field_descriptor("", sizeof(**x), itemsof(*x), 0, \
                                            &describe_field(**x))))

#define VARYING(x) \
(assert(sizeof(x) == sizeof(*x)),  \
 *new GOODS_NAMESPACE::field_descriptor(#x, sizeof(*x), 0, (char*)(x) - (char*)this, \
                       &describe_field(*x)))

// The following set of macros should be used whengoods namespace in not implcitely used
// and snadard GOODS types are described
#define GOODS_FIELD(x) \
(*new GOODS_NAMESPACE::field_descriptor(#x, sizeof(x), 1, (char*)&(x) - (char*)this, \
                       &GOODS_NAMESPACE::describe_field(x)))

#define GOODS_FIELD_EX(x,flags) \
(*new GOODS_NAMESPACE::field_descriptor(#x, sizeof(x), 1, (char*)&(x) - (char*)this, \
                                        &GOODS_NAMESPACE::describe_field(x), flags))

#define GOODS_ARRAY(x) \
(*new GOODS_NAMESPACE::field_descriptor(#x, sizeof(*x), itemsof(x), (char*)(x) - (char*)this, \
                       &GOODS_NAMESPACE::describe_field(*x)))

#define GOODS_ARRAY_EX(x, flags)							\
(*new GOODS_NAMESPACE::field_descriptor(#x, sizeof(*x), itemsof(x), (char*)(x) - (char*)this, \
                       &GOODS_NAMESPACE::describe_field(*x), flags))

#define GOODS_MATRIX(x) \
(*new GOODS_NAMESPACE::field_descriptor(#x, sizeof(*x), itemsof(x), (char*)(x) - (char*)this, \
                       new GOODS_NAMESPACE::field_descriptor("", sizeof(**x), itemsof(*x), 0, \
                                            &GOODS_NAMESPACE::describe_field(**x))))

#define GOODS_VARYING(x) \
(assert(sizeof(x) == sizeof(*x)),  \
 *new GOODS_NAMESPACE::field_descriptor(#x, sizeof(*x), 0, (char*)(x) - (char*)this, \
                       &GOODS_NAMESPACE::describe_field(*x)))


#else
#define FIELD(x) \
(*new field_descriptor(#x, sizeof(x), 1, (char*)&(x) - (char*)this, \
                       &describe_field(x)))

#define FIELD_EX(x, flags) \
(*new field_descriptor(#x, sizeof(x), 1, (char*)&(x) - (char*)this, \
                       &describe_field(x), flags))

#define ARRAY(x) \
(*new field_descriptor(#x, sizeof(*x), itemsof(x), (char*)(x) - (char*)this, \
                       &describe_field(*x)))

#define ARRAY_EX(x, flags) \
(*new field_descriptor(#x, sizeof(*x), itemsof(x), (char*)(x) - (char*)this, \
                       &describe_field(*x), flags))

#define MATRIX(x) \
(*new field_descriptor(#x, sizeof(*x), itemsof(x), (char*)(x) - (char*)this, \
                       new field_descriptor("", sizeof(**x), itemsof(*x), 0, \
                                            &describe_field(**x))))

#define VARYING(x) \
(assert(sizeof(x) == sizeof(*x)),  \
 *new field_descriptor(#x, sizeof(*x), 0, (char*)(x) - (char*)this, \
                       &describe_field(*x)))
#endif

//
// Application class descriptor. It contains information about
// name, type, size and offset of all class fields in memory and
// in database. This class also contains reference to database class
// descriptor. To perform convertion of instances of modified classes special
// class descriptor is created which is used only for loading object.
// 
#define DESCRIPTOR_HASH_TABLE_SIZE 1041

class GOODS_DLL_EXPORT class_descriptor { 
    friend class object_handle;
    friend class object;  
    friend class obj_storage;
  public:   
    const char*  const name;
    const cid_t ctid;
    class_descriptor* const base_class; 

    typedef object* (*constructor_t)(hnd_t hnd, class_descriptor& cls,
                                     size_t varying_size);
    const constructor_t constructor;

    metaobject*  mop; // metaobject be default for this class object instances

    class_descriptor* derived_classes;// list of derived classes for thus class
    class_descriptor* next_derived;   // next derived class for base class

    void* class_data; // can be used to refer class specific data

    enum class_attributes { 
        cls_no_attr = 0, 
		cls_aligned = 1, // instance should be aligned to nearest power of 2
		cls_hierarchy_root = 2, // Classes which are considered as roots of inheritance hierarchy (used by ORM)
		cls_hierarchy_super_root = 4, // Classes which siblings are considered as roots of inheritance hierarchy (used by ORM)
		cls_non_relational = 8, // no table should be generated for this class (used by ORM)
		cls_binary = 16 // create text fields of base class as binary 
    };
    int class_attr; // mask of class attributes

    static class_descriptor* find(const char* name);

    boolean is_superclass_for(hnd_t hnd) const;

    //
    // Class object is no more used by storage
    // 
    void dealloc() { 
        if (new_cpid != 0) { 
            // class was created only for conversion of loaded objects
            delete this;
        }
    }

    inline size_t     unpacked_size(size_t packed_size) const { 
        return packed_varying_size
            ? fixed_size + (packed_size-packed_fixed_size)
                            / packed_varying_size * varying_size
            : fixed_size;
    }
    inline size_t     packed_size(char* obj, size_t unpacked_size) const { 
        return packed_fixed_size
            + (has_strings ? calculate_strings_size(obj) : 0) 
            + (varying_size ? (unpacked_size-fixed_size) / varying_size * packed_varying_size
               : 0);
    }

    //
    // Constructor is usually called by REGISTER or REGISTER_EX macro
    //
    class_descriptor(const char* name, // class name
                     size_t size,      // size of fized part of class instance
                     metaobject* mop,  // metaobject
                     constructor_t cons, // constructor of class instance
                     field_descriptor* fields, // list of fields
                     class_descriptor* base,  // base class
                     int class_attr = cls_no_attr);
    virtual ~class_descriptor();

public:
    size_t  packed_fixed_size;
    size_t  packed_varying_size;
    size_t  n_fixed_references;
    size_t  n_varying_references;
    size_t  fixed_size;
    size_t  varying_size;
    boolean has_strings;
    
    //
    // This fields are used only in convertion class descriptor
    // 
    cpid_t const             new_cpid;
    class_descriptor* const  new_class; // class descriptor assigned to 
                                        // created object

    dbs_class_descriptor*    dbs_desc;

    static cid_t            last_ctid;
 
    static class_descriptor* hash_table[DESCRIPTOR_HASH_TABLE_SIZE];
    class_descriptor* next; // collision chain

    field_descriptor* fields;  // list of all fields in class
    field_descriptor* varying; // pointer to varying field (if any)
    
    static unsigned   hash_function(const char* name); 

    boolean           initialized() { return dbs_desc != NULL; }

    //
    // Calculate class and it's components attributes: 
    // packed size, offset, number of references. Returns number of fields.
    //
    int               calculate_attributes(field_descriptor* field,
                                           size_t& size,
                                           size_t& n_refs, 
                                           size_t& names_size);

    //
    // Build database class descriptor for this class
    //
    static int build_dbs_class_descriptor(dbs_class_descriptor* dbs_desc, 
                                          int field_no,
                                          field_descriptor* field,
                                          size_t& name_offs);

    //
    // Calculate total packed size of all strings in the object
    //
    size_t calculate_strings_size(char* obj) const;

    //
    // Convert object from database format to internal format
    //
    void unpack(dbs_object_header* obj, hnd_t hnd, int flags);
    //
    // Convert object from internal format to database representation
    //
    void pack(dbs_object_header* obj, hnd_t hnd);

    //
    // Include into chain of modified objects all transient
    // objects referenced by object with 'hnd' indentifier
    //
    void make_persistent_closure(hnd_t hnd);


    //
    // Destroy object references in varying part of object
    //
    void destroy_varying_part_references(object* obj) const;

    //
    // Create fields tree for mapping fields of old version of the class
    // to new version of the class
    //
    field_descriptor* create_field_mapping(dbs_class_descriptor* old_desc,
                                           field_descriptor* new_field,
                                           int old_field_no,
                                           int n_dbs_fileds,
                                           size_t& ref_offs,
                                           size_t& bin_offs);
    
    //
    // Create class descriptor for modified class (in comparence 
    // with database class 'old_desc' with the same name). This class is used 
    // only for loading object from database, then object 'cpid'
    // will be changed to 'new_cpid' and object will be contolled
    // by 'new_desc' descriptor of new version of class.
    //
    class_descriptor(cpid_t new_cpid,
                     class_descriptor* new_desc, 
                     dbs_class_descriptor* old_desc);

    //
    // Collect information about class components. This method is
    // really part of contructor and is separated only to avoid
    // problem with order of invokation of constructors of static objects.
    //
    void build_components_info();
};

#if GOODS_RUNTIME_TYPE_CHECKING
#if GNUC_BEFORE(2,96)
#define METACLASS_DECLARATIONS(CLASS, BASE_CLASS)                 \
    static GOODS_NAMESPACE::class_descriptor self_class;                           \
    static GOODS_NAMESPACE::object* constructor(GOODS_NAMESPACE::hnd_t hnd, GOODS_NAMESPACE::class_descriptor& desc, \
                               size_t varying_size);              \
    CLASS(GOODS_NAMESPACE::hnd_t hnd, GOODS_NAMESPACE::class_descriptor& desc, size_t varying_size) \
    : BASE_CLASS(hnd, desc, varying_size) {}                      \
    virtual GOODS_NAMESPACE::field_descriptor& describe_components()
#else
//[MC] -- change static member self_class to function to avoid race condition
#define self_class get_self_class()
#define METACLASS_DECLARATIONS(CLASS, BASE_CLASS)                 \
    static GOODS_NAMESPACE::class_descriptor& get_self_class();                 \
    friend GOODS_NAMESPACE::class_descriptor& classof(CLASS const*);            \
    static GOODS_NAMESPACE::object* constructor(GOODS_NAMESPACE::hnd_t hnd, GOODS_NAMESPACE::class_descriptor& desc, \
                               size_t varying_size);              \
    CLASS(GOODS_NAMESPACE::hnd_t hnd, GOODS_NAMESPACE::class_descriptor& desc, size_t varying_size) \
    : BASE_CLASS(hnd, desc, varying_size) {}                      \
    virtual GOODS_NAMESPACE::field_descriptor& describe_components()
#endif

#if defined(_MSC_VER) && _MSC_VER < 1300
#define REGISTER_TEMPLATE(cls, bas, mop) REGISTER(cls, bas, mop)
#define REGISTER_ABSTRACT_TEMPLATE(cls, bas, mop) REGISTER_ABSTRACT(cls, bas, mop)
#define REGISTER_TEMPLATE_EX(cls, bas, mop, attr) REGISTER_EX(cls, bas, mop, attr)
#define REGISTER_ABSTRACT_TEMPLATE_EX(cls, bas, mop, attr) REGISTER_ABSTRACT_EX(cls, bas, mop, attr)
#else
//[MC] -- change static member self_class to function to avoid race condition
#define REGISTER_TEMPLATE(CLASS, BASE, MOP)                \
auto& CLASS##_self_class = CLASS::get_self_class();\
    GOODS_NAMESPACE::class_descriptor& classof(CLASS const*)   \
{ return CLASS::get_self_class(); }                              \
template<>                                                 \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,     \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
template<>                                                 \
GOODS_NAMESPACE::class_descriptor& CLASS::get_self_class() \
{ \
	static GOODS_NAMESPACE::class_descriptor _self_class(#CLASS, sizeof(CLASS), &MOP, &CLASS::constructor, &((CLASS*)0)->CLASS::describe_components(), &BASE::get_self_class()); \
	return _self_class; \
}

//[MC] -- change static member self_class to function to avoid race condition
#define REGISTER_ABSTRACT_TEMPLATE(CLASS, BASE, MOP)       \
auto& CLASS##_self_class = CLASS::get_self_class();\
GOODS_NAMESPACE::class_descriptor& classof(CLASS const*)   \
{ return CLASS::self_class; }                              \
template<>                                                 \
GOODS_NAMESPACE::class_descriptor& CLASS::get_self_class() \
{ \
	static GOODS_NAMESPACE::class_descriptor _self_class(#CLASS, sizeof(CLASS), &MOP, NULL, &((CLASS*)0)->CLASS::describe_components(), &BASE::get_self_class()); \
	return _self_class; \
}

#define REGISTER_TEMPLATE_EX(CLASS, BASE, MOP, ATTR)                \
auto& CLASS##_self_class = CLASS::get_self_class();\
    GOODS_NAMESPACE::class_descriptor& classof(CLASS const*)   \
{ return CLASS::get_self_class(); }                              \
template<>                                                 \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,     \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
template<>                                                 \
GOODS_NAMESPACE::class_descriptor& CLASS::get_self_class() \
{ \
	static GOODS_NAMESPACE::class_descriptor _self_class(#CLASS, sizeof(CLASS), &MOP, &CLASS::constructor, &((CLASS*)0)->CLASS::describe_components(), &BASE::get_self_class(), ATTR); \
	return _self_class; \
}

#define REGISTER_ABSTRACT_TEMPLATE_EX(CLASS, BASE, MOP, ATTR)   \
GOODS_NAMESPACE::class_descriptor& classof(CLASS const*)   \
{ return CLASS::self_class; }                              \
template<>                                                 \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,\
						    sizeof(CLASS),	\
						    &MOP,		\
						    NULL,		\
						    &((CLASS*)0)->CLASS::describe_components(), \
						    &BASE::self_class, ATTR)
#endif

//[MC] -- change static member self_class to function to avoid race condition
#define DO_REGISTER(CLASS, BASE, MOP, ATTR)\
GOODS_NAMESPACE::class_descriptor& classof(CLASS const*)   \
{ return CLASS::get_self_class(); }                              \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,                      \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
GOODS_NAMESPACE::class_descriptor& CLASS::get_self_class() \
{ \
	static GOODS_NAMESPACE::class_descriptor _self_class(#CLASS, sizeof(CLASS), &MOP, &CLASS::constructor, &((CLASS*)0)->CLASS::describe_components(), &BASE::get_self_class(), ATTR); \
	return _self_class; \
}

//[MC] -- change static member self_class to function to avoid race condition
#define REGISTER(CLASS, BASE, MOP)                         \
auto& CLASS##_self_class = CLASS::get_self_class();\
DO_REGISTER(CLASS, BASE, MOP, 0)

#define REGISTER_NAME(NAME, CLASS, BASE, MOP, ATTR)                         \
auto& NAME##_self_class = CLASS::get_self_class();\
DO_REGISTER(CLASS, BASE, MOP, ATTR)

//[MC] -- change static member self_class to function to avoid race condition
#define REGISTER_ABSTRACT(CLASS, BASE, MOP)                \
auto& CLASS##_self_class = CLASS::get_self_class();\
GOODS_NAMESPACE::class_descriptor& classof(CLASS const*)   \
{ return CLASS::self_class; }                              \
GOODS_NAMESPACE::class_descriptor& CLASS::get_self_class() \
{ \
	static GOODS_NAMESPACE::class_descriptor _self_class(#CLASS, sizeof(CLASS), &MOP, NULL, &((CLASS*)0)->CLASS::describe_components(), &BASE::get_self_class()); \
	return _self_class; \
}

//[MC] -- change static member self_class to function to avoid race condition
#define REGISTER_EX(CLASS, BASE, MOP, ATTR)                \
auto& CLASS##_self_class = CLASS::get_self_class();\
GOODS_NAMESPACE::class_descriptor& classof(CLASS const*)   \
{ return CLASS::self_class; }                              \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,     \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
GOODS_NAMESPACE::class_descriptor& CLASS::get_self_class() \
{ \
	static GOODS_NAMESPACE::class_descriptor _self_class(#CLASS, sizeof(CLASS), &MOP, &CLASS::constructor, &((CLASS*)0)->CLASS::describe_components(), &BASE::get_self_class(), ATTR); \
	return _self_class; \
}

//[MC] -- change static member self_class to function to avoid race condition
#define REGISTER_ABSTRACT_EX(CLASS, BASE, MOP, ATTR)       \
auto& CLASS##_self_class = CLASS::get_self_class();\
GOODS_NAMESPACE::class_descriptor& classof(CLASS const*)   \
{ return CLASS::self_class; }                              \
GOODS_NAMESPACE::class_descriptor& CLASS::get_self_class() \
{ \
	static GOODS_NAMESPACE::class_descriptor _self_class(#CLASS, sizeof(CLASS), &MOP, NULL, &((CLASS*)0)->CLASS::describe_components(), &BASE::get_self_class(), ATTR); \
	return _self_class; \
}

#else

#define METACLASS_DECLARATIONS(CLASS, BASE_CLASS)                 \
    static GOODS_NAMESPACE::class_descriptor self_class;                           \
    static GOODS_NAMESPACE::object* constructor(GOODS_NAMESPACE::hnd_t hnd, GOODS_NAMESPACE::class_descriptor& desc, \
                               size_t varying_size);              \
    CLASS(GOODS_NAMESPACE::hnd_t hnd, GOODS_NAMESPACE::class_descriptor& desc, size_t varying_size) \
    : BASE_CLASS(hnd, desc, varying_size) {}                      \
    virtual GOODS_NAMESPACE::field_descriptor& describe_components()


#if defined(_MSC_VER) && _MSC_VER < 1300
#define REGISTER_TEMPLATE(cls, bas, mop) REGISTER(cls, bas, mop)
#define REGISTER_ABSTRACT_TEMPLATE(cls, bas, mop) REGISTER_ABSTRACT(cls, bas, mop)
#define REGISTER_TEMPLATE_EX(cls, bas, mop, attr) REGISTER_EX(cls, bas, mop, attr)
#define REGISTER_ABSTRACT_TEMPLATE_EX(cls, bas, mop, attr) REGISTER_ABSTRACT_EX(cls, bas, mop, attr)
#else
#define REGISTER_TEMPLATE(CLASS, BASE, MOP)                \
template<>                                                 \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,                      \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
template<>                                                 \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,                 \
                                   sizeof(CLASS),          \
                                   &MOP,                   \
                                   &CLASS::constructor,    \
                                   &((CLASS*)0)->CLASS::describe_components(), \
                                   &BASE::self_class)

#define REGISTER_ABSTRACT_TEMPLATE(CLASS, BASE, MOP)       \
template<>                                                 \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,                 \
                                   sizeof(CLASS),          \
                                   &MOP,                   \
                                   NULL,                   \
                                   &((CLASS*)0)->CLASS::describe_components(), \
                                   &BASE::self_class)

#define REGISTER_TEMPLATE_EX(CLASS, BASE, MOP, ATTR)		   \
template<>                                                 \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,                      \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
template<>                                                 \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,                 \
						    sizeof(CLASS),	\
						    &MOP,		\
						    &CLASS::constructor, \
						    &((CLASS*)0)->CLASS::describe_components(), \
						    &BASE::self_class, ATTR)

#define REGISTER_ABSTRACT_TEMPLATE_EX(CLASS, BASE, MOP, ATTR)   \
template<>                                                 \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,                 \
						    sizeof(CLASS),	\
						    &MOP,		\
						    NULL,		\
						    &((CLASS*)0)->CLASS::describe_components(), \
						    &BASE::self_class, ATTR)
#endif

#define REGISTER(CLASS, BASE, MOP)                         \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,                      \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,                 \
                                   sizeof(CLASS),          \
                                   &MOP,                   \
                                   &CLASS::constructor,    \
                                   &((CLASS*)0)->CLASS::describe_components(), \
                                   &BASE::self_class)

#define REGISTER_ABSTRACT(CLASS, BASE, MOP)                \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,                 \
                                   sizeof(CLASS),          \
                                   &MOP,                   \
                                   NULL,                   \
                                   &((CLASS*)0)->CLASS::describe_components(), \
                                   &BASE::self_class)

#define REGISTER_EX(CLASS, BASE, MOP, ATTR)                \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,                      \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,                 \
                                   sizeof(CLASS),          \
                                   &MOP,                   \
                                   &CLASS::constructor,    \
                                   &((CLASS*)0)->CLASS::describe_components(), \
                                   &BASE::self_class,      \
                                   ATTR)

#define REGISTER_ABSTRACT_EX(CLASS, BASE, MOP, ATTR)       \
GOODS_NAMESPACE::object* CLASS::constructor(GOODS_NAMESPACE::hnd_t hnd,                      \
               GOODS_NAMESPACE::class_descriptor& desc, size_t var_len)     \
{ return new (desc, var_len) CLASS(hnd, desc, var_len); }  \
GOODS_NAMESPACE::class_descriptor CLASS::self_class(#CLASS,                 \
                                   sizeof(CLASS),          \
                                   &MOP,                   \
                                   NULL,                   \
                                   &((CLASS*)0)->CLASS::describe_components(), \
                                   &BASE::self_class,      \
                                   ATTR)
#endif

END_GOODS_NAMESPACE

#endif


