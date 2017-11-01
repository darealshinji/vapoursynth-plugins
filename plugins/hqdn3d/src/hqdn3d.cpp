/*
    HQDN3D 1.00 for Vapoursynth

    Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
    Avisynth port (C) 2005 Loren Merritt <lorenm@u.washington.edu>
    Vapoursynth port (C) 2017 Martin Güthle  <mguethle@xunit.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <cstdint>
#include <algorithm>

#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>

typedef struct Hqdn3dData {
	VSNodeRef *clip;
	const VSVideoInfo *vi;

	double lumSpac   = -1.0;
	double chromSpac = -1.0;
	double lumTmp    = -1.0;
	double chromTmp  = -1.0;
	int restartLap   = -1;

	int coefs[4][512*16];
	int planeCnt     = -1;
	unsigned int ** prevFrame = nullptr;
	unsigned int ** prevLine  = nullptr;
} Hqdn3dData;

static void
VS_CC hqdn3dFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
	(void)core;

	Hqdn3dData *d = (Hqdn3dData *)instanceData;
	if (!d) {
		return;
	}

	for (int p = 0; p < d->planeCnt; p++) {
		free(d->prevLine[p]);
		free(d->prevFrame[p]);
	}
	free(d->prevFrame);
	free(d->prevLine);
	vsapi->freeNode(d->clip);
}

static inline unsigned int
LowPassMul(unsigned int pMul, unsigned int cMul, int* coef){
	static const unsigned int ROUND_CONVOLUTION = 0x10007FF;
	static const unsigned int SHIFT_CONVOLUTION = 12;
	int d = (static_cast<int>(pMul - cMul) + ROUND_CONVOLUTION) >> SHIFT_CONVOLUTION;
	return cMul + coef[d];
}

static void
deNoise(
	  const uint8_t * srcPlane
	, unsigned int * prevPlane
	, unsigned int * prevLine
	, uint8_t * tarPlane
	, const int frameWidth
	, const int frameHeight
	, const int srcStride
	, const int tarStride
	, int *coefsHorizontal
	, int *coefsVertical
	, int *coefsTemporal
	, const bool isFirstFrame
) {
	static const unsigned int ROUND_LINE  = 0x1000007F;
	static const unsigned int SHIFT_LINE  = 8;
	static const unsigned int ROUND_PIXEL = 0x10007FFF;
	static const unsigned int SHIFT_PIXEL = 16;

	for (int row = 0; row < frameHeight; ++row) {
		/* gcc assume prevPixel might be used in an uninitialized way, but
		 * it's not. So feel free to use any other value
		 */
		unsigned int prevPixel = 0;
		for (int col = 0; col < frameWidth; ++col) {
			// Correlate current pixel with previous pixel
			prevPixel = col == 0
				? srcPlane[row * srcStride + col] << SHIFT_PIXEL
				: LowPassMul(
					  prevPixel
					, srcPlane[row * srcStride + col] << SHIFT_PIXEL
					, coefsHorizontal
				);
			// Correlate previous line with previous pixel
			prevLine[col] = row == 0
				? prevPixel
				: LowPassMul(
					  prevLine[col]
					, prevPixel
					, coefsVertical
				);
			unsigned int resPix;
			if (isFirstFrame) {
				resPix = prevLine[col];
			} else {
				// Correlate vertical result with previous result frame pixel
				resPix = LowPassMul(
					  prevPlane[row * frameWidth + col] << SHIFT_LINE
					, prevLine[col]
					, coefsTemporal
				);
			}

			prevPlane[row * frameWidth + col]
				= ((resPix + ROUND_LINE) >> SHIFT_LINE) & 0xFFFF;
			tarPlane[row * tarStride + col]
				= (resPix + ROUND_PIXEL) >> SHIFT_PIXEL;
		}
	}
}

static const VSFrameRef *VS_CC hqdn3dGetFrame(
	  int n
	, int activationReason
	, void **instanceData
	, void **frameData
	, VSFrameContext *frameCtx
	, VSCore *core
	, const VSAPI *vsapi
) {
	(void)frameData;

	// Get the user data
	Hqdn3dData * usrData = reinterpret_cast<Hqdn3dData *>(*instanceData);

	if (activationReason == arInitial) {
		vsapi->requestFrameFilter(std::max(0, n - 1), usrData->clip, frameCtx);
		vsapi->requestFrameFilter(n, usrData->clip, frameCtx);
		vsapi->requestFrameFilter(
			  std::min(n + 1, usrData->vi->numFrames - 1)
			, usrData->clip
			, frameCtx);
		return nullptr;
	}
	if (activationReason != arAllFramesReady) {
		return nullptr;
	}

	// Get current frame
	const VSFrameRef * srcFrame =
		vsapi->getFrameFilter(n, usrData->clip, frameCtx);

	// Get current frame format
	const VSFormat *srcFrameFmt = vsapi->getFrameFormat(srcFrame);

	// Create the temporary storages, if needed
	if (usrData->prevFrame == nullptr) {
		auto const planeCnt = srcFrameFmt->numPlanes;
		usrData->planeCnt   = planeCnt;
		usrData->prevFrame  = (unsigned int **)malloc(planeCnt * sizeof(unsigned int *));
		usrData->prevLine   = (unsigned int **)malloc(planeCnt * sizeof(unsigned int *));
		for (int p = 0; p < usrData->planeCnt; p++) {
			usrData->prevFrame[p] = (unsigned int *)malloc(
				vsapi->getFrameWidth(srcFrame, p)
				* vsapi->getFrameHeight(srcFrame, p)
				* sizeof(unsigned int));
			usrData->prevLine[p] = (unsigned int *)malloc(
				vsapi->getFrameWidth(srcFrame, p)
				* sizeof(unsigned int));
		}
	}


	// Create target frame
	VSFrameRef *newFrame = vsapi->newVideoFrame(
		  srcFrameFmt
		, vsapi->getFrameWidth( srcFrame, 0)
		, vsapi->getFrameHeight(srcFrame, 0)
		, srcFrame
		, core
	);

	for (int plane = 0; plane < srcFrameFmt->numPlanes; plane++) {
		deNoise(
			  vsapi->getReadPtr(srcFrame, plane)
			, usrData->prevFrame[plane]
			, usrData->prevLine[plane]
			, vsapi->getWritePtr(newFrame, plane)
			, vsapi->getFrameWidth(srcFrame, plane)
			, vsapi->getFrameHeight(srcFrame, plane)
			, vsapi->getStride(srcFrame, plane)
			, vsapi->getStride(newFrame, plane)
			, usrData->coefs[plane == 0 ? 0 : 2] // Y or U/V
			, usrData->coefs[plane == 0 ? 0 : 2] // Y or U/V
			, usrData->coefs[plane == 0 ? 1 : 3] // Y or U/V
			, n == 0
		);
	}
	return newFrame;
}


static void VS_CC hqdn3dInit(
	  VSMap *in
	, VSMap *out
	, void **instanceData
	, VSNode *node
	, VSCore *core
	, const VSAPI *vsapi
) {
	(void)in;
	(void)out;
	(void)core;

	Hqdn3dData *d = (Hqdn3dData *) * instanceData;

	vsapi->setVideoInfo(d->vi, 1, node);
}

// Create the plugin
static void VS_CC hqdn3dCreate(
	  const VSMap *in
	, VSMap *out
	, void *userData
	, VSCore *core
	, const VSAPI *vsapi
) {
	(void)userData;

	Hqdn3dData d;
	int err;

	d.lumSpac    = vsapi->propGetFloat(in, "lum_spac",    0, &err);
	d.chromSpac  = vsapi->propGetFloat(in, "chrom_spac",  0, &err);
	d.lumTmp     = vsapi->propGetFloat(in, "lum_tmp",     0, &err);
	d.chromTmp   = vsapi->propGetFloat(in, "chrom_tmp",   0, &err);
	d.restartLap = vsapi->propGetInt(  in, "restart_lap", 0, &err);

	d.clip = vsapi->propGetNode(in, "clip", 0, NULL);
	d.vi = vsapi->getVideoInfo(d.clip);

	//TODO check for colorspace necessary?
	//if(!vi.IsYV12())

	// Correct the given values
	if (d.lumSpac < 0) {
		d.lumSpac = 4.0;
	}
	if (d.chromSpac < 0) {
		d.chromSpac = .75 * d.lumSpac;
	}
	if (d.lumTmp < 0) {
		d.lumTmp = 1.5 * d.lumSpac;
	}
	if (d.chromTmp < 0) {
		d.chromTmp = d.lumSpac == 0
			? d.chromSpac * 1.5
			: d.lumTmp * d.chromSpac / d.lumSpac;
	}
	d.lumSpac   = std::min(254.9, d.lumSpac);
	d.chromSpac = std::min(254.9, d.chromSpac);
	d.lumTmp    = std::min(254.9, d.lumTmp);
	d.chromTmp  = std::min(254.9, d.chromTmp);

	if (d.restartLap < 0) {
		d.restartLap = std::max(2
			, static_cast<int>(1 + std::max(d.lumTmp, d.chromTmp)));
	}

	// Calculate the coefficients
	for (auto const cc : {
		  std::make_pair(0, d.lumSpac)
		, std::make_pair(1, d.lumTmp)
		, std::make_pair(2, d.chromSpac)
		, std::make_pair(3, d.chromTmp)
	} ) {
		const double gamma = log(0.25) / log(1.0 - cc.second / 255.0 - 0.00001);
		for (int i = -255 * 16; i < 256 * 16; ++i) {
			const double simil = 1.0 - std::abs(i) / (16*255.0);
			const double c = pow(simil, gamma) * 65536.0 * i / 16.0;
			d.coefs[cc.first][16*256+i]
				= static_cast<int>(c < 0 ? c - 0.5 : c + 0.5);
		}
	}

	Hqdn3dData * data = (Hqdn3dData *)malloc(sizeof(Hqdn3dData));
	*data = d;

	vsapi->createFilter(
		  in
		, out
		, "hqdn3d"
		, hqdn3dInit
		, hqdn3dGetFrame
		, hqdn3dFree
		, 0 /* No flags set */
		, 0
		, data
		, core
	);
}

VS_EXTERNAL_API(void) VapourSynthPluginInit(
	  VSConfigPlugin configFunc
	, VSRegisterFunction registerFunc
	, VSPlugin *plugin
) {
	configFunc(
		  "com.vapoursynth.hqdn3d"
		, "hqdnd"
		, "HQDn3D port as used in avisynth/mplayer"
		, VAPOURSYNTH_API_VERSION
		, 1
		, plugin
	);
	registerFunc(
		  "hqdn3d"
		, "clip:clip;"
		  "lum_spac:float:opt;"
		  "chrom_spac:float:opt;"
		  "lum_tmp:float:opt;"
		  "chrom_tmp:float:opt;"
		  "restart_lap:float:opt;"
		, hqdn3dCreate
		, 0
		, plugin
	);
}
