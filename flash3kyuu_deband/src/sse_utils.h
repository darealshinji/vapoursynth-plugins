#pragma once

// See Intel Optimization Guide: Ch. 5.6.6.2 Clipping to an Arbitrary Unsigned Range [High, Low]
// high_add = 0xffff - high
// high_sub = 0xffff - high + low
static __m128i __forceinline high_bit_depth_pixels_clamp(__m128i pixels, __m128i high_add, __m128i high_sub, const __m128i& low)
{
    pixels = _mm_adds_epu16(pixels, high_add);
    pixels = _mm_subs_epu16(pixels, high_sub);
    pixels = _mm_add_epi16(pixels, low);

    return pixels;
}


// like high_bit_depth_pixels_clamp, but all values are 8bit
static __m128i __forceinline low_bit_depth_pixels_clamp(__m128i pixels, __m128i high_add, __m128i high_sub, const __m128i& low)
{
    pixels = _mm_adds_epu8(pixels, high_add);
    pixels = _mm_subs_epu8(pixels, high_sub);
    pixels = _mm_add_epi8(pixels, low);

    return pixels;
}
