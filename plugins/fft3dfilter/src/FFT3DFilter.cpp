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
 * You must put libfftw3f-3.dll in the same directory as the plugin dll
 *
 * The algorithm is based on the 3D IIR/3D Frequency Domain Filter from:
 * MOTION PICTURE RESTORATION. by Anil Christopher Kokaram. Ph.D. Thesis. May 1993.
 * http://www.mee.tcd.ie/~ack/papers/a4ackphd.ps.gz
 *
 *****************************************************************************/

#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "FFT3DFilter.h"

/** declarations of filtering functions: **/
/* C */
void ApplyWiener2D_C( fftwf_complex *out, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float dehalo, const float *wdehalo, float ht2n );
void ApplyPattern2D_C( fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int howmanyblocks, float pfactor, const float *pattern2d0, float beta );
void ApplyWiener3D2_C( const fftwf_complex *outcur, fftwf_complex *outprev, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta );
void ApplyPattern3D2_C( const fftwf_complex *outcur, fftwf_complex *outprev, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta );
void ApplyWiener3D3_C( const fftwf_complex *out, fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta );
void ApplyPattern3D3_C( const fftwf_complex *out, fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta );
void ApplyWiener3D4_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta );
void ApplyPattern3D4_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta );
void ApplyWiener3D5_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta );
void ApplyPattern3D5_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta );
void ApplyKalmanPattern_C( const fftwf_complex *outcur, fftwf_complex *outLast, fftwf_complex *covar, fftwf_complex *covarProcess, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *covarNoiseNormed, float kratio2 );
void ApplyKalman_C( const fftwf_complex *outcur, fftwf_complex *outLast, fftwf_complex *covar, fftwf_complex *covarProcess, int outwidth, int outpitchelems, int bh, int howmanyblocks,  float covarNoiseNormed, float kratio2 );
void Sharpen_C( fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float dehalo, const float *wdehalo, float ht2n );
/* degrid_C */
void ApplyWiener2D_degrid_C( fftwf_complex *out, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float degrid, const fftwf_complex *gridsample, float dehalo, const float *wdehalo, float ht2n );
void ApplyWiener3D2_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyWiener3D3_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyWiener3D4_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyWiener3D5_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, const fftwf_complex *gridsample );
void Sharpen_degrid_C( fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float degrid, const fftwf_complex *gridsample, float dehalo, const float *wdehalo, float ht2n );
void ApplyPattern2D_degrid_C( fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int howmanyblocks, float pfactor, const float *pattern2d0, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyPattern3D2_degrid_C( const fftwf_complex *outcur, fftwf_complex *outprev, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyPattern3D3_degrid_C( const fftwf_complex *out, fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyPattern3D4_degrid_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta, float degrid, const fftwf_complex *gridsample );
void ApplyPattern3D5_degrid_C( const fftwf_complex *out, fftwf_complex *outprev2, const fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta, float degrid, const fftwf_complex *gridsample );
//-------------------------------------------------------------------------------------------
static void ApplyWiener2D( fftwf_complex *out, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed,
                           float beta, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float dehalo, const float *wdehalo, float ht2n )
{
    ApplyWiener2D_C( out, outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, sharpen, sigmaSquaredSharpenMin, sigmaSquaredSharpenMax, wsharpen, dehalo, wdehalo, ht2n );
}
//-------------------------------------------------------------------------------------------
static void ApplyPattern2D( fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int howmanyblocks, float pfactor, const float *pattern2d0, float beta )
{
    ApplyPattern2D_C( outcur, outwidth, outpitchelems, bh, howmanyblocks, pfactor, pattern2d0, beta );
}
//-------------------------------------------------------------------------------------------
template < int btcur >
static void ApplyWiener3D_degrid( const fftwf_complex *out, fftwf_complex *outprev2, fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta, float degrid, fftwf_complex *gridsample )
{
    if( btcur == 5 ) ApplyWiener3D5_degrid_C( out, outprev2, outprev, outnext, outnext2, outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
    if( btcur == 4 ) ApplyWiener3D4_degrid_C( out, outprev2, outprev, outnext,           outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
    if( btcur == 3 ) ApplyWiener3D3_degrid_C( out,           outprev, outnext,           outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
    if( btcur == 2 ) ApplyWiener3D2_degrid_C( out,           outprev,                    outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample );
}
//-------------------------------------------------------------------------------------------
template < int btcur >
static void ApplyPattern3D_degrid( const fftwf_complex *out, fftwf_complex *outprev2, fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitchelems, int bh, int howmanyblocks, float *pattern3d, float beta, float degrid, fftwf_complex *gridsample )
{
    if( btcur == 5 ) ApplyPattern3D5_degrid_C( out, outprev2, outprev, outnext, outnext2, outwidth, outpitchelems, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
    if( btcur == 4 ) ApplyPattern3D4_degrid_C( out, outprev2, outprev, outnext,           outwidth, outpitchelems, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
    if( btcur == 3 ) ApplyPattern3D3_degrid_C( out,           outprev, outnext,           outwidth, outpitchelems, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
    if( btcur == 2 ) ApplyPattern3D2_degrid_C( out,           outprev,                    outwidth, outpitchelems, bh, howmanyblocks, pattern3d, beta, degrid, gridsample );
}
//-------------------------------------------------------------------------------------------
template < int btcur >
static void ApplyWiener3D( const fftwf_complex *out, fftwf_complex *outprev2, fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sigmaSquaredNoiseNormed, float beta )
{
    if( btcur == 5 ) ApplyWiener3D5_C( out, outprev2, outprev, outnext, outnext2, outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
    if( btcur == 4 ) ApplyWiener3D4_C( out, outprev2, outprev, outnext,           outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
    if( btcur == 3 ) ApplyWiener3D3_C( out,           outprev, outnext,           outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
    if( btcur == 2 ) ApplyWiener3D2_C( out,           outprev,                    outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
}
//-------------------------------------------------------------------------------------------
template < int btcur >
static void ApplyPattern3D( const fftwf_complex *out, fftwf_complex *outprev2, fftwf_complex *outprev, const fftwf_complex *outnext, const fftwf_complex *outnext2, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *pattern3d, float beta )
{
    if( btcur == 5 ) ApplyPattern3D5_C( out, outprev2, outprev, outnext, outnext2, outwidth, outpitchelems, bh, howmanyblocks, pattern3d, beta );
    if( btcur == 4 ) ApplyPattern3D4_C( out, outprev2, outprev, outnext,           outwidth, outpitchelems, bh, howmanyblocks, pattern3d, beta );
    if( btcur == 3 ) ApplyPattern3D3_C( out,           outprev, outnext,           outwidth, outpitchelems, bh, howmanyblocks, pattern3d, beta );
    if( btcur == 2 ) ApplyPattern3D2_C( out,           outprev,                    outwidth, outpitchelems, bh, howmanyblocks, pattern3d, beta );
}
//-------------------------------------------------------------------------------------------
static void ApplyKalmanPattern( const fftwf_complex *outcur, fftwf_complex *outLast, fftwf_complex *covar, fftwf_complex *covarProcess, int outwidth, int outpitchelems, int bh, int howmanyblocks, const float *covarNoiseNormed, float kratio2 )
{
    ApplyKalmanPattern_C( outcur, outLast, covar, covarProcess, outwidth, outpitchelems, bh, howmanyblocks,  covarNoiseNormed, kratio2 );
}
//-------------------------------------------------------------------------------------------
static void ApplyKalman( const fftwf_complex *outcur, fftwf_complex *outLast, fftwf_complex *covar, fftwf_complex *covarProcess, int outwidth, int outpitchelems, int bh, int howmanyblocks,  float covarNoiseNormed, float kratio2 )
{
    ApplyKalman_C( outcur, outLast, covar, covarProcess, outwidth, outpitchelems, bh, howmanyblocks,  covarNoiseNormed, kratio2 );
}
//-------------------------------------------------------------------------------------------
static void Sharpen( fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float dehalo, const float *wdehalo, float ht2n )
{
    Sharpen_C( outcur, outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMin, sigmaSquaredSharpenMax, wsharpen, dehalo, wdehalo, ht2n );
}
//-------------------------------------------------------------------------------------------
static void Sharpen_degrid( fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int howmanyblocks, float sharpen, float sigmaSquaredSharpenMin, float sigmaSquaredSharpenMax, const float *wsharpen, float degrid, const fftwf_complex *gridsample, float dehalo, const float *wdehalo, float ht2n )
{
    Sharpen_degrid_C( outcur, outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMin, sigmaSquaredSharpenMax, wsharpen, degrid, gridsample, dehalo, wdehalo, ht2n );
}
//-------------------------------------------------------------------------------------------
template<typename T>
static void fft3d_memset(T *dst, T val, size_t count) {
    for (size_t i = 0; i < count; i++)
        dst[i] = val;
}
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
static void SigmasToPattern( float sigma, float sigma2, float sigma3, float sigma4, int bh, int outwidth, int outpitchelems, float norm, float *pattern2d )
{
    /* it is not fast, but called only in constructor */
    float sigmacur;
    const float ft2 = sqrt( 0.5f ) / 2; /* frequency for sigma2 */
    const float ft3 = sqrt( 0.5f ) / 4; /* frequency for sigma3 */
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
        pattern2d += outpitchelems;
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
    float _dehalo, float _hr, float _ht, int _ncpu,
    VSVideoInfo _vi, VSNodeRef *_node
) : sigma(_sigma), beta(_beta), plane(_plane), bw(_bw), bh(_bh), bt(_bt), ow(_ow), oh(_oh),
kratio(_kratio), sharpen(_sharpen), scutoff(_scutoff), svr(_svr), smin(_smin), smax(_smax),
measure(_measure), interlaced(_interlaced), wintype(_wintype),
pframe(_pframe), px(_px), py(_py), pshow(_pshow), pcutoff(_pcutoff), pfactor(_pfactor),
sigma2(_sigma2), sigma3(_sigma3), sigma4(_sigma4), degrid(_degrid),
dehalo(_dehalo), hr(_hr), ht(_ht), ncpu(_ncpu),
vi(_vi), node(_node), wsharpen(nullptr, nullptr), wdehalo(nullptr, nullptr),
pattern2d(nullptr, nullptr), pattern3d(nullptr, nullptr), in(nullptr, nullptr),
gridsample(nullptr, nullptr), outLast(nullptr, nullptr), covar(nullptr, nullptr),
covarProcess(nullptr, nullptr), outrez(nullptr, nullptr), plan(nullptr, nullptr), planinv(nullptr, nullptr),
plan1(nullptr, nullptr) {
    int istat = fftwf_init_threads();
    if (istat == 0)
        throw std::runtime_error{ "fftwf_init_threads() failed!" };

#ifdef HAVE_FFTW3_MAKE_PLANNER_THREAD_SAFE
    fftwf_make_planner_thread_safe();
#endif

    int i, j;

    if (ow < 0) ow = bw / 3; /* changed from bw/4 to bw/3 in v.1.2 */
    if (oh < 0) oh = bh / 3; /* changed from bh/4 to bh/3 in v.1.2 */

    maxval = (1 << vi.format->bitsPerSample) - 1;

    planeBase = (plane && vi.format->sampleType == stInteger) ? (1 << (vi.format->bitsPerSample - 1)) : 0;

    nox = ((vi.width >> (plane ? vi.format->subSamplingW : 0)) - ow + (bw - ow - 1)) / (bw - ow);
    noy = ((vi.height >> (plane ? vi.format->subSamplingH : 0)) - oh + (bh - oh - 1)) / (bh - oh);

    /* padding by 1 block per side */
    nox += 2;
    noy += 2;
    mirw = bw - ow; /* set mirror size as block interval */
    mirh = bh - oh;

    coverwidth = nox * (bw - ow) + ow;
    coverheight = noy * (bh - oh) + oh;
    coverpitch = ((coverwidth + 7) / 8) * 8 * vi.format->bytesPerSample;
    coverbuf = std::unique_ptr<uint8_t[]>(new uint8_t[coverheight * coverpitch]);

    int insize = bw * bh * nox * noy;
    in = std::unique_ptr<float[], decltype(&fftw_free)>(fftwf_alloc_real(insize), fftwf_free);
    outwidth = bw / 2 + 1;                  /* width (pitch) of complex fft block */
    outpitchelems = ((outwidth + 1) / 2) * 2;    /* must be even for SSE - v1.7 */
    outpitch = outpitchelems * vi.format->bytesPerSample;

    outsize = outpitchelems * bh * nox * noy;   /* replace outwidth to outpitchelems here and below in v1.7 */

    if (bt == 0) /* Kalman */
    { 
        outLast = std::unique_ptr<fftwf_complex[], decltype(&fftw_free)>(fftwf_alloc_complex(outsize), fftwf_free);
        covar = std::unique_ptr<fftwf_complex[], decltype(&fftw_free)>(fftwf_alloc_complex(outsize), fftwf_free);
        covarProcess = std::unique_ptr<fftwf_complex[], decltype(&fftw_free)>(fftwf_alloc_complex(outsize), fftwf_free);
    }
    outrez = std::unique_ptr<fftwf_complex[], decltype(&fftw_free)>(fftwf_alloc_complex(outsize), fftwf_free);
    gridsample = std::unique_ptr<fftwf_complex[], decltype(&fftw_free)>(fftwf_alloc_complex(outsize), fftwf_free);

    fftcache.Initialize(bt + 2 + 4, outsize);

    int planFlags = measure ? FFTW_MEASURE: FFTW_ESTIMATE;
    int rank = 2; /* 2d */
    ndim[0] = bh; /* size of block along height */
    ndim[1] = bw; /* size of block along width */
    int istride = 1;
    int ostride = 1;
    int idist   = bw * bh;
    int odist   = outpitchelems * bh;/*  v1.7 (was outwidth) */
    inembed[0] = bh;
    inembed[1] = bw;
    onembed[0] = bh;
    onembed[1] = outpitchelems;      /*  v1.7 (was outwidth) */
    howmanyblocks = nox * noy;

    fftwf_plan_with_nthreads( ncpu );

    plan = std::unique_ptr<fftwf_plan_s, decltype(&fftwf_destroy_plan)>(fftwf_plan_many_dft_r2c(rank, ndim, howmanyblocks,
        in.get(), inembed, istride, idist, outrez.get(), onembed, ostride, odist, planFlags), fftwf_destroy_plan);
    if( !plan )
        throw std::runtime_error{ "fftwf_plan_many_dft_r2c" };

    planinv = std::unique_ptr<fftwf_plan_s, decltype(&fftwf_destroy_plan)>(fftwf_plan_many_dft_c2r( rank, ndim, howmanyblocks,
                                       outrez.get(), onembed, ostride, odist, in.get(), inembed, istride, idist, planFlags), fftwf_destroy_plan);
    if( !planinv )
        throw std::runtime_error{ "fftwf_plan_many_dft_c2r" };

    fftwf_plan_with_nthreads( 1 );

    wanxl = std::unique_ptr<float[]>(new float[ow]);
    wanxr = std::unique_ptr<float[]>(new float[ow]);
    wanyl = std::unique_ptr<float[]>(new float[oh]);
    wanyr = std::unique_ptr<float[]>(new float[oh]);

    wsynxl = std::unique_ptr<float[]>(new float[ow]);
    wsynxr = std::unique_ptr<float[]>(new float[ow]);
    wsynyl = std::unique_ptr<float[]>(new float[oh]);
    wsynyr = std::unique_ptr<float[]>(new float[oh]);

    wsharpen = std::unique_ptr<float[], decltype(&fftw_free)>(fftwf_alloc_real(bh * outpitchelems), fftwf_free);
    wdehalo  = std::unique_ptr<float[], decltype(&fftw_free)>(fftwf_alloc_real(bh * outpitchelems), fftwf_free);

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
            wsharpen[i + j * outpitchelems] = 1 - exp( -d2 / (2 * scutoff * scutoff) );
        }
    }

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
            wdehalo[i + j * outpitchelems] = exp( -0.7f * d2 * hr * hr ) - exp( -d2 * hr * hr ); /* some window with max around 1/hr, small at low and high frequencies */
            if( wdehalo[i + j * outpitchelems] > wmax )
                wmax = wdehalo[i]; /* for normalization */
        }
    }

    for( j = 0; j < bh; j++ )
    {
        for( i = 0; i < outwidth; i++ )
        {
            wdehalo[i + j * outpitchelems] /= wmax; /* normalize */
        }
    }

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
        fill_complex( outLast.get(),      outsize, 0, 0 );
        fill_complex( covar.get(),        outsize, sigmaSquaredNoiseNormed2D, sigmaSquaredNoiseNormed2D );
        fill_complex( covarProcess.get(), outsize, sigmaSquaredNoiseNormed2D, sigmaSquaredNoiseNormed2D );
    }

    mean = std::unique_ptr<float[]>(new float[nox * noy]); 
    pwin = std::unique_ptr<float[]>(new float[bh * outpitchelems]); /* pattern window array */

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
            pwin[i + j * outpitchelems] = (fh2 + fw2) / (fh2 + fw2 + pcutoff * pcutoff);
        }
    }

    pattern2d = std::unique_ptr<float[], decltype(&fftw_free)>(fftwf_alloc_real(bh * outpitchelems), fftwf_free); /* noise pattern window array */
    pattern3d = std::unique_ptr<float[], decltype(&fftw_free)>(fftwf_alloc_real(bh * outpitchelems), fftwf_free); /* noise pattern window array */

    if( (sigma2 != sigma || sigma3 != sigma || sigma4 != sigma) && pfactor == 0 )
    {   /* we have different sigmas, so create pattern from sigmas */
        SigmasToPattern( sigma, sigma2, sigma3, sigma4, bh, outwidth, outpitchelems, norm, pattern2d.get());
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
    plan1 = std::unique_ptr<fftwf_plan_s, decltype(&fftwf_destroy_plan)>(fftwf_plan_many_dft_r2c( rank, ndim, 1,
                                     in.get(), inembed, istride, idist, outrez.get(), onembed, ostride, odist, planFlags), fftwf_destroy_plan); /* 1 block */

    if (vi.format->bytesPerSample == 1) {
        memset(coverbuf.get(), 255, coverheight * coverpitch);
        InitOverlapPlane(in.get(), coverbuf.get(), coverpitch, 0);
    } else if (vi.format->bytesPerSample == 2) {
        fft3d_memset(reinterpret_cast<uint16_t *>(coverbuf.get()), static_cast<uint16_t>(maxval), coverheight * coverpitch / 2);
        InitOverlapPlane(in.get(), reinterpret_cast<uint16_t *>(coverbuf.get()), coverpitch, 0);
    } else if (vi.format->bytesPerSample == 4) {
        fft3d_memset(reinterpret_cast<float *>(coverbuf.get()), 1.f, coverheight * coverpitch / 4);
        InitOverlapPlane(in.get(), reinterpret_cast<float *>(coverbuf.get()), coverpitch, 0);
    }
    /* make FFT 2D */
    fftwf_execute_dft_r2c( plan1.get(), in.get(), gridsample.get() );
}
//-----------------------------------------------------------------------
//
template<typename T>
static void FramePlaneToCoverbuf( int plane, const VSFrameRef *src, T * __restrict coverbuf, int coverwidth, int coverheight, int coverpitch, int mirw, int mirh, bool interlaced, const VSAPI *vsapi )
{
    const T * __restrict srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
    int            src_height = vsapi->getFrameHeight(src, plane);
    int            src_width = vsapi->getFrameWidth(src, plane);
    int            src_pitch = vsapi->getStride(src, plane) / sizeof(T);
    coverpitch /= sizeof(T);

    int h, w;
    int width2 = src_width + src_width + mirw + mirw - 2;
    T * __restrict coverbuf1 = coverbuf + coverpitch * mirh;

    if( !interlaced ) /* progressive */
    {
        for( h = mirh; h < src_height + mirh; h++ )
        {
            for( w = 0; w < mirw; w++ )
            {
                coverbuf1[w] = coverbuf1[mirw + mirw - w]; /* mirror left border */
            }
            memcpy(coverbuf1 + mirw, srcp, src_width * sizeof(T)); /* copy line */
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
            for( w = 0; w < mirw; w++ )
            {
                coverbuf1[w] = coverbuf1[mirw + mirw - w]; /* mirror left border */
            }
            memcpy(coverbuf1 + mirw, srcp, src_width * sizeof(T)); /* copy line */
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
            for( w = 0; w < mirw; w++ )
            {
                coverbuf1[w] = coverbuf1[mirw + mirw - w]; /* mirror left border */
            }
            memcpy(coverbuf1 + mirw, srcp, src_width * sizeof(T)); /* copy line */
            for( w = src_width + mirw; w < coverwidth; w++ )
            {
                coverbuf1[w] = coverbuf1[width2 - w]; /* mirror right border */
            }
            coverbuf1 += coverpitch;
            srcp      -= src_pitch * 2;
        }
    }

    T *pmirror = coverbuf1 - coverpitch * 2; /* pointer to vertical mirror */
    for( h = src_height + mirh; h < coverheight; h++ )
    {
        memcpy( coverbuf1, pmirror, coverwidth * sizeof(T)); /* mirror bottom line by line */
        coverbuf1 += coverpitch;
        pmirror   -= coverpitch;
    }
    coverbuf1 = coverbuf;
    pmirror   = coverbuf1 + coverpitch * mirh * 2; /* pointer to vertical mirror */
    for( h = 0; h < mirh; h++ )
    {
        memcpy( coverbuf1, pmirror, coverwidth * sizeof(T)); /* mirror bottom line by line */
        coverbuf1 += coverpitch;
        pmirror   -= coverpitch;
    }
}
//-----------------------------------------------------------------------
//
template<typename T>
static void CoverbufToFramePlane(int plane, const T * __restrict coverbuf, int coverwidth, int coverheight, int coverpitch, VSFrameRef *dst, int mirw, int mirh, bool interlaced, const VSAPI *vsapi )
{
    T *__restrict dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
    int      dst_height = vsapi->getFrameHeight(dst, plane);
    int      dst_width = vsapi->getFrameWidth(dst, plane);
    int      dst_pitch = vsapi->getStride(dst, plane) / sizeof(T);
    coverpitch /= sizeof(T);

    const T * __restrict coverbuf1 = coverbuf + coverpitch * mirh + mirw;
    if( !interlaced ) /* progressive */
    {
        for( int h = 0; h < dst_height; h++ )
        {
            memcpy( dstp, coverbuf1, dst_width * sizeof(T) ); /* copy pure frame size only */
            dstp      += dst_pitch;
            coverbuf1 += coverpitch;
        }
    }
    else /* interlaced */
    {
        for( int h = 0; h < dst_height; h += 2 )
        {
            memcpy( dstp, coverbuf1, dst_width * sizeof(T)); /* copy pure frame size only */
            dstp      += dst_pitch * 2;
            coverbuf1 += coverpitch;
        }
        /* second field is flipped */
        dstp -= dst_pitch;
        for( int h = 0; h < dst_height; h += 2 )
        {
            memcpy( dstp, coverbuf1, dst_width * sizeof(T)); /* copy pure frame size only */
            dstp      -= dst_pitch * 2;
            coverbuf1 += coverpitch;
        }
    }
}
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
/* put source bytes to float array of overlapped blocks
 * use analysis windows */
template<typename T>
void FFT3DFilter::InitOverlapPlane( float * __restrict inp0, const T * __restrict srcp0, int src_pitch, int planeBase )
{
    int w, h;
    int ihx, ihy;
    const T * __restrict srcp = srcp0;
    float ftmp;
    int xoffset = bh * bw - (bw - ow); /* skip frames */
    int yoffset = bw * nox * bh - bw * (bh - oh); /* vertical offset of same block (overlap) */
    src_pitch /= sizeof(T);

    float * __restrict inp = inp0;

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
template<typename T>
void FFT3DFilter::DecodeOverlapPlane( const float * __restrict inp0, float norm, T * __restrict dstp0, int dst_pitch, int planeBase, int maxval )
{
    int w, h;
    int ihx, ihy;
    T * __restrict dstp = dstp0;
    const float * __restrict inp = inp0;
    int xoffset = bh * bw - (bw - ow);
    int yoffset = bw * nox * bh - bw * (bh - oh); /* vertical offset of same block (overlap) */
    dst_pitch /= sizeof(T);

    ihy = 0; /* first top big non-overlapped) part */
    {
        for( h = 0; h < bh - oh; h++ )
        {
            inp = inp0 + h * bw;
            for( w = 0; w < bw - ow; w++ )   /* first half line of first block */
            {   
                if /*constexpr*/ (std::is_integral<T>::value)
                    dstp[w] = std::min(maxval, std::max( 0, (int)(inp[w] * norm) + planeBase ) ); /* Copy each byte from float array to dest with windows */
                else 
                    dstp[w] = inp[w] * norm;
            }
            inp  += bw - ow;
            dstp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle horizontal half-blocks */
            {
                for( w = 0; w < ow; w++ )   /* half line of block */
                {
                    if /*constexpr*/ (std::is_integral<T>::value)
                        dstp[w] = std::min(maxval, std::max( 0, (int)((inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * norm) + planeBase ) );  /* overlapped Copy */
                    else
                        dstp[w] = (inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * norm;
                }
                inp  += xoffset + ow;
                dstp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* first half line of first block */
                {
                    if /*constexpr*/ (std::is_integral<T>::value)
                        dstp[w] = std::min(maxval, std::max( 0, (int)(inp[w] * norm) + planeBase ) );   /* Copy each byte from float array to dest with windows */
                    else
                        dstp[w] = inp[w] * norm;
                }
                inp  += bw - ow - ow;
                dstp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                if /*constexpr*/ (std::is_integral<T>::value)
                    dstp[w] = std::min(maxval, std::max( 0,(int)(inp[w] * norm) + planeBase ) );
                else
                    dstp[w] = inp[w] * norm;
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
                if /*constexpr*/ (std::is_integral<T>::value)
                    dstp[w] = std::min(maxval, std::max( 0, (int)((inp[w] * wsynyrh + inp[w + yoffset] * wsynylh)) + planeBase ) );   /* y overlapped */
                else
                    dstp[w] = inp[w] * wsynyrh + inp[w + yoffset] * wsynylh;
            }
            inp  += bw - ow;
            dstp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half overlapped line of block */
                {
                    if /*constexpr*/ (std::is_integral<T>::value)
                        dstp[w] = std::min(maxval, std::max( 0, (int)(((inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * wsynyrh
                            + (inp[w + yoffset] * wsynxr[w] + inp[w + xoffset + yoffset] * wsynxl[w]) * wsynylh)) + planeBase ) );   /* x overlapped */
                    else
                        dstp[w] = (inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * wsynyrh
                            + (inp[w + yoffset] * wsynxr[w] + inp[w + xoffset + yoffset] * wsynxl[w]) * wsynylh;
                }
                inp  += xoffset + ow;
                dstp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* double minus - half non-overlapped line of block */
                {
                    if /*constexpr*/ (std::is_integral<T>::value)
                        dstp[w] = std::min(maxval, std::max( 0, (int)((inp[w] * wsynyrh + inp[w + yoffset] * wsynylh )) + planeBase ) );
                    else
                        dstp[w] = inp[w] * wsynyrh + inp[w + yoffset] * wsynylh;
                }
                inp  += bw - ow - ow;
                dstp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                if /*constexpr*/ (std::is_integral<T>::value)
                    dstp[w] = std::min(maxval, std::max( 0, (int)((inp[w] * wsynyrh + inp[w + yoffset] * wsynylh)) + planeBase ) );
                else
                    dstp[w] = inp[w] * wsynyrh + inp[w + yoffset] * wsynylh;
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
                if /*constexpr*/ (std::is_integral<T>::value)
                    dstp[w] = std::min(maxval, std::max( 0, (int)(inp[w] * norm) + planeBase ) );
                else
                    dstp[w] = inp[w] * norm;
            }
            inp  += bw - ow;
            dstp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half overlapped line of block */
                {
                    if /*constexpr*/ (std::is_integral<T>::value)
                        dstp[w] = std::min(maxval, std::max( 0, (int)((inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w] ) * norm) + planeBase ) );   /* x overlapped */
                    else
                        dstp[w] = (inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * norm;
                }
                inp  += xoffset + ow;
                dstp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* half non-overlapped line of block */
                {
                    if /*constexpr*/ (std::is_integral<T>::value)
                        dstp[w] = std::min(maxval, std::max( 0, (int)(inp[w] * norm) + planeBase ) );
                    else
                        dstp[w] = inp[w] * norm;
                }
                inp  += bw - ow - ow;
                dstp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                if /*constexpr*/ (std::is_integral<T>::value)
                    dstp[w] = std::min(maxval, std::max( 0, (int)(inp[w] * norm) + planeBase ) );
                else
                    dstp[w] = inp[w] * norm;
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
                if /*constexpr*/ (std::is_integral<T>::value)
                    dstp[w] = std::min(maxval, std::max( 0, (int)(inp[w] * norm) + planeBase ) );
                else
                    dstp[w] = inp[w] * norm;
            }
            inp  += bw - ow;
            dstp += bw - ow;
            for( ihx = 1; ihx < nox; ihx++ ) /* middle blocks */
            {
                for( w = 0; w < ow; w++ )   /* half line of block */
                {
                    if /*constexpr*/ (std::is_integral<T>::value)
                        dstp[w] = std::min(maxval, std::max( 0, (int)((inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * norm) + planeBase ) );  /* overlapped Copy */
                    else
                        dstp[w] = (inp[w] * wsynxr[w] + inp[w + xoffset] * wsynxl[w]) * norm;
                }
                inp  += xoffset + ow;
                dstp += ow;
                for( w = 0; w < bw - ow - ow; w++ )   /* half line of block */
                {
                    if /*constexpr*/ (std::is_integral<T>::value)
                        dstp[w] = std::min(maxval, std::max( 0, (int)(inp[w] * norm) + planeBase ) );
                    else
                        dstp[w] = inp[w] * norm;
                }
                inp  += bw - ow - ow;
                dstp += bw - ow - ow;
            }
            for( w = 0; w < ow; w++ )   /* last half line of last block */
            {
                if /*constexpr*/ (std::is_integral<T>::value)
                    dstp[w] = std::min(maxval, std::max( 0, (int)(inp[w] * norm) + planeBase ) );
                else
                    dstp[w] = inp[w] * norm;
            }
            inp  += ow;
            dstp += ow;

            dstp += (dst_pitch - coverwidth);  /* Add the pitch of one line (in bytes) to the source image. */
        }
    }
}
//-------------------------------------------------------------------------------------------
static void FindPatternBlock( const fftwf_complex *outcur0, int outwidth, int outpitchelems, int bh, int nox, int noy, int &px, int &py, const float *pwin, float degrid, const fftwf_complex *gridsample )
{
    /* since v1.7 outwidth must be really an outpitchelems */
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
            outcur = outcur0 + nox * by * bh * outpitchelems + bx * bh * outpitchelems;
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
                outcur     += outpitchelems;
                pwin       += outpitchelems;
                gridsample += outpitchelems;
            }
            pwin -= outpitchelems * bh; /* restore */
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
static void SetPattern( const fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int nox, int noy, int px, int py, const float *pwin, float *pattern2d, float &psigma, float degrid, const fftwf_complex *gridsample )
{
    int h;
    int w;
    outcur += nox * py * bh * outpitchelems + px * bh * outpitchelems;
    float psd;
    float sigmaSquared = 0;
    float weight = 0;

    for( h = 0; h < bh; h++ )
    {
        for( w = 0; w < outwidth; w++ )
        {
            weight += pwin[w];
        }
        pwin += outpitchelems;
    }
    pwin -= outpitchelems * bh; /* restore */

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
        outcur     += outpitchelems;
        pattern2d  += outpitchelems;
        pwin       += outpitchelems;
        gridsample += outpitchelems;
    }
    psigma = sqrt( sigmaSquared / (weight * bh * outwidth) ); /* mean std deviation (sigma) */
}
//-------------------------------------------------------------------------------------------
static void PutPatternOnly( fftwf_complex *outcur, int outwidth, int outpitchelems, int bh, int nox, int noy, int px, int py )
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
            outcur += outpitchelems;
        }
    }

    outcur += bh * outpitchelems;

    for( block = pblock + 1; block < blocks; block++ )
    {
        for( h = 0; h < bh; h++ )
        {
            for( w = 0; w < outwidth; w++ )
            {
                outcur[w][0] = 0;
                outcur[w][1] = 0;
            }
            outcur += outpitchelems;
        }
    }
}
//-------------------------------------------------------------------------------------------
static void Pattern2Dto3D( const float *pattern2d, int bh, int outwidth, int outpitchelems, float mult, float *pattern3d )
{
    /* slow, but executed once only per clip */
    int size = bh * outpitchelems;
    for( int i = 0; i < size; i++ )
    { /* get 3D pattern */
        pattern3d[i] = pattern2d[i] * mult;
    }
}
//-------------------------------------------------------------------------------------------
template < typename T, int btcur >
void FFT3DFilter::Wiener3D
(
    int               n,
    const VSFrameRef *src,
    VSFrameContext   *frame_ctx,
    const VSAPI      *vsapi
)
{
    int fromframe = n - btcur / 2;
    int toframe = n + (btcur - 1) / 2;
    int outcenter = btcur / 2;
    fftwf_complex *frames[btcur] = {};
    bool valid[btcur];

    fftcache.GetCachedFrames(fromframe, toframe, frames, valid);

    for (int i = 0; i <= toframe - fromframe; i++) {
        if (!valid[i]) {
            if (i + fromframe != n) {
                const VSFrameRef *frame = vsapi->getFrameFilter(i + fromframe, node, frame_ctx);
                FramePlaneToCoverbuf(plane, frame, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi);
                vsapi->freeFrame(frame);
            } else {
                FramePlaneToCoverbuf(plane, src, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi);
            }
            InitOverlapPlane(in.get(), reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase);
            /* make FFT 2D */
            fftwf_execute_dft_r2c(plan.get(), in.get(), frames[i]);
        }
        if (i + fromframe == n - btcur / 2) {
            // oldest frame will be used as scratch space so use a copy of it instead
            // originally a complicated scheme that assumed perfectly linear access was used to actually consume the cached frame but that's too complicated,
            // and possibly not that great when out of order access can happen due to multithreading
            memcpy(outrez.get(),
                frames[i],
                outsize * sizeof(fftwf_complex));
        }
    }

    // unwrap the frames to outp again, because this step was in the original code and rewriting things nicer is effort
    // also clamp the index unlike the original so no reads happen beyond the array bounds...
    fftwf_complex *outp[5] = { frames[std::max(0, outcenter - 2)], frames[std::max(0, outcenter - 1)], frames[outcenter], frames[std::min(outcenter + 1, btcur - 1)], frames[std::min(outcenter + 2, btcur - 1)] };
    outp[2 - btcur / 2] = outrez.get();

    if( degrid != 0 )
    {
        if( pfactor != 0 )
            ApplyPattern3D_degrid< btcur >( outp[2], outp[0], outp[1], outp[3], outp[4], outwidth, outpitchelems, bh, howmanyblocks, pattern3d.get(), beta, degrid, gridsample.get());
        else
            ApplyWiener3D_degrid< btcur >( outp[2], outp[0], outp[1], outp[3], outp[4], outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, degrid, gridsample.get());
        Sharpen_degrid( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), degrid, gridsample.get(), dehalo, wdehalo.get(), ht2n );
    }
    else
    {
        if( pfactor != 0 )
            ApplyPattern3D< btcur >( outp[2], outp[0], outp[1], outp[3], outp[4], outwidth, outpitchelems, bh, howmanyblocks, pattern3d.get(), beta );
        else
            ApplyWiener3D< btcur >( outp[2], outp[0], outp[1], outp[3], outp[4], outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta );
        Sharpen( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), dehalo, wdehalo.get(), ht2n );
    }
    /* do inverse FFT 2D, get filtered 'in' array
     * note: input "outrez" array is destroyed by execute algo. */
    fftwf_execute_dft_c2r( planinv.get(), outrez.get(), in.get());
}

template<typename T>
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
    if( pfactor != 0 && isPatternSet == false && pshow == false ) /* get noise pattern */
    {
        const VSFrameRef *psrc = vsapi->getFrameFilter( pframe, node, frame_ctx ); /* get noise pattern frame */

        /* put source bytes to float array of overlapped blocks */
        FramePlaneToCoverbuf( plane, psrc, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        vsapi->freeFrame( psrc );
        InitOverlapPlane( in.get(), reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan.get(), in.get(), outrez.get());
        if( px == 0 && py == 0 ) /* try find pattern block with minimal noise sigma */
            FindPatternBlock( outrez.get(), outwidth, outpitchelems, bh, nox, noy, px, py, pwin.get(), degrid, gridsample.get());
        SetPattern( outrez.get(), outwidth, outpitchelems, bh, nox, noy, px, py, pwin.get(), pattern2d.get(), psigma, degrid, gridsample.get());
        isPatternSet = true;
    }
    else if( pfactor != 0 && pshow == true )
    {   /* show noise pattern window */
        /* put source bytes to float array of overlapped blocks */
        FramePlaneToCoverbuf( plane, src, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        InitOverlapPlane( in.get(), reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan.get(), in.get(), outrez.get());

        int pxf, pyf;
        if( px == 0 && py == 0 ) /* try find pattern block with minimal noise sigma */
            FindPatternBlock( outrez.get(), outwidth, outpitchelems, bh, nox, noy, pxf, pyf, pwin.get(), degrid, gridsample.get() );
        else
        {
            pxf = px; /* fixed bug in v1.6 */
            pyf = py;
        }
        SetPattern( outrez.get(), outwidth, outpitchelems, bh, nox, noy, pxf, pyf, pwin.get(), pattern2d.get(), psigma, degrid, gridsample.get());

        /* change analysis and synthesis window to constant to show */
        for( int i = 0; i < ow; i++ )
        {
            wanxl[i] = 1;    wanxr[i] = 1;    wsynxl[i] = 1;    wsynxr[i] = 1;
        }
        for( int i = 0; i < oh; i++ )
        {
            wanyl[i] = 1;    wanyr[i] = 1;    wsynyl[i] = 1;    wsynyr[i] = 1;
        }

        //FIXME, why is planebase assigned here? originally always assigned 128
        planeBase = (vi.format->sampleType == stInteger) ? (1 << (vi.format->bitsPerSample - 1)) : 0;

        /* put source bytes to float array of overlapped blocks */
        /* cur frame */
        FramePlaneToCoverbuf( plane, src, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        InitOverlapPlane( in.get(), reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan.get(), in.get(), outrez.get());

        PutPatternOnly( outrez.get(), outwidth, outpitchelems, bh, nox, noy, pxf, pyf );
        /* do inverse 2D FFT, get filtered 'in' array */
        fftwf_execute_dft_c2r( planinv.get(), outrez.get(), in.get());

        /* make destination frame plane from current overlaped blocks */
        DecodeOverlapPlane( in.get(), norm, reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase, maxval );
        CoverbufToFramePlane( plane, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, dst, mirw, mirh, interlaced, vsapi );
        vsapi->propSetData(vsapi->getFramePropsRW(dst), "FFT3DFilterPShowSigma", std::to_string(psigma).c_str(), -1, paAppend);
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
            Pattern2Dto3D( pattern2d.get(), bh, outwidth, outpitchelems, (float)btcur, pattern3d.get());

        /* get power spectral density (abs quadrat) for every block and apply filter */

        /* put source bytes to float array of overlapped blocks */

        if( btcur == 1 ) /* 2D */
        {
            /* cur frame */
            FramePlaneToCoverbuf( plane, src, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
            InitOverlapPlane( in.get(), reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase );

            /* make FFT 2D */
            fftwf_execute_dft_r2c( plan.get(), in.get(), outrez.get());
            if( degrid != 0 )
            {
                if( pfactor != 0 )
                {
                    ApplyPattern2D_degrid_C( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, pfactor, pattern2d.get(), beta, degrid, gridsample.get());
                    Sharpen_degrid( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), degrid, gridsample.get(), dehalo, wdehalo.get(), ht2n );
                }
                else
                    ApplyWiener2D_degrid_C( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), degrid, gridsample.get(), dehalo, wdehalo.get(), ht2n );
            }
            else
            {
                if( pfactor != 0 )
                {
                    ApplyPattern2D( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, pfactor, pattern2d.get(), beta );
                    Sharpen( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), dehalo, wdehalo.get(), ht2n );
                }
                else
                    ApplyWiener2D( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed, beta, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), dehalo, wdehalo.get(), ht2n );
            }

            /* do inverse FFT 2D, get filtered 'in' array */
            fftwf_execute_dft_c2r( planinv.get(), outrez.get(), in.get());
        }
        else if( btcur == 2 ) /* 3D2 */
        {
            Wiener3D< T, 2 >( n, src, frame_ctx, vsapi );
        }
        else if( btcur == 3 ) /* 3D3 */
        {
            Wiener3D< T, 3 >( n, src, frame_ctx, vsapi );
        }
        else if( btcur == 4 ) /* 3D4 */
        {
            Wiener3D< T, 4 >( n, src, frame_ctx, vsapi );
        }
        else if( btcur == 5 ) /* 3D5 */
        {
            Wiener3D< T, 5 >( n, src, frame_ctx, vsapi );
        }
        /* make destination frame plane from current overlaped blocks */
        DecodeOverlapPlane( in.get(), norm, reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase, maxval);
        CoverbufToFramePlane( plane, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, dst, mirw, mirh, interlaced, vsapi );
    }
    else if( bt == 0 ) /* Kalman filter */
    {
        /* get power spectral density (abs quadrat) for every block and apply filter */

        if( n == 0 )
            return;

        /* put source bytes to float array of overlapped blocks */
        /* cur frame */
        FramePlaneToCoverbuf( plane, src, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        InitOverlapPlane( in.get(), reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan.get(), in.get(), outrez.get());
        if( pfactor != 0 )
            ApplyKalmanPattern( outrez.get(), outLast.get(), covar.get(), covarProcess.get(), outwidth, outpitchelems, bh, howmanyblocks, pattern2d.get(), kratio * kratio );
        else
            ApplyKalman( outrez.get(), outLast.get(), covar.get(), covarProcess.get(), outwidth, outpitchelems, bh, howmanyblocks, sigmaSquaredNoiseNormed2D, kratio * kratio );

        /* copy outLast to outrez */
        memcpy( outrez.get(),
                outLast.get(),
                outsize * sizeof(fftwf_complex) );  /* v.0.9.2 */
        if( degrid != 0 )
            Sharpen_degrid( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), degrid, gridsample.get(), dehalo, wdehalo.get(), ht2n );
        else
            Sharpen( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), dehalo, wdehalo.get(), ht2n );
        /* do inverse FFT 2D, get filtered 'in' array
         * note: input "out" array is destroyed by execute algo.
         * that is why we must have its copy in "outLast" array */
        fftwf_execute_dft_c2r( planinv.get(), outrez.get(), in.get());
        /* make destination frame plane from current overlaped blocks */
        DecodeOverlapPlane( in.get(), norm, reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase, maxval);
        CoverbufToFramePlane( plane, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, dst, mirw, mirh, interlaced, vsapi );
    }
    else if( bt == -1 ) /* sharpen only */
    {
        /* put source bytes to float array of overlapped blocks */
        FramePlaneToCoverbuf( plane, src, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, mirw, mirh, interlaced, vsapi );
        InitOverlapPlane( in.get(), reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase );
        /* make FFT 2D */
        fftwf_execute_dft_r2c( plan.get(), in.get(), outrez.get());
        if( degrid != 0 )
            Sharpen_degrid( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), degrid, gridsample.get(), dehalo, wdehalo.get(), ht2n );
        else
            Sharpen( outrez.get(), outwidth, outpitchelems, bh, howmanyblocks, sharpen, sigmaSquaredSharpenMinNormed, sigmaSquaredSharpenMaxNormed, wsharpen.get(), dehalo, wdehalo.get(), ht2n );
        /* do inverse FFT 2D, get filtered 'in' array */
        fftwf_execute_dft_c2r( planinv.get(), outrez.get(), in.get());
        /* make destination frame plane from current overlaped blocks */
        DecodeOverlapPlane( in.get(), norm, reinterpret_cast<T *>(coverbuf.get()), coverpitch, planeBase, maxval);
        CoverbufToFramePlane( plane, reinterpret_cast<T *>(coverbuf.get()), coverwidth, coverheight, coverpitch, dst, mirw, mirh, interlaced, vsapi );
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
    float _sigma, float _beta, bool _process[3], int _bw, int _bh, int _bt, int _ow, int _oh,
    float _kratio, float _sharpen, float _scutoff, float _svr, float _smin, float _smax,
    bool _measure, bool _interlaced, int _wintype,
    int _pframe, int _px, int _py, bool _pshow, float _pcutoff, float _pfactor,
    float _sigma2, float _sigma3, float _sigma4, float _degrid,
    float _dehalo, float _hr, float _ht, int _ncpu,
    const VSMap *in, const VSAPI *vsapi
) : Clips(),
    bt( _bt ), pframe( _pframe ), pshow( _pshow ), pfactor( _pfactor )
{
    node =  vsapi->propGetNode( in, "clip", 0, 0 );
    vi   = *vsapi->getVideoInfo( node );

    try {
        if ((vi.format->bitsPerSample > 16 && vi.format->sampleType == stInteger) || (vi.format->bitsPerSample != 32 && vi.format->sampleType == stFloat))
            throw std::runtime_error{ "only 8-16 bit integer and 32 bit float are supported" };

        for (int i = 0; i < vi.format->numPlanes; i++) {
            if (_process[i])
                Clips[i] = new FFT3DFilter(_sigma, _beta, i, _bw, _bh, _bt, _ow, _oh,
                    _kratio, _sharpen, _scutoff, _svr, _smin, _smax,
                    _measure, _interlaced, _wintype,
                    _pframe, _px, _py, _pshow, _pcutoff, _pfactor,
                    _sigma2, _sigma3, _sigma4, _degrid, _dehalo, _hr, _ht, _ncpu,
                    vi, node);
        }

        for (int i = 2; i >= 0; i--) {
            if (Clips[i]) {
                isPatternSet = Clips[i]->getIsPatternSet();
                break;
            }
        }
    } catch (std::runtime_error &) {
        Free(vsapi);
        throw;
    }
}

void FFT3DFilterMulti::Free(const VSAPI *vsapi) {
    for (int i = 0; i < 3; i++)
        delete Clips[i];
    vsapi->freeNode(node);
    delete this;
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

    VSFrameRef *dst = nullptr;
    if( pfactor != 0 && pshow == true )
        dst = vsapi->copyFrame( src, core );
    else if( bt == 0 && n == 0 )
        /* Kalman filter does nothing for the first frame. */
        dst = vsapi->copyFrame(src, core);
    else
    {
        int planes[3] = { 0, 1, 2 };
        const VSFrameRef *srcf[3] = { Clips[0] ? nullptr : src, Clips[1] ? nullptr : src, Clips[2] ? nullptr : src };
        dst = vsapi->newVideoFrame2(vsapi->getFrameFormat(src), vsapi->getFrameWidth(src, 0), vsapi->getFrameHeight(src, 0), srcf, planes, src, core);
    }
    
    for (int i = 0; i < 3; i++) {
        if (Clips[i]) {
            if (vi.format->bytesPerSample == 1)
                Clips[i]->ApplyFilter<uint8_t>(n, dst, src, frame_ctx, core, vsapi);
            else if (vi.format->bytesPerSample == 2)
                Clips[i]->ApplyFilter<uint16_t>(n, dst, src, frame_ctx, core, vsapi);
            else if (vi.format->bytesPerSample == 4)
                Clips[i]->ApplyFilter<float>(n, dst, src, frame_ctx, core, vsapi);
        }
    }

    for (int i = 2; i >= 0; i--) {
        if (Clips[i]) {
            isPatternSet = Clips[i]->getIsPatternSet();
            break;
        }
    }

    vsapi->freeFrame( src );
    return dst;
}
