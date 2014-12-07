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


#ifndef BILATERAL_H_
#define BILATERAL_H_


#include "Helper.h"
#include "Gaussian.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class BilateralData {
public:
    const VSAPI *vsapi = nullptr;
    VSNodeRef *node = nullptr;
    const VSVideoInfo *vi = nullptr;
    VSNodeRef *rnode = nullptr;
    const VSVideoInfo *rvi = nullptr;

    bool joint = false;
    double sigmaS[3];
    double sigmaR[3];
    int process[3];
    int algorithm[3];

    int PBFICnum[3];

    int radius[3];
    int samples[3];
    int step[3];

private:
    FLType * GS_LUT_[3];
    FLType * GR_LUT_[3];

public:
    BilateralData(const VSAPI *_vsapi = nullptr)
        : vsapi(_vsapi)
    {
        for (int i = 0; i < 3; i++)
        {
            GS_LUT_[i] = nullptr;
            GR_LUT_[i] = nullptr;
        }
    }

    ~BilateralData()
    {
        for (int i = 0; i < 3; i++)
        {
            if (process[i])
            {
                Gaussian_Function_Spatial_LUT_Free(GS_LUT_[i]);
                Gaussian_Function_Range_LUT_Free(GR_LUT_[i]);
            }
        }

        if (node) vsapi->freeNode(node);
        if (rnode) vsapi->freeNode(rnode);
    }

    bool isYUV() const
    {
        return vi->format->colorFamily == cmYUV || vi->format->colorFamily == cmYCoCg;
    }

    FLType * GS_LUT(int plane) { return GS_LUT_[plane]; }
    FLType * GR_LUT(int plane) { return GR_LUT_[plane]; }
    const FLType * GS_LUT(int plane) const { return GS_LUT_[plane]; }
    const FLType * GR_LUT(int plane) const { return GR_LUT_[plane]; }

    BilateralData & GS_LUT_Init()
    {
        for (int i = 0; i < 3; i++)
        {
            if (process[i] && algorithm[i] == 2)
            {
                GS_LUT_[i] = Gaussian_Function_Spatial_LUT_Generation(radius[i] + 1, radius[i] + 1, sigmaS[i]);
            }
        }

        return *this;
    }

    BilateralData & GR_LUT_Init()
    {
        for (int i = 0; i < 3; i++)
        {
            if (process[i])
            {
                GR_LUT_[i] = Gaussian_Function_Range_LUT_Generation((1 << vi->format->bitsPerSample) - 1, sigmaR[i]);
            }
        }

        return *this;
    }

    BilateralData & Bilateral2D_1_Paras()
    {
        for (int i = 0; i < 3; i++)
        {
            if (process[i] && PBFICnum[i] == 0)
            {
                if (sigmaR[i] >= 0.08)
                {
                    PBFICnum[i] = 4;
                }
                else if (sigmaR[i] >= 0.015)
                {
                    PBFICnum[i] = Min(16, static_cast<int>(4 * 0.08 / sigmaR[i] + 0.5));
                }
                else
                {
                    PBFICnum[i] = Min(32, static_cast<int>(16 * 0.015 / sigmaR[i] + 0.5));
                }

                if (i > 0 && isYUV() && PBFICnum[i] % 2 == 0 && PBFICnum[i] < 256) // Set odd PBFIC number to chroma planes by default
                    PBFICnum[i]++;
            }
        }

        return *this;
    }

    BilateralData & Bilateral2D_2_Paras()
    {
        int orad[3];

        for (int i = 0; i < 3; i++)
        {
            if (process[i])
            {
                orad[i] = Max(static_cast<int>(sigmaS[i] * sigmaSMul + 0.5), 1);

                step[i] = orad[i] < 4 ? 1 : orad[i] < 8 ? 2 : 3;

                samples[i] = 1;
                radius[i] = 1 + (samples[i] - 1)*step[i];

                while (orad[i] * 2 > radius[i] * 3)
                {
                    samples[i]++;
                    radius[i] = 1 + (samples[i] - 1)*step[i];
                    if (radius[i] >= orad[i] && samples[i] > 2)
                    {
                        samples[i]--;
                        radius[i] = 1 + (samples[i] - 1)*step[i];
                        break;
                    }
                }
            }
        }

        return *this;
    }

    BilateralData & algorithm_select()
    {
        for (int i = 0; i < 3; i++)
        {
            if (algorithm[i] <= 0)
                algorithm[i] = step[i] == 1 ? 2 : sigmaR[i] < 0.08 && samples[i] < 5 ? 2
                : 4 * samples[i] * samples[i] <= 15 * PBFICnum[i] ? 2 : 1;
        }

        return *this;
    }
};


