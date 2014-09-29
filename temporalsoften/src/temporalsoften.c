#include <stdlib.h>

#include <VapourSynth.h>


// Computes the sum of absolute differences between plane1 and plane2.
uint64_t there_is_only_c_scenechange(const uint8_t* plane1, const uint8_t* plane2, int height, int width, int stride1, int stride2, int bps) {
   int wp = (width / 32) * 32;
   int hp = height;

   int x, y;
   uint64_t sum = 0;
   for (y = 0; y < hp; y++) {
      for (x = 0; x < wp; x++) {
         int diff;
         if (bps == 8) {
            diff = abs(plane2[x + y * stride2] - plane1[x + y * stride1]);
         } else {
            diff = abs(((uint16_t*)plane2)[x + y * stride2 / 2] - ((uint16_t*)plane1)[x + y * stride1 / 2]);
         }
         sum += diff;
      }
   }
   return sum;
}


// This function processes only one line. The dstp and srcp pointers are modified externally to point to the "current" line.
// I'm not sure my translation of this function would do the right thing to rgb data. [...] I think it would.
void there_is_only_c_accumulate_line_mode2(uint8_t* dstp, const uint8_t** srcp, int frames, int width, int threshold, int div, int half_div, int bps) {
   // dstp: pointer to the destination line. This gets "softened".
   // srcp: array of pointers to the source lines.
   // frames: the number of elements in the srcp array.
   // width: width of a line.
   // threshold: the luma or chroma threshold (whichever we're working on).
   // div: planes+1.
   // half_div: (planes+1)/2.
   // bps: bits per pixel

   int x, frame;

   if (bps == 8) {
      // loop over the pixels in dstp
      for (x = 0; x < width; x++) { // testplane loop
         uint64_t sum = dstp[x];

         // For some reason it wants to start with the last frame.
         for (frame = frames - 1; frame >= 0; frame--) { // kernel_loop
            uint8_t absolute = abs(dstp[x] - srcp[frame][x]);

            if (absolute <= threshold) {
               sum += srcp[frame][x];
            } else {
               sum += dstp[x];
            }
         }

         dstp[x] = (uint8_t)((sum + half_div) / div);
      }
   } else { // yay code duplication
      uint16_t* dstp16 = (uint16_t*)dstp;
      uint16_t** srcp16 = (uint16_t**)srcp;

      // loop over the pixels in dstp
      for (x = 0; x < width; x++) { // testplane loop
         uint64_t sum = dstp16[x];

         // For some reason it wants to start with the last frame.
         for (frame = frames - 1; frame >= 0; frame--) { // kernel_loop
            uint16_t absolute = abs(dstp16[x] - srcp16[frame][x]);

            if (absolute <= threshold) {
               sum += srcp16[frame][x];
            } else {
               sum += dstp16[x];
            }
         }

         dstp16[x] = (uint16_t)((sum + half_div) / div);
      }
   }
}


typedef struct {
   VSNodeRef *node;
   const VSVideoInfo *vi;

   // Filter parameters.
   int radius;
   int luma_threshold;
   int chroma_threshold;
   uint64_t scenechange;
   int mode;
} TemporalSoftenData;


static void VS_CC temporalSoftenInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   TemporalSoftenData *d = (TemporalSoftenData *) * instanceData;
   vsapi->setVideoInfo(d->vi, 1, node);

   d->scenechange *= ((d->vi->width/32)*32) * d->vi->height;

   int bps = d->vi->format->bitsPerSample;
   // FIXME: perhaps don't do this.
   d->luma_threshold   <<= bps - 8;
   d->chroma_threshold <<= bps - 8;
   d->scenechange      <<= bps - 8;
}


static inline int min(int a, int b) {
   return (((a) < (b)) ? (a) : (b));
}


static inline int max(int a, int b) {
   return (((a) > (b)) ? (a) : (b));
}


