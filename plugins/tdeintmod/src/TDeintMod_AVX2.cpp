#ifdef VS_TARGET_CPU_X86
#ifndef __AVX2__
#define __AVX2__
#endif

#include "TDeintMod.hpp"

template<typename T>
static inline T abs_dif(const T & a, const T & b) noexcept {
    return sub_saturated(a, b) | sub_saturated(b, a);
}

template<typename T1, typename T2, int step>
void threshMask_avx2(const VSFrameRef * src, VSFrameRef * dst, const int plane, const TDeintModData * d, const VSAPI * vsapi) noexcept {
    constexpr T1 peak = std::numeric_limits<T1>::max();

    const int width = d->vi.width >> (plane ? d->vi.format->subSamplingW : 0);
    const unsigned height = d->vi.height >> (plane ? d->vi.format->subSamplingH : 0);
    const unsigned stride = vsapi->getStride(src, 0) / sizeof(T1);
    const T1 * srcp = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src, 0)) + d->widthPad;
    T1 * dstp0 = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, 0)) + d->widthPad;
    T1 * dstp1 = dstp0 + stride * height;

    if (plane == 0 && d->mtqL > -1 && d->mthL > -1) {
        std::fill_n(dstp0 - d->widthPad, stride * height, static_cast<T1>(d->mtqL));
        std::fill_n(dstp1 - d->widthPad, stride * height, static_cast<T1>(d->mthL));
        return;
    } else if (plane > 0 && d->mtqC > -1 && d->mthC > -1) {
        std::fill_n(dstp0 - d->widthPad, stride * height, static_cast<T1>(d->mtqC));
        std::fill_n(dstp1 - d->widthPad, stride * height, static_cast<T1>(d->mthC));
        return;
    }

    const T1 * srcpp = srcp;
    const T1 * srcpn = srcp + stride;

    for (unsigned y = 0; y < height; y++) {
        for (int x = 0; x < width; x += step) {
            const T2 topLeft = T2().load(srcpp + x - 1);
            const T2 top = T2().load_a(srcpp + x);
            const T2 topRight = T2().load(srcpp + x + 1);
            const T2 left = T2().load(srcp + x - 1);
            const T2 center = T2().load_a(srcp + x);
            const T2 right = T2().load(srcp + x + 1);
            const T2 bottomLeft = T2().load(srcpn + x - 1);
            const T2 bottom = T2().load_a(srcpn + x);
            const T2 bottomRight = T2().load(srcpn + x + 1);

            T2 min0{ peak }, max0 = zero_256b();
            T2 min1{ peak }, max1 = zero_256b();

            if (d->ttype == 0) { // 4 neighbors - compensated
                min0 = min(min0, top);
                max0 = max(max0, top);
                min1 = min(min1, left);
                max1 = max(max1, left);
                min1 = min(min1, right);
                max1 = max(max1, right);
                min0 = min(min0, bottom);
                max0 = max(max0, bottom);

                const T2 atv = max((abs_dif<T2>(center, min0) + d->vHalf[plane]) >> d->vShift[plane], (abs_dif<T2>(center, max0) + d->vHalf[plane]) >> d->vShift[plane]);
                const T2 ath = max((abs_dif<T2>(center, min1) + d->hHalf[plane]) >> d->hShift[plane], (abs_dif<T2>(center, max1) + d->hHalf[plane]) >> d->hShift[plane]);
                const T2 atmax = max(atv, ath);
                ((atmax + 2) >> 2).stream(dstp0 + x);
                ((atmax + 1) >> 1).stream(dstp1 + x);
            } else if (d->ttype == 1) { // 8 neighbors - compensated
                min0 = min(min0, topLeft);
                max0 = max(max0, topLeft);
                min0 = min(min0, top);
                max0 = max(max0, top);
                min0 = min(min0, topRight);
                max0 = max(max0, topRight);
                min1 = min(min1, left);
                max1 = max(max1, left);
                min1 = min(min1, right);
                max1 = max(max1, right);
                min0 = min(min0, bottomLeft);
                max0 = max(max0, bottomLeft);
                min0 = min(min0, bottom);
                max0 = max(max0, bottom);
                min0 = min(min0, bottomRight);
                max0 = max(max0, bottomRight);

                const T2 atv = max((abs_dif<T2>(center, min0) + d->vHalf[plane]) >> d->vShift[plane], (abs_dif<T2>(center, max0) + d->vHalf[plane]) >> d->vShift[plane]);
                const T2 ath = max((abs_dif<T2>(center, min1) + d->hHalf[plane]) >> d->hShift[plane], (abs_dif<T2>(center, max1) + d->hHalf[plane]) >> d->hShift[plane]);
                const T2 atmax = max(atv, ath);
                ((atmax + 2) >> 2).stream(dstp0 + x);
                ((atmax + 1) >> 1).stream(dstp1 + x);
            } else if (d->ttype == 2) { // 4 neighbors - not compensated
                min0 = min(min0, top);
                max0 = max(max0, top);
                min0 = min(min0, left);
                max0 = max(max0, left);
                min0 = min(min0, right);
                max0 = max(max0, right);
                min0 = min(min0, bottom);
                max0 = max(max0, bottom);

                const T2 at = max(abs_dif<T2>(center, min0), abs_dif<T2>(center, max0));
                ((at + 2) >> 2).stream(dstp0 + x);
                ((at + 1) >> 1).stream(dstp1 + x);
            } else if (d->ttype == 3) { // 8 neighbors - not compensated
                min0 = min(min0, topLeft);
                max0 = max(max0, topLeft);
                min0 = min(min0, top);
                max0 = max(max0, top);
                min0 = min(min0, topRight);
                max0 = max(max0, topRight);
                min0 = min(min0, left);
                max0 = max(max0, left);
                min0 = min(min0, right);
                max0 = max(max0, right);
                min0 = min(min0, bottomLeft);
                max0 = max(max0, bottomLeft);
                min0 = min(min0, bottom);
                max0 = max(max0, bottom);
                min0 = min(min0, bottomRight);
                max0 = max(max0, bottomRight);

                const T2 at = max(abs_dif<T2>(center, min0), abs_dif<T2>(center, max0));
                ((at + 2) >> 2).stream(dstp0 + x);
                ((at + 1) >> 1).stream(dstp1 + x);
            } else if (d->ttype == 4) { // 4 neighbors - not compensated (range)
                min0 = min(min0, top);
                max0 = max(max0, top);
                min0 = min(min0, left);
                max0 = max(max0, left);
                min0 = min(min0, center);
                max0 = max(max0, center);
                min0 = min(min0, right);
                max0 = max(max0, right);
                min0 = min(min0, bottom);
                max0 = max(max0, bottom);

                const T2 at = max0 - min0;
                ((at + 2) >> 2).stream(dstp0 + x);
                ((at + 1) >> 1).stream(dstp1 + x);
            } else { // 8 neighbors - not compensated (range)
                min0 = min(min0, topLeft);
                max0 = max(max0, topLeft);
                min0 = min(min0, top);
                max0 = max(max0, top);
                min0 = min(min0, topRight);
                max0 = max(max0, topRight);
                min0 = min(min0, left);
                max0 = max(max0, left);
                min0 = min(min0, center);
                max0 = max(max0, center);
                min0 = min(min0, right);
                max0 = max(max0, right);
                min0 = min(min0, bottomLeft);
                max0 = max(max0, bottomLeft);
                min0 = min(min0, bottom);
                max0 = max(max0, bottom);
                min0 = min(min0, bottomRight);
                max0 = max(max0, bottomRight);

                const T2 at = max0 - min0;
                ((at + 2) >> 2).stream(dstp0 + x);
                ((at + 1) >> 1).stream(dstp1 + x);
            }
        }

        srcpp = srcp;
        srcp = srcpn;
        if (y < height - 2)
            srcpn += stride;
        dstp0 += stride;
        dstp1 += stride;
    }

    T1 * dstp = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, 0));
    if (plane == 0 && d->mtqL > -1)
        std::fill_n(dstp, stride * height, static_cast<T1>(d->mtqL));
    else if (plane == 0 && d->mthL > -1)
        std::fill_n(dstp + stride * height, stride * height, static_cast<T1>(d->mthL));
    else if (plane > 0 && d->mtqC > -1)
        std::fill_n(dstp, stride * height, static_cast<T1>(d->mtqC));
    else if (plane > 0 && d->mthC > -1)
        std::fill_n(dstp + stride * height, stride * height, static_cast<T1>(d->mthC));
}

