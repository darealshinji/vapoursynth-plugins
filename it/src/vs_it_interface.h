/*
VS_IT Copyright(C) 2002 thejam79, 2003 minamina, 2014 msg7086

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
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <VapourSynth.h>
#include <VSHelper.h>

#ifdef _MSC_VER
#include <intrin.h>
#define alignas(x) __declspec(align(x))
#define ALIGNED_ARRAY(decl, alignment) alignas(alignment) decl
#else
#include <x86intrin.h>
#define __forceinline inline
#define ALIGNED_ARRAY(decl, alignment) __attribute__((aligned(alignment))) decl
#define _aligned_malloc(decl, alignment) aligned_alloc(alignment, decl)
#define _aligned_free(buffer) free(buffer)
#endif

#define PARAM_INT(name, def) int name = int64ToIntS(vsapi->propGetInt(in, #name, 0, &err)); if (err) { name = def; }

#define IT_VERSION "0103." "1.1"

#include "IScriptEnvironment.h"
#include "vs_it.h"
