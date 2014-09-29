#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <VapourSynth.h>
#include <VSHelper.h>


enum DrawMode {
   dmSide = (1 << 0),
   dmBottom = (1 << 1)
};


enum HistoType {
   htY,
   htU,
   htV,
   htYUV,
   htUV
};


enum CornerType {
   ctBlank,
   ctColormap,
   ctY,
   ctU,
   ctV,
   ctUV
};


typedef struct {
   VSNodeRef *node;
   VSVideoInfo vi;

   int drawmode;
   int tickmarks;
   int histotypeside;
   int histotypebottom;
   int cornertype;

   int hist_width;
   int hist_height;
   int src_width;
   int src_height;

   int row_scale;
   int col_scale;

   int luma_min;
   int luma_max;

   int colors[3];
} ScopeData;


static void VS_CC scopeInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   ScopeData *d = (ScopeData *) * instanceData;

   d->src_width = d->vi.width;
   d->src_height = d->vi.height;

   d->hist_width = d->hist_height = 256;

   if (d->drawmode & dmSide) {
      d->vi.width += d->hist_width;
   }
   if (d->drawmode & dmBottom) {
      d->vi.height += d->hist_height;
   }

   d->row_scale = 3000 / d->src_width;
   d->col_scale = 3000 / d->src_height;

   vsapi->setVideoInfo(&d->vi, 1, node);

   d->luma_min = 16;
   d->luma_max = 235;

   d->colors[0] = 128;
   d->colors[1] = 144;
   d->colors[2] = 112;
}


static void copyFrame(VSFrameRef *dst, const VSFrameRef *src, const VSAPI *vsapi) {
   const VSFormat *format = vsapi->getFrameFormat(src);
   int plane;
   for (plane = 0; plane < format->numPlanes; plane++) {
      const uint8_t *srcp = vsapi->getReadPtr(src, plane);
      uint8_t *dstp = vsapi->getWritePtr(dst, plane);
      int src_stride = vsapi->getStride(src, plane);
      int src_height = vsapi->getFrameHeight(src, plane);
      int dst_stride = vsapi->getStride(dst, plane);
      int y;

      for (y = 0; y < src_height; y++) {
         memcpy(dstp + y*dst_stride, srcp + y*src_stride, src_stride);
      }
   }
}


static int **gatherStatsSide(const VSFrameRef *frame, int plane, ScopeData *d, const VSAPI *vsapi) {
   const uint8_t *ptr = vsapi->getReadPtr(frame, plane);
   int stride = vsapi->getStride(frame, plane);
   int src_width;
   int src_height;
   int x, y;
   int **stats;

   if (plane > 0) {
      src_width = d->src_width >> d->vi.format->subSamplingW;
      src_height = d->src_height >> d->vi.format->subSamplingH;
   } else {
      src_width = d->src_width;
      src_height = d->src_height;
   }

   stats = malloc(src_height * sizeof(int *));
   for (y = 0; y < src_height; y++) {
      stats[y] = malloc(256 * sizeof(int));
      memset(stats[y], 0, 256 * sizeof(int));
      for (x = 0; x < src_width; x++) {
         stats[y][ptr[y*stride + x]]++;
      }
   }

   return stats;
}


static int **gatherStatsBottom(const VSFrameRef *frame, int plane, ScopeData *d, const VSAPI *vsapi) {
   const uint8_t *ptr = vsapi->getReadPtr(frame, plane);
   int stride = vsapi->getStride(frame, plane);
   int src_width;
   int src_height;
   int x, y;
   int **stats;

   if (plane > 0) {
      src_width = d->src_width >> d->vi.format->subSamplingW;
      src_height = d->src_height >> d->vi.format->subSamplingH;
   } else {
      src_width = d->src_width;
      src_height = d->src_height;
   }

   stats = malloc(src_width * sizeof(int *));
   for (x = 0; x < src_width; x++) {
      stats[x] = malloc(256 * sizeof(int));
      memset(stats[x], 0, 256 * sizeof(int));
      for (y = 0; y < src_height; y++) {
         stats[x][ptr[y*stride + x]]++;
      }
   }

   return stats;
}


