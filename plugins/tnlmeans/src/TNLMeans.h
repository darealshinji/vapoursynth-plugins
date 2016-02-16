/*
**                 TNLMeans for VapourSynth
**
**   TNLMeans is an implementation of the NL-means denoising algorithm.
**   Aside from the original method, TNLMeans also supports extension
**   into 3D, a faster, block based approach, and a multiscale version.
**
**   Copyright (C) 2006-2007 Kevin Stone
**   Copyright (C) 2015      Yusuke Nakamura
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <cmath>
#include <cstring>
#include <algorithm>
#include <limits>
#include <string>

#ifdef __MINGW32__
#include "mingw.thread.h"
#include "mingw.mutex.h"
#else
#include <thread>
#include <mutex>
#endif

#include "AlignedMemory.h"

class CustomException
{
private:
    const std::string name;
public:
    CustomException() : name( std::string() ) {}
    CustomException( const std::string name ) : name( name ) {}
    const char * what() const noexcept { return name.c_str(); }
};

struct SDATA
{
    AlignedArrayObject< double, 16 > *weights;
    AlignedArrayObject< double, 16 > *sums;
    AlignedArrayObject< double, 16 > *wmaxs;
};

class nlFrame
{
public:
    int               fnum;
    const VSAPI      *vsapi;
    const VSFrameRef *pf;
    SDATA           **ds;
    int              *dsa;
    typedef class {} bad_alloc;
    nlFrame( bool _useblocks, int _size, const VSVideoInfo &vi, const VSAPI *_vsapi );
    ~nlFrame();
    void setFNum( int i );
    void clean();
};

class nlCache
{
public:
    nlFrame **frames;
    int start_pos, size;
    typedef class {} bad_alloc;
    nlCache( int _size, bool _useblocks, const VSVideoInfo &vi, const VSAPI *vsapi );
    ~nlCache();
    void resetCacheStart( int first, int last );
    int  getCachePos    ( int n );
    void clearDS        ( nlFrame *nl );
    void clean();
};

class nlThread
{
public:
    bool active;
    AlignedArrayObject< double, 16 > *sumsb;
    AlignedArrayObject< double, 16 > *weightsb;
    AlignedArrayObject< double, 16 > *gw;
    nlCache *fc;
    SDATA   *ds;
    nlThread();
    ~nlThread();
};

class ActiveThread
{
private:
    int        id;
    nlThread  *thread;
public:
    inline int &GetId() { return id; };
    ActiveThread
    (
        nlThread * &threads,
        int        &numThreads,
        std::mutex &mtx
    );
    ~ActiveThread();
};

class TNLMeans
{
private:
    int       Ax, Ay, Az;
    int       Sx, Sy;
    int       Bx, By;
    int       Sxd, Syd, Sxa;
    int       Bxd, Byd, Bxa;
    int       Axd, Ayd, Axa, Azdm1;
    double    a, a2;
    double    h, hin, h2in;
    bool      ssd;
    int       numThreads;
    nlThread *threads;
    std::mutex mtx;
    int mapn( int n );
    template < typename pixel > inline double GetSSD( const pixel * &s1, const pixel * &s2, const double * &gwT, const int &k ) { return (s1[k] - s2[k]) * (s1[k] - s2[k]) * gwT[k]; }
    template < typename pixel > inline double GetSAD( const pixel * &s1, const pixel * &s2, const double * &gwT, const int &k ) { return std::abs( s1[k] - s2[k] ) * gwT[k]; }
    inline double GetSSDWeight( const double &diff, const double &gweights ) { return std::exp( (diff / gweights) * h2in ); }
    inline double GetSADWeight( const double &diff, const double &gweights ) { return std::exp( (diff / gweights) * hin ); }
    template < int ssd, typename pixel > void GetFrameByMethod( int n, const int threadId, const int peak, VSFrameRef *dst, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    template < int ssd, typename pixel > void GetFrameWZ      ( int n, const int threadId, const int peak, VSFrameRef *dst, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    template < int ssd, typename pixel > void GetFrameWZB     ( int n, const int threadId, const int peak, VSFrameRef *dst, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    template < int ssd, typename pixel > void GetFrameWOZ     ( int n, const int threadId, const int peak, VSFrameRef *dst, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    template < int ssd, typename pixel > void GetFrameWOZB    ( int n, const int threadId, const int peak, VSFrameRef *dst, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    template < typename T > inline void ForwardPointer(       T * &p, const int offset ) { p = reinterpret_cast<      T *>(reinterpret_cast<      uint8_t *>(p) + offset); }
    template < typename T > inline void ForwardPointer( const T * &p, const int offset ) { p = reinterpret_cast<const T *>(reinterpret_cast<const uint8_t *>(p) + offset); }
    template < typename pixel > inline       pixel *GetPixel(       pixel *p, const int offset ) { return reinterpret_cast<      pixel *>(reinterpret_cast<      uint8_t *>(p) + offset); }
    template < typename pixel > inline const pixel *GetPixel( const pixel *p, const int offset ) { return reinterpret_cast<const pixel *>(reinterpret_cast<const uint8_t *>(p) + offset); }
    template < typename pixel > inline const pixel GetPixelValue( const pixel *p, const int offset ) { return *reinterpret_cast<const pixel *>(reinterpret_cast<const uint8_t *>(p) + offset); }
    inline const int GetPixelMaxValue( const int bps )
    {
        static constexpr int peak[16] = { 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535 };
        return peak[bps - 1];
    }

public:
    VSVideoInfo vi;
    VSNodeRef  *node;
    void RequestFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    VSFrameRef *GetFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    using bad_param = class bad_param : public CustomException { using CustomException::CustomException; };
    using bad_alloc = class bad_alloc : public CustomException { using CustomException::CustomException; };
    /* Constructor */
    TNLMeans
    (
        int _Ax, int _Ay, int _Az,
        int _Sx, int _Sy,
        int _Bx, int _By,
        double _a, double _h, bool ssd,
        const VSMap *in,
        VSMap       *out,
        VSCore      *core,
        const VSAPI *vsapi
    );
    /* Destructor */
    ~TNLMeans();
};

static inline void fill_zero_d( double *x, size_t n )
{
    if( std::numeric_limits<double>::is_iec559 )
        std::memset( x, 0, n * sizeof(double) );
    else
        std::fill_n( x, n, 0.0 );
}
