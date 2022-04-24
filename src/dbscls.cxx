// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< DBSCLS.CXX >----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     28-Mar-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 18-Jun-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Some common database classes
//-------------------------------------------------------------------*--------*

#include "dbscls.h"

BEGIN_GOODS_NAMESPACE

field_descriptor& String::describe_components()
{
    return NO_FIELDS;
}

//
// Registration of array classes
//

#ifndef USER_REGISTER
REGISTER_TEMPLATE(ArrayOfByte, object, pessimistic_repeatable_read_scheme);
REGISTER_TEMPLATE(ArrayOfInt, object, pessimistic_repeatable_read_scheme);
REGISTER_TEMPLATE(ArrayOfDouble, object, pessimistic_repeatable_read_scheme);
REGISTER_TEMPLATE(ArrayOfObject, object, pessimistic_repeatable_read_scheme);
REGISTER(String, ArrayOfByte, pessimistic_repeatable_read_scheme);
#endif

//
// Set methods
//

ref<set_member> set_member::first() const
{
    return owner->first; 
}

ref<set_member> set_member::last() const
{
    return owner->last; 
}

int set_member::compare(const char* thatKey) const
{
    return compare(thatKey, strlen(thatKey)+1);
}

int set_member::compare(ref<set_member> mbr) const
{
    return -mbr->compare(key, size - this_offsetof(set_member, key));
}

int set_member::compare(const char* thatKey, size_t thatSize) const
{
    size_t this_size = this->size - this_offsetof(set_member, key);
    size_t len = thatSize < this_size ? thatSize : this_size; 
    int diff = memcmp(this->key, thatKey, len);
    return diff == 0 ? (int)(this_size - thatSize) : diff; 
}

int set_member::compareIgnoreCase(const char* thatKey) const
{
    int n = size - this_offsetof(set_member, key);
    char const* p = key;
    while (--n >= 0) { 
        if (*thatKey == '\0') { 
            return 1;
        }
        if (toupper(*thatKey & 0xFF) != toupper(*p & 0xFF)) { 
            return toupper(*thatKey & 0xFF) - toupper(*p & 0xFF);
        }
        p += 1;
        thatKey += 1;
    }
    return -(*thatKey & 0xFF);
}

size_t set_member::copyKeyTo(char* buf, size_t bufSize) const
{
    size_t len = size - this_offsetof(set_member, key);
    if (len < bufSize) { 
        bufSize = len;
        buf[len] = '\0';
    }
    memcpy(buf, key, bufSize);
    return len;
}

skey_t set_member::get_key() const
{
    return str2key(key, size - this_offsetof(set_member, key));
}

skey_t set_member::str2key(const char* key_str, size_t key_len)
{
#ifdef USE_LOCALE_SETTINGS
    char buf[sizeof(skey_t)];
    strxfrm(buf, (char*)key_str, sizeof buf);
    key_str = buf;
#endif
    skey_t key = 0;
    nat1* s = (nat1*)key_str;
    for (int n = sizeof(skey_t); key_len != 0 && --n >= 0; key_len -= 1) { 
        key |= skey_t(*s++) << (n << 3);
    } 
    return key;
} 

void set_member::clear()
{
    next = NULL;
    prev = NULL;
    owner = NULL;
    obj = NULL;
}

field_descriptor& set_member::describe_components()
{
    return FIELD(next), FIELD(prev), FIELD(owner), FIELD(obj), VARYING(key); 
}

void result_set_cursor::select(hnd_t owner, char const* table, char const* where, char const* order_by, nat4 limit, nat4 offset)
{
	storage = owner->storage;
	i = 0;
	first_mbr = 0;
	storage->query(*this, owner->opid, table, where, order_by, limit, offset);
}

void result_set_cursor::select(hnd_t first, char const* q, nat4 result_set_buf_size, int query_flags, nat4 max_traversed_members)
{
	query.change_size(strlen(q) + 1);
    strcpy(&query, q);
    buf_size = result_set_buf_size;
    max_members = max_traversed_members;
    flags = query_flags;
	i = 0;
	first_mbr = first->opid;
	last_mbr = 0;
	storage = first->storage;
	storage->query(*this, first_mbr, last_mbr, &query, buf_size, flags, max_members);
}

void result_set_cursor::select_range(hnd_t first, hnd_t last, char const* q, nat4 result_set_buf_size, int query_flags, nat4 max_traversed_members)
{
	query.change_size(strlen(q) + 1);
	strcpy(&query, q);
	buf_size = result_set_buf_size;
	max_members = max_traversed_members;
	flags = query_flags;
	i = 0;
	first_mbr = first->opid;
	last_mbr = last->opid;
	storage = first->storage;
	storage->query(*this, first_mbr, last_mbr, &query, buf_size, flags, max_members);
}

anyref result_set_cursor::next()
{
    if (i == result.size() && first_mbr) { 
        i = 0;
        do { 
			storage->query(*this, first_mbr, last_mbr, &query, buf_size, flags, max_members);
		} while (first_mbr != 0 && result.size() == 0 && max_members != UINT_MAX);
    } 
    if (i < result.size()) { 
        return result[i++];
    }
    return NULL;
}

result_set_cursor::result_set_cursor() : buf_size(0), i(0), first_mbr(0), last_mbr(0), storage(NULL)
{
}

void result_set_cursor::clear()
{
	 buf_size = 0;
	 i = 0;
	 first_mbr = 0;
	 last_mbr = 0;
	 storage = NULL;
	 max_members = 0;
	 flags = 0;
}

