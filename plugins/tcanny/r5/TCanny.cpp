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

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>
#ifdef VS_TARGET_CPU_X86
#include "vectorclass/vectormath_trig.h"
#endif

#define M_PIF 3.14159265358979323846f
#define M_1_PIF 0.318309886183790671538f

struct TCannyData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    float sigma, t_h, t_l, gmmax;
    int nms, mode, op;
    bool process[3];
    int grad, bins;
    float * weights;
    float magnitude;
    int peak;
    float lower[3], upper[3];
    void (*genConvV)(const uint8_t * srcp, float * dstp, const float * weights, const int width, const int height, const int stride, const int rad, const float offset);
};

struct Stack {
    uint8_t * map;
    std::pair<int, int> * pos;
    int index;
};

static inline void push(Stack & s, const int x, const int y) {
    s.pos[++s.index].first = x;
    s.pos[s.index].second = y;
}

static inline std::pair<int, int> pop(Stack & s) {
    return s.pos[s.index--];
}

static inline float scale(const float val, const int bits) {
    return val * ((1 << bits) - 1) / 255.f;
}

static float * gaussianWeights(const float sigma, int & rad) {
    const int dia = std::max(static_cast<int>(sigma * 3.f + 0.5f), 1) * 2 + 1;
    rad = dia / 2;
    float sum = 0.f;

    float * VS_RESTRICT weights = new (std::nothrow) float[dia];
    if (!weights)
        return nullptr;

    for (int k = -rad; k <= rad; k++) {
        const float w = std::exp(-(k * k) / (2.f * sigma * sigma));
        weights[k + rad] = w;
        sum += w;
    }

    for (int k = 0; k < dia; k++)
        weights[k] /= sum;

    return weights;
}

#ifdef VS_TARGET_CPU_X86
static void genConvV_uint8(const uint8_t * __srcp, float * dstp, const float * _weights, const int width, const int height, const int stride, const int rad, const float offset) {
    const int length = rad * 2 + 1;
    const uint8_t ** _srcp = new const uint8_t *[length];
    const Vec8f zero(0.f);

    for (int i = -rad; i <= rad; i++)
        _srcp[i + rad] = __srcp + stride * std::abs(i);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            Vec8f sum = zero;

            for (int i = 0; i < length; i++) {
                const Vec16uc srcp_16uc = Vec16uc().load(_srcp[i] + x);
                const Vec8s srcp_8s = Vec8s(extend_low(srcp_16uc));
                const Vec8i srcp_8i = Vec8i(extend_low(srcp_8s), extend_high(srcp_8s));
                const Vec8f srcp = to_float(srcp_8i);
                const Vec8f weights(_weights[i]);
                sum = mul_add(srcp, weights, sum);
            }

            sum.store_a(dstp + x);
        }

        for (int i = 0; i < length - 1; i++)
            _srcp[i] = _srcp[i + 1];

        _srcp[length - 1] += stride * (y < height - rad - 1 ? 1 : -1);
        dstp += stride;
    }

    delete[] _srcp;
}

static void genConvV_uint16(const uint8_t * __srcp, float * dstp, const float * _weights, const int width, const int height, const int stride, const int rad, const float offset) {
    const int length = rad * 2 + 1;
    const uint16_t ** _srcp = new const uint16_t *[length];
    const Vec8f zero(0.f);

    for (int i = -rad; i <= rad; i++)
        _srcp[i + rad] = reinterpret_cast<const uint16_t *>(__srcp) + stride * std::abs(i);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            Vec8f sum = zero;

            for (int i = 0; i < length; i++) {
                const Vec8us srcp_8us = Vec8us().load_a(_srcp[i] + x);
                const Vec8i srcp_8i = Vec8i(extend_low(srcp_8us), extend_high(srcp_8us));
                const Vec8f srcp = to_float(srcp_8i);
                const Vec8f weights(_weights[i]);
                sum = mul_add(srcp, weights, sum);
            }

            sum.store_a(dstp + x);
        }

        for (int i = 0; i < length - 1; i++)
            _srcp[i] = _srcp[i + 1];

        _srcp[length - 1] += stride * (y < height - rad - 1 ? 1 : -1);
        dstp += stride;
    }

    delete[] _srcp;
}

