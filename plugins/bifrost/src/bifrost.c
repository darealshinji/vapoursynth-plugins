#include <stdint.h>
#include <stdlib.h>
//#include <string.h>

#include <VapourSynth.h>
#include <VSHelper.h>


enum BlendDirection {
   bdNext,
   bdPrev,
   bdBoth
};


typedef struct {
   VSNodeRef *node;
   VSNodeRef *altnode;
   float luma_thresh;
   int variation;
   int conservative_mask;
   int interlaced;
   int block_width;
   int block_height;
   int block_width_uv;
   int block_height_uv;
   int blocks_x;
   int blocks_y;

   const VSVideoInfo *vi;
   float relativeframediff;
   int offset;
} BifrostData;


static void VS_CC bifrostInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   BifrostData *d = (BifrostData *) * instanceData;
   vsapi->setVideoInfo(d->vi, 1, node);
}


static void applyBlockRainbowMask(const uint8_t *srcp_u, const uint8_t *srcp_v,
                                  const uint8_t *srcc_u, const uint8_t *srcc_v,
                                  const uint8_t *srcn_u, const uint8_t *srcn_v,
                                  uint8_t *dst_u, uint8_t *dst_v,
                                  int block_width_uv, int block_height_uv, int stride_uv, int blenddirection) {

   for (int y = 0; y < block_height_uv; y++) {
      if (blenddirection == bdNext) {
         for (int x = 0; x < block_width_uv; x++) {
            if (dst_v[x]) {
               dst_u[x] = (srcc_u[x]+srcn_u[x]+1) >> 1;
               dst_v[x] = (srcc_v[x]+srcn_v[x]+1) >> 1;
            } else {
               dst_u[x] = srcc_u[x];
               dst_v[x] = srcc_v[x];
            }
         }
      } else if (blenddirection == bdPrev) {
         for (int x = 0; x < block_width_uv; x++) {
            if (dst_v[x]) {
               dst_u[x] = (srcc_u[x]+srcp_u[x]+1) >> 1;
               dst_v[x] = (srcc_v[x]+srcp_v[x]+1) >> 1;
            } else {
               dst_u[x] = srcc_u[x];
               dst_v[x] = srcc_v[x];
            }
         }
      } else if (blenddirection == bdBoth) {
         for (int x = 0; x < block_width_uv; x++) {
            if (dst_v[x]) {
               dst_u[x] = (2*srcc_u[x]+srcp_u[x]+srcn_u[x]+3) >> 2;
               dst_v[x] = (2*srcc_v[x]+srcp_v[x]+srcn_v[x]+3) >> 2;
            } else {
               dst_u[x] = srcc_u[x];
               dst_v[x] = srcc_v[x];
            }
         }
      }

      srcp_u += stride_uv;
      srcp_v += stride_uv;

      srcc_u += stride_uv;
      srcc_v += stride_uv;

      srcn_u += stride_uv;
      srcn_v += stride_uv;

      dst_u += stride_uv;
      dst_v += stride_uv;
   }
}


static void processBlockRainbowMask(uint8_t *dst_u, uint8_t *dst_v,
                                    int block_width_uv, int block_height_uv, int stride_uv, int conservative_mask) {

   // Maybe needed later.
   uint8_t *tmp = dst_v;

   //denoise mask, remove marked pixels with no horizontal marked neighbors
   for (int y = 0; y < block_height_uv; y++) {
      dst_v[0] = dst_u[0] && dst_u[1];
      for (int x = 1; x < block_width_uv - 1; x++) {
         dst_v[x] = dst_u[x] && (dst_u[x-1] || dst_u[x+1]);
      }
      dst_v[block_width_uv - 1] = dst_u[block_width_uv - 1] && dst_u[block_width_uv - 2];

      dst_u += stride_uv;
      dst_v += stride_uv;
   }

   //expand mask vertically
   if (!conservative_mask) {
      dst_v = tmp;

      for (int x = 0; x < block_width_uv; x++) {
         dst_v[x] = dst_v[x] || dst_v[x + stride_uv];
      }

      dst_v += stride_uv;

      for (int y = 1; y < block_height_uv - 1; y++) {
         for (int x = 0; x < block_width_uv; x++) {
            dst_v[x] = dst_v[x] || (dst_v[x + stride_uv] && dst_v[x - stride_uv]);
         }

         dst_v += stride_uv;
      }

      for (int x = 0; x < block_width_uv; x++) {
         dst_v[x] = dst_v[x] || dst_v[x - stride_uv];
      }
   }
}


