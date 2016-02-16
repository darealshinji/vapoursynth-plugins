#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <VapourSynth.h>
#include <VSHelper.h>


typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    double threshold;
    double strength;
    int mask;
    int process[3];
} MSmoothData;


static void VS_CC msmoothInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    MSmoothData *d = (MSmoothData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}


template <typename T>
static inline void blur3x3(void *maskpv, const void *srcpv, int stride, int width, int height) {
    T *maskp = (T *)maskpv;
    const T *srcp = (const T *)srcpv;

    stride /= sizeof(T);

    maskp[0] = (srcp[0] + srcp[1] +
                srcp[stride] + srcp[stride + 1]) / 4;

    for (int x = 1; x < width - 1; x++)
        maskp[x] = (srcp[x - 1] + srcp[x] + srcp[x + 1] +
                    srcp[x + stride - 1] + srcp[x + stride] + srcp[x + stride + 1]) / 6;

    maskp[width - 1] = (srcp[width - 2] + srcp[width - 1] +
                        srcp[width + stride - 2] + srcp[width + stride - 1]) / 4;

    srcp += stride;
    maskp += stride;

    for (int y = 1; y < height - 1; y++) {
        maskp[0] = (srcp[-stride] + srcp[-stride + 1] +
                    srcp[0] + srcp[1] +
                    srcp[stride] + srcp[stride + 1]) / 6;

        for (int x = 1; x < width - 1; x++)
            maskp[x] = (srcp[x - stride - 1] + srcp[x - stride] + srcp[x - stride + 1] +
                        srcp[x - 1] + srcp[x] + srcp[x + 1] +
                        srcp[x + stride - 1] + srcp[x + stride] + srcp[x + stride + 1]) / 9;

        maskp[width - 1] = (srcp[width - stride - 2] + srcp[width - stride - 1] +
                            srcp[width - 2] + srcp[width - 1] +
                            srcp[width + stride - 2] + srcp[width + stride - 1]) / 6;

        srcp += stride;
        maskp += stride;
    }

    maskp[0] = (srcp[-stride] + srcp[-stride + 1] +
                srcp[0] + srcp[1]) / 4;

    for (int x = 1; x < width - 1; x++)
        maskp[x] = (srcp[x - stride - 1] + srcp[x - stride] + srcp[x - stride + 1] +
                    srcp[x - 1] + srcp[x] + srcp[x + 1]) / 6;

    maskp[width - 1] = (srcp[width - stride - 2] + srcp[width - stride - 1] +
                        srcp[width - 2] + srcp[width - 1]) / 4;
}


template <typename T, bool rgb>
static inline void findEdges(void **maskpv, int stride, int width, int height, int th, int maximum) {
    T *maskp[3];
    maskp[0] = (T *)maskpv[0];
    maskp[1] = (T *)maskpv[1];
    maskp[2] = (T *)maskpv[2];

    stride /= sizeof(T);

    for (int y = 0; y < height - 1; y++) {
        for (int x = 0; x < width - 1; x++) {
            int edge = abs(maskp[0][x] - maskp[0][x + stride + 1]) >= th ||
                       abs(maskp[0][x + 1] - maskp[0][x + stride]) >= th ||
                       abs(maskp[0][x] - maskp[0][x + 1]) >= th ||
                       abs(maskp[0][x] - maskp[0][x + stride]) >= th;
            if (rgb) {
                int edge2 = abs(maskp[1][x] - maskp[1][x + stride + 1]) >= th ||
                            abs(maskp[1][x + 1] - maskp[1][x + stride]) >= th ||
                            abs(maskp[1][x] - maskp[1][x + 1]) >= th ||
                            abs(maskp[1][x] - maskp[1][x + stride]) >= th;
                int edge3 = abs(maskp[2][x] - maskp[2][x + stride + 1]) >= th ||
                            abs(maskp[2][x + 1] - maskp[2][x + stride]) >= th ||
                            abs(maskp[2][x] - maskp[2][x + 1]) >= th ||
                            abs(maskp[2][x] - maskp[2][x + stride]) >= th;
                edge = edge || edge2 || edge3;
            }
                
            if (edge)
                maskp[0][x] = maximum;
            else
                maskp[0][x] = 0;
        }

        int edge = abs(maskp[0][width - 1] - maskp[0][width + stride - 1]) >= th;
        if (rgb) {
            int edge2 = abs(maskp[1][width - 1] - maskp[1][width + stride - 1]) >= th;
            int edge3 = abs(maskp[2][width - 1] - maskp[2][width + stride - 1]) >= th;
            edge = edge || edge2 || edge3;
        }

        if (edge)
            maskp[0][width - 1] = maximum;
        else
            maskp[0][width - 1] = 0;

        maskp[0] += stride;
        if (rgb) {
            maskp[1] += stride;
            maskp[2] += stride;
        }
    }

    for (int x = 0; x < width - 1; x++) {
        int edge = abs(maskp[0][x] - maskp[0][x + 1]) >= th;
        if (rgb) {
            int edge2 = abs(maskp[1][x] - maskp[1][x + 1]) >= th;
            int edge3 = abs(maskp[2][x] - maskp[2][x + 1]) >= th;
            edge = edge || edge2 || edge3;
        }

        if (edge)
            maskp[0][x] = maximum;
        else
            maskp[0][x] = 0;
    }

    maskp[0][width - 1] = maximum;
}


