// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< REFS.H >--------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     23-Feb-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 25-Sep-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Smart object references templates
//-------------------------------------------------------------------*--------*

#ifndef __REFS_H__
#define __REFS_H__

#include "goodsdlx.h"
#include "object.h"

BEGIN_GOODS_NAMESPACE

#ifndef PROHIBIT_UNSAFE_IMPLICIT_CASTS
#define PROHIBIT_UNSAFE_IMPLICIT_CASTS 1
//[MC] We do not accept convertible assignments
#define ENABLE_CONVERTIBLE_ASSIGNMENTS 0
#endif



#ifndef ENABLE_CONVERTIBLE_ASSIGNMENTS
#define ENABLE_CONVERTIBLE_ASSIGNMENTS 1
#endif


#if ENABLE_CONVERTIBLE_ASSIGNMENTS
template< class Y, class T > struct sp_convertible
{
    typedef char (&yes) [1];
    typedef char (&no)  [2];

    static yes f( T* );
    static no  f( ... );

    enum _vt { value = sizeof( (f)( static_cast<Y*>(0) ) ) == sizeof(yes) };
};

struct sp_empty
{
};

template< bool > struct sp_enable_if_convertible_impl;

template<> struct sp_enable_if_convertible_impl<true>
{
    typedef sp_empty type;
};

template<> struct sp_enable_if_convertible_impl<false>
{
};

template< class Y, class T > struct sp_enable_if_convertible: public sp_enable_if_convertible_impl< sp_convertible< Y, T >::value >
{
};
#endif

class /*GOODS_DLL_EXPORT*/ object_ref { 
    friend class database;
  public: 
    hnd_t get_handle() const { return hnd; }

  protected:
    hnd_t hnd;
    
    void unlink() { 
        object_handle::remove_reference(hnd); 
    }
    void link() { 
        if (hnd != 0) { 
            hnd->access += 1;
            if (!IS_VALID_OBJECT(hnd->obj)) { 
                internal_assert(hnd->storage != NULL); 
                hnd->storage->load(hnd, lof_auto);
                internal_assert(IS_VALID_OBJECT(hnd->obj));
            }
        }
    }   
    object_ref(hnd_t hnd) { 
        object_monitor::lock_global();
        this->hnd = hnd; 
        link();
    }
    object_ref() { 
        hnd = 0; 
    }
    ~object_ref() { 
        unlink(); 
        object_monitor::unlock_global();
    }
};


template<class T>
class /*GOODS_DLL_EXPORT*/ read_access : public object_ref { 
  private: 
    object* obj; 
  public:
    T const* operator ->() { 
        return (T const*)obj; 
    }
    read_access(hnd_t hnd) : object_ref(hnd) {
        hnd->obj->mop->begin_read(hnd, False); 
        // object can be reloaded by begin_read
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }
    read_access(read_access const& ra) : object_ref(ra.hnd) {
        obj = ra.obj;
        obj->mop->begin_read(hnd, False); 
        object_monitor::unlock_global(); 
    }
    ~read_access() { 
        object_monitor::lock_global();
        // object can be changed by become operator
        hnd->obj->mop->end_read(hnd); 
    } 
};

template<class T>
class /*GOODS_DLL_EXPORT*/ read_for_update_access : public object_ref { 
  private: 
    object* obj; 
  public:
    T const* operator ->() { 
        return (T const*)obj; 
    }
    read_for_update_access(hnd_t hnd) : object_ref(hnd) {
        hnd->obj->mop->begin_read(hnd, True); 
        // object can be reloaded by begin_read
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }
    read_for_update_access(read_for_update_access const& ra) : object_ref(ra.hnd) {
        obj = ra.obj;
        obj->mop->begin_read(hnd, True); 
        object_monitor::unlock_global(); 
    }
    ~read_for_update_access() { 
        object_monitor::lock_global();
        // object can be changed by become operator
        hnd->obj->mop->end_read(hnd); 
    } 
};

template<class T>
class /*GOODS_DLL_EXPORT*/ write_transient_access : public object_ref { 
  private: 
    object* obj; 
  public:
    T* operator ->() { 
        return (T*)obj; 
    }
    write_transient_access(hnd_t hnd) : object_ref(hnd) {
        hnd->obj->mop->begin_read(hnd, False); 
        // object can be reloaded by begin_read
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }
    write_transient_access(write_transient_access const& ra) : object_ref(ra.hnd) {
        obj = ra.obj;
        obj->mop->begin_read(hnd, False); 
        object_monitor::unlock_global(); 
    }
    ~write_transient_access() { 
        object_monitor::lock_global();
        // object can be changed by become operator
        hnd->obj->mop->end_write_transient(hnd); 
    } 
};


