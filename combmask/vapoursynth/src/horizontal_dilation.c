/*
  horizontal_dilation.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include <string.h>
#include <emmintrin.h>

#define USE_ALIGNED_MALLOC
#include "combmask.h"


static void CM_FUNC_ALIGN VS_CC
horizontal_dilation_8bit(combmask_t *ch, VSFrameRef *cmask, const VSAPI *vsapi)
{
    uint8_t *tmp = (uint8_t *)_aligned_malloc(((ch->vi->width + 63) / 32) * 32, 16);
    uint8_t *buff = tmp + 16;

    for (int p = 0; p < ch->vi->format->numPlanes; p++) {
        if (ch->planes[p] == 0) {
            continue;
        }

        int width = vsapi->getFrameWidth(cmask, p);
        int height = vsapi->getFrameHeight(cmask, p);
        int stride = vsapi->getStride(cmask, p);
        uint8_t *maskp = vsapi->getWritePtr(cmask, p);

        for (int y = 0; y < height; y++) {
            memcpy(buff, maskp, stride);
            buff[-1] = buff[0];
            buff[width] = buff[width - 1];

            for (int x = 0; x < width; x += 16) {
                __m128i xmm0 = _mm_load_si128((__m128i*)(buff + x));
                __m128i xmm1 = _mm_loadu_si128((__m128i*)(buff + x + 1));
                __m128i xmm2 = _mm_loadu_si128((__m128i*)(buff + x - 1));
                xmm0 = _mm_or_si128(xmm0, _mm_or_si128(xmm1, xmm2));
                _mm_store_si128((__m128i*)(maskp + x), xmm0);
            }
            maskp += stride;
        }
    }

    _aligned_free(tmp);
}


static void CM_FUNC_ALIGN VS_CC
horizontal_dilation_16bit(combmask_t *ch, VSFrameRef *cmask, const VSAPI *vsapi)
{
    uint16_t *tmp = (uint16_t *)_aligned_malloc(((ch->vi->width * 2 + 63) / 32) * 32, 16);
    uint16_t *buff = tmp + 8;

    for (int p = 0; p < ch->vi->format->numPlanes; p++) {
        if (ch->planes[p] == 0) {
            continue;
        }

        int width = vsapi->getFrameWidth(cmask, p);
        int height = vsapi->getFrameHeight(cmask, p);
        int stride = vsapi->getStride(cmask, p) / 2;
        uint16_t *maskp = (uint16_t *)vsapi->getWritePtr(cmask, p);

        for (int y = 0; y < height; y++) {
            memcpy(buff, maskp, stride * 2);
            buff[-1] = buff[0];
            buff[width] = buff[width - 1];

            for (int x = 0; x < width; x += 8) {
                __m128i xmm0 = _mm_load_si128((__m128i*)(buff + x));
                __m128i xmm1 = _mm_loadu_si128((__m128i*)(buff + x + 1));
                __m128i xmm2 = _mm_loadu_si128((__m128i*)(buff + x - 1));
                xmm0 = _mm_or_si128(xmm0, _mm_or_si128(xmm1, xmm2));
                _mm_store_si128((__m128i*)(maskp + x), xmm0);
            }
            maskp += stride;
        }
    }

    _aligned_free(tmp);
}


const func_h_dilation h_dilation_funcs[] = {
    horizontal_dilation_8bit,
    horizontal_dilation_16bit,
    horizontal_dilation_16bit
};
