/*
  edge.h: Copyright (C) 2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This file is part of Generic.

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


#ifndef EDGE_DETECTION_FILTER_HEADER
#define EDGE_DETECTION_FILTER_HEADER

#include <stdint.h>
#include "VapourSynth.h"


typedef struct filter_data edge_t;

typedef void (VS_CC *proc_edge_detection)(uint8_t *, int, int, int, int,
                                          uint8_t *, const uint8_t *, edge_t *,
                                          uint16_t);

struct filter_data {
    int min;
    int max;
    int rshift;
    const proc_edge_detection *function;
};


extern const proc_edge_detection sobel[];
extern const proc_edge_detection prewitt[];
extern const proc_edge_detection tedge[];

#endif // EDGE_DETECTION_FILTER_HEADER
