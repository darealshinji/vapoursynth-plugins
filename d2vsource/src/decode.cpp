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

#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <stdio.h>
}

#include "compat.hpp"
#include "d2v.hpp"
#include "decode.hpp"
#include "gop.hpp"
#include "libavversion.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

/*
 * AVIO seek function to handle GOP offsets and multi-file support
 * in libavformat without it knowing about it.
 */
static int64_t file_seek(void *opaque, int64_t offset, int whence)
{
    decodecontext *ctx = (decodecontext *) opaque;

    switch(whence) {
    case SEEK_SET: {
        /*
         * This mutli-file seek is likely very broken, but I don't
         * really care much, since it is only used in avformat_find_stream_info,
         * which does its job fine as-is.
         */
        int64_t real_offset = offset + ctx->orig_file_offset;
        unsigned int i;

        for(i = ctx->orig_file; i < ctx->cur_file; i++)
            real_offset -= ctx->file_sizes[i];

        while(real_offset > ctx->file_sizes[ctx->cur_file] && ctx->cur_file != ctx->files.size() - 1) {
            real_offset -= ctx->file_sizes[ctx->cur_file];
            ctx->cur_file++;
        }

        while(real_offset < 0 && ctx->cur_file) {
            ctx->cur_file--;
            real_offset += ctx->file_sizes[ctx->cur_file];
        }

        fseeko(ctx->files[ctx->cur_file], real_offset, SEEK_SET);

        return offset;
    }
    case AVSEEK_SIZE: {
        /*
         * Return the total filesize of all files combined,
         * adjusted for GOP offset.
         */
        int64_t size = -((int64_t) ctx->orig_file_offset);
        unsigned int i;

        for(i = ctx->orig_file; i < ctx->file_sizes.size(); i++)
            size += ctx->file_sizes[i];

        return size;
    }
    default:
        /* Shouldn't need to support anything else for our use case. */
        cout << "Unsupported seek!" << endl;
        return -1;
    }
}

/*
 * AVIO packet reading function to handle GOP offsets and multi-file support
 * in libavformat without it knowing about it.
 */
static int read_packet(void *opaque, uint8_t *buf, int size)
{
    decodecontext *ctx = (decodecontext *) opaque;
    size_t ret;

    /*
     * If we read in less than we got asked for, and we're
     * not on the last file, then start reading seamlessly
     * on the next file.
     */
    ret = fread(buf, 1, size, ctx->files[ctx->cur_file]);
    if (ret < size && ctx->cur_file != ctx->files.size() - 1) {
        ctx->cur_file++;
        fseeko(ctx->files[ctx->cur_file], 0, SEEK_SET);
        ret += fread(buf + ret, 1, size - ret, ctx->files[ctx->cur_file]);
    }

    return ((int) ret);
}

/* Conditionally free all memebers of decodecontext. */
void decodefreep(decodecontext **ctx)
{
    decodecontext *lctx = *ctx;
    unsigned int i;

    if (!lctx)
        return;

    av_freep(&lctx->in);
    av_free_packet(&lctx->inpkt);

    if (lctx->fctx) {
        if (lctx->fctx->pb)
            av_freep(&lctx->fctx->pb);

        avformat_close_input(&lctx->fctx);
    }

    for(i = 0; i < lctx->files.size(); i++)
        fclose(lctx->files[i]);

    lctx->files.clear();
    lctx->file_sizes.clear();

    if (lctx->avctx) {
        avcodec_close(lctx->avctx);
        av_freep(&lctx->avctx);
    }

    delete lctx->fakename;
    delete lctx;

    *ctx = NULL;
}

