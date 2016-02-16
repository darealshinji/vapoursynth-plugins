/*
  alone.h: Copyright (C) 2012-2013  Oka Motofumi

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


#ifndef ALONE_COMMON_HEADER
#define ALONE_COMMON_HEADER

#include <stdint.h>
#include "common.h"

typedef struct filter_data alone_t;

typedef const char * (VS_CC *set_alone_handler_func)(generic_handler_t *);

struct filter_data {
    size_t lut_size;
    uint16_t *lut;
};


extern const set_alone_handler_func set_alone;


#endif
