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


#include "Gaussian.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static void VS_CC GaussianInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    GaussianData *d = reinterpret_cast<GaussianData *>(*instanceData);

    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC GaussianGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
    const GaussianData *d = reinterpret_cast<GaussianData *>(*instanceData);

    if (activationReason == arInitial)
    {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady)
    {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFormat *fi = vsapi->getFrameFormat(src);
        int width = vsapi->getFrameWidth(src, 0);
        int height = vsapi->getFrameHeight(src, 0);
        const int planes[] = { 0, 1, 2 };
        const VSFrameRef * cp_planes[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        VSFrameRef *dst = vsapi->newVideoFrame2(fi, width, height, cp_planes, planes, src, core);

        if (d->vi->format->bytesPerSample == 1)
        {
            Gaussian2D<uint8_t>(dst, src, d, vsapi);
        }
        else if (d->vi->format->bytesPerSample == 2)
        {
            Gaussian2D<uint16_t>(dst, src, d, vsapi);
        }

        vsapi->freeFrame(src);

        return dst;
    }

    return nullptr;
}

static void VS_CC GaussianFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    GaussianData *d = reinterpret_cast<GaussianData *>(instanceData);

    delete d;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void VS_CC GaussianCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
    GaussianData *data = new GaussianData(vsapi);
    GaussianData &d = *data;

    int i, m;

    d.node = vsapi->propGetNode(in, "input", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!d.vi->format)
    {
        delete data;
        vsapi->setError(out, "bilateral.Gaussian: Invalid input clip, Only constant format input supported");
        return;
    }
    if (d.vi->format->sampleType != stInteger || (d.vi->format->bytesPerSample != 1 && d.vi->format->bytesPerSample != 2))
    {
        delete data;
        vsapi->setError(out, "bilateral.Gaussian: Invalid input clip, Only 8-16 bit int formats supported");
        return;
    }

    bool chroma = d.vi->format->colorFamily == cmYUV || d.vi->format->colorFamily == cmYCoCg;

    m = vsapi->propNumElements(in, "sigma");
    for (i = 0; i < 3; i++)
    {
        if (i < m)
        {
            d.sigma[i] = vsapi->propGetFloat(in, "sigma", i, nullptr);
        }
        else if (i == 0)
        {
            d.sigma[0] = 3.0;
        }
        else if (i == 1 && chroma && d.vi->format->subSamplingW) // Reduce sigma for sub-sampled chroma planes by default
        {
            d.sigma[1] = d.sigma[0] / (1 << d.vi->format->subSamplingW);
        }
        else
        {
            d.sigma[i] = d.sigma[i - 1];
        }

        if (d.sigma[i] < 0)
        {
            delete data;
            vsapi->setError(out, "bilateral.Gaussian: Invalid \"sigma\" assigned, must be non-negative float number");
            return;
        }
    }

    int mV = vsapi->propNumElements(in, "sigmaV");
    for (i = 0; i < 3; i++)
    {
        if (i < mV)
        {
            d.sigmaV[i] = vsapi->propGetFloat(in, "sigmaV", i, nullptr);
        }
        else if (i < m)
        {
            d.sigmaV[i] = d.sigma[i];
        }
        else if (i == 0)
        {
            d.sigmaV[0] = d.sigma[0];
        }
        else if (i == 1 && chroma && d.vi->format->subSamplingH) // Reduce sigma for sub-sampled chroma planes by default
        {
            d.sigmaV[1] = d.sigmaV[0] / (1 << d.vi->format->subSamplingH);
        }
        else
        {
            d.sigmaV[i] = d.sigmaV[i - 1];
        }

        if (d.sigmaV[i] < 0)
        {
            delete data;
            vsapi->setError(out, "bilateral.Gaussian: Invalid \"sigmaV\" assigned, must be non-negative float number");
            return;
        }
    }

    for (i = 0; i < 3; i++)
    {
        if (d.sigma[i] == 0 && d.sigmaV[i] == 0)
            d.process[i] = 0;
        else
            d.process[i] = 1;
    }

    // Create filter
    vsapi->createFilter(in, out, "Gaussian", GaussianInit, GaussianGetFrame, GaussianFree, fmParallel, 0, data, core);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void Recursive_Gaussian_Parameters(const double sigma, FLType & B, FLType & B1, FLType & B2, FLType & B3)
{
    const double q = sigma < 2.5 ? 3.97156 - 4.14554*sqrt(1 - 0.26891*sigma) : 0.98711*sigma - 0.96330;

    const double b0 = 1.57825 + 2.44413*q + 1.4281*q*q + 0.422205*q*q*q;
    const double b1 = 2.44413*q + 2.85619*q*q + 1.26661*q*q*q;
    const double b2 = -(1.4281*q*q + 1.26661*q*q*q);
    const double b3 = 0.422205*q*q*q;

    B = static_cast<FLType>(1 - (b1 + b2 + b3) / b0);
    B1 = static_cast<FLType>(b1 / b0);
    B2 = static_cast<FLType>(b2 / b0);
    B3 = static_cast<FLType>(b3 / b0);
}

void Recursive_Gaussian2D_Vertical(FLType * output, const FLType * input, int height, int width, int stride, const FLType B, const FLType B1, const FLType B2, const FLType B3)
{
    int i0, i1, i2, i3, j, lower, upper;
    FLType P0, P1, P2, P3;

    if (output != input)
    {
        memcpy(output, input, sizeof(FLType) * width);
    }

    for (j = 0; j < height; j++)
    {
        lower = stride * j;
        upper = lower + width;

        i0 = lower;
        i1 = j < 1 ? i0 : i0 - stride;
        i2 = j < 2 ? i1 : i1 - stride;
        i3 = j < 3 ? i2 : i2 - stride;

        for (; i0 < upper; i0++, i1++, i2++, i3++)
        {
            P3 = output[i3];
            P2 = output[i2];
            P1 = output[i1];
            P0 = input[i0];
            output[i0] = B*P0 + B1*P1 + B2*P2 + B3*P3;
        }
    }

    for (j = height - 1; j >= 0; j--)
    {
        lower = stride * j;
        upper = lower + width;

        i0 = lower;
        i1 = j >= height - 1 ? i0 : i0 + stride;
        i2 = j >= height - 2 ? i1 : i1 + stride;
        i3 = j >= height - 3 ? i2 : i2 + stride;

        for (; i0 < upper; i0++, i1++, i2++, i3++)
        {
            P3 = output[i3];
            P2 = output[i2];
            P1 = output[i1];
            P0 = output[i0];
            output[i0] = B*P0 + B1*P1 + B2*P2 + B3*P3;
        }
    }
}

void Recursive_Gaussian2D_Horizontal(FLType * output, const FLType * input, int height, int width, int stride, const FLType B, const FLType B1, const FLType B2, const FLType B3)
{
    int i, j, lower, upper;
    FLType P0, P1, P2, P3;

    for (j = 0; j < height; j++)
    {
        lower = stride * j;
        upper = lower + width;

        i = lower;
        output[i] = P3 = P2 = P1 = input[i];

        for (i++; i < upper; i++)
        {
            P0 = B*input[i] + B1*P1 + B2*P2 + B3*P3;
            P3 = P2;
            P2 = P1;
            P1 = P0;
            output[i] = P0;
        }

        i--;
        P3 = P2 = P1 = output[i];

        for (i--; i >= lower; i--)
        {
            P0 = B*output[i] + B1*P1 + B2*P2 + B3*P3;
            P3 = P2;
            P2 = P1;
            P1 = P0;
            output[i] = P0;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
