#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(FLUXSMOOTH_X86)
#include <emmintrin.h>
#endif

#include <VSHelper.h>
#include <VapourSynth.h>


enum {
    TemporalFlux,
    SpatioTemporalFlux
};


typedef struct FluxSmoothData {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    int temporal_threshold;
    int spatial_threshold;
    int process[3];

    void (*flux_function)(const uint8_t *, const uint8_t *, const uint8_t *, uint8_t *, int, int, int, int, int);
} FluxSmoothData;


#if defined(FLUXSMOOTH_X86)

#ifdef _WIN32
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif


static void fluxsmooth_temporal_uint8_sse2(const uint8_t *srcpp, const uint8_t *srccp, const uint8_t *srcnp, uint8_t *dstp, int width, int height, int stride, int temporal_threshold, int spatial_threshold) {
    (void)width;
    (void)spatial_threshold;

#define zeroes _mm_setzero_si128()

    const __m128i bytes_sign_bit = _mm_set1_epi8(128);

    const __m128i bytes_threshold = _mm_add_epi8(_mm_set1_epi8(temporal_threshold), bytes_sign_bit);

    const __m128i words_multiplier3 = _mm_set1_epi16(10923);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < stride; x += 16) {
            __m128i prev = _mm_load_si128((const __m128i *)&srcpp[x]);
            __m128i curr = _mm_load_si128((const __m128i *)&srccp[x]);
            __m128i next = _mm_load_si128((const __m128i *)&srcnp[x]);

            __m128i count = _mm_set1_epi8(3);

            __m128i prev_diff = _mm_or_si128(_mm_subs_epu8(prev, curr),
                                             _mm_subs_epu8(curr, prev));
            prev_diff = _mm_add_epi8(prev_diff, bytes_sign_bit);

            __m128i prev_mask = _mm_cmpgt_epi8(prev_diff, bytes_threshold);
            count = _mm_add_epi8(count, prev_mask); // add -1 (255) or 0
            __m128i selected_prev_pixels = _mm_andnot_si128(prev_mask, prev);

            __m128i next_diff = _mm_or_si128(_mm_subs_epu8(curr, next),
                                             _mm_subs_epu8(next, curr));
            next_diff = _mm_add_epi8(next_diff, bytes_sign_bit);

            __m128i next_mask = _mm_cmpgt_epi8(next_diff, bytes_threshold);
            count = _mm_add_epi8(count, next_mask); // add -1 (255) or 0
            __m128i selected_next_pixels = _mm_andnot_si128(next_mask, next);

            __m128i sumlo = _mm_unpacklo_epi8(curr, zeroes);
            sumlo = _mm_add_epi16(sumlo, _mm_unpacklo_epi8(selected_prev_pixels, zeroes));
            sumlo = _mm_add_epi16(sumlo, _mm_unpacklo_epi8(selected_next_pixels, zeroes));

            __m128i sumhi = _mm_unpackhi_epi8(curr, zeroes);
            sumhi = _mm_add_epi16(sumhi, _mm_unpackhi_epi8(selected_prev_pixels, zeroes));
            sumhi = _mm_add_epi16(sumhi, _mm_unpackhi_epi8(selected_next_pixels, zeroes));

            sumlo = _mm_slli_epi16(sumlo, 1);
            sumhi = _mm_slli_epi16(sumhi, 1);

            sumlo = _mm_add_epi16(sumlo, _mm_unpacklo_epi8(count, zeroes));
            sumhi = _mm_add_epi16(sumhi, _mm_unpackhi_epi8(count, zeroes));

            __m128i average_three = _mm_packus_epi16(_mm_mulhi_epi16(sumlo, words_multiplier3),
                                                     _mm_mulhi_epi16(sumhi, words_multiplier3));

            __m128i average_two = _mm_packus_epi16(_mm_srli_epi16(sumlo, 2),
                                                   _mm_srli_epi16(sumhi, 2));

            __m128i m4 = _mm_and_si128(next_mask, prev_mask);
            __m128i m8 = _mm_xor_si128(next_mask, prev_mask);
            __m128i m6 = _mm_or_si128(m4, m8);

            m4 = _mm_and_si128(m4, curr);               // middle pixels only
            m8 = _mm_and_si128(m8, average_two);        // average of two pixels
            m6 = _mm_andnot_si128(m6, average_three);   // average of three pixels

            m4 = _mm_or_si128(m4, m6);
            m4 = _mm_or_si128(m4, m8);

            __m128i m3 = curr;

            prev = _mm_add_epi8(prev, bytes_sign_bit);
            curr = _mm_add_epi8(curr, bytes_sign_bit);
            next = _mm_add_epi8(next, bytes_sign_bit);

            __m128i m5, m7;

            m5 = _mm_cmpgt_epi8(curr, prev);
            m6 = _mm_cmpgt_epi8(curr, next);
            m5 = _mm_and_si128(m5, m6);

            m6 = _mm_cmpgt_epi8(prev, curr);
            m7 = _mm_cmpgt_epi8(next, curr);
            m6 = _mm_and_si128(m6, m7);

            m5 = _mm_or_si128(m5, m6);

            m6 = _mm_and_si128(m5, m4);
            m5 = _mm_andnot_si128(m5, m3);
            m5 = _mm_or_si128(m5, m6);

            _mm_store_si128((__m128i *)&dstp[x], m5);
        }

        srcpp += stride;
        srccp += stride;
        srcnp += stride;
        dstp += stride;
    }
}


