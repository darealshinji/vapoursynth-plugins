#include <stdio.h>
#include <string.h>

#include "VapourSynth.h"
#include "VSHelper.h" //isConstantFormat()

#include "declarations.h"

#define UNUSED(x) (void)(x)

//==============================================================================
// Filter internal data structure

typedef struct tagPluginData
{
	VSNodeRef * pInputNode;
	const VSVideoInfo * cpVideoInfo;
	double factors[8][8];
	double lut[8][8];
	int32_t maxIntegerValue;
}
PluginData;

//==============================================================================
// Forward declarations

static void VS_CC PluginInit(VSMap * a_pInMap, VSMap * a_pOutMap,
	void ** a_ppInstanceData, VSNode * a_pNode, VSCore * a_pCore,
	const VSAPI * a_cpVSAPI);

static const VSFrameRef * VS_CC PluginGetFrame(int a_frameNumber,
	int a_activationReason, void ** a_ppInstanceData, void ** a_ppFrameData,
	VSFrameContext * a_pFrameContext, VSCore * a_pCore,
	const VSAPI * a_cpVSAPI);

static void VS_CC PluginFree(void * a_pInstanceData, VSCore * a_pCore,
	const VSAPI * a_cpVSAPI);

//==============================================================================

static void VS_CC PluginCreate(const VSMap * a_pInMap, VSMap * a_pOutMap,
	void * a_pUserData, VSCore * a_pCore, const VSAPI * a_cpVSAPI)
{
	UNUSED(a_pUserData);

	PluginData internalData;

	internalData.pInputNode = a_cpVSAPI->propGetNode(a_pInMap, "clip", 0, 0);
	internalData.cpVideoInfo = a_cpVSAPI->getVideoInfo(internalData.pInputNode);

	//--------------------------------------------------------------------------
	// Format check

	const VSFormat * pFormat = internalData.cpVideoInfo->format;

	if(!isConstantFormat(internalData.cpVideoInfo))
	{
		a_cpVSAPI->setError(a_pOutMap, "dct.Filter: "
			"only constant format input is supported.");
		a_cpVSAPI->freeNode(internalData.pInputNode);
		return;
	}

	if(pFormat->sampleType == stInteger)
	{
		internalData.maxIntegerValue =
			(((uint32_t)1) << pFormat->bitsPerSample) - 1;
	}

	//--------------------------------------------------------------------------
	// Factors

	int factorsNumber = a_cpVSAPI->propNumElements(a_pInMap, "factors");

	if(factorsNumber != 8)
	{
		a_cpVSAPI->setError(a_pOutMap, "dct.Filter: 8 factors must be passed.");
		a_cpVSAPI->freeNode(internalData.pInputNode);
		return;
	}

	double factors[8] = {0.0};
	int error = 0;
	for(ptrdiff_t i = 0; i < 8; ++i)
	{
		factors[i] = a_cpVSAPI->propGetFloat(a_pInMap, "factors", (int)i, &error);
		if(error)
		{
			char errorString[51] = {0};
			sprintf(errorString, "dct.Filter: %td element in factors array "
				"is not a valid real number.", i);
			a_cpVSAPI->setError(a_pOutMap, errorString);
			a_cpVSAPI->freeNode(internalData.pInputNode);
			return;
		}
	}

	fillFactors(factors, internalData.factors);

	//--------------------------------------------------------------------------

	fillLUT(internalData.lut);

	PluginData * pInternalData = (PluginData *)malloc(sizeof(PluginData));
	*pInternalData = internalData;
	a_cpVSAPI->createFilter(a_pInMap, a_pOutMap, "Filter", PluginInit,
		PluginGetFrame, PluginFree, fmParallel, 0, pInternalData, a_pCore);
}

//==============================================================================

static void VS_CC PluginInit(VSMap * a_pInMap, VSMap * a_pOutMap,
	void ** a_ppInstanceData, VSNode * a_pNode, VSCore * a_pCore,
	const VSAPI * a_cpVSAPI)
{
	UNUSED(a_pInMap);
	UNUSED(a_pOutMap);
	UNUSED(a_pNode);
	UNUSED(a_pCore);

	PluginData * pInternalData = (PluginData *) *a_ppInstanceData;
	a_cpVSAPI->setVideoInfo(pInternalData->cpVideoInfo, 1, a_pNode);
}

//==============================================================================

// Free all allocated data on filter destruction
static void VS_CC PluginFree(void * a_pInstanceData, VSCore * a_pCore,
	const VSAPI * a_cpVSAPI)
{
	UNUSED(a_pCore);
	UNUSED(a_cpVSAPI);

	PluginData * pPluginData = (PluginData *)a_pInstanceData;
	a_cpVSAPI->freeNode(pPluginData->pInputNode);
	free(a_pInstanceData);
}

//==============================================================================

