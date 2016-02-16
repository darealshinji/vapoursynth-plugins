#ifndef PROCESSPLANE_H_INCLUDED
#define PROCESSPLANE_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

double gcToLinear(double a_gc);
double linearToGC(double a_linear);

void tlaAverage1B(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride);
void tlaAverage2B(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride);
void tlaAverageS(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride);

void tlaApproximate1B(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride);
void tlaApproximate2B(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride, uint16_t a_maxValue);
void tlaApproximateS(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride, float a_minValue, float a_maxValue);

void tlaAverage1BGamma(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride, double * a_lut);
void tlaAverage2BGamma(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride, uint16_t a_maxValue, double * a_lut);
void tlaAverageSGamma(const uint8_t ** a_cppSources, size_t a_length,
	uint8_t * a_pDestination, size_t a_width, size_t a_height,
	ptrdiff_t a_stride);

void tlaApproximate1BGamma(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride, double * a_lut);
void tlaApproximate2BGamma(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride, uint16_t a_maxValue, double * a_lut);
void tlaApproximateSGamma(const uint8_t ** a_cppSources, size_t a_begin,
	size_t a_end, size_t a_n, uint8_t * a_pDestination, size_t a_width,
	size_t a_height, ptrdiff_t a_stride);

#endif // PROCESSPLANE_H_INCLUDED
