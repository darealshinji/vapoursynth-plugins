/*
 * VapourSynth D2V Plugin
 *
 * Copyright (c) 2012 Derek Buitenhuis
 *
 * This file is part of d2vsource.
 *
 * d2vsource is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * d2vsource is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with d2vsource; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

extern "C" {
#include <stdint.h>
#include <stdlib.h>
}

#include <algorithm>

#include <VapourSynth.h>
#include <VSHelper.h>

#include "applyrff.hpp"
#include "d2v.hpp"
#include "gop.hpp"

void VS_CC rffInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    rffData *d = (rffData *) *instanceData;
    vsapi->setVideoInfo(&d->vi, 1, node);
}

const VSFrameRef *VS_CC rffGetFrame(int n, int activationReason, void **instanceData, void **frameData,
                                    VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
    const rffData *d = (const rffData *) *instanceData;
    const VSFrameRef *st, *sb;
    VSFrameRef *f;
    VSMap *props;
    int top, bottom;
    int fieldbased;
    int i;
    bool samefields;

    /* What frames to use for fields. */
    const rffField *top_field = &d->fields[n * 2];
    const rffField *bottom_field = &d->fields[n * 2 + 1];
    if (top_field->type == Bottom)
        std::swap(top_field, bottom_field);

    top    = top_field->frame;
    bottom = bottom_field->frame;

    samefields = top == bottom;

    /* Request out source frames. */
    if (activationReason == arInitial) {
        if (samefields) {
            vsapi->requestFrameFilter(top, d->node, frameCtx);
        } else {
            vsapi->requestFrameFilter(min(top, bottom), d->node, frameCtx);
            vsapi->requestFrameFilter(max(top, bottom), d->node, frameCtx);
        }
        return NULL;
    }

    /* Check if we're ready yet. */
    if (activationReason != arAllFramesReady)
        return NULL;

    /* Source and destination frames. */
    st = vsapi->getFrameFilter(top, d->node, frameCtx);
    sb = samefields ? NULL : vsapi->getFrameFilter(bottom, d->node, frameCtx);

    /* Copy into VS's buffers. */
    if (samefields) {
        f = vsapi->copyFrame(st, core);
    } else {
        int dst_stride[3], srct_stride[3], srcb_stride[3];

        /*
         * Copy properties from the first field's source frame.
         * Some of them will be wrong for this frame, but ¯\_(ツ)_/¯.
        */
        const VSFrameRef *prop_src = bottom_field < top_field ? sb : st;

        f  = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, prop_src, core);

        for (i = 0; i < d->vi.format->numPlanes; i++) {
            dst_stride[i]  = vsapi->getStride(f, i);
            srct_stride[i] = vsapi->getStride(st, i);
            srcb_stride[i] = vsapi->getStride(sb, i);

            uint8_t *dstp = vsapi->getWritePtr(f, i);
            const uint8_t *srctp = vsapi->getReadPtr(st, i);
            const uint8_t *srcbp = vsapi->getReadPtr(sb, i);
            int width = vsapi->getFrameWidth(f, i);
            int height = vsapi->getFrameHeight(f, i);

            vs_bitblt(dstp, dst_stride[i] * 2,
                      srctp, srct_stride[i] * 2,
                      width * d->vi.format->bytesPerSample, height / 2);

            vs_bitblt(dstp + dst_stride[i], dst_stride[i] * 2,
                      srcbp + srcb_stride[i], srcb_stride[i] * 2,
                      width * d->vi.format->bytesPerSample, height / 2);
        }
    }

    if (!samefields) {
        /* Set field order. */
        props = vsapi->getFramePropsRW(f);

        // They point to elements of an array, so pointer comparison is fine.
        if (bottom_field < top_field)
            fieldbased = 1; // bff
        else
            fieldbased = 2; // tff

        vsapi->propSetInt(props, "_FieldBased", fieldbased, paReplace);
    }

    vsapi->freeFrame(st);
    if (!samefields)
        vsapi->freeFrame(sb);

    return f;
}

void VS_CC rffFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    rffData *d = (rffData *) instanceData;
    vsapi->freeNode(d->node);
    d2vfreep(&d->d2v);
    d->fields.clear();
    delete d;
}

void VS_CC rffCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
    rffData *data;
    string msg;
    int i;

    /* Allocate our private data. */
    data = new(nothrow) rffData;
    if (!data) {
        vsapi->setError(out, "Cannot allocate private data.");
        return;
    }

    /* Parse the D2V to get flags. */
    data->d2v = d2vparse((char *) vsapi->propGetData(in, "d2v", 0, 0), msg);
    if (!data->d2v) {
        vsapi->setError(out, msg.c_str());
        delete data;
        return;
    }

    /* Get our frame info and copy it, so we can modify it after. */
    data->node = vsapi->propGetNode(in, "clip", 0, 0);
    data->vi   = *vsapi->getVideoInfo(data->node);

    /*
     * Parse all the RFF flags to figure out which fields go
     * with which frames, and out total number of frames after
     * apply the RFF flags.
     */
    for(i = 0; i < data->vi.numFrames; i++) {
        frame f  = data->d2v->frames[i];
        bool rff = !!(data->d2v->gops[f.gop].flags[f.offset] & FRAME_FLAG_RFF);
        bool tff = !!(data->d2v->gops[f.gop].flags[f.offset] & FRAME_FLAG_TFF);

        int progressive_sequence = !!(data->d2v->gops[f.gop].info & GOP_FLAG_PROGRESSIVE_SEQUENCE);

        if (progressive_sequence) {
            /*
             * We repeat whole frames instead of fields, to turn one
             * coded progressive frame into either two or three
             * identical progressive frames.
             */
            rffField field;
            field.frame = i;
            field.type = Progressive;

            data->fields.push_back(field);
            data->fields.push_back(field);

            if (rff) {
                data->fields.push_back(field);
                data->fields.push_back(field);

                if (tff) {
                    data->fields.push_back(field);
                    data->fields.push_back(field);
                }
            }
        } else {
            /* Sequence is not progressive. Repeat fields. */

            rffField first_field, second_field;
            first_field.frame = second_field.frame = i;
            first_field.type = tff ? Top : Bottom;
            second_field.type = tff ? Bottom : Top;

            data->fields.push_back(first_field);
            data->fields.push_back(second_field);

            if (rff)
                data->fields.push_back(first_field);
        }
    }

    data->vi.numFrames = (int)data->fields.size() / 2;

    vsapi->createFilter(in, out, "applyrff", rffInit, rffGetFrame, rffFree, fmParallel, 0, data, core);
}
