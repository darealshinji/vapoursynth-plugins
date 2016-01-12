/*
VS_DELOGO Copyright(C) 2003 MakKi, 2014-2015 msg7086

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

#ifndef __DELOGO_INTERFACE_H
#define __DELOGO_INTERFACE_H

#include "delogo.hpp"

#ifdef _MSC_VER
#define snprintf(a, b, c, ...) _snprintf_s(a, b, b, c, __VA_ARGS__)
#define stricmp _stricmp
#define TS_ALIGN __declspec(align(16))
#define TS_FUNC_ALIGN
#else
#define TS_ALIGN __attribute__((aligned(16)))
#define TS_FUNC_ALIGN __attribute__((force_align_arg_pointer))
#endif

#define PARAM_INT(name, def)                                       \
    int name = int64ToIntS(vsapi->propGetInt(in, #name, 0, &err)); \
    if (err) {                                                     \
        name = def;                                                \
    }

#define PARAM_STR(name, def)                                   \
    const char* name = vsapi->propGetData(in, #name, 0, &err); \
    if (err) {                                                 \
        name = def;                                            \
    }

#define FAIL_IF_ERROR(cond, ...)                   \
    {                                              \
        if (cond) {                                \
            snprintf(msg, 200, "%s", __VA_ARGS__); \
            vsapi->setError(out, msg_buff);        \
            return nullptr;                        \
        }                                          \
    }

#endif
