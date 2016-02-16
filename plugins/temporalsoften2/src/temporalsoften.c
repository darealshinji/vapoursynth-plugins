#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <emmintrin.h>
#include "VapourSynth.h"

#ifdef _MSC_VER
#pragma warning(disable:4224 4996)
#define snprintf _snprintf
#define TS_ALIGN __declspec(align(16))
#define TS_FUNC_ALIGN
#else
#define TS_ALIGN __attribute__((aligned(16)))
#define TS_FUNC_ALIGN __attribute__((force_align_arg_pointer))
#endif

#define TEMPORALSOFTEN2_VERSION "0.1.1"

typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

   // Filter parameters.
    int radius;
    int threshold[3];
    int scenechange;
    int mode;
    void (VS_CC *proc)(uint8_t *, const uint8_t **, int, int, int);
} TemporalSoftenData;


#if 0
static void VS_CC
mode2_8bit(uint8_t *dstp, const uint8_t **srcp, int frames, int width,
           int height, int stride, int threshold)
{
    int y;
    int half_frames = frames / 2;

    for (y = 0; y < height; y++) {
        int x;
        for (x = 0; x < width; x++) {
            unsigned sum = dstp[x];
            int f;
            for (f = 1; f < frames; f++) { // kernel_loop
                int val = dstp[x];
                if (abs(val - srcp[f][x]) <= threshold) {
                    val = srcp[f][x];
                }
                sum += val;
            }
            dstp[x] = (uint8_t)((sum + half_frames) / frames);
        }
        for (x = 1; x < frames; srcp[x++] += stride);
        dstp += stride;
    }
}


static void VS_CC
mode2_16bit(uint8_t *dstp8, const uint8_t **srcp8, int frames, int width,
            int height, int stride, int threshold)
{
    uint16_t *dstp = (uint16_t *)dstp8;
    const uint16_t *srcp[16];
    int y;
    int half_frames = frames / 2;

    stride /= 2;
    for (y = 1; y < frames; y++) {
        srcp[y] = (uint16_t *)srcp8[y];
    }

    for (y = 0; y < height; y++) {
        int x;
        for (x = 0; x < width; x++) {
            unsigned sum = dstp[x];
            int f;
            for (f = 1; f < frames; f++) { // kernel_loop
                int val = dstp[x];
                if (abs(val - srcp[f][x]) <= threshold) {
                    val = srcp[f][x];
                }
                sum += val;
            }
            dstp[x] = (uint16_t)((sum + half_frames) / frames);
        }
        for (x = 1; x < frames; srcp[x++] += stride);
        dstp += stride;
    }
}
#endif

static void TS_FUNC_ALIGN VS_CC
mode2_8bit_sse2(uint8_t *dstp, const uint8_t **srcp, int frames, int frame_size,
                int threshold)
{
    int16_t half_frames = frames / 2;
    int f;
    int16_t mul = (int16_t)ceil((1.0 / frames) * 65536);
    __m128i thrsh, dstpx, sum_lo, sum_hi, srcpf, temp0, temp1, temp2;

    thrsh = _mm_set1_epi8((int8_t)threshold);

    do {
        sum_lo = _mm_set1_epi16(half_frames);
        sum_hi = _mm_set1_epi16(half_frames);

        dstpx = _mm_load_si128((__m128i *)dstp);

        temp0 = _mm_setzero_si128();

        temp1 = _mm_unpacklo_epi8(dstpx, temp0);
        sum_lo = _mm_adds_epu16(sum_lo, temp1);

        temp1 = _mm_unpackhi_epi8(dstpx, temp0);
        sum_hi = _mm_adds_epu16(sum_hi, temp1);

        for (f = 1; f < frames; f++) {
            srcpf = _mm_load_si128((__m128i *)srcp[f]);

            temp0 = _mm_max_epu8(dstpx, srcpf);
            temp1 = _mm_min_epu8(dstpx, srcpf);
            temp0 = _mm_subs_epu8(temp0, temp1); // abs(dstp - srcp[f])

            temp1 = _mm_subs_epu8(temp0, thrsh);
            temp0 = _mm_setzero_si128();
            temp0 = _mm_cmpeq_epi8(temp1, temp0);

            srcpf = _mm_and_si128(srcpf, temp0); // effective values of srcp[f]

            temp1 = _mm_set1_epi8((char)0xFF);
            temp0 = _mm_xor_si128(temp0, temp1); // invert temp0
            temp0 = _mm_and_si128(dstpx, temp0); // effective values of dstp

            srcpf = _mm_or_si128(srcpf, temp0); // all effective values

            temp0 = _mm_setzero_si128();

            temp1 = _mm_unpacklo_epi8(srcpf, temp0);
            sum_lo = _mm_adds_epu16(sum_lo, temp1);

            temp1 = _mm_unpackhi_epi8(srcpf, temp0);
            sum_hi = _mm_adds_epu16(sum_hi, temp1);
        }
        if (frames > 2) {
            temp2 = _mm_set1_epi16(mul);
            sum_lo = _mm_mulhi_epi16(sum_lo, temp2);
            sum_hi = _mm_mulhi_epi16(sum_hi, temp2);
        } else {
            sum_lo = _mm_srai_epi16(sum_lo, frames - 1);
            sum_hi = _mm_srai_epi16(sum_hi, frames - 1);
        }
        sum_lo = _mm_packus_epi16(sum_lo, sum_hi);
        _mm_store_si128((__m128i *)dstp, sum_lo);

        dstp += 16;
        for (f = 1; f < frames; srcp[f++] += 16);
    } while ((frame_size -= 16) > 0);
}


static void TS_FUNC_ALIGN VS_CC
mode2_9_or_10_sse2(uint8_t *dstp8, const uint8_t **srcp8, int frames,
                   int frame_size, int threshold)
{
    uint16_t *dstp = (uint16_t *)dstp8;
    const uint16_t *srcp[16];
    int half_frames = frames / 2;
    int f;
    int16_t mul = (int16_t)ceil((1.0 / frames) * 65536);
    __m128i thrsh, dstpx, sum, srcpf, temp0, temp1;

    for (f = 1; f < frames; f++) {
        srcp[f] = (uint16_t *)srcp8[f];
    }

    thrsh = _mm_set1_epi16((int16_t)threshold);

    do {
        sum = _mm_set1_epi16(half_frames);

        dstpx = _mm_load_si128((__m128i *)dstp);

        sum = _mm_add_epi16(sum, dstpx);

        for (f = 1; f < frames; f++) {
            srcpf = _mm_load_si128((__m128i *)srcp[f]);

            temp0 = _mm_max_epi16(dstpx, srcpf);
            temp1 = _mm_min_epi16(dstpx, srcpf);
            temp0 = _mm_sub_epi16(temp0, temp1); // abs(dstp - srcp[f])

            temp0 = _mm_cmplt_epi16(temp0, thrsh);
            srcpf = _mm_and_si128(srcpf, temp0); // efective values of srcp[f]

            temp1 = _mm_set1_epi8((char)0xFF);
            temp0 = _mm_xor_si128(temp0, temp1); // invert temp0
            temp0 = _mm_and_si128(dstpx, temp0); // efective values of dstp

            srcpf = _mm_or_si128(srcpf, temp0); // all efective values

            sum = _mm_add_epi16(sum, srcpf);
        }

        if (frames > 2) {
            temp0 = _mm_set1_epi16(mul);
            sum = _mm_mulhi_epu16(sum, temp0);
        } else {
            sum = _mm_srai_epi16(sum, frames - 1);
        }
        _mm_store_si128((__m128i *)dstp, sum);

        dstp += 8;
        for (f = 1; f < frames; srcp[f++] += 8);
    } while ((frame_size -= 16) > 0);
}


typedef struct {
    TS_ALIGN uint16_t buff[8];
} buff_t;

static void TS_FUNC_ALIGN VS_CC
mode2_16bit_sse2(uint8_t *dstp8, const uint8_t **srcp8, int frames,
                   int frame_size, int threshold)
{
    uint16_t *dstp = (uint16_t *)dstp8;
    const uint16_t *srcp[16];
    int half_frames = frames / 2;
    int f;
    uint64_t r = ((uint64_t)1 << 40) / frames;
    __m128i thrsh, dstpx, srcpf, temp0, temp1;
    buff_t buffer[16];

    for (f = 1; f < frames; f++) {
        srcp[f] = (uint16_t *)srcp8[f];
    }

    thrsh = _mm_set1_epi16((int16_t)threshold);

    do {
        dstpx = _mm_load_si128((__m128i *)dstp);
        for (f = 0; f < 8; f++) {
            buffer[0].buff[f] = dstp[f];
        }

        for (f = 1; f < frames; f++) {
            srcpf = _mm_load_si128((__m128i *)srcp[f]);

            temp0 = _mm_subs_epu16(dstpx, srcpf);
            temp1 = _mm_adds_epu16(srcpf, temp0); // max(dstpx, srcpf)
            temp0 = _mm_subs_epu16(dstpx, temp0); // min(dstpx, srcpf)
            temp1 = _mm_subs_epu16(temp1, temp0); // abs(dstp - srcp[f])

            temp0 = _mm_subs_epu16(temp1, thrsh);
            temp1 = _mm_setzero_si128();
            temp0 = _mm_cmpeq_epi16(temp0, temp1);
            srcpf = _mm_and_si128(srcpf, temp0); // effective values of srcp[f]

            temp1 = _mm_set1_epi8((char)0xFF);
            temp0 = _mm_xor_si128(temp0, temp1); // invert temp0
            temp0 = _mm_and_si128(dstpx, temp0); // effective values of dstp

            srcpf = _mm_or_si128(srcpf, temp0); // all effective values

            _mm_store_si128((__m128i *)buffer[f].buff, srcpf);
        }

        // I want _mm_mulhi_epi32(), but it doesn't exists.
        for (int t = 0; t < 8; t++) {
            uint32_t sum = half_frames;
            for (f = 0; f < frames; sum += buffer[f++].buff[t]);
            dstp[t] = (uint16_t)((sum * r + (1 << 20)) >> 40);
        }

        dstp += 8;
        for (f = 1; f < frames; srcp[f++] += 8);
    } while ((frame_size -= 16) > 0);
}


