/*
  imagereader.c

  This file is part of vsimagereader

  Copyright (C) 2012-2013  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Libav; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "turbojpeg.h"
#include "VapourSynth.h"
#include "imagereader.h"


#define VS_IMGR_VERSION "0.2.1"
#define INITIAL_SRC_BUFF_SIZE (2 * 1024 * 1024) /* 2MiByte */


static const VSFrameRef * VS_CC
img_get_frame(int n, int activation_reason, void **instance_data,
              void **frame_data, VSFrameContext *frame_ctx, VSCore *core,
              const VSAPI *vsapi)
{
    if (activation_reason != arInitial) {
        return NULL;
    }
    
    img_hnd_t *ih = (img_hnd_t *)*instance_data;

    int frame_number = n;
    if (n >= ih->vi[0].numFrames) {
        frame_number = ih->vi[0].numFrames - 1;
    }

    if (ih->src[frame_number].read(ih, frame_number)) {
        return NULL;
    }
    ih->row_adjust--;

    VSFrameRef *dst[2];
    dst[0] = vsapi->newVideoFrame(ih->src[frame_number].format,
                                  ih->src[frame_number].width,
                                  ih->src[frame_number].height,
                                  NULL, core);

    VSMap *props = vsapi->getFramePropsRW(dst[0]);
    vsapi->propSetInt(props, "_DurationNum", ih->vi[0].fpsDen, paReplace);
    vsapi->propSetInt(props, "_DurationDen", ih->vi[0].fpsNum, paReplace);

    ih->write_frame(ih, frame_number, dst, core, vsapi);

    if (ih->enable_alpha == 0) {
        return dst[0];
    }

    if (vsapi->getOutputIndex(frame_ctx) == 0) {
        vsapi->freeFrame(dst[1]);
        return dst[0];
    }

    vsapi->freeFrame(dst[0]);
    props = vsapi->getFramePropsRW(dst[1]);
    vsapi->propSetInt(props, "_DurationNum", ih->vi[1].fpsDen, paReplace);
    vsapi->propSetInt(props, "_DurationDen", ih->vi[1].fpsNum, paReplace);

    return dst[1];
}


static void VS_CC
vs_init(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
        VSCore *core, const VSAPI *vsapi)
{
    img_hnd_t *ih = (img_hnd_t *)*instance_data;
    vsapi->setVideoInfo(ih->vi, ih->enable_alpha + 1, node);
}


static void VS_CC
close_handler(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    img_hnd_t *ih = (img_hnd_t *)instance_data;
    if (!ih) {
        return;
    }
    if (ih->tjhandle && tjDestroy((tjhandle)ih->tjhandle)) {
        fprintf(stderr, tjGetErrorStr());
    }
    ih->tjhandle = NULL;
    if (ih->src) {
        free(ih->src);
        ih->src = NULL;
    }
    if (ih->src_buff) {
        free(ih->src_buff);
        ih->src_buff = NULL;
    }
    if (ih->image_buff) {
        free(ih->image_buff);
        ih->image_buff = NULL;
    }
    if (ih->png_row_index) {
        free(ih->png_row_index);
        ih->png_row_index = NULL;
    }
    free(ih);
    ih = NULL;
}


static image_type_t VS_CC detect_image_type(FILE *fp)
{
    uint16_t sof;
    if (fread(&sof, 1, 2, fp) != 2) {
        return IMG_TYPE_NONE;
    }

    if (sof == 0x4D42) {
        return IMG_TYPE_BMP;
    }
    if (sof == 0xD8FF) {
        return IMG_TYPE_JPG;
    }
    if (sof == 0x5089) {
        return IMG_TYPE_PNG;
    }
    if (((uint8_t *)&sof)[1] == 0x00) { // 0x01(color map) is unsupported.
        return IMG_TYPE_TGA;
    }
    
    return IMG_TYPE_NONE;
}


static const char * VS_CC
check_src_props(img_hnd_t *ih, int n, vs_args_t *va)
{
    const func_check_src check_src[] = {
        NULL,
        check_src_bmp,
        check_src_jpeg,
        check_src_png,
        check_src_tga
    };

    FILE *fp = imgr_fopen(ih->src[n].name);
    if (!fp) {
        return "failed to open file";
    }

    image_type_t img_type = detect_image_type(fp);
    if (img_type == IMG_TYPE_NONE) {
        fclose(fp);
        return "unsupported format";
    }

    fseek(fp, 0, SEEK_SET);
    const char *ret = check_src[img_type](ih, n, fp, va);

    fclose(fp);
    if (ret) {
        return ret;
    }

    if (va->max_height < ih->src[n].height) {
        va->max_height = ih->src[n].height;
    }
    if (n == 0) {
        ih->vi[0].width = ih->src[0].width;
        ih->vi[0].height = ih->src[0].height;
        ih->vi[0].format = ih->src[0].format;
    }

    if ((va->variable_width << 2 |
         va->variable_height << 1 |
         va->variable_format) == 3) {
        return NULL;
    }

    if (ih->vi[0].width != ih->src[n].width) {
        va->variable_width = 1;
    }
    if (ih->vi[0].height != ih->src[n].height) {
        va->variable_height = 1;
    }
    if (ih->vi[0].format != ih->src[n].format) {
        va->variable_format = 1;
    }

    return NULL;
}