static void fluxsmooth_temporal_uint16_sse2(const uint8_t *srcpp, const uint8_t *srccp, const uint8_t *srcnp, uint8_t *dstp, int width, int height, int stride, int temporal_threshold, int spatial_threshold) {
    (void)width;
    (void)spatial_threshold;

    __m128i words_sign_bit = _mm_set1_epi16(0x8000);

    __m128i words_threshold = _mm_add_epi16(_mm_set1_epi16(temporal_threshold), words_sign_bit);

    __m128 floats_0_5 = _mm_set1_ps(0.5f);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < stride; x += 16) {
            __m128i prev = _mm_load_si128((const __m128i *)&srcpp[x]);
            __m128i curr = _mm_load_si128((const __m128i *)&srccp[x]);
            __m128i next = _mm_load_si128((const __m128i *)&srcnp[x]);

            __m128i count = _mm_set1_epi16(3);

            __m128i prev_diff = _mm_or_si128(_mm_subs_epu16(prev, curr),
                                             _mm_subs_epu16(curr, prev));
            prev_diff = _mm_add_epi16(prev_diff, words_sign_bit);

            __m128i prev_mask = _mm_cmpgt_epi16(prev_diff, words_threshold);
            count = _mm_add_epi16(count, prev_mask); // add -1 (65535) or 0
            __m128i selected_prev_pixels = _mm_andnot_si128(prev_mask, prev);

            __m128i next_diff = _mm_or_si128(_mm_subs_epu16(curr, next),
                                             _mm_subs_epu16(next, curr));
            next_diff = _mm_add_epi16(next_diff, words_sign_bit);

            __m128i next_mask = _mm_cmpgt_epi16(next_diff, words_threshold);
            count = _mm_add_epi16(count, next_mask); // add -1 (65535) or 0
            __m128i selected_next_pixels = _mm_andnot_si128(next_mask, next);

            __m128i sumlo = _mm_unpacklo_epi16(curr, zeroes);
            sumlo = _mm_add_epi32(sumlo, _mm_unpacklo_epi16(selected_prev_pixels, zeroes));
            sumlo = _mm_add_epi32(sumlo, _mm_unpacklo_epi16(selected_next_pixels, zeroes));

            __m128i sumhi = _mm_unpackhi_epi16(curr, zeroes);
            sumhi = _mm_add_epi32(sumhi, _mm_unpackhi_epi16(selected_prev_pixels, zeroes));
            sumhi = _mm_add_epi32(sumhi, _mm_unpackhi_epi16(selected_next_pixels, zeroes));

            __m128 fcountlo = _mm_cvtepi32_ps(_mm_unpacklo_epi16(count, zeroes));
            __m128 fcounthi = _mm_cvtepi32_ps(_mm_unpackhi_epi16(count, zeroes));

            __m128 favglo = _mm_mul_ps(_mm_cvtepi32_ps(sumlo), _mm_rcp_ps(fcountlo));
            __m128 favghi = _mm_mul_ps(_mm_cvtepi32_ps(sumhi), _mm_rcp_ps(fcounthi));

            favglo = _mm_add_ps(favglo, floats_0_5);
            favghi = _mm_add_ps(favghi, floats_0_5);

            __m128i avglo = _mm_cvttps_epi32(favglo);
            __m128i avghi = _mm_cvttps_epi32(favghi);

            avglo = _mm_srai_epi32(_mm_slli_epi32(_mm_add_epi16(avglo, words_sign_bit), 16), 16);
            avghi = _mm_srai_epi32(_mm_slli_epi32(_mm_add_epi16(avghi, words_sign_bit), 16), 16);
            __m128i avg = _mm_add_epi16(_mm_packs_epi32(avglo, avghi), words_sign_bit);

            __m128i m3 = curr;

            prev = _mm_add_epi16(prev, words_sign_bit);
            curr = _mm_add_epi16(curr, words_sign_bit);
            next = _mm_add_epi16(next, words_sign_bit);

            __m128i m5 = _mm_cmpgt_epi16(curr, prev);
            __m128i m6 = _mm_cmpgt_epi16(curr, next);
            m5 = _mm_and_si128(m5, m6);

            m6 = _mm_cmpgt_epi16(prev, curr);
            __m128i m7 = _mm_cmpgt_epi16(next, curr);
            m6 = _mm_and_si128(m6, m7);

            m5 = _mm_or_si128(m5, m6);

            m6 = _mm_and_si128(m5, avg);
            m5 = _mm_andnot_si128(m5, m3);
            m5 = _mm_or_si128(m5, m6);

            _mm_store_si128((__m128i *)&dstp[x], m5);
        }

        srcpp += stride;
        srccp += stride;
        srcnp += stride;
        dstp += stride;
    }
}


