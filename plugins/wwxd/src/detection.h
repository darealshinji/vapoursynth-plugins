#include <stdint.h>


typedef struct {
   int x, y;
} VECTOR;


typedef struct {
   VECTOR mvs[4];
   int32_t sad16;
   int mode;
} MACROBLOCK;


typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t edged_width;
	uint32_t edged_height;
	uint32_t mb_width;
	uint32_t mb_height;
   int edge_size;
} MBParam;


typedef struct {
   int min_dx, max_dx, min_dy, max_dy;
   int32_t iMinSAD[5];
   VECTOR currentMV[5];
   int temp[4];
   unsigned int dir;
   VECTOR predMV;
   const uint8_t *RefP[1], *Cur;
   uint32_t iEdgedWidth;
} SearchData;


int MEanalysis(	const uint8_t *pRef,
            const uint8_t *pCurrent,
			const MBParam * const pParam,
         MACROBLOCK * const pMBs,
         const int intraCount,
         const int fcode);
