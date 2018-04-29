#ifdef VS_TARGET_CPU_X86
#ifndef __SSE4_1__
#define __SSE4_1__
#endif

#include "DeblockPP7.hpp"

template<typename T>
static inline void dctA(const T * srcp, T * dstp, const int stride) noexcept;

template<>
inline void dctA(const int * srcp, int * dstp, const int stride) noexcept {
    Vec4i s0 = Vec4i().load(srcp + 0 * stride) + Vec4i().load(srcp + 6 * stride);
    Vec4i s1 = Vec4i().load(srcp + 1 * stride) + Vec4i().load(srcp + 5 * stride);
    Vec4i s2 = Vec4i().load(srcp + 2 * stride) + Vec4i().load(srcp + 4 * stride);
    Vec4i s3 = Vec4i().load(srcp + 3 * stride);
    Vec4i s = s3 + s3;
    s3 = s - s0;
    s0 = s + s0;
    s = s2 + s1;
    s2 = s2 - s1;
    const Vec4i temp0 = s0 + s;
    const Vec4i temp2 = s0 - s;
    const Vec4i temp1 = 2 * s3 + s2;
    const Vec4i temp3 = s3 - 2 * s2;

    Vec4i r1 = blend4i<0, 4, -256, -256>(temp0, temp1);
    Vec4i r2 = blend4i<-256, -256, 0, 4>(temp2, temp3);
    blend4i<0, 1, 6, 7>(r1, r2).store_a(dstp + 0 * 4);

    r1 = blend4i<1, 5, -256, -256>(temp0, temp1);
    r2 = blend4i<-256, -256, 1, 5>(temp2, temp3);
    blend4i<0, 1, 6, 7>(r1, r2).store_a(dstp + 1 * 4);

    r1 = blend4i<2, 6, -256, -256>(temp0, temp1);
    r2 = blend4i<-256, -256, 2, 6>(temp2, temp3);
    blend4i<0, 1, 6, 7>(r1, r2).store_a(dstp + 2 * 4);

    r1 = blend4i<3, 7, -256, -256>(temp0, temp1);
    r2 = blend4i<-256, -256, 3, 7>(temp2, temp3);
    blend4i<0, 1, 6, 7>(r1, r2).store_a(dstp + 3 * 4);
}

template<>
inline void dctA(const float * srcp, float * dstp, const int stride) noexcept {
    Vec4f s0 = (Vec4f().load(srcp + 0 * stride) + Vec4f().load(srcp + 6 * stride)) * 255.f;
    Vec4f s1 = (Vec4f().load(srcp + 1 * stride) + Vec4f().load(srcp + 5 * stride)) * 255.f;
    Vec4f s2 = (Vec4f().load(srcp + 2 * stride) + Vec4f().load(srcp + 4 * stride)) * 255.f;
    Vec4f s3 = Vec4f().load(srcp + 3 * stride) * 255.f;
    Vec4f s = s3 + s3;
    s3 = s - s0;
    s0 = s + s0;
    s = s2 + s1;
    s2 = s2 - s1;
    const Vec4f temp0 = s0 + s;
    const Vec4f temp2 = s0 - s;
    const Vec4f temp1 = 2 * s3 + s2;
    const Vec4f temp3 = s3 - 2 * s2;

    Vec4f r1 = blend4f<0, 4, -256, -256>(temp0, temp1);
    Vec4f r2 = blend4f<-256, -256, 0, 4>(temp2, temp3);
    blend4f<0, 1, 6, 7>(r1, r2).store_a(dstp + 0 * 4);

    r1 = blend4f<1, 5, -256, -256>(temp0, temp1);
    r2 = blend4f<-256, -256, 1, 5>(temp2, temp3);
    blend4f<0, 1, 6, 7>(r1, r2).store_a(dstp + 1 * 4);

    r1 = blend4f<2, 6, -256, -256>(temp0, temp1);
    r2 = blend4f<-256, -256, 2, 6>(temp2, temp3);
    blend4f<0, 1, 6, 7>(r1, r2).store_a(dstp + 2 * 4);

    r1 = blend4f<3, 7, -256, -256>(temp0, temp1);
    r2 = blend4f<-256, -256, 3, 7>(temp2, temp3);
    blend4f<0, 1, 6, 7>(r1, r2).store_a(dstp + 3 * 4);
}

template<typename T1, typename T2>
static inline void dctB(const T1 * srcp, T1 * dstp) noexcept {
    T2 s0 = T2().load_a(srcp + 0 * 4) + T2().load_a(srcp + 6 * 4);
    T2 s1 = T2().load_a(srcp + 1 * 4) + T2().load_a(srcp + 5 * 4);
    T2 s2 = T2().load_a(srcp + 2 * 4) + T2().load_a(srcp + 4 * 4);
    T2 s3 = T2().load_a(srcp + 3 * 4);
    T2 s = s3 + s3;
    s3 = s - s0;
    s0 = s + s0;
    s = s2 + s1;
    s2 = s2 - s1;
    (s0 + s).store_a(dstp + 0 * 4);
    (s0 - s).store_a(dstp + 2 * 4);
    (2 * s3 + s2).store_a(dstp + 1 * 4);
    (s3 - 2 * s2).store_a(dstp + 3 * 4);
}

template<typename T>
void pp7Filter_sse4(const VSFrameRef * src, VSFrameRef * dst, const DeblockPP7Data * const VS_RESTRICT d, const VSAPI * vsapi) noexcept {
    const auto threadId = std::this_thread::get_id();
    int * buffer = d->buffer.at(threadId);

    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int srcStride = vsapi->getStride(src, plane) / sizeof(T);
            const int stride = d->stride[plane];
            const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
            T * VS_RESTRICT dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

            int * VS_RESTRICT p_src = buffer + stride * 8;
            int * VS_RESTRICT block = buffer;
            int * VS_RESTRICT temp = buffer + 16;

            for (int y = 0; y < height; y++) {
                const int index = stride * (8 + y) + 8;
                std::copy_n(srcp + srcStride * y, width, p_src + index);
                for (int x = 0; x < 8; x++) {
                    p_src[index - 1 - x] = p_src[index + x];
                    p_src[index + width + x] = p_src[index + width - 1 - x];
                }
            }
            for (int y = 0; y < 8; y++) {
                memcpy(p_src + stride * (7 - y), p_src + stride * (8 + y), stride * sizeof(int));
                memcpy(p_src + stride * (height + 8 + y), p_src + stride * (height + 7 - y), stride * sizeof(int));
            }

            for (int y = 0; y < height; y++) {
                for (int x = -8; x < 0; x += 4) {
                    const int index = (stride + 1) * (8 - 3) + stride * y + 8 + x;
                    int * VS_RESTRICT tp = temp + 4 * x;

                    dctA(p_src + index, tp + 4 * 8, stride);
                }

                for (int x = 0; x < width; x++) {
                    const int index = (stride + 1) * (8 - 3) + stride * y + 8 + x;
                    int * VS_RESTRICT tp = temp + 4 * x;

                    if (!(x & 3))
                        dctA(p_src + index, tp + 4 * 8, stride);
                    dctB<int, Vec4i>(tp, block);

                    int64_t v = static_cast<int64_t>(block[0]) * d->factor[0];
                    if (d->mode == 0) {
                        for (int i = 1; i < 16; i++) {
                            const unsigned threshold1 = d->thresh[i];
                            const unsigned threshold2 = threshold1 * 2;
                            if (block[i] + threshold1 > threshold2)
                                v += static_cast<int64_t>(block[i]) * d->factor[i];
                        }
                    } else if (d->mode == 1) {
                        for (int i = 1; i < 16; i++) {
                            const unsigned threshold1 = d->thresh[i];
                            const unsigned threshold2 = threshold1 * 2;
                            if (block[i] + threshold1 > threshold2) {
                                if (block[i] > 0)
                                    v += (block[i] - static_cast<int64_t>(threshold1)) * d->factor[i];
                                else
                                    v += (block[i] + static_cast<int64_t>(threshold1)) * d->factor[i];
                            }
                        }
                    } else {
                        for (int i = 1; i < 16; i++) {
                            const unsigned threshold1 = d->thresh[i];
                            const unsigned threshold2 = threshold1 * 2;
                            if (block[i] + threshold1 > threshold2) {
                                if (block[i] + threshold2 > threshold2 * 2) {
                                    v += static_cast<int64_t>(block[i]) * d->factor[i];
                                } else {
                                    if (block[i] > 0)
                                        v += 2 * (block[i] - static_cast<int64_t>(threshold1)) * d->factor[i];
                                    else
                                        v += 2 * (block[i] + static_cast<int64_t>(threshold1)) * d->factor[i];
                                }
                            }
                        }
                    }
                    v = (v + (1 << 17)) >> 18;
                    if (static_cast<unsigned>(v) > d->peak)
                        v = -v >> 63;

                    dstp[srcStride * y + x] = static_cast<T>(v);
                }
            }
        }
    }
}

