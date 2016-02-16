#include <memory.h>
#include <assert.h>
#include <exception>
#include <stdio.h>
#include <stdarg.h>
#include <functional>
#include <string>

#include "compiler_compat.h"
#include "core.h"
#include "auto_utils.h"
#include "constants.h"
#include "impl_dispatch.h"
#include "presets.h"

using namespace std;

F3KDB_API(int) f3kdb_params_init_defaults(f3kdb_params_t* params, int interface_version)
{
    if (interface_version != F3KDB_INTERFACE_VERSION)
    {
        return F3KDB_ERROR_INVALID_INTERFACE_VERSION;
    }
    memset(params, 0, sizeof(f3kdb_params_t));
    params_set_defaults(params);
    return F3KDB_SUCCESS;
}

static int parse_param_string(const char* params, bool has_name, function<int (const char*, const char*)> item_callback)
{
    string param_string(params);
    string name_str, value_str;
    size_t pos = 0;
    for (size_t i = 0; i < param_string.size() + 1; i++)
    {
        switch (param_string[i])
        {
        case '\0':
        case ',':
        case ':':
        case '/':
            value_str = param_string.substr(pos, i - pos);
            if (has_name && name_str.empty() && !value_str.empty())
            {
                return F3KDB_ERROR_UNEXPECTED_END;
            }
            // Fall below
            if (!value_str.empty() || !name_str.empty()) {
                auto ret = item_callback(name_str.c_str(), value_str.c_str());
                if (ret != F3KDB_SUCCESS)
                {
                    return ret;
                }
            }
            name_str.clear();
            value_str.clear();
            pos = i + 1;
            break;
        case '=':
            if (has_name && name_str.empty())
            {
                name_str = param_string.substr(pos, i - pos);
                if (name_str.empty())
                {
                    return F3KDB_ERROR_INVALID_NAME;
                }
                pos = i + 1;
            }
            break;
        default:
            // Nothing to do
            break;
        }
    }
    return F3KDB_SUCCESS;
}

F3KDB_API(int) f3kdb_params_fill_preset(f3kdb_params_t* params, const char* preset, int interface_version)
{
    if (interface_version != F3KDB_INTERFACE_VERSION)
    {
        return F3KDB_ERROR_INVALID_INTERFACE_VERSION;
    }
    if (!params || !preset)
    {
        return F3KDB_ERROR_INVALID_ARGUMENT;
    }
    return parse_param_string(preset, false, [params](const char*, const char* preset_item) -> int {
        for (size_t i = 0; i < sizeof(PRESETS) / sizeof(PRESETS[0]) - 2; i += 2)
        {
            if (!_stricmp(preset_item, PRESETS[i]))
            {
                return f3kdb_params_fill_by_string(params, PRESETS[i + 1]);
            }
        }
        return (int)F3KDB_ERROR_INVALID_NAME;
    });
}

F3KDB_API(int) f3kdb_params_fill_by_string(f3kdb_params_t* params, const char* param_string, int interface_version)
{
    if (interface_version != F3KDB_INTERFACE_VERSION)
    {
        return F3KDB_ERROR_INVALID_INTERFACE_VERSION;
    }
    return parse_param_string(param_string, true, [=](const char* name, const char* value) {
        return params_set_by_string(params, name, value);
    });
}

static void sanitize_mode_and_depth(PIXEL_MODE* mode, int* depth) {
    if (*mode == DEFAULT_PIXEL_MODE)
    {
        *mode = *depth <= 8 ? LOW_BIT_DEPTH : HIGH_BIT_DEPTH_STACKED;
    }
    if (*depth == -1)
    {
        *depth = *mode == LOW_BIT_DEPTH ? 8 : 16;
    }
}

F3KDB_API(int) f3kdb_params_sanitize(f3kdb_params_t* params, int interface_version)
{
    if (interface_version != F3KDB_INTERFACE_VERSION)
    {
        return F3KDB_ERROR_INVALID_INTERFACE_VERSION;
    }

    sanitize_mode_and_depth(&params->output_mode, &params->output_depth);

    return F3KDB_SUCCESS;
}

F3KDB_API(int) f3kdb_video_info_sanitize(f3kdb_video_info_t* vi, int interface_version)
{
    if (interface_version != F3KDB_INTERFACE_VERSION)
    {
        return F3KDB_ERROR_INVALID_INTERFACE_VERSION;
    }

    sanitize_mode_and_depth(&vi->pixel_mode, &vi->depth);

    return F3KDB_SUCCESS;
}

static void print_error(char* buffer, size_t buffer_size, const char* format, ...)
{
    if (!buffer || buffer_size <= 0)
    {
        return;
    }
    va_list va;
    va_start(va, format);
    vsnprintf(buffer, buffer_size, format, va);
}

static inline bool is_out_of_range(int value, int lower_bound, int upper_bound)
{
    // Suppress -Wtype-limits
    return value < lower_bound || value > upper_bound;
}

