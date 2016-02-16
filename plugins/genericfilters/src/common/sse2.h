/*
  sse2.h: Copyright (C) 2012-2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This file is part of GenericFilters.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with the author; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


#ifndef SSE2_DEFINE_HEADER
#define SSE2_DEFINE_HEADER

#include <string.h>
#include <stdint.h>
#include <emmintrin.h>

#ifdef _MSC_VER
#define GF_ALIGN __declspec(align(16))
#define GF_FUNC_ALIGN
#define F_INLINE __forceinline
#else
#ifdef __MINGW32__
#define GF_FUNC_ALIGN __attribute__((force_align_arg_pointer))
#else
#define GF_FUNC_ALIGN
#endif // __MINGW32__
#define GF_ALIGN __attribute__((aligned(16)))
#define F_INLINE inline __attribute__((always_inline))
#endif // _MSC_VER

#define MM_MAX_EPU16(X, Y) (_mm_adds_epu16(Y, _mm_subs_epu16(X, Y)))
#define MM_MIN_EPU16(X, Y) (_mm_subs_epu16(X, _mm_subs_epu16(X, Y)))


static F_INLINE __m128i mm_cast_epi32(__m128i xmm0, __m128i xmm1)
{
    xmm0 = _mm_shufflelo_epi16(xmm0, _MM_SHUFFLE(3, 1, 2, 0));
    xmm0 = _mm_shufflehi_epi16(xmm0, _MM_SHUFFLE(3, 1, 2, 0));
    xmm1 = _mm_shufflelo_epi16(xmm1, _MM_SHUFFLE(2, 0, 3, 1));
    xmm1 = _mm_shufflehi_epi16(xmm1, _MM_SHUFFLE(2, 0, 3, 1));
    xmm0 = _mm_or_si128(xmm0, xmm1);
    return _mm_shuffle_epi32(xmm0, _MM_SHUFFLE(3, 1, 2, 0));
}


static F_INLINE __m128i mm_max_epi32(__m128i xmm0, __m128i xmm1)
{
    __m128i mask = _mm_cmpgt_epi32(xmm0, xmm1);
    return _mm_or_si128(_mm_and_si128(mask, xmm0),
                         _mm_andnot_si128(mask, xmm1));
}


static F_INLINE __m128i mm_min_epi32(__m128i xmm0, __m128i xmm1)
{
    __m128i mask = _mm_cmplt_epi32(xmm0, xmm1);
    return _mm_or_si128(_mm_and_si128(mask, xmm0),
                        _mm_andnot_si128(mask, xmm1));
}


static F_INLINE __m128i mm_abs_epi16(__m128i xmm0)
{
    __m128i all1 = _mm_cmpeq_epi32(xmm0, xmm0);
    __m128i mask = _mm_cmpgt_epi16(xmm0, _mm_setzero_si128());
    __m128i temp = _mm_add_epi16(_mm_xor_si128(xmm0, all1),
                                 _mm_srli_epi16(all1, 15));
    return _mm_or_si128(_mm_and_si128(mask, xmm0),
                         _mm_andnot_si128(mask, temp));
}


static F_INLINE __m128i mm_abs_epi32(__m128i xmm0)
{
    __m128i all1 = _mm_cmpeq_epi32(xmm0, xmm0);
    __m128i mask = _mm_cmpgt_epi32(xmm0, _mm_setzero_si128());
    __m128i temp = _mm_add_epi32(_mm_xor_si128(xmm0, all1),
                                 _mm_srli_epi32(all1, 31));
    return _mm_or_si128(_mm_and_si128(mask, xmm0),
                        _mm_andnot_si128(mask, temp));
}


static F_INLINE __m128 mm_abs_ps(const __m128 xmm0)
{
    const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    return _mm_and_ps(xmm0, mask);
}


static F_INLINE __m128 mm_ivtsign_ps(const __m128 xmm0)
{
    const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
    return _mm_xor_ps(xmm0, mask);
}


static F_INLINE __m128 mm_rcp_hq_ps(const __m128 xmm0)
{
    __m128 rcp = _mm_rcp_ps(xmm0);
    __m128 xmm1 =  _mm_mul_ps(_mm_mul_ps(xmm0, rcp), rcp);
    rcp = _mm_add_ps(rcp, rcp);
    return _mm_sub_ps(rcp, xmm1);
}


static inline void
line_copy8(uint8_t *line, const uint8_t *srcp, int width, int mergin)
{
    memcpy(line, srcp, width);
    for (int i = mergin; i > 0; i--) {
        line[-i] = line[i];
        line[width - 1 + i] = line[width - 1 - i];
    }
}


static inline void
line_copy16(uint16_t *line, const uint16_t *srcp, int width, int mergin)
{
    memcpy(line, srcp, width * 2);
    for (int i = mergin; i > 0; i--) {
        line[-i] = line[i];
        line[width - 1 + i] = line[width - 1 - i];
    }
}


static inline void
line_copyf(float *line, const float *srcp, int width, int mergin)
{
    memcpy(line, srcp, width * sizeof(float));
    for (int i = mergin; i > 0; i--) {
        line[-i] = line[i];
        line[width - 1 + i] = line[width - 1 - i];
    }
}

#endif
