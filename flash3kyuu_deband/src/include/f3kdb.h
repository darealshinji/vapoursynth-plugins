#pragma once

#include "f3kdb_enums.h"
#include "f3kdb_params.h"

// Input plane can be unaligned, but all output planes need to be aligned to 16-byte boundary
static const int PLANE_ALIGNMENT = 16;

enum {
    // Supposed to be the same as corresponding values in avisynth
    PLANE_Y = 1<<0,
    PLANE_CB = 1<<1,
    PLANE_CR = 1<<2
};
typedef struct _f3kdb_video_info_t
{
    // width and height are in actual pixels, NOT pixels after applying hacks
    int width;
    int height;

    int chroma_width_subsampling;
    int chroma_height_subsampling;

    PIXEL_MODE pixel_mode;
    int depth;


    // Can be set to an estimated number if unknown when initializing
    int num_frames;

    inline int get_plane_width(int plane)
    {
        return plane == PLANE_Y ? width : width >> chroma_width_subsampling;
    }

    inline int get_plane_height(int plane)
    {
        return plane == PLANE_Y ? height : height >> chroma_height_subsampling;
    }
} f3kdb_video_info_t;

static const int F3KDB_INTERFACE_VERSION = 2 << 16 | sizeof(f3kdb_params_t) << 8 | sizeof(f3kdb_video_info_t);

class f3kdb_core_t;

enum
{
    F3KDB_SUCCESS = 0,
    F3KDB_ERROR_INVALID_INTERFACE_VERSION,
    F3KDB_ERROR_INSUFFICIENT_MEMORY,
    F3KDB_ERROR_INVALID_ARGUMENT,
    F3KDB_ERROR_INVALID_STATE,
    F3KDB_ERROR_INVALID_NAME,
    F3KDB_ERROR_INVALID_VALUE,
    F3KDB_ERROR_VALUE_OUT_OF_RANGE,
    F3KDB_ERROR_NOT_IMPLEMENTED,
    F3KDB_ERROR_UNEXPECTED_END,

    F3KDB_ERROR_MAX
};

// Stolen from VapourSynth
// Convenience for C++ users.
#ifdef __cplusplus
# define F3KDB_EXTERN_C extern "C"
#else
# define F3KDB_EXTERN_C
#endif

#if defined(_WIN32) && !defined(_WIN64)
# define F3KDB_CC __stdcall
#else
# define F3KDB_CC
#endif

// And now for some symbol hide-and-seek...
#if defined(_WIN32) // Windows being special
# define F3KDB_EXTERNAL_API(ret) F3KDB_EXTERN_C __declspec(dllexport) ret F3KDB_CC
#elif defined(__GNUC__) && __GNUC__ >= 4
# define F3KDB_EXTERNAL_API(ret) F3KDB_EXTERN_C __attribute__((visibility("default"))) ret F3KDB_CC
#else
# define F3KDB_EXTERNAL_API(ret) F3KDB_EXTERN_C ret F3KDB_CC
#endif

#if !defined(FLASH3KYUU_DEBAND_EXPORTS) && defined(_WIN32) && !defined(F3KDB_STATIC) && !defined(__GNUC__)
# define F3KDB_API(ret) F3KDB_EXTERN_C __declspec(dllimport) ret F3KDB_CC
#else
# define F3KDB_API(ret) F3KDB_EXTERNAL_API(ret)
#endif

F3KDB_API(int) f3kdb_params_init_defaults(f3kdb_params_t* params, int interface_version = F3KDB_INTERFACE_VERSION);
F3KDB_API(int) f3kdb_params_fill_by_string(f3kdb_params_t* params, const char* param_string, int interface_version = F3KDB_INTERFACE_VERSION);
F3KDB_API(int) f3kdb_params_fill_preset(f3kdb_params_t* params, const char* preset, int interface_version = F3KDB_INTERFACE_VERSION);
F3KDB_API(int) f3kdb_params_sanitize(f3kdb_params_t* params, int interface_version = F3KDB_INTERFACE_VERSION);
F3KDB_API(int) f3kdb_video_info_sanitize(f3kdb_video_info_t* vi, int interface_version = F3KDB_INTERFACE_VERSION);
F3KDB_API(int) f3kdb_create(const f3kdb_video_info_t* video_info, const f3kdb_params_t* params, f3kdb_core_t** core_out, char* extra_error_msg = nullptr, int error_msg_size = 0, int interface_version = F3KDB_INTERFACE_VERSION);
F3KDB_API(int) f3kdb_destroy(f3kdb_core_t* core);
F3KDB_API(int) f3kdb_process_plane(f3kdb_core_t* core, int frame_index, int plane, unsigned char* dst_frame_ptr, int dst_pitch, const unsigned char* src_frame_ptr, int src_pitch);
