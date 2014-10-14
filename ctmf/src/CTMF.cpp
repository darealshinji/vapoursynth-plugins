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
#include "VapourSynth.h"
#include "VSHelper.h"

#ifdef _MSC_VER
#define ALIGN __declspec(align(32))
#else
#define ALIGN __attribute__((aligned(32)))
#endif

struct CTMFData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    int radius, memsize;
    int num;
    bool process[3];
};

template<int num>
struct ALIGN Histogram{
    uint16_t coarse[num];
    uint16_t fine[num][num];
};

template<int num>
static inline void addHist(const uint16_t * const VS_RESTRICT x, uint16_t * const VS_RESTRICT y) {
    for (int i = 0; i < num; i++)
        y[i] += x[i];
}

template<int num>
static inline void subHist(const uint16_t * const VS_RESTRICT x, uint16_t * const VS_RESTRICT y) {
    for (int i = 0; i < num; i++)
        y[i] -= x[i];
}

template<int num>
static inline void muladdHist(const uint16_t a, const uint16_t * const VS_RESTRICT x, uint16_t * const VS_RESTRICT y) {
    for (int i = 0; i < num; i++)
        y[i] += a * x[i];
}

template<typename T, int num>
static void CTMF(const T * const VS_RESTRICT srcp, T * const VS_RESTRICT dstp, const int width, const int height, const int stride, const CTMFData * const d,
                 const bool padLeft, const bool padRight, uint16_t * const VS_RESTRICT hCoarse, uint16_t * const VS_RESTRICT hFine) {
    const int shift = d->vi->format->bitsPerSample == 9 ? 1 : 0;
    const int modulo = d->vi->format->bitsPerSample == 9 ? 31 : (1 << (d->vi->format->bitsPerSample / 2)) - 1;
    const T * p, * q;

    Histogram<num> H;
    uint16_t luc[num];
    memset(hCoarse, 0, num * width * sizeof(uint16_t));
    memset(hFine, 0, num * num * width * sizeof(uint16_t));

    // First row initialization
    for (int j = 0; j < width; j++) {
        hCoarse[num * j + ((srcp[j] << shift) / num)] += d->radius + 1;
        hFine[num * (width * ((srcp[j] << shift) / num) + j) + ((srcp[j] << shift) & modulo)] += d->radius + 1;
    }
    for (int i = 0; i < d->radius; i++) {
        for (int j = 0; j < width; j++) {
            hCoarse[num * j + ((srcp[stride * i + j] << shift) / num)]++;
            hFine[num * (width * ((srcp[stride * i + j] << shift) / num) + j) + ((srcp[stride * i + j] << shift) & modulo)]++;
        }
    }

    for (int i = 0; i < height; i++) {
        // Update column histograms for entire row
        p = srcp + stride * VSMAX(0, i - d->radius - 1);
        q = p + width;
        for (int j = 0; p != q; j++) {
            hCoarse[num * j + ((*p << shift) / num)]--;
            hFine[num * (width * ((*p << shift) / num) + j) + ((*p << shift) & modulo)]--;
            p++;
        }

        p = srcp + stride * VSMIN(height - 1, i + d->radius);
        q = p + width;
        for (int j = 0; p != q; ++j) {
            hCoarse[num * j + ((*p << shift) / num)]++;
            hFine[num * (width * ((*p << shift) / num) + j) + ((*p << shift) & modulo)]++;
            p++;
        }

        // First column initialization
        memset(&H, 0, sizeof(H));
        memset(luc, 0, sizeof(luc));
        if (padLeft)
            muladdHist<num>(d->radius, &hCoarse[0], H.coarse);
        for (int j = 0; j < (padLeft ? d->radius : 2 * d->radius); j++)
            addHist<num>(&hCoarse[num * j], H.coarse);
        for (int k = 0; k < num; ++k)
            muladdHist<num>(2 * d->radius + 1, &hFine[num * width * k], &H.fine[k][0]);

        for (int j = padLeft ? 0 : d->radius; j < (padRight ? width : width - d->radius); j++) {
            const uint16_t t = 2 * d->radius * d->radius + 2 * d->radius;
            uint16_t sum = 0, * segment;
            int k;

            addHist<num>(&hCoarse[num * VSMIN(j + d->radius, width - 1)], H.coarse);

            // Find median at coarse level
            for (k = 0; k < num; k++) {
                sum += H.coarse[k];
                if (sum > t) {
                    sum -= H.coarse[k];
                    break;
                }
            }

            // Update corresponding histogram segment
            if (luc[k] <= j - d->radius) {
                memset(&H.fine[k], 0, num * sizeof(uint16_t));
                for (luc[k] = j - d->radius; luc[k] < VSMIN(j + d->radius + 1, width); ++luc[k])
                    addHist<num>(&hFine[num * (width * k + luc[k])], H.fine[k]);
                if (luc[k] < j + d->radius + 1) {
                    muladdHist<num>(j + d->radius + 1 - width, &hFine[num * (width * k + (width - 1))], &H.fine[k][0]);
                    luc[k] = j + d->radius + 1;
                }
            } else {
                for (; luc[k] < j + d->radius + 1; ++luc[k]) {
                    subHist<num>(&hFine[num * (width * k + VSMAX(luc[k] - 2 * d->radius - 1, 0))], H.fine[k]);
                    addHist<num>(&hFine[num * (width * k + VSMIN(luc[k], width - 1))], H.fine[k]);
                }
            }

            subHist<num>(&hCoarse[num * VSMAX(j - d->radius, 0)], H.coarse);

            // Find median in segment
            segment = H.fine[k];
            for (int b = 0; b < num; b++) {
                sum += segment[b];
                if (sum > t) {
                    dstp[stride * i + j] = (num * k + b) >> shift;
                    break;
                }
            }
        }
    }
}

