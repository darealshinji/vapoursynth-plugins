/*
  sobel_sse2.c: Copyright (C) 2013  Oka Motofumi

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
     pixels         horizontal      vertical
    v0 v1 v2        -1  0  1        -1 -2 -1
    v3 v4 v5        -2  0  2         0  0  0
    v6 v7 v8        -1  0  1         1  2  1

    H = -v0 + v2 - 2*v3 + 2*v5 - v6 + v8
    V = -v0 - 2*v1 - v2 + v6 + 2*v7 + v8
    out = sqrt(pow(H, 2) + pow(V, 2))
        = max(H, V) * 15/16 + min(H, V) * 15/32
    largest error: 6.25% mean error: 1.88%
*/


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
    __m128i all1 = _mm_cmpeq_epi32(zero, zero);
    __m128i one = _mm_srli_epi16(all1, 15);
    __m128i xm15 = _mm_set1_epi16(15);
    __m128i max = _mm_set1_epi8((int8_t)th_max);
    __m128i min = _mm_set1_epi8((int8_t)th_min);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy8(p2, srcp, width, 1);
        uint8_t *array[][6] = {
            /*  -1      -1       -2       1       1       2   */
            { p0 - 1, p2 - 1, p1 - 1, p0 + 1, p2 + 1, p1 + 1 },
            { p0 - 1, p0 + 1, p0    , p2 - 1, p2 + 1, p2     }
        };

        for (int x = 0; x < width; x += 16) {
            __m128i sumlo[2], sumhi[2];

            __m128i xmm0 = _mm_loadu_si128((__m128i *)(p0 + x - 1));
            sumlo[0] = _mm_unpacklo_epi8(xmm0, zero);
            sumhi[0] = _mm_unpackhi_epi8(xmm0, zero);
            sumlo[1] = sumlo[0];
            sumhi[1] = sumhi[0];

            for (int i = 0; i < 2; i++) {
                xmm0 = _mm_loadu_si128((__m128i *)(array[i][1] + x));
                __m128i xmm1 = _mm_unpackhi_epi8(xmm0, zero);
                xmm0 = _mm_unpacklo_epi8(xmm0, zero);
                sumlo[i] = _mm_add_epi16(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi16(sumhi[i], xmm1);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][2] + x));
                xmm1 = _mm_slli_epi16(_mm_unpackhi_epi8(xmm0, zero), 1);
                xmm0 = _mm_slli_epi16(_mm_unpacklo_epi8(xmm0, zero), 1);
                sumlo[i] = _mm_add_epi16(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi16(sumhi[i], xmm1);

                // -x - y - 2z = (x + y + 2z) * -1
                sumlo[i] = _mm_add_epi16(one, _mm_xor_si128(sumlo[i], all1));
                sumhi[i] = _mm_add_epi16(one, _mm_xor_si128(sumhi[i], all1));

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][3] + x));
                xmm1 = _mm_unpackhi_epi8(xmm0, zero);
                xmm0 = _mm_unpacklo_epi8(xmm0, zero);
                sumlo[i] = _mm_add_epi16(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi16(sumhi[i], xmm1);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][4] + x));
                xmm1 = _mm_unpackhi_epi8(xmm0, zero);
                xmm0 = _mm_unpacklo_epi8(xmm0, zero);
                sumlo[i] = _mm_add_epi16(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi16(sumhi[i], xmm1);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][5] + x));
                xmm1 = _mm_slli_epi16(_mm_unpackhi_epi8(xmm0, zero), 1);
                xmm0 = _mm_slli_epi16(_mm_unpacklo_epi8(xmm0, zero), 1);
                sumlo[i] = _mm_add_epi16(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi16(sumhi[i], xmm1);

                sumlo[i] = mm_abs_epi16(sumlo[i]);
                sumhi[i] = mm_abs_epi16(sumhi[i]);
            }

            __m128i xmax = _mm_max_epi16(sumlo[0], sumlo[1]);
            __m128i xmin = _mm_min_epi16(sumlo[0], sumlo[1]);
            xmax = _mm_srli_epi16(_mm_mullo_epi16(xmax, xm15), 4);
            xmin = _mm_srli_epi16(_mm_mullo_epi16(xmin, xm15), 5);
            __m128i outlo = _mm_add_epi16(xmax, xmin);

            xmax = _mm_max_epi16(sumhi[0], sumhi[1]);
            xmin = _mm_min_epi16(sumhi[0], sumhi[1]);
            xmax = _mm_srli_epi16(_mm_mullo_epi16(xmax, xm15), 4);
            xmin = _mm_srli_epi16(_mm_mullo_epi16(xmin, xm15), 5);
            __m128i outhi = _mm_add_epi16(xmax, xmin);

            outlo = _mm_srli_epi16(outlo, eh->rshift);
            outhi = _mm_srli_epi16(outhi, eh->rshift);

            xmm0  = _mm_packus_epi16(outlo, outhi);

            __m128i temp = _mm_min_epu8(xmm0, max);
            temp = _mm_cmpeq_epi8(temp, max);
            xmm0 = _mm_or_si128(temp, xmm0);

            temp = _mm_max_epu8(xmm0, min);
            temp = _mm_cmpeq_epi8(temp, min);
            xmm0 = _mm_andnot_si128(temp, xmm0);

            _mm_store_si128((__m128i *)(dstp + x), xmm0);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig: p2 + bstride;
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

    __m128i xm15 = _mm_set1_epi16(15);
    __m128i all1 = _mm_cmpeq_epi32(xm15, xm15);
    __m128i one = _mm_srli_epi16(all1, 15);
    __m128i min = _mm_set1_epi16((int16_t)th_min);
    __m128i max = _mm_set1_epi16((int16_t)th_max);
    __m128i pmax = _mm_set1_epi16((int16_t)plane_max);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);
        uint16_t *array[][6] = {
            /*  -1      -1       -2       1       1       2   */
            { p0 - 1, p2 - 1, p1 - 1, p0 + 1, p2 + 1, p1 + 1 },
            { p0 - 1, p0 + 1, p0    , p2 - 1, p2 + 1, p2     }
        };
        for (int x = 0; x < width; x += 8) {
            __m128i sum[2];
            sum[0] = _mm_loadu_si128((__m128i *)(p0 + x - 1));
            sum[1] = sum[0];

            for (int i = 0; i < 2; i++) {
                __m128i xmm0 = _mm_loadu_si128((__m128i *)(array[i][1] + x));
                sum[i] = _mm_add_epi16(sum[i], xmm0);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][2] + x));
                xmm0 = _mm_slli_epi16(xmm0, 1);
                sum[i] = _mm_add_epi16(sum[i], xmm0);

                sum[i] = _mm_add_epi16(one, _mm_xor_si128(sum[i], all1));

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][3] + x));
                sum[i] = _mm_add_epi16(sum[i], xmm0);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][4] + x));
                sum[i] = _mm_add_epi16(sum[i], xmm0);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][5] + x));
                xmm0 = _mm_slli_epi16(xmm0, 1);
                sum[i]  = _mm_add_epi16(sum[i], xmm0);

                sum[i] = mm_abs_epi16(sum[i]);
            }

            __m128i xmax = _mm_max_epi16(sum[0], sum[1]);
            __m128i xmin = _mm_min_epi16(sum[0], sum[1]);
            xmax = _mm_srli_epi16(_mm_mullo_epi16(xmax, xm15), 4);
            xmin = _mm_srli_epi16(_mm_mullo_epi16(xmin, xm15), 5);
            __m128i out = _mm_add_epi16(xmax, xmin);

            out = _mm_srli_epi16(out, eh->rshift);
            out = _mm_min_epi16(out, max);
            out = _mm_max_epi16(out, min);

            __m128i temp = _mm_cmpeq_epi16(out, max);
            temp = _mm_and_si128(temp, pmax);
            out  = _mm_or_si128(out, temp);

            temp = _mm_cmpeq_epi16(out, min);
            out = _mm_andnot_si128(temp, out);

            _mm_store_si128((__m128i *)(dstp + x), out);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig: p2 + bstride;
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
    __m128i one = _mm_srli_epi32(all1, 31);
    __m128 alpha = _mm_set1_ps((float)0.96043387);
    __m128 beta = _mm_set1_ps((float)0.39782473);
    __m128i pmax = _mm_set1_epi32(0xFFFF);
    __m128i min = _mm_set1_epi16((int16_t)eh->min);
    __m128i max = _mm_set1_epi16((int16_t)eh->max);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);
        uint16_t *array[][6] = {
            /*  -1      -1       -2       1       1       2   */
            { p0 - 1, p2 - 1, p1 - 1, p0 + 1, p2 + 1, p1 + 1 },
            { p0 - 1, p0 + 1, p0    , p2 - 1, p2 + 1, p2     }
        };
        for (int x = 0; x < width; x += 8) {
            __m128i sumlo[2], sumhi[2];

            __m128i xmm0 = _mm_loadu_si128((__m128i *)(p0 + x - 1));
            sumlo[0] = _mm_unpacklo_epi16(xmm0, zero);
            sumhi[0] = _mm_unpackhi_epi16(xmm0, zero);
            sumlo[1] = sumlo[0];
            sumhi[1] = sumhi[0];

            for (int i = 0; i < 2; i++) {
                xmm0 = _mm_loadu_si128((__m128i *)(array[i][1] + x));
                __m128i xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sumlo[i] = _mm_add_epi32(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi32(sumhi[i], xmm1);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][2] + x));
                xmm1 = _mm_slli_epi32(_mm_unpackhi_epi16(xmm0, zero), 1);
                xmm0 = _mm_slli_epi32(_mm_unpacklo_epi16(xmm0, zero), 1);
                sumlo[i] = _mm_add_epi32(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi32(sumhi[i], xmm1);

                sumlo[i] = _mm_add_epi32(one, _mm_xor_si128(sumlo[i], all1));
                sumhi[i] = _mm_add_epi32(one, _mm_xor_si128(sumhi[i], all1));

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][3] + x));
                xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sumlo[i] = _mm_add_epi32(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi32(sumhi[i], xmm1);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][4] + x));
                xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sumlo[i] = _mm_add_epi32(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi32(sumhi[i], xmm1);

                xmm0 = _mm_loadu_si128((__m128i *)(array[i][5] + x));
                xmm1 = _mm_slli_epi32(_mm_unpackhi_epi16(xmm0, zero), 1);
                xmm0 = _mm_slli_epi32(_mm_unpacklo_epi16(xmm0, zero), 1);
                sumlo[i] = _mm_add_epi32(sumlo[i], xmm0);
                sumhi[i] = _mm_add_epi32(sumhi[i], xmm1);

                sumlo[i] = mm_abs_epi32(sumlo[i]);
                sumhi[i] = mm_abs_epi32(sumhi[i]);
            }

            __m128 xmax = _mm_cvtepi32_ps(mm_max_epi32(sumlo[0], sumlo[1]));
            __m128 xmin = _mm_cvtepi32_ps(mm_min_epi32(sumlo[0], sumlo[1]));
            xmax = _mm_mul_ps(xmax, alpha);
            xmin = _mm_mul_ps(xmin, beta);
            __m128i outlo = _mm_cvtps_epi32(_mm_add_ps(xmax, xmin));

            xmax = _mm_cvtepi32_ps(mm_max_epi32(sumhi[0], sumhi[1]));
            xmin = _mm_cvtepi32_ps(mm_min_epi32(sumhi[0], sumhi[1]));
            xmax = _mm_mul_ps(xmax, alpha);
            xmin = _mm_mul_ps(xmin, beta);
            __m128i outhi = _mm_cvtps_epi32(_mm_add_ps(xmax, xmin));

            outlo = _mm_srli_epi32(outlo, eh->rshift);
            outhi = _mm_srli_epi32(outhi, eh->rshift);

            outlo = mm_min_epi32(pmax, outlo);
            outhi = mm_min_epi32(pmax, outhi);

            outlo = mm_cast_epi32(outlo, outhi);

            outhi = MM_MIN_EPU16(outlo, max);
            outhi = _mm_cmpeq_epi16(outhi, max);
            outlo = _mm_or_si128(outhi, outlo);

            outhi = MM_MAX_EPU16(outlo, min);
            outhi = _mm_cmpeq_epi16(outhi, min);
            outlo = _mm_andnot_si128(outhi, outlo);

            _mm_store_si128((__m128i *)(dstp + x), outlo);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig: p2 + bstride;
    }
}


const proc_edge_detection sobel[] = {
    proc_8bit_sse2,
    proc_9_10_sse2,
    proc_16bit_sse2
};
