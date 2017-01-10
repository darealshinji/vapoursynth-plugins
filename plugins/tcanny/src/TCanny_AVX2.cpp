#ifdef VS_TARGET_CPU_X86
#ifndef __AVX2__
#define __AVX2__
#endif

#include <limits>

#include "vectorclass/vectormath_trig.h"

static constexpr float M_PIF = 3.14159265358979323846f;
static constexpr float M_1_PIF = 0.318309886183790671538f;

void gaussianBlurHorizontal_AVX2(float * buffer, float * blur, const float * weights, const int width, const int radius) noexcept {
    for (int i = 1; i <= radius; i++) {
        buffer[-i] = buffer[i - 1];
        buffer[width - 1 + i] = buffer[width - i];
    }

    for (int x = 0; x < width; x += 8) {
        Vec8f sum = setzero_8f();

        for (int i = -radius; i <= radius; i++) {
            const Vec8f srcp = Vec8f().load(buffer + x + i);
            sum = mul_add(srcp, weights[i], sum);
        }

        sum.stream(blur + x);
    }
}

template<typename T> void gaussianBlurVertical_AVX2(const T *, float *, float *, const float *, const float *, const unsigned, const int, const unsigned, const unsigned, const int, const int, const float) noexcept;

template<>
void gaussianBlurVertical_AVX2(const uint8_t * __srcp, float * buffer, float * blur, const float * weightsHorizontal, const float * weightsVertical,
                               const unsigned width, const int height, const unsigned stride, const unsigned blurStride,
                               const int radiusHorizontal, const int radiusVertical, const float offset) noexcept {
    const unsigned diameter = radiusVertical * 2 + 1;
    const uint8_t ** _srcp = new const uint8_t *[diameter];

    _srcp[radiusVertical] = __srcp;
    for (int i = 1; i <= radiusVertical; i++) {
        _srcp[radiusVertical - i] = _srcp[radiusVertical + i - 1];
        _srcp[radiusVertical + i] = _srcp[radiusVertical] + stride * i;
    }

    for (int y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            Vec8f sum = setzero_8f();

            for (unsigned i = 0; i < diameter; i++) {
                const Vec8i srcp_8i { _mm256_cvtepu8_epi32(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(_srcp[i] + x))) };
                const Vec8f srcp = to_float(srcp_8i);
                sum = mul_add(srcp, weightsVertical[i], sum);
            }

            sum.store_a(buffer + x);
        }

        gaussianBlurHorizontal_AVX2(buffer, blur, weightsHorizontal + radiusHorizontal, width, radiusHorizontal);

        for (unsigned i = 0; i < diameter - 1; i++)
            _srcp[i] = _srcp[i + 1];
        if (y < height - 1 - radiusVertical)
            _srcp[diameter - 1] += stride;
        else if (y > height - 1 - radiusVertical)
            _srcp[diameter - 1] -= stride;
        blur += blurStride;
    }

    delete[] _srcp;
}

template<>
void gaussianBlurVertical_AVX2(const uint16_t * __srcp, float * buffer, float * blur, const float * weightsHorizontal, const float * weightsVertical,
                               const unsigned width, const int height, const unsigned stride, const unsigned blurStride,
                               const int radiusHorizontal, const int radiusVertical, const float offset) noexcept {
    const unsigned diameter = radiusVertical * 2 + 1;
    const uint16_t ** _srcp = new const uint16_t *[diameter];

    _srcp[radiusVertical] = __srcp;
    for (int i = 1; i <= radiusVertical; i++) {
        _srcp[radiusVertical - i] = _srcp[radiusVertical + i - 1];
        _srcp[radiusVertical + i] = _srcp[radiusVertical] + stride * i;
    }

    for (int y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            Vec8f sum = setzero_8f();

            for (unsigned i = 0; i < diameter; i++) {
                const Vec8us srcp_8us = Vec8us().load_a(_srcp[i] + x);
                const Vec8i srcp_8i { _mm256_cvtepu16_epi32(srcp_8us) };
                const Vec8f srcp = to_float(srcp_8i);
                sum = mul_add(srcp, weightsVertical[i], sum);
            }

            sum.store_a(buffer + x);
        }

        gaussianBlurHorizontal_AVX2(buffer, blur, weightsHorizontal + radiusHorizontal, width, radiusHorizontal);

        for (unsigned i = 0; i < diameter - 1; i++)
            _srcp[i] = _srcp[i + 1];
        if (y < height - 1 - radiusVertical)
            _srcp[diameter - 1] += stride;
        else if (y > height - 1 - radiusVertical)
            _srcp[diameter - 1] -= stride;
        blur += blurStride;
    }

    delete[] _srcp;
}

