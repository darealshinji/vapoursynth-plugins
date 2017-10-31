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

#include <thread>
#include <unordered_map>
#include <vector>

#include "TCanny.hpp"

#ifdef VS_TARGET_CPU_X86
template<typename T> extern void copyPlane_sse2(const T *, float *, const int, const int, const int, const int, const float) noexcept;
template<typename T> extern void copyPlane_avx(const T *, float *, const int, const int, const int, const int, const float) noexcept;
template<typename T> extern void copyPlane_avx2(const T *, float *, const int, const int, const int, const int, const float) noexcept;

extern void gaussianBlurH_sse2(float *, float *, const float *, const int, const int) noexcept;
extern void gaussianBlurH_avx(float *, float *, const float *, const int, const int) noexcept;
extern void gaussianBlurH_avx2(float *, float *, const float *, const int, const int) noexcept;

template<typename T> extern void gaussianBlurV_sse2(const T *, float *, float *, const float *, const float *, const int, const int, const int, const int, const int, const int, const float) noexcept;
template<typename T> extern void gaussianBlurV_avx(const T *, float *, float *, const float *, const float *, const int, const int, const int, const int, const int, const int, const float) noexcept;
template<typename T> extern void gaussianBlurV_avx2(const T *, float *, float *, const float *, const float *, const int, const int, const int, const int, const int, const int, const float) noexcept;

extern void detectEdge_sse2(float *, float *, unsigned *, const int, const int, const int, const int, const int, const unsigned) noexcept;
extern void detectEdge_avx(float *, float *, unsigned *, const int, const int, const int, const int, const int, const unsigned) noexcept;
extern void detectEdge_avx2(float *, float *, unsigned *, const int, const int, const int, const int, const int, const unsigned) noexcept;

extern void nonMaximumSuppression_sse2(const unsigned *, float *, float *, const int, const int, const int, const int) noexcept;
extern void nonMaximumSuppression_avx(const unsigned *, float *, float *, const int, const int, const int, const int) noexcept;
extern void nonMaximumSuppression_avx2(const unsigned *, float *, float *, const int, const int, const int, const int) noexcept;

template<typename T> extern void outputGB_sse2(const float *, T *, const int, const int, const int, const int, const uint16_t, const float) noexcept;
template<typename T> extern void outputGB_avx(const float *, T *, const int, const int, const int, const int, const uint16_t, const float) noexcept;
template<typename T> extern void outputGB_avx2(const float *, T *, const int, const int, const int, const int, const uint16_t, const float) noexcept;

template<typename T> extern void binarizeCE_sse2(const float *, T *, const int, const int, const int, const int, const uint16_t, const float, const float) noexcept;
template<typename T> extern void binarizeCE_avx(const float *, T *, const int, const int, const int, const int, const uint16_t, const float, const float) noexcept;
template<typename T> extern void binarizeCE_avx2(const float *, T *, const int, const int, const int, const int, const uint16_t, const float, const float) noexcept;

template<typename T> extern void discretizeGM_sse2(const float *, T *, const int, const int, const int, const int, const float, const uint16_t, const float) noexcept;
template<typename T> extern void discretizeGM_avx(const float *, T *, const int, const int, const int, const int, const float, const uint16_t, const float) noexcept;
template<typename T> extern void discretizeGM_avx2(const float *, T *, const int, const int, const int, const int, const float, const uint16_t, const float) noexcept;
#endif

template<typename T> static void (*copyPlane)(const T *, float *, const int, const int, const int, const int, const float) = nullptr;
template<typename T> static void (*gaussianBlurV)(const T *, float *, float *, const float *, const float *, const int, const int, const int, const int, const int, const int, const float) = nullptr;
static void (*detectEdge)(float *, float *, unsigned *, const int, const int, const int, const int, const int, const unsigned) = nullptr;
static void (*nonMaximumSuppression)(const unsigned *, float *, float *, const int, const int, const int, const int) = nullptr;
template<typename T> static void (*outputGB)(const float *, T *, const int, const int, const int, const int, const uint16_t, const float) = nullptr;
template<typename T> static void (*binarizeCE)(const float *, T *, const int, const int, const int, const int, const uint16_t, const float, const float) = nullptr;
template<typename T> static void (*discretizeGM)(const float *, T *, const int, const int, const int, const int, const float, const uint16_t, const float) = nullptr;

