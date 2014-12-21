
/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#pragma once

#include <stddef.h>

#include "avisynth.h"
#include <f3kdb.h>

static const char* F3KDB_AVS_PARAMS = "c[range]i[Y]i[Cb]i[Cr]i[grainY]i[grainC]i[sample_mode]i[seed]i[blur_first]b[dynamic_grain]b[opt]i[mt]b[dither_algo]i[keep_tv_range]b[input_mode]i[input_depth]i[output_mode]i[output_depth]i[random_algo_ref]i[random_algo_grain]i[random_param_ref]f[random_param_grain]f[preset]s";

typedef struct _F3KDB_RAW_ARGS
{
    AVSValue child, range, Y, Cb, Cr, grainY, grainC, sample_mode, seed, blur_first, dynamic_grain, opt, mt, dither_algo, keep_tv_range, input_mode, input_depth, output_mode, output_depth, random_algo_ref, random_algo_grain, random_param_ref, random_param_grain, preset;
} F3KDB_RAW_ARGS;

#define F3KDB_ARG_INDEX(name) (offsetof(F3KDB_RAW_ARGS, name) / sizeof(AVSValue))

#define F3KDB_ARG(name) args[F3KDB_ARG_INDEX(name)]

#ifdef F3KDB_SIMPLE_MACRO_NAME

#ifdef SIMPLE_MACRO_NAME
#error Simple macro name has already been defined for SIMPLE_MACRO_NAME
#endif

#define SIMPLE_MACRO_NAME f3kdb

#define ARG F3KDB_ARG

#endif

static void f3kdb_params_from_avs(AVSValue args, f3kdb_params_t* f3kdb_params)
{
    if (F3KDB_ARG(range).Defined()) { f3kdb_params->range = F3KDB_ARG(range).AsInt(); }
    if (F3KDB_ARG(Y).Defined()) { f3kdb_params->Y = (unsigned short)F3KDB_ARG(Y).AsInt(); }
    if (F3KDB_ARG(Cb).Defined()) { f3kdb_params->Cb = (unsigned short)F3KDB_ARG(Cb).AsInt(); }
    if (F3KDB_ARG(Cr).Defined()) { f3kdb_params->Cr = (unsigned short)F3KDB_ARG(Cr).AsInt(); }
    if (F3KDB_ARG(grainY).Defined()) { f3kdb_params->grainY = F3KDB_ARG(grainY).AsInt(); }
    if (F3KDB_ARG(grainC).Defined()) { f3kdb_params->grainC = F3KDB_ARG(grainC).AsInt(); }
    if (F3KDB_ARG(sample_mode).Defined()) { f3kdb_params->sample_mode = F3KDB_ARG(sample_mode).AsInt(); }
    if (F3KDB_ARG(seed).Defined()) { f3kdb_params->seed = F3KDB_ARG(seed).AsInt(); }
    if (F3KDB_ARG(blur_first).Defined()) { f3kdb_params->blur_first = F3KDB_ARG(blur_first).AsBool(); }
    if (F3KDB_ARG(dynamic_grain).Defined()) { f3kdb_params->dynamic_grain = F3KDB_ARG(dynamic_grain).AsBool(); }
    if (F3KDB_ARG(opt).Defined()) { f3kdb_params->opt = (OPTIMIZATION_MODE)F3KDB_ARG(opt).AsInt(); }
    if (F3KDB_ARG(dither_algo).Defined()) { f3kdb_params->dither_algo = (DITHER_ALGORITHM)F3KDB_ARG(dither_algo).AsInt(); }
    if (F3KDB_ARG(keep_tv_range).Defined()) { f3kdb_params->keep_tv_range = F3KDB_ARG(keep_tv_range).AsBool(); }
    if (F3KDB_ARG(output_mode).Defined()) { f3kdb_params->output_mode = (PIXEL_MODE)F3KDB_ARG(output_mode).AsInt(); }
    if (F3KDB_ARG(output_depth).Defined()) { f3kdb_params->output_depth = F3KDB_ARG(output_depth).AsInt(); }
    if (F3KDB_ARG(random_algo_ref).Defined()) { f3kdb_params->random_algo_ref = (RANDOM_ALGORITHM)F3KDB_ARG(random_algo_ref).AsInt(); }
    if (F3KDB_ARG(random_algo_grain).Defined()) { f3kdb_params->random_algo_grain = (RANDOM_ALGORITHM)F3KDB_ARG(random_algo_grain).AsInt(); }
    if (F3KDB_ARG(random_param_ref).Defined()) { f3kdb_params->random_param_ref = F3KDB_ARG(random_param_ref).AsFloat(); }
    if (F3KDB_ARG(random_param_grain).Defined()) { f3kdb_params->random_param_grain = F3KDB_ARG(random_param_grain).AsFloat(); }
}