static const VSFrameRef *VS_CC temporalSoftenGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   TemporalSoftenData *d = (TemporalSoftenData *) * instanceData;

   if (activationReason == arInitial) {
      int i, first, last;

      // Avoid requesting the first and last frames several times.
      first = n - d->radius;
      if (first < 0)
         first = 0;

      last = n + d->radius;
      if (last > d->vi->numFrames - 1)
         last = d->vi->numFrames - 1;

      // Request the frames.
      for (i = first; i <= last; i++) {
         vsapi->requestFrameFilter(i, d->node, frameCtx);
      }
   } else if (activationReason == arAllFramesReady) {
      int i, plane;
      // Not sure why 16... the most we can have is 7*2+1
      const VSFrameRef* src[16];
      // Get the frames.
      for (i = n - d->radius; i <= n + d->radius; i++) {
         src[i - n + d->radius] = vsapi->getFrameFilter(min(d->vi->numFrames - 1, max(i, 0)), d->node, frameCtx);
      }
      
      // Used to be bool, but we don't have bool in C.
      int planeDisabled[16] = { 0 };

      // src[radius] contains frame number n. Copy it to dst.
      VSFrameRef *dst = vsapi->copyFrame(src[d->radius], core);

      const VSFormat *fi = d->vi->format;


      // It's processing loop time!
      // Loop over all the planes
      for (plane = 0; plane < fi->numPlanes; plane++) {
         if (fi->colorFamily != cmRGB) {
            if (plane == 0 && d->luma_threshold == 0) {
               // Skip the luma plane if luma_threshold is 0.
               continue;
            }
            if (plane == 1 && d->chroma_threshold == 0) {
               // Skip the chroma planes if chroma_threshold is 0.
               break;
            }
         } // If it's RGB, process all planes.
         // ^ I like this better than the "planes[c++]" stuff in the original.

         // Use luma_threshold for all planes when the input is RGB.
         int current_threshold = (plane == 0 || fi->colorFamily == cmRGB) ? d->luma_threshold : d->chroma_threshold;
         int dd = 0;
         int src_stride[16];
         int src_stride_trimmed[16];
         const uint8_t *srcp[16];
         const uint8_t *srcp_trimmed[16];

         // Get the plane pointers and strides.
         for (i = 0; i < d->radius; i++) {
            src_stride[dd] = vsapi->getStride(src[i], plane);
            srcp[dd] = vsapi->getReadPtr(src[i], plane);
            dd++;
         }
         for (i = 1; i <= d->radius; i++) {
            src_stride[dd] = vsapi->getStride(src[d->radius + i], plane);
            srcp[dd] = vsapi->getReadPtr(src[d->radius + i], plane);
            dd++;
         }
         int dst_stride = vsapi->getStride(dst, plane);
         uint8_t *dstp = vsapi->getWritePtr(dst, plane);

         // Since planes may be subsampled you have to query the height of them individually
         int h = vsapi->getFrameHeight(src[d->radius], plane);
         int y;
         int w = vsapi->getFrameWidth(src[d->radius], plane);
         //int x;

         if (d->scenechange > 0) {
            int dd2 = 0;
            int skiprest = 0;

            for (i = d->radius - 1; i >= 0; i--) {
               if (!skiprest && !planeDisabled[i]) {
                  uint64_t scenevalues = there_is_only_c_scenechange(dstp, srcp[i], h, w, dst_stride, src_stride[i], fi->bitsPerSample);
                  if (scenevalues < d->scenechange) {
                     src_stride_trimmed[dd2] = src_stride[i];
                     srcp_trimmed[dd2] = srcp[i];
                     dd2++;
                  } else {
                     skiprest = 1;
                  }
                  planeDisabled[i] = skiprest;
               } else {
                  planeDisabled[i] = 1;
               }
            }
            skiprest = 0;

            for (i = 0; i < d->radius; i++) {
               if (!skiprest && !planeDisabled[i + d->radius]) {
                  uint64_t scenevalues = there_is_only_c_scenechange(dstp, srcp[i + d->radius], h, w, dst_stride, src_stride[i + d->radius], fi->bitsPerSample);
                  if (scenevalues < d->scenechange) {
                     src_stride_trimmed[dd2] = src_stride[i + d->radius];
                     srcp_trimmed[dd2] = srcp[i + d->radius];
                     dd2++;
                  } else {
                     skiprest = 1;
                  }
                  planeDisabled[i + d->radius] = skiprest;
               } else {
                  planeDisabled[i + d->radius] = 1;
               }
            }

            for (i = 0; i < dd2; i++) {
               srcp[i] = srcp_trimmed[i];
               src_stride[i] = src_stride_trimmed[i];
            }
            dd = dd2;
         }

         if (dd < 1) {
            // Always free the source frame(s) before returning.
            for (i = 0; i < d->radius * 2 + 1; i++) {
               vsapi->freeFrame(src[i]);
            }

            return dst;
         }

         int c_div = dd + 1;
         int half_c_div = c_div / 2;

         // There was a "if (current_threshold)" in the original at this point, but current_threshold can't be zero here.
         for (y = 0; y < h; y++) {
            // if (mode == 1) {
            //    do_mode1_stuff();
            //    Yeah, this is where the mode 1 code would go
            //    if someone were to translate the mmx_accumulate_line()
            //    function. qtgmc doesn't use mode 1 so I don't care.
            // } else {
            //    there_is_only_c_accumulate_line_mode2(...);
            // }
            there_is_only_c_accumulate_line_mode2(dstp, srcp, dd, w, current_threshold, c_div, half_c_div, fi->bitsPerSample);

            for (i = 0; i < dd; i++) {
               srcp[i] += src_stride[i];
            }
            dstp += dst_stride;
         }
      }


      // Release the source frames.
      for (i = 0; i < d->radius * 2 + 1; i++) {
         vsapi->freeFrame(src[i]);
      }

      // A reference is consumed when it is returned so saving the dst ref somewhere
      // and reusing it is not allowed.
      return dst;
   }

   return 0;
}


static void VS_CC temporalSoftenFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   TemporalSoftenData *d = (TemporalSoftenData *)instanceData;
   vsapi->freeNode(d->node);
   free(d);
}


static void VS_CC temporalSoftenCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   TemporalSoftenData d;
   TemporalSoftenData *data;
   int err;

   // Get a clip reference from the input arguments. This must be freed later.
   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   if (!d.vi->format || d.vi->format->sampleType != stInteger
                     || d.vi->format->bitsPerSample > 16
                     || (d.vi->format->colorFamily != cmYUV && d.vi->format->colorFamily != cmRGB
                                                            && d.vi->format->colorFamily != cmGray)) {
      vsapi->setError(out, "TemporalSoften: only constant format 8..16 bit integer YUV, RGB, or Gray input supported");
      vsapi->freeNode(d.node);
      return;
   }

   // Get the parameters.
   d.radius = vsapi->propGetInt(in, "radius", 0, &err);
   if (err) {
      d.radius = 4;
   }
   d.luma_threshold = vsapi->propGetInt(in, "luma_threshold", 0, &err);
   if (err) {
      d.luma_threshold = 4;
   }
   d.chroma_threshold = vsapi->propGetInt(in, "chroma_threshold", 0, &err);
   if (err) {
      d.chroma_threshold = 8;
   }
   // Unused optional parameters default to 0,
   // which happens to be fine for scenechange.
   d.scenechange = vsapi->propGetInt(in, "scenechange", 0, &err);
   d.mode = vsapi->propGetInt(in, "mode", 0, &err);
   if (err) {
      d.mode = 2;
   }

   // Check the values.
   if (d.radius < 1 || d.radius > 7) {
      vsapi->setError(out, "TemporalSoften: radius must be between 1 and 7 (inclusive)");
      vsapi->freeNode(d.node);
      return;
   }

   if (d.luma_threshold < 0 || d.luma_threshold > 255) {
      vsapi->setError(out, "TemporalSoften: luma_threshold must be between 0 and 255 (inclusive)");
      vsapi->freeNode(d.node);
      return;
   }

   if (d.chroma_threshold < 0 || d.chroma_threshold > 255) {
      vsapi->setError(out, "TemporalSoften: chroma_threshold must be between 0 and 255 (inclusive)");
      vsapi->freeNode(d.node);
      return;
   }

   // With both thresholds at 0 TemporalSoften would do nothing to the frames.
   if (d.luma_threshold == 0 && d.chroma_threshold == 0) {
      vsapi->setError(out, "TemporalSoften: luma_threshold and chroma_threshold can't both be 0");
      vsapi->freeNode(d.node);
      return;
   }

   if (d.luma_threshold == 0 && (d.vi->format->colorFamily == cmRGB || d.vi->format->colorFamily == cmGray)) {
      vsapi->setError(out, "TemporalSoften: luma_threshold must not be 0 when the input is RGB or Gray");
      vsapi->freeNode(d.node);
      return;
   }

   // FIXME: make scenechange work with RGB input.
   if (d.scenechange > 0 && d.vi->format->colorFamily == cmRGB) {
      vsapi->setError(out, "TemporalSoften: scenechange is not available with RGB input");
      vsapi->freeNode(d.node);
      return;
   }

   if (d.scenechange > 254) {
      vsapi->setError(out, "TemporalSoften: scenechange must be between 0 and 254 (inclusive)");
      vsapi->freeNode(d.node);
      return;
   }

   if (d.mode != 2) {
      vsapi->setError(out, "TemporalSoften: mode must be 2. mode 1 is not implemented");
      vsapi->freeNode(d.node);
      return;
   }


   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "TemporalSoften", temporalSoftenInit, temporalSoftenGetFrame, temporalSoftenFree, fmParallel, 0, data, core);
   return;
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.focus.temporalsoften", "focus", "VapourSynth TemporalSoften Filter", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("TemporalSoften", "clip:clip;radius:int:opt;luma_threshold:int:opt;chroma_threshold:int:opt;scenechange:int:opt;mode:int:opt", temporalSoftenCreate, 0, plugin);
}

