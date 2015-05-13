/*
  VapourSynth port by HolyWu

  VagueDenoiser plugin for Avisynth -- a wavelet denoiser plugin
  VagueDenoiser (C) 2003, LeFunGus
  VagueDenoiser assembly parts, debug and adaptative thresholding (C) Kurosu
  WienerChop mode, 9/7 and 6/10 wavelets, AutoThreshold, aux clip (c)2004-2005 Fizick 

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  The author can be contacted at:
  lefungus@altern.org

  Thanks to Geoff Davis for his wavelet Transform Coder Construction Kit
  Look at http://www.geoffdavis.net/ for the full package.
  Thanks to everyone at Doom9.org for their help.
*/

#include <algorithm>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>

#define NPAD 10

struct VagueDenoiserData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    float threshold, percent;
    int method, nsteps;
    bool process[3];
    float * analysisLow, * analysisHigh, * synthesisLow, * synthesisHigh;
    void (*byteToFloat)(const uint8_t * srcp, float * block, const int width, const int height, const int stride, const int bitsPerSample);
    void (*floatToByte)(const float * block, uint8_t * dstp, const int width, const int height, const int stride, const int bitsPerSample);
    void (*thresholding)(float * block, const int width, const int height, const int stride, const VagueDenoiserData * d);
};

static void byteToFloat_8(const uint8_t * srcp, float * VS_RESTRICT block, const int width, const int height, const int stride, const int bitsPerSample) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            block[x] = srcp[x];
        srcp += stride;
        block += stride;
    }
}

static void byteToFloat_16(const uint8_t * _srcp, float * VS_RESTRICT block, const int width, const int height, const int stride, const int bitsPerSample) {
    const uint16_t * srcp = reinterpret_cast<const uint16_t *>(_srcp);
    const float divisor = 1.f / (1 << (bitsPerSample - 8));

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            block[x] = srcp[x] * divisor;
        srcp += stride;
        block += stride;
    }
}

static void floatToByte_8(const float * block, uint8_t * VS_RESTRICT dstp, const int width, const int height, const int stride, const int bitsPerSample) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = std::min(std::max(static_cast<int>(block[x] + 0.5f), 0), 255);
        block += stride;
        dstp += stride;
    }
}

static void floatToByte_16(const float * block, uint8_t * VS_RESTRICT _dstp, const int width, const int height, const int stride, const int bitsPerSample) {
    uint16_t * VS_RESTRICT dstp = reinterpret_cast<uint16_t *>(_dstp);
    const float multiplier = static_cast<float>(1 << (bitsPerSample - 8));
    const int peak = (1 << bitsPerSample) - 1;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = std::min(std::max(static_cast<int>(block[x] * multiplier + 0.5f), 0), peak);
        block += stride;
        dstp += stride;
    }
}

static void hardThresholding(float * VS_RESTRICT block, const int width, const int height, const int stride, const VagueDenoiserData * d) {
    const float frac = 1.f - d->percent * 0.01f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (std::fabs(block[x]) <= d->threshold)
                block[x] *= frac;
        }
        block += stride;
    }
}

static void softThresholding(float * VS_RESTRICT block, const int width, const int height, const int stride, const VagueDenoiserData * d) {
    const float frac = 1.f - d->percent * 0.01f;
    const float shift = d->threshold * 0.01f * d->percent;

    int w = width;
    int h = height;
    for (int l = 0; l < d->nsteps; l++) {
        w = (w + 1) >> 1;
        h = (h + 1) >> 1;
    }

    for (int y = 0; y < height; y++) {
        const int x0 = (y < h) ? w : 0;
        for (int x = x0; x < width; x++) {
            const float temp = std::fabs(block[x]);
            if (temp <= d->threshold)
                block[x] *= frac;
            else
                block[x] = (block[x] < 0.f ? -1.f : (block[x] > 0.f ? 1.f : 0.f)) * (temp - shift);
        }
        block += stride;
    }
}

static void qianThresholding(float * VS_RESTRICT block, const int width, const int height, const int stride, const VagueDenoiserData * d) {
    const float percent01 = d->percent * 0.01f;
    const float tr2 = d->threshold * d->threshold * percent01;
    const float frac = 1.f - percent01;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const float temp = std::fabs(block[x]);
            if (temp <= d->threshold) {
                block[x] *= frac;
            } else {
                const float tp2 = temp * temp;
                block[x] *= (tp2 - tr2) / tp2;
            }
        }
        block += stride;
    }
}

