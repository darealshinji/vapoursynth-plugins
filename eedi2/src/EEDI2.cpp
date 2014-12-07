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
#include <vector>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>

struct EEDI2Data {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    VSVideoInfo vi2;
    int field, mthresh, lthresh, vthresh, estr, dstr, maxd, map, nt, pp;
    int fieldS;
    std::vector<int> limlut;
};

static void buildEdgeMask(const uint8_t * VS_RESTRICT srcp, uint8_t * VS_RESTRICT dstp, const int width, const int height, const int stride, const EEDI2Data * d) {
    memset(dstp, 0, stride * height);
    dstp += stride;
    srcp += stride;
    const uint8_t * srcpp = srcp - stride;
    const uint8_t * srcpn = srcp + stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if ((std::abs(srcpp[x] - srcp[x]) < 10 && std::abs(srcp[x] - srcpn[x]) < 10 && std::abs(srcpp[x] - srcpn[x]) < 10) ||
                (std::abs(srcpp[x - 1] - srcp[x - 1]) < 10 && std::abs(srcp[x - 1] - srcpn[x - 1]) < 10 && std::abs(srcpp[x - 1] - srcpn[x - 1]) < 10 &&
                 std::abs(srcpp[x + 1] - srcp[x + 1]) < 10 && std::abs(srcp[x + 1] - srcpn[x + 1]) < 10 && std::abs(srcpp[x + 1] - srcpn[x + 1]) < 10))
                continue;
            const int sum = srcpp[x - 1] + srcpp[x] + srcpp[x + 1] +
                            srcp[x - 1] + srcp[x] + srcp[x + 1] +
                            srcpn[x - 1] + srcpn[x] + srcpn[x + 1];
            const int sumsq = srcpp[x - 1] * srcpp[x - 1] + srcpp[x] * srcpp[x] + srcpp[x + 1] * srcpp[x + 1] +
                              srcp[x - 1] * srcp[x - 1] + srcp[x] * srcp[x] + srcp[x + 1] * srcp[x + 1] +
                              srcpn[x - 1] * srcpn[x - 1] + srcpn[x] * srcpn[x] + srcpn[x + 1] * srcpn[x + 1];
            if (9 * sumsq - sum*sum < d->vthresh)
                continue;
            const int Ix = srcp[x + 1] - srcp[x - 1];
            const int Iy = std::max(std::max(std::abs(srcpp[x] - srcpn[x]), std::abs(srcpp[x] - srcp[x])), std::abs(srcp[x] - srcpn[x]));
            if (Ix * Ix + Iy * Iy >= d->mthresh) {
                dstp[x] = 255;
                continue;
            }
            const int Ixx = srcp[x - 1] - 2 * srcp[x] + srcp[x + 1];
            const int Iyy = srcpp[x] - 2 * srcp[x] + srcpn[x];
            if (std::abs(Ixx) + std::abs(Iyy) >= d->lthresh)
                dstp[x] = 255;
        }
        dstp += stride;
        srcpp += stride;
        srcp += stride;
        srcpn += stride;
    }
}

static void dilate(const uint8_t * VS_RESTRICT mskp, uint8_t * VS_RESTRICT dstp, const int width, const int height, const int stride, const EEDI2Data * d) {
    memcpy(dstp, mskp, stride * height);
    mskp += stride;
    dstp += stride;
    const uint8_t * mskpp = mskp - stride;
    const uint8_t * mskpn = mskp + stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (mskp[x] != 0)
                continue;
            int count = 0;
            if (mskpp[x - 1] == 255)
                count++;
            if (mskpp[x] == 255)
                count++;
            if (mskpp[x + 1] == 255)
                count++;
            if (mskp[x - 1] == 255)
                count++;
            if (mskp[x + 1] == 255)
                count++;
            if (mskpn[x - 1] == 255)
                count++;
            if (mskpn[x] == 255)
                count++;
            if (mskpn[x + 1] == 255)
                count++;
            if (count >= d->dstr)
                dstp[x] = 255;
        }
        mskpp += stride;
        mskp += stride;
        mskpn += stride;
        dstp += stride;
    }
}

static void erode(const uint8_t * VS_RESTRICT mskp, uint8_t * VS_RESTRICT dstp, const int width, const int height, const int stride, const EEDI2Data * d) {
    memcpy(dstp, mskp, stride * height);
    mskp += stride;
    dstp += stride;
    const uint8_t * mskpp = mskp - stride;
    const uint8_t * mskpn = mskp + stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (mskp[x] != 255)
                continue;
            int count = 0;
            if (mskpp[x - 1] == 255)
                count++;
            if (mskpp[x] == 255)
                count++;
            if (mskpp[x + 1] == 255)
                count++;
            if (mskp[x - 1] == 255)
                count++;
            if (mskp[x + 1] == 255)
                count++;
            if (mskpn[x - 1] == 255)
                count++;
            if (mskpn[x] == 255)
                count++;
            if (mskpn[x + 1] == 255)
                count++;
            if (count < d->estr)
                dstp[x] = 0;
        }
        mskpp += stride;
        mskp += stride;
        mskpn += stride;
        dstp += stride;
    }
}

static void removeSmallHorzGaps(const uint8_t * VS_RESTRICT mskp, uint8_t * VS_RESTRICT dstp, const int width, const int height, const int stride, const EEDI2Data * d) {
    memcpy(dstp, mskp, stride * height);
    mskp += stride;
    dstp += stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 3; x < width - 3; x++) {
            if (mskp[x]) {
                if (mskp[x - 3])
                    continue;
                if (mskp[x - 2])
                    continue;
                if (mskp[x - 1])
                    continue;
                if (mskp[x + 1])
                    continue;
                if (mskp[x + 2])
                    continue;
                if (mskp[x + 3])
                    continue;
                dstp[x] = 0;
            } else {
                if ((mskp[x + 1] && (mskp[x - 1] || mskp[x - 2] || mskp[x - 3])) ||
                    (mskp[x + 2] && (mskp[x - 1] || mskp[x - 2])) ||
                    (mskp[x + 3] && mskp[x - 1]))
                    dstp[x] = 255;
            }
        }
        mskp += stride;
        dstp += stride;
    }
}

