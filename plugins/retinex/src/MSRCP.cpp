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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void VS_CC MSRCPInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    MSRCPData *d = reinterpret_cast<MSRCPData *>(*instanceData);

    vsapi->setVideoInfo(d->vi, 1, node);
}

const VSFrameRef *VS_CC MSRCPGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
    const MSRCPData *d = reinterpret_cast<MSRCPData *>(*instanceData);

    if (activationReason == arInitial)
    {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady)
    {
        MSRCPProcess p(*d, n, frameCtx, core, vsapi);

        return p.process();
    }

    return nullptr;
}

void VS_CC MSRCPFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    MSRCPData *d = reinterpret_cast<MSRCPData *>(instanceData);

    delete d;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void VS_CC MSRCPCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
    MSRCPData *d = new MSRCPData(vsapi);

    if (d->arguments_process(in, out))
    {
        delete d;
        return;
    }

    // Create filter
    vsapi->createFilter(in, out, "MSRCP", MSRCPInit, MSRCPGetFrame, MSRCPFree, fmParallel, 0, d, core);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
