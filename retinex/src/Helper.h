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


#ifndef HELPER_H_
#define HELPER_H_


#include <string>
#include <vector>
#include <cmath>
#include <cfloat>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


typedef double FLType;


const FLType FLType_MAX = sizeof(FLType) < 8 ? FLT_MAX : DBL_MAX;
const FLType FLType_MIN = sizeof(FLType) < 8 ? FLT_MIN : DBL_MIN;


template < typename T >
T Min(T a, T b)
{
    return b < a ? b : a;
}

template < typename T >
T Max(T a, T b)
{
    return b > a ? b : a;
}

template < typename T >
T Clip(T input, T Floor, T Ceil)
{
    return input <= Floor ? Floor : input >= Ceil ? Ceil : input;
}

template <typename T>
inline T Abs(T input)
{
    return input < 0 ? -input : input;
}

template <typename T>
inline T Round_Div(T dividend, T divisor)
{
    return (dividend + divisor / 2) / divisor;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const size_t Alignment = 32;


template < typename T >
int stride_cal(int width)
{
    size_t Alignment2 = Alignment / sizeof(T);
    return static_cast<int>(width % Alignment2 == 0 ? width : (width / Alignment2 + 1) * Alignment2);
}


template < typename T >
void data2buff(T * dst, const T * src, int xoffset, int yoffset,
    int bufheight, int bufwidth, int bufstride, int height, int width, int stride)
{
    int x, y;
    T *dstp;
    const T *srcp;

    for (y = 0; y < height; y++)
    {
        dstp = dst + (yoffset + y) * bufstride;
        srcp = src + y * stride;
        for (x = 0; x < xoffset; x++)
            dstp[x] = srcp[0];
        memcpy(dstp + xoffset, srcp, sizeof(T)*width);
        for (x = xoffset + width; x < bufwidth; x++)
            dstp[x] = srcp[width - 1];
    }

    srcp = dst + yoffset * bufstride;
    for (y = 0; y < yoffset; y++)
    {
        dstp = dst + y * bufstride;
        memcpy(dstp, srcp, sizeof(T)*bufwidth);
    }

    srcp = dst + (yoffset + height - 1) * bufstride;
    for (y = yoffset + height; y < bufheight; y++)
    {
        dstp = dst + y * bufstride;
        memcpy(dstp, srcp, sizeof(T)*bufwidth);
    }
}

template < typename T >
T * newbuff(const T * src, int xoffset, int yoffset,
    int bufheight, int bufwidth, int bufstride, int height, int width, int stride)
{
    T * dst = vs_aligned_malloc<T>(sizeof(T)*bufheight*bufstride, Alignment);
    data2buff(dst, src, xoffset, yoffset, bufheight, bufwidth, bufstride, height, width, stride);
    return dst;
}

template < typename T >
void freebuff(T * buff)
{
    vs_aligned_free(buff);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const int VSMaxPlaneCount = 3;


class VSData
{
protected:
    std::string NameSpace = "";
    std::string FunctionName = "";

public:
    const VSAPI *vsapi = nullptr;
    VSNodeRef *node = nullptr;
    const VSVideoInfo *vi = nullptr;

    int process[VSMaxPlaneCount];

protected:
    void setError(VSMap *out, const char *error_msg) const
    {
        std::string str = NameSpace + "." + FunctionName + ": " + error_msg;
        vsapi->setError(out, str.c_str());
    }

public:
    VSData(const VSAPI *_vsapi = nullptr, std::string _FunctionName = "", std::string _NameSpace = "")
        : NameSpace(_NameSpace), FunctionName(_FunctionName), vsapi(_vsapi)
    {
        for (int i = 0; i < VSMaxPlaneCount; i++)
            process[i] = 1;
    }

    virtual ~VSData()
    {
        if (node) vsapi->freeNode(node);
    }

    virtual int arguments_process(const VSMap *in, VSMap *out) = 0;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class VSProcess
{
private:
    const VSData &d;

protected:
    const VSAPI *vsapi = nullptr;

    const VSFrameRef *src = nullptr;
    const VSFormat *fi = nullptr;
    VSFrameRef *dst = nullptr;

    int PlaneCount;
    int Bps;
    int bps;

    int height;
    int width;
    int stride;
    int pcount;

    int src_height[VSMaxPlaneCount];
    int src_width[VSMaxPlaneCount];
    int src_stride[VSMaxPlaneCount];
    int src_pcount[VSMaxPlaneCount];

    int dst_height[VSMaxPlaneCount];
    int dst_width[VSMaxPlaneCount];
    int dst_stride[VSMaxPlaneCount];
    int dst_pcount[VSMaxPlaneCount];

private:
    template < typename T >
    void process_core();

protected:
    virtual void process_core8() = 0;
    virtual void process_core16() = 0;

public:
    VSProcess(const VSData &_d, int n, VSFrameContext *frameCtx, VSCore *core, const VSAPI *_vsapi)
        : d(_d), vsapi(_vsapi)
    {
        src = vsapi->getFrameFilter(n, d.node, frameCtx);
        fi = vsapi->getFrameFormat(src);

        PlaneCount = fi->numPlanes;
        Bps = fi->bytesPerSample;
        bps = fi->bitsPerSample;

        height = vsapi->getFrameHeight(src, 0);
        width = vsapi->getFrameWidth(src, 0);
        stride = vsapi->getStride(src, 0) / Bps;
        pcount = stride * height;

        int planes[VSMaxPlaneCount];
        const VSFrameRef *cp_planes[VSMaxPlaneCount];

        for (int i = 0; i < VSMaxPlaneCount; i++)
        {
            planes[i] = i;
            cp_planes[i] = d.process[i] ? nullptr : src;
        }

        dst = vsapi->newVideoFrame2(fi, width, height, cp_planes, planes, src, core);

        for (int i = 0; i < PlaneCount; i++)
        {
            src_height[i] = vsapi->getFrameHeight(src, i);
            src_width[i] = vsapi->getFrameWidth(src, i);
            src_stride[i] = vsapi->getStride(src, i) / Bps;
            src_pcount[i] = src_stride[i] * src_height[i];

            dst_height[i] = vsapi->getFrameHeight(dst, i);
            dst_width[i] = vsapi->getFrameWidth(dst, i);
            dst_stride[i] = vsapi->getStride(dst, i) / Bps;
            dst_pcount[i] = dst_stride[i] * dst_height[i];
        }
    }

    virtual ~VSProcess()
    {
        vsapi->freeFrame(src);
    }

    VSFrameRef * process()
    {
        int i;

        for (i = 0; i < PlaneCount; i++)
        {
            if (d.process[i]) break;
        }
        if (i >= PlaneCount) return dst;

        else if (Bps == 1)
        {
            process_core8();
        }
        else if (Bps == 2)
        {
            process_core16();
        }

        return dst;
    }
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif