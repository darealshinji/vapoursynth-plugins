#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <VapourSynth.h>
#include <VSHelper.h>


enum FillMode {
   ModeFillMargins,
   ModeRepeat,
   ModeMirror
};


typedef struct {
   VSNodeRef *node;
   const VSVideoInfo *vi;

   int left;
   int right;
   int top;
   int bottom;
   int mode;
} FillBordersData;


static void VS_CC fillBordersInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   FillBordersData *d = (FillBordersData *) * instanceData;
   vsapi->setVideoInfo(d->vi, 1, node);
}


static void fillBorders8bit(uint8_t *dstp, int width, int height, int stride, int left, int right, int top, int bottom, int mode) {
   int x, y;

   if (mode == ModeFillMargins) {
      for (y = top; y < height - bottom; y++) {
         memset(dstp + stride*y, (dstp + stride*y)[left], left);
         memset(dstp + stride*y + width - right, (dstp + stride*y + width - right)[-1], right);
      }

      for (y = top - 1; y >= 0; y--) {
         // copy first pixel
         // copy last eight pixels
         dstp[stride*y] = dstp[stride*(y+1)];
         memcpy(dstp + stride*y + width - 8, dstp + stride*(y+1) + width - 8, 8);

         // weighted average for the rest
         for (x = 1; x < width - 8; x++) {
            uint8_t prev = dstp[stride*(y+1) + x - 1];
            uint8_t cur  = dstp[stride*(y+1) + x];
            uint8_t next = dstp[stride*(y+1) + x + 1];
            dstp[stride*y + x] = (3*prev + 2*cur + 3*next + 4) / 8;
         }
      }

      for (y = height - bottom; y < height; y++) {
         // copy first pixel
         // copy last eight pixels
         dstp[stride*y] = dstp[stride*(y-1)];
         memcpy(dstp + stride*y + width - 8, dstp + stride*(y-1) + width - 8, 8);

         // weighted average for the rest
         for (x = 1; x < width - 8; x++) {
            uint8_t prev = dstp[stride*(y-1) + x - 1];
            uint8_t cur  = dstp[stride*(y-1) + x];
            uint8_t next = dstp[stride*(y-1) + x + 1];
            dstp[stride*y + x] = (3*prev + 2*cur + 3*next + 4) / 8;
         }
      }
  } else if (mode == ModeRepeat) {
      for (y = top; y < height - bottom; y++) {
         memset(dstp + stride*y, (dstp + stride*y)[left], left);
         memset(dstp + stride*y + width - right, (dstp + stride*y + width - right)[-1], right);
      }

      for (y = 0; y < top; y++) {
         memcpy(dstp + stride*y, dstp + stride*top, stride);
      }

      for (y = height - bottom; y < height; y++) {
         memcpy(dstp + stride*y, dstp + stride*(height - bottom - 1), stride);
      }
   } else if (mode == ModeMirror) {
      for (y = top; y < height - bottom; y++) {
         for (x = 0; x < left; x++) {
            dstp[stride*y + x] = dstp[stride*y + left*2 - 1 - x];
         }

         for (x = 0; x < right; x++) {
            dstp[stride*y + width - right + x] = dstp[stride*y + width - right - 1 - x];
         }
      }

      for (y = 0; y < top; y++) {
         memcpy(dstp + stride*y, dstp + stride*(top*2 - 1 - y), stride);
      }

      for (y = 0; y < bottom; y++) {
         memcpy(dstp + stride*(height - bottom + y), dstp + stride*(height - bottom - 1 - y), stride);
      }
   }
}


static inline void vs_memset16(void *ptr, int value, size_t num) {
    uint16_t *tptr = (uint16_t *)ptr;
    while (num-- > 0)
        *tptr++ = (uint16_t)value;
}


static void fillBorders16bit(uint8_t *dstp, int width, int height, int stride, int left, int right, int top, int bottom, int mode) {
   int x, y;
   uint16_t *dstp16 = (uint16_t *)dstp;
   stride /= 2;

   if (mode == ModeFillMargins) {
      for (y = top; y < height - bottom; y++) {
         vs_memset16(dstp16 + stride*y, (dstp16 + stride*y)[left], left);
         vs_memset16(dstp16 + stride*y + width - right, (dstp16 + stride*y + width - right)[-1], right);
      }

      for (y = top - 1; y >= 0; y--) {
         // copy first pixel
         // copy last eight pixels
         dstp16[stride*y] = dstp16[stride*(y+1)];
         memcpy(dstp16 + stride*y + width - 8, dstp16 + stride*(y+1) + width - 8, 8*2);

         // weighted average for the rest
         for (x = 1; x < width - 8; x++) {
            uint16_t prev = dstp16[stride*(y+1) + x - 1];
            uint16_t cur  = dstp16[stride*(y+1) + x];
            uint16_t next = dstp16[stride*(y+1) + x + 1];
            dstp16[stride*y + x] = (3*prev + 2*cur + 3*next + 4) / 8;
         }
      }

      for (y = height - bottom; y < height; y++) {
         // copy first pixel
         // copy last eight pixels
         dstp16[stride*y] = dstp16[stride*(y-1)];
         memcpy(dstp16 + stride*y + width - 8, dstp16 + stride*(y-1) + width - 8, 8*2);

         // weighted average for the rest
         for (x = 1; x < width - 8; x++) {
            uint16_t prev = dstp16[stride*(y-1) + x - 1];
            uint16_t cur  = dstp16[stride*(y-1) + x];
            uint16_t next = dstp16[stride*(y-1) + x + 1];
            dstp16[stride*y + x] = (3*prev + 2*cur + 3*next + 4) / 8;
         }
      }
   } else if (mode == ModeRepeat) {
      for (y = top; y < height - bottom; y++) {
         vs_memset16(dstp16 + stride*y, (dstp16 + stride*y)[left], left);
         vs_memset16(dstp16 + stride*y + width - right, (dstp16 + stride*y + width - right)[-1], right);
      }

      for (y = 0; y < top; y++) {
         memcpy(dstp16 + stride*y, dstp16 + stride*top, stride*2);
      }

      for (y = height - bottom; y < height; y++) {
         memcpy(dstp16 + stride*y, dstp16 + stride*(height - bottom - 1), stride*2);
      }
   } else if (mode == ModeMirror) {
      for (y = top; y < height - bottom; y++) {
         for (x = 0; x < left; x++) {
            dstp16[stride*y + x] = dstp16[stride*y + left*2 - 1 - x];
         }

         for (x = 0; x < right; x++) {
            dstp16[stride*y + width - right + x] = dstp16[stride*y + width - right - 1 - x];
         }
      }

      for (y = 0; y < top; y++) {
         memcpy(dstp16 + stride*y, dstp16 + stride*(top*2 - 1 - y), stride*2);
      }

      for (y = 0; y < bottom; y++) {
         memcpy(dstp16 + stride*(height - bottom + y), dstp16 + stride*(height - bottom - 1 - y), stride*2);
      }
   }
}


static const VSFrameRef *VS_CC fillBordersGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   FillBordersData *d = (FillBordersData *) * instanceData;

   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(n, d->node, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
      VSFrameRef *dst = vsapi->copyFrame(src, core);
      int plane;
      vsapi->freeFrame(src);

      if (d->vi->format->bytesPerSample == 1) {
         for (plane = 0; plane < d->vi->format->numPlanes; plane++) {
            uint8_t *dstp = vsapi->getWritePtr(dst, plane);
            int width = vsapi->getFrameWidth(dst, plane);
            int height = vsapi->getFrameHeight(dst, plane);
            int stride = vsapi->getStride(dst, plane);

            if (plane) {
               fillBorders8bit(dstp, width, height, stride, d->left >> d->vi->format->subSamplingW, d->right >> d->vi->format->subSamplingW, d->top >> d->vi->format->subSamplingH, d->bottom >> d->vi->format->subSamplingH, d->mode);
            } else {
               fillBorders8bit(dstp, width, height, stride, d->left, d->right, d->top, d->bottom, d->mode);
            }
         }
      } else if (d->vi->format->bytesPerSample == 2) {
         for (plane = 0; plane < d->vi->format->numPlanes; plane++) {
            uint8_t *dstp = vsapi->getWritePtr(dst, plane);
            int width = vsapi->getFrameWidth(dst, plane);
            int height = vsapi->getFrameHeight(dst, plane);
            int stride = vsapi->getStride(dst, plane);

            if (plane) {
               fillBorders16bit(dstp, width, height, stride, d->left >> d->vi->format->subSamplingW, d->right >> d->vi->format->subSamplingW, d->top >> d->vi->format->subSamplingH, d->bottom >> d->vi->format->subSamplingH, d->mode);
            } else {
               fillBorders16bit(dstp, width, height, stride, d->left, d->right, d->top, d->bottom, d->mode);
            }
         }
      }

      return dst;
   }

   return 0;
}


static void VS_CC fillBordersFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   FillBordersData *d = (FillBordersData *)instanceData;

   vsapi->freeNode(d->node);
   free(d);
}


static void VS_CC fillBordersCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   FillBordersData d;
   FillBordersData *data;
   int err;

   d.left = vsapi->propGetInt(in, "left", 0, &err);
   d.right = vsapi->propGetInt(in, "right", 0, &err);
   d.top = vsapi->propGetInt(in, "top", 0, &err);
   d.bottom = vsapi->propGetInt(in, "bottom", 0, &err);

   const char *mode = vsapi->propGetData(in, "mode", 0, &err);
   if (err) {
      d.mode = ModeRepeat;
   } else {
      if (strcmp(mode, "fillmargins") == 0) {
         d.mode = ModeFillMargins;
      } else if (strcmp(mode, "repeat") == 0) {
         d.mode = ModeRepeat;
      } else if (strcmp(mode, "mirror") == 0) {
         d.mode = ModeMirror;
      } else {
         vsapi->setError(out, "FillBorders: Invalid mode. Valid values are 'fillmargins', 'mirror', and 'repeat'.");
         return;
      }
   }

   if (d.left < 0 || d.right < 0 || d.top < 0 || d.bottom < 0) {
      vsapi->setError(out, "FillBorders: Can't fill a negative number of pixels.");
      return;
   }

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   if (!isConstantFormat(d.vi) || d.vi->format->sampleType != stInteger || d.vi->format->bytesPerSample > 2) {
      vsapi->setError(out, "FillBorders: Only constant format 8..16 bit integer input supported.");
      vsapi->freeNode(d.node);
      return;
   }

   if (!d.left && !d.right && !d.top && !d.bottom) {
      // Just pass the input node through.
      vsapi->propSetNode(out, "clip", d.node, paReplace);
      vsapi->freeNode(d.node);
      return;
   }

   if (d.mode == ModeFillMargins || d.mode == ModeRepeat) {
      if (d.vi->width < d.left + d.right || d.vi->width <= d.left || d.vi->width <= d.right ||
          d.vi->height < d.top + d.bottom || d.vi->height <= d.top || d.vi->height <= d.bottom) {
         vsapi->setError(out, "FillBorders: The input clip is too small or the borders are too big.");
         vsapi->freeNode(d.node);
         return;
      }
   } else if (d.mode == ModeMirror) {
      if (d.vi->width < 2*d.left || d.vi->width < 2*d.right ||
          d.vi->height < 2*d.top || d.vi->height < 2*d.bottom) {
         vsapi->setError(out, "FillBorders: The input clip is too small or the borders are too big.");
         vsapi->freeNode(d.node);
         return;
      }
   }

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "FillBorders", fillBordersInit, fillBordersGetFrame, fillBordersFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.fillborders", "fb", "FillBorders plugin for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("FillBorders",
                "clip:clip;"
                "left:int:opt;"
                "right:int:opt;"
                "top:int:opt;"
                "bottom:int:opt;"
                "mode:data:opt;",
                fillBordersCreate, 0, plugin);
}
