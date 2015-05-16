/*
  edge.c: Copyright (C) 2013  Oka Motofumi

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
#include "edge.h"


static void VS_CC
edge_get_frame(generic_handler_t *gh, const VSFormat *fi, const VSFrameRef **fr,
               const VSAPI *vsapi, const VSFrameRef *src, VSFrameRef *dst)
{
    edge_t *eh = gh->fdata;

    int bps = fi->bytesPerSample;
    int bheight = eh->function == tedge ? 5 : 3;
    int bstride = ((vsapi->getFrameWidth(src, 0) * bps + 32 + 15) / 16) * 16;
    uint8_t *buff = (uint8_t *)_aligned_malloc(bstride * bheight, 16);
    if (!buff) {
        return;
    }

    int idx = bps == 1 ? 0 : fi->bitsPerSample == 16 ? 2 : 1;
    uint16_t plane_max = (uint16_t)((1 << fi->bitsPerSample) - 1);

    for (int plane = 0; plane < fi->numPlanes; plane++) {
        if (fr[plane]) {
            continue;
        }

        int width = vsapi->getFrameWidth(src, plane);
        int height = vsapi->getFrameHeight(src, plane);
        if (width < 2 || height < 2) {
            continue;
        }

        eh->function[idx](buff, bstride, width, height,
                          vsapi->getStride(src, plane),
                          vsapi->getWritePtr(dst, plane),
                          vsapi->getReadPtr(src, plane),
                          eh, plane_max);
    }
    _aligned_free(buff);
}


static void VS_CC
set_edge_data(generic_handler_t *gh, filter_id_t id, char *msg, const VSMap *in,
              VSMap *out, const VSAPI *vsapi)
{
    edge_t *eh = (edge_t *)calloc(sizeof(edge_t), 1);
    RET_IF_ERROR(!eh, "failed to allocate filter data");
    gh->fdata = eh;

    eh->function = sobel;
    if (id == ID_PREWITT) {
        eh->function = prewitt;
    } else if (id == ID_TEDGE) {
        eh->function = tedge;
    }

    int err;

    eh->min = (int)vsapi->propGetInt(in, "min", 0, &err);
    if (err || eh->min < 0 || eh->min > 0xFFFF) {
        eh->min = 0;
    }

    eh->max = (int)vsapi->propGetInt(in, "max", 0, &err);
    if (err || eh->max < 0 || eh->max > 0xFFFF) {
        eh->max = 0xFFFF;
    }

    eh->rshift = (int)vsapi->propGetInt(in, "rshift", 0, &err);
    if (err) {
        eh->rshift = 0;
    }

    gh->get_frame_filter = edge_get_frame;
}


const set_filter_data_func set_edge = set_edge_data;
