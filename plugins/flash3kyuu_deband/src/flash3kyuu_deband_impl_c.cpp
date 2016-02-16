#include "stdafx.h"

// we intend the C version to be usable on most CPUs
#define NO_SSE

#include <cstdlib>
#include "core.h"
#include "pixel_proc_c.h"
#include <limits.h>
#include "debug_dump.h"

static inline bool _is_above_threshold(int threshold, int diff) {
    return abs(diff) >= threshold;
}

static inline bool is_above_threshold(int threshold, int diff1) {
    return _is_above_threshold(threshold, diff1);
}

static inline bool is_above_threshold(int threshold, int diff1, int diff2) {
    return _is_above_threshold(threshold, diff1) ||
           _is_above_threshold(threshold, diff2);
}

static inline bool is_above_threshold(int threshold, int diff1, int diff2, int diff3, int diff4) {
    return _is_above_threshold(threshold, diff1) ||
           _is_above_threshold(threshold, diff2) ||
           _is_above_threshold(threshold, diff3) ||
           _is_above_threshold(threshold, diff4);
}

template <int mode>
static __inline int read_pixel(const process_plane_params& params, void* context, const unsigned char* base, int offset = 0)
{
    const unsigned char* ptr = base + offset;
    if (params.input_mode == LOW_BIT_DEPTH)
    {
        return pixel_proc_upsample<mode>(context, *ptr);
    }

    int ret;

    switch (params.input_mode)
    {
    case HIGH_BIT_DEPTH_STACKED:
        ret = *ptr << 8 | *(ptr + params.plane_height_in_pixels * params.src_pitch);
        break;
    case HIGH_BIT_DEPTH_INTERLEAVED:
        ret = *(unsigned short*)ptr;
        break;
    default:
        // shouldn't happen!
        abort();
        return 0;
    }

    ret <<= (INTERNAL_BIT_DEPTH - params.input_depth);
    return ret;
}

