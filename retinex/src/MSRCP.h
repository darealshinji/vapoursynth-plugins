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


#ifndef MSRCP_H_
#define MSRCP_H_


#include "Helper.h"
#include "MSR.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const struct MSRCPPara
{
    double chroma_protect = 1.2;
} MSRCPDefault;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class MSRCPData
    : public MSRData
{
public:
    double chroma_protect = MSRCPDefault.chroma_protect;

public:
    MSRCPData(const VSAPI *_vsapi = nullptr, std::string _FunctionName = "MSRCP")
        : MSRData(_vsapi, _FunctionName) {}

    ~MSRCPData() {}

    virtual int arguments_process(const VSMap *in, VSMap *out)
    {
        MSRData::arguments_process(in, out);

        int error;

        chroma_protect = vsapi->propGetFloat(in, "chroma_protect", 0, &error);
        if (error)
            chroma_protect = MSRCPDefault.chroma_protect;
        if (chroma_protect < 1)
        {
            setError(out, "Invalid \"chroma_protect\" assigned, must be float number ranges in [1, +inf)");
            return 1;
        }

        return 0;
    }
};


void VS_CC MSRCPCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class MSRCPProcess
    : public MSRProcess
{
private:
    const MSRCPData &d;

private:
    template < typename T >
    void process_core();

protected:
    virtual void process_core8() { process_core<uint8_t>(); }
    virtual void process_core16() { process_core<uint16_t>(); }

public:
    MSRCPProcess(const MSRCPData &_d, int n, VSFrameContext *frameCtx, VSCore *core, const VSAPI *_vsapi)
        : MSRProcess(_d, n, frameCtx, core, _vsapi), d(_d) {}

    virtual ~MSRCPProcess() {}
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template < typename T >
void MSRCPProcess::process_core()
{
    int i, j, upper;
    FLType gain, offset, scale;
    const T *Ysrcp;
    T *Ydstp;

    // Calculate quantization parameters according to bit per sample and limited/full range
    // Floor and Ceil for limited range src will be determined later according to minimum and maximum value in the frame
    T sFloor = 0;
    //T sFloorC = 0;
    int sNeutral = 128 << (bps - 8);
    T sCeil = (1 << bps) - 1;
    //T sCeilC = (1 << bps) - 1;
    T sRange = d.fulls ? (1 << bps) - 1 : 219 << (bps - 8);
    T sRangeC = d.fulls ? (1 << bps) - 1 : 224 << (bps - 8);
    T dFloor = d.fulld ? 0 : 16 << (bps - 8);
    //T dFloorC = d.fulld ? 0 : 16 << (bps - 8);
    int dNeutral = 128 << (bps - 8);
    //T dCeil = d.fulld ? (1 << bps) - 1 : 235 << (bps - 8);
    //T dCeilC = d.fulld ? (1 << bps) - 1 : 240 << (bps - 8);
    T dRange = d.fulld ? (1 << bps) - 1 : 219 << (bps - 8);
    T dRangeC = d.fulld ? (1 << bps) - 1 : 224 << (bps - 8);
    FLType sFloorFL = static_cast<FLType>(sFloor);
    //FLType sFloorCFL = static_cast<FLType>(sFloorC);
    //FLType sNeutralFL = static_cast<FLType>(sNeutral);
    //FLType sCeilFL = static_cast<FLType>(sCeil);
    //FLType sCeilCFL = static_cast<FLType>(sCeilC);
    FLType sRangeFL = static_cast<FLType>(sRange);
    FLType sRangeCFL = static_cast<FLType>(sRangeC);
    FLType sRangeC2FL = static_cast<FLType>(sRangeC) / 2.;
    FLType dFloorFL = static_cast<FLType>(dFloor);
    //FLType dFloorCFL = static_cast<FLType>(dFloorC);
    FLType dNeutralFL = static_cast<FLType>(dNeutral);
    //FLType dCeilFL = static_cast<FLType>(dCeil);
    //FLType dCeilCFL = static_cast<FLType>(dCeilC);
    FLType dRangeFL = static_cast<FLType>(dRange);
    FLType dRangeCFL = static_cast<FLType>(dRangeC);

    // Allocate floating point data buff
    FLType *idata = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);
    FLType *odata = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);

    if (fi->colorFamily == cmGray) // Procedure for Gray color family
    {
        // Get read and write pointer for src and dst
        Ysrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 0));
        Ydstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 0));

        // Derive floating point intensity channel from integer Y channel
        if (d.fulls)
        {
            gain = 1 / sRangeFL;
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                    idata[i] = Ysrcp[i] * gain;
            }
        }
        else
        {
            // If src is of limited range, determine the Floor and Ceil by the minimum and maximum value in the frame
            T min, max;

            min = sCeil;
            max = sFloor;
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                {
                    min = Min(min, Ysrcp[i]);
                    max = Max(max, Ysrcp[i]);
                }
            }
            if (max > min)
            {
                sFloor = min;
                sCeil = max;
                sFloorFL = static_cast<FLType>(sFloor);
                //sCeilFL = static_cast<FLType>(sCeil);
            }

            gain = 1 / static_cast<FLType>(sCeil - sFloor);
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                    idata[i] = (Ysrcp[i] - sFloor) * gain;
            }
        }

        // Apply MSR to floating point intensity channel
        MSRKernel(odata, idata);
        // Simplest color balance with pixel clipping on either side of the dynamic range
        SimplestColorBalance(odata, idata);

        // Convert floating point intensity channel to integer Y channel
        offset = dFloorFL + FLType(0.5);
        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
                Ydstp[i] = static_cast<T>(odata[i] * dRangeFL + offset);
        }
    }
    else if (fi->colorFamily == cmRGB) // Procedure for RGB color family
    {
        // Get read and write pointer for src and dst
        const T *Rsrcp, *Gsrcp, *Bsrcp;
        T *Rdstp, *Gdstp, *Bdstp;

        Rsrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 0));
        Rdstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 0));
        Gsrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 1));
        Gdstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 1));
        Bsrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 2));
        Bdstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 2));

        // Derive floating point intensity channel from integer RGB channel
        if (d.fulls)
        {
            gain = 1 / (sRangeFL * 3);
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                    idata[i] = (Rsrcp[i] + Gsrcp[i] + Bsrcp[i]) * gain;
            }
        }
        else
        {
            // If src is of limited range, determine the Floor and Ceil by the minimum and maximum value in the frame
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

            offset = sFloorFL * -3;
            gain = 1 / (static_cast<FLType>(sCeil - sFloor) * 3);
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                    idata[i] = (Rsrcp[i] + Gsrcp[i] + Bsrcp[i] + offset) * gain;
            }
        }

        // Apply MSR to floating point intensity channel
        MSRKernel(odata, idata);
        // Simplest color balance with pixel clipping on either side of the dynamic range
        SimplestColorBalance(odata, idata);

        // Adjust integer RGB channel according to filtering result in floating point intensity channel
        T Rval, Gval, Bval;

        if (sFloor == 0 && dFloorFL == 0 && sRangeFL == dRangeFL)
        {
            offset = FLType(0.5);
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                {
                    Rval = Rsrcp[i];
                    Gval = Gsrcp[i];
                    Bval = Bsrcp[i];
                    gain = idata[i] <= 0 ? 1 : odata[i] / idata[i];
                    gain = Min(sRangeFL / Max(Rval, Max(Gval, Bval)), gain);
                    Rdstp[i] = static_cast<T>(Rval * gain + offset);
                    Gdstp[i] = static_cast<T>(Gval * gain + offset);
                    Bdstp[i] = static_cast<T>(Bval * gain + offset);
                }
            }
        }
        else
        {
            scale = dRangeFL / sRangeFL;
            offset = dFloorFL + FLType(0.5);
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                {
                    Rval = Rsrcp[i] - sFloor;
                    Gval = Gsrcp[i] - sFloor;
                    Bval = Bsrcp[i] - sFloor;
                    gain = idata[i] <= 0 ? 1 : odata[i] / idata[i];
                    gain = Min(sRangeFL / Max(Rval, Max(Gval, Bval)), gain) * scale;
                    Rdstp[i] = static_cast<T>(Rval * gain + offset);
                    Gdstp[i] = static_cast<T>(Gval * gain + offset);
                    Bdstp[i] = static_cast<T>(Bval * gain + offset);
                }
            }
        }
    }
    else // Procedure for YUV or YCoCg color family
    {
        // Get read and write pointer for src and dst
        const T *Usrcp, *Vsrcp;
        T *Udstp, *Vdstp;

        Ysrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 0));
        Ydstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 0));
        Usrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 1));
        Udstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 1));
        Vsrcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, 2));
        Vdstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 2));

        // Derive floating point intensity channel from integer Y channel
        if (d.fulls)
        {
            gain = 1 / sRangeFL;
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                    idata[i] = Ysrcp[i] * gain;
            }
        }
        else
        {
            // If src is of limited range, determine the Floor and Ceil by the minimum and maximum value in the frame
            T min, max;

            min = sCeil;
            max = sFloor;
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                {
                    min = Min(min, Ysrcp[i]);
                    max = Max(max, Ysrcp[i]);
                }
            }
            if (max > min)
            {
                sFloor = min;
                sCeil = max;
                sFloorFL = static_cast<FLType>(sFloor);
                //sCeilFL = static_cast<FLType>(sCeil);
            }

            gain = 1 / static_cast<FLType>(sCeil - sFloor);
            for (j = 0; j < height; j++)
            {
                i = stride * j;
                for (upper = i + width; i < upper; i++)
                    idata[i] = (Ysrcp[i] - sFloor) * gain;
            }
        }

        // Apply MSR to floating point intensity channel
        MSRKernel(odata, idata);
        // Simplest color balance with pixel clipping on either side of the dynamic range
        SimplestColorBalance(odata, idata);

        // Convert floating point intensity channel to integer Y channel
        // Adjust integer UV channel according to filtering result in floating point intensity channel
        // Chroma protect uses log function to attenuate the adjustment in UV channel
        FLType chroma_protect_mul1 = static_cast<FLType>(d.chroma_protect - 1);
        FLType chroma_protect_mul2 = static_cast<FLType>(1 / log(d.chroma_protect));

        int Uval, Vval;
        scale = dRangeCFL / sRangeCFL;
        if (d.fulld)
            offset = dNeutralFL + FLType(0.499999);
        else
            offset = dNeutralFL + FLType(0.5);
        FLType offsetY = dFloorFL + FLType(0.5);

        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
            {
                Uval = Usrcp[i] - sNeutral;
                Vval = Vsrcp[i] - sNeutral;
                if (d.chroma_protect > 1)
                    gain = idata[i] <= 0 ? 1 : log(odata[i] / idata[i] * chroma_protect_mul1 + 1) * chroma_protect_mul2;
                else
                    gain = idata[i] <= 0 ? 1 : odata[i] / idata[i];
                if (dRangeCFL == sRangeCFL)
                    gain = Min(sRangeC2FL / Max(Abs(Uval), Abs(Vval)), gain);
                else
                    gain = Min(sRangeC2FL / Max(Abs(Uval), Abs(Vval)), gain) * scale;
                Ydstp[i] = static_cast<T>(odata[i] * dRangeFL + offsetY);
                Udstp[i] = static_cast<T>(Uval * gain + offset);
                Vdstp[i] = static_cast<T>(Vval * gain + offset);
            }
        }
    }

    // Free floating point data buff
    vs_aligned_free(idata);
    vs_aligned_free(odata);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif
