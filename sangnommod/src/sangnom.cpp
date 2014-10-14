#include <VSHelper.h>
#include <VapourSynth.h>
#include <emmintrin.h>
#include <stdio.h>
#include "IScriptEnvironment.h"


#ifdef _MSC_VER
#define SG_FORCEINLINE __forceinline
#else
#define SG_FORCEINLINE inline
#endif

//#define USE_MOVPS

extern "C" {
#ifdef USE_MOVPS
	SG_FORCEINLINE __m128i simd_load_si128(const BYTE * ptr) {
		return _mm_castps_si128(_mm_load_ps(reinterpret_cast<const float*>(ptr)));
	}
	SG_FORCEINLINE __m128i simd_loadu_si128(const BYTE * ptr) {
		return _mm_castps_si128(_mm_loadu_ps(reinterpret_cast<const float*>(ptr)));
	}
	SG_FORCEINLINE void simd_store_si128(BYTE * ptr, __m128i value) {
		_mm_store_ps(reinterpret_cast<float*>(ptr), _mm_castsi128_ps(value));
	}
	SG_FORCEINLINE void simd_storeu_si128(BYTE * ptr, __m128i value) {
		_mm_storeu_ps(reinterpret_cast<float*>(ptr), _mm_castsi128_ps(value));
	}
#else
	SG_FORCEINLINE __m128i simd_load_si128(const BYTE * ptr) {
        return _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
	}
	SG_FORCEINLINE __m128i simd_loadu_si128(const BYTE * ptr) {
        return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
	}
	SG_FORCEINLINE void simd_store_si128(BYTE * ptr, __m128i value) {
        _mm_store_si128(reinterpret_cast<__m128i*>(ptr), value);
	}
	SG_FORCEINLINE void simd_storeu_si128(BYTE * ptr, __m128i value) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), value);
	}
#endif
}
#ifdef _MSC_VER
#pragma warning(disable: 4309)
#endif

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_one_to_left(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_setr_epi8(0xFF, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_slli_si128(val, 1);
		auto andm = _mm_and_si128(val, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr - 1);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_two_to_left(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_setr_epi8(0xFF, 0xFF, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_slli_si128(val, 2);
		auto unpck = _mm_unpacklo_epi8(val, val);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr - 2);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_three_to_left(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_setr_epi8(0xFF, 0xFF, 0xFF, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_slli_si128(val, 3);
		auto unpck = _mm_unpacklo_epi8(val, val);
		unpck = _mm_unpacklo_epi16(unpck, unpck);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr - 3);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_one_epi16_to_left(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_setr_epi16(0xFFFF, 00, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_slli_si128(val, 2);
		auto unpck = _mm_unpacklo_epi16(val, val);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr - 2);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_two_epi16_to_left(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_setr_epi16(0xFFFF, 0xFFFF, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_slli_si128(val, 4);
		auto unpck = _mm_unpacklo_epi16(val, val);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr - 4);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_three_epi16_to_left(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_setr_epi16(0xFFFF, 0xFFFF, 0xFFFF, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_slli_si128(val, 6);
		auto unpck = _mm_unpacklo_epi16(val, val);
		unpck = _mm_unpacklo_epi32(unpck, unpck);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr - 6);
}

//note the difference between set and setr for left and right loading
template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_one_to_right(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_set_epi8(0xFF, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_srli_si128(val, 1);
		auto andm = _mm_and_si128(val, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr + 1);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_two_to_right(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_set_epi8(0xFF, 0xFF, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_srli_si128(val, 2);
		auto unpck = _mm_unpackhi_epi8(val, val);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr + 2);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_three_to_right(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_set_epi8(0xFF, 0xFF, 0xFF, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_srli_si128(val, 3);
		auto unpck = _mm_unpackhi_epi8(val, val);
		unpck = _mm_unpackhi_epi16(unpck, unpck);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr + 3);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_one_epi16_to_right(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_set_epi16(0xFFFF, 00, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_srli_si128(val, 2);
		auto unpck = _mm_unpackhi_epi16(val, val);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr + 2);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_two_epi16_to_right(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_set_epi16(0xFFFF, 0xFFFF, 00, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_srli_si128(val, 4);
		auto unpck = _mm_unpackhi_epi16(val, val);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr + 4);
}

template <bool isBorder, decltype(simd_load_si128) simd_load>
static SG_FORCEINLINE __m128i simd_load_three_epi16_to_right(const BYTE * ptr) {
	if (isBorder) {
		auto mask = _mm_set_epi16(0xFFFF, 0xFFFF, 0xFFFF, 00, 00, 00, 00, 00);
		auto val = simd_load(ptr);
		auto shifted = _mm_srli_si128(val, 6);
		auto unpck = _mm_unpackhi_epi16(val, val);
		unpck = _mm_unpackhi_epi32(unpck, unpck);
		auto andm = _mm_and_si128(unpck, mask);
		return _mm_or_si128(shifted, andm);
	}
	return simd_loadu_si128(ptr + 6);
}

#ifdef _MSC_VER
#pragma warning(default: 4309)
#endif

enum Buffers {
	ADIFF_M3_P3 = 0,
	ADIFF_M2_P2 = 1,
	ADIFF_M1_P1 = 2,
	ADIFF_P0_M0 = 4,
	ADIFF_P1_M1 = 6,
	ADIFF_P2_M2 = 7,
	ADIFF_P3_M3 = 8,

	SG_FORWARD = 3,
	SG_REVERSE = 5
};

enum class BorderMode {
	LEFT,
	RIGHT,
	NONE
};

const int BUFFERS_COUNT = 9;


static SG_FORCEINLINE __m128i simd_abs_diff_epu8(__m128i a, __m128i b) {
	auto positive = _mm_subs_epu8(a, b);
	auto negative = _mm_subs_epu8(b, a);
	return _mm_or_si128(positive, negative);
}

static SG_FORCEINLINE __m128i calculateSangnom(const __m128i & p1, const __m128i & p2, const __m128i & p3) {
	auto zero = _mm_setzero_si128();

	auto temp_lo = _mm_unpacklo_epi8(p1, zero);
	auto temp_hi = _mm_unpackhi_epi8(p1, zero);

	temp_lo = _mm_slli_epi16(temp_lo, 2); //p1*4
	temp_hi = _mm_slli_epi16(temp_hi, 2);

	auto t2_lo = _mm_unpacklo_epi8(p2, zero);
	auto t2_hi = _mm_unpackhi_epi8(p2, zero);

	temp_lo = _mm_adds_epu16(temp_lo, t2_lo); //p1*4 + p2
	temp_hi = _mm_adds_epu16(temp_hi, t2_hi);

	t2_lo = _mm_slli_epi16(t2_lo, 2);
	t2_hi = _mm_slli_epi16(t2_hi, 2);

	temp_lo = _mm_adds_epu16(temp_lo, t2_lo); //p1*4 + p2*4 + p2 = p1*4 + p2*5
	temp_hi = _mm_adds_epu16(temp_hi, t2_hi);

	auto t3_lo = _mm_unpacklo_epi8(p3, zero);
	auto t3_hi = _mm_unpackhi_epi8(p3, zero);

	temp_lo = _mm_subs_epu16(temp_lo, t3_lo); //p1*4 + p2*5 - p3
	temp_hi = _mm_subs_epu16(temp_hi, t3_hi);

	temp_lo = _mm_srli_epi16(temp_lo, 3); //(p1*4 + p2*5 - p3) / 8
	temp_hi = _mm_srli_epi16(temp_hi, 3);

	return _mm_packus_epi16(temp_lo, temp_hi); //(p1*4 + p2*5 - p3) / 8
}


static SG_FORCEINLINE __m128i blendAvgOnMinimalBuffer(const __m128i & a1, const __m128i & a2, const __m128i & buf,
                                                      const __m128i & minv, const __m128i & acc, const __m128i & zero) {
	auto average = _mm_avg_epu8(a1, a2);
	//buffer is minimal
	auto mask = _mm_cmpeq_epi8(buf, minv);
	//blend
	auto avgPart = _mm_and_si128(mask, average);
	auto accPart = _mm_andnot_si128(mask, acc);
	return _mm_or_si128(avgPart, accPart);
}

template <BorderMode border, decltype(simd_load_si128) simd_load, decltype(simd_store_si128) simd_store>
static SG_FORCEINLINE void prepareBuffersLine(const BYTE * pSrc, const BYTE * pSrcn2, BYTE * pBuffers[BUFFERS_COUNT], int bufferOffset, int width) {
	for (int x = 0; x < width; x += 16) {
		auto cur_minus_3 = simd_load_three_to_left<border == BorderMode::LEFT, simd_load>(pSrc + x);
		auto cur_minus_2 = simd_load_two_to_left<border == BorderMode::LEFT, simd_load>(pSrc + x);
		auto cur_minus_1 = simd_load_one_to_left<border == BorderMode::LEFT, simd_load>(pSrc + x);
		auto cur = simd_load(pSrc + x);
		auto cur_plus_1 = simd_load_one_to_right<border == BorderMode::RIGHT, simd_load>(pSrc + x);
		auto cur_plus_2 = simd_load_two_to_right<border == BorderMode::RIGHT, simd_load>(pSrc + x);
		auto cur_plus_3 = simd_load_three_to_right<border == BorderMode::RIGHT, simd_load>(pSrc + x);

		auto next_minus_3 = simd_load_three_to_left<border == BorderMode::LEFT, simd_load>(pSrcn2 + x);
		auto next_minus_2 = simd_load_two_to_left<border == BorderMode::LEFT, simd_load>(pSrcn2 + x);
		auto next_minus_1 = simd_load_one_to_left<border == BorderMode::LEFT, simd_load>(pSrcn2 + x);
		auto next = simd_load(pSrcn2 + x);
		auto next_plus_1 = simd_load_one_to_right<border == BorderMode::RIGHT, simd_load>(pSrcn2 + x);
		auto next_plus_2 = simd_load_two_to_right<border == BorderMode::RIGHT, simd_load>(pSrcn2 + x);
		auto next_plus_3 = simd_load_three_to_right<border == BorderMode::RIGHT, simd_load>(pSrcn2 + x);

		auto adiff_m3_p3 = simd_abs_diff_epu8(cur_minus_3, next_plus_3);
		simd_store(pBuffers[ADIFF_M3_P3] + bufferOffset + x, adiff_m3_p3);

		auto adiff_m2_p2 = simd_abs_diff_epu8(cur_minus_2, next_plus_2);
		simd_store(pBuffers[ADIFF_M2_P2] + bufferOffset + x, adiff_m2_p2);

		auto adiff_m1_p1 = simd_abs_diff_epu8(cur_minus_1, next_plus_1);
		simd_store(pBuffers[ADIFF_M1_P1] + bufferOffset + x, adiff_m1_p1);

		auto adiff_0 = simd_abs_diff_epu8(cur, next);
		simd_store(pBuffers[ADIFF_P0_M0] + bufferOffset + x, adiff_0);

		auto adiff_p1_m1 = simd_abs_diff_epu8(cur_plus_1, next_minus_1);
		simd_store(pBuffers[ADIFF_P1_M1] + bufferOffset + x, adiff_p1_m1);

		auto adiff_p2_m2 = simd_abs_diff_epu8(cur_plus_2, next_minus_2);
		simd_store(pBuffers[ADIFF_P2_M2] + bufferOffset + x, adiff_p2_m2);

		auto adiff_p3_m3 = simd_abs_diff_epu8(cur_plus_3, next_minus_3);
		simd_store(pBuffers[ADIFF_P3_M3] + bufferOffset + x, adiff_p3_m3);

		//////////////////////////////////////////////////////////////////////////
		auto temp1 = calculateSangnom(cur_minus_1, cur, cur_plus_1);
		auto temp2 = calculateSangnom(next_plus_1, next, next_minus_1);

		//abs((cur_minus_1*4 + cur*5 - cur_plus_1) / 8  - (next_plus_1*4 + next*5 - next_minus_1) / 8)
		auto absdiff_p1_p2 = simd_abs_diff_epu8(temp1, temp2);
		simd_store(pBuffers[SG_FORWARD] + bufferOffset + x, absdiff_p1_p2);
		//////////////////////////////////////////////////////////////////////////
		auto temp3 = calculateSangnom(cur_plus_1, cur, cur_minus_1);
		auto temp4 = calculateSangnom(next_minus_1, next, next_plus_1);

		//abs((cur_plus_1*4 + cur*5 - cur_minus_1) / 8  - (next_minus_1*4 + next*5 - next_plus_1) / 8)
		auto absdiff_p3_p4 = simd_abs_diff_epu8(temp3, temp4);
		simd_store(pBuffers[SG_REVERSE] + bufferOffset + x, absdiff_p3_p4);
		//////////////////////////////////////////////////////////////////////////
	}
}

template <BorderMode border>
static SG_FORCEINLINE void finalizeBufferProcessingBlock(BYTE * pTemp, BYTE * pSrcn, int x) {
	auto cur_minus_6_lo = simd_load_three_epi16_to_left<border == BorderMode::LEFT, simd_load_si128>(pTemp + x * 2);
	auto cur_minus_4_lo = simd_load_two_epi16_to_left<border == BorderMode::LEFT, simd_load_si128>(pTemp + x * 2);
	auto cur_minus_2_lo = simd_load_one_epi16_to_left<border == BorderMode::LEFT, simd_load_si128>(pTemp + x * 2);
	auto cur_lo = simd_load_si128(pTemp + x * 2);
	auto cur_plus_2_lo = simd_load_one_epi16_to_right<false, simd_load_si128>(pTemp + x * 2);
	auto cur_plus_4_lo = simd_load_two_epi16_to_right<false, simd_load_si128>(pTemp + x * 2);
	auto cur_plus_6_lo = simd_load_three_epi16_to_right<false, simd_load_si128>(pTemp + x * 2);

	auto cur_minus_6_hi = simd_load_three_epi16_to_left<false, simd_load_si128>(pTemp + x * 2 + 16);
	auto cur_minus_4_hi = simd_load_two_epi16_to_left<false, simd_load_si128>(pTemp + x * 2 + 16);
	auto cur_minus_2_hi = simd_load_one_epi16_to_left<false, simd_load_si128>(pTemp + x * 2 + 16);
	auto cur_hi = simd_load_si128(pTemp + x * 2 + 16);
	auto cur_plus_2_hi = simd_load_one_epi16_to_right<border == BorderMode::RIGHT, simd_load_si128>(pTemp + x * 2 + 16);
	auto cur_plus_4_hi = simd_load_two_epi16_to_right<border == BorderMode::RIGHT, simd_load_si128>(pTemp + x * 2 + 16);
	auto cur_plus_6_hi = simd_load_three_epi16_to_right<border == BorderMode::RIGHT, simd_load_si128>(pTemp + x * 2 + 16);

	auto sum_lo = _mm_adds_epu16(cur_minus_6_lo, cur_minus_4_lo);
	sum_lo = _mm_adds_epu16(sum_lo, cur_minus_2_lo);
	sum_lo = _mm_adds_epu16(sum_lo, cur_lo);
	sum_lo = _mm_adds_epu16(sum_lo, cur_plus_2_lo);
	sum_lo = _mm_adds_epu16(sum_lo, cur_plus_4_lo);
	sum_lo = _mm_adds_epu16(sum_lo, cur_plus_6_lo);

	sum_lo = _mm_srli_epi16(sum_lo, 4);

	auto sum_hi = _mm_adds_epu16(cur_minus_6_hi, cur_minus_4_hi);
	sum_hi = _mm_adds_epu16(sum_hi, cur_minus_2_hi);
	sum_hi = _mm_adds_epu16(sum_hi, cur_hi);
	sum_hi = _mm_adds_epu16(sum_hi, cur_plus_2_hi);
	sum_hi = _mm_adds_epu16(sum_hi, cur_plus_4_hi);
	sum_hi = _mm_adds_epu16(sum_hi, cur_plus_6_hi);

	sum_hi = _mm_srli_epi16(sum_hi, 4);

	auto result = _mm_packus_epi16(sum_lo, sum_hi);
	simd_store_si128(pSrcn + x, result);
}

static void processBuffer(BYTE * pBuffer, BYTE * pTemp, int pitch, int height) {
	auto pSrc = pBuffer;
	auto pSrcn = pSrc + pitch;
	auto pSrcn2 = pSrcn + pitch;

	for (int y = 0; y < height - 1; ++y) {
		auto zero = _mm_setzero_si128();
		for (int x = 0; x < pitch; x += 16) {
			auto src = simd_load_si128(pSrc + x);
			auto srcn = simd_load_si128(pSrcn + x);
			auto srcn2 = simd_load_si128(pSrcn2 + x);

			auto src_lo = _mm_unpacklo_epi8(src, zero);
			auto srcn_lo = _mm_unpacklo_epi8(srcn, zero);
			auto srcn2_lo = _mm_unpacklo_epi8(srcn2, zero);

			auto src_hi = _mm_unpackhi_epi8(src, zero);
			auto srcn_hi = _mm_unpackhi_epi8(srcn, zero);
			auto srcn2_hi = _mm_unpackhi_epi8(srcn2, zero);

			auto sum_lo = _mm_adds_epu16(src_lo, srcn_lo);
			sum_lo = _mm_adds_epu16(sum_lo, srcn2_lo);

			auto sum_hi = _mm_adds_epu16(src_hi, srcn_hi);
			sum_hi = _mm_adds_epu16(sum_hi, srcn2_hi);

			simd_store_si128(pTemp + (x * 2), sum_lo);
			simd_store_si128(pTemp + (x * 2) + 16, sum_hi);
		}

		finalizeBufferProcessingBlock<BorderMode::LEFT>(pTemp, pSrcn, 0);

		for (int x = 16; x < pitch - 16; x += 16) {
			finalizeBufferProcessingBlock<BorderMode::NONE>(pTemp, pSrcn, x);
		}

		finalizeBufferProcessingBlock<BorderMode::RIGHT>(pTemp, pSrcn, pitch - 16);

		pSrc += pitch;
		pSrcn += pitch;
		pSrcn2 += pitch;
	}
}

template <BorderMode border, decltype(simd_load_si128) simd_load, decltype(simd_load_si128) simd_load_buffer, decltype(simd_store_si128) simd_store>
static SG_FORCEINLINE void finalizePlaneLine(const BYTE * pSrc, const BYTE * pSrcn2, BYTE * pDstn, BYTE * pBuffers[BUFFERS_COUNT], int bufferOffset, int width, const __m128i & aath) {
	auto zero = _mm_setzero_si128();
	for (int x = 0; x < width; x += 16) {
		auto buf0 = simd_load_buffer(pBuffers[ADIFF_M3_P3] + bufferOffset + x);
		auto buf1 = simd_load_buffer(pBuffers[ADIFF_M2_P2] + bufferOffset + x);
		auto buf2 = simd_load_buffer(pBuffers[ADIFF_M1_P1] + bufferOffset + x);
		auto buf3 = simd_load_buffer(pBuffers[SG_FORWARD] + bufferOffset + x);
		auto buf4 = simd_load_buffer(pBuffers[ADIFF_P0_M0] + bufferOffset + x);
		auto buf5 = simd_load_buffer(pBuffers[SG_REVERSE] + bufferOffset + x);
		auto buf6 = simd_load_buffer(pBuffers[ADIFF_P1_M1] + bufferOffset + x);
		auto buf7 = simd_load_buffer(pBuffers[ADIFF_P2_M2] + bufferOffset + x);
		auto buf8 = simd_load_buffer(pBuffers[ADIFF_P3_M3] + bufferOffset + x);

		auto cur_minus_3 = simd_load_three_to_left<border == BorderMode::LEFT, simd_load>(pSrc + x);
		auto cur_minus_2 = simd_load_two_to_left<border == BorderMode::LEFT, simd_load>(pSrc + x);
		auto cur_minus_1 = simd_load_one_to_left<border == BorderMode::LEFT, simd_load>(pSrc + x);
		auto cur = simd_load(pSrc + x);
		auto cur_plus_1 = simd_load_one_to_right<border == BorderMode::RIGHT, simd_load>(pSrc + x);
		auto cur_plus_2 = simd_load_two_to_right<border == BorderMode::RIGHT, simd_load>(pSrc + x);
		auto cur_plus_3 = simd_load_three_to_right<border == BorderMode::RIGHT, simd_load>(pSrc + x);

		auto next_minus_3 = simd_load_three_to_left<border == BorderMode::LEFT, simd_load>(pSrcn2 + x);
		auto next_minus_2 = simd_load_two_to_left<border == BorderMode::LEFT, simd_load>(pSrcn2 + x);
		auto next_minus_1 = simd_load_one_to_left<border == BorderMode::LEFT, simd_load>(pSrcn2 + x);
		auto next = simd_load(pSrcn2 + x);
		auto next_plus_1 = simd_load_one_to_right<border == BorderMode::RIGHT, simd_load>(pSrcn2 + x);
		auto next_plus_2 = simd_load_two_to_right<border == BorderMode::RIGHT, simd_load>(pSrcn2 + x);
		auto next_plus_3 = simd_load_three_to_right<border == BorderMode::RIGHT, simd_load>(pSrcn2 + x);

		auto minbuf = _mm_min_epu8(buf0, buf1);
		minbuf = _mm_min_epu8(minbuf, buf2);
		minbuf = _mm_min_epu8(minbuf, buf3);
		minbuf = _mm_min_epu8(minbuf, buf4);
		minbuf = _mm_min_epu8(minbuf, buf5);
		minbuf = _mm_min_epu8(minbuf, buf6);
		minbuf = _mm_min_epu8(minbuf, buf7);
		minbuf = _mm_min_epu8(minbuf, buf8);

		auto processed = _mm_setzero_si128();

		processed = blendAvgOnMinimalBuffer(cur_minus_3, next_plus_3, buf0, minbuf, processed, zero);
		processed = blendAvgOnMinimalBuffer(cur_plus_3, next_minus_3, buf8, minbuf, processed, zero);

		processed = blendAvgOnMinimalBuffer(cur_minus_2, next_plus_2, buf1, minbuf, processed, zero);
		processed = blendAvgOnMinimalBuffer(cur_plus_2, next_minus_2, buf7, minbuf, processed, zero);

		processed = blendAvgOnMinimalBuffer(cur_minus_1, next_plus_1, buf2, minbuf, processed, zero);
		processed = blendAvgOnMinimalBuffer(cur_plus_1, next_minus_1, buf6, minbuf, processed, zero);

		////////////////////////////////////////////////////////////////////////////
		auto temp1 = calculateSangnom(cur_minus_1, cur, cur_plus_1);
		auto temp2 = calculateSangnom(next_plus_1, next, next_minus_1);

		processed = blendAvgOnMinimalBuffer(temp1, temp2, buf3, minbuf, processed, zero);
		////////////////////////////////////////////////////////////////////////////
		auto temp3 = calculateSangnom(cur_plus_1, cur, cur_minus_1);
		auto temp4 = calculateSangnom(next_minus_1, next, next_plus_1);

		processed = blendAvgOnMinimalBuffer(temp3, temp4, buf5, minbuf, processed, zero);
		////////////////////////////////////////////////////////////////////////////

		auto average = _mm_avg_epu8(cur, next);

		auto buf4IsMinimal = _mm_cmpeq_epi8(buf4, minbuf);

		auto takeAaa = _mm_subs_epu8(minbuf, aath);
		//this isn't strictly negation, don't optimize
		auto takeProcessed = _mm_cmpeq_epi8(takeAaa, zero);
		auto mask = _mm_andnot_si128(buf4IsMinimal, takeProcessed);

		//blending
		processed = _mm_and_si128(mask, processed);
		average = _mm_andnot_si128(mask, average);
		auto result = _mm_or_si128(processed, average);

		simd_store(pDstn + x, result);
	}
}


class SangNom2 {
public:
	SangNom2(VSVideoInfo * vi, VSNodeRef * node, int order, int aa, int aac);

	VSFrameRef* VS_CC GetFrame(int n, IScriptEnvironment * env);

	void GetFramePre(int i, IScriptEnvironment * env) {
		env->PrefetchFrame(i);
	}

	~SangNom2() {
	}

	VSNodeRef * node_;
	VSVideoInfo * vi_;
private:
	int order_;
	int offset_;
	int aa_;
	int aaUv_;

	//BYTE * buffers_[BUFFERS_COUNT];
	//BYTE * buffersPool_;
	int bufferPitch_;
	int bufferHeight_;
	int bufferSize_;

	void processPlane(IScriptEnvironment * env, BYTE * pBuffers[BUFFERS_COUNT], const BYTE * srcp, BYTE * dstp, int width, int height, int src_pitch, int dst_pitch, int aa);
};

SangNom2::SangNom2(VSVideoInfo * vi, VSNodeRef * node, int order, int aa, int aac)
	: node_(node), vi_(vi), order_(order) {

	bufferPitch_ = (vi->width + 15) / 16 * 16;
	bufferHeight_ = (vi->height + 1) / 2;
	bufferSize_ = bufferPitch_ * (bufferHeight_ + 1);

	aa_ = (21 * VSMIN(128, aa)) / 16;
	aaUv_ = (21 * VSMIN(128, aac)) / 16;
}

void SangNom2::processPlane(IScriptEnvironment * env, BYTE * pBuffers[BUFFERS_COUNT], const BYTE * pSrc, BYTE * pDst, int width, int height, int srcPitch, int dstPitch, int aa) {
	env->BitBlt(pDst + offset_ * dstPitch, dstPitch * 2, pSrc + offset_ * srcPitch, srcPitch * 2, width, height / 2);

	if (offset_ == 1) {
		env->BitBlt(pDst, dstPitch, pSrc + srcPitch, srcPitch, width, 1);
	}
	else {
		env->BitBlt(pDst + dstPitch * (height - 1), dstPitch, pSrc + srcPitch * (height - 2), srcPitch, width, 1);
	}

	// Prepare Buffer
	{
		auto pSrcn1 = pSrc + offset_ * srcPitch;
		auto pSrcn2 = pSrc + srcPitch * 2;

		auto bufferOffset = bufferPitch_;
		int sse2Width = (width - 1 - 16) / 16 * 16 + 16;

		for (int y = 0; y < height / 2 - 1; y++) {
			prepareBuffersLine<BorderMode::LEFT, simd_load_si128, simd_store_si128>(pSrcn1, pSrcn2, pBuffers, bufferOffset, 16);
			prepareBuffersLine<BorderMode::NONE, simd_load_si128, simd_store_si128>(pSrcn1 + 16, pSrcn2 + 16, pBuffers, bufferOffset + 16, sse2Width - 16);
			// Unligned load/store due to width not mod16 under u/v plane
			prepareBuffersLine<BorderMode::RIGHT, simd_loadu_si128, simd_storeu_si128>(pSrcn1 + width - 16, pSrcn2 + width - 16, pBuffers, bufferOffset + width - 16, 16);

			pSrcn1 += srcPitch * 2;
			pSrcn2 += srcPitch * 2;
			bufferOffset += bufferPitch_;
		}
		
	}
	// Process Buffer
	auto temp = static_cast<BYTE*>(_mm_malloc(bufferPitch_ * 2, 16));
	for (int i = 0; i < BUFFERS_COUNT; ++i) {
		processBuffer(pBuffers[i], temp, bufferPitch_, bufferHeight_);
	}
	_mm_free(temp);

	// Finalize Plane
	{
		auto pSrcn1 = pSrc + offset_ * srcPitch;
		auto pDstn = pDst + dstPitch + offset_ * dstPitch;
		auto pSrcn2 = pSrc + srcPitch * 2;
		auto aav = _mm_set1_epi8(aa);
		auto bufferOffset = bufferPitch_;
		int sse2Width = (width - 1 - 16) / 16 * 16 + 16;

		for (int y = 0; y < height / 2 - 1; ++y) {
			finalizePlaneLine<BorderMode::LEFT, simd_load_si128, simd_load_si128, simd_store_si128>(pSrcn1, pSrcn2, pDstn, pBuffers, bufferOffset, 16, aav);
			finalizePlaneLine<BorderMode::NONE, simd_load_si128, simd_load_si128, simd_store_si128>(pSrcn1 + 16, pSrcn2 + 16, pDstn + 16, pBuffers, bufferOffset + 16, sse2Width - 16, aav);
			finalizePlaneLine<BorderMode::RIGHT, simd_loadu_si128, simd_loadu_si128, simd_storeu_si128>(pSrcn1 + width - 16, pSrcn2 + width - 16, pDstn + width - 16, pBuffers, bufferOffset + width - 16, 16, aav);

			pSrcn1 += srcPitch * 2;
			pSrcn2 += srcPitch * 2;
			pDstn += dstPitch * 2;
			bufferOffset += bufferPitch_;
		}
		
	}
}


VSFrameRef* VS_CC SangNom2::GetFrame(int n, IScriptEnvironment * env) {
	auto srcFrame = env->GetFrame(n);
	auto dstFrame = env->NewVideoFrame(env->vi);
	// Make sure we are thread-safe
	auto buffer = static_cast<BYTE*>(_mm_malloc(bufferSize_ * BUFFERS_COUNT, 16));
	BYTE * pBuffers[BUFFERS_COUNT];
	for (int i = 0; i < BUFFERS_COUNT; i++) {
		pBuffers[i] = buffer + bufferPitch_ * i;
		memset(pBuffers[i], 0, bufferPitch_);
	}

	offset_ = order_ > 1 ? 1 : 0; // Due to no parity() in VS

	processPlane(env, pBuffers, env->GetReadPtr(srcFrame, PLANAR_Y), env->GetWritePtr(dstFrame, PLANAR_Y), env->GetRowSize(srcFrame, PLANAR_Y),
	             env->GetHeight(srcFrame, PLANAR_Y), env->GetPitch(srcFrame, PLANAR_Y), env->GetPitch(srcFrame, PLANAR_Y), aa_);

	if (vi_->format->colorFamily == cmGray) {
		// Skip
	}
	else if (aaUv_ < 0) {
		for (int planar = 1; planar < vi_->format->numPlanes; planar++)
			env->BitBlt(
			env->GetWritePtr(dstFrame, planar),
			env->GetPitch(dstFrame, planar),
			env->GetReadPtr(srcFrame, planar),
			env->GetPitch(srcFrame, planar),
			env->GetRowSize(srcFrame, planar),
			env->GetHeight(srcFrame, planar)
			);
	}
	else {
		for (int planar = 1; planar < vi_->format->numPlanes; planar++)
			processPlane(env, pBuffers,
			env->GetReadPtr(srcFrame, planar),
			env->GetWritePtr(dstFrame, planar),
			env->GetRowSize(srcFrame, planar),
			env->GetHeight(srcFrame, planar),
			env->GetPitch(srcFrame, planar),
			env->GetPitch(dstFrame, planar),
			aaUv_);
	}

	_mm_free(buffer);

	env->FreeFrame(srcFrame);
	return dstFrame;
}

void VS_CC
snmInit(VSMap * in, VSMap * out, void ** instanceData, VSNode * node, VSCore * core, const VSAPI * vsapi) {
	SangNom2 * d = *reinterpret_cast<SangNom2**>(instanceData);
	vsapi->setVideoInfo(d->vi_, 1, node);
}

void VS_CC
snmFree(void * instanceData, VSCore * core, const VSAPI * vsapi) {
	SangNom2 * d = static_cast<SangNom2*>(instanceData);
	// This can cause deadlock under Linux. FIXME
	// vsapi->freeNode(d->node);
	delete d;
}

const VSFrameRef*VS_CC
snmGetFrame(int n, int activationReason, void ** instanceData, void ** frameData, VSFrameContext * frameCtx, VSCore * core, const VSAPI * vsapi) {
	SangNom2 * d = *reinterpret_cast<SangNom2**>(instanceData);
	IScriptEnvironment env(frameCtx, core, vsapi, d->node_);
	if (activationReason == arInitial) {
		d->GetFramePre(n, &env);
		return nullptr;
	}
	if (activationReason != arAllFramesReady) {
		return nullptr;
	}

	return d->GetFrame(n, &env);
}

#ifdef _MSC_VER
#define snprintf(a,b,c)  _snprintf_s(a,b,b,c)
#endif

#define FAIL_IF_ERROR(cond, ...) {\
	if (cond) {\
		snprintf(msg, 200, __VA_ARGS__);\
		vsapi->setError(out, msg_buff);\
		return;\
	}\
}

static void VS_CC
Create_SangNomMod(const VSMap * in, VSMap * out, void * userData, VSCore * core, const VSAPI * vsapi) {
	int err;
	char msg_buff[256] = "SangNom2: ";
	char * msg = msg_buff + strlen(msg_buff);

	VSNodeRef * node = vsapi->propGetNode(in, "clip", 0, nullptr);
	VSVideoInfo * vi = new VSVideoInfo;
	*vi = *vsapi->getVideoInfo(node);

	FAIL_IF_ERROR(!vi->format || vi->width == 0 || vi->height == 0,
		"clip must be constant format");

	FAIL_IF_ERROR(vi->width & 15,
		"width must be mod 16");

	FAIL_IF_ERROR(vi->format->sampleType != stInteger ||
		vi->format->bitsPerSample != 8 ||
		(vi->format->colorFamily != cmYUV && vi->format->colorFamily != cmGray),
		"SangNom2 works only with 8bit planar colorspaces");

	FAIL_IF_ERROR(vi->width < 16,
		"width must be bigger or equal to 16");

	PARAM_INT(order, 1);
	PARAM_INT(aa, 48);
	PARAM_INT(aac, 0);
	SangNom2 * d = new SangNom2(vi, node, order, aa, aac);

	vsapi->createFilter(in, out, "SangNomMod", snmInit, snmGetFrame, snmFree, fmParallel, 0, d, core);
}

VS_EXTERNAL_API(void)
VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin * plugin) {
	configFunc("in.7086.sangnom", "sangnom",
	           "VapourSynth SangNomMod Filter v0.1",
	           VAPOURSYNTH_API_VERSION, 1, plugin);
	registerFunc("SangNomMod", "clip:clip;order:int:opt;aa:int:opt;aac:int:opt;", Create_SangNomMod, nullptr, plugin);
}
