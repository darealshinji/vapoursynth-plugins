/*
  rawsource.h

  This file is a part of vsrawsource

  Copyright (C) 2012  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Libav; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


#ifndef VS_RAW_SOURCE_H
#define VS_RAW_SOURCE_H

#define VS_RAWS_VERSION "0.3.1"

#ifdef _WIN32
#ifdef __MINGW32__
#define rs_fseek fseeko64
#define rs_ftell ftello64
#else
#pragma warning(disable:4996)
#define rs_fseek _fseeki64
#define rs_ftell _ftelli64
#define snprintf _snprintf
#define strcasecmp stricmp
#endif
#include <windows.h>
#endif

#include <stdio.h>

#ifndef rs_fseek
#define _FILE_OFFSET_BITS 64
#define rs_fseek fseek
#define rs_ftell ftell
#endif

#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>

#ifdef __GNUC__
#include <inttypes.h>
#else
#include <stdint.h>
#define SCNi64 "lld"
#endif

typedef struct {
    uint32_t header_size;
    int32_t width;
    int32_t height;
    uint16_t num_planes;
    uint16_t bits_per_pixel;
    uint32_t fourcc;
    uint32_t image_size;
    int32_t pix_per_meter_h;
    int32_t pix_per_meter_v;
    uint32_t num_palette;
    uint32_t indx_palette;
} bmp_info_header_t;


#endif /* VS_RAW_SOURCE_H */
