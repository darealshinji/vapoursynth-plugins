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

#ifdef __C

#include "vs_it.h"

__forceinline unsigned char eval_iv_asm(
	unsigned const char * eax,
	unsigned const char * ebx,
	unsigned const char * ecx,
	int i) {
	auto a = eax[i];
	auto b = ebx[i];
	auto c = ecx[i];
	auto ab = a > b ? a - b : b - a;
	auto ac = a > c ? a - c : c - a;
	auto bc = (b + c + 1) >> 1;
	auto a_bc = a > bc ? a - bc : bc - a;
	auto min_ab_ac = VSMIN(ab, ac);
	return VSMIN(a_bc, min_ab_ac);
}

__forceinline unsigned char _mm_subs_abs_epu8(unsigned char a, unsigned char b) {
	return a > b ? a - b : b - a;
}

void IT::EvalIV_YV12(IScriptEnvironment * env, int n, const VSFrameRef * ref, long & counter, long & counterp) {
	const VSFrameRef * srcC = env->GetFrame(clipFrame(n));

	auto th = 40;
	auto th2 = 6;

	MakeDEmap_YV12(env, ref, 1);

	const int widthminus16 = (width - 16) >> 1;
	int sum = 0, sum2 = 0;
	for (int yy = 16; yy < height - 16; yy += 2) {
		int y;
		y = yy + 1;
		const unsigned char * pT = env->SYP(srcC, y - 1);
		const unsigned char * pC = env->SYP(ref, y);
		const unsigned char * pB = env->SYP(srcC, y + 1);
		const unsigned char * pT_U = env->SYP(srcC, y - 1, 1);
		const unsigned char * pC_U = env->SYP(ref, y, 1);
		const unsigned char * pB_U = env->SYP(srcC, y + 1, 1);
		const unsigned char * pT_V = env->SYP(srcC, y - 1, 2);
		const unsigned char * pC_V = env->SYP(ref, y, 2);
		const unsigned char * pB_V = env->SYP(srcC, y + 1, 2);

		const unsigned char * peT = &env->m_edgeMap[clipY(y - 1) * width];
		const unsigned char * peC = &env->m_edgeMap[clipY(y) * width];
		const unsigned char * peB = &env->m_edgeMap[clipY(y + 1) * width];


		for (int i = 16; i < widthminus16; i++) {
			auto yl = eval_iv_asm(pC, pT, pB, i * 2);
			auto yh = eval_iv_asm(pC, pT, pB, i * 2 + 1);
			auto u = eval_iv_asm(pC_U, pT_U, pB_U, i);
			auto v = eval_iv_asm(pC_V, pT_V, pB_V, i);

			auto uv = VSMAX(u, v);
			auto mm0l = VSMAX(yl, uv);
			auto mm0h = VSMAX(yh, uv);
			// mm0 <- max(y, max(u, v))

			auto peCl = peC[i * 2];
			auto peCh = peC[i * 2 + 1];
			auto peTl = peT[i * 2];
			auto peTh = peT[i * 2 + 1];
			auto peBl = peB[i * 2];
			auto peBh = peB[i * 2 + 1];
			auto pel = VSMAX(peTl, peBl);
			auto peh = VSMAX(peTh, peBh);
			pel = VSMAX(pel, peCl);
			peh = VSMAX(peh, peCh);
			// pe <- max(peC, peT, peB)

			// Saturate Subtract mm0 - pe * 2
			mm0l = mm0l > pel ? mm0l - pel : 0;
			mm0l = mm0l > pel ? mm0l - pel : 0;
			mm0h = mm0h > peh ? mm0h - peh : 0;
			mm0h = mm0h > peh ? mm0h - peh : 0;

			sum += mm0l > th ? 1 : 0;
			sum += mm0h > th ? 1 : 0;

			sum2 += mm0l > th2 ? 1 : 0;
			sum2 += mm0h > th2 ? 1 : 0;
		}

		if (sum > m_iPThreshold) {
			sum = m_iPThreshold;
			break;
		}
	}
	counter = sum;
	counterp = sum2;

	env->FreeFrame(srcC);
	return;
}

__forceinline unsigned char make_de_map_asm(
	unsigned const char * eax,
	unsigned const char * ebx,
	unsigned const char * ecx,
	int i, int step, int offset) {
	// return abs(a - (b + c) / 2)
	auto a = eax[i * step + offset];
	auto b = ebx[i * step + offset];
	auto c = ecx[i * step + offset];
	auto bc = (b + c + 1) >> 1;
	return _mm_subs_abs_epu8(a, bc);
}

void IT::MakeDEmap_YV12(IScriptEnvironment * env, const VSFrameRef * ref, int offset) {
	const int twidth = width >> 1;

	for (int yy = 0; yy < height; yy += 2) {
		int y = yy + offset;
		const unsigned char * pTT = env->SYP(ref, y - 2);
		const unsigned char * pC = env->SYP(ref, y);
		const unsigned char * pBB = env->SYP(ref, y + 2);
		const unsigned char * pTT_U = env->SYP(ref, y - 2, 1);
		const unsigned char * pC_U = env->SYP(ref, y, 1);
		const unsigned char * pBB_U = env->SYP(ref, y + 2, 1);
		const unsigned char * pTT_V = env->SYP(ref, y - 2, 2);
		const unsigned char * pC_V = env->SYP(ref, y, 2);
		const unsigned char * pBB_V = env->SYP(ref, y + 2, 2);
		unsigned char * pED = env->m_edgeMap + y * width;

		unsigned char y0, y1, u0, v0, uv;
		for (int i = 0; i < twidth; i++) {
			y0 = make_de_map_asm(pC, pTT, pBB, i, 2, 0);
			y1 = make_de_map_asm(pC, pTT, pBB, i, 2, 1);
			u0 = make_de_map_asm(pC_U, pTT_U, pBB_U, i, 1, 0);
			v0 = make_de_map_asm(pC_V, pTT_V, pBB_V, i, 1, 0);
			uv = VSMAX(u0, v0);
			pED[i * 2] = VSMAX(uv, y0);
			pED[i * 2 + 1] = VSMAX(uv, y1);
		}
	}
}

void IT::MakeMotionMap_YV12(IScriptEnvironment * env, int n, bool flag) {
	n = clipFrame(n);
	if (flag == false && m_frameInfo[n].diffP0 >= 0)
		return;


	const int twidth = width;
	const int widthminus8 = width - 8;
	const int widthminus16 = width - 16;
	int i;

	const VSFrameRef * srcP = env->GetFrame(clipFrame(n - 1));
	const VSFrameRef * srcC = env->GetFrame(n);
	short bufP0[MAX_WIDTH];
	unsigned char bufP1[MAX_WIDTH];
	int pe0 = 0, po0 = 0, pe1 = 0, po1 = 0;
	for (int yy = 16; yy < height - 16; ++yy) {
		int y = yy;
		const unsigned char * pC = env->SYP(srcC, y);
		const unsigned char * pP = env->SYP(srcP, y);

		for (i = 0; i < twidth; i++) {
			bufP0[i] = static_cast<short>(pC[i]) - static_cast<short>(pP[i]);
		}

		for (i = 8; i < widthminus8; i++) {
			auto A = bufP0[i - 1];
			auto B = bufP0[i];
			auto C = bufP0[i + 1];
			auto delta = A - B + C - B;
			// Abs(B)
			B = B >= 0 ? B : -B;
			// Abs(delta)
			delta = delta >= 0 ? delta : -delta;
			// Saturate Subtract (B-delta) To 8-bit
			bufP1[i] = VSMIN(255, VSMAX(0, B - delta));
		}

		int tsum = 0, tsum1 = 0;
		for (i = 16; i < widthminus16; i++) {
			auto A = bufP1[i - 1];
			auto B = bufP1[i + 1];
			auto C = bufP1[i];
			auto ABC = A + B + C;
			if (ABC > 36) tsum++;
			if (ABC > 18) tsum1++;
		}

		if ((y & 1) == 0) {
			pe0 += tsum;
			pe1 += tsum1;
		}
		else {
			po0 += tsum;
			po1 += tsum1;
		}
	}
	m_frameInfo[n].diffP0 = pe0;
	m_frameInfo[n].diffP1 = po0;
	m_frameInfo[n].diffS0 = pe1;
	m_frameInfo[n].diffS1 = po1;
	env->FreeFrame(srcC);
	env->FreeFrame(srcP);
}

