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

#include "VapourSynth.h"
#include "TNLMeans.h"

#include <cstdlib>

TNLMeans::TNLMeans
(
    int _Ax, int _Ay, int _Az,
    int _Sx, int _Sy,
    int _Bx, int _By,
    double _a, double _h, bool _ssd,
    const VSMap *in,
    VSMap       *out,
    VSCore      *core,
    const VSAPI *vsapi
) : Ax( _Ax ), Ay( _Ay ), Az( _Az ),
    Sx( _Sx ), Sy( _Sy ),
    Bx( _Bx ), By( _By ),
    a( _a ), h( _h ), ssd( _ssd )
{
    node =  vsapi->propGetNode( in, "clip", 0, 0 );
    vi   = *vsapi->getVideoInfo( node );
    numThreads = vsapi->getCoreInfo( core )->numThreads;
    if( h <= 0.0 ) throw bad_param{ "h must be greater than 0" };
    if( a <= 0.0 ) throw bad_param{ "a must be greater than 0" };
    if( Ax < 0 )   throw bad_param{ "ax must be greater than or equal to 0" };
    if( Ay < 0 )   throw bad_param{ "ay must be greater than or equal to 0" };
    if( Az < 0 )   throw bad_param{ "az must be greater than or equal to 0" };
    if( Bx < 0 )   throw bad_param{ "bx must be greater than or equal to 0" };
    if( By < 0 )   throw bad_param{ "by must be greater than or equal to 0" };
    if( Sx < 0 )   throw bad_param{ "sx must be greater than or equal to 0" };
    if( Sy < 0 )   throw bad_param{ "sy must be greater than or equal to 0" };
    if( Sx < Bx )  throw bad_param{ "sx must be greater than or equal to bx" };
    if( Sy < By )  throw bad_param{ "sy must be greater than or equal to by" };
    h2in = -1.0 / (h * h);
    hin = -1.0 / h;
    Sxd = Sx * 2 + 1;
    Syd = Sy * 2 + 1;
    Sxa = Sxd * Syd;
    Bxd = Bx * 2 + 1;
    Byd = By * 2 + 1;
    Bxa = Bxd * Byd;
    Axd = Ax * 2 + 1;
    Ayd = Ay * 2 + 1;
    Axa = Axd * Ayd;
    Azdm1 = Az * 2;
    a2 = a * a;

    std::unique_ptr< nlThread [] > threads( new ( std::nothrow ) nlThread[numThreads] );
    if( threads == nullptr ) throw bad_alloc{ "threads" };

    for( int i = 0; i < numThreads; ++i )
    {
        nlThread *t = &threads.get()[i];
        if( Az )
        {
            try { t->fc = new nlCache{ Az * 2 + 1, (Bx > 0 || By > 0), vi, vsapi }; }
            catch( nlFrame::bad_alloc & ) { throw bad_alloc{ "nlFrame" }; }
            catch( ... )                  { throw bad_alloc{ "nlCache" }; }
        }

        if( Bx || By )
        {
            try { t->sumsb    = new AlignedArrayObject< double, 16 >{ Bxa }; }
            catch( ... ) { throw bad_alloc{ "sumsb" }; }
            try { t->weightsb = new AlignedArrayObject< double, 16 >{ Bxa }; }
            catch( ... ) { throw bad_alloc{ "weightsb" }; }
        }
        else if( Az == 0 )
        {
            SDATA *ds = new SDATA();
            t->ds = ds;
            try { ds->sums    = new AlignedArrayObject< double, 16 >{ vi.width * vi.height }; }
            catch( ... ) { throw bad_alloc{ "sums" }; }
            try { ds->weights = new AlignedArrayObject< double, 16 >{ vi.width * vi.height }; }
            catch( ... ) { throw bad_alloc{ "weights" }; }
            try { ds->wmaxs   = new AlignedArrayObject< double, 16 >{ vi.width * vi.height }; }
            catch( ... ) { throw bad_alloc{ "wmaxs" }; }
        }

        try { t->gw = new AlignedArrayObject< double, 16 >{ Sxd * Syd }; }
        catch( ... ) { throw bad_alloc{ "gw" }; }
        double *gw = t->gw->get();
        int w = 0, m, n;
        for( int j = -Sy; j <= Sy; ++j )
        {
            if( j < 0 )
                m = std::min( j + By, 0 );
            else
                m = std::max( j - By, 0 );
            for( int k = -Sx; k <= Sx; ++k )
            {
                if( k < 0 )
                    n = std::min( k + Bx, 0 );
                else
                    n = std::max( k - Bx, 0 );
                gw[w++] = std::exp( -((m * m + n * n) / (2 * a2)) );
            }
        }
    }
    this->threads = threads.release();
}

