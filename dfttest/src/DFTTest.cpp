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
#include <string>
#include <fftw3.h>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>
#ifdef VS_TARGET_CPU_X86
#include "vectorclass/vectormath_exp.h"
#endif

#define EXTRA(a,b) (((a) % (b)) ? ((b) - ((a) % (b))) : 0)

struct DFTTestData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    int ftype, sbsize, smode, sosize, tbsize, tmode, tosize, swin, twin;
    float sigma, sigma2, pmin, pmax, sbeta, tbeta, f0beta;
    bool zmean, process[3];
    const char * nstring, * sstring, * ssx, * ssy, * sst;
    int barea, bvolume, ccnt, type;
    const VSFormat * padFormat;
    int padWidth[3], padHeight[3], eheight[3];
    float * hw, * sigmas, * sigmas2, * pmins, * pmaxs;
    fftwf_complex * dftgc;
    fftwf_plan ft, fti;
    void (*proc0)(const uint8_t * s0, const float * s1, float * d, const int p0, const int p1, const int bitsPerSample);
    void (*filterCoeffs)(float * dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2);
    void (*intcast)(const float * ebp, uint8_t * dstp, const int dstWidth, const int dstHeight, const int dstStride, const int width, const int bitsPerSample);
};

struct NPInfo {
    int fn, b, y, x;
};

static double besselI0(double p) {
    p /= 2.;
    double n = 1., t = 1., d = 1.;
    int k = 1;
    double v;
    do {
        n *= p;
        d *= k;
        v = n / d;
        t += v * v;
    } while (++k < 15 && v > 1.0e-8);
    return t;
}

static double getWinValue(const double n, const double size, const int win, const double beta) {
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
        return 2. / size * (size / 2. - std::fabs(n - size / 2.));
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

static void normalizeForOverlapAdd(double * VS_RESTRICT hw, const int bsize, const int osize) {
    double * nw = reinterpret_cast<double *>(calloc(bsize, sizeof(double)));
    const int inc = bsize - osize;
    for (int q = 0; q < bsize; q++) {
        for (int h = q; h >= 0; h -= inc)
            nw[q] += hw[h] * hw[h];
        for (int h = q + inc; h < bsize; h += inc)
            nw[q] += hw[h] * hw[h];
    }
    for (int q = 0; q < bsize; q++)
        hw[q] /= std::sqrt(nw[q]);
    free(nw);
}

static void createWindow(float * VS_RESTRICT hw, const int tmode, const int smode, const DFTTestData * d) {
    double * tw = new double[d->tbsize];
    for (int j = 0; j < d->tbsize; j++)
        tw[j] = getWinValue(j + 0.5, d->tbsize, d->twin, d->tbeta);
    if (tmode == 1)
        normalizeForOverlapAdd(tw, d->tbsize, d->tosize);
    double * sw = new double[d->sbsize];
    for (int j = 0; j < d->sbsize; j++)
        sw[j] = getWinValue(j + 0.5, d->sbsize, d->swin, d->sbeta);
    if (smode == 1)
        normalizeForOverlapAdd(sw, d->sbsize, d->sosize);
    const double nscale = 1. / std::sqrt(d->tbsize * d->sbsize * d->sbsize);
    for (int j = 0; j < d->tbsize; j++)
        for (int k = 0; k < d->sbsize; k++)
            for (int q = 0; q < d->sbsize; q++)
                hw[(j * d->sbsize + k) * d->sbsize + q] = static_cast<float>(tw[j] * sw[k] * sw[q] * nscale);
    delete[] tw;
    delete[] sw;
}

static float * parseString(const char * s, int & poscnt, const float sigma, const float pfact, VSMap * out, const VSAPI * vsapi) {
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
            if (std::sscanf(sT, "%f:%f", &pos, &sval) != 2) {
                vsapi->setError(out, "DFTTest: invalid entry in sigma string");
                return parray;
            }
            if (pos < 0.f || pos > 1.f) {
                vsapi->setError(out, ("DFTTest: sigma string - invalid pos (" + std::to_string(pos) + ")").c_str());
                return parray;
            }
            if (pos == 0.f)
                found[0] = true;
            else if (pos == 1.f)
                found[1] = true;
            poscnt++;
            while (sT[1] != 0 && sT[1] != ' ')
                sT++;
            sT++;
        }
        if (!found[0] || !found[1]) {
            vsapi->setError(out, "DFTTest: sigma string - one or more end points not provided");
            return parray;
        }
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

static float interp(const float pf, const float * VS_RESTRICT pv, const int cnt) {
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

static float getSVal(const int pos, const int len, const float * VS_RESTRICT pv, const int cnt, float & pf) {
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

#ifdef VS_TARGET_CPU_X86
static void proc0_8(const uint8_t * _s0, const float * _s1, float * VS_RESTRICT d, const int p0, const int p1, const int bitsPerSample) {
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v += 8) {
            const Vec16uc s016uc = Vec16uc().load(_s0 + v);
            const Vec8s s08s = Vec8s(extend_low(s016uc));
            const Vec8i s08i = Vec8i(extend_low(s08s), extend_high(s08s));
            Vec8f s0 = to_float(s08i);
            const Vec8f s1 = Vec8f().load(_s1 + v);
            s0 *= s1;
            if (p1 - v >= 8)
                s0.store(d + v);
            else
                s0.store_partial(p1 - v, d + v);
        }
        _s0 += p0;
        _s1 += p1;
        d += p1;
    }
}

static void proc0_16(const uint8_t * _s0_, const float * _s1, float * VS_RESTRICT d, const int p0, const int p1, const int bitsPerSample) {
    const uint16_t * _s0 = reinterpret_cast<const uint16_t *>(_s0_);
    const Vec8f divisor(1.f / (1 << (bitsPerSample - 8)));
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v += 8) {
            const Vec8us s08us = Vec8us().load(_s0 + v);
            const Vec8i s08i = Vec8i(extend_low(s08us), extend_high(s08us));
            Vec8f s0 = to_float(s08i);
            const Vec8f s1 = Vec8f().load(_s1 + v);
            s0 *= s1 * divisor;
            if (p1 - v >= 8)
                s0.store(d + v);
            else
                s0.store_partial(p1 - v, d + v);
        }
        _s0 += p0;
        _s1 += p1;
        d += p1;
    }
}

static void proc1(const float * _s0, const float * _s1, float * VS_RESTRICT _d, const int p0, const int p1) {
    for (int u = 0; u < p0; u++) {
        for (int v = 0; v < p0; v += 8) {
            Vec8f s0 = Vec8f().load(_s0 + v);
            const Vec8f s1 = Vec8f().load(_s1 + v);
            const Vec8f d = Vec8f().load(_d + v);
            s0 = mul_add(s0, s1, d);
            if (p0 - v >= 8)
                s0.store(_d + v);
            else
                s0.store_partial(p0 - v, _d + v);
        }
        _s0 += p0;
        _s1 += p0;
        _d += p1;
    }
}

static void removeMean(float * VS_RESTRICT _dftc, const float * _dftgc, const int ccnt, float * VS_RESTRICT _dftc2) {
    const Vec8f gf(_dftc[0] / _dftgc[0]);
    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftgc = Vec8f().load_a(_dftgc + h);
        const Vec8f dftc2 = gf * dftgc;
        Vec8f dftc = Vec8f().load_a(_dftc + h);
        dftc -= dftc2;
        if (ccnt - h >= 8) {
            dftc2.store_a(_dftc2 + h);
            dftc.store_a(_dftc + h);
        } else {
            dftc2.store_partial(ccnt - h, _dftc2 + h);
            dftc.store_partial(ccnt - h, _dftc + h);
        }
    }
}

