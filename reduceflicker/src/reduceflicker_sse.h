/*****************************************************************************
 * reduceflicker_sse.h
 *****************************************************************************
 * An VapourSynth plugin for reducing flicker
 *
 * Copyright (C) 2005 Rainer Wittmann <gorw@gmx.de>
 * Copyright (C) 2015 Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * To get a copy of the GNU General Public License write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
 * http://www.gnu.org/copyleft/gpl.html .
 *****************************************************************************/

#include <cstdint>

#if ISSE > 1
    #define SSE_INCREMENT 16
    #define SSE_SHIFT     4
    #define SSE_MOVE      movdqu
    #if ISSE > 2
        #define SSE3_MOVE lddqu
    #else
        #define SSE3_MOVE movdqu
    #endif
    #define SSE_RMOVE     movdqa
    #define SSE0          xmm0
    #define SSE1          xmm1
    #define SSE2          xmm2
    #define SSE3          xmm3
    #define SSE4          xmm4
    #define SSE5          xmm5
    #define SSE6          xmm6
    #define SSE7          xmm7
    #define SHIFTLEFT( reg, amount )  \
    __asm    pslldq    reg, amount
    #define SHIFTRIGHT( reg, amount ) \
    __asm    psrldq    reg, amount
    #define SSE_EMMS
#else
    #define SSE_INCREMENT 8
    #define SSE_SHIFT     3
    #define SSE_MOVE      movq
    #define SSE3_MOVE     movq
    #define SSE_RMOVE     movq
    #define SSE0          mm0
    #define SSE1          mm1
    #define SSE2          mm2
    #define SSE3          mm3
    #define SSE4          mm4
    #define SSE5          mm5
    #define SSE6          mm6
    #define SSE7          mm7
    #define SHIFTLEFT( reg, amount ) \
    __asm    psllq     reg, 8*(amount)
    #define SHIFTRIGHT( reg, amount ) \
    __asm    psrlq     reg, 8*(amount)
    #if ISSE == 1
        #define SSE_EMMS  __asm    emms
    #else
        #define SSE_EMMS
    #endif
#endif  /* ISSE > 1 */

#if ISSE > 0
alignas(SSE_INCREMENT) static constexpr uint8_t bias1[SSE_INCREMENT] =
{
    1, 1, 1, 1, 1, 1, 1, 1
#if SSE_INCREMENT == 16
  , 1, 1, 1, 1, 1, 1, 1, 1
#endif
};

#define REDFLICKER1( dest, prev2, prev1, src, next, bias, reg1, reg2, reg3, reg4, reg5, reg6, mread, mwrite ) \
__asm    mread        reg1,                src     \
__asm    mread        reg2,                prev2   \
__asm    SSE_RMOVE    reg3,                reg1    \
__asm    mread        reg5,                prev1   \
__asm    psubusb      reg3,                reg2    \
__asm    mread        reg4,                next    \
__asm    psubusb      reg2,                reg1    \
__asm    SSE_RMOVE    reg6,                reg5    \
__asm    paddb        reg2,                reg3    \
__asm    pminub       reg5,                reg4    \
__asm    pmaxub       reg6,                reg4    \
__asm    SSE_RMOVE    reg3,                reg5    \
__asm    psubusb      reg5,                reg2    \
__asm    pavgb        reg3,                reg6    \
__asm    paddusb      reg6,                reg2    \
__asm    psubusb      reg3,                bias    \
__asm    pmaxub       reg5,                reg1    \
__asm    pavgb        reg3,                reg1    \
__asm    pminub       reg6,                reg1    \
__asm    pminub       reg3,                reg5    \
__asm    pmaxub       reg3,                reg6    \
__asm    mwrite       dest,                reg3

static inline void areduceflicker1( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                dest
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch,             eax
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            eax,                next
    __asm    mov            ecx,                hblocks
    __asm    align          16
    __asm    _loop:
            REDFLICKER1([edi], [edx], [ebx], [esi], [eax], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE_RMOVE, SSE_RMOVE)
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
    __asm    add            eax,                SSE_INCREMENT
    __asm    loop           _loop
    __asm    add            edi,                remainder
    __asm    add            edx,                remainder
    __asm    add            ebx,                remainder
    __asm    add            esi,                remainder
    __asm    add            eax,                remainder
            REDFLICKER1([edi], [edx], [ebx], [esi], [eax], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE_MOVE, SSE_MOVE)
    __asm    add            edi,                dpitch
    __asm    add            edx,                ppitch2
    __asm    add            ebx,                ppitch1
    __asm    add            esi,                spitch
    __asm    add            eax,                npitch
    __asm    dec            height
    __asm    mov            ecx,                hblocks
    __asm    jnz            _loop
}