static void makeBlockRainbowMask(const uint8_t *srcp_u, const uint8_t *srcp_v,
                                 const uint8_t *srcc_u, const uint8_t *srcc_v,
                                 const uint8_t *srcn_u, const uint8_t *srcn_v,
                                 uint8_t *dst_u, uint8_t *dst_v,
                                 int block_width_uv, int block_height_uv, int stride_uv, int variation) {

   for (int y = 0; y < block_height_uv; y++) {
      for (int x = 0; x < block_width_uv; x++) {
         uint8_t up = srcp_u[x];
         uint8_t uc = srcc_u[x];
         uint8_t un = srcn_u[x];

         uint8_t vp = srcp_v[x];
         uint8_t vc = srcc_v[x];
         uint8_t vn = srcn_v[x];

         int ucup = uc-up;
         int ucun = uc-un;

         int vcvp = vc-vp;
         int vcvn = vc-vn;

         dst_u[x] = ((( ucup+variation) & ( ucun+variation)) < 0)
                 || (((-ucup+variation) & (-ucun+variation)) < 0)
                 || ((( vcvp+variation) & ( vcvn+variation)) < 0)
                 || (((-vcvp+variation) & (-vcvn+variation)) < 0);
      }

      srcp_u += stride_uv;
      srcp_v += stride_uv;

      srcc_u += stride_uv;
      srcc_v += stride_uv;

      srcn_u += stride_uv;
      srcn_v += stride_uv;

      dst_u += stride_uv;
      dst_v += stride_uv;
   }
}


static void copyChromaBlock(uint8_t *dst_u, uint8_t *dst_v,
                      const uint8_t *src_u, const uint8_t *src_v,
                      int block_width_uv, int block_height_uv, int stride_uv) {

   for (int y = 0; y < block_height_uv; y++) {
      memcpy(dst_u, src_u, block_width_uv);
      memcpy(dst_v, src_v, block_width_uv);

      dst_u += stride_uv;
      dst_v += stride_uv;
      src_u += stride_uv;
      src_v += stride_uv;
   }
}


static int blockLumaDiff(const uint8_t *src1_y, const uint8_t *src2_y, int block_width, int block_height, int stride_y) {
   int diff = 0;

   for (int y = 0; y < block_height; y++) {
      for (int x = 0; x < block_width; x++) {
         diff += abs(src1_y[x] - src2_y[x]);
      }

      src1_y += stride_y;
      src2_y += stride_y;
   }

   return diff;
}


static const VSFrameRef *VS_CC bifrostGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   BifrostData *d = (BifrostData *) * instanceData;

#define min(a, b)  (((a) < (b)) ? (a) : (b))
#define max(a, b)  (((a) > (b)) ? (a) : (b))

   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(max(n - d->offset*2, 0), d->node, frameCtx);
      vsapi->requestFrameFilter(max(n - d->offset,   0), d->node, frameCtx);
      vsapi->requestFrameFilter(n, d->node, frameCtx);
      vsapi->requestFrameFilter(min(n + d->offset,   d->vi->numFrames-1), d->node, frameCtx);
      vsapi->requestFrameFilter(min(n + d->offset*2, d->vi->numFrames-1), d->node, frameCtx);

      vsapi->requestFrameFilter(n, d->altnode, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      const VSFrameRef *srcpp = vsapi->getFrameFilter(max(n - d->offset*2, 0), d->node, frameCtx);
      const VSFrameRef *srcp  = vsapi->getFrameFilter(max(n - d->offset,   0), d->node, frameCtx);
      const VSFrameRef *srcc  = vsapi->getFrameFilter(n, d->node, frameCtx);
      const VSFrameRef *srcn  = vsapi->getFrameFilter(min(n + d->offset,   d->vi->numFrames-1), d->node, frameCtx);
      const VSFrameRef *srcnn = vsapi->getFrameFilter(min(n + d->offset*2, d->vi->numFrames-1), d->node, frameCtx);

      const VSFrameRef *altsrcc = vsapi->getFrameFilter(n, d->altnode, frameCtx);

#undef min
#undef max
      const VSFrameRef *planeSrc[3] = { srcc, NULL, NULL };
      const int planes[3] = { 0 };
      VSFrameRef *dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, planeSrc, planes, srcc, core);
      VSMap *dst_props = vsapi->getFramePropsRW(dst);
      const char *prop = "BifrostLumaDiff";
      vsapi->propDeleteKey(dst_props, prop);


      const uint8_t *srcpp_y = vsapi->getReadPtr(srcpp, 0);
      const uint8_t *srcpp_u = vsapi->getReadPtr(srcpp, 1);
      const uint8_t *srcpp_v = vsapi->getReadPtr(srcpp, 2);
      const VSMap *srcpp_props = vsapi->getFramePropsRO(srcpp);
      const int *srcpp_diffs = (const int *)vsapi->propGetData(srcpp_props, prop, 0, NULL);

      const uint8_t *srcp_y = vsapi->getReadPtr(srcp, 0);
      const uint8_t *srcp_u = vsapi->getReadPtr(srcp, 1);
      const uint8_t *srcp_v = vsapi->getReadPtr(srcp, 2);
      const VSMap *srcp_props = vsapi->getFramePropsRO(srcp);
      const int *srcp_diffs = (const int *)vsapi->propGetData(srcp_props, prop, 0, NULL);

      const uint8_t *srcc_y = vsapi->getReadPtr(srcc, 0);
      const uint8_t *srcc_u = vsapi->getReadPtr(srcc, 1);
      const uint8_t *srcc_v = vsapi->getReadPtr(srcc, 2);
      const VSMap *srcc_props = vsapi->getFramePropsRO(srcc);
      const int *srcc_diffs = (const int *)vsapi->propGetData(srcc_props, prop, 0, NULL);

      const uint8_t *srcn_y = vsapi->getReadPtr(srcn, 0);
      const uint8_t *srcn_u = vsapi->getReadPtr(srcn, 1);
      const uint8_t *srcn_v = vsapi->getReadPtr(srcn, 2);
      const VSMap *srcn_props = vsapi->getFramePropsRO(srcn);
      const int *srcn_diffs = (const int *)vsapi->propGetData(srcn_props, prop, 0, NULL);

      const uint8_t *srcnn_y = vsapi->getReadPtr(srcnn, 0);
      const uint8_t *srcnn_u = vsapi->getReadPtr(srcnn, 1);
      const uint8_t *srcnn_v = vsapi->getReadPtr(srcnn, 2);

      const uint8_t *altsrcc_u = vsapi->getReadPtr(altsrcc, 1);
      const uint8_t *altsrcc_v = vsapi->getReadPtr(altsrcc, 2);

      uint8_t *dst_u = vsapi->getWritePtr(dst, 1);
      uint8_t *dst_v = vsapi->getWritePtr(dst, 2);

      int stride_y = vsapi->getStride(srcc, 0);
      int stride_uv = vsapi->getStride(srcc, 1);

      int block_height = d->block_height;
      int block_width_uv = d->block_width_uv;
      int block_height_uv = d->block_height_uv;

      int blocks_x = d->blocks_x;
      int blocks_y = d->blocks_y;

      for (int y = 0; y < blocks_y; y++) {
         for (int x = 0; x < blocks_x; x++) {
            int current_block = y*blocks_x + x;
            float ldprev = srcp_diffs[current_block];
            float ldnext = srcc_diffs[current_block];
            float ldprevprev = 0.0f;
            float ldnextnext = 0.0f;

            //too much movement in both directions?
            if (ldnext > d->luma_thresh && ldprev > d->luma_thresh) {
               copyChromaBlock(dst_u + block_width_uv*x, dst_v + block_width_uv*x,
                               altsrcc_u + block_width_uv*x, altsrcc_v + block_width_uv*x,
                               block_width_uv, block_height_uv, stride_uv);
               continue;
            }

            if (ldnext > d->luma_thresh) {
               ldprevprev = srcpp_diffs[current_block];
            } else if (ldprev > d->luma_thresh) {
               ldnextnext = srcn_diffs[current_block];
            }

            //two consecutive frames in one direction to generate mask?
            if ((ldnext > d->luma_thresh && ldprevprev > d->luma_thresh) ||
                (ldprev > d->luma_thresh && ldnextnext > d->luma_thresh)) {
               copyChromaBlock(dst_u + block_width_uv*x, dst_v + block_width_uv*x,
                               altsrcc_u + block_width_uv*x, altsrcc_v + block_width_uv*x,
                               block_width_uv, block_height_uv, stride_uv);
               continue;
            }

            //generate mask from correct side of scenechange
            if (ldnext > d->luma_thresh) {
               makeBlockRainbowMask(srcpp_u + block_width_uv*x, srcpp_v + block_width_uv*x,
                                     srcp_u + block_width_uv*x,  srcp_v + block_width_uv*x,
                                     srcc_u + block_width_uv*x,  srcc_v + block_width_uv*x,
                                      dst_u + block_width_uv*x,   dst_v + block_width_uv*x,
                                    block_width_uv, block_height_uv, stride_uv, d->variation);
            } else if (ldprev > d->luma_thresh) {
               makeBlockRainbowMask( srcc_u + block_width_uv*x,  srcc_v + block_width_uv*x,
                                     srcn_u + block_width_uv*x,  srcn_v + block_width_uv*x,
                                    srcnn_u + block_width_uv*x, srcnn_v + block_width_uv*x,
                                      dst_u + block_width_uv*x,   dst_v + block_width_uv*x,
                                    block_width_uv, block_height_uv, stride_uv, d->variation);
            } else {
               makeBlockRainbowMask(srcp_u + block_width_uv*x, srcp_v + block_width_uv*x,
                                    srcc_u + block_width_uv*x, srcc_v + block_width_uv*x,
                                    srcn_u + block_width_uv*x, srcn_v + block_width_uv*x,
                                     dst_u + block_width_uv*x,  dst_v + block_width_uv*x,
                                    block_width_uv, block_height_uv, stride_uv, d->variation);
            }

            //denoise and expand mask
            processBlockRainbowMask(dst_u + block_width_uv*x, dst_v + block_width_uv*x,
                                    block_width_uv, block_height_uv, stride_uv, d->conservative_mask);

            //determine direction to blend in
            int direction;
            if (ldprev > ldnext*d->relativeframediff) {
               direction = bdNext;
            } else if (ldnext > ldprev*d->relativeframediff) {
               direction = bdPrev;
            } else {
               direction = bdBoth;
            }
            applyBlockRainbowMask(srcp_u + block_width_uv*x, srcp_v + block_width_uv*x,
                                  srcc_u + block_width_uv*x, srcc_v + block_width_uv*x,
                                  srcn_u + block_width_uv*x, srcn_v + block_width_uv*x,
                                   dst_u + block_width_uv*x,  dst_v + block_width_uv*x,
                                  block_width_uv, block_height_uv, stride_uv, direction);

         }

         srcpp_y += block_height * stride_y;
         srcpp_u += block_height_uv * stride_uv;
         srcpp_v += block_height_uv * stride_uv;

         srcp_y += block_height * stride_y;
         srcp_u += block_height_uv * stride_uv;
         srcp_v += block_height_uv * stride_uv;

         srcc_y += block_height * stride_y;
         srcc_u += block_height_uv * stride_uv;
         srcc_v += block_height_uv * stride_uv;

         srcn_y += block_height * stride_y;
         srcn_u += block_height_uv * stride_uv;
         srcn_v += block_height_uv * stride_uv;

         srcnn_y += block_height * stride_y;
         srcnn_u += block_height_uv * stride_uv;
         srcnn_v += block_height_uv * stride_uv;

         altsrcc_u += block_height_uv * stride_uv;
         altsrcc_v += block_height_uv * stride_uv;

         dst_u += block_height_uv * stride_uv;
         dst_v += block_height_uv * stride_uv;
      }

      vsapi->freeFrame(srcpp);
      vsapi->freeFrame(srcp);
      vsapi->freeFrame(srcc);
      vsapi->freeFrame(srcn);
      vsapi->freeFrame(srcnn);
      vsapi->freeFrame(altsrcc);

      return dst;
   }

   return 0;
}


