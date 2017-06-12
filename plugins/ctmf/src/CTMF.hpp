#pragma once

#include <algorithm>
#include <thread>
#include <unordered_map>

#include <VapourSynth.h>
#include <VSHelper.h>

#ifdef VS_TARGET_CPU_X86
#include "vectorclass/vectorclass.h"
#endif

struct CTMFData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    int radius;
    bool process[3];
    uint16_t bins, shiftRight, mask, t;
    int stripeSize[3];
    bool specialRadius2;
    uint8_t widthPad;
    std::unordered_map<std::thread::id, uint16_t *> hCoarse, hFine;
};

template<uint16_t bins>
struct alignas(32) Histogram {
    uint16_t coarse[bins];
    uint16_t fine[bins][bins];
};
