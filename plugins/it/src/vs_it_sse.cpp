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

#ifdef __SSE

#include "vs_it.h"

auto mmask1 = _mm_set1_epi8(1);
auto zero = _mm_setzero_si128();

__forceinline __m128i _mm_subs_abs_epu8(__m128i a, __m128i b) {
	// subs+subs+or seems to be faster than sub+abs
	auto delta1 = _mm_subs_epu8(a, b);
	auto delta2 = _mm_subs_epu8(b, a);
	return _mm_or_si128(delta1, delta2);
}

__forceinline int _mm_sum_epu8(__m128i source) {
	auto sum = _mm_sad_epu8(source, zero);
	ALIGNED_ARRAY(unsigned int dest[4], 16);
	_mm_store_si128(reinterpret_cast<__m128i*>(dest), sum);
	return dest[0] + dest[2];
}

__forceinline __m128i _mm_cmpge_cnt_epu8(__m128i source, __m128i threshold) {
	auto dest = _mm_subs_epu8(source, threshold);
	//psubusb mm0, th
	dest = _mm_cmpeq_epi8(dest, zero);
	//pcmpeqb mm0, mm7
	dest = _mm_cmpeq_epi8(dest, zero);
	//pcmpeqb mm0, mm7
	dest = _mm_and_si128(dest, mmask1);
	return dest;
}

__forceinline __m128i eval_iv_asm(
	unsigned const char * eax,
	unsigned const char * ebx,
	unsigned const char * ecx,
	int i) {
	auto mma = _mm_load_si128(reinterpret_cast<const __m128i*>(eax + i));
	auto mmb = _mm_load_si128(reinterpret_cast<const __m128i*>(ebx + i));
	auto mmc = _mm_load_si128(reinterpret_cast<const __m128i*>(ecx + i));
	auto mmab = _mm_subs_abs_epu8(mma, mmb);
	auto mmac = _mm_subs_abs_epu8(mma, mmc);
	auto mmbc = _mm_avg_epu8(mmb, mmc);
	auto mm_a_bc = _mm_subs_abs_epu8(mma, mmbc);
	auto mm_min_ab_ac = _mm_min_epu8(mmab, mmac);
	return _mm_min_epu8(mm_a_bc, mm_min_ab_ac);

}