template void threshMask_avx2<uint8_t, Vec32uc, 32>(const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *) noexcept;
template void threshMask_avx2<uint16_t, Vec16us, 16>(const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *) noexcept;

template<typename T1, typename T2, int step>
void motionMask_avx2(const VSFrameRef * src1, const VSFrameRef * msk1, const VSFrameRef * src2, const VSFrameRef * msk2, VSFrameRef * dst,
                     const int plane, const TDeintModData * d, const VSAPI * vsapi) noexcept {
    const unsigned width = d->vi.width >> (plane ? d->vi.format->subSamplingW : 0);
    const unsigned height = d->vi.height >> (plane ? d->vi.format->subSamplingH : 0);
    const unsigned stride = vsapi->getStride(src1, 0) / sizeof(T1);
    const T1 * srcp1 = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src1, 0)) + d->widthPad;
    const T1 * srcp2 = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src2, 0)) + d->widthPad;
    const T1 * mskp1q = reinterpret_cast<const T1 *>(vsapi->getReadPtr(msk1, 0)) + d->widthPad;
    const T1 * mskp2q = reinterpret_cast<const T1 *>(vsapi->getReadPtr(msk2, 0)) + d->widthPad;
    T1 * dstpq = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, 0)) + d->widthPad;

    const T1 * mskp1h = mskp1q + stride * height;
    const T1 * mskp2h = mskp2q + stride * height;
    T1 * dstph = dstpq + stride * height;

    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += step) {
            const T2 diff = abs_dif<T2>(T2().load_a(srcp1 + x), T2().load_a(srcp2 + x));
            const T2 minq = min(T2().load_a(mskp1q + x), T2().load_a(mskp2q + x));
            const T2 minh = min(T2().load_a(mskp1h + x), T2().load_a(mskp2h + x));
            const T2 thresh1 = min(max(add_saturated(minq, d->nt), d->minthresh), d->maxthresh);
            const T2 thresh2 = min(max(add_saturated(minh, d->nt), d->minthresh), d->maxthresh);
            select(diff <= thresh1, T2{ 1 }, zero_256b()).stream(dstpq + x);
            select(diff <= thresh2, T2{ 1 }, zero_256b()).stream(dstph + x);
        }

        srcp1 += stride;
        srcp2 += stride;
        mskp1q += stride;
        mskp1h += stride;
        mskp2q += stride;
        mskp2h += stride;
        dstpq += stride;
        dstph += stride;
    }
}

