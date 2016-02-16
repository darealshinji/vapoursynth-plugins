#include <stdlib.h>

#include "impl_dispatch.h"
#include "sse_compat.h"
#include "sse_utils.h"
#include "dither_high.h"
#include "x64_compat.h"
#include "debug_dump.h"

/****************************************************************************
 * NOTE: DON'T remove static from any function in this file, it is required *
 *       for generating code in multiple SSE versions.                      *
 ****************************************************************************/

typedef struct _info_cache
{
    int pitch;
    char* data_stream;
} info_cache;

static void destroy_cache(void* data)
{
    assert(data);

    info_cache* cache = (info_cache*) data;
    _aligned_free(cache->data_stream);
    free(data);
}

#ifdef ENABLE_DEBUG_DUMP

static void __forceinline _dump_value_group(const TCHAR* name, __m128i part1, bool is_signed=false)
{
    DUMP_VALUE_S(name, part1, 2, is_signed);
}

#define DUMP_VALUE_GROUP(name, ...) _dump_value_group(TEXT(name), __VA_ARGS__)

#else

#define DUMP_VALUE_GROUP(name, ...) ((void)0)

#endif


template <int sample_mode, int ref_part_index>
static __forceinline void process_plane_info_block(
    pixel_dither_info *&info_ptr, 
    const unsigned char* src_addr_start, 
    const __m128i &src_pitch_vector, 
    const __m128i &minus_one, 
    const __m128i &width_subsample_vector,
    const __m128i &height_subsample_vector,
    const __m128i &pixel_step_shift_bits,
    char*& info_data_stream)
{
    assert(ref_part_index <= 2);

    __m128i info_block = _mm_load_si128((__m128i*)info_ptr);

    // ref1: bit 0-7
    // left-shift & right-shift 24bits to remove other elements and preserve sign
    __m128i ref1 = info_block;
    ref1 = _mm_slli_epi32(ref1, 24); // << 24
    ref1 = _mm_srai_epi32(ref1, 24); // >> 24

    DUMP_VALUE("ref1", ref1, 4, true);

    __m128i ref_offset1;
    __m128i ref_offset2;

    __m128i temp_ref1;
    switch (sample_mode)
    {
    case 0:
        // ref1 = (abs(ref1) >> height_subsampling) * (sign(ref1))
        temp_ref1 = _mm_abs_epi32(ref1);
        temp_ref1 = _mm_sra_epi32(temp_ref1, height_subsample_vector);
        temp_ref1 = _cmm_mullo_limit16_epi32(temp_ref1, _mm_srai_epi32(ref1, 31));
        ref_offset1 = _cmm_mullo_limit16_epi32(src_pitch_vector, temp_ref1); // packed DWORD multiplication
        DUMP_VALUE("ref_pos", ref_offset1, 4, true);
        break;
    case 1:
        // ref1 is guarenteed to be postive
        temp_ref1 = _mm_sra_epi32(ref1, height_subsample_vector);
        ref_offset1 = _cmm_mullo_limit16_epi32(src_pitch_vector, temp_ref1); // packed DWORD multiplication
        DUMP_VALUE("ref_pos", ref_offset1, 4, true);

        ref_offset2 = _cmm_negate_all_epi32(ref_offset1, minus_one); // negates all offsets
        break;
    case 2:
        // ref2: bit 8-15
        // similar to above
        __m128i ref2;
        ref2 = info_block;
        ref2 = _mm_slli_epi32(ref2, 16); // << 16
        ref2 = _mm_srai_epi32(ref2, 24); // >> 24

        __m128i ref1_fix, ref2_fix;
        // ref_px = src_pitch * info.ref2 + info.ref1;
        ref1_fix = _mm_sra_epi32(ref1, width_subsample_vector);
        ref2_fix = _mm_sra_epi32(ref2, height_subsample_vector);
        ref_offset1 = _cmm_mullo_limit16_epi32(src_pitch_vector, ref2_fix); // packed DWORD multiplication
        ref_offset1 = _mm_add_epi32(ref_offset1, _mm_sll_epi32(ref1_fix, pixel_step_shift_bits));
        DUMP_VALUE("ref_pos", ref_offset1, 4, true);

        // ref_px_2 = info.ref2 - src_pitch * info.ref1;
        ref1_fix = _mm_sra_epi32(ref1, height_subsample_vector);
        ref2_fix = _mm_sra_epi32(ref2, width_subsample_vector);
        ref_offset2 = _cmm_mullo_limit16_epi32(src_pitch_vector, ref1_fix); // packed DWORD multiplication
        ref_offset2 = _mm_sub_epi32(_mm_sll_epi32(ref2_fix, pixel_step_shift_bits), ref_offset2);
        DUMP_VALUE("ref_pos_2", ref_offset2, 4, true);
        break;
    default:
        abort();
    }

    if (info_data_stream){
        _mm_store_si128((__m128i*)info_data_stream, ref_offset1);
        info_data_stream += 16;

        if (sample_mode == 2) {
            _mm_store_si128((__m128i*)info_data_stream, ref_offset2);
            info_data_stream += 16;
        }
    }

    info_ptr += 4;
}

