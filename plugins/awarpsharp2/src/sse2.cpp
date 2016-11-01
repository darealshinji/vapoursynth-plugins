#include <algorithm>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>


#ifdef _WIN32
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif


template <typename PixelType>
static FORCE_INLINE __m128i mm_avg_epu(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_avg_epu8(a, b);
    else
        return _mm_avg_epu16(a, b);
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_subs_epu(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_subs_epu8(a, b);
    else
        return _mm_subs_epu16(a, b);
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_adds_epu(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_adds_epu8(a, b);
    else
        return _mm_adds_epu16(a, b);
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_max_epu(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_max_epu8(a, b);
    else {
        __m128i word_32768 = _mm_set1_epi16(32768);

        __m128i a_minus = _mm_sub_epi16(a, word_32768);
        __m128i b_minus = _mm_sub_epi16(b, word_32768);

        return _mm_add_epi16(_mm_max_epi16(a_minus, b_minus), word_32768);
    }
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_min_epu(const __m128i &a, const __m128i &b) {
    if (sizeof(PixelType) == 1)
        return _mm_min_epu8(a, b);
    else {
        __m128i word_32768 = _mm_set1_epi16(32768);

        __m128i a_minus = _mm_sub_epi16(a, word_32768);
        __m128i b_minus = _mm_sub_epi16(b, word_32768);

        return _mm_add_epi16(_mm_min_epi16(a_minus, b_minus), word_32768);
    }
}


template <typename PixelType>
static FORCE_INLINE __m128i mm_set1_epi(PixelType a) {
    if (sizeof(PixelType) == 1)
        return _mm_set1_epi8(a);
    else
        return _mm_set1_epi16(a);
}


template <typename PixelType>
static FORCE_INLINE void sobel_xmmword_sse2(const PixelType *srcp, PixelType *dstp, int stride, const __m128i &th, const __m128i &pixel_max) {
    __m128i a11, a21, a31,
            a12,      a32,
            a13, a23, a33;

    a11 = _mm_loadu_si128((const __m128i *)(srcp - stride - 1));
    a21 = _mm_loadu_si128((const __m128i *)(srcp - stride));
    a31 = _mm_loadu_si128((const __m128i *)(srcp - stride + 1));

    a12 = _mm_loadu_si128((const __m128i *)(srcp - 1));
    a32 = _mm_loadu_si128((const __m128i *)(srcp + 1));

    a13 = _mm_loadu_si128((const __m128i *)(srcp + stride - 1));
    a23 = _mm_loadu_si128((const __m128i *)(srcp + stride));
    a33 = _mm_loadu_si128((const __m128i *)(srcp + stride + 1));

    __m128i avg_up    = mm_avg_epu<PixelType>(a21, mm_avg_epu<PixelType>(a11, a31));
    __m128i avg_down  = mm_avg_epu<PixelType>(a23, mm_avg_epu<PixelType>(a13, a33));
    __m128i avg_left  = mm_avg_epu<PixelType>(a12, mm_avg_epu<PixelType>(a13, a11));
    __m128i avg_right = mm_avg_epu<PixelType>(a32, mm_avg_epu<PixelType>(a33, a31));

    __m128i abs_v = _mm_or_si128(mm_subs_epu<PixelType>(avg_up, avg_down), mm_subs_epu<PixelType>(avg_down, avg_up));
    __m128i abs_h = _mm_or_si128(mm_subs_epu<PixelType>(avg_left, avg_right), mm_subs_epu<PixelType>(avg_right, avg_left));

    __m128i absolute = mm_adds_epu<PixelType>(abs_v, abs_h);
    if (sizeof(PixelType) == 2)
        absolute = mm_min_epu<PixelType>(absolute, pixel_max);

    __m128i abs_max = mm_max_epu<PixelType>(abs_h, abs_v);

    absolute = mm_adds_epu<PixelType>(absolute, abs_max);
    if (sizeof(PixelType) == 2)
        absolute = mm_min_epu<PixelType>(absolute, pixel_max);

    __m128i absolute2 = mm_adds_epu<PixelType>(absolute, absolute);
    if (sizeof(PixelType) == 2)
        absolute2 = mm_min_epu<PixelType>(absolute2, pixel_max);

    absolute = mm_adds_epu<PixelType>(absolute2, absolute);
    if (sizeof(PixelType) == 2)
        absolute = mm_min_epu<PixelType>(absolute, pixel_max);

    absolute = mm_adds_epu<PixelType>(absolute, absolute);
    if (sizeof(PixelType) == 2)
        absolute = mm_min_epu<PixelType>(absolute, pixel_max);

    _mm_storeu_si128((__m128i *)(dstp), mm_min_epu<PixelType>(absolute, th));
}


template <typename PixelType>
static void sobel_sse2(const uint8_t *srcp8, uint8_t *dstp8, int stride, int width, int height, int thresh, int bits_per_sample) {
    const PixelType *srcp = (const PixelType *)srcp8;
    PixelType *dstp = (PixelType *)dstp8;

    stride /= sizeof(PixelType);

    __m128i pixel_max = _mm_set1_epi16((1 << bits_per_sample) - 1);

    const int pixels_in_xmm = 16 / sizeof(PixelType);

    PixelType *dstp_orig = dstp;

    srcp += stride;
    dstp += stride;

    __m128i th = mm_set1_epi<PixelType>(thresh);

    int width_sse2 = (width & ~(pixels_in_xmm - 1)) + 2;
    if (width_sse2 > stride)
        width_sse2 -= pixels_in_xmm;

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width_sse2 - 1; x += pixels_in_xmm)
            sobel_xmmword_sse2<PixelType>(srcp + x, dstp + x, stride, th, pixel_max);

        if (width + 2 > width_sse2)
            sobel_xmmword_sse2<PixelType>(srcp + width - pixels_in_xmm - 1, dstp + width - pixels_in_xmm - 1, stride, th, pixel_max);

        dstp[0] = dstp[1];
        dstp[width - 1] = dstp[width - 2];

        srcp += stride;
        dstp += stride;
    }

    memcpy(dstp_orig, dstp_orig + stride, width * sizeof(PixelType));
    memcpy(dstp, dstp - stride, width * sizeof(PixelType));
}


template <typename PixelType>
static FORCE_INLINE void blur_r6_h_left_sse2(const PixelType *srcp, PixelType *dstp) {
    __m128i avg12 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp + 1)), _mm_loadu_si128((const __m128i *)(srcp + 2)));
    __m128i avg34 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp + 3)), _mm_loadu_si128((const __m128i *)(srcp + 4)));
    __m128i avg56 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp + 5)), _mm_loadu_si128((const __m128i *)(srcp + 6)));

    __m128i avg012 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp)), avg12);
    __m128i avg3456 = mm_avg_epu<PixelType>(avg34, avg56);
    __m128i avg0123456 = mm_avg_epu<PixelType>(avg012, avg3456);
    __m128i avg = mm_avg_epu<PixelType>(avg012, avg0123456);

    _mm_storeu_si128((__m128i *)(dstp), avg);
}


template <typename PixelType>
static FORCE_INLINE void blur_r6_h_middle_sse2(const PixelType *srcp, PixelType *dstp) {
    __m128i avg11 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 1)), _mm_loadu_si128((const __m128i *)(srcp + 1)));
    __m128i avg22 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 2)), _mm_loadu_si128((const __m128i *)(srcp + 2)));
    __m128i avg33 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 3)), _mm_loadu_si128((const __m128i *)(srcp + 3)));
    __m128i avg44 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 4)), _mm_loadu_si128((const __m128i *)(srcp + 4)));
    __m128i avg55 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 5)), _mm_loadu_si128((const __m128i *)(srcp + 5)));
    __m128i avg66 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 6)), _mm_loadu_si128((const __m128i *)(srcp + 6)));

    __m128i avg12 = mm_avg_epu<PixelType>(avg11, avg22);
    __m128i avg34 = mm_avg_epu<PixelType>(avg33, avg44);
    __m128i avg56 = mm_avg_epu<PixelType>(avg55, avg66);
    __m128i avg012 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp)), avg12);
    __m128i avg3456 = mm_avg_epu<PixelType>(avg34, avg56);
    __m128i avg0123456 = mm_avg_epu<PixelType>(avg012, avg3456);
    __m128i avg = mm_avg_epu<PixelType>(avg012, avg0123456);

    _mm_storeu_si128((__m128i *)(dstp), avg);
}


template <typename PixelType>
static FORCE_INLINE void blur_r6_h_right_sse2(const PixelType *srcp, PixelType *dstp) {
    __m128i avg12 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 1)), _mm_loadu_si128((const __m128i *)(srcp - 2)));
    __m128i avg34 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 3)), _mm_loadu_si128((const __m128i *)(srcp - 4)));
    __m128i avg56 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 5)), _mm_loadu_si128((const __m128i *)(srcp - 6)));

    __m128i avg012 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp)), avg12);
    __m128i avg3456 = mm_avg_epu<PixelType>(avg34, avg56);
    __m128i avg0123456 = mm_avg_epu<PixelType>(avg012, avg3456);
    __m128i avg = mm_avg_epu<PixelType>(avg012, avg0123456);

    // This is the right edge. Only the highest six pixels are needed.
    if (sizeof(PixelType) == 1) {
        int extra_bytes = *(int16_t *)(dstp + 8);
        avg = _mm_insert_epi16(avg, extra_bytes, 4);
        _mm_storeh_pi((__m64 *)(dstp + 8), _mm_castsi128_ps(avg));
    } else {
        int extra_bytes = dstp[0];
        avg = _mm_insert_epi16(avg, extra_bytes, 0);
        extra_bytes = dstp[1];
        avg = _mm_insert_epi16(avg, extra_bytes, 1);
        _mm_storeu_si128((__m128i *)(dstp), avg);
    }
}