static const VSFrameRef * VS_CC PluginGetFrame(int a_frameNumber,
	int a_activationReason, void ** a_ppInstanceData, void ** a_ppFrameData,
	VSFrameContext * a_pFrameContext, VSCore * a_pCore,
	const VSAPI * a_cpVSAPI)
{
	UNUSED(a_ppFrameData);

	PluginData * pInternalData = (PluginData *) *a_ppInstanceData;

	if(a_activationReason == arInitial)
	{
		a_cpVSAPI->requestFrameFilter(a_frameNumber,
			pInternalData->pInputNode, a_pFrameContext);
	}
	else if(a_activationReason == arAllFramesReady)
	{
		// Allocate double values buffer of sizes padded to mod8
		int bufferWidth = pInternalData->cpVideoInfo->width;
		if(bufferWidth % 8 != 0)
			bufferWidth = (bufferWidth / 8 + 1) * 8;
		int bufferHeight = pInternalData->cpVideoInfo->height;
		if(bufferHeight % 8 != 0)
			bufferHeight = (bufferHeight / 8 + 1) * 8;

		const VSFrameRef * pInputFrame = a_cpVSAPI->getFrameFilter(
			a_frameNumber, pInternalData->pInputNode, a_pFrameContext);

		const VSFormat * cpFormat = pInternalData->cpVideoInfo->format;

		VSFrameRef * pOutFrame = a_cpVSAPI->newVideoFrame(cpFormat,
			pInternalData->cpVideoInfo->width,
			pInternalData->cpVideoInfo->height,
			pInputFrame, a_pCore);

		size_t bufferSize = bufferWidth * bufferHeight * sizeof(double);
		double * pSource = (double *)malloc(bufferSize);
		double * pDCT = (double *)malloc(bufferSize);

		int plane;
		for(plane = 0; plane < cpFormat->numPlanes; ++plane)
		{
			int planeHeight = a_cpVSAPI->getFrameHeight(pInputFrame, plane);
			int planeWidth = a_cpVSAPI->getFrameWidth(pInputFrame, plane);
			int stride = a_cpVSAPI->getStride(pInputFrame, plane);
			const uint8_t * pRead = a_cpVSAPI->getReadPtr(pInputFrame, plane);
			const uint8_t * pReadLine = pRead;
			double * pSourceLine = pSource;
			int w, h;

			memset(pSource, 0, bufferSize);
			memset(pDCT, 0, bufferSize);

			ptrdiff_t bufferStride = planeWidth;
			if(bufferStride % 8 != 0)
				bufferStride = (bufferStride / 8 + 1) * 8;

			if(cpFormat->sampleType == stFloat)
			{
				const float * pFloatLine;
				for(h = 0; h < planeHeight; ++h)
				{
					pFloatLine = (const float *)pReadLine;
					for(w = 0; w < planeWidth; ++w)
						pSourceLine[w] = (double)pFloatLine[w];
					pReadLine += stride;
					pSourceLine += bufferStride;
				}
			}
			else if(cpFormat->bytesPerSample == 2)
			{
				const uint16_t * pShortLine;
				for(h = 0; h < planeHeight; ++h)
				{
					pShortLine = (const uint16_t *)pReadLine;
					for(w = 0; w < planeWidth; ++w)
						pSourceLine[w] = (double)pShortLine[w];
					pReadLine += stride;
					pSourceLine += bufferStride;
				}
			}
			else
			{
				for(h = 0; h < planeHeight; ++h)
				{
					for(w = 0; w < planeWidth; ++w)
						pSourceLine[w] = (double)pReadLine[w];
					pReadLine += stride;
					pSourceLine += bufferStride;
				}
			}

			pSourceLine = pSource;
			double * pDCTLine = pDCT;
			for(h = 0; h < planeHeight; h += 8)
			{
				for(w = 0; w < planeWidth; w += 8)
				{
					cdct(pSourceLine + w, pDCTLine + w, bufferStride,
						pInternalData->lut);
					multiply(pDCTLine + w, pInternalData->factors,
						bufferStride);
					cidct(pDCTLine + w, pSourceLine + w, bufferStride,
						pInternalData->lut);
				}
				pSourceLine += bufferStride * 8;
				pDCTLine += bufferStride * 8;
			}

			double min = 0.0, max = 1.0;
			if(cpFormat->sampleType == stFloat)
			{
				if(plane != 0)
				{
					min = -0.5;
					max = -0.5;
				}
			}
			else
				max = pInternalData->maxIntegerValue;

			for(w = 0; w < bufferWidth * bufferHeight; ++w)
				clamp(pSource + w, min, max);

			uint8_t * pWrite = a_cpVSAPI->getWritePtr(pOutFrame, plane);
			uint8_t * pWriteLine = pWrite;
			pSourceLine = pSource;
			if(cpFormat->sampleType == stFloat)
			{
				float * pFloatLine;
				for(h = 0; h < planeHeight; ++h)
				{
					pFloatLine = (float *)pWriteLine;
					for(w = 0; w < planeWidth; ++w)
						pFloatLine[w] = (float)pSourceLine[w];
					pWriteLine += stride;
					pSourceLine += bufferStride;
				}
			}
			else if(cpFormat->bytesPerSample == 2)
			{
				uint16_t * pShortLine;
				for(h = 0; h < planeHeight; ++h)
				{
					pShortLine = (uint16_t *)pWriteLine;
					for(w = 0; w < planeWidth; ++w)
						pShortLine[w] = (uint16_t)pSourceLine[w];
					pWriteLine += stride;
					pSourceLine += bufferStride;
				}
			}
			else
			{
				for(h = 0; h < planeHeight; ++h)
				{
					for(w = 0; w < planeWidth; ++w)
						pWriteLine[w] = (uint8_t)pSourceLine[w];
					pWriteLine += stride;
					pSourceLine += bufferStride;
				}
			}
		}

		free(pSource);
		free(pDCT);

		a_cpVSAPI->freeFrame(pInputFrame);
		return(pOutFrame);
	}

	return 0;
}

//==============================================================================

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin a_configFunc,
	VSRegisterFunction a_registerFunc, VSPlugin * a_pPlugin)
{
	a_configFunc("filter.indirect.dct", "dct", "DCT filtering plugin",
		VAPOURSYNTH_API_VERSION, 1, a_pPlugin);

	a_registerFunc("Filter",
		"clip:clip;"
		"factors:float[];"
		, PluginCreate, 0, a_pPlugin);
}

//==============================================================================
