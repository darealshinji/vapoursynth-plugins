// VapourSynth port by HolyWu
//
// Copyright (c) 2002 Tom Barry.  All rights reserved.
//      trbarry@trbarry.com
//  modified by Foxyshadis
//      foxyshadis@hotmail.com
//  modified by Firesledge
//      http://ldesoras.free.fr
//  modified by LaTo INV.
//      http://forum.doom9.org/member.php?u=131032
// Requires Avisynth source code to compile for Avisynth
// Avisynth Copyright 2000 Ben Rudiak-Gould.
//      http://www.math.berkeley.edu/~benrg/avisynth.html
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.  If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
//  
//  Also, this program is "Philanthropy-Ware".  That is, if you like it and 
//  feel the need to reward or inspire the author then please feel free (but
//  not obligated) to consider joining or donating to the Electronic Frontier
//  Foundation. This will help keep cyber space free of barbed wire and bullsh*t.  
//
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Version  Developer      Changes
//
// 07 May 2003   1.0.0.0  Tom Barry      New Release
// 01 Jun 2006   1.1.0.0  Foxyshadis     Chroma noise, constant seed
// 06 Jun 2006   1.2.0.0  Foxyshadis     Supports YUY2, RGB. Fix cache mess.
// 10 Jun 2006   1.3.0.0  Foxyshadis     Crashfix, noisegen optimization
// 11 Nov 2006   1.4.0.0  Foxyshadis     Constant replaces seed, seed repeatable
// 07 May 2010   1.5.0.0  Foxyshadis     Limit the initial seed generation to fix memory issues.
// 13 May 2010   1.5.1.0  Firesledge     The source code compiles on Visual C++ versions older than 2008
// 26 Oct 2011   1.5.2.0  Firesledge     Removed the SSE2 requirement.
// 26 Oct 2011   1.5.3.0  Firesledge     Fixed coloring and bluring in RGB24 mode.
// 27 Oct 2011   1.5.4.0  Firesledge     Fixed bad pixels on the last line in YV12 mode when constant=true,
//                                       fixed potential problems with frame width > 4096 pixels
//                                       and fixed several other minor things.
// 28 Oct 2011   1.6.0.0  LaTo INV.      Added SSE2 code (50% faster than MMX).
// 29 Oct 2011   1.6.1.0  LaTo INV.      Automatic switch to MMX if SSE2 is not supported by the CPU.
// 16 Aug 2012   1.7.0.0  Firesledge     Supports Y8, YV16, YV24 and YV411 colorspaces.
//
/////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <cmath>
#include <ctime>
#include <vector>
#include <VapourSynth.h>
#include <VSHelper.h>

// max # of noise planes
#define MAXP 2

// offset in pixels of the fake plane MAXP relative to plane MAXP-1
#define OFFSET_FAKEPLANE 32

struct AddGrainData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    float var, uvar, hcorr, vcorr;
    long long seed;
    bool constant;
    int storedFrames;
    bool iset;
    float gset;
    long long idum;
    std::vector<uint8_t> pNoiseSeeds;
    // increase this to lessen inter-frame noise coherence and increase memory
    std::vector<int16_t> pN[MAXP];
    int nPitch[MAXP], nHeight[MAXP], nSize[MAXP];
    bool process[3];
};

template<typename T1, typename T2>
static void UpdateFrame(T1 *dstp, int width, int height, int stride, int plane, int noiseOffs, const AddGrainData *d) {
    const int16_t * pNW2 = &(d->pN[plane][noiseOffs]);
    const int noisePitch2 = d->nPitch[plane];
    assert(noiseOffs + noisePitch2 * (height - 1) + width <= d->nSize[plane]);
    const unsigned shift1 = sizeof(T1) == 1 ? 0 : 16 - d->vi->format->bitsPerSample;
    const int shift2 = sizeof(T1) == 1 ? 8 : 0;
    const int lower = sizeof(T1) == 1 ? SCHAR_MIN : SHRT_MIN;
    const int upper = sizeof(T1) == 1 ? SCHAR_MAX : SHRT_MAX;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            T2 val = (dstp[x] << shift1) ^ lower;
            T2 tmp = pNW2[x] >> shift2;
            val = val + tmp < lower ? lower : (val + tmp > upper ? upper : val + tmp);
            dstp[x] = val ^ lower;
            dstp[x] >>= shift1;
        }
        dstp += stride;
        pNW2 += noisePitch2;
    }
}

