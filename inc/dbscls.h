// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< DBSCLS.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     28-Mar-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 17-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Some common database classes
//-------------------------------------------------------------------*--------*

#ifndef __DBSCLS_H__
#define __DBSCLS_H__

#include "goods.h"
#include "goodsdlx.h"
#include "wwwapi.h"

#ifdef USE_STL
#include <string>
#ifndef USE_STRICT_STL
using namespace std;
#endif
#endif

BEGIN_GOODS_NAMESPACE

//
// Dynamicly reallocatable array template. Array is automatically
// expanded when new element is stored at position beyond array bound. 
// This template can't be used directly because it is impossible 
// to register template classes in database.
// Instead of this concrete instantiations of this template can be used. 
//
template<class T>
class array_template : public object { 
  protected: 
    nat4 used;
    T    array[1];

    virtual array_template* clone(size_t new_size) const { 
        return new (cls, new_size) array_template(0, cls, new_size); 
    }

    array_template* set_size(nat4 n) { 
        nat4 allocated = (nat4)((size - this_offsetof(array_template, array)) / sizeof(T)); 
        if (n > allocated) { 
            array_template* new_arr = clone(n < allocated*2 ? allocated*2 : n);
            memcpy(new_arr->array, array, used*sizeof(T));
            memset(array, 0, used*sizeof(T));
            new_arr->used = n; 
            new_arr->mop = mop;
            become(new_arr); 
            return new_arr; 
        }
        used = n; 
        return this; 
    }
  public:
    virtual ref<array_template<T> > clone() const { 
        array_template* new_arr = new (cls, used) array_template(cls, used, used);
        for (int i = used; --i >= 0;) { 
            new_arr->array[i] = array[i];
        }
        return new_arr;
    }

    static ref<array_template<T> > create(size_t allocated_size,
                                          size_t init_size = 0) 
    { 
        assert(init_size <= allocated_size);
        if (allocated_size == 0) { 
            allocated_size = 1;
        }
        return new (self_class, allocated_size) array_template(self_class,
                                                               allocated_size,
                                                               init_size);
    }

    static ref<array_template<T> > create() { 
        return create(4, 0);
    }

    static ref<array_template<T> > create(ref<array_template<T> > copy) { 
        return copy->clone();
    }

    static ref<array_template<T> > create(T const* buf, size_t size) { 
        return new (self_class, size) array_template(self_class, buf, size);
    }

    size_t length() const { return used; }

    //
    // Array elements accessors
    //
    void putat(nat4 index, T const& elem) { 
        if (index >= used) { 
            set_size(index+1)->array[index] = elem; 
        } else { 
            array[index] = elem; 
        }
    }  
    T getat(nat4 index) const { 
        assert(index < used); 
        return array[index]; 
    }  
    T operator[](nat4 index) const {  
        assert(index < used); 
        return array[index]; 
    }  
    
    //
    // Stack protocol
    //
    void push(T const& elem) { 
        int n = used; 
        set_size(n+1)->array[n] = elem;
    }
    T pop() { 
        assert(used > 0);
        used -= 1;
        return array[used]; 
    }
    T top() const { 
        assert(used > 0);
        return array[used-1];
    }
    
    //
    // Massive operation
    // 
    void clear() {
        set_size( 0);
    }

    void insert(nat4 pos, T const& elem) { 
        nat4 n = used;
        assert(pos <= n);
        array_template* arr = set_size(n+1);
        memmove(&arr->array[pos+1], &arr->array[pos], (n - pos)*sizeof(T));
        memset(&arr->array[pos], 0, sizeof(T)); 
        arr->array[pos] = elem; 
    }

    void append(T const* tail, size_t len) { 
        nat4 n = used;
        array_template* arr = set_size(n+len);
        for (size_t i = 0; i < len; i++) { 
            arr->array[i+n] = tail[i];
        }
    }   

    void remove(nat4 pos) {
        assert(pos < used);
        T zero = (T)0;
        array[pos] = zero;
        memcpy(&array[pos], &array[pos+1], (used - pos - 1)*sizeof(T)); 
        used -= 1;
    }

    /** 
     * Iterates over all elements and applies the provided function to each element.
     * The iteration will be stopped if function f returns true.
     * @param f - pointer to function to be applied to each element
     * @param arg - argument to be passed to function f
     * @return The index of the last element to which f has been applied to 
     * or number of elements if function never returns true
     */
    int iterate(boolean (*f)(const T &mbr, void const* arg), 
                void const* arg = NULL) const 
    {
        int i = -1;
        while (++i < (int)used && !f( array[i], arg));
        return i;
    }

    /** 
     * Iterates over all elements in reverse order and applies the provided function to each element.
     * The iteration will be stopped if function f returns true.
     * @param f - pointer to function to be applied to each element
     * @param arg - argument to be passed to function f
     * @return The index of the last element to which f has been applied to 
     * or -1 if function never returns true
     */
    int reverseIterate(boolean (*f)(const T &mbr, void const* arg), 
                void const* arg = NULL) const 
    {
        int i = used; 
        while (--i >= 0 && !f(array[i], arg));
        return i;
    }

    int index_of(T const& elem) const {
        for (int i = 0, n = used; i < n; i++) { 
            if (array[i] == elem) { 
                return i;
            }
        }
        return -1;
    }

    int last_index_of(T const& elem) const {
        int i = used; 
        while (--i >= 0 && array[i] != elem);
        return i;
    }

    T*  copy() const { 
        T* arr = NEW T[used];
        int n = used; 
        while (--n >= 0) { 
            arr[n] = array[n]; 
        }
        return arr;
    }

    void copy(T* array, int from, int len) const { 
        assert(unsigned(from + len) <= used && from >= 0 && len >= 0);
        int n = len; 
        while (--n >= 0) { 
            array[n] = this->array[n + from]; 
        }
    }
 
    METACLASS_DECLARATIONS(array_template, object); 

  protected: 
    array_template(class_descriptor& desc, size_t allocated_size, 
                   size_t init_size) 
    : object(desc, allocated_size) 
    {
        used = (nat4)init_size;
		//[MC]
		memset(array, 0, allocated_size * sizeof(T));
    }

    array_template(class_descriptor& desc, T const* buf, size_t buf_size)  
    : object(desc, buf_size) 
    {
        used = buf_size;
        for (size_t i = 0; i < buf_size; i++) { 
            array[i] = buf[i];
        }
    }
};

template<class T>
inline field_descriptor& array_template<T>::describe_components() { 
    return FIELD(used), VARYING(array); 
}

typedef array_template<char>          ArrayOfByte; 
typedef array_template<int4>          ArrayOfInt; 
typedef array_template<real8>         ArrayOfDouble; 
typedef array_template< ref<object> > ArrayOfObject; 

