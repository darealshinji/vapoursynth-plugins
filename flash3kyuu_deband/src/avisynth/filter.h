#pragma once

#include <f3kdb.h>
#include "avisynth.h"
#include "flash3kyuu_deband.def.h"

#include "mt_info.hpp"

AVSValue __cdecl Create_flash3kyuu_deband(AVSValue args, void* user_data, IScriptEnvironment* env);

class f3kdb_avisynth : public GenericVideoFilter {
private:
    f3kdb_core_t* _core;
    bool _mt;
    volatile mt_info* _mt_info;

    void process_plane(int n, PVideoFrame src, PVideoFrame dst, unsigned char *dstp, int plane, IScriptEnvironment* env);

public:
    void mt_proc(void);
    f3kdb_avisynth(PClip child, f3kdb_core_t* core, int dst_width, int dst_height, bool mt);
    ~f3kdb_avisynth();

    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};