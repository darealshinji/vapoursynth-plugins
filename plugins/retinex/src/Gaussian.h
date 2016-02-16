/*
* Retinex filter - VapourSynth plugin
* Copyright (C) 2014  mawen1250
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef GAUSSIAN_H_
#define GAUSSIAN_H_


#include "Helper.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const double Pi = 3.1415926535897932384626433832795;
const double sqrt_2Pi = sqrt(2 * Pi);

const double sigmaSMul = 2.;
const double sigmaRMul = sizeof(FLType) < 8 ? 8. : 32.; // 8. when FLType is float, 32. when FLType is double


void Recursive_Gaussian_Parameters(const double sigma, FLType & B, FLType & B1, FLType & B2, FLType & B3);
void Recursive_Gaussian2D_Vertical(FLType * output, const FLType * input, int height, int width, int stride, const FLType B, const FLType B1, const FLType B2, const FLType B3);
void Recursive_Gaussian2D_Horizontal(FLType * output, const FLType * input, int height, int width, int stride, const FLType B, const FLType B1, const FLType B2, const FLType B3);


inline double Gaussian_Function(double x, double sigma)
{
    x /= sigma;
    return exp(x*x / -2);
}

inline double Gaussian_Function_sqr_x(double sqr_x, double sigma)
{
    return exp(sqr_x / (sigma*sigma*-2));
}

inline double Normalized_Gaussian_Function(double x, double sigma)
{
    x /= sigma;
    return exp(x*x / -2) / (sqrt_2Pi*sigma);
}

inline double Normalized_Gaussian_Function_sqr_x(double sqr_x, double sigma)
{
    return exp(sqr_x / (sigma*sigma*-2)) / (sqrt_2Pi*sigma);
}


inline FLType * Gaussian_Function_Spatial_LUT_Generation(const int xUpper, const int yUpper, const double sigmaS)
{
    int x, y;
    FLType * GS_LUT = new FLType[xUpper*yUpper];

    for (y = 0; y < yUpper; y++)
    {
        for (x = 0; x < xUpper; x++)
        {
            GS_LUT[y*xUpper + x] = static_cast<FLType>(Gaussian_Function_sqr_x(static_cast<FLType>(x*x + y*y), sigmaS));
        }
    }

    return GS_LUT;
}

inline FLType Gaussian_Distribution2D_Spatial_LUT_Lookup(const FLType * GS_LUT, const int xUpper, const int x, const int y)
{
    return GS_LUT[y*xUpper + x];
}

inline void Gaussian_Function_Spatial_LUT_Free(FLType * GS_LUT)
{
    delete[] GS_LUT;
}


inline FLType * Gaussian_Function_Range_LUT_Generation(const int ValueRange, double sigmaR)
{
    int i;
    int Levels = ValueRange + 1;
    const int upper = Min(ValueRange, static_cast<int>(sigmaR*sigmaRMul*ValueRange + 0.5));
    FLType * GR_LUT = new FLType[Levels];

    for (i = 0; i <= upper; i++)
    {
        GR_LUT[i] = static_cast<FLType>(Normalized_Gaussian_Function(static_cast<double>(i) / ValueRange, sigmaR));
    }
    // For unknown reason, when more range weights are too small or equal 0, the runtime speed gets lower - mainly in function Recursive_Gaussian2D_Horizontal.
    // To avoid this issue, we set range weights whose range values are larger than sigmaR*sigmaRMul to the Gaussian function value at sigmaR*sigmaRMul.
    if (i < Levels)
    {
        const FLType upperLUTvalue = GR_LUT[upper];
        for (; i < Levels; i++)
        {
            GR_LUT[i] = upperLUTvalue;
        }
    }

    return GR_LUT;
}

template < typename T >
inline FLType Gaussian_Distribution2D_Range_LUT_Lookup(const FLType * GR_LUT, const T Value1, const T Value2)
{
    return GR_LUT[Value1 > Value2 ? Value1 - Value2 : Value2 - Value1];
}

inline void Gaussian_Function_Range_LUT_Free(FLType * GR_LUT)
{
    delete[] GR_LUT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif