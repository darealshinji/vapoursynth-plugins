/*
  sobel.c: Copyright (C) 2013  Oka Motofumi

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
#include "simd/sobel_sse2.c"
#else


#include "math.h"
#include "edge.h"
#include "no_simd.h"


static void VS_CC
proc_8bit(uint8_t *buff, int bstride, int width, int height, int stride,
          uint8_t *dstp, const uint8_t *srcp, edge_t *eh, uint16_t plane_max)
{
    uint8_t *p0 = buff + 16;
    uint8_t *p1 = p0 + bstride;
    uint8_t *p2 = p1 + bstride;
    uint8_t *orig = p0, *end = p2;

    line_copy8(p0, srcp + stride, width, 1);
    line_copy8(p1, srcp, width, 1);

    int th_min = min_int(eh->min, 255);
    int th_max = min_int(eh->max, 255);
    
    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy8(p2, srcp, width, 1);
        for (int x = 0; x < width; x++) {
            int gx = -p0[x - 1] + p0[x + 1] - 2 * p1[x - 1] + 2 * p1[x + 1]
                     - p2[x - 1] + p2[x + 1];
            int gy = -p0[x - 1] - 2 * p0[x] - p0[x + 1] + p2[x - 1] + 2 * p2[x]
                     + p2[x + 1];
            int g = (int)(sqrtf(gx * gx + gy * gy) + 0.5f);
            g = g >> eh->rshift;
            if (g >= th_max) {
                g = 255;
            }
            if (g <= th_min) {
                g = 0;
            }
            dstp[x] = g;
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


static void VS_CC
proc_16bit(uint8_t *buff, int bstride, int width, int height, int stride,
           uint8_t *d, const uint8_t *s, edge_t *eh, uint16_t plane_max)
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

    int th_min = min_int(eh->min, plane_max);
    int th_max = min_int(eh->max, plane_max);

    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);
        for (int x = 0; x < width; x++) {
            int gx = -p0[x - 1] + p0[x + 1] - 2 * p1[x - 1] + 2 * p1[x + 1]
                     - p2[x - 1] + p2[x + 1];
            int gy = -p0[x - 1] - 2 * p0[x] - p0[x + 1] + p2[x - 1] + 2 * p2[x]
                     + p2[x + 1];
            int g = (int)(sqrtf(gx * gx + gy * gy) + 0.5f);
            g = g >> eh->rshift;
            if (g >= th_max) {
                g = plane_max;
            }
            if (g <= th_min) {
                g = 0;
            }
            dstp[x] = g;
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


const proc_edge_detection sobel[] = {
    proc_8bit,
    proc_16bit,
    proc_16bit
};

#endif