static void genConvV_float(const uint8_t * __srcp, float * dstp, const float * _weights, const int width, const int height, const int stride, const int rad, const float _offset) {
    const int length = rad * 2 + 1;
    const float ** _srcp = new const float *[length];
    const Vec8f zero(0.f);
    const Vec8f offset(_offset);

    for (int i = -rad; i <= rad; i++)
        _srcp[i + rad] = reinterpret_cast<const float *>(__srcp) + stride * std::abs(i);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            Vec8f sum = zero;

            for (int i = 0; i < length; i++) {
                const Vec8f srcp = Vec8f().load_a(_srcp[i] + x);
                const Vec8f weights(_weights[i]);
                sum = mul_add((srcp + offset), weights, sum);
            }

            sum.store_a(dstp + x);
        }

        for (int i = 0; i < length - 1; i++)
            _srcp[i] = _srcp[i + 1];

        _srcp[length - 1] += stride * (y < height - rad - 1 ? 1 : -1);
        dstp += stride;
    }

    delete[] _srcp;
}

static void genConvH(const float * __srcp, float * dstp, const float * _weights, const int width, const int height, const int stride, const int rad) {
    float * VS_RESTRICT _srcp = new float[stride + rad * 2];
    float * srcpSaved = _srcp;
    _srcp += rad;
    const Vec8f zero(0.f);

    for (int y = 0; y < height; y++) {
        memcpy(_srcp, __srcp, width * sizeof(float));
        for (int i = 1; i <= rad; i++) {
            _srcp[-i] = __srcp[i];
            _srcp[width - 1 + i] = __srcp[width - 1 - i];
        }

        for (int x = 0; x < width; x += 8) {
            Vec8f sum = zero;

            for (int i = -rad; i <= rad; i++) {
                const Vec8f srcp = Vec8f().load(_srcp + x + i);
                const Vec8f weights(_weights[i + rad]);
                sum = mul_add(srcp, weights, sum);
            }

            sum.store_a(dstp + x);
        }

        __srcp += stride;
        dstp += stride;
    }

    delete[] srcpSaved;
}

