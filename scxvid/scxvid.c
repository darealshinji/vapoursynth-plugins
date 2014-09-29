#include <stdlib.h>
#include <string.h>
#include <xvid.h>
#include <vapoursynth/VapourSynth.h>

#define SCXVID_BUFFER_SIZE (1024*1024*4)

static int xvid_inited = 0;


typedef struct {
   VSNodeRef *node;
   const VSVideoInfo *vi;

   const char *log;
   int use_slices;
   void *xvid_handle;
   xvid_enc_frame_t xvid_enc_frame;
   void *output_buffer;
   int next_frame;
   xvid_enc_create_t xvid_enc_create;
} ScxvidData;


static void VS_CC scxvidInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   ScxvidData *d = (ScxvidData *) * instanceData;
   vsapi->setVideoInfo(d->vi, 1, node);

   d->output_buffer = NULL;
   d->next_frame = 0;
   int error = 0;

   if (!xvid_inited) {
      xvid_gbl_init_t xvid_init;
      memset(&xvid_init, 0, sizeof(xvid_init));
      xvid_init.version = XVID_VERSION;
      xvid_init.debug = ~0;
      error = xvid_global(NULL, XVID_GBL_INIT, &xvid_init, NULL);
      if (error) {
         vsapi->freeNode(d->node);
         free(d);
         vsapi->setError(out, "Scxvid: Failed to initialize Xvid");
         return;
      }
      xvid_inited = 1;
   }

   xvid_gbl_info_t xvid_info;
   memset(&xvid_info, 0, sizeof(xvid_info));
   xvid_info.version = XVID_VERSION;
   error = xvid_global(NULL, XVID_GBL_INFO, &xvid_info, NULL);
   if (error) {
      vsapi->freeNode(d->node);
      free(d);
      vsapi->setError(out, "Scxvid: Failed to initialize Xvid");
      return;
   }

   memset(&d->xvid_enc_create, 0, sizeof(d->xvid_enc_create));
   d->xvid_enc_create.version = XVID_VERSION;
   d->xvid_enc_create.profile = 0;
   d->xvid_enc_create.width = d->vi->width;
   d->xvid_enc_create.height = d->vi->height;
   d->xvid_enc_create.num_threads = xvid_info.num_threads;
   if (d->use_slices)
      d->xvid_enc_create.num_slices = xvid_info.num_threads;
   d->xvid_enc_create.fincr = 1;
   d->xvid_enc_create.fbase = 1;
   d->xvid_enc_create.max_key_interval = 10000000; //huge number
   xvid_enc_plugin_t plugins[1];
   xvid_plugin_2pass1_t xvid_rc_plugin;
   memset(&xvid_rc_plugin, 0, sizeof(xvid_rc_plugin));
   xvid_rc_plugin.version = XVID_VERSION;
   xvid_rc_plugin.filename = d->log;
   plugins[0].func = xvid_plugin_2pass1;
   plugins[0].param = &xvid_rc_plugin;
   d->xvid_enc_create.plugins = plugins;
   d->xvid_enc_create.num_plugins = 1;

   error = xvid_encore(NULL, XVID_ENC_CREATE, &d->xvid_enc_create, NULL);
   if (error) {
      vsapi->freeNode(d->node);
      free(d);
      vsapi->setError(out, "Scxvid: Failed to initialize Xvid encoder");
      return;
   }
   d->xvid_handle = d->xvid_enc_create.handle;

   //default identical(?) to xvid 1.1.2 vfw general preset
   memset(&d->xvid_enc_frame, 0, sizeof(d->xvid_enc_frame));
   d->xvid_enc_frame.version = XVID_VERSION;
   d->xvid_enc_frame.vol_flags = 0;
   d->xvid_enc_frame.vop_flags = XVID_VOP_MODEDECISION_RD
                               | XVID_VOP_HALFPEL
                               | XVID_VOP_HQACPRED
                               | XVID_VOP_TRELLISQUANT
                               | XVID_VOP_INTER4V;

   d->xvid_enc_frame.motion = XVID_ME_CHROMA_PVOP
                            | XVID_ME_CHROMA_BVOP
                            | XVID_ME_HALFPELREFINE16
                            | XVID_ME_EXTSEARCH16
                            | XVID_ME_HALFPELREFINE8
                            | 0
                            | XVID_ME_USESQUARES16;

   d->xvid_enc_frame.type = XVID_TYPE_AUTO;
   d->xvid_enc_frame.quant = 0;

   /* 
    * NOT XVID_CSP_YV12, even though we are feeding it that,
    * because with XVID_CSP_YV12 it assumes the U plane
    * is located just after the Y plane, and the V plane
    * just after the U plane. This used to be the way
    * VapourSynth allocated the planes, before r316.
    *
    * With XVID_CSP_PLANAR it doesn't assume anything and
    * just uses whatever pointers we pass.
    */
   d->xvid_enc_frame.input.csp = XVID_CSP_PLANAR;

   if (!(d->output_buffer = malloc(SCXVID_BUFFER_SIZE))) {
      vsapi->freeNode(d->node);
      xvid_encore(d->xvid_handle, XVID_ENC_DESTROY, NULL, NULL);
      free(d);
      vsapi->setError(out, "Scxvid: Failed to allocate buffer");
      return;
   }
}


static const VSFrameRef *VS_CC scxvidGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   ScxvidData *d = (ScxvidData *) * instanceData;

   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(n, d->node, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);

      if (d->next_frame == n) {
         int plane;
         for (plane = 0; plane < d->vi->format->numPlanes; plane++) {
            d->xvid_enc_frame.input.plane[plane] = (void*)vsapi->getReadPtr(src, plane);
            d->xvid_enc_frame.input.stride[plane] = vsapi->getStride(src, plane);
         }
         
         d->xvid_enc_frame.length = SCXVID_BUFFER_SIZE;
         d->xvid_enc_frame.bitstream = d->output_buffer;

         int error = xvid_encore(d->xvid_handle, XVID_ENC_ENCODE, &d->xvid_enc_frame, NULL);
         if (error < 0) {
            vsapi->setFilterError("Scxvid: xvid_encore returned an error code", frameCtx);
            vsapi->freeFrame(src);
            return 0;
         }
         d->next_frame++;
      }

      return src;
   }

   return 0;
}


static void VS_CC scxvidFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   ScxvidData *d = (ScxvidData *)instanceData;
   vsapi->freeNode(d->node);

   free(d->output_buffer);
   xvid_encore(d->xvid_handle, XVID_ENC_DESTROY, NULL, NULL);

   free(d);
}


static void VS_CC scxvidCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   ScxvidData d;
   ScxvidData *data;
   int err;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   if (!d.vi->format || d.vi->format->id != pfYUV420P8) {
      vsapi->setError(out, "Scxvid: only constant format YUV420P8 input supported");
      vsapi->freeNode(d.node);
      return;
   }

   d.log = vsapi->propGetData(in, "log", 0, 0);
   d.use_slices = vsapi->propGetInt(in, "use_slices", 0, &err);
   if (err) {
      // Enabled by default.
      d.use_slices = 1;
   }

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "Scxvid", scxvidInit, scxvidGetFrame, scxvidFree, fmSerial, 0, data, core);
   return;
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.scxvid", "scxvid", "VapourSynth Scxvid Plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("Scxvid", "clip:clip;log:data;use_slices:int:opt", scxvidCreate, 0, plugin);
}