static void freeStatsSide(int **stats, int plane, ScopeData *d) {
   int src_height = d->src_height;
   int y;

   if (plane > 0) {
      src_height >>= d->vi.format->subSamplingH;
   }

   for (y = 0; y < src_height; y++) {
      free(stats[y]);
   }
   free(stats);
}


static void freeStatsBottom(int **stats, int plane, ScopeData *d) {
   int src_width = d->src_width;
   int x;

   if (plane > 0) {
      src_width >>= d->vi.format->subSamplingH;
   }

   for (x = 0; x < src_width; x++) {
      free(stats[x]);
   }
   free(stats);
}


static void drawSide(VSFrameRef *frame, int plane, int **stats, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   int x, y, p;

   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int src_height = d->src_height >> d->vi.format->subSamplingH;
   int hist_width = d->hist_width >> d->vi.format->subSamplingW;

   for (y = 0; y < d->src_height; y++) {
      for (x = 0; x < d->hist_width; x++) {
         int i = stats[plane ? y >> d->vi.format->subSamplingH : y][x];
         if (i > 0) {
            i = 48 + i*d->row_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
         } else {
            i = d->luma_min;
         }
         luma[y*luma_stride + d->src_width + x] = i;
      }
   }

   for (p = 1; p < d->vi.format->numPlanes; p++) {
      uint8_t *chroma = vsapi->getWritePtr(frame, p);
      int chroma_stride = vsapi->getStride(frame, p);

      for (y = 0; y < src_height; y++) {
         memset(chroma + y*chroma_stride + src_width, d->colors[plane], hist_width);
      }
   }
}


static void drawSideUV(VSFrameRef *frame, int **stats_u, int **stats_v, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   int x, y, p;

   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int src_height = d->src_height >> d->vi.format->subSamplingH;
   int hist_width = d->hist_width >> d->vi.format->subSamplingW;

   for (y = 0; y < d->src_height; y++) {
      int ysub = y >> d->vi.format->subSamplingH;

      for (x = 0; x < d->hist_width/2; x++) {
         int i = stats_u[ysub][x*2] + stats_u[ysub][x*2 + 1];
         if (i > 0) {
            i = 48 + i*d->row_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
         } else {
            i = d->luma_min;
         }
         luma[y*luma_stride + d->src_width + x] = i;

         i = stats_v[ysub][x*2] + stats_v[ysub][x*2 + 1];
         if (i > 0) {
            i = 48 + i*d->row_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
         } else {
            i = d->luma_min;
         }
         luma[y*luma_stride + d->src_width + d->hist_width/2 + x] = i;
      }
   }

   for (p = 1; p < d->vi.format->numPlanes; p++) {
      uint8_t *chroma = vsapi->getWritePtr(frame, p);
      int chroma_stride = vsapi->getStride(frame, p);

      for (y = 0; y < src_height; y++) {
         memset(chroma + y*chroma_stride + src_width, d->colors[1], hist_width/2);
         memset(chroma + y*chroma_stride + src_width + hist_width/2, d->colors[2], hist_width/2);
      }
   }
}


