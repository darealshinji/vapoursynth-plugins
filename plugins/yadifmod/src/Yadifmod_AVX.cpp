#ifdef VS_TARGET_CPU_X86
#ifndef __AVX__
#define __AVX__
#endif

#include "vectorclass/vectorclass.h"

template<typename T> void filter_avx(const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const unsigned) noexcept;

template<>
void filter_avx(const uint8_t * _prev2pp, const uint8_t * _prev2pn, const uint8_t * _prevp2p, const uint8_t * _prevp, const uint8_t * _prevp2n,
                const uint8_t * _srcpp, const uint8_t * _srcpn,
                const uint8_t * _nextp2p, const uint8_t * _nextp, const uint8_t * _nextp2n, const uint8_t * _next2pp, const uint8_t * _next2pn,
                const uint8_t * _edeintp, uint8_t * dstp,
                const unsigned width, const unsigned yStart, const unsigned yStop, const unsigned stride, const unsigned mode) noexcept {
    for (unsigned y = yStart; y <= yStop; y += 2) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8s prev2pp = Vec8s().load_8uc(_prev2pp + x);
            const Vec8s prev2pn = Vec8s().load_8uc(_prev2pn + x);
            const Vec8s prevp = Vec8s().load_8uc(_prevp + x);
            const Vec8s srcpp = Vec8s().load_8uc(_srcpp + x);
            const Vec8s srcpn = Vec8s().load_8uc(_srcpn + x);
            const Vec8s nextp = Vec8s().load_8uc(_nextp + x);
            const Vec8s next2pp = Vec8s().load_8uc(_next2pp + x);
            const Vec8s next2pn = Vec8s().load_8uc(_next2pn + x);
            const Vec8s edeintp = Vec8s().load_8uc(_edeintp + x);

            Vec8s p1 = srcpp;
            const Vec8s p2 = (prevp + nextp) >> 1;
            Vec8s p3 = srcpn;
            const Vec8s tdiff0 = abs(prevp - nextp) >> 1;
            const Vec8s tdiff1 = (abs(prev2pp - p1) + abs(prev2pn - p3)) >> 1;
            const Vec8s tdiff2 = (abs(next2pp - p1) + abs(next2pn - p3)) >> 1;
            Vec8s diff = max(max(tdiff0, tdiff1), tdiff2);

            if (mode < 2) {
                const Vec8s prevp2p = Vec8s().load_8uc(_prevp2p + x);
                const Vec8s prevp2n = Vec8s().load_8uc(_prevp2n + x);
                const Vec8s nextp2p = Vec8s().load_8uc(_nextp2p + x);
                const Vec8s nextp2n = Vec8s().load_8uc(_nextp2n + x);

                const Vec8s p0 = ((prevp2p + nextp2p) >> 1) - p1;
                const Vec8s p4 = ((prevp2n + nextp2n) >> 1) - p3;
                p1 = p2 - p1;
                p3 = p2 - p3;
                const Vec8s maxs = max(max(p3, p1), min(p0, p4));
                const Vec8s mins = min(min(p3, p1), max(p0, p4));
                diff = max(max(diff, mins), -maxs);
            }

            const Vec8s spatialPred = min(max(edeintp, p2 - diff), p2 + diff);
            compress_saturated_s2u(spatialPred, spatialPred).storel(dstp + x);
        }

        _prev2pp += stride;
        _prev2pn += stride;
        _prevp2p += stride;
        _prevp += stride;
        _prevp2n += stride;
        _srcpp += stride;
        _srcpn += stride;
        _nextp2p += stride;
        _nextp += stride;
        _nextp2n += stride;
        _next2pp += stride;
        _next2pn += stride;
        _edeintp += stride;
        dstp += stride;
    }
}