template<>
void gaussianBlurVertical_AVX2(const float * __srcp, float * buffer, float * blur, const float * weightsHorizontal, const float * weightsVertical,
                               const unsigned width, const int height, const unsigned stride, const unsigned blurStride,
                               const int radiusHorizontal, const int radiusVertical, const float offset) noexcept {
    const unsigned diameter = radiusVertical * 2 + 1;
    const float ** _srcp = new const float *[diameter];

    _srcp[radiusVertical] = __srcp;
    for (int i = 1; i <= radiusVertical; i++) {
        _srcp[radiusVertical - i] = _srcp[radiusVertical + i - 1];
        _srcp[radiusVertical + i] = _srcp[radiusVertical] + stride * i;
    }

    for (int y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            Vec8f sum = setzero_8f();

            for (unsigned i = 0; i < diameter; i++) {
                const Vec8f srcp = Vec8f().load_a(_srcp[i] + x);
                sum = mul_add(srcp + offset, weightsVertical[i], sum);
            }

            sum.store_a(buffer + x);
        }

        gaussianBlurHorizontal_AVX2(buffer, blur, weightsHorizontal + radiusHorizontal, width, radiusHorizontal);

        for (unsigned i = 0; i < diameter - 1; i++)
            _srcp[i] = _srcp[i + 1];
        if (y < height - 1 - radiusVertical)
            _srcp[diameter - 1] += stride;
        else if (y > height - 1 - radiusVertical)
            _srcp[diameter - 1] -= stride;
        blur += blurStride;
    }

    delete[] _srcp;
}

void detectEdge_AVX2(float * blur, float * gradient, float * direction, const int width, const unsigned height,
                     const unsigned stride, const unsigned blurStride, const int mode, const unsigned op) noexcept {
    float * srcpp = blur;
    float * srcp = blur;
    float * srcpn = blur + blurStride;

    srcp[-1] = srcp[0];
    srcp[width] = srcp[width - 1];

    for (unsigned y = 0; y < height; y++) {
        srcpn[-1] = srcpn[0];
        srcpn[width] = srcpn[width - 1];

        for (int x = 0; x < width; x += 8) {
            Vec8f gx, gy;

            if (op == 0) {
                gx = Vec8f().load(srcp + x + 1) - Vec8f().load(srcp + x - 1);
                gy = Vec8f().load_a(srcpp + x) - Vec8f().load_a(srcpn + x);
            } else if (op == 1) {
                gx = (Vec8f().load(srcpp + x + 1) + Vec8f().load(srcp + x + 1) + Vec8f().load(srcpn + x + 1)
                    - Vec8f().load(srcpp + x - 1) - Vec8f().load(srcp + x - 1) - Vec8f().load(srcpn + x - 1)) * 0.5f;
                gy = (Vec8f().load(srcpp + x - 1) + Vec8f().load_a(srcpp + x) + Vec8f().load(srcpp + x + 1)
                    - Vec8f().load(srcpn + x - 1) - Vec8f().load_a(srcpn + x) - Vec8f().load(srcpn + x + 1)) * 0.5f;
            } else if (op == 2) {
                gx = Vec8f().load(srcpp + x + 1) + mul_add(2.f, Vec8f().load(srcp + x + 1), Vec8f().load(srcpn + x + 1))
                    - Vec8f().load(srcpp + x - 1) - mul_add(2.f, Vec8f().load(srcp + x - 1), Vec8f().load(srcpn + x - 1));
                gy = Vec8f().load(srcpp + x - 1) + mul_add(2.f, Vec8f().load_a(srcpp + x), Vec8f().load(srcpp + x + 1))
                    - Vec8f().load(srcpn + x - 1) - mul_add(2.f, Vec8f().load_a(srcpn + x), Vec8f().load(srcpn + x + 1));
            } else {
                gx = mul_add(3.f, Vec8f().load(srcpp + x + 1), mul_add(10.f, Vec8f().load(srcp + x + 1), 3.f * Vec8f().load(srcpn + x + 1)))
                    - mul_add(3.f, Vec8f().load(srcpp + x - 1), mul_add(10.f, Vec8f().load(srcp + x - 1), 3.f * Vec8f().load(srcpn + x - 1)));
                gy = mul_add(3.f, Vec8f().load(srcpp + x - 1), mul_add(10.f, Vec8f().load_a(srcpp + x), 3.f * Vec8f().load(srcpp + x + 1)))
                    - mul_add(3.f, Vec8f().load(srcpn + x - 1), mul_add(10.f, Vec8f().load_a(srcpn + x), 3.f * Vec8f().load(srcpn + x + 1)));
            }

            sqrt(mul_add(gx, gx, gy * gy)).stream(gradient + x);

            if (mode != 1) {
                const Vec8f dr = atan2(gy, gx);
                if_add(dr < 0.f, dr, M_PIF).stream(direction + x);
            }
        }

        srcpp = srcp;
        srcp = srcpn;
        if (y < height - 2)
            srcpn += blurStride;
        gradient += stride;
        direction += stride;
    }
}

