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


#ifndef WAIFU2X_BASE_HPP_
#define WAIFU2X_BASE_HPP_


#include "Conversion.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template < typename _Ty >
void Waifu2x_Process_Base::process_core()
{
    if (fi->colorFamily == cmGray)
    {
        process_core_gray<_Ty>();
    }
    else if (fi->colorFamily == cmYUV || fi->colorFamily == cmYCoCg)
    {
        process_core_yuv<_Ty>();
    }
    else if (fi->colorFamily == cmRGB)
    {
        process_core_rgb<_Ty>();
    }
}


template < typename _Ty >
void Waifu2x_Process_Base::process_core_gray()
{
    FLType *dstYd = nullptr, *srcYd = nullptr;
    //FLType *refYd = nullptr;

    // Get write/read pointer
    auto dstY = reinterpret_cast<_Ty *>(vsapi->getWritePtr(dst, 0));
    auto srcY = reinterpret_cast<const _Ty *>(vsapi->getReadPtr(src, 0));

    // Allocate memory for floating point Y data
    AlignedMalloc(dstYd, dst_pcount[0]);
    AlignedMalloc(srcYd, src_pcount[0]);

    // Convert src and ref from integer Y data to floating point Y data
    Int2Float(srcYd, srcY, src_height[0], src_width[0], src_stride[0], src_stride[0], false, d.para.full, false);

    // Execute kernel
    Kernel(dstYd, srcYd);

    // Convert dst from floating point Y data to integer Y data
    Float2Int(dstY, dstYd, dst_height[0], dst_width[0], dst_stride[0], dst_stride[0], false, d.para.full, !isFloat(_Ty));

    // Free memory for floating point Y data
    AlignedFree(dstYd);
    AlignedFree(srcYd);
}

template <>
inline void Waifu2x_Process_Base::process_core_gray<FLType>()
{
    // Get write/read pointer
    auto dstY = reinterpret_cast<FLType *>(vsapi->getWritePtr(dst, 0));
    auto srcY = reinterpret_cast<const FLType *>(vsapi->getReadPtr(src, 0));

    // Execute kernel
    Kernel(dstY, srcY);
}


template < typename _Ty >
void Waifu2x_Process_Base::process_core_yuv()
{
    FLType *dstYd = nullptr, *dstUd = nullptr, *dstVd = nullptr;
    FLType *srcYd = nullptr, *srcUd = nullptr, *srcVd = nullptr;

    // Get write/read pointer
    auto dstY = reinterpret_cast<_Ty *>(vsapi->getWritePtr(dst, 0));
    auto dstU = reinterpret_cast<_Ty *>(vsapi->getWritePtr(dst, 1));
    auto dstV = reinterpret_cast<_Ty *>(vsapi->getWritePtr(dst, 2));

    auto srcY = reinterpret_cast<const _Ty *>(vsapi->getReadPtr(src, 0));
    auto srcU = reinterpret_cast<const _Ty *>(vsapi->getReadPtr(src, 1));
    auto srcV = reinterpret_cast<const _Ty *>(vsapi->getReadPtr(src, 2));

    // Allocate memory for floating point YUV data
    AlignedMalloc(dstYd, dst_pcount[0]);
    if (d.process[1]) AlignedMalloc(dstUd, dst_pcount[1]);
    if (d.process[2]) AlignedMalloc(dstVd, dst_pcount[2]);

    AlignedMalloc(srcYd, src_pcount[0]);
    if (d.process[1]) AlignedMalloc(srcUd, src_pcount[1]);
    if (d.process[2]) AlignedMalloc(srcVd, src_pcount[2]);

    // Convert src and ref from integer YUV data to floating point YUV data
    Int2Float(srcYd, srcY, src_height[0], src_width[0], src_stride[0], src_stride[0], false, d.para.full, false);
    if (d.process[1]) Int2Float(srcUd, srcU, src_height[1], src_width[1], src_stride[1], src_stride[1], true, d.para.full, false);
    if (d.process[2]) Int2Float(srcVd, srcV, src_height[2], src_width[2], src_stride[2], src_stride[2], true, d.para.full, false);

    // Execute kernel
    if (d.chroma) Kernel(dstYd, dstUd, dstVd, srcYd, srcUd, srcVd);
    else Kernel(dstYd, srcYd);

    // Convert dst from floating point YUV data to integer YUV data
    Float2Int(dstY, dstYd, dst_height[0], dst_width[0], dst_stride[0], dst_stride[0], false, d.para.full, !isFloat(_Ty));
    if (d.process[1]) Float2Int(dstU, dstUd, dst_height[1], dst_width[1], dst_stride[1], dst_stride[1], true, d.para.full, !isFloat(_Ty));
    if (d.process[2]) Float2Int(dstV, dstVd, dst_height[2], dst_width[2], dst_stride[2], dst_stride[2], true, d.para.full, !isFloat(_Ty));

    // Free memory for floating point YUV data
    AlignedFree(dstYd);
    if (d.process[1]) AlignedFree(dstUd);
    if (d.process[2]) AlignedFree(dstVd);

    AlignedFree(srcYd);
    if (d.process[1]) AlignedFree(srcUd);
    if (d.process[2]) AlignedFree(srcVd);
}

