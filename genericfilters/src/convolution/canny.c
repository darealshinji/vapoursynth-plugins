/*
  canny.c: Copyright (C) 2013  Oka Motofumi

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
#include <math.h>

#define USE_ALIGNED_MALLOC
#include "common.h"
#include "canny.h"


static stack_t *create_stack(int size)
{
    stack_t *s = (stack_t *)malloc(sizeof(stack_t));
    if (!s) {
        return NULL;
    }
    s->pos = (int32_t *)malloc(size * sizeof(int32_t));
    if (!s->pos) {
        free(s);
        return NULL;
    }
    s->map = (uint8_t *)malloc(size);
    if (!s->map) {
        free(s->pos);
        free(s);
        return NULL;
    }
    return s;
}


static void free_stack(stack_t *s)
{
    if(!s) {
        return;
    }
    free(s->pos);
    s->pos = NULL;
    free(s->map);
    s->map = NULL;
    free(s);
    s = NULL;
}


static void VS_CC
canny_get_frame(generic_handler_t *gh, const VSFormat *fi, const VSFrameRef **fr,
                const VSAPI *vsapi, const VSFrameRef *src, VSFrameRef *dst)
{
    canny_t *ch = gh->fdata;

    int fwidth = vsapi->getFrameWidth(src, 0);
    int fheight = vsapi->getFrameHeight(src, 0);
    int fstride = ((fwidth + 15) / 16) * 16;
    int bstride = ((fwidth + 32 + 15) / 16) * 16;

    float *blur = (float *)_aligned_malloc(fstride * fheight * sizeof(float), 16);
    float *edge = (float *)_aligned_malloc(fstride * fheight * sizeof(float), 16);
    uint8_t *direction = (uint8_t *)_aligned_malloc(fstride * fheight, 16);
    float *buff = (float *)_aligned_malloc(bstride * sizeof(float) * 3, 16);
    stack_t *stack = create_stack(fwidth * fheight);

    if (!blur || !buff || !edge || !direction || !stack) {
        goto close_canny;
    }

    int idx = fi->bytesPerSample - 1;

    for (int plane = 0; plane < fi->numPlanes; plane++) {
        if (fr[plane]) {
            continue;
        }

        int width = vsapi->getFrameWidth(src, plane);
        int height = vsapi->getFrameHeight(src, plane);
        int stride = vsapi->getStride(src, plane);
        const uint8_t *srcp = vsapi->getReadPtr(src, plane);
        uint8_t *dstp = vsapi->getWritePtr(dst, plane);
        if (width < 16 || height < 16) {
            continue;
        }

        ch->gaussian_blur[idx](ch->radius, ch->kernel, srcp, buff + 8, blur,
                               width, height, stride, fstride);
        
        edge_detect(buff + 8, bstride, blur, edge, direction, width, height,
                    fstride);

        non_max_suppress(edge, blur, direction, width, height, fstride);

        hysteresis(blur, width, height, fstride, ch->th, ch->tl, stack);

        write_dst_canny[idx](blur, dstp, width, height, fstride, stride, ch->th,
                             fi->bitsPerSample);

    }

close_canny:
    _aligned_free(blur);
    _aligned_free(edge);
    _aligned_free(direction);
    _aligned_free(buff);
    free_stack(stack);
}


static void VS_CC
gblur_get_frame(generic_handler_t *gh, const VSFormat *fi, const VSFrameRef **fr,
                const VSAPI *vsapi, const VSFrameRef *src, VSFrameRef *dst)
{
    canny_t *ch = gh->fdata;

    int fwidth = vsapi->getFrameWidth(src, 0);
    int fheight = vsapi->getFrameHeight(src, 0);
    int fstride = ((fwidth + 15) / 16) * 16;
    int bstride = ((fwidth + 32 + 15) / 16) * 16;

    float *blur = (float *)_aligned_malloc(fstride * fheight * sizeof(float), 16);
    float *buff = (float *)_aligned_malloc(bstride * sizeof(float), 16);

    if (!blur || !buff) {
        goto close_gblur;
    }

    int idx = fi->bytesPerSample - 1;

    for (int plane = 0; plane < fi->numPlanes; plane++) {
        if (fr[plane]) {
            continue;
        }

        int width = vsapi->getFrameWidth(src, plane);
        int height = vsapi->getFrameHeight(src, plane);
        int stride = vsapi->getStride(src, plane);
        const uint8_t *srcp = vsapi->getReadPtr(src, plane);
        uint8_t *dstp = vsapi->getWritePtr(dst, plane);
        if (width < 16 || height < 16) {
            continue;
        }

        ch->gaussian_blur[idx](ch->radius, ch->kernel, srcp, buff + 8, blur,
                               width, height, stride, fstride);

        write_dst_gblur[idx](blur, dstp, width, height, fstride, stride, 0.0f,
                             fi->bitsPerSample);

    }

close_gblur:
    _aligned_free(blur);
    _aligned_free(buff);
}


static void VS_CC
set_kernel(float sigma, int *radius, float *kernel)
{
    if (sigma == 0.0f) {
        return;
    }

    int rad = (int)(sigma * 3.0f + 0.5f);
    if (rad < 1) {
        rad = 1;
    }

    float sum = 0.0f;
    for (int i = -rad; i <= rad; i++) {
        float weight = expf(-(i * i)/(2.0f * sigma * sigma));
        kernel[i + rad] = weight;
        sum += weight;
    }
    for (int i = 0, len = rad * 2 + 1; i < len; kernel[i++] /= sum);
    *radius = rad;
}


static void VS_CC
set_canny_data(generic_handler_t *gh, filter_id_t id, char *msg,
               const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    canny_t *ch = (canny_t *)calloc(sizeof(canny_t), 1);
    RET_IF_ERROR(!ch, "failed to allocate filter data");
    gh->fdata = ch;

    int err;
    float sigma = (float)vsapi->propGetFloat(in, "sigma", 0, &err);
    if (err) {
        sigma = id == ID_CANNY ? 1.50f : 0.50f;
    }
    RET_IF_ERROR(sigma < 0, "sigma must be greater than zero");
    RET_IF_ERROR(sigma > 2.833f, "sigma must be lesser than 2.83");

    if (id == ID_CANNY) {
        ch->tl = (float)vsapi->propGetFloat(in, "tl", 0, &err);
        if (err) {
            ch->tl = 1.0f;
        }
        RET_IF_ERROR(ch->tl < 0.0f, "tl must be greater than zero");
        ch->th = (float)vsapi->propGetFloat(in, "th", 0, &err);
        if (err) {
            ch->th = 8.0f;
        }
        RET_IF_ERROR(ch->th < ch->tl, "th must be greater than tl");
    }

    set_kernel(sigma, &ch->radius, ch->kernel);

    ch->gaussian_blur = sigma == 0.0f ? convert_to_float : gblur;

    gh->get_frame_filter = id == ID_CANNY ? canny_get_frame : gblur_get_frame;
}

const set_filter_data_func set_canny = set_canny_data;
