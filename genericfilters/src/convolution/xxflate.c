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
#include "xxflate.h"


static void VS_CC
xxflate_get_frame(generic_handler_t *gh, const VSFormat *fi,
                  const VSFrameRef **fr, const VSAPI *vsapi,
                  const VSFrameRef *src, VSFrameRef *dst)
{
    xxflate_t *xh = gh->fdata;

    int bps = fi->bytesPerSample;
    int bstride = ((vsapi->getFrameWidth(src, 0) * bps + 32 + 15) / 16) * 16;
    uint8_t *buff = (uint8_t *)_aligned_malloc(bstride * 3, 16);
    if (!buff) {
        return;
    }

    int idx = bps == 1 ? 0 : fi->bitsPerSample == 16 ? 2 : 1;

    for (int plane = 0; plane < fi->numPlanes; plane++) {
        if (fr[plane]) {
            continue;
        }

        int width = vsapi->getFrameWidth(src, plane);
        int height = vsapi->getFrameHeight(src, plane);
        if (width < 2 || height < 2) {
            continue;
        }

        xh->function[idx](buff, bstride, width, height,
                          vsapi->getStride(src, plane),
                          vsapi->getWritePtr(dst, plane),
                          vsapi->getReadPtr(src, plane),
                          xh->th);
    }

    _aligned_free(buff);
}


static void VS_CC
set_xxflate_data(generic_handler_t *gh, filter_id_t id, char *msg,
                 const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    xxflate_t *xh = (xxflate_t *)calloc(sizeof(xxflate_t), 1);
    RET_IF_ERROR(!xh, "failed to allocate filter data");
    gh->fdata = xh;

    int err;
    xh->th = (int)vsapi->propGetInt(in, "threshold", 0, &err);
    if (err || xh->th > 0xFFFF || xh->th < 0) {
        xh->th = 0xFFFF;
    }

    xh->function = id == ID_INFLATE ? inflate : deflate;

    gh->get_frame_filter = xxflate_get_frame;
}


const set_filter_data_func set_xxflate = set_xxflate_data;