TNLMeans::~TNLMeans()
{
    delete [] threads;
}

void TNLMeans::RequestFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    for( int i = n - Az; i <= n + Az; ++i )
        vsapi->requestFrameFilter( mapn( i ), node, frame_ctx );
}

template < int ssd, typename pixel >
void TNLMeans::GetFrameByMethod
(
    int             n,
    const int       threadId,
    const int       peak,
    VSFrameRef     *dst,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    if( Az )
    {
        if( Bx || By )
            GetFrameWZB< ssd, pixel >( n, threadId, peak, dst, frame_ctx, core, vsapi );
        else
            GetFrameWZ< ssd, pixel >( n, threadId, peak, dst, frame_ctx, core, vsapi );
    }
    else
    {
        if( Bx || By )
            GetFrameWOZB< ssd, pixel >( n, threadId, peak, dst, frame_ctx, core, vsapi );
        else
            GetFrameWOZ< ssd, pixel >( n, threadId, peak, dst, frame_ctx, core, vsapi );
    }
}

VSFrameRef *TNLMeans::GetFrame
(
    int             n,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    ActiveThread thread( threads, numThreads, mtx );

    int peak;
    std::unique_ptr< const VSFrameRef, decltype( vsapi->freeFrame ) > unique_src
    (
        vsapi->getFrameFilter( mapn( n ), node, frame_ctx ),
        vsapi->freeFrame
    );
    const VSFrameRef *src = unique_src.get();
    if( src )
    {
        const VSFormat *format = vsapi->getFrameFormat( src );

        if( format->colorFamily == cmCompat )
        {
            vsapi->setFilterError( "TNLMeans:  only planar formats are supported", frame_ctx );
            return nullptr;
        }
        if( format->sampleType != stInteger )
        {
            vsapi->setFilterError( "TNLMeans:  sample type must be integer!", frame_ctx );
            return nullptr;
        }

        const int bps = format->bitsPerSample;
        if( bps <= 0 || bps > 16 )
        {
            vsapi->setFilterError( "TNLMeans:  bitsPerSample must be 1 to 16!", frame_ctx );
            return nullptr;
        }
        peak = GetPixelMaxValue( bps );
    }
    else
    {
        vsapi->setFilterError( "TNLMeans:  getFrameFilter failure (src)!", frame_ctx );
        return nullptr;
    }

    std::unique_ptr< VSFrameRef, decltype( vsapi->freeFrame ) > unique_dst
    (
        vsapi->newVideoFrame
        (
            vsapi->getFrameFormat( src ),
            vsapi->getFrameWidth ( src, 0 ),
            vsapi->getFrameHeight( src, 0 ),
            src, core
        ),
        vsapi->freeFrame
    );
    VSFrameRef *dst = unique_dst.get();
    if( dst == nullptr )
    {
        vsapi->setFilterError( "TNLMeans:  newVideoFrame failure (dst)!", frame_ctx );
        return nullptr;
    }

    unique_src.reset();

    if( peak <= 255 )
    {
        if( ssd )
            GetFrameByMethod< 1, uint8_t >( n, thread.GetId(), peak, dst, frame_ctx, core, vsapi );
        else
            GetFrameByMethod< 0, uint8_t >( n, thread.GetId(), peak, dst, frame_ctx, core, vsapi );
    }
    else
    {
        if( ssd )
            GetFrameByMethod< 1, uint16_t >( n, thread.GetId(), peak, dst, frame_ctx, core, vsapi );
        else
            GetFrameByMethod< 0, uint16_t >( n, thread.GetId(), peak, dst, frame_ctx, core, vsapi );
    }

    return unique_dst.release();
}

