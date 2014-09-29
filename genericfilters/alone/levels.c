/*
  levels.c: Copyright (C) 2012-2013  Oka Motofumi

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

#include <math.h>
#include "alone.h"


static void VS_CC
set_lut(int imin, int imax, int omin, int omax, double gamma, int size,
        uint16_t *lut)
{
    int c0 = omax - omin;
    double c1 = 1.0 / (imax - imin);
    double rgamma = 1.0 / gamma;
    for (int pix = 0; pix < size; pix++) {
        lut[pix] = (uint16_t)((int)(pow(((pix - imin) * c1), rgamma) * c0 + 0.5) + omin);
    }
}


static void VS_CC
set_levels_data(generic_handler_t *gh, filter_id_t id, char *msg,
                const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    RET_IF_ERROR(!gh->vi->format, "format is not constant");

    const char *ret = set_alone(gh);
    RET_IF_ERROR(ret, "%s", ret);
    alone_t *ah = (alone_t *)gh->fdata;

    int err;
    int bps = gh->vi->format->bitsPerSample;
    int size = 1 << bps;
    int imin = (int)vsapi->propGetInt(in, "min_in", 0, &err);
    if (err || imin < 0) {
        imin = 0;
    }
    int imax = (int)vsapi->propGetInt(in, "max_in", 0, &err);
    if (err || imax > size - 1) {
        imax = 0xFF << (bps - 8);
    }
    int omin = (int)vsapi->propGetInt(in, "min_out", 0, &err);
    if (err || omin < 0) {
        omin = 0;
    }
    int omax = (int)vsapi->propGetInt(in, "max_out", 0, &err);
    if (err || omax > size - 1) {
        omax = 0xFF << (bps - 8);
    }
    double gamma = vsapi->propGetFloat(in, "gamma", 0, &err);
    if (err || gamma <= 0.0f) {
        gamma = 1.0;
    }

    set_lut(imin, imax, omin, omax, gamma, size, ah->lut);
}


const set_filter_data_func set_levels = set_levels_data;
