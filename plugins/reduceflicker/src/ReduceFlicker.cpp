/*****************************************************************************
 * ReduceFlicker.cpp
 *****************************************************************************
 * An VapourSynth plugin for reducing flicker
 *
 * Copyright (C) 2005 Rainer Wittmann <gorw@gmx.de>
 * Copyright (C) 2015 Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * To get a copy of the GNU General Public License write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
 * http://www.gnu.org/copyleft/gpl.html .
 *****************************************************************************/

#include "VapourSynth.h"
#include "ReduceFlicker.h"

#define ISSE 0
#include "reduceflicker_sse.h"

extern void reduceflicker1( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height );
extern void reduceflicker1_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height );
extern void reduceflicker2( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height );
extern void reduceflicker2_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height );
extern void reduceflicker3_a( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                              const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height );
extern void reduceflicker3( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                            const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height );

ReduceFlicker::ReduceFlicker
(
    const bool   grey,
    const int    blocksize,
    const VSMap *in,
    const VSAPI *vsapi
)
{
    node   =  vsapi->propGetNode( in, "clip", 0, 0 );
    vi     = *vsapi->getVideoInfo( node );
    planes = grey ? 0 : vi.format->numPlanes;

    for( int i = 0; i < planes; ++i )
    {
        width [i] = vi.width  >> (i ? vi.format->subSamplingW : 0);
        height[i] = vi.height >> (i ? vi.format->subSamplingW : 0);
        int w = width[i] - 1;
        hblocks  [i] = w / blocksize;
        remainder[i] = (w % blocksize) - (blocksize - 1);
        incpitch [i] = blocksize - width[i];
    }
}

ReduceFlicker::~ReduceFlicker()
{
}

void ReduceFlicker1::RequestFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    if( (n - 2) < lastframe )
        for( int i = -2 + (n >= 2 ? 0 : 2 - n); i <= 1; ++i )
            vsapi->requestFrameFilter( n + i, node, frame_ctx );
    else
        vsapi->requestFrameFilter( n, node, frame_ctx );
}

const VSFrameRef * ReduceFlicker1::GetFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    if( (n - 2) >= lastframe )
        return vsapi->getFrameFilter( n, node, frame_ctx );

    const VSFrameRef *pf2 = vsapi->getFrameFilter( n - 2, node, frame_ctx );
    const VSFrameRef *pf1 = vsapi->getFrameFilter( n - 1, node, frame_ctx );
    const VSFrameRef *sf  = vsapi->getFrameFilter( n,     node, frame_ctx );
    const VSFrameRef *nf  = vsapi->getFrameFilter( n + 1, node, frame_ctx );
    VSFrameRef *df = vsapi->newVideoFrame
                        (
                            vsapi->getFrameFormat( sf ),
                            vsapi->getFrameWidth ( sf, 0 ),
                            vsapi->getFrameHeight( sf, 0 ),
                            sf, core
                        );

    if( pf2 == nullptr ) pf2 = sf;
    if( pf1 == nullptr ) pf1 = sf;

    for( int i = 0; i < planes; ++i )
        func( vsapi->getWritePtr( df, i ), vsapi->getStride( df,  i ),
              vsapi->getReadPtr( pf2, i ), vsapi->getStride( pf2, i ),
              vsapi->getReadPtr( pf1, i ), vsapi->getStride( pf1, i ),
              vsapi->getReadPtr( sf,  i ), vsapi->getStride( sf,  i ),
              vsapi->getReadPtr( nf,  i ), vsapi->getStride( nf,  i ),
              hblocks[i], remainder[i], incpitch[i], height[i] );

    if( pf2 != sf ) vsapi->freeFrame( pf2 );
    if( pf1 != sf ) vsapi->freeFrame( pf1 );
    vsapi->freeFrame( sf );
    vsapi->freeFrame( nf );

    SSE_EMMS
    return df;
}

ReduceFlicker1::ReduceFlicker1
(
    const bool   aggressive,
    const bool   grey,
    const VSMap *in,
    const VSAPI *vsapi
) : ReduceFlicker( grey, SSE_INCREMENT, in, vsapi )
{
    func = aggressive ? reduceflicker1_a : reduceflicker1;
    if( (lastframe = vi.numFrames - 3) < 0 )
        throw bad_param{ "clip is too small, there must be at least 3 frames" };
}

ReduceFlicker1::~ReduceFlicker1()
{
}

void ReduceFlicker2::RequestFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    if( (n - 2) < lastframe )
        for( int i = -2 + (n >= 2 ? 0 : 2 - n); i <= 2; ++i )
            vsapi->requestFrameFilter( n + i, node, frame_ctx );
    else
        vsapi->requestFrameFilter( n, node, frame_ctx );
}

