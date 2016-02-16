/*
  no_simd.h: Copyright (C) 2012-2013  Oka Motofumi

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


#ifndef NO_SIMD_H
#define NO_SIMD_H

#include <string.h>
#include <stdint.h>

#define GF_FUNC_ALIGN
#define GF_ALIGN

static inline void
line_copy8(uint8_t *line, const uint8_t *srcp, int width, int mergin)
{
    memcpy(line, srcp, width);
    for (int i = mergin; i > 0; i--) {
        line[-i] = line[i];
        line[width - 1 + i] = line[width - 1 - i];
    }
}


static inline void
line_copy16(uint16_t *line, const uint16_t *srcp, int width, int mergin)
{
    memcpy(line, srcp, width * 2);
    for (int i = mergin; i > 0; i--) {
        line[-i] = line[i];
        line[width - 1 + i] = line[width - 1 - i];
    }
}


static inline void
line_copyf(float *line, const float *srcp, int width, int mergin)
{
    memcpy(line, srcp, width * sizeof(float));
    for (int i = mergin; i > 0; i--) {
        line[-i] = line[i];
        line[width - 1 + i] = line[width - 1 - i];
    }
}


static inline int max_int(int x, int y)
{
    return x > y ? x : y;
}


static inline float max_float(float x, float y)
{
    return x > y ? x : y;
}


static inline int min_int(int x, int y)
{
    return x < y ? x : y;
}


static inline float min_float(float x, float y)
{
    return x < y ? x : y;
}
#endif