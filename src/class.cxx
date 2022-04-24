// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CLASS.CXX >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 28-Oct-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Runtime type information: class and fields descriptors 
//-------------------------------------------------------------------*--------*

#include "goods.h"
#ifdef _WIN32
#pragma hdrstop
#endif
#include <float.h>

#include "dbscls.h"

BEGIN_GOODS_NAMESPACE

//
// Application field descriptor methods
//

void* field_descriptor::operator new(size_t size)
{
    return ::operator new(size);
}

void  field_descriptor::operator delete(void* p)
{
    ::operator delete(p);
}

inline static field_type extract_field_type(field_descriptor* f_components)
{
	if (f_components == NULL || reinterpret_cast<size_t>(f_components) > size_t(fld_last))
	{
		return fld_structure;
	}

#ifdef _WIN64
	struct _deconstruct
	{
		int hi;
		int low;
	};
	return static_cast<field_type>(reinterpret_cast<_deconstruct*>(&f_components)->hi);
#else
	return static_cast<field_type>(int(f_components));
#endif
	
}

field_descriptor::field_descriptor(const char* f_name,
                                   int size, 
                                   int n_items, 
                                   int offs,
                                   field_descriptor* f_components,
                                   int f_flags)
{
    assert(f_name != NULL);
    name = f_name;
	loc.type = extract_field_type(f_components);
    loc.size = size;
    loc.n_items  = n_items;
    loc.offs = offs;
    dbs = loc;
    dbs.size = (loc.type == fld_reference) ? sizeof(dbs_reference_t) 
        : (loc.type == fld_string) ? 2 : (loc.type == fld_raw_binary) ? 4 : size;
    n_refs = 0;
    components = f_components;
    flags = f_flags;
}


field_descriptor::field_descriptor(field_descriptor const* new_field,
                                   dbs_field_descriptor const* old_field)
{
    name = new_field->name;
    loc = new_field->loc;
    components = new_field->components;
    dbs.type = (field_type)old_field->type;
    dbs.size = old_field->size;
    dbs.n_items = old_field->n_items;
	n_refs = 0;
	flags = new_field->flags;
}

field_descriptor::~field_descriptor()
{
    if (loc.type == fld_structure && components != NULL) { 
        field_descriptor *next = components, *field;
        do {
            field = next;
            next = (field_descriptor*)field->next;
            delete field;
        } while (next != components);
    }
}


inline char* convert_signed_integer(char* dst, size_t dst_size, 
                                    field_type dst_type,
                                    char* src, size_t src_size)
{
    union { 
        char buf[8];
        int8 val;
    } u;
    int8 ival;

    switch (src_size) { 
      case 1:
        if (dst_size == 1) { 
            *dst = *src++;
            return src;
        }
        ival = *src++;
        break;
      case 2:
        if (dst_size == 2) { 
            return unpack2(dst, src);
        }
        ival = (int2)unpack2(src);
        src += 2;
        break;
      case 4:
        if (dst_size == 4 && dst_type != fld_real) { 
            return unpack4(dst, src);
        }
        ival = (int4)unpack4(src);
        src += 4; 
        break;
      default:
        src = unpack8(u.buf, src);
        ival = u.val;
    }
    if (dst_type == fld_real) {
        if (dst_size == sizeof(real8)) { 
            *(real8*)dst = real8(ival);
        } else {
            *(real4*)dst = real4(ival);
        }               
    } else { 
        switch (dst_size) {
          case 1:
            *dst = char(ival);
            break;
          case 2:
            *(int2*)dst = int2(ival);
            break;
          case 4: 
            *(int4*)dst = int4(ival);
            break;
          default:
            *(int8*)dst = ival;
        }  
    }
    return src;
}

inline char* convert_unsigned_integer(char* dst, size_t dst_size, 
                                    field_type dst_type,
                                    char* src, size_t src_size)
{
    union { 
        char buf[8];
        nat8 val;
    } u;
    nat8 uval;

    switch (src_size) { 
      case 1:
        if (dst_size == 1) { 
            *dst = *src++;
            return src;
        }
        uval = nat1(*src++);
        break;
      case 2:
        if (dst_size == 2) { 
            return unpack2(dst, src);
        }
        uval = unpack2(src);
        src += 2;
        break;
      case 4:
        if (dst_size == 4 && dst_type != fld_real) { 
            return unpack4(dst, src);
        }
        uval = unpack4(src);
        src += 4; 
        break;
      default:
        src = unpack8(u.buf, src);
        uval = u.val;
    }
    if (dst_type == fld_real) {
        int8 ival = uval;
        if (ival < 0) { 
            if (dst_size == sizeof(real8)) { 
                *(real8*)dst = DBL_MAX;
            } else { 
                *(real4*)dst = FLT_MAX;
            }   
        } else { 
            if (dst_size == sizeof(real8)) { 
                *(real8*)dst = real8(ival);
            } else { 
                *(real4*)dst = real4(ival);
            }
        }
    } else { 
        switch (dst_size) {
          case 1:
            *dst = nat1(uval);
            break;
          case 2:
            *(nat2*)dst = nat2(uval);
            break;
          case 4: 
            *(nat4*)dst = nat4(uval);
            break;
          default:
            *(nat8*)dst = uval;
        }  
    }
    return src;
}

