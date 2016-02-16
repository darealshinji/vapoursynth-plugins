/*****************************************************************************
 * ReduceFlicker.h
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

#include <string>

class CustomException
{
private:
    const std::string name;
public:
    CustomException() : name( std::string() ) {}
    CustomException( const std::string name ) : name( name ) {}
    const char * what() const noexcept { return name.c_str(); }
};

class ReduceFlicker
{
protected:
    int planes;
    int lastframe;
    int width [3];
    int height[3];
    int hblocks  [3];
    int remainder[3];
    int incpitch [3];

public:
    VSVideoInfo vi;
    VSNodeRef  *node;

    using bad_param = class bad_param : public CustomException { using CustomException::CustomException; };

    virtual void RequestFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi ) = 0;
    virtual const VSFrameRef * GetFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi ) = 0;

    ReduceFlicker( const bool grey, const int blocksize, const VSMap *in, const VSAPI *vsapi );
    virtual ~ReduceFlicker();
};

class ReduceFlicker1 : public ReduceFlicker
{
private:
    void (*func)
    (
              uint8_t *dest,  int dpitch,
        const uint8_t *prev2, int ppitch2,
        const uint8_t *prev1, int ppitch1,
        const uint8_t *src,   int spitch,
        const uint8_t *next,  int npitch,
        int hblocks, int remainder, int incpitch, int height
    );

    void RequestFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    const VSFrameRef * GetFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );

public:
    ReduceFlicker1( const bool aggressive, const bool grey, const VSMap *in, const VSAPI *vsapi );
    ~ReduceFlicker1();
};

class ReduceFlicker2 : public ReduceFlicker
{
private:
    void (*func)
    (
              uint8_t *dest,  int dpitch,
        const uint8_t *prev2, int ppitch2,
        const uint8_t *prev1, int ppitch1,
        const uint8_t *src,   int spitch,
        const uint8_t *next1, int npitch1,
        const uint8_t *next2, int npitch2,
        int hblocks, int remainder, int incpitch, int height
    );

    void RequestFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    const VSFrameRef * GetFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );

public:
    ReduceFlicker2( const bool aggressive, const bool grey, const VSMap *in, const VSAPI *vsapi );
    ~ReduceFlicker2();
};

class ReduceFlicker3 : public ReduceFlicker
{
private:
    void (*func)
    (
              uint8_t *dest,  int dpitch,
        const uint8_t *prev3, int ppitch3,
        const uint8_t *prev2, int ppitch2,
        const uint8_t *prev1, int ppitch1,
        const uint8_t *src,   int spitch,
        const uint8_t *next1, int npitch1,
        const uint8_t *next2, int npitch2,
        const uint8_t *next3, int npitch3,
        int hblocks, int remainder, int incpitch, int height
    );

    void RequestFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );
    const VSFrameRef * GetFrame( int n, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi );

public:
    ReduceFlicker3( const bool aggressive, const bool grey, const VSMap *in, const VSAPI *vsapi );
    ~ReduceFlicker3();
};
