/*
  proc_v_sse2.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include "convolution.h"
#include "sse2.h"


static void GF_FUNC_ALIGN VS_CC
proc_8bit_sse2(convolution_t *ch, uint8_t *buff, int bstride, int width,
               int height, int stride, uint8_t *dstp, const uint8_t *srcp)
{
    const uint8_t *p[17] = {NULL};
    int num = ch->length;
    int border = num / 2;

    for (int i = 0; i <= border; i++) {
        p[i] = srcp + (border - i) * stride;
    }
    for (int i = border + 1; i < num; i++) {
        p[i] = p[i - 1] + stride;
    }

    __m128i zero = _mm_setzero_si128();
    __m128 rdiv = _mm_set1_ps((float)ch->rdiv);
    __m128 bias = _mm_set1_ps((float)ch->bias);
    GF_ALIGN int32_t ar_matrix[17][4];
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < 4; j++) {
            ar_matrix[i][j] = ch->m[i];
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i sum[4] = { zero, zero, zero, zero };

            for (int i = 0; i < num; i++) {
                __m128i xmm0, xmm1, xmm2;
                __m128i matrix = _mm_load_si128((__m128i *)ar_matrix[i]);

                xmm0 = _mm_load_si128((__m128i *)(p[i] + x));
                xmm2 = _mm_unpackhi_epi8(xmm0, zero);
                xmm0 = _mm_unpacklo_epi8(xmm0, zero);

                xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sum[0] = _mm_add_epi32(sum[0], _mm_madd_epi16(xmm0, matrix));
                sum[1] = _mm_add_epi32(sum[1], _mm_madd_epi16(xmm1, matrix));

                xmm1 = _mm_unpackhi_epi16(xmm2, zero);
                xmm0 = _mm_unpacklo_epi16(xmm2, zero);
                sum[2] = _mm_add_epi32(sum[2], _mm_madd_epi16(xmm0, matrix));
                sum[3] = _mm_add_epi32(sum[3], _mm_madd_epi16(xmm1, matrix));
            }

            for (int i = 0; i < 4; i++) {
                __m128 sumfp = _mm_cvtepi32_ps(sum[i]);
                sumfp = _mm_mul_ps(sumfp, rdiv);
                sumfp = _mm_add_ps(sumfp, bias);
                if (!ch->saturate) {
                    sumfp = mm_abs_ps(sumfp);
                }
                sum[i] = _mm_cvttps_epi32(sumfp);
            }

            sum[0] = _mm_packs_epi32(sum[0], sum[1]);
            sum[1] = _mm_packs_epi32(sum[2], sum[3]);
            sum[0] = _mm_packus_epi16(sum[0], sum[1]);

            _mm_store_si128((__m128i *)(dstp + x), sum[0]);
        }
        dstp += stride;
        for (int i = 0; i < num - 1; i++) {
            p[i] = p[i + 1];
        }
        p[num - 1] += stride * (y < height - border - 1 ? 1 : -1);
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_9_10_sse2(convolution_t *ch, uint8_t *buff, int bstride, int width,
               int height, int stride, uint8_t *d, const uint8_t *s)
{
    const uint16_t *srcp = (uint16_t *)s;
    uint16_t *dstp = (uint16_t *)d;
    stride /= 2;

    const uint16_t *p[17] = {NULL};
    int num = ch->length;
    int border = num / 2;
    for (int i = 0; i <= border; i++) {
        p[i] = srcp + (border - i) * stride;
    }
    for (int i = border + 1; i < num; i++) {
        p[i] = p[i - 1] + stride;
    }

    __m128i zero = _mm_setzero_si128();
    __m128 rdiv = _mm_set1_ps((float)ch->rdiv);
    __m128 bias = _mm_set1_ps((float)ch->bias);
    GF_ALIGN int32_t ar_matrix[17][4];
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < 4; j++) {
            ar_matrix[i][j] = ch->m[i];
        }
    }

    for (int y = 0; y < height; y++) {

        for (int x = 0; x < width; x += 8) {
            __m128i sum[2] = { zero, zero };

            for (int i = 0; i < num; i++) {
                __m128i matrix = _mm_load_si128((__m128i *)ar_matrix[i]);
                __m128i xmm0, xmm1;

                xmm0 = _mm_load_si128((__m128i *)(p[i] + x));
                xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                sum[0] = _mm_add_epi32(sum[0], _mm_madd_epi16(xmm0, matrix));
                sum[1] = _mm_add_epi32(sum[1], _mm_madd_epi16(xmm1, matrix));
            }

            for (int i = 0; i < 2; i++) {
                __m128 sumfp = _mm_cvtepi32_ps(sum[i]);
                sumfp = _mm_mul_ps(sumfp, rdiv);
                sumfp = _mm_add_ps(sumfp, bias);
                if (!ch->saturate) {
                    sumfp = mm_abs_ps(sumfp);
                }
                sum[i] = _mm_cvttps_epi32(sumfp);
            }

            sum[0] = _mm_packs_epi32(sum[0], sum[1]);
            __m128i mask = _mm_cmpgt_epi16(sum[0], zero);
            sum[0] = _mm_and_si128(sum[0], mask);

            _mm_store_si128((__m128i *)(dstp + x), sum[0]);
        }
        dstp += stride;
        for (int i = 0; i < num - 1; i++) {
            p[i] = p[i + 1];
        }
        p[num - 1] = (y < height - border - 1) ? p[num - 1] + stride :
                                                 p[num - 1] - stride;
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_16bit_sse2(convolution_t *ch, uint8_t *buff, int bstride, int width,
                  int height, int stride, uint8_t *d, const uint8_t *s)
{
    const uint16_t *srcp = (uint16_t *)s;
    uint16_t *dstp = (uint16_t *)d;
    stride /= 2;

    const uint16_t *p[17] = {NULL};
    int num = ch->length;
    int border = num / 2;

    for (int i = 0; i <= border; i++) {
        p[i] = srcp + (border - i) * stride;
    }
    for (int i = border + 1; i < num; i++) {
        p[i] = p[i - 1] + stride;
    }

    __m128i zero = _mm_setzero_si128();
    __m128i max = _mm_set1_epi32(0xFFFF);
    __m128 rdiv = _mm_set1_ps((float)ch->rdiv);
    __m128 bias = _mm_set1_ps((float)ch->bias);
    GF_ALIGN float ar_matrix[17][4];
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < 4; j++) {
            ar_matrix[i][j] = (float)ch->m[i];
        }
    }

    for (int y = 0; y < height; y++) {

        for (int x = 0; x < width; x += 8) {
            __m128 sum[2] = {(__m128)zero, (__m128)zero};

            for (int i = 0; i < num; i++) {
                __m128 matrix = _mm_load_ps(ar_matrix[i]);
                __m128i xmm0, xmm1;

                xmm0 = _mm_load_si128((__m128i *)(p[i] + x));
                xmm1 = _mm_unpackhi_epi16(xmm0, zero);
                xmm0 = _mm_unpacklo_epi16(xmm0, zero);
                __m128 t0 = _mm_mul_ps(matrix, _mm_cvtepi32_ps(xmm0));
                __m128 t1 = _mm_mul_ps(matrix, _mm_cvtepi32_ps(xmm1));
                sum[0] = _mm_add_ps(sum[0], t0);
                sum[1] = _mm_add_ps(sum[1], t1);
            }

            __m128i out[2];
            for (int i = 0; i < 2; i++) {
                sum[i]  = _mm_mul_ps(sum[i], rdiv);
                sum[i]  = _mm_add_ps(sum[i], bias);
                if (!ch->saturate) {
                    sum[i] = mm_abs_ps(sum[i]);
                }
                out[i] = _mm_cvttps_epi32(sum[i]);
                out[i] = mm_min_epi32(out[i], max);
                __m128i mask = _mm_cmpgt_epi32(out[i], zero);
                out[i] = _mm_and_si128(mask, out[i]);
            }

            out[0] = mm_cast_epi32(out[0], out[1]);

            _mm_store_si128((__m128i *)(dstp + x), out[0]);
        }

        dstp += stride;
        for (int i = 0; i < num - 1; i++) {
            p[i] = p[i + 1];
        }
        p[num - 1] += stride * (y < height - border - 1 ? 1 : -1);
    }
}


const proc_convolution convo_v[] = {
    proc_8bit_sse2,
    proc_9_10_sse2,
    proc_16bit_sse2
};