template < int ssd, typename pixel >
void TNLMeans::GetFrameWZ
(
    int             n,
    const int       threadId,
    const int       peak,
    VSFrameRef     *dstPF,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    nlCache *fc = threads[threadId].fc;
    double  *gw = threads[threadId].gw->get();
    fc->resetCacheStart( n - Az, n + Az );
    for( int i = n - Az; i <= n + Az; ++i )
    {
        nlFrame *nl = fc->frames[fc->getCachePos( i - n + Az )];
        if( nl->fnum != i )
        {
            vsapi->freeFrame( nl->pf );
            nl->pf = vsapi->getFrameFilter( mapn( i ), node, frame_ctx );
            nl->setFNum( i );
            fc->clearDS( nl );
        }
    }
    std::unique_ptr< AlignedArrayObject< const pixel *, 16 > > _pfplut( new AlignedArrayObject< const pixel *, 16 >{ fc->size } );
    std::unique_ptr< AlignedArrayObject< const SDATA *, 16 > > _dslut ( new AlignedArrayObject< const SDATA *, 16 >{ fc->size } );
    std::unique_ptr< AlignedArrayObject<       int   *, 16 > > _dsalut( new AlignedArrayObject<       int   *, 16 >{ fc->size } );
    const pixel **pfplut = _pfplut.get()->get();
    const SDATA **dslut  = _dslut.get()->get();
    int         **dsalut = _dsalut.get()->get();
    for( int i = 0; i < fc->size; ++i )
        dsalut[i] = fc->frames[fc->getCachePos( i )]->dsa;
    int *ddsa = dsalut[Az];
    const VSFrameRef *srcPF = fc->frames[fc->getCachePos( Az )]->pf;
    const int startz = Az - std::min( n, Az );
    const int stopz  = Az + std::min( vi.numFrames - n - 1, Az );
    for( int plane = 0; plane < vi.format->numPlanes; ++plane )
    {
        const pixel *srcp = reinterpret_cast<const pixel *>(vsapi->getReadPtr( srcPF, plane ));
        const pixel *pf2p = reinterpret_cast<const pixel *>(vsapi->getReadPtr( srcPF, plane ));
        pixel    *dstp     = reinterpret_cast<pixel *>(vsapi->getWritePtr( dstPF, plane ));
        const int pitch    = vsapi->getStride     ( dstPF, plane );
        const int height   = vsapi->getFrameHeight( dstPF, plane );
        const int width    = vsapi->getFrameWidth ( dstPF, plane );
        const int heightm1 = height - 1;
        const int widthm1  = width  - 1;
        for( int i = 0; i < fc->size; ++i )
        {
            const int pos = fc->getCachePos( i );
            pfplut[i] = reinterpret_cast<const pixel *>(vsapi->getReadPtr( fc->frames[pos]->pf, plane ));
            dslut [i] = fc->frames[pos]->ds[plane];
        }
        const SDATA *dds = dslut[Az];
        for( int y = 0; y < height; ++y )
        {
            const int startyt = std::max( y - Ay, 0 );
            const int stopy   = std::min( y + Ay, heightm1 );
            const int doffy   = y * width;
            for( int x = 0; x < width; ++x )
            {
                const int startxt = std::max( x - Ax, 0 );
                const int stopx   = std::min( x + Ax, widthm1 );
                const int doff = doffy + x;
                double *dsum    = &dds->sums->get()   [doff];
                double *dweight = &dds->weights->get()[doff];
                double *dwmax   = &dds->wmaxs->get()  [doff];
                for( int z = startz; z <= stopz; ++z )
                {
                    if( ddsa[z] == 1 ) continue;
                    else ddsa[z] = 2;
                    const int starty = (z == Az) ? y : startyt;
                    const SDATA *cds = dslut[z];
                    int *cdsa = dsalut[z];
                    const pixel *pf1p = pfplut[z];
                    for( int u = starty; u <= stopy; ++u )
                    {
                        const int startx = (u == y && z == Az) ? x+1 : startxt;
                        const int yT = -std::min( std::min( Sy, u ), y );
                        const int yB =  std::min( std::min( Sy, heightm1 - u ), heightm1 - y );
                        const pixel *s1_saved = GetPixel( pf1p,     (u+yT)*pitch );
                        const pixel *s2_saved = GetPixel( pf2p + x, (y+yT)*pitch );
                        const double *gw_saved = gw+(yT+Sy)*Sxd+Sx;
                        const int pf1pl = u * pitch;
                        const int coffy = u * width;
                        for( int v = startx; v <= stopx; ++v )
                        {
                            const int coff = coffy + v;
                            double *csum    = &cds->sums->get()   [coff];
                            double *cweight = &cds->weights->get()[coff];
                            double *cwmax   = &cds->wmaxs->get()  [coff];
                            const int xL = -std::min( std::min( Sx, v ), x );
                            const int xR =  std::min( std::min( Sx, widthm1 - v ), widthm1 - x );
                            const pixel *s1 = s1_saved + v;
                            const pixel *s2 = s2_saved;
                            const double *gwT = gw_saved;
                            double diff = 0.0, gweights = 0.0;
                            for( int j = yT; j <= yB; ++j )
                            {
                                for( int k = xL; k <= xR; ++k )
                                {
                                    diff     += ssd ? GetSSD( s1, s2, gwT, k ) : GetSAD( s1, s2, gwT, k );
                                    gweights += gwT[k];
                                }
                                ForwardPointer( s1, pitch );
                                ForwardPointer( s2, pitch );
                                gwT += Sxd;
                            }
                            const double weight = ssd ? GetSSDWeight( diff, gweights ) : GetSADWeight( diff, gweights );
                            *dweight += weight;
                            *dsum    += weight*GetPixelValue( pf1p + v, pf1pl );
                            if( weight > *dwmax ) *dwmax = weight;
                            if( cdsa[Azdm1-z] != 1 )
                            {
                                *cweight += weight;
                                *csum    += weight*srcp[x];
                                if( weight > *cwmax ) *cwmax = weight;
                            }
                        }
                    }
                }
                const double wmax = *dwmax <= std::numeric_limits<double>::epsilon() ? 1.0 : *dwmax;
                *dsum    += wmax*srcp[x];
                *dweight += wmax;
                dstp[x] = std::max( std::min( int(((*dsum) / (*dweight)) + 0.5), peak ), 0 );
            }
            ForwardPointer( dstp, pitch );
            ForwardPointer( srcp, pitch );
        }
    }
    int j = fc->size - 1;
    for( int i = 0; i < fc->size; ++i, --j )
    {
        int *cdsa = fc->frames[fc->getCachePos( i )]->dsa;
        if( ddsa[i] == 2 ) ddsa[i] = cdsa[j] = 1;
    }
}

