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
    int sample_size;
    int sample_type;
    int format;
    int subtype;
    double quality;
    int last_frame;
    int initialised;

    int original_channels;
    int original_samplerate;
} DambWriteData;


static void VS_CC dambWriteInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DambWriteData *d = (DambWriteData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC dambWriteGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DambWriteData *d = (DambWriteData *) * instanceData;

    if (activationReason == arInitial) {
        // Do it like this because the frame requests sometimes arrive out of
        // order and the audio samples get written in the wrong order.
        int distance = n - d->last_frame;
        for (int frame = d->last_frame + 1; frame <= n && distance < 50; frame++)
            vsapi->requestFrameFilter(frame, d->node, frameCtx);
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        int distance = n - d->last_frame;
        for (int frame = d->last_frame + 1; frame <= n && distance < 50; frame++) {
            const VSFrameRef *src = vsapi->getFrameFilter(frame, d->node, frameCtx);
            const VSMap *props = vsapi->getFramePropsRO(src);
            int err;

            int input_channels = vsapi->propGetInt(props, damb_channels, 0, &err);
            int input_samplerate = vsapi->propGetInt(props, damb_samplerate, 0, &err);
            int input_format = vsapi->propGetInt(props, damb_format, 0, &err);
            // Either they are all there, or they are all missing. Probably.
            if (err) {
                vsapi->setFilterError(std::string("Write: Audio data not found in frame ").append(std::to_string(frame)).append(".").c_str(), frameCtx);
                vsapi->freeFrame(src);
                return NULL;
            }

            if (!d->initialised) {
                d->initialised = 1;

                d->original_channels = input_channels;
                d->original_samplerate = input_samplerate;

                // If the input was WAVEX, make the output WAVEX too.
                if ((input_format & SF_FORMAT_TYPEMASK) == SF_FORMAT_WAVEX &&
                    d->format == SF_FORMAT_WAV)
                    d->format = SF_FORMAT_WAVEX;

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

            if (d->original_channels != input_channels ||
                d->original_samplerate != input_samplerate ||
                d->sample_type != getSampleType(input_format)) {
                vsapi->setFilterError(std::string("Write: Clip contains more than one type of audio data. Mismatch found at frame ").append(std::to_string(frame)).append(".").c_str(), frameCtx);
                vsapi->freeFrame(src);
                return NULL;
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
    DambWriteData *d = (DambWriteData *)instanceData;

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
    if (f == "wavex")
        return SF_FORMAT_WAVEX;
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
    DambWriteData d;
    DambWriteData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, NULL);
    d.vi = vsapi->getVideoInfo(d.node);

    d.format = 0;
    d.subtype = 0;

    d.filename = vsapi->propGetData(in, "file", 0, NULL);
    size_t last_dot = d.filename.find_last_of('.');
    if (last_dot != std::string::npos)
        d.format = getMajorFormatFromString(d.filename.substr(last_dot + 1).c_str());

    const char *format = vsapi->propGetData(in, "format", 0, &err);
    if (!err)
        d.format = getMajorFormatFromString(format);

    if (d.format == SF_FORMAT_OGG)
        d.subtype = SF_FORMAT_VORBIS;
    else {
        const char *subtype = vsapi->propGetData(in, "sample_type", 0, &err);
        if (!err)
            d.subtype = getSubtypeFromString(subtype);
    }

    d.quality = vsapi->propGetFloat(in, "quality", 0, &err);
    if (err)
        d.quality = 0.7;


    d.initialised = 0;
    d.sndfile = NULL;

    // The rest of the initialisation happens the first time a frame
    // is requested.

    d.last_frame = -1;


    data = new DambWriteData();
    *data = d;

    vsapi->createFilter(in, out, "Write", dambWriteInit, dambWriteGetFrame, dambWriteFree, fmSerial, 0, data, core);
}


void writeRegister(VSRegisterFunction registerFunc, VSPlugin *plugin) {
    registerFunc("Write",
            "clip:clip;"
            "file:data;"
            "format:data:opt;"
            "sample_type:data:opt;"
            "quality:float:opt;"
            , dambWriteCreate, 0, plugin);
}