static void drawSideYUV(VSFrameRef *frame, int **stats_y, int **stats_u, int **stats_v, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   uint8_t *chroma_u = vsapi->getWritePtr(frame, 1);
   uint8_t *chroma_v = vsapi->getWritePtr(frame, 2);
   int chroma_stride = vsapi->getStride(frame, 1);

   // FIXME: a more elegant way, maybe?
   int moo = 1 << d->vi.format->subSamplingW;

   int x, y;

   // FIXME: this is confusing. maybe append "_sub" to the names
   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int src_height = d->src_height >> d->vi.format->subSamplingH;
   int hist_width = d->hist_width >> d->vi.format->subSamplingW;

   drawSide(frame, 0, stats_y, d, vsapi);

   for (y = 0; y < d->src_height; y++) {
      int ysub = y >> d->vi.format->subSamplingH;

      for (x = 0; x < d->hist_width/2; x++) {
         int i = stats_u[ysub][x*2] + stats_u[ysub][x*2 + 1];
         if (i > 0) {
            i = 48 + i*d->row_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
            luma[y*luma_stride + d->src_width + x] = i;
         }

         i = stats_v[ysub][x*2] + stats_v[ysub][x*2 + 1];
         if (i > 0) {
            i = 48 + i*d->row_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
            luma[y*luma_stride + d->src_width + d->hist_width/2 + x] = i;
         }
      }
   }

   for (y = 0; y < src_height; y++) {
      for (x = 0; x < hist_width/2; x++) {
         int i = stats_u[y][x*2*moo] + stats_u[y][x*2*moo + 1];
         if (i > 0) {
            chroma_u[y*chroma_stride + src_width + x] = d->colors[1];
         }

         i = stats_u[y][x*2*moo + 2] + stats_u[y][x*2*moo + 3];
         if (i > 0) {
            chroma_v[y*chroma_stride + src_width + x] = d->colors[1];
         }


         i = stats_v[y][x*2*moo] + stats_v[y][x*2*moo + 1];
         if (i > 0) {
            chroma_u[y*chroma_stride + src_width + hist_width/2 + x] = d->colors[2];
         }

         i = stats_v[y][x*2*moo + 2] + stats_v[y][x*2*moo + 3];
         if (i > 0) {
            chroma_v[y*chroma_stride + src_width + hist_width/2 + x] = d->colors[2];
         }
      }
   }
}


static void drawBottom(VSFrameRef *frame, int plane, int **stats, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   int x, y, p;

   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int src_height = d->src_height >> d->vi.format->subSamplingH;
   int height = d->vi.height >> d->vi.format->subSamplingH;

   for (x = 0; x < d->src_width; x++) {
      for (y = 0; y < d->hist_height; y++) {
         int i = stats[plane ? x >> d->vi.format->subSamplingW : x][y];
         if (i > 0) {
            i = 48 + i*d->col_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
         } else {
            i = d->luma_min;
         }
         luma[(d->vi.height - 1 - y)*luma_stride + x] = i;
      }
   }

   for (p = 1; p < d->vi.format->numPlanes; p++) {
      uint8_t *chroma = vsapi->getWritePtr(frame, p);
      int chroma_stride = vsapi->getStride(frame, p);

      for (y = src_height; y < height; y++) {
         memset(chroma + y*chroma_stride, d->colors[plane], src_width);
      }
   }
}


static void drawBottomUV(VSFrameRef *frame, int **stats_u, int **stats_v, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   int x, y, p;

   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int src_height = d->src_height >> d->vi.format->subSamplingH;
   int hist_height = d->hist_height >> d->vi.format->subSamplingH;

   for (x = 0; x < d->src_width; x++) {
      int xsub = x >> d->vi.format->subSamplingW;

      for (y = 0; y < d->hist_height/2; y++) {
         int i = stats_u[xsub][y*2] + stats_u[xsub][y*2 + 1];
         if (i > 0) {
            i = 48 + i*d->col_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
         } else {
            i = d->luma_min;
         }
         luma[(d->vi.height - 1 - y)*luma_stride + x] = i;

         i = stats_v[xsub][y*2] + stats_v[xsub][y*2 + 1];
         if (i > 0) {
            i = 48 + i*d->col_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
         } else {
            i = d->luma_min;
         }
         luma[(d->vi.height - 1 - d->hist_height/2 - y)*luma_stride + x] = i;
      }
   }

   for (p = 1; p < d->vi.format->numPlanes; p++) {
      uint8_t *chroma = vsapi->getWritePtr(frame, p);
      int chroma_stride = vsapi->getStride(frame, p);

      for (y = 0; y < hist_height/2; y++) {
         memset(chroma + (src_height + y)*chroma_stride, d->colors[2], src_width);
         memset(chroma + (src_height + hist_height/2 + y)*chroma_stride, d->colors[1], src_width);
      }
   }
}


