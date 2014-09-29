/*
  maximum_sse2.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include "neighbors.h"
#include "sse2.h"

#define COORDINATES {\
    p0 + x - 1, p0 + x, p0 + x + 1,\
    p1 + x - 1,         p1 + x + 1,\
    p2 + x - 1, p2 + x, p2 + x + 1\
}


static void GF_FUNC_ALIGN VS_CC
proc_8bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
               uint8_t *dstp, const uint8_t *srcp, int th, int *enable)
{
    uint8_t *p0 = buff + 16;
    uint8_t *p1 = p0 + bstride;
    uint8_t *p2 = p1 + bstride;
    uint8_t *orig = p0, *end = p2;
    uint8_t threshold = th > 255 ? 255 : (uint8_t)th;

    line_copy8(p0, srcp + stride, width, 1);
    line_copy8(p1, srcp, width, 1);

    __m128i xth = _mm_set1_epi8((int8_t)threshold);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy8(p2, srcp, width, 1);

        for (int x = 0; x < width; x += 16) {
            uint8_t *coordinates[] = COORDINATES;

            __m128i src = _mm_load_si128((__m128i *)(p1 + x));
            __m128i max = src;
            
            for (int i = 0; i < 8; i++) {
                if (enable[i]) {
                    __m128i target = _mm_loadu_si128((__m128i *)coordinates[i]);
                    max = _mm_max_epu8(target, max);
                }
            }
            
            __m128i limit = _mm_adds_epu8(src, xth);
            max = _mm_min_epu8(max, limit);
            _mm_store_si128((__m128i *)(dstp + x), max);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_16bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
                uint8_t *d, const uint8_t *s, int th, int *enable)
{
    stride /= 2;
    bstride /= 2;
    uint16_t *dstp = (uint16_t *)d;
    const uint16_t *srcp = (uint16_t *)s;

    uint16_t *p0 = (uint16_t *)buff + 8;
    uint16_t *p1 = p0 + bstride;
    uint16_t *p2 = p1 + bstride;
    uint16_t *orig = p0, *end = p2;
    uint16_t threshold = (uint16_t)th;

    line_copy16(p0, srcp, width, 1);
    line_copy16(p1, srcp, width, 1);

    __m128i xth = _mm_set1_epi16((int16_t)threshold);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);

        for (int x = 0; x < width; x += 8) {
            uint16_t *coordinates[] = COORDINATES;

            __m128i src = _mm_load_si128((__m128i *)(p1 + x));
            __m128i max = src;

            for (int i = 0; i < 8; i++) {
                if (enable[i]) {
                    __m128i target = _mm_loadu_si128((__m128i *)coordinates[i]);
                    max = MM_MAX_EPU16(max, target);
                }
            }
            
            __m128i limit = _mm_adds_epu8(src, xth);
            max = MM_MIN_EPU16(max, limit);
            _mm_store_si128((__m128i *)(dstp + x), max);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


const proc_neighbors maximum[] = {
    proc_8bit_sse2,
    proc_16bit_sse2
};
