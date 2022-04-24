// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< RTREE.CXX >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     14-Jan-98    K.A. Knizhnik  * / [] \ *
//                          Last update: 21-Jan-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// R tree class implementation
//-------------------------------------------------------------------*--------*

#include "rtree.h"

BEGIN_GOODS_NAMESPACE

void R_tree::insert(rectangle const& r, anyref obj)
{
    if (root.is_nil()) { 
        root = NEW R_page(r, obj);
        height = 1;
    } else { 
        ref<R_page> p = root->insert(r, obj, height);
        if (!p.is_nil()) { 
            // root splitted
            root = NEW R_page(root, p);
            height += 1;
        }
    }
    n_records += 1;
}


boolean R_tree::remove(rectangle const& r, anyref obj)
{
    if (height != 0) { 
        R_page::reinsert_list rlist;
        if (root->remove(r, obj, height, rlist)) { 
            ref<R_page> pg = rlist.chain;
            int level = rlist.level;
            while (!pg.is_nil()) {
                for (int i = 0, n = pg->n; i < n; i++) { 
                    ref<R_page> p = root->insert(pg->b[i].rect, 
                                                 pg->b[i].p, height-level);
                    if (!p.is_nil()) { 
                        // root splitted
                        root = NEW R_page(root, p);
                        height += 1;
                    }
                }
                level -= 1;
                pg = pg->next_reinsert_page();
            }
            if (root->n == 1 && height > 1) { 
                root = root->b[0].p;
                height -= 1;
            }
            n_records -= 1;
            return True;
        }
    }
    return False;
}

int R_tree::search(rectangle const& r, callback& cb) const
{
    return (n_records != 0) ? root->search(r, cb, height) : 0;
}

int R_tree::search(rectangle const& r, search_buffer& sb) const
{
    return (n_records != 0) ? root->search(r, sb, height) : 0;
}

field_descriptor& R_tree::describe_components()
{
    return FIELD(n_records), FIELD(height), FIELD(root); 
}

#ifndef USER_REGISTER
REGISTER_EX(R_tree, object, pessimistic_repeatable_read_scheme,
			class_descriptor::cls_aligned | class_descriptor::cls_non_relational);
#endif

//-------------------------------------------------------------------------
// R-tree page methods
//-------------------------------------------------------------------------

//
// Search for objects overlapped with specified rectangle and call
// callback method for all such objects.
//
int R_page::search(rectangle const& r, callback& cb, int level) const
{
    assert(level >= 0);
    int hit_count = 0;
    if (--level != 0) { /* this is an internal node in the tree */
        for (int i=0; i < n; i++) { 
            if ((r & b[i].rect) != 0) {
                ref<R_page> child = (ref<R_page>)b[i].p;
                hit_count += child->search(r, cb, level);
            }
        }
    } else { /* this is a leaf node */
        for (int i=0; i < n; i++) { 
            if ((r & b[i].rect) != 0) {
                cb.apply(b[i].p);
                hit_count += 1;
            }
        }
    }
    return hit_count;
}

//
// Search for objects overlapped with specified rectangle and place
// all such objects in search buffer.
//
int R_page::search(rectangle const& r, search_buffer& sb, int level) const
{
    assert(level >= 0);
    int hit_count = 0;
    if (--level != 0) { /* this is an internal node in the tree */
        for (int i=0; i < n; i++) { 
            if ((r & b[i].rect) != 0) {
                ref<R_page> child = (ref<R_page>)b[i].p;
                hit_count += child->search(r, sb, level);
            }
        }
    } else { /* this is a leaf node */
        for (int i=0; i < n; i++) { 
            if ((r & b[i].rect) != 0) {
                sb.put(b[i].p);
                hit_count += 1;
            }
        }
    }
    return hit_count;
}

//
// Create root page
//
R_page::R_page(rectangle const& r, anyref obj) : object(self_class)
{
    n = 1;
    b[0].rect = r;
    b[0].p = obj;
}

//
// Create new root page (root splitting)
//
R_page::R_page(ref<R_page> old_root, ref<R_page> new_page) : object(self_class)
{
    n = 2;
    b[0].rect = old_root->cover();
    b[0].p = old_root;
    b[1].rect = new_page->cover();
    b[1].p = new_page;
}

//
// Calculate cover of all rectangles at page
//
rectangle R_page::cover() const 
{
    rectangle r = b[0].rect;
    for (int i = 1; i < n; i++) { 
        r += b[i].rect;
    }
    return r;
}

