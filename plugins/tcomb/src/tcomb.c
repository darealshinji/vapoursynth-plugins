/*
 **
 **   TComb is a temporal comb filter (it reduces cross-luminance (rainbowing)
 **   and cross-chrominance (dot crawl) artifacts in static areas of the picture).
 **   It will ONLY work with NTSC material, and WILL NOT work with telecined material
 **   where the rainbowing/dotcrawl was introduced prior to the telecine process!
 **   It must be used before ivtc or deinterlace.
 **
 **   Copyright (C) 2005-2006 Kevin Stone
 **
 **   VapourSynth port by dubhater
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



#include <VapourSynth.h>
#include <VSHelper.h>


#define min3(a,b,c) VSMIN(VSMIN(a,b),c)
#define max3(a,b,c) VSMAX(VSMAX(a,b),c)
#define min4(a,b,c,d) VSMIN(VSMIN(a,b),VSMIN(c,d))
#define max4(a,b,c,d) VSMAX(VSMAX(a,b),VSMAX(c,d))


#ifdef TCOMB_X86
// Implemented in simd_sse2.c
extern void buildFinalMask_sse2( const uint8_t *s1p, const uint8_t *s2p, const uint8_t *m1p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height, intptr_t thresh);
extern void absDiff_sse2( const uint8_t *srcp1, const uint8_t *srcp2, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
extern void absDiffAndMinMask_sse2( const uint8_t *srcp1, const uint8_t *srcp2, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
extern void absDiffAndMinMaskThresh_sse2( const uint8_t *srcp1, const uint8_t *srcp2, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height, intptr_t thresh);
extern void checkOscillation5_sse2( const uint8_t *p2p, const uint8_t *p1p, const uint8_t *s1p, const uint8_t *n1p, const uint8_t *n2p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height, intptr_t thresh);
extern void calcAverages_sse2( const uint8_t *s1p, const uint8_t *s2p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
extern void checkAvgOscCorrelation_sse2( const uint8_t *s1p, const uint8_t *s2p, const uint8_t *s3p, const uint8_t *s4p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height, intptr_t thresh);
extern void or3Masks_sse2( const uint8_t *s1p, const uint8_t *s2p, const uint8_t *s3p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
extern void orAndMasks_sse2( const uint8_t *s1p, const uint8_t *s2p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
extern void andMasks_sse2( const uint8_t *s1p, const uint8_t *s2p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
extern void checkSceneChange_sse2( const uint8_t *s1p, const uint8_t *s2p, intptr_t height, intptr_t width, intptr_t stride, int64_t *diffp);
extern void verticalBlur3_sse2( const uint8_t *srcp, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
extern void andNeighborsInPlace_sse2( uint8_t *srcp, intptr_t width, intptr_t height, intptr_t stride);
extern void minMax_sse2( const uint8_t *srcp, uint8_t *minp, uint8_t *maxp, intptr_t width, intptr_t height, intptr_t src_stride, intptr_t min_stride, intptr_t thresh);
extern void horizontalBlur3_sse2( const uint8_t *srcp, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
extern void horizontalBlur6_sse2( const uint8_t *srcp, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height);
#endif


typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    int mode;
    int fthreshl;
    int fthreshc;
    int othreshl;
    int othreshc;
    int map;
    double scthresh;

    int lc, start, stop;
    int64_t diffmaxsc;
} TCombData;



static void copyPad(const VSFrameRef *src, VSFrameRef *dst, TCombData *d, const VSAPI *vsapi)
{
    for (int b = d->start; b < d->stop; ++b) {
        int pitch = vsapi->getStride(dst, b);

        vs_bitblt(vsapi->getWritePtr(dst, b) + pitch + 1, pitch,
                vsapi->getReadPtr(src, b), vsapi->getStride(src, b),
                vsapi->getFrameWidth(src, b) * d->vi->format->bytesPerSample, vsapi->getFrameHeight(src, b));
        int height = vsapi->getFrameHeight(dst, b);
        int width = vsapi->getFrameWidth(dst, b);
        if (b == 0) {
            height -= 2;
            width -= 2;
        }
        uint8_t *dstp = vsapi->getWritePtr(dst, b) + pitch * 2;
        for (int y = 2; y < height - 2; ++y) {
            dstp[0] = dstp[1];
            dstp[width - 1] = dstp[width - 2];
            dstp += pitch;
        }

        dstp = vsapi->getWritePtr(dst, b);
        memcpy(dstp, dstp + pitch, pitch);
        memcpy(dstp + pitch * (height - 1), dstp + pitch * (height - 2), pitch);
    }
}


static void MinMax(const VSFrameRef *src, VSFrameRef *dmin, VSFrameRef *dmax, VSFrameRef *pad, TCombData *d, const VSAPI *vsapi)
{
    copyPad(src, pad, d, vsapi);

    for (int b = d->start; b < d->stop; ++b) {
        const int src_pitch = vsapi->getStride(pad, b);
        const uint8_t *srcp = vsapi->getReadPtr(pad, b) + src_pitch + 1;
        const int width = vsapi->getFrameWidth(src, b);
        const int height = vsapi->getFrameHeight(src, b);
        uint8_t *dminp = vsapi->getWritePtr(dmin, b);
        const int dmin_pitch = vsapi->getStride(dmin, b);
        uint8_t *dmaxp = vsapi->getWritePtr(dmax, b);

        const int thresh = b == 0 ? 2 : 8;

#ifdef TCOMB_X86
        minMax_sse2(srcp, dminp, dmaxp, width, height, src_pitch, dmin_pitch, thresh);
#else
        const uint8_t *srcpp = srcp - src_pitch;
        const uint8_t *srcpn = srcp + src_pitch;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                dminp[x] = VSMAX(VSMIN(VSMIN(VSMIN(VSMIN(srcpp[x - 1], srcpp[x]),
                                    VSMIN(srcpp[x + 1], srcp[x - 1])),
                                VSMIN(VSMIN(srcp[x], srcp[x + 1]),
                                    VSMIN(srcpn[x - 1], srcpn[x]))), srcpn[x + 1]) - thresh, 0);
                dmaxp[x] = VSMIN(VSMAX(VSMAX(VSMAX(VSMAX(srcpp[x - 1], srcpp[x]),
                                    VSMAX(srcpp[x + 1], srcp[x - 1])),
                                VSMAX(VSMAX(srcp[x], srcp[x + 1]),
                                    VSMAX(srcpn[x - 1], srcpn[x]))), srcpn[x + 1]) + thresh, 255);
            }
            srcpp += src_pitch;
            srcp += src_pitch;
            srcpn += src_pitch;
            dminp += dmin_pitch;
            dmaxp += dmin_pitch;
        }
#endif
    }
}


static void buildFinalFrame(const VSFrameRef *p2, const VSFrameRef *p1, const VSFrameRef *src,
        const VSFrameRef *n1, const VSFrameRef *n2, const VSFrameRef *m1, const VSFrameRef *m2, const VSFrameRef *m3,
        VSFrameRef *dst, VSFrameRef *min, VSFrameRef *max, VSFrameRef *pad, TCombData *d, const VSAPI *vsapi)
{
    if (!d->map)
        for (int b = 0; b < d->vi->format->numPlanes; ++b)
            memcpy(vsapi->getWritePtr(dst, b), vsapi->getReadPtr(src, b), vsapi->getStride(src, b) * vsapi->getFrameHeight(src, b));
    else
        for (int b = 0; b < d->vi->format->numPlanes; ++b)
            memset(vsapi->getWritePtr(dst, b), 0, vsapi->getStride(dst, b) * vsapi->getFrameHeight(dst, b));

    MinMax(src, min, max, pad, d, vsapi);

    for (int b = d->start; b < d->stop; ++b) {
        const uint8_t *p2p = vsapi->getReadPtr(p2, b);
        const int p2_pitch = vsapi->getStride(p2, b);
        const uint8_t *p1p = vsapi->getReadPtr(p1, b);
        const int p1_pitch = vsapi->getStride(p1, b);
        const uint8_t *srcp = vsapi->getReadPtr(src, b);
        const int src_pitch = vsapi->getStride(src, b);
        const int height = vsapi->getFrameHeight(src, b);
        const int width = vsapi->getFrameWidth(src, b);
        const uint8_t *n1p = vsapi->getReadPtr(n1, b);
        const int n1_pitch = vsapi->getStride(n1, b);
        const uint8_t *n2p = vsapi->getReadPtr(n2, b);
        const int n2_pitch = vsapi->getStride(n2, b);
        const uint8_t *m1p = vsapi->getReadPtr(m1, b);
        const int m1_pitch = vsapi->getStride(m1, b);
        const uint8_t *m2p = vsapi->getReadPtr(m2, b);
        const int m2_pitch = vsapi->getStride(m2, b);
        const uint8_t *m3p = vsapi->getReadPtr(m3, b);
        const int m3_pitch = vsapi->getStride(m3, b);
        const uint8_t *minp = vsapi->getReadPtr(min, b);
        const int min_pitch = vsapi->getStride(min, b);
        const uint8_t *maxp = vsapi->getReadPtr(max, b);
        const int max_pitch = vsapi->getStride(max, b);
        uint8_t *dstp = vsapi->getWritePtr(dst, b);
        const int dst_pitch = vsapi->getStride(dst, b);

        if (!d->map) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    if (m2p[x]) {
                        const int val = (p1p[x] + (srcp[x] * 2) + n1p[x] + 2) / 4;
                        if (val >= minp[x] && val <= maxp[x]) {
                            dstp[x] = val;
                            continue;
                        }
                    }
                    if (m1p[x]) {
                        const int val = (p2p[x] + (p1p[x] * 2) + srcp[x] + 2) / 4;
                        if (val >= minp[x] && val <= maxp[x]) {
                            dstp[x] = val;
                            continue;
                        }
                    }
                    if (m3p[x]) {
                        const int val = (srcp[x] + (n1p[x] * 2) + n2p[x] + 2) / 4;
                        if (val >= minp[x] && val <= maxp[x]) {
                            dstp[x] = val;
                            continue;
                        }
                    }
                }
                m1p += m1_pitch;
                m2p += m2_pitch;
                m3p += m3_pitch;
                p2p += p2_pitch;
                p1p += p1_pitch;
                srcp += src_pitch;
                n1p += n1_pitch;
                n2p += n2_pitch;
                dstp += dst_pitch;
                minp += min_pitch;
                maxp += max_pitch;
            }
        } else {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    if (m2p[x]) {
                        const int val = (p1p[x] + (srcp[x] * 2) + n1p[x] + 2) / 4;
                        if (val >= minp[x] && val <= maxp[x]) {
                            dstp[x] = 255;
                            continue;
                        }
                    }
                    if (m1p[x]) {
                        const int val = (p2p[x] + (p1p[x] * 2) + srcp[x] + 2) / 4;
                        if (val >= minp[x] && val <= maxp[x]) {
                            dstp[x] = 170;
                            continue;
                        }
                    }
                    if (m3p[x]) {
                        const int val = (srcp[x] + (n1p[x] * 2) + n2p[x] + 2) / 4;
                        if (val >= minp[x] && val <= maxp[x]) {
                            dstp[x] = 85;
                            continue;
                        }
                    }
                }
                m1p += m1_pitch;
                m2p += m2_pitch;
                m3p += m3_pitch;
                p2p += p2_pitch;
                p1p += p1_pitch;
                srcp += src_pitch;
                n1p += n1_pitch;
                n2p += n2_pitch;
                dstp += dst_pitch;
                minp += min_pitch;
                maxp += max_pitch;
            }
        }
    }
}


static void buildFinalMask(const VSFrameRef *s1, const VSFrameRef *s2, const VSFrameRef *m1,
        VSFrameRef *dst, TCombData *d, const VSAPI *vsapi)
{
    for (int b = d->start; b < d->stop; ++b) {
        const uint8_t *s1p = vsapi->getReadPtr(s1, b);
        const int stride = vsapi->getStride(s1, b);
        const int width = vsapi->getFrameWidth(s1, b);
        const int height = vsapi->getFrameHeight(s1, b);
        const uint8_t *s2p = vsapi->getReadPtr(s2, b);
        const uint8_t *m1p = vsapi->getReadPtr(m1, b);
        uint8_t *dstp = vsapi->getWritePtr(dst, b);

        const int thresh = b == 0 ? d->othreshl : d->othreshc;

#ifdef TCOMB_X86
        buildFinalMask_sse2(s1p, s2p, m1p, dstp, stride, width, height, thresh);
#else
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (m1p[x] && abs(s1p[x] - s2p[x]) < thresh)
                    dstp[x] = 0xFF;
                else
                    dstp[x] = 0;
            }
            m1p += stride;
            s1p += stride;
            s2p += stride;
            dstp += stride;
        }
#endif
    }
}


static void andNeighborsInPlace(VSFrameRef *src, const VSAPI *vsapi)
{
    uint8_t *srcp = vsapi->getWritePtr(src, 0);
    const int height = vsapi->getFrameHeight(src, 0);
    const int width = vsapi->getFrameWidth(src, 0);
    const int src_pitch = vsapi->getStride(src, 0);
    uint8_t *srcpp = srcp - src_pitch;
    uint8_t *srcpn = srcp + src_pitch;

    srcp[0] &= (srcpn[0] | srcpn[1]);
    for (int x = 1; x < width - 1; ++x)
        srcp[x] &= (srcpn[x - 1] | srcpn[x] | srcpn[x + 1]);
    srcp[width - 1] &= (srcpn[width - 2] | srcpn[width - 1]);
    srcpp += src_pitch;
    srcp += src_pitch;
    srcpn += src_pitch;

#ifdef TCOMB_X86
    const int widtha = (width % 16) ? ((width / 16) * 16) : width - 16;

    andNeighborsInPlace_sse2(srcp + 16, widtha - 16, height - 2, src_pitch);

    for (int y = 1; y < height - 1; y++) {
        srcp[0] &= (srcpp[0] | srcpp[1] | srcpn[0] | srcpn[1]);
        for (int x = 1; x < 16; x++)
            srcp[x] &= (srcpp[x - 1] | srcpp[x] | srcpp[x + 1] | srcpn[x - 1] | srcpn[x] | srcpn[x + 1]);

        for (int x = widtha; x < width - 1; x++)
            srcp[x] &= (srcpp[x - 1] | srcpp[x] | srcpp[x + 1] | srcpn[x - 1] | srcpn[x] | srcpn[x + 1]);
        srcp[width - 1] &= (srcpp[width - 2] | srcpp[width - 1] | srcpn[width - 2] | srcpn[width - 1]);

        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
    }
#else
    for (int y = 1; y < height - 1; ++y) {
        srcp[0] &= (srcpp[0] | srcpp[1] | srcpn[0] | srcpn[1]);
        for (int x = 1; x < width - 1; ++x)
            srcp[x] &= (srcpp[x - 1] | srcpp[x] | srcpp[x + 1] | srcpn[x - 1] | srcpn[x] | srcpn[x + 1]);
        srcp[width - 1] &= (srcpp[width - 2] | srcpp[width - 1] | srcpn[width - 2] | srcpn[width - 1]);
        srcpp += src_pitch;
        srcp += src_pitch;
        srcpn += src_pitch;
    }
#endif

    srcp[0] &= (srcpp[0] | srcpp[1]);
    for (int x = 1; x < width - 1; ++x)
        srcp[x] &= (srcpp[x - 1] | srcpp[x] | srcpp[x + 1]);
    srcp[width - 1] &= (srcpp[width - 2] | srcpp[width - 1]);
}


static void absDiff(const VSFrameRef *src1, const VSFrameRef *src2, VSFrameRef *dst, const VSAPI *vsapi)
{
    const uint8_t *srcp1 = vsapi->getReadPtr(src1, 0);
    const uint8_t *srcp2 = vsapi->getReadPtr(src2, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);
    const int height = vsapi->getFrameHeight(src1, 0);
    const int width = vsapi->getFrameWidth(src1, 0);
    const int stride = vsapi->getStride(src1, 0);

#ifdef TCOMB_X86
    absDiff_sse2(srcp1, srcp2, dstp, stride, width, height);
#else
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dstp[x] = abs(srcp1[x] - srcp2[x]);
        }
        srcp1 += stride;
        srcp2 += stride;
        dstp += stride;
    }
#endif
}


static void absDiffAndMinMask(const VSFrameRef *src1, const VSFrameRef *src2, VSFrameRef *dst, const VSAPI *vsapi)
{
    const uint8_t *srcp1 = vsapi->getReadPtr(src1, 0);
    const uint8_t *srcp2 = vsapi->getReadPtr(src2, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);
    const int height = vsapi->getFrameHeight(src1, 0);
    const int width = vsapi->getFrameWidth(src1, 0);
    const int stride = vsapi->getStride(src1, 0);

#ifdef TCOMB_X86
    absDiffAndMinMask_sse2(srcp1, srcp2, dstp, stride, width, height);
#else
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int diff = abs(srcp1[x] - srcp2[x]);
            if (diff < dstp[x])
                dstp[x] = diff;
        }
        srcp1 += stride;
        srcp2 += stride;
        dstp += stride;
    }
#endif
}


static void absDiffAndMinMaskThresh(const VSFrameRef *src1, const VSFrameRef *src2, VSFrameRef *dst,
        TCombData *d, const VSAPI *vsapi)
{
    const uint8_t *srcp1 = vsapi->getReadPtr(src1, 0);
    const uint8_t *srcp2 = vsapi->getReadPtr(src2, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);
    const int height = vsapi->getFrameHeight(src1, 0);
    const int width = vsapi->getFrameWidth(src1, 0);
    const int stride = vsapi->getStride(src1, 0);

    const int thresh = d->fthreshl;

#ifdef TCOMB_X86
    absDiffAndMinMaskThresh_sse2(srcp1, srcp2, dstp, stride, width, height, thresh);
#else
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int diff = abs(srcp1[x] - srcp2[x]);
            if (diff < dstp[x])
                dstp[x] = diff;
            if (dstp[x] < thresh)
                dstp[x] = 0xFF;
            else
                dstp[x] = 0;
        }
        srcp1 += stride;
        srcp2 += stride;
        dstp += stride;
    }
#endif
}


static void checkOscillation5(const VSFrameRef *p2, const VSFrameRef *p1, const VSFrameRef *s1,
        const VSFrameRef *n1, const VSFrameRef *n2, VSFrameRef *dst, TCombData *d, const VSAPI *vsapi)
{
    for (int b = d->start; b < d->stop; ++b) {
        const uint8_t *p1p = vsapi->getReadPtr(p1, b);
        const int stride = vsapi->getStride(p1, b);
        const int width = vsapi->getFrameWidth(p1, b);
        const int height = vsapi->getFrameHeight(p1, b);
        const uint8_t *p2p = vsapi->getReadPtr(p2, b);
        const uint8_t *s1p = vsapi->getReadPtr(s1, b);
        const uint8_t *n1p = vsapi->getReadPtr(n1, b);
        const uint8_t *n2p = vsapi->getReadPtr(n2, b);
        uint8_t *dstp = vsapi->getWritePtr(dst, b);

        const int thresh = b == 0 ? d->othreshl : d->othreshc;

#ifdef TCOMB_X86
        checkOscillation5_sse2(p2p, p1p, s1p, n1p, n2p, dstp, stride, width, height, thresh);
#else
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int min31 = min3(p2p[x], s1p[x], n2p[x]);
                const int max31 = max3(p2p[x], s1p[x], n2p[x]);
                const int min22 = VSMIN(p1p[x], n1p[x]);
                const int max22 = VSMAX(p1p[x], n1p[x]);
                if (((min31 > max22) || max22 == 0 || (max31 < min22) || max31 == 0) &&
                        max31 - min31 < thresh && max22 - min22 < thresh)
                    dstp[x] = 0xFF;
                else
                    dstp[x] = 0;
            }
            p2p += stride;
            p1p += stride;
            s1p += stride;
            n1p += stride;
            n2p += stride;
            dstp += stride;
        }
#endif
    }
}


static void calcAverages(const VSFrameRef *s1, const VSFrameRef *s2, VSFrameRef *dst, TCombData *d, const VSAPI *vsapi)
{
    for (int b = d->start; b < d->stop; ++b) {
        const uint8_t *s1p = vsapi->getReadPtr(s1, b);
        const int stride = vsapi->getStride(s1, b);
        const int height = vsapi->getFrameHeight(s1, b);
        const int width = vsapi->getFrameWidth(s1, b);
        const uint8_t *s2p = vsapi->getReadPtr(s2, b);
        uint8_t *dstp = vsapi->getWritePtr(dst, b);

#ifdef TCOMB_X86
        calcAverages_sse2(s1p, s2p, dstp, stride, width, height);
#else
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x)
                dstp[x] = (s1p[x] + s2p[x] + 1) / 2;
            s1p += stride;
            s2p += stride;
            dstp += stride;
        }
#endif
    }
}


static void checkAvgOscCorrelation(const VSFrameRef *s1, const VSFrameRef *s2, const VSFrameRef *s3,
        const VSFrameRef *s4, VSFrameRef *dst, TCombData *d, const VSAPI *vsapi)
{
    for (int b = d->start; b < d->stop; ++b) {
        const uint8_t *s1p = vsapi->getReadPtr(s1, b);
        const int stride = vsapi->getStride(s1, b);
        const int width = vsapi->getFrameWidth(s1, b);
        const int height = vsapi->getFrameHeight(s1, b);
        const uint8_t *s2p = vsapi->getReadPtr(s2, b);
        const uint8_t *s3p = vsapi->getReadPtr(s3, b);
        const uint8_t *s4p = vsapi->getReadPtr(s4, b);
        uint8_t *dstp = vsapi->getWritePtr(dst, b);

        const int thresh = b == 0 ? d->fthreshl : d->fthreshc;

#ifdef TCOMB_X86
        checkAvgOscCorrelation_sse2(s1p, s2p, s3p, s4p, dstp, stride, width, height, thresh);
#else
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (max4(s1p[x], s2p[x], s3p[x], s4p[x]) -
                        min4(s1p[x], s2p[x], s3p[x], s4p[x]) >= thresh)
                    dstp[x] = 0;
            }
            s1p += stride;
            s2p += stride;
            s3p += stride;
            s4p += stride;
            dstp += stride;
        }
#endif
    }
}


static void or3Masks(const VSFrameRef *s1, const VSFrameRef *s2, const VSFrameRef *s3,
        VSFrameRef *dst, const VSAPI *vsapi)
{
    for (int b = 1; b < 3; ++b) {
        const uint8_t *s1p = vsapi->getReadPtr(s1, b);
        const int stride = vsapi->getStride(s1, b);
        const int width = vsapi->getFrameWidth(s1, b);
        const int height = vsapi->getFrameHeight(s1, b);
        const uint8_t *s2p = vsapi->getReadPtr(s2, b);
        const uint8_t *s3p = vsapi->getReadPtr(s3, b);
        uint8_t *dstp = vsapi->getWritePtr(dst, b);

#ifdef TCOMB_X86
        or3Masks_sse2(s1p, s2p, s3p, dstp, stride, width, height);
#else
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                dstp[x] = (s1p[x] | s2p[x] | s3p[x]);
            }
            s1p += stride;
            s2p += stride;
            s3p += stride;
            dstp += stride;
        }
#endif
    }
}


static void orAndMasks(const VSFrameRef *s1, const VSFrameRef *s2, VSFrameRef *dst, const VSAPI *vsapi)
{
    const uint8_t *s1p = vsapi->getReadPtr(s1, 0);
    const int stride = vsapi->getStride(s1, 0);
    const int height = vsapi->getFrameHeight(s1, 0);
    const int width = vsapi->getFrameWidth(s1, 0);
    const uint8_t *s2p = vsapi->getReadPtr(s2, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);

#ifdef TCOMB_X86
    orAndMasks_sse2(s1p, s2p, dstp, stride, width, height);
#else
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dstp[x] |= (s1p[x] & s2p[x]);
        }
        s1p += stride;
        s2p += stride;
        dstp += stride;
    }
#endif
}


static void andMasks(const VSFrameRef *s1, const VSFrameRef *s2, VSFrameRef *dst, const VSAPI *vsapi)
{
    const uint8_t *s1p = vsapi->getReadPtr(s1, 0);
    const int stride = vsapi->getStride(s1, 0);
    const int height = vsapi->getFrameHeight(s1, 0);
    const int width = vsapi->getFrameWidth(s1, 0);
    const uint8_t *s2p = vsapi->getReadPtr(s2, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);

#ifdef TCOMB_X86
    andMasks_sse2(s1p, s2p, dstp, stride, width, height);
#else
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dstp[x] = (s1p[x] & s2p[x]);
        }
        s1p += stride;
        s2p += stride;
        dstp += stride;
    }
#endif
}


static int checkSceneChange(const VSFrameRef *s1, const VSFrameRef *s2, TCombData *d, const VSAPI *vsapi)
{
    if (d->scthresh < 0.0)
        return 0;

    const uint8_t *s1p = vsapi->getReadPtr(s1, 0);
    const uint8_t *s2p = vsapi->getReadPtr(s2, 0);
    const int height = vsapi->getFrameHeight(s1, 0);
    const int width = (vsapi->getFrameWidth(s1, 0) / 16) * 16;
    const int stride = vsapi->getStride(s1, 0);

    int64_t diff = 0;

#ifdef TCOMB_X86
    checkSceneChange_sse2(s1p, s2p, height, width, stride, &diff);
#else
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 4) {
            diff += abs(s1p[x + 0] - s2p[x + 0]);
            diff += abs(s1p[x + 1] - s2p[x + 1]);
            diff += abs(s1p[x + 2] - s2p[x + 2]);
            diff += abs(s1p[x + 3] - s2p[x + 3]);
        }
        s1p += stride;
        s2p += stride;
    }
#endif

    if (diff > d->diffmaxsc)
        return 1;

    return 0;
}


static void VerticalBlur3(const VSFrameRef *src, VSFrameRef *dst, const VSAPI *vsapi)
{
    const uint8_t *srcp = vsapi->getReadPtr(src, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);
    const int stride = vsapi->getStride(src, 0);
    const int width = vsapi->getFrameWidth(src, 0);
    const int height = vsapi->getFrameHeight(src, 0);

#ifdef TCOMB_X86
    verticalBlur3_sse2(srcp, dstp, stride, width, height);
#else
    const uint8_t *srcpp = srcp - stride;
    const uint8_t *srcpn = srcp + stride;

    for (int x = 0; x < width; ++x)
        dstp[x] = (srcp[x] + srcpn[x] + 1) / 2;
    srcpp += stride;
    srcp += stride;
    srcpn += stride;
    dstp += stride;
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 0; x < width; ++x)
            dstp[x] = (srcpp[x] + (srcp[x] * 2) + srcpn[x] + 2) / 4;
        srcpp += stride;
        srcp += stride;
        srcpn += stride;
        dstp += stride;
    }
    for (int x = 0; x < width; ++x)
        dstp[x] = (srcpp[x] + srcp[x] + 1) / 2;
#endif
}


static inline void horizontalBlur3_c( const uint8_t *srcp, uint8_t *dstp, int stride, int width, int height) {
    for (int y = 0; y < height; ++y) {
        dstp[0] = (srcp[0] + srcp[1] + 1) / 2;

        for (int x = 1; x < width - 1; ++x)
            dstp[x] = (srcp[x - 1] + (srcp[x] * 2) + srcp[x + 1] + 2) / 4;

        dstp[width - 1] = (srcp[width - 2] + srcp[width - 1] + 1) / 2;

        srcp += stride;
        dstp += stride;
    }
}


static void HorizontalBlur3(const VSFrameRef *src, VSFrameRef *dst, const VSAPI *vsapi)
{
    const uint8_t *srcp = vsapi->getReadPtr(src, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);
    const int stride = vsapi->getStride(src, 0);
    const int width = vsapi->getFrameWidth(src, 0);
    const int height = vsapi->getFrameHeight(src, 0);

#ifdef TCOMB_X86
    if (width >= 16) {
        const int widtha = (width / 16) * 16;

        horizontalBlur3_sse2(srcp + 16, dstp + 16, stride, widtha - 32, height);

        for (int y = 0; y < height; y++) {
            dstp[0] = (srcp[0] + srcp[1] + 1) / 2;

            for (int x = 1; x < 16; x++)
                dstp[x] = (srcp[x - 1] + (srcp[x] * 2) + srcp[x + 1] + 2) / 4;

            for (int x = widtha - 16; x < width - 1; x++)
                dstp[x] = (srcp[x - 1] + (srcp[x] * 2) + srcp[x + 1] + 2) / 4;

            dstp[width - 1] = (srcp[width - 2] + srcp[width - 1] + 1) / 2;

            srcp += stride;
            dstp += stride;
        }
    } else {
        horizontalBlur3_c(srcp, dstp, stride, width, height);
    }
#else
    horizontalBlur3_c(srcp, dstp, stride, width, height);
#endif
}


static inline void horizontalBlur6_c( const uint8_t *srcp, uint8_t *dstp, int stride, int width, int height) {
    for (int y = 0; y < height; ++y) {
        dstp[0] = (srcp[0] * 6 + (srcp[1] * 8) + (srcp[2] * 2) + 8) / 16;
        dstp[1] = (((srcp[0] + srcp[2]) * 4) + srcp[1] * 6 + (srcp[3] * 2) + 8) / 16;

        for (int x = 2; x < width - 2; ++x)
            dstp[x] = (srcp[x - 2] + ((srcp[x - 1] + srcp[x + 1]) * 4) + srcp[x] * 6 + srcp[x + 2] + 8) / 16;

        dstp[width - 2] = ((srcp[width - 4] * 2) + ((srcp[width - 3] + srcp[width - 1]) * 4) + srcp[width - 2] * 6 + 8) / 16;
        dstp[width - 1] = ((srcp[width - 3] * 2) + (srcp[width - 2] * 8) + srcp[width - 1] * 6 + 8) / 16;

        srcp += stride;
        dstp += stride;
    }
}


static void HorizontalBlur6(const VSFrameRef *src, VSFrameRef *dst, const VSAPI *vsapi)
{
    const uint8_t *srcp = vsapi->getReadPtr(src, 0);
    uint8_t *dstp = vsapi->getWritePtr(dst, 0);
    const int stride = vsapi->getStride(src, 0);
    const int width = vsapi->getFrameWidth(src, 0);
    const int height = vsapi->getFrameHeight(src, 0);

#ifdef TCOMB_X86
    if (width >= 16) {
        const int widtha = (width / 16) * 16;

        horizontalBlur6_sse2(srcp + 16, dstp + 16, stride, widtha - 32, height);

        for (int y = 0; y < height; y++) {
            dstp[0] = (srcp[0] * 6 + (srcp[1] * 8) + (srcp[2] * 2) + 8) / 16;
            dstp[1] = (((srcp[0] + srcp[2]) * 4) + srcp[1] * 6 + (srcp[3] * 2) + 8) / 16;

            for (int x = 2; x < 16; x++)
                dstp[x] = (srcp[x - 2] + ((srcp[x - 1] + srcp[x + 1]) * 4) + srcp[x] * 6 + srcp[x + 2] + 8) / 16;

            for (int x = widtha - 16; x < width - 2; x++)
                dstp[x] = (srcp[x - 2] + ((srcp[x - 1] + srcp[x + 1]) * 4) + srcp[x] * 6 + srcp[x + 2] + 8) / 16;

            dstp[width - 2] = ((srcp[width - 4] * 2) + ((srcp[width - 3] + srcp[width - 1]) * 4) + srcp[width - 2] * 6 + 8) / 16;
            dstp[width - 1] = ((srcp[width - 3] * 2) + (srcp[width - 2] * 8) + srcp[width - 1] * 6 + 8) / 16;

            srcp += stride;
            dstp += stride;
        }
    } else {
        horizontalBlur6_c(srcp, dstp, stride, width, height);
    }
#else
    horizontalBlur6_c(srcp, dstp, stride, width, height);
#endif
}


static void getStartStop(int lc, int *start, int *stop)
{
    switch (lc) {
        case 0x0:
            *stop = 0;
            return;
        case 0x1:
            *stop = 1;
            return;
        case 0x10:
            *start = 1;
            *stop = 2;
            return;
        case 0x11:
            *stop = 2;
            return;
        case 0x100:
            *start = 2;
            return;
        case 0x110:
            *start = 1;
            return;
        case 0x101:
        case 0x111:
            return;
        default:
            return;
    }
}


static void VS_CC tcombInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    TCombData *d = (TCombData *) * instanceData;

    vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC tcombStage1GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    TCombData *d = (TCombData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(VSMAX(0, n - 2), d->node, frameCtx);
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *prev = vsapi->getFrameFilter(VSMAX(0, n - 2), d->node, frameCtx);
        const VSFrameRef *cur = vsapi->getFrameFilter(n, d->node, frameCtx);

        VSFrameRef *dst = vsapi->copyFrame(cur, core);
        VSMap *props = vsapi->getFramePropsRW(dst);

        int sc = checkSceneChange(cur, prev, d, vsapi);
        vsapi->propSetInt(props, "tcomb_sc", sc, paReplace);

        if (d->lc & 0x1) {
            VSFrameRef *blurred[6];
            for (int i = 0; i < 6; i++)
                blurred[i] = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);

            HorizontalBlur3(cur, blurred[0], vsapi);
            VerticalBlur3(cur, blurred[1], vsapi);
            HorizontalBlur3(blurred[1], blurred[2], vsapi);
            HorizontalBlur6(cur, blurred[3], vsapi);
            VerticalBlur3(blurred[1], blurred[4], vsapi);
            HorizontalBlur6(blurred[4], blurred[5], vsapi);

            for (int i = 0; i < 6; i++) {
                vsapi->propSetFrame(props, "tcomb_blurred", blurred[i], paAppend);
                vsapi->freeFrame(blurred[i]);
            }
        }

        vsapi->freeFrame(prev);
        vsapi->freeFrame(cur);

        return dst;
    }

    return 0;
}


static const VSFrameRef *VS_CC tcombStage2GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    TCombData *d = (TCombData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(VSMAX(0, n - 2), d->node, frameCtx);
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *prev = vsapi->getFrameFilter(VSMAX(0, n - 2), d->node, frameCtx);
        const VSFrameRef *cur = vsapi->getFrameFilter(n, d->node, frameCtx);

        VSFrameRef *dst = vsapi->copyFrame(cur, core);
        VSMap *props = vsapi->getFramePropsRW(dst);

        if (d->mode == 0 || d->mode == 2) {
            const VSMap *prev_props = vsapi->getFramePropsRO(prev);
            const VSMap *cur_props = vsapi->getFramePropsRO(cur);

            const VSFrameRef *prev_blurred[6], *cur_blurred[6];

            for (int i = 0; i < 6; i++) {
                prev_blurred[i] = vsapi->propGetFrame(prev_props, "tcomb_blurred", i, NULL);
                cur_blurred[i] = vsapi->propGetFrame(cur_props, "tcomb_blurred", i, NULL);
            }

            VSFrameRef *msk1 = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);    

            absDiff(prev, cur, msk1, vsapi);
            for (int i = 0; i < 5; ++i)
                absDiffAndMinMask(prev_blurred[i], cur_blurred[i], msk1, vsapi);
            absDiffAndMinMaskThresh(prev_blurred[5], cur_blurred[5], msk1, d, vsapi);

            for (int i = 0; i < 6; i++) {
                vsapi->freeFrame(prev_blurred[i]);
                vsapi->freeFrame(cur_blurred[i]);
            }

            vsapi->propDeleteKey(props, "tcomb_blurred");

            vsapi->propSetFrame(props, "tcomb_msk1", msk1, paReplace);
            vsapi->freeFrame(msk1);
        }

        VSFrameRef *avg = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);    

        calcAverages(cur, prev, avg, d, vsapi);

        vsapi->propSetFrame(props, "tcomb_avg", avg, paReplace);
        vsapi->freeFrame(avg);

        vsapi->freeFrame(prev);
        vsapi->freeFrame(cur);

        return dst;
    }

    return 0;
}


static const VSFrameRef *VS_CC tcombStage3GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    TCombData *d = (TCombData *) * instanceData;

    if (activationReason == arInitial) {
        for (int i = -8; i <= 0; i += 2) {
            vsapi->requestFrameFilter(VSMAX(0, n - i), d->node, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src[5];

        for (int i = -8; i <= 0; i += 2)
            src[(i + 8) / 2] = vsapi->getFrameFilter(VSMAX(0, n - i), d->node, frameCtx);

        VSFrameRef *dst = vsapi->copyFrame(src[4], core);
        VSMap *props = vsapi->getFramePropsRW(dst);

        VSFrameRef *omsk = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);    

        checkOscillation5(src[0], src[1], src[2], src[3], src[4], omsk, d, vsapi);

        const VSFrameRef *avg[4];
        const VSMap *src_props[4];

        for (int i = 0; i < 4; i++) {
            src_props[i] = vsapi->getFramePropsRO(src[i + 1]);
            avg[i] = vsapi->propGetFrame(src_props[i], "tcomb_avg", 0, NULL);
        }

        checkAvgOscCorrelation(avg[0], avg[1], avg[2], avg[3], omsk, d, vsapi);

        for (int i = 0; i < 4; i++)
            vsapi->freeFrame(avg[i]);

        for (int i = 0; i < 5; i++)
            vsapi->freeFrame(src[i]);

        vsapi->propDeleteKey(props, "tcomb_avg");

        vsapi->propSetFrame(props, "tcomb_omsk", omsk, paReplace);
        vsapi->freeFrame(omsk);

        return dst;
    }

    return 0;
}


static const VSFrameRef *VS_CC tcombStage4GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    TCombData *d = (TCombData *) * instanceData;

    if (activationReason == arInitial) {
        for (int i = -4; i <= 6; i += 2)
            vsapi->requestFrameFilter(VSMIN(VSMAX(0, n + i), d->vi->numFrames - 1), d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src[6];

        for (int i = -4; i <= 6; i += 2)
            src[(i + 4) / 2] = vsapi->getFrameFilter(VSMIN(VSMAX(0, n + i), d->vi->numFrames - 1), d->node, frameCtx);

        VSFrameRef *dst = vsapi->copyFrame(src[2], core);

        int sc[6];
        const VSFrameRef *omsk[6];
        const VSFrameRef *msk1[6];

        const VSMap *src_props[6];

        for (int i = 1; i < 6; i++) {
            src_props[i] = vsapi->getFramePropsRO(src[i]);
            omsk[i] = vsapi->propGetFrame(src_props[i], "tcomb_omsk", 0, NULL);
        }

        for (int i = 1; i <= 2; i++) {
            sc[i] = vsapi->propGetInt(src_props[i], "tcomb_sc", 0, NULL);
            msk1[i] = vsapi->propGetFrame(src_props[i], "tcomb_msk1", 0, NULL);
        }

        VSFrameRef *msk2 = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);    
        VSFrameRef *tmp = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);    

        if (sc[1] || sc[2]) {
            for (int i = 0; i < d->vi->format->numPlanes; i++)
                memset(vsapi->getWritePtr(msk2, i), 0, vsapi->getStride(msk2, i) * vsapi->getFrameHeight(msk2, i));
        } else {
            if (d->lc & 0x1) { // if processing luma
                andMasks(omsk[1], omsk[2], tmp, vsapi);

                for (int i = 2; i < 5; i++)
                    orAndMasks(omsk[i], omsk[i + 1], tmp, vsapi);

                andNeighborsInPlace(tmp, vsapi);

                orAndMasks(msk1[1], msk1[2], tmp, vsapi);
            }
            if (d->lc & 0x110) { // if processing chroma
                or3Masks(omsk[2], omsk[3], omsk[4], tmp, vsapi);
            }
            buildFinalMask(src[0], src[2], tmp, msk2, d, vsapi);
        }

        vsapi->freeFrame(tmp);

        for (int i = 1; i < 6; i++)
            vsapi->freeFrame(omsk[i]);

        for (int i = 1; i <= 2; i++)
            vsapi->freeFrame(msk1[i]);

        for (int i = 0; i < 6; i++)
            vsapi->freeFrame(src[i]);

        VSMap *props = vsapi->getFramePropsRW(dst);
        vsapi->propSetFrame(props, "tcomb_msk2", msk2, paReplace);
        vsapi->freeFrame(msk2);

        vsapi->propDeleteKey(props, "tcomb_msk1");
        vsapi->propDeleteKey(props, "tcomb_omsk");
        vsapi->propDeleteKey(props, "tcomb_sc");

        return dst;
    }

    return 0;
}


static const VSFrameRef *VS_CC tcombStage5GetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    TCombData *d = (TCombData *) * instanceData;

    if (activationReason == arInitial) {
        for (int i = -4; i <= 4; i += 2)
            vsapi->requestFrameFilter(VSMIN(VSMAX(0, n + i), d->vi->numFrames - 1), d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src[5];

        for (int i = -4; i <= 4; i += 2)
            src[(i + 4) / 2] = vsapi->getFrameFilter(VSMIN(VSMAX(0, n + i), d->vi->numFrames - 1), d->node, frameCtx);

        const VSMap *src_props[5];
        const VSFrameRef *msk2[5];

        for (int i = 2; i < 5; i++) {
            src_props[i] = vsapi->getFramePropsRO(src[i]);
            msk2[i] = vsapi->propGetFrame(src_props[i], "tcomb_msk2", 0, NULL);
        }

        VSFrameRef *dst = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, src[2], core);

        VSFrameRef *min = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);
        VSFrameRef *max = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);
        VSFrameRef *pad = vsapi->newVideoFrame(d->vi->format, d->vi->width + 4, d->vi->height + 4, NULL, core);
    
        buildFinalFrame(src[0], src[1], src[2], src[3], src[4],
                msk2[2], msk2[3], msk2[4],
                dst, min, max, pad, d, vsapi);

        vsapi->freeFrame(min);
        vsapi->freeFrame(max);
        vsapi->freeFrame(pad);

        for (int i = 2; i < 5; i++)
            vsapi->freeFrame(msk2[i]);

        for (int i = 0; i < 5; i++)
            vsapi->freeFrame(src[i]);

        VSMap *props = vsapi->getFramePropsRW(dst);
        vsapi->propDeleteKey(props, "tcomb_msk2");

        return dst;
    }

    return 0;
}


static void VS_CC tcombFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    TCombData *d = (TCombData *)instanceData;

    vsapi->freeNode(d->node);
    free(d);
}


static int invokeCache(VSNodeRef **node, VSMap *out, VSPlugin *stdPlugin, const VSAPI *vsapi) {
    VSMap *args = vsapi->createMap();
    vsapi->propSetNode(args, "clip", *node, paReplace);
    vsapi->freeNode(*node);
    VSMap *ret = vsapi->invoke(stdPlugin, "Cache", args);
    vsapi->freeMap(args);
    if (!vsapi->getError(ret)) {
        *node = vsapi->propGetNode(ret, "clip", 0, NULL);
        vsapi->freeMap(ret);
        return 1;
    } else {
        vsapi->setError(out, vsapi->getError(ret));
        vsapi->freeMap(ret);
        return 0;
    }
}


static int invokeSeparateFields(VSNodeRef **node, VSMap *out, VSPlugin *stdPlugin, const VSAPI *vsapi) {
    VSMap *args = vsapi->createMap();
    vsapi->propSetNode(args, "clip", *node, paReplace);
    vsapi->freeNode(*node);
    vsapi->propSetInt(args, "tff", 1, paReplace);
    VSMap *ret = vsapi->invoke(stdPlugin, "SeparateFields", args);
    vsapi->freeMap(args);
    if (!vsapi->getError(ret)) {
        *node = vsapi->propGetNode(ret, "clip", 0, NULL);
        vsapi->freeMap(ret);
        return 1;
    } else {
        vsapi->setError(out, vsapi->getError(ret));
        vsapi->freeMap(ret);
        return 0;
    }
}


static int invokeDoubleWeave(VSNodeRef **node, VSMap *out, VSPlugin *stdPlugin, const VSAPI *vsapi) {
    VSMap *args = vsapi->createMap();
    vsapi->propSetNode(args, "clip", *node, paReplace);
    vsapi->freeNode(*node);
    vsapi->propSetInt(args, "tff", 1, paReplace);
    VSMap *ret = vsapi->invoke(stdPlugin, "DoubleWeave", args);
    vsapi->freeMap(args);
    if (!vsapi->getError(ret)) {
        *node = vsapi->propGetNode(ret, "clip", 0, NULL);
        vsapi->freeMap(ret);
        return 1;
    } else {
        vsapi->setError(out, vsapi->getError(ret));
        vsapi->freeMap(ret);
        return 0;
    }
}


static int invokeSelectEvery(VSNodeRef **node, VSMap *out, VSPlugin *stdPlugin, const VSAPI *vsapi) {
    VSMap *args = vsapi->createMap();
    vsapi->propSetNode(args, "clip", *node, paReplace);
    vsapi->freeNode(*node);
    vsapi->propSetInt(args, "cycle", 2, paReplace);
    vsapi->propSetInt(args, "offsets", 0, paReplace);
    VSMap *ret = vsapi->invoke(stdPlugin, "SelectEvery", args);
    vsapi->freeMap(args);
    if (!vsapi->getError(ret)) {
        *node = vsapi->propGetNode(ret, "clip", 0, NULL);
        vsapi->freeMap(ret);
        return 1;
    } else {
        vsapi->setError(out, vsapi->getError(ret));
        vsapi->freeMap(ret);
        return 0;
    }
}


static void VS_CC tcombCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    TCombData d;
    TCombData *data;
    int err;

    d.mode = vsapi->propGetInt(in, "mode", 0, &err);
    if (err)
        d.mode = 2;

    d.fthreshl = vsapi->propGetInt(in, "fthreshl", 0, &err);
    if (err)
        d.fthreshl = 4;

    d.fthreshc = vsapi->propGetInt(in, "fthreshc", 0, &err);
    if (err)
        d.fthreshc = 5;

    d.othreshl = vsapi->propGetInt(in, "othreshl", 0, &err);
    if (err)
        d.othreshl = 5;

    d.othreshc = vsapi->propGetInt(in, "othreshc", 0, &err);
    if (err)
        d.othreshc = 6;

    d.map = !!vsapi->propGetInt(in, "map", 0, &err);

    d.scthresh = vsapi->propGetFloat(in, "scthresh", 0, &err);
    if (err)
        d.scthresh = 12.0;


    if (d.mode < 0 || d.mode > 2) {
        vsapi->setError(out, "TComb: mode must be 0, 1, or 2.");
        return;
    }

    if (d.fthreshl < 1 || d.fthreshl > 255) {
        vsapi->setError(out, "TComb: fthreshl must be between 1 and 255 (inclusive).");
        return;
    }

    if (d.fthreshc < 1 || d.fthreshc > 255) {
        vsapi->setError(out, "TComb: fthreshc must be between 1 and 255 (inclusive).");
        return;
    }

    if (d.othreshl < 1 || d.othreshl > 255) {
        vsapi->setError(out, "TComb: othreshl must be between 1 and 255 (inclusive).");
        return;
    }

    if (d.othreshc < 1 || d.othreshc > 255) {
        vsapi->setError(out, "TComb: othreshc must be between 1 and 255 (inclusive).");
        return;
    }

    if (d.scthresh > 100.0) {
        vsapi->setError(out, "TComb: scthresh must not be more than 100.");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) ||
        (d.vi->format->colorFamily != cmGray && d.vi->format->colorFamily != cmYUV) ||
        d.vi->format->sampleType != stInteger ||
        d.vi->format->bitsPerSample != 8) {
        vsapi->setError(out, "TComb: Input must be 8 bit Gray or YUV with constant format and dimensions.");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi->format->colorFamily == cmGray && d.mode > 0) {
        vsapi->setError(out, "TComb: Mode must be 0 when input is Gray.");
        vsapi->freeNode(d.node);
        return;
    }

    d.lc = d.mode == 2 ? 0x111 : (d.mode == 1 ? 0x110 : 0x1);
    d.start = 0;
    d.stop = 3;
    getStartStop(d.lc, &d.start, &d.stop);

    VSPlugin *stdPlugin = vsapi->getPluginById("com.vapoursynth.std", core);

    if (!invokeSeparateFields(&d.node, out, stdPlugin, vsapi))
        return;

    // It's rather different after SeparateFields.
    d.vi = vsapi->getVideoInfo(d.node);

    d.diffmaxsc = (int64_t)((d.vi->width / 16) * 16) * d.vi->height * 219;
    if (d.scthresh >= 0.0)
        d.diffmaxsc = (int64_t)(d.diffmaxsc * d.scthresh / 100.0);

    if (!invokeCache(&d.node, out, stdPlugin, vsapi))
        return;
    
    data = malloc(sizeof(d));
    *data = d;
    vsapi->createFilter(in, out, "TCombStage1", tcombInit, tcombStage1GetFrame, tcombFree, fmParallel, 0, data, core);
    d.node = vsapi->propGetNode(out, "clip", 0, NULL);
    if (!invokeCache(&d.node, out, stdPlugin, vsapi))
        return;
    vsapi->clearMap(out);

    data = malloc(sizeof(d));
    *data = d;
    vsapi->createFilter(in, out, "TCombStage2", tcombInit, tcombStage2GetFrame, tcombFree, fmParallel, 0, data, core);
    d.node = vsapi->propGetNode(out, "clip", 0, NULL);
    if (!invokeCache(&d.node, out, stdPlugin, vsapi))
        return;
    vsapi->clearMap(out);

    data = malloc(sizeof(d));
    *data = d;
    vsapi->createFilter(in, out, "TCombStage3", tcombInit, tcombStage3GetFrame, tcombFree, fmParallel, 0, data, core);
    d.node = vsapi->propGetNode(out, "clip", 0, NULL);
    if (!invokeCache(&d.node, out, stdPlugin, vsapi))
        return;
    vsapi->clearMap(out);

    data = malloc(sizeof(d));
    *data = d;
    vsapi->createFilter(in, out, "TCombStage4", tcombInit, tcombStage4GetFrame, tcombFree, fmParallel, 0, data, core);
    d.node = vsapi->propGetNode(out, "clip", 0, NULL);
    if (!invokeCache(&d.node, out, stdPlugin, vsapi))
        return;
    vsapi->clearMap(out);

    data = malloc(sizeof(d));
    *data = d;
    vsapi->createFilter(in, out, "TComb", tcombInit, tcombStage5GetFrame, tcombFree, fmParallel, 0, data, core);
    d.node = vsapi->propGetNode(out, "clip", 0, NULL);

    if (!invokeDoubleWeave(&d.node, out, stdPlugin, vsapi))
        return;

    if (!invokeCache(&d.node, out, stdPlugin, vsapi))
        return;

    if (!invokeSelectEvery(&d.node, out, stdPlugin, vsapi))
        return;

    vsapi->propSetNode(out, "clip", d.node, paReplace);
    vsapi->freeNode(d.node);

    return;
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.tcomb", "tcomb", "Dotcrawl and rainbow remover", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("TComb",
                 "clip:clip;"
                 "mode:int:opt;"
                 "fthreshl:int:opt;"
                 "fthreshc:int:opt;"
                 "othreshl:int:opt;"
                 "othreshc:int:opt;"
                 "map:int:opt;"
                 "scthresh:float:opt;",
                 tcombCreate, 0, plugin);
}
