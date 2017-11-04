#ifdef VS_TARGET_CPU_X86
#ifndef __SSE4_1__
#define __SSE4_1__
#endif

#include "EEDI3.hpp"

template<typename T>
static inline void calculateConnectionCosts(const void * srcp, float * ccosts, const int width, const int stride, const EEDI3Data * d) noexcept {
    const __m128i * src3p = reinterpret_cast<const __m128i *>(srcp) + 12;
    const __m128i * src1p = src3p + stride;
    const __m128i * src1n = src1p + stride;
    const __m128i * src3n = src1n + stride;

    if (d->cost3) {
        for (int x = 0; x < width; x++) {
            const int umax = std::min({ x, width - 1 - x, d->mdis });

            for (int u = -umax; u <= umax; u++) {
                const int u2 = u * 2;
                const bool s1Flag = (u >= 0 && x >= u2) || (u <= 0 && x < width + u2);
                const bool s2Flag = (u <= 0 && x >= -u2) || (u >= 0 && x < width - u2);

                Vec4i s0 = zero_128b(), s1 = zero_128b(), s2 = zero_128b();

                for (int k = -(d->nrad); k <= d->nrad; k++)
                    s0 += abs(Vec4i().load_a(src3p + x + u + k) - Vec4i().load_a(src1p + x - u + k)) +
                          abs(Vec4i().load_a(src1p + x + u + k) - Vec4i().load_a(src1n + x - u + k)) +
                          abs(Vec4i().load_a(src1n + x + u + k) - Vec4i().load_a(src3n + x - u + k));

                if (s1Flag) {
                    for (int k = -(d->nrad); k <= d->nrad; k++)
                        s1 += abs(Vec4i().load_a(src3p + x + k) - Vec4i().load_a(src1p + x - u2 + k)) +
                              abs(Vec4i().load_a(src1p + x + k) - Vec4i().load_a(src1n + x - u2 + k)) +
                              abs(Vec4i().load_a(src1n + x + k) - Vec4i().load_a(src3n + x - u2 + k));
                }

                if (s2Flag) {
                    for (int k = -(d->nrad); k <= d->nrad; k++)
                        s2 += abs(Vec4i().load_a(src3p + x + u2 + k) - Vec4i().load_a(src1p + x + k)) +
                              abs(Vec4i().load_a(src1p + x + u2 + k) - Vec4i().load_a(src1n + x + k)) +
                              abs(Vec4i().load_a(src1n + x + u2 + k) - Vec4i().load_a(src3n + x + k));
                }

                s1 = s1Flag ? s1 : (s2Flag ? s2 : s0);
                s2 = s2Flag ? s2 : (s1Flag ? s1 : s0);

                const Vec4i ip = (Vec4i().load_a(src1p + x + u) + Vec4i().load_a(src1n + x - u) + 1) >> 1; // should use cubic if ucubic=true
                const Vec4f v = to_float(abs(Vec4i().load_a(src1p + x) - ip) + abs(Vec4i().load_a(src1n + x) - ip));
                const Vec4f result = mul_add(d->alpha, to_float(s0 + s1 + s2), d->beta * std::abs(u)) + d->remainingWeight * v;
                result.stream(ccosts + (d->tpitch * x + u) * d->vectorSize);
            }
        }
    } else {
        for (int x = 0; x < width; x++) {
            const int umax = std::min({ x, width - 1 - x, d->mdis });

            for (int u = -umax; u <= umax; u++) {
                Vec4i s = zero_128b();

                for (int k = -(d->nrad); k <= d->nrad; k++)
                    s += abs(Vec4i().load_a(src3p + x + u + k) - Vec4i().load_a(src1p + x - u + k)) +
                         abs(Vec4i().load_a(src1p + x + u + k) - Vec4i().load_a(src1n + x - u + k)) +
                         abs(Vec4i().load_a(src1n + x + u + k) - Vec4i().load_a(src3n + x - u + k));

                const Vec4i ip = (Vec4i().load_a(src1p + x + u) + Vec4i().load_a(src1n + x - u) + 1) >> 1; // should use cubic if ucubic=true
                const Vec4f v = to_float(abs(Vec4i().load_a(src1p + x) - ip) + abs(Vec4i().load_a(src1n + x) - ip));
                const Vec4f result = mul_add(d->alpha, to_float(s), d->beta * std::abs(u)) + d->remainingWeight * v;
                result.stream(ccosts + (d->tpitch * x + u) * d->vectorSize);
            }
        }
    }
}