template <typename PixelType>
static FORCE_INLINE void blur_r6_v_top_sse2(const PixelType *srcp, PixelType *dstp, int stride) {
    __m128i l0 = _mm_loadu_si128((const __m128i *)(srcp));
    __m128i l1 = _mm_loadu_si128((const __m128i *)(srcp + stride));
    __m128i l2 = _mm_loadu_si128((const __m128i *)(srcp + stride * 2));
    __m128i l3 = _mm_loadu_si128((const __m128i *)(srcp + stride * 3));
    __m128i l4 = _mm_loadu_si128((const __m128i *)(srcp + stride * 4));
    __m128i l5 = _mm_loadu_si128((const __m128i *)(srcp + stride * 5));
    __m128i l6 = _mm_loadu_si128((const __m128i *)(srcp + stride * 6));

    __m128i avg12 = mm_avg_epu<PixelType>(l1, l2);
    __m128i avg34 = mm_avg_epu<PixelType>(l3, l4);
    __m128i avg56 = mm_avg_epu<PixelType>(l5, l6);

    __m128i avg3456 = mm_avg_epu<PixelType>(avg34, avg56);
    __m128i avg012 = mm_avg_epu<PixelType>(l0, avg12);
    __m128i avg0123456 = mm_avg_epu<PixelType>(avg012, avg3456);
    __m128i avg = mm_avg_epu<PixelType>(avg012, avg0123456);

    _mm_storeu_si128((__m128i *)(dstp), avg);
}


template <typename PixelType>
static FORCE_INLINE void blur_r6_v_middle_sse2(const PixelType *srcp, PixelType *dstp, int stride) {
    __m128i m6 = _mm_loadu_si128((const __m128i *)(srcp - stride * 6));
    __m128i m5 = _mm_loadu_si128((const __m128i *)(srcp - stride * 5));
    __m128i m4 = _mm_loadu_si128((const __m128i *)(srcp - stride * 4));
    __m128i m3 = _mm_loadu_si128((const __m128i *)(srcp - stride * 3));
    __m128i m2 = _mm_loadu_si128((const __m128i *)(srcp - stride * 2));
    __m128i m1 = _mm_loadu_si128((const __m128i *)(srcp - stride));
    __m128i l0 = _mm_loadu_si128((const __m128i *)(srcp));
    __m128i l1 = _mm_loadu_si128((const __m128i *)(srcp + stride));
    __m128i l2 = _mm_loadu_si128((const __m128i *)(srcp + stride * 2));
    __m128i l3 = _mm_loadu_si128((const __m128i *)(srcp + stride * 3));
    __m128i l4 = _mm_loadu_si128((const __m128i *)(srcp + stride * 4));
    __m128i l5 = _mm_loadu_si128((const __m128i *)(srcp + stride * 5));
    __m128i l6 = _mm_loadu_si128((const __m128i *)(srcp + stride * 6));

    __m128i avg11 = mm_avg_epu<PixelType>(m1, l1);
    __m128i avg22 = mm_avg_epu<PixelType>(m2, l2);
    __m128i avg33 = mm_avg_epu<PixelType>(m3, l3);
    __m128i avg44 = mm_avg_epu<PixelType>(m4, l4);
    __m128i avg55 = mm_avg_epu<PixelType>(m5, l5);
    __m128i avg66 = mm_avg_epu<PixelType>(m6, l6);

    __m128i avg12 = mm_avg_epu<PixelType>(avg11, avg22);
    __m128i avg34 = mm_avg_epu<PixelType>(avg33, avg44);
    __m128i avg56 = mm_avg_epu<PixelType>(avg55, avg66);
    __m128i avg012 = mm_avg_epu<PixelType>(l0, avg12);
    __m128i avg3456 = mm_avg_epu<PixelType>(avg34, avg56);
    __m128i avg0123456 = mm_avg_epu<PixelType>(avg012, avg3456);
    __m128i avg = mm_avg_epu<PixelType>(avg012, avg0123456);

    _mm_storeu_si128((__m128i *)(dstp), avg);
}


template <typename PixelType>
static FORCE_INLINE void blur_r6_v_bottom_sse2(const PixelType *srcp, PixelType *dstp, int stride) {
    __m128i m6 = _mm_loadu_si128((const __m128i *)(srcp - stride * 6));
    __m128i m5 = _mm_loadu_si128((const __m128i *)(srcp - stride * 5));
    __m128i m4 = _mm_loadu_si128((const __m128i *)(srcp - stride * 4));
    __m128i m3 = _mm_loadu_si128((const __m128i *)(srcp - stride * 3));
    __m128i m2 = _mm_loadu_si128((const __m128i *)(srcp - stride * 2));
    __m128i m1 = _mm_loadu_si128((const __m128i *)(srcp - stride));
    __m128i l0 = _mm_loadu_si128((const __m128i *)(srcp));

    __m128i avg12 = mm_avg_epu<PixelType>(m1, m2);
    __m128i avg34 = mm_avg_epu<PixelType>(m3, m4);
    __m128i avg56 = mm_avg_epu<PixelType>(m5, m6);
    __m128i avg012 = mm_avg_epu<PixelType>(l0, avg12);
    __m128i avg3456 = mm_avg_epu<PixelType>(avg34, avg56);
    __m128i avg0123456 = mm_avg_epu<PixelType>(avg012, avg3456);
    __m128i avg = mm_avg_epu<PixelType>(avg012, avg0123456);

    _mm_storeu_si128((__m128i *)(dstp), avg);
}


