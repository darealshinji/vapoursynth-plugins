/*
    VapourSynth port by HolyWu

    DePan & DePanEstimate plugin for Avisynth 2.5 - global motion compensation
    Copyright(c) 2004-2016, A.G. Balakhnin aka Fizick
    bag@hotmail.ru

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <fftw3.h>
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>

static const float MOTION_UNKNOWN = 9999.f;
static const float MOTION_BAD = 0.f;
static const float PI = 3.14159265358979323846f;

//////////////////////////////////////////
// DePanEstimate

struct DePanEstimateData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    int range, winx, winy, wLeft, wTop, dxMax, dyMax;
    float trustLimit, zoomMax, stab, pixAspect;
    int fftCacheCapacity, winxPadded;
    int * fftCacheList, * fftCacheList2, * fftCacheListComp, * fftCacheListComp2;
    fftwf_complex ** fftCache, ** fftCache2, ** fftCacheComp, ** fftCacheComp2;
    fftwf_complex * correl, * correl2;
    float * realCorrel, * realCorrel2, * motionx, * motiony, * motionZoom, * trust;
    fftwf_plan plan, planInv;
};

static int getCacheNumber(const int * fftCacheList, const int fftCacheCapacity, const int n) {
    int found = -1;

    for (int i = 0; i < fftCacheCapacity; i++) {
        if (fftCacheList[i] == n)
            found = i;
    }

    return found;
}

static int getFreeCacheNumber(const int * fftCacheList, const int fftCacheCapacity) {
    int found = -1;

    for (int i = 0; i < fftCacheCapacity; i++) {
        if (fftCacheList[i] == -1)
            found = i;
    }

    return found;
}

template<typename T>
static fftwf_complex * getPlaneFFT(const T * srcp, const int width, const int height, const int stride, const int n,
                                   int * fftCacheList, fftwf_complex ** fftCache, const int winLeft, const DePanEstimateData * d) {
    const int ncs = getFreeCacheNumber(fftCacheList, d->fftCacheCapacity);
    assert(ncs >= 0);

    // get address of fft matrice in cache
    fftwf_complex * fftSrc = fftCache[ncs];
    float * VS_RESTRICT realData = reinterpret_cast<float *>(fftSrc);

    srcp += stride * d->wTop + winLeft; // offset of window data

    // make forward fft of src frame
    // prepare 2d data for fft
    for (int y = 0; y < d->winy; y++) {
        for (int x = 0; x < d->winx; x++)
            realData[x] = srcp[x];

        srcp += stride;
        realData += d->winxPadded;
    }

    // make forward fft of data
    fftwf_execute_dft_r2c(d->plan, reinterpret_cast<float *>(fftSrc), fftSrc);

    // now data is fft
    // reserve cache with this number
    fftCacheList[ncs] = n;

    return fftSrc;
}

static void multConjData2D(const fftwf_complex * fftNext, const fftwf_complex * fftSrc, fftwf_complex * VS_RESTRICT mult, const int winx, const int winy) {
    for (int i = 0; i < (winx / 2 + 1) * winy; i++) {
        mult[i][0] = fftNext[i][0] * fftSrc[i][0] + fftNext[i][1] * fftSrc[i][1]; // real part
        mult[i][1] = fftNext[i][0] * fftSrc[i][1] - fftNext[i][1] * fftSrc[i][0]; // imagine part
    }
}

static void getMotionVector(float * VS_RESTRICT correl, const int n, const int field, float * fdx, float * fdy, float * trust, const DePanEstimateData * d) {
    float xAdd, yAdd;
    int dx, dy, xMaxP1, xMaxM1, yMaxP1, yMaxM1;
    int xMax = 0, yMax = 0;

    // find global max on real part of correlation surface
    // new version: search only at 4 corners with ranges dxMax, dyMax
    float correlMax = correl[0];
    float correlMean = 0.f;
    int count = 0;
    float * correlp = correl;

    for (int y = 0; y <= d->dyMax; y++) { // top
        for (int x = 0; x <= d->dxMax; x++) { // left
            const float cur = correlp[x]; // real part
            correlMean += cur;
            count++;

            if (correlMax < cur) {
                correlMax = cur;
                xMax = x;
                yMax = y;
            }
        }

        for (int x = d->winx - d->dxMax; x < d->winx; x++) { // right
            const float cur = correlp[x]; // real part
            correlMean += cur;
            count++;

            if (correlMax < cur) {
                correlMax = cur;
                xMax = x;
                yMax = y;
            }
        }

        correlp += d->winxPadded;
    }

    correlp = correl + d->winxPadded * (d->winy - d->dyMax);

    for (int y = d->winy - d->dyMax; y < d->winy; y++) { // bottom
        for (int x = 0; x <= d->dxMax; x++) { // left
            const float cur = correlp[x]; // real part
            correlMean += cur;
            count++;

            if (correlMax < cur) {
                correlMax = cur;
                xMax = x;
                yMax = y;
            }
        }

        for (int x = d->winx - d->dxMax; x < d->winx; x++) { // right
            const float cur = correlp[x]; // real part
            correlMean += cur;
            count++;

            if (correlMax < cur) {
                correlMax = cur;
                xMax = x;
                yMax = y;
            }
        }

        correlp += d->winxPadded;
    }

    correlMean /= count; // mean value

    correlMax /= (d->winx * d->winy); // normalize value
    correlMean /= (d->winx * d->winy); // normalize value

    *trust = (correlMax - correlMean) * 100.f / (correlMax + FLT_EPSILON);

    if (xMax * 2 < d->winx)
        dx = xMax;
    else // get correct shift values on periodic surface (adjusted borders)
        dx = xMax - d->winx;

    if (yMax * 2 < d->winy)
        dy = yMax;
    else // get correct shift values on periodic surface (adjusted borders)
        dy = yMax - d->winy;

    // some trust decreasing for large shifts
    *trust *= (d->dxMax + 1) / (d->dxMax + 1 + d->stab * std::abs(dx)) * (d->dyMax + 1) / (d->dyMax + 1 + d->stab * std::abs(dy));

    if (*trust < d->trustLimit) { // reject if relative diffference correlMax from correlMean is small
        // set value to pure 0, what will be interpreted as bad mark (scene change)
        *fdx = 0.f;
        *fdy = 0.f;
    } else { // normal, no scene change
        // get more precise dx, dy by interpolation
        // get x, y of left and right of max
        if (xMax + 1 < d->winx)
            xMaxP1 = xMax + 1; // plus 1
        else
            xMaxP1 = xMax + 1 - d->winx; // over period

        if (xMax - 1 >= 0)
            xMaxM1 = xMax - 1; // minus 1
        else
            xMaxM1 = xMax - 1 + d->winx;

        if (yMax + 1 < d->winy)
            yMaxP1 = yMax + 1;
        else
            yMaxP1 = yMax + 1 - d->winy;

        if (yMax - 1 >= 0)
            yMaxM1 = yMax - 1;
        else
            yMaxM1 = yMax - 1 + d->winy;

        // first and second differential
        float f1 = (correl[yMax * d->winxPadded + xMaxP1] - correl[yMax * d->winxPadded + xMaxM1]) / 2.f;
        float f2 = correl[yMax * d->winxPadded + xMaxP1] + correl[yMax * d->winxPadded + xMaxM1] - correl[yMax * d->winxPadded + xMax] * 2.f;

        if (f2 == 0.f)
            xAdd = 0.f;
        else
            xAdd = std::min(std::max(-f1 / f2, -1.f), 1.f); // limit addition for stability

        if (std::abs(dx + xAdd) > d->dxMax)
            xAdd = 0.f;

        f1 = (correl[yMaxP1 * d->winxPadded + xMax] - correl[yMaxM1 * d->winxPadded + xMax]) / 2.f;
        f2 = correl[yMaxP1 * d->winxPadded + xMax] + correl[yMaxM1 * d->winxPadded + xMax] - correl[yMax * d->winxPadded + xMax] * 2.f;

        if (f2 == 0.f)
            yAdd = 0.f;
        else
            yAdd = std::min(std::max(-f1 / f2, -1.f), 1.f); // limit addition for stability

        if (std::abs(dy + yAdd) > d->dyMax)
            yAdd = 0.f;

        // correct line shift for fields
        if (field != -1) {
            const int isNOdd = n & 1; // =0 for even, =1 for odd

            // correct unneeded fields matching
            yAdd += (field == 1) ? 0.5f - isNOdd : -0.5f + isNOdd;

            // scale dy for fieldbased frame by factor 2
            yAdd *= 2.f;
            dy *= 2;
        }

        *fdx = dx + xAdd;
        *fdy = dy + yAdd;

        *fdy /= d->pixAspect;

        // if it is accidentally very small, reset it to small but non-zero value, to differ from pure 0 which be interpreted as bad value mark (scene change)
        if (std::abs(*fdx) < 0.01f)
            *fdx = (std::rand() * 2 - RAND_MAX > 0) ? 0.011f : -0.011f;
    }
}

template<typename T>
static void estimate(const VSFrameRef ** src, VSFrameRef * dst, const int n, DePanEstimateData * VS_RESTRICT d, const VSAPI * vsapi) {
    const int width = vsapi->getFrameWidth(dst, 0);
    const int height = vsapi->getFrameHeight(dst, 0);
    const int stride = vsapi->getStride(dst, 0) / sizeof(T);

    int err;
    int field = int64ToIntS(vsapi->propGetInt(vsapi->getFramePropsRO(dst), "_Field", 0, &err));
    if (err)
        field = -1;

    fftwf_complex * fftCur, * fftCur2, * fftPrev, * fftPrev2;

    // calculate motion data
    for (int nCur = std::max(n - d->range - 1, 1); nCur <= std::min(n + d->range + 1, d->vi->numFrames - 1); nCur++) { // extended range by 1 for scene detection
        if (d->motionx[nCur] == MOTION_UNKNOWN) {
            // calculate dx, dy now
            if (d->zoomMax == 1.f) { // NO ZOOM
                // check if fft of cur frame is in cache
                int ncs = getCacheNumber(d->fftCacheList, d->fftCacheCapacity, nCur);

                if (ncs != -1) { // found in cache
                    // get address of fft matrice in cache
                    fftCur = d->fftCache[ncs]; // central
                } else { // not found in cache
                    const T * curp = reinterpret_cast<const T *>(vsapi->getReadPtr(src[nCur - n + d->range + 2], 0)); // get current frame pointer

                    // get forward fft of src frame from cache or calculation
                    fftCur = getPlaneFFT<T>(curp, width, height, stride, nCur, d->fftCacheList, d->fftCache, d->wLeft, d);
                }

                // check if fft of prev frame is in cache
                ncs = getCacheNumber(d->fftCacheList, d->fftCacheCapacity, nCur - 1);

                if (ncs != -1) { // found in cache
                    // get address of fft matrice in cache
                    fftPrev = d->fftCache[ncs];
                } else { // not found in cache
                    const T * prevp = reinterpret_cast<const T *>(vsapi->getReadPtr(src[nCur - n + d->range + 1], 0)); // get previous frame pointer

                    // get forward fft of prev frame from cache or calculation
                    fftPrev = getPlaneFFT<T>(prevp, width, height, stride, nCur - 1, d->fftCacheList, d->fftCache, d->wLeft, d);
                }

                // prepare correlation data = mult fftSrc* by fftPrev
                multConjData2D(fftCur, fftPrev, d->correl, d->winx, d->winy);

                // make inverse fft of prepared correl data
                fftwf_execute_dft_c2r(d->planInv, d->correl, d->realCorrel);

                // now correl is true correlation surface
                // find global motion vector as maximum on correlation surface
                // save vector to motion table
                getMotionVector(d->realCorrel, nCur, field, &d->motionx[nCur], &d->motiony[nCur], &d->trust[nCur], d);

                d->motionZoom[nCur] = 1.f; // no zoom
            } else { // ZOOM, calculate 2 data sets (left and right)
                const int winLeft2 = d->wLeft + width / 2;
                float dx1, dx2, dy1, dy2, trust1, trust2;

                // check if fft of cur frame is in cache
                int ncs = getCacheNumber(d->fftCacheList, d->fftCacheCapacity, nCur);

                if (ncs != -1) { // found in cache
                    // get address of fft matrice in cache
                    fftCur = d->fftCache[ncs]; // left
                    fftCur2 = d->fftCache2[ncs]; // right
                } else { // not found in cache
                    const T * curp = reinterpret_cast<const T *>(vsapi->getReadPtr(src[nCur - n + d->range + 2], 0)); // get current frame pointer

                    // get forward fft of src frame from cache or calculation
                    fftCur = getPlaneFFT<T>(curp, width, height, stride, nCur, d->fftCacheList, d->fftCache, d->wLeft, d);
                    fftCur2 = getPlaneFFT<T>(curp, width, height, stride, nCur, d->fftCacheList2, d->fftCache2, winLeft2, d);
                }

                // check if fft of prev frame is in cache
                ncs = getCacheNumber(d->fftCacheList, d->fftCacheCapacity, nCur - 1);

                if (ncs != -1) { // found in cache
                    // get address of fft matrice in cache
                    fftPrev = d->fftCache[ncs]; // left
                    fftPrev2 = d->fftCache2[ncs]; // right
                } else { // not found in cache
                    const T * prevp = reinterpret_cast<const T *>(vsapi->getReadPtr(src[nCur - n + d->range + 1], 0)); // get previous frame pointer

                    // get forward fft of prev frame from cache or calculation
                    fftPrev = getPlaneFFT<T>(prevp, width, height, stride, nCur - 1, d->fftCacheList, d->fftCache, d->wLeft, d);
                    fftPrev2 = getPlaneFFT<T>(prevp, width, height, stride, nCur - 1, d->fftCacheList2, d->fftCache2, winLeft2, d);
                }

                // do estimation for left and right windows

                // left window
                // prepare correlation data = mult fftSrc* by fftPrev
                multConjData2D(fftCur, fftPrev, d->correl, d->winx, d->winy);

                // make inverse fft of prepared correl data
                fftwf_execute_dft_c2r(d->planInv, d->correl, d->realCorrel);

                // now correl is true correlation surface
                // find global motion vector as maximum on correlation surface
                // save vector to motion table
                getMotionVector(d->realCorrel, nCur, field, &dx1, &dy1, &trust1, d);

                // right window
                // prepare correlation data = mult fftSrc* by fftPrev
                multConjData2D(fftCur2, fftPrev2, d->correl2, d->winx, d->winy);

                // make inverse fft of prepared correl data
                fftwf_execute_dft_c2r(d->planInv, d->correl2, d->realCorrel2);

                // now correl is true correlation surface
                // find global motion vector as maximum on correlation surface
                // save vector to motion table
                getMotionVector(d->realCorrel2, nCur, field, &dx2, &dy2, &trust2, d);

                // now we have 2 motion data sets for left and right windows
                // estimate zoom factor
                const float zoom = 1.f + (dx2 - dx1) / (winLeft2 - d->wLeft);

                if (dx1 != 0.f && dx2 != 0.f && std::abs(zoom - 1.f) < d->zoomMax - 1.f) { // if motion data and zoom good
                    d->motionx[nCur] = (dx1 + dx2) / 2.f;
                    d->motiony[nCur] = (dy1 + dy2) / 2.f;
                    d->motionZoom[nCur] = zoom;
                    d->trust[nCur] = std::min(trust1, trust2);
                } else { // bad zoom
                    d->motionx[nCur] = 0.f;
                    d->motiony[nCur] = 0.f;
                    d->motionZoom[nCur] = 1.f;
                    d->trust[nCur] = std::min(trust1, trust2);
                }
            }
        }
    }

    // check scenechanges in range, as sharp decreasing of trust
    for (int nCur = n - d->range; nCur <= n + d->range; nCur++) {
        if (nCur - 1 >= 0 && nCur < d->vi->numFrames && d->trust[nCur] < d->trustLimit * 2.f && d->trust[nCur] < d->trust[nCur - 1] / 2.f) {
            // very sharp decrease of not very big trust, probably due to scenechange
            d->motionx[nCur] = 0.f;
            d->motiony[nCur] = 0.f;
            d->motionZoom[nCur] = 1.f;
        }

        if (nCur >= 0 && nCur + 1 < d->vi->numFrames && d->trust[nCur] < d->trustLimit * 2.f && d->trust[nCur] < d->trust[nCur + 1] / 2.f) {
            // very sharp decrease of not very big trust, probably due to scenechange
            d->motionx[nCur] = 0.f;
            d->motiony[nCur] = 0.f;
            d->motionZoom[nCur] = 1.f;
        }
    }

    // So, now we got all needed global motion info
    VSMap * props = vsapi->getFramePropsRW(dst);
    vsapi->propSetFloat(props, "DePanEstimateDx", d->motionx[n], paReplace);
    vsapi->propSetFloat(props, "DePanEstimateDy", d->motiony[n], paReplace);
    vsapi->propSetFloat(props, "DePanEstimateZoom", d->motionZoom[n], paReplace);
}

static void VS_CC estimateInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DePanEstimateData * d = static_cast<DePanEstimateData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC estimateGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DePanEstimateData * d = static_cast<DePanEstimateData *>(*instanceData);

    if (activationReason == arInitial) {
        for (int i = std::max(n - d->range - 2, 0); i <= std::min(n + d->range + 1, d->vi->numFrames - 1); i++)
            vsapi->requestFrameFilter(i, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        // clear some places in fft cache, un-needed for current frame range calculation (all besides n-range-1 to n+range+1)
        for (int i = 0; i < d->fftCacheCapacity; i++) {
            if (d->fftCacheList[i] < n - d->range - 1 || d->fftCacheList[i] > n + d->range + 1)
                d->fftCacheList[i] = -1;
            if (d->fftCacheList2[i] < n - d->range - 1 || d->fftCacheList2[i] > n + d->range + 1)
                d->fftCacheList2[i] = -1;
            if (d->fftCacheListComp[i] < n - d->range - 1 || d->fftCacheListComp[i] > n + d->range + 1)
                d->fftCacheListComp[i] = -1;
            if (d->fftCacheListComp2[i] < n - d->range - 1 || d->fftCacheListComp2[i] > n + d->range + 1)
                d->fftCacheListComp2[i] = -1;
        }

        const VSFrameRef ** src = new const VSFrameRef *[d->fftCacheCapacity];
        for (int i = n - d->range - 2; i <= n + d->range + 1; i++)
            src[i - n + d->range + 2] = vsapi->getFrameFilter(std::min(std::max(i, 0), d->vi->numFrames - 1), d->node, frameCtx);
        VSFrameRef * dst = vsapi->copyFrame(src[d->range + 2], core);

        if (d->vi->format->sampleType == stInteger) {
            if (d->vi->format->bitsPerSample == 8)
                estimate<uint8_t>(src, dst, n, d, vsapi);
            else
                estimate<uint16_t>(src, dst, n, d, vsapi);
        } else {
            estimate<float>(src, dst, n, d, vsapi);
        }

        for (int i = n - d->range - 2; i <= n + d->range + 1; i++)
            vsapi->freeFrame(src[i - n + d->range + 2]);
        delete[] src;
        return dst;
    }

    return nullptr;
}

static void VS_CC estimateFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DePanEstimateData * d = static_cast<DePanEstimateData *>(instanceData);

    vsapi->freeNode(d->node);

    delete[] d->fftCacheList;
    delete[] d->fftCacheList2;
    delete[] d->fftCacheListComp;
    delete[] d->fftCacheListComp2;

    for (int i = 0; i < d->fftCacheCapacity; i++) {
        vs_aligned_free(d->fftCache[i]);
        vs_aligned_free(d->fftCacheComp[i]);
        if (d->zoomMax != 1.f){
            vs_aligned_free(d->fftCache2[i]);
            vs_aligned_free(d->fftCacheComp2[i]);
        }
    }

    delete[] d->fftCache;
    delete[] d->fftCache2;
    delete[] d->fftCacheComp;
    delete[] d->fftCacheComp2;

    vs_aligned_free(d->correl);
    if (d->zoomMax != 1.f)
        vs_aligned_free(d->correl2);

    fftwf_destroy_plan(d->plan);
    fftwf_destroy_plan(d->planInv);

    delete[] d->motionx;
    delete[] d->motiony;
    delete[] d->motionZoom;
    delete[] d->trust;

    delete d;
}

static void VS_CC estimateCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    DePanEstimateData d;
    int err;

    d.range = int64ToIntS(vsapi->propGetInt(in, "range", 0, &err));

    d.trustLimit = static_cast<float>(vsapi->propGetFloat(in, "trust", 0, &err));
    if (err)
        d.trustLimit = 4.f;

    d.wLeft = int64ToIntS(vsapi->propGetInt(in, "wleft", 0, &err));
    if (err)
        d.wLeft = -1;

    const int wLeft0 = d.wLeft;
    d.wLeft = std::max(d.wLeft, 0);

    d.wTop = int64ToIntS(vsapi->propGetInt(in, "wtop", 0, &err));
    if (err)
        d.wTop = -1;

    const int wTop0 = d.wTop;
    d.wTop = std::max(d.wTop, 0);

    d.zoomMax = static_cast<float>(vsapi->propGetFloat(in, "zoommax", 0, &err));
    if (err)
        d.zoomMax = 1.f;

    d.stab = static_cast<float>(vsapi->propGetFloat(in, "stab", 0, &err));
    if (err)
        d.stab = 1.f;

    d.pixAspect = static_cast<float>(vsapi->propGetFloat(in, "pixaspect", 0, &err));
    if (err)
        d.pixAspect = 1.f;

    if (d.range < 0) {
        vsapi->setError(out, "DePanEstimate: range must be greater than or equal to 0");
        return;
    }

    if (d.trustLimit < 0.f || d.trustLimit > 100.f) {
        vsapi->setError(out, "DePanEstimate: trust must be between 0.0 and 100.0 (inclusive)");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || (d.vi->format->sampleType == stInteger && d.vi->format->bitsPerSample > 16) ||
        (d.vi->format->sampleType == stFloat && d.vi->format->bitsPerSample != 32)) {
        vsapi->setError(out, "DePanEstimate: only constant format 8-16 bits integer and 32 bits float input supported");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vi->format->colorFamily == cmRGB) {
        vsapi->setError(out, "DePanEstimate: RGB color family is not supported");
        vsapi->freeNode(d.node);
        return;
    }

    d.winx = int64ToIntS(vsapi->propGetInt(in, "winx", 0, &err));
    if (err)
        d.winx = 1 << static_cast<int>(std::log2(d.vi->width - d.wLeft));

    d.winy = int64ToIntS(vsapi->propGetInt(in, "winy", 0, &err));
    if (err)
        d.winy = 1 << static_cast<int>(std::log2(d.vi->height - d.wTop));

    if (d.winx < 1 || d.winx > d.vi->width - d.wLeft) {
        vsapi->setError(out, "DePanEstimate: winx must be greater than or equal to 1 and less than or equal to width-wleft");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.winy < 1 || d.winy > d.vi->height - d.wTop) {
        vsapi->setError(out, "DePanEstimate: winy must be greater than or equal to 1 and less than or equal to height-wtop");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.zoomMax != 1.f) {
        d.winx /= 2; // divide window x by 2 part (left and right)

        if (wLeft0 < 0)
            d.wLeft = (d.vi->width - d.winx * 2) / 4;
    } else {
        if (wLeft0 < 0)
            d.wLeft = (d.vi->width - d.winx) / 2;
    }

    if (wTop0 < 0)
        d.wTop = (d.vi->height - d.winy) / 2;

    d.dxMax = int64ToIntS(vsapi->propGetInt(in, "dxmax", 0, &err));
    if (err)
        d.dxMax = d.winx / 4;

    d.dyMax = int64ToIntS(vsapi->propGetInt(in, "dymax", 0, &err));
    if (err)
        d.dyMax = d.winy / 4;

    if (d.dxMax < 0 || d.dxMax >= d.winx / 2) {
        vsapi->setError(out, "DePanEstimate: dxmax must be greater than or equal to 0 and less than winx/2");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.dyMax < 0 || d.dyMax >= d.winy / 2) {
        vsapi->setError(out, "DePanEstimate: dymax must be greater than or equal to 0 and less than winy/2");
        vsapi->freeNode(d.node);
        return;
    }

    // set frames capacity of fft cache
    d.fftCacheCapacity = d.range * 2 + 4;

    d.fftCacheList = new int[d.fftCacheCapacity];
    d.fftCacheList2 = new int[d.fftCacheCapacity];
    d.fftCacheListComp = new int[d.fftCacheCapacity];
    d.fftCacheListComp2 = new int[d.fftCacheCapacity];

    // init (clear) cached frame numbers and fft matrices
    for (int i = 0; i < d.fftCacheCapacity; i++) {
        d.fftCacheList[i] = -1;
        d.fftCacheList2[i] = -1; // right window if zoom
        d.fftCacheListComp[i] = -1;
        d.fftCacheListComp2[i] = -1; // right window if zoom
    }

    // winsize = winx*winy;
    d.winxPadded = (d.winx / 2 + 1) * 2;
    const int fftSize = d.winxPadded * d.winy / 2; // complex

    // memory for cached fft
    d.fftCache = new fftwf_complex *[d.fftCacheCapacity];
    d.fftCache2 = new fftwf_complex *[d.fftCacheCapacity];
    d.fftCacheComp = new fftwf_complex *[d.fftCacheCapacity];
    d.fftCacheComp2 = new fftwf_complex *[d.fftCacheCapacity];

    for (int i = 0; i < d.fftCacheCapacity; i++) {
        d.fftCache[i] = vs_aligned_malloc<fftwf_complex>(fftSize * sizeof(fftwf_complex), 32);
        d.fftCacheComp[i] = vs_aligned_malloc<fftwf_complex>(fftSize * sizeof(fftwf_complex), 32);
        if (!d.fftCache[i] || !d.fftCacheComp[i]) {
            vsapi->setError(out, "DePanEstimate: malloc failure (fftCache/fftCacheComp)");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.zoomMax != 1.f) {
            d.fftCache2[i] = vs_aligned_malloc<fftwf_complex>(fftSize * sizeof(fftwf_complex), 32); // right window if zoom
            d.fftCacheComp2[i] = vs_aligned_malloc<fftwf_complex>(fftSize * sizeof(fftwf_complex), 32); // right window if zoom
            if (!d.fftCache2[i] || !d.fftCacheComp2[i]) {
                vsapi->setError(out, "DePanEstimate: malloc failure (fftCache2/fftCacheComp2)");
                vsapi->freeNode(d.node);
                return;
            }
        }
    }

    // memory for correlation matrice
    d.correl = vs_aligned_malloc<fftwf_complex>(fftSize * sizeof(fftwf_complex), 32);
    if (!d.correl) {
        vsapi->setError(out, "DePanEstimate: malloc failure (correl)");
        vsapi->freeNode(d.node);
        return;
    }
    d.realCorrel = reinterpret_cast<float *>(d.correl); // for inplace transform

    if (d.zoomMax != 1.f) {
        d.correl2 = vs_aligned_malloc<fftwf_complex>(fftSize * sizeof(fftwf_complex), 32);
        if (!d.correl2) {
            vsapi->setError(out, "DePanEstimate: malloc failure (correl2)");
            vsapi->freeNode(d.node);
            return;
        }
        d.realCorrel2 = reinterpret_cast<float *>(d.correl2); // for inplace transform
    }

    // create FFTW plan
    d.plan = fftwf_plan_dft_r2c_2d(d.winy, d.winx, d.realCorrel, d.correl, FFTW_MEASURE); // direct fft
    d.planInv = fftwf_plan_dft_c2r_2d(d.winy, d.winx, d.correl, d.realCorrel, FFTW_MEASURE); // inverse fft

    d.motionx = new float[d.vi->numFrames];
    d.motiony = new float[d.vi->numFrames];
    d.motionZoom = new float[d.vi->numFrames];
    d.trust = new float[d.vi->numFrames];

    // set motion value for initial frame as 0 (interpreted as scene change)
    d.motionx[0] = 0.f;
    d.motiony[0] = 0.f;
    d.motionZoom[0] = 1.f;
    for (int i = 1; i < d.vi->numFrames; i++)
        d.motionx[i] = MOTION_UNKNOWN; // set motion as unknown for all frames beside 0

    DePanEstimateData * data = new DePanEstimateData(d);

    vsapi->createFilter(in, out, "DePanEstimate", estimateInit, estimateGetFrame, estimateFree, fmSerial, 0, data, core);
}

//////////////////////////////////////////
// DePan

struct DePanData {
    VSNodeRef * node, * data;
    const VSVideoInfo * vi;
    float offset, pixAspect;
    int mirror, blur;
    bool matchFields, process[3];
    int intOffset, peak, border[3];
    float xCenter, yCenter;
    bool mTop, mBottom, mLeft, mRight;
};

struct Transform {
    float dxc, dxx, dxy, dyc, dyx, dyy;
};

enum Mirror {
    mirrorTop = 1,
    mirrorBottom = 2,
    mirrorLeft = 4,
    mirrorRight = 8
};

static void motion2Transform(const float dx1, const float dy1, const float rotation, const float zoom1, const float pixAspect, const float xCenter, const float yCenter,
                             const bool forward, const float fractureOffset, Transform * tr) {
    // fractureOffset > 0 for forward, < 0 for backward
    const float dx = fractureOffset * dx1;
    const float dy = fractureOffset * dy1;

    float rotradian = fractureOffset * rotation * PI / 180.f; // from degree to radian
    if (std::abs(rotradian) < 1e-6f)
        rotradian = 0.f; // for some stability of rounding precision

    float zoom = std::pow(zoom1, fractureOffset);
    if (std::abs(zoom - 1.f) < 1e-6f)
        zoom = 1.f; // for some stability of rounding precision

    const float sinus = std::sin(rotradian);
    const float cosinus = std::cos(rotradian);

    tr->dxx = cosinus * zoom;
    tr->dxy = -sinus / pixAspect * zoom;
    tr->dyx = sinus * zoom * pixAspect;
    tr->dyy = cosinus * zoom;

    if (forward) { // coefficients for forward
        tr->dxc = xCenter + (-xCenter * cosinus + yCenter / pixAspect * sinus) * zoom + dx;
        tr->dyc = yCenter + ((-yCenter / pixAspect * cosinus + -xCenter * sinus) * zoom + dy) * pixAspect;
    } else { // coefficients for backward
        tr->dxc = xCenter + ((-xCenter + dx) * cosinus - (-yCenter / pixAspect + dy) * sinus) * zoom;
        tr->dyc = yCenter + ((-yCenter / pixAspect + dy) * cosinus + (-xCenter + dx) * sinus) * zoom * pixAspect;
    }
}

static void transform2Motion(const Transform * tr, const bool forward, const float xCenter, const float yCenter, const float pixAspect,
                             float * dx, float * dy, float * rotation, float * zoom) {
    const float rotradian = -std::atan(pixAspect * tr->dxy / tr->dxx);
    *rotation = rotradian * 180.f / PI;
    const float sinus = std::sin(rotradian);
    const float cosinus = std::cos(rotradian);
    *zoom = tr->dxx / cosinus;

    if (forward) { // coefficients for forward
        *dx = tr->dxc - xCenter - (-xCenter * cosinus + yCenter / pixAspect * sinus) * *zoom;
        *dy = tr->dyc / pixAspect - yCenter / pixAspect - (-yCenter / pixAspect * cosinus + -xCenter * sinus) * *zoom;
    } else { // coefficients for backward
        *dx = tr->dxc / *zoom * cosinus + tr->dyc / *zoom / pixAspect * sinus - xCenter / *zoom * cosinus + xCenter - yCenter / *zoom / pixAspect * sinus;
        *dy = -tr->dxc / *zoom * sinus + tr->dyc / *zoom / pixAspect * cosinus + xCenter / *zoom * sinus - -yCenter / pixAspect - yCenter / *zoom / pixAspect * cosinus;
    }
}

static void sumTransform(const Transform * ta, const Transform * tb, Transform * tba) {
    tba->dxc = tb->dxc + tb->dxx * ta->dxc + tb->dxy * ta->dyc;
    tba->dxx = tb->dxx * ta->dxx + tb->dxy * ta->dyx;
    tba->dxy = tb->dxx * ta->dxy + tb->dxy * ta->dyy;
    tba->dyc = tb->dyc + tb->dyx * ta->dxc + tb->dyy * ta->dyc;
    tba->dyx = tb->dyx * ta->dxx + tb->dyy * ta->dyx;
    tba->dyy = tb->dyx * ta->dxy + tb->dyy * ta->dyy;
}

template<typename T>
static void compensate(const VSFrameRef * src, VSFrameRef * dst, const int plane, const Transform * tr, int * VS_RESTRICT work2Width1030, const DePanData * d, const VSAPI * vsapi) {
    const int width = vsapi->getFrameWidth(src, plane);
    const int height = vsapi->getFrameHeight(src, plane);
    const int stride = vsapi->getStride(src, plane) / sizeof(T);
    const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
    T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));
    const T border = d->border[plane];

    int * VS_RESTRICT ix4Work = work2Width1030 + width;
    int * VS_RESTRICT intCoef = ix4Work + width;
    int intCoef2D[16];

    // float coeff. are changed by integer coeff. scaled by 256*256*256/8192 = 2048
    for (int i = 0; i < 257; i++) {
        intCoef[i * 4] = -((i * (256 - i) * (256 - i))) / 8192;
        intCoef[i * 4 + 1] = (256 * 256 * 256 - 2 * 256 * i * i + i * i * i) / 8192;
        intCoef[i * 4 + 2] = (i * (256 * 256 + 256 * i - i * i)) / 8192;
        intCoef[i * 4 + 3] = -(i * i * (256 - i)) / 8192;
    }

    if (tr->dxy == 0 && tr->dyx == 0 && tr->dxx == 1 && tr->dyy == 1) { // only translation - fast
        for (int y = 0; y < height; y++) {
            const float ySrc = tr->dyc + y;
            const int inttr3 = static_cast<int>(std::floor(tr->dyc));
            int hLow = static_cast<int>(std::floor(ySrc));
            const int iy4 = 4 * static_cast<int>((ySrc - hLow) * 256.f);

            const int inttr0 = static_cast<int>(std::floor(tr->dxc));
            const int ix4 = 4 * static_cast<int>((tr->dxc - inttr0) * 256.f);

            for (int j = 0; j < 4; j++) {
                for (int i = 0; i < 4; i++)
                    intCoef2D[j * 4 + i] = intCoef[j + iy4] * intCoef[i + ix4] / 2048; // 16 coeff. for bicubic 2D, scaled by 2048
            }

            // mirror borders
            if (hLow < 0 && d->mTop)
                hLow = -hLow;
            if (hLow >= height && d->mBottom)
                hLow = height * 2 - hLow - 2;

            const int w0 = stride * hLow;

            if (hLow >= 1 && hLow < height - 2) { // middle lines
                for (int x = 0; x < width; x++) {
                    const int rowLeft = inttr0 + x;

                    if (rowLeft >= 1 && rowLeft < width - 2) {
                        const int w = w0 + rowLeft;
                        const int pixel = (intCoef2D[0] * srcp[w - stride - 1] + intCoef2D[1] * srcp[w - stride] + intCoef2D[2] * srcp[w - stride + 1] + intCoef2D[3] * srcp[w - stride + 2] + intCoef2D[4] * srcp[w - 1] + intCoef2D[5] * srcp[w] + intCoef2D[6] * srcp[w + 1] + intCoef2D[7] * srcp[w + 2] + intCoef2D[8] * srcp[w + stride - 1] + intCoef2D[9] * srcp[w + stride] + intCoef2D[10] * srcp[w + stride + 1] + intCoef2D[11] * srcp[w + stride + 2] + intCoef2D[12] * srcp[w + stride * 2 - 1] + intCoef2D[13] * srcp[w + stride * 2] + intCoef2D[14] * srcp[w + stride * 2 + 1] + intCoef2D[15] * srcp[w + stride * 2 + 2] + 1024) / 2048;
                        dstp[x] = std::min(std::max(pixel, 0), d->peak);
                    } else if (rowLeft < 0 && d->mLeft) {
                        if (d->blur > 0) {
                            const int blurLen = std::min(d->blur, -rowLeft);
                            int smoothed = 0;
                            for (int i = -rowLeft - blurLen + 1; i <= -rowLeft; i++)
                                smoothed += srcp[w0 + i];
                            dstp[x] = smoothed / blurLen;
                        } else { // no blur
                            dstp[x] = srcp[w0 - rowLeft]; // not very precise - may be bicubic?
                        }
                    } else if (rowLeft >= width && d->mRight) {
                        if (d->blur > 0) {
                            const int blurLen = std::min(d->blur, rowLeft - width + 1);
                            int smoothed = 0;
                            for (int i = width * 2 - rowLeft - 2; i < width * 2 - rowLeft - 2 + blurLen; i++)
                                smoothed += srcp[w0 + i];
                            dstp[x] = smoothed / blurLen;
                        } else { // no blur
                            dstp[x] = srcp[w0 + width * 2 - rowLeft - 2]; // not very precise - may be bicubic?
                        }
                    } else if (rowLeft == 0 || rowLeft == width - 1 || rowLeft == width - 2) { // edges
                        dstp[x] = srcp[w0 + rowLeft];
                    } else { // if shifted point is out of frame, fill using border value
                        dstp[x] = border;
                    }
                }
            } else if (hLow == 0 || hLow == height - 2) { // near edge (top-1, bottom-1) lines
                for (int x = 0; x < width; x++) {
                    const int rowLeft = inttr0 + x;
                    const float sx = tr->dxc - inttr0;
                    const float sy = tr->dyc - inttr3;

                    if (rowLeft >= 0 && rowLeft < width - 1) {
                        const int w = w0 + rowLeft;
                        dstp[x] = static_cast<int>((1.f - sy) * ((1.f - sx) * srcp[w] + sx * srcp[w + 1]) + sy * ((1.f - sx) * srcp[w + stride] + sx * srcp[w + stride + 1])); // bilinear
                    } else if (rowLeft == width - 1) {
                        dstp[x] = srcp[rowLeft + w0];
                    } else if (rowLeft < 0 && d->mLeft) {
                        dstp[x] = srcp[w0 - rowLeft]; // not very precise - may be bicubic?
                    } else if (rowLeft >= width && d->mRight) {
                        dstp[x] = srcp[w0 + width * 2 - rowLeft - 2];
                    } else { // left or right
                        dstp[x] = border;
                    }
                }
            } else if (hLow == height - 1) { // bottom line
                for (int x = 0; x < width; x++) {
                    const int rowLeft = inttr0 + x;

                    if (rowLeft >= 0 && rowLeft < width)
                        dstp[x] = srcp[w0 + rowLeft];
                    else if (rowLeft < 0 && d->mLeft)
                        dstp[x] = srcp[w0 - rowLeft]; // not very precise - may be bicubic?
                    else if (rowLeft >= width && d->mRight)
                        dstp[x] = srcp[w0 + width * 2 - rowLeft - 2];
                    else // left or right
                        dstp[x] = border;
                }
            } else { // out lines
                for (int x = 0; x < width; x++)
                    dstp[x] = border;
            }

            dstp += stride;
        }
    } else if (tr->dxy == 0 && tr->dyx == 0) { // no rotation, only zoom and translation - fast
        // prepare positions (they are not dependent from y) for fast processing
        for (int x = 0; x < width; x++) {
            const float xSrc = tr->dxc + tr->dxx * x;
            work2Width1030[x] = static_cast<int>(std::floor(xSrc));
            const int rowLeft = work2Width1030[x];
            ix4Work[x] = 4 * static_cast<int>((xSrc - rowLeft) * 256.f);
        }

        for (int y = 0; y < height; y++) {
            const float ySrc = tr->dyc + tr->dyy * y;
            int hLow = static_cast<int>(std::floor(ySrc));
            const int iy4 = 4 * static_cast<int>((ySrc - hLow) * 256.f);
            const float sy = ySrc - hLow;

            // mirror borders
            if (hLow < 0 && d->mTop)
                hLow = -hLow;
            if (hLow >= height && d->mBottom)
                hLow = height * 2 - hLow - 2;

            const int w0 = stride * hLow;

            if (hLow >= 1 && hLow < height - 2) { // inside
                for (int x = 0; x < width; x++) {
                    const int rowLeft = work2Width1030[x];

                    if (rowLeft >= 1 && rowLeft < width - 2) {
                        const int ix4 = ix4Work[x];
                        const int w = w0 + rowLeft;
                        int ts[4];

                        srcp -= stride; // prev line
                        ts[0] = intCoef[ix4] * srcp[w - 1] + intCoef[ix4 + 1] * srcp[w] + intCoef[ix4 + 2] * srcp[w + 1] + intCoef[ix4 + 3] * srcp[w + 2];
                        srcp += stride; // next line
                        ts[1] = intCoef[ix4] * srcp[w - 1] + intCoef[ix4 + 1] * srcp[w] + intCoef[ix4 + 2] * srcp[w + 1] + intCoef[ix4 + 3] * srcp[w + 2];
                        srcp += stride; // next line
                        ts[2] = intCoef[ix4] * srcp[w - 1] + intCoef[ix4 + 1] * srcp[w] + intCoef[ix4 + 2] * srcp[w + 1] + intCoef[ix4 + 3] * srcp[w + 2];
                        srcp += stride; // next line
                        ts[3] = intCoef[ix4] * srcp[w - 1] + intCoef[ix4 + 1] * srcp[w] + intCoef[ix4 + 2] * srcp[w + 1] + intCoef[ix4 + 3] * srcp[w + 2];
                        srcp -= stride * 2; // restore pointer

                        const int pixel = (intCoef[iy4] * ts[0] + intCoef[iy4 + 1] * ts[1] + intCoef[iy4 + 2] * ts[2] + intCoef[iy4 + 3] * ts[3]) >> 22;
                        dstp[x] = std::min(std::max(pixel, 0), d->peak);
                    } else if (rowLeft < 0 && d->mLeft) {
                        if (d->blur > 0) {
                            const int blurLen = std::min(d->blur, -rowLeft);
                            int smoothed = 0;
                            for (int i = -rowLeft - blurLen + 1; i <= -rowLeft; i++)
                                smoothed += srcp[w0 + i];
                            dstp[x] = smoothed / blurLen;
                        } else { // no blur
                            dstp[x] = srcp[w0 - rowLeft]; // not very precise - may be bicubic?
                        }
                    } else if (rowLeft >= width && d->mRight) {
                        if (d->blur > 0) {
                            const int blurLen = std::min(d->blur, rowLeft - width + 1);
                            int smoothed = 0;
                            for (int i = width * 2 - rowLeft - 2; i < width * 2 - rowLeft - 2 + blurLen; i++)
                                smoothed += srcp[w0 + i];
                            dstp[x] = smoothed / blurLen;
                        } else { // no blur
                            dstp[x] = srcp[w0 + width * 2 - rowLeft - 2]; // not very precise - may be bicubic?
                        }
                    } else if (rowLeft == 0 || rowLeft == width - 1 || rowLeft == width - 2) { // edges
                        dstp[x] = srcp[w0 + rowLeft];
                    } else { // if shifted point is out of frame, fill using border value
                        dstp[x] = border;
                    }
                }
            } else if (hLow == 0 || hLow == height - 2) { // near edge (top-1, bottom-1) lines
                for (int x = 0; x < width; x++) {
                    const int rowLeft = work2Width1030[x];

                    if (rowLeft >= 0 && rowLeft < width - 1) {
                        const float xSrc = tr->dxc + tr->dxx * x;
                        const float sx = xSrc - rowLeft;
                        const int w = w0 + rowLeft;
                        const int pixel = static_cast<int>((1.f - sy) * ((1.f - sx) * srcp[w] + sx * srcp[w + 1]) + sy * ((1.f - sx) * srcp[w + stride] + sx * srcp[w + stride + 1])); // bilinear
                        dstp[x] = std::min(std::max(pixel, 0), d->peak);
                    } else if (rowLeft == width - 1) {
                        dstp[x] = srcp[rowLeft + w0];
                    } else if (rowLeft < 0 && d->mLeft) {
                        dstp[x] = srcp[w0 - rowLeft]; // not very precise - may be bicubic?
                    } else if (rowLeft >= width && d->mRight) {
                        dstp[x] = srcp[w0 + width * 2 - rowLeft - 2];
                    } else { // left or right
                        dstp[x] = border;
                    }
                }
            } else if (hLow == height - 1) { // bottom line
                for (int x = 0; x < width; x++) {
                    const int rowLeft = work2Width1030[x];

                    if (rowLeft >= 0 && rowLeft < width) {
                        dstp[x] = (srcp[w0 + rowLeft] + srcp[w0 + rowLeft - stride] + 1) / 2; // for some smoothing
                    } else if (rowLeft < 0 && d->mLeft) {
                        dstp[x] = srcp[w0 - rowLeft];
                    } else if (rowLeft >= width && d->mRight) {
                        dstp[x] = srcp[w0 + width * 2 - rowLeft - 2];
                    } else { // left or right
                        dstp[x] = border;
                    }
                }
            } else { // out lines
                for (int x = 0; x < width; x++)
                    dstp[x] = border;
            }

            dstp += stride;
        }
    } else { // rotation, zoom and translation - slow
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const float xSrc = tr->dxc + tr->dxx * x + tr->dxy * y;
                const float ySrc = tr->dyc + tr->dyx * x + tr->dyy * y;

                int rowLeft = static_cast<int>(xSrc);
                if (xSrc < rowLeft)
                    rowLeft -= 1;

                int hLow = static_cast<int>(ySrc);
                if (ySrc < hLow)
                    hLow -= 1;

                if (rowLeft >= 1 && rowLeft < width - 2 && hLow >= 1 && hLow < height - 2) {
                    const int ix4 = 4 * static_cast<int>((xSrc - rowLeft) * 256.f);
                    const int w0 = rowLeft + stride * hLow;
                    int ts[4];

                    srcp -= stride; // prev line
                    ts[0] = intCoef[ix4] * srcp[w0 - 1] + intCoef[ix4 + 1] * srcp[w0] + intCoef[ix4 + 2] * srcp[w0 + 1] + intCoef[ix4 + 3] * srcp[w0 + 2];
                    srcp += stride; // next line
                    ts[1] = intCoef[ix4] * srcp[w0 - 1] + intCoef[ix4 + 1] * srcp[w0] + intCoef[ix4 + 2] * srcp[w0 + 1] + intCoef[ix4 + 3] * srcp[w0 + 2];
                    srcp += stride; // next line
                    ts[2] = intCoef[ix4] * srcp[w0 - 1] + intCoef[ix4 + 1] * srcp[w0] + intCoef[ix4 + 2] * srcp[w0 + 1] + intCoef[ix4 + 3] * srcp[w0 + 2];
                    srcp += stride; // next line
                    ts[3] = intCoef[ix4] * srcp[w0 - 1] + intCoef[ix4 + 1] * srcp[w0] + intCoef[ix4 + 2] * srcp[w0 + 1] + intCoef[ix4 + 3] * srcp[w0 + 2];
                    srcp -= stride * 2; // restore pointer

                    const int iy4 = static_cast<int>((ySrc - hLow) * 256.f) * 4;
                    const int pixel = (intCoef[iy4] * ts[0] + intCoef[iy4 + 1] * ts[1] + intCoef[iy4 + 2] * ts[2] + intCoef[iy4 + 3] * ts[3]) >> 22;
                    dstp[x] = std::min(std::max(pixel, 0), d->peak);
                } else {
                    // mirror borders
                    if (hLow < 0 && d->mTop)
                        hLow = -hLow;
                    if (hLow >= height && d->mBottom)
                        hLow = height * 2 - hLow - 2;
                    if (rowLeft < 0 && d->mLeft)
                        rowLeft = -rowLeft;
                    if (rowLeft >= width && d->mRight)
                        rowLeft = width * 2 - rowLeft - 2;

                    // check mirrowed
                    if (rowLeft >= 0 && rowLeft < width && hLow >= 0 && hLow < height)
                        dstp[x] = srcp[stride * hLow + rowLeft];
                    else // if shifted point is out of frame, fill using border value
                        dstp[x] = border;
                }
            }

            dstp += stride;
        }
    }
}

static void VS_CC depanInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DePanData * d = static_cast<DePanData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC depanGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    const DePanData * d = static_cast<const DePanData *>(*instanceData);
    const int nSrc = n - d->intOffset;

    if (activationReason == arInitial) {
        if (nSrc < 0 || nSrc >= d->vi->numFrames) { // no frames before 0 or after last
            vsapi->requestFrameFilter(n, d->node, frameCtx);
            return nullptr;
        }

        vsapi->requestFrameFilter(nSrc, d->node, frameCtx);

        if (d->intOffset > 0) { // FORWARD compensation from some previous frame
            for (int i = n; i > nSrc; i--)
                vsapi->requestFrameFilter(i, d->data, frameCtx);
        } else { // BACKWARD compensation from some next frame
            for (int i = n + 1; i <= nSrc; i++)
                vsapi->requestFrameFilter(i, d->data, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        if (nSrc < 0 || nSrc >= d->vi->numFrames) // no frames before 0 or after last
            return vsapi->getFrameFilter(n, d->node, frameCtx); // return current source frame

        int * work2Width1030 = vs_aligned_malloc<int>((d->vi->width * 2 + 1030) * sizeof(int), 32);
        if (!work2Width1030) {
            vsapi->setFilterError("DePan: malloc failure (work2Width1030)", frameCtx);
            return nullptr;
        }

        const VSFrameRef * src = vsapi->getFrameFilter(nSrc, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        int err, numFields;
        int field = int64ToIntS(vsapi->propGetInt(vsapi->getFramePropsRO(src), "_Field", 0, &err));
        if (err) {
            field = -1;
            numFields = 1;
        } else {
            numFields = 2;
        }

        Transform tr, trNull, trSum, trSumUV;

        motion2Transform(0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, true, 1.f, &trNull);
        motion2Transform(0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, true, 1.f, &trSum); // null as initial

        float dxSum, dySum, rotationSum, zoomSum;
        bool motionGood = true;

        if (d->intOffset > 0) { // FORWARD compensation from some previous frame
            const float fractureOffset = d->offset + 1.f - d->intOffset; // 0 < f <= 1

            for (int i = n; i > nSrc; i--) {
                const VSFrameRef * data = vsapi->getFrameFilter(i, d->data, frameCtx);
                const VSMap * props = vsapi->getFramePropsRO(data);

                int errX, errY, errZoom;
                const float motionx = static_cast<float>(vsapi->propGetFloat(props, "DePanEstimateDx", 0, &errX));
                const float motiony = static_cast<float>(vsapi->propGetFloat(props, "DePanEstimateDy", 0, &errY));
                const float motionZoom = static_cast<float>(vsapi->propGetFloat(props, "DePanEstimateZoom", 0, &errZoom));
                if (errX || errY || errZoom) {
                    vsapi->setFilterError("DePan: data clip is not good DePanEstimate clip", frameCtx);
                    vsapi->freeFrame(src);
                    vsapi->freeFrame(dst);
                    vsapi->freeFrame(data);
                    return nullptr;
                }
                vsapi->freeFrame(data);

                motion2Transform(motionx, motiony, 0.f, motionZoom, d->pixAspect / numFields, d->xCenter, d->yCenter, true, fractureOffset, &tr);
                sumTransform(&trSum, &tr, &trSum);

                if (motionx == MOTION_BAD)
                    motionGood = false; // if any strictly =0, then no good
            }

            if (!motionGood)
                sumTransform(&trNull, &trNull, &trSum); // null transform if any is no good

            if (field != -1 && d->matchFields) { // reverse 1 line motion correction if matchfields mode
                const int isNOdd = n & 1; // =1 for odd field number, =0 for even field number
                trSum.dyc -= (field == 1) ? 0.5f - isNOdd : -0.5f + isNOdd;
            }

            // summary motion from summary transform
            transform2Motion(&trSum, true, d->xCenter, d->yCenter, d->pixAspect / numFields, &dxSum, &dySum, &rotationSum, &zoomSum);

            VSMap * props = vsapi->getFramePropsRW(dst);
            vsapi->propSetFloat(props, "DePanDx", dxSum, paReplace);
            vsapi->propSetFloat(props, "DePanDy", dySum, paReplace);
            vsapi->propSetFloat(props, "DePanZoom", zoomSum, paReplace);
        } else { // BACKWARD compensation from some next frame
            const float fractureOffset = d->offset - 1.f - d->intOffset; // -1 <= f < 0

            for (int i = n + 1; i <= nSrc; i++) {
                const VSFrameRef * data = vsapi->getFrameFilter(i, d->data, frameCtx);
                const VSMap * props = vsapi->getFramePropsRO(data);

                int errX, errY, errZoom;
                const float motionx = static_cast<float>(vsapi->propGetFloat(props, "DePanEstimateDx", 0, &errX));
                const float motiony = static_cast<float>(vsapi->propGetFloat(props, "DePanEstimateDy", 0, &errY));
                const float motionZoom = static_cast<float>(vsapi->propGetFloat(props, "DePanEstimateZoom", 0, &errZoom));
                if (errX || errY || errZoom) {
                    vsapi->setFilterError("DePan: data clip is not good DePanEstimate clip", frameCtx);
                    vsapi->freeFrame(src);
                    vsapi->freeFrame(dst);
                    vsapi->freeFrame(data);
                    return nullptr;
                }
                vsapi->freeFrame(data);

                motion2Transform(motionx, motiony, 0.f, motionZoom, d->pixAspect / numFields, d->xCenter, d->yCenter, false, fractureOffset, &tr);
                sumTransform(&trSum, &tr, &trSum);

                if (motionx == MOTION_BAD)
                    motionGood = false; // if any strictly =0, then no good
            }

            if (!motionGood)
                sumTransform(&trNull, &trNull, &trSum); // null transform if any is no good

            if (field != -1 && d->matchFields) { // reverse 1 line motion correction if matchfields mode
                const int isNOdd = n & 1; // =1 for odd field number, =0 for even field number
                trSum.dyc -= (field == 1) ? 0.5f - isNOdd : -0.5f + isNOdd;
            }

            // summary motion from summary transform
            transform2Motion(&trSum, false, d->xCenter, d->yCenter, d->pixAspect / numFields, &dxSum, &dySum, &rotationSum, &zoomSum);

            VSMap * props = vsapi->getFramePropsRW(dst);
            vsapi->propSetFloat(props, "DePanDx", dxSum, paReplace);
            vsapi->propSetFloat(props, "DePanDy", dySum, paReplace);
            vsapi->propSetFloat(props, "DePanZoom", zoomSum, paReplace);
        }

        // scale transform for UV
        trSumUV.dxc = trSum.dxc / (1 << d->vi->format->subSamplingW);
        trSumUV.dxx = trSum.dxx;
        trSumUV.dxy = trSum.dxy;
        trSumUV.dyc = trSum.dyc / (1 << d->vi->format->subSamplingH);
        trSumUV.dyx = trSum.dyx;
        trSumUV.dyy = trSum.dyy;

        if (d->vi->format->bitsPerSample == 8) {
            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (d->process[plane]) {
                    if (plane == 0)
                        compensate<uint8_t>(src, dst, plane, &trSum, work2Width1030, d, vsapi);
                    else
                        compensate<uint8_t>(src, dst, plane, &trSumUV, work2Width1030, d, vsapi);
                }
            }
        } else {
            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (d->process[plane]) {
                    if (plane == 0)
                        compensate<uint16_t>(src, dst, plane, &trSum, work2Width1030, d, vsapi);
                    else
                        compensate<uint16_t>(src, dst, plane, &trSumUV, work2Width1030, d, vsapi);
                }
            }
        }

        vs_aligned_free(work2Width1030);
        vsapi->freeFrame(src);
        return dst;
    }

    return nullptr;
}

static void VS_CC depanFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DePanData * d = static_cast<DePanData *>(instanceData);

    vsapi->freeNode(d->node);
    vsapi->freeNode(d->data);

    delete d;
}

static void VS_CC depanCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    DePanData d;
    int err;

    d.offset = static_cast<float>(vsapi->propGetFloat(in, "offset", 0, &err));

    d.pixAspect = static_cast<float>(vsapi->propGetFloat(in, "pixaspect", 0, &err));
    if (err)
        d.pixAspect = 1.f;

    d.matchFields = !!vsapi->propGetInt(in, "matchfields", 0, &err);
    if (err)
        d.matchFields = true;

    d.mirror = int64ToIntS(vsapi->propGetInt(in, "mirror", 0, &err));

    d.blur = int64ToIntS(vsapi->propGetInt(in, "blur", 0, &err));

    if (d.offset < -10.f || d.offset > 10.f) {
        vsapi->setError(out, "DePan: offset must be between -10.0 and 10.0 (inclusive)");
        return;
    }

    if (d.mirror < 0 || d.mirror > 15) {
        vsapi->setError(out, "DePan: mirror must be between 0 and 15 (inclusive)");
        return;
    }

    if (d.blur < 0) {
        vsapi->setError(out, "DePan: blur must be greater than or equal to 0");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.data = vsapi->propGetNode(in, "data", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bitsPerSample > 16) {
        vsapi->setError(out, "DePan: only constant format 8-16 bits integer input supported");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.data);
        return;
    }

    if (d.vi->format->colorFamily == cmRGB) {
        vsapi->setError(out, "DePan: RGB color family is not supported");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.data);
        return;
    }

    if (vsapi->getVideoInfo(d.data)->numFrames != d.vi->numFrames) {
        vsapi->setError(out, "DePan: data clip's number of frames doesn't match");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.data);
        return;
    }

    // integer of offset, get frame for motion compensation
    if (d.offset > 0.f)
        d.intOffset = static_cast<int>(std::ceil(d.offset)); // = 1 for both 0.5 and 1
    else
        d.intOffset = static_cast<int>(std::floor(d.offset)); // = -1 for both -0.5 and -1

    if (d.intOffset == 0) { // NULL transform, return source
        vsapi->propSetNode(out, "clip", d.node, paReplace);
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.data);
        return;
    }

    const int m = vsapi->propNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        d.process[i] = m <= 0;

    for (int i = 0; i < m; i++) {
        const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (n < 0 || n >= d.vi->format->numPlanes) {
            vsapi->setError(out, "DePan: plane index out of range");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.data);
            return;
        }

        if (d.process[n]) {
            vsapi->setError(out, "DePan: plane specified twice");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.data);
            return;
        }

        d.process[n] = true;
    }

    d.peak = (1 << d.vi->format->bitsPerSample) - 1;

    d.xCenter = d.vi->width / 2.f;
    d.yCenter = d.vi->height / 2.f;

    d.mTop = !!(d.mirror & mirrorTop);
    d.mBottom = !!(d.mirror & mirrorBottom);
    d.mLeft = !!(d.mirror & mirrorLeft);
    d.mRight = !!(d.mirror & mirrorRight);

    d.border[0] = 0;
    d.border[1] = d.border[2] = 1 << (d.vi->format->bitsPerSample - 1);

    DePanData * data = new DePanData(d);

    vsapi->createFilter(in, out, "DePan", depanInit, depanGetFrame, depanFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.depan", "depan", "Tools for estimation and compensation of global motion (pan)", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("DePanEstimate",
                 "clip:clip;"
                 "range:int:opt;"
                 "trust:float:opt;"
                 "winx:int:opt;"
                 "winy:int:opt;"
                 "wleft:int:opt;"
                 "wtop:int:opt;"
                 "dxmax:int:opt;"
                 "dymax:int:opt;"
                 "zoommax:float:opt;"
                 "stab:float:opt;"
                 "pixaspect:float:opt;",
                 estimateCreate, nullptr, plugin);
    registerFunc("DePan",
                 "clip:clip;"
                 "data:clip;"
                 "offset:float:opt;"
                 "pixaspect:float:opt;"
                 "matchfields:int:opt;"
                 "mirror:int:opt;"
                 "blur:int:opt;"
                 "planes:int[]:opt;",
                 depanCreate, nullptr, plugin);
}