static inline void ureduceflicker1( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                dest
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch,             eax
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            eax,                next
    __asm    mov            ecx,                hblocks
    __asm    align          16
    __asm    _loop:
            REDFLICKER1([edi], [edx], [ebx], [esi], [eax], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE3_MOVE, SSE_RMOVE)
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
    __asm    add            eax,                SSE_INCREMENT
    __asm    loop           _loop
    __asm    add            edi,                remainder
    __asm    add            edx,                remainder
    __asm    add            ebx,                remainder
    __asm    add            esi,                remainder
    __asm    add            eax,                remainder
            REDFLICKER1([edi], [edx], [ebx], [esi], [eax], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE_MOVE, SSE_MOVE)
    __asm    add            edi,                dpitch
    __asm    add            edx,                ppitch2
    __asm    add            ebx,                ppitch1
    __asm    add            esi,                spitch
    __asm    add            eax,                npitch
    __asm    dec            height
    __asm    mov            ecx,                hblocks
    __asm    jnz            _loop
}

void reduceflicker1( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height )
{
    if( ((unsigned)prev2 & (SSE_INCREMENT - 1))
      + ((unsigned)prev1 & (SSE_INCREMENT - 1))
      + ((unsigned)src   & (SSE_INCREMENT - 1))
      + ((unsigned)next  & (SSE_INCREMENT - 1))
      + (spitch          & (SSE_INCREMENT - 1))
      + (ppitch2         & (SSE_INCREMENT - 1))
      + (ppitch1         & (SSE_INCREMENT - 1))
      + (npitch          & (SSE_INCREMENT - 1))
      == 0 )
        areduceflicker1( dest, dpitch, prev2, ppitch2, prev1, ppitch1, src, spitch, next, npitch, hblocks, remainder, incpitch, height );
    else
        ureduceflicker1( dest, dpitch, prev2, ppitch2, prev1, ppitch1, src, spitch, next, npitch, hblocks, remainder, incpitch, height );
}

#define REDFLICKER1_A( dest, prev2, prev1, src, next, bias, reg1, reg2, reg3, reg4, reg5, reg6, reg7, mread, mwrite ) \
__asm    mread        reg1,                src     \
__asm    mread        reg2,                prev2   \
__asm    SSE_RMOVE    reg3,                reg1    \
__asm    mread        reg5,                prev1   \
__asm    psubusb      reg3,                reg2    \
__asm    SSE_RMOVE    reg6,                reg5    \
__asm    mread        reg4,                next    \
__asm    psubusb      reg2,                reg1    \
__asm    SSE_RMOVE    reg7,                reg5    \
__asm    pminub       reg5,                reg4    \
__asm    pmaxub       reg6,                reg4    \
__asm    psubusb      reg5,                reg2    \
__asm    pavgb        reg4,                reg7    \
__asm    paddusb      reg6,                reg3    \
__asm    psubusb      reg4,                bias    \
__asm    pmaxub       reg5,                reg1    \
__asm    pavgb        reg4,                reg1    \
__asm    pminub       reg6,                reg1    \
__asm    pminub       reg4,                reg5    \
__asm    pmaxub       reg4,                reg6    \
__asm    mwrite       dest,                reg4

static inline void areduceflicker1_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                dest
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch,             eax
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            eax,                next
    __asm    mov            ecx,                hblocks
    __asm    align          16
    __asm    _loop:
            REDFLICKER1_A([edi], [edx], [ebx], [esi], [eax], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_RMOVE, SSE_RMOVE)
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
    __asm    add            eax,                SSE_INCREMENT
    __asm    loop           _loop
    __asm    add            edi,                remainder
    __asm    add            edx,                remainder
    __asm    add            ebx,                remainder
    __asm    add            esi,                remainder
    __asm    add            eax,                remainder
            REDFLICKER1_A([edi], [edx], [ebx], [esi], [eax], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_MOVE, SSE_MOVE)
    __asm    add            edi,                dpitch
    __asm    add            edx,                ppitch2
    __asm    add            ebx,                ppitch1
    __asm    add            esi,                spitch
    __asm    add            eax,                npitch
    __asm    dec            height
    __asm    mov            ecx,                hblocks
    __asm    jnz            _loop
}

static inline void ureduceflicker1_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                dest
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch,             eax
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            eax,                next
    __asm    mov            ecx,                hblocks
    __asm    align          16
    __asm    _loop:
            REDFLICKER1_A([edi], [edx], [ebx], [esi], [eax], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_RMOVE)
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
    __asm    add            eax,                SSE_INCREMENT
    __asm    loop           _loop
    __asm    add            edi,                remainder
    __asm    add            edx,                remainder
    __asm    add            ebx,                remainder
    __asm    add            esi,                remainder
    __asm    add            eax,                remainder
            REDFLICKER1_A([edi], [edx], [ebx], [esi], [eax], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_MOVE, SSE_MOVE)
    __asm    add            edi,                dpitch
    __asm    add            edx,                ppitch2
    __asm    add            ebx,                ppitch1
    __asm    add            esi,                spitch
    __asm    add            eax,                npitch
    __asm    dec            height
    __asm    mov            ecx,                hblocks
    __asm    jnz            _loop
}