inline char* convert_real(char* dst, size_t dst_size, field_type dst_type,
                          char* src, size_t src_size)
{
    union { 
        char  buf[8];
        real8 val;
    } u;
    real8 rval;

    if (src_size == 4)  { 
        if (dst_type == fld_real && dst_size == 4) { 
            return unpack4(dst, src);
        }
        src = unpack4(u.buf, src);
        rval = *(real4*)u.buf;
    } else { 
        if (dst_type == fld_real && dst_size == 8) { 
            return unpack8(dst, src);
        }
        src = unpack8(u.buf, src);
        rval = u.val;
    }
    if (dst_type == fld_real) {
        if (dst_size == sizeof(real8)) { 
            *(real8*)dst = rval;
        } else { 
            *(real4*)dst = real4(rval);
        }               
    } else { 
        switch (dst_size) {
          case 1:
            *dst = (char)rval;
            break;
          case 2:
            *(int2*)dst = (int2)rval;
            break;
          case 4:
            *(int4*)dst = (int4)rval;
            break;
          default:
            *(int8*)dst = (int8)rval;
        }
    }
    return src;
}

//[MC] Function used to convert array of wchar to wstring_t
inline char* convert_integer_to_string(char* dst, size_t dst_size, 
										field_type dst_type,
										char* src, size_t src_size, 
										field_type src_type,  
										int n)
{
	//we can only convert char and wchar_t types
	assert(src_size <= 2);

	wstring_t& str = *(wstring_t*)dst;

	wchar_t* tmp = new wchar_t[n + 1];
	memset(tmp, 0, sizeof(wchar_t)*(n + 1));

	for (int i = 0; i < n; i++)
	{
		if (src_type == fld_unsigned_integer)
		{
			src = convert_unsigned_integer((char*)&tmp[i], sizeof(wchar_t), 
										   fld_unsigned_integer,				
										   src, src_size);
		}
		else
		{
			src = convert_signed_integer((char*)&tmp[i], sizeof(wchar_t), 
										 fld_signed_integer,				
										 src, src_size);
		}

		//end of string
		if(tmp[i] == 0)
		{
			src += (n-i-1)*src_size;
			break;		
		}
	}

	int len = (int)wcslen(tmp);
	if(len)
		str = tmp;
	else
		str.setNull();

	delete[] tmp;
	return src;
}

//[MC] Function used to convert array of char to rawbinary_t
inline char* convert_integer_to_raw_binary(	char* dst, size_t dst_size, 
											field_type dst_type,
											char* src, size_t src_size, 
											field_type src_type,  
											int n)
{
	//we can only convert char types
	assert(src_size <= 1);

	//Convert the entire array of chars
	raw_binary_t& bin = *(raw_binary_t*)dst;

	bin.set_size(n);    
    memcpy((char*)bin, src, n);
    src += n;

	return src;
}