static void drawBottomYUV(VSFrameRef *frame, int **stats_y, int **stats_u, int **stats_v, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   uint8_t *chroma_u = vsapi->getWritePtr(frame, 1);
   uint8_t *chroma_v = vsapi->getWritePtr(frame, 2);
   int chroma_stride = vsapi->getStride(frame, 1);

   // FIXME: a more elegant way, maybe?
   int moo = 1 << d->vi.format->subSamplingH;

   int x, y;

   // FIXME: this is confusing. maybe append "_sub" to the names
   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int height = d->vi.height >> d->vi.format->subSamplingH;
   int hist_height = d->hist_height >> d->vi.format->subSamplingH;

   drawBottom(frame, 0, stats_y, d, vsapi);

   for (x = 0; x < d->src_width; x++) {
      int xsub = x >> d->vi.format->subSamplingW;

      for (y = 0; y < d->hist_height/2; y++) {
         int i = stats_u[xsub][y*2] + stats_u[xsub][y*2 + 1];
         if (i > 0) {
            i = 48 + i*d->col_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
            luma[(d->vi.height - 1 - y)*luma_stride + x] = i;
         }

         i = stats_v[xsub][y*2] + stats_v[xsub][y*2 + 1];
         if (i > 0) {
            i = 48 + i*d->col_scale;
            if (i > d->luma_max) {
               i = d->luma_max;
            }
            luma[(d->vi.height - 1 - d->hist_height/2 - y)*luma_stride + x] = i;
         }
      }
   }

   for (x = 0; x < src_width; x++) {
      for (y = 0; y < hist_height/2; y++) {
         int i = stats_u[x][y*2*moo] + stats_u[x][y*2*moo + 1];
         if (i > 0) {
            chroma_u[(height - 1 - y)*chroma_stride + x] = d->colors[1];
         }

         i = stats_u[x][y*2*moo + 2] + stats_u[x][y*2*moo + 3];
         if (i > 0) {
            chroma_v[(height - 1 - y)*chroma_stride + x] = d->colors[1];
         }


         i = stats_v[x][y*2*moo] + stats_v[x][y*2*moo + 1];
         if (i > 0) {
            chroma_u[(height - 1 - hist_height/2 - y)*chroma_stride + x] = d->colors[2];
         }

         i = stats_v[x][y*2*moo + 2] + stats_v[x][y*2*moo + 3];
         if (i > 0) {
            chroma_v[(height - 1 - hist_height/2 - y)*chroma_stride + x] = d->colors[2];
         }
      }
   }
}


static void drawCornerBlank(VSFrameRef *frame, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   int y, p;

   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int src_height = d->src_height >> d->vi.format->subSamplingH;
   int hist_width = d->hist_width >> d->vi.format->subSamplingW;
   int hist_height = d->hist_height >> d->vi.format->subSamplingH;

   for (y = 0; y < d->hist_height; y++) {
      memset(luma + (d->src_height + y)*luma_stride + d->src_width, d->luma_min, d->hist_width);
   }

   for (p = 1; p < d->vi.format->numPlanes; p++) {
      uint8_t *chroma = vsapi->getWritePtr(frame, p);
      int chroma_stride = vsapi->getStride(frame, p);

      for (y = 0; y < hist_height; y++) {
         memset(chroma + (src_height + y)*chroma_stride + src_width, 128, hist_width);
      }
   }
}


static void drawCorner(VSFrameRef *frame, int *framesum, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);

   int maximum;
   double scale;
   int x, y, j;

   maximum = 0;
   for (x = 0; x < d->hist_width; x++) {
      if (framesum[x] > maximum) {
         maximum = framesum[x];
      }
   }

   scale = maximum / 254.0;

   y = 0;
   for (x = 0; x < d->hist_width; x++) {
      int old_y = y;
      y = (int)(framesum[x] / scale);

      if (old_y < y) {
         for (j = old_y; j <= y; j++) {
            luma[(d->vi.height - 1 - j)*luma_stride + d->src_width + x] = 192;
         }
      } else {
         for (j = old_y; j >= y; j--) {
            luma[(d->vi.height - 1 - j)*luma_stride + d->src_width + x] = 192;
         }
      }
   }
}


static int *calcSum(int **stats, int plane, ScopeData *d) {
   int *sum;
   int height = d->src_height;
   int x, y;

   sum = malloc(d->hist_width * sizeof(int));
   memset(sum, 0, d->hist_width * sizeof(int));

   if (plane) {
      height >>= d->vi.format->subSamplingH;
   }

   for (y = 0; y < height; y++) {
      for (x = 0; x < d->hist_width; x++) {
         sum[x] += stats[y][x];
      }
   }

   return sum;
}


static void drawCornerColormap(VSFrameRef *frame, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   uint8_t *chroma_u = vsapi->getWritePtr(frame, 1);
   uint8_t *chroma_v = vsapi->getWritePtr(frame, 2);
   int chroma_stride = vsapi->getStride(frame, 1);

   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int height = d->vi.height >> d->vi.format->subSamplingH;
   int hist_width = d->hist_width >> d->vi.format->subSamplingW;
   int hist_height = d->hist_height >> d->vi.format->subSamplingH;

   int x, y;

   for (y = 0; y < d->hist_height; y++) {
      memset(luma + (d->src_height + y)*luma_stride + d->src_width, 128, d->hist_width);
   }

   for (y = 0; y < hist_height; y++) {
      for (x = 0; x < hist_width; x++) {
         // TV range
         chroma_u[(height - 1 - y)*chroma_stride + src_width + x] = (x << d->vi.format->subSamplingW)*224/256 + 16;
         chroma_v[(height - 1 - y)*chroma_stride + src_width + x] = (y << d->vi.format->subSamplingH)*224/256 + 16;
      }
   }
}


static void drawCornerUV(VSFrameRef *frame, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);
   const uint8_t *chroma_u = vsapi->getReadPtr(frame, 1);
   const uint8_t *chroma_v = vsapi->getReadPtr(frame, 2);
   int chroma_stride = vsapi->getStride(frame, 1);

   double scale = 500000.0 / (d->src_width * d->src_height);
   int x, y;

   int src_width = d->src_width >> d->vi.format->subSamplingW;
   int src_height = d->src_height >> d->vi.format->subSamplingH;

   int *UVmap = malloc(d->hist_width * d->hist_height * sizeof(int));
   memset(UVmap, 0, d->hist_width * d->hist_height * sizeof(int));

   for (y = 0; y < src_height; y++) {
      for (x = 0; x < src_width; x++) {
         UVmap[chroma_v[y*chroma_stride + x]*d->hist_width + chroma_u[y*chroma_stride + x]]++;
      }
   }


   for (y = 0; y < d->hist_height; y++) {
      for (x = 0; x < d->hist_width-1 /*why?*/; x++) {
         int i = UVmap[y*d->hist_width + x];
         if (i > 0) {
            i = (int)(i * scale);
            if (i > d->luma_max) {
               i = d->luma_max;
            }
            luma[(d->vi.height - 1 - y)*luma_stride + d->src_width + x] = i;
         }
      }
   }

   free(UVmap);

   memset(luma + (d->src_height + d->hist_height/2)*luma_stride + d->src_width, d->luma_min, d->hist_width);

   for (y = 0; y < d->hist_height; y++) {
      luma[(d->src_height + y)*luma_stride + d->src_width + d->hist_width/2] = d->luma_min;
   }
}


static void drawSideTickmarks(VSFrameRef *frame, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);

   int x, y;

   for (y = 8; y < d->src_height; y += 16) {
      for (x = 0; x < d->hist_width; x += 16) {
         luma[y*luma_stride + d->src_width + x] = 192;
         
         if ((y & 63) == 56) {
            luma[y*luma_stride + d->src_width + 8 + x] = 192;
         } else if ((y & 63) == 24) {
            luma[y*luma_stride + d->src_width + 4 + x] = 192;
            luma[y*luma_stride + d->src_width + 12 + x] = 192;
         }
      }
   }
}