template <typename T>
static inline void copyMask(void *dstpv, const void *srcpv, int dst_stride, int src_stride, int width, int height, int subSamplingW, int subSamplingH) {
    T *dstp = (T *)dstpv;
    const T *srcp = (const T *)srcpv;

    src_stride /= sizeof(T);
    dst_stride /= sizeof(T);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = srcp[x << subSamplingW];

        dstp += dst_stride;
        srcp += src_stride << subSamplingH;
    }
}


static inline int clamp(int val, int minimum, int maximum) {
    if (val < minimum)
        val = minimum;
    else if (val > maximum)
        val = maximum;

    return val;
}


template <typename T, bool rgb>
static void edgeMask(VSFrameRef *mask, const VSFrameRef *src, MSmoothData *d, const VSAPI *vsapi) {
    const uint8_t *srcp[3];
    uint8_t *maskp[3];

    srcp[0] = vsapi->getReadPtr(src, 0);
    maskp[0] = vsapi->getWritePtr(mask, 0);

    const int stride = vsapi->getStride(src, 0);

    const int width = vsapi->getFrameWidth(src, 0);
    const int height = vsapi->getFrameHeight(src, 0);

    const VSFormat *f = vsapi->getFrameFormat(mask);

    blur3x3<T>(maskp[0], srcp[0], stride, width, height);
    if (rgb) {
        srcp[1] = vsapi->getReadPtr(src, 1);
        srcp[2] = vsapi->getReadPtr(src, 2);
        maskp[1] = vsapi->getWritePtr(mask, 1);
        maskp[2] = vsapi->getWritePtr(mask, 2);

        blur3x3<T>(maskp[1], srcp[1], stride, width, height);
        blur3x3<T>(maskp[2], srcp[2], stride, width, height);
    }

    int maximum = 0xffff >> (16 - f->bitsPerSample);
    int threshold = (int)((d->threshold * maximum) / 100);
    threshold = clamp(threshold, 0, maximum);

    findEdges<T, rgb>((void **)maskp, stride, width, height, threshold, maximum);

    if (rgb) {
        if (d->mask) {
            memcpy(maskp[1], maskp[0], height * stride);
            memcpy(maskp[2], maskp[0], height * stride);
        }
    } else {
        if (f->numPlanes > 1 && (d->process[1] || d->process[2])) {
            uint8_t *maskpu = vsapi->getWritePtr(mask, 1);
            const int strideu = vsapi->getStride(mask, 1);
            const int widthu = vsapi->getFrameWidth(mask, 1);
            const int heightu = vsapi->getFrameHeight(mask, 1);

            copyMask<T>(maskpu, maskp[0], strideu, stride, widthu, heightu, f->subSamplingW, f->subSamplingH);

            if (d->mask)
                memcpy(vsapi->getWritePtr(mask, 2), vsapi->getWritePtr(mask, 1), heightu * strideu);
        }
    }
}


template <typename T>
static inline void maskedBlur3x3(void *dstpv, const void *srcpv, const void *maskpv, int stride, int width, int height) {
    T *dstp = (T *)dstpv;
    const T *srcp = (const T *)srcpv;
    const T *maskp = (const T *)maskpv;

    stride /= sizeof(T);

    if (!maskp[0]) {
        int count = 1;
        int sum = srcp[0];
        if (!maskp[1]) {
            sum += srcp[1];
            count++;
        }
        if (!maskp[stride]) {
            sum += srcp[stride];
            count++;
        }
        if (!maskp[stride + 1]) {
            sum += srcp[stride + 1];
            count++;
        }

        dstp[0] = (int)((float)sum / count + 0.5f);
    } else {
        dstp[0] = srcp[0];
    }

    for (int x = 1; x < width - 1; x++) {
        if (!maskp[x]) {
            int count = 1;
            int sum = 0;
            if (!maskp[x - 1]) {
                sum += srcp[x - 1];
                count++;
            }
            sum += srcp[x];
            if (!maskp[x + 1]) {
                sum += srcp[x + 1];
                count++;
            }
            if (!maskp[x + stride - 1]) {
                sum += srcp[x + stride - 1];
                count++;
            }
            if (!maskp[x + stride]) {
                sum += srcp[x + stride];
                count++;
            }
            if (!maskp[x + stride + 1]) {
                sum += srcp[x + stride + 1];
                count++;
            }

            dstp[x] = (int)((float)sum / count + 0.5f);
        } else {
            dstp[x] = srcp[x];
        }
    }

    if (!maskp[width - 1]) {
        int count = 1;
        int sum = 0;
        if (!maskp[width - 2]) {
            sum += srcp[width - 2];
            count++;
        }
        sum += srcp[width - 1];
        if (!maskp[width + stride - 2]) {
            sum += srcp[width + stride - 2];
            count++;
        }
        if (!maskp[width + stride - 1]) {
            sum += srcp[width + stride - 1];
            count++;
        }

        dstp[width - 1] = (int)((float)sum / count + 0.5f);
    } else {
        dstp[width - 1] = srcp[width - 1];
    }

    srcp += stride;
    dstp += stride;
    maskp += stride;

    for (int y = 1; y < height - 1; y++) {
        if (!maskp[0]) {
            int count = 1;
            int sum = 0;
            if (!maskp[-stride]) {
                sum += srcp[-stride];
                count++;
            }
            if (!maskp[-stride + 1]) {
                sum += srcp[-stride + 1];
                count++;
            }
            sum += srcp[0];
            if (!maskp[1]) {
                sum += srcp[1];
                count++;
            }
            if (!maskp[stride]) {
                sum += srcp[stride];
                count++;
            }
            if (!maskp[stride + 1]) {
                sum += srcp[stride + 1];
                count++;
            }

            dstp[0] = (int)((float)sum / count + 0.5f);
        } else {
            dstp[0] = srcp[0];
        }

        for (int x = 1; x < width - 1; x++) {
            if (!maskp[x]) {
                int count = 1;
                int sum = 0;
                if (!maskp[x - stride - 1]) {
                    sum += srcp[x - stride - 1];
                    count++;
                }
                if (!maskp[x - stride]) {
                    sum += srcp[x - stride];
                    count++;
                }
                if (!maskp[x - stride + 1]) {
                    sum += srcp[x - stride + 1];
                    count++;
                }
                if (!maskp[x - 1]) {
                    sum += srcp[x - 1];
                    count++;
                }
                sum += srcp[x];
                if (!maskp[x + 1]) {
                    sum += srcp[x + 1];
                    count++;
                }
                if (!maskp[x + stride - 1]) {
                    sum += srcp[x + stride - 1];
                    count++;
                }
                if (!maskp[x + stride]) {
                    sum += srcp[x + stride];
                    count++;
                }
                if (!maskp[x + stride + 1]) {
                    sum += srcp[x + stride + 1];
                    count++;
                }

                dstp[x] = (int)((float)sum / count + 0.5f);
            } else {
                dstp[x] = srcp[x];
            }
        }

        if (!maskp[width - 1]) {
            int count = 1;
            int sum = 0;
            if (!maskp[width - stride - 2]) {
                sum += srcp[width - stride - 2];
                count++;
            }
            if (!maskp[width - stride - 1]) {
                sum += srcp[width - stride - 1];
                count++;
            }
            if (!maskp[width - 2]) {
                sum += srcp[width - 2];
                count++;
            }
            sum += srcp[width - 1];
            if (!maskp[width + stride - 2]) {
                sum += srcp[width + stride - 2];
                count++;
            }
            if (!maskp[width + stride - 1]) {
                sum += srcp[width + stride - 1];
                count++;
            }

            dstp[width - 1] = (int)((float)sum / count + 0.5f);
        } else {
            dstp[width - 1] = srcp[width - 1];
        }

        srcp += stride;
        dstp += stride;
        maskp += stride;
    }

    if (!maskp[0]) {
        int count = 1;
        int sum = 0;
        if (!maskp[-stride]) {
            sum += srcp[-stride];
            count++;
        }
        if (!maskp[-stride + 1]) {
            sum += srcp[-stride + 1];
            count++;
        }
        sum += srcp[0];
        if (!maskp[1]) {
            sum += srcp[1];
            count++;
        }

        dstp[0] = (int)((float)sum / count + 0.5f);
    } else {
        dstp[0] = srcp[0];
    }

    for (int x = 1; x < width - 1; x++) {
        if (!maskp[x]) {
            int count = 1;
            int sum = 0;
            if (!maskp[x - stride - 1]) {
                sum += srcp[x - stride - 1];
                count++;
            }
            if (!maskp[x - stride]) {
                sum += srcp[x - stride];
                count++;
            }
            if (!maskp[x - stride + 1]) {
                sum += srcp[x - stride + 1];
                count++;
            }
            if (!maskp[x - 1]) {
                sum += srcp[x - 1];
                count++;
            }
            sum += srcp[x];
            if (!maskp[x + 1]) {
                sum += srcp[x + 1];
                count++;
            }

            dstp[x] = (int)((float)sum / count + 0.5f);
        } else {
            dstp[x] = srcp[x];
        }
    }

    if (!maskp[width - 1]) {
        int count = 1;
        int sum = 0;
        if (!maskp[width - stride - 2]) {
            sum += srcp[width - stride - 2];
            count++;
        }
        if (!maskp[width - stride - 1]) {
            sum += srcp[width - stride - 1];
            count++;
        }
        if (!maskp[width - 2]) {
            sum += srcp[width - 2];
            count++;
        }
        sum += srcp[width - 1];

        dstp[width - 1] = (int)((float)sum / count + 0.5f);
    } else {
        dstp[width - 1] = srcp[width - 1];
    }
}