template < int ssd, typename pixel >
void TNLMeans::GetFrameWZB
(
    int             n,
    const int       threadId,
    const int       peak,
    VSFrameRef     *dstPF,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    nlCache *fc       = threads[threadId].fc;
    double  *sumsb    = threads[threadId].sumsb->get();
    double  *weightsb = threads[threadId].weightsb->get();
    double  *gw       = threads[threadId].gw->get();
    fc->resetCacheStart( n - Az, n + Az );
    for( int i = n - Az; i <= n + Az; ++i )
    {
        nlFrame *nl = fc->frames[fc->getCachePos( i - n + Az )];
        if( nl->fnum != i )
        {
            vsapi->freeFrame( nl->pf );
            nl->pf = vsapi->getFrameFilter( mapn( i ), node, frame_ctx );
            nl->setFNum( i );
        }
    }
    std::unique_ptr< AlignedArrayObject< const pixel *, 16 > > _pfplut( new AlignedArrayObject< const pixel *, 16 >{ fc->size } );
    const pixel **pfplut = _pfplut.get()->get();
    const VSFrameRef *srcPF = fc->frames[fc->getCachePos( Az )]->pf;
    const int startz = Az - std::min( n, Az );
    const int stopz  = Az + std::min( vi.numFrames - n - 1, Az );
    for( int plane = 0; plane < vi.format->numPlanes; ++plane )
    {
        const pixel *srcp = reinterpret_cast<const pixel *>(vsapi->getReadPtr( srcPF, plane ));
        const pixel *pf2p = reinterpret_cast<const pixel *>(vsapi->getReadPtr( srcPF, plane ));
        pixel    *dstp     = reinterpret_cast<pixel *>(vsapi->getWritePtr( dstPF, plane ));
        const int pitch    = vsapi->getStride     ( dstPF, plane );
        const int height   = vsapi->getFrameHeight( dstPF, plane );
        const int width    = vsapi->getFrameWidth ( dstPF, plane );
        const int heightm1 = height - 1;
        const int widthm1  = width  - 1;
        double *sumsb_saved    = sumsb    + Bx;
        double *weightsb_saved = weightsb + Bx;
        for( int i = 0; i < fc->size; ++i )
            pfplut[i] = reinterpret_cast<const pixel *>(vsapi->getReadPtr( fc->frames[fc->getCachePos( i )]->pf, plane ));
        for( int y = By; y < height + By; y += Byd )
        {
            const int starty = std::max( y - Ay, By );
            const int stopy  = std::min( y + Ay, heightm1 - std::min( By, heightm1 - y ) );
            const int yTr    = std::min( Byd, height - y + By );
            for( int x = Bx; x < width + Bx; x += Bxd )
            {
                fill_zero_d( sumsb,    Bxa );
                fill_zero_d( weightsb, Bxa );
                double wmax = 0.0;
                const int startx = std::max( x - Ax, Bx );
                const int stopx  = std::min( x + Ax, widthm1 - std::min( Bx, widthm1 - x ) );
                const int xTr    = std::min( Bxd,  width - x + Bx );
                for( int z = startz; z <= stopz; ++z )
                {
                    const pixel *pf1p = pfplut[z];
                    for( int u = starty; u <= stopy; ++u )
                    {
                        const int yT  = -std::min( std::min( Sy, u ), y );
                        const int yB  =  std::min( std::min( Sy, heightm1 - u ), heightm1 - y );
                        const int yBb =  std::min( std::min( By, heightm1 - u ), heightm1 - y );
                        const pixel *s1_saved  = GetPixel( pf1p,     (u+yT)*pitch );
                        const pixel *s2_saved  = GetPixel( pf2p + x, (y+yT)*pitch );
                        const pixel *sbp_saved = GetPixel( pf1p,     (u-By)*pitch );
                        const double *gw_saved = gw+(yT+Sy)*Sxd+Sx;
                        //const int pf1pl = u*pitch;
                        for( int v = startx; v <= stopx; ++v )
                        {
                            if( z == Az && u == y && v == x ) continue;
                            const int xL = -std::min( std::min( Sx, v ), x );
                            const int xR =  std::min( std::min( Sx, widthm1 - v ), widthm1 - x );
                            const pixel *s1 = s1_saved + v;
                            const pixel *s2 = s2_saved;
                            const double *gwT = gw_saved;
                            double diff = 0.0, gweights = 0.0;
                            for( int j = yT; j <= yB; ++j )
                            {
                                for( int k = xL; k <= xR; ++k )
                                {
                                    diff     += ssd ? GetSSD( s1, s2, gwT, k ) : GetSAD( s1, s2, gwT, k );
                                    gweights += gwT[k];
                                }
                                ForwardPointer( s1, pitch );
                                ForwardPointer( s2, pitch );
                                gwT += Sxd;
                            }
                            const double weight = ssd ? GetSSDWeight( diff, gweights ) : GetSADWeight( diff, gweights );
                            const int xRb = std::min( std::min( Bx, widthm1 - v ), widthm1 - x );
                            const pixel *sbp = sbp_saved + v;
                            double *sumsbT    = sumsb_saved;
                            double *weightsbT = weightsb_saved;
                            for( int j = -By; j <= yBb; ++j )
                            {
                                for( int k = -Bx; k <= xRb; ++k )
                                {
                                    sumsbT   [k] += sbp[k]*weight;
                                    weightsbT[k] += weight;
                                }
                                ForwardPointer( sbp, pitch );
                                sumsbT    += Bxd;
                                weightsbT += Bxd;
                            }
                            if( weight > wmax ) wmax = weight;
                        }
                    }
                }
                const pixel *srcpT = srcp + x - Bx;
                      pixel *dstpT = dstp + x - Bx;
                double *sumsbTr    = sumsb;
                double *weightsbTr = weightsb;
                if( wmax <= std::numeric_limits<double>::epsilon() )
                    wmax = 1.0;
                for( int j = 0; j < yTr; ++j )
                {
                    for( int k = 0; k < xTr; ++k )
                    {
                        sumsbTr   [k] += srcpT[k]*wmax;
                        weightsbTr[k] += wmax;
                        dstpT     [k] = std::max( std::min( int((sumsbTr[k] / weightsbTr[k]) + 0.5), peak ), 0 );
                    }
                    ForwardPointer( srcpT, pitch );
                    ForwardPointer( dstpT, pitch );
                    sumsbTr    += Bxd;
                    weightsbTr += Bxd;
                }
            }
            ForwardPointer( dstp, pitch*Byd );
            ForwardPointer( srcp, pitch*Byd );
        }
    }
}