void field_descriptor::unpack(field_descriptor* field,
							  obj_storage* storage,
							  char* dst_obj,
							  char* &src_refs,
							  char* &src_bins)
{
	field_descriptor* first = field;
	do
	{
		int    n = field->dbs.n_items;
		nat2   sid;
		objref_t opid;

		if (dst_obj == NULL || field->loc.offs < 0)
		{ // field is deleted
			if (field->loc.size == 1 && field->dbs.size == 1)
			{
				src_bins += n;
			}
			else
			{
				switch (field->dbs.type)
				{
					case fld_reference:
					src_refs += sizeof(dbs_reference_t)*n;
					break;
					case fld_string:
					while (--n >= 0)
					{
						int size = unpack2(src_bins);
						src_bins += 2;
						if (size != 0xFFFF)
						{
							src_bins += 2*size;
						}
					}
					break;
					case fld_raw_binary:
					while (--n >= 0)
					{
						int size = unpack4(src_bins);
						src_bins += 4 + size;
					}
					break;
					case fld_signed_integer:
					case fld_unsigned_integer:
					case fld_real:
					src_bins += field->dbs.size*n;
					break;
					case fld_structure:
					if (field->components != NULL)
					{
						while (--n >= 0)
						{
							unpack(field->components, storage, NULL, src_refs, src_bins);
						}
					}
					break;
					default:
					assert(((void)"Unsupported field type", False));
				}
			}
		}
		else
		{
			char*  dst = dst_obj + field->loc.offs;

			if (field->loc.size == 1 && field->dbs.size == 1)
			{
				while (--n >= 0)
				{
					*dst++ = *src_bins++;
				}
			}
			else
			{
				while (--n >= 0)
				{
					switch (field->dbs.type)
					{
						case fld_reference:
						src_refs = unpackref(sid, opid, src_refs);
						if (opid != 0)
						{
							*(hnd_t*)dst =
								object_handle::create_persistent_reference
								(storage->db->get_storage(sid), opid);
						}
						else
						{
							*(hnd_t*)dst = 0;
						}
						break;
						case fld_string:
						{
							wstring_t& str = *(wstring_t*)dst;
							int size = unpack2(src_bins);
							src_bins += 2;
							if (size == 0xFFFF)
							{
								str.chars = NULL;
								str.len = 0;
							}
							else
							{
								str.chars = new wchar_t[size+1];
								str.len = size + 1;
								for (int i = 0; i < size; i++)
								{
									str.chars[i] = unpack2(src_bins);
									src_bins += 2;
								}
								str.chars[size] = 0;
							}
							break;
						}
						case fld_raw_binary:
						{
							raw_binary_t& bin = *(raw_binary_t*)dst;
							int size = unpack4(src_bins);
							src_bins += 4;
							if (size == 0)
							{
								bin.data = NULL;
								bin.size = 0;
							}
							else
							{
								bin.data = new char[size];
								bin.size = size;
								memcpy(bin.data, src_bins, size);
								src_bins += size;
							}
							break;
						}
						case fld_signed_integer:
						{
							//[MC]	
							//Convert char, wchar_t or array of them representing a string
							//to a wstring_t field type
							if (field->loc.type == fld_string)
							{
								src_bins = convert_integer_to_string(dst,
																	 field->loc.size,
																	 field->loc.type,
																	 src_bins,
																	 field->dbs.size,
																	 field->dbs.type,
																	 n+1);
								n = 0; //end of loop
							}
							//Convert char or array of char to raw_binary field type
							else if (field->loc.type == fld_raw_binary)
							{
								src_bins = convert_integer_to_raw_binary(dst,
																		 field->loc.size,
																		 field->loc.type,
																		 src_bins,
																		 field->dbs.size,
																		 field->dbs.type,
																		 n+1);
								n = 0; //end of loop
							}
							else
							{
								src_bins = convert_signed_integer(dst, field->loc.size, field->loc.type, src_bins, field->dbs.size);
							}
						}
						break;
						case fld_unsigned_integer:
						{
							//[MC]
							//Convert char, wchar_t or array of them representing a string
							//to a wstring_t field type
							if (field->loc.type == fld_string)
							{
								src_bins = convert_integer_to_string(dst,
																	 field->loc.size,
																	 field->loc.type,
																	 src_bins,
																	 field->dbs.size,
																	 field->dbs.type,
																	 n+1);
								n = 0; //end of loop
							}
							//Convert unsigned char or array of unsigned char to raw_binary field type
							else if (field->loc.type == fld_raw_binary)
							{
								src_bins = convert_integer_to_raw_binary(dst,
																		 field->loc.size,
																		 field->loc.type,
																		 src_bins,
																		 field->dbs.size,
																		 field->dbs.type,
																		 n+1);
								n = 0; //end of loop
							}
							else
							{
								src_bins = convert_unsigned_integer(dst, field->loc.size, field->loc.type, src_bins, field->dbs.size);
							}
						}
						break;
						case fld_real:
						src_bins = convert_real(dst,
												field->loc.size, field->loc.type,
												src_bins, field->dbs.size);
						break;
						case fld_structure:
						if (field->components != NULL)
						{
							unpack(field->components, storage, dst,
								   src_refs, src_bins);
						}
						break;
						default:
						assert(((void)"Unsupported field type", False));
					}
					dst += field->loc.size;
				}
			}
		}
		field = (field_descriptor*)field->next;
	} while (field != first);
}

