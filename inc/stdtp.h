// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< STDTP.H >-------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 23-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Standart type and macro definitions
//-------------------------------------------------------------------*--------*

#ifndef __STDTP_H__
#define __STDTP_H__

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#if defined(__APPLE__) && !defined(__FreeBSD__)
// MAC OS X is Free BSD
#define __FreeBSD__ 4
#endif

#if defined(__FreeBSD__) && __FreeBSD__ >= 5
#include <netinet/in.h>
#endif

#define GNUC_BEFORE(major,minor) (defined(__GNUC__) && (major > __GNUC__ || (major == __GNUC__ && minor > __GNUC_MINOR__)))
#define GNUC_SINCE(major,minor) (defined(__GNUC__) && (major < __GNUC__ || (major == __GNUC__ && minor <= __GNUC_MINOR__)))

//
// Do not used builtin C++ bool type because as far as sizeof(bool) == 1 
// concurrent access to neighboring boolean componets by different threads 
// will require locking
// 
#define boolean  int 
#ifndef True
#define True     1
#endif
#ifndef False
#define False    0
#endif

// Align value 'x' to boundary 'b' which should be power of 2
#define DOALIGN(x,b)   (((x) + (b) - 1) & ~((b) - 1))

BEGIN_GOODS_NAMESPACE

typedef signed char    int1;
typedef unsigned char  nat1;

typedef signed short   int2;
typedef unsigned short nat2;

#if defined(_WIN32)&& !defined(__IBMCPP__) && !defined(__MINGW32__)
#if defined(_MSC_VER) || _MSC_VER < 1300
typedef signed   int int4;
typedef unsigned int nat4;
#else
typedef signed   __int32 int4;
typedef unsigned __int32 nat4;
#endif
typedef unsigned __int64 nat8;
typedef signed   __int64 int8;
#define INT8_FORMAT "I64"
#define CONST64(c)  c
#else
#if defined(__osf__ )
typedef signed   int  int4;
typedef unsigned int  nat4;
typedef unsigned long nat8;
typedef signed   long int8;
#define INT8_FORMAT "l"
#define CONST64(c)  c##L
#else
#if defined(__GNUC__) || defined(__SUNPRO_CC) || defined(__IBMCPP__) || defined(__sgi) || defined(__hp9000s700)
typedef signed   int       int4;
typedef unsigned int       nat4;
typedef unsigned long long nat8;
typedef signed   long long int8;
#define INT8_FORMAT "ll"
#define CONST64(c)  c##LL
#else
#error "integer 8 byte type is not defined" 
#endif
#endif
#endif

#define nat8_low_part(x)  ((nat4)(x))
#define nat8_high_part(x) ((nat4)((nat8)(x)>>32))
#define int8_low_part(x)  ((int4)(x))
#define int8_high_part(x) ((int4)((int8)(x)>>32))
#define cons_nat8(hi, lo) ((((nat8)(hi)) << 32) | (nat4)(lo))
#define cons_int8(hi, lo) ((((int8)(hi)) << 32) | (nat4)(lo))
 
#define MAX_INT1 SCHAR_MAX
#define MAX_NAT1 UCHAR_MAX
#define MAX_INT2 SHRT_MAX
#define MAX_NAT2 USHRT_MAX
#define MAX_INT4 INT_MAX
#define MAX_NAT4 UINT_MAX
#define MAX_INT8 ((int8)(MAX_NAT8 >> 1))
#define MAX_NAT8 nat8(-1)

typedef float  real4;
typedef double real8; 

#ifndef BIG_ENDIAN
#define BIG_ENDIAN      4321    /* most-significant byte first (IBM, net) */
#endif
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234
#endif

#ifndef BYTE_ORDER
#if defined(__sparc__) || defined(__m68k__) || defined(__sparc) || defined(__sgi) || defined(__hp9000s700)
#define BYTE_ORDER      BIG_ENDIAN 
#else
#define BYTE_ORDER      LITTLE_ENDIAN
#endif
#endif

typedef char msg_buf[256]; 

#define   itemsof(array) (sizeof(array)/sizeof*(array))
#define   this_offsetof(c,f) ((char*)f - (char*)this)

END_GOODS_NAMESPACE

#endif