template < int ssd, typename pixel >
void TNLMeans::GetFrameWOZ
(
    int             n,
    const int       threadId,
    const int       peak,
    VSFrameRef     *dstPF,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    const VSFrameRef *srcPF = vsapi->getFrameFilter( mapn( n ), node, frame_ctx );
    SDATA  *ds = threads[threadId].ds;
    double *gw = threads[threadId].gw->get();
    for( int plane = 0; plane < vi.format->numPlanes; ++plane )
    {
        const pixel *srcp = reinterpret_cast<const pixel *>(vsapi->getReadPtr( srcPF, plane ));
        const pixel *pfp  = reinterpret_cast<const pixel *>(vsapi->getReadPtr( srcPF, plane ));
        pixel    *dstp     = reinterpret_cast<pixel *>(vsapi->getWritePtr( dstPF, plane ));
        const int pitch    = vsapi->getStride     ( dstPF, plane );
        const int height   = vsapi->getFrameHeight( dstPF, plane );
        const int width    = vsapi->getFrameWidth ( dstPF, plane );
        const int heightm1 = height - 1;
        const int widthm1  = width  - 1;
        fill_zero_d( ds->sums->get(),    height * width );
        fill_zero_d( ds->weights->get(), height * width );
        fill_zero_d( ds->wmaxs->get(),   height * width );
        for( int y = 0; y < height; ++y )
        {
            const int stopy = std::min( y + Ay, heightm1 );
            const int doffy = y * width;
            for( int x = 0; x < width; ++x )
            {
                const int startxt = std::max( x - Ax, 0 );
                const int stopx   = std::min( x + Ax, widthm1 );
                const int doff = doffy + x;
                double *dsum    = &ds->sums->get()   [doff];
                double *dweight = &ds->weights->get()[doff];
                double *dwmax   = &ds->wmaxs->get()  [doff];
                for( int u = y; u <= stopy; ++u )
                {
                    const int startx = u == y ? x+1 : startxt;
                    const int yT = -std::min( std::min( Sy, u ), y );
                    const int yB =  std::min( std::min( Sy, heightm1 - u ), heightm1 - y );
                    const pixel *s1_saved = GetPixel( pfp,     (u+yT)*pitch );
                    const pixel *s2_saved = GetPixel( pfp + x, (y+yT)*pitch );
                    const double *gw_saved = gw+(yT+Sy)*Sxd+Sx;
                    const int pfpl  = u * pitch;
                    const int coffy = u * width;
                    for( int v = startx; v <= stopx; ++v )
                    {
                        const int coff = coffy+v;
                        double *csum    = &ds->sums->get()   [coff];
                        double *cweight = &ds->weights->get()[coff];
                        double *cwmax   = &ds->wmaxs->get()  [coff];
                        const int xL = -std::min( std::min( Sx, v ), x );
                        const int xR =  std::min( std::min( Sx, widthm1 - v ), widthm1 - x );
                        const pixel *s1 = s1_saved + v;
                        const pixel *s2 = s2_saved;
                        const double *gwT = gw_saved;
                        double diff = 0.0, gweights = 0.0;
                        for( int j = yT; j <= yB; ++j )
                        {
                            for( int k = xL; k <= xR; ++k )
                            {
                                diff     += ssd ? GetSSD( s1, s2, gwT, k ) : GetSAD( s1, s2, gwT, k );
                                gweights += gwT[k];
                            }
                            ForwardPointer( s1, pitch );
                            ForwardPointer( s2, pitch );
                            gwT += Sxd;
                        }
                        const double weight = ssd ? GetSSDWeight( diff, gweights ) : GetSADWeight( diff, gweights );
                        *cweight += weight;
                        *dweight += weight;
                        *csum += weight * srcp[x];
                        *dsum += weight * GetPixelValue( pfp + v, pfpl );
                        if( weight > *cwmax ) *cwmax = weight;
                        if( weight > *dwmax ) *dwmax = weight;
                    }
                }
                const double wmax = *dwmax <= std::numeric_limits<double>::epsilon() ? 1.0 : *dwmax;
                *dsum    += wmax*srcp[x];
                *dweight += wmax;
                dstp[x] = std::max( std::min( int(((*dsum) / (*dweight)) + 0.5), peak ), 0 );
            }
            ForwardPointer( dstp, pitch );
            ForwardPointer( srcp, pitch );
        }
    }
    vsapi->freeFrame( srcPF );
}