class GOODS_DLL_EXPORT String : public ArrayOfByte { 
  protected: 
    virtual array_template<char>* clone(size_t new_size) const { 
        return new (cls, new_size) String(0, cls, new_size); 
    }
  public: 

#ifdef USE_LOCALE_SETTINGS
    int  compare(const char* str) const { return strcoll(array, str); }
    int  compareIgnoreCase(const char* str) const { 
        return stricoll(array, str); 
    }
#else
    int  compare(const char* str) const { return strcmp(array, str); }
    int  compareIgnoreCase(const char* str) const {
        return stricmp(array, str); 
    }   
#endif
    int  compare(ref<String> const& str) const { return -str->compare(array); }
    int  compareIgnoreCase(ref<String> const& str) const { 
        return -str->compareIgnoreCase(array); 
    }

    int  index(const char* str) const { 
        char const* p = strstr(array, str); 
        return p ? p - array : -1; 
    }
    
    void append(const char* tail) { 
        nat4 n = used ? used-1 : 0;
        size_t len = strlen(tail) + 1;
        String* s = (String*)set_size(n+(nat4)len);
        for (size_t i = 0; i < len; i++) { 
            s->array[i+n] = tail[i];
        }
    }   

#ifdef USE_STL
    std::string getString() const { 
        return std::string(array);
    }
    static ref<String> create(std::string const& str) { 
        return create(str.data());
    }
#endif
    char* data() const { 
        char* str = NEW char[used+1];
        strcpy(str, array);
        return str;
    }

    WWWconnection& toPage(WWWconnection& con) const { 
        return con.append(array);
    }

    friend WWWconnection& operator << (WWWconnection& con, ref<String> str);

    virtual ref<ArrayOfByte> clone() const { 
        return (ref<ArrayOfByte>)create(array); 
    }

    static ref<String> create(ref<String> copy) { 
        return (ref<String>)copy->clone();
    }

    static ref<String> create(size_t init_size = 0) { 
        return new (self_class, init_size) String(self_class, init_size); 
    }

    static ref<String> create(const char* str) { 
        size_t len = strlen(str)+1;
        String* s = new (self_class, len) String(self_class, len); 
        strcpy(s->array, str);
        return s;
    }

    void replace(const char* str) { 
        String* s = (String*)set_size((nat4)strlen(str)+1);
        strcpy(s->array, str);
    }

    const char* get_text() const {
        // it is safe to use pointer to buffer of persistent string 
        // only within transaction in which this object was included
        return array;
    }

    void print() const { console::output(array); }

    METACLASS_DECLARATIONS(String, ArrayOfByte); 

  protected: 
    String(class_descriptor& desc, size_t init_size) 
    : ArrayOfByte(desc, init_size, init_size) {}
}; 

inline WWWconnection& operator << (WWWconnection& con, ref<String> str) { 
    return str->toPage(con);
}


//
// Representation of one-to-many relationship: set of objects.
// Set conssist of set owner and a number of set members. 
// Each member contains references to previouse and next member, 
// reference to set owner, reference to associated object 
// and full key of this object. First (last) member has next (prev) field 
// equal to NULL. Member of a set has virtual function to compare
// two members. Also default function for converting complete key 
// to short integer form
//

typedef nat8 skey_t;

class set_owner;

class obj_storage;

/**
 * Iterator through query result set
 */
class GOODS_DLL_EXPORT result_set_cursor
{
    friend class obj_storage;
  public:
    /** 
     * Move cursor forward
     * @return next object matching query or Nil if no more objects
     */
    anyref next();

	void select(hnd_t owner, char const* table, char const* where = NULL, char const* order_by = NULL, nat4 limit = 0, nat4 offset = 0);

	void select(hnd_t first, char const* query, nat4 buf_size = UINT_MAX, int flags = qf_default, nat4 max_members = UINT_MAX);

	void select_range(hnd_t first, hnd_t last, char const* query, nat4 buf_size = UINT_MAX, int flags = qf_default, nat4 max_members = UINT_MAX);

	void clear();

    result_set_cursor();

  private:
    dnm_string query;
    dnm_array<anyref> result;
    nat4 buf_size;
    nat4 max_members;
    int  flags;
	size_t i;
	objref_t first_mbr;
	objref_t last_mbr;
    obj_storage* storage;
};

class GOODS_DLL_EXPORT set_member : public object {
    friend class set_owner;
  public: 
    ref<set_member> next; 
    ref<set_member> prev; 
    ref<set_owner>  owner; 
    anyref          obj;

    char            key[1];

    static ref<set_member> create(anyref obj, const char* key) { 
        size_t key_size = strlen(key) + 1;
        return new (self_class, key_size) 
            set_member(self_class, obj, key, key_size);
    }
    static ref<set_member> create(anyref obj, const char* key, 
                                  size_t key_size) 
    { 
        return new (self_class, key_size) 
            set_member(self_class, obj, key, key_size);
    }

    ref<set_member> first() const;  
    ref<set_member> last() const;  

    void filter(result_set_cursor& cursor, char const* query, nat4 buf_size = UINT_MAX, int flags = qf_default, nat4 max_members = UINT_MAX) const;

    size_t          copyKeyTo(char* buf, size_t bufSize) const;

    inline size_t   getKeyLength() const { 
        return size - this_offsetof(set_member, key);
    }

    virtual int     compare(ref<set_member> mbr) const;
    virtual int     compare(const char* key, size_t size) const; 
    virtual int     compare(const char* key) const; 
    virtual int     compareIgnoreCase(const char* key) const; 
    virtual skey_t  get_key() const; 
    virtual void    clear();
    static skey_t   str2key(const char* s, size_t len);

    METACLASS_DECLARATIONS(set_member, object);

  protected: 

    set_member(class_descriptor& aDesc, anyref const& obj, 
               const char* key, size_t key_size) 
    : object(aDesc, key_size)
    { 
        next = NULL;
        prev = NULL; 
        owner = NULL; 
        this->obj = obj; 
        memcpy(this->key, key, key_size);
    }
}; 



class GOODS_DLL_EXPORT set_owner : public object { 
  public: 
    ref<set_member> first; 
    ref<set_member> last; 
    anyref          obj;
    nat4            n_members;
    
    boolean empty() const { return n_members == 0; }

    virtual ref<set_member> find(const char* key, size_t len) const; 

    virtual ref<set_member> find(const char* key) const { 
        return find(key, strlen(key)+1);
    }

    virtual ref<set_member> find(const wchar_t* str) const;

    void put_first(ref<set_member> mbr);
    void put_last(ref<set_member> mbr, boolean cluster = False);
    void put_after(ref<set_member> after, ref<set_member> mbr);
    void put_before(ref<set_member> before, ref<set_member> mbr);

	void preload_members(boolean pin = True) const;

