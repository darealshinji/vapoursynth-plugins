/*
* Bilateral filter - VapourSynth plugin
* Copyright (C) 2014  mawen1250
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "Bilateral.h"
#include "Gaussian.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin)
{
    configFunc("mawen1250.Bilateral", "bilateral",
        "Bilateral filter and Gaussian filter for VapourSynth.",
        VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("Bilateral", "input:clip;ref:clip:opt;sigmaS:float[]:opt;sigmaR:float[]:opt;planes:int[]:opt;algorithm:int[]:opt;PBFICnum:int[]:opt", BilateralCreate, nullptr, plugin);
    registerFunc("Gaussian", "input:clip;sigma:float[]:opt;sigmaV:float[]:opt", GaussianCreate, nullptr, plugin);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
