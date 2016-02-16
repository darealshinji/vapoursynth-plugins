/*
  common.h: Copyright (C) 2012-2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This file is part of GenericFilters.

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


#ifndef GENERIC_COMMON_H
#define GENERIC_COMMON_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "VapourSynth.h"

#ifdef _MSC_VER
#pragma warning(disable:4996 4244)
#define snprintf _snprintf
#endif

#define GENERIC_FILTERS_VERSION "1.0.0"


typedef struct generic_handler generic_handler_t;

struct generic_handler {
    VSNodeRef *node;
    const VSVideoInfo *vi;
    int planes[3];
    struct filter_data *fdata;
    void (VS_CC *free_data)(void *);
    void (VS_CC *get_frame_filter)(generic_handler_t *, const VSFormat *,
                                   const VSFrameRef **, const VSAPI *,
                                   const VSFrameRef *, VSFrameRef *);
};

typedef enum {
    ID_NONE,
    ID_CONVOLUTION,
    ID_CONVOLUTION_HV,
    ID_BLUR,
    ID_SOBEL,
    ID_PREWITT,
    ID_TEDGE,
    ID_MAXIMUM,
    ID_MEDIAN,
    ID_MINIMUM,
    ID_INVERT,
    ID_LIMITER,
    ID_LEVELS,
    ID_INFLATE,
    ID_DEFLATE,
    ID_BINARIZE,
    ID_BINARIZE2,
    ID_CANNY,
    ID_GAUSSIAN_BLUR
} filter_id_t;


typedef void (VS_CC *set_filter_data_func)(generic_handler_t *gh,
                                            filter_id_t id, char *msg,
                                            const VSMap *in, VSMap *out,
                                            const VSAPI *vsapi);

extern const set_filter_data_func set_convolution;
extern const set_filter_data_func set_convolution_hv;
extern const set_filter_data_func set_blur;
extern const set_filter_data_func set_edge;
extern const set_filter_data_func set_neighbors;
extern const set_filter_data_func set_invert;
extern const set_filter_data_func set_limiter;
extern const set_filter_data_func set_levels;
extern const set_filter_data_func set_xxflate;
extern const set_filter_data_func set_binarize;
extern const set_filter_data_func set_binarize2;
extern const set_filter_data_func set_canny;


#ifdef USE_ALIGNED_MALLOC
#ifdef USE_X86_INTRINSICS
#ifdef _WIN32
#include <malloc.h>
#else
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
#endif // _WIN32
#else
#define _aligned_malloc(X, Y) malloc(X)
#define _aligned_free(X) free(X)
#endif // USE_X86_INTRINSICS
#endif // USE_ALIGNED_MALLOC

#define RET_IF_ERROR(cond, ...) { \
    if (cond) { \
        snprintf(msg, 240, __VA_ARGS__); \
        return; \
    } \
}

#endif // GENERIC_COMMON_H