template <typename T>
static void smooth(VSFrameRef *dst, const VSFrameRef *src, const VSFrameRef *mask, MSmoothData *d, const VSAPI *vsapi) {
    if (d->process[0]) {
        const uint8_t *srcp = vsapi->getReadPtr(src, 0);
        const uint8_t *maskp = vsapi->getReadPtr(mask, 0);
        uint8_t *dstp = vsapi->getWritePtr(dst, 0);

        const int stride = vsapi->getStride(src, 0);
        const int width = vsapi->getFrameWidth(src, 0);
        const int height = vsapi->getFrameHeight(src, 0);

        maskedBlur3x3<T>(dstp, srcp, maskp, stride, width, height);
    }

    const VSFormat *f = vsapi->getFrameFormat(src);

    if (f->numPlanes > 1) {
        const uint8_t *maskpu = vsapi->getReadPtr(mask, f->colorFamily == cmRGB ? 0 : 1);

        if (d->process[1]) {
            const uint8_t *srcpu = vsapi->getReadPtr(src, 1);
            uint8_t *dstpu = vsapi->getWritePtr(dst, 1);

            const int strideu = vsapi->getStride(src, 1);
            const int widthu = vsapi->getFrameWidth(src, 1);
            const int heightu = vsapi->getFrameHeight(src, 1);

            maskedBlur3x3<T>(dstpu, srcpu, maskpu, strideu, widthu, heightu);
        }

        if (d->process[2]) {
            const uint8_t *srcpv = vsapi->getReadPtr(src, 2);
            uint8_t *dstpv = vsapi->getWritePtr(dst, 2);

            const int stridev = vsapi->getStride(src, 2);
            const int widthv = vsapi->getFrameWidth(src, 2);
            const int heightv = vsapi->getFrameHeight(src, 2);

            maskedBlur3x3<T>(dstpv, srcpv, maskpu, stridev, widthv, heightv);
        }
    }
}


