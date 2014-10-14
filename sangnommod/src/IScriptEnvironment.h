/*
IScriptEnvironment Compatibility Layer  Copyright(C) 2014 msg7086

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

#pragma once
typedef unsigned char       BYTE;
#define PARAM_INT(name, def) int name = int64ToIntS(vsapi->propGetInt(in, #name, 0, &err)); if (err) { name = def; }

enum {PLANAR_Y, PLANAR_U, PLANAR_V};

class IScriptEnvironment
{
public:
	VSFrameContext *frameCtx;
	VSCore *core;
	const VSAPI *vsapi;
	VSNodeRef *node;
	const VSVideoInfo *vi;

	IScriptEnvironment(VSFrameContext *_frameCtx, VSCore *_core, const VSAPI *_vsapi, VSNodeRef *_node)
		: frameCtx(_frameCtx), core(_core), vsapi(_vsapi), node(_node) {
		vi = vsapi->getVideoInfo(node);
	}
	~IScriptEnvironment() { }
	VSFrameRef *NewVideoFrame(const VSVideoInfo * vi) {
		return vsapi->newVideoFrame(vi->format, vi->width, vi->height, nullptr, core);
	}
	const VSFrameRef *GetFrame(int n) {
		return vsapi->getFrame(n, node, nullptr, 0);
	}
	void FreeFrame(const VSFrameRef* source) {
		vsapi->freeFrame(source);
	}

	VSFrameRef * MakeWritable(const VSFrameRef* source) {
		return vsapi->copyFrame(source, core);
	}
	void BitBlt(void* dstp, int dst_pitch, const void* srcp, int src_pitch, int row_size, int height) {
		return vs_bitblt(dstp, dst_pitch, srcp, src_pitch, row_size, height);
	}
	void PrefetchFrame(int n) {
		vsapi->requestFrameFilter(n, node, frameCtx);
	}
	int GetRowSize(const VSFrameRef* frame, int plane = PLANAR_Y) { return plane == PLANAR_Y ? vi->width : (vi->width >> vi->format->subSamplingW); } // FIXME: row-size = width * sizeof(size_t)
	int GetHeight(const VSFrameRef* frame, int plane = PLANAR_Y) { return plane == PLANAR_Y ? vi->height : (vi->height >> vi->format->subSamplingH); }
	int GetPitch(const VSFrameRef* frame, int plane = PLANAR_Y) { return vsapi->getStride(frame, plane); }
	BYTE* GetWritePtr(VSFrameRef* frame, int plane = PLANAR_Y) { return vsapi->getWritePtr(frame, plane); }
	const BYTE* GetReadPtr(const VSFrameRef* frame, int plane = PLANAR_Y) { return vsapi->getReadPtr(frame, plane); }
};
