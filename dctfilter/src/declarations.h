#ifndef DECLARATIONS_H_INCLUDED
#define DECLARATIONS_H_INCLUDED

#include <stddef.h>

void fillFactors(double a_source[8], double a_destination[8][8]);

void fillLUT(double a_lut[8][8]);

void cdct(double * a_pSource, double * a_pDestination,
	ptrdiff_t a_stride, double a_lut[8][8]);

void cidct(double * a_pSource, double * a_pDestination,
	ptrdiff_t a_stride, double a_lut[8][8]);

void multiply(double * a_pDestination, double a_factors[8][8],
	ptrdiff_t a_stride);

void clamp(double * a_pValue, double a_min, double a_max);

#endif // DECLARATIONS_H_INCLUDED