static void calcDirections(const uint8_t * VS_RESTRICT mskp, const uint8_t * VS_RESTRICT srcp, uint8_t * VS_RESTRICT dstp,
                           const int width, const int height, const int stride, const int plane, const EEDI2Data * d) {
    memset(dstp, 255, stride * height);
    mskp += stride;
    srcp += stride;
    dstp += stride;
    const uint8_t * src2p = srcp - stride * 2;
    const uint8_t * srcpp = srcp - stride;
    const uint8_t * srcpn = srcp + stride;
    const uint8_t * src2n = srcp + stride * 2;
    const uint8_t * mskpp = mskp - stride;
    const uint8_t * mskpn = mskp + stride;
    const int maxdt = d->maxd >> (plane ? d->vi->format->subSamplingW : 0);
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (mskp[x] != 255 || (mskp[x - 1] != 255 && mskp[x + 1] != 255))
                continue;
            const int startu = std::max(-x + 1, -maxdt);
            const int stopu = std::min(width - 2 - x, maxdt);
            int mina = std::min(19 * d->nt, (std::abs(srcp[x] - srcpn[x]) + std::abs(srcp[x] - srcpp[x])) * 9);
            int minb = std::min(13 * d->nt, (std::abs(srcp[x] - srcpn[x]) + std::abs(srcp[x] - srcpp[x])) * 6);
            int minc = mina;
            int mind = minb;
            int mine = minb;
            int dira = -5000, dirb = -5000, dirc = -5000, dird = -5000, dire = -5000;
            for (int u = startu; u <= stopu; u++) {
                if ((y == 1 || mskpp[x - 1 + u] == 255 || mskpp[x + u] == 255 || mskpp[x + 1 + u] == 255) &&
                    (y == height - 2 || mskpn[x - 1 - u] == 255 || mskpn[x - u] == 255 || mskpn[x + 1 - u] == 255)) {
                    const int diffsn = std::abs(srcp[x - 1] - srcpn[x - 1 - u]) + std::abs(srcp[x] - srcpn[x - u]) + std::abs(srcp[x + 1] - srcpn[x + 1 - u]);
                    const int diffsp = std::abs(srcp[x - 1] - srcpp[x - 1 + u]) + std::abs(srcp[x] - srcpp[x + u]) + std::abs(srcp[x + 1] - srcpp[x + 1 + u]);
                    const int diffps = std::abs(srcpp[x - 1] - srcp[x - 1 - u]) + std::abs(srcpp[x] - srcp[x - u]) + std::abs(srcpp[x + 1] - srcp[x + 1 - u]);
                    const int diffns = std::abs(srcpn[x - 1] - srcp[x - 1 + u]) + std::abs(srcpn[x] - srcp[x + u]) + std::abs(srcpn[x + 1] - srcp[x + 1 + u]);
                    const int diff = diffsn + diffsp + diffps + diffns;
                    int diffd = diffsp + diffns;
                    int diffe = diffsn + diffps;
                    if (diff < minb) {
                        dirb = u;
                        minb = diff;
                    }
                    if (y > 1) {
                        const int diff2pp = std::abs(src2p[x - 1] - srcpp[x - 1 - u]) + std::abs(src2p[x] - srcpp[x - u]) + std::abs(src2p[x + 1] - srcpp[x + 1 - u]);
                        const int diffp2p = std::abs(srcpp[x - 1] - src2p[x - 1 + u]) + std::abs(srcpp[x] - src2p[x + u]) + std::abs(srcpp[x + 1] - src2p[x + 1 + u]);
                        const int diffa = diff + diff2pp + diffp2p;
                        diffd += diffp2p;
                        diffe += diff2pp;
                        if (diffa < mina) {
                            dira = u;
                            mina = diffa;
                        }
                    }
                    if (y < height - 2) {
                        const int diff2nn = std::abs(src2n[x - 1] - srcpn[x - 1 + u]) + std::abs(src2n[x] - srcpn[x + u]) + std::abs(src2n[x + 1] - srcpn[x + 1 + u]);
                        const int diffn2n = std::abs(srcpn[x - 1] - src2n[x - 1 - u]) + std::abs(srcpn[x] - src2n[x - u]) + std::abs(srcpn[x + 1] - src2n[x + 1 - u]);
                        const int diffc = diff + diff2nn + diffn2n;
                        diffd += diff2nn;
                        diffe += diffn2n;
                        if (diffc < minc) {
                            dirc = u;
                            minc = diffc;
                        }
                    }
                    if (diffd < mind) {
                        dird = u;
                        mind = diffd;
                    }
                    if (diffe < mine) {
                        dire = u;
                        mine = diffe;
                    }
                }
            }
            int order[5], k = 0;
            if (dira != -5000)
                order[k++] = dira;
            if (dirb != -5000)
                order[k++] = dirb;
            if (dirc != -5000)
                order[k++] = dirc;
            if (dird != -5000)
                order[k++] = dird;
            if (dire != -5000)
                order[k++] = dire;
            if (k > 1) {
                std::sort(order, order + k);
                const int mid = k & 1 ? order[k >> 1] : (order[(k - 1) >> 1] + order[k >> 1] + 1) >> 1;
                const int tlim = std::max(d->limlut[std::abs(mid)] >> 2, 2);
                int sum = 0, count = 0;
                for (int i = 0; i < k; i++) {
                    if (std::abs(order[i] - mid) <= tlim) {
                        count++;
                        sum += order[i];
                    }
                }
                dstp[x] = count > 1 ? 128 + static_cast<int>(static_cast<float>(sum) / count) * 4 : 128;
            } else {
                dstp[x] = 128;
            }
        }
        mskpp += stride;
        mskp += stride;
        mskpn += stride;
        src2p += stride;
        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        src2n += stride;
        dstp += stride;
    }
}

static void filterMap(const uint8_t * VS_RESTRICT mskp, const uint8_t * VS_RESTRICT dmskp, uint8_t * VS_RESTRICT dstp,
                      const int width, const int height, const int stride, const EEDI2Data * d) {
    memcpy(dstp, dmskp, stride * height);
    mskp += stride;
    dmskp += stride;
    dstp += stride;
    const uint8_t * dmskpp = dmskp - stride;
    const uint8_t * dmskpn = dmskp + stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (dmskp[x] == 255 || mskp[x] != 255)
                continue;
            const int dir = (dmskp[x] - 128) >> 2;
            const int lim = std::max(std::abs(dir) * 2, 12);
            bool ict = false, icb = false;
            if (dir < 0) {
                const int dirt = std::max(-x, dir);
                for (int j = dirt; j <= 0; j++) {
                    if ((std::abs(dmskpp[x + j] - dmskp[x]) > lim && dmskpp[x + j] != 255) ||
                        (dmskp[x + j] == 255 && dmskpp[x + j] == 255) ||
                        (std::abs(dmskp[x + j] - dmskp[x]) > lim && dmskp[x + j] != 255)) {
                        ict = true;
                        break;
                    }
                }
            } else {
                const int dirt = std::min(width - x - 1, dir);
                for (int j = 0; j <= dirt; j++) {
                    if ((std::abs(dmskpp[x + j] - dmskp[x]) > lim && dmskpp[x + j] != 255) ||
                        (dmskp[x + j] == 255 && dmskpp[x + j] == 255) ||
                        (std::abs(dmskp[x + j] - dmskp[x]) > lim && dmskp[x + j] != 255)) {
                        ict = true;
                        break;
                    }
                }
            }
            if (ict) {
                if (dir < 0) {
                    const int dirt = std::min(width - x - 1, std::abs(dir));
                    for (int j = 0; j <= dirt; j++) {
                        if ((std::abs(dmskpn[x + j] - dmskp[x]) > lim && dmskpn[x + j] != 255) ||
                            (dmskpn[x + j] == 255 && dmskp[x + j] == 255) ||
                            (std::abs(dmskp[x + j] - dmskp[x]) > lim && dmskp[x + j] != 255)) {
                            icb = true;
                            break;
                        }
                    }
                } else {
                    const int dirt = std::max(-x, -dir);
                    for (int j = dirt; j <= 0; j++) {
                        if ((std::abs(dmskpn[x + j] - dmskp[x]) > lim && dmskpn[x + j] != 255) ||
                            (dmskpn[x + j] == 255 && dmskp[x + j] == 255) ||
                            (std::abs(dmskp[x + j] - dmskp[x]) > lim && dmskp[x + j] != 255)) {
                            icb = true;
                            break;
                        }
                    }
                }
                if (icb)
                    dstp[x] = 255;
            }
        }
        mskp += stride;
        dmskpp += stride;
        dmskp += stride;
        dmskpn += stride;
        dstp += stride;
    }
}