static void addMean(float * VS_RESTRICT _dftc, const int ccnt, const float * _dftc2) {
    for (int h = 0; h < ccnt; h += 8) {
        Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f dftc2 = Vec8f().load_a(_dftc2 + h);
        dftc += dftc2;
        if (ccnt - h >= 8)
            dftc.store_a(_dftc + h);
        else
            dftc.store_partial(ccnt - h, _dftc + h);
    }
}

static void filter_0(float * VS_RESTRICT _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    const Vec4f epsilon(1e-15f);
    const Vec4f zero(0.f);
    for (int h = 0; h < ccnt; h += 8) {
        Vec4f dftcLow = Vec4f().load_a(_dftc + h);
        Vec4f dftcHigh = Vec4f().load_a(_dftc + h + 4);
        Vec4f real = blend4f<0, 2, 4, 6>(dftcLow, dftcHigh);
        Vec4f imag = blend4f<1, 3, 5, 7>(dftcLow, dftcHigh);
        const Vec4f psd = mul_add(real, real, imag * imag);
        const Vec4f sigmasLow = Vec4f().load_a(_sigmas + h);
        const Vec4f sigmasHigh = Vec4f().load_a(_sigmas + h + 4);
        const Vec4f sigmas = blend4f<0, 2, 4, 6>(sigmasLow, sigmasHigh);
        const Vec4f coeff = max((psd - sigmas) * approx_recipr(psd + epsilon), zero);
        real *= coeff;
        imag *= coeff;
        dftcLow = blend4f<0, 4, 1, 5>(real, imag);
        dftcHigh = blend4f<2, 6, 3, 7>(real, imag);
        const Vec8f dftc(dftcLow, dftcHigh);
        if (ccnt - h >= 8)
            dftc.store_a(_dftc + h);
        else
            dftc.store_partial(ccnt - h, _dftc + h);
    }
}

static void filter_1(float * VS_RESTRICT _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    const Vec4f zero(0.f);
    for (int h = 0; h < ccnt; h += 8) {
        Vec4f dftcLow = Vec4f().load_a(_dftc + h);
        Vec4f dftcHigh = Vec4f().load_a(_dftc + h + 4);
        Vec4f real = blend4f<0, 2, 4, 6>(dftcLow, dftcHigh);
        Vec4f imag = blend4f<1, 3, 5, 7>(dftcLow, dftcHigh);
        const Vec4f psd = mul_add(real, real, imag * imag);
        const Vec4f sigmasLow = Vec4f().load_a(_sigmas + h);
        const Vec4f sigmasHigh = Vec4f().load_a(_sigmas + h + 4);
        const Vec4f sigmas = blend4f<0, 2, 4, 6>(sigmasLow, sigmasHigh);
        real = select(psd < sigmas, zero, real);
        imag = select(psd < sigmas, zero, imag);
        dftcLow = blend4f<0, 4, 1, 5>(real, imag);
        dftcHigh = blend4f<2, 6, 3, 7>(real, imag);
        const Vec8f dftc(dftcLow, dftcHigh);
        if (ccnt - h >= 8)
            dftc.store_a(_dftc + h);
        else
            dftc.store_partial(ccnt - h, _dftc + h);
    }
}

static void filter_2(float * VS_RESTRICT _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    for (int h = 0; h < ccnt; h += 8) {
        Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f sigmas = Vec8f().load_a(_sigmas + h);
        dftc *= sigmas;
        if (ccnt - h >= 8)
            dftc.store_a(_dftc + h);
        else
            dftc.store_partial(ccnt - h, _dftc + h);
    }
}

static void filter_3(float * VS_RESTRICT _dftc, const float * _sigmas, const int ccnt, const float * _pmin, const float * _pmax, const float * _sigmas2) {
    for (int h = 0; h < ccnt; h += 8) {
        Vec4f dftcLow = Vec4f().load_a(_dftc + h);
        Vec4f dftcHigh = Vec4f().load_a(_dftc + h + 4);
        Vec4f real = blend4f<0, 2, 4, 6>(dftcLow, dftcHigh);
        Vec4f imag = blend4f<1, 3, 5, 7>(dftcLow, dftcHigh);
        const Vec4f psd = mul_add(real, real, imag * imag);
        const Vec4f pminLow = Vec4f().load(_pmin + h);
        const Vec4f pminHigh = Vec4f().load(_pmin + h + 4);
        const Vec4f pmin = blend4f<0, 2, 4, 6>(pminLow, pminHigh);
        const Vec4f pmaxLow = Vec4f().load_a(_pmax + h);
        const Vec4f pmaxHigh = Vec4f().load_a(_pmax + h + 4);
        const Vec4f pmax = blend4f<0, 2, 4, 6>(pmaxLow, pmaxHigh);
        const Vec4f sigmasLow = Vec4f().load_a(_sigmas + h);
        const Vec4f sigmasHigh = Vec4f().load_a(_sigmas + h + 4);
        const Vec4f sigmas = blend4f<0, 2, 4, 6>(sigmasLow, sigmasHigh);
        const Vec4f sigmas2Low = Vec4f().load_a(_sigmas2 + h);
        const Vec4f sigmas2High = Vec4f().load_a(_sigmas2 + h + 4);
        const Vec4f sigmas2 = blend4f<0, 2, 4, 6>(sigmas2Low, sigmas2High);
        real = select(psd >= pmin & psd <= pmax, real * sigmas, real * sigmas2);
        imag = select(psd >= pmin & psd <= pmax, imag * sigmas, imag * sigmas2);
        dftcLow = blend4f<0, 4, 1, 5>(real, imag);
        dftcHigh = blend4f<2, 6, 3, 7>(real, imag);
        const Vec8f dftc(dftcLow, dftcHigh);
        if (ccnt - h >= 8)
            dftc.store_a(_dftc + h);
        else
            dftc.store_partial(ccnt - h, _dftc + h);
    }
}

static void filter_4(float * VS_RESTRICT _dftc, const float * _sigmas, const int ccnt, const float * _pmin, const float * _pmax, const float * sigmas2) {
    const Vec4f epsilon(1e-15f);
    for (int h = 0; h < ccnt; h += 8) {
        Vec4f dftcLow = Vec4f().load_a(_dftc + h);
        Vec4f dftcHigh = Vec4f().load_a(_dftc + h + 4);
        Vec4f real = blend4f<0, 2, 4, 6>(dftcLow, dftcHigh);
        Vec4f imag = blend4f<1, 3, 5, 7>(dftcLow, dftcHigh);
        const Vec4f psd = mul_add(real, real, mul_add(imag, imag, epsilon));
        const Vec4f sigmasLow = Vec4f().load_a(_sigmas + h);
        const Vec4f sigmasHigh = Vec4f().load_a(_sigmas + h + 4);
        const Vec4f sigmas = blend4f<0, 2, 4, 6>(sigmasLow, sigmasHigh);
        const Vec4f pminLow = Vec4f().load(_pmin + h);
        const Vec4f pminHigh = Vec4f().load(_pmin + h + 4);
        const Vec4f pmin = blend4f<0, 2, 4, 6>(pminLow, pminHigh);
        const Vec4f pmaxLow = Vec4f().load_a(_pmax + h);
        const Vec4f pmaxHigh = Vec4f().load_a(_pmax + h + 4);
        const Vec4f pmax = blend4f<0, 2, 4, 6>(pmaxLow, pmaxHigh);
        const Vec4f mult = sigmas * sqrt(psd * pmax * approx_recipr((psd + pmin) * (psd + pmax)));
        real *= mult;
        imag *= mult;
        dftcLow = blend4f<0, 4, 1, 5>(real, imag);
        dftcHigh = blend4f<2, 6, 3, 7>(real, imag);
        const Vec8f dftc(dftcLow, dftcHigh);
        if (ccnt - h >= 8)
            dftc.store_a(_dftc + h);
        else
            dftc.store_partial(ccnt - h, _dftc + h);
    }
}

