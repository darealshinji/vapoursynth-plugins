/*
  CombMask for AviSynth2.6x

  Copyright (C) 2013 Oka Motofumi

  Authors: Oka Motofumi (chikuzen.mo at gmail dot com)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
*/


#include <malloc.h>
#include <emmintrin.h>
#include <stdint.h>
#include <windows.h>
#include "avisynth.h"

#define CMASK_VERSION "0.0.1"
#define RSIZE sizeof(__m128i)
#define USIZE sizeof(unsigned)


static const AVS_Linkage* AVS_linkage = 0;

static const int planes[] = {PLANAR_Y, PLANAR_U, PLANAR_V};


/******************************************************************************
                                CombMask
******************************************************************************/

static void __stdcall
write_mmask_sse2(int num_planes, int mthresh, PVideoFrame& src,
                 uint8_t* temp, int t_width, PVideoFrame& prev)
{
    const __m128i xmth = _mm_set1_epi8((char)mthresh);
    const __m128i zero = _mm_setzero_si128();
    const __m128i all1 = _mm_cmpeq_epi32(zero, zero);
    int mmask_pitch = t_width / RSIZE;

    for (int p = 0; p < num_planes; p++) {
        const int width = (src->GetRowSize(planes[p]) + RSIZE - 1) / RSIZE;
        const int height = src->GetHeight(planes[p]);
        const int src_pitch = src->GetPitch(planes[p]) / RSIZE;
        const int prev_pitch = prev->GetPitch(planes[p]) / RSIZE;

        const __m128i* srcp = (__m128i*)src->GetReadPtr(planes[p]);
        const __m128i* prevp = (__m128i*)prev->GetReadPtr(planes[p]);
        __m128i* mmask = (__m128i*)temp;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(srcp + x);
                __m128i xmm1 = _mm_load_si128(prevp + x);

                __m128i xmm2 = _mm_max_epu8(xmm0, xmm1);
                xmm0 = _mm_min_epu8(xmm0, xmm1);
                xmm2 = _mm_subs_epu8(xmm2, xmm0);
                xmm2 = _mm_subs_epu8(xmm2, xmth);
                xmm2 = _mm_cmpeq_epi8(xmm2, zero);
                xmm2 = _mm_xor_si128(xmm2, all1);
                _mm_store_si128(mmask + x, xmm2);
            }
            srcp += src_pitch;
            prevp += prev_pitch;
            mmask += mmask_pitch;
        }

        __m128i* mmaskc = (__m128i*)temp;
        __m128i* mmaskt = mmaskc + mmask_pitch;
        __m128i* mmaskb = mmaskt;
        __m128i* dstp = (__m128i*)prev->GetWritePtr(planes[p]);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(mmaskt + x);
                __m128i xmm1 = _mm_load_si128(mmaskc + x);
                __m128i xmm2 = _mm_load_si128(mmaskb + x);
                xmm0 = _mm_or_si128(xmm0, xmm2);
                xmm1 = _mm_or_si128(xmm1, xmm0);
                _mm_store_si128(dstp + x, xmm1);
            }
            mmaskt = mmaskc;
            mmaskc = mmaskb;
            mmaskb = (y < height - 2) ? mmaskb + mmask_pitch : mmaskb - mmask_pitch;
            dstp += prev_pitch;
        }
    }
}


