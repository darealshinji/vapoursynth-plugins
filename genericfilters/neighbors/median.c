/*
  median.c: Copyright (C) 2012-2013  Oka Motofumi

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
#include "simd/median_sse2.c"
#else

#include "neighbors.h"
#include "no_simd.h"

/* from 'Implementing Median Filters in XC4000E FPGAs' by John L. Smith */

#define LOWHIGH(X, Y) {\
    if (X > Y) {\
        int t = X;\
        X = Y;\
        Y = t;\
    }\
}

static void VS_CC
proc_8bit(uint8_t *buff, int bstride, int width, int height, int stride,
          uint8_t *dstp, const uint8_t *srcp, int th, int *enable)
{
    uint8_t *p0 = buff + 16;
    uint8_t *p1 = p0 + bstride;
    uint8_t *p2 = p1 + bstride;
    uint8_t *orig = p0, *end = p2;

    line_copy8(p0, srcp + stride, width, 1);
    line_copy8(p1, srcp, width, 1);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy8(p2, srcp, width, 1);
        
        for (int x = 0; x < width; x++) {
            int x0, x1, x2, x3, x4, x5, x6, x7;
            x0 = p0[x];
            x1 = p0[x + 1];
            LOWHIGH(x0, x1);
            x2 = p0[x - 1];
            LOWHIGH(x0, x2);
            LOWHIGH(x1, x2);
            x3 = p1[x];
            x4 = p1[x + 1];
            LOWHIGH(x3, x4);
            x5 = p1[x - 1];
            LOWHIGH(x3, x5);
            LOWHIGH(x4, x5);
            x0 = max_int(x0, x3);
            x3 = p2[x];
            x6 = p2[x + 1];
            LOWHIGH(x3, x6);
            x7 = p2[x - 1];
            LOWHIGH(x3, x7);
            LOWHIGH(x6, x7);
            x0 = max_int(x0, x3);
            LOWHIGH(x4, x6);
            x1 = max_int(x1, x4);
            x5 = min_int(x5, x7);
            x2 = min_int(x2, x5);
            x1 = min_int(x1, x6);
            LOWHIGH(x1, x2);
            x0 = max_int(x0, x1);
            x0 = min_int(x0, x2);
            dstp[x] = (uint8_t)x0;
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


static void VS_CC
proc_16bit(uint8_t *buff, int bstride, int width, int height, int stride,
           uint8_t *d, const uint8_t *s, int th, int *enable)
{
    stride /= 2;
    uint16_t *dstp = (uint16_t *)d;
    const uint16_t *srcp = (uint16_t *)s;
    bstride /= 2;

    uint16_t *p0 = (uint16_t *)buff + 8;
    uint16_t *p1 = p0 + bstride;
    uint16_t *p2 = p1 + bstride;
    uint16_t *orig = p0, *end = p2;

    line_copy16(p0, srcp + stride, width, 1);
    line_copy16(p1, srcp, width, 1);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);

        for (int x = 0; x < width; x++) {
            int x0, x1, x2, x3, x4, x5, x6, x7;
            x0 = p0[x];
            x1 = p0[x + 1];
            LOWHIGH(x0, x1);
            x2 = p0[x - 1];
            LOWHIGH(x0, x2);
            LOWHIGH(x1, x2);
            x3 = p1[x];
            x4 = p1[x + 1];
            LOWHIGH(x3, x4);
            x5 = p1[x - 1];
            LOWHIGH(x3, x5);
            LOWHIGH(x4, x5);
            x0 = max_int(x0, x3);
            x3 = p2[x];
            x6 = p2[x + 1];
            LOWHIGH(x3, x6);
            x7 = p2[x - 1];
            LOWHIGH(x3, x7);
            LOWHIGH(x6, x7);
            x0 = max_int(x0, x3);
            LOWHIGH(x4, x6);
            x1 = max_int(x1, x4);
            x5 = min_int(x5, x7);
            x2 = min_int(x2, x5);
            x1 = min_int(x1, x6);
            LOWHIGH(x1, x2);
            x0 = max_int(x0, x1);
            x0 = min_int(x0, x2);
            dstp[x] = (uint16_t)x0;
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


const proc_neighbors median[] = {
    proc_8bit,
    proc_16bit
};

#endif
