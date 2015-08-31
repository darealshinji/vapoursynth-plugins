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


#include <algorithm>
#include "Waifu2x_Resize.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int Waifu2x_Resize_Data::arguments_process(const VSMap *in, VSMap *out)
{
    if (_Mybase::arguments_process(in, out))
    {
        return 1;
    }

    int error;

    // width - int
    para.width = int64ToIntS(vsapi->propGetInt(in, "width", 0, &error));

    if (error)
    {
        para.width = vi->width * 2;
    }
    else if (para.width <= 0)
    {
        setError(out, "\'width\' must be a positive integer");
        return 1;
    }
    else if (para.width % (1 << vi->format->subSamplingW) != 0)
    {
        setError(out, "\'width\' must be a multiplicate of horizontal sub-sampling ratio");
        return 1;
    }

    // height - int
    para.height = int64ToIntS(vsapi->propGetInt(in, "height", 0, &error));

    if (error)
    {
        para.height = vi->height * 2;
    }
    else if (para.height <= 0)
    {
        setError(out, "\'height\' must be a positive integer");
        return 1;
    }
    else if (para.height % (1 << vi->format->subSamplingH) != 0)
    {
        setError(out, "\'height\' must be a multiplicate of vertical sub-sampling ratio");
        return 1;
    }

    // shift_w - float
    para.shift_w = vsapi->propGetFloat(in, "shift_w", 0, &error);

    if (error)
    {
        para.shift_w = para_default.shift_w;
    }

    // shift_h - float
    para.shift_h = vsapi->propGetFloat(in, "shift_h", 0, &error);

    if (error)
    {
        para.shift_h = para_default.shift_h;
    }

    // subwidth - float
    para.subwidth = vsapi->propGetFloat(in, "subwidth", 0, &error);

    if (error)
    {
        para.subwidth = para_default.subwidth;
    }
    else if (para.subwidth < 0 && para.shift_w - para.subwidth >= vi->width)
    {
        setError(out, "The source width set by \"shift_w\" and \"subwidth\" must be positive");
        return 1;
    }

    // subheight - float
    para.subheight = vsapi->propGetFloat(in, "subheight", 0, &error);

    if (error)
    {
        para.subheight = para_default.subheight;
    }
    else if (para.subheight < 0 && para.shift_h - para.subheight >= vi->width)
    {
        setError(out, "The source width set by \"shift_h\" and \"subheight\" must be positive");
        return 1;
    }

    // filter - data
    auto filter_cstr = vsapi->propGetData(in, "filter", 0, &error);

    if (error)
    {
        para.filter = para_default.filter;
    }
    else
    {
        std::string filter = filter_cstr;
        std::transform(filter.begin(), filter.end(), filter.begin(), tolower);
        para.filter = zimg_translate_filter(filter.c_str());

        if (para.filter < 0)
        {
            setError(out, "Invalid \'filter\' specified");
            return 1;
        }
    }

    // filter_param_a - float
    para.filter_param_a = vsapi->propGetFloat(in, "filter_param_a", 0, &error);

    if (error)
    {
        if (para.filter == ZIMG_RESIZE_LANCZOS)
        {
            para.filter_param_a = 3;
        }
        else
        {
            para.filter_param_a = para_default.filter_param_a;
        }
    }

    // filter_param_b - float
    para.filter_param_b = vsapi->propGetFloat(in, "filter_param_b", 0, &error);

    if (error)
    {
        if (para.filter == ZIMG_RESIZE_LANCZOS)
        {
            para.filter_param_b = 0;
        }
        else
        {
            para.filter_param_b = para_default.filter_param_b;
        }
    }

    // filter_uv - data
    auto filter_uv_cstr = vsapi->propGetData(in, "filter_uv", 0, &error);

    if (error)
    {
        para.filter_uv = para_default.filter_uv;
    }
    else
    {
        std::string filter_uv = filter_uv_cstr;
        std::transform(filter_uv.begin(), filter_uv.end(), filter_uv.begin(), tolower);
        para.filter_uv = zimg_translate_filter(filter_uv.c_str());

        if (para.filter_uv < 0)
        {
            setError(out, "Invalid \'filter_uv\' specified");
            return 1;
        }
    }

    // filter_param_a_uv - float
    para.filter_param_a_uv = vsapi->propGetFloat(in, "filter_param_a_uv", 0, &error);

    if (error)
    {
        if (para.filter_uv == ZIMG_RESIZE_LANCZOS)
        {
            para.filter_param_a_uv = 3;
        }
        else
        {
            para.filter_param_a_uv = para_default.filter_param_a_uv;
        }
    }

    // filter_param_b_uv - float
    para.filter_param_b_uv = vsapi->propGetFloat(in, "filter_param_b_uv", 0, &error);

    if (error)
    {
        if (para.filter_uv == ZIMG_RESIZE_LANCZOS)
        {
            para.filter_param_b_uv = 0;
        }
        else
        {
            para.filter_param_b_uv = para_default.filter_param_b_uv;
        }
    }

    // subsample_w - int
    para.subsample_w = int64ToIntS(vsapi->propGetInt(in, "subsample_w", 0, &error));

    if (error)
    {
        para.subsample_w = vi->format->subSamplingW;
    }
    else if (vi->format->colorFamily != cmYUV && vi->format->colorFamily != cmYCoCg
        && para.subsample_w != 0)
    {
        setError(out, "Invalid \'subsample_w\' specified for this color family, should be 0");
        return 1;
    }

    // subsample_h - int
    para.subsample_h = int64ToIntS(vsapi->propGetInt(in, "subsample_h", 0, &error));

    if (error)
    {
        para.subsample_h = vi->format->subSamplingH;
    }
    else if (vi->format->colorFamily != cmYUV && vi->format->colorFamily != cmYCoCg
        && para.subsample_h != 0)
    {
        setError(out, "Invalid \'subsample_h\' specified for the output color family, should be 0");
        return 1;
    }

    // chroma_loc_in - data
    auto chroma_loc_cstr = vsapi->propGetData(in, "chroma_loc_in", 0, &error);

    if (error)
    {
        para.chroma_loc_in = para_default.chroma_loc_in;
    }
    else
    {
        std::string chroma_loc = chroma_loc_cstr;
        std::transform(chroma_loc.begin(), chroma_loc.end(), chroma_loc.begin(), tolower);

        if (chroma_loc == "mpeg1")
        {
            para.chroma_loc_in = CHROMA_LOC_MPEG1;
        }
        else if (chroma_loc == "mpeg2")
        {
            para.chroma_loc_in = CHROMA_LOC_MPEG2;
        }
        else
        {
            setError(out, "Invalid \'chroma_loc_in\' specified, should be \'mpeg1\' or \'mpeg2\'");
            return 1;
        }
    }

    // chroma_loc_out - data
    chroma_loc_cstr = vsapi->propGetData(in, "chroma_loc_out", 0, &error);

    if (error)
    {
        para.chroma_loc_out = para.chroma_loc_in;
    }
    else
    {
        std::string chroma_loc = chroma_loc_cstr;
        std::transform(chroma_loc.begin(), chroma_loc.end(), chroma_loc.begin(), tolower);

        if (chroma_loc == "mpeg1")
        {
            para.chroma_loc_out = CHROMA_LOC_MPEG1;
        }
        else if (chroma_loc == "mpeg2")
        {
            para.chroma_loc_out = CHROMA_LOC_MPEG2;
        }
        else
        {
            setError(out, "Invalid \'chroma_loc_out\' specified, should be \'mpeg1\' or \'mpeg2\'");
            return 1;
        }
    }

    // process
    chroma = vi->format->colorFamily != cmGray;

    return 0;
}