void VS_CC BilateralCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template < typename T >
void Bilateral2D(VSFrameRef * dstf, const VSFrameRef * srcf, const VSFrameRef * reff, const BilateralData * d, const VSAPI * vsapi)
{
    const T *src, *ref;
    T *dst;
    int height, width, stride;

    const VSFormat *fi = vsapi->getFrameFormat(srcf);
    int bps = fi->bitsPerSample;

    for (int plane = 0; plane < fi->numPlanes; plane++)
    {
        if (d->process[plane])
        {
            src = reinterpret_cast<const T *>(vsapi->getReadPtr(srcf, plane));
            ref = reinterpret_cast<const T *>(vsapi->getReadPtr(reff, plane));
            dst = reinterpret_cast<T *>(vsapi->getWritePtr(dstf, plane));
            height = vsapi->getFrameHeight(srcf, plane);
            width = vsapi->getFrameWidth(srcf, plane);
            stride = vsapi->getStride(srcf, plane) / sizeof(T);

            switch (d->algorithm[plane])
            {
            case 1:
                Bilateral2D_1(dst, src, ref, d, plane, height, width, stride, bps);
                break;
            case 2:
                if (d->joint)
                    Bilateral2D_2(dst, src, ref, d, plane, height, width, stride, bps);
                else
                    Bilateral2D_2(dst, src, d, plane, height, width, stride, bps);
                break;
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Implementation of O(1) cross/joint Bilateral filter algorithm from "Qingxiong Yang, Kar-Han Tan, Narendra Ahuja - Real-Time O(1) Bilateral Filtering"
template < typename T >
void Bilateral2D_1(T * dst, const T * src, const T * ref, const BilateralData * d, int plane, int height, int width, int stride, int bps)
{
    int i, j, k, upper;
    int pcount = stride * height;
    int PBFICnum = d->PBFICnum[plane];

    int Floor = 0;
    int Ceil = (1 << bps) - 1;
    FLType FloorFL = static_cast<FLType>(Floor);
    FLType CeilFL = static_cast<FLType>(Ceil);

    const FLType * GR_LUT = d->GR_LUT(plane);

    // Value range of Plane "ref"
    T rLower, rUpper, rRange;

    rLower = Floor;
    rUpper = Ceil;
    rRange = rUpper - rLower;

    // Generate quantized PBFICs' parameters
    T * PBFICk = new T[PBFICnum];

    for (k = 0; k < PBFICnum; k++)
    {
        PBFICk[k] = static_cast<T>(static_cast<double>(rRange)*k / (PBFICnum - 1) + rLower + 0.5);
    }

    // Generate recursive Gaussian parameters
    FLType B, B1, B2, B3;
    Recursive_Gaussian_Parameters(d->sigmaS[plane], B, B1, B2, B3);

    // Generate quantized PBFICs
    FLType ** PBFIC = new FLType*[PBFICnum];
    FLType * Wk = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);
    FLType * Jk = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);

    for (k = 0; k < PBFICnum; k++)
    {
        PBFIC[k] = vs_aligned_malloc<FLType>(sizeof(FLType)*pcount, Alignment);

        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
            {
                Wk[i] = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, PBFICk[k], ref[i]);
                Jk[i] = Wk[i] * static_cast<FLType>(src[i]);
            }
        }

        Recursive_Gaussian2D_Horizontal(Wk, Wk, height, width, stride, B, B1, B2, B3);
        Recursive_Gaussian2D_Vertical(Wk, Wk, height, width, stride, B, B1, B2, B3);
        Recursive_Gaussian2D_Horizontal(Jk, Jk, height, width, stride, B, B1, B2, B3);
        Recursive_Gaussian2D_Vertical(Jk, Jk, height, width, stride, B, B1, B2, B3);

        for (j = 0; j < height; j++)
        {
            i = stride * j;
            for (upper = i + width; i < upper; i++)
            {
                PBFIC[k][i] = Wk[i] == 0 ? 0 : Jk[i] / Wk[i];
            }
        }
    }

    // Generate filtered result from PBFICs using linear interpolation
    for (j = 0; j < height; j++)
    {
        i = stride * j;
        for (upper = i + width; i < upper; i++)
        {
            for (k = 0; k < PBFICnum - 2; k++)
            {
                if (ref[i] < PBFICk[k + 1] && ref[i] >= PBFICk[k]) break;
            }

            dst[i] = static_cast<T>(Clip(((PBFICk[k + 1] - ref[i])*PBFIC[k][i] + (ref[i] - PBFICk[k])*PBFIC[k + 1][i]) / (PBFICk[k + 1] - PBFICk[k]) + FLType(0.5), FloorFL, CeilFL));
        }
    }

    // Clear
    for (k = 0; k < PBFICnum; k++)
        vs_aligned_free(PBFIC[k]);
    vs_aligned_free(Jk);
    vs_aligned_free(Wk);
    delete[] PBFIC;
    delete[] PBFICk;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Implementation of cross/joint Bilateral filter with truncated spatial window and sub-sampling
template < typename T >
void Bilateral2D_2(T * dst, const T * src, const T * ref, const BilateralData * d, int plane, int height, int width, int stride, int bps)
{
    int i, j, x, y;

    int radiusx = d->radius[plane];
    int radiusy = d->radius[plane];
    int buffnum = radiusy * 2 + 1;
    int samplestep = d->step[plane];
    int samplecenter = buffnum / 2;

    int bufheight = height + radiusy * 2;
    int bufwidth = width + radiusx * 2;
    int bufstride = stride_cal<T>(bufwidth);

    int Floor = 0;
    int Ceil = (1 << bps) - 1;
    FLType FloorFL = static_cast<FLType>(Floor);
    FLType CeilFL = static_cast<FLType>(Ceil);

    const T * srcp = src;
    const T * refp = ref;
    T * dstp = dst;

    const FLType * GS_LUT = d->GS_LUT(plane);
    const FLType * GR_LUT = d->GR_LUT(plane);

    // Allocate buffs
    T *srcbuff = newbuff(srcp, radiusx, radiusy, bufheight, bufwidth, bufstride, height, width, stride);
    T *refbuff = newbuff(refp, radiusx, radiusy, bufheight, bufwidth, bufstride, height, width, stride);
    T *srcbuffp1, *refbuffp1, *srcbuffp2, *refbuffp2;

    // Process
    FLType SWei, RWei1, RWei2, RWei3, RWei4, WeightSum, Sum;
    int xoffset, yoffset;
    const int xUpper = radiusx + 1, yUpper = radiusy + 1;

    yoffset = samplecenter;
    for (j = 0; j < height; j++, srcp += stride, refp += stride, dstp += stride)
    {
        srcbuffp1 = srcbuff + (yoffset + j) * bufstride;
        refbuffp1 = refbuff + (yoffset + j) * bufstride;

        for (i = 0; i < width; i++)
        {
            xoffset = samplecenter + i;
            srcbuffp2 = srcbuffp1 + xoffset;
            refbuffp2 = refbuffp1 + xoffset;

            WeightSum = GS_LUT[0] * GR_LUT[0];
            Sum = srcp[i] * WeightSum;

            for (y = 1; y < yUpper; y += samplestep)
            {
                for (x = 1; x < xUpper; x += samplestep)
                {
                    SWei = Gaussian_Distribution2D_Spatial_LUT_Lookup(GS_LUT, xUpper, x, y);
                    RWei1 = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, refp[i], refbuffp2[+y*bufstride + x]);
                    RWei2 = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, refp[i], refbuffp2[+y*bufstride - x]);
                    RWei3 = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, refp[i], refbuffp2[-y*bufstride - x]);
                    RWei4 = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, refp[i], refbuffp2[-y*bufstride + x]);

                    WeightSum += SWei * (RWei1 + RWei2 + RWei3 + RWei4);
                    Sum += SWei * (
                        srcbuffp2[+y*bufstride + x] * RWei1 +
                        srcbuffp2[+y*bufstride - x] * RWei2 +
                        srcbuffp2[-y*bufstride - x] * RWei3 +
                        srcbuffp2[-y*bufstride + x] * RWei4);
                }
            }

            dstp[i] = static_cast<T>(Clip(Sum / WeightSum + FLType(0.5), FloorFL, CeilFL));
        }
    }

    // Clear and output
    freebuff(srcbuff);
    freebuff(refbuff);
}

