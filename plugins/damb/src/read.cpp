#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <string>

#include <VapourSynth.h>
#include <VSHelper.h>

#include <cstdio>
#include <sndfile.h>

#include "shared.h"


typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    std::string filename;

    SNDFILE *sndfile;
    SF_INFO sfinfo;
    uint8_t *buffer;
    int sample_size;
    int sample_type;
    double samples_per_frame;
    double delay_seconds;
    sf_count_t delay_samples;
} DambReadData;


static void VS_CC dambReadInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DambReadData *d = (DambReadData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}


static void read_samples(SNDFILE *sndfile, SF_INFO *sfinfo, sf_count_t sample_start, sf_count_t sample_count, int sample_type, int sample_size, uint8_t *buffer) {
    sf_count_t seek_ret = sf_seek(sndfile, sample_start, SEEK_SET);

    sf_count_t readf_ret = 0;
    if (seek_ret == sample_start) {
        if (sample_type == SF_FORMAT_PCM_16)
            readf_ret = sf_readf_short(sndfile, (short *)buffer, sample_count);
        else if (sample_type == SF_FORMAT_PCM_32)
            readf_ret = sf_readf_int(sndfile, (int *)buffer, sample_count);
        else if (sample_type == SF_FORMAT_FLOAT)
            readf_ret = sf_readf_float(sndfile, (float *)buffer, sample_count);
        else
            readf_ret = sf_readf_double(sndfile, (double *)buffer, sample_count);
    }

    if (readf_ret < sample_count) {
        int64_t silence_start_bytes = readf_ret * sfinfo->channels * sample_size;
        int64_t silence_count_bytes = (sample_count - readf_ret) * sfinfo->channels * sample_size;
        memset(buffer + silence_start_bytes, 0, silence_count_bytes);
    }
}


static const VSFrameRef *VS_CC dambReadGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DambReadData *d = (DambReadData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef *dst = vsapi->copyFrame(src, core);
        vsapi->freeFrame(src);

        // sf_count_t is int64_t
        sf_count_t sample_start = (sf_count_t)(d->samples_per_frame * n + 0.5);
        sf_count_t sample_end = (sf_count_t)(d->samples_per_frame * (n + 1) + 0.5);
        sf_count_t sample_count = sample_end - sample_start;

        sf_count_t delayed_start = sample_start - d->delay_samples;
        sf_count_t delayed_end = sample_end - d->delay_samples;

        int64_t sample_count_bytes = sample_count * d->sfinfo.channels * d->sample_size;

        if (delayed_start < 0) {
            if (delayed_end > 0) {
                sf_count_t leading_silence = sample_count - delayed_end;
                int64_t leading_silence_bytes = leading_silence * d->sfinfo.channels * d->sample_size;
                memset(d->buffer, 0, leading_silence_bytes);

                read_samples(d->sndfile, &d->sfinfo, 0, delayed_end, d->sample_type, d->sample_size, d->buffer + leading_silence_bytes);
            } else {
                memset(d->buffer, 0, sample_count_bytes);
            }
        } else {
            read_samples(d->sndfile, &d->sfinfo, delayed_start, sample_count, d->sample_type, d->sample_size, d->buffer);
        }

        VSMap *props = vsapi->getFramePropsRW(dst);
        vsapi->propSetData(props, damb_samples, (char *)d->buffer, sample_count_bytes, paReplace);
        vsapi->propSetInt(props, damb_channels, d->sfinfo.channels, paReplace);
        vsapi->propSetInt(props, damb_samplerate, d->sfinfo.samplerate, paReplace);
        vsapi->propSetInt(props, damb_format, d->sfinfo.format, paReplace);

        return dst;
    }

    return NULL;
}


static void VS_CC dambReadFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DambReadData *d = (DambReadData *)instanceData;

    sf_close(d->sndfile);
    free(d->buffer);
    vsapi->freeNode(d->node);
    delete d;
}


static inline int isAcceptableFormatType(int format) {
    int type = format & SF_FORMAT_TYPEMASK;

    int formats[] = {
        SF_FORMAT_WAV,
        SF_FORMAT_W64,
        SF_FORMAT_WAVEX,
        SF_FORMAT_FLAC,
        SF_FORMAT_OGG,
        0
    };

    for (int i = 0; formats[i]; i++)
        if (type == formats[i])
            return 1;

    return 0;
}


static inline int isAcceptableFormatSubtype(int format) {
    int subtype = format & SF_FORMAT_SUBMASK;

    int formats[] = {
        SF_FORMAT_PCM_S8,
        SF_FORMAT_PCM_16,
        SF_FORMAT_PCM_24,
        SF_FORMAT_PCM_32,
        SF_FORMAT_PCM_U8,
        SF_FORMAT_FLOAT,
        SF_FORMAT_DOUBLE,
        SF_FORMAT_VORBIS,
        0
    };

    for (int i = 0; formats[i]; i++)
        if (subtype == formats[i])
            return 1;

    return 0;
}


static void VS_CC dambReadCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    DambReadData d;
    DambReadData *data;
    int err;

    d.delay_seconds = vsapi->propGetFloat(in, "delay", 0, &err);

    d.node = vsapi->propGetNode(in, "clip", 0, NULL);
    d.vi = vsapi->getVideoInfo(d.node);

    d.filename = vsapi->propGetData(in, "file", 0, NULL);


    if (!d.vi->numFrames) {
        vsapi->setError(out, "Read: Can't accept clips with unknown length.");
        vsapi->freeNode(d.node);
        return;
    }

    if (!d.vi->fpsNum || !d.vi->fpsDen) {
        vsapi->setError(out, "Read: Can't accept clips with variable frame rate.");
        vsapi->freeNode(d.node);
        return;
    }


    d.sfinfo.format = 0;
    d.sndfile = sf_open(d.filename.c_str(), SFM_READ, &d.sfinfo);
    if (d.sndfile == NULL) {
        vsapi->setError(out, std::string("Read: Couldn't open audio file. Error message from libsndfile: ").append(sf_strerror(NULL)).c_str());
        vsapi->freeNode(d.node);
        return;
    }

    if (!isAcceptableFormatType(d.sfinfo.format)) {
        vsapi->setError(out, "Read: Audio file's type is not supported.");
        sf_close(d.sndfile);
        vsapi->freeNode(d.node);
        return;
    }

    if (!isAcceptableFormatSubtype(d.sfinfo.format)) {
        vsapi->setError(out, "Read: Audio file's subtype is not supported.");
        sf_close(d.sndfile);
        vsapi->freeNode(d.node);
        return;
    }

    d.samples_per_frame = (d.sfinfo.samplerate * d.vi->fpsDen) / (double)d.vi->fpsNum;

    d.sample_type = getSampleType(d.sfinfo.format);
    d.sample_size = getSampleSize(d.sample_type);

    // sample_count will be sometimes (int)samples_per_frame,
    // sometimes (int)(samples_per_frame + 1), depending on the frame number
    d.buffer = (uint8_t *)malloc((int)(d.samples_per_frame + 1) * d.sfinfo.channels * d.sample_size);

    d.delay_samples = (sf_count_t)(d.delay_seconds * d.sfinfo.samplerate);


    data = new DambReadData();
    *data = d;

    vsapi->createFilter(in, out, "Read", dambReadInit, dambReadGetFrame, dambReadFree, fmSerial, 0, data, core);
}


void readRegister(VSRegisterFunction registerFunc, VSPlugin *plugin) {
    registerFunc("Read",
            "clip:clip;"
            "file:data;"
            "delay:float:opt;"
            , dambReadCreate, 0, plugin);
}