static void VS_CC
temporalSoftenInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node,
                   VSCore *core, const VSAPI *vsapi)
{
    TemporalSoftenData *d = (TemporalSoftenData *)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}


static int max(int n, int m)
{
    if (n < m) {
        return m;
    }
    return n;
}

static int min(int n, int m)
{
    if (n > m) {
        return m;
    }
    return n;
}

static const VSFrameRef *VS_CC
temporalSoftenGetFrame(int n, int activationReason, void **instanceData,
                       void **frameData, VSFrameContext *frameCtx, VSCore *core,
                       const VSAPI *vsapi)
{
    TemporalSoftenData *d = (TemporalSoftenData *)*instanceData;
    n = min(max(n, 0), d->vi->numFrames - 1);
    int first = max(n - d->radius, 0);
    int last = min(n + d->radius, d->vi->numFrames - 1);
    int i;

    if (activationReason == arInitial) {
        for (i = first; i <= last; i++) {
            vsapi->requestFrameFilter(i, d->node, frameCtx);
        }
        return NULL;
    }

    if (activationReason != arAllFramesReady) {
        return NULL;
    }

    // Not sure why 16... the most we can have is 7*2+1
    const VSFrameRef *src[16] = {
        vsapi->getFrameFilter(n, d->node, frameCtx), // frame n is always src[0]
        NULL
    };
    int frames = 1; // number of effective source frames
    int sc_prev = 0, sc_next = 0; // flags for checking scene change
    if (d->scenechange != 0) {
        sc_prev = vsapi->propGetInt(vsapi->getFramePropsRO(src[0]),
                                    "_SceneChangePrev", 0, 0);
        sc_next = vsapi->propGetInt(vsapi->getFramePropsRO(src[0]),
                                    "_SceneChangeNext", 0, 0);
    }

    int num = n - first;
    for (i = 1; i <= num; i++) {
        src[frames] = vsapi->getFrameFilter(n - i, d->node, frameCtx);
        if (sc_prev != 0) {
            vsapi->freeFrame(src[frames]); // Release garbage immediately
            continue;
        }
        if (d->scenechange != 0) {
            sc_prev = vsapi->propGetInt(vsapi->getFramePropsRO(src[frames]),
                                        "_SceneChangePrev", 0, 0);
        }
        frames++;
    }
    num = last - n;
    for (i = 1; i <= num; i++) {
        src[frames] = vsapi->getFrameFilter(n + i, d->node, frameCtx);
        if (sc_next != 0) {
            vsapi->freeFrame(src[frames]);
            continue;
        }
        if (d->scenechange != 0) {
            sc_next = vsapi->propGetInt(vsapi->getFramePropsRO(src[frames]),
                                        "_SceneChangeNext", 0, 0);
        }
        frames++;
    }

    VSFrameRef *dst = vsapi->copyFrame(src[0], core);

    int plane;
    num = d->vi->format->numPlanes;
    for (plane = 0; plane < num; plane++) {
        if (d->threshold[plane] == 0) {
            continue;
        }
        const uint8_t *srcp[16];
        for (i = 0; i < frames; i++) {
            srcp[i] = vsapi->getReadPtr(src[i], plane);
        }
        int frame_size = vsapi->getFrameHeight(dst, plane) *
                         vsapi->getStride(dst, plane);
        uint8_t *dstp = vsapi->getWritePtr(dst, plane);

        d->proc(dstp, srcp, frames, frame_size, d->threshold[plane]);
    }

    for (i = 0; i < frames; i++) {
        vsapi->freeFrame(src[i]);
    }

    return dst;
}