static const VSFrameRef *VS_CC msmoothGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    MSmoothData *d = (MSmoothData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);

        const VSFormat *f = vsapi->getFrameFormat(src);

        if (f->bytesPerSample > 2 || f->sampleType != stInteger)
            return src;

        int width = vsapi->getFrameWidth(src, 0);
        int height = vsapi->getFrameHeight(src, 0);

        VSFrameRef *mask = vsapi->newVideoFrame(f, width, height, NULL, core);

        if (f->colorFamily == cmRGB) {
            if (f->bitsPerSample == 8)
                edgeMask<uint8_t, true>(mask, src, d, vsapi);
            else
                edgeMask<uint16_t, true>(mask, src, d, vsapi);
        } else {
            if (f->bitsPerSample == 8)
                edgeMask<uint8_t, false>(mask, src, d, vsapi);
            else
                edgeMask<uint16_t, false>(mask, src, d, vsapi);
        }

        if (d->mask) {
            vsapi->freeFrame(src);
            return mask;
        }

        const VSFrameRef *frames[3] = { d->process[0] ? NULL : src, d->process[1] ? NULL : src, d->process[2] ? NULL : src };
        const int planes[3] = { 0, 1, 2 };

        VSFrameRef *temp = vsapi->newVideoFrame2(f, width, height, frames, planes, src, core);
        VSFrameRef *dst = vsapi->newVideoFrame2(f, width, height, frames, planes, src, core);

        if (f->bitsPerSample == 8)
            smooth<uint8_t>(dst, src, mask, d, vsapi);
        else
            smooth<uint16_t>(dst, src, mask, d, vsapi);

        for (int i = 1; i < (int)(d->strength + 0.5); i++) {
            VSFrameRef *t = temp;
            temp = dst;
            dst = t;
            if (f->bitsPerSample == 8)
                smooth<uint8_t>(dst, temp, mask, d, vsapi);
            else
                smooth<uint16_t>(dst, temp, mask, d, vsapi);
        }

        vsapi->freeFrame(src);
        vsapi->freeFrame(temp);
        vsapi->freeFrame(mask);

        return dst;
    }

    return 0;
}


template <typename T>
static inline void msharpenFindEdges(void *maskpv, const void *srcpv, int stride, int width, int height, int th, int maximum) {
    T *maskp = (T *)maskpv;
    const T *srcp = (const T *)srcpv;

    stride /= sizeof(T);

    for (int y = 0; y < height - 1; y++) {
        for (int x = 0; x < width - 1; x++) {
            int edge = abs(srcp[x] - srcp[x + stride + 1]) >= th ||
                       abs(srcp[x + 1] - srcp[x + stride]) >= th ||
                       abs(srcp[x] - srcp[x + 1]) >= th ||
                       abs(srcp[x] - srcp[x + stride]) >= th;

            if (edge)
                maskp[x] = maximum;
            else
                maskp[x] = 0;
        }

        int edge = abs(srcp[width - 1] - srcp[width + stride - 1]) >= th;

        if (edge)
            maskp[width - 1] = maximum;
        else
            maskp[width - 1] = 0;

        maskp += stride;
        srcp += stride;
    }

    for (int x = 0; x < width - 1; x++) {
        int edge = abs(srcp[x] - srcp[x + 1]) >= th;

        if (edge)
            maskp[x] = maximum;
        else
            maskp[x] = 0;
    }

    maskp[width - 1] = maximum;
}


