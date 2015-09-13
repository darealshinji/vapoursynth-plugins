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

#include "config.h"
#include "VapourSynth.h"
#include "ReduceFlicker.h"

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

static void VS_CC initReduceFlicker
(
    VSMap       *in,
    VSMap       *out,
    void       **instance_data,
    VSNode      *node,
    VSCore      *core,
    const VSAPI *vsapi
)
{
    ReduceFlicker *d = static_cast<ReduceFlicker *>(*instance_data);
    vsapi->setVideoInfo( &d->vi, 1, node );
}

static const VSFrameRef * VS_CC getFrameReduceFlicker
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
    ReduceFlicker *d = static_cast<ReduceFlicker *>(*instance_data);

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
        vsapi->setFilterError( ("ReduceFlicker:  " + std::string(e.what())).c_str(), frame_ctx );
    }

    return nullptr;
}

static void VS_CC closeReduceFlicker
(
    void        *instance_data,
    VSCore      *core,
    const VSAPI *vsapi
)
{
    ReduceFlicker *d = static_cast<ReduceFlicker *>(instance_data);
    vsapi->freeNode( d->node );
    delete d;
}

static void VS_CC createReduceFlicker
(
    const VSMap *in,
    VSMap       *out,
    void        *user_data,
    VSCore      *core,
    const VSAPI *vsapi
)
{
    int64_t strength;
    int64_t aggressive;
    int64_t grey;
    set_option_int64( &strength,   2, "strength",   in, vsapi );
    set_option_int64( &aggressive, 0, "aggressive", in, vsapi );
    set_option_int64( &grey,       0, "grey",       in, vsapi );

    try
    {
        ReduceFlicker *d;
        switch( strength )
        {
            case 1 :
                d = new ReduceFlicker1( aggressive, grey, in, vsapi );
                break;
            case 2 :
                d = new ReduceFlicker2( aggressive, grey, in, vsapi );
                break;
            case 3 :
                d = new ReduceFlicker3( aggressive, grey, in, vsapi );
                break;
            default :
                vsapi->setError( out, "ReduceFlicker:  illegal value for strength!\n" );
                return;
        }
        if( d == nullptr )
            throw std::bad_alloc();

        vsapi->createFilter
        (
            in, out,
            "ReduceFlicker",
            initReduceFlicker,
            getFrameReduceFlicker,
            closeReduceFlicker,
            fmParallel, 0, d, core
        );
    }
    catch( std::bad_alloc & )
    {
        vsapi->setError( out, "ReduceFlicker:  create failure (ReduceFlicker)!" );
    }
    catch( ReduceFlicker::bad_param &e )
    {
        vsapi->setError( out, ("ReduceFlicker:  " + std::string(e.what())).c_str() );
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
        "systems.innocent.reduceflicker", "reduceflicker",
        "ReduceFlicker rev" VSREDUCEFLICKER_REV "-" VSREDUCEFLICKER_GIT_HASH,
        VAPOURSYNTH_API_VERSION, 1, plugin
    );
    register_func
    (
        "ReduceFlicker",
        "clip:clip;strength:int:opt;aggressive:int:opt;grey:int:opt;",
        createReduceFlicker, nullptr, plugin
    );
}
