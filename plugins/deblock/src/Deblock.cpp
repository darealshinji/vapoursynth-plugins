// VapourSynth port by HolyWu
//
// DeBlock plugin for Avisynth 2.5 - takes a clip, and deblock it using H264 deblocking
// Copyright(c)2004 Manao as a function in MVTools v.0.9.6.2
// Copyright(c)2006 Alexander Balakhnin aka Fizick - separate plugin, YUY2 support
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

#include <VapourSynth.h>
#include <VSHelper.h>

static constexpr int QUANT_MAX = 60; // generalized by Fizick (was max=51)

static constexpr int alphas[] = {
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 4,
    5, 6, 7, 8, 9, 10,
    12, 13, 15, 17, 20,
    22, 25, 28, 32, 36,
    40, 45, 50, 56, 63,
    71, 80, 90, 101, 113,
    127, 144, 162, 182,
    203, 226, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255 // added by Fizick 
};

static constexpr int betas[] = {
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 2,
    2, 3, 3, 3, 3, 4,
    4, 4, 6, 6,
    7, 7, 8, 8, 9, 9,
    10, 10, 11, 11, 12,
    12, 13, 13, 14, 14,
    15, 15, 16, 16, 17,
    17, 18, 18,
    19, 20, 21, 22, 23, 24, 25, 26, 27 // added by Fizick 
};

static constexpr int cs[] = {
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    1, 2, 2, 2, 2, 3,
    3, 3, 4, 4, 5, 5,
    6, 7, 8, 8, 10,
    11, 12, 13, 15, 17,
    19, 21, 23, 25, 27, 29, 31, 33, 35 // added by Fizick for really strong deblocking :)
};

struct DeblockData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    bool process[3];
    int alpha, beta, c0, c1;
    float alphaF, betaF, c0F, c1F;
    int peak;
    float lower[3], upper[3];
};

template<typename T>
static void deblockHorEdge(T * VS_RESTRICT dstp, const unsigned stride, const unsigned plane, const DeblockData * d) {
    const int alpha = d->alpha;
    const int beta = d->beta;
    const int c0 = d->c0;
    const int c1 = d->c1;

    T * VS_RESTRICT sq0 = dstp;
    T * VS_RESTRICT sq1 = dstp + stride;
    const T * sq2 = dstp + stride * 2;
    T * VS_RESTRICT sp0 = dstp - stride;
    T * VS_RESTRICT sp1 = dstp - stride * 2;
    const T * sp2 = dstp - stride * 3;

    for (unsigned i = 0; i < 4; i++) {
        if (std::abs(sp0[i] - sq0[i]) < alpha && std::abs(sp1[i] - sp0[i]) < beta && std::abs(sq0[i] - sq1[i]) < beta) {
            const int ap = std::abs(sp2[i] - sp0[i]);
            const int aq = std::abs(sq2[i] - sq0[i]);

            int c = c0;
            if (aq < beta)
                c += c1;
            if (ap < beta)
                c += c1;

            const int avg = (sp0[i] + sq0[i] + 1) / 2;
            const int delta = std::min(std::max(((sq0[i] - sp0[i]) * 4 + sp1[i] - sq1[i] + 4) / 8, -c), c);
            const int deltap1 = std::min(std::max((sp2[i] + avg - sp1[i] * 2) / 2, -c0), c0);
            const int deltaq1 = std::min(std::max((sq2[i] + avg - sq1[i] * 2) / 2, -c0), c0);

            sp0[i] = std::min(std::max(sp0[i] + delta, 0), d->peak);
            sq0[i] = std::min(std::max(sq0[i] - delta, 0), d->peak);
            if (ap < beta)
                sp1[i] += deltap1;
            if (aq < beta)
                sq1[i] += deltaq1;
        }
    }
}