static __forceinline __m128i generate_blend_mask_high(__m128i a, __m128i b, __m128i threshold)
{
    __m128i diff1 = _mm_subs_epu16(a, b);
    __m128i diff2 = _mm_subs_epu16(b, a);

    __m128i abs_diff = _mm_or_si128(diff1, diff2);

    __m128i sign_convert_vector = _mm_set1_epi16( (short)0x8000 );

    __m128i converted_diff = _mm_sub_epi16(abs_diff, sign_convert_vector);

    __m128i converted_threshold = _mm_sub_epi16(threshold, sign_convert_vector);

    // mask: if threshold >= diff, set to 0xff, otherwise 0x00
    // note that this is the opposite of low bitdepth implementation
    return _mm_cmpgt_epi16(converted_threshold, converted_diff);
}


template<int sample_mode, bool blur_first>
static __m128i __forceinline process_pixels_mode12_high_part(__m128i src_pixels, __m128i threshold_vector, __m128i change, const __m128i& ref_pixels_1, const __m128i& ref_pixels_2, const __m128i& ref_pixels_3, const __m128i& ref_pixels_4)
{	
    __m128i use_orig_pixel_blend_mask;
    __m128i avg;

    if (!blur_first)
    {
        use_orig_pixel_blend_mask = generate_blend_mask_high(src_pixels, ref_pixels_1, threshold_vector);

        // note: use AND instead of OR, because two operands are reversed
        // (different from low bit-depth mode!)
        use_orig_pixel_blend_mask = _mm_and_si128(
            use_orig_pixel_blend_mask, 
            generate_blend_mask_high(src_pixels, ref_pixels_2, threshold_vector) );
    }

    avg = _mm_avg_epu16(ref_pixels_1, ref_pixels_2);

    if (sample_mode == 2)
    {
        if (!blur_first)
        {
            use_orig_pixel_blend_mask = _mm_and_si128(
                use_orig_pixel_blend_mask, 
                generate_blend_mask_high(src_pixels, ref_pixels_3, threshold_vector) );

            use_orig_pixel_blend_mask = _mm_and_si128(
                use_orig_pixel_blend_mask, 
                generate_blend_mask_high(src_pixels, ref_pixels_4, threshold_vector) );
        }

        avg = _mm_subs_epu16(avg, _mm_set1_epi16(1));
        avg = _mm_avg_epu16(avg, _mm_avg_epu16(ref_pixels_3, ref_pixels_4));

    }

    if (blur_first)
    {
        use_orig_pixel_blend_mask = generate_blend_mask_high(src_pixels, avg, threshold_vector);
    }
    
    DUMP_VALUE("avg", avg, 2, false);

    // if mask is 0xff (NOT over threshold), select second operand, otherwise select first
    // note this is different from low bitdepth code
    __m128i dst_pixels;

    dst_pixels = _cmm_blendv_by_cmp_mask_epi8(src_pixels, avg, use_orig_pixel_blend_mask);
    
    __m128i sign_convert_vector = _mm_set1_epi16((short)0x8000);

    // convert to signed form, since change is signed
    dst_pixels = _mm_sub_epi16(dst_pixels, sign_convert_vector);

    // saturated add
    dst_pixels = _mm_adds_epi16(dst_pixels, change);

    // convert back to unsigned
    dst_pixels = _mm_add_epi16(dst_pixels, sign_convert_vector);
    return dst_pixels;
}

template<int sample_mode, bool blur_first, int dither_algo>
static __m128i __forceinline process_pixels(
    __m128i src_pixels_0, 
    __m128i threshold_vector, 
    const __m128i& change_1, 
    const __m128i& ref_pixels_1_0,
    const __m128i& ref_pixels_2_0,
    const __m128i& ref_pixels_3_0,
    const __m128i& ref_pixels_4_0,
    const __m128i& clamp_high_add,
    const __m128i& clamp_high_sub,
    const __m128i& clamp_low,
    bool need_clamping,
    int row,
    int column,
    void* dither_context)
{
    __m128i ret = process_pixels_mode12_high_part<sample_mode, blur_first>
        (src_pixels_0, 
         threshold_vector, 
         change_1, 
         ref_pixels_1_0, 
         ref_pixels_2_0, 
         ref_pixels_3_0, 
         ref_pixels_4_0);
    
    DUMP_VALUE_GROUP("new_pixel_before_downsample", ret);

    
    switch (dither_algo)
    {
    case DA_HIGH_NO_DITHERING:
    case DA_HIGH_ORDERED_DITHERING:
    case DA_HIGH_FLOYD_STEINBERG_DITHERING:
        ret = dither_high::dither<dither_algo>(dither_context, ret, row, column);
        break;
    default:
        break;
    }
    if (need_clamping)
    {
        ret = high_bit_depth_pixels_clamp(ret, clamp_high_add, clamp_high_sub, clamp_low);
    }


    return ret;
}

template <PIXEL_MODE output_mode>
static int __forceinline store_pixels(
    __m128i pixels,
    __m128i downshift_bits,
    unsigned char* dst,
    int dst_pitch,
    int height_in_pixels)
{
    switch (output_mode)
    {
    case LOW_BIT_DEPTH:
        {
            pixels = _mm_srli_epi16(pixels, 8);
            pixels = _mm_packus_epi16(pixels, pixels);
            _mm_storel_epi64((__m128i*)dst, pixels);
            return 8;
            break;
        }
    case HIGH_BIT_DEPTH_STACKED:
        {
            pixels = _mm_srl_epi16(pixels, downshift_bits);
            __m128i msb = _mm_srli_epi16(pixels, 8);
            msb = _mm_packus_epi16(msb, msb);
            _mm_storel_epi64((__m128i*)dst, msb);

            __m128i mask = _mm_set1_epi16(0x00ff);
            __m128i lsb = _mm_and_si128(pixels, mask);
            lsb = _mm_packus_epi16(lsb, lsb);
            _mm_storel_epi64((__m128i*)(dst + dst_pitch * height_in_pixels), lsb);
            return 8;
        }
        break;
    case HIGH_BIT_DEPTH_INTERLEAVED:
        pixels = _mm_srl_epi16(pixels, downshift_bits);
        _mm_store_si128((__m128i*)dst, pixels);
        return 16;
        break;
    default:
        abort();
    }
    return 0;
}


template<bool aligned>
static __m128i load_m128(const unsigned char *ptr)
{
    if (aligned)
    {
        return _mm_load_si128((const __m128i*)ptr);
    } else {
        return _mm_loadu_si128((const __m128i*)ptr);
    }
}

template<PIXEL_MODE input_mode, bool aligned>
static __m128i __forceinline read_pixels(
    const process_plane_params& params,
    const unsigned char *ptr, 
    __m128i upsample_shift)
{
    __m128i ret;

    switch (input_mode)
    {
    case LOW_BIT_DEPTH:
        {
            __m128i zero = _mm_setzero_si128();
            ret = _mm_loadl_epi64((__m128i*)ptr);
            ret = _mm_unpacklo_epi8(zero, ret);
            return ret;
        }
        break;
    case HIGH_BIT_DEPTH_STACKED:
        ret = _mm_unpacklo_epi8(
            _mm_loadl_epi64((__m128i*)(ptr + params.plane_height_in_pixels * params.src_pitch)),
            _mm_loadl_epi64((__m128i*)ptr));
        break;
    case HIGH_BIT_DEPTH_INTERLEAVED:
        ret = load_m128<aligned>(ptr);
        break;
    default:
        abort();
    }
    ret = _mm_sll_epi16(ret, upsample_shift);
    return ret;
}

template<PIXEL_MODE input_mode>
static unsigned short __forceinline read_pixel(
    int plane_height_in_pixels,
    int src_pitch,
    const unsigned char* base,
    int offset)
{
    const unsigned char* ptr = base + offset;

    switch (input_mode)
    {
    case LOW_BIT_DEPTH:
        return *ptr;
        break;
    case HIGH_BIT_DEPTH_STACKED:
        return *ptr << 8 | *(ptr + plane_height_in_pixels * src_pitch);
        break;
    case HIGH_BIT_DEPTH_INTERLEAVED:
        return *(unsigned short*)ptr;
        break;
    default:
        // shouldn't happen!
        abort();
        return 0;
    }

}

template <int dither_algo>
static __m128i __forceinline load_reference_pixels(
    __m128i shift,
    const unsigned short src[8])
{
    __m128i ret = _mm_load_si128((const __m128i*)src);
    ret = _mm_sll_epi16(ret, shift);
    return ret;
}


template<int sample_mode, int dither_algo, PIXEL_MODE input_mode>
static void __forceinline read_reference_pixels(
    const process_plane_params& params,
    __m128i shift,
    const unsigned char* src_px_start,
    const char* info_data_start,
    __m128i& ref_pixels_1_0,
    __m128i& ref_pixels_2_0,
    __m128i& ref_pixels_3_0,
    __m128i& ref_pixels_4_0)
{
    alignas(16)
    unsigned short tmp_1[8];
    alignas(16)
    unsigned short tmp_2[8];
    alignas(16)
    unsigned short tmp_3[8];
    alignas(16)
    unsigned short tmp_4[8];

    // cache layout: 8 offset groups (1 or 2 offsets / group depending on sample mode) in a pack, 
    //               followed by 16 bytes of change values
    // in the case of 2 offsets / group, offsets are stored like this:
    // [1 1 1 1 
    //  2 2 2 2
    //  1 1 1 1
    //  2 2 2 2
    //  .. snip
    //  1 1 1 1
    //  2 2 2 2]

    int plane_height_in_pixels = params.plane_height_in_pixels;
    int src_pitch = params.src_pitch;

    int i_fix = 0;
    int i_fix_step = (input_mode != HIGH_BIT_DEPTH_INTERLEAVED ? 1 : 2);
    
    for (int i = 0; i < 8; i++)
    {
        switch (sample_mode)
        {
        case 0:
            tmp_1[i] = read_pixel<input_mode>(plane_height_in_pixels, src_pitch, src_px_start, i_fix + *(int*)(info_data_start + 4 * i));
            break;
        case 1:
            tmp_1[i] = read_pixel<input_mode>(plane_height_in_pixels, src_pitch, src_px_start, i_fix + *(int*)(info_data_start + 4 * i));
            tmp_2[i] = read_pixel<input_mode>(plane_height_in_pixels, src_pitch, src_px_start, i_fix + -*(int*)(info_data_start + 4 * i));
            break;
        case 2:
            tmp_1[i] = read_pixel<input_mode>(plane_height_in_pixels, src_pitch, src_px_start, i_fix + *(int*)(info_data_start + 4 * (i + i / 4 * 4)));
            tmp_2[i] = read_pixel<input_mode>(plane_height_in_pixels, src_pitch, src_px_start, i_fix + *(int*)(info_data_start + 4 * (i + i / 4 * 4 + 4)));
            tmp_3[i] = read_pixel<input_mode>(plane_height_in_pixels, src_pitch, src_px_start, i_fix + -*(int*)(info_data_start + 4 * (i + i / 4 * 4)));
            tmp_4[i] = read_pixel<input_mode>(plane_height_in_pixels, src_pitch, src_px_start, i_fix + -*(int*)(info_data_start + 4 * (i + i / 4 * 4 + 4)));
            break;
        }
        i_fix += i_fix_step;
    }

    switch (sample_mode)
    {
    case 0:
        ref_pixels_1_0 = load_reference_pixels<dither_algo>(shift, tmp_1);
        break;
    case 1:
        ref_pixels_1_0 = load_reference_pixels<dither_algo>(shift, tmp_1);
        ref_pixels_2_0 = load_reference_pixels<dither_algo>(shift, tmp_2);
        break;
    case 2:
        ref_pixels_1_0 = load_reference_pixels<dither_algo>(shift, tmp_1);
        ref_pixels_2_0 = load_reference_pixels<dither_algo>(shift, tmp_2);
        ref_pixels_3_0 = load_reference_pixels<dither_algo>(shift, tmp_3);
        ref_pixels_4_0 = load_reference_pixels<dither_algo>(shift, tmp_4);
        break;
    }
}


template<int sample_mode, bool blur_first, int dither_algo, bool aligned, PIXEL_MODE output_mode>
static void __cdecl _process_plane_sse_impl(const process_plane_params& params, process_plane_context* context)
{
    assert(sample_mode > 0);

    DUMP_INIT("sse", params.plane, params.plane_width_in_pixels);

    pixel_dither_info* info_ptr = params.info_ptr_base;

    __m128i src_pitch_vector = _mm_set1_epi32(params.src_pitch);
           
    __m128i threshold_vector = _mm_set1_epi16(params.threshold);

    // general-purpose constant
    __m128i minus_one = _mm_set1_epi32(-1);

    alignas(16)
    char context_buffer[DITHER_CONTEXT_BUFFER_SIZE];

    dither_high::init<dither_algo>(context_buffer, params.plane_width_in_pixels, params.output_depth);

    
    __m128i width_subsample_vector = _mm_set_epi32(0, 0, 0, params.width_subsampling);
    __m128i height_subsample_vector = _mm_set_epi32(0, 0, 0, params.height_subsampling);

    bool need_clamping =  INTERNAL_BIT_DEPTH < 16 || 
                          params.pixel_min > 0 || 
                          params.pixel_max < 0xffff;
    __m128i clamp_high_add = _mm_setzero_si128();
    __m128i clamp_high_sub = _mm_setzero_si128();
    __m128i clamp_low = _mm_setzero_si128();
    if (need_clamping)
    {
        clamp_low = _mm_set1_epi16((short)params.pixel_min);
        clamp_high_add = _mm_sub_epi16(_mm_set1_epi16((short)0xffff), _mm_set1_epi16((short)params.pixel_max));
        clamp_high_sub = _mm_add_epi16(clamp_high_add, clamp_low);
    }
    
    __m128i pixel_step_shift_bits;
    __m128i upsample_to_16_shift_bits;

    if (params.input_mode == HIGH_BIT_DEPTH_INTERLEAVED)
    {
        pixel_step_shift_bits = _mm_set_epi32(0, 0, 0, 1);
    } else {
        pixel_step_shift_bits = _mm_setzero_si128();
    }
    upsample_to_16_shift_bits = _mm_set_epi32(0, 0, 0, 16 - params.input_depth);

    __m128i downshift_bits = _mm_set_epi32(0, 0, 0, 16 - params.output_depth);

    bool use_cached_info = false;
    info_cache *cache = NULL;
    char* info_data_stream = NULL;

    alignas(16)
    char dummy_info_buffer[128];

    // initialize storage for pre-calculated pixel offsets
    if (context->data) {
        cache = (info_cache*) context->data;
        // we need to ensure src_pitch is the same, otherwise offsets will be completely wrong
        // also, if pitch changes, don't waste time to update the cache since it is likely to change again
        if (cache->pitch == params.src_pitch) {
            info_data_stream = cache->data_stream;
            use_cached_info = true;
        } else {
            // info_data_stream can be NULL, in this case dummy_info_buffer will be used for temporary storage
        }
        cache = NULL;
    } else {
        // set up buffer for cache
        cache = (info_cache*)malloc(sizeof(info_cache));
        // 4 offsets (2 bytes per item) + 2-byte change
        info_data_stream = (char*)_aligned_malloc(params.info_stride * (4 * 2 + 2) * params.get_src_height(), FRAME_LUT_ALIGNMENT);
        cache->data_stream = info_data_stream;
        cache->pitch = params.src_pitch;
    }

    const int info_cache_block_size = (sample_mode == 2 ? 64 : 32);

    int input_mode = params.input_mode;

    for (int row = 0; row < params.plane_height_in_pixels; row++)
    {
        const unsigned char* src_px = params.src_plane_ptr + params.src_pitch * row;
        unsigned char* dst_px = params.dst_plane_ptr + params.dst_pitch * row;

        info_ptr = params.info_ptr_base + params.info_stride * row;

        const short* grain_buffer_ptr = params.grain_buffer + params.grain_buffer_stride * row;

        int processed_pixels = 0;

        while (processed_pixels < params.plane_width_in_pixels)
        {
            __m128i change_1;
            
            __m128i ref_pixels_1_0;
            __m128i ref_pixels_2_0;
            __m128i ref_pixels_3_0;
            __m128i ref_pixels_4_0;

#define READ_REFS(data_stream, inp_mode) read_reference_pixels<sample_mode, dither_algo, inp_mode>( \
                    params, \
                    upsample_to_16_shift_bits, \
                    src_px, \
                    data_stream, \
                    ref_pixels_1_0, \
                    ref_pixels_2_0, \
                    ref_pixels_3_0, \
                    ref_pixels_4_0)

            char * data_stream_block_start;

            if (LIKELY(use_cached_info)) {
                data_stream_block_start = info_data_stream;
                info_data_stream += info_cache_block_size;
            } else {
                // we need to process the info block

                char * data_stream_ptr = info_data_stream;
                if (!data_stream_ptr)
                {
                    data_stream_ptr = dummy_info_buffer;
                }

                data_stream_block_start = data_stream_ptr;
            
    #define PROCESS_INFO_BLOCK(n) \
                process_plane_info_block<sample_mode, n>(info_ptr, src_px, src_pitch_vector, minus_one, width_subsample_vector, height_subsample_vector, pixel_step_shift_bits, data_stream_ptr);
            
                PROCESS_INFO_BLOCK(0);
                PROCESS_INFO_BLOCK(1);

    #undef PROCESS_INFO_BLOCK
                
                if (info_data_stream) {
                    info_data_stream += info_cache_block_size;
                    assert(info_data_stream == data_stream_ptr);
                }

            }

            __m128i src_pixels;
            // abuse the guard bytes on the end of frame, as long as they are present there won't be segfault
            // garbage data is not a problem
            if (LIKELY(input_mode == LOW_BIT_DEPTH))
            {
                READ_REFS(data_stream_block_start, LOW_BIT_DEPTH);
                src_pixels = read_pixels<LOW_BIT_DEPTH, aligned>(params, src_px, upsample_to_16_shift_bits);
            } else if (input_mode == HIGH_BIT_DEPTH_INTERLEAVED)
            {
                READ_REFS(data_stream_block_start, HIGH_BIT_DEPTH_INTERLEAVED);
                src_pixels = read_pixels<HIGH_BIT_DEPTH_INTERLEAVED, aligned>(params, src_px, upsample_to_16_shift_bits);
            } else if (input_mode == HIGH_BIT_DEPTH_STACKED)
            {
                READ_REFS(data_stream_block_start, HIGH_BIT_DEPTH_STACKED);
                src_pixels = read_pixels<HIGH_BIT_DEPTH_STACKED, aligned>(params, src_px, upsample_to_16_shift_bits);
            } else {
                abort();
                return;
            }

            change_1 = _mm_load_si128((__m128i*)grain_buffer_ptr);
            
            DUMP_VALUE_GROUP("change", change_1, true);
            DUMP_VALUE_GROUP("ref_1_up", ref_pixels_1_0);
            DUMP_VALUE_GROUP("ref_2_up", ref_pixels_2_0);
            DUMP_VALUE_GROUP("ref_3_up", ref_pixels_3_0);
            DUMP_VALUE_GROUP("ref_4_up", ref_pixels_4_0);

            DUMP_VALUE_GROUP("src_px_up", src_pixels);

            __m128i dst_pixels = process_pixels<sample_mode, blur_first, dither_algo>(
                                     src_pixels, 
                                     threshold_vector,
                                     change_1, 
                                     ref_pixels_1_0, 
                                     ref_pixels_2_0, 
                                     ref_pixels_3_0, 
                                     ref_pixels_4_0, 
                                     clamp_high_add, 
                                     clamp_high_sub, 
                                     clamp_low, 
                                     need_clamping, 
                                     row, 
                                     processed_pixels, 
                                     context_buffer);

            dst_px += store_pixels<output_mode>(dst_pixels, downshift_bits, dst_px, params.dst_pitch, params.plane_height_in_pixels);
            processed_pixels += 8;
            src_px += params.input_mode != HIGH_BIT_DEPTH_INTERLEAVED ? 8 : 16;
            grain_buffer_ptr += 8;
        }
        DUMP_NEXT_LINE();
        dither_high::next_row<dither_algo>(context_buffer);
    }
    
    dither_high::complete<dither_algo>(context_buffer);

    // for thread-safety, save context after all data is processed
    if (!use_cached_info && !context->data && cache)
    {
        context->destroy = destroy_cache;
        if (InterlockedCompareExchangePointer(&context->data, cache, NULL) != NULL)
        {
            // other thread has completed first, so we can destroy our copy
            destroy_cache(cache);
        }
    }

    DUMP_FINISH();
}