/* Initialize everything we can with regards to decoding */
decodecontext *decodeinit(d2vcontext *dctx, int threads, string& err)
{
    decodecontext *ret;
    int i, av_ret;

    ret = new decodecontext;

    /* Zero the context to aid in conditional freeing later. */
    memset(ret, 0, sizeof(*ret));

    /* Holds our "filename" we pass to libavformat. */
    ret->fakename = new string;

    /* Set our stream index to -1 (uninitialized). */
    ret->stream_index = -1;

    /* Open each file and stash its size. */
    for(i = 0; i < dctx->num_files; i++) {
        FILE *in;
        int64_t size;

#ifdef _WIN32
        wchar_t filename[_MAX_PATH];

        size = MultiByteToWideChar(CP_UTF8, 0, dctx->files[i].c_str(), -1, filename, ARRAYSIZE(filename));
        if (!size) {
            err  = "Cannot parse file name: ";
            err += dctx->files[i];
            goto fail;
        }

        in = _wfopen(filename, L"rb");
#else
        in = fopen(dctx->files[i].c_str(), "rb");
#endif

        if (!in) {
            err  = "Cannot open file: ";
            err += dctx->files[i];
            goto fail;
        }

        fseeko(in, 0, SEEK_END);
        size = ftello(in);
        fseeko(in, 0, SEEK_SET);

        ret->file_sizes.push_back(size);
        ret->files.push_back(in);
    }

    /*
     * Register all of our demuxers, parsers, and decoders.
     * Ideally, to create a smaller binary, we only enable the
     * following:
     *
     * Demuxers: mpegvideo, mpegps, mpegts.
     * Parsers: mpegvideo, mpegaudio.
     * Decoders: mpeg1video, mpeg2video.
     */
    avcodec_register_all();
    av_register_all();

    /* Set the correct decoder. */
    if (dctx->mpeg_type == 1) {
        ret->incodec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    } else if (dctx->mpeg_type == 2) {
        ret->incodec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
    } else {
        err = "Invalid MPEG Type.";
        goto fail;
    }

    /* Allocate the codec's context. */
    ret->avctx = avcodec_alloc_context3(ret->incodec);
    if (!ret->avctx) {
        err = "Cannot allocate AVCodecContext.";
        goto fail;
    }

    /* Set the IDCT algorithm. */
    ret->avctx->idct_algo = dctx->idct_algo;

    /* Set the thread count. */
    ret->avctx->thread_count = threads;

    /*
     * Enable EMU_EDGE so that we can use buffers that are
     * not padded by 32 pixels.
     */
    ret->avctx->flags |= CODEC_FLAG_EMU_EDGE;

#if !defined(USE_OLD_FFAPI)
    /* Use refcounted frames. */
    ret->avctx->refcounted_frames = 1;
#endif

    /* Open it. */
    av_ret = avcodec_open2(ret->avctx, ret->incodec, NULL);
    if (av_ret < 0) {
        err = "Cannot open decoder.";
        goto fail;
    }

    /* Allocate the scratch buffer for our custom AVIO context. */
    ret->in = (uint8_t *) av_malloc(32 * 1024);
    if (!ret->in) {
        err = "Cannot alloc inbuf.";
        goto fail;
    }

    /* We don't want to hear all the info it has. */
    av_log_set_level(AV_LOG_PANIC);

    return ret;

fail:
    decodefreep(&ret);
    return NULL;
}

