/*
VS_DELOGO Copyright(C) 2003 MakKi, 2014-2015 msg7086

This program is free software; you can redistribute it and / or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
*/

#ifndef __DELOGO_HPP
#define __DELOGO_HPP

#include <stdio.h>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>
#include "IScriptEnvironment.h"
#include "logo.h"

#define PLUGIN_VERSION "005a." \
                       "0.4"

#define LOGO_FADE_MAX 256
#define LOGO_DEFAULT_DEPTH 128

#ifndef _MSC_VER
#define fopen_s(a, b, c) *a = fopen(b, c)
#endif

typedef struct {
    short dp, c;
} LOCAL_LOGO_PIXEL;

class delogo {
    const char* m_logofile;
    const char* m_logoname;
    int m_pos_x;
    int m_pos_y;
    int m_depth;
    int m_yc_y;
    int m_yc_u;
    int m_yc_v;
    int m_start;
    int m_end;
    int m_fadein;
    int m_fadeout;
    int m_cutoff;
    int m_mode;
    LOGO_HEADER m_lgh;
    LOCAL_LOGO_PIXEL* m_lgd;

public:
    VSVideoInfo* vi;
    VSNodeRef* node;

    delogo(const VSAPI* vsapi,
        VSVideoInfo* vi,
        VSNodeRef* node,
        const char* logofile,
        const char* logoname,
        int pos_x,
        int pos_y,
        int depth,
        int yc_y,
        int yc_u,
        int yc_v,
        int start,
        int end,
        int fadein,
        int fadeout,
        int cutoff,
        int mode);

    ~delogo()
    {
    }

    inline void GetFramePre(IScriptEnvironment* env, int n)
    {
        env->PrefetchFrame(n);
    }

    inline const VSFrameRef* GetFrame(IScriptEnvironment* env, int n)
    {
        return m_mode == -1 ? GetFrameErase(n, env) : GetFrameAdd(n, env);
    }

private:
    LOGO_PIXEL* ReadLogoData();
    LOGO_PIXEL* AdjustLogo(LOGO_PIXEL* lgd);
    LOGO_PIXEL* ColorTuning(LOGO_PIXEL* lgd);
    LOGO_PIXEL* AlphaCutoff(LOGO_PIXEL* lgd);

    LOCAL_LOGO_PIXEL* Convert(LOGO_PIXEL* src, LOGO_HEADER& m_lgh);
    const VSFrameRef* GetFrameAdd(int n, IScriptEnvironment* env);
    const VSFrameRef* GetFrameErase(int n, IScriptEnvironment* env);

    // yv12
    LOCAL_LOGO_PIXEL* Convert_yv12(LOGO_PIXEL* src, LOGO_HEADER& m_lgh);
    const VSFrameRef* GetFrameAdd_yv12(int n, IScriptEnvironment* env);
    const VSFrameRef* GetFrameErase_yv12(int n, IScriptEnvironment* env);

    // yv24
    LOCAL_LOGO_PIXEL* Convert_yv24(LOGO_PIXEL* src, LOGO_HEADER& m_lgh);
    const VSFrameRef* GetFrameAdd_yv24(int n, IScriptEnvironment* env);
    const VSFrameRef* GetFrameErase_yv24(int n, IScriptEnvironment* env);

    /// Compute depth by fade
    int CalcFade(int n)
    {
        if (n < m_start || (m_end < n && m_end >= m_start)) { // Out of frame range
            return 0;
        }
        if (n < m_start + m_fadein) // Fade in
            return ((n - m_start) * 2 + 1) * LOGO_FADE_MAX / (m_fadein * 2);
        if (n > m_end - m_fadeout && m_end >= 0) // Fade out
            return ((m_end - n) * 2 + 1) * LOGO_FADE_MAX / (m_fadeout * 2);
        // Regular frames
        return LOGO_FADE_MAX;
    }

    inline int Clamp(int n, int l, int h)
    {
        return VSMIN(VSMAX(n, l), h);
    }

    inline int YtoAUY(int y)
    {
        //return ((y - 16) * 1197 + 32) >> 6;
        return (y * 1197 - 19120) >> 6;
    }

    inline int CtoAUC(int c)
    {
        //return ((c - 128) * 4681 + 128) >> 8;
        return (c * 4681 - 599040) >> 8;
    }

    inline int AUYtoY(int y)
    {
        //return Clamp(((y * 219 + 2048) >> 12) + 16, 0, 255);
        return Clamp((y * 219 + 67584) >> 12, 0, 255);
    }

    inline int AUCtoC(int c)
    {
        //return Clamp(((c * 7 + 64) >> 7) + 128, 0, 255);
        return Clamp((c * 7 + 16448) >> 7, 0, 255);
    }
};

#endif
