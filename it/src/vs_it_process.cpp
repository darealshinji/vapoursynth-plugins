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
#include "vs_it_interface.h"

void IT::SetFT(IScriptEnvironment * env, int base, int n, char c) {
	m_frameInfo[clipFrame(base + n)].mflag = c;
	m_blockInfo[base / 5].cfi = n;
	m_blockInfo[base / 5].level = '0';
}

void IT::ChooseBest(IScriptEnvironment * env, int n) {
	const VSFrameRef * srcC = env->GetFrame(clipFrame(n));
	const VSFrameRef * srcP = env->GetFrame(clipFrame(n - 1));

	MakeMotionMap_YV12(env, env->m_iCurrentFrame, false);
	MakeMotionMap_YV12(env, env->m_iCurrentFrame + 1, false);
	MakeDEmap_YV12(env, srcC, 0);
	EvalIV_YV12(env, n, srcC, env->m_iSumC, env->m_iSumPC);
	EvalIV_YV12(env, n, srcP, env->m_iSumP, env->m_iSumPP);

	CompCP(env);
	env->FreeFrame(srcC);
	env->FreeFrame(srcP);
}

void IT::Decide(IScriptEnvironment * env, int n) {
	if (m_blockInfo[n / 5].level != 'U')
		return;

	int base = (n / 5) * 5;
	int i;
	int min0 = m_frameInfo[clipFrame(base)].diffP0;
	for (i = 1; i < 5; ++i) {
		min0 = VSMIN(min0, m_frameInfo[clipFrame(base + i)].diffP0);
	}
	int mmin = AdjPara(50);

	for (i = 0; i < 5; ++i) {
		int m = m_frameInfo[clipFrame(base + i)].diffP0;
		m_frameInfo[clipFrame(base + i)].mflag = m >= VSMAX(mmin, min0) * 5 ? '.' : '+';
	}

	int ncf = 0;
	int cfi = -1;
	for (i = 0; i < 5; ++i) {
		if (m_frameInfo[clipFrame(base + i)].mflag == '.')
			++ncf;
		else
			cfi = i;
	}

	int mmin2 = AdjPara(50);
	if (ncf == 0) {
		min0 = m_frameInfo[clipFrame(base)].diffS0;
		for (i = 1; i < 5; ++i) {
			min0 = VSMIN(min0, m_frameInfo[clipFrame(base + i)].diffS0);
		}
		for (i = 0; i < 5; ++i) {
			int m = m_frameInfo[clipFrame(base + i)].diffS0;
			m_frameInfo[clipFrame(base + i)].mflag = m >= VSMAX(mmin2, min0) * 3 ? '.' : '+';
		}
		ncf = 0;
		cfi = -1;
		for (i = 0; i < 5; ++i) {
			if (m_frameInfo[clipFrame(base + i)].mflag == '.')
				++ncf;
			else
				cfi = i;
		}
	}

	if (ncf == 4 && cfi >= 0) {
		SetFT(env, base, cfi, 'D');
		return;
	}
	if (ncf != 0 || 1) {
		bool flag = false;
		for (i = 0; i < 5; ++i) {
			int rr = (i + 2 + 5) % 5;
			int r = (i + 1 + 5) % 5;
			int l = (i - 1 + 5) % 5;
			if (m_frameInfo[clipFrame(base + i)].mflag != '.' && m_frameInfo[clipFrame(base + i)].match == 'P') {
				if (m_frameInfo[clipFrame(base + i)].mflag == '+') {
					m_frameInfo[clipFrame(base + i)].mflag = '*';
					flag = true;
				}
				if (m_frameInfo[clipFrame(base + r)].mflag == '+') {
					m_frameInfo[clipFrame(base + r)].mflag = '*';
					flag = true;
				}
				if (m_frameInfo[clipFrame(base + l)].mflag == '+') {
					m_frameInfo[clipFrame(base + l)].mflag = '*';
					flag = true;
				}
			}
			if (m_frameInfo[clipFrame(base + i)].match == 'N') {
				if (m_frameInfo[clipFrame(base + r)].mflag == '+') {
					m_frameInfo[clipFrame(base + r)].mflag = '*';
					flag = true;
				}
				if (m_frameInfo[clipFrame(base + rr)].mflag == '+') {
					m_frameInfo[clipFrame(base + rr)].mflag = '*';
					flag = true;
				}
			}

		}

		//31228 39045

		if (flag) {
			for (i = 0; i < 5; ++i) {
				char c = m_frameInfo[clipFrame(base + i)].mflag;
				if (c == '+')
					m_frameInfo[clipFrame(base + i)].mflag = '*';
				if (c == '*')
					m_frameInfo[clipFrame(base + i)].mflag = '+';
			}
		}
		for (i = 0; i < 5; ++i) {
			if (m_frameInfo[clipFrame(base + i)].pos == '2') {
				SetFT(env, base, i, 'd');
				return;
			}
		}
		if (base - 5 >= 0 && m_blockInfo[base / 5 - 1].level != 'U') {
			int tcfi = m_blockInfo[base / 5 - 1].cfi;
			if (m_frameInfo[base + tcfi].mflag == '+') {
				SetFT(env, base, tcfi, 'y');
				return;
			}
		}
		int pnpos[5], pncnt = 0;
		for (i = 0; i < 5; ++i) {
			if (toupper(m_frameInfo[clipFrame(base + i)].match) == 'P') {
				pnpos[pncnt++] = i;
			}
		}
		if (pncnt == 2) {
			int k = pnpos[0];
			if (pnpos[0] == 0 && pnpos[1] == 4) {
				k = 4;
			}
			if (m_frameInfo[clipFrame(base + k)].mflag != '.') {
				SetFT(env, base, k, 'x');
				return;
			}
		}

		pncnt = 0;
		for (i = 0; i < 5; ++i) {
			if (toupper(m_frameInfo[clipFrame(base + i)].match) != 'N') {
				pnpos[pncnt++] = i;
			}
		}
		if (pncnt == 2) {
			int k = pnpos[0];
			if (pnpos[0] == 3 && pnpos[1] == 4) {
				k = 4;
			}
			k = (k + 2) % 5;
			if (m_frameInfo[clipFrame(base + k)].mflag != '.') {
				SetFT(env, base, k, 'x');
				return;
			}
		}

		for (i = 0; i < 5; ++i) {
			if (m_frameInfo[clipFrame(base + i)].mflag == '+') {
				SetFT(env, base, i, 'd');
				return;
			}
		}
	}

	cfi = 0;
	int minx = m_frameInfo[clipFrame(base)].diffS0;
	for (i = 1; i < 5; ++i) {
		int m = m_frameInfo[clipFrame(base + i)].diffS0;
		if (m < minx) {
			cfi = i;
			minx = m;
		}
	}
	SetFT(env, base, cfi, 'z');
	return;
}