ref<R_page> R_page::split_page(branch const& br)
{
    int i, j, seed[2];
    size_t rect_area[card+1], waste, worst_waste = 0;
    //
    // As the seeds for the two groups, find two rectangles which waste 
    // the most area if covered by a single rectangle.
    //
    rect_area[0] = area(br.rect);
    for (i = 0; i < card; i++) { 
        rect_area[i+1] = area(b[i].rect);
    }
    branch const* bp = &br;
    for (i = 0; i < card; i++) { 
        for (j = i+1; j <= card; j++) { 
            waste = area(bp->rect + b[j-1].rect) - rect_area[i] - rect_area[j];
            if (waste >= worst_waste) {
                worst_waste = waste;
                seed[0] = i;
                seed[1] = j;
            }
        }
        bp = &b[i];
    }       
    char taken[card];
    rectangle group[2];
    size_t group_area[2];
    int group_card[2];
    ref<R_page> p;
    
    memset(taken, 0, sizeof taken);
    taken[seed[1]-1] = 2;
    group[1] = b[seed[1]-1].rect;
    
    if (seed[0] == 0) { 
        group[0] = br.rect;
        p = NEW R_page(br.rect, br.p);
    } else { 
        group[0] = b[seed[0]-1].rect;
        p = NEW R_page(group[0], b[seed[0]-1].p);
        b[seed[0]-1] = br;
    }
    group_card[0] = group_card[1] = 1;
    group_area[0] = rect_area[seed[0]];
    group_area[1] = rect_area[seed[1]];
    //
    // Split remaining rectangles between two groups.
    // The one chosen is the one with the greatest difference in area 
    // expansion depending on which group - the rect most strongly 
    // attracted to one group and repelled from the other.
    //
    while (group_card[0] + group_card[1] < card + 1 
           && group_card[0] < card + 1 - min_fill
           && group_card[1] < card + 1 - min_fill)
    {
        int better_group = -1, chosen = -1, biggest_diff = -1;
        for (i = 0; i < card; i++) { 
            if (!taken[i]) { 
                int diff = (int)((area(group[0] + b[i].rect) - group_area[0])
                         - (area(group[1] + b[i].rect) - group_area[1]));
                if (diff > biggest_diff || -diff > biggest_diff) { 
                    chosen = i;
                    if (diff < 0) { 
                        better_group = 0;
                        biggest_diff = -diff;
                    } else { 
                        better_group = 1;
                        biggest_diff = diff;
                    }
                }
            }
        }
        assert(chosen >= 0);
        group_card[better_group] += 1;
        group[better_group] += b[chosen].rect;
        group_area[better_group] = area(group[better_group]);
        taken[chosen] = better_group+1;
        if (better_group == 0) { 
            modify(p)->b[group_card[0]-1] = b[chosen];
        }
    }
    //
    // If one group gets too full, then remaining rectangle are
    // split between two groups in such way to balance cards of two groups.
    //
    if (group_card[0] + group_card[1] < card + 1) { 
        for (i = 0; i < card; i++) { 
            if (!taken[i]) { 
                if (group_card[0] >= group_card[1]) { 
                    taken[i] = 2;
                    group_card[1] += 1;
                } else { 
                    taken[i] = 1;
                    modify(p)->b[group_card[0]++] = b[i];               
                }
            }
        }
    }
    modify(p)->n = group_card[0];
    n = group_card[1];
    for (i = 0, j = 0; i < n; j++) { 
        if (taken[j] == 2) {
            b[i++] = b[j];
        }
    }
    while (i < card) { 
        b[i++].p = NULL;
    }
    return p;
}

void R_page::remove_branch(int i)
{
    b[i].p = NULL; // memset will not call reference destructor,
                   // so to provide reference consistency, zero reference
    memcpy(&b[i], &b[i+1], (n-i-1)*sizeof(branch));
    memset(&b[--n], 0, sizeof(branch));
}
    
ref<R_page> R_page::insert(rectangle const& r, anyref obj, int level) 
const
{
    branch br;
    if (--level != 0) { 
        // not leaf page
        int i, mini = 0;
        size_t min_incr = INT_MAX;
	size_t best_area = INT_MAX;
        for (i = 0; i < n; i++) { 
            size_t r_area = area(b[i].rect);
            size_t incr = area(b[i].rect + r) - r_area;
            if (incr < min_incr) { 
                best_area = r_area;
                min_incr = incr;
                mini = i;
            } else if (incr == min_incr && r_area < best_area) { 
                best_area = r_area;
                mini = i;
            } 
        }
        ref<R_page> p = (ref<R_page>)b[mini].p;
        ref<R_page> q = p->insert(r, obj, level);
        if (q.is_nil()) { 
            // child was not split
            modify(this)->b[mini].rect += r;
            return NULL;
        } else { 
            // child was split
            modify(this)->b[mini].rect = p->cover();
            br.p = q;
            br.rect = q->cover();
            return modify(this)->add_branch(br);
        }
    } else { 
        br.p = obj;
        br.rect = r;
        return modify(this)->add_branch(br);
    }
}

boolean R_page::remove(rectangle const& r, anyref obj, 
                       int level, reinsert_list& rlist) const
{
    if (--level != 0) { 
        for (int i = 0; i < n; i++) { 
            if (b[i].rect & r) { 
                ref<R_page> p = (ref<R_page>)b[i].p;
                if (p->remove(r, obj, level, rlist)) { 
                    if (p->n >= min_fill) { 
                        modify(this)->b[i].rect = p->cover();
                    } else { 
                        // not enough entries in child
                        modify(p)->b[card-1].p = rlist.chain;
                        rlist.chain = p;
                        rlist.level = level - 1; 
                        modify(this)->remove_branch(i);
                    }
                    return True;
                }
            }
        }
    } else {
        for (int i = 0; i < n; i++) { 
            if (b[i].p == obj) { 
                modify(this)->remove_branch(i);
                return True;
            }
        }
    }
    return False;
}

field_descriptor& R_page::describe_components()
{
    return FIELD(n), ARRAY(b); 
}

REGISTER_EX(R_page, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);

END_GOODS_NAMESPACE