static void __stdcall
write_mmask_c(const int num_planes, const int mthresh, PVideoFrame& src,
              uint8_t* temp, int t_width, PVideoFrame& prev)
{
    for (int p = 0; p < num_planes; p++) {
        int width = src->GetRowSize(planes[p]);
        const int height = src->GetHeight(planes[p]);
        const int src_pitch = src->GetPitch(planes[p]);
        int prv_pitch = prev->GetPitch(planes[p]);

        const uint8_t* srcp = src->GetReadPtr(planes[p]);
        const uint8_t* prvp = prev->GetReadPtr(planes[p]);
        uint8_t* mmask = temp;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                mmask[x] = abs((int)srcp[x] - prvp[x]) > mthresh ? 0xFF : 0x00;
            }
            srcp += src_pitch;
            prvp += prv_pitch;
            mmask += t_width;
        }

        width = (width + USIZE - 1) / USIZE;
        prv_pitch /= USIZE;
        int mmask_pitch = t_width / USIZE;

        unsigned* mmaskc = (unsigned*)temp;
        unsigned* mmaskt = mmaskc + mmask_pitch;
        unsigned* mmaskb = mmaskt;
        unsigned* dstp = (unsigned*)prev->GetWritePtr(planes[p]);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                dstp[x] = (mmaskt[x] | mmaskc[x] | mmaskb[x]);
            }
            mmaskt = mmaskc;
            mmaskc = mmaskb;
            mmaskb = y < height - 2 ? mmaskb + mmask_pitch : mmaskb - mmask_pitch;
            dstp += prv_pitch;
        }
    }
}


/*
How to detect combs (quoted from TFM - README.txt written by tritical)

    Assume 5 neighboring pixels (a,b,c,d,e) positioned vertically.

      a
      b
      c
      d
      e

    d1 = c - b;
    d2 = c - d;
    if ((d1 > cthresh && d2 > cthresh) || (d1 < -cthresh && d2 < -cthresh)) {
        if (abs(a+4*c+e-3*(b+d)) > cthresh*6) it's combed;
    }
--------------------------------------------------------------------------------

    x = a - b -> -x = b - a

    (d1 > cthresh && d2 > cthresh)    == min(d1, d2) - cthresh > 0
                                      == (c - max(b, d)) - cthresh > 0

    (d1 < -cthresh && d2 < -cthresh)  == (-d1 > cthresh && -d2 > cthresh)
                                      == min(-d1, -d2) - cthresh > 0
                                      == min(b - c, d - c) - cthresh > 0
                                      == (min(b, d) - c) - cthresh > 0

    !A || !B == !(A && B)

    abs(x) > cthresh * 6 == (x > cthresh * 6 || x < cthresh * -6)
*/

