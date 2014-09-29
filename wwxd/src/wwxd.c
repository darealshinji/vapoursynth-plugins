#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <VapourSynth.h>
#include <VSHelper.h>

#include "detection.h"


typedef struct {
   VSNodeRef *node;
   const VSVideoInfo *vi;

   MBParam Param;
   int intraCount;
   int fcode;

   uint8_t *padded_prevp;
   uint8_t *padded_curp;

   MACROBLOCK *pMBs;
} WwxdData;


static void VS_CC wwxdInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   WwxdData *d = (WwxdData *) * instanceData;
   vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC wwxdGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   WwxdData *d = (WwxdData *) * instanceData;

   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(VSMAX(0, n - 1), d->node, frameCtx);
      vsapi->requestFrameFilter(n, d->node, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      int scenechange, i;

      const VSFrameRef *prev = vsapi->getFrameFilter(VSMAX(0, n - 1), d->node, frameCtx);
      const VSFrameRef *cur = vsapi->getFrameFilter(n, d->node, frameCtx);

      int stride = vsapi->getStride(cur, 0);

      uint8_t *padded_prevp = d->padded_prevp + d->Param.edge_size * d->Param.edged_width + d->Param.edge_size;
      uint8_t *padded_curp = d->padded_curp + d->Param.edge_size * d->Param.edged_width + d->Param.edge_size;

      vs_bitblt(padded_prevp, d->Param.edged_width, vsapi->getReadPtr(prev, 0), stride, d->Param.width, d->Param.height);
      vs_bitblt(padded_curp, d->Param.edged_width, vsapi->getReadPtr(cur, 0), stride, d->Param.width, d->Param.height);

      if (d->vi->width % 16) {
         int pad_width = 16 - (d->vi->width % 16);
         for (i = 0; i < d->vi->height; i++) {
            memset(padded_prevp + i*d->Param.edged_width + d->vi->width, *(padded_prevp + i*d->Param.edged_width + d->vi->width - 1), pad_width);
            memset(padded_curp + i*d->Param.edged_width + d->vi->width, *(padded_curp + i*d->Param.edged_width + d->vi->width - 1), pad_width);
         }
      }

      if (d->vi->height % 16) {
         int pad_height = 16 - (d->vi->height % 16);
         for (i = 0; i < pad_height; i++) {
            memcpy(padded_prevp + (d->vi->height + i)*d->Param.edged_width, padded_prevp + (d->vi->height - 1)*d->Param.edged_width, d->Param.edged_width);
            memcpy(padded_curp + (d->vi->height + i)*d->Param.edged_width, padded_curp + (d->vi->height - 1)*d->Param.edged_width, d->Param.edged_width);
         }
      }

      if (n == 0) {
         scenechange = 1;
      } else {
         scenechange = MEanalysis(padded_prevp, padded_curp, &d->Param, d->pMBs, d->intraCount, d->fcode);
      }

      if (scenechange) {
         d->intraCount = 1;
      } else {
         d->intraCount++;
      }

      VSFrameRef *dst = vsapi->copyFrame(cur, core);
      vsapi->freeFrame(prev);
      vsapi->freeFrame(cur);

      VSMap *props = vsapi->getFramePropsRW(dst);
      vsapi->propSetInt(props, "Scenechange", scenechange, paReplace);

      return dst;
   }

   return 0;
}


static void VS_CC wwxdFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   WwxdData *d = (WwxdData *)instanceData;

   vsapi->freeNode(d->node);
   free(d->padded_prevp);
   free(d->padded_curp);
   free(d->pMBs);
   free(d);
}


static void VS_CC wwxdCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   WwxdData d;
   WwxdData *data;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   d.Param.edge_size = 64;
   d.Param.width = d.vi->width;
   d.Param.height = d.vi->height;
   d.Param.mb_width = (d.vi->width + 15) / 16;
   d.Param.mb_height = (d.vi->height + 15) / 16;
   d.Param.edged_width = 16 * d.Param.mb_width + 2 * d.Param.edge_size;
   d.Param.edged_height = 16 * d.Param.mb_height + 2 * d.Param.edge_size;

   d.intraCount = 1;

   d.fcode = 4;

   d.padded_prevp = (uint8_t *)malloc(d.Param.edged_width * (d.Param.edged_height + 1) + 64);
   d.padded_curp = (uint8_t *)malloc(d.Param.edged_width * (d.Param.edged_height + 1) + 64);
   memset(d.padded_prevp, 0, d.Param.edged_width * (d.Param.edged_height + 1) + 64);
   memset(d.padded_curp, 0, d.Param.edged_width * (d.Param.edged_height + 1) + 64);

   d.pMBs = (MACROBLOCK *)malloc(d.Param.mb_width * d.Param.mb_height * sizeof(MACROBLOCK));
   memset(d.pMBs, 0, d.Param.mb_width * d.Param.mb_height * sizeof(MACROBLOCK));

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "Wwxd", wwxdInit, wwxdGetFrame, wwxdFree, fmSerial, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.wwxd", "wwxd", "Scene change detection approximately like Xvid's", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("WWXD",
                "clip:clip;",
                wwxdCreate, 0, plugin);
}
