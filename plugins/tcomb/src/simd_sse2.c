#include <stdint.h>
#include <emmintrin.h>


#define zeroes _mm_setzero_si128()


void buildFinalMask_sse2( const uint8_t *s1p, const uint8_t *s2p, const uint8_t *m1p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height, intptr_t thresh) {
    __m128i th = _mm_set1_epi8(thresh - 1);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&s1p[x]);
            __m128i m1 = _mm_load_si128((const __m128i *)&s2p[x]);
            m0 = _mm_or_si128(_mm_subs_epu8(m0, m1),
                              _mm_subs_epu8(m1, m0));
            m0 = _mm_subs_epu8(m0, th);
            m0 = _mm_cmpeq_epi8(m0, zeroes);
            m0 = _mm_and_si128(m0, _mm_load_si128((const __m128i *)&m1p[x]));
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        s1p += stride;
        s2p += stride;
        m1p += stride;
        dstp += stride;
    }
}


void absDiff_sse2( const uint8_t *srcp1, const uint8_t *srcp2, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&srcp1[x]);
            __m128i m1 = _mm_load_si128((const __m128i *)&srcp2[x]);
            m0 = _mm_or_si128(_mm_subs_epu8(m0, m1),
                              _mm_subs_epu8(m1, m0));
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        srcp1 += stride;
        srcp2 += stride;
        dstp += stride;
    }
}


void absDiffAndMinMask_sse2( const uint8_t *srcp1, const uint8_t *srcp2, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&srcp1[x]);
            __m128i m1 = _mm_load_si128((const __m128i *)&srcp2[x]);
            m0 = _mm_or_si128(_mm_subs_epu8(m0, m1),
                              _mm_subs_epu8(m1, m0));
            m0 = _mm_min_epu8(m0, _mm_load_si128((__m128i *)&dstp[x]));
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        srcp1 += stride;
        srcp2 += stride;
        dstp += stride;
    }
}


void absDiffAndMinMaskThresh_sse2( const uint8_t *srcp1, const uint8_t *srcp2, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height, intptr_t thresh) {
    __m128i th = _mm_set1_epi8(thresh - 1);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&srcp1[x]);
            __m128i m1 = _mm_load_si128((const __m128i *)&srcp2[x]);
            m0 = _mm_or_si128(_mm_subs_epu8(m0, m1),
                              _mm_subs_epu8(m1, m0));
            m0 = _mm_min_epu8(m0, _mm_load_si128((__m128i *)&dstp[x]));
            m0 = _mm_subs_epu8(m0, th);
            m0 = _mm_cmpeq_epi8(m0, zeroes);
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        srcp1 += stride;
        srcp2 += stride;
        dstp += stride;
    }
}


void checkOscillation5_sse2( const uint8_t *p2p, const uint8_t *p1p, const uint8_t *s1p, const uint8_t *n1p, const uint8_t *n2p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height, intptr_t thresh) {
    __m128i th = _mm_set1_epi8(thresh - 1);

    __m128i bytes_1 = _mm_set1_epi8(1);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0, m1, m2, m3, m4, m5, m8;

            m0 = _mm_load_si128((const __m128i *)&p2p[x]);
            m2 = _mm_load_si128((const __m128i *)&p1p[x]);
            m1 = m0;
            m3 = m2;

            m8 = _mm_load_si128((const __m128i *)&s1p[x]);
            m0 = _mm_min_epu8(m0, m8);
            m1 = _mm_max_epu8(m1, m8);

            m8 = _mm_load_si128((const __m128i *)&n1p[x]);
            m2 = _mm_min_epu8(m2, m8);
            m3 = _mm_max_epu8(m3, m8);

            m8 = _mm_load_si128((const __m128i *)&n2p[x]);
            m0 = _mm_min_epu8(m0, m8);
            m1 = _mm_max_epu8(m1, m8);

            m4 = m3;
            m5 = m1;

            m4 = _mm_subs_epu8(m4, m2);
            m5 = _mm_subs_epu8(m5, m0);
            m4 = _mm_subs_epu8(m4, th);
            m5 = _mm_subs_epu8(m5, th);
            m2 = _mm_subs_epu8(m2, bytes_1);
            m0 = _mm_subs_epu8(m0, bytes_1);
            m1 = _mm_subs_epu8(m1, m2);
            m3 = _mm_subs_epu8(m3, m0);

            m1 = _mm_cmpeq_epi8(m1, zeroes);
            m3 = _mm_cmpeq_epi8(m3, zeroes);
            m4 = _mm_cmpeq_epi8(m4, zeroes);
            m5 = _mm_cmpeq_epi8(m5, zeroes);
            m1 = _mm_or_si128(m1, m3);
            m4 = _mm_and_si128(m4, m5);
            m1 = _mm_and_si128(m1, m4);
            _mm_store_si128((__m128i *)&dstp[x], m1);
        }

        p2p += stride;
        p1p += stride;
        s1p += stride;
        n1p += stride;
        n2p += stride;
        dstp += stride;
    }
}


