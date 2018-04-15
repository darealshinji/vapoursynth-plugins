/*
    MIT License

    Copyright (c) 2017 HolyWu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include <VapourSynth.h>
#include <VSHelper.h>

#include <fftw3.h>

struct DCTFilterData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    bool process[3];
    int peak;
    float factors[64];
    fftwf_plan dct, idct;
    std::unordered_map<std::thread::id, float *> buffer;
};

template<typename T>
static void process(const VSFrameRef * src, VSFrameRef * dst, const DCTFilterData * d, const VSAPI * vsapi) noexcept {
    const auto threadId = std::this_thread::get_id();
    float * VS_RESTRICT buffer = d->buffer.at(threadId);

    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int stride = vsapi->getStride(src, plane) / sizeof(T);
            const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
            T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

            for (int y = 0; y < height; y += 8) {
                for (int x = 0; x < width; x += 8) {
                    for (int yy = 0; yy < 8; yy++) {
                        const T * input = srcp + stride * yy + x;
                        float * VS_RESTRICT output = buffer + 8 * yy;

                        for (int xx = 0; xx < 8; xx++)
                            output[xx] = input[xx] * (1.f / 256.f);
                    }

                    fftwf_execute_r2r(d->dct, buffer, buffer);

                    for (int i = 0; i < 64; i++)
                        buffer[i] *= d->factors[i];

                    fftwf_execute_r2r(d->idct, buffer, buffer);

                    for (int yy = 0; yy < 8; yy++) {
                        const float * input = buffer + 8 * yy;
                        T * VS_RESTRICT output = dstp + stride * yy + x;

                        for (int xx = 0; xx < 8; xx++) {
                            if (std::is_integral<T>::value)
                                output[xx] = std::min(std::max(static_cast<int>(input[xx] + 0.5f), 0), d->peak);
                            else
                                output[xx] = input[xx];
                        }
                    }
                }

                srcp += stride * 8;
                dstp += stride * 8;
            }
        }
    }
}

static void VS_CC dctfilterInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DCTFilterData * d = static_cast<DCTFilterData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC dctfilterGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DCTFilterData * d = static_cast<DCTFilterData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        try {
            auto threadId = std::this_thread::get_id();

            if (!d->buffer.count(threadId)) {
                float * buffer = fftwf_alloc_real(64);
                if (!buffer)
                    throw std::string{ "malloc failure (buffer)" };
                d->buffer.emplace(threadId, buffer);
            }
        } catch (const std::string & error) {
            vsapi->setFilterError(("DCTFilter: " + error).c_str(), frameCtx);
            return nullptr;
        }

        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        if (d->vi->format->bytesPerSample == 1)
            process<uint8_t>(src, dst, d, vsapi);
        else if (d->vi->format->bytesPerSample == 2)
            process<uint16_t>(src, dst, d, vsapi);
        else
            process<float>(src, dst, d, vsapi);

        vsapi->freeFrame(src);
        return dst;
    }

    return nullptr;
}

static void VS_CC dctfilterFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DCTFilterData * d = static_cast<DCTFilterData *>(instanceData);

    vsapi->freeNode(d->node);

    fftwf_destroy_plan(d->dct);
    fftwf_destroy_plan(d->idct);

    for (auto & iter : d->buffer)
        fftwf_free(iter.second);

    delete d;
}

static void VS_CC dctfilterCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<DCTFilterData> d{ new DCTFilterData{} };

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    const int padWidth = (d->vi->width & 15) ? 16 - d->vi->width % 16 : 0;
    const int padHeight = (d->vi->height & 15) ? 16 - d->vi->height % 16 : 0;

    try {
        if (!isConstantFormat(d->vi) || (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bit integer and 32 bit float input supported" };

        const double * factors = vsapi->propGetFloatArray(in, "factors", nullptr);

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

        if (vsapi->propNumElements(in, "factors") != 8)
            throw std::string{ "the number of factors must be 8" };

        for (int i = 0; i < 8; i++) {
            if (factors[i] < 0. || factors[i] > 1.)
                throw std::string{ "factor must be between 0.0 and 1.0 (inclusive)" };
        }

        const unsigned numThreads = vsapi->getCoreInfo(core)->numThreads;
        d->buffer.reserve(numThreads);

        if (d->vi->format->sampleType == stInteger)
            d->peak = (1 << d->vi->format->bitsPerSample) - 1;

        if (padWidth || padHeight) {
            VSMap * args = vsapi->createMap();
            vsapi->propSetNode(args, "clip", d->node, paReplace);
            vsapi->freeNode(d->node);
            vsapi->propSetInt(args, "width", d->vi->width + padWidth, paReplace);
            vsapi->propSetInt(args, "height", d->vi->height + padHeight, paReplace);
            vsapi->propSetFloat(args, "src_width", d->vi->width + padWidth, paReplace);
            vsapi->propSetFloat(args, "src_height", d->vi->height + padHeight, paReplace);

            VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.resize", core), "Point", args);
            if (vsapi->getError(ret)) {
                vsapi->setError(out, vsapi->getError(ret));
                vsapi->freeMap(args);
                vsapi->freeMap(ret);
                return;
            }

            d->node = vsapi->propGetNode(ret, "clip", 0, nullptr);
            d->vi = vsapi->getVideoInfo(d->node);
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
        }

        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++)
                d->factors[8 * y + x] = static_cast<float>(factors[y] * factors[x]);
        }

        float * buffer = fftwf_alloc_real(64);
        if (!buffer)
            throw std::string{ "malloc failure (buffer)" };

        d->dct = fftwf_plan_r2r_2d(8, 8, buffer, buffer, FFTW_REDFT10, FFTW_REDFT10, FFTW_PATIENT);
        d->idct = fftwf_plan_r2r_2d(8, 8, buffer, buffer, FFTW_REDFT01, FFTW_REDFT01, FFTW_PATIENT);

        fftwf_free(buffer);
    } catch (const std::string & error) {
        vsapi->setError(out, ("DCTFilter: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "DCTFilter", dctfilterInit, dctfilterGetFrame, dctfilterFree, fmParallel, 0, d.release(), core);

    if (padWidth || padHeight) {
        VSNodeRef * node = vsapi->propGetNode(out, "clip", 0, nullptr);
        vsapi->clearMap(out);

        VSMap * args = vsapi->createMap();
        vsapi->propSetNode(args, "clip", node, paReplace);
        vsapi->freeNode(node);
        vsapi->propSetInt(args, "right", padWidth, paReplace);
        vsapi->propSetInt(args, "bottom", padHeight, paReplace);

        VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.std", core), "Crop", args);
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
    configFunc("com.holywu.dctfilter", "dctf", "DCT/IDCT Frequency Suppressor", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("DCTFilter",
                 "clip:clip;"
                 "factors:float[];"
                 "planes:int[]:opt;",
                 dctfilterCreate, nullptr, plugin);
}
