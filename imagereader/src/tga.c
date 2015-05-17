/*
  tga.c

  original author Matthias Brueckner (libtga)
  modified by Oka Motofumi

  Copyright (C) 2001-2002  Matthias Brueckner <matbrc@gmx.de>
  Copyright (C) 2012-2013  Oka Motofumi <chikuzen.mo at gmail dot com>

  This file is part of vstgareader

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

#define TGA_HEADER_SIZE 18

typedef enum {
    TGA_OK,
    TGA_ERROR,
    TGA_SEEK_FAIL,
    TGA_READ_FAIL,
    TGA_UNKNOWN_FORMAT,
    TGA_UNSUPPORTED_FORMAT,
    TGA_NO_IMAGE_DATA
} tga_retcode_t;

typedef struct {
    FILE *fd;
    int id_len;
    int img_t;
    int width;
    int height;
    int depth;
} tga_t;


static inline int get_image_data_offset(tga_t *tga)
{
    return TGA_HEADER_SIZE + tga->id_len;
}

static inline uint32_t get_scanline_size(tga_t *tga)
{
    return tga->width * tga->depth >> 3;
}

static inline int is_encoded_data(tga_t *tga)
{
    return tga->img_t == 10;
}

static inline int bitor_int(uint8_t a, uint8_t b)
{
    return ((int)a << 8) | ((int)b);
}


static tga_retcode_t VS_CC tga_read_rle(tga_t *tga, uint8_t *buf)
{
    if (!tga || !buf) {
        return TGA_ERROR;
    }

    int repeat = 0;
    int direct = 0;
    int width = tga->width;
    int bytes = tga->depth >> 3;
    char sample[4];
    FILE *fd = tga->fd;

    for (int x = 0; x < width; x++) {
        if (repeat == 0 && direct == 0) {
            int head = getc(fd);
            if (head == EOF) {
                return TGA_ERROR;
            }
            if (head >= 128) {
                repeat = head - 127;
                if (fread(sample, bytes, 1, fd) < 1) {
                    return TGA_ERROR;
                }
            } else {
                direct = head + 1;
            }
        }
        if (repeat > 0) {
            for (int k = 0; k < bytes; k++) {
                buf[k] = sample[k];
            }
            repeat--;
        } else {
            if (fread(buf, bytes, 1, fd) < 1) {
                return TGA_ERROR;
            }
            --direct;
        }
        buf += bytes;
    }

    return TGA_OK;
}


static const char * VS_CC tga_get_error_string(tga_retcode_t code)
{
    const struct {
        tga_retcode_t retcode;
        char *error_string;
    } table[] = {
        { TGA_OK,                 "Success"                },
        { TGA_ERROR,              "Error"                  },
        { TGA_SEEK_FAIL,          "Seek failed"            },
        { TGA_READ_FAIL,          "Read failed"            },
        { TGA_UNKNOWN_FORMAT,     "Unknown format"         },
        { TGA_UNSUPPORTED_FORMAT, "Unsupported format"     },
        { TGA_NO_IMAGE_DATA,      "File has no image data" },
        { code,                   "Unkown retcode"         }
    };

    int i = 0;
    while (table[i].retcode != code) i++;
    return table[i].error_string;
}


static tga_retcode_t VS_CC tga_read_metadata(tga_t *tga)
{
    if (!tga) {
        return TGA_ERROR;
    }

    if (fseek(tga->fd, 0, SEEK_SET) != 0) {
        return TGA_SEEK_FAIL;
    }

    uint8_t tmp[TGA_HEADER_SIZE] = { 0 };
    if (fread(tmp, TGA_HEADER_SIZE, 1, tga->fd) == 0) {
        return TGA_READ_FAIL;
    }

    if (tmp[1] != 0 && tmp[1] != 1) {
        return TGA_UNKNOWN_FORMAT;
    }

    if (tmp[2] == 0) {
        return TGA_NO_IMAGE_DATA;
    }

    if (tmp[2] != 1 && tmp[2] != 2 && tmp[2] != 3 &&
        tmp[2] != 9 && tmp[2] != 10 && tmp[2] != 11) {
        return TGA_UNKNOWN_FORMAT;
    }

    if (tmp[2] != 2 && tmp[2] != 10) {
        return TGA_UNSUPPORTED_FORMAT;
    }

    if (tmp[16] != 24 && tmp[16] != 32) {
        return TGA_UNSUPPORTED_FORMAT;
    }

    tga->id_len     = tmp[0];
    tga->img_t      = tmp[2];
    tga->width      = bitor_int(tmp[13], tmp[12]);
    tga->height     = bitor_int(tmp[15], tmp[14]);
    tga->depth      = tmp[16];

    return TGA_OK;
}


static tga_retcode_t VS_CC tga_read_all_scanlines(tga_t *tga, uint8_t *buf)
{
    if (!tga || !buf) {
        return TGA_ERROR;
    }

    if (fseek(tga->fd, get_image_data_offset(tga), SEEK_SET) != 0) {
        return TGA_SEEK_FAIL;
    }

    size_t sln_size = get_scanline_size(tga);
    size_t read;
    size_t lines = tga->height;
    if (is_encoded_data(tga)) {
        for (read = 0; read < lines; read++) {
            if (tga_read_rle(tga, buf + read * sln_size) != TGA_OK) {
                break;
            }
        }
    } else {
        read = fread(buf, sln_size, lines, tga->fd);
    }

    return read == lines ? TGA_OK : TGA_READ_FAIL;
}


static int VS_CC read_tga(img_hnd_t *ih, int n)
{
    FILE *fp = imgr_fopen(ih->src[n].name);
    if (!fp) {
        return -1;
    }

    tga_t tga;
    tga.fd = fp;

    tga_retcode_t ret = tga_read_metadata(&tga);
    if (ret != TGA_OK) {
        fclose(fp);
        return -1;
    }

    ret = tga_read_all_scanlines(&tga, ih->image_buff);
    fclose(fp);
    if (ret != TGA_OK) {
        return -1;
    }

    ih->misc = IMG_ORDER_BGR;
    ih->row_adjust = 1;
    ih->write_frame = tga.depth == 24 ? func_write_rgb24 : func_write_rgb32;

    return 0;
}


static const char * VS_CC
check_tga(img_hnd_t *ih, int n, FILE *fp, vs_args_t *va)
{
    tga_t tga = {0};
    tga.fd = fp;
    tga_retcode_t ret = tga_read_metadata(&tga);
    if (ret != TGA_OK) {
        return tga_get_error_string(ret);
    }

    ih->src[n].width = tga.width;

    ih->src[n].height = tga.height;

    uint32_t row_size = tga.width * (tga.depth >> 3);
    if (row_size > va->max_row_size) {
        va->max_row_size = row_size;
    }
    
    ih->src[n].format = va->vsapi->getFormatPreset(pfRGB24, va->core);
    
    ih->src[n].read = read_tga;

    ih->src[n].flip = 1;

    return NULL;
}

const func_check_src check_src_tga = check_tga;
