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


#ifndef WAIFU2X_DENOISE_H_
#define WAIFU2X_DENOISE_H_


#include "Waifu2x_Base.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Waifu2x_Denoise_Data
    : public Waifu2x_Data_Base
{
public:
    typedef Waifu2x_Denoise_Data _Myt;
    typedef Waifu2x_Data_Base _Mybase;

public:
    explicit Waifu2x_Denoise_Data(const VSAPI *_vsapi = nullptr, std::string _FunctionName = "Denoise", std::string _NameSpace = "waifu2x")
        : _Mybase(_vsapi, _FunctionName, _NameSpace)
    {}

    virtual int arguments_process(const VSMap *in, VSMap *out) override;

    virtual void init(VSCore *core) override;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Waifu2x_Denoise_Process
    : public Waifu2x_Process_Base
{
public:
    typedef Waifu2x_Denoise_Process _Myt;
    typedef Waifu2x_Process_Base _Mybase;
    typedef Waifu2x_Denoise_Data _Mydata;

private:
    const _Mydata &d;

public:
    Waifu2x_Denoise_Process(const _Mydata &_d, int _n, VSFrameContext *_frameCtx, VSCore *_core, const VSAPI *_vsapi)
        : _Mybase(_d, _n, _frameCtx, _core, _vsapi), d(_d)
    {}

protected:
    virtual void Kernel(FLType *dst, const FLType*src) const override;

    virtual void Kernel(FLType *dstY, FLType *dstU, FLType *dstV,
        const FLType *srcY, const FLType *srcU, const FLType *srcV) const override;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif
