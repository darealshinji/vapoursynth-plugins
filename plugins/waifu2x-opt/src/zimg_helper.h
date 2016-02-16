/*
* Waifu2x-opt image restoration filter - VapourSynth plugin
* Copyright (C) 2015  mawen1250
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


#ifndef ZIMG_HELPER_H_
#define ZIMG_HELPER_H_


#include <cstring>
#include <zimg++.hpp>
#include <vapoursynth/VapourSynth.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Enumerators


enum chroma_location
{
    CHROMA_LOC_MPEG1,
    CHROMA_LOC_MPEG2
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// zimg helper functions


inline int zimg_translate_dither(const char *dither)
{
    if (!strcmp(dither, "none"))
        return ZIMG_DITHER_NONE;
    else if (!strcmp(dither, "ordered"))
        return ZIMG_DITHER_ORDERED;
    else if (!strcmp(dither, "random"))
        return ZIMG_DITHER_RANDOM;
    else if (!strcmp(dither, "error_diffusion"))
        return ZIMG_DITHER_ERROR_DIFFUSION;
    else
        return -1;
}


inline int zimg_translate_pixel(const VSFormat *format)
{
    if (format->sampleType == stInteger && format->bytesPerSample == 1)
        return ZIMG_PIXEL_BYTE;
    else if (format->sampleType == stInteger && format->bytesPerSample == 2)
        return ZIMG_PIXEL_WORD;
    else if (format->sampleType == stFloat && format->bitsPerSample == 16)
        return ZIMG_PIXEL_HALF;
    else if (format->sampleType == stFloat && format->bitsPerSample == 32)
        return ZIMG_PIXEL_FLOAT;
    else
        return -1;
}


inline int zimg_translate_filter(const char *filter)
{
    if (!strcmp(filter, "point"))
        return ZIMG_RESIZE_POINT;
    else if (!strcmp(filter, "bilinear"))
        return ZIMG_RESIZE_BILINEAR;
    else if (!strcmp(filter, "bicubic"))
        return ZIMG_RESIZE_BICUBIC;
    else if (!strcmp(filter, "spline16"))
        return ZIMG_RESIZE_SPLINE16;
    else if (!strcmp(filter, "spline36"))
        return ZIMG_RESIZE_SPLINE36;
    else if (!strcmp(filter, "lanczos"))
        return ZIMG_RESIZE_LANCZOS;
    else
        return -1;
}


inline double chroma_h_mpeg1_distance(chroma_location chroma_loc, int subsample)
{
    return (chroma_loc == CHROMA_LOC_MPEG2 && subsample >= 1) ? -0.5 : 0.0;
}


inline double chroma_adjust_h(chroma_location loc_in, chroma_location loc_out, int subsample_in, int subsample_out)
{
    double scale = 1.0 / static_cast<double>(1 << subsample_in);
    double to_444_offset = -chroma_h_mpeg1_distance(loc_in, subsample_in) * scale;
    double from_444_offset = chroma_h_mpeg1_distance(loc_out, subsample_out) * scale;

    return to_444_offset + from_444_offset;
}


inline double chroma_adjust_v(chroma_location loc_in, chroma_location loc_out, int subsample_in, int subsample_out)
{
    return 0.0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif
