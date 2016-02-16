//==============================================================================
// Copyright (c) 2014 Aleksey [Mystery Keeper] Lyashin (mystkeeper@gmail.com)
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//==============================================================================

#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h> //isConstantFormat()

#include "processplane.h"

#define UNUSED(x) (void)(x)

//==============================================================================

typedef enum
{
	stzInteger1B = 0,
	stzInteger2B,
	stzSingle
}
SampleTypeSize;

//==============================================================================
// Filter internal data structure

typedef struct tagTLAData
{
	VSNodeRef * inputNode;
	const VSVideoInfo * videoInfo;
	// Frames neighbourhood size
	int64_t radius;
	// Bit sum of planes to process
	uint32_t plane;
	// Bitdepth specific clamping value
	uint32_t maxInValue;
	// Type and size of video sample
	SampleTypeSize sampleTypeSize;
	// Gamma corrected to linear value conversion LUT
	double * lutGCToLinear;
	// Gamma correction flag
	int64_t gamma;
}
TLAData;

//==============================================================================
// Forward declarations

static void VS_CC TLAInit(VSMap * in, VSMap * out, void ** instanceData,
	VSNode * node, VSCore * core, const VSAPI * vsapi);

static const VSFrameRef * VS_CC TLAGetFrame(int n, int activationReason,
	void ** instanceData, void ** frameData, VSFrameContext * frameCtx,
	VSCore * core, const VSAPI * vsapi);

static void VS_CC TLAFree(void * instanceData, VSCore * core,
	const VSAPI * vsapi);

//==============================================================================

static void VS_CC TLACreate(const VSMap * in, VSMap * out, void * userData,
	VSCore * core, const VSAPI * vsapi)
{
	UNUSED(userData);

	TLAData internalData;

	internalData.inputNode = vsapi->propGetNode(in, "clip", 0, 0);
	internalData.videoInfo = vsapi->getVideoInfo(internalData.inputNode);

	//--------------------------------------------------------------------------
	// Format check

	const VSFormat * pFormat = internalData.videoInfo->format;

	int acceptableFormat = isConstantFormat(internalData.videoInfo) &&
		(internalData.videoInfo->numFrames != 0);
	if(!acceptableFormat)
	{
		vsapi->setError(out, "TempLinearApproximate: "
			"only constant format input with fixed frame number "
			"is supported.");
		vsapi->freeNode(internalData.inputNode);
		return;
	}

	if((pFormat->sampleType == stInteger) && (pFormat->bytesPerSample == 1))
		internalData.sampleTypeSize = stzInteger1B;
	else if((pFormat->sampleType == stInteger) &&
		(pFormat->bytesPerSample == 2))
		internalData.sampleTypeSize = stzInteger2B;
	else if((pFormat->sampleType == stFloat) &&
		(pFormat->bytesPerSample == 4))
		internalData.sampleTypeSize = stzSingle;
	else
	{
		vsapi->setError(out, "TempLinearApproximate: "
			"only integer and single precision input is supported.");
		vsapi->freeNode(internalData.inputNode);
		return;
	}

	uint32_t levels = 1u << pFormat->bitsPerSample;
	internalData.maxInValue = levels - 1;

	//--------------------------------------------------------------------------
	// Radius parameter

	int error = 0;

	internalData.radius = vsapi->propGetInt(in, "radius", 0, &error);
	if(error != 0)
		internalData.radius = 5;

	if(internalData.radius < 1)
	{
		vsapi->setError(out, "TempLinearApproximate: "
			"radius must be greater than 0.");
		vsapi->freeNode(internalData.inputNode);
		return;
	}

	//--------------------------------------------------------------------------
	// Planes parameter

	int64_t planesNumber = vsapi->propNumElements(in, "planes");
	if(planesNumber == -1)
	{
		// Planes array wasn't specified. Process all planes.
		internalData.plane = ~0u;
	}
	else
	{
		internalData.plane = 0u;
		int64_t processPlane;
		int i;
		for(i = 0; i < planesNumber; i++)
		{
			processPlane = vsapi->propGetInt(in, "planes", i, &error);
			if(error == 0)
				internalData.plane |= (1u << processPlane);
		}
	}

	//--------------------------------------------------------------------------
	// Gamma parameter

	internalData.gamma = vsapi->propGetInt(in, "gamma", 0, &error);
	if(error != 0)
		internalData.gamma = 1;

	//--------------------------------------------------------------------------
	// Fill LUT for integer formats

	internalData.lutGCToLinear = 0;
	if(internalData.gamma && (pFormat->sampleType == stInteger))
	{
		internalData.lutGCToLinear = (double *)malloc(levels * sizeof(double));
		uint32_t i;
		for(i = 0; i < levels; i++)
		{
			internalData.lutGCToLinear[i] = gcToLinear((double)i /
				(double)internalData.maxInValue);
		}
	}

	//--------------------------------------------------------------------------

	TLAData * pInternalData = malloc(sizeof(internalData));
	*pInternalData = internalData;
	vsapi->createFilter(in, out, "TempLinearApproximate", TLAInit, TLAGetFrame,
		TLAFree, fmParallel, 0, pInternalData, core);
}

