/*
VS_IT Copyright(C) 2002 thejam79, 2003 minamina, 2014 msg7086

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
#include "vs_it_interface.h"

struct CFrameInfo {
	char pos;
	char match;
	char matchAcc;
	char ip;
	char out;
	char mflag;
	int diffP0;
	int diffP1;
	int diffS0;
	int diffS1;
	long ivC, ivP, ivN, ivM;
	long ivPC, ivPP, ivPN;
};

struct CTFblockInfo {
	int cfi;
	char level;
	char itype;
};

class IScriptEnvironment
{
public:
	int m_iRealFrame;
	unsigned char *m_edgeMap, *m_motionMap4DI, *m_motionMap4DIMax;

	long m_iSumC, m_iSumP, m_iSumN, m_iSumM;
	long m_iSumPC, m_iSumPP, m_iSumPN, m_iSumPM;
	int m_iCurrentFrame;
	bool m_bRefP;
	int m_iUsePrev, m_iUseNext;
	int m_iUseFrame;

	VSFrameContext *frameCtx;
	VSCore *core;
	const VSAPI *vsapi;
	VSNodeRef *node;
	const VSVideoInfo *vi;
	IScriptEnvironment(VSFrameContext *_frameCtx, VSCore *_core, const VSAPI *_vsapi, VSNodeRef *_node)
		: frameCtx(_frameCtx), core(_core), vsapi(_vsapi), node(_node) {
		vi = vsapi->getVideoInfo(node);
		m_iSumC = m_iSumP = m_iSumN = 0;
		m_iUsePrev = m_iUseNext = 0;
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
    __forceinline const unsigned char* SYP(const VSFrameRef * pv, int y, int plane = 0) {
		y = VSMAX(0, VSMIN(vi->height - 1, y));
		auto rPtr = vsapi->getReadPtr(pv, plane);
		auto rStr = vsapi->getStride(pv, plane);
		return rPtr + (plane == 0 ? y : (y >> 2 << 1) + y % 2) * rStr;
	}
    __forceinline unsigned char* DYP(VSFrameRef * pv, int y, int plane = 0) {
		y = VSMAX(0, VSMIN(vi->height - 1, y));
		auto wPtr = vsapi->getWritePtr(pv, plane);
		auto wStr = vsapi->getStride(pv, plane);
		return wPtr + (plane == 0 ? y : (y >> 2 << 1) + y % 2) * wStr;
	}
};