template <int sample_mode, bool blur_first, int mode, int output_mode>
static __forceinline void __cdecl process_plane_plainc_mode12_high(const process_plane_params& params, process_plane_context*)
{
    pixel_dither_info* info_ptr;
    char context[CONTEXT_BUFFER_SIZE];

    unsigned short threshold = params.threshold;

    int pixel_min = params.pixel_min;
    int pixel_max = params.pixel_max;

    int width_subsamp = params.width_subsampling;

    pixel_proc_init_context<mode>(context, params.plane_width_in_pixels, params.output_depth);

    int pixel_step = params.input_mode == HIGH_BIT_DEPTH_INTERLEAVED ? 2 : 1;

    int process_width = params.plane_width_in_pixels;

    DUMP_INIT("c", params.plane, process_width);



    for (int i = 0; i < params.plane_height_in_pixels; i++)
    {
        const unsigned char* src_px = params.src_plane_ptr + params.src_pitch * i;
        unsigned char* dst_px = params.dst_plane_ptr + params.dst_pitch * i;

        const short* grain_buffer_ptr = params.grain_buffer + params.grain_buffer_stride * i;

        info_ptr = params.info_ptr_base + params.info_stride * i;


        for (int j = 0; j < process_width; j++)
        {
            pixel_dither_info info = *info_ptr;
            int src_px_up = read_pixel<mode>(params, context, src_px);

            DUMP_VALUE("src_px_up", src_px_up);
            
            assert(info.ref1 >= 0);
            assert((info.ref1 >> params.height_subsampling) <= i && 
                   (info.ref1 >> params.height_subsampling) + i < params.plane_height_in_pixels);

            
            DUMP_VALUE("ref1", info.ref1);

            int avg;
            bool use_org_px_as_base;
            int ref_pos, ref_pos_2;
            if (sample_mode == 1)
            {
                ref_pos = (info.ref1 >> params.height_subsampling) * params.src_pitch;
                
                DUMP_VALUE("ref_pos", ref_pos);

                int ref_1_up = read_pixel<mode>(params, context, src_px, ref_pos);
                int ref_2_up = read_pixel<mode>(params, context, src_px, -ref_pos);
                
                DUMP_VALUE("ref_1_up", ref_1_up);
                DUMP_VALUE("ref_2_up", ref_2_up);

                avg = pixel_proc_avg_2<mode>(context, ref_1_up, ref_2_up);

                if (blur_first)
                {
                    int diff = avg - src_px_up;
                    use_org_px_as_base = is_above_threshold(threshold, diff);
                } else {
                    int diff = src_px_up - ref_1_up;
                    int diff_n = src_px_up - ref_2_up;
                    use_org_px_as_base = is_above_threshold(threshold, diff, diff_n);
                }
            } else {
                int x_multiplier = 1;
                
                assert(info.ref2 >= 0);
                assert((info.ref2 >> params.height_subsampling) <= i && 
                       (info.ref2 >> params.height_subsampling) + i < params.plane_height_in_pixels);
                assert(((info.ref1 >> width_subsamp) * x_multiplier) <= j && 
                       ((info.ref1 >> width_subsamp) * x_multiplier) + j < process_width);
                assert(((info.ref2 >> width_subsamp) * x_multiplier) <= j && 
                       ((info.ref2 >> width_subsamp) * x_multiplier) + j < process_width);

                
                DUMP_VALUE("ref2", info.ref2);

                ref_pos = params.src_pitch * (info.ref2 >> params.height_subsampling) + 
                          ((info.ref1 * x_multiplier) >> width_subsamp) * pixel_step;

                ref_pos_2 = ((info.ref2 * x_multiplier) >> width_subsamp) * pixel_step - 
                            params.src_pitch * (info.ref1 >> params.height_subsampling);

                
                DUMP_VALUE("ref_pos", ref_pos);
                DUMP_VALUE("ref_pos_2", ref_pos_2);

                int ref_1_up = read_pixel<mode>(params, context, src_px, ref_pos);
                int ref_2_up = read_pixel<mode>(params, context, src_px, ref_pos_2);
                int ref_3_up = read_pixel<mode>(params, context, src_px, -ref_pos);
                int ref_4_up = read_pixel<mode>(params, context, src_px, -ref_pos_2);

                
                DUMP_VALUE("ref_1_up", ref_1_up);
                DUMP_VALUE("ref_2_up", ref_2_up);
                DUMP_VALUE("ref_3_up", ref_3_up);
                DUMP_VALUE("ref_4_up", ref_4_up);

                avg = pixel_proc_avg_4<mode>(context, ref_1_up, ref_2_up, ref_3_up, ref_4_up);

                if (blur_first)
                {
                    int diff = avg - src_px_up;
                    use_org_px_as_base = is_above_threshold(threshold, diff);
                } else {
                    int diff1 = ref_1_up - src_px_up;
                    int diff2 = ref_2_up - src_px_up;
                    int diff3 = ref_3_up - src_px_up;
                    int diff4 = ref_4_up - src_px_up;
                    use_org_px_as_base = is_above_threshold(threshold, diff1, diff2, diff3, diff4);
                }
            }
            
            DUMP_VALUE("avg", avg);
            DUMP_VALUE("change", *grain_buffer_ptr);

            int new_pixel;

            if (use_org_px_as_base) {
                new_pixel = src_px_up + *grain_buffer_ptr;
            } else {
                new_pixel = avg + *grain_buffer_ptr;
            }
            
            DUMP_VALUE("new_pixel_before_downsample", new_pixel);

            new_pixel = pixel_proc_downsample<mode>(context, new_pixel, i, j, pixel_min, pixel_max, params.output_depth);
            
            DUMP_VALUE("dst", new_pixel);
            switch (output_mode)
            {
            case LOW_BIT_DEPTH:
                *dst_px = (unsigned char)new_pixel;
                break;
            case HIGH_BIT_DEPTH_STACKED:
                *dst_px = (unsigned char)((new_pixel >> 8) & 0xFF);
                *(dst_px + params.plane_height_in_pixels * params.dst_pitch) = (unsigned char)(new_pixel & 0xFF);
                break;
            case HIGH_BIT_DEPTH_INTERLEAVED:
                *((unsigned short*)dst_px) = (unsigned short)(new_pixel & 0xFFFF);
                dst_px++;
                break;
            default:
                abort();
            }

            src_px += pixel_step;
            dst_px++;
            info_ptr++;
            grain_buffer_ptr++;
            pixel_proc_next_pixel<mode>(context);
        }
        pixel_proc_next_row<mode>(context);
        DUMP_NEXT_LINE();
    }

    DUMP_FINISH();

    pixel_proc_destroy_context<mode>(context);
}

template <int sample_mode, bool blur_first, int mode>
void __cdecl process_plane_plainc(const process_plane_params& params, process_plane_context* context)
{
    static_assert(sample_mode != 0, "No longer support sample_mode = 0");
    switch (params.output_mode)
    {
    case LOW_BIT_DEPTH:
        process_plane_plainc_mode12_high<sample_mode, blur_first, mode, LOW_BIT_DEPTH>(params, context);
        break;

    case HIGH_BIT_DEPTH_STACKED:
        process_plane_plainc_mode12_high<sample_mode, blur_first, mode, HIGH_BIT_DEPTH_STACKED>(params, context);
        break;

    case HIGH_BIT_DEPTH_INTERLEAVED:
        process_plane_plainc_mode12_high<sample_mode, blur_first, mode, HIGH_BIT_DEPTH_INTERLEAVED>(params, context);
        break;

    default:
        abort();
    }
}

#define DECLARE_IMPL_C
#include "impl_dispatch_decl.h"