void reduceflicker1_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height )
{
    if( ((unsigned)prev2 & (SSE_INCREMENT - 1))
      + ((unsigned)prev1 & (SSE_INCREMENT - 1))
      + ((unsigned)src   & (SSE_INCREMENT - 1))
      + ((unsigned)next  & (SSE_INCREMENT - 1))
      + (spitch          & (SSE_INCREMENT - 1))
      + (ppitch2         & (SSE_INCREMENT - 1))
      + (ppitch1         & (SSE_INCREMENT - 1))
      + (npitch          & (SSE_INCREMENT - 1))
      == 0 )
        areduceflicker1_a( dest, dpitch, prev2, ppitch2, prev1, ppitch1, src, spitch, next, npitch, hblocks, remainder, incpitch, height );
    else
        ureduceflicker1_a( dest, dpitch, prev2, ppitch2, prev1, ppitch1, src, spitch, next, npitch, hblocks, remainder, incpitch, height );
}

#define REDFLICKER2( dest, prev2, prev1, src, next1, next2, bias, reg1, reg2, reg3, reg4, reg5, reg6, reg7, mread, mwrite ) \
__asm    mread        reg1,                src     \
__asm    mread        reg2,                prev2   \
__asm    SSE_RMOVE    reg3,                reg1    \
__asm    SSE_RMOVE    reg6,                reg1    \
__asm    mread        reg7,                next2   \
__asm    psubusb      reg3,                reg2    \
__asm    psubusb      reg6,                reg7    \
__asm    psubusb      reg2,                reg1    \
__asm    mread        reg5,                prev1   \
__asm    psubusb      reg7,                reg1    \
__asm    paddb        reg2,                reg3    \
__asm    paddb        reg7,                reg6    \
__asm    mread        reg4,                next1   \
__asm    SSE_RMOVE    reg6,                reg5    \
__asm    pminub       reg2,                reg7    \
__asm    pminub       reg5,                reg4    \
__asm    pmaxub       reg6,                reg4    \
__asm    SSE_RMOVE    reg3,                reg5    \
__asm    psubusb      reg5,                reg2    \
__asm    pavgb        reg3,                reg6    \
__asm    paddusb      reg6,                reg2    \
__asm    psubusb      reg3,                bias    \
__asm    pmaxub       reg5,                reg1    \
__asm    pavgb        reg3,                reg1    \
__asm    pminub       reg6,                reg1    \
__asm    pminub       reg3,                reg5    \
__asm    pmaxub       reg3,                reg6    \
__asm    mwrite       dest,                reg3

static inline void areduceflicker2( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                dest
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch1,            eax
    __asm    add            npitch2,            eax
    __asm    mov            ecx,                next2
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            eax,                hblocks
    __asm    mov            dest,               eax
    __asm    mov            eax,                next1
    __asm    align          16
    __asm    _loop:
            REDFLICKER2([edi], [edx], [ebx], [esi], [eax], [ecx], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_RMOVE, SSE_RMOVE)
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            eax,                SSE_INCREMENT
    __asm    add            ecx,                SSE_INCREMENT
#ifndef ALTLOOP
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
#endif
    __asm    dec            dest
#ifdef ALTLOOP
    __asm    lea            ebx,                [ebx + SSE_INCREMENT]
    __asm    lea            esi,                [esi + SSE_INCREMENT]
#endif
    __asm    jnz            _loop
    __asm    add            edi,                remainder
    __asm    add            edx,                remainder
    __asm    add            ebx,                remainder
    __asm    add            esi,                remainder
    __asm    add            eax,                remainder
    __asm    add            ecx,                remainder
            REDFLICKER2([edi], [edx], [ebx], [esi], [eax], [ecx], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_MOVE, SSE_MOVE)
    __asm    add            edi,                dpitch
    __asm    add            edx,                ppitch2
    __asm    add            ebx,                ppitch1
    __asm    add            esi,                spitch
    __asm    add            eax,                npitch1
    __asm    add            ecx,                npitch2
    __asm    dec            height
    __asm    push           hblocks
    __asm    pop            dest
    __asm    jnz            _loop
}

static inline void ureduceflicker2( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                dest
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch1,            eax
    __asm    add            npitch2,            eax
    __asm    mov            ecx,                next2
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            eax,                hblocks
    __asm    mov            dest,               eax
    __asm    mov            eax,                next1
    __asm    align          16
    __asm    _loop:
            REDFLICKER2([edi], [edx], [ebx], [esi], [eax], [ecx], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_RMOVE)
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            eax,                SSE_INCREMENT
    __asm    add            ecx,                SSE_INCREMENT
#ifndef ALTLOOP
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
#endif
    __asm    dec            dest
#ifdef ALTLOOP
    __asm    lea            ebx,                [ebx + SSE_INCREMENT]
    __asm    lea            esi,                [esi + SSE_INCREMENT]
#endif
    __asm    jnz            _loop
    __asm    add            edi,                remainder
    __asm    add            edx,                remainder
    __asm    add            ebx,                remainder
    __asm    add            esi,                remainder
    __asm    add            eax,                remainder
    __asm    add            ecx,                remainder
            REDFLICKER2([edi], [edx], [ebx], [esi], [eax], [ecx], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_MOVE)
    __asm    add            edi,                dpitch
    __asm    add            edx,                ppitch2
    __asm    add            ebx,                ppitch1
    __asm    add            esi,                spitch
    __asm    add            eax,                npitch1
    __asm    add            ecx,                npitch2
    __asm    dec            height
    __asm    push           hblocks
    __asm    pop            dest
    __asm    jnz            _loop
}

void reduceflicker2( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height )
{
    if( ((unsigned)prev2 & (SSE_INCREMENT - 1))
      + ((unsigned)prev1 & (SSE_INCREMENT - 1))
      + ((unsigned)src   & (SSE_INCREMENT - 1))
      + ((unsigned)next1 & (SSE_INCREMENT - 1))
      + ((unsigned)next2 & (SSE_INCREMENT - 1))
      + (spitch          & (SSE_INCREMENT - 1))
      + (ppitch2         & (SSE_INCREMENT - 1))
      + (ppitch1         & (SSE_INCREMENT - 1))
      + (npitch1         & (SSE_INCREMENT - 1))
      + (npitch2         & (SSE_INCREMENT - 1))
      == 0 )
        areduceflicker2( dest, dpitch, prev2, ppitch2, prev1, ppitch1, src, spitch, next1, npitch1, next2, npitch2, hblocks, remainder, incpitch, height );
    else
        ureduceflicker2( dest, dpitch, prev2, ppitch2, prev1, ppitch1, src, spitch, next1, npitch1, next2, npitch2, hblocks, remainder, incpitch, height );
}

#define REDFLICKER2_A( dest, prev2, prev1, src, next1, next2, bias, reg1, reg2, reg3, reg4, reg5, reg6, reg7, mread, mwrite ) \
__asm    mread        reg1,                src     \
__asm    mread        reg2,                prev2   \
__asm    SSE_RMOVE    reg3,                reg1    \
__asm    mread        reg4,                next2   \
__asm    SSE_RMOVE    reg5,                reg1    \
__asm    psubusb      reg3,                reg2    \
__asm    psubusb      reg5,                reg4    \
__asm    psubusb      reg2,                reg1    \
__asm    psubusb      reg4,                reg1    \
__asm    pminub       reg3,                reg5    \
__asm    mread        reg5,                prev1   \
__asm    pminub       reg2,                reg4    \
__asm    SSE_RMOVE    reg6,                reg5    \
__asm    mread        reg4,                next1   \
__asm    SSE_RMOVE    reg7,                reg6    \
__asm    pminub       reg5,                reg4    \
__asm    pmaxub       reg6,                reg4    \
__asm    psubusb      reg5,                reg2    \
__asm    pavgb        reg4,                reg7    \
__asm    paddusb      reg6,                reg3    \
__asm    psubusb      reg4,                bias    \
__asm    pmaxub       reg5,                reg1    \
__asm    pavgb        reg4,                reg1    \
__asm    pminub       reg6,                reg1    \
__asm    pminub       reg4,                reg5    \
__asm    pmaxub       reg4,                reg6    \
__asm    mwrite       dest,                reg4

static inline void areduceflicker2_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                dest
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch1,            eax
    __asm    add            npitch2,            eax
    __asm    mov            ecx,                next2
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            eax,                hblocks
    __asm    mov            dest,               eax
    __asm    mov            eax,                next1
    __asm    align          16
    __asm    _loop:
            REDFLICKER2_A([edi], [edx], [ebx], [esi], [eax], [ecx], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_RMOVE, SSE_RMOVE)
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            eax,                SSE_INCREMENT
    __asm    add            ecx,                SSE_INCREMENT
#ifndef ALTLOOP
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
#endif
    __asm    dec            dest
#ifdef ALTLOOP
    __asm    lea            ebx,                [ebx + SSE_INCREMENT]
    __asm    lea            esi,                [esi + SSE_INCREMENT]
#endif
    __asm    jnz            _loop
    __asm    add            edi,                remainder
    __asm    add            edx,                remainder
    __asm    add            ebx,                remainder
    __asm    add            esi,                remainder
    __asm    add            eax,                remainder
    __asm    add            ecx,                remainder
            REDFLICKER2_A([edi], [edx], [ebx], [esi], [eax], [ecx], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_MOVE, SSE_MOVE)
    __asm    add            edi,                dpitch
    __asm    add            edx,                ppitch2
    __asm    add            ebx,                ppitch1
    __asm    add            esi,                spitch
    __asm    add            eax,                npitch1
    __asm    add            ecx,                npitch2
    __asm    dec            height
    __asm    push           hblocks
    __asm    pop            dest
    __asm    jnz            _loop
}

