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

#ifndef D2V_H
#define D2V_H

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "gop.hpp"

#define D2V_VERSION "16"

#include "deprecated_enums.h"

using namespace std;

enum streamtype {
    UNSET      = -1,
    ELEMENTARY = 0,
    PROGRAM    = 1,
    TRANSPORT  = 2,
    PVA        = 3
};

enum scaletype {
    TV = 0,
    PC = 1
};

static const enum scaletype scaletype_conv[2] = {
    TV,
    PC
};

static const enum streamtype streamtype_conv[4] = {
    ELEMENTARY,
    PROGRAM,
    TRANSPORT,
    PVA
};

static const int idct_algo_conv[8] = {
    FF_IDCT_AUTO,
    FF_IDCT_AUTO,
    FF_IDCT_AUTO,
    FF_IDCT_AUTO,
    FF_IDCT_AUTO,
    FF_IDCT_AUTO,
    FF_IDCT_XVIDMMX,
    FF_IDCT_SIMPLEMMX
};

typedef struct location {
    int startfile;
    int startoffset;
    int endfile;
    int endoffset;
} location;

typedef struct d2vcontext {
    int num_files;
    string *files;

    enum streamtype stream_type;
    int ts_pid;
    int mpeg_type;
    int idct_algo;
    enum scaletype yuvrgb_scale;
    int width;
    int height;
    int fps_num;
    int fps_den;
    location loc;

    vector<frame> frames;
    vector<gop> gops;
} d2vcontext;

void d2vfreep(d2vcontext **ctx);
d2vcontext *d2vparse(const char *filename, string& err);
string d2vgetpath(const char *d2v_path, const string& file);

#endif
