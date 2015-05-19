/*
**   VapourSynth port by HolyWu
**
**                 tcanny v1.0 for Avisynth 2.5.x
**
**   Copyright (C) 2009 Kevin Stone
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
#include <cfloat>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>

#define M_PIF 3.14159265358979323846f

struct TCannyData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    float sigma, t_h, t_l, gmmax;
    int nms, mode;
    bool process[3];
    int grad;
    float * weights;
};

struct Stack {
    uint8_t * map;
    std::pair<int, int> * pos;
    int index;
};

static void reset(Stack & s, const size_t size) {
    memset(s.map, 0, size);
    s.index = -1;
}

static void push(Stack & s, const int x, const int y) {
    s.pos[++s.index].first = x;
    s.pos[s.index].second = y;
}

static std::pair<int, int> pop(Stack & s) {
    return s.pos[s.index--];
}

static float * gaussianWeights(const float sigma, int & rad) {
    const int dia = std::max(static_cast<int>(sigma * 3.f + 0.5f), 1) * 2 + 1;
    rad = dia >> 1;
    float * weights = vs_aligned_malloc<float>(dia * sizeof(float), 32);
    if (!weights)
        return nullptr;
    float sum = 0.f;
    for (int k = -rad; k <= rad; k++) {
        const float w = std::exp(-(k * k) / (2.f * sigma * sigma));
        weights[k + rad] = w;
        sum += w;
    }
    for (int k = 0; k < dia; k++)
        weights[k] /= sum;
    return weights;
}

template<typename T>
static void genConvV(const T * srcp, float * VS_RESTRICT dstp, const int width, const int height, const int stride, const int rad, const float * weights, const int bitsPerSample) {
    const float divisor = 1.f / (1 << (bitsPerSample - 8));
    weights += rad;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.f;
            for (int v = -rad; v <= rad; v++) {
                int yc = y + v;
                if (yc < 0)
                    yc = -yc;
                else if (yc >= height)
                    yc = 2 * (height - 1) - yc;
                sum += srcp[yc * stride + x] * divisor * weights[v];
            }
            dstp[x] = sum;
        }
        dstp += stride;
    }
}

static void genConvH(const float * srcp, float * VS_RESTRICT dstp, const int width, const int height, const int stride, const int rad, const float * weights) {
    weights += rad;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.f;
            for (int v = -rad; v <= rad; v++) {
                int xc = x + v;
                if (xc < 0)
                    xc = -xc;
                else if (xc >= width)
                    xc = 2 * (width - 1) - xc;
                sum += srcp[xc] * weights[v];
            }
            dstp[x] = sum;
        }
        srcp += stride;
        dstp += stride;
    }
}

static int getBin(const float dir, const int n) {
    const float scale = n / M_PIF;
    const int bin = static_cast<int>(dir * scale + 0.5f);
    return (bin >= n) ? 0 : bin;
}

static void gmDirImages(float * VS_RESTRICT srcp, float * VS_RESTRICT gimg, float * VS_RESTRICT dimg, const int width, const int height, const int stride, const int nms) {
    memset(gimg, 0, stride * height * sizeof(float));
    memset(dimg, 0, stride * height * sizeof(float));
    float * VS_RESTRICT srcpT = srcp + stride;
    float * VS_RESTRICT gmnT = gimg + stride;
    float * VS_RESTRICT dirT = dimg + stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            const float dx = srcpT[x + 1] - srcpT[x - 1];
            const float dy = srcpT[x - stride] - srcpT[x + stride];
            gmnT[x] = std::sqrt(dx * dx + dy * dy);
            const float dr = std::atan2(dy, dx);
            dirT[x] = dr + (dr < 0.f ? M_PIF : 0.f);
        }
        srcpT += stride;
        gmnT += stride;
        dirT += stride;
    }
    memcpy(srcp, gimg, stride * height * sizeof(float));
    if (!nms)
        return;
    const int offTable[4] = { 1, -stride + 1, -stride, -stride - 1 };
    srcpT = srcp + stride;
    gmnT = gimg + stride;
    dirT = dimg + stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            const float dir = dirT[x];
            if (nms & 1) {
                const int off = offTable[getBin(dir, 4)];
                if (gmnT[x] >= std::max(gmnT[x + off], gmnT[x - off]))
                    continue;
            }
            if (nms & 2) {
                float val1, val2;
                const int c = static_cast<int>(dir * (4.f / M_PIF));
                if (c == 0 || c >= 4) {
                    const float h = std::tan(dir);
                    val1 = (1.f - h) * gmnT[x + 1] + h * gmnT[x - stride + 1];
                    val2 = (1.f - h) * gmnT[x - 1] + h * gmnT[x + stride - 1];
                } else if (c == 1) {
                    const float w = 1.f / std::tan(dir);
                    val1 = (1.f - w) * gmnT[x - stride] + w * gmnT[x - stride + 1];
                    val2 = (1.f - w) * gmnT[x + stride] + w * gmnT[x + stride - 1];
                } else if (c == 2) {
                    const float w = 1.f / std::tan(M_PIF - dir);
                    val1 = (1.f - w) * gmnT[x - stride] + w * gmnT[x - stride - 1];
                    val2 = (1.f - w) * gmnT[x + stride] + w * gmnT[x + stride + 1];
                } else {
                    const float h = std::tan(M_PIF - dir);
                    val1 = (1.f - h) * gmnT[x - 1] + h * gmnT[x - stride - 1];
                    val2 = (1.f - h) * gmnT[x + 1] + h * gmnT[x + stride + 1];
                }
                if (gmnT[x] >= std::max(val1, val2))
                    continue;
            }
            srcpT[x] = -FLT_MAX;
        }
        gmnT += stride;
        dirT += stride;
        srcpT += stride;
    }
}

static void hystersis(float * VS_RESTRICT srcp, Stack & VS_RESTRICT stack, const int width, const int height, const int stride, const float t_h, const float t_l) {
    reset(stack, width * height);

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (srcp[x + y * stride] < t_h || stack.map[x + y * width])
                continue;
            srcp[x + y * stride] = FLT_MAX;
            stack.map[x + y * width] = UINT8_MAX;
            push(stack, x, y);

            while (stack.index > -1) {
                const std::pair<int, int> pos = pop(stack);
                const int xMin = (pos.first > 1) ? pos.first - 1 : 1;
                const int xMax = (pos.first < width - 2) ? pos.first + 1 : pos.first;
                const int yMin = (pos.second > 1) ? pos.second - 1 : 1;
                const int yMax = (pos.second < height - 2) ? pos.second + 1 : pos.second;
                for (int yy = yMin; yy <= yMax; yy++) {
                    for (int xx = xMin; xx <= xMax; xx++) {
                        if (srcp[xx + yy * stride] > t_l && !stack.map[xx + yy * width]) {
                            srcp[xx + yy * stride] = FLT_MAX;
                            stack.map[xx + yy * width] = UINT8_MAX;
                            push(stack, xx, yy);
                        }
                    }
                }
            }
        }
    }
}

template<typename T>
static void binarizeCE(const float * srcp, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const float t_h, const int bitsPerSample) {
    const T peak = (1 << bitsPerSample) - 1;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = (srcp[x] >= t_h) ? peak : 0;
        srcp += stride;
        dstp += stride;
    }
}

template<typename T>
static void discretizeGM(const float * gimg, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const float scale, const int bitsPerSample) {
    const float multiplier = static_cast<float>(1 << (bitsPerSample - 8));
    const int peak = (1 << bitsPerSample) - 1;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = std::min(static_cast<int>(gimg[x] * multiplier * scale + 0.5f), peak);
        gimg += stride;
        dstp += stride;
    }
}

template<typename T>
static void discretizeDM_T(const float * srcp, const float * dimg, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const float t_h, const int bitsPerSample) {
    const int n = 1 << bitsPerSample;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = (srcp[x] >= t_h) ? getBin(dimg[x], n) : 0;
        srcp += stride;
        dimg += stride;
        dstp += stride;
    }
}

template<typename T>
static void discretizeDM(const float * dimg, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const int bitsPerSample) {
    const int n = 1 << bitsPerSample;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = getBin(dimg[x], n);
        dimg += stride;
        dstp += stride;
    }
}

template<typename T>
static void TCanny(const VSFrameRef * src, VSFrameRef * dst, float * VS_RESTRICT fa[3], Stack & VS_RESTRICT stack, const TCannyData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int stride = vsapi->getStride(src, plane) / sizeof(T);
            const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
            T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
            genConvV(srcp, fa[1], width, height, stride, d->grad, d->weights, d->vi->format->bitsPerSample);
            genConvH(fa[1], fa[0], width, height, stride, d->grad, d->weights);
            gmDirImages(fa[0], fa[1], fa[2], width, height, stride, (d->mode & 1) ? 0 : d->nms);
            if (!(d->mode & 1))
                hystersis(fa[0], stack, width, height, stride, d->t_h, d->t_l);
            if (d->mode == 0)
                binarizeCE(fa[0], dstp, width, height, stride, d->t_h, d->vi->format->bitsPerSample);
            else if (d->mode == 1)
                discretizeGM(fa[1], dstp, width, height, stride, 255.f / d->gmmax, d->vi->format->bitsPerSample);
            else if (d->mode == 2)
                discretizeDM_T(fa[0], fa[2], dstp, width, height, stride, d->t_h, d->vi->format->bitsPerSample);
            else
                discretizeDM(fa[2], dstp, width, height, stride, d->vi->format->bitsPerSample);
        }
    }
}

static void VS_CC tcannyInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    TCannyData * d = static_cast<TCannyData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC tcannyGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const TCannyData * d = static_cast<const TCannyData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        float * fa[3];
        for (int i = 0; i < 3; i++) {
            fa[i] = vs_aligned_malloc<float>(vsapi->getStride(src, 0) / d->vi->format->bytesPerSample * d->vi->height * sizeof(float), 32);
            if (!fa[i]) {
                vsapi->setFilterError("TCanny: malloc failure (fa)", frameCtx);
                vsapi->freeFrame(src);
                vsapi->freeFrame(dst);
                return nullptr;
            }
        }

        Stack stack = {};
        if (!(d->mode & 1)) {
            stack.map = vs_aligned_malloc<uint8_t>(d->vi->width * d->vi->height, 32);
            stack.pos = vs_aligned_malloc<std::pair<int, int>>(d->vi->width * d->vi->height * sizeof(std::pair<int, int>), 32);
            if (!stack.map || !stack.pos) {
                vsapi->setFilterError("TCanny: malloc failure (stack)", frameCtx);
                vsapi->freeFrame(src);
                vsapi->freeFrame(dst);
                return nullptr;
            }
        }

        if (d->vi->format->bitsPerSample == 8)
            TCanny<uint8_t>(src, dst, fa, stack, d, vsapi);
        else
            TCanny<uint16_t>(src, dst, fa, stack, d, vsapi);

        vsapi->freeFrame(src);
        for (int i = 0; i < 3; i++)
            vs_aligned_free(fa[i]);
        vs_aligned_free(stack.map);
        vs_aligned_free(stack.pos);
        return dst;
    }

    return nullptr;
}

static void VS_CC tcannyFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TCannyData * d = static_cast<TCannyData *>(instanceData);
    vsapi->freeNode(d->node);
    vs_aligned_free(d->weights);
    delete d;
}

static void VS_CC tcannyCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    TCannyData d;
    int err;

    d.sigma = static_cast<float>(vsapi->propGetFloat(in, "sigma", 0, &err));
    if (err)
        d.sigma = 1.5f;
    d.t_h = static_cast<float>(vsapi->propGetFloat(in, "t_h", 0, &err));
    if (err)
        d.t_h = 8.f;
    d.t_l = static_cast<float>(vsapi->propGetFloat(in, "t_l", 0, &err));
    if (err)
        d.t_l = 1.f;
    d.nms = int64ToIntS(vsapi->propGetInt(in, "nms", 0, &err));
    if (err)
        d.nms = 3;
    d.mode = int64ToIntS(vsapi->propGetInt(in, "mode", 0, &err));
    d.gmmax = static_cast<float>(vsapi->propGetFloat(in, "gmmax", 0, &err));
    if (err)
        d.gmmax = 50.f;

    if (d.sigma <= 0.f) {
        vsapi->setError(out, "TCanny: sigma must be greater than 0.0");
        return;
    }
    if (d.nms < 0 || d.nms > 3) {
        vsapi->setError(out, "TCanny: nms must be set to 0, 1, 2 or 3");
        return;
    }
    if (d.mode < 0 || d.mode > 3) {
        vsapi->setError(out, "TCanny: mode must be set to 0, 1, 2 or 3");
        return;
    }
    if (d.gmmax < 1.f) {
        vsapi->setError(out, "TCanny: gmmax must be greater than or equal to 1.0");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bitsPerSample > 16) {
        vsapi->setError(out, "TCanny: only constant format 8-16 bits integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    const int m = vsapi->propNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        d.process[i] = m <= 0;

    for (int i = 0; i < m; i++) {
        const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (n < 0 || n >= d.vi->format->numPlanes) {
            vsapi->setError(out, "TCanny: plane index out of range");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.process[n]) {
            vsapi->setError(out, "TCanny: plane specified twice");
            vsapi->freeNode(d.node);
            return;
        }

        d.process[n] = true;
    }

    d.weights = gaussianWeights(d.sigma, d.grad);
    if (!d.weights) {
        vsapi->setError(out, "TCanny: malloc failure (weights)");
        vsapi->freeNode(d.node);
        return;
    }

    TCannyData * data = new TCannyData(d);

    vsapi->createFilter(in, out, "TCanny", tcannyInit, tcannyGetFrame, tcannyFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.tcanny", "tcanny", "Build an edge map using canny edge detection", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("TCanny", "clip:clip;sigma:float:opt;t_h:float:opt;t_l:float:opt;nms:int:opt;mode:int:opt;gmmax:float:opt;planes:int[]:opt;", tcannyCreate, nullptr, plugin);
}