static void VS_CC ctmfInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    CTMFData * d = (CTMFData *)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC ctmfGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const CTMFData * const d = (const CTMFData *)*instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * const src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * const dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane]) {
                const int width = vsapi->getFrameWidth(src, plane);
                const int height = vsapi->getFrameHeight(src, plane);
                const int stride = vsapi->getStride(src, plane);
                const uint8_t * const srcp = vsapi->getReadPtr(src, plane);
                uint8_t * const dstp = vsapi->getWritePtr(dst, plane);

                uint16_t * const hCoarse = vs_aligned_malloc<uint16_t>(d->num * width * sizeof(uint16_t), 32);
                uint16_t * const hFine = vs_aligned_malloc<uint16_t>(d->num * d->num * width * sizeof(uint16_t), 32);
                if (!hCoarse || !hFine) {
                    vsapi->setFilterError("CTMF: malloc failure (Histogram)", frameCtx);
                    return nullptr;
                }

                if (d->vi->format->bitsPerSample == 8) {
                    const int stripes = (int)std::ceil(double(width - 2 * d->radius) / (d->memsize / sizeof(Histogram<16>) - 2 * d->radius));
                    const int stripeSize = (int)std::ceil(double(width + stripes * 2 * d->radius - 2 * d->radius) / stripes);
                    for (int i = 0; i < width; i += stripeSize - 2 * d->radius) {
                        int stripe = stripeSize;
                        // Make sure that the filter kernel fits into one stripe
                        if (i + stripeSize - 2 * d->radius >= width || width - (i + stripeSize - 2 * d->radius) < 2 * d->radius + 1)
                            stripe = width - i;
                        CTMF<uint8_t, 16>(srcp + i, dstp + i, stripe, height, stride, d, i == 0, stripe == width - i, hCoarse, hFine);
                        if (stripe == width - i)
                            break;
                    }
                } else if (d->vi->format->bitsPerSample == 9 || d->vi->format->bitsPerSample == 10) {
                    const int stripes = (int)std::ceil(double(width - 2 * d->radius) / (d->memsize / sizeof(Histogram<32>) - 2 * d->radius));
                    const int stripeSize = (int)std::ceil(double(width + stripes * 2 * d->radius - 2 * d->radius) / stripes);
                    for (int i = 0; i < width; i += stripeSize - 2 * d->radius) {
                        int stripe = stripeSize;
                        // Make sure that the filter kernel fits into one stripe
                        if (i + stripeSize - 2 * d->radius >= width || width - (i + stripeSize - 2 * d->radius) < 2 * d->radius + 1)
                            stripe = width - i;
                        CTMF<uint16_t, 32>((const uint16_t *)srcp + i, (uint16_t *)dstp + i, stripe, height, stride / 2, d, i == 0, stripe == width - i, hCoarse, hFine);
                        if (stripe == width - i)
                            break;
                    }
                } else if (d->vi->format->bitsPerSample == 12) {
                    const int stripes = (int)std::ceil(double(width - 2 * d->radius) / (d->memsize / sizeof(Histogram<64>) - 2 * d->radius));
                    const int stripeSize = (int)std::ceil(double(width + stripes * 2 * d->radius - 2 * d->radius) / stripes);
                    for (int i = 0; i < width; i += stripeSize - 2 * d->radius) {
                        int stripe = stripeSize;
                        // Make sure that the filter kernel fits into one stripe
                        if (i + stripeSize - 2 * d->radius >= width || width - (i + stripeSize - 2 * d->radius) < 2 * d->radius + 1)
                            stripe = width - i;
                        CTMF<uint16_t, 64>((const uint16_t *)srcp + i, (uint16_t *)dstp + i, stripe, height, stride / 2, d, i == 0, stripe == width - i, hCoarse, hFine);
                        if (stripe == width - i)
                            break;
                    }
                } else {
                    const int stripes = (int)std::ceil(double(width - 2 * d->radius) / (d->memsize / sizeof(Histogram<256>) - 2 * d->radius));
                    const int stripeSize = (int)std::ceil(double(width + stripes * 2 * d->radius - 2 * d->radius) / stripes);
                    for (int i = 0; i < width; i += stripeSize - 2 * d->radius) {
                        int stripe = stripeSize;
                        // Make sure that the filter kernel fits into one stripe
                        if (i + stripeSize - 2 * d->radius >= width || width - (i + stripeSize - 2 * d->radius) < 2 * d->radius + 1)
                            stripe = width - i;
                        CTMF<uint16_t, 256>((const uint16_t *)srcp + i, (uint16_t *)dstp + i, stripe, height, stride / 2, d, i == 0, stripe == width - i, hCoarse, hFine);
                        if (stripe == width - i)
                            break;
                    }
                }

                vs_aligned_free(hCoarse);
                vs_aligned_free(hFine);
            }
        }

        vsapi->freeFrame(src);
        return dst;
    }

    return nullptr;
}