static void filterDirMap(const uint8_t * VS_RESTRICT mskp, const uint8_t * VS_RESTRICT dmskp, uint8_t * VS_RESTRICT dstp,
                         const int width, const int height, const int stride, const EEDI2Data * d) {
    memcpy(dstp, dmskp, stride * height);
    mskp += stride;
    dmskp += stride;
    dstp += stride;
    const uint8_t * dmskpp = dmskp - stride;
    const uint8_t * dmskpn = dmskp + stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (mskp[x] != 255)
                continue;
            int order[9], u = 0;
            if (dmskpp[x - 1] != 255)
                order[u++] = dmskpp[x - 1];
            if (dmskpp[x] != 255)
                order[u++] = dmskpp[x];
            if (dmskpp[x + 1] != 255)
                order[u++] = dmskpp[x + 1];
            if (dmskp[x - 1] != 255)
                order[u++] = dmskp[x - 1];
            if (dmskp[x] != 255)
                order[u++] = dmskp[x];
            if (dmskp[x + 1] != 255)
                order[u++] = dmskp[x + 1];
            if (dmskpn[x - 1] != 255)
                order[u++] = dmskpn[x - 1];
            if (dmskpn[x] != 255)
                order[u++] = dmskpn[x];
            if (dmskpn[x + 1] != 255)
                order[u++] = dmskpn[x + 1];
            if (u < 4) {
                dstp[x] = 255;
                continue;
            }
            std::sort(order, order + u);
            const int mid = u & 1 ? order[u >> 1] : (order[(u - 1) >> 1] + order[u >> 1] + 1) >> 1;
            const int lim = d->limlut[std::abs(mid - 128) >> 2];
            int sum = 0, count = 0;
            for (int i = 0; i < u; i++) {
                if (std::abs(order[i] - mid) <= lim) {
                    count++;
                    sum += order[i];
                }
            }
            if (count < 4 || (count < 5 && dmskp[x] == 255)) {
                dstp[x] = 255;
                continue;
            }
            dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / static_cast<float>(count + 1) + 0.5f);
        }
        mskp += stride;
        dmskpp += stride;
        dmskp += stride;
        dmskpn += stride;
        dstp += stride;
    }
}

static void filterDirMap2X(const uint8_t * VS_RESTRICT mskp, const uint8_t * VS_RESTRICT dmskp, uint8_t * VS_RESTRICT dstp,
                           const int width, const int height, const int stride, const int field, const EEDI2Data * d) {
    memcpy(dstp, dmskp, stride * height);
    mskp += stride * (1 - field);
    dmskp += stride * (2 - field);
    dstp += stride * (2 - field);
    const uint8_t * mskpn = mskp + stride * 2;
    const uint8_t * dmskpp = dmskp - stride * 2;
    const uint8_t * dmskpn = dmskp + stride * 2;
    for (int y = 2 - field; y < height - 1; y += 2) {
        for (int x = 1; x < width - 1; x++) {
            if (mskp[x] != 255 && mskpn[x] != 255)
                continue;
            int order[9], u = 0;
            if (y > 1) {
                if (dmskpp[x - 1] != 255)
                    order[u++] = dmskpp[x - 1];
                if (dmskpp[x] != 255)
                    order[u++] = dmskpp[x];
                if (dmskpp[x + 1] != 255)
                    order[u++] = dmskpp[x + 1];
            }
            if (dmskp[x - 1] != 255)
                order[u++] = dmskp[x - 1];
            if (dmskp[x] != 255)
                order[u++] = dmskp[x];
            if (dmskp[x + 1] != 255)
                order[u++] = dmskp[x + 1];
            if (y < height - 2) {
                if (dmskpn[x - 1] != 255)
                    order[u++] = dmskpn[x - 1];
                if (dmskpn[x] != 255)
                    order[u++] = dmskpn[x];
                if (dmskpn[x + 1] != 255)
                    order[u++] = dmskpn[x + 1];
            }
            if (u < 4) {
                dstp[x] = 255;
                continue;
            }
            std::sort(order, order + u);
            const int mid = u & 1 ? order[u >> 1] : (order[(u - 1) >> 1] + order[u >> 1] + 1) >> 1;
            const int lim = d->limlut[std::abs(mid - 128) >> 2];
            int sum = 0, count = 0;
            for (int i = 0; i < u; i++) {
                if (std::abs(order[i] - mid) <= lim) {
                    count++;
                    sum += order[i];
                }
            }
            if (count < 4 || (count < 5 && dmskp[x] == 255)) {
                dstp[x] = 255;
                continue;
            }
            dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / static_cast<float>(count + 1) + 0.5f);
        }
        mskp += stride * 2;
        mskpn += stride * 2;
        dmskpp += stride * 2;
        dmskp += stride * 2;
        dmskpn += stride * 2;
        dstp += stride * 2;
    }
}

static void expandDirMap(const uint8_t * VS_RESTRICT mskp, const uint8_t * VS_RESTRICT dmskp, uint8_t * VS_RESTRICT dstp,
                         const int width, const int height, const int stride, const EEDI2Data * d) {
    memcpy(dstp, dmskp, stride * height);
    mskp += stride;
    dmskp += stride;
    dstp += stride;
    const uint8_t * dmskpp = dmskp - stride;
    const uint8_t * dmskpn = dmskp + stride;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (dmskp[x] != 255 || mskp[x] != 255)
                continue;
            int order[9], u = 0;
            if (dmskpp[x - 1] != 255)
                order[u++] = dmskpp[x - 1];
            if (dmskpp[x] != 255)
                order[u++] = dmskpp[x];
            if (dmskpp[x + 1] != 255)
                order[u++] = dmskpp[x + 1];
            if (dmskp[x - 1] != 255)
                order[u++] = dmskp[x - 1];
            if (dmskp[x + 1] != 255)
                order[u++] = dmskp[x + 1];
            if (dmskpn[x - 1] != 255)
                order[u++] = dmskpn[x - 1];
            if (dmskpn[x] != 255)
                order[u++] = dmskpn[x];
            if (dmskpn[x + 1] != 255)
                order[u++] = dmskpn[x + 1];
            if (u < 5)
                continue;
            std::sort(order, order + u);
            const int mid = u & 1 ? order[u >> 1] : (order[(u - 1) >> 1] + order[u >> 1] + 1) >> 1;
            const int lim = d->limlut[std::abs(mid - 128) >> 2];
            int sum = 0, count = 0;
            for (int i = 0; i < u; i++) {
                if (std::abs(order[i] - mid) <= lim) {
                    count++;
                    sum += order[i];
                }
            }
            if (count < 5)
                continue;
            dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / static_cast<float>(count + 1) + 0.5f);
        }
        mskp += stride;
        dmskpp += stride;
        dmskp += stride;
        dmskpn += stride;
        dstp += stride;
    }
}

static void expandDirMap2X(const uint8_t * VS_RESTRICT mskp, const uint8_t * VS_RESTRICT dmskp, uint8_t * VS_RESTRICT dstp,
                           const int width, const int height, const int stride, const int field, const EEDI2Data * d) {
    memcpy(dstp, dmskp, stride * height);
    mskp += stride * (1 - field);
    dmskp += stride * (2 - field);
    dstp += stride * (2 - field);
    const uint8_t * mskpn = mskp + stride * 2;
    const uint8_t * dmskpp = dmskp - stride * 2;
    const uint8_t * dmskpn = dmskp + stride * 2;
    for (int y = 2 - field; y < height - 1; y += 2) {
        for (int x = 1; x < width - 1; x++) {
            if (dmskp[x] != 255 || (mskp[x] != 255 && mskpn[x] != 255))
                continue;
            int order[9], u = 0;
            if (y > 1) {
                if (dmskpp[x - 1] != 255)
                    order[u++] = dmskpp[x - 1];
                if (dmskpp[x] != 255)
                    order[u++] = dmskpp[x];
                if (dmskpp[x + 1] != 255)
                    order[u++] = dmskpp[x + 1];
            }
            if (dmskp[x - 1] != 255)
                order[u++] = dmskp[x - 1];
            if (dmskp[x + 1] != 255)
                order[u++] = dmskp[x + 1];
            if (y < height - 2) {
                if (dmskpn[x - 1] != 255)
                    order[u++] = dmskpn[x - 1];
                if (dmskpn[x] != 255)
                    order[u++] = dmskpn[x];
                if (dmskpn[x + 1] != 255)
                    order[u++] = dmskpn[x + 1];
            }
            if (u < 5)
                continue;
            std::sort(order, order + u);
            const int mid = u & 1 ? order[u >> 1] : (order[(u - 1) >> 1] + order[u >> 1] + 1) >> 1;
            const int lim = d->limlut[std::abs(mid - 128) >> 2];
            int sum = 0, count = 0;
            for (int i = 0; i < u; i++) {
                if (std::abs(order[i] - mid) <= lim) {
                    count++;
                    sum += order[i];
                }
            }
            if (count < 5)
                continue;
            dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / static_cast<float>(count + 1) + 0.5f);
        }
        mskp += stride * 2;
        mskpn += stride * 2;
        dmskpp += stride * 2;
        dmskp += stride * 2;
        dmskpn += stride * 2;
        dstp += stride * 2;
    }
}

