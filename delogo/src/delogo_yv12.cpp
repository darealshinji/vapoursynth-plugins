/*
VS_DELOGO Copyright(C) 2003 MakKi, 2014 msg7086

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

#include "logo.h"
#include "delogo.hpp"

static void ConvertUV(LOCAL_LOGO_PIXEL * dst, LOGO_PIXEL * src, const LOGO_HEADER & m_lgh,
                      bool oddx, bool oddy, bool oddw, bool oddh,
                      int(* DP)(LOGO_PIXEL *), int(* Cx)(LOGO_PIXEL *)) {
	const int srcw = m_lgh.w - (oddx ? 1 : 0) - (oddw ? 1 : 0);
	const int srch = m_lgh.h - (oddy ? 1 : 0) - (oddh ? 1 : 0);
	const int srcw_r = (srcw - (oddx ? 1 : 0) - (oddw ? 1 : 0)) / 2 + (oddw ? 1 : 0); // one more if oddw=t
	const int srch_r = (srch - (oddy ? 1 : 0) - (oddh ? 1 : 0)) / 2;
	LOGO_PIXEL * s1 = src;

	if (oddy) {
		if (oddx) {
			dst->dp = 0;
			++dst;
			++s1;
		}
		for (int j = srcw_r; j; --j) {
			dst->dp = (DP(s1) + 1) / 2;
			dst->c = Cx(s1);
			++dst;
			s1 += 2;
		}
		if (oddw) { // Move back if move too much
			--s1;
		}
	}
	LOGO_PIXEL * s2 = s1 + srcw;
	for (int i = srch_r; i; --i) {
		if (oddx) {
			dst->dp = 0;
			++dst;
			++s1;
			++s2;
		}
		for (int j = srcw_r; j; --j) {
			int dp = DP(s1) + DP(s2);
			dst->dp = (dp + 1) / 2;
			if (dp) {
				dst->c = (Cx(s1) * DP(s1)
					+ Cx(s2) * DP(s2) + (dp + 1) / 2) / dp;
			}
			++dst;
			s1 += 2;
			s2 += 2;
		}
		if (oddw) {
			--s1;
			--s2;
		}
		s1 += srcw;
		s2 += srcw;
	}
	if (oddh) {
		if (oddx) {
			dst->dp = 0;
			++dst;
			++s1;
		}
		for (int j = srcw_r; j; --j) {
			dst->dp = (DP(s1) + 1) / 2;
			dst->c = Cx(s1);
			++dst;
			s1 += 2;
		}
	}
}

static void ConvertU(LOCAL_LOGO_PIXEL * dst, LOGO_PIXEL * src, const LOGO_HEADER & m_lgh, bool oddx, bool oddy, bool oddw, bool oddh) {
	ConvertUV(dst, src, m_lgh, oddx, oddy, oddw, oddh,
	          [](LOGO_PIXEL * p)->int { return p->dp_cb; },
	          [](LOGO_PIXEL * p)->int { return p->cb; }
	);
}

static void ConvertV(LOCAL_LOGO_PIXEL * dst, LOGO_PIXEL * src, const LOGO_HEADER & m_lgh, bool oddx, bool oddy, bool oddw, bool oddh) {
	ConvertUV(dst, src, m_lgh, oddx, oddy, oddw, oddh,
	          [](LOGO_PIXEL * p)->int { return p->dp_cr; },
	          [](LOGO_PIXEL * p)->int { return p->cr; }
	);
}

static void ConvertY(LOCAL_LOGO_PIXEL * dst, LOGO_PIXEL * src, const LOGO_HEADER & m_lgh, bool oddx, bool oddy, bool oddw, bool oddh) {
	const int w = m_lgh.w;
	const int srcw = w - (oddx ? 1 : 0) - (oddw ? 1 : 0);
	const int srch = m_lgh.h - (oddy ? 1 : 0) - (oddh ? 1 : 0);
	if (oddy) {
		for (int i = w; i; --i) {
			dst->dp = 0;
			++dst;
		}
	}
	for (int i = srch; i; --i) {
		if (oddx) {
			dst->dp = 0;
			++dst;
		}
		for (int j = srcw; j; --j) {
			dst->dp = src->dp_y;
			dst->c = src->y;
			++src;
			++dst;
		}
		if (oddw) {
			dst->dp = 0;
			++dst;
		}
	}
	if (oddh) {
		for (int i = w; i; --i) {
			dst->dp = 0;
			++dst;
		}
	}
}

LOCAL_LOGO_PIXEL* delogo::Convert(LOGO_PIXEL * src, LOGO_HEADER & m_lgh) {
	bool oddx = m_lgh.x % 2 != 0;
	if (oddx) {
		m_lgh.x -= 1;
		m_lgh.w += 1;
	}
	bool oddy = m_lgh.y % 2 != 0;
	if (oddy) {
		m_lgh.y -= 1;
		m_lgh.h += 1;
	}
	bool oddw = m_lgh.w % 2 != 0;
	if (oddw) {
		m_lgh.w += 1;
	}
	bool oddh = m_lgh.h % 2 != 0;
	if (oddh) {
		m_lgh.h += 1;
	}

	LOCAL_LOGO_PIXEL * p = new LOCAL_LOGO_PIXEL[m_lgh.w * m_lgh.h * 3 / 2];
	if (p == NULL) {
		throw "Failed in memory allocation.";
	}
	LOCAL_LOGO_PIXEL * dst = p;

	// Y
	ConvertY(dst, src, m_lgh, oddx, oddy, oddw, oddh);
	dst += m_lgh.w * m_lgh.h;
	ConvertU(dst, src, m_lgh, oddx, oddy, oddw, oddh);
	dst += m_lgh.w * m_lgh.h / 4; // FIXME: Support YUV444 etc.
	ConvertV(dst, src, m_lgh, oddx, oddy, oddw, oddh);

	return p;
}

/* GetFrame */
const VSFrameRef* delogo::GetFrameErase(int n, IScriptEnvironment * env) {
	const VSFrameRef * srcframe(env->GetFrame(n));
	int fade = CalcFade(n);
	if (fade == 0) return srcframe;

	VSFrameRef * frame = env->MakeWritable(srcframe);
	env->FreeFrame(srcframe);

	// Logo->xywh, frame->wh, all even number
	int logo_w = VSMIN(m_lgh.w, env->GetRowSize(frame) - m_lgh.x);
	int logo_h = VSMIN(m_lgh.h, env->GetHeight(frame) - m_lgh.y);
	int dst_x = m_lgh.x;
	int dst_y = m_lgh.y;
	int logo_x = 0;
	int logo_y = 0;
	if (dst_x < 0) {
		logo_x = -dst_x;
		logo_w -= logo_x;
		dst_x = 0;
	}
	if (dst_y < 0) {
		logo_y = -dst_y;
		logo_h -= logo_y;
		dst_y = 0;
	}
	if (logo_w <= 0 || logo_h <= 0) return frame; // Out of frame

	//Y
	int dst_pitch = env->GetPitch(frame, PLANAR_Y);
	int dst_pitch_r = dst_pitch - logo_w;
	BYTE * dst = env->GetWritePtr(frame, PLANAR_Y) + dst_x + dst_y * dst_pitch;
	int logo_pitch = m_lgh.w;
	int logo_pitch_r = logo_pitch - logo_w;
	LOCAL_LOGO_PIXEL * lgp = m_lgd + logo_x + logo_y * m_lgh.w;
	int i, j;
	for (i = logo_h; i; --i) {
		for (j = logo_w; j; --j) {
			int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
			if (dp) {
				if (dp == LOGO_MAX_DP) --dp;
				*dst = AUYtoY((YtoAUY(*dst) * LOGO_MAX_DP - lgp->c * dp
					+ (LOGO_MAX_DP - dp) / 2) / (LOGO_MAX_DP - dp));
			}
			++dst;
			++lgp;
		}
		dst += dst_pitch_r;
		lgp += logo_pitch_r;
	}
	//U
	dst_pitch = env->GetPitch(frame, PLANAR_U);
	dst_pitch_r = dst_pitch - logo_w / 2;
	dst = env->GetWritePtr(frame, PLANAR_U) + (dst_x + dst_y * dst_pitch) / 2;
	logo_pitch = m_lgh.w / 2;
	logo_pitch_r = logo_pitch - logo_w / 2;
	lgp = m_lgd + (m_lgh.w * m_lgh.h) + (logo_x + logo_y * m_lgh.w) / 2;
	for (i = logo_h / 2; i; --i) {
		for (j = logo_w / 2; j; --j) {
			int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
			if (dp) {
				if (dp == LOGO_MAX_DP) --dp;
				*dst = AUCtoC((CtoAUC(*dst) * LOGO_MAX_DP - lgp->c * dp
					+ (LOGO_MAX_DP - dp) / 2) / (LOGO_MAX_DP - dp));
			}
			++dst;
			++lgp;
		}
		dst += dst_pitch_r;
		lgp += logo_pitch_r;
	}
	//V
	dst_pitch = env->GetPitch(frame, PLANAR_V);
	dst_pitch_r = dst_pitch - logo_w / 2;
	dst = env->GetWritePtr(frame, PLANAR_V) + (dst_x + dst_y * dst_pitch) / 2;
	lgp = m_lgd + (m_lgh.w * m_lgh.h) / 4 * 5 + (logo_x + logo_y * m_lgh.w) / 2;
	for (i = logo_h / 2; i; --i) {
		for (j = logo_w / 2; j; --j) {
			int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
			if (dp) {
				if (dp == LOGO_MAX_DP) --dp;
				*dst = AUCtoC((CtoAUC(*dst) * LOGO_MAX_DP - lgp->c * dp
					+ (LOGO_MAX_DP - dp) / 2) / (LOGO_MAX_DP - dp));
			}
			++dst;
			++lgp;
		}
		dst += dst_pitch_r;
		lgp += logo_pitch_r;
	}

	return frame;
}

