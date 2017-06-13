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
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>
#ifdef VS_TARGET_CPU_X86
#include "vectorclass/vectorclass.h"
#endif

struct YadifmodData {
    VSNodeRef * node;
    VSNodeRef * edeint;
    VSVideoInfo vi;
    const VSVideoInfo * viSaved;
    int order, field, mode;
};

#ifdef VS_TARGET_CPU_X86
template<typename T, typename V1, typename V2, int vectorSize>
static void filter(const T * _prev2pp, const T * _prev2pn, const T * _prevp2p, const T * _prevp, const T * _prevp2n, const T * _srcpp, const T * _srcpn,
                   const T * _nextp2p, const T * _nextp, const T * _nextp2n, const T * _next2pp, const T * _next2pn, const T * _edeintp, T * dstp,
                   const int width, const int starty, const int stopy, const int stride, const int mode) {
    for (int y = starty; y <= stopy; y += 2) {
        for (int x = 0; x < width; x += vectorSize) {
            const V1 prev2pp = V1().load_a(_prev2pp + x);
            const V1 prev2pn = V1().load_a(_prev2pn + x);
            const V1 prevp2p = V1().load_a(_prevp2p + x);
            const V1 prevp = V1().load_a(_prevp + x);
            const V1 prevp2n = V1().load_a(_prevp2n + x);
            const V1 srcpp = V1().load_a(_srcpp + x);
            const V1 srcpn = V1().load_a(_srcpn + x);
            const V1 nextp2p = V1().load_a(_nextp2p + x);
            const V1 nextp = V1().load_a(_nextp + x);
            const V1 nextp2n = V1().load_a(_nextp2n + x);
            const V1 next2pp = V1().load_a(_next2pp + x);
            const V1 next2pn = V1().load_a(_next2pn + x);
            const V1 edeintp = V1().load_a(_edeintp + x);

            const V2 p1 = V2(extend_low(srcpp), extend_high(srcpp));
            const V2 p2 = (V2(extend_low(prevp), extend_high(prevp)) + V2(extend_low(nextp), extend_high(nextp))) >> 1;
            const V2 p3 = V2(extend_low(srcpn), extend_high(srcpn));
            const V2 tdiff0 = abs(V2(extend_low(prevp), extend_high(prevp)) - V2(extend_low(nextp), extend_high(nextp))) >> 1;
            const V2 tdiff1 = (abs(V2(extend_low(prev2pp), extend_high(prev2pp)) - p1) + abs(V2(extend_low(prev2pn), extend_high(prev2pn)) - p3)) >> 1;
            const V2 tdiff2 = (abs(V2(extend_low(next2pp), extend_high(next2pp)) - p1) + abs(V2(extend_low(next2pn), extend_high(next2pn)) - p3)) >> 1;
            V2 diff = max(max(tdiff0, tdiff1), tdiff2);
            if (mode < 2) {
                const V2 p0 = (V2(extend_low(prevp2p), extend_high(prevp2p)) + V2(extend_low(nextp2p), extend_high(nextp2p))) >> 1;
                const V2 p4 = (V2(extend_low(prevp2n), extend_high(prevp2n)) + V2(extend_low(nextp2n), extend_high(nextp2n))) >> 1;
                const V2 maxs = max(max(p2 - p3, p2 - p1), min(p0 - p1, p4 - p3));
                const V2 mins = min(min(p2 - p3, p2 - p1), max(p0 - p1, p4 - p3));
                diff = max(max(diff, mins), -maxs);
            }
            const V2 spatialPred = min(max(V2(extend_low(edeintp), extend_high(edeintp)), p2 - diff), p2 + diff);
            V1(compress(spatialPred.get_low(), spatialPred.get_high())).store_a(dstp + x);
        }
        _prev2pp += stride;
        _prev2pn += stride;
        _prevp2p += stride;
        _prevp += stride;
        _prevp2n += stride;
        _srcpp += stride;
        _srcpn += stride;
        _nextp2p += stride;
        _nextp += stride;
        _nextp2n += stride;
        _next2pp += stride;
        _next2pn += stride;
        _edeintp += stride;
        dstp += stride;
    }
}