void Waifu2x_Resize_Data::init(VSCore *core)
{
    // Process resizing parameters
    int src_width = vi->width;
    int src_height = vi->height;
    int sr_width = src_width * sr_ratio;
    int sr_height = src_height * sr_ratio;
    int dst_width = para.width;
    int dst_height = para.height;
    double shift_w = para.shift_w;
    double shift_h = para.shift_h;
    double subwidth = para.subwidth <= 0 ? src_width - para.subwidth : para.subwidth;
    double subheight = para.subheight <= 0 ? src_height - para.subheight : para.height;

    double scaleH = static_cast<double>(dst_width) / subwidth;
    double scaleV = static_cast<double>(dst_height) / subheight;
    double src_sub_w = 1 << vi->format->subSamplingW;
    double src_sub_h = 1 << vi->format->subSamplingH;
    double dst_sub_w = 1 << para.subsample_w;
    double dst_sub_h = 1 << para.subsample_h;

    bool sCLeftAlign = para.chroma_loc_in == CHROMA_LOC_MPEG2;
    double sHCPlace = !sCLeftAlign ? 0 : 0.5 - src_sub_w / 2;
    double sVCPlace = 0;
    bool dCLeftAlign = para.chroma_loc_out == CHROMA_LOC_MPEG2;
    double dHCPlace = !dCLeftAlign ? 0 : 0.5 - dst_sub_w / 2;
    double dVCPlace = 0;

    int src_width_uv = src_width >> vi->format->subSamplingW;
    int src_height_uv = src_height >> vi->format->subSamplingH;
    int dst_width_uv = dst_width >> para.subsample_w;
    int dst_height_uv = dst_height >> para.subsample_h;
    double shift_w_uv = ((shift_w - sHCPlace) * scaleH + dHCPlace) / scaleH / src_sub_w;
    double shift_h_uv = ((shift_h - sVCPlace) * scaleV + dVCPlace) / scaleV / src_sub_h;
    double subwidth_uv = subwidth / src_sub_w;
    double subheight_uv = subheight / src_sub_h;

    // Additional parameters
    resize_post = !(dst_width == sr_width && dst_height == sr_height
        && shift_w == 0 && shift_h == 0 && subwidth == sr_width && subheight == sr_height);

    // Initialize z_resize
    init_z_resize(z_resize_pre, ZIMG_RESIZE_POINT,
        src_width, src_height, sr_width, sr_height,
        0, 0, src_width, src_height,
        0, 0.5);
    if (resize_post) init_z_resize(z_resize_post, para.filter,
        sr_width, sr_height, dst_width, dst_height,
        shift_w * sr_ratio, shift_h * sr_ratio, subwidth * sr_ratio, subheight * sr_ratio,
        para.filter_param_a, para.filter_param_b);
    init_z_resize(z_resize_uv, para.filter_uv,
        src_width_uv, src_height_uv, dst_width_uv, dst_height_uv,
        shift_w_uv, shift_h_uv, subwidth_uv, subheight_uv,
        para.filter_param_a_uv, para.filter_param_b_uv);

    // Initialize waifu2x
    init_waifu2x(waifu2x, waifu2x_mutex, 0, USE_INTERNAL_MT ? para.threads : 1,
        sr_width, sr_height, para.block_width, para.block_height, core, vsapi);
}


void Waifu2x_Resize_Data::release()
{
    delete z_resize_pre;
    delete z_resize_post;
    delete z_resize_uv;

    z_resize_pre = nullptr;
    z_resize_post = nullptr;
    z_resize_uv = nullptr;
}


void Waifu2x_Resize_Data::moveFrom(_Myt &right)
{
    z_resize_pre = right.z_resize_pre;
    z_resize_post = right.z_resize_post;
    z_resize_uv = right.z_resize_uv;
    
    right.z_resize_pre = nullptr;
    right.z_resize_post = nullptr;
    right.z_resize_uv = nullptr;
}


void Waifu2x_Resize_Data::init_z_resize(ZimgResizeContext *&context,
    int filter_type, int src_width, int src_height, int dst_width, int dst_height,
    double shift_w, double shift_h, double subwidth, double subheight,
    double filter_param_a, double filter_param_b)
{
    context = new ZimgResizeContext(filter_type, src_width, src_height,
        dst_width, dst_height, shift_w, shift_h, subwidth, subheight, filter_param_a, filter_param_b);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const int pixel_type = ZIMG_PIXEL_FLOAT;


void Waifu2x_Resize_Process::Kernel(FLType *dst, const FLType *src) const
{
    void *buf = nullptr;
    size_t buf_size_pre = d.z_resize_pre->tmp_size(pixel_type);
    size_t buf_size_post = d.resize_post ? d.z_resize_post->tmp_size(pixel_type) : 0;
    size_t buf_size = std::max(buf_size_pre, buf_size_post);
    VS_ALIGNED_MALLOC(&buf, buf_size, 32);

    Kernel_Y(dst, src, buf);

    VS_ALIGNED_FREE(buf);
}


void Waifu2x_Resize_Process::Kernel(FLType *dstY, FLType *dstU, FLType *dstV,
    const FLType *srcY, const FLType *srcU, const FLType *srcV) const
{
    void *buf = nullptr;
    size_t buf_size_pre = d.z_resize_pre->tmp_size(pixel_type);
    size_t buf_size_post = d.resize_post ? d.z_resize_post->tmp_size(pixel_type) : 0;
    size_t buf_size_uv = d.z_resize_uv->tmp_size(pixel_type);
    size_t buf_size = std::max(std::max(buf_size_pre, buf_size_post), buf_size_uv);
    VS_ALIGNED_MALLOC(&buf, buf_size, 32);

    Kernel_Y(dstY, srcY, buf);

    d.z_resize_uv->process(srcU, dstU, buf, src_width[1], src_height[1], dst_width[1], dst_height[1],
        src_stride[1] * sizeof(FLType), dst_stride[1] * sizeof(FLType), pixel_type);
    d.z_resize_uv->process(srcV, dstV, buf, src_width[2], src_height[2], dst_width[2], dst_height[2],
        src_stride[2] * sizeof(FLType), dst_stride[2] * sizeof(FLType), pixel_type);

    VS_ALIGNED_FREE(buf);
}


void Waifu2x_Resize_Process::Kernel_Y(FLType *dst, const FLType *src, void *buf) const
{
    FLType *temp1 = nullptr, *temp2 = nullptr;
    AlignedMalloc(temp1, sr_pcount);
    if (d.resize_post) AlignedMalloc(temp2, sr_pcount);
    else temp2 = dst;

    d.z_resize_pre->process(src, temp1, buf, src_width[0], src_height[0], sr_width, sr_height,
        src_stride[0] * sizeof(FLType), sr_stride * sizeof(FLType), pixel_type);
    waifu2x->process(temp2, temp1, sr_width, sr_height, sr_stride, false);
    if (d.resize_post) d.z_resize_post->process(temp2, dst, buf, sr_width, sr_height, dst_width[0], dst_height[0],
        sr_stride * sizeof(FLType), dst_stride[0] * sizeof(FLType), pixel_type);

    AlignedFree(temp1);
    if (d.resize_post) AlignedFree(temp2);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
