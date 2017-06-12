/*
 * VapourSynth port by HolyWu
 *
 * ctmf.c - Constant-time median filtering
 * Copyright (C) 2006  Simon Perreault
 *
 * Reference: S. Perreault and P. Hébert, "Median Filtering in Constant Time",
 * IEEE Transactions on Image Processing, September 2007.
 *
 * This program has been obtained from http://nomis80.org/ctmf.html. No patent
 * covers this program, although it is subject to the following license:
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:
 *  Laboratoire de vision et systèmes numériques
 *  Pavillon Adrien-Pouliot
 *  Université Laval
 *  Sainte-Foy, Québec, Canada
 *  G1K 7P4
 *
 *  perreaul@gel.ulaval.ca
 */

#include <cmath>
#include <memory>
#include <string>

#include "CTMF.hpp"

#ifdef VS_TARGET_CPU_X86
template<typename T, uint16_t bins> extern void process_sse2(const T *, T *, uint16_t *, uint16_t *, const CTMFData *, const int, const int, const int, const bool, const bool) noexcept;
template<typename T, uint16_t bins> extern void process_avx2(const T *, T *, uint16_t *, uint16_t *, const CTMFData *, const int, const int, const int, const bool, const bool) noexcept;

template<typename T1, typename T2, uint8_t step> extern void processRadius2_sse2(const VSFrameRef *, VSFrameRef *, const int, const uint8_t, const VSAPI *) noexcept;
template<typename T1, typename T2, uint8_t step> extern void processRadius2_avx2(const VSFrameRef *, VSFrameRef *, const int, const uint8_t, const VSAPI *) noexcept;
#endif

template<typename T, uint16_t bins> static void (*process)(const T *, T *, uint16_t *, uint16_t *, const CTMFData *, const int, const int, const int, const bool, const bool) = nullptr;
template<typename T1, typename T2 = void, uint8_t step = 0> static void (*processRadius2)(const VSFrameRef *, VSFrameRef *, const int, const uint8_t, const VSAPI *) = nullptr;

template<typename T>
static void copyPad(const VSFrameRef * src, VSFrameRef * dst, const int plane, const uint8_t widthPad, const VSAPI * vsapi) noexcept {
    const unsigned width = vsapi->getFrameWidth(src, plane);
    const unsigned height = vsapi->getFrameHeight(src, plane);
    const unsigned stride = vsapi->getStride(dst, 0) / sizeof(T);
    const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 0)) + stride * 2 + widthPad;

    vs_bitblt(dstp, vsapi->getStride(dst, 0), srcp, vsapi->getStride(src, plane), width * sizeof(T), height);

    for (unsigned y = 0; y < height; y++) {
        dstp[-1] = dstp[-2] = 0;
        dstp[width] = dstp[width + 1] = 0;

        dstp += stride;
    }

    dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 0));
    memset(dstp, 0, vsapi->getStride(dst, 0));
    memset(dstp + stride, 0, vsapi->getStride(dst, 0));
    dstp += stride * (vsapi->getFrameHeight(dst, 0) - 2);
    memset(dstp, 0, vsapi->getStride(dst, 0));
    memset(dstp + stride, 0, vsapi->getStride(dst, 0));
}

static inline void histogramAdd_c(const uint16_t * x, uint16_t * VS_RESTRICT y, const uint16_t bins) noexcept {
    for (uint16_t i = 0; i < bins; i++)
        y[i] += x[i];
}

static inline void histogramSub_c(const uint16_t * x, uint16_t * VS_RESTRICT y, const uint16_t bins) noexcept {
    for (uint16_t i = 0; i < bins; i++)
        y[i] -= x[i];
}

static inline void histogramMulAdd_c(const uint16_t a, const uint16_t * x, uint16_t * VS_RESTRICT y, const uint16_t bins) noexcept {
    for (uint16_t i = 0; i < bins; i++)
        y[i] += a * x[i];
}