template<>
void filter_avx(const uint16_t * _prev2pp, const uint16_t * _prev2pn, const uint16_t * _prevp2p, const uint16_t * _prevp, const uint16_t * _prevp2n,
                const uint16_t * _srcpp, const uint16_t * _srcpn,
                const uint16_t * _nextp2p, const uint16_t * _nextp, const uint16_t * _nextp2n, const uint16_t * _next2pp, const uint16_t * _next2pn,
                const uint16_t * _edeintp, uint16_t * dstp,
                const unsigned width, const unsigned yStart, const unsigned yStop, const unsigned stride, const unsigned mode) noexcept {
    for (unsigned y = yStart; y <= yStop; y += 2) {
        for (unsigned x = 0; x < width; x += 4) {
            const Vec4i prev2pp = Vec4i().load_4us(_prev2pp + x);
            const Vec4i prev2pn = Vec4i().load_4us(_prev2pn + x);
            const Vec4i prevp = Vec4i().load_4us(_prevp + x);
            const Vec4i srcpp = Vec4i().load_4us(_srcpp + x);
            const Vec4i srcpn = Vec4i().load_4us(_srcpn + x);
            const Vec4i nextp = Vec4i().load_4us(_nextp + x);
            const Vec4i next2pp = Vec4i().load_4us(_next2pp + x);
            const Vec4i next2pn = Vec4i().load_4us(_next2pn + x);
            const Vec4i edeintp = Vec4i().load_4us(_edeintp + x);

            Vec4i p1 = srcpp;
            const Vec4i p2 = (prevp + nextp) >> 1;
            Vec4i p3 = srcpn;
            const Vec4i tdiff0 = abs(prevp - nextp) >> 1;
            const Vec4i tdiff1 = (abs(prev2pp - p1) + abs(prev2pn - p3)) >> 1;
            const Vec4i tdiff2 = (abs(next2pp - p1) + abs(next2pn - p3)) >> 1;
            Vec4i diff = max(max(tdiff0, tdiff1), tdiff2);

            if (mode < 2) {
                const Vec4i prevp2p = Vec4i().load_4us(_prevp2p + x);
                const Vec4i prevp2n = Vec4i().load_4us(_prevp2n + x);
                const Vec4i nextp2p = Vec4i().load_4us(_nextp2p + x);
                const Vec4i nextp2n = Vec4i().load_4us(_nextp2n + x);

                const Vec4i p0 = ((prevp2p + nextp2p) >> 1) - p1;
                const Vec4i p4 = ((prevp2n + nextp2n) >> 1) - p3;
                p1 = p2 - p1;
                p3 = p2 - p3;
                const Vec4i maxs = max(max(p3, p1), min(p0, p4));
                const Vec4i mins = min(min(p3, p1), max(p0, p4));
                diff = max(max(diff, mins), -maxs);
            }

            const Vec4i spatialPred = min(max(edeintp, p2 - diff), p2 + diff);
            compress_saturated_s2u(spatialPred, spatialPred).storel(dstp + x);
        }

        _prev2pp += stride;
        _prev2pn += stride;
        _prevp2p += stride;
        _prevp += stride;
        _prevp2n += stride;
        _srcpp += stride;
        _srcpn += stride;
        _nextp2p += stride;
        _nextp += stride;
        _nextp2n += stride;
        _next2pp += stride;
        _next2pn += stride;
        _edeintp += stride;
        dstp += stride;
    }
}

template<>
void filter_avx(const float * _prev2pp, const float * _prev2pn, const float * _prevp2p, const float * _prevp, const float * _prevp2n, const float * _srcpp, const float * _srcpn,
                const float * _nextp2p, const float * _nextp, const float * _nextp2n, const float * _next2pp, const float * _next2pn, const float * _edeintp, float * dstp,
                const unsigned width, const unsigned yStart, const unsigned yStop, const unsigned stride, const unsigned mode) noexcept {
    for (unsigned y = yStart; y <= yStop; y += 2) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8f prev2pp = Vec8f().load_a(_prev2pp + x);
            const Vec8f prev2pn = Vec8f().load_a(_prev2pn + x);
            const Vec8f prevp = Vec8f().load_a(_prevp + x);
            const Vec8f srcpp = Vec8f().load_a(_srcpp + x);
            const Vec8f srcpn = Vec8f().load_a(_srcpn + x);
            const Vec8f nextp = Vec8f().load_a(_nextp + x);
            const Vec8f next2pp = Vec8f().load_a(_next2pp + x);
            const Vec8f next2pn = Vec8f().load_a(_next2pn + x);
            const Vec8f edeintp = Vec8f().load_a(_edeintp + x);

            Vec8f p1 = srcpp;
            const Vec8f p2 = (prevp + nextp) * 0.5f;
            Vec8f p3 = srcpn;
            const Vec8f tdiff0 = abs(prevp - nextp) * 0.5f;
            const Vec8f tdiff1 = (abs(prev2pp - p1) + abs(prev2pn - p3)) * 0.5f;
            const Vec8f tdiff2 = (abs(next2pp - p1) + abs(next2pn - p3)) * 0.5f;
            Vec8f diff = max(max(tdiff0, tdiff1), tdiff2);

            if (mode < 2) {
                const Vec8f prevp2p = Vec8f().load_a(_prevp2p + x);
                const Vec8f prevp2n = Vec8f().load_a(_prevp2n + x);
                const Vec8f nextp2p = Vec8f().load_a(_nextp2p + x);
                const Vec8f nextp2n = Vec8f().load_a(_nextp2n + x);

                const Vec8f p0 = mul_sub(prevp2p + nextp2p, 0.5f, p1);
                const Vec8f p4 = mul_sub(prevp2n + nextp2n, 0.5f, p3);
                p1 = p2 - p1;
                p3 = p2 - p3;
                const Vec8f maxs = max(max(p3, p1), min(p0, p4));
                const Vec8f mins = min(min(p3, p1), max(p0, p4));
                diff = max(max(diff, mins), -maxs);
            }

            const Vec8f spatialPred = min(max(edeintp, p2 - diff), p2 + diff);
            spatialPred.stream(dstp + x);
        }

        _prev2pp += stride;
        _prev2pn += stride;
        _prevp2p += stride;
        _prevp += stride;
        _prevp2n += stride;
        _srcpp += stride;
        _srcpn += stride;
        _nextp2p += stride;
        _nextp += stride;
        _nextp2n += stride;
        _next2pp += stride;
        _next2pn += stride;
        _edeintp += stride;
        dstp += stride;
    }
}
#endif
