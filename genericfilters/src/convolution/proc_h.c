/*
  proc_h.c: Copyright (C) 2012  Oka Motofumi

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
#include "simd/proc_h_sse2.c"
#else


#include <stdlib.h>
#include "convolution.h"
#include "no_simd.h"


static void GF_FUNC_ALIGN VS_CC
proc_8bit(convolution_t *ch, uint8_t *buff, int bstride, int width,
          int height, int stride, uint8_t *dstp, const uint8_t *srcp)
{
    uint8_t *p0 = buff + 16;

    float rdiv = (float)ch->rdiv;
    float bias = (float)ch->bias;
    int border = ch->length / 2;

    for (int y = 0; y < height; y++) {
        line_copy8(p0, srcp, width, border);

        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int i = -border; i <= border; i++) {
                sum += p0[x + i] * ch->m[i + border];
            }
            sum = (int)(sum * rdiv + bias + 0.5f);
            if (!ch->saturate) {
                sum = abs(sum);
            }
            dstp[x] = min_int(255, max_int(sum, 0));
        }
        srcp += stride;
        dstp += stride;
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

    float rdiv = (float)ch->rdiv;
    float bias = (float)ch->bias;
    int border = ch->length / 2;

    for (int y = 0; y < height; y++) {
        line_copy16(p0, srcp, width, border);

        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int i = -border; i <= border; i++) {
                sum += p0[x + i] * ch->m[i + border];
            }
            sum = (int)(sum * rdiv + bias + 0.5f);
            if (!ch->saturate) {
                sum = abs(sum);
            }
            dstp[x] = min_int(65535, max_int(sum, 0));
        }
        srcp += stride;
        dstp += stride;
    }
}


const proc_convolution convo_h[] = {
    proc_8bit,
    proc_16bit,
    proc_16bit
};

#endif
