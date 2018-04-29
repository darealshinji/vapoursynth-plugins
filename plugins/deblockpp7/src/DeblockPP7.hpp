#pragma once

#include <algorithm>
#include <thread>
#include <unordered_map>

#include <VapourSynth.h>
#include <VSHelper.h>

#ifdef VS_TARGET_CPU_X86
#define MAX_VECTOR_SIZE 128
#include "vectorclass/vectorclass.h"
#endif

static constexpr int N = 1 << 16;
static constexpr int N0 = 4;
static constexpr int N1 = 5;
static constexpr int N2 = 10;
static constexpr int SN0 = 2;
static constexpr double SN2 = 3.1622776601683795;

struct DeblockPP7Data {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    int mode;
    bool process[3];
    int stride[3];
    unsigned thresh[16], peak;
    std::unordered_map<std::thread::id, int *> buffer;
    const int16_t factor[16] = {
        N / (N0 * N0), N / (N0 * N1), N / (N0 * N0), N / (N0 * N2),
        N / (N1 * N0), N / (N1 * N1), N / (N1 * N0), N / (N1 * N2),
        N / (N0 * N0), N / (N0 * N1), N / (N0 * N0), N / (N0 * N2),
        N / (N2 * N0), N / (N2 * N1), N / (N2 * N0), N / (N2 * N2)
    };
    void (*pp7Filter)(const VSFrameRef *, VSFrameRef *, const DeblockPP7Data * const VS_RESTRICT, const VSAPI *) noexcept;
};