void IT::DeintOneField_YV12(IScriptEnvironment * env, VSFrameRef * dst, int n) {
	const VSFrameRef * srcC = env->GetFrame(n);
	const VSFrameRef * srcR;
	switch (toupper(env->m_iUseFrame)) {
		default:
		case 'C':
			srcR = srcC;
			break;
		case 'P':
			srcR = env->GetFrame(n - 1);
			break;
		case 'N':
			srcR = env->GetFrame(n + 1);
			break;
	}

	// const unsigned char *pT;
	const unsigned char * pC;
	const unsigned char * pB;
	const unsigned char * pBB;
	const unsigned char * pC_U;
	// const unsigned char *pB_U;
	const unsigned char * pBB_U;
	const unsigned char * pC_V;
	// const unsigned char *pB_V;
	const unsigned char * pBB_V;
	unsigned char * pDC;
	unsigned char * pDB;
	unsigned char * pDC_U;
	unsigned char * pDC_V;
	unsigned char * pDB_U;
	unsigned char * pDB_V;

	MakeSimpleBlurMap_YV12(env, env->m_iCurrentFrame);
	MakeMotionMap2Max_YV12(env, env->m_iCurrentFrame);

	unsigned char * pFieldMap;
	pFieldMap = new unsigned char[width * height];
	memset(pFieldMap, 0, width * height);
	int x, y;
	for (y = 0; y < height; y += 1) {
		unsigned char * pFM = pFieldMap + width * clipY(y);
		for (x = 1; x < width - 1; x++) {
			const unsigned char * pmSC = env->m_motionMap4DI + width * clipY(y);
			const unsigned char * pmSB = env->m_motionMap4DI + width * clipY(y + 1);
			const unsigned char * pmMC = env->m_motionMap4DIMax + width * clipY(y);
			const unsigned char * pmMB = env->m_motionMap4DIMax + width * clipY(y + 1);
			const int nTh = 12;
			const int nThLine = 1;
			if (((pmSC[x - 1] > nThLine && pmSC[x] > nThLine && pmSC[x + 1] > nThLine) ||
					(pmSB[x - 1] > nThLine && pmSB[x] > nThLine && pmSB[x + 1] > nThLine)) &&
				((pmMC[x - 1] > nTh && pmMC[x] > nTh && pmMC[x + 1] > nTh) ||
					(pmMB[x - 1] > nTh && pmMB[x] > nTh && pmMB[x + 1] > nTh))) {
				pFM[x - 1] = 1;
				pFM[x] = 1;
				pFM[x + 1] = 1;
			}
		}
	}

	const int nPitchSrc = env->vsapi->getStride(srcC, 0);
	const int nPitchSrcU = env->vsapi->getStride(srcC, 1);
	const int nPitchDst = env->vsapi->getStride(dst, 0);
	const int nRowSizeDst = width; // Shall DIVIDE BY SIZEOF(T) if not 8bit
	const int nPitchDstU = env->vsapi->getStride(dst, 1);
	const int nRowSizeDstU = width >> vi->format->subSamplingW;

	for (y = 0; y < height; y += 2) {
		// pT = env->SYP(srcR, y - 1);
		pC = env->SYP(srcC, y);
		pB = env->SYP(srcR, y + 1);
		pBB = env->SYP(srcC, y + 2);
		pC_U = env->SYP(srcC, y, 1);
		// pB_U = env->SYP(srcR, y + 1, 1);
		pBB_U = env->SYP(srcC, y + 4, 1);
		pC_V = env->SYP(srcC, y, 2);
		// pB_V = env->SYP(srcR, y + 1, 2);
		pBB_V = env->SYP(srcC, y + 4, 2);

		pDC = env->DYP(dst, y);
		pDB = env->DYP(dst, y + 1);
		pDC_U = env->DYP(dst, y, 1);
		pDB_U = env->DYP(dst, y + 1, 1);
		pDC_V = env->DYP(dst, y, 2);
		pDB_V = env->DYP(dst, y + 1, 2);

		vs_bitblt(pDC, nPitchDst, pC, nPitchSrc, nRowSizeDst, 1);
		if ((y >> 1) % 2) {
			vs_bitblt(pDC_U, nPitchDstU, pC_U, nPitchSrcU, nRowSizeDstU, 1);
			vs_bitblt(pDC_V, nPitchDstU, pC_V, nPitchSrcU, nRowSizeDstU, 1);
		}

		const unsigned char * pFM = pFieldMap + width * clipY(y);
		const unsigned char * pFMB = pFieldMap + width * clipY(y + 1);
		for (x = 0; x < width; ++x) {
			int x_half = x >> 1;
			pDB[x] = pFM[x - 1] == 1 || pFM[x] == 1 || pFM[x + 1] == 1 ||
			         (pFMB[x - 1] == 1 || pFMB[x] == 1 || pFMB[x + 1] == 1) ?
				         static_cast<unsigned char>((pC[x] + pBB[x] + 1) >> 1) : pB[x];

			if ((y >> 1) % 2) {
				pDB_U[x_half] = static_cast<unsigned char>((pC_U[x_half] + pBB_U[x_half] + 1) >> 1);
				pDB_V[x_half] = static_cast<unsigned char>((pC_V[x_half] + pBB_V[x_half] + 1) >> 1);
			}
		}
	}
	delete[] pFieldMap;
	if (srcC != srcR)
		env->FreeFrame(srcR);
	env->FreeFrame(srcC);

	return;
}