static void drawBottomTickmarks(VSFrameRef *frame, ScopeData *d, const VSAPI *vsapi) {
   uint8_t *luma = vsapi->getWritePtr(frame, 0);
   int luma_stride = vsapi->getStride(frame, 0);

   int x, y;

   for (x = 8; x < d->src_width; x += 16) {
      for (y = 0; y < d->hist_height; y += 16) {
         luma[(d->vi.height - 1 - y)*luma_stride + x] = 192;

         if ((x & 63) == 56) {
            luma[(d->vi.height - 1 - 8 - y)*luma_stride + x] = 192;
         } else if ((x & 63) == 24) {
            luma[(d->vi.height - 1 - 4 - y)*luma_stride + x] = 192;
            luma[(d->vi.height - 1 - 12 - y)*luma_stride + x] = 192;
         }
      }
   }
}


static const VSFrameRef *VS_CC scopeGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   ScopeData *d = (ScopeData *) * instanceData;

   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(n, d->node, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
      VSFrameRef *dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);

      int **stats_side_y = NULL;
      int **stats_side_u = NULL;
      int **stats_side_v = NULL;

      copyFrame(dst, src, vsapi);
      vsapi->freeFrame(src);

      if (d->drawmode & dmSide) {
         switch (d->histotypeside) {
            case htY:
               stats_side_y = gatherStatsSide(dst, 0, d, vsapi);
               drawSide(dst, 0, stats_side_y, d, vsapi);
               break;
            case htU:
               stats_side_u = gatherStatsSide(dst, 1, d, vsapi);
               drawSide(dst, 1, stats_side_u, d, vsapi);
               break;
            case htV:
               stats_side_v = gatherStatsSide(dst, 2, d, vsapi);
               drawSide(dst, 2, stats_side_v, d, vsapi);
               break;
            case htYUV:
               stats_side_y = gatherStatsSide(dst, 0, d, vsapi);
               stats_side_u = gatherStatsSide(dst, 1, d, vsapi);
               stats_side_v = gatherStatsSide(dst, 2, d, vsapi);
               drawSideYUV(dst, stats_side_y, stats_side_u, stats_side_v, d, vsapi);
               break;
            case htUV:
               stats_side_u = gatherStatsSide(dst, 1, d, vsapi);
               stats_side_v = gatherStatsSide(dst, 2, d, vsapi);
               drawSideUV(dst, stats_side_u, stats_side_v, d, vsapi);
               break;
         }

         if (d->tickmarks) {
            drawSideTickmarks(dst, d, vsapi);
         }
      }

      if (d->drawmode & dmBottom) {
         int **stats_bottom_y = NULL;
         int **stats_bottom_u = NULL;
         int **stats_bottom_v = NULL;

         switch (d->histotypebottom) {
            case htY:
               stats_bottom_y = gatherStatsBottom(dst, 0, d, vsapi);
               drawBottom(dst, 0, stats_bottom_y, d, vsapi);
               freeStatsBottom(stats_bottom_y, 0, d);
               break;
            case htU:
               stats_bottom_u = gatherStatsBottom(dst, 1, d, vsapi);
               drawBottom(dst, 1, stats_bottom_u, d, vsapi);
               freeStatsBottom(stats_bottom_u, 1, d);
               break;
            case htV:
               stats_bottom_v = gatherStatsBottom(dst, 2, d, vsapi);
               drawBottom(dst, 2, stats_bottom_v, d, vsapi);
               freeStatsBottom(stats_bottom_v, 2, d);
               break;
            case htYUV:
               stats_bottom_y = gatherStatsBottom(dst, 0, d, vsapi);
               stats_bottom_u = gatherStatsBottom(dst, 1, d, vsapi);
               stats_bottom_v = gatherStatsBottom(dst, 2, d, vsapi);
               drawBottomYUV(dst, stats_bottom_y, stats_bottom_u, stats_bottom_v, d, vsapi);
               freeStatsBottom(stats_bottom_y, 0, d);
               freeStatsBottom(stats_bottom_u, 1, d);
               freeStatsBottom(stats_bottom_v, 2, d);
               break;
            case htUV:
               stats_bottom_u = gatherStatsBottom(dst, 1, d, vsapi);
               stats_bottom_v = gatherStatsBottom(dst, 2, d, vsapi);
               drawBottomUV(dst, stats_bottom_u, stats_bottom_v, d, vsapi);
               freeStatsBottom(stats_bottom_u, 1, d);
               freeStatsBottom(stats_bottom_v, 2, d);
               break;
         }

         if (d->tickmarks) {
            drawBottomTickmarks(dst, d, vsapi);
         }
      }

      if (d->drawmode == (dmSide | dmBottom)) {
         int *framesum_y = NULL;
         int *framesum_u = NULL;
         int *framesum_v = NULL;

         switch (d->cornertype) {
            case ctBlank:
               drawCornerBlank(dst, d, vsapi);
               break;
            case ctY:
               drawCornerBlank(dst, d, vsapi);
               if (!stats_side_y) {
                  stats_side_y = gatherStatsSide(dst, 0, d, vsapi);
               }
               framesum_y = calcSum(stats_side_y, 0, d);
               drawCorner(dst, framesum_y, d, vsapi);
               free(framesum_y);
               break;
            case ctU:
               drawCornerBlank(dst, d, vsapi);
               if (!stats_side_u) {
                  stats_side_u = gatherStatsSide(dst, 1, d, vsapi);
               }
               framesum_u = calcSum(stats_side_u, 1, d);
               drawCorner(dst, framesum_u, d, vsapi);
               free(framesum_u);
               break;
            case ctV:
               drawCornerBlank(dst, d, vsapi);
               if (!stats_side_v) {
                  stats_side_v = gatherStatsSide(dst, 2, d, vsapi);
               }
               framesum_v = calcSum(stats_side_v, 2, d);
               drawCorner(dst, framesum_v, d, vsapi);
               free(framesum_v);
               break;
            case ctColormap:
               drawCornerColormap(dst, d, vsapi);
               break;
            case ctUV:
               drawCornerColormap(dst, d, vsapi);
               drawCornerUV(dst, d, vsapi);
               break;
         }
      }

      if (stats_side_y) {
         freeStatsSide(stats_side_y, 0, d);
      }
      if (stats_side_u) {
         freeStatsSide(stats_side_u, 1, d);
      }
      if (stats_side_v) {
         freeStatsSide(stats_side_v, 2, d);
      }

      return dst;
   }

   return 0;
}


static void VS_CC scopeFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   ScopeData *d = (ScopeData *)instanceData;

   vsapi->freeNode(d->node);
   free(d);
}


static void VS_CC scopeCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   ScopeData d;
   ScopeData *data;
   int err;
   const char *arg;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = *vsapi->getVideoInfo(d.node);

   arg = vsapi->propGetData(in, "mode", 0, &err);
   if (err) {
      d.drawmode = dmSide | dmBottom;
   } else {
      if (strcmp(arg, "both") == 0) {
         d.drawmode = dmSide | dmBottom;
      } else if (strcmp(arg, "side") == 0) {
         d.drawmode = dmSide;
      } else if (strcmp(arg, "bottom") == 0) {
         d.drawmode = dmBottom;
      } else {
         vsapi->setError(out, "Scope: mode can only be one of 'side', 'bottom', or 'both'.");
         vsapi->freeNode(d.node);
         return;
      }
   }

   d.tickmarks = !!vsapi->propGetInt(in, "tickmarks", 0, &err);
   if (err) {
      d.tickmarks = 1;
   }

   arg = vsapi->propGetData(in, "side", 0, &err);
   if (err) {
      d.histotypeside = htY;
   } else {
      if (strcmp(arg, "Y") == 0) {
         d.histotypeside = htY;
      } else if (strcmp(arg, "U") == 0) {
         d.histotypeside = htU;
      } else if (strcmp(arg, "V") == 0) {
         d.histotypeside = htV;
      } else if (strcmp(arg, "YUV") == 0) {
         d.histotypeside = htYUV;
      } else if (strcmp(arg, "UV") == 0) {
         d.histotypeside = htUV;
      } else {
         vsapi->setError(out, "Scope: side can only be one of 'Y', 'U', 'V', 'YUV', or 'UV'.");
         vsapi->freeNode(d.node);
         return;
      }
   }

   arg = vsapi->propGetData(in, "bottom", 0, &err);
   if (err) {
      d.histotypebottom = htY;
   } else {
      if (strcmp(arg, "Y") == 0) {
         d.histotypebottom = htY;
      } else if (strcmp(arg, "U") == 0) {
         d.histotypebottom = htU;
      } else if (strcmp(arg, "V") == 0) {
         d.histotypebottom = htV;
      } else if (strcmp(arg, "YUV") == 0) {
         d.histotypebottom = htYUV;
      } else if (strcmp(arg, "UV") == 0) {
         d.histotypebottom = htUV;
      } else {
         vsapi->setError(out, "Scope: bottom can only be one of 'Y', 'U', 'V', 'YUV', or 'UV'.");
         vsapi->freeNode(d.node);
         return;
      }
   }

   arg = vsapi->propGetData(in, "corner", 0, &err);
   if (err) {
      d.cornertype = ctBlank;
   } else {
      if (strcmp(arg, "blank") == 0) {
         d.cornertype = ctBlank;
      } else if (strcmp(arg, "colormap") == 0) {
         d.cornertype = ctColormap;
      } else if (strcmp(arg, "Y") == 0) {
         d.cornertype = ctY;
      } else if (strcmp(arg, "U") == 0) {
         d.cornertype = ctU;
      } else if (strcmp(arg, "V") == 0) {
         d.cornertype = ctV;
      } else if (strcmp(arg, "UV") == 0) {
         d.cornertype = ctUV;
      } else {
         vsapi->setError(out, "Scope: corner can only be one of 'blank', 'colormap', 'Y', 'U', 'V', or 'UV'.");
         vsapi->freeNode(d.node);
         return;
      }
   }

   if (!isConstantFormat(&d.vi) ||
       (d.vi.format->colorFamily != cmYUV &&
       d.vi.format->colorFamily != cmGray) ||
       d.vi.format->bitsPerSample != 8 ||
       d.vi.format->sampleType != stInteger) {
      vsapi->setError(out, "Scope: only constant format, 8 bit, YUV or Gray formats.");
      vsapi->freeNode(d.node);
      return;
   }

   if (d.vi.format->colorFamily == cmGray) {
      if (d.histotypeside != htY) {
         vsapi->setError(out, "Scope: side can only be 'Y' when input is Gray.");
         vsapi->freeNode(d.node);
         return;
      }

      if (d.histotypebottom != htY) {
         vsapi->setError(out, "Scope: bottom can only be 'Y' when input is Gray.");
         vsapi->freeNode(d.node);
         return;
      }

      if (d.cornertype != ctBlank && d.cornertype != ctY) {
         vsapi->setError(out, "Scope: corner can only be 'blank' or 'Y' when input is Gray.");
         vsapi->freeNode(d.node);
         return;
      }
   }


   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "Scope", scopeInit, scopeGetFrame, scopeFree, fmParallel, 0, data, core);
   return;
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.videoscope", "vscope", "Videoscope plugin for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("Scope",
                "clip:clip;"
                "mode:data:opt;"
                "tickmarks:int:opt;"
                "side:data:opt;"
                "bottom:data:opt;"
                "corner:data:opt;",
                scopeCreate, 0, plugin);
}