size_t field_descriptor::calculate_strings_size(field_descriptor* field, 
                                                char*             src_obj, 
                                                size_t            size)
{
    field_descriptor* first = field;
    do { 
        char* src = src_obj + field->loc.offs; 
        int   n = field->loc.n_items;
        switch (field->loc.type) {
          case fld_raw_binary:
            while (--n >= 0) { 
                raw_binary_t& bin = *(raw_binary_t*)src;
                size += bin.size;
                src += sizeof(raw_binary_t);
            }
            break;
          case fld_string:
            while (--n >= 0) { 
                wstring_t& str = *(wstring_t*)src;
                size += (str.len > 0 ? (str.len - 1) : 0) * 2;
                src += sizeof(wstring_t);
            }
            break;
          case fld_structure:
            if (field->components != NULL) { 
                while (--n >= 0) {              
                    size = calculate_strings_size(field->components, src, size);
                    src += field->loc.size;
                }
            }
          default:;
        }
        field = (field_descriptor*)field->next;
    } while (field != first);

    return size;
}
   

void field_descriptor::pack(field_descriptor* field, 
                             char* &dst_refs, 
                             char* &dst_bins,
                             char* src_obj,
                             hnd_t parent_hnd,
                             field_descriptor* varying,
                             int varying_length)
{
    field_descriptor* first = field;
    do { 
        char* src = src_obj + field->loc.offs; 
        int   n = field->loc.n_items;
        hnd_t hnd;
        
        if (field == varying) {
            n = varying_length;
        }
        if (field->loc.size == 1 && field->dbs.size == 1) { 
            while (--n >= 0) {
                *dst_bins++ = *src++;
            }
        } else { 
            while (--n >= 0) {
                switch (field->loc.type) {
                  case fld_reference:
                    hnd = *(hnd_t*)src;
                    if (hnd != 0) { 
                        if (hnd->opid == 0) {
                            //
                            // Make transient object persistent
                            //
                            hnd->obj->mop->make_persistent(hnd, parent_hnd);
                        }
                        dst_refs = packref(dst_refs, 
                                           hnd->storage->id, hnd->opid);
                    } else { 
                        assert(!(field->flags & fld_not_null));
                        dst_refs = packref(dst_refs, 0, 0);
                    }
                    break;
                  case fld_string:
                    { 
                        wstring_t& str = *(wstring_t*)src;
                        int size = str.len - 1;
                        dst_bins = pack2(dst_bins, size);
                        for (int i = 0; i < size; i++) { 
                            dst_bins = pack2(dst_bins, str.chars[i]);
                        }
                        break;
                    }               
                   case fld_raw_binary:
                    { 
                        raw_binary_t& bin = *(raw_binary_t*)src;
                        dst_bins = pack4(dst_bins, (nat4)bin.size);
                        memcpy(dst_bins, bin.data, bin.size);
                        dst_bins += bin.size;
                        break;
                    }               
                  case fld_signed_integer:
                  case fld_unsigned_integer:
                  case fld_real:
                    switch (field->loc.size) { 
                      case 1:
                        *dst_bins++ = *src;
                        break;
                      case 2:
                        dst_bins = pack2(dst_bins, src);
                        break;
                      case 4:
                        dst_bins = pack4(dst_bins, src);
                        break;
                      case 8:
                        dst_bins = pack8(dst_bins, src);
                    }   
                    break;
                  case fld_structure:
                    if (field->components != NULL) { 
                        pack(field->components, dst_refs, dst_bins, src, 
                             parent_hnd, varying, varying_length);
                    }
                  default:;
                }
                src += field->loc.size;
            }
        }
        field = (field_descriptor*)field->next;
    } while (field != first);
}

void field_descriptor::destroy_references(char* src, int len) 
{
    if (loc.type == fld_reference) { 
        while (--len >= 0) { 
            hnd_t hnd = *(hnd_t*)src; 
            object_handle::remove_reference(hnd);
            src += sizeof(hnd_t);
        }
    } else if (loc.type == fld_structure) { 
        while (--len >= 0) { 
            field_descriptor* field = components;
            if (field != NULL) { 
                field_descriptor* first = field;
                do { 
                    field->destroy_references(src + field->loc.offs, 
                                              field->loc.n_items); 
                    field = (field_descriptor*)field->next;
                } while(field != first);
            }
            src += loc.size;
        }
    }
}

//
// Class descriptor methods
//

class_descriptor* class_descriptor::hash_table[DESCRIPTOR_HASH_TABLE_SIZE];
cid_t             class_descriptor::last_ctid;

