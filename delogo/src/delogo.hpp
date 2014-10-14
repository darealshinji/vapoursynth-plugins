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

#pragma once
#include "delogo_interface.h"
#include "IScriptEnvironment.h"
#include "logo.h"

typedef struct {
	short dp, c;
} LOCAL_LOGO_PIXEL;

class delogo
{
	const char* m_logofile;
	const char* m_logoname;
	int m_pos_x;
	int m_pos_y;
	int m_depth;
	int m_yc_y;
	int m_yc_u;
	int m_yc_v;
	int m_start;
	int m_end;
	int m_fadein;
	int m_fadeout;
	int m_cutoff;
	int m_mode;
	LOGO_HEADER m_lgh;
	LOCAL_LOGO_PIXEL* m_lgd;

public:
	VSVideoInfo* vi;
	VSNodeRef* node;

	delogo(const VSAPI *vsapi,
		VSVideoInfo* vi,
		VSNodeRef* node,
		const char* logofile,
		const char* logoname,
		int pos_x,
		int pos_y,
		int depth,
		int yc_y,
		int yc_u,
		int yc_v,
		int start,
		int end,
		int fadein,
		int fadeout,
		int cutoff,
		int mode);
	~delogo();
	void GetFramePre(IScriptEnvironment*env, int n) { env->PrefetchFrame(n); }

	const VSFrameRef* GetFrame(IScriptEnvironment*env, int n){
		if (m_mode == -1)
			return GetFrameErase(n, env);
		else
			return GetFrameAdd(n, env);
	}
	const VSFrameRef* GetFrameAdd(int n, IScriptEnvironment *env);
	const VSFrameRef* GetFrameErase(int n, IScriptEnvironment *env);
	LOGO_PIXEL* ReadLogoData();
	LOGO_PIXEL* AdjustLogo(LOGO_PIXEL* lgd);
	LOGO_PIXEL* ColorTuning(LOGO_PIXEL* lgd);
	LOGO_PIXEL* AlphaCutoff(LOGO_PIXEL* lgd);
	LOCAL_LOGO_PIXEL * Convert(LOGO_PIXEL *src, LOGO_HEADER &m_lgh);

	/// フェードによる深度計算
	int CalcFade(int n)
	{
		if (n < m_start || (m_end < n && m_end >= m_start)){	// 範囲外
			return 0;
		}
		if (n < m_start + m_fadein){			// フェードイン
			return ((n - m_start) * 2 + 1)*LOGO_FADE_MAX / (m_fadein * 2);
		}
		else if (n > m_end - m_fadeout && m_end >= 0){		// フェードアウト
			return ((m_end - n) * 2 + 1)*LOGO_FADE_MAX / (m_fadeout * 2);
		}
		// 通常
		return LOGO_FADE_MAX;
	}

	int Abs(int x){ return abs(x); }
	int Clamp(int n, int l, int h){ return VSMIN(VSMAX(n, l), h); }

	int YtoAUY(int y){ return ((y * 1197) >> 6) - 299; }
	int CtoAUC(int c){ return (c * 4681 - 128 * 4681 + 164) >> 8; }
	int AUYtoY(int y){ return Clamp((y * 219 + 383 + (16 << 12)) >> 12, 0, 255); }
	int AUCtoC(int c){ return Clamp((c * 7 + 2048 * 7 + 66 + (16 << 7)) >> 7, 0, 255); }
	int min(int a, int b) { return VSMIN(a, b); }
	int max(int a, int b) { return VSMAX(a, b); }
};

