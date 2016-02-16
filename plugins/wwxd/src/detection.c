#include <stdlib.h>
#include <stdio.h>

#include <VSHelper.h>

#include "detection.h"


#define MVequal(A,B) ( ((A).x)==((B).x) && ((A).y)==((B).y) )

#define iDiamondSize 2

#define CHECK_CANDIDATE(X,Y,D) { \
   CheckCandidate((X),(Y), data, (D) ); }

#define MV_MAX_ERROR   (4096 * 256)

#define MODE_INTER      0
#define MODE_INTRA      3


typedef void(CheckFunc)(const int x, const int y,
      SearchData * const Data,
      const unsigned int Direction);


static uint32_t
sad16_c(const uint8_t * const cur,
      const uint8_t * const ref,
      const uint32_t stride)
{

   uint32_t sad = 0;
   uint32_t j;
   uint8_t const *ptr_cur = cur;
   uint8_t const *ptr_ref = ref;

   for (j = 0; j < 16; j++) {
      sad += abs(ptr_cur[0] - ptr_ref[0]);
      sad += abs(ptr_cur[1] - ptr_ref[1]);
      sad += abs(ptr_cur[2] - ptr_ref[2]);
      sad += abs(ptr_cur[3] - ptr_ref[3]);
      sad += abs(ptr_cur[4] - ptr_ref[4]);
      sad += abs(ptr_cur[5] - ptr_ref[5]);
      sad += abs(ptr_cur[6] - ptr_ref[6]);
      sad += abs(ptr_cur[7] - ptr_ref[7]);
      sad += abs(ptr_cur[8] - ptr_ref[8]);
      sad += abs(ptr_cur[9] - ptr_ref[9]);
      sad += abs(ptr_cur[10] - ptr_ref[10]);
      sad += abs(ptr_cur[11] - ptr_ref[11]);
      sad += abs(ptr_cur[12] - ptr_ref[12]);
      sad += abs(ptr_cur[13] - ptr_ref[13]);
      sad += abs(ptr_cur[14] - ptr_ref[14]);
      sad += abs(ptr_cur[15] - ptr_ref[15]);

      ptr_cur += stride;
      ptr_ref += stride;

   }

   return sad;

}

static uint32_t sad32v_c(const uint8_t * const cur,
      const uint8_t * const ref,
      const uint32_t stride,
      int32_t *sad)
{
   sad[0] = sad16_c(cur, ref, stride);
   sad[1] = sad16_c(cur + 16, ref + 16, stride);
   sad[2] = sad16_c(cur + 16*stride, ref + 16*stride, stride);
   sad[3] = sad16_c(cur + 16*stride + 16, ref + 16*stride + 16, stride);

   return sad[0]+sad[1]+sad[2]+sad[3];
}


static void
CheckCandidate32I(const int x, const int y, SearchData * const data, const unsigned int Direction)
{
   /* maximum speed */
   int32_t sad;

   if ( (x > data->max_dx) || (x < data->min_dx)
         || (y > data->max_dy) || (y < data->min_dy) ) return;

   sad = sad32v_c(data->Cur, data->RefP[0] + x + y*((int)data->iEdgedWidth),
         data->iEdgedWidth, data->temp);

   if (sad < *(data->iMinSAD)) {
      *(data->iMinSAD) = sad;
      data->currentMV[0].x = x; data->currentMV[0].y = y;
      data->dir = Direction;
   }
   if (data->temp[0] < data->iMinSAD[1]) {
      data->iMinSAD[1] = data->temp[0]; data->currentMV[1].x = x; data->currentMV[1].y = y; }
   if (data->temp[1] < data->iMinSAD[2]) {
      data->iMinSAD[2] = data->temp[1]; data->currentMV[2].x = x; data->currentMV[2].y = y; }
   if (data->temp[2] < data->iMinSAD[3]) {
      data->iMinSAD[3] = data->temp[2]; data->currentMV[3].x = x; data->currentMV[3].y = y; }
   if (data->temp[3] < data->iMinSAD[4]) {
      data->iMinSAD[4] = data->temp[3]; data->currentMV[4].x = x; data->currentMV[4].y = y; }
}


/*
 * Calculate the min/max range
 * relative to the _MACROBLOCK_ position
 */
static inline void 
get_range(int32_t * const min_dx,
      int32_t * const max_dx,
      int32_t * const min_dy,
      int32_t * const max_dy,
      const uint32_t x,
      const uint32_t y,
      const uint32_t width,
      const uint32_t height,
      int fcode)
{
   const uint32_t block_sz = 4;
   const int precision = 0;
   int k;
   const int search_range = 1 << (4+fcode);
   int high = search_range - 1;
   int low = -search_range;

   k = (int)(width - (x<<block_sz))<<precision;
   *max_dx = VSMIN(high, k);
   k = (int)(height -  (y<<block_sz))<<precision;
   *max_dy = VSMIN(high, k);

   k = (-(int)((x+1)<<block_sz))<<precision;
   *min_dx = VSMAX(low, k);
   k = (-(int)((y+1)<<block_sz))<<precision;
   *min_dy = VSMAX(low, k);
}


static VECTOR
get_pmv2(const MACROBLOCK * const mbs,
      const int mb_width,
      const int x,
      const int y)
{
   const int block = 0;
   const int bound = 0;
   int lx, ly, lz;      /* left */
   int tx, ty, tz;      /* top */
   int rx, ry, rz;      /* top-right */
   int lpos, tpos, rpos;
   int num_cand = 0, last_cand = 1;

   VECTOR pmv[4];   /* left neighbour, top neighbour, top-right neighbour */

   switch (block) {
      case 0:
         lx = x - 1;   ly = y;      lz = 1;
         tx = x;      ty = y - 1;   tz = 2;
         rx = x + 1;   ry = y - 1;   rz = 2;
         break;
      case 1:
         lx = x;      ly = y;      lz = 0;
         tx = x;      ty = y - 1;   tz = 3;
         rx = x + 1;   ry = y - 1;   rz = 2;
         break;
      case 2:
         lx = x - 1;   ly = y;      lz = 3;
         tx = x;      ty = y;      tz = 0;
         rx = x;      ry = y;      rz = 1;
         break;
      default:
         lx = x;      ly = y;      lz = 2;
         tx = x;      ty = y;      tz = 0;
         rx = x;      ry = y;      rz = 1;
   }

   lpos = lx + ly * mb_width;
   rpos = rx + ry * mb_width;
   tpos = tx + ty * mb_width;

   if (lpos >= bound && lx >= 0) {
      num_cand++;
      pmv[1] = mbs[lpos].mvs[lz];
   } else pmv[1] = (VECTOR){ 0, 0 };

   if (tpos >= bound) {
      num_cand++;
      last_cand = 2;
      pmv[2] = mbs[tpos].mvs[tz];
   } else pmv[2] = (VECTOR){ 0, 0 };

   if (rpos >= bound && rx < mb_width) {
      num_cand++;
      last_cand = 3;
      pmv[3] = mbs[rpos].mvs[rz];
   } else pmv[3] = (VECTOR){ 0, 0 };

   /* If there're more than one candidate, we return the median vector */

   if (num_cand > 1) {
      /* set median */
      pmv[0].x =
         VSMIN(VSMAX(pmv[1].x, pmv[2].x),
               VSMIN(VSMAX(pmv[2].x, pmv[3].x), VSMAX(pmv[1].x, pmv[3].x)));
      pmv[0].y =
         VSMIN(VSMAX(pmv[1].y, pmv[2].y),
               VSMIN(VSMAX(pmv[2].y, pmv[3].y), VSMAX(pmv[1].y, pmv[3].y)));
      return pmv[0];
   }

   return pmv[last_cand];   /* no point calculating median mv */
}


/* check if given vector is equal to any vector checked before */
static inline int
vector_repeats(const VECTOR * const pmv, const unsigned int i)
{
   unsigned int j;
   for (j = 0; j < i; j++)
      if (MVequal(pmv[i], pmv[j])) return 1; /* same vector has been checked already */
   return 0;
}


/*   make a binary mask that prevents diamonds/squares
   from checking a vector which has been checked as a prediction */
static inline int
make_mask(const VECTOR * const pmv, const unsigned int i, const unsigned int current)
{
   unsigned int mask = 255, j;
   for (j = 0; j < i; j++) {
      if (pmv[current].x == pmv[j].x) {
         if (pmv[current].y == pmv[j].y + iDiamondSize) mask &= ~4;
         else if (pmv[current].y == pmv[j].y - iDiamondSize) mask &= ~8;
      } else
         if (pmv[current].y == pmv[j].y) {
            if (pmv[current].x == pmv[j].x + iDiamondSize) mask &= ~1;
            else if (pmv[current].x == pmv[j].x - iDiamondSize) mask &= ~2;
         }
   }
   return mask;
}


static void
xvid_me_DiamondSearch(int x, int y, SearchData * const data,
      int bDirection, CheckFunc * const CheckCandidate)
{

   /* directions: 1 - left (x-1); 2 - right (x+1), 4 - up (y-1); 8 - down (y+1) */

   unsigned int * const iDirection = &data->dir;

   for (;;) {
      *iDirection = 0;
      if (bDirection & 1) CHECK_CANDIDATE(x - iDiamondSize, y, 1);
      if (bDirection & 2) CHECK_CANDIDATE(x + iDiamondSize, y, 2);
      if (bDirection & 4) CHECK_CANDIDATE(x, y - iDiamondSize, 4);
      if (bDirection & 8) CHECK_CANDIDATE(x, y + iDiamondSize, 8);

      if (*iDirection == 0)
         break;

      /* now we're doing diagonal checks near our candidate */
      bDirection = *iDirection;
      x = data->currentMV->x; y = data->currentMV->y;
      if (bDirection & 3) {   /* our candidate is left or right */
         CHECK_CANDIDATE(x, y + iDiamondSize, 8);
         CHECK_CANDIDATE(x, y - iDiamondSize, 4);
      } else {         /* what remains here is up or down */
         CHECK_CANDIDATE(x + iDiamondSize, y, 2);
         CHECK_CANDIDATE(x - iDiamondSize, y, 1);
      }
      bDirection |= *iDirection;
      x = data->currentMV->x; y = data->currentMV->y;
   }
}


static void
xvid_me_SubpelRefine(VECTOR centerMV, SearchData * const data, CheckFunc * const CheckCandidate, int dir)
{
   /* Do a half-pel or q-pel refinement */

   CHECK_CANDIDATE(centerMV.x, centerMV.y - 1, dir);
   CHECK_CANDIDATE(centerMV.x + 1, centerMV.y - 1, dir);
   CHECK_CANDIDATE(centerMV.x + 1, centerMV.y, dir);
   CHECK_CANDIDATE(centerMV.x + 1, centerMV.y + 1, dir);
   CHECK_CANDIDATE(centerMV.x, centerMV.y + 1, dir);
   CHECK_CANDIDATE(centerMV.x - 1, centerMV.y + 1, dir);
   CHECK_CANDIDATE(centerMV.x - 1, centerMV.y, dir);
   CHECK_CANDIDATE(centerMV.x - 1, centerMV.y - 1, dir);
}


static inline void
MEanalyzeMB (   const uint8_t * const pRef,
      const uint8_t * const pCur,
      const int x,
      const int y,
      const MBParam * const pParam,
      MACROBLOCK * const pMBs,
      SearchData * const Data,
      const int fcode)
{

   int i;
   VECTOR pmv[3];
   MACROBLOCK * const pMB = &pMBs[x + y * pParam->mb_width];

   unsigned int simplicity = 0;

   for (i = 0; i < 5; i++) Data->iMinSAD[i] = MV_MAX_ERROR;

   get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y,
         pParam->width, pParam->height, fcode - 1);

   Data->Cur = pCur + (x + y * pParam->edged_width) * 16;
   Data->RefP[0] = pRef + (x + y * pParam->edged_width) * 16;

   pmv[0].x = pMB->mvs[0].x;
   pmv[0].y = pMB->mvs[0].y;

   CheckCandidate32I(pmv[0].x, pmv[0].y, Data, 0);


   if (*Data->iMinSAD > 200) {

      pmv[1].x = pmv[1].y = 0;

      /* median is only used as prediction. it doesn't have to be real */
      if (x == 1 && y == 1) Data->predMV.x = Data->predMV.y = 0;
      else
         if (x == 1) /* left macroblock does not have any vector now */
            Data->predMV = (pMB - pParam->mb_width)->mvs[0]; /* top instead of median */
         else if (y == 1) /* top macroblock doesn't have it's vector */
            Data->predMV = (pMB - 1)->mvs[0]; /* left instead of median */
         else
            Data->predMV = get_pmv2(pMBs, pParam->mb_width, x, y); /* else median */

      pmv[2].x = Data->predMV.x;
      pmv[2].y = Data->predMV.y;

      //if (!vector_repeats(pmv, 1)) // branch never taken
      //   CheckCandidate32I(pmv[1].x, pmv[1].y, Data, 1);
      if (!vector_repeats(pmv, 2))
         CheckCandidate32I(pmv[2].x, pmv[2].y, Data, 2);

      if (*Data->iMinSAD > 500) { /* diamond only if needed */
         unsigned int mask = make_mask(pmv, 3, Data->dir);
         xvid_me_DiamondSearch(Data->currentMV->x, Data->currentMV->y, Data, mask, CheckCandidate32I);
      } else simplicity++;

      if (*Data->iMinSAD > 500) /* refinement from 2-pixel to 1-pixel */
         xvid_me_SubpelRefine(Data->currentMV[0], Data, CheckCandidate32I, 0);
      else simplicity++;
   } else simplicity++;

   for (i = 0; i < 4; i++) {
      MACROBLOCK * MB = &pMBs[x + (i&1) + (y+(i>>1)) * pParam->mb_width];
      MB->mvs[0] = MB->mvs[1] = MB->mvs[2] = MB->mvs[3] = Data->currentMV[i];
      MB->mode = MODE_INTER;
      /* if we skipped some search steps, we have to assume that SAD would be lower with them */
      MB->sad16 = Data->iMinSAD[i+1] - (simplicity<<7);
      if (MB->sad16 < 0) MB->sad16 = 0;
   }
}


