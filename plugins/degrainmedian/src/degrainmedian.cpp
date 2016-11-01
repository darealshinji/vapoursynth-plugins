#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include <VapourSynth.h>
#include <VSHelper.h>


#ifdef _WIN32
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif


#if defined(DEGRAINMEDIAN_X86)

#include <emmintrin.h>


#define LoadPixelsSSE2 \
    __m128i p1, p2, p3, \
            p4, p5, p6, \
            p7, p8, p9; \
    \
    __m128i s1, s2, s3, \
            s4, s5, s6, \
            s7, s8, s9; \
    \
    __m128i n1, n2, n3, \
            n4, n5, n6, \
            n7, n8, n9; \
    \
    p1 = _mm_loadu_si128((const __m128i *)&prevp[x - distance - 1]); \
    p2 = _mm_loadu_si128((const __m128i *)&prevp[x - distance]); \
    p3 = _mm_loadu_si128((const __m128i *)&prevp[x - distance + 1]); \
    p4 = _mm_loadu_si128((const __m128i *)&prevp[x - 1]); \
    p5 = _mm_loadu_si128((const __m128i *)&prevp[x]); \
    p6 = _mm_loadu_si128((const __m128i *)&prevp[x + 1]); \
    p7 = _mm_loadu_si128((const __m128i *)&prevp[x + distance - 1]); \
    p8 = _mm_loadu_si128((const __m128i *)&prevp[x + distance]); \
    p9 = _mm_loadu_si128((const __m128i *)&prevp[x + distance + 1]); \
    \
    s1 = _mm_loadu_si128((const __m128i *)&srcp[x - distance - 1]); \
    s2 = _mm_loadu_si128((const __m128i *)&srcp[x - distance]); \
    s3 = _mm_loadu_si128((const __m128i *)&srcp[x - distance + 1]); \
    s4 = _mm_loadu_si128((const __m128i *)&srcp[x - 1]); \
    s5 = _mm_loadu_si128((const __m128i *)&srcp[x]); \
    s6 = _mm_loadu_si128((const __m128i *)&srcp[x + 1]); \
    s7 = _mm_loadu_si128((const __m128i *)&srcp[x + distance - 1]); \
    s8 = _mm_loadu_si128((const __m128i *)&srcp[x + distance]); \
    s9 = _mm_loadu_si128((const __m128i *)&srcp[x + distance + 1]); \
    \
    n1 = _mm_loadu_si128((const __m128i *)&nextp[x - distance - 1]); \
    n2 = _mm_loadu_si128((const __m128i *)&nextp[x - distance]); \
    n3 = _mm_loadu_si128((const __m128i *)&nextp[x - distance + 1]); \
    n4 = _mm_loadu_si128((const __m128i *)&nextp[x - 1]); \
    n5 = _mm_loadu_si128((const __m128i *)&nextp[x]); \
    n6 = _mm_loadu_si128((const __m128i *)&nextp[x + 1]); \
    n7 = _mm_loadu_si128((const __m128i *)&nextp[x + distance - 1]); \
    n8 = _mm_loadu_si128((const __m128i *)&nextp[x + distance]); \
    n9 = _mm_loadu_si128((const __m128i *)&nextp[x + distance + 1]);