static inline void ureduceflicker2_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                dest
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch1,            eax
    __asm    add            npitch2,            eax
    __asm    mov            ecx,                next2
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            eax,                hblocks
    __asm    mov            dest,               eax
    __asm    mov            eax,                next1
    __asm    align          16
    __asm    _loop:
            REDFLICKER2_A([edi], [edx], [ebx], [esi], [eax], [ecx], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_RMOVE)
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            eax,                SSE_INCREMENT
    __asm    add            ecx,                SSE_INCREMENT
#ifndef ALTLOOP
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
#endif
    __asm    dec            dest
#ifdef ALTLOOP
    __asm    lea            ebx,                [ebx + SSE_INCREMENT]
    __asm    lea            esi,                [esi + SSE_INCREMENT]
#endif
    __asm    jnz            _loop
    __asm    add            edi,                remainder
    __asm    add            edx,                remainder
    __asm    add            ebx,                remainder
    __asm    add            esi,                remainder
    __asm    add            eax,                remainder
    __asm    add            ecx,                remainder
            REDFLICKER2_A([edi], [edx], [ebx], [esi], [eax], [ecx], SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_MOVE)
    __asm    add            edi,                dpitch
    __asm    add            edx,                ppitch2
    __asm    add            ebx,                ppitch1
    __asm    add            esi,                spitch
    __asm    add            eax,                npitch1
    __asm    add            ecx,                npitch2
    __asm    dec            height
    __asm    push           hblocks
    __asm    pop            dest
    __asm    jnz            _loop
}

void reduceflicker2_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height )
{
    if( ((unsigned)prev2 & (SSE_INCREMENT - 1))
      + ((unsigned)prev1 & (SSE_INCREMENT - 1))
      + ((unsigned)src   & (SSE_INCREMENT - 1))
      + ((unsigned)next1 & (SSE_INCREMENT - 1))
      + ((unsigned)next2 & (SSE_INCREMENT - 1))
      + (spitch          & (SSE_INCREMENT - 1))
      + (ppitch2         & (SSE_INCREMENT - 1))
      + (ppitch1         & (SSE_INCREMENT - 1))
      + (npitch1         & (SSE_INCREMENT - 1))
      + (npitch2         & (SSE_INCREMENT - 1))
      == 0 )
        areduceflicker2_a( dest, dpitch, prev2, ppitch2, prev1, ppitch1, src, spitch, next1, npitch1, next2, npitch2, hblocks, remainder, incpitch, height );
    else
        ureduceflicker2_a( dest, dpitch, prev2, ppitch2, prev1, ppitch1, src, spitch, next1, npitch1, next2, npitch2, hblocks, remainder, incpitch, height );
}

#define REDFLICKER3( destaddr, prev3addr, prev2, prev1, src, next1addr, next2, next3addr, greg1, bias, reg1, reg2, reg3, reg4, reg5, reg6, reg7, mread, mwrite ) \
__asm    mread        reg1,                src         \
__asm    mread        reg2,                prev2       \
__asm    SSE_RMOVE    reg3,                reg1        \
__asm    mov          greg1,               prev3addr   \
__asm    SSE_RMOVE    reg6,                reg1        \
__asm    mread        reg7,                next2       \
__asm    SSE_RMOVE    reg4,                reg1        \
__asm    psubusb      reg3,                reg2        \
__asm    psubusb      reg6,                reg7        \
__asm    psubusb      reg2,                reg1        \
__asm    mread        reg5,                [greg1]     \
__asm    psubusb      reg7,                reg1        \
__asm    psubusb      reg4,                reg5        \
__asm    paddb        reg2,                reg3        \
__asm    mov          greg1,               next3addr   \
__asm    psubusb      reg5,                reg1        \
__asm    paddb        reg7,                reg6        \
__asm    mread        reg3,                [greg1]     \
__asm    SSE_RMOVE    reg6,                reg1        \
__asm    paddb        reg4,                reg5        \
__asm    mov          greg1,               next1addr   \
__asm    psubusb      reg6,                reg3        \
__asm    mread        reg5,                prev1       \
__asm    pminub       reg2,                reg4        \
__asm    psubusb      reg3,                reg1        \
__asm    mread        reg4,                [greg1]     \
__asm    paddb        reg3,                reg6        \
__asm    pminub       reg2,                reg7        \
__asm    SSE_RMOVE    reg6,                reg5        \
__asm    pminub       reg5,                reg4        \
__asm    pminub       reg2,                reg3        \
__asm    pmaxub       reg6,                reg4        \
__asm    SSE_RMOVE    reg3,                reg5        \
__asm    psubusb      reg5,                reg2        \
__asm    pavgb        reg3,                reg6        \
__asm    paddusb      reg6,                reg2        \
__asm    psubusb      reg3,                bias        \
__asm    pmaxub       reg5,                reg1        \
__asm    mov          greg1,               destaddr    \
__asm    pavgb        reg3,                reg1        \
__asm    pminub       reg6,                reg1        \
__asm    pminub       reg3,                reg5        \
__asm    pmaxub       reg3,                reg6        \
__asm    mwrite       [greg1],             reg3

static inline void areduceflicker3( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                                    const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                next2
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch3,            eax
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch1,            eax
    __asm    add            npitch2,            eax
    __asm    add            npitch3,            eax
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            ecx,                hblocks
    __asm    align          16
    __asm    _loop:
            REDFLICKER3(dest, prev3, [edx], [ebx], [esi], next1, [edi], next3, eax, SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_RMOVE, SSE_RMOVE)
    __asm    add            prev3,              SSE_INCREMENT
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            next3,              SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            dest,               SSE_INCREMENT
    __asm    add            next1,              SSE_INCREMENT
#ifndef ALTLOOP
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
#endif
    __asm    dec            ecx
#ifdef ALTLOOP
    __asm    lea            ebx,                [ebx + SSE_INCREMENT]
    __asm    lea            esi,                [esi + SSE_INCREMENT]
#endif
    __asm    jnz            _loop
    __asm    mov            eax,                remainder
    __asm    add            edi,                eax
    __asm    add            dest,               eax
    __asm    add            edx,                eax
    __asm    add            prev3,              eax
    __asm    add            ebx,                eax
    __asm    add            next3,              eax
    __asm    add            esi,                eax
    __asm    add            next1,              eax
            REDFLICKER3(dest, prev3, [edx], [ebx], [esi], next1, [edi], next3, eax, SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_MOVE, SSE_MOVE)
    __asm    mov            eax,                dpitch
    __asm    add            edi,                npitch2
    __asm    add            dest,               eax
    __asm    mov            eax,                ppitch3
    __asm    add            edx,                ppitch2
    __asm    add            prev3,              eax
    __asm    mov            eax,                npitch1
    __asm    add            ebx,                ppitch1
    __asm    add            next1,              eax
    __asm    mov            eax,                npitch3
    __asm    add            esi,                spitch
    __asm    add            next3,              eax
    __asm    dec            height
    __asm    mov            ecx,                hblocks
    __asm    jnz            _loop
}

static inline void ureduceflicker3( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                                    const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                next2
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch3,            eax
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch1,            eax
    __asm    add            npitch2,            eax
    __asm    add            npitch3,            eax
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            ecx,                hblocks
    __asm    align          16
    __asm    _loop:
            REDFLICKER3(dest, prev3, [edx], [ebx], [esi], next1, [edi], next3, eax, SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_RMOVE)
    __asm    add            prev3,              SSE_INCREMENT
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            next3,              SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            dest,               SSE_INCREMENT
    __asm    add            next1,              SSE_INCREMENT
#ifndef ALTLOOP
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
#endif
    __asm    dec            ecx
#ifdef ALTLOOP
    __asm    lea            ebx,                [ebx + SSE_INCREMENT]
    __asm    lea            esi,                [esi + SSE_INCREMENT]
#endif
    __asm    jnz            _loop
    __asm    mov            eax,                remainder
    __asm    add            edi,                eax
    __asm    add            dest,               eax
    __asm    add            edx,                eax
    __asm    add            prev3,              eax
    __asm    add            ebx,                eax
    __asm    add            next3,              eax
    __asm    add            esi,                eax
    __asm    add            next1,              eax
            REDFLICKER3(dest, prev3, [edx], [ebx], [esi], next1, [edi], next3, eax, SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_MOVE)
    __asm    mov            eax,                dpitch
    __asm    add            edi,                npitch2
    __asm    add            dest,               eax
    __asm    mov            eax,                ppitch3
    __asm    add            edx,                ppitch2
    __asm    add            prev3,              eax
    __asm    mov            eax,                npitch1
    __asm    add            ebx,                ppitch1
    __asm    add            next1,              eax
    __asm    mov            eax,                npitch3
    __asm    add            esi,                spitch
    __asm    add            next3,              eax
    __asm    dec            height
    __asm    mov            ecx,                hblocks
    __asm    jnz            _loop
}

