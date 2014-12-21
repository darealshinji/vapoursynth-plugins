#include "VapourSynth.h"
#include "VSHelper.h"
#include "plugin.def.h"

typedef struct {
    VSNodeRef *node;
    VSVideoInfo vi;
    f3kdb_core_t* core;
} f3kdb_vs_context_t;

static const int F3KDB_PLANES[] = {PLANE_Y, PLANE_CB, PLANE_CR};

static void VS_CC f3kdbInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    f3kdb_vs_context_t *d = (f3kdb_vs_context_t *) * instanceData;
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC f3kdbGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    f3kdb_vs_context_t *d = (f3kdb_vs_context_t *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef *dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);
        for (int i = 0; i < d->vi.format->numPlanes; i++)
        {
            const unsigned char* src_ptr = vsapi->getReadPtr(src, i);
            int src_stride = vsapi->getStride(src, i);
            unsigned char* dst_ptr = vsapi->getWritePtr(dst, i);
            int dst_stride = vsapi->getStride(dst, i);
            int f3kdb_plane = F3KDB_PLANES[i];

            int result = f3kdb_process_plane(d->core, n, f3kdb_plane, dst_ptr, dst_stride, src_ptr, src_stride);
            if (result != F3KDB_SUCCESS)
            {
                char msg[1024];
                memset(msg, 0, sizeof(msg));
                _snprintf(msg, sizeof(msg) - 1, "f3kdb: Error while processing plane, f3kdb_plane: %d, code: %d", f3kdb_plane, result);
                vsapi->setFilterError(msg, frameCtx);
                return 0;
            }
        }
        vsapi->freeFrame(src);
        return dst;
    }

    return 0;
}

static void VS_CC f3kdbFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    f3kdb_vs_context_t *d = (f3kdb_vs_context_t *)instanceData;
    f3kdb_destroy(d->core);
    vsapi->freeNode(d->node);
    free(d);
}

static void VS_CC f3kdbCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    auto node = vsapi->propGetNode(in, "clip", 0, 0);
    VSVideoInfo vi = *vsapi->getVideoInfo(node);

    if (!isConstantFormat(&vi))
    {
        vsapi->setError(out, "f3kdb: Only constant format is supported");
        vsapi->freeNode(node);
        return;
    }

    if (vi.format->sampleType != stInteger)
    {
        vsapi->setError(out, "f3kdb: Only integer sample format is supported");
        vsapi->freeNode(node);
        return;
    }

    if (vi.format->bitsPerSample < 8 || vi.format->bitsPerSample > 16)
    {
        vsapi->setError(out, "f3kdb: Only 8 ~ 16 bits per sample is supported");
        vsapi->freeNode(node);
        return;
    }

    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);

    int error;
    const char* preset = vsapi->propGetData(in, "preset", 0, &error);
    if (error != peUnset)
    {
        if (error == peType)
        {
            vsapi->setError(out, "f3kdb: preset must be a string");
            vsapi->freeNode(node);
            return;
        } else if (error != _peNoError) {
            vsapi->setError(out, "f3kdb: Unknown preset error");
            vsapi->freeNode(node);
            return;
        }
        int result = f3kdb_params_fill_preset(&params, preset);
        if (result != F3KDB_SUCCESS)
        {
            vsapi->setError(out, "f3kdb: Invalid preset");
            vsapi->freeNode(node);
            return;
        }
    }

    bool success = f3kdb_params_from_vs(&params, in, out, vsapi);
    if (!success)
    {
        // Error message has been set in param_from_vsmap
        vsapi->freeNode(node);
        return;
    }
    params.output_mode = params.output_depth <= 8 ? LOW_BIT_DEPTH : HIGH_BIT_DEPTH_INTERLEAVED;
    f3kdb_params_sanitize(&params);

    f3kdb_video_info_t video_info;
    video_info.width = vi.width;
    video_info.height = vi.height;
    video_info.depth = vi.format->bitsPerSample;
    video_info.pixel_mode = vi.format->bitsPerSample == 8 ? LOW_BIT_DEPTH : HIGH_BIT_DEPTH_INTERLEAVED;
    video_info.num_frames = vi.numFrames;
    video_info.chroma_width_subsampling = vi.format->subSamplingW;
    video_info.chroma_height_subsampling = vi.format->subSamplingH;

    const VSFormat* new_format = vsapi->registerFormat(vi.format->colorFamily, vi.format->sampleType, params.output_depth, vi.format->subSamplingW, vi.format->subSamplingH, core);
    if (!new_format)
    {
        vsapi->setError(out, "f3kdb: Unable to register output format");
        vsapi->freeNode(node);
        return;
    }

    char error_msg[1024];
    memset(error_msg, 0, sizeof(error_msg));
    f3kdb_core_t* f3kdb_core = nullptr;
    int result = f3kdb_create(&video_info, &params, &f3kdb_core, error_msg, sizeof(error_msg) - 1);
    if (result != F3KDB_SUCCESS)
    {
        char vs_err_msg[2048];
        memset(vs_err_msg, 0, sizeof(vs_err_msg));
        _snprintf(vs_err_msg, sizeof(vs_err_msg) - 1, "f3kdb: Core initialization failed, code = %d. %s", result, error_msg);
        vsapi->setError(out, vs_err_msg);
        vsapi->freeNode(node);
        return;
    }

    f3kdb_vs_context_t* context = (f3kdb_vs_context_t*)malloc(sizeof(f3kdb_vs_context_t));
    if (!context)
    {
        // Shouldn't really happen, just in case
        f3kdb_destroy(f3kdb_core);
        vsapi->freeNode(node);
        return;
    }

    context->core = f3kdb_core;
    context->node = node;
    context->vi = vi;
    context->vi.format = new_format;

    vsapi->createFilter(in, out, "f3kdb", f3kdbInit, f3kdbGetFrame, f3kdbFree, fmParallel, 0, context, core);
    return;
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("net.sapikachu.f3kdb", "f3kdb", "flash3kyuu_deband", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Deband", F3KDB_VAPOURSYNTH_PARAMS, f3kdbCreate, 0, plugin);
}
