#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <string>

#include <VapourSynth.h>
#include <VSHelper.h>

#include <cstdio>
#include <sndfile.h>


typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    std::string filename;

    SNDFILE *sndfile;
    SF_INFO sfinfo;
    void *buffer;
    int sample_size;
    int sample_type;
    sf_count_t samples_per_frame;
    int format;
    int subtype;
    double quality;
    int last_frame;
} DambData;


static void VS_CC dambInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DambData *d = (DambData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}


const char *damb_samples = "Damb samples";
const char *damb_channels = "Damb channels";
const char *damb_samplerate = "Damb sample rate";
const char *damb_format = "Damb format";


static const VSFrameRef *VS_CC dambReadGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DambData *d = (DambData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef *dst = vsapi->copyFrame(src, core);
        vsapi->freeFrame(src);

        // sf_count_t is int64_t
        sf_count_t sample_start = d->samples_per_frame * n;

        sf_count_t seek_ret = sf_seek(d->sndfile, sample_start, SEEK_SET);
        if (seek_ret != sample_start) {
            vsapi->setFilterError(std::string("Read: sf_seek failed at frame ").append(std::to_string(n)).append(".").c_str(), frameCtx);
            vsapi->freeFrame(dst);
            return NULL;
        }

        sf_count_t readf_ret;
        if (d->sample_type == SF_FORMAT_PCM_16)
            readf_ret = sf_readf_short(d->sndfile, (short *)d->buffer, d->samples_per_frame);
        else if (d->sample_type == SF_FORMAT_PCM_32)
            readf_ret = sf_readf_int(d->sndfile, (int *)d->buffer, d->samples_per_frame);
        else if (d->sample_type == SF_FORMAT_FLOAT)
            readf_ret = sf_readf_float(d->sndfile, (float *)d->buffer, d->samples_per_frame);
        else
            readf_ret = sf_readf_double(d->sndfile, (double *)d->buffer, d->samples_per_frame);

        if (readf_ret == 0) {
            vsapi->setFilterError(std::string("Read: sf_readf_blah returned 0 at frame ").append(std::to_string(n)).append(".").c_str(), frameCtx);
            vsapi->freeFrame(dst);
            return NULL;
        }

        int64_t readf_ret_bytes = readf_ret * d->sfinfo.channels * d->sample_size;

        VSMap *props = vsapi->getFramePropsRW(dst);
        vsapi->propSetData(props, damb_samples, (char *)d->buffer, readf_ret_bytes, paReplace);
        vsapi->propSetInt(props, damb_channels, d->sfinfo.channels, paReplace);
        vsapi->propSetInt(props, damb_samplerate, d->sfinfo.samplerate, paReplace);
        vsapi->propSetInt(props, damb_format, d->sfinfo.format, paReplace);


        return dst;
    }

    return NULL;
}


static void VS_CC dambReadFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DambData *d = (DambData *)instanceData;

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
        SF_FORMAT_FLAC,
        SF_FORMAT_OGG
    };

    for (size_t i = 0; i < sizeof(formats); i++)
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
        SF_FORMAT_VORBIS
    };

    for (size_t i = 0; i < sizeof(formats); i++)
        if (subtype == formats[i])
            return 1;

    return 0;
}


static inline int getSampleType(int format) {
    if (format & SF_FORMAT_PCM_S8 || format & SF_FORMAT_PCM_U8 || format & SF_FORMAT_PCM_16)
        return SF_FORMAT_PCM_16;

    if (format & SF_FORMAT_PCM_24 || format & SF_FORMAT_PCM_32)
        return SF_FORMAT_PCM_32;

    if (format & SF_FORMAT_FLOAT || format & SF_FORMAT_VORBIS)
        return SF_FORMAT_FLOAT;

    return SF_FORMAT_DOUBLE;
}


static inline int getSampleSize(int sample_type) {
    if (sample_type == SF_FORMAT_PCM_16)
        return 2;

    if (sample_type == SF_FORMAT_PCM_32 || sample_type == SF_FORMAT_FLOAT)
        return 4;

    // SF_FORMAT_DOUBLE
    return 8;
}


