/*
  gaussian_blur_sse2.c: Copyright (C) 2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This file is part of Generic.

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


#include <stdlib.h>
#include "sse2.h"
#include "canny.h"


static void GF_FUNC_ALIGN VS_CC
convert_to_float_8bit(int radius, float *kernel, const uint8_t *srcp,
                      float *buff, float *dstp, int width, int height,
                      int src_stride, int dst_stride)
{
    __m128i zero = _mm_setzero_si128();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i xmm0 = _mm_load_si128((__m128i *)(srcp + x));
            __m128i xmm1 = _mm_unpackhi_epi8(xmm0, zero);
            xmm0 = _mm_unpacklo_epi8(xmm0, zero);
            __m128 f0 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(xmm0, zero));
            __m128 f1 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(xmm0, zero));
            __m128 f2 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(xmm1, zero));
            __m128 f3 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(xmm1, zero));
            
            _mm_store_ps(dstp + x , f0);
            _mm_store_ps(dstp + x + 4, f1);
            _mm_store_ps(dstp + x + 8, f2);
            _mm_store_ps(dstp + x + 12, f3);
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void GF_FUNC_ALIGN VS_CC
convert_to_float_16bit(int radius, float *kernel, const uint8_t *s,
                       float *buff, float *dstp, int width, int height,
                       int src_stride, int dst_stride)
{
    const uint16_t *srcp = (uint16_t *)s;
    src_stride /= 2;
    __m128i zero = _mm_setzero_si128();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            __m128i xmm0 = _mm_load_si128((__m128i *)(srcp + x));
            __m128 f0 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(xmm0, zero));
            __m128 f1 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(xmm0, zero));
            
            _mm_store_ps(dstp + x , f0);
            _mm_store_ps(dstp + x + 4, f1);
        }
        srcp += src_stride;
        dstp += dst_stride;
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_horizontal(float *srcp, int radius, int length, int width, float *kernel,
                float *dstp)
{
    for (int i = 1; i <= radius; i++) {
        srcp[-i] = srcp[i];
        srcp[width - 1 + i] = srcp[width - 1 - i];
    }
    
    GF_ALIGN float ar_kernel[17][4];
    for (int i = 0; i < length; i++) {
        for (int j = 0; j < 4; j++) {
            ar_kernel[i][j] = kernel[i];
        }
    }
    
    for (int x = 0; x < width; x += 4) {
        __m128 sum = _mm_setzero_ps();
        
        for (int i = -radius; i <= radius; i++) {
            __m128 k = _mm_load_ps(ar_kernel[i + radius]);
            __m128 xmm0 = _mm_loadu_ps(srcp + x + i);
            sum = _mm_add_ps(sum, _mm_mul_ps(xmm0, k));
        }
        _mm_store_ps(dstp + x, sum);
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_8bit(int radius, float *kernel, const uint8_t *srcp, float *buff,
          float *dstp, int width, int height, int src_stride, int dst_stride)
{
    int length = radius * 2 + 1;
    const uint8_t *p[17];
    for (int i = -radius; i <= radius; i++) {
        p[i + radius] = srcp + abs(i) * src_stride;
    }

    __m128i zero = _mm_setzero_si128();
    __m128 zerof = _mm_castsi128_ps(zero);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128 sum[4] = {zerof, zerof, zerof, zerof};
            
            for (int i = 0; i < length; i++) {
                __m128 f[4];
                __m128i xmm0 = _mm_load_si128((__m128i *)(p[i] + x));
                __m128i xmm1 = _mm_unpackhi_epi8(xmm0, zero);
                xmm0 = _mm_unpacklo_epi8(xmm0, zero);
                f[0] = _mm_cvtepi32_ps(_mm_unpacklo_epi16(xmm0, zero));
                f[1] = _mm_cvtepi32_ps(_mm_unpackhi_epi16(xmm0, zero));
                f[2] = _mm_cvtepi32_ps(_mm_unpacklo_epi16(xmm1, zero));
                f[3] = _mm_cvtepi32_ps(_mm_unpackhi_epi16(xmm1, zero));
                __m128 k = _mm_set1_ps(kernel[i]);
                
                for (int j = 0; j < 4; j++) {
                    sum[j] = _mm_add_ps(sum[j], _mm_mul_ps(k, f[j]));
                }
            }
            _mm_store_ps(buff + x,      sum[0]);
            _mm_store_ps(buff + x +  4, sum[1]);
            _mm_store_ps(buff + x +  8, sum[2]);
            _mm_store_ps(buff + x + 12, sum[3]);
        }
        proc_horizontal(buff, radius, length, width, kernel, dstp);
        
        for (int i = 0; i < length - 1; i++) {
            p[i] = p[i + 1];
        }
        p[length - 1] += (y < height - radius - 1 ? 1 : -1) * src_stride;
        dstp += dst_stride;
    }
}


static void GF_FUNC_ALIGN VS_CC
proc_16bit(int radius, float *kernel, const uint8_t *srcp, float *buff,
           float *dstp, int width, int height, int src_stride, int dst_stride)
{
    int length = radius * 2 + 1;
    src_stride /= 2;
    const uint16_t *p[17];
    for (int i = -radius; i <= radius; i++) {
        p[i + radius] =(uint16_t *)srcp + abs(i) * src_stride;
    }
    
    __m128i zero = _mm_setzero_si128();
    __m128 zerof = _mm_castsi128_ps(zero);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            __m128 sum0 = zerof, sum1 = zerof;
            
            for (int i = 0; i < length; i++) {
                __m128i xmm0 = _mm_load_si128((__m128i *)(p[i] + x));
                __m128 f0 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(xmm0, zero));
                __m128 f1 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(xmm0, zero));
                __m128 k = _mm_set1_ps(kernel[i]);
                sum0 = _mm_add_ps(sum0, _mm_mul_ps(f0, k));
                sum1 = _mm_add_ps(sum1, _mm_mul_ps(f1, k));
            }
            _mm_store_ps(buff + x, sum0);
            _mm_store_ps(buff + x + 4, sum1);
        }
        proc_horizontal(buff, radius, length, width, kernel, dstp);
        
        for (int i = 0; i < length - 1; i++) {
            p[i] = p[i + 1];
        }
        p[length - 1] += (y < height - radius - 1 ? 1 : -1) * src_stride;
        dstp += dst_stride;
    }
}


const proc_gblur gblur[] = {
    proc_8bit, proc_16bit
};


const proc_gblur convert_to_float[] = {
    convert_to_float_8bit, convert_to_float_16bit
};
