#pragma once

#include <thread>
#include <unordered_map>

#include <VapourSynth.h>
#include <VSHelper.h>

#include <fftw3.h>

struct DFTTestData {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    int sbsize, sosize, tbsize, tosize, swin, twin;
    float sbeta, tbeta, f0beta;
    bool zmean, process[3];
    float divisor, multiplier;
    int peak, barea, bvolume, ccnt, type, sbd1, ccnt2, inc;
    bool uf0b;
    const VSFormat * padFormat;
    int padWidth[3], padHeight[3], eheight[3];
    float * hw, * sigmas, * sigmas2, * pmins, * pmaxs;
    fftwf_complex * dftgc;
    fftwf_plan ft, fti;
    std::unordered_map<std::thread::id, VSFrameRef *> ebuff;
    std::unordered_map<std::thread::id, float *> dftr;
    std::unordered_map<std::thread::id, fftwf_complex *> dftc, dftc2;
    void (*copyPad)(const VSFrameRef *, VSFrameRef *[3], const DFTTestData *, const VSAPI *) noexcept;
    void (*filterCoeffs)(float *, const float *, const int, const float *, const float *, const float *) noexcept;
    void (*func_0)(VSFrameRef *[3], VSFrameRef *, const DFTTestData *, const VSAPI *) noexcept;
    void (*func_1)(VSFrameRef *[15][3], VSFrameRef *, const int, const DFTTestData *, const VSAPI *) noexcept;
};