template<int sample_mode, bool blur_first, int dither_algo, bool aligned>
static void process_plane_sse_impl_stub1(const process_plane_params& params, process_plane_context* context)
{
    switch (params.output_mode)
    {
    case LOW_BIT_DEPTH:
        _process_plane_sse_impl<sample_mode, blur_first, dither_algo, aligned, LOW_BIT_DEPTH>(params, context);
        break;
    case HIGH_BIT_DEPTH_STACKED:
        _process_plane_sse_impl<sample_mode, blur_first, dither_algo, aligned, HIGH_BIT_DEPTH_STACKED>(params, context);
        break;
    case HIGH_BIT_DEPTH_INTERLEAVED:
        _process_plane_sse_impl<sample_mode, blur_first, dither_algo, aligned, HIGH_BIT_DEPTH_INTERLEAVED>(params, context);
        break;
    default:
        abort();
    }
}

template<int sample_mode, bool blur_first, int dither_algo>
static void __cdecl process_plane_sse_impl(const process_plane_params& params, process_plane_context* context)
{
    if ( ( (POINTER_INT)params.src_plane_ptr & (PLANE_ALIGNMENT - 1) ) == 0 && (params.src_pitch & (PLANE_ALIGNMENT - 1) ) == 0 )
    {
        process_plane_sse_impl_stub1<sample_mode, blur_first, dither_algo, true>(params, context);
    } else {
        process_plane_sse_impl_stub1<sample_mode, blur_first, dither_algo, false>(params, context);
    }
}