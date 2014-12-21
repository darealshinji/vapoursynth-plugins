
/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#pragma once

#include <f3kdb.h>
#include "plugin.h"
#include "VapourSynth.h"

static const char* F3KDB_VAPOURSYNTH_PARAMS = "clip:clip;range:int:opt;y:int:opt;cb:int:opt;cr:int:opt;grainy:int:opt;grainc:int:opt;sample_mode:int:opt;seed:int:opt;blur_first:int:opt;dynamic_grain:int:opt;opt:int:opt;dither_algo:int:opt;keep_tv_range:int:opt;output_depth:int:opt;random_algo_ref:int:opt;random_algo_grain:int:opt;random_param_ref:float:opt;random_param_grain:float:opt;preset:data:opt;";

static bool f3kdb_params_from_vs(f3kdb_params_t* f3kdb_params, const VSMap* in, VSMap* out, const VSAPI* vsapi)
{
    if (!param_from_vsmap(&f3kdb_params->range, "range", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->Y, "y", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->Cb, "cb", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->Cr, "cr", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->grainY, "grainy", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->grainC, "grainc", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->sample_mode, "sample_mode", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->seed, "seed", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->blur_first, "blur_first", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->dynamic_grain, "dynamic_grain", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->opt, "opt", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->dither_algo, "dither_algo", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->keep_tv_range, "keep_tv_range", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->output_depth, "output_depth", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->random_algo_ref, "random_algo_ref", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->random_algo_grain, "random_algo_grain", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->random_param_ref, "random_param_ref", in, out, vsapi)) { return false; }
    if (!param_from_vsmap(&f3kdb_params->random_param_grain, "random_param_grain", in, out, vsapi)) { return false; }
    return true;
}