static void VS_CC bifrostFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   BifrostData *d = (BifrostData *)instanceData;

   vsapi->freeNode(d->node);
   vsapi->freeNode(d->altnode);
   free(d);
}


static void VS_CC bifrostCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   BifrostData d;
   BifrostData *data;
   int err;

   d.variation = vsapi->propGetInt(in, "variation", 0, &err);
   if (err) {
      d.variation = 5;
   }

   d.interlaced = !!vsapi->propGetInt(in, "interlaced", 0, &err);
   if (err) {
      d.interlaced = 1;
   }

   d.offset = d.interlaced ? 2 : 1;

   d.relativeframediff = 1.2f;

   d.luma_thresh = (float)vsapi->propGetFloat(in, "luma_thresh", 0, &err);
   if (err) {
      d.luma_thresh = 10.0f;
   }

   d.conservative_mask = !!vsapi->propGetInt(in, "conservative_mask", 0, &err);

   d.block_width = vsapi->propGetInt(in, "blockx", 0, &err);
   if (err) {
      d.block_width = 4;
   }

   d.block_height = vsapi->propGetInt(in, "blocky", 0, &err);
   if (err) {
      d.block_height = 4;
   }

   d.luma_thresh = d.luma_thresh * d.block_width * d.block_height;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   d.altnode = vsapi->propGetNode(in, "altclip", 0, &err);
   if (err) {
      d.altnode = vsapi->cloneNodeRef(d.node);
   }
   const VSVideoInfo *altvi = vsapi->getVideoInfo(d.altnode);

   if (!isConstantFormat(d.vi) ||
       d.vi->format->colorFamily != cmYUV ||
       d.vi->format->sampleType != stInteger ||
       d.vi->format->bitsPerSample != 8) {
      vsapi->setError(out, "Bifrost: Only constant format 8 bit integer YUV allowed.");
      vsapi->freeNode(d.node);
      vsapi->freeNode(d.altnode);
      return;
   }

   if (d.vi->format != altvi->format ||
       d.vi->width != altvi->width ||
       d.vi->height != altvi->height ||
       d.vi->numFrames != altvi->numFrames) {

      vsapi->setError(out, "Bifrost: The two input clips must have the same format, dimensions and length.");
      vsapi->freeNode(d.node);
      vsapi->freeNode(d.altnode);
      return;
   }

   VSMap *args = vsapi->createMap();
   VSMap *ret;
   const char *error;
   VSPlugin *stdPlugin = vsapi->getPluginById("com.vapoursynth.std", core);

   if (d.interlaced) {
      vsapi->propSetNode(args, "clip", d.node, paReplace);
      vsapi->freeNode(d.node);
      vsapi->propSetInt(args, "tff", 1, paReplace);
      ret = vsapi->invoke(stdPlugin, "SeparateFields", args);
      error = vsapi->getError(ret);
      if (error) {
         vsapi->setError(out, error);
         vsapi->freeMap(args);
         vsapi->freeMap(ret);
         vsapi->freeNode(d.altnode);
         return;
      }
      d.node = vsapi->propGetNode(ret, "clip", 0, NULL);
      vsapi->freeMap(ret);

      vsapi->clearMap(args);

      vsapi->propSetNode(args, "clip", d.altnode, paReplace);
      vsapi->freeNode(d.altnode);
      vsapi->propSetInt(args, "tff", 1, paReplace);
      ret = vsapi->invoke(stdPlugin, "SeparateFields", args);
      error = vsapi->getError(ret);
      if (error) {
         vsapi->setError(out, error);
         vsapi->freeMap(args);
         vsapi->freeMap(ret);
         vsapi->freeNode(d.node);
         return;
      }
      d.altnode = vsapi->propGetNode(ret, "clip", 0, NULL);
      vsapi->freeMap(ret);
   }

   vsapi->clearMap(args);
   vsapi->propSetNode(args, "clip", d.node, paReplace);
   vsapi->freeNode(d.node);
   
   ret = vsapi->invoke(stdPlugin, "Cache", args);
   error = vsapi->getError(ret);
   if (error) {
      vsapi->setError(out, error);
      vsapi->freeMap(args);
      vsapi->freeMap(ret);
      vsapi->freeNode(d.node);
      vsapi->freeNode(d.altnode);
      return;
   }
   d.node = vsapi->propGetNode(ret, "clip", 0, NULL);
   vsapi->freeMap(ret);

   vsapi->clearMap(args);

   vsapi->propSetNode(args, "clip", d.node, paReplace);
   vsapi->freeNode(d.node);
   vsapi->propSetInt(args, "interlaced", d.interlaced, paReplace);
   vsapi->propSetInt(args, "blockx", d.block_width, paReplace);
   vsapi->propSetInt(args, "blocky", d.block_height, paReplace);

   VSPlugin *bifrostPlugin = vsapi->getPluginById("com.nodame.bifrost", core);
   ret = vsapi->invoke(bifrostPlugin, "BlockDiff", args);
   error = vsapi->getError(ret);
   if (error) {
      vsapi->setError(out, error);
      vsapi->freeMap(args);
      vsapi->freeMap(ret);
      vsapi->freeNode(d.node);
      vsapi->freeNode(d.altnode);
      return;
   }
   d.node = vsapi->propGetNode(ret, "clip", 0, NULL);
   vsapi->freeMap(ret);

   vsapi->clearMap(args);
   vsapi->propSetNode(args, "clip", d.node, paReplace);
   vsapi->freeNode(d.node);
   
   ret = vsapi->invoke(stdPlugin, "Cache", args);
   error = vsapi->getError(ret);
   if (error) {
      vsapi->setError(out, error);
      vsapi->freeMap(args);
      vsapi->freeMap(ret);
      vsapi->freeNode(d.node);
      vsapi->freeNode(d.altnode);
      return;
   }
   d.node = vsapi->propGetNode(ret, "clip", 0, NULL);
   vsapi->freeMap(ret);

   d.vi = vsapi->getVideoInfo(d.node);

   if (d.block_width % (1 << d.vi->format->subSamplingW) ||
       d.block_height % (1 << d.vi->format->subSamplingH)) {
      vsapi->setError(out, "Bifrost: The requested block size is incompatible with the clip's subsampling.");
      vsapi->freeMap(args);
      vsapi->freeNode(d.node);
      vsapi->freeNode(d.altnode);
      return;
   }

   d.block_width_uv = d.block_width >> d.vi->format->subSamplingW;
   d.block_height_uv = d.block_height >> d.vi->format->subSamplingH;

   if (d.block_width_uv < 2 || d.block_height_uv < 2) {
      vsapi->setError(out, "Bifrost: The requested block size is too small.");
      vsapi->freeMap(args);
      vsapi->freeNode(d.node);
      vsapi->freeNode(d.altnode);
      return;
   }

   d.blocks_x = d.vi->width / d.block_width;
   d.blocks_y = d.vi->height / d.block_height;


   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "Bifrost", bifrostInit, bifrostGetFrame, bifrostFree, fmParallel, 0, data, core);

   if (d.interlaced) {
      VSNodeRef *tmpnode = vsapi->propGetNode(out, "clip", 0, NULL);

      vsapi->clearMap(args);

      vsapi->propSetNode(args, "clip", tmpnode, paReplace);
      vsapi->freeNode(tmpnode);
      vsapi->propSetInt(args, "tff", 1, paReplace);
      ret = vsapi->invoke(stdPlugin, "DoubleWeave", args);
      error = vsapi->getError(ret);
      if (error) {
         vsapi->setError(out, error);
         vsapi->freeMap(args);
         vsapi->freeMap(ret);
         vsapi->freeNode(d.node);
         vsapi->freeNode(d.altnode);
         return;
      }
      tmpnode = vsapi->propGetNode(ret, "clip", 0, NULL);
      vsapi->freeMap(ret);

      vsapi->clearMap(args);

      vsapi->propSetNode(args, "clip", tmpnode, paReplace);
      vsapi->freeNode(tmpnode);
      vsapi->propSetInt(args, "cycle", 2, paReplace);
      vsapi->propSetInt(args, "offsets", 0, paReplace);
      ret = vsapi->invoke(stdPlugin, "SelectEvery", args);
      error = vsapi->getError(ret);
      if (error) {
         vsapi->setError(out, error);
         vsapi->freeMap(args);
         vsapi->freeMap(ret);
         vsapi->freeNode(d.node);
         vsapi->freeNode(d.altnode);
         return;
      }
      tmpnode = vsapi->propGetNode(ret, "clip", 0, NULL);
      vsapi->freeMap(ret);
      vsapi->freeMap(args);
      vsapi->propSetNode(out, "clip", tmpnode, paReplace);
      vsapi->freeNode(tmpnode);
   }
}


