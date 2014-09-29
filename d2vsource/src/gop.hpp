/*
 * VapourSynth D2V Plugin
 *
 * Copyright (c) 2012 Derek Buitenhuis
 *
 * This file is part of d2vsource.
 *
 * d2vsource is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * d2vsource is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with d2vsource; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef GOP_H
#define GOP_H

#include <vector>

extern "C" {
#include <stdint.h>
}

#define GOP_FLAG_PROGRESSIVE 0x80
#define GOP_FLAG_CLOSED 0x400

#define FRAME_FLAG_RFF 0x01

using namespace std;

typedef struct frame {
    int gop;
    int offset;
} frame;

typedef struct gop {
    uint16_t info;
    int matrix;
    int file;
    uint64_t pos;
    int skip;
    int vob;
    int cell;
    vector<uint8_t> flags;
} gop;

#endif