static FORCE_INLINE void fluxsmooth_spatiotemporal_mmword_uint8_sse2(const uint8_t *srcpp, const uint8_t *srccp, const uint8_t *srcnp, uint8_t *dstp, int x, int stride, const __m128i words_temporal_threshold, const __m128i words_spatial_threshold) {
    __m128i prev = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srcpp[x]), zeroes);
    __m128i curr = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x]), zeroes);
    __m128i next = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srcnp[x]), zeroes);
    // "n" for "neighbour"
    __m128i n1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x - stride - 1]), zeroes);
    __m128i n2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x - stride]),     zeroes);
    __m128i n3 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x - stride + 1]), zeroes);
    __m128i n4 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x - 1]),          zeroes);
    __m128i n5 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x + 1]),          zeroes);
    __m128i n6 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x + stride - 1]), zeroes);
    __m128i n7 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x + stride]),     zeroes);
    __m128i n8 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&srccp[x + stride + 1]), zeroes);

    __m128i count = _mm_set1_epi16(11);
    __m128i sum = curr;

#define CHECK_NEIGHBOUR(n, threshold) \
    __m128i n##_diff = _mm_or_si128(_mm_subs_epu16(n, curr), \
                                    _mm_subs_epu16(curr, n)); \
    __m128i n##_mask = _mm_cmpgt_epi16(n##_diff, threshold); \
    count = _mm_add_epi16(count, n##_mask); \
    sum = _mm_add_epi16(sum, _mm_andnot_si128(n##_mask, n));

    CHECK_NEIGHBOUR(prev, words_temporal_threshold)
    CHECK_NEIGHBOUR(next, words_temporal_threshold)
    CHECK_NEIGHBOUR(n1, words_spatial_threshold)
    CHECK_NEIGHBOUR(n2, words_spatial_threshold)
    CHECK_NEIGHBOUR(n3, words_spatial_threshold)
    CHECK_NEIGHBOUR(n4, words_spatial_threshold)
    CHECK_NEIGHBOUR(n5, words_spatial_threshold)
    CHECK_NEIGHBOUR(n6, words_spatial_threshold)
    CHECK_NEIGHBOUR(n7, words_spatial_threshold)
    CHECK_NEIGHBOUR(n8, words_spatial_threshold)

#undef CHECK_NEIGHBOUR

    __m128 fcountlo = _mm_cvtepi32_ps(_mm_unpacklo_epi16(count, zeroes));
    __m128 fcounthi = _mm_cvtepi32_ps(_mm_unpackhi_epi16(count, zeroes));

    __m128 favglo = _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(sum, zeroes)),
                               _mm_rcp_ps(fcountlo));
    __m128 favghi = _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(sum, zeroes)),
                               _mm_rcp_ps(fcounthi));

    const __m128 floats_0_5 = _mm_set1_ps(0.5f);

    favglo = _mm_add_ps(favglo, floats_0_5);
    favghi = _mm_add_ps(favghi, floats_0_5);

    __m128i avg = _mm_packs_epi32(_mm_cvttps_epi32(favglo),
                                  _mm_cvttps_epi32(favghi));

    __m128i m0 = _mm_and_si128(_mm_cmpgt_epi16(curr, prev),
                               _mm_cmpgt_epi16(curr, next));
    __m128i m1 = _mm_and_si128(_mm_cmpgt_epi16(prev, curr),
                               _mm_cmpgt_epi16(next, curr));

    m0 = _mm_or_si128(m0, m1);

    m1 = _mm_or_si128(_mm_and_si128(m0, avg),
                      _mm_andnot_si128(m0, curr));

    _mm_storel_epi64((__m128i *)&dstp[x],
                     _mm_packus_epi16(m1, zeroes));
}