static void __stdcall
write_cmask_sse2(int num_planes, int cthresh, PVideoFrame& src, PVideoFrame& dst,
                 IScriptEnvironment* env)
{
    const __m128i xcth = _mm_set1_epi8((char)cthresh);
    const __m128i xct6p = _mm_set1_epi16((short)(cthresh * 6));
    const __m128i xct6n = _mm_set1_epi16((short)(cthresh * -6));
    const __m128i zero = _mm_setzero_si128();

    for (int p = 0; p < num_planes; p++) {
        const int src_pitch = src->GetPitch(planes[p]) / RSIZE;
        const int width = (src->GetRowSize(planes[p]) + RSIZE - 1) / RSIZE;
        const int height = src->GetHeight(planes[p]);

        const __m128i* srcpc = (__m128i*)src->GetReadPtr(planes[p]);
        const __m128i* srcpb = srcpc + src_pitch;
        const __m128i* srcpa = srcpb + src_pitch;
        const __m128i* srcpd = srcpc + src_pitch;
        const __m128i* srcpe = srcpd + src_pitch;

        __m128i* dstp = (__m128i*)dst->GetWritePtr(planes[p]);
        const int dst_pitch = dst->GetPitch(planes[p]) / RSIZE;

        if ((intptr_t)srcpc & 15) {
            env->ThrowError("CombMask: INVALID MEMORY ALIGNMENT");
        }
        memset(dstp, 0, dst_pitch * height * RSIZE);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(srcpc + x);
                __m128i xmm1 = _mm_load_si128(srcpb + x);
                __m128i xmm2 = _mm_load_si128(srcpd + x);

                __m128i xmm3 = _mm_subs_epu8(xmm0, _mm_max_epu8(xmm1, xmm2));
                xmm3 = _mm_cmpeq_epi8(zero, _mm_subs_epu8(xmm3, xcth)); // !(d1 > cthresh && d2 > cthresh)

                __m128i xmm4 = _mm_subs_epu8(_mm_min_epu8(xmm1, xmm2), xmm0);
                xmm4 = _mm_cmpeq_epi8(zero, _mm_subs_epu8(xmm4, xcth)); // !(d1 < -cthresh && d2 < -cthresh)

                xmm3 = _mm_and_si128(xmm3, xmm4);

                xmm4 = _mm_add_epi16(_mm_unpacklo_epi8(xmm1, zero),
                                     _mm_unpacklo_epi8(xmm2, zero)); // lo of (b+d)
                xmm1 = _mm_add_epi16(_mm_unpackhi_epi8(xmm1, zero),
                                     _mm_unpackhi_epi8(xmm2, zero)); // hi of (b+d)
                xmm4 = _mm_add_epi16(xmm4, _mm_add_epi16(xmm4, xmm4));      // lo of 3*(b+d)
                xmm1 = _mm_add_epi16(xmm1, _mm_add_epi16(xmm1, xmm1));      // hi of 3*(b+d)

                xmm4 = _mm_sub_epi16(_mm_slli_epi16(_mm_unpacklo_epi8(xmm0, zero), 2), xmm4); // lo of 4*c-3*(b+d)
                xmm1 = _mm_sub_epi16(_mm_slli_epi16(_mm_unpackhi_epi8(xmm0, zero), 2), xmm1); // hi of 4*c-3*(b+d)

                xmm0 = _mm_load_si128(srcpa + x);
                xmm2 = _mm_load_si128(srcpe + x);
                xmm4 = _mm_add_epi16(xmm4, _mm_unpacklo_epi8(xmm0, zero)); // lo of a+4*c-3*(b+d)
                xmm1 = _mm_add_epi16(xmm1, _mm_unpackhi_epi8(xmm0, zero)); // hi of a+4*c-3*(b+d)
                xmm4 = _mm_add_epi16(xmm4, _mm_unpacklo_epi8(xmm2, zero)); // lo of a+4*c+e-3*(b+d)
                xmm1 = _mm_add_epi16(xmm1, _mm_unpackhi_epi8(xmm2, zero)); // hi of a+4*c+e-3*(b+d)

                xmm4 = _mm_or_si128(_mm_cmpgt_epi16(xmm4, xct6p),
                                    _mm_cmplt_epi16(xmm4, xct6n));
                xmm1 = _mm_or_si128(_mm_cmpgt_epi16(xmm1, xct6p),
                                    _mm_cmplt_epi16(xmm1, xct6n));
                xmm1 = _mm_packs_epi16(xmm4, xmm1);

                xmm3 = _mm_andnot_si128(xmm3, xmm1);

                _mm_store_si128(dstp + x, xmm3);
            }
            dstp += dst_pitch;
            srcpa = srcpb;
            srcpb = srcpc;
            srcpc = srcpd;
            srcpd = srcpe;
            srcpe = (y < height - 2) ? srcpe + src_pitch : srcpe - src_pitch;
        }
    }
}


static void __stdcall
write_cmask_c(int num_planes, int cthresh, PVideoFrame& src, PVideoFrame& dst,
              IScriptEnvironment* env)
{
    for (int p = 0; p < num_planes; p++) {
        const int width = src->GetRowSize(planes[p]);
        const int height = src->GetHeight(planes[p]);
        const int src_pitch = src->GetPitch(planes[p]);
        const int dst_pitch = dst->GetPitch(planes[p]);

        const uint8_t* srcpc = src->GetReadPtr(planes[p]);
        const uint8_t* srcpb = srcpc + src_pitch;
        const uint8_t* srcpa = srcpb + src_pitch;
        const uint8_t* srcpd = srcpc + src_pitch;
        const uint8_t* srcpe = srcpc + src_pitch;

        uint8_t* dstp = dst->GetWritePtr(planes[p]);

        memset(dstp, 0, dst_pitch * height);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int d1 = srcpc[x] - srcpb[x];
                int d2 = srcpc[x] - srcpd[x];

                if ((d1 > cthresh && d2 > cthresh) ||
                    (d1 < -cthresh && d2 < -cthresh)) {

                    if (abs(srcpa[x] + 4 * srcpc[x] + srcpe[x]
                        - 3 * (srcpb[x] + srcpd[x])) > cthresh * 6) {

                        dstp[x] = 0xFF;
                    }
                }
            }
            srcpa = srcpb;
            srcpb = srcpc;
            srcpc = srcpd;
            srcpd = srcpe;
            srcpe = (y < height - 2) ? srcpe + src_pitch : srcpe - src_pitch;
            dstp += dst_pitch;
        }
    }
}


static void __stdcall
c_and_m_sse2(int num_planes, PVideoFrame& cmask, PVideoFrame& mmask)
{
    for (int p = 0; p < num_planes; p++) {
        __m128i* cp = (__m128i*)cmask->GetWritePtr(planes[p]);
        const __m128i* mp = (__m128i*)mmask->GetReadPtr(planes[p]);

        const int pitch_c = cmask->GetPitch(planes[p]) / RSIZE;
        const int pitch_m = mmask->GetPitch(planes[p]) / RSIZE;
        const int width = (cmask->GetRowSize(planes[p]) + RSIZE - 1) / RSIZE;
        const int height = cmask->GetHeight(planes[p]);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xmm0 = _mm_load_si128(cp + x);
                __m128i xmm1 = _mm_load_si128(mp + x);
                xmm0 = _mm_and_si128(xmm0, xmm1);
                _mm_store_si128(cp + x, xmm0);
            }
            cp += pitch_c;
            mp += pitch_m;
        }
    }
}


static void __stdcall
c_and_m_c(int num_planes, PVideoFrame& cmask, PVideoFrame& mmask)
{
    for (int p = 0; p < num_planes; p++) {
        unsigned* cmskp = (unsigned*)cmask->GetWritePtr(planes[p]);
        const unsigned* mmskp = (unsigned*)mmask->GetReadPtr(planes[p]);

        const int pitch_c = cmask->GetPitch(planes[p]) / USIZE;
        const int pitch_m = mmask->GetPitch(planes[p]) / USIZE;
        const int width = (cmask->GetRowSize(planes[p]) + USIZE - 1) / USIZE;
        const int height = cmask->GetHeight(planes[p]);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                cmskp[x] &= mmskp[x];
            }
            cmskp += pitch_c;
            mmskp += pitch_m;
        }
    }
}


static void __stdcall
horizontal_dilation_sse2(int num_planes, PVideoFrame& mask, uint8_t* buff)
{
    for (int p = 0; p < num_planes; p++) {
        int width = mask->GetRowSize(planes[p]);
        int height = mask->GetHeight(planes[p]);
        int pitch = mask->GetPitch(planes[p]);
        uint8_t* maskp = mask->GetWritePtr(planes[p]);

        for (int y = 0; y < height; y++) {
            memcpy(buff, maskp, pitch);
            buff[-1] = buff[0];
            buff[width] = buff[width - 1];

            for (int x = 0; x < width; x += 16) {
                __m128i xmm0 = _mm_load_si128((__m128i*)(buff + x));
                __m128i xmm1 = _mm_loadu_si128((__m128i*)(buff + x + 1));
                __m128i xmm2 = _mm_loadu_si128((__m128i*)(buff + x - 1));
                xmm0 = _mm_or_si128(xmm0, _mm_or_si128(xmm1, xmm2));
                _mm_store_si128((__m128i*)(maskp + x), xmm0);
            }
            maskp += pitch;
        }
    }
}


static void __stdcall
horizontal_dilation_c(int num_planes, PVideoFrame& mask, uint8_t* buff)
{
    for (int p = 0; p < num_planes; p++) {
        int height = mask->GetHeight(planes[p]);
        int width = mask->GetRowSize(planes[p]);
        int pitch = mask->GetPitch(planes[p]);
        uint8_t* maskp = mask->GetWritePtr(planes[p]);

        for (int y = 0; y < height; y++) {
            memcpy(buff, maskp, pitch);
            buff[-1] = buff[0];
            buff[width] = buff[width - 1];
            for (int x = 0; x < width; x++) {
                maskp[x] = (buff[x - 1] | buff[x]) | buff[x + 1];
            }
        }
    }
}


class CombMask : public GenericVideoFilter {

    int cthresh;
    int mthresh;
    int num_planes;
    bool dilation;
    uint8_t* buff;
    uint8_t* mmtemp;
    int mmtemp_width;

    void (__stdcall *write_motion_mask)(int num_planes, int mthresh,
                                        PVideoFrame& src, uint8_t* temp,
                                        int t_width, PVideoFrame& prev);
    void (__stdcall *write_comb_mask)(int num_planes, int cthresh,
                                      PVideoFrame& src, PVideoFrame& dst,
                                      IScriptEnvironment* env);
    void (__stdcall *comb_and_motion)(int num_planes, PVideoFrame& cmask,
                                      PVideoFrame& mmask);
    void (__stdcall *horizontal_dilation)(int num_planes, PVideoFrame& mask,
                                          uint8_t* buff);

public:
    CombMask(PClip c, int cth, int mth, bool chroma, bool sse2, bool hd,
             IScriptEnvironment* env);
    ~CombMask();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};


CombMask::
CombMask(PClip c, int cth, int mth, bool chroma, bool sse2, bool hd,
         IScriptEnvironment* env)
    : GenericVideoFilter(c), cthresh(cth), mthresh(mth), dilation(hd)
{
    buff = 0;
    mmtemp = 0;

    if (cthresh < 0 || cthresh > 255) {
        env->ThrowError("CombMask: cthresh must be between 0 and 255.");
    }

    if (mthresh < 0 || mthresh > 255) {
        env->ThrowError("CombMask: mthresh must be between 0 and 255.");
    }

    if (!vi.IsPlanar()) {
        env->ThrowError("CombMask: planar format only.");
    }

    if (dilation) {
        buff = (uint8_t*)_aligned_malloc(((vi.width + 31) / 16) * 16, 16);
        if (!buff) {
            env->ThrowError("CombMask: failed to allocate temporal buffer.");
        }
    }

    if (mthresh > 0) {
        mmtemp_width = (vi.width + 15) / 16 * 16;
        mmtemp = (uint8_t*)_aligned_malloc(mmtemp_width * vi.height, 16);
        if (!mmtemp) {
            env->ThrowError("CombMask: failed to allocate temporal buffer.");
        }
    }

    num_planes = vi.IsY8() || !chroma ? 1 : 3;

    if (!(env->GetCPUFlags() & CPUF_SSE2) && sse2) {
        sse2 = false;
    }

    write_motion_mask = sse2 ? write_mmask_sse2 : write_mmask_c;

    write_comb_mask = sse2 ? write_cmask_sse2 : write_cmask_c;

    comb_and_motion = sse2 ? c_and_m_sse2 : c_and_m_c;

    horizontal_dilation = sse2 ? horizontal_dilation_sse2 :
                                 horizontal_dilation_c;
}


CombMask::~CombMask()
{
    if (buff) {
        _aligned_free(buff);
        buff = 0;
    }
    if (mmtemp) {
        _aligned_free(mmtemp);
        mmtemp = 0;
    }
}


