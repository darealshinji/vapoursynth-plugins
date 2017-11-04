#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <unordered_map>

#include <VapourSynth.h>
#include <VSHelper.h>

template<typename T>
static void copyPad(const VSFrameRef * src, VSFrameRef * dst, const int plane, const int off, const bool dh, const VSAPI * vsapi) noexcept {
    const int srcWidth = vsapi->getFrameWidth(src, plane);
    const int dstWidth = vsapi->getFrameWidth(dst, 0);
    const int srcHeight = vsapi->getFrameHeight(src, plane);
    const int dstHeight = vsapi->getFrameHeight(dst, 0);
    const int srcStride = vsapi->getStride(src, plane) / sizeof(T);
    const int dstStride = vsapi->getStride(dst, 0) / sizeof(T);
    const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 0));

    if (!dh)
        vs_bitblt(dstp + dstStride * (4 + off) + 12, vsapi->getStride(dst, 0) * 2,
                  srcp + srcStride * off, vsapi->getStride(src, plane) * 2,
                  srcWidth * sizeof(T), srcHeight / 2);
    else
        vs_bitblt(dstp + dstStride * (4 + off) + 12, vsapi->getStride(dst, 0) * 2,
                  srcp, vsapi->getStride(src, plane),
                  srcWidth * sizeof(T), srcHeight);

    dstp += dstStride * (4 + off);

    for (int y = 4 + off; y < dstHeight - 4; y += 2) {
        for (int x = 0; x < 12; x++)
            dstp[x] = dstp[24 - x];

        for (int x = dstWidth - 12, c = 2; x < dstWidth; x++, c += 2)
            dstp[x] = dstp[x - c];

        dstp += dstStride * 2;
    }

    dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, 0));

    for (int y = off; y < 4; y += 2)
        memcpy(dstp + dstStride * y, dstp + dstStride * (8 - y), dstWidth * sizeof(T));

    for (int y = dstHeight - 4 + off, c = 2 + 2 * off; y < dstHeight; y += 2, c += 4)
        memcpy(dstp + dstStride * y, dstp + dstStride * (y - c), dstWidth * sizeof(T));
}

template<typename T>
static inline void interpolate(const T * src3p, const T * src1p, const T * src1n, const T * src3n, const int * fpath, int * VS_RESTRICT dmap, T * VS_RESTRICT dstp,
                               const int width, const bool ucubic, const int peak) noexcept {
    for (int x = 0; x < width; x++) {
        const int dir = fpath[x];
        const int dir3 = dir * 3;
        const int absDir3 = std::abs(dir3);

        dmap[x] = dir;

        if (ucubic && x >= absDir3 && x <= width - 1 - absDir3)
            dstp[x] = std::min(std::max((36 * (src1p[x + dir] + src1n[x - dir]) - 4 * (src3p[x + dir3] + src3n[x - dir3]) + 32) >> 6, 0), peak);
        else
            dstp[x] = (src1p[x + dir] + src1n[x - dir] + 1) >> 1;
    }
}

template<>
inline void interpolate(const float * src3p, const float * src1p, const float * src1n, const float * src3n, const int * fpath, int * VS_RESTRICT dmap, float * VS_RESTRICT dstp,
                        const int width, const bool ucubic, const int peak) noexcept {
    for (int x = 0; x < width; x++) {
        const int dir = fpath[x];
        const int dir3 = dir * 3;
        const int absDir3 = std::abs(dir3);

        dmap[x] = dir;

        if (ucubic && x >= absDir3 && x <= width - 1 - absDir3)
            dstp[x] = 0.5625f * (src1p[x + dir] + src1n[x - dir]) - 0.0625f * (src3p[x + dir3] + src3n[x - dir3]);
        else
            dstp[x] = (src1p[x + dir] + src1n[x - dir]) * 0.5f;
    }
}