static void filter_5(float * VS_RESTRICT _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    const Vec4f epsilon(1e-15f);
    const Vec4f zero(0.f);
    for (int h = 0; h < ccnt; h += 8) {
        Vec4f dftcLow = Vec4f().load_a(_dftc + h);
        Vec4f dftcHigh = Vec4f().load_a(_dftc + h + 4);
        Vec4f real = blend4f<0, 2, 4, 6>(dftcLow, dftcHigh);
        Vec4f imag = blend4f<1, 3, 5, 7>(dftcLow, dftcHigh);
        const Vec4f psd = mul_add(real, real, imag * imag);
        const Vec4f sigmasLow = Vec4f().load_a(_sigmas + h);
        const Vec4f sigmasHigh = Vec4f().load_a(_sigmas + h + 4);
        const Vec4f sigmas = blend4f<0, 2, 4, 6>(sigmasLow, sigmasHigh);
        const Vec4f coeff = pow(max((psd - sigmas) * approx_recipr(psd + epsilon), zero), pmin[0]);
        real *= coeff;
        imag *= coeff;
        dftcLow = blend4f<0, 4, 1, 5>(real, imag);
        dftcHigh = blend4f<2, 6, 3, 7>(real, imag);
        const Vec8f dftc(dftcLow, dftcHigh);
        if (ccnt - h >= 8)
            dftc.store_a(_dftc + h);
        else
            dftc.store_partial(ccnt - h, _dftc + h);
    }
}

static void filter_6(float * VS_RESTRICT _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    const Vec4f epsilon(1e-15f);
    const Vec4f zero(0.f);
    for (int h = 0; h < ccnt; h += 8) {
        Vec4f dftcLow = Vec4f().load_a(_dftc + h);
        Vec4f dftcHigh = Vec4f().load_a(_dftc + h + 4);
        Vec4f real = blend4f<0, 2, 4, 6>(dftcLow, dftcHigh);
        Vec4f imag = blend4f<1, 3, 5, 7>(dftcLow, dftcHigh);
        const Vec4f psd = mul_add(real, real, imag * imag);
        const Vec4f sigmasLow = Vec4f().load_a(_sigmas + h);
        const Vec4f sigmasHigh = Vec4f().load_a(_sigmas + h + 4);
        const Vec4f sigmas = blend4f<0, 2, 4, 6>(sigmasLow, sigmasHigh);
        const Vec4f coeff = sqrt(max((psd - sigmas) * approx_recipr(psd + epsilon), zero));
        real *= coeff;
        imag *= coeff;
        dftcLow = blend4f<0, 4, 1, 5>(real, imag);
        dftcHigh = blend4f<2, 6, 3, 7>(real, imag);
        const Vec8f dftc(dftcLow, dftcHigh);
        if (ccnt - h >= 8)
            dftc.store_a(_dftc + h);
        else
            dftc.store_partial(ccnt - h, _dftc + h);
    }
}

static void intcast_8(const float * _ebp, uint8_t * VS_RESTRICT dstp, const int dstWidth, const int dstHeight, const int dstStride, const int width, const int bitsPerSample) {
    const Vec8f pointFive(0.5f);
    const Vec16s zero(0);
    const Vec16s peak(255);
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x += 16) {
            Vec8f ebp8f = Vec8f().load(_ebp + x);
            const Vec8i ebp8iLow = truncate_to_int(ebp8f + pointFive);
            ebp8f = Vec8f().load(_ebp + x + 8);
            const Vec8i ebp8iHigh = truncate_to_int(ebp8f + pointFive);
            const Vec16s ebp16s = min(max(compress_saturated(ebp8iLow, ebp8iHigh), zero), peak);
            const Vec16uc ebp = Vec16uc(compress(ebp16s.get_low(), ebp16s.get_high()));
            ebp.store_a(dstp + x);
        }
        _ebp += width;
        dstp += dstStride;
    }
}

static void intcast_16(const float * _ebp, uint8_t * VS_RESTRICT _dstp, const int dstWidth, const int dstHeight, const int dstStride, const int width, const int bitsPerSample) {
    uint16_t * VS_RESTRICT dstp = reinterpret_cast<uint16_t *>(_dstp);
    const Vec8f multiplier(static_cast<float>(1 << (bitsPerSample - 8)));
    const Vec8f pointFive(0.5f);
    const Vec8i zero(0);
    const Vec8i peak((1 << bitsPerSample) - 1);
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x += 8) {
            const Vec8f ebp8f = Vec8f().load(_ebp + x);
            const Vec8i ebp8i = min(max(truncate_to_int(mul_add(ebp8f, multiplier, pointFive)), zero), peak);
            const Vec8us ebp = Vec8us(compress(ebp8i.get_low(), ebp8i.get_high()));
            ebp.store_a(dstp + x);
        }
        _ebp += width;
        dstp += dstStride;
    }
}
#else
static void proc0_8(const uint8_t * s0, const float * s1, float * VS_RESTRICT d, const int p0, const int p1, const int bitsPerSample) {
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v++)
            d[v] = s0[v] * s1[v];
        s0 += p0;
        s1 += p1;
        d += p1;
    }
}

static void proc0_16(const uint8_t * _s0, const float * s1, float * VS_RESTRICT d, const int p0, const int p1, const int bitsPerSample) {
    const uint16_t * s0 = reinterpret_cast<const uint16_t *>(_s0);
    const float divisor = 1.f / (1 << (bitsPerSample - 8));
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v++)
            d[v] = s0[v] * s1[v] * divisor;
        s0 += p0;
        s1 += p1;
        d += p1;
    }
}

static void proc1(const float * s0, const float * s1, float * VS_RESTRICT d, const int p0, const int p1) {
    for (int u = 0; u < p0; u++) {
        for (int v = 0; v < p0; v++)
            d[v] += s0[v] * s1[v];
        s0 += p0;
        s1 += p0;
        d += p1;
    }
}

static void removeMean(float * VS_RESTRICT dftc, const float * dftgc, const int ccnt, float * VS_RESTRICT dftc2) {
    const float gf = dftc[0] / dftgc[0];
    for (int h = 0; h < ccnt; h++) {
        dftc2[h] = gf * dftgc[h];
        dftc[h] -= dftc2[h];
    }
}

static void addMean(float * VS_RESTRICT dftc, const int ccnt, const float * dftc2) {
    for (int h = 0; h < ccnt; h++)
        dftc[h] += dftc2[h];
}

static void filter_0(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];
        const float coeff = std::max((psd - sigmas[h]) / (psd + 1e-15f), 0.f);
        dftc[h] *= coeff;
        dftc[h + 1] *= coeff;
    }
}

static void filter_1(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];
        if (psd < sigmas[h])
            dftc[h] = dftc[h + 1] = 0.f;
    }
}

static void filter_2(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    for (int h = 0; h < ccnt; h++)
        dftc[h] *= sigmas[h];
}

static void filter_3(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
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

static void filter_4(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1] + 1e-15f;
        const float mult = sigmas[h] * std::sqrt(psd * pmax[h] / ((psd + pmin[h]) * (psd + pmax[h])));
        dftc[h] *= mult;
        dftc[h + 1] *= mult;
    }
}