PVideoFrame __stdcall CombMask::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame cmask = env->NewVideoFrame(vi);
    write_comb_mask(num_planes, cthresh, src, cmask, env);

    if (mthresh > 0) {
        PVideoFrame mmask = child->GetFrame(n == 0 ? 1 : n - 1, env);
        env->MakeWritable(&mmask);
        write_motion_mask(num_planes, mthresh, src, mmtemp, mmtemp_width, mmask);
        comb_and_motion(num_planes, cmask, mmask);
    }

    if (dilation) {
        horizontal_dilation(num_planes, cmask, buff + 16);
    }

    return cmask;
}


static AVSValue __cdecl
create_combmask(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new CombMask(args[0].AsClip(),  args[1].AsInt(6), args[2].AsInt(9),
                         args[3].AsBool(true), args[4].AsBool(true), true,
                         env);
}

/****************************************************************************
                                MaskedMerge
******************************************************************************/

static bool __stdcall
check_combed_sse2(PVideoFrame& cmask, int mi, IScriptEnvironment* env)
{
    const int width = cmask->GetRowSize(PLANAR_Y) / RSIZE;
    const int height = cmask->GetHeight(PLANAR_Y) / 16;
    const int pitch_0 = cmask->GetPitch(PLANAR_Y) / RSIZE;
    const int pitch_1 = pitch_0 * 16;

    const __m128i* srcp = (__m128i*)cmask->GetReadPtr(PLANAR_Y);
    if ((intptr_t)srcp & 15) {
        env->ThrowError("check_combed_sse2: INVALID MEMORY ALIGNMENT");
    }

    const __m128i zero = _mm_setzero_si128();
    const __m128i all1 = _mm_cmpeq_epi32(zero, zero);
    const __m128i one = _mm_set1_epi8((char)1);

    __declspec(align(16)) int64_t array[2];
    __m128i* arr = (__m128i*)array;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            __m128i sum = zero;

            for (int i = 0; i < 16; i++) {
                // 0xFF == -1, thus the range of each bytes of sum is -16 to 0.
                sum = _mm_add_epi8(sum, _mm_load_si128(srcp + x + pitch_0 * i));
            }

            sum = _mm_xor_si128(sum, all1);
            sum = _mm_add_epi8(sum, one);       // -x = ~x + 1
            sum = _mm_sad_epu8(sum, zero);
            _mm_store_si128(arr, sum);

            if (array[0] > mi || array[1] > mi) {
                return true;
            }
        }
        srcp += pitch_1;
    }
    return false;
}


static bool __stdcall
check_combed_c(PVideoFrame& cmask, int mi, IScriptEnvironment* env)
{
    const int width = (cmask->GetRowSize(PLANAR_Y) / 8) * 8;
    const int height = cmask->GetHeight(PLANAR_Y) / 16;
    const int pitch_0 = cmask->GetPitch(PLANAR_Y);
    const int pitch_1 = pitch_0 * 16;

    const uint8_t* srcp = cmask->GetReadPtr(PLANAR_Y);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            int count = 0;
            for (int i = 0; i < 16; i++) {
                for (int j = 0; j < 8; j++) {
                    count += !!srcp[x + j + i * pitch_0];
                }
            }
            if (count > mi) {
                return true;
            }
        }
        srcp += pitch_1;
    }
    return false;
}