template<typename T>
static void vCheck(const T * srcp, const T * scpp, T * VS_RESTRICT dstp, const int * dmap, void * _tline, const int field_n,
                   const int dstWidth, const int srcHeight, const int srcStride, const int dstStride,
                   const int vcheck, const float vthresh2, const float rcpVthresh0, const float rcpVthresh1, const float rcpVthresh2, const int peak) noexcept {
    for (int y = 4 + field_n; y < srcHeight - 4; y += 2) {
        if (y >= 6 && y < srcHeight - 6) {
            const T * dst3p = srcp - srcStride * 3;
            const T * dst2p = dstp - dstStride * 2;
            const T * dst1p = dstp - dstStride;
            const T * dst1n = dstp + dstStride;
            const T * dst2n = dstp + dstStride * 2;
            const T * dst3n = srcp + srcStride * 3;
            T * VS_RESTRICT tline = reinterpret_cast<T *>(_tline);

            for (int x = 0; x < dstWidth; x++) {
                const int dirc = dmap[x];
                const T cint = scpp ? scpp[x] : std::min(std::max((36 * (dst1p[x] + dst1n[x]) - 4 * (dst3p[x] + dst3n[x]) + 32) >> 6, 0), peak);

                if (dirc == 0) {
                    tline[x] = cint;
                    continue;
                }

                const int dirt = dmap[x - dstWidth];
                const int dirb = dmap[x + dstWidth];

                if (std::max(dirc * dirt, dirc * dirb) < 0 || (dirt == dirb && dirt == 0)) {
                    tline[x] = cint;
                    continue;
                }

                const int it = (dst2p[x + dirc] + dstp[x - dirc] + 1) >> 1;
                const int vt = std::abs(dst2p[x + dirc] - dst1p[x + dirc]) + std::abs(dstp[x + dirc] - dst1p[x + dirc]);
                const int ib = (dstp[x + dirc] + dst2n[x - dirc] + 1) >> 1;
                const int vb = std::abs(dst2n[x - dirc] - dst1n[x - dirc]) + std::abs(dstp[x - dirc] - dst1n[x - dirc]);
                const int vc = std::abs(dstp[x] - dst1p[x]) + std::abs(dstp[x] - dst1n[x]);

                const int d0 = std::abs(it - dst1p[x]);
                const int d1 = std::abs(ib - dst1n[x]);
                const int d2 = std::abs(vt - vc);
                const int d3 = std::abs(vb - vc);

                const int mdiff0 = (vcheck == 1) ? std::min(d0, d1) : (vcheck == 2 ? (d0 + d1 + 1) >> 1 : std::max(d0, d1));
                const int mdiff1 = (vcheck == 1) ? std::min(d2, d3) : (vcheck == 2 ? (d2 + d3 + 1) >> 1 : std::max(d2, d3));

                const float a0 = mdiff0 * rcpVthresh0;
                const float a1 = mdiff1 * rcpVthresh1;
                const float a2 = std::max((vthresh2 - std::abs(dirc)) * rcpVthresh2, 0.f);
                const float a = std::min(std::max({ a0, a1, a2 }), 1.f);

                tline[x] = static_cast<T>((1.f - a) * dstp[x] + a * cint);
            }

            memcpy(dstp, tline, dstWidth * sizeof(T));
        }

        srcp += srcStride * 2;
        if (scpp)
            scpp += dstStride * 2;
        dstp += dstStride * 2;
        dmap += dstWidth;
    }
}

template<>
void vCheck(const float * srcp, const float * scpp, float * VS_RESTRICT dstp, const int * dmap, void * _tline, const int field_n,
            const int dstWidth, const int srcHeight, const int srcStride, const int dstStride,
            const int vcheck, const float vthresh2, const float rcpVthresh0, const float rcpVthresh1, const float rcpVthresh2, const int peak) noexcept {
    for (int y = 4 + field_n; y < srcHeight - 4; y += 2) {
        if (y >= 6 && y < srcHeight - 6) {
            const float * dst3p = srcp - srcStride * 3;
            const float * dst2p = dstp - dstStride * 2;
            const float * dst1p = dstp - dstStride;
            const float * dst1n = dstp + dstStride;
            const float * dst2n = dstp + dstStride * 2;
            const float * dst3n = srcp + srcStride * 3;
            float * VS_RESTRICT tline = reinterpret_cast<float *>(_tline);

            for (int x = 0; x < dstWidth; x++) {
                const int dirc = dmap[x];
                const float cint = scpp ? scpp[x] : 0.5625f * (dst1p[x] + dst1n[x]) - 0.0625f * (dst3p[x] + dst3n[x]);

                if (dirc == 0) {
                    tline[x] = cint;
                    continue;
                }

                const int dirt = dmap[x - dstWidth];
                const int dirb = dmap[x + dstWidth];

                if (std::max(dirc * dirt, dirc * dirb) < 0 || (dirt == dirb && dirt == 0)) {
                    tline[x] = cint;
                    continue;
                }

                const float it = (dst2p[x + dirc] + dstp[x - dirc]) * 0.5f;
                const float vt = std::abs(dst2p[x + dirc] - dst1p[x + dirc]) + std::abs(dstp[x + dirc] - dst1p[x + dirc]);
                const float ib = (dstp[x + dirc] + dst2n[x - dirc]) * 0.5f;
                const float vb = std::abs(dst2n[x - dirc] - dst1n[x - dirc]) + std::abs(dstp[x - dirc] - dst1n[x - dirc]);
                const float vc = std::abs(dstp[x] - dst1p[x]) + std::abs(dstp[x] - dst1n[x]);

                const float d0 = std::abs(it - dst1p[x]);
                const float d1 = std::abs(ib - dst1n[x]);
                const float d2 = std::abs(vt - vc);
                const float d3 = std::abs(vb - vc);

                const float mdiff0 = (vcheck == 1) ? std::min(d0, d1) : (vcheck == 2 ? (d0 + d1) * 0.5f : std::max(d0, d1));
                const float mdiff1 = (vcheck == 1) ? std::min(d2, d3) : (vcheck == 2 ? (d2 + d3) * 0.5f : std::max(d2, d3));

                const float a0 = mdiff0 * rcpVthresh0;
                const float a1 = mdiff1 * rcpVthresh1;
                const float a2 = std::max((vthresh2 - std::abs(dirc)) * rcpVthresh2, 0.f);
                const float a = std::min(std::max({ a0, a1, a2 }), 1.f);

                tline[x] = (1.f - a) * dstp[x] + a * cint;
            }

            memcpy(dstp, tline, dstWidth * sizeof(float));
        }

        srcp += srcStride * 2;
        if (scpp)
            scpp += dstStride * 2;
        dstp += dstStride * 2;
        dmap += dstWidth;
    }
}
