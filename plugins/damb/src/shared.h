
static const char *damb_samples = "DambSamples";
static const char *damb_channels = "DambChannels";
static const char *damb_samplerate = "DambSampleRate";
static const char *damb_format = "DambFormat";



static inline int getSampleType(int format) {
    int subtype = format & SF_FORMAT_SUBMASK;

    if (subtype == SF_FORMAT_PCM_S8 ||
        subtype == SF_FORMAT_PCM_U8 ||
        subtype == SF_FORMAT_PCM_16)
        return SF_FORMAT_PCM_16;

    if (subtype == SF_FORMAT_PCM_24 ||
        subtype == SF_FORMAT_PCM_32)
        return SF_FORMAT_PCM_32;

    if (subtype == SF_FORMAT_FLOAT ||
        subtype == SF_FORMAT_VORBIS)
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