template < int ssd, typename pixel >
void TNLMeans::GetFrameWOZB
(
    int             n,
    const int       threadId,
    const int       peak,
    VSFrameRef     *dstPF,
    VSFrameContext *frame_ctx,
    VSCore         *core,
    const VSAPI    *vsapi
)
{
    const VSFrameRef *srcPF = vsapi->getFrameFilter( mapn( n ), node, frame_ctx );
    double *sumsb    = threads[threadId].sumsb->get();
    double *weightsb = threads[threadId].weightsb->get();
    double *gw       = threads[threadId].gw->get();
    for( int plane = 0; plane < vi.format->numPlanes; ++plane )
    {
        const pixel *srcp = reinterpret_cast<const pixel *>(vsapi->getReadPtr( srcPF, plane ));
        const pixel *pfp  = reinterpret_cast<const pixel *>(vsapi->getReadPtr( srcPF, plane ));
        pixel    *dstp     = reinterpret_cast<pixel *>(vsapi->getWritePtr( dstPF, plane ));
        const int pitch    = vsapi->getStride     ( dstPF, plane );
        const int height   = vsapi->getFrameHeight( dstPF, plane );
        const int width    = vsapi->getFrameWidth ( dstPF, plane );
        const int heightm1 = height - 1;
        const int widthm1  = width  - 1;
        double *sumsb_saved    = sumsb    + Bx;
        double *weightsb_saved = weightsb + Bx;
        for( int y = By; y < height + By; y += Byd )
        {
            const int starty = std::max( y - Ay, By );
            const int stopy  = std::min( y + Ay, heightm1 - std::min( By, heightm1 - y ) );
            const int yTr    = std::min( Byd, height - y + By );
            for( int x = Bx; x < width + Bx; x += Bxd )
            {
                fill_zero_d( sumsb,    Bxa );
                fill_zero_d( weightsb, Bxa );
                double wmax = 0.0;
                const int startx = std::max( x - Ax, Bx );
                const int stopx  = std::min( x + Ax, widthm1 - std::min( Bx, widthm1 - x ) );
                const int xTr    = std::min( Bxd, width - x + Bx );
                for( int u = starty; u <= stopy; ++u )
                {
                    const int yT  = -std::min( std::min( Sy, u ), y );
                    const int yB  =  std::min( std::min( Sy, heightm1 - u ), heightm1 - y );
                    const int yBb =  std::min( std::min( By, heightm1 - u ), heightm1 - y );
                    const pixel *s1_saved  = GetPixel( pfp,     (u+yT)*pitch );
                    const pixel *s2_saved  = GetPixel( pfp + x, (y+yT)*pitch );
                    const pixel *sbp_saved = GetPixel( pfp,     (u-By)*pitch );
                    const double *gw_saved = gw+(yT+Sy)*Sxd+Sx;
                    for( int v = startx; v <= stopx; ++v )
                    {
                        if (u == y && v == x) continue;
                        const int xL = -std::min( std::min( Sx, v ), x );
                        const int xR =  std::min( std::min( Sx, widthm1 - v ), widthm1 - x );
                        const pixel *s1 = s1_saved + v;
                        const pixel *s2 = s2_saved;
                        const double *gwT = gw_saved;
                        double diff = 0.0, gweights = 0.0;
                        for( int j = yT; j <= yB; ++j )
                        {
                            for( int k = xL; k <= xR; ++k )
                            {
                                diff     += ssd ? GetSSD( s1, s2, gwT, k ) : GetSAD( s1, s2, gwT, k );
                                gweights += gwT[k];
                            }
                            ForwardPointer( s1, pitch );
                            ForwardPointer( s2, pitch );
                            gwT += Sxd;
                        }
                        const double weight = ssd ? GetSSDWeight( diff, gweights ) : GetSADWeight( diff, gweights );
                        const int xRb = std::min( std::min( Bx, widthm1 - v ), widthm1 - x );
                        const pixel *sbp = sbp_saved + v;
                        double *sumsbT    = sumsb_saved;
                        double *weightsbT = weightsb_saved;
                        for( int j = -By; j <= yBb; ++j )
                        {
                            for( int k = -Bx; k <= xRb; ++k )
                            {
                                sumsbT   [k] += sbp[k]*weight;
                                weightsbT[k] += weight;
                            }
                            sumsbT    += Bxd;
                            weightsbT += Bxd;
                            ForwardPointer( sbp, pitch );
                        }
                        if( weight > wmax ) wmax = weight;
                    }
                }
                const pixel *srcpT = srcp + x - Bx;
                      pixel *dstpT = dstp + x - Bx;
                double *sumsbTr    = sumsb;
                double *weightsbTr = weightsb;
                if( wmax <= std::numeric_limits<double>::epsilon() )
                    wmax = 1.0;
                for( int j = 0; j < yTr; ++j )
                {
                    for( int k = 0; k < xTr; ++k )
                    {
                        sumsbTr   [k] += srcpT[k]*wmax;
                        weightsbTr[k] += wmax;
                        dstpT     [k] = std::max( std::min( int((sumsbTr[k] / weightsbTr[k]) + 0.5), peak ), 0 );
                    }
                    ForwardPointer( srcpT, pitch );
                    ForwardPointer( dstpT, pitch );
                    sumsbTr    += Bxd;
                    weightsbTr += Bxd;
                }
            }
            ForwardPointer( dstp, pitch*Byd );
            ForwardPointer( srcp, pitch*Byd );
        }
    }
    vsapi->freeFrame( srcPF );
}

