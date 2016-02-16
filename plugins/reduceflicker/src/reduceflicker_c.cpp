/*****************************************************************************
 * reduceflicker_c.cpp
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

#include <algorithm>

#define ISSE 0
#include "reduceflicker_sse.h"

#define PRECISECV 1

void reduceflicker1( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height )
{
    int width = (hblocks + 1) * SSE_INCREMENT + remainder;
    dpitch  -= width;
    ppitch1 -= width;
    ppitch2 -= width;
    spitch  -= width;
    npitch  -= width;

    do
    {
        int i = width;
        do
        {
            int d = abs(prev2[0] - src[0]);
#if PRECISECV
            unsigned int avg = (prev1[0] + next[0] + 1)/2 - 1;
            if( (int) avg < 0 ) avg = 0;
            avg = (avg + src[0] + 1) / 2;
#else
            unsigned int avg = ((prev1[0] + next1[0] + 1)/2 + src[0])/2;
#endif
            unsigned int ul = std::max( std::min( prev1[0], next[0] ) - d, (int)src[0] );
            unsigned int ll = std::min( std::max( prev1[0], next[0] ) + d, (int)src[0] );
            dest[0] = std::min( ul, std::max( ll, avg ) );
            ++dest;
            ++prev2;
            ++prev1;
            ++src;
            ++next;
        } while( --i );
        dest  += dpitch;
        prev2 += ppitch2;
        prev1 += ppitch1;
        src   += spitch;
        next  += npitch;
    } while( --height );
}

void reduceflicker1_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next, int npitch, int hblocks, int remainder, int incpitch, int height )
{
    int width = (hblocks + 1) * SSE_INCREMENT + remainder;
    dpitch  -= width;
    ppitch1 -= width;
    ppitch2 -= width;
    spitch  -= width;
    npitch  -= width;

    do
    {
        int i = width;
        do
        {
            int d1 = prev2[0] - src[0];
            int d2 = 0;
            if( d1 < 0 )
            {
                d2 = -d1;
                d1 = 0;
            }
#if PRECISECV
            unsigned int avg = (prev1[0] + next[0] + 1)/2 - 1;
            if( (int) avg < 0 ) avg = 0;
            avg = (avg + src[0] + 1) / 2;
#else
            unsigned int avg = ((prev1[0] + next1[0] + 1)/2 + src[0])/2;
#endif
            unsigned int ul = std::max( std::min( prev1[0], next[0] ) - d1, (int)src[0] );
            unsigned int ll = std::min( std::max( prev1[0], next[0] ) + d2, (int)src[0] );
            dest[0] = std::min( ul, std::max( ll, avg ) );
            ++dest;
            ++prev2;
            ++prev1;
            ++src;
            ++next;
        } while( --i );
        dest  += dpitch;
        prev2 += ppitch2;
        prev1 += ppitch1;
        src   += spitch;
        next  += npitch;
    } while( --height );
}

void reduceflicker2( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height )
{
    int width = (hblocks + 1) * SSE_INCREMENT + remainder;
    dpitch  -= width;
    ppitch1 -= width;
    ppitch2 -= width;
    spitch  -= width;
    npitch1 -= width;
    npitch2 -= width;

    do
    {
        int i = width;
        do
        {
            int d  = abs(prev2[0] - src[0]);
            int d2 = abs(next2[0] - src[0]);
            if( d2 < d ) d = d2;
#if PRECISECV
            unsigned int avg = (prev1[0] + next1[0] + 1)/2 - 1;
            if( (int) avg < 0 ) avg = 0;
            avg = (avg + src[0] + 1) / 2;
#else
            unsigned int avg = ((prev1[0] + next1[0] + 1)/2 + src[0])/2;
#endif
            unsigned int ul = std::max( std::min( prev1[0], next1[0] ) - d, (int)src[0] );
            unsigned int ll = std::min( std::max( prev1[0], next1[0] ) + d, (int)src[0] );
            dest[0] = (uint8_t)std::min( ul, std::max( ll, avg ) );
            ++dest;
            ++prev2;
            ++prev1;
            ++src;
            ++next1;
            ++next2;
        } while( --i );
        dest += dpitch;
        prev2 += ppitch2;
        prev1 += ppitch1;
        src += spitch;
        next1 += npitch1;
        next2 += npitch2;
    } while( --height );
}

void reduceflicker2_a( uint8_t *dest, int dpitch, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch, const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, int hblocks, int remainder, int incpitch, int height )
{
    int width = (hblocks + 1) * SSE_INCREMENT + remainder;
    dpitch  -= width;
    ppitch1 -= width;
    ppitch2 -= width;
    spitch  -= width;
    npitch1 -= width;
    npitch2 -= width;

    do
    {
        int i = width;
        do
        {
            int d1 = prev2[0] - src[0];
            int d2 = 0;
            if( d1 < 0 )
            {
                d2 = -d1;
                d1 = 0;
            }
            int d3 = next2[0] - src[0];
            if( d3 >= 0 )
            {
                d2 = 0;
                if( d3 <= d1 ) d1 = d3;
            }
            else
            {
                d1 = 0;
                if( d3 + d2 > 0 ) d2 = -d3;
            }
#if PRECISECV
            unsigned int avg = (prev1[0] + next1[0] + 1)/2 - 1;
            if( (int) avg < 0 ) avg = 0;
            avg = (avg + src[0] + 1) / 2;
#else
            unsigned int avg = ((prev1[0] + next1[0] + 1)/2 + src[0])/2;
#endif
            unsigned int ul = std::max( std::min( prev1[0], next1[0] ) - d1, (int)src[0] );
            unsigned int ll = std::min( std::max( prev1[0], next1[0] ) + d2, (int)src[0] );
            dest[0] = (uint8_t)std::min( ul, std::max( ll, avg ) );
            ++dest;
            ++prev2;
            ++prev1;
            ++src;
            ++next1;
            ++next2;
        } while( --i );
        dest  += dpitch;
        prev2 += ppitch2;
        prev1 += ppitch1;
        src   += spitch;
        next1 += npitch1;
        next2 += npitch2;
    } while( --height );
}

void reduceflicker3( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                     const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height )
{
    int width = (hblocks + 1) * SSE_INCREMENT + remainder;
    dpitch  -= width;
    ppitch1 -= width;
    ppitch2 -= width;
    ppitch3 -= width;
    spitch  -= width;
    npitch1 -= width;
    npitch2 -= width;
    npitch3 -= width;

    do
    {
        int i = width;
        do
        {
            int d  = abs(prev2[0] - src[0]);
            int d2 = abs(next2[0] - src[0]);
            if( d2 < d ) d = d2;
            d2 = abs(prev3[0] - src[0]);
            if( d2 < d ) d = d2;
            d2 = abs(next3[0] - src[0]);
            if( d2 < d ) d = d2;
#if PRECISECV
            unsigned int avg = (prev1[0] + next1[0] + 1)/2 - 1;
            if( (int) avg < 0 ) avg = 0;
            avg = (avg + src[0] + 1) / 2;
#else
            unsigned int avg = ((prev1[0] + next1[0] + 1)/2 + src[0])/2;
#endif
            unsigned int ul = std::max( std::min( prev1[0], next1[0] ) - d, (int)src[0] );
            unsigned int ll = std::min( std::max( prev1[0], next1[0] ) + d, (int)src[0] );
            dest[0] = (uint8_t)std::min( ul, std::max( ll, avg ) );
            ++dest;
            ++prev3;
            ++prev2;
            ++prev1;
            ++src;
            ++next1;
            ++next2;
            ++next3;
        } while( --i );
        dest  += dpitch;
        prev3 += ppitch3;
        prev2 += ppitch2;
        prev1 += ppitch1;
        src   += spitch;
        next1 += npitch1;
        next2 += npitch2;
        next3 += npitch3;
    } while( --height );
}

void reduceflicker3_a( uint8_t *dest, int dpitch, const uint8_t *prev3, int ppitch3, const uint8_t *prev2, int ppitch2, const uint8_t *prev1, int ppitch1, const uint8_t *src, int spitch,
                       const uint8_t *next1, int npitch1, const uint8_t *next2, int npitch2, const uint8_t *next3, int npitch3, int hblocks, int remainder, int incpitch, int height )
{
    int width = (hblocks + 1) * SSE_INCREMENT + remainder;
    dpitch  -= width;
    ppitch1 -= width;
    ppitch2 -= width;
    ppitch3 -= width;
    spitch  -= width;
    npitch1 -= width;
    npitch2 -= width;
    npitch3 -= width;

    do
    {
        int i = width;
        do
        {
            int d1 = prev2[0] - src[0];
            int d2 = 0;
            if( d1 < 0 )
            {
                d2 = -d1;
                d1 = 0;
            }
            int d3 = next2[0] - src[0];
            if( d3 >= 0 )
            {
                d2 = 0;
                if( d3 <= d1 ) d1 = d3;
            }
            else
            {
                d1 = 0;
                if( d3 + d2 > 0 ) d2 = -d3;
            }
            d3 = prev3[0] - src[0];
            if( d3 >= 0 )
            {
                d2 = 0;
                if( d3 <= d1 ) d1 = d3;
            }
            else
            {
                d1 = 0;
                if( d3 + d2 > 0 ) d2 = -d3;
            }
            d3 = next3[0] - src[0];
            if( d3 >= 0 )
            {
                d2 = 0;
                if( d3 <= d1 ) d1 = d3;
            }
            else
            {
                d1 = 0;
                if( d3 + d2 > 0 ) d2 = -d3;
            }
#if PRECISECV
            unsigned int avg = (prev1[0] + next1[0] + 1)/2 - 1;
            if( (int) avg < 0 ) avg = 0;
            avg = (avg + src[0] + 1) / 2;
#else
            unsigned int avg = ((prev1[0] + next1[0] + 1)/2 + src[0])/2;
#endif
            unsigned int ul = std::max( std::min( prev1[0], next1[0] ) - d1, (int)src[0] );
            unsigned int ll = std::min( std::max( prev1[0], next1[0] ) + d2, (int)src[0] );
            dest[0] = (uint8_t) std::min( ul, std::max( ll, avg ) );
            ++dest;
            ++prev3;
            ++prev2;
            ++prev1;
            ++src;
            ++next1;
            ++next2;
            ++next3;
        } while( --i );
        dest  += dpitch;
        prev3 += ppitch3;
        prev2 += ppitch2;
        prev1 += ppitch1;
        src   += spitch;
        next1 += npitch1;
        next2 += npitch2;
        next3 += npitch3;
    } while( --height );
}
