/*
  maximum.c: Copyright (C) 2012-2013  Oka Motofumi

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
#include "simd/maximum_sse2.c"
#else
#include "neighbors.h"
#include "no_simd.h"


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
        uint8_t *coordinates[] = {p0 - 1, p0, p0 + 1,
                                  p1 - 1,     p1 + 1,
                                  p2 - 1, p2, p2 + 1};

        for (int x = 0; x < width; x++) {
            int max = p1[x];
            int limit = min_int(max + th, 255);
            for (int i = 0; i < 8; i++) {
                if (enable[i]) {
                    max = max_int(*(coordinates[i] + x), max);
                }
            }
            max = min_int(limit, max);
            dstp[x] = (uint8_t)max;
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
    bstride /= 2;
    uint16_t *dstp = (uint16_t *)d;
    const uint16_t *srcp = (uint16_t *)s;

    uint16_t *p0 = (uint16_t *)buff + 8;
    uint16_t *p1 = p0 + bstride;
    uint16_t *p2 = p1 + bstride;
    uint16_t *orig = p0, *end = p2;

    line_copy16(p0, srcp + stride, width, 1);
    line_copy16(p1, srcp, width, 1);
    
    for (int y = 0; y < height; y++) {
        srcp += stride * (y < height - 1 ? 1 : -1);
        line_copy16(p2, srcp, width, 1);
        uint16_t *coordinates[] = {p0 - 1, p0, p0 + 1,
                                   p1 - 1,     p1 + 1,
                                   p2 - 1, p2, p2 + 1};
                                   
        for (int x = 0; x < width; x++) {
            int max = p1[x];
            int limit = min_int(max + th, 0xFFFF);
            for (int i = 0; i < 8; i++) {
                if (enable[i]) {
                    max = max_int(*(coordinates[i] + x), max);
                }
            }
            max = min_int(limit, max);
            dstp[x] = (uint16_t)max;
        }
        dstp += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
}


const proc_neighbors maximum[] = {
    proc_8bit,
    proc_16bit
};

#endif