static inline long long FastUniformRandL(AddGrainData *d) {
    return d->idum = 1664525LL * d->idum + 1013904223LL;
}

// very fast & reasonably random
static inline float FastUniformRandF(AddGrainData *d) {
    // work with 32-bit IEEE floating point only!
    FastUniformRandL(d);
    unsigned long long itemp = 0x3f800000 | (0x007fffff & d->idum);
    return *(float *)itemp - 1.f;
}

static float GaussianRand(AddGrainData *d) {
    float fac, rsq, v1, v2;

    // return saved second
    if (d->iset) {
        d->iset = false;
        return d->gset;
    }

    do {
        v1 = 2.f * FastUniformRandF(d) - 1.f;
        v2 = 2.f * FastUniformRandF(d) - 1.f;
        rsq = v1 * v1 + v2 * v2;
    } while (rsq >= 1.f || rsq == 0.f);

    fac = std::sqrt(-2.f * std::log(rsq) / rsq);

    // function generates two values every iteration, so save one for later
    d->gset = v1 * fac;
    d->iset = true;

    return v2 * fac;
}

static float GaussianRand(float mean, float variance, AddGrainData *d) {
    if (variance == 0.f)
        return mean;
    return GaussianRand(d) * std::sqrt(variance) + mean;
}

// on input, plane is the frame plane index (if applicable, 0 otherwise), and on output, it contains the selected noise plane
static void SetRand(int &plane, int &noiseOffs, int frameNumber, AddGrainData *d) {
    if (d->constant) {
        // force noise to be identical every frame
        if (plane >= MAXP) {
            plane = MAXP - 1;
            noiseOffs = OFFSET_FAKEPLANE;
        }
    } else {
        // pull seed back out, to keep cache happy
        const int seedIndex = frameNumber % d->storedFrames;
        const int p0 = d->pNoiseSeeds[seedIndex];
        if (plane == 0)
            d->idum = p0;
        else {
            d->idum = d->pNoiseSeeds[seedIndex + d->storedFrames];
            if (plane == 2) {
                // the trick to needing only 2 planes ^.~
                d->idum ^= p0;
                plane--;
            }
        }
        // start noise at random qword in top half of noise area
        noiseOffs = int(FastUniformRandF(d) * d->nSize[plane] / MAXP) & 0xfffffff8;
    }
    assert(plane >= 0);
    assert(plane < MAXP);
    assert(noiseOffs >= 0);
    assert(noiseOffs < d->nSize[plane]);    // minimal check
}

static void VS_CC addgrainInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    AddGrainData * d = (AddGrainData *)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC addgrainGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    AddGrainData * d = (AddGrainData *)*instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef * dst = vsapi->copyFrame(src, core);
        vsapi->freeFrame(src);

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (d->process[plane]) {
                const int width = vsapi->getFrameWidth(dst, plane);
                const int height = vsapi->getFrameHeight(dst, plane);
                const int stride = vsapi->getStride(dst, plane);
                uint8_t * dstp = vsapi->getWritePtr(dst, plane);
                int noisePlane = plane;
                int noiseOffs = 0;
                SetRand(noisePlane, noiseOffs, n, d);   // seeds randomness w/ plane & frame

                if (d->vi->format->bytesPerSample == 1) {
                    UpdateFrame<uint8_t, int8_t>(dstp, width, height, stride, noisePlane, noiseOffs, d);
                } else if (d->vi->format->bytesPerSample == 2) {
                    UpdateFrame<uint16_t, int16_t>((uint16_t *)dstp, width, height, stride / 2, noisePlane, noiseOffs, d);
                }
            }
        }

        return dst;
    }

    return nullptr;
}

static void VS_CC addgrainFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    AddGrainData * d = (AddGrainData *)instanceData;
    vsapi->freeNode(d->node);
    delete d;
}

