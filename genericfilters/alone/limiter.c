/*
  limiter.c: Copyright (C) 2012-2013  Oka Motofumi

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


static void VS_CC set_lut(alone_t *ah, int min, int max)
{
    uint16_t *lut = ah->lut;
    size_t size = ah->lut_size;

    int i = 0;
    while (i < min) lut[i++] = min;
    for (; i < max; i++) lut[i] = i;
    while (i < size) lut[i++] = max;
}


static void VS_CC
set_limiter_data(generic_handler_t *gh, filter_id_t id, char *msg,
                 const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    RET_IF_ERROR(!gh->vi->format, "format is not constant");

    int err;

    int th_min = (int)vsapi->propGetInt(in, "min", 0, &err);
    if (err || th_min < 0) {
        th_min = 0;
    }

    int max = (1 << gh->vi->format->bitsPerSample) - 1;
    int th_max = (int)vsapi->propGetInt(in, "max", 0, &err);
    if (err || th_max > max) {
        th_max = max;
    }

    RET_IF_ERROR(th_min > th_max, "min is larger than max");

    const char *ret = set_alone(gh);
    RET_IF_ERROR(ret, "%s", ret);
    alone_t *ah = (alone_t *)gh->fdata;

    set_lut(ah, th_min, th_max);
}


const set_filter_data_func set_limiter = set_limiter_data;
