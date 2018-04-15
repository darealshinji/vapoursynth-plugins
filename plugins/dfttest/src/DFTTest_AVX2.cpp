#ifdef VS_TARGET_CPU_X86
#ifndef __AVX2__
#define __AVX2__
#endif

#include "DFTTest.hpp"

#include "vectorclass/vectormath_exp.h"

template<typename T>
static inline void proc0(const T * s0, const float * s1, float * d, const int p0, const int p1, const float divisor) noexcept;

template<>
inline void proc0(const uint8_t * _s0, const float * _s1, float * d, const int p0, const int p1, const float divisor) noexcept {
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v += 8) {
            const Vec8f s0 = to_float(Vec8i().load_8uc(_s0 + v));
            const Vec8f s1 = Vec8f().load(_s1 + v);
            (s0 * s1).store(d + v);
        }

        _s0 += p0;
        _s1 += p1;
        d += p1;
    }
}

template<>
inline void proc0(const uint16_t * _s0, const float * _s1, float * d, const int p0, const int p1, const float divisor) noexcept {
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v += 8) {
            const Vec8f s0 = to_float(Vec8i().load_8us(_s0 + v));
            const Vec8f s1 = Vec8f().load(_s1 + v);
            (s0 * divisor * s1).store(d + v);
        }

        _s0 += p0;
        _s1 += p1;
        d += p1;
    }
}

template<>
inline void proc0(const float * _s0, const float * _s1, float * d, const int p0, const int p1, const float divisor) noexcept {
    for (int u = 0; u < p1; u++) {
        for (int v = 0; v < p1; v += 8) {
            const Vec8f s0 = Vec8f().load(_s0 + v);
            const Vec8f s1 = Vec8f().load(_s1 + v);
            (s0 * 255.f * s1).store(d + v);
        }

        _s0 += p0;
        _s1 += p1;
        d += p1;
    }
}

static inline void proc1(const float * _s0, const float * _s1, float * _d, const int p0, const int p1) noexcept {
    for (int u = 0; u < p0; u++) {
        for (int v = 0; v < p0; v += 8) {
            const Vec8f s0 = Vec8f().load(_s0 + v);
            const Vec8f s1 = Vec8f().load(_s1 + v);
            const Vec8f d = Vec8f().load(_d + v);
            mul_add(s0, s1, d).store(_d + v);
        }

        _s0 += p0;
        _s1 += p0;
        _d += p1;
    }
}

static inline void proc1Partial(const float * _s0, const float * _s1, float * _d, const int p0, const int p1) noexcept {
    const int regularPart = p0 & -8;

    for (int u = 0; u < p0; u++) {
        int v;

        for (v = 0; v < regularPart; v += 8) {
            const Vec8f s0 = Vec8f().load(_s0 + v);
            const Vec8f s1 = Vec8f().load(_s1 + v);
            const Vec8f d = Vec8f().load(_d + v);
            mul_add(s0, s1, d).store(_d + v);
        }

        const Vec8f s0 = Vec8f().load(_s0 + v);
        const Vec8f s1 = Vec8f().load(_s1 + v);
        const Vec8f d = Vec8f().load(_d + v);
        mul_add(s0, s1, d).store_partial(p0 - v, _d + v);

        _s0 += p0;
        _s1 += p0;
        _d += p1;
    }
}

static inline void removeMean(float * _dftc, const float * _dftgc, const int ccnt, float * _dftc2) noexcept {
    const Vec8f gf = _dftc[0] / _dftgc[0];

    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftgc = Vec8f().load_a(_dftgc + h);
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f dftc2 = gf * dftgc;
        dftc2.store_a(_dftc2 + h);
        (dftc - dftc2).store_a(_dftc + h);
    }
}

static inline void addMean(float * _dftc, const int ccnt, const float * _dftc2) noexcept {
    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f dftc2 = Vec8f().load_a(_dftc2 + h);
        (dftc + dftc2).store_a(_dftc + h);
    }
}

