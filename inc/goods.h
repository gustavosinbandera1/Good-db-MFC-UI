// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< GOODS.H >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update:  3-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Main header files. Collect all neccessary includes and definitions
//-------------------------------------------------------------------*--------*

#ifndef __GOODS_H__
#define __GOODS_H__

//[MC]
#if !defined(NDEBUG)
# define USE_DYNAMIC_TYPECAST 0
#endif
#define GOODS_RUNTIME_TYPE_CHECKING 1

#ifndef USE_DYNAMIC_TYPECAST
#define USE_DYNAMIC_TYPECAST 1
#endif

#if !USE_DYNAMIC_TYPECAST && !defined(GOODS_RUNTIME_TYPE_CHECKING)
#ifdef __GOODS_MFCDLL
#define GOODS_RUNTIME_TYPE_CHECKING 0
#else
#define GOODS_RUNTIME_TYPE_CHECKING 1
#endif
#endif


#include "goodsdlx.h"
#include "stdinc.h"

BEGIN_GOODS_NAMESPACE
class object_handle;
typedef object_handle* hnd_t; 
END_GOODS_NAMESPACE

#include "database.h"
#include "class.h"
#include "object.h"
#include "mop.h"
#include "refs.h"

#include "dbexcept.h"

#endif

