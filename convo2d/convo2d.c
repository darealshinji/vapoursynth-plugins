/*
  convo2d: Spatial convolution filter for VapourSynth

  Copyright (C) 2012  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "VapourSynth.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#define snprintf _snprintf
#endif

#define CONVO2D_VERSION "0.2.0"

typedef struct convo2d_handle convo2d_t;
typedef void (VS_CC *proc_convolution)(convo2d_t *ch, int plane,
                                       const VSFrameRef *src, VSFrameRef *dst,
                                       const VSAPI *vsapi, uint16_t max);

typedef enum {
    MATRIX_TYPE_3X3 = 0,
    MATRIX_TYPE_5X5,
    MATRIX_TYPE_3H,
    MATRIX_TYPE_5H,
    MATRIX_TYPE_3V,
    MATRIX_TYPE_5V
} mtype_t;

struct convo2d_handle {
    VSNodeRef *node;
    const VSVideoInfo *vi;
    int m[25];
    mtype_t mtype;
    double div;
    double bias;
    int planes[3];
};

static uint16_t clamp(double val, uint16_t max)
{
    if (val < 0) {
        return 0;
    }
    if (val > max) {
        return max;
    }
    return (uint16_t)val;
}


static void VS_CC
proc_horizontal3_8bit(convo2d_t *ch, int plane, const VSFrameRef *src,
                     VSFrameRef *dst, const VSAPI *vsapi, uint16_t max)
{
    int m0 = ch->m[0], m1 = ch->m[1], m2 = ch->m[2];

    int w = vsapi->getFrameWidth(src, plane) - 1;
    int h = vsapi->getFrameHeight(src, plane);
    int stride = vsapi->getStride(src, plane);
    double div = ch->div;
    double bias = ch->bias;

    uint8_t *dstp = vsapi->getWritePtr(dst, plane);
    const uint8_t *r = vsapi->getReadPtr(src, plane);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x <= w; x++) {
            int xl = x - !!x;
            int xr = x + !!(w - x);
            int64_t value = *(r + xl) * m0 + *(r + x) * m1 + *(r + xr) * m2;
            dstp[x] = (uint8_t)clamp(value / div + bias, max);
        }
        r += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_horizontal3_16bit(convo2d_t *ch, int plane, const VSFrameRef *src,
                       VSFrameRef *dst, const VSAPI *vsapi, uint16_t max)
{
    int m0 = ch->m[0], m1 = ch->m[1], m2 = ch->m[2];

    int w = vsapi->getFrameWidth(src, plane) - 1;
    int h = vsapi->getFrameHeight(src, plane);
    int stride = vsapi->getStride(src, plane) / 2;
    double div = ch->div;
    double bias = ch->bias;

    uint16_t *dstp = (uint16_t *)vsapi->getWritePtr(dst, plane);
    const uint16_t *r = (uint16_t *)vsapi->getReadPtr(src, plane);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x <= w; x++) {
            int xl = x - !!x;
            int xr = x + !!(w - x);
            int64_t value = *(r + xl) * m0 + *(r + x) * m1 + *(r + xr) * m2;
            dstp[x] = clamp(value / div + bias, max);
        }
        r += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_horizontal5_8bit(convo2d_t *ch, int plane, const VSFrameRef *src,
                     VSFrameRef *dst, const VSAPI *vsapi, uint16_t max)
{
    int m0, m1, m2, m3, m4;
    if (ch->mtype == MATRIX_TYPE_3H) {
        m0 = 0; m1 = ch->m[0]; m2 = ch->m[1]; m3 = ch->m[2], m4 = 0;
    } else {
        m0 = ch->m[0]; m1 = ch->m[1]; m2 = ch->m[2]; m3 = ch->m[3]; m4 = ch->m[4];
    }

    int w = vsapi->getFrameWidth(src, plane) - 1;
    int h = vsapi->getFrameHeight(src, plane);
    int stride = vsapi->getStride(src, plane);
    double div = ch->div;
    double bias = ch->bias;

    uint8_t *dstp = vsapi->getWritePtr(dst, plane);
    const uint8_t *r = vsapi->getReadPtr(src, plane);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x <= w; x++) {
            int x1 = x - !!x;
            int x0 = x1 + !x - !!(x - 1);
            int x3 = x + !!(w - x);
            int x4 = x3 + !!(w - x - 1) - !(w - x);
            int64_t value = *(r + x0) * m0 + *(r + x1) * m1 + *(r + x) * m2 +
                            *(r + x3) * m3 + *(r + x4) * m4;
            dstp[x] = (uint8_t)clamp(value / div + bias, max);
        }
        r += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_horizontal5_16bit(convo2d_t *ch, int plane, const VSFrameRef *src,
                      VSFrameRef *dst, const VSAPI *vsapi, uint16_t max)
{
    int m0, m1, m2, m3, m4;
    if (ch->mtype == MATRIX_TYPE_3H) {
        m0 = 0; m1 = ch->m[0]; m2 = ch->m[1]; m3 = ch->m[2], m4 = 0;
    } else {
        m0 = ch->m[0]; m1 = ch->m[1]; m2 = ch->m[2]; m3 = ch->m[3]; m4 = ch->m[4];
    }

    int w = vsapi->getFrameWidth(src, plane) - 1;
    int h = vsapi->getFrameHeight(src, plane);
    int stride = vsapi->getStride(src, plane) / 2;
    double div = ch->div;
    double bias = ch->bias;

    uint16_t *dstp = (uint16_t *)vsapi->getWritePtr(dst, plane);
    const uint16_t *r = (uint16_t *)vsapi->getReadPtr(src, plane);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x <= w; x++) {
            int x1 = x - !!x;
            int x0 = x1 + !x - !!(x - 1);
            int x3 = x + !!(w - x);
            int x4 = x3 + !!(w - x - 1) - !(w - x);
            int64_t value = *(r + x0) * m0 + *(r + x1) * m1 + *(r + x) * m2 +
                            *(r + x3) * m3 + *(r + x4) * m4;
            dstp[x] = clamp(value / div + bias, max);
        }
        r += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_vertical3_8bit(convo2d_t *ch, int plane, const VSFrameRef *src,
                    VSFrameRef *dst, const VSAPI *vsapi, uint16_t max)
{
    int m0 = ch->m[0], m1 = ch->m[1], m2 = ch->m[2];

    int w = vsapi->getFrameWidth(src, plane);
    int h = vsapi->getFrameHeight(src, plane) - 1;
    int stride = vsapi->getStride(src, plane);
    double div = ch->div;
    double bias = ch->bias;

    uint8_t *dstp = vsapi->getWritePtr(dst, plane);
    const uint8_t *r1 = vsapi->getReadPtr(src, plane);

    for (int y = 0; y <= h; y++) {
        const uint8_t *r0 = r1 - !!y * stride;
        const uint8_t *r2 = r1 + !!(h - y) * stride;
        for (int x = 0; x < w; x++) {
            int64_t value = *(r0 + x) * m0 + *(r1 + x) * m1 + *(r2 + x) * m2;
            dstp[x] = (uint8_t)clamp(value / div + bias, max);
        }
        r1 += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_vertical3_16bit(convo2d_t *ch, int plane, const VSFrameRef *src,
                    VSFrameRef *dst, const VSAPI *vsapi, uint16_t max)
{
    int m0 = ch->m[0], m1 = ch->m[1], m2 = ch->m[2];

    int w = vsapi->getFrameWidth(src, plane);
    int h = vsapi->getFrameHeight(src, plane) - 1;
    int stride = vsapi->getStride(src, plane) / 2;
    double div = ch->div;
    double bias = ch->bias;

    uint16_t *dstp = (uint16_t *)vsapi->getWritePtr(dst, plane);
    const uint16_t *r1 = (uint16_t *)vsapi->getReadPtr(src, plane);

    for (int y = 0; y <= h; y++) {
        const uint16_t *r0 = r1 - !!y * stride;
        const uint16_t *r2 = r1 + !!(h - y) * stride;
        for (int x = 0; x < w; x++) {
            int64_t value = *(r0 + x) * m0 + *(r1 + x) * m1 + *(r2 + x) * m2;
            dstp[x] = clamp(value / div + bias, max);
        }
        r1 += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_vertical5_8bit(convo2d_t *ch, int plane, const VSFrameRef *src,
                   VSFrameRef *dst, const VSAPI *vsapi, uint16_t max)
{
    int m0, m1, m2, m3, m4;
    if (ch->mtype == MATRIX_TYPE_3V) {
        m0 = 0; m1 = ch->m[0]; m2 = ch->m[1]; m3 = ch->m[2], m4 = 0;
    } else {
        m0 = ch->m[0]; m1 = ch->m[1]; m2 = ch->m[2]; m3 = ch->m[3]; m4 = ch->m[4];
    }

    int w = vsapi->getFrameWidth(src, plane);
    int h = vsapi->getFrameHeight(src, plane) - 1;
    int stride = vsapi->getStride(src, plane);
    double div = ch->div;
    double bias = ch->bias;

    uint8_t *dstp = vsapi->getWritePtr(dst, plane);
    const uint8_t *r2 = vsapi->getReadPtr(src, plane);

    for (int y = 0; y <= h; y++) {
        int offy_t = !y;
        int offy_b = !(h - y);
        const uint8_t *r1 = r2 - !offy_t * stride;
        const uint8_t *r0 = r1 + (offy_t - !!(y - 1)) * stride;
        const uint8_t *r3 = r2 + !offy_b * stride;
        const uint8_t *r4 = r3 + (!!(h - y - 1) - offy_b) * stride;
        for (int x = 0; x < w; x++) {
            int64_t value = *(r0 + x) * m0 + *(r1 + x) * m1 + *(r2 + x) * m2 +
                            *(r3 + x) * m3 + *(r4 + x) * m4;
            dstp[x] = (uint8_t)clamp(value / div + bias, max);
        }
        r2 += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_vertical5_16bit(convo2d_t *ch, int plane, const VSFrameRef *src,
                    VSFrameRef *dst, const VSAPI *vsapi, uint16_t max)
{
    int m0, m1, m2, m3, m4;
    if (ch->mtype == MATRIX_TYPE_3V) {
        m0 = 0; m1 = ch->m[0]; m2 = ch->m[1]; m3 = ch->m[2], m4 = 0;
    } else {
        m0 = ch->m[0]; m1 = ch->m[1]; m2 = ch->m[2]; m3 = ch->m[3]; m4 = ch->m[4];
    }

    int w = vsapi->getFrameWidth(src, plane);
    int h = vsapi->getFrameHeight(src, plane) - 1;
    int stride = vsapi->getStride(src, plane) / 2;
    double div = ch->div;
    double bias = ch->bias;

    uint16_t *dstp = (uint16_t *)vsapi->getWritePtr(dst, plane);
    const uint16_t *r2 = (uint16_t *)vsapi->getReadPtr(src, plane);

    for (int y = 0; y <= h; y++) {
        int offy_t = !y;
        int offy_b = !(h - y);
        const uint16_t *r1 = r2 - !offy_t * stride;
        const uint16_t *r0 = r1 + (offy_t - !!(y - 1)) * stride;
        const uint16_t *r3 = r2 + !offy_b * stride;
        const uint16_t *r4 = r3 + (!!(h - y - 1) - offy_b) * stride;
        for (int x = 0; x < w; x++) {
            int64_t value = *(r0 + x) * m0 + *(r1 + x) * m1 + *(r2 + x) * m2 +
                            *(r3 + x) * m3 + *(r4 + x) * m4;
            dstp[x] = clamp(value / div + bias, max);
        }
        r2 += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_3x3_8bit(convo2d_t *ch, int plane, const VSFrameRef *src, VSFrameRef *dst,
              const VSAPI *vsapi, uint16_t max)
{
    int m00 = ch->m[0], m01 = ch->m[1], m02 = ch->m[2],
        m10 = ch->m[3], m11 = ch->m[4], m12 = ch->m[5],
        m20 = ch->m[6], m21 = ch->m[7], m22 = ch->m[8];

    int w = vsapi->getFrameWidth(src, plane) - 1;
    int h = vsapi->getFrameHeight(src, plane) - 1;
    int stride = vsapi->getStride(src, plane);
    double div = ch->div;
    double bias = ch->bias;

    uint8_t *dstp = vsapi->getWritePtr(dst, plane);
    const uint8_t *r1 = vsapi->getReadPtr(src, plane);

    for (int y = 0; y <= h; y++) {
        const uint8_t *r0 = r1 - stride * !!y;
        const uint8_t *r2 = r1 + stride * !!(h -y);
        for (int x = 0; x <= w; x++) {
            int xl = x - !!x;
            int xr = x + !!(w - x);
            int64_t value =
                *(r0 + xl) * m00 + *(r0 + x) * m01 + *(r0 + xr) * m02 +
                *(r1 + xl) * m10 + *(r1 + x) * m11 + *(r1 + xr) * m12 +
                *(r2 + xl) * m20 + *(r2 + x) * m21 + *(r2 + xr) * m22;
            dstp[x] = (uint8_t)clamp(value / div + bias, max);
        }
        r1 += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_3x3_16bit(convo2d_t *ch, int plane, const VSFrameRef *src, VSFrameRef *dst,
               const VSAPI *vsapi, uint16_t max)
{
    int m00 = ch->m[0], m01 = ch->m[1], m02 = ch->m[2],
        m10 = ch->m[3], m11 = ch->m[4], m12 = ch->m[5],
        m20 = ch->m[6], m21 = ch->m[7], m22 = ch->m[8];

    int w = vsapi->getFrameWidth(src, plane) - 1;
    int h = vsapi->getFrameHeight(src, plane) - 1;
    int stride = vsapi->getStride(src, plane) / 2;
    double div = ch->div;
    double bias = ch->bias;

    uint16_t *dstp = (uint16_t *)vsapi->getWritePtr(dst, plane);
    const uint16_t *r1 = (uint16_t *)vsapi->getReadPtr(src, plane);

    for (int y = 0; y <= h; y++) {
        const uint16_t *r0 = r1 - stride * !!y;
        const uint16_t *r2 = r1 + stride * !!(h -y);
        for (int x = 0; x <= w; x++) {
            int xl = x - !!x;
            int xr = x + !!(w - x);
            int64_t value =
                *(r0 + xl) * m00 + *(r0 + x) * m01 + *(r0 + xr) * m02 +
                *(r1 + xl) * m10 + *(r1 + x) * m11 + *(r1 + xr) * m12 +
                *(r2 + xl) * m20 + *(r2 + x) * m21 + *(r2 + xr) * m22;
            dstp[x] = clamp(value / div + bias, max);
        }
        r1 += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_5x5_8bit(convo2d_t *ch, int plane, const VSFrameRef *src, VSFrameRef *dst,
              const VSAPI *vsapi, uint16_t max)
{
    int m00 = ch->m[ 0], m01 = ch->m[ 1], m02 = ch->m[ 2], m03 = ch->m[ 3], m04 = ch->m[ 4],
        m10 = ch->m[ 5], m11 = ch->m[ 6], m12 = ch->m[ 7], m13 = ch->m[ 8], m14 = ch->m[ 9],
        m20 = ch->m[10], m21 = ch->m[11], m22 = ch->m[12], m23 = ch->m[13], m24 = ch->m[14],
        m30 = ch->m[15], m31 = ch->m[16], m32 = ch->m[17], m33 = ch->m[18], m34 = ch->m[19],
        m40 = ch->m[20], m41 = ch->m[21], m42 = ch->m[22], m43 = ch->m[23], m44 = ch->m[24];

    int w = vsapi->getFrameWidth(src, plane) - 1;
    int h = vsapi->getFrameHeight(src, plane) - 1;
    int stride = vsapi->getStride(src, plane);
    double div = ch->div;
    double bias = ch->bias;

    uint8_t *dstp = vsapi->getWritePtr(dst, plane);
    const uint8_t *r2 = vsapi->getReadPtr(src, plane);

    for (int y = 0; y <= h; y++) {
        int offy_t = !y;
        int offy_b = !(h - y);
        const uint8_t *r1 = r2 - !offy_t * stride;
        const uint8_t *r0 = r1 + (offy_t - !!(y - 1)) * stride;
        const uint8_t *r3 = r2 + !offy_b * stride;
        const uint8_t *r4 = r3 + (!!(h - y - 1) - offy_b) * stride;
        for (int x = 0; x <= w; x++) {
            int offx_l = !x;
            int offx_r = !(w - x);
            int x1 = x - !offx_l;
            int x0 = x1 + offx_l - !!(x - 1);
            int x3 = x + !offx_r;
            int x4 = x3 + !!(w - x - 1) - offx_r;
            int64_t value =
                *(r0 + x0) * m00 + *(r0 + x1) * m01 + *(r0 + x) * m02 + *(r0 + x3) * m03 + *(r0 + x4) * m04 +
                *(r1 + x0) * m10 + *(r1 + x1) * m11 + *(r1 + x) * m12 + *(r1 + x3) * m13 + *(r1 + x4) * m14 +
                *(r2 + x0) * m20 + *(r2 + x1) * m21 + *(r2 + x) * m22 + *(r2 + x3) * m23 + *(r2 + x4) * m24 +
                *(r3 + x0) * m30 + *(r3 + x1) * m31 + *(r3 + x) * m32 + *(r3 + x3) * m33 + *(r3 + x4) * m34 +
                *(r4 + x0) * m40 + *(r4 + x1) * m41 + *(r4 + x) * m42 + *(r4 + x3) * m43 + *(r4 + x4) * m44;
            dstp[x] = (uint8_t)clamp(value / div + bias, max);
        }
        r2 += stride;
        dstp += stride;
    }
}


static void VS_CC
proc_5x5_16bit(convo2d_t *ch, int plane, const VSFrameRef *src, VSFrameRef *dst,
               const VSAPI *vsapi, uint16_t max)
{
    int m00 = ch->m[ 0], m01 = ch->m[ 1], m02 = ch->m[ 2], m03 = ch->m[ 3], m04 = ch->m[ 4],
        m10 = ch->m[ 5], m11 = ch->m[ 6], m12 = ch->m[ 7], m13 = ch->m[ 8], m14 = ch->m[ 9],
        m20 = ch->m[10], m21 = ch->m[11], m22 = ch->m[12], m23 = ch->m[13], m24 = ch->m[14],
        m30 = ch->m[15], m31 = ch->m[16], m32 = ch->m[17], m33 = ch->m[18], m34 = ch->m[19],
        m40 = ch->m[20], m41 = ch->m[21], m42 = ch->m[22], m43 = ch->m[23], m44 = ch->m[24];

    int w = vsapi->getFrameWidth(src, plane) - 1;
    int h = vsapi->getFrameHeight(src, plane) - 1;
    int stride = vsapi->getStride(src, plane) / 2;
    double div = ch->div;
    double bias = ch->bias;

    uint16_t *dstp = (uint16_t *)vsapi->getWritePtr(dst, plane);
    const uint16_t *r2 = (uint16_t *)vsapi->getReadPtr(src, plane);

    for (int y = 0; y <= h; y++) {
        int offy_t = !y;
        int offy_b = !(h - y);
        const uint16_t *r1 = r2 - !offy_t * stride;
        const uint16_t *r0 = r1 + (offy_t - !!(y - 1)) * stride;
        const uint16_t *r3 = r2 + !offy_b * stride;
        const uint16_t *r4 = r3 + (!!(h - y - 1) - offy_b) * stride;
        for (int x = 0; x <= w; x++) {
            int offx_l = !x;
            int offx_r = !(w - x);
            int x1 = x - !offx_l;
            int x0 = x1 + offx_l - !!(x - 1);
            int x3 = x + !offx_r;
            int x4 = x3 + !!(w - x - 1) - offx_r;
            int64_t value =
                *(r0 + x0) * m00 + *(r0 + x1) * m01 + *(r0 + x) * m02 + *(r0 + x3) * m03 + *(r0 + x4) * m04 +
                *(r1 + x0) * m10 + *(r1 + x1) * m11 + *(r1 + x) * m12 + *(r1 + x3) * m13 + *(r1 + x4) * m14 +
                *(r2 + x0) * m20 + *(r2 + x1) * m21 + *(r2 + x) * m22 + *(r2 + x3) * m23 + *(r2 + x4) * m24 +
                *(r3 + x0) * m30 + *(r3 + x1) * m31 + *(r3 + x) * m32 + *(r3 + x3) * m33 + *(r3 + x4) * m34 +
                *(r4 + x0) * m40 + *(r4 + x1) * m41 + *(r4 + x) * m42 + *(r4 + x3) * m43 + *(r4 + x4) * m44;
            dstp[x] = clamp(value / div + bias, max);
        }
        r2 += stride;
        dstp += stride;
    }
}


#define MAKE_KEY(mtype, bytes) (((uint32_t)mtype << 16) | (uint32_t)bytes)

static proc_convolution get_proc_func(mtype_t m, int bytes_per_sample)
{
    uint32_t current_frame = MAKE_KEY(m, bytes_per_sample);
    const struct {
        uint32_t key;
        proc_convolution func;
    } table[] = {
        { MAKE_KEY(MATRIX_TYPE_3X3, 1), proc_3x3_8bit          },
        { MAKE_KEY(MATRIX_TYPE_3X3, 2), proc_3x3_16bit         },
        { MAKE_KEY(MATRIX_TYPE_5X5, 1), proc_5x5_8bit          },
        { MAKE_KEY(MATRIX_TYPE_5X5, 2), proc_5x5_16bit         },
        { MAKE_KEY(MATRIX_TYPE_3H,  1), proc_horizontal3_8bit  },
        { MAKE_KEY(MATRIX_TYPE_5H,  1), proc_horizontal5_8bit  },
        { MAKE_KEY(MATRIX_TYPE_3H,  2), proc_horizontal3_16bit },
        { MAKE_KEY(MATRIX_TYPE_5H,  2), proc_horizontal5_16bit },
        { MAKE_KEY(MATRIX_TYPE_3V,  1), proc_vertical3_8bit    },
        { MAKE_KEY(MATRIX_TYPE_5V,  1), proc_vertical5_8bit    },
        { MAKE_KEY(MATRIX_TYPE_3V,  2), proc_vertical3_16bit   },
        { MAKE_KEY(MATRIX_TYPE_5V,  2), proc_vertical5_16bit   },
        { current_frame, NULL }
    };

    int i = 0;
    while (table[i].key != current_frame) i++;

    return table[i].func;
}
#undef MAKE_KEY


static const VSFrameRef * VS_CC
convo2d_get_frame(int n, int activation_reason, void **instance_data,
                  void **frame_data, VSFrameContext *frame_ctx,
                  VSCore *core, const VSAPI *vsapi)
{
    convo2d_t *ch = (convo2d_t *)*instance_data;

    if (activation_reason == arInitial) {
        vsapi->requestFrameFilter(n, ch->node, frame_ctx);
        return NULL;
    }

    if (activation_reason != arAllFramesReady) {
        return NULL;
    }

    const VSFrameRef *src = vsapi->getFrameFilter(n, ch->node, frame_ctx);
    const VSFormat *fi = vsapi->getFrameFormat(src);
    if (fi->sampleType != stInteger) {
        return src;
    }

    const int pl[] = {0, 1, 2};
    const VSFrameRef *fr[] = {ch->planes[0] ? NULL : src,
                              ch->planes[1] ? NULL : src,
                              ch->planes[2] ? NULL : src};
    VSFrameRef *dst = vsapi->newVideoFrame2(fi, vsapi->getFrameWidth(src, 0),
                                            vsapi->getFrameHeight(src, 0),
                                            fr, pl, src, core);

    proc_convolution proc_func = get_proc_func(ch->mtype, fi->bytesPerSample);
    if (!proc_func) {
        vsapi->freeFrame(src);
        return dst;
    }
    uint16_t max = (1 << fi->bitsPerSample) - 1;

    for (int plane = 0; plane < fi->numPlanes; plane++) {
        if (fr[plane]) {
            continue;
        }
        proc_func(ch, plane, src, dst, vsapi, max);
    }

    vsapi->freeFrame(src);
    return dst;
}


static void VS_CC
init_convo2d(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
             VSCore *core, const VSAPI *vsapi)
{
    convo2d_t *ch = (convo2d_t *)*instance_data;
    vsapi->setVideoInfo(ch->vi, 1, node);
    vsapi->clearMap(in);
}


static void VS_CC close_handler(convo2d_t *ch, const VSAPI *vsapi)
{
    if (!ch) {
        return;
    }
    if (ch->node) {
        vsapi->freeNode(ch->node);
        ch->node = NULL;
    }
    free(ch);
    ch = NULL;
}


static void VS_CC
close_convo2d(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    convo2d_t *ch = (convo2d_t *)instance_data;
    close_handler(ch, vsapi);
}


#define RET_IF_ERROR(cond, ...) \
{\
    if (cond) {\
        close_handler(ch, vsapi);\
        snprintf(msg, 240, __VA_ARGS__);\
        vsapi->setError(out, msg_buff);\
        return;\
    }\
}

static void VS_CC
create_convo2d(const VSMap *in, VSMap *out, void *user_data, VSCore *core,
               const VSAPI *vsapi)
{
    char msg_buff[256] = "convolution: ";
    char *msg = msg_buff + strlen(msg_buff);
    int err;

    convo2d_t *ch = (convo2d_t *)calloc(sizeof(convo2d_t), 1);
    RET_IF_ERROR(!ch, "failed to allocate handler");

    ch->node = vsapi->propGetNode(in, "clip", 0, 0);
    ch->vi = vsapi->getVideoInfo(ch->node);

    
    int num = vsapi->propNumElements(in, "planes");
    if (num < 1) {
        for (int i = 0; i < 3; ch->planes[i++] = 1);
    } else {
        for (int i = 0; i < num; i++) {
            int p = (int)vsapi->propGetInt(in, "planes", i, NULL);
            RET_IF_ERROR(p < 0 || p > 2, "planes index out of range");
            ch->planes[p] = 1;
        }
    }

    num = vsapi->propNumElements(in, "matrix");
    RET_IF_ERROR(num > 0 && num != 3 && num != 5 && num != 9 && num != 25,
                 "invalid matrix");
    if (num == 3 || num == 5) {
        ch->mtype = num == 3 ? MATRIX_TYPE_3H : MATRIX_TYPE_5H;
        const char *mode = vsapi->propGetData(in, "mode", 0, &err);
        if (!err && mode[0] == 'v') {
            ch->mtype = num == 3 ? MATRIX_TYPE_3V : MATRIX_TYPE_5V;
        }
    }
    if (num == 25) {
        ch->mtype = MATRIX_TYPE_5X5;
    }

    ch->m[4] = 1;
    for (int i = 0; i < num; i++) {
        int element = (int)vsapi->propGetInt(in, "matrix", i, NULL);
        ch->m[i] = element;
        ch->div += element;
    }
    if (ch->div == 0.0) {
        ch->div = 1.0;
    }

    ch->bias = vsapi->propGetFloat(in, "bias", 0, &err);
    if (err) {
        ch->bias = 0.0;
    }
    ch->bias += 0.5;

    double div = vsapi->propGetFloat(in, "divisor", 0, &err);
    if (!err && div != 0.0) {
        ch->div = div;
    }
    vsapi->createFilter(in, out, "Convolution", init_convo2d,
                        convo2d_get_frame, close_convo2d, fmParallel,
                        0, ch, core);
}
#undef RET_IF_ERROR


VS_EXTERNAL_API(void) VapourSynthPluginInit(
    VSConfigPlugin f_config, VSRegisterFunction f_register, VSPlugin *plugin)
{
    f_config("chikuzen.does.not.have.his.own.domain.convo2d", "convo2d",
             "Spatial convolution filter for VapourSynth v" CONVO2D_VERSION,
             VAPOURSYNTH_API_VERSION, 1, plugin);
    f_register("Convolution",
               "clip:clip;matrix:int[]:opt;bias:float:opt;divisor:float:opt;"
               "planes:int[]:opt;mode:data:opt;",
               create_convo2d, NULL, plugin);
}
