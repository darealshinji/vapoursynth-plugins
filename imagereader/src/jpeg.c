/*
  jpgread.c:

  This file is part of vsimagereader

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


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "turbojpeg.h"
#include "imagereader.h"


static int VS_CC read_jpeg(img_hnd_t *ih, int n)
{
    FILE *fp = imgr_fopen(ih->src[n].name);
    if (!fp) {
        return -1;
    }

    unsigned long read = fread(ih->src_buff, 1, ih->src[n].image_size, fp);
    fclose(fp);
    if (read < ih->src[n].image_size) {
        return -1;
    }

    tjhandle tjh = (tjhandle)ih->tjhandle;
    if (tjDecompressToYUV(tjh, ih->src_buff, read, ih->image_buff, 0)) {
        return -1;
    }

    ih->write_frame = func_write_planar;
    ih->row_adjust = 4;

    return 0;
}


static VSPresetFormat VS_CC tjsamp_to_vspresetformat(enum TJSAMP tjsamp)
{
    const struct {
        enum TJSAMP tjsample_type;
        VSPresetFormat vsformat;
    } table[] = {
        { TJSAMP_444,  pfYUV444P8 },
        { TJSAMP_422,  pfYUV422P8 },
        { TJSAMP_420,  pfYUV420P8 },
        { TJSAMP_GRAY, pfGray8    },
        { TJSAMP_440,  pfYUV440P8 },
        { tjsamp,      pfNone     }
    };

    int i = 0;
    while (table[i].tjsample_type != tjsamp) i++;
    return table[i].vsformat;
}


static const char * VS_CC
check_jpeg(img_hnd_t *ih, int n, FILE *fp, vs_args_t *va)
{
    struct stat st;
#ifdef _WIN32
    wchar_t tmp[FILENAME_MAX * 2];
    MultiByteToWideChar(CP_UTF8, 0, ih->src[n].name, -1, tmp, FILENAME_MAX * 2);
    if (wstat(tmp, &st)) {
#else
    if (stat(ih->src[n].name, &st)) {
#endif
        return "source file does not exist";
    }
    ih->src[n].image_size = st.st_size;
    if (ih->src_buff_size < st.st_size) {
        ih->src_buff_size = st.st_size;
        free(ih->src_buff);
        ih->src_buff = malloc(ih->src_buff_size);
        if (!ih->src_buff) {
            return "failed to allocate read buffer";
        }
    }

    unsigned long read = fread(ih->src_buff, 1, st.st_size, fp);
    fclose(fp);
    if (read < st.st_size) {
        return "failed to read jpeg file";
    }

    int subsample, width, height;
    tjhandle handle = (tjhandle)ih->tjhandle;
    if (tjDecompressHeader2(handle, ih->src_buff, read, &width, &height,
                            &subsample) != 0) {
        return tjGetErrorStr();
    }

    if (subsample == TJSAMP_420 || subsample == TJSAMP_422) {
        width += width & 1;
    }
    if (subsample == TJSAMP_420 || subsample == TJSAMP_440) {
        height += height & 1;
    }

    ih->src[n].width = width;

    ih->src[n].height = height;

    VSPresetFormat pf = tjsamp_to_vspresetformat(subsample);
    ih->src[n].format = va->vsapi->getFormatPreset(pf, va->core);

    uint32_t row_size = tjBufSizeYUV(width, height, subsample) / height;
    if (row_size > va->max_row_size) {
        va->max_row_size = row_size;
    }

    ih->src[n].read = read_jpeg;

    return NULL;
}

const func_check_src check_src_jpeg = check_jpeg;