static inline void copy(const float * p1, float * VS_RESTRICT p2, const int length) {
    memcpy(p2, p1, length * sizeof(float));
}

static inline void copy(const float * p1, const int stride1, float * VS_RESTRICT p2, const int length) {
    for (int i = 0; i < length; i++) {
        p2[i] = *p1;
        p1 += stride1;
    }
}

static inline void copy(const float * p1, float * VS_RESTRICT p2, const int stride2, const int length) {
    for (int i = 0; i < length; i++) {
        *p2 = p1[i];
        p2 += stride2;
    }
}

// Do symmetric extension of data using prescribed symmetries
// Original values are in output[npad] through output[npad+size-1]
// New values will be placed in output[0] through output[npad] and in output[npad+size] through output[2*npad+size-1] (note: end values may not be filled in)
// extension at left bdry is ... 3 2 1 0 | 0 1 2 3 ...
// same for right boundary
// if rightExt=1 then ... 3 2 1 0 | 1 2 3
static void symmetricExtension(float * VS_RESTRICT output, const int size, const int leftExt, const int rightExt) {
    int first = NPAD;
    int last = NPAD - 1 + size;

    const int originalLast = last;

    // output[9] = output[10];
    if (leftExt == 2)
        output[--first] = output[NPAD];
    if (rightExt == 2)
        output[++last] = output[originalLast];

    // extend left end
    int nextend = first;
    for (int i = 0; i < nextend; i++)
        output[--first] = output[NPAD + 1 + i];

    const int idx = NPAD + NPAD - 1 + size;

    // extend right end
    nextend = idx - last;
    for (int i = 0; i < nextend; i++)
        output[++last] = output[originalLast - 1 - i];
}

static void transformStep(float * VS_RESTRICT input, float * VS_RESTRICT output, const int size, const int lowSize, const VagueDenoiserData * data) {
    symmetricExtension(input, size, 1, 1);
    for (int i = NPAD; i < NPAD + lowSize; i++) {
        const float a = input[2 * i - 14] * data->analysisLow[0];
        const float b = input[2 * i - 13] * data->analysisLow[1];
        const float c = input[2 * i - 12] * data->analysisLow[2];
        const float d = input[2 * i - 11] * data->analysisLow[3];
        const float e = input[2 * i - 10] * data->analysisLow[4];
        const float f = input[2 * i - 9] * data->analysisLow[3];
        const float g = input[2 * i - 8] * data->analysisLow[2];
        const float h = input[2 * i - 7] * data->analysisLow[1];
        const float k = input[2 * i - 6] * data->analysisLow[0];
        output[i] = a + b + c + d + e + f + g + h + k;
    }
    for (int i = NPAD; i < NPAD + lowSize; i++) {
        const float a = input[2 * i - 12] * data->analysisHigh[0];
        const float b = input[2 * i - 11] * data->analysisHigh[1];
        const float c = input[2 * i - 10] * data->analysisHigh[2];
        const float d = input[2 * i - 9] * data->analysisHigh[3];
        const float e = input[2 * i - 8] * data->analysisHigh[2];
        const float f = input[2 * i - 7] * data->analysisHigh[1];
        const float g = input[2 * i - 6] * data->analysisHigh[0];
        output[i + lowSize] = a + b + c + d + e + f + g;
    }
}