static void fluxsmooth_spatiotemporal_uint8_sse2(const uint8_t *srcpp, const uint8_t *srccp, const uint8_t *srcnp, uint8_t *dstp, int width, int height, int stride, int temporal_threshold, int spatial_threshold) {
    const __m128i words_temporal_threshold = _mm_set1_epi16(temporal_threshold);
    const __m128i words_spatial_threshold = _mm_set1_epi16(spatial_threshold);

    memcpy(dstp, srccp, width);
    srcpp += stride;
    srccp += stride;
    srcnp += stride;
    dstp += stride;

    const int pixels_in_mm = 8 / sizeof(uint8_t);
    const int skip_left = 1;
    const int skip_right = 1;

    int width_sse2 = (width & ~(pixels_in_mm - 1)) + skip_left + skip_right;
    if (width_sse2 > stride)
        width_sse2 -= pixels_in_mm;

    for (int y = 1; y < height - 1; y++) {
        dstp[0] = srccp[0];

        for (int x = skip_left; x < width_sse2 - skip_right; x += pixels_in_mm)
            fluxsmooth_spatiotemporal_mmword_uint8_sse2(srcpp, srccp, srcnp, dstp, x, stride, words_temporal_threshold, words_spatial_threshold);

        if (width + skip_left + skip_right > width_sse2)
            fluxsmooth_spatiotemporal_mmword_uint8_sse2(srcpp, srccp, srcnp, dstp, width - pixels_in_mm - skip_right, stride, words_temporal_threshold, words_spatial_threshold);

        dstp[width - 1] = srccp[width - 1];

        srcpp += stride;
        srccp += stride;
        srcnp += stride;
        dstp += stride;
    }

    memcpy(dstp, srccp, width);

#undef zeroes
}


#else // FLUXSMOOTH_X86