template<typename T, uint16_t bins>
static void process_c(const T * srcp, T * VS_RESTRICT dstp, uint16_t * VS_RESTRICT hCoarse, uint16_t * VS_RESTRICT hFine, const CTMFData * d,
                      const int width, const int height, const int stride, const bool padLeft, const bool padRight) noexcept {
    const T * p, * q;

    Histogram<bins> H;
    uint16_t luc[bins];

    memset(hCoarse, 0, bins * width * sizeof(uint16_t));
    memset(hFine, 0, bins * bins * width * sizeof(uint16_t));

    // First row initialization
    for (int j = 0; j < width; j++) {
        hCoarse[bins * j + (srcp[j] >> d->shiftRight)] += d->radius + 1;
        hFine[bins * (width * (srcp[j] >> d->shiftRight) + j) + (srcp[j] & d->mask)] += d->radius + 1;
    }
    for (int i = 0; i < d->radius; i++) {
        for (int j = 0; j < width; j++) {
            hCoarse[bins * j + (srcp[stride * i + j] >> d->shiftRight)]++;
            hFine[bins * (width * (srcp[stride * i + j] >> d->shiftRight) + j) + (srcp[stride * i + j] & d->mask)]++;
        }
    }

    for (int i = 0; i < height; i++) {
        // Update column histograms for entire row
        p = srcp + stride * std::max(0, i - d->radius - 1);
        q = p + width;
        for (int j = 0; p != q; j++) {
            hCoarse[bins * j + (*p >> d->shiftRight)]--;
            hFine[bins * (width * (*p >> d->shiftRight) + j) + (*p & d->mask)]--;
            p++;
        }

        p = srcp + stride * std::min(height - 1, i + d->radius);
        q = p + width;
        for (int j = 0; p != q; j++) {
            hCoarse[bins * j + (*p >> d->shiftRight)]++;
            hFine[bins * (width * (*p >> d->shiftRight) + j) + (*p & d->mask)]++;
            p++;
        }

        // First column initialization
        memset(&H, 0, sizeof(H));
        memset(luc, 0, sizeof(luc));
        if (padLeft)
            histogramMulAdd_c(d->radius, &hCoarse[0], H.coarse, bins);
        for (int j = 0; j < (padLeft ? d->radius : 2 * d->radius); j++)
            histogramAdd_c(&hCoarse[bins * j], H.coarse, bins);
        for (int k = 0; k < bins; k++)
            histogramMulAdd_c(2 * d->radius + 1, &hFine[bins * width * k], &H.fine[k][0], bins);

        for (int j = padLeft ? 0 : d->radius; j < (padRight ? width : width - d->radius); j++) {
            uint16_t sum = 0, * segment;
            int k, b;

            histogramAdd_c(&hCoarse[bins * std::min(j + d->radius, width - 1)], H.coarse, bins);

            // Find median at coarse level
            for (k = 0; k < bins; k++) {
                sum += H.coarse[k];
                if (sum > d->t) {
                    sum -= H.coarse[k];
                    break;
                }
            }
            assert(k < bins);

            // Update corresponding histogram segment
            if (luc[k] <= j - d->radius) {
                memset(&H.fine[k], 0, bins * sizeof(uint16_t));
                for (luc[k] = j - d->radius; luc[k] < std::min(j + d->radius + 1, width); luc[k]++)
                    histogramAdd_c(&hFine[bins * (width * k + luc[k])], H.fine[k], bins);
                if (luc[k] < j + d->radius + 1) {
                    histogramMulAdd_c(j + d->radius + 1 - width, &hFine[bins * (width * k + width - 1)], &H.fine[k][0], bins);
                    luc[k] = j + d->radius + 1;
                }
            } else {
                for (; luc[k] < j + d->radius + 1; luc[k]++) {
                    histogramSub_c(&hFine[bins * (width * k + std::max(luc[k] - 2 * d->radius - 1, 0))], H.fine[k], bins);
                    histogramAdd_c(&hFine[bins * (width * k + std::min(static_cast<int>(luc[k]), width - 1))], H.fine[k], bins);
                }
            }

            histogramSub_c(&hCoarse[bins * std::max(j - d->radius, 0)], H.coarse, bins);

            // Find median in segment
            segment = H.fine[k];
            for (b = 0; b < bins; b++) {
                sum += segment[b];
                if (sum > d->t) {
                    dstp[stride * i + j] = bins * k + b;
                    break;
                }
            }
            assert(b < bins);
        }
    }
}