template<class T>
class /*GOODS_DLL_EXPORT*/ write_access : public object_ref { 
  private: 
    object* obj; 
  public:
    T* operator ->() { 
        return (T*)obj; 
    }
    write_access(hnd_t hnd) : object_ref(hnd) {
        assert(hnd != 0);
        hnd->obj->mop->begin_write(hnd); 
        // object can be reloaded by begin_write
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }
    write_access(write_access const& wa) : object_ref(wa.hnd) {
        obj = wa.obj;
        obj->mop->begin_write(hnd); 
        object_monitor::unlock_global(); 
    }
    ~write_access() { 
        object_monitor::lock_global();
        // object can be changed by become operator
        hnd->obj->mop->end_write(hnd); 
    } 
};

#if GOODS_RUNTIME_TYPE_CHECKING
template<class T>
extern class_descriptor& classof(T const*);
#endif

//[MC] added to check for uncleared in-memory lists
#ifdef CHECK_FOR_MEMORY_LEAKS
void check_set_owner_memory_leaks(hnd_t hnd);
#endif // CHECK_FOR_MEMORY_LEAKS

class anyref;

template<class T>
class /*GOODS_DLL_EXPORT*/ ref : public object_ref { 
  public:
    inline read_access<T> operator->() const { 
        assert(hnd != 0);
        return read_access<T>(hnd);
    }
	//[MC] changed operator return type to bool from boolean to eliminate conversion from int to bool when used in STL containers
    inline bool operator==(object_ref const& r) const { 
        return hnd == r.get_handle();
    }
	//[MC] changed operator return type to bool from boolean to eliminate conversion from int to bool when used in STL containers
    inline bool operator!=(object_ref const& r) const { 
        return hnd != r.get_handle();
    }
	//[MC] changed operator return type to bool from boolean to eliminate conversion from int to bool when used in STL containers
    inline bool operator==(T const* p) const { 
        return p ? (hnd == ((object*)p)->get_handle()) : (hnd == 0);
    }
	//[MC] changed operator return type to bool from boolean to eliminate conversion from int to bool when used in STL containers
    inline bool operator!=(T const* p) const { 
        return p ? (hnd != ((object*)p)->get_handle()) : (hnd != 0);
    }
	//[MC] changed operator return type to bool from boolean to eliminate conversion from int to bool when used in STL containers
    // for std::map or std::set :
    inline bool operator<( object_ref const& r) const {
      return hnd < r.get_handle();
    }

    inline boolean is_nil() const { return hnd == 0; }

    inline ref& operator=(ref const& r) { 
        object_monitor::lock_global(); 
        if (r.hnd != hnd) { 
            unlink();
            hnd = r.hnd;
            link();
        }
        object_monitor::unlock_global(); 
        return *this;
    }
    
    inline ref& operator=(object_ref const& r) { 
        object_monitor::lock_global(); 
        if (r.get_handle() != hnd) { 
            unlink();
            hnd = r.get_handle();
            link();
#if GOODS_RUNTIME_TYPE_CHECKING
            assert(hnd == 0 || classof((T const*)0).is_superclass_for(hnd)); 
#elif USE_DYNAMIC_TYPECAST
            if (hnd != NULL) { 
                dynamic_cast<T&>(*hnd->obj);
            }
#endif
        }
        object_monitor::unlock_global(); 
        return *this;
    }
 
    inline ref& operator=(T const* p) { 
        object_monitor::lock_global(); 
        unlink();
        if (p == NULL) { 
            hnd = 0;
        } else { 
            hnd = ((object*)p)->get_handle();
            link();
        }
        object_monitor::unlock_global(); 
        return *this;
    }

    ref(ref const& r) : object_ref(r.hnd) { 
        object_monitor::unlock_global(); 
    }

#if ENABLE_CONVERTIBLE_ASSIGNMENTS
    template<class Y>
    ref(ref<Y> const& r, typename sp_enable_if_convertible<Y,T>::type = sp_empty()) : object_ref(r.get_handle()) { 
        object_monitor::unlock_global(); 
    }
#endif

