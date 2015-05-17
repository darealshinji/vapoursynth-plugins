/*
  bmp.c

  This file is part of vsimagereader

  Copyright (C) 2012-2013  Oka Motofumi

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


#include <stdlib.h>

#include "imagereader.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t file_type;
    uint32_t file_size;
    uint16_t reserved0;
    uint16_t reserved1;
    uint32_t offset_data;
    uint32_t header_size;
    int32_t width;
    int32_t height;
    uint16_t num_planes;
    uint16_t bits_per_pix;
    uint32_t fourcc;
    uint32_t image_size;
    int32_t pix_per_meter_h;
    int32_t pix_per_meter_v;
    uint32_t num_palettes;
    uint32_t indx_palettes;
} bmp_header_t;
#pragma pack(pop)

#define BMP_HEADER_MAGIC (0x4D42)


int VS_CC read_bmp(img_hnd_t *ih, int n)
{
    bmp_header_t h;

    FILE *fp = imgr_fopen(ih->src[n].name);
    if (!fp) {
        return -1;
    }
    if (fread(&h, 1, sizeof(bmp_header_t), fp) != sizeof(bmp_header_t)) {
        fclose(fp);
        return -1;
    };

    ih->misc = IMG_ORDER_BGR;
    ih->row_adjust = 4;
    if (h.bits_per_pix < 24) {
        fread(ih->palettes, sizeof(color_palette_t), 1 << h.bits_per_pix, fp);
        ih->misc |= h.bits_per_pix;
        ih->write_frame = func_write_palette;
    } else if (h.bits_per_pix == 24) {
        ih->write_frame = func_write_rgb24;
    } else {
        ih->write_frame = func_write_rgb32;
    }

    fseek(fp, h.offset_data, SEEK_SET);
    uint32_t read = fread(ih->image_buff, 1, ih->src[n].image_size, fp);
    fclose(fp);
    if (read != ih->src[n].image_size) {
        return -1;
    }

    return 0;
}


const char * VS_CC
check_bmp(img_hnd_t *ih, int n, FILE *fp, vs_args_t *va)
{
    bmp_header_t h = { 0 };
    if (fread(&h, 1, sizeof(bmp_header_t), fp) != sizeof(bmp_header_t) ||
        h.file_type != BMP_HEADER_MAGIC || h.header_size != 40 ||
        h.num_planes != 1 || h.fourcc != 0 ||
        (h.bits_per_pix != 1 && h.bits_per_pix != 2 &&
         h.bits_per_pix != 4 && h.bits_per_pix != 8 &&
         h.bits_per_pix != 24 && h.bits_per_pix != 32)) {
        return "unsupported format";
    }

    ih->src[n].width = abs(h.width);

    ih->src[n].height = abs(h.height);
    
    ih->src[n].format = va->vsapi->getFormatPreset(pfRGB24, va->core);

    uint32_t row_size = (((ih->src[n].width * h.bits_per_pix + 7) / 8) + 3) & ~3;
    ih->src[n].image_size = row_size * ih->src[n].height;
    if (row_size > va->max_row_size) {
        va->max_row_size = row_size;
    }

    ih->src[n].read = read_bmp;
    ih->src[n].flip = 1;

    return NULL;
}


const func_check_src check_src_bmp = check_bmp;