int TNLMeans::mapn( int n )
{
    if( n < 0 ) return 0;
    if( n >= vi.numFrames ) return vi.numFrames - 1;
    return n;
}

nlFrame::nlFrame( bool _useblocks, int _size, const VSVideoInfo &vi, const VSAPI *_vsapi )
{
    vsapi = _vsapi;
    fnum = -20;
    pf   = nullptr;
    ds   = nullptr;
    dsa  = nullptr;
    if( !_useblocks )
    {
        try
        {
            ds = new SDATA * [3];
            std::memset( ds, 0, 3 * sizeof(SDATA *) );
            for( int i = 0; i < vi.format->numPlanes; ++i )
            {
                const int width  = vi.width  >> (i ? vi.format->subSamplingW : 0);
                const int height = vi.height >> (i ? vi.format->subSamplingH : 0);
                const size_t mem_size = width * height * sizeof(double);
                ds[i] = new SDATA();
                ds[i]->sums    = new AlignedArrayObject< double, 16 >{ mem_size };
                ds[i]->weights = new AlignedArrayObject< double, 16 >{ mem_size };
                ds[i]->wmaxs   = new AlignedArrayObject< double, 16 >{ mem_size };
            }
            dsa = new int[_size];
            for( int i = 0; i < _size; ++i )
                dsa[i] = 0;
        }
        catch( ... )
        {
            clean();
            throw bad_alloc{};
        }
    }
}

