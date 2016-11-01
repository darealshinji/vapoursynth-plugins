#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <VapourSynth.h>
#include <VSHelper.h>


typedef struct Cnr2Data {
    VSNodeRef *clip;
    int scenechroma;

    const VSVideoInfo *vi;

    const VSFrameRef *prev;

    int last_frame;

    uint8_t *table_y;
    uint8_t *table_u;
    uint8_t *table_v;

    uint8_t *curp_y;
    uint8_t *prevp_y;

    int64_t diff_max;
} Cnr2Data;


static void VS_CC cnr2Init(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    (void)in;
    (void)out;
    (void)core;

    Cnr2Data *d = (Cnr2Data *) * instanceData;

    vsapi->setVideoInfo(d->vi, 1, node);
}


static void downSampleLuma(uint8_t *dstp, const VSFrameRef *src, const VSAPI *vsapi) {
    const uint8_t *srcp = vsapi->getReadPtr(src, 0);
    int src_stride = vsapi->getStride(src, 0);
    const VSFormat *src_format = vsapi->getFrameFormat(src);

    int dst_width = vsapi->getFrameWidth(src, 1);
    int dst_height = vsapi->getFrameHeight(src, 1);
    int dst_stride = dst_width;

    for (int y = 0; y < dst_height; y++) {
        const uint8_t *srcpn = srcp + (src_stride * src_format->subSamplingH);

        for (int x = 0; x < dst_width; x++)
            dstp[x] = (srcp[x * 2] + srcp[x * 2 + src_format->subSamplingW] + srcpn[x * 2] + srcpn[x * 2 + src_format->subSamplingW] + 2) >> 2;

        srcp += src_stride << src_format->subSamplingH;
        dstp += dst_stride;
    }
}


static const VSFrameRef *VS_CC cnr2GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    (void)frameData;

    Cnr2Data *d = (Cnr2Data *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(VSMAX(0, n - 1), d->clip, frameCtx);
        vsapi->requestFrameFilter(n, d->clip, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *cur = vsapi->getFrameFilter(n, d->clip, frameCtx);

        if (n == 0) {
            vsapi->freeFrame(d->prev);
            d->prev = vsapi->cloneFrameRef(cur);
            return cur;
        }

        if (d->last_frame != n - 1) {
            vsapi->freeFrame(d->prev);
            d->prev = vsapi->getFrameFilter(n - 1, d->clip, frameCtx);
            downSampleLuma(d->prevp_y, d->prev, vsapi);
        }

        const VSFrameRef *frames[3] = { cur };
        int planes[3] = { 0 };
        VSFrameRef *dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, frames, planes, cur, core);

        downSampleLuma(d->curp_y, cur, vsapi);


        int width_uv = vsapi->getFrameWidth(cur, 1);
        int height_uv = vsapi->getFrameHeight(cur, 1);
        int stride_uv = vsapi->getStride(cur, 1);

        const uint8_t *curp_y = d->curp_y;
        const uint8_t *prevp_y = d->prevp_y;

        const uint8_t *curp_u = vsapi->getReadPtr(cur, 1);
        const uint8_t *prevp_u = vsapi->getReadPtr(d->prev, 1);
        uint8_t *dstp_u = vsapi->getWritePtr(dst, 1);

        const uint8_t *curp_v = vsapi->getReadPtr(cur, 2);
        const uint8_t *prevp_v = vsapi->getReadPtr(d->prev, 2);
        uint8_t *dstp_v = vsapi->getWritePtr(dst, 2);

        int64_t diff_total = 0;

        for (int y = 0; y < height_uv; y++) {
            for (int x = 0; x < width_uv; x++) {
                int diff_y = curp_y[x] - prevp_y[x];
                int diff_u = curp_u[x] - prevp_u[x];
                int diff_v = curp_v[x] - prevp_v[x];

                diff_total += abs(diff_y << (d->vi->format->subSamplingW + d->vi->format->subSamplingH));
                if (d->scenechroma)
                    diff_total += abs(diff_u) + abs(diff_v);

                int weight_u = d->table_y[diff_y + 256] * d->table_u[diff_u + 256];
                int weight_v = d->table_y[diff_y + 256] * d->table_v[diff_v + 256];

                dstp_u[x] = (weight_u * prevp_u[x] + (65536 - weight_u) * curp_u[x] + 32768) >> 16;
                dstp_v[x] = (weight_v * prevp_v[x] + (65536 - weight_v) * curp_v[x] + 32768) >> 16;
            }

            if (diff_total > d->diff_max) {
                vsapi->freeFrame(dst);
                return cur;
            }

            curp_u += stride_uv;
            dstp_u += stride_uv;
            prevp_u += stride_uv;

            curp_v += stride_uv;
            dstp_v += stride_uv;
            prevp_v += stride_uv;

            curp_y += width_uv;
            prevp_y += width_uv;
        }

        vsapi->freeFrame(cur);

        d->last_frame = n;
        uint8_t *temp = d->curp_y;
        d->curp_y = d->prevp_y;
        d->prevp_y = temp;
        vsapi->freeFrame(d->prev);
        d->prev = vsapi->cloneFrameRef(dst);

        return dst;
    }

    return NULL;
}


static void VS_CC cnr2Free(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    (void)core;

    Cnr2Data *d = (Cnr2Data *)instanceData;

    free(d->curp_y);
    free(d->prevp_y);

    free(d->table_y);
    free(d->table_u);
    free(d->table_v);

    vsapi->freeFrame(d->prev);

    vsapi->freeNode(d->clip);
    free(d);
}