static void VS_CC ctmfFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    CTMFData * d = (CTMFData *)instanceData;
    vsapi->freeNode(d->node);
    delete d;
}

static void VS_CC ctmfCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    CTMFData d;
    CTMFData * data;
    int err;

    d.radius = int64ToIntS(vsapi->propGetInt(in, "radius", 0, &err));
    if (err)
        d.radius = 2;
    d.memsize = int64ToIntS(vsapi->propGetInt(in, "memsize", 0, &err));
    if (err)
        d.memsize = 1048576;

    if (d.radius < 1 || d.radius > 127) {
        vsapi->setError(out, "CTMF: radius must be 1 <= x <= 127");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bytesPerSample > 2) {
        vsapi->setError(out, "CTMF: only constant format 8-16 bits integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    d.num = d.vi->format->bitsPerSample == 9 ? 32 : 1 << (d.vi->format->bitsPerSample / 2);

    const int m = vsapi->propNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        d.process[i] = m <= 0;

    for (int i = 0; i < m; i++) {
        const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (n < 0 || n >= d.vi->format->numPlanes) {
            vsapi->setError(out, "CTMF: plane index out of range");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.process[n]) {
            vsapi->setError(out, "CTMF: plane specified twice");
            vsapi->freeNode(d.node);
            return;
        }

        d.process[n] = true;
    }

    for (int plane = 0; plane < d.vi->format->numPlanes; plane++) {
        if (d.process[plane]) {
            const int width = d.vi->width >> (plane ? d.vi->format->subSamplingW : 0);
            const int height = d.vi->height >> (plane ? d.vi->format->subSamplingH : 0);

            if (width < 2 * d.radius + 1) {
                vsapi->setError(out, "CTMF: width must be >= 2*radius+1");
                vsapi->freeNode(d.node);
                return;
            }

            if (height < 2 * d.radius + 1) {
                vsapi->setError(out, "CTMF: height must be >= 2*radius+1");
                vsapi->freeNode(d.node);
                return;
            }
        }
    }

    data = new CTMFData;
    *data = d;

    vsapi->createFilter(in, out, "CTMF", ctmfInit, ctmfGetFrame, ctmfFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.ctmf", "ctmf", "Constant Time Median Filtering", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("CTMF", "clip:clip;radius:int:opt;memsize:int:opt;planes:int[]:opt;", ctmfCreate, nullptr, plugin);
}