const VSFrameRef * ReduceFlicker2::GetFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    if( (n - 2) >= lastframe )
        return vsapi->getFrameFilter( n, node, frame_ctx );

    const VSFrameRef *pf2 = vsapi->getFrameFilter( n - 2, node, frame_ctx );
    const VSFrameRef *pf1 = vsapi->getFrameFilter( n - 1, node, frame_ctx );
    const VSFrameRef *sf  = vsapi->getFrameFilter( n,     node, frame_ctx );
    const VSFrameRef *nf1 = vsapi->getFrameFilter( n + 1, node, frame_ctx );
    const VSFrameRef *nf2 = vsapi->getFrameFilter( n + 2, node, frame_ctx );
    VSFrameRef *df = vsapi->newVideoFrame
                        (
                            vsapi->getFrameFormat( sf ),
                            vsapi->getFrameWidth ( sf, 0 ),
                            vsapi->getFrameHeight( sf, 0 ),
                            sf, core
                        );

    if( pf2 == nullptr ) pf2 = sf;
    if( pf1 == nullptr ) pf1 = sf;

    for( int i = 0; i < planes; ++i )
        func( vsapi->getWritePtr( df, i ), vsapi->getStride( df,  i ),
              vsapi->getReadPtr( pf2, i ), vsapi->getStride( pf2, i ),
              vsapi->getReadPtr( pf1, i ), vsapi->getStride( pf1, i ),
              vsapi->getReadPtr( sf,  i ), vsapi->getStride( sf,  i ),
              vsapi->getReadPtr( nf1, i ), vsapi->getStride( nf1, i ),
              vsapi->getReadPtr( nf2, i ), vsapi->getStride( nf2, i ),
              hblocks[i], remainder[i], incpitch[i], height[i] );

    if( pf2 != sf ) vsapi->freeFrame( pf2 );
    if( pf1 != sf ) vsapi->freeFrame( pf1 );
    vsapi->freeFrame( sf );
    vsapi->freeFrame( nf1 );
    vsapi->freeFrame( nf2 );

    SSE_EMMS
    return df;
}

ReduceFlicker2::ReduceFlicker2
(
    const bool   aggressive,
    const bool   grey,
    const VSMap *in,
    const VSAPI *vsapi
) : ReduceFlicker( grey, SSE_INCREMENT, in, vsapi )
{
    func = aggressive ? reduceflicker2_a : reduceflicker2;
    if( (lastframe = vi.numFrames - 4) < 0 )
        throw bad_param{ "clip is too small, there must be at least 4 frames" };
}

ReduceFlicker2::~ReduceFlicker2()
{
}

void ReduceFlicker3::RequestFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    if( (n - 3) < lastframe )
        for( int i = -3 + (n >= 3 ? 0 : 3 - n); i <= 3; ++i )
            vsapi->requestFrameFilter( n + i, node, frame_ctx );
    else
        vsapi->requestFrameFilter( n, node, frame_ctx );
}

const VSFrameRef * ReduceFlicker3::GetFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    if( (n - 3) >= lastframe )
        return vsapi->getFrameFilter( n, node, frame_ctx );

    const VSFrameRef *pf3 = vsapi->getFrameFilter( n - 3, node, frame_ctx );
    const VSFrameRef *pf2 = vsapi->getFrameFilter( n - 2, node, frame_ctx );
    const VSFrameRef *pf1 = vsapi->getFrameFilter( n - 1, node, frame_ctx );
    const VSFrameRef *sf  = vsapi->getFrameFilter( n,     node, frame_ctx );
    const VSFrameRef *nf1 = vsapi->getFrameFilter( n + 1, node, frame_ctx );
    const VSFrameRef *nf2 = vsapi->getFrameFilter( n + 2, node, frame_ctx );
    const VSFrameRef *nf3 = vsapi->getFrameFilter( n + 3, node, frame_ctx );
    VSFrameRef *df = vsapi->newVideoFrame
                        (
                            vsapi->getFrameFormat( sf ),
                            vsapi->getFrameWidth ( sf, 0 ),
                            vsapi->getFrameHeight( sf, 0 ),
                            sf, core
                        );

    if( pf3 == nullptr ) pf3 = sf;
    if( pf2 == nullptr ) pf2 = sf;
    if( pf1 == nullptr ) pf1 = sf;

    for( int i = 0; i < planes; ++i )
        func( vsapi->getWritePtr( df, i ), vsapi->getStride( df,  i ),
              vsapi->getReadPtr( pf3, i ), vsapi->getStride( pf3, i ),
              vsapi->getReadPtr( pf2, i ), vsapi->getStride( pf2, i ),
              vsapi->getReadPtr( pf1, i ), vsapi->getStride( pf1, i ),
              vsapi->getReadPtr( sf,  i ), vsapi->getStride( sf,  i ),
              vsapi->getReadPtr( nf1, i ), vsapi->getStride( nf1, i ),
              vsapi->getReadPtr( nf2, i ), vsapi->getStride( nf2, i ),
              vsapi->getReadPtr( nf3, i ), vsapi->getStride( nf3, i ),
              hblocks[i], remainder[i], incpitch[i], height[i] );

    if( pf3 != sf ) vsapi->freeFrame( pf3 );
    if( pf2 != sf ) vsapi->freeFrame( pf2 );
    if( pf1 != sf ) vsapi->freeFrame( pf1 );
    vsapi->freeFrame( sf );
    vsapi->freeFrame( nf1 );
    vsapi->freeFrame( nf2 );
    vsapi->freeFrame( nf3 );

    SSE_EMMS
    return df;
}

ReduceFlicker3::ReduceFlicker3
(
    const bool   aggressive,
    const bool   grey,
    const VSMap *in,
    const VSAPI *vsapi
) : ReduceFlicker( grey, SSE_INCREMENT, in, vsapi )
{
    func = aggressive ? reduceflicker3_a : reduceflicker3;
    if( (lastframe = vi.numFrames - 6) < 0 )
        throw bad_param{ "clip is too small, there must be at least 6 frames" };
}

ReduceFlicker3::~ReduceFlicker3()
{
}