template <typename PixelType>
static void blur_r6_sse2(uint8_t *mask8, uint8_t *temp8, int stride, int width, int height) {
    // Horizontal blur from mask to temp.
    // Vertical blur from temp back to mask.

    PixelType *mask = (PixelType *)mask8;
    PixelType *temp = (PixelType *)temp8;

    stride /= sizeof(PixelType);

    const int pixels_in_xmm = 16 / sizeof(PixelType);

    int width_sse2 = (width & ~(pixels_in_xmm - 1)) + 12;
    if (width_sse2 > stride)
        width_sse2 -= pixels_in_xmm;

    PixelType *mask_orig = mask;
    PixelType *temp_orig = temp;

    // Horizontal blur.

    for (int y = 0; y < height; y++) {
        blur_r6_h_left_sse2<PixelType>(mask, temp);

        for (int x = 6; x < width_sse2 - 6; x += pixels_in_xmm)
            blur_r6_h_middle_sse2<PixelType>(mask + x, temp + x);

        if (width + 12 > width_sse2)
            blur_r6_h_middle_sse2<PixelType>(mask + width - pixels_in_xmm - 6, temp + width - pixels_in_xmm - 6);

        blur_r6_h_right_sse2<PixelType>(mask + width - pixels_in_xmm, temp + width - pixels_in_xmm);

        mask += stride;
        temp += stride;
    }


    // Vertical blur.

    width_sse2 = width & ~(pixels_in_xmm - 1);

    mask = mask_orig;
    temp = temp_orig;
    int y;

    for (y = 0; y < 6; y++) {
        for (int x = 0; x < width_sse2; x += pixels_in_xmm)
            blur_r6_v_top_sse2<PixelType>(temp + x, mask + x, stride);

        if (width > width_sse2)
            blur_r6_v_top_sse2<PixelType>(temp + width - pixels_in_xmm, mask + width - pixels_in_xmm, stride);

        mask += stride;
        temp += stride;
    }

    for ( ; y < height - 6; y++) {
        for (int x = 0; x < width_sse2; x += pixels_in_xmm)
            blur_r6_v_middle_sse2<PixelType>(temp + x, mask + x, stride);

        if (width > width_sse2)
            blur_r6_v_middle_sse2<PixelType>(temp + width - pixels_in_xmm, mask + width - pixels_in_xmm, stride);

        mask += stride;
        temp += stride;
    }

    for ( ; y < height; y++) {
        for (int x = 0; x < width_sse2; x += pixels_in_xmm)
            blur_r6_v_bottom_sse2<PixelType>(temp + x, mask + x, stride);

        if (width > width_sse2)
            blur_r6_v_bottom_sse2<PixelType>(temp + width - pixels_in_xmm, mask + width - pixels_in_xmm, stride);

        mask += stride;
        temp += stride;
    }
}


template <typename PixelType>
static FORCE_INLINE void blur_r2_h_sse2(const PixelType *srcp, PixelType *dstp) {
    __m128i avg1 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 1)), _mm_loadu_si128((const __m128i *)(srcp + 1)));
    __m128i avg2 = mm_avg_epu<PixelType>(_mm_loadu_si128((const __m128i *)(srcp - 2)), _mm_loadu_si128((const __m128i *)(srcp + 2)));
    __m128i middle = _mm_loadu_si128((const __m128i *)(srcp));
    __m128i avg = mm_avg_epu<PixelType>(avg2, middle);
    avg = mm_avg_epu<PixelType>(avg, middle);
    avg = mm_avg_epu<PixelType>(avg, avg1);

    _mm_storeu_si128((__m128i *)(dstp), avg);
}


template <typename PixelType>
static FORCE_INLINE void blur_r2_v_sse2(const PixelType *srcp, PixelType *dstp, int stride_p2, int stride_p1, int stride_n1, int stride_n2) {
    __m128i m2 = _mm_loadu_si128((const __m128i *)(srcp + stride_p2));
    __m128i m1 = _mm_loadu_si128((const __m128i *)(srcp + stride_p1));
    __m128i l0 = _mm_loadu_si128((const __m128i *)(srcp));
    __m128i l1 = _mm_loadu_si128((const __m128i *)(srcp + stride_n1));
    __m128i l2 = _mm_loadu_si128((const __m128i *)(srcp + stride_n2));

    __m128i avg1 = mm_avg_epu<PixelType>(m1, l1);
    __m128i avg2 = mm_avg_epu<PixelType>(m2, l2);
    __m128i avg = mm_avg_epu<PixelType>(avg2, l0);
    avg = mm_avg_epu<PixelType>(avg, l0);
    avg = mm_avg_epu<PixelType>(avg, avg1);

    _mm_storeu_si128((__m128i *)(dstp), avg);
}


template <typename PixelType>
static void blur_r2_sse2(uint8_t *mask8, uint8_t *temp8, int stride, int width, int height) {
    // Horizontal blur from mask to temp.
    // Vertical blur from temp back to mask.

    PixelType *mask = (PixelType *)mask8;
    PixelType *temp = (PixelType *)temp8;

    stride /= sizeof(PixelType);

    const int pixels_in_xmm = 16 / sizeof(PixelType);

    int width_sse2 = (width & ~(pixels_in_xmm - 1)) + 4;
    if (width_sse2 > stride)
        width_sse2 -= pixels_in_xmm;

    PixelType *mask_orig = mask;
    PixelType *temp_orig = temp;

    // Horizontal blur.

    for (int y = 0; y < height; y++) {
        int avg, avg1, avg2;

        avg1 = (mask[0] + mask[1] + 1) >> 1;
        avg2 = (mask[0] + mask[2] + 1) >> 1;
        avg = (avg2 + mask[0] + 1) >> 1;
        avg = (avg + mask[0] + 1) >> 1;
        avg = (avg + avg1 + 1) >> 1;

        temp[0] = avg;

        avg1 = (mask[0] + mask[2] + 1) >> 1;
        avg2 = (mask[0] + mask[3] + 1) >> 1;
        avg = (avg2 + mask[1] + 1) >> 1;
        avg = (avg + mask[1] + 1) >> 1;
        avg = (avg + avg1 + 1) >> 1;

        temp[1] = avg;

        for (int x = 2; x < width_sse2 - 2; x += pixels_in_xmm)
            blur_r2_h_sse2<PixelType>(mask + x, temp + x);

        if (width + 4 > width_sse2)
            blur_r2_h_sse2<PixelType>(mask + width - pixels_in_xmm - 2, temp + width - pixels_in_xmm - 2);

        avg1 = (mask[width - 3] + mask[width - 1] + 1) >> 1;
        avg2 = (mask[width - 4] + mask[width - 1] + 1) >> 1;
        avg = (avg2 + mask[width - 2] + 1) >> 1;
        avg = (avg + mask[width - 2] + 1) >> 1;
        avg = (avg + avg1 + 1) >> 1;

        temp[width - 2] = avg;

        avg1 = (mask[width - 2] + mask[width - 1] + 1) >> 1;
        avg2 = (mask[width - 3] + mask[width - 1] + 1) >> 1;
        avg = (avg2 + mask[width - 1] + 1) >> 1;
        avg = (avg + mask[width - 1] + 1) >> 1;
        avg = (avg + avg1 + 1) >> 1;

        temp[width - 1] = avg;

        mask += stride;
        temp += stride;
    }


    // Vertical blur.

    width_sse2 = width & ~(pixels_in_xmm - 1);

    mask = mask_orig;
    temp = temp_orig;

    for (int y = 0; y < height; y++) {
        int stride_p1 = y ? -stride : 0;
        int stride_p2 = y > 1 ? stride_p1 * 2 : stride_p1;
        int stride_n1 = y < height - 1 ? stride : 0;
        int stride_n2 = y < height - 2 ? stride_n1 * 2 : stride_n1;

        for (int x = 0; x < width_sse2; x += pixels_in_xmm)
            blur_r2_v_sse2<PixelType>(temp + x, mask + x, stride_p2, stride_p1, stride_n1, stride_n2);

        if (width > width_sse2)
            blur_r2_v_sse2<PixelType>(temp + width - pixels_in_xmm, mask + width - pixels_in_xmm, stride_p2, stride_p1, stride_n1, stride_n2);

        mask += stride;
        temp += stride;
    }
}


