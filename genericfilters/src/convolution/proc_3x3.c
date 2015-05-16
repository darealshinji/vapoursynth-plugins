/*
  proc_3x3.c: Copyright (C) 2012-2013  Oka Motofumi

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


#ifdef USE_X86_INTRINSICS
#include "simd/proc_3x3_sse2.c"
#else


#include <stdlib.h>
#include "convolution.h"
#include "no_simd.h"


static void VS_CC
proc_8bit(convolution_t *ch, uint8_t *buff, int bstride, int width,
          int height, int stride, uint8_t *dstp, const uint8_t *srcp)
{
    uint8_t *p0 = buff + 16;
    uint8_t *p1 = p0 + bstride;
    uint8_t *p2 = p1 + bstride;
    uint8_t *orig = p0, *end = p2;

    line_copy8(p0, srcp + stride, width, 1);
    line_copy8(p1, srcp, width, 1);

    float rdiv = (float)ch->rdiv;
    float bias = (float)ch->bias;

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy8(p2, srcp, width, 1);

        for (int x = 0; x < width; x++) {
            int sum = p0[x - 1] * ch->m[0] +
                      p0[x] * ch->m[1] +
                      p0[x + 1] * ch->m[2] +
                      p1[x - 1] * ch->m[3] +
                      p1[x] * ch->m[4] +
                      p1[x + 1] * ch->m[5] +
                      p2[x - 1] * ch->m[6] +
                      p2[x] * ch->m[7] +
                      p2[x + 1] * ch->m[8];
            sum = (int)(sum * rdiv + bias + 0.5f);
            if (!ch->saturate) {
                sum = abs(sum);
            }
            dstp[x] = min_int(255, max_int(sum, 0));
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


static void VS_CC
proc_16bit(convolution_t *ch, uint8_t *buff, int bstride, int width,
           int height, int stride, uint8_t *d, const uint8_t *s)
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

    float rdiv = (float)ch->rdiv;
    float bias = (float)ch->bias;

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);

        for (int x = 0; x < width; x++) {
            int sum = p0[x - 1] * ch->m[0] +
                      p0[x] * ch->m[1] +
                      p0[x + 1] * ch->m[2] +
                      p1[x - 1] * ch->m[3] +
                      p1[x] * ch->m[4] +
                      p1[x + 1] * ch->m[5] +
                      p2[x - 1] * ch->m[6] +
                      p2[x] * ch->m[7] +
                      p2[x + 1] * ch->m[8];
            sum = (int)(sum * rdiv + bias + 0.5f);
            if (!ch->saturate) {
                sum = abs(sum);
            }
            dstp[x] = min_int(65535, max_int(sum, 0));
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


const proc_convolution convo_3x3[] = {
    proc_8bit,
    proc_16bit,
    proc_16bit
};

#endif