static void detectEdge(float * _srcp, float * _gimg, float * _dimg, const int width, const int height, const int stride, const int mode, const int op) {
    const int regularPart = (width % 8 ? width : width - 1) & -8;
    const Vec8f zero(0.f);
    const Vec8f pointFive(0.5f);
    const Vec8f two(2.f);
    const Vec8f PI(M_PIF);

    memset(_gimg, 0, stride * height * sizeof(float));
    memset(_dimg, 0, stride * height * sizeof(float));

    float * VS_RESTRICT srcp = _srcp + stride;
    float * VS_RESTRICT gimg = _gimg + stride;
    float * VS_RESTRICT dimg = _dimg + stride;

    for (int y = 1; y < height - 1; y++) {
        int x;

        for (x = 1; x < regularPart; x += 8) {
            Vec8f dx, dy;

            if (op == 0) {
                dx = Vec8f().load(srcp + x + 1) - Vec8f().load_a(srcp + x - 1);
                dy = Vec8f().load(srcp + x - stride) - Vec8f().load(srcp + x + stride);
            } else if (op == 1) {
                dx = (Vec8f().load(srcp + x - stride + 1) + Vec8f().load(srcp + x + 1) + Vec8f().load(srcp + x + stride + 1)
                    - Vec8f().load_a(srcp + x - stride - 1) - Vec8f().load_a(srcp + x - 1) - Vec8f().load_a(srcp + x + stride - 1)) * pointFive;
                dy = (Vec8f().load_a(srcp + x - stride - 1) + Vec8f().load(srcp + x - stride) + Vec8f().load(srcp + x - stride + 1)
                    - Vec8f().load_a(srcp + x + stride - 1) - Vec8f().load(srcp + x + stride) - Vec8f().load(srcp + x + stride + 1)) * pointFive;
            } else {
                dx = Vec8f().load(srcp + x - stride + 1) + mul_add(two, Vec8f().load(srcp + x + 1), Vec8f().load(srcp + x + stride + 1))
                    - Vec8f().load_a(srcp + x - stride - 1) - mul_add(two, Vec8f().load_a(srcp + x - 1), Vec8f().load_a(srcp + x + stride - 1));
                dy = Vec8f().load_a(srcp + x - stride - 1) + mul_add(two, Vec8f().load(srcp + x - stride), Vec8f().load(srcp + x - stride + 1))
                    - Vec8f().load_a(srcp + x + stride - 1) - mul_add(two, Vec8f().load(srcp + x + stride), Vec8f().load(srcp + x + stride + 1));
            }

            sqrt(mul_add(dx, dx, dy * dy)).store(gimg + x);

            if (mode != 1) {
                const Vec8f dr = atan2(dy, dx);
                (dr + select(dr < zero, PI, zero)).store(dimg + x);
            }
        }

        for (; x < width - 1; x++) {
            float dx, dy;

            if (op == 0) {
                dx = srcp[x + 1] - srcp[x - 1];
                dy = srcp[x - stride] - srcp[x + stride];
            } else if (op == 1) {
                dx = (srcp[x - stride + 1] + srcp[x + 1] + srcp[x + stride + 1] - srcp[x - stride - 1] - srcp[x - 1] - srcp[x + stride - 1]) * 0.5f;
                dy = (srcp[x - stride - 1] + srcp[x - stride] + srcp[x - stride + 1] - srcp[x + stride - 1] - srcp[x + stride] - srcp[x + stride + 1]) * 0.5f;
            } else {
                dx = srcp[x - stride + 1] + 2.f * srcp[x + 1] + srcp[x + stride + 1] - srcp[x - stride - 1] - 2.f * srcp[x - 1] - srcp[x + stride - 1];
                dy = srcp[x - stride - 1] + 2.f * srcp[x - stride] + srcp[x - stride + 1] - srcp[x + stride - 1] - 2.f * srcp[x + stride] - srcp[x + stride + 1];
            }

            gimg[x] = std::sqrt(dx * dx + dy * dy);

            if (mode != 1) {
                const float dr = std::atan2(dy, dx);
                dimg[x] = dr + (dr < 0.f ? M_PIF : 0.f);
            }
        }

        srcp += stride;
        gimg += stride;
        dimg += stride;
    }

    memcpy(_srcp, _gimg, stride * height * sizeof(float));
}
#else
static void genConvV_uint8(const uint8_t * _srcp, float * VS_RESTRICT dstp, const float * weights, const int width, const int height, const int stride, const int rad, const float offset) {
    const int length = rad * 2 + 1;
    const uint8_t ** srcp = new const uint8_t *[length];

    for (int i = -rad; i <= rad; i++)
        srcp[i + rad] = _srcp + stride * std::abs(i);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.f;
            for (int i = 0; i < length; i++)
                sum += srcp[i][x] * weights[i];
            dstp[x] = sum;
        }

        for (int i = 0; i < length - 1; i++)
            srcp[i] = srcp[i + 1];

        srcp[length - 1] += stride * (y < height - rad - 1 ? 1 : -1);
        dstp += stride;
    }

    delete[] srcp;
}

