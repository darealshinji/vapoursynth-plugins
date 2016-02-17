/*
  The MIT License (MIT)

  Copyright (c) 2015 HolyWu

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

#include <cmath>
#include <string>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>
#include <w2xconv.h>

struct Waifu2xData {
    VSNodeRef * node;
    VSVideoInfo vi;
    int noise, scale, block;
    int iterTimesTwiceScaling;
    float * srcInterleaved, * dstInterleaved, * buffer;
    W2XConv * conv;
};

static inline bool isPowerOf2(const int i) {
    return i && !(i & (i - 1));
}

static bool Waifu2x(const VSFrameRef * src, VSFrameRef * dst, Waifu2xData * VS_RESTRICT d, const VSAPI * vsapi) {
    if (d->vi.format->colorFamily == cmRGB) {
        const int width = vsapi->getFrameWidth(src, 0);
        const int height = vsapi->getFrameHeight(src, 0);
        const int srcStride = vsapi->getStride(src, 0) / sizeof(float);
        const int dstStride = vsapi->getStride(dst, 0) / sizeof(float);
        const float * srcpR = reinterpret_cast<const float *>(vsapi->getReadPtr(src, 0));
        const float * srcpG = reinterpret_cast<const float *>(vsapi->getReadPtr(src, 1));
        const float * srcpB = reinterpret_cast<const float *>(vsapi->getReadPtr(src, 2));
        float * VS_RESTRICT dstpR = reinterpret_cast<float *>(vsapi->getWritePtr(dst, 0));
        float * VS_RESTRICT dstpG = reinterpret_cast<float *>(vsapi->getWritePtr(dst, 1));
        float * VS_RESTRICT dstpB = reinterpret_cast<float *>(vsapi->getWritePtr(dst, 2));

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int pos = y * width + x;
                d->srcInterleaved[pos * 3] = srcpR[x];
                d->srcInterleaved[pos * 3 + 1] = srcpG[x];
                d->srcInterleaved[pos * 3 + 2] = srcpB[x];
            }
            srcpR += srcStride;
            srcpG += srcStride;
            srcpB += srcStride;
        }

        if (w2xconv_convert_rgb_f32(d->conv, reinterpret_cast<unsigned char *>(d->dstInterleaved), d->vi.width * 3 * sizeof(float),
                                    reinterpret_cast<unsigned char *>(d->srcInterleaved), width * 3 * sizeof(float), width, height, d->noise, d->scale, d->block) < 0)
            return false;

        for (int y = 0; y < d->vi.height; y++) {
            for (int x = 0; x < d->vi.width; x++) {
                const int pos = y * d->vi.width + x;
                dstpR[x] = d->dstInterleaved[pos * 3];
                dstpG[x] = d->dstInterleaved[pos * 3 + 1];
                dstpB[x] = d->dstInterleaved[pos * 3 + 2];
            }
            dstpR += dstStride;
            dstpG += dstStride;
            dstpB += dstStride;
        }
    } else {
        for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
            const int srcWidth = vsapi->getFrameWidth(src, plane);
            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int srcHeight = vsapi->getFrameHeight(src, plane);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int srcStride = vsapi->getStride(src, plane) / sizeof(float);
            const int dstStride = vsapi->getStride(dst, plane) / sizeof(float);
            const float * srcp = reinterpret_cast<const float *>(vsapi->getReadPtr(src, plane));
            float * VS_RESTRICT dstp = reinterpret_cast<float *>(vsapi->getWritePtr(dst, plane));

            if (plane != 0) {
                const float * input = srcp;

                if (d->noise == 0) {
                    float * VS_RESTRICT output = dstp;
                    for (int y = 0; y < srcHeight; y++) {
                        for (int x = 0; x < srcWidth; x++)
                            output[x] = input[x] + 0.5f;
                        input += srcStride;
                        output += dstStride;
                    }
                } else {
                    float * VS_RESTRICT output = d->buffer;
                    for (int y = 0; y < srcHeight; y++) {
                        for (int x = 0; x < srcWidth; x++)
                            output[x] = input[x] + 0.5f;
                        input += srcStride;
                        output += srcWidth;
                    }
                }
            }

            if (d->noise != 0) {
                if (plane == 0) {
                    if (w2xconv_apply_filter_y(d->conv, static_cast<W2XConvFilterType>(d->noise - 1), reinterpret_cast<unsigned char *>(dstp), vsapi->getStride(dst, plane),
                                               const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(srcp)), vsapi->getStride(src, plane), srcWidth, srcHeight, d->block) < 0)
                        return false;
                } else {
                    if (w2xconv_apply_filter_y(d->conv, static_cast<W2XConvFilterType>(d->noise - 1), reinterpret_cast<unsigned char *>(dstp), vsapi->getStride(dst, plane),
                                               reinterpret_cast<unsigned char *>(d->buffer), srcWidth * sizeof(float), srcWidth, srcHeight, d->block) < 0)
                        return false;
                }
            }

            if (d->scale != 1) {
                if (d->noise == 0 && plane == 0)
                    vs_bitblt(dstp, vsapi->getStride(dst, plane), srcp, vsapi->getStride(src, plane), srcWidth * sizeof(float), srcHeight);

                for (int times = 0; times < d->iterTimesTwiceScaling; times++) {
                    const int currentWidth = srcWidth << times;
                    const int currentHeight = srcHeight << times;
                    const int currentWidth2 = currentWidth * 2;
                    const int currentHeight2 = currentHeight * 2;
                    const float * input = dstp;

                    for (int y = 0; y < currentHeight; y++) {
                        for (int x = 0; x < currentWidth; x++) {
                            const int pos = y * 2 * currentWidth2 + x * 2;
                            d->buffer[pos] = input[x];
                            d->buffer[pos + 1] = input[x];
                            d->buffer[pos + currentWidth2] = input[x];
                            d->buffer[pos + currentWidth2 + 1] = input[x];
                        }
                        input += dstStride;
                    }

                    if (w2xconv_apply_filter_y(d->conv, W2XCONV_FILTER_SCALE2x, reinterpret_cast<unsigned char *>(dstp), vsapi->getStride(dst, plane),
                                               reinterpret_cast<unsigned char *>(d->buffer), currentWidth2 * sizeof(float), currentWidth2, currentHeight2, d->block) < 0)
                        return false;
                }
            }

            if (plane != 0) {
                for (int y = 0; y < dstHeight; y++) {
                    for (int x = 0; x < dstWidth; x++)
                        dstp[x] -= 0.5f;
                    dstp += dstStride;
                }
            }
        }
    }

    return true;
}

static void VS_CC waifu2xInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    Waifu2xData * d = static_cast<Waifu2xData *>(*instanceData);
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC waifu2xGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    Waifu2xData * d = static_cast<Waifu2xData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);

        if (!Waifu2x(src, dst, d, vsapi)) {
            char * err = w2xconv_strerror(&d->conv->last_error);
            vsapi->setFilterError(std::string("Waifu2x-w2xc: ").append(err).c_str(), frameCtx);
            vsapi->freeFrame(src);
            vsapi->freeFrame(dst);
            w2xconv_free(err);
            return nullptr;
        }

        vsapi->freeFrame(src);
        return dst;
    }

    return nullptr;
}

static void VS_CC waifu2xFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    Waifu2xData * d = static_cast<Waifu2xData *>(instanceData);
    vsapi->freeNode(d->node);
    vs_aligned_free(d->srcInterleaved);
    vs_aligned_free(d->dstInterleaved);
    vs_aligned_free(d->buffer);
    w2xconv_fini(d->conv);
    delete d;
}

static void VS_CC waifu2xCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    Waifu2xData d = {};
    int err;

    d.noise = int64ToIntS(vsapi->propGetInt(in, "noise", 0, &err));
    if (err)
        d.noise = 1;

    d.scale = int64ToIntS(vsapi->propGetInt(in, "scale", 0, &err));
    if (err)
        d.scale = 2;

    d.block = int64ToIntS(vsapi->propGetInt(in, "block", 0, &err));
    if (err)
        d.block = 512;

    const bool photo = !!vsapi->propGetInt(in, "photo", 0, &err);

    int processor = int64ToIntS(vsapi->propGetInt(in, "processor", 0, &err));
    if (err)
        processor = -1;

    W2XConvGPUMode gpu = static_cast<W2XConvGPUMode>(int64ToIntS(vsapi->propGetInt(in, "gpu", 0, &err)));
    if (err)
        gpu = W2XCONV_GPU_AUTO;

    const bool log = !!vsapi->propGetInt(in, "log", 0, &err);

    int numProcessors;
    const W2XConvProcessor * processors = w2xconv_get_processor_list(&numProcessors);

    if (d.noise < 0 || d.noise > 2) {
        vsapi->setError(out, "Waifu2x-w2xc: noise must be set to 0, 1 or 2");
        return;
    }

    if (d.scale < 1 || !isPowerOf2(d.scale)) {
        vsapi->setError(out, "Waifu2x-w2xc: scale must be greater than or equal to 1 and be a power of 2");
        return;
    }

    if (d.block < 1) {
        vsapi->setError(out, "Waifu2x-w2xc: block must be greater than or equal to 1");
        return;
    }

    if (processor >= numProcessors) {
        vsapi->setError(out, "Waifu2x-w2xc: selected processor is not available");
        return;
    }

    if (gpu < 0 || gpu > 2) {
        vsapi->setError(out, "Waifu2x-w2xc: gpu must be set to 0, 1 or 2");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = *vsapi->getVideoInfo(d.node);

    if (!!vsapi->propGetInt(in, "list_proc", 0, &err)) {
        std::string text;

        for (int i = 0; i < numProcessors; i++) {
            const W2XConvProcessor * p = &processors[i];
            const char * type;

            switch (p->type) {
            case W2XCONV_PROC_HOST:
                switch (p->sub_type) {
                case W2XCONV_PROC_HOST_FMA:
                    type = "FMA";
                    break;
                case W2XCONV_PROC_HOST_AVX:
                    type = "AVX";
                    break;
                case W2XCONV_PROC_HOST_SSE3:
                    type = "SSE3";
                    break;
                default:
                    type = "OpenCV";
                    break;
                }
                break;

            case W2XCONV_PROC_CUDA:
                type = "CUDA";
                break;

            case W2XCONV_PROC_OPENCL:
                type = "OpenCL";
                break;

            default:
                type = "??";
                break;
            }

            text = text.append(std::to_string(i)).append(": ").append(p->dev_name).append(" (").append(type).append(")\n");
        }

        VSMap * args = vsapi->createMap();
        vsapi->propSetNode(args, "clip", d.node, paReplace);
        vsapi->freeNode(d.node);
        vsapi->propSetData(args, "text", text.c_str(), -1, paReplace);

        VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.text", core), "Text", args);
        if (vsapi->getError(ret)) {
            vsapi->setError(out, vsapi->getError(ret));
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
            return;
        }

        d.node = vsapi->propGetNode(ret, "clip", 0, nullptr);
        vsapi->freeMap(args);
        vsapi->freeMap(ret);
        vsapi->propSetNode(out, "clip", d.node, paReplace);
        vsapi->freeNode(d.node);
        return;
    } else if (d.noise == 0 && d.scale == 1) {
        vsapi->propSetNode(out, "clip", d.node, paReplace);
        vsapi->freeNode(d.node);
        return;
    }

    if (!isConstantFormat(&d.vi) || d.vi.format->sampleType != stFloat || d.vi.format->bitsPerSample != 32) {
        vsapi->setError(out, "Waifu2x-w2xc: only constant format 32-bit float input supported");
        vsapi->freeNode(d.node);
        return;
    }

    VSPlugin * fmtcPlugin = vsapi->getPluginById("fmtconv", core);
    if (d.scale != 1 && d.vi.format->subSamplingW != 0 && !fmtcPlugin) {
        vsapi->setError(out, "Waifu2x-w2xc: the fmtconv plugin is required for fixing horizontal chroma shift");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.scale != 1) {
        d.vi.width *= d.scale;
        d.vi.height *= d.scale;
        d.iterTimesTwiceScaling = static_cast<int>(std::log2(d.scale));
    }

    if (d.vi.format->colorFamily == cmRGB) {
        d.srcInterleaved = vs_aligned_malloc<float>(vsapi->getVideoInfo(d.node)->width * vsapi->getVideoInfo(d.node)->height * 3 * sizeof(float), 32);
        d.dstInterleaved = vs_aligned_malloc<float>(d.vi.width * d.vi.height * 3 * sizeof(float), 32);
        if (!d.srcInterleaved || !d.dstInterleaved) {
            vsapi->setError(out, "Waifu2x-w2xc: malloc failure (srcInterleaved/dstInterleaved)");
            vsapi->freeNode(d.node);
            return;
        }
    } else {
        d.buffer = vs_aligned_malloc<float>(d.vi.width * d.vi.height * sizeof(float), 32);
        if (!d.buffer) {
            vsapi->setError(out, "Waifu2x-w2xc: malloc failure (buffer)");
            vsapi->freeNode(d.node);
            return;
        }
    }

    if (processor > -1)
        d.conv = w2xconv_init_with_processor(processor, 0, log);
    else
        d.conv = w2xconv_init(gpu, 0, log);

    const std::string pluginPath(vsapi->getPluginPath(vsapi->getPluginById("com.holywu.waifu2x-w2xc", core)));
    std::string modelPath(pluginPath.substr(0, pluginPath.find_last_of('/')));
    if (d.vi.format->colorFamily == cmRGB) {
        if (photo)
            modelPath = modelPath.append("/models/photo");
        else
            modelPath = modelPath.append("/models/anime_style_art_rgb");
    } else {
        modelPath = modelPath.append("/models/anime_style_art");
    }

    if (w2xconv_load_models(d.conv, modelPath.c_str()) < 0) {
        char * err = w2xconv_strerror(&d.conv->last_error);
        vsapi->setError(out, std::string("Waifu2x-w2xc: ").append(err).c_str());
        vsapi->freeNode(d.node);
        w2xconv_free(err);
        w2xconv_fini(d.conv);
        return;
    }

    Waifu2xData * data = new Waifu2xData(d);

    vsapi->createFilter(in, out, "Waifu2x-w2xc", waifu2xInit, waifu2xGetFrame, waifu2xFree, fmParallelRequests, 0, data, core);

    if (d.scale != 1 && d.vi.format->subSamplingW != 0) {
        const double offset = 0.5 * (1 << d.vi.format->subSamplingW) - 0.5;
        double shift = 0.;
        for (int times = 0; times < d.iterTimesTwiceScaling; times++)
            shift = shift * 2. + offset;

        VSNodeRef * node = vsapi->propGetNode(out, "clip", 0, nullptr);
        vsapi->clearMap(out);
        VSMap * args = vsapi->createMap();
        vsapi->propSetNode(args, "clip", node, paReplace);
        vsapi->freeNode(node);
        vsapi->propSetFloat(args, "sx", shift, paReplace);
        vsapi->propSetFloat(args, "planes", 2, paReplace);
        vsapi->propSetFloat(args, "planes", 3, paAppend);
        vsapi->propSetFloat(args, "planes", 3, paAppend);

        VSMap * ret = vsapi->invoke(fmtcPlugin, "resample", args);
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
    configFunc("com.holywu.waifu2x-w2xc", "w2xc", "Image Super-Resolution using Deep Convolutional Neural Networks", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Waifu2x",
                 "clip:clip;"
                 "noise:int:opt;"
                 "scale:int:opt;"
                 "block:int:opt;"
                 "photo:int:opt;"
                 "processor:int:opt;"
                 "gpu:int:opt;"
                 "list_proc:int:opt;"
                 "log:int:opt;",
                 waifu2xCreate, nullptr, plugin);
}
