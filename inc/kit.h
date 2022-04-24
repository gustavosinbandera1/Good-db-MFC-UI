/* ------------------------------------------------------------------------- */
/*                                                                           */
/* FILE: kit.h                                                               */
/* DATE: 22/12/2006                                                          */
/* AUTHOR: Piergiorgio Beruto                                                */
/* DESCRIPTION: template library to perform linear database searches         */
/* building queries over generic class data members.                         */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#ifndef __KIT_H__
#define __KIT_H__

#include "goods.h"
#include "dbscls.h"

#include <functional>

namespace kit
{

/* ------------------------------------------------------------------------- */

// foreward declaration
template<class X> class query;


/* ------------------------------------------------------------------------- */

template<class X>
class query_base
{
public:
   
   virtual ~query_base<X>()                     {}

   query<X> operator && (const query_base<X> &qb) const;
   query<X> operator || (const query_base<X> &qb) const;
   query<X> operator ^  (const query_base<X> &qb) const;
   query<X> operator !  () const;

   virtual query_base<X>* clone() const = 0;
   virtual bool operator() (const X &obj) const = 0;
};


/* ------------------------------------------------------------------------- */

template<class X>
class query : public query_base<X>
{
public:
   query()
   : left(0), right(0), op(enTRUE)              {}

   ~query()
   {
      if(left != 0)
         delete left;

      if(right != 0)
         delete right;
   }

   query(const query_base<X>& qb)
   : left(0), right(0)
   { 
      left = qb.clone();
      op = enID;
   }

   query(const query<X>& qb)
   : left(0), right(0), op(qb.op)
   {
      // copying a query object
      if(qb.left != 0)
         left = qb.left->clone();

      if(qb.right != 0)
         right = qb.right->clone();
   }

   query_base<X>& operator = (const query_base<X> &qb)
   {
      if(left != 0)
         delete left;

      if(right != 0)
         delete right;

      // copying a generic object
      left = qb.clone();
      right = 0;
      op = enID;
      
      return *this;
   }

   query<X>& operator = (const query<X> &qb)
   {
      if(left != 0)
         delete left;

      if(right != 0)
         delete right;

      left = 0;
      right = 0;
      
      // copying a query object
      if(qb.left != 0)
         left = qb.left->clone();

      if(qb.right != 0)
         right = qb.right->clone();
         
      op = qb.op;

      return *this;
   }
   
protected:
   
   query_base<X>* clone() const
      { return new query<X>(*this); }

   bool operator()(const X &obj) const
   {
      switch(op)
      {
      case enTRUE:
         return true;

      case enID:
         return (*left)(obj);
         
      case enNOT:
         return !(*left)(obj);
         
      case enAND:
         return (*left)(obj) && (*right)(obj);
         
      case enOR:
         return (*left)(obj) || (*right)(obj);
      }
      return true;
   }

private:

   enum _opType
   {
      enTRUE,
      enID,
      enNOT,
      enAND,
      enOR,
   };

   query<X>(query_base<X> *_left, query_base<X> *_right, _opType _op)
   : left(_left), right(_right), op(_op)      {}

   query_base<X> *left, *right;
   _opType op;

   friend class query_base<X>;
};


/* ------------------------------------------------------------------------- */

template<class X>
query<X> query_base<X>::operator && (const query_base<X> &qb) const
{
   return query<X>(clone(), qb.clone(), query<X>::enAND);
}


/* ------------------------------------------------------------------------- */

template<class X>
query<X> query_base<X>::operator || (const query_base<X> &qb) const
{
   return query<X>(clone(), qb.clone(), query<X>::enOR);
}


/* ------------------------------------------------------------------------- */

template<class X>
query<X> query_base<X>::operator ^ (const query_base<X> &qb) const
{
   return (*this || qb) && !(*this && qb);
}


/* ------------------------------------------------------------------------- */

template<class X>
query<X> query_base<X>::operator ! () const
{
   return query<X>(clone(), 0, query<X>::enNOT);
}


/* ------------------------------------------------------------------------- */

template<class X, class T, class Comp = std::less<T> >
class qless : public query_base<X>
{
public:
   qless(T X::*what, const T &than)
   : left(what), right(than)                    {}
   
protected:
   query_base<X>* clone() const
      { return new qless<X, T, Comp>(*this); }

   bool operator()(const X &obj) const
      { return cmp(obj.*left, right); }

private:
   T X::*left;
   T right;
   Comp cmp;
};


/* ------------------------------------------------------------------------- */

template<class X, class T, class Comp = std::greater<T> >
class qgreat : public query_base<X>
{
public:
   qgreat(T X::*what, const T &than)
   : left(what), right(than)                    {}
   
protected:
   query_base<X>* clone() const
      { return new qgreat<X, T, Comp>(*this); }

   bool operator()(const X &obj) const
      { return cmp(obj.*left, right); }

private:
   T X::*left;
   T right;
   Comp cmp;
};


/* ------------------------------------------------------------------------- */

template<class X, class T, class Comp = std::equal_to<T> >
class qequal : public query_base<X>
{
public:
   qequal(T X::*what, const T &than)
   : left(what), right(than)                    {}
   
protected:
   query_base<X>* clone() const
      { return new qequal<X, T, Comp>(*this); }

   bool operator()(const X &obj) const
      { return cmp(obj.*left, right); }

private:
   T X::*left;
   T right;
   Comp cmp;
};


/* ------------------------------------------------------------------------- */

template<class X>
class const_iterator
{
public:
   const_iterator(const X* _start, const query_base<X> &_cr = query<X>())
   : curr(_start), match(_cr.clone())
   {
      if(!(*match)(*curr))
         ++(*this);
   }

   const_iterator(const const_iterator<X> &it)
   : match(it.match->clone()), curr(it.curr)                       {}

   ~const_iterator()
   {
      delete match;
   }

   const_iterator<X>& operator++()
   {
      for(curr = (X*) curr->getNext(); curr != 0;
          curr = (X*) curr->getNext())
         if((*match)(*curr)) break;

      return *this;
   }

   const_iterator<X> operator++(int)
   {
      const_iterator<X> it(*this);
      ++(*this);

      return it;
   }

   const_iterator<X>& operator = (const const_iterator<X> &it)
   {
      delete match;
      match = it.match->clone();

      curr = it.curr;
      return *this;
   }

   inline const X* operator->() const
      { return curr; }

   inline const X& operator*() const
      { return *curr; }

   inline const X* get() const
      { return curr; }

   inline operator bool() const
      { return curr != 0; }

private:
   const X* curr;
   const query_base<X> *match;
};


/* ------------------------------------------------------------------------- */

template<class X>
class iterator
{
public:
   iterator(X* _start, const query_base<X> &_cr = query<X>())
   : curr(_start), match(_cr.clone())
   {
      if(!(*match)(*curr))
         ++(*this);
   }

   iterator(const iterator<X> &it)
   : match(it.match->clone()), curr(it.curr)                       {}

   ~iterator()
   {
      delete match;
   }

   iterator<X>& operator++()
   {
      for(curr = curr->getNext(); curr != 0; curr = curr->getNext())
         if((*match)(*curr)) break;

      return *this;
   }

   iterator<X> operator++(int)
   {
      iterator<X> it(*this);
      ++(*this);

      return it;
   }

   iterator<X>& operator = (const const_iterator<X> &it)
   {
      delete match;
      match = it.match->clone();

      curr = it.curr;
      return *this;
   }

   inline X* operator->()
      { return curr; }

   inline X& operator*()
      { return *curr; }

   inline X* get()
      { return curr; }

   inline operator bool() const
      { return curr != 0; }

private:
   X* curr;
   const query_base<X> *match;
};


/* ------------------------------------------------------------------------- */

} // namespace kit



BEGIN_GOODS_NAMESPACE

// specialization of iterators to work on goods refs instead of normal pointers

template<class X>
class const_iterator_ref
{
public:
   const_iterator_ref()
   : match(new kit::query<X>())  {}

   const_iterator_ref(const ref<X> _start,
                      const kit::query_base<X> &_cr = kit::query<X>())
   : curr(_start), match(_cr.clone())
   {
      if(!curr.is_nil())
      {
         pin = curr;

         if(!(*match)(*pin))
          ++(*this);
      }
   }

