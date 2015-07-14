/*
* Waifu2x-opt image restoration filter - VapourSynth plugin
* Copyright (C) 2015  mawen1250
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


#include "Waifu2x_Denoise.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int Waifu2x_Denoise_Data::arguments_process(const VSMap *in, VSMap *out)
{
    if (_Mybase::arguments_process(in, out))
    {
        return 1;
    }

    int error;

    // mode - int
    para.mode = int64ToIntS(vsapi->propGetInt(in, "mode", 0, &error));

    if (error)
    {
        para.mode = 1;
    }
    else if (para.mode != 1 && para.mode != 2)
    {
        setError(out, "\'mode\' must be either 1 or 2");
        return 1;
    }

    // process
    for (int i = 0; i < VSMaxPlaneCount; i++)
    {
        if (vi->format->colorFamily != cmRGB && i > 0)
        {
            process[i] = 0;
        }
    }

    chroma = false;

    return 0;
}


void Waifu2x_Denoise_Data::init(VSCore *core)
{
    // Initialize waifu2x
    init_waifu2x(waifu2x, waifu2x_mutex, para.mode, USE_INTERNAL_MT ? para.threads : 1,
        vi->width, vi->height, para.block_width, para.block_height, core, vsapi);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void Waifu2x_Denoise_Process::Kernel(FLType *dst, const FLType *src) const
{
    waifu2x->process(dst, src, dst_width[0], dst_height[0], dst_stride[0], false);
}


void Waifu2x_Denoise_Process::Kernel(FLType *dstY, FLType *dstU, FLType *dstV,
    const FLType *srcY, const FLType *srcU, const FLType *srcV) const
{
    Kernel(dstY, srcY);
    MatCopy(dstU, srcU, dst_height[1], dst_width[1], dst_stride[1], src_stride[1]);
    MatCopy(dstV, srcV, dst_height[2], dst_width[2], dst_stride[2], src_stride[2]);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
