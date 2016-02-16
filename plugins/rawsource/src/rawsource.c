/*
  rawsource.c: Raw Format Reader for VapourSynth

  This file is a part of vsrawsource

  Copyright (C) 2012  Oka Motofumi

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


#include "rawsource.h"
#include "VapourSynth.h"

#define FORMAT_MAX_LEN 32


typedef struct rs_hndle rs_hnd_t;
typedef void (VS_CC *func_write_frame)(rs_hnd_t *, VSFrameRef **, const VSAPI *,
                                       VSCore *);

struct rs_hndle {
    FILE *file;
    int64_t file_size;
    uint32_t frame_size;
    char src_format[FORMAT_MAX_LEN];
    int order[4];
    int off_header;
    int off_frame;
    int sar_num;
    int sar_den;
    int row_adjust;
    int has_alpha;
    int64_t *index;
    uint64_t *total_pix;
    uint8_t *frame_buff;
    func_write_frame write_frame;
    VSVideoInfo vi[2];
};



typedef struct {
    const VSMap *in;
    VSMap *out;
    VSCore *core;
    const VSAPI *vsapi;
} vs_args_t;


static const char *open_source_file(rs_hnd_t *rh, const char *src_name)
{
    struct stat st;
#ifdef _WIN32
    wchar_t tmp[FILENAME_MAX * 4];
    MultiByteToWideChar(CP_UTF8, 0, src_name, -1, tmp, FILENAME_MAX * 4);

    if (wstat(tmp, &st) != 0) {
#else
    if (stat(src_name, &st) != 0) {
#endif
    return "source does not exist.";
    }

    if (st.st_size == 0) {
        return "failed to get file size.";
    }
    rh->file_size = st.st_size;
#ifdef _WIN32
    rh->file = _wfopen(tmp, L"rb");
#else
    rh->file = fopen(src_name, "rb");
#endif
    if (!rh->file) {
        return "failed to open source file";
    }

    return NULL;
}


static void VS_CC
rs_bit_blt(uint8_t *srcp, int row_size, int height, VSFrameRef *dst, int plane,
           const VSAPI *vsapi)
{
    uint8_t *dstp = vsapi->getWritePtr(dst, plane);
    int dst_stride = vsapi->getStride(dst, plane);

    if (row_size == dst_stride) {
        memcpy(dstp, srcp, row_size * height);
        return;
    }

    for (int i = 0; i < height; i++) {
        memcpy(dstp, srcp, row_size);
        dstp += dst_stride;
        srcp += row_size;
    }
}


static inline uint32_t VS_CC
bitor8to32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
           ((uint32_t)b2 << 8) | (uint32_t)b3;
}


static void VS_CC
write_planar_frame(rs_hnd_t *rh, VSFrameRef **dst, const VSAPI *vsapi,
                   VSCore *core)
{
    uint8_t *srcp = rh->frame_buff;
    int bps = rh->vi[0].format->bytesPerSample;
    int row_size, height;

    for (int i = 0, num = rh->vi[0].format->numPlanes; i < num; i++) {
        int plane = rh->order[i];
        row_size = vsapi->getFrameWidth(dst[0], plane) * bps;
        row_size = (row_size + rh->row_adjust) & (~rh->row_adjust);
        height = vsapi->getFrameHeight(dst[0], plane);
        rs_bit_blt(srcp, row_size, height, dst[0], plane, vsapi);
        srcp += row_size * height;
    }

    if (rh->has_alpha == 0) {
        return;
    }

    dst[1] = vsapi->newVideoFrame(rh->vi[1].format, rh->vi[1].width,
                                  rh->vi[1].height, NULL, core);
    row_size = vsapi->getFrameWidth(dst[1], 0) * bps;
    row_size = (row_size + rh->row_adjust) & (~rh->row_adjust);
    height = vsapi->getFrameHeight(dst[1], 0);
    rs_bit_blt(srcp, row_size, height, dst[1], 0, vsapi);
}


static void VS_CC
write_nvxx_frame(rs_hnd_t *rh, VSFrameRef **dst, const VSAPI *vsapi,
                 VSCore *core)
{
    struct uv_t {
        uint8_t c[8];
    };

    uint8_t *srcp_orig = rh->frame_buff;

    int row_size = vsapi->getFrameWidth(dst[0], 0);
    row_size = (row_size + rh->row_adjust) & (~rh->row_adjust);
    int height = vsapi->getFrameHeight(dst[0], 0);
    rs_bit_blt(srcp_orig, row_size, height, dst[0], 0, vsapi);

    srcp_orig += row_size * height;
    int src_stride = row_size;
    row_size = (vsapi->getFrameWidth(dst[0], 1) + 3) >> 2;
    height = vsapi->getFrameHeight(dst[0], 1);

    int dst_stride = vsapi->getStride(dst[0], 1);
    uint8_t *dstp0_orig = vsapi->getWritePtr(dst[0], rh->order[1]);
    uint8_t *dstp1_orig = vsapi->getWritePtr(dst[0], rh->order[2]);

    for (int y = 0; y < height; y++) {
        struct uv_t *srcp = (struct uv_t *)(srcp_orig + y * src_stride);
        uint32_t *dstp0 = (uint32_t *)(dstp0_orig + y * dst_stride);
        uint32_t *dstp1 = (uint32_t *)(dstp1_orig + y * dst_stride);
        for (int x = 0; x < row_size; x++) {
            dstp0[x] = bitor8to32(srcp[x].c[6], srcp[x].c[4], srcp[x].c[2],
                                  srcp[x].c[0]);
            dstp1[x] = bitor8to32(srcp[x].c[7], srcp[x].c[5], srcp[x].c[3],
                                  srcp[x].c[1]);
        }
    }
}


static void VS_CC
write_px1x_frame(rs_hnd_t *rh, VSFrameRef **dst, const VSAPI *vsapi,
                 VSCore *core)
{
    struct uv16_t {
        uint16_t c[2];
    };

    uint8_t *srcp_orig = rh->frame_buff;

    int row_size = vsapi->getFrameWidth(dst[0], 0) << 1;
    row_size = (row_size + rh->row_adjust) & (~rh->row_adjust);
    int height = vsapi->getFrameHeight(dst[0], 0);
    rs_bit_blt(srcp_orig, row_size, height, dst[0], 0, vsapi);

    srcp_orig += row_size * height;
    int src_stride = row_size;
    row_size = vsapi->getFrameWidth(dst[0], 1);
    height = vsapi->getFrameHeight(dst[0], 1);
    int dst_stride = vsapi->getStride(dst[0], 1) >> 1;
    uint16_t *dstp0 = (uint16_t *)vsapi->getWritePtr(dst[0], rh->order[1]);
    uint16_t *dstp1 = (uint16_t *)vsapi->getWritePtr(dst[0], rh->order[2]);

    for (int y = 0; y < height; y++) {
        struct uv16_t *srcp_uv = (struct uv16_t *)(srcp_orig + y *src_stride);
        for (int x = 0; x < row_size; x++) {
            dstp0[x] = srcp_uv[x].c[0];
            dstp1[x] = srcp_uv[x].c[1];
        }
        dstp0 += dst_stride;
        dstp1 += dst_stride;
    }
}


static void VS_CC
write_packed_rgb24(rs_hnd_t *rh, VSFrameRef **dst, const VSAPI *vsapi,
                   VSCore *core)
{
    struct rgb24_t {
        uint8_t c[12];
    };

    uint8_t *srcp_orig = rh->frame_buff;
    int row_size = (rh->vi[0].width + 3) >> 2;
    int height = rh->vi[0].height;
    int src_stride = (rh->vi[0].width * 3 + rh->row_adjust) & (~rh->row_adjust);

    uint8_t *dstp0_orig = vsapi->getWritePtr(dst[0], rh->order[0]);
    uint8_t *dstp1_orig = vsapi->getWritePtr(dst[0], rh->order[1]);
    uint8_t *dstp2_orig = vsapi->getWritePtr(dst[0], rh->order[2]);
    int dst_stride = vsapi->getStride(dst[0], 0);

    for (int y = 0; y < height; y++) {
        struct rgb24_t *srcp = (struct rgb24_t *)(srcp_orig + y * src_stride);
        uint32_t *dstp0 = (uint32_t *)(dstp0_orig + y * dst_stride);
        uint32_t *dstp1 = (uint32_t *)(dstp1_orig + y * dst_stride);
        uint32_t *dstp2 = (uint32_t *)(dstp2_orig + y * dst_stride);
        for (int x = 0; x < row_size; x++) {
            dstp0[x] = bitor8to32(srcp[x].c[9], srcp[x].c[6],
                                  srcp[x].c[3], srcp[x].c[0]);
            dstp1[x] = bitor8to32(srcp[x].c[10], srcp[x].c[7],
                                  srcp[x].c[4], srcp[x].c[1]);
            dstp2[x] = bitor8to32(srcp[x].c[11], srcp[x].c[8],
                                  srcp[x].c[5], srcp[x].c[2]);
        }
    }
}


static void VS_CC
write_packed_rgb48(rs_hnd_t *rh, VSFrameRef **dst, const VSAPI *vsapi,
                   VSCore *core)
{
    struct rgb48_t {
        uint16_t c[3];
    };

    uint8_t *srcp_orig = rh->frame_buff;
    int src_stride = (rh->vi[0].width * 6 + rh->row_adjust) & (~rh->row_adjust);
    int width = rh->vi[0].width;
    int height = rh->vi[0].height;

    uint16_t *dstp0 = (uint16_t *)vsapi->getWritePtr(dst[0], rh->order[0]);
    uint16_t *dstp1 = (uint16_t *)vsapi->getWritePtr(dst[0], rh->order[1]);
    uint16_t *dstp2 = (uint16_t *)vsapi->getWritePtr(dst[0], rh->order[2]);
    int stride = vsapi->getStride(dst[0], 0) >> 1;;

    for (int y = 0; y < height; y++) {
        struct rgb48_t *srcp = (struct rgb48_t *)(srcp_orig + y * src_stride);
        for (int x = 0; x < width; x++) {
            dstp0[x] = srcp[x].c[0];
            dstp1[x] = srcp[x].c[1];
            dstp2[x] = srcp[x].c[2];
        }
        dstp0 += stride;
        dstp1 += stride;
        dstp2 += stride;
    }
}


static void VS_CC
write_packed_rgb32(rs_hnd_t *rh, VSFrameRef **dst, const VSAPI *vsapi,
                   VSCore *core)
{
    struct rgb32_t {
        uint8_t c[16];
    };

    uint8_t *srcp_orig = rh->frame_buff;
    int src_stride = ((rh->vi[0].width << 2) + rh->row_adjust) & (~rh->row_adjust);
    int row_size = (rh->vi[0].width + 3) >> 2;
    int height = rh->vi[0].height;

    int *order = rh->order;

    dst[1] = vsapi->newVideoFrame(rh->vi[1].format, rh->vi[1].width,
                                  rh->vi[1].height, NULL, core);

    uint32_t *dstp[4];
    for (int i = 0; i < 3; i++) {
        dstp[i] = (uint32_t *)vsapi->getWritePtr(dst[0], i);
    }
    dstp[3] = (uint32_t *)vsapi->getWritePtr(dst[1], 0);
    int dst_stride = vsapi->getStride(dst[0], 0) >> 2;

    for (int y = 0; y < height; y++) {
        struct rgb32_t *srcp = (struct rgb32_t *)(srcp_orig + y * src_stride);
        for (int x = 0; x < row_size; x++) {
            *(dstp[order[0]] + x) = bitor8to32(srcp[x].c[12], srcp[x].c[8],
                                               srcp[x].c[4], srcp[x].c[0]);
            *(dstp[order[1]] + x) = bitor8to32(srcp[x].c[13], srcp[x].c[9],
                                               srcp[x].c[5], srcp[x].c[1]);
            *(dstp[order[2]] + x) = bitor8to32(srcp[x].c[14], srcp[x].c[10],
                                               srcp[x].c[6], srcp[x].c[2]);
            *(dstp[order[3]] + x) = bitor8to32(srcp[x].c[15], srcp[x].c[11],
                                               srcp[x].c[7], srcp[x].c[3]);
        }
        for (int i = 0; i < 4; i++) {
            dstp[i] += dst_stride;
        }
    }
}


static void VS_CC
write_packed_yuv422(rs_hnd_t *rh, VSFrameRef **dst, const VSAPI *vsapi,
                    VSCore *core)
{
    struct packed422_t {
        uint8_t c[4];
    };

    uint8_t *srcp_orig = rh->frame_buff;
    int src_stride = ((rh->vi[0].width << 1) + rh->row_adjust) & (~rh->row_adjust);
    int width = rh->vi[0].width >> 1;
    int height = rh->vi[0].height;
    int o0 = rh->order[0];
    int o1 = rh->order[1];
    int o2 = rh->order[2];
    int o3 = rh->order[3];

    uint8_t *dstp[3];
    int padding[3];
    for (int i = 0; i < 3; i++) {
        dstp[i] = vsapi->getWritePtr(dst[0], i);
        padding[i] = vsapi->getStride(dst[0], i) - vsapi->getFrameWidth(dst[0], i);
    }

    for (int y = 0; y < height; y++) {
        struct packed422_t *srcp = (struct packed422_t *)(srcp_orig + y * src_stride);
        for (int x = 0; x < width; x++) {
            *(dstp[o0]++) = srcp[x].c[0];
            *(dstp[o1]++) = srcp[x].c[1];
            *(dstp[o2]++) = srcp[x].c[2];
            *(dstp[o3]++) = srcp[x].c[3];
        }
        dstp[0] += padding[0];
        dstp[1] += padding[1];
        dstp[2] += padding[2];
    }
}


static int VS_CC create_index(rs_hnd_t *rh)
{
    int num_frames = rh->vi[0].numFrames;

    int64_t *index = (int64_t *)malloc(sizeof(int64_t) * num_frames);
    if (!index) {
        return -1;
    }

    int off_frame = rh->off_frame;
    uint32_t frame_size = rh->frame_size;
    int64_t pos = rh->off_header;
    for (int i = 0; i < num_frames; i++) {
        pos += off_frame;
        index[i] = pos;
        pos += frame_size;
    }

    rh->index = index;
    return 0;
}


static inline const char * VS_CC get_format(char *ctag)
{
    const struct {
        char *tag;
        char *format;
    } table[] = {
        { "420jpeg",  "YUV420P8"  },
        { "420mpeg2", "YUV420P8"  },
        { "420paldv", "YUV420P8"  },
        { "420p9",    "YUV420P9"  },
        { "420p10",   "YUV420P10" },
        { "420p16",   "YUV420P16" },
        { "411",      "YUV411P8"  },
        { "422",      "YUV422P8"  },
        { "422p9",    "YUV422P9"  },
        { "422p10",   "YUV422P10" },
        { "422p16",   "YUV422P16" },
        { "444",      "YUV444P8"  },
        { "444p9",    "YUV444P9"  },
        { "444p10",   "YUV444P10" },
        { "444p16",   "YUV444P16" },
        { "444alpha", "YUV444P8A" },
        { ctag,       "YUV420P8"  }
    };

    int i = 0;
    while (strcasecmp(ctag, table[i].tag) != 0) i++;
    return table[i].format;
}


#define PARSE_HEADER(cond, ...) {\
    i += 2;\
    sscanf(buff + i, __VA_ARGS__);\
    if (cond) {\
        return -1;\
    }\
}
static int VS_CC check_y4m(rs_hnd_t *rh)
{
    const char *stream_header = "YUV4MPEG2";
    const char *frame_header = "FRAME\n";
    size_t sh_length = strlen(stream_header);
    size_t fh_length = strlen(frame_header);
    char buff[256] = { 0 };
    char ctag[32] = { 0 };

    fread(buff, 1, sizeof buff, rh->file);
    if (strncmp(buff, stream_header, sh_length) != 0) {
        return 1;
    }

    int i;
    for (i = sh_length; buff[i] != '\n'; i++) {
        if (!strncmp(buff + i, " W", 2)) {
            PARSE_HEADER(rh->vi[0].width < 1, "%d", &rh->vi[0].width);
        }
        if (!strncmp(buff + i, " H", 2)) {
            PARSE_HEADER(rh->vi[0].height < 1, "%d", &rh->vi[0].height);
        }
        if (!strncmp(buff + i, " F", 2)) {
            i += 2;
            sscanf(buff + i, "%"SCNi64":%"SCNi64, &rh->vi[0].fpsNum, &rh->vi[0].fpsDen);
            if (rh->vi[0].fpsNum < 1 || rh->vi[0].fpsDen < 1) {
                return -1;
            }
        }
        if (!strncmp(buff + i, " A", 2)) {
            i += 2;
            sscanf(buff + i, "%d:%d", &rh->sar_num, &rh->sar_den);
            if (rh->sar_num < 0 || rh->sar_den < 0) {
                return -1;
            }
        }
        if (!strncmp(buff + i, " I", 2)) {
            i += 2;
            if (buff[i] == 'm') {
                return -2;
            }
        }
        if (!strncmp(buff + i, " C", 2)) {
            i += 2;
            sscanf(buff + i, "%s", ctag);
            strcpy(rh->src_format, get_format(ctag));
        }
        if (i == sizeof buff - 1) {
            return -2;
        }
    }

    rh->off_header = ++i;

    if (strncmp(buff + i, frame_header, strlen(frame_header)) != 0) {
        return -2;
    }

    rh->off_frame = fh_length;

    if (strlen(rh->src_format) == 0) {
        strcpy(rh->src_format, "YUV420P8");
    }

    rh->row_adjust = 1;
    return 0;
}
#undef PARSE_HEADER


static inline int32_t abs_i(int32_t i)
{
    return i < 0 ? -i : i;
}


static int check_bmp(rs_hnd_t *rh)
{
    uint32_t offset_data;
    bmp_info_header_t info = { 0 };

    fseek(rh->file, 10, SEEK_SET);
    fread(&offset_data, 1, sizeof(uint32_t), rh->file);
    fread(&info, 1, sizeof(bmp_info_header_t), rh->file);

    if (info.num_planes != 1 || info.fourcc != 0 ||
        (info.bits_per_pixel != 24 && info.bits_per_pixel != 32)) {
        return 1;
    }

    rh->vi[0].width = abs_i(info.width);
    rh->vi[0].height = abs_i(info.height);
    strcpy(rh->src_format, info.bits_per_pixel == 24 ? "BGR" : "BGRA");
    rh->off_header = offset_data;
    rh->row_adjust = 4;

    return 0;
}


static int check_header(rs_hnd_t *rh)
{
    char head[2];
    fread(head, 1, 2, rh->file);
    fseek(rh->file, 0, SEEK_SET);

    if (head[0] == 'B' && head[1] == 'M') {
        return check_bmp(rh);
    }

    if (head[0] == 'Y' && head[1] == 'U') {
        return check_y4m(rh);
    }

    return 1;
}


static const char * VS_CC check_args(rs_hnd_t *rh, vs_args_t *va)
{
    const struct {
        char *format_name;
        int subsample_h;
        int subsample_v;
        int num_planes;
        int bytes_per_row_sample;
        int has_alpha;
        int order[4];
        VSPresetFormat vsformat;
        func_write_frame func;
    } table[] = {
        { "i420",      2, 2, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV420P8,  write_planar_frame  },
        { "IYUV",      2, 2, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV420P8,  write_planar_frame  },
        { "YV12",      2, 2, 3, 1, 0, { 0, 2, 1, 9 }, pfYUV420P8,  write_planar_frame  },
        { "YUV420P8",  2, 2, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV420P8,  write_planar_frame  },
        { "i422",      2, 1, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV422P8,  write_planar_frame  },
        { "YV16",      2, 1, 3, 1, 0, { 0, 2, 1, 9 }, pfYUV422P8,  write_planar_frame  },
        { "YUV422P8",  2, 1, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV422P8,  write_planar_frame  },
        { "i444",      1, 1, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV444P8,  write_planar_frame  },
        { "YV24",      1, 1, 3, 1, 0, { 0, 2, 1, 9 }, pfYUV444P8,  write_planar_frame  },
        { "YUV444P8",  1, 1, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV444P8,  write_planar_frame  },
        { "Y8",        1, 1, 1, 1, 0, { 0, 9, 9, 9 }, pfGray8,     write_planar_frame  },
        { "Y800",      1, 1, 1, 1, 0, { 0, 9, 9, 9 }, pfGray8,     write_planar_frame  },
        { "GRAY",      1, 1, 1, 1, 0, { 0, 9, 9, 9 }, pfGray8,     write_planar_frame  },
        { "GRAY16",    1, 1, 1, 2, 0, { 0, 9, 9, 9 }, pfGray16,    write_planar_frame  },
        { "YV411",     4, 1, 3, 1, 0, { 0, 2, 1, 9 }, pfYUV444P8,  write_planar_frame  },
        { "YUV411P8",  4, 1, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV411P8,  write_planar_frame  },
        { "YUV9",      4, 4, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV410P8,  write_planar_frame  },
        { "YVU9",      4, 4, 3, 1, 0, { 0, 2, 1, 9 }, pfYUV410P8,  write_planar_frame  },
        { "YUV410P8",  4, 4, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV410P8,  write_planar_frame  },
        { "YUV440P8",  1, 2, 3, 1, 0, { 0, 1, 2, 9 }, pfYUV440P8,  write_planar_frame  },
        { "YUV420P9",  2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV420P9,  write_planar_frame  },
        { "YUV420P10", 2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV420P10, write_planar_frame  },
        { "YUV420P16", 2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV420P16, write_planar_frame  },
        { "YUV422P9",  2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV422P9,  write_planar_frame  },
        { "YUV422P10", 2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV422P10, write_planar_frame  },
        { "YUV422P16", 2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV422P16, write_planar_frame  },
        { "YUV444P9",  2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV444P9,  write_planar_frame  },
        { "YUV444P10", 2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV444P10, write_planar_frame  },
        { "YUV444P16", 2, 2, 3, 2, 0, { 0, 1, 2, 9 }, pfYUV444P16, write_planar_frame  },
        { "YUV444P8A", 1, 1, 4, 1, 1, { 0, 1, 2, 3 }, pfYUV444P8,  write_planar_frame  },
        { "YUY2",      2, 1, 1, 4, 0, { 0, 1, 0, 2 }, pfYUV422P8,  write_packed_yuv422 },
        { "YUYV",      2, 1, 1, 4, 0, { 0, 1, 0, 2 }, pfYUV422P8,  write_packed_yuv422 },
        { "UYVY",      2, 1, 1, 4, 0, { 1, 0, 2, 0 }, pfYUV422P8,  write_packed_yuv422 },
        { "YVYU",      2, 1, 1, 4, 0, { 0, 1, 0, 2 }, pfYUV422P8,  write_packed_yuv422 },
        { "VYUY",      2, 1, 1, 4, 0, { 2, 0, 1, 0 }, pfYUV422P8,  write_packed_yuv422 },
        { "BGR",       1, 1, 1, 3, 0, { 2, 1, 0, 9 }, pfRGB24,     write_packed_rgb24  },
        { "RGB",       1, 1, 1, 3, 0, { 0, 1, 2, 9 }, pfRGB24,     write_packed_rgb24  },
        { "BGRA",      1, 1, 1, 4, 1, { 2, 1, 0, 3 }, pfRGB24,     write_packed_rgb32  },
        { "ABGR",      1, 1, 1, 4, 1, { 3, 2, 1, 0 }, pfRGB24,     write_packed_rgb32  },
        { "RGBA",      1, 1, 1, 4, 1, { 0, 1, 2, 3 }, pfRGB24,     write_packed_rgb32  },
        { "ARGB",      1, 1, 1, 4, 1, { 3, 0, 1, 2 }, pfRGB24,     write_packed_rgb32  },
        { "AYUV",      1, 1, 1, 4, 1, { 3, 0, 1, 2 }, pfYUV444P8,  write_packed_rgb32  },
        { "GBRP8",     1, 1, 3, 1, 0, { 1, 2, 0, 9 }, pfRGB24,     write_planar_frame  },
        { "RGBP8",     1, 1, 3, 1, 0, { 0, 1, 2, 9 }, pfRGB24,     write_planar_frame  },
        { "GBRP9",     1, 1, 3, 2, 0, { 1, 2, 0, 9 }, pfRGB27,     write_planar_frame  },
        { "RGBP9",     1, 1, 3, 2, 0, { 0, 1, 2, 9 }, pfRGB27,     write_planar_frame  },
        { "GBRP10",    1, 1, 3, 2, 0, { 1, 2, 0, 9 }, pfRGB30,     write_planar_frame  },
        { "RGBP10",    1, 1, 3, 2, 0, { 0, 1, 2, 9 }, pfRGB30,     write_planar_frame  },
        { "GBRP16",    1, 1, 3, 2, 0, { 1, 2, 0, 9 }, pfRGB48,     write_planar_frame  },
        { "RGBP16",    1, 1, 3, 2, 0, { 0, 1, 2, 9 }, pfRGB48,     write_planar_frame  },
        { "BGR48",     1, 1, 3, 2, 0, { 2, 1, 0, 3 }, pfRGB48,     write_packed_rgb48  },
        { "RGB48",     1, 1, 3, 2, 0, { 0, 1, 2, 3 }, pfRGB48,     write_packed_rgb48  },
        { "NV12",      2, 2, 2, 1, 0, { 0, 1, 2, 9 }, pfYUV420P8,  write_nvxx_frame    },
        { "NV21",      2, 2, 2, 1, 0, { 0, 2, 1, 9 }, pfYUV420P8,  write_nvxx_frame    },
        { "P010",      2, 2, 2, 2, 0, { 0, 1, 2, 9 }, pfYUV420P16, write_px1x_frame    },
        { "P016",      2, 2, 2, 2, 0, { 0, 1, 2, 9 }, pfYUV420P16, write_px1x_frame    },
        { "P210",      2, 1, 2, 2, 0, { 0, 1, 2, 9 }, pfYUV422P16, write_px1x_frame    },
        { "P216",      2, 1, 2, 2, 0, { 0, 1, 2, 9 }, pfYUV422P16, write_px1x_frame    },
        { rh->src_format, 0 }
    };

    int i = 0;
    while (strcasecmp(rh->src_format, table[i].format_name) != 0) i++;
    if (table[i].vsformat == 0) {
        return "unsupported format";
    }
    if (rh->vi[0].width % table[i].subsample_h != 0) {
        return "invalid width was specified";
    }
    if (rh->vi[0].height % table[i].subsample_v != 0) {
        return "invalid height was specified";
    }

    int frame_size = 0;
    for (int p = 0; p < table[i].num_planes; p++) {
        int width_plane =
            (rh->vi[0].width / (p ? table[i].subsample_h : 1)) << (table[i].num_planes == 2 && p ? 1 : 0);
        int height_plane = rh->vi[0].height / (p ? table[i].subsample_h : 1);
        int row_size_plane =
            (width_plane * table[i].bytes_per_row_sample + rh->row_adjust) & (~rh->row_adjust);
        frame_size += row_size_plane * height_plane;
    }
    rh->frame_size = frame_size;
    rh->vi[0].format = va->vsapi->getFormatPreset(table[i].vsformat, va->core);
    memcpy(rh->order, table[i].order, sizeof(int) * 4);
    rh->write_frame = table[i].func;
    rh->has_alpha = table[i].has_alpha;

    return NULL;
}


static void close_handler(rs_hnd_t *rh)
{
    if (!rh) {
        return;
    }
    if (rh->frame_buff) {
        free(rh->frame_buff);
    }
    if (rh->index) {
        free(rh->index);
    }
    if (rh->file) {
        fclose(rh->file);
    }
    free(rh);
}


static void VS_CC
vs_close(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    rs_hnd_t *rh = (rs_hnd_t *)instance_data;
    close_handler(rh);
}


static void VS_CC
vs_init(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
        VSCore *core, const VSAPI *vsapi)
{
    rs_hnd_t *rh = (rs_hnd_t *)*instance_data;
    vsapi->setVideoInfo(rh->vi, rh->has_alpha + 1, node);
}


static const VSFrameRef * VS_CC
rs_get_frame(int n, int activation_reason, void **instance_data,
             void **frame_data, VSFrameContext *frame_ctx, VSCore *core,
             const VSAPI *vsapi)
{
    if (activation_reason != arInitial) {
        return NULL;
    }

    rs_hnd_t *rh = (rs_hnd_t *)*instance_data;

    int frame_number = n;
    if (n >= rh->vi[0].numFrames) {
        frame_number = rh->vi[0].numFrames - 1;
    }

    if (rs_fseek(rh->file, rh->index[frame_number], SEEK_SET) != 0 ||
        fread(rh->frame_buff, 1, rh->frame_size, rh->file) < rh->frame_size) {
        return NULL;
    }

    VSFrameRef *dst[2];
    dst[0] = vsapi->newVideoFrame(rh->vi[0].format, rh->vi[0].width, rh->vi[0].height,
                                  NULL, core);

    VSMap *props = vsapi->getFramePropsRW(dst[0]);
    vsapi->propSetInt(props, "_DurationNum", rh->vi[0].fpsDen, paReplace);
    vsapi->propSetInt(props, "_DurationDen", rh->vi[0].fpsNum, paReplace);
    vsapi->propSetInt(props, "_SARNum", rh->sar_num, paReplace);
    vsapi->propSetInt(props, "_SARDen", rh->sar_den, paReplace);

    rh->write_frame(rh, dst, vsapi, core);

    if (rh->has_alpha == 0) {
        return dst[0];
    }

    if (vsapi->getOutputIndex(frame_ctx) == 0) {
        vsapi->freeFrame(dst[1]);
        return dst[0];
    }

    vsapi->freeFrame(dst[0]);
    props = vsapi->getFramePropsRW(dst[1]);
    vsapi->propSetInt(props, "_DurationNum", rh->vi[1].fpsDen, paReplace);
    vsapi->propSetInt(props, "_DurationDen", rh->vi[1].fpsNum, paReplace);
    vsapi->propSetInt(props, "_SARNum", rh->sar_num, paReplace);
    vsapi->propSetInt(props, "_SARDen", rh->sar_den, paReplace);

    return dst[1];
}


static void VS_CC
set_args_int(int *p, int default_value, const char *arg, vs_args_t *va)
{
    int err;
    *p = (int)va->vsapi->propGetInt(va->in, arg, 0, &err);
    if (err) {
        *p = default_value;
    }
}


static void VS_CC
set_args_int64(int64_t *p, int default_value, const char *arg, vs_args_t *va)
{
    int err;
    *p = va->vsapi->propGetInt(va->in, arg, 0, &err);
    if (err) {
        *p = default_value;
    }
}


static void VS_CC
set_args_data(char *p, const char *default_value, const char *arg, size_t n,
              vs_args_t *va)
{
    int err;
    const char *data = va->vsapi->propGetData(va->in, arg, 0, &err);
    strncpy(p, err ? default_value : data, n);
}


#define RET_IF_ERROR(cond, ...) \
{\
    if (cond) {\
        close_handler(rh);\
        snprintf(msg, 240, __VA_ARGS__);\
        vsapi->setError(out, msg_buff);\
        return;\
    }\
}

static void VS_CC
create_source(const VSMap *in, VSMap *out, void *user_data, VSCore *core,
              const VSAPI *vsapi)
{
    char msg_buff[256] = "raws: ";
    char *msg = msg_buff + strlen(msg_buff);

    rs_hnd_t *rh = (rs_hnd_t *)calloc(sizeof(rs_hnd_t), 1);
    RET_IF_ERROR(!rh, "couldn't create handler");

    const char *err =
        open_source_file(rh, vsapi->propGetData(in, "source", 0, 0));
    RET_IF_ERROR(err, "%s", err);

    int header = check_header(rh);
    RET_IF_ERROR(header == -1, "invalid YUV4MPEG2 header was found");
    RET_IF_ERROR(header == -2, "unsupported YUV4MPEG2 header was found");

    vs_args_t va = { in, out, core, vsapi };

    if (header > 0) {
        set_args_int(&rh->vi[0].width, 720, "width", &va);
        set_args_int(&rh->vi[0].height, 480, "height", &va);
        set_args_int(&rh->off_header, 0, "off_header", &va);
        set_args_int(&rh->off_frame, 0, "off_frame", &va);
        set_args_int(&rh->sar_num, 1, "sarnum", &va);
        set_args_int(&rh->sar_den, 1, "sarden", &va);
        set_args_int(&rh->row_adjust, 1, "rowbytes_align", &va);
        set_args_data(rh->src_format, "I420", "src_fmt", FORMAT_MAX_LEN, &va);
    }

    if (rh->vi[0].fpsNum == 0 && rh->vi[0].fpsDen == 0) {
        set_args_int64(&rh->vi[0].fpsNum, 30000, "fpsnum", &va);
        set_args_int64(&rh->vi[0].fpsDen, 1001, "fpsden", &va);
    }

    rh->row_adjust--;
    if (rh->row_adjust < 0 || rh->row_adjust > 15) {
        rh->row_adjust = 0;
    }

    const char *ca = check_args(rh, &va);
    RET_IF_ERROR(ca, "%s", ca);

    rh->vi[0].numFrames =
        (int)((rh->file_size - rh->off_header) / (rh->off_frame + rh->frame_size));
    RET_IF_ERROR(rh->vi[0].numFrames < 1, "too small file size");

    RET_IF_ERROR(create_index(rh), "failed to create index");

    rh->frame_buff = (uint8_t *)malloc(rh->frame_size + 32);
    RET_IF_ERROR(!rh->frame_buff, "failed to allocate buffer");

    if (rh->has_alpha) {
        rh->vi[1] = rh->vi[0];
        VSPresetFormat pf =
            rh->vi[0].format->bytesPerSample == 1 ? pfGray8 : pfGray16;
        rh->vi[1].format = vsapi->getFormatPreset(pf, core);
    }
    vsapi->createFilter(in, out, "Source", vs_init, rs_get_frame, vs_close,
                        fmSerial, 0, rh, core);
}
#undef RET_IF_ERROR


VS_EXTERNAL_API(void) VapourSynthPluginInit(
    VSConfigPlugin f_config, VSRegisterFunction f_register, VSPlugin *plugin)
{
    f_config("chikuzen.does.not.have.his.own.domain.raws", "raws",
             "Raw-format file Reader for VapourSynth " VS_RAWS_VERSION,
             VAPOURSYNTH_API_VERSION, 1, plugin);
    f_register("Source", "source:data;width:int:opt;height:int:opt;"
               "fpsnum:int:opt;fpsden:int:opt;sarnum:int:opt;sarden:int:opt;"
               "src_fmt:data:opt;off_header:int:opt;off_frame:int:opt;"
               "rowbytes_align:int:opt", create_source, NULL, plugin);
}
