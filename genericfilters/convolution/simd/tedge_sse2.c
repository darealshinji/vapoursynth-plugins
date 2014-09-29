/*
  tedge_sse2.c: Copyright (C) 2013  Oka Motofumi

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


// 12:74 -> (int)(12.0/3):(int)(74.0/3+0.5)
static const GF_ALIGN int16_t ar_mulx[][8] = {
    {  4,   4,   4,   4,   4,   4,   4,   4},
    {-25, -25, -25, -25, -25, -25, -25, -25},
    { 25,  25,  25,  25,  25,  25,  25,  25},
    { -4,  -4,  -4,  -4,  -4,  -4,  -4,  -4}
};
static const GF_ALIGN int16_t ar_muly[][8] = {
    { -4,  -4,  -4,  -4,  -4,  -4,  -4,  -4},
    { 25,  25,  25,  25,  25,  25,  25,  25},
    {-25, -25, -25, -25, -25, -25, -25, -25},
    {  4,   4,   4,   4,   4,   4,   4,   4}
};


static void GF_FUNC_ALIGN VS_CC
proc_8bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
               uint8_t *dstp, const uint8_t *srcp, edge_t *eh,
               uint16_t plane_max)
{
    uint8_t* p0 = buff + 16;
    uint8_t* p1 = p0 + bstride;
    uint8_t* p2 = p1 + bstride;
    uint8_t* p3 = p2 + bstride;
    uint8_t* p4 = p3 + bstride;
    uint8_t* orig = p0;
    uint8_t* end = p4;

    line_copy8(p0, srcp + 2 * stride, width, 2);
    line_copy8(p1, srcp + stride, width, 2);
    line_copy8(p2, srcp, width, 2);
    srcp += stride;
    line_copy8(p3, srcp, width, 2);

    uint8_t th_min = eh->min > 0xFF ? 0xFF : (uint8_t)eh->min;
    uint8_t th_max = eh->max > 0xFF ? 0xFF : (uint8_t)eh->max;

    __m128i zero = _mm_setzero_si128();
    __m128i ab = _mm_set1_epi16(15);
    __m128i max = _mm_set1_epi8((int8_t)th_max);
    __m128i min = _mm_set1_epi8((int8_t)th_min);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 2 ? 1 : -1);
        line_copy8(p4, srcp, width, 2);
        uint8_t* posh[] = {p2 - 2, p2 - 1, p2 + 1, p2 + 2};
        uint8_t* posv[] = {p0, p1, p3, p4};

        for (int x = 0; x < width; x += 16) {
            __m128i sumx[2] = {zero, zero};
            __m128i sumy[2] = {zero, zero};

            for (int i = 0; i < 4; i++) {
                __m128i xmm0, xmm1, xmul;
                xmul = _mm_load_si128((__m128i *)ar_mulx[i]);
                xmm0 = _mm_loadu_si128((__m128i *)(posh[i] + x));
                xmm1 = _mm_unpackhi_epi8(xmm0, zero);
                xmm0 = _mm_unpacklo_epi8(xmm0, zero);
                sumx[0] = _mm_add_epi16(sumx[0], _mm_mullo_epi16(xmm0, xmul));
                sumx[1] = _mm_add_epi16(sumx[1], _mm_mullo_epi16(xmm1, xmul));

                xmul = _mm_load_si128((__m128i *)ar_muly[i]);
                xmm0 = _mm_load_si128((__m128i *)(posv[i] + x));
                xmm1 = _mm_unpackhi_epi8(xmm0, zero);
                xmm0 = _mm_unpacklo_epi8(xmm0, zero);
                sumy[0] = _mm_add_epi16(sumy[0], _mm_mullo_epi16(xmm0, xmul));
                sumy[1] = _mm_add_epi16(sumy[1], _mm_mullo_epi16(xmm1, xmul));
            }

            for (int i = 0; i < 2; i++) {
                __m128i xmax, xmin, mull, mulh;
                sumx[i] = mm_abs_epi16(sumx[i]);
                sumy[i] = mm_abs_epi16(sumy[i]);
                xmax = _mm_max_epi16(sumx[i], sumy[i]);
                xmin = _mm_min_epi16(sumx[i], sumy[i]);

                mull = _mm_srli_epi32(_mm_madd_epi16(ab, _mm_unpacklo_epi16(xmax, zero)), 4);
                mulh = _mm_srli_epi32(_mm_madd_epi16(ab, _mm_unpackhi_epi16(xmax, zero)), 4);
                xmax = mm_cast_epi32(mull, mulh);

                mull = _mm_srli_epi32(_mm_madd_epi16(ab, _mm_unpacklo_epi16(xmin, zero)), 5);
                mulh = _mm_srli_epi32(_mm_madd_epi16(ab, _mm_unpackhi_epi16(xmin, zero)), 5);
                xmin = mm_cast_epi32(mull, mulh);

                sumx[i] = _mm_adds_epu16(xmax, xmin);
                sumx[i] = _mm_srli_epi16(sumx[i], eh->rshift);
            }

            __m128i out = _mm_packus_epi16(sumx[0], sumx[1]);
            __m128i temp = _mm_min_epu8(out, max);
            temp = _mm_cmpeq_epi8(temp, max);
            out = _mm_or_si128(temp, out);

            temp = _mm_max_epu8(out, min);
            temp = _mm_cmpeq_epi8(temp, min);
            out = _mm_andnot_si128(temp, out);

            _mm_store_si128((__m128i*)(dstp + x), out);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = p3;
        p3 = p4;
        p4 = (p4 == end) ? orig : p4 + bstride;
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

    uint16_t* p0 = (uint16_t *)buff + 8;
    uint16_t* p1 = p0 + bstride;
    uint16_t* p2 = p1 + bstride;
    uint16_t* p3 = p2 + bstride;
    uint16_t* p4 = p3 + bstride;
    uint16_t *orig = p0, *end = p4;

    line_copy16(p0, srcp + 2 * stride, width, 2);
    line_copy16(p1, srcp + stride, width, 2);
    line_copy16(p2, srcp, width, 2);
    srcp += stride;
    line_copy16(p3, srcp, width, 2);

    uint16_t th_min = eh->min > plane_max ? plane_max : (uint16_t)eh->min;
    uint16_t th_max = eh->max > plane_max ? plane_max : (uint16_t)eh->max;

    __m128i zero = _mm_setzero_si128();
    __m128 alpha = _mm_set1_ps((float)0.96043387);
    __m128 beta = _mm_set1_ps((float)0.39782473);
    __m128i pmax = _mm_set1_epi16((int16_t)plane_max);
    __m128i max = _mm_set1_epi16((int16_t)th_max);
    __m128i min = _mm_set1_epi16((int16_t)th_min);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 2 ? 1 : -1);
        line_copy16(p4, srcp, width, 2);
        uint16_t* posh[] = {p2 - 2, p2 - 1, p2 + 1, p2 + 2};
        uint16_t* posv[] = {p0, p1, p3, p4};

        for (int x = 0; x < width; x += 8) {
            __m128i sumx[2] = {zero, zero};
            __m128i sumy[2] = {zero, zero};

            for (int i = 0; i < 4; i++) {
                __m128i xmm0, xmm1, xmul;
                xmul = _mm_load_si128((__m128i *)ar_mulx[i]);
                xmm0 = _mm_loadu_si128((__m128i *)(posh[i] + x));
                xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sumx[0] = _mm_add_epi32(sumx[0], _mm_madd_epi16(xmul, xmm0));
                sumx[1] = _mm_add_epi32(sumx[1], _mm_madd_epi16(xmul, xmm1));

                xmul = _mm_load_si128((__m128i *)ar_muly[i]);
                xmm0 = _mm_load_si128((__m128i *)(posv[i] + x));
                xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sumy[0] = _mm_add_epi32(sumy[0], _mm_madd_epi16(xmul, xmm0));
                sumy[1] = _mm_add_epi32(sumy[1], _mm_madd_epi16(xmul, xmm1));
            }

            for (int i = 0; i < 2; i++) {
                sumx[i] = mm_abs_epi32(sumx[i]);
                sumy[i] = mm_abs_epi32(sumy[i]);
                __m128 t0 = _mm_cvtepi32_ps(mm_max_epi32(sumx[i], sumy[i]));
                __m128 t1 = _mm_cvtepi32_ps(mm_min_epi32(sumx[i], sumy[i]));
                t0 = _mm_add_ps(_mm_mul_ps(alpha, t0), _mm_mul_ps(beta, t1));
                sumx[i] = _mm_cvtps_epi32(t0);
                sumx[i] = _mm_srli_epi32(sumx[i], eh->rshift);
            }

            __m128i out = _mm_packs_epi32(sumx[0], sumx[1]);
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
        p2 = p3;
        p3 = p4;
        p4 = (p4 == end) ? orig : p4 + bstride;
    }
}

static const GF_ALIGN float ar_mulxf[][4] = {
    {  4.0,  4.0,  4.0,  4.0}, { -25.0, -25.0, -25.0, -25.0},
    { 25.0, 25.0, 25.0, 25.0}, {  -4.0,  -4.0,  -4.0,  -4.0}
};
static const GF_ALIGN float ar_mulyf[][4] = {
    {  -4.0,  -4.0,  -4.0,  -4.0}, { 25.0, 25.0, 25.0, 25.0},
    { -25.0, -25.0, -25.0, -25.0}, {  4.0,  4.0,  4.0,  4.0}
};

static void GF_FUNC_ALIGN VS_CC
proc_16bit_sse2(uint8_t *buff, int bstride, int width, int height, int stride,
                uint8_t *d, const uint8_t *s, edge_t *eh, uint16_t plane_max)
{
    const uint16_t *srcp = (uint16_t *)s;
    uint16_t *dstp = (uint16_t *)d;
    stride /= 2;
    bstride /= 2;

    uint16_t* p0 = (uint16_t *)buff + 8;
    uint16_t* p1 = p0 + bstride;
    uint16_t* p2 = p1 + bstride;
    uint16_t* p3 = p2 + bstride;
    uint16_t* p4 = p3 + bstride;
    uint16_t *orig = p0, *end = p4;

    line_copy16(p0, srcp + 2 * stride, width, 2);
    line_copy16(p1, srcp + stride, width, 2);
    line_copy16(p2, srcp, width, 2);
    srcp += stride;
    line_copy16(p3, srcp, width, 2);

    __m128i zero = _mm_setzero_si128();
    __m128 alpha = _mm_set1_ps((float)0.96043387);
    __m128 beta = _mm_set1_ps((float)0.39782473);
    __m128i pmax = _mm_set1_epi32(0xFFFF);
    __m128i min = _mm_set1_epi16((int16_t)eh->min);
    __m128i max = _mm_set1_epi16((int16_t)eh->max);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 2 ? 1 : -1);
        line_copy16(p4, srcp, width, 2);
        uint16_t* posh[] = {p2 - 2, p2 - 1, p2 + 1, p2 + 2};
        uint16_t* posv[] = {p0, p1, p3, p4};

        for (int x = 0; x < width; x += 8) {
            __m128 sumx[2] = {(__m128)zero, (__m128)zero};
            __m128 sumy[2] = {(__m128)zero, (__m128)zero};

            for (int i = 0; i < 4; i++) {
                __m128 xmul = _mm_load_ps(ar_mulxf[i]);
                __m128i xmm0 = _mm_loadu_si128((__m128i *)(posh[i] + x));
                __m128i xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sumx[0] = _mm_add_ps(sumx[0], _mm_mul_ps(_mm_cvtepi32_ps(xmm0), xmul));
                sumx[1] = _mm_add_ps(sumx[1], _mm_mul_ps(_mm_cvtepi32_ps(xmm1), xmul));

                xmul = _mm_load_ps(ar_mulyf[i]);
                xmm0 = _mm_load_si128((__m128i *)(posv[i] + x));
                xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sumy[0] = _mm_add_ps(sumy[0], _mm_mul_ps(_mm_cvtepi32_ps(xmm0), xmul));
                sumy[1] = _mm_add_ps(sumy[1], _mm_mul_ps(_mm_cvtepi32_ps(xmm1), xmul));
            }

            __m128i out[2];
            for (int i = 0; i < 2; i++) {
                sumx[i] = mm_abs_ps(sumx[i]);
                sumy[i] = mm_abs_ps(sumy[i]);
                __m128 t0 = _mm_max_ps(sumx[i], sumy[i]);
                __m128 t1 = _mm_min_ps(sumx[i], sumy[i]);
                t0 = _mm_add_ps(_mm_mul_ps(alpha, t0), _mm_mul_ps(beta, t1));
                out[i] = _mm_srli_epi32(_mm_cvtps_epi32(t0), eh->rshift);
                out[i] = mm_min_epi32(out[i], pmax);
            }
            out[0] = mm_cast_epi32(out[0], out[1]);

            out[1] = MM_MIN_EPU16(out[0], max);
            out[1] = _mm_cmpeq_epi16(out[1], max);
            out[0] = _mm_or_si128(out[1], out[0]);

            out[1] = MM_MAX_EPU16(out[0], min);
            out[1] = _mm_cmpeq_epi16(out[1], min);
            out[0] = _mm_andnot_si128(out[1], out[0]);

            _mm_store_si128((__m128i *)(dstp + x), out[0]);
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = p3;
        p3 = p4;
        p4 = (p4 == end) ? orig : p4 + bstride;
    }
}


const proc_edge_detection tedge[] = {
    proc_8bit_sse2,
    proc_9_10_sse2,
    proc_16bit_sse2
};