#define RET_IF_ERR(cond, ...) {\
    if (cond) {\
        close_handler(ih, core, vsapi);\
        snprintf(msg, 240, __VA_ARGS__);\
        vsapi->setError(out, msg_buff);\
        return;\
    }\
}

static void VS_CC
create_reader(const VSMap *in, VSMap *out, void *user_data, VSCore *core,
              const VSAPI *vsapi)
{
    const char *filter_name = (const char *)user_data;
    char msg_buff[256] = { 0 };
    sprintf(msg_buff, "%s: ", filter_name);
    char *msg = msg_buff + strlen(msg_buff);

    img_hnd_t *ih = (img_hnd_t *)calloc(sizeof(img_hnd_t), 1);
    RET_IF_ERR(!ih, "failed to create handler");

    int num_srcs = vsapi->propNumElements(in, "files");
    RET_IF_ERR(num_srcs < 1, "no source file");
    ih->vi[0].numFrames = num_srcs;

    ih->src = (src_info_t *)malloc(sizeof(src_info_t) * num_srcs);
    RET_IF_ERR(!ih->src, "failed to allocate array of src infomation");

    ih->tjhandle = tjInitDecompress();
    RET_IF_ERR(!ih->tjhandle, "%s", tjGetErrorStr());

    ih->src_buff = (uint8_t *)malloc(INITIAL_SRC_BUFF_SIZE);
    RET_IF_ERR(!ih->src_buff, "failed to allocate src read buffer");
    ih->src_buff_size = INITIAL_SRC_BUFF_SIZE;

    int err;

    int alpha = (int)vsapi->propGetInt(in, "alpha", 0, &err);
    if (err) {
        alpha = 0;
    }
    ih->enable_alpha = !!alpha;

    vs_args_t va = {in, out, core, vsapi, 0, 0, 0, 0, 0};
    for (int i = 0; i < num_srcs; i++) {
        ih->src[i].name = vsapi->propGetData(in, "files", i, &err);
        RET_IF_ERR(err || strlen(ih->src[i].name) == 0,
                   "zero length file name was found");
        const char *cs = check_src_props(ih, i, &va);
        RET_IF_ERR(cs, "file %d: %s", i, cs);
    }
    if (va.variable_width != 0) {
        ih->vi[0].width = 0;
    }
    if (va.variable_height != 0) {
        ih->vi[0].height = 0;
    }
    if (va.variable_format != 0) {
        ih->vi[0].format = NULL;
    }

    uint8_t *buff = (uint8_t *)malloc(va.max_row_size * va.max_height + 32);
    RET_IF_ERR(!buff, "failed to allocate image buffer");
    ih->image_buff = buff;

    ih->png_row_index = (uint8_t **)malloc(sizeof(uint8_t *) * va.max_height);
    RET_IF_ERR(!ih->png_row_index, "failed to allocate image buffer index");
    for (int i = 0; i < va.max_height; i++) {
        ih->png_row_index[i] = buff;
        buff += va.max_row_size;
    }

    ih->vi[0].fpsNum = vsapi->propGetInt(in, "fpsnum", 0, &err);
    if (err) {
        ih->vi[0].fpsNum = 24;
    }
    ih->vi[0].fpsDen = vsapi->propGetInt(in, "fpsden", 0, &err);
    if (err) {
        ih->vi[0].fpsDen = 1;
    }

    ih->vi[1] = ih->vi[0];
    if (ih->enable_alpha && ih->vi[0].format) {
        VSPresetFormat pf = ih->vi[0].format->bytesPerSample == 1 ? pfGray8 : pfGray16;
        ih->vi[1].format = vsapi->getFormatPreset(pf, core);
    }

    vsapi->createFilter(in, out, filter_name, vs_init, img_get_frame,
                        close_handler, fmSerial, 0, ih, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(
    VSConfigPlugin f_config, VSRegisterFunction f_register, VSPlugin *plugin)
{
    f_config("chikuzen.does.not.have.his.own.domain.imgr", "imgr",
             "Image reader for VapourSynth " VS_IMGR_VERSION,
             VAPOURSYNTH_API_VERSION, 1, plugin);
    f_register("Read",
               "files:data[];fpsnum:int:opt;fpsden:int:opt;alpha:int:opt;",
               create_reader, NULL, plugin);
}
