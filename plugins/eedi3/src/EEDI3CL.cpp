#ifdef HAVE_OPENCL
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

#include <clocale>
#include <cstdio>
#include <memory>
#include <string>

#include "EEDI3CL.hpp"
#include "EEDI3CL.cl"

template<typename T> extern void processCL_sse2(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3CLData *, const VSAPI *);

template<typename T>
static void processCL_c(const VSFrameRef * src, const VSFrameRef * scp, VSFrameRef * dst, VSFrameRef ** pad, const int field_n, const EEDI3CLData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        if (d->process[plane]) {
            copyPad<T>(src, pad[plane], plane, 1 - field_n, d->dh, vsapi);

            const int srcWidth = vsapi->getFrameWidth(pad[plane], 0);
            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int srcHeight = vsapi->getFrameHeight(pad[plane], 0);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int srcStride = vsapi->getStride(pad[plane], 0) / sizeof(T);
            const int dstStride = vsapi->getStride(dst, plane) / sizeof(T);
            const T * _srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(pad[plane], 0));
            T * VS_RESTRICT _dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

            const auto threadId = std::this_thread::get_id();
            auto queue = d->queue.at(threadId);
            auto calculateConnectionCosts = d->calculateConnectionCosts.at(threadId);
            auto srcImage = d->src.at(threadId);
            auto _ccosts = d->ccosts.at(threadId);
            float * pcosts = d->pcosts.at(threadId) + d->mdis;
            int * pbackt = d->pbackt.at(threadId) + d->mdis;
            int * fpath = d->fpath.at(threadId);
            int * _dmap = d->dmap.at(threadId);
            float * tline = d->tline.at(threadId);

            const size_t globalWorkSize[] = { static_cast<size_t>((dstWidth + 63) & -64), 1 };
            constexpr size_t localWorkSize[] = { 64, 1 };
            const int bufferSize = dstWidth * d->tpitch * sizeof(cl_float);

            vs_bitblt(_dstp + dstStride * (1 - field_n), vsapi->getStride(dst, plane) * 2,
                      _srcp + srcStride * (4 + 1 - field_n) + 12, vsapi->getStride(pad[plane], 0) * 2,
                      dstWidth * sizeof(T), dstHeight / 2);

            queue.enqueue_write_image(srcImage, compute::dim(0, 0), compute::dim(srcWidth, srcHeight), _srcp, vsapi->getStride(pad[plane], 0));

            for (int y = 4 + field_n; y < srcHeight - 4; y += 2) {
                const int off = (y - 4 - field_n) >> 1;
                const T * srcp = _srcp + srcStride * (4 + field_n + 2 * off) + 12;
                T * dstp = _dstp + dstStride * (field_n + 2 * off);
                int * dmap = _dmap + dstWidth * off;

                const T * src3p = srcp - srcStride * 3;
                const T * src1p = srcp - srcStride;
                const T * src1n = srcp + srcStride;
                const T * src3n = srcp + srcStride * 3;

                calculateConnectionCosts.set_args(srcImage, _ccosts, dstWidth, srcHeight - 4, y);
                queue.enqueue_nd_range_kernel(calculateConnectionCosts, 2, nullptr, globalWorkSize, localWorkSize);

                float * ccosts = reinterpret_cast<float *>(queue.enqueue_map_buffer(_ccosts, CL_MAP_READ, 0, bufferSize)) + d->mdis;

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

                interpolate<T>(src3p, src1p, src1n, src3n, fpath, dmap, dstp, dstWidth, d->ucubic, d->peak);

                queue.enqueue_unmap_buffer(_ccosts, ccosts - d->mdis);
            }

            if (d->vcheck) {
                const T * srcp = _srcp + srcStride * (4 + field_n) + 12;
                const T * scpp = nullptr;
                if (d->sclip)
                    scpp = reinterpret_cast<const T *>(vsapi->getReadPtr(scp, plane)) + dstStride * field_n;
                T * dstp = _dstp + dstStride * field_n;;

                vCheck<T>(srcp, scpp, dstp, _dmap, tline, field_n, dstWidth, srcHeight, srcStride, dstStride, d->vcheck, d->vthresh2, d->rcpVthresh0, d->rcpVthresh1, d->rcpVthresh2, d->peak);
            }
        }
    }
}

