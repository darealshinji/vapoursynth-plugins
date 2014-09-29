/*
  hysteresis.c: Copyright (C) 2013  Oka Motofumi

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


#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "hysteresis.h"


typedef struct {
    uint8_t *map;
    int width;
    int height;
    uint32_t *xy;
    int index;
    int xy_size;
    int stride;
    const uint8_t *altp8;
    const uint16_t *altp16;
    uint8_t *dstp8;
    uint16_t *dstp16;
} footprint_t;


static footprint_t *
create_footprint(int width, int height, int stride, int is_16bit,
                 const uint8_t *altp, uint8_t *dstp)
{
    footprint_t *fp = (footprint_t *)malloc(sizeof(footprint_t));
    if (!fp) {
        return NULL;
    }

    fp->map = (uint8_t *)calloc(width * height, 1);
    if (!fp->map) {
        free(fp);
        return NULL;
    }

    fp->xy = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    if (!fp->xy) {
        free(fp->map);
        free(fp);
        return NULL;
    }

    fp->width = width;
    fp->height = height;
    fp->index = -1;

    if (!is_16bit) {
        fp->stride = stride;
        fp->altp8 = altp;
        fp->dstp8 = dstp;
    } else {
        fp->stride = stride / 2;
        fp->altp16 = (uint16_t *)altp;
        fp->dstp16 = (uint16_t *)dstp;
    }

    return fp;
}


static void free_footprint(footprint_t *fp) {
    if (!fp) {
        return;
    }
    if (fp->xy) {
        free(fp->xy);
        fp->xy = NULL;
    }
    if (fp->map) {
        free(fp->map);
        fp->map = NULL;
    }
    free(fp);
    fp = NULL;
}


static void push_8bit(footprint_t *fp, int x, int y)
{
    fp->dstp8[x + y * fp->stride] = fp->altp8[x + y * fp->stride];
    fp->map[x + y * fp->width] = 0xFF;
    fp->xy[++fp->index] = ((uint16_t)x << 16) | (uint16_t)y;
}


static void push_16bit(footprint_t *fp, int x, int y)
{
    fp->dstp16[x + y * fp->stride] = fp->altp16[x + y * fp->stride];
    fp->map[x + y * fp->width] = 0xFF;
    fp->xy[++fp->index] = ((uint16_t)x << 16) | (uint16_t)y;
}


static void pop(footprint_t *fp, int *x, int *y)
{
    uint32_t val = fp->xy[fp->index--];
    *x = val >> 16;
    *y = val & 0x0000FFFF;
}


static int passed(footprint_t *fp, int x, int y)
{
    return (int)fp->map[x + y * fp->width];
}


static int is_empty(footprint_t *fp)
{
    return fp->index <= -1;
}


static void VS_CC
write_hysteresis_8bit(footprint_t *fp, const uint8_t *basep)
{
    int width = fp->width;
    int height = fp->height;
    int stride = fp->stride;
    const uint8_t *altp = fp->altp8;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (basep[x + y * stride] && altp[x + y * stride] &&
                !passed(fp, x, y)) {
                push_8bit(fp, x, y);
                int posx, posy;

                while (!is_empty(fp)) {
                    pop(fp, &posx, &posy);
                    int x_min = posx > 0 ? posx - 1 : 0;
                    int x_max = posx < width - 1 ? posx + 1 : posx;
                    int y_min = posy > 0 ? posy - 1 : 0;
                    int y_max = posy < height - 1 ? posy + 1 : posy;

                    for (int yy = y_min; yy <= y_max; yy++) {
                        for (int xx = x_min; xx <= x_max; xx++) {
                            if (altp[xx + yy * stride] && !passed(fp, xx, yy)) {
                                push_8bit(fp, xx, yy);
                            }
                        }
                    }
                }
            }
        }
    }
}


static void VS_CC
write_hysteresis_16bit(footprint_t *fp, const uint8_t *b)
{
    int width = fp->width;
    int height = fp->height;
    int stride = fp->stride;
    const uint16_t *basep = (uint16_t *)b;
    const uint16_t *altp = fp->altp16;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (basep[x + y * stride] && altp[x + y * stride] &&
                !passed(fp, x, y)) {

                push_16bit(fp, x, y);
                int posx, posy;

                while (!is_empty(fp)) {
                    pop(fp, &posx, &posy);
                    int x_min = posx - (posx > 0);
                    int x_max = posx + (posx < width - 1);
                    int y_min = posy - (posy > 0);
                    int y_max = posy + (posy < height - 1);

                    for (int yy = y_min; yy <= y_max; yy++) {
                        for (int xx = x_min; xx <= x_max; xx++) {
                            if (altp[xx + yy * stride] && !passed(fp, xx, yy)) {
                                push_16bit(fp, xx, yy);
                            }
                        }
                    }
                }
            }
        }
    }
}


typedef void (VS_CC *proc_function)(footprint_t *, const uint8_t *);
static const proc_function write_hysteresis[] = {
    write_hysteresis_8bit, write_hysteresis_16bit
};


static const VSFrameRef * VS_CC
get_frame(int n, int activation_reason, void **instance_data, void **frame_data,
          VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi)
{
    hysteresis_t *hh = (hysteresis_t *)*instance_data;

    if (activation_reason == arInitial) {
        vsapi->requestFrameFilter(n, hh->base, frame_ctx);
        vsapi->requestFrameFilter(n, hh->alt, frame_ctx);
        return NULL;
    }

    if (activation_reason != arAllFramesReady) {
        return NULL;
    }

    const VSFrameRef *base = vsapi->getFrameFilter(n, hh->base, frame_ctx);
    const VSFrameRef *alt = vsapi->getFrameFilter(n, hh->alt, frame_ctx);

    const VSFormat *fi = vsapi->getFrameFormat(base);
    if (fi->sampleType != stInteger || fi != vsapi->getFrameFormat(alt) ||
        vsapi->getFrameWidth(base, 0) != vsapi->getFrameWidth(alt, 0) ||
        vsapi->getFrameHeight(base, 0) != vsapi->getFrameHeight(alt, 0)) {
        vsapi->freeFrame(alt);
        return base;
    }

    const int pl[] = {0, 1, 2};
    const VSFrameRef *fr[] = {hh->planes[0] ? NULL : base,
                              hh->planes[1] ? NULL : base,
                              hh->planes[2] ? NULL : base};
    VSFrameRef *dst = vsapi->newVideoFrame2(fi, vsapi->getFrameWidth(base, 0),
                                            vsapi->getFrameHeight(base, 0),
                                            fr, pl, base, core);

    int index = fi->bytesPerSample - 1;

    for (int p = 0; p < fi->numPlanes; p++) {
        if (fr[p]) {
            continue;
        }

        int width = vsapi->getFrameWidth(base, p);
        int height = vsapi->getFrameHeight(base, p);
        int stride = vsapi->getStride(base, p);
        const uint8_t *basep = vsapi->getReadPtr(base, p);
        const uint8_t *altp = vsapi->getReadPtr(alt, p);
        uint8_t *dstp = vsapi->getWritePtr(dst, p);

        if (width > 65535 || height > 65535) {
            vsapi->setFilterError("Hysteresis: plane width/height must be "
                                  "lesser than 65536", frame_ctx);
            return NULL;
        }
        footprint_t *fp = create_footprint(width, height, stride, index, altp,
                                           dstp);
        if (!fp) {
            vsapi->setFilterError("Hysteresis: failed to allocate footprint",
                                  frame_ctx);
            return NULL;
        }

        memset(dstp, 0, stride * height);

        write_hysteresis[index](fp, basep);

        free_footprint(fp);
    }

    vsapi->freeFrame(base);
    vsapi->freeFrame(alt);

    return dst;
}


static void VS_CC
initialize(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
           VSCore *core, const VSAPI *vsapi)
{
    hysteresis_t *hh = (hysteresis_t *)*instance_data;
    vsapi->setVideoInfo(hh->vi, 1, node);
    vsapi->clearMap(in);
}


static void VS_CC
close(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    hysteresis_t *hh = (hysteresis_t *)instance_data;
    if (!hh) {
        return;
    }
    if (hh->base) {
        vsapi->freeNode(hh->base);
        hh->base = NULL;
    }
    if (hh->alt) {
        vsapi->freeNode(hh->alt);
        hh->alt = NULL;
    }
    free(hh);
    hh = NULL;
}

const VSFilterGetFrame get_frame_hysteresis = get_frame;
const VSFilterInit filter_init_hysteresis = initialize;
const VSFilterFree filter_free_hysteresis = close;
