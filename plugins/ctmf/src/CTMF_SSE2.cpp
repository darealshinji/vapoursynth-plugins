#ifdef VS_TARGET_CPU_X86
#include "CTMF.hpp"

static inline void histogramAdd_sse2(const uint16_t * _x, uint16_t * _y, const uint16_t bins) noexcept {
    for (uint16_t i = 0; i < bins; i += 8) {
        const Vec8us x = Vec8us().load_a(_x + i);
        const Vec8us y = Vec8us().load_a(_y + i);
        (y + x).store_a(_y + i);
    }
}

static inline void histogramSub_sse2(const uint16_t * _x, uint16_t * _y, const uint16_t bins) noexcept {
    for (uint16_t i = 0; i < bins; i += 8) {
        const Vec8us x = Vec8us().load_a(_x + i);
        const Vec8us y = Vec8us().load_a(_y + i);
        (y - x).store_a(_y + i);
    }
}

static inline void histogramMulAdd_sse2(const uint16_t a, const uint16_t * _x, uint16_t * _y, const uint16_t bins) noexcept {
    for (uint16_t i = 0; i < bins; i += 8) {
        const Vec8us x = Vec8us().load_a(_x + i);
        const Vec8us y = Vec8us().load_a(_y + i);
        (y + a * x).store_a(_y + i);
    }
}

template<typename T, uint16_t bins>
void process_sse2(const T * srcp, T * VS_RESTRICT dstp, uint16_t * VS_RESTRICT hCoarse, uint16_t * VS_RESTRICT hFine, const CTMFData * d,
                  const int width, const int height, const int stride, const bool padLeft, const bool padRight) noexcept {
    const T * p, * q;

    Histogram<bins> H;
    uint16_t luc[bins];

    memset(hCoarse, 0, bins * width * sizeof(uint16_t));
    memset(hFine, 0, bins * bins * width * sizeof(uint16_t));

    // First row initialization
    for (int j = 0; j < width; j++) {
        hCoarse[bins * j + (srcp[j] >> d->shiftRight)] += d->radius + 1;
        hFine[bins * (width * (srcp[j] >> d->shiftRight) + j) + (srcp[j] & d->mask)] += d->radius + 1;
    }
    for (int i = 0; i < d->radius; i++) {
        for (int j = 0; j < width; j++) {
            hCoarse[bins * j + (srcp[stride * i + j] >> d->shiftRight)]++;
            hFine[bins * (width * (srcp[stride * i + j] >> d->shiftRight) + j) + (srcp[stride * i + j] & d->mask)]++;
        }
    }

    for (int i = 0; i < height; i++) {
        // Update column histograms for entire row
        p = srcp + stride * std::max(0, i - d->radius - 1);
        q = p + width;
        for (int j = 0; p != q; j++) {
            hCoarse[bins * j + (*p >> d->shiftRight)]--;
            hFine[bins * (width * (*p >> d->shiftRight) + j) + (*p & d->mask)]--;
            p++;
        }

        p = srcp + stride * std::min(height - 1, i + d->radius);
        q = p + width;
        for (int j = 0; p != q; j++) {
            hCoarse[bins * j + (*p >> d->shiftRight)]++;
            hFine[bins * (width * (*p >> d->shiftRight) + j) + (*p & d->mask)]++;
            p++;
        }

        // First column initialization
        memset(&H, 0, sizeof(H));
        memset(luc, 0, sizeof(luc));
        if (padLeft)
            histogramMulAdd_sse2(d->radius, &hCoarse[0], H.coarse, bins);
        for (int j = 0; j < (padLeft ? d->radius : 2 * d->radius); j++)
            histogramAdd_sse2(&hCoarse[bins * j], H.coarse, bins);
        for (int k = 0; k < bins; k++)
            histogramMulAdd_sse2(2 * d->radius + 1, &hFine[bins * width * k], &H.fine[k][0], bins);

        for (int j = padLeft ? 0 : d->radius; j < (padRight ? width : width - d->radius); j++) {
            uint16_t sum = 0, * segment;
            int k, b;

            histogramAdd_sse2(&hCoarse[bins * std::min(j + d->radius, width - 1)], H.coarse, bins);

            // Find median at coarse level
            for (k = 0; k < bins; k++) {
                sum += H.coarse[k];
                if (sum > d->t) {
                    sum -= H.coarse[k];
                    break;
                }
            }
            assert(k < bins);

            // Update corresponding histogram segment
            if (luc[k] <= j - d->radius) {
                memset(&H.fine[k], 0, bins * sizeof(uint16_t));
                for (luc[k] = j - d->radius; luc[k] < std::min(j + d->radius + 1, width); luc[k]++)
                    histogramAdd_sse2(&hFine[bins * (width * k + luc[k])], H.fine[k], bins);
                if (luc[k] < j + d->radius + 1) {
                    histogramMulAdd_sse2(j + d->radius + 1 - width, &hFine[bins * (width * k + width - 1)], &H.fine[k][0], bins);
                    luc[k] = j + d->radius + 1;
                }
            } else {
                for (; luc[k] < j + d->radius + 1; luc[k]++) {
                    histogramSub_sse2(&hFine[bins * (width * k + std::max(luc[k] - 2 * d->radius - 1, 0))], H.fine[k], bins);
                    histogramAdd_sse2(&hFine[bins * (width * k + std::min(static_cast<int>(luc[k]), width - 1))], H.fine[k], bins);
                }
            }

            histogramSub_sse2(&hCoarse[bins * std::max(j - d->radius, 0)], H.coarse, bins);

            // Find median in segment
            segment = H.fine[k];
            for (b = 0; b < bins; b++) {
                sum += segment[b];
                if (sum > d->t) {
                    dstp[stride * i + j] = bins * k + b;
                    break;
                }
            }
            assert(b < bins);
        }
    }
}

