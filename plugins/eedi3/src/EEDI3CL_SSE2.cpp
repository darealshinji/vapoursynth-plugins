#ifdef HAVE_OPENCL
#include "EEDI3CL.hpp"

template<typename T>
void processCL_sse2(const VSFrameRef * src, const VSFrameRef * scp, VSFrameRef * dst, VSFrameRef ** pad, const int field_n, const EEDI3CLData * d, const VSAPI * vsapi) {
    for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
        if (d->process[plane]) {
            copyPad<T>(src, pad[plane], plane, 1 - field_n, d->dh, vsapi);

            const int srcWidth = vsapi->getFrameWidth(pad[plane], 0);
            const int dstWidth = vsapi->getFrameWidth(dst, plane);
            const int srcHeight = vsapi->getFrameHeight(pad[plane], 0);
            const int dstHeight = vsapi->getFrameHeight(dst, plane);
            const int srcStride = vsapi->getStride(pad[plane], 0) / sizeof(T);
            const int dstStride = vsapi->getStride(dst, plane) / sizeof(T);
            const T * _srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(pad[plane], 0));
            T * VS_RESTRICT _dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

            const auto threadId = std::this_thread::get_id();
            auto queue = d->queue.at(threadId);
            auto calculateConnectionCosts = d->calculateConnectionCosts.at(threadId);
            auto srcImage = d->src.at(threadId);
            auto _ccosts = d->ccosts.at(threadId);
            float * pcosts = d->pcosts.at(threadId) + d->mdisVector;
            int * _pbackt = d->pbackt.at(threadId) + d->mdisVector;
            int * fpath = d->fpath.at(threadId);
            int * _dmap = d->dmap.at(threadId);
            float * tline = d->tline.at(threadId);

            const size_t globalWorkSize[] = { static_cast<size_t>(dstWidth), static_cast<size_t>(d->vectorSize) };
            const int bufferSize = dstWidth * d->tpitchVector * sizeof(cl_float);

            vs_bitblt(_dstp + dstStride * (1 - field_n), vsapi->getStride(dst, plane) * 2,
                      _srcp + srcStride * (4 + 1 - field_n) + 12, vsapi->getStride(pad[plane], 0) * 2,
                      dstWidth * sizeof(T), dstHeight / 2);

            queue.enqueue_write_image(srcImage, compute::dim(0, 0), compute::dim(srcWidth, srcHeight), _srcp, vsapi->getStride(pad[plane], 0));

            for (int y = 4 + field_n; y < srcHeight - 4; y += 2 * d->vectorSize) {
                const int off = (y - 4 - field_n) >> 1;

                calculateConnectionCosts.set_args(srcImage, _ccosts, dstWidth, srcHeight - 4, y);
                queue.enqueue_nd_range_kernel(calculateConnectionCosts, 2, nullptr, globalWorkSize, nullptr);

                float * ccosts = reinterpret_cast<float *>(queue.enqueue_map_buffer(_ccosts, CL_MAP_READ, 0, bufferSize)) + d->mdisVector;

                // calculate path costs
                Vec4f().load(ccosts).store_a(pcosts);
                for (int x = 1; x < dstWidth; x++) {
                    const float * tT = ccosts + d->tpitchVector * x;
                    const float * ppT = pcosts + d->tpitchVector * (x - 1);
                    float * pT = pcosts + d->tpitchVector * x;
                    int * piT = _pbackt + d->tpitchVector * (x - 1);

                    const int umax = std::min({ x, dstWidth - 1 - x, d->mdis });
                    const int umax2 = std::min({ x - 1, dstWidth - x, d->mdis });

                    for (int u = -umax; u <= umax; u++) {
                        Vec4i idx = zero_128b();
                        Vec4f bval = FLT_MAX;

                        for (int v = std::max(-umax2, u - 1); v <= std::min(umax2, u + 1); v++) {
                            const Vec4f z = Vec4f().load_a(ppT + v * d->vectorSize) + d->gamma * std::abs(u - v);
                            const Vec4f ccost = min(z, FLT_MAX * 0.9f);
                            idx = select(Vec4ib(ccost < bval), v, idx);
                            bval = min(ccost, bval);
                        }

                        const Vec4f z = bval + Vec4f().load(tT + u * d->vectorSize);
                        min(z, FLT_MAX * 0.9f).store_a(pT + u * d->vectorSize);
                        idx.stream(piT + u * d->vectorSize);
                    }
                }

                for (int vs = 0; vs < d->vectorSize; vs++) {
                    const int realY = 4 + field_n + 2 * (off + vs);
                    if (realY >= srcHeight - 4)
                        break;

                    const T * srcp = _srcp + srcStride * realY + 12;
                    T * dstp = _dstp + dstStride * (field_n + 2 * (off + vs));
                    int * dmap = _dmap + dstWidth * (off + vs);

                    const T * src3p = srcp - srcStride * 3;
                    const T * src1p = srcp - srcStride;
                    const T * src1n = srcp + srcStride;
                    const T * src3n = srcp + srcStride * 3;

                    const int * pbackt = _pbackt + vs;

                    // backtrack
                    fpath[dstWidth - 1] = 0;
                    for (int x = dstWidth - 2; x >= 0; x--)
                        fpath[x] = pbackt[(d->tpitch * x + fpath[x + 1]) * d->vectorSize];

                    interpolate<T>(src3p, src1p, src1n, src3n, fpath, dmap, dstp, dstWidth, d->ucubic, d->peak);
                }

                queue.enqueue_unmap_buffer(_ccosts, ccosts - d->mdisVector);
            }

            if (d->vcheck) {
                const T * srcp = _srcp + srcStride * (4 + field_n) + 12;
                const T * scpp = nullptr;
                if (d->sclip)
                    scpp = reinterpret_cast<const T *>(vsapi->getReadPtr(scp, plane)) + dstStride * field_n;
                T * dstp = _dstp + dstStride * field_n;;

                vCheck<T>(srcp, scpp, dstp, _dmap, tline, field_n, dstWidth, srcHeight, srcStride, dstStride, d->vcheck, d->vthresh2, d->rcpVthresh0, d->rcpVthresh1, d->rcpVthresh2, d->peak);
            }
        }
    }
}

template void processCL_sse2<uint8_t>(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3CLData *, const VSAPI *);
template void processCL_sse2<uint16_t>(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3CLData *, const VSAPI *);
template void processCL_sse2<float>(const VSFrameRef *, const VSFrameRef *, VSFrameRef *, VSFrameRef **, const int, const EEDI3CLData *, const VSAPI *);
#endif
