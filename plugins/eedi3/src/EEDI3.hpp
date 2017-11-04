#pragma once

#include "shared.hpp"

#ifdef VS_TARGET_CPU_X86
#define MAX_VECTOR_SIZE 512
#include "vectorclass/vectorclass.h"

template<typename T1, typename T2>
static inline void reorder(const T1 * srcp, T2 * _dstp, const int width, const int height, const int srcStride, const int dstStride, const int srcY, const int vectorSize) noexcept {
    for (int y = srcY - 2; y < srcY + 2; y++) {
        T2 * VS_RESTRICT dstp = _dstp;

        for (int y2 = y; y2 < y + vectorSize; y2++) {
            int realY = (y2 < 0) ? -1 - y2 : y2;
            if (realY >= height)
                realY = height * 2 - 1 - realY;
            realY = std::max(realY, 0);

            const T1 * line = srcp + srcStride * realY;

            for (int x = 0; x < 12; x++) {
                const int srcX = std::min(12 - 1 - x, width - 1);
                dstp[x * vectorSize] = line[srcX];
            }

            for (int x = 0; x < width; x++)
                dstp[(x + 12) * vectorSize] = line[x];

            for (int x = 0; x < 12; x++) {
                const int srcX = std::max(width - 1 - x, 0);
                dstp[(width + x + 12) * vectorSize] = line[srcX];
            }

            dstp++;
        }

        _dstp += dstStride * vectorSize;
    }
}
#endif

struct EEDI3Data {
    VSNodeRef * node, * sclip;
    VSVideoInfo vi;
    int field, nrad, mdis, vcheck;
    bool dh, process[3], ucubic, cost3;
    float alpha, beta, gamma, vthresh2;
    int peak, vectorSize, tpitch, mdisVector, tpitchVector, alignment;
    float remainingWeight, rcpVthresh0, rcpVthresh1, rcpVthresh2;
    std::unordered_map<std::thread::id, float *> ccosts, pcosts, tline;
    std::unordered_map<std::thread::id, int *> srcVector, pbackt, fpath, dmap;
    void (*processor)(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3Data *, const VSAPI *);
};