static void filter_5(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    const float beta = pmin[0];
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];
        const float coeff = std::pow(std::max((psd - sigmas[h]) / (psd + 1e-15f), 0.f), beta);
        dftc[h] *= coeff;
        dftc[h + 1] *= coeff;
    }
}

static void filter_6(float * VS_RESTRICT dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) {
    for (int h = 0; h < ccnt; h += 2) {
        const float psd = dftc[h] * dftc[h] + dftc[h + 1] * dftc[h + 1];
        const float coeff = std::sqrt(std::max((psd - sigmas[h]) / (psd + 1e-15f), 0.f));
        dftc[h] *= coeff;
        dftc[h + 1] *= coeff;
    }
}

static void intcast_8(const float * ebp, uint8_t * VS_RESTRICT dstp, const int dstWidth, const int dstHeight, const int dstStride, const int width, const int bitsPerSample) {
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x++)
            dstp[x] = std::min(std::max(static_cast<int>(ebp[x] + 0.5f), 0), 255);
        ebp += width;
        dstp += dstStride;
    }
}

static void intcast_16(const float * ebp, uint8_t * VS_RESTRICT _dstp, const int dstWidth, const int dstHeight, const int dstStride, const int width, const int bitsPerSample) {
    uint16_t * VS_RESTRICT dstp = reinterpret_cast<uint16_t *>(_dstp);
    const float multiplier = static_cast<float>(1 << (bitsPerSample - 8));
    const int peak = (1 << bitsPerSample) - 1;
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x++)
            dstp[x] = std::min(std::max(static_cast<int>(ebp[x] * multiplier + 0.5f), 0), peak);
        ebp += width;
        dstp += dstStride;
    }
}
#endif

template<typename T>
static void copyPad(const VSFrameRef * src, VSFrameRef * dst[3], const DFTTestData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int srcWidth = vsapi->getFrameWidth(src, plane);
            const int srcHeight = vsapi->getFrameHeight(src, plane);
            const int dstWidth = vsapi->getFrameWidth(dst[plane], 0);
            const int dstHeight = vsapi->getFrameHeight(dst[plane], 0);
            const int dstStride = vsapi->getStride(dst[plane], 0) / d->vi->format->bytesPerSample;
            const int offy = (dstHeight - srcHeight) / 2;
            const int offx = (dstWidth - srcWidth) / 2;
            vs_bitblt(vsapi->getWritePtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * offy + offx * d->vi->format->bytesPerSample, vsapi->getStride(dst[plane], 0),
                      vsapi->getReadPtr(src, plane), vsapi->getStride(src, plane), srcWidth * d->vi->format->bytesPerSample, srcHeight);
            T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst[plane], 0)) + dstStride * offy;
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
                vs_bitblt(vsapi->getWritePtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * y, vsapi->getStride(dst[plane], 0),
                          vsapi->getWritePtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * w, vsapi->getStride(dst[plane], 0), dstWidth * d->vi->format->bytesPerSample, 1);
            w = offy + srcHeight - 2;
            for (int y = offy + srcHeight; y < dstHeight; y++, w--)
                vs_bitblt(vsapi->getWritePtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * y, vsapi->getStride(dst[plane], 0),
                          vsapi->getWritePtr(dst[plane], 0) + vsapi->getStride(dst[plane], 0) * w, vsapi->getStride(dst[plane], 0), dstWidth * d->vi->format->bytesPerSample, 1);
        }
    }
}

template<typename T>
static void func_0(VSFrameRef * src[3], VSFrameRef * dst, float * ebuff[3], float * VS_RESTRICT dftr, fftwf_complex * VS_RESTRICT dftc, fftwf_complex * VS_RESTRICT dftc2,
                   const DFTTestData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = d->padWidth[plane];
            const int height = d->padHeight[plane];
            const int eheight = d->eheight[plane];
            const int stride = vsapi->getStride(src[plane], 0) / d->vi->format->bytesPerSample;
            const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src[plane], 0));
            float * ebpSaved = ebuff[plane];
            const int sbd1 = d->sbsize / 2;
            const int ccnt = d->ccnt * 2;
            const bool uf0b = std::fabs(d->f0beta - 1.f) < 0.00005f ? false : true;
            const int inc = (d->type & 1) ? d->sbsize - d->sosize : 1;
            for (int y = 0; y < eheight; y += inc) {
                for (int x = 0; x <= width - d->sbsize; x += inc) {
                    d->proc0(reinterpret_cast<const uint8_t *>(srcp + x), d->hw, dftr, stride, d->sbsize, d->vi->format->bitsPerSample);
                    fftwf_execute_dft_r2c(d->ft, dftr, dftc);
                    if (d->zmean)
                        removeMean(reinterpret_cast<float *>(dftc), reinterpret_cast<const float *>(d->dftgc), ccnt, reinterpret_cast<float *>(dftc2));
                    d->filterCoeffs(reinterpret_cast<float *>(dftc), d->sigmas, ccnt, uf0b ? &d->f0beta : d->pmins, d->pmaxs, d->sigmas2);
                    if (d->zmean)
                        addMean(reinterpret_cast<float *>(dftc), ccnt, reinterpret_cast<const float *>(dftc2));
                    fftwf_execute_dft_c2r(d->fti, dftc, dftr);
                    if (d->type & 1) // spatial overlapping
                        proc1(dftr, d->hw, ebpSaved + x, d->sbsize, width);
                    else
                        ebpSaved[x + sbd1 * width + sbd1] = dftr[sbd1 * d->sbsize + sbd1] * d->hw[sbd1 * d->sbsize + sbd1];
                }
                srcp += stride * inc;
                ebpSaved += width * inc;
            }
            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int dstStride = vsapi->getStride(dst, plane) / d->vi->format->bytesPerSample;
            uint8_t * dstp = vsapi->getWritePtr(dst, plane);
            float * ebp = ebuff[plane] + width * ((height - dstHeight) / 2) + (width - dstWidth) / 2;
            d->intcast(ebp, dstp, dstWidth, dstHeight, dstStride, width, d->vi->format->bitsPerSample);
        }
    }
}

template<typename T>
static void func_1(VSFrameRef * src[15][3], VSFrameRef * dst, float * ebuff[3], float * VS_RESTRICT dftr, fftwf_complex * VS_RESTRICT dftc, fftwf_complex * VS_RESTRICT dftc2,
                   const int pos, const DFTTestData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = d->padWidth[plane];
            const int height = d->padHeight[plane];
            const int eheight = d->eheight[plane];
            const int stride = vsapi->getStride(src[0][plane], 0) / d->vi->format->bytesPerSample;
            const T * srcp[15];
            for (int i = 0; i < d->tbsize; i++)
                srcp[i] = reinterpret_cast<const T *>(vsapi->getReadPtr(src[i][plane], 0));
            const int sbd1 = d->sbsize / 2;
            const int ccnt = d->ccnt * 2;
            const bool uf0b = std::fabs(d->f0beta - 1.f) < 0.00005f ? false : true;
            const int inc = (d->type & 1) ? d->sbsize - d->sosize : 1;
            for (int y = 0; y < eheight; y += inc) {
                for (int x = 0; x <= width - d->sbsize; x += inc) {
                    for (int z = 0; z < d->tbsize; z++)
                        d->proc0(reinterpret_cast<const uint8_t *>(srcp[z] + x), d->hw + d->sbsize * d->sbsize * z, dftr + d->sbsize * d->sbsize * z,
                                 stride, d->sbsize, d->vi->format->bitsPerSample);
                    fftwf_execute_dft_r2c(d->ft, dftr, dftc);
                    if (d->zmean)
                        removeMean(reinterpret_cast<float *>(dftc), reinterpret_cast<const float *>(d->dftgc), ccnt, reinterpret_cast<float *>(dftc2));
                    d->filterCoeffs(reinterpret_cast<float *>(dftc), d->sigmas, ccnt, uf0b ? &d->f0beta : d->pmins, d->pmaxs, d->sigmas2);
                    if (d->zmean)
                        addMean(reinterpret_cast<float *>(dftc), ccnt, reinterpret_cast<const float *>(dftc2));
                    fftwf_execute_dft_c2r(d->fti, dftc, dftr);
                    if (d->type & 1) { // spatial overlapping
                        // FIXME: tmode 1 is not implemented so just skip the if-else check
                        //if (d->type & 4) { // temporal overlapping
                        //    for (int z = 0; z < d->tbsize; z++)
                        //        proc1_C(dftr + z * d->barea, d->hw + z * d->barea, ebuff[z * 3 + plane] + y * width + x, d->sbsize, width);
                        //} else {
                            proc1(dftr + pos * d->barea, d->hw + pos * d->barea, ebuff[plane] + y * width + x, d->sbsize, width);
                        //}
                    } else {
                        // FIXME: tmode 1 is not implemented so just skip the if-else check
                        //if (d->type & 4) { // temporal overlapping
                        //    for (int z = 0; z < d->tbsize; z++)
                        //        ebuff[z * 3 + plane][(y + sbd1) * width + x + sbd1] += dftr[z * d->barea + sbd1 * d->sbsize + sbd1] * d->hw[z * d->barea + sbd1 * d->sbsize + sbd1];
                        //} else {
                            ebuff[plane][(y + sbd1) * width + x + sbd1] = dftr[pos * d->barea + sbd1 * d->sbsize + sbd1] * d->hw[pos * d->barea + sbd1 * d->sbsize + sbd1];
                        //}
                    }
                }
                for (int q = 0; q < d->tbsize; q++)
                    srcp[q] += stride * inc;
            }
            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int dstStride = vsapi->getStride(dst, plane) / d->vi->format->bytesPerSample;
            uint8_t * dstp = vsapi->getWritePtr(dst, plane);
            float * ebp = ebuff[plane] + width * ((height - dstHeight) / 2) + (width - dstWidth) / 2;
            d->intcast(ebp, dstp, dstWidth, dstHeight, dstStride, width, d->vi->format->bitsPerSample);
        }
    }
}

static void VS_CC dfttestInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DFTTestData * d = static_cast<DFTTestData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC dfttestGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const DFTTestData * d = static_cast<const DFTTestData *>(*instanceData);

    if (activationReason == arInitial) {
        if (d->tbsize == 1) {
            vsapi->requestFrameFilter(n, d->node, frameCtx);
        } else {
            const int first = std::max(n - d->tbsize / 2, 0);
            const int last = std::min(n + d->tbsize / 2, d->vi->numFrames - 1);
            for (int i = first; i <= last; i++)
                vsapi->requestFrameFilter(i, d->node, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
#ifdef VS_TARGET_CPU_X86
        no_subnormals();
#endif

        float * ebuff[3];
        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane]) {
                ebuff[plane] = vs_aligned_malloc<float>(d->padWidth[plane] * d->padHeight[plane] * sizeof(float), 32);
                if (!ebuff[plane]) {
                    vsapi->setFilterError("DFTTest: malloc failure (ebuff)", frameCtx);
                    return nullptr;
                }
                memset(ebuff[plane], 0, d->padWidth[plane] * d->padHeight[plane] * sizeof(float));
            }
        }

        float * dftr = vs_aligned_malloc<float>(d->bvolume * sizeof(float), 32);
        fftwf_complex * dftc = vs_aligned_malloc<fftwf_complex>((d->ccnt + 11) * sizeof(fftwf_complex), 32);
        fftwf_complex * dftc2 = vs_aligned_malloc<fftwf_complex>((d->ccnt + 11) * sizeof(fftwf_complex), 32);
        if (!dftr || !dftc || !dftc2) {
            vsapi->setFilterError("DFTTest: malloc failure (dftr/dftc/dftc2)", frameCtx);
            return nullptr;
        }

        const VSFrameRef * src0 = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src0, d->process[1] ? nullptr : src0, d->process[2] ? nullptr : src0 };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src0, core);

        if (d->tbsize == 1) {
            VSFrameRef * pad[3];
            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (d->process[plane])
                    pad[plane] = vsapi->newVideoFrame(d->padFormat, d->padWidth[plane], d->padHeight[plane], nullptr, core);
            }

            if (d->vi->format->bitsPerSample == 8) {
                copyPad<uint8_t>(src0, pad, d, vsapi);
                func_0<uint8_t>(pad, dst, ebuff, dftr, dftc, dftc2, d, vsapi);
            } else {
                copyPad<uint16_t>(src0, pad, d, vsapi);
                func_0<uint16_t>(pad, dst, ebuff, dftr, dftc, dftc2, d, vsapi);
            }

            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (d->process[plane])
                    vsapi->freeFrame(pad[plane]);
            }
        } else {
            const VSFrameRef * src[15];
            VSFrameRef * pad[15][3];

            const int pos = d->tbsize / 2;

            for (int i = n - pos; i <= n + pos; i++) {
                src[i - n + pos] = vsapi->getFrameFilter(std::min(std::max(i, 0), d->vi->numFrames - 1), d->node, frameCtx);
                for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                    if (d->process[plane])
                        pad[i - n + pos][plane] = vsapi->newVideoFrame(d->padFormat, d->padWidth[plane], d->padHeight[plane], nullptr, core);
                }

                if (d->vi->format->bitsPerSample == 8)
                    copyPad<uint8_t>(src[i - n + pos], pad[i - n + pos], d, vsapi);
                else
                    copyPad<uint16_t>(src[i - n + pos], pad[i - n + pos], d, vsapi);
            }

            if (d->vi->format->bitsPerSample == 8)
                func_1<uint8_t>(pad, dst, ebuff, dftr, dftc, dftc2, pos, d, vsapi);
            else
                func_1<uint16_t>(pad, dst, ebuff, dftr, dftc, dftc2, pos, d, vsapi);

            for (int i = n - pos; i <= n + pos; i++) {
                vsapi->freeFrame(src[i - n + pos]);
                for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                    if (d->process[plane])
                        vsapi->freeFrame(pad[i - n + pos][plane]);
                }
            }
        }

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane])
                vs_aligned_free(ebuff[plane]);
        }
        vs_aligned_free(dftr);
        vs_aligned_free(dftc);
        vs_aligned_free(dftc2);
        vsapi->freeFrame(src0);
        return dst;
    }

    return nullptr;
}

static void VS_CC dfttestFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DFTTestData * d = static_cast<DFTTestData *>(instanceData);
    vsapi->freeNode(d->node);
    vs_aligned_free(d->hw);
    vs_aligned_free(d->sigmas);
    vs_aligned_free(d->sigmas2);
    vs_aligned_free(d->pmins);
    vs_aligned_free(d->pmaxs);
    vs_aligned_free(d->dftgc);
    fftwf_destroy_plan(d->ft);
    fftwf_destroy_plan(d->fti);
    delete d;
}

static void VS_CC dfttestCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
#ifdef VS_TARGET_CPU_X86
    no_subnormals();