void IT::EvalIV_YV12(IScriptEnvironment * env, int n, const VSFrameRef * ref, long & counter, long & counterp) {
	const VSFrameRef * srcC = env->GetFrame(clipFrame(n));

	auto mth = _mm_set1_epi8(40);
	auto mth2 = _mm_set1_epi8(6);

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

		//pxor mm7, mm7
		//mov esi, 16
		//movq rsum, mm7
		//movq psum, mm7
		//movq psum0, mm7
		//movq psum1, mm7
		//align 16
		//loopB:
		auto mrsum = zero, mpsum = zero;
		for (int i = 16; i < widthminus16; i += 16) { // unrolling to twice a time, low/high
			//EVAL_IV_ASM_INIT(pC, pT, pB)
			//EVAL_IV_ASM(mm0, 2)
			auto yl = eval_iv_asm(pC, pT, pB, i * 2);
			auto yh = eval_iv_asm(pC, pT, pB, i * 2 + 16);

			//EVAL_IV_ASM_INIT(pC_U, pT_U, pB_U)
			//EVAL_IV_ASM(mm5, 1)
			auto u = eval_iv_asm(pC_U, pT_U, pB_U, i);

			//EVAL_IV_ASM_INIT(pC_V, pT_V, pB_V)
			//EVAL_IV_ASM(mm6, 1)
			auto v = eval_iv_asm(pC_V, pT_V, pB_V, i);

			auto uv = _mm_max_epu8(u, v);
			//pmaxub mm5, mm6
			auto uvl = _mm_unpacklo_epi8(uv, uv);
			auto uvh = _mm_unpackhi_epi8(uv, uv);
			//punpcklbw mm5, mm5
			auto mm0l = _mm_max_epu8(yl, uvl);
			auto mm0h = _mm_max_epu8(yh, uvh);
			//pmaxub mm0, mm5; mm0 <-max(y, max(u, v))

			auto peCl = _mm_load_si128(reinterpret_cast<const __m128i*>(peC + i * 2));
			auto peCh = _mm_load_si128(reinterpret_cast<const __m128i*>(peC + i * 2 + 16));
			auto peTl = _mm_load_si128(reinterpret_cast<const __m128i*>(peT + i * 2));
			auto peTh = _mm_load_si128(reinterpret_cast<const __m128i*>(peT + i * 2 + 16));
			auto peBl = _mm_load_si128(reinterpret_cast<const __m128i*>(peB + i * 2));
			auto peBh = _mm_load_si128(reinterpret_cast<const __m128i*>(peB + i * 2 + 16));
			//mov rdx, peC
			//movq mm3, [rdx + rsi * 2]
			//mov rdx, peT
			auto pel = _mm_max_epu8(peTl, peBl);
			auto peh = _mm_max_epu8(peTh, peBh);
			//pmaxub mm3, [rdx + rsi * 2]
			//mov rdx, peB
			pel = _mm_max_epu8(pel, peCl);
			peh = _mm_max_epu8(peh, peCh);
			//pmaxub mm3, [rdx + rsi * 2]; mm3 <-max(peC[x], max(peT[x], peB[x]))

			mm0l = _mm_subs_epu8(mm0l, pel);
			mm0h = _mm_subs_epu8(mm0h, peh);
			//psubusb mm0, mm3
			mm0l = _mm_subs_epu8(mm0l, pel);
			mm0h = _mm_subs_epu8(mm0h, peh);
			//psubusb mm0, mm3
			//movq mm1, mm0

			auto mth_l = _mm_cmpge_cnt_epu8(mm0l, mth);
			auto mth_h = _mm_cmpge_cnt_epu8(mm0h, mth);
			mrsum = _mm_adds_epu8(mrsum, mth_l);
			mrsum = _mm_adds_epu8(mrsum, mth_h);
			//paddusb mm0, rsum; if (max - maxpe * 2 > 40) sum++

			auto mth2_l = _mm_cmpge_cnt_epu8(mm0l, mth2);
			auto mth2_h = _mm_cmpge_cnt_epu8(mm0h, mth2);
			mpsum = _mm_adds_epu8(mpsum, mth2_l);
			mpsum = _mm_adds_epu8(mpsum, mth2_h);
			//paddusb mm1, psum; if (max - maxpe * 2 > 6) sum2++

			//lea esi, [esi + 4]
			//cmp esi, widthminus16
			//jl loopB
		}

		sum += _mm_sum_epu8(mrsum);
		sum2 += _mm_sum_epu8(mpsum);

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

__forceinline __m128i make_de_map_asm(
	unsigned const char * eax,
	unsigned const char * ebx,
	unsigned const char * ecx,
	int i, int step, int offset) {
	auto mma = _mm_load_si128(reinterpret_cast<const __m128i*>(eax + i * step + offset));
	auto mmb = _mm_load_si128(reinterpret_cast<const __m128i*>(ebx + i * step + offset));
	auto mmc = _mm_load_si128(reinterpret_cast<const __m128i*>(ecx + i * step + offset));
	auto mmbc = _mm_avg_epu8(mmb, mmc);

	return _mm_subs_abs_epu8(mma, mmbc);
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

		//		mov rdi, pED
		//		xor esi, esi
		//		align 16
		//	loopA:
		for (int i = 0; i < twidth; i += 16) {
			// MAKE_DE_MAP_ASM_INIT(pC, pTT, pBB);
			auto yl = make_de_map_asm(pC, pTT, pBB, i, 2, 0);
			// MAKE_DE_MAP_ASM(mm0, 2, 0);
			auto yh = make_de_map_asm(pC, pTT, pBB, i, 2, 16);
			// MAKE_DE_MAP_ASM(mm3, 2, 8);
			// MAKE_DE_MAP_ASM_INIT(pC_U, pTT_U, pBB_U);
			auto u = make_de_map_asm(pC_U, pTT_U, pBB_U, i, 1, 0);
			// MAKE_DE_MAP_ASM(mm1, 1, 0);
			// MAKE_DE_MAP_ASM(mm4, 1, 4);
			// MAKE_DE_MAP_ASM_INIT(pC_V, pTT_V, pBB_V);
			auto v = make_de_map_asm(pC_V, pTT_V, pBB_V, i, 1, 0);
			// MAKE_DE_MAP_ASM(mm2, 1, 0);
			// MAKE_DE_MAP_ASM(mm5, 1, 4);

			auto uv = _mm_max_epu8(v, u);
			//	pmaxub mm2, mm1
			// mm5 = _mm_max_epu8(mm5, mm4);
			//	pmaxub mm5, mm4
			auto uvl = _mm_unpacklo_epi8(uv, uv);
			//	punpcklbw mm2, mm2
			auto uvh = _mm_unpackhi_epi8(uv, uv);
			//	punpcklbw mm5, mm5
			yl = _mm_max_epu8(yl, uvl);
			//	pmaxub mm0, mm2
			yh = _mm_max_epu8(yh, uvh);
			//	pmaxub mm3, mm5

			//	lea esi, [esi + 8]
			_mm_stream_si128(reinterpret_cast<__m128i*>(pED + i * 2), yl);
			//	movntq[rdi + rsi * 2 - 16], mm0
			//	cmp esi, twidth
			_mm_stream_si128(reinterpret_cast<__m128i*>(pED + i * 2 + 16), yh);
			//	movntq[rdi + rsi * 2 - 8], mm3
			//	jl loopA
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
	auto mmbTh = _mm_set1_epi8(36);
	auto mmbTh2 = _mm_set1_epi8(18);
	auto mmask1 = _mm_set1_epi8(1);

	const VSFrameRef * srcP = env->GetFrame(clipFrame(n - 1));
	const VSFrameRef * srcC = env->GetFrame(n);
	ALIGNED_ARRAY(short bufP0[MAX_WIDTH], 16);
	ALIGNED_ARRAY(unsigned char bufP1[MAX_WIDTH], 16);
	int pe0 = 0, po0 = 0, pe1 = 0, po1 = 0;
	for (int yy = 16; yy < height - 16; ++yy) {
		int y = yy;
		const unsigned char * pC = env->SYP(srcC, y);
		const unsigned char * pP = env->SYP(srcP, y);

		__m128i c, p, cx, px;
		for (i = 0; i < twidth; i += 16) {
			_mm_prefetch(reinterpret_cast<const char*>(pC + i + 64), _MM_HINT_NTA);
			_mm_prefetch(reinterpret_cast<const char*>(pP + i + 64), _MM_HINT_NTA);
			c = _mm_load_si128(reinterpret_cast<const __m128i *>(pC + i));
			p = _mm_load_si128(reinterpret_cast<const __m128i *>(pP + i));
			cx = _mm_unpacklo_epi8(c, zero);
			px = _mm_unpacklo_epi8(p, zero);
			cx = _mm_sub_epi16(cx, px);
			_mm_stream_si128(reinterpret_cast<__m128i *>(bufP0 + i), cx);
			cx = _mm_unpackhi_epi8(c, zero);
			px = _mm_unpackhi_epi8(p, zero);
			cx = _mm_sub_epi16(cx, px);
			_mm_stream_si128(reinterpret_cast<__m128i *>(bufP0 + i + 8), cx);
		}

		//lea rax, bufP0
		//lea rdi, bufP1
		//mov esi, 8
		//align 16
		for (i = 8; i < widthminus8; i += 8) {
			_mm_prefetch(reinterpret_cast<const char*>(bufP0 + i + 64), _MM_HINT_NTA);
			//prefetchnta[rax + rsi + 16]
			auto mmA = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bufP0 + i - 1));
			//movq mm0, [rax + rsi * 2 - 2]
			auto mmC = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bufP0 + i + 1));
			auto delta = _mm_add_epi16(mmA, mmC);
			//paddw mm0, [rax + rsi * 2 + 2]
			auto mmB = _mm_load_si128(reinterpret_cast<const __m128i*>(bufP0 + i));
			//movq mm2, [rax + rsi * 2]
			delta = _mm_sub_epi16(delta, mmB);
			//psubw mm0, mm2
			delta = _mm_sub_epi16(delta, mmB);
			//psubw mm0, mm2
			mmB = _mm_abs_epi16(mmB);
			//movq mm3, mm7  |
			//psubw mm3, mm2  }-> abs(mm3)
			//pmaxsw mm2, mm3|
			delta = _mm_abs_epi16(delta);
			//movq mm1, mm7  |
			//psubw mm1, mm0  }-> abs(mm1)
			//pmaxsw mm0, mm1|
			//lea esi, [esi + 4]
			mmB = _mm_subs_epu16(mmB, delta);
			//psubusw mm2, mm0
			//cmp esi, widthminus8
			mmB = _mm_packus_epi16(mmB, zero);
			//packuswb mm2, mm7
			_mm_storel_epi64(reinterpret_cast<__m128i*>(bufP1 + i), mmB);
			//movd[rdi + rsi - 4], mm2
			//jl loopB
		}
		//pxor mm7, mm7
		//movq mm5, mbTh

		//lea rax, bufP1
		//mov esi, 16

		//pxor mm4, mm4
		//pxor mm3, mm3
		auto msum = zero;
		auto msum1 = zero;
		//align 16
		for (i = 16; i < widthminus16; i += 16) {
			_mm_prefetch(reinterpret_cast<const char*>(bufP1 + i + 64), _MM_HINT_NTA);
			//prefetchnta[rax + rsi + 16]
			auto mmA = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bufP1 + i - 1));
			//movq mm0, [rax + rsi - 1]
			auto mmB = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bufP1 + i + 1));
			auto mmABC = _mm_adds_epu8(mmA, mmB);
			//paddusb mm0, [rax + rsi + 1]
			auto mmC = _mm_load_si128(reinterpret_cast<const __m128i*>(bufP1 + i));
			mmABC = _mm_adds_epu8(mmABC, mmC);
			//paddusb mm0, [rax + rsi]
			auto mmABC2 = mmABC;
			//movq mm1, mm0
			mmABC = _mm_subs_epu8(mmABC, mmbTh);
			//psubusb mm0, mm5
			mmABC2 = _mm_subs_epu8(mmABC2, mmbTh2);
			//psubusb mm1, mbTh2
			mmABC = _mm_cmpeq_epi8(mmABC, zero);
			//pcmpeqb mm0, mm7
			mmABC2 = _mm_cmpeq_epi8(mmABC2, zero);
			//pcmpeqb mm1, mm7
			mmABC = _mm_cmpeq_epi8(mmABC, zero);
			//pcmpeqb mm0, mm7
			mmABC2 = _mm_cmpeq_epi8(mmABC2, zero);
			//pcmpeqb mm1, mm7

			//lea esi, [esi + 8]
			mmABC = _mm_and_si128(mmABC, mmask1);
			//pand mm0, mask1
			mmABC2 = _mm_and_si128(mmABC2, mmask1);
			//pand mm1, mask1
			//cmp esi, widthminus16
			msum = _mm_add_epi8(msum, mmABC);
			//paddb mm4, mm0
			msum1 = _mm_add_epi8(msum1, mmABC2);
			//paddb mm3, mm1
			//jl loopC
		}

		//psadbw mm4, mm7
		int tsum = _mm_sum_epu8(msum);
		//movd tsum, mm4

		//psadbw mm3, mm7
		int tsum1 = _mm_sum_epu8(msum1);
		//movd tsum1, mm3

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