	void select(result_set_cursor& cursor, char const* table, char const* where = NULL, char const* order_by = NULL, nat4 limit = 0, nat4 offset = 0) const;

    void filter(result_set_cursor& cursor, char const* query, nat4 buf_size = UINT_MAX, int flags = qf_default, nat4 max_members = UINT_MAX) const;
	
	void filter_range(result_set_cursor& cursor, ref<set_member> const& start, ref<set_member> const& end, char const* query, nat4 buf_size = UINT_MAX, int flags = qf_default, nat4 max_members = UINT_MAX) const;

    virtual void clear();

    virtual void insert(char const* key, anyref obj) { 
        put_last(set_member::create(obj, key));
    }

    virtual void insert(ref<set_member> mbr) { 		
		//[MC] Avoid inserting a used member in the release version
		if(mbr->owner != NULL || mbr->next != NULL || mbr->prev != NULL)
		{
			abort_transaction();
			exit(EXIT_FAILURE);
		}
        put_last(mbr);
    }

    virtual boolean insert_unique(char const* key, anyref obj) { 
        if (find(key) == NULL) { 
            insert(key, obj);
            return True;
        }
        return False;
    }

    typedef void (*member_function)(ref<set_member> mbr, void const* arg);
    size_t iterate(member_function, void const* arg = NULL) const;

    ref<set_member> get_first(); 
    ref<set_member> get_last(); 

    virtual void remove(ref<set_member> mbr);

    static ref<set_owner> create(anyref obj) { 
        return NEW set_owner(self_class, obj); 
    } 

    METACLASS_DECLARATIONS(set_owner, object);

  protected: 
    set_owner(class_descriptor& desc, anyref const& obj, int varying=0) 
    : object(desc, varying)
    { 
        this->obj = obj; 
        n_members = 0;
    }
};  

class B_page;

//
// B tree - multibranch balanced tree, provides fast access for objects 
// in database. B_tree class is derived from set_owner and use set members
// to reference objects. 
//
class GOODS_DLL_EXPORT B_tree : public set_owner {
  public:
    //
    // Default minimal number of used items at not root page 
    //
    enum { dflt_n = (4096-4)/(sizeof(skey_t)+sizeof(dbs_reference_t))/2 }; 
    // enum { dflt_n = 2 };    // only for testing

    virtual ref<set_member> find(const char* str, size_t len, skey_t key) const;
    virtual ref<set_member> find(const char* str) const;
    virtual ref<set_member> find(skey_t key) const;
	virtual ref<set_member> find(const wchar_t* str) const {
		return set_owner::find((const wchar_t*)str);
	}
    
    // Find member with greater or equal key
    virtual ref<set_member> findGE(skey_t key) const;
    virtual ref<set_member> findGE(const char* str, size_t len, skey_t key) const;
    virtual ref<set_member> findGE(const char* str) const;

    virtual void insert(ref<set_member> mbr);
    virtual void remove(ref<set_member> mbr);

    virtual void insert(char const* key, anyref obj) { 
        insert(set_member::create(obj, key));
    }

    void            dump() const;
    void            ok() const;

    nat4            get_height() const { return height; }

    virtual void    clear();

    static ref<B_tree> create(anyref obj, nat4 min_used = dflt_n) {
        return NEW B_tree(self_class, obj, 0, min_used);
    }

	//[MC] -- change static member self_class to function to avoid race condition
    static class_descriptor& self_class; 
    friend class_descriptor& classof(B_tree const*); 
    static object* constructor(hnd_t hnd, class_descriptor& desc, 
                               size_t varying_size); 
    B_tree(hnd_t hnd, class_descriptor& desc, size_t varying_size) 
    : set_owner(hnd, desc, varying_size), n(dflt_n) {} 
    virtual field_descriptor& describe_components(); 

  protected:
    friend class B_page;

    ref<B_page> root;   // root page
    nat4        height; // height of tree
    int4        n;      // minimal number of used items at not root page

    B_tree(class_descriptor& desc, anyref const& obj=NULL, int varying=0,
        nat4 min_used = dflt_n) 
    : set_owner(desc, obj, varying)
    {
        root = NULL; 
        height = 0;
        n = min_used;
    }
}; 

class GOODS_DLL_EXPORT B_page : public object { 
  public: 
    int4 m;            // Index of first used item

    struct item { 
        anyref p; // for leaf page - pointer to 'set_member' object
                       // for all other pages - pointer to child page
        skey_t key; 

        field_descriptor& describe_components() { 
            return FIELD(p), FIELD(key);
        } 
    } e[1];

    friend field_descriptor& describe_field(item& s);

	//
    // Class invariants: 
    //
    //   0 <= m <= 2*n-2, for root page
    //   0 <= m <= n, for other pages
    //
    // For not leaf page:
    // ( for all i : m < i < 2*n : e[i-1].key < e[i].key ) &&
    // ( for all i, j : m <= i < 2*n, e[i].p->m <= j < 2*n-1 :
    //   e[i].key > e[i].p->e[j].key &&
    //   e[i].key == e[i].p->e[2*n-1].key )
    //

    ref<set_member> find(int height, skey_t key, int4 n) const; 
    ref<set_member> findGE(int height, skey_t key, int4 n) const; 

    enum operation_effect { 
        done, 
        overflow,
        underflow, 
        propagation
    };  

    operation_effect insert(int r, item& ins, int4 n);
    operation_effect remove(int r, item& rem, int4 n);
    operation_effect handle_underflow(int r, skey_t key, item& rem, int4 n);

    operation_effect find_insert_position(int level, 
                                          ref<B_tree> const& tree, 
                                          item& ins, int4 n) const;
    operation_effect find_remove_position(int level, item& rem, int4 n) const;

    void dump(int level, int height, int4 n) const;
    void ok(skey_t min_key_val, skey_t max_key_val,
            int level, int height, int4 n) const;

    B_page(size_t varying_length) : object(self_class, varying_length) {}

    // create new B tree root
    B_page(ref<B_page> const& root, item& ins, int4 n); 

    METACLASS_DECLARATIONS(B_page, object);

    static ref<B_page> create(int4 n) { 
        size_t len = n*2;
        B_page* p = new (self_class, len) B_page(len);
        return p;
    }

    static ref<B_page> create(ref<B_page> const& root, item& ins, int4 n) { 
        size_t len = n*2;
        B_page* p = new (self_class, len) B_page(root, ins, n); 
        return p;
    }
}; 

inline field_descriptor& describe_field(B_page::item& s) { 
    return s.describe_components();
}


//
// B-Tree implementation for string keys 
//
template<size_t key_size>
struct strkey_t 
{ 
    char body[key_size];

    boolean operator > (strkey_t const& other) const { 
        return memcmp(body, other.body, key_size) > 0;
    }

    boolean operator >= (strkey_t const& other) const { 
        return memcmp(body, other.body, key_size) >= 0;
    }

    boolean operator < (strkey_t const& other) const { 
        return memcmp(body, other.body, key_size) < 0;
    }

    boolean operator <= (strkey_t const& other) const { 
        return memcmp(body, other.body, key_size) <= 0;
    }

    boolean operator == (strkey_t const& other) const { 
        return memcmp(body, other.body, key_size) == 0;
    }

    boolean operator != (strkey_t const& other) const { 
        return memcmp(body, other.body, key_size) != 0;
    }

    boolean is_max() const { 
        for (size_t i = 0; i < key_size; i++) { 
            if (body[i] != (char)0xFF) { 
                return false;
            }
        }
        return true;
    }
    
    void set_max() { 
        for (size_t i = 0; i < key_size; i++) { 
            body[i] = (char)0xFF;
        }
    }

    void extract(ref<set_member> const& mbr) { 
        size_t size = mbr->copyKeyTo(body, key_size);
        if (size < key_size) { 
            memset(body + size, 0, key_size - size);
        }
    }
        
    field_descriptor& describe_components() { 
        return ARRAY(body);
    }    

    strkey_t() {}

    strkey_t(char const* key, size_t size) { 
        if (size > key_size) { 
            size = key_size;
        }
        memcpy(body, key, size);
        if (size < key_size) { 
            memset(body + size, 0, key_size - size);
        }
    }
};

template<size_t key_size>
inline field_descriptor& describe_field(strkey_t<key_size>& s) {
    return s.describe_components();
}


template<size_t key_size>
struct SB_item { 
    anyref p; // for leaf page - pointer to 'set_member' object
    // for all other pages - pointer to child page
    strkey_t<key_size> key;
    
    field_descriptor& describe_components() { 
        return FIELD(p), FIELD(key);
    } 
};

template<size_t key_size>
inline field_descriptor& describe_field(SB_item<key_size>& s) {
    return s.describe_components();
}


template<size_t key_size>
class SB_page;

template<size_t key_size>
class SB_tree : public set_owner 
{
  public:
    //
    // Default minimal number of used items at not root page 
    //
    enum { dflt_n = (4096-4)/(key_size + sizeof(dbs_reference_t))/2 }; 

    typedef SB_page<key_size> page_t;
    typedef ref< SB_page<key_size> > pageref_t;
    typedef ref< SB_tree<key_size> > treeref_t;
    typedef strkey_t<key_size> key_t;
    typedef SB_item<key_size> item_t;

	virtual ref<set_member> find(const char* str, size_t len, key_t key) const
	{
		if (root != NULL) { 
			ref<set_member> mbr = find(key);
			while (mbr != NULL) { 
				int diff = mbr->compare(str, len);
				if (diff == 0) { 
					return mbr;
				} else if (diff > 0) { 
					return NULL; 
				}
				mbr = mbr->next;
			}
		}
		return NULL;
	}

	virtual ref<set_member> find(key_t key) const
	{
		if (root.is_nil()) { 
			return NULL; 
		} else { 
			return root->find(height, key, n); 
		}
	}

    virtual ref<set_member> find(const char* str, size_t len) const
    {
        if (root != NULL) { 
            ref<set_member> mbr = root->find(height, key_t(str, len), n);
            while (mbr != NULL) { 
                int diff = mbr->compare(str, len);
                if (diff == 0) { 
                    return mbr;
                } else if (diff > 0) { 
                    return NULL; 
                }
                mbr = mbr->next;
            }
        }
        return NULL;
    }

    virtual ref<set_member> find(const char* str) const {
        return find(str, strlen(str) + 1);
    }

	virtual ref<set_member> find(const wchar_t* str) const {
		return set_owner::find((const wchar_t*)str);
	}

    
    // Find member with greater or equal key
    virtual ref<set_member> findGE(const char* str, size_t len) const {
        if (root != NULL) { 
            ref<set_member> mbr = root->findGE(height, key_t(str, len), n);
            while (mbr != NULL) { 
                int diff = mbr->compare(str, len);
                if (diff >= 0) { 
                    return mbr;
                }
                mbr = mbr->next;
            }
        }
        return NULL;
    }

	virtual ref<set_member> findGE(key_t key) const
	{
		if (root.is_nil()) { 
			return NULL; 
		} else { 
			return root->findGE(height, key, n); 
		}
	}

	virtual ref<set_member> findGE(const char* str, size_t len, key_t key) const
	{
		if (root != NULL) { 
			ref<set_member> mbr = findGE(key);
			while (mbr != NULL) { 
				int diff = mbr->compare(str, len);
				if (diff >= 0) { 
					return mbr;
				}
				mbr = mbr->next;
			}
		}
		return NULL;
	}

    virtual ref<set_member> findGE(const char* str) const {
        return findGE(str, strlen(str) + 1);
    }

    virtual void insert(ref<set_member> mbr) {   
		//[MC] Avoid inserting a used member in the release version 
		if(mbr->owner != NULL || mbr->next != NULL || mbr->prev != NULL)
		{
			abort_transaction();
			exit(EXIT_FAILURE);
		}

        item_t ins;
        ins.p = mbr;
        ins.key.extract(mbr);
        
        if (root == NULL) { 
            put_last(mbr); 
            root = page_t::create(NULL, ins, n); 
            height = 1;
        } else if (root->find_insert_position(height, this, ins, n) == page_t::overflow) { 
            root = page_t::create(root, ins, n);
            height += 1;
        } 
    }

    virtual void remove(ref<set_member> mbr) {
        item_t rem; 
        rem.p = mbr; 
        rem.key.extract(mbr);
    
        if (modify(root)->find_remove_position(height, rem, n) == page_t::underflow
            && root->m == n*2-1) 
        { 
            root = root->e[n*2-1].p;
            height -= 1;
        }
        set_owner::remove(mbr);
    }
    
    virtual void insert(char const* key, anyref obj) { 
        insert(set_member::create(obj, key));
    }

    nat4 get_height() const { return height; }

    virtual void clear() {
        set_owner::clear();
        root = NULL;
        height = 0;
    }

    static treeref_t create(anyref obj, nat4 min_used = dflt_n) {
        return NEW SB_tree<key_size>(self_class, obj, 0, min_used);
    }

    METACLASS_DECLARATIONS(SB_tree, set_owner);

  protected:
    pageref_t root;   // root page
    nat4      height; // height of tree
    int4      n;      // minimal number of used items at not root page

    SB_tree(class_descriptor& desc, anyref const& obj=NULL, int varying=0, nat4 min_used = dflt_n)
    : set_owner(desc, obj, varying)
    {
        root = NULL; 
        height = 0;
        n = min_used;
    }
}; 

template<size_t key_size>
inline field_descriptor& SB_tree<key_size>::describe_components() { 
    return FIELD(root), FIELD(height), FIELD(n); 
}