typedef struct {
   VSNodeRef *node;
   int interlaced;
   int block_width;
   int block_height;
   int blocks_x;
   int blocks_y;

   const VSVideoInfo *vi;
   int offset;
} BlockDiffData;


static void VS_CC blockDiffInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   BlockDiffData *d = (BlockDiffData *) * instanceData;
   vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC blockDiffGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   BlockDiffData *d = (BlockDiffData *) * instanceData;

#define min(a, b)  (((a) < (b)) ? (a) : (b))
#define max(a, b)  (((a) > (b)) ? (a) : (b))
   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(n, d->node, frameCtx);
      vsapi->requestFrameFilter(min(n + d->offset, d->vi->numFrames-1), d->node, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      const VSFrameRef *srcc = vsapi->getFrameFilter(n, d->node, frameCtx);
      const VSFrameRef *srcn = vsapi->getFrameFilter(min(n + d->offset, d->vi->numFrames-1), d->node, frameCtx);
#undef min
#undef max

      VSFrameRef *dst = vsapi->copyFrame(srcc, core);

      VSMap *props = vsapi->getFramePropsRW(dst);

      const uint8_t *srcc_y = vsapi->getReadPtr(srcc, 0);
      const uint8_t *srcn_y = vsapi->getReadPtr(srcn, 0);

      int stride_y = vsapi->getStride(srcc, 0);

      int block_width = d->block_width;
      int block_height = d->block_height;

      int blocks_x = d->blocks_x;
      int blocks_y = d->blocks_y;

      void *diffs = malloc(blocks_x * blocks_y * sizeof(int));

      for (int y = 0; y < blocks_y; y++) {
         for (int x = 0; x < blocks_x; x++) {
            ((int *)diffs)[y*blocks_x+x] = blockLumaDiff(srcc_y + block_width*x, srcn_y + block_width*x, block_width, block_height, stride_y);
         }

         srcc_y += block_height * stride_y;
         srcn_y += block_height * stride_y;
      }

      vsapi->propSetData(props, "BifrostLumaDiff", (const char *)diffs, blocks_x * blocks_y * sizeof(int), paReplace);
      free(diffs);

      vsapi->freeFrame(srcc);
      vsapi->freeFrame(srcn);

      return dst;
   }

   return 0;
}