template void motionMask_avx2<uint8_t, Vec32uc, 32>(const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *) noexcept;
template void motionMask_avx2<uint16_t, Vec16us, 16>(const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *) noexcept;

template<typename T1, typename T2, int step>
void andMasks_avx2(const VSFrameRef * src1, const VSFrameRef * src2, VSFrameRef * dst, const int plane, const TDeintModData * d, const VSAPI * vsapi) noexcept {
    const unsigned width = d->vi.width >> (plane ? d->vi.format->subSamplingW : 0);
    const unsigned height = (d->vi.height * 2) >> (plane ? d->vi.format->subSamplingH : 0);
    const unsigned stride = vsapi->getStride(src1, 0) / sizeof(T1);
    const T1 * srcp1 = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src1, 0)) + d->widthPad;
    const T1 * srcp2 = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src2, 0)) + d->widthPad;
    T1 * dstp = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, 0)) + d->widthPad;

    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x += step)
            (T2().load_a(srcp1 + x) & T2().load_a(srcp2 + x) & T2().load_a(dstp + x)).stream(dstp + x);

        dstp[-1] = dstp[0];
        dstp[width] = dstp[width - 1];

        srcp1 += stride;
        srcp2 += stride;
        dstp += stride;
    }
}

template void andMasks_avx2<uint8_t, Vec32uc, 32>(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *) noexcept;
template void andMasks_avx2<uint16_t, Vec16us, 16>(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *) noexcept;

template<typename T1, typename T2, int step>
void combineMasks_avx2(const VSFrameRef * src, VSFrameRef * dst, const int plane, const TDeintModData * d, const VSAPI * vsapi) noexcept {
    const int width = vsapi->getFrameWidth(dst, plane);
    const unsigned height = vsapi->getFrameHeight(dst, plane);
    const unsigned srcStride = vsapi->getStride(src, 0) / sizeof(T1);
    const unsigned dstStride = vsapi->getStride(dst, plane) / sizeof(T1);
    const T1 * srcp0 = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src, 0)) + d->widthPad;
    T1 * dstp = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane));

    const T1 * srcpp0 = srcp0;
    const T1 * srcpn0 = srcp0 + srcStride;
    const T1 * srcp1 = srcp0 + srcStride * height;

    vs_bitblt(dstp, vsapi->getStride(dst, plane), srcp0, vsapi->getStride(src, 0), width * sizeof(T1), height);

    for (unsigned y = 0; y < height; y++) {
        for (int x = 0; x < width; x += step) {
            const T2 count = T2().load(srcpp0 + x - 1) + T2().load_a(srcpp0 + x) + T2().load(srcpp0 + x + 1) +
                             T2().load(srcp0 + x - 1) + T2().load(srcp0 + x + 1) +
                             T2().load(srcpn0 + x - 1) + T2().load_a(srcpn0 + x) + T2().load(srcpn0 + x + 1);
            select((T2().load_a(srcp0 + x) == T2{ zero_256b() }) & (T2().load_a(srcp1 + x) != T2{ zero_256b() }) & (count >= d->cstr),
                   std::numeric_limits<T1>::max(), T2().load_a(dstp + x)).stream(dstp + x);
        }

        srcpp0 = srcp0;
        srcp0 = srcpn0;
        if (y < height - 2)
            srcpn0 += srcStride;
        srcp1 += srcStride;
        dstp += dstStride;
    }
}

template void combineMasks_avx2<uint8_t, Vec32uc, 32>(const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *) noexcept;
template void combineMasks_avx2<uint16_t, Vec16us, 16>(const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *) noexcept;
#endif
