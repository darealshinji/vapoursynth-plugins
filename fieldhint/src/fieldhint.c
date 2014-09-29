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
   VSVideoInfo vi;

   const char *ovrfile;
   ovr_t *ovr;
} FieldhintData;


static void VS_CC fieldhintInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   FieldhintData *d = (FieldhintData *) * instanceData;
   vsapi->setVideoInfo(&d->vi, 1, node);

   int line = 0;
   char buf[80];
   char* pos;
   FILE* fh = fopen(d->ovrfile, "r");
   if (!fh) {
      vsapi->freeNode(d->node);
      free(d);
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

   d->ovr = malloc(line * sizeof(ovr_t));

   line = 0;
   memset(buf, 0, sizeof(buf));
   while (fgets(buf, 80, fh)) {
      char hint = 0;
      ovr_t *entry = &d->ovr[line];
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
         free(d->ovr);
         vsapi->freeNode(d->node);
         free(d);
         char err[80];
         sprintf(err, "Fieldhint: Can't parse override at line %d", line);
         vsapi->setError(out, err);
         return;
      }

      entry->hint = HINT_MISSING;
      if (hint == '-') {
         entry->hint = HINT_NOTCOMBED;
      } else if (hint == '+') {
         entry->hint = HINT_COMBED;
      } else if (hint != 0) {
         fclose(fh);
         free(d->ovr);
         vsapi->freeNode(d->node);
         free(d);
         char err[80];
         sprintf(err, "Fieldhint: Invalid combed hint at line %d", line);
         vsapi->setError(out, err);
         return;
      }

      while (buf[78] != 0 && buf[78] != '\n' && fgets(buf, 80, fh)) {
         ; // slurp the rest of a long line
      }
   }

   d->vi.numFrames = line;
   fclose(fh);
}


static const VSFrameRef *VS_CC fieldhintGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   FieldhintData *d = (FieldhintData *) * instanceData;

   int tf = d->ovr[n].tf;
   int bf = d->ovr[n].bf;

   if (activationReason == arInitial) {
      if (tf == bf) {
         vsapi->requestFrameFilter(tf, d->node, frameCtx);
      } else {
         if (tf < bf) {
            vsapi->requestFrameFilter(tf, d->node, frameCtx);
         }

         vsapi->requestFrameFilter(bf, d->node, frameCtx);

         if (tf > bf) {
            vsapi->requestFrameFilter(tf, d->node, frameCtx);
         }
      }
   } else if (activationReason == arAllFramesReady) {
      VSFrameRef *frame;
      if (tf == bf) {
         frame = (VSFrameRef *)vsapi->getFrameFilter(tf, d->node, frameCtx);
      } else {
         frame = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, NULL, core);
         const VSFrameRef *top = vsapi->getFrameFilter(tf, d->node, frameCtx);
         const VSFrameRef *bottom = vsapi->getFrameFilter(bf, d->node, frameCtx);

         int plane;

         for (plane = 0; plane < d->vi.format->numPlanes; plane++) {
            uint8_t *dstp = vsapi->getWritePtr(frame, plane);
            int dst_stride = vsapi->getStride(frame, plane);
            int width = vsapi->getFrameWidth(frame, plane);
            int height = vsapi->getFrameHeight(frame, plane);

            const uint8_t *srcp = vsapi->getReadPtr(top, plane);
            int src_stride = vsapi->getStride(top, plane);
            vs_bitblt(dstp, dst_stride*2,
                      srcp, src_stride*2,
                      width*d->vi.format->bytesPerSample, (height+1)/2);

            srcp = vsapi->getReadPtr(bottom, plane);
            src_stride = vsapi->getStride(bottom, plane);
            vs_bitblt(dstp + dst_stride, dst_stride*2,
                      srcp + src_stride, src_stride*2,
                      width*d->vi.format->bytesPerSample, height/2);
         }

         vsapi->freeFrame(top);
         vsapi->freeFrame(bottom);
      }
         
      if (d->ovr[n].hint != HINT_MISSING) {
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
   free(d->ovr);
   free(d);
}


static void VS_CC fieldhintCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   FieldhintData d;
   FieldhintData *data;

   d.node = vsapi->propGetNode(in, "clip", 0, NULL);
   d.vi = *vsapi->getVideoInfo(d.node);

   if (!d.vi.format) {
      vsapi->setError(out, "Fieldhint: only constant format input supported");
      vsapi->freeNode(d.node);
      return;
   }

   d.ovrfile = vsapi->propGetData(in, "ovr", 0, NULL);

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "Fieldhint", fieldhintInit, fieldhintGetFrame, fieldhintFree, fmParallel, 0, data, core);
   return;
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.fieldhint", "fh", "VapourSynth Fieldhint Plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("Fieldhint", "clip:clip;ovr:data;", fieldhintCreate, 0, plugin);
}
