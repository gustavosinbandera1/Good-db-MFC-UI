// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< SUPPORT.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 10-Nov-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Miscellaneuse support classes and functions: buffers, memory allocators... 
//-------------------------------------------------------------------*--------*

#ifndef __SUPPORT_H__
#define __SUPPORT_H__

#include "goodsdlx.h"

BEGIN_GOODS_NAMESPACE

class GOODS_DLL_EXPORT l2elem {
  public:
    l2elem* next;
    l2elem* prev;

    void link_after(l2elem* after) {
        (next = after->next)->prev = this;
        (prev = after)->next = this;
    }
    void link_before(l2elem* before) {
        (prev = before->prev)->next = this;
        (next = before)->prev = this;
    }
    void unlink() {
        prev->next = next;
        next->prev = prev;
    }
    void prune() {
        next = prev = this;
    }
    boolean empty() const {
        return  next == this;
    };

    l2elem() {
        next = prev = this;
    }
};

//
// Attention ! All memory allocators below are not thread safe, so
// access to them should be synchronized by mutexes. 
//

#define INIT_DNM_BUFFER_SIZE 4096

class GOODS_DLL_EXPORT dnm_buffer { 
  protected:
    char*  ptr;
    size_t used;
    size_t allocated;
  public:
    char*  operator& () { return ptr; }

    char operator[] (size_t index) const {
        assert(index < used);
        return ptr[index];
    }
 
    char*  put(size_t size) { 
        if (size > allocated) { 
            delete[] ptr;
            ptr = NEW char[size];
            allocated = size;
        }
        used = size;
        return ptr;
    }
    char *getPointerAt (size_t index) {
        if (!ptr) return 0;
        if (index < 0) index = 0;
        if (index > used - 1) index = used - 1;
        return &ptr[index];
    }
    char*  append(const char *buffer, size_t size) { 
        char *tptr = append (size);
        if (!tptr) return NULL;
        memcpy(tptr, buffer, size);
        return tptr;
    } 

    char*  append(size_t size) { 
        size_t cur_size = used;
        used += size;
        if (used > allocated) { 
            size_t new_size = (used > allocated*2) ? used : allocated*2; 
            char* new_buf = NEW char[new_size];
            memcpy(new_buf, ptr, cur_size);
            delete[] ptr;
            ptr = new_buf;
            allocated = new_size;
        }
        return &ptr[cur_size];
    }

    void cut(size_t size) { 
        used -= size;
    }

    char*  allocateExtra(size_t size) { 
        size_t need_size = used + size;
        if (need_size > allocated) { 
            size_t new_size = (need_size > allocated * 2) ? need_size : allocated * 2; 
            char* new_buf = NEW char[new_size];
            memcpy(new_buf, ptr, used);
            delete[] ptr;
            ptr = new_buf;
            allocated = new_size;
        }
        return &ptr[used];
    }

    size_t size() const { 
        return used;
    }

    size_t allocated_size() const { 
        return allocated;
    }

    char *still_buffer() { 
        used = 0;
        allocated = INIT_DNM_BUFFER_SIZE;
        char *tptr = ptr;
        ptr = NEW char[INIT_DNM_BUFFER_SIZE];
        return tptr;
    }

    void truncate(size_t size) { 
        if (allocated > size) { 
            delete[] ptr;
            ptr = NEW char[size];
            allocated = size;
        }
    }

    dnm_buffer(size_t init_size = INIT_DNM_BUFFER_SIZE) { 
        used = 0;
        allocated = init_size;
        ptr = NEW char[init_size];
    }
    ~dnm_buffer() { 
        delete[] ptr;
    }
};

#define DEFAULT_DNM_POOL_QUANT 1024

class GOODS_DLL_EXPORT dnm_object_pool { 
  protected:
    struct pool_item { 
        pool_item* next;
        char       data[1];
    }; 
    struct pool_block { 
        pool_block* next; 
    }; 
    size_t      obj_size;
    size_t      obj_alignment;
    size_t      alloc_quantum;
    pool_block* block_chain;
    pool_item*  free_chain; 
    int         n_allocated_blocks;
    boolean     cleanup; //disable destruction of static object

  public:

    void*       alloc() { 
        pool_item* ip = free_chain; 
        if (ip == NULL) { 
            pool_block* block = (pool_block*)NEW char[sizeof(pool_block) 
                                                     + alloc_quantum*obj_size 
                                                     + obj_alignment-1];
            block->next = block_chain; 
            block_chain = block;
            pool_item* item = (pool_item*)
                DOALIGN(size_t(block)+sizeof(pool_block), obj_alignment);
            for (int i = (int)alloc_quantum; -- i >= 0;) {
                item->next = ip;
                ip = item;
                item = (pool_item*)((char*)item + obj_size);
            }
            n_allocated_blocks += 1;
        }
        free_chain = ip->next;
        return ip;
    }
    void        dealloc(void* addr) { 
        pool_item* item = (pool_item*)addr; 
        item->next = free_chain;
        free_chain = item;
    }

    void toggle_cleanup(boolean enabled) {
        cleanup = enabled;
    }

    dnm_object_pool(size_t  object_size, 
                    boolean cleanup = True, 
                    size_t  allocation_quantum = DEFAULT_DNM_POOL_QUANT,
                    size_t  object_alignment = 8) 
    { 
        this->cleanup = cleanup;
        free_chain = NULL;
        block_chain = NULL;
        obj_size = object_size;
        obj_alignment = object_alignment;
        alloc_quantum = allocation_quantum;
        n_allocated_blocks = 0;
    }
    void reset() { 
        pool_block* bp = block_chain;
        while (bp != NULL) {
            pool_block* next = bp->next;
            delete[] bp;
            bp = next;
        }
        block_chain = NULL;
        free_chain = NULL;
        n_allocated_blocks = 0;
    }
        
    size_t allocated_size() const { 
        return n_allocated_blocks
            * (sizeof(pool_block) + alloc_quantum*obj_size + obj_alignment-1);
    }

    ~dnm_object_pool() { 
        if (cleanup) { 
            reset(); 
        }
    }
};

#define SMALL_ARRAY_SIZE 64

template<class T>
class GOODS_DLL_EXPORT dnm_array { 
  protected:
    T*     ptr; 
    size_t allocated;
    size_t used;
    T      buf[SMALL_ARRAY_SIZE];

    inline void reallocate(size_t new_size) {
        if (new_size > allocated) {
            size_t new_allocated = (allocated*2 > new_size) 
                ? allocated*2 : new_size;
            T* new_ptr = NEW T[new_allocated];
            T* old_ptr = ptr;
            for (size_t i = 0, n = used; i < n; i++) { 
                new_ptr[i] = old_ptr[i];
            }
            if (old_ptr != buf) { 
                delete[] old_ptr;
            }
            ptr = new_ptr;
            allocated = new_allocated;
        }
    }

  public:
    T* operator&() const { return ptr; }

    size_t size() const { 
        return used; 
    }
    size_t allocated_size() const { 
        return allocated;
    } 
    void  change_size(size_t size) { 
        reallocate(size);
        used = size;
    }

	T const& operator[] (size_t i) const {
		return ptr[i];
	}

	T& operator[] (size_t i) {
		reallocate(i+1);
		if (size_t(i) >= used) {
			memset(&ptr[used], 0, (i + 1 - used)*sizeof(T));
			used = i + 1;
		}
		return ptr[i];
	}

    void clear() { 
        for (size_t i = 0; i < used; i++) { 
            ptr[i] = (T)0;
        }
        used = 0;
    }

    void push(T val) { 
        reallocate(used+1);
        ptr[used++] = val;
    }
    T pop() { 
        assert(used > 0); 
        return ptr[--used]; 
    }
    boolean empty() { 
        return used == 0;
    }

    dnm_array(dnm_array<T> const& other) { 
        allocated = itemsof(buf);
        ptr = buf;
        change_size(other.used);
        for (size_t i = 0; i < used; i++) { 
            ptr[i] = other[i];
        }
    }

    dnm_array() { 
        used = 0;
        allocated = itemsof(buf);
        ptr = buf;
    }
    dnm_array(size_t size) { 
        used = 0;
        allocated = itemsof(buf);
        ptr = buf;
        change_size(size);
    }
    ~dnm_array() { 
        if (ptr != buf) { 
            delete[] ptr;
        }
    }
};

typedef dnm_array<char> dnm_string;


#define DEFAULT_STACK_BLOCK_SIZE 4096

template<class T>
class GOODS_DLL_EXPORT dnm_stack { 
  protected:
    struct stack_block { 
        stack_block* next;
        stack_block* prev;

        stack_block(stack_block* chain) : next(NULL), prev(chain) {}
        void* operator new(size_t hdr_size, size_t body_size) { 
            return ::operator new(hdr_size + body_size);
        }
    };
    const size_t block_size;
    stack_block* chain;
    stack_block* top;
    size_t       sp;
    T*           bp;
    int          n_blocks;

  public: 
    void push(T val) { 
        if (sp == block_size) { 
            n_blocks += 1;
            if (top->next != NULL) { 
                top = top->next;
            } else {
                top = top->next = new (block_size*sizeof(T)) stack_block(top);
            }
            sp = 0;
            bp = (T*)(top+1);
        }            
        bp[sp++] = val;
    }
    T pop() { 
        if (sp == 0) { 
            assert (top->prev != NULL); 
            n_blocks -= 1;
            top = top->prev;
            bp = (T*)(top+1);
            sp = block_size;
        }
        return bp[--sp];
    }
    boolean is_empty() const { return sp == 0 && top == chain; }

    void make_empty() { 
        top = chain;
        bp = (T*)(top+1);
        sp = 0;
        n_blocks = 0;
    }
    size_t size() const { 
        return n_blocks*block_size + sp;
    }
    dnm_stack(size_t stack_block_size = 4096) : block_size(stack_block_size) {
        chain = new (stack_block_size*sizeof(T)) stack_block(NULL);
        make_empty();
    }
    ~dnm_stack() { 
        stack_block* bp = chain;
        while (bp != NULL) { 
            stack_block* next = bp->next;
            delete bp;
            bp = next;
        }
    }
};


#define DEFAULT_QUEUE_BLOCK_SIZE 4096

template<class T>
class GOODS_DLL_EXPORT dnm_queue { 
  protected:
    struct queue_block { 
        queue_block* next;

        queue_block() { next = NULL; }
        void* operator new(size_t hdr_size, size_t body_size) { 
            return ::operator new(hdr_size + body_size);
        }
		void operator delete(void* p) {
			::operator delete(p);
		}
    };
    const int    block_size;
    queue_block* tail;
    queue_block* head;
    int          tail_pos;
    int          head_pos;
    int          n_items;

  public: 
    void put(T val) { 
        n_items += 1;
        if (head_pos == block_size) { 
            if (head->next != NULL) { 
                head = head->next;
            } else {
                head = head->next = new (block_size*sizeof(T)) queue_block;
            }
            head_pos = 0;
        }            
        ((T*)(head+1))[head_pos++] = val;
    }

    T get() { 
        n_items -= 1;
        if (tail_pos == block_size) { 
            assert (tail->next != NULL); 
            queue_block* next = tail->next;
            tail->next = head->next;
            head->next = tail;
            tail = next;
            tail_pos = 0;
        }
        return ((T*)(tail+1))[tail_pos++];
    }

    boolean is_empty() const { return n_items == 0; }

    void make_empty() { 
        head = tail;
        head_pos = tail_pos = 0;
        n_items = 0;
    }

    size_t size() const { return n_items; }

    dnm_queue(size_t queue_block_size = 4096) : block_size(int(queue_block_size)) {
        tail = new (queue_block_size*sizeof(T)) queue_block;
        make_empty();
    }
    ~dnm_queue() { 
        queue_block* bp = tail;
        while (bp != NULL) { 
            queue_block* next = bp->next;
            delete bp;
            bp = next;
        }
    }
};

inline unsigned asci_string_hash_function(const char* name)
{
    unsigned h = 0;
    char ch;
    while((ch = *name++) != 0) { 
        h = 31*h + ch;
    }
    return h;
}

inline unsigned string_hash_function(const char* name)
{ 
    unsigned h = 0, g;
    while(*name) { 
        h = (h << 4) + *name++;
        if ((g = h & 0xF0000000) != 0) { 
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

//
// Compare strings with ingnoring case 
//

#ifndef __STRICMP_DEFINED__
inline int stricmp(const char* p, const char* q)
{
    while (toupper(*(unsigned char*)p) == toupper(*(unsigned char*)q)) { 
        if (*p == '\0') { 
            return 0;
        }
        p += 1;
        q += 1;
    }
    return toupper(*(unsigned char*)p) - toupper(*(unsigned char*)q);
}
#endif

inline int strincmp(const char* p, const char* q, size_t n)
{
    while (n > 0) { 
        int diff = toupper(*(unsigned char*)p) - toupper(*(unsigned char*)q);
        if (diff != 0) { 
            return diff;
        } else if (*p == '\0') { 
            return 0;
        }
        p += 1;
        q += 1;
        n -= 1; 
    }
    return 0;
}

//
// Find one string within another, ignoring case
//

inline char* stristr(char* haystack, const char* needle)
{
    size_t i, hayLen, ndlLen;

    ndlLen = strlen(needle);
    hayLen = strlen(haystack);

    if (ndlLen > hayLen) {
        return NULL;
    }

    for (i = 0; i <= (hayLen - ndlLen); i++) {
        if (strincmp(&haystack[i], needle, ndlLen) == 0) {
            return &haystack[i];
        }
    }

    return NULL;
}

END_GOODS_NAMESPACE

#endif