static void VS_CC addgrainCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    AddGrainData d;
    AddGrainData * data;
    int err;

    d.var = (float)vsapi->propGetFloat(in, "var", 0, &err);
    if (err)
        d.var = 1.f;
    d.uvar = (float)vsapi->propGetFloat(in, "uvar", 0, &err);
    d.hcorr = (float)vsapi->propGetFloat(in, "hcorr", 0, &err);
    d.vcorr = (float)vsapi->propGetFloat(in, "vcorr", 0, &err);
    d.seed = vsapi->propGetInt(in, "seed", 0, &err);
    if (err)
        d.seed = -1;
    d.constant = !!vsapi->propGetInt(in, "constant", 0, &err);

    if (d.hcorr < 0.f || d.hcorr > 1.f || d.vcorr < 0.f || d.vcorr > 1.f) {
        vsapi->setError(out, "AddGrain: hcorr & vcorr must be 0.0 <= x <= 1.0");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bytesPerSample > 2) {
        vsapi->setError(out, "AddGrain: only constant format 8-16 bits integer input supported");
        vsapi->freeNode(d.node);
        return;
    }

    d.iset = false;
    if (d.seed < 0)
        d.seed = std::time(nullptr);    // init random
    d.idum = d.seed;

    int planesNoise = 1;
    d.nPitch[0] = d.vi->width;  // first plane
    d.nHeight[0] = d.vi->height;
    if (d.vi->format->numPlanes == 1)
        d.uvar = 0.f;
    else {
        planesNoise = 2;
        d.nPitch[1] = d.vi->width >> d.vi->format->subSamplingW;    // second and third plane
        d.nHeight[1] = d.vi->height >> d.vi->format->subSamplingH;
    }
    d.storedFrames = VSMIN(d.vi->numFrames, 256);
    d.pNoiseSeeds.resize(d.storedFrames * planesNoise);
    std::vector<uint8_t>::iterator pns = d.pNoiseSeeds.begin();
    float nRep[] = { 2.f, 2.f };
    if (d.constant) {
        nRep[0] = 1.f;
        nRep[1] = 1.f;
    }

    const float pvar[] = { d.var, d.uvar };
    std::vector<float> lastLine(d.nPitch[0]);   // assume plane 0 is the widest one
    const float mean = 0.f;
    for (int plane = 0; plane < planesNoise; plane++) {
        int h = (int)ceil(d.nHeight[plane] * nRep[plane]);
        if (planesNoise == 2 && plane == 1) {
            // fake plane needs at least one more row, and more if the rows are too small. round to the upper number
            h += (OFFSET_FAKEPLANE + d.nPitch[plane] - 1) / d.nPitch[plane];
        }
        d.nSize[plane] = d.nPitch[plane] * h;
        // allocate space for noise
        d.pN[plane].resize(d.nSize[plane]);
        for (int x = 0; x < d.nPitch[plane]; x++)
            lastLine[x] = GaussianRand(mean, pvar[plane], &d);  // things to vertically smooth against
        for (int y = 0; y < h; y++) {
            std::vector<int16_t>::iterator pNW = d.pN[plane].begin() + y * d.nPitch[plane];
            float lastr = GaussianRand(mean, pvar[plane], &d);  // something to horiz smooth against
            for (int x = 0; x < d.nPitch[plane]; x++) {
                float r = GaussianRand(mean, pvar[plane], &d);
                r = lastr * d.hcorr + r * (1.f - d.hcorr);  // horizontal correlation
                lastr = r;
                r = lastLine[x] * d.vcorr + r * (1.f - d.vcorr);    // vert corr
                lastLine[x] = r;
                // set noise block
                *pNW++ = (int16_t)std::round(r * 256.f);
            }
        }
        for (int x = d.storedFrames; x > 0; x--)
            *pns++ = FastUniformRandL(&d) & 0xff;   // insert seed, to keep cache happy
    }

    d.process[0] = d.var > 0.f;
    d.process[1] = d.uvar > 0.f;
    d.process[2] = d.process[1];

    data = new AddGrainData;
    *data = d;

    vsapi->createFilter(in, out, "AddGrain", addgrainInit, addgrainGetFrame, addgrainFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.addgrain", "grain", "Add some correlated color gaussian noise", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Add", "clip:clip;var:float:opt;uvar:float:opt;hcorr:float:opt;vcorr:float:opt;seed:int:opt;constant:int:opt;", addgrainCreate, nullptr, plugin);
}
