#include <math.h>

#include "declarations.h"

//==============================================================================

const double pi = 3.1415926535897932384626433832795028841971;
const double c = 0.707106781186547524400844362; // 1 / sqrt(2)

//==============================================================================

void fillFactors(double a_source[8], double a_destination[8][8])
{
	ptrdiff_t x, y;
	for(y = 0; y < 8; ++y)
		for(x = 0; x < 8; ++x)
			a_destination[y][x] = a_source[y] * a_source[x];
}

//==============================================================================

void fillLUT(double a_lut[8][8])
{
	ptrdiff_t x, y;
	for(y = 0; y < 8; ++y)
		for(x = 0; x < 8; ++x)
			a_lut[x][y] = cos((2.0 * x + 1.0) * y * pi / 16.0);
}

//==============================================================================

void cdct(double * a_pSource, double * a_pDestination,
	ptrdiff_t a_stride, double a_lut[8][8])
{
	const double * l_pSource = a_pSource;
	double * l_pDestination = a_pDestination;
	double result;
	ptrdiff_t u, v, x, y;

	// rows
	for(v = 0; v < 8; ++v)
	{
		for(u = 0; u < 8; ++u)
		{
			result = 0.0;
			for(x = 0; x < 8; ++x)
				result += l_pSource[x] * a_lut[x][u];
			if(u == 0)
				result *= c;
			result *= 0.5;
			l_pDestination[u] = result;
		}
		l_pSource += a_stride;
		l_pDestination += a_stride;
	}

	// columns
	double temp[8];
	l_pDestination = a_pDestination;
	ptrdiff_t voffset;
	for(u = 0; u < 8; ++u)
	{
		voffset = 0;
		for(v = 0; v < 8; ++v)
		{
			temp[v] = l_pDestination[voffset];
			voffset += a_stride;
		}
		voffset = 0;
		for(v = 0; v < 8; ++v)
		{
			result = 0.0;
			for(y = 0; y < 8; ++y)
				result += temp[y] * a_lut[y][v];
			if(v == 0)
				result *= c;
			result *= 0.5;
			l_pDestination[voffset] = result;
			voffset += a_stride;
		}
		l_pDestination++;
	}
}

//==============================================================================

void cidct(double * a_pSource, double * a_pDestination,
	ptrdiff_t a_stride, double a_lut[8][8])
{
	const double * l_pSource = a_pSource;
	double * l_pDestination = a_pDestination;
	double item;
	double result;
	ptrdiff_t u, v, x, y;

	// rows
	for(y = 0; y < 8; ++y)
	{
		for(x = 0; x < 8; ++x)
		{
			result = 0.0;
			for(u = 0; u < 8; ++u)
			{
				item = l_pSource[u] * a_lut[x][u];
				if(u == 0)
					item *= c;
				result += item;
			}
			result *= 0.5;
			l_pDestination[x] = result;
		}
		l_pSource += a_stride;
		l_pDestination += a_stride;
	}

	// columns
	double temp[8];
	l_pDestination = a_pDestination;
	ptrdiff_t yoffset;
	for(x = 0; x < 8; ++x)
	{
		yoffset = 0;
		for(y = 0; y < 8; ++y)
		{
			temp[y] = l_pDestination[yoffset];
			yoffset += a_stride;
		}
		yoffset = 0;
		for(y = 0; y < 8; ++y)
		{
			result = 0.0;
			for(v = 0; v < 8; ++v)
			{
				item = temp[v] * a_lut[y][v];
				if(v == 0)
					item *= c;
				result += item;
			}
			result *= 0.5;
			l_pDestination[yoffset] = result;
			yoffset += a_stride;
		}
		l_pDestination++;
	}
}

//==============================================================================

void multiply(double * a_pDestination, double a_factors[8][8],
	ptrdiff_t a_stride)
{
	double * l_pDestination = a_pDestination;
	ptrdiff_t x, y;
	for(y = 0; y < 8; ++y)
	{
		for(x = 0; x < 8; ++x)
			l_pDestination[x] = l_pDestination[x] * a_factors[y][x];
		l_pDestination += a_stride;
	}
}

//==============================================================================

void clamp(double * a_pValue, double a_min, double a_max)
{
	if(*a_pValue < a_min)
		*a_pValue = a_min;
	else if(*a_pValue > a_max)
		*a_pValue = a_max;
}

//==============================================================================
