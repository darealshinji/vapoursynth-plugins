/*
  convolution_hv.h: Copyright (C) 2012-2013  Oka Motofumi

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


#ifndef CONVOLUTION_HV_FILTER_HEADER
#define CONVOLUTION_HV_FILTER_HEADER


#include <stdint.h>
#include "VapourSynth.h"

typedef struct filter_data convolution_hv_t;

typedef void (VS_CC *proc_convo_hv)(convolution_hv_t *, uint8_t *, int, int,
                                     int, int, uint8_t *, const uint8_t *);

struct filter_data {
    int m_h[5];
    int m_v[5];
    double rdiv_h;
    double rdiv_v;
    double bias;
    int saturate;
    const proc_convo_hv *function;
};


extern const proc_convo_hv convo_hv5[];

#endif
