/*
  canny.h: Copyright (C) 2013  Oka Motofumi

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


#ifndef CANNY_EDGE_DETECTION_HEADER
#define CANNY_EDGE_DETECTION_HEADER

#include <stdint.h>
#include "VapourSynth.h"


typedef struct filter_data canny_t;

typedef struct stack {
    int index;
    int32_t *pos;
    uint8_t *map;
} stack_t;


typedef void (VS_CC *proc_gblur)(int radius, float *kernel, const uint8_t *srcp,
                                 float *buff, float *dstp, int width,
                                 int height, int src_stride, int dst_stride);

typedef void (VS_CC *proc_edetect)(float *buff, int bstride, const float *srcp,
                                   float *dstp, uint8_t *direction,int width,
                                   int height, int stride);

typedef void (VS_CC *proc_nms)(const float *srcp, float *dstp,
                               const uint8_t *direction, int width, int height,
                               int stride);

typedef void (VS_CC *proc_hyst)(float *edge, int width, int height, int stride,
                                float tmax, float tmin, stack_t *stack);

typedef void (VS_CC *write_dst)(const float *srcp, uint8_t *d, int width,
                                int height, int src_stride, int dst_stride,
                                float th, int bits);

struct filter_data {
    float tl;
    float th;
    float kernel[17];
    int radius;
    const proc_gblur *gaussian_blur;
};


extern const proc_gblur gblur[];
extern const proc_gblur convert_to_float[];
extern const proc_edetect edge_detect;
extern const proc_nms non_max_suppress;
extern const proc_hyst hysteresis;
extern const write_dst write_dst_canny[];
extern const write_dst write_dst_gblur[];

#endif // CANNY_EDGE_DETECTION_HEADER