bool IT::CompCP(IScriptEnvironment * env) {
	int n = env->m_iCurrentFrame;
	int p0 = m_frameInfo[n].diffP0;
	int p1 = m_frameInfo[n].diffP1;
	int n0 = m_frameInfo[clipFrame(n + 1)].diffP0;
	int n1 = m_frameInfo[clipFrame(n + 1)].diffP1;
	int ps0 = m_frameInfo[n].diffS0;
	int ps1 = m_frameInfo[n].diffS1;
	int ns0 = m_frameInfo[clipFrame(n + 1)].diffS0;
	int ns1 = m_frameInfo[clipFrame(n + 1)].diffS1;

	int th = AdjPara(5);
	int thm = AdjPara(5);
	int ths = AdjPara(200);

	bool spe = p0 < th && ps0 < ths;
	bool spo = p1 < th && ps1 < ths;
	bool sne = n0 < th && ns0 < ths;
	bool sno = n1 < th && ns1 < ths;

	bool mpe = p0 > thm;
	bool mpo = p1 > thm;
	bool mne = n0 > thm;
	bool mno = n1 > thm;

	//1773
	int thcomb = AdjPara(20);
	if (n != 0) {
		if ((env->m_iSumC < thcomb && env->m_iSumP < thcomb) || labs(env->m_iSumC - env->m_iSumP) * 10 < env->m_iSumC + env->m_iSumP) {
			if (labs(env->m_iSumC - env->m_iSumP) > AdjPara(8)) {
				env->m_iUseFrame = env->m_iSumP >= env->m_iSumC ? 'c' : 'p';
				return true;
			}
			if (labs(env->m_iSumPC - env->m_iSumPP) > AdjPara(10)) {
				env->m_iUseFrame = env->m_iSumPP >= env->m_iSumPC ? 'c' : 'p';
				return true;
			}

			if (spe && mpo) {
				env->m_iUseFrame = 'p';
				return true;
			}
			if (mpe && spo) {
				env->m_iUseFrame = 'c';
				return true;
			}
			if (mne && sno) {
				env->m_iUseFrame = 'p';
				return true;
			}
			if (sne && mno) {
				env->m_iUseFrame = 'c';
				return true;
			}
			if (spe && spo) {
				env->m_iUseFrame = 'c';
				return false;
			}
			if (sne && sno) {
				env->m_iUseFrame = 'c';
				return false;
			}
			if (mpe && mpo && mne && mno) {
				env->m_iUseFrame = 'c';
				return false;
			}

			if (env->m_iSumPC > env->m_iSumPP) {
				env->m_iUseFrame = 'p';
				return true;
			}
			env->m_iUseFrame = 'c';
			return false;
		}
	}
	m_frameInfo[n].pos = '.';
	if (env->m_iSumP >= env->m_iSumC) {
		env->m_iUseFrame = 'C';
		if (!spe) {
			m_frameInfo[n].pos = '.';
		}
	}
	else {
		env->m_iUseFrame = 'P';
		if (spe && !sno) {
			m_frameInfo[n].pos = '2';
		}
		if (!spe && sno) {
			m_frameInfo[n].pos = '3';
		}
	}
	return true;
}

