/*
VS_DELOGO Copyright(C) 2003 MakKi, 2014-2015 msg7086

This program is free software; you can redistribute it and / or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
*/

#ifndef __ISCRIPTENVIRONMENT_H
#define __ISCRIPTENVIRONMENT_H

#define BYTE unsigned char

enum { PLANAR_Y,
    PLANAR_U,
    PLANAR_V };

class IScriptEnvironment {
public:
    VSFrameContext* frameCtx;
    VSCore* core;
    const VSAPI* vsapi;
    VSNodeRef* node;
    const VSVideoInfo* vi;

    IScriptEnvironment(VSFrameContext* _frameCtx, VSCore* _core, const VSAPI* _vsapi, VSNodeRef* _node)
        : frameCtx(_frameCtx)
        , core(_core)
        , vsapi(_vsapi)
        , node(_node)
    {
        vi = vsapi->getVideoInfo(node);
    }
    ~IScriptEnvironment() {}
    inline VSFrameRef* NewVideoFrame(const VSVideoInfo* vi)
    {
        return vsapi->newVideoFrame(vi->format, vi->width, vi->height, nullptr, core);
    }
    inline const VSFrameRef* GetFrame(int n)
    {
        return vsapi->getFrame(n, node, nullptr, 0);
    }
    inline void FreeFrame(const VSFrameRef* source)
    {
        vsapi->freeFrame(source);
    }
    inline VSFrameRef* MakeWritable(const VSFrameRef* source)
    {
        return vsapi->copyFrame(source, core);
    }
    inline void BitBlt(void* dstp, int dst_pitch, const void* srcp, int src_pitch, int row_size, int height)
    {
        return vs_bitblt(dstp, dst_pitch, srcp, src_pitch, row_size, height);
    }
    inline void PrefetchFrame(int n)
    {
        vsapi->requestFrameFilter(n, node, frameCtx);
    }
    inline int GetRowSize(const VSFrameRef* frame) { return vi->width * vi->format->bytesPerSample; }
    inline int GetWidth(const VSFrameRef* frame) { return vi->width; }
    inline int GetHeight(const VSFrameRef* frame) { return vi->height; }
    inline int GetPitch(const VSFrameRef* frame, int plane = PLANAR_Y) { return vsapi->getStride(frame, plane); }
    inline BYTE* GetWritePtr(VSFrameRef* frame, int plane = PLANAR_Y) { return vsapi->getWritePtr(frame, plane); }
};

#endif