template void process_sse2<uint8_t, 16>(const uint8_t *, uint8_t *, uint16_t *, uint16_t *, const CTMFData *, const int, const int, const int, const bool, const bool) noexcept;
template void process_sse2<uint16_t, 32>(const uint16_t *, uint16_t *, uint16_t *, uint16_t *, const CTMFData *, const int, const int, const int, const bool, const bool) noexcept;
template void process_sse2<uint16_t, 64>(const uint16_t *, uint16_t *, uint16_t *, uint16_t *, const CTMFData *, const int, const int, const int, const bool, const bool) noexcept;
template void process_sse2<uint16_t, 128>(const uint16_t *, uint16_t *, uint16_t *, uint16_t *, const CTMFData *, const int, const int, const int, const bool, const bool) noexcept;
template void process_sse2<uint16_t, 256>(const uint16_t *, uint16_t *, uint16_t *, uint16_t *, const CTMFData *, const int, const int, const int, const bool, const bool) noexcept;

template<typename T>
static inline void sort(T & a, T & b) noexcept {
    const T temp = max(a, b);
    a = min(a, b);
    b = temp;
}

template<typename T1, typename T2, uint8_t step>
void processRadius2_sse2(const VSFrameRef * src, VSFrameRef * dst, const int plane, const uint8_t widthPad, const VSAPI * vsapi) noexcept {
    const int width = vsapi->getFrameWidth(dst, plane);
    const unsigned height = vsapi->getFrameHeight(dst, plane);
    const unsigned srcStride = vsapi->getStride(src, 0) / sizeof(T1);
    const unsigned dstStride = vsapi->getStride(dst, plane) / sizeof(T1);
    const T1 * srcp = reinterpret_cast<const T1 *>(vsapi->getReadPtr(src, 0)) + srcStride * 2 + widthPad;
    T1 * dstp = reinterpret_cast<T1 *>(vsapi->getWritePtr(dst, plane));

    const T1 * srcp2p = srcp - srcStride * 2;
    const T1 * srcp1p = srcp - srcStride;
    const T1 * srcp1n = srcp + srcStride;
    const T1 * srcp2n = srcp + srcStride * 2;

    for (unsigned y = 0; y < height; y++) {
        for (int x = 0; x < width; x += step) {
            T2 a[25];
            a[0] = T2().load(srcp2p + x - 2);
            a[1] = T2().load(srcp2p + x - 1);
            a[2] = T2().load_a(srcp2p + x);
            a[3] = T2().load(srcp2p + x + 1);
            a[4] = T2().load(srcp2p + x + 2);
            a[5] = T2().load(srcp1p + x - 2);
            a[6] = T2().load(srcp1p + x - 1);
            a[7] = T2().load_a(srcp1p + x);
            a[8] = T2().load(srcp1p + x + 1);
            a[9] = T2().load(srcp1p + x + 2);
            a[10] = T2().load(srcp + x - 2);
            a[11] = T2().load(srcp + x - 1);
            a[12] = T2().load_a(srcp + x);
            a[13] = T2().load(srcp + x + 1);
            a[14] = T2().load(srcp + x + 2);
            a[15] = T2().load(srcp1n + x - 2);
            a[16] = T2().load(srcp1n + x - 1);
            a[17] = T2().load_a(srcp1n + x);
            a[18] = T2().load(srcp1n + x + 1);
            a[19] = T2().load(srcp1n + x + 2);
            a[20] = T2().load(srcp2n + x - 2);
            a[21] = T2().load(srcp2n + x - 1);
            a[22] = T2().load_a(srcp2n + x);
            a[23] = T2().load(srcp2n + x + 1);
            a[24] = T2().load(srcp2n + x + 2);

            sort(a[0], a[1]); sort(a[3], a[4]); sort(a[2], a[4]);
            sort(a[2], a[3]); sort(a[6], a[7]); sort(a[5], a[7]);
            sort(a[5], a[6]); sort(a[9], a[10]); sort(a[8], a[10]);
            sort(a[8], a[9]); sort(a[12], a[13]); sort(a[11], a[13]);
            sort(a[11], a[12]); sort(a[15], a[16]); sort(a[14], a[16]);
            sort(a[14], a[15]); sort(a[18], a[19]); sort(a[17], a[19]);
            sort(a[17], a[18]); sort(a[21], a[22]); sort(a[20], a[22]);
            sort(a[20], a[21]); sort(a[23], a[24]); sort(a[2], a[5]);
            sort(a[3], a[6]); sort(a[0], a[6]); sort(a[0], a[3]);
            sort(a[4], a[7]); sort(a[1], a[7]); sort(a[1], a[4]);
            sort(a[11], a[14]); sort(a[8], a[14]); sort(a[8], a[11]);
            sort(a[12], a[15]); sort(a[9], a[15]); sort(a[9], a[12]);
            sort(a[13], a[16]); sort(a[10], a[16]); sort(a[10], a[13]);
            sort(a[20], a[23]); sort(a[17], a[23]); sort(a[17], a[20]);
            sort(a[21], a[24]); sort(a[18], a[24]); sort(a[18], a[21]);
            sort(a[19], a[22]); sort(a[9], a[18]); sort(a[0], a[18]);
            a[17] = max(a[8], a[17]);
            a[9] = max(a[0], a[9]);
            sort(a[10], a[19]); sort(a[1], a[19]); sort(a[1], a[10]);
            sort(a[11], a[20]); sort(a[2], a[20]); sort(a[12], a[21]);
            a[11] = max(a[2], a[11]);
            sort(a[3], a[21]); sort(a[3], a[12]); sort(a[13], a[22]);
            a[4] = min(a[4], a[22]);
            sort(a[4], a[13]); sort(a[14], a[23]);
            sort(a[5], a[23]); sort(a[5], a[14]); sort(a[15], a[24]);
            a[6] = min(a[6], a[24]);
            sort(a[6], a[15]);
            a[7] = min(a[7], a[16]);
            a[7] = min(a[7], a[19]);
            a[13] = min(a[13], a[21]);
            a[15] = min(a[15], a[23]);
            a[7] = min(a[7], a[13]);
            a[7] = min(a[7], a[15]);
            a[9] = max(a[1], a[9]);
            a[11] = max(a[3], a[11]);
            a[17] = max(a[5], a[17]);
            a[17] = max(a[11], a[17]);
            a[17] = max(a[9], a[17]);
            sort(a[4], a[10]);
            sort(a[6], a[12]); sort(a[7], a[14]); sort(a[4], a[6]);
            a[7] = max(a[4], a[7]);
            sort(a[12], a[14]);
            a[10] = min(a[10], a[14]);
            sort(a[6], a[7]); sort(a[10], a[12]); sort(a[6], a[10]);
            a[17] = max(a[6], a[17]);
            sort(a[12], a[17]);
            a[7] = min(a[7], a[17]);
            sort(a[7], a[10]); sort(a[12], a[18]);
            a[12] = max(a[7], a[12]);
            a[10] = min(a[10], a[18]);
            sort(a[12], a[20]);
            a[10] = min(a[10], a[20]);
            a[12] = max(a[10], a[12]);

            a[12].stream(dstp + x);
        }

        srcp2p += srcStride;
        srcp1p += srcStride;
        srcp += srcStride;
        srcp1n += srcStride;
        srcp2n += srcStride;
        dstp += dstStride;
    }
}

template void processRadius2_sse2<uint8_t, Vec16uc, 16>(const VSFrameRef *, VSFrameRef *, const int, const uint8_t, const VSAPI *) noexcept;
template void processRadius2_sse2<uint16_t, Vec8us, 8>(const VSFrameRef *, VSFrameRef *, const int, const uint8_t, const VSAPI *) noexcept;
#endif