#endif

    DFTTestData d;
    int err;

    d.ftype = int64ToIntS(vsapi->propGetInt(in, "ftype", 0, &err));
    d.sigma = static_cast<float>(vsapi->propGetFloat(in, "sigma", 0, &err));
    if (err)
        d.sigma = 5.f;
    d.sigma2 = static_cast<float>(vsapi->propGetFloat(in, "sigma2", 0, &err));
    if (err)
        d.sigma2 = 5.f;
    d.pmin = static_cast<float>(vsapi->propGetFloat(in, "pmin", 0, &err));
    if (err)
        d.pmin = 0.f;
    d.pmax = static_cast<float>(vsapi->propGetFloat(in, "pmax", 0, &err));
    if (err)
        d.pmax = 500.f;
    d.sbsize = int64ToIntS(vsapi->propGetInt(in, "sbsize", 0, &err));
    if (err)
        d.sbsize = 12;
    d.smode = int64ToIntS(vsapi->propGetInt(in, "smode", 0, &err));
    if (err)
        d.smode = 1;
    d.sosize = int64ToIntS(vsapi->propGetInt(in, "sosize", 0, &err));
    if (err)
        d.sosize = 9;
    d.tbsize = int64ToIntS(vsapi->propGetInt(in, "tbsize", 0, &err));
    if (err)
        d.tbsize = 3;
    d.tmode = int64ToIntS(vsapi->propGetInt(in, "tmode", 0, &err));
    d.tosize = int64ToIntS(vsapi->propGetInt(in, "tosize", 0, &err));
    d.swin = int64ToIntS(vsapi->propGetInt(in, "swin", 0, &err));
    d.twin = int64ToIntS(vsapi->propGetInt(in, "twin", 0, &err));
    if (err)
        d.twin = 7;
    d.sbeta = static_cast<float>(vsapi->propGetFloat(in, "sbeta", 0, &err));
    if (err)
        d.sbeta = 2.5f;
    d.tbeta = static_cast<float>(vsapi->propGetFloat(in, "tbeta", 0, &err));
    if (err)
        d.tbeta = 2.5f;
    d.zmean = !!vsapi->propGetInt(in, "zmean", 0, &err);
    if (err)
        d.zmean = true;
    d.f0beta = static_cast<float>(vsapi->propGetFloat(in, "f0beta", 0, &err));
    if (err)
        d.f0beta = 1.f;
    d.nstring = vsapi->propGetData(in, "nstring", 0, &err);
    if (err)
        d.nstring = "";
    d.sstring = vsapi->propGetData(in, "sstring", 0, &err);
    if (err)
        d.sstring = "";
    d.ssx = vsapi->propGetData(in, "ssx", 0, &err);
    if (err)
        d.ssx = "";
    d.ssy = vsapi->propGetData(in, "ssy", 0, &err);
    if (err)
        d.ssy = "";
    d.sst = vsapi->propGetData(in, "sst", 0, &err);
    if (err)
        d.sst = "";

    if (d.ftype < 0 || d.ftype > 4) {
        vsapi->setError(out, "DFTTest: ftype must be set to 0, 1, 2, 3 or 4");
        return;
    }
    if (d.sbsize < 1) {
        vsapi->setError(out, "DFTTest: sbsize must be greater than or equal to 1");
        return;
    }
    if (d.smode < 0 || d.smode > 1) {
        vsapi->setError(out, "DFTTest: smode must be set to 0 or 1");
        return;
    }
    if (d.smode == 0 && !(d.sbsize & 1)) {
        vsapi->setError(out, "DFTTest: sbsize must be odd when using smode=0");
        return;
    }
    if (d.smode == 0)
        d.sosize = 0;
    if (d.sosize < 0 || d.sosize >= d.sbsize) {
        vsapi->setError(out, "DFTTest: sosize must be between 0 and sbsize-1 inclusive");
        return;
    }
    if (d.sosize > d.sbsize / 2 && d.sbsize % (d.sbsize - d.sosize) != 0) {
        vsapi->setError(out, "DFTTest: spatial overlap greater than 50% requires that sbsize-sosize be a divisor of sbsize");
        return;
    }
    if (d.tbsize < 1 || d.tbsize > 15) {
        vsapi->setError(out, "DFTTest: tbsize must be between 1 and 15 inclusive");
        return;
    }
    if (d.tmode != 0) {
        vsapi->setError(out, "DFTTest: tmode must be set to 0. tmode 1 is not implemented");
        return;
    }
    if (d.tmode == 0 && !(d.tbsize & 1)) {
        vsapi->setError(out, "DFTTest: tbsize must be odd when using tmode=0");
        return;
    }
    if (d.tmode == 0)
        d.tosize = 0;
    if (d.tosize < 0 || d.tosize >= d.tbsize) {
        vsapi->setError(out, "DFTTest: tosize must be between 0 and tbsize-1 inclusive");
        return;
    }
    if (d.tosize > d.tbsize / 2 && d.tbsize % (d.tbsize - d.tosize) != 0) {
        vsapi->setError(out, "DFTTest: temporal overlap greater than 50% requires that tbsize-tosize be a divisor of tbsize");
        return;
    }
    if (d.swin < 0 || d.swin > 11) {
        vsapi->setError(out, "DFTTest: swin must be between 0 and 11 inclusive");
        return;
    }
    if (d.twin < 0 || d.twin > 11) {
        vsapi->setError(out, "DFTTest: twin must be between 0 and 11 inclusive");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bitsPerSample > 16) {
        vsapi->setError(out, "DFTTest: only constant format 8-16 bits integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    if ((d.tbsize > 1 || d.nstring[0]) && !d.vi->numFrames) {
        vsapi->setError(out, "DFTTest: the clip's length must not be unknown when using tbsize>1 or nstring");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.tbsize > 1 && d.tbsize > d.vi->numFrames) {
        vsapi->setError(out, "DFTTest: tbsize must be less than or equal to the number of frames in the clip");
        vsapi->freeNode(d.node);
        return;
    }

    const int m = vsapi->propNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        d.process[i] = m <= 0;

    for (int i = 0; i < m; i++) {
        const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (n < 0 || n >= d.vi->format->numPlanes) {
            vsapi->setError(out, "DFTTest: plane index out of range");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.process[n]) {
            vsapi->setError(out, "DFTTest: plane specified twice");
            vsapi->freeNode(d.node);
            return;
        }

        d.process[n] = true;
    }

    d.barea = d.sbsize * d.sbsize;
    d.bvolume = d.barea * d.tbsize;
    d.ccnt = (d.sbsize / 2 + 1) * d.sbsize * d.tbsize;
    d.type = d.tmode * 4 + (d.tbsize > 1 ? 2 : 0) + d.smode;

    d.padFormat = vsapi->registerFormat(cmGray, stInteger, d.vi->format->bitsPerSample, 0, 0, core);
    for (int plane = 0; plane < d.vi->format->numPlanes; plane++) {
        if (d.process[plane]) {
            const int width = d.vi->width >> (plane ? d.vi->format->subSamplingW : 0);
            const int height = d.vi->height >> (plane ? d.vi->format->subSamplingH : 0);

            if (d.smode == 0) {
                const int ae = (d.sbsize >> 1) << 1;
                d.padWidth[plane] = width + ae;
                d.padHeight[plane] = height + ae;
                d.eheight[plane] = height;
            } else {
                const int ae = std::max(d.sbsize - d.sosize, d.sosize) * 2;
                d.padWidth[plane] = width + EXTRA(width, d.sbsize) + ae;
                d.padHeight[plane] = height + EXTRA(height, d.sbsize) + ae;
                d.eheight[plane] = ((d.padHeight[plane] - d.sosize) / (d.sbsize - d.sosize)) * (d.sbsize - d.sosize);
            }
        }
    }

    d.hw = vs_aligned_malloc<float>(d.bvolume * sizeof(float), 32);
    if (!d.hw) {
        vsapi->setError(out, "DFTTest: malloc failure (hw)");
        vsapi->freeNode(d.node);
        return;
    }
    createWindow(d.hw, d.tmode, d.smode, &d);

    float * dftgr = vs_aligned_malloc<float>(d.bvolume * sizeof(float), 32);
    d.dftgc = vs_aligned_malloc<fftwf_complex>((d.ccnt + 11) * sizeof(fftwf_complex), 32);
    if (!dftgr || !d.dftgc) {
        vsapi->setError(out, "DFTTest: malloc failure (dftgr/dftgc)");
        vsapi->freeNode(d.node);
        return;
    }

    fftwf_plan ftg;
    if (d.tbsize > 1)
        ftg = fftwf_plan_dft_r2c_3d(d.tbsize, d.sbsize, d.sbsize, dftgr, d.dftgc, FFTW_PATIENT | FFTW_DESTROY_INPUT);
    else
        ftg = fftwf_plan_dft_r2c_2d(d.sbsize, d.sbsize, dftgr, d.dftgc, FFTW_PATIENT | FFTW_DESTROY_INPUT);
    float wscale = 0.f;
    const float * hwT = d.hw;
    float * dftgrT = dftgr;
    for (int s = 0; s < d.tbsize; s++) {
        for (int i = 0; i < d.sbsize; i++) {
            for (int k = 0; k < d.sbsize; k++) {
                dftgrT[k] = 255.f * hwT[k];
                wscale += hwT[k] * hwT[k];
            }
            dftgrT += d.sbsize;
            hwT += d.sbsize;
        }
    }
    wscale = 1.f / wscale;
    const float wscalef = (d.ftype < 2) ? wscale : 1.f;
    fftwf_execute_dft_r2c(ftg, dftgr, d.dftgc);

    d.sigmas = vs_aligned_malloc<float>((d.ccnt * 2 + 11) * sizeof(float), 32);
    d.sigmas2 = vs_aligned_malloc<float>((d.ccnt * 2 + 11) * sizeof(float), 32);
    d.pmins = vs_aligned_malloc<float>((d.ccnt * 2 + 11) * sizeof(float), 32);
    d.pmaxs = vs_aligned_malloc<float>((d.ccnt * 2 + 11) * sizeof(float), 32);
    if (!d.sigmas || !d.sigmas2 || !d.pmins || !d.pmaxs) {
        vsapi->setError(out, "DFTTest: malloc failure (sigmas/sigmas2/pmins/pmaxs)");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.sstring[0] || d.ssx[0] || d.ssy[0] || d.sst[0]) {
        int ndim = 3;
        if (d.tbsize == 1)
            ndim -= 1;
        if (d.sbsize == 1)
            ndim -= 2;
        const float ndiv = 1.f / ndim;
        int tcnt = 0, sycnt = 0, sxcnt = 0;
        float * tdata, * sydata, * sxdata;
        bool edis = false;
        if (d.sstring[0]) {
            const char * w = d.sstring;
            if (d.sstring[0] == '$') { // FFT3DFilter method
                edis = true;
                while ((w[0] == '$' || w[0] == ' ') && w[0] != 0)
                    w++;
            }
            tdata = parseString(w, tcnt, d.sigma, edis ? 1.f : ndiv, out, vsapi);
            if (!tdata) {
                vsapi->freeNode(d.node);
                return;
            }
            sydata = parseString(w, sycnt, d.sigma, edis ? 1.f : ndiv, out, vsapi);
            if (!sydata) {
                vsapi->freeNode(d.node);
                return;
            }
            sxdata = parseString(w, sxcnt, d.sigma, edis ? 1.f : ndiv, out, vsapi);
            if (!sxdata) {
                vsapi->freeNode(d.node);
                return;
            }
        } else {
            tdata = parseString(d.sst, tcnt, d.sigma, ndiv, out, vsapi);
            if (!tdata) {
                vsapi->freeNode(d.node);
                return;
            }
            sydata = parseString(d.ssy, sycnt, d.sigma, ndiv, out, vsapi);
            if (!sydata) {
                vsapi->freeNode(d.node);
                return;
            }
            sxdata = parseString(d.ssx, sxcnt, d.sigma, ndiv, out, vsapi);
            if (!sxdata) {
                vsapi->freeNode(d.node);
                return;
            }
        }
        const int cpx = d.sbsize / 2 + 1;
        float pft, pfy, pfx;
        for (int z = 0; z < d.tbsize; z++) {
            const float tval = getSVal(z, d.tbsize, tdata, tcnt, pft);
            for (int y = 0; y < d.sbsize; y++) {
                const float syval = getSVal(y, d.sbsize, sydata, sycnt, pfy);
                for (int x = 0; x < cpx; x++) {
                    const float sxval = getSVal(x, d.sbsize, sxdata, sxcnt, pfx);
                    float val;
                    if (edis) {
                        const float dw = std::sqrt((pft * pft + pfy * pfy + pfx * pfx) / ndim);
                        val = interp(dw, tdata, tcnt);
                    } else {
                        val = tval * syval * sxval;
                    }
                    const int pos = ((z * d.sbsize + y) * cpx + x) * 2;
                    d.sigmas[pos] = d.sigmas[pos + 1] = val / wscalef;
                }
            }
        }
        delete[] tdata;
        delete[] sydata;
        delete[] sxdata;
    } else {
        for (int i = 0; i < d.ccnt * 2; i++)
            d.sigmas[i] = d.sigma / wscalef;
    }
    for (int i = 0; i < d.ccnt * 2; i++) {
        d.sigmas2[i] = d.sigma2 / wscalef;
        d.pmins[i] = d.pmin / wscale;
        d.pmaxs[i] = d.pmax / wscale;
    }

    fftwf_complex * ta = vs_aligned_malloc<fftwf_complex>((d.ccnt + 3) * sizeof(fftwf_complex), 32);
    if (!ta) {
        vsapi->setError(out, "DFTTest: malloc failure (ta)");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.tbsize > 1) {
        d.ft = fftwf_plan_dft_r2c_3d(d.tbsize, d.sbsize, d.sbsize, dftgr, ta, FFTW_PATIENT | FFTW_DESTROY_INPUT);
        d.fti = fftwf_plan_dft_c2r_3d(d.tbsize, d.sbsize, d.sbsize, ta, dftgr, FFTW_PATIENT | FFTW_DESTROY_INPUT);
    } else {
        d.ft = fftwf_plan_dft_r2c_2d(d.sbsize, d.sbsize, dftgr, ta, FFTW_PATIENT | FFTW_DESTROY_INPUT);
        d.fti = fftwf_plan_dft_c2r_2d(d.sbsize, d.sbsize, ta, dftgr, FFTW_PATIENT | FFTW_DESTROY_INPUT);
    }
    vs_aligned_free(dftgr);
    vs_aligned_free(ta);

    if (d.vi->format->bitsPerSample == 8) {
        d.proc0 = proc0_8;
        d.intcast = intcast_8;
    } else {
        d.proc0 = proc0_16;
        d.intcast = intcast_16;
    }

    if (d.ftype == 0) {
        if (std::fabs(d.f0beta - 1.f) < 0.00005f)
            d.filterCoeffs = filter_0;
        else if (std::fabs(d.f0beta - 0.5f) < 0.00005f)
            d.filterCoeffs = filter_6;
        else
            d.filterCoeffs = filter_5;
    } else if (d.ftype == 1) {
        d.filterCoeffs = filter_1;
    } else if (d.ftype == 2) {
        d.filterCoeffs = filter_2;
    } else if (d.ftype == 3) {
        d.filterCoeffs = filter_3;
    } else {
        d.filterCoeffs = filter_4;
    }

    if (d.ftype != 0)
        d.f0beta = 1.f;

    if (d.nstring[0] && d.ftype < 2) {
        memset(d.sigmas, 0, d.ccnt * 2 * sizeof(float));
        float * hw2 = vs_aligned_malloc<float>(d.bvolume * sizeof(float), 32);
        if (!hw2) {
            vsapi->setError(out, "DFTTest: malloc failure (hw2)");
            vsapi->freeNode(d.node);
            return;
        }
        createWindow(hw2, 0, 0, &d);
        float * dftr = vs_aligned_malloc<float>(d.bvolume * sizeof(float), 32);
        fftwf_complex * dftgc2 = vs_aligned_malloc<fftwf_complex>((d.ccnt + 11) * sizeof(fftwf_complex), 32);
        if (!dftr || !dftgc2) {
            vsapi->setError(out, "DFTTest: malloc failure (dftr/dftgc2)");
            vsapi->freeNode(d.node);
            return;
        }
        float wscale2 = 0.f;
        float alpha = (d.ftype == 0) ? 5.f : 7.f;
        int w = 0;
        for (int s = 0; s < d.tbsize; s++) {
            for (int i = 0; i < d.sbsize; i++) {
                for (int k = 0; k < d.sbsize; k++, w++) {
                    dftr[w] = 255.f * hw2[w];
                    wscale2 += hw2[w] * hw2[w];
                }
            }
        }
        wscale2 = 1.f / wscale2;
        fftwf_execute_dft_r2c(ftg, dftr, dftgc2);
        int nnpoints = 0;
        NPInfo * npts = new NPInfo[500];
        const char * q = d.nstring;
        if (q[0] == 'a' || q[0] == 'A') {
            float alphat;
            if (std::sscanf(q, "%*c:%f", &alphat) != 1) {
                vsapi->setError(out, "DFTTest: error reading alpha value from nstring");
                vsapi->freeNode(d.node);
                return;
            }
            if (alphat <= 0.f) {
                vsapi->setError(out, "DFTTest: nstring - invalid alpha factor");
                vsapi->freeNode(d.node);
                return;
            }
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
            if (fn < 0 || fn > d.vi->numFrames - d.tbsize) {
                vsapi->setError(out, ("DFTTest: invalid frame number in nstring (" + std::to_string(fn) + ")").c_str());
                vsapi->freeNode(d.node);
                return;
            }
            if (b < 0 || b >= d.vi->format->numPlanes) {
                vsapi->setError(out, ("DFTTest: invalid plane number in nstring (" + std::to_string(b) + ")").c_str());
                vsapi->freeNode(d.node);
                return;
            }
            const int height = d.vi->height >> (b ? d.vi->format->subSamplingH : 0);
            if (y < 0 || y > height - d.sbsize) {
                vsapi->setError(out, ("DFTTest: invalid y pos in nstring (" + std::to_string(y) + ")").c_str());
                vsapi->freeNode(d.node);
                return;
            }
            const int width = d.vi->width >> (b ? d.vi->format->subSamplingW : 0);
            if (x < 0 || x > width - d.sbsize) {
                vsapi->setError(out, ("DFTTest: invalid x pos in nstring (" + std::to_string(x) + ")").c_str());
                vsapi->freeNode(d.node);
                return;
            }
            if (nnpoints >= 300) {
                vsapi->setError(out, "DFTTest: maximum number of entries in nstring is 500");
                vsapi->freeNode(d.node);
                return;
            }
            npts[nnpoints].fn = fn;
            npts[nnpoints].b = b;
            npts[nnpoints].y = y;
            npts[nnpoints].x = x;
            nnpoints++;
            while (q[0] != ' ' && q[0] != 0)
                q++;
        }
        for (int ct = 0; ct < nnpoints; ct++) {
            fftwf_complex * dftc = vs_aligned_malloc<fftwf_complex>((d.ccnt + 11) * sizeof(fftwf_complex), 32);
            fftwf_complex * dftc2 = vs_aligned_malloc<fftwf_complex>((d.ccnt + 11) * sizeof(fftwf_complex), 32);
            if (!dftc || !dftc2) {
                vsapi->setError(out, "DFTTest: malloc failure (dftc/dftc2)");
                vsapi->freeNode(d.node);
                return;
            }
            for (int z = 0; z < d.tbsize; z++) {
                const VSFrameRef * src = vsapi->getFrame(npts[ct].fn + z, d.node, nullptr, 0);
                const int stride = vsapi->getStride(src, npts[ct].b) / d.vi->format->bytesPerSample;
                if (d.vi->format->bitsPerSample == 8) {
                    const uint8_t * srcp = vsapi->getReadPtr(src, npts[ct].b) + stride * npts[ct].y + npts[ct].x;
                    d.proc0(srcp, hw2 + d.sbsize * d.sbsize * z, dftr + d.sbsize * d.sbsize * z, stride, d.sbsize, d.vi->format->bitsPerSample);
                } else {
                    const uint16_t * srcp = reinterpret_cast<const uint16_t *>(vsapi->getReadPtr(src, npts[ct].b)) + stride * npts[ct].y + npts[ct].x;
                    d.proc0(reinterpret_cast<const uint8_t *>(srcp), hw2 + d.sbsize * d.sbsize * z, dftr + d.sbsize * d.sbsize * z, stride, d.sbsize, d.vi->format->bitsPerSample);
                }
                vsapi->freeFrame(src);
            }
            fftwf_execute_dft_r2c(d.ft, dftr, dftc);
            if (d.zmean)
                removeMean(reinterpret_cast<float *>(dftc), reinterpret_cast<const float *>(dftgc2), d.ccnt * 2, reinterpret_cast<float *>(dftc2));
            for (int h = 0; h < d.ccnt * 2; h += 2) {
                const float psd = reinterpret_cast<float *>(dftc)[h] * reinterpret_cast<float *>(dftc)[h] + reinterpret_cast<float *>(dftc)[h + 1] * reinterpret_cast<float *>(dftc)[h + 1];
                d.sigmas[h] += psd;
                d.sigmas[h + 1] += psd;
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
            for (int h = 0; h < d.ccnt * 2; h++)
                d.sigmas[h] *= scale * (wscale2 / wscale) * alpha;
        } else {
            vsapi->setError(out, "DFTTest: no noise blocks in nstring");
            vsapi->freeNode(d.node);
            return;
        }
    }
    fftwf_destroy_plan(ftg);

    DFTTestData * data = new DFTTestData(d);

    vsapi->createFilter(in, out, "DFTTest", dfttestInit, dfttestGetFrame, dfttestFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.dfttest", "dfttest", "2D/3D frequency domain denoiser", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("DFTTest",
                 "clip:clip;ftype:int:opt;sigma:float:opt;sigma2:float:opt;pmin:float:opt;pmax:float:opt;"
                 "sbsize:int:opt;smode:int:opt;sosize:int:opt;tbsize:int:opt;tmode:int:opt;tosize:int:opt;"
                 "swin:int:opt;twin:int:opt;sbeta:float:opt;tbeta:float:opt;zmean:int:opt;f0beta:float:opt;"
                 "nstring:data:opt;sstring:data:opt;ssx:data:opt;ssy:data:opt;sst:data:opt;planes:int[]:opt;",
                 dfttestCreate, nullptr, plugin);
}