void calcAverages_sse2( const uint8_t *s1p, const uint8_t *s2p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&s1p[x]);
            __m128i m1 = _mm_load_si128((const __m128i *)&s2p[x]);
            m0 = _mm_avg_epu8(m0, m1);
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        s1p += stride;
        s2p += stride;
        dstp += stride;
    }
}


void checkAvgOscCorrelation_sse2( const uint8_t *s1p, const uint8_t *s2p, const uint8_t *s3p, const uint8_t *s4p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height, intptr_t thresh) {
    __m128i th = _mm_set1_epi8(thresh - 1);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0, m1, m4, m5;

            m0 = m1 = _mm_load_si128((const __m128i *)&s1p[x]);
            m5 = _mm_load_si128((const __m128i *)&s2p[x]);
            m0 = _mm_min_epu8(m0, m5);
            m1 = _mm_max_epu8(m1, m5);

            m5 = _mm_load_si128((const __m128i *)&s3p[x]);
            m0 = _mm_min_epu8(m0, m5);
            m1 = _mm_max_epu8(m1, m5);

            m5 = _mm_load_si128((const __m128i *)&s4p[x]);
            m0 = _mm_min_epu8(m0, m5);
            m1 = _mm_max_epu8(m1, m5);

            m1 = _mm_subs_epu8(m1, m0);
            m1 = _mm_subs_epu8(m1, th);
            m1 = _mm_cmpeq_epi8(m1, zeroes);
            m4 = _mm_load_si128((const __m128i *)&dstp[x]);
            m1 = _mm_and_si128(m1, m4);
            _mm_store_si128((__m128i *)&dstp[x], m1);
        }

        s1p += stride;
        s2p += stride;
        s3p += stride;
        s4p += stride;
        dstp += stride;
    }
}


void or3Masks_sse2( const uint8_t *s1p, const uint8_t *s2p, const uint8_t *s3p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&s1p[x]);
            m0 = _mm_or_si128(m0, _mm_load_si128((const __m128i *)&s2p[x]));
            m0 = _mm_or_si128(m0, _mm_load_si128((const __m128i *)&s3p[x]));
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        s1p += stride;
        s2p += stride;
        s3p += stride;
        dstp += stride;
    }
}


void orAndMasks_sse2( const uint8_t *s1p, const uint8_t *s2p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&s1p[x]);
            m0 = _mm_and_si128(m0, _mm_load_si128((const __m128i *)&s2p[x]));
            __m128i m1 = _mm_load_si128((const __m128i *)&dstp[x]);
            m1 = _mm_or_si128(m1, m0);
            _mm_store_si128((__m128i *)&dstp[x], m1);
        }

        s1p += stride;
        s2p += stride;
        dstp += stride;
    }
}


void andMasks_sse2( const uint8_t *s1p, const uint8_t *s2p, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&s1p[x]);
            __m128i m1 = _mm_load_si128((const __m128i *)&s2p[x]);
            m0 = _mm_and_si128(m0, m1);
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        s1p += stride;
        s2p += stride;
        dstp += stride;
    }
}


void checkSceneChange_sse2( const uint8_t *s1p, const uint8_t *s2p, intptr_t height, intptr_t width, intptr_t stride, int64_t *diffp) {
    __m128i sum = zeroes;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&s1p[x]);
            __m128i m1 = _mm_load_si128((const __m128i *)&s2p[x]);
            m0 = _mm_sad_epu8(m0, m1);
            sum = _mm_add_epi64(sum, m0);
        }

        s1p += stride;
        s2p += stride;
    }

    sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8));
    _mm_storel_epi64((__m128i *)diffp, sum);
}


