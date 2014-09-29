/*
  binarize.c: Copyright (C) 2012-2013  Oka Motofumi

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


#include "alone.h"


static void VS_CC set_lut(alone_t *ah, int thresh, uint16_t val0, uint16_t val1)
{
    size_t size = ah->lut_size;
    uint16_t *lut = ah->lut;

    int i = 0;
    while (i < thresh) lut[i++] = val0;
    while (i < size) lut[i++] = val1;
}


static void VS_CC
set_binarize_data(generic_handler_t *gh, filter_id_t id, char *msg,
                  const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    RET_IF_ERROR(!gh->vi->format, "format is not constant");

    int err;
    int max = (1 << gh->vi->format->bitsPerSample) - 1;
    int thresh = (int)vsapi->propGetInt(in, "threshold", 0, &err);
    if (err || thresh < 0 || thresh > max) {
        thresh = max / 2 + 1;
    }

    int val0 = (int)vsapi->propGetInt(in, "v0", 0, &err);
    if (err || val0 < 0 || val0 > max) {
        val0 = 0;
    }

    int val1 = (int)vsapi->propGetInt(in, "v1", 0, &err);
    if (err || val1 < 0 || val1 > max) {
        val1 = max;
    }

    const char *ret = set_alone(gh);
    RET_IF_ERROR(ret, "%s", ret);

    set_lut((alone_t *)gh->fdata, thresh, val0, val1);
}


const set_filter_data_func set_binarize = set_binarize_data;
