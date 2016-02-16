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


#ifndef WAIFU2X_RESIZE_H_
#define WAIFU2X_RESIZE_H_


#include "Waifu2x_Base.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Waifu2x_Resize_Data
    : public Waifu2x_Data_Base
{
public:
    typedef Waifu2x_Resize_Data _Myt;
    typedef Waifu2x_Data_Base _Mybase;

public:
    int sr_times = 1;
    int sr_ratio = 1 << sr_times;
    bool resize_post = true;

    ZimgResizeContext *z_resize_pre = nullptr;
    ZimgResizeContext *z_resize_post = nullptr;
    ZimgResizeContext *z_resize_uv = nullptr;

public:
    explicit Waifu2x_Resize_Data(const VSAPI *_vsapi = nullptr, std::string _FunctionName = "Resize", std::string _NameSpace = "waifu2x")
        : _Mybase(_vsapi, _FunctionName, _NameSpace)
    {}

    Waifu2x_Resize_Data(_Myt &&right)
        : _Mybase(std::move(right))
    {
        moveFrom(right);
    }

    _Myt &operator=(_Myt &&right)
    {
        _Mybase::operator=(std::move(right));
        moveFrom(right);
    }

    virtual ~Waifu2x_Resize_Data() override
    {
        release();
    }

    virtual int arguments_process(const VSMap *in, VSMap *out) override;

    virtual void init(VSCore *core) override;

protected:
    void release();

    void moveFrom(_Myt &right);

protected:
    static void init_z_resize(ZimgResizeContext *&context,
        int filter_type, int src_width, int src_height, int dst_width, int dst_height,
        double shift_w, double shift_h, double subwidth, double subheight,
        double filter_param_a, double filter_param_b);
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Waifu2x_Resize_Process
    : public Waifu2x_Process_Base
{
public:
    typedef Waifu2x_Resize_Process _Myt;
    typedef Waifu2x_Process_Base _Mybase;
    typedef Waifu2x_Resize_Data _Mydata;

private:
    const _Mydata &d;

protected:
    PCType sr_height;
    PCType sr_width;
    PCType sr_stride;
    PCType sr_pcount;

public:
    Waifu2x_Resize_Process(const _Mydata &_d, int _n, VSFrameContext *_frameCtx, VSCore *_core, const VSAPI *_vsapi)
        : _Mybase(_d, _n, _frameCtx, _core, _vsapi), d(_d)
    {
        if (d.resize_post)
        {
            sr_height = src_height[0] * d.sr_ratio;
            sr_width = src_width[0] * d.sr_ratio;
            sr_stride = src_stride[0] * d.sr_ratio;
            sr_pcount = sr_height * sr_stride;
        }
        else
        {
            sr_height = dst_height[0];
            sr_width = dst_width[0];
            sr_stride = dst_stride[0];
            sr_pcount = sr_height * sr_stride;
        }
    }

    static const VSFormat *NewFormat(const _Mydata &d, const VSFormat *f, VSCore *core, const VSAPI *vsapi)
    {
        return vsapi->registerFormat(f->colorFamily, f->sampleType, f->bitsPerSample,
            d.para.subsample_w, d.para.subsample_h, core);
    }

protected:
    virtual void NewFormat() override
    {
        dfi = NewFormat(d, fi, core, vsapi);
    }

    virtual void NewFrame() override
    {
        _NewFrame(d.para.width, d.para.height, false);
    }

    virtual void Kernel(FLType *dst, const FLType*src) const override;

    virtual void Kernel(FLType *dstY, FLType *dstU, FLType *dstV,
        const FLType *srcY, const FLType *srcU, const FLType *srcV) const override;

    void Kernel_Y(FLType *dst, const FLType *src, void *buf) const;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif
