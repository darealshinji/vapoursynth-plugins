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

#include "vs_it.h"

IT::IT(VSVideoInfo * vi, VSNodeRef * node, int _fps, int _threshold, int _pthreshold, const VSAPI * vsapi) :
	m_iFPS(_fps),
	m_iThreshold(_threshold),
	m_iPThreshold(_pthreshold),
	vi(vi),
	node(node) {
	m_iMaxFrames = vi->numFrames;
	m_iCounter = 0;
	width = vi->width;
	height = vi->height;

	m_iPThreshold = AdjPara(m_iPThreshold);

	if (m_iFPS == 24) {
		vi->numFrames = vi->numFrames * 4 / 5;
		vi->fpsNum *= 4;
		if (vi->fpsNum % 5 == 0)
			vi->fpsNum /= 5;
		else
			vi->fpsDen *= 5;
	}

	int i;

	m_frameInfo = new CFrameInfo[m_iMaxFrames + 6];
	for (i = 0; i < m_iMaxFrames + 6; ++i) {
		m_frameInfo[i].match = 'U';
		m_frameInfo[i].matchAcc = 'U';
		m_frameInfo[i].pos = 'U';
		m_frameInfo[i].ip = 'U';
		m_frameInfo[i].mflag = 'U';
		m_frameInfo[i].diffP0 = -1;
		m_frameInfo[i].diffP1 = -1;
	}
	m_blockInfo = new CTFblockInfo[m_iMaxFrames / 5 + 6];
	for (i = 0; i < m_iMaxFrames / 5 + 1; ++i) {
		m_blockInfo[i].level = 'U';
		m_blockInfo[i].itype = 'U';
	}
}

void IT::GetFramePre(IScriptEnvironment * env, int n) {
	if (m_iFPS == 24) {
		int base = n + n / 4;
		base = (base / 5) * 5;
		int i;
		for (i = 0; i < 6; ++i)
			env->vsapi->requestFrameFilter(base + i, node, env->frameCtx);
	}
	else
		env->vsapi->requestFrameFilter(n, node, env->frameCtx);
}

const VSFrameRef* IT::GetFrame(IScriptEnvironment * env, int n) {
	++m_iCounter;
	env->m_iRealFrame = n;
	
	auto size = width * height;
	env->m_edgeMap = static_cast<unsigned char*>(_aligned_malloc(size, 16));
	memset(env->m_edgeMap, 0, width * height);

	env->m_motionMap4DI = static_cast<unsigned char*>(_aligned_malloc(size, 16));
	memset(env->m_motionMap4DI, 0, width * height);

	env->m_motionMap4DIMax = static_cast<unsigned char*>(_aligned_malloc(size, 16));
	memset(env->m_motionMap4DIMax, 0, width * height);

	int tfFrame;
	if (m_iFPS == 24) {
		tfFrame = n + n / (5 - 1);

		int base = (tfFrame / 5) * 5;
		int i;

		for (i = 0; i < 5; ++i)
			GetFrameSub(env, base + i);
		Decide(env, base);

		bool iflag = true;
		for (i = 0; i < 5; ++i) {
			if (m_frameInfo[clipFrame(base + i)].ivC >= m_iPThreshold) {
				iflag = false;
			}
		}
		m_blockInfo[base / 5].itype = iflag ? '3' : '2';
		int no = tfFrame - base;
		for (i = 0; i < 5; ++i) {
			char f = m_frameInfo[clipFrame(base + i)].mflag;
			if (f != 'D' && f != 'd' && f != 'X' && f != 'x' && f != 'y' && f != 'z' && f != 'R') {
				if (no == 0)
					break;
				--no;
			}
		}
		n = clipFrame(i + base);
	}
	else {
		GetFrameSub(env, n);
	}
	VSFrameRef * dst = env->NewVideoFrame(vi);
	MakeOutput(env, dst, n);
	_aligned_free(env->m_edgeMap);
	_aligned_free(env->m_motionMap4DI);
	_aligned_free(env->m_motionMap4DIMax);
	return dst;
}

void IT::GetFrameSub(IScriptEnvironment * env, int n) {
	if (n >= m_iMaxFrames)
		return;
	if (m_frameInfo[n].ip != 'U') {
		return;
	}
	env->m_iCurrentFrame = n;

	env->m_iUseFrame = 'C';
	env->m_iSumC = env->m_iSumP = env->m_iSumN = env->m_iSumM = vi->width * vi->height;
	env->m_bRefP = true;

	ChooseBest(env, n);
	m_frameInfo[n].match = static_cast<unsigned char>(env->m_iUseFrame);
	switch (toupper(env->m_iUseFrame)) {
		case 'C':
			env->m_iSumM = env->m_iSumC;
			env->m_iSumPM = env->m_iSumPC;
			break;
		case 'P':
			env->m_iSumM = env->m_iSumP;
			env->m_iSumPM = env->m_iSumPP;
			break;
		case 'N':
			env->m_iSumM = env->m_iSumN;
			env->m_iSumPM = env->m_iSumPN;
			break;
	}

	m_frameInfo[n].ivC = env->m_iSumC;
	m_frameInfo[n].ivP = env->m_iSumP;
	m_frameInfo[n].ivN = env->m_iSumN;
	m_frameInfo[n].ivM = env->m_iSumM;
	m_frameInfo[n].ivPC = env->m_iSumPC;
	m_frameInfo[n].ivPP = env->m_iSumPP;
	m_frameInfo[n].ivPN = env->m_iSumPN;
	m_frameInfo[n].ip = env->m_iSumM < m_iPThreshold && env->m_iSumPM < m_iPThreshold * 3 ? 'P' : 'I';
	return;
}

const VSFrameRef* IT::MakeOutput(IScriptEnvironment * env, VSFrameRef * dst, int n) {
	env->m_iCurrentFrame = n;

	env->m_iSumC = m_frameInfo[n].ivC;
	env->m_iSumP = m_frameInfo[n].ivP;
	env->m_iSumN = m_frameInfo[n].ivN;
	env->m_iSumM = m_frameInfo[n].ivM;
	env->m_iSumPC = m_frameInfo[n].ivPC;
	env->m_iSumPP = m_frameInfo[n].ivPP;
	env->m_iSumPN = m_frameInfo[n].ivPN;

	env->m_bRefP = true;

	env->m_iUseFrame = toupper(m_frameInfo[n].match);

	if (m_frameInfo[n].ip == 'P')
		CopyCPNField(env, dst, n);
	else if (!DrawPrevFrame(env, dst, n))
		DeintOneField_YV12(env, dst, n);

	return dst;
}
