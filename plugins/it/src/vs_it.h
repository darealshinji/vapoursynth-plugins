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

const int MAX_WIDTH = 8192;

class IT {
	int m_iFPS;
	int m_iThreshold;
	int m_iPThreshold;
	int m_iCounter;

	int width;
	int height;
	int m_iMaxFrames;

	CFrameInfo *m_frameInfo;
	CTFblockInfo *m_blockInfo;

public:
	const VSVideoInfo* vi;
	VSNodeRef * node;
	IT(VSVideoInfo * vi,
		VSNodeRef * node,
		int _fps,
		int _threshold,
		int _pthreshold,
		const VSAPI *vsapi);
	~IT() {
		delete[] m_frameInfo;
		delete[] m_blockInfo;
	}
	int AdjPara(int v) {
		return (((v * width) / 720) * height) / 480;
	}
    __forceinline int clipFrame(int n) {
		return VSMAX(0, VSMIN(n, m_iMaxFrames - 1));
	}
    __forceinline int clipX(int x) {
		return VSMAX(0, VSMIN(width - 1, x));
	}
    __forceinline int clipY(int y) {
        return VSMAX(0, VSMIN(height - 1, y));
	}
    __forceinline int clipYH(int y) {
        return VSMAX(0, VSMIN((height >> 1) - 1, y));
	}
	__forceinline unsigned char* B2YP(unsigned char *dst, int y) {
		y = VSMAX(0, VSMIN(height - 1, y));
		return dst + y * width * 2;
	}
    __forceinline unsigned char* BYP(unsigned char *dst, int y) {
        y = VSMAX(0, VSMIN(height - 1, y));
		return dst + y * width;
	}
    void VS_CC GetFramePre(IScriptEnvironment* env, int n);
    const VSFrameRef* VS_CC GetFrame(IScriptEnvironment* env, int n);
    const VSFrameRef* VS_CC MakeOutput(IScriptEnvironment* env, VSFrameRef* dst, int n);
	bool VS_CC CheckSceneChange(IScriptEnvironment* env, int n);
	void VS_CC GetFrameSub(IScriptEnvironment* env, int n);
	
	void VS_CC EvalIV_YV12(IScriptEnvironment* env, int n, const VSFrameRef * ref, long &counter, long &counterp);
	void VS_CC MakeDEmap_YV12(IScriptEnvironment* env, const VSFrameRef * ref, int offset);
	void VS_CC MakeMotionMap_YV12(IScriptEnvironment* env, int fno, bool flag);
	void VS_CC MakeMotionMap2Max_YV12(IScriptEnvironment* env, int fno);
	void VS_CC MakeSimpleBlurMap_YV12(IScriptEnvironment* env, int fno);

    void VS_CC CopyCPNField(IScriptEnvironment* env, VSFrameRef * dst, int n);
    void VS_CC DeintOneField_YV12(IScriptEnvironment* env, VSFrameRef * dst, int n);
    void VS_CC ChooseBest(IScriptEnvironment* env, int n);
    bool VS_CC CompCP(IScriptEnvironment* env);
    void VS_CC Decide(IScriptEnvironment* env, int n);
    void VS_CC SetFT(IScriptEnvironment* env, int base, int n, char c);
    bool VS_CC DrawPrevFrame(IScriptEnvironment* env, VSFrameRef * dst, int n);
	// Unported method signature below.
	// void VS_CC MakeMotionMap2_YV12(IScriptEnvironment* env, int fno, bool flag);
	// void VS_CC Deinterlace_YV12(IScriptEnvironment* env, VSFrameRef * dst, int n, int nParameterMode = DI_MODE_DEINTERLACE);
	// void VS_CC SimpleBlur_YV12(IScriptEnvironment* env, VSFrameRef * dst, int n);
	// void VS_CC BlendFrame_YV12(IScriptEnvironment* env, VSFrameRef * dst, int base, int n);
	// void VS_CC ShowInterlaceArea(const VSAPI * vsapi, VSFrameRef * dst, int n);
	// void VS_CC ShowDifference();
	// bool VS_CC CompCN();
	// void VS_CC ReadLog();
	// void VS_CC WriteLog();
};
