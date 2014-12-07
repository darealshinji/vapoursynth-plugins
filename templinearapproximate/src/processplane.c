#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "processplane.h"

#define CLAMP(x, xmin, xmax) (x < xmin ? xmin : (x > xmax ? xmax : x))

//==============================================================================

const double gamma_a = 0.055;
const double gamma_k = 12.92;

double gcToLinear(double a_gc)
{
	if(a_gc <= 0.04045)
		return a_gc / gamma_k;
	else
		return pow((a_gc + gamma_a) / (1.0 + gamma_a), 2.4);
}

double linearToGC(double a_linear)
{
	if(a_linear <= 0.0031308)
		return a_linear * gamma_k;
	else
		return (1.0 + gamma_a) * pow(a_linear, 1.0 / 2.4) - gamma_a;
}

//==============================================================================

void tlaAverage1B(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride)
{
	double average;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(a_length *
		sizeof(uint8_t *));

	size_t i;
	for(i = 0; i < a_length; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		size_t w;
		for(w = 0; w < a_width; w++)
		{
			average = 0.0;
			for(i = 0; i < a_length; i++)
				average += (double)cppReadPointers[i][w];
			average /= a_length;
			a_pDestination[w] = (uint8_t)(average + 0.5);
		}

		for(i = 0; i < a_length; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
}

//==============================================================================

void tlaAverage2B(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride)
{
	double average;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(a_length *
		sizeof(uint8_t *));
	const uint16_t ** cppShortReadPointers = (const uint16_t **)
		malloc(a_length * sizeof(uint16_t *));
	uint16_t * pShortDestination;

	size_t i;
	for(i = 0; i < a_length; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		for(i = 0; i < a_length; i++)
			cppShortReadPointers[i] = (const uint16_t *)cppReadPointers[i];
		pShortDestination = (uint16_t *)a_pDestination;

		size_t w;
		for(w = 0; w < a_width; w++)
		{
			average = 0.0;
			for(i = 0; i < a_length; i++)
				average += (double)cppShortReadPointers[i][w];
			average /= a_length;
			pShortDestination[w] = (uint16_t)(average + 0.5);
		}

		for(i = 0; i < a_length; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
	free((void *)cppShortReadPointers);
}

//==============================================================================

void tlaAverageS(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride)
{
	double average;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(a_length *
		sizeof(uint8_t *));
	const float ** cppFloatReadPointers = (const float **)malloc(a_length *
		sizeof(float *));
	float * pFloatDestination;

	size_t i;
	for(i = 0; i < a_length; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		for(i = 0; i < a_length; i++)
			cppFloatReadPointers[i] = (const float *)cppReadPointers[i];
		pFloatDestination = (float *)a_pDestination;

		size_t w;
		for(w = 0; w < a_width; w++)
		{
			average = 0.0;
			for(i = 0; i < a_length; i++)
				average += (double)cppFloatReadPointers[i][w];
			average /= a_length;
			pFloatDestination[w] = (float)average;
		}

		for(i = 0; i < a_length; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
	free((void *)cppFloatReadPointers);
}

//==============================================================================

void tlaApproximate1B(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride)
{
	double a, b, x, y, xsum, ysum, xysum, x2sum;
	size_t length = a_end + 1;
	double xn = (double)(length - a_begin);

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(length *
		sizeof(uint8_t *));

	size_t i;
	for(i = a_begin; i <= a_end; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		size_t w;
		for(w = 0; w < a_width; w++)
		{
			// Gathering data to compute linear approximation
			// using the least squares method
			xsum = 0.0;
			ysum = 0.0;
			xysum = 0.0;
			x2sum = 0.0;

			for(i = a_begin; i <= a_end; i++)
			{
				x = (double)i;
				y = (double)cppReadPointers[i][w];
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
			y = a * (double)a_n + b;
			y = CLAMP(y, 0.0, 255.0);
			a_pDestination[w] = (uint8_t)(y + 0.5);
		}

		for(i = a_begin; i <= a_end; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
}

//==============================================================================

void tlaApproximate2B(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride, uint16_t a_maxValue)
{
	double a, b, x, y, xsum, ysum, xysum, x2sum;
	size_t length = a_end + 1;
	double xn = (double)(length - a_begin);
	double l_maxValue = (double)a_maxValue;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(length *
		sizeof(uint8_t *));
	const uint16_t ** cppShortReadPointers = (const uint16_t **)malloc(length *
		sizeof(uint16_t *));
	uint16_t * pShortDestination;

	size_t i;
	for(i = a_begin; i <= a_end; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		for(i = a_begin; i <= a_end; i++)
			cppShortReadPointers[i] = (const uint16_t *)cppReadPointers[i];
		pShortDestination = (uint16_t *)a_pDestination;

		size_t w;
		for(w = 0; w < a_width; w++)
		{
			// Gathering data to compute linear approximation
			// using the least squares method
			xsum = 0.0;
			ysum = 0.0;
			xysum = 0.0;
			x2sum = 0.0;

			for(i = a_begin; i <= a_end; i++)
			{
				x = (double)i;
				y = (double)cppShortReadPointers[i][w];
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
			y = a * (double)a_n + b;
			y = CLAMP(y, 0.0, l_maxValue);
			pShortDestination[w] = (uint16_t)(y + 0.5);
		}

		for(i = a_begin; i <= a_end; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
	free((void *)cppShortReadPointers);
}

//==============================================================================

void tlaApproximateS(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride, float a_minValue, float a_maxValue)
{
	double a, b, x, y, xsum, ysum, xysum, x2sum;
	size_t length = a_end + 1;
	double xn = (double)(length - a_begin);
	double l_minValue = (double)a_minValue;
	double l_maxValue = (double)a_maxValue;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(length *
		sizeof(uint8_t *));
	const float ** cppFloatReadPointers = (const float **)malloc(length *
		sizeof(float *));
	float * pFloatDestination;

	size_t i;
	for(i = a_begin; i <= a_end; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		for(i = a_begin; i <= a_end; i++)
			cppFloatReadPointers[i] = (const float *)cppReadPointers[i];
		pFloatDestination = (float *)a_pDestination;

		size_t w;
		for(w = 0; w < a_width; w++)
		{
			// Gathering data to compute linear approximation
			// using the least squares method
			xsum = 0.0;
			ysum = 0.0;
			xysum = 0.0;
			x2sum = 0.0;

			for(i = a_begin; i <= a_end; i++)
			{
				x = (double)i;
				y = (double)cppFloatReadPointers[i][w];
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
			y = a * (double)a_n + b;
			y = CLAMP(y, l_minValue, l_maxValue);
			pFloatDestination[w] = (float)y;
		}

		for(i = a_begin; i <= a_end; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
	free((void *)cppFloatReadPointers);
}

//==============================================================================

void tlaAverage1BGamma(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride, double * a_lut)
{
	double average;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(a_length *
		sizeof(uint8_t *));

	size_t i;
	for(i = 0; i < a_length; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		size_t w;
		for(w = 0; w < a_width; w++)
		{
			average = 0.0;
			for(i = 0; i < a_length; i++)
				average += a_lut[cppReadPointers[i][w]];
			average = linearToGC(average / a_length) * 255.0;
			a_pDestination[w] = (uint8_t)(average + 0.5);
		}

		for(i = 0; i < a_length; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
}

//==============================================================================

void tlaAverage2BGamma(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride, uint16_t a_maxValue, double * a_lut)
{
	double average;
	double l_maxValue = (double)a_maxValue;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(a_length *
		sizeof(uint8_t *));
	const uint16_t ** cppShortReadPointers = (const uint16_t **)
		malloc(a_length * sizeof(uint16_t *));
	uint16_t * pShortDestination;

	size_t i;
	for(i = 0; i < a_length; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		for(i = 0; i < a_length; i++)
			cppShortReadPointers[i] = (const uint16_t *)cppReadPointers[i];
		pShortDestination = (uint16_t *)a_pDestination;

		size_t w;
		for(w = 0; w < a_width; w++)
		{
			average = 0.0;
			for(i = 0; i < a_length; i++)
				average += a_lut[cppShortReadPointers[i][w]];
			average = linearToGC(average / a_length) * l_maxValue;
			pShortDestination[w] = (uint16_t)(average + 0.5);
		}

		for(i = 0; i < a_length; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
	free((void *)cppShortReadPointers);
}

//==============================================================================

void tlaAverageSGamma(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride)
{
	double average;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(a_length *
		sizeof(uint8_t *));
	const float ** cppFloatReadPointers = (const float **)malloc(a_length *
		sizeof(float *));
	float * pFloatDestination;

	size_t i;
	for(i = 0; i < a_length; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		for(i = 0; i < a_length; i++)
			cppFloatReadPointers[i] = (const float *)cppReadPointers[i];
		pFloatDestination = (float *)a_pDestination;

		size_t w;
		for(w = 0; w < a_width; w++)
		{
			average = 0.0;
			for(i = 0; i < a_length; i++)
				average += gcToLinear((double)cppFloatReadPointers[i][w]);
			average = linearToGC(average / a_length);
			pFloatDestination[w] = (float)average;
		}

		for(i = 0; i < a_length; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
	free((void *)cppFloatReadPointers);
}

//==============================================================================

void tlaApproximate1BGamma(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride, double * a_lut)
{
	double a, b, x, y, xsum, ysum, xysum, x2sum;
	size_t length = a_end + 1;
	double xn = (double)(length - a_begin);

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(length *
		sizeof(uint8_t *));

	size_t i;
	for(i = a_begin; i <= a_end; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		size_t w;
		for(w = 0; w < a_width; w++)
		{
			// Gathering data to compute linear approximation
			// using the least squares method
			xsum = 0.0;
			ysum = 0.0;
			xysum = 0.0;
			x2sum = 0.0;

			for(i = a_begin; i <= a_end; i++)
			{
				x = (double)i;
				y = a_lut[cppReadPointers[i][w]];
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
			y = a * (double)a_n + b;
			y = linearToGC(y) * 255.0;
			y = CLAMP(y, 0.0, 255.0);
			a_pDestination[w] = (uint8_t)(y + 0.5);
		}

		for(i = a_begin; i <= a_end; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
}

//==============================================================================

void tlaApproximate2BGamma(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride, uint16_t a_maxValue, double * a_lut)
{
	double a, b, x, y, xsum, ysum, xysum, x2sum;
	size_t length = a_end + 1;
	double xn = (double)(length - a_begin);
	double l_maxValue = (double)a_maxValue;

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(length *
		sizeof(uint8_t *));
	const uint16_t ** cppShortReadPointers = (const uint16_t **)malloc(length *
		sizeof(uint16_t *));
	uint16_t * pShortDestination;

	size_t i;
	for(i = a_begin; i <= a_end; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		for(i = a_begin; i <= a_end; i++)
			cppShortReadPointers[i] = (const uint16_t *)cppReadPointers[i];
		pShortDestination = (uint16_t *)a_pDestination;

		size_t w;
		for(w = 0; w < a_width; w++)
		{
			// Gathering data to compute linear approximation
			// using the least squares method
			xsum = 0.0;
			ysum = 0.0;
			xysum = 0.0;
			x2sum = 0.0;

			for(i = a_begin; i <= a_end; i++)
			{
				x = (double)i;
				y = a_lut[cppShortReadPointers[i][w]];
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
			y = a * (double)a_n + b;
			y = linearToGC(y) * l_maxValue;
			y = CLAMP(y, 0.0, l_maxValue);
			pShortDestination[w] = (uint16_t)(y + 0.5);
		}

		for(i = a_begin; i <= a_end; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
	free((void *)cppShortReadPointers);
}

//==============================================================================

void tlaApproximateSGamma(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride)
{
	double a, b, x, y, xsum, ysum, xysum, x2sum;
	size_t length = a_end + 1;
	double xn = (double)(length - a_begin);

	const uint8_t ** cppReadPointers = (const uint8_t **)malloc(length *
		sizeof(uint8_t *));
	const float ** cppFloatReadPointers = (const float **)malloc(length *
		sizeof(float *));
	float * pFloatDestination;

	size_t i;
	for(i = a_begin; i <= a_end; i++)
		cppReadPointers[i] = a_cppSources[i];

	size_t h;
	for(h = 0; h < a_height; h++)
	{
		for(i = a_begin; i <= a_end; i++)
			cppFloatReadPointers[i] = (const float *)cppReadPointers[i];
		pFloatDestination = (float *)a_pDestination;

		size_t w;
		for(w = 0; w < a_width; w++)
		{
			// Gathering data to compute linear approximation
			// using the least squares method
			xsum = 0.0;
			ysum = 0.0;
			xysum = 0.0;
			x2sum = 0.0;

			for(i = a_begin; i <= a_end; i++)
			{
				x = (double)i;
				assert((cppFloatReadPointers[i][w] >= 0.0f) &&
					(cppFloatReadPointers[i][w] <= 1.0f));
				y = gcToLinear((double)cppFloatReadPointers[i][w]);
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
			y = a * (double)a_n + b;
			y = linearToGC(y);
			y = CLAMP(y, 0.0, 1.0);
			pFloatDestination[w] = (float)y;
		}

		for(i = a_begin; i <= a_end; i++)
			cppReadPointers[i] += a_stride;
		a_pDestination += a_stride;
	}

	free((void *)cppReadPointers);
	free((void *)cppFloatReadPointers);
}

//==============================================================================
