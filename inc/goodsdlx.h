// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
#ifndef _GOODS_DLL_EXPORT_H
#define _GOODS_DLL_EXPORT_H

#undef GOODS_DLL_EXPORT
#ifdef __GOODS_MFCDLL
#ifdef __GOODS
#define GOODS_DLL_EXPORT __declspec(dllexport)
#else
#define GOODS_DLL_EXPORT __declspec(dllimport)
#endif
#else
#define GOODS_DLL_EXPORT
#endif

#endif
