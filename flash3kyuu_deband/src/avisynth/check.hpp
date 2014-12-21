#pragma once

#include "avisynth.h"

#include <stdio.h>

static void check_parameter_range(const char* name, int value, int lower_bound, int upper_bound, char* param_name, IScriptEnvironment* env) {
    if (value < lower_bound || value > upper_bound) {
        char msg[1024];
        sprintf_s(msg, "%s: Invalid value for parameter %s: %d, must be %d ~ %d.",
            name, param_name, value, lower_bound, upper_bound);
        env->ThrowError(_strdup(msg));
    }
}

static void check_video_format(const char* name, const VideoInfo& vi, IScriptEnvironment* env)
{
    char name_buffer[256];
    if (!vi.IsYUV() || !vi.IsPlanar()) {
        sprintf_s(name_buffer, "%s: Only planar YUV clips are supported.", name);
        env->ThrowError(_strdup(name_buffer));
    }
    if (vi.IsFieldBased()) {
        sprintf_s(name_buffer, "%s: Field-based clip is not supported.", name);
        env->ThrowError(_strdup(name_buffer));
    }
}