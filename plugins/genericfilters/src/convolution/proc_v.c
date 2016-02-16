/*
  proc_v.c: Copyright (C) 2012-2013  Oka Motofumi

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
#include "simd/proc_v_sse2.c"
#else


#include "stdlib.h"
#include "convolution.h"
#include "no_simd.h"


static void VS_CC
proc_8bit(convolution_t *ch, uint8_t *buff, int bstride, int width,
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

    float rdiv = (float)ch->rdiv;
    float bias = (float)ch->bias;
    int *m = ch->m;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int i = 0; i < num; i++) {
                sum += *(p[i] + x) * m[i];
            }
            sum = (int)(sum * rdiv + bias + 0.5f);
            if (!ch->saturate) {
                sum = abs(sum);
            }
            dstp[x] = min_int(255, max_int(sum, 0));
        }
        dstp += stride;
        for (int i = 0; i < num - 1; i++) {
            p[i] = p[i + 1];
        }
        p[num - 1] += stride * (y < height - border - 1 ? 1 : -1);
    }
}


static void VS_CC
proc_16bit(convolution_t *ch, uint8_t *buff, int bstride, int width,
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

    float rdiv = (float)ch->rdiv;
    float bias = (float)ch->bias;
    int *m = ch->m;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int i = 0; i < num; i++) {
                sum += *(p[i] + x) * m[i];
            }
            sum = (int)(sum * rdiv + bias + 0.5f);
            if (!ch->saturate) {
                sum = abs(sum);
            }
            dstp[x] = min_int(65535, max_int(sum, 0));
        }
        dstp += stride;
        for (int i = 0; i < num - 1; i++) {
            p[i] = p[i + 1];
        }
        p[num - 1] += stride * (y < height - border - 1 ? 1 : -1);
    }
}


const proc_convolution convo_v[] = {
    proc_8bit,
    proc_16bit,
    proc_16bit
};

#endif
