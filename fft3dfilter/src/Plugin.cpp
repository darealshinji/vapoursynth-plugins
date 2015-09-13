/*****************************************************************************
 * Plugin.cpp
 *****************************************************************************
 * Copyright (C) 2015
 *
 * Authors: Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

#include <string>

#include "config.h"
#include "VapourSynth.h"
#include "FFT3DFilter.h"

static inline void set_option_int64
(
    int64_t     *opt,
    int64_t      default_value,
    const char  *arg,
    const VSMap *in,
    const VSAPI *vsapi
)
{
    int e;
    *opt = vsapi->propGetInt( in, arg, 0, &e );
    if( e )
        *opt = default_value;
}

static inline void set_option_float
(
    float       *opt,
    float        default_value,
    const char  *arg,
    const VSMap *in,
    const VSAPI *vsapi
)
{
    int e;
    *opt = static_cast<float>(vsapi->propGetFloat( in, arg, 0, &e ));
    if( e )
        *opt = static_cast<float>(default_value);
}

static void VS_CC initFFT3DFilter
(
    VSMap       *in,
    VSMap       *out,
    void       **instance_data,
    VSNode      *node,
    VSCore      *core,
    const VSAPI *vsapi
)
{
    FFT3DFilterMulti *d = static_cast<FFT3DFilterMulti *>(*instance_data);
    vsapi->setVideoInfo( &d->vi, 1, node );
}

static const VSFrameRef * VS_CC getFrameFFT3DFilter
(
    int             n,
    int             activation_reason,
    void          **instance_data,
    void          **frame_data,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    FFT3DFilterMulti *d = static_cast<FFT3DFilterMulti *>(*instance_data);

    if( n < 0 )                n = 0;
    if( n >= d->vi.numFrames ) n = d->vi.numFrames - 1;

    try
    {
        if( activation_reason == arInitial )
            d->RequestFrame( n, frame_ctx, core, vsapi );
        else if( activation_reason == arAllFramesReady )
            return d->GetFrame( n, frame_ctx, core, vsapi );
    }
    catch( std::bad_alloc &e )
    {
        vsapi->setFilterError( ("FFT3DFilter:  " + std::string(e.what())).c_str(), frame_ctx );
    }

    return nullptr;
}

static void VS_CC closeFFT3DFilter
(
    void        *instance_data,
    VSCore      *core,
    const VSAPI *vsapi
)
{
    FFT3DFilterMulti *d = static_cast<FFT3DFilterMulti *>(instance_data);
    vsapi->freeNode( d->node );
    delete d;
}

static void VS_CC createFFT3DFilter
(
    const VSMap *in,
    VSMap       *out,
    void        *user_data,
    VSCore      *core,
    const VSAPI *vsapi
)
{
    float sigma1;
    float beta;
    int64_t plane;
    int64_t bw;
    int64_t bh;
    int64_t bt;
    int64_t ow;
    int64_t oh;
    float   kratio;
    float   sharpen;
    float   scutoff;
    float   svr;
    float   smin;
    float   smax;
    int64_t measure;
    int64_t interlaced;
    int64_t wintype;
    int64_t pframe;
    int64_t px;
    int64_t py;
    int64_t pshow;
    float   pcutoff;
    float   pfactor;
    float   sigma2;
    float   sigma3;
    float   sigma4;
    float   degrid;
    float   dehalo;
    float   hr;
    float   ht;
    int64_t ncpu;
    set_option_float( &sigma1,    2.0, "sigma",      in, vsapi );
    set_option_float( &beta,      1.0, "beta",       in, vsapi );
    set_option_int64( &plane,       0, "plane",      in, vsapi );
    const int64_t b = plane < 3 ? 48 : 32;
    set_option_int64( &bw,          b, "bw",         in, vsapi );
    set_option_int64( &bh,          b, "bh",         in, vsapi );
    set_option_int64( &bt,          3, "bt",         in, vsapi );
    set_option_int64( &ow,       bw/3, "ow",         in, vsapi );
    set_option_int64( &oh,       bh/3, "oh",         in, vsapi );
    set_option_float( &kratio,    2.0, "kratio",     in, vsapi );
    set_option_float( &sharpen,     0, "sharpen",    in, vsapi );
    set_option_float( &scutoff,   0.3, "scutoff",    in, vsapi );
    set_option_float( &svr,       1.0, "svr",        in, vsapi );
    set_option_float( &smin,      4.0, "smin",       in, vsapi );
    set_option_float( &smax,     20.0, "smax",       in, vsapi );
    set_option_int64( &measure,     1, "measure",    in, vsapi );
    set_option_int64( &interlaced,  0, "interlaced", in, vsapi );
    set_option_int64( &wintype,     0, "wintype",    in, vsapi );
    set_option_int64( &pframe,      0, "pframe",     in, vsapi );
    set_option_int64( &px,          0, "px",         in, vsapi );
    set_option_int64( &py,          0, "py",         in, vsapi );
    set_option_int64( &pshow,       0, "pshow",      in, vsapi );
    set_option_float( &pcutoff,   0.1, "pcutoff",    in, vsapi );
    set_option_float( &pfactor,     0, "pfactor",    in, vsapi );
    set_option_float( &sigma2, sigma1, "sigma2",     in, vsapi );
    set_option_float( &sigma3, sigma1, "sigma3",     in, vsapi );
    set_option_float( &sigma4, sigma1, "sigma4",     in, vsapi );
    set_option_float( &degrid,    1.0, "degrid",     in, vsapi );
    set_option_float( &dehalo,      0, "dehalo",     in, vsapi );
    set_option_float( &hr,        2.0, "hr",         in, vsapi );
    set_option_float( &ht,       50.0, "ht",         in, vsapi );
    set_option_int64( &ncpu,        1, "ncpu",       in, vsapi );    

    try
    {
        FFT3DFilterMulti *d = new FFT3DFilterMulti( sigma1, beta, plane, bw, bh, bt, ow, oh,
                                                    kratio, sharpen, scutoff, svr, smin, smax,
                                                    measure, interlaced, wintype,
                                                    pframe, px, py, pshow, pcutoff, pfactor,
                                                    sigma2, sigma3, sigma4, degrid,
                                                    dehalo, hr, ht, ncpu,
                                                    in, vsapi );
        if( d == nullptr )
            throw std::bad_alloc();

        vsapi->createFilter
        (
            in, out,
            "FFT3DFilter",
            initFFT3DFilter,
            getFrameFFT3DFilter,
            closeFFT3DFilter,
            fmParallelRequests, 0, d, core
        );
    }
    catch( std::bad_alloc & )
    {
        vsapi->setError( out, "FFT3DFilter:  create failure (FFT3DFilter)!" );
    }
    catch( FFT3DFilterMulti::bad_param &e )
    {
        vsapi->setError( out, ("FFT3DFilter:  " + std::string(e.what())).c_str() );
    }
    catch( FFT3DFilter::bad_param &e )
    {
        vsapi->setError( out, ("FFT3DFilter:  " + std::string(e.what())).c_str() );
    }
    catch( FFT3DFilter::bad_open &e )
    {
        vsapi->setError( out, ("FFT3DFilter:  " + std::string(e.what())).c_str() );
    }
    catch( FFT3DFilter::bad_alloc &e )
    {
        vsapi->setError( out, ("FFT3DFilter:  allocation failure (" + std::string(e.what()) + ")").c_str() );
    }
    catch( FFT3DFilter::bad_plan &e )
    {
        vsapi->setError( out, ("FFT3DFilter:  FFTW3 plan failure (" + std::string(e.what()) + ")").c_str() );
    }
    catch( ... )
    {
    }
}

VS_EXTERNAL_API( void ) VapourSynthPluginInit
(
    VSConfigPlugin     config_func,
    VSRegisterFunction register_func,
    VSPlugin *         plugin
)
{
    config_func
    (
        "systems.innocent.fft3dfilter", "fft3dfilter",
        "FFT3DFilter rev" VSFFT3DFILTER_REV "-" VSFFT3DFILTER_GIT_HASH,
        VAPOURSYNTH_API_VERSION, 1, plugin
    );
    register_func
    (
        "FFT3DFilter",
        "clip:clip;sigma:float:opt;beta:float:opt;plane:int:opt;bw:int:opt;bh:int:opt;bt:int:opt;ow:int:opt;oh:int:opt;"
        "kratio:float:opt;sharpen:float:opt;scutoff:float:opt;svr:float:opt;smin:float:opt;smax:float:opt;"
        "measure:int:opt;interlaced:int:opt;wintype:int:opt;"
        "pframe:int:opt;px:int:opt;py:int:opt;pshow:int:opt;pcutoff:float:opt;pfactor:float:opt;"
        "sigma2:float:opt;sigma3:float:opt;sigma4:float:opt;degrid:float:opt;"
        "dehalo:float:opt;hr:float:opt;ht:float:opt;ncpu:int:opt;",
        createFFT3DFilter, nullptr, plugin
    );
}
