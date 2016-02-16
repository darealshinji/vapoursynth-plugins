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

#include "delogo.hpp"

LOCAL_LOGO_PIXEL* delogo::Convert_yv24(LOGO_PIXEL* src, LOGO_HEADER& m_lgh)
{
    LOCAL_LOGO_PIXEL* p = new LOCAL_LOGO_PIXEL[m_lgh.w * m_lgh.h * 3];
    if (p == NULL) {
        throw "Failed in memory allocation.";
    }
    LOCAL_LOGO_PIXEL* dstY = p;
    LOCAL_LOGO_PIXEL* dstU = p + m_lgh.h * m_lgh.w;
    LOCAL_LOGO_PIXEL* dstV = p + 2 * m_lgh.h * m_lgh.w;

    for (int i = m_lgh.h; i; --i) {
        for (int j = m_lgh.w; j; --j) {
            dstY->dp = src->dp_y;
            dstY->c = src->y;
            dstU->dp = src->dp_cb;
            dstU->c = src->cb;
            dstV->dp = src->dp_cr;
            dstV->c = src->cr;
            ++src;
            ++dstY;
            ++dstU;
            ++dstV;
        }
    }

    return p;
}

/* GetFrame */
const VSFrameRef* delogo::GetFrameErase_yv24(int n, IScriptEnvironment* env)
{
    const VSFrameRef* srcframe(env->GetFrame(n));
    int fade = CalcFade(n);
    if (fade == 0)
        return srcframe;

    VSFrameRef* frame = env->MakeWritable(srcframe);
    env->FreeFrame(srcframe);

    // Logo->xywh, frame->wh, all even number
    int logo_w = VSMIN(m_lgh.w, env->GetWidth(frame) - m_lgh.x);
    int logo_h = VSMIN(m_lgh.h, env->GetHeight(frame) - m_lgh.y);
    int dst_x = m_lgh.x;
    int dst_y = m_lgh.y;
    int logo_x = 0;
    int logo_y = 0;
    if (dst_x < 0) {
        logo_x = -dst_x;
        logo_w -= logo_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        logo_y = -dst_y;
        logo_h -= logo_y;
        dst_y = 0;
    }
    if (logo_w <= 0 || logo_h <= 0)
        return frame; // Out of frame

    //Y
    int dst_pitch = env->GetPitch(frame, PLANAR_Y);
    int dst_pitch_r = dst_pitch - logo_w;
    BYTE* dst = env->GetWritePtr(frame, PLANAR_Y) + dst_x + dst_y * dst_pitch;
    int logo_pitch = m_lgh.w;
    int logo_pitch_r = logo_pitch - logo_w;
    LOCAL_LOGO_PIXEL* lgp = m_lgd + logo_x + logo_y * m_lgh.w;
    int i, j;
    for (i = logo_h; i; --i) {
        for (j = logo_w; j; --j) {
            int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
            if (dp) {
                if (dp == LOGO_MAX_DP)
                    --dp;
                *dst = AUYtoY((YtoAUY(*dst) * LOGO_MAX_DP - lgp->c * dp
                                  + (LOGO_MAX_DP - dp) / 2) / (LOGO_MAX_DP - dp));
            }
            ++dst;
            ++lgp;
        }
        dst += dst_pitch_r;
        lgp += logo_pitch_r;
    }
    //U
    dst_pitch = env->GetPitch(frame, PLANAR_U);
    dst_pitch_r = dst_pitch - logo_w;
    dst = env->GetWritePtr(frame, PLANAR_U) + dst_x + dst_y * dst_pitch;
    logo_pitch = m_lgh.w;
    logo_pitch_r = logo_pitch - logo_w;
    lgp = m_lgd + m_lgh.w * m_lgh.h + logo_x + logo_y * m_lgh.w;
    for (i = logo_h; i; --i) {
        for (j = logo_w; j; --j) {
            int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
            if (dp) {
                if (dp == LOGO_MAX_DP)
                    --dp;
                *dst = AUCtoC((CtoAUC(*dst) * LOGO_MAX_DP - lgp->c * dp
                                  + (LOGO_MAX_DP - dp) / 2) / (LOGO_MAX_DP - dp));
            }
            ++dst;
            ++lgp;
        }
        dst += dst_pitch_r;
        lgp += logo_pitch_r;
    }
    //V
    dst_pitch = env->GetPitch(frame, PLANAR_V);
    dst_pitch_r = dst_pitch - logo_w;
    dst = env->GetWritePtr(frame, PLANAR_V) + dst_x + dst_y * dst_pitch;
    lgp = m_lgd + 2 * m_lgh.w * m_lgh.h + logo_x + logo_y * m_lgh.w;
    for (i = logo_h; i; --i) {
        for (j = logo_w; j; --j) {
            int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
            if (dp) {
                if (dp == LOGO_MAX_DP)
                    --dp;
                *dst = AUCtoC((CtoAUC(*dst) * LOGO_MAX_DP - lgp->c * dp
                                  + (LOGO_MAX_DP - dp) / 2) / (LOGO_MAX_DP - dp));
            }
            ++dst;
            ++lgp;
        }
        dst += dst_pitch_r;
        lgp += logo_pitch_r;
    }

    return frame;
}

