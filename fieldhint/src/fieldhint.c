#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <VapourSynth.h>
#include <VSHelper.h>


typedef enum {
    HINT_MISSING = -1,
    HINT_NOTCOMBED,
    HINT_COMBED
} hint_t;


typedef struct {
    int tf, bf;
    hint_t hint;
} ovr_t;


typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    const char *ovrfile;
    ovr_t *ovr;
    int tff;
    char *matches;
    int num_matches;
} FieldhintData;


static void VS_CC fieldhintInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    FieldhintData *d = (FieldhintData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC fieldhintGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    if (activationReason == arFrameReady)
        return NULL;

    FieldhintData *d = (FieldhintData *) * instanceData;

    int tf, bf;

    if (d->ovrfile) {
        tf = d->ovr[n].tf;
        bf = d->ovr[n].bf;
    } else {
        char match = d->matches[n];
        int tff = d->tff;
        if (match == 'n') {
            tf = n + tff;
            bf = n + !tff;
        } else if (match == 'u') {
            tf = n + !tff;
            bf = n + tff;
        } else if (match == 'p') {
            tf = n - !tff;
            bf = n - tff;
        } else if (match == 'b') {
            tf = n - tff;
            bf = n - !tff;
        } else { // 'c' and any invalid characters.
            tf = bf = n;
        }
    }

    if (activationReason == arInitial) {
        if (tf < bf) {
            vsapi->requestFrameFilter(tf, d->node, frameCtx);
        }

        vsapi->requestFrameFilter(bf, d->node, frameCtx);

        if (tf > bf) {
            vsapi->requestFrameFilter(tf, d->node, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        VSFrameRef *frame;
        if (tf == bf) {
            const VSFrameRef *tmp = vsapi->getFrameFilter(tf, d->node, frameCtx);
            frame = vsapi->copyFrame(tmp, core);
            vsapi->freeFrame(tmp);
        } else {
            frame = vsapi->newVideoFrame(d->vi->format, d->vi->width, d->vi->height, NULL, core);
            const VSFrameRef *top = vsapi->getFrameFilter(tf, d->node, frameCtx);
            const VSFrameRef *bottom = vsapi->getFrameFilter(bf, d->node, frameCtx);

            int plane;

            for (plane = 0; plane < d->vi->format->numPlanes; plane++) {
                uint8_t *dstp = vsapi->getWritePtr(frame, plane);
                int dst_stride = vsapi->getStride(frame, plane);
                int width = vsapi->getFrameWidth(frame, plane);
                int height = vsapi->getFrameHeight(frame, plane);

                const uint8_t *srcp = vsapi->getReadPtr(top, plane);
                int src_stride = vsapi->getStride(top, plane);
                vs_bitblt(dstp, dst_stride*2,
                          srcp, src_stride*2,
                          width*d->vi->format->bytesPerSample, (height+1)/2);

                srcp = vsapi->getReadPtr(bottom, plane);
                src_stride = vsapi->getStride(bottom, plane);
                vs_bitblt(dstp + dst_stride, dst_stride*2,
                          srcp + src_stride, src_stride*2,
                          width*d->vi->format->bytesPerSample, height/2);
            }

            vsapi->freeFrame(top);
            vsapi->freeFrame(bottom);
        }

        if (d->ovrfile && d->ovr[n].hint != HINT_MISSING) {
            VSMap *props = vsapi->getFramePropsRW(frame);
            vsapi->propSetInt(props, "_Combed", d->ovr[n].hint, paReplace);
        }

        return frame;
    }

    return 0;
}


static void VS_CC fieldhintFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    FieldhintData *d = (FieldhintData *)instanceData;
    vsapi->freeNode(d->node);
    if (d->ovr)
        free(d->ovr);
    if (d->matches)
        free(d->matches);
    free(d);
}


static void VS_CC fieldhintCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    FieldhintData d = { 0 };
    FieldhintData *data;
    int err;

    d.ovrfile = vsapi->propGetData(in, "ovr", 0, &err);

    const char *matches = vsapi->propGetData(in, "matches", 0, &err);

    if (!d.ovrfile && !matches) {
        vsapi->setError(out, "FieldHint: Either 'ovr' or 'matches' must be passed.");
        return;
    }

    if (d.ovrfile && matches) {
        vsapi->setError(out, "FieldHint: Only one of 'ovr' and 'matches' must be passed.");
        return;
    }

    d.tff = !!vsapi->propGetInt(in, "tff", 0, &err);
    if (err && matches) {
        vsapi->setError(out, "FieldHint: 'tff' must be passed when 'matches' is passed.");
        return;
    }

    if (!err && d.ovrfile) {
        vsapi->setError(out, "FieldHint: 'tff' must not be passed when 'ovr' is passed.");
        return;
    }

    d.node = vsapi->propGetNode(in, "clip", 0, NULL);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!d.vi->format) {
        vsapi->setError(out, "FieldHint: only constant format input supported");
        vsapi->freeNode(d.node);
        return;
    }


    if (d.ovrfile) {
        int line = 0;
        char buf[80];
        char* pos;
        FILE* fh = fopen(d.ovrfile, "r");
        if (!fh) {
            vsapi->freeNode(d.node);
            vsapi->setError(out, "FieldHint: can't open ovr file");
            return;
        }

        while (fgets(buf, 80, fh)) {
            if (buf[strspn(buf, " \t\r\n")] == 0) {
                continue;
            }
            line++;
        }
        fseek(fh, 0, 0);

        d.ovr = malloc(line * sizeof(ovr_t));

        line = 0;
        memset(buf, 0, sizeof(buf));
        while (fgets(buf, 80, fh)) {
            char hint = 0;
            ovr_t *entry = &d.ovr[line];
            line++;
            pos = buf + strspn(buf, " \t\r\n");

            if (pos[0] == '#' || pos[0] == 0) {
                continue;
            } else if (sscanf(pos, " %u, %u, %c", &entry->tf, &entry->bf, &hint) == 3) {
                ;
            } else if (sscanf(pos, " %u, %u", &entry->tf, &entry->bf) == 2) {
                ;
            } else {
                fclose(fh);
                free(d.ovr);
                vsapi->freeNode(d.node);
                char error[80];
                sprintf(error, "FieldHint: Can't parse override at line %d", line);
                vsapi->setError(out, error);
                return;
            }

            entry->hint = HINT_MISSING;
            if (hint == '-') {
                entry->hint = HINT_NOTCOMBED;
            } else if (hint == '+') {
                entry->hint = HINT_COMBED;
            } else if (hint != 0) {
                fclose(fh);
                free(d.ovr);
                vsapi->freeNode(d.node);
                char error[80];
                sprintf(error, "FieldHint: Invalid combed hint at line %d", line);
                vsapi->setError(out, error);
                return;
            }

            while (buf[78] != 0 && buf[78] != '\n' && fgets(buf, 80, fh)) {
                ; // slurp the rest of a long line
            }
        }

        fclose(fh);
        if (d.vi->numFrames != line) {
            vsapi->setError(out, "FieldHint: The number of overrides and the number of frames don't match.");
            free(d.ovr);
            vsapi->freeNode(d.node);
            return;
        }
    } else { // No overrides file. Use matches.
        d.num_matches = vsapi->propGetDataSize(in, "matches", 0, &err);
        if (d.num_matches == 0) {
            vsapi->setError(out, "FieldHint: 'matches' must not be an empty string.");
            vsapi->freeNode(d.node);
            return;
        }

        if (d.vi->numFrames != d.num_matches) {
            vsapi->setError(out, "FieldHint: The number of matches and the number of frames don't match.");
            vsapi->freeNode(d.node);
            return;
        }

        if (matches[0] == 'p' || matches[0] == 'b') {
            vsapi->setError(out, "FieldHint: The first match cannot be 'p' or 'b'.");
            vsapi->freeNode(d.node);
            return;
        }

        if (matches[d.num_matches - 1] == 'n' || matches[d.num_matches - 1] == 'u') {
            vsapi->setError(out, "FieldHint: The last match cannot be 'n' or 'u'.");
            vsapi->freeNode(d.node);
            return;
        }

        d.matches = malloc(d.num_matches + 1);
        memcpy(d.matches, matches, d.num_matches + 1);
    }

    data = malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "FieldHint", fieldhintInit, fieldhintGetFrame, fieldhintFree, fmParallel, 0, data, core);
    return;
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.fieldhint", "fh", "FieldHint Plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Fieldhint",
            "clip:clip;"
            "ovr:data:opt;"
            "tff:int:opt;"
            "matches:data:opt;"
            , fieldhintCreate, NULL, plugin);
    registerFunc("FieldHint",
            "clip:clip;"
            "ovr:data:opt;"
            "tff:int:opt;"
            "matches:data:opt;"
            , fieldhintCreate, NULL, plugin);
}