void verticalBlur3_sse2( const uint8_t *srcp, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    __m128i words_2 = _mm_set1_epi16(2);

    for (int x = 0; x < width; x += 16) {
        __m128i m0 = _mm_load_si128((const __m128i *)&srcp[x]);
        __m128i m1 = _mm_load_si128((const __m128i *)&srcp[x + stride]);
        m0 = _mm_avg_epu8(m0, m1);
        _mm_store_si128((__m128i *)&dstp[x], m0);
    }

    srcp += stride;
    dstp += stride;

    for (int y = 0; y < height - 2; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0, m1, m2, m3, m4, m5;

            m0 = m3 = _mm_load_si128((const __m128i *)&srcp[x - stride]);
            m1 = m4 = _mm_load_si128((const __m128i *)&srcp[x]);
            m2 = m5 = _mm_load_si128((const __m128i *)&srcp[x + stride]);

            m0 = _mm_unpacklo_epi8(m0, zeroes);
            m1 = _mm_unpacklo_epi8(m1, zeroes);
            m2 = _mm_unpacklo_epi8(m2, zeroes);

            m3 = _mm_unpackhi_epi8(m3, zeroes);
            m4 = _mm_unpackhi_epi8(m4, zeroes);
            m5 = _mm_unpackhi_epi8(m5, zeroes);

            m0 = _mm_add_epi16(m0, m2);
            m3 = _mm_add_epi16(m3, m5);

            m1 = _mm_slli_epi16(m1, 1);
            m4 = _mm_slli_epi16(m4, 1);

            m0 = _mm_add_epi16(m0, m1);
            m3 = _mm_add_epi16(m3, m4);

            m0 = _mm_add_epi16(m0, words_2);
            m3 = _mm_add_epi16(m3, words_2);

            m0 = _mm_srli_epi16(m0, 2);
            m3 = _mm_srli_epi16(m3, 2);

            m0 = _mm_packus_epi16(m0, m3);
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        srcp += stride;
        dstp += stride;
    }

    for (int x = 0; x < width; x += 16) {
        __m128i m0 = _mm_load_si128((const __m128i *)&srcp[x - stride]);
        __m128i m1 = _mm_load_si128((const __m128i *)&srcp[x]);
        m0 = _mm_avg_epu8(m0, m1);
        _mm_store_si128((__m128i *)&dstp[x], m0);
    }
}


void andNeighborsInPlace_sse2( uint8_t *srcp, intptr_t width, intptr_t height, intptr_t stride) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0 = _mm_load_si128((const __m128i *)&srcp[x - stride]);
            __m128i m1 = _mm_loadu_si128((const __m128i *)&srcp[x - stride - 1]);
            m0 = _mm_or_si128(m0, m1);

            m1 = _mm_loadu_si128((const __m128i *)&srcp[x - stride + 1]);
            m0 = _mm_or_si128(m0, m1);

            m1 = _mm_load_si128((const __m128i *)&srcp[x]);

            __m128i m2 = _mm_loadu_si128((const __m128i *)&srcp[x + stride - 1]);
            m0 = _mm_or_si128(m0, m2);

            m2 = _mm_load_si128((const __m128i *)&srcp[x + stride]);
            m0 = _mm_or_si128(m0, m2);

            m2 = _mm_loadu_si128((const __m128i *)&srcp[x + stride + 1]);
            m0 = _mm_or_si128(m0, m2);

            m0 = _mm_and_si128(m0, m1);

            _mm_store_si128((__m128i *)&srcp[x], m0);
        }

        srcp += stride;
    }
}


void minMax_sse2( const uint8_t *srcp, uint8_t *minp, uint8_t *maxp, intptr_t width, intptr_t height, intptr_t src_stride, intptr_t min_stride, intptr_t thresh) {
    __m128i th = _mm_set1_epi8(thresh);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0, m1, m2;

            m0 = m1 = _mm_load_si128((const __m128i *)&srcp[x - src_stride - 1]);
            m2 = _mm_loadu_si128((const __m128i *)&srcp[x - src_stride]);
            m0 = _mm_min_epu8(m0, m2);
            m1 = _mm_max_epu8(m1, m2);

            m2 = _mm_loadu_si128((const __m128i *)&srcp[x - src_stride + 1]);
            m0 = _mm_min_epu8(m0, m2);
            m1 = _mm_max_epu8(m1, m2);

            m2 = _mm_load_si128((const __m128i *)&srcp[x - 1]);
            m0 = _mm_min_epu8(m0, m2);
            m1 = _mm_max_epu8(m1, m2);

            m2 = _mm_loadu_si128((const __m128i *)&srcp[x]);
            m0 = _mm_min_epu8(m0, m2);
            m1 = _mm_max_epu8(m1, m2);

            m2 = _mm_loadu_si128((const __m128i *)&srcp[x + 1]);
            m0 = _mm_min_epu8(m0, m2);
            m1 = _mm_max_epu8(m1, m2);

            m2 = _mm_load_si128((const __m128i *)&srcp[x + src_stride - 1]);
            m0 = _mm_min_epu8(m0, m2);
            m1 = _mm_max_epu8(m1, m2);

            m2 = _mm_loadu_si128((const __m128i *)&srcp[x + src_stride]);
            m0 = _mm_min_epu8(m0, m2);
            m1 = _mm_max_epu8(m1, m2);

            m2 = _mm_loadu_si128((const __m128i *)&srcp[x + src_stride + 1]);
            m0 = _mm_min_epu8(m0, m2);
            m1 = _mm_max_epu8(m1, m2);

            m0 = _mm_subs_epu8(m0, th);
            m1 = _mm_adds_epu8(m1, th);

            _mm_store_si128((__m128i *)&minp[x], m0);
            _mm_store_si128((__m128i *)&maxp[x], m1);
        }

        srcp += src_stride;
        minp += min_stride;
        maxp += min_stride;
    }
}


void horizontalBlur3_sse2( const uint8_t *srcp, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    __m128i words_2 = _mm_set1_epi16(2);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0, m1, m2, m3, m4, m5;

            m0 = m3 = _mm_loadu_si128((const __m128i *)&srcp[x - 1]);
            m1 = m4 = _mm_load_si128((const __m128i *)&srcp[x]);
            m2 = m5 = _mm_loadu_si128((const __m128i *)&srcp[x + 1]);

            m0 = _mm_unpacklo_epi8(m0, zeroes);
            m1 = _mm_unpacklo_epi8(m1, zeroes);
            m2 = _mm_unpacklo_epi8(m2, zeroes);

            m3 = _mm_unpackhi_epi8(m3, zeroes);
            m4 = _mm_unpackhi_epi8(m4, zeroes);
            m5 = _mm_unpackhi_epi8(m5, zeroes);

            m0 = _mm_add_epi16(m0, m2);
            m3 = _mm_add_epi16(m3, m5);

            m1 = _mm_slli_epi16(m1, 1);
            m4 = _mm_slli_epi16(m4, 1);

            m0 = _mm_add_epi16(m0, m1);
            m3 = _mm_add_epi16(m3, m4);

            m0 = _mm_add_epi16(m0, words_2);
            m3 = _mm_add_epi16(m3, words_2);

            m0 = _mm_srli_epi16(m0, 2);
            m3 = _mm_srli_epi16(m3, 2);

            m0 = _mm_packus_epi16(m0, m3);
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        srcp += stride;
        dstp += stride;
    }
}


void horizontalBlur6_sse2( const uint8_t *srcp, uint8_t *dstp, intptr_t stride, intptr_t width, intptr_t height) {
    __m128i words_6 = _mm_set1_epi16(6);
    __m128i words_8 = _mm_set1_epi16(8);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i m0, m1, m2, m3, m4, m5, m6, m7, m8, m9;

            m0 = m5 = _mm_loadu_si128((const __m128i *)&srcp[x - 2]);
            m1 = m6 = _mm_loadu_si128((const __m128i *)&srcp[x - 1]);
            m2 = m7 = _mm_load_si128((const __m128i *)&srcp[x]);
            m3 = m8 = _mm_loadu_si128((const __m128i *)&srcp[x + 1]);
            m4 = m9 = _mm_loadu_si128((const __m128i *)&srcp[x + 2]);

            m0 = _mm_unpacklo_epi8(m0, zeroes);
            m1 = _mm_unpacklo_epi8(m1, zeroes);
            m2 = _mm_unpacklo_epi8(m2, zeroes);
            m3 = _mm_unpacklo_epi8(m3, zeroes);
            m4 = _mm_unpacklo_epi8(m4, zeroes);

            m5 = _mm_unpackhi_epi8(m5, zeroes);
            m6 = _mm_unpackhi_epi8(m6, zeroes);
            m7 = _mm_unpackhi_epi8(m7, zeroes);
            m8 = _mm_unpackhi_epi8(m8, zeroes);
            m9 = _mm_unpackhi_epi8(m9, zeroes);

            m0 = _mm_add_epi16(m0, m4);
            m5 = _mm_add_epi16(m5, m9);

            m1 = _mm_add_epi16(m1, m3);
            m6 = _mm_add_epi16(m6, m8);

            m1 = _mm_slli_epi16(m1, 2);
            m6 = _mm_slli_epi16(m6, 2);

            m0 = _mm_add_epi16(m0, m1);
            m5 = _mm_add_epi16(m5, m6);

            m2 = _mm_mullo_epi16(m2, words_6);
            m7 = _mm_mullo_epi16(m7, words_6);

            m0 = _mm_add_epi16(m0, m2);
            m5 = _mm_add_epi16(m5, m7);

            m0 = _mm_add_epi16(m0, words_8);
            m5 = _mm_add_epi16(m5, words_8);

            m0 = _mm_srli_epi16(m0, 4);
            m5 = _mm_srli_epi16(m5, 4);

            m0 = _mm_packus_epi16(m0, m5);
            _mm_store_si128((__m128i *)&dstp[x], m0);
        }

        srcp += stride;
        dstp += stride;
    }
}
