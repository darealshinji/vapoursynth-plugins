#ifdef VS_TARGET_CPU_X86
#ifndef __AVX__
#define __AVX__
#endif

#include <limits>

#include "vectorclass/vectormath_trig.h"

static constexpr float M_PIF = 3.14159265358979323846f;
static constexpr float M_1_PIF = 0.318309886183790671538f;

template<typename T> void copyData_AVX(const T *, float *, const unsigned, const unsigned, const unsigned, const unsigned, const float) noexcept;

template<>
void copyData_AVX(const uint8_t * srcp, float * blur, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride, const float offset) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8)
            to_float(Vec8i().load_8uc(srcp + x)).stream(blur + x);

        srcp += stride;
        blur += blurStride;
    }
}

template<>
void copyData_AVX(const uint16_t * srcp, float * blur, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride, const float offset) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8)
            to_float(Vec8i().load_8us(srcp + x)).stream(blur + x);

        srcp += stride;
        blur += blurStride;
    }
}

template<>
void copyData_AVX(const float * srcp, float * blur, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride, const float offset) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8)
            (Vec8f().load_a(srcp + x) + offset).stream(blur + x);

        srcp += stride;
        blur += blurStride;
    }
}

void gaussianBlurHorizontal_AVX(float * buffer, float * blur, const float * weights, const int width, const int radius) noexcept {
    for (int i = 1; i <= radius; i++) {
        buffer[-i] = buffer[i - 1];
        buffer[width - 1 + i] = buffer[width - i];
    }

    for (int x = 0; x < width; x += 8) {
        Vec8f sum = zero_8f();

        for (int i = -radius; i <= radius; i++) {
            const Vec8f srcp = Vec8f().load(buffer + x + i);
            sum = mul_add(srcp, weights[i], sum);
        }

        sum.stream(blur + x);
    }
}

template<typename T> void gaussianBlurVertical_AVX(const T *, float *, float *, const float *, const float *, const unsigned, const int, const unsigned, const unsigned, const int, const int, const float) noexcept;

template<>
void gaussianBlurVertical_AVX(const uint8_t * __srcp, float * buffer, float * blur, const float * weightsHorizontal, const float * weightsVertical,
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
            Vec8f sum = zero_8f();

            for (unsigned i = 0; i < diameter; i++) {
                const Vec8f srcp = to_float(Vec8i().load_8uc(_srcp[i] + x));
                sum = mul_add(srcp, weightsVertical[i], sum);
            }

            sum.store_a(buffer + x);
        }

        gaussianBlurHorizontal_AVX(buffer, blur, weightsHorizontal + radiusHorizontal, width, radiusHorizontal);

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
void gaussianBlurVertical_AVX(const uint16_t * __srcp, float * buffer, float * blur, const float * weightsHorizontal, const float * weightsVertical,
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
            Vec8f sum = zero_8f();

            for (unsigned i = 0; i < diameter; i++) {
                const Vec8f srcp = to_float(Vec8i().load_8us(_srcp[i] + x));
                sum = mul_add(srcp, weightsVertical[i], sum);
            }

            sum.store_a(buffer + x);
        }

        gaussianBlurHorizontal_AVX(buffer, blur, weightsHorizontal + radiusHorizontal, width, radiusHorizontal);

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
void gaussianBlurVertical_AVX(const float * __srcp, float * buffer, float * blur, const float * weightsHorizontal, const float * weightsVertical,
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
            Vec8f sum = zero_8f();

            for (unsigned i = 0; i < diameter; i++) {
                const Vec8f srcp = Vec8f().load_a(_srcp[i] + x);
                sum = mul_add(srcp + offset, weightsVertical[i], sum);
            }

            sum.store_a(buffer + x);
        }

        gaussianBlurHorizontal_AVX(buffer, blur, weightsHorizontal + radiusHorizontal, width, radiusHorizontal);

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

void detectEdge_AVX(float * blur, float * gradient, float * direction, const int width, const unsigned height,
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

void nonMaximumSuppression_AVX(const float * _gradient, const float * _direction, float * blur, const int width, const unsigned height,
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

template<typename T> void outputGB_AVX(const float *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const uint16_t, const float, const float) noexcept;

template<>
void outputGB_AVX(const float * blur, uint8_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                  const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 16) {
            const Vec8i srcp_8i_0 = truncate_to_int(Vec8f().load_a(blur + x) + 0.5f);
            const Vec8i srcp_8i_1 = truncate_to_int(Vec8f().load_a(blur + x + 8) + 0.5f);
            const Vec8s srcp_8s_0 = compress_saturated(srcp_8i_0.get_low(), srcp_8i_0.get_high());
            const Vec8s srcp_8s_1 = compress_saturated(srcp_8i_1.get_low(), srcp_8i_1.get_high());
            const Vec16uc srcp = compress_saturated_s2u(srcp_8s_0, srcp_8s_1);
            srcp.stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<>
void outputGB_AVX(const float * blur, uint16_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                  const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8i srcp_8i = truncate_to_int(Vec8f().load_a(blur + x) + 0.5f);
            const Vec8us srcp = compress_saturated_s2u(srcp_8i.get_low(), srcp_8i.get_high());
            min(srcp, peak).stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<>
void outputGB_AVX(const float * blur, float * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
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

template<typename T> void binarizeCE_AVX(const float *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const uint16_t, const float, const float) noexcept;

template<>
void binarizeCE_AVX(const float * blur, uint8_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                    const uint16_t peak, const float lower, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 16) {
            const Vec8ib mask_8ib_0 = Vec8ib(Vec8f().load_a(blur + x) == std::numeric_limits<float>::max());
            const Vec8ib mask_8ib_1 = Vec8ib(Vec8f().load_a(blur + x + 8) == std::numeric_limits<float>::max());
            const Vec8sb mask_8sb_0 = Vec8sb(compress_saturated(mask_8ib_0.get_low(), mask_8ib_0.get_high()));
            const Vec8sb mask_8sb_1 = Vec8sb(compress_saturated(mask_8ib_1.get_low(), mask_8ib_1.get_high()));
            const Vec16cb mask = Vec16cb(compress_saturated(mask_8sb_0, mask_8sb_1));
            select(mask, Vec16uc(255), Vec16uc(0)).stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<>
void binarizeCE_AVX(const float * blur, uint16_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
                    const uint16_t peak, const float lower, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8ib mask_8ib = Vec8ib(Vec8f().load_a(blur + x) == std::numeric_limits<float>::max());
            const Vec8sb mask = Vec8sb(compress_saturated(mask_8ib.get_low(), mask_8ib.get_high()));
            select(mask, Vec8us(peak), Vec8us(0)).stream(dstp + x);
        }

        blur += blurStride;
        dstp += stride;
    }
}

template<>
void binarizeCE_AVX(const float * blur, float * dstp, const unsigned width, const unsigned height, const unsigned stride, const unsigned blurStride,
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

template<typename T> void discretizeGM_AVX(const float *, T *, const unsigned, const unsigned, const unsigned, const float, const uint16_t, const float, const float) noexcept;

template<>
void discretizeGM_AVX(const float * gradient, uint8_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const float magnitude,
                      const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 16) {
            const Vec8f srcp_8f_0 = Vec8f().load_a(gradient + x);
            const Vec8f srcp_8f_1 = Vec8f().load_a(gradient + x + 8);
            const Vec8i srcp_8i_0 = truncate_to_int(mul_add(srcp_8f_0, magnitude, 0.5f));
            const Vec8i srcp_8i_1 = truncate_to_int(mul_add(srcp_8f_1, magnitude, 0.5f));
            const Vec8s srcp_8s_0 = compress_saturated(srcp_8i_0.get_low(), srcp_8i_0.get_high());
            const Vec8s srcp_8s_1 = compress_saturated(srcp_8i_1.get_low(), srcp_8i_1.get_high());
            const Vec16uc srcp = compress_saturated_s2u(srcp_8s_0, srcp_8s_1);
            srcp.stream(dstp + x);
        }

        gradient += stride;
        dstp += stride;
    }
}

template<>
void discretizeGM_AVX(const float * gradient, uint16_t * dstp, const unsigned width, const unsigned height, const unsigned stride, const float magnitude,
                      const uint16_t peak, const float offset, const float upper) noexcept {
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8f srcp_8f = Vec8f().load_a(gradient + x);
            const Vec8i srcp_8i = truncate_to_int(mul_add(srcp_8f, magnitude, 0.5f));
            const Vec8us srcp = compress_saturated_s2u(srcp_8i.get_low(), srcp_8i.get_high());
            min(srcp, peak).stream(dstp + x);
        }

        gradient += stride;
        dstp += stride;
    }
}

template<>
void discretizeGM_AVX(const float * gradient, float * dstp, const unsigned width, const unsigned height, const unsigned stride, const float magnitude,
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
