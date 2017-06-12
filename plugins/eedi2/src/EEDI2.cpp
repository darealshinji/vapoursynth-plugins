/*
**   VapourSynth port by HolyWu
**
**                    EEDI2 v0.9.2 for AviSynth 2.5.x
**
**   EEDI2 resizes an image by 2x in the vertical direction by copying the
**   existing image to 2*y(n) and interpolating the missing field.  It is
**   intended for edge-directed interpolation for deinterlacing (i.e. not
**   really made for resizing a normal image).
**   
**   Copyright (C) 2005-2006 Kevin Stone
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
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include <VapourSynth.h>
#include <VSHelper.h>

struct EEDI2Data {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    VSVideoInfo vi2;
    int field, mthresh, lthresh, vthresh, estr, dstr, maxd, map, pp;
    unsigned fieldS, nt4, nt7, nt8, nt13, nt19;
    int8_t * limlut;
    int16_t * limlut2;
    std::unordered_map<std::thread::id, int *> cx2, cy2, cxy, tmpc;
};

template<typename T>
static void buildEdgeMask(const VSFrameRef * src, VSFrameRef * dst, const int plane, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift = d->vi->format->bitsPerSample - 8;
    const T ten = 10 << shift;

    const unsigned width = vsapi->getFrameWidth(src, plane);
    const unsigned height = vsapi->getFrameHeight(src, plane);
    const unsigned stride = vsapi->getStride(src, plane) / sizeof(T);
    const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    memset(dstp, 0, vsapi->getStride(dst, plane) * height);

    srcp += stride;
    dstp += stride;

    const T * srcpp = srcp - stride;
    const T * srcpn = srcp + stride;

    for (unsigned y = 1; y < height - 1; y++) {
        for (unsigned x = 1; x < width - 1; x++) {
            if ((std::abs(srcpp[x] - srcp[x]) < ten && std::abs(srcp[x] - srcpn[x]) < ten && std::abs(srcpp[x] - srcpn[x]) < ten) ||
                (std::abs(srcpp[x - 1] - srcp[x - 1]) < ten && std::abs(srcp[x - 1] - srcpn[x - 1]) < ten && std::abs(srcpp[x - 1] - srcpn[x - 1]) < ten &&
                 std::abs(srcpp[x + 1] - srcp[x + 1]) < ten && std::abs(srcp[x + 1] - srcpn[x + 1]) < ten && std::abs(srcpp[x + 1] - srcpn[x + 1]) < ten))
                continue;

            const unsigned sum = (srcpp[x - 1] + srcpp[x] + srcpp[x + 1] +
                                  srcp[x - 1] + srcp[x] + srcp[x + 1] +
                                  srcpn[x - 1] + srcpn[x] + srcpn[x + 1]) >> shift;
            const unsigned sumsq = (srcpp[x - 1] >> shift) * (srcpp[x - 1] >> shift) + (srcpp[x] >> shift) * (srcpp[x] >> shift) + (srcpp[x + 1] >> shift) * (srcpp[x + 1] >> shift) +
                                   (srcp[x - 1] >> shift) * (srcp[x - 1] >> shift) + (srcp[x] >> shift) * (srcp[x] >> shift) + (srcp[x + 1] >> shift) * (srcp[x + 1] >> shift) +
                                   (srcpn[x - 1] >> shift) * (srcpn[x - 1] >> shift) + (srcpn[x] >> shift) * (srcpn[x] >> shift) + (srcpn[x + 1] >> shift) * (srcpn[x + 1] >> shift);
            if (9 * sumsq - sum * sum < static_cast<unsigned>(d->vthresh))
                continue;

            const unsigned Ix = std::abs(srcp[x + 1] - srcp[x - 1]) >> shift;
            const unsigned Iy = std::max({ std::abs(srcpp[x] - srcpn[x]), std::abs(srcpp[x] - srcp[x]), std::abs(srcp[x] - srcpn[x]) }) >> shift;
            if (Ix * Ix + Iy * Iy >= static_cast<unsigned>(d->mthresh)) {
                dstp[x] = peak;
                continue;
            }

            const unsigned Ixx = std::abs(srcp[x - 1] - 2 * srcp[x] + srcp[x + 1]) >> shift;
            const unsigned Iyy = std::abs(srcpp[x] - 2 * srcp[x] + srcpn[x]) >> shift;
            if (Ixx + Iyy >= static_cast<unsigned>(d->lthresh))
                dstp[x] = peak;
        }

        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        dstp += stride;
    }
}

template<typename T>
static void erode(const VSFrameRef * msk, VSFrameRef * dst, const int plane, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), mskp, vsapi->getStride(msk, plane), width * sizeof(T), height);

    mskp += stride;
    dstp += stride;

    const T * mskpp = mskp - stride;
    const T * mskpn = mskp + stride;

    for (unsigned y = 1; y < height - 1; y++) {
        for (unsigned x = 1; x < width - 1; x++) {
            if (mskp[x] != peak)
                continue;

            unsigned count = 0;

            if (mskpp[x - 1] == peak)
                count++;
            if (mskpp[x] == peak)
                count++;
            if (mskpp[x + 1] == peak)
                count++;
            if (mskp[x - 1] == peak)
                count++;
            if (mskp[x + 1] == peak)
                count++;
            if (mskpn[x - 1] == peak)
                count++;
            if (mskpn[x] == peak)
                count++;
            if (mskpn[x + 1] == peak)
                count++;

            if (count < static_cast<unsigned>(d->estr))
                dstp[x] = 0;
        }

        mskpp += stride;
        mskp += stride;
        mskpn += stride;
        dstp += stride;
    }
}

template<typename T>
static void dilate(const VSFrameRef * msk, VSFrameRef * dst, const int plane, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), mskp, vsapi->getStride(msk, plane), width * sizeof(T), height);

    mskp += stride;
    dstp += stride;

    const T * mskpp = mskp - stride;
    const T * mskpn = mskp + stride;

    for (unsigned y = 1; y < height - 1; y++) {
        for (unsigned x = 1; x < width - 1; x++) {
            if (mskp[x] != 0)
                continue;

            unsigned count = 0;

            if (mskpp[x - 1] == peak)
                count++;
            if (mskpp[x] == peak)
                count++;
            if (mskpp[x + 1] == peak)
                count++;
            if (mskp[x - 1] == peak)
                count++;
            if (mskp[x + 1] == peak)
                count++;
            if (mskpn[x - 1] == peak)
                count++;
            if (mskpn[x] == peak)
                count++;
            if (mskpn[x + 1] == peak)
                count++;

            if (count >= static_cast<unsigned>(d->dstr))
                dstp[x] = peak;
        }

        mskpp += stride;
        mskp += stride;
        mskpn += stride;
        dstp += stride;
    }
}

template<typename T>
static void removeSmallHorzGaps(const VSFrameRef * msk, VSFrameRef * dst, const int plane, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), mskp, vsapi->getStride(msk, plane), width * sizeof(T), height);

    mskp += stride;
    dstp += stride;

    for (unsigned y = 1; y < height - 1; y++) {
        for (unsigned x = 3; x < width - 3; x++) {
            if (mskp[x]) {
                if (mskp[x - 3] || mskp[x - 2] || mskp[x - 1] ||
                    mskp[x + 1] || mskp[x + 2] || mskp[x + 3])
                    continue;
                dstp[x] = 0;
            } else {
                if ((mskp[x + 1] && (mskp[x - 1] || mskp[x - 2] || mskp[x - 3])) ||
                    (mskp[x + 2] && (mskp[x - 1] || mskp[x - 2])) ||
                    (mskp[x + 3] && mskp[x - 1]))
                    dstp[x] = peak;
            }
        }

        mskp += stride;
        dstp += stride;
    }
}

template<typename T>
static void calcDirections(const VSFrameRef * src, const VSFrameRef * msk, VSFrameRef * dst, const int plane, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift2 = 2 + (d->vi->format->bitsPerSample - 8);

    const int width = vsapi->getFrameWidth(src, plane);
    const unsigned height = vsapi->getFrameHeight(src, plane);
    const unsigned stride = vsapi->getStride(src, plane) / sizeof(T);
    const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    std::fill_n(dstp, stride * height, peak);

    srcp += stride;
    mskp += stride;
    dstp += stride;

    const T * src2p = srcp - stride * 2;
    const T * srcpp = srcp - stride;
    const T * srcpn = srcp + stride;
    const T * src2n = srcp + stride * 2;
    const T * mskpp = mskp - stride;
    const T * mskpn = mskp + stride;

    const int maxd = d->maxd >> (plane ? d->vi->format->subSamplingW : 0);

    for (unsigned y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (mskp[x] != peak || (mskp[x - 1] != peak && mskp[x + 1] != peak))
                continue;

            const int uStart = std::max(-x + 1, -maxd);
            const int uStop = std::min(width - 2 - x, maxd);
            const unsigned min0 = std::abs(srcp[x] - srcpn[x]) + std::abs(srcp[x] - srcpp[x]);
            unsigned minA = std::min(d->nt19, min0 * 9);
            unsigned minB = std::min(d->nt13, min0 * 6);
            unsigned minC = minA;
            unsigned minD = minB;
            unsigned minE = minB;
            int dirA = -5000, dirB = -5000, dirC = -5000, dirD = -5000, dirE = -5000;

            for (int u = uStart; u <= uStop; u++) {
                if ((y == 1 || mskpp[x - 1 + u] == peak || mskpp[x + u] == peak || mskpp[x + 1 + u] == peak) &&
                    (y == height - 2 || mskpn[x - 1 - u] == peak || mskpn[x - u] == peak || mskpn[x + 1 - u] == peak)) {
                    const unsigned diffsn = std::abs(srcp[x - 1] - srcpn[x - 1 - u]) + std::abs(srcp[x] - srcpn[x - u]) + std::abs(srcp[x + 1] - srcpn[x + 1 - u]);
                    const unsigned diffsp = std::abs(srcp[x - 1] - srcpp[x - 1 + u]) + std::abs(srcp[x] - srcpp[x + u]) + std::abs(srcp[x + 1] - srcpp[x + 1 + u]);
                    const unsigned diffps = std::abs(srcpp[x - 1] - srcp[x - 1 - u]) + std::abs(srcpp[x] - srcp[x - u]) + std::abs(srcpp[x + 1] - srcp[x + 1 - u]);
                    const unsigned diffns = std::abs(srcpn[x - 1] - srcp[x - 1 + u]) + std::abs(srcpn[x] - srcp[x + u]) + std::abs(srcpn[x + 1] - srcp[x + 1 + u]);
                    const unsigned diff = diffsn + diffsp + diffps + diffns;
                    unsigned diffD = diffsp + diffns;
                    unsigned diffE = diffsn + diffps;

                    if (diff < minB) {
                        dirB = u;
                        minB = diff;
                    }

                    if (y > 1) {
                        const unsigned diff2pp = std::abs(src2p[x - 1] - srcpp[x - 1 - u]) + std::abs(src2p[x] - srcpp[x - u]) + std::abs(src2p[x + 1] - srcpp[x + 1 - u]);
                        const unsigned diffp2p = std::abs(srcpp[x - 1] - src2p[x - 1 + u]) + std::abs(srcpp[x] - src2p[x + u]) + std::abs(srcpp[x + 1] - src2p[x + 1 + u]);
                        const unsigned diffA = diff + diff2pp + diffp2p;
                        diffD += diffp2p;
                        diffE += diff2pp;

                        if (diffA < minA) {
                            dirA = u;
                            minA = diffA;
                        }
                    }

                    if (y < height - 2) {
                        const unsigned diff2nn = std::abs(src2n[x - 1] - srcpn[x - 1 + u]) + std::abs(src2n[x] - srcpn[x + u]) + std::abs(src2n[x + 1] - srcpn[x + 1 + u]);
                        const unsigned diffn2n = std::abs(srcpn[x - 1] - src2n[x - 1 - u]) + std::abs(srcpn[x] - src2n[x - u]) + std::abs(srcpn[x + 1] - src2n[x + 1 - u]);
                        const unsigned diffC = diff + diff2nn + diffn2n;
                        diffD += diff2nn;
                        diffE += diffn2n;

                        if (diffC < minC) {
                            dirC = u;
                            minC = diffC;
                        }
                    }

                    if (diffD < minD) {
                        dirD = u;
                        minD = diffD;
                    }

                    if (diffE < minE) {
                        dirE = u;
                        minE = diffE;
                    }
                }
            }

            int order[5];
            unsigned k = 0;

            if (dirA != -5000)
                order[k++] = dirA;
            if (dirB != -5000)
                order[k++] = dirB;
            if (dirC != -5000)
                order[k++] = dirC;
            if (dirD != -5000)
                order[k++] = dirD;
            if (dirE != -5000)
                order[k++] = dirE;

            if (k > 1) {
                std::sort(order, order + k);

                const int mid = (k & 1) ? order[k / 2] : (order[(k - 1) / 2] + order[k / 2] + 1) / 2;
                const int lim = std::max(d->limlut[std::abs(mid)] / 4, 2);
                int sum = 0;
                unsigned count = 0;

                for (unsigned i = 0; i < k; i++) {
                    if (std::abs(order[i] - mid) <= lim) {
                        sum += order[i];
                        count++;
                    }
                }

                dstp[x] = (count > 1) ? neutral + (static_cast<int>(static_cast<float>(sum) / count) << shift2) : neutral;
            } else {
                dstp[x] = neutral;
            }
        }

        src2p += stride;
        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        src2n += stride;
        mskpp += stride;
        mskp += stride;
        mskpn += stride;
        dstp += stride;
    }
}

template<typename T>
static void filterDirMap(const VSFrameRef * msk, const VSFrameRef * dmsk, VSFrameRef * dst, const int plane, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift2 = 2 + (d->vi->format->bitsPerSample - 8);

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    const T * dmskp = reinterpret_cast<const T *>(vsapi->getReadPtr(dmsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), dmskp, vsapi->getStride(dmsk, plane), width * sizeof(T), height);

    mskp += stride;
    dmskp += stride;
    dstp += stride;

    const T * dmskpp = dmskp - stride;
    const T * dmskpn = dmskp + stride;

    for (unsigned y = 1; y < height - 1; y++) {
        for (unsigned x = 1; x < width - 1; x++) {
            if (mskp[x] != peak)
                continue;

            int order[9];
            unsigned u = 0;

            if (dmskpp[x - 1] != peak)
                order[u++] = dmskpp[x - 1];
            if (dmskpp[x] != peak)
                order[u++] = dmskpp[x];
            if (dmskpp[x + 1] != peak)
                order[u++] = dmskpp[x + 1];
            if (dmskp[x - 1] != peak)
                order[u++] = dmskp[x - 1];
            if (dmskp[x] != peak)
                order[u++] = dmskp[x];
            if (dmskp[x + 1] != peak)
                order[u++] = dmskp[x + 1];
            if (dmskpn[x - 1] != peak)
                order[u++] = dmskpn[x - 1];
            if (dmskpn[x] != peak)
                order[u++] = dmskpn[x];
            if (dmskpn[x + 1] != peak)
                order[u++] = dmskpn[x + 1];

            if (u < 4) {
                dstp[x] = peak;
                continue;
            }

            std::sort(order, order + u);

            const int mid = (u & 1) ? order[u / 2] : (order[(u - 1) / 2] + order[u / 2] + 1) / 2;
            const int lim = d->limlut2[std::abs(mid - neutral) >> shift2];
            int sum = 0;
            unsigned count = 0;

            for (unsigned i = 0; i < u; i++) {
                if (std::abs(order[i] - mid) <= lim) {
                    sum += order[i];
                    count++;
                }
            }

            if (count < 4 || (count < 5 && dmskp[x] == peak)) {
                dstp[x] = peak;
                continue;
            }

            dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / (count + 1) + 0.5f);
        }

        mskp += stride;
        dmskpp += stride;
        dmskp += stride;
        dmskpn += stride;
        dstp += stride;
    }
}

template<typename T>
static void expandDirMap(const VSFrameRef * msk, const VSFrameRef * dmsk, VSFrameRef * dst, const int plane, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift2 = 2 + (d->vi->format->bitsPerSample - 8);

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    const T * dmskp = reinterpret_cast<const T *>(vsapi->getReadPtr(dmsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), dmskp, vsapi->getStride(dmsk, plane), width * sizeof(T), height);

    mskp += stride;
    dmskp += stride;
    dstp += stride;

    const T * dmskpp = dmskp - stride;
    const T * dmskpn = dmskp + stride;

    for (unsigned y = 1; y < height - 1; y++) {
        for (unsigned x = 1; x < width - 1; x++) {
            if (dmskp[x] != peak || mskp[x] != peak)
                continue;

            int order[9];
            unsigned u = 0;

            if (dmskpp[x - 1] != peak)
                order[u++] = dmskpp[x - 1];
            if (dmskpp[x] != peak)
                order[u++] = dmskpp[x];
            if (dmskpp[x + 1] != peak)
                order[u++] = dmskpp[x + 1];
            if (dmskp[x - 1] != peak)
                order[u++] = dmskp[x - 1];
            if (dmskp[x + 1] != peak)
                order[u++] = dmskp[x + 1];
            if (dmskpn[x - 1] != peak)
                order[u++] = dmskpn[x - 1];
            if (dmskpn[x] != peak)
                order[u++] = dmskpn[x];
            if (dmskpn[x + 1] != peak)
                order[u++] = dmskpn[x + 1];

            if (u < 5)
                continue;

            std::sort(order, order + u);

            const int mid = (u & 1) ? order[u / 2] : (order[(u - 1) / 2] + order[u / 2] + 1) / 2;
            const int lim = d->limlut2[std::abs(mid - neutral) >> shift2];
            int sum = 0;
            unsigned count = 0;

            for (unsigned i = 0; i < u; i++) {
                if (std::abs(order[i] - mid) <= lim) {
                    sum += order[i];
                    count++;
                }
            }

            if (count < 5)
                continue;

            dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / (count + 1) + 0.5f);
        }

        mskp += stride;
        dmskpp += stride;
        dmskp += stride;
        dmskpn += stride;
        dstp += stride;
    }
}

template<typename T>
static void filterMap(const VSFrameRef * msk, const VSFrameRef * dmsk, VSFrameRef * dst, const int plane, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift = d->vi->format->bitsPerSample - 8;
    const int twleve = 12 << shift;

    const int width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    const T * dmskp = reinterpret_cast<const T *>(vsapi->getReadPtr(dmsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), dmskp, vsapi->getStride(dmsk, plane), width * sizeof(T), height);

    mskp += stride;
    dmskp += stride;
    dstp += stride;

    const T * dmskpp = dmskp - stride;
    const T * dmskpn = dmskp + stride;

    for (unsigned y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (dmskp[x] == peak || mskp[x] != peak)
                continue;

            int dir = (dmskp[x] - neutral) / 4;
            const int lim = std::max(std::abs(dir) * 2, twleve);
            dir >>= shift;
            bool ict = false, icb = false;

            if (dir < 0) {
                for (int j = std::max(-x, dir); j <= 0; j++) {
                    if ((std::abs(dmskpp[x + j] - dmskp[x]) > lim && dmskpp[x + j] != peak) ||
                        (dmskp[x + j] == peak && dmskpp[x + j] == peak) ||
                        (std::abs(dmskp[x + j] - dmskp[x]) > lim && dmskp[x + j] != peak)) {
                        ict = true;
                        break;
                    }
                }
            } else {
                for (int j = 0; j <= std::min(width - x - 1, dir); j++) {
                    if ((std::abs(dmskpp[x + j] - dmskp[x]) > lim && dmskpp[x + j] != peak) ||
                        (dmskp[x + j] == peak && dmskpp[x + j] == peak) ||
                        (std::abs(dmskp[x + j] - dmskp[x]) > lim && dmskp[x + j] != peak)) {
                        ict = true;
                        break;
                    }
                }
            }

            if (ict) {
                if (dir < 0) {
                    for (int j = 0; j <= std::min(width - x - 1, std::abs(dir)); j++) {
                        if ((std::abs(dmskpn[x + j] - dmskp[x]) > lim && dmskpn[x + j] != peak) ||
                            (dmskpn[x + j] == peak && dmskp[x + j] == peak) ||
                            (std::abs(dmskp[x + j] - dmskp[x]) > lim && dmskp[x + j] != peak)) {
                            icb = true;
                            break;
                        }
                    }
                } else {
                    for (int j = std::max(-x, -dir); j <= 0; j++) {
                        if ((std::abs(dmskpn[x + j] - dmskp[x]) > lim && dmskpn[x + j] != peak) ||
                            (dmskpn[x + j] == peak && dmskp[x + j] == peak) ||
                            (std::abs(dmskp[x + j] - dmskp[x]) > lim && dmskp[x + j] != peak)) {
                            icb = true;
                            break;
                        }
                    }
                }

                if (icb)
                    dstp[x] = peak;
            }
        }

        mskp += stride;
        dmskpp += stride;
        dmskp += stride;
        dmskpn += stride;
        dstp += stride;
    }
}

static void upscaleBy2(const VSFrameRef * src, VSFrameRef * dst, const int plane, const unsigned field, const unsigned bytesPerSample, const VSAPI * vsapi) noexcept {
    vs_bitblt(vsapi->getWritePtr(dst, plane) + vsapi->getStride(dst, plane) * (1 - field), vsapi->getStride(dst, plane) * 2,
              vsapi->getReadPtr(src, plane), vsapi->getStride(src, plane), vsapi->getFrameWidth(src, plane) * bytesPerSample, vsapi->getFrameHeight(src, plane));
}

template<typename T>
static void markDirections2X(const VSFrameRef * msk, const VSFrameRef * dmsk, VSFrameRef * dst, const int plane, const unsigned field, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift2 = 2 + (d->vi->format->bitsPerSample - 8);

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    const T * dmskp = reinterpret_cast<const T *>(vsapi->getReadPtr(dmsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    std::fill_n(dstp, stride * height, peak);

    mskp += stride * (1 - field);
    dmskp += stride * (1 - field);
    dstp += stride * (2 - field);

    const T * mskpn = mskp + stride * 2;
    const T * dmskpn = dmskp + stride * 2;

    for (unsigned y = 2 - field; y < height - 1; y += 2) {
        for (unsigned x = 1; x < width - 1; x++) {
            if (mskp[x] != peak && mskpn[x] != peak)
                continue;

            int order[6];
            unsigned v = 0;

            if (dmskp[x - 1] != peak)
                order[v++] = dmskp[x - 1];
            if (dmskp[x] != peak)
                order[v++] = dmskp[x];
            if (dmskp[x + 1] != peak)
                order[v++] = dmskp[x + 1];
            if (dmskpn[x - 1] != peak)
                order[v++] = dmskpn[x - 1];
            if (dmskpn[x] != peak)
                order[v++] = dmskpn[x];
            if (dmskpn[x + 1] != peak)
                order[v++] = dmskpn[x + 1];

            if (v < 3) {
                continue;
            } else {
                std::sort(order, order + v);

                const int mid = (v & 1) ? order[v / 2] : (order[(v - 1) / 2] + order[v / 2] + 1) / 2;
                const int lim = d->limlut2[std::abs(mid - neutral) >> shift2];
                int sum = 0;
                unsigned count = 0;

                unsigned u = 0;
                if (std::abs(dmskp[x - 1] - dmskpn[x - 1]) <= lim || dmskp[x - 1] == peak || dmskpn[x - 1] == peak)
                    u++;
                if (std::abs(dmskp[x] - dmskpn[x]) <= lim || dmskp[x] == peak || dmskpn[x] == peak)
                    u++;
                if (std::abs(dmskp[x + 1] - dmskpn[x - 1]) <= lim || dmskp[x + 1] == peak || dmskpn[x + 1] == peak)
                    u++;
                if (u < 2)
                    continue;

                for (unsigned i = 0; i < v; i++) {
                    if (std::abs(order[i] - mid) <= lim) {
                        sum += order[i];
                        count++;
                    }
                }

                if (count < v - 2 || count < 2)
                    continue;

                dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / (count + 1) + 0.5f);
            }
        }

        mskp += stride * 2;
        mskpn += stride * 2;
        dmskp += stride * 2;
        dmskpn += stride * 2;
        dstp += stride * 2;
    }
}

template<typename T>
static void filterDirMap2X(const VSFrameRef * msk, const VSFrameRef * dmsk, VSFrameRef * dst, const int plane, const unsigned field, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift2 = 2 + (d->vi->format->bitsPerSample - 8);

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    const T * dmskp = reinterpret_cast<const T *>(vsapi->getReadPtr(dmsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), dmskp, vsapi->getStride(dmsk, plane), width * sizeof(T), height);

    mskp += stride * (1 - field);
    dmskp += stride * (2 - field);
    dstp += stride * (2 - field);

    const T * mskpn = mskp + stride * 2;
    const T * dmskpp = dmskp - stride * 2;
    const T * dmskpn = dmskp + stride * 2;

    for (unsigned y = 2 - field; y < height - 1; y += 2) {
        for (unsigned x = 1; x < width - 1; x++) {
            if (mskp[x] != peak && mskpn[x] != peak)
                continue;

            int order[9];
            unsigned u = 0;

            if (y > 1) {
                if (dmskpp[x - 1] != peak)
                    order[u++] = dmskpp[x - 1];
                if (dmskpp[x] != peak)
                    order[u++] = dmskpp[x];
                if (dmskpp[x + 1] != peak)
                    order[u++] = dmskpp[x + 1];
            }

            if (dmskp[x - 1] != peak)
                order[u++] = dmskp[x - 1];
            if (dmskp[x] != peak)
                order[u++] = dmskp[x];
            if (dmskp[x + 1] != peak)
                order[u++] = dmskp[x + 1];

            if (y < height - 2) {
                if (dmskpn[x - 1] != peak)
                    order[u++] = dmskpn[x - 1];
                if (dmskpn[x] != peak)
                    order[u++] = dmskpn[x];
                if (dmskpn[x + 1] != peak)
                    order[u++] = dmskpn[x + 1];
            }

            if (u < 4) {
                dstp[x] = peak;
                continue;
            }

            std::sort(order, order + u);

            const int mid = (u & 1) ? order[u / 2] : (order[(u - 1) / 2] + order[u / 2] + 1) / 2;
            const int lim = d->limlut2[std::abs(mid - neutral) >> shift2];
            int sum = 0;
            unsigned count = 0;

            for (unsigned i = 0; i < u; i++) {
                if (std::abs(order[i] - mid) <= lim) {
                    sum += order[i];
                    count++;
                }
            }

            if (count < 4 || (count < 5 && dmskp[x] == peak)) {
                dstp[x] = peak;
                continue;
            }

            dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / (count + 1) + 0.5f);
        }

        mskp += stride * 2;
        mskpn += stride * 2;
        dmskpp += stride * 2;
        dmskp += stride * 2;
        dmskpn += stride * 2;
        dstp += stride * 2;
    }
}

template<typename T>
static void expandDirMap2X(const VSFrameRef * msk, const VSFrameRef * dmsk, VSFrameRef * dst, const int plane, const unsigned field, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift2 = 2 + (d->vi->format->bitsPerSample - 8);

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    const T * dmskp = reinterpret_cast<const T *>(vsapi->getReadPtr(dmsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), dmskp, vsapi->getStride(dmsk, plane), width * sizeof(T), height);

    mskp += stride * (1 - field);
    dmskp += stride * (2 - field);
    dstp += stride * (2 - field);

    const T * mskpn = mskp + stride * 2;
    const T * dmskpp = dmskp - stride * 2;
    const T * dmskpn = dmskp + stride * 2;

    for (unsigned y = 2 - field; y < height - 1; y += 2) {
        for (unsigned x = 1; x < width - 1; x++) {
            if (dmskp[x] != peak || (mskp[x] != peak && mskpn[x] != peak))
                continue;

            int order[9];
            unsigned u = 0;

            if (y > 1) {
                if (dmskpp[x - 1] != peak)
                    order[u++] = dmskpp[x - 1];
                if (dmskpp[x] != peak)
                    order[u++] = dmskpp[x];
                if (dmskpp[x + 1] != peak)
                    order[u++] = dmskpp[x + 1];
            }

            if (dmskp[x - 1] != peak)
                order[u++] = dmskp[x - 1];
            if (dmskp[x + 1] != peak)
                order[u++] = dmskp[x + 1];

            if (y < height - 2) {
                if (dmskpn[x - 1] != peak)
                    order[u++] = dmskpn[x - 1];
                if (dmskpn[x] != peak)
                    order[u++] = dmskpn[x];
                if (dmskpn[x + 1] != peak)
                    order[u++] = dmskpn[x + 1];
            }

            if (u < 5)
                continue;

            std::sort(order, order + u);

            const int mid = (u & 1) ? order[u / 2] : (order[(u - 1) / 2] + order[u / 2] + 1) / 2;
            const int lim = d->limlut2[std::abs(mid - neutral) >> shift2];
            int sum = 0;
            unsigned count = 0;

            for (unsigned i = 0; i < u; i++) {
                if (std::abs(order[i] - mid) <= lim) {
                    sum += order[i];
                    count++;
                }
            }

            if (count < 5)
                continue;

            dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / (count + 1) + 0.5f);
        }

        mskp += stride * 2;
        mskpn += stride * 2;
        dmskpp += stride * 2;
        dmskp += stride * 2;
        dmskpn += stride * 2;
        dstp += stride * 2;
    }
}

template<typename T>
static void fillGaps2X(const VSFrameRef * msk, const VSFrameRef * dmsk, VSFrameRef * dst, const int plane, const unsigned field, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift = d->vi->format->bitsPerSample - 8;
    const T shift2 = 2 + shift;
    const int eight = 8 << shift;
    const int twenty = 20 << shift;
    const int fiveHundred = 500 << shift;

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    const T * dmskp = reinterpret_cast<const T *>(vsapi->getReadPtr(dmsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    vs_bitblt(dstp, vsapi->getStride(dst, plane), dmskp, vsapi->getStride(dmsk, plane), width * sizeof(T), height);

    mskp += stride * (1 - field);
    dmskp += stride * (2 - field);
    dstp += stride * (2 - field);

    const T * mskpp = mskp - stride * 2;
    const T * mskpn = mskp + stride * 2;
    const T * mskpnn = mskpn + stride * 2;
    const T * dmskpp = dmskp - stride * 2;
    const T * dmskpn = dmskp + stride * 2;

    for (unsigned y = 2 - field; y < height - 1; y += 2) {
        for (unsigned x = 1; x < width - 1; x++) {
            if (dmskp[x] != peak || (mskp[x] != peak && mskpn[x] != peak))
                continue;

            unsigned u = x - 1, v = x + 1;
            int back = fiveHundred, forward = -fiveHundred;

            while (u) {
                if (dmskp[u] != peak) {
                    back = dmskp[u];
                    break;
                }
                if (mskp[u] != peak && mskpn[u] != peak)
                    break;
                u--;
            }

            while (v < width) {
                if (dmskp[v] != peak) {
                    forward = dmskp[v];
                    break;
                }
                if (mskp[v] != peak && mskpn[v] != peak)
                    break;
                v++;
            }

            bool tc = true, bc = true;
            int mint = fiveHundred, maxt = -twenty;
            int minb = fiveHundred, maxb = -twenty;

            for (unsigned j = u; j <= v; j++) {
                if (tc) {
                    if (y <= 2 || dmskpp[j] == peak || (mskpp[j] != peak && mskp[j] != peak)) {
                        tc = false;
                        mint = maxt = twenty;
                    } else {
                        if (dmskpp[j] < mint)
                            mint = dmskpp[j];
                        if (dmskpp[j] > maxt)
                            maxt = dmskpp[j];
                    }
                }

                if (bc) {
                    if (y >= height - 3 || dmskpn[j] == peak || (mskpn[j] != peak && mskpnn[j] != peak)) {
                        bc = false;
                        minb = maxb = twenty;
                    } else {
                        if (dmskpn[j] < minb)
                            minb = dmskpn[j];
                        if (dmskpn[j] > maxb)
                            maxb = dmskpn[j];
                    }
                }
            }

            if (maxt == -twenty)
                maxt = mint = twenty;
            if (maxb == -twenty)
                maxb = minb = twenty;

            const int thresh = std::max({ std::max(std::abs(forward - neutral), std::abs(back - neutral)) / 4, eight, std::abs(mint - maxt), std::abs(minb - maxb) });
            const unsigned lim = std::min(std::max(std::abs(forward - neutral), std::abs(back - neutral)) >> shift2, 6);
            if (std::abs(forward - back) <= thresh && (v - u - 1 <= lim || tc || bc)) {
                const float step = static_cast<float>(forward - back) / (v - u);
                for (unsigned j = 0; j < v - u - 1; j++)
                    dstp[u + j + 1] = back + static_cast<int>(j * step + 0.5);
            }
        }

        mskpp += stride * 2;
        mskp += stride * 2;
        mskpn += stride * 2;
        mskpnn += stride * 2;
        dmskpp += stride * 2;
        dmskp += stride * 2;
        dmskpn += stride * 2;
        dstp += stride * 2;
    }
}

template<typename T>
static void interpolateLattice(const VSFrameRef * omsk, VSFrameRef * dmsk, VSFrameRef * dst, const int plane, const unsigned field, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift = d->vi->format->bitsPerSample - 8;
    const T shift2 = 2 + shift;
    const T three = 3 << shift;
    const T nine = 9 << shift;

    const int width = vsapi->getFrameWidth(omsk, plane);
    const unsigned height = vsapi->getFrameHeight(omsk, plane);
    const unsigned stride = vsapi->getStride(omsk, plane) / sizeof(T);
    const T * omskp = reinterpret_cast<const T *>(vsapi->getReadPtr(omsk, plane));
    T * VS_RESTRICT dmskp = reinterpret_cast<T *>(vsapi->getWritePtr(dmsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    if (field)
        std::copy_n(dstp + stride * (height - 2), width, dstp + stride * (height - 1));
    else
        std::copy_n(dstp + stride, width, dstp);

    omskp += stride * (1 - field);
    dmskp += stride * (2 - field);
    dstp += stride * (1 - field);

    const T * omskn = omskp + stride * 2;
    T * VS_RESTRICT dstpn = dstp + stride;
    const T * dstpnn = dstp + stride * 2;

    for (unsigned y = 2 - field; y < height - 1; y += 2) {
        for (int x = 0; x < width; x++) {
            int dir = dmskp[x];
            const int lim = d->limlut2[std::abs(dir - neutral) >> shift2];

            if (dir == peak || (std::abs(dmskp[x] - dmskp[x - 1]) > lim && std::abs(dmskp[x] - dmskp[x + 1]) > lim)) {
                dstpn[x] = (dstp[x] + dstpnn[x] + 1) / 2;
                if (dir != peak)
                    dmskp[x] = neutral;
                continue;
            }

            if (lim < nine) {
                const unsigned sum = (dstp[x - 1] + dstp[x] + dstp[x + 1] +
                                      dstpnn[x - 1] + dstpnn[x] + dstpnn[x + 1]) >> shift;
                const unsigned sumsq = (dstp[x - 1] >> shift) * (dstp[x - 1] >> shift) + (dstp[x] >> shift) * (dstp[x] >> shift) + (dstp[x + 1] >> shift) * (dstp[x + 1] >> shift) +
                                       (dstpnn[x - 1] >> shift) * (dstpnn[x - 1] >> shift) + (dstpnn[x] >> shift) * (dstpnn[x] >> shift) + (dstpnn[x + 1] >> shift) * (dstpnn[x + 1] >> shift);
                if (6 * sumsq - sum * sum < 576) {
                    dstpn[x] = (dstp[x] + dstpnn[x] + 1) / 2;
                    dmskp[x] = peak;
                    continue;
                }
            }

            if (x > 1 && x < width - 2 &&
                ((dstp[x] < std::max(dstp[x - 2], dstp[x - 1]) - three && dstp[x] < std::max(dstp[x + 2], dstp[x + 1]) - three &&
                  dstpnn[x] < std::max(dstpnn[x - 2], dstpnn[x - 1]) - three && dstpnn[x] < std::max(dstpnn[x + 2], dstpnn[x + 1]) - three) ||
                 (dstp[x] > std::min(dstp[x - 2], dstp[x - 1]) + three && dstp[x] > std::min(dstp[x + 2], dstp[x + 1]) + three &&
                  dstpnn[x]  > std::min(dstpnn[x - 2], dstpnn[x - 1]) + three && dstpnn[x] > std::min(dstpnn[x + 2], dstpnn[x + 1]) + three))) {
                dstpn[x] = (dstp[x] + dstpnn[x] + 1) / 2;
                dmskp[x] = neutral;
                continue;
            }

            dir = (dir - neutral + (1 << (shift2 - 1))) >> shift2;
            const int uStart = (dir - 2 < 0) ? std::max({ -x + 1, dir - 2, -width + 2 + x }) : std::min({ x - 1, dir - 2, width - 2 - x });
            const int uStop = (dir + 2 < 0) ? std::max({ -x + 1, dir + 2, -width + 2 + x }) : std::min({ x - 1, dir + 2, width - 2 - x });
            unsigned min = d->nt8;
            unsigned val = (dstp[x] + dstpnn[x] + 1) / 2;

            for (int u = uStart; u <= uStop; u++) {
                const unsigned diff = std::abs(dstp[x - 1] - dstpnn[x - u - 1]) + std::abs(dstp[x] - dstpnn[x - u]) + std::abs(dstp[x + 1] - dstpnn[x - u + 1]) +
                                      std::abs(dstpnn[x - 1] - dstp[x + u - 1]) + std::abs(dstpnn[x] - dstp[x + u]) + std::abs(dstpnn[x + 1] - dstp[x + u + 1]);
                if (diff < min &&
                    ((omskp[x - 1 + u] != peak && std::abs(omskp[x - 1 + u] - dmskp[x]) <= lim) ||
                     (omskp[x + u] != peak && std::abs(omskp[x + u] - dmskp[x]) <= lim) ||
                     (omskp[x + 1 + u] != peak && std::abs(omskp[x + 1 + u] - dmskp[x]) <= lim)) &&
                    ((omskn[x - 1 - u] != peak && std::abs(omskn[x - 1 - u] - dmskp[x]) <= lim) ||
                     (omskn[x - u] != peak && std::abs(omskn[x - u] - dmskp[x]) <= lim) ||
                     (omskn[x + 1 - u] != peak && std::abs(omskn[x + 1 - u] - dmskp[x]) <= lim))) {
                    const unsigned diff2 = std::abs(dstp[x + u / 2 - 1] - dstpnn[x - u / 2 - 1]) +
                                           std::abs(dstp[x + u / 2] - dstpnn[x - u / 2]) +
                                           std::abs(dstp[x + u / 2 + 1] - dstpnn[x - u / 2 + 1]);
                    if (diff2 < d->nt4 &&
                        (((std::abs(omskp[x + u / 2] - omskn[x - u / 2]) <= lim ||
                           std::abs(omskp[x + u / 2] - omskn[x - ((u + 1) / 2)]) <= lim) &&
                          omskp[x + u / 2] != peak) ||
                         ((std::abs(omskp[x + ((u + 1) / 2)] - omskn[x - u / 2]) <= lim ||
                           std::abs(omskp[x + ((u + 1) / 2)] - omskn[x - ((u + 1) / 2)]) <= lim) &&
                          omskp[x + ((u + 1) / 2)] != peak))) {
                        if ((std::abs(dmskp[x] - omskp[x + u / 2]) <= lim || std::abs(dmskp[x] - omskp[x + ((u + 1) / 2)]) <= lim) &&
                            (std::abs(dmskp[x] - omskn[x - u / 2]) <= lim || std::abs(dmskp[x] - omskn[x - ((u + 1) / 2)]) <= lim)) {
                            val = (dstp[x + u / 2] + dstp[x + ((u + 1) / 2)] +
                                   dstpnn[x - u / 2] + dstpnn[x - ((u + 1) / 2)] + 2) / 4;
                            min = diff;
                            dir = u;
                        }
                    }
                }
            }

            if (min != d->nt8) {
                dstpn[x] = val;
                dmskp[x] = neutral + (dir << shift2);
            } else {
                const int dt = 4 >> (plane ? d->vi->format->subSamplingW : 0);
                const int uStart2 = std::max(-x + 1, -dt);
                const int uStop2 = std::min(width - 2 - x, dt);
                const unsigned minm = std::min(dstp[x], dstpnn[x]);
                const unsigned maxm = std::max(dstp[x], dstpnn[x]);
                min = d->nt7;

                for (int u = uStart2; u <= uStop2; u++) {
                    const int p1 = dstp[x + u / 2] + dstp[x + ((u + 1) / 2)];
                    const int p2 = dstpnn[x - u / 2] + dstpnn[x - ((u + 1) / 2)];
                    const unsigned diff = std::abs(dstp[x - 1] - dstpnn[x - u - 1]) + std::abs(dstp[x] - dstpnn[x - u]) + std::abs(dstp[x + 1] - dstpnn[x - u + 1]) +
                                          std::abs(dstpnn[x - 1] - dstp[x + u - 1]) + std::abs(dstpnn[x] - dstp[x + u]) + std::abs(dstpnn[x + 1] - dstp[x + u + 1]) +
                                          std::abs(p1 - p2);
                    if (diff < min) {
                        const unsigned valt = (p1 + p2 + 2) / 4;
                        if (valt >= minm && valt <= maxm) {
                            val = valt;
                            min = diff;
                            dir = u;
                        }
                    }
                }

                dstpn[x] = val;
                dmskp[x] = (min == d->nt7) ? neutral : neutral + (dir << shift2);
            }
        }

        omskp += stride * 2;
        omskn += stride * 2;
        dmskp += stride * 2;
        dstp += stride * 2;
        dstpn += stride * 2;
        dstpnn += stride * 2;
    }
}

template<typename T>
static void postProcess(const VSFrameRef * nmsk, const VSFrameRef * omsk, VSFrameRef * dst, const int plane, const unsigned field, const EEDI2Data * d, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (d->vi->format->bitsPerSample - 1);
    const T peak = (1 << d->vi->format->bitsPerSample) - 1;
    const T shift2 = 2 + (d->vi->format->bitsPerSample - 8);

    const unsigned width = vsapi->getFrameWidth(nmsk, plane);
    const unsigned height = vsapi->getFrameHeight(nmsk, plane);
    const unsigned stride = vsapi->getStride(nmsk, plane) / sizeof(T);
    const T * nmskp = reinterpret_cast<const T *>(vsapi->getReadPtr(nmsk, plane));
    const T * omskp = reinterpret_cast<const T *>(vsapi->getReadPtr(omsk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    nmskp += stride * (2 - field);
    omskp += stride * (2 - field);
    dstp += stride * (2 - field);

    const T * dstpp = dstp - stride;
    const T * dstpn = dstp + stride;

    for (unsigned y = 2 - field; y < height - 1; y += 2) {
        for (unsigned x = 0; x < width; x++) {
            const int lim = d->limlut2[std::abs(nmskp[x] - neutral) >> shift2];
            if (std::abs(nmskp[x] - omskp[x]) > lim && omskp[x] != peak && omskp[x] != neutral)
                dstp[x] = (dstpp[x] + dstpn[x] + 1) / 2;
        }

        nmskp += stride * 2;
        omskp += stride * 2;
        dstpp += stride * 2;
        dstp += stride * 2;
        dstpn += stride * 2;
    }
}

template<typename T>
static void gaussianBlur1(const VSFrameRef * src, VSFrameRef * tmp, VSFrameRef * dst, const int plane, const VSAPI * vsapi) noexcept {
    const unsigned width = vsapi->getFrameWidth(src, plane);
    const unsigned height = vsapi->getFrameHeight(src, plane);
    const unsigned stride = vsapi->getStride(src, plane) / sizeof(T);
    const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(tmp, plane));

    for (unsigned y = 0; y < height; y++) {
        dstp[0] = (srcp[3] * 582u + srcp[2] * 7078u + srcp[1] * 31724u + srcp[0] * 26152u + 32768u) / 65536u;
        dstp[1] = (srcp[4] * 582u + srcp[3] * 7078u + (srcp[0] + srcp[2]) * 15862u + srcp[1] * 26152u + 32768u) / 65536u;
        dstp[2] = (srcp[5] * 582u + (srcp[0] + srcp[4]) * 3539u + (srcp[1] + srcp[3]) * 15862u + srcp[2] * 26152u + 32768u) / 65536u;
        unsigned x;
        for (x = 3; x < width - 3; x++)
            dstp[x] = ((srcp[x - 3] + srcp[x + 3]) * 291u + (srcp[x - 2] + srcp[x + 2]) * 3539u + (srcp[x - 1] + srcp[x + 1]) * 15862u + srcp[x] * 26152u + 32768u) / 65536u;
        dstp[x] = (srcp[x - 3] * 582u + (srcp[x - 2] + srcp[x + 2]) * 3539u + (srcp[x - 1] + srcp[x + 1]) * 15862u + srcp[x] * 26152u + 32768u) / 65536u; x++;
        dstp[x] = (srcp[x - 3] * 582u + srcp[x - 2] * 7078u + (srcp[x - 1] + srcp[x + 1]) * 15862u + srcp[x] * 26152u + 32768u) / 65536u; x++;
        dstp[x] = (srcp[x - 3] * 582u + srcp[x - 2] * 7078u + srcp[x - 1] * 31724u + srcp[x] * 26152u + 32768u) / 65536u;

        srcp += stride;
        dstp += stride;
    }

    srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(tmp, plane));
    dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
    const T * src3p = srcp - stride * 3;
    const T * src2p = srcp - stride * 2;
    const T * srcpp = srcp - stride;
    const T * srcpn = srcp + stride;
    const T * src2n = srcp + stride * 2;
    const T * src3n = srcp + stride * 3;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src3n[x] * 582u + src2n[x] * 7078u + srcpn[x] * 31724u + srcp[x] * 26152u + 32768u) / 65536u;

    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src3n[x] * 582u + src2n[x] * 7078u + (srcpp[x] + srcpn[x]) * 15862u + srcp[x] * 26152u + 32768u) / 65536u;

    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src3n[x] * 582u + (src2p[x] + src2n[x]) * 3539u + (srcpp[x] + srcpn[x]) * 15862u + srcp[x] * 26152u + 32768u) / 65536u;

    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;

    for (unsigned y = 3; y < height - 3; y++) {
        for (unsigned x = 0; x < width; x++)
            dstp[x] = ((src3p[x] + src3n[x]) * 291u + (src2p[x] + src2n[x]) * 3539u + (srcpp[x] + srcpn[x]) * 15862u + srcp[x] * 26152u + 32768u) / 65536u;

        src3p += stride;
        src2p += stride;
        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        src2n += stride;
        src3n += stride;
        dstp += stride;
    }

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src3p[x] * 582u + (src2p[x] + src2n[x]) * 3539u + (srcpp[x] + srcpn[x]) * 15862u + srcp[x] * 26152u + 32768u) / 65536u;

    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src3p[x] * 582u + src2p[x] * 7078u + (srcpp[x] + srcpn[x]) * 15862u + srcp[x] * 26152u + 32768u) / 65536u;

    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src3p[x] * 582u + src2p[x] * 7078u + srcpp[x] * 31724u + srcp[x] * 26152u + 32768u) / 65536u;
}

template<typename T>
static void calcDerivatives(const VSFrameRef * src, int * VS_RESTRICT x2, int * VS_RESTRICT y2, int * VS_RESTRICT xy,
                            const int plane, const unsigned bitsPerSample, const VSAPI * vsapi) noexcept {
    const T shift = bitsPerSample - 8;

    const unsigned width = vsapi->getFrameWidth(src, plane);
    const unsigned height = vsapi->getFrameHeight(src, plane);
    const unsigned stride = vsapi->getStride(src, plane) / sizeof(T);
    const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
    const T * srcpp = srcp - stride;
    const T * srcpn = srcp + stride;

    {
        const int Ix = (srcp[1] - srcp[0]) >> shift;
        const int Iy = (srcp[0] - srcpn[0]) >> shift;
        x2[0] = (Ix * Ix) / 2;
        y2[0] = (Iy * Iy) / 2;
        xy[0] = (Ix * Iy) / 2;
    }

    unsigned x;

    for (x = 1; x < width - 1; x++) {
        const int Ix = (srcp[x + 1] - srcp[x - 1]) >> shift;
        const int Iy = (srcp[x] - srcpn[x]) >> shift;
        x2[x] = (Ix * Ix) / 2;
        y2[x] = (Iy * Iy) / 2;
        xy[x] = (Ix * Iy) / 2;
    }

    {
        const int Ix = (srcp[x] - srcp[x - 1]) >> shift;
        const int Iy = (srcp[x] - srcpn[x]) >> shift;
        x2[x] = (Ix * Ix) / 2;
        y2[x] = (Iy * Iy) / 2;
        xy[x] = (Ix * Iy) / 2;
    }

    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    x2 += width;
    y2 += width;
    xy += width;

    for (unsigned y = 1; y < height - 1; y++) {
        {
            const int Ix = (srcp[1] - srcp[0]) >> shift;
            const int Iy = (srcpp[0] - srcpn[0]) >> shift;
            x2[0] = (Ix * Ix) / 2;
            y2[0] = (Iy * Iy) / 2;
            xy[0] = (Ix * Iy) / 2;
        }

        for (x = 1; x < width - 1; x++) {
            const int Ix = (srcp[x + 1] - srcp[x - 1]) >> shift;
            const int Iy = (srcpp[x] - srcpn[x]) >> shift;
            x2[x] = (Ix * Ix) / 2;
            y2[x] = (Iy * Iy) / 2;
            xy[x] = (Ix * Iy) / 2;
        }

        {
            const int Ix = (srcp[x] - srcp[x - 1]) >> shift;
            const int Iy = (srcpp[x] - srcpn[x]) >> shift;
            x2[x] = (Ix * Ix) / 2;
            y2[x] = (Iy * Iy) / 2;
            xy[x] = (Ix * Iy) / 2;
        }

        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        x2 += width;
        y2 += width;
        xy += width;
    }

    {
        const int Ix = (srcp[1] - srcp[0]) >> shift;
        const int Iy = (srcpp[0] - srcp[0]) >> shift;
        x2[0] = (Ix * Ix) / 2;
        y2[0] = (Iy * Iy) / 2;
        xy[0] = (Ix * Iy) / 2;
    }

    for (x = 1; x < width - 1; x++) {
        const int Ix = (srcp[x + 1] - srcp[x - 1]) >> shift;
        const int Iy = (srcpp[x] - srcp[x]) >> shift;
        x2[x] = (Ix * Ix) / 2;
        y2[x] = (Iy * Iy) / 2;
        xy[x] = (Ix * Iy) / 2;
    }

    {
        const int Ix = (srcp[x] - srcp[x - 1]) >> shift;
        const int Iy = (srcpp[x] - srcp[x]) >> shift;
        x2[x] = (Ix * Ix) / 2;
        y2[x] = (Iy * Iy) / 2;
        xy[x] = (Ix * Iy) / 2;
    }
}

static void gaussianBlurSqrt2(const int * src, int * tmp, int * dst, const unsigned width, const unsigned height) noexcept {
    const int * srcp = src;
    int * VS_RESTRICT dstp = tmp;

    for (unsigned y = 0; y < height; y++) {
        unsigned x = 0;
        dstp[x] = (srcp[x + 4] * 678 + srcp[x + 3] * 3902 + srcp[x + 2] * 13618 + srcp[x + 1] * 28830 + srcp[x] * 18508 + 32768) / 65536; x++;
        dstp[x] = (srcp[x + 4] * 678 + srcp[x + 3] * 3902 + srcp[x + 2] * 13618 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) / 65536; x++;
        dstp[x] = (srcp[x + 4] * 678 + srcp[x + 3] * 3902 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) / 65536; x++;
        dstp[x] = (srcp[x + 4] * 678 + (srcp[x - 3] + srcp[x + 3]) * 1951 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) / 65536;
        for (x = 4; x < width - 4; x++)
            dstp[x] = ((srcp[x - 4] + srcp[x + 4]) * 339 + (srcp[x - 3] + srcp[x + 3]) * 1951 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) / 65536;
        dstp[x] = (srcp[x - 4] * 678 + (srcp[x - 3] + srcp[x + 3]) * 1951 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) / 65536; x++;
        dstp[x] = (srcp[x - 4] * 678 + srcp[x - 3] * 3902 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) / 65536; x++;
        dstp[x] = (srcp[x - 4] * 678 + srcp[x - 3] * 3902 + srcp[x - 2] * 13618 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) / 65536; x++;
        dstp[x] = (srcp[x - 4] * 678 + srcp[x - 3] * 3902 + srcp[x - 2] * 13618 + srcp[x - 1] * 28830 + srcp[x] * 18508 + 32768) / 65536;

        srcp += width;
        dstp += width;
    }

    srcp = tmp;
    dstp = dst;
    const int * src4p = srcp - width * 4;
    const int * src3p = srcp - width * 3;
    const int * src2p = srcp - width * 2;
    const int * srcpp = srcp - width;
    const int * srcpn = srcp + width;
    const int * src2n = srcp + width * 2;
    const int * src3n = srcp + width * 3;
    const int * src4n = srcp + width * 4;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src4n[x] * 678 + src3n[x] * 3902 + src2n[x] * 13618 + srcpn[x] * 28830 + srcp[x] * 18508 + 32768) / 65536;

    src4p += width;
    src3p += width;
    src2p += width;
    srcpp += width;
    srcp += width;
    srcpn += width;
    src2n += width;
    src3n += width;
    src4n += width;
    dstp += width;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src4n[x] * 678 + src3n[x] * 3902 + src2n[x] * 13618 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) / 65536;

    src4p += width;
    src3p += width;
    src2p += width;
    srcpp += width;
    srcp += width;
    srcpn += width;
    src2n += width;
    src3n += width;
    src4n += width;
    dstp += width;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src4n[x] * 678 + src3n[x] * 3902 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) / 65536;

    src4p += width;
    src3p += width;
    src2p += width;
    srcpp += width;
    srcp += width;
    srcpn += width;
    src2n += width;
    src3n += width;
    src4n += width;
    dstp += width;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src4n[x] * 678 + (src3p[x] + src3n[x]) * 1951 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) / 65536;

    src4p += width;
    src3p += width;
    src2p += width;
    srcpp += width;
    srcp += width;
    srcpn += width;
    src2n += width;
    src3n += width;
    src4n += width;
    dstp += width;

    for (unsigned y = 4; y < height - 4; y++) {
        for (unsigned x = 0; x < width; x++)
            dstp[x] = ((src4p[x] + src4n[x]) * 339 + (src3p[x] + src3n[x]) * 1951 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) / 65536;

        src4p += width;
        src3p += width;
        src2p += width;
        srcpp += width;
        srcp += width;
        srcpn += width;
        src2n += width;
        src3n += width;
        src4n += width;
        dstp += width;
    }

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src4p[x] * 678 + (src3p[x] + src3n[x]) * 1951 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) / 65536;

    src4p += width;
    src3p += width;
    src2p += width;
    srcpp += width;
    srcp += width;
    srcpn += width;
    src2n += width;
    src3n += width;
    src4n += width;
    dstp += width;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src4p[x] * 678 + src3p[x] * 3902 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) / 65536;

    src4p += width;
    src3p += width;
    src2p += width;
    srcpp += width;
    srcp += width;
    srcpn += width;
    src2n += width;
    src3n += width;
    src4n += width;
    dstp += width;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src4p[x] * 678 + src3p[x] * 3902 + src2p[x] * 13618 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) / 65536;

    src4p += width;
    src3p += width;
    src2p += width;
    srcpp += width;
    srcp += width;
    srcpn += width;
    src2n += width;
    src3n += width;
    src4n += width;
    dstp += width;

    for (unsigned x = 0; x < width; x++)
        dstp[x] = (src4p[x] * 678 + src3p[x] * 3902 + src2p[x] * 13618 + srcpp[x] * 28830 + srcp[x] * 18508 + 32768) / 65536;
}

template<typename T>
static void postProcessCorner(const VSFrameRef * msk, VSFrameRef * dst, const int * x2, const int * y2, const int * xy,
                              const int plane, const unsigned field, const unsigned bitsPerSample, const VSAPI * vsapi) noexcept {
    const T neutral = 1 << (bitsPerSample - 1);
    const T peak = (1 << bitsPerSample) - 1;

    const unsigned width = vsapi->getFrameWidth(msk, plane);
    const unsigned height = vsapi->getFrameHeight(msk, plane);
    const unsigned stride = vsapi->getStride(msk, plane) / sizeof(T);
    const T * mskp = reinterpret_cast<const T *>(vsapi->getReadPtr(msk, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

    mskp += stride * (8 - field);
    dstp += stride * (8 - field);
    x2 += width * 3;
    y2 += width * 3;
    xy += width * 3;

    const T * dstpp = dstp - stride;
    const T * dstpn = dstp + stride;
    const int * x2n = x2 + width;
    const int * y2n = y2 + width;
    const int * xyn = xy + width;

    for (unsigned y = 8 - field; y < height - 7; y += 2) {
        for (unsigned x = 4; x < width - 4; x++) {
            if (mskp[x] == peak || mskp[x] == neutral)
                continue;

            const int c1 = static_cast<int>(x2[x] * y2[x] - xy[x] * xy[x] - 0.09f * (x2[x] + y2[x]) * (x2[x] + y2[x]));
            const int c2 = static_cast<int>(x2n[x] * y2n[x] - xyn[x] * xyn[x] - 0.09f * (x2n[x] + y2n[x]) * (x2n[x] + y2n[x]));
            if (c1 > 775 || c2 > 775)
                dstp[x] = (dstpp[x] + dstpn[x] + 1) / 2;
        }

        mskp += stride * 2;
        dstpp += stride * 2;
        dstp += stride * 2;
        dstpn += stride * 2;
        x2 += width;
        x2n += width;
        y2 += width;
        y2n += width;
        xy += width;
        xyn += width;
    }
}

template<typename T>
static void process(const VSFrameRef * src, VSFrameRef * dst, VSFrameRef * msk, VSFrameRef * tmp,
                    VSFrameRef * dst2, VSFrameRef * dst2M, VSFrameRef * tmp2, VSFrameRef * tmp2_2, VSFrameRef * msk2,
                    const unsigned field, const EEDI2Data * d, VSCore * core, const VSAPI * vsapi) noexcept {
    const auto threadId = std::this_thread::get_id();
    int * cx2 = d->cx2.at(threadId);
    int * cy2 = d->cy2.at(threadId);
    int * cxy = d->cxy.at(threadId);
    int * tmpc = d->tmpc.at(threadId);

    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        buildEdgeMask<T>(src, msk, plane, d, vsapi);
        erode<T>(msk, tmp, plane, d, vsapi);
        dilate<T>(tmp, msk, plane, d, vsapi);
        erode<T>(msk, tmp, plane, d, vsapi);
        removeSmallHorzGaps<T>(tmp, msk, plane, d, vsapi);

        if (d->map != 1) {
            calcDirections<T>(src, msk, tmp, plane, d, vsapi);
            filterDirMap<T>(msk, tmp, dst, plane, d, vsapi);
            expandDirMap<T>(msk, dst, tmp, plane, d, vsapi);
            filterMap<T>(msk, tmp, dst, plane, d, vsapi);

            if (d->map != 2) {
                memset(vsapi->getWritePtr(dst2, plane), 0, vsapi->getStride(dst2, plane) * vsapi->getFrameHeight(dst2, plane));

                upscaleBy2(src, dst2, plane, field, sizeof(T), vsapi);
                upscaleBy2(dst, tmp2_2, plane, field, sizeof(T), vsapi);
                upscaleBy2(msk, msk2, plane, field, sizeof(T), vsapi);
                markDirections2X<T>(msk2, tmp2_2, tmp2, plane, field, d, vsapi);
                filterDirMap2X<T>(msk2, tmp2, dst2M, plane, field, d, vsapi);
                expandDirMap2X<T>(msk2, dst2M, tmp2, plane, field, d, vsapi);
                fillGaps2X<T>(msk2, tmp2, dst2M, plane, field, d, vsapi);
                fillGaps2X<T>(msk2, dst2M, tmp2, plane, field, d, vsapi);

                if (d->map != 3) {
                    interpolateLattice<T>(tmp2_2, tmp2, dst2, plane, field, d, vsapi);

                    if (d->pp == 1 || d->pp == 3) {
                        vs_bitblt(vsapi->getWritePtr(tmp2_2, plane), vsapi->getStride(tmp2_2, plane), vsapi->getReadPtr(tmp2, plane), vsapi->getStride(tmp2, plane),
                                  vsapi->getFrameWidth(tmp2, plane) * sizeof(T), vsapi->getFrameHeight(tmp2, plane));

                        filterDirMap2X<T>(msk2, tmp2, dst2M, plane, field, d, vsapi);
                        expandDirMap2X<T>(msk2, dst2M, tmp2, plane, field, d, vsapi);
                        postProcess<T>(tmp2, tmp2_2, dst2, plane, field, d, vsapi);
                    }

                    if (d->pp == 2 || d->pp == 3) {
                        gaussianBlur1<T>(src, tmp, dst, plane, vsapi);
                        calcDerivatives<T>(dst, cx2, cy2, cxy, plane, d->vi->format->bitsPerSample, vsapi);
                        gaussianBlurSqrt2(cx2, tmpc, cx2, vsapi->getFrameWidth(src, plane), vsapi->getFrameHeight(src, plane));
                        gaussianBlurSqrt2(cy2, tmpc, cy2, vsapi->getFrameWidth(src, plane), vsapi->getFrameHeight(src, plane));
                        gaussianBlurSqrt2(cxy, tmpc, cxy, vsapi->getFrameWidth(src, plane), vsapi->getFrameHeight(src, plane));
                        postProcessCorner<T>(tmp2_2, dst2, cx2, cy2, cxy, plane, field, d->vi->format->bitsPerSample, vsapi);
                    }
                }
            }
        }
    }
}

static void VS_CC eedi2Init(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    EEDI2Data * d = static_cast<EEDI2Data *>(*instanceData);
    vsapi->setVideoInfo(&d->vi2, 1, node);
}

static const VSFrameRef *VS_CC eedi2GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    EEDI2Data * d = static_cast<EEDI2Data *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        try {
            auto threadId = std::this_thread::get_id();

            if (!d->cx2.count(threadId)) {
                if (d->pp > 1 && d->map == 0) {
                    int * cx2 = new (std::nothrow) int[d->vi->width * d->vi->height];
                    if (!cx2)
                        throw std::string{ "malloc failure (cx2)" };
                    d->cx2.emplace(threadId, cx2);
                } else {
                    d->cx2.emplace(threadId, nullptr);
                }
            }

            if (!d->cy2.count(threadId)) {
                if (d->pp > 1 && d->map == 0) {
                    int * cy2 = new (std::nothrow) int[d->vi->width * d->vi->height];
                    if (!cy2)
                        throw std::string{ "malloc failure (cy2)" };
                    d->cy2.emplace(threadId, cy2);
                } else {
                    d->cy2.emplace(threadId, nullptr);
                }
            }

            if (!d->cxy.count(threadId)) {
                if (d->pp > 1 && d->map == 0) {
                    int * cxy = new (std::nothrow) int[d->vi->width * d->vi->height];
                    if (!cxy)
                        throw std::string{ "malloc failure (cxy)" };
                    d->cxy.emplace(threadId, cxy);
                } else {
                    d->cxy.emplace(threadId, nullptr);
                }
            }

            if (!d->tmpc.count(threadId)) {
                if (d->pp > 1 && d->map == 0) {
                    int * tmpc = new (std::nothrow) int[d->vi->width * d->vi->height];
                    if (!tmpc)
                        throw std::string{ "malloc failure (tmpc)" };
                    d->tmpc.emplace(threadId, tmpc);
                } else {
                    d->tmpc.emplace(threadId, nullptr);
                }
            }
        } catch (const std::string & error) {
            vsapi->setFilterError(("EEDI2: " + error).c_str(), frameCtx);
            return nullptr;
        }

        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, src, core);
        VSFrameRef * msk = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, src, core);
        VSFrameRef * tmp = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, src, core);

        VSFrameRef * dst2 = nullptr, * dst2M = nullptr, * tmp2 = nullptr, * tmp2_2 = nullptr, * msk2 = nullptr;
        if (d->map == 0 || d->map == 3) {
            dst2 = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
            dst2M = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
            tmp2 = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
            tmp2_2 = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
            msk2 = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
        }

        unsigned field = d->field;
        if (d->fieldS > 1)
            field = (n & 1) ? (d->fieldS == 2 ? 1 : 0) : (d->fieldS == 2 ? 0 : 1);

        if (d->vi->format->bytesPerSample == 1)
            process<uint8_t>(src, dst, msk, tmp, dst2, dst2M, tmp2, tmp2_2, msk2, field, d, core, vsapi);
        else
            process<uint16_t>(src, dst, msk, tmp, dst2, dst2M, tmp2, tmp2_2, msk2, field, d, core, vsapi);

        if (d->map == 0) {
            vsapi->freeFrame(dst);
            dst = const_cast<VSFrameRef *>(vsapi->cloneFrameRef(dst2));
        } else if (d->map == 1) {
            vsapi->freeFrame(dst);
            dst = const_cast<VSFrameRef *>(vsapi->cloneFrameRef(msk));
        } else if (d->map == 3) {
            vsapi->freeFrame(dst);
            dst = const_cast<VSFrameRef *>(vsapi->cloneFrameRef(tmp2));
        }

        vsapi->freeFrame(src);
        vsapi->freeFrame(msk);
        vsapi->freeFrame(tmp);
        vsapi->freeFrame(dst2);
        vsapi->freeFrame(dst2M);
        vsapi->freeFrame(tmp2);
        vsapi->freeFrame(tmp2_2);
        vsapi->freeFrame(msk2);
        return dst;
    }

    return nullptr;
}

static void VS_CC eedi2Free(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    EEDI2Data * d = static_cast<EEDI2Data *>(instanceData);

    vsapi->freeNode(d->node);

    delete[] d->limlut;
    delete[] d->limlut2;

    for (auto & element : d->cx2)
        delete[] element.second;
    for (auto & element : d->cy2)
        delete[] element.second;
    for (auto & element : d->cxy)
        delete[] element.second;
    for (auto & element : d->tmpc)
        delete[] element.second;

    delete d;
}

static void VS_CC eedi2Create(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<EEDI2Data> d{ new EEDI2Data{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);
    d->vi2 = *d->vi;

    try {
        if (!isConstantFormat(d->vi) || d->vi->format->sampleType != stInteger || d->vi->format->bitsPerSample > 16)
            throw std::string{ "only constant format 8-16 bits integer input supported" };

        if (d->vi->width < 8)
            throw std::string{ "the clip's width must be greater than or equal to 8" };

        if (d->vi->height < 7)
            throw std::string{ "the clip's height must be greater than or equal to 7" };

        d->field = int64ToIntS(vsapi->propGetInt(in, "field", 0, nullptr));

        d->mthresh = int64ToIntS(vsapi->propGetInt(in, "mthresh", 0, &err));
        if (err)
            d->mthresh = 10;

        d->lthresh = int64ToIntS(vsapi->propGetInt(in, "lthresh", 0, &err));
        if (err)
            d->lthresh = 20;

        d->vthresh = int64ToIntS(vsapi->propGetInt(in, "vthresh", 0, &err));
        if (err)
            d->vthresh = 20;

        d->estr = int64ToIntS(vsapi->propGetInt(in, "estr", 0, &err));
        if (err)
            d->estr = 2;

        d->dstr = int64ToIntS(vsapi->propGetInt(in, "dstr", 0, &err));
        if (err)
            d->dstr = 4;

        d->maxd = int64ToIntS(vsapi->propGetInt(in, "maxd", 0, &err));
        if (err)
            d->maxd = 24;

        d->map = int64ToIntS(vsapi->propGetInt(in, "map", 0, &err));

        int nt = int64ToIntS(vsapi->propGetInt(in, "nt", 0, &err));
        if (err)
            nt = 50;

        d->pp = int64ToIntS(vsapi->propGetInt(in, "pp", 0, &err));
        if (err)
            d->pp = 1;

        if (d->field < 0 || d->field > 3)
            throw std::string{ "field must be 0, 1, 2 or 3" };

        if (d->mthresh < 0)
            throw std::string{ "mthresh must be greater than or equal to 0" };

        if (d->lthresh < 0)
            throw std::string{ "lthresh must be greater than or equal to 0" };

        if (d->vthresh < 0)
            throw std::string{ "vthresh must be greater than or equal to 0" };

        if (d->estr < 0)
            throw std::string{ "estr must be greater than or equal to 0" };

        if (d->dstr < 0)
            throw std::string{ "dstr must be greater than or equal to 0" };

        if (d->maxd < 1 || d->maxd > 29)
            throw std::string{ "maxd must be between 1 and 29 (inclusive)" };

        if (d->map < 0 || d->map > 3)
            throw std::string{ "map must be 0, 1, 2 or 3" };

        if (nt < 0)
            throw std::string{ "nt must be greater than or equal to 0" };

        if (d->pp < 0 || d->pp > 3)
            throw std::string{ "pp must be 0, 1, 2 or 3" };

        d->fieldS = d->field;
        if (d->fieldS == 2)
            d->field = 0;
        else if (d->fieldS == 3)
            d->field = 1;

        if (d->map == 0 || d->map == 3)
            d->vi2.height *= 2;

        d->mthresh *= d->mthresh;
        d->vthresh *= 81;

        const int8_t limlut[33]{
            6, 6, 7, 7, 8, 8, 9, 9, 9, 10,
            10, 11, 11, 12, 12, 12, 12, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
            12, -1, -1
        };

        d->limlut = new int8_t[33];
        d->limlut2 = new int16_t[33];
        std::copy_n(limlut, 33, d->limlut);

        const unsigned shift = d->vi->format->bitsPerSample - 8;
        nt <<= shift;
        for (unsigned i = 0; i < 33; i++)
            d->limlut2[i] = limlut[i] << shift;

        d->nt4 = nt * 4;
        d->nt7 = nt * 7;
        d->nt8 = nt * 8;
        d->nt13 = nt * 13;
        d->nt19 = nt * 19;

        const unsigned numThreads = vsapi->getCoreInfo(core)->numThreads;
        d->cx2.reserve(numThreads);
        d->cy2.reserve(numThreads);
        d->cxy.reserve(numThreads);
        d->tmpc.reserve(numThreads);
    } catch (const std::string & error) {
        vsapi->setError(out, ("EEDI2: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "EEDI2", eedi2Init, eedi2GetFrame, eedi2Free, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.eedi2", "eedi2", "EEDI2", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("EEDI2",
                 "clip:clip;"
                 "field:int;"
                 "mthresh:int:opt;"
                 "lthresh:int:opt;"
                 "vthresh:int:opt;"
                 "estr:int:opt;"
                 "dstr:int:opt;"
                 "maxd:int:opt;"
                 "map:int:opt;"
                 "nt:int:opt;"
                 "pp:int:opt;",
                 eedi2Create, nullptr, plugin);
}