/* average deviation from mean */

static uint32_t
dev16_c(const uint8_t * const cur,
      const uint32_t stride)
{

   uint32_t mean = 0;
   uint32_t dev = 0;
   uint32_t i, j;
   uint8_t const *ptr_cur = cur;

   for (j = 0; j < 16; j++) {

      for (i = 0; i < 16; i++)
         mean += *(ptr_cur + i);

      ptr_cur += stride;

   }

   mean /= (16 * 16);
   ptr_cur = cur;

   for (j = 0; j < 16; j++) {

      for (i = 0; i < 16; i++)
         dev += abs(*(ptr_cur + i) - (int32_t) mean);

      ptr_cur += stride;

   }

   return dev;
}


int
MEanalysis(   const uint8_t *pRef,
      const uint8_t *pCurrent,
      const MBParam * const pParam,
      MACROBLOCK * const pMBs,
      const int intraCount,
      const int fcode)
{
   uint32_t x, y, intra = 0;
   int sSAD = 0;
   int IntraThresh = 2000,
       IntraThresh2 = 90;

   int blocks = 10;
   int complexity = 0;

   SearchData Data;
   Data.iEdgedWidth = pParam->edged_width;

   if (intraCount > 0 && intraCount < 30) {
      if (intraCount < 10) {
         IntraThresh += 15 * (10 - intraCount) * (10 - intraCount);
      }
      IntraThresh2 += 4 * (30 - intraCount);
   }

   for (y = 1; y < pParam->mb_height-1; y += 2) {
      for (x = 1; x < pParam->mb_width-1; x += 2) {
         int i;
         blocks += 10;

         pMBs[x + y * pParam->mb_width].mvs[0] = (VECTOR){ 0, 0 };

         MEanalyzeMB(pRef, pCurrent, x, y, pParam, pMBs, &Data, fcode);

         for (i = 0; i < 4; i++) {
            int dev;
            MACROBLOCK *pMB = &pMBs[x+(i&1) + (y+(i>>1)) * pParam->mb_width];
            dev = dev16_c(pCurrent + (x + (i&1) + (y + (i>>1)) * pParam->edged_width) * 16,
                  pParam->edged_width);

            complexity += VSMAX(dev, 300);
            if (dev + IntraThresh < pMB->sad16) {
               pMB->mode = MODE_INTRA;
               if (++intra > ((pParam->mb_height-2)*(pParam->mb_width-2))/2)
                  return 1;
            }

            if (pMB->mvs[0].x == 0 && pMB->mvs[0].y == 0)
               if (dev > 1000 && pMB->sad16 < 1000)
                  sSAD += 512;

            sSAD += (dev < 3000) ? pMB->sad16 : pMB->sad16/2; /* blocks with big contrast differences usually have large SAD - while they look very good in b-frames */
         }
      }
   }
   complexity >>= 7;

   sSAD /= complexity + 4*blocks;

   return sSAD > IntraThresh2;
}
