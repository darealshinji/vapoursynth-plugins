#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <VapourSynth.h>
#include <VSHelper.h>


typedef uint8_t YPixel;
typedef int16_t IQPixel;


struct sum {
	int c_sum, count;
};


typedef struct {
   VSNodeRef *node;
   const VSVideoInfo *vi;

   int diameter;
   int threshold;
   int interlaced;
} SSIQData;


static inline void sum_pixels11(IQPixel *src, int compareval, IQPixel nucleus, struct sum *destarray) {
   IQPixel bc;
   int csum = 0, num = 0;
   int low = nucleus - compareval;
   int hi  = nucleus + compareval;

   bc = *src;
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+1);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+2);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+3);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+4);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+5);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+6);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+7);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+8);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+9);
   if (bc <= hi && bc >= low) csum += bc, num++;
   bc = *(src+10);
   if (bc <= hi && bc >= low) csum += bc, num++;

   destarray->c_sum += csum;
   destarray->count += num;
}


static inline void sum_pixels(IQPixel *src, int w2m1, int compareval, IQPixel nucleus, struct sum *destarray) {
   IQPixel bc;
   int csum = 0, num = 0;
   int low = nucleus - compareval;
   int hi  = nucleus + compareval;

   do
   {
      bc = *src++;
      if (bc <= hi && bc >= low)
      {
         csum += bc;
         num++;
      }
   } while(++w2m1);
   destarray->c_sum += csum;
   destarray->count += num;
}


