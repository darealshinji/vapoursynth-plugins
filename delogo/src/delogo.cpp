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

#include "delogo.hpp"

delogo::delogo(const VSAPI *vsapi,
	VSVideoInfo* _vi, VSNodeRef* _node,
	const char* logofile, const char* logoname,
	int pos_x, int pos_y, int depth,
	int yc_y, int yc_u, int yc_v,
	int start, int end,
	int fadein, int fadeout,
	int cutoff,
	int mode
	) : 
	m_logofile(logofile), m_logoname(logoname),
	m_pos_x(pos_x), m_pos_y(pos_y), m_depth(depth),
	m_yc_y(yc_y), m_yc_u(yc_u), m_yc_v(yc_v),
	m_start(start), m_end(end),
	m_fadein(fadein), m_fadeout(fadeout),
	m_cutoff(cutoff),
	m_mode(mode),
	vi(_vi), node(_node)
{
	// Load Logo
	LOGO_PIXEL* lgd = ReadLogoData();

	if (pos_x != 0 || pos_y != 0 || depth != LOGO_DEFAULT_DEPTH){
		lgd = AdjustLogo(lgd);
	}

	if (yc_y || yc_u || yc_v){
		lgd = ColorTuning(lgd);
	}

	if (cutoff > 0)
		lgd = AlphaCutoff(lgd);

	m_lgd = Convert(lgd, m_lgh);
}

delogo::~delogo()
{
}

LOGO_PIXEL* delogo::ReadLogoData() {
	if (m_logofile == NULL) throw "logo file not specified.";
	FILE* lfp;
	fopen_s(&lfp, m_logofile, "rb");
	if (!lfp) throw "unable to open logo file, wrong file name?";
	fseek(lfp, 0, SEEK_END);
	int flen = ftell(lfp);
	if (flen < int(sizeof(LOGO_HEADER)+LOGO_FILE_HEADER_STR_SIZE))
		throw "too small for a logo file, wrong file?";

	// Read header
	LOGO_FILE_HEADER lfh;
	size_t rbytes;
	fseek(lfp, 0, SEEK_SET);
	rbytes = fread(&lfh, sizeof(LOGO_FILE_HEADER), 1, lfp);
	if (!rbytes)
		throw "failed to read from logo file, disk error?";

	// Loop to read logo data
	unsigned long num = SWAP_ENDIAN(lfh.logonum.l);
	unsigned long i;
	for (i = 0; i < num; i++) {
		rbytes = fread(&m_lgh, sizeof(LOGO_HEADER), 1, lfp);
		if (!rbytes)
			throw "failed to read from logo file, disk error?";
		if (m_logoname == NULL || strcmp(m_logoname, m_lgh.name) == 0)
			break; // We find our logo
		// Skip the data block if not match
		fseek(lfp, LOGO_PIXELSIZE(&m_lgh), SEEK_CUR);
	}

	if (i == num) // So we couldn't find a match
		throw "unable to find a matching logo";

	// Now we can read it and return
	LOGO_PIXEL* lgd = new LOGO_PIXEL[m_lgh.h * m_lgh.w];
	if (lgd == NULL) throw "Unable to allocate memory";
	//fread(lgd, LOGO_PIXELSIZE(&m_lgh), 1, lfp);
	fclose(lfp);
	return lgd;
}

