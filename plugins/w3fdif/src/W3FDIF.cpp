/*
 * Copyright (C) 2012 British Broadcasting Corporation, All Rights Reserved
 * Author of de-interlace algorithm: Jim Easterbrook for BBC R&D
 * Based on the process described by Martin Weston for BBC R&D
 * Author of FFmpeg filter: Mark Himsley for BBC Broadcast Systems Development
 * VapourSynth port: HolyWu
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <algorithm>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>

struct W3FDIFData {
    VSNodeRef * node;
    VSVideoInfo vi;
    const VSVideoInfo * viSaved;
    int order, mode;
    int peak;
    float lower[3], upper[3];
};

/*
 * Filter coefficients from PH-2071.
 * Each set of coefficients has a set for low-frequencies and high-frequencies.
 * n_coef_lf[] and n_coef_hf[] are the number of coefs for simple and more-complex.
 * It is important for later that n_coef_lf[] is even and n_coef_hf[] is odd.
 * coef_lf[][] and coef_hf[][] are the coefficients for low-frequencies
 * and high-frequencies for simple and more-complex mode.
 */
static const int n_coef_lf[2] = { 2, 4 };
static const int n_coef_hf[2] = { 3, 5 };
static const float coef_lf[2][4] = { { 0.5f, 0.5f, 0.f, 0.f }, { -0.026f, 0.526f, 0.526f, -0.026f } };
static const float coef_hf[2][5] = { { -0.0625f, 0.125f, -0.0625f, 0.f, 0.f }, { 0.031f, -0.116f, 0.17f, -0.116f, 0.031f } };

template<typename T>
static void filterSimpleLow(const T ** srcp, float * VS_RESTRICT buffer, const float * coef, const int width) {
    for (int i = 0; i < 2; i++) {
        for (int x = 0; x < width; x++)
            buffer[x] += srcp[i][x] * coef[i];
    }
}

template<typename T>
static void filterComplexLow(const T ** srcp, float * VS_RESTRICT buffer, const float * coef, const int width) {
    for (int i = 0; i < 4; i++) {
        for (int x = 0; x < width; x++)
            buffer[x] += srcp[i][x] * coef[i];
    }
}

template<typename T>
static void filterSimpleHigh(const T ** srcp, const T ** adjp, float * VS_RESTRICT buffer, const float * coef, const int width) {
    for (int i = 0; i < 3; i++) {
        for (int x = 0; x < width; x++) {
            buffer[x] += srcp[i][x] * coef[i];
            buffer[x] += adjp[i][x] * coef[i];
        }
    }
}

template<typename T>
static void filterComplexHigh(const T ** srcp, const T ** adjp, float * VS_RESTRICT buffer, const float * coef, const int width) {
    for (int i = 0; i < 5; i++) {
        for (int x = 0; x < width; x++) {
            buffer[x] += srcp[i][x] * coef[i];
            buffer[x] += adjp[i][x] * coef[i];
        }
    }
}

template<typename T>
static void filterScale(const float * buffer, T * VS_RESTRICT dstp, const int width, const int peak, const float lower, const float upper) {
    for (int x = 0; x < width; x++)
        dstp[x] = std::min(std::max(static_cast<int>(buffer[x] + 0.5f), 0), peak);
}

template<>
void filterScale<float>(const float * buffer, float * VS_RESTRICT dstp, const int width, const int peak, const float lower, const float upper) {
    for (int x = 0; x < width; x++)
        dstp[x] = std::min(std::max(buffer[x], lower), upper);
}

template<typename T>
static void process(const VSFrameRef * src, const VSFrameRef * adj, VSFrameRef * dst, float * VS_RESTRICT buffer, const int field, const W3FDIFData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane) / sizeof(T);
        const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
        const T * adjp = reinterpret_cast<const T *>(vsapi->getReadPtr(adj, plane));
        T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
        const T * srcps[5], * adjps[5];

        /* copy the unchanged lines of the field */
        vs_bitblt(dstp + stride * (1 - field), vsapi->getStride(dst, plane) * 2, srcp + stride * (1 - field), vsapi->getStride(src, plane) * 2, width * sizeof(T), height / 2);
        dstp += stride * field;

        /* interpolate the other lines of the field */
        for (int yOut = field; yOut < height; yOut += 2) {
            memset(buffer, 0, width * sizeof(float));

            /* get low vertical frequencies from current field */
            for (int j = 0; j < n_coef_lf[d->mode]; j++) {
                int yIn = yOut + 1 + j * 2 - n_coef_lf[d->mode];

                while (yIn < 0)
                    yIn += 2;
                while (yIn >= height)
                    yIn -= 2;

                srcps[j] = srcp + stride * yIn;
            }

            if (n_coef_lf[d->mode] == 2)
                filterSimpleLow<T>(srcps, buffer, coef_lf[d->mode], width);
            else
                filterComplexLow<T>(srcps, buffer, coef_lf[d->mode], width);

            /* get high vertical frequencies from adjacent fields */
            for (int j = 0; j < n_coef_hf[d->mode]; j++) {
                int yIn = yOut + 1 + j * 2 - n_coef_hf[d->mode];

                while (yIn < 0)
                    yIn += 2;
                while (yIn >= height)
                    yIn -= 2;

                srcps[j] = srcp + stride * yIn;
                adjps[j] = adjp + stride * yIn;
            }

            if (n_coef_hf[d->mode] == 3)
                filterSimpleHigh<T>(srcps, adjps, buffer, coef_hf[d->mode], width);
            else
                filterComplexHigh<T>(srcps, adjps, buffer, coef_hf[d->mode], width);

            filterScale<T>(buffer, dstp, width, d->peak, d->lower[plane], d->upper[plane]);

            dstp += stride * 2;
        }
    }
}

static void VS_CC w3fdifInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    W3FDIFData * d = static_cast<W3FDIFData *>(*instanceData);
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC w3fdifGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const W3FDIFData * d = static_cast<const W3FDIFData *>(*instanceData);

    if (activationReason == arInitial) {
        n /= 2;

        if (n > 0)
            vsapi->requestFrameFilter(n - 1, d->node, frameCtx);
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        if (n < d->viSaved->numFrames - 1)
            vsapi->requestFrameFilter(n + 1, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        float * buffer = vs_aligned_malloc<float>(d->vi.width * sizeof(float), 32);
        if (!buffer) {
            vsapi->setFilterError("W3FDIF: malloc failure (buffer)", frameCtx);
            return nullptr;
        }

        const int nSaved = n;
        n /= 2;

        const VSFrameRef * prv = vsapi->getFrameFilter(std::max(n - 1, 0), d->node, frameCtx);
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * nxt = vsapi->getFrameFilter(std::min(n + 1, d->viSaved->numFrames - 1), d->node, frameCtx);
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);

        int err;
        const int fieldBased = int64ToIntS(vsapi->propGetInt(vsapi->getFramePropsRO(src), "_FieldBased", 0, &err));
        int effectiveOrder = d->order;
        if (fieldBased == 1)
            effectiveOrder = 0;
        else if (fieldBased == 2)
            effectiveOrder = 1;

        const VSFrameRef * adj = (nSaved & 1) ? nxt : prv;
        const int field = (nSaved & 1) ? 1 - effectiveOrder : effectiveOrder;

        if (d->vi.format->sampleType == stInteger) {
            if (d->vi.format->bitsPerSample == 8)
                process<uint8_t>(src, adj, dst, buffer, field, d, vsapi);
            else
                process<uint16_t>(src, adj, dst, buffer, field, d, vsapi);
        } else {
            process<float>(src, adj, dst, buffer, field, d, vsapi);
        }

        VSMap * props = vsapi->getFramePropsRW(dst);
        vsapi->propSetInt(props, "_FieldBased", 0, paReplace);

        int errNum, errDen;
        int64_t durationNum = vsapi->propGetInt(props, "_DurationNum", 0, &errNum);
        int64_t durationDen = vsapi->propGetInt(props, "_DurationDen", 0, &errDen);
        if (!errNum && !errDen) {
            muldivRational(&durationNum, &durationDen, 1, 2);
            vsapi->propSetInt(props, "_DurationNum", durationNum, paReplace);
            vsapi->propSetInt(props, "_DurationDen", durationDen, paReplace);
        }

        vs_aligned_free(buffer);
        vsapi->freeFrame(prv);
        vsapi->freeFrame(src);
        vsapi->freeFrame(nxt);
        return dst;
    }

    return nullptr;
}

static void VS_CC w3fdifFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    W3FDIFData * d = static_cast<W3FDIFData *>(instanceData);
    vsapi->freeNode(d->node);
    delete d;
}

static void VS_CC w3fdifCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    W3FDIFData d;
    int err;

    d.order = int64ToIntS(vsapi->propGetInt(in, "order", 0, nullptr));

    d.mode = int64ToIntS(vsapi->propGetInt(in, "mode", 0, &err));
    if (err)
        d.mode = 1;

    if (d.order < 0 || d.order > 1) {
        vsapi->setError(out, "W3FDIF: order must be 0 or 1");
        return;
    }

    if (d.mode < 0 || d.mode > 1) {
        vsapi->setError(out, "W3FDIF: mode must be 0 or 1");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = *vsapi->getVideoInfo(d.node);
    d.viSaved = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(&d.vi) || (d.vi.format->sampleType == stInteger && d.vi.format->bitsPerSample > 16) ||
        (d.vi.format->sampleType == stFloat && d.vi.format->bitsPerSample != 32)) {
        vsapi->setError(out, "W3FDIF: only constant format 8-16 bits integer and 32 bits float input supported");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi.numFrames > INT_MAX / 2) {
        vsapi->setError(out, "W3FDIF: resulting clip is too long");
        vsapi->freeNode(d.node);
        return;
    }
    d.vi.numFrames *= 2;

    if (d.vi.fpsNum && d.vi.fpsDen)
        muldivRational(&d.vi.fpsNum, &d.vi.fpsDen, 2, 1);

    if (d.vi.format->sampleType == stInteger) {
        d.peak = (1 << d.vi.format->bitsPerSample) - 1;
    } else {
        for (int plane = 0; plane < d.vi.format->numPlanes; plane++) {
            if (plane == 0 || d.vi.format->colorFamily == cmRGB) {
                d.lower[plane] = 0.f;
                d.upper[plane] = 1.f;
            } else {
                d.lower[plane] = -0.5f;
                d.upper[plane] = 0.5f;
            }
        }
    }

    W3FDIFData * data = new W3FDIFData(d);

    vsapi->createFilter(in, out, "W3FDIF", w3fdifInit, w3fdifGetFrame, w3fdifFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.w3fdif", "w3fdif", "Weston 3 Field Deinterlacing Filter", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("W3FDIF",
                 "clip:clip;"
                 "order:int;"
                 "mode:int:opt;",
                 w3fdifCreate, nullptr, plugin);
}
