#pragma once

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define LIKELY(x)       __builtin_expect((x),1)
#define UNLIKELY(x)     __builtin_expect((x),0)
#define EXPECT(x, val)  __builtin_expect((x),val)
#else
#define LIKELY(x)       (x)
#define UNLIKELY(x)     (x)
#define EXPECT(x, val)  (x)
#endif

#ifdef __INTEL_COMPILER
#define __PRAGMA_NOUNROLL__ __pragma(nounroll)
#else
#define __PRAGMA_NOUNROLL__
#endif

#if defined(__GNUC__) || defined(__clang__)
#include <cstring>
#include <stdio.h>

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _snprintf snprintf
#endif

#ifndef _WIN32
#include <stdlib.h>
#define __forceinline inline
#ifndef __cdecl
#define __cdecl
#endif
#define InterlockedCompareExchangePointer(a,b,c) __sync_val_compare_and_swap(a,c,b)

static inline void* _aligned_malloc(size_t size, size_t alignment)
{
    void *tmp;
    if (posix_memalign(&tmp, alignment, size))
    {
        tmp = 0;
    }
    return tmp;
}
#define _aligned_free free
#endif

#ifndef HAVE_ALIGNAS
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define _ALLOW_KEYWORD_MACROS
#define alignas(x) __declspec(align(x))
#else
#error "I don't know how to align variables"
#endif
#endif

#if !defined(HAVE_ALIGNAS) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
#define ALIGNED_ARRAY(type, decl, alignment) type alignas(alignment) decl
#else
#define ALIGNED_ARRAY(type, decl, alignment) alignas(alignment) type decl
#endif
