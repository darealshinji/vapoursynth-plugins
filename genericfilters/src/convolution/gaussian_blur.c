/*
  gaussian_blur.c: Copyright (C) 2013  Oka Motofumi

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
#include "simd/gaussian_blur_sse2.c"
#else

#include <stdlib.h>
#include "no_simd.h"
#include "canny.h"


static void VS_CC
convert_to_float_8bit(int radius, float *kernel, const uint8_t *srcp,
                      float *buff, float *dstp, int width, int height,
                      int src_stride, int dst_stride)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstp[x] = (float)(srcp[x]);
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void VS_CC
convert_to_float_16bit(int radius, float *kernel, const uint8_t *s,
                       float *buff, float *dstp, int width, int height,
                       int src_stride, int dst_stride)
{
    const uint16_t *srcp = (uint16_t *)s;
    src_stride /= 2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstp[x] = (float)(srcp[x]);
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void VS_CC
proc_horizontal(float *srcp, int radius, int length, int width, float *kernel,
                float *dstp)
{
    for (int i = 1; i <= radius; i++) {
        srcp[-i] = srcp[i];
        srcp[width - 1 + i] = srcp[width - 1 - i];
    }

    for (int x = 0; x < width; x++) {
        float sum = 0.0f;
        for (int i = -radius; i <= radius; i++) {
            sum += srcp[x + i] * kernel[i + radius];
        }
        dstp[x] = sum;
    }
}


static void VS_CC
proc_8bit(int radius, float *kernel, const uint8_t *srcp, float *buff,
          float *dstp, int width, int height, int src_stride, int dst_stride)
{
    int length = radius * 2 + 1;
    const uint8_t *p[17];
    for (int i = -radius; i <= radius; i++) {
        p[i + radius] = srcp + abs(i) * src_stride;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            for (int i = 0; i < length; i++) {
                sum += p[i][x] * kernel[i];
            }
            buff[x] = sum;
        }
        proc_horizontal(buff, radius, length, width, kernel, dstp);
        
        for (int i = 0; i < length - 1; i++) {
            p[i] = p[i + 1];
        }
        p[length - 1] += (y < height - radius - 1 ? 1 : -1) * src_stride;
        dstp += dst_stride;
    }
}


static void VS_CC
proc_16bit(int radius, float *kernel, const uint8_t *srcp, float *buff,
           float *dstp, int width, int height, int src_stride, int dst_stride)
{
    int length = radius * 2 + 1;
    src_stride /= 2;
    const uint16_t *p[17];
    for (int i = -radius; i <= radius; i++) {
        p[i + radius] =(uint16_t *)srcp + abs(i) * src_stride;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            for (int i = 0; i < length; i++) {
                sum += p[i][x] * kernel[i];
            }
            buff[x] = sum;
        }
        proc_horizontal(buff, radius, length, width, kernel, dstp);
        
        for (int i = 0; i < length - 1; i++) {
            p[i] = p[i + 1];
        }
        p[length - 1] += (y < height - radius - 1 ? 1 : -1) * src_stride;
        dstp += dst_stride;
    }
}


const proc_gblur gblur[] = {
    proc_8bit, proc_16bit
};

const proc_gblur convert_to_float[] = {
    convert_to_float_8bit, convert_to_float_16bit
};
#endif