static void fluxsmooth_temporal_uint8_c(const uint8_t *srcpp, const uint8_t *srccp, const uint8_t *srcnp, uint8_t *dstp, int width, int height, int stride, int temporal_threshold, int spatial_threshold) {
    (void)spatial_threshold;

    int x, y;
    int16_t magic_numbers[] = { 0, 32767, 16384, 10923 };
    /* Calculated thusly:
    magic_numbers[1] = 32767;
    for (int i = 2; i < 4; i++) {
        magic_numbers[i] = (int16_t)(32768.0 / i + 0.5);
    }
    */

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            uint8_t prev = srcpp[x];
            uint8_t curr = srccp[x];
            uint8_t next = srcnp[x];
            int prevdiff = prev - curr;
            int nextdiff = next - curr;

            if ((prevdiff < 0 && nextdiff < 0) || (prevdiff > 0 && nextdiff > 0)) {
                int sum = curr;
                int count = 1;

                if (abs(prevdiff) <= temporal_threshold) {
                    sum += prev;
                    count++;
                }

                if (abs(nextdiff) <= temporal_threshold) {
                    sum += next;
                    count++;
                }

                // the sum is multiplied by 2 so that the division is always by an even number,
                // thus rounding can always be done by adding half the divisor
                dstp[x] = (uint8_t)(((sum * 2 + count) * magic_numbers[count]) >> 16);
                //dstp[x] = (uint8_t)(sum / (float)count + 0.5f);
            } else {
                dstp[x] = curr;
            }
        }

        srcpp += stride;
        srccp += stride;
        srcnp += stride;
        dstp += stride;
    }
}


static void fluxsmooth_temporal_uint16_c(const uint8_t *srcpp, const uint8_t *srccp, const uint8_t *srcnp, uint8_t *dstp, int width, int height, int stride, int temporal_threshold, int spatial_threshold) {
    (void)spatial_threshold;

    int x, y;
    int magic_numbers[] = { 0, 262144, 131072, 87381 };

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            uint16_t prev = ((const uint16_t *)srcpp)[x];
            uint16_t curr = ((const uint16_t *)srccp)[x];
            uint16_t next = ((const uint16_t *)srcnp)[x];
            int prevdiff = prev - curr;
            int nextdiff = next - curr;

            if ((prevdiff < 0 && nextdiff < 0) || (prevdiff > 0 && nextdiff > 0)) {
                int sum = curr;
                int count = 1;

                if (abs(prevdiff) <= temporal_threshold) {
                    sum += prev;
                    count++;
                }

                if (abs(nextdiff) <= temporal_threshold) {
                    sum += next;
                    count++;
                }

                ((uint16_t *)dstp)[x] = (uint16_t)(((sum * 2 + count) * (int64_t)magic_numbers[count]) >> 19);
            } else {
                ((uint16_t *)dstp)[x] = curr;
            }
        }

        srcpp += stride;
        srccp += stride;
        srcnp += stride;
        dstp += stride;
    }
}


static void fluxsmooth_spatiotemporal_uint8_c(const uint8_t *srcpp, const uint8_t *srccp, const uint8_t *srcnp, uint8_t *dstp, int width, int height, int stride, int temporal_threshold, int spatial_threshold) {
    int x, y;
    int16_t magic_numbers[] = { 0, 32767, 16384, 10923, 8192, 6554, 5461, 4681, 4096, 3641, 3277, 2979 };
    /* Calculated thusly:
    magic_numbers[1] = 32767;
    for (int i = 2; i < 12; i++) {
        magic_numbers[i] = (int16_t)(32768.0 / i + 0.5);
    }
    */
    memcpy(dstp, srccp, stride);
    srcpp += stride;
    srccp += stride;
    srcnp += stride;
    dstp += stride;

    for (y = 0; y < height - 2; y++) {
        dstp[0] = srccp[0];

        for (x = 1; x < width - 1; x++) {
            uint8_t prev = srcpp[x];
            uint8_t curr = srccp[x];
            uint8_t next = srcnp[x];
            int prevdiff = prev - curr;
            int nextdiff = next - curr;
            // "n" for "neighbour"
            uint8_t n1 = srccp[x - stride - 1];
            uint8_t n2 = srccp[x - stride];
            uint8_t n3 = srccp[x - stride + 1];
            uint8_t n4 = srccp[x - 1];
            uint8_t n5 = srccp[x + 1];
            uint8_t n6 = srccp[x + stride - 1];
            uint8_t n7 = srccp[x + stride];
            uint8_t n8 = srccp[x + stride + 1];

            if ((prevdiff < 0 && nextdiff < 0) || (prevdiff > 0 && nextdiff > 0)) {
                int sum = curr;
                int count = 1;

                if (abs(prevdiff) <= temporal_threshold) {
                    sum += prev;
                    count++;
                }

                if (abs(nextdiff) <= temporal_threshold) {
                    sum += next;
                    count++;
                }

                if (abs(n1 - curr) <= spatial_threshold) {
                    sum += n1;
                    count++;
                }

                if (abs(n2 - curr) <= spatial_threshold) {
                    sum += n2;
                    count++;
                }

                if (abs(n3 - curr) <= spatial_threshold) {
                    sum += n3;
                    count++;
                }

                if (abs(n4 - curr) <= spatial_threshold) {
                    sum += n4;
                    count++;
                }

                if (abs(n5 - curr) <= spatial_threshold) {
                    sum += n5;
                    count++;
                }

                if (abs(n6 - curr) <= spatial_threshold) {
                    sum += n6;
                    count++;
                }

                if (abs(n7 - curr) <= spatial_threshold) {
                    sum += n7;
                    count++;
                }

                if (abs(n8 - curr) <= spatial_threshold) {
                    sum += n8;
                    count++;
                }

                // the sum is multiplied by 2 so that the division is always by an even number,
                // thus rounding can always be done by adding half the divisor
                dstp[x] = (uint8_t)(((sum * 2 + count) * magic_numbers[count]) >> 16);
            } else {
                dstp[x] = curr;
            }
        }

        dstp[width - 1] = srccp[width - 1];

        srcpp += stride;
        srccp += stride;
        srcnp += stride;
        dstp += stride;
    }

    memcpy(dstp, srccp, stride);
}
#endif // FLUXSMOOTH_X86


