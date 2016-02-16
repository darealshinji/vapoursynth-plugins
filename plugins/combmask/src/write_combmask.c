/*
  write_combmask.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include <stdint.h>
#include <string.h>
#include <emmintrin.h>
#include "combmask.h"


static void CM_FUNC_ALIGN VS_CC
write_combmask_8bit(combmask_t *ch, const VSAPI *vsapi, const VSFrameRef *src,
                    VSFrameRef *cmask)
{
    __m128i xcth = _mm_set1_epi8((int8_t)ch->cthresh);
    __m128i xct6 = _mm_set1_epi16((int16_t)(ch->cthresh * 6));
    __m128i zero = _mm_setzero_si128();

    for (int p = 0; p < ch->vi->format->numPlanes; p++) {

        __m128i* dstp = (__m128i*)vsapi->getWritePtr(cmask, p);

        int height = vsapi->getFrameHeight(src, p);
        int stride = vsapi->getStride(src, p) / 16;

        if (ch->planes[p] == 0 || height < 3) {
            memset(dstp, 0, stride * height * 16);
            continue;
        }

        int width = (vsapi->getFrameWidth(src, p) + 15) / 16;

        const __m128i* srcpc = (__m128i*)vsapi->getReadPtr(src, p);
        const __m128i* srcpb = srcpc + stride;
        const __m128i* srcpa = srcpb + stride;
        const __m128i* srcpd = srcpc + stride;
        const __m128i* srcpe = srcpd + stride;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(srcpc + x);
                __m128i xmm1 = _mm_load_si128(srcpb + x);
                __m128i xmm2 = _mm_load_si128(srcpd + x);

                __m128i xmm3 = _mm_subs_epu8(xmm0, _mm_max_epu8(xmm1, xmm2));
                xmm3 = _mm_cmpeq_epi8(zero, _mm_subs_epu8(xmm3, xcth)); // !(d1 > cthresh && d2 > cthresh)

                __m128i xmm4 = _mm_subs_epu8(_mm_min_epu8(xmm1, xmm2), xmm0);
                xmm4 = _mm_cmpeq_epi8(zero, _mm_subs_epu8(xmm4, xcth)); // !(d1 < -cthresh && d2 < -cthresh)

                xmm3 = _mm_and_si128(xmm3, xmm4);

                xmm4 = _mm_add_epi16(_mm_unpacklo_epi8(xmm1, zero),
                                     _mm_unpacklo_epi8(xmm2, zero)); // lo of (b+d)
                xmm1 = _mm_add_epi16(_mm_unpackhi_epi8(xmm1, zero),
                                     _mm_unpackhi_epi8(xmm2, zero)); // hi of (b+d)
                xmm4 = _mm_add_epi16(xmm4, _mm_add_epi16(xmm4, xmm4));      // lo of 3*(b+d)
                xmm1 = _mm_add_epi16(xmm1, _mm_add_epi16(xmm1, xmm1));      // hi of 3*(b+d)
                
                xmm2 = _mm_load_si128(srcpa + x);
                __m128i xmm5 = _mm_add_epi16(_mm_slli_epi16(_mm_unpacklo_epi8(xmm0, zero), 2),
                                             _mm_unpacklo_epi8(xmm2, zero));
                xmm2 = _mm_add_epi16(_mm_slli_epi16(_mm_unpackhi_epi8(xmm0, zero), 2),
                                     _mm_unpackhi_epi8(xmm2, zero));
                
                xmm0 = _mm_load_si128(srcpe + x);
                xmm5 = _mm_add_epi16(xmm5, _mm_unpacklo_epi8(xmm0, zero));
                xmm2 = _mm_add_epi16(xmm2, _mm_unpackhi_epi8(xmm0, zero));
                
                xmm0 = _mm_max_epi16(xmm4, xmm5);
                xmm4 = _mm_min_epi16(xmm4, xmm5);
                xmm0 = _mm_sub_epi16(xmm0, xmm4);
                xmm0 = _mm_cmpgt_epi16(xmm0, xct6);
                
                xmm4 = _mm_max_epi16(xmm1, xmm2);
                xmm1 = _mm_min_epi16(xmm1, xmm2);
                xmm4 = _mm_sub_epi16(xmm4, xmm1);
                xmm4 = _mm_cmpgt_epi16(xmm4, xct6);
                
                xmm1 = _mm_packs_epi16(xmm0, xmm4);

                xmm3 = _mm_andnot_si128(xmm3, xmm1);

                _mm_store_si128(dstp + x, xmm3);
            }
            dstp += stride;
            srcpa = srcpb;
            srcpb = srcpc;
            srcpc = srcpd;
            srcpd = srcpe;
            srcpe = (y < height - 3) ? srcpe + stride : srcpe - stride;
        }
    }
}


static void CM_FUNC_ALIGN VS_CC
write_combmask_9_10(combmask_t *ch, const VSAPI *vsapi, const VSFrameRef *src,
                    VSFrameRef *cmask)
{
    __m128i xcth = _mm_set1_epi16((int16_t)ch->cthresh);
    __m128i xct6p = _mm_set1_epi32((int16_t)(ch->cthresh * 6));
    __m128i xct6n = _mm_set1_epi32((int16_t)(ch->cthresh * -6));
    int shift = 16 - ch->vi->format->bitsPerSample;

    for (int p = 0; p < ch->vi->format->numPlanes; p++) {

        __m128i* dstp = (__m128i*)vsapi->getWritePtr(cmask, p);

        int height = vsapi->getFrameHeight(src, p);
        int stride = vsapi->getStride(cmask, p) / 16;

        if (ch->planes[p] == 0 || height < 3) {
            memset(dstp, 0, stride * height * 16);
            continue;
        }

        int width = (vsapi->getFrameWidth(src, p) + 7) / 8;

        const __m128i* srcpc = (__m128i*)vsapi->getReadPtr(src, p);
        const __m128i* srcpb = srcpc + stride;
        const __m128i* srcpa = srcpb + stride;
        const __m128i* srcpd = srcpc + stride;
        const __m128i* srcpe = srcpd + stride;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(srcpc + x);
                __m128i xmm1 = _mm_load_si128(srcpb + x);
                __m128i xmm2 = _mm_load_si128(srcpd + x);

                __m128i xmm3 = _mm_cmpgt_epi16(_mm_sub_epi16(xmm0, xmm1), xcth);
                __m128i xmm4 = _mm_cmpgt_epi16(_mm_sub_epi16(xmm0, xmm2), xcth);
                xmm3 = _mm_and_si128(xmm3, xmm4);

                xmm4 = _mm_cmpgt_epi16(_mm_sub_epi16(xmm1, xmm0), xcth);
                __m128i xmm5 = _mm_cmpgt_epi16(_mm_sub_epi16(xmm2, xmm0), xcth);
                xmm4 = _mm_and_si128(xmm4, xmm5);

                xmm3 = _mm_and_si128(xmm3, xmm4);

                xmm1 = _mm_add_epi16(xmm1, xmm2); // b + d
                xmm1 = _mm_add_epi16(xmm1, _mm_add_epi16(xmm1, xmm1)); //3 * (b + d)
                xmm0 = _mm_slli_epi16(xmm0, 2); // 4 * c

                xmm2 = _mm_add_epi16(_mm_load_si128(srcpa + x),
                                     _mm_load_si128(srcpe + x)); // a + e
                xmm0 = _mm_add_epi16(xmm0, xmm2); // a + 4 * c + e
                xmm0 = _mm_sub_epi16(xmm0, xmm1); // a+4*c+e-3*(b+d)

                xmm1 = _mm_cmpgt_epi16(xmm0, xct6p);
                xmm0 = _mm_cmplt_epi16(xmm0, xct6n);

                xmm0 = _mm_or_si128(xmm0, xmm1);

                xmm0 = _mm_srli_epi16(_mm_and_si128(xmm0, xmm3), shift);

                _mm_store_si128(dstp + x, xmm0);
            }
            dstp += stride;
            srcpa = srcpb;
            srcpb = srcpc;
            srcpc = srcpd;
            srcpd = srcpe;
            srcpe = (y < height - 3) ? srcpe + stride : srcpe - stride;
        }
    }
}


static void CM_FUNC_ALIGN VS_CC
write_combmask_16bit(combmask_t *ch, const VSAPI *vsapi, const VSFrameRef *src,
                     VSFrameRef *cmask)
{
    __m128i xcth = _mm_set1_epi16((int16_t)ch->cthresh);
    __m128i xct6p = _mm_set1_epi16((int16_t)(ch->cthresh * 6));
    __m128i xct6n = _mm_set1_epi16((int16_t)(ch->cthresh * -6));
    __m128i zero = _mm_setzero_si128();

    for (int p = 0; p < ch->vi->format->numPlanes; p++) {

        __m128i* dstp = (__m128i*)vsapi->getWritePtr(cmask, p);

        int height = vsapi->getFrameHeight(src, p);
        int stride = vsapi->getStride(cmask, p) / 16;

        if (ch->planes[p] == 0 || height < 3) {
            memset(dstp, 0, stride * height * 16);
            continue;
        }

        int width = (vsapi->getFrameWidth(src, p) + 7) / 8;

        const __m128i* srcpc = (__m128i*)vsapi->getReadPtr(src, p);
        const __m128i* srcpb = srcpc + stride;
        const __m128i* srcpa = srcpb + stride;
        const __m128i* srcpd = srcpc + stride;
        const __m128i* srcpe = srcpd + stride;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(srcpc + x);
                __m128i xmm1 = _mm_load_si128(srcpb + x);
                __m128i xmm2 = _mm_load_si128(srcpd + x);
                
                __m128i xmm3 = _mm_subs_epu16(xmm0, MM_MAX_EPU16(xmm1, xmm2));
                xmm3 = _mm_cmpeq_epi16(zero, _mm_subs_epu16(xmm3, xcth)); // !(d1 > cthresh && d2 > cthresh)

                __m128i xmm4 = _mm_subs_epu16(MM_MIN_EPU16(xmm1, xmm2), xmm0);
                xmm4 = _mm_cmpeq_epi16(zero, _mm_subs_epu16(xmm4, xcth)); // !(d1 < -cthresh && d2 < -cthresh)

                xmm3 = _mm_and_si128(xmm3, xmm4);

                xmm4 = _mm_add_epi32(_mm_unpacklo_epi16(xmm1, zero),
                                     _mm_unpacklo_epi16(xmm2, zero)); // lo of (b+d)
                xmm1 = _mm_add_epi32(_mm_unpackhi_epi16(xmm1, zero),
                                     _mm_unpackhi_epi16(xmm2, zero)); // hi of (b+d)
                xmm4 = _mm_add_epi32(xmm4, _mm_add_epi32(xmm4, xmm4));      // lo of 3*(b+d)
                xmm1 = _mm_add_epi32(xmm1, _mm_add_epi32(xmm1, xmm1));      // hi of 3*(b+d)

                xmm4 = _mm_sub_epi32(_mm_slli_epi32(_mm_unpacklo_epi16(xmm0, zero), 2), xmm4); // lo of 4*c-3*(b+d)
                xmm1 = _mm_sub_epi32(_mm_slli_epi32(_mm_unpackhi_epi16(xmm0, zero), 2), xmm1); // hi of 4*c-3*(b+d)

                xmm0 = _mm_load_si128(srcpa + x);
                xmm4 = _mm_add_epi32(xmm4, _mm_unpacklo_epi16(xmm0, zero)); // lo of a+4*c-3*(b+d)
                xmm1 = _mm_add_epi32(xmm1, _mm_unpackhi_epi16(xmm0, zero)); // hi of a+4*c-3*(b+d)

                xmm0 = _mm_load_si128(srcpe + x);
                xmm4 = _mm_add_epi32(xmm4, _mm_unpacklo_epi16(xmm0, zero)); // lo of a+4*c+e-3*(b+d)
                xmm1 = _mm_add_epi32(xmm1, _mm_unpackhi_epi16(xmm0, zero)); // hi of a+4*c+e-3*(b+d)

                xmm4 = _mm_or_si128(_mm_cmpgt_epi32(xmm4, xct6p),
                                    _mm_cmplt_epi32(xmm4, xct6n));
                xmm1 = _mm_or_si128(_mm_cmpgt_epi32(xmm1, xct6p),
                                    _mm_cmplt_epi32(xmm1, xct6n));
                xmm1 = _mm_packs_epi32(xmm4, xmm1);

                xmm3 = _mm_andnot_si128(xmm3, xmm1);

                _mm_store_si128(dstp + x, xmm3);
            }
            dstp += stride;
            srcpa = srcpb;
            srcpb = srcpc;
            srcpc = srcpd;
            srcpd = srcpe;
            srcpe = (y < height - 3) ? srcpe + stride : srcpe - stride;
        }
    }

}


const func_write_combmask write_combmask_funcs[] = {
    write_combmask_8bit,
    write_combmask_9_10,
    write_combmask_16bit
};
