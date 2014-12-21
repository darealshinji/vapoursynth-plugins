#include "stdafx.h"

#include <intrin.h>
#include <process.h>
#include <windows.h>
#include <assert.h>
#include <exception>

#define F3KDB_SIMPLE_MACRO_NAME

#include "filter.h"
#include "check.hpp"

AVSValue __cdecl Create_flash3kyuu_deband(AVSValue args, void* user_data, IScriptEnvironment* env){
    PClip child = ARG(child).AsClip();
    const VideoInfo& vi = child->GetVideoInfo();
    check_video_format("f3kdb", vi, env);

    SYSTEM_INFO si;
    memset(&si, 0, sizeof(si));
    GetSystemInfo(&si);
    bool mt = ARG(mt).AsBool(si.dwNumberOfProcessors > 1);

    f3kdb_params_t params;
    f3kdb_params_init_defaults(&params);

    if (F3KDB_ARG(preset).Defined()) {
        int result = f3kdb_params_fill_preset(&params, F3KDB_ARG(preset).AsString());
        if (result != F3KDB_SUCCESS) {
            env->ThrowError("f3kdb: Invalid preset (code: %d)", result);
        }
    }

    f3kdb_params_from_avs(args, &params);
    f3kdb_params_sanitize(&params);

    f3kdb_video_info_t video_info;
    video_info.num_frames = vi.num_frames;
    video_info.pixel_mode = (PIXEL_MODE)ARG(input_mode).AsInt(DEFAULT_PIXEL_MODE);
    video_info.depth = ARG(input_depth).AsInt(-1);
    video_info.chroma_width_subsampling  = vi.IsY8() ? 0 : vi.GetPlaneWidthSubsampling(PLANAR_U);
    video_info.chroma_height_subsampling = vi.IsY8() ? 0 : vi.GetPlaneHeightSubsampling(PLANAR_U);
    f3kdb_video_info_sanitize(&video_info);

    video_info.width = vi.width;
    if (video_info.pixel_mode == HIGH_BIT_DEPTH_INTERLEAVED)
    {
        int width_mod = 2 << video_info.chroma_width_subsampling;
        if (video_info.width % width_mod != 0)
        {
            env->ThrowError("f3kdb: The clip does not appear to be an interleaved high bit depth clip. (width MOD)");
        }
        video_info.width /= 2;
    }

    video_info.height = vi.height;
    if (video_info.pixel_mode == HIGH_BIT_DEPTH_STACKED)
    {
        int height_mod = 2 << video_info.chroma_height_subsampling;
        if (video_info.height % height_mod != 0)
        {
            env->ThrowError("f3kdb: The clip does not appear to be an stacked high bit depth clip. (height MOD)");
        }
        video_info.height /= 2;
    }

    f3kdb_core_t* core = NULL;
    char error_msg[1024];
    memset(error_msg, 0, sizeof(error_msg));
    int result = f3kdb_create(&video_info, &params, &core, error_msg, sizeof(error_msg) - 1);
    if (result != F3KDB_SUCCESS)
    {
        env->ThrowError("f3kdb: Initialization failed (code: %d). %s", result, error_msg);
    }

    int dst_width = video_info.width;
    if (params.output_mode == HIGH_BIT_DEPTH_INTERLEAVED)
    {
        dst_width *= 2;
    }

    int dst_height = video_info.height;
    if (params.output_mode == HIGH_BIT_DEPTH_STACKED)
    {
        dst_height *= 2;
    }
    
    return new f3kdb_avisynth(child, core, dst_width, dst_height, mt);
}
f3kdb_avisynth::f3kdb_avisynth(PClip child, f3kdb_core_t* core, int dst_width, int dst_height, bool mt) :
            GenericVideoFilter(child),
            _mt_info(NULL),
            _core(core)
{
    vi.width = dst_width;
    vi.height = dst_height;
}

f3kdb_avisynth::~f3kdb_avisynth()
{
    mt_info_destroy(_mt_info);
    _mt_info = NULL;
    f3kdb_destroy(_core);
    _core = NULL;
}

void f3kdb_avisynth::process_plane(int n, PVideoFrame src, PVideoFrame dst, unsigned char *dstp, int plane, IScriptEnvironment* env)
{
    int f3kdb_plane;
    switch (plane & 7)
    {
    case PLANAR_Y:
        f3kdb_plane = PLANE_Y;
        break;
    case PLANAR_U:
        f3kdb_plane = PLANE_CB;
        break;
    case PLANAR_V:
        f3kdb_plane = PLANE_CR;
        break;
    default:
        assert(false);
        env->ThrowError("f3kdb: Invalid plane");
    }
    int result = f3kdb_process_plane(_core, n, f3kdb_plane, dstp, dst->GetPitch(plane), src->GetReadPtr(plane), src->GetPitch(plane));
    if (result != F3KDB_SUCCESS)
    {
        env->ThrowError("f3kdb: Unknown error, code = %d", result);
    }
}

void f3kdb_avisynth::mt_proc(void)
{
    volatile mt_info* info = _mt_info;
    assert(info);

    while (!info->exit)
    {
        process_plane(info->n, info->src, info->dst, info->dstp_u, PLANAR_U, info->env);
        process_plane(info->n, info->src, info->dst, info->dstp_v, PLANAR_V, info->env);

        mt_info_reset_pointers(info);

        SetEvent(info->work_complete_event);
        WaitForSingleObject(info->work_event, INFINITE);
    }
}

static unsigned int __stdcall mt_proc_wrapper(void* filter_instance) 
{
    assert(filter_instance);
    ((f3kdb_avisynth*)filter_instance)->mt_proc();
    return 0;
}

PVideoFrame __stdcall f3kdb_avisynth::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    // interleaved 16bit output needs extra alignment
    PVideoFrame dst = env->NewVideoFrame(vi, PLANE_ALIGNMENT * 2);

    if (vi.IsPlanar() && !vi.IsY8())
    {
        if (!_mt) 
        {
            process_plane(n, src, dst, dst->GetWritePtr(PLANAR_Y), PLANAR_Y, env);
            process_plane(n, src, dst, dst->GetWritePtr(PLANAR_U), PLANAR_U, env);
            process_plane(n, src, dst, dst->GetWritePtr(PLANAR_V), PLANAR_V, env);
        } else {
            bool new_thread = _mt_info == NULL;
            if (new_thread)
            {
                _mt_info = mt_info_create();
                if (!_mt_info) {
                    env->ThrowError("f3kdb_avisynth: Failed to allocate mt_info.");
                    return NULL;
                }
            }
            // we must get write pointer before copying the frame pointer
            // otherwise NULL will be returned
            unsigned char* dstp_y = dst->GetWritePtr(PLANAR_Y);
            _mt_info->n = n;
            _mt_info->dstp_u = dst->GetWritePtr(PLANAR_U);
            _mt_info->dstp_v = dst->GetWritePtr(PLANAR_V);
            _mt_info->env = env;
            _mt_info->dst = dst;
            _mt_info->src = src;
            if (!new_thread)
            {
                SetEvent(_mt_info->work_event);
            }
            else
            {
                _mt_info->thread_handle = (HANDLE)_beginthreadex(NULL, 0, mt_proc_wrapper, this, 0, NULL);
                if (!_mt_info->thread_handle) {
                    int err = errno;
                    mt_info_destroy(_mt_info);
                    _mt_info = NULL;
                    env->ThrowError("f3kdb_avisynth: Failed to create worker thread, code = %d.", err);
                    return NULL;
                }
            }
            process_plane(n, src, dst, dstp_y, PLANAR_Y, env);
            WaitForSingleObject(_mt_info->work_complete_event, INFINITE);
        }
    } else {
        // Y8
        process_plane(n, src, dst, dst->GetWritePtr(), PLANAR_Y, env);
    }
    return dst;
}