//==============================================================================

static void VS_CC TLAInit(VSMap * in, VSMap * out, void ** instanceData,
	VSNode * node, VSCore * core, const VSAPI * vsapi)
{
	UNUSED(in);
	UNUSED(out);
	UNUSED(core);

	TLAData * pInternalData = (TLAData *) *instanceData;
	vsapi->setVideoInfo(pInternalData->videoInfo, 1, node);
}

//==============================================================================

// Free all allocated data on filter destruction
static void VS_CC TLAFree(void * instanceData, VSCore * core,
	const VSAPI * vsapi)
{
	UNUSED(core);

	TLAData * pInternalData = (TLAData *)instanceData;
	vsapi->freeNode(pInternalData->inputNode);
	if(pInternalData->lutGCToLinear)
		free(pInternalData->lutGCToLinear);
	free(pInternalData);
}

//==============================================================================

static const VSFrameRef * VS_CC TLAGetFrame(int n, int activationReason,
	void ** instanceData, void ** frameData, VSFrameContext * frameCtx,
	VSCore * core, const VSAPI * vsapi)
{
	UNUSED(frameData);

	TLAData * pInternalData = (TLAData *) *instanceData;
	int64_t radius = pInternalData->radius;
	int framesNumber = pInternalData->videoInfo->numFrames;

	// Guard against illegal frame number request.
	if(n < 0)
		n = 0;
	else if(n > framesNumber - 1)
		n = framesNumber - 1;

	// The arrays used for computations are always of size of (radius * 2 + 1).
	// The frame processed always takes the (radius) place - the middle of
	// array.
	// Since that frame might be near the beginning or the end, a range
	// in arrays is specified with "begin" and "end" variables.
	size_t begin = (size_t)((n > radius) ? 0 : (radius - n));
	size_t end = (size_t)(((framesNumber - 1) < (n + radius)) ?
		(radius + framesNumber - 1 - n) : (radius * 2));

	if(activationReason == arInitial)
	{
		size_t i;
		for(i = begin; i <= end; i++)
		{
			vsapi->requestFrameFilter(n - (int)radius + (int)i,
				pInternalData->inputNode, frameCtx);
		}
	}
	else if(activationReason == arAllFramesReady)
	{
		size_t sourcesNumber = (radius * 2 + 1);
		const VSFrameRef **  sources = (const VSFrameRef **)
			malloc(sizeof(VSFrameRef *) * sourcesNumber);
		const uint8_t ** readPointers = (const uint8_t **)
			malloc(sizeof(uint8_t *) * sourcesNumber);

		size_t i;
		for(i = begin; i <= end; i++)
		{
			sources[i] = vsapi->getFrameFilter(n - (int)radius + (int)i,
				pInternalData->inputNode, frameCtx);
		}

		const VSFormat * pFormat = pInternalData->videoInfo->format;

		//----------------------------------------------------------------------
		// Copying planes that should remain unprocessed

		const VSFrameRef * referenceFrame = sources[radius];
		const VSFrameRef ** frames = (const VSFrameRef **)
			malloc(sizeof(VSFrameRef *) * pFormat->numPlanes);
		int * planes = (int *)malloc(sizeof(int) * pFormat->numPlanes);

		int copyPlane;
		int j;
		for(j = 0; j < pFormat->numPlanes; j++)
		{
			copyPlane = ((pInternalData->plane & (1 << j)) == 0);
			frames[j] = copyPlane ? referenceFrame : NULL;
			planes[j] = j;
		}

		VSFrameRef * outFrame = vsapi->newVideoFrame2(pFormat,
			pInternalData->videoInfo->width, pInternalData->videoInfo->height,
			frames, planes, referenceFrame, core);

		free((void *)frames);
		free(planes);

		//----------------------------------------------------------------------

		const int colorFamily = pInternalData->videoInfo->format->colorFamily;
		const int averageFallback = (n >= radius) &&
			(n < pInternalData->videoInfo->numFrames - radius);

		const int isInteger1B = (pInternalData->sampleTypeSize == stzInteger1B);
		const int isInteger2B = (pInternalData->sampleTypeSize == stzInteger2B);
		const int isSingle = (pInternalData->sampleTypeSize == stzSingle);
		double * lut = pInternalData->lutGCToLinear;
		int gamma;

		int plane;
		for(plane = 0; plane < pFormat->numPlanes; plane++)
		{
			if((pInternalData->plane & (1u << plane)) == 0)
				continue;

			for(i = begin; i <= end; i++)
				readPointers[i] = vsapi->getReadPtr(sources[i], plane);
			uint8_t * writePointer = vsapi->getWritePtr(outFrame, plane);
			int width = vsapi->getFrameWidth(sources[radius], plane);
			int height = vsapi->getFrameHeight(sources[radius], plane);
			int stride = vsapi->getStride(sources[radius], plane);

			int chroma = ((colorFamily == cmYUV) || (colorFamily == cmYCoCg)) &&
				(plane != 0);

			gamma = (pInternalData->gamma != 0) && !chroma;

			if(averageFallback)
			{
				if(gamma)
				{
					if(isInteger1B)
					{
						tlaAverage1BGamma(readPointers, sourcesNumber,
							writePointer, width, height, stride, lut);
					}
					else if(isInteger2B)
					{
						tlaAverage2BGamma(readPointers, sourcesNumber,
							writePointer, width, height, stride,
							(uint16_t)pInternalData->maxInValue, lut);
					}
					else if(isSingle)
					{
						tlaAverageSGamma(readPointers, sourcesNumber,
							writePointer, width, height, stride);
					}
				}
				else // !gamma
				{
					if(isInteger1B)
					{
						tlaAverage1B(readPointers, sourcesNumber, writePointer,
							width, height, stride);
					}
					else if(isInteger2B)
					{
						tlaAverage2B(readPointers, sourcesNumber, writePointer,
							width, height, stride);
					}
					else if(isSingle)
					{
						tlaAverageS(readPointers, sourcesNumber, writePointer,
							width, height, stride);
					}
				}
			}
			else // !averageFallback
			{
				if(gamma)
				{
					if(isInteger1B)
					{
						tlaApproximate1BGamma(readPointers, begin, end, radius,
							writePointer, width, height, stride, lut);
					}
					else if(isInteger2B)
					{
						tlaApproximate2BGamma(readPointers, begin, end, radius,
							writePointer, width, height, stride,
							(uint16_t)pInternalData->maxInValue, lut);
					}
					else if(isSingle)
					{
						tlaApproximateSGamma(readPointers, begin, end, radius,
							writePointer, width, height, stride);
					}
				}
				else // !gamma
				{
					if(isInteger1B)
					{
						tlaApproximate1B(readPointers, begin, end, radius,
							writePointer, width, height, stride);
					}
					else if(isInteger2B)
					{
						tlaApproximate2B(readPointers, begin, end, radius,
							writePointer, width, height, stride,
							(uint16_t)pInternalData->maxInValue);
					}
					else if(isSingle)
					{
						float minValue = 0.0f;
						float maxValue = 1.0f;

						if(chroma)
						{
							minValue = -0.5f;
							maxValue = 0.5f;
						}

						tlaApproximateS(readPointers, begin, end, radius,
							writePointer, width, height, stride, minValue,
							maxValue);
					}
				}
			}
		}

		for(i = begin; i <= end; i++)
			vsapi->freeFrame(sources[i]);

		free((void *)sources);
		free((void *)readPointers);

		return outFrame;
	}

	return 0;
}

//==============================================================================

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc,
	VSRegisterFunction registerFunc, VSPlugin *plugin)
{
	configFunc("denoising.temporal.templinearapproximate", "tla",
		"VapourSynth Temporal Linear Approximation plugin",
		VAPOURSYNTH_API_VERSION, 1, plugin);

	registerFunc("TempLinearApproximate",
		"clip:clip;"
		"radius:int:opt;"
		"planes:int[]:opt:empty;"
		"gamma:int:opt;"
		, TLACreate, 0, plugin);
}

//==============================================================================
