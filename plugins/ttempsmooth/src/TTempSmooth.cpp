/*
**   VapourSynth port by HolyWu
**
**                TTempSmooth v0.9.4 for AviSynth 2.5.x
**
**   TTempSmooth is a basic, motion adaptive, temporal smoothing filter.
**   It currently supports YV12 and YUY2 colorspaces.
**
**   Copyright (C) 2004-2005 Kevin Stone
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

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

#include <VapourSynth.h>
#include <VSHelper.h>

struct TTempSmoothData {
    VSNodeRef * node, * pfclip, * propNode;
    const VSVideoInfo * vi;
    int maxr, thresh[3], mdiff[3];
    double scthresh;
    bool fp, process[3];
    int diameter, shift;
    float threshF[3];
    unsigned * weight[3], cw;
    void (*filter[3])(const VSFrameRef *[15], const VSFrameRef *[15], VSFrameRef *, const int, const int, const int, const TTempSmoothData * const VS_RESTRICT, const VSAPI *);
};

template<typename T1, typename T2, bool useDiff>
static void filterI(const VSFrameRef * src[15], const VSFrameRef * pf[15], VSFrameRef * dst, const int fromFrame, const int toFrame, const int plane,
                    const TTempSmoothData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept {
    const int width = vsapi->getFrameWidth(dst, plane);
    const int height = vsapi->getFrameHeight(dst, plane);
    const int stride = vsapi->getStride(dst, plane) / sizeof(T1);
    const T1 * srcp[15] = {}, * pfp[15] = {};
    for (int i = 0; i < d->diameter; i++) {
        srcp[i] = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src[i], plane));
        pfp[i] = reinterpret_cast<const T1 *>(vsapi->getReadPtr(pf[i], plane));
    }
    T1 * VS_RESTRICT dstp = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane));

    const int thresh = d->thresh[plane];
    const unsigned * const weightSaved = d->weight[plane];

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int c = pfp[d->maxr][x];
            T2 weights = d->cw;
            T2 sum = srcp[d->maxr][x] * d->cw;

            int frameIndex = d->maxr - 1;

            if (frameIndex > fromFrame) {
                int t1 = pfp[frameIndex][x];
                int diff = std::abs(c - t1);

                if (diff < thresh) {
                    unsigned weight = weightSaved[useDiff ? diff >> d->shift : frameIndex];
                    weights += weight;
                    sum += srcp[frameIndex][x] * weight;

                    frameIndex--;
                    int v = 256;

                    while (frameIndex > fromFrame) {
                        const int t2 = t1;
                        t1 = pfp[frameIndex][x];
                        diff = std::abs(c - t1);

                        if (diff < thresh && std::abs(t1 - t2) < thresh) {
                            weight = weightSaved[useDiff ? (diff >> d->shift) + v : frameIndex];
                            weights += weight;
                            sum += srcp[frameIndex][x] * weight;

                            frameIndex--;
                            v += 256;
                        } else {
                            break;
                        }
                    }
                }
            }

            frameIndex = d->maxr + 1;

            if (frameIndex < toFrame) {
                int t1 = pfp[frameIndex][x];
                int diff = std::abs(c - t1);

                if (diff < thresh) {
                    unsigned weight = weightSaved[useDiff ? diff >> d->shift : frameIndex];
                    weights += weight;
                    sum += srcp[frameIndex][x] * weight;

                    frameIndex++;
                    int v = 256;

                    while (frameIndex < toFrame) {
                        const int t2 = t1;
                        t1 = pfp[frameIndex][x];
                        diff = std::abs(c - t1);

                        if (diff < thresh && std::abs(t1 - t2) < thresh) {
                            weight = weightSaved[useDiff ? (diff >> d->shift) + v : frameIndex];
                            weights += weight;
                            sum += srcp[frameIndex][x] * weight;

                            frameIndex++;
                            v += 256;
                        } else {
                            break;
                        }
                    }
                }
            }

            if (d->fp)
                dstp[x] = static_cast<T1>((srcp[d->maxr][x] * (65536 - weights) + sum + 32768) >> 16);
            else
                dstp[x] = static_cast<T1>((sum + (weights >> 1)) / weights);
        }

        for (int i = 0; i < d->diameter; i++) {
            srcp[i] += stride;
            pfp[i] += stride;
        }
        dstp += stride;
    }
}

template<bool useDiff>
static void filterF(const VSFrameRef * src[15], const VSFrameRef * pf[15], VSFrameRef * dst, const int fromFrame, const int toFrame, const int plane,
                    const TTempSmoothData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept {
    const int width = vsapi->getFrameWidth(dst, plane);
    const int height = vsapi->getFrameHeight(dst, plane);
    const int stride = vsapi->getStride(dst, plane) / sizeof(float);
    const float * srcp[15] = {}, * pfp[15] = {};
    for (int i = 0; i < d->diameter; i++) {
        srcp[i] = reinterpret_cast<const float *>(vsapi->getReadPtr(src[i], plane));
        pfp[i] = reinterpret_cast<const float *>(vsapi->getReadPtr(pf[i], plane));
    }
    float * VS_RESTRICT dstp = reinterpret_cast<float *>(vsapi->getWritePtr(dst, plane));

    const float thresh = d->threshF[plane];
    const unsigned * const weightSaved = d->weight[plane];

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const float c = pfp[d->maxr][x];
            unsigned weights = d->cw;
            float sum = srcp[d->maxr][x] * d->cw;

            int frameIndex = d->maxr - 1;

            if (frameIndex > fromFrame) {
                float t1 = pfp[frameIndex][x];
                float diff = std::abs(c - t1);

                if (diff < thresh) {
                    unsigned weight = weightSaved[useDiff ? static_cast<int>(diff * 255.f) : frameIndex];
                    weights += weight;
                    sum += srcp[frameIndex][x] * weight;

                    frameIndex--;
                    int v = 256;

                    while (frameIndex > fromFrame) {
                        const float t2 = t1;
                        t1 = pfp[frameIndex][x];
                        diff = std::abs(c - t1);

                        if (diff < thresh && std::abs(t1 - t2) < thresh) {
                            weight = weightSaved[useDiff ? static_cast<int>(diff * 255.f) + v : frameIndex];
                            weights += weight;
                            sum += srcp[frameIndex][x] * weight;

                            frameIndex--;
                            v += 256;
                        } else {
                            break;
                        }
                    }
                }
            }

            frameIndex = d->maxr + 1;

            if (frameIndex < toFrame) {
                float t1 = pfp[frameIndex][x];
                float diff = std::abs(c - t1);

                if (diff < thresh) {
                    unsigned weight = weightSaved[useDiff ? static_cast<int>(diff * 255.f) : frameIndex];
                    weights += weight;
                    sum += srcp[frameIndex][x] * weight;

                    frameIndex++;
                    int v = 256;

                    while (frameIndex < toFrame) {
                        const float t2 = t1;
                        t1 = pfp[frameIndex][x];
                        diff = std::abs(c - t1);

                        if (diff < thresh && std::abs(t1 - t2) < thresh) {
                            weight = weightSaved[useDiff ? static_cast<int>(diff * 255.f) + v : frameIndex];
                            weights += weight;
                            sum += srcp[frameIndex][x] * weight;

                            frameIndex++;
                            v += 256;
                        } else {
                            break;
                        }
                    }
                }
            }

            if (d->fp)
                dstp[x] = (srcp[d->maxr][x] * (65536 - weights) + sum) / 65536.f;
            else
                dstp[x] = sum / weights;
        }

        for (int i = 0; i < d->diameter; i++) {
            srcp[i] += stride;
            pfp[i] += stride;
        }
        dstp += stride;
    }
}

static void selectFunctions(TTempSmoothData * d) noexcept {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            if (d->thresh[plane] > d->mdiff[plane] + 1) {
                if (d->vi->format->bytesPerSample == 1)
                    d->filter[plane] = filterI<uint8_t, uint32_t, true>;
                else if (d->vi->format->bytesPerSample == 2)
                    d->filter[plane] = filterI<uint16_t, uint64_t, true>;
                else
                    d->filter[plane] = filterF<true>;
            } else {
                if (d->vi->format->bytesPerSample == 1)
                    d->filter[plane] = filterI<uint8_t, uint32_t, false>;
                else if (d->vi->format->bytesPerSample == 2)
                    d->filter[plane] = filterI<uint16_t, uint64_t, false>;
                else
                    d->filter[plane] = filterF<false>;
            }
        }
    }
}

static void VS_CC ttempsmoothInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    TTempSmoothData * d = static_cast<TTempSmoothData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC ttempsmoothGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    TTempSmoothData * d = static_cast<TTempSmoothData *>(*instanceData);

    if (activationReason == arInitial) {
        for (int i = std::max(n - d->maxr, 0); i <= std::min(n + d->maxr, d->vi->numFrames - 1); i++) {
            vsapi->requestFrameFilter(i, d->node, frameCtx);

            if (d->pfclip)
                vsapi->requestFrameFilter(i, d->pfclip, frameCtx);

            if (d->scthresh)
                vsapi->requestFrameFilter(i, d->propNode, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src[15] = {}, * pf[15] = {}, * prop[15] = {};
        for (int i = n - d->maxr; i <= n + d->maxr; i++) {
            const int frameNumber = std::min(std::max(i, 0), d->vi->numFrames - 1);

            src[i - n + d->maxr] = vsapi->getFrameFilter(frameNumber, d->node, frameCtx);

            if (d->pfclip)
                pf[i - n + d->maxr] = vsapi->getFrameFilter(frameNumber, d->pfclip, frameCtx);

            if (d->scthresh)
                prop[i - n + d->maxr] = vsapi->getFrameFilter(frameNumber, d->propNode, frameCtx);
        }
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src[d->maxr], d->process[1] ? nullptr : src[d->maxr], d->process[2] ? nullptr : src[d->maxr] };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src[d->maxr], core);

        int fromFrame = -1, toFrame = d->diameter;
        if (d->scthresh) {
            for (int i = d->maxr; i > 0; i--) {
                if (vsapi->propGetInt(vsapi->getFramePropsRO(prop[i]), "_SceneChangePrev", 0, nullptr)) {
                    fromFrame = i;
                    break;
                }
            }

            for (int i = d->maxr; i < d->diameter - 1; i++) {
                if (vsapi->propGetInt(vsapi->getFramePropsRO(prop[i]), "_SceneChangeNext", 0, nullptr)) {
                    toFrame = i;
                    break;
                }
            }
        }

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane])
                d->filter[plane](src, d->pfclip ? pf : src, dst, fromFrame, toFrame, plane, d, vsapi);
        }

        for (int i = 0; i < d->diameter; i++) {
            vsapi->freeFrame(src[i]);
            vsapi->freeFrame(pf[i]);
            vsapi->freeFrame(prop[i]);
        }

        return dst;
    }

    return nullptr;
}

static void VS_CC ttempsmoothFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TTempSmoothData * d = static_cast<TTempSmoothData *>(instanceData);

    vsapi->freeNode(d->node);
    vsapi->freeNode(d->pfclip);
    vsapi->freeNode(d->propNode);

    for (int i = 0; i < 3; i++)
        delete[] d->weight[i];

    delete d;
}

static void VS_CC ttempsmoothCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<TTempSmoothData> d{ new TTempSmoothData{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->pfclip = vsapi->propGetNode(in, "pfclip", 0, &err);
    d->vi = vsapi->getVideoInfo(d->node);

    try {
        if (!isConstantFormat(d->vi) || (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bit integer and 32 bit float input supported" };

        d->maxr = int64ToIntS(vsapi->propGetInt(in, "maxr", 0, &err));
        if (err)
            d->maxr = 3;

        const int numThresh = vsapi->propNumElements(in, "thresh");
        if (numThresh > d->vi->format->numPlanes)
            throw std::string{ "more thresh given than there are planes" };

        for (int i = 0; i < numThresh; i++)
            d->thresh[i] = int64ToIntS(vsapi->propGetInt(in, "thresh", i, nullptr));
        if (numThresh <= 0) {
            d->thresh[0] = 4;
            d->thresh[1] = d->thresh[2] = 5;
        } else if (numThresh == 1) {
            d->thresh[1] = d->thresh[2] = d->thresh[0];
        } else if (numThresh == 2) {
            d->thresh[2] = d->thresh[1];
        }

        const int numMdiff = vsapi->propNumElements(in, "mdiff");
        if (numMdiff > d->vi->format->numPlanes)
            throw std::string{ "more mdiff given than there are planes" };

        for (int i = 0; i < numMdiff; i++)
            d->mdiff[i] = int64ToIntS(vsapi->propGetInt(in, "mdiff", i, nullptr));
        if (numMdiff <= 0) {
            d->mdiff[0] = 2;
            d->mdiff[1] = d->mdiff[2] = 3;
        } else if (numMdiff == 1) {
            d->mdiff[1] = d->mdiff[2] = d->mdiff[0];
        } else if (numMdiff == 2) {
            d->mdiff[2] = d->mdiff[1];
        }

        int strength = int64ToIntS(vsapi->propGetInt(in, "strength", 0, &err));
        if (err)
            strength = 2;

        d->scthresh = vsapi->propGetFloat(in, "scthresh", 0, &err);
        if (err)
            d->scthresh = 12.;

        d->fp = !!vsapi->propGetInt(in, "fp", 0, &err);
        if (err)
            d->fp = true;

        const int m = vsapi->propNumElements(in, "planes");

        for (int i = 0; i < 3; i++)
            d->process[i] = (m <= 0);

        for (int i = 0; i < m; i++) {
            const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

            if (n < 0 || n >= d->vi->format->numPlanes)
                throw std::string{ "plane index out of range" };

            if (d->process[n])
                throw std::string{ "plane specified twice" };

            d->process[n] = true;
        }

        if (d->maxr < 1 || d->maxr > 7)
            throw std::string{ "maxr must be between 1 and 7 (inclusive)" };

        for (int i = 0; i < 3; i++) {
            if (d->thresh[i] < 1 || d->thresh[i] > 256)
                throw std::string{ "thresh must be between 1 and 256 (inclusive)" };

            if (d->mdiff[i] < 0 || d->mdiff[i] > 255)
                throw std::string{ "mdiff must be between 0 and 255 (inclusive)" };
        }

        if (strength < 1 || strength > 8)
            throw std::string{ "strength must be 1, 2, 3, 4, 5, 6, 7 or 8" };

        if (d->scthresh < 0. || d->scthresh > 100.)
            throw std::string{ "scthresh must be between 0.0 and 100.0 (inclusive)" };

        if (d->pfclip) {
            if (!isSameFormat(vsapi->getVideoInfo(d->pfclip), d->vi))
                throw std::string{ "pfclip must have the same dimensions as main clip and be the same format" };

            if (vsapi->getVideoInfo(d->pfclip)->numFrames != d->vi->numFrames)
                throw std::string{ "pfclip's number of frames doesn't match" };
        }

        selectFunctions(d.get());

        d->shift = d->vi->format->bitsPerSample - 8;

        d->diameter = d->maxr * 2 + 1;

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane]) {
                if (d->thresh[plane] > d->mdiff[plane] + 1) {
                    d->weight[plane] = new unsigned[256 * d->maxr];
                    double dt[15] = {}, rt[256] = {}, sum = 0.;

                    for (int i = 0; i < strength && i <= d->maxr; i++)
                        dt[i] = 1.;
                    for (int i = strength; i <= d->maxr; i++)
                        dt[i] = 1. / (i - strength + 2);

                    sum += dt[0];
                    for (int i = 1; i <= d->maxr; i++)
                        sum += dt[i] * 2.;

                    const double scale = 65536. / sum;

                    const double step = 256. / (d->thresh[plane] - std::min(d->mdiff[plane], d->thresh[plane] - 1));
                    double base = 256.;

                    for (int i = 0; i < d->thresh[plane]; i++) {
                        if (d->mdiff[plane] > i) {
                            rt[i] = 256.;
                        } else {
                            if (base > 0.)
                                rt[i] = base;
                            else
                                break;
                            base -= step;
                        }
                    }

                    for (int i = 1; i <= d->maxr; i++) {
                        for (int v = 0; v < 256; v++)
                            d->weight[plane][256 * (i - 1) + v] = static_cast<unsigned>((dt[i] * scale * rt[v] / 256.) + 0.5);
                    }

                    d->cw = static_cast<unsigned>(dt[0] * scale + 0.5);
                } else {
                    d->weight[plane] = new unsigned[d->diameter];
                    double dt[15] = {}, sum = 0.;

                    for (int i = 0; i < strength && i <= d->maxr; i++)
                        dt[d->maxr - i] = dt[d->maxr + i] = 1.;
                    for (int i = strength; i <= d->maxr; i++)
                        dt[d->maxr - i] = dt[d->maxr + i] = 1. / (i - strength + 2);

                    for (int i = 0; i < d->diameter; i++)
                        sum += dt[i];

                    const double scale = 65536. / sum;

                    for (int i = 0; i < d->diameter; i++)
                        d->weight[plane][i] = static_cast<unsigned>(dt[i] * scale + 0.5);

                    d->cw = d->weight[plane][d->maxr];
                }

                if (d->vi->format->sampleType == stInteger)
                    d->thresh[plane] <<= d->shift;
                else
                    d->threshF[plane] = d->thresh[plane] / 256.f;
            }
        }

        if (d->scthresh) {
            VSMap * args = vsapi->createMap();

            if (d->vi->format->colorFamily == cmRGB) {
                vsapi->propSetNode(args, "clip", d->pfclip ? d->pfclip : d->node, paReplace);
                vsapi->propSetInt(args, "format", pfGray8, paReplace);
                vsapi->propSetData(args, "matrix_s", "709", -1, paReplace);

                VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.resize", core), "Bicubic", args);
                if (vsapi->getError(ret)) {
                    vsapi->setError(out, vsapi->getError(ret));
                    vsapi->freeNode(d->node);
                    vsapi->freeNode(d->pfclip);
                    vsapi->freeMap(args);
                    vsapi->freeMap(ret);
                    return;
                }

                vsapi->clearMap(args);
                vsapi->propSetNode(args, "clip", vsapi->propGetNode(ret, "clip", 0, nullptr), paReplace);
                vsapi->freeMap(ret);
            } else {
                vsapi->propSetNode(args, "clip", d->pfclip ? d->pfclip : d->node, paReplace);
            }
            vsapi->propSetFloat(args, "threshold", d->scthresh / 100., paReplace);

            VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.misc", core), "SCDetect", args);
            if (vsapi->getError(ret)) {
                vsapi->setError(out, vsapi->getError(ret));
                vsapi->freeNode(d->node);
                vsapi->freeNode(d->pfclip);
                vsapi->freeMap(args);
                vsapi->freeMap(ret);
                return;
            }

            d->propNode = vsapi->propGetNode(ret, "clip", 0, nullptr);
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
        }
    } catch (const std::string & error) {
        vsapi->setError(out, ("TTempSmooth: " + error).c_str());
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->pfclip);
        return;
    }

    vsapi->createFilter(in, out, "TTempSmooth", ttempsmoothInit, ttempsmoothGetFrame, ttempsmoothFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.ttempsmooth", "ttmpsm", "A basic, motion adaptive, temporal smoothing filter", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("TTempSmooth",
                 "clip:clip;"
                 "maxr:int:opt;"
                 "thresh:int[]:opt;"
                 "mdiff:int[]:opt;"
                 "strength:int:opt;"
                 "scthresh:float:opt;"
                 "fp:int:opt;"
                 "pfclip:clip:opt;"
                 "planes:int[]:opt;",
                 ttempsmoothCreate, nullptr, plugin);
}
