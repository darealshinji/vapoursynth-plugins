
/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#include <string>

#include "random.h"
#include "auto_utils.h"
#include "auto_utils_helper.h"

void params_set_defaults(f3kdb_params_t* params)
{
    params->range = 15;
    params->Y = 64;
    params->Cb = 64;
    params->Cr = 64;
    params->grainY = 64;
    params->grainC = 64;
    params->sample_mode = 2;
    params->seed = 0;
    params->blur_first = true;
    params->dynamic_grain = false;
    params->opt = IMPL_AUTO_DETECT;
    params->dither_algo = DA_HIGH_FLOYD_STEINBERG_DITHERING;
    params->keep_tv_range = false;
    params->output_mode = DEFAULT_PIXEL_MODE;
    params->output_depth = -1;
    params->random_algo_ref = RANDOM_ALGORITHM_UNIFORM;
    params->random_algo_grain = RANDOM_ALGORITHM_UNIFORM;
    params->random_param_ref = DEFAULT_RANDOM_PARAM;
    params->random_param_grain = DEFAULT_RANDOM_PARAM;
}

int params_set_by_string(f3kdb_params_t* params, const char* name, const char* value_string)
{
    if (!_stricmp(name, "range")) { return params_set_value_by_string(&params->range, value_string); }
    if (!_stricmp(name, "Y")) { return params_set_value_by_string(&params->Y, value_string); }
    if (!_stricmp(name, "Cb")) { return params_set_value_by_string(&params->Cb, value_string); }
    if (!_stricmp(name, "Cr")) { return params_set_value_by_string(&params->Cr, value_string); }
    if (!_stricmp(name, "grainY")) { return params_set_value_by_string(&params->grainY, value_string); }
    if (!_stricmp(name, "grainC")) { return params_set_value_by_string(&params->grainC, value_string); }
    if (!_stricmp(name, "sample_mode")) { return params_set_value_by_string(&params->sample_mode, value_string); }
    if (!_stricmp(name, "seed")) { return params_set_value_by_string(&params->seed, value_string); }
    if (!_stricmp(name, "blur_first")) { return params_set_value_by_string(&params->blur_first, value_string); }
    if (!_stricmp(name, "dynamic_grain")) { return params_set_value_by_string(&params->dynamic_grain, value_string); }
    if (!_stricmp(name, "opt")) { return params_set_value_by_string(&params->opt, value_string); }
    if (!_stricmp(name, "dither_algo")) { return params_set_value_by_string(&params->dither_algo, value_string); }
    if (!_stricmp(name, "keep_tv_range")) { return params_set_value_by_string(&params->keep_tv_range, value_string); }
    if (!_stricmp(name, "output_mode")) { return params_set_value_by_string(&params->output_mode, value_string); }
    if (!_stricmp(name, "output_depth")) { return params_set_value_by_string(&params->output_depth, value_string); }
    if (!_stricmp(name, "random_algo_ref")) { return params_set_value_by_string(&params->random_algo_ref, value_string); }
    if (!_stricmp(name, "random_algo_grain")) { return params_set_value_by_string(&params->random_algo_grain, value_string); }
    if (!_stricmp(name, "random_param_ref")) { return params_set_value_by_string(&params->random_param_ref, value_string); }
    if (!_stricmp(name, "random_param_grain")) { return params_set_value_by_string(&params->random_param_grain, value_string); }
    return F3KDB_ERROR_INVALID_NAME;
}
