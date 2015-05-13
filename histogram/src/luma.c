#include <stdlib.h>
#include <string.h>
#include <VapourSynth.h>

#include "common.h"

typedef struct {
   VSNodeRef *node;
   VSVideoInfo vi;
} LumaData;


static void VS_CC lumaInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   LumaData *d = (LumaData *) * instanceData;
   vsapi->setVideoInfo(&d->vi, 1, node);
}


static const VSFrameRef *VS_CC lumaGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   LumaData *d = (LumaData *) * instanceData;

   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(n, d->node, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);

      const VSFormat *fi = d->vi.format;
      int height = d->vi.height;
      int width = d->vi.width;

      // When creating a new frame for output it is VERY EXTREMELY SUPER IMPORTANT to
      // supply the "domainant" source frame to copy properties from. Frame props
      // are an essential part of the filter chain and you should NEVER break it.
      VSFrameRef *dst = vsapi->newVideoFrame(fi, width, height, src, core);
      
      int src_stride = vsapi->getStride(src, 0);
      int dst_stride = vsapi->getStride(dst, 0);

      int src_height = vsapi->getFrameHeight(src, 0);
      int src_width = vsapi->getFrameWidth(src, 0);

      int y;
      int x;
     
      if(fi->bitsPerSample == 8)
      {
         const uint8_t *srcp = vsapi->getReadPtr(src, 0);
         uint8_t *dstp = vsapi->getWritePtr(dst, 0);
       
         for (y = 0; y < src_height; y++) {
            for (x = 0; x < src_width; x++) {
               int p = srcp[src_stride * y + x] << 4;
               dstp[dst_stride * y + x] = (p & 256) ? (255 - (p & 0xff)) : p & 0xff;
            }
         }
      }
      else
      {
         const uint16_t *srcp = (const uint16_t *)vsapi->getReadPtr(src, 0);
         uint16_t *dstp = (uint16_t *)vsapi->getWritePtr(dst, 0);
         src_stride = src_stride / 2;
         dst_stride = dst_stride / 2;
       
         for (y = 0; y < src_height; y++) {
            for (x = 0; x < src_width; x++) {
               int p = srcp[src_stride * y + x] << 4;
               dstp[dst_stride * y + x] = (p & (0xffff+1)) ? (0xffff - (p & 0xffff)) : p & 0xffff;
            }
         }
      }
     
      // Release the source frame
      vsapi->freeFrame(src);

      // A reference is consumed when it is returned so saving the dst ref somewhere
      // and reusing it is not allowed.
      return dst;
   }

   return 0;
}


static void VS_CC lumaFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   LumaData *d = (LumaData *)instanceData;
   vsapi->freeNode(d->node);
   free(d);
}


void VS_CC lumaCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   LumaData d;
   LumaData *data;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = *vsapi->getVideoInfo(d.node);


   // In this first version we only want to handle 8 and 16bit integer formats. Note that
   // vi->format can be 0 if the input clip can change format midstream.
   if (!d.vi.format || d.vi.format->sampleType != stInteger || (d.vi.format->bitsPerSample != 8 && d.vi.format->bitsPerSample != 16)) {
      vsapi->setError(out, "Luma: only constant format 8 and 16bit integer input supported");
      vsapi->freeNode(d.node);
      return;
   }

   // We don't need any chroma.
   if(d.vi.format->bitsPerSample == 8)
      d.vi.format = vsapi->getFormatPreset(pfGray8, core);
   else
      d.vi.format = vsapi->getFormatPreset(pfGray16, core);
   

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "Luma", lumaInit, lumaGetFrame, lumaFree, fmParallel, 0, data, core);
   return;
}

