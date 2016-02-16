/*****************************************************************************
 * FFT3DFilter.cpp
 *****************************************************************************
 * FFT3DFilter plugin for VapourSynth - 3D Frequency Domain filter
 *
 * Copyright (C) 2004-2006 A.G.Balakhnin aka Fizick <bag@hotmail.ru> http://avisynth.org.ru
 * Copyright (C) 2015      Yusuke Nakamura, <muken.the.vfrmaniac@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************
 *
 * Plugin uses external FFTW library version 3 (http://www.fftw.org)
 * You must put libfftw3f-3.dll file from this package to some directory in path
 * (for example, C:\WINNT\System32).
 *
 * The algorithm is based on the 3D IIR/3D Frequency Domain Filter from:
 * MOTION PICTURE RESTORATION. by Anil Christopher Kokaram. Ph.D. Thesis. May 1993.
 * http://www.mee.tcd.ie/~ack/papers/a4ackphd.ps.gz
 *
 * For AviSynth 2.5
 *    Version 0.1, 23 November 2004 - initial
 *    Version 0.2, 3 December 2004 - add beta parameter of noise margin
 *    Version 0.3, 21 December 2004 - add bt parameter of temporal size
 *    Version 0.4, 16 January 2005 - algorithm optimized for speed for bt=2 (now default),
 *        mode bt=3 is temporary disabled, changed default bw=bh=32, filtered region now centered.
 *    Version 0.5, 28 January 2005 - added YUY2 support
 *    Version 0.6, 29 January 2005 - added Kalman filter mode for bt=0, ratio parameter
 *    Version 0.7, 30 January 2005 - re-enabled Wiener filter mode with 3 frames (bt=3)
 *    Version 0.8, 05 February2005 - added option to sharpen, and bt=-1
 *    Version 0.8.1, 6 February2005 - skip sharpening of the lowest frequencies to prevent parasitic lines near border
 *    Version 0.8.2,  February 15, 2005 - added internal buffer to process whole frame (borders included) for any bw, bh (a little slower)
 *    Version 0.8.3, March 16, 2005 - fixed sharpen mode (bt=-1) for YUY2
 *    Version 0.8.4, April 3, 2005 - delayed FFTW3.DLL loading
 *    Version 0.9 - April 3,2005 - variable overlapping size
 *    Version 0.9.1 - April 7,2005 - some assembler 3DNow! optimization for mode bt=3
 *    Version 0.9.2 - April 10,2005 - some assembler 3DNow! optimization for mode bt=2,
 *        option measure=true is now default as more fast
 *    Version 0.9.3 - April 24,2005 - bug fixed for bt=2 with 3DNow; * bt=3 now default;
 *        modifyed sharpen to horizontal only (still experimental)
 *    Version 1.0 - June 22, 2005 - improved edges processing (by padding);
 *        added svr parameter to control vertical sharpening
 *    Version 1.0.1 - July 05, 2005 - fixed bug for YUY2 chroma planes
 *    Version 1.1 - July 8,2005 - improved sharpen mode to prevent grid artifactes and to limit sharpening,
 *        added parameters smin, smax; renamed parameter ratio to kratio.
 *    Version 1.2 - July 12, 2005 - changed parameters defaults (bw=bh=48, ow=bw/3, oh=bh/3) to prevent grid artifactes
 *    Version 1.3 - July 20, 2005 - added interlaced mode
 *    Version 1.3.1 - July 21, 2005 - fixed bug for YUY2 interlaced
 *    Version 1.4 - July 23, 2005 - corrected neutral level for chroma processing, added wintype to decrease grid artefactes
 *    Version 1.5 - July 26, 2005 - added noise pattern method and its parameters pframe, px, py, pshow, pcutoff, pfactor
 *    Version 1.5.1 - July 29, 2005 - fixed bug with pshow
 *    Version 1.5.2 - July 31, 2005 - fixed bug with Kalman mode (bt=0) for Athlon (introduced in v1.5)
 *    Version 1.6 - August 01, 2005 - added mode bt=4; optimized SSE version for bt=2,3
 *    Version 1.7 - August 29, 2005 - added SSE version for for sharpen and pattern modes bt=2,3 ; restuctured code, GPL v2
 *    Version 1.8 - September 6, 2005 - improved internal fft cache; added degrid=0; changed wintype=0
 *    Version 1.8.1 - October 26, 2005 - fixed bug with sharpen>0 AND degrid>0 for bt not equal 1.
 *    Version 1.8.2 - November 04, 2005 - really set default degrid=1.0 (was = 0)
 *    Version 1.8.3 - November 28, 2005 - fixed bug with first frame for Kalman YV12 (thanks to Tsp)
 *    Version 1.8.4 - November 29, 2005 - added multiplane modes plane=3,4
 *    Version 1.8.5 - 4 December 2005 - fixed bug with memory leakage (thanks to tsp).
 *    Version 1.9 - April 25, 2006 - added dehalo options; corrected sharpen mode a little;
 *        re-enabled 3DNow and SSE optimization for degrid=0;  added SSE optimization for bt=3,-1 with degrid>0 (faster by 15%)
 *    Version 1.9.1 - May 10, 2006 - added SSE optimization for bt=4 with degrid>0 (faster by 30%)
 *    Version 1.9.2 - September 6, 2006 - added new mode bt=5
 *    Version 2.0.0 - november 6, 2006 - added motion compensation mc parameter, window reorganized, multi-cpu
 *    Version 2.1.0 - January 17, 2007 - removed motion compensation mc parameter
 *    Version 2.1.1 - February 19, 2007 - fixed bug with bw not mod 4 (restored v1.9.2 window method)
 *
 * For VapourSynth
 *    February 2, 2015 - imported for VapourSynth without SIMD optimizations.
 *    February 4, 2015 - stopped using explicit linking to FFTW3 library.
 *****************************************************************************/

#include <cstring>
#include <algorithm>

#include "FFT3DFilter.h"
#include "info.h"

