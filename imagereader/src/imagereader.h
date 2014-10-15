/*
  imagereader.h

  This file is part of vsimagereader

  Copyright (C) 2013  Oka Motofumi

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


#ifndef VS_IMAGE_READER_H
#define VS_IMAGE_READER_H

#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "VapourSynth.h"

#define IMG_ORDER_BGR 0x0100
#define IMG_ORDER_RGB 0x0200

typedef struct {
    const VSMap *in;
    VSMap *out;
    VSCore *core;
    const VSAPI *vsapi;
    int max_row_size;
    int max_height;
    int variable_width;
    int variable_height;
    int variable_format;
} vs_args_t;

typedef struct image_handler img_hnd_t;

typedef const char * (VS_CC *func_check_src)(img_hnd_t *, int, FILE *,
                                              vs_args_t *);

typedef int (VS_CC *func_read_image)(img_hnd_t *, int);

typedef void (VS_CC *func_write_frame)(img_hnd_t *, int, VSFrameRef **,
                                        VSCore *core, const VSAPI *);

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} color_palette_t;

typedef struct {
    const char *name;
    func_read_image read;
    unsigned long image_size;
    uint16_t width;
    uint16_t height;
    const VSFormat *format;
    int flip;
} src_info_t;

struct image_handler {
    VSVideoInfo vi[2]; // 0: base image, 1: for alpha
    src_info_t *src;
    uint8_t *src_buff; // libturbojpeg require this
    size_t src_buff_size;
    uint8_t *image_buff; // buffer for decoded image
    uint8_t **png_row_index; // libpng require this
    void *tjhandle; // libturbojpeg require this
    func_write_frame write_frame;
    color_palette_t palettes[256];
    int row_adjust;
    int enable_alpha;
    int misc;
};

typedef enum {
    IMG_TYPE_NONE,
    IMG_TYPE_BMP,
    IMG_TYPE_JPG,
    IMG_TYPE_PNG,
    IMG_TYPE_TGA
} image_type_t;

extern const func_check_src check_src_bmp;
extern const func_check_src check_src_jpeg;
extern const func_check_src check_src_png;
extern const func_check_src check_src_tga;

extern const func_write_frame func_write_planar;
extern const func_write_frame func_write_gray8_a;
extern const func_write_frame func_write_gray16_a;
extern const func_write_frame func_write_rgb24;
extern const func_write_frame func_write_rgb32;
extern const func_write_frame func_write_rgb48;
extern const func_write_frame func_write_rgb64;
extern const func_write_frame func_write_palette;


static inline FILE *imgr_fopen(const char *filename)
{
#ifdef _WIN32
    wchar_t tmp[FILENAME_MAX * 2];
    MultiByteToWideChar(CP_UTF8, 0, filename, -1, tmp, FILENAME_MAX * 2);
    return _wfopen(tmp, L"rb");
#else
    return fopen(filename, "rb");
#endif
}
#endif
