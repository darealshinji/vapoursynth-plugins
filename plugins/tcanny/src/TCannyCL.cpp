#ifdef HAVE_OPENCL
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

#include "TCanny.hpp"
#include "TCanny.cl"

#include <boost/compute/core.hpp>
namespace compute = boost::compute;

struct TCannyCLData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    int mode;
    bool process[3];
    compute::buffer horizontalWeights[3], verticalWeights[3];
    int horizontalRadius[3], verticalRadius[3];
    unsigned peak;
    float offset[3], lower[3], upper[3];
    compute::command_queue queue;
    compute::kernel copyPlane, gaussianBlurH, gaussianBlurV, detectEdge, nonMaximumSuppression, hysteresis, outputGB, binarizeCE, discretizeGM;
    compute::image2d src[3], dst[3], blur[3], gradient[3], direction[3];
    compute::buffer buffer, label;
};

static void VS_CC tcannyCLInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    TCannyCLData * d = static_cast<TCannyCLData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC tcannyCLGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    TCannyCLData * d = static_cast<TCannyCLData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[]{ d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[]{ 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        try {
            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (d->process[plane]) {
                    const size_t width = vsapi->getFrameWidth(src, plane);
                    const size_t height = vsapi->getFrameHeight(src, plane);
                    const size_t stride = vsapi->getStride(src, plane);
                    const uint8_t * srcp = vsapi->getReadPtr(src, plane);
                    uint8_t * dstp = vsapi->getWritePtr(dst, plane);

                    const size_t origin[]{ 0, 0, 0 };
                    const size_t region[]{ width, height, 1 };
                    const size_t globalWorkSize[]{ width, height };

                    d->queue.enqueue_write_image(d->src[plane], origin, region, srcp, stride);

                    if (d->horizontalRadius[plane]) {
                        d->gaussianBlurV.set_args(d->src[plane], d->gradient[plane], d->verticalWeights[plane], d->verticalRadius[plane], d->offset[plane]);
                        d->queue.enqueue_nd_range_kernel(d->gaussianBlurV, 2, nullptr, globalWorkSize, nullptr);

                        d->gaussianBlurH.set_args(d->gradient[plane], d->blur[plane], d->horizontalWeights[plane], d->horizontalRadius[plane]);
                        d->queue.enqueue_nd_range_kernel(d->gaussianBlurH, 2, nullptr, globalWorkSize, nullptr);
                    } else {
                        d->copyPlane.set_args(d->src[plane], d->blur[plane], d->offset[plane]);
                        d->queue.enqueue_nd_range_kernel(d->copyPlane, 2, nullptr, globalWorkSize, nullptr);
                    }

                    if (d->mode != -1) {
                        d->detectEdge.set_args(d->blur[plane], d->gradient[plane], d->direction[plane]);
                        d->queue.enqueue_nd_range_kernel(d->detectEdge, 2, nullptr, globalWorkSize, nullptr);

                        if (d->mode == 0) {
                            d->nonMaximumSuppression.set_args(d->gradient[plane], d->direction[plane], d->buffer);
                            d->queue.enqueue_nd_range_kernel(d->nonMaximumSuppression, 2, nullptr, globalWorkSize, nullptr);

                            constexpr cl_uchar pattern = 0;
                            d->queue.enqueue_fill_buffer(d->label, &pattern, sizeof(cl_uchar), 0, width * height * sizeof(cl_uchar));

                            const size_t paddedGlobalWorkSize[]{ (width + 7) & -8, (height + 7) & -8 };
                            const size_t localWorkSize[]{ 8, 8 };

                            d->hysteresis.set_args(d->buffer, d->label, static_cast<int>(width), static_cast<int>(height));
                            d->queue.enqueue_nd_range_kernel(d->hysteresis, 2, nullptr, paddedGlobalWorkSize, localWorkSize);
                        }
                    }

                    if (d->mode == -1) {
                        d->outputGB.set_args(d->blur[plane], d->dst[plane], d->peak, d->offset[plane]);
                        d->queue.enqueue_nd_range_kernel(d->outputGB, 2, nullptr, globalWorkSize, nullptr);
                    } else if (d->mode == 0) {
                        d->binarizeCE.set_args(d->buffer, d->dst[plane], d->peak, d->lower[plane], d->upper[plane]);
                        d->queue.enqueue_nd_range_kernel(d->binarizeCE, 2, nullptr, globalWorkSize, nullptr);
                    } else {
                        d->discretizeGM.set_args(d->gradient[plane], d->dst[plane], d->peak, d->offset[plane]);
                        d->queue.enqueue_nd_range_kernel(d->discretizeGM, 2, nullptr, globalWorkSize, nullptr);
                    }

                    d->queue.enqueue_read_image(d->dst[plane], origin, region, stride, 0, dstp);
                }
            }
        } catch (const compute::opencl_error & error) {
            vsapi->setFilterError(("TCannyCL: " + error.error_string()).c_str(), frameCtx);
            vsapi->freeFrame(src);
            vsapi->freeFrame(dst);
            return nullptr;
        }

        vsapi->freeFrame(src);
        return dst;
    }

    return nullptr;
}

static void VS_CC tcannyCLFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TCannyCLData * d = static_cast<TCannyCLData *>(instanceData);
    vsapi->freeNode(d->node);
    delete d;
}

void VS_CC tcannyCLCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<TCannyCLData> d{ new TCannyCLData{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    try {
        if (!isConstantFormat(d->vi) || (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bits integer and 32 bits float input supported" };

        const int numSigma = vsapi->propNumElements(in, "sigma");
        if (numSigma > d->vi->format->numPlanes)
            throw std::string{ "more sigma given than the number of planes" };

        float horizontalSigma[3], verticalSigma[3];

        for (int i = 0; i < 3; i++) {
            if (i < numSigma) {
                horizontalSigma[i] = verticalSigma[i] = static_cast<float>(vsapi->propGetFloat(in, "sigma", i, nullptr));
            } else if (i == 0) {
                horizontalSigma[0] = verticalSigma[0] = 1.5f;
            } else if (i == 1) {
                horizontalSigma[1] = horizontalSigma[0] / (1 << d->vi->format->subSamplingW);
                verticalSigma[1] = verticalSigma[0] / (1 << d->vi->format->subSamplingH);
            } else {
                horizontalSigma[2] = horizontalSigma[1];
                verticalSigma[2] = verticalSigma[1];
            }
        }

        float t_h = static_cast<float>(vsapi->propGetFloat(in, "t_h", 0, &err));
        if (err)
            t_h = 8.f;

        float t_l = static_cast<float>(vsapi->propGetFloat(in, "t_l", 0, &err));
        if (err)
            t_l = 1.f;

        d->mode = int64ToIntS(vsapi->propGetInt(in, "mode", 0, &err));

        int op = int64ToIntS(vsapi->propGetInt(in, "op", 0, &err));
        if (err)
            op = 1;

        float gmmax = static_cast<float>(vsapi->propGetFloat(in, "gmmax", 0, &err));
        if (err)
            gmmax = 50.f;

        int device = int64ToIntS(vsapi->propGetInt(in, "device", 0, &err));
        if (err)
            device = -1;

        for (int i = 0; i < 3; i++) {
            if (horizontalSigma[i] < 0.f)
                throw std::string{ "sigma must be greater than or equal to 0.0" };
        }

        if (t_l >= t_h)
            throw std::string{ "t_h must be greater than t_l" };

        if (d->mode < -1 || d->mode > 1)
            throw std::string{ "mode must be -1, 0 or 1" };

        if (op < 0 || op > 3)
            throw std::string{ "op must be 0, 1, 2 or 3" };

        if (gmmax < 1.f)
            throw std::string{ "gmmax must be greater than or equal to 1.0" };

        if (device >= static_cast<int>(compute::system::device_count()))
            throw std::string{ "device index out of range" };

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

        if (!!vsapi->propGetInt(in, "list_device", 0, &err)) {
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

        if (d->vi->format->sampleType == stInteger) {
            d->peak = (1 << d->vi->format->bitsPerSample) - 1;
            const float scale = d->peak / 255.f;
            t_h *= scale;
            t_l *= scale;
        } else {
            t_h /= 255.f;
            t_l /= 255.f;

            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (plane == 0 || d->vi->format->colorFamily == cmRGB) {
                    d->offset[plane] = 0.f;
                    d->lower[plane] = 0.f;
                    d->upper[plane] = 1.f;
                } else {
                    d->offset[plane] = 0.5f;
                    d->lower[plane] = -0.5f;
                    d->upper[plane] = 0.5f;
                }
            }
        }

        compute::device gpu = compute::system::default_device();
        if (device > -1)
            gpu = compute::system::devices()[device];
        const compute::context ctx{ gpu };
        d->queue = compute::command_queue{ ctx, gpu };

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane] && horizontalSigma[plane]) {
                float * horizontalWeights = gaussianWeights(horizontalSigma[plane], &d->horizontalRadius[plane]);
                float * verticalWeights = gaussianWeights(verticalSigma[plane], &d->verticalRadius[plane]);
                if (!horizontalWeights || !verticalWeights)
                    throw std::string{ "malloc failure (weights)" };

                d->horizontalWeights[plane] = compute::buffer{ ctx, (d->horizontalRadius[plane] * 2 + 1) * sizeof(cl_float), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_NO_ACCESS, horizontalWeights };
                d->verticalWeights[plane] = compute::buffer{ ctx, (d->verticalRadius[plane] * 2 + 1) * sizeof(cl_float), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_NO_ACCESS, verticalWeights };

                delete[] horizontalWeights;
                delete[] verticalWeights;
            }
        }

        compute::program program = compute::program::create_with_source(source, ctx);
        try {
            std::string options{ "-cl-denorms-are-zero -cl-fast-relaxed-math -Werror" };
            options += " -D T_H=" + std::to_string(t_h);
            options += " -D T_L=" + std::to_string(t_l);
            options += " -D MODE=" + std::to_string(d->mode);
            options += " -D OP=" + std::to_string(op);
            options += " -D MAGNITUDE=" + std::to_string(255.f / gmmax);
            program.build(options);
        } catch (const compute::opencl_error & error) {
            throw error.error_string() + "\n" + program.build_log();
        }

        if (d->vi->format->sampleType == stInteger) {
            d->copyPlane = program.create_kernel("copyPlane_uint");
            d->gaussianBlurV = program.create_kernel("gaussianBlurV_uint");
            d->outputGB = program.create_kernel("outputGB_uint");
            d->binarizeCE = program.create_kernel("binarizeCE_uint");
            d->discretizeGM = program.create_kernel("discretizeGM_uint");
        } else {
            d->copyPlane = program.create_kernel("copyPlane_float");
            d->gaussianBlurV = program.create_kernel("gaussianBlurV_float");
            d->outputGB = program.create_kernel("outputGB_float");
            d->binarizeCE = program.create_kernel("binarizeCE_float");
            d->discretizeGM = program.create_kernel("discretizeGM_float");
        }
        d->gaussianBlurH = program.create_kernel("gaussianBlurH");
        d->detectEdge = program.create_kernel("detectEdge");
        d->nonMaximumSuppression = program.create_kernel("nonMaximumSuppression");
        d->hysteresis = program.create_kernel("hysteresis");

        if (!!vsapi->propGetInt(in, "info", 0, &err)) {
            std::string text{ "=== Device Info ===\n" };
            text += "Name: " + gpu.get_info<CL_DEVICE_NAME>() + "\n";
            text += "Vendor: " + gpu.get_info<CL_DEVICE_VENDOR>() + "\n";
            text += "Profile: " + gpu.get_info<CL_DEVICE_PROFILE>() + "\n";
            text += "Version: " + gpu.get_info<CL_DEVICE_VERSION>() + "\n";
            text += "Global Memory Size: " + std::to_string(gpu.get_info<CL_DEVICE_GLOBAL_MEM_SIZE>() / 1024 / 1024) + " MB\n";
            text += "Local Memory Size: " + std::to_string(gpu.get_info<CL_DEVICE_LOCAL_MEM_SIZE>() / 1024) + " KB\n";
            text += "Local Memory Type: " + std::string{ gpu.get_info<CL_DEVICE_LOCAL_MEM_TYPE>() == CL_LOCAL ? "CL_LOCAL" : "CL_GLOBAL" } + "\n";
            text += "Memory Base Address Alignment: " + std::to_string(gpu.get_info<cl_uint>(CL_DEVICE_MEM_BASE_ADDR_ALIGN) / 8) + " bytes\n";
            text += "Image Support: " + std::string{ gpu.get_info<CL_DEVICE_IMAGE_SUPPORT>() ? "CL_TRUE" : "CL_FALSE" } + "\n";
            text += "2D Image Max Width: " + std::to_string(gpu.get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>()) + "\n";
            text += "2D Image Max Height: " + std::to_string(gpu.get_info<CL_DEVICE_IMAGE2D_MAX_HEIGHT>()) + "\n";
            text += "Max Constant Arguments: " + std::to_string(gpu.get_info<CL_DEVICE_MAX_CONSTANT_ARGS>()) + "\n";
            text += "Max Constant Buffer Size: " + std::to_string(gpu.get_info<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() / 1024) + " KB\n";
            text += "Max Work-group Size: " + std::to_string(gpu.get_info<CL_DEVICE_MAX_WORK_GROUP_SIZE>()) + "\n";
            const auto MAX_WORK_ITEM_SIZES = gpu.get_info<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
            text += "Max Work-item Sizes: (" + std::to_string(MAX_WORK_ITEM_SIZES[0]) + ", " + std::to_string(MAX_WORK_ITEM_SIZES[1]) + ", " + std::to_string(MAX_WORK_ITEM_SIZES[2]) + ")\n";

            text += "\n=== Kernel Work-group Info ===\n";
            text += "== copyPlane ==\n";
            text += "Local Memory Size: " + std::to_string(d->copyPlane.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_LOCAL_MEM_SIZE) / 1024.f) + " KB\n";
            text += "Private Memory Size: " + std::to_string(d->copyPlane.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_PRIVATE_MEM_SIZE)) + " bytes\n";
            text += "Preferred Work-group Size Multiple: " + std::to_string(d->copyPlane.get_work_group_info<size_t>(gpu, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE)) + "\n";
            text += "== gaussianBlurH ==\n";
            text += "Local Memory Size: " + std::to_string(d->gaussianBlurH.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_LOCAL_MEM_SIZE) / 1024.f) + " KB\n";
            text += "Private Memory Size: " + std::to_string(d->gaussianBlurH.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_PRIVATE_MEM_SIZE)) + " bytes\n";
            text += "Preferred Work-group Size Multiple: " + std::to_string(d->gaussianBlurH.get_work_group_info<size_t>(gpu, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE)) + "\n";
            text += "== gaussianBlurV ==\n";
            text += "Local Memory Size: " + std::to_string(d->gaussianBlurV.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_LOCAL_MEM_SIZE) / 1024.f) + " KB\n";
            text += "Private Memory Size: " + std::to_string(d->gaussianBlurV.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_PRIVATE_MEM_SIZE)) + " bytes\n";
            text += "Preferred Work-group Size Multiple: " + std::to_string(d->gaussianBlurV.get_work_group_info<size_t>(gpu, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE)) + "\n";
            text += "== detectEdge ==\n";
            text += "Local Memory Size: " + std::to_string(d->detectEdge.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_LOCAL_MEM_SIZE) / 1024.f) + " KB\n";
            text += "Private Memory Size: " + std::to_string(d->detectEdge.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_PRIVATE_MEM_SIZE)) + " bytes\n";
            text += "Preferred Work-group Size Multiple: " + std::to_string(d->detectEdge.get_work_group_info<size_t>(gpu, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE)) + "\n";
            text += "== nonMaximumSuppression ==\n";
            text += "Local Memory Size: " + std::to_string(d->nonMaximumSuppression.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_LOCAL_MEM_SIZE) / 1024.f) + " KB\n";
            text += "Private Memory Size: " + std::to_string(d->nonMaximumSuppression.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_PRIVATE_MEM_SIZE)) + " bytes\n";
            text += "Preferred Work-group Size Multiple: " + std::to_string(d->nonMaximumSuppression.get_work_group_info<size_t>(gpu, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE)) + "\n";
            text += "== hysteresis ==\n";
            text += "Local Memory Size: " + std::to_string(d->hysteresis.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_LOCAL_MEM_SIZE) / 1024.f) + " KB\n";
            text += "Private Memory Size: " + std::to_string(d->hysteresis.get_work_group_info<cl_ulong>(gpu, CL_KERNEL_PRIVATE_MEM_SIZE)) + " bytes\n";
            text += "Preferred Work-group Size Multiple: " + std::to_string(d->hysteresis.get_work_group_info<size_t>(gpu, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE));

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

        cl_image_format clImageFormat;
        if (d->vi->format->bytesPerSample == 1)
            clImageFormat = { CL_R, CL_UNSIGNED_INT8 };
        else if (d->vi->format->bytesPerSample == 2)
            clImageFormat = { CL_R, CL_UNSIGNED_INT16 };
        else
            clImageFormat = { CL_R, CL_FLOAT };
        const compute::image_format imageFormat{ clImageFormat };

        size_t width = d->vi->width;
        size_t height = d->vi->height;

        d->src[0] = compute::image2d{ ctx, width, height, imageFormat, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY };
        d->dst[0] = compute::image2d{ ctx, width, height, imageFormat, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY };
        d->blur[0] = compute::image2d{ ctx, width, height, compute::image_format{ CL_R, CL_FLOAT }, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS };
        d->gradient[0] = compute::image2d{ ctx, width, height, compute::image_format{ CL_R, CL_FLOAT }, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS };
        d->direction[0] = compute::image2d{ ctx, width, height, compute::image_format{ CL_R, CL_UNSIGNED_INT8 }, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS };

        if (d->vi->format->subSamplingW || d->vi->format->subSamplingH) {
            width >>= d->vi->format->subSamplingW;
            height >>= d->vi->format->subSamplingH;

            d->src[1] = d->src[2] = compute::image2d{ ctx, width, height, imageFormat, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY };
            d->dst[1] = d->dst[2] = compute::image2d{ ctx, width, height, imageFormat, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY };
            d->blur[1] = d->blur[2] = compute::image2d{ ctx, width, height, compute::image_format{ CL_R, CL_FLOAT }, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS };
            d->gradient[1] = d->gradient[2] = compute::image2d{ ctx, width, height, compute::image_format{ CL_R, CL_FLOAT }, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS };
            d->direction[1] = d->direction[2] = compute::image2d{ ctx, width, height, compute::image_format{ CL_R, CL_UNSIGNED_INT8 }, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS };
        } else {
            d->src[1] = d->src[2] = d->src[0];
            d->dst[1] = d->dst[2] = d->dst[0];
            d->blur[1] = d->blur[2] = d->blur[0];
            d->gradient[1] = d->gradient[2] = d->gradient[0];
            d->direction[1] = d->direction[2] = d->direction[0];
        }

        d->buffer = compute::buffer{ ctx, d->vi->width * d->vi->height * sizeof(cl_float), CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS };
        d->label = compute::buffer{ ctx, d->vi->width * d->vi->height * sizeof(cl_uchar), CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS };
    } catch (const std::string & error) {
        vsapi->setError(out, ("TCannyCL: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    } catch (const compute::no_device_found & error) {
        vsapi->setError(out, (std::string{ "TCannyCL: " } + error.what()).c_str());
        vsapi->freeNode(d->node);
        return;
    } catch (const compute::opencl_error & error) {
        vsapi->setError(out, ("TCannyCL: " + error.error_string()).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "TCannyCL", tcannyCLInit, tcannyCLGetFrame, tcannyCLFree, fmParallelRequests, 0, d.release(), core);
}
#endif