static void VS_CC dambReadCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    DambData d;
    DambData *data;

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

    d.samples_per_frame = (sf_count_t)((d.sfinfo.samplerate * d.vi->fpsDen) / (double)d.vi->fpsNum + 0.5);

    d.sample_type = getSampleType(d.sfinfo.format);
    d.sample_size = getSampleSize(d.sample_type);

    d.buffer = malloc(d.samples_per_frame * d.sfinfo.channels * d.sample_size);


    data = new DambData();
    *data = d;

    vsapi->createFilter(in, out, "Read", dambInit, dambReadGetFrame, dambReadFree, fmSerial, 0, data, core);
}


static const VSFrameRef *VS_CC dambWriteGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DambData *d = (DambData *) * instanceData;

    if (activationReason == arInitial) {
        // Do it like this because the frame requests sometimes arrive out of
        // order and the audio samples get written in the wrong order.
        for (int frame = d->last_frame + 1; frame <= n && n - d->last_frame < 50; frame++)
            vsapi->requestFrameFilter(frame, d->node, frameCtx);
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        for (int frame = d->last_frame + 1; frame <= n && n - d->last_frame < 50; frame++) {
            const VSFrameRef *src = vsapi->getFrameFilter(frame, d->node, frameCtx);
            const VSMap *props = vsapi->getFramePropsRO(src);
            int err;

            if (d->samples_per_frame == -1) { // Not initialised yet.
                d->samples_per_frame = 0;

                int input_channels = vsapi->propGetInt(props, damb_channels, 0, &err);
                int input_samplerate = vsapi->propGetInt(props, damb_samplerate, 0, &err);
                int input_format = vsapi->propGetInt(props, damb_format, 0, &err);
                // Either they are all there, or they are all missing. Probably.
                if (err) {
                    vsapi->setFilterError(std::string("Write: Audio data not found in frame ").append(std::to_string(frame)).append(".").c_str(), frameCtx);
                    vsapi->freeFrame(src);
                    return NULL;
                }

                int new_format = 0;
                new_format |= d->format ? d->format : (input_format & SF_FORMAT_TYPEMASK);
                new_format |= d->subtype ? d->subtype : (input_format & SF_FORMAT_SUBMASK);
                new_format |= input_format & SF_FORMAT_ENDMASK;

                d->sfinfo.channels = input_channels;
                d->sfinfo.samplerate = input_samplerate;
                d->sfinfo.format = new_format;

                if (!sf_format_check(&d->sfinfo)) {
                    vsapi->setFilterError("Write: libsndfile doesn't support this combination of channels, sample rate, sample type, and format for writing.", frameCtx);
                    vsapi->freeFrame(src);
                    return NULL;
                }

                d->sndfile = sf_open(d->filename.c_str(), SFM_WRITE, &d->sfinfo);
                if (d->sndfile == NULL) {
                    vsapi->setFilterError(std::string("Write: Couldn't open audio file for writing. Error message from libsndfile: ").append(sf_strerror(NULL)).c_str(), frameCtx);
                    vsapi->freeFrame(src);
                    return NULL;
                }

                if ((d->sfinfo.format & SF_FORMAT_VORBIS) == SF_FORMAT_VORBIS) {
                    int cmd_ret = sf_command(d->sndfile, SFC_SET_VBR_ENCODING_QUALITY, &d->quality, sizeof(d->quality));
                    if (!cmd_ret) {
                        vsapi->setFilterError("Write: Failed to set the encoding quality.", frameCtx);
                        vsapi->freeFrame(src);
                        return NULL;
                    }
                }

                // These are used to pick the sf_writef_* function to use and
                // to calculate the number of audio frames stored in the props,
                // so they need to be based on the input format.
                d->sample_type = getSampleType(input_format);
                d->sample_size = getSampleSize(d->sample_type);
            }

            const char *buffer = vsapi->propGetData(props, damb_samples, 0, &err);
            sf_count_t buffer_size = vsapi->propGetDataSize(props, damb_samples, 0, &err);
            buffer_size = buffer_size / (d->sfinfo.channels * d->sample_size);
            if (err) {
                vsapi->setFilterError(std::string("Write: Audio data not found in frame ").append(std::to_string(frame)).append(".").c_str(), frameCtx);
                vsapi->freeFrame(src);
                return NULL;
            }

            sf_count_t writef_ret;
            if (d->sample_type == SF_FORMAT_PCM_16)
                writef_ret = sf_writef_short(d->sndfile, (short *)buffer, buffer_size);
            else if (d->sample_type == SF_FORMAT_PCM_32)
                writef_ret = sf_writef_int(d->sndfile, (int *)buffer, buffer_size);
            else if (d->sample_type == SF_FORMAT_FLOAT)
                writef_ret = sf_writef_float(d->sndfile, (float *)buffer, buffer_size);
            else
                writef_ret = sf_writef_double(d->sndfile, (double *)buffer, buffer_size);

            if (writef_ret != buffer_size) {
                vsapi->setFilterError(std::string("Write: sf_writef_blah didn't write the expected number of samples at frame ").append(std::to_string(frame)).append(".").c_str(), frameCtx);
                vsapi->freeFrame(src);
                return NULL;
            }

            vsapi->freeFrame(src);
            d->last_frame = n;
        }


        return vsapi->getFrameFilter(n, d->node, frameCtx);
    }

    return 0;
}