static void VS_CC cnr2Create(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    (void)userData;

    Cnr2Data d;
    Cnr2Data *data;

    memset(&d, 0, sizeof(d));

    int err;

    const char *mode = vsapi->propGetData(in, "mode", 0, &err);
    if (err)
        mode = "oxx";

    double scdthr = vsapi->propGetFloat(in, "scdthr", 0, &err);
    if (err)
        scdthr = 10.0;

    int ln = int64ToIntS(vsapi->propGetInt(in, "ln", 0, &err));
    if (err)
        ln = 35;

    int lm = int64ToIntS(vsapi->propGetInt(in, "lm", 0, &err));
    if (err)
        lm = 192;

    int un = int64ToIntS(vsapi->propGetInt(in, "un", 0, &err));
    if (err)
        un = 47;

    int um = int64ToIntS(vsapi->propGetInt(in, "um", 0, &err));
    if (err)
        um = 255;

    int vn = int64ToIntS(vsapi->propGetInt(in, "vn", 0, &err));
    if (err)
        vn = 47;

    int vm = int64ToIntS(vsapi->propGetInt(in, "vm", 0, &err));
    if (err)
        vm = 255;

    d.scenechroma = !!vsapi->propGetInt(in, "scenechroma", 0, &err);


    int mode_size = vsapi->propGetDataSize(in, "mode", 0, &err);
    if (!err && mode_size < 3) {
        vsapi->setError(out, "Cnr2: mode must have at least three characters.");
        return;
    }

    if (scdthr < 0.0 || scdthr > 100.0) {
        vsapi->setError(out, "Cnr2: scdthr must be between 0.0 and 100.0 (inclusive).");
        return;
    }

    if (ln < 0 || ln > 255 ||
        lm < 0 || lm > 255 ||
        un < 0 || un > 255 ||
        um < 0 || um > 255 ||
        vn < 0 || vn > 255 ||
        vm < 0 || vm > 255) {
        vsapi->setError(out, "Cnr2: ln, lm, un, um, vn, vm all must be between 0 and 255 (inclusive).");
        return;
    }


    d.clip = vsapi->propGetNode(in, "clip", 0, NULL);
    d.vi = vsapi->getVideoInfo(d.clip);

    if (!isConstantFormat(d.vi) ||
        d.vi->format->colorFamily != cmYUV ||
        d.vi->format->sampleType != stInteger ||
        d.vi->format->bitsPerSample > 8 ||
        d.vi->format->subSamplingW > 1 ||
        d.vi->format->subSamplingH > 1) {
        vsapi->setError(out, "Cnr2: clip must have constant format and dimensions, and it must be YUV420P8, YUV422P8, YUV440P8, or YUV444P8.");
        vsapi->freeNode(d.clip);
        return;
    }


    d.curp_y = (uint8_t *)malloc((d.vi->width >> d.vi->format->subSamplingW) * (d.vi->height >> d.vi->format->subSamplingH));
    d.prevp_y = (uint8_t *)malloc((d.vi->width >> d.vi->format->subSamplingW) * (d.vi->height >> d.vi->format->subSamplingH));


    // We use the limits from TV range YUV for some reason.
    int max_luma_diff = 235 - 16;
    int max_chroma_diff = 240 - 16;

    int max_pixel_diff = max_luma_diff;
    if (d.scenechroma)
        max_pixel_diff += (max_chroma_diff * 2) >> (d.vi->format->subSamplingW + d.vi->format->subSamplingH);

    d.diff_max = (int)((scdthr * d.vi->width * d.vi->height * max_pixel_diff) / 100.0);


#define TABLE_SIZE 513
    d.table_y = (uint8_t *)malloc(TABLE_SIZE);
    d.table_u = (uint8_t *)malloc(TABLE_SIZE);
    d.table_v = (uint8_t *)malloc(TABLE_SIZE);

    memset(d.table_y, 0, TABLE_SIZE);
    memset(d.table_u, 0, TABLE_SIZE);
    memset(d.table_v, 0, TABLE_SIZE);
#undef TABLE_SIZE

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

    for (int j = -ln; j <= ln; j++) {
        if (mode[0] != 'x')
            d.table_y[j + 256] = (int)(lm / 2 * (1 + cos(j * j * M_PI / (ln * ln))));
        else
            d.table_y[j + 256] = (int)(lm / 2 * (1 + cos(j * M_PI / ln)));
    }

    for (int j = -un; j <= un; j++) {
        if (mode[1] != 'x')
            d.table_u[j + 256] = (int)(um / 2 * (1 + cos(j * j * M_PI / (un * un))));
        else
            d.table_u[j + 256] = (int)(um / 2 * (1 + cos(j * M_PI / un)));
    }

    for (int j = -vn; j <= vn; j++) {
        if (mode[2] != 'x')
            d.table_v[j + 256] = (int)(vm / 2 * (1 + cos(j * j * M_PI / (vn * vn))));
        else
            d.table_v[j + 256] = (int)(vm / 2 * (1 + cos(j * M_PI / vn)));
    }


    data = (Cnr2Data *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Cnr2", cnr2Init, cnr2GetFrame, cnr2Free, fmSerial, nfMakeLinear, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.cnr2", "cnr2", "Chroma noise reducer", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Cnr2",
            "clip:clip;"
            "mode:data:opt;"
            "scdthr:float:opt;"
            "ln:int:opt;"
            "lm:int:opt;"
            "un:int:opt;"
            "um:int:opt;"
            "vn:int:opt;"
            "vm:int:opt;"
            "scenechroma:int:opt;"
            , cnr2Create, 0, plugin);
}