int decodeframe(int frame_num, d2vcontext *ctx, decodecontext *dctx, AVFrame *out, string& err)
{
    frame f;
    gop g;
    unsigned int i;
    int o, j, av_ret, offset;
    bool next = true;

    /* Get our frame and the GOP its in. */
    f = ctx->frames[frame_num];
    g = ctx->gops[f.gop];

    /*
     * The offset is how many frames we have to decode from our
     * current position in order to get to the frame we want.
     * The initial offset is obtaiend during the parsing of the
     * D2V file, but it may be more in an open GOP situation,
     * which we handle below.
     */
    offset = f.offset;

    /*
     * If we're in a open GOP situation, then start decoding
     * from the previous GOP (one at most is needed), and adjust
     * out offset accordingly.
     */
    if (!(g.info & GOP_FLAG_CLOSED)) {
        if (f.gop == 0) {
            int n = 0;

            /*
             * Adjust the offset by the number of frames
             * that require of the previous GOP when the
             * first GOP is open.
             */
            while(!(g.flags[n] & GOP_FLAG_PROGRESSIVE))
                n++;

            /*
             * Only adjust the offset if it's feasible;
             * that is, if it produces a positive offset.
             */
            offset = n > f.offset ? 0 : f.offset - n;

            /*
             * If the offset is 0, force decoding.
             *
             * FIXME: This method increases the number
             * of frames to be decoded.
             */
            next = offset != 0;
        } else {
            int n = frame_num;
            frame t = ctx->frames[n];

            g = ctx->gops[f.gop - 1];

            /*
             * Find the offset of the last frame in the
             * previous GOP and add it to our offset.
             */
            while(t.offset)
                t = ctx->frames[--n];

            t = ctx->frames[--n];

            /*
             * Subtract number of frames that require the
             * previous GOP.
             */
            n = 0;
            if (!(g.info & GOP_FLAG_CLOSED))
                while(!(g.flags[n] & GOP_FLAG_PROGRESSIVE))
                    n++;

            offset += t.offset + 1 - n;
        }
    }

    /*
     * Check if we're decoding linearly, and if the GOP
     * of the current frame and previous frame are either
     * the same, or also linear. If so, we can decode
     * linearly.
     */
    next = next && (dctx->last_gop == f.gop || dctx->last_gop == f.gop - 1) && dctx->last_frame == frame_num - 1;

    /* Skip GOP initialization if we're decoding linearly. */
    if (!next) {
        /* Free out format and AVIO contexts from the previous seek. */
        if (dctx->fctx) {
            if (dctx->fctx->pb)
                av_freep(&dctx->fctx->pb);

            avformat_close_input(&dctx->fctx);
        }

        /* Seek to our GOP offset and stash the info. */
        fseeko(dctx->files[g.file], g.pos, SEEK_SET);
        dctx->orig_file_offset = g.pos;
        dctx->orig_file        = g.file;
        dctx->cur_file         = g.file;

        /* Allocate format context. */
        dctx->fctx = avformat_alloc_context();
        if (!dctx->fctx) {
            err = "Cannot allocate AVFormatContext.";
            goto dfail;
        }

        /*
         * Find the demuxer for our input type, and also set
         * the "filename" that we pass to libavformat when
         * we open the demuxer with our custom AVIO context.
         */
        if (ctx->stream_type == ELEMENTARY) {
            dctx->fctx->iformat = av_find_input_format("mpegvideo");
            *dctx->fakename      = "fakevideo.m2v";
        } else if (ctx->stream_type == PROGRAM) {
            dctx->fctx->iformat = av_find_input_format("mpeg");
            *dctx->fakename      = "fakevideo.vob";
        } else if (ctx->stream_type == TRANSPORT) {
            dctx->fctx->iformat = av_find_input_format("mpegts");
            *dctx->fakename      = "fakevideo.ts";
        } else {
            err = "Unsupported format.";
            goto dfail;
        }

        /*
         * Initialize out custom AVIO context that libavformat
         * will use instead of a file. It uses our custom packet
         * reading and seeking functions that transparently work
         * with our indexed GOP offsets and multiple files.
         */
        dctx->fctx->pb = avio_alloc_context(dctx->in, 32 * 1024, 0, dctx, read_packet, NULL, file_seek);

        /* Open the demuxer. */
        av_ret = avformat_open_input(&dctx->fctx, (*dctx->fakename).c_str(), NULL, NULL);
        if (av_ret < 0) {
            err = "Cannot open buffer in libavformat.";
            goto dfail;
        }

        /*
         * Flush the buffers of our codec's context so we
         * don't need to re-initialize it.
         */
        avcodec_flush_buffers(dctx->avctx);

        /*
         * Call the abomination function to find out
         * how many streams we have.
         */
        avformat_find_stream_info(dctx->fctx, NULL);

        /* Free and re-initialize any existing packet. */
        av_free_packet(&dctx->inpkt);
        av_init_packet(&dctx->inpkt);
    }

    /*
     * Set our stream index if we need to.
     * Set it to the stream that matches our MPEG-TS PID if applicable.
     */
    if (dctx->stream_index == -1) {
        if (ctx->ts_pid) {
            for(i = 0; i < dctx->fctx->nb_streams; i++)
                if (dctx->fctx->streams[i]->id == ctx->ts_pid)
                    break;
        } else {
            for(i = 0; i < dctx->fctx->nb_streams; i++)
                if (dctx->fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
                    break;
        }

        if (i >= dctx->fctx->nb_streams) {
            if (ctx->ts_pid)
                err = "PID does not exist in source file.";
            else
                err = "No video stream found.";

            goto dfail;
        }

        dctx->stream_index = (int) i;
    }

    /*
     * We don't need to read a new packet in if we are decoding
     * linearly, since it's still there from the previous iteration.
     */
    if (!next)
        av_read_frame(dctx->fctx, &dctx->inpkt);

    /* If we're decoding linearly, there is obviously no offset. */
    o = next ? 0 : offset;
    for(j = 0; j <= o; j++) {
        while(dctx->inpkt.stream_index != dctx->stream_index) {
            av_free_packet(&dctx->inpkt);
            av_read_frame(dctx->fctx, &dctx->inpkt);
        }

        /*
         * Handle the last frame of the file, which is tramsitted
         * with one frame of latency in libavcodec.
         */
        if ((unsigned int) frame_num == ctx->frames.size() - 1) {
            av_free_packet(&dctx->inpkt);
            avcodec_decode_video2(dctx->avctx, out, &av_ret, &dctx->inpkt);
            break;
        }

        /*
         * Loop until we have a whole frame, since there can be
         * multi-packet frames.
         */
        av_ret = 0;
        while(!av_ret) {
            AVPacket orig = dctx->inpkt;

            /*
             * Decoding might not consume out whole packet, so
             * stash the original packet info, loop until it
             * is all consumed, and then restore it, it so
             * we can free it properly.
             */
            while(dctx->inpkt.size > 0) {
                int r = avcodec_decode_video2(dctx->avctx, out, &av_ret, &dctx->inpkt);

                dctx->inpkt.size -= r;
                dctx->inpkt.data += r;
            }

            dctx->inpkt = orig;

            do {
                av_free_packet(&dctx->inpkt);
                av_read_frame(dctx->fctx, &dctx->inpkt);
            } while(dctx->inpkt.stream_index != dctx->stream_index);
        }

        /* Unreference all but the last frame. */
        if (j != o)
#if defined(USE_OLD_FFAPI)
            avcodec_get_frame_defaults(out);
#else
            av_frame_unref(out);
#endif
    }

    /*
     * Stash the frame number we just decoded, and the GOP it
     * is a part of so we can check if we're decoding linearly
     * later on.
     */
    dctx->last_gop   = f.gop;
    dctx->last_frame = frame_num;

    return 0;

dfail:
    avformat_close_input(&dctx->fctx);
    return -1;
}