static void invertStep(const float * input, float * VS_RESTRICT output, float * VS_RESTRICT temp, const int size, const VagueDenoiserData * data) {
    const int lowSize = (size + 1) >> 1;
    const int highSize = size >> 1;

    memcpy(temp + NPAD, input + NPAD, lowSize * sizeof(float));

    int leftExt = 1;
    int rightExt = (size % 2 == 0) ? 2 : 1;
    symmetricExtension(temp, lowSize, leftExt, rightExt);

    memset(output, 0, (NPAD + NPAD + size) * sizeof(float));
    const int findex = (size + 2) >> 1;
    for (int i = 9; i < findex + 11; i++) {
        const float a = temp[i] * data->synthesisLow[0];
        const float b = temp[i] * data->synthesisLow[1];
        const float c = temp[i] * data->synthesisLow[2];
        const float d = temp[i] * data->synthesisLow[3];
        output[2 * i - 13] += a;
        output[2 * i - 12] += b;
        output[2 * i - 11] += c;
        output[2 * i - 10] += d;
        output[2 * i - 9] += c;
        output[2 * i - 8] += b;
        output[2 * i - 7] += a;
    }

    memcpy(temp + NPAD, input + NPAD + lowSize, highSize * sizeof(float));

    leftExt = 2;
    rightExt = (size % 2 == 0) ? 1 : 2;
    symmetricExtension(temp, highSize, leftExt, rightExt);
    for (int i = 8; i < findex + 11; i++) {
        const float a = temp[i] * data->synthesisHigh[0];
        const float b = temp[i] * data->synthesisHigh[1];
        const float c = temp[i] * data->synthesisHigh[2];
        const float d = temp[i] * data->synthesisHigh[3];
        const float e = temp[i] * data->synthesisHigh[4];
        output[2 * i - 13] += a;
        output[2 * i - 12] += b;
        output[2 * i - 11] += c;
        output[2 * i - 10] += d;
        output[2 * i - 9] += e;
        output[2 * i - 8] += d;
        output[2 * i - 7] += c;
        output[2 * i - 6] += b;
        output[2 * i - 5] += a;
    }
}

static void filterBlock(const VSFrameRef * src, VSFrameRef * dst, float * block, float * VS_RESTRICT tempIn, float * VS_RESTRICT tempOut, float * VS_RESTRICT temp2,
                        int * VS_RESTRICT hLowSize, int * VS_RESTRICT hHighSize, int * VS_RESTRICT vLowSize, int * VS_RESTRICT vHighSize,
                        const VagueDenoiserData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int stride = vsapi->getStride(src, plane) / d->vi->format->bytesPerSample;
            const uint8_t * srcp = vsapi->getReadPtr(src, plane);
            uint8_t * dstp = vsapi->getWritePtr(dst, plane);

            d->byteToFloat(srcp, block, width, height, stride, d->vi->format->bitsPerSample);

            int nstepsTransform = d->nsteps;
            int hLowSize0 = width;
            int vLowSize0 = height;
            while (nstepsTransform--) {
                int lowSize = (hLowSize0 + 1) >> 1;
                float * inputp = block;
                for (int j = 0; j < vLowSize0; j++) {
                    copy(inputp, tempIn + NPAD, hLowSize0);
                    transformStep(tempIn, tempOut, hLowSize0, lowSize, d);
                    copy(tempOut + NPAD, inputp, hLowSize0);
                    inputp += stride;
                }

                lowSize = (vLowSize0 + 1) >> 1;
                inputp = block;
                for (int j = 0; j < hLowSize0; j++) {
                    copy(inputp, stride, tempIn + NPAD, vLowSize0);
                    transformStep(tempIn, tempOut, vLowSize0, lowSize, d);
                    copy(tempOut + NPAD, inputp, stride, vLowSize0);
                    inputp++;
                }

                hLowSize0 = (hLowSize0 + 1) >> 1;
                vLowSize0 = (vLowSize0 + 1) >> 1;
            }

            d->thresholding(block, width, height, stride, d);

            int nstepsInvert = d->nsteps;
            hLowSize[0] = (width + 1) >> 1;
            hHighSize[0] = width >> 1;
            vLowSize[0] = (height + 1) >> 1;
            vHighSize[0] = height >> 1;
            for (int i = 1; i < nstepsInvert; i++) {
                hLowSize[i] = (hLowSize[i - 1] + 1) >> 1;
                hHighSize[i] = hLowSize[i - 1] >> 1;
                vLowSize[i] = (vLowSize[i - 1] + 1) >> 1;
                vHighSize[i] = vLowSize[i - 1] >> 1;
            }
            while (nstepsInvert--) {
                const int idx = vLowSize[nstepsInvert] + vHighSize[nstepsInvert];
                const int idx2 = hLowSize[nstepsInvert] + hHighSize[nstepsInvert];
                float * idx3 = block;
                for (int i = 0; i < idx2; i++) {
                    copy(idx3, stride, tempIn + NPAD, idx);
                    invertStep(tempIn, tempOut, temp2, idx, d);
                    copy(tempOut + NPAD, idx3, stride, idx);
                    idx3++;
                }

                idx3 = block;
                for (int i = 0; i < idx; i++) {
                    copy(idx3, tempIn + NPAD, idx2);
                    invertStep(tempIn, tempOut, temp2, idx2, d);
                    copy(tempOut + NPAD, idx3, idx2);
                    idx3 += stride;
                }
            }

            d->floatToByte(block, dstp, width, height, stride, d->vi->format->bitsPerSample);
        }
    }
}