static void VS_CC fluxSmoothInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    FluxSmoothData *d = (FluxSmoothData *)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}


static void fluxsmooth_spatiotemporal_uint16_c(const uint8_t *srcpp, const uint8_t *srccp, const uint8_t *srcnp, uint8_t *dstp, int width, int height, int stride, int temporal_threshold, int spatial_threshold) {
    int x, y;
    int magic_numbers[] = { 0, 1048576, 524288, 349525, 262144, 209715, 174763, 149797, 131072, 116508, 104858, 95325 };

    memcpy(dstp, srccp, stride);
    srcpp += stride;
    srccp += stride;
    srcnp += stride;
    dstp += stride;

    for (y = 0; y < height - 2; y++) {
        ((uint16_t *)dstp)[0] = ((const uint16_t *)srccp)[0];

        for (x = 1; x < width - 1; x++) {
            uint16_t prev = ((const uint16_t *)srcpp)[x];
            uint16_t curr = ((const uint16_t *)srccp)[x];
            uint16_t next = ((const uint16_t *)srcnp)[x];
            int prevdiff = prev - curr;
            int nextdiff = next - curr;
            // "n" for "neighbour"
            uint16_t n1 = ((const uint16_t *)srccp)[x - stride / 2 - 1];
            uint16_t n2 = ((const uint16_t *)srccp)[x - stride / 2];
            uint16_t n3 = ((const uint16_t *)srccp)[x - stride / 2 + 1];
            uint16_t n4 = ((const uint16_t *)srccp)[x - 1];
            uint16_t n5 = ((const uint16_t *)srccp)[x + 1];
            uint16_t n6 = ((const uint16_t *)srccp)[x + stride / 2 - 1];
            uint16_t n7 = ((const uint16_t *)srccp)[x + stride / 2];
            uint16_t n8 = ((const uint16_t *)srccp)[x + stride / 2 + 1];

            if ((prevdiff < 0 && nextdiff < 0) || (prevdiff > 0 && nextdiff > 0)) {
                int sum = curr;
                int count = 1;

                if (abs(prevdiff) <= temporal_threshold) {
                    sum += prev;
                    count++;
                }

                if (abs(nextdiff) <= temporal_threshold) {
                    sum += next;
                    count++;
                }

                if (abs(n1 - curr) <= spatial_threshold) {
                    sum += n1;
                    count++;
                }

                if (abs(n2 - curr) <= spatial_threshold) {
                    sum += n2;
                    count++;
                }

                if (abs(n3 - curr) <= spatial_threshold) {
                    sum += n3;
                    count++;
                }

                if (abs(n4 - curr) <= spatial_threshold) {
                    sum += n4;
                    count++;
                }

                if (abs(n5 - curr) <= spatial_threshold) {
                    sum += n5;
                    count++;
                }

                if (abs(n6 - curr) <= spatial_threshold) {
                    sum += n6;
                    count++;
                }

                if (abs(n7 - curr) <= spatial_threshold) {
                    sum += n7;
                    count++;
                }

                if (abs(n8 - curr) <= spatial_threshold) {
                    sum += n8;
                    count++;
                }

                ((uint16_t *)dstp)[x] = (uint16_t)(((sum * 2 + count) * (int64_t)magic_numbers[count]) >> 21);
            } else {
                ((uint16_t *)dstp)[x] = curr;
            }
        }

        ((uint16_t *)dstp)[width - 1] = ((const uint16_t *)srccp)[width - 1];

        srcpp += stride;
        srccp += stride;
        srcnp += stride;
        dstp += stride;
    }

    memcpy(dstp, srccp, stride);
}


static const VSFrameRef *VS_CC fluxSmoothGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    FluxSmoothData *d = (FluxSmoothData *)*instanceData;

    if (activationReason == arInitial) {
        if (n == 0 || n == d->vi->numFrames - 1) {
            vsapi->requestFrameFilter(n, d->node, frameCtx);
        } else {
            vsapi->requestFrameFilter(n - 1, d->node, frameCtx);
            vsapi->requestFrameFilter(n, d->node, frameCtx);
            vsapi->requestFrameFilter(n + 1, d->node, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *srcp, *srcc, *srcn;
        VSFrameRef *dst;
        int plane;

        if (n == 0 || n == d->vi->numFrames - 1) {
            return vsapi->getFrameFilter(n, d->node, frameCtx);
        }

        srcp = vsapi->getFrameFilter(n - 1, d->node, frameCtx);
        srcc = vsapi->getFrameFilter(n, d->node, frameCtx);
        srcn = vsapi->getFrameFilter(n + 1, d->node, frameCtx);
        const VSFrameRef *frames[] = { d->process[0] ? 0 : srcc, d->process[1] ? 0 : srcc, d->process[2] ? 0 : srcc };
        const int planes[] = { 0, 1, 2 };
        dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, frames, planes, srcc, core);

        for (plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane]) {
                const uint8_t *srcpp = vsapi->getReadPtr(srcp, plane);
                const uint8_t *srccp = vsapi->getReadPtr(srcc, plane);
                const uint8_t *srcnp = vsapi->getReadPtr(srcn, plane);
                uint8_t *dstp = vsapi->getWritePtr(dst, plane);
                int width = vsapi->getFrameWidth(srcc, plane);
                int height = vsapi->getFrameHeight(srcc, plane);
                int stride = vsapi->getStride(srcc, plane);

                d->flux_function(srcpp, srccp, srcnp, dstp, width, height, stride, d->temporal_threshold, d->spatial_threshold);
            }
        }

        vsapi->freeFrame(srcp);
        vsapi->freeFrame(srcc);
        vsapi->freeFrame(srcn);

        return dst;
    }

    return 0;
}


static void VS_CC fluxSmoothFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    FluxSmoothData *d = (FluxSmoothData *)instanceData;

    vsapi->freeNode(d->node);
    free(d);
}


static void VS_CC fluxSmoothCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    FluxSmoothData d;
    FluxSmoothData *data;
    int err;
    int i, m, n, o;
    intptr_t function = (intptr_t)userData;
    const char *filter_name = function == SpatioTemporalFlux ? "SmoothST" : "SmoothT";

