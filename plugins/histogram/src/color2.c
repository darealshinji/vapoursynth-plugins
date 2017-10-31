#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <VapourSynth.h>

#include "common.h"

typedef struct {
    VSNodeRef *node;
    VSVideoInfo vi;

    int deg15cos[24];
    int deg15sin[24];
} Color2Data;


static void VS_CC color2Init(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    Color2Data *d = (Color2Data *)* instanceData;
    vsapi->setVideoInfo(&d->vi, 1, node);

    for (int i = 0; i < 24; i++) {
        d->deg15cos[i] = (int)(126.0 * cos(i * 3.14159 / 12.0) + 0.5) + 127;
        d->deg15sin[i] = (int)(-126.0 * sin(i * 3.14159 / 12.0) + 0.5) + 127;
    }
}


static const VSFrameRef *VS_CC color2GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    Color2Data *d = (Color2Data *)* instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);

        const VSFormat *fi = d->vi.format;
        int height = MAX(256, vsapi->getFrameHeight(src, 0));
        int width = vsapi->getFrameWidth(src, 0) + 256;

        VSFrameRef *dst = vsapi->newVideoFrame(fi, width, height, src, core);

        const uint8_t *srcp[fi->numPlanes];
        int src_stride[fi->numPlanes];

        uint8_t *dstp[fi->numPlanes];
        int dst_stride[fi->numPlanes];

        int src_height[fi->numPlanes];
        int src_width[fi->numPlanes];

        int dst_height[fi->numPlanes];

        int y;
        int x;

        int plane;

        for (plane = 0; plane < fi->numPlanes; plane++) {
            srcp[plane] = vsapi->getReadPtr(src, plane);
            src_stride[plane] = vsapi->getStride(src, plane);

            dstp[plane] = vsapi->getWritePtr(dst, plane);
            dst_stride[plane] = vsapi->getStride(dst, plane);

            src_height[plane] = vsapi->getFrameHeight(src, plane);
            src_width[plane] = vsapi->getFrameWidth(src, plane);

            dst_height[plane] = vsapi->getFrameHeight(dst, plane);

            // Copy src to dst one line at a time.
            for (y = 0; y < src_height[plane]; y++) {
                memcpy(dstp[plane] + dst_stride[plane] * y,
                    srcp[plane] + src_stride[plane] * y,
                    src_stride[plane]);
            }

            // If src was less than 256 px tall, make the extra lines black.
            if (src_height[plane] < dst_height[plane]) {
                memset(dstp[plane] + src_height[plane] * dst_stride[plane],
                    (plane == 0) ? 16 : 128,
                    (dst_height[plane] - src_height[plane]) * dst_stride[plane]);
            }
        }

        // Clear the luma.
        for (y = 0; y < dst_height[Y]; y++) {
            memset(dstp[Y] + src_width[Y] + y * dst_stride[Y], 16, 256);
        }

        int subW = fi->subSamplingW;
        int subH = fi->subSamplingH;

        // Clear the chroma.
        for (y = 0; y < dst_height[U]; y++) {
            memset(dstp[U] + src_width[U] + y * dst_stride[U], 128, 256 >> subW);
            memset(dstp[V] + src_width[V] + y * dst_stride[V], 128, 256 >> subW);
        }

        // Draw the gray square.
        memset(dstp[Y] + src_width[Y] + 16 * dst_stride[Y] + 16, 128, 225); // top
        memset(dstp[Y] + src_width[Y] + 240 * dst_stride[Y] + 16, 128, 225); // bottom
        for (y = 17; y < 240; y++) {
            dstp[Y][src_width[Y] + y * dst_stride[Y] + 16] = 128;
            dstp[Y][src_width[Y] + y * dst_stride[Y] + 240] = 128;
        }

        // Original comments:
        // six hues in the color-wheel:
        // LC[3j,3j+1,3j+2], RC[3j,3j+1,3j+2] in YRange[j]+1 and YRange[j+1]
        int Yrange[8] = { -1, 26, 104, 127, 191, 197, 248, 256 };
        // 2x green, 2x yellow, 3x red
        int LC[21] = { 145,54,34, 145,54,34, 210,16,146, 210,16,146, 81,90,240, 81,90,240, 81,90,240 };
        // cyan, 4x blue, magenta, red:
        int RC[21] = { 170,166,16, 41,240,110, 41,240,110, 41,240,110, 41,240,110, 106,202,222, 81,90,240 };

        // example boundary of cyan and blue:
        // red = min(r,g,b), blue if g < 2/3 b, green if b < 2/3 g.
        // cyan between green and blue.
        // thus boundary of cyan and blue at (r,g,b) = (0,170,255), since 2/3*255 = 170.
        // => yuv = (127,190,47); hue = -52 degr; sat = 103
        // => u'v' = (207,27) (same hue, sat=128)
        // similar for the other hues.
        // luma

        float innerF = 124.9f; // .9 is for better visuals in subsampled mode
        float thicknessF = 1.5f;
        float oneOverThicknessF = 1.0f / thicknessF;
        float outerF = innerF + thicknessF * 2.0f;
        float centerF = innerF + thicknessF;
        int innerSq = (int)(innerF * innerF);
        int outerSq = (int)(outerF * outerF);
        int activeY = 0;
        int xRounder = (1 << subW) / 2;
        int yRounder = (1 << subH) / 2;

        // Draw the circle.
        for (y = -127; y < 128; y++) {
            if (y + 127 > Yrange[activeY + 1]) {
                activeY++;
            }
            for (x = -127; x <= 0; x++) {
                int distSq = x * x + y * y;
                if (distSq <= outerSq && distSq >= innerSq) {
                    int interp = (int)(256.0f - (255.9f * (oneOverThicknessF * fabs(sqrt((float)distSq) - centerF))));
                    // 255.9 is to account for float imprecision, which could cause underflow.

                    int xP = 127 + x;
                    int yP = 127 + y;

                    dstp[Y][src_width[Y] + xP + yP * dst_stride[Y]] = (interp * LC[3 * activeY]) >> 8; // left upper half
                    dstp[Y][src_width[Y] + 255 - xP + yP * dst_stride[Y]] = (interp * RC[3 * activeY]) >> 8; // right upper half

                    xP = (xP + xRounder) >> subW;
                    yP = (yP + yRounder) >> subH;

                    interp = MIN(256, interp);
                    int invInt = 256 - interp;

                    dstp[U][src_width[U] + xP + yP * dst_stride[U]] = (dstp[U][src_width[U] + xP + yP * dst_stride[U]] * invInt + interp * LC[3 * activeY + 1]) >> 8; // left half
                    dstp[V][src_width[V] + xP + yP * dst_stride[V]] = (dstp[V][src_width[V] + xP + yP * dst_stride[V]] * invInt + interp * LC[3 * activeY + 2]) >> 8; // left half

                    xP = (255 >> subW) - xP;
                    dstp[U][src_width[U] + xP + yP * dst_stride[U]] = (dstp[U][src_width[U] + xP + yP * dst_stride[U]] * invInt + interp * RC[3 * activeY + 1]) >> 8; // left half
                    dstp[V][src_width[V] + xP + yP * dst_stride[V]] = (dstp[V][src_width[V] + xP + yP * dst_stride[V]] * invInt + interp * RC[3 * activeY + 2]) >> 8; // left half
                }
            }
        }

        // Draw the white dots every 15 degrees.
        for (int i = 0; i < 24; i++) {
            dstp[Y][src_width[Y] + d->deg15cos[i] + d->deg15sin[i] * dst_stride[Y]] = 235;
        }

        // Draw the vectorscope(!).
        for (y = 0; y < src_height[U]; y++) {
            for (x = 0; x < src_width[U]; x++) {
                int uval = srcp[U][x + y * src_stride[U]];
                int vval = srcp[V][x + y * src_stride[V]];

                dstp[Y][src_width[Y] + uval + vval * dst_stride[Y]] = srcp[Y][(x << subW) + y * (src_stride[Y] << subH)];
                dstp[U][src_width[U] + (uval >> subW) + (vval >> subW) * dst_stride[U]] = uval;
                dstp[V][src_width[V] + (uval >> subW) + (vval >> subW) * dst_stride[V]] = vval;
            }
        }


        // Release the source frame
        vsapi->freeFrame(src);

        // A reference is consumed when it is returned so saving the dst ref somewhere
        // and reusing it is not allowed.
        return dst;
    }

    return 0;
}


static void VS_CC color2Free(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    Color2Data *d = (Color2Data *)instanceData;
    vsapi->freeNode(d->node);
    free(d);
}


void VS_CC color2Create(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    Color2Data d;
    Color2Data *data;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = *vsapi->getVideoInfo(d.node);

    if (!d.vi.format || d.vi.format->sampleType != stInteger || d.vi.format->bitsPerSample != 8) {
        vsapi->setError(out, "Color2: only constant format 8bit integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi.width)
        d.vi.width += 256;
    if (d.vi.height)
        d.vi.height = MAX(256, d.vi.height);

    data = malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Color2", color2Init, color2GetFrame, color2Free, fmParallel, 0, data, core);
    return;
}