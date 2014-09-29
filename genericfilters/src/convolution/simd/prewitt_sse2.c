/*
  prewitt_sse2.c: Copyright (C) 2013  Oka Motofumi

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


#include "edge.h"
#include "sse2.h"

/*
                Prewitt compass
       0          1          2          3
    1  1  1    1  1  1    1  1 -1    1 -1 -1
    1 -2  1    1 -2 -1    1 -2 -1    1 -2 -1
   -1 -1 -1    1 -1 -1    1  1 -1    1  1  1

       4          5           6         7
   -1 -1 -1   -1 -1  1    -1  1  1   1  1  1
    1 -2  1   -1 -2  1    -1 -2  1  -1 -2  1
    1  1  1    1  1  1    -1  1  1  -1 -1  1
*/

#define COORDINATES {\
    {p0-1, p0  , p0+1, p1-1, p1+1, p2-1, p2  , p2+1},\
    {p0-1, p0  , p0+1, p1-1, p2-1, p1+1, p2  , p2+1},\
    {p0-1, p0  , p1-1, p2-1, p2  , p0+1, p1+1, p2+1},\
    {p0-1, p1-1, p2-1, p2  , p2+1, p0  , p0+1, p1+1},\
    {p1-1, p1+1, p2-1, p2  , p2+1, p0-1, p0  , p0+1},\
    {p0+1, p1+1, p2-1, p2  , p2+1, p0-1, p0  , p1-1},\
    {p0  , p0+1, p1+1, p2  , p2+1, p0-1, p1-1, p2-1},\
    {p0-1, p0  , p0+1, p1+1, p2+1, p1-1, p2-1, p2  }\
}