static void genConvV_uint16(const uint8_t * _srcp, float * VS_RESTRICT dstp, const float * weights, const int width, const int height, const int stride, const int rad, const float offset) {
    const int length = rad * 2 + 1;
    const uint16_t ** srcp = new const uint16_t *[length];

    for (int i = -rad; i <= rad; i++)
        srcp[i + rad] = reinterpret_cast<const uint16_t *>(_srcp) + stride * std::abs(i);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.f;
            for (int i = 0; i < length; i++)
                sum += srcp[i][x] * weights[i];
            dstp[x] = sum;
        }

        for (int i = 0; i < length - 1; i++)
            srcp[i] = srcp[i + 1];

        srcp[length - 1] += stride * (y < height - rad - 1 ? 1 : -1);
        dstp += stride;
    }

    delete[] srcp;
}

static void genConvV_float(const uint8_t * _srcp, float * VS_RESTRICT dstp, const float * weights, const int width, const int height, const int stride, const int rad, const float offset) {
    const int length = rad * 2 + 1;
    const float ** srcp = new const float *[length];

    for (int i = -rad; i <= rad; i++)
        srcp[i + rad] = reinterpret_cast<const float *>(_srcp) + stride * std::abs(i);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.f;
            for (int i = 0; i < length; i++)
                sum += (srcp[i][x] + offset) * weights[i];
            dstp[x] = sum;
        }

        for (int i = 0; i < length - 1; i++)
            srcp[i] = srcp[i + 1];

        srcp[length - 1] += stride * (y < height - rad - 1 ? 1 : -1);
        dstp += stride;
    }

    delete[] srcp;
}

static void genConvH(const float * _srcp, float * VS_RESTRICT dstp, const float * weights, const int width, const int height, const int stride, const int rad) {
    float * VS_RESTRICT srcp = new float[stride + rad * 2];
    float * srcpSaved = srcp;
    srcp += rad;

    for (int y = 0; y < height; y++) {
        memcpy(srcp, _srcp, width * sizeof(float));
        for (int i = 1; i <= rad; i++) {
            srcp[-i] = _srcp[i];
            srcp[width - 1 + i] = _srcp[width - 1 - i];
        }

        for (int x = 0; x < width; x++) {
            float sum = 0.f;
            for (int i = -rad; i <= rad; i++)
                sum += srcp[x + i] * weights[i + rad];
            dstp[x] = sum;
        }

        _srcp += stride;
        dstp += stride;
    }

    delete[] srcpSaved;
}

static void detectEdge(float * _srcp, float * _gimg, float * _dimg, const int width, const int height, const int stride, const int mode, const int op) {
    memset(_gimg, 0, stride * height * sizeof(float));
    memset(_dimg, 0, stride * height * sizeof(float));

    float * VS_RESTRICT srcp = _srcp + stride;
    float * VS_RESTRICT gimg = _gimg + stride;
    float * VS_RESTRICT dimg = _dimg + stride;

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            float dx, dy;

            if (op == 0) {
                dx = srcp[x + 1] - srcp[x - 1];
                dy = srcp[x - stride] - srcp[x + stride];
            } else if (op == 1) {
                dx = (srcp[x - stride + 1] + srcp[x + 1] + srcp[x + stride + 1] - srcp[x - stride - 1] - srcp[x - 1] - srcp[x + stride - 1]) * 0.5f;
                dy = (srcp[x - stride - 1] + srcp[x - stride] + srcp[x - stride + 1] - srcp[x + stride - 1] - srcp[x + stride] - srcp[x + stride + 1]) * 0.5f;
            } else {
                dx = srcp[x - stride + 1] + 2.f * srcp[x + 1] + srcp[x + stride + 1] - srcp[x - stride - 1] - 2.f * srcp[x - 1] - srcp[x + stride - 1];
                dy = srcp[x - stride - 1] + 2.f * srcp[x - stride] + srcp[x - stride + 1] - srcp[x + stride - 1] - 2.f * srcp[x + stride] - srcp[x + stride + 1];
            }

            gimg[x] = std::sqrt(dx * dx + dy * dy);

            if (mode != 1) {
                const float dr = std::atan2(dy, dx);
                dimg[x] = dr + (dr < 0.f ? M_PIF : 0.f);
            }
        }

        srcp += stride;
        gimg += stride;
        dimg += stride;
    }

    memcpy(_srcp, _gimg, stride * height * sizeof(float));
}
#endif