static void selectFunctions(const unsigned opt, CTMFData * d) noexcept {
    process<uint8_t, 16> = process_c<uint8_t, 16>;
    process<uint16_t, 32> = process_c<uint16_t, 32>;
    process<uint16_t, 64> = process_c<uint16_t, 64>;
    process<uint16_t, 128> = process_c<uint16_t, 128>;
    process<uint16_t, 256> = process_c<uint16_t, 256>;

#ifdef VS_TARGET_CPU_X86
    const int iset = instrset_detect();
    if ((opt == 0 && iset >= 8) || opt == 3) {
        if (d->radius == 2) {
            d->specialRadius2 = true;
            processRadius2<uint8_t> = processRadius2_avx2<uint8_t, Vec32uc, 32>;
            processRadius2<uint16_t> = processRadius2_avx2<uint16_t, Vec16us, 16>;
        } else {
            process<uint8_t, 16> = process_avx2<uint8_t, 16>;
            process<uint16_t, 32> = process_avx2<uint16_t, 32>;
            process<uint16_t, 64> = process_avx2<uint16_t, 64>;
            process<uint16_t, 128> = process_avx2<uint16_t, 128>;
            process<uint16_t, 256> = process_avx2<uint16_t, 256>;
        }
    } else if ((opt == 0 && iset >= 2) || opt == 2) {
        if (d->radius == 2) {
            d->specialRadius2 = true;
            processRadius2<uint8_t> = processRadius2_sse2<uint8_t, Vec16uc, 16>;
            processRadius2<uint16_t> = processRadius2_sse2<uint16_t, Vec8us, 8>;
        } else {
            process<uint8_t, 16> = process_sse2<uint8_t, 16>;
            process<uint16_t, 32> = process_sse2<uint16_t, 32>;
            process<uint16_t, 64> = process_sse2<uint16_t, 64>;
            process<uint16_t, 128> = process_sse2<uint16_t, 128>;
            process<uint16_t, 256> = process_sse2<uint16_t, 256>;
        }
    }
#endif
}