template <typename T>
static void msharpenEdgeMask(VSFrameRef *mask, VSFrameRef *blur, const VSFrameRef *src, MSmoothData *d, const VSAPI *vsapi) {
    const uint8_t *srcp[3];
    uint8_t *maskp[3];
    uint8_t *blurp[3];
    int width[3];
    int height[3];
    int stride[3];

    const VSFormat *f = vsapi->getFrameFormat(src);

    int maximum = 0xffff >> (16 - f->bitsPerSample);
    int threshold = (int)((d->threshold * maximum) / 100);
    threshold = clamp(threshold, 0, maximum);

    for (int plane = 0; plane < f->numPlanes; plane++) {
        if (!d->process[plane])
            continue;

        srcp[plane] = vsapi->getReadPtr(src, plane);
        maskp[plane] = vsapi->getWritePtr(mask, plane);
        blurp[plane] = vsapi->getWritePtr(blur, plane);

        stride[plane] = vsapi->getStride(src, plane);

        width[plane] = vsapi->getFrameWidth(src, plane);
        height[plane] = vsapi->getFrameHeight(src, plane);

        blur3x3<T>(blurp[plane], srcp[plane], stride[plane], width[plane], height[plane]);

        msharpenFindEdges<T>(maskp[plane], blurp[plane], stride[plane], width[plane], height[plane], threshold, maximum);
    }

    if (f->colorFamily == cmRGB && d->process[0] && d->process[1] && d->process[2]) {
        for (int x = 0; x < height[0] * stride[0]; x++)
            maskp[0][x] = maskp[0][x] | maskp[1][x] | maskp[2][x];

        memcpy(maskp[1], maskp[0], height[0] * stride[0]);
        memcpy(maskp[2], maskp[0], height[0] * stride[0]);
    }
}


template <typename T>
static void sharpen(VSFrameRef *dst, const VSFrameRef *blur, const VSFrameRef *src, MSmoothData *d, const VSAPI *vsapi) {
    const VSFormat *f = vsapi->getFrameFormat(src);

    int maximum = 0xffff >> (16 - f->bitsPerSample);
    int strength = (int)((d->strength * maximum) / 100);
    strength = clamp(strength, 0, maximum);

    int invstrength = maximum - strength;

    for (int plane = 0; plane < f->numPlanes; plane++) {
        if (!d->process[plane])
            continue;

        const T *srcp = (const T *)vsapi->getReadPtr(src, plane);
        const T *blurp = (const T *)vsapi->getReadPtr(blur, plane);
        T *dstp = (T *)vsapi->getWritePtr(dst, plane);

        int stride = vsapi->getStride(src, plane);
        int width = vsapi->getFrameWidth(src, plane);
        int height = vsapi->getFrameHeight(src, plane);

        stride /= sizeof(T);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (dstp[x]) {
                    int tmp = 4 * srcp[x] - 3 * blurp[x];
                    tmp = clamp(tmp, 0, maximum);

                    dstp[x] = (strength * tmp + invstrength * srcp[x]) >> f->bitsPerSample;
                } else {
                    dstp[x] = srcp[x];
                }
            }

            srcp += stride;
            dstp += stride;
            blurp += stride;
        }
    }
}


static const VSFrameRef *VS_CC msharpenGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    MSmoothData *d = (MSmoothData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);

        const VSFormat *f = vsapi->getFrameFormat(src);

        if (f->bytesPerSample > 2 || f->sampleType != stInteger)
            return src;

        int width = vsapi->getFrameWidth(src, 0);
        int height = vsapi->getFrameHeight(src, 0);

        VSFrameRef *blur = vsapi->newVideoFrame(f, width, height, NULL, core);

        const VSFrameRef *frames[3] = { d->process[0] ? NULL : src, d->process[1] ? NULL : src, d->process[2] ? NULL : src };
        const int planes[3] = { 0, 1, 2 };

        VSFrameRef *dst = vsapi->newVideoFrame2(f, width, height, frames, planes, src, core);

        if (f->bitsPerSample == 8)
            msharpenEdgeMask<uint8_t>(dst, blur, src, d, vsapi);
        else
            msharpenEdgeMask<uint16_t>(dst, blur, src, d, vsapi);

        if (!d->mask) {
            if (f->bitsPerSample == 8)
                sharpen<uint8_t>(dst, blur, src, d, vsapi);
            else
                sharpen<uint16_t>(dst, blur, src, d, vsapi);
        }

        vsapi->freeFrame(src);
        vsapi->freeFrame(blur);

        return dst;
    }

    return 0;
}


