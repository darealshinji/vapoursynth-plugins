/*
**   VapourSynth port by HolyWu
**
**   Modification of Fizick's yadif avisynth filter.
**   Copyright (C) 2007 Kevin Stone
**   Yadif C-plugin for Avisynth 2.5 - Yet Another DeInterlacing Filter
**   Copyright (C) 2007 Alexander G. Balakhnin aka Fizick  http://avisynth.org.ru
**   Port of YADIF filter from MPlayer
**   Copyright (C) 2006 Michael Niedermayer <michaelni@gmx.at>
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

#include <VapourSynth.h>
#include <VSHelper.h>

#ifdef VS_TARGET_CPU_X86
#include "vectorclass/vectorclass.h"

template<typename T> extern void filter_sse2(const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const unsigned) noexcept;
template<typename T> extern void filter_avx(const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const unsigned) noexcept;
template<typename T> extern void filter_avx2(const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const unsigned) noexcept;
#endif

template<typename T> static void (*filter)(const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const unsigned);

struct YadifmodData {
    VSNodeRef * node, * edeint;
    VSVideoInfo vi;
    const VSVideoInfo * viSaved;
    int order, field, mode;
};

template<typename T>
static void filter_c(const T * prev2pp, const T * prev2pn, const T * prevp2p, const T * prevp, const T * prevp2n, const T * srcpp, const T * srcpn,
                     const T * nextp2p, const T * nextp, const T * nextp2n, const T * next2pp, const T * next2pn, const T * edeintp, T * VS_RESTRICT dstp,
                     const unsigned width, const unsigned yStart, const unsigned yStop, const unsigned stride, const unsigned mode) noexcept {
    for (unsigned y = yStart; y <= yStop; y += 2) {
        for (unsigned x = 0; x < width; x++) {
            int p1 = srcpp[x];
            const int p2 = (prevp[x] + nextp[x]) / 2;
            int p3 = srcpn[x];
            const int tdiff0 = std::abs(prevp[x] - nextp[x]) / 2;
            const int tdiff1 = (std::abs(prev2pp[x] - p1) + std::abs(prev2pn[x] - p3)) / 2;
            const int tdiff2 = (std::abs(next2pp[x] - p1) + std::abs(next2pn[x] - p3)) / 2;
            int diff = std::max({ tdiff0, tdiff1, tdiff2 });

            if (mode < 2) {
                const int p0 = (prevp2p[x] + nextp2p[x]) / 2 - p1;
                const int p4 = (prevp2n[x] + nextp2n[x]) / 2 - p3;
                p1 = p2 - p1;
                p3 = p2 - p3;
                const int maxs = std::max({ p3, p1, std::min(p0, p4) });
                const int mins = std::min({ p3, p1, std::max(p0, p4) });
                diff = std::max({ diff, mins, -maxs });
            }

            dstp[x] = std::min(std::max<int>(edeintp[x], p2 - diff), p2 + diff);
        }

        prev2pp += stride;
        prev2pn += stride;
        prevp2p += stride;
        prevp += stride;
        prevp2n += stride;
        srcpp += stride;
        srcpn += stride;
        nextp2p += stride;
        nextp += stride;
        nextp2n += stride;
        next2pp += stride;
        next2pn += stride;
        edeintp += stride;
        dstp += stride;
    }
}

template<>
void filter_c(const float * prev2pp, const float * prev2pn, const float * prevp2p, const float * prevp, const float * prevp2n, const float * srcpp, const float * srcpn,
              const float * nextp2p, const float * nextp, const float * nextp2n, const float * next2pp, const float * next2pn, const float * edeintp, float * VS_RESTRICT dstp,
              const unsigned width, const unsigned yStart, const unsigned yStop, const unsigned stride, const unsigned mode) noexcept {
    for (unsigned y = yStart; y <= yStop; y += 2) {
        for (unsigned x = 0; x < width; x++) {
            float p1 = srcpp[x];
            const float p2 = (prevp[x] + nextp[x]) * 0.5f;
            float p3 = srcpn[x];
            const float tdiff0 = std::abs(prevp[x] - nextp[x]) * 0.5f;
            const float tdiff1 = (std::abs(prev2pp[x] - p1) + std::abs(prev2pn[x] - p3)) * 0.5f;
            const float tdiff2 = (std::abs(next2pp[x] - p1) + std::abs(next2pn[x] - p3)) * 0.5f;
            float diff = std::max({ tdiff0, tdiff1, tdiff2 });

            if (mode < 2) {
                const float p0 = (prevp2p[x] + nextp2p[x]) * 0.5f - p1;
                const float p4 = (prevp2n[x] + nextp2n[x]) * 0.5f - p3;
                p1 = p2 - p1;
                p3 = p2 - p3;
                const float maxs = std::max({ p3, p1, std::min(p0, p4) });
                const float mins = std::min({ p3, p1, std::max(p0, p4) });
                diff = std::max({ diff, mins, -maxs });
            }

            dstp[x] = std::min(std::max(edeintp[x], p2 - diff), p2 + diff);
        }

        prev2pp += stride;
        prev2pn += stride;
        prevp2p += stride;
        prevp += stride;
        prevp2n += stride;
        srcpp += stride;
        srcpn += stride;
        nextp2p += stride;
        nextp += stride;
        nextp2n += stride;
        next2pp += stride;
        next2pn += stride;
        edeintp += stride;
        dstp += stride;
    }
}

template<typename T>
static void process(const VSFrameRef * prv, const VSFrameRef * src, const VSFrameRef * nxt, const VSFrameRef * edeint, VSFrameRef * dst,
                    const unsigned order, const unsigned field, const YadifmodData * d, const VSAPI * vsapi) noexcept {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const unsigned width = vsapi->getFrameWidth(src, plane);
        const unsigned height = vsapi->getFrameHeight(src, plane);
        const unsigned stride = vsapi->getStride(src, plane) / sizeof(T);
        const T * srcp0 = reinterpret_cast<const T *>(vsapi->getReadPtr(prv, plane));
        const T * srcp1 = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
        const T * srcp2 = reinterpret_cast<const T *>(vsapi->getReadPtr(nxt, plane));
        const T * edeintp = reinterpret_cast<const T *>(vsapi->getReadPtr(edeint, plane));
        T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

        if (!field) {
            memcpy(dstp, srcp1 + stride, width * sizeof(T));
            memcpy(dstp + stride * (height - 2), edeintp + stride * (height - 2), width * sizeof(T));
        } else {
            memcpy(dstp + stride, edeintp + stride, width * sizeof(T));
            memcpy(dstp + stride * (height - 1), srcp1 + stride * (height - 2), width * sizeof(T));
        }
        vs_bitblt(dstp + stride * (1 - field), vsapi->getStride(dst, plane) * 2, srcp1 + stride * (1 - field), vsapi->getStride(src, plane) * 2, width * sizeof(T), height / 2);

        const unsigned yStart = 2 + field;
        const unsigned yStop = field ? height - 3 : height - 4;

        const T * prev2pp = srcp0 + stride * (yStart - 1);
        const T * srcpp = srcp1 + stride * (yStart - 1);
        const T * next2pp = srcp2 + stride * (yStart - 1);

        const T * prevp, * nextp;
        if (field ^ order) {
            prevp = srcp1 + stride * yStart;
            nextp = srcp2 + stride * yStart;
        } else {
            prevp = srcp0 + stride * yStart;
            nextp = srcp1 + stride * yStart;
        }
        edeintp += stride * yStart;
        dstp += stride * yStart;

        const T * prev2pn = prev2pp + stride * 2;
        const T * prevp2p = prevp - stride * 2;
        const T * prevp2n = prevp + stride * 2;
        const T * srcpn = srcpp + stride * 2;
        const T * nextp2p = nextp - stride * 2;
        const T * nextp2n = nextp + stride * 2;
        const T * next2pn = next2pp + stride * 2;

        filter<T>(prev2pp, prev2pn, prevp2p, prevp, prevp2n, srcpp, srcpn, nextp2p, nextp, nextp2n, next2pp, next2pn, edeintp, dstp, width, yStart, yStop, stride * 2, d->mode);
    }
}

static void selectFunctions(const unsigned opt) noexcept {
    filter<uint8_t> = filter_c;
    filter<uint16_t> = filter_c;
    filter<float> = filter_c;

#ifdef VS_TARGET_CPU_X86
    const int iset = instrset_detect();
    if (opt == 4 || (opt == 0 && iset >= 8)) {
        filter<uint8_t> = filter_avx2;
        filter<uint16_t> = filter_avx2;
        filter<float> = filter_avx2;
    } else if (opt == 3 || (opt == 0 && iset == 7)) {
        filter<uint8_t> = filter_avx;
        filter<uint16_t> = filter_avx;
        filter<float> = filter_avx;
    } else if (opt == 2 || (opt == 0 && iset >= 2)) {
        filter<uint8_t> = filter_sse2;
        filter<uint16_t> = filter_sse2;
        filter<float> = filter_sse2;
    }
#endif
}

static void VS_CC yadifmodInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    YadifmodData * d = static_cast<YadifmodData *>(*instanceData);
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC yadifmodGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const YadifmodData * d = static_cast<const YadifmodData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->edeint, frameCtx);

        if (d->mode & 1)
            n /= 2;

        if (n > 0)
            vsapi->requestFrameFilter(n - 1, d->node, frameCtx);
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        if (n < d->viSaved->numFrames - 1)
            vsapi->requestFrameFilter(n + 1, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
#ifdef VS_TARGET_CPU_X86
        no_subnormals();
#endif

        const VSFrameRef * edeint = vsapi->getFrameFilter(n, d->edeint, frameCtx);

        const int nSaved = n;
        if (d->mode & 1)
            n /= 2;

        const VSFrameRef * prv = vsapi->getFrameFilter(std::max(n - 1, 0), d->node, frameCtx);
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * nxt = vsapi->getFrameFilter(std::min(n + 1, d->viSaved->numFrames - 1), d->node, frameCtx);
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);

        int err;
        const int fieldBased = int64ToIntS(vsapi->propGetInt(vsapi->getFramePropsRO(src), "_FieldBased", 0, &err));
        unsigned order = d->order;
        if (fieldBased == 1)
            order = 0;
        else if (fieldBased == 2)
            order = 1;

        unsigned field;
        if (d->mode & 1)
            field = (nSaved & 1) ? 1 - order : order;
        else
            field = (d->field == -1) ? order : d->field;

        if (d->vi.format->bytesPerSample == 1)
            process<uint8_t>(prv, src, nxt, edeint, dst, order, field, d, vsapi);
        else if (d->vi.format->bytesPerSample == 2)
            process<uint16_t>(prv, src, nxt, edeint, dst, order, field, d, vsapi);
        else
            process<float>(prv, src, nxt, edeint, dst, order, field, d, vsapi);

        VSMap * props = vsapi->getFramePropsRW(dst);
        vsapi->propSetInt(props, "_FieldBased", 0, paReplace);

        if (d->mode & 1) {
            int errNum, errDen;
            int64_t durationNum = vsapi->propGetInt(props, "_DurationNum", 0, &errNum);
            int64_t durationDen = vsapi->propGetInt(props, "_DurationDen", 0, &errDen);
            if (!errNum && !errDen) {
                muldivRational(&durationNum, &durationDen, 1, 2);
                vsapi->propSetInt(props, "_DurationNum", durationNum, paReplace);
                vsapi->propSetInt(props, "_DurationDen", durationDen, paReplace);
            }
        }

        vsapi->freeFrame(prv);
        vsapi->freeFrame(src);
        vsapi->freeFrame(nxt);
        vsapi->freeFrame(edeint);
        return dst;
    }

    return nullptr;
}

static void VS_CC yadifmodFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    YadifmodData * d = static_cast<YadifmodData *>(instanceData);
    vsapi->freeNode(d->node);
    vsapi->freeNode(d->edeint);
    delete d;
}

static void VS_CC yadifmodCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<YadifmodData> d{ new YadifmodData{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->edeint = vsapi->propGetNode(in, "edeint", 0, nullptr);
    d->vi = *vsapi->getVideoInfo(d->node);
    d->viSaved = vsapi->getVideoInfo(d->node);

    try {
        if (!isConstantFormat(&d->vi) || (d->vi.format->sampleType == stInteger && d->vi.format->bitsPerSample > 16) ||
            (d->vi.format->sampleType == stFloat && d->vi.format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bits integer and 32 bits float input supported" };

        if (d->vi.height < 4)
            throw std::string{ "the clip's height must be greater than or equal to 4" };

        if (!isSameFormat(vsapi->getVideoInfo(d->edeint), &d->vi))
            throw std::string{ "edeint clip must have the same dimensions as main clip and be the same format" };

        d->order = int64ToIntS(vsapi->propGetInt(in, "order", 0, nullptr));

        d->field = int64ToIntS(vsapi->propGetInt(in, "field", 0, &err));
        if (err)
            d->field = -1;

        d->mode = int64ToIntS(vsapi->propGetInt(in, "mode", 0, &err));

        const int opt = int64ToIntS(vsapi->propGetInt(in, "opt", 0, &err));

        if (d->order < 0 || d->order > 1)
            throw std::string{ "order must be 0 or 1" };

        if (d->field < -1 || d->field > 1)
            throw std::string{ "field must be -1, 0 or 1" };

        if (d->mode < 0 || d->mode > 3)
            throw std::string{ "mode must be 0, 1, 2 or 3" };

        if (opt < 0 || opt > 4)
            throw std::string{ "opt must be 0, 1, 2, 3 or 4" };

        if (d->mode & 1) {
            if (d->vi.numFrames > INT_MAX / 2)
                throw std::string{ "resulting clip is too long" };
            d->vi.numFrames *= 2;

            muldivRational(&d->vi.fpsNum, &d->vi.fpsDen, 2, 1);
        }

        if (vsapi->getVideoInfo(d->edeint)->numFrames != d->vi.numFrames)
            throw std::string{ "edeint clip's number of frames doesn't match" };

        selectFunctions(opt);
    } catch (const std::string & error) {
        vsapi->setError(out, ("Yadifmod: " + error).c_str());
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->edeint);
        return;
    }

    vsapi->createFilter(in, out, "Yadifmod", yadifmodInit, yadifmodGetFrame, yadifmodFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.yadifmod", "yadifmod", "Modification of Fizick's yadif avisynth filter", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Yadifmod",
                 "clip:clip;"
                 "edeint:clip;"
                 "order:int;"
                 "field:int:opt;"
                 "mode:int:opt;"
                 "opt:int:opt;",
                 yadifmodCreate, nullptr, plugin);
}