static void VS_CC ctmfInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    CTMFData * d = static_cast<CTMFData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC ctmfGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    CTMFData * d = static_cast<CTMFData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[]{ d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[]{ 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        VSFrameRef * pad = nullptr;
        if (d->specialRadius2)
            pad = vsapi->newVideoFrame(vsapi->registerFormat(cmGray, stInteger, d->vi->format->bitsPerSample, 0, 0, core),
                                       d->vi->width + d->widthPad * 2, d->vi->height + 4, nullptr, core);

        try {
            auto threadId = std::this_thread::get_id();

            if (!d->hCoarse.count(threadId)) {
                if (!d->specialRadius2) {
                    uint16_t * hCoarse = vs_aligned_malloc<uint16_t>(d->bins * d->vi->width * sizeof(uint16_t), 32);
                    if (!hCoarse)
                        throw std::string{ "malloc failure (hCoarse)" };
                    d->hCoarse.emplace(threadId, hCoarse);
                } else {
                    d->hCoarse.emplace(threadId, nullptr);
                }
            }

            if (!d->hFine.count(threadId)) {
                if (!d->specialRadius2) {
                    uint16_t * hFine = vs_aligned_malloc<uint16_t>(d->bins * d->bins * d->vi->width * sizeof(uint16_t), 32);
                    if (!hFine)
                        throw std::string{ "malloc failure (hFine)" };
                    d->hFine.emplace(threadId, hFine);
                } else {
                    d->hFine.emplace(threadId, nullptr);
                }
            }

            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (d->process[plane]) {
                    if (d->specialRadius2) {
                        if (d->vi->format->bytesPerSample == 1) {
                            copyPad<uint8_t>(src, pad, plane, d->widthPad, vsapi);
                            processRadius2<uint8_t>(pad, dst, plane, d->widthPad, vsapi);
                        } else {
                            copyPad<uint16_t>(src, pad, plane, d->widthPad, vsapi);
                            processRadius2<uint16_t>(pad, dst, plane, d->widthPad, vsapi);
                        }
                    } else {
                        const int width = vsapi->getFrameWidth(src, plane);
                        const int height = vsapi->getFrameHeight(src, plane);
                        const int stride = vsapi->getStride(src, plane);
                        const uint8_t * srcp = vsapi->getReadPtr(src, plane);
                        uint8_t * dstp = vsapi->getWritePtr(dst, plane);

                        if (d->vi->format->bitsPerSample == 8) {
                            for (int i = 0; i < width; i += d->stripeSize[plane] - 2 * d->radius) {
                                int stripe = d->stripeSize[plane];
                                // Make sure that the filter kernel fits into one stripe
                                if (i + d->stripeSize[plane] - 2 * d->radius >= width || width - (i + d->stripeSize[plane] - 2 * d->radius) < 2 * d->radius + 1)
                                    stripe = width - i;

                                process<uint8_t, 16>(srcp + i, dstp + i, d->hCoarse.at(threadId), d->hFine.at(threadId), d, stripe, height, stride, i == 0, stripe == width - i);

                                if (stripe == width - i)
                                    break;
                            }
                        } else if (d->vi->format->bitsPerSample == 10) {
                            for (int i = 0; i < width; i += d->stripeSize[plane] - 2 * d->radius) {
                                int stripe = d->stripeSize[plane];
                                // Make sure that the filter kernel fits into one stripe
                                if (i + d->stripeSize[plane] - 2 * d->radius >= width || width - (i + d->stripeSize[plane] - 2 * d->radius) < 2 * d->radius + 1)
                                    stripe = width - i;

                                process<uint16_t, 32>(reinterpret_cast<const uint16_t *>(srcp) + i, reinterpret_cast<uint16_t *>(dstp) + i,
                                                      d->hCoarse.at(threadId), d->hFine.at(threadId), d, stripe, height, stride / 2, i == 0, stripe == width - i);

                                if (stripe == width - i)
                                    break;
                            }
                        } else if (d->vi->format->bitsPerSample == 12) {
                            for (int i = 0; i < width; i += d->stripeSize[plane] - 2 * d->radius) {
                                int stripe = d->stripeSize[plane];
                                // Make sure that the filter kernel fits into one stripe
                                if (i + d->stripeSize[plane] - 2 * d->radius >= width || width - (i + d->stripeSize[plane] - 2 * d->radius) < 2 * d->radius + 1)
                                    stripe = width - i;

                                process<uint16_t, 64>(reinterpret_cast<const uint16_t *>(srcp) + i, reinterpret_cast<uint16_t *>(dstp) + i,
                                                      d->hCoarse.at(threadId), d->hFine.at(threadId), d, stripe, height, stride / 2, i == 0, stripe == width - i);

                                if (stripe == width - i)
                                    break;
                            }
                        } else if (d->vi->format->bitsPerSample == 14) {
                            for (int i = 0; i < width; i += d->stripeSize[plane] - 2 * d->radius) {
                                int stripe = d->stripeSize[plane];
                                // Make sure that the filter kernel fits into one stripe
                                if (i + d->stripeSize[plane] - 2 * d->radius >= width || width - (i + d->stripeSize[plane] - 2 * d->radius) < 2 * d->radius + 1)
                                    stripe = width - i;

                                process<uint16_t, 128>(reinterpret_cast<const uint16_t *>(srcp) + i, reinterpret_cast<uint16_t *>(dstp) + i,
                                                       d->hCoarse.at(threadId), d->hFine.at(threadId), d, stripe, height, stride / 2, i == 0, stripe == width - i);

                                if (stripe == width - i)
                                    break;
                            }
                        } else {
                            for (int i = 0; i < width; i += d->stripeSize[plane] - 2 * d->radius) {
                                int stripe = d->stripeSize[plane];
                                // Make sure that the filter kernel fits into one stripe
                                if (i + d->stripeSize[plane] - 2 * d->radius >= width || width - (i + d->stripeSize[plane] - 2 * d->radius) < 2 * d->radius + 1)
                                    stripe = width - i;

                                process<uint16_t, 256>(reinterpret_cast<const uint16_t *>(srcp) + i, reinterpret_cast<uint16_t *>(dstp) + i,
                                                       d->hCoarse.at(threadId), d->hFine.at(threadId), d, stripe, height, stride / 2, i == 0, stripe == width - i);

                                if (stripe == width - i)
                                    break;
                            }
                        }
                    }
                }
            }
        } catch (const std::string & error) {
            vsapi->setFilterError(("CTMF: " + error).c_str(), frameCtx);
            vsapi->freeFrame(src);
            vsapi->freeFrame(pad);
            vsapi->freeFrame(dst);
            return nullptr;
        }

        vsapi->freeFrame(src);
        vsapi->freeFrame(pad);
        return dst;
    }

    return nullptr;
}

