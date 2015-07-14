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


#ifndef WAIFU2X_BASE_H_
#define WAIFU2X_BASE_H_


#include <mutex>
#include "Helper.h"
#include <waifu2x.hpp>
#include "zimg_helper.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constants


const bool USE_VAPOURSYNTH_MT = true;
const bool USE_INTERNAL_MT = true;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct Waifu2x_Para
{
    // General parameters
    PCType block_width = 1280;
    PCType block_height = 1280;
    int threads = 1;
    ColorMatrix matrix = ColorMatrix::smpte170m;
    bool full = true;

    // Denoise parameters
    int mode = 1;

    // Resize parameters
    PCType width = -1;
    PCType height = -1;
    double shift_w = 0;
    double shift_h = 0;
    double subwidth = 0;
    double subheight = 0;
    int filter = ZIMG_RESIZE_BICUBIC;
    double filter_param_a = 0;
    double filter_param_b = 0.5;
    int filter_uv = ZIMG_RESIZE_BICUBIC;
    double filter_param_a_uv = 0;
    double filter_param_b_uv = 0.5;
    int subsample_w = 0;
    int subsample_h = 0;
    chroma_location chroma_loc_in = CHROMA_LOC_MPEG2;
    chroma_location chroma_loc_out = CHROMA_LOC_MPEG2;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Waifu2x_Data_Base
    : public VSData
{
public:
    typedef Waifu2x_Data_Base _Myt;
    typedef VSData _Mybase;
    typedef Waifu2x_Para _Mypara;

public:
    _Mypara para;
    _Mypara para_default;
    bool chroma = true;

    std::vector<Waifu2x *> waifu2x;
    std::vector<std::mutex *> waifu2x_mutex;

public:
    explicit Waifu2x_Data_Base(const VSAPI *_vsapi = nullptr, std::string _FunctionName = "Base", std::string _NameSpace = "waifu2x")
        : _Mybase(_vsapi, _FunctionName, _NameSpace)
    {}

    Waifu2x_Data_Base(_Myt &&right)
        : _Mybase(std::move(right))
    {
        moveFrom(right);
    }

    _Myt &operator=(_Myt &&right)
    {
        _Mybase::operator=(std::move(right));
        moveFrom(right);
    }

    virtual ~Waifu2x_Data_Base() override
    {
        release();
    }

    virtual int arguments_process(const VSMap *in, VSMap *out) override;

    virtual void init(VSCore *core) = 0;

protected:
    void release();

    void moveFrom(_Myt &right);

protected:
    static void init_waifu2x(std::vector<Waifu2x *> &context, std::vector<std::mutex *> &mutex,
        int model, int threads, PCType width, PCType height, PCType block_width, PCType block_height,
        VSCore *core, const VSAPI *vsapi);

    static PCType waifu2x_get_optimal_block_size(PCType size, PCType block_size, PCType padding);
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Waifu2x_Process_Base
    : public VSProcess
{
public:
    typedef Waifu2x_Process_Base _Myt;
    typedef VSProcess _Mybase;
    typedef Waifu2x_Data_Base _Mydata;

private:
    const _Mydata &d;

protected:
    Waifu2x *waifu2x = nullptr;
    std::mutex *waifu2x_mutex = nullptr;

private:
    template < typename _Ty >
    void process_core();

    template < typename _Ty >
    void process_core_gray();

    template < typename _Ty >
    void process_core_yuv();

    template < typename _Ty >
    void process_core_rgb();

protected:
    virtual void process_core8() override { process_core<uint8_t>(); };
    virtual void process_core16() override { process_core<uint16_t>(); };
    virtual void process_coreS() override { process_core<float>(); }

public:
    Waifu2x_Process_Base(const _Mydata &_d, int _n, VSFrameContext *_frameCtx, VSCore *_core, const VSAPI *_vsapi)
        : _Mybase(_d, _n, _frameCtx, _core, _vsapi), d(_d)
    {}

    virtual ~Waifu2x_Process_Base() override
    {
        release_mutex();
    }

    void set_mutex(std::vector<Waifu2x *> &context, std::vector<std::mutex *> &mutex);

    void release_mutex();

protected:
    virtual void Kernel(FLType *dst, const FLType*src) const = 0;

    virtual void Kernel(FLType *dstY, FLType *dstU, FLType *dstV,
        const FLType *srcY, const FLType *srcU, const FLType *srcV) const = 0;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#include "Waifu2x_Base.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif
