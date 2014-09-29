/*
  adapt_motion.c: Copyright (C) 2012-2013  Oka Motofumi

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

#define USE_ALIGNED_MALLOC
#include "combmask.h"


static void CM_FUNC_ALIGN VS_CC
vertical_proc(__m128i *center, int width, int height, __m128i *dst)
{
    __m128i *top = center + width;
    __m128i *bottom = top;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            __m128i xmm0 = _mm_load_si128(top + x);
            __m128i xmm1 = _mm_load_si128(center + x);
            __m128i xmm2 = _mm_load_si128(bottom + x);
            xmm0 = _mm_or_si128(xmm0, xmm2);
            xmm1 = _mm_or_si128(xmm1, xmm0);
            _mm_store_si128(dst + x, xmm1);
        }
        top = center;
        center = bottom;
        bottom = y < height - 2 ? bottom + width : bottom - width;
        dst += width;
    }
}


static void CM_FUNC_ALIGN VS_CC
write_motionmask_8bit(int mthresh, int width, int height, int stride,
                      __m128i *maskp, __m128i *srcp, __m128i *prevp)
{
    __m128i xmth = _mm_set1_epi8((int8_t)mthresh);
    __m128i zero = _mm_setzero_si128();
    __m128i all1 = _mm_cmpeq_epi32(zero, zero);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            __m128i xmm0 = _mm_load_si128(srcp + x);
            __m128i xmm1 = _mm_load_si128(prevp + x);

            __m128i xmm2 = _mm_max_epu8(xmm0, xmm1);
            xmm0 = _mm_min_epu8(xmm0, xmm1);
            xmm2 = _mm_subs_epu8(xmm2, xmm0);
            xmm2 = _mm_subs_epu8(xmm2, xmth);
            xmm2 = _mm_cmpeq_epi8(xmm2, zero);
            xmm2 = _mm_xor_si128(xmm2, all1);

            _mm_store_si128(maskp + x, xmm2);
        }
        srcp += stride;
        prevp += stride;
        maskp += width;
    }
}


static void CM_FUNC_ALIGN VS_CC
write_motionmask_9_10(int mthresh, int width, int height, int stride,
                      __m128i *maskp, __m128i *srcp, __m128i *prevp)
{
    __m128i xmth = _mm_set1_epi16((int16_t)mthresh);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            __m128i xmm0 = _mm_load_si128(srcp + x);
            __m128i xmm1 = _mm_load_si128(prevp + x);

            __m128i xmm2 = _mm_max_epi16(xmm0, xmm1);
            xmm0 = _mm_min_epi16(xmm0, xmm1);
            xmm2 = _mm_sub_epi16(xmm2, xmm0);
            xmm2 = _mm_cmpgt_epi16(xmm2, xmth);

            _mm_store_si128(maskp, xmm2);
        }
        srcp += stride;
        prevp += stride;
        maskp += width;
    }
}


static void CM_FUNC_ALIGN VS_CC
write_motionmask_16bit(int mthresh, int width, int height, int stride,
                       __m128i *maskp, __m128i *srcp, __m128i *prevp)
{
    __m128i xmth = _mm_set1_epi16((int16_t)mthresh);
    __m128i zero = _mm_setzero_si128();
    __m128i all1 = _mm_cmpeq_epi32(zero, zero);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            __m128i xmm0 = _mm_load_si128(srcp + x);
            __m128i xmm1 = _mm_load_si128(prevp + x);

            __m128i xmm2 = MM_MAX_EPU16(xmm0, xmm1);
            xmm0 = MM_MIN_EPU16(xmm0, xmm1);
            xmm2 = _mm_subs_epu16(xmm2, xmm0);
            xmm2 = _mm_subs_epu16(xmm2, xmth);
            xmm2 = _mm_cmpeq_epi16(xmm2, zero);
            xmm2 = _mm_xor_si128(xmm2, all1);

            _mm_store_si128(maskp, xmm2);
        }
        srcp += stride;
        prevp += stride;
        maskp += width;
    }
}


static void CM_FUNC_ALIGN VS_CC
adapt_motion_all(combmask_t *ch, const VSAPI *vsapi, const VSFrameRef *src,
                 const VSFrameRef *prev, VSFrameRef *cmask)
{
    int adjust = 16 / ch->vi->format->bytesPerSample;

    for (int p = 0; p < ch->vi->format->numPlanes; p++) {
        if (ch->planes[p] == 0) {
            continue;
        }

        int stride = vsapi->getStride(cmask, p) / 16;
        int width = (vsapi->getFrameWidth(cmask, p) + adjust - 1) / adjust;
        int height = vsapi->getFrameHeight(cmask, p);

        __m128i *mmtemp = (__m128i *)_aligned_malloc(width * height * 16 * 2, 16);
        __m128i *mmaskp = mmtemp + width * height;

        ch->write_motionmask(ch->mthresh, width, height, stride, mmtemp,
                             (__m128i *)vsapi->getReadPtr(src, p),
                             (__m128i *)vsapi->getReadPtr(prev, p));
        vertical_proc(mmtemp, width, height, mmaskp);

        __m128i *cmaskp = (__m128i *)vsapi->getWritePtr(cmask, p);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(cmaskp + x);
                __m128i xmm1 = _mm_load_si128(mmaskp + x);
                xmm0 = _mm_and_si128(xmm0, xmm1);
                _mm_store_si128(cmaskp + x, xmm0);
            }
            cmaskp += stride;
            mmaskp += width;
        }
        _aligned_free(mmtemp);
    }
}


const func_adapt_motion adapt_motion = adapt_motion_all;

const func_write_motionmask write_motionmask_funcs[] = {
    write_motionmask_8bit,
    write_motionmask_9_10,
    write_motionmask_16bit
};