template<>
void filter<float, void, void, 0>(const float * _prev2pp, const float * _prev2pn, const float * _prevp2p, const float * _prevp, const float * _prevp2n,
                                  const float * _srcpp, const float * _srcpn,
                                  const float * _nextp2p, const float * _nextp, const float * _nextp2n, const float * _next2pp, const float * _next2pn,
                                  const float * _edeintp, float * dstp, const int width, const int starty, const int stopy, const int stride, const int mode) {
    const Vec8f pointFive(0.5f);

    for (int y = starty; y <= stopy; y += 2) {
        for (int x = 0; x < width; x += 8) {
            const Vec8f prev2pp = Vec8f().load_a(_prev2pp + x);
            const Vec8f prev2pn = Vec8f().load_a(_prev2pn + x);
            const Vec8f prevp2p = Vec8f().load_a(_prevp2p + x);
            const Vec8f prevp = Vec8f().load_a(_prevp + x);
            const Vec8f prevp2n = Vec8f().load_a(_prevp2n + x);
            const Vec8f srcpp = Vec8f().load_a(_srcpp + x);
            const Vec8f srcpn = Vec8f().load_a(_srcpn + x);
            const Vec8f nextp2p = Vec8f().load_a(_nextp2p + x);
            const Vec8f nextp = Vec8f().load_a(_nextp + x);
            const Vec8f nextp2n = Vec8f().load_a(_nextp2n + x);
            const Vec8f next2pp = Vec8f().load_a(_next2pp + x);
            const Vec8f next2pn = Vec8f().load_a(_next2pn + x);
            const Vec8f edeintp = Vec8f().load_a(_edeintp + x);

            const Vec8f p1 = srcpp;
            const Vec8f p2 = (prevp + nextp) * pointFive;
            const Vec8f p3 = srcpn;
            const Vec8f tdiff0 = abs(prevp - nextp) * pointFive;
            const Vec8f tdiff1 = (abs(prev2pp - p1) + abs(prev2pn - p3)) * pointFive;
            const Vec8f tdiff2 = (abs(next2pp - p1) + abs(next2pn - p3)) * pointFive;
            Vec8f diff = max(max(tdiff0, tdiff1), tdiff2);
            if (mode < 2) {
                const Vec8f p0 = (prevp2p + nextp2p) * pointFive;
                const Vec8f p4 = (prevp2n + nextp2n) * pointFive;
                const Vec8f maxs = max(max(p2 - p3, p2 - p1), min(p0 - p1, p4 - p3));
                const Vec8f mins = min(min(p2 - p3, p2 - p1), max(p0 - p1, p4 - p3));
                diff = max(max(diff, mins), -maxs);
            }
            const Vec8f spatialPred = min(max(edeintp, p2 - diff), p2 + diff);
            spatialPred.store_a(dstp + x);
        }
        _prev2pp += stride;
        _prev2pn += stride;
        _prevp2p += stride;
        _prevp += stride;
        _prevp2n += stride;
        _srcpp += stride;
        _srcpn += stride;
        _nextp2p += stride;
        _nextp += stride;
        _nextp2n += stride;
        _next2pp += stride;
        _next2pn += stride;
        _edeintp += stride;
        dstp += stride;
    }
}
#else
template<typename T>
static void filter(const T * prev2pp, const T * prev2pn, const T * prevp2p, const T * prevp, const T * prevp2n, const T * srcpp, const T * srcpn,
                   const T * nextp2p, const T * nextp, const T * nextp2n, const T * next2pp, const T * next2pn, const T * edeintp, T * VS_RESTRICT dstp,
                   const int width, const int starty, const int stopy, const int stride, const int mode) {
    for (int y = starty; y <= stopy; y += 2) {
        for (int x = 0; x < width; x++) {
            const int p1 = srcpp[x];
            const int p2 = (prevp[x] + nextp[x]) >> 1;
            const int p3 = srcpn[x];
            const int tdiff0 = std::abs(prevp[x] - nextp[x]) >> 1;
            const int tdiff1 = (std::abs(prev2pp[x] - p1) + std::abs(prev2pn[x] - p3)) >> 1;
            const int tdiff2 = (std::abs(next2pp[x] - p1) + std::abs(next2pn[x] - p3)) >> 1;
            int diff = std::max(std::max(tdiff0, tdiff1), tdiff2);
            if (mode < 2) {
                const int p0 = (prevp2p[x] + nextp2p[x]) >> 1;
                const int p4 = (prevp2n[x] + nextp2n[x]) >> 1;
                const int maxs = std::max(std::max(p2 - p3, p2 - p1), std::min(p0 - p1, p4 - p3));
                const int mins = std::min(std::min(p2 - p3, p2 - p1), std::max(p0 - p1, p4 - p3));
                diff = std::max(std::max(diff, mins), -maxs);
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
void filter<float>(const float * prev2pp, const float * prev2pn, const float * prevp2p, const float * prevp, const float * prevp2n, const float * srcpp, const float * srcpn,
                   const float * nextp2p, const float * nextp, const float * nextp2n, const float * next2pp, const float * next2pn, const float * edeintp, float * VS_RESTRICT dstp,
                   const int width, const int starty, const int stopy, const int stride, const int mode) {
    for (int y = starty; y <= stopy; y += 2) {
        for (int x = 0; x < width; x++) {
            const float p1 = srcpp[x];
            const float p2 = (prevp[x] + nextp[x]) * 0.5f;
            const float p3 = srcpn[x];
            const float tdiff0 = std::abs(prevp[x] - nextp[x]) * 0.5f;
            const float tdiff1 = (std::abs(prev2pp[x] - p1) + std::abs(prev2pn[x] - p3)) * 0.5f;
            const float tdiff2 = (std::abs(next2pp[x] - p1) + std::abs(next2pn[x] - p3)) * 0.5f;
            float diff = std::max(std::max(tdiff0, tdiff1), tdiff2);
            if (mode < 2) {
                const float p0 = (prevp2p[x] + nextp2p[x]) * 0.5f;
                const float p4 = (prevp2n[x] + nextp2n[x]) * 0.5f;
                const float maxs = std::max(std::max(p2 - p3, p2 - p1), std::min(p0 - p1, p4 - p3));
                const float mins = std::min(std::min(p2 - p3, p2 - p1), std::max(p0 - p1, p4 - p3));
                diff = std::max(std::max(diff, mins), -maxs);
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
#endif

template<typename T, typename V1, typename V2, int vectorSize>
static void process(const VSFrameRef * prv, const VSFrameRef * src, const VSFrameRef * nxt, const VSFrameRef * edeint, VSFrameRef * dst,
                    const int order, const int field, const YadifmodData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane) / sizeof(T);
        const T * srcp0 = reinterpret_cast<const T *>(vsapi->getReadPtr(prv, plane));
        const T * srcp1 = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
        const T * srcp2 = reinterpret_cast<const T *>(vsapi->getReadPtr(nxt, plane));
        const T * edeintp = reinterpret_cast<const T *>(vsapi->getReadPtr(edeint, plane));
        T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

        if (!field)
            memcpy(dstp, srcp1 + stride, width * sizeof(T));
        else
            memcpy(dstp + stride, edeintp + stride, width * sizeof(T));
        vs_bitblt(dstp + stride * (1 - field), vsapi->getStride(src, plane) * 2, srcp1 + stride * (1 - field), vsapi->getStride(src, plane) * 2, width * sizeof(T), height / 2);

        const int starty = 2 + field;
        const int stopy = field ? height - 3 : height - 4;

        const int stride1 = stride * starty;
        const T * prevp, * nextp;
        if (field ^ order) {
            prevp = srcp1 + stride1;
            nextp = srcp2 + stride1;
        } else {
            prevp = srcp0 + stride1;
            nextp = srcp1 + stride1;
        }
        edeintp += stride1;
        dstp += stride1;

        const int stride2 = stride * (starty - 1);
        const T * prev2pp = srcp0 + stride2;
        const T * srcpp = srcp1 + stride2;
        const T * next2pp = srcp2 + stride2;

        const int stride3 = stride * 2;
        const T * prev2pn = prev2pp + stride3;
        const T * prevp2p = prevp - stride3;
        const T * prevp2n = prevp + stride3;
        const T * srcpn = srcpp + stride3;
        const T * nextp2p = nextp - stride3;
        const T * nextp2n = nextp + stride3;
        const T * next2pn = next2pp + stride3;

#ifdef VS_TARGET_CPU_X86
        filter<T, V1, V2, vectorSize>(prev2pp, prev2pn, prevp2p, prevp, prevp2n, srcpp, srcpn, nextp2p, nextp, nextp2n, next2pp, next2pn, edeintp, dstp,
                                      width, starty, stopy, stride3, d->mode);
#else
        filter<T>(prev2pp, prev2pn, prevp2p, prevp, prevp2n, srcpp, srcpn, nextp2p, nextp, nextp2n, next2pp, next2pn, edeintp, dstp, width, starty, stopy, stride3, d->mode);
#endif

        if (!field)
            memcpy(vsapi->getWritePtr(dst, plane) + vsapi->getStride(src, plane) * (height - 2), vsapi->getReadPtr(edeint, plane) + vsapi->getStride(src, plane) * (height - 2),
                   width * sizeof(T));
        else
            memcpy(vsapi->getWritePtr(dst, plane) + vsapi->getStride(src, plane) * (height - 1), srcp1 + stride * (height - 2), width * sizeof(T));
    }
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
        int effectiveOrder = d->order;
        if (fieldBased == 1)
            effectiveOrder = 0;
        else if (fieldBased == 2)
            effectiveOrder = 1;

        int fieldt;
        if (d->mode & 1)
            fieldt = (nSaved & 1) ? 1 - effectiveOrder : effectiveOrder;
        else
            fieldt = (d->field == -1) ? effectiveOrder : d->field;

        if (d->vi.format->sampleType == stInteger) {
            if (d->vi.format->bitsPerSample == 8) {
#ifdef VS_TARGET_CPU_X86
                process<uint8_t, Vec16uc, Vec16s, 16>(prv, src, nxt, edeint, dst, effectiveOrder, fieldt, d, vsapi);
#else
                process<uint8_t, void, void, 0>(prv, src, nxt, edeint, dst, effectiveOrder, fieldt, d, vsapi);
#endif
            } else {
#ifdef VS_TARGET_CPU_X86
                process<uint16_t, Vec8us, Vec8i, 8>(prv, src, nxt, edeint, dst, effectiveOrder, fieldt, d, vsapi);
#else
                process<uint16_t, void, void, 0>(prv, src, nxt, edeint, dst, effectiveOrder, fieldt, d, vsapi);
#endif
            }
        } else {
            process<float, void, void, 0>(prv, src, nxt, edeint, dst, effectiveOrder, fieldt, d, vsapi);
        }

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
    YadifmodData d;
    int err;

    d.order = int64ToIntS(vsapi->propGetInt(in, "order", 0, nullptr));

    d.field = int64ToIntS(vsapi->propGetInt(in, "field", 0, &err));
    if (err)
        d.field = -1;

    d.mode = int64ToIntS(vsapi->propGetInt(in, "mode", 0, &err));

    if (d.order < 0 || d.order > 1) {
        vsapi->setError(out, "Yadifmod: order must be 0 or 1");
        return;
    }

    if (d.field < -1 || d.field > 1) {
        vsapi->setError(out, "Yadifmod: field must be -1, 0 or 1");
        return;
    }

    if (d.mode < 0 || d.mode > 3) {
        vsapi->setError(out, "Yadifmod: mode must be 0, 1, 2 or 3");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.edeint = vsapi->propGetNode(in, "edeint", 0, nullptr);
    d.vi = *vsapi->getVideoInfo(d.node);
    d.viSaved = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(&d.vi) || (d.vi.format->sampleType == stInteger && d.vi.format->bitsPerSample > 16) ||
        (d.vi.format->sampleType == stFloat && d.vi.format->bitsPerSample != 32)) {
        vsapi->setError(out, "Yadifmod: only constant format 8-16 bits integer and 32 bits float input supported");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.edeint);
        return;
    }

    if (!isSameFormat(vsapi->getVideoInfo(d.edeint), &d.vi)) {
        vsapi->setError(out, "Yadifmod: edeint clip must have the same dimensions as main clip and be the same format");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.edeint);
        return;
    }

    if (d.mode & 1) {
        if (d.vi.numFrames > INT_MAX / 2) {
            vsapi->setError(out, "Yadifmod: resulting clip is too long");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.edeint);
            return;
        }
        d.vi.numFrames *= 2;

        if (d.vi.fpsNum && d.vi.fpsDen)
            muldivRational(&d.vi.fpsNum, &d.vi.fpsDen, 2, 1);
    }

    if (vsapi->getVideoInfo(d.edeint)->numFrames != d.vi.numFrames) {
        vsapi->setError(out, "Yadifmod: edeint clip's number of frames doesn't match");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.edeint);
        return;
    }

    YadifmodData * data = new YadifmodData(d);

    vsapi->createFilter(in, out, "Yadifmod", yadifmodInit, yadifmodGetFrame, yadifmodFree, fmParallel, 0, data, core);
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
                 "mode:int:opt;",
                 yadifmodCreate, nullptr, plugin);
}