template<typename T>
static T getBin(const float dir, const int n) {
    const int bin = static_cast<int>(dir * (n * M_1_PIF) + 0.5f);
    return (bin >= n) ? 0 : bin;
}

template<>
float getBin<float>(const float dir, const int n) {
    const float bin = dir * (n * M_1_PIF);
    return (bin > static_cast<float>(n)) ? 0.f : bin;
}

static void gmDirImages(float * VS_RESTRICT srcp, float * VS_RESTRICT gimg, float * VS_RESTRICT dimg, const int width, const int height, const int stride, const int nms) {
    const int offTable[4] = { 1, -stride + 1, -stride, -stride - 1 };
    srcp += stride;
    gimg += stride;
    dimg += stride;

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (nms & 1) {
                const int off = offTable[getBin<int>(dimg[x], 4)];
                if (gimg[x] >= std::max(gimg[x + off], gimg[x - off]))
                    continue;
            }

            if (nms & 2) {
                const int c = static_cast<int>(dimg[x] * (4.f * M_1_PIF));
                float val1, val2;

                if (c == 0 || c >= 4) {
                    const float h = std::tan(dimg[x]);
                    val1 = (1.f - h) * gimg[x + 1] + h * gimg[x - stride + 1];
                    val2 = (1.f - h) * gimg[x - 1] + h * gimg[x + stride - 1];
                } else if (c == 1) {
                    const float w = 1.f / std::tan(dimg[x]);
                    val1 = (1.f - w) * gimg[x - stride] + w * gimg[x - stride + 1];
                    val2 = (1.f - w) * gimg[x + stride] + w * gimg[x + stride - 1];
                } else if (c == 2) {
                    const float w = 1.f / std::tan(M_PIF - dimg[x]);
                    val1 = (1.f - w) * gimg[x - stride] + w * gimg[x - stride - 1];
                    val2 = (1.f - w) * gimg[x + stride] + w * gimg[x + stride + 1];
                } else {
                    const float h = std::tan(M_PIF - dimg[x]);
                    val1 = (1.f - h) * gimg[x - 1] + h * gimg[x - stride - 1];
                    val2 = (1.f - h) * gimg[x + 1] + h * gimg[x + stride + 1];
                }

                if (gimg[x] >= std::max(val1, val2))
                    continue;
            }

            srcp[x] = -FLT_MAX;
        }

        srcp += stride;
        gimg += stride;
        dimg += stride;
    }
}

static void hystersis(float * VS_RESTRICT srcp, Stack & VS_RESTRICT stack, const int width, const int height, const int stride, const float t_h, const float t_l) {
    memset(stack.map, 0, width * height);
    stack.index = -1;

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (srcp[x + stride * y] < t_h || stack.map[x + width * y])
                continue;

            srcp[x + stride * y] = FLT_MAX;
            stack.map[x + width * y] = UINT8_MAX;
            push(stack, x, y);

            while (stack.index > -1) {
                const std::pair<int, int> pos = pop(stack);
                const int xMin = (pos.first > 1) ? pos.first - 1 : 1;
                const int xMax = (pos.first < width - 2) ? pos.first + 1 : pos.first;
                const int yMin = (pos.second > 1) ? pos.second - 1 : 1;
                const int yMax = (pos.second < height - 2) ? pos.second + 1 : pos.second;

                for (int yy = yMin; yy <= yMax; yy++) {
                    for (int xx = xMin; xx <= xMax; xx++) {
                        if (srcp[xx + stride * yy] > t_l && !stack.map[xx + width * yy]) {
                            srcp[xx + stride * yy] = FLT_MAX;
                            stack.map[xx + width * yy] = UINT8_MAX;
                            push(stack, xx, yy);
                        }
                    }
                }
            }
        }
    }
}