template <int SMAGL>
static FORCE_INLINE void warp_mmword_u8_sse2(const uint8_t *srcp, const uint8_t *edgep, uint8_t *dstp, int src_stride, int edge_stride, int height, int x, int y, const __m128i &depth, const __m128i &zero, const __m128i &x_limit_min, const __m128i &x_limit_max, const __m128i &y_limit_min, const __m128i &y_limit_max, const __m128i &word_64, const __m128i &word_127, const __m128i &word_128, const __m128i &word_255, const __m128i &one_stride) {
    int SMAG = 1 << SMAGL;

    // calculate displacement

    __m128i above = _mm_loadl_epi64((const __m128i *)(edgep + x - (y ? edge_stride : 0)));
    __m128i below = _mm_loadl_epi64((const __m128i *)(edgep + x + (y < height - 1 ? edge_stride : 0)));

    __m128i left = _mm_loadl_epi64((const __m128i *)(edgep + x - 1));
    __m128i right = _mm_loadl_epi64((const __m128i *)(edgep + x + 1));

    above = _mm_unpacklo_epi8(above, zero);
    below = _mm_unpacklo_epi8(below, zero);
    left = _mm_unpacklo_epi8(left, zero);
    right = _mm_unpacklo_epi8(right, zero);

    __m128i h = _mm_sub_epi16(left, right);
    __m128i v = _mm_sub_epi16(above, below);

    h = _mm_slli_epi16(h, 7);
    v = _mm_slli_epi16(v, 7);

    h = _mm_mulhi_epi16(h, depth);
    v = _mm_mulhi_epi16(v, depth);

    v = _mm_max_epi16(v, y_limit_min);
    v = _mm_min_epi16(v, y_limit_max);

    __m128i remainder_h = h;
    __m128i remainder_v = v;

    if (SMAGL) {
        remainder_h = _mm_slli_epi16(remainder_h, SMAGL);
        remainder_v = _mm_slli_epi16(remainder_v, SMAGL);
    }

    remainder_h = _mm_and_si128(remainder_h, word_127);
    remainder_v = _mm_and_si128(remainder_v, word_127);

    h = _mm_srai_epi16(h, 7 - SMAGL);
    v = _mm_srai_epi16(v, 7 - SMAGL);

    __m128i xx = _mm_set1_epi32(x << SMAGL);
    xx = _mm_packs_epi32(xx, xx);

    h = _mm_adds_epi16(h, xx);

    remainder_h = _mm_and_si128(remainder_h, _mm_cmpgt_epi16(x_limit_max, h));
    remainder_h = _mm_andnot_si128(_mm_cmpgt_epi16(x_limit_min, h), remainder_h);

    h = _mm_max_epi16(h, x_limit_min);
    h = _mm_min_epi16(h, x_limit_max);

    // h and v contain the displacement now.

    __m128i disp_lo = _mm_unpacklo_epi16(v, h);
    __m128i disp_hi = _mm_unpackhi_epi16(v, h);
    disp_lo = _mm_madd_epi16(disp_lo, one_stride);
    disp_hi = _mm_madd_epi16(disp_hi, one_stride);

    __m128i line0 = _mm_setzero_si128();
    __m128i line1 = _mm_setzero_si128();

    int offset = _mm_cvtsi128_si32(disp_lo);
    disp_lo = _mm_srli_si128(disp_lo, 4);
    line0 = _mm_insert_epi16(line0, *(int16_t *)(srcp + offset), 0);
    line1 = _mm_insert_epi16(line1, *(int16_t *)(srcp + offset + src_stride), 0);

    offset = _mm_cvtsi128_si32(disp_lo);
    disp_lo = _mm_srli_si128(disp_lo, 4);
    line0 = _mm_insert_epi16(line0, *(int16_t *)(srcp + offset + 1 * SMAG), 1);
    line1 = _mm_insert_epi16(line1, *(int16_t *)(srcp + offset + src_stride + 1 * SMAG), 1);

    offset = _mm_cvtsi128_si32(disp_lo);
    disp_lo = _mm_srli_si128(disp_lo, 4);
    line0 = _mm_insert_epi16(line0, *(int16_t *)(srcp + offset + 2 * SMAG), 2);
    line1 = _mm_insert_epi16(line1, *(int16_t *)(srcp + offset + src_stride + 2 * SMAG), 2);

    offset = _mm_cvtsi128_si32(disp_lo);
    disp_lo = _mm_srli_si128(disp_lo, 4);
    line0 = _mm_insert_epi16(line0, *(int16_t *)(srcp + offset + 3 * SMAG), 3);
    line1 = _mm_insert_epi16(line1, *(int16_t *)(srcp + offset + src_stride + 3 * SMAG), 3);

    offset = _mm_cvtsi128_si32(disp_hi);
    disp_hi = _mm_srli_si128(disp_hi, 4);
    line0 = _mm_insert_epi16(line0, *(int16_t *)(srcp + offset + 4 * SMAG), 4);
    line1 = _mm_insert_epi16(line1, *(int16_t *)(srcp + offset + src_stride + 4 * SMAG), 4);

    offset = _mm_cvtsi128_si32(disp_hi);
    disp_hi = _mm_srli_si128(disp_hi, 4);
    line0 = _mm_insert_epi16(line0, *(int16_t *)(srcp + offset + 5 * SMAG), 5);
    line1 = _mm_insert_epi16(line1, *(int16_t *)(srcp + offset + src_stride + 5 * SMAG), 5);

    offset = _mm_cvtsi128_si32(disp_hi);
    disp_hi = _mm_srli_si128(disp_hi, 4);
    line0 = _mm_insert_epi16(line0, *(int16_t *)(srcp + offset + 6 * SMAG), 6);
    line1 = _mm_insert_epi16(line1, *(int16_t *)(srcp + offset + src_stride + 6 * SMAG), 6);

    offset = _mm_cvtsi128_si32(disp_hi);
    disp_hi = _mm_srli_si128(disp_hi, 4);
    line0 = _mm_insert_epi16(line0, *(int16_t *)(srcp + offset + 7 * SMAG), 7);
    line1 = _mm_insert_epi16(line1, *(int16_t *)(srcp + offset + src_stride + 7 * SMAG), 7);

    __m128i left0 = _mm_and_si128(line0, word_255);
    __m128i left1 = _mm_and_si128(line1, word_255);

    __m128i right0 = _mm_srli_epi16(line0, 8);
    __m128i right1 = _mm_srli_epi16(line1, 8);

    left0 = _mm_mullo_epi16(left0, _mm_sub_epi16(word_128, remainder_h));
    left1 = _mm_mullo_epi16(left1, _mm_sub_epi16(word_128, remainder_h));

    right0 = _mm_mullo_epi16(right0, remainder_h);
    right1 = _mm_mullo_epi16(right1, remainder_h);

    line0 = _mm_add_epi16(left0, right0);
    line1 = _mm_add_epi16(left1, right1);

    line0 = _mm_add_epi16(line0, word_64);
    line1 = _mm_add_epi16(line1, word_64);

    line0 = _mm_srai_epi16(line0, 7);
    line1 = _mm_srai_epi16(line1, 7);

    line0 = _mm_mullo_epi16(line0, _mm_sub_epi16(word_128, remainder_v));
    line1 = _mm_mullo_epi16(line1, remainder_v);

    __m128i result = _mm_add_epi16(line0, line1);

    result = _mm_add_epi16(result, word_64);

    result = _mm_srai_epi16(result, 7);

    result = _mm_packus_epi16(result, result);

    _mm_storel_epi64((__m128i *)(dstp + x), result);
}


