/*
  median_sse2.c: Copyright (C) 2012-2013  Oka Motofumi

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

#include <string.h>
#include <stdint.h>

#include "neighbors.h"
#include "sse2.h"

/* from 'Implementing Median Filters in XC4000E FPGAs' by John L. Smith */
#define LOWHIGHu8(X, Y) {\
    __m128i min = _mm_min_epu8(X, Y); \
    __m128i max = _mm_max_epu8(X, Y); \
    X = min; \
    Y = max; \
}


static void GF_FUNC_ALIGN VS_CC
proc_8bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
               uint8_t *dstp, const uint8_t *srcp, int th, int *enable)
{
    uint8_t *p0 = buff + 16;
    uint8_t *p1 = p0 + bstride;
    uint8_t *p2 = p1 + bstride;
    uint8_t *orig = p0, *end = p2;

    line_copy8(p0, srcp, width, 1);
    line_copy8(p1, srcp, width, 1);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy8(p2, srcp, width, 1);

        for (int x = 0; x < width; x += 16) {
            __m128i x0 = _mm_load_si128((__m128i *)(p0 + x));
            __m128i x1 = _mm_loadu_si128((__m128i *)(p0 + x + 1));
            LOWHIGHu8(x0, x1);
            __m128i x2 = _mm_loadu_si128((__m128i *)(p0 + x - 1));
            LOWHIGHu8(x0, x2);
            LOWHIGHu8(x1, x2);
            __m128i x3 = _mm_load_si128((__m128i *)(p1 + x));
            __m128i x4 = _mm_loadu_si128((__m128i *)(p1 + x + 1));
            LOWHIGHu8(x3, x4);
            __m128i x5 = _mm_loadu_si128((__m128i *)(p1 + x - 1));
            LOWHIGHu8(x3, x5);
            LOWHIGHu8(x4, x5);
            x0 = _mm_max_epu8(x0, x3);
            x3 = _mm_load_si128((__m128i *)(p2 + x));
            __m128i x6 = _mm_loadu_si128((__m128i *)(p2 + x + 1));
            LOWHIGHu8(x3, x6);
            __m128i x7 = _mm_loadu_si128((__m128i *)(p2 + x - 1));
            LOWHIGHu8(x3, x7);
            LOWHIGHu8(x6, x7);
            x0 = _mm_max_epu8(x0, x3);
            LOWHIGHu8(x4, x6);
            x1 = _mm_max_epu8(x1, x4);
            x5 = _mm_min_epu8(x5, x7);
            x2 = _mm_min_epu8(x2, x5);
            x1 = _mm_min_epu8(x1, x6);
            LOWHIGHu8(x1, x2);
            x0 = _mm_max_epu8(x0, x1);
            x0 = _mm_min_epu8(x0, x2);
            _mm_store_si128((__m128i *)(dstp + x), x0);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}
#undef LOWHIGHu8

#define LOWHIGHu16(X, Y) {\
    __m128i min = MM_MIN_EPU16(X, Y); \
    __m128i max = MM_MAX_EPU16(X, Y); \
    X = min; \
    Y = max; \
}


static void GF_FUNC_ALIGN VS_CC
proc_16bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
                uint8_t *d, const uint8_t *s, int th, int *enable)
{
    stride /= 2;
    uint16_t *dstp = (uint16_t *)d;
    const uint16_t *srcp = (uint16_t *)s;

    bstride /= 2;
    uint16_t *p0 = (uint16_t *)buff + 8;
    uint16_t *p1 = p0 + bstride;
    uint16_t *p2 = p1 + bstride;
    uint16_t *orig = p0, *end = p2;

    line_copy16(p0, srcp, width, 1);
    line_copy16(p1, srcp, width, 1);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);

        for (int x = 0; x < width; x += 8) {
            __m128i x0 = _mm_load_si128((__m128i *)(p0 + x));
            __m128i x1 = _mm_loadu_si128((__m128i *)(p0 + x + 1));
            LOWHIGHu16(x0, x1);
            __m128i x2 = _mm_loadu_si128((__m128i *)(p0 + x - 1));
            LOWHIGHu16(x0, x2);
            LOWHIGHu16(x1, x2);
            __m128i x3 = _mm_load_si128((__m128i *)(p1 + x));
            __m128i x4 = _mm_loadu_si128((__m128i *)(p1 + x + 1));
            LOWHIGHu16(x3, x4);
            __m128i x5 = _mm_loadu_si128((__m128i *)(p1 + x - 1));
            LOWHIGHu16(x3, x5);
            LOWHIGHu16(x4, x5);
            x0 = MM_MAX_EPU16(x0, x3);
            x3 = _mm_load_si128((__m128i *)(p2 + x));
            __m128i x6 = _mm_loadu_si128((__m128i *)(p2 + x + 1));
            LOWHIGHu16(x3, x6);
            __m128i x7 = _mm_loadu_si128((__m128i *)(p2 + x - 1));
            LOWHIGHu16(x3, x7);
            LOWHIGHu16(x6, x7);
            x0 = MM_MAX_EPU16(x0, x3);
            LOWHIGHu16(x4, x6);
            x1 = MM_MAX_EPU16(x1, x4);
            x5 = MM_MIN_EPU16(x5, x7);
            x2 = MM_MIN_EPU16(x2, x5);
            x1 = MM_MIN_EPU16(x1, x6);
            LOWHIGHu16(x1, x2);
            x0 = MM_MAX_EPU16(x0, x1);
            x0 = MM_MIN_EPU16(x0, x2);
            _mm_store_si128((__m128i *)(dstp + x), x0);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}
#undef LOWHIGHu16


const proc_neighbors median[] = {
    proc_8bit_sse2,
    proc_16bit_sse2
};