const VSFrameRef* delogo::GetFrameAdd(int n, IScriptEnvironment * env) {
	const VSFrameRef * srcframe(env->GetFrame(n));
	int fade = CalcFade(n);
	if (fade == 0) return srcframe;

	VSFrameRef * frame = env->MakeWritable(srcframe);
	env->FreeFrame(srcframe);

	// Logo->xywh, frame->wh, all even number
	int logo_w = VSMIN(m_lgh.w, env->GetRowSize(frame) - m_lgh.x);
	int logo_h = VSMIN(m_lgh.h, env->GetHeight(frame) - m_lgh.y);
	int dst_x = m_lgh.x;
	int dst_y = m_lgh.y;
	int logo_x = 0;
	int logo_y = 0;
	if (dst_x < 0) {
		logo_x = -dst_x;
		logo_w -= logo_x;
		dst_x = 0;
	}
	if (dst_y < 0) {
		logo_y = -dst_y;
		logo_h -= logo_y;
		dst_y = 0;
	}
	if (logo_w <= 0 || logo_h <= 0) return frame; // Out of frame

	//Y
	int dst_pitch = env->GetPitch(frame, PLANAR_Y);
	int dst_pitch_r = dst_pitch - logo_w;
	BYTE * dst = env->GetWritePtr(frame, PLANAR_Y) + dst_x + dst_y * dst_pitch;
	int logo_pitch = m_lgh.w;
	int logo_pitch_r = logo_pitch - logo_w;
	LOCAL_LOGO_PIXEL * lgp = m_lgd + logo_x + logo_y * m_lgh.w;
	int i, j;
	for (i = logo_h; i; --i) {
		for (j = logo_w; j; --j) {
			int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
			if (dp) {
				*dst = AUYtoY((YtoAUY(*dst) * (LOGO_MAX_DP - dp) + lgp->c * dp
					+ LOGO_MAX_DP / 2) / LOGO_MAX_DP);
			}
			++dst;
			++lgp;
		}
		dst += dst_pitch_r;
		lgp += logo_pitch_r;
	}
	//U
	dst_pitch = env->GetPitch(frame, PLANAR_U);
	dst_pitch_r = dst_pitch - logo_w / 2;
	dst = env->GetWritePtr(frame, PLANAR_U) + (dst_x + dst_y * dst_pitch) / 2;
	logo_pitch = m_lgh.w / 2;
	logo_pitch_r = logo_pitch - logo_w / 2;
	lgp = m_lgd + (m_lgh.w * m_lgh.h) + (logo_x + logo_y * m_lgh.w) / 2;
	for (i = logo_h / 2; i; --i) {
		for (j = logo_w / 2; j; --j) {
			int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
			if (dp) {
				*dst = AUCtoC((CtoAUC(*dst) * (LOGO_MAX_DP - dp) + lgp->c * dp
					+ LOGO_MAX_DP / 2) / LOGO_MAX_DP);
			}
			++dst;
			++lgp;
		}
		dst += dst_pitch_r;
		lgp += logo_pitch_r;
	}
	//V
	dst_pitch = env->GetPitch(frame, PLANAR_V);
	dst_pitch_r = dst_pitch - logo_w / 2;
	dst = env->GetWritePtr(frame, PLANAR_V) + (dst_x + dst_y * dst_pitch) / 2;
	lgp = m_lgd + (m_lgh.w * m_lgh.h) * 5 / 4 + (logo_x + logo_y * m_lgh.w) / 2;
	for (i = logo_h / 2; i; --i) {
		for (j = logo_w / 2; j; --j) {
			int dp = (lgp->dp * fade + LOGO_FADE_MAX / 2) / LOGO_FADE_MAX;
			if (dp) {
				*dst = AUCtoC((CtoAUC(*dst) * (LOGO_MAX_DP - dp) + lgp->c * dp
					+ LOGO_MAX_DP / 2) / LOGO_MAX_DP);
			}
			++dst;
			++lgp;
		}
		dst += dst_pitch_r;
		lgp += logo_pitch_r;
	}

	return frame;
}