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

#define UNUSED(x) (void)(x)

#define CLAMP(x, xmin, xmax) (x < xmin ? xmin : (x > xmax ? xmax : x))

//==============================================================================
// Filter internal data structure

typedef struct tagTLAData
{
	VSNodeRef * inputNode;
	const VSVideoInfo * videoInfo;
	// Frames neighbourhood size
	int radius;
	// Bit sum of planes to process
	int plane;
	// Bitdepth specific clamping value
	int32_t maxInValue;
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

	int acceptableFormat = (pFormat->sampleType == stInteger) &&
		isConstantFormat(internalData.videoInfo) &&
		(internalData.videoInfo->numFrames != 0);
	if(!acceptableFormat)
	{
		vsapi->setError(out, "TempLinearApproximate: "
			"only constant format integer input with fixed frame number "
			"is supported.");
		vsapi->freeNode(internalData.inputNode);
		return;
	}

	internalData.maxInValue =
		(((uint32_t)1) << internalData.videoInfo->format->bitsPerSample) - 1;

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

	int planesNumber = vsapi->propNumElements(in, "planes");
	if(planesNumber == -1)
	{
		// Planes array wasn't specified. Process all planes.
		internalData.plane = ~0;
	}
	else
	{
		internalData.plane = 0;
		int processPlane;
		int i;
		for(i = 0; i < planesNumber; i++)
		{
			processPlane = vsapi->propGetInt(in, "planes", i, &error);
			if(error == 0)
				internalData.plane |= (1 << processPlane);
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
	free(pInternalData);
}

//==============================================================================

static const VSFrameRef * VS_CC TLAGetFrame(int n, int activationReason,
	void ** instanceData, void ** frameData, VSFrameContext * frameCtx,
	VSCore * core, const VSAPI * vsapi)
{
	UNUSED(frameData);

	TLAData * pInternalData = (TLAData *) *instanceData;
	int radius = pInternalData->radius;
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
	int begin = (n > radius) ? 0 : (radius - n);
	int end = ((framesNumber - 1) < (n + radius)) ?
		(radius + framesNumber - 1 - n) : (radius * 2);
	// The number of frames used in computations is also used.
	double xn = (double)(end - begin + 1);

	if(activationReason == arInitial)
	{
		int i;
		for(i = begin; i <= end; i++)
		{
			vsapi->requestFrameFilter(n - radius + i,
				pInternalData->inputNode, frameCtx);
		}
	}
	else if(activationReason == arAllFramesReady)
	{
		size_t sourcesNumber = (radius * 2 + 1);
		const VSFrameRef ** sources = (const VSFrameRef **)
			malloc(sizeof(VSFrameRef *) * sourcesNumber);
		const uint8_t ** readPointers = (const uint8_t **)
			malloc(sizeof(uint8_t *) * sourcesNumber);

		int i;
		for(i = begin; i <= end; i++)
		{
			sources[i] = vsapi->getFrameFilter(n - radius + i,
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
		for(i = 0; i < pFormat->numPlanes; i++)
		{
			copyPlane = ((pInternalData->plane & (1 << i)) == 0);
			frames[i] = copyPlane ? referenceFrame : NULL;
			planes[i] = i;
		}

		VSFrameRef * outFrame = vsapi->newVideoFrame2(pFormat,
			pInternalData->videoInfo->width, pInternalData->videoInfo->height,
			frames, planes, referenceFrame, core);

		free(frames);
		free(planes);

		//----------------------------------------------------------------------

		double a, b, x, y, xsum, ysum, xysum, x2sum;
		int bytesPS = pInternalData->videoInfo->format->bytesPerSample;

		int averageFallback = (n >= radius) &&
			(n < pInternalData->videoInfo->numFrames - radius);

		if(bytesPS == 2)
		{
			const uint16_t ** shortReadPointers = (const uint16_t **)
				malloc(sizeof(uint16_t *) * sourcesNumber);

			int plane;
			for(plane = 0; plane < pFormat->numPlanes; plane++)
			{
				if((pInternalData->plane & (1 << plane)) == 0)
					continue;

				uint8_t * writePointer = vsapi->getWritePtr(outFrame, plane);
				uint16_t * shortWritePointer;
				int planeHeight = vsapi->getFrameHeight(sources[radius], plane);
				int planeWidth = vsapi->getFrameWidth(sources[radius], plane);
				int stride = vsapi->getStride(sources[radius], plane);

				for(i = begin; i <= end; i++)
					readPointers[i] = vsapi->getReadPtr(sources[i], plane);

				if(averageFallback)
				{
					int h;
					for(h = 0; h < planeHeight; h++)
					{
						for(i = begin; i <= end; i++)
						{
							shortReadPointers[i] =
								(const uint16_t *)readPointers[i];
						}
						shortWritePointer = (uint16_t *)writePointer;

						int w;
						for(w = 0; w < planeWidth; w++)
						{
							// Averaging point value between frames
							xsum = 0.0;
							for(i = begin; i <= end; i++)
								xsum += (double)shortReadPointers[i][w];
							y = xsum / xn;

							int32_t outValue = (int32_t)(y + 0.5);
							outValue =
								CLAMP(outValue, 0, pInternalData->maxInValue);

							shortWritePointer[w] = (uint16_t)outValue;
						}

						for(i = begin; i <= end; i++)
							readPointers[i] += stride;
						writePointer += stride;
					}
				}
				else
				{
					int h;
					for(h = 0; h < planeHeight; h++)
					{
						for(i = begin; i <= end; i++)
						{
							shortReadPointers[i] =
								(const uint16_t *)readPointers[i];
						}
						shortWritePointer = (uint16_t *)writePointer;

						int w;
						for(w = 0; w < planeWidth; w++)
						{
							// Gathering data to compute linear approximation
							// using the least squares method
							xsum = 0.0;
							ysum = 0.0;
							xysum = 0.0;
							x2sum = 0.0;

							for(i = begin; i <= end; i++)
							{
								x = (double)i;
								y = (double)shortReadPointers[i][w];
								xsum += x;
								ysum += y;
								xysum += x * y;
								x2sum += x * x;
							}

							// Computing linear approximation coefficients
							a = (xn * xysum - xsum * ysum) /
								(xn * x2sum - xsum * xsum);
							b = (ysum - a * xsum) / xn;

							// Taking the value of linear function
							// in the desired point
							y = a * (double)radius + b;

							int32_t outValue = (int32_t)(y + 0.5);
							outValue =
								CLAMP(outValue, 0, pInternalData->maxInValue);

							shortWritePointer[w] = (uint16_t)outValue;
						}

						for(i = begin; i <= end; i++)
							readPointers[i] += stride;
						writePointer += stride;
					}
				}
			}

			free(shortReadPointers);
		}
		else // 1 byte per sample
		{
			int plane;
			for(plane = 0; plane < pFormat->numPlanes; plane++)
			{
				if((pInternalData->plane & (1 << plane)) == 0)
					continue;

				uint8_t * writePointer = vsapi->getWritePtr(outFrame, plane);
				int planeHeight = vsapi->getFrameHeight(sources[radius], plane);
				int planeWidth = vsapi->getFrameWidth(sources[radius], plane);
				int stride = vsapi->getStride(sources[radius], plane);

				for(i = begin; i <= end; i++)
					readPointers[i] = vsapi->getReadPtr(sources[i], plane);

				if(averageFallback)
				{
					int h;
					for(h = 0; h < planeHeight; h++)
					{
						int w;
						for(w = 0; w < planeWidth; w++)
						{
							// Averaging point value between frames
							xsum = 0.0;
							for(i = begin; i <= end; i++)
								xsum += (double)readPointers[i][w];
							y = xsum / xn;

							int32_t outValue = (int32_t)(y + 0.5);
							outValue =
								CLAMP(outValue, 0, pInternalData->maxInValue);
							writePointer[w] = (uint8_t)outValue;
						}

						for(i = begin; i <= end; i++)
							readPointers[i] += stride;
						writePointer += stride;
					}
				}
				else
				{
					int h;
					for(h = 0; h < planeHeight; h++)
					{
						int w;
						for(w = 0; w < planeWidth; w++)
						{
							// Gathering data to compute linear approximation
							// using the least squares method
							xsum = 0.0;
							ysum = 0.0;
							xysum = 0.0;
							x2sum = 0.0;

							for(i = begin; i <= end; i++)
							{
								x = (double)i;
								y = (double)readPointers[i][w];
								xsum += x;
								ysum += y;
								xysum += x * y;
								x2sum += x * x;
							}

							// Computing linear approximation coefficients
							a = (xn * xysum - xsum * ysum) /
								(xn * x2sum - xsum * xsum);
							b = (ysum - a * xsum) / xn;

							// Taking the value of linear function
							// in the desired point
							y = a * (double)radius + b;

							int32_t outValue = (int32_t)(y + 0.5);
							outValue =
								CLAMP(outValue, 0, pInternalData->maxInValue);
							writePointer[w] = (uint8_t)outValue;
						}

						for(i = begin; i <= end; i++)
							readPointers[i] += stride;
						writePointer += stride;
					}
				}
			}
		}

		for(i = begin; i <= end; i++)
			vsapi->freeFrame(sources[i]);

		free(sources);
		free(readPointers);

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
		, TLACreate, 0, plugin);
}

//==============================================================================