static void selectFunctions(const unsigned opt, EEDI3CLData * d) noexcept {
    d->vectorSize = 1;

    const int iset = instrset_detect();

    if ((opt == 0 && iset >= 2) || opt == 2)
        d->vectorSize = 4;

    if (d->vi.format->bytesPerSample == 1) {
        d->processor = processCL_c<uint8_t>;

        if ((opt == 0 && iset >= 2) || opt == 2)
            d->processor = processCL_sse2<uint8_t>;
    } else if (d->vi.format->bytesPerSample == 2) {
        d->processor = processCL_c<uint16_t>;

        if ((opt == 0 && iset >= 2) || opt == 2)
            d->processor = processCL_sse2<uint16_t>;
    } else {
        d->processor = processCL_c<float>;

        if ((opt == 0 && iset >= 2) || opt == 2)
            d->processor = processCL_sse2<float>;
    }
}

static void VS_CC eedi3clInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    EEDI3CLData * d = static_cast<EEDI3CLData *>(*instanceData);
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC eedi3clGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    EEDI3CLData * d = static_cast<EEDI3CLData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(d->field > 1 ? n / 2 : n, d->node, frameCtx);

        if (d->vcheck && d->sclip)
            vsapi->requestFrameFilter(n, d->sclip, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        no_subnormals();

        try {
            auto threadId = std::this_thread::get_id();

            if (!d->queue.count(threadId))
                d->queue.emplace(threadId, compute::command_queue{ d->ctx, d->gpu });

            if (!d->calculateConnectionCosts.count(threadId)) {
                if (d->vi.format->sampleType == stInteger)
                    d->calculateConnectionCosts.emplace(threadId, d->program.create_kernel("calculateConnectionCosts_uint"));
                else
                    d->calculateConnectionCosts.emplace(threadId, d->program.create_kernel("calculateConnectionCosts_float"));
            }

            if (!d->src.count(threadId))
                d->src.emplace(threadId, compute::image2d{ d->ctx, d->vi.width + 24U, d->vi.height + 8U, compute::image_format{ d->clImageFormat }, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY });

            if (!d->ccosts.count(threadId))
                d->ccosts.emplace(threadId, compute::buffer{ d->ctx, d->vi.width * d->tpitchVector * sizeof(cl_float), CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_HOST_READ_ONLY });

            if (!d->pcosts.count(threadId)) {
                float * pcosts = vs_aligned_malloc<float>(d->vi.width * d->tpitchVector * sizeof(float), 16);
                if (!pcosts)
                    throw std::string{ "malloc failure (pcosts)" };
                d->pcosts.emplace(threadId, pcosts);
            }

            if (!d->pbackt.count(threadId)) {
                int * pbackt = vs_aligned_malloc<int>(d->vi.width * d->tpitchVector * sizeof(int), 16);
                if (!pbackt)
                    throw std::string{ "malloc failure (pbackt)" };
                d->pbackt.emplace(threadId, pbackt);
            }

            if (!d->fpath.count(threadId)) {
                int * fpath = new (std::nothrow) int[d->vi.width * d->vectorSize];
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
            vsapi->setFilterError(("EEDI3CL: " + error).c_str(), frameCtx);
            return nullptr;
        } catch (const compute::opencl_error & error) {
            vsapi->setFilterError(("EEDI3CL: " + error.error_string()).c_str(), frameCtx);
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

        try {
            d->processor(src, scp, dst, pad, field_n, d, vsapi);
        } catch (const compute::opencl_error & error) {
            vsapi->setFilterError(("EEDI3CL: " + error.error_string()).c_str(), frameCtx);
            vsapi->freeFrame(src);
            vsapi->freeFrame(scp);
            vsapi->freeFrame(dst);
            for (int plane = 0; plane < d->vi.format->numPlanes; plane++)
                vsapi->freeFrame(pad[plane]);
            return nullptr;
        }

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

static void VS_CC eedi3clFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    EEDI3CLData * d = static_cast<EEDI3CLData *>(instanceData);

    vsapi->freeNode(d->node);
    vsapi->freeNode(d->sclip);

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

void VS_CC eedi3clCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<EEDI3CLData> d{ new EEDI3CLData{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->sclip = vsapi->propGetNode(in, "sclip", 0, &err);
    d->vi = *vsapi->getVideoInfo(d->node);

    try {
        if (!isConstantFormat(&d->vi) || (d->vi.format->sampleType == stInteger && d->vi.format->bitsPerSample > 16) ||
            (d->vi.format->sampleType == stFloat && d->vi.format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bit integer and 32 bit float input supported" };

        d->field = int64ToIntS(vsapi->propGetInt(in, "field", 0, nullptr));

        d->dh = !!vsapi->propGetInt(in, "dh", 0, &err);

        const int m = vsapi->propNumElements(in, "planes");

        for (int i = 0; i < 3; i++)
            d->process[i] = (m <= 0);

        for (int i = 0; i < m; i++) {
            const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

            if (n < 0 || n >= d->vi.format->numPlanes)
                throw std::string{ "plane index out of range" };

            if (d->process[n])
                throw std::string{ "plane specified twice" };

            d->process[n] = true;
        }

        float alpha = static_cast<float>(vsapi->propGetFloat(in, "alpha", 0, &err));
        if (err)
            alpha = 0.2f;

        float beta = static_cast<float>(vsapi->propGetFloat(in, "beta", 0, &err));
        if (err)
            beta = 0.25f;

        d->gamma = static_cast<float>(vsapi->propGetFloat(in, "gamma", 0, &err));
        if (err)
            d->gamma = 20.f;

        int nrad = int64ToIntS(vsapi->propGetInt(in, "nrad", 0, &err));
        if (err)
            nrad = 2;

        d->mdis = int64ToIntS(vsapi->propGetInt(in, "mdis", 0, &err));
        if (err)
            d->mdis = 20;

        d->ucubic = !!vsapi->propGetInt(in, "ucubic", 0, &err);
        if (err)
            d->ucubic = true;

        int cost3 = !!vsapi->propGetInt(in, "cost3", 0, &err);
        if (err)
            cost3 = 1;

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

        int device = int64ToIntS(vsapi->propGetInt(in, "device", 0, &err));
        if (err)
            device = -1;

        if (d->field < 0 || d->field > 3)
            throw std::string{ "field must be 0, 1, 2 or 3" };

        if (!d->dh && (d->vi.height & 1))
            throw std::string{ "height must be mod 2 when dh=False" };

        if (d->dh && d->field > 1)
            throw std::string{ "field must be 0 or 1 when dh=True" };

        if (alpha < 0.f || alpha > 1.f)
            throw std::string{ "alpha must be between 0.0 and 1.0 (inclusive)" };

        if (beta < 0.f || beta > 1.f)
            throw std::string{ "beta must be between 0.0 and 1.0 (inclusive)" };

        if (alpha + beta > 1.f)
            throw std::string{ "alpha+beta must be between 0.0 and 1.0 (inclusive)" };

        if (d->gamma < 0.f)
            throw std::string{ "gamma must be greater than or equal to 0.0" };

        if (nrad < 0 || nrad > 3)
            throw std::string{ "nrad must be between 0 and 3 (inclusive)" };

        if (d->mdis < 1 || d->mdis > 40)
            throw std::string{ "mdis must be between 1 and 40 (inclusive)" };

        if (d->vcheck < 0 || d->vcheck > 3)
            throw std::string{ "vcheck must be 0, 1, 2 or 3" };

        if (d->vcheck && (vthresh0 <= 0.f || vthresh1 <= 0.f || d->vthresh2 <= 0.f))
            throw std::string{ "vthresh0, vthresh1 and vthresh2 must be greater than 0.0" };

        if (opt < 0 || opt > 2)
            throw std::string{ "opt must be 0, 1 or 2" };

        if (device >= static_cast<int>(compute::system::device_count()))
            throw std::string{ "device index out of range" };

        if (!!vsapi->propGetInt(in, "list_device", 0, &err)) {
            vsapi->freeNode(d->sclip);

            const auto devices = compute::system::devices();
            std::string text;

            for (size_t i = 0; i < devices.size(); i++)
                text += std::to_string(i) + ": " + devices[i].name() + " (" + devices[i].platform().name() + ")" + "\n";

            VSMap * args = vsapi->createMap();
            vsapi->propSetNode(args, "clip", d->node, paReplace);
            vsapi->freeNode(d->node);
            vsapi->propSetData(args, "text", text.c_str(), -1, paReplace);

            VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.text", core), "Text", args);
            if (vsapi->getError(ret)) {
                vsapi->setError(out, vsapi->getError(ret));
                vsapi->freeMap(args);
                vsapi->freeMap(ret);
                return;
            }

            d->node = vsapi->propGetNode(ret, "clip", 0, nullptr);
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
            vsapi->propSetNode(out, "clip", d->node, paReplace);
            vsapi->freeNode(d->node);
            return;
        }

        if (d->field > 1) {
            if (d->vi.numFrames > INT_MAX / 2)
                throw std::string{ "resulting clip is too long" };
            d->vi.numFrames *= 2;

            muldivRational(&d->vi.fpsNum, &d->vi.fpsDen, 2, 1);
        }

        if (d->dh)
            d->vi.height *= 2;

        const float remainingWeight = 1.f - alpha - beta;

        if (cost3)
            alpha /= 3.f;

        if (d->vcheck && d->sclip) {
            if (!isSameFormat(vsapi->getVideoInfo(d->sclip), &d->vi))
                throw std::string{ "sclip must have the same dimensions as main clip and be the same format" };

            if (vsapi->getVideoInfo(d->sclip)->numFrames != d->vi.numFrames)
                throw std::string{ "sclip's number of frames doesn't match" };
        }

        const unsigned numThreads = vsapi->getCoreInfo(core)->numThreads;
        d->queue.reserve(numThreads);
        d->calculateConnectionCosts.reserve(numThreads);
        d->src.reserve(numThreads);
        d->ccosts.reserve(numThreads);
        d->pcosts.reserve(numThreads);
        d->pbackt.reserve(numThreads);
        d->fpath.reserve(numThreads);
        d->dmap.reserve(numThreads);
        d->tline.reserve(numThreads);

        if (d->vi.format->sampleType == stInteger) {
            d->peak = (1 << d->vi.format->bitsPerSample) - 1;
            const float scale = d->peak / 255.f;
            beta *= scale;
            d->gamma *= scale;
            vthresh0 *= scale;
            vthresh1 *= scale;
        } else {
            beta /= 255.f;
            d->gamma /= 255.f;
            vthresh0 /= 255.f;
            vthresh1 /= 255.f;
        }

        selectFunctions(opt, d.get());

        d->tpitch = d->mdis * 2 + 1;
        d->mdisVector = d->mdis * d->vectorSize;
        d->tpitchVector = d->tpitch * d->vectorSize;

        d->rcpVthresh0 = 1.f / vthresh0;
        d->rcpVthresh1 = 1.f / vthresh1;
        d->rcpVthresh2 = 1.f / d->vthresh2;

        d->gpu = compute::system::default_device();
        if (device > -1)
            d->gpu = compute::system::devices().at(device);
        d->ctx = compute::context{ d->gpu };

        if (!!vsapi->propGetInt(in, "info", 0, &err)) {
            vsapi->freeNode(d->sclip);

            std::string text{ "=== Device Info ===\n" };
            text += "Name: " + d->gpu.get_info<CL_DEVICE_NAME>() + "\n";
            text += "Vendor: " + d->gpu.get_info<CL_DEVICE_VENDOR>() + "\n";
            text += "Profile: " + d->gpu.get_info<CL_DEVICE_PROFILE>() + "\n";
            text += "Version: " + d->gpu.get_info<CL_DEVICE_VERSION>() + "\n";
            text += "Global Memory Size: " + std::to_string(d->gpu.get_info<CL_DEVICE_GLOBAL_MEM_SIZE>() / 1024 / 1024) + " MB\n";
            text += "Local Memory Size: " + std::to_string(d->gpu.get_info<CL_DEVICE_LOCAL_MEM_SIZE>() / 1024) + " KB\n";
            text += "Local Memory Type: " + std::string{ d->gpu.get_info<CL_DEVICE_LOCAL_MEM_TYPE>() == CL_LOCAL ? "CL_LOCAL" : "CL_GLOBAL" } +"\n";
            text += "Image Support: " + std::string{ d->gpu.get_info<CL_DEVICE_IMAGE_SUPPORT>() ? "CL_TRUE" : "CL_FALSE" } +"\n";
            text += "1D Image Max Buffer Size: " + std::to_string(d->gpu.get_info<size_t>(CL_DEVICE_IMAGE_MAX_BUFFER_SIZE)) + "\n";
            text += "2D Image Max Width: " + std::to_string(d->gpu.get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>()) + "\n";
            text += "2D Image Max Height: " + std::to_string(d->gpu.get_info<CL_DEVICE_IMAGE2D_MAX_HEIGHT>()) + "\n";
            text += "Max Constant Arguments: " + std::to_string(d->gpu.get_info<CL_DEVICE_MAX_CONSTANT_ARGS>()) + "\n";
            text += "Max Constant Buffer Size: " + std::to_string(d->gpu.get_info<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() / 1024) + " KB\n";
            text += "Max Work-group Size: " + std::to_string(d->gpu.get_info<CL_DEVICE_MAX_WORK_GROUP_SIZE>()) + "\n";
            const auto MAX_WORK_ITEM_SIZES = d->gpu.get_info<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
            text += "Max Work-item Sizes: (" + std::to_string(MAX_WORK_ITEM_SIZES[0]) + ", " + std::to_string(MAX_WORK_ITEM_SIZES[1]) + ", " + std::to_string(MAX_WORK_ITEM_SIZES[2]) + ")";

            VSMap * args = vsapi->createMap();
            vsapi->propSetNode(args, "clip", d->node, paReplace);
            vsapi->freeNode(d->node);
            vsapi->propSetData(args, "text", text.c_str(), -1, paReplace);

            VSMap * ret = vsapi->invoke(vsapi->getPluginById("com.vapoursynth.text", core), "Text", args);
            if (vsapi->getError(ret)) {
                vsapi->setError(out, vsapi->getError(ret));
                vsapi->freeMap(args);
                vsapi->freeMap(ret);
                return;
            }

            d->node = vsapi->propGetNode(ret, "clip", 0, nullptr);
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
            vsapi->propSetNode(out, "clip", d->node, paReplace);
            vsapi->freeNode(d->node);
            return;
        }

        try {
            std::setlocale(LC_ALL, "C");
            char buf[100];
            std::string options{ "-cl-denorms-are-zero -cl-fast-relaxed-math -Werror" };
            std::snprintf(buf, 100, "%.20ff", alpha);
            options += " -D ALPHA=" + std::string{ buf };
            std::snprintf(buf, 100, "%.20ff", beta);
            options += " -D BETA=" + std::string{ buf };
            options += " -D NRAD=" + std::to_string(nrad);
            options += " -D MDIS=" + std::to_string(d->mdis);
            options += " -D COST3=" + std::to_string(cost3);
            std::snprintf(buf, 100, "%.20ff", remainingWeight);
            options += " -D REMAINING_WEIGHT=" + std::string{ buf };
            options += " -D TPITCH=" + std::to_string(d->tpitch);
            options += " -D VECTOR_SIZE=" + std::to_string(d->vectorSize);
            options += " -D MDIS_VECTOR=" + std::to_string(d->mdisVector);
            options += " -D TPITCH_VECTOR=" + std::to_string(d->tpitchVector);
            options += " -D LOCAL_WORK_SIZE_X=" + std::to_string(64 / d->vectorSize);
            options += " -D LOCAL_WORK_SIZE_Y=" + std::to_string(d->vectorSize);
            std::setlocale(LC_ALL, "");
            d->program = compute::program::build_with_source(source, d->ctx, options);
        } catch (const compute::opencl_error & error) {
            throw error.error_string() + "\n" + d->program.build_log();
        }

        if (d->vi.format->bytesPerSample == 1)
            d->clImageFormat = { CL_R, CL_UNSIGNED_INT8 };
        else if (d->vi.format->bytesPerSample == 2)
            d->clImageFormat = { CL_R, CL_UNSIGNED_INT16 };
        else
            d->clImageFormat = { CL_R, CL_FLOAT };
    } catch (const std::string & error) {
        vsapi->setError(out, ("EEDI3CL: " + error).c_str());
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->sclip);
        return;
    } catch (const compute::no_device_found & error) {
        vsapi->setError(out, (std::string{ "EEDI3CL: " } + error.what()).c_str());
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->sclip);
        return;
    } catch (const compute::opencl_error & error) {
        vsapi->setError(out, ("EEDI3CL: " + error.error_string()).c_str());
        vsapi->freeNode(d->node);
        vsapi->freeNode(d->sclip);
        return;
    }

    vsapi->createFilter(in, out, "EEDI3CL", eedi3clInit, eedi3clGetFrame, eedi3clFree, fmParallel, 0, d.release(), core);
}
#endif
