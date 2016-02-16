/*
* Bilateral filter - VapourSynth plugin
* Copyright (C) 2014  mawen1250
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef HELPER_H_
#define HELPER_H_


#include <vector>
#include <cfloat>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


typedef float FLType;


const FLType FLType_MAX = sizeof(FLType) < 8 ? FLT_MAX : DBL_MAX;
const FLType FLType_MIN = sizeof(FLType) < 8 ? FLT_MIN : DBL_MIN;


template < typename T >
T Min(T a, T b)
{
    return b < a ? b : a;
}

template < typename T >
T Max(T a, T b)
{
    return b > a ? b : a;
}

template < typename T >
T Clip(T input, T Floor, T Ceil)
{
    return input <= Floor ? Floor : input >= Ceil ? Ceil : input;
}

template <typename T>
inline T Abs(T input)
{
    return input < 0 ? -input : input;
}

template <typename T>
inline T Round_Div(T dividend, T divisor)
{
    return (dividend + divisor / 2) / divisor;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const size_t Alignment = 32;


template < typename T >
int stride_cal(int width)
{
    size_t Alignment2 = Alignment / sizeof(T);
    return static_cast<int>(width % Alignment2 == 0 ? width : (width / Alignment2 + 1) * Alignment2);
}


template < typename T >
void data2buff(T * dst, const T * src, int xoffset, int yoffset,
    int bufheight, int bufwidth, int bufstride, int height, int width, int stride)
{
    int x, y;
    T *dstp;
    const T *srcp;

    for (y = 0; y < height; y++)
    {
        dstp = dst + (yoffset + y) * bufstride;
        srcp = src + y * stride;
        for (x = 0; x < xoffset; x++)
            dstp[x] = srcp[0];
        memcpy(dstp + xoffset, srcp, sizeof(T)*width);
        for (x = xoffset + width; x < bufwidth; x++)
            dstp[x] = srcp[width - 1];
    }

    srcp = dst + yoffset * bufstride;
    for (y = 0; y < yoffset; y++)
    {
        dstp = dst + y * bufstride;
        memcpy(dstp, srcp, sizeof(T)*bufwidth);
    }

    srcp = dst + (yoffset + height - 1) * bufstride;
    for (y = yoffset + height; y < bufheight; y++)
    {
        dstp = dst + y * bufstride;
        memcpy(dstp, srcp, sizeof(T)*bufwidth);
    }
}

template < typename T >
T * newbuff(const T * src, int xoffset, int yoffset,
    int bufheight, int bufwidth, int bufstride, int height, int width, int stride)
{
    T * dst = vs_aligned_malloc<T>(sizeof(T)*bufheight*bufstride, Alignment);
    data2buff(dst, src, xoffset, yoffset, bufheight, bufwidth, bufstride, height, width, stride);
    return dst;
}

template < typename T >
void freebuff(T * buff)
{
    vs_aligned_free(buff);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif