// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< RTREE.H >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     14-Jan-98    K.A. Knizhnik  * / [] \ *
//                          Last update: 14-Jan-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// R tree class definition
//-------------------------------------------------------------------*--------*

#ifndef __RTREE_H__
#define __RTREE_H__

#include "goods.h"
#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

const double SPLITRATIO = 0.55;

class GOODS_DLL_EXPORT rectangle
{
  public:
    enum { dim = 2 };
    int4 boundary[dim*2];
    friend size_t area(rectangle const& r) { 
        size_t area = 1;
        for (int i = dim; --i >= 0; area *= r.boundary[i+dim] - r.boundary[i]);
        return area;
    }
    void operator +=(rectangle const& r) { 
        int i = dim; 
        while (--i >= 0) { 
            boundary[i] = (boundary[i] <= r.boundary[i]) 
                ? boundary[i] : r.boundary[i];
            boundary[i+dim] = (boundary[i+dim] >= r.boundary[i+dim]) 
                ? boundary[i+dim] : r.boundary[i+dim];
        }
    }
    rectangle operator + (rectangle const& r) const { 
        rectangle res;
        int i = dim; 
        while (--i >= 0) { 
            res.boundary[i] = (boundary[i] <= r.boundary[i]) 
                ? boundary[i] : r.boundary[i];
            res.boundary[i+dim] = (boundary[i+dim] >= r.boundary[i+dim]) 
                ? boundary[i+dim] : r.boundary[i+dim];
        }
        return res;
    }
    boolean operator& (rectangle const& r) const {
        int i = dim; 
        while (--i >= 0) { 
            if (boundary[i] > r.boundary[i+dim] ||
                r.boundary[i] > boundary[i+dim])
            {
                return False;
            }
        }
        return True;
    }
    boolean operator <= (rectangle const& r) const { 
        int i = dim; 
        while (--i >= 0) { 
            if (boundary[i] < r.boundary[i] ||
                boundary[i+dim] > r.boundary[i+dim])
            {
                return False;
            }
        }
        return True;
    }
    boolean contains(rectangle const& r) const { 
        return r <= *this;
    }

    void split(rectangle& r0, rectangle& r1) const {
        double range;
        r0 = r1 = *this;          
        // Split in the widest direction
        if ((boundary[dim] - boundary[0]) > (boundary[dim+1] - boundary[1])) {
            // X direction
            range = boundary[dim] - boundary[0];
            
            r0.boundary[dim] = boundary[0] + (int4) (range * SPLITRATIO +.5);
            r1.boundary[0] = boundary[dim] - (int4) (range * SPLITRATIO +.5);
        } else {
            // Y direction
            range = boundary[dim+1] - boundary[1];
            
            r0.boundary[dim+1] = boundary[1] + (int4) (range * SPLITRATIO +.5);
            r1.boundary[1] = boundary[dim+1] - (int4) (range * SPLITRATIO +.5);
        }
    }

    field_descriptor& describe_components() { return ARRAY(boundary); }

    friend field_descriptor& describe_field(rectangle& r);

};

inline field_descriptor& describe_field(rectangle& r) { 
    return r.describe_components();
}

class GOODS_DLL_EXPORT callback { 
  public:
    virtual void apply(anyref obj) = 0;

    virtual ~callback() {};
};

class GOODS_DLL_EXPORT search_buffer { 
  public:
    struct item { 
        item*  next;
        anyref obj;
        
        item(anyref o, item* chain) : next(chain), obj(o) {} 

        void* operator new(size_t, dnm_object_pool& pool) { 
            return pool.alloc();
        }
    };
    
    void put(anyref obj) {
        curr_item = first_item = new (pool) item(obj, first_item);
        n_items += 1;
    }
    void clean() { 
        item* ip = first_item; 
        while (ip != NULL) { 
            item* next = ip->next;
            ip->obj = NULL;
            pool.dealloc(ip);
            ip = next;
        }
        first_item = curr_item = NULL;
        n_items = 0;
    }
    void reset() { 
        curr_item = first_item; 
    }
    boolean is_empty() const { return curr_item == NULL; }

    anyref get() { 
        if (curr_item != NULL) { 
            anyref obj = curr_item->obj;
            curr_item = curr_item->next;
            return obj;
        }
        return NULL;
    }
    size_t get_number_of_items() { return n_items; }

    ~search_buffer() { clean(); }

    search_buffer() : pool(sizeof(item)) { 
        first_item = curr_item = NULL;
        n_items = 0;
    }
  protected:
    item*  first_item;
    item*  curr_item;
    size_t n_items;
    dnm_object_pool pool;
};      


class GOODS_DLL_EXPORT R_page : public object { 
  public:
    enum { 
        card = (4096-4)/(6+4*4), // maximal number of branches at page
        min_fill = card/2        // minimal number of branches at non-root page
    };

    struct branch { 
        rectangle   rect;
        anyref p;

        field_descriptor& describe_components() { 
            return FIELD(rect), FIELD(p);
        }
    };
    friend field_descriptor& describe_field(branch& b);

    struct reinsert_list { 
        ref<R_page> chain;
        int         level;
        reinsert_list() { chain = NULL; }
    };

    int search(rectangle const& r, callback& cb, int level) const;
    int search(rectangle const& r, search_buffer& sbuf, int level) const;

    ref<R_page> insert(rectangle const& r, anyref obj, int level) const;

    boolean     remove(rectangle const& r, anyref obj, int level,
                       reinsert_list& rlist) const;

    rectangle   cover() const;

    ref<R_page> split_page(branch const& br);

    ref<R_page> add_branch(branch const& br) { 
        if (n < card) { 
            b[n++] = br;
            return NULL;
        } else { 
            return split_page(br);
        }
    }
    void remove_branch(int i);

    ref<R_page> next_reinsert_page() const { return (ref<R_page>)b[card-1].p; }

    R_page(rectangle const& rect, anyref obj);
    R_page(ref<R_page> old_root, ref<R_page> new_page);

    int4   n; // number of branches at page
    branch b[card];

    METACLASS_DECLARATIONS(R_page, object);
};

inline field_descriptor& describe_field(R_page::branch& b) { 
    return b.describe_components();
}


class GOODS_DLL_EXPORT R_tree : public object { 
  public: 
    int     search(rectangle const& r, callback& cb) const;     
    int     search(rectangle const& r, search_buffer& sbuf) const;       
    void    insert(rectangle const& r, anyref obj);
    boolean remove(rectangle const& r, anyref obj);

    void prune() { 
        root = NULL; 
        n_records = 0; 
        height = 0; 
    }

    METACLASS_DECLARATIONS(R_tree, object);
    R_tree(class_descriptor& desc = self_class) : object(desc) {
        prune();
    }
  protected:
    nat4        n_records;
    nat4        height;
    ref<R_page> root;
};

END_GOODS_NAMESPACE

#endif



