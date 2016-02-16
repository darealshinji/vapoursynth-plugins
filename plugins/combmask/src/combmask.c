/*
  combmask.c: Copyright (C) 2012-2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This file is part of CombMask.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with the author; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "VapourSynth.h"
#include "combmask.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif


static int VS_CC
set_planes(int *planes, const VSMap *in, const VSAPI *vsapi)
{
    int num = vsapi->propNumElements(in, "planes");
    if (num < 1) {
        for (int i = 0; i < 3; planes[i++] = 1);
        return 0;
    }

    for (int i = 0; i < num; i++) {
        int p = (int)vsapi->propGetInt(in, "planes", i, NULL);
        if (p < 0 || p > 2) {
            return -1;
        }
        planes[p] = 1;
    }

    return 0;
}


static void
set_param_int(int *param, const char *name, int undef, int min, int max,
              const VSMap *in, const VSAPI *vsapi, char *buff)
{
    int err;
    *param = (int)vsapi->propGetInt(in, name, 0, &err);
    if (err) {
        *param = undef;
    }

    if (*param < min || *param > max) {
        sprintf(buff, "%s must be between %d and %d.", name, min, max);
    }
}



static const VSFrameRef * VS_CC
get_frame_combmask(int n, int activation_reason, void **instance_data,
                   void **frame_data, VSFrameContext *frame_ctx, VSCore *core,
                   const VSAPI *vsapi)
{
    combmask_t *ch = (combmask_t *)*instance_data;
    int p = n == 0 ? 1 : n - 1;

    if (activation_reason == arInitial) {
        vsapi->requestFrameFilter(n, ch->node, frame_ctx);
        if (ch->mthresh > 0) {
            vsapi->requestFrameFilter(p, ch->node, frame_ctx);
        }
        return NULL;
    }

    if (activation_reason != arAllFramesReady) {
        return NULL;
    }

    const VSFrameRef *src = vsapi->getFrameFilter(n, ch->node, frame_ctx);

    VSFrameRef *cmask = vsapi->newVideoFrame(ch->vi->format, ch->vi->width,
                                             ch->vi->height, NULL, core);

    ch->write_combmask(ch, vsapi, src, cmask);

    if (ch->mthresh > 0) {
        const VSFrameRef *prev = vsapi->getFrameFilter(p, ch->node, frame_ctx);
        adapt_motion(ch, vsapi, src, prev, cmask);
        vsapi->freeFrame(prev);
    }

    vsapi->freeFrame(src);

    vsapi->propSetInt(vsapi->getFramePropsRW(cmask), "_Combed",
                      ch->is_combed(ch, cmask, vsapi), paReplace);

    return cmask;
}


static void VS_CC
init_combmask(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
              VSCore *core, const VSAPI *vsapi)
{
    combmask_t *ch = (combmask_t *)*instance_data;
    vsapi->setVideoInfo(ch->vi, 1, node);
    vsapi->clearMap(in);
}


static void VS_CC
close_combmask(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    combmask_t * ch = (combmask_t *)instance_data;
    if (!ch) {
        return;
    }
    if (ch->node) {
        vsapi->freeNode(ch->node);
        ch->node = NULL;
    }
    free(ch);
    ch = NULL;
}


static void VS_CC
create_combmask(const VSMap *in, VSMap *out, void *user_data, VSCore *core,
                const VSAPI *vsapi)
{
#define RET_IF_ERROR(cond, ...) \
{ \
    if (cond) { \
        close_combmask(ch, core, vsapi); \
        snprintf(msg, 240, __VA_ARGS__); \
        vsapi->setError(out, msg_buff); \
        return; \
    } \
}

    char msg_buff[256] = "CombMask: ";
    char *msg = msg_buff + strlen(msg_buff);

    combmask_t *ch = (combmask_t *)calloc(sizeof(combmask_t), 1);
    RET_IF_ERROR(!ch, "failed to allocate handler.");

    ch->node = vsapi->propGetNode(in, "clip", 0, 0);
    ch->vi = vsapi->getVideoInfo(ch->node);
    RET_IF_ERROR(ch->vi->width == 0 || ch->vi->height == 0 || !ch->vi->format,
                 "clip is not constant resolution/format.");
    RET_IF_ERROR(ch->vi->format->sampleType != stInteger,
                 "clip is not integer format.");

    RET_IF_ERROR(set_planes(ch->planes, in, vsapi), "planes index out of range");

    int mag = 1 << (ch->vi->format->bitsPerSample - 8);
    int max = (1 << ch->vi->format->bitsPerSample) - 1;
    char err[256] = {0};

    set_param_int(&ch->cthresh, "cthresh", 6 * mag, 0, max, in, vsapi, err);
    RET_IF_ERROR(err[0], "%s", err);

    set_param_int(&ch->mthresh, "mthresh", 9 * mag, 0, max, in, vsapi, err);
    RET_IF_ERROR(err[0], "%s", err);

    set_param_int(&ch->mi, "mi", 40, 0, 128, in, vsapi, err);
    RET_IF_ERROR(err[0], "%s", err);

    if (ch->vi->numFrames == 1) {
        ch->mthresh = 0;
    }

    int func_index = ch->vi->format->bytesPerSample - 1;
    if (ch->vi->format->bitsPerSample == 16) {
        func_index = 2;
    }

    ch->write_combmask = write_combmask_funcs[func_index];
    ch->write_motionmask = write_motionmask_funcs[func_index];
    ch->is_combed = is_combed_funcs[func_index];
    ch->horizontal_dilation = h_dilation_funcs[func_index];

    vsapi->createFilter(in, out, "CombMask", init_combmask, get_frame_combmask,
                        close_combmask, fmParallel, 0, ch, core);

#undef RET_IF_ERROR
}



static const VSFrameRef * VS_CC
get_frame_maskedmerge(int n, int activation_reason, void **instance_data,
                      void **frame_data, VSFrameContext *frame_ctx,
                      VSCore *core, const VSAPI *vsapi)
{
    maskedmerge_t *mh = (maskedmerge_t *)*instance_data;

    if (activation_reason == arInitial) {
        vsapi->requestFrameFilter(n, mh->base, frame_ctx);
        vsapi->requestFrameFilter(n, mh->altc, frame_ctx);
        vsapi->requestFrameFilter(n, mh->mask, frame_ctx);
        return NULL;
    }

    if (activation_reason != arAllFramesReady) {
        return NULL;
    }

    const VSFrameRef *base = vsapi->getFrameFilter(n, mh->base, frame_ctx);

    VSFrameRef *dst = vsapi->copyFrame(base, core);
    vsapi->freeFrame(base);

    const VSFrameRef *alt  = vsapi->getFrameFilter(n, mh->altc, frame_ctx);
    const VSFrameRef *mask = vsapi->getFrameFilter(n, mh->mask, frame_ctx);

    merge_frames(mh, vsapi, mask, alt, dst);

    vsapi->freeFrame(alt);
    vsapi->freeFrame(mask);

    return dst;
}


static void VS_CC
init_maskedmerge(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
                 VSCore *core, const VSAPI *vsapi)
{
    maskedmerge_t *mh = (maskedmerge_t *)*instance_data;
    vsapi->setVideoInfo(mh->vi, 1, node);
    vsapi->clearMap(in);
}


static void VS_CC
close_maskedmerge(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    maskedmerge_t *mh = (maskedmerge_t *)instance_data;
    if (!mh) {
        return;
    }
    if (mh->base) {
        vsapi->freeNode(mh->base);
        mh->base = NULL;
    }
    if (mh->altc) {
        vsapi->freeNode(mh->altc);
        mh->altc = NULL;
    }
    if (mh->mask) {
        vsapi->freeNode(mh->mask);
        mh->mask = NULL;
    }
    free(mh);
    mh = NULL;
}


static void VS_CC
is_valid_node(const VSVideoInfo *base, const VSVideoInfo *target,
              char *buff, const char *name)
{
    if (base->width != target->width || base->height != target->height) {
         sprintf(buff, "base and %s are not the same resolution.", name);
         return;
    }
    if (base->format != target->format) {
        sprintf(buff, "base and %s are not the same format.", name);
    }
}


static void VS_CC
create_maskedmerge(const VSMap *in, VSMap *out, void *user_data, VSCore *core,
                   const VSAPI *vsapi)
{
#define RET_IF_ERROR(cond, ...) \
{ \
    if (cond) { \
        close_maskedmerge(mh, core, vsapi); \
        snprintf(msg, 240, __VA_ARGS__); \
        vsapi->setError(out, msg_buff); \
        return; \
    } \
}

    char msg_buff[256] = "CMaskedMerge: ";
    char *msg = msg_buff + strlen(msg_buff);

    maskedmerge_t *mh = (maskedmerge_t *)calloc(sizeof(maskedmerge_t), 1);
    RET_IF_ERROR(!mh, "failed to allocate handler.");

    mh->base = vsapi->propGetNode(in, "base", 0, 0);
    mh->vi = vsapi->getVideoInfo(mh->base);
    RET_IF_ERROR(mh->vi->width == 0 || mh->vi->height == 0 || !mh->vi->format,
                 "base is not constant resolution/format.");
    RET_IF_ERROR(mh->vi->format->sampleType != stInteger,
                 "clip is not integer format.");

    char err[256] = {0};

    mh->altc = vsapi->propGetNode(in, "alt", 0, 0);
    is_valid_node(mh->vi, vsapi->getVideoInfo(mh->altc), "alt", err);
    RET_IF_ERROR(err[0], "%s", err);

    mh->mask = vsapi->propGetNode(in, "mask", 0, 0);
    is_valid_node(mh->vi, vsapi->getVideoInfo(mh->mask), "mask", err);
    RET_IF_ERROR(err[0], "%s", err);

    RET_IF_ERROR(set_planes(mh->planes, in, vsapi),
                 "planes index out of range");

    vsapi->createFilter(in, out, "CMaskedMerge", init_maskedmerge,
                        get_frame_maskedmerge, close_maskedmerge, fmParallel,
                        0, mh, core);

#undef RET_IF_ERROR
}


VS_EXTERNAL_API(void)
VapourSynthPluginInit(VSConfigPlugin conf, VSRegisterFunction reg,
                      VSPlugin *plugin)
{
    conf("chikuzen.does.not.have.his.own.domain.combmask", "comb",
         "comb filters v"
         COMBMASK_VERSION, VAPOURSYNTH_API_VERSION, 1, plugin);
    reg("CombMask",
        "clip:clip;cthresh:int:opt;mthresh:int:opt;mi:int:opt;planes:int[]:opt;",
        create_combmask, NULL, plugin);
    reg("CMaskedMerge",
        "base:clip;alt:clip;mask:clip;planes:int[]:opt;", create_maskedmerge,
        NULL, plugin);
}