const size_t CNV_BUFFER_SIZE = 1024;
    
void set_member::filter(result_set_cursor& cursor, char const* query, nat4 buf_limit, int flags, nat4 max_members) const
{
    cursor.select(hnd, query, buf_limit, flags, max_members);
}

void set_owner::select(result_set_cursor& cursor, char const* table, char const* where, char const* order_by, nat4 limit, nat4 offset) const
{
	if (empty()) {
		cursor.clear();
		return;
	}
	cursor.select(hnd, table, where, order_by, limit, offset);
}

void set_owner::filter(result_set_cursor& cursor, char const* query, nat4 buf_limit, int flags, nat4 max_members) const
{
	if (empty()) {
		cursor.clear();
		return;
	}
	hnd_t first_hnd = (flags & qf_backward) == 0 ? first->get_handle() : last->get_handle();
	cursor.select(first_hnd, query, buf_limit, flags, max_members);
}

void set_owner::filter_range(result_set_cursor& cursor, ref<set_member> const& start, ref<set_member> const& end, char const* query, nat4 buf_limit, int flags, nat4 max_members) const
{
	if (empty() || start.is_nil() || end.is_nil() || start->owner != this || end->owner != this)
	{
		cursor.clear();
		return;
	}

	hnd_t start_hnd = (flags & qf_backward) == 0 ? start->get_handle() : end->get_handle();
	hnd_t end_hnd = (flags & qf_backward) == 0 ? end->get_handle() : start->get_handle();

	cursor.select_range(start_hnd, end_hnd, query, buf_limit, flags, max_members);
}

void set_owner::preload_members(boolean pin) const
{
    int flags = lof_bulk;
    if (pin) { 
        flags |= lof_pin;
    }
    object_monitor::lock_global();
    hnd->storage->load(hnd, flags);
    object_monitor::unlock_global();
}



ref<set_member> set_owner::find(const wchar_t* str) const
{
    char buf[CNV_BUFFER_SIZE];
    wcstombs(buf, str, CNV_BUFFER_SIZE-1);
    buf[CNV_BUFFER_SIZE-1] = 0;
    return find(buf);
}

ref<set_member> set_owner::find(const char* key, size_t key_len) const
{
    for (ref<set_member> mbr = first; mbr != NULL; mbr = mbr->next) { 
        if (mbr->compare(key, key_len) == 0) { 
            return mbr; 
        }
    }
    return NULL;
} 

void set_owner::clear()
{
    ref<set_member> mbr = first;
    while (!mbr.is_nil()) {
        ref<set_member> next = mbr->next;
        modify(mbr)->clear();
        mbr = next;
    }
    n_members = 0;
    last = NULL;
    first = NULL;
    obj = NULL;
}

void set_owner::put_first(ref<set_member> mbr)
{
    assert(mbr->owner == NULL && mbr->next == NULL && mbr->prev == NULL);
    modify(mbr)->owner = this;
    modify(mbr)->next = first;
    modify(mbr)->prev = NULL; 
    if (!first.is_nil()) { 
        modify(first)->prev = mbr;
    } else { 
        last = mbr; 
    }
    first = mbr; 
    n_members += 1;
}

void set_owner::put_last(ref<set_member> mbr, boolean cluster)
{
    assert(mbr->owner == NULL && mbr->next == NULL && mbr->prev == NULL);
    modify(mbr)->owner = this;
    modify(mbr)->next = NULL;
    modify(mbr)->prev = last; 
    if (!last.is_nil()) { 
        modify(last)->next = mbr;
        if (cluster) { 
            modify(mbr)->cluster_with(last, True);
        }
    } else { 
        first = mbr; 
    }
    last = mbr; 
    n_members += 1;
}

void set_owner::put_before(ref<set_member> before, ref<set_member> mbr)
{
    assert(mbr->owner == NULL && mbr->next == NULL && mbr->prev == NULL);
    if (first == before) { 
        put_first(mbr);
    } else { 
        modify(mbr)->owner = this;
        modify(mbr)->next = before;
        modify(mbr)->prev = before->prev; 
        modify(before)->prev = mbr;
        modify(mbr->prev)->next = mbr;
        n_members += 1;
    }
}


void set_owner::put_after(ref<set_member> after, ref<set_member> mbr)
{
    assert(mbr->owner == NULL && mbr->next == NULL && mbr->prev == NULL);
    if (last == after) { 
       put_last(mbr);
    } else {  
        modify(mbr)->owner = this;
        modify(mbr)->next = after->next;
        modify(mbr)->prev = after; 
        modify(after)->next = mbr;
        modify(mbr->next)->prev = mbr;
        n_members += 1;
    }
}

void set_owner::remove(ref<set_member> mbr)
{
	//[MC] Avoid removing a member of another list in release version 
	if (mbr->owner != this)
	{
		assert(0);
		return;
	}

    if (mbr == first) { 
        (void)get_first();
    } else if (mbr == last) { 
        (void)get_last();
    } else { 
        modify(mbr->prev)->next = mbr->next; 
        modify(mbr->next)->prev = mbr->prev; 
        modify(mbr)->next = NULL; 
        modify(mbr)->prev = NULL; 
        modify(mbr)->owner = NULL; 
        n_members -= 1;
    }    
}
   