__forceinline unsigned char make_motion_map2_asm(
	const unsigned char * eax,
	const unsigned char * ebx,
	int i) {
	return _mm_subs_abs_epu8(eax[i], ebx[i]);
}

void IT::MakeMotionMap2Max_YV12(IScriptEnvironment * env, int n) {
	const int twidth = width >> 1;

	const VSFrameRef * srcP = env->GetFrame(n - 1);
	const VSFrameRef * srcC = env->GetFrame(n);
	const VSFrameRef * srcN = env->GetFrame(n + 1);

	for (int y = 0; y < height; y++) {
		unsigned char * pD = env->m_motionMap4DIMax + y * width;
		const unsigned char * pC = env->SYP(srcC, y);
		const unsigned char * pP = env->SYP(srcP, y);
		const unsigned char * pN = env->SYP(srcN, y);
		const unsigned char * pC_U = env->SYP(srcC, y, 1);
		const unsigned char * pP_U = env->SYP(srcP, y, 1);
		const unsigned char * pN_U = env->SYP(srcN, y, 1);
		const unsigned char * pC_V = env->SYP(srcC, y, 2);
		const unsigned char * pP_V = env->SYP(srcP, y, 2);
		const unsigned char * pN_V = env->SYP(srcN, y, 2);

		for (int i = 0; i < twidth; i++) {
			///P
			auto yl = make_motion_map2_asm(pC, pP, i * 2);
			auto yh = make_motion_map2_asm(pC, pP, i * 2 + 1);
			auto u = make_motion_map2_asm(pC_U, pP_U, i);
			auto v = make_motion_map2_asm(pC_V, pP_V, i);
			auto uv = VSMAX(u, v);
			auto pl = VSMAX(uv, yl);
			auto ph = VSMAX(uv, yh);

			///N
			yl = make_motion_map2_asm(pC, pN, i * 2);
			yh = make_motion_map2_asm(pC, pN, i * 2 + 1);
			u = make_motion_map2_asm(pC_U, pN_U, i);
			v = make_motion_map2_asm(pC_V, pN_V, i);
			uv = VSMAX(u, v);
			auto nl = VSMAX(uv, yl);
			auto nh = VSMAX(uv, yh);

			pD[i * 2] = VSMAX(pl, nl);
			pD[i * 2 + 1] = VSMAX(ph, nh);
		}
	}
	env->FreeFrame(srcC);
	env->FreeFrame(srcP);
	env->FreeFrame(srcN);
}

void IT::MakeSimpleBlurMap_YV12(IScriptEnvironment * env, int n) {
	int twidth = width;
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
	const unsigned char * pT;
	const unsigned char * pC;
	const unsigned char * pB;
	for (int y = 0; y < height; y++) {
		unsigned char * pD = env->m_motionMap4DI + y * width;
		if (y % 2) {
			pT = env->SYP(srcC, y - 1);
			pC = env->SYP(srcR, y);
			pB = env->SYP(srcC, y + 1);
		}
		else {
			pT = env->SYP(srcR, y - 1);
			pC = env->SYP(srcC, y);
			pB = env->SYP(srcR, y + 1);
		}
		for (int i = 0; i < twidth; i++) {
			auto c = pC[i];
			auto t = pT[i];
			auto b = pB[i];
			auto ct = _mm_subs_abs_epu8(c, t);
			auto cb = _mm_subs_abs_epu8(c, b);
			auto tb = _mm_subs_abs_epu8(t, b);
			int delta = ct;
			delta = VSMIN(255, delta + cb);
			delta = VSMAX(0, delta - tb - tb);
			pD[i] = static_cast<unsigned char>(delta);
		}
	}
	if (srcC != srcR)
		env->FreeFrame(srcR);
	env->FreeFrame(srcC);
}
#endif