static void VS_CC dambWriteFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DambData *d = (DambData *)instanceData;

    if (d->sndfile)
        sf_close(d->sndfile);
    vsapi->freeNode(d->node);
    delete d;
}


static inline int getMajorFormatFromString(const char *format) {
    if (!format)
        return 0;

    std::string f(format);

    if (f == "wav")
        return SF_FORMAT_WAV;
    if (f == "w64")
        return SF_FORMAT_W64;
    if (f == "flac")
        return SF_FORMAT_FLAC;
    if (f == "ogg")
        return SF_FORMAT_OGG;
    return 0;
}


static inline int getSubtypeFromString(const char *subtype) {
    if (!subtype)
        return 0;

    std::string s(subtype);

    if (s == "u8")
        return SF_FORMAT_PCM_U8;
    if (s == "s8")
        return SF_FORMAT_PCM_S8;
    if (s == "s16")
        return SF_FORMAT_PCM_16;
    if (s == "s24")
        return SF_FORMAT_PCM_24;
    if (s == "s32")
        return SF_FORMAT_PCM_32;
    if (s == "float")
        return SF_FORMAT_FLOAT;
    if (s == "double")
        return SF_FORMAT_DOUBLE;
    return 0;
}


static void VS_CC dambWriteCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    DambData d;
    DambData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, NULL);
    d.vi = vsapi->getVideoInfo(d.node);

    d.filename = vsapi->propGetData(in, "file", 0, NULL);

    const char *format = vsapi->propGetData(in, "format", 0, &err);
    d.format = getMajorFormatFromString(format);

    if (d.format == SF_FORMAT_OGG)
        d.subtype = SF_FORMAT_VORBIS;
    else {
        const char *subtype = vsapi->propGetData(in, "sample_type", 0, &err);
        d.subtype = getSubtypeFromString(subtype);
    }

    d.quality = vsapi->propGetFloat(in, "quality", 0, &err);
    if (err)
        d.quality = 0.7;


    d.samples_per_frame = -1; // Use it as a flag.
    d.sndfile = NULL;

    // The rest of the initialisation happens the first time a frame
    // is requested.

    d.last_frame = -1;


    data = new DambData();
    *data = d;

    vsapi->createFilter(in, out, "Write", dambInit, dambWriteGetFrame, dambWriteFree, fmSerial, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.damb", "damb", "Audio file reader and writer", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Read",
            "clip:clip;"
            "file:data;"
            , dambReadCreate, 0, plugin);
    registerFunc("Write",
            "clip:clip;"
            "file:data;"
            "format:data:opt;"
            "sample_type:data:opt;"
            "quality:float:opt;"
            , dambWriteCreate, 0, plugin);
}