template<size_t key_size>
class SB_page : public object 
{ 
  public: 
    int4 m;            // Index of first used item

    typedef ref< SB_page<key_size> > pageref_t;
    typedef ref< SB_tree<key_size> > treeref_t;
    typedef strkey_t<key_size> key_t;
    typedef SB_item<key_size> item_t;

    item_t e[1];

    //
    // Class invariants: 
    //
    //   0 <= m <= 2*n-2, for root page
    //   0 <= m <= n, for other pages
    //
    // For not leaf page:
    // ( for all i : m < i < 2*n : e[i-1].key < e[i].key ) &&
    // ( for all i, j : m <= i < 2*n, e[i].p->m <= j < 2*n-1 :
    //   e[i].key > e[i].p->e[j].key &&
    //   e[i].key == e[i].p->e[2*n-1].key )
    //

    ref<set_member> find(int height, key_t const& key, int4 n) const
    {
        int l = m, r = 2*n-1;
        while (l < r)  {
            int i = (l+r) >> 1;
            if (key > e[i].key) l = i+1; else r = i;
        }
        internal_assert(r == l && e[r].key >= key); 
        if (--height != 0) { 
            pageref_t child = (pageref_t)e[r].p;
            return child->find(height, key, n); 
        } else { 
            if (key == e[r].key) { 
                return (ref<set_member>)e[r].p; 
            } else { 
                return NULL;
            }
        }
    }

    ref<set_member> findGE(int height, key_t const& key, int4 n) const
    {
        int l = m, r = 2*n-1;
        while (l < r)  {
            int i = (l+r) >> 1;
            if (key > e[i].key) l = i+1; else r = i;
        }
        internal_assert(r == l && e[r].key >= key); 
        if (--height != 0) { 
            pageref_t child = (pageref_t)e[r].p;
            return child->findGE(height, key, n); 
        } else { 
            return (ref<set_member>)e[r].p; 
        }
    }

    enum operation_effect { 
        done, 
        overflow,
        underflow, 
        propagation
    };  

    operation_effect insert(int r, item_t& ins, int4 n) {
        // insert before e[r]
        if (m > 0) {
            memmove(&e[m-1], &e[m], (r - m)*sizeof(item_t));
            memset(&e[r-1], 0, sizeof(item_t));
            m -= 1;
            e[r-1] = ins;
            return done;
        } else { // page is full then divide page
            pageref_t b = (pageref_t)SB_page::create(n);
            if (r < n) {
                memcpy(&modify(b)->e[n], e, r*sizeof(item_t));
                modify(b)->e[n+r] = ins;
                memcpy(&modify(b)->e[n+r+1], &e[r], (n-r-1)*sizeof(item_t));
            } else {
                memcpy(&modify(b)->e[n], e, n*sizeof(item_t));
                memmove(&e[n-1], &e[n], (r-n)*sizeof(item_t));
                memset(&e[r-1], 0, sizeof(item_t));
                e[r-1] = ins;
            }
            ins.key = b->e[2*n-1].key;
            ins.p = b;
            m = n-1;
            modify(b)->m = n;
            memset(e, 0, (n-1)*sizeof(item_t));
            return overflow;
        }
    }

    operation_effect remove(int r, item_t& rem, int4 n) {
        e[r].p = NULL; 
        if (e[r].key.is_max()) { 
            return done; 
        } else { 
            memmove(&e[m+1], &e[m], (r - m)*sizeof(item_t));
            memset(&e[m], 0, sizeof(item_t));
            rem.key = e[2*n-1].key; 
            return (++m > n) ? underflow : propagation; 
        }
    }

    operation_effect handle_underflow(int r, key_t const& key, item_t& rem, int4 n)
    {
        pageref_t a = (pageref_t)e[r].p;
        internal_assert(a->m == n+1);
        if (r < 2*n-1) { // exists greater page
            pageref_t b = (pageref_t)e[r+1].p;
            int bm = b->m; 
            if (bm < n) {  // reallocation of nodes between pages a and b
                int i = (n - bm + 1) >> 1;
                memmove(&modify(a)->e[n+1-i], &a->e[n+1], (n-1)*sizeof(item_t));
                memcpy(&modify(a)->e[2*n-i], &b->e[bm], i*sizeof(item_t));
                memset(&modify(b)->e[bm], 0, i*sizeof(item_t) );
                modify(b)->m += i;
                modify(a)->m -= i;
                e[r].key = a->e[2*n-1].key;
                return done;
            } else { // merge page a to b  
                internal_assert(bm == n); 
                memcpy(&modify(b)->e[1], &a->e[n+1], (n-1)*sizeof(item_t));
                memset(&modify(a)->e[n+1], 0, (n-1)*sizeof(item_t));
                e[r].p = NULL; 
                memmove(&e[m+1], &e[m], (r-m)*sizeof(item_t));
                memset(&e[m], 0, sizeof(item_t));
                modify(b)->m = 1;
                // dismiss page 'a'
                return ++m > n ? underflow : done;
            }
        } else { // page b is before a
            pageref_t b = (pageref_t)e[r-1].p;
            int bm = b->m; 
            if (key == e[r].key) { 
                e[r].key = rem.key;
            }
            if (bm < n) { // reallocation
                int i = (n - bm + 1) >> 1;
                memcpy(&modify(a)->e[n+1-i], &b->e[2*n-i], i*sizeof(item_t));
                memmove(&modify(b)->e[bm+i], &b->e[bm], (2*n-bm-i)*sizeof(item_t));
                memset(&modify(b)->e[bm], 0, i*sizeof(item_t));
                e[r-1].key = b->e[2*n-1].key;
                modify(b)->m += i;
                modify(a)->m -= i;
                return propagation;
            } else { // merge page b to a
                internal_assert(bm == n); 
                memcpy(&modify(a)->e[1], &b->e[n], n*sizeof(item_t));
                memset(&modify(b)->e[n], 0, n*sizeof(item_t));
                e[r-1].p = NULL; 
                memmove(&e[m+1], &e[m], (r-1-m)*sizeof(item_t));
                memset(&e[m], 0, sizeof(item_t));
                modify(a)->m = 1;
                // dismiss page 'b'
                return ++m > n ? underflow : propagation;
            }
        }
    }

