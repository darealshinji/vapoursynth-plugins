/*
  convolution.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include <stdlib.h>

#define USE_ALIGNED_MALLOC
#include "common.h"
#include "convolution.h"


static void VS_CC
convolution_get_frame(generic_handler_t *gh, const VSFormat *fi,
                      const VSFrameRef **fr, const VSAPI *vsapi,
                      const VSFrameRef *src, VSFrameRef *dst)
{
    convolution_t *ch = gh->fdata;

    int bps = fi->bytesPerSample;

    uint8_t *buff = NULL;
    int bstride = 0;

    int num = 1;
    if (ch->function == convo_3x3) {
        num = 3;
    }
    if (ch->function == convo_5x5) {
        num = 5;
    }
    if (ch->function != convo_v) {
        bstride = ((vsapi->getFrameWidth(src, 0) * bps + 32 + 15) / 16) * 16;
        buff = (uint8_t *)_aligned_malloc(bstride * num, 16);
        if (!buff) {
            return;
        }
    }

    int idx = bps == 1 ? 0 : fi->bitsPerSample == 16 ? 2 : 1;

    for (int plane = 0; plane < fi->numPlanes; plane++) {
        if (fr[plane]) {
            continue;
        }

        int width = vsapi->getFrameWidth(src, plane);
        int height = vsapi->getFrameHeight(src, plane);
        if (width < 16 || height < 16) {
            continue;
        }

        ch->function[idx](ch, buff, bstride, width, height,
                          vsapi->getStride(src, plane),
                          vsapi->getWritePtr(dst, plane),
                          vsapi->getReadPtr(src, plane));
    }
    _aligned_free(buff);
}


static void VS_CC
set_matrix_and_proc_function(convolution_t *ch, const VSMap *in,
                             const VSAPI *vsapi, char *msg)
{
    int err;
    const char *mode = vsapi->propGetData(in, "mode", 0, &err);
    if (err) {
        mode = "square";
    }
    RET_IF_ERROR(!(mode[0] == 's' || mode[0] == 'h' || mode[0] == 'v'),
                 "invalid mode was specified");
    
    int num = vsapi->propNumElements(in, "matrix");
    if (num < 0) {
        num = 3;
        if (mode[0] == 's') {
            num = 9;
        }
    }
    RET_IF_ERROR(num < 3 || num % 2 == 0, "invalid matrix length");
    RET_IF_ERROR(mode[0] != 's' && num > 17, "invalid matrix length");
    RET_IF_ERROR(mode[0] == 's' && num != 9 && num != 25,
                 "invalid matrix length");

    switch (mode[0]) {
    case 's':
        ch->function = num == 9 ? convo_3x3 : convo_5x5;
        break;
    case 'h':
        ch->function = convo_h;
        break;
    case 'v':
        ch->function = convo_v;
        break;
    default:
        break;
    }

    ch->m[num / 2] = 1;
    for (int i = 0; i < num; i++) {
        int element = (int)vsapi->propGetInt(in, "matrix", i, NULL);
        RET_IF_ERROR(element < -1024 || element > 1023,
                     "matrix has out of range value");
        ch->m[i] = element;
        ch->rdiv += element;
    }

    ch->length = num;

    if (ch->rdiv == 0.0) {
        ch->rdiv = 1.0;
    }
}


static void VS_CC
set_convolution_data(generic_handler_t *gh, filter_id_t id, char *msg,
                     const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    convolution_t *ch = (convolution_t *)calloc(sizeof(convolution_t), 1);
    RET_IF_ERROR(!ch, "failed to allocate filter data");
    gh->fdata = ch;

    set_matrix_and_proc_function(ch, in, vsapi, msg);
    if (msg[0]) {
        return;
    }

    int err;
    ch->bias = vsapi->propGetFloat(in, "bias", 0, &err);
    if (err) {
        ch->bias = 0.0;
    }

    double div = vsapi->propGetFloat(in, "divisor", 0, &err);
    if (!err && div != 0.0) {
        ch->rdiv = div;
    }
    ch->rdiv = 1.0 / ch->rdiv;

    ch->saturate = (int)vsapi->propGetInt(in, "saturate", 0, &err);
    if (err || ch->saturate != 0) {
        ch->saturate = 1;
    }

    gh->get_frame_filter = convolution_get_frame;
}


static int VS_CC
get_gcd(int x, int y)
{
    if (y == 0) {
        return x;
    }
    return get_gcd(y, x % y);
}


static void VS_CC
reduce_fraction(int *a, int *b)
{
    int gcd = get_gcd(*a, *b);
    *a /= gcd;
    *b /= gcd;
}


static void VS_CC
set_blur_data(generic_handler_t *gh, filter_id_t id, char *msg,
              const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    convolution_t *ch = (convolution_t *)calloc(sizeof(convolution_t), 1);
    RET_IF_ERROR(!ch, "failed to allocate filter data");
    gh->fdata = ch;
    
    int err;
    int center_h = 1000, center_v = 1000;
    int outer_h = (int)((vsapi->propGetFloat(in, "ratio_h", 0, &err) + 0.0005) * 1000);
    if (err) {
        outer_h = 500;
    }
    int outer_v = (int)((vsapi->propGetFloat(in, "ratio_v", 0, &err) + 0.0005) * 1000);
    if (err) {
        outer_v = outer_h;
    }
    RET_IF_ERROR(outer_h < 0 || outer_h > 1000, "ratio_h is out of range");
    RET_IF_ERROR(outer_v < 0 || outer_v > 1000, "ratio_v is out of range");

    reduce_fraction(&center_h, &outer_h);
    reduce_fraction(&center_v, &outer_v);

    int matrix[] = {
        outer_h * outer_v,  center_h * outer_v,  outer_h * outer_v,
        outer_h * center_v, center_h * center_v, outer_h * center_v,
        outer_h * outer_v,  center_h * outer_v,  outer_h * outer_v
    };
    int div = 0;
    for (int i = 0; i < 9; i++) {
        ch->m[i] = matrix[i];
        div += matrix[i];
    }
    ch->rdiv = 1.0 / div;

    ch->function = convo_3x3;

    gh->get_frame_filter = convolution_get_frame;
}


const set_filter_data_func set_convolution = set_convolution_data;
const set_filter_data_func set_blur = set_blur_data;
