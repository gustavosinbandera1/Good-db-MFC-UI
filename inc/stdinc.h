// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< STDINC.H >------------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      5-May-97    K.A. Knizhnik  * / [] \ *
//                          Last update:  9-May-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Database storage server
//-------------------------------------------------------------------*--------*

#ifndef __STDINC_H__
#define __STDINC_H__

#define GOODS_VERSION 3.20
//[MC]{
#define MURSIW_SUPPORT
#define PROHIBIT_UNSAFE_IMPLICIT_CASTS (0)
#define ENABLE_CONVERTIBLE_ASSIGNMENTS (0)
#define OLD_BACKUP_FORMAT_COMPATIBILITY (1)
//}[MC]

#if defined(_WIN32)

#ifndef _CRT_SECURE_NO_DEPRECATE
#	define _CRT_SECURE_NO_DEPRECATE
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
#	define _CRT_NONSTDC_NO_DEPRECATE
#endif 

#define __STRICMP_DEFINED__

#include <windows.h>
#ifndef __MINGW32__
#pragma warning(disable:4800 4355 4146 4291 4244 4996)
#endif
#endif

#ifdef CHECK_FOR_MEMORY_LEAKS
#ifdef _WIN32
#define NEW  new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define _CRTDBG_MAP_ALLOC 
#include <crtdbg.h>
#else
#define NEW new
#undef CHECK_FOR_MEMORY_LEAKS // not supported at other platforms yet
#endif
#else
#define NEW new
#endif

#ifdef USE_NAMESPACES
#define BEGIN_GOODS_NAMESPACE namespace goods {
#define END_GOODS_NAMESPACE }
#define USE_GOODS_NAMESPACE using namespace goods;
#define GOODS_NAMESPACE goods
#else
#define BEGIN_GOODS_NAMESPACE
#define END_GOODS_NAMESPACE
#define USE_GOODS_NAMESPACE 
#define GOODS_NAMESPACE
#endif

#include "stdtp.h"
#include "config.h"
#include "support.h"
#include "console.h"
#include "task.h"

#endif