    operation_effect find_insert_position(int level, 
                                          treeref_t const& tree, 
                                          item_t& ins, int4 n) const
    {
        int  i, l = m, r = 2*n-1;
        key_t const& key = ins.key;
        while (l < r)  {
            i = (l+r) >> 1;
            if (key > e[i].key) l = i+1; else r = i;
        }
        internal_assert(r == l && e[r].key >= key); 
        if (--level == 0)  {
            if (e[r].key == key) { 
                ref<set_member> mbr;
                
                for (mbr = (ref<set_member>)e[r].p; 
                     mbr != NULL && mbr->compare((ref<set_member>)ins.p) < 0; 
                     mbr = mbr->next);
                if (mbr == NULL) { 
                    modify(tree)->put_last((ref<set_member>)ins.p);
                } else { 
                    modify(tree)->put_before(mbr, (ref<set_member>)ins.p);
                    if (mbr == e[r].p) { 
                        modify(this)->e[r].p = ins.p;
                    }
                }
                return done;
            } else { 
                if (e[r].p == NULL) { 
                    modify(tree)->put_last((ref<set_member>)ins.p);
                } else { 
                    modify(tree)->put_before((ref<set_member>)e[r].p, (ref<set_member>)ins.p);
                }
            }               
        } else { 
            pageref_t child = (pageref_t)e[r].p;
            if (child->find_insert_position(level, tree, ins, n) == done) { 
                return done;
            }
        }
        // insert before e[r]
        return modify(this)->insert(r, ins, n); 
    }


    operation_effect find_remove_position(int level, item_t& rem, int4 n) const
    {
        int i, l = m, r = 2*n-1;
        key_t key = rem.key;
        while (l < r) {
            i = (l+r) >> 1;
            if (key > e[i].key) l = i+1; else r = i;
        }
        internal_assert(r == l && e[r].key >= key); 
        if (--level == 0) {
            assert(e[r].key == key);  
            ref<set_member> mbr = (ref<set_member>)rem.p;
            if (mbr == e[r].p) { 
                if (mbr->next != NULL) { 
                    key_t next_key;
                    next_key.extract(mbr->next);
                    if (next_key == key) { 
                        modify(this)->e[r].p = mbr->next;
                        return done; 
                    }
                }
                return modify(this)->remove(r, rem, n); 
            }
        } else { 
            pageref_t child = (pageref_t)e[r].p; 
            switch (child->find_remove_position(level, rem, n)) { 
              case underflow: 
                return modify(this)->handle_underflow(r, key, rem, n);
              case propagation: 
                if (key == e[r].key) { 
                    modify(this)->e[r].key = rem.key; 
                    return propagation;
                }
              default:;
            } 
        }
        return done; 
    }

	//[MC]
	void ok(key_t min_key_val, key_t max_key_val,
            int level, int height, int4 n) const {
		int i;
		assert(level == 0 || m <= n);

		if (++level == height) { // leaf page
			assert(e[m].key >= min_key_val);
			assert(e[2*n-1].key == max_key_val);

			for (i = m; i < 2*n-1; i++) { 
				ref<set_member> mbr = e[i].p;
				if (mbr.is_nil()) { 
					assert(i == 2*n-1 && e[2*n-1].key.is_max());
					break;
				}
				while (TRUE) {
					key_t mbr_key;
					mbr_key.extract(mbr);
					
					if(mbr_key != e[i].key)
						break;

					mbr = mbr->next; 
					if (mbr.is_nil()) { 
						assert(i >= 2*n-2 && e[2*n-1].key.is_max());
						break;
					}
					assert(mbr->prev->compare(mbr) <= 0);
				}
				assert((i == 2*n-1 && mbr.is_nil()) 
					   || (i < 2*n-1 && mbr == e[i+1].p));
				if (!mbr.is_nil()) { 
					key_t mbr_key;
					mbr_key.extract(mbr);

					assert(mbr_key == e[i+1].key);
					assert(e[i].key < e[i+1].key);
				}
			}
		} else { 
			for (i = m; i < 2*n; i++) { 
				ref<SB_page> p = e[i].p;
				assert(e[i].key > min_key_val 
					   || (i == 0 && e[i].key == min_key_val));
				p->ok(min_key_val, e[i].key, level, height, n);
				min_key_val = e[i].key;
			}
			assert(min_key_val == max_key_val);
		}
	}

	//[MC]
	bool check_integrity(int level, int4 n) const {
		bool rt = true;
		for(int i = m; i <= n*2-1; i++)
		{			
			if(level == 0)
			{
				if(e[i].key.is_max())
					continue;

				ref<set_member> const& fmbr = e[i].p;
				if(fmbr->owner.is_nil())
				{
					console::output("invalid member opid = %x key = %s page opid = %x\r\n", fmbr->get_handle()->opid, e[i].key.body, get_handle()->opid);
					rt = false;
				}
			}
			else
			{
				pageref_t const& page = (pageref_t)e[i].p; 
				if(!page->check_integrity(level-1, n))
					rt = false;
			}
		}
		
		return rt;
	}

    SB_page(size_t varying_length) : object(self_class, varying_length) {}

    // create new B tree root
    SB_page(pageref_t const& root, item_t& ins, int4 n) : object(self_class, 2*n)
    { 
        if (ins.key.is_max()) { 
            m = 2*n-1;
            e[2*n-1] = ins;
        } else {
            m = 2*n-2;
            e[2*n-2] = ins;
            e[2*n-1].key.set_max();
            e[2*n-1].p = root;
        }
    }

    METACLASS_DECLARATIONS(SB_page, object);

    static pageref_t create(int4 n) { 
        size_t len = n*2;
        SB_page<key_size>* p = new (self_class, len) SB_page<key_size>(len);
        return p;
    }

    static pageref_t create(ref<SB_page> const& root, item_t& ins, int4 n) { 
        size_t len = n*2;
        SB_page<key_size>* p = new (self_class, len) SB_page<key_size>(root, ins, n); 
        return p;
    }
}; 

template<size_t key_size>
field_descriptor& SB_page<key_size>::describe_components()
{
    return FIELD(m), VARYING(e); 
}


typedef SB_tree<16> SB_tree16; 
typedef SB_page<16> SB_page16;
typedef SB_tree<32> SB_tree32;
typedef SB_page<32> SB_page32;

#define BIT(i) ((nat8)1 << (i))

//
// Multidimensional index: KD-Tree 
//
class GOODS_DLL_EXPORT KD_tree : public object
{
  protected:
    /**
     * Compare  i-th component of two objects.
     * This method should be overriden in the derived class.
     * @param m1 first object
     * @param m2 second object
     * @param i dimension index
     * @return -1, 0, 1 depending on result of comparison
     */
    virtual int compare(anyref obj1, anyref obj2, int dimension) const = 0;
    
    /**                                                                 
     * Create multidimensional index with specified number of dimensions
     * @param n_dimensions number of dimensions
     */
    KD_tree(class_descriptor& desc, int n_dimensions) : object(desc), size(0), height(0), n_dims(n_dimensions)
    {
    }

  public:
    class Node : public object
    {
      public:
        ref<Node> left;
        ref<Node> right;
        ref<Node> parent;
        anyref    obj;
  
        Node() : object(self_class) {}
    
        METACLASS_DECLARATIONS(Node, object);
    };      