template<>
inline void calculateConnectionCosts<float>(const void * srcp, float * ccosts, const int width, const int stride, const EEDI3Data * d) noexcept {
    const __m128 * src3p = reinterpret_cast<const __m128 *>(srcp) + 12;
    const __m128 * src1p = src3p + stride;
    const __m128 * src1n = src1p + stride;
    const __m128 * src3n = src1n + stride;

    if (d->cost3) {
        for (int x = 0; x < width; x++) {
            const int umax = std::min({ x, width - 1 - x, d->mdis });

            for (int u = -umax; u <= umax; u++) {
                const int u2 = u * 2;
                const bool s1Flag = (u >= 0 && x >= u2) || (u <= 0 && x < width + u2);
                const bool s2Flag = (u <= 0 && x >= -u2) || (u >= 0 && x < width - u2);

                Vec4f s0 = zero_4f(), s1 = zero_4f(), s2 = zero_4f();

                for (int k = -(d->nrad); k <= d->nrad; k++)
                    s0 += abs(Vec4f().load_a(src3p + x + u + k) - Vec4f().load_a(src1p + x - u + k)) +
                          abs(Vec4f().load_a(src1p + x + u + k) - Vec4f().load_a(src1n + x - u + k)) +
                          abs(Vec4f().load_a(src1n + x + u + k) - Vec4f().load_a(src3n + x - u + k));

                if (s1Flag) {
                    for (int k = -(d->nrad); k <= d->nrad; k++)
                        s1 += abs(Vec4f().load_a(src3p + x + k) - Vec4f().load_a(src1p + x - u2 + k)) +
                              abs(Vec4f().load_a(src1p + x + k) - Vec4f().load_a(src1n + x - u2 + k)) +
                              abs(Vec4f().load_a(src1n + x + k) - Vec4f().load_a(src3n + x - u2 + k));
                }

                if (s2Flag) {
                    for (int k = -(d->nrad); k <= d->nrad; k++)
                        s2 += abs(Vec4f().load_a(src3p + x + u2 + k) - Vec4f().load_a(src1p + x + k)) +
                              abs(Vec4f().load_a(src1p + x + u2 + k) - Vec4f().load_a(src1n + x + k)) +
                              abs(Vec4f().load_a(src1n + x + u2 + k) - Vec4f().load_a(src3n + x + k));
                }

                s1 = s1Flag ? s1 : (s2Flag ? s2 : s0);
                s2 = s2Flag ? s2 : (s1Flag ? s1 : s0);

                const Vec4f ip = (Vec4f().load_a(src1p + x + u) + Vec4f().load_a(src1n + x - u)) * 0.5f; // should use cubic if ucubic=true
                const Vec4f v = abs(Vec4f().load_a(src1p + x) - ip) + abs(Vec4f().load_a(src1n + x) - ip);
                const Vec4f result = mul_add(d->alpha, s0 + s1 + s2, d->beta * std::abs(u)) + d->remainingWeight * v;
                result.stream(ccosts + (d->tpitch * x + u) * d->vectorSize);
            }
        }
    } else {
        for (int x = 0; x < width; x++) {
            const int umax = std::min({ x, width - 1 - x, d->mdis });

            for (int u = -umax; u <= umax; u++) {
                Vec4f s = zero_4f();

                for (int k = -(d->nrad); k <= d->nrad; k++)
                    s += abs(Vec4f().load_a(src3p + x + u + k) - Vec4f().load_a(src1p + x - u + k)) +
                         abs(Vec4f().load_a(src1p + x + u + k) - Vec4f().load_a(src1n + x - u + k)) +
                         abs(Vec4f().load_a(src1n + x + u + k) - Vec4f().load_a(src3n + x - u + k));

                const Vec4f ip = (Vec4f().load_a(src1p + x + u) + Vec4f().load_a(src1n + x - u)) * 0.5f; // should use cubic if ucubic=true
                const Vec4f v = abs(Vec4f().load_a(src1p + x) - ip) + abs(Vec4f().load_a(src1n + x) - ip);
                const Vec4f result = mul_add(d->alpha, s, d->beta * std::abs(u)) + d->remainingWeight * v;
                result.stream(ccosts + (d->tpitch * x + u) * d->vectorSize);
            }
        }
    }
}