#define ERROR_SIZE 128
    char error[ERROR_SIZE] = { 0 };

    d.temporal_threshold = vsapi->propGetInt(in, "temporal_threshold", 0, &err);
    if (err) {
        d.temporal_threshold = 7;
    }

    if (function == SpatioTemporalFlux) {
        d.spatial_threshold = vsapi->propGetInt(in, "spatial_threshold", 0, &err);
        if (err) {
            d.spatial_threshold = 7;
        }

        if (d.temporal_threshold < -1) {
            snprintf(error, ERROR_SIZE, "%s: temporal_threshold must be -1 or greater.", filter_name);
            vsapi->setError(out, error);
            return;
        }

        if (d.spatial_threshold < -1) {
            snprintf(error, ERROR_SIZE, "%s: spatial_threshold must be -1 or greater.", filter_name);
            vsapi->setError(out, error);
            return;
        }

        if (d.temporal_threshold == -1 && d.spatial_threshold == -1) {
            snprintf(error, ERROR_SIZE, "%s: At least one threshold must be greater than -1.", filter_name);
            vsapi->setError(out, error);
            return;
        }
    } else {
        if (d.temporal_threshold < 0) {
            snprintf(error, ERROR_SIZE, "%s: temporal_threshold must be 0 or greater.", filter_name);
            vsapi->setError(out, error);
            return;
        }
    }

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bitsPerSample > 16) {
        snprintf(error, ERROR_SIZE, "%s: Only 8..16 bit integer input with constant format and dimensions supported.", filter_name);
        vsapi->setError(out, error);
        vsapi->freeNode(d.node);
        return;
    }

    // TODO: Find better way.
    /*
   if (function == SpatioTemporalFlux) {
      if (d.temporal_threshold != -1) {
         d.temporal_threshold <<= d.vi->format->bitsPerSample - 8;
      }

      if (d.spatial_threshold != -1) {
         d.spatial_threshold <<= d.vi->format->bitsPerSample - 8;
      }
   } else {
      d.temporal_threshold <<= d.vi->format->bitsPerSample - 8;
   }
   */

    n = d.vi->format->numPlanes;
    m = vsapi->propNumElements(in, "planes");

    for (i = 0; i < 3; i++) {
        d.process[i] = (m <= 0);
    }

    for (i = 0; i < m; i++) {
        o = vsapi->propGetInt(in, "planes", i, 0);

        if (o < 0 || o >= n) {
            snprintf(error, ERROR_SIZE, "%s: Plane index out of range.", filter_name);
            vsapi->setError(out, error);
            vsapi->freeNode(d.node);
            return;
        }

        if (d.process[o]) {
            vsapi->setError(out, error);
            snprintf(error, ERROR_SIZE, "%s: Plane specified twice.", filter_name);
#undef ERROR_SIZE
            vsapi->freeNode(d.node);
            return;
        }

        d.process[o] = 1;
    }

    // Select the functions.
    if (function == SpatioTemporalFlux) {
        if (d.vi->format->bytesPerSample == 1) {
#if defined(FLUXSMOOTH_X86)
            d.flux_function = fluxsmooth_spatiotemporal_uint8_sse2;
#else
            d.flux_function = fluxsmooth_spatiotemporal_uint8_c;
#endif
        } else {
            d.flux_function = fluxsmooth_spatiotemporal_uint16_c;
        }
    } else {
        if (d.vi->format->bytesPerSample == 1) {
#if defined(FLUXSMOOTH_X86)
            d.flux_function = fluxsmooth_temporal_uint8_sse2;
#else
            d.flux_function = fluxsmooth_temporal_uint8_c;
#endif
        } else {
#if defined(FLUXSMOOTH_X86)
            d.flux_function = fluxsmooth_temporal_uint16_sse2;
#else
            d.flux_function = fluxsmooth_temporal_uint16_c;
#endif
        }
    }

    data = malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, filter_name, fluxSmoothInit, fluxSmoothGetFrame, fluxSmoothFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.fluxsmooth", "flux", "FluxSmooth plugin for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("SmoothT",
                 "clip:clip;"
                 "temporal_threshold:int:opt;"
                 "planes:int[]:opt;",
                 fluxSmoothCreate, (void *)TemporalFlux, plugin);
    registerFunc("SmoothST",
                 "clip:clip;"
                 "temporal_threshold:int:opt;"
                 "spatial_threshold:int:opt;"
                 "planes:int[]:opt;",
                 fluxSmoothCreate, (void *)SpatioTemporalFlux, plugin);
}
