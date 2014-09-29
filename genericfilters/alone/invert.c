/*
  invert.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include "common.h"


static void VS_CC
invert_get_frame(generic_handler_t *gh, const VSFormat *fi,
                 const VSFrameRef **fr, const VSAPI *vsapi,
                 const VSFrameRef *src, VSFrameRef *dst)
{
    int bps = fi->bitsPerSample;
    for (int plane = 0; plane < fi->numPlanes; plane++) {
        if (fr[plane]) {
            continue;
        }

        int plane_size = (vsapi->getStride(src, plane) / sizeof(unsigned)) *
                         vsapi->getFrameHeight(src, plane);
        const unsigned *srcp = (unsigned *)vsapi->getReadPtr(src, plane);
        unsigned *dstp = (unsigned *)vsapi->getWritePtr(dst, plane);

        if ((bps & 7) == 0) {
            while (plane_size--) {
                *dstp++ = ~(*srcp++);
            }
            continue;
        }

        unsigned mask = (1 << bps) - 1;
        int usize = sizeof(unsigned) / sizeof(uint16_t);
        while (usize-- > 1) {
            mask = (mask << 16) | mask;
        }

        while (plane_size--) {
            *dstp++ = *srcp++ ^ mask;
        }
    }
}


static void VS_CC
set_invert_data(generic_handler_t *gh, filter_id_t id, char *msg,
                const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    gh->get_frame_filter = invert_get_frame;
}


const set_filter_data_func set_invert = set_invert_data;