template void pp7Filter_sse4<uint8_t>(const VSFrameRef *, VSFrameRef *, const DeblockPP7Data * const VS_RESTRICT, const VSAPI *) noexcept;
template void pp7Filter_sse4<uint16_t>(const VSFrameRef *, VSFrameRef *, const DeblockPP7Data * const VS_RESTRICT, const VSAPI *) noexcept;

template<>
void pp7Filter_sse4<float>(const VSFrameRef * src, VSFrameRef * dst, const DeblockPP7Data * const VS_RESTRICT d, const VSAPI * vsapi) noexcept {
    const auto threadId = std::this_thread::get_id();
    float * buffer = reinterpret_cast<float *>(d->buffer.at(threadId));

    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int srcStride = vsapi->getStride(src, plane) / sizeof(float);
            const int stride = d->stride[plane];
            const float * srcp = reinterpret_cast<const float *>(vsapi->getReadPtr(src, plane));
            float * VS_RESTRICT dstp = reinterpret_cast<float *>(vsapi->getWritePtr(dst, plane));

            float * VS_RESTRICT p_src = buffer + stride * 8;
            float * VS_RESTRICT block = buffer;
            float * VS_RESTRICT temp = buffer + 16;

            for (int y = 0; y < height; y++) {
                const int index = stride * (8 + y) + 8;
                std::copy_n(srcp + srcStride * y, width, p_src + index);
                for (int x = 0; x < 8; x++) {
                    p_src[index - 1 - x] = p_src[index + x];
                    p_src[index + width + x] = p_src[index + width - 1 - x];
                }
            }
            for (int y = 0; y < 8; y++) {
                memcpy(p_src + stride * (7 - y), p_src + stride * (8 + y), stride * sizeof(float));
                memcpy(p_src + stride * (height + 8 + y), p_src + stride * (height + 7 - y), stride * sizeof(float));
            }

            for (int y = 0; y < height; y++) {
                for (int x = -8; x < 0; x += 4) {
                    const int index = (stride + 1) * (8 - 3) + stride * y + 8 + x;
                    float * VS_RESTRICT tp = temp + 4 * x;

                    dctA(p_src + index, tp + 4 * 8, stride);
                }

                for (int x = 0; x < width; x++) {
                    const int index = (stride + 1) * (8 - 3) + stride * y + 8 + x;
                    float * VS_RESTRICT tp = temp + 4 * x;

                    if (!(x & 3))
                        dctA(p_src + index, tp + 4 * 8, stride);
                    dctB<float, Vec4f>(tp, block);

                    float v = block[0] * d->factor[0];
                    if (d->mode == 0) {
                        for (int i = 1; i < 16; i++) {
                            const unsigned threshold1 = d->thresh[i];
                            const unsigned threshold2 = threshold1 * 2;
                            if (static_cast<unsigned>(block[i]) + threshold1 > threshold2)
                                v += block[i] * d->factor[i];
                        }
                    } else if (d->mode == 1) {
                        for (int i = 1; i < 16; i++) {
                            const unsigned threshold1 = d->thresh[i];
                            const unsigned threshold2 = threshold1 * 2;
                            if (static_cast<unsigned>(block[i]) + threshold1 > threshold2) {
                                if (block[i] > 0.f)
                                    v += (block[i] - threshold1) * d->factor[i];
                                else
                                    v += (block[i] + threshold1) * d->factor[i];
                            }
                        }
                    } else {
                        for (int i = 1; i < 16; i++) {
                            const unsigned threshold1 = d->thresh[i];
                            const unsigned threshold2 = threshold1 * 2;
                            if (static_cast<unsigned>(block[i]) + threshold1 > threshold2) {
                                if (static_cast<unsigned>(block[i]) + threshold2 > threshold2 * 2) {
                                    v += block[i] * d->factor[i];
                                } else {
                                    if (block[i] > 0.f)
                                        v += 2.f * (block[i] - threshold1) * d->factor[i];
                                    else
                                        v += 2.f * (block[i] + threshold1) * d->factor[i];
                                }
                            }
                        }
                    }

                    dstp[srcStride * y + x] = v * ((1.f / (1 << 18)) * (1.f / 255.f));
                }
            }
        }
    }
}
#endif
