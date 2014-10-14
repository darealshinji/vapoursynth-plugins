/*
VS_DELOGO Copyright(C) 2003 MakKi, 2014 msg7086

This program is free software; you can redistribute it and / or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
*/

#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <VapourSynth.h>
#include <VSHelper.h>
#define BYTE unsigned char

#ifdef _MSC_VER
#define snprintf(a,b,c) _snprintf_s(a,b,b,c)
#define stricmp _stricmp
#define TS_ALIGN __declspec(align(16))
#define TS_FUNC_ALIGN
#else
#define TS_ALIGN __attribute__((aligned(16)))
#define TS_FUNC_ALIGN __attribute__((force_align_arg_pointer))
#define fopen_s(a,b,c) *a = fopen(b,c)
#endif

#define PARAM_INT(name, def) int name = int64ToIntS(vsapi->propGetInt(in, #name, 0, &err)); if (err) { name = def; }
#define PARAM_STR(name, def) const char* name = vsapi->propGetData(in, #name, 0, &err); if (err) { name = def; }

#define PLUGIN_VERSION "005a." "0.11"

#define LOGO_FADE_MAX 256
#define LOGO_DEFAULT_DEPTH 128

#define FAIL_IF_ERROR(cond, ...) {\
    if (cond) {\
        snprintf(msg, 200, __VA_ARGS__);\
        goto fail;\
    }\
}

#include "IScriptEnvironment.h"
#include "delogo.hpp"
#include "logo.h"