static void fillGaps2X(const uint8_t * VS_RESTRICT mskp, const uint8_t * VS_RESTRICT dmskp, uint8_t * VS_RESTRICT dstp,
                       const int width, const int height, const int stride, const int field, const EEDI2Data * d) {
    memcpy(dstp, dmskp, stride * height);
    mskp += stride * (1 - field);
    dmskp += stride * (2 - field);
    dstp += stride * (2 - field);
    const uint8_t * mskpp = mskp - stride * 2;
    const uint8_t * mskpn = mskp + stride * 2;
    const uint8_t * mskpnn = mskpn + stride * 2;
    const uint8_t * dmskpp = dmskp - stride * 2;
    const uint8_t * dmskpn = dmskp + stride * 2;
    for (int y = 2 - field; y < height - 1; y += 2) {
        for (int x = 1; x < width - 1; x++) {
            if (dmskp[x] != 255 || (mskp[x] != 255 && mskpn[x] != 255))
                continue;
            int u = x - 1, back = 500, forward = -500;
            while (u) {
                if (dmskp[u] != 255) {
                    back = dmskp[u];
                    break;
                }
                if (mskp[u] != 255 && mskpn[u] != 255)
                    break;
                u--;
            }
            int v = x + 1;
            while (v < width) {
                if (dmskp[v] != 255) {
                    forward = dmskp[v];
                    break;
                }
                if (mskp[v] != 255 && mskpn[v] != 255)
                    break;
                v++;
            }
            bool tc = true, bc = true;
            int mint = 500, maxt = -20;
            int minb = 500, maxb = -20;
            for (int j = u; j <= v; j++) {
                if (tc) {
                    if (y <= 2 || dmskpp[j] == 255 || (mskpp[j] != 255 && mskp[j] != 255)) {
                        tc = false;
                        mint = maxt = 20;
                    } else {
                        if (dmskpp[j] < mint)
                            mint = dmskpp[j];
                        if (dmskpp[j] > maxt)
                            maxt = dmskpp[j];
                    }
                }
                if (bc) {
                    if (y >= height - 3 || dmskpn[j] == 255 || (mskpn[j] != 255 && mskpnn[j] != 255)) {
                        bc = false;
                        minb = maxb = 20;
                    } else {
                        if (dmskpn[j] < minb)
                            minb = dmskpn[j];
                        if (dmskpn[j] > maxb)
                            maxb = dmskpn[j];
                    }
                }
            }
            if (maxt == -20)
                maxt = mint = 20;
            if (maxb == -20)
                maxb = minb = 20;
            const int thresh = std::max(std::max(std::max(std::abs(forward - 128), std::abs(back - 128)) >> 2, 8), std::max(std::abs(mint - maxt), std::abs(minb - maxb)));
            const int flim = std::min(std::max(std::abs(forward - 128), std::abs(back - 128)) >> 2, 6);
            if (std::abs(forward - back) <= thresh && (v - u - 1 <= flim || tc || bc)) {
                const double step = static_cast<double>(forward - back) / static_cast<double>(v - u);
                for (int j = 0; j < v - u - 1; j++)
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

static inline void upscaleBy2(const uint8_t * VS_RESTRICT srcp, uint8_t * VS_RESTRICT dstp, const int width, const int height, const int stride, const int field, const EEDI2Data * d) {
    vs_bitblt(dstp + stride * (1 - field), stride * 2, srcp, stride, width * d->vi->format->bytesPerSample, height);
}

static void markDirections2X(const uint8_t * VS_RESTRICT mskp, const uint8_t * VS_RESTRICT dmskp, uint8_t * VS_RESTRICT dstp,
                             const int width, const int height, const int stride, const int field, const EEDI2Data * d) {
    memset(dstp, 255, stride * height);
    mskp += stride * (1 - field);
    dmskp += stride * (1 - field);
    dstp += stride * (2 - field);
    const uint8_t * mskpn = mskp + stride * 2;
    const uint8_t * dmskpn = dmskp + stride * 2;
    for (int y = 2 - field; y < height - 1; y += 2) {
        for (int x = 1; x < width - 1; x++) {
            if (mskp[x] != 255 && mskpn[x] != 255)
                continue;
            int order[6], v = 0;
            if (dmskp[x - 1] != 255)
                order[v++] = dmskp[x - 1];
            if (dmskp[x] != 255)
                order[v++] = dmskp[x];
            if (dmskp[x + 1] != 255)
                order[v++] = dmskp[x + 1];
            if (dmskpn[x - 1] != 255)
                order[v++] = dmskpn[x - 1];
            if (dmskpn[x] != 255)
                order[v++] = dmskpn[x];
            if (dmskpn[x + 1] != 255)
                order[v++] = dmskpn[x + 1];
            if (v < 3) {
                continue;
            } else {
                std::sort(order, order + v);
                const int mid = v & 1 ? order[v >> 1] : (order[(v - 1) >> 1] + order[v >> 1] + 1) >> 1;
                const int lim = d->limlut[std::abs(mid - 128) >> 2];
                int u = 0;
                if (std::abs(dmskp[x - 1] - dmskpn[x - 1]) <= lim || dmskp[x - 1] == 255 || dmskpn[x - 1] == 255)
                    u++;
                if (std::abs(dmskp[x] - dmskpn[x]) <= lim || dmskp[x] == 255 || dmskpn[x] == 255)
                    u++;
                if (std::abs(dmskp[x + 1] - dmskpn[x - 1]) <= lim || dmskp[x + 1] == 255 || dmskpn[x + 1] == 255)
                    u++;
                if (u < 2)
                    continue;
                int count = 0, sum = 0;
                for (int i = 0; i < v; i++) {
                    if (std::abs(order[i] - mid) <= lim) {
                        count++;
                        sum += order[i];
                    }
                }
                if (count < v - 2 || count < 2)
                    continue;
                dstp[x] = static_cast<int>(static_cast<float>(sum + mid) / static_cast<float>(count + 1) + 0.5f);
            }
        }
        mskp += stride * 2;
        mskpn += stride * 2;
        dmskp += stride * 2;
        dmskpn += stride * 2;
        dstp += stride * 2;
    }
}

static void interpolateLattice(const uint8_t * VS_RESTRICT omskp, uint8_t * VS_RESTRICT dmskp, uint8_t * VS_RESTRICT dstp,
                               const int width, const int height, const int stride, const int plane, const int field, const EEDI2Data * d) {
    if (field == 1)
        memcpy(dstp + stride * (height - 1), dstp + stride * (height - 2), stride);
    else
        memcpy(dstp, dstp + stride, stride);
    dmskp += stride * (2 - field);
    dstp += stride * (1 - field);
    omskp += stride * (1 - field);
    uint8_t * dstpn = dstp + stride;
    const uint8_t * dstpnn = dstp + stride * 2;
    const uint8_t * omskn = omskp + stride * 2;
    for (int y = 2 - field; y < height - 1; y += 2) {
        for (int x = 0; x < width; x++) {
            int dir = dmskp[x];
            const int lim = d->limlut[std::abs(dir - 128) >> 2];
            if (dir == 255 || (std::abs(dmskp[x] - dmskp[x - 1]) > lim && std::abs(dmskp[x] - dmskp[x + 1]) > lim)) {
                dstpn[x] = (dstp[x] + dstpnn[x] + 1) >> 1;
                if (dir != 255)
                    dmskp[x] = 128;
                continue;
            }
            if (lim < 9) {
                const int sum = dstp[x - 1] + dstp[x] + dstp[x + 1] + dstpnn[x - 1] + dstpnn[x] + dstpnn[x + 1];
                const int sumsq = dstp[x - 1] * dstp[x - 1] + dstp[x] * dstp[x] + dstp[x + 1] * dstp[x + 1] +
                                  dstpnn[x - 1] * dstpnn[x - 1] + dstpnn[x] * dstpnn[x] + dstpnn[x + 1] * dstpnn[x + 1];
                if (6 * sumsq - sum*sum < 576) {
                    dstpn[x] = (dstp[x] + dstpnn[x] + 1) >> 1;
                    dmskp[x] = 255;
                    continue;
                }
            }
            if (x > 1 && x < width - 2 && (dstp[x] < std::max(dstp[x - 2], dstp[x - 1]) - 3 && dstp[x] < std::max(dstp[x + 2], dstp[x + 1]) - 3 &&
                                           dstpnn[x] < std::max(dstpnn[x - 2], dstpnn[x - 1]) - 3 && dstpnn[x] < std::max(dstpnn[x + 2], dstpnn[x + 1]) - 3) ||
                                          (dstp[x] > std::min(dstp[x - 2], dstp[x - 1]) + 3 && dstp[x] > std::min(dstp[x + 2], dstp[x + 1]) + 3 &&
                                           dstpnn[x] > std::min(dstpnn[x - 2], dstpnn[x - 1]) + 3 && dstpnn[x] > std::min(dstpnn[x + 2], dstpnn[x + 1]) + 3)) {
                dstpn[x] = (dstp[x] + dstpnn[x] + 1) >> 1;
                dmskp[x] = 128;
                continue;
            }
            dir = (dir - 128 + 2) >> 2;
            int val = (dstp[x] + dstpnn[x] + 1) >> 1;
            const int startu = dir - 2 < 0 ? std::max(-x + 1, std::max(dir - 2, -width + 2 + x)) : std::min(x - 1, std::min(dir - 2, width - 2 - x));
            const int stopu = dir + 2 < 0 ? std::max(-x + 1, std::max(dir + 2, -width + 2 + x)) : std::min(x - 1, std::min(dir + 2, width - 2 - x));
            int min = 8 * d->nt;
            for (int u = startu; u <= stopu; u++) {
                const int diff = std::abs(dstp[x - 1] - dstpnn[x - u - 1]) + std::abs(dstp[x] - dstpnn[x - u]) + std::abs(dstp[x + 1] - dstpnn[x - u + 1]) +
                                 std::abs(dstpnn[x - 1] - dstp[x + u - 1]) + std::abs(dstpnn[x] - dstp[x + u]) + std::abs(dstpnn[x + 1] - dstp[x + u + 1]);
                if (diff < min && ((omskp[x - 1 + u] != 255 && std::abs(omskp[x - 1 + u] - dmskp[x]) <= lim) ||
                                   (omskp[x + u] != 255 && std::abs(omskp[x + u] - dmskp[x]) <= lim) ||
                                   (omskp[x + 1 + u] != 255 && std::abs(omskp[x + 1 + u] - dmskp[x]) <= lim)) &&
                                  ((omskn[x - 1 - u] != 255 && std::abs(omskn[x - 1 - u] - dmskp[x]) <= lim) ||
                                   (omskn[x - u] != 255 && std::abs(omskn[x - u] - dmskp[x]) <= lim) ||
                                   (omskn[x + 1 - u] != 255 && std::abs(omskn[x + 1 - u] - dmskp[x]) <= lim))) {
                    const int diff2 = std::abs(dstp[x + (u >> 1) - 1] - dstpnn[x - (u >> 1) - 1]) +
                                      std::abs(dstp[x + (u >> 1)] - dstpnn[x - (u >> 1)]) +
                                      std::abs(dstp[x + (u >> 1) + 1] - dstpnn[x - (u >> 1) + 1]);
                    if (diff2 < 4 * d->nt && (((std::abs(omskp[x + (u >> 1)] - omskn[x - (u >> 1)]) <= lim ||
                                                std::abs(omskp[x + (u >> 1)] - omskn[x - ((u + 1) >> 1)]) <= lim) &&
                                               omskp[x + (u >> 1)] != 255) ||
                                              ((std::abs(omskp[x + ((u + 1) >> 1)] - omskn[x - (u >> 1)]) <= lim ||
                                                std::abs(omskp[x + ((u + 1) >> 1)] - omskn[x - ((u + 1) >> 1)]) <= lim) &&
                                               omskp[x + ((u + 1) >> 1)] != 255))) {
                        if ((std::abs(dmskp[x] - omskp[x + (u >> 1)]) <= lim || std::abs(dmskp[x] - omskp[x + ((u + 1) >> 1)]) <= lim) &&
                            (std::abs(dmskp[x] - omskn[x - (u >> 1)]) <= lim || std::abs(dmskp[x] - omskn[x - ((u + 1) >> 1)]) <= lim)) {
                            val = (dstp[x + (u >> 1)] + dstp[x + ((u + 1) >> 1)] + dstpnn[x - (u >> 1)] + dstpnn[x - ((u + 1) >> 1)] + 2) >> 2;
                            min = diff;
                            dir = u;
                        }
                    }
                }
            }
            if (min != 8 * d->nt) {
                dstpn[x] = val;
                dmskp[x] = 128 + dir * 4;
            } else {
                const int minm = std::min(dstp[x], dstpnn[x]);
                const int maxm = std::max(dstp[x], dstpnn[x]);
                const int dd = 4 >> (plane ? d->vi->format->subSamplingW : 0);
                const int startu = std::max(-x + 1, -dd);
                const int stopu = std::min(width - 2 - x, dd);
                min = 7 * d->nt;
                for (int u = startu; u <= stopu; u++) {
                    const int p1 = dstp[x + (u >> 1)] + dstp[x + ((u + 1) >> 1)];
                    const int p2 = dstpnn[x - (u >> 1)] + dstpnn[x - ((u + 1) >> 1)];
                    const int diff = std::abs(dstp[x - 1] - dstpnn[x - u - 1]) + std::abs(dstp[x] - dstpnn[x - u]) + std::abs(dstp[x + 1] - dstpnn[x - u + 1]) +
                                     std::abs(dstpnn[x - 1] - dstp[x + u - 1]) + std::abs(dstpnn[x] - dstp[x + u]) + std::abs(dstpnn[x + 1] - dstp[x + u + 1]) +
                                     std::abs(p1 - p2);
                    if (diff < min) {
                        const int valt = (p1 + p2 + 2) >> 2;
                        if (valt >= minm && valt <= maxm) {
                            val = valt;
                            min = diff;
                            dir = u;
                        }
                    }
                }
                dstpn[x] = val;
                dmskp[x] = min == 7 * d->nt ? 128 : 128 + dir * 4;
            }
        }
        dmskp += stride * 2;
        dstp += stride * 2;
        dstpn += stride * 2;
        dstpnn += stride * 2;
        omskp += stride * 2;
        omskn += stride * 2;
    }
}

static void postProcess(const uint8_t * VS_RESTRICT nmskp, const uint8_t * VS_RESTRICT omskp, uint8_t * VS_RESTRICT dstp,
                        const int width, const int height, const int stride, const int field, const EEDI2Data * d) {
    nmskp += stride * (2 - field);
    omskp += stride * (2 - field);
    dstp += stride * (2 - field);
    const uint8_t * srcpp = dstp - stride;
    const uint8_t * srcpn = dstp + stride;
    for (int y = 2 - field; y < height - 1; y += 2) {
        for (int x = 0; x < width; x++) {
            const int lim = d->limlut[std::abs(nmskp[x] - 128) >> 2];
            if (std::abs(nmskp[x] - omskp[x]) > lim && omskp[x] != 255 && omskp[x] != 128)
                dstp[x] = (srcpp[x] + srcpn[x] + 1) >> 1;
        }
        nmskp += stride * 2;
        omskp += stride * 2;
        srcpp += stride * 2;
        dstp += stride * 2;
        srcpn += stride * 2;
    }
}

static void postProcessCorner(const uint8_t * VS_RESTRICT mskp, uint8_t * VS_RESTRICT dstp, const int * VS_RESTRICT x2, const int * VS_RESTRICT y2, const int * VS_RESTRICT xy,
                              const int width, const int height, const int stride, const int field, const EEDI2Data * d) {
    mskp += stride * (8 - field);
    dstp += stride * (8 - field);
    x2 += stride * 3;
    y2 += stride * 3;
    xy += stride * 3;
    const uint8_t * dstpp = dstp - stride;
    const uint8_t * dstpn = dstp + stride;
    const int * x2n = x2 + stride;
    const int * y2n = y2 + stride;
    const int * xyn = xy + stride;
    for (int y = 8 - field; y < height - 7; y += 2) {
        for (int x = 4; x < width - 4; x++) {
            if (mskp[x] == 255 || mskp[x] == 128)
                continue;
            const int c1 = static_cast<int>(x2[x] * y2[x] - xy[x] * xy[x] - 0.09 * (x2[x] + y2[x]) * (x2[x] + y2[x]));
            const int c2 = static_cast<int>(x2n[x] * y2n[x] - xyn[x] * xyn[x] - 0.09 * (x2n[x] + y2n[x]) * (x2n[x] + y2n[x]));
            if (c1 > 775 || c2 > 775)
                dstp[x] = (dstpp[x] + dstpn[x] + 1) >> 1;
        }
        mskp += stride * 2;
        dstpp += stride * 2;
        dstp += stride * 2;
        dstpn += stride * 2;
        x2 += stride;
        x2n += stride;
        y2 += stride;
        y2n += stride;
        xy += stride;
        xyn += stride;
    }
}

static void gaussianBlur1(const uint8_t * VS_RESTRICT srcp, uint8_t * VS_RESTRICT tmpp, uint8_t * VS_RESTRICT dstp, const int width, const int height, const int stride) {
    const uint8_t * const tmp = tmpp;
    for (int y = 0; y < height; y++) {
        tmpp[0] = (srcp[3] * 582 + srcp[2] * 7078 + srcp[1] * 31724 + srcp[0] * 26152 + 32768) >> 16;
        tmpp[1] = (srcp[4] * 582 + srcp[3] * 7078 + (srcp[0] + srcp[2]) * 15862 + srcp[1] * 26152 + 32768) >> 16;
        tmpp[2] = (srcp[5] * 582 + (srcp[0] + srcp[4]) * 3539 + (srcp[1] + srcp[3]) * 15862 + srcp[2] * 26152 + 32768) >> 16;
        int x;
        for (x = 3; x < width - 3; x++)
            tmpp[x] = ((srcp[x - 3] + srcp[x + 3]) * 291 + (srcp[x - 2] + srcp[x + 2]) * 3539 + (srcp[x - 1] + srcp[x + 1]) * 15862 + srcp[x] * 26152 + 32768) >> 16;
        tmpp[x] = (srcp[x - 3] * 582 + (srcp[x - 2] + srcp[x + 2]) * 3539 + (srcp[x - 1] + srcp[x + 1]) * 15862 + srcp[x] * 26152 + 32768) >> 16;
        x++;
        tmpp[x] = (srcp[x - 3] * 582 + srcp[x - 2] * 7078 + (srcp[x - 1] + srcp[x + 1]) * 15862 + srcp[x] * 26152 + 32768) >> 16;
        x++;
        tmpp[x] = (srcp[x - 3] * 582 + srcp[x - 2] * 7078 + srcp[x - 1] * 31724 + srcp[x] * 26152 + 32768) >> 16;
        srcp += stride;
        tmpp += stride;
    }
    srcp = tmp;
    const uint8_t * src3p = srcp - stride * 3;
    const uint8_t * src2p = srcp - stride * 2;
    const uint8_t * srcpp = srcp - stride;
    const uint8_t * srcpn = srcp + stride;
    const uint8_t * src2n = srcp + stride * 2;
    const uint8_t * src3n = srcp + stride * 3;
    for (int x = 0; x < width; x++)
        dstp[x] = (src3n[x] * 582 + src2n[x] * 7078 + srcpn[x] * 31724 + srcp[x] * 26152 + 32768) >> 16;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src3n[x] * 582 + src2n[x] * 7078 + (srcpp[x] + srcpn[x]) * 15862 + srcp[x] * 26152 + 32768) >> 16;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src3n[x] * 582 + (src2p[x] + src2n[x]) * 3539 + (srcpp[x] + srcpn[x]) * 15862 + srcp[x] * 26152 + 32768) >> 16;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;
    for (int y = 3; y < height - 3; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = ((src3p[x] + src3n[x]) * 291 + (src2p[x] + src2n[x]) * 3539 + (srcpp[x] + srcpn[x]) * 15862 + srcp[x] * 26152 + 32768) >> 16;
        src3p += stride;
        src2p += stride;
        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        src2n += stride;
        src3n += stride;
        dstp += stride;
    }
    for (int x = 0; x < width; x++)
        dstp[x] = (src3p[x] * 582 + (src2p[x] + src2n[x]) * 3539 + (srcpp[x] + srcpn[x]) * 15862 + srcp[x] * 26152 + 32768) >> 16;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src3p[x] * 582 + src2p[x] * 7078 + (srcpp[x] + srcpn[x]) * 15862 + srcp[x] * 26152 + 32768) >> 16;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src3p[x] * 582 + src2p[x] * 7078 + srcpp[x] * 31724 + srcp[x] * 26152 + 32768) >> 16;
}

