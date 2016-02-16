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


#include "MSRCR.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void VS_CC MSRCRInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    MSRCRData *d = reinterpret_cast<MSRCRData *>(*instanceData);

    vsapi->setVideoInfo(d->vi, 1, node);
}

const VSFrameRef *VS_CC MSRCRGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
    const MSRCRData *d = reinterpret_cast<MSRCRData *>(*instanceData);

    if (activationReason == arInitial)
    {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady)
    {
        MSRCRProcess p(*d, n, frameCtx, core, vsapi);

        return p.process();
    }

    return nullptr;
}

void VS_CC MSRCRFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    MSRCRData *d = reinterpret_cast<MSRCRData *>(instanceData);

    delete d;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void VS_CC MSRCRCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
    MSRCRData *d = new MSRCRData(vsapi);

    if (d->arguments_process(in, out))
    {
        delete d;
        return;
    }

    // Create filter
    vsapi->createFilter(in, out, "MSRCR", MSRCRInit, MSRCRGetFrame, MSRCRFree, fmParallel, 0, d, core);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