    /**
     * Removes all memebers from the index
     */
    void clear() 
    {
        root = NULL;
        size = 0;
        height = 0;
    }

    /**
     * Get tree height
     */
    int getHeight() const 
    { 
        return height;
    }

    /**
     * Get number of elements in the index
     */
    int getSize() const
    { 
        return size;
    }

    /**
     * Get number of dimensions
     */
    int getDimensions() const
    { 
        return n_dims;
    }

    /**
     * Add new element to the tree
     * @param obj object to be inserted in the index
     */
    void add(anyref obj);

    /**
     * Removes element from the tree
     * @param obj object to be removed in the index
     * @return true if object was successfully removed or false is object was not found in the index
     */
    bool remove(anyref obj);
    
    /**
     * KD_tree iterator
     */
    class Iterator 
    {
      public:
        /**
         * Advance iterator to the next object 
         * @return reference to the next object matching search criteria or nil if there are no more such objects
         */
        anyref next();

        /**
         * Just an alias to next method
         */
        anyref operator++() { 
            return next();
        }

        Iterator(ref<KD_tree> tree, anyref low, nat8 low_mask, anyref high, nat8 high_mask); 
              
      private:
        void getMin();

        ref<Node> curr_node;
        anyref low;
        anyref high;
        nat8   low_mask;
        nat8   high_mask;
        int    curr_level;
        ref<KD_tree> tree;
        boolean has_current;
    };
        
    /**
     * Locate all objects which components matches with correspodent componens of pattern object.
     * @param pattern object used as query example
     * @param mask mask of specified fields of pattern object which should be considered during search: first field corresponds to the first bit (1), second - to second (1 << 1), 
     * and so on...
     * @return  iterator through the object matching search criteria. Please notice that you ensure that index is not modified during iteration.
     * Iterator's method itslef are not able to provide proper synchronization, so you should either ensure that all access to the KD_Tree index is perfromed through 
     * methods of some other object, either use r_ref, w_ref classes o hold lock on the object during iteration, either path callback to  iterate method of KD_tree.
     */
    Iterator queryByExample(anyref pattern, nat8 mask) const { 
        return queryByExample(pattern, mask, pattern, mask);
    }

    /**
     * Locate all objects which components values belongs to the range between low and high pattern objects;
     * @param low object used to specify low boundary
     * @param low_mask mask of specified fields of low boundary pattern object. If correspondent bit of the mask is not set, then there is no restriction
     * for the minimal value of this component (open interval)
     * @param high object used to specify high boundary
     * @param high_mask mask of specified fields of high boundary pattern object. If correspondent bit of the mask is not set, then there is no restriction
     * for the maxinmal value of this component (open interval)
     * @return  iterator through the object matching search criteria. Please notice that you ensure that index is not modified during iteration.
     * Iterator's method itslef are not able to provide proper synchronization, so you should either ensure that all access to the KD_Tree index is perfromed through 
     * methods of some other object, either use r_ref, w_ref classes o hold lock on the object during iteration, either path callback to  iterate method of KD_tree.
     */
    Iterator queryByExample(anyref low, nat8 low_mask, anyref high, nat8 high_mask) const
    {
        return Iterator(this, low, low_mask, high, high_mask);
    }

    /**
     * Iterate through all object matching search criteria.
     * @param low object used to specify low boundary
     * @param low_mask mask of specified fields of low boundary pattern object. If correspondent bit of the mask is not set, then there is no restriction
     * for the minimal value of this component (open interval)
     * @param high object used to specify high boundary
     * @param high_mask mask of specified fields of high boundary pattern object. If correspondent bit of the mask is not set, then there is no restriction
     * for the maxinmal value of this component (open interval)
     * @param callback function. Callback function should return false to stop iteration.
     * @param arg argument passed to clback function with arbitrary user context
     * @return number of iterated objects 
     */
    nat8 iterate(anyref low, nat8 low_mask, anyref high, nat8 high_mask,
                 boolean (*func)(const anyref &mbr, void const* arg), void const* arg = NULL) const;
            
    
    METACLASS_DECLARATIONS(KD_tree, object);


  private:
    bool lessOrEquals(anyref obj1, anyref obj2, nat8 mask) const;
    bool greaterOrEquals(anyref obj1, anyref obj2, nat8 mask) const;

    ref<Node> root;
    nat8      size;
    int4      height;
    int4      n_dims;
    ref<Node> free_chain;
};

   
class GOODS_DLL_EXPORT hash_item : public object { 
  public: 
    ref<hash_item> next;
    anyref         obj;
    char           name[1];

    //
    // Call function for object. Apply method returns value
    // returned by function. Iteration through all hash table 
    // elements will finished if True value is return by apply method.
    //
    typedef boolean (*item_function)(const char* name, anyref obj);
    boolean apply(item_function f) const { return f(name, obj); }

    hash_item(anyref obj, ref<hash_item> next, 
              const char* name, size_t name_len) 
    : object(self_class, name_len)
    {
        this->obj = obj;
        this->next = next;
        strcpy(this->name, name);
    }
#ifdef USE_LOCALE_SETTINGS
     int compare(const char* name) const { return strcoll(name, this->name); }
#else
     int compare(const char* name) const { return strcmp(name, this->name); }
#endif
    static ref<hash_item> create(anyref obj, ref<hash_item> chain,
                                 const char* name)
    {
        size_t name_len = strlen(name)+1;
        return new (self_class,name_len) hash_item(obj, chain, name, name_len);
    }
    METACLASS_DECLARATIONS(hash_item, object);
};

//
// Common base for hash_table and H_tree
//
class GOODS_DLL_EXPORT dictionary : public object {
public:
	//
	// Add to hash table association of object with specified name.
	//
	virtual void        put(const char* name, anyref obj) = 0;
	//
	// Search for object with specified name in hash table.
	// If object is not found NULL is returned.
	//
	virtual anyref      get(const char* name) const = 0;
	//
	// Remove object with specified name from hash table.
	// If there are several objects with the same name the one last inserted
	// is removed. If such name was not found 'False' is returned.
	//
	virtual boolean     del(const char* name) = 0;
	//
	// Remove concrete object with specified name from hash table.
	// If such name was not found or it is associated with different object
	// 'False' is returned. If there are several objects with the same name, 
	// all of them are compared with specified object.
	//    
	virtual boolean     del(const char* name, anyref obj) = 0;

	//
	// Apply function to objects placed in hash table.
	// Function will be applied in some unspecified order
	// to all elements in hash table or until True will be returned by 
	// the function. In the last case reference to this object is returned.
	// Otherwise (if function returns False for all elements in hash table)
	// NULL is returned.
	//
	virtual anyref apply(hash_item::item_function) const = 0;

	//
	// Clear dictionary
	//
	virtual void reset() = 0;