static void VS_CC ctmfFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    CTMFData * d = static_cast<CTMFData *>(instanceData);

    vsapi->freeNode(d->node);

    for (auto & iter : d->hCoarse)
        vs_aligned_free(iter.second);

    for (auto & iter : d->hFine)
        vs_aligned_free(iter.second);

    delete d;
}

static void VS_CC ctmfCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<CTMFData> d{ new CTMFData{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    try {
        if (!isConstantFormat(d->vi) || d->vi->format->sampleType != stInteger || d->vi->format->bitsPerSample > 16 || d->vi->format->bitsPerSample & 1)
            throw std::string{ "only constant format 8, 10, 12, 14 or 16 bits integer input supported" };

        d->radius = int64ToIntS(vsapi->propGetInt(in, "radius", 0, &err));
        if (err)
            d->radius = 2;

        int memsize = int64ToIntS(vsapi->propGetInt(in, "memsize", 0, &err));
        if (err)
            memsize = 1048576;

        const int opt = int64ToIntS(vsapi->propGetInt(in, "opt", 0, &err));

        if (d->radius < 1 || d->radius > 127)
            throw std::string{ "radius must be between 1 and 127 (inclusive)" };

        if (memsize < 1024)
            throw std::string{ "memsize must be greater than or equal to 1024" };

        if (opt < 0 || opt > 3)
            throw std::string{ "opt must be 0, 1, 2 or 3" };

        const int m = vsapi->propNumElements(in, "planes");

        for (int i = 0; i < 3; i++)
            d->process[i] = m <= 0;

        for (int i = 0; i < m; i++) {
            const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

            if (n < 0 || n >= d->vi->format->numPlanes)
                throw std::string{ "plane index out of range" };

            if (d->process[n])
                throw std::string{ "plane specified twice" };

            d->process[n] = true;
        }

        selectFunctions(opt, d.get());

        d->widthPad = 32 / d->vi->format->bytesPerSample;

        const unsigned numThreads = vsapi->getCoreInfo(core)->numThreads;
        d->hCoarse.reserve(numThreads);
        d->hFine.reserve(numThreads);

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane]) {
                const int width = d->vi->width >> (plane ? d->vi->format->subSamplingW : 0);
                const int height = d->vi->height >> (plane ? d->vi->format->subSamplingH : 0);

                if (width < 2 * d->radius + 1)
                    throw std::string{ "the plane's width must be greater than or equal to 2*radius+1" };

                if (height < 2 * d->radius + 1)
                    throw std::string{ "the plane's height must be greater than or equal to 2*radius+1" };

                int histogramSize;
                if (d->vi->format->bitsPerSample == 8)
                    histogramSize = sizeof(Histogram<16>);
                else if (d->vi->format->bitsPerSample == 10)
                    histogramSize = sizeof(Histogram<32>);
                else if (d->vi->format->bitsPerSample == 12)
                    histogramSize = sizeof(Histogram<64>);
                else if (d->vi->format->bitsPerSample == 14)
                    histogramSize = sizeof(Histogram<128>);
                else
                    histogramSize = sizeof(Histogram<256>);

                const int stripes = static_cast<int>(std::ceil(static_cast<float>(width - 2 * d->radius) / (memsize / histogramSize - 2 * d->radius)));
                d->stripeSize[plane] = static_cast<int>(std::ceil(static_cast<float>(width + stripes * 2 * d->radius - 2 * d->radius) / stripes));
            }
        }

        d->bins = 1 << (d->vi->format->bitsPerSample / 2);
        d->shiftRight = d->vi->format->bitsPerSample / 2;
        d->mask = d->bins - 1;

        d->t = 2 * d->radius * d->radius + 2 * d->radius;
    } catch (const std::string & error) {
        vsapi->setError(out, ("CTMF: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "CTMF", ctmfInit, ctmfGetFrame, ctmfFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.ctmf", "ctmf", "Constant Time Median Filtering", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("CTMF",
                 "clip:clip;"
                 "radius:int:opt;"
                 "memsize:int:opt;"
                 "opt:int:opt;"
                 "planes:int[]:opt;",
                 ctmfCreate, nullptr, plugin);
}
