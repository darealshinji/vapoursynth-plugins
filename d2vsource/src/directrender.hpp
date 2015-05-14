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

#ifndef DIRECTRENDER_H
#define DIRECTRENDER_H

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "libavversion.hpp"

#if defined(__OLD_AVCODEC_API)
#include "d2v.hpp"

typedef struct VSData {
    VSFrameRef *vs_frame;
    d2vData *d2v;
} VSData;

int VSGetBuffer(AVCodecContext *avctx, AVFrame *pic, int flag);
void VSReleaseBuffer(void *opaque, uint8_t *data);
#else
int VSGetBuffer(AVCodecContext *avctx, AVFrame *pic);
void VSReleaseBuffer(AVCodecContext *avctx, AVFrame *pic);
#endif

#endif