const VSFrameRef* delogo::GetFrameAdd_yv24(int n, IScriptEnvironment* env)
{
    const VSFrameRef* srcframe(env->GetFrame(n));
    int fade = CalcFade(n);
    if (fade == 0)
        return srcframe;

    VSFrameRef* frame = env->MakeWritable(srcframe);
    env->FreeFrame(srcframe);

    // Logo->xywh, frame->wh, all even number
    int logo_w = VSMIN(m_lgh.w, env->GetWidth(frame) - m_lgh.x);
    int logo_h = VSMIN(m_lgh.h, env->GetHeight(frame) - m_lgh.y);
    int dst_x = m_lgh.x;
    int dst_y = m_lgh.y;
    int logo_x = 0;
    int logo_y = 0;
    if (dst_x < 0) {
        logo_x = -dst_x;
        logo_w -= logo_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        logo_y = -dst_y;
        logo_h -= logo_y;
        dst_y = 0;
    }
    if (logo_w <= 0 || logo_h <= 0)
        return frame; // Out of frame

    //Y
    int dst_pitch = env->GetPitch(frame, PLANAR_Y);
    int dst_pitch_r = dst_pitch - logo_w;
    BYTE* dst = env->GetWritePtr(frame, PLANAR_Y) + dst_x + dst_y * dst_pitch;
    int logo_pitch = m_lgh.w;
    int logo_pitch_r = logo_pitch - logo_w;
    LOCAL_LOGO_PIXEL* lgp = m_lgd + logo_x + logo_y * m_lgh.w;
    int i, j;
    for (i = logo_h; i; --i) {
        for (j = logo_w; j; --j) {
            int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
            if (dp) {
                *dst = AUYtoY((YtoAUY(*dst) * (LOGO_MAX_DP - dp) + lgp->c * dp
                                  + LOGO_MAX_DP / 2) / LOGO_MAX_DP);
            }
            ++dst;
            ++lgp;
        }
        dst += dst_pitch_r;
        lgp += logo_pitch_r;
    }
    //U
    dst_pitch = env->GetPitch(frame, PLANAR_U);
    dst_pitch_r = dst_pitch - logo_w;
    dst = env->GetWritePtr(frame, PLANAR_U) + dst_x + dst_y * dst_pitch;
    logo_pitch = m_lgh.w;
    logo_pitch_r = logo_pitch - logo_w;
    lgp = m_lgd + m_lgh.w * m_lgh.h + logo_x + logo_y * m_lgh.w;
    for (i = logo_h; i; --i) {
        for (j = logo_w; j; --j) {
            int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
            if (dp) {
                *dst = AUCtoC((CtoAUC(*dst) * (LOGO_MAX_DP - dp) + lgp->c * dp
                                  + LOGO_MAX_DP / 2) / LOGO_MAX_DP);
            }
            ++dst;
            ++lgp;
        }
        dst += dst_pitch_r;
        lgp += logo_pitch_r;
    }
    //V
    dst_pitch = env->GetPitch(frame, PLANAR_V);
    dst_pitch_r = dst_pitch - logo_w;
    dst = env->GetWritePtr(frame, PLANAR_V) + dst_x + dst_y * dst_pitch;
    lgp = m_lgd + 2 * m_lgh.w * m_lgh.h + logo_x + logo_y * m_lgh.w;
    for (i = logo_h; i; --i) {
        for (j = logo_w; j; --j) {
            int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
            if (dp) {
                *dst = AUCtoC((CtoAUC(*dst) * (LOGO_MAX_DP - dp) + lgp->c * dp
                                  + LOGO_MAX_DP / 2) / LOGO_MAX_DP);
            }
            ++dst;
            ++lgp;
        }
        dst += dst_pitch_r;
        lgp += logo_pitch_r;
    }

    return frame;
}