void reduceflicker3( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                     const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height )
{
    if( ((unsigned)prev3 & (SSE_INCREMENT - 1))
      + ((unsigned)prev2 & (SSE_INCREMENT - 1))
      + ((unsigned)prev1 & (SSE_INCREMENT - 1))
      + ((unsigned)src   & (SSE_INCREMENT - 1))
      + ((unsigned)next1 & (SSE_INCREMENT - 1))
      + ((unsigned)next2 & (SSE_INCREMENT - 1))
      + ((unsigned)next3 & (SSE_INCREMENT - 1))
      + (spitch          & (SSE_INCREMENT - 1))
      + (ppitch3         & (SSE_INCREMENT - 1))
      + (ppitch2         & (SSE_INCREMENT - 1))
      + (ppitch1         & (SSE_INCREMENT - 1))
      + (npitch1         & (SSE_INCREMENT - 1))
      + (npitch2         & (SSE_INCREMENT - 1))
      + (npitch3         & (SSE_INCREMENT - 1))
      == 0 )
        areduceflicker3( dest, dpitch, prev3, ppitch3, prev2, ppitch2, prev1, ppitch1, src, spitch, next1, npitch1, next2, npitch2, next3, npitch3, hblocks, remainder, incpitch, height );
    else
        ureduceflicker3( dest, dpitch, prev3, ppitch3, prev2, ppitch2, prev1, ppitch1, src, spitch, next1, npitch1, next2, npitch2, next3, npitch3, hblocks, remainder, incpitch, height );
}

#define REDFLICKER3_A( destaddr, prev3addr, prev2, prev1, src, next1addr, next2, next3addr, greg1, bias, reg1, reg2, reg3, reg4, reg5, reg6, reg7, mread, mwrite ) \
__asm    mread        reg1,                src       \
__asm    mread        reg2,                prev2     \
__asm    SSE_RMOVE    reg3,                reg1      \
__asm    mread        reg4,                next2     \
__asm    SSE_RMOVE    reg5,                reg1      \
__asm    mov          greg1,               prev3addr \
__asm    psubusb      reg3,                reg2      \
__asm    psubusb      reg5,                reg4      \
__asm    psubusb      reg2,                reg1      \
__asm    psubusb      reg4,                reg1      \
__asm    pminub       reg3,                reg5      \
__asm    mread        reg5,                [greg1]   \
__asm    pminub       reg2,                reg4      \
__asm    SSE_RMOVE    reg4,                reg1      \
__asm    mov          greg1,               next3addr \
__asm    psubusb      reg4,                reg5      \
__asm    psubusb      reg5,                reg1      \
__asm    pminub       reg3,                reg4      \
__asm    mread        reg4,                [greg1]   \
__asm    pminub       reg2,                reg5      \
__asm    SSE_RMOVE    reg5,                reg1      \
__asm    mov          greg1,               next1addr \
__asm    psubusb      reg5,                reg4      \
__asm    psubusb      reg4,                reg1      \
__asm    pminub       reg3,                reg5      \
__asm    mread        reg5,                [greg1]   \
__asm    pminub       reg2,                reg4      \
__asm    SSE_RMOVE    reg6,                reg5      \
__asm    mread        reg4,                prev1     \
__asm    SSE_RMOVE    reg7,                reg6      \
__asm    pminub       reg5,                reg4      \
__asm    pmaxub       reg6,                reg4      \
__asm    psubusb      reg5,                reg2      \
__asm    pavgb        reg4,                reg7      \
__asm    paddusb      reg6,                reg3      \
__asm    psubusb      reg4,                bias      \
__asm    pmaxub       reg5,                reg1      \
__asm    pavgb        reg4,                reg1      \
__asm    pminub       reg6,                reg1      \
__asm    pminub       reg4,                reg5      \
__asm    mov          greg1,               destaddr  \
__asm    pmaxub       reg4,                reg6      \
__asm    mwrite       [greg1],             reg4

static inline void areduceflicker3_a( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                                      const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                next2
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch3,            eax
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch1,            eax
    __asm    add            npitch2,            eax
    __asm    add            npitch3,            eax
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            ecx,                hblocks
    __asm    align          16
    __asm    _loop:
            REDFLICKER3_A(dest, prev3, [edx], [ebx], [esi], next1, [edi], next3, eax, SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_RMOVE, SSE_RMOVE)
    __asm    add            prev3,              SSE_INCREMENT
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            next3,              SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            dest,               SSE_INCREMENT
    __asm    add            next1,              SSE_INCREMENT
#ifndef ALTLOOP
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
#endif
    __asm    dec            ecx
#ifdef ALTLOOP
    __asm    lea            ebx,                [ebx + SSE_INCREMENT]
    __asm    lea            esi,                [esi + SSE_INCREMENT]
#endif
    __asm    jnz            _loop
    __asm    mov            eax,                remainder
    __asm    add            edi,                eax
    __asm    add            dest,               eax
    __asm    add            edx,                eax
    __asm    add            prev3,              eax
    __asm    add            ebx,                eax
    __asm    add            next3,              eax
    __asm    add            esi,                eax
    __asm    add            next1,                eax
            REDFLICKER3_A(dest, prev3, [edx], [ebx], [esi], next1, [edi], next3, eax, SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE_MOVE, SSE_MOVE)
    __asm    mov            eax,                dpitch
    __asm    add            edi,                npitch2
    __asm    add            dest,               eax
    __asm    mov            eax,                ppitch3
    __asm    add            edx,                ppitch2
    __asm    add            prev3,              eax
    __asm    mov            eax,                npitch1
    __asm    add            ebx,                ppitch1
    __asm    add            next1,              eax
    __asm    mov            eax,                npitch3
    __asm    add            esi,                spitch
    __asm    add            next3,              eax
    __asm    dec            height
    __asm    mov            ecx,                hblocks
    __asm    jnz            _loop
}