inline unsigned class_descriptor::hash_function(const char* name)
{ 
    return string_hash_function(name) % DESCRIPTOR_HASH_TABLE_SIZE;
}


class_descriptor* class_descriptor::find(const char* name) 
{ 
    class_descriptor* cdp;
    for (cdp = hash_table[hash_function(name)];
         cdp != NULL && strcmp(cdp->name, name) != 0;
         cdp = cdp->next);
    return cdp;
}


class_descriptor::class_descriptor(const char* cls_name, 
                                   size_t size, 
                                   metaobject* meta,
                                   constructor_t cons,
                                   field_descriptor* fieldList,
                                   class_descriptor* base,
                                   int attr)
: name(cls_name),
  ctid(++last_ctid),
  base_class(base), 
  constructor(cons), 
  mop(meta),
  new_cpid(0),
  new_class(this),
  fields(fieldList)
{
    assert(!dbs_desc); // this field should be staticaly inialized by linker
    assert(find(cls_name) == NULL); // class name should be unique
    unsigned h = hash_function(cls_name);
    next = hash_table[h];
    hash_table[h] = this;

    class_data = NULL;
    class_attr = attr;

    if (base != NULL) {
        next_derived = base->derived_classes;
        base->derived_classes = this;
    }

    fixed_size = size; 
    varying_size = 0;
    packed_fixed_size = packed_varying_size = 0;
    n_fixed_references = n_varying_references = 0;
    varying = NULL;
    has_strings = False;

    if (base == NULL || base->initialized()) {
        build_components_info();
    }
}

void class_descriptor::build_components_info()
{
    //
    // Class with instance components can't be derived
    // from class with varying component
    //
    assert(base_class == NULL || fields == NULL
          || base_class->varying_size == 0);

    if (base_class != NULL && base_class != &object::self_class) {
        field_descriptor* base_desc =
            new field_descriptor(base_class->name,
                                 (int)base_class->fixed_size, 1,
                                 0, base_class->fields);
        if (fields != NULL) {
            base_desc->link_before(fields);
        }
        fields = base_desc;
    }

    size_t n_fields = 0; // number of all fields in class (including subclasses)
    size_t name_offs = strlen(name) + 1;
    size_t total_names_size = name_offs; // total size of names of all fields
    if (fields != NULL) {
        n_fields = calculate_attributes(fields, packed_fixed_size,
                                        n_fixed_references, total_names_size);
    }
    dbs_desc = new (n_fields, total_names_size) dbs_class_descriptor;
	dbs_desc->fixed_size = (nat4)packed_fixed_size;
	dbs_desc->varying_size = (nat4)packed_varying_size;
	dbs_desc->n_fixed_references = (nat4)n_fixed_references;
	dbs_desc->n_varying_references = (nat4)n_varying_references;
	dbs_desc->n_fields = (nat4)n_fields;
	dbs_desc->total_names_size = (nat4)total_names_size;
    strcpy(dbs_desc->name(), name);

    if (fields != NULL) {
        name_offs += n_fields*sizeof(dbs_field_descriptor);
        build_dbs_class_descriptor(dbs_desc, 0, fields, name_offs);
    }

    for (class_descriptor* subclass = derived_classes;
         subclass != NULL;
         subclass = subclass->next_derived)
    {
        subclass->build_components_info();
    }
}
                                        

size_t class_descriptor::calculate_strings_size(char* obj) const
{
    return field_descriptor::calculate_strings_size(fields, obj, 0);
}
 
  
int class_descriptor::calculate_attributes(field_descriptor* field,
                                           size_t& size, 
                                           size_t& n_refs, 
                                           size_t& names_size)
{
    int n_fields = 0;
    field_descriptor *first = field;
    do { 
        size_t base_size = size;
        size_t base_n_refs = n_refs;

        n_fields += 1;
        names_size += strlen(field->name) + 1; 

        switch (field->loc.type) { 
          case fld_reference:
            size += sizeof(dbs_reference_t);
            n_refs += 1;
            break;
          case fld_raw_binary:
          case fld_string:
            assert(((void)"Varying components can not be used in class with strings or raw binary fields",
                    varying == NULL)); 
            has_strings = True;
            // no break
          case fld_signed_integer:
          case fld_unsigned_integer:
          case fld_real:
			size += field->dbs.size;
            break;
          case fld_structure:
            if (field->components != NULL) { 
                n_fields += calculate_attributes(field->components, 
                                                 size, n_refs, names_size);
            }
            field->n_refs = (int)(n_refs - base_n_refs);
            break;
          default: 
            assert(False);
        } 

        if (field->loc.n_items == 0) { 
            //
            // Only last component of class can be varying
            //
            assert(((void)"Varying component should be the last compoennt of the class without string components", 
                   varying == NULL && field->next == first && !has_strings)); 
            varying = field;
            packed_varying_size = size - base_size;
            packed_fixed_size -= packed_varying_size;
            fixed_size = field->loc.offs;
            varying_size = field->loc.size;
            n_varying_references = n_refs - base_n_refs;
            n_fixed_references -= n_varying_references;
        } else { 
            size += (size - base_size) * (field->loc.n_items - 1);
            n_refs += (n_refs - base_n_refs) * (field->loc.n_items - 1);
        }
        field = (field_descriptor*)field->next; 
    } while (field != first);

    return n_fields;
}


