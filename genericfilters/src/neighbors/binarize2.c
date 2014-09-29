#define USE_ALIGNED_MALLOC
#include "common.h"
#ifdef USE_X86_INTRINSICS
#include "sse2.h"
#else
#include <stdlib.h>
#include "no_simd.h"
#endif


typedef struct filter_data {
    void (VS_CC *function)(uint16_t *, int, int, int, int, uint8_t *, const uint8_t *);
} binarize2_t;


static void GF_FUNC_ALIGN VS_CC
sierra24a(uint16_t *buff_orig, int bstride, int width, int height, int stride,
          uint8_t *dstp, const uint8_t *srcp)
{
    uint16_t *buff = buff_orig;
    bstride /= 2;

#ifdef USE_X86_INTRINSICS
    __m128i zero = _mm_setzero_si128();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i load = _mm_load_si128((__m128i *)(srcp + x));
            __m128i lo = _mm_unpacklo_epi8(load, zero);
            __m128i hi = _mm_unpackhi_epi8(load, zero);
            _mm_store_si128((__m128i *)(buff + x), lo);
            _mm_store_si128((__m128i *)(buff + x + 8), hi);
        }
        srcp += stride;
        buff += bstride;
    }
#else
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            buff[x] = srcp[x];
        }
        srcp += stride;
        buff += bstride;
    }
#endif

    buff = buff_orig;
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; x++) {
            uint16_t mod = (buff[x] % 255) / 2;
            buff[x + 1] += mod;
            buff[x - 1 + bstride] += mod / 2;
            buff[x + bstride] += mod / 2;
            buff[x] = (buff[x] >= 255) * 255;
        }
        buff += bstride;
        for (int x = width - 1; x > 0; x--) {
            uint16_t mod = (buff[x] % 255) / 2;
            buff[x - 1] += mod;
            buff[x + 1 + bstride] += mod / 2;
            buff[x + bstride] += mod / 2;
            buff[x] = (buff[x] >= 255) * 255;
        }
        buff += bstride;
    }

    buff = buff_orig;
#ifdef USE_X86_INTRINSICS
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            __m128i lo = _mm_load_si128((__m128i *)(buff + x));
            __m128i hi = _mm_load_si128((__m128i *)(buff + x + 8));
            __m128i result = _mm_packus_epi16(lo, hi);
            _mm_store_si128((__m128i *)(dstp + x), result);
        }
        dstp += stride;
        buff += bstride;
    }
#else
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstp[x] = (uint8_t)(buff[x]);
        }
        dstp += stride;
        buff += bstride;
    }
#endif
}


static void VS_CC
binarize2_get_frame(generic_handler_t *gh, const VSFormat *fi,
                    const VSFrameRef **fr, const VSAPI *vsapi,
                    const VSFrameRef *src, VSFrameRef *dst)
{
    binarize2_t *b2 = gh->fdata;

    if (fi->bitsPerSample > 8) {
        for (int plane = 0; plane < fi->numPlanes; plane++) {
            if (fr[plane]) {
                continue;
            }
            memcpy(vsapi->getWritePtr(dst, plane),
                   vsapi->getReadPtr(src, plane),
                   vsapi->getStride(dst, plane) *
                   vsapi->getFrameHeight(dst, plane));
        }
        return;
    }

    int bstride = ((vsapi->getFrameWidth(src, 0) * sizeof(uint16_t) + 31) / 32) * 32;
    int height = vsapi->getFrameHeight(src, 0) + 2;
    uint16_t *buff = (uint16_t *)_aligned_malloc(bstride * height, 16);
    if (!buff) {
        return;
    }

    for (int plane = 0; plane < fi->numPlanes; plane++) {
        if (fr[plane]) {
            continue;
        }

        b2->function(buff, bstride,
                     vsapi->getFrameWidth(src, plane),
                     vsapi->getFrameHeight(src, plane),
                     vsapi->getStride(src, plane),
                     vsapi->getWritePtr(dst, plane),
                     vsapi->getReadPtr(src, plane));
    }

    _aligned_free(buff);
}


static void VS_CC
set_binarize2_data(generic_handler_t *gh, filter_id_t id, char *msg,
                   const VSMap *in, VSMap *out, const VSAPI *vsapi)
{
    binarize2_t *b2 = (binarize2_t *)calloc(sizeof(binarize2_t), 1);
    RET_IF_ERROR(!b2, "failed to allocate filter data");
    gh->fdata = b2;

    b2->function = sierra24a;

    gh->get_frame_filter = binarize2_get_frame;
}

const set_filter_data_func set_binarize2 = set_binarize2_data;