static inline void ureduceflicker3_a( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                                      const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height )
{
    __asm    mov            eax,                incpitch
    __asm    mov            edi,                next2
    __asm    add            dpitch,             eax
    __asm    mov            edx,                prev2
    __asm    add            ppitch3,            eax
    __asm    add            ppitch2,            eax
    __asm    mov            ebx,                prev1
    __asm    add            ppitch1,            eax
    __asm    mov            esi,                src
    __asm    add            spitch,             eax
    __asm    add            npitch1,            eax
    __asm    add            npitch2,            eax
    __asm    add            npitch3,            eax
    __asm    SSE_RMOVE      SSE0,               bias1
    __asm    mov            ecx,                hblocks
    __asm    align          16
    __asm    _loop:
            REDFLICKER3_A(dest, prev3, [edx], [ebx], [esi], next1, [edi], next3, eax, SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_RMOVE)
    __asm    add            prev3,              SSE_INCREMENT
    __asm    add            edi,                SSE_INCREMENT
    __asm    add            next3,              SSE_INCREMENT
    __asm    add            edx,                SSE_INCREMENT
    __asm    add            dest,               SSE_INCREMENT
    __asm    add            next1,              SSE_INCREMENT
#ifndef ALTLOOP
    __asm    add            ebx,                SSE_INCREMENT
    __asm    add            esi,                SSE_INCREMENT
#endif
    __asm    dec            ecx
#ifdef ALTLOOP
    __asm    lea            ebx,                [ebx + SSE_INCREMENT]
    __asm    lea            esi,                [esi + SSE_INCREMENT]
#endif
    __asm    jnz            _loop
    __asm    mov            eax,                remainder
    __asm    add            edi,                eax
    __asm    add            dest,              eax
    __asm    add            edx,                eax
    __asm    add            prev3,              eax
    __asm    add            ebx,                eax
    __asm    add            next3,              eax
    __asm    add            esi,                eax
    __asm    add            next1,              eax
            REDFLICKER3_A(dest, prev3, [edx], [ebx], [esi], next1, [edi], next3, eax, SSE0, SSE1, SSE2, SSE3, SSE4, SSE5, SSE6, SSE7, SSE3_MOVE, SSE_MOVE)
    __asm    mov            eax,                dpitch
    __asm    add            edi,                npitch2
    __asm    add            dest,              eax
    __asm    mov            eax,                ppitch3
    __asm    add            edx,                ppitch2
    __asm    add            prev3,              eax
    __asm    mov            eax,                npitch1
    __asm    add            ebx,                ppitch1
    __asm    add            next1,              eax
    __asm    mov            eax,                npitch3
    __asm    add            esi,                spitch
    __asm    add            next3,              eax
    __asm    dec            height
    __asm    mov            ecx,                hblocks
    __asm    jnz            _loop
}

void reduceflicker3_a( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                       const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height )
{
    if( ((unsigned)prev3 & (SSE_INCREMENT - 1))
      + ((unsigned)prev2 & (SSE_INCREMENT - 1))
      + ((unsigned)prev1 & (SSE_INCREMENT - 1))
      + ((unsigned)src   & (SSE_INCREMENT - 1))
      + ((unsigned)next1 & (SSE_INCREMENT - 1))
      + ((unsigned)next2 & (SSE_INCREMENT - 1))
      + ((unsigned)next3 & (SSE_INCREMENT - 1))
      + (spitch          & (SSE_INCREMENT - 1))
      + (ppitch3         & (SSE_INCREMENT - 1))
      + (ppitch2         & (SSE_INCREMENT - 1))
      + (ppitch1         & (SSE_INCREMENT - 1))
      + (npitch1         & (SSE_INCREMENT - 1))
      + (npitch2         & (SSE_INCREMENT - 1))
      + (npitch3         & (SSE_INCREMENT - 1))
      == 0 )
        areduceflicker3_a( dest, dpitch, prev3, ppitch3, prev2, ppitch2, prev1, ppitch1, src, spitch, next1, npitch1, next2, npitch2, next3, npitch3, hblocks, remainder, incpitch, height );
    else
        ureduceflicker3_a( dest, dpitch, prev3, ppitch3, prev2, ppitch2, prev1, ppitch1, src, spitch, next1, npitch1, next2, npitch2, next3, npitch3, hblocks, remainder, incpitch, height );
}
#endif  /* ISSE > 0 */