template <int SMAGL>
static FORCE_INLINE void warp_edge_c(const uint8_t *srcp, const uint8_t *edgep, uint8_t *dstp, int src_stride, int edge_stride, int width, int height, int x, int y, int depth) {
    int SMAG = 1 << SMAGL;

    depth <<= 8;

    const int x_limit_min = 0 * SMAG;
    const int x_limit_max = (width - 1) * SMAG;

    int y_limit_min = -y * 128;
    int y_limit_max = (height - y) * 128 - 129; // (height - y - 1) * 128 - 1

    // calculate displacement

    int above = edgep[x - (y ? edge_stride : 0)];
    int below = edgep[x + (y < height - 1 ? edge_stride : 0)];

    int left = edgep[x - (x ? 1 : 0)];
    int right = edgep[x + (x < width - 1 ? 1 : 0)];

    int h = left - right;
    int v = above - below;

    h <<= 7;
    v <<= 7;

    h *= depth;
    h >>= 16;
    v *= depth;
    v >>= 16;

    v = std::max(v, y_limit_min);
    v = std::min(v, y_limit_max);

    int remainder_h = h;
    int remainder_v = v;

    if (SMAGL) {
        remainder_h <<= SMAGL;
        remainder_v <<= SMAGL;
    }

    remainder_h &= 127;
    remainder_v &= 127;

    h >>= 7 - SMAGL;
    v >>= 7 - SMAGL;

    h += x << SMAGL;
    h = std::min(std::max(h, -32768), 32767); // likely pointless

    bool remainder_needed = (x_limit_max > h) && !(x_limit_min > h);
    if (!remainder_needed)
        remainder_h = 0; // probably correct

    h = std::min(h, x_limit_max);
    h = std::max(h, x_limit_min);

    // h and v contain the displacement now.

    int s00 = srcp[v * src_stride + h];
    int s01 = srcp[v * src_stride + h + 1];
    int s10 = srcp[(v + 1) * src_stride + h];
    int s11 = srcp[(v + 1) * src_stride + h + 1];

    int s0 = s00 * (128 - remainder_h);
    int s1 = s10 * (128 - remainder_h);

    s0 += s01 * remainder_h;
    s1 += s11 * remainder_h;

    s0 += 64;
    s1 += 64;

    s0 >>= 7;
    s1 >>= 7;

    s0 *= 128 - remainder_v;
    s1 *= remainder_v;

    int s = s0 + s1;

    s += 64;

    s >>= 7;

    dstp[x] = std::min(std::max(s, 0), 255);
}


