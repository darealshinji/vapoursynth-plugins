
#include "impl_dispatch.h"
#include "compiler_compat.h"

#define FS_DITHER_SKIP_PRE_CLAMP

#include "pixel_proc_c_high_f_s_dithering.h"
#include "pixel_proc_c_high_ordered_dithering.h"

#include <assert.h>
#include <emmintrin.h>

namespace dither_high
{
    static __m128i _ordered_dithering_threshold_map[16] [2];
    static __m128i _ordered_dithering_threshold_map_yuy2[16] [8];
    static volatile bool _threshold_map_initialized = false;

    static __inline void init_ordered_dithering()
    {
        if (UNLIKELY(!_threshold_map_initialized)) {
            __m128i threhold_row;
            __m128i zero = _mm_setzero_si128();
            for (int i = 0; i < 16; i++) 
            {
                threhold_row = *(__m128i*)pixel_proc_high_ordered_dithering::THRESHOLD_MAP[i];
                    
                __m128i part_0 = _mm_unpacklo_epi8(threhold_row, zero);
                __m128i part_1 = _mm_unpackhi_epi8(threhold_row, zero);

                if (INTERNAL_BIT_DEPTH < 16)
                {
                    part_0 = _mm_srli_epi16(part_0, 16 - INTERNAL_BIT_DEPTH);
                    part_1 = _mm_srli_epi16(part_1, 16 - INTERNAL_BIT_DEPTH);
                }
                _ordered_dithering_threshold_map[i][0] = part_0;
                _ordered_dithering_threshold_map[i][1] = part_1;
                
                __m128i tmp = _mm_unpacklo_epi8(part_0, part_0);
                _ordered_dithering_threshold_map_yuy2[i][0] = _mm_unpacklo_epi16(part_0, tmp);
                _ordered_dithering_threshold_map_yuy2[i][1] = _mm_unpackhi_epi16(part_0, tmp);

                tmp = _mm_unpackhi_epi8(part_0, part_0);
                _ordered_dithering_threshold_map_yuy2[i][2] = _mm_unpacklo_epi16(part_1, tmp);
                _ordered_dithering_threshold_map_yuy2[i][3] = _mm_unpackhi_epi16(part_1, tmp);

                tmp = _mm_unpacklo_epi8(part_1, part_1);
                _ordered_dithering_threshold_map_yuy2[i][4] = _mm_unpacklo_epi16(part_0, tmp);
                _ordered_dithering_threshold_map_yuy2[i][5] = _mm_unpackhi_epi16(part_0, tmp);

                tmp = _mm_unpackhi_epi8(part_1, part_1);
                _ordered_dithering_threshold_map_yuy2[i][6] = _mm_unpacklo_epi16(part_1, tmp);
                _ordered_dithering_threshold_map_yuy2[i][7] = _mm_unpackhi_epi16(part_1, tmp);
            }
            _mm_mfence();
            _threshold_map_initialized = true;
        }
    }

    static void init_ordered_dithering_with_output_depth(char context_buffer[CONTEXT_BUFFER_SIZE], int output_depth)
    {
        assert(_threshold_map_initialized);

        __m128i shift = _mm_set_epi32(0, 0, 0, output_depth - 8);

        for (int i = 0; i < 16; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                __m128i item = _ordered_dithering_threshold_map[i][j];
                item = _mm_srl_epi16(item, shift);
                _mm_store_si128((__m128i*)(context_buffer + (i * 2 + j) * 16), item);
            }
        }
    }

    template <int dither_algo>
    static __inline void init(char context_buffer[CONTEXT_BUFFER_SIZE], int frame_width, int output_depth) 
    {
        if (dither_algo == DA_HIGH_FLOYD_STEINBERG_DITHERING)
        {
            pixel_proc_high_f_s_dithering::init_context(context_buffer, frame_width, output_depth);
        } else if (dither_algo == DA_HIGH_ORDERED_DITHERING) {
            init_ordered_dithering();
            init_ordered_dithering_with_output_depth(context_buffer, output_depth);
        }
    }

    template <int dither_algo>
    static __inline void complete(void* context) 
    {
        if (dither_algo == DA_HIGH_FLOYD_STEINBERG_DITHERING)
        {
            pixel_proc_high_f_s_dithering::destroy_context(context);
        }
    }
    
    template <int dither_algo>
    static __forceinline __m128i dither(void* context, __m128i pixels, int row, int column)
    {
        switch (dither_algo)
        {
        case DA_HIGH_NO_DITHERING:
            return pixels;
        case DA_HIGH_ORDERED_DITHERING:
            {
            // row: use lowest 4 bits as index, mask = 0b00001111 = 15
            // column: always multiples of 8, so use 8 (bit 4) as selector, mask = 0b00001000
            assert((column & 7) == 0);
            __m128i threshold = _mm_load_si128((__m128i*)((char*)context + ( ( (row & 15) * 2 ) + ( (column & 8) >> 3 ) ) * 16 ) );
            return _mm_adds_epu16(pixels, threshold);
            }
        case DA_HIGH_FLOYD_STEINBERG_DITHERING:
            // due to an ICC bug, accessing pixels using union will give us incorrect results
            // so we have to use a buffer here
            // tested on ICC 12.0.1024.2010
            alignas(16)
            unsigned short buffer[8];
            _mm_store_si128((__m128i*)buffer, pixels);
            __PRAGMA_NOUNROLL__
            for (int i = 0; i < 8; i++)
            {
                buffer[i] = (unsigned short)pixel_proc_high_f_s_dithering::dither(context, buffer[i], row, column + i);
                pixel_proc_high_f_s_dithering::next_pixel(context);
            }
            return _mm_load_si128((__m128i*)buffer);
        case DA_16BIT_STACKED:
        case DA_16BIT_INTERLEAVED:
            return _mm_setzero_si128();
            break;
        default:
            abort();
            return _mm_setzero_si128();
        }
    }

    template <int dither_algo>
    static __forceinline __m128i dither_yuy2(char contexts[3][CONTEXT_BUFFER_SIZE], __m128i pixels, int row, int column)
    {
        switch (dither_algo)
        {
        case DA_HIGH_NO_DITHERING:
            return pixels;
        case DA_HIGH_ORDERED_DITHERING:
            // row: use lowest 4 bits as index, mask = 0b00001111 = 15
            // column: always multiples of 8, yuy2 threshold map has 8 items, mask = 0b00111000
            assert((column & 7) == 0);
            return _mm_adds_epu16(pixels, _ordered_dithering_threshold_map_yuy2[row & 15][(column >> 3) & 7]);
        case DA_HIGH_FLOYD_STEINBERG_DITHERING:
            // due to an ICC bug, accessing pixels using union will give us incorrect results
            // so we have to use a buffer here
            // tested on ICC 12.0.1024.2010
            alignas(16)
            unsigned short buffer[8];
            _mm_store_si128((__m128i*)buffer, pixels);
            for (int i = 0; i < 8; i++)
            {
                int cur_column = column + i;
                void *cur_context;
                switch (i & 3)
                {
                case 0:
                case 2:
                    cur_column >>= 1;
                    cur_context = contexts[0];
                    break;
                case 1:
                    cur_column >>= 2;
                    cur_context = contexts[1];
                    break;
                case 3:
                    cur_column >>= 2;
                    cur_context = contexts[2];
                    break;
                }
                buffer[i] = (unsigned short)pixel_proc_high_f_s_dithering::dither(cur_context, buffer[i], row, cur_column);
                pixel_proc_high_f_s_dithering::next_pixel(cur_context);
            }
            return _mm_load_si128((__m128i*)buffer);
        case DA_16BIT_STACKED:
        case DA_16BIT_INTERLEAVED:
            return _mm_setzero_si128();
            break;
        default:
            abort();
            return _mm_setzero_si128();
        }
    }
    
    template <int dither_algo>
    static __inline void next_row(void* context)
    {
        if (dither_algo == DA_HIGH_FLOYD_STEINBERG_DITHERING)
        {
            pixel_proc_high_f_s_dithering::next_row(context);
        }
    }
};