F3KDB_API(int) f3kdb_create(const f3kdb_video_info_t* video_info_in, const f3kdb_params_t* params_in, f3kdb_core_t** core_out, char* extra_error_msg, int error_msg_size, int interface_version)
{
    if (interface_version != F3KDB_INTERFACE_VERSION)
    {
        return F3KDB_ERROR_INVALID_INTERFACE_VERSION;
    }
    *core_out = nullptr;
    if (extra_error_msg && error_msg_size > 0)
    {
        extra_error_msg[0] = 0;
    }

#define INVALID_PARAM_IF(cond) \
    do { if (cond) { print_error(extra_error_msg, error_msg_size, "Invalid parameter condition: %s", #cond); return F3KDB_ERROR_INVALID_ARGUMENT; } } while (0)

    INVALID_PARAM_IF(!video_info_in);
    INVALID_PARAM_IF(!params_in);

    f3kdb_video_info_t video_info;
    memcpy(&video_info, video_info_in, sizeof(f3kdb_video_info_t));
    f3kdb_video_info_sanitize(&video_info);

    INVALID_PARAM_IF(video_info.width < 16);
    INVALID_PARAM_IF(video_info.height < 16);
    INVALID_PARAM_IF(video_info.chroma_width_subsampling < 0 || video_info.chroma_width_subsampling > 4);
    INVALID_PARAM_IF(video_info.chroma_height_subsampling < 0 || video_info.chroma_height_subsampling > 4);
    INVALID_PARAM_IF(video_info.num_frames <= 0);
    INVALID_PARAM_IF(video_info.depth < 8 || video_info.depth > INTERNAL_BIT_DEPTH);
    INVALID_PARAM_IF(video_info.pixel_mode < 0 || video_info.pixel_mode >= PIXEL_MODE_COUNT);
    INVALID_PARAM_IF(video_info.pixel_mode == LOW_BIT_DEPTH && video_info.depth != 8);
    INVALID_PARAM_IF(video_info.pixel_mode != LOW_BIT_DEPTH && video_info.depth == 8);

    f3kdb_params_t params;
    memcpy(&params, params_in, sizeof(f3kdb_params_t));

    f3kdb_params_sanitize(&params);

    if (params.output_depth == 8 && params.output_mode != LOW_BIT_DEPTH)
    {
        print_error(extra_error_msg, error_msg_size, "%s", "output_mode > 0 is only valid when output_depth > 8");
        return F3KDB_ERROR_INVALID_ARGUMENT;
    }
    if (params.output_depth > 8 && params.output_mode == LOW_BIT_DEPTH)
    {
        print_error(extra_error_msg, error_msg_size, "%s", "output_mode = 0 is only valid when output_depth = 8");
        return F3KDB_ERROR_INVALID_ARGUMENT;
    }
    if (params.output_depth == 16)
    {
        // set to appropriate precision mode
        switch (params.output_mode)
        {
        case HIGH_BIT_DEPTH_INTERLEAVED:
            params.dither_algo = DA_16BIT_INTERLEAVED;
            break;
        case HIGH_BIT_DEPTH_STACKED:
            params.dither_algo = DA_16BIT_STACKED;
            break;
        default:
            // something is wrong here!
            assert(false);
            return F3KDB_ERROR_INVALID_STATE;
        }
    }

    int threshold_upper_limit = 64 * 8 - 1;
    int dither_upper_limit = 4096;

#define CHECK_PARAM(value, lower_bound, upper_bound) \
    do { if (is_out_of_range(params.value, lower_bound, upper_bound)) { print_error(extra_error_msg, error_msg_size, "Invalid parameter %s, must be between %d and %d", #value, lower_bound, upper_bound); return F3KDB_ERROR_INVALID_ARGUMENT; } } while(0)

    CHECK_PARAM(range, 0, 31);
    CHECK_PARAM(Y, 0, threshold_upper_limit);
    CHECK_PARAM(Cb, 0, threshold_upper_limit);
    CHECK_PARAM(Cr, 0, threshold_upper_limit);
    CHECK_PARAM(grainY, 0, dither_upper_limit);
    CHECK_PARAM(grainC, 0, dither_upper_limit);
    CHECK_PARAM(sample_mode, 1, 2);
    CHECK_PARAM(opt, IMPL_AUTO_DETECT, (IMPL_COUNT - 1) );
    CHECK_PARAM(dither_algo, DA_HIGH_NO_DITHERING, (DA_COUNT - 1) );
    CHECK_PARAM(random_algo_ref, 0, (RANDOM_ALGORITHM_COUNT - 1) );
    CHECK_PARAM(random_algo_grain, 0, (RANDOM_ALGORITHM_COUNT - 1) );
    CHECK_PARAM(output_mode, 0, PIXEL_MODE_COUNT - 1);
    

    if (params.output_mode != LOW_BIT_DEPTH)
    {
        CHECK_PARAM(output_depth, 9, INTERNAL_BIT_DEPTH);
    }

    // now the internal bit depth is 16, 
    // scale parameters to be consistent with 14bit range in previous versions
    params.Y <<= 2;
    params.Cb <<= 2;
    params.Cr <<= 2;
    params.grainY <<= 2;
    params.grainC <<= 2;

    try
    {
        *core_out = new f3kdb_core_t(&video_info, &params);
    } catch (std::bad_alloc&) {
        return F3KDB_ERROR_INSUFFICIENT_MEMORY;
    }
    return F3KDB_SUCCESS;
}

F3KDB_API(int) f3kdb_destroy(f3kdb_core_t* core)
{
    delete core;
    return F3KDB_SUCCESS;
}

F3KDB_API(int) f3kdb_process_plane(f3kdb_core_t* core, int frame_index, int plane, unsigned char* dst_frame_ptr, int dst_pitch, const unsigned char* src_frame_ptr, int src_pitch)
{
    if (!core)
    {
        return F3KDB_ERROR_INVALID_ARGUMENT;
    }
    return core->process_plane(frame_index, plane, dst_frame_ptr, dst_pitch, src_frame_ptr, src_pitch);
}
