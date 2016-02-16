/*
  is_combed.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include <emmintrin.h>
#include "combmask.h"

#ifdef _MSC_VER
#define CM_ALIGN __declspec(align(16))
#else
#define CM_ALIGN __attribute__((aligned(16)))
#endif


static int CM_FUNC_ALIGN VS_CC
is_combed_8bit(combmask_t *ch, VSFrameRef *cmask, const VSAPI *vsapi)
{
    int mi = ch->mi;
    int p = ch->planes[0] ? 0 : ch->planes[1] ? 1 : 2;

    int width = vsapi->getFrameWidth(cmask, p) / 16;
    int height = vsapi->getFrameHeight(cmask, p) / 16;
    int stride_0 = vsapi->getStride(cmask, p) / 16;
    int stride_1 = stride_0 * 16;

    const __m128i *srcp = (__m128i*)vsapi->getReadPtr(cmask, p);

    __m128i zero = _mm_setzero_si128();
    __m128i all1 = _mm_cmpeq_epi32(zero, zero);
    __m128i one = _mm_set1_epi8((char)1);

    CM_ALIGN int64_t array[2];
    __m128i *arr = (__m128i *)array;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            __m128i sum = zero;

            for (int i = 0; i < 16; i++) {
                // 0xFF == -1, thus the range of each bytes of sum is -16 to 0.
                __m128i xmm0 = _mm_load_si128(srcp + x + stride_0 * i);
                sum = _mm_add_epi8(sum, xmm0);
            }

            sum = _mm_xor_si128(sum, all1);
            sum = _mm_add_epi8(sum, one);       // -x = ~x + 1
            sum = _mm_sad_epu8(sum, zero);
            _mm_store_si128(arr, sum);

            if (array[0] > mi || array[1] > mi) {
                ch->horizontal_dilation(ch, cmask, vsapi);
                return 1;
            }
        }
        srcp += stride_1;
    }

    return 0;
}


static int CM_FUNC_ALIGN VS_CC
is_combed_9_10(combmask_t *ch, VSFrameRef *cmask, const VSAPI *vsapi)
{
    int mi = ch->mi;
    int p = ch->planes[0] ? 0 : ch->planes[1] ? 1 : 2;

    int width = vsapi->getFrameWidth(cmask, p) / 8;
    int height = vsapi->getFrameHeight(cmask, p) / 16;
    int stride_0 = vsapi->getStride(cmask, p) / 16;
    int stride_1 = stride_0 * 16;

    const __m128i *srcp = (__m128i*)vsapi->getReadPtr(cmask, p);

    __m128i zero = _mm_setzero_si128();
    __m128i one = _mm_srli_epi16(_mm_cmpeq_epi32(zero, zero), 15);

    CM_ALIGN int64_t array[2];
    __m128i *arr = (__m128i *)array;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            __m128i sum = zero;

            for (int i = 0; i < 16; i++) {
                __m128i xmm0 = _mm_load_si128(srcp + x + stride_0 * i);
                xmm0 = _mm_and_si128(xmm0, one);
                sum = _mm_add_epi16(sum, xmm0);
            }

            sum = _mm_sad_epu8(sum, zero);
            _mm_store_si128(arr, sum);

            if (array[0] + array[1] > mi) {
                ch->horizontal_dilation(ch, cmask, vsapi);
                return 1;
            }
        }
        srcp += stride_1;
    }

    return 0;
}


static int CM_FUNC_ALIGN VS_CC
is_combed_16bit(combmask_t *ch, VSFrameRef *cmask, const VSAPI *vsapi)
{
    int mi = ch->mi;
    int p = ch->planes[0] ? 0 : ch->planes[1] ? 1 : 2;

    int width = vsapi->getFrameWidth(cmask, p) / 8;
    int height = vsapi->getFrameHeight(cmask, p) / 16;
    int stride_0 = vsapi->getStride(cmask, p) / 16;
    int stride_1 = stride_0 * 16;

    const __m128i *srcp = (__m128i*)vsapi->getReadPtr(cmask, p);

    __m128i zero = _mm_setzero_si128();
    __m128i all1 = _mm_cmpeq_epi32(zero, zero);
    __m128i one = _mm_srli_epi16(all1, 15);

    CM_ALIGN int64_t array[2];
    __m128i *arr = (__m128i *)array;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            __m128i sum = zero;

            for (int i = 0; i < 16; i++) {
                // 0xFFFF == -1, thus the range of each 2bytes of sum is -16 to 0.
                __m128i xmm0 = _mm_load_si128(srcp + x + stride_0 * i);
                sum = _mm_add_epi16(sum, xmm0);
            }

            sum = _mm_xor_si128(sum, all1);
            sum = _mm_add_epi16(sum, one);       // -x = ~x + 1
            sum = _mm_sad_epu8(sum, zero);
            _mm_store_si128(arr, sum);

            if (array[0] + array[1] > mi) {
                ch->horizontal_dilation(ch, cmask, vsapi);
                return 1;
            }
        }
        srcp += stride_1;
    }

    return 0;
}


const func_is_combed is_combed_funcs[] = {
    is_combed_8bit,
    is_combed_9_10,
    is_combed_16bit
};
