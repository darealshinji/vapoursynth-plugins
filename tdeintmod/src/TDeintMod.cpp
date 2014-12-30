/*
**   VapourSynth port by HolyWu
**
**                TDeinterlace v1.1 for Avisynth 2.5.x
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which
**   help to reduce "jaggy" edges in places where interpolation must
**   be used. TDeinterlace currently supports YV12 and YUY2 colorspaces.
**
**   Copyright (C) 2004-2007 Kevin Stone
**
**                    TMM v1.0 for Avisynth 2.5.x
**
**   TMM builds a motion-mask for TDeint, which TDeint uses via
**   its 'emask' parameter.  TMM can use fixed or per-pixel adaptive
**   motion thresholds, as well as any length static period.  It
**   checks backwards, across, and forwards when looking for motion.
**
**   Copyright (C) 2007 Kevin Stone
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
#include <vector>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>
#ifdef VS_TARGET_CPU_X86
#include "vectorclass/vectorclass.h"
#endif

struct TDeintModData {
    VSNodeRef * node, * node2, * mask, * edeint;
    VSVideoInfo vi;
    const VSVideoInfo * viSaved;
    int order, field, mode, length, mtype, ttype, mtqL, mthL, mtqC, mthC, nt, minthresh, maxthresh, cstr;
    bool show;
    int8_t gvlut[60];
    std::vector<int8_t> vlut;
    std::vector<int> tmmlut16;
    int ten, twenty, thirty, forty, fifty, sixty, seventy;
};

struct IsCombedData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    int cthresh, blockx, blocky, MI, metric;
    bool chroma;
    int xhalf, yhalf, xshift, yshift, cthresh6, cthreshsq;
};

static inline void memset16(void * ptr, int value, size_t num) {
    uint16_t * tptr = static_cast<uint16_t *>(ptr);
    while (num-- > 0)
        *tptr++ = value;
}

static inline bool isPowerOf2(const int i) {
    return i && !(i & (i - 1));
}

#ifdef VS_TARGET_CPU_X86
template<typename T>
static inline T abs_dif(const T & a, const T & b) {
    return sub_saturated(a, b) | sub_saturated(b, a);
}

template<typename T1, typename T2>
static inline void threshMask(const VSFrameRef * src, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    const T1 peak = (1 << d->vi.format->bitsPerSample) - 1;
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane) / d->vi.format->bytesPerSample;
        const T1 * srcp_ = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src, plane));
        T1 * dstp0 = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane));
        T1 * dstp1 = dstp0 + stride * height;
        if (d->vi.format->bitsPerSample == 8) {
            if (plane == 0 && d->mtqL > -1 && d->mthL > -1) {
                memset(dstp0, d->mtqL, stride * height);
                memset(dstp1, d->mthL, stride * height);
                continue;
            } else if (plane > 0 && d->mtqC > -1 && d->mthC > -1) {
                memset(dstp0, d->mtqC, stride * height);
                memset(dstp1, d->mthC, stride * height);
                continue;
            }
        } else {
            if (plane == 0 && d->mtqL > -1 && d->mthL > -1) {
                memset16(dstp0, d->mtqL, stride * height);
                memset16(dstp1, d->mthL, stride * height);
                continue;
            } else if (plane > 0 && d->mtqC > -1 && d->mthC > -1) {
                memset16(dstp0, d->mtqC, stride * height);
                memset16(dstp1, d->mthC, stride * height);
                continue;
            }
        }
        const T1 hs = plane ? d->vi.format->subSamplingW : 0;
        const T1 vs = plane ? 1 << d->vi.format->subSamplingH : 1;
        const T1 vss = 1 << (vs - 1);
        if (d->ttype == 0) { // 4 neighbors - compensated
            for (int y = 0; y < height; y++) {
                const T1 * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const T1 * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    T2 min0(peak), max0(0);
                    T2 min1(peak), max1(0);
                    const T2 srcp = T2().load_a(srcp_ + x);
                    const T2 srcpmp = T2().load(srcp_ + x - 1);
                    const T2 srcppn = T2().load(srcp_ + x + 1);
                    const T2 srcpp = T2().load_a(srcpp_ + x);
                    const T2 srcpn = T2().load_a(srcpn_ + x);
                    min0 = min(srcpp, min0);
                    max0 = max(srcpp, max0);
                    min1 = min(srcpmp, min1);
                    max1 = max(srcpmp, max1);
                    min1 = min(srcppn, min1);
                    max1 = max(srcppn, max1);
                    min0 = min(srcpn, min0);
                    max0 = max(srcpn, max0);
                    const T2 atv = max((abs_dif<T2>(srcp, min0) + vss) >> vs, (abs_dif<T2>(srcp, max0) + vss) >> vs);
                    const T2 ath = max((abs_dif<T2>(srcp, min1) + hs) >> hs, (abs_dif<T2>(srcp, max1) + hs) >> hs);
                    const T2 atmax = max(atv, ath);
                    ((atmax + 2) >> 2).store_a(dstp0 + x);
                    ((atmax + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T1 min0 = peak, max0 = 0;
                    T1 min1 = peak, max1 = 0;
                    if (srcpp_[x] < min0)
                        min0 = srcpp_[x];
                    if (srcpp_[x] > max0)
                        max0 = srcpp_[x];
                    if (srcp_[x - offp] < min1)
                        min1 = srcp_[x - offp];
                    if (srcp_[x - offp] > max1)
                        max1 = srcp_[x - offp];
                    if (srcp_[x + offn] < min1)
                        min1 = srcp_[x + offn];
                    if (srcp_[x + offn] > max1)
                        max1 = srcp_[x + offn];
                    if (srcpn_[x] < min0)
                        min0 = srcpn_[x];
                    if (srcpn_[x] > max0)
                        max0 = srcpn_[x];
                    const T1 atv = std::max((std::abs(srcp_[x] - min0) + vss) >> vs, (std::abs(srcp_[x] - max0) + vss) >> vs);
                    const T1 ath = std::max((std::abs(srcp_[x] - min1) + hs) >> hs, (std::abs(srcp_[x] - max1) + hs) >> hs);
                    const T1 atmax = std::max(atv, ath);
                    dstp0[x] = (atmax + 2) >> 2;
                    dstp1[x] = (atmax + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 1) { // 8 neighbors - compensated
            for (int y = 0; y < height; y++) {
                const T1 * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const T1 * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    T2 min0(peak), max0(0);
                    T2 min1(peak), max1(0);
                    const T2 srcp = T2().load_a(srcp_ + x);
                    const T2 srcpmp = T2().load(srcp_ + x - 1);
                    const T2 srcppn = T2().load(srcp_ + x + 1);
                    const T2 srcpp = T2().load_a(srcpp_ + x);
                    const T2 srcppmp = T2().load(srcpp_ + x - 1);
                    const T2 srcpppn = T2().load(srcpp_ + x + 1);
                    const T2 srcpn = T2().load_a(srcpn_ + x);
                    const T2 srcpnmp = T2().load(srcpn_ + x - 1);
                    const T2 srcpnpn = T2().load(srcpn_ + x + 1);
                    min0 = min(srcppmp, min0);
                    max0 = max(srcppmp, max0);
                    min0 = min(srcpp, min0);
                    max0 = max(srcpp, max0);
                    min0 = min(srcpppn, min0);
                    max0 = max(srcpppn, max0);
                    min1 = min(srcpmp, min1);
                    max1 = max(srcpmp, max1);
                    min1 = min(srcppn, min1);
                    max1 = max(srcppn, max1);
                    min0 = min(srcpnmp, min0);
                    max0 = max(srcpnmp, max0);
                    min0 = min(srcpn, min0);
                    max0 = max(srcpn, max0);
                    min0 = min(srcpnpn, min0);
                    max0 = max(srcpnpn, max0);
                    const T2 atv = max((abs_dif<T2>(srcp, min0) + vss) >> vs, (abs_dif<T2>(srcp, max0) + vss) >> vs);
                    const T2 ath = max((abs_dif<T2>(srcp, min1) + hs) >> hs, (abs_dif<T2>(srcp, max1) + hs) >> hs);
                    const T2 atmax = max(atv, ath);
                    ((atmax + 2) >> 2).store_a(dstp0 + x);
                    ((atmax + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T1 min0 = peak, max0 = 0;
                    T1 min1 = peak, max1 = 0;
                    if (srcpp_[x - offp] < min0)
                        min0 = srcpp_[x - offp];
                    if (srcpp_[x - offp] > max0)
                        max0 = srcpp_[x - offp];
                    if (srcpp_[x] < min0)
                        min0 = srcpp_[x];
                    if (srcpp_[x] > max0)
                        max0 = srcpp_[x];
                    if (srcpp_[x + offn] < min0)
                        min0 = srcpp_[x + offn];
                    if (srcpp_[x + offn] > max0)
                        max0 = srcpp_[x + offn];
                    if (srcp_[x - offp] < min1)
                        min1 = srcp_[x - offp];
                    if (srcp_[x - offp] > max1)
                        max1 = srcp_[x - offp];
                    if (srcp_[x + offn] < min1)
                        min1 = srcp_[x + offn];
                    if (srcp_[x + offn] > max1)
                        max1 = srcp_[x + offn];
                    if (srcpn_[x - offp] < min0)
                        min0 = srcpn_[x - offp];
                    if (srcpn_[x - offp] > max0)
                        max0 = srcpn_[x - offp];
                    if (srcpn_[x] < min0)
                        min0 = srcpn_[x];
                    if (srcpn_[x] > max0)
                        max0 = srcpn_[x];
                    if (srcpn_[x + offn] < min0)
                        min0 = srcpn_[x + offn];
                    if (srcpn_[x + offn] > max0)
                        max0 = srcpn_[x + offn];
                    const T1 atv = std::max((std::abs(srcp_[x] - min0) + vss) >> vs, (std::abs(srcp_[x] - max0) + vss) >> vs);
                    const T1 ath = std::max((std::abs(srcp_[x] - min1) + hs) >> hs, (std::abs(srcp_[x] - max1) + hs) >> hs);
                    const T1 atmax = std::max(atv, ath);
                    dstp0[x] = (atmax + 2) >> 2;
                    dstp1[x] = (atmax + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 2) { // 4 neighbors - not compensated
            for (int y = 0; y < height; y++) {
                const T1 * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const T1 * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    T2 min0(peak), max0(0);
                    const T2 srcp = T2().load_a(srcp_ + x);
                    const T2 srcpmp = T2().load(srcp_ + x - 1);
                    const T2 srcppn = T2().load(srcp_ + x + 1);
                    const T2 srcpp = T2().load_a(srcpp_ + x);
                    const T2 srcpn = T2().load_a(srcpn_ + x);
                    min0 = min(srcpp, min0);
                    max0 = max(srcpp, max0);
                    min0 = min(srcpmp, min0);
                    max0 = max(srcpmp, max0);
                    min0 = min(srcppn, min0);
                    max0 = max(srcppn, max0);
                    min0 = min(srcpn, min0);
                    max0 = max(srcpn, max0);
                    const T2 at = max(abs_dif<T2>(srcp, min0), abs_dif<T2>(srcp, max0));
                    ((at + 2) >> 2).store_a(dstp0 + x);
                    ((at + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T1 min0 = peak, max0 = 0;
                    if (srcpp_[x] < min0)
                        min0 = srcpp_[x];
                    if (srcpp_[x] > max0)
                        max0 = srcpp_[x];
                    if (srcp_[x - offp] < min0)
                        min0 = srcp_[x - offp];
                    if (srcp_[x - offp] > max0)
                        max0 = srcp_[x - offp];
                    if (srcp_[x + offn] < min0)
                        min0 = srcp_[x + offn];
                    if (srcp_[x + offn] > max0)
                        max0 = srcp_[x + offn];
                    if (srcpn_[x] < min0)
                        min0 = srcpn_[x];
                    if (srcpn_[x] > max0)
                        max0 = srcpn_[x];
                    const T1 at = std::max(std::abs(srcp_[x] - min0), std::abs(srcp_[x] - max0));
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 3) { // 8 neighbors - not compensated
            for (int y = 0; y < height; y++) {
                const T1 * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const T1 * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    T2 min0(peak), max0(0);
                    const T2 srcp = T2().load_a(srcp_ + x);
                    const T2 srcpmp = T2().load(srcp_ + x - 1);
                    const T2 srcppn = T2().load(srcp_ + x + 1);
                    const T2 srcpp = T2().load_a(srcpp_ + x);
                    const T2 srcppmp = T2().load(srcpp_ + x - 1);
                    const T2 srcpppn = T2().load(srcpp_ + x + 1);
                    const T2 srcpn = T2().load_a(srcpn_ + x);
                    const T2 srcpnmp = T2().load(srcpn_ + x - 1);
                    const T2 srcpnpn = T2().load(srcpn_ + x + 1);
                    min0 = min(srcppmp, min0);
                    max0 = max(srcppmp, max0);
                    min0 = min(srcpp, min0);
                    max0 = max(srcpp, max0);
                    min0 = min(srcpppn, min0);
                    max0 = max(srcpppn, max0);
                    min0 = min(srcpmp, min0);
                    max0 = max(srcpmp, max0);
                    min0 = min(srcppn, min0);
                    max0 = max(srcppn, max0);
                    min0 = min(srcpnmp, min0);
                    max0 = max(srcpnmp, max0);
                    min0 = min(srcpn, min0);
                    max0 = max(srcpn, max0);
                    min0 = min(srcpnpn, min0);
                    max0 = max(srcpnpn, max0);
                    const T2 at = max(abs_dif<T2>(srcp, min0), abs_dif<T2>(srcp, max0));
                    ((at + 2) >> 2).store_a(dstp0 + x);
                    ((at + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T1 min0 = peak, max0 = 0;
                    if (srcpp_[x - offp] < min0)
                        min0 = srcpp_[x - offp];
                    if (srcpp_[x - offp] > max0)
                        max0 = srcpp_[x - offp];
                    if (srcpp_[x] < min0)
                        min0 = srcpp_[x];
                    if (srcpp_[x] > max0)
                        max0 = srcpp_[x];
                    if (srcpp_[x + offn] < min0)
                        min0 = srcpp_[x + offn];
                    if (srcpp_[x + offn] > max0)
                        max0 = srcpp_[x + offn];
                    if (srcp_[x - offp] < min0)
                        min0 = srcp_[x - offp];
                    if (srcp_[x - offp] > max0)
                        max0 = srcp_[x - offp];
                    if (srcp_[x + offn] < min0)
                        min0 = srcp_[x + offn];
                    if (srcp_[x + offn] > max0)
                        max0 = srcp_[x + offn];
                    if (srcpn_[x - offp] < min0)
                        min0 = srcpn_[x - offp];
                    if (srcpn_[x - offp] > max0)
                        max0 = srcpn_[x - offp];
                    if (srcpn_[x] < min0)
                        min0 = srcpn_[x];
                    if (srcpn_[x] > max0)
                        max0 = srcpn_[x];
                    if (srcpn_[x + offn] < min0)
                        min0 = srcpn_[x + offn];
                    if (srcpn_[x + offn] > max0)
                        max0 = srcpn_[x + offn];
                    const T1 at = std::max(std::abs(srcp_[x] - min0), std::abs(srcp_[x] - max0));
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 4) { // 4 neighbors - not compensated (range)
            for (int y = 0; y < height; y++) {
                const T1 * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const T1 * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    T2 min0(peak), max0(0);
                    const T2 srcp = T2().load_a(srcp_ + x);
                    const T2 srcpmp = T2().load(srcp_ + x - 1);
                    const T2 srcppn = T2().load(srcp_ + x + 1);
                    const T2 srcpp = T2().load_a(srcpp_ + x);
                    const T2 srcpn = T2().load_a(srcpn_ + x);
                    min0 = min(srcpp, min0);
                    max0 = max(srcpp, max0);
                    min0 = min(srcpmp, min0);
                    max0 = max(srcpmp, max0);
                    min0 = min(srcp, min0);
                    max0 = max(srcp, max0);
                    min0 = min(srcppn, min0);
                    max0 = max(srcppn, max0);
                    min0 = min(srcpn, min0);
                    max0 = max(srcpn, max0);
                    const T2 at = max0 - min0;
                    ((at + 2) >> 2).store_a(dstp0 + x);
                    ((at + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T1 min0 = peak, max0 = 0;
                    if (srcpp_[x] < min0)
                        min0 = srcpp_[x];
                    if (srcpp_[x] > max0)
                        max0 = srcpp_[x];
                    if (srcp_[x - offp] < min0)
                        min0 = srcp_[x - offp];
                    if (srcp_[x - offp] > max0)
                        max0 = srcp_[x - offp];
                    if (srcp_[x] < min0)
                        min0 = srcp_[x];
                    if (srcp_[x] > max0)
                        max0 = srcp_[x];
                    if (srcp_[x + offn] < min0)
                        min0 = srcp_[x + offn];
                    if (srcp_[x + offn] > max0)
                        max0 = srcp_[x + offn];
                    if (srcpn_[x] < min0)
                        min0 = srcpn_[x];
                    if (srcpn_[x] > max0)
                        max0 = srcpn_[x];
                    const T1 at = max0 - min0;
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 5) { // 8 neighbors - not compensated (range)
            for (int y = 0; y < height; y++) {
                const T1 * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const T1 * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    T2 min0(peak), max0(0);
                    const T2 srcp = T2().load_a(srcp_ + x);
                    const T2 srcpmp = T2().load(srcp_ + x - 1);
                    const T2 srcppn = T2().load(srcp_ + x + 1);
                    const T2 srcpp = T2().load_a(srcpp_ + x);
                    const T2 srcppmp = T2().load(srcpp_ + x - 1);
                    const T2 srcpppn = T2().load(srcpp_ + x + 1);
                    const T2 srcpn = T2().load_a(srcpn_ + x);
                    const T2 srcpnmp = T2().load(srcpn_ + x - 1);
                    const T2 srcpnpn = T2().load(srcpn_ + x + 1);
                    min0 = min(srcppmp, min0);
                    max0 = max(srcppmp, max0);
                    min0 = min(srcpp, min0);
                    max0 = max(srcpp, max0);
                    min0 = min(srcpppn, min0);
                    max0 = max(srcpppn, max0);
                    min0 = min(srcpmp, min0);
                    max0 = max(srcpmp, max0);
                    min0 = min(srcp, min0);
                    max0 = max(srcp, max0);
                    min0 = min(srcppn, min0);
                    max0 = max(srcppn, max0);
                    min0 = min(srcpnmp, min0);
                    max0 = max(srcpnmp, max0);
                    min0 = min(srcpn, min0);
                    max0 = max(srcpn, max0);
                    min0 = min(srcpnpn, min0);
                    max0 = max(srcpnpn, max0);
                    const T2 at = max0 - min0;
                    ((at + 2) >> 2).store_a(dstp0 + x);
                    ((at + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T1 min0 = peak, max0 = 0;
                    if (srcpp_[x - offp] < min0)
                        min0 = srcpp_[x - offp];
                    if (srcpp_[x - offp] > max0)
                        max0 = srcpp_[x - offp];
                    if (srcpp_[x] < min0)
                        min0 = srcpp_[x];
                    if (srcpp_[x] > max0)
                        max0 = srcpp_[x];
                    if (srcpp_[x + offn] < min0)
                        min0 = srcpp_[x + offn];
                    if (srcpp_[x + offn] > max0)
                        max0 = srcpp_[x + offn];
                    if (srcp_[x - offp] < min0)
                        min0 = srcp_[x - offp];
                    if (srcp_[x - offp] > max0)
                        max0 = srcp_[x - offp];
                    if (srcp_[x] < min0)
                        min0 = srcp_[x];
                    if (srcp_[x] > max0)
                        max0 = srcp_[x];
                    if (srcp_[x + offn] < min0)
                        min0 = srcp_[x + offn];
                    if (srcp_[x + offn] > max0)
                        max0 = srcp_[x + offn];
                    if (srcpn_[x - offp] < min0)
                        min0 = srcpn_[x - offp];
                    if (srcpn_[x - offp] > max0)
                        max0 = srcpn_[x - offp];
                    if (srcpn_[x] < min0)
                        min0 = srcpn_[x];
                    if (srcpn_[x] > max0)
                        max0 = srcpn_[x];
                    if (srcpn_[x + offn] < min0)
                        min0 = srcpn_[x + offn];
                    if (srcpn_[x + offn] > max0)
                        max0 = srcpn_[x + offn];
                    const T1 at = max0 - min0;
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        }
        if (d->vi.format->bitsPerSample == 8) {
            if (plane == 0 && d->mtqL > -1)
                memset(vsapi->getWritePtr(dst, plane), d->mtqL, stride * height);
            else if (plane == 0 && d->mthL > -1)
                memset(vsapi->getWritePtr(dst, plane) + stride * height, d->mthL, stride * height);
            else if (plane > 0 && d->mtqC > -1)
                memset(vsapi->getWritePtr(dst, plane), d->mtqC, stride * height);
            else if (plane > 0 && d->mthC > -1)
                memset(vsapi->getWritePtr(dst, plane) + stride * height, d->mthC, stride * height);
        } else {
            if (plane == 0 && d->mtqL > -1)
                memset16(reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane)), d->mtqL, stride * height);
            else if (plane == 0 && d->mthL > -1)
                memset16(reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane)) + stride * height, d->mthL, stride * height);
            else if (plane > 0 && d->mtqC > -1)
                memset16(reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane)), d->mtqC, stride * height);
            else if (plane > 0 && d->mthC > -1)
                memset16(reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane)) + stride * height, d->mthC, stride * height);
        }
    }
}
#else
template<typename T>
static inline void threshMask(const VSFrameRef * src, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    const T peak = (1 << d->vi.format->bitsPerSample) - 1;
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane) / d->vi.format->bytesPerSample;
        const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
        T * dstp0 = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
        T * dstp1 = dstp0 + stride * height;
        if (d->vi.format->bitsPerSample == 8) {
            if (plane == 0 && d->mtqL > -1 && d->mthL > -1) {
                memset(dstp0, d->mtqL, stride * height);
                memset(dstp1, d->mthL, stride * height);
                continue;
            } else if (plane > 0 && d->mtqC > -1 && d->mthC > -1) {
                memset(dstp0, d->mtqC, stride * height);
                memset(dstp1, d->mthC, stride * height);
                continue;
            }
        } else {
            if (plane == 0 && d->mtqL > -1 && d->mthL > -1) {
                memset16(dstp0, d->mtqL, stride * height);
                memset16(dstp1, d->mthL, stride * height);
                continue;
            } else if (plane > 0 && d->mtqC > -1 && d->mthC > -1) {
                memset16(dstp0, d->mtqC, stride * height);
                memset16(dstp1, d->mthC, stride * height);
                continue;
            }
        }
        const T hs = plane ? d->vi.format->subSamplingW : 0;
        const T vs = plane ? 1 << d->vi.format->subSamplingH : 1;
        const T vss = 1 << (vs - 1);
        if (d->ttype == 0) { // 4 neighbors - compensated
            for (int y = 0; y < height; y++) {
                const T * srcpp = srcp - (y == 0 ? -stride : stride);
                const T * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T min0 = peak, max0 = 0;
                    T min1 = peak, max1 = 0;
                    if (srcpp[x] < min0)
                        min0 = srcpp[x];
                    if (srcpp[x] > max0)
                        max0 = srcpp[x];
                    if (srcp[x - offp] < min1)
                        min1 = srcp[x - offp];
                    if (srcp[x - offp] > max1)
                        max1 = srcp[x - offp];
                    if (srcp[x + offn] < min1)
                        min1 = srcp[x + offn];
                    if (srcp[x + offn] > max1)
                        max1 = srcp[x + offn];
                    if (srcpn[x] < min0)
                        min0 = srcpn[x];
                    if (srcpn[x] > max0)
                        max0 = srcpn[x];
                    const T atv = std::max((std::abs(srcp[x] - min0) + vss) >> vs, (std::abs(srcp[x] - max0) + vss) >> vs);
                    const T ath = std::max((std::abs(srcp[x] - min1) + hs) >> hs, (std::abs(srcp[x] - max1) + hs) >> hs);
                    const T atmax = std::max(atv, ath);
                    dstp0[x] = (atmax + 2) >> 2;
                    dstp1[x] = (atmax + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 1) { // 8 neighbors - compensated
            for (int y = 0; y < height; y++) {
                const T * srcpp = srcp - (y == 0 ? -stride : stride);
                const T * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T min0 = peak, max0 = 0;
                    T min1 = peak, max1 = 0;
                    if (srcpp[x - offp] < min0)
                        min0 = srcpp[x - offp];
                    if (srcpp[x - offp] > max0)
                        max0 = srcpp[x - offp];
                    if (srcpp[x] < min0)
                        min0 = srcpp[x];
                    if (srcpp[x] > max0)
                        max0 = srcpp[x];
                    if (srcpp[x + offn] < min0)
                        min0 = srcpp[x + offn];
                    if (srcpp[x + offn] > max0)
                        max0 = srcpp[x + offn];
                    if (srcp[x - offp] < min1)
                        min1 = srcp[x - offp];
                    if (srcp[x - offp] > max1)
                        max1 = srcp[x - offp];
                    if (srcp[x + offn] < min1)
                        min1 = srcp[x + offn];
                    if (srcp[x + offn] > max1)
                        max1 = srcp[x + offn];
                    if (srcpn[x - offp] < min0)
                        min0 = srcpn[x - offp];
                    if (srcpn[x - offp] > max0)
                        max0 = srcpn[x - offp];
                    if (srcpn[x] < min0)
                        min0 = srcpn[x];
                    if (srcpn[x] > max0)
                        max0 = srcpn[x];
                    if (srcpn[x + offn] < min0)
                        min0 = srcpn[x + offn];
                    if (srcpn[x + offn] > max0)
                        max0 = srcpn[x + offn];
                    const T atv = std::max((std::abs(srcp[x] - min0) + vss) >> vs, (std::abs(srcp[x] - max0) + vss) >> vs);
                    const T ath = std::max((std::abs(srcp[x] - min1) + hs) >> hs, (std::abs(srcp[x] - max1) + hs) >> hs);
                    const T atmax = std::max(atv, ath);
                    dstp0[x] = (atmax + 2) >> 2;
                    dstp1[x] = (atmax + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 2) { // 4 neighbors - not compensated
            for (int y = 0; y < height; y++) {
                const T * srcpp = srcp - (y == 0 ? -stride : stride);
                const T * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T min0 = peak, max0 = 0;
                    if (srcpp[x] < min0)
                        min0 = srcpp[x];
                    if (srcpp[x] > max0)
                        max0 = srcpp[x];
                    if (srcp[x - offp] < min0)
                        min0 = srcp[x - offp];
                    if (srcp[x - offp] > max0)
                        max0 = srcp[x - offp];
                    if (srcp[x + offn] < min0)
                        min0 = srcp[x + offn];
                    if (srcp[x + offn] > max0)
                        max0 = srcp[x + offn];
                    if (srcpn[x] < min0)
                        min0 = srcpn[x];
                    if (srcpn[x] > max0)
                        max0 = srcpn[x];
                    const T at = std::max(std::abs(srcp[x] - min0), std::abs(srcp[x] - max0));
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 3) { // 8 neighbors - not compensated
            for (int y = 0; y < height; y++) {
                const T * srcpp = srcp - (y == 0 ? -stride : stride);
                const T * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T min0 = peak, max0 = 0;
                    if (srcpp[x - offp] < min0)
                        min0 = srcpp[x - offp];
                    if (srcpp[x - offp] > max0)
                        max0 = srcpp[x - offp];
                    if (srcpp[x] < min0)
                        min0 = srcpp[x];
                    if (srcpp[x] > max0)
                        max0 = srcpp[x];
                    if (srcpp[x + offn] < min0)
                        min0 = srcpp[x + offn];
                    if (srcpp[x + offn] > max0)
                        max0 = srcpp[x + offn];
                    if (srcp[x - offp] < min0)
                        min0 = srcp[x - offp];
                    if (srcp[x - offp] > max0)
                        max0 = srcp[x - offp];
                    if (srcp[x + offn] < min0)
                        min0 = srcp[x + offn];
                    if (srcp[x + offn] > max0)
                        max0 = srcp[x + offn];
                    if (srcpn[x - offp] < min0)
                        min0 = srcpn[x - offp];
                    if (srcpn[x - offp] > max0)
                        max0 = srcpn[x - offp];
                    if (srcpn[x] < min0)
                        min0 = srcpn[x];
                    if (srcpn[x] > max0)
                        max0 = srcpn[x];
                    if (srcpn[x + offn] < min0)
                        min0 = srcpn[x + offn];
                    if (srcpn[x + offn] > max0)
                        max0 = srcpn[x + offn];
                    const T at = std::max(std::abs(srcp[x] - min0), std::abs(srcp[x] - max0));
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 4) { // 4 neighbors - not compensated (range)
            for (int y = 0; y < height; y++) {
                const T * srcpp = srcp - (y == 0 ? -stride : stride);
                const T * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T min0 = peak, max0 = 0;
                    if (srcpp[x] < min0)
                        min0 = srcpp[x];
                    if (srcpp[x] > max0)
                        max0 = srcpp[x];
                    if (srcp[x - offp] < min0)
                        min0 = srcp[x - offp];
                    if (srcp[x - offp] > max0)
                        max0 = srcp[x - offp];
                    if (srcp[x] < min0)
                        min0 = srcp[x];
                    if (srcp[x] > max0)
                        max0 = srcp[x];
                    if (srcp[x + offn] < min0)
                        min0 = srcp[x + offn];
                    if (srcp[x + offn] > max0)
                        max0 = srcp[x + offn];
                    if (srcpn[x] < min0)
                        min0 = srcpn[x];
                    if (srcpn[x] > max0)
                        max0 = srcpn[x];
                    const T at = max0 - min0;
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 5) { // 8 neighbors - not compensated (range)
            for (int y = 0; y < height; y++) {
                const T * srcpp = srcp - (y == 0 ? -stride : stride);
                const T * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = x == 0 ? -1 : 1;
                    const int offn = x == width - 1 ? -1 : 1;
                    T min0 = peak, max0 = 0;
                    if (srcpp[x - offp] < min0)
                        min0 = srcpp[x - offp];
                    if (srcpp[x - offp] > max0)
                        max0 = srcpp[x - offp];
                    if (srcpp[x] < min0)
                        min0 = srcpp[x];
                    if (srcpp[x] > max0)
                        max0 = srcpp[x];
                    if (srcpp[x + offn] < min0)
                        min0 = srcpp[x + offn];
                    if (srcpp[x + offn] > max0)
                        max0 = srcpp[x + offn];
                    if (srcp[x - offp] < min0)
                        min0 = srcp[x - offp];
                    if (srcp[x - offp] > max0)
                        max0 = srcp[x - offp];
                    if (srcp[x] < min0)
                        min0 = srcp[x];
                    if (srcp[x] > max0)
                        max0 = srcp[x];
                    if (srcp[x + offn] < min0)
                        min0 = srcp[x + offn];
                    if (srcp[x + offn] > max0)
                        max0 = srcp[x + offn];
                    if (srcpn[x - offp] < min0)
                        min0 = srcpn[x - offp];
                    if (srcpn[x - offp] > max0)
                        max0 = srcpn[x - offp];
                    if (srcpn[x] < min0)
                        min0 = srcpn[x];
                    if (srcpn[x] > max0)
                        max0 = srcpn[x];
                    if (srcpn[x + offn] < min0)
                        min0 = srcpn[x + offn];
                    if (srcpn[x + offn] > max0)
                        max0 = srcpn[x + offn];
                    const T at = max0 - min0;
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        }
        if (d->vi.format->bitsPerSample == 8) {
            if (plane == 0 && d->mtqL > -1)
                memset(vsapi->getWritePtr(dst, plane), d->mtqL, stride * height);
            else if (plane == 0 && d->mthL > -1)
                memset(vsapi->getWritePtr(dst, plane) + stride * height, d->mthL, stride * height);
            else if (plane > 0 && d->mtqC > -1)
                memset(vsapi->getWritePtr(dst, plane), d->mtqC, stride * height);
            else if (plane > 0 && d->mthC > -1)
                memset(vsapi->getWritePtr(dst, plane) + stride * height, d->mthC, stride * height);
        } else {
            if (plane == 0 && d->mtqL > -1)
                memset16(reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane)), d->mtqL, stride * height);
            else if (plane == 0 && d->mthL > -1)
                memset16(reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane)) + stride * height, d->mthL, stride * height);
            else if (plane > 0 && d->mtqC > -1)
                memset16(reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane)), d->mtqC, stride * height);
            else if (plane > 0 && d->mthC > -1)
                memset16(reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane)) + stride * height, d->mthC, stride * height);
        }
    }
}
#endif

#ifdef VS_TARGET_CPU_X86
template<typename T1, typename T2>
static void motionMask(const VSFrameRef * src1, const VSFrameRef * msk1, const VSFrameRef * src2, const VSFrameRef * msk2, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    const T2 peak(d->vi.format->bitsPerSample == 8 ? UINT8_MAX : UINT16_MAX), zero(0);
    const T2 nt(d->nt), minthresh(d->minthresh), maxthresh(d->maxthresh);
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src1, plane);
        const int height = vsapi->getFrameHeight(src1, plane);
        const int stride = vsapi->getStride(src1, plane) / d->vi.format->bytesPerSample;
        const T1 * srcp1_ = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src1, plane));
        const T1 * srcp2_ = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src2, plane));
        const T1 * mskp1q_ = reinterpret_cast<const T1 *>(vsapi->getReadPtr(msk1, plane));
        const T1 * mskp1h_ = mskp1q_ + stride * height;
        const T1 * mskp2q_ = reinterpret_cast<const T1 *>(vsapi->getReadPtr(msk2, plane));
        const T1 * mskp2h_ = mskp2q_ + stride * height;
        T1 * dstpq = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane));
        T1 * dstph = dstpq + stride * height;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += 16) {
                const T2 srcp1 = T2().load_a(srcp1_ + x);
                const T2 srcp2 = T2().load_a(srcp2_ + x);
                const T2 mskp1q = T2().load_a(mskp1q_ + x);
                const T2 mskp1h = T2().load_a(mskp1h_ + x);
                const T2 mskp2q = T2().load_a(mskp2q_ + x);
                const T2 mskp2h = T2().load_a(mskp2h_ + x);
                const T2 diff = abs_dif<T2>(srcp1, srcp2);
                const T2 threshq = min(mskp1q, mskp2q);
                select(diff <= min(max(threshq + nt, minthresh), maxthresh), peak, zero).store_a(dstpq + x);
                const T2 threshh = min(mskp1h, mskp2h);
                select(diff <= min(max(threshh + nt, minthresh), maxthresh), peak, zero).store_a(dstph + x);
            }
            srcp1_ += stride;
            srcp2_ += stride;
            mskp1q_ += stride;
            mskp1h_ += stride;
            mskp2q_ += stride;
            mskp2h_ += stride;
            dstpq += stride;
            dstph += stride;
        }
    }
}
#else
template<typename T>
static void motionMask(const VSFrameRef * src1, const VSFrameRef * msk1, const VSFrameRef * src2, const VSFrameRef * msk2, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    const T peak = d->vi.format->bitsPerSample == 8 ? UINT8_MAX : UINT16_MAX;
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src1, plane);
        const int height = vsapi->getFrameHeight(src1, plane);
        const int stride = vsapi->getStride(src1, plane) / d->vi.format->bytesPerSample;
        const T * srcp1 = reinterpret_cast<const T *>(vsapi->getReadPtr(src1, plane));
        const T * srcp2 = reinterpret_cast<const T *>(vsapi->getReadPtr(src2, plane));
        const T * mskp1q = reinterpret_cast<const T *>(vsapi->getReadPtr(msk1, plane));
        const T * mskp1h = mskp1q + stride * height;
        const T * mskp2q = reinterpret_cast<const T *>(vsapi->getReadPtr(msk2, plane));
        const T * mskp2h = mskp2q + stride * height;
        T * dstpq = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
        T * dstph = dstpq + stride * height;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int diff = std::abs(srcp1[x] - srcp2[x]);
                const int threshq = std::min(mskp1q[x], mskp2q[x]);
                dstpq[x] = diff <= std::min(std::max(threshq + d->nt, d->minthresh), d->maxthresh) ? peak : 0;
                const int threshh = std::min(mskp1h[x], mskp2h[x]);
                dstph[x] = diff <= std::min(std::max(threshh + d->nt, d->minthresh), d->maxthresh) ? peak : 0;
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
}
#endif

#ifdef VS_TARGET_CPU_X86
template<typename T1, typename T2>
static inline void andMasks(const VSFrameRef * src1, const VSFrameRef * src2, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src1, plane);
        const int height = vsapi->getFrameHeight(src1, plane);
        const int stride = vsapi->getStride(src1, plane) / d->vi.format->bytesPerSample;
        const T1 * srcp1_ = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src1, plane));
        const T1 * srcp2_ = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src2, plane));
        T1 * dstp_ = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane));
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += 16) {
                const T2 srcp1 = T2().load_a(srcp1_ + x);
                const T2 srcp2 = T2().load_a(srcp2_ + x);
                const T2 dstp = T2().load_a(dstp_ + x);
                (dstp & (srcp1 & srcp2)).store_a(dstp_ + x);
            }
            srcp1_ += stride;
            srcp2_ += stride;
            dstp_ += stride;
        }
    }
}
#else
template<typename T>
static inline void andMasks(const VSFrameRef * src1, const VSFrameRef * src2, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src1, plane);
        const int height = vsapi->getFrameHeight(src1, plane);
        const int stride = vsapi->getStride(src1, plane) / d->vi.format->bytesPerSample;
        const T * srcp1 = reinterpret_cast<const T *>(vsapi->getReadPtr(src1, plane));
        const T * srcp2 = reinterpret_cast<const T *>(vsapi->getReadPtr(src2, plane));
        T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++)
                dstp[x] &= (srcp1[x] & srcp2[x]);
            srcp1 += stride;
            srcp2 += stride;
            dstp += stride;
        }
    }
}
#endif

template<typename T>
static inline void combineMasks(const VSFrameRef * src, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    const T peak = d->vi.format->bitsPerSample == 8 ? UINT8_MAX : UINT16_MAX;
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(dst, plane);
        const int stride = vsapi->getStride(src, plane) / d->vi.format->bytesPerSample;
        const T * srcp0 = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
        const T * srcp1 = srcp0 + stride * height;
        T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
        memcpy(dstp, srcp0, vsapi->getStride(src, plane) * height);
        for (int y = 0; y < height; y++) {
            const T * srcpp0 = srcp0 - (y == 0 ? -stride : stride);
            const T * srcpn0 = srcp0 + (y == height - 1 ? -stride : stride);
            for (int x = 0; x < width; x++) {
                if (srcp0[x] || !srcp1[x])
                    continue;
                const int offp = x == 0 ? -1 : 1;
                const int offn = x == width - 1 ? -1 : 1;
                int count = 0;
                if (srcpp0[x - offp])
                    count++;
                if (srcpp0[x])
                    count++;
                if (srcpp0[x + offn])
                    count++;
                if (srcp0[x - offp])
                    count++;
                if (srcp0[x + offn])
                    count++;
                if (srcpn0[x - offp])
                    count++;
                if (srcpn0[x])
                    count++;
                if (srcpn0[x + offn])
                    count++;
                if (count >= d->cstr)
                    dstp[x] = peak;
            }
            srcp0 += stride;
            srcp1 += stride;
            dstp += stride;
        }
    }
}

template<typename T>
static inline void buildMask(VSFrameRef ** csrc, VSFrameRef ** osrc, VSFrameRef * dst, const int fieldt, const int ccount, const int ocount,
                             const TDeintModData * d, const VSAPI * vsapi) {
    const int * tmmlut = d->tmmlut16.data() + d->order * 8 + fieldt * 4;
    int tmmlutf[64];
    for (int i = 0; i < 64; i++)
        tmmlutf[i] = tmmlut[d->vlut[i]];
    int plut[2][119];   // The size is (2 * length - 1) for the second dimension in the original version
    T ** ptlut[3];
    for (int i = 0; i < 3; i++)
        ptlut[i] = new T *[i & 1 ? ccount : ocount];
    const int offo = d->length & 1 ? 0 : 1;
    const int offc = d->length & 1 ? 1 : 0;

    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(dst, plane);
        const int height = vsapi->getFrameHeight(dst, plane);
        const int stride = vsapi->getStride(dst, plane) / d->vi.format->bytesPerSample;
        for (int i = 0; i < ccount; i++)
            ptlut[1][i] = reinterpret_cast<T *>(vsapi->getWritePtr(csrc[i], plane));
        for (int i = 0; i < ocount; i++) {
            if (fieldt == 1) {
                ptlut[0][i] = reinterpret_cast<T *>(vsapi->getWritePtr(osrc[i], plane));
                ptlut[2][i] = ptlut[0][i] + stride;
            } else {
                ptlut[0][i] = ptlut[2][i] = reinterpret_cast<T *>(vsapi->getWritePtr(osrc[i], plane));
            }
        }
        T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

        if (d->vi.format->bitsPerSample == 8) {
            if (fieldt == 1) {
                for (int j = 0; j < height; j += 2)
                    memset(dstp + stride * j, d->ten, width);
                dstp += stride;
            } else {
                for (int j = 1; j < height; j += 2)
                    memset(dstp + stride * j, d->ten, width);
            }
        } else {
            if (fieldt == 1) {
                for (int j = 0; j < height; j += 2)
                    memset16(dstp + stride * j, d->ten, width);
                dstp += stride;
            } else {
                for (int j = 1; j < height; j += 2)
                    memset16(dstp + stride * j, d->ten, width);
            }
        }

        const int ct = ccount / 2;
        for (int y = fieldt; y < height; y += 2) {
            for (int x = 0; x < width; x++) {
                if (!ptlut[1][ct - 2][x] && !ptlut[1][ct][x] && !ptlut[1][ct + 1][x]) {
                    dstp[x] = d->sixty;
                    continue;
                }
                for (int j = 0; j < ccount; j++)
                    plut[0][j * 2 + offc] = plut[1][j * 2 + offc] = ptlut[1][j][x];
                for (int j = 0; j < ocount; j++) {
                    plut[0][j * 2 + offo] = ptlut[0][j][x];
                    plut[1][j * 2 + offo] = ptlut[2][j][x];
                }
                int val = 0;
                for (int i = 0; i < d->length; i++) {
                    for (int j = 0; j < d->length - 4; j++) {
                        if (!plut[0][i + j])
                            goto j1;
                    }
                    val |= d->gvlut[i] * 8;
                j1:
                    for (int j = 0; j < d->length - 4; j++) {
                        if (!plut[1][i + j])
                            goto j2;
                    }
                    val |= d->gvlut[i];
                j2:
                    if (d->vlut[val] == 2)
                        break;
                }
                dstp[x] = tmmlutf[val];
            }
            for (int i = 0; i < ccount; i++)
                ptlut[1][i] += stride;
            for (int i = 0; i < ocount; i++) {
                if (y != 0)
                    ptlut[0][i] += stride;
                if (y != height - 3)
                    ptlut[2][i] += stride;
            }
            dstp += stride * 2;
        }
    }

    for (int i = 0; i < 3; i++)
        delete[] ptlut[i];
}

template<typename T>
static inline void setMaskForUpsize(VSFrameRef * msk, const int fieldt, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(msk, plane);
        const int height = vsapi->getFrameHeight(msk, plane) / 2;
        const int stride = vsapi->getStride(msk, plane) / d->vi.format->bytesPerSample * 2;
        T * maskwc = reinterpret_cast<T *>(vsapi->getWritePtr(msk, plane));
        T * maskwn = maskwc + stride / 2;
        if (d->vi.format->bitsPerSample == 8) {
            if (fieldt == 1) {
                for (int y = 0; y < height - 1; y++) {
                    memset(maskwc, d->ten, width);
                    memset(maskwn, d->sixty, width);
                    maskwc += stride;
                    maskwn += stride;
                }
                memset(maskwc, d->ten, width);
                memset(maskwn, d->ten, width);
            } else {
                memset(maskwc, d->ten, width);
                memset(maskwn, d->ten, width);
                for (int y = 0; y < height - 1; y++) {
                    maskwc += stride;
                    maskwn += stride;
                    memset(maskwc, d->sixty, width);
                    memset(maskwn, d->ten, width);
                }
            }
        } else {
            if (fieldt == 1) {
                for (int y = 0; y < height - 1; y++) {
                    memset16(maskwc, d->ten, width);
                    memset16(maskwn, d->sixty, width);
                    maskwc += stride;
                    maskwn += stride;
                }
                memset16(maskwc, d->ten, width);
                memset16(maskwn, d->ten, width);
            } else {
                memset16(maskwc, d->ten, width);
                memset16(maskwn, d->ten, width);
                for (int y = 0; y < height - 1; y++) {
                    maskwc += stride;
                    maskwn += stride;
                    memset16(maskwc, d->sixty, width);
                    memset16(maskwn, d->ten, width);
                }
            }
        }
    }
}

template<typename T>
static inline void eDeint(VSFrameRef * dst, const VSFrameRef * mask, const VSFrameRef * prv, const VSFrameRef * src, const VSFrameRef * nxt, const VSFrameRef * efrm,
                          const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane) / d->vi.format->bytesPerSample;
        const T * prvp = reinterpret_cast<const T *>(vsapi->getReadPtr(prv, plane));
        const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
        const T * nxtp = reinterpret_cast<const T *>(vsapi->getReadPtr(nxt, plane));
        const T * maskp = reinterpret_cast<const T *>(vsapi->getReadPtr(mask, plane));
        const T * efrmp = reinterpret_cast<const T *>(vsapi->getReadPtr(efrm, plane));
        T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (maskp[x] == d->ten)
                    dstp[x] = srcp[x];
                else if (maskp[x] == d->twenty)
                    dstp[x] = prvp[x];
                else if (maskp[x] == d->thirty)
                    dstp[x] = nxtp[x];
                else if (maskp[x] == d->forty)
                    dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
                else if (maskp[x] == d->fifty)
                    dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
                else if (maskp[x] == d->seventy)
                    dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
                else if (maskp[x] == d->sixty)
                    dstp[x] = efrmp[x];
            }
            prvp += stride;
            srcp += stride;
            nxtp += stride;
            maskp += stride;
            efrmp += stride;
            dstp += stride;
        }
    }
}

template<typename T>
static inline void cubicDeint(VSFrameRef * dst, const VSFrameRef * mask, const VSFrameRef * prv, const VSFrameRef * src, const VSFrameRef * nxt,
                              const TDeintModData * d, const VSAPI * vsapi) {
    const int peak = (1 << d->vi.format->bitsPerSample) - 1;
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane) / d->vi.format->bytesPerSample;
        const T * prvp = reinterpret_cast<const T *>(vsapi->getReadPtr(prv, plane));
        const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
        const T * nxtp = reinterpret_cast<const T *>(vsapi->getReadPtr(nxt, plane));
        const T * maskp = reinterpret_cast<const T *>(vsapi->getReadPtr(mask, plane));
        T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
        const T * srcpp = srcp - stride;
        const T * srcppp = srcpp - stride * 2;
        const T * srcpn = srcp + stride;
        const T * srcpnn = srcpn + stride * 2;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (maskp[x] == d->ten)
                    dstp[x] = srcp[x];
                else if (maskp[x] == d->twenty)
                    dstp[x] = prvp[x];
                else if (maskp[x] == d->thirty)
                    dstp[x] = nxtp[x];
                else if (maskp[x] == d->forty)
                    dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
                else if (maskp[x] == d->fifty)
                    dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
                else if (maskp[x] == d->seventy)
                    dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
                else if (maskp[x] == d->sixty) {
                    if (y == 0) {
                        dstp[x] = srcpn[x];
                    } else if (y == height - 1) {
                        dstp[x] = srcpp[x];
                    } else if (y < 3 || y > height - 4) {
                        dstp[x] = (srcpn[x] + srcpp[x] + 1) >> 1;
                    } else {
                        const int temp = (19 * (srcpp[x] + srcpn[x]) - 3 * (srcppp[x] + srcpnn[x]) + 16) >> 5;
                        dstp[x] = std::min(std::max(temp, 0), peak);
                    }
                }
            }
            prvp += stride;
            srcppp += stride;
            srcpp += stride;
            srcp += stride;
            srcpn += stride;
            srcpnn += stride;
            nxtp += stride;
            maskp += stride;
            dstp += stride;
        }
    }
}

template<typename T>
static inline int checkCombed(const VSFrameRef * src, VSFrameRef * cmask, int * VS_RESTRICT cArray, const IsCombedData * d, const VSAPI * vsapi) {
    const T peak = d->vi->format->bitsPerSample == 8 ? UINT8_MAX : UINT16_MAX;
    for (int plane = 0; plane < (d->chroma ? 3 : 1); plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane) / d->vi->format->bytesPerSample;
        const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
        const T * srcpp = srcp - stride;
        const T * srcppp = srcpp - stride;
        const T * srcpn = srcp + stride;
        const T * srcpnn = srcpn + stride;
        T * cmkp = reinterpret_cast<T *>(vsapi->getWritePtr(cmask, plane));
        if (d->cthresh < 0) {
            memset(cmkp, 255, vsapi->getStride(src, plane) * height);
            continue;
        }
        memset(cmkp, 0, vsapi->getStride(src, plane) * height);
        if (d->metric == 0) {
            for (int x = 0; x < width; x++) {
                const int sFirst = srcp[x] - srcpn[x];
                if ((sFirst > d->cthresh || sFirst < -d->cthresh) && std::abs(srcpnn[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpn[x] + srcpn[x]))) > d->cthresh6)
                    cmkp[x] = peak;
            }
            srcppp += stride;
            srcpp += stride;
            srcp += stride;
            srcpn += stride;
            srcpnn += stride;
            cmkp += stride;
            for (int x = 0; x < width; x++) {
                const int sFirst = srcp[x] - srcpp[x];
                const int sSecond = srcp[x] - srcpn[x];
                if (((sFirst > d->cthresh && sSecond > d->cthresh) || (sFirst < -d->cthresh && sSecond < -d->cthresh)) &&
                    std::abs(srcpnn[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpp[x] + srcpn[x]))) > d->cthresh6)
                    cmkp[x] = peak;
            }
            srcppp += stride;
            srcpp += stride;
            srcp += stride;
            srcpn += stride;
            srcpnn += stride;
            cmkp += stride;
            for (int y = 2; y < height - 2; y++) {
                for (int x = 0; x < width; x++) {
                    const int sFirst = srcp[x] - srcpp[x];
                    const int sSecond = srcp[x] - srcpn[x];
                    if (((sFirst > d->cthresh && sSecond > d->cthresh) || (sFirst < -d->cthresh && sSecond < -d->cthresh)) &&
                        std::abs(srcppp[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpp[x] + srcpn[x]))) > d->cthresh6)
                        cmkp[x] = peak;
                }
                srcppp += stride;
                srcpp += stride;
                srcp += stride;
                srcpn += stride;
                srcpnn += stride;
                cmkp += stride;
            }
            for (int x = 0; x < width; x++) {
                const int sFirst = srcp[x] - srcpp[x];
                const int sSecond = srcp[x] - srcpn[x];
                if (((sFirst > d->cthresh && sSecond > d->cthresh) || (sFirst < -d->cthresh && sSecond < -d->cthresh)) &&
                    std::abs(srcppp[x] + (srcp[x] << 2) + srcppp[x] - (3 * (srcpp[x] + srcpn[x]))) > d->cthresh6)
                    cmkp[x] = peak;
            }
            srcppp += stride;
            srcpp += stride;
            srcp += stride;
            srcpn += stride;
            srcpnn += stride;
            cmkp += stride;
            for (int x = 0; x < width; x++) {
                const int sFirst = srcp[x] - srcpp[x];
                if ((sFirst > d->cthresh || sFirst < -d->cthresh) && std::abs(srcppp[x] + (srcp[x] << 2) + srcppp[x] - (3 * (srcpp[x] + srcpp[x]))) > d->cthresh6)
                    cmkp[x] = peak;
            }
        } else {
            for (int x = 0; x < width; x++) {
                if ((srcp[x] - srcpn[x]) * (srcp[x] - srcpn[x]) > d->cthreshsq)
                    cmkp[x] = peak;
            }
            srcpp += stride;
            srcp += stride;
            srcpn += stride;
            cmkp += stride;
            for (int y = 1; y < height - 1; y++) {
                for (int x = 0; x < width; x++) {
                    if ((srcp[x] - srcpp[x]) * (srcp[x] - srcpn[x]) > d->cthreshsq)
                        cmkp[x] = peak;
                }
                srcpp += stride;
                srcp += stride;
                srcpn += stride;
                cmkp += stride;
            }
            for (int x = 0; x < width; x++) {
                if ((srcp[x] - srcpp[x]) * (srcp[x] - srcpp[x]) > d->cthreshsq)
                    cmkp[x] = peak;
            }
        }
    }
    if (d->chroma) {
        const int width = vsapi->getFrameWidth(cmask, 2);
        const int height = vsapi->getFrameHeight(cmask, 2);
        const int stride = vsapi->getStride(cmask, 0) / d->vi->format->bytesPerSample;
        const int strideY = stride << d->vi->format->subSamplingH;
        const int strideUV = vsapi->getStride(cmask, 2) / d->vi->format->bytesPerSample;
        T * cmkp = reinterpret_cast<T *>(vsapi->getWritePtr(cmask, 0));
        const T * cmkpU = reinterpret_cast<const T *>(vsapi->getReadPtr(cmask, 1));
        const T * cmkpV = reinterpret_cast<const T *>(vsapi->getReadPtr(cmask, 2));
        T * cmkpp3 = cmkp - stride * 3;
        T * cmkpp2 = cmkp - stride * 2;
        T * cmkpp = cmkp - stride;
        T * cmkpn = cmkp + stride;
        T * cmkpn2 = cmkp + stride * 2;
        T * cmkpn3 = cmkp + stride * 3;
        T * cmkpn4 = cmkp + stride * 4;
        T * cmkpn5 = cmkp + stride * 5;
        T * cmkpn6 = cmkp + stride * 6;
        const T * cmkppU = cmkpU - strideUV;
        const T * cmkpnU = cmkpU + strideUV;
        const T * cmkppV = cmkpV - strideUV;
        const T * cmkpnV = cmkpV + strideUV;
        for (int y = 1; y < height - 1; y++) {
            cmkpp3 += strideY;
            cmkpp2 += strideY;
            cmkpp += strideY;
            cmkp += strideY;
            cmkpn += strideY;
            cmkpn2 += strideY;
            cmkpn3 += strideY;
            cmkpn4 += strideY;
            cmkpn5 += strideY;
            cmkpn6 += strideY;
            cmkppU += strideUV;
            cmkpU += strideUV;
            cmkpnU += strideUV;
            cmkppV += strideUV;
            cmkpV += strideUV;
            cmkpnV += strideUV;
            for (int x = 1; x < width - 1; x++) {
                if ((cmkpU[x] == peak && (cmkpU[x - 1] == peak || cmkpU[x + 1] == peak ||
                     cmkppU[x - 1] == peak || cmkppU[x] == peak || cmkppU[x + 1] == peak ||
                     cmkpnU[x - 1] == peak || cmkpnU[x] == peak || cmkpnU[x + 1] == peak)) ||
                    (cmkpV[x] == peak && (cmkpV[x - 1] == peak || cmkpV[x + 1] == peak ||
                     cmkppV[x - 1] == peak || cmkppV[x] == peak || cmkppV[x + 1] == peak ||
                     cmkpnV[x - 1] == peak || cmkpnV[x] == peak || cmkpnV[x + 1] == peak))) {
                    if (d->vi->format->subSamplingW == 0) {
                        cmkp[x] = peak;
                        if (d->vi->format->subSamplingH > 0) {
                            cmkpn[x] = peak;
                            (y & 1 ? cmkpp : cmkpn2)[x] = peak;
                            if (d->vi->format->subSamplingH == 2) {
                                if (y & 1) {
                                    cmkpp2[x] = peak;
                                    cmkpp3[x] = peak;
                                } else {
                                    cmkpn3[x] = peak;
                                    cmkpn4[x] = peak;
                                    cmkpn5[x] = peak;
                                    cmkpn6[x] = peak;
                                }
                            }
                        }
                    } else if (d->vi->format->subSamplingW == 1) {
                        if (d->vi->format->bitsPerSample == 8) {
                            reinterpret_cast<uint16_t *>(cmkp)[x] = UINT16_MAX;
                            if (d->vi->format->subSamplingH > 0) {
                                reinterpret_cast<uint16_t *>(cmkpn)[x] = UINT16_MAX;
                                reinterpret_cast<uint16_t *>(y & 1 ? cmkpp : cmkpn2)[x] = UINT16_MAX;
                                if (d->vi->format->subSamplingH == 2) {
                                    if (y & 1) {
                                        reinterpret_cast<uint16_t *>(cmkpp2)[x] = UINT16_MAX;
                                        reinterpret_cast<uint16_t *>(cmkpp3)[x] = UINT16_MAX;
                                    } else {
                                        reinterpret_cast<uint16_t *>(cmkpn3)[x] = UINT16_MAX;
                                        reinterpret_cast<uint16_t *>(cmkpn4)[x] = UINT16_MAX;
                                        reinterpret_cast<uint16_t *>(cmkpn5)[x] = UINT16_MAX;
                                        reinterpret_cast<uint16_t *>(cmkpn6)[x] = UINT16_MAX;
                                    }
                                }
                            }
                        } else {
                            reinterpret_cast<uint32_t *>(cmkp)[x] = UINT32_MAX;
                            if (d->vi->format->subSamplingH > 0) {
                                reinterpret_cast<uint32_t *>(cmkpn)[x] = UINT32_MAX;
                                reinterpret_cast<uint32_t *>(y & 1 ? cmkpp : cmkpn2)[x] = UINT32_MAX;
                                if (d->vi->format->subSamplingH == 2) {
                                    if (y & 1) {
                                        reinterpret_cast<uint32_t *>(cmkpp2)[x] = UINT32_MAX;
                                        reinterpret_cast<uint32_t *>(cmkpp3)[x] = UINT32_MAX;
                                    } else {
                                        reinterpret_cast<uint32_t *>(cmkpn3)[x] = UINT32_MAX;
                                        reinterpret_cast<uint32_t *>(cmkpn4)[x] = UINT32_MAX;
                                        reinterpret_cast<uint32_t *>(cmkpn5)[x] = UINT32_MAX;
                                        reinterpret_cast<uint32_t *>(cmkpn6)[x] = UINT32_MAX;
                                    }
                                }
                            }
                        }
                    } else {
                        if (d->vi->format->bitsPerSample == 8) {
                            reinterpret_cast<uint32_t *>(cmkp)[x] = UINT32_MAX;
                            if (d->vi->format->subSamplingH > 0) {
                                reinterpret_cast<uint32_t *>(cmkpn)[x] = UINT32_MAX;
                                reinterpret_cast<uint32_t *>(y & 1 ? cmkpp : cmkpn2)[x] = UINT32_MAX;
                                if (d->vi->format->subSamplingH == 2) {
                                    if (y & 1) {
                                        reinterpret_cast<uint32_t *>(cmkpp2)[x] = UINT32_MAX;
                                        reinterpret_cast<uint32_t *>(cmkpp3)[x] = UINT32_MAX;
                                    } else {
                                        reinterpret_cast<uint32_t *>(cmkpn3)[x] = UINT32_MAX;
                                        reinterpret_cast<uint32_t *>(cmkpn4)[x] = UINT32_MAX;
                                        reinterpret_cast<uint32_t *>(cmkpn5)[x] = UINT32_MAX;
                                        reinterpret_cast<uint32_t *>(cmkpn6)[x] = UINT32_MAX;
                                    }
                                }
                            }
                        } else {
                            reinterpret_cast<uint64_t *>(cmkp)[x] = UINT64_MAX;
                            if (d->vi->format->subSamplingH > 0) {
                                reinterpret_cast<uint64_t *>(cmkpn)[x] = UINT64_MAX;
                                reinterpret_cast<uint64_t *>(y & 1 ? cmkpp : cmkpn2)[x] = UINT64_MAX;
                                if (d->vi->format->subSamplingH == 2) {
                                    if (y & 1) {
                                        reinterpret_cast<uint64_t *>(cmkpp2)[x] = UINT64_MAX;
                                        reinterpret_cast<uint64_t *>(cmkpp3)[x] = UINT64_MAX;
                                    } else {
                                        reinterpret_cast<uint64_t *>(cmkpn3)[x] = UINT64_MAX;
                                        reinterpret_cast<uint64_t *>(cmkpn4)[x] = UINT64_MAX;
                                        reinterpret_cast<uint64_t *>(cmkpn5)[x] = UINT64_MAX;
                                        reinterpret_cast<uint64_t *>(cmkpn6)[x] = UINT64_MAX;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    const int width = vsapi->getFrameWidth(cmask, 0);
    const int height = vsapi->getFrameHeight(cmask, 0);
    const int stride = vsapi->getStride(cmask, 0) / d->vi->format->bytesPerSample;
    const T * cmkp = reinterpret_cast<const T *>(vsapi->getReadPtr(cmask, 0)) + stride;
    const T * cmkpp = cmkp - stride;
    const T * cmkpn = cmkp + stride;
    const int xblocks = ((width + d->xhalf) >> d->xshift) + 1;
    const int xblocks4 = xblocks * 4;
    const int yblocks = ((height + d->yhalf) >> d->yshift) + 1;
    const int arraySize = (xblocks * yblocks) * 4;
    memset(cArray, 0, arraySize * sizeof(int));
    const int widtha = (width >> (d->xshift - 1)) << (d->xshift - 1);
    int heighta = (height >> (d->yshift - 1)) << (d->yshift - 1);
    if (heighta == height)
        heighta = height - d->yhalf;
    for (int y = 1; y < d->yhalf; y++) {
        const int temp1 = (y >> d->yshift) * xblocks4;
        const int temp2 = ((y + d->yhalf) >> d->yshift) * xblocks4;
        for (int x = 0; x < width; x++) {
            if (cmkpp[x] == peak && cmkp[x] == peak && cmkpn[x] == peak) {
                const int box1 = (x >> d->xshift) << 2;
                const int box2 = ((x + d->xhalf) >> d->xshift) << 2;
                ++cArray[temp1 + box1];
                ++cArray[temp1 + box2 + 1];
                ++cArray[temp2 + box1 + 2];
                ++cArray[temp2 + box2 + 3];
            }
        }
        cmkpp += stride;
        cmkp += stride;
        cmkpn += stride;
    }
    for (int y = d->yhalf; y < heighta; y += d->yhalf) {
        const int temp1 = (y >> d->yshift) * xblocks4;
        const int temp2 = ((y + d->yhalf) >> d->yshift) * xblocks4;
        for (int x = 0; x < widtha; x += d->xhalf) {
            const T * cmkppT = cmkpp;
            const T * cmkpT = cmkp;
            const T * cmkpnT = cmkpn;
            int sum = 0;
            for (int u = 0; u < d->yhalf; u++) {
                for (int v = 0; v < d->xhalf; v++) {
                    if (cmkppT[x + v] == peak && cmkpT[x + v] == peak && cmkpnT[x + v] == peak)
                        sum++;
                }
                cmkppT += stride;
                cmkpT += stride;
                cmkpnT += stride;
            }
            if (sum) {
                const int box1 = (x >> d->xshift) << 2;
                const int box2 = ((x + d->xhalf) >> d->xshift) << 2;
                cArray[temp1 + box1] += sum;
                cArray[temp1 + box2 + 1] += sum;
                cArray[temp2 + box1 + 2] += sum;
                cArray[temp2 + box2 + 3] += sum;
            }
        }
        for (int x = widtha; x < width; x++) {
            const T * cmkppT = cmkpp;
            const T * cmkpT = cmkp;
            const T * cmkpnT = cmkpn;
            int sum = 0;
            for (int u = 0; u < d->yhalf; u++) {
                if (cmkppT[x] == peak && cmkpT[x] == peak && cmkpnT[x] == peak)
                    sum++;
                cmkppT += stride;
                cmkpT += stride;
                cmkpnT += stride;
            }
            if (sum) {
                const int box1 = (x >> d->xshift) << 2;
                const int box2 = ((x + d->xhalf) >> d->xshift) << 2;
                cArray[temp1 + box1] += sum;
                cArray[temp1 + box2 + 1] += sum;
                cArray[temp2 + box1 + 2] += sum;
                cArray[temp2 + box2 + 3] += sum;
            }
        }
        cmkpp += stride * d->yhalf;
        cmkp += stride * d->yhalf;
        cmkpn += stride * d->yhalf;
    }
    for (int y = heighta; y < height - 1; y++) {
        const int temp1 = (y >> d->yshift) * xblocks4;
        const int temp2 = ((y + d->yhalf) >> d->yshift) * xblocks4;
        for (int x = 0; x < width; x++) {
            if (cmkpp[x] == peak && cmkp[x] == peak && cmkpn[x] == peak) {
                const int box1 = (x >> d->xshift) << 2;
                const int box2 = ((x + d->xhalf) >> d->xshift) << 2;
                ++cArray[temp1 + box1];
                ++cArray[temp1 + box2 + 1];
                ++cArray[temp2 + box1 + 2];
                ++cArray[temp2 + box2 + 3];
            }
        }
        cmkpp += stride;
        cmkp += stride;
        cmkpn += stride;
    }
    int MIC = 0;
    for (int x = 0; x < arraySize; x++) {
        if (cArray[x] > MIC)
            MIC = cArray[x];
    }
    return MIC > d->MI;
}

static bool invokeCache(VSNodeRef ** node, VSMap * out, VSPlugin * stdPlugin, const VSAPI * vsapi) {
    VSMap * args = vsapi->createMap();
    vsapi->propSetNode(args, "clip", *node, paReplace);
    vsapi->freeNode(*node);
    VSMap * ret = vsapi->invoke(stdPlugin, "Cache", args);
    vsapi->freeMap(args);
    if (!vsapi->getError(ret)) {
        *node = vsapi->propGetNode(ret, "clip", 0, nullptr);
        vsapi->freeMap(ret);
        return true;
    } else {
        vsapi->setError(out, vsapi->getError(ret));
        vsapi->freeMap(ret);
        return false;
    }
}

static void VS_CC tdeintmodInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    TDeintModData * d = static_cast<TDeintModData *>(*instanceData);
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static void VS_CC iscombedInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    IsCombedData * d = static_cast<IsCombedData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC tdeintmodCreateMMGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const TDeintModData * d = static_cast<const TDeintModData *>(*instanceData);

    if (activationReason == arInitial) {
        for (int i = 0; i < 3; i++) {
            if (n < d->vi.numFrames - i)
                vsapi->requestFrameFilter(n + i, d->node, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src[3];
        VSFrameRef * msk[3][2];
        VSFrameRef * dst[] = {
            vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height * 2, nullptr, core), vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, nullptr, core)
        };

        if (d->vi.format->bitsPerSample == 8) {
            for (int i = 0; i < 3; i++) {
                src[i] = vsapi->getFrameFilter(std::min(n + i, d->vi.numFrames - 1), d->node, frameCtx);
                msk[i][0] = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height * 2, nullptr, core);
                msk[i][1] = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height * 2, nullptr, core);
#ifdef VS_TARGET_CPU_X86
                threshMask<uint8_t, Vec16uc>(src[i], msk[i][0], d, vsapi);
#else
                threshMask<uint8_t>(src[i], msk[i][0], d, vsapi);
#endif
            }
            for (int i = 0; i < 2; i++)
#ifdef VS_TARGET_CPU_X86
                motionMask<uint8_t, Vec16uc>(src[i], msk[i][0], src[i + 1], msk[i + 1][0], msk[i][1], d, vsapi);
            motionMask<uint8_t, Vec16uc>(src[0], msk[0][0], src[2], msk[2][0], dst[0], d, vsapi);
            andMasks<uint8_t, Vec16uc>(msk[0][1], msk[1][1], dst[0], d, vsapi);
#else
                motionMask<uint8_t>(src[i], msk[i][0], src[i + 1], msk[i + 1][0], msk[i][1], d, vsapi);
            motionMask<uint8_t>(src[0], msk[0][0], src[2], msk[2][0], dst[0], d, vsapi);
            andMasks<uint8_t>(msk[0][1], msk[1][1], dst[0], d, vsapi);
#endif
            combineMasks<uint8_t>(dst[0], dst[1], d, vsapi);
        } else {
            for (int i = 0; i < 3; i++) {
                src[i] = vsapi->getFrameFilter(std::min(n + i, d->vi.numFrames - 1), d->node, frameCtx);
                msk[i][0] = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height * 2, nullptr, core);
                msk[i][1] = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height * 2, nullptr, core);
#ifdef VS_TARGET_CPU_X86
                threshMask<uint16_t, Vec16us>(src[i], msk[i][0], d, vsapi);
#else
                threshMask<uint16_t>(src[i], msk[i][0], d, vsapi);
#endif
            }
            for (int i = 0; i < 2; i++)
#ifdef VS_TARGET_CPU_X86
                motionMask<uint16_t, Vec16us>(src[i], msk[i][0], src[i + 1], msk[i + 1][0], msk[i][1], d, vsapi);
            motionMask<uint16_t, Vec16us>(src[0], msk[0][0], src[2], msk[2][0], dst[0], d, vsapi);
            andMasks<uint16_t, Vec16us>(msk[0][1], msk[1][1], dst[0], d, vsapi);
#else
                motionMask<uint16_t>(src[i], msk[i][0], src[i + 1], msk[i + 1][0], msk[i][1], d, vsapi);
            motionMask<uint16_t>(src[0], msk[0][0], src[2], msk[2][0], dst[0], d, vsapi);
            andMasks<uint16_t>(msk[0][1], msk[1][1], dst[0], d, vsapi);
#endif
            combineMasks<uint16_t>(dst[0], dst[1], d, vsapi);
        }

        for (int i = 0; i < 3; i++) {
            vsapi->freeFrame(src[i]);
            vsapi->freeFrame(msk[i][0]);
            vsapi->freeFrame(msk[i][1]);
        }
        vsapi->freeFrame(dst[0]);
        return dst[1];
    }

    return nullptr;
}

static const VSFrameRef *VS_CC tdeintmodBuildMMGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const TDeintModData * d = static_cast<const TDeintModData *>(*instanceData);

    if (activationReason == arInitial) {
        int fieldt = d->field;
        if (d->mode == 1) {
            fieldt = n & 1 ? 1 - d->order : d->order;
            n /= 2;
        }

        int tstart, tstop, bstart, bstop;
        if (fieldt == 1) {
            tstart = n - (d->length - 1) / 2;
            tstop = n + (d->length - 1) / 2 - 2;
            const int bn = d->order == 1 ? n - 1 : n;
            bstart = bn - (d->length - 2) / 2;
            bstop = bn + 1 + (d->length - 2) / 2 - 2;
        } else {
            const int tn = d->order == 0 ? n - 1 : n;
            tstart = tn - (d->length - 2) / 2;
            tstop = tn + 1 + (d->length - 2) / 2 - 2;
            bstart = n - (d->length - 1) / 2;
            bstop = n + (d->length - 1) / 2 - 2;
        }

        for (int i = tstart; i <= tstop; i++) {
            if (i >= 0 && i < d->viSaved->numFrames - 2)
                vsapi->requestFrameFilter(i, d->node, frameCtx);
        }
        for (int i = bstart; i <= bstop; i++) {
            if (i >= 0 && i < d->viSaved->numFrames - 2)
                vsapi->requestFrameFilter(i, d->node2, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        // In the original version, it's dynamically allocated to the size of (length - 2) and length doesn't have an upper limit
        // Since I set the upper limit of length to 60 in VS port now, I just declare the array to the maximum possible size instead of using dynamic memory allocation
        VSFrameRef * srct[58];
        VSFrameRef * srcb[58];
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, nullptr, core);

        int fieldt = d->field;
        if (d->mode == 1) {
            fieldt = n & 1 ? 1 - d->order : d->order;
            n /= 2;
        }

        int tstart, tstop, bstart, bstop, ccount, ocount;
        VSFrameRef ** csrc, ** osrc;
        if (fieldt == 1) {
            tstart = n - (d->length - 1) / 2;
            tstop = n + (d->length - 1) / 2 - 2;
            const int bn = d->order == 1 ? n - 1 : n;
            bstart = bn - (d->length - 2) / 2;
            bstop = bn + 1 + (d->length - 2) / 2 - 2;
            ocount = tstop - tstart + 1;
            ccount = bstop - bstart + 1;
            osrc = srct;
            csrc = srcb;
        } else {
            const int tn = d->order == 0 ? n - 1 : n;
            tstart = tn - (d->length - 2) / 2;
            tstop = tn + 1 + (d->length - 2) / 2 - 2;
            bstart = n - (d->length - 1) / 2;
            bstop = n + (d->length - 1) / 2 - 2;
            ccount = tstop - tstart + 1;
            ocount = bstop - bstart + 1;
            csrc = srct;
            osrc = srcb;
        }

        for (int i = tstart; i <= tstop; i++) {
            if (i < 0 || i >= d->viSaved->numFrames - 2) {
                srct[i - tstart] = vsapi->newVideoFrame(d->viSaved->format, d->viSaved->width, d->viSaved->height, nullptr, core);
                for (int plane = 0; plane < d->viSaved->format->numPlanes; plane++)
                    memset(vsapi->getWritePtr(srct[i - tstart], plane), 0, vsapi->getStride(srct[i - tstart], plane) * vsapi->getFrameHeight(srct[i - tstart], plane));
            } else {
                const VSFrameRef * src = vsapi->getFrameFilter(i, d->node, frameCtx);
                srct[i - tstart] = vsapi->copyFrame(src, core);
                vsapi->freeFrame(src);
            }
        }
        for (int i = bstart; i <= bstop; i++) {
            if (i < 0 || i >= d->viSaved->numFrames - 2) {
                srcb[i - bstart] = vsapi->newVideoFrame(d->viSaved->format, d->viSaved->width, d->viSaved->height, nullptr, core);
                for (int plane = 0; plane < d->viSaved->format->numPlanes; plane++)
                    memset(vsapi->getWritePtr(srcb[i - bstart], plane), 0, vsapi->getStride(srcb[i - bstart], plane) * vsapi->getFrameHeight(srcb[i - bstart], plane));
            } else {
                const VSFrameRef * src = vsapi->getFrameFilter(i, d->node2, frameCtx);
                srcb[i - bstart] = vsapi->copyFrame(src, core);
                vsapi->freeFrame(src);
            }
        }

        if (d->vi.format->bitsPerSample == 8)
            buildMask<uint8_t>(csrc, osrc, dst, fieldt, ccount, ocount, d, vsapi);
        else
            buildMask<uint16_t>(csrc, osrc, dst, fieldt, ccount, ocount, d, vsapi);

        for (int i = tstart; i <= tstop; i++)
            vsapi->freeFrame(srct[i - tstart]);
        for (int i = bstart; i <= bstop; i++)
            vsapi->freeFrame(srcb[i - bstart]);
        return dst;
    }

    return nullptr;
}

static const VSFrameRef *VS_CC tdeintmodGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const TDeintModData * d = static_cast<const TDeintModData *>(*instanceData);

    if (activationReason == arInitial) {
        const int nSaved = n;
        if (d->mode == 1)
            n /= 2;

        if (d->mask)
            vsapi->requestFrameFilter(nSaved, d->mask, frameCtx);

        if (!d->show) {
            if (n > 0)
                vsapi->requestFrameFilter(n - 1, d->node, frameCtx);
            vsapi->requestFrameFilter(n, d->node, frameCtx);
            if (n < d->viSaved->numFrames - 1)
                vsapi->requestFrameFilter(n + 1, d->node, frameCtx);
            if (d->edeint)
                vsapi->requestFrameFilter(nSaved, d->edeint, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        const int nSaved = n;
        int fieldt = d->field;
        if (d->mode == 1) {
            fieldt = n & 1 ? 1 - d->order : d->order;
            n /= 2;
        }

        VSFrameRef * mask;
        if (d->mask) {
            const VSFrameRef * msk = vsapi->getFrameFilter(nSaved, d->mask, frameCtx);
            mask = vsapi->copyFrame(msk, core);
            vsapi->freeFrame(msk);
        } else {
            mask = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, nullptr, core);
            if (d->vi.format->bitsPerSample == 8)
                setMaskForUpsize<uint8_t>(mask, fieldt, d, vsapi);
            else
                setMaskForUpsize<uint16_t>(mask, fieldt, d, vsapi);
        }
        if (d->show)
            return mask;

        const VSFrameRef * prv = vsapi->getFrameFilter(std::max(n - 1, 0), d->node, frameCtx);
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * nxt = vsapi->getFrameFilter(std::min(n + 1, d->viSaved->numFrames - 1), d->node, frameCtx);
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);

        if (d->edeint) {
            const VSFrameRef * efrm = vsapi->getFrameFilter(nSaved, d->edeint, frameCtx);
            if (d->vi.format->bitsPerSample == 8)
                eDeint<uint8_t>(dst, mask, prv, src, nxt, efrm, d, vsapi);
            else
                eDeint<uint16_t>(dst, mask, prv, src, nxt, efrm, d, vsapi);
            vsapi->freeFrame(efrm);
        } else {
            if (d->vi.format->bitsPerSample == 8)
                cubicDeint<uint8_t>(dst, mask, prv, src, nxt, d, vsapi);
            else
                cubicDeint<uint16_t>(dst, mask, prv, src, nxt, d, vsapi);
        }

        vsapi->freeFrame(mask);
        vsapi->freeFrame(prv);
        vsapi->freeFrame(src);
        vsapi->freeFrame(nxt);
        return dst;
    }

    return nullptr;
}

static const VSFrameRef *VS_CC iscombedGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const IsCombedData * d = static_cast<const IsCombedData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        int * cArray = vs_aligned_malloc<int>((((d->vi->width + d->xhalf) >> d->xshift) + 1) * (((d->vi->height + d->yhalf) >> d->yshift) + 1) * 4 * sizeof(int), 32);
        if (!cArray) {
            vsapi->setFilterError("IsCombed: malloc failure (cArray)", frameCtx);
            return nullptr;
        }

        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef * cmask = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, nullptr, core);
        VSFrameRef * dst = vsapi->copyFrame(src, core);

        if (d->vi->format->bitsPerSample == 8)
            vsapi->propSetInt(vsapi->getFramePropsRW(dst), "_Combed", checkCombed<uint8_t>(src, cmask, cArray, d, vsapi), paReplace);
        else
            vsapi->propSetInt(vsapi->getFramePropsRW(dst), "_Combed", checkCombed<uint16_t>(src, cmask, cArray, d, vsapi), paReplace);

        vs_aligned_free(cArray);
        vsapi->freeFrame(src);
        vsapi->freeFrame(cmask);
        return dst;
    }

    return nullptr;
}

static void VS_CC tdeintmodCreateMMFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TDeintModData * d = static_cast<TDeintModData *>(instanceData);
    vsapi->freeNode(d->node);
    delete d;
}

static void VS_CC tdeintmodBuildMMFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TDeintModData * d = static_cast<TDeintModData *>(instanceData);
    vsapi->freeNode(d->node);
    vsapi->freeNode(d->node2);
    delete d;
}

static void VS_CC tdeintmodFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TDeintModData * d = static_cast<TDeintModData *>(instanceData);
    vsapi->freeNode(d->node);
    vsapi->freeNode(d->mask);
    vsapi->freeNode(d->edeint);
    delete d;
}

static void VS_CC iscombedFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    IsCombedData * d = static_cast<IsCombedData *>(instanceData);
    vsapi->freeNode(d->node);
    delete d;
}

static void VS_CC tdeintmodCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    TDeintModData d;
    int err;

    d.order = !!vsapi->propGetInt(in, "order", 0, nullptr);
    d.field = !!vsapi->propGetInt(in, "field", 0, &err);
    if (err)
        d.field = d.order;
    d.mode = !!vsapi->propGetInt(in, "mode", 0, &err);
    d.length = int64ToIntS(vsapi->propGetInt(in, "length", 0, &err));
    if (err)
        d.length = 10;
    d.mtype = int64ToIntS(vsapi->propGetInt(in, "mtype", 0, &err));
    if (err)
        d.mtype = 1;
    d.ttype = int64ToIntS(vsapi->propGetInt(in, "ttype", 0, &err));
    if (err)
        d.ttype = 1;
    d.mtqL = int64ToIntS(vsapi->propGetInt(in, "mtql", 0, &err));
    if (err)
        d.mtqL = -1;
    d.mthL = int64ToIntS(vsapi->propGetInt(in, "mthl", 0, &err));
    if (err)
        d.mthL = -1;
    d.mtqC = int64ToIntS(vsapi->propGetInt(in, "mtqc", 0, &err));
    if (err)
        d.mtqC = -1;
    d.mthC = int64ToIntS(vsapi->propGetInt(in, "mthc", 0, &err));
    if (err)
        d.mthC = -1;
    d.nt = int64ToIntS(vsapi->propGetInt(in, "nt", 0, &err));
    if (err)
        d.nt = 2;
    d.minthresh = int64ToIntS(vsapi->propGetInt(in, "minthresh", 0, &err));
    if (err)
        d.minthresh = 4;
    d.maxthresh = int64ToIntS(vsapi->propGetInt(in, "maxthresh", 0, &err));
    if (err)
        d.maxthresh = 75;
    d.cstr = int64ToIntS(vsapi->propGetInt(in, "cstr", 0, &err));
    if (err)
        d.cstr = 4;
    d.show = !!vsapi->propGetInt(in, "show", 0, &err);

    if (d.length < 6 || d.length > 60) {
        vsapi->setError(out, "TDeintMod: length must be between 6 and 60 inclusive");
        return;
    }
    if (d.mtype < 0 || d.mtype > 2) {
        vsapi->setError(out, "TDeintMod: mtype must be 0, 1, or 2");
        return;
    }
    if (d.ttype < 0 || d.ttype > 5) {
        vsapi->setError(out, "TDeintMod: ttype must be 0, 1, 2, 3, 4, or 5");
        return;
    }
    if (d.mtqL < -2 || d.mtqL > 255) {
        vsapi->setError(out, "TDeintMod: mtql must be between -2 and 255 inclusive");
        return;
    }
    if (d.mthL < -2 || d.mthL > 255) {
        vsapi->setError(out, "TDeintMod: mthl must be between -2 and 255 inclusive");
        return;
    }
    if (d.mtqC < -2 || d.mtqC > 255) {
        vsapi->setError(out, "TDeintMod: mtqc must be between -2 and 255 inclusive");
        return;
    }
    if (d.mthC < -2 || d.mthC > 255) {
        vsapi->setError(out, "TDeintMod: mthc must be between -2 and 255 inclusive");
        return;
    }
    if (d.minthresh < 0 || d.minthresh > 255) {
        vsapi->setError(out, "TDeintMod: minthresh must be between 0 and 255 inclusive");
        return;
    }
    if (d.maxthresh < 0 || d.maxthresh > 255) {
        vsapi->setError(out, "TDeintMod: maxthresh must be between 0 and 255 inclusive");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = *vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(&d.vi) || !d.vi.numFrames || d.vi.format->sampleType != stInteger || d.vi.format->bitsPerSample > 16) {
        vsapi->setError(out, "TDeintMod: only constant format 8-16 bits integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi.width & 1 || d.vi.height & 1) {
        vsapi->setError(out, "TDeintMod: width and height must be multiples of 2");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi.format->bitsPerSample > 8) {
        const int shift = d.vi.format->bitsPerSample - 8;
        if (d.mtqL > -1)
            d.mtqL <<= shift;
        if (d.mthL > -1)
            d.mthL <<= shift;
        if (d.mtqC > -1)
            d.mtqC <<= shift;
        if (d.mthC > -1)
            d.mthC <<= shift;
        d.nt <<= shift;
        d.minthresh <<= shift;
        d.maxthresh <<= shift;
    }

    const int shift = 16 - d.vi.format->bitsPerSample;
    d.ten = ((10 << 8) + 10) >> shift;
    d.twenty = ((20 << 8) + 20) >> shift;
    d.thirty = ((30 << 8) + 30) >> shift;
    d.forty = ((40 << 8) + 40) >> shift;
    d.fifty = ((50 << 8) + 50) >> shift;
    d.sixty = ((60 << 8) + 60) >> shift;
    d.seventy = ((70 << 8) + 70) >> shift;

    d.mask = nullptr;
    if (d.mtqL > -2 || d.mthL > -2 || d.mtqC > -2 || d.mthC > -2) {
        VSMap * args = vsapi->createMap();
        VSPlugin * stdPlugin = vsapi->getPluginById("com.vapoursynth.std", core);

        vsapi->propSetNode(args, "clip", d.node, paReplace);
        vsapi->freeNode(d.node);
        vsapi->propSetInt(args, "tff", 1, paReplace);
        VSMap * ret = vsapi->invoke(stdPlugin, "SeparateFields", args);
        if (vsapi->getError(ret)) {
            vsapi->setError(out, vsapi->getError(ret));
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
            return;
        }
        VSNodeRef * separated = vsapi->propGetNode(ret, "clip", 0, nullptr);
        vsapi->clearMap(args);
        vsapi->freeMap(ret);

        vsapi->propSetNode(args, "clip", separated, paReplace);
        vsapi->propSetInt(args, "cycle", 2, paReplace);
        vsapi->propSetInt(args, "offsets", 0, paReplace);
        ret = vsapi->invoke(stdPlugin, "SelectEvery", args);
        if (vsapi->getError(ret)) {
            vsapi->setError(out, vsapi->getError(ret));
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
            vsapi->freeNode(separated);
            return;
        }
        d.node = vsapi->propGetNode(ret, "clip", 0, nullptr);
        d.vi = *vsapi->getVideoInfo(d.node);
        vsapi->clearMap(args);
        vsapi->freeMap(ret);

        TDeintModData * data = new TDeintModData(d);

        vsapi->createFilter(in, out, "TDeintMod", tdeintmodInit, tdeintmodCreateMMGetFrame, tdeintmodCreateMMFree, fmParallel, 0, data, core);
        VSNodeRef * temp = vsapi->propGetNode(out, "clip", 0, nullptr);
        vsapi->clearMap(out);
        if (!invokeCache(&temp, out, stdPlugin, vsapi))
            return;

        vsapi->propSetNode(args, "clip", separated, paReplace);
        vsapi->freeNode(separated);
        vsapi->propSetInt(args, "cycle", 2, paReplace);
        vsapi->propSetInt(args, "offsets", 1, paReplace);
        ret = vsapi->invoke(stdPlugin, "SelectEvery", args);
        if (vsapi->getError(ret)) {
            vsapi->setError(out, vsapi->getError(ret));
            vsapi->freeMap(args);
            vsapi->freeMap(ret);
            vsapi->freeNode(temp);
            return;
        }
        d.node = vsapi->propGetNode(ret, "clip", 0, nullptr);
        d.vi = *vsapi->getVideoInfo(d.node);
        vsapi->freeMap(args);
        vsapi->freeMap(ret);

        data = new TDeintModData(d);

        vsapi->createFilter(in, out, "TDeintMod", tdeintmodInit, tdeintmodCreateMMGetFrame, tdeintmodCreateMMFree, fmParallel, 0, data, core);
        d.node2 = vsapi->propGetNode(out, "clip", 0, nullptr);
        vsapi->clearMap(out);
        if (!invokeCache(&d.node2, out, stdPlugin, vsapi))
            return;

        d.node = temp;
        d.vi = *vsapi->getVideoInfo(d.node);
        d.viSaved = vsapi->getVideoInfo(d.node);

        d.vi.height *= 2;
        if (d.mode == 1) {
            d.vi.numFrames *= 2;
            d.vi.fpsNum *= 2;
        }

        for (int i = 0; i < d.length; i++)
            d.gvlut[i] = i == 0 ? 1 : (i == d.length - 1 ? 4 : 2);

        if (d.mtype == 0) {
            d.vlut = {
                0, 1, 2, 2, 3, 0, 2, 2,
                1, 1, 2, 2, 0, 1, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2,
                3, 0, 2, 2, 3, 3, 2, 2,
                0, 1, 2, 2, 3, 1, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2
            };
        } else if (d.mtype == 1) {
            d.vlut = {
                0, 0, 2, 2, 0, 0, 2, 2,
                0, 1, 2, 2, 0, 1, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2,
                0, 0, 2, 2, 3, 3, 2, 2,
                0, 1, 2, 2, 3, 1, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2
            };
        } else {
            d.vlut = {
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 1, 0, 1, 0, 1, 0, 1,
                0, 0, 2, 2, 0, 0, 2, 2,
                0, 1, 2, 2, 0, 1, 2, 2,
                0, 0, 0, 0, 3, 3, 3, 3,
                0, 1, 0, 1, 3, 1, 3, 1,
                0, 0, 2, 2, 3, 3, 2, 2,
                0, 1, 2, 2, 3, 1, 2, 2
            };
        }

        d.tmmlut16 = {
            d.sixty, d.twenty, d.fifty, d.ten, d.sixty, d.ten, d.forty, d.thirty,
            d.sixty, d.ten, d.forty, d.thirty, d.sixty, d.twenty, d.fifty, d.ten
        };

        data = new TDeintModData(d);

        vsapi->createFilter(in, out, "TDeintMod", tdeintmodInit, tdeintmodBuildMMGetFrame, tdeintmodBuildMMFree, fmParallel, 0, data, core);
        d.mask = vsapi->propGetNode(out, "clip", 0, nullptr);
        vsapi->clearMap(out);
        if (!invokeCache(&d.mask, out, stdPlugin, vsapi))
            return;
    }

    if (d.mask)
        d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.edeint = vsapi->propGetNode(in, "edeint", 0, &err);
    d.vi = *vsapi->getVideoInfo(d.node);
    d.viSaved = vsapi->getVideoInfo(d.node);

    if (d.edeint) {
        if (!isSameFormat(vsapi->getVideoInfo(d.edeint), d.viSaved)) {
            vsapi->setError(out, "TDeintMod: edeint clip must have the same dimensions as main clip and be the same format");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.mask);
            vsapi->freeNode(d.edeint);
            return;
        }

        if (vsapi->getVideoInfo(d.edeint)->numFrames != d.viSaved->numFrames * (d.mode == 0 ? 1 : 2)) {
            vsapi->setError(out, "TDeintMod: edeint clip's number of frames doesn't match");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.mask);
            vsapi->freeNode(d.edeint);
            return;
        }
    }

    if (d.mode == 1) {
        d.vi.numFrames *= 2;
        d.vi.fpsNum *= 2;
    }

    TDeintModData * data = new TDeintModData(d);

    vsapi->createFilter(in, out, "TDeintMod", tdeintmodInit, tdeintmodGetFrame, tdeintmodFree, fmParallel, 0, data, core);
}

static void VS_CC iscombedCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    IsCombedData d;
    int err;

    d.cthresh = int64ToIntS(vsapi->propGetInt(in, "cthresh", 0, &err));
    if (err)
        d.cthresh = 6;
    d.blockx = int64ToIntS(vsapi->propGetInt(in, "blockx", 0, &err));
    if (err)
        d.blockx = 16;
    d.blocky = int64ToIntS(vsapi->propGetInt(in, "blocky", 0, &err));
    if (err)
        d.blocky = 16;
    d.chroma = !!vsapi->propGetInt(in, "chroma", 0, &err);
    d.MI = int64ToIntS(vsapi->propGetInt(in, "mi", 0, &err));
    if (err)
        d.MI = 64;
    d.metric = !!vsapi->propGetInt(in, "metric", 0, &err);

    if (d.blockx < 4 || d.blockx > 2048 || !isPowerOf2(d.blockx)) {
        vsapi->setError(out, "IsCombed: illegal blockx size");
        return;
    }
    if (d.blocky < 4 || d.blocky > 2048 || !isPowerOf2(d.blocky)) {
        vsapi->setError(out, "IsCombed: illegal blocky size");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bitsPerSample > 16) {
        vsapi->setError(out, "IsCombed: only constant format 8-16 bits integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi->format->numPlanes == 1)
        d.chroma = false;

    if (d.vi->format->bitsPerSample > 8)
        d.cthresh <<= d.vi->format->bitsPerSample - 8;

    d.xhalf = d.blockx / 2;
    d.yhalf = d.blocky / 2;
    d.xshift = static_cast<int>(std::log2(d.blockx));
    d.yshift = static_cast<int>(std::log2(d.blocky));
    d.cthresh6 = d.cthresh * 6;
    d.cthreshsq = d.cthresh * d.cthresh;

    IsCombedData * data = new IsCombedData(d);

    vsapi->createFilter(in, out, "IsCombed", iscombedInit, iscombedGetFrame, iscombedFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.tdeintmod", "tdm", "A bi-directionally motion adaptive deinterlacer", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("TDeintMod",
                 "clip:clip;order:int;field:int:opt;mode:int:opt;"
                 "length:int:opt;mtype:int:opt;ttype:int:opt;mtql:int:opt;mthl:int:opt;mtqc:int:opt;mthc:int:opt;nt:int:opt;minthresh:int:opt;maxthresh:int:opt;cstr:int:opt;show:int:opt;"
                 "edeint:clip:opt;",
                 tdeintmodCreate, nullptr, plugin);
    registerFunc("IsCombed", "clip:clip;cthresh:int:opt;blockx:int:opt;blocky:int:opt;chroma:int:opt;mi:int:opt;metric:int:opt;", iscombedCreate, nullptr, plugin);
}
