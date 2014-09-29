/*
  hysteresis.h: Copyright (C) 2013  Oka Motofumi

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


#ifndef HYSTERESYS_H
#define HYSTERESYS_H

#include "VapourSynth.h"

typedef struct {
    VSNodeRef *base;
    VSNodeRef *alt;
    const VSVideoInfo *vi;
    int planes[3];
} hysteresis_t;


extern const VSFilterGetFrame get_frame_hysteresis;
extern const VSFilterInit filter_init_hysteresis;
extern const VSFilterFree filter_free_hysteresis;

#endif