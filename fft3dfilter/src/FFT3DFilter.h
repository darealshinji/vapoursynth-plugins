/*****************************************************************************
 * FFT3DFilter.h
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
 *****************************************************************************/

#include <string>

#include <fftw3.h>

#include "VapourSynth.h"

class CustomException
{
private:
    const std::string name;
public:
    CustomException() : name( std::string() ) {}
    CustomException( const std::string name ) : name( name ) {}
    const char * what() const noexcept { return name.c_str(); }
};

class FFT3DFilter
{
private:
    /* parameters */
    float sigma;    /* noise level (std deviation) for high frequncies */
    float beta;     /* relative noise margin for Wiener filter */
    int   plane;    /* color plane */
    int   bw;       /* block width */
    int   bh;       /* block height */
    int   bt;       /* block size  along time (mumber of frames), =0 for Kalman, >0 for Wiener */
    int   ow;       /* overlap width - v.0.9 */
    int   oh;       /* overlap height - v.0.9 */
    float kratio;   /* threshold to sigma ratio for Kalman filter */
    float sharpen;  /* sharpen factor (0 to 1 and above) */
    float scutoff;  /* sharpen cufoff frequency (relative to max) - v1.7 */
    float svr;      /* sharpen vertical ratio (0 to 1 and above) - v.1.0 */
    float smin;     /* minimum limit for sharpen (prevent noise amplifying) - v.1.1 */
    float smax;     /* maximum limit for sharpen (prevent oversharping) - v.1.1 */
    bool  measure;  /* fft optimal method */
    bool  interlaced;
    int   wintype;  /* window type */
    int   pframe;   /* noise pattern frame number */
    int   px;       /* noise pattern window x-position */
    int   py;       /* noise pattern window y-position */
    bool  pshow;    /* show noise pattern */
    float pcutoff;  /* pattern cutoff frequency (relative to max) */
    float pfactor;  /* noise pattern denoise strength */
    float sigma2;   /* noise level for middle frequencies */
    float sigma3;   /* noise level for low frequencies */
    float sigma4;   /* noise level for lowest (zero) frequencies */
    float degrid;   /* decrease grid */
    float dehalo;   /* remove halo strength - v.1.9 */
    float hr;       /* halo radius - v1.9 */
    float ht;       /* halo threshold - v1.9 */
    int   ncpu;     /* number of threads - v2.0 */

    int multiplane; /* multiplane value */

    /* additional parameterss */
    float *in;
    fftwf_complex *outcache[5], *outtemp;
    fftwf_complex *outrez, *gridsample; /* v1.8 */
    fftwf_plan plan, planinv, plan1;
    int nox, noy;
    int outwidth;
    int outpitch; /* v.1.7 */

    int outsize;
    int howmanyblocks;

    int ndim[2];
    int inembed[2];
    int onembed[2];

    float *wanxl; /* analysis */
    float *wanxr;
    float *wanyl;
    float *wanyr;

    float *wsynxl; /* synthesis */
    float *wsynxr;
    float *wsynyl;
    float *wsynyr;

    float *wsharpen;
    float *wdehalo;

    int nlast;  /* frame number at last step */
    int btcurlast;  /* v1.7 */

    fftwf_complex *outLast,*covar, *covarProcess;
    float sigmaSquaredNoiseNormed;
    float sigmaSquaredNoiseNormed2D;
    float sigmaNoiseNormed2D;
    float sigmaMotionNormed;
    float sigmaSquaredSharpenMinNormed;
    float sigmaSquaredSharpenMaxNormed;
    float ht2n; /* halo threshold squared normed */
    float norm; /* normalization factor */

    uint8_t *coverbuf; /*  block buffer covering the frame without remainders (with sufficient width and heigth) */
    int coverwidth;
    int coverheight;
    int coverpitch;

    int mirw; /* mirror width for padding */
    int mirh; /* mirror height for padding */

    int planeBase; /* color base value (0 for luma, 128 for chroma) */

    float *mean;

    float *pwin;
    float *pattern2d;
    float *pattern3d;
    bool  isPatternSet;
    float psigma;
    char *messagebuf;

    fftwf_complex ** cachefft;  /* v1.8 */
    int            * cachewhat; /* v1.8 */
    int              cachesize; /* v1.8 */

    void InitOverlapPlane( float * inp, const uint8_t *srcp, int src_pitch, int planeBase );
    void DecodeOverlapPlane( const float *in, float norm, uint8_t *dstp, int dst_pitch, int planeBase );

    template < int btcur > void Wiener3D( int n, const VSFrameRef *src, VSFrameContext *frame_ctx, const VSAPI *vsapi );

public:
    VSVideoInfo vi;
    VSNodeRef  *node;
    using bad_param = class bad_param : public CustomException { using CustomException::CustomException; };
    using bad_alloc = class bad_alloc : public CustomException { using CustomException::CustomException; };
    using bad_open  = class bad_open  : public CustomException { using CustomException::CustomException; };
    using bad_plan  = class bad_plan  : public CustomException { using CustomException::CustomException; };

    void ApplyFilter( int n, VSFrameRef *dst, const VSFrameRef *src, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );

    inline bool getIsPatternSet() { return isPatternSet; }

    /* Constructor */
    FFT3DFilter
    (
        float _sigma, float _beta, int _plane, int _bw, int _bh, int _bt, int _ow, int _oh,
        float _kratio, float _sharpen, float _scutoff, float _svr, float _smin, float _smax,
        bool _measure, bool _interlaced, int _wintype,
        int _pframe, int _px, int _py, bool _pshow, float _pcutoff, float _pfactor,
        float _sigma2, float _sigma3, float _sigma4, float _degrid,
        float _dehalo, float _hr, float _ht, int _ncpu, int _multiplane,
        VSVideoInfo _vi, VSNodeRef *node
    );

    /* Destructor */
    ~FFT3DFilter();
};

class FFT3DFilterMulti
{
    FFT3DFilter *filtered;
    FFT3DFilter *YClip, *UClip, *VClip;
    int   multiplane;
    int   bt;       /* block size  along time (mumber of frames), =0 for Kalman, >0 for Wiener */
    int   pframe;   /* noise pattern frame number */
    bool  pshow;    /* show noise pattern */
    float pfactor;  /* noise pattern denoise strength */
    bool  isPatternSet;

    VSFrameRef *newVideoFrame( const VSFrameRef *src, VSCore *core, const VSAPI *vsapi );

public:
    VSVideoInfo vi;
    VSNodeRef  *node;
    using bad_param = class bad_param : public CustomException { using CustomException::CustomException; };

    void RequestFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    VSFrameRef *GetFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );

    /* Constructor */
    FFT3DFilterMulti
    (
        float _sigma, float _beta, int _multiplane, int _bw, int _bh, int _bt, int _ow, int _oh,
        float _kratio, float _sharpen, float _scutoff, float _svr, float _smin, float _smax,
        bool _measure, bool _interlaced, int _wintype,
        int _pframe, int _px, int _py, bool _pshow, float _pcutoff, float _pfactor,
        float _sigma2, float _sigma3, float _sigma4, float _degrid,
        float _dehalo, float _hr, float _ht, int _ncpu,
        const VSMap *in, const VSAPI *vsapi
    );

    /* Destructor */
    ~FFT3DFilterMulti();
};