static void smoothN(int N, IQPixel *origsrc, IQPixel *origdst, SSIQData *d) { 
   const int w = d->vi->width;
   const int h = d->vi->height;
   IQPixel *src, *dst;
   int xlo, ylo, nRow, aStart;
   int xhi, yhi;
   int x, y, a;

   int MinusN = -N;
   int Nover2 = N/2;
   int TWOW = w + w;
   //int Nsquared = N * N;
   int nucleus, T = (d->threshold+2)/3;
   int Tsquared = (T * T)/3;
   int SqrtTsquared = (int)sqrt((float)Tsquared);

   src = origsrc;
   dst = origdst;
   if (d->interlaced)
   {
      for (y = 0; y < h; y++)
      {
         for (x = 0; x < w; x++)
         {
            IQPixel *kernelsrc;
            struct sum accum;

            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            xlo = x - Nover2; if (xlo < 0) xlo = 0;
            xhi = x + Nover2; if (xhi >= w) xhi = w - 1;
            ylo = y - N + 1; if (ylo < 0) ylo = y & 1;
            yhi = y + N - 1; if (yhi >= h) yhi = h - 1;
            kernelsrc = origsrc + xlo + ylo*w;
            a = (yhi - ylo) / 2 + 1;
            do
            {
               sum_pixels(kernelsrc, -(xhi+1-xlo), SqrtTsquared, nucleus, &accum);
               kernelsrc += TWOW;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
         }
         dst += w;
         src += w;
      }
   }
   else
   {
      int hend = h - Nover2;
      int wend = w - Nover2;
      IQPixel *kernelsrc,*kernelsrc1;
      struct sum accum;

      // Process the first Nover2+1 lines:
      ylo = 0;
      aStart = Nover2 + 1;
      for (y=0; y<Nover2; y++)
      {
         xlo = 0;
         nRow = -(Nover2 + 1);
         for (x=0; x<Nover2; x++) {  
            nucleus = src[x];
            kernelsrc = origsrc;
            accum.count = accum.c_sum = 0;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow--;
         }
         for (; x<wend; x++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + x - Nover2;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, MinusN, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
         }
         nRow = MinusN+1;
         for (; x<w; x++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + x - Nover2;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow++;
         }
         aStart++;
         dst += w;
         src += w;
      }

      // Process the middle lines
      ylo = 0;
      for (; y < hend; y++,ylo++)
      {
         kernelsrc1 = origsrc + ylo*w;
         nRow = -(Nover2 + 1);
         for (x = 0; x<Nover2; x++) {
            nucleus = src[x];
            kernelsrc = kernelsrc1;
            accum.count = accum.c_sum = 0;
            a = N;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow--;
         }
         xlo = 0;
         for (; x < wend; x++,xlo++)
         {
            IQPixel *kernelsrc;
            struct sum accum;

            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + xlo + ylo*w;
            a = N;
            do
            {
               sum_pixels11(kernelsrc, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
         }
         nRow = MinusN+1;
         for (; x<w; x++,xlo++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + xlo + ylo*w;
            a = N;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow++;
         }
         dst += w;
         src += w;
      }

      //  Process the last Nover2 lines
      aStart = N-1;
      for (; y<h; y++,ylo++) {
         kernelsrc1 = origsrc + ylo*w;
         xlo = 0;
         nRow = -(Nover2 + 1);
         for (x=0; x<Nover2; x++) {  
            nucleus = src[x];
            kernelsrc = origsrc + xlo + ylo*w;
            accum.count = accum.c_sum = 0;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow--;
         }
         xlo = 0;
         for (; x<wend; x++,xlo++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + xlo + ylo*w;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, MinusN, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
         }
         nRow = MinusN+1;
         for (; x<w; x++,xlo++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + xlo + ylo*w;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow++;
         }
         aStart--;
         dst += w;
         src += w;
      }
   }
}


static void smooth11(IQPixel *origsrc, IQPixel *origdst, SSIQData *d)
{
   const int w = d->vi->width;
   const int h = d->vi->height;
   IQPixel *src, *dst;
   int xlo, ylo, nRow, aStart;
   int xhi, yhi;
   int x, y, a;

   int TWOW = w + w;
   int nucleus, T = (d->threshold+2)/3;
   int Tsquared = (T * T)/3;
   int SqrtTsquared = (int) sqrt((float)Tsquared);

   src = origsrc;
   dst = origdst;

   if (d->interlaced)
   {
      for (y = 0; y < h; y++)
      {
         for (x = 0; x < w; x++)
         {
            IQPixel *kernelsrc;
            struct sum accum;

            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            xlo = x - 5; if (xlo < 0) xlo = 0;
            xhi = x + 5; if (xhi >= w) xhi = w - 1;
            ylo = y - 11 + 1; if (ylo < 0) ylo = y & 1;
            yhi = y + 11 - 1; if (yhi >= h) yhi = h - 1;
            kernelsrc = origsrc + xlo + ylo*w;
            a = (yhi - ylo) / 2 + 1;
            do
            {
               sum_pixels(kernelsrc, -(xhi+1-xlo), SqrtTsquared, nucleus, &accum);
               kernelsrc += TWOW;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
         }
         dst += w;
         src += w;
      }
   }
   else
   {
      int hend = h - 5;
      int wend = w - 5;
      IQPixel *kernelsrc,*kernelsrc1;
      struct sum accum;

      // Process the first Nover2+1 lines:
      ylo = 0;
      aStart = 5 + 1;
      for (y=0; y<5; y++)
      {
         xlo = 0;
         nRow = -(5 + 1);
         for (x=0; x<5; x++) {  
            nucleus = src[x];
            kernelsrc = origsrc;
            accum.count = accum.c_sum = 0;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow--;
         }
         for (; x<wend; x++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + x - 5;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, -11, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
         }
         nRow = -11+1;
         for (; x<w; x++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + x - 5;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow++;
         }
         aStart++;
         dst += w;
         src += w;
      }

      // Process the middle lines
      ylo = 0;
      for (; y < hend; y++,ylo++)
      {
         kernelsrc1 = origsrc + ylo*w;
         nRow = -(5 + 1);
         for (x = 0; x<5; x++) {
            nucleus = src[x];
            kernelsrc = kernelsrc1;
            accum.count = accum.c_sum = 0;
            a = 11;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow--;
         }
         xlo = 0;
         for (; x < wend; x++,xlo++)
         {
            IQPixel *kernelsrc;
            struct sum accum;

            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + xlo + ylo*w;
            a = 11;
            do
            {
               sum_pixels11(kernelsrc, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
         }
         nRow = -11+1;
         for (; x<w; x++,xlo++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + xlo + ylo*w;
            a = 11;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow++;
         }
         dst += w;
         src += w;
      }

      //  Process the last 5 lines
      aStart = 10;
      for (; y<h; y++,ylo++) {
         kernelsrc1 = origsrc + ylo*w;
         xlo = 0;
         nRow = -(5 + 1);
         for (x=0; x<5; x++) {  
            nucleus = src[x];
            kernelsrc = origsrc + xlo + ylo*w;
            accum.count = accum.c_sum = 0;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow--;
         }
         xlo = 0;
         for (; x<wend; x++,xlo++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + xlo + ylo*w;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, -11, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
         }
         nRow = -11+1;
         for (; x<w; x++,xlo++) {
            nucleus = src[x];
            accum.count = accum.c_sum = 0;
            kernelsrc = origsrc + xlo + ylo*w;
            a = aStart;
            do
            {
               sum_pixels(kernelsrc, nRow, SqrtTsquared, nucleus, &accum);
               kernelsrc += w;
            } while (--a);
            dst[x] = (accum.c_sum + accum.count/2) / accum.count;
            nRow++;
         }
         aStart--;
         dst += w;
         src += w;
      }
   }
}


static void smoothProc(IQPixel *origsrc, IQPixel *origdst, SSIQData *d) {
   if (d->diameter == 11)
      smooth11(origsrc, origdst, d);
   else
      smoothN(d->diameter, origsrc, origdst, d);
}


static void VS_CC ssiqInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   SSIQData *d = (SSIQData *) * instanceData;
   vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC ssiqGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   SSIQData *d = (SSIQData *) * instanceData;

   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(n, d->node, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
      VSFrameRef *dst = vsapi->copyFrame(src, core);

      IQPixel *iqmap1 = malloc(sizeof(IQPixel) * d->vi->width * d->vi->height);
      IQPixel *iqmap2 = malloc(sizeof(IQPixel) * d->vi->width * d->vi->height);
      IQPixel *iqmap3 = malloc(sizeof(IQPixel) * d->vi->width * d->vi->height);

      if (d->vi->format->id == pfYUV420P8) {
         uint8_t *src_u = vsapi->getWritePtr(dst, 1);
         uint8_t *src_v = vsapi->getWritePtr(dst, 2);

         //int rowsize_uv = vsapi->getFrameWidth(dst, 1);
         int pitch_uv = vsapi->getStride(dst, 1);

         IQPixel *li1, *li2, *lq1, *lq2;

         int x, y;

         li1 = iqmap1;
         li2 = iqmap1;
         li2 += d->vi->width;

         lq1 = iqmap2;
         lq2 = iqmap2;	
         lq2 += d->vi->width;

         if (d->interlaced) {
            li2 += d->vi->width;
            lq2 += d->vi->width;
         }

         for (y = 0; y < d->vi->height / 2; y++) {
            for (x = 0; x < d->vi->width / 2; x++) {
               uint8_t srcpixel_u = src_u[x];
               uint8_t srcpixel_v = src_v[x];

               li1[0] = srcpixel_u;
               li1[1] = srcpixel_u;
               li1 += 2;

               li2[0] = srcpixel_u;
               li2[1] = srcpixel_u;
               li2 += 2;

               lq1[0] = srcpixel_v;
               lq1[1] = srcpixel_v;
               lq1 += 2;

               lq2[0] = srcpixel_v;
               lq2[1] = srcpixel_v;
               lq2 += 2;
            }

            src_u += pitch_uv;
            src_v += pitch_uv;

            if (d->interlaced) {
               int ri = 2 * (y & 1) * d->vi->width;

               li1 += ri;
               li2 += ri;

               lq1 += ri;
               lq2 += ri;
            } else {
               int ri = d->vi->width;

               li1 += ri;
               li2 += ri;

               lq1 += ri;
               lq2 += ri;
            }
         }

         smoothProc(iqmap2, iqmap3, d);
         smoothProc(iqmap1, iqmap2, d);

         li1 = iqmap2;
         li2 = iqmap2;
         li2 += d->vi->width;

         lq1 = iqmap3;
         lq2 = iqmap3;	
         lq2 += d->vi->width;

         if (d->interlaced) {
            li2 += d->vi->width;
            lq2 += d->vi->width;
         }

         src_u = vsapi->getWritePtr(dst, 1);
         src_v = vsapi->getWritePtr(dst, 2);

         for (y = 0; y < d->vi->height / 2; y++) {
            for (x = 0; x < d->vi->width / 2; x++) {
               src_u[x] = (li1[0] + li1[1] + li2[0] + li2[1] + 3) / 4;
               li1 += 2;
               li2 += 2;
               src_v[x] = (lq1[0] + lq1[1] + lq2[0] + lq2[1] + 3) / 4;
               lq1 += 2;
               lq2 += 2;
            }

            src_u += pitch_uv;
            src_v += pitch_uv;

            if (d->interlaced) {
               int ri = 2 * (y & 1) * d->vi->width;

               li1 += ri;
               li2 += ri;

               lq1 += ri;
               lq2 += ri;
            } else {
               int ri = d->vi->width;

               li1 += ri;
               li2 += ri;

               lq1 += ri;
               lq2 += ri;
            }
         }

         free(iqmap1);
         free(iqmap2);
         free(iqmap3);
      } else if (d->vi->format->id == pfRGB24) {
#if 0
         YPixel *ymap = malloc(d->vi->width * d->vi->height);
         uint8_t *srcpointer = src->GetWritePtr();

         int rowsize = src->GetRowSize();
         int pitch = src->GetPitch();
         int modulo = pitch - rowsize;
         int height = src->GetHeight();

         int w = vi.width;
         int h = vi.height;
         Pixel *src, *dst;
         YPixel *ly;
         IQPixel *li, *lq;

         ly = ymap;
         li = iqmap1;
         lq = iqmap2;
         src = (Pixel *)srcpointer;
         h = vi.height;
         do {
            w = vi.width;
            do {
               Pixel c;
               long r, g, b;

               c = *src++;
               r = (c>>16) & 255;
               g = (c>> 8) & 255;
               b = c & 255;

               *ly++ = (unsigned char)((19595 * r + 38470 * g + 7471 * b) >> 16);
               *li++ = (short)((39059 * r - 17957 * g - 21103 * b) >> 16);
               *lq++ = (short)((13894 * r - 34275 * g + 20382 * b) >> 16);
            } while(--w);
            src = (Pixel *)((char *)src + modulo);
         } while(--h);

         smoothProc(iqmap2, iqmap3);
         smoothProc(iqmap1, iqmap2);

         ly = ymap;
         li = iqmap2;
         lq = iqmap3;
         dst = (Pixel *)srcpointer;
         h = vi.height;
         do {
            w = vi.width;
            do {
               unsigned char y;
               short i, q, r, g, b;

               y = *ly++;
               i = *li++;
               q = *lq++;

               r = (short)(((y << 16) + 62652 * i + 40698 * q) >> 16);
               g = (short)(((y << 16) - 17826 * i - 42402 * q) >> 16);
               b = (short)(((y << 16) - 72417 * i + 111542 * q) >> 16);

               if (r<0) r=0; else if (r>255) r=255;
               if (g<0) g=0; else if (g>255) g=255;
               if (b<0) b=0; else if (b>255) b=255;
               *dst++ = (r<<16) + (g<<8) + b;
            } while(--w);
            dst = (Pixel *)((char *)dst + modulo);
         } while(--h);

         free(ymap);
#endif
      }

      vsapi->freeFrame(src);

      return dst;
   }

   return 0;
}


static void VS_CC ssiqFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   SSIQData *d = (SSIQData *)instanceData;

   vsapi->freeNode(d->node);
   free(d);
}


static void VS_CC ssiqCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   SSIQData d;
   SSIQData *data;
   int err = 0;

   d.diameter = vsapi->propGetInt(in, "diameter", 0, &err);
   if (err) {
      d.diameter = 11;
   }

   d.threshold = vsapi->propGetInt(in, "strength", 0, &err);
   if (err) {
      d.threshold = 200;
   }

   d.interlaced = !!vsapi->propGetInt(in, "interlaced", 0, &err);

   if (d.diameter < 3 || d.diameter > 11 || !(d.diameter % 2)) {
      vsapi->setError(out, "SSIQ: diameter must be an odd number between 3 and 11 (inclusive).");
      return;
   }

   if (d.threshold < 1 || d.threshold > 300) {
      vsapi->setError(out, "SSIQ: strength must be between 1 and 300 (inclusive).");
      return;
   }


   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   const VSFormat *f = d.vi->format;
   if (!isConstantFormat(d.vi) || (f->id != pfYUV420P8 && f->id != pfRGB24)) {
      vsapi->setError(out, "SSIQ: Only constant format YUV420P8 or RGB24 input supported.");
      vsapi->freeNode(d.node);
      return;
   }


   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "SSIQ", ssiqInit, ssiqGetFrame, ssiqFree, fmParallel, 0, data, core);
   return;
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.ssiq", "ssiq", "SSIQ plugin for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("SSIQ",
                "clip:clip;"
                "diameter:int:opt;"
                "strength:int:opt;"
                "interlaced:int:opt;",
                ssiqCreate, 0, plugin);
}