template<int type>
void filter_avx2(float * dftc, const float * sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept;

template<>
void filter_avx2<0>(float * _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f sigmas = Vec8f().load_a(_sigmas + h);
        const Vec8f dftcSquare = dftc * dftc;
        const Vec8f psd = dftcSquare + permute8f<1, 0, 3, 2, 5, 4, 7, 6>(dftcSquare);
        const Vec8f mult = max((psd - sigmas) * approx_recipr(psd + 1e-15f), zero_8f());
        (dftc * mult).store_a(_dftc + h);
    }
}

template<>
void filter_avx2<1>(float * _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f sigmas = Vec8f().load_a(_sigmas + h);
        const Vec8f dftcSquare = dftc * dftc;
        const Vec8f psd = dftcSquare + permute8f<1, 0, 3, 2, 5, 4, 7, 6>(dftcSquare);
        select(psd < sigmas, zero_8f(), dftc).store_a(_dftc + h);
    }
}

template<>
void filter_avx2<2>(float * _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f sigmas = Vec8f().load_a(_sigmas + h);
        (dftc * sigmas).store_a(_dftc + h);
    }
}

template<>
void filter_avx2<3>(float * _dftc, const float * _sigmas, const int ccnt, const float * _pmin, const float * _pmax, const float * _sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f sigmas = Vec8f().load_a(_sigmas + h);
        const Vec8f sigmas2 = Vec8f().load_a(_sigmas2 + h);
        const Vec8f pmin = Vec8f().load_a(_pmin + h);
        const Vec8f pmax = Vec8f().load_a(_pmax + h);
        const Vec8f dftcSquare = dftc * dftc;
        const Vec8f psd = dftcSquare + permute8f<1, 0, 3, 2, 5, 4, 7, 6>(dftcSquare);
        select(psd >= pmin && psd <= pmax, dftc * sigmas, dftc * sigmas2).store_a(_dftc + h);
    }
}

template<>
void filter_avx2<4>(float * _dftc, const float * _sigmas, const int ccnt, const float * _pmin, const float * _pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f sigmas = Vec8f().load_a(_sigmas + h);
        const Vec8f pmin = Vec8f().load_a(_pmin + h);
        const Vec8f pmax = Vec8f().load_a(_pmax + h);
        const Vec8f dftcSquare = dftc * dftc;
        const Vec8f psd = dftcSquare + permute8f<1, 0, 3, 2, 5, 4, 7, 6>(dftcSquare) + 1e-15f;
        const Vec8f mult = sigmas * sqrt(psd * pmax * approx_recipr((psd + pmin) * (psd + pmax)));
        (dftc * mult).store_a(_dftc + h);
    }
}

template<>
void filter_avx2<5>(float * _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    const Vec8f beta = pmin[0];

    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f sigmas = Vec8f().load_a(_sigmas + h);
        const Vec8f dftcSquare = dftc * dftc;
        const Vec8f psd = dftcSquare + permute8f<1, 0, 3, 2, 5, 4, 7, 6>(dftcSquare);
        const Vec8f mult = pow(max((psd - sigmas) * approx_recipr(psd + 1e-15f), zero_8f()), beta);
        (dftc * mult).store_a(_dftc + h);
    }
}

template<>
void filter_avx2<6>(float * _dftc, const float * _sigmas, const int ccnt, const float * pmin, const float * pmax, const float * sigmas2) noexcept {
    for (int h = 0; h < ccnt; h += 8) {
        const Vec8f dftc = Vec8f().load_a(_dftc + h);
        const Vec8f sigmas = Vec8f().load_a(_sigmas + h);
        const Vec8f dftcSquare = dftc * dftc;
        const Vec8f psd = dftcSquare + permute8f<1, 0, 3, 2, 5, 4, 7, 6>(dftcSquare);
        const Vec8f mult = sqrt(max((psd - sigmas) * approx_recipr(psd + 1e-15f), zero_8f()));
        (dftc * mult).store_a(_dftc + h);
    }
}

template<typename T>
static void cast(const float * ebp, T * dstp, const int dstWidth, const int dstHeight, const int dstStride, const int ebpStride, const float multiplier, const int peak) noexcept;

template<>
void cast(const float * ebp, uint8_t * dstp, const int dstWidth, const int dstHeight, const int dstStride, const int ebpStride, const float multiplier, const int peak) noexcept {
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x += 16) {
            const Vec8i srcp_8i_1 = truncate_to_int(Vec8f().load(ebp + x) + 0.5f);
            const Vec8i srcp_8i_2 = truncate_to_int(Vec8f().load(ebp + x + 8) + 0.5f);
            const Vec16s srcp_16s = compress_saturated(srcp_8i_1, srcp_8i_2);
            const Vec16uc srcp = compress_saturated_s2u(srcp_16s.get_low(), srcp_16s.get_high());
            srcp.stream(dstp + x);
        }

        ebp += ebpStride;
        dstp += dstStride;
    }
}

template<>
void cast(const float * ebp, uint16_t * dstp, const int dstWidth, const int dstHeight, const int dstStride, const int ebpStride, const float multiplier, const int peak) noexcept {
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x += 8) {
            const Vec8i srcp_8i = truncate_to_int(mul_add(Vec8f().load(ebp + x), multiplier, 0.5f));
            const Vec8us srcp = compress_saturated_s2u(srcp_8i.get_low(), srcp_8i.get_high());
            min(srcp, peak).stream(dstp + x);
        }

        ebp += ebpStride;
        dstp += dstStride;
    }
}

template<>
void cast(const float * ebp, float * dstp, const int dstWidth, const int dstHeight, const int dstStride, const int ebpStride, const float multiplier, const int peak) noexcept {
    for (int y = 0; y < dstHeight; y++) {
        for (int x = 0; x < dstWidth; x += 8) {
            const Vec8f srcp = Vec8f().load(ebp + x);
            (srcp * (1.f / 255.f)).stream(dstp + x);
        }

        ebp += ebpStride;
        dstp += dstStride;
    }
}

template<typename T>
void func_0_avx2(VSFrameRef * src[3], VSFrameRef * dst, const DFTTestData * d, const VSAPI * vsapi) noexcept {
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

                    if (d->type & 1) { // spatial overlapping
                        if (!(d->sbsize & 7))
                            proc1(dftr, d->hw, ebpSaved + x, d->sbsize, ebpStride);
                        else
                            proc1Partial(dftr, d->hw, ebpSaved + x, d->sbsize, ebpStride);
                    } else {
                        ebpSaved[x + d->sbd1 * ebpStride + d->sbd1] = dftr[d->sbd1 * d->sbsize + d->sbd1] * d->hw[d->sbd1 * d->sbsize + d->sbd1];
                    }
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

template void func_0_avx2<uint8_t>(VSFrameRef * src[3], VSFrameRef * dst, const DFTTestData * d, const VSAPI * vsapi) noexcept;
template void func_0_avx2<uint16_t>(VSFrameRef * src[3], VSFrameRef * dst, const DFTTestData * d, const VSAPI * vsapi) noexcept;
template void func_0_avx2<float>(VSFrameRef * src[3], VSFrameRef * dst, const DFTTestData * d, const VSAPI * vsapi) noexcept;

template<typename T>
void func_1_avx2(VSFrameRef * src[15][3], VSFrameRef * dst, const int pos, const DFTTestData * d, const VSAPI * vsapi) noexcept {
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

                    if (d->type & 1) { // spatial overlapping
                        if (!(d->sbsize & 7))
                            proc1(dftr + pos * d->barea, d->hw + pos * d->barea, ebuff + y * ebpStride + x, d->sbsize, ebpStride);
                        else
                            proc1Partial(dftr + pos * d->barea, d->hw + pos * d->barea, ebuff + y * ebpStride + x, d->sbsize, ebpStride);
                    } else {
                        ebuff[(y + d->sbd1) * ebpStride + x + d->sbd1] = dftr[pos * d->barea + d->sbd1 * d->sbsize + d->sbd1] * d->hw[pos * d->barea + d->sbd1 * d->sbsize + d->sbd1];
                    }
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

template void func_1_avx2<uint8_t>(VSFrameRef * src[15][3], VSFrameRef * dst, const int pos, const DFTTestData * d, const VSAPI * vsapi) noexcept;
template void func_1_avx2<uint16_t>(VSFrameRef * src[15][3], VSFrameRef * dst, const int pos, const DFTTestData * d, const VSAPI * vsapi) noexcept;
template void func_1_avx2<float>(VSFrameRef * src[15][3], VSFrameRef * dst, const int pos, const DFTTestData * d, const VSAPI * vsapi) noexcept;
#endif