template<typename T1, typename T2>
void process_sse4(const VSFrameRef * src, const VSFrameRef * scp, VSFrameRef * dst, VSFrameRef ** pad, const int field_n, const EEDI3Data * d, const VSAPI * vsapi) noexcept {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        if (d->process[plane]) {
            copyPad<T1>(src, pad[plane], plane, 1 - field_n, d->dh, vsapi);

            const int srcWidth = vsapi->getFrameWidth(pad[plane], 0);
            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int srcHeight = vsapi->getFrameHeight(pad[plane], 0);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int srcStride = vsapi->getStride(pad[plane], 0) / sizeof(T1);
            const int dstStride = vsapi->getStride(dst, plane) / sizeof(T1);
            const T1 * _srcp = reinterpret_cast<const T1 *>(vsapi->getReadPtr(pad[plane], 0)) + 12;
            T1 * VS_RESTRICT _dstp = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane));

            const auto threadId = std::this_thread::get_id();
            T2 * srcVector = reinterpret_cast<T2 *>(d->srcVector.at(threadId));
            float * ccosts = d->ccosts.at(threadId) + d->mdisVector;
            float * pcosts = d->pcosts.at(threadId) + d->mdisVector;
            int * _pbackt = d->pbackt.at(threadId) + d->mdisVector;
            int * fpath = d->fpath.at(threadId);
            int * _dmap = d->dmap.at(threadId);
            float * tline = d->tline.at(threadId);

            vs_bitblt(_dstp + dstStride * (1 - field_n), vsapi->getStride(dst, plane) * 2,
                      _srcp + srcStride * (4 + 1 - field_n), vsapi->getStride(pad[plane], 0) * 2,
                      dstWidth * sizeof(T1), dstHeight / 2);

            _srcp += srcStride * 4;
            _dstp += dstStride * field_n;

            for (int y = field_n; y < dstHeight; y += 2 * d->vectorSize) {
                const int off = (y - field_n) >> 1;

                reorder<T1, T2>(_srcp + srcStride * (1 - field_n), srcVector, dstWidth, (dstHeight + field_n) >> 1, srcStride * 2, srcWidth, off + field_n, d->vectorSize);

                calculateConnectionCosts<T1>(srcVector, ccosts, dstWidth, srcWidth, d);

                // calculate path costs
                Vec4f().load_a(ccosts).store_a(pcosts);
                for (int x = 1; x < dstWidth; x++) {
                    const float * tT = ccosts + d->tpitchVector * x;
                    const float * ppT = pcosts + d->tpitchVector * (x - 1);
                    float * pT = pcosts + d->tpitchVector * x;
                    int * piT = _pbackt + d->tpitchVector * (x - 1);

                    const int umax = std::min({ x, dstWidth - 1 - x, d->mdis });
                    const int umax2 = std::min({ x - 1, dstWidth - x, d->mdis });

                    for (int u = -umax; u <= umax; u++) {
                        Vec4i idx = zero_128b();
                        Vec4f bval = FLT_MAX;

                        for (int v = std::max(-umax2, u - 1); v <= std::min(umax2, u + 1); v++) {
                            const Vec4f z = Vec4f().load_a(ppT + v * d->vectorSize) + d->gamma * std::abs(u - v);
                            const Vec4f ccost = min(z, FLT_MAX * 0.9f);
                            idx = select(Vec4ib(ccost < bval), v, idx);
                            bval = min(ccost, bval);
                        }

                        const Vec4f z = bval + Vec4f().load_a(tT + u * d->vectorSize);
                        min(z, FLT_MAX * 0.9f).store_a(pT + u * d->vectorSize);
                        idx.stream(piT + u * d->vectorSize);
                    }
                }

                for (int vs = 0; vs < d->vectorSize; vs++) {
                    const int realY = field_n + 2 * (off + vs);
                    if (realY >= dstHeight)
                        break;

                    const T1 * srcp = _srcp + srcStride * realY;
                    T1 * dstp = _dstp + dstStride * 2 * (off + vs);
                    int * dmap = _dmap + dstWidth * (off + vs);

                    const T1 * src3p = srcp - srcStride * 3;
                    const T1 * src1p = srcp - srcStride;
                    const T1 * src1n = srcp + srcStride;
                    const T1 * src3n = srcp + srcStride * 3;

                    const int * pbackt = _pbackt + vs;

                    // backtrack
                    fpath[dstWidth - 1] = 0;
                    for (int x = dstWidth - 2; x >= 0; x--)
                        fpath[x] = pbackt[(d->tpitch * x + fpath[x + 1]) * d->vectorSize];

                    interpolate<T1>(src3p, src1p, src1n, src3n, fpath, dmap, dstp, dstWidth, d->ucubic, d->peak);
                }
            }

            _srcp += srcStride * field_n;

            if (d->vcheck) {
                const T1 * scpp = nullptr;
                if (d->sclip)
                    scpp = reinterpret_cast<const T1 *>(vsapi->getReadPtr(scp, plane)) + dstStride * field_n;

                vCheck<T1>(_srcp, scpp, _dstp, _dmap, tline, field_n, dstWidth, srcHeight, srcStride, dstStride, d->vcheck, d->vthresh2, d->rcpVthresh0, d->rcpVthresh1, d->rcpVthresh2, d->peak);
            }
        }
    }
}

template void process_sse4<uint8_t, int>(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3Data *, const VSAPI *) noexcept;
template void process_sse4<uint16_t, int>(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3Data *, const VSAPI *) noexcept;
template void process_sse4<float, float>(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3Data *, const VSAPI *) noexcept;
#endif