template<typename T>
static void outputGB(const float * srcp, T * VS_RESTRICT dstp, const int width, const int height, const int stride,
                     const int peak, const float offset, const float lower, const float upper) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = std::min(std::max(static_cast<int>(srcp[x] + 0.5f), 0), peak);

        srcp += stride;
        dstp += stride;
    }
}

template<>
void outputGB<float>(const float * srcp, float * VS_RESTRICT dstp, const int width, const int height, const int stride,
                     const int peak, const float offset, const float lower, const float upper) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = std::min(std::max(srcp[x] - offset, lower), upper);

        srcp += stride;
        dstp += stride;
    }
}

template<typename T>
static void binarizeCE(const float * srcp, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const float t_h, const T peak, const float lower, const float upper) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = (srcp[x] >= t_h) ? peak : 0;

        srcp += stride;
        dstp += stride;
    }
}

template<>
void binarizeCE<float>(const float * srcp, float * VS_RESTRICT dstp, const int width, const int height, const int stride,
                       const float t_h, const float peak, const float lower, const float upper) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = (srcp[x] >= t_h) ? upper : lower;

        srcp += stride;
        dstp += stride;
    }
}

template<typename T>
static void discretizeGM(const float * gimg, T * VS_RESTRICT dstp, const int width, const int height, const int stride,
                         const float magnitude, const int peak, const float offset, const float upper) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = std::min(static_cast<int>(gimg[x] * magnitude + 0.5f), peak);

        gimg += stride;
        dstp += stride;
    }
}

template<>
void discretizeGM<float>(const float * gimg, float * VS_RESTRICT dstp, const int width, const int height, const int stride,
                         const float magnitude, const int peak, const float offset, const float upper) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = std::min(gimg[x] * magnitude - offset, upper);

        gimg += stride;
        dstp += stride;
    }
}

template<typename T>
static void discretizeDM_T(const float * srcp, const float * dimg, T * VS_RESTRICT dstp, const int width, const int height, const int stride,
                           const float t_h, const int bins, const float offset, const float lower) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = (srcp[x] >= t_h) ? getBin<T>(dimg[x], bins) : 0;

        srcp += stride;
        dimg += stride;
        dstp += stride;
    }
}

template<>
void discretizeDM_T<float>(const float * srcp, const float * dimg, float * VS_RESTRICT dstp, const int width, const int height, const int stride,
                           const float t_h, const int bins, const float offset, const float lower) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = (srcp[x] >= t_h) ? getBin<float>(dimg[x], bins) - offset : lower;

        srcp += stride;
        dimg += stride;
        dstp += stride;
    }
}

template<typename T>
static void discretizeDM(const float * dimg, T * VS_RESTRICT dstp, const int width, const int height, const int stride, const int bins, const float offset) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = getBin<T>(dimg[x], bins);

        dimg += stride;
        dstp += stride;
    }
}

template<>
void discretizeDM<float>(const float * dimg, float * VS_RESTRICT dstp, const int width, const int height, const int stride, const int bins, const float offset) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = getBin<float>(dimg[x], bins) - offset;

        dimg += stride;
        dstp += stride;
    }
}