template <int SMAGL> // 0 or 2
static void warp_u8_sse2(const uint8_t *srcp, const uint8_t *edgep, uint8_t *dstp, int src_stride, int edge_stride, int dst_stride, int width, int height, int depth_scalar) {
    int SMAG = 1 << SMAGL;

    __m128i depth = _mm_set1_epi32(depth_scalar << 8);
    depth = _mm_packs_epi32(depth, depth);

    const int16_t x_limit_min_array[8] = {
        (int16_t)(0 * SMAG),
        (int16_t)(-1 * SMAG),
        (int16_t)(-2 * SMAG),
        (int16_t)(-3 * SMAG),
        (int16_t)(-4 * SMAG),
        (int16_t)(-5 * SMAG),
        (int16_t)(-6 * SMAG),
        (int16_t)(-7 * SMAG)
    };
    const int16_t x_limit_max_array[8] = {
        (int16_t)((width - 1) * SMAG),
        (int16_t)((width - 2) * SMAG),
        (int16_t)((width - 3) * SMAG),
        (int16_t)((width - 4) * SMAG),
        (int16_t)((width - 5) * SMAG),
        (int16_t)((width - 6) * SMAG),
        (int16_t)((width - 7) * SMAG),
        (int16_t)((width - 8) * SMAG)
    };

    __m128i x_limit_min = _mm_loadu_si128((const __m128i *)x_limit_min_array);
    __m128i x_limit_max = _mm_loadu_si128((const __m128i *)x_limit_max_array);

    int width_sse2 = (width & ~7) + 2;
    if (width_sse2 > dst_stride)
        width_sse2 -= 8;

    __m128i zero = _mm_setzero_si128();

    __m128i word_255 = _mm_setzero_si128();
    word_255 = _mm_cmpeq_epi16(word_255, word_255);
    word_255 = _mm_srli_epi16(word_255, 8);

    __m128i word_127 = _mm_setzero_si128();
    word_127 = _mm_cmpeq_epi16(word_127, word_127);
    word_127 = _mm_srli_epi16(word_127, 9);

    __m128i word_1 = _mm_setzero_si128();
    word_1 = _mm_cmpeq_epi16(word_1, word_1);
    word_1 = _mm_srli_epi16(word_1, 15);
    __m128i one_stride = _mm_unpacklo_epi16(_mm_set1_epi16(src_stride), word_1);

    __m128i word_128 = _mm_setzero_si128();
    word_128 = _mm_cmpeq_epi16(word_128, word_128);
    word_128 = _mm_slli_epi16(word_128, 15);
    word_128 = _mm_srli_epi16(word_128, 8);

    __m128i word_64 = _mm_setzero_si128();
    word_64 = _mm_cmpeq_epi16(word_64, word_64);
    word_64 = _mm_slli_epi16(word_64, 15);
    word_64 = _mm_srli_epi16(word_64, 9);

    for (int y = 0; y < height; y++) {
        __m128i y_limit_min = _mm_set1_epi32(-y * 128);
        __m128i y_limit_max = _mm_set1_epi32((height - y) * 128 - 129); // (height - y - 1) * 128 - 1
        y_limit_min = _mm_packs_epi32(y_limit_min, y_limit_min);
        y_limit_max = _mm_packs_epi32(y_limit_max, y_limit_max);

        warp_edge_c<SMAGL>(srcp, edgep, dstp, src_stride, edge_stride, width, height, 0, y, depth_scalar);

        for (int x = 1; x < width_sse2 - 1; x += 8)
            warp_mmword_u8_sse2<SMAGL>(srcp, edgep, dstp, src_stride, edge_stride, height, x, y, depth, zero, x_limit_min, x_limit_max, y_limit_min, y_limit_max, word_64, word_127, word_128, word_255, one_stride);

        if (width + 2 > width_sse2)
            warp_mmword_u8_sse2<SMAGL>(srcp, edgep, dstp, src_stride, edge_stride, height, width - 9, y, depth, zero, x_limit_min, x_limit_max, y_limit_min, y_limit_max, word_64, word_127, word_128, word_255, one_stride);

        warp_edge_c<SMAGL>(srcp, edgep, dstp, src_stride, edge_stride, width, height, width - 1, y, depth_scalar);

        srcp += src_stride * SMAG;
        edgep += edge_stride;
        dstp += dst_stride;
    }
}


void sobel_u8_sse2(const uint8_t *srcp, uint8_t *dstp, int stride, int width, int height, int thresh, int bits_per_sample) {
    sobel_sse2<uint8_t>(srcp, dstp, stride, width, height, thresh, bits_per_sample);
}


void sobel_u16_sse2(const uint8_t *srcp, uint8_t *dstp, int stride, int width, int height, int thresh, int bits_per_sample) {
    sobel_sse2<uint16_t>(srcp, dstp, stride, width, height, thresh, bits_per_sample);
}


void blur_r6_u8_sse2(uint8_t *mask, uint8_t *temp, int stride, int width, int height) {
    blur_r6_sse2<uint8_t>(mask, temp, stride, width, height);
}


void blur_r6_u16_sse2(uint8_t *mask, uint8_t *temp, int stride, int width, int height) {
    blur_r6_sse2<uint16_t>(mask, temp, stride, width, height);
}


void blur_r2_u8_sse2(uint8_t *mask, uint8_t *temp, int stride, int width, int height) {
    blur_r2_sse2<uint8_t>(mask, temp, stride, width, height);
}


void blur_r2_u16_sse2(uint8_t *mask, uint8_t *temp, int stride, int width, int height) {
    blur_r2_sse2<uint16_t>(mask, temp, stride, width, height);
}


void warp0_u8_sse2(const uint8_t *srcp, const uint8_t *edgep, uint8_t *dstp, int src_stride, int edge_stride, int dst_stride, int width, int height, int depth, int bits_per_sample) {
    (void)bits_per_sample;

    warp_u8_sse2<0>(srcp, edgep, dstp, src_stride, edge_stride, dst_stride, width, height, depth);
}


void warp2_u8_sse2(const uint8_t *srcp, const uint8_t *edgep, uint8_t *dstp, int src_stride, int edge_stride, int dst_stride, int width, int height, int depth, int bits_per_sample) {
    (void)bits_per_sample;

    warp_u8_sse2<2>(srcp, edgep, dstp, src_stride, edge_stride, dst_stride, width, height, depth);
}