template <typename PixelType>
static FORCE_INLINE __m128i mm_min_epu(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_min_epu8(a, b);
    else {
        const __m128i sign = _mm_set1_epi16(32768);
        return _mm_add_epi16(_mm_min_epi16(_mm_sub_epi16(a, sign), _mm_sub_epi16(b, sign)), sign);
    }
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_max_epu(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_max_epu8(a, b);
    else {
        const __m128i sign = _mm_set1_epi16(32768);
        return _mm_add_epi16(_mm_max_epi16(_mm_sub_epi16(a, sign), _mm_sub_epi16(b, sign)), sign);
    }
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_adds_epu(const __m128i &a, const __m128i &b, const __m128i &pixel_max) {
    if (sizeof(PixelType) == 1)
        return _mm_adds_epu8(a, b);
    else {
        __m128i sum = _mm_adds_epu16(a, b);
        return mm_min_epu<PixelType>(sum, pixel_max);
    }
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_subs_epu(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_subs_epu8(a, b);
    else {
        return _mm_subs_epu16(a, b);
    }
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_cmpeq_epi(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_cmpeq_epi8(a, b);
    else {
        return _mm_cmpeq_epi16(a, b);
    }
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_set1_epi(int value) {
    if (sizeof(PixelType) == 1)
        return _mm_set1_epi8(value);
    else {
        return _mm_set1_epi16(value);
    }
}


template <typename PixelType>
static FORCE_INLINE void checkBetterNeighboursSSE2(const __m128i &a, const __m128i &b, __m128i &diff, __m128i &min, __m128i &max) {
    __m128i new_min = mm_min_epu<PixelType>(a, b);
    __m128i new_max = mm_max_epu<PixelType>(a, b);
    __m128i new_diff = mm_subs_epu<PixelType>(new_max, new_min);

    __m128i mask = mm_subs_epu<PixelType>(new_diff, diff);
    mask = mm_cmpeq_epi<PixelType>(mask, _mm_setzero_si128());

    new_min = _mm_and_si128(new_min, mask);
    new_max = _mm_and_si128(new_max, mask);
    new_diff = _mm_and_si128(new_diff, mask);

    mask = mm_cmpeq_epi<PixelType>(mask, _mm_setzero_si128());

    diff = _mm_and_si128(diff, mask);
    min = _mm_and_si128(min, mask);
    max = _mm_and_si128(max, mask);

    diff = _mm_or_si128(diff, new_diff);
    min = _mm_or_si128(min, new_min);
    max = _mm_or_si128(max, new_max);
}


template <int mode, typename PixelType>
struct Asdf {

    static FORCE_INLINE void diagWeightSSE2(const __m128i &oldp, const __m128i &bound1, const __m128i &bound2, __m128i &old_result, __m128i &old_weight, const __m128i &pixel_max) {
        __m128i max = mm_max_epu<PixelType>(bound1, bound2);
        __m128i min = mm_min_epu<PixelType>(bound1, bound2);
        __m128i diff = mm_subs_epu<PixelType>(max, min);

        __m128i reg2 = mm_subs_epu<PixelType>(oldp, max);

        __m128i newp = mm_min_epu<PixelType>(max, oldp);
        newp = mm_max_epu<PixelType>(newp, min);

        __m128i weight = mm_subs_epu<PixelType>(min, oldp);
        weight = mm_max_epu<PixelType>(weight, reg2);

        if (mode == 4)
            weight = mm_adds_epu<PixelType>(weight, weight, pixel_max);
        else if (mode == 2)
            diff = mm_adds_epu<PixelType>(diff, diff, pixel_max);
        else if (mode == 1) {
            diff = mm_adds_epu<PixelType>(diff, diff, pixel_max);
            diff = mm_adds_epu<PixelType>(diff, diff, pixel_max);
        }

        weight = mm_adds_epu<PixelType>(weight, diff, pixel_max);

        old_weight = mm_min_epu<PixelType>(old_weight, weight);
        weight = mm_cmpeq_epi<PixelType>(weight, old_weight);
        old_result = mm_subs_epu<PixelType>(old_result, weight);
        weight = _mm_and_si128(weight, newp);
        old_result = _mm_or_si128(old_result, weight);
    }

};


template <typename PixelType>
struct Asdf<5, PixelType> {

    static FORCE_INLINE void diagWeightSSE2(const __m128i &oldp, const __m128i &bound1, const __m128i &bound2, __m128i &old_result, __m128i &old_weight, const __m128i &pixel_max) {
        (void)pixel_max;

        __m128i max = mm_max_epu<PixelType>(bound1, bound2);
        __m128i min = mm_min_epu<PixelType>(bound1, bound2);

        __m128i newp = mm_min_epu<PixelType>(max, oldp);
        newp = mm_max_epu<PixelType>(newp, min);

        __m128i reg2 = mm_subs_epu<PixelType>(oldp, max);
        __m128i weight = mm_subs_epu<PixelType>(min, oldp);
        weight = mm_max_epu<PixelType>(weight, reg2);

        old_weight = mm_min_epu<PixelType>(old_weight, weight);
        weight = mm_cmpeq_epi<PixelType>(weight, old_weight);
        old_result = mm_subs_epu<PixelType>(old_result, weight);
        weight = _mm_and_si128(weight, newp);
        old_result = _mm_or_si128(old_result, weight);
    }

};


template <typename PixelType>
static FORCE_INLINE __m128i limitPixelCorrectionSSE2(const __m128i &old_pixel, const __m128i &new_pixel, const __m128i &limit, const __m128i &pixel_max) {
    __m128i m1, m3;

    __m128i upper = mm_adds_epu<PixelType>(old_pixel, limit, pixel_max);
    __m128i lower = mm_subs_epu<PixelType>(old_pixel, limit);

    m3 = mm_subs_epu<PixelType>(new_pixel, old_pixel);
    m3 = mm_subs_epu<PixelType>(m3, limit);
    m3 = mm_cmpeq_epi<PixelType>(m3, _mm_setzero_si128());

    m1 = _mm_and_si128(new_pixel, m3);
    m3 = _mm_andnot_si128(m3, upper);
    m1 = _mm_or_si128(m1, m3);

    m3 = mm_subs_epu<PixelType>(old_pixel, m1);
    m3 = mm_subs_epu<PixelType>(m3, limit);
    m3 = mm_cmpeq_epi<PixelType>(m3, _mm_setzero_si128());

    m1 = _mm_and_si128(m1, m3);
    m3 = _mm_andnot_si128(m3, lower);
    m1 = _mm_or_si128(m1, m3);

    return m1;
}


// Wrapped in struct because function templates can't be partially specialised.
template <int mode, bool norow, typename PixelType>
struct DegrainSSE2 {

    static FORCE_INLINE __m128i degrainPixels(const PixelType *prevp, const PixelType *srcp, const PixelType *nextp, int x, int distance, const __m128i &limit, const __m128i &pixel_max) {
        LoadPixelsSSE2;

        __m128i result;
        __m128i weight = _mm_set1_epi8(255);

        Asdf<mode, PixelType>::diagWeightSSE2(s5, s1, s9, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, s7, s3, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, s8, s2, result, weight, pixel_max);
        if (!norow)
            Asdf<mode, PixelType>::diagWeightSSE2(s5, s6, s4, result, weight, pixel_max);

        Asdf<mode, PixelType>::diagWeightSSE2(s5, n1, p9, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, n3, p7, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, n7, p3, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, n9, p1, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, n8, p2, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, n2, p8, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, n4, p6, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, n6, p4, result, weight, pixel_max);
        Asdf<mode, PixelType>::diagWeightSSE2(s5, n5, p5, result, weight, pixel_max);

        return limitPixelCorrectionSSE2<PixelType>(s5, result, limit, pixel_max);
    }

};


template <bool norow, typename PixelType>
struct DegrainSSE2<0, norow, PixelType> {

    static FORCE_INLINE __m128i degrainPixels(const PixelType *prevp, const PixelType *srcp, const PixelType *nextp, int x, int distance, const __m128i &limit, const __m128i &pixel_max) {
        LoadPixelsSSE2;

        __m128i diff = _mm_set1_epi8(255);
        __m128i min = _mm_setzero_si128();
        __m128i max = _mm_set1_epi8(255);

        checkBetterNeighboursSSE2<PixelType>(n1, p9, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(n3, p7, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(n7, p3, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(n9, p1, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(n8, p2, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(n2, p8, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(n4, p6, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(n6, p4, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(n5, p5, diff, min, max);

        checkBetterNeighboursSSE2<PixelType>(s1, s9, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(s3, s7, diff, min, max);
        checkBetterNeighboursSSE2<PixelType>(s2, s8, diff, min, max);
        if (!norow)
            checkBetterNeighboursSSE2<PixelType>(s4, s6, diff, min, max);

        __m128i result = mm_max_epu<PixelType>(min, mm_min_epu<PixelType>(s5, max));

        return limitPixelCorrectionSSE2<PixelType>(s5, result, limit, pixel_max);
    }

};


template <int mode, bool norow, typename PixelType>
static void degrainPlaneSSE2(const uint8_t *prevp8, const uint8_t *srcp8, const uint8_t *nextp8, uint8_t *dstp8, int stride, int width, int height, int limit, int interlaced, int pixel_max) {
    const PixelType *prevp = (const PixelType *)prevp8;
    const PixelType *srcp = (const PixelType *)srcp8;
    const PixelType *nextp = (const PixelType *)nextp8;
    PixelType *dstp = (PixelType *)dstp8;

    stride /= sizeof(PixelType);

    const int distance = stride << interlaced;
    const int skip_rows = 1 << interlaced;

    // Copy first line(s).
    for (int y = 0; y < skip_rows; y++) {
        memcpy(dstp, srcp, width * sizeof(PixelType));

        prevp += stride;
        srcp += stride;
        nextp += stride;
        dstp += stride;
    }

    __m128i packed_limit = mm_set1_epi<PixelType>(limit);

    // Only used in the uint16_t case.
    __m128i packed_pixel_max = _mm_set1_epi16(pixel_max);

    const int pixels_in_xmm = 16 / sizeof(PixelType);

    int width_sse2 = (width & ~(pixels_in_xmm - 1)) + 2;
    if (width_sse2 > stride)
        width_sse2 -= pixels_in_xmm;

    for (int y = skip_rows; y < height - skip_rows; y++) {
        dstp[0] = srcp[0];

        for (int x = 1; x < width_sse2 - 1; x += pixels_in_xmm)
            _mm_storeu_si128((__m128i *)&dstp[x], DegrainSSE2<mode, norow, PixelType>::degrainPixels(prevp, srcp, nextp, x, distance, packed_limit, packed_pixel_max));

        if (width + 2 > width_sse2)
            _mm_storeu_si128((__m128i *)&dstp[width - pixels_in_xmm - 1], DegrainSSE2<mode, norow, PixelType>::degrainPixels(prevp, srcp, nextp, width - pixels_in_xmm - 1, distance, packed_limit, packed_pixel_max));

        dstp[width - 1] = srcp[width - 1];

        prevp += stride;
        srcp += stride;
        nextp += stride;
        dstp += stride;
    }

    // Copy last line(s).
    for (int y = 0; y < skip_rows; y++) {
        memcpy(dstp, srcp, width * sizeof(PixelType));

        srcp += stride;
        dstp += stride;
    }
}

#endif // DEGRAINMEDIAN_X86


#define LoadPixelsScalar \
    int p1, p2, p3, \
        p4, p5, p6, \
        p7, p8, p9; \
    \
    int s1, s2, s3, \
        s4, s5, s6, \
        s7, s8, s9; \
    \
    int n1, n2, n3, \
        n4, n5, n6, \
        n7, n8, n9; \
    \
    p1 = prevp[x - distance - 1]; \
    p2 = prevp[x - distance]; \
    p3 = prevp[x - distance + 1]; \
    p4 = prevp[x - 1]; \
    p5 = prevp[x]; \
    p6 = prevp[x + 1]; \
    p7 = prevp[x + distance - 1]; \
    p8 = prevp[x + distance]; \
    p9 = prevp[x + distance + 1]; \
    \
    s1 = srcp[x - distance - 1]; \
    s2 = srcp[x - distance]; \
    s3 = srcp[x - distance + 1]; \
    s4 = srcp[x - 1]; \
    s5 = srcp[x]; \
    s6 = srcp[x + 1]; \
    s7 = srcp[x + distance - 1]; \
    s8 = srcp[x + distance]; \
    s9 = srcp[x + distance + 1]; \
    \
    n1 = nextp[x - distance - 1]; \
    n2 = nextp[x - distance]; \
    n3 = nextp[x - distance + 1]; \
    n4 = nextp[x - 1]; \
    n5 = nextp[x]; \
    n6 = nextp[x + 1]; \
    n7 = nextp[x + distance - 1]; \
    n8 = nextp[x + distance]; \
    n9 = nextp[x + distance + 1];


static FORCE_INLINE void checkBetterNeighboursScalar(int a, int b, int &diff, int &min, int &max) {
    int new_diff = std::abs(a - b);

    if (new_diff <= diff) {
        diff = new_diff;
        min = std::min(a, b);
        max = std::max(a, b);
    }
}


template <int mode>
static FORCE_INLINE void diagWeightScalar(int oldp, int bound1, int bound2, int &old_result, int &old_weight, int pixel_max) {
    // Sucks but I can't figure it out any further.

    int newp = std::max(bound1, bound2);
    int weight = std::min(bound1, bound2);

    int reg2 = std::max(0, oldp - std::max(bound1, bound2));

    newp = std::max(weight, std::min(newp, oldp));
    weight = std::max(0, weight - oldp);
    weight = std::max(weight, reg2);

    int diff = std::abs(bound1 - bound2);

    if (mode == 4)
        weight += weight;
    else if (mode == 2)
        diff += diff;
    else if (mode == 1) {
        diff += diff;
        diff += diff;
    }

    weight = std::min(weight + diff, pixel_max);

    if (weight <= old_weight) {
        old_weight = weight;
        old_result = newp;
    }
}


template <>
FORCE_INLINE void diagWeightScalar<5>(int oldp, int bound1, int bound2, int &old_result, int &old_weight, int pixel_max) {
    (void)pixel_max;

    int newp = std::max(bound1, bound2);
    int weight = std::min(bound1, bound2);
    int reg2 = std::max(0, oldp - newp);
    newp = std::min(newp, oldp);
    newp = std::max(newp, weight);
    weight = std::max(0, weight - oldp);
    weight = std::max(weight, reg2);

    if (weight <= old_weight) {
        old_weight = weight;
        old_result = newp;
    }
}


static FORCE_INLINE int limitPixelCorrectionScalar(int old_pixel, int new_pixel, int limit, int pixel_max) {
    int lower = std::max(0, old_pixel - limit);
    int upper = std::min(old_pixel + limit, pixel_max);
    return std::max(lower, std::min(new_pixel, upper));
}


template <int mode, bool norow, typename PixelType>
struct DegrainScalar {

    static FORCE_INLINE int degrainPixel(const PixelType *prevp, const PixelType *srcp, const PixelType *nextp, int x, int distance, int limit, int pixel_max) {
        LoadPixelsScalar;

        // 65535 works for any bit depth between 8 and 16.
        int result = 0;
        int weight = 65535;

        diagWeightScalar<mode>(s5, s1, s9, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, s7, s3, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, s8, s2, result, weight, pixel_max);
        if (!norow)
            diagWeightScalar<mode>(s5, s6, s4, result, weight, pixel_max);

        diagWeightScalar<mode>(s5, n1, p9, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, n3, p7, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, n7, p3, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, n9, p1, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, n8, p2, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, n2, p8, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, n4, p6, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, n6, p4, result, weight, pixel_max);
        diagWeightScalar<mode>(s5, n5, p5, result, weight, pixel_max);

        return limitPixelCorrectionScalar(s5, result, limit, pixel_max);
    }

};


template <bool norow, typename PixelType>
struct DegrainScalar<0, norow, PixelType> {

    static FORCE_INLINE int degrainPixel(const PixelType *prevp, const PixelType *srcp, const PixelType *nextp, int x, int distance, int limit, int pixel_max) {
        LoadPixelsScalar;

        // 65535 works for any bit depth between 8 and 16.
        int diff = 65535;
        int min = 0;
        int max = 65535;

        checkBetterNeighboursScalar(n1, p9, diff, min, max);
        checkBetterNeighboursScalar(n3, p7, diff, min, max);
        checkBetterNeighboursScalar(n7, p3, diff, min, max);
        checkBetterNeighboursScalar(n9, p1, diff, min, max);
        checkBetterNeighboursScalar(n8, p2, diff, min, max);
        checkBetterNeighboursScalar(n2, p8, diff, min, max);
        checkBetterNeighboursScalar(n4, p6, diff, min, max);
        checkBetterNeighboursScalar(n6, p4, diff, min, max);
        checkBetterNeighboursScalar(n5, p5, diff, min, max);

        checkBetterNeighboursScalar(s1, s9, diff, min, max);
        checkBetterNeighboursScalar(s3, s7, diff, min, max);
        checkBetterNeighboursScalar(s2, s8, diff, min, max);
        if (!norow)
            checkBetterNeighboursScalar(s4, s6, diff, min, max);

        int result = std::max(min, std::min(s5, max));

        return limitPixelCorrectionScalar(s5, result, limit, pixel_max);
    }

};


template <int mode, bool norow, typename PixelType>
static void degrainPlaneScalar(const uint8_t *prevp8, const uint8_t *srcp8, const uint8_t *nextp8, uint8_t *dstp8, int stride, int width, int height, int limit, int interlaced, int pixel_max) {
    const PixelType *prevp = (const PixelType *)prevp8;
    const PixelType *srcp = (const PixelType *)srcp8;
    const PixelType *nextp = (const PixelType *)nextp8;
    PixelType *dstp = (PixelType *)dstp8;

    stride /= sizeof(PixelType);

    const int distance = stride << interlaced;
    const int skip_rows = 1 << interlaced;

    // Copy first line(s).
    for (int y = 0; y < skip_rows; y++) {
        memcpy(dstp, srcp, width * sizeof(PixelType));

        prevp += stride;
        srcp += stride;
        nextp += stride;
        dstp += stride;
    }

    for (int y = skip_rows; y < height - skip_rows; y++) {
        dstp[0] = srcp[0];

        for (int x = 1; x < width - 1; x++)
            dstp[x] = DegrainScalar<mode, norow, PixelType>::degrainPixel(prevp, srcp, nextp, x, distance, limit, pixel_max);

        dstp[width - 1] = srcp[width - 1];
        
        prevp += stride;
        srcp += stride;
        nextp += stride;
        dstp += stride;
    }
    
    // Copy last line(s).
    for (int y = 0; y < skip_rows; y++) {
        memcpy(dstp, srcp, width * sizeof(PixelType));

        srcp += stride;
        dstp += stride;
    }
}


typedef void (*DegrainFunction) (const uint8_t *prevp, const uint8_t *srcp, const uint8_t *nextp, uint8_t *dstp, int stride, int width, int height, int limit, int interlaced, int pixel_max);


typedef struct DegrainMedianData {
    VSNodeRef *clip;
    const VSVideoInfo *vi;

    int limit[3];
    int mode[3];
    bool interlaced;
    bool norow;
    bool opt;

    DegrainFunction degrain[3];
} DegrainMedianData;


static void VS_CC degrainMedianInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    (void)in;
    (void)out;
    (void)core;

    DegrainMedianData *d = (DegrainMedianData *) * instanceData;

    vsapi->setVideoInfo(d->vi, 1, node);
}


static void selectFunctions(const VSFormat *fmt, const int *limit, const int *mode, bool norow, bool opt, DegrainFunction *f) {
    for (int plane = 0; plane < fmt->numPlanes; plane++) {
        if (limit[plane] == 0) {
            f[plane] = nullptr;
            continue;
        }

        if (fmt->bitsPerSample == 8) {
            if (norow) {
                if (mode[plane] == 0)
                    f[plane] = degrainPlaneScalar<0, true, uint8_t>;
                else if (mode[plane] == 1)
                    f[plane] = degrainPlaneScalar<1, true, uint8_t>;
                else if (mode[plane] == 2)
                    f[plane] = degrainPlaneScalar<2, true, uint8_t>;
                else if (mode[plane] == 3)
                    f[plane] = degrainPlaneScalar<3, true, uint8_t>;
                else if (mode[plane] == 4)
                    f[plane] = degrainPlaneScalar<4, true, uint8_t>;
                else if (mode[plane] == 5)
                    f[plane] = degrainPlaneScalar<5, true, uint8_t>;
            } else {
                if (mode[plane] == 0)
                    f[plane] = degrainPlaneScalar<0, false, uint8_t>;
                else if (mode[plane] == 1)
                    f[plane] = degrainPlaneScalar<1, false, uint8_t>;
                else if (mode[plane] == 2)
                    f[plane] = degrainPlaneScalar<2, false, uint8_t>;
                else if (mode[plane] == 3)
                    f[plane] = degrainPlaneScalar<3, false, uint8_t>;
                else if (mode[plane] == 4)
                    f[plane] = degrainPlaneScalar<4, false, uint8_t>;
                else if (mode[plane] == 5)
                    f[plane] = degrainPlaneScalar<5, false, uint8_t>;
            }
        } else {
            if (norow) {
                if (mode[plane] == 0)
                    f[plane] = degrainPlaneScalar<0, true, uint16_t>;
                else if (mode[plane] == 1)
                    f[plane] = degrainPlaneScalar<1, true, uint16_t>;
                else if (mode[plane] == 2)
                    f[plane] = degrainPlaneScalar<2, true, uint16_t>;
                else if (mode[plane] == 3)
                    f[plane] = degrainPlaneScalar<3, true, uint16_t>;
                else if (mode[plane] == 4)
                    f[plane] = degrainPlaneScalar<4, true, uint16_t>;
                else if (mode[plane] == 5)
                    f[plane] = degrainPlaneScalar<5, true, uint16_t>;
            } else {
                if (mode[plane] == 0)
                    f[plane] = degrainPlaneScalar<0, false, uint16_t>;
                else if (mode[plane] == 1)
                    f[plane] = degrainPlaneScalar<1, false, uint16_t>;
                else if (mode[plane] == 2)
                    f[plane] = degrainPlaneScalar<2, false, uint16_t>;
                else if (mode[plane] == 3)
                    f[plane] = degrainPlaneScalar<3, false, uint16_t>;
                else if (mode[plane] == 4)
                    f[plane] = degrainPlaneScalar<4, false, uint16_t>;
                else if (mode[plane] == 5)
                    f[plane] = degrainPlaneScalar<5, false, uint16_t>;
            }
        }

#if defined(DEGRAINMEDIAN_X86)
        if (opt) {
            if (fmt->bitsPerSample == 8) {
                if (norow) {
                    if (mode[plane] == 0)
                        f[plane] = degrainPlaneSSE2<0, true, uint8_t>;
                    else if (mode[plane] == 1)
                        f[plane] = degrainPlaneSSE2<1, true, uint8_t>;
                    else if (mode[plane] == 2)
                        f[plane] = degrainPlaneSSE2<2, true, uint8_t>;
                    else if (mode[plane] == 3)
                        f[plane] = degrainPlaneSSE2<3, true, uint8_t>;
                    else if (mode[plane] == 4)
                        f[plane] = degrainPlaneSSE2<4, true, uint8_t>;
                    else if (mode[plane] == 5)
                        f[plane] = degrainPlaneSSE2<5, true, uint8_t>;
                } else {
                    if (mode[plane] == 0)
                        f[plane] = degrainPlaneSSE2<0, false, uint8_t>;
                    else if (mode[plane] == 1)
                        f[plane] = degrainPlaneSSE2<1, false, uint8_t>;
                    else if (mode[plane] == 2)
                        f[plane] = degrainPlaneSSE2<2, false, uint8_t>;
                    else if (mode[plane] == 3)
                        f[plane] = degrainPlaneSSE2<3, false, uint8_t>;
                    else if (mode[plane] == 4)
                        f[plane] = degrainPlaneSSE2<4, false, uint8_t>;
                    else if (mode[plane] == 5)
                        f[plane] = degrainPlaneSSE2<5, false, uint8_t>;
                }
            } else {
                if (norow) {
                    if (mode[plane] == 0)
                        f[plane] = degrainPlaneSSE2<0, true, uint16_t>;
                    else if (mode[plane] == 1)
                        f[plane] = degrainPlaneSSE2<1, true, uint16_t>;
                    else if (mode[plane] == 2)
                        f[plane] = degrainPlaneSSE2<2, true, uint16_t>;
                    else if (mode[plane] == 3)
                        f[plane] = degrainPlaneSSE2<3, true, uint16_t>;
                    else if (mode[plane] == 4)
                        f[plane] = degrainPlaneSSE2<4, true, uint16_t>;
                    else if (mode[plane] == 5)
                        f[plane] = degrainPlaneSSE2<5, true, uint16_t>;
                } else {
                    if (mode[plane] == 0)
                        f[plane] = degrainPlaneSSE2<0, false, uint16_t>;
                    else if (mode[plane] == 1)
                        f[plane] = degrainPlaneSSE2<1, false, uint16_t>;
                    else if (mode[plane] == 2)
                        f[plane] = degrainPlaneSSE2<2, false, uint16_t>;
                    else if (mode[plane] == 3)
                        f[plane] = degrainPlaneSSE2<3, false, uint16_t>;
                    else if (mode[plane] == 4)
                        f[plane] = degrainPlaneSSE2<4, false, uint16_t>;
                    else if (mode[plane] == 5)
                        f[plane] = degrainPlaneSSE2<5, false, uint16_t>;
                }
            }
        }
#endif // DEGRAINMEDIAN_X86
    }
}


static const VSFrameRef *VS_CC degrainMedianGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    (void)frameData;

    const DegrainMedianData *d = (const DegrainMedianData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(std::max(0, n - 1), d->clip, frameCtx);
        vsapi->requestFrameFilter(n, d->clip, frameCtx);
        vsapi->requestFrameFilter(std::min(n + 1, d->vi->numFrames - 1), d->clip, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        if (n == 0 || n == d->vi->numFrames - 1)
            return vsapi->getFrameFilter(n, d->clip, frameCtx);

        const VSFrameRef *prev = vsapi->getFrameFilter(n - 1, d->clip, frameCtx);
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->clip, frameCtx);
        const VSFrameRef *next = vsapi->getFrameFilter(n + 1, d->clip, frameCtx);

        const VSFormat *prev_fmt = vsapi->getFrameFormat(prev);
        const VSFormat *src_fmt = vsapi->getFrameFormat(src);
        const VSFormat *next_fmt = vsapi->getFrameFormat(next);

        if (prev_fmt != src_fmt || next_fmt != src_fmt ||
            vsapi->getFrameWidth(prev, 0) != vsapi->getFrameWidth(src, 0) ||
            vsapi->getFrameWidth(next, 0) != vsapi->getFrameWidth(src, 0) ||
            vsapi->getFrameHeight(prev, 0) != vsapi->getFrameHeight(src, 0) ||
            vsapi->getFrameHeight(next, 0) != vsapi->getFrameHeight(src, 0) ||
            src_fmt->sampleType != stInteger ||
            src_fmt->bitsPerSample > 16) {
            vsapi->freeFrame(prev);
            vsapi->freeFrame(next);
            return src;
        }


        DegrainFunction degrain[3] = { d->degrain[0], d->degrain[1], d->degrain[2] };
        if (!d->vi->format)
            selectFunctions(src_fmt, d->limit, d->mode, d->norow, d->opt, degrain);


        const VSFrameRef *frames[3] = {
            d->limit[0] == 0 ? src : nullptr,
            d->limit[1] == 0 ? src : nullptr,
            d->limit[2] == 0 ? src : nullptr
        };
        int planes[3] = { 0, 1, 2 };

        VSFrameRef *dst = vsapi->newVideoFrame2(src_fmt, vsapi->getFrameWidth(src, 0), vsapi->getFrameHeight(src, 0), frames, planes, src, core);


        int pixel_max = (1 << src_fmt->bitsPerSample) - 1;

        for (int plane = 0; plane < src_fmt->numPlanes; plane++) {
            if (d->limit[plane] == 0)
                continue;

            const uint8_t *prevp = vsapi->getReadPtr(prev, plane);
            const uint8_t *srcp = vsapi->getReadPtr(src, plane);
            const uint8_t *nextp = vsapi->getReadPtr(next, plane);
            uint8_t *dstp = vsapi->getWritePtr(dst, plane);

            int stride = vsapi->getStride(src, plane);
            int width = vsapi->getFrameWidth(src, plane);
            int height = vsapi->getFrameHeight(src, plane);

            int limit = pixel_max * d->limit[plane] / 255;

            degrain[plane](prevp, srcp, nextp, dstp, stride, width, height, limit, d->interlaced, pixel_max);
        }


        vsapi->freeFrame(prev);
        vsapi->freeFrame(src);
        vsapi->freeFrame(next);

        return dst;
    }

    return NULL;
}


static void VS_CC degrainMedianFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    (void)core;

    DegrainMedianData *d = (DegrainMedianData *)instanceData;

    vsapi->freeNode(d->clip);
    free(d);
}


static void VS_CC degrainMedianCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    (void)userData;

    DegrainMedianData d;
    DegrainMedianData *data;

    int err;

    d.interlaced = !!vsapi->propGetInt(in, "interlaced", 0, &err);

    d.norow = !!vsapi->propGetInt(in, "norow", 0, &err);

    d.opt = !!vsapi->propGetInt(in, "opt", 0, &err);
    if (err)
        d.opt = true;

    d.clip = vsapi->propGetNode(in, "clip", 0, NULL);
    d.vi = vsapi->getVideoInfo(d.clip);

    if (d.vi->format && (d.vi->format->sampleType != stInteger || d.vi->format->bitsPerSample > 16)) {
        vsapi->setError(out, "DegrainMedian: only 8..16 bit clips are supported.");
        vsapi->freeNode(d.clip);
        return;
    }


#define ERROR_SIZE 128
    char msg[ERROR_SIZE + 1] = { 0 };

    int num_limits = vsapi->propNumElements(in, "limit");
    if (d.vi->format && num_limits > d.vi->format->numPlanes) {
        vsapi->setError(out, "DegrainMedian: limit has more elements than there are planes.");
        vsapi->freeNode(d.clip);
        return;
    }

    if (num_limits == -1) {
        d.limit[0] = d.limit[1] = d.limit[2] = 4;
    } else {
        for (int i = 0; i < 3; i++) {
            d.limit[i] = int64ToIntS(vsapi->propGetInt(in, "limit", i, &err));
            if (err) {
                d.limit[i] = d.limit[i - 1];
            } else {
                if (d.limit[i] < 0 || d.limit[i] > 255) {
                    snprintf(msg, ERROR_SIZE, "DegrainMedian: limit[%d] must be between 0 and 255 (inclusive).", i);
                    vsapi->setError(out, msg);
                    vsapi->freeNode(d.clip);
                    return;
                }
            }
        }
    }


    int num_modes = vsapi->propNumElements(in, "mode");
    if (d.vi->format && num_modes > d.vi->format->numPlanes) {
        vsapi->setError(out, "DegrainMedian: mode has more elements than there are planes.");
        vsapi->freeNode(d.clip);
        return;
    }

    if (num_modes == -1) {
        d.mode[0] = d.mode[1] = d.mode[2] = 1;
    } else {
        for (int i = 0; i < 3; i++) {
            d.mode[i] = int64ToIntS(vsapi->propGetInt(in, "mode", i, &err));
            if (err) {
                d.mode[i] = d.mode[i - 1];
            } else {
                if (d.mode[i] < 0 || d.mode[i] > 5) {
                    snprintf(msg, ERROR_SIZE, "DegrainMedian: mode[%d] must be between 0 and 5 (inclusive).", i);
#undef ERROR_SIZE
                    vsapi->setError(out, msg);
                    vsapi->freeNode(d.clip);
                    return;
                }
            }
        }
    }


    if (d.vi->format)
        selectFunctions(d.vi->format, d.limit, d.mode, d.norow, d.opt, d.degrain);


    data = (DegrainMedianData *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "DegrainMedian", degrainMedianInit, degrainMedianGetFrame, degrainMedianFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.degrainmedian", "dgm", "Spatio-temporal limited median denoiser", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("DegrainMedian",
                 "clip:clip;"
                 "limit:int[]:opt;"
                 "mode:int[]:opt;"
                 "interlaced:int:opt;"
                 "norow:int:opt;"
                 "opt:int:opt;"
                 , degrainMedianCreate, 0, plugin);
}
