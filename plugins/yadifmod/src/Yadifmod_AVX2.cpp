#ifdef VS_TARGET_CPU_X86
#ifndef __AVX2__
#define __AVX2__
#endif

#include "vectorclass/vectorclass.h"

template<typename T> void filter_avx2(const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, const T *, T *, const unsigned, const unsigned, const unsigned, const unsigned, const unsigned) noexcept;

template<>
void filter_avx2(const uint8_t * _prev2pp, const uint8_t * _prev2pn, const uint8_t * _prevp2p, const uint8_t * _prevp, const uint8_t * _prevp2n,
                 const uint8_t * _srcpp, const uint8_t * _srcpn,
                 const uint8_t * _nextp2p, const uint8_t * _nextp, const uint8_t * _nextp2n, const uint8_t * _next2pp, const uint8_t * _next2pn,
                 const uint8_t * _edeintp, uint8_t * dstp,
                 const unsigned width, const unsigned yStart, const unsigned yStop, const unsigned stride, const unsigned mode) noexcept {
    for (unsigned y = yStart; y <= yStop; y += 2) {
        for (unsigned x = 0; x < width; x += 16) {
            const Vec16s prev2pp = Vec16s().load_16uc(_prev2pp + x);
            const Vec16s prev2pn = Vec16s().load_16uc(_prev2pn + x);
            const Vec16s prevp = Vec16s().load_16uc(_prevp + x);
            const Vec16s srcpp = Vec16s().load_16uc(_srcpp + x);
            const Vec16s srcpn = Vec16s().load_16uc(_srcpn + x);
            const Vec16s nextp = Vec16s().load_16uc(_nextp + x);
            const Vec16s next2pp = Vec16s().load_16uc(_next2pp + x);
            const Vec16s next2pn = Vec16s().load_16uc(_next2pn + x);
            const Vec16s edeintp = Vec16s().load_16uc(_edeintp + x);

            Vec16s p1 = srcpp;
            const Vec16s p2 = (prevp + nextp) >> 1;
            Vec16s p3 = srcpn;
            const Vec16s tdiff0 = abs(prevp - nextp) >> 1;
            const Vec16s tdiff1 = (abs(prev2pp - p1) + abs(prev2pn - p3)) >> 1;
            const Vec16s tdiff2 = (abs(next2pp - p1) + abs(next2pn - p3)) >> 1;
            Vec16s diff = max(max(tdiff0, tdiff1), tdiff2);

            if (mode < 2) {
                const Vec16s prevp2p = Vec16s().load_16uc(_prevp2p + x);
                const Vec16s prevp2n = Vec16s().load_16uc(_prevp2n + x);
                const Vec16s nextp2p = Vec16s().load_16uc(_nextp2p + x);
                const Vec16s nextp2n = Vec16s().load_16uc(_nextp2n + x);

                const Vec16s p0 = ((prevp2p + nextp2p) >> 1) - p1;
                const Vec16s p4 = ((prevp2n + nextp2n) >> 1) - p3;
                p1 = p2 - p1;
                p3 = p2 - p3;
                const Vec16s maxs = max(max(p3, p1), min(p0, p4));
                const Vec16s mins = min(min(p3, p1), max(p0, p4));
                diff = max(max(diff, mins), -maxs);
            }

            const Vec16s spatialPred = min(max(edeintp, p2 - diff), p2 + diff);
            compress_saturated_s2u(spatialPred, spatialPred).get_low().stream(dstp + x);
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
void filter_avx2(const uint16_t * _prev2pp, const uint16_t * _prev2pn, const uint16_t * _prevp2p, const uint16_t * _prevp, const uint16_t * _prevp2n,
                 const uint16_t * _srcpp, const uint16_t * _srcpn,
                 const uint16_t * _nextp2p, const uint16_t * _nextp, const uint16_t * _nextp2n, const uint16_t * _next2pp, const uint16_t * _next2pn,
                 const uint16_t * _edeintp, uint16_t * dstp,
                 const unsigned width, const unsigned yStart, const unsigned yStop, const unsigned stride, const unsigned mode) noexcept {
    for (unsigned y = yStart; y <= yStop; y += 2) {
        for (unsigned x = 0; x < width; x += 8) {
            const Vec8i prev2pp = Vec8i().load_8us(_prev2pp + x);
            const Vec8i prev2pn = Vec8i().load_8us(_prev2pn + x);
            const Vec8i prevp = Vec8i().load_8us(_prevp + x);
            const Vec8i srcpp = Vec8i().load_8us(_srcpp + x);
            const Vec8i srcpn = Vec8i().load_8us(_srcpn + x);
            const Vec8i nextp = Vec8i().load_8us(_nextp + x);
            const Vec8i next2pp = Vec8i().load_8us(_next2pp + x);
            const Vec8i next2pn = Vec8i().load_8us(_next2pn + x);
            const Vec8i edeintp = Vec8i().load_8us(_edeintp + x);

            Vec8i p1 = srcpp;
            const Vec8i p2 = (prevp + nextp) >> 1;
            Vec8i p3 = srcpn;
            const Vec8i tdiff0 = abs(prevp - nextp) >> 1;
            const Vec8i tdiff1 = (abs(prev2pp - p1) + abs(prev2pn - p3)) >> 1;
            const Vec8i tdiff2 = (abs(next2pp - p1) + abs(next2pn - p3)) >> 1;
            Vec8i diff = max(max(tdiff0, tdiff1), tdiff2);

            if (mode < 2) {
                const Vec8i prevp2p = Vec8i().load_8us(_prevp2p + x);
                const Vec8i prevp2n = Vec8i().load_8us(_prevp2n + x);
                const Vec8i nextp2p = Vec8i().load_8us(_nextp2p + x);
                const Vec8i nextp2n = Vec8i().load_8us(_nextp2n + x);

                const Vec8i p0 = ((prevp2p + nextp2p) >> 1) - p1;
                const Vec8i p4 = ((prevp2n + nextp2n) >> 1) - p3;
                p1 = p2 - p1;
                p3 = p2 - p3;
                const Vec8i maxs = max(max(p3, p1), min(p0, p4));
                const Vec8i mins = min(min(p3, p1), max(p0, p4));
                diff = max(max(diff, mins), -maxs);
            }

            const Vec8i spatialPred = min(max(edeintp, p2 - diff), p2 + diff);
            compress_saturated_s2u(spatialPred, spatialPred).get_low().stream(dstp + x);
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
void filter_avx2(const float * _prev2pp, const float * _prev2pn, const float * _prevp2p, const float * _prevp, const float * _prevp2n, const float * _srcpp, const float * _srcpn,
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
