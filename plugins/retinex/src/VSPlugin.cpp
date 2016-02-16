/*
* Retinex filter - VapourSynth plugin
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


#include "MSRCP.h"
#include "MSRCR.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin)
{
    configFunc("AOJIAO_mawen1250->AOwen.Retina_Cortex->Retinex", "retinex",
        "Implementation of Retinex algorithm for VapourSynth.",
        VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("MSRCP", "input:clip;sigma:float[]:opt;lower_thr:float:opt;upper_thr:float:opt;fulls:int:opt;fulld:int:opt;chroma_protect:float:opt", MSRCPCreate, nullptr, plugin);
    registerFunc("MSRCR", "input:clip;sigma:float[]:opt;lower_thr:float:opt;upper_thr:float:opt;fulls:int:opt;fulld:int:opt;restore:float:opt", MSRCRCreate, nullptr, plugin);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
