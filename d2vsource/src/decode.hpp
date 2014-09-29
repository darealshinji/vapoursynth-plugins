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

#ifndef DECODE_H
#define DECODE_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include <stdint.h>
}

typedef struct decodecontext {
    vector<FILE *> files;
    vector<int64_t> file_sizes;

    AVCodecContext *avctx;
    AVFormatContext *fctx;
    AVCodec *incodec;
    string *fakename;

    AVPacket inpkt;

    int stream_index;

    int last_frame;
    int last_gop;

    uint8_t *in;

    unsigned int orig_file;
    unsigned int cur_file;
    uint64_t orig_file_offset;
} decodecontext;

decodecontext *decodeinit(d2vcontext *dctx, int threads, string& err);
void decodefreep(decodecontext **ctx);
int decodeframe(int frame, d2vcontext *ctx, decodecontext *dctx, AVFrame *out, string& err);

#endif
