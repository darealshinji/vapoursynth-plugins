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
    VSNodeRef * node, * node2, * mask, * clip2, * edeint;
    VSVideoInfo vi;
    const VSVideoInfo * viSaved;
    int order, field, mode, length, mtype, ttype, mtqL, mthL, mtqC, mthC, nt, minthresh, maxthresh, cstr, cthresh, blockx, blocky, MI, metric;
    bool show, full, chroma;
    int8_t * offplut[3], * offnlut[3], gvlut[60];
    uint8_t mlut[256];
    std::vector<int> vlut, tmmlut16;
    int xhalf, yhalf, xshift, yshift, cthresh6, cthreshsq;
    bool useClip2;
};

static inline bool isPowerOf2(int i) {
    return i && !(i & (i - 1));
}

#ifdef VS_TARGET_CPU_X86
static inline Vec16uc abs_dif(const Vec16uc & a, const Vec16uc & b) {
    return sub_saturated(a, b) | sub_saturated(b, a);
}

static inline void threshMask(const VSFrameRef * src, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane);
        const uint8_t * srcp_ = vsapi->getReadPtr(src, plane);
        uint8_t * dstp0 = vsapi->getWritePtr(dst, plane);
        uint8_t * dstp1 = dstp0 + stride * height;
        if (plane == 0 && d->mtqL > -1 && d->mthL > -1) {
            memset(dstp0, d->mtqL, stride * height);
            memset(dstp1, d->mthL, stride * height);
            continue;
        } else if (plane > 0 && d->mtqC > -1 && d->mthC > -1) {
            memset(dstp0, d->mtqC, stride * height);
            memset(dstp1, d->mthC, stride * height);
            continue;
        }
        const int hs = plane ? d->vi.format->subSamplingW : 0;
        const int vs = plane ? 1 << d->vi.format->subSamplingH : 1;
        const int vss = 1 << (vs - 1);
        const int8_t * offpt = d->offplut[plane];
        const int8_t * offnt = d->offnlut[plane];
        if (d->ttype == 0) { // 4 neighbors - compensated
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const uint8_t * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    Vec16uc min0(255), max0(0);
                    Vec16uc min1(255), max1(0);
                    const Vec16uc srcp = Vec16uc().load_a(srcp_ + x);
                    const Vec16uc srcpmp = Vec16uc().load(srcp_ + x - 1);
                    const Vec16uc srcppn = Vec16uc().load(srcp_ + x + 1);
                    const Vec16uc srcpp = Vec16uc().load_a(srcpp_ + x);
                    const Vec16uc srcpn = Vec16uc().load_a(srcpn_ + x);
                    min0 = select(srcpp < min0, srcpp, min0);
                    max0 = select(srcpp > max0, srcpp, max0);
                    min1 = select(srcpmp < min1, srcpmp, min1);
                    max1 = select(srcpmp > max1, srcpmp, max1);
                    min1 = select(srcppn < min1, srcppn, min1);
                    max1 = select(srcppn > max1, srcppn, max1);
                    min0 = select(srcpn < min0, srcpn, min0);
                    max0 = select(srcpn > max0, srcpn, max0);
                    const Vec16uc atv = max((abs_dif(srcp, min0) + vss) >> vs, (abs_dif(srcp, max0) + vss) >> vs);
                    const Vec16uc ath = max((abs_dif(srcp, min1) + hs) >> hs, (abs_dif(srcp, max1) + hs) >> hs);
                    const Vec16uc atmax = max(atv, ath);
                    ((atmax + 2) >> 2).store_a(dstp0 + x);
                    ((atmax + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
                    int min1 = 256, max1 = -1;
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
                    const int atv = std::max((std::abs(srcp_[x] - min0) + vss) >> vs, (std::abs(srcp_[x] - max0) + vss) >> vs);
                    const int ath = std::max((std::abs(srcp_[x] - min1) + hs) >> hs, (std::abs(srcp_[x] - max1) + hs) >> hs);
                    const int atmax = std::max(atv, ath);
                    dstp0[x] = (atmax + 2) >> 2;
                    dstp1[x] = (atmax + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 1) { // 8 neighbors - compensated
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const uint8_t * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    Vec16uc min0(255), max0(0);
                    Vec16uc min1(255), max1(0);
                    const Vec16uc srcp = Vec16uc().load_a(srcp_ + x);
                    const Vec16uc srcpmp = Vec16uc().load(srcp_ + x - 1);
                    const Vec16uc srcppn = Vec16uc().load(srcp_ + x + 1);
                    const Vec16uc srcpp = Vec16uc().load_a(srcpp_ + x);
                    const Vec16uc srcppmp = Vec16uc().load(srcpp_ + x - 1);
                    const Vec16uc srcpppn = Vec16uc().load(srcpp_ + x + 1);
                    const Vec16uc srcpn = Vec16uc().load_a(srcpn_ + x);
                    const Vec16uc srcpnmp = Vec16uc().load(srcpn_ + x - 1);
                    const Vec16uc srcpnpn = Vec16uc().load(srcpn_ + x + 1);
                    min0 = select(srcppmp < min0, srcppmp, min0);
                    max0 = select(srcppmp > max0, srcppmp, max0);
                    min0 = select(srcpp < min0, srcpp, min0);
                    max0 = select(srcpp > max0, srcpp, max0);
                    min0 = select(srcpppn < min0, srcpppn, min0);
                    max0 = select(srcpppn > max0, srcpppn, max0);
                    min1 = select(srcpmp < min1, srcpmp, min1);
                    max1 = select(srcpmp > max1, srcpmp, max1);
                    min1 = select(srcppn < min1, srcppn, min1);
                    max1 = select(srcppn > max1, srcppn, max1);
                    min0 = select(srcpnmp < min0, srcpnmp, min0);
                    max0 = select(srcpnmp > max0, srcpnmp, max0);
                    min0 = select(srcpn < min0, srcpn, min0);
                    max0 = select(srcpn > max0, srcpn, max0);
                    min0 = select(srcpnpn < min0, srcpnpn, min0);
                    max0 = select(srcpnpn > max0, srcpnpn, max0);
                    const Vec16uc atv = max((abs_dif(srcp, min0) + vss) >> vs, (abs_dif(srcp, max0) + vss) >> vs);
                    const Vec16uc ath = max((abs_dif(srcp, min1) + hs) >> hs, (abs_dif(srcp, max1) + hs) >> hs);
                    const Vec16uc atmax = max(atv, ath);
                    ((atmax + 2) >> 2).store_a(dstp0 + x);
                    ((atmax + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
                    int min1 = 256, max1 = -1;
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
                    const int atv = std::max((std::abs(srcp_[x] - min0) + vss) >> vs, (std::abs(srcp_[x] - max0) + vss) >> vs);
                    const int ath = std::max((std::abs(srcp_[x] - min1) + hs) >> hs, (std::abs(srcp_[x] - max1) + hs) >> hs);
                    const int atmax = std::max(atv, ath);
                    dstp0[x] = (atmax + 2) >> 2;
                    dstp1[x] = (atmax + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 2) { // 4 neighbors - not compensated
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const uint8_t * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    Vec16uc min0(255), max0(0);
                    const Vec16uc srcp = Vec16uc().load_a(srcp_ + x);
                    const Vec16uc srcpmp = Vec16uc().load(srcp_ + x - 1);
                    const Vec16uc srcppn = Vec16uc().load(srcp_ + x + 1);
                    const Vec16uc srcpp = Vec16uc().load_a(srcpp_ + x);
                    const Vec16uc srcpn = Vec16uc().load_a(srcpn_ + x);
                    min0 = select(srcpp < min0, srcpp, min0);
                    max0 = select(srcpp > max0, srcpp, max0);
                    min0 = select(srcpmp < min0, srcpmp, min0);
                    max0 = select(srcpmp > max0, srcpmp, max0);
                    min0 = select(srcppn < min0, srcppn, min0);
                    max0 = select(srcppn > max0, srcppn, max0);
                    min0 = select(srcpn < min0, srcpn, min0);
                    max0 = select(srcpn > max0, srcpn, max0);
                    const Vec16uc at = max(abs_dif(srcp, min0), abs_dif(srcp, max0));
                    ((at + 2) >> 2).store_a(dstp0 + x);
                    ((at + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
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
                    const int at = std::max(std::abs(srcp_[x] - min0), std::abs(srcp_[x] - max0));
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 3) { // 8 neighbors - not compensated
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const uint8_t * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    Vec16uc min0(255), max0(0);
                    const Vec16uc srcp = Vec16uc().load_a(srcp_ + x);
                    const Vec16uc srcpmp = Vec16uc().load(srcp_ + x - 1);
                    const Vec16uc srcppn = Vec16uc().load(srcp_ + x + 1);
                    const Vec16uc srcpp = Vec16uc().load_a(srcpp_ + x);
                    const Vec16uc srcppmp = Vec16uc().load(srcpp_ + x - 1);
                    const Vec16uc srcpppn = Vec16uc().load(srcpp_ + x + 1);
                    const Vec16uc srcpn = Vec16uc().load_a(srcpn_ + x);
                    const Vec16uc srcpnmp = Vec16uc().load(srcpn_ + x - 1);
                    const Vec16uc srcpnpn = Vec16uc().load(srcpn_ + x + 1);
                    min0 = select(srcppmp < min0, srcppmp, min0);
                    max0 = select(srcppmp > max0, srcppmp, max0);
                    min0 = select(srcpp < min0, srcpp, min0);
                    max0 = select(srcpp > max0, srcpp, max0);
                    min0 = select(srcpppn < min0, srcpppn, min0);
                    max0 = select(srcpppn > max0, srcpppn, max0);
                    min0 = select(srcpmp < min0, srcpmp, min0);
                    max0 = select(srcpmp > max0, srcpmp, max0);
                    min0 = select(srcppn < min0, srcppn, min0);
                    max0 = select(srcppn > max0, srcppn, max0);
                    min0 = select(srcpnmp < min0, srcpnmp, min0);
                    max0 = select(srcpnmp > max0, srcpnmp, max0);
                    min0 = select(srcpn < min0, srcpn, min0);
                    max0 = select(srcpn > max0, srcpn, max0);
                    min0 = select(srcpnpn < min0, srcpnpn, min0);
                    max0 = select(srcpnpn > max0, srcpnpn, max0);
                    const Vec16uc at = max(abs_dif(srcp, min0), abs_dif(srcp, max0));
                    ((at + 2) >> 2).store_a(dstp0 + x);
                    ((at + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
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
                    const int at = std::max(std::abs(srcp_[x] - min0), std::abs(srcp_[x] - max0));
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 4) { // 4 neighbors - not compensated (range)
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const uint8_t * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    Vec16uc min0(255), max0(0);
                    const Vec16uc srcp = Vec16uc().load_a(srcp_ + x);
                    const Vec16uc srcpmp = Vec16uc().load(srcp_ + x - 1);
                    const Vec16uc srcppn = Vec16uc().load(srcp_ + x + 1);
                    const Vec16uc srcpp = Vec16uc().load_a(srcpp_ + x);
                    const Vec16uc srcpn = Vec16uc().load_a(srcpn_ + x);
                    min0 = select(srcpp < min0, srcpp, min0);
                    max0 = select(srcpp > max0, srcpp, max0);
                    min0 = select(srcpmp < min0, srcpmp, min0);
                    max0 = select(srcpmp > max0, srcpmp, max0);
                    min0 = select(srcp < min0, srcp, min0);
                    max0 = select(srcp > max0, srcp, max0);
                    min0 = select(srcppn < min0, srcppn, min0);
                    max0 = select(srcppn > max0, srcppn, max0);
                    min0 = select(srcpn < min0, srcpn, min0);
                    max0 = select(srcpn > max0, srcpn, max0);
                    const Vec16uc at = max0 - min0;
                    ((at + 2) >> 2).store_a(dstp0 + x);
                    ((at + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
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
                    const int at = max0 - min0;
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 5) { // 8 neighbors - not compensated (range)
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp_ = srcp_ - (y == 0 ? -stride : stride);
                const uint8_t * srcpn_ = srcp_ + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x += 16) {
                    Vec16uc min0(255), max0(0);
                    const Vec16uc srcp = Vec16uc().load_a(srcp_ + x);
                    const Vec16uc srcpmp = Vec16uc().load(srcp_ + x - 1);
                    const Vec16uc srcppn = Vec16uc().load(srcp_ + x + 1);
                    const Vec16uc srcpp = Vec16uc().load_a(srcpp_ + x);
                    const Vec16uc srcppmp = Vec16uc().load(srcpp_ + x - 1);
                    const Vec16uc srcpppn = Vec16uc().load(srcpp_ + x + 1);
                    const Vec16uc srcpn = Vec16uc().load_a(srcpn_ + x);
                    const Vec16uc srcpnmp = Vec16uc().load(srcpn_ + x - 1);
                    const Vec16uc srcpnpn = Vec16uc().load(srcpn_ + x + 1);
                    min0 = select(srcppmp < min0, srcppmp, min0);
                    max0 = select(srcppmp > max0, srcppmp, max0);
                    min0 = select(srcpp < min0, srcpp, min0);
                    max0 = select(srcpp > max0, srcpp, max0);
                    min0 = select(srcpppn < min0, srcpppn, min0);
                    max0 = select(srcpppn > max0, srcpppn, max0);
                    min0 = select(srcpmp < min0, srcpmp, min0);
                    max0 = select(srcpmp > max0, srcpmp, max0);
                    min0 = select(srcp < min0, srcp, min0);
                    max0 = select(srcp > max0, srcp, max0);
                    min0 = select(srcppn < min0, srcppn, min0);
                    max0 = select(srcppn > max0, srcppn, max0);
                    min0 = select(srcpnmp < min0, srcpnmp, min0);
                    max0 = select(srcpnmp > max0, srcpnmp, max0);
                    min0 = select(srcpn < min0, srcpn, min0);
                    max0 = select(srcpn > max0, srcpn, max0);
                    min0 = select(srcpnpn < min0, srcpnpn, min0);
                    max0 = select(srcpnpn > max0, srcpnpn, max0);
                    const Vec16uc at = max0 - min0;
                    ((at + 2) >> 2).store_a(dstp0 + x);
                    ((at + 1) >> 1).store_a(dstp1 + x);
                }
                const int offx[] = { 0, width - 1 };
                for (int i = 0; i < 2; i++) {
                    const int x = offx[i];
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
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
                    const int at = max0 - min0;
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp_ += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        }
        if (plane == 0 && d->mtqL > -1)
            memset(vsapi->getWritePtr(dst, plane), d->mtqL, stride * height);
        else if (plane == 0 && d->mthL > -1)
            memset(vsapi->getWritePtr(dst, plane) + stride * height, d->mthL, stride * height);
        else if (plane > 0 && d->mtqC > -1)
            memset(vsapi->getWritePtr(dst, plane), d->mtqC, stride * height);
        else if (plane > 0 && d->mthC > -1)
            memset(vsapi->getWritePtr(dst, plane) + stride * height, d->mthC, stride * height);
    }
}
#else
static inline void threshMask(const VSFrameRef * src, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane);
        const uint8_t * srcp = vsapi->getReadPtr(src, plane);
        uint8_t * dstp0 = vsapi->getWritePtr(dst, plane);
        uint8_t * dstp1 = dstp0 + stride * height;
        if (plane == 0 && d->mtqL > -1 && d->mthL > -1) {
            memset(dstp0, d->mtqL, stride * height);
            memset(dstp1, d->mthL, stride * height);
            continue;
        } else if (plane > 0 && d->mtqC > -1 && d->mthC > -1) {
            memset(dstp0, d->mtqC, stride * height);
            memset(dstp1, d->mthC, stride * height);
            continue;
        }
        const int hs = plane ? d->vi.format->subSamplingW : 0;
        const int vs = plane ? 1 << d->vi.format->subSamplingH : 1;
        const int vss = 1 << (vs - 1);
        const int8_t * offpt = d->offplut[plane];
        const int8_t * offnt = d->offnlut[plane];
        if (d->ttype == 0) { // 4 neighbors - compensated
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp = srcp - (y == 0 ? -stride : stride);
                const uint8_t * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
                    int min1 = 256, max1 = -1;
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
                    const int atv = std::max((std::abs(srcp[x] - min0) + vss) >> vs, (std::abs(srcp[x] - max0) + vss) >> vs);
                    const int ath = std::max((std::abs(srcp[x] - min1) + hs) >> hs, (std::abs(srcp[x] - max1) + hs) >> hs);
                    const int atmax = std::max(atv, ath);
                    dstp0[x] = (atmax + 2) >> 2;
                    dstp1[x] = (atmax + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 1) { // 8 neighbors - compensated
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp = srcp - (y == 0 ? -stride : stride);
                const uint8_t * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
                    int min1 = 256, max1 = -1;
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
                    const int atv = std::max((std::abs(srcp[x] - min0) + vss) >> vs, (std::abs(srcp[x] - max0) + vss) >> vs);
                    const int ath = std::max((std::abs(srcp[x] - min1) + hs) >> hs, (std::abs(srcp[x] - max1) + hs) >> hs);
                    const int atmax = std::max(atv, ath);
                    dstp0[x] = (atmax + 2) >> 2;
                    dstp1[x] = (atmax + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 2) { // 4 neighbors - not compensated
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp = srcp - (y == 0 ? -stride : stride);
                const uint8_t * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
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
                    const int at = std::max(std::abs(srcp[x] - min0), std::abs(srcp[x] - max0));
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 3) { // 8 neighbors - not compensated
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp = srcp - (y == 0 ? -stride : stride);
                const uint8_t * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
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
                    const int at = std::max(std::abs(srcp[x] - min0), std::abs(srcp[x] - max0));
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 4) { // 4 neighbors - not compensated (range)
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp = srcp - (y == 0 ? -stride : stride);
                const uint8_t * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
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
                    const int at = max0 - min0;
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        } else if (d->ttype == 5) { // 8 neighbors - not compensated (range)
            for (int y = 0; y < height; y++) {
                const uint8_t * srcpp = srcp - (y == 0 ? -stride : stride);
                const uint8_t * srcpn = srcp + (y == height - 1 ? -stride : stride);
                for (int x = 0; x < width; x++) {
                    const int offp = offpt[x];
                    const int offn = offnt[x];
                    int min0 = 256, max0 = -1;
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
                    const int at = max0 - min0;
                    dstp0[x] = (at + 2) >> 2;
                    dstp1[x] = (at + 1) >> 1;
                }
                srcp += stride;
                dstp0 += stride;
                dstp1 += stride;
            }
        }
        if (plane == 0 && d->mtqL > -1)
            memset(vsapi->getWritePtr(dst, plane), d->mtqL, stride * height);
        else if (plane == 0 && d->mthL > -1)
            memset(vsapi->getWritePtr(dst, plane) + stride * height, d->mthL, stride * height);
        else if (plane > 0 && d->mtqC > -1)
            memset(vsapi->getWritePtr(dst, plane), d->mtqC, stride * height);
        else if (plane > 0 && d->mthC > -1)
            memset(vsapi->getWritePtr(dst, plane) + stride * height, d->mthC, stride * height);
    }
}
#endif

#ifdef VS_TARGET_CPU_X86
static void motionMask(const VSFrameRef * src1, const VSFrameRef * msk1, const VSFrameRef * src2, const VSFrameRef * msk2, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    const Vec16uc zero(0), two_five_five(255);
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src1, plane);
        const int height = vsapi->getFrameHeight(src1, plane);
        const int stride = vsapi->getStride(src1, plane);
        const uint8_t * srcp1_ = vsapi->getReadPtr(src1, plane);
        const uint8_t * srcp2_ = vsapi->getReadPtr(src2, plane);
        const uint8_t * mskp1q_ = vsapi->getReadPtr(msk1, plane);
        const uint8_t * mskp1h_ = mskp1q_ + stride * height;
        const uint8_t * mskp2q_ = vsapi->getReadPtr(msk2, plane);
        const uint8_t * mskp2h_ = mskp2q_ + stride * height;
        uint8_t * dstpq = vsapi->getWritePtr(dst, plane);
        uint8_t * dstph = dstpq + stride * height;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += 16) {
                const Vec16uc srcp1 = Vec16uc().load_a(srcp1_ + x);
                const Vec16uc srcp2 = Vec16uc().load_a(srcp2_ + x);
                const Vec16uc mskp1q = Vec16uc().load_a(mskp1q_ + x);
                const Vec16uc mskp1h = Vec16uc().load_a(mskp1h_ + x);
                const Vec16uc mskp2q = Vec16uc().load_a(mskp2q_ + x);
                const Vec16uc mskp2h = Vec16uc().load_a(mskp2h_ + x);
                const Vec16uc diff = abs_dif(srcp1, srcp2);
                const Vec16uc threshq = min(mskp1q, mskp2q);
                const Vec16uc lookupq = lookup<256>(threshq, d->mlut);
                select(diff <= lookupq, two_five_five, zero).store_a(dstpq + x);
                const Vec16uc threshh = min(mskp1h, mskp2h);
                const Vec16uc lookuph = lookup<256>(threshh, d->mlut);
                select(diff <= lookuph, two_five_five, zero).store_a(dstph + x);
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
static void motionMask(const VSFrameRef * src1, const VSFrameRef * msk1, const VSFrameRef * src2, const VSFrameRef * msk2, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src1, plane);
        const int height = vsapi->getFrameHeight(src1, plane);
        const int stride = vsapi->getStride(src1, plane);
        const uint8_t * srcp1 = vsapi->getReadPtr(src1, plane);
        const uint8_t * srcp2 = vsapi->getReadPtr(src2, plane);
        const uint8_t * mskp1q = vsapi->getReadPtr(msk1, plane);
        const uint8_t * mskp1h = mskp1q + stride * height;
        const uint8_t * mskp2q = vsapi->getReadPtr(msk2, plane);
        const uint8_t * mskp2h = mskp2q + stride * height;
        uint8_t * dstpq = vsapi->getWritePtr(dst, plane);
        uint8_t * dstph = dstpq + stride * height;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int diff = std::abs(srcp1[x] - srcp2[x]);
                const int threshq = std::min(mskp1q[x], mskp2q[x]);
                if (diff <= d->mlut[threshq])
                    dstpq[x] = 255;
                else
                    dstpq[x] = 0;
                const int threshh = std::min(mskp1h[x], mskp2h[x]);
                if (diff <= d->mlut[threshh])
                    dstph[x] = 255;
                else
                    dstph[x] = 0;
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
static inline void andMasks(const VSFrameRef * src1, const VSFrameRef * src2, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src1, plane);
        const int height = vsapi->getFrameHeight(src1, plane);
        const int stride = vsapi->getStride(src1, plane);
        const uint8_t * srcp1_ = vsapi->getReadPtr(src1, plane);
        const uint8_t * srcp2_ = vsapi->getReadPtr(src2, plane);
        uint8_t * dstp_ = vsapi->getWritePtr(dst, plane);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += 16) {
                const Vec16uc srcp1 = Vec16uc().load_a(srcp1_ + x);
                const Vec16uc srcp2 = Vec16uc().load_a(srcp2_ + x);
                const Vec16uc dstp = Vec16uc().load_a(dstp_ + x);
                (dstp & (srcp1 & srcp2)).store_a(dstp_ + x);
            }
            srcp1_ += stride;
            srcp2_ += stride;
            dstp_ += stride;
        }
    }
}
#else
static inline void andMasks(const VSFrameRef * src1, const VSFrameRef * src2, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src1, plane);
        const int height = vsapi->getFrameHeight(src1, plane);
        const int stride = vsapi->getStride(src1, plane);
        const uint8_t * srcp1 = vsapi->getReadPtr(src1, plane);
        const uint8_t * srcp2 = vsapi->getReadPtr(src2, plane);
        uint8_t * dstp = vsapi->getWritePtr(dst, plane);
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

static inline void combineMasks(const VSFrameRef * src, VSFrameRef * dst, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(dst, plane);
        const int stride = vsapi->getStride(src, plane);
        const uint8_t * srcp0 = vsapi->getReadPtr(src, plane);
        const uint8_t * srcp1 = srcp0 + stride * height;
        uint8_t * dstp = vsapi->getWritePtr(dst, plane);
        memcpy(dstp, srcp0, stride * height);
        const int8_t * offpt = d->offplut[plane];
        const int8_t * offnt = d->offnlut[plane];
        for (int y = 0; y < height; y++) {
            const uint8_t * srcpp0 = srcp0 - (y == 0 ? -stride : stride);
            const uint8_t * srcpn0 = srcp0 + (y == height - 1 ? -stride : stride);
            for (int x = 0; x < width; x++) {
                if (srcp0[x] || !srcp1[x])
                    continue;
                const int offp = offpt[x];
                const int offn = offnt[x];
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
                    dstp[x] = 255;
            }
            srcp0 += stride;
            srcp1 += stride;
            dstp += stride;
        }
    }
}

static inline bool checkCombed(const VSFrameRef * src, VSFrameRef * cmask, int * VS_RESTRICT cArray, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < (d->chroma ? 3 : 1); plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane);
        const uint8_t * srcp = vsapi->getReadPtr(src, plane);
        const uint8_t * srcpp = srcp - stride;
        const uint8_t * srcppp = srcpp - stride;
        const uint8_t * srcpn = srcp + stride;
        const uint8_t * srcpnn = srcpn + stride;
        uint8_t * cmkp = vsapi->getWritePtr(cmask, plane);
        if (d->cthresh < 0) {
            memset(cmkp, 255, stride * height);
            continue;
        }
        memset(cmkp, 0, stride * height);
        if (d->metric == 0) {
            for (int x = 0; x < width; x++) {
                const int sFirst = srcp[x] - srcpn[x];
                if ((sFirst > d->cthresh || sFirst < -d->cthresh) && std::abs(srcpnn[x] + (srcp[x] << 2) + srcpnn[x] - (3 * (srcpn[x] + srcpn[x]))) > d->cthresh6)
                    cmkp[x] = 0xFF;
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
                    cmkp[x] = 0xFF;
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
                        cmkp[x] = 0xFF;
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
                    cmkp[x] = 0xFF;
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
                    cmkp[x] = 0xFF;
            }
        } else {
            for (int x = 0; x < width; x++) {
                if ((srcp[x] - srcpn[x]) * (srcp[x] - srcpn[x]) > d->cthreshsq)
                    cmkp[x] = 0xFF;
            }
            srcpp += stride;
            srcp += stride;
            srcpn += stride;
            cmkp += stride;
            for (int y = 1; y < height - 1; y++) {
                for (int x = 0; x < width; x++) {
                    if ((srcp[x] - srcpp[x]) * (srcp[x] - srcpn[x]) > d->cthreshsq)
                        cmkp[x] = 0xFF;
                }
                srcpp += stride;
                srcp += stride;
                srcpn += stride;
                cmkp += stride;
            }
            for (int x = 0; x < width; x++) {
                if ((srcp[x] - srcpp[x]) * (srcp[x] - srcpp[x]) > d->cthreshsq)
                    cmkp[x] = 0xFF;
            }
        }
    }
    if (d->chroma) {
        const int width = vsapi->getFrameWidth(cmask, 2);
        const int height = vsapi->getFrameHeight(cmask, 2);
        const int stride = vsapi->getStride(cmask, 0) * 2;
        const int strideUV = vsapi->getStride(cmask, 2);
        uint8_t * cmkp = vsapi->getWritePtr(cmask, 0);
        const uint8_t * cmkpU = vsapi->getReadPtr(cmask, 1);
        const uint8_t * cmkpV = vsapi->getReadPtr(cmask, 2);
        uint8_t * cmkpp = cmkp - (stride / 2);
        uint8_t * cmkpn = cmkp + (stride / 2);
        uint8_t * cmkpnn = cmkpn + (stride / 2);
        const uint8_t * cmkppU = cmkpU - strideUV;
        const uint8_t * cmkpnU = cmkpU + strideUV;
        const uint8_t * cmkppV = cmkpV - strideUV;
        const uint8_t * cmkpnV = cmkpV + strideUV;
        for (int y = 1; y < height - 1; y++) {
            cmkpp += stride;
            cmkp += stride;
            cmkpn += stride;
            cmkpnn += stride;
            cmkppU += strideUV;
            cmkpU += strideUV;
            cmkpnU += strideUV;
            cmkppV += strideUV;
            cmkpV += strideUV;
            cmkpnV += strideUV;
            for (int x = 1; x < width - 1; x++) {
                if ((cmkpU[x] == 0xFF && (cmkpU[x - 1] == 0xFF || cmkpU[x + 1] == 0xFF ||
                     cmkppU[x - 1] == 0xFF || cmkppU[x] == 0xFF || cmkppU[x + 1] == 0xFF ||
                     cmkpnU[x - 1] == 0xFF || cmkpnU[x] == 0xFF || cmkpnU[x + 1] == 0xFF)) ||
                    (cmkpV[x] == 0xFF && (cmkpV[x - 1] == 0xFF || cmkpV[x + 1] == 0xFF ||
                     cmkppV[x - 1] == 0xFF || cmkppV[x] == 0xFF || cmkppV[x + 1] == 0xFF ||
                     cmkpnV[x - 1] == 0xFF || cmkpnV[x] == 0xFF || cmkpnV[x + 1] == 0xFF))) {
                    ((uint16_t *)cmkp)[x] = 0xFFFF;
                    ((uint16_t *)cmkpn)[x] = 0xFFFF;
                    if (y & 1)
                        ((uint16_t *)cmkpp)[x] = 0xFFFF;
                    else
                        ((uint16_t *)cmkpnn)[x] = 0xFFFF;
                }
            }
        }
    }
    const int width = vsapi->getFrameWidth(cmask, 0);
    const int height = vsapi->getFrameHeight(cmask, 0);
    const int stride = vsapi->getStride(cmask, 0);
    const uint8_t * cmkp = vsapi->getReadPtr(cmask, 0) + stride;
    const uint8_t * cmkpp = cmkp - stride;
    const uint8_t * cmkpn = cmkp + stride;
    const int xBlocks = ((width + d->xhalf) >> d->xshift) + 1;
    const int xBlocks4 = xBlocks * 4;
    const int yBlocks = ((height + d->yhalf) >> d->yshift) + 1;
    const int arraySize = (xBlocks * yBlocks) * 4;
    memset(cArray, 0, arraySize * sizeof(int));
    const int widtha = (width >> (d->xshift - 1)) << (d->xshift - 1);
    int heighta = (height >> (d->yshift - 1)) << (d->yshift - 1);
    if (heighta == height)
        heighta = height - d->yhalf;
    for (int y = 1; y < d->yhalf; y++) {
        const int temp1 = (y >> d->yshift) * xBlocks4;
        const int temp2 = ((y + d->yhalf) >> d->yshift) * xBlocks4;
        for (int x = 0; x < width; x++) {
            if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF) {
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
        const int temp1 = (y >> d->yshift) * xBlocks4;
        const int temp2 = ((y + d->yhalf) >> d->yshift) * xBlocks4;
        for (int x = 0; x < widtha; x += d->xhalf) {
            const uint8_t * cmkppT = cmkpp;
            const uint8_t * cmkpT = cmkp;
            const uint8_t * cmkpnT = cmkpn;
            int sum = 0;
            for (int u = 0; u < d->yhalf; u++) {
                for (int v = 0; v < d->xhalf; v++) {
                    if (cmkppT[x + v] == 0xFF && cmkpT[x + v] == 0xFF && cmkpnT[x + v] == 0xFF)
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
            const uint8_t * cmkppT = cmkpp;
            const uint8_t * cmkpT = cmkp;
            const uint8_t * cmkpnT = cmkpn;
            int sum = 0;
            for (int u = 0; u < d->yhalf; u++) {
                if (cmkppT[x] == 0xFF && cmkpT[x] == 0xFF && cmkpnT[x] == 0xFF)
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
        const int temp1 = (y >> d->yshift) * xBlocks4;
        const int temp2 = ((y + d->yhalf) >> d->yshift) * xBlocks4;
        for (int x = 0; x < width; x++) {
            if (cmkpp[x] == 0xFF && cmkp[x] == 0xFF && cmkpn[x] == 0xFF) {
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
    if (MIC > d->MI)
        return true;
    else
        return false;
}

static inline void setMaskForUpsize(VSFrameRef * msk, const int fieldt, const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(msk, plane);
        const int height = vsapi->getFrameHeight(msk, plane) / 2;
        const int stride = vsapi->getStride(msk, plane) * 2;
        uint8_t * maskwc = vsapi->getWritePtr(msk, plane);
        uint8_t * maskwn = maskwc + stride / 2;
        if (fieldt == 1) {
            for (int y = 0; y < height - 1; y++) {
                memset(maskwc, 10, width);
                memset(maskwn, 60, width);
                maskwc += stride;
                maskwn += stride;
            }
            memset(maskwc, 10, width);
            memset(maskwn, 10, width);
        } else {
            memset(maskwc, 10, width);
            memset(maskwn, 10, width);
            for (int y = 0; y < height - 1; y++) {
                maskwc += stride;
                maskwn += stride;
                memset(maskwc, 60, width);
                memset(maskwn, 10, width);
            }
        }
    }
}

static inline void eDeint(VSFrameRef * dst, const VSFrameRef * mask, const VSFrameRef * prv, const VSFrameRef * src, const VSFrameRef * nxt, const VSFrameRef * efrm,
                          const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane);
        const uint8_t * prvp = vsapi->getReadPtr(prv, plane);
        const uint8_t * srcp = vsapi->getReadPtr(src, plane);
        const uint8_t * nxtp = vsapi->getReadPtr(nxt, plane);
        const uint8_t * maskp = vsapi->getReadPtr(mask, plane);
        const uint8_t * efrmp = vsapi->getReadPtr(efrm, plane);
        uint8_t * dstp = vsapi->getWritePtr(dst, plane);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (maskp[x] == 10)
                    dstp[x] = srcp[x];
                else if (maskp[x] == 20)
                    dstp[x] = prvp[x];
                else if (maskp[x] == 30)
                    dstp[x] = nxtp[x];
                else if (maskp[x] == 40)
                    dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
                else if (maskp[x] == 50)
                    dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
                else if (maskp[x] == 70)
                    dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
                else if (maskp[x] == 60)
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

static inline void cubicDeint(VSFrameRef * dst, const VSFrameRef * mask, const VSFrameRef * prv, const VSFrameRef * src, const VSFrameRef * nxt,
                              const TDeintModData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        const int width = vsapi->getFrameWidth(src, plane);
        const int height = vsapi->getFrameHeight(src, plane);
        const int stride = vsapi->getStride(src, plane);
        const uint8_t * prvp = vsapi->getReadPtr(prv, plane);
        const uint8_t * srcp = vsapi->getReadPtr(src, plane);
        const uint8_t * nxtp = vsapi->getReadPtr(nxt, plane);
        const uint8_t * maskp = vsapi->getReadPtr(mask, plane);
        uint8_t * dstp = vsapi->getWritePtr(dst, plane);
        const uint8_t * srcpp = srcp - stride;
        const uint8_t * srcppp = srcpp - stride * 2;
        const uint8_t * srcpn = srcp + stride;
        const uint8_t * srcpnn = srcpn + stride * 2;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (maskp[x] == 10)
                    dstp[x] = srcp[x];
                else if (maskp[x] == 20)
                    dstp[x] = prvp[x];
                else if (maskp[x] == 30)
                    dstp[x] = nxtp[x];
                else if (maskp[x] == 40)
                    dstp[x] = (srcp[x] + nxtp[x] + 1) >> 1;
                else if (maskp[x] == 50)
                    dstp[x] = (srcp[x] + prvp[x] + 1) >> 1;
                else if (maskp[x] == 70)
                    dstp[x] = (prvp[x] + (srcp[x] << 1) + nxtp[x] + 2) >> 2;
                else if (maskp[x] == 60) {
                    if (y == 0) {
                        dstp[x] = srcpn[x];
                    } else if (y == height - 1) {
                        dstp[x] = srcpp[x];
                    } else if (y < 3 || y > height - 4) {
                        dstp[x] = (srcpn[x] + srcpp[x] + 1) >> 1;
                    } else {
                        const int temp = (19 * (srcpp[x] + srcpn[x]) - 3 * (srcppp[x] + srcpnn[x]) + 16) >> 5;
                        dstp[x] = std::max(std::min(temp, 255), 0);
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
    TDeintModData * d = (TDeintModData *)*instanceData;
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC tdeintmodCreateMMGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const TDeintModData * d = (const TDeintModData *)*instanceData;

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

        for (int i = 0; i < 3; i++) {
            src[i] = vsapi->getFrameFilter(std::min(n + i, d->vi.numFrames - 1), d->node, frameCtx);
            msk[i][0] = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height * 2, nullptr, core);
            msk[i][1] = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height * 2, nullptr, core);
            threshMask(src[i], msk[i][0], d, vsapi);
        }
        for (int i = 0; i < 2; i++)
            motionMask(src[i], msk[i][0], src[i + 1], msk[i + 1][0], msk[i][1], d, vsapi);
        motionMask(src[0], msk[0][0], src[2], msk[2][0], dst[0], d, vsapi);
        andMasks(msk[0][1], msk[1][1], dst[0], d, vsapi);
        combineMasks(dst[0], dst[1], d, vsapi);

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
    const TDeintModData * d = (const TDeintModData *)*instanceData;

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
        // In the AVS version, it's dynamically allocated to the size of (length - 2) and length doesn't have an upper limit.
        // Since I set the upper limit of length to 60 in VS port now, I just declare the array to the maximum possible size instead of using dynamic memory allocation.
        VSFrameRef * srct[58];
        VSFrameRef * srcb[58];
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, nullptr, core);

        int fieldt = d->field;
        if (d->mode == 1) {
            fieldt = n & 1 ? 1 - d->order : d->order;
            n /= 2;
        }

        const int * tmmlut = d->tmmlut16.data() + d->order * 8 + fieldt * 4;
        int tmmlutf[64];
        for (int i = 0; i < 64; i++)
            tmmlutf[i] = tmmlut[d->vlut[i]];

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

        int plut[2][119];     // Again, the size is (2 * length - 1) for the second dimension in the AVS version.
        uint8_t ** ptlut[3];
        for (int i = 0; i < 3; i++)
            ptlut[i] = new uint8_t *[i & 1 ? ccount : ocount];
        const int offo = d->length & 1 ? 0 : 1;
        const int offc = d->length & 1 ? 1 : 0;

        for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
            const int width = vsapi->getFrameWidth(dst, plane);
            const int height = vsapi->getFrameHeight(dst, plane);
            const int stride = vsapi->getStride(dst, plane);
            for (int i = 0; i < ccount; i++)
                ptlut[1][i] = vsapi->getWritePtr(csrc[i], plane);
            for (int i = 0; i < ocount; i++) {
                if (fieldt == 1) {
                    ptlut[0][i] = vsapi->getWritePtr(osrc[i], plane);
                    ptlut[2][i] = ptlut[0][i] + stride;
                } else {
                    ptlut[0][i] = ptlut[2][i] = vsapi->getWritePtr(osrc[i], plane);
                }
            }
            uint8_t * dstp = vsapi->getWritePtr(dst, plane);

            if (fieldt == 1) {
                for (int j = 0; j < height; j += 2)
                    memset(dstp + stride * j, 10, width);
                dstp += stride;
            } else {
                for (int j = 1; j < height; j += 2)
                    memset(dstp + stride * j, 10, width);
            }

            const int ct = ccount / 2;
            for (int y = fieldt; y < height; y += 2) {
                for (int x = 0; x < width; x++) {
                    if (!ptlut[1][ct - 2][x] && !ptlut[1][ct][x] && !ptlut[1][ct + 1][x]) {
                        dstp[x] = 60;
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

        for (int i = tstart; i <= tstop; i++)
            vsapi->freeFrame(srct[i - tstart]);
        for (int i = bstart; i <= bstop; i++)
            vsapi->freeFrame(srcb[i - bstart]);
        for (int i = 0; i < 3; i++)
            delete[] ptlut[i];
        return dst;
    }

    return nullptr;
}

static const VSFrameRef *VS_CC tdeintmodGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const TDeintModData * d = (const TDeintModData *)*instanceData;

    if (activationReason == arInitial) {
        if (d->mask)
            vsapi->requestFrameFilter(n, d->mask, frameCtx);
        if (d->edeint)
            vsapi->requestFrameFilter(n, d->edeint, frameCtx);

        if (d->mode == 1)
            n /= 2;

        if (n > 0)
            vsapi->requestFrameFilter(n - 1, !d->useClip2 ? d->node : d->clip2, frameCtx);
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        if (d->useClip2)
            vsapi->requestFrameFilter(n, d->clip2, frameCtx);
        if (n < d->viSaved->numFrames - 1)
            vsapi->requestFrameFilter(n + 1, !d->useClip2 ? d->node : d->clip2, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const int nSaved = n;
        int fieldt = d->field;
        if (d->mode == 1) {
            fieldt = n & 1 ? 1 - d->order : d->order;
            n /= 2;
        }

        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);

        if (d->mode == 0 && !d->full && !d->show) {
            VSFrameRef * cmask = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, nullptr, core);
            int * cArray = vs_aligned_malloc<int>((((d->viSaved->width + d->xhalf) >> d->xshift) + 1) * (((d->viSaved->height + d->yhalf) >> d->yshift) + 1) * 4 * sizeof(int), 32);
            if (!cArray) {
                vsapi->setFilterError("TDeintMod: malloc failure (cArray)", frameCtx);
                vsapi->freeFrame(src);
                vsapi->freeFrame(cmask);
                return nullptr;
            }
            const bool combed = checkCombed(src, cmask, cArray, d, vsapi);
            vsapi->freeFrame(cmask);
            vs_aligned_free(cArray);
            if (!combed)
                return src;
        }

        VSFrameRef * mask;
        if (d->mask) {
            const VSFrameRef * msk = vsapi->getFrameFilter(nSaved, d->mask, frameCtx);
            mask = vsapi->copyFrame(msk, core);
            vsapi->freeFrame(msk);
        } else {
            mask = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, nullptr, core);
            setMaskForUpsize(mask, fieldt, d, vsapi);
        }
        if (d->show) {
            vsapi->freeFrame(src);
            return mask;
        }

        const VSFrameRef * prv = vsapi->getFrameFilter(std::max(n - 1, 0), !d->useClip2 ? d->node : d->clip2, frameCtx);
        if (d->useClip2) {
            vsapi->freeFrame(src);
            src = vsapi->getFrameFilter(n, d->clip2, frameCtx);
        }
        const VSFrameRef * nxt = vsapi->getFrameFilter(std::min(n + 1, d->viSaved->numFrames - 1), !d->useClip2 ? d->node : d->clip2, frameCtx);

        VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);

        if (d->edeint) {
            const VSFrameRef * efrm = vsapi->getFrameFilter(nSaved, d->edeint, frameCtx);
            eDeint(dst, mask, prv, src, nxt, efrm, d, vsapi);
            vsapi->freeFrame(efrm);
        } else {
            cubicDeint(dst, mask, prv, src, nxt, d, vsapi);
        }

        vsapi->freeFrame(prv);
        vsapi->freeFrame(src);
        vsapi->freeFrame(nxt);
        vsapi->freeFrame(mask);
        return dst;
    }

    return nullptr;
}

static void VS_CC tdeintmodCreateMMFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TDeintModData * d = (TDeintModData *)instanceData;
    vsapi->freeNode(d->node);
    delete d;
}

static void VS_CC tdeintmodBuildMMFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TDeintModData * d = (TDeintModData *)instanceData;
    vsapi->freeNode(d->node);
    vsapi->freeNode(d->node2);
    delete d;
}

static void VS_CC tdeintmodFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TDeintModData * d = (TDeintModData *)instanceData;
    vsapi->freeNode(d->node);
    vsapi->freeNode(d->mask);
    vsapi->freeNode(d->clip2);
    vsapi->freeNode(d->edeint);
    if (d->mask) {
        for (int i = 0; i < d->vi.format->numPlanes; i++) {
            delete[] d->offplut[i];
            delete[] d->offnlut[i];
        }
    }
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
    d.full = !!vsapi->propGetInt(in, "full", 0, &err);
    if (err)
        d.full = true;
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
    if (d.blockx < 4 || d.blockx > 2048 || !isPowerOf2(d.blockx)) {
        vsapi->setError(out, "TDeintMod: illegal blockx size");
        return;
    }
    if (d.blocky < 4 || d.blocky > 2048 || !isPowerOf2(d.blocky)) {
        vsapi->setError(out, "TDeintMod: illegal blocky size");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = *vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(&d.vi) || !d.vi.numFrames || (d.vi.format->colorFamily != cmGray && d.vi.format->colorFamily != cmYUV) ||
        d.vi.format->sampleType != stInteger || d.vi.format->bitsPerSample != 8) {
        vsapi->setError(out, "TDeintMod: only constant format 8-bit Gray or YUV integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi.width & 1 || d.vi.height & 1) {
        vsapi->setError(out, "TDeintMod: width and height must be multiples of 2");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi.format->colorFamily == cmGray)
        d.chroma = false;

    vsapi->freeNode(d.node);

    d.mask = nullptr;
    if (d.mtqL > -2 || d.mthL > -2 || d.mtqC > -2 || d.mthC > -2) {
        VSMap * args = vsapi->createMap();
        VSPlugin * stdPlugin = vsapi->getPluginById("com.vapoursynth.std", core);

        d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
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

        for (int i = 0; i < d.vi.format->numPlanes; i++) {
            const int width = d.vi.width >> (i ? d.vi.format->subSamplingW : 0);
            d.offplut[i] = new int8_t[width];
            d.offnlut[i] = new int8_t[width];
            for (int j = 0; j < width; j++) {
                if (j == 0)
                    d.offplut[i][j] = -1;
                else
                    d.offplut[i][j] = 1;
                if (j == width - 1)
                    d.offnlut[i][j] = -1;
                else
                    d.offnlut[i][j] = 1;
            }
        }

        for (int i = 0; i < 256; i++)
            d.mlut[i] = std::min(std::max(i + d.nt, d.minthresh), d.maxthresh);

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
            60, 20, 50, 10, 60, 10, 40, 30,
            60, 10, 40, 30, 60, 20, 50, 10
        };

        data = new TDeintModData(d);

        vsapi->createFilter(in, out, "TDeintMod", tdeintmodInit, tdeintmodBuildMMGetFrame, tdeintmodBuildMMFree, fmParallel, 0, data, core);
        d.mask = vsapi->propGetNode(out, "clip", 0, nullptr);
        vsapi->clearMap(out);
        if (!invokeCache(&d.mask, out, stdPlugin, vsapi))
            return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.clip2 = vsapi->propGetNode(in, "clip2", 0, &err);
    d.edeint = vsapi->propGetNode(in, "edeint", 0, &err);
    d.vi = *vsapi->getVideoInfo(d.node);
    d.viSaved = vsapi->getVideoInfo(d.node);

    d.useClip2 = false;
    if (!d.full && d.mode == 0 && d.clip2) {
        if (!isSameFormat(vsapi->getVideoInfo(d.clip2), d.viSaved)) {
            vsapi->setError(out, "TDeintMod: clip2 must have the same dimensions as main clip and be the same format");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.mask);
            vsapi->freeNode(d.clip2);
            vsapi->freeNode(d.edeint);
            return;
        }

        if (vsapi->getVideoInfo(d.clip2)->numFrames != d.viSaved->numFrames) {
            vsapi->setError(out, "TDeintMod: clip2's number of frames doesn't match");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.mask);
            vsapi->freeNode(d.clip2);
            vsapi->freeNode(d.edeint);
            return;
        }

        d.useClip2 = true;
    }

    if (d.edeint) {
        if (!isSameFormat(vsapi->getVideoInfo(d.edeint), d.viSaved)) {
            vsapi->setError(out, "TDeintMod: edeint clip must have the same dimensions as main clip and be the same format");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.mask);
            vsapi->freeNode(d.clip2);
            vsapi->freeNode(d.edeint);
            return;
        }

        if (vsapi->getVideoInfo(d.edeint)->numFrames != d.viSaved->numFrames * (d.mode == 0 ? 1 : 2)) {
            vsapi->setError(out, "TDeintMod: edeint clip's number of frames doesn't match");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.mask);
            vsapi->freeNode(d.clip2);
            vsapi->freeNode(d.edeint);
            return;
        }
    }

    d.xhalf = d.blockx / 2;
    d.yhalf = d.blocky / 2;
    d.xshift = (int)std::log2(d.blockx);
    d.yshift = (int)std::log2(d.blocky);
    d.cthresh6 = d.cthresh * 6;
    d.cthreshsq = d.cthresh * d.cthresh;

    if (d.mode == 1) {
        d.vi.numFrames *= 2;
        d.vi.fpsNum *= 2;
    }

    TDeintModData * data = new TDeintModData(d);

    vsapi->createFilter(in, out, "TDeintMod", tdeintmodInit, tdeintmodGetFrame, tdeintmodFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.tdeintmod", "tdm", "A bi-directionally motion adaptive deinterlacer", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("TDeintMod",
                 "clip:clip;order:int;field:int:opt;mode:int:opt;"
                 "length:int:opt;mtype:int:opt;ttype:int:opt;mtql:int:opt;mthl:int:opt;mtqc:int:opt;mthc:int:opt;nt:int:opt;minthresh:int:opt;maxthresh:int:opt;cstr:int:opt;show:int:opt;"
                 "clip2:clip:opt;full:int:opt;cthresh:int:opt;blockx:int:opt;blocky:int:opt;chroma:int:opt;mi:int:opt;edeint:clip:opt;metric:int:opt;",
                 tdeintmodCreate, nullptr, plugin);
}
