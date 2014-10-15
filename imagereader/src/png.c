/*
  pngread.c:

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

#include <stdio.h>
#include <stdint.h>
#ifdef ENABLE_NEW_PNG
#include "pnglibconf.h"
#include "pngconf.h"
#endif
#include "png.h"
#include "imagereader.h"

#define PNG_SIG_LENGTH 8


static int VS_CC read_png(img_hnd_t *ih, int n)
{
    FILE *fp = imgr_fopen(ih->src[n].name);
    if (!fp) {
        return -1;
    }

    png_structp p_str =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!p_str) {
        fclose(fp);
        return -1;
    }

    png_infop p_info = png_create_info_struct(p_str);
    if (!p_info) {
        fclose(fp);
        png_destroy_read_struct(&p_str, NULL, NULL);
        return -1;
    }

    png_init_io(p_str, fp);
    png_read_info(p_str, p_info);

    png_uint_32 width, height;
    int color_type, bit_depth;
    png_get_IHDR(p_str, p_info, &width, &height, &bit_depth, &color_type,
                 NULL, NULL, NULL);
    if (color_type & PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(p_str);
    }
    if (bit_depth < 8) {
        png_set_packing(p_str);
    }
    if (bit_depth > 8) {
        png_set_swap(p_str);
    }
    if (ih->enable_alpha == 0) {
        if (color_type & PNG_COLOR_MASK_ALPHA) {
            png_set_strip_alpha(p_str);
        }
    } else if ((color_type & PNG_COLOR_MASK_ALPHA) == 0) {
        png_set_add_alpha(p_str, 0x00, PNG_FILLER_AFTER);
    }
    png_read_update_info(p_str, p_info);
    png_read_image(p_str, ih->png_row_index);

    fclose(fp);
    png_destroy_read_struct(&p_str, &p_info, NULL);

    ih->misc = IMG_ORDER_RGB;
    ih->row_adjust = 1;

    switch ((ih->src[n].format->id << 1) | ih->enable_alpha) {
    case (pfRGB24 << 1 | 0):
        ih->write_frame = func_write_rgb24;
        break;
    case (pfRGB24 << 1 | 1):
        ih->write_frame = func_write_rgb32;
        break;
    case (pfRGB48 << 1 | 0):
        ih->write_frame = func_write_rgb48;
        break;
    case (pfRGB48 << 1 | 1):
        ih->write_frame = func_write_rgb64;
        break;
    case (pfGray8 << 1 | 0):
    case (pfGray16 << 1 | 0):
        ih->write_frame = func_write_planar;
        break;
    case (pfGray8 << 1 | 1):
        ih->write_frame = func_write_gray8_a;
        break;
    case (pfGray16 << 1 | 1):
        ih->write_frame = func_write_gray16_a;
        break;
    default:
        break;
    }

    return 0;
}


#define COLOR_OR_BITS(color, bits) \
    (((uint32_t)color << 16) | (uint32_t)bits)
static VSPresetFormat VS_CC get_dst_format(int color_type, int bits)
{
    uint32_t p_color = COLOR_OR_BITS(color_type, bits);
    const struct {
        uint32_t png_color_type;
        VSPresetFormat vsformat;
    } table[] = {
        { COLOR_OR_BITS(PNG_COLOR_TYPE_GRAY,        8),  pfGray8  },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_GRAY_ALPHA,  8),  pfGray8  },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_GRAY,       16),  pfGray16 },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_GRAY_ALPHA, 16),  pfGray16 },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_RGB,         8),  pfRGB24  },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_RGB_ALPHA,   8),  pfRGB24  },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_RGB,        16),  pfRGB48  },
        { COLOR_OR_BITS(PNG_COLOR_TYPE_RGB_ALPHA,  16),  pfRGB48  },
        { p_color, pfNone }
    };

    int i = 0;
    while (table[i].png_color_type != p_color) i++;
    return table[i].vsformat;
}
#undef COLOR_OR_BITS


static const char * VS_CC
check_png(img_hnd_t *ih, int n, FILE *fp, vs_args_t *va)
{
    uint8_t signature[PNG_SIG_LENGTH];
    if (fread(signature, 1, PNG_SIG_LENGTH, fp) != PNG_SIG_LENGTH ||
        png_sig_cmp(signature, 0, PNG_SIG_LENGTH)) {
        return "unsupported format";
    }

    png_structp p_str = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                               NULL, NULL);
    if (!p_str) {
        return "failed to create png_read_struct";
    }

    png_infop p_info = png_create_info_struct(p_str);
    if (!p_info) {
        png_destroy_read_struct(&p_str, NULL, NULL);
        return "failed to create png_info_struct";
    }

    png_init_io(p_str, fp);
    png_set_sig_bytes(p_str, PNG_SIG_LENGTH);
    png_read_info(p_str, p_info);

    png_uint_32 width, height;
    int color_type, bit_depth;
    png_get_IHDR(p_str, p_info, &width, &height, &bit_depth, &color_type,
                 NULL, NULL, NULL);
    if (color_type & PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(p_str);
    }
    if (bit_depth < 8) {
        png_set_packing(p_str);
    }
    if (ih->enable_alpha == 0) {
        if (color_type & PNG_COLOR_MASK_ALPHA) {
            png_set_strip_alpha(p_str);
        }
    } else if ((color_type & PNG_COLOR_MASK_ALPHA) == 0) {
        png_set_add_alpha(p_str, 0x00, PNG_FILLER_AFTER);
    }

    png_read_update_info(p_str, p_info);
    png_get_IHDR(p_str, p_info, &width, &height, &bit_depth, &color_type,
                 NULL, NULL, NULL);
    uint32_t row_size = png_get_rowbytes(p_str, p_info);

    png_destroy_read_struct(&p_str, &p_info, NULL);

    ih->src[n].width = width;

    ih->src[n].height = height;

    VSPresetFormat pf = get_dst_format(color_type, bit_depth);
    if (pf == pfNone) {
        return "unsupported png color type";
    }
    ih->src[n].format = va->vsapi->getFormatPreset(pf, va->core);

    ih->src[n].read = read_png;

    ih->src[n].flip = 0;

    if (row_size > va->max_row_size) {
        va->max_row_size = row_size;
    }

    return NULL;
}


const func_check_src check_src_png = check_png;