// Implementation of Bilateral filter with truncated spatial window and sub-sampling
template < typename T >
void Bilateral2D_2(T * dst, const T * src, const BilateralData * d, int plane, int height, int width, int stride, int bps)
{
    int i, j, x, y;

    int radiusx = d->radius[plane];
    int radiusy = d->radius[plane];
    int buffnum = radiusy * 2 + 1;
    int samplestep = d->step[plane];
    int samplecenter = buffnum / 2;

    int bufheight = height + radiusy * 2;
    int bufwidth = width + radiusx * 2;
    int bufstride = stride_cal<T>(bufwidth);

    int Floor = 0;
    int Ceil = (1 << bps) - 1;
    FLType FloorFL = static_cast<FLType>(Floor);
    FLType CeilFL = static_cast<FLType>(Ceil);

    const T * srcp = src;
    T * dstp = dst;

    const FLType * GS_LUT = d->GS_LUT(plane);
    const FLType * GR_LUT = d->GR_LUT(plane);

    // Allocate buffs
    T *srcbuff = newbuff(srcp, radiusx, radiusy, bufheight, bufwidth, bufstride, height, width, stride);
    T *srcbuffp1, *srcbuffp2;

    // Process
    FLType SWei, RWei1, RWei2, RWei3, RWei4, WeightSum, Sum;
    int xoffset, yoffset;
    const int xUpper = radiusx + 1, yUpper = radiusy + 1;

    yoffset = samplecenter;
    for (j = 0; j < height; j++, srcp += stride, dstp += stride)
    {
        srcbuffp1 = srcbuff + (yoffset + j) * bufstride;

        for (i = 0; i < width; i++)
        {
            xoffset = samplecenter + i;
            srcbuffp2 = srcbuffp1 + xoffset;

            WeightSum = GS_LUT[0] * GR_LUT[0];
            Sum = srcp[i] * WeightSum;

            for (y = 1; y < yUpper; y += samplestep)
            {
                for (x = 1; x < xUpper; x += samplestep)
                {
                    SWei = Gaussian_Distribution2D_Spatial_LUT_Lookup(GS_LUT, xUpper, x, y);
                    RWei1 = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, srcp[i], srcbuffp2[+y*bufstride + x]);
                    RWei2 = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, srcp[i], srcbuffp2[+y*bufstride - x]);
                    RWei3 = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, srcp[i], srcbuffp2[-y*bufstride - x]);
                    RWei4 = Gaussian_Distribution2D_Range_LUT_Lookup(GR_LUT, srcp[i], srcbuffp2[-y*bufstride + x]);

                    WeightSum += SWei * (RWei1 + RWei2 + RWei3 + RWei4);
                    Sum += SWei * (
                        srcbuffp2[+y*bufstride + x] * RWei1 +
                        srcbuffp2[+y*bufstride - x] * RWei2 +
                        srcbuffp2[-y*bufstride - x] * RWei3 +
                        srcbuffp2[-y*bufstride + x] * RWei4);
                }
            }

            dstp[i] = static_cast<T>(Clip(Sum / WeightSum + FLType(0.5), FloorFL, CeilFL));
        }
    }

    // Clear and output
    freebuff(srcbuff);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif