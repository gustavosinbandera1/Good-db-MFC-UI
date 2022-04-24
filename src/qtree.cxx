// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< QTREE.CXX >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     14-Jan-98    K.A. Knizhnik  * / [] \ *
//                          Last update: 21-Jan-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Q tree class implementation
//-------------------------------------------------------------------*--------*

#include "qtree.h"

BEGIN_GOODS_NAMESPACE

field_descriptor& Q_tree::describe_components()
{
    return FIELD(n_records), FIELD(m_maxdepth), FIELD(root);
}

void Q_tree::insert(const rectangle & r, anyref obj)
{
    ref<Q_node> node = root;
    if (modify(node)->insert(r, obj, m_maxdepth))
        n_records++;
}

boolean Q_tree::remove(rectangle const& r, anyref obj)
{
    ref<Q_node> node = root;
    return (modify(node)->remove(r, obj));
}

int Q_tree::search(rectangle const& r, callback& cb) const
{
    return root->search(r, cb);
}

int Q_tree::search(rectangle const& r, search_buffer& sbuf) const
{
    return root->search(r, sbuf);
}

REGISTER_EX(Q_tree, object, pessimistic_repeatable_read_scheme, 
            class_descriptor::cls_aligned);

Q_node::Q_node(rectangle const& r) : object(self_class)
{
    numobjects = 0;
    rect = r;
    next = NULL;
}

field_descriptor& Q_node::describe_components()
{
    return FIELD(rect), FIELD(nsubnodes), ARRAY(subnodes), FIELD(numobjects), FIELD(page);
}

bool Q_node::insert(const rectangle & r, anyref obj, int depth)
{
    if (depth > 1) 
    {
        // We don't have reached the max depth
        if (nsubnodes > 0)
        {
            // Try to insert into existing subnodes
            for (int i=0; i < nsubnodes; i++)
            {
                if (subnodes[i]->rect.contains(r))
                {
                    return modify(subnodes[i])->insert(r, obj, depth-1);
                }
            }
        }
        else 
        {
            // No nodes allready : split this one
            rectangle half1, half2, quad1, quad2, quad3, quad4;
            rect.split(half1, half2);
            half1.split(quad1, quad2);
            half2.split(quad3, quad4);
            if (quad1.contains(r) || quad2.contains(r) || quad3.contains(r) || quad4.contains(r))
            {
                nsubnodes=4;
                subnodes[0]= NEW Q_node(quad1);
                subnodes[1]= NEW Q_node(quad2);
                subnodes[2]= NEW Q_node(quad3);
                subnodes[3]= NEW Q_node(quad4);

                                // Now insert in one of these subnodes
                return insert(r, obj, depth);
            }
        }
    }

    // All that's left is putting it there...
    if (page.is_nil())
        page = NEW Q_page();
    modify(page)->insert(r, obj);

    numobjects++;

    return True;
}

int Q_node::search(rectangle const& r, callback& cb) const
{
    if (!(r & rect))
        return 0;

    int hit_count=0;

    if (!page.is_nil())
    {
        hit_count += page->search(r, cb);
    }

    for (int4 i=0; i < nsubnodes; i++)
    {
        hit_count += subnodes[i]->search(r, cb);
    }

    return hit_count;
}

int Q_node::search(rectangle const& r, search_buffer& sbuf) const
{
    // If not overlap, return with nothing
    if (!(r & rect))
        return 0;

    int hit_count=0;
    if (!page.is_nil())
    {
        hit_count += page->search(r, sbuf);
    }

    for (int4 i=0; i < nsubnodes; i++)
    {
        hit_count += subnodes[i]->search(r, sbuf);
    }

    return hit_count;

}

boolean Q_node::remove(rectangle const& r, anyref obj)
{
    if (!(r & rect))
        return false;

    boolean done = false;
    if (!page.is_nil())
    {
        done = modify(page)->remove(r, obj);
    }

    for (int4 i=0; i < nsubnodes && !done; i++)
    {
        done = modify(subnodes[i])->remove(r, obj);
    }

    return done;
}

REGISTER_EX(Q_node, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);

int Q_page::search(rectangle const& r, callback& cb) const
{
    int hit_count=0;

    if ((r & rect))
    {
        for (int4 i=0; i < n; i++)
        {
            if (r & b[i].rect)
            {
                cb.apply(b[i].p);
                hit_count += 1;
            }
        }
    }

    if (!next.is_nil())
        hit_count += next->search(r, cb);

    return hit_count;
}

int Q_page::search(rectangle const& r, search_buffer& sbuf) const
{
    int hit_count=0;

    if ((r & rect))
    {
        for (int4 i=0; i < n; i++)
        {
            if (r & b[i].rect)
            {
                sbuf.put((b[i].p));
                hit_count += 1;
            }
        }
    }

    if (!next.is_nil())
        hit_count += next->search(r, sbuf);

    return hit_count;

}

void Q_page::remove_branch(int i)
{
    b[i].p = NULL; // memset will not call reference destructor,
    // so to provide reference consistency, zero reference
    memcpy(&b[i], &b[i+1], (n-i-1)*sizeof(branch));
    memset(&b[--n], 0, sizeof(branch));

    if (n)
    {
        rect = b[0].rect;
        for(int4 j=1; j < n; j++)
            rect += b[j].rect;
    }
}

int Q_page::remove(rectangle const& r, anyref obj)
{
    boolean done = false;

    if ((r & rect))
    {
        for (int4 i=0; i < n; i++)
        {
            if (obj == b[i].p)
            {
                remove_branch(i);
                done = true;
            }
        }
    }

    if (!next.is_nil() && !done)
        done = modify(next)->remove(r, obj);

    return done;
}

field_descriptor& Q_page::describe_components()
{
    return FIELD(rect), FIELD(n), FIELD(next), ARRAY(b);
}

REGISTER_EX(Q_page, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);

END_GOODS_NAMESPACE