static void VS_CC msmoothFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    MSmoothData *d = (MSmoothData *)instanceData;

    vsapi->freeNode(d->node);
    free(d);
}


static void VS_CC msmoothCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    MSmoothData d;
    MSmoothData *data;

    int err;
    int i, m, n, o;

    d.threshold = vsapi->propGetFloat(in, "threshold", 0, &err);
    if (err)
        d.threshold = 6;

    if (d.threshold < 0.0 || d.threshold > 100.0) {
        vsapi->setError(out, "MSmooth: threshold must be between 0 and 100 %.");
        return;
    }

    d.strength = vsapi->propGetInt(in, "strength", 0, &err);
    if (err)
        d.strength = 3;

    d.mask = !!vsapi->propGetInt(in, "mask", 0, &err);

    if (d.strength < 1 || d.strength > 25) {
        vsapi->setError(out, "MSmooth: strength must be between 1 and 25 (inclusive).");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    n = 3;
    m = vsapi->propNumElements(in, "planes");

    for (i = 0; i < 3; i++)
        d.process[i] = (m <= 0);

    for (i = 0; i < m; i++) {
        o = vsapi->propGetInt(in, "planes", i, 0);

        if (o < 0 || o >= n) {
            vsapi->setError(out, "MSmooth: Plane index out of range.");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.process[o]) {
            vsapi->setError(out, "MSmooth: Plane specified twice.");
            vsapi->freeNode(d.node);
            return;
        }

        d.process[o] = 1;
    }


    data = (MSmoothData *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "MSmooth", msmoothInit, msmoothGetFrame, msmoothFree, fmParallel, 0, data, core);
}


static void VS_CC msharpenCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    MSmoothData d;
    MSmoothData *data;

    int err;
    int i, m, n, o;

    d.threshold = vsapi->propGetFloat(in, "threshold", 0, &err);
    if (err)
        d.threshold = 6.0;

    d.strength = vsapi->propGetFloat(in, "strength", 0, &err);
    if (err)
        d.strength = 39.0;

    d.mask = !!vsapi->propGetInt(in, "mask", 0, &err);

    if (d.threshold < 0.0 || d.threshold > 100.0) {
        vsapi->setError(out, "MSharpen: threshold must be between 0 and 100 %.");
        return;
    }

    if (d.strength < 0.0 || d.strength > 100.0) {
        vsapi->setError(out, "MSharpen: strength must be between 0 and 100 %.");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    n = 3;
    m = vsapi->propNumElements(in, "planes");

    for (i = 0; i < 3; i++)
        d.process[i] = (m <= 0);

    for (i = 0; i < m; i++) {
        o = vsapi->propGetInt(in, "planes", i, 0);

        if (o < 0 || o >= n) {
            vsapi->setError(out, "MSharpen: Plane index out of range.");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.process[o]) {
            vsapi->setError(out, "MSharpen: Plane specified twice.");
            vsapi->freeNode(d.node);
            return;
        }

        d.process[o] = 1;
    }


    data = (MSmoothData *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "MSharpen", msmoothInit, msharpenGetFrame, msmoothFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.msmoosh", "msmoosh", "MSmooth and MSharpen", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("MSmooth",
                 "clip:clip;"
                 "threshold:float:opt;"
                 "strength:int:opt;"
                 "mask:int:opt;"
                 "planes:int[]:opt;"
                 , msmoothCreate, 0, plugin);
    registerFunc("MSharpen",
                 "clip:clip;"
                 "threshold:float:opt;"
                 "strength:float:opt;"
                 "mask:int:opt;"
                 "planes:int[]:opt;"
                 , msharpenCreate, 0, plugin);
}