struct TCannyData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    float t_h, t_l;
    int mode, op;
    bool process[3];
    float * horizontalWeights[3], * verticalWeights[3];
    int horizontalRadius[3], verticalRadius[3], radiusAlign;
    float magnitude;
    uint16_t peak;
    float offset[3], lower[3], upper[3];
    std::unordered_map<std::thread::id, float *> buffer, blur, gradient;
    std::unordered_map<std::thread::id, unsigned *> direction;
    std::unordered_map<std::thread::id, bool *> label;
};

template<typename T>
static void copyPlane_c(const T * srcp, float * VS_RESTRICT blur, const int width, const int height, const int stride, const int bgStride, const float offset) noexcept {
    if (std::is_integral<T>::value) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++)
                blur[x] = srcp[x];

            srcp += stride;
            blur += bgStride;
        }
    } else {
        if (offset) {
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++)
                    blur[x] = srcp[x] + offset;

                srcp += stride;
                blur += bgStride;
            }
        } else {
            vs_bitblt(blur, bgStride * sizeof(float), srcp, stride * sizeof(float), width * sizeof(float), height);
        }
    }
}

static inline void gaussianBlurH_c(float * VS_RESTRICT buffer, float * VS_RESTRICT blur, const float * weights, const int width, const int radius) noexcept {
    weights += radius;

    for (int i = 1; i <= radius; i++) {
        buffer[-i] = buffer[-1 + i];
        buffer[width - 1 + i] = buffer[width - i];
    }

    for (int x = 0; x < width; x++) {
        float sum = 0.f;

        for (int i = -radius; i <= radius; i++)
            sum += buffer[x + i] * weights[i];

        blur[x] = sum;
    }
}

template<typename T>
static void gaussianBlurV_c(const T * _srcp, float * VS_RESTRICT buffer, float * VS_RESTRICT blur, const float * horizontalWeights, const float * verticalWeights,
                            const int width, const int height, const int stride, const int bgStride, const int horizontalRadius, const int verticalRadius, const float offset) noexcept {
    const int diameter = verticalRadius * 2 + 1;
    const T ** srcp = new const T *[diameter];

    srcp[verticalRadius] = _srcp;
    for (int i = 1; i <= verticalRadius; i++) {
        srcp[verticalRadius - i] = srcp[verticalRadius - 1 + i];
        srcp[verticalRadius + i] = srcp[verticalRadius] + stride * i;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.f;

            for (int i = 0; i < diameter; i++) {
                if (std::is_integral<T>::value)
                    sum += srcp[i][x] * verticalWeights[i];
                else
                    sum += (srcp[i][x] + offset) * verticalWeights[i];
            }

            buffer[x] = sum;
        }

        gaussianBlurH_c(buffer, blur, horizontalWeights, width, horizontalRadius);

        for (int i = 0; i < diameter - 1; i++)
            srcp[i] = srcp[i + 1];
        if (y < height - 1 - verticalRadius)
            srcp[diameter - 1] += stride;
        else if (y > height - 1 - verticalRadius)
            srcp[diameter - 1] -= stride;
        blur += bgStride;
    }

    delete[] srcp;
}

