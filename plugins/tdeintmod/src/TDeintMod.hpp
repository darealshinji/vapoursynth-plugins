#pragma once

#include <algorithm>
#include <array>
#include <limits>

#include <VapourSynth.h>
#include <VSHelper.h>

#ifdef VS_TARGET_CPU_X86
#include "vectorclass/vectorclass.h"
#endif

struct TDeintModData {
    VSNodeRef * node, * node2, * propNode, * mask, * edeint;
    VSVideoInfo vi;
    const VSVideoInfo * viSaved;
    int order, field, mode, length, mtype, ttype, mtqL, mthL, mtqC, mthC, nt, minthresh, maxthresh, cstr, athresh, metric, expand;
    bool link, show, process[3];
    int hShift[3], vShift[3], hHalf[3], vHalf[3], athresh6, athreshsq, widthPad, peak;
    uint8_t * gvlut;
    std::array<uint8_t, 64> vlut;
    std::array<uint8_t, 16> tmmlut16;
    const VSFormat * format;
    void (*copyPad)(const VSFrameRef *, VSFrameRef *, const int, const int, const VSAPI *);
    void (*threshMask)(const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *);
    void (*motionMask)(const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *);
    void (*andMasks)(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *);
    void (*combineMasks)(const VSFrameRef *, VSFrameRef *, const int, const TDeintModData *, const VSAPI *);
    void (*buildMask)(VSFrameRef **, VSFrameRef **, VSFrameRef *, const int, const int, const int, const int, const TDeintModData *, const VSAPI *);
    void (*setMaskForUpsize)(VSFrameRef *, const int, const TDeintModData *, const VSAPI *);
    void (*checkSpatial)(const VSFrameRef *, VSFrameRef *, const TDeintModData *, const VSAPI *);
    void (*expandMask)(VSFrameRef *, const int, const TDeintModData *, const VSAPI *);
    void (*linkMask)(VSFrameRef *, const int, const TDeintModData *, const VSAPI *);
    void (*eDeint)(VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const TDeintModData *, const VSAPI *);
    void (*cubicDeint)(VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const VSFrameRef *, const TDeintModData *, const VSAPI *);
    void (*binaryMask)(const VSFrameRef *, VSFrameRef *, const TDeintModData *, const VSAPI *);
};