int class_descriptor::build_dbs_class_descriptor(dbs_class_descriptor* desc, 
                                                 int field_no,
                                                 field_descriptor* field,
                                                 size_t& name_offs)
{
    field_descriptor* first = field;
    dbs_field_descriptor dummy;
    dbs_field_descriptor* prev_dbs_field = &dummy;

    do { 
        dbs_field_descriptor* dbs_field = &desc->fields[field_no];
        dbs_field->name = (nat2)name_offs;
        strcpy(desc->names + name_offs, field->name);
        name_offs += strlen(field->name) + 1;
        dbs_field->size = field->dbs.size;
        dbs_field->n_items = field->dbs.n_items;
        dbs_field->type = field->dbs.type; 
        prev_dbs_field->next = field_no;
        prev_dbs_field = dbs_field;

        field_no += 1;

        if (dbs_field->type == fld_structure && field->components != NULL) {
            field_no = build_dbs_class_descriptor(desc,
                                                  field_no,
                                                  field->components,
                                                  name_offs);
        }
        field = (field_descriptor*)field->next; 
    } while (field != first);

    prev_dbs_field->next = 0;
    return field_no;
}
                                                  


void class_descriptor::unpack(dbs_object_header* buf, hnd_t hnd, int flags) 
{
    size_t dbs_size = buf->get_size();
    int varying_length = packed_varying_size
        ? (int)((dbs_size - packed_fixed_size) / packed_varying_size) : 0;

    object* obj = constructor(hnd, *new_class, varying_length);
    obj->cpid = new_cpid ? new_cpid : buf->get_cpid();

    if (fields != NULL) { 
        char* src_refs = buf->body();
        char* src_bins = src_refs + sizeof(dbs_reference_t) *
            (n_fixed_references + n_varying_references * varying_length);
                
        if (varying) {
			//[MC]
			//if field is being converted from varying to wstring then assign varying_length as n_items
			if (varying->dbs.type == fld_unsigned_integer && varying->loc.type == fld_string)
			{
				varying->dbs.n_items = varying_length;
			}
			else
			{
				varying->dbs.n_items =
					varying->loc.n_items != 0 && varying->loc.n_items < varying_length
					? varying->loc.n_items : varying_length;
			}
        }
        field_descriptor::unpack(fields, hnd->storage, (char*)obj, 
                                 src_refs, src_bins);
        if (varying) { 
            varying->dbs.n_items = 0;
        }
    }
	if (IS_VALID_OBJECT(hnd->obj)) {
		if ((flags & lof_bulk) == 0 && (hnd->obj->state & object::reloading)) {
			internal_assert((hnd->obj->state & (object::fixed | object::reloading))
							== (object::fixed | object::reloading));
			hnd->obj->become(obj);
			obj->state &= ~object_header::reloading;
		}
    } else { 
        if (hnd->obj == COPIED_OBJECT) { 
            //
            // Create transient copy of object
            //
            hnd->obj = obj;
            hnd->opid = 0;
        } else { 
            if (hnd->obj == INVALIDATED_OBJECT) { 
                obj->state |= object::invalidated; 
            } 
            obj->state |= object::persistent; 
            hnd->obj = obj;
            if (flags & lof_pin) { 
                obj->mop->pin_object(hnd);
            } else { 
            obj->mop->put_in_cache(hnd); 
        }
    }
    }
}       

void class_descriptor::pack(dbs_object_header* buf, hnd_t hnd) 
{
    object* obj = hnd->obj;
    int varying_length = 
        varying_size ? (int)((obj->size - fixed_size) / varying_size) : 0;

	buf->set_ref(hnd->opid);
    buf->set_cpid(obj->cpid);
    buf->set_size(packed_size((char*)obj, obj->size));
    char* dst_refs = buf->body();
    char* dst_bins = 
        dst_refs + (n_fixed_references + n_varying_references * varying_length)
                 * sizeof(dbs_reference_t); 
    if (fields) {
        field_descriptor::pack(fields, dst_refs, dst_bins, 
                               (char*)obj, hnd, varying, varying_length);
    }
}       

    
class_descriptor::class_descriptor(cpid_t new_dbs_cpid,
                                   class_descriptor* new_desc, 
                                   dbs_class_descriptor* old_desc)
: name(new_desc->name),
  ctid(++last_ctid), 
  base_class(new_desc->base_class),
  constructor(new_desc->constructor), 
  mop(new_desc->mop),
  new_cpid(new_dbs_cpid),
  new_class(new_desc)
{
    packed_fixed_size = old_desc->fixed_size;
    packed_varying_size = old_desc->varying_size;
    n_fixed_references = old_desc->n_fixed_references;    
    n_varying_references = old_desc->n_varying_references;
    fixed_size = new_desc->fixed_size;
    varying_size = new_desc->varying_size;
    varying = NULL; 
    fields = NULL;
    has_strings = False;
    dbs_desc = old_desc->clone();
	class_data = new_desc->class_data;
	class_attr = new_desc->class_attr;

    if (new_desc->fields != NULL) {
        size_t ref_offs = 0, bin_offs = 0;
        fields = create_field_mapping(dbs_desc,
                                      new_desc->fields,
                                      0, dbs_desc->n_fields,
                                      ref_offs, bin_offs); 
        //
        // If there is no correpondence between varying parts of
        // transient and persistent object, then include size of varying
        // part into fixed size of object
        //
        if (varying == NULL || varying->loc.n_items != 0) { 
            fixed_size += varying_size; 
            varying_size = 0;
        }
    } 
}

void class_descriptor::destroy_varying_part_references(object* obj) const
{
    if (n_varying_references > 0) { 
        int varying_length = (int)((obj->size - fixed_size) / varying_size);
        varying->destroy_references((char*)obj + fixed_size + 
                                    varying_size, varying_length - 1);
    }
}
                                          

static void skip_field(dbs_class_descriptor* desc, 
                       int field_no, int next_field,  
                       size_t& n_refs, size_t& n_bins)
{ 
    dbs_field_descriptor* field = &desc->fields[field_no];
    size_t base_n_refs = n_refs; 
    size_t base_n_bins = n_bins; 

    switch (field->type) { 
      case fld_structure:
        if (field->next != 0) { 
            next_field = field->next;
        }
        if (field_no+1 != next_field) { 
            for (int i = field_no+1; i != 0; i = desc->fields[i].next) { 
                skip_field(desc, i, next_field, n_refs, n_bins);
            }
        }
        break;
      case fld_reference:
        n_refs += 1;
        break;
      default: // scalar type
        n_bins += field->size;
    }
    if (field->n_items > 1) { 
        n_refs += (n_refs - base_n_refs) * (field->n_items-1); 
        n_bins += (n_bins - base_n_bins) * (field->n_items-1); 
    }
}