    operator anyref();

    ref(anyref const& r) : object_ref(((object_ref*)&r)->get_handle()) { 
#if GOODS_RUNTIME_TYPE_CHECKING 
#if GNUC_BEFORE(2,96)
        extern class_descriptor& classof(T const*); 
#endif
        assert(hnd == 0 || classof((T const*)0).is_superclass_for(hnd)); 
#elif USE_DYNAMIC_TYPECAST
		if (hnd != NULL) {
			assert(dynamic_cast<T*>(hnd->obj) != NULL);
        }
#endif
        object_monitor::unlock_global(); 
    }
        
#if PROHIBIT_UNSAFE_IMPLICIT_CASTS
    explicit 
#endif
    ref(object_ref const& r) : object_ref(r.get_handle()) { 
#if GOODS_RUNTIME_TYPE_CHECKING 
#if GNUC_BEFORE(2,96)
        extern class_descriptor& classof(T const*); 
#endif
        assert(hnd == 0 || classof((T const*)0).is_superclass_for(hnd)); 
#elif USE_DYNAMIC_TYPECAST
		if (hnd != NULL) {
			assert(dynamic_cast<T*>(hnd->obj) != NULL);
        }
#endif
        object_monitor::unlock_global(); 
    }

    ref(T const* p) { 
        if (p != NULL) { 
            object_monitor::lock_global(); 
            hnd = ((object*)p)->get_handle();
            link();
            object_monitor::unlock_global(); 
        }
    }
    
    ref() {}

    ~ref() 
	{
		//[MC] added to check for uncleared in-memory lists
#ifdef CHECK_FOR_MEMORY_LEAKS
		check_set_owner_memory_leaks(hnd);
#endif // CHECK_FOR_MEMORY_LEAKS

		object_monitor::lock_global(); 
	}
};

class anyref : public ref<object> { 
  public:
    anyref(object_ref const& r) : ref<object>(r) {}
    anyref(object const* p) : ref<object>(p) {}
    anyref() {}
};

template<class T>
inline ref<T>::operator anyref() {
    return *(anyref*)this;
}

template<class T>
inline write_access<T> modify(ref<T> const& r) { 
    return write_access<T>(r.get_handle()); 
}  

template<class T>
inline write_access<T> modify(T const* p) { 
    assert(p != NULL);
    return write_access<T>(((object*)p)->get_handle()); 
}  

template<class T>
inline read_for_update_access<T> update(ref<T> const& r) { 
    return read_for_update_access<T>(r.get_handle()); 
}  

template<class T>
inline read_for_update_access<T> update(T const* p) { 
    assert(p != NULL);
    return read_for_update_access<T>(((object*)p)->get_handle()); 
} 
 
template<class T>
inline write_transient_access<T> cast(ref<T> const& r) { 
    return write_transient_access<T>(r.get_handle()); 
}  

template<class T>
inline write_transient_access<T> cast(T const* p) { 
    assert(p != NULL);
    return write_transient_access<T>(((object*)p)->get_handle()); 
}  


// read only reference to pinned object
template<class T>
class /*GOODS_DLL_EXPORT*/ r_ref : public object_ref { 
  private: 
    object* obj; 
  public:
    T const* operator ->() const { 
        return (T const*)obj; 
    }
    T const* get() const {
        return (T*)obj; 
    }
	T const& operator *() const {
		return *((T*)obj);
	}
    
	r_ref() {}

	//[MC] allow conversion from other refs
	r_ref(object_ref const& r) : object_ref(r.get_handle()) {
#if GOODS_RUNTIME_TYPE_CHECKING 
#if GNUC_BEFORE(2,96)
        extern class_descriptor& classof(T const*); 
#endif
        assert(hnd == 0 || classof((T const*)0).is_superclass_for(hnd)); 
#elif USE_DYNAMIC_TYPECAST
        if (hnd != NULL) { 
            dynamic_cast<T&>(*hnd->obj);
        }
#endif
        hnd->obj->mop->begin_read(hnd, False); 
        // object can be reloaded by begin_read
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }

    r_ref(ref<T> const& r) : object_ref(r.get_handle()) {
        hnd->obj->mop->begin_read(hnd, False); 
        // object can be reloaded by begin_read
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }
    