void nonMaximumSuppression_AVX2(const float * _gradient, const float * _direction, float * blur, const int width, const unsigned height,
                                const int stride, const unsigned blurStride) noexcept {
    for (int x = 0; x < width; x += 8)
        Vec8f(std::numeric_limits<float>::lowest()).stream(blur + x);

    for (unsigned y = 1; y < height - 1; y++) {
        _gradient += stride;
        _direction += stride;
        blur += blurStride;

        for (int x = 1; x < width - 1; x += 8) {
            const Vec8f direction = Vec8f().load(_direction + x);
            const Vec8i bin = truncate_to_int(mul_add(direction, 4.f * M_1_PIF, 0.5f));

            Vec8fb mask = Vec8fb((bin == 0) | (bin >= 4));
            Vec8f gradient = max(Vec8f().load(_gradient + x + 1), Vec8f().load_a(_gradient + x - 1));
            Vec8f result = gradient & mask;

            mask = Vec8fb(bin == 1);
            gradient = max(Vec8f().load(_gradient + x - stride + 1), Vec8f().load_a(_gradient + x + stride - 1));
            result |= gradient & mask;

            mask = Vec8fb(bin == 2);
            gradient = max(Vec8f().load(_gradient + x - stride), Vec8f().load(_gradient + x + stride));
            result |= gradient & mask;

            mask = Vec8fb(bin == 3);
            gradient = max(Vec8f().load_a(_gradient + x - stride - 1), Vec8f().load(_gradient + x + stride + 1));
            result |= gradient & mask;

            gradient = Vec8f().load(_gradient + x);
            select(gradient >= result, gradient, std::numeric_limits<float>::lowest()).store(blur + x);
        }

        blur[0] = blur[width - 1] = std::numeric_limits<float>::lowest();
    }

    blur += blurStride;

    for (int x = 0; x < width; x += 8)
        Vec8f(std::numeric_limits<float>::lowest()).stream(blur + x);
}

template<typename T> void outputGB_AVX2(const float *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const uint16_t, const float, const float) noexcept;

