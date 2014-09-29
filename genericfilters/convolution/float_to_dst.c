/*
  float_to_dst.c: Copyright (C) 2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This file is part of Generic.

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
#include "simd/float_to_dst_sse2.c"
#else
#include "canny.h"


static void VS_CC
float_to_dst_8bit(const float *srcp, uint8_t *dstp, int width, int height,
                  int src_stride, int dst_stride, float th, int bits)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstp[x] = srcp[x] >= th ? 0xFF : 0;
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void VS_CC
float_to_dst_16bit(const float *srcp, uint8_t *d, int width, int height,
                   int src_stride, int dst_stride, float th, int bits)
{
    uint16_t *dstp = (uint16_t *)d;
    dst_stride /= 2;
    uint16_t val = (1 << bits) - 1;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstp[x] = srcp[x] >= th ? val : 0;
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void VS_CC
float_to_dst_gb_8bit(const float *srcp, uint8_t *dstp, int width, int height,
                    int src_stride, int dst_stride, float th, int bits)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int tmp = (int)(srcp[x] + 0.5f);
            dstp[x] = tmp > 255 ? 255 : tmp;
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void VS_CC
float_to_dst_gb_16bit(const float *srcp, uint8_t *d, int width, int height,
                      int src_stride, int dst_stride, float th, int bits)
{
    uint16_t *dstp = (uint16_t *)d;
    dst_stride /= 2;
    int tmax = (1 << bits) - 1;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int tmp = (int)(srcp[x] + 0.5f);
            dstp[x] = tmp > tmax ? tmax : tmp;
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


const write_dst write_dst_canny[] = {
    float_to_dst_8bit, float_to_dst_16bit
};

const write_dst write_dst_gblur[] = {
    float_to_dst_gb_8bit, float_to_dst_gb_16bit
};

#endif
