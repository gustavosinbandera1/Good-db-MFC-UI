// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< CONFIG.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 25-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Definition of GOODS specific types and macros.
//-------------------------------------------------------------------*--------*

#ifndef __CONFIG_H__
#define __CONFIG_H__

BEGIN_GOODS_NAMESPACE

typedef nat2 stid_t; // store  identifier in database
typedef nat2 cpid_t; // class  persistent identifier in store
typedef nat4 cid_t;  // class  transient identifier in program
typedef nat4 opid_t; // object persistent indetifier in store

#if PGSQL_ORM

typedef nat8 objref_t; // cid + opid

#define MAKE_OBJREF(cid,oid) (((objref_t)(cid) << 48) | (oid))
#define GET_CID(oref)        ((cid_t)((oref) >> 48))
#define GET_OID(oref)        ((oref) & (((objref_t)1 << 48) - 1))
#else

typedef nat4 objref_t;

#define GET_OID(oref)        oref

#endif

typedef nat4 trid_t; // global transaction identifier

typedef nat8 page_timestamp_t; // page timestamp 

#ifdef _WIN32
typedef HANDLE descriptor_t; 
#else
typedef int descriptor_t; 
#endif

//
// This are different level of debugging
//
#define DEBUG_NONE  0
#define DEBUG_CHECK 1
#define DEBUG_TRACE 2

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_TRACE
#endif

//[MC] Enable detection of deadlocks
//#define DETECT_DEADLOCKS

//
// This assert statement is used to check system invariants which
// can't be violated by applicate (so correctness of system itself is checked)
//
#if DEBUG_LEVEL >= DEBUG_CHECK
#define internal_assert(cond) assert(cond)
//[MC] Use this internal_assert when we want to log some potential issues in the release version
//#define internal_assert(x) do if (!(x)) TRACE_MSG((msg_error, "%s:%d: Assertion " #x " is failed\n", __FILE__, __LINE__)); while (0)
#else
#define internal_assert(cond)
#endif

#ifdef DEBUG_ASSERT
//
// Sometimes it is more convenient to catch SIGSEGV instead of calling system 
// assert(). For example in Digital Unix debugger failed to unroll stack after
// assertion failure. And in Windows NT assertion failure will not cause
// invocation of debugger if program is not started from MSDEV. That is
// why this version of assert was implemented. 
//
#undef assert
#define assert(c) debug_assert(c, #c, __FILE__, __LINE__)
#define abort() (*(int*)0 = 0)

inline void debug_assert(int c, const char* cond, const char* file, int line) {
    if (!c) { 
        fprintf(stderr, "Assertion %s:%d: %s failed\n", file, line, cond);
        abort();
    }
}
#endif

#ifndef CHECK_LEVEL
//[MC] Debug level was changed to 2 by default
//#define CHECK_LEVEL 0 // no checking
//#define CHECK_LEVEL 1 // checking that can be efficiently performed without sufferring performance 
#define CHECK_LEVEL 2 // more sohisticated checking
//#define CHECK_LEVEL 3 // this check level can be used only temporary in debug mode to catch an error
#endif

#define MAX_CFG_FILE_LINE_SIZE 1024

END_GOODS_NAMESPACE

#endif