template <>
inline void Waifu2x_Process_Base::process_core_yuv<FLType>()
{
    // Get write/read pointer
    auto dstY = reinterpret_cast<FLType *>(vsapi->getWritePtr(dst, 0));
    auto dstU = reinterpret_cast<FLType *>(vsapi->getWritePtr(dst, 1));
    auto dstV = reinterpret_cast<FLType *>(vsapi->getWritePtr(dst, 2));

    auto srcY = reinterpret_cast<const FLType *>(vsapi->getReadPtr(src, 0));
    auto srcU = reinterpret_cast<const FLType *>(vsapi->getReadPtr(src, 1));
    auto srcV = reinterpret_cast<const FLType *>(vsapi->getReadPtr(src, 2));

    // Execute kernel
    if (d.chroma) Kernel(dstY, dstU, dstV, srcY, srcU, srcV);
    else Kernel(dstY, srcY);
}


template < typename _Ty >
void Waifu2x_Process_Base::process_core_rgb()
{
    FLType *dstYd = nullptr, *dstUd = nullptr, *dstVd = nullptr;
    FLType *srcYd = nullptr, *srcUd = nullptr, *srcVd = nullptr;

    // Get write/read pointer
    auto dstR = reinterpret_cast<_Ty *>(vsapi->getWritePtr(dst, 0));
    auto dstG = reinterpret_cast<_Ty *>(vsapi->getWritePtr(dst, 1));
    auto dstB = reinterpret_cast<_Ty *>(vsapi->getWritePtr(dst, 2));

    auto srcR = reinterpret_cast<const _Ty *>(vsapi->getReadPtr(src, 0));
    auto srcG = reinterpret_cast<const _Ty *>(vsapi->getReadPtr(src, 1));
    auto srcB = reinterpret_cast<const _Ty *>(vsapi->getReadPtr(src, 2));

    // Allocate memory for floating point YUV data
    AlignedMalloc(dstYd, dst_pcount[0]);
    if (d.chroma) AlignedMalloc(dstUd, dst_pcount[1]);
    if (d.chroma) AlignedMalloc(dstVd, dst_pcount[2]);

    AlignedMalloc(srcYd, src_pcount[0]);
    AlignedMalloc(srcUd, src_pcount[1]);
    AlignedMalloc(srcVd, src_pcount[2]);

    // Convert src and ref from RGB data to floating point YUV data
    RGB2FloatYUV(srcYd, srcUd, srcVd, srcR, srcG, srcB,
        src_height[0], src_width[0], src_stride[0], src_stride[0],
        d.para.matrix, d.para.full, false);

    // Execute kernel
    if (d.chroma)
    {
        Kernel(dstYd, dstUd, dstVd, srcYd, srcUd, srcVd);
    }
    else
    {
        Kernel(dstYd, srcYd);
        dstUd = srcUd;
        dstVd = srcVd;
    }

    // Convert dst from floating point YUV data to RGB data
    FloatYUV2RGB(dstR, dstG, dstB, dstYd, dstUd, dstVd,
        dst_height[0], dst_width[0], dst_stride[0], dst_stride[0],
        d.para.matrix, d.para.full, !isFloat(_Ty));

    // Free memory for floating point YUV data
    AlignedFree(dstYd);
    if (d.chroma) AlignedFree(dstUd);
    if (d.chroma) AlignedFree(dstVd);

    AlignedFree(srcYd);
    AlignedFree(srcUd);
    AlignedFree(srcVd);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif
