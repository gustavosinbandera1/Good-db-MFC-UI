// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< EXCEPTION.H >---------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     17-Oct-99    K.A. Knizhnik  * / [] \ *
//                          Last update: 17-Oct-99    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Base class for all exception which can be raise during database session
// When such exeption is raised and not handle within methods of
// persistent capable object, current transaction is aborted.
//-------------------------------------------------------------------*--------*

#ifndef __DBEXCEPT_H__
#define __DBEXCEPT_H__


BEGIN_GOODS_NAMESPACE

class dbException { 
  public:
    dbException();
    ~dbException();
};

#ifdef GOODS_SUPPORT_EXCEPTIONS
class QueryException : public dbException
#else
class QueryException
#endif
{
  public:
    QueryException(char const* err) : msg(err) {}
    const char* const msg;
};

#ifdef GOODS_SUPPORT_EXCEPTIONS
class DeadlockException : public dbException
#else
class DeadlockException
#endif
{
};


END_GOODS_NAMESPACE

#endif