static void VS_CC blockDiffFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   BlockDiffData *d = (BlockDiffData *)instanceData;

   vsapi->freeNode(d->node);
   free(d);
}


static void VS_CC blockDiffCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   BlockDiffData d;
   BlockDiffData *data;
   int err;

   d.interlaced = !!vsapi->propGetInt(in, "interlaced", 0, &err);
   if (err) {
      d.interlaced = 1;
   }

   d.offset = d.interlaced ? 2 : 1;

   d.block_width = vsapi->propGetInt(in, "blockx", 0, &err);
   if (err) {
      d.block_width = 4;
   }

   d.block_height = vsapi->propGetInt(in, "blocky", 0, &err);
   if (err) {
      d.block_height = 4;
   }

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   d.blocks_x = d.vi->width / d.block_width;
   d.blocks_y = d.vi->height / d.block_height;

   if (!isConstantFormat(d.vi) ||
       d.vi->format->colorFamily != cmYUV ||
       d.vi->format->sampleType != stInteger ||
       d.vi->format->bitsPerSample != 8) {
      vsapi->setError(out, "Bifrost: Only constant format 8 bit integer YUV allowed.");
      vsapi->freeNode(d.node);
      return;
   }


   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "BlockDiff", blockDiffInit, blockDiffGetFrame, blockDiffFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.bifrost", "bifrost", "Bifrost plugin for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("Bifrost",
                "clip:clip;"
                "altclip:clip:opt;"
                "luma_thresh:float:opt;"
                "variation:int:opt;"
                "conservative_mask:int:opt;"
                "interlaced:int:opt;"
                "blockx:int:opt;"
                "blocky:int:opt;",
                bifrostCreate, 0, plugin);
   registerFunc("BlockDiff",
                "clip:clip;"
                "interlaced:int:opt;"
                "blockx:int:opt;"
                "blocky:int:opt;",
                blockDiffCreate, 0, plugin);
}