template<typename T>
static void process(const VSFrameRef * src, VSFrameRef * dst, float * VS_RESTRICT fa[3], Stack & VS_RESTRICT stack, const TCannyData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int stride = vsapi->getStride(src, plane) / sizeof(T);
            const uint8_t * srcp = vsapi->getReadPtr(src, plane);
            T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
            const float offset = (d->vi->format->sampleType == stInteger || plane == 0 || d->vi->format->colorFamily == cmRGB) ? 0.f : 0.5f;

            d->genConvV(srcp, fa[1], d->weights, width, height, stride, d->grad, offset);
            genConvH(fa[1], fa[0], d->weights, width, height, stride, d->grad);

            if (d->mode != -1) {
                detectEdge(fa[0], fa[1], fa[2], width, height, stride, d->mode, d->op);
                if (!((d->mode & 1) || d->nms == 0))
                    gmDirImages(fa[0], fa[1], fa[2], width, height, stride, d->nms);
            }

            if (!(d->mode & 1))
                hystersis(fa[0], stack, width, height, stride, d->t_h, d->t_l);

            if (d->mode == -1)
                outputGB<T>(fa[0], dstp, width, height, stride, d->peak, offset, d->lower[plane], d->upper[plane]);
            else if (d->mode == 0)
                binarizeCE<T>(fa[0], dstp, width, height, stride, d->t_h, d->peak, d->lower[plane], d->upper[plane]);
            else if (d->mode == 1)
                discretizeGM<T>(fa[1], dstp, width, height, stride, d->magnitude, d->peak, offset, d->upper[plane]);
            else if (d->mode == 2)
                discretizeDM_T<T>(fa[0], fa[2], dstp, width, height, stride, d->t_h, d->bins, offset, d->lower[plane]);
            else
                discretizeDM<T>(fa[2], dstp, width, height, stride, d->bins, offset);
        }
    }
}

static void VS_CC tcannyInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    TCannyData * d = static_cast<TCannyData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC tcannyGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const TCannyData * d = static_cast<const TCannyData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
#ifdef VS_TARGET_CPU_X86
        no_subnormals();
#endif

        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        float * fa[3];
        for (int i = 0; i < 3; i++) {
            fa[i] = vs_aligned_malloc<float>(vsapi->getStride(src, 0) / d->vi->format->bytesPerSample * d->vi->height * sizeof(float), 32);
            if (!fa[i]) {
                vsapi->setFilterError("TCanny: malloc failure (fa)", frameCtx);
                vsapi->freeFrame(src);
                vsapi->freeFrame(dst);
                return nullptr;
            }
        }

        Stack stack = {};
        if (!(d->mode & 1)) {
            stack.map = vs_aligned_malloc<uint8_t>(d->vi->width * d->vi->height, 32);
            stack.pos = vs_aligned_malloc<std::pair<int, int>>(d->vi->width * d->vi->height * sizeof(std::pair<int, int>), 32);
            if (!stack.map || !stack.pos) {
                vsapi->setFilterError("TCanny: malloc failure (stack)", frameCtx);
                vsapi->freeFrame(src);
                vsapi->freeFrame(dst);
                return nullptr;
            }
        }

        if (d->vi->format->sampleType == stInteger) {
            if (d->vi->format->bitsPerSample == 8)
                process<uint8_t>(src, dst, fa, stack, d, vsapi);
            else
                process<uint16_t>(src, dst, fa, stack, d, vsapi);
        } else {
            process<float>(src, dst, fa, stack, d, vsapi);
        }

        vsapi->freeFrame(src);
        for (int i = 0; i < 3; i++)
            vs_aligned_free(fa[i]);
        vs_aligned_free(stack.map);
        vs_aligned_free(stack.pos);
        return dst;
    }

    return nullptr;
}

static void VS_CC tcannyFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TCannyData * d = static_cast<TCannyData *>(instanceData);
    vsapi->freeNode(d->node);
    delete[] d->weights;
    delete d;
}

