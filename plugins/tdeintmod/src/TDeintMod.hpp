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
    VSNodeRef * node, *node2, *propNode, *mask, *edeint;
    VSVideoInfo vi;
    const VSVideoInfo * viSaved;
    int order, field, mode, length, mtype, ttype, mtqL, mthL, mtqC, mthC, nt, minthresh, maxthresh, cstr;
    bool show, process[3];
    int hShift[3], vShift[3], hHalf[3], vHalf[3];
    uint8_t * gvlut;
    std::array<uint8_t, 64> vlut;
    std::array<uint16_t, 16> tmmlut16;
    uint16_t ten, twenty, thirty, forty, fifty, sixty, seventy, peak;
    const VSFormat * format;
    uint8_t widthPad;
};