static void gaussianBlurSqrt2(const int * VS_RESTRICT src, int * VS_RESTRICT tmp, int * VS_RESTRICT dst, const int width, const int height, const int stride) {
    const int * srcp = src;
    int * dstp = tmp;
    for (int y = 0; y < height; y++) {
        int x = 0;
        dstp[x] = (srcp[x + 4] * 678 + srcp[x + 3] * 3902 + srcp[x + 2] * 13618 + srcp[x + 1] * 28830 + srcp[x] * 18508 + 32768) >> 16;
        x++;
        dstp[x] = (srcp[x + 4] * 678 + srcp[x + 3] * 3902 + srcp[x + 2] * 13618 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) >> 16;
        x++;
        dstp[x] = (srcp[x + 4] * 678 + srcp[x + 3] * 3902 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) >> 16;
        x++;
        dstp[x] = (srcp[x + 4] * 678 + (srcp[x - 3] + srcp[x + 3]) * 1951 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) >> 16;
        for (x = 4; x < width - 4; x++)
            dstp[x] = ((srcp[x - 4] + srcp[x + 4]) * 339 + (srcp[x - 3] + srcp[x + 3]) * 1951 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) >> 16;
        dstp[x] = (srcp[x - 4] * 678 + (srcp[x - 3] + srcp[x + 3]) * 1951 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) >> 16;
        x++;
        dstp[x] = (srcp[x - 4] * 678 + srcp[x - 3] * 3902 + (srcp[x - 2] + srcp[x + 2]) * 6809 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) >> 16;
        x++;
        dstp[x] = (srcp[x - 4] * 678 + srcp[x - 3] * 3902 + srcp[x - 2] * 13618 + (srcp[x - 1] + srcp[x + 1]) * 14415 + srcp[x] * 18508 + 32768) >> 16;
        x++;
        dstp[x] = (srcp[x - 4] * 678 + srcp[x - 3] * 3902 + srcp[x - 2] * 13618 + srcp[x - 1] * 28830 + srcp[x] * 18508 + 32768) >> 16;
        srcp += stride;
        dstp += stride;
    }
    srcp = tmp;
    dstp = dst;
    const int * src4p = srcp - stride * 4;
    const int * src3p = srcp - stride * 3;
    const int * src2p = srcp - stride * 2;
    const int * srcpp = srcp - stride;
    const int * srcpn = srcp + stride;
    const int * src2n = srcp + stride * 2;
    const int * src3n = srcp + stride * 3;
    const int * src4n = srcp + stride * 4;
    for (int x = 0; x < width; x++)
        dstp[x] = (src4n[x] * 678 + src3n[x] * 3902 + src2n[x] * 13618 + srcpn[x] * 28830 + srcp[x] * 18508 + 32768) >> 18;
    src4p += stride;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    src4n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src4n[x] * 678 + src3n[x] * 3902 + src2n[x] * 13618 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) >> 18;
    src4p += stride;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    src4n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src4n[x] * 678 + src3n[x] * 3902 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) >> 18;
    src4p += stride;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    src4n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src4n[x] * 678 + (src3p[x] + src3n[x]) * 1951 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) >> 18;
    src4p += stride;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    src4n += stride;
    dstp += stride;
    for (int y = 4; y < height - 4; y++) {
        for (int x = 0; x < width; x++)
            dstp[x] = ((src4p[x] + src4n[x]) * 339 + (src3p[x] + src3n[x]) * 1951 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) >> 18;
        src4p += stride;
        src3p += stride;
        src2p += stride;
        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        src2n += stride;
        src3n += stride;
        src4n += stride;
        dstp += stride;
    }
    for (int x = 0; x < width; x++)
        dstp[x] = (src4p[x] * 678 + (src3p[x] + src3n[x]) * 1951 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) >> 18;
    src4p += stride;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    src4n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src4p[x] * 678 + src3p[x] * 3902 + (src2p[x] + src2n[x]) * 6809 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) >> 18;
    src4p += stride;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    src4n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src4p[x] * 678 + src3p[x] * 3902 + src2p[x] * 13618 + (srcpp[x] + srcpn[x]) * 14415 + srcp[x] * 18508 + 32768) >> 18;
    src4p += stride;
    src3p += stride;
    src2p += stride;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    src2n += stride;
    src3n += stride;
    src4n += stride;
    dstp += stride;
    for (int x = 0; x < width; x++)
        dstp[x] = (src4p[x] * 678 + src3p[x] * 3902 + src2p[x] * 13618 + srcpp[x] * 28830 + srcp[x] * 18508 + 32768) >> 18;
}