static void VS_CC tcannyCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    TCannyData d;
    int err;

    d.sigma = static_cast<float>(vsapi->propGetFloat(in, "sigma", 0, &err));
    if (err)
        d.sigma = 1.5f;

    d.t_h = static_cast<float>(vsapi->propGetFloat(in, "t_h", 0, &err));
    if (err)
        d.t_h = 8.f;

    d.t_l = static_cast<float>(vsapi->propGetFloat(in, "t_l", 0, &err));
    if (err)
        d.t_l = 1.f;

    d.nms = int64ToIntS(vsapi->propGetInt(in, "nms", 0, &err));
    if (err)
        d.nms = 3;

    d.mode = int64ToIntS(vsapi->propGetInt(in, "mode", 0, &err));

    d.op = int64ToIntS(vsapi->propGetInt(in, "op", 0, &err));
    if (err)
        d.op = 1;

    d.gmmax = static_cast<float>(vsapi->propGetFloat(in, "gmmax", 0, &err));
    if (err)
        d.gmmax = 50.f;

    if (d.sigma <= 0.f) {
        vsapi->setError(out, "TCanny: sigma must be greater than 0.0");
        return;
    }

    if (d.nms < 0 || d.nms > 3) {
        vsapi->setError(out, "TCanny: nms must be 0, 1, 2 or 3");
        return;
    }

    if (d.mode < -1 || d.mode > 3) {
        vsapi->setError(out, "TCanny: mode must be -1, 0, 1, 2 or 3");
        return;
    }

    if (d.op < 0 || d.op > 2) {
        vsapi->setError(out, "TCanny: op must be 0, 1 or 2");
        return;
    }

    if (d.gmmax < 1.f) {
        vsapi->setError(out, "TCanny: gmmax must be greater than or equal to 1.0");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || (d.vi->format->sampleType == stInteger && d.vi->format->bitsPerSample > 16) ||
        (d.vi->format->sampleType == stFloat && d.vi->format->bitsPerSample != 32)) {
        vsapi->setError(out, "TCanny: only constant format 8-16 bits integer and 32 bits float input supported");
        vsapi->freeNode(d.node);
        return;
    }

    const int m = vsapi->propNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        d.process[i] = m <= 0;

    for (int i = 0; i < m; i++) {
        const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (n < 0 || n >= d.vi->format->numPlanes) {
            vsapi->setError(out, "TCanny: plane index out of range");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.process[n]) {
            vsapi->setError(out, "TCanny: plane specified twice");
            vsapi->freeNode(d.node);
            return;
        }

        d.process[n] = true;
    }

    if (d.vi->format->sampleType == stInteger) {
        d.t_h = scale(d.t_h, d.vi->format->bitsPerSample);
        d.t_l = scale(d.t_l, d.vi->format->bitsPerSample);
        d.bins = 1 << d.vi->format->bitsPerSample;
        d.peak = d.bins - 1;

        if (d.vi->format->bitsPerSample == 8)
            d.genConvV = genConvV_uint8;
        else
            d.genConvV = genConvV_uint16;
    } else {
        d.t_h /= 255.f;
        d.t_l /= 255.f;
        d.bins = 1;

        for (int plane = 0; plane < d.vi->format->numPlanes; plane++) {
            if (d.process[plane]) {
                if (plane == 0 || d.vi->format->colorFamily == cmRGB) {
                    d.lower[plane] = 0.f;
                    d.upper[plane] = 1.f;
                } else {
                    d.lower[plane] = -0.5f;
                    d.upper[plane] = 0.5f;
                }
            }
        }

        d.genConvV = genConvV_float;
    }

    d.weights = gaussianWeights(d.sigma, d.grad);
    if (!d.weights) {
        vsapi->setError(out, "TCanny: malloc failure (weights)");
        vsapi->freeNode(d.node);
        return;
    }

    d.magnitude = 255.f / d.gmmax;

    TCannyData * data = new TCannyData(d);

    vsapi->createFilter(in, out, "TCanny", tcannyInit, tcannyGetFrame, tcannyFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.tcanny", "tcanny", "Build an edge map using canny edge detection", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("TCanny",
                 "clip:clip;"
                 "sigma:float:opt;"
                 "t_h:float:opt;"
                 "t_l:float:opt;"
                 "nms:int:opt;"
                 "mode:int:opt;"
                 "op:int:opt;"
                 "gmmax:float:opt;"
                 "planes:int[]:opt;",
                 tcannyCreate, nullptr, plugin);
}