template<>
void outputGB_AVX2(const float * blur, uint8_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                   const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 32) {
            const Vec8i srcp_8i_0 = truncate_to_int(Vec8f().load_a(blur + x) + 0.5f);
            const Vec8i srcp_8i_1 = truncate_to_int(Vec8f().load_a(blur + x + 8) + 0.5f);
            const Vec8i srcp_8i_2 = truncate_to_int(Vec8f().load_a(blur + x + 16) + 0.5f);
            const Vec8i srcp_8i_3 = truncate_to_int(Vec8f().load_a(blur + x + 24) + 0.5f);
            const Vec16s srcp_16s_0 = compress_saturated(srcp_8i_0, srcp_8i_1);
            const Vec16s srcp_16s_1 = compress_saturated(srcp_8i_2, srcp_8i_3);
            const Vec32uc srcp = compress_saturated_s2u(srcp_16s_0, srcp_16s_1);
            srcp.stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<>
void outputGB_AVX2(const float * blur, uint16_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                   const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 16) {
            const Vec8i srcp_8i_0 = truncate_to_int(Vec8f().load_a(blur + x) + 0.5f);
            const Vec8i srcp_8i_1 = truncate_to_int(Vec8f().load_a(blur + x + 8) + 0.5f);
            const Vec16us srcp = compress_saturated_s2u(srcp_8i_0, srcp_8i_1);
            min(srcp, peak).stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<>
void outputGB_AVX2(const float * blur, float * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                   const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8f srcp = Vec8f().load_a(blur + x);
            min(srcp - offset, upper).stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<typename T> void binarizeCE_AVX2(const float *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const uint16_t, const float, const float) noexcept;

template<>
void binarizeCE_AVX2(const float * blur, uint8_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                     const uint16_t peak, const float lower, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 32) {
            const Vec8ib mask_8ib_0 = Vec8ib(Vec8f().load_a(blur + x) == std::numeric_limits<float>::max());
            const Vec8ib mask_8ib_1 = Vec8ib(Vec8f().load_a(blur + x + 8) == std::numeric_limits<float>::max());
            const Vec8ib mask_8ib_2 = Vec8ib(Vec8f().load_a(blur + x + 16) == std::numeric_limits<float>::max());
            const Vec8ib mask_8ib_3 = Vec8ib(Vec8f().load_a(blur + x + 24) == std::numeric_limits<float>::max());
            const Vec16sb mask_16sb_0 = Vec16sb(compress_saturated(mask_8ib_0, mask_8ib_1));
            const Vec16sb mask_16sb_1 = Vec16sb(compress_saturated(mask_8ib_2, mask_8ib_3));
            const Vec32cb mask = Vec32cb(compress_saturated(mask_16sb_0, mask_16sb_1));
            select(mask, Vec32uc(255), Vec32uc(0)).stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<>
void binarizeCE_AVX2(const float * blur, uint16_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                     const uint16_t peak, const float lower, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 16) {
            const Vec8ib mask_8ib_0 = Vec8ib(Vec8f().load_a(blur + x) == std::numeric_limits<float>::max());
            const Vec8ib mask_8ib_1 = Vec8ib(Vec8f().load_a(blur + x + 8) == std::numeric_limits<float>::max());
            const Vec16sb mask = Vec16sb(compress_saturated(mask_8ib_0, mask_8ib_1));
            select(mask, Vec16us(peak), Vec16us(0)).stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<>
void binarizeCE_AVX2(const float * blur, float * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                     const uint16_t peak, const float lower, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8fb mask = Vec8f().load_a(blur + x) == std::numeric_limits<float>::max();
            select(mask, Vec8f(upper), Vec8f(lower)).stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<typename T> void discretizeGM_AVX2(const float *, T *, const unsigned, const unsigned, const unsigned, const float, const uint16_t, const float, const float) noexcept;

template<>
void discretizeGM_AVX2(const float * gradient, uint8_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const float magnitude,
                       const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 32) {
            const Vec8f srcp_8f_0 = Vec8f().load_a(gradient + x);
            const Vec8f srcp_8f_1 = Vec8f().load_a(gradient + x + 8);
            const Vec8f srcp_8f_2 = Vec8f().load_a(gradient + x + 16);
            const Vec8f srcp_8f_3 = Vec8f().load_a(gradient + x + 24);
            const Vec8i srcp_8i_0 = truncate_to_int(mul_add(srcp_8f_0, magnitude, 0.5f));
            const Vec8i srcp_8i_1 = truncate_to_int(mul_add(srcp_8f_1, magnitude, 0.5f));
            const Vec8i srcp_8i_2 = truncate_to_int(mul_add(srcp_8f_2, magnitude, 0.5f));
            const Vec8i srcp_8i_3 = truncate_to_int(mul_add(srcp_8f_3, magnitude, 0.5f));
            const Vec16s srcp_16s_0 = compress_saturated(srcp_8i_0, srcp_8i_1);
            const Vec16s srcp_16s_1 = compress_saturated(srcp_8i_2, srcp_8i_3);
            const Vec32uc srcp = compress_saturated_s2u(srcp_16s_0, srcp_16s_1);
            srcp.stream(dstp + x);
        }

        gradient += stride;
        dstp += stride;
    }
}

template<>
void discretizeGM_AVX2(const float * gradient, uint16_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const float magnitude,
                       const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 16) {
            const Vec8f srcp_8f_0 = Vec8f().load_a(gradient + x);
            const Vec8f srcp_8f_1 = Vec8f().load_a(gradient + x + 8);
            const Vec8i srcp_8i_0 = truncate_to_int(mul_add(srcp_8f_0, magnitude, 0.5f));
            const Vec8i srcp_8i_1 = truncate_to_int(mul_add(srcp_8f_1, magnitude, 0.5f));
            const Vec16us srcp = compress_saturated_s2u(srcp_8i_0, srcp_8i_1);
            min(srcp, peak).stream(dstp + x);
        }

        gradient += stride;
        dstp += stride;
    }
}

template<>
void discretizeGM_AVX2(const float * gradient, float * dstp, const unsigned width, const unsigned height, const unsigned stride, const float magnitude,
                       const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8f srcp = Vec8f().load_a(gradient + x);
            min(mul_sub(srcp, magnitude, offset), upper).stream(dstp + x);
        }

        gradient += stride;
        dstp += stride;
    }
}
#endif
