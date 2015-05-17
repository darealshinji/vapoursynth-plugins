/*
  PluginInit.cpp: Copyright (C) 2013  Acfun (www dot acfun dot tv)
  
  Author: Li Yonggang (gnaggnoyil at gmail dot com)
  
  This file is part of the VAutoDeint project.
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 3 of
  the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with the author; if not, write to the Free Software Foundation,
  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "VapourSynth.h"
#include "VSHelper.h"

void VS_CC IsCombedTIVTCCreate(const VSMap *in,VSMap *out,void *userData,VSCore *core,const VSAPI *vsapi);
void VS_CC PlaneDifferenceFromPreviousCreate(const VSMap *in,VSMap *out,void *userData,VSCore *core,const VSAPI *vsapi);

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc,VSRegisterFunction registerFunc,VSPlugin *plugin){
  configFunc("http://8.8.8.8","deintconf","auto deinterlace external filters",VAPOURSYNTH_API_VERSION,1,plugin);
  registerFunc("IsCombedTIVTC","clip:clip;cthresh:int:opt;MI:int:opt;chroma:int:opt;blockx:int:opt;blocky:int:opt;metric:int:opt;opt:int:opt;",IsCombedTIVTCCreate,0,plugin);
  registerFunc("PlaneDifferenceFromPrevious","clip:clip;plane:int;prop:data:opt;",PlaneDifferenceFromPreviousCreate,0,plugin);
}