    r_ref(anyref const& r) : object_ref(r.get_handle()) {
        hnd->obj->mop->begin_read(hnd, False); 
        // object can be reloaded by begin_read
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }

    r_ref(r_ref<T> const& r) : object_ref(r.get_handle()) {
        hnd->obj->mop->begin_read(hnd, False); 
        // object can be reloaded by begin_read
        obj = hnd->obj;
        object_monitor::unlock_global();
    }

    inline boolean operator==(object_ref const& r) const { 
        return hnd == r.get_handle();
    }

    inline boolean operator!=(object_ref const& r) const { 
        return hnd != r.get_handle();
    }

    inline r_ref& operator=(ref<T> const& r) { 
#if GOODS_RUNTIME_TYPE_CHECKING && GNUC_BEFORE(2,96)
        extern class_descriptor& classof(T const*); 
#endif
        object_monitor::lock_global(); 
        if (r.get_handle() != hnd) { 
            if (hnd != NULL) { 
                hnd->obj->mop->end_read(hnd); 
            }
            unlink();
            hnd = r.get_handle();
            link();
#if GOODS_RUNTIME_TYPE_CHECKING
            assert(hnd == 0 || classof((T const*)0).is_superclass_for(hnd)); 
#elif USE_DYNAMIC_TYPECAST
            if (hnd != NULL) { 
                dynamic_cast<T&>(*hnd->obj);
            }
#endif
            if (hnd != NULL) { 
                hnd->obj->mop->begin_read(hnd, False); 
                // object can be reloaded by begin_write
                obj = hnd->obj;
            } else { 
                obj = NULL;
            }
        }
        object_monitor::unlock_global(); 
        return *this;
    }

    ~r_ref() { 
        object_monitor::lock_global();
        if (hnd != NULL) { 
            // object can be changed by become operator
            hnd->obj->mop->end_read(hnd); 
        }
    } 
};

// read-write reference to pinned object
template<class T>
class /*GOODS_DLL_EXPORT*/ w_ref : public object_ref { 
  private: 
    object* obj; 
  public:
    T* operator ->() { 
        return (T*)obj; 
    }
    T* get() {
        return (T*)obj; 
    }
    T& operator *() {
        return *((T*)obj); 
    }

    w_ref() {}

    w_ref(ref<T> const& r) : object_ref(r.get_handle()) {
        hnd->obj->mop->begin_write(hnd); 
        // object can be reloaded by begin_write
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }

    w_ref(anyref const& r) : object_ref(r.get_handle()) {
        hnd->obj->mop->begin_write(hnd); 
        // object can be reloaded by begin_write
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }

    w_ref(w_ref<T> const& r) : object_ref(r.get_handle()) {
        hnd->obj->mop->begin_write(hnd); 
        // object can be reloaded by begin_write
        obj = hnd->obj;
        object_monitor::unlock_global(); 
    }

    inline boolean operator==(object_ref const& r) const { 
        return hnd == r.get_handle();
    }

    inline boolean operator!=(object_ref const& r) const { 
        return hnd != r.get_handle();
    }

    inline w_ref& operator=(ref<T> const& r) { 
#if GOODS_RUNTIME_TYPE_CHECKING && GNUC_BEFORE(2,96)
        extern class_descriptor& classof(T const*); 
#endif
        object_monitor::lock_global(); 
        if (r.get_handle() != hnd) { 
            if (hnd != NULL) { 
                hnd->obj->mop->end_write(hnd); 
            }
            unlink();
            hnd = r.get_handle();
            link();
#if GOODS_RUNTIME_TYPE_CHECKING
            assert(hnd == 0 || classof((T const*)0).is_superclass_for(hnd)); 
#elif USE_DYNAMIC_TYPECAST
            if (hnd != NULL) { 
                dynamic_cast<T&>(*hnd->obj);
            }
#endif
            if (hnd != NULL) { 
                hnd->obj->mop->begin_write(hnd); 
                // object can be reloaded by begin_write
                obj = hnd->obj;
            } else { 
                obj = NULL;
            }
        }
        object_monitor::unlock_global(); 
        return *this;
    }

    ~w_ref() { 
        object_monitor::lock_global();
        if (hnd != NULL) { 
            // object can be changed by become operator
            hnd->obj->mop->end_write(hnd); 
        }
    } 
};


END_GOODS_NAMESPACE

#endif
