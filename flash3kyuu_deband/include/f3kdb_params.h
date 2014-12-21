
/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#pragma once

#include "f3kdb_enums.h"

typedef struct _f3kdb_params_t
{
    int range; 
    unsigned short Y; 
    unsigned short Cb; 
    unsigned short Cr; 
    int grainY; 
    int grainC; 
    int sample_mode; 
    int seed; 
    bool blur_first; 
    bool dynamic_grain; 
    OPTIMIZATION_MODE opt; 
    DITHER_ALGORITHM dither_algo; 
    bool keep_tv_range; 
    PIXEL_MODE output_mode; 
    int output_depth; 
    RANDOM_ALGORITHM random_algo_ref; 
    RANDOM_ALGORITHM random_algo_grain; 
    double random_param_ref; 
    double random_param_grain; 
} f3kdb_params_t;

