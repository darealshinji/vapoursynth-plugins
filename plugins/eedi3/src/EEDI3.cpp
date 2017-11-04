/*
**   Another VapourSynth port by HolyWu
**
**   eedi3 (enhanced edge directed interpolation 3). Works by finding the
**   best non-decreasing (non-crossing) warping between two lines according to
**   a cost functional. Doesn't really have anything to do with eedi2 aside
**   from doing edge-directed interpolation (they use different techniques).
**
**   Copyright (C) 2010 Kevin Stone - some part by Laurent de Soras, 2013
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <memory>
#include <string>

#include "EEDI3.hpp"

#ifdef VS_TARGET_CPU_X86
template<typename T1, typename T2> extern void process_sse2(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3Data *, const VSAPI *) noexcept;
template<typename T1, typename T2> extern void process_sse4(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3Data *, const VSAPI *) noexcept;
template<typename T1, typename T2> extern void process_avx(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3Data *, const VSAPI *) noexcept;
template<typename T1, typename T2> extern void process_avx512(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3Data *, const VSAPI *) noexcept;
#endif

template<typename T>
static inline void calculateConnectionCosts(const T * src3p, const T * src1p, const T * src1n, const T * src3n, float * VS_RESTRICT ccosts,
                                            const int width, const EEDI3Data * d) noexcept {
    if (d->cost3) {
        for (int x = 0; x < width; x++) {
            const int umax = std::min({ x, width - 1 - x, d->mdis });

            for (int u = -umax; u <= umax; u++) {
                const int u2 = u * 2;
                int s0 = 0, s1 = -1, s2 = -1;

                for (int k = -(d->nrad); k <= d->nrad; k++)
                    s0 += std::abs(src3p[x + u + k] - src1p[x - u + k]) +
                          std::abs(src1p[x + u + k] - src1n[x - u + k]) +
                          std::abs(src1n[x + u + k] - src3n[x - u + k]);

                if ((u >= 0 && x >= u2) || (u <= 0 && x < width + u2)) {
                    s1 = 0;
                    for (int k = -(d->nrad); k <= d->nrad; k++)
                        s1 += std::abs(src3p[x + k] - src1p[x - u2 + k]) +
                              std::abs(src1p[x + k] - src1n[x - u2 + k]) +
                              std::abs(src1n[x + k] - src3n[x - u2 + k]);
                }

                if ((u <= 0 && x >= -u2) || (u >= 0 && x < width - u2)) {
                    s2 = 0;
                    for (int k = -(d->nrad); k <= d->nrad; k++)
                        s2 += std::abs(src3p[x + u2 + k] - src1p[x + k]) +
                              std::abs(src1p[x + u2 + k] - src1n[x + k]) +
                              std::abs(src1n[x + u2 + k] - src3n[x + k]);
                }

                s1 = (s1 >= 0) ? s1 : (s2 >= 0 ? s2 : s0);
                s2 = (s2 >= 0) ? s2 : (s1 >= 0 ? s1 : s0);

                const int ip = (src1p[x + u] + src1n[x - u] + 1) >> 1; // should use cubic if ucubic=true
                const int v = std::abs(src1p[x] - ip) + std::abs(src1n[x] - ip);
                ccosts[d->tpitch * x + u] = d->alpha * (s0 + s1 + s2) + d->beta * std::abs(u) + d->remainingWeight * v;
            }
        }
    } else {
        for (int x = 0; x < width; x++) {
            const int umax = std::min({ x, width - 1 - x, d->mdis });

            for (int u = -umax; u <= umax; u++) {
                int s = 0;

                for (int k = -(d->nrad); k <= d->nrad; k++)
                    s += std::abs(src3p[x + u + k] - src1p[x - u + k]) +
                         std::abs(src1p[x + u + k] - src1n[x - u + k]) +
                         std::abs(src1n[x + u + k] - src3n[x - u + k]);

                const int ip = (src1p[x + u] + src1n[x - u] + 1) >> 1; // should use cubic if ucubic=true
                const int v = std::abs(src1p[x] - ip) + std::abs(src1n[x] - ip);
                ccosts[d->tpitch * x + u] = d->alpha * s + d->beta * std::abs(u) + d->remainingWeight * v;
            }
        }
    }
}

template<>
inline void calculateConnectionCosts(const float * src3p, const float * src1p, const float * src1n, const float * src3n, float * VS_RESTRICT ccosts,
                                     const int width, const EEDI3Data * d) noexcept {
    if (d->cost3) {
        for (int x = 0; x < width; x++) {
            const int umax = std::min({ x, width - 1 - x, d->mdis });

            for (int u = -umax; u <= umax; u++) {
                const int u2 = u * 2;
                float s0 = 0.f, s1 = -FLT_MAX, s2 = -FLT_MAX;

                for (int k = -(d->nrad); k <= d->nrad; k++)
                    s0 += std::abs(src3p[x + u + k] - src1p[x - u + k]) +
                          std::abs(src1p[x + u + k] - src1n[x - u + k]) +
                          std::abs(src1n[x + u + k] - src3n[x - u + k]);

                if ((u >= 0 && x >= u2) || (u <= 0 && x < width + u2)) {
                    s1 = 0.f;
                    for (int k = -(d->nrad); k <= d->nrad; k++)
                        s1 += std::abs(src3p[x + k] - src1p[x - u2 + k]) +
                              std::abs(src1p[x + k] - src1n[x - u2 + k]) +
                              std::abs(src1n[x + k] - src3n[x - u2 + k]);
                }

                if ((u <= 0 && x >= -u2) || (u >= 0 && x < width - u2)) {
                    s2 = 0.f;
                    for (int k = -(d->nrad); k <= d->nrad; k++)
                        s2 += std::abs(src3p[x + u2 + k] - src1p[x + k]) +
                              std::abs(src1p[x + u2 + k] - src1n[x + k]) +
                              std::abs(src1n[x + u2 + k] - src3n[x + k]);
                }

                s1 = (s1 > -FLT_MAX) ? s1 : (s2 > -FLT_MAX ? s2 : s0);
                s2 = (s2 > -FLT_MAX) ? s2 : (s1 > -FLT_MAX ? s1 : s0);

                const float ip = (src1p[x + u] + src1n[x - u]) * 0.5f; // should use cubic if ucubic=true
                const float v = std::abs(src1p[x] - ip) + std::abs(src1n[x] - ip);
                ccosts[d->tpitch * x + u] = d->alpha * (s0 + s1 + s2) + d->beta * std::abs(u) + d->remainingWeight * v;
            }
        }
    } else {
        for (int x = 0; x < width; x++) {
            const int umax = std::min({ x, width - 1 - x, d->mdis });

            for (int u = -umax; u <= umax; u++) {
                float s = 0.f;

                for (int k = -(d->nrad); k <= d->nrad; k++)
                    s += std::abs(src3p[x + u + k] - src1p[x - u + k]) +
                         std::abs(src1p[x + u + k] - src1n[x - u + k]) +
                         std::abs(src1n[x + u + k] - src3n[x - u + k]);

                const float ip = (src1p[x + u] + src1n[x - u]) * 0.5f; // should use cubic if ucubic=true
                const float v = std::abs(src1p[x] - ip) + std::abs(src1n[x] - ip);
                ccosts[d->tpitch * x + u] = d->alpha * s + d->beta * std::abs(u) + d->remainingWeight * v;
            }
        }
    }
}

template<typename T1, typename T2>
static void process_c(const VSFrameRef * src, const VSFrameRef * scp, VSFrameRef * dst, VSFrameRef ** pad, const int field_n, const EEDI3Data * d, const VSAPI * vsapi) noexcept {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        if (d->process[plane]) {
            copyPad<T1>(src, pad[plane], plane, 1 - field_n, d->dh, vsapi);

            const int srcWidth = vsapi->getFrameWidth(pad[plane], 0);
            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int srcHeight = vsapi->getFrameHeight(pad[plane], 0);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int srcStride = vsapi->getStride(pad[plane], 0) / sizeof(T1);
            const int dstStride = vsapi->getStride(dst, plane) / sizeof(T1);
            const T1 * _srcp = reinterpret_cast<const T1 *>(vsapi->getReadPtr(pad[plane], 0)) + 12;
            T1 * VS_RESTRICT _dstp = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane));

            const auto threadId = std::this_thread::get_id();
            float * ccosts = d->ccosts.at(threadId) + d->mdis;
            float * pcosts = d->pcosts.at(threadId) + d->mdis;
            int * pbackt = d->pbackt.at(threadId) + d->mdis;
            int * fpath = d->fpath.at(threadId);
            int * _dmap = d->dmap.at(threadId);
            float * tline = d->tline.at(threadId);

            vs_bitblt(_dstp + dstStride * (1 - field_n), vsapi->getStride(dst, plane) * 2,
                      _srcp + srcStride * (4 + 1 - field_n), vsapi->getStride(pad[plane], 0) * 2,
                      dstWidth * sizeof(T1), dstHeight / 2);

            _srcp += srcStride * (4 + field_n);
            _dstp += dstStride * field_n;

            for (int y = 4 + field_n; y < srcHeight - 4; y += 2) {
                const int off = (y - 4 - field_n) >> 1;
                const T1 * srcp = _srcp + srcStride * 2 * off;
                T1 * dstp = _dstp + dstStride * 2 * off;
                int * dmap = _dmap + dstWidth * off;

                const T1 * src3p = srcp - srcStride * 3;
                const T1 * src1p = srcp - srcStride;
                const T1 * src1n = srcp + srcStride;
                const T1 * src3n = srcp + srcStride * 3;

                calculateConnectionCosts<T1>(src3p, src1p, src1n, src3n, ccosts, dstWidth, d);

                // calculate path costs
                *pcosts = *ccosts;
                for (int x = 1; x < dstWidth; x++) {
                    const float * tT = ccosts + d->tpitch * x;
                    const float * ppT = pcosts + d->tpitch * (x - 1);
                    float * pT = pcosts + d->tpitch * x;
                    int * piT = pbackt + d->tpitch * (x - 1);

                    const int umax = std::min({ x, dstWidth - 1 - x, d->mdis });
                    const int umax2 = std::min({ x - 1, dstWidth - x, d->mdis });

                    for (int u = -umax; u <= umax; u++) {
                        int idx = 0;
                        float bval = FLT_MAX;

                        for (int v = std::max(-umax2, u - 1); v <= std::min(umax2, u + 1); v++) {
                            const double z = ppT[v] + d->gamma * std::abs(u - v);
                            const float ccost = static_cast<float>(std::min(z, FLT_MAX * 0.9));
                            if (ccost < bval) {
                                bval = ccost;
                                idx = v;
                            }
                        }

                        const double z = bval + tT[u];
                        pT[u] = static_cast<float>(std::min(z, FLT_MAX * 0.9));
                        piT[u] = idx;
                    }
                }

                // backtrack
                fpath[dstWidth - 1] = 0;
                for (int x = dstWidth - 2; x >= 0; x--)
                    fpath[x] = pbackt[d->tpitch * x + fpath[x + 1]];

                interpolate<T1>(src3p, src1p, src1n, src3n, fpath, dmap, dstp, dstWidth, d->ucubic, d->peak);
            }

            if (d->vcheck) {
                const T1 * scpp = nullptr;
                if (d->sclip)
                    scpp = reinterpret_cast<const T1 *>(vsapi->getReadPtr(scp, plane)) + dstStride * field_n;

                vCheck<T1>(_srcp, scpp, _dstp, _dmap, tline, field_n, dstWidth, srcHeight, srcStride, dstStride, d->vcheck, d->vthresh2, d->rcpVthresh0, d->rcpVthresh1, d->rcpVthresh2, d->peak);
            }
        }
    }
}

static void selectFunctions(const unsigned opt, EEDI3Data * d) noexcept {
    d->vectorSize = 1;
    d->alignment = 1;

#ifdef VS_TARGET_CPU_X86
    const int iset = instrset_detect();

    if ((opt == 0 && iset >= 9) || opt == 5) {
        d->vectorSize = 16;
        d->alignment = 64;
    } else if ((opt == 0 && iset >= 7) || opt == 4) {
        d->vectorSize = 8;
        d->alignment = 32;
    } else if ((opt == 0 && iset >= 2) || opt >= 2) {
        d->vectorSize = 4;
        d->alignment = 16;
    }
#endif

    if (d->vi.format->bytesPerSample == 1) {
        d->processor = process_c<uint8_t, void>;

#ifdef VS_TARGET_CPU_X86
        if ((opt == 0 && iset >= 9) || opt == 5)
            d->processor = process_avx512<uint8_t, int>;
        else if ((opt == 0 && iset >= 7) || opt == 4)
            d->processor = process_avx<uint8_t, float>;
        else if ((opt == 0 && iset >= 5) || opt == 3)
            d->processor = process_sse4<uint8_t, int>;
        else if ((opt == 0 && iset >= 2) || opt == 2)
            d->processor = process_sse2<uint8_t, int>;
#endif
    } else if (d->vi.format->bytesPerSample == 2) {
        d->processor = process_c<uint16_t, void>;

#ifdef VS_TARGET_CPU_X86
        if ((opt == 0 && iset >= 9) || opt == 5)
            d->processor = process_avx512<uint16_t, int>;
        else if ((opt == 0 && iset >= 7) || opt == 4)
            d->processor = process_avx<uint16_t, float>;
        else if ((opt == 0 && iset >= 5) || opt == 3)
            d->processor = process_sse4<uint16_t, int>;
        else if ((opt == 0 && iset >= 2) || opt == 2)
            d->processor = process_sse2<uint16_t, int>;
#endif
    } else {
        d->processor = process_c<float, void>;

#ifdef VS_TARGET_CPU_X86
        if ((opt == 0 && iset >= 9) || opt == 5)
            d->processor = process_avx512<float, float>;
        else if ((opt == 0 && iset >= 7) || opt == 4)
            d->processor = process_avx<float, float>;
        else if ((opt == 0 && iset >= 5) || opt == 3)
            d->processor = process_sse4<float, float>;
        else if ((opt == 0 && iset >= 2) || opt == 2)
            d->processor = process_sse2<float, float>;
#endif
    }
}

static void VS_CC eedi3Init(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    EEDI3Data * d = static_cast<EEDI3Data *>(*instanceData);
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC eedi3GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    EEDI3Data * d = static_cast<EEDI3Data *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(d->field > 1 ? n / 2 : n, d->node, frameCtx);

        if (d->vcheck && d->sclip)
            vsapi->requestFrameFilter(n, d->sclip, frameCtx);
    } else if (activationReason == arAllFramesReady) {
#ifdef VS_TARGET_CPU_X86
        no_subnormals();
#endif

        try {
            auto threadId = std::this_thread::get_id();

            if (!d->srcVector.count(threadId)) {
                if (d->vectorSize != 1) {
                    int * srcVector = vs_aligned_malloc<int>((d->vi.width + 24) * 4 * d->vectorSize * sizeof(int), d->alignment);
                    if (!srcVector)
                        throw std::string{ "malloc failure (srcVector)" };
                    d->srcVector.emplace(threadId, srcVector);
                } else {
                    d->srcVector.emplace(threadId, nullptr);
                }
            }

            if (!d->ccosts.count(threadId)) {
                float * ccosts = vs_aligned_malloc<float>(d->vi.width * d->tpitchVector * sizeof(float), d->alignment);
                if (!ccosts)
                    throw std::string{ "malloc failure (ccosts)" };
                d->ccosts.emplace(threadId, ccosts);
            }

            if (!d->pcosts.count(threadId)) {
                float * pcosts = vs_aligned_malloc<float>(d->vi.width * d->tpitchVector * sizeof(float), d->alignment);
                if (!pcosts)
                    throw std::string{ "malloc failure (pcosts)" };
                d->pcosts.emplace(threadId, pcosts);
            }

            if (!d->pbackt.count(threadId)) {
                int * pbackt = vs_aligned_malloc<int>(d->vi.width * d->tpitchVector * sizeof(int), d->alignment);
                if (!pbackt)
                    throw std::string{ "malloc failure (pbackt)" };
                d->pbackt.emplace(threadId, pbackt);
            }

            if (!d->fpath.count(threadId)) {
                int * fpath = new (std::nothrow) int[d->vi.width];
                if (!fpath)
                    throw std::string{ "malloc failure (fpath)" };
                d->fpath.emplace(threadId, fpath);
            }

            if (!d->dmap.count(threadId)) {
                int * dmap = new (std::nothrow) int[d->vi.width * d->vi.height];
                if (!dmap)
                    throw std::string{ "malloc failure (dmap)" };
                d->dmap.emplace(threadId, dmap);
            }

            if (!d->tline.count(threadId)) {
                if (d->vcheck) {
                    float * tline = new (std::nothrow) float[d->vi.width];
                    if (!tline)
                        throw std::string{ "malloc failure (tline)" };
                    d->tline.emplace(threadId, tline);
                } else {
                    d->tline.emplace(threadId, nullptr);
                }
            }
        } catch (const std::string & error) {
            vsapi->setFilterError(("EEDI3: " + error).c_str(), frameCtx);
            return nullptr;
        }

        const VSFrameRef * src = vsapi->getFrameFilter(d->field > 1 ? n / 2 : n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi.format, d->vi.width, d->vi.height, fr, pl, src, core);

        const VSFrameRef * scp = nullptr;
        if (d->vcheck && d->sclip)
            scp = vsapi->getFrameFilter(n, d->sclip, frameCtx);

        VSFrameRef * pad[3] = {};
        for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
            if (d->process[plane])
                pad[plane] = vsapi->newVideoFrame(vsapi->registerFormat(cmGray, d->vi.format->sampleType, d->vi.format->bitsPerSample, 0, 0, core),
                                                  vsapi->getFrameWidth(dst, plane) + 24,
                                                  vsapi->getFrameHeight(dst, plane) + 8,
                                                  nullptr, core);
        }

        int field = d->field;
        if (field > 1)
            field -= 2;

        int err;
        const int fieldBased = int64ToIntS(vsapi->propGetInt(vsapi->getFramePropsRO(src), "_FieldBased", 0, &err));
        if (fieldBased == 1)
            field = 0;
        else if (fieldBased == 2)
            field = 1;

        int field_n;
        if (d->field > 1) {
            if (n & 1)
                field_n = (field == 0);
            else
                field_n = (field == 1);
        } else {
            field_n = field;
        }

        d->processor(src, scp, dst, pad, field_n, d, vsapi);

        VSMap * props = vsapi->getFramePropsRW(dst);
        vsapi->propSetInt(props, "_FieldBased", 0, paReplace);

        if (d->field > 1) {
            int errNum, errDen;
            int64_t durationNum = vsapi->propGetInt(props, "_DurationNum", 0, &errNum);
            int64_t durationDen = vsapi->propGetInt(props, "_DurationDen", 0, &errDen);
            if (!errNum && !errDen) {
                muldivRational(&durationNum, &durationDen, 1, 2);
                vsapi->propSetInt(props, "_DurationNum", durationNum, paReplace);
                vsapi->propSetInt(props, "_DurationDen", durationDen, paReplace);
            }
        }

        vsapi->freeFrame(src);
        vsapi->freeFrame(scp);
        for (int plane = 0; plane < d->vi.format->numPlanes; plane++)
            vsapi->freeFrame(pad[plane]);

        return dst;
    }

    return nullptr;
}

static void VS_CC eedi3Free(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    EEDI3Data * d = static_cast<EEDI3Data *>(instanceData);

    vsapi->freeNode(d->node);
    vsapi->freeNode(d->sclip);

    for (auto & iter : d->srcVector)
        vs_aligned_free(iter.second);

    for (auto & iter : d->ccosts)
        vs_aligned_free(iter.second);

    for (auto & iter : d->pcosts)
        vs_aligned_free(iter.second);

    for (auto & iter : d->pbackt)
        vs_aligned_free(iter.second);

    for (auto & iter : d->fpath)
        delete[] iter.second;

    for (auto & iter : d->dmap)
        delete[] iter.second;

    for (auto & iter : d->tline)
        delete[] iter.second;

    delete d;
}

void VS_CC eedi3Create(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<EEDI3Data> d{ new EEDI3Data{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->sclip = vsapi->propGetNode(in, "sclip", 0, &err);
    d->vi = *vsapi->getVideoInfo(d->node);

    try {
        if (!isConstantFormat(&d->vi) || (d->vi.format->sampleType == stInteger && d->vi.format->bitsPerSample > 16) ||
            (d->vi.format->sampleType == stFloat && d->vi.format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bits integer and 32 bits float input supported" };

        d->field = int64ToIntS(vsapi->propGetInt(in, "field", 0, nullptr));

        d->dh = !!vsapi->propGetInt(in, "dh", 0, &err);

        const int m = vsapi->propNumElements(in, "planes");

        for (int i = 0; i < 3; i++)
            d->process[i] = m <= 0;

        for (int i = 0; i < m; i++) {
            const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

            if (n < 0 || n >= d->vi.format->numPlanes)
                throw std::string{ "plane index out of range" };

            if (d->process[n])
                throw std::string{ "plane specified twice" };

            d->process[n] = true;
        }

        d->alpha = static_cast<float>(vsapi->propGetFloat(in, "alpha", 0, &err));
        if (err)
            d->alpha = 0.2f;

        d->beta = static_cast<float>(vsapi->propGetFloat(in, "beta", 0, &err));
        if (err)
            d->beta = 0.25f;

        d->gamma = static_cast<float>(vsapi->propGetFloat(in, "gamma", 0, &err));
        if (err)
            d->gamma = 20.f;

        d->nrad = int64ToIntS(vsapi->propGetInt(in, "nrad", 0, &err));
        if (err)
            d->nrad = 2;

        d->mdis = int64ToIntS(vsapi->propGetInt(in, "mdis", 0, &err));
        if (err)
            d->mdis = 20;

        d->ucubic = !!vsapi->propGetInt(in, "ucubic", 0, &err);
        if (err)
            d->ucubic = true;

        d->cost3 = !!vsapi->propGetInt(in, "cost3", 0, &err);
        if (err)
            d->cost3 = true;

        d->vcheck = int64ToIntS(vsapi->propGetInt(in, "vcheck", 0, &err));
        if (err)
            d->vcheck = 2;

        float vthresh0 = static_cast<float>(vsapi->propGetFloat(in, "vthresh0", 0, &err));
        if (err)
            vthresh0 = 32.f;

        float vthresh1 = static_cast<float>(vsapi->propGetFloat(in, "vthresh1", 0, &err));
        if (err)
            vthresh1 = 64.f;

        d->vthresh2 = static_cast<float>(vsapi->propGetFloat(in, "vthresh2", 0, &err));
        if (err)
            d->vthresh2 = 4.f;

        const int opt = int64ToIntS(vsapi->propGetInt(in, "opt", 0, &err));

        if (d->field < 0 || d->field > 3)
            throw std::string{ "field must be 0, 1, 2 or 3" };

        if (!d->dh && (d->vi.height & 1))
            throw std::string{ "height must be mod 2 when dh=False" };

        if (d->dh && d->field > 1)
            throw std::string{ "field must be 0 or 1 when dh=True" };

        if (d->alpha < 0.f || d->alpha > 1.f)
            throw std::string{ "alpha must be between 0.0 and 1.0 (inclusive)" };

        if (d->beta < 0.f || d->beta > 1.f)
            throw std::string{ "beta must be between 0.0 and 1.0 (inclusive)" };

        if (d->alpha + d->beta > 1.f)
            throw std::string{ "alpha+beta must be between 0.0 and 1.0 (inclusive)" };

        if (d->gamma < 0.f)
            throw std::string{ "gamma must be greater than or equal to 0.0" };

        if (d->nrad < 0 || d->nrad > 3)
            throw std::string{ "nrad must be between 0 and 3 (inclusive)" };

        if (d->mdis < 1 || d->mdis > 40)
            throw std::string{ "mdis must be between 1 and 40 (inclusive)" };

        if (d->vcheck < 0 || d->vcheck > 3)
            throw std::string{ "vcheck must be 0, 1, 2 or 3" };

        if (d->vcheck && (vthresh0 <= 0.f || vthresh1 <= 0.f || d->vthresh2 <= 0.f))
            throw std::string{ "vthresh0, vthresh1 and vthresh2 must be greater than 0.0" };

        if (opt < 0 || opt > 5)
            throw std::string{ "opt must be 0, 1, 2, 3, 4 or 5" };

        if (d->field > 1) {
            if (d->vi.numFrames > INT_MAX / 2)
                throw std::string{ "resulting clip is too long" };
            d->vi.numFrames *= 2;

            muldivRational(&d->vi.fpsNum, &d->vi.fpsDen, 2, 1);
        }

        if (d->dh)
            d->vi.height *= 2;

        d->remainingWeight = 1.f - d->alpha - d->beta;

        if (d->cost3)
            d->alpha /= 3.f;

        if (d->vcheck && d->sclip) {
            if (!isSameFormat(vsapi->getVideoInfo(d->sclip), &d->vi))
                throw std::string{ "sclip must have the same dimensions as main clip and be the same format" };

            if (vsapi->getVideoInfo(d->sclip)->numFrames != d->vi.numFrames)
                throw std::string{ "sclip's number of frames doesn't match" };
        }

        const unsigned numThreads = vsapi->getCoreInfo(core)->numThreads;
        d->srcVector.reserve(numThreads);
        d->ccosts.reserve(numThreads);
        d->pcosts.reserve(numThreads);
        d->pbackt.reserve(numThreads);
        d->fpath.reserve(numThreads);
        d->dmap.reserve(numThreads);
        d->tline.reserve(numThreads);

        selectFunctions(opt, d.get());

        if (d->vi.format->sampleType == stInteger) {
            d->peak = (1 << d->vi.format->bitsPerSample) - 1;
            const float scale = d->peak / 255.f;
            d->beta *= scale;
            d->gamma *= scale;
            vthresh0 *= scale;
            vthresh1 *= scale;
        } else {
            d->beta /= 255.f;
            d->gamma /= 255.f;
            vthresh0 /= 255.f;
            vthresh1 /= 255.f;
        }

        d->tpitch = d->mdis * 2 + 1;
        d->mdisVector = d->mdis * d->vectorSize;
        d->tpitchVector = d->tpitch * d->vectorSize;

        d->rcpVthresh0 = 1.f / vthresh0;
        d->rcpVthresh1 = 1.f / vthresh1;
        d->rcpVthresh2 = 1.f / d->vthresh2;
    } catch (const std::string & error) {
        vsapi->setError(out, ("EEDI3: " + error).c_str());
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->sclip);
        return;
    }

    vsapi->createFilter(in, out, "EEDI3", eedi3Init, eedi3GetFrame, eedi3Free, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

#ifdef HAVE_OPENCL
extern void VS_CC eedi3clCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
#endif

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.eedi3", "eedi3m", "An intra-field only deinterlacer", VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("EEDI3",
                 "clip:clip;"
                 "field:int;"
                 "dh:int:opt;"
                 "planes:int[]:opt;"
                 "alpha:float:opt;"
                 "beta:float:opt;"
                 "gamma:float:opt;"
                 "nrad:int:opt;"
                 "mdis:int:opt;"
                 "hp:int:opt;"
                 "ucubic:int:opt;"
                 "cost3:int:opt;"
                 "vcheck:int:opt;"
                 "vthresh0:float:opt;"
                 "vthresh1:float:opt;"
                 "vthresh2:float:opt;"
                 "sclip:clip:opt;"
                 "opt:int:opt;",
                 eedi3Create, nullptr, plugin);

#ifdef HAVE_OPENCL
    registerFunc("EEDI3CL",
                 "clip:clip;"
                 "field:int;"
                 "dh:int:opt;"
                 "planes:int[]:opt;"
                 "alpha:float:opt;"
                 "beta:float:opt;"
                 "gamma:float:opt;"
                 "nrad:int:opt;"
                 "mdis:int:opt;"
                 "hp:int:opt;"
                 "ucubic:int:opt;"
                 "cost3:int:opt;"
                 "vcheck:int:opt;"
                 "vthresh0:float:opt;"
                 "vthresh1:float:opt;"
                 "vthresh2:float:opt;"
                 "sclip:clip:opt;"
                 "opt:int:opt;"
                 "device:int:opt;"
                 "list_device:int:opt;"
                 "info:int:opt;",
                 eedi3clCreate, nullptr, plugin);
#endif
}