static void GF_FUNC_ALIGN VS_CC
proc_8bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
               uint8_t *dstp, const uint8_t *srcp, edge_t *eh,
               uint16_t plane_max)
{
    uint8_t *p0 = buff + 16;
    uint8_t *p1 = p0 + bstride;
    uint8_t *p2 = p1 + bstride;
    uint8_t *orig = p0, *end = p2;

    line_copy8(p0, srcp + stride, width, 1);
    line_copy8(p1, srcp, width, 1);

    uint8_t th_min = eh->min > 0xFF ? 0xFF : (uint8_t)eh->min;
    uint8_t th_max = eh->max > 0xFF ? 0xFF : (uint8_t)eh->max;

    __m128i zero = _mm_setzero_si128();
    __m128i min = _mm_set1_epi8((int8_t)th_min);
    __m128i max = _mm_set1_epi8((int8_t)th_max);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy8(p2, srcp, width, 1);
        uint8_t *array[][8] = COORDINATES;

        for (int x = 0; x < width; x += 16) {
            __m128i abs_lo = zero, abs_hi = zero;
            __m128i xmm0, xmm1, p1_lo, p1_hi;

            xmm0   = _mm_load_si128((__m128i *)(p1 + x));
            p1_lo = _mm_slli_epi16(_mm_unpacklo_epi8(xmm0, zero), 1);
            p1_hi = _mm_slli_epi16(_mm_unpackhi_epi8(xmm0, zero), 1);

            for (int i = 0; i < 8; i++) {
                __m128i sum_lo = p1_lo;
                __m128i sum_hi = p1_hi;

                for (int j = 5; j < 8; j++) {
                    xmm0 = _mm_loadu_si128((__m128i *)(array[i][j] + x));
                    xmm1 = _mm_unpackhi_epi8(xmm0, zero);
                    xmm0 = _mm_unpacklo_epi8(xmm0, zero);
                    sum_lo = _mm_add_epi16(sum_lo, xmm0);
                    sum_hi = _mm_add_epi16(sum_hi, xmm1);
                }

                __m128i all1 = _mm_cmpeq_epi32(zero, zero);
                __m128i one = _mm_srli_epi16(all1, 15);
                sum_lo = _mm_add_epi16(one, _mm_xor_si128(sum_lo, all1));
                sum_hi = _mm_add_epi16(one, _mm_xor_si128(sum_hi, all1));

                for (int j = 0; j < 5; j++) {
                    xmm0   = _mm_loadu_si128((__m128i *)(array[i][j] + x));
                    xmm1   = _mm_unpackhi_epi8(xmm0, zero);
                    xmm0   = _mm_unpacklo_epi8(xmm0, zero);
                    sum_lo = _mm_add_epi16(sum_lo, xmm0);
                    sum_hi = _mm_add_epi16(sum_hi, xmm1);
                }

                sum_lo = mm_abs_epi16(sum_lo);
                abs_lo = _mm_max_epi16(abs_lo, sum_lo);

                sum_hi = mm_abs_epi16(sum_hi);
                abs_hi = _mm_max_epi16(abs_hi, sum_hi);
            }

            abs_lo = _mm_srli_epi16(abs_lo, eh->rshift);
            abs_hi = _mm_srli_epi16(abs_hi, eh->rshift);

            xmm0 = _mm_packus_epi16(abs_lo, abs_hi);

            xmm1 = _mm_min_epu8(xmm0, max);
            xmm1 = _mm_cmpeq_epi8(xmm1, max);
            xmm0 = _mm_or_si128(xmm0, xmm1);

            xmm1 = _mm_max_epu8(xmm0, min);
            xmm1 = _mm_cmpeq_epi8(xmm1, min);
            xmm0 = _mm_andnot_si128(xmm1, xmm0);

            _mm_store_si128((__m128i *)(dstp + x), xmm0);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_9_10_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
               uint8_t *d, const uint8_t *s, edge_t *eh, uint16_t plane_max)
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

    uint16_t th_min = eh->min > plane_max ? plane_max : (uint16_t)eh->min;
    uint16_t th_max = eh->max > plane_max ? plane_max : (uint16_t)eh->max;

    __m128i xmin = _mm_set1_epi16((int16_t)th_min);
    __m128i xmax = _mm_set1_epi16((int16_t)th_max);
    __m128i pmax = _mm_set1_epi16((int16_t)plane_max);
    __m128i all1 = _mm_cmpeq_epi32(xmin, xmin);
    __m128i one = _mm_srli_epi16(all1, 15);
    __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);
        uint16_t *array[][8] = COORDINATES;

        for (int x = 0; x < width; x += 8) {

            __m128i abs = zero;
            __m128i xmm0, xp1;

            xmm0 = _mm_load_si128((__m128i *)(p1 + x));
            xp1  = _mm_slli_epi16(xmm0, 1);

            for (int i = 0; i < 8; i++) {
                __m128i sum = xp1;

                for (int j = 5; j < 8; j++) {
                    xmm0 = _mm_loadu_si128((__m128i *)(array[i][j] + x));
                    sum  = _mm_add_epi16(sum, xmm0);
                }
                sum = _mm_add_epi16(one, _mm_xor_si128(sum, all1));

                for (int j = 0; j < 5; j++) {
                    xmm0 = _mm_loadu_si128((__m128i *)(array[i][j] + x));
                    sum  = _mm_add_epi16(sum, xmm0);
                }

                sum = mm_abs_epi16(sum);
                abs  = _mm_max_epi16(abs, sum);
            }

            abs = _mm_srli_epi16(abs, eh->rshift);
            abs = _mm_min_epi16(abs, xmax);
            abs = _mm_max_epi16(abs, xmin);

            xmm0 = _mm_cmpeq_epi16(abs, xmax);
            xmm0 = _mm_and_si128(xmm0, pmax);
            abs  = _mm_or_si128(abs, xmm0);

            xmm0 = _mm_cmpeq_epi16(abs, xmin);
            abs  = _mm_andnot_si128(xmm0, abs);

            _mm_store_si128((__m128i *)(dstp + x), abs);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_16bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
                uint8_t *d, const uint8_t *s, edge_t *eh, uint16_t plane_max)
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

    __m128i zero = _mm_setzero_si128();
    __m128i all1 = _mm_cmpeq_epi32(zero, zero);
    __m128i one  = _mm_srli_epi32(all1, 31);
    __m128i pmax = _mm_set1_epi32(0xFFFF);
    __m128i min = _mm_set1_epi16((int16_t)eh->min);
    __m128i max = _mm_set1_epi16((int16_t)eh->max);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);
        uint16_t *array[][8] = COORDINATES;

        for (int x = 0; x < width; x += 8) {
             __m128i abs_lo = zero, abs_hi = zero;
            __m128i xmm0, xmm1, p1_lo, p1_hi;

            xmm0  = _mm_load_si128((__m128i *)(p1 + x));
            xmm1  = _mm_unpackhi_epi16(xmm0, zero);
            xmm0  = _mm_unpacklo_epi16(xmm0, zero);
            p1_lo = _mm_slli_epi32(xmm0, 1);
            p1_hi = _mm_slli_epi32(xmm1, 1);

            for (int i = 0; i < 8; i++) {
                __m128i sum_lo = p1_lo;
                __m128i sum_hi = p1_hi;

                for (int j = 5; j < 8; j++) {
                    xmm0 = _mm_loadu_si128((__m128i *)(array[i][j] + x));
                    xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                    xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                    sum_lo = _mm_add_epi32(sum_lo, xmm0);
                    sum_hi = _mm_add_epi32(sum_hi, xmm1);
                }

                sum_lo = _mm_add_epi32(one, _mm_xor_si128(sum_lo, all1));
                sum_hi = _mm_add_epi32(one, _mm_xor_si128(sum_hi, all1));

                for (int j = 0; j < 5; j++) {
                    xmm0   = _mm_loadu_si128((__m128i *)(array[i][j] + x));
                    xmm1   = _mm_unpackhi_epi16(xmm0, zero);
                    xmm0   = _mm_unpacklo_epi16(xmm0, zero);
                    sum_lo = _mm_add_epi32(sum_lo, xmm0);
                    sum_hi = _mm_add_epi32(sum_hi, xmm1);
                }

                sum_lo = mm_abs_epi32(sum_lo);
                sum_hi = mm_abs_epi32(sum_hi);
                abs_lo = mm_max_epi32(abs_lo, sum_lo);
                abs_hi = mm_max_epi32(abs_hi, sum_hi);
            }

            abs_lo = mm_min_epi32(pmax, _mm_srli_epi32(abs_lo, eh->rshift));
            abs_hi = mm_min_epi32(pmax, _mm_srli_epi32(abs_hi, eh->rshift));

            abs_lo = mm_cast_epi32(abs_lo, abs_hi);

            xmm0 = MM_MIN_EPU16(abs_lo, max);
            xmm0 = _mm_cmpeq_epi16(xmm0, max);
            abs_lo = _mm_or_si128(xmm0, abs_lo);

            xmm0 = MM_MAX_EPU16(abs_lo, min);
            xmm0 = _mm_cmpeq_epi16(xmm0, min);
            abs_lo = _mm_andnot_si128(xmm0, abs_lo);

            _mm_store_si128((__m128i *)(dstp + x), abs_lo);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig: p2 + bstride;
    }
}



const proc_edge_detection prewitt[] = {
    proc_8bit_sse2,
    proc_9_10_sse2,
    proc_16bit_sse2
};