/** declarations of filtering functions: **/
/* C */
void ApplyWiener2D_C( fftwf_complex *out, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float dehalo, const float *wdehalo, float ht2n );
void ApplyPattern2D_C( fftwf_complex *outcur, int outwidth, int outpitch, int bh, int howmanyblocks, float pfactor, const float *pattern2d0, float beta );
void ApplyWiener3D2_C( const fftwf_complex *outcur, fftwf_complex *outprev, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta );
void ApplyPattern3D2_C( const fftwf_complex *outcur, fftwf_complex *outprev, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta );
void ApplyWiener3D3_C( const fftwf_complex *out, fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta );
void ApplyPattern3D3_C( const fftwf_complex *out, fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta );
void ApplyWiener3D4_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta );
void ApplyPattern3D4_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta );
void ApplyWiener3D5_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta );
void ApplyPattern3D5_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta );
void ApplyKalmanPattern_C( const fftwf_complex *outcur, fftwf_complex *outLast, fftwf_complex *covar, fftwf_complex *covarProcess, int outwidth, int outpitch, int bh, int howmanyblocks, const float *covarNoiseNormed, float kratio2 );
void ApplyKalman_C( const fftwf_complex *outcur, fftwf_complex *outLast, fftwf_complex *covar, fftwf_complex *covarProcess, int outwidth, int outpitch, int bh, int howmanyblocks,  float covarNoiseNormed, float kratio2 );
void Sharpen_C( fftwf_complex *outcur, int outwidth, int outpitch, int bh, int howmanyblocks, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float dehalo, const float *wdehalo, float ht2n );
/* degrid_C */
void ApplyWiener2D_degrid_C( fftwf_complex *out, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float degrid, const fftwf_complex *gridsample, float dehalo, const float *wdehalo, float ht2n );
void ApplyWiener3D2_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyWiener3D3_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyWiener3D4_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyWiener3D5_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, const fftwf_complex *gridsample );
void Sharpen_degrid_C( fftwf_complex *outcur, int outwidth, int outpitch, int bh, int howmanyblocks, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float degrid, const fftwf_complex *gridsample, float dehalo, const float *wdehalo, float ht2n );
void ApplyPattern2D_degrid_C( fftwf_complex *outcur, int outwidth, int outpitch, int bh, int howmanyblocks, float pfactor, const float *pattern2d0, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyPattern3D2_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyPattern3D3_degrid_C( const fftwf_complex *out, fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyPattern3D4_degrid_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyPattern3D5_degrid_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta, float degrid, const fftwf_complex *gridsample );
//-------------------------------------------------------------------------------------------
static void ApplyWiener2D( fftwf_complex *out, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed,
                           float beta, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float dehalo, const float *wdehalo, float ht2n )
{
    ApplyWiener2D_C( out, outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, sharpen, sigmaSquaredSharpenMin, sigmaSquaredSharpenMax, wsharpen, dehalo, wdehalo, ht2n );
}
//-------------------------------------------------------------------------------------------
static void ApplyPattern2D( fftwf_complex *outcur, int outwidth, int outpitch, int bh, int howmanyblocks, float pfactor, const float *pattern2d0, float beta )
{
    ApplyPattern2D_C( outcur, outwidth, outpitch, bh, howmanyblocks, pfactor, pattern2d0, beta );
}
//-------------------------------------------------------------------------------------------
template < int btcur >
static void ApplyWiener3D_degrid( const fftwf_complex *out, fftwf_complex *outprev2, fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, fftwf_complex *gridsample )
{
    if( btcur == 5 ) ApplyWiener3D5_degrid_C( out, outprev2, outprev, outnext, outnext2, outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
    if( btcur == 4 ) ApplyWiener3D4_degrid_C( out, outprev2, outprev, outnext,           outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
    if( btcur == 3 ) ApplyWiener3D3_degrid_C( out,           outprev, outnext,           outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
    if( btcur == 2 ) ApplyWiener3D2_degrid_C( out,           outprev,                    outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
}
//-------------------------------------------------------------------------------------------
template < int btcur >
static void ApplyPattern3D_degrid( const fftwf_complex *out, fftwf_complex *outprev2, fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitch, int bh, int howmanyblocks, float *pattern3d, float beta, float degrid, fftwf_complex *gridsample )
{
    if( btcur == 5 ) ApplyPattern3D5_degrid_C( out, outprev2, outprev, outnext, outnext2, outwidth, outpitch, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
    if( btcur == 4 ) ApplyPattern3D4_degrid_C( out, outprev2, outprev, outnext,           outwidth, outpitch, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
    if( btcur == 3 ) ApplyPattern3D3_degrid_C( out,           outprev, outnext,           outwidth, outpitch, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
    if( btcur == 2 ) ApplyPattern3D2_degrid_C( out,           outprev,                    outwidth, outpitch, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
}
//-------------------------------------------------------------------------------------------
template < int btcur >
static void ApplyWiener3D( const fftwf_complex *out, fftwf_complex *outprev2, fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitch, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta )
{
    if( btcur == 5 ) ApplyWiener3D5_C( out, outprev2, outprev, outnext, outnext2, outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
    if( btcur == 4 ) ApplyWiener3D4_C( out, outprev2, outprev, outnext,           outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
    if( btcur == 3 ) ApplyWiener3D3_C( out,           outprev, outnext,           outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
    if( btcur == 2 ) ApplyWiener3D2_C( out,           outprev,                    outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
}
//-------------------------------------------------------------------------------------------
template < int btcur >
static void ApplyPattern3D( const fftwf_complex *out, fftwf_complex *outprev2, fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitch, int bh, int howmanyblocks, const float *pattern3d, float beta )
{
    if( btcur == 5 ) ApplyPattern3D5_C( out, outprev2, outprev, outnext, outnext2, outwidth, outpitch, bh, howmanyblocks, pattern3d, beta );
    if( btcur == 4 ) ApplyPattern3D4_C( out, outprev2, outprev, outnext,           outwidth, outpitch, bh, howmanyblocks, pattern3d, beta );
    if( btcur == 3 ) ApplyPattern3D3_C( out,           outprev, outnext,           outwidth, outpitch, bh, howmanyblocks, pattern3d, beta );
    if( btcur == 2 ) ApplyPattern3D2_C( out,           outprev,                    outwidth, outpitch, bh, howmanyblocks, pattern3d, beta );
}
//-------------------------------------------------------------------------------------------
static void ApplyKalmanPattern( const fftwf_complex *outcur, fftwf_complex *outLast, fftwf_complex *covar, fftwf_complex *covarProcess, int outwidth, int outpitch, int bh, int howmanyblocks, const float *covarNoiseNormed, float kratio2 )
{
    ApplyKalmanPattern_C( outcur, outLast, covar, covarProcess, outwidth, outpitch, bh, howmanyblocks,  covarNoiseNormed, kratio2 );
}
//-------------------------------------------------------------------------------------------
static void ApplyKalman( const fftwf_complex *outcur, fftwf_complex *outLast, fftwf_complex *covar, fftwf_complex *covarProcess, int outwidth, int outpitch, int bh, int howmanyblocks,  float covarNoiseNormed, float kratio2 )
{
    ApplyKalman_C( outcur, outLast, covar, covarProcess, outwidth, outpitch, bh, howmanyblocks,  covarNoiseNormed, kratio2 );
}
//-------------------------------------------------------------------------------------------
static void Sharpen( fftwf_complex *outcur, int outwidth, int outpitch, int bh, int howmanyblocks, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float dehalo, const float *wdehalo, float ht2n )
{
    Sharpen_C( outcur, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMin, sigmaSquaredSharpenMax, wsharpen, dehalo, wdehalo, ht2n );
}
//-------------------------------------------------------------------------------------------
static void Sharpen_degrid( fftwf_complex *outcur, int outwidth, int outpitch, int bh, int howmanyblocks, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float degrid, const fftwf_complex *gridsample, float dehalo, const float *wdehalo, float ht2n )
{
    Sharpen_degrid_C( outcur, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMin, sigmaSquaredSharpenMax, wsharpen, degrid, gridsample, dehalo, wdehalo, ht2n );
}
//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------
static void fill_complex( fftwf_complex *plane, int outsize, float realvalue, float imgvalue)
{
    /* it is not fast, but called only in constructor */
    for( int w = 0; w < outsize; w++ )
    {
        plane[w][0] = realvalue;
        plane[w][1] = imgvalue;
    }
}
//-------------------------------------------------------------------
static void SigmasToPattern( float sigma, float sigma2, float sigma3, float sigma4, int bh, int outwidth, int outpitch, float norm, float *pattern2d )
{
    /* it is not fast, but called only in constructor */
    float sigmacur;
    constexpr float ft2 = sqrt( 0.5f ) / 2; /* frequency for sigma2 */
    constexpr float ft3 = sqrt( 0.5f ) / 4; /* frequency for sigma3 */
    for( int h = 0; h < bh; h++ )
    {
        for( int w = 0; w < outwidth; w++ )
        {
            float fy = (bh - 2.0f * abs( h - bh / 2)) / bh; /* normalized to 1 */
            float fx = (w * 1.0f) / outwidth;               /* normalized to 1 */
            float f = sqrt( (fx * fx + fy * fy) * 0.5f );   /* normalized to 1 */
            if( f < ft3 )
            {   /* low frequencies */
                sigmacur = sigma4 + (sigma3 - sigma4) * f / ft3;
            }
            else if( f < ft2 )
            {   /* middle frequencies */
                sigmacur = sigma3 + (sigma2 - sigma3) * (f - ft3) / (ft2 - ft3);
            }
            else
            {   /* high frequencies */
                sigmacur = sigma + (sigma2 - sigma) * (1 - f) / (1 - ft2);
            }
            pattern2d[w] = sigmacur * sigmacur / norm;
        }
        pattern2d += outpitch;
    }
}


//-------------------------------------------------------------------------------------------
static void BitBlt
(
    void       *dstp,
    int         dst_stride,
    const void *srcp,
    int         src_stride,
    int         row_size,
    int         height
)
{
    if( height <= 0 )
        return;
    if( src_stride == dst_stride
     && src_stride == row_size )
        std::memcpy( dstp, srcp, row_size * height );
    else
    {
        unsigned char *srcp8 = static_cast<unsigned char *>(const_cast<void *>(srcp));
        unsigned char *dstp8 = static_cast<unsigned char *>                   (dstp);
        for( int i = 0; i < height; ++i )
        {
            std::memcpy( dstp8, srcp8, row_size );
            srcp8 += src_stride;
            dstp8 += dst_stride;
        }
    }
}

//-------------------------------------------------------------------
FFT3DFilter::FFT3DFilter
(
    float _sigma, float _beta, int _plane, int _bw, int _bh, int _bt, int _ow, int _oh,
    float _kratio, float _sharpen, float _scutoff, float _svr, float _smin, float _smax,
    bool _measure, bool _interlaced, int _wintype,
    int _pframe, int _px, int _py, bool _pshow, float _pcutoff, float _pfactor,
    float _sigma2, float _sigma3, float _sigma4, float _degrid,
    float _dehalo, float _hr, float _ht, int _ncpu, int _multiplane,
    VSVideoInfo _vi, VSNodeRef *_node
) : sigma( _sigma ), beta( _beta ), plane( _plane ), bw( _bw ), bh( _bh ), bt( _bt ), ow( _ow ), oh( _oh ),
    kratio( _kratio ), sharpen( _sharpen ), scutoff( _scutoff ), svr( _svr ), smin( _smin ), smax( _smax ),
    measure( _measure ), interlaced( _interlaced ), wintype( _wintype ),
    pframe( _pframe ), px( _px ), py( _py ), pshow( _pshow ), pcutoff( _pcutoff ), pfactor( _pfactor ),
    sigma2( _sigma2 ), sigma3( _sigma3 ), sigma4( _sigma4 ), degrid( _degrid ),
    dehalo( _dehalo ), hr( _hr ), ht( _ht ), ncpu( _ncpu ), multiplane( _multiplane ),
    vi( _vi ), node( _node )
{
    int i, j;
    //_asm emms;

    if( ow * 2 > bw ) throw bad_param{ "Must not be 2*ow > bw" };
    if( oh * 2 > bh ) throw bad_param{ "Must not be 2*oh > bh" };
    if( ow < 0 ) ow = bw / 3; /* changed from bw/4 to bw/3 in v.1.2 */
    if( oh < 0 ) oh = bh / 3; /* changed from bh/4 to bh/3 in v.1.2 */

    if( bt < -1 || bt > 5 ) throw bad_param{ "bt must be -1(Sharpen), 0(Kalman), 1,2,3,4,5(Wiener)" };

    if( vi.format->colorFamily != cmGray && vi.format->colorFamily != cmYUV )
        throw bad_param{ "only planar YUV formats are supported" };
    if( vi.format->bitsPerSample != 8 )
        throw bad_param{ "only 8-bit formats are supported" };

    if( plane < 3 )
    {
        nox = ((vi.width  >> (plane ? vi.format->subSamplingW : 0)) - ow + (bw - ow - 1)) / (bw - ow);
        noy = ((vi.height >> (plane ? vi.format->subSamplingW : 0)) - oh + (bh - oh - 1)) / (bh - oh);
    }
    else
        throw bad_param{ "internal plane must be 0, 1 or 2" };

    /* padding by 1 block per side */
    nox += 2;
    noy += 2;
    mirw = bw - ow; /* set mirror size as block interval */
    mirh = bh - oh;

    if( beta < 1 )
        throw bad_param{ "beta must be not less 1.0" };

    int istat = fftwf_init_threads();
    if( istat == 0 )
        throw bad_open{ "fftwf_init_threads() failed!" };

    coverwidth  = nox * ( bw - ow ) + ow;
    coverheight = noy * ( bh - oh ) + oh;
    coverpitch  = ((coverwidth + 7) / 8 ) * 8;
    coverbuf    = (uint8_t *)malloc( coverheight * coverpitch );

    int insize = bw * bh * nox * noy;
    in = (float *)fftwf_malloc( sizeof(float) * insize );
    outwidth = bw / 2 + 1;                  /* width (pitch) of complex fft block */
    outpitch = ((outwidth + 1) / 2) * 2;    /* must be even for SSE - v1.7 */
    outsize  = outpitch * bh * nox * noy;   /* replace outwidth to outpitch here and below in v1.7 */

    if( bt == 0 ) /* Kalman */
    {
        outLast      = (fftwf_complex *)fftwf_malloc( sizeof(fftwf_complex) * outsize );
        covar        = (fftwf_complex *)fftwf_malloc( sizeof(fftwf_complex) * outsize );
        covarProcess = (fftwf_complex *)fftwf_malloc( sizeof(fftwf_complex) * outsize );
    }
    outrez     = (fftwf_complex *)fftwf_malloc( sizeof(fftwf_complex) * outsize ); /* v1.8 */
    gridsample = (fftwf_complex *)fftwf_malloc( sizeof(fftwf_complex) * outsize ); /* v1.8 */

    /* fft cache - added in v1.8 */
    cachesize = bt + 2;
    cachewhat = (int *)malloc( sizeof(int) * cachesize );
    cachefft  = (fftwf_complex **)fftwf_malloc( sizeof(fftwf_complex *) * cachesize );
    for( i = 0; i < cachesize; i++ )
    {
        cachefft [i] = (fftwf_complex *)fftwf_malloc( sizeof(fftwf_complex) * outsize );
        cachewhat[i] = -1; /* init as notexistant */
    }

    int planFlags;
    /* use FFTW_ESTIMATE or FFTW_MEASURE (more optimal plan, but with time calculation at load stage) */
    if( measure )
        planFlags = FFTW_MEASURE;
    else
        planFlags = FFTW_ESTIMATE;

    int rank = 2; /* 2d */
    ndim[0] = bh; /* size of block along height */
    ndim[1] = bw; /* size of block along width */
    int istride = 1;
    int ostride = 1;
    int idist   = bw * bh;
    int odist   = outpitch * bh;/*  v1.7 (was outwidth) */
    inembed[0] = bh;
    inembed[1] = bw;
    onembed[0] = bh;
    onembed[1] = outpitch;      /*  v1.7 (was outwidth) */
    howmanyblocks = nox * noy;

    fftwf_plan_with_nthreads( ncpu );

    plan = fftwf_plan_many_dft_r2c( rank, ndim, howmanyblocks,
                                    in, inembed, istride, idist, outrez, onembed, ostride, odist, planFlags );
    if( plan == nullptr )
        throw bad_plan{ "fftwf_plan_many_dft_r2c" };

    planinv = fftwf_plan_many_dft_c2r( rank, ndim, howmanyblocks,
                                       outrez, onembed, ostride, odist, in, inembed, istride, idist, planFlags );
    if( planinv == nullptr )
        throw bad_plan{ "fftwf_plan_many_dft_c2r" };

    fftwf_plan_with_nthreads( 1 );

    wanxl = (float *)malloc( ow * sizeof(float) );
    wanxr = (float *)malloc( ow * sizeof(float) );
    wanyl = (float *)malloc( oh * sizeof(float) );
    wanyr = (float *)malloc( oh * sizeof(float) );

    wsynxl = (float *)malloc( ow * sizeof(float) );
    wsynxr = (float *)malloc( ow * sizeof(float) );
    wsynyl = (float *)malloc( oh * sizeof(float) );
    wsynyr = (float *)malloc( oh * sizeof(float) );

    wsharpen = (float *)fftwf_malloc( bh * outpitch * sizeof(float) );
    wdehalo  = (float *)fftwf_malloc( bh * outpitch * sizeof(float) );

    /* define analysis and synthesis windows
     * combining window (analize mult by synthesis) is raised cosine (Hanning) */

    constexpr float pi = 3.1415926535897932384626433832795f;
    if( wintype == 0 ) /* window type */
    {   /* , used in all version up to 1.3
         * half-cosine, the same for analysis and synthesis
         * define analysis windows */
        for( i = 0; i < ow; i++ )
        {
            wanxl[i] = cosf( pi * (i - ow + 0.5f) / (ow * 2) ); /* left analize window (half-cosine) */
            wanxr[i] = cosf( pi * (i      + 0.5f) / (ow * 2) ); /* right analize window (half-cosine) */
        }
        for( i = 0; i < oh; i++ )
        {
            wanyl[i] = cosf( pi * (i - oh + 0.5f) / (oh * 2) );
            wanyr[i] = cosf( pi * (i      + 0.5f) / (oh * 2) );
        }
        /* use the same windows for synthesis too. */
        for( i = 0; i < ow; i++ )
        {
            wsynxl[i] = wanxl[i]; /* left  window (half-cosine) */

            wsynxr[i] = wanxr[i]; /* right  window (half-cosine) */
        }
        for( i = 0; i < oh; i++ )
        {
            wsynyl[i] = wanyl[i];
            wsynyr[i] = wanyr[i];
        }
    }
    else if( wintype == 1 ) /* added in v.1.4 */
    {
        /* define analysis windows as more flat (to decrease grid) */
        for( i = 0; i < ow; i++ )
        {
            wanxl[i] = sqrt( cosf( pi * (i - ow + 0.5f) / (ow * 2) ) );
            wanxr[i] = sqrt( cosf( pi * (i      + 0.5f) / (oh * 2) ) );
        }
        for( i = 0; i < oh; i++ )
        {
            wanyl[i] = sqrt( cosf( pi * (i - oh + 0.5f) / (oh * 2) ) );
            wanyr[i] = sqrt( cosf( pi * (i      + 0.5f) / (oh * 2) ) );
        }
        /* define synthesis as supplenent to rised cosine (Hanning) */
        for( i = 0; i < ow; i++ )
        {
            wsynxl[i] = wanxl[i] * wanxl[i] * wanxl[i]; /* left window */
            wsynxr[i] = wanxr[i] * wanxr[i] * wanxr[i]; /* right window */
        }
        for( i = 0; i < oh; i++ )
        {
            wsynyl[i] = wanyl[i] * wanyl[i] * wanyl[i];
            wsynyr[i] = wanyr[i] * wanyr[i] * wanyr[i];
        }
    }
    else /* (wintype==2) - added in v.1.4 */
    {
        /* define analysis windows as flat (to prevent grid) */
        for( i = 0; i < ow; i++ )
        {
            wanxl[i] = 1;
            wanxr[i] = 1;
        }
        for( i = 0; i < oh; i++ )
        {
            wanyl[i] = 1;
            wanyr[i] = 1;
        }
        /* define synthesis as rised cosine (Hanning) */
        for( i = 0; i < ow; i++ )
        {
            wsynxl[i] = cosf( pi * (i - ow + 0.5f) / (ow * 2) );
            wsynxl[i] = wsynxl[i] * wsynxl[i];    /* left window (rised cosine) */
            wsynxr[i] = cosf( pi * (i      + 0.5f) / (ow * 2));
            wsynxr[i] = wsynxr[i] * wsynxr[i];    /* right window (falled cosine) */
        }
        for( i = 0; i < oh; i++ )
        {
            wsynyl[i] = cosf( pi * (i - oh + 0.5f) / (oh * 2) );
            wsynyl[i] = wsynyl[i] * wsynyl[i];
            wsynyr[i] = cosf( pi * (i      + 0.5f) / (oh * 2) );
            wsynyr[i] = wsynyr[i] * wsynyr[i];
        }
    }

    /* window for sharpen */
    for( j = 0; j < bh; j++ )
    {
        int dj = j;
        if( j >= bh / 2)
            dj = bh - j;
        float d2v = float(dj * dj) * (svr * svr) / ((bh / 2) * (bh / 2)); /* v1.7 */
        for( i = 0; i < outwidth; i++ )
        {
            float d2 = d2v + float(i * i) / ((bw / 2) * (bw / 2)); /* distance_2 - v1.7 */
            wsharpen[i] = 1 - exp( -d2 / (2 * scutoff * scutoff) );
        }
        wsharpen += outpitch;
    }
    wsharpen -= outpitch * bh; /* restore pointer */

    /* window for dehalo - added in v1.9 */
    float wmax = 0;
    for( j = 0; j < bh; j++ )
    {
        int dj = j;
        if( j >= bh / 2 )
            dj = bh - j;
        float d2v = float(dj * dj) * (svr * svr) / ((bh / 2) * (bh / 2));
        for( i = 0; i < outwidth; i++ )
        {
            float d2 = d2v + float(i * i) / ((bw / 2) * (bw / 2)); /* squared distance in frequency domain */
            //float d1 = sqrt( d2 );
            wdehalo[i] = exp( -0.7f * d2 * hr * hr ) - exp( -d2 * hr * hr ); /* some window with max around 1/hr, small at low and high frequencies */
            if( wdehalo[i] > wmax )
                wmax = wdehalo[i]; /* for normalization */
        }
        wdehalo += outpitch;
    }
    wdehalo -= outpitch * bh; /* restore pointer */

    for( j = 0; j < bh; j++ )
    {
        for( i = 0; i < outwidth; i++ )
        {
            wdehalo[i] /= wmax; /* normalize */
        }
        wdehalo += outpitch;
    }
    wdehalo -= outpitch * bh; /* restore pointer */

    /* init nlast */
    nlast     = -999; /* init as nonexistant */
    btcurlast = -999; /* init as nonexistant */

    norm = 1.0f / (bw * bh); /* do not forget set FFT normalization factor */

    sigmaSquaredNoiseNormed2D = sigma * sigma / norm;
    sigmaNoiseNormed2D = sigma / sqrtf( norm );
    sigmaMotionNormed  = sigma * kratio / sqrtf( norm );
    sigmaSquaredSharpenMinNormed = smin * smin / norm;
    sigmaSquaredSharpenMaxNormed = smax * smax / norm;
    ht2n = ht * ht / norm; /* halo threshold squared and normed - v1.9 */

    /* init Kalman */
    if( bt == 0 ) /* Kalman */
    {
        fill_complex( outLast,      outsize, 0, 0 );
        fill_complex( covar,        outsize, sigmaSquaredNoiseNormed2D, sigmaSquaredNoiseNormed2D );
        fill_complex( covarProcess, outsize, sigmaSquaredNoiseNormed2D, sigmaSquaredNoiseNormed2D );
    }

    mean = (float *)malloc( nox * noy * sizeof(float) );

    pwin = (float *)malloc( bh * outpitch * sizeof(float) ); /* pattern window array */

    for( j = 0; j < bh; j++ )
    {
        float fh2;
        if( j < bh / 2 )
            fh2 = (j * 2.0f / bh) * (j * 2.0f / bh);
        else
            fh2 = ((bh - 1 - j) * 2.0f / bh) * ((bh - 1 - j) * 2.0f / bh);
        for( i = 0; i < outwidth; i++ )
        {
            float fw2 = (i * 2.0f / bw) * (j * 2.0f / bw);
            pwin[i] = (fh2 + fw2) / (fh2 + fw2 + pcutoff * pcutoff);
        }
        pwin += outpitch;
    }
    pwin -= outpitch * bh; /* restore pointer */

    pattern2d = (float *)fftwf_malloc( bh * outpitch * sizeof(float) ); /* noise pattern window array */
    pattern3d = (float *)fftwf_malloc( bh * outpitch * sizeof(float) ); /* noise pattern window array */

    if( (sigma2 != sigma || sigma3 != sigma || sigma4 != sigma) && pfactor == 0 )
    {   /* we have different sigmas, so create pattern from sigmas */
        SigmasToPattern( sigma, sigma2, sigma3, sigma4, bh, outwidth, outpitch, norm, pattern2d );
        isPatternSet = true;
        pfactor = 1;
    }
    else
    {
        isPatternSet = false; /* pattern must be estimated */
    }

    /* prepare window compensation array gridsample
     * allocate large array for simplicity :)
     * but use one block only for speed
     * Attention: other block could be the same, but we do not calculate them! */
    plan1 = fftwf_plan_many_dft_r2c( rank, ndim, 1,
                                     in, inembed, istride, idist, outrez, onembed, ostride, odist, planFlags ); /* 1 block */

    memset( coverbuf, 255, coverheight * coverpitch );
    FFT3DFilter::InitOverlapPlane( in, coverbuf, coverpitch, 0 );
    /* make FFT 2D */
    fftwf_execute_dft_r2c( plan1, in, gridsample );

    messagebuf = (char *)malloc( 80 ); /* 1.8.5 */
}
//-------------------------------------------------------------------------------------------

FFT3DFilter::~FFT3DFilter()
{
    fftwf_destroy_plan( plan );
    fftwf_destroy_plan( plan1 );
    fftwf_destroy_plan( planinv );
    fftwf_free( in );
    free( wanxl );
    free( wanxr );
    free( wanyl );
    free( wanyr );
    free( wsynxl );
    free( wsynxr );
    free( wsynyl );
    free( wsynyr );
    fftwf_free( wsharpen );
    fftwf_free( wdehalo );
    free( mean );
    free( pwin );
    fftwf_free( pattern2d );
    fftwf_free( pattern3d );
    fftwf_free( outrez );
    if( bt == 0 ) /* Kalman */
    {
        fftwf_free( outLast );
        fftwf_free( covar );
        fftwf_free( covarProcess );
    }
    free( coverbuf );
    free( cachewhat );
    for( int i = 0; i < cachesize; i++ )
    {
        fftwf_free( cachefft[i] );
    }
    fftwf_free( cachefft );
    fftwf_free( gridsample ); /* fixed memory leakage in v1.8.5 */

    free( messagebuf ); /* v1.8.5 */
}
//-----------------------------------------------------------------------
//
static void PlanarPlaneToCovebuf( const uint8_t *srcp, int src_width, int src_height, int src_pitch, uint8_t *coverbuf, int coverwidth, int coverheight, int coverpitch, int mirw, int mirh, bool interlaced )
{
    int h, w;
    int width2 = src_width + src_width + mirw + mirw - 2;
    uint8_t *coverbuf1 = coverbuf + coverpitch * mirh;

    if( !interlaced ) /* progressive */
    {
        for( h = mirh; h < src_height + mirh; h++ )
        {
            BitBlt( coverbuf1 + mirw, coverpitch, srcp, src_pitch, src_width, 1 ); /* copy line */
            for( w = 0; w < mirw; w++ )
            {
                coverbuf1[w] = coverbuf1[mirw + mirw - w]; /* mirror left border */
            }
            for( w = src_width + mirw; w < coverwidth; w++ )
            {
                coverbuf1[w] = coverbuf1[width2 - w]; /* mirror right border */
            }
            coverbuf1 += coverpitch;
            srcp      += src_pitch;
        }
    }
    else /* interlaced */
    {
        for( h = mirh; h < src_height / 2 + mirh; h++ ) /* first field */
        {
            BitBlt( coverbuf1 + mirw, coverpitch, srcp, src_pitch, src_width, 1 ); /* copy line */
            for( w = 0; w < mirw; w++ )
            {
                coverbuf1[w] = coverbuf1[mirw + mirw - w]; /* mirror left border */
            }
            for( w = src_width + mirw; w < coverwidth; w++ )
            {
                coverbuf1[w] = coverbuf1[width2 - w]; /* mirror right border */
            }
            coverbuf1 += coverpitch;
            srcp      += src_pitch * 2;
        }

        srcp -= src_pitch;
        for( h = src_height / 2 + mirh; h < src_height + mirh; h++ ) /* flip second field */
        {
            BitBlt( coverbuf1 + mirw, coverpitch, srcp, src_pitch, src_width, 1 ); /* copy line */
            for( w = 0; w < mirw; w++ )
            {
                coverbuf1[w] = coverbuf1[mirw + mirw - w]; /* mirror left border */
            }
            for( w = src_width + mirw; w < coverwidth; w++ )
            {
                coverbuf1[w] = coverbuf1[width2 - w]; /* mirror right border */
            }
            coverbuf1 += coverpitch;
            srcp      -= src_pitch * 2;
        }
    }

    uint8_t *pmirror = coverbuf1 - coverpitch * 2; /* pointer to vertical mirror */
    for( h = src_height + mirh; h < coverheight; h++ )
    {
        BitBlt( coverbuf1, coverpitch, pmirror, coverpitch, coverwidth, 1 ); /* mirror bottom line by line */
        coverbuf1 += coverpitch;
        pmirror   -= coverpitch;
    }
    coverbuf1 = coverbuf;
    pmirror   = coverbuf1 + coverpitch * mirh * 2; /* pointer to vertical mirror */
    for( h = 0; h < mirh; h++ )
    {
        BitBlt( coverbuf1, coverpitch, pmirror, coverpitch, coverwidth, 1 ); /* mirror bottom line by line */
        coverbuf1 += coverpitch;
        pmirror   -= coverpitch;
    }
}
//-----------------------------------------------------------------------
//
static void CoverbufToPlanarPlane( const uint8_t *coverbuf, int coverwidth, int coverheight, int coverpitch, uint8_t *dstp, int dst_width, int dst_height, int dst_pitch, int mirw, int mirh, bool interlaced )
{
    int h;
    const uint8_t *coverbuf1 = coverbuf + coverpitch * mirh + mirw;
    if( !interlaced ) /* progressive */
    {
        for( h = 0; h < dst_height; h++ )
        {
            BitBlt( dstp, dst_pitch, coverbuf1, coverpitch, dst_width, 1 ); /* copy pure frame size only */
            dstp      += dst_pitch;
            coverbuf1 += coverpitch;
        }
    }
    else /* interlaced */
    {
        for( h = 0; h < dst_height; h += 2 )
        {
            BitBlt( dstp, dst_pitch, coverbuf1, coverpitch, dst_width, 1 ); /* copy pure frame size only */
            dstp      += dst_pitch * 2;
            coverbuf1 += coverpitch;
        }
        /* second field is flipped */
        dstp -= dst_pitch;
        for( h = 0; h < dst_height; h += 2 )
        {
            BitBlt( dstp, dst_pitch, coverbuf1, coverpitch, dst_width, 1 ); /* copy pure frame size only */
            dstp      -= dst_pitch * 2;
            coverbuf1 += coverpitch;
        }
    }
}
//-----------------------------------------------------------------------
//
static void FramePlaneToCoverbuf
(
    int               plane,
    const VSFrameRef *src,
    uint8_t          *coverbuf,
    int               coverwidth,
    int               coverheight,
    int               coverpitch,
    int               mirw,
    int               mirh,
    bool              interlaced,
    const VSAPI      *vsapi
)
{
    const uint8_t *srcp       = vsapi->getReadPtr    ( src, plane );
    int            src_height = vsapi->getFrameHeight( src, plane );
    int            src_width  = vsapi->getFrameWidth ( src, plane );
    int            src_pitch  = vsapi->getStride     ( src, plane );
    PlanarPlaneToCovebuf( srcp, src_width, src_height, src_pitch, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced );
}
//-----------------------------------------------------------------------
//
static void CoverbufToFramePlane
(
    int            plane,
    const uint8_t *coverbuf,
    int            coverwidth,
    int            coverheight,
    int            coverpitch,
    VSFrameRef    *dst,
    int            mirw,
    int            mirh,
    bool           interlaced,
    const VSAPI   *vsapi
)
{
    uint8_t *dstp       = vsapi->getWritePtr   ( dst, plane );
    int      dst_height = vsapi->getFrameHeight( dst, plane );
    int      dst_width  = vsapi->getFrameWidth ( dst, plane );
    int      dst_pitch  = vsapi->getStride     ( dst, plane );
    CoverbufToPlanarPlane( coverbuf, coverwidth, coverheight, coverpitch, dstp, dst_width, dst_height, dst_pitch, mirw, mirh, interlaced );
}
//-----------------------------------------------------------------------
/* put source bytes to float array of overlapped blocks
 * use analysis windows */
void FFT3DFilter::InitOverlapPlane( float *inp0, const uint8_t *srcp0, int src_pitch, int planeBase )
{
    int w, h;
    int ihx, ihy;
    const uint8_t *srcp = srcp0;// + (hrest/2)*src_pitch + wrest/2; /* centered */
    float ftmp;
    int xoffset = bh * bw - (bw - ow); /* skip frames */
    int yoffset = bw * nox * bh - bw * (bh - oh); /* vertical offset of same block (overlap) */

    float *inp = inp0;

    ihy = 0; /* first top (big non-overlapped) part */
    {
        for( h = 0; h < oh; h++ )
        {
            inp = inp0 + h * bw;
            for( w = 0; w < ow; w++ )   /* left part  (non-overlapped) row of first block */
            {
                inp[w] = float(wanxl[w] * wanyl[h] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
            }
            for( w = ow; w < bw - ow; w++ )   /* left part  (non-overlapped) row of first block */
            {
                inp[w] = float(wanyl[h] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
            }
            inp  += bw - ow;
            srcp += bw - ow;
            for( ihx =1; ihx < nox; ihx += 1 ) /* middle horizontal blocks */
            {
                for( w = 0; w < ow; w++ )   /* first part (overlapped) row of block */
                {
                    ftmp = float(wanyl[h] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
                    inp[w          ] = ftmp * wanxr[w]; /* cur block */
                    inp[w + xoffset] = ftmp * wanxl[w]; /* overlapped Copy - next block */
                }
                inp  += ow;
                inp  += xoffset;
                srcp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* center part  (non-overlapped) row of first block */
                {
                    inp[w] = float(wanyl[h] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
                }
                inp  += bw - ow - ow;
                srcp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last part (non-overlapped) of line of last block */
            {
                inp[w] = float(wanxr[w] * wanyl[h] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
            }
            inp  += ow;
            srcp += ow;
            srcp += (src_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }
        for( h = oh; h < bh - oh; h++ )
        {
            inp = inp0 + h * bw;
            for( w = 0; w < ow; w++ )   /* left part  (non-overlapped) row of first block */
            {
                inp[w] = float(wanxl[w] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
            }
            for( w = ow; w < bw - ow; w++ )   /* left part  (non-overlapped) row of first block */
            {
                inp[w] = float((srcp[w] - planeBase));   /* Copy each byte from source to float array */
            }
            inp  += bw - ow;
            srcp += bw - ow;
            for( ihx = 1; ihx < nox; ihx += 1 ) /* middle horizontal blocks */
            {
                for( w = 0; w < ow; w++ )   /* first part (overlapped) row of block */
                {
                    ftmp = float((srcp[w] - planeBase));  /* Copy each byte from source to float array */
                    inp[w          ] = ftmp * wanxr[w]; /* cur block */
                    inp[w + xoffset] = ftmp * wanxl[w]; /* overlapped Copy - next block */
                }
                inp  += ow;
                inp  += xoffset;
                srcp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* center part  (non-overlapped) row of first block */
                {
                    inp[w] = float((srcp[w] - planeBase));   /* Copy each byte from source to float array */
                }
                inp  += bw - ow - ow;
                srcp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last part (non-overlapped) line of last block */
            {
                inp[w] = float(wanxr[w] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
            }
            inp  += ow;
            srcp += ow;

            srcp += (src_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }
    }

    for( ihy = 1; ihy < noy; ihy += 1 ) /* middle vertical */
    {
        for( h = 0; h < oh; h++ ) /* top overlapped part */
        {
            inp = inp0 + (ihy - 1) * (yoffset + (bh - oh) * bw) + (bh - oh) * bw + h * bw;
            for( w = 0; w < ow; w++ )   /* first half line of first block */
            {
                ftmp = float(wanxl[w] * (srcp[w] - planeBase));
                inp[w          ] = ftmp * wanyr[h];   /* Copy each byte from source to float array */
                inp[w + yoffset] = ftmp * wanyl[h];   /* y overlapped */
            }
            for( w = ow; w < bw - ow; w++ )   /* first half line of first block */
            {
                ftmp = float((srcp[w] - planeBase));
                inp[w          ] = ftmp * wanyr[h];   /* Copy each byte from source to float array */
                inp[w + yoffset] = ftmp * wanyl[h];   /* y overlapped */
            }
            inp  += bw - ow;
            srcp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half overlapped line of block */
                {
                    ftmp = float((srcp[w] - planeBase));   /* Copy each byte from source to float array */
                    inp[w                    ] = ftmp * wanxr[w] * wanyr[h];
                    inp[w + xoffset          ] = ftmp * wanxl[w] * wanyr[h];   /* x overlapped */
                    inp[w           + yoffset] = ftmp * wanxr[w] * wanyl[h];
                    inp[w + xoffset + yoffset] = ftmp * wanxl[w] * wanyl[h];   /* x overlapped */
                }
                inp  += ow;
                inp  += xoffset;
                srcp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* half non-overlapped line of block */
                {
                    ftmp = float((srcp[w] - planeBase));   /* Copy each byte from source to float array */
                    inp[w          ] = ftmp * wanyr[h];
                    inp[w + yoffset] = ftmp * wanyl[h];
                }
                inp  += bw - ow - ow;
                srcp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                ftmp = float(wanxr[w] * (srcp[w] - planeBase)); /* Copy each byte from source to float array */
                inp[w          ] = ftmp * wanyr[h];
                inp[w + yoffset] = ftmp * wanyl[h];
            }
            inp  += ow;
            srcp += ow;

            srcp += (src_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }
        /* middle  vertical nonovelapped part */
        for( h = 0; h < bh - oh - oh; h++ )
        {
            inp = inp0 + (ihy - 1) * (yoffset + (bh - oh) * bw) + (bh) * bw + h * bw + yoffset;
            for( w = 0; w < ow; w++ )   /* first half line of first block */
            {
                ftmp = float(wanxl[w] * (srcp[w] - planeBase));
                inp[w] = ftmp;   /* Copy each byte from source to float array */
            }
            for( w = ow; w < bw - ow; w++ )   /* first half line of first block */
            {
                ftmp = float((srcp[w] - planeBase));
                inp[w] = ftmp;   /* Copy each byte from source to float array */
            }
            inp  += bw - ow;
            srcp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half overlapped line of block */
                {
                    ftmp = float((srcp[w] - planeBase));   /* Copy each byte from source to float array */
                    inp[w          ] = ftmp * wanxr[w];
                    inp[w + xoffset] = ftmp * wanxl[w];   /* x overlapped */
                }
                inp  += ow;
                inp  += xoffset;
                srcp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* half non-overlapped line of block */
                {
                    ftmp = float((srcp[w] - planeBase));   /* Copy each byte from source to float array */
                    inp[w] = ftmp;
                }
                inp  += bw - ow - ow;
                srcp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                ftmp = float(wanxr[w] * (srcp[w] - planeBase)); /* Copy each byte from source to float array */
                inp[w] = ftmp;
            }
            inp  += ow;
            srcp += ow;

            srcp += (src_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }

    }

    ihy = noy ; /* last bottom  part */
    {
        for( h = 0; h < oh; h++ )
        {
            inp = inp0 + (ihy - 1) * (yoffset + (bh - oh) * bw) + (bh - oh) * bw + h * bw;
            for( w = 0; w < ow; w++ )   /* first half line of first block */
            {
                ftmp = float(wanxl[w] * wanyr[h] * (srcp[w] - planeBase));
                inp[w] = ftmp;   /* Copy each byte from source to float array */
            }
            for( w = ow; w < bw - ow; w++ )   /* first half line of first block */
            {
                ftmp = float(wanyr[h] * (srcp[w] - planeBase));
                inp[w] = ftmp;   /* Copy each byte from source to float array */
            }
            inp  += bw - ow;
            srcp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half line of block */
                {
                    float ftmp = float(wanyr[h] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
                    inp[w          ] = ftmp * wanxr[w];
                    inp[w + xoffset] = ftmp * wanxl[w];   /* overlapped Copy */
                }
                inp  += ow;
                inp  += xoffset;
                srcp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* center part  (non-overlapped) row of first block */
                {
                    inp[w] = float(wanyr[h] * (srcp[w] - planeBase));   /* Copy each byte from source to float array */
                }
                inp  += bw - ow - ow;
                srcp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                ftmp = float(wanxr[w] * wanyr[h] * (srcp[w] - planeBase));
                inp[w] = ftmp;   /* Copy each byte from source to float array */
            }
            inp  += ow;
            srcp += ow;

            srcp += (src_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }

    }
}
//
//-----------------------------------------------------------------------------------------
/* make destination frame plane from overlaped blocks
 * use synthesis windows wsynxl, wsynxr, wsynyl, wsynyr */
void FFT3DFilter::DecodeOverlapPlane( const float *inp0, float norm, uint8_t *dstp0, int dst_pitch, int planeBase )
{
    int w, h;
    int ihx, ihy;
    uint8_t *dstp = dstp0;// + (hrest/2)*dst_pitch + wrest/2; /* centered */
    const float *inp = inp0;
    int xoffset = bh * bw - (bw - ow);
    int yoffset = bw * nox * bh - bw * (bh - oh); /* vertical offset of same block (overlap) */

    ihy = 0; /* first top big non-overlapped) part */
    {
        for( h = 0; h < bh - oh; h++ )
        {
            inp = inp0 + h * bw;
            for( w = 0; w < bw - ow; w++ )   /* first half line of first block */
            {
                dstp[w] = std::min( 255, std::max( 0, (int)(inp[w] * norm) + planeBase ) ); /* Copy each byte from float array to dest with windows */
            }
            inp  += bw - ow;
            dstp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle horizontal half-blocks */
            {
                for( w = 0; w < ow; w++ )   /* half line of block */
                {
                    dstp[w] = std::min( 255, std::max( 0, (int)((inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * norm) + planeBase ) );  /* overlapped Copy */
                }
                inp  += xoffset + ow;
                dstp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* first half line of first block */
                {
                    dstp[w] = std::min( 255, std::max( 0, (int)(inp[w] * norm) + planeBase ) );   /* Copy each byte from float array to dest with windows */
                }
                inp  += bw - ow - ow;
                dstp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                dstp[w] = std::min( 255, std::max( 0,(int)(inp[w] * norm) + planeBase ) );
            }
            inp  += ow;
            dstp += ow;

            dstp += (dst_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the dest image. */
        }
    }

    for( ihy = 1; ihy < noy; ihy += 1 ) /* middle vertical */
    {
        for( h = 0; h < oh; h++ ) /* top overlapped part */
        {
            inp = inp0 + (ihy - 1) * (yoffset + (bh - oh) * bw) + (bh - oh) * bw + h * bw;

            float wsynyrh = wsynyr[h] * norm; /* remove from cycle for speed */
            float wsynylh = wsynyl[h] * norm;

            for( w = 0; w < bw - ow; w++ )   /* first half line of first block */
            {
                dstp[w] = std::min( 255, std::max( 0, (int)((inp[w] * wsynyrh + inp[w + yoffset] * wsynylh)) + planeBase ) );   /* y overlapped */
            }
            inp  += bw - ow;
            dstp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half overlapped line of block */
                {
                    dstp[w] = std::min( 255, std::max( 0, (int)(((inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * wsynyrh
                            + (inp[w + yoffset] * wsynxr[w] + inp[w + xoffset + yoffset] * wsynxl[w]) * wsynylh)) + planeBase ) );   /* x overlapped */
                }
                inp  += xoffset + ow;
                dstp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* double minus - half non-overlapped line of block */
                {
                    dstp[w] = std::min( 255, std::max( 0, (int)((inp[w] * wsynyrh + inp[w + yoffset] * wsynylh )) + planeBase ) );
                }
                inp  += bw - ow - ow;
                dstp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                dstp[w] = std::min( 255, std::max( 0, (int)((inp[w] * wsynyrh + inp[w + yoffset] * wsynylh)) + planeBase ) );
            }
            inp  += ow;
            dstp += ow;

            dstp += (dst_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }
        /* middle  vertical non-ovelapped part */
        for( h = 0; h < (bh - oh - oh); h++ )
        {
            inp = inp0 + (ihy - 1) * (yoffset + (bh - oh) * bw) + (bh) * bw + h * bw + yoffset;
            for( w = 0; w < bw - ow; w++ )   /* first half line of first block */
            {
                dstp[w] = std::min( 255, std::max( 0, (int)((inp[w]) * norm) + planeBase ) );
            }
            inp  += bw - ow;
            dstp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half overlapped line of block */
                {
                    dstp[w] = std::min( 255, std::max( 0, (int)((inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w] ) * norm) + planeBase ) );   /* x overlapped */
                }
                inp  += xoffset + ow;
                dstp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* half non-overlapped line of block */
                {
                    dstp[w] = std::min( 255, std::max( 0, (int)((inp[w]) * norm) + planeBase ) );
                }
                inp  += bw - ow - ow;
                dstp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                dstp[w] = std::min( 255, std::max( 0, (int)((inp[w]) * norm) + planeBase ) );
            }
            inp  += ow;
            dstp += ow;

            dstp += (dst_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }
    }

    ihy = noy ; /* last bottom part */
    {
        for( h = 0; h < oh; h++ )
        {
            inp = inp0 + (ihy - 1) * (yoffset + (bh - oh) * bw) + (bh - oh) * bw + h * bw;
            for( w = 0; w < bw - ow; w++ )   /* first half line of first block */
            {
                dstp[w] = std::min( 255, std::max( 0, (int)(inp[w] * norm) + planeBase ) );
            }
            inp  += bw - ow;
            dstp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half line of block */
                {
                    dstp[w] = std::min( 255, std::max( 0, (int)((inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * norm) + planeBase ) );  /* overlapped Copy */
                }
                inp  += xoffset + ow;
                dstp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* half line of block */
                {
                    dstp[w] = std::min( 255, std::max( 0, (int)((inp[w]) * norm) + planeBase ) );
                }
                inp  += bw - ow - ow;
                dstp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                dstp[w] = std::min( 255, std::max( 0, (int)(inp[w] * norm) + planeBase ) );
            }
            inp  += ow;
            dstp += ow;

            dstp += (dst_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }
    }
}
//-------------------------------------------------------------------------------------------
static void FindPatternBlock( const fftwf_complex *outcur0, int outwidth, int outpitch, int bh, int nox, int noy, int &px, int &py, const float *pwin, float degrid, const fftwf_complex *gridsample )
{
    /* since v1.7 outwidth must be really an outpitch */
    int h;
    int w;
    const fftwf_complex *outcur;
    float psd;
    float sigmaSquaredcur;
    float sigmaSquared = 1e15f;

    for( int by = 2; by < noy - 2; by++ )
    {
        for( int bx = 2; bx < nox - 2; bx++ )
        {
            outcur = outcur0 + nox * by * bh * outpitch + bx * bh * outpitch;
            sigmaSquaredcur = 0;
            float gcur = degrid * outcur[0][0] / gridsample[0][0]; /* grid (windowing) correction factor */
            for( h = 0; h < bh; h++ )
            {
                for( w = 0; w < outwidth; w++ )
                {
                    float grid0 = gcur * gridsample[w][0];
                    float grid1 = gcur * gridsample[w][1];
                    float corrected0 = outcur[w][0] - grid0;
                    float corrected1 = outcur[w][1] - grid1;
                    psd = corrected0 * corrected0 + corrected1 * corrected1;
                    sigmaSquaredcur += psd * pwin[w]; /* windowing */
                }
                outcur     += outpitch;
                pwin       += outpitch;
                gridsample += outpitch;
            }
            pwin -= outpitch * bh; /* restore */
            if( sigmaSquaredcur < sigmaSquared )
            {
                px = bx;
                py = by;
                sigmaSquared = sigmaSquaredcur;
            }
        }
    }
}
//-------------------------------------------------------------------------------------------
static void SetPattern( const fftwf_complex *outcur, int outwidth, int outpitch, int bh, int nox, int noy, int px, int py, const float *pwin, float *pattern2d, float &psigma, float degrid, const fftwf_complex *gridsample )
{
    int h;
    int w;
    outcur += nox * py * bh * outpitch + px * bh * outpitch;
    float psd;
    float sigmaSquared = 0;
    float weight = 0;

    for( h = 0; h < bh; h++ )
    {
        for( w = 0; w < outwidth; w++ )
        {
            weight += pwin[w];
        }
        pwin += outpitch;
    }
    pwin -= outpitch * bh; /* restore */

    float gcur = degrid * outcur[0][0] / gridsample[0][0]; /* grid (windowing) correction factor */

    for( h = 0; h < bh; h++ )
    {
        for( w = 0; w < outwidth; w++ )
        {
            float grid0 = gcur * gridsample[w][0];
            float grid1 = gcur * gridsample[w][1];
            float corrected0 = outcur[w][0] - grid0;
            float corrected1 = outcur[w][1] - grid1;
            psd = corrected0 * corrected0 + corrected1 * corrected1;
            pattern2d[w] = psd * pwin[w]; /* windowing */
            sigmaSquared += pattern2d[w]; /* sum */
        }
        outcur     += outpitch;
        pattern2d  += outpitch;
        pwin       += outpitch;
        gridsample += outpitch;
    }
    psigma = sqrt( sigmaSquared / (weight * bh * outwidth) ); /* mean std deviation (sigma) */
}
//-------------------------------------------------------------------------------------------
static void PutPatternOnly( fftwf_complex *outcur, int outwidth, int outpitch, int bh, int nox, int noy, int px, int py )
{
    int h,w;
    int block;
    int pblock = py * nox + px;
    int blocks = nox * noy;

    for( block = 0; block < pblock; block++ )
    {
        for( h = 0; h < bh; h++ )
        {
            for( w = 0; w < outwidth; w++ )
            {
                outcur[w][0] = 0;
                outcur[w][1] = 0;
            }
            outcur += outpitch;
        }
    }

    outcur += bh * outpitch;

    for( block = pblock + 1; block < blocks; block++ )
    {
        for( h = 0; h < bh; h++ )
        {
            for( w = 0; w < outwidth; w++ )
            {
                outcur[w][0] = 0;
                outcur[w][1] = 0;
            }
            outcur += outpitch;
        }
    }
}
//-------------------------------------------------------------------------------------------
static void Pattern2Dto3D( const float *pattern2d, int bh, int outwidth, int outpitch, float mult, float *pattern3d )
{
    /* slow, but executed once only per clip */
    int size = bh * outpitch;
    for( int i = 0; i < size; i++ )
    { /* get 3D pattern */
        pattern3d[i] = pattern2d[i] * mult;
    }
}
//-------------------------------------------------------------------------------------------
static void Copyfft( fftwf_complex *outrez, const fftwf_complex *outprev, int outsize )
{ /* save outprev to outrez to prevent cache change (inverse fft2d will destroy the array) */
#if 0
    for( int i = 0; i < outsize; i++ )
    {
        outrez[i][0] = outprev[i][0];
        outrez[i][1] = outprev[i][1];
    }
#else
    /* more fast */
    BitBlt( (uint8_t *)&outrez [0][0], outsize * 8,
            (uint8_t *)&outprev[0][0], outsize * 8,
            outsize * 8, 1 );
#endif
}
//-------------------------------------------------------------------------------------------
static void SortCache( int *cachewhat, fftwf_complex **cachefft, int cachesize, int cachestart, int cachestartold )
{
    /* sort ordered series, put existant ffts to proper places */
    int i;
    int ctemp;
    fftwf_complex *ffttemp;

    int offset = cachestart - cachestartold;
    if( offset > 0 ) /* right */
    {
        for( i = 0; i < cachesize; i++ )
        {
            if( (i + offset) < cachesize )
            {
                /* swap */
                ctemp = cachewhat[i + offset];
                cachewhat[i + offset] = cachewhat[i];
                cachewhat[i         ] = ctemp;
                ffttemp = cachefft[i + offset];
                cachefft[i + offset] = cachefft[i];
                cachefft[i         ] = ffttemp;
            }
        }
    }
    else if( offset < 0 )
    {
        for( i = cachesize - 1; i >= 0; i-- )
        {
            if( (i + offset) >= 0 )
            {
                ctemp = cachewhat[i + offset];
                cachewhat[i + offset] = cachewhat[i];
                cachewhat[i         ] = ctemp;
                ffttemp = cachefft[i + offset];
                cachefft[i + offset] = cachefft[i];
                cachefft[i         ] = ffttemp;
            }
        }
    }
}
//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------
static void CopyFrame
(
    const VSFrameRef *src,
    VSFrameRef       *dst,
    VSVideoInfo      &vi,
    int               planeskip,
    const VSAPI      *vsapi
)
{
    for( int plane = 0; plane < vi.format->numPlanes; plane++ )
        if( plane != planeskip )
        {
            const uint8_t *srcp       = vsapi->getReadPtr    ( src, plane );
            int            src_pitch  = vsapi->getStride     ( src, plane );
                  uint8_t *dstp       = vsapi->getWritePtr   ( dst, plane );
            int            dst_height = vsapi->getFrameHeight( dst, plane );
            int            dst_width  = vsapi->getFrameWidth ( dst, plane );
            int            dst_pitch  = vsapi->getStride     ( dst, plane );
            BitBlt( dstp, dst_pitch, srcp, src_pitch, dst_width, dst_height ); /* copy one plane */
        }
}
//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------
template < int btcur >
void FFT3DFilter::Wiener3D
(
    int               n,
    const VSFrameRef *src,
    VSFrameContext   *frame_ctx,
    const VSAPI      *vsapi
)
{
    /* cycle prev2(-2), prev(-1), cur(0), next(1) and next2(2) */
    constexpr int cachecur      = btcur / 2 + 1;
    int           cachestart    = n     - cachecur;
    int           cachestartold = nlast - cachecur;
    SortCache( cachewhat, cachefft, cachesize, cachestart, cachestartold );

    fftwf_complex **out = &outcache[btcur / 2];

    for( int offset = - btcur / 2; offset <= (btcur - 1) / 2; ++offset )
    {
        out[offset] = cachefft[cachecur + offset];
        if( cachewhat[cachecur + offset] != n + offset )
        {
            /* calculate out[offset] */
            if( offset != 0 )
            {
                const VSFrameRef *frame = vsapi->getFrameFilter( n + offset, node, frame_ctx );
                FramePlaneToCoverbuf( plane, frame, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
                vsapi->freeFrame( frame );
            }
            else
                FramePlaneToCoverbuf( plane, src, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
            FFT3DFilter::InitOverlapPlane( in, coverbuf, coverpitch, planeBase );
            /* make FFT 2D */
            fftwf_execute_dft_r2c( plan, in, out[offset] );
            cachewhat[cachecur + offset] = n + offset;
        }
        if( offset == - btcur / 2 )
        {
            if( n != nlast + 1 )
            {
                Copyfft( outrez, out[offset], outsize ); /* save out[offset] to outrez to prevent its change in cache */
            }
            else
            {
                /* swap */
                outtemp     = outrez;
                outrez      = out[offset];
                out[offset] = outtemp;
                cachefft [cachecur + offset] = outtemp;
                cachewhat[cachecur + offset] = -1; /* will be destroyed */
            }
        }
    }

    fftwf_complex *outp[5] = { out[-2], out[-1], out[0], out[1], out[2] };
    outp[2 - btcur / 2] = outrez;

    if( degrid != 0 )
    {
        if( pfactor != 0 )
            ApplyPattern3D_degrid< btcur >( outp[2], outp[0], outp[1], outp[3], outp[4], outwidth, outpitch, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
        else
            ApplyWiener3D_degrid< btcur >( outp[2], outp[0], outp[1], outp[3], outp[4], outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
        Sharpen_degrid( outrez, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, degrid, gridsample, dehalo, wdehalo, ht2n );
    }
    else
    {
        if( pfactor != 0 )
            ApplyPattern3D< btcur >( outp[2], outp[0], outp[1], outp[3], outp[4], outwidth, outpitch, bh, howmanyblocks, pattern3d, beta );
        else
            ApplyWiener3D< btcur >( outp[2], outp[0], outp[1], outp[3], outp[4], outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
        Sharpen( outrez, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, dehalo, wdehalo, ht2n );
    }
    /* do inverse FFT 2D, get filtered 'in' array
     * note: input "outrez" array is destroyed by execute algo. */
    fftwf_execute_dft_c2r( planinv, outrez, in );
}

void FFT3DFilter::ApplyFilter
(
    int               n,
    VSFrameRef       *dst,
    const VSFrameRef *src,
    VSFrameContext   *frame_ctx,
    VSCore           *core,
    const VSAPI      *vsapi
)
{
    //_asm emms;

    if( plane == 0 )
        planeBase = 0;
    else
        planeBase = 128; /* neutral chroma value */

    if( pfactor != 0 && isPatternSet == false && pshow == false ) /* get noise pattern */
    {
        const VSFrameRef *psrc = vsapi->getFrameFilter( pframe, node, frame_ctx ); /* get noise pattern frame */

        /* put source bytes to float array of overlapped blocks */
        FramePlaneToCoverbuf( plane, psrc, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        vsapi->freeFrame( psrc );
        FFT3DFilter::InitOverlapPlane( in, coverbuf, coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan, in, outrez );
        if( px == 0 && py == 0 ) /* try find pattern block with minimal noise sigma */
            FindPatternBlock( outrez, outwidth, outpitch, bh, nox, noy, px, py, pwin, degrid, gridsample );
        SetPattern( outrez, outwidth, outpitch, bh, nox, noy, px, py, pwin, pattern2d, psigma, degrid, gridsample );
        isPatternSet = true;
    }
    else if( pfactor != 0 && pshow == true )
    {   /* show noise pattern window */
        /* put source bytes to float array of overlapped blocks */
        FramePlaneToCoverbuf( plane, src, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        FFT3DFilter::InitOverlapPlane( in, coverbuf, coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan, in, outrez );

        int pxf, pyf;
        if( px == 0 && py == 0 ) /* try find pattern block with minimal noise sigma */
            FindPatternBlock( outrez, outwidth, outpitch, bh, nox, noy, pxf, pyf, pwin, degrid, gridsample );
        else
        {
            pxf = px; /* fixed bug in v1.6 */
            pyf = py;
        }
        SetPattern( outrez, outwidth, outpitch, bh, nox, noy, pxf, pyf, pwin, pattern2d, psigma, degrid, gridsample );

        /* change analysis and synthesis window to constant to show */
        for( int i = 0; i < ow; i++ )
        {
            wanxl[i] = 1;    wanxr[i] = 1;    wsynxl[i] = 1;    wsynxr[i] = 1;
        }
        for( int i = 0; i < oh; i++ )
        {
            wanyl[i] = 1;    wanyr[i] = 1;    wsynyl[i] = 1;    wsynyr[i] = 1;
        }

        planeBase = 128;

        /* put source bytes to float array of overlapped blocks */
        /* cur frame */
        FramePlaneToCoverbuf( plane, src, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        FFT3DFilter::InitOverlapPlane( in, coverbuf, coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan, in, outrez );

        PutPatternOnly( outrez, outwidth, outpitch, bh, nox, noy, pxf, pyf );
        /* do inverse 2D FFT, get filtered 'in' array */
        fftwf_execute_dft_c2r( planinv, outrez, in );

        /* make destination frame plane from current overlaped blocks */
        FFT3DFilter::DecodeOverlapPlane( in, norm, coverbuf, coverpitch, planeBase );
        CoverbufToFramePlane( plane, coverbuf, coverwidth, coverheight, coverpitch, dst, mirw, mirh, interlaced, vsapi );
        int psigmaint = ((int)(10 * psigma)) / 10;
        int psigmadec = (int)((psigma - psigmaint) * 10);
        sprintf( messagebuf," frame=%d, px=%d, py=%d, sigma=%d.%d", n, pxf, pyf, psigmaint, psigmadec );
        DrawString( dst, 0, 0, messagebuf, vsapi );

        return;
    }

    int btcur = bt; /* bt used for current frame */

    if( (bt / 2 > n) || (bt - 1) / 2 > (vi.numFrames - 1 - n) )
    {
        btcur = 1; /* do 2D filter for first and last frames */
    }

    if( btcur > 0 ) /* Wiener */
    {
        sigmaSquaredNoiseNormed = btcur * sigma * sigma / norm; /* normalized variation=sigma^2 */

        if( btcur != btcurlast )
            Pattern2Dto3D( pattern2d, bh, outwidth, outpitch, (float)btcur, pattern3d );

        /* get power spectral density (abs quadrat) for every block and apply filter */

        /* put source bytes to float array of overlapped blocks */

        if( btcur == 1 ) /* 2D */
        {
            /* cur frame */
            FramePlaneToCoverbuf( plane, src, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
            FFT3DFilter::InitOverlapPlane( in, coverbuf, coverpitch, planeBase );

            /* make FFT 2D */
            fftwf_execute_dft_r2c( plan, in, outrez );
            if( degrid != 0 )
            {
                if( pfactor != 0 )
                {
                    ApplyPattern2D_degrid_C( outrez, outwidth, outpitch, bh, howmanyblocks, pfactor, pattern2d, beta, degrid, gridsample );
                    Sharpen_degrid( outrez, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, degrid, gridsample, dehalo, wdehalo, ht2n );
                }
                else
                    ApplyWiener2D_degrid_C( outrez, outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, degrid, gridsample, dehalo, wdehalo, ht2n );
            }
            else
            {
                if( pfactor != 0 )
                {
                    ApplyPattern2D( outrez, outwidth, outpitch, bh, howmanyblocks, pfactor, pattern2d, beta );
                    Sharpen( outrez, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, dehalo, wdehalo, ht2n );
                }
                else
                    ApplyWiener2D( outrez, outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, dehalo, wdehalo, ht2n );
            }

            /* do inverse FFT 2D, get filtered 'in' array */
            fftwf_execute_dft_c2r( planinv, outrez, in );
        }
        else if( btcur == 2 ) /* 3D2 */
        {
            Wiener3D< 2 >( n, src, frame_ctx, vsapi );
        }
        else if( btcur == 3 ) /* 3D3 */
        {
            Wiener3D< 3 >( n, src, frame_ctx, vsapi );
        }
        else if( btcur == 4 ) /* 3D4 */
        {
            Wiener3D< 4 >( n, src, frame_ctx, vsapi );
        }
        else if( btcur == 5 ) /* 3D5 */
        {
            Wiener3D< 5 >( n, src, frame_ctx, vsapi );
        }
        /* make destination frame plane from current overlaped blocks */
        FFT3DFilter::DecodeOverlapPlane( in, norm, coverbuf, coverpitch, planeBase );
        CoverbufToFramePlane( plane, coverbuf, coverwidth, coverheight, coverpitch, dst, mirw, mirh, interlaced, vsapi );
    }
    else if( bt == 0 ) /* Kalman filter */
    {
        /* get power spectral density (abs quadrat) for every block and apply filter */

        if( n == 0 )
            return;

        /* put source bytes to float array of overlapped blocks */
        /* cur frame */
        FramePlaneToCoverbuf( plane, src, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        FFT3DFilter::InitOverlapPlane( in, coverbuf, coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan, in, outrez );
        if( pfactor != 0 )
            ApplyKalmanPattern( outrez, outLast, covar, covarProcess, outwidth, outpitch, bh, howmanyblocks, pattern2d, kratio * kratio );
        else
            ApplyKalman( outrez, outLast, covar, covarProcess, outwidth, outpitch, bh, howmanyblocks, sigmaSquaredNoiseNormed2D, kratio * kratio );

        /* copy outLast to outrez */
        BitBlt( (uint8_t *)&outrez [0][0], outsize * sizeof(fftwf_complex),
                (uint8_t *)&outLast[0][0], outsize * sizeof(fftwf_complex),
                outsize * sizeof(fftwf_complex), 1 );  /* v.0.9.2 */
        if( degrid != 0 )
            Sharpen_degrid( outrez, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, degrid, gridsample, dehalo, wdehalo, ht2n );
        else
            Sharpen( outrez, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, dehalo, wdehalo, ht2n );
        /* do inverse FFT 2D, get filtered 'in' array
         * note: input "out" array is destroyed by execute algo.
         * that is why we must have its copy in "outLast" array */
        fftwf_execute_dft_c2r( planinv, outrez, in );
        /* make destination frame plane from current overlaped blocks */
        FFT3DFilter::DecodeOverlapPlane( in, norm, coverbuf, coverpitch, planeBase );
        CoverbufToFramePlane( plane, coverbuf, coverwidth, coverheight, coverpitch, dst, mirw, mirh, interlaced, vsapi );
    }
    else if( bt == -1 ) /* sharpen only */
    {
        /* put source bytes to float array of overlapped blocks */
        FramePlaneToCoverbuf( plane, src, coverbuf, coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        FFT3DFilter::InitOverlapPlane( in, coverbuf, coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan, in, outrez );
        if( degrid != 0 )
            Sharpen_degrid( outrez, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, degrid, gridsample, dehalo, wdehalo, ht2n );
        else
            Sharpen( outrez, outwidth, outpitch, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen, dehalo, wdehalo, ht2n );
        /* do inverse FFT 2D, get filtered 'in' array */
        fftwf_execute_dft_c2r( planinv, outrez, in );
        /* make destination frame plane from current overlaped blocks */
        FFT3DFilter::DecodeOverlapPlane( in, norm, coverbuf, coverpitch, planeBase );
        CoverbufToFramePlane( plane, coverbuf, coverwidth, coverheight, coverpitch, dst, mirw, mirh, interlaced, vsapi );
    }

    if( btcur == bt )
    {/* for normal step */
        nlast = n; /* set last frame to current */
    }
    btcurlast = btcur;

    /* As we now are finished processing the image. */
}

//-------------------------------------------------------------------
FFT3DFilterMulti::FFT3DFilterMulti
(
    float _sigma, float _beta, int _multiplane, int _bw, int _bh, int _bt, int _ow, int _oh,
    float _kratio, float _sharpen, float _scutoff, float _svr, float _smin, float _smax,
    bool _measure, bool _interlaced, int _wintype,
    int _pframe, int _px, int _py, bool _pshow, float _pcutoff, float _pfactor,
    float _sigma2, float _sigma3, float _sigma4, float _degrid,
    float _dehalo, float _hr, float _ht, int _ncpu,
    const VSMap *in, const VSAPI *vsapi
) : filtered( nullptr ), YClip( nullptr ), UClip( nullptr ), VClip( nullptr ),
    bt( _bt ), pframe( _pframe ), pshow( _pshow ), pfactor( _pfactor )
{
    node =  vsapi->propGetNode( in, "clip", 0, 0 );
    vi   = *vsapi->getVideoInfo( node );

    if( vi.format->colorFamily == cmGray )
        _multiplane = 0;
    multiplane = _multiplane;

    if( _multiplane == 0 || _multiplane == 1 || _multiplane == 2 )
    {
        filtered = new FFT3DFilter( _sigma, _beta, _multiplane, _bw, _bh, _bt, _ow, _oh,
                                    _kratio, _sharpen, _scutoff, _svr, _smin, _smax,
                                    _measure, _interlaced, _wintype,
                                    _pframe, _px, _py, _pshow, _pcutoff, _pfactor,
                                    _sigma2, _sigma3, _sigma4, _degrid, _dehalo, _hr, _ht, _ncpu, _multiplane,
                                    vi, node );
        isPatternSet = filtered->getIsPatternSet();
    }
    else if( _multiplane == 3 || _multiplane == 4 )
    {

        UClip = new FFT3DFilter( _sigma, _beta, 1, _bw, _bh, _bt, _ow, _oh,
                                 _kratio, _sharpen, _scutoff, _svr, _smin, _smax,
                                 _measure, _interlaced, _wintype,
                                 _pframe, _px, _py, _pshow, _pcutoff, _pfactor,
                                 _sigma2, _sigma3, _sigma4, _degrid, _dehalo, _hr,  _ht, _ncpu, _multiplane,
                                 vi, node );

        VClip = new FFT3DFilter( _sigma, _beta, 2, _bw, _bh, _bt, _ow, _oh,
                                 _kratio, _sharpen, _scutoff, _svr, _smin, _smax,
                                 _measure, _interlaced, _wintype,
                                 _pframe, _px, _py, _pshow, _pcutoff, _pfactor,
                                 _sigma2, _sigma3, _sigma4, _degrid, _dehalo, _hr,  _ht, _ncpu, _multiplane,
                                 vi, node );

        if( _multiplane != 3 )
        {
            YClip = new FFT3DFilter( _sigma, _beta, 0, _bw, _bh, _bt, _ow, _oh,
                                     _kratio, _sharpen, _scutoff, _svr, _smin, _smax,
                                     _measure, _interlaced, _wintype,
                                     _pframe, _px, _py, _pshow, _pcutoff, _pfactor,
                                     _sigma2, _sigma3, _sigma4, _degrid, _dehalo, _hr, _ht, _ncpu, _multiplane,
                                     vi, node );
        }
        isPatternSet = UClip->getIsPatternSet();
    }
    else
        throw bad_param{ "plane must be from 0 to 4!" };
}

FFT3DFilterMulti::~FFT3DFilterMulti()
{
    delete filtered;
    delete YClip;
    delete UClip;
    delete VClip;
}

void FFT3DFilterMulti::RequestFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    if( pfactor != 0 && isPatternSet == false && pshow == false )
        vsapi->requestFrameFilter( pframe, node, frame_ctx );

    int btcur = bt; /* bt used for current frame */

    if( (bt / 2 > n) || (bt - 1) / 2 > (vi.numFrames - 1 - n) )
    {
        btcur = 1; /* do 2D filter for first and last frames */
    }

    if( btcur > 0 )
    {
        for( int i = n - btcur / 2; i <= n + (btcur - 1) / 2; ++i )
            vsapi->requestFrameFilter( i, node, frame_ctx );
    }
    else
        vsapi->requestFrameFilter( n, node, frame_ctx );
}

VSFrameRef *FFT3DFilterMulti::newVideoFrame
(
    const VSFrameRef *src,
    VSCore           *core,
    const VSAPI      *vsapi
)
{
    VSFrameRef *dst = vsapi->newVideoFrame
    (
        vsapi->getFrameFormat( src ),
        vsapi->getFrameWidth ( src, 0 ),
        vsapi->getFrameHeight( src, 0 ),
        src, core
    );
    return dst;
}

VSFrameRef *FFT3DFilterMulti::GetFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    /* Request frame 'n' from the source clip. */
    const VSFrameRef *src = vsapi->getFrameFilter( n, node, frame_ctx );

    VSFrameRef *dst;
    if( pfactor != 0 && pshow == true )
        dst = vsapi->copyFrame( src, core );
    else if( bt == 0 && n == 0 )
        /* Kalman filter does nothing for the first frame. */
        dst = const_cast<VSFrameRef *>(vsapi->cloneFrameRef( src ));
    else
    {
        dst = newVideoFrame( src, core, vsapi );
        if( multiplane < 3 )
            CopyFrame( src, dst, vi, multiplane, vsapi );
    }

    if( multiplane < 3 )
    {
        filtered->ApplyFilter( n, dst, src, frame_ctx, core, vsapi );
        isPatternSet = filtered->getIsPatternSet();
    }
    else
    {
        if( YClip != nullptr )
            YClip->ApplyFilter( n, dst, src, frame_ctx, core, vsapi );
        else
            BitBlt( vsapi->getWritePtr( dst, 0 ), vsapi->getStride( dst, 0 ), vsapi->getReadPtr( src, 0 ),
                    vsapi->getStride( src, 0 ), vsapi->getFrameWidth( src, 0 ), vsapi->getFrameHeight( src, 0 ) );
        UClip->ApplyFilter( n, dst, src, frame_ctx, core, vsapi );
        VClip->ApplyFilter( n, dst, src, frame_ctx, core, vsapi );
        isPatternSet = UClip->getIsPatternSet();
    }

    vsapi->freeFrame( src );
    return dst;
}