static void __stdcall
merge_frames_sse2(int num_planes, PVideoFrame& src, PVideoFrame& alt,
                  PVideoFrame& mask, PVideoFrame& dst,
                  IScriptEnvironment* env)
{
    for (int p = 0; p < num_planes; p++) {
        const __m128i* srcp = (__m128i*)src->GetReadPtr(planes[p]);
        const __m128i* altp = (__m128i*)alt->GetReadPtr(planes[p]);
        const __m128i* mskp = (__m128i*)mask->GetReadPtr(planes[p]);
        __m128i* dstp = (__m128i*)dst->GetWritePtr(planes[p]);

        if (((intptr_t)srcp | (intptr_t)altp | (intptr_t)mskp) & 15) {
            env->ThrowError("MaskedMerge: INVALID MEMORY ALIGNMENT");
        }

        int width = (src->GetRowSize(planes[p]) + RSIZE - 1) / RSIZE;
        int height = src->GetHeight(planes[p]);

        int src_pitch = src->GetPitch(planes[p]) / RSIZE;
        int alt_pitch = alt->GetPitch(planes[p]) / RSIZE;
        int msk_pitch = mask->GetPitch(planes[p]) / RSIZE;
        int dst_pitch = dst->GetPitch(planes[p]) / RSIZE;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                __m128i xsrc = _mm_load_si128(srcp + x);
                __m128i xalt = _mm_load_si128(altp + x);
                __m128i xmsk = _mm_load_si128(mskp + x);

                xsrc = _mm_andnot_si128(xmsk, xsrc);
                xalt = _mm_and_si128(xalt, xmsk);
                xsrc = _mm_or_si128(xsrc, xalt);
                _mm_store_si128(dstp + x, xsrc);
            }
            srcp += src_pitch;
            altp += alt_pitch;
            mskp += msk_pitch;
            dstp += dst_pitch;
        }
    }
}


static void __stdcall
merge_frames_c(int num_planes, PVideoFrame& src, PVideoFrame& alt,
               PVideoFrame& mask, PVideoFrame& dst,
               IScriptEnvironment* env)
{
    for (int p = 0; p < num_planes; p++) {
        const unsigned* srcp = (unsigned*)src->GetReadPtr(planes[p]);
        const unsigned* altp = (unsigned*)alt->GetReadPtr(planes[p]);
        const unsigned* mskp = (unsigned*)mask->GetReadPtr(planes[p]);
        unsigned* dstp = (unsigned*)dst->GetWritePtr(planes[p]);

        int width = (src->GetRowSize(planes[p]) + USIZE - 1) / USIZE;
        int height = src->GetHeight(planes[p]);

        int src_pitch = src->GetPitch(planes[p]) / USIZE;
        int alt_pitch = alt->GetPitch(planes[p]) / USIZE;
        int msk_pitch = mask->GetPitch(planes[p]) / USIZE;
        int dst_pitch = dst->GetPitch(planes[p]) / USIZE;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                dstp[x] = (srcp[x] & (~mskp[x])) | (altp[x] & mskp[x]);
            }
            srcp += src_pitch;
            altp += alt_pitch;
            mskp += msk_pitch;
            dstp += dst_pitch;
        }
    }
}


class MaskedMerge : public GenericVideoFilter {

    PClip altc;
    PClip maskc;
    int mi;
    int num_planes;

    bool (__stdcall *check_combed)(PVideoFrame& cmask, int mi,
                                   IScriptEnvironment* env);

    void (__stdcall *merge_frames)(int mum_planes, PVideoFrame& src,
                                   PVideoFrame& alt, PVideoFrame& mask,
                                   PVideoFrame& dst, IScriptEnvironment* env);

public:
    MaskedMerge(PClip c, PClip a, PClip m, int _mi, bool chroma, bool sse2,
                IScriptEnvironment* env);
    ~MaskedMerge() {}
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};