static void detectEdge_c(float * blur, float * VS_RESTRICT gradient, unsigned * VS_RESTRICT direction, const int width, const int height, const int stride, const int bgStride,
                         const int mode, const unsigned op) noexcept {
    float * VS_RESTRICT srcpp = blur;
    float * VS_RESTRICT srcp = blur;
    float * VS_RESTRICT srcpn = blur + bgStride;

    srcp[-1] = srcp[0];
    srcp[width] = srcp[width - 1];

    for (int y = 0; y < height; y++) {
        srcpn[-1] = srcpn[0];
        srcpn[width] = srcpn[width - 1];

        for (int x = 0; x < width; x++) {
            float gx, gy;

            if (op == 0) {
                gx = srcp[x + 1] - srcp[x - 1];
                gy = srcpp[x] - srcpn[x];
            } else if (op == 1) {
                gx = (srcpp[x + 1] + srcp[x + 1] + srcpn[x + 1] - srcpp[x - 1] - srcp[x - 1] - srcpn[x - 1]) / 2.f;
                gy = (srcpp[x - 1] + srcpp[x] + srcpp[x + 1] - srcpn[x - 1] - srcpn[x] - srcpn[x + 1]) / 2.f;
            } else if (op == 2) {
                gx = srcpp[x + 1] + 2.f * srcp[x + 1] + srcpn[x + 1] - srcpp[x - 1] - 2.f * srcp[x - 1] - srcpn[x - 1];
                gy = srcpp[x - 1] + 2.f * srcpp[x] + srcpp[x + 1] - srcpn[x - 1] - 2.f * srcpn[x] - srcpn[x + 1];
            } else {
                gx = 3.f * srcpp[x + 1] + 10.f * srcp[x + 1] + 3.f * srcpn[x + 1] - 3.f * srcpp[x - 1] - 10.f * srcp[x - 1] - 3.f * srcpn[x - 1];
                gy = 3.f * srcpp[x - 1] + 10.f * srcpp[x] + 3.f * srcpp[x + 1] - 3.f * srcpn[x - 1] - 10.f * srcpn[x] - 3.f * srcpn[x + 1];
            }

            gradient[x] = std::sqrt(gx * gx + gy * gy);

            if (mode == 0) {
                float dr = std::atan2(gy, gx);
                if (dr < 0.f)
                    dr += M_PIF;

                const unsigned bin = static_cast<unsigned>(dr * 4.f * M_1_PIF + 0.5f);
                direction[x] = (bin >= 4) ? 0 : bin;
            }
        }

        srcpp = srcp;
        srcp = srcpn;
        if (y < height - 2)
            srcpn += bgStride;
        gradient += bgStride;
        direction += stride;
    }
}

static void nonMaximumSuppression_c(const unsigned * direction, float * VS_RESTRICT gradient, float * VS_RESTRICT blur, const int width, const int height,
                                    const int stride, const int bgStride) noexcept {
    const int offsets[]{ 1, -bgStride + 1, -bgStride, -bgStride - 1 };

    gradient[-1] = gradient[0];
    gradient[-1 + bgStride * (height - 1)] = gradient[bgStride * (height - 1)];
    gradient[width] = gradient[width - 1];
    gradient[width + bgStride * (height - 1)] = gradient[width - 1 + bgStride * (height - 1)];
    std::copy_n(gradient - 8, width + 16, gradient - 8 - bgStride);
    std::copy_n(gradient - 8 + bgStride * (height - 1), width + 16, gradient - 8 + bgStride * height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int offset = offsets[direction[x]];
            blur[x] = (gradient[x] >= std::max(gradient[x + offset], gradient[x - offset])) ? gradient[x] : fltLowest;
        }

        direction += stride;
        gradient += bgStride;
        blur += bgStride;
    }
}

static void hysteresis(float * VS_RESTRICT blur, bool * VS_RESTRICT label, const int width, const int height, const int bgStride, const float t_h, const float t_l) noexcept {
    std::fill_n(label, width * height, false);

    std::vector<std::pair<int, int>> coordinates;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (!label[width * y + x] && blur[bgStride * y + x] >= t_h) {
                label[width * y + x] = true;
                blur[bgStride * y + x] = fltMax;

                coordinates.emplace_back(std::make_pair(x, y));

                while (!coordinates.empty()) {
                    const auto pos = coordinates.back();
                    coordinates.pop_back();

                    const int xxStart = std::max(pos.first - 1, 0);
                    const int xxStop = std::min(pos.first + 1, width - 1);
                    const int yyStart = std::max(pos.second - 1, 0);
                    const int yyStop = std::min(pos.second + 1, height - 1);

                    for (int yy = yyStart; yy <= yyStop; yy++) {
                        for (int xx = xxStart; xx <= xxStop; xx++) {
                            if (!label[width * yy + xx] && blur[bgStride * yy + xx] >= t_l) {
                                label[width * yy + xx] = true;
                                blur[bgStride * yy + xx] = fltMax;

                                coordinates.emplace_back(std::make_pair(xx, yy));
                            }
                        }
                    }
                }
            }
        }
    }
}

template<typename T>
static void outputGB_c(const float * blur, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const int bgStride,
                       const uint16_t peak, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (std::is_integral<T>::value)
                dstp[x] = std::min<unsigned>(blur[x] + 0.5f, peak);
            else
                dstp[x] = blur[x] - offset;
        }

        blur += bgStride;
        dstp += stride;
    }
}

template<typename T>
static void binarizeCE_c(const float * blur, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const int bgStride,
                         const uint16_t peak, const float lower, const float upper) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (std::is_integral<T>::value)
                dstp[x] = (blur[x] == fltMax) ? peak : 0;
            else
                dstp[x] = (blur[x] == fltMax) ? upper : lower;
        }

        blur += bgStride;
        dstp += stride;
    }
}

template<typename T>
static void discretizeGM_c(const float * gradient, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const int bgStride,
                           const float magnitude, const uint16_t peak, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (std::is_integral<T>::value)
                dstp[x] = std::min<unsigned>(gradient[x] * magnitude + 0.5f, peak);
            else
                dstp[x] = gradient[x] * magnitude - offset;
        }

        gradient += bgStride;
        dstp += stride;
    }
}

template<typename T>
static void process(const VSFrameRef * src, VSFrameRef * dst, const TCannyData * d, const VSAPI * vsapi) noexcept {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int stride = vsapi->getStride(src, plane) / sizeof(T);
            const int bgStride = stride + 16;
            const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
            T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

            const auto threadId = std::this_thread::get_id();
            float * buffer = d->buffer.at(threadId) + d->radiusAlign;
            float * blur = d->blur.at(threadId) + 8;
            float * gradient = d->gradient.at(threadId) + bgStride + 8;
            unsigned * direction = d->direction.at(threadId);
            bool * label = d->label.at(threadId);

            if (d->horizontalRadius[plane])
                gaussianBlurV<T>(srcp, buffer, blur, d->horizontalWeights[plane], d->verticalWeights[plane], width, height, stride, bgStride,
                                 d->horizontalRadius[plane], d->verticalRadius[plane], d->offset[plane]);
            else
                copyPlane<T>(srcp, blur, width, height, stride, bgStride, d->offset[plane]);

            if (d->mode != -1) {
                detectEdge(blur, gradient, direction, width, height, stride, bgStride, d->mode, d->op);

                if (d->mode == 0) {
                    nonMaximumSuppression(direction, gradient, blur, width, height, stride, bgStride);
                    hysteresis(blur, label, width, height, bgStride, d->t_h, d->t_l);
                }
            }

            if (d->mode == -1)
                outputGB<T>(blur, dstp, width, height, stride, bgStride, d->peak, d->offset[plane]);
            else if (d->mode == 0)
                binarizeCE<T>(blur, dstp, width, height, stride, bgStride, d->peak, d->lower[plane], d->upper[plane]);
            else
                discretizeGM<T>(gradient, dstp, width, height, stride, bgStride, d->magnitude, d->peak, d->offset[plane]);
        }
    }
}

