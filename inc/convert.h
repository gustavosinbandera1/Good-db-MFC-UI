// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
#ifndef __CONVERT_H__
#define __CONVERT_H__

#if defined(__FreeBSD__) && __FreeBSD__ < 5
#include <sys/param.h>
#define USE_HTON_NTOH
#elif defined(__linux__) || (defined(__FreeBSD__) && __FreeBSD__ >= 5)
//
// At Linux inline assembly declarations of ntohl, htonl... are available
//
#include <netinet/in.h>
#define USE_HTON_NTOH
#else
#if defined(_WIN32) && _M_IX86 >= 400 && !defined(__IBMCPP__)
#pragma warning(disable:4035) // disable "no return" warning
#pragma warning(disable:4251) // disable "needs to have dll-interface" warning
#pragma warning(disable:4275) // disable "used as base for dll-inte..." warning
BEGIN_GOODS_NAMESPACE
inline int swap_bytes_in_dword(int val) {
    __asm {
          mov eax, val
          bswap eax
    }
}
inline short swap_bytes_in_word(short val) {
    __asm {
          mov ax, val
          xchg al,ah
    }
}
END_GOODS_NAMESPACE
#pragma warning(default:4035)
#define ntohl(w) swap_bytes_in_dword(w)
#define htonl(w) swap_bytes_in_dword(w)
#define ntohs(w) swap_bytes_in_word(w)
#define htons(w) swap_bytes_in_word(w)

#define USE_HTON_NTOH
#endif
#endif


BEGIN_GOODS_NAMESPACE

inline char* pack2(char* dst, nat2 val) { 
    *dst++ = char(val >> 8);
    *dst++ = char(val);     
    return dst;
}

inline char* pack2(char* dst, char* src) { 
    return pack2(dst, *(nat2*)src); 
}

inline void pack2(nat2& val) { 
#if BYTE_ORDER != BIG_ENDIAN
#ifdef USE_HTON_NTOH
    val = htons(val);
#else
    pack2((char*)&val, val); 
#endif
#endif
}


inline char* pack4(char* dst, nat4 val) { 
    *dst++ = char(val >> 24);
    *dst++ = char(val >> 16);     
    *dst++ = char(val >> 8); 
    *dst++ = char(val);
    return dst;
}

inline char* pack4(char* dst, char* src) { 
    return pack4(dst, *(nat4*)src); 
}

inline void pack4(nat4& val) { 
#if BYTE_ORDER != BIG_ENDIAN
#ifdef USE_HTON_NTOH
    val = htonl(val);
#else
    pack4((char*)&val, val); 
#endif
#endif
}


inline char* pack8(char* dst, char* src) { 
#if BYTE_ORDER == BIG_ENDIAN
    return pack4( pack4(dst, src), src + 4);
#else
    return pack4( pack4(dst, src + 4), src);
#endif
}

inline void pack8(nat8& val) { 
    union {
        nat8 i8;
        char c8[8];
    } dst, src;
    src.i8 = val;
    pack8(dst.c8, src.c8);
    val = dst.i8;
}


inline char* packref(char* dst, nat2 sid, objref_t opid) {
#if PGSQL_ORM
	return pack8(dst, (char*)&opid);
#else
	return pack4(pack2(dst, sid), opid);
#endif
}

inline nat2 unpack2(char* src) { 
    nat1* s = (nat1*)src;
    return (s[0] << 8) + s[1]; 
}

inline char* unpack2(char* dst, char* src) { 
    *(nat2*)dst = unpack2(src);
    return src + 2;
}

inline void  unpack2(nat2& val) { 
#if BYTE_ORDER != BIG_ENDIAN
#ifdef USE_HTON_NTOH
    val = ntohs(val);
#else
    val = unpack2((char*)&val); 
#endif
#endif
}


inline nat4  unpack4(char* src) { 
    nat1* s = (nat1*)src;
    return (((((s[0] << 8) + s[1]) << 8) + s[2]) << 8) + s[3];
} 

inline char* unpack4(char* dst, char* src) { 
    *(nat4*)dst = unpack4(src);
    return src + 4;
}

inline void unpack4(nat4& val) { 
#if BYTE_ORDER != BIG_ENDIAN
#ifdef USE_HTON_NTOH
    val = ntohl(val);
#else
    val = unpack4((char*)&val); 
#endif
#endif
}

inline char* unpack8(char* dst, char* src) { 
#if BYTE_ORDER == BIG_ENDIAN
    *(nat4*)dst = unpack4(src);
    *((nat4*)dst+1) = unpack4(src+4);
#else
    *(nat4*)dst = unpack4(src+4);
    *((nat4*)dst+1) = unpack4(src);
#endif
    return src + 8;
}

inline void unpack8(nat8& val) { 
    union {
        nat8 i8;
        char c8[8];
    } dst, src;
    src.i8 = val;
    unpack8(dst.c8, src.c8);
    val = dst.i8;
}

inline char* unpackref(nat2& sid, objref_t& opid, char* src) {
#if PGSQL_ORM
	sid = 0;
	return unpack8((char*)&opid, src);
#else
	return unpack4((char*)&opid, unpack2((char*)&sid, src));
#endif
}

END_GOODS_NAMESPACE

#endif