static void VS_CC vaguedenoiserInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    VagueDenoiserData * d = static_cast<VagueDenoiserData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC vaguedenoiserGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const VagueDenoiserData * d = static_cast<const VagueDenoiserData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        float * block = vs_aligned_malloc<float>((vsapi->getStride(src, 0) / d->vi->format->bytesPerSample * d->vi->height) * sizeof(float), 32);
        if (!block) {
            vsapi->setFilterError("VagueDenoiser: malloc failure (block)", frameCtx);
            vsapi->freeFrame(src);
            vsapi->freeFrame(dst);
            return nullptr;
        }

        float * tempIn = vs_aligned_malloc<float>((32 + std::max(d->vi->width, d->vi->height)) * sizeof(float), 32);
        float * tempOut = vs_aligned_malloc<float>((32 + std::max(d->vi->width, d->vi->height)) * sizeof(float), 32);
        float * temp2 = vs_aligned_malloc<float>((32 + std::max(d->vi->width, d->vi->height)) * sizeof(float), 32);
        if (!tempIn || !tempOut || !temp2) {
            vsapi->setFilterError("VagueDenoiser: malloc failure (tempIn/tempOut/temp2)", frameCtx);
            vsapi->freeFrame(src);
            vsapi->freeFrame(dst);
            return nullptr;
        }

        int * hLowSize = new int[d->nsteps];
        int * hHighSize = new int[d->nsteps];
        int * vLowSize = new int[d->nsteps];
        int * vHighSize = new int[d->nsteps];

        filterBlock(src, dst, block, tempIn, tempOut, temp2, hLowSize, hHighSize, vLowSize, vHighSize, d, vsapi);

        vsapi->freeFrame(src);
        vs_aligned_free(block);
        vs_aligned_free(tempIn);
        vs_aligned_free(tempOut);
        vs_aligned_free(temp2);
        delete[] hLowSize;
        delete[] hHighSize;
        delete[] vLowSize;
        delete[] vHighSize;
        return dst;
    }

    return nullptr;
}

static void VS_CC vaguedenoiserFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    VagueDenoiserData * d = static_cast<VagueDenoiserData *>(instanceData);
    vsapi->freeNode(d->node);
    vs_aligned_free(d->analysisLow);
    vs_aligned_free(d->analysisHigh);
    vs_aligned_free(d->synthesisLow);
    vs_aligned_free(d->synthesisHigh);
    delete d;
}

