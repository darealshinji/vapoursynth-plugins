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


#ifndef MSRCR_H_
#define MSRCR_H_


#include "Helper.h"
#include "MSR.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const struct MSRCRPara
{
    double restore = 125.0;
} MSRCRDefault;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class MSRCRData
    : public MSRData
{
public:
    double restore = MSRCRDefault.restore;

public:
    MSRCRData(const VSAPI *_vsapi = nullptr, std::string _FunctionName = "MSRCR")
        : MSRData(_vsapi, _FunctionName) {}

    ~MSRCRData() {}

    virtual int arguments_process(const VSMap *in, VSMap *out)
    {
        MSRData::arguments_process(in, out);

        int error;

        if (vi->format->colorFamily != cmRGB)
        {
            setError(out, "Invalid input clip, only RGB format input supported");
            return 1;
        }

        restore = vsapi->propGetFloat(in, "restore", 0, &error);
        if (error)
            restore = MSRCRDefault.restore;
        if (restore < 0)
        {
            setError(out, "Invalid \"restore\" assigned, must be non-negative float number");
            return 1;
        }

        return 0;
    }
};


void VS_CC MSRCRCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class MSRCRProcess
    : public MSRProcess
{
private:
    const MSRCRData &d;

private:
    template < typename T >
    void process_core();

protected:
    virtual void process_core8() { process_core<uint8_t>(); }
    virtual void process_core16() { process_core<uint16_t>(); }

public:
    MSRCRProcess(const MSRCRData &_d, int n, VSFrameContext *frameCtx, VSCore *core, const VSAPI *_vsapi)
        : MSRProcess(_d, n, frameCtx, core, _vsapi), d(_d) {}

    virtual ~MSRCRProcess() {}
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template < typename T >
void MSRCRProcess::process_core()
{
    int i, j, upper;
    FLType gain, offset;

    // Calculate quantization parameters according to bit per sample and limited/full range
    // Floor and Ceil for limited range src will be determined later according to minimum and maximum value in the frame
    T sFloor = 0;
    T sCeil = (1 << bps) - 1;
    T sRange = d.fulls ? (1 << bps) - 1 : 219 << (bps - 8);
    T dFloor = d.fulld ? 0 : 16 << (bps - 8);
    T dCeil = d.fulld ? (1 << bps) - 1 : 235 << (bps - 8);
    //T dRange = d.fulld ? (1 << bps) - 1 : 219 << (bps - 8);
    FLType sFloorFL = static_cast<FLType>(sFloor);
    //FLType sCeilFL = static_cast<FLType>(sCeil);
    FLType sRangeFL = static_cast<FLType>(sRange);
    //FLType dFloorFL = static_cast<FLType>(dFloor);
    //FLType dCeilFL = static_cast<FLType>(dCeil);
    //FLType dRangeFL = static_cast<FLType>(dRange);

    // Allocate floating point data buff
    FLType *idata = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);
    FLType *odataR = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);
    FLType *odataG = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);
    FLType *odataB = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);

    // Get read and write pointer for src and dst
    const T *Rsrcp, *Gsrcp, *Bsrcp;
    T *Rdstp, *Gdstp, *Bdstp;

    Rsrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 0));
    Rdstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 0));
    Gsrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 1));
    Gdstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 1));
    Bsrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 2));
    Bdstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 2));

    // If src is not of full range, determine the Floor and Ceil by the maximum and minimum value in the frame
    if (!d.fulls)
    {
        T min, max;

        min = sCeil;
        max = sFloor;
        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
            {
                min = Min(min, Min(Rsrcp[i], Min(Gsrcp[i], Bsrcp[i])));
                max = Max(max, Max(Rsrcp[i], Max(Gsrcp[i], Bsrcp[i])));
            }
        }
        if (max > min)
        {
            sFloor = min;
            sCeil = max;
            sFloorFL = static_cast<FLType>(sFloor);
            //sCeilFL = static_cast<FLType>(sCeil);
        }
    }

    // Derive floating point R channel from integer R channel
    if (d.fulls)
    {
        gain = 1 / sRangeFL;
        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
                idata[i] = Rsrcp[i] * gain;
        }
    }
    else
    {
        offset = -sFloorFL;
        gain = 1 / static_cast<FLType>(sCeil - sFloor);
        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
                idata[i] = (Rsrcp[i] + offset) * gain;
        }
    }

    // Apply MSR to floating point R channel
    MSRKernel(odataR, idata);

    // Derive floating point G channel from integer G channel
    if (d.fulls)
    {
        gain = 1 / sRangeFL;
        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
                idata[i] = Gsrcp[i] * gain;
        }
    }
    else
    {
        offset = -sFloorFL;
        gain = 1 / static_cast<FLType>(sCeil - sFloor);
        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
                idata[i] = (Gsrcp[i] + offset) * gain;
        }
    }

    // Apply MSR to floating point G channel
    MSRKernel(odataG, idata);

    // Derive floating point B channel from integer B channel
    if (d.fulls)
    {
        gain = 1 / sRangeFL;
        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
                idata[i] = Bsrcp[i] * gain;
        }
    }
    else
    {
        offset = -sFloorFL;
        gain = 1 / static_cast<FLType>(sCeil - sFloor);
        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
                idata[i] = (Bsrcp[i] + offset) * gain;
        }
    }

    // Apply MSR to floating point B channel
    MSRKernel(odataB, idata);

    // Color restoration
    FLType RvalFL, GvalFL, BvalFL;
    FLType temp;

    for (j = 0; j < height; j++)
    {
        i = stride * j;
        for (upper = i + width; i < upper; i++)
        {
            RvalFL = Rsrcp[i] - sFloor;
            GvalFL = Gsrcp[i] - sFloor;
            BvalFL = Bsrcp[i] - sFloor;
            temp = RvalFL + GvalFL + BvalFL;
            temp = temp <= 0 ? 0 : d.restore / temp;
            odataR[i] *= log(RvalFL * temp + 1);
            odataG[i] *= log(GvalFL * temp + 1);
            odataB[i] *= log(BvalFL * temp + 1);
        }
    }

    // Simplest color balance with pixel clipping on either side of the dynamic range
    SimplestColorBalance(Rdstp, odataR, Rsrcp, dFloor, dCeil);
    SimplestColorBalance(Gdstp, odataG, Gsrcp, dFloor, dCeil);
    SimplestColorBalance(Bdstp, odataB, Bsrcp, dFloor, dCeil);

    // Free floating point data buff
    vs_aligned_free(idata);
    vs_aligned_free(odataR);
    vs_aligned_free(odataG);
    vs_aligned_free(odataB);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif