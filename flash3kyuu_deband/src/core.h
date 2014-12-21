#pragma once

#include <f3kdb.h>
#include "process_plane_context.h"
#include "compiler_compat.h"

typedef struct _pixel_dither_info {
    alignas(4) signed char ref1;
    signed char ref2;
    signed short change;
} pixel_dither_info;

static_assert(sizeof(pixel_dither_info) == 4, "Something wrong in pixel_dither_info");

typedef struct _process_plane_params
{
    const unsigned char *src_plane_ptr;
    int src_pitch;

    unsigned char *dst_plane_ptr;
    int dst_pitch;

    int plane_width_in_pixels;
    int plane_height_in_pixels;

    PIXEL_MODE input_mode;
    int input_depth;
    PIXEL_MODE output_mode;
    int output_depth;

    unsigned short threshold;
    pixel_dither_info *info_ptr_base;
    int info_stride;
    
    short* grain_buffer;
    int grain_buffer_stride;

    int plane;

    unsigned char width_subsampling;
    unsigned char height_subsampling;
    
    int pixel_max;
    int pixel_min;
    
    // Helper functions
    inline int get_dst_width() const {
        return output_mode == HIGH_BIT_DEPTH_INTERLEAVED ? plane_width_in_pixels * 2 : plane_width_in_pixels;
    }
    inline int get_dst_height() const {
        return output_mode == HIGH_BIT_DEPTH_STACKED ? plane_height_in_pixels * 2 : plane_height_in_pixels;
    }
    inline int get_src_width() const {
        return input_mode == HIGH_BIT_DEPTH_INTERLEAVED ? plane_width_in_pixels * 2 : plane_width_in_pixels;
    }
    inline int get_src_height() const {
        return input_mode == HIGH_BIT_DEPTH_STACKED ? plane_height_in_pixels * 2 : plane_height_in_pixels;
    }
} process_plane_params;

typedef void (*process_plane_impl_t)(const process_plane_params& params, process_plane_context* context);

class f3kdb_core_t {
private:
    process_plane_impl_t _process_plane_impl;
        
    pixel_dither_info *_y_info;
    pixel_dither_info *_cb_info;
    pixel_dither_info *_cr_info;
    
    process_plane_context _y_context;
    process_plane_context _cb_context;
    process_plane_context _cr_context;
    
    short* _grain_buffer_y;
    short* _grain_buffer_c;

    int* _grain_buffer_offsets;

    f3kdb_video_info_t _video_info;
    f3kdb_params_t _params;

    void init(void);
    void init_frame_luts(void);

    void destroy_frame_luts(void);

    f3kdb_core_t(const f3kdb_core_t&);
    f3kdb_core_t operator=(const f3kdb_core_t&);
    
public:
    f3kdb_core_t(const f3kdb_video_info_t* video_info, const f3kdb_params_t* params);
    virtual ~f3kdb_core_t();

    int process_plane(int frame_index, int plane, unsigned char* dst_frame_ptr, int dst_pitch, const unsigned char* src_frame_ptr, int src_pitch);
};