field_descriptor* class_descriptor::create_field_mapping(
                                    dbs_class_descriptor* old_desc,
                                    field_descriptor* new_field,
                                    int old_field_no,
                                    int n_dbs_fields,
                                    size_t& n_refs,
                                    size_t& n_bins)
{
    if (n_dbs_fields == 0) { 
        return NULL;
    } 
    field_descriptor* components = NULL;
    int end_of_region = old_field_no + n_dbs_fields;

    do { 
        dbs_field_descriptor* old_field = &old_desc->fields[old_field_no];
		field_descriptor* field;
		field_descriptor* first = new_field;
		if (first != NULL) {
			do {
				field = first;
				do {
					if (strcmp(field->name,
							   &old_desc->names[old_field->name]) == 0) 
					{
						//[MC]
						// Check if types of fields are compatible 
						// char, wchar_t and array of those types representing strings
						// can be converted to wstring_t types 
						// char, unisgned char and array of those types can be converted to raw_binary types
						assert(field->loc.type == old_field->type ||
							(IS_SCALAR(field->loc.type)
							&& IS_SCALAR(old_field->type)) ||
							(field->loc.type == fld_string &&
							(old_field->type == fld_unsigned_integer || old_field->type == fld_signed_integer) &&
							old_field->size <= 2) ||
							(field->loc.type == fld_raw_binary &&
							(old_field->type == fld_unsigned_integer || old_field->type == fld_signed_integer) &&
							old_field->size == 1));

						field = new field_descriptor(field, old_field);
						goto next_field;
					}
					field = (field_descriptor*)field->next;
				} while (field != first);
			} while (first->loc.type == fld_structure && (first = first->components) != NULL);
		}	
		//assert(((void)"Deleting fields from persistent objects is not yet supported", False));
        field = new field_descriptor(&old_desc->names[old_field->name], 
                                     old_field->size, 
                                     old_field->n_items,
                                     -1,
                                     NULL);        
        field->loc.type = field->dbs.type = (field_type)old_field->type;
        field->dbs.size = old_field->size;

      next_field: 
        {
            size_t base_n_refs = n_refs; 
            size_t base_n_bins = n_bins; 
            int n_components; 
            
            switch (field->loc.type) { 
              case fld_structure:
                n_components = old_field->next != 0
                    ? old_field->next - old_field_no - 1
                    : end_of_region - old_field_no - 1;
                field->components = 
                    create_field_mapping(old_desc, 
                                         field->components, 
                                         old_field_no+1,
                                         n_components,
                                         n_refs,
                                         n_bins);
                field->n_refs = (int)(n_refs - base_n_refs);
                field->dbs.size = 
                    (int)((n_refs - base_n_refs)*sizeof(dbs_reference_t)
                    + n_bins - base_n_bins); 
                break;
              case fld_reference:
                field->n_refs = 1;
                n_refs += 1;
                break;
              case fld_raw_binary:
              case fld_string:
                has_strings = 1;
                internal_assert(varying == NULL); 
                // no break
              default: // scalar type
                n_bins += field->dbs.size;
            }
            if (components) { 
                field->link_after(components->prev);
            } else { 
                components = field;
            }
            if (field->dbs.n_items > 1) { 
                n_refs += (n_refs-base_n_refs)*(field->dbs.n_items-1); 
                n_bins += (n_bins-base_n_bins)*(field->dbs.n_items-1); 
            }
			if (field->dbs.n_items == 0) // varying part
			{
				//[MC] 
				//internal_assert(!has_strings && varying == NULL);
				//we can now convert from varying to string
				internal_assert((field->loc.type != fld_raw_binary) && ((field->loc.type != fld_string) || (field->dbs.type == fld_unsigned_integer)) && varying == NULL);
				varying = field;
			}
			else if (field->loc.n_items == 0) { 
                packed_varying_size = field->dbs.size;
                packed_fixed_size -= field->dbs.n_items*packed_varying_size;
                internal_assert(!has_strings && varying == NULL); 
                varying = field; 
            }
			else if (field->loc.type == fld_string && 
					(field->dbs.type == fld_unsigned_integer || field->dbs.type == fld_signed_integer))
			{
				//[MC]	
				//do nothing because we are converting the array to string
			}
			else if (field->loc.type == fld_raw_binary && 
					(field->dbs.type == fld_unsigned_integer || field->dbs.type == fld_signed_integer))
			{
 				//[MC]	
				//do nothing because we are converting the array to raw_binary
			}
			else if (field->loc.n_items < field->dbs.n_items) {
                field->dbs.n_items = field->loc.n_items;
            }
            old_field_no = old_field->next; 
        }
    } while (old_field_no != 0);

    return components;
}

boolean class_descriptor::is_superclass_for(hnd_t hnd) const
{
	if (IS_VALID_OBJECT(hnd->obj)) {
		class_descriptor const* cls = &hnd->obj->cls;
		while (cls != NULL) { 
			if (cls == this) {
				return True;
			}
			cls = cls->base_class; 
		}
		return False; 
	}
	return True;
}

class_descriptor::~class_descriptor() 
{ 
    if (fields != NULL) { 
		field_descriptor *next = fields;        
        if (base_class != NULL && base_class != &object::self_class && new_cpid == 0) {
            if (new_cpid == 0) { 
                fields->components = NULL;
            }
        }
        do { 
            field_descriptor* field = next;
            next = (field_descriptor*)field->next;
            delete field;
        } while (next != fields);
       /*
        if (base_class != NULL && base_class != &object::self_class) {
            next = (field_descriptor*)fields->next; // skip base class 
        }
        while (next != fields) { 
            field = next;
            next = (field_descriptor*)field->next;
            delete field;
        }
        */
    }
    delete dbs_desc; 
}







END_GOODS_NAMESPACE