static void calcDerivatives(const uint8_t * VS_RESTRICT srcp, int * VS_RESTRICT x2, int * VS_RESTRICT y2, int * VS_RESTRICT xy, const int width, const int height, const int stride) {
    const uint8_t * srcpp = srcp - stride;
    const uint8_t * srcpn = srcp + stride;
    {
        const int Ix = srcp[1] - srcp[0];
        const int Iy = srcp[0] - srcpn[0];
        x2[0] = (Ix * Ix) >> 1;
        y2[0] = (Iy * Iy) >> 1;
        xy[0] = (Ix * Iy) >> 1;
    }
    int x;
    for (x = 1; x < width - 1; x++) {
        const int Ix = srcp[x + 1] - srcp[x - 1];
        const int Iy = srcp[x] - srcpn[x];
        x2[x] = (Ix * Ix) >> 1;
        y2[x] = (Iy * Iy) >> 1;
        xy[x] = (Ix * Iy) >> 1;
    }
    {
        const int Ix = srcp[x] - srcp[x - 1];
        const int Iy = srcp[x] - srcpn[x];
        x2[x] = (Ix * Ix) >> 1;
        y2[x] = (Iy * Iy) >> 1;
        xy[x] = (Ix * Iy) >> 1;
    }
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    x2 += stride;
    y2 += stride;
    xy += stride;
    for (int y = 1; y < height - 1; y++)
    {
        {
            const int Ix = srcp[1] - srcp[0];
            const int Iy = srcpp[0] - srcpn[0];
            x2[0] = (Ix * Ix) >> 1;
            y2[0] = (Iy * Iy) >> 1;
            xy[0] = (Ix * Iy) >> 1;
        }
        for (x = 1; x < width - 1; x++) {
            const int Ix = srcp[x + 1] - srcp[x - 1];
            const int Iy = srcpp[x] - srcpn[x];
            x2[x] = (Ix * Ix) >> 1;
            y2[x] = (Iy * Iy) >> 1;
            xy[x] = (Ix * Iy) >> 1;
        }
        {
            const int Ix = srcp[x] - srcp[x - 1];
            const int Iy = srcpp[x] - srcpn[x];
            x2[x] = (Ix * Ix) >> 1;
            y2[x] = (Iy * Iy) >> 1;
            xy[x] = (Ix * Iy) >> 1;
        }
        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        x2 += stride;
        y2 += stride;
        xy += stride;
    }
    {
        const int Ix = srcp[1] - srcp[0];
        const int Iy = srcpp[0] - srcp[0];
        x2[0] = (Ix * Ix) >> 1;
        y2[0] = (Iy * Iy) >> 1;
        xy[0] = (Ix * Iy) >> 1;
    }
    for (x = 1; x < width - 1; x++) {
        const int Ix = srcp[x + 1] - srcp[x - 1];
        const int Iy = srcpp[x] - srcp[x];
        x2[x] = (Ix * Ix) >> 1;
        y2[x] = (Iy * Iy) >> 1;
        xy[x] = (Ix * Iy) >> 1;
    }
    {
        const int Ix = srcp[x] - srcp[x - 1];
        const int Iy = srcpp[x] - srcp[x];
        x2[x] = (Ix * Ix) >> 1;
        y2[x] = (Iy * Iy) >> 1;
        xy[x] = (Ix * Iy) >> 1;
    }
}

static void VS_CC eedi2Init(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    EEDI2Data * d = static_cast<EEDI2Data *>(*instanceData);
    vsapi->setVideoInfo(&d->vi2, 1, node);
}

static const VSFrameRef *VS_CC eedi2GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const EEDI2Data * d = static_cast<const EEDI2Data *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        int * cx2 = nullptr, * cy2 = nullptr, * cxy = nullptr, * tmpc = nullptr;
        if (d->pp > 1 && d->map == 0) {
            const int alignment = 32;
            const int stride = (d->vi->width * d->vi->format->bytesPerSample + (alignment - 1)) & ~(alignment - 1);
            cx2 = vs_aligned_malloc<int>(stride * d->vi->height * sizeof(int), 32);
            cy2 = vs_aligned_malloc<int>(stride * d->vi->height * sizeof(int), 32);
            cxy = vs_aligned_malloc<int>(stride * d->vi->height * sizeof(int), 32);
            tmpc = vs_aligned_malloc<int>(stride * d->vi->height * sizeof(int), 32);
            if (!cx2 || !cy2 || !cxy || !tmpc) {
                vsapi->setFilterError("EEDI2: malloc failure (pp>1)", frameCtx);
                vs_aligned_free(cx2);
                vs_aligned_free(cy2);
                vs_aligned_free(cxy);
                vs_aligned_free(tmpc);
                return nullptr;
            }
        }

        int field = d->field;
        if (d->fieldS > 1)
            field = n & 1 ? (d->fieldS == 2 ? 1 : 0) : (d->fieldS == 2 ? 0 : 1);

        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, src, core);
        VSFrameRef * msk = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, src, core);
        VSFrameRef * tmp = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, src, core);
        VSFrameRef * dst2 = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
        VSFrameRef * dst2M = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
        VSFrameRef * tmp2 = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
        VSFrameRef * tmp2_2 = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);
        VSFrameRef * msk2 = vsapi->newVideoFrame(d->vi2.format, d->vi2.width, d->vi2.height, src, core);

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int height2 = vsapi->getFrameHeight(dst2, plane);
            const int stride = vsapi->getStride(src, plane);
            const uint8_t * srcp = vsapi->getReadPtr(src, plane);
            uint8_t * dstp = vsapi->getWritePtr(dst, plane);
            uint8_t * mskp = vsapi->getWritePtr(msk, plane);
            uint8_t * tmpp = vsapi->getWritePtr(tmp, plane);
            uint8_t * dst2p = vsapi->getWritePtr(dst2, plane);
            uint8_t * dst2Mp = vsapi->getWritePtr(dst2M, plane);
            uint8_t * tmp2p = vsapi->getWritePtr(tmp2, plane);
            uint8_t * tmp2_2p = vsapi->getWritePtr(tmp2_2, plane);
            uint8_t * msk2p = vsapi->getWritePtr(msk2, plane);

            buildEdgeMask(srcp, mskp, width, height, stride, d);
            erode(mskp, tmpp, width, height, stride, d);
            dilate(tmpp, mskp, width, height, stride, d);
            erode(mskp, tmpp, width, height, stride, d);
            removeSmallHorzGaps(tmpp, mskp, width, height, stride, d);
            if (d->map != 1) {
                calcDirections(mskp, srcp, tmpp, width, height, stride, plane, d);
                filterDirMap(mskp, tmpp, dstp, width, height, stride, d);
                expandDirMap(mskp, dstp, tmpp, width, height, stride, d);
                filterMap(mskp, tmpp, dstp, width, height, stride, d);
                if (d->map == 2)
                    continue;
                memset(dst2p, 0, stride * height2);
                upscaleBy2(srcp, dst2p, width, height, stride, field, d);
                upscaleBy2(dstp, tmp2_2p, width, height, stride, field, d);
                upscaleBy2(mskp, msk2p, width, height, stride, field, d);
                markDirections2X(msk2p, tmp2_2p, tmp2p, width, height2, stride, field, d);
                filterDirMap2X(msk2p, tmp2p, dst2Mp, width, height2, stride, field, d);
                expandDirMap2X(msk2p, dst2Mp, tmp2p, width, height2, stride, field, d);
                fillGaps2X(msk2p, tmp2p, dst2Mp, width, height2, stride, field, d);
                fillGaps2X(msk2p, dst2Mp, tmp2p, width, height2, stride, field, d);
                if (d->map == 3)
                    continue;
                interpolateLattice(tmp2_2p, tmp2p, dst2p, width, height2, stride, plane, field, d);
                if (d->pp == 1 || d->pp == 3) {
                    vsapi->freeFrame(tmp2_2);
                    tmp2_2 = vsapi->copyFrame(tmp2, core);
                    tmp2_2p = vsapi->getWritePtr(tmp2_2, plane);
                    filterDirMap2X(msk2p, tmp2p, dst2Mp, width, height2, stride, field, d);
                    expandDirMap2X(msk2p, dst2Mp, tmp2p, width, height2, stride, field, d);
                    postProcess(tmp2p, tmp2_2p, dst2p, width, height2, stride, field, d);
                }
                if (d->pp == 2 || d->pp == 3) {
                    gaussianBlur1(srcp, tmpp, dstp, width, height, stride);
                    calcDerivatives(dstp, cx2, cy2, cxy, width, height, stride);
                    gaussianBlurSqrt2(cx2, tmpc, cx2, width, height, stride);
                    gaussianBlurSqrt2(cy2, tmpc, cy2, width, height, stride);
                    gaussianBlurSqrt2(cxy, tmpc, cxy, width, height, stride);
                    postProcessCorner(tmp2_2p, dst2p, cx2, cy2, cxy, width, height2, stride, field, d);
                }
            }
        }

        vs_aligned_free(cx2);
        vs_aligned_free(cy2);
        vs_aligned_free(cxy);
        vs_aligned_free(tmpc);
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
    delete d;
}

static void VS_CC eedi2Create(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    EEDI2Data d;
    int err;

    d.field = int64ToIntS(vsapi->propGetInt(in, "field", 0, nullptr));
    d.mthresh = int64ToIntS(vsapi->propGetInt(in, "mthresh", 0, &err));
    if (err)
        d.mthresh = 10;
    d.lthresh = int64ToIntS(vsapi->propGetInt(in, "lthresh", 0, &err));
    if (err)
        d.lthresh = 20;
    d.vthresh = int64ToIntS(vsapi->propGetInt(in, "vthresh", 0, &err));
    if (err)
        d.vthresh = 20;
    d.estr = int64ToIntS(vsapi->propGetInt(in, "estr", 0, &err));
    if (err)
        d.estr = 2;
    d.dstr = int64ToIntS(vsapi->propGetInt(in, "dstr", 0, &err));
    if (err)
        d.dstr = 4;
    d.maxd = int64ToIntS(vsapi->propGetInt(in, "maxd", 0, &err));
    if (err)
        d.maxd = 24;
    d.map = int64ToIntS(vsapi->propGetInt(in, "map", 0, &err));
    d.nt = int64ToIntS(vsapi->propGetInt(in, "nt", 0, &err));
    if (err)
        d.nt = 50;
    d.pp = int64ToIntS(vsapi->propGetInt(in, "pp", 0, &err));
    if (err)
        d.pp = 1;

    if (d.field < 0 || d.field > 3) {
        vsapi->setError(out, "EEDI2: field must be 0, 1, 2, or 3");
        return;
    }
    if (d.maxd > 29) {
        vsapi->setError(out, "EEDI2: maxd must be less than or equal to 29");
        return;
    }
    if (d.map < 0 || d.map > 3) {
        vsapi->setError(out, "EEDI2: map must be 0, 1, 2, or 3");
        return;
    }
    if (d.pp < 0 || d.pp > 3) {
        vsapi->setError(out, "EEDI2: pp must be 0, 1, 2, or 3");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);
    d.vi2 = *d.vi;

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bytesPerSample != 1) {
        vsapi->setError(out, "EEDI2: only constant format 8-bit integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    d.fieldS = d.field;
    if (d.fieldS == 2)
        d.field = 0;
    else if (d.fieldS == 3)
        d.field = 1;
    if (d.map == 0 || d.map == 3)
        d.vi2.height *= 2;
    d.mthresh *= d.mthresh;
    d.vthresh *= 81;
    d.limlut = {
        6, 6, 7, 7, 8, 8, 9, 9, 9, 10,
        10, 11, 11, 12, 12, 12, 12, 12, 12, 12,
        12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        12, -1, -1
    };

    EEDI2Data * data = new EEDI2Data(d);

    vsapi->createFilter(in, out, "EEDI2", eedi2Init, eedi2GetFrame, eedi2Free, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.eedi2", "eedi2", "EEDI2", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("EEDI2", "clip:clip;field:int;mthresh:int:opt;lthresh:int:opt;vthresh:int:opt;estr:int:opt;dstr:int:opt;maxd:int:opt;map:int:opt;nt:int:opt;pp:int:opt;",
                 eedi2Create, nullptr, plugin);
}
