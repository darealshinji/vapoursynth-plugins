/*
  merge_frames.c: Copyright (C) 2012-2013  Oka Motofumi

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


static void CM_FUNC_ALIGN VS_CC
merge_frames_all(maskedmerge_t *mh, const VSAPI *vsapi, const VSFrameRef *mask,
                 const VSFrameRef *alt, VSFrameRef *dst)
{
    int err;
    int is_combed = vsapi->propGetInt(vsapi->getFramePropsRO(mask), "_Combed",
                                      0, &err);
    if (err == 0 && is_combed == 0) {
        return;
    }

    int adjust = 16 / mh->vi->format->bytesPerSample;

    for (int p = 0; p < mh->vi->format->numPlanes; p++) {
        if (mh->planes[p] == 0) {
            continue;
        }

        const __m128i *altp = (__m128i *)vsapi->getReadPtr(alt, p);
        const __m128i *maskp = (__m128i *)vsapi->getReadPtr(mask, p);
        __m128i *dstp = (__m128i *)vsapi->getWritePtr(dst, p);

        int width = (vsapi->getFrameWidth(dst, p) + adjust - 1) / adjust;
        int height = vsapi->getFrameHeight(dst, p);
        int stride = vsapi->getStride(dst, p) / adjust;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(dstp + x);
                __m128i xmm1 = _mm_load_si128(altp + x);
                __m128i xmm2 = _mm_load_si128(maskp + x);

                xmm0 = _mm_andnot_si128(xmm2, xmm0);
                xmm1 = _mm_and_si128(xmm2, xmm1);
                xmm0 = _mm_or_si128(xmm0, xmm1);

                _mm_store_si128(dstp + x, xmm0);
            }
            altp += stride;
            maskp += stride;
            dstp += stride;
        }
    }
}


const func_merge_frames merge_frames = merge_frames_all;