	//
	// Get number of elements in dictionary
	//
	virtual size_t get_number_of_elements() const = 0;

	METACLASS_DECLARATIONS(dictionary, object);

	dictionary(class_descriptor& desc, size_t varying_length = 0) : object(desc, varying_length) {}
};

//   
// Simple hash table.
//
class GOODS_DLL_EXPORT hash_table : public dictionary {
  public: 
    //
    // Add to hash table association of object with specified name.
    //
    void        put(const char* name, anyref obj);
    //
    // Search for object with specified name in hash table.
    // If object is not found NULL is returned.
    //
    anyref      get(const char* name) const;
    //
    // Remove object with specified name from hash table.
    // If there are several objects with the same name the one last inserted
    // is removed. If such name was not found 'False' is returned.
    //
    boolean     del(const char* name);
    //
    // Remove concrete object with specified name from hash table.
    // If such name was not found or it is associated with different object
    // 'False' is returned. If there are several objects with the same name, 
    // all of them are compared with specified object.
    //    
    boolean     del(const char* name, anyref obj);

    //
    // Apply function to objects placed in hash table.
    // Function will be applied in some unspecified order
    // to all elements in hash table or until True will be returned by 
    // the function. In the last case reference to this object is returned.
    // Otherwise (if function returns False for all elements in hash table)
    // NULL is returned.
    //
    anyref apply(hash_item::item_function) const;

    void reset() { 
        n_elems = 0;
        memset(table, 0, size*sizeof(anyref)); 
    }

    size_t get_number_of_elements() const { return n_elems; }

	static ref<hash_table> create(obj_storage* storage, size_t size)
	{
        return new (self_class, size) hash_table(size);
    }
	METACLASS_DECLARATIONS(hash_table, dictionary);

  protected: 
    nat4   size;
    nat4   n_elems;
    anyref table[1];
    
    hash_table(size_t size, class_descriptor& desc = self_class) 
		: dictionary(desc, size)
    { 
        this->size = (nat4)size;
        n_elems = 0;
    }
};

//
// Hash tree is combination of hash table and B-tree. Calculated 
// values of hash function are placed at B*-tree pages.
//
class GOODS_DLL_EXPORT H_page : public object { // page of hash tree
  public:
    enum { size_log=7 };
    anyref p[1 << size_log];

    anyref apply(hash_item::item_function f, int height) const;
    void reset(int height);
    H_page() : object(self_class) {}

    METACLASS_DECLARATIONS(H_page, object);
};

class GOODS_DLL_EXPORT H_tree : public dictionary {
  public: 
    //
    // Add to hash table association of object with specified name.
    //
    void        put(const char* name, anyref obj);
    //
    // Search for object with specified name in hash table.
    // If object is not found NULL is returned.
    //
    anyref      get(const char* name) const;
    //
    // Remove object with specified name from hash table.
    // If there are several objects with the same name the one last inserted
    // is removed. If such name was not found 'False' is returned.
    //
    boolean     del(const char* name);
    //
    // Remove concrete object with specified name from hash table.
    // If such name was not found or it is associated with different object
    // 'False' is returned. If there are several objects with the same name, 
    // all of them are compared with specified object.
    //    
    boolean     del(const char* name, anyref obj);

    //
    // Apply function to objects placed in hash table.
    // Function will be applied in some unspecified order
    // to all elements in hash table or until True will be returned by 
    // the function. In the last case reference to this object is returned.
    // Otherwise (if function returns False for all elements in hash table)
    // NULL is returned.
    //
    anyref apply(hash_item::item_function f) const { 
        if (!root.is_nil()) {
            return root->apply(f, height);
        }
        return NULL;
    }
    //
    // Clear hash table
    //
    void reset() { 
        if (!root.is_nil()) { 
            modify(root)->reset(height);
        }
        n_elems = 0;
    }
    size_t get_number_of_elements() const { return n_elems; }

	METACLASS_DECLARATIONS(H_tree, dictionary);
    H_tree(size_t hash_size, class_descriptor& desc = self_class);

  protected: 
    nat4        size;
    nat4        height;
    nat4        n_elems;
    ref<H_page> root;
};

//
// Binary large object class. This class can be used as base for
// creating multimedia or text objects with large size and sequential 
// access. This class provide functionality for incremental object
// loading by parallel task. 
//

class GOODS_DLL_EXPORT Blob : public object { 
  protected: 
    ref<Blob> next;
    ref<Blob> last;
    nat1      data[1];
    
    static void task_proc load_next_blob_part(void* arg);

  public: 
    //
    // This method is called by 'play' method for each part of blob
    // object within conext of current task. If this method returns False 
    // preloading process is terminated. 
    //
    virtual boolean handle(void* arg) const;
    //
    // Default implementation of this method just extracts all parts of 
    // blob from database and calls handle method for each part.
    //
    virtual void play(void* arg) const;

    //
    // Append new blob object at the end of blob objects chain.
    //
    void append(ref<Blob> bp) { 
        modify(last)->next = bp; 
        last = bp;
    }

    //
    // Size of BLOB segment
    //
    size_t get_size() const { 
        return  size - this_offsetof(Blob, data);
    }   
	const nat1* get_data() const {
		return data;
	}

    //
    // How much segments of BLOB object can be read ahead before processing.
    // Default value is 1.
    //
    static int readahead;

    METACLASS_DECLARATIONS(Blob, object);

    Blob(size_t part_size, class_descriptor& desc = self_class) 
    : object(desc, part_size) 
    { 
        next = NULL;
        last = this;
    }

    Blob(void* buf, size_t buf_size, class_descriptor& desc = self_class) 
    : object(desc, buf_size) 
    { 
        next = NULL;
        last = this;
        memcpy(data, buf, buf_size);
    }
};

class GOODS_DLL_EXPORT ExternalBlob : public object 
{ 
  private:
    nat1 data[1];

  public:
    METACLASS_DECLARATIONS(ExternalBlob, object);
    
    size_t get_body(char* dst_buf, size_t offs, size_t buf_size) const {
        size_t blob_size = get_size();
        if (offs > blob_size) {
            offs = blob_size;
        } 
        size_t rest = blob_size - offs;
        size_t n = buf_size < rest ? buf_size : rest;
        memcpy(dst_buf, data+offs, n);
        return rest;
    } 

	char const* get_body() const {
		return (char const*)data;
	}
	
    size_t get_size() const { 
        return  size - this_offsetof(ExternalBlob, data);
    }   

	ExternalBlob(void const* buf, size_t buf_size) : object(self_class, buf_size)
	{
		memcpy(data, buf, buf_size);
	}

	static ref<ExternalBlob> create(void const* buf, size_t buf_size) {
		return new (self_class, buf_size) ExternalBlob(buf, buf_size);
	}
};


END_GOODS_NAMESPACE

#endif