ref<set_member> set_owner::get_first()
{
    ref<set_member> mbr = first; 
    if (!mbr.is_nil()) { 
        first = first->next; 
        if (first.is_nil()) { 
            last = NULL; 
        } else { 
            modify(first)->prev = NULL; 
        }
        modify(mbr)->next = NULL; 
        modify(mbr)->prev = NULL; 
        modify(mbr)->owner = NULL; 
        n_members -= 1;
    }
    return mbr; 
}

ref<set_member> set_owner::get_last()
{
    ref<set_member> mbr = last; 
    if (!mbr.is_nil()) { 
        last = last->prev; 
        if (last.is_nil()) { 
            first = NULL; 
        } else { 
            modify(last)->next = NULL; 
        }
        modify(mbr)->next = NULL; 
        modify(mbr)->prev = NULL; 
        modify(mbr)->owner = NULL; 
        n_members -= 1;
    }
    return mbr; 
}

size_t set_owner::iterate(member_function f, void const* arg) const
{
    size_t count = 0;
    ref<set_member> next, mbr;
    for (mbr = first; !mbr.is_nil(); mbr = next) {
        next = mbr->next;
        f(mbr, arg);
        count += 1;
    }
    return count;
}


field_descriptor& set_owner::describe_components()
{
    return FIELD(first), FIELD(last), FIELD(obj), FIELD(n_members); 
}

#ifndef USER_REGISTER
REGISTER(set_member, object, optimistic_scheme);
//[MC] We replaced pessimistic_repeatable_read_scheme with pessimistic_concurrent_read_scheme
//to allow more concurrency and avoid deadlocks
REGISTER_EX(set_owner, object, pessimistic_concurrent_read_scheme, class_descriptor::cls_hierarchy_super_root);
#endif

//
// B tree
//

void B_tree::clear()
{
    set_owner::clear();
    root = NULL;
    height = 0;
}

ref<set_member> B_tree::find(skey_t key) const
{
    if (root.is_nil()) { 
        return NULL; 
    } else { 
        return root->find(height, key, n); 
    }
}

ref<set_member> B_tree::findGE(skey_t key) const
{
    if (root.is_nil()) { 
        return NULL; 
    } else { 
        return root->findGE(height, key, n); 
    }
}

ref<set_member> B_tree::find(const char* str, size_t len, skey_t key) const
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

ref<set_member> B_tree::findGE(const char* str, size_t len, skey_t key) const
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

ref<set_member> B_tree::find(const char* str) const
{
    size_t len = strlen(str); 
    return find(str, len + 1, set_member::str2key(str, len)); 
}

ref<set_member> B_tree::findGE(const char* str) const
{
    size_t len = strlen(str); 
    return findGE(str, len, set_member::str2key(str, len)); 
}

#define MAX_KEY ((skey_t)-1)

void B_tree::insert(ref<set_member> mbr) 
{    
	//[MC] Avoid inserting a used member in release version 
	if(mbr->owner != NULL || mbr->next != NULL || mbr->prev != NULL)
	{
		abort_transaction();
		exit(EXIT_FAILURE);
	}

    B_page::item ins;
    ins.p = mbr;
    ins.key = mbr->get_key();

    if (root == NULL) { 
        put_last(mbr); 
        root = B_page::create(NULL, ins, n); 
        height = 1;
    } else if (root->find_insert_position(height, this, ins, n) 
               == B_page::overflow) 
    { 
        root = B_page::create(root, ins, n);
        height += 1;
    } 
}

void B_tree::remove(ref<set_member> mbr)
{
    B_page::item rem; 
    rem.key = mbr->get_key(); 
    rem.p = mbr; 
    if (modify(root)->find_remove_position(height, rem, n) == B_page::underflow
        && root->m == n*2-1) 
    { 
        root = root->e[n*2-1].p;
        height -= 1;
    }
    set_owner::remove(mbr);
}

void B_tree::ok() const 
{
    if (root != NULL) { 
        root->ok(0, MAX_KEY, 0, height, n);
    }
}

void B_tree::dump() const
{ 
    console::output("B_tree[%08lx]: height = %d, records = %d\n",
                    hnd->opid, height, n_members); 
    if (root != NULL) { 
        root->dump(0, height, n); 
    }
}

field_descriptor& B_tree::describe_components()
{
    return FIELD(root), FIELD(height), FIELD(n); 
}


B_page::B_page(ref<B_page> const& root, item& ins, int4 n) :
    object(self_class, 2*n)
{
    if (ins.key == MAX_KEY) { 
        m = 2*n-1;
        e[2*n-1] = ins;
    } else {
        m = 2*n-2;
        e[2*n-2] = ins;
        e[2*n-1].key = MAX_KEY;
        e[2*n-1].p = root;
    }
}

ref<set_member> B_page::find(int height, skey_t key, int4 n) const
{
    int l = m, r = 2*n-1;
    while (l < r)  {
        int i = (l+r) >> 1;
        if (key > e[i].key) l = i+1; else r = i;
    }
    internal_assert(r == l && e[r].key >= key); 
    if (--height != 0) { 
        ref<B_page> child = (ref<B_page>)e[r].p;
        return child->find(height, key, n); 
    } else { 
        if (key == e[r].key) { 
            return (ref<set_member>)e[r].p; 
        } else { 
            return NULL;
        }
    }
}

ref<set_member> B_page::findGE(int height, skey_t key, int4 n) const
{
    int l = m, r = 2*n-1;
    while (l < r)  {
        int i = (l+r) >> 1;
        if (key > e[i].key) l = i+1; else r = i;
    }
    internal_assert(r == l && e[r].key >= key); 
    if (--height != 0) { 
        ref<B_page> child = (ref<B_page>)e[r].p;
        return child->findGE(height, key, n); 
    } else { 
        return (ref<set_member>)e[r].p; 
    }
}

B_page::operation_effect B_page::find_insert_position(int level, 
                                                      ref<B_tree> const& tree, 
                                                      item& ins,
                                                      int4 n) const
{
    int  i, l = m, r = 2*n-1;
    skey_t key = ins.key;
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
        ref<B_page> child = (ref<B_page>)e[r].p;
        if (child->find_insert_position(level, tree, ins, n) == done) { 
            return done;
        }
    }
    // insert before e[r]
    return modify(this)->insert(r, ins, n); 
}

B_page::operation_effect B_page::insert(int r, item& ins, int4 n)
{
    // insert before e[r]
    if (m > 0) {
        memmove(&e[m-1], &e[m], (r - m)*sizeof(item));
        memset(&e[r-1], 0, sizeof(item));
        m -= 1;
        e[r-1] = ins;
        return done;
    } else { // page is full then divide page
        ref<B_page> b = B_page::create(n);
        if (r < n) {
            memcpy(&modify(b)->e[n], e, r*sizeof(item));
            modify(b)->e[n+r] = ins;
            memcpy(&modify(b)->e[n+r+1], &e[r], (n-r-1)*sizeof(item));
        } else {
            memcpy(&modify(b)->e[n], e, n*sizeof(item));
            memmove(&e[n-1], &e[n], (r-n)*sizeof(item));
            memset(&e[r-1], 0, sizeof(item));
            e[r-1] = ins;
        }
        ins.key = b->e[2*n-1].key;
        ins.p = b;
        m = n-1;
        modify(b)->m = n;
        memset(e, 0, (n-1)*sizeof(item));
        return overflow;
    }
}

B_page::operation_effect B_page::find_remove_position(int level, 
                                                      B_page::item& rem,
                                                      int4 n) const
{
    int i, l = m, r = 2*n-1;
    skey_t key = rem.key;
    while (l < r) {
        i = (l+r) >> 1;
        if (key > e[i].key) l = i+1; else r = i;
    }
    internal_assert(r == l && e[r].key >= key); 
    if (--level == 0) {
        assert(e[r].key == key);  
        ref<set_member> mbr = (ref<set_member>)rem.p;
        if (mbr == e[r].p) { 
            if (mbr->next != NULL && mbr->next->get_key() == key) { 
                modify(this)->e[r].p = mbr->next;
                return done; 
            } else {
                return modify(this)->remove(r, rem, n); 
            }
        }
    } else { 
        ref<B_page> child = (ref<B_page>)e[r].p; 
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

B_page::operation_effect B_page::remove(int r, item& rem, int4 n)
{
    e[r].p = NULL; 
    if (e[r].key == MAX_KEY) { 
        return done; 
    } else { 
        memmove(&e[m+1], &e[m], (r - m)*sizeof(item));
        memset(&e[m], 0, sizeof(item));
        rem.key = e[2*n-1].key; 
        return (++m > n) ? underflow : propagation; 
    }
}

B_page::operation_effect B_page::handle_underflow(int r, skey_t key, item& rem,
                                                  int4 n)
{
    ref<B_page> a = (ref<B_page>)e[r].p;
    internal_assert(a->m == n+1);
    if (r < 2*n-1) { // exists greater page
        ref<B_page> b = (ref<B_page>)e[r+1].p;
        int bm = b->m; 
        if (bm < n) {  // reallocation of nodes between pages a and b
            int i = (n - bm + 1) >> 1;
            memmove(&modify(a)->e[n+1-i], &a->e[n+1], (n-1)*sizeof(item));
            memcpy(&modify(a)->e[2*n-i], &b->e[bm], i*sizeof(item));
            memset(&modify(b)->e[bm], 0, i*sizeof(item) );
            modify(b)->m += i;
            modify(a)->m -= i;
            e[r].key = a->e[2*n-1].key;
            return done;
        } else { // merge page a to b  
            internal_assert(bm == n); 
            memcpy(&modify(b)->e[1], &a->e[n+1], (n-1)*sizeof(item));
            memset(&modify(a)->e[n+1], 0, (n-1)*sizeof(item));
            e[r].p = NULL; 
            memmove(&e[m+1], &e[m], (r-m)*sizeof(item));
            memset(&e[m], 0, sizeof(item));
            modify(b)->m = 1;
            // dismiss page 'a'
            return ++m > n ? underflow : done;
        }
    } else { // page b is before a
        ref<B_page> b = (ref<B_page>)e[r-1].p;
        int bm = b->m; 
        if (key == e[r].key) { 
            e[r].key = rem.key;
        }
        if (bm < n) { // reallocation
            int i = (n - bm + 1) >> 1;
            memcpy(&modify(a)->e[n+1-i], &b->e[2*n-i], i*sizeof(item));
            memmove(&modify(b)->e[bm+i], &b->e[bm], (2*n-bm-i)*sizeof(item));
            memset(&modify(b)->e[bm], 0, i*sizeof(item));
            e[r-1].key = b->e[2*n-1].key;
            modify(b)->m += i;
            modify(a)->m -= i;
            return propagation;
        } else { // merge page b to a
            internal_assert(bm == n); 
            memcpy(&modify(a)->e[1], &b->e[n], n*sizeof(item));
            memset(&modify(b)->e[n], 0, n*sizeof(item));
            e[r-1].p = NULL; 
            memmove(&e[m+1], &e[m], (r-1-m)*sizeof(item));
            memset(&e[m], 0, sizeof(item));
            modify(a)->m = 1;
            // dismiss page 'b'
            return ++m > n ? underflow : propagation;
        }
    }
}

void B_page::dump(int level, int height, int4 n) const
{
    static const char tabs[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
    char buf[8];
    union { 
        char   str[8];
        skey_t val;
    } key;
    int i;
    
    console::output(&tabs[(sizeof tabs) - 1 - level]);
    for (i = m; i < 2*n; i++) { 
        key.val = e[i].key;
        if (key.val != MAX_KEY) {
            if (sizeof(skey_t) == 4) { 
                unpack4(buf, key.str);
            } else { 
                unpack8(buf, key.str);
            }
            console::output("%.*s ", sizeof(skey_t), buf);
        } 
    }
    console::output("\n");
    if (++level < height) { 
        for (i = m; i < 2*n; i++) { 
            ref<B_page> page = (ref<B_page>)e[i].p;
            page->dump(level, height, n);
        }
    }
}

void B_page::ok(skey_t min_key_val, skey_t max_key_val, 
                int level, int height, int4 n) const
{
    int i;
    assert(level == 0 || m <= n);

    if (++level == height) { // leaf page
        assert(e[m].key >= min_key_val);
        assert(e[2*n-1].key == max_key_val);

        for (i = m; i < 2*n-1; i++) { 
            ref<set_member> mbr = (ref<set_member>)e[i].p;
            if (mbr.is_nil()) { 
                assert(i == 2*n-1 && e[2*n-1].key == MAX_KEY);
                break;
            }
            while (mbr->get_key() == e[i].key) {
                mbr = mbr->next; 
                if (mbr.is_nil()) { 
                    assert(i >= 2*n-2 && e[2*n-1].key == MAX_KEY);
                    break;
                }
                assert(mbr->prev->compare(mbr) <= 0);
            }
            assert((i == 2*n-1 && mbr.is_nil()) 
                   || (i < 2*n-1 && mbr == e[i+1].p));
            if (!mbr.is_nil()) { 
                assert(mbr->get_key() == e[i+1].key);
                assert(e[i].key < e[i+1].key);
            }
        }
    } else { 
        for (i = m; i < 2*n; i++) { 
            ref<B_page> p = (ref<B_page>)e[i].p;
            assert(e[i].key > min_key_val 
                   || (i == 0 && e[i].key == min_key_val));
            p->ok(min_key_val, e[i].key, level, height, n);
            min_key_val = e[i].key;
        }
        assert(min_key_val == max_key_val);
    }
}

field_descriptor& B_page::describe_components()
{
    return FIELD(m), VARYING(e); 
}


#ifndef USER_REGISTER
//[MC] We replaced pessimistic_repeatable_read_scheme with pessimistic_concurrent_read_scheme
//to allow more concurrency and avoid deadlocks
REGISTER_EX(B_tree, set_owner, pessimistic_concurrent_read_scheme, class_descriptor::cls_hierarchy_super_root | class_descriptor::cls_non_relational);
#if 0
REGISTER_EX(B_page, object, hierarchical_access_scheme,
            class_descriptor::cls_aligned);
#else
REGISTER_EX(B_page, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);
#endif
//[MC] We replaced pessimistic_repeatable_read_scheme with pessimistic_concurrent_read_scheme
//to allow more concurrency and avoid deadlocks
REGISTER_TEMPLATE_EX(SB_tree16, set_owner, pessimistic_concurrent_read_scheme, class_descriptor::cls_non_relational);
REGISTER_TEMPLATE_EX(SB_page16, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);
REGISTER_TEMPLATE_EX(SB_tree32, set_owner, pessimistic_concurrent_read_scheme, class_descriptor::cls_non_relational);
REGISTER_TEMPLATE_EX(SB_page32, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);
#endif

//
// Dictionary
//

#ifndef USER_REGISTER
REGISTER_ABSTRACT_EX(dictionary, object, pessimistic_repeatable_read_scheme, class_descriptor::cls_hierarchy_super_root);
#endif

field_descriptor& dictionary::describe_components()
{
	return NO_FIELDS;
}


//
// Hash table
//


field_descriptor& hash_item::describe_components()
{
    return FIELD(next), FIELD(obj), VARYING(name); 
}

void hash_table::put(const char* name, anyref obj)
{
    unsigned h = string_hash_function(name) % size;
    table[h] = (ref<hash_item>)hash_item::create(obj, (ref<hash_item>)table[h], name);
    n_elems += 1;
}

anyref hash_table::get(const char* name) const
{
    ref<hash_item> ip = (ref<hash_item>)table[string_hash_function(name) % size];
    while (ip != NULL) { 
        if (ip->compare(name) == 0) { 
            return ip->obj;
        }
        ip = ip->next;
    }
    return NULL;
}

boolean hash_table::del(const char* name)
{
    unsigned h = string_hash_function(name) % size;
    ref<hash_item> curr = (ref<hash_item>)table[h], prev = NULL;
    while (curr != NULL) { 
        if (curr->compare(name) == 0) { 
            if (prev == NULL) { 
                table[h] = curr->next;
            } else { 
                modify(prev)->next = curr->next;
            }
            n_elems -= 1;
            return True;
        }
        prev = curr;
        curr = curr->next;
    }
    return False;    
}

boolean hash_table::del(const char* name, anyref obj)
{
    unsigned h = string_hash_function(name) % size;
    ref<hash_item> curr = (ref<hash_item>)table[h], prev = NULL;
    while (curr != NULL) { 
        if (curr->obj == obj && curr->compare(name) == 0) { 
            if (prev == NULL) { 
                table[h] = curr->next;
            } else { 
                modify(prev)->next = curr->next;
            }
            n_elems -= 1;
            return True;
        }
        prev = curr;
        curr = curr->next;
    }
    return False;    
}

anyref hash_table::apply(hash_item::item_function f) const
{
    for (int i = size; --i >= 0;) { 
        for (ref<hash_item> ip = (ref<hash_item>)table[i]; ip != NULL; ip = ip->next) { 
            if (ip->apply(f)) { 
                return ip->obj;
            }
        }
    }
    return NULL;
}

field_descriptor& hash_table::describe_components()
{
    return FIELD(size), FIELD(n_elems), VARYING(table); 
}

#ifndef USER_REGISTER
//[MC] We replaced pessimistic_repeatable_read_scheme with pessimistic_concurrent_read_scheme
//to allow more concurrency and avoid deadlocks
REGISTER_EX(hash_table, dictionary, pessimistic_repeatable_read_scheme, class_descriptor::cls_non_relational);
REGISTER_EX(hash_item, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);
#endif

//
// Hash tree implementation
//
anyref H_page::apply(hash_item::item_function f, int height) const 
{ 
    if (--height == 0){ 
        for (int i = 0; i < (1 << size_log); i++) {
            for (ref<hash_item> ip = (ref<hash_item>)p[i]; ip != NULL; ip = ip->next) { 
                if (ip->apply(f)) { 
                    return ip->obj;
                }
            }
        }
    } else { 
        for (int i = 0; i < (1 << size_log); i++) {
            ref<H_page> pg = (ref<H_page>)p[i];
            if (pg != NULL) { 
                anyref obj = pg->apply(f, height);
                if (!obj.is_nil()) { 
                    return obj;
                }
            }
        }
    }
    return NULL;
}

void H_page::reset(int height)
{
    if (--height != 0) { 
        for (int i = 0; i < (1 << size_log); i++) {
            ref<H_page> pg = (ref<H_page>)p[i];
            if (pg != NULL) { 
                modify(pg)->reset(height);
            }
        }
    }
    for (int i = 0; i < (1 << size_log); i++) {
        p[i] = NULL;
    }
}

field_descriptor& H_page::describe_components()
{
    return ARRAY(p); 
}

#ifndef USER_REGISTER
REGISTER_EX(H_page, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);
#endif

void H_tree::put(const char* name, anyref obj)
{
    unsigned h = string_hash_function(name) % size;
    int i, j;
    ref<H_page> pg = root;
    if (pg.is_nil()) { 
        pg = NEW H_page;
        root = pg;
    }
    for (i = height; --i > 0; ) { 
        j = (h >> (i*H_page::size_log)) & ((1 << H_page::size_log) - 1);
        ref<H_page> child = (ref<H_page>)pg->p[j];
        if (child == NULL) {
            child = NEW H_page;
            modify(pg)->p[j] = child;
        }
        pg = child;
    }
    i = h & ((1 << H_page::size_log) - 1);
    modify(pg)->p[i] = (ref<hash_item>)hash_item::create(obj, (ref<hash_item>)pg->p[i], name);
    n_elems += 1;
}

anyref H_tree::get(const char* name) const
{
    unsigned h = string_hash_function(name) % size;
    int i, j;
    ref<H_page> pg = root;
    for (i = height; --i > 0; ) { 
        if (pg.is_nil()) return NULL;
        j = (h >> (i*H_page::size_log)) & ((1 << H_page::size_log) - 1);
        pg = pg->p[j];
    }
    if (pg.is_nil()) return NULL;
    i = h & ((1 << H_page::size_log) - 1);
    ref<hash_item> ip = (ref<hash_item>)pg->p[i];
    while (ip != NULL) { 
        if (ip->compare(name) == 0) { 
            return ip->obj;
        }
        ip = ip->next;
    }
    return NULL;
}

boolean H_tree::del(const char* name)
{
    unsigned h = string_hash_function(name) % size;
    int i, j;
    ref<H_page> pg = root;
    for (i = height; --i > 0; ) { 
        if (pg.is_nil()) return False;
        j = (h >> (i*H_page::size_log)) & ((1 << H_page::size_log) - 1);
        pg = pg->p[j];
    }
    if (pg.is_nil()) return False;
    i = h & ((1 << H_page::size_log) - 1);
    ref<hash_item> curr = (ref<hash_item>)pg->p[i], prev = NULL;
    while (curr != NULL) { 
        if (curr->compare(name) == 0) { 
            if (prev == NULL) { 
                modify(pg)->p[i] = curr->next;
            } else { 
                modify(prev)->next = curr->next;
            }
            n_elems -= 1;
            return True;
        }
        prev = curr;
        curr = curr->next;
    }
    return False;    
}

boolean H_tree::del(const char* name, anyref obj)
{
    unsigned h = string_hash_function(name) % size;
    int i, j;
    ref<H_page> pg = root;
    for (i = height; --i > 0; ) { 
        if (pg.is_nil()) return False;
        j = (h >> (i*H_page::size_log)) & ((1 << H_page::size_log) - 1);
        pg = pg->p[j];
    }
    if (pg.is_nil()) return False;
    i = h & ((1 << H_page::size_log) - 1);
    ref<hash_item> curr = (ref<hash_item>)pg->p[i], prev = NULL;
    while (curr != NULL) { 
        if (curr->obj == obj && curr->compare(name) == 0) { 
            if (prev == NULL) { 
                modify(pg)->p[i] = curr->next;
            } else { 
                modify(prev)->next = curr->next;
            }
            n_elems -= 1;
            return True;
        }
        prev = curr;
        curr = curr->next;
    }
    return False;    
}

field_descriptor& H_tree::describe_components()
{
    return FIELD(size), FIELD(n_elems), FIELD(height), FIELD(root); 
}

H_tree::H_tree(size_t hash_size, class_descriptor& desc) : dictionary(desc)
{ 
    unsigned h = 1, pow2 = 1 << H_page::size_log;
    while (pow2 <= hash_size) { 
        pow2 <<= H_page::size_log;
        h += 1;
    }
    height = h;
    size = (nat4)hash_size;
    n_elems = 0;
}

#ifndef USER_REGISTER
REGISTER_EX(H_tree, dictionary, pessimistic_repeatable_read_scheme, class_descriptor::cls_non_relational);
#endif

//
//  Binary large object
//

int Blob::readahead = 1;

struct blob_sync { 
    boolean     cancel;
    Blob const* chain;
    semaphore   credit;
    event       done;
};
    
void task_proc Blob::load_next_blob_part(void* arg) 
{
    blob_sync& bs = *(blob_sync*)arg;
    ref<Blob> head = bs.chain; 
    ref<Blob> curr = bs.chain->next; 
    head->notify(); // we are ready to load next block
    do { 
        curr = curr->next;
        head->notify();
        if (bs.cancel) { 
            break;
        }    
        bs.credit.wait();
    } while (curr != NULL);

    bs.done.signal();
}

void Blob::play(void* arg) const
{ 
    if (next.is_nil()) { 
        handle(arg); // that is all
    } else { 
        blob_sync sync;
        sync.chain = this;
        sync.cancel = False;
        for (int credit = readahead; --credit >= 0; sync.credit.signal());

        task::create(load_next_blob_part, &sync, 
                     task::pri_high, task::min_stack); 
        wait(); // wait until thread started
        ref<Blob> curr = this;
        while (curr->handle(arg)) { 
            if (!curr->next.is_nil()) { 
                wait();
                sync.credit.signal();
                curr = curr->next; 
            } else { 
                sync.done.wait();
                return;
            }
        }
        if (!curr->next.is_nil()) { 
            sync.cancel = True;
            sync.credit.signal();
            wait();
        }
        sync.done.wait();
    }
}

boolean Blob::handle(void*) const 
{
    return False;
} 


field_descriptor& Blob::describe_components()
{
    return FIELD(next), FIELD(last), VARYING(data); 
}

field_descriptor& ExternalBlob::describe_components()
{
    return VARYING(data); 
}


#ifndef USER_REGISTER
REGISTER(Blob, object, optimistic_scheme);
REGISTER_EX(ExternalBlob, object, optimistic_scheme, class_descriptor::cls_non_relational);
#endif

field_descriptor& KD_tree::describe_components()
{
    return FIELD(root), FIELD(size), FIELD(height), FIELD(n_dims), FIELD(free_chain); 
}

field_descriptor& KD_tree::Node::describe_components()
{
    return FIELD(left), FIELD(right), FIELD(parent), FIELD(obj); 
}


void KD_tree::add(anyref obj)
{
    int level = 0;
    ref<Node> node; 
    ref<Node> parent; 
    int diff = 0;

    node = root;
    while (!node.is_nil()) { 
        parent = node;
        diff = compare(obj, node->obj, level % n_dims);
        if (diff <= 0) { 
            node = node->left;
        } else { 
            node = node->right;
        }
        level += 1;
    }
    if (free_chain.is_nil()) { 
        node = NEW KD_tree::Node();
    } else {
        node = free_chain;
        free_chain = node->left;
    }
    modify(node)->left = NULL;
    modify(node)->right = NULL;
    modify(node)->parent = parent;
    modify(node)->obj = obj;

    if (parent.is_nil()) { 
        root = node;
    } else { 
        if (diff <= 0) { 
            modify(parent)->left = node;
        } else { 
            modify(parent)->right = node;
        }
    }
    if (level >= height) { 
        height = level + 1;
    }
    size += 1;
}
    
bool KD_tree::remove(anyref obj)
{
    ref<Node> node; 
    ref<Node> parent; 
    int level = 0;
    int diff = 0;
    node = root;
    while (!node.is_nil()) { 
        if (node->obj == obj) { 
            if (node->left.is_nil() && node->right.is_nil()) { /* leaf */ 
                if (parent.is_nil()) { 
                    root = NULL;
                    height = 0;
                } else { 
                    if (diff <= 0) { 
                        modify(parent)->left = NULL;
                    } else { 
                        modify(parent)->right = NULL;
                    }
                }                         
            } else { 
                ref<Node> new_root;
                ref<Node> child = node; 
                do { 
                    if (!child->left.is_nil()) { 
                        child = child->left;
                    } else { 
                        ref<Node> next = child->right;
                        while (next.is_nil() && child != node) { 
                            next = child->parent;
                            obj = child->obj;
                            if (!new_root.is_nil()) { 
                                int new_level = level;
                                ref<Node> sibling = new_root;
                                do {
                                    parent = sibling;
                                    diff = compare(obj, sibling->obj, new_level % n_dims);
                                    if (diff <= 0) { 
                                        sibling = sibling->left;
                                    } else { 
                                        sibling = sibling->right;
                                    }
                                    new_level += 1;
                                } while (!sibling.is_nil());

                                if (new_level >= height) { 
                                    height = new_level + 1;
                                }
                            } else { 
                                new_root = child;
                            }
                            if (parent.is_nil()) { 
                                root = child;
                                height -= 1;
                            } else { 
                                if (diff <= 0) { 
                                    modify(parent)->left = child;
                                } else { 
                                    modify(parent)->right = child;
                                }
                            }
                            modify(child)->parent = parent;
                            modify(child)->left = NULL;
                            modify(child)->right = NULL;

                            if (next->right != child) { 
                                child = next;                                
                                next = next->right;
                            } else {
                                child = next;                                
                                next = NULL;
                            }
                        }
                        child = next;
                    }
                } while (!child.is_nil());
            }
            modify(node)->left = free_chain;
            free_chain = node;
            size -= 1;
            return true;
        }
        parent = node;
        diff = compare(obj, node->obj, level % n_dims);
        if (diff <= 0) { 
            node = node->left;
        } else {
            node = node->right;
        }
        level += 1;
    }
    return false;
}

KD_tree::Iterator::Iterator(ref<KD_tree> tree, anyref low, nat8 low_mask, anyref high, nat8 high_mask) 
{ 
    this->tree = tree;
    this->low = low;                    
    this->low_mask = low_mask;
    this->high = high;
    this->high_mask = high_mask;
    curr_node = tree->root;
    curr_level = 0;
    has_current = False;
    getMin();
}
              
void KD_tree::Iterator::getMin()
{
    ref<Node> node = curr_node;
    int level = curr_level;
    while (!node.is_nil()) { 
        level += 1;
        curr_node = node;       
        if (((low_mask >> ((level-1) % tree->n_dims)) & 1) == 0 || tree->compare(node->obj, low, (level-1) % tree->n_dims) >= 0) { 
            node = node->left;
        } else { 
            break;
        }
    } 
    curr_level = level-1;
}

nat8 KD_tree::iterate(anyref low, nat8 low_mask, anyref high, nat8 high_mask,
                     boolean (*func)(const anyref &mbr, void const* arg), void const* arg) const
{
    Iterator i(this, low, low_mask, high, high_mask);
    anyref obj;
    nat8 n;
    for (n = 0; !(obj = i.next()).is_nil() && func(obj, arg); n++);
    return n;
}

anyref KD_tree::Iterator::next()
{
    ref<Node> prev = curr_node;
    while (!curr_node.is_nil()) { 
        anyref obj = curr_node->obj;
        if (!has_current 
            && curr_node->right != prev
            && (low_mask == 0 || tree->greaterOrEquals(obj, low, low_mask))
            && (high_mask == 0 || tree->lessOrEquals(obj, high, high_mask)))
        {
            has_current = True;
            return obj;
        }
        if (!curr_node->right.is_nil()
            && curr_node->right != prev
            && (((high_mask >> (curr_level % tree->n_dims)) & 1) == 0 || tree->compare(obj, high, curr_level % tree->n_dims) < 0))
        {
            prev = curr_node;
            curr_node = curr_node->right; 
            curr_level += 1;
            getMin();
        } else { 
            prev = curr_node;
            curr_node = curr_node->parent;
            curr_level -= 1;
        }
    }
    return NULL;
}

bool KD_tree::lessOrEquals(anyref obj1, anyref obj2, nat8 mask) const
{
    for (int i = 0; i < n_dims; i++) {
        if ((mask >> i) & 1) { 
            if (compare(obj1, obj2, i) > 0) { 
                return false;
            }
        }
    }
    return true;
}
   
bool KD_tree::greaterOrEquals(anyref obj1, anyref obj2, nat8 mask) const
{
    for (int i = 0; i < n_dims; i++) {
        if ((mask >> i) & 1) { 
            if (compare(obj1, obj2, i) < 0) { 
                return false;
            }
        }
    }
    return true;
}
   
#ifndef USER_REGISTER
REGISTER_ABSTRACT_EX(KD_tree, object, pessimistic_repeatable_read_scheme, class_descriptor::cls_non_relational);
//[MC] -- change static member self_class to function to avoid race condition
REGISTER_NAME(KD_tree_Node, KD_tree::Node, object, hierarchical_access_scheme, class_descriptor::cls_non_relational);
#endif

//[MC] added to check for uncleared in-memory lists
void check_set_owner_memory_leaks(hnd_t hnd)
{
	if (hnd == nullptr)
	{
		return;
	}
	
	if (!IS_VALID_OBJECT(hnd->obj))
	{
		return;
	}

	if (auto set_owner_ptr = dynamic_cast<set_owner*>(hnd->obj))
	{
		const auto size = set_owner_ptr->n_members;
		if (size == 0)
		{
			return;
		}

		const auto current_reference_count = hnd->access - 1; // -1 because we are compensating for the current reference destruction
		assert((size < current_reference_count || hnd->opid > 0) && "Memory leak detected, in-memory list must be cleared! Use CInMemorySetOwner!");
	}
}

END_GOODS_NAMESPACE