static void selectFunctions(const unsigned opt) noexcept {
    copyPlane<uint8_t> = copyPlane_c;
    copyPlane<uint16_t> = copyPlane_c;
    copyPlane<float> = copyPlane_c;

    gaussianBlurV<uint8_t> = gaussianBlurV_c;
    gaussianBlurV<uint16_t> = gaussianBlurV_c;
    gaussianBlurV<float> = gaussianBlurV_c;

    detectEdge = detectEdge_c;

    nonMaximumSuppression = nonMaximumSuppression_c;

    outputGB<uint8_t> = outputGB_c;
    outputGB<uint16_t> = outputGB_c;
    outputGB<float> = outputGB_c;

    binarizeCE<uint8_t> = binarizeCE_c;
    binarizeCE<uint16_t> = binarizeCE_c;
    binarizeCE<float> = binarizeCE_c;

    discretizeGM<uint8_t> = discretizeGM_c;
    discretizeGM<uint16_t> = discretizeGM_c;
    discretizeGM<float> = discretizeGM_c;

#ifdef VS_TARGET_CPU_X86
    const int iset = instrset_detect();
    if ((opt == 0 && iset >= 8) || opt == 4) {
        copyPlane<uint8_t> = copyPlane_avx2;
        copyPlane<uint16_t> = copyPlane_avx2;
        copyPlane<float> = copyPlane_avx2;

        gaussianBlurV<uint8_t> = gaussianBlurV_avx2;
        gaussianBlurV<uint16_t> = gaussianBlurV_avx2;
        gaussianBlurV<float> = gaussianBlurV_avx2;

        detectEdge = detectEdge_avx2;

        nonMaximumSuppression = nonMaximumSuppression_avx2;

        outputGB<uint8_t> = outputGB_avx2;
        outputGB<uint16_t> = outputGB_avx2;
        outputGB<float> = outputGB_avx2;

        binarizeCE<uint8_t> = binarizeCE_avx2;
        binarizeCE<uint16_t> = binarizeCE_avx2;
        binarizeCE<float> = binarizeCE_avx2;

        discretizeGM<uint8_t> = discretizeGM_avx2;
        discretizeGM<uint16_t> = discretizeGM_avx2;
        discretizeGM<float> = discretizeGM_avx2;
    } else if ((opt == 0 && iset == 7) || opt == 3) {
        copyPlane<uint8_t> = copyPlane_avx;
        copyPlane<uint16_t> = copyPlane_avx;
        copyPlane<float> = copyPlane_avx;

        gaussianBlurV<uint8_t> = gaussianBlurV_avx;
        gaussianBlurV<uint16_t> = gaussianBlurV_avx;
        gaussianBlurV<float> = gaussianBlurV_avx;

        detectEdge = detectEdge_avx;

        nonMaximumSuppression = nonMaximumSuppression_avx;

        outputGB<uint8_t> = outputGB_avx;
        outputGB<uint16_t> = outputGB_avx;
        outputGB<float> = outputGB_avx;

        binarizeCE<uint8_t> = binarizeCE_avx;
        binarizeCE<uint16_t> = binarizeCE_avx;
        binarizeCE<float> = binarizeCE_avx;

        discretizeGM<uint8_t> = discretizeGM_avx;
        discretizeGM<uint16_t> = discretizeGM_avx;
        discretizeGM<float> = discretizeGM_avx;
    } else if ((opt == 0 && iset >= 2) || opt == 2) {
        copyPlane<uint8_t> = copyPlane_sse2;
        copyPlane<uint16_t> = copyPlane_sse2;
        copyPlane<float> = copyPlane_sse2;

        gaussianBlurV<uint8_t> = gaussianBlurV_sse2;
        gaussianBlurV<uint16_t> = gaussianBlurV_sse2;
        gaussianBlurV<float> = gaussianBlurV_sse2;

        detectEdge = detectEdge_sse2;

        nonMaximumSuppression = nonMaximumSuppression_sse2;

        outputGB<uint8_t> = outputGB_sse2;
        outputGB<uint16_t> = outputGB_sse2;
        outputGB<float> = outputGB_sse2;

        binarizeCE<uint8_t> = binarizeCE_sse2;
        binarizeCE<uint16_t> = binarizeCE_sse2;
        binarizeCE<float> = binarizeCE_sse2;

        discretizeGM<uint8_t> = discretizeGM_sse2;
        discretizeGM<uint16_t> = discretizeGM_sse2;
        discretizeGM<float> = discretizeGM_sse2;
    }
#endif
}

static void VS_CC tcannyInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    TCannyData * d = static_cast<TCannyData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC tcannyGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    TCannyData * d = static_cast<TCannyData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
#ifdef VS_TARGET_CPU_X86
        no_subnormals();
