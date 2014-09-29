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

extern "C" {
#include <stdint.h>
#include <stdlib.h>
}

#include <VapourSynth.h>
#include <VSHelper.h>

#include "applyrff.hpp"
#include "d2vsource.hpp"

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin)
{
    configFunc("com.sources.d2vsource", "d2v", "D2V Source", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Source", "input:data;threads:int:opt;nocrop:int:opt;rff:int:opt;", d2vCreate, 0, plugin);
    registerFunc("ApplyRFF", "clip:clip;d2v:data;", rffCreate, 0, plugin);
}
