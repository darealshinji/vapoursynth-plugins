#include <stdlib.h>
#include <string.h>
#include <VapourSynth.h>

typedef struct {
    VSNodeRef *node;
    VSVideoInfo vi;
    uint16_t maxVal;
} LumaData;


static void VS_CC lumaInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    LumaData *d = (LumaData *)* instanceData;
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC lumaGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    LumaData *d = (LumaData *)* instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);

        const VSFormat *fi = d->vi.format;
        const int src_height = vsapi->getFrameHeight(src, 0);
        const int src_width = vsapi->getFrameWidth(src, 0);

        VSFrameRef *dst = vsapi->newVideoFrame(fi, src_width, src_height, src, core);

        int src_stride = vsapi->getStride(src, 0);
        int dst_stride = vsapi->getStride(dst, 0);

        const uint8_t highBitDepth = fi->bitsPerSample / 9;

        int y;
        int x;

        //Some duplicate code due to lack of templates
        if (highBitDepth) {
            src_stride /= 2;
            dst_stride /= 2;
            const uint16_t *srcp = vsapi->getReadPtr(src, 0);
            uint16_t *dstp = vsapi->getWritePtr(dst, 0);
            for (y = 0; y < src_height; y++) {
                for (x = 0; x < src_width; x++) {
                    int p = srcp[x] << 4;
                    dstp[x] = (p & d->maxVal + 1) ? (d->maxVal - (p & d->maxVal)) : p & d->maxVal;
                }
                srcp += src_stride;
                dstp += dst_stride;
            }
        }
        else {
            const uint8_t *srcp = vsapi->getReadPtr(src, 0);
            uint8_t *dstp = vsapi->getWritePtr(dst, 0);
            for (y = 0; y < src_height; y++) {
                for (x = 0; x < src_width; x++) {
                    int p = srcp[x] << 4;
                    dstp[x] = (p & d->maxVal + 1) ? (d->maxVal - (p & d->maxVal)) : p & d->maxVal;
                }
                srcp += src_stride;
                dstp += dst_stride;
            }
        }

        vsapi->freeFrame(src);

        return dst;
    }

    return 0;
}


static void VS_CC lumaFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    LumaData *d = (LumaData *)instanceData;
    vsapi->freeNode(d->node);
    free(d);
}


void VS_CC lumaCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    LumaData d;
    LumaData *data;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = *vsapi->getVideoInfo(d.node);

    if (!d.vi.format || d.vi.format->sampleType != stInteger || d.vi.format->bitsPerSample > 16) {
        vsapi->setError(out, "Luma: only constant format 8 to 16 bit integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    // We don't need any chroma.
    d.vi.format = vsapi->registerFormat(cmGray, stInteger, d.vi.format->bitsPerSample, 0, 0, core);

    d.maxVal = (1 << d.vi.format->bitsPerSample) - 1;

    data = malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Luma", lumaInit, lumaGetFrame, lumaFree, fmParallel, 0, data, core);
    return;
}

