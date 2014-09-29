/*
  inflate_sse2.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include "xxflate.h"
#include "sse2.h"


#define COORDINATES {\
    p0 - 1, p0, p0 + 1,\
    p1 - 1,     p1 + 1,\
    p2 - 1, p2, p2 + 1\
}


static void GF_FUNC_ALIGN VS_CC
proc_8bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
               uint8_t *dstp, const uint8_t *srcp, int th)
{
    uint8_t *p0 = buff + 16;
    uint8_t *p1 = p0 + bstride;
    uint8_t *p2 = p1 + bstride;
    uint8_t *orig = p0, *end = p2;

    line_copy8(p0, srcp + stride, width, 1);
    line_copy8(p1, srcp, width, 1);

    uint8_t threshold = (uint8_t)th;

    __m128i zero = _mm_setzero_si128();
    __m128i xth = _mm_set1_epi8((int8_t)threshold);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy8(p2, srcp, width, 1);
        uint8_t *coordinates[] = COORDINATES;
        for (int x = 0; x < width; x += 16) {
            __m128i sumlo = zero;
            __m128i sumhi = zero;

            for (int i = 0; i < 8; i++) {
                __m128i target = _mm_loadu_si128((__m128i *)(coordinates[i] + x));
                sumlo  = _mm_add_epi16(sumlo, _mm_unpacklo_epi8(target, zero));
                sumhi  = _mm_add_epi16(sumhi, _mm_unpackhi_epi8(target, zero));
            }

            sumlo = _mm_srai_epi16(sumlo, 3);
            sumhi = _mm_srai_epi16(sumhi, 3);
            sumlo = _mm_packus_epi16(sumlo, sumhi);

            __m128i src = _mm_load_si128((__m128i *)(p1 + x));
            __m128i limit = _mm_adds_epu8(src, xth);

            sumlo = _mm_max_epu8(sumlo, src);
            sumlo = _mm_min_epu8(sumlo, limit);

            _mm_store_si128((__m128i *)(dstp + x), sumlo);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_9_10_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
               uint8_t *d, const uint8_t *s, int th)
{
    const uint16_t *srcp = (uint16_t *)s;
    uint16_t *dstp = (uint16_t *)d;
    stride /= 2;
    bstride /= 2;

    uint16_t *p0 = (uint16_t *)buff + 8;
    uint16_t *p1 = p0 + bstride;
    uint16_t *p2 = p1 + bstride;
    uint16_t *orig = p0, *end = p2;

    line_copy16(p0, srcp + stride, width, 1);
    line_copy16(p1, srcp, width, 1);

    int16_t threshold = (int16_t)th;

    __m128i zero = _mm_setzero_si128();
    __m128i xth  = _mm_set1_epi16(threshold);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);
        uint16_t *coordinates[] = COORDINATES;

        for (int x = 0; x < width; x += 8) {
            __m128i sum = zero;
            for (int i = 0; i < 8; i++) {
                __m128i xmm0 = _mm_loadu_si128((__m128i *)(coordinates[i] + x));
                sum = _mm_adds_epu16(sum, xmm0);
            }
            sum = _mm_srai_epi16(sum, 3);

            __m128i src = _mm_load_si128((__m128i *)(p1 + x));
            __m128i limit = _mm_adds_epu16(src, xth);
            
            sum = MM_MAX_EPU16(sum, src);
            sum = MM_MIN_EPU16(sum, limit);
            
            _mm_store_si128((__m128i *)(dstp + x), sum);
        }
        
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_16bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
                uint8_t *d, const uint8_t *s, int th)
{
    const uint16_t *srcp = (uint16_t *)s;
    uint16_t *dstp = (uint16_t *)d;
    stride /= 2;
    bstride /= 2;

    uint16_t *p0 = (uint16_t *)buff + 8;
    uint16_t *p1 = p0 + bstride;
    uint16_t *p2 = p1 + bstride;
    uint16_t *orig = p0, *end = p2;

    line_copy16(p0, srcp + stride, width, 1);
    line_copy16(p1, srcp, width, 1);

    int16_t threshold = (int16_t)th;

    __m128i zero = _mm_setzero_si128();
    __m128i xth  = _mm_set1_epi16(threshold);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);
        uint16_t *coordinates[] = COORDINATES;
        for (int x = 0; x < width; x += 8) {
            __m128i sumlo = zero;
            __m128i sumhi = zero;

            for (int i = 0; i < 8; i++) {
                __m128i target = _mm_loadu_si128((__m128i *)(coordinates[i] + x));
                sumlo = _mm_add_epi32(sumlo, _mm_unpacklo_epi16(target, zero));
                sumhi = _mm_add_epi32(sumhi, _mm_unpackhi_epi16(target, zero));
            }

            sumlo = _mm_srai_epi32(sumlo, 3);
            sumhi = _mm_srai_epi32(sumhi, 3);
            sumlo = mm_cast_epi32(sumlo, sumhi);

            __m128i src = _mm_load_si128((__m128i *)(p1 + x));
            __m128i limit = _mm_adds_epu16(src, xth);

            sumlo = MM_MAX_EPU16(sumlo, src);
            sumlo = MM_MIN_EPU16(sumlo, limit);

            _mm_store_si128((__m128i *)(dstp + x), sumlo);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


const proc_xxflate inflate[] = {
    proc_8bit_sse2,
    proc_9_10_sse2,
    proc_16bit_sse2
};
