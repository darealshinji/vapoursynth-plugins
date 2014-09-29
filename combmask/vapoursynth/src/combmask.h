/*
  combmask.h: Copyright (C) 2012-2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This file is part of CombMask.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with the author; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


#ifndef VS_COMBMASK_H
#define VS_COMBMASK_H

#include <emmintrin.h>
#include "VapourSynth.h"

#define COMBMASK_VERSION "0.0.1"

#define MM_MIN_EPU16(X, Y) (_mm_subs_epu16(X, _mm_subs_epu16(X, Y)))
#define MM_MAX_EPU16(X, Y) (_mm_adds_epu16(Y, _mm_subs_epu16(X, Y)))

#ifdef _MSC_VER
#pragma warning(disable:4996 4244)
#endif

#ifdef __MINGW32__
#define CM_FUNC_ALIGN __attribute__((force_align_arg_pointer))
#else
#define CM_FUNC_ALIGN
#endif

typedef struct combmask combmask_t;

typedef struct maskedmerge maskedmerge_t;

typedef void (VS_CC *func_write_combmask)(combmask_t *ch, const VSAPI *vsapi,
                                           const VSFrameRef *src,
                                           VSFrameRef *cmask);

typedef void (VS_CC *func_adapt_motion)(combmask_t *ch, const VSAPI *vsapi,
                                         const VSFrameRef *src,
                                         const VSFrameRef *prev,
                                         VSFrameRef *cmask);

typedef void (VS_CC *func_write_motionmask)(int mthresh, int width,
                                             int height, int stride,
                                             __m128i *maskp, __m128i *srcp,
                                             __m128i *prevp);

typedef int (VS_CC *func_is_combed)(combmask_t *ch, VSFrameRef *cmask,
                                     const VSAPI *vsapi);

typedef void (VS_CC *func_h_dilation)(combmask_t *ch, VSFrameRef *cmask,
                                       const VSAPI *vsapi);

typedef void (VS_CC *func_merge_frames)(maskedmerge_t *mh, const VSAPI *vsapi,
                                         const VSFrameRef *mask,
                                         const VSFrameRef *alt,
                                         VSFrameRef *dst);


struct combmask {
    VSNodeRef *node;
    const VSVideoInfo *vi;
    int planes[3];
    int cthresh;
    int mthresh;
    int mi;
    func_write_combmask write_combmask;
    func_write_motionmask write_motionmask;
    func_is_combed is_combed;
    func_h_dilation horizontal_dilation;
};

struct maskedmerge {
    VSNodeRef *base;
    VSNodeRef *altc;
    VSNodeRef *mask;
    const VSVideoInfo *vi;
    int planes[3];
};


extern const func_adapt_motion      adapt_motion;
extern const func_write_combmask    write_combmask_funcs[];
extern const func_write_motionmask  write_motionmask_funcs[];
extern const func_is_combed         is_combed_funcs[];
extern const func_h_dilation        h_dilation_funcs[];
extern const func_merge_frames      merge_frames;


#ifdef USE_ALIGNED_MALLOC
#   ifdef _WIN32
#       include <malloc.h>
#   else
static inline void *_aligned_malloc(size_t size, size_t alignment)
{
    void *p;
    int ret = posix_memalign(&p, alignment, size);
    return (ret == 0) ? p : 0;
}

static inline void _aligned_free(void *p)
{
    free(p);
}
#   endif // _WIN32
#endif // USE_ALIGNED_MALLOC

#endif // VS_COMBMASK_H