nlFrame::~nlFrame()
{
    clean();
}

void nlFrame::setFNum( int i )
{
    fnum = i;
}

void nlFrame::clean()
{
    if( pf )
        vsapi->freeFrame( pf );
    if( ds )
    {
        for( int i = 0; i < 3; ++i )
            if( ds[i] )
            {
                delete ds[i]->sums;
                delete ds[i]->weights;
                delete ds[i]->wmaxs;
                delete ds[i];
            }
        delete [] ds;
    }
    if( dsa )
        delete [] dsa;
}

nlCache::nlCache( int _size, bool _useblocks, const VSVideoInfo &vi, const VSAPI *vsapi )
{
    frames = nullptr;
    start_pos = size = -20;
    if( _size > 0 )
    {
        start_pos = 0;
        size = _size;
        try
        {
            frames = new nlFrame * [size];
            std::memset( frames, 0, size * sizeof(nlFrame *) );
            for( int i = 0; i < size; ++i )
                frames[i] = new nlFrame( _useblocks, _size, vi, vsapi );
        }
        catch( ... )
        {
            clean();
            throw bad_alloc{};
        }
    }
}

nlCache::~nlCache()
{
    clean();
}

void nlCache::resetCacheStart( int first, int last )
{
    for( int j = first; j <= last; ++j )
        for( int i = 0; i < size; ++i )
            if( frames[i]->fnum == j )
            {
                start_pos = i - j + first;
                if( start_pos < 0 )
                    start_pos += size;
                else if( start_pos >= size )
                    start_pos -= size;
                return;
            }
}

void nlCache::clearDS( nlFrame *nl )
{
    for( int i = 0; i < 3; ++i )
        if( nl->ds[i] )
        {
            const size_t res = nl->vsapi->getFrameWidth( nl->pf, i ) * nl->vsapi->getFrameHeight( nl->pf, i );
            fill_zero_d( nl->ds[i]->sums->get(),    res );
            fill_zero_d( nl->ds[i]->weights->get(), res );
            fill_zero_d( nl->ds[i]->wmaxs->get(),   res );
        }
    for( int i = 0; i < size; ++i ) nl->dsa[i] = 0;
}

int nlCache::getCachePos( int n )
{
    return (start_pos + n) % size;
}

void nlCache::clean()
{
    if( frames )
    {
        for( int i = 0; i < size; ++i )
            if( frames[i] )
                delete frames[i];
        delete [] frames;
    }
}

nlThread::nlThread()
{
    active = false;
    sumsb = weightsb = gw = nullptr;
    fc = nullptr;
    ds = nullptr;
}
nlThread::~nlThread()
{
    if( fc )
        delete fc;
    if( gw )
        delete gw;
    if( sumsb )
        delete sumsb;
    if( weightsb )
        delete weightsb;
    if( ds )
    {
        delete ds->sums;
        delete ds->weights;
        delete ds->wmaxs;
        delete ds;
    }
}

ActiveThread::ActiveThread
(
    nlThread * &threads,
    int        &numThreads,
    std::mutex &mtx
) : id( -1 ), thread( nullptr )
{
    do
    {
        std::lock_guard< std::mutex > lock( mtx );
        for( int i = 0; i < numThreads; ++i )
            if( threads[i].active == false )
            {
                id     = i;
                thread = &threads[i];
                thread->active = true;
                break;
            }
    } while( id == -1 );
}

ActiveThread::~ActiveThread()
{
    if( thread )
        thread->active = false;
}
