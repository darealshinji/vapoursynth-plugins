/*
**   VapourSynth port by HolyWu
**
**                    dfttest v1.8 for Avisynth 2.5.x
**
**   2D/3D frequency domain denoiser.
**
**   Copyright (C) 2007-2010 Kevin Stone
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

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "DFTTest.hpp"

#ifdef VS_TARGET_CPU_X86
#include "vectorclass/vectorclass.h"

template<int type> extern void filter_sse2(float *, const float *, const int, const float *, const float *, const float *) noexcept;
template<int type> extern void filter_avx2(float *, const float *, const int, const float *, const float *, const float *) noexcept;

template<typename T> extern void func_0_sse2(VSFrameRef *[3], VSFrameRef *, const DFTTestData *, const VSAPI *) noexcept;
template<typename T> extern void func_0_avx2(VSFrameRef *[3], VSFrameRef *, const DFTTestData *, const VSAPI *) noexcept;

template<typename T> extern void func_1_sse2(VSFrameRef *[15][3], VSFrameRef *, const int, const DFTTestData *, const VSAPI *) noexcept;
template<typename T> extern void func_1_avx2(VSFrameRef *[15][3], VSFrameRef *, const int, const DFTTestData *, const VSAPI *) noexcept;
#endif

#define EXTRA(a,b) (((a) % (b)) ? ((b) - ((a) % (b))) : 0)

struct NPInfo {
    int fn, b, y, x;
};

static double besselI0(double p) noexcept {
    p /= 2.;
    double n = 1., t = 1., d = 1.;
    int k = 1;
    double v;

    do {
        n *= p;
        d *= k;
        v = n / d;
        t += v * v;
    } while (++k < 15 && v > 1e-8);

    return t;
}

static double getWinValue(const double n, const double size, const int win, const double beta) noexcept {
    switch (win) {
    case 0: // hanning
        return 0.5 - 0.5 * std::cos(2. * M_PI * n / size);
    case 1: // hamming
        return 0.53836 - 0.46164 * std::cos(2. * M_PI * n / size);
    case 2: // blackman
        return 0.42 - 0.5 * std::cos(2. * M_PI * n / size) + 0.08 * std::cos(4. * M_PI * n / size);
    case 3: // 4 term blackman-harris
        return 0.35875 - 0.48829 * std::cos(2. * M_PI * n / size) + 0.14128 * std::cos(4. * M_PI * n / size) - 0.01168 * std::cos(6. * M_PI * n / size);
    case 4: // kaiser-bessel
    {
        const double v = 2. * n / size - 1.;
        return besselI0(M_PI * beta * std::sqrt(1. - v * v)) / besselI0(M_PI * beta);
    }
    case 5: // 7 term blackman-harris
        return 0.27105140069342415 -
               0.433297939234486060 * std::cos(2. * M_PI * n / size) +
               0.218122999543110620 * std::cos(4. * M_PI * n / size) -
               0.065925446388030898 * std::cos(6. * M_PI * n / size) +
               0.010811742098372268 * std::cos(8. * M_PI * n / size) -
               7.7658482522509342E-4 * std::cos(10. * M_PI * n / size) +
               1.3887217350903198E-5 * std::cos(12. * M_PI * n / size);
    case 6: // flat top
        return 0.2810639 - 0.5208972 * std::cos(2. * M_PI * n / size) + 0.1980399 * std::cos(4. * M_PI * n / size);
    case 7: // rectangular
        return 1.;
    case 8: // Bartlett
        return 2. / size * (size / 2. - std::abs(n - size / 2.));
    case 9: // Bartlett-Hann
        return 0.62 - 0.48 * (n / size - 0.5) - 0.38 * std::cos(2. * M_PI * n / size);
    case 10: // Nuttall
        return 0.355768 - 0.487396 * std::cos(2. * M_PI * n / size) + 0.144232 * std::cos(4. * M_PI * n / size) - 0.012604 * std::cos(6. * M_PI * n / size);
    case 11: // Blackman-Nuttall
        return 0.3635819 - 0.4891775 * std::cos(2. * M_PI * n / size) + 0.1365995 * std::cos(4. * M_PI * n / size) - 0.0106411 * std::cos(6. * M_PI * n / size);
    default:
        return 0.;
    }
}

static void normalizeForOverlapAdd(double * VS_RESTRICT hw, const int bsize, const int osize) noexcept {
    double * VS_RESTRICT nw = new double[bsize]();
    const int inc = bsize - osize;

    for (int q = 0; q < bsize; q++) {
        for (int h = q; h >= 0; h -= inc)
            nw[q] += hw[h] * hw[h];
        for (int h = q + inc; h < bsize; h += inc)
            nw[q] += hw[h] * hw[h];
    }

    for (int q = 0; q < bsize; q++)
        hw[q] /= std::sqrt(nw[q]);

    delete[] nw;
}

static void createWindow(float * VS_RESTRICT hw, const int tmode, const int smode, const DFTTestData * d) noexcept {
    double * VS_RESTRICT tw = new double[d->tbsize];
    for (int j = 0; j < d->tbsize; j++)
        tw[j] = getWinValue(j + 0.5, d->tbsize, d->twin, d->tbeta);
    if (tmode == 1)
        normalizeForOverlapAdd(tw, d->tbsize, d->tosize);

    double * VS_RESTRICT sw = new double[d->sbsize];
    for (int j = 0; j < d->sbsize; j++)
        sw[j] = getWinValue(j + 0.5, d->sbsize, d->swin, d->sbeta);
    if (smode == 1)
        normalizeForOverlapAdd(sw, d->sbsize, d->sosize);

    const double nscale = 1. / std::sqrt(d->bvolume);
    for (int j = 0; j < d->tbsize; j++)
        for (int k = 0; k < d->sbsize; k++)
            for (int q = 0; q < d->sbsize; q++)
                hw[(j * d->sbsize + k) * d->sbsize + q] = static_cast<float>(tw[j] * sw[k] * sw[q] * nscale);

    delete[] tw;
    delete[] sw;
}

static float * parseString(const char * s, int & poscnt, const float sigma, const float pfact) {
    float * parray = nullptr;

    if (s[0] == 0) {
        parray = new float[4];
        parray[0] = 0.f;
        parray[2] = 1.f;
        parray[1] = parray[3] = std::pow(sigma, pfact);
        poscnt = 2;
    } else {
        poscnt = 0;
        bool found[2] = { false, false };
        const char * sT = s;

        while (sT[0] != 0) {
            float pos, sval;

            if (std::sscanf(sT, "%f:%f", &pos, &sval) != 2)
                throw std::string{ "invalid entry in sigma string" };

            if (pos < 0.f || pos > 1.f)
                throw std::string{ "sigma string - invalid pos (" } + std::to_string(pos) + ")";

            if (pos == 0.f)
                found[0] = true;
            else if (pos == 1.f)
                found[1] = true;

            poscnt++;

            while (sT[1] != 0 && sT[1] != ' ')
                sT++;
            sT++;
        }

        if (!found[0] || !found[1])
            throw std::string{ "sigma string - one or more end points not provided" };

        parray = new float[poscnt * 2];
        sT = s;
        poscnt = 0;

        while (sT[0] != 0) {
            std::sscanf(sT, "%f:%f", &parray[poscnt * 2], &parray[poscnt * 2 + 1]);
            parray[poscnt * 2 + 1] = std::pow(parray[poscnt * 2 + 1], pfact);

            poscnt++;

            while (sT[1] != 0 && sT[1] != ' ')
                sT++;
            sT++;
        }

        for (int i = 1; i < poscnt; i++) {
            int j = i;
            const float t0 = parray[j * 2];
            const float t1 = parray[j * 2 + 1];

            while (j > 0 && parray[(j - 1) * 2] > t0) {
                parray[j * 2] = parray[(j - 1) * 2];
                parray[j * 2 + 1] = parray[(j - 1) * 2 + 1];
                j--;
            }

            parray[j * 2] = t0;
            parray[j * 2 + 1] = t1;
        }
    }

    return parray;
}

static float interp(const float pf, const float * pv, const int cnt) noexcept {
    int lidx = 0;
    for (int i = cnt - 1; i >= 0; i--) {
        if (pv[i * 2] <= pf) {
            lidx = i;
            break;
        }
    }

    int hidx = cnt - 1;
    for (int i = 0; i < cnt; i++) {
        if (pv[i * 2] >= pf) {
            hidx = i;
            break;
        }
    }

    const float d0 = pf - pv[lidx * 2];
    const float d1 = pv[hidx * 2] - pf;

    if (hidx == lidx || d0 <= 0.f)
        return pv[lidx * 2 + 1];
    if (d1 <= 0.f)
        return pv[hidx * 2 + 1];

    const float tf = d0 / (d0 + d1);
    return pv[lidx * 2 + 1] * (1.f - tf) + pv[hidx * 2 + 1] * tf;
}

static float getSVal(const int pos, const int len, const float * pv, const int cnt, float & pf) noexcept {
    if (len == 1) {
        pf = 0.f;
        return 1.f;
    }

    const int ld2 = len / 2;
    if (pos > ld2)
        pf = (len - pos) / static_cast<float>(ld2);
    else
        pf = pos / static_cast<float>(ld2);

    return interp(pf, pv, cnt);
}

template<typename T>
static void copyPad(const VSFrameRef * src, VSFrameRef * dst[3], const DFTTestData * d, const VSAPI * vsapi) noexcept {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int srcWidth = vsapi->getFrameWidth(src, plane);
            const int dstWidth = vsapi->getFrameWidth(dst[plane], 0);
            const int srcHeight = vsapi->getFrameHeight(src, plane);
            const int dstHeight = vsapi->getFrameHeight(dst[plane], 0);
            const int dstStride = vsapi->getStride(dst[plane], 0) / sizeof(T);

            const int offy = (dstHeight - srcHeight) / 2;
            const int offx = (dstWidth - srcWidth) / 2;

            vs_bitblt(vsapi->getWritePtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * offy + offx * sizeof(T), vsapi->getStride(dst[plane], 0),
                      vsapi->getReadPtr(src, plane), vsapi->getStride(src, plane),
                      srcWidth * sizeof(T), srcHeight);

            T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst[plane], 0)) + dstStride * offy;

            for (int y = offy; y < srcHeight + offy; y++) {
                int w = offx * 2;
                for (int x = 0; x < offx; x++, w--)
                    dstp[x] = dstp[w];

                w = offx + srcWidth - 2;
                for (int x = offx + srcWidth; x < dstWidth; x++, w--)
                    dstp[x] = dstp[w];

                dstp += dstStride;
            }

            int w = offy * 2;
            for (int y = 0; y < offy; y++, w--)
                memcpy(vsapi->getWritePtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * y,
                       vsapi->getReadPtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * w,
                       dstWidth * sizeof(T));

            w = offy + srcHeight - 2;
            for (int y = offy + srcHeight; y < dstHeight; y++, w--)
                memcpy(vsapi->getWritePtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * y,
                       vsapi->getReadPtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * w,
                       dstWidth * sizeof(T));
        }
    }
}

template<typename T>
static inline void proc0(const T * s0, const float * s1, float * VS_RESTRICT d, const int p0, const int p1, const float divisor) noexcept;

template<>
inline void proc0(const uint8_t * s0, const float * s1, float * VS_RESTRICT d, const int p0, const int p1, const float divisor) noexcept {
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v++)
            d[v] = s0[v] * s1[v];

        s0 += p0;
        s1 += p1;
        d += p1;
    }
}

template<>
inline void proc0(const uint16_t * s0, const float * s1, float * VS_RESTRICT d, const int p0, const int p1, const float divisor) noexcept {
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v++)
            d[v] = s0[v] * divisor * s1[v];

        s0 += p0;
        s1 += p1;
        d += p1;
    }
}

template<>
inline void proc0(const float * s0, const float * s1, float * VS_RESTRICT d, const int p0, const int p1, const float divisor) noexcept {
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v++)
            d[v] = s0[v] * 255.f * s1[v];

        s0 += p0;
        s1 += p1;
        d += p1;
    }
}

static inline void proc1(const float * s0, const float * s1, float * VS_RESTRICT d, const int p0, const int p1) noexcept {
    for (int u = 0; u < p0; u++) {
        for (int v = 0; v < p0; v++)
            d[v] += s0[v] * s1[v];

        s0 += p0;
        s1 += p0;
        d += p1;
    }
}

static inline void removeMean(float * VS_RESTRICT dftc, const float * dftgc, const int ccnt, float * VS_RESTRICT dftc2) noexcept {
    const float gf = dftc[0] / dftgc[0];

    for (int h = 0; h < ccnt; h += 2) {
        dftc2[h] = gf * dftgc[h];
        dftc2[h + 1] = gf * dftgc[h + 1];
        dftc[h] -= dftc2[h];
        dftc[h + 1] -= dftc2[h + 1];
    }
}

static inline void addMean(float * VS_RESTRICT dftc, const int ccnt, const float * dftc2) noexcept {
    for (int h = 0; h < ccnt; h += 2) {
        dftc[h] += dftc2[h];
        dftc[h + 1] += dftc2[h + 1];
    }
}

template<int type>
static inline void filter_c(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept;

template<>
inline void filter_c<0>(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];
        const float mult = std::max((psd - sigmas[h]) / (psd + 1e-15f), 0.f);
        dftc[h] *= mult;
        dftc[h + 1] *= mult;
    }
}

template<>
inline void filter_c<1>(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];
        if (psd < sigmas[h])
            dftc[h] = dftc[h + 1] = 0.f;
    }
}

template<>
inline void filter_c<2>(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 2) {
        dftc[h] *= sigmas[h];
        dftc[h + 1] *= sigmas[h];
    }
}

template<>
inline void filter_c<3>(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];

        if (psd >= pmin[h] && psd <= pmax[h]) {
            dftc[h] *= sigmas[h];
            dftc[h + 1] *= sigmas[h];
        } else {
            dftc[h] *= sigmas2[h];
            dftc[h + 1] *= sigmas2[h];
        }
    }
}

template<>
inline void filter_c<4>(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1] + 1e-15f;
        const float mult = sigmas[h] * std::sqrt(psd * pmax[h] / ((psd + pmin[h]) * (psd + pmax[h])));
        dftc[h] *= mult;
        dftc[h + 1] *= mult;
    }
}

template<>
inline void filter_c<5>(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    const float beta = pmin[0];

    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];
        const float mult = std::pow(std::max((psd - sigmas[h]) / (psd + 1e-15f), 0.f), beta);
        dftc[h] *= mult;
        dftc[h + 1] *= mult;
    }
}

template<>
inline void filter_c<6>(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];
        const float mult = std::sqrt(std::max((psd - sigmas[h]) / (psd + 1e-15f), 0.f));
        dftc[h] *= mult;
        dftc[h + 1] *= mult;
    }
}

template<typename T>
static void cast(const float * ebp, T * VS_RESTRICT dstp, const int dstWidth, const int dstHeight, const int dstStride, const int ebpStride,
                 const float multiplier, const int peak) noexcept;

template<>
void cast(const float * ebp, uint8_t * VS_RESTRICT dstp, const int dstWidth, const int dstHeight, const int dstStride, const int ebpStride,
          const float multiplier, const int peak) noexcept {
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x++)
            dstp[x] = std::min(std::max(static_cast<int>(ebp[x] + 0.5f), 0), 255);

        ebp += ebpStride;
        dstp += dstStride;
    }
}

template<>
void cast(const float * ebp, uint16_t * VS_RESTRICT dstp, const int dstWidth, const int dstHeight, const int dstStride, const int ebpStride,
          const float multiplier, const int peak) noexcept {
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x++)
            dstp[x] = std::min(std::max(static_cast<int>(ebp[x] * multiplier + 0.5f), 0), peak);

        ebp += ebpStride;
        dstp += dstStride;
    }
}

template<>
void cast(const float * ebp, float * VS_RESTRICT dstp, const int dstWidth, const int dstHeight, const int dstStride, const int ebpStride,
          const float multiplier, const int peak) noexcept {
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x++)
            dstp[x] = ebp[x] * (1.f / 255.f);

        ebp += ebpStride;
        dstp += dstStride;
    }
}

template<typename T>
static void func_0_c(VSFrameRef * src[3], VSFrameRef * dst, const DFTTestData * d, const VSAPI * vsapi) noexcept {
    const auto threadId = std::this_thread::get_id();
    float * ebuff = reinterpret_cast<float *>(vsapi->getWritePtr(d->ebuff.at(threadId), 0));
    float * dftr = d->dftr.at(threadId);
    fftwf_complex * dftc = d->dftc.at(threadId);
    fftwf_complex * dftc2 = d->dftc2.at(threadId);

    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = d->padWidth[plane];
            const int height = d->padHeight[plane];
            const int eheight = d->eheight[plane];
            const int srcStride = vsapi->getStride(src[plane], 0) / sizeof(T);
            const int ebpStride = vsapi->getStride(d->ebuff.at(threadId), 0) / sizeof(float);
            const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src[plane], 0));
            float * ebpSaved = ebuff;

            memset(ebuff, 0, ebpStride * height * sizeof(float));

            for (int y = 0; y < eheight; y += d->inc) {
                for (int x = 0; x <= width - d->sbsize; x += d->inc) {
                    proc0(srcp + x, d->hw, dftr, srcStride, d->sbsize, d->divisor);

                    fftwf_execute_dft_r2c(d->ft, dftr, dftc);
                    if (d->zmean)
                        removeMean(reinterpret_cast<float *>(dftc), reinterpret_cast<const float *>(d->dftgc), d->ccnt2, reinterpret_cast<float *>(dftc2));

                    d->filterCoeffs(reinterpret_cast<float *>(dftc), d->sigmas, d->ccnt2, d->uf0b ? &d->f0beta : d->pmins, d->pmaxs, d->sigmas2);

                    if (d->zmean)
                        addMean(reinterpret_cast<float *>(dftc), d->ccnt2, reinterpret_cast<const float *>(dftc2));
                    fftwf_execute_dft_c2r(d->fti, dftc, dftr);

                    if (d->type & 1) // spatial overlapping
                        proc1(dftr, d->hw, ebpSaved + x, d->sbsize, ebpStride);
                    else
                        ebpSaved[x + d->sbd1 * ebpStride + d->sbd1] = dftr[d->sbd1 * d->sbsize + d->sbd1] * d->hw[d->sbd1 * d->sbsize + d->sbd1];
                }

                srcp += srcStride * d->inc;
                ebpSaved += ebpStride * d->inc;
            }

            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int dstStride = vsapi->getStride(dst, plane) / sizeof(T);
            T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
            const float * ebp = ebuff + ebpStride * ((height - dstHeight) / 2) + (width - dstWidth) / 2;
            cast(ebp, dstp, dstWidth, dstHeight, dstStride, ebpStride, d->multiplier, d->peak);
        }
    }
}

template<typename T>
static void func_1_c(VSFrameRef * src[15][3], VSFrameRef * dst, const int pos, const DFTTestData * d, const VSAPI * vsapi) noexcept {
    const auto threadId = std::this_thread::get_id();
    float * ebuff = reinterpret_cast<float *>(vsapi->getWritePtr(d->ebuff.at(threadId), 0));
    float * dftr = d->dftr.at(threadId);
    fftwf_complex * dftc = d->dftc.at(threadId);
    fftwf_complex * dftc2 = d->dftc2.at(threadId);

    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = d->padWidth[plane];
            const int height = d->padHeight[plane];
            const int eheight = d->eheight[plane];
            const int srcStride = vsapi->getStride(src[0][plane], 0) / sizeof(T);
            const int ebpStride = vsapi->getStride(d->ebuff.at(threadId), 0) / sizeof(float);
            const T * srcp[15] = {};
            for (int i = 0; i < d->tbsize; i++)
                srcp[i] = reinterpret_cast<const T *>(vsapi->getReadPtr(src[i][plane], 0));

            memset(ebuff, 0, ebpStride * height * sizeof(float));

            for (int y = 0; y < eheight; y += d->inc) {
                for (int x = 0; x <= width - d->sbsize; x += d->inc) {
                    for (int z = 0; z < d->tbsize; z++)
                        proc0(srcp[z] + x, d->hw + d->barea * z, dftr + d->barea * z, srcStride, d->sbsize, d->divisor);

                    fftwf_execute_dft_r2c(d->ft, dftr, dftc);
                    if (d->zmean)
                        removeMean(reinterpret_cast<float *>(dftc), reinterpret_cast<const float *>(d->dftgc), d->ccnt2, reinterpret_cast<float *>(dftc2));

                    d->filterCoeffs(reinterpret_cast<float *>(dftc), d->sigmas, d->ccnt2, d->uf0b ? &d->f0beta : d->pmins, d->pmaxs, d->sigmas2);

                    if (d->zmean)
                        addMean(reinterpret_cast<float *>(dftc), d->ccnt2, reinterpret_cast<const float *>(dftc2));
                    fftwf_execute_dft_c2r(d->fti, dftc, dftr);

                    if (d->type & 1) // spatial overlapping
                        proc1(dftr + pos * d->barea, d->hw + pos * d->barea, ebuff + y * ebpStride + x, d->sbsize, ebpStride);
                    else
                        ebuff[(y + d->sbd1) * ebpStride + x + d->sbd1] = dftr[pos * d->barea + d->sbd1 * d->sbsize + d->sbd1] * d->hw[pos * d->barea + d->sbd1 * d->sbsize + d->sbd1];
                }

                for (int q = 0; q < d->tbsize; q++)
                    srcp[q] += srcStride * d->inc;
            }

            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int dstStride = vsapi->getStride(dst, plane) / sizeof(T);
            T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
            const float * ebp = ebuff + ebpStride * ((height - dstHeight) / 2) + (width - dstWidth) / 2;
            cast(ebp, dstp, dstWidth, dstHeight, dstStride, ebpStride, d->multiplier, d->peak);
        }
    }
}

static void selectFunctions(const unsigned ftype, const unsigned opt, DFTTestData * d) noexcept {
    if (ftype == 0) {
        if (std::abs(d->f0beta - 1.f) < 0.00005f)
            d->filterCoeffs = filter_c<0>;
        else if (std::abs(d->f0beta - 0.5f) < 0.00005f)
            d->filterCoeffs = filter_c<6>;
        else
            d->filterCoeffs = filter_c<5>;
    } else if (ftype == 1) {
        d->filterCoeffs = filter_c<1>;
    } else if (ftype == 2) {
        d->filterCoeffs = filter_c<2>;
    } else if (ftype == 3) {
        d->filterCoeffs = filter_c<3>;
    } else {
        d->filterCoeffs = filter_c<4>;
    }

    if (d->vi->format->bytesPerSample == 1) {
        d->copyPad = copyPad<uint8_t>;
        d->func_0 = func_0_c<uint8_t>;
        d->func_1 = func_1_c<uint8_t>;
    } else if (d->vi->format->bytesPerSample == 2) {
        d->copyPad = copyPad<uint16_t>;
        d->func_0 = func_0_c<uint16_t>;
        d->func_1 = func_1_c<uint16_t>;
    } else {
        d->copyPad = copyPad<float>;
        d->func_0 = func_0_c<float>;
        d->func_1 = func_1_c<float>;
    }

#ifdef VS_TARGET_CPU_X86
    const int iset = instrset_detect();

    if ((opt == 0 && iset >= 8) || opt == 3) {
        if (ftype == 0) {
            if (std::abs(d->f0beta - 1.f) < 0.00005f)
                d->filterCoeffs = filter_avx2<0>;
            else if (std::abs(d->f0beta - 0.5f) < 0.00005f)
                d->filterCoeffs = filter_avx2<6>;
            else
                d->filterCoeffs = filter_avx2<5>;
        } else if (ftype == 1) {
            d->filterCoeffs = filter_avx2<1>;
        } else if (ftype == 2) {
            d->filterCoeffs = filter_avx2<2>;
        } else if (ftype == 3) {
            d->filterCoeffs = filter_avx2<3>;
        } else {
            d->filterCoeffs = filter_avx2<4>;
        }

        if (d->vi->format->bytesPerSample == 1) {
            d->func_0 = func_0_avx2<uint8_t>;
            d->func_1 = func_1_avx2<uint8_t>;
        } else if (d->vi->format->bytesPerSample == 2) {
            d->func_0 = func_0_avx2<uint16_t>;
            d->func_1 = func_1_avx2<uint16_t>;
        } else {
            d->func_0 = func_0_avx2<float>;
            d->func_1 = func_1_avx2<float>;
        }
    } else if ((opt == 0 && iset >= 2) || opt == 2) {
        if (ftype == 0) {
            if (std::abs(d->f0beta - 1.f) < 0.00005f)
                d->filterCoeffs = filter_sse2<0>;
            else if (std::abs(d->f0beta - 0.5f) < 0.00005f)
                d->filterCoeffs = filter_sse2<6>;
            else
                d->filterCoeffs = filter_sse2<5>;
        } else if (ftype == 1) {
            d->filterCoeffs = filter_sse2<1>;
        } else if (ftype == 2) {
            d->filterCoeffs = filter_sse2<2>;
        } else if (ftype == 3) {
            d->filterCoeffs = filter_sse2<3>;
        } else {
            d->filterCoeffs = filter_sse2<4>;
        }

        if (d->vi->format->bytesPerSample == 1) {
            d->func_0 = func_0_sse2<uint8_t>;
            d->func_1 = func_1_sse2<uint8_t>;
        } else if (d->vi->format->bytesPerSample == 2) {
            d->func_0 = func_0_sse2<uint16_t>;
            d->func_1 = func_1_sse2<uint16_t>;
        } else {
            d->func_0 = func_0_sse2<float>;
            d->func_1 = func_1_sse2<float>;
        }
    }
#endif
}

static void VS_CC dfttestInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DFTTestData * d = static_cast<DFTTestData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC dfttestGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DFTTestData * d = static_cast<DFTTestData *>(*instanceData);

    if (activationReason == arInitial) {
        if (d->tbsize == 1) {
            vsapi->requestFrameFilter(n, d->node, frameCtx);
        } else {
            const int start = std::max(n - d->tbsize / 2, 0);
            const int stop = std::min(n + d->tbsize / 2, d->vi->numFrames - 1);
            for (int i = start; i <= stop; i++)
                vsapi->requestFrameFilter(i, d->node, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        try {
            auto threadId = std::this_thread::get_id();

            if (!d->ebuff.count(threadId))
                d->ebuff.emplace(threadId, vsapi->newVideoFrame(vsapi->registerFormat(cmGray, stFloat, 32, 0, 0, core), d->padWidth[0], d->padHeight[0], nullptr, core));

            if (!d->dftr.count(threadId)) {
                float * dftr = vs_aligned_malloc<float>((d->bvolume + 7) * sizeof(float), 32);
                if (!dftr)
                    throw std::string{ "malloc failure (dftr)" };
                d->dftr.emplace(threadId, dftr);
            }

            if (!d->dftc.count(threadId)) {
                fftwf_complex * dftc = vs_aligned_malloc<fftwf_complex>((d->ccnt + 7) * sizeof(fftwf_complex), 32);
                if (!dftc)
                    throw std::string{ "malloc failure (dftc)" };
                d->dftc.emplace(threadId, dftc);
            }

            if (!d->dftc2.count(threadId)) {
                fftwf_complex * dftc2 = vs_aligned_malloc<fftwf_complex>((d->ccnt + 7) * sizeof(fftwf_complex), 32);
                if (!dftc2)
                    throw std::string{ "malloc failure (dftc2)" };
                d->dftc2.emplace(threadId, dftc2);
            }
        } catch (const std::string & error) {
            vsapi->setFilterError(("DFTTest: " + error).c_str(), frameCtx);
            return nullptr;
        }

        const VSFrameRef * src0 = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src0, d->process[1] ? nullptr : src0, d->process[2] ? nullptr : src0 };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src0, core);
        vsapi->freeFrame(src0);

        if (d->tbsize == 1) {
            const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
            VSFrameRef * pad[3] = {};

            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (d->process[plane])
                    pad[plane] = vsapi->newVideoFrame(d->padFormat, d->padWidth[plane], d->padHeight[plane], nullptr, core);
            }

            d->copyPad(src, pad, d, vsapi);
            d->func_0(pad, dst, d, vsapi);

            vsapi->freeFrame(src);
            for (int plane = 0; plane < d->vi->format->numPlanes; plane++)
                vsapi->freeFrame(pad[plane]);
        } else {
            const VSFrameRef * src[15] = {};
            VSFrameRef * pad[15][3] = {};

            const int pos = d->tbsize / 2;

            for (int i = n - pos; i <= n + pos; i++) {
                src[i - n + pos] = vsapi->getFrameFilter(std::min(std::max(i, 0), d->vi->numFrames - 1), d->node, frameCtx);

                for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                    if (d->process[plane])
                        pad[i - n + pos][plane] = vsapi->newVideoFrame(d->padFormat, d->padWidth[plane], d->padHeight[plane], nullptr, core);
                }

                d->copyPad(src[i - n + pos], pad[i - n + pos], d, vsapi);
            }

            d->func_1(pad, dst, pos, d, vsapi);

            for (int i = n - pos; i <= n + pos; i++) {
                vsapi->freeFrame(src[i - n + pos]);
                for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                    vsapi->freeFrame(pad[i - n + pos][plane]);
                }
            }
        }

        return dst;
    }

    return nullptr;
}

static void VS_CC dfttestFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DFTTestData * d = static_cast<DFTTestData *>(instanceData);

    vsapi->freeNode(d->node);

    vs_aligned_free(d->hw);
    vs_aligned_free(d->dftgc);
    vs_aligned_free(d->sigmas);
    vs_aligned_free(d->sigmas2);
    vs_aligned_free(d->pmins);
    vs_aligned_free(d->pmaxs);

    fftwf_destroy_plan(d->ft);
    fftwf_destroy_plan(d->fti);

    for (auto & iter : d->ebuff)
        vsapi->freeFrame(iter.second);

    for (auto & iter : d->dftr)
        vs_aligned_free(iter.second);

    for (auto & iter : d->dftc)
        vs_aligned_free(iter.second);

    for (auto & iter : d->dftc2)
        vs_aligned_free(iter.second);

    delete d;
}

static void VS_CC dfttestCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<DFTTestData> d{ new DFTTestData{} };
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    try {
        if (!isConstantFormat(d->vi) || (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw std::string{ "only constant format 8-16 bit integer and 32 bit float input supported" };

        const int ftype = int64ToIntS(vsapi->propGetInt(in, "ftype", 0, &err));

        float sigma = static_cast<float>(vsapi->propGetFloat(in, "sigma", 0, &err));
        if (err)
            sigma = 8.f;

        float sigma2 = static_cast<float>(vsapi->propGetFloat(in, "sigma2", 0, &err));
        if (err)
            sigma2 = 8.f;

        const float pmin = static_cast<float>(vsapi->propGetFloat(in, "pmin", 0, &err));

        float pmax = static_cast<float>(vsapi->propGetFloat(in, "pmax", 0, &err));
        if (err)
            pmax = 500.f;

        d->sbsize = int64ToIntS(vsapi->propGetInt(in, "sbsize", 0, &err));
        if (err)
            d->sbsize = 16;

        int smode = int64ToIntS(vsapi->propGetInt(in, "smode", 0, &err));
        if (err)
            smode = 1;

        d->sosize = int64ToIntS(vsapi->propGetInt(in, "sosize", 0, &err));
        if (err)
            d->sosize = 12;

        d->tbsize = int64ToIntS(vsapi->propGetInt(in, "tbsize", 0, &err));
        if (err)
            d->tbsize = 3;

        const int tmode = int64ToIntS(vsapi->propGetInt(in, "tmode", 0, &err));

        d->tosize = int64ToIntS(vsapi->propGetInt(in, "tosize", 0, &err));

        d->swin = int64ToIntS(vsapi->propGetInt(in, "swin", 0, &err));

        d->twin = int64ToIntS(vsapi->propGetInt(in, "twin", 0, &err));
        if (err)
            d->twin = 7;

        d->sbeta = static_cast<float>(vsapi->propGetFloat(in, "sbeta", 0, &err));
        if (err)
            d->sbeta = 2.5f;

        d->tbeta = static_cast<float>(vsapi->propGetFloat(in, "tbeta", 0, &err));
        if (err)
            d->tbeta = 2.5f;

        d->zmean = !!vsapi->propGetInt(in, "zmean", 0, &err);
        if (err)
            d->zmean = true;

        d->f0beta = static_cast<float>(vsapi->propGetFloat(in, "f0beta", 0, &err));
        if (err)
            d->f0beta = 1.f;

        const char * nstring = vsapi->propGetData(in, "nstring", 0, &err);
        if (err)
            nstring = "";

        const char * sstring = vsapi->propGetData(in, "sstring", 0, &err);
        if (err)
            sstring = "";

        const char * ssx = vsapi->propGetData(in, "ssx", 0, &err);
        if (err)
            ssx = "";

        const char * ssy = vsapi->propGetData(in, "ssy", 0, &err);
        if (err)
            ssy = "";

        const char * sst = vsapi->propGetData(in, "sst", 0, &err);
        if (err)
            sst = "";

        const int m = vsapi->propNumElements(in, "planes");

        for (int i = 0; i < 3; i++)
            d->process[i] = (m <= 0);

        for (int i = 0; i < m; i++) {
            const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

            if (n < 0 || n >= d->vi->format->numPlanes)
                throw std::string{ "plane index out of range" };

            if (d->process[n])
                throw std::string{ "plane specified twice" };

            d->process[n] = true;
        }

        const int opt = int64ToIntS(vsapi->propGetInt(in, "opt", 0, &err));

        if (ftype < 0 || ftype > 4)
            throw std::string{ "ftype must be 0, 1, 2, 3 or 4" };

        if (d->sbsize < 1)
            throw std::string{ "sbsize must be greater than or equal to 1" };

        if (smode < 0 || smode > 1)
            throw std::string{ "smode must be 0 or 1" };

        if (smode == 0 && !(d->sbsize & 1))
            throw std::string{ "sbsize must be odd when using smode=0" };

        if (smode == 0)
            d->sosize = 0;

        if (d->sosize < 0 || d->sosize >= d->sbsize)
            throw std::string{ "sosize must be between 0 and sbsize-1 (inclusive)" };

        if (d->sosize > d->sbsize / 2 && d->sbsize % (d->sbsize - d->sosize) != 0)
            throw std::string{ "spatial overlap greater than 50% requires that sbsize-sosize is a divisor of sbsize" };

        if (d->tbsize < 1 || d->tbsize > 15)
            throw std::string{ "tbsize must be between 1 and 15 (inclusive)" };

        if (tmode != 0)
            throw std::string{ "tmode must be 0. tmode=1 is not implemented" };

        if (tmode == 0 && !(d->tbsize & 1))
            throw std::string{ "tbsize must be odd when using tmode=0" };

        if (tmode == 0)
            d->tosize = 0;

        if (d->tosize < 0 || d->tosize >= d->tbsize)
            throw std::string{ "tosize must be between 0 and tbsize-1 (inclusive)" };

        if (d->tosize > d->tbsize / 2 && d->tbsize % (d->tbsize - d->tosize) != 0)
            throw std::string{ "temporal overlap greater than 50% requires that tbsize-tosize is a divisor of tbsize" };

        if (d->tbsize > d->vi->numFrames)
            throw std::string{ "tbsize must be less than or equal to the number of frames in the clip" };

        if (d->swin < 0 || d->swin > 11)
            throw std::string{ "swin must be between 0 and 11 (inclusive)" };

        if (d->twin < 0 || d->twin > 11)
            throw std::string{ "twin must be between 0 and 11 (inclusive)" };

        if (opt < 0 || opt > 3)
            throw std::string{ "opt must be 0, 1, 2 or 3" };

        const unsigned numThreads = vsapi->getCoreInfo(core)->numThreads;
        d->ebuff.reserve(numThreads);
        d->dftr.reserve(numThreads);
        d->dftc.reserve(numThreads);
        d->dftc2.reserve(numThreads);

        selectFunctions(ftype, opt, d.get());

        if (d->vi->format->sampleType == stInteger) {
            d->multiplier = static_cast<float>(1 << (d->vi->format->bitsPerSample - 8));
            d->divisor = 1.f / d->multiplier;
            d->peak = (1 << d->vi->format->bitsPerSample) - 1;
        }

        if (ftype != 0)
            d->f0beta = 1.f;

        d->barea = d->sbsize * d->sbsize;
        d->bvolume = d->barea * d->tbsize;
        d->ccnt = (d->sbsize / 2 + 1) * d->sbsize * d->tbsize;
        d->ccnt2 = d->ccnt * 2;
        d->type = tmode * 4 + (d->tbsize > 1 ? 2 : 0) + smode;
        d->sbd1 = d->sbsize / 2;
        d->uf0b = (std::abs(d->f0beta - 1.f) < 0.00005f) ? false : true;
        d->inc = (d->type & 1) ? d->sbsize - d->sosize : 1;

        d->padFormat = vsapi->registerFormat(cmGray, d->vi->format->sampleType, d->vi->format->bitsPerSample, 0, 0, core);
        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            const int width = d->vi->width >> (plane ? d->vi->format->subSamplingW : 0);
            const int height = d->vi->height >> (plane ? d->vi->format->subSamplingH : 0);

            if (smode == 0) {
                const int ae = (d->sbsize >> 1) << 1;
                d->padWidth[plane] = width + ae;
                d->padHeight[plane] = height + ae;
                d->eheight[plane] = height;
            } else {
                const int ae = std::max(d->sbsize - d->sosize, d->sosize) * 2;
                d->padWidth[plane] = width + EXTRA(width, d->sbsize) + ae;
                d->padHeight[plane] = height + EXTRA(height, d->sbsize) + ae;
                d->eheight[plane] = (d->padHeight[plane] - d->sosize) / (d->sbsize - d->sosize) * (d->sbsize - d->sosize);
            }
        }

        d->hw = vs_aligned_malloc<float>((d->bvolume + 7) * sizeof(float), 32);
        if (!d->hw)
            throw std::string{ "malloc failure (hw)" };
        createWindow(d->hw, tmode, smode, d.get());

        float * dftgr = vs_aligned_malloc<float>((d->bvolume + 7) * sizeof(float), 32);
        d->dftgc = vs_aligned_malloc<fftwf_complex>((d->ccnt + 7) * sizeof(fftwf_complex), 32);
        if (!dftgr || !d->dftgc)
            throw std::string{ "malloc failure (dftgr/dftgc)" };

        if (d->tbsize > 1) {
            d->ft = fftwf_plan_dft_r2c_3d(d->tbsize, d->sbsize, d->sbsize, dftgr, d->dftgc, FFTW_PATIENT | FFTW_DESTROY_INPUT);
            d->fti = fftwf_plan_dft_c2r_3d(d->tbsize, d->sbsize, d->sbsize, d->dftgc, dftgr, FFTW_PATIENT | FFTW_DESTROY_INPUT);
        } else {
            d->ft = fftwf_plan_dft_r2c_2d(d->sbsize, d->sbsize, dftgr, d->dftgc, FFTW_PATIENT | FFTW_DESTROY_INPUT);
            d->fti = fftwf_plan_dft_c2r_2d(d->sbsize, d->sbsize, d->dftgc, dftgr, FFTW_PATIENT | FFTW_DESTROY_INPUT);
        }

        float wscale = 0.f;

        const float * hwT = d->hw;
        float * VS_RESTRICT dftgrT = dftgr;
        for (int s = 0; s < d->tbsize; s++) {
            for (int i = 0; i < d->sbsize; i++) {
                for (int k = 0; k < d->sbsize; k++) {
                    dftgrT[k] = 255.f * hwT[k];
                    wscale += hwT[k] * hwT[k];
                }
                hwT += d->sbsize;
                dftgrT += d->sbsize;
            }
        }
        fftwf_execute_dft_r2c(d->ft, dftgr, d->dftgc);
        vs_aligned_free(dftgr);

        wscale = 1.f / wscale;
        const float wscalef = (ftype < 2) ? wscale : 1.f;

        d->sigmas = vs_aligned_malloc<float>((d->ccnt2 + 7) * sizeof(float), 32);
        d->sigmas2 = vs_aligned_malloc<float>((d->ccnt2 + 7) * sizeof(float), 32);
        d->pmins = vs_aligned_malloc<float>((d->ccnt2 + 7) * sizeof(float), 32);
        d->pmaxs = vs_aligned_malloc<float>((d->ccnt2 + 7) * sizeof(float), 32);
        if (!d->sigmas || !d->sigmas2 || !d->pmins || !d->pmaxs)
            throw std::string{ "malloc failure (sigmas/sigmas2/pmins/pmaxs)" };

        if (sstring[0] || ssx[0] || ssy[0] || sst[0]) {
            int ndim = 3;
            if (d->tbsize == 1)
                ndim -= 1;
            if (d->sbsize == 1)
                ndim -= 2;

            const float ndiv = 1.f / ndim;
            int tcnt = 0, sycnt = 0, sxcnt = 0;
            float * tdata, * sydata, * sxdata;
            bool edis = false;

            if (sstring[0]) {
                const char * w = sstring;
                if (sstring[0] == '$') { // FFT3DFilter method
                    edis = true;
                    while ((w[0] == '$' || w[0] == ' ') && w[0] != 0)
                        w++;
                }

                tdata = parseString(w, tcnt, sigma, edis ? 1.f : ndiv);
                sydata = parseString(w, sycnt, sigma, edis ? 1.f : ndiv);
                sxdata = parseString(w, sxcnt, sigma, edis ? 1.f : ndiv);
            } else {
                tdata = parseString(sst, tcnt, sigma, ndiv);
                sydata = parseString(ssy, sycnt, sigma, ndiv);
                sxdata = parseString(ssx, sxcnt, sigma, ndiv);
            }

            const int cpx = d->sbsize / 2 + 1;
            float pft, pfy, pfx;

            for (int z = 0; z < d->tbsize; z++) {
                const float tval = getSVal(z, d->tbsize, tdata, tcnt, pft);

                for (int y = 0; y < d->sbsize; y++) {
                    const float syval = getSVal(y, d->sbsize, sydata, sycnt, pfy);

                    for (int x = 0; x < cpx; x++) {
                        const float sxval = getSVal(x, d->sbsize, sxdata, sxcnt, pfx);
                        float val;

                        if (edis) {
                            const float dw = std::sqrt((pft * pft + pfy * pfy + pfx * pfx) / ndim);
                            val = interp(dw, tdata, tcnt);
                        } else {
                            val = tval * syval * sxval;
                        }

                        const int pos = ((z * d->sbsize + y) * cpx + x) * 2;
                        d->sigmas[pos] = d->sigmas[pos + 1] = val / wscalef;
                    }
                }
            }

            delete[] tdata;
            delete[] sydata;
            delete[] sxdata;
        } else {
            for (int i = 0; i < d->ccnt2; i++)
                d->sigmas[i] = sigma / wscalef;
        }

        for (int i = 0; i < d->ccnt2; i++) {
            d->sigmas2[i] = sigma2 / wscalef;
            d->pmins[i] = pmin / wscale;
            d->pmaxs[i] = pmax / wscale;
        }

        if (nstring[0] && ftype < 2) {
            memset(d->sigmas, 0, d->ccnt2 * sizeof(float));

            float * VS_RESTRICT hw2 = vs_aligned_malloc<float>((d->bvolume + 7) * sizeof(float), 32);
            if (!hw2)
                throw std::string{ "malloc failure (hw2)" };
            createWindow(hw2, 0, 0, d.get());

            float * VS_RESTRICT dftr = vs_aligned_malloc<float>((d->bvolume + 7) * sizeof(float), 32);
            fftwf_complex * dftgc2 = vs_aligned_malloc<fftwf_complex>((d->ccnt + 7) * sizeof(fftwf_complex), 32);
            if (!dftr || !dftgc2)
                throw std::string{ "malloc failure (dftr/dftgc2)" };

            float wscale2 = 0.f;
            int w = 0;
            for (int s = 0; s < d->tbsize; s++) {
                for (int i = 0; i < d->sbsize; i++) {
                    for (int k = 0; k < d->sbsize; k++, w++) {
                        dftr[w] = 255.f * hw2[w];
                        wscale2 += hw2[w] * hw2[w];
                    }
                }
            }
            wscale2 = 1.f / wscale2;
            fftwf_execute_dft_r2c(d->ft, dftr, dftgc2);

            float alpha = (ftype == 0) ? 5.f : 7.f;
            int nnpoints = 0;
            NPInfo * npts = new NPInfo[500];
            const char * q = nstring;

            if (q[0] == 'a' || q[0] == 'A') {
                float alphat;

                if (std::sscanf(q, "%*c:%f", &alphat) != 1)
                    throw std::string{ "error reading alpha value from nstring" };

                if (alphat <= 0.f)
                    throw std::string{ "nstring - invalid alpha factor" };

                alpha = alphat;

                while (q[0] != ' ' && q[0] != 0)
                    q++;
            }

            while (true) {
                while ((q[0] < '0' || q[0] > '9') && q[0] != 0)
                    q++;

                int fn, b, y, x;

                if (q[0] == 0 || std::sscanf(q, "%d,%d,%d,%d", &fn, &b, &y, &x) != 4)
                    break;

                if (fn < 0 || fn > d->vi->numFrames - d->tbsize)
                    throw std::string{ "invalid frame number in nstring (" } + std::to_string(fn) + ")";

                if (b < 0 || b >= d->vi->format->numPlanes)
                    throw std::string{ "invalid plane number in nstring (" } + std::to_string(b) + ")";

                const int height = d->vi->height >> (b ? d->vi->format->subSamplingH : 0);
                if (y < 0 || y > height - d->sbsize)
                    throw std::string{ "invalid y pos in nstring (" } + std::to_string(y) + ")";

                const int width = d->vi->width >> (b ? d->vi->format->subSamplingW : 0);
                if (x < 0 || x > width - d->sbsize)
                    throw std::string{ "invalid x pos in nstring (" } + std::to_string(x) + ")";

                if (nnpoints >= 300)
                    throw std::string{ "maximum number of entries in nstring is 500" };

                npts[nnpoints].fn = fn;
                npts[nnpoints].b = b;
                npts[nnpoints].y = y;
                npts[nnpoints].x = x;
                nnpoints++;

                while (q[0] != ' ' && q[0] != 0)
                    q++;
            }

            for (int ct = 0; ct < nnpoints; ct++) {
                fftwf_complex * dftc = vs_aligned_malloc<fftwf_complex>((d->ccnt + 7) * sizeof(fftwf_complex), 32);
                fftwf_complex * dftc2 = vs_aligned_malloc<fftwf_complex>((d->ccnt + 7) * sizeof(fftwf_complex), 32);
                if (!dftc || !dftc2)
                    throw std::string{ "malloc failure (dftc/dftc2)" };

                for (int z = 0; z < d->tbsize; z++) {
                    const VSFrameRef * src = vsapi->getFrame(npts[ct].fn + z, d->node, nullptr, 0);
                    const int stride = vsapi->getStride(src, npts[ct].b) / d->vi->format->bytesPerSample;

                    if (d->vi->format->bytesPerSample == 1) {
                        const uint8_t * srcp = vsapi->getReadPtr(src, npts[ct].b) + stride * npts[ct].y + npts[ct].x;
                        proc0(srcp, hw2 + d->barea * z, dftr + d->barea * z, stride, d->sbsize, d->divisor);
                    } else if (d->vi->format->bytesPerSample == 2) {
                        const uint16_t * srcp = reinterpret_cast<const uint16_t *>(vsapi->getReadPtr(src, npts[ct].b)) + stride * npts[ct].y + npts[ct].x;
                        proc0(srcp, hw2 + d->barea * z, dftr + d->barea * z, stride, d->sbsize, d->divisor);
                    } else {
                        const float * srcp = reinterpret_cast<const float *>(vsapi->getReadPtr(src, npts[ct].b)) + stride * npts[ct].y + npts[ct].x;
                        proc0(srcp, hw2 + d->barea * z, dftr + d->barea * z, stride, d->sbsize, d->divisor);
                    }

                    vsapi->freeFrame(src);
                }

                fftwf_execute_dft_r2c(d->ft, dftr, dftc);

                if (d->zmean)
                    removeMean(reinterpret_cast<float *>(dftc), reinterpret_cast<const float *>(dftgc2), d->ccnt2, reinterpret_cast<float *>(dftc2));

                for (int h = 0; h < d->ccnt2; h += 2) {
                    const float psd = reinterpret_cast<float *>(dftc)[h] * reinterpret_cast<float *>(dftc)[h] + reinterpret_cast<float *>(dftc)[h + 1] * reinterpret_cast<float *>(dftc)[h + 1];
                    d->sigmas[h] += psd;
                    d->sigmas[h + 1] += psd;
                }

                vs_aligned_free(dftc);
                vs_aligned_free(dftc2);
            }

            vs_aligned_free(hw2);
            vs_aligned_free(dftr);
            vs_aligned_free(dftgc2);
            delete[] npts;

            if (nnpoints != 0) {
                const float scale = 1.f / nnpoints;
                for (int h = 0; h < d->ccnt2; h++)
                    d->sigmas[h] *= scale * (wscale2 / wscale) * alpha;
            } else {
                throw std::string{ "no noise blocks in nstring" };
            }
        }
    } catch (const std::string & error) {
        vsapi->setError(out, ("DFTTest: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "DFTTest", dfttestInit, dfttestGetFrame, dfttestFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.dfttest", "dfttest", "2D/3D frequency domain denoiser", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("DFTTest",
                 "clip:clip;"
                 "ftype:int:opt;"
                 "sigma:float:opt;"
                 "sigma2:float:opt;"
                 "pmin:float:opt;"
                 "pmax:float:opt;"
                 "sbsize:int:opt;"
                 "smode:int:opt;"
                 "sosize:int:opt;"
                 "tbsize:int:opt;"
                 "tmode:int:opt;"
                 "tosize:int:opt;"
                 "swin:int:opt;"
                 "twin:int:opt;"
                 "sbeta:float:opt;"
                 "tbeta:float:opt;"
                 "zmean:int:opt;"
                 "f0beta:float:opt;"
                 "nstring:data:opt;"
                 "sstring:data:opt;"
                 "ssx:data:opt;"
                 "ssy:data:opt;"
                 "sst:data:opt;"
                 "planes:int[]:opt;"
                 "opt:int:opt;",
                 dfttestCreate, nullptr, plugin);
}
