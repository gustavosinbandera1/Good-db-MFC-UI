// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< QTREE.H >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     14-Jan-98    K.A. Knizhnik  * / [] \ *
//                          Last update: 14-Jan-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Quad tree class definition
//-------------------------------------------------------------------*--------*

#ifndef __QTREE_H__
#define __QTREE_H__

#include "goods.h"
#include "goodsdlx.h"
#include "rtree.h"

BEGIN_GOODS_NAMESPACE

class GOODS_DLL_EXPORT Q_page : public object { 
  public:
    enum { 
        card = (4096-4-4*4-6)/(6+4*4) // maximal number of branches at page
    };

    struct branch { 
        rectangle   rect;
        anyref p;

        field_descriptor& describe_components() { 
            return FIELD(rect), FIELD(p);
        }
    };
    friend field_descriptor& describe_field(branch& b);
    
    int search(rectangle const& r, callback& cb) const;
    int search(rectangle const& r, search_buffer& sbuf) const;

    void insert(rectangle const& r, anyref obj)
    {
        branch br;
        br.p = obj;
        br.rect = r;
        
        if (n < card) { 
            b[n++] = br;
            if (n)
                rect += r;      // rect contains the overlapping rectangle of all objects
            else
                rect = b[0].rect; // initialize
            
        } else { 
            
            if (next.is_nil())
            {
                modify(this)->next =  NEW Q_page();
            }
            modify(next)->insert(r, obj);
        }
    }

    void remove_branch(int i);
    int remove(rectangle const& r, anyref obj);

    Q_page() : object(self_class)
    {
        n = 0;
        next = NULL;
    }

    int4   n; // number of branches at page
    branch b[card];
    rectangle rect;
    ref<Q_page> next;

    METACLASS_DECLARATIONS(Q_page, object);
};

inline field_descriptor& describe_field(Q_page::branch& b) { 
    return b.describe_components();
}

class GOODS_DLL_EXPORT Q_node : public object { 
  public:
  
    int search(rectangle const& r, callback& cb) const;
    int search(rectangle const& r, search_buffer& sbuf) const;

    int getnodescount() const {
        int n=1;
        for (int4 i =0; i < nsubnodes; i++)
        {
            n += subnodes[i]->getnodescount();
        }
        return n;
    };
    
    void getstats(int& nEmptyNodes, int& nNonEmptyNodes, int& nFullPage, int& nMinCount, int& nMaxCount, rectangle& fullest) const
    {
        if (numobjects > 0)
        {
            nNonEmptyNodes++;
            if ((nMinCount == -1) || numobjects < nMinCount)
                nMinCount = numobjects;
            if ((nMaxCount == -1) || numobjects > nMaxCount)
            {
                nMaxCount = numobjects;
                fullest = rect;
            }
            if (!page->next.is_nil())
                nFullPage++;
        } else { 
            nEmptyNodes++;
        }
        for (int4 i =0; i < nsubnodes; i++)
        {
            subnodes[i]->getstats(nEmptyNodes, nNonEmptyNodes, nFullPage, nMinCount, nMaxCount, fullest);
        }               
    };

    bool insert(rectangle const& r, anyref obj, int depth);
    boolean remove(rectangle const& r, anyref obj);

    Q_node(rectangle const& r);
    Q_node(ref<Q_node> old_root, ref<Q_node> new_page);

    rectangle   rect;       // bounding rect
    int4    nsubnodes; // number of subnodes
    ref<Q_node>     subnodes[4];    // subnodes
    int4   numobjects; // number of objects
    ref<Q_page> page;


    METACLASS_DECLARATIONS(Q_node, object);
};



class GOODS_DLL_EXPORT Q_tree : public object { 
  public: 
    int     search(rectangle const& r, callback& cb) const;     
    int     search(rectangle const& r, search_buffer& sbuf) const;       
    void    insert(rectangle const& r, anyref obj);
    boolean remove(rectangle const& r, anyref obj);

    int getnodescount() const {
        return root->getnodescount();
    }
    
    void getstats(int& nEmptyNodes, int& nNonEmptyNodes, int& nFullPage, int& nMinCount, int& nMaxCount, int& nAvgCount, rectangle& fullest ) const
    {
        nMinCount = -1;
        nMaxCount = -1;
        nEmptyNodes = 0;
        nNonEmptyNodes = 0;
        nFullPage = 0;
        root->getstats(nEmptyNodes, nNonEmptyNodes, nFullPage, nMinCount, nMaxCount, fullest);
        nAvgCount = n_records / nNonEmptyNodes;
    }

    void prune(nat4 maxdepth, rectangle const& r) { 
        root = NEW Q_node(r); 
        n_records = 0; 
        m_maxdepth = maxdepth; 
    }

    METACLASS_DECLARATIONS(Q_tree, object);

    Q_tree(nat4 maxdepth, rectangle const& r, class_descriptor& desc = self_class) : object(desc) {
        prune(maxdepth, r);
    }

  protected:
    nat4        n_records;
    nat4        m_maxdepth;
    ref<Q_node> root;
};

END_GOODS_NAMESPACE

#endif



