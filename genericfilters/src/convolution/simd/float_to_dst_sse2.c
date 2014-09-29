/*
  float_to_dst_sse2.c: Copyright (C) 2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This file is part of Generic.

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


#include "sse2.h"
#include "canny.h"


static void GF_FUNC_ALIGN VS_CC
float_to_dst_8bit(const float *srcp, uint8_t *dstp, int width, int height,
                  int src_stride, int dst_stride, float th, int bits)
{
    __m128 tmax = _mm_set1_ps(th);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128 xmf0 = _mm_cmpge_ps(_mm_load_ps(srcp + x), tmax);
            __m128 xmf1 = _mm_cmpge_ps(_mm_load_ps(srcp + x + 4), tmax);
            __m128 xmf2 = _mm_cmpge_ps(_mm_load_ps(srcp + x + 8), tmax);
            __m128 xmf3 = _mm_cmpge_ps(_mm_load_ps(srcp + x + 12), tmax);
            __m128i xmi0 = _mm_packs_epi32(_mm_castps_si128(xmf0),
                                           _mm_castps_si128(xmf1));
            __m128i xmi1 = _mm_packs_epi32(_mm_castps_si128(xmf2),
                                           _mm_castps_si128(xmf3));
            xmi0 = _mm_packs_epi16(xmi0, xmi1);
            _mm_store_si128((__m128i *)(dstp + x), xmi0);
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void GF_FUNC_ALIGN VS_CC
float_to_dst_16bit(const float *srcp, uint8_t *d, int width, int height,
                   int src_stride, int dst_stride, float th, int bits)
{
    uint16_t *dstp = (uint16_t *)d;
    dst_stride /= 2;
    __m128 tmax = _mm_set1_ps(th);
    int rshift = 32 - bits;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            __m128 xmf0 = _mm_cmpge_ps(_mm_load_ps(srcp + x), tmax);
            __m128 xmf1 = _mm_cmpge_ps(_mm_load_ps(srcp + x + 4), tmax);

            __m128i xmi0 = _mm_srli_epi32(_mm_castps_si128(xmf0), rshift);
            __m128i xmi1 = _mm_srli_epi32(_mm_castps_si128(xmf1), rshift);
            xmi0 = mm_cast_epi32(xmi0, xmi1);
            _mm_store_si128((__m128i *)(dstp + x), xmi0);
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void GF_FUNC_ALIGN VS_CC
float_to_dst_gb_8bit(const float *srcp, uint8_t *dstp, int width, int height,
                    int src_stride, int dst_stride, float th, int bits)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i xmm0 = _mm_cvtps_epi32(_mm_load_ps(srcp + x));
            __m128i xmm1 = _mm_cvtps_epi32(_mm_load_ps(srcp + x + 4));
            __m128i xmm2 = _mm_cvtps_epi32(_mm_load_ps(srcp + x + 8));
            __m128i xmm3 = _mm_cvtps_epi32(_mm_load_ps(srcp + x + 12));
            xmm0 = _mm_packus_epi16(_mm_packs_epi32(xmm0, xmm1),
                                    _mm_packs_epi32(xmm2, xmm3));
            _mm_store_si128((__m128i *)(dstp + x), xmm0);
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void GF_FUNC_ALIGN VS_CC
float_to_dst_gb_16bit(const float *srcp, uint8_t *d, int width, int height,
                      int src_stride, int dst_stride, float th, int bits)
{
    uint16_t *dstp = (uint16_t *)d;
    dst_stride /= 2;
    __m128i tmax = _mm_set1_epi32((1 << bits) - 1);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            __m128i xmm0 = _mm_cvtps_epi32(_mm_load_ps(srcp + x));
            __m128i xmm1 = _mm_cvtps_epi32(_mm_load_ps(srcp + x + 4));
            xmm0 = _mm_packs_epi32(mm_min_epi32(tmax, xmm0),
                                   mm_min_epi32(tmax, xmm1));

            _mm_store_si128((__m128i *)(dstp + x), xmm0);
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


const write_dst write_dst_canny[] = {
    float_to_dst_8bit, float_to_dst_16bit
};

const write_dst write_dst_gblur[] = {
    float_to_dst_gb_8bit, float_to_dst_gb_16bit
};
