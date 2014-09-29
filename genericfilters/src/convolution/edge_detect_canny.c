/*
  edge_detect_canny.c: Copyright (C) 2013  Oka Motofumi

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


#include <float.h>
#include <math.h>

#ifdef USE_X86_INTRINSICS
#include "simd/edge_detect_canny_sse2.c"
#else
#include "canny.h"
#include "no_simd.h"


static inline uint8_t calc_direction(float gx, float gy)
{
    const float t0225 = (float)(sqrt(2.0) - 1.0); // tan(pi/8)
    const float t0675 = (float)(sqrt(2.0) + 1.0); // tan(3*pi/8)
    const float t1125 = -t0675;
    const float t1575 = -t0225;

    if (gx == 0.0f) {
        gx = 0.0001f;
    }

    if (gy < 0) {
        gx = -gx;
        gy = -gy;
    }

    float tan = gy / gx;
    if (tan < t0225 && tan >= t1575) {
        return 1;
    }
    if (tan >= t0225 && tan < t0675) {
        return 3;
    }
    if (tan >= t0675 || tan < t1125) {
        return 7;
    }
    return 15;
}


static void VS_CC
proc_edge_detect(float *buff, int bstride, const float *srcp, float *dstp,
                 uint8_t *direction,int width, int height, int stride)
{
    float *p0 = buff;
    float *p1 = p0 + bstride;
    float *p2 = p1 + bstride;
    float *orig = p0, *end = p2;
    
    line_copyf(p0, srcp, width, 1);
    srcp += stride;
    line_copyf(p1, srcp, width, 1);
    memset(dstp, 0, stride * sizeof(float));

    for (int y = 1; y < height - 1; y++) {
        srcp += stride;
        line_copyf(p2, srcp, width, 1);
        for (int x = 0; x < width; x++) {
            float gx = p1[x + 1] - p1[x - 1];
            float gy = p0[x] - p2[x];
            direction[x] = calc_direction(gx, gy);
            dstp[x] = sqrtf(gx * gx + gy * gy);
        }
        dstp += stride;
        direction += stride;
        p0 = p1;
        p1 = p2;
        p2 = (p2 == end) ? orig : p2 + bstride;
    }
    memset(dstp, 0, stride * sizeof(float));
}

#endif

static void VS_CC
proc_non_max_suppress(const float *srcp, float *dstp, const uint8_t *direction,
                      int width, int height, int stride)
{
    memset(dstp, 0, stride * sizeof(float));
    srcp += stride;
    dstp += stride;
    memcpy(dstp, srcp, stride * sizeof(float) * (height - 2));

    int pos[8] = {1, 1 - stride, 0, stride, 0, 0, 0, 1 + stride};

    for (int y = 1; y < height - 1; y++) {
        direction += stride;
        dstp[0] = 0.0f;
        
        for (int x = 1; x < width - 1; x++) {
            int p = pos[(direction[x] + 1) / 2 - 1];
            if (srcp[x] < srcp[x + p] || srcp[x] < srcp[x - p]) {
                dstp[x] = 0.0f;
            }
        }
        dstp[width - 1] = 0.0f;
        srcp += stride;
        dstp += stride;
    }
    memset(dstp, 0, stride * sizeof(float));
}


static void reset(stack_t *s, int size)
{
    s->index = -1;
    memset(s->map, 0, size);
}


static void push(stack_t *s, int x, int y)
{
    s->pos[++s->index] = (int32_t)((x << 16) | y);
}


static int32_t pop(stack_t *s)
{
    return s->pos[s->index--];
}


static void VS_CC
proc_hysteresis(float *edge, int width, int height, int stride, float tmax,
                float tmin, stack_t *stack)
{
    reset(stack, width * height);

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (edge[x + y * stride] < tmax || stack->map[x + y * width]) {
                continue;
            }
            edge[x + y * stride] = FLT_MAX;
            stack->map[x + y * width] = 0xFF;
            push(stack, x, y);
            
            while (stack->index > -1) {
                int32_t posx = pop(stack);
                int32_t posy = posx & 0xFFFF;
                posx >>= 16;
                int32_t xmin = posx > 1 ? posx - 1 : 1;
                int32_t xmax = posx < width - 2 ? posx + 1 : posx;
                int32_t ymin = posy > 1 ? posy - 1 : 1;
                int32_t ymax = posy < height - 2 ? posy + 1 : posy;
                for (int yy = ymin; yy <= ymax; yy++) {
                    for (int xx = xmin; xx <= xmax; xx++) {
                        if (edge[xx + yy * stride] > tmin
                            && !stack->map[xx + yy * width]) {
                            edge[xx + yy * stride] = FLT_MAX;
                            stack->map[xx + yy * width] = 0xFF;
                            push(stack, xx, yy);
                        }
                    }
                }
            }
        }
    }
}


const proc_edetect edge_detect = proc_edge_detect;
const proc_nms  non_max_suppress = proc_non_max_suppress;
const proc_hyst hysteresis = proc_hysteresis;