template<>
void deblockHorEdge(float * VS_RESTRICT dstp, const unsigned stride, const unsigned plane, const DeblockData * d) {
    const float alpha = d->alphaF;
    const float beta = d->betaF;
    const float c0 = d->c0F;
    const float c1 = d->c1F;

    float * VS_RESTRICT sq0 = dstp;
    float * VS_RESTRICT sq1 = dstp + stride;
    const float * sq2 = dstp + stride * 2;
    float * VS_RESTRICT sp0 = dstp - stride;
    float * VS_RESTRICT sp1 = dstp - stride * 2;
    const float * sp2 = dstp - stride * 3;

    for (unsigned i = 0; i < 4; i++) {
        if (std::abs(sp0[i] - sq0[i]) < alpha && std::abs(sp1[i] - sp0[i]) < beta && std::abs(sq0[i] - sq1[i]) < beta) {
            const float ap = std::abs(sp2[i] - sp0[i]);
            const float aq = std::abs(sq2[i] - sq0[i]);

            float c = c0;
            if (aq < beta)
                c += c1;
            if (ap < beta)
                c += c1;

            const float avg = (sp0[i] + sq0[i]) / 2.f;
            const float delta = std::min(std::max(((sq0[i] - sp0[i]) * 4.f + sp1[i] - sq1[i]) / 8.f, -c), c);
            const float deltap1 = std::min(std::max((sp2[i] + avg - sp1[i] * 2.f) / 2.f, -c0), c0);
            const float deltaq1 = std::min(std::max((sq2[i] + avg - sq1[i] * 2.f) / 2.f, -c0), c0);

            sp0[i] = std::min(std::max(sp0[i] + delta, d->lower[plane]), d->upper[plane]);
            sq0[i] = std::min(std::max(sq0[i] - delta, d->lower[plane]), d->upper[plane]);
            if (ap < beta)
                sp1[i] += deltap1;
            if (aq < beta)
                sq1[i] += deltaq1;
        }
    }
}

template<typename T>
static void deblockVerEdge(T * VS_RESTRICT dstp, const unsigned stride, const unsigned plane, const DeblockData * d) {
    const int alpha = d->alpha;
    const int beta = d->beta;
    const int c0 = d->c0;
    const int c1 = d->c1;

    for (unsigned i = 0; i < 4; i++) {
        if (std::abs(dstp[0] - dstp[-1]) < alpha && std::abs(dstp[1] - dstp[0]) < beta && std::abs(dstp[-1] - dstp[-2]) < beta) {
            const int ap = std::abs(dstp[2] - dstp[0]);
            const int aq = std::abs(dstp[-3] - dstp[-1]);

            int c = c0;
            if (aq < beta)
                c += c1;
            if (ap < beta)
                c += c1;

            const int avg = (dstp[0] + dstp[-1] + 1) / 2;
            const int delta = std::min(std::max(((dstp[0] - dstp[-1]) * 4 + dstp[-2] - dstp[1] + 4) / 8, -c), c);
            const int deltaq1 = std::min(std::max((dstp[2] + avg - dstp[1] * 2) / 2, -c0), c0);
            const int deltap1 = std::min(std::max((dstp[-3] + avg - dstp[-2] * 2) / 2, -c0), c0);

            dstp[0] = std::min(std::max(dstp[0] - delta, 0), d->peak);
            dstp[-1] = std::min(std::max(dstp[-1] + delta, 0), d->peak);
            if (ap < beta)
                dstp[1] += deltaq1;
            if (aq < beta)
                dstp[-2] += deltap1;
        }

        dstp += stride;
    }
}

template<>
void deblockVerEdge(float * VS_RESTRICT dstp, const unsigned stride, const unsigned plane, const DeblockData * d) {
    const float alpha = d->alphaF;
    const float beta = d->betaF;
    const float c0 = d->c0F;
    const float c1 = d->c1F;

    for (unsigned i = 0; i < 4; i++) {
        if (std::abs(dstp[0] - dstp[-1]) < alpha && std::abs(dstp[1] - dstp[0]) < beta && std::abs(dstp[-1] - dstp[-2]) < beta) {
            const float ap = std::abs(dstp[2] - dstp[0]);
            const float aq = std::abs(dstp[-3] - dstp[-1]);

            float c = c0;
            if (aq < beta)
                c += c1;
            if (ap < beta)
                c += c1;

            const float avg = (dstp[0] + dstp[-1]) / 2.f;
            const float delta = std::min(std::max(((dstp[0] - dstp[-1]) * 4.f + dstp[-2] - dstp[1]) / 8.f, -c), c);
            const float deltaq1 = std::min(std::max((dstp[2] + avg - dstp[1] * 2.f) / 2.f, -c0), c0);
            const float deltap1 = std::min(std::max((dstp[-3] + avg - dstp[-2] * 2.f) / 2.f, -c0), c0);

            dstp[0] = std::min(std::max(dstp[0] - delta, d->lower[plane]), d->upper[plane]);
            dstp[-1] = std::min(std::max(dstp[-1] + delta, d->lower[plane]), d->upper[plane]);
            if (ap < beta)
                dstp[1] += deltaq1;
            if (aq < beta)
                dstp[-2] += deltap1;
        }

        dstp += stride;
    }
}

template<typename T>
static void process(VSFrameRef * dst, const DeblockData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const unsigned width = vsapi->getFrameWidth(dst, plane);
            const unsigned height = vsapi->getFrameHeight(dst, plane);
            const unsigned stride = vsapi->getStride(dst, plane) / sizeof(T);
            T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

            for (unsigned x = 4; x < width; x += 4)
                deblockVerEdge(dstp + x, stride, plane, d);

            dstp += stride * 4;

            for (unsigned y = 4; y < height; y += 4) {
                deblockHorEdge(dstp, stride, plane, d);

                for (unsigned x = 4; x < width; x += 4) {
                    deblockHorEdge(dstp + x, stride, plane, d);
                    deblockVerEdge(dstp + x, stride, plane, d);
                }

                dstp += stride * 4;
            }
        }
    }
}

static void VS_CC deblockInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DeblockData * d = static_cast<DeblockData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC deblockGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const DeblockData * d = static_cast<const DeblockData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef * dst = vsapi->copyFrame(src, core);

        if (d->vi->format->bytesPerSample == 1)
            process<uint8_t>(dst, d, vsapi);
        else if (d->vi->format->bytesPerSample == 2)
            process<uint16_t>(dst, d, vsapi);
        else
            process<float>(dst, d, vsapi);

        vsapi->freeFrame(src);
        return dst;
    }

    return nullptr;
}

static void VS_CC deblockFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DeblockData * d = static_cast<DeblockData *>(instanceData);
    vsapi->freeNode(d->node);
    delete d;
}

static void VS_CC deblockCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    DeblockData d{};
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    const int padWidth = (d.vi->width & 7) ? 8 - d.vi->width % 8 : 0;
    const int padHeight = (d.vi->height & 7) ? 8 - d.vi->height % 8 : 0;

    try {
        if (!isConstantFormat(d.vi) || (d.vi->format->sampleType == stInteger && d.vi->format->bitsPerSample > 16) ||
            (d.vi->format->sampleType == stFloat && d.vi->format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bits integer and 32 bits float input supported" };

        int quant = int64ToIntS(vsapi->propGetInt(in, "quant", 0, &err));
        if (err)
            quant = 25;

        int aOffset = int64ToIntS(vsapi->propGetInt(in, "aoffset", 0, &err));

        int bOffset = int64ToIntS(vsapi->propGetInt(in, "boffset", 0, &err));

        if (quant < 0 || quant > QUANT_MAX)
            throw std::string{ "quant must be between 0 and " + std::to_string(QUANT_MAX) + " (inclusive)" };

        const int m = vsapi->propNumElements(in, "planes");

        for (int i = 0; i < 3; i++)
            d.process[i] = m <= 0;

        for (int i = 0; i < m; i++) {
            const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

            if (n < 0 || n >= d.vi->format->numPlanes)
                throw std::string{ "plane index out of range" };

            if (d.process[n])
                throw std::string{ "plane specified twice" };

            d.process[n] = true;
        }

        aOffset = std::min(std::max(aOffset, -quant), QUANT_MAX - quant);
        bOffset = std::min(std::max(bOffset, -quant), QUANT_MAX - quant);
        const int aIndex = quant + aOffset;
        const int bIndex = quant + bOffset;
        d.alpha = alphas[aIndex];
        d.beta = betas[bIndex];
        d.c0 = cs[aIndex];

        if (d.vi->format->sampleType == stInteger) {
            d.peak = (1 << d.vi->format->bitsPerSample) - 1;
            d.alpha = d.alpha * d.peak / 255;
            d.beta = d.beta * d.peak / 255;
            d.c0 = d.c0 * d.peak / 255;
            d.c1 = 1 * d.peak / 255;
        } else {
            d.alphaF = d.alpha / 255.f;
            d.betaF = d.beta / 255.f;
            d.c0F = d.c0 / 255.f;
            d.c1F = 1.f / 255.f;

            for (int plane = 0; plane < d.vi->format->numPlanes; plane++) {
                if (plane == 0 || d.vi->format->colorFamily == cmRGB) {
                    d.lower[plane] = 0.f;
                    d.upper[plane] = 1.f;
                } else {
                    d.lower[plane] = -0.5f;
                    d.upper[plane] = 0.5f;
                }
            }
        }

        if (padWidth || padHeight) {
            VSMap * args = vsapi->createMap();
            vsapi->propSetNode(args, "clip", d.node, paReplace);
            vsapi->freeNode(d.node);
            vsapi->propSetInt(args, "width", d.vi->width + padWidth, paReplace);
            vsapi->propSetInt(args, "height", d.vi->height + padHeight, paReplace);
            vsapi->propSetFloat(args, "src_width", d.vi->width + padWidth, paReplace);
            vsapi->propSetFloat(args, "src_height", d.vi->height + padHeight, paReplace);

            VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.resize", core), "Point", args);
            if (vsapi->getError(ret)) {
                vsapi->setError(out, vsapi->getError(ret));
                vsapi->freeMap(args);
                vsapi->freeMap(ret);
                return;
            }

            d.node = vsapi->propGetNode(ret, "clip", 0, nullptr);
            d.vi = vsapi->getVideoInfo(d.node);
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
        }
    } catch (const std::string & error) {
        vsapi->setError(out, ("Deblock: " + error).c_str());
        vsapi->freeNode(d.node);
        return;
    }

    DeblockData * data = new DeblockData{ std::move(d) };

    vsapi->createFilter(in, out, "Deblock", deblockInit, deblockGetFrame, deblockFree, fmParallel, 0, data, core);

    if (padWidth || padHeight) {
        VSNodeRef * node = vsapi->propGetNode(out, "clip", 0, nullptr);
        vsapi->clearMap(out);

        VSMap * args = vsapi->createMap();
        vsapi->propSetNode(args, "clip", node, paReplace);
        vsapi->freeNode(node);
        vsapi->propSetInt(args, "right", padWidth, paReplace);
        vsapi->propSetInt(args, "bottom", padHeight, paReplace);

        VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.std", core), "CropRel", args);
        if (vsapi->getError(ret)) {
            vsapi->setError(out, vsapi->getError(ret));
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
            return;
        }

        node = vsapi->propGetNode(ret, "clip", 0, nullptr);
        vsapi->freeMap(args);
        vsapi->freeMap(ret);
        vsapi->propSetNode(out, "clip", node, paReplace);
        vsapi->freeNode(node);
    }
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.deblock", "deblock", "It does a deblocking of the picture, using the deblocking filter of h264", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Deblock",
                 "clip:clip;"
                 "quant:int:opt;"
                 "aoffset:int:opt;"
                 "boffset:int:opt;"
                 "planes:int[]:opt;",
                 deblockCreate, nullptr, plugin);
}