   const_iterator_ref(const const_iterator_ref<X> &it)
   : match(it.match->clone()), curr(it.curr), pin(it.pin)                  {}

   virtual ~const_iterator_ref()
   {
      delete match;
   }

   const_iterator_ref<X>& operator++()
   {
      for(curr = curr->getNext(); !curr.is_nil();
          curr = curr->getNext())
      {
        pin = curr;
         if((*match)(*pin)) break;
      }

      return *this;
   }

   const_iterator_ref<X> operator++(int)
   {
      const_iterator_ref<X> it(*this);
      ++(*this);

      return it;
   }

   const_iterator_ref<X>& operator = (const const_iterator_ref<X> &it)
   {
      delete match;
      match = it.match->clone();

      curr = it.curr;
	   
      if(!curr.is_nil())
        pin = curr;

      return *this;
   }

   inline const X* operator->() const
      { return pin.get(); }

   inline const X& operator*() const
      { return *pin.get(); }

   inline const ref<X> get() const
      { return curr; }

   inline const ref<X> release() const
   {
      pin = NULL;
      return curr;
   }

   inline const X* getObj() const
      { return pin.get(); }

   inline operator bool() const
      { return !curr.is_nil(); }

private:
   mutable ref<X> curr;
   mutable r_ref<X> pin;

   const kit::query_base<X> *match;
};


/* ------------------------------------------------------------------------- */

template<class X>
class iterator_ref
{
public:
   iterator_ref()
   : ppin_w(0), match(new kit::query<X>())  {}

   iterator_ref(ref<X> _start, const kit::query_base<X> &_cr = kit::query<X>())
   : curr(_start), ppin_w(0), match(_cr.clone())
   {
      if(!curr.is_nil())
      {
         r_ref<X> pin_r(curr);

         if(!(*match)(*pin_r))
          ++(*this);
      }
   }

   iterator_ref(const iterator_ref<X> &it)
   : match(it.match->clone()), curr(it.curr)
   {
      ppin_w = (it.ppin_w != 0 ? new w_ref<X>(*it.ppin_w) : 0);
   }

   virtual ~iterator_ref()
   {
      release();
      delete match;
   }

   iterator_ref<X>& operator++()
   {
      release();

      for(curr = curr->getNext(); !curr.is_nil();
          curr = curr->getNext())
      {
         r_ref<X> pin(curr);
         if((*match)(*pin)) break;
      }

      return *this;
   }

   iterator_ref<X> operator++(int)
   {
      iterator_ref<X> it(*this);
      ++(*this);

      return it;
   }

   iterator_ref<X>& operator = (const iterator_ref<X> &it)
   {
      release();

      delete match;
      match = it.match->clone();

      curr = it.curr;

      if(it.ppin_w != 0)
         ppin_w = new w_ref<X>(curr);

      return *this;
   }

   inline X* operator->()
   {
      lock();
      return ppin_w->get();
   }

   inline X& operator*()
   {
      lock();
      return *ppin_w->get();
   }

   inline ref<X> get()
      { return curr; }

   inline ref<X> release()
   {
      if(ppin_w != 0)
      {
         delete ppin_w;
         ppin_w = 0;
      }

      return curr;
   }

   inline X* getObj()
   {
      lock();
      return ppin_w->get();
   }

   inline operator bool() const
      { return !curr.is_nil(); }

private:

   inline void lock()
   {
      if(ppin_w != 0 || curr.is_nil())
         return;

      ppin_w = new w_ref<X>(curr);
   }

   ref<X> curr;
   w_ref<X> *ppin_w;

   const kit::query_base<X> *match;
};


/* ------------------------------------------------------------------------- */

// functor to compare goods String (case sensitive)
struct cmpString : public std::binary_function<ref<String>, ref<String>, bool>
{
   bool operator()(const ref<String> &s1, const ref<String> &s2) const
   { return s1->compare(s2) == 0; }
};

// functor to compare goods String (case insensitive)
struct cmpIString : public std::binary_function<ref<String>, ref<String>, bool>
{
   bool operator()(const ref<String> &s1, const ref<String> &s2) const
   { return s1->compareIgnoreCase(s2) == 0; }
};


/* ------------------------------------------------------------------------- */

END_GOODS_NAMESPACE

#endif