#endif

        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[]{ d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[]{ 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        try {
            auto threadId = std::this_thread::get_id();

            if (!d->buffer.count(threadId)) {
                float * buffer = vs_aligned_malloc<float>((d->vi->width + d->radiusAlign * 2) * sizeof(float), 32);
                if (!buffer)
                    throw std::string{ "malloc failure (buffer)" };
                d->buffer.emplace(threadId, buffer);
            }

            if (!d->blur.count(threadId)) {
                float * blur = vs_aligned_malloc<float>((vsapi->getStride(src, 0) / d->vi->format->bytesPerSample + 16) * d->vi->height * sizeof(float), 32);
                if (!blur)
                    throw std::string{ "malloc failure (blur)" };
                d->blur.emplace(threadId, blur);
            }

            if (!d->gradient.count(threadId)) {
                if (d->mode != -1) {
                    float * gradient = vs_aligned_malloc<float>((vsapi->getStride(src, 0) / d->vi->format->bytesPerSample + 16) * (d->vi->height + 2) * sizeof(float), 32);
                    if (!gradient)
                        throw std::string{ "malloc failure (gradient)" };
                    d->gradient.emplace(threadId, gradient);
                } else {
                    d->gradient.emplace(threadId, nullptr);
                }
            }

            if (!d->direction.count(threadId)) {
                if (d->mode == 0) {
                    unsigned * direction = vs_aligned_malloc<unsigned>(vsapi->getStride(src, 0) / d->vi->format->bytesPerSample * d->vi->height * sizeof(unsigned), 32);
                    if (!direction)
                        throw std::string{ "malloc failure (direction)" };
                    d->direction.emplace(threadId, direction);
                } else {
                    d->direction.emplace(threadId, nullptr);
                }
            }

            if (!d->label.count(threadId)) {
                if (d->mode == 0) {
                    bool * label = new (std::nothrow) bool[d->vi->width * d->vi->height];
                    if (!label)
                        throw std::string{ "malloc failure (label)" };
                    d->label.emplace(threadId, label);
                } else {
                    d->label.emplace(threadId, nullptr);
                }
            }
        } catch (const std::string & error) {
            vsapi->setFilterError(("TCanny: " + error).c_str(), frameCtx);
            vsapi->freeFrame(src);
            vsapi->freeFrame(dst);
            return nullptr;
        }

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

static void VS_CC tcannyFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TCannyData * d = static_cast<TCannyData *>(instanceData);

    vsapi->freeNode(d->node);

    for (int i = 0; i < 3; i++) {
        delete[] d->horizontalWeights[i];
        delete[] d->verticalWeights[i];
    }

    for (auto & iter : d->buffer)
        vs_aligned_free(iter.second);

    for (auto & iter : d->blur)
        vs_aligned_free(iter.second);

    for (auto & iter : d->gradient)
        vs_aligned_free(iter.second);

    for (auto & iter : d->direction)
        vs_aligned_free(iter.second);

    for (auto & iter : d->label)
        delete[] iter.second;

    delete d;
}

static void VS_CC tcannyCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<TCannyData> d{ new TCannyData{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    try {
        if (!isConstantFormat(d->vi) || (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bits integer and 32 bits float input supported" };

        if (d->vi->height < 2)
            throw std::string{ "the clip's height must be greater than or equal to 2" };

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

        d->t_h = static_cast<float>(vsapi->propGetFloat(in, "t_h", 0, &err));
        if (err)
            d->t_h = 8.f;

        d->t_l = static_cast<float>(vsapi->propGetFloat(in, "t_l", 0, &err));
        if (err)
            d->t_l = 1.f;

        d->mode = int64ToIntS(vsapi->propGetInt(in, "mode", 0, &err));

        d->op = int64ToIntS(vsapi->propGetInt(in, "op", 0, &err));
        if (err)
            d->op = 1;

        float gmmax = static_cast<float>(vsapi->propGetFloat(in, "gmmax", 0, &err));
        if (err)
            gmmax = 50.f;

        const int opt = int64ToIntS(vsapi->propGetInt(in, "opt", 0, &err));

        for (int i = 0; i < 3; i++) {
            if (horizontalSigma[i] < 0.f)
                throw std::string{ "sigma must be greater than or equal to 0.0" };
        }

        if (d->t_l >= d->t_h)
            throw std::string{ "t_h must be greater than t_l" };

        if (d->mode < -1 || d->mode > 1)
            throw std::string{ "mode must be -1, 0 or 1" };

        if (d->op < 0 || d->op > 3)
            throw std::string{ "op must be 0, 1, 2 or 3" };

        if (gmmax < 1.f)
            throw std::string{ "gmmax must be greater than or equal to 1.0" };

        if (opt < 0 || opt > 4)
            throw std::string{ "opt must be 0, 1, 2, 3 or 4" };

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

        const unsigned numThreads = vsapi->getCoreInfo(core)->numThreads;
        d->buffer.reserve(numThreads);
        d->blur.reserve(numThreads);
        d->gradient.reserve(numThreads);
        d->direction.reserve(numThreads);
        d->label.reserve(numThreads);

        selectFunctions(opt);

        if (d->vi->format->sampleType == stInteger) {
            d->peak = (1 << d->vi->format->bitsPerSample) - 1;
            const float scale = d->peak / 255.f;
            d->t_h *= scale;
            d->t_l *= scale;
        } else {
            d->t_h /= 255.f;
            d->t_l /= 255.f;

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

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane] && horizontalSigma[plane]) {
                d->horizontalWeights[plane] = gaussianWeights(horizontalSigma[plane], &d->horizontalRadius[plane]);
                d->verticalWeights[plane] = gaussianWeights(verticalSigma[plane], &d->verticalRadius[plane]);
                if (!d->horizontalWeights[plane] || !d->verticalWeights[plane])
                    throw std::string{ "malloc failure (weights)" };

                const int width = d->vi->width >> (plane ? d->vi->format->subSamplingW : 0);
                const int height = d->vi->height >> (plane ? d->vi->format->subSamplingH : 0);
                const std::string planeOrder{ plane == 0 ? "first" : (plane == 1 ? "second" : "third") };

                if (width < d->horizontalRadius[plane] + 1)
                    throw std::string{ "the " + planeOrder + " plane's width must be greater than or equal to " + std::to_string(d->horizontalRadius[plane] + 1) + " for specified sigma" };

                if (height < d->verticalRadius[plane] + 1)
                    throw std::string{ "the " + planeOrder + " plane's height must be greater than or equal to " + std::to_string(d->verticalRadius[plane] + 1) + " for specified sigma" };
            }
        }

        d->radiusAlign = (std::max({ d->horizontalRadius[0], d->horizontalRadius[1], d->horizontalRadius[2] }) + 7) & -8;

        d->magnitude = 255.f / gmmax;
    } catch (const std::string & error) {
        vsapi->setError(out, ("TCanny: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "TCanny", tcannyInit, tcannyGetFrame, tcannyFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

#ifdef HAVE_OPENCL
extern void VS_CC tcannyCLCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
#endif

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.tcanny", "tcanny", "Build an edge map using canny edge detection", VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("TCanny",
                 "clip:clip;"
                 "sigma:float[]:opt;"
                 "t_h:float:opt;"
                 "t_l:float:opt;"
                 "mode:int:opt;"
                 "op:int:opt;"
                 "gmmax:float:opt;"
                 "opt:int:opt;"
                 "planes:int[]:opt;",
                 tcannyCreate, nullptr, plugin);

#ifdef HAVE_OPENCL
    registerFunc("TCannyCL",
                 "clip:clip;"
                 "sigma:float[]:opt;"
                 "t_h:float:opt;"
                 "t_l:float:opt;"
                 "mode:int:opt;"
                 "op:int:opt;"
                 "gmmax:float:opt;"
                 "device:int:opt;"
                 "list_device:int:opt;"
                 "info:int:opt;"
                 "planes:int[]:opt;",
                 tcannyCLCreate, nullptr, plugin);
#endif
}