static void VS_CC
temporalSoftenFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    TemporalSoftenData *d = (TemporalSoftenData *)instanceData;
    vsapi->freeNode(d->node);
    free(d);
}


#define FAIL_IF_ERROR(cond, ...) {\
    if (cond) {\
        snprintf(msg, 235, __VA_ARGS__);\
        goto fail;\
    }\
}

static void VS_CC
temporalSoftenCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core,
                     const VSAPI *vsapi) {
    TemporalSoftenData d = { 0 };
    int err;
    char msg_buff[256] = "TemporalSoften2: ";
    char *msg = msg_buff + strlen(msg_buff);

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    FAIL_IF_ERROR(!d.vi->format || d.vi->width == 0 || d.vi->height == 0,
                  "clip must be constant format");

    FAIL_IF_ERROR(d.vi->format->sampleType != stInteger ||
                  d.vi->format->bitsPerSample > 16 ||
                  (d.vi->format->colorFamily != cmYUV &&
                   d.vi->format->colorFamily != cmRGB &&
                   d.vi->format->colorFamily != cmGray),
                  "only 8..16 bit integer YUV, RGB, or Gray input supported");

    int bshift = d.vi->format->bitsPerSample - 8;

    d.radius = vsapi->propGetInt(in, "radius", 0, &err);
    if (err) {
        d.radius = 4;
    }

    d.threshold[0] = vsapi->propGetInt(in, "luma_threshold", 0, &err);
    if (err) {
        d.threshold[0] = (4 << bshift);
    }
    if (d.vi->format->colorFamily == cmRGB) {
        d.threshold[1] = d.threshold[0];
    }

    if (d.vi->format->colorFamily == cmYUV) {
        d.threshold[1] = vsapi->propGetInt(in, "chroma_threshold", 0, &err);
        if (err) {
            d.threshold[1] = (8 << bshift);
        }
    }
    d.threshold[2] = d.threshold[1];

    d.scenechange = vsapi->propGetInt(in, "scenechange", 0, &err);
    if (err) {
        d.scenechange = 1;
    }
    d.mode = vsapi->propGetInt(in, "mode", 0, &err);
    if (err) {
        d.mode = 2;
    }

    int maximum = 0xFF << bshift;

    FAIL_IF_ERROR(d.radius < 1 || d.radius > 7,
                  "radius must be between 1 and 7 (inclusive)");

    FAIL_IF_ERROR(d.threshold[0] < 0 || d.threshold[0] > maximum,
                  "luma_threshold must be between 0 and %d (inclusive)", maximum);

    FAIL_IF_ERROR(d.threshold[1] < 0 || d.threshold[2] > maximum,
                  "chroma_threshold must be between 0 and %d (inclusive)", maximum);

    FAIL_IF_ERROR(d.threshold[0] == 0 &&
                  (d.vi->format->colorFamily == cmRGB ||
                   d.vi->format->colorFamily == cmGray),
                  "luma_threshold must not be 0 when the input is RGB or Gray");

    FAIL_IF_ERROR(d.threshold[0] == 0 && d.threshold[1] == 0,
                  "luma_threshold and chroma_threshold can't both be 0");

    FAIL_IF_ERROR(d.mode != 2, "mode must be 2. mode 1 is not implemented");

    switch (bshift) {
    case 0:
        d.proc =  mode2_8bit_sse2;
        break;
    case 1:
    case 2:
        d.proc = mode2_9_or_10_sse2;
        break;
    default:
        d.proc = mode2_16bit_sse2;
    }

    TemporalSoftenData *data = (TemporalSoftenData *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "TemporalSoften2", temporalSoftenInit,
                        temporalSoftenGetFrame, temporalSoftenFree, fmParallel,
                        0, data, core);
    return;

fail:
    vsapi->freeNode(d.node);
    vsapi->setError(out, msg_buff);
}


VS_EXTERNAL_API(void)
VapourSynthPluginInit(VSConfigPlugin configFunc,
                      VSRegisterFunction registerFunc, VSPlugin *plugin)
{
    configFunc("chikuzen.does.not.have.his.own.domain.focus2", "focus2",
               "VapourSynth TemporalSoften Filter v" TEMPORALSOFTEN2_VERSION,
               VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("TemporalSoften2",
                 "clip:clip;radius:int:opt;luma_threshold:int:opt;"
                 "chroma_threshold:int:opt;scenechange:int:opt;mode:int:opt;",
                 temporalSoftenCreate, 0, plugin);
}