static void VS_CC vaguedenoiserCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    VagueDenoiserData d;
    int err;

    d.threshold = static_cast<float>(vsapi->propGetFloat(in, "threshold", 0, &err));
    if (err)
        d.threshold = 2.f;
    d.method = int64ToIntS(vsapi->propGetInt(in, "method", 0, &err));
    if (err)
        d.method = 2;
    d.nsteps = int64ToIntS(vsapi->propGetInt(in, "nsteps", 0, &err));
    if (err)
        d.nsteps = 6;
    d.percent = static_cast<float>(vsapi->propGetFloat(in, "percent", 0, &err));
    if (err)
        d.percent = 85.f;

    if (d.threshold <= 0.f) {
        vsapi->setError(out, "VagueDenoiser: threshold must be greater than 0.0");
        return;
    }
    if (d.method < 0 || d.method > 2) {
        vsapi->setError(out, "VagueDenoiser: method must be set to 0, 1 or 2");
        return;
    }
    if (d.nsteps < 1) {
        vsapi->setError(out, "VagueDenoiser: nsteps must be greater than or equal to 1");
        return;
    }
    if (d.percent < 0.f || d.percent > 100.f) {
        vsapi->setError(out, "VagueDenoiser: percent must be between 0.0 and 100.0 inclusive");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bitsPerSample > 16) {
        vsapi->setError(out, "VagueDenoiser: only constant format 8-16 bits integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    const int m = vsapi->propNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        d.process[i] = m <= 0;

    for (int i = 0; i < m; i++) {
        const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (n < 0 || n >= d.vi->format->numPlanes) {
            vsapi->setError(out, "VagueDenoiser: plane index out of range");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.process[n]) {
            vsapi->setError(out, "VagueDenoiser: plane specified twice");
            vsapi->freeNode(d.node);
            return;
        }

        d.process[n] = true;
    }

    for (int plane = 0; plane < d.vi->format->numPlanes; plane++) {
        if (d.process[plane]) {
            const int width = d.vi->width >> (plane ? d.vi->format->subSamplingW : 0);
            const int height = d.vi->height >> (plane ? d.vi->format->subSamplingH : 0);

            if (width <= 4) {
                vsapi->setError(out, "VagueDenoiser: the plane's width must be greater than 4");
                vsapi->freeNode(d.node);
                return;
            }

            if (height <= 4) {
                vsapi->setError(out, "VagueDenoiser: the plane's height must be greater than 4");
                vsapi->freeNode(d.node);
                return;
            }
        }
    }

    const int nstepsWidth = d.vi->width >> (d.process[1] || d.process[2] ? d.vi->format->subSamplingW : 0);
    const int nstepsHeight = d.vi->height >> (d.process[1] || d.process[2] ? d.vi->format->subSamplingH : 0);
    int nstepsMax; // caclulate max possible nsteps
    for (nstepsMax = 1; nstepsMax < 15; nstepsMax++) {
        if (std::pow(2, nstepsMax) >= nstepsWidth || std::pow(2, nstepsMax) >= nstepsHeight)
            break;
    }
    nstepsMax -= 2;
    if (d.nsteps > nstepsMax)
        d.nsteps = nstepsMax;

    const float analysisLow[9] = {
        0.037828455506995f, -0.023849465019380f, -0.110624404418423f, 0.377402855612654f,
        0.852698679009403f, 0.377402855612654f, -0.110624404418423f, -0.023849465019380f, 0.037828455506995f
    };
    const float analysisHigh[7] = {
        -0.064538882628938f, 0.040689417609558f, 0.418092273222212f, -0.788485616405664f,
        0.418092273222212f, 0.040689417609558f, -0.064538882628938f
    };
    const float synthesisLow[7] = {
        -0.064538882628938f, -0.040689417609558f, 0.418092273222212f, 0.788485616405664f,
        0.418092273222212f, -0.040689417609558f, -0.064538882628938f
    };
    const float synthesisHigh[9] = {
        -0.037828455506995f, -0.023849465019380f, 0.110624404418423f, 0.377402855612654f,
        -0.852698679009403f, 0.377402855612654f, 0.110624404418423f, -0.023849465019380f, -0.037828455506995f
    };
    d.analysisLow = vs_aligned_malloc<float>(sizeof(analysisLow), 32);
    d.analysisHigh = vs_aligned_malloc<float>(sizeof(analysisHigh), 32);
    d.synthesisLow = vs_aligned_malloc<float>(sizeof(synthesisLow), 32);
    d.synthesisHigh = vs_aligned_malloc<float>(sizeof(synthesisHigh), 32);
    memcpy(d.analysisLow, analysisLow, sizeof(analysisLow));
    memcpy(d.analysisHigh, analysisHigh, sizeof(analysisHigh));
    memcpy(d.synthesisLow, synthesisLow, sizeof(synthesisLow));
    memcpy(d.synthesisHigh, synthesisHigh, sizeof(synthesisHigh));

    if (d.vi->format->bitsPerSample == 8) {
        d.byteToFloat = byteToFloat_8;
        d.floatToByte = floatToByte_8;
    } else {
        d.byteToFloat = byteToFloat_16;
        d.floatToByte = floatToByte_16;
    }

    if (d.method == 0)
        d.thresholding = hardThresholding;
    else if (d.method == 1)
        d.thresholding = softThresholding;
    else
        d.thresholding = qianThresholding;

    VagueDenoiserData * data = new VagueDenoiserData(d);

    vsapi->createFilter(in, out, "VagueDenoiser", vaguedenoiserInit, vaguedenoiserGetFrame, vaguedenoiserFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.vaguedenoiser", "vd", "A wavelet based denoiser", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("VagueDenoiser", "clip:clip;threshold:float:opt;method:int:opt;nsteps:int:opt;percent:float:opt;planes:int[]:opt;", vaguedenoiserCreate, nullptr, plugin);
}