bool IT::DrawPrevFrame(IScriptEnvironment * env, VSFrameRef * dst, int n) {
	bool bResult = false;

	int nPrevFrame = clipFrame(n - 1);
	int nNextFrame = clipFrame(n + 1);

	int nOldCurrentFrame = env->m_iCurrentFrame;
	int nOldUseFrame = env->m_iUseFrame;

	GetFrameSub(env, nPrevFrame);
	GetFrameSub(env, nNextFrame);

	env->m_iCurrentFrame = nOldCurrentFrame;

	if (m_frameInfo[nPrevFrame].ip == 'P' && m_frameInfo[nNextFrame].ip == 'P')
		bResult = CheckSceneChange(env, n);

	if (bResult) {
		env->m_iUseFrame = m_frameInfo[nPrevFrame].match;
		CopyCPNField(env, dst, nPrevFrame);
	}

	env->m_iUseFrame = nOldUseFrame;

	return bResult;
}

void IT::CopyCPNField(IScriptEnvironment * env, VSFrameRef * dst, int n) {
	const VSFrameRef * srcC = env->GetFrame(clipFrame(n));
	const VSFrameRef * srcR;
	switch (toupper(env->m_iUseFrame)) {
		default:
		case 'C':
			srcR = srcC;
			break;
		case 'P':
			srcR = env->GetFrame(clipFrame(n - 1));
			break;
		case 'N':
			srcR = env->GetFrame(clipFrame(n + 1));
			break;
	}

	int nPitch = env->vsapi->getStride(dst, 0);
	int nRowSize = width;
	int nPitchU = env->vsapi->getStride(dst, 1);
	int nRowSizeU = width >> vi->format->subSamplingW;

	for (int yy = 0; yy < height; yy += 2) {
		int y, yo;
		y = yy + 1;
		yo = yy + 0;
		vs_bitblt(env->DYP(dst, yo), nPitch, env->SYP(srcC, yo), nPitch, nRowSize, 1);
		vs_bitblt(env->DYP(dst, y), nPitch, env->SYP(srcR, y), nPitch, nRowSize, 1);

		if ((yy >> 1) % 2) {
			vs_bitblt(env->DYP(dst, yo, 1), nPitchU, env->SYP(srcC, yo, 1), nPitchU, nRowSizeU, 1);
			vs_bitblt(env->DYP(dst, y, 1), nPitchU, env->SYP(srcR, y, 1), nPitchU, nRowSizeU, 1);
			vs_bitblt(env->DYP(dst, yo, 2), nPitchU, env->SYP(srcC, yo, 2), nPitchU, nRowSizeU, 1);
			vs_bitblt(env->DYP(dst, y, 2), nPitchU, env->SYP(srcR, y, 2), nPitchU, nRowSizeU, 1);
		}
	}
	// USE_MMX2;
	if (srcC != srcR)
		env->FreeFrame(srcR);
	env->FreeFrame(srcC);
}

bool IT::CheckSceneChange(IScriptEnvironment * env, int n) {
	const VSFrameRef * srcP = env->GetFrame(clipFrame(n - 1));
	const VSFrameRef * srcC = env->GetFrame(clipFrame(n));

	int rowSize = env->vsapi->getStride(srcC, 0);

	int sum3 = 0;
	int x, y;

	int startY = 1;

	for (y = startY; y < height; y += 2) {
		const unsigned char * pC = env->SYP(srcC, y);
		const unsigned char * pP = env->SYP(srcP, y);

		for (x = 0; x < rowSize; x++) {
			int a = abs(pC[x] - pP[x]);
			if (a > 50) sum3 += 1;
		}
	}
	env->FreeFrame(srcP);
	env->FreeFrame(srcC);
	return sum3 > height * rowSize / 8;
}
