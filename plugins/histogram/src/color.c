#include <stdlib.h>
#include <string.h>
#include <VapourSynth.h>

#include "common.h"

typedef struct {
   VSNodeRef *node;
   VSVideoInfo vi;
} ColorData;


static void VS_CC colorInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   ColorData *d = (ColorData *) * instanceData;
   vsapi->setVideoInfo(&d->vi, 1, node);
}


static const VSFrameRef *VS_CC colorGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   ColorData *d = (ColorData *) * instanceData;

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


      const uint8_t *srcp[fi->numPlanes];
      int src_stride[fi->numPlanes];

      uint8_t *dstp[fi->numPlanes];
      int dst_stride[fi->numPlanes];

      int src_height[fi->numPlanes];
      int src_width[fi->numPlanes];

      int dst_height[fi->numPlanes];

      int y;
      int x;

      int plane;

      for (plane = 0; plane < fi->numPlanes; plane++) {
         srcp[plane] = vsapi->getReadPtr(src, plane);
         src_stride[plane] = vsapi->getStride(src, plane);

         dstp[plane] = vsapi->getWritePtr(dst, plane);
         dst_stride[plane] = vsapi->getStride(dst, plane);

         src_height[plane] = vsapi->getFrameHeight(src, plane);
         src_width[plane] = vsapi->getFrameWidth(src, plane);

         dst_height[plane] = vsapi->getFrameHeight(dst, plane);

         // Copy src to dst one line at a time.
         for (y = 0; y < src_height[plane]; y++) {
            memcpy(dstp[plane] + dst_stride[plane] * y,
                   srcp[plane] + src_stride[plane] * y,
                   src_stride[plane]);
         }

         // If src was less than 256 px tall, make the extra lines black.
         if (src_height[plane] < dst_height[plane]) {
            memset(dstp[plane] + src_height[plane] * dst_stride[plane],
                   (plane == 0) ? 16 : 128,
                   (dst_height[plane] - src_height[plane]) * dst_stride[plane]);
         }
      }

      // Why not histUV[256][256] ?
      int histUV[256*256] = {0};

      for (y = 0; y < src_height[U]; y++) {
         for (x = 0; x < src_width[U]; x++) {
            histUV[srcp[V][y * src_stride[V] + x] * 256 + srcp[U][y * src_stride[U] + x]]++;
         }
      }

      int maxval = 1;

      // Original comment: // Should we adjust the divisor (maxval)??

      // Draw the luma.
      for (y = 0; y < 256; y++) {
         for (x = 0; x < 256; x++) {
            int disp_val = histUV[x + y *256] / maxval;
            if (y < 16 || y > 240 || x < 16 || x > 240) {
               disp_val -= 16;
            }
            dstp[Y][src_width[Y] + y * dst_stride[Y] + x] = MIN(235, 16 + disp_val);
         }
      }

      int subW = fi->subSamplingW;
      int subH = fi->subSamplingH;

      // Draw the chroma.
      for (y = 0; y < (256 >> subH); y++) {
         for (x = 0; x < (256 >> subW); x++) {
            dstp[U][src_width[U] + y * dst_stride[U] + x] = x << subW;
            dstp[V][src_width[V] + y * dst_stride[V] + x] = y << subH;
         }
      }

      // Clear the luma under the histogram.
      for (y = 256; y < dst_height[Y]; y++) {
         memset(dstp[Y] + src_width[Y] + y * dst_stride[Y], 16, 256);
      }

      // Clear the chroma under the histogram.
      for (y = (256 >> subH); y < dst_height[U]; y++) {
         // The third argument was originally "(256 >> subW) - 1",
         // leaving the last column uninitialised. (Why?)
         memset(dstp[U] + src_width[U] + y * dst_stride[U], 128, 256 >> subW);
         memset(dstp[V] + src_width[V] + y * dst_stride[V], 128, 256 >> subW);
      }


      // Release the source frame
      vsapi->freeFrame(src);

      // A reference is consumed when it is returned so saving the dst ref somewhere
      // and reusing it is not allowed.
      return dst;
   }

   return 0;
}


static void VS_CC colorFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   ColorData *d = (ColorData *)instanceData;
   vsapi->freeNode(d->node);
   free(d);
}


void VS_CC colorCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   ColorData d;
   ColorData *data;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = *vsapi->getVideoInfo(d.node);


   // In this first version we only want to handle 8bit integer formats. Note that
   // vi->format can be 0 if the input clip can change format midstream.
   if (!d.vi.format || d.vi.format->sampleType != stInteger || d.vi.format->bitsPerSample != 8) {
      vsapi->setError(out, "Color: only constant format 8bit integer input supported");
      vsapi->freeNode(d.node);
      return;
   }

   d.vi.width += 256;
   d.vi.height = MAX(256, d.vi.height);

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "Color", colorInit, colorGetFrame, colorFree, fmParallel, 0, data, core);
   return;
}