MaskedMerge::
MaskedMerge(PClip c, PClip a, PClip m, int _mi, bool chroma, bool sse2,
            IScriptEnvironment* env)
    : GenericVideoFilter(c), altc(a), maskc(m), mi(_mi)
{
    if (!vi.IsPlanar()) {
        env->ThrowError("MaskedMerge: planar format only.");
    }

    if (mi < 0 || mi > 128) {
        env->ThrowError("MaskedMerge: mi must be between 0 and 128.");
    }

    const VideoInfo& a_vi = altc->GetVideoInfo();
    const VideoInfo& m_vi = maskc->GetVideoInfo();
    if (!vi.IsSameColorspace(a_vi) || !vi.IsSameColorspace(m_vi)) {
        env->ThrowError("MaskedMerge: unmatch colorspaces.");
    }
    if (vi.width != a_vi.width || vi.width != m_vi.width ||
        vi.height != a_vi.height || vi.height != m_vi.height) {
        env->ThrowError("MaskedMerge: unmatch resolutions.");
    }

    num_planes = vi.IsY8() || !chroma ? 1 : 3;

    if (!(env->GetCPUFlags() & CPUF_SSE2) && sse2) {
        sse2 = false;
    }
    merge_frames = sse2 ? merge_frames_sse2 : merge_frames_c;
    check_combed = sse2 ? check_combed_sse2 : check_combed_c;
}


PVideoFrame __stdcall MaskedMerge::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame mask = maskc->GetFrame(n, env);
    if (mi > 0 && !check_combed(mask, mi, env)) {
        return src;
    }

    PVideoFrame alt = altc->GetFrame(n, env);
    PVideoFrame dst = env->NewVideoFrame(vi);

    merge_frames(num_planes, src, alt, mask, dst, env);

    if (num_planes == 1 && !vi.IsY8()) {
        const int src_pitch = src->GetPitch(PLANAR_U);
        const int dst_pitch = dst->GetPitch(PLANAR_U);
        const int width = src->GetRowSize(PLANAR_U);
        const int height = src->GetHeight(PLANAR_U);
        env->BitBlt(dst->GetWritePtr(PLANAR_U), dst_pitch,
                    src->GetReadPtr(PLANAR_U), src_pitch, width, height);
        env->BitBlt(dst->GetWritePtr(PLANAR_V), dst_pitch,
                    src->GetReadPtr(PLANAR_V), src_pitch, width, height);
    }

    return dst;
}


static AVSValue __cdecl
create_maskedmerge(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    if (!args[0].Defined()) {
        env->ThrowError("MaskedMerge: base clip is not set.");
    }
    if (!args[1].Defined()) {
        env->ThrowError("MaskedMerge: alt clip is not set.");
    }
    if (!args[2].Defined()) {
        env->ThrowError("MaskedMerge: mask clip is not set.");
    }
    return new MaskedMerge(args[0].AsClip(), args[1].AsClip(),
                           args[2].AsClip(), args[3].AsInt(40),
                           args[4].AsBool(true), args[5].AsBool(true),env);
}


static AVSValue __cdecl
create_iscombed(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    AVSValue cf = env->GetVar("current_frame");
    if (!cf.IsInt()) {
        env->ThrowError("IsCombed: This filter can only be used within"
                        " ConditionalFilter.");
    }
    int n = cf.AsInt();

    int mi = args[3].AsInt(40);
    if (mi < 0 || mi > 128) {
        env->ThrowError("IsCombed: MI must be between 0 and 128.");
    }

    bool sse2 = args[4].AsBool(true);

    CombMask *cm = new CombMask(args[0].AsClip(), args[1].AsInt(6),
                                args[2].AsInt(9), false, sse2, false, env);

    AVSValue is_combed = sse2 ? check_combed_sse2(cm->GetFrame(n, env), mi, env) :
                                check_combed_c(cm->GetFrame(n, env), mi, env);

    delete cm;

    return is_combed;
}


extern "C" __declspec(dllexport) const char* __stdcall
AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;
    env->AddFunction("CombMask", "c[cthresh]i[mthresh]i[chroma]b[sse2]b",
                     create_combmask, 0);
    env->AddFunction("MaskedMerge", "[base]c[alt]c[mask]c[MI]i[chroma]b[sse2]b",
                     create_maskedmerge, 0);
    env->AddFunction("IsCombed", "c[cthresh]i[mthresh]i[MI]i[sse2]b",
                     create_iscombed, 0);
    return "CombMask filter for Avisynth2.6 version "CMASK_VERSION;
}