LOGO_PIXEL* delogo::AdjustLogo(LOGO_PIXEL* lgd) {
	int adjx, adjy;
	if (m_pos_x >= 0){
		m_lgh.x = m_lgh.x + int(m_pos_x / 4);
		adjx = m_pos_x % 4;
	}
	else{
		m_lgh.x = m_lgh.x + int((m_pos_x - 3) / 4);
		adjx = (4 + (m_pos_x % 4)) % 4;
	}
	if (m_pos_y >= 0){
		m_lgh.y = m_lgh.y + int(m_pos_y / 4);
		adjy = m_pos_y % 4;
	}
	else{
		m_lgh.y = m_lgh.y + int((m_pos_y - 3) / 4);
		adjy = (4 + (m_pos_y % 4)) % 4;
	}

	if (m_depth == LOGO_DEFAULT_DEPTH && adjx == 0 && adjy == 0) return lgd;

	int pitch = m_lgh.w;
	// 1/4単位調節のため、1増やす
	int w = ++m_lgh.w;
	int h = ++m_lgh.h;

	LOGO_PIXEL* dstdata;
	dstdata = new LOGO_PIXEL[(m_lgh.h + 1) * (m_lgh.w + 1)];
	if (dstdata == NULL){
		throw "Failed on memory allocation.";
	}

	LOGO_PIXEL *df = lgd;
	LOGO_PIXEL *ex = dstdata;
	int i, j;

	//----------------------------------------------------- 一番上の列
	ex[0].dp_y = df[0].dp_y *(4 - adjx)*(4 - adjy)*m_depth / 128 / 16;	// 左端
	ex[0].dp_cb = df[0].dp_cb*(4 - adjx)*(4 - adjy)*m_depth / 128 / 16;
	ex[0].dp_cr = df[0].dp_cr*(4 - adjx)*(4 - adjy)*m_depth / 128 / 16;
	ex[0].y = df[0].y;
	ex[0].cb = df[0].cb;
	ex[0].cr = df[0].cr;
	for (i = 1; i < w - 1; i++){									//中
		// Y
		ex[i].dp_y = (df[i - 1].dp_y*adjx*(4 - adjy)
			+ df[i].dp_y*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[i].dp_y)
			ex[i].y = (df[i - 1].y*Abs(df[i - 1].dp_y)*adjx*(4 - adjy)
			+ df[i].y * Abs(df[i].dp_y)*(4 - adjx)*(4 - adjy))
			/ (Abs(df[i - 1].dp_y)*adjx*(4 - adjy) + Abs(df[i].dp_y)*(4 - adjx)*(4 - adjy));
		// Cb
		ex[i].dp_cb = (df[i - 1].dp_cb*adjx*(4 - adjy)
			+ df[i].dp_cb*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[i].dp_cb)
			ex[i].cb = (df[i - 1].cb*Abs(df[i - 1].dp_cb)*adjx*(4 - adjy)
			+ df[i].cb * Abs(df[i].dp_cb)*(4 - adjx)*(4 - adjy))
			/ (Abs(df[i - 1].dp_cb)*adjx*(4 - adjy) + Abs(df[i].dp_cb)*(4 - adjx)*(4 - adjy));
		// Cr
		ex[i].dp_cr = (df[i - 1].dp_cr*adjx*(4 - adjy)
			+ df[i].dp_cr*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[i].dp_cr)
			ex[i].cr = (df[i - 1].cr*Abs(df[i - 1].dp_cr)*adjx*(4 - adjy)
			+ df[i].cr * Abs(df[i].dp_cr)*(4 - adjx)*(4 - adjy))
			/ (Abs(df[i - 1].dp_cr)*adjx*(4 - adjy) + Abs(df[i].dp_cr)*(4 - adjx)*(4 - adjy));
	}
	ex[i].dp_y = df[i - 1].dp_y * adjx *(4 - adjy)*m_depth / 128 / 16;	// 右端
	ex[i].dp_cb = df[i - 1].dp_cb* adjx *(4 - adjy)*m_depth / 128 / 16;
	ex[i].dp_cr = df[i - 1].dp_cr* adjx *(4 - adjy)*m_depth / 128 / 16;
	ex[i].y = df[i - 1].y;
	ex[i].cb = df[i - 1].cb;
	ex[i].cr = df[i - 1].cr;

	//----------------------------------------------------------- 中
	for (j = 1; j < h - 1; j++){
		// 輝度Y		//---------------------- 左端
		ex[j*w].dp_y = (df[(j - 1)*pitch].dp_y*(4 - adjx)*adjy
			+ df[j*pitch].dp_y*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[j*w].dp_y)
			ex[j*w].y = (df[(j - 1)*pitch].y*Abs(df[(j - 1)*pitch].dp_y)*(4 - adjx)*adjy
			+ df[j*pitch].y*Abs(df[j*pitch].dp_y)*(4 - adjx)*(4 - adjy))
			/ (Abs(df[(j - 1)*pitch].dp_y)*(4 - adjx)*adjy + Abs(df[j*pitch].dp_y)*(4 - adjx)*(4 - adjy));
		// 色差(青)Cb
		ex[j*w].dp_cb = (df[(j - 1)*pitch].dp_cb*(4 - adjx)*adjy
			+ df[j*pitch].dp_cb*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[j*w].dp_cb)
			ex[j*w].cb = (df[(j - 1)*pitch].cb*Abs(df[(j - 1)*pitch].dp_cb)*(4 - adjx)*adjy
			+ df[j*pitch].cb*Abs(df[j*pitch].dp_cb)*(4 - adjx)*(4 - adjy))
			/ (Abs(df[(j - 1)*pitch].dp_cb)*(4 - adjx)*adjy + Abs(df[j*pitch].dp_cb)*(4 - adjx)*(4 - adjy));
		// 色差(赤)Cr
		ex[j*w].dp_cr = (df[(j - 1)*pitch].dp_cr*(4 - adjx)*adjy
			+ df[j*pitch].dp_cr*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[j*w].dp_cr)
			ex[j*w].cr = (df[(j - 1)*pitch].cr*Abs(df[(j - 1)*pitch].dp_cr)*(4 - adjx)*adjy
			+ df[j*pitch].cr*Abs(df[j*pitch].dp_cr)*(4 - adjx)*(4 - adjy))
			/ (Abs(df[(j - 1)*pitch].dp_cr)*(4 - adjx)*adjy + Abs(df[j*pitch].dp_cr)*(4 - adjx)*(4 - adjy));
		for (i = 1; i < w - 1; i++){	//------------------ 中
			// Y
			ex[j*w + i].dp_y = (df[(j - 1)*pitch + i - 1].dp_y*adjx*adjy
				+ df[(j - 1)*pitch + i].dp_y*(4 - adjx)*adjy
				+ df[j*pitch + i - 1].dp_y*adjx*(4 - adjy)
				+ df[j*pitch + i].dp_y*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
			if (ex[j*w + i].dp_y)
				ex[j*w + i].y = (df[(j - 1)*pitch + i - 1].y*Abs(df[(j - 1)*pitch + i - 1].dp_y)*adjx*adjy
				+ df[(j - 1)*pitch + i].y*Abs(df[(j - 1)*pitch + i].dp_y)*(4 - adjx)*adjy
				+ df[j*pitch + i - 1].y*Abs(df[j*pitch + i - 1].dp_y)*adjx*(4 - adjy)
				+ df[j*pitch + i].y*Abs(df[j*pitch + i].dp_y)*(4 - adjx)*(4 - adjy))
				/ (Abs(df[(j - 1)*pitch + i - 1].dp_y)*adjx*adjy + Abs(df[(j - 1)*pitch + i].dp_y)*(4 - adjx)*adjy
				+ Abs(df[j*pitch + i - 1].dp_y)*adjx*(4 - adjy) + Abs(df[j*pitch + i].dp_y)*(4 - adjx)*(4 - adjy));
			// Cb
			ex[j*w + i].dp_cb = (df[(j - 1)*pitch + i - 1].dp_cb*adjx*adjy
				+ df[(j - 1)*pitch + i].dp_cb*(4 - adjx)*adjy
				+ df[j*pitch + i - 1].dp_cb*adjx*(4 - adjy)
				+ df[j*pitch + i].dp_cb*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
			if (ex[j*w + i].dp_cb)
				ex[j*w + i].cb = (df[(j - 1)*pitch + i - 1].cb*Abs(df[(j - 1)*pitch + i - 1].dp_cb)*adjx*adjy
				+ df[(j - 1)*pitch + i].cb*Abs(df[(j - 1)*pitch + i].dp_cb)*(4 - adjx)*adjy
				+ df[j*pitch + i - 1].cb*Abs(df[j*pitch + i - 1].dp_cb)*adjx*(4 - adjy)
				+ df[j*pitch + i].cb*Abs(df[j*pitch + i].dp_cb)*(4 - adjx)*(4 - adjy))
				/ (Abs(df[(j - 1)*pitch + i - 1].dp_cb)*adjx*adjy + Abs(df[(j - 1)*pitch + i].dp_cb)*(4 - adjx)*adjy
				+ Abs(df[j*pitch + i - 1].dp_cb)*adjx*(4 - adjy) + Abs(df[j*pitch + i].dp_cb)*(4 - adjx)*(4 - adjy));
			// Cr
			ex[j*w + i].dp_cr = (df[(j - 1)*pitch + i - 1].dp_cr*adjx*adjy
				+ df[(j - 1)*pitch + i].dp_cr*(4 - adjx)*adjy
				+ df[j*pitch + i - 1].dp_cr*adjx*(4 - adjy)
				+ df[j*pitch + i].dp_cr*(4 - adjx)*(4 - adjy)) *m_depth / 128 / 16;
			if (ex[j*w + i].dp_cr)
				ex[j*w + i].cr = (df[(j - 1)*pitch + i - 1].cr*Abs(df[(j - 1)*pitch + i - 1].dp_cr)*adjx*adjy
				+ df[(j - 1)*pitch + i].cr*Abs(df[(j - 1)*pitch + i].dp_cr)*(4 - adjx)*adjy
				+ df[j*pitch + i - 1].cr*Abs(df[j*pitch + i - 1].dp_cr)*adjx*(4 - adjy)
				+ df[j*pitch + i].cr*Abs(df[j*pitch + i].dp_cr)*(4 - adjx)*(4 - adjy))
				/ (Abs(df[(j - 1)*pitch + i - 1].dp_cr)*adjx*adjy + Abs(df[(j - 1)*pitch + i].dp_cr)*(4 - adjx)*adjy
				+ Abs(df[j*pitch + i - 1].dp_cr)*adjx*(4 - adjy) + Abs(df[j*pitch + i].dp_cr)*(4 - adjx)*(4 - adjy));
		}
		// Y		//----------------------- 右端
		ex[j*w + i].dp_y = (df[(j - 1)*pitch + i - 1].dp_y*adjx*adjy
			+ df[j*pitch + i - 1].dp_y*adjx*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[j*w + i].dp_y)
			ex[j*w + i].y = (df[(j - 1)*pitch + i - 1].y*Abs(df[(j - 1)*pitch + i - 1].dp_y)*adjx*adjy
			+ df[j*pitch + i - 1].y*Abs(df[j*pitch + i - 1].dp_y)*adjx*(4 - adjy))
			/ (Abs(df[(j - 1)*pitch + i - 1].dp_y)*adjx*adjy + Abs(df[j*pitch + i - 1].dp_y)*adjx*(4 - adjy));
		// Cb
		ex[j*w + i].dp_cb = (df[(j - 1)*pitch + i - 1].dp_cb*adjx*adjy
			+ df[j*pitch + i - 1].dp_cb*adjx*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[j*w + i].dp_cb)
			ex[j*w + i].cb = (df[(j - 1)*pitch + i - 1].cb*Abs(df[(j - 1)*pitch + i - 1].dp_cb)*adjx*adjy
			+ df[j*pitch + i - 1].cb*Abs(df[j*pitch + i - 1].dp_cb)*adjx*(4 - adjy))
			/ (Abs(df[(j - 1)*pitch + i - 1].dp_cb)*adjx*adjy + Abs(df[j*pitch + i - 1].dp_cb)*adjx*(4 - adjy));
		// Cr
		ex[j*w + i].dp_cr = (df[(j - 1)*pitch + i - 1].dp_cr*adjx*adjy
			+ df[j*pitch + i - 1].dp_cr*adjx*(4 - adjy)) *m_depth / 128 / 16;
		if (ex[j*w + i].dp_cr)
			ex[j*w + i].cr = (df[(j - 1)*pitch + i - 1].cr*Abs(df[(j - 1)*pitch + i - 1].dp_cr)*adjx*adjy
			+ df[j*pitch + i - 1].cr*Abs(df[j*pitch + i - 1].dp_cr)*adjx*(4 - adjy))
			/ (Abs(df[(j - 1)*pitch + i - 1].dp_cr)*adjx*adjy + Abs(df[j*pitch + i - 1].dp_cr)*adjx*(4 - adjy));
	}
	//--------------------------------------------------------- 一番下
	ex[j*w].dp_y = df[(j - 1)*pitch].dp_y *(4 - adjx)*adjy *m_depth / 128 / 16;	// 左端
	ex[j*w].dp_cb = df[(j - 1)*pitch].dp_cb*(4 - adjx)*adjy *m_depth / 128 / 16;
	ex[j*w].dp_cr = df[(j - 1)*pitch].dp_cr*(4 - adjx)*adjy *m_depth / 128 / 16;
	ex[j*w].y = df[(j - 1)*pitch].y;
	ex[j*w].cb = df[(j - 1)*pitch].cb;
	ex[j*w].cr = df[(j - 1)*pitch].cr;
	for (i = 1; i < w - 1; i++){		// 中
		// Y
		ex[j*w + i].dp_y = (df[(j - 1)*pitch + i - 1].dp_y * adjx * adjy
			+ df[(j - 1)*pitch + i].dp_y * (4 - adjx) *adjy) *m_depth / 128 / 16;
		if (ex[j*w + i].dp_y)
			ex[j*w + i].y = (df[(j - 1)*pitch + i - 1].y*Abs(df[(j - 1)*pitch + i - 1].dp_y)*adjx*adjy
			+ df[(j - 1)*pitch + i].y*Abs(df[(j - 1)*pitch + i].dp_y)*(4 - adjx)*adjy)
			/ (Abs(df[(j - 1)*pitch + i - 1].dp_y)*adjx*adjy + Abs(df[(j - 1)*pitch + i].dp_y)*(4 - adjx)*adjy);
		// Cb
		ex[j*w + i].dp_cb = (df[(j - 1)*pitch + i - 1].dp_cb * adjx * adjy
			+ df[(j - 1)*pitch + i].dp_cb * (4 - adjx) *adjy) *m_depth / 128 / 16;
		if (ex[j*w + i].dp_cb)
			ex[j*w + i].cb = (df[(j - 1)*pitch + i - 1].cb*Abs(df[(j - 1)*pitch + i - 1].dp_cb)*adjx*adjy
			+ df[(j - 1)*pitch + i].cb*Abs(df[(j - 1)*pitch + i].dp_cb)*(4 - adjx)*adjy)
			/ (Abs(df[(j - 1)*pitch + i - 1].dp_cb)*adjx*adjy + Abs(df[(j - 1)*pitch + i].dp_cb)*(4 - adjx)*adjy);
		// Cr
		ex[j*w + i].dp_cr = (df[(j - 1)*pitch + i - 1].dp_cr * adjx * adjy
			+ df[(j - 1)*pitch + i].dp_cr * (4 - adjx) *adjy) *m_depth / 128 / 16;
		if (ex[j*w + i].dp_cr)
			ex[j*w + i].cr = (df[(j - 1)*pitch + i - 1].cr*Abs(df[(j - 1)*pitch + i - 1].dp_cr)*adjx*adjy
			+ df[(j - 1)*pitch + i].cr*Abs(df[(j - 1)*pitch + i].dp_cr)*(4 - adjx)*adjy)
			/ (Abs(df[(j - 1)*pitch + i - 1].dp_cr)*adjx*adjy + Abs(df[(j - 1)*pitch + i].dp_cr)*(4 - adjx)*adjy);
	}
	ex[j*w + i].dp_y = df[(j - 1)*pitch + i - 1].dp_y *adjx*adjy *m_depth / 128 / 16;	// 右端
	ex[j*w + i].dp_cb = df[(j - 1)*pitch + i - 1].dp_cb*adjx*adjy *m_depth / 128 / 16;
	ex[j*w + i].dp_cr = df[(j - 1)*pitch + i - 1].dp_cr*adjx*adjy *m_depth / 128 / 16;
	ex[j*w + i].y = df[(j - 1)*pitch + i - 1].y;
	ex[j*w + i].cb = df[(j - 1)*pitch + i - 1].cb;
	ex[j*w + i].cr = df[(j - 1)*pitch + i - 1].cr;

	delete[] lgd;
	return dstdata;
}

LOGO_PIXEL* delogo::ColorTuning(LOGO_PIXEL* lgd)
{
	for (int i = 0; i < m_lgh.h * m_lgh.w; i++)	{
		lgd[i].y += m_yc_y * 16;
		lgd[i].cb += m_yc_u * 16;
		lgd[i].cr += m_yc_v * 16;
	}
	return lgd;
}

LOGO_PIXEL* delogo::AlphaCutoff(LOGO_PIXEL* lgd)
{
	for (int i = 0; i < m_lgh.h * m_lgh.w; i++)	{
		if (lgd[i].dp_y < m_cutoff &&
			lgd[i].dp_cb < m_cutoff &&
			lgd[i].dp_cr < m_cutoff)
		{
			lgd[i].dp_y = lgd[i].dp_cb = lgd[i].dp_cr = 0;
		}
	}
	return lgd;
}