__forceinline __m128i make_motion_map2_asm(
	const unsigned char * eax,
	const unsigned char * ebx,
	int i) {
	auto mma = _mm_load_si128(reinterpret_cast<const __m128i*>(eax + i));
	auto mmb = _mm_load_si128(reinterpret_cast<const __m128i*>(ebx + i));
	return _mm_subs_abs_epu8(mma, mmb);
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

		//_asm {
		//mov rdi, pD
		//xor esi, esi
		//align 16
		//loopA:
		for (int i = 0; i < twidth; i += 16) { // unrolling to twice a time, low/high
			///P
			auto yl = make_motion_map2_asm(pC, pP, i * 2);
			auto yh = make_motion_map2_asm(pC, pP, i * 2 + 16);
			//MAKE_MOTION_MAP2_ASM_INIT(pC, pP)
			//MAKE_MOTION_MAP2_ASM(mm0, 2)

			auto u = make_motion_map2_asm(pC_U, pP_U, i);
			//MAKE_MOTION_MAP2_ASM_INIT(pC_U, pP_U)
			//MAKE_MOTION_MAP2_ASM(mm3, 1)

			auto v = make_motion_map2_asm(pC_V, pP_V, i);
			//MAKE_MOTION_MAP2_ASM_INIT(pC_V, pP_V)
			//MAKE_MOTION_MAP2_ASM(mm4, 1)

			auto uv = _mm_max_epu8(u, v);
			//pmaxub mm3, mm4
			auto uvl = _mm_unpacklo_epi8(uv, uv);
			auto uvh = _mm_unpackhi_epi8(uv, uv);
			//punpcklbw mm3, mm3
			auto pl = _mm_max_epu8(uvl, yl);
			auto ph = _mm_max_epu8(uvh, yh);
			//pmaxub mm0, mm3

			///N
			yl = make_motion_map2_asm(pC, pN, i * 2);
			yh = make_motion_map2_asm(pC, pN, i * 2 + 16);
			//MAKE_MOTION_MAP2_ASM_INIT(pC, pN)
			//MAKE_MOTION_MAP2_ASM(mm5, 2)

			u = make_motion_map2_asm(pC_U, pN_U, i);
			//MAKE_MOTION_MAP2_ASM_INIT(pC_U, pN_U)
			//MAKE_MOTION_MAP2_ASM(mm3, 1)

			v = make_motion_map2_asm(pC_V, pN_V, i);
			//MAKE_MOTION_MAP2_ASM_INIT(pC_V, pN_V)
			//MAKE_MOTION_MAP2_ASM(mm4, 1)

			uv = _mm_max_epu8(u, v);
			//pmaxub mm3, mm4
			uvl = _mm_unpacklo_epi8(uv, uv);
			uvh = _mm_unpackhi_epi8(uv, uv);
			//punpcklbw mm3, mm3
			auto nl = _mm_max_epu8(uvl, yl);
			auto nh = _mm_max_epu8(uvh, yh);
			//pmaxub mm5, mm3

			auto mm0l = _mm_max_epu8(pl, nl);
			auto mm0h = _mm_max_epu8(ph, nh);
			//pmaxub mm0, mm5

			//lea esi, [esi + 4]
			//cmp esi, twidth
			_mm_stream_si128(reinterpret_cast<__m128i*>(pD + i * 2), mm0l);
			_mm_stream_si128(reinterpret_cast<__m128i*>(pD + i * 2 + 16), mm0h);
			//movntq[rdi + rsi * 2 - 8], mm0
			//jl loopA
		}
	}
	// USE_MMX2;
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
		// mov rax, pC
		// mov rbx, pT
		// mov rcx, pB
		// mov rdi, pD
		// xor esi, esi
		// align 16
		// loopA:
		for (int i = 0; i < twidth; i += 16) {
			auto c = _mm_load_si128(reinterpret_cast<const __m128i*>(pC + i));
			auto t = _mm_load_si128(reinterpret_cast<const __m128i*>(pT + i));
			auto b = _mm_load_si128(reinterpret_cast<const __m128i*>(pB + i));
			// movq mm0, [rax + rsi]
			// movq mm1, [rbx + rsi]
			// movq mm2, mm0
			// movq mm3, mm1
			auto ct = _mm_subs_abs_epu8(c, t);
			// MAKE_BLUR_MAP_ASM(mm0, mm1)

			// movq mm4, [rcx + rsi]
			// movq mm1, mm4
			auto cb = _mm_subs_abs_epu8(c, b);
			// MAKE_BLUR_MAP_ASM(mm2, mm4)

			auto tb = _mm_subs_abs_epu8(t, b);
			// MAKE_BLUR_MAP_ASM(mm3, mm1)

			auto delta = _mm_adds_epu8(ct, cb);
			// paddusb mm0, mm2
			delta = _mm_subs_epu8(delta, tb);
			// psubusb mm0, mm3
			delta = _mm_subs_epu8(delta, tb);
			// psubusb mm0, mm3

			// lea esi, [esi + 8]
			// cmp esi, twidth
			_mm_stream_si128(reinterpret_cast<__m128i*>(pD + i), delta);
			// movntq[rdi + rsi - 8], mm0
			// jl loopA
		}
	}
	// USE_MMX2;
	if (srcC != srcR)
		env->FreeFrame(srcR);
	env->FreeFrame(srcC);
}
#endif
