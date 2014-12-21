#!/usr/bin/env python3

import os
import sys

def generate_output():
    p = FilterParam
    params = (
        p("c", "child", optional=False, has_field=False,
          scope_exclude=["vapoursynth"]),
        p("i", "range", default_value=15),
        p("i", "Y", c_type="unsigned short", default_value=64),
        p("i", "Cb", c_type="unsigned short", default_value=64),
        p("i", "Cr", c_type="unsigned short", default_value=64),
        p("i", "grainY", default_value=64),
        p("i", "grainC", default_value=64),
        p("i", "sample_mode", default_value=2),
        p("i", "seed", default_value=0),
        p("b", "blur_first", default_value="true"),
        p("b", "dynamic_grain", default_value="false"),
        p("i", "opt", c_type="OPTIMIZATION_MODE",
          default_value="IMPL_AUTO_DETECT"),
        p("b", "mt", scope=["avisynth"], has_field=False),
        p("i", "dither_algo", c_type="DITHER_ALGORITHM", 
          default_value="DA_HIGH_FLOYD_STEINBERG_DITHERING"),
        p("b", "keep_tv_range", default_value="false"),
        p("i", "input_mode", c_type="PIXEL_MODE", has_field=False,
          scope=["avisynth"], default_value="DEFAULT_PIXEL_MODE"),
        p("i", "input_depth", default_value=-1, has_field=False,
          scope=["avisynth"]),
        p("i", "output_mode", c_type="PIXEL_MODE", 
          scope_exclude=["vapoursynth"], default_value="DEFAULT_PIXEL_MODE"),
        p("i", "output_depth", default_value=-1),
        p("i", "random_algo_ref", c_type="RANDOM_ALGORITHM", 
          default_value="RANDOM_ALGORITHM_UNIFORM"),
        p("i", "random_algo_grain", c_type="RANDOM_ALGORITHM",
          default_value="RANDOM_ALGORITHM_UNIFORM"),
        p("f", "random_param_ref",
          default_value="DEFAULT_RANDOM_PARAM"),
        p("f", "random_param_grain",
          default_value="DEFAULT_RANDOM_PARAM"),
        p("s", "preset", optional=True, has_field=False,
          scope=["avisynth", "vapoursynth"]),
    )

    def _generate(file_name, template, scope):
        if isinstance(file_name, list):
            file_name = os.path.join(*file_name)

        if "--list-outputs" in sys.argv:
            print(file_name)
            return

        with open(file_name, "w") as f:
            f.write(generate_definition(
                "f3kdb",
                template,
                scope,
                *params
            ))

    _generate(
        ["avisynth", "flash3kyuu_deband.def.h"],
        OUTPUT_TEMPLATE_AVISYNTH,
        "avisynth",
    )
    _generate(
        ["vapoursynth", "plugin.def.h"],
        OUTPUT_TEMPLATE_VAPOURSYNTH,
        "vapoursynth",
    )
    _generate(
        ["../", "include", "f3kdb_params.h"],
        OUTPUT_TEMPLATE_PUBLIC_PARAMS,
        "public_params",
    )
    _generate(
        ["auto_utils.cpp"],
        OUTPUT_TEMPLATE_AUTO_UTILS,
        "common",
    )

PARAM_TYPES = {
    #     AVS type       AVS accessor     VS type
    "c": ("PClip",       None,            None),
    "b": ("bool",        "AsBool",        "int"),
    "i": ("int",         "AsInt",         "int"),
    "f": ("double",      "AsFloat",       "float"),
    "s": ("const char*", "AsString",      "data"),
}

class FilterParam:
    def __init__(
            self, 
            type, 
            name, 
            field_name=None,
            c_type=None, 
            optional=True,
            has_field=True,
            scope=None,
            scope_exclude=None,
            default_value=None,
    ):
        if type not in PARAM_TYPES.keys():
            raise ValueError("Type {} is not supported.".format(type))

        if scope and isinstance(scope, str):
            raise ValueError("scope must be a collection, not a string")

        if scope_exclude and isinstance(scope_exclude, str):
            raise ValueError("scope must be a collection, not a string")

        self.type = type
        self.name = name
        self.field_name = field_name or name
        self.c_type = c_type or PARAM_TYPES[type][0]
        self.custom_c_type = c_type is not None
        self.converter = PARAM_TYPES[type][1]
        self.vs_type = PARAM_TYPES[type][2]
        self.optional = optional
        self.has_field = has_field
        self.scope = scope or []
        self.scope_exclude = scope_exclude or []
        self.default_value = default_value

OUTPUT_HEADER = """
/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/
"""

OUTPUT_TEMPLATE_PUBLIC_PARAMS = """
#pragma once

#include "f3kdb_enums.h"

typedef struct _f3kdb_params_t
{{
    {class_field_def_public}
}} f3kdb_params_t;

"""

OUTPUT_TEMPLATE_AUTO_UTILS = """
#include <string>

#include "random.h"
#include "auto_utils.h"
#include "auto_utils_helper.h"

void params_set_defaults(f3kdb_params_t* params)
{{
    {params_set_defaults}
}}

int params_set_by_string(f3kdb_params_t* params, const char* name, const char* value_string)
{{
    {params_set_by_string}
    return F3KDB_ERROR_INVALID_NAME;
}}
"""

OUTPUT_TEMPLATE_AVISYNTH = """
#pragma once

#include <stddef.h>

#include "avisynth.h"
#include <f3kdb.h>

static const char* {filter_name_u}_AVS_PARAMS = "{avs_params}";

typedef struct _{filter_name_u}_RAW_ARGS
{{
    AVSValue {init_param_list};
}} {filter_name_u}_RAW_ARGS;

#define {filter_name_u}_ARG_INDEX(name) (offsetof({filter_name_u}_RAW_ARGS, name) / sizeof(AVSValue))

#define {filter_name_u}_ARG(name) args[{filter_name_u}_ARG_INDEX(name)]

#ifdef {filter_name_u}_SIMPLE_MACRO_NAME

#ifdef SIMPLE_MACRO_NAME
#error Simple macro name has already been defined for SIMPLE_MACRO_NAME
#endif

#define SIMPLE_MACRO_NAME {filter_name}

#define ARG {filter_name_u}_ARG

#endif

static void f3kdb_params_from_avs(AVSValue args, f3kdb_params_t* f3kdb_params)
{{
    {f3kdb_params_from_avs}
}}

"""

OUTPUT_TEMPLATE_VAPOURSYNTH = """
#pragma once

#include <f3kdb.h>
#include "plugin.h"
#include "VapourSynth.h"

static const char* F3KDB_VAPOURSYNTH_PARAMS = "clip:clip;{vapoursynth_params}";

static bool f3kdb_params_from_vs(f3kdb_params_t* f3kdb_params, const VSMap* in, VSMap* out, const VSAPI* vsapi)
{{
    {f3kdb_params_from_vs}
    return true;
}}
"""

def build_avs_params(params):
    def get_param(param):
        return param.optional and '[{0.name}]{0.type}'.format(param) or param.type

    return ''.join([get_param(x) for x in params])

def build_init_param_list_invoke(params, predicate=lambda x:True):
    return ", ".join([x.custom_c_type and "({}){}".format(x.c_type, x.field_name) or x.field_name for x in params if predicate(x)])

def build_declaration_list(params, name_prefix='', predicate=lambda x:True):
    return ["{} {}{}".format(x.c_type, name_prefix, x.field_name) for x in params if predicate(x)]

def build_init_param_list_func_def(params, predicate=lambda x: True):
    return ", ".join(build_declaration_list(params, predicate=predicate))

def build_class_field_def(params, prefix="_"):
    return "\n    ".join([x + "; " for x in build_declaration_list(params, prefix, lambda x: x.has_field)])

def build_class_field_init(params):
    return "\n        ".join(["_{0} = {0}; ".format(x.field_name) for x in params if x.has_field])

def build_class_field_copy(params):
    return "\n        ".join(["_{0} = o._{0}; ".format(x.field_name) for x in params if x.has_field])

def build_params_set_defaults(params):
    return "\n    ".join(["params->{0} = {1};".format(x.field_name, x.default_value) for x in params if x.has_field])

def build_params_set_by_string(params):
    return "\n    ".join([
        """if (!_stricmp(name, "{field_name}")) {{ return params_set_value_by_string(&params->{field_name}, value_string); }}""".
        format(
            field_name=x.field_name,
        ) 
        for x in params if x.has_field])

def build_f3kdb_params_from_avs(filter_name, params):
    params = [x for x in params if x.has_field]
    return "\n    ".join([
        "if ({filter_name_u}_ARG({field_name}).Defined()) {{ f3kdb_params->{field_name} = {type_conversion}{filter_name_u}_ARG({field_name}).{converter}(); }}".
        format(
            filter_name_u=filter_name.upper(),
            field_name=x.field_name,
            type_conversion="({})".format(x.c_type) if x.custom_c_type else "",
            converter=x.converter,
        ) 
        for x in params if x.has_field])

def build_vapoursynth_params(params):
    return "".join(["{}:{}{};".format(
                        x.field_name.lower(),
                        x.vs_type,
                        ":opt" if x.optional else "",
                    )
                    for x in params])

def build_f3kdb_params_from_vs(params):
    params = [x for x in params if x.has_field]
    return "\n    ".join([
        """if (!param_from_vsmap(&f3kdb_params->{field_name}, "{field_name_l}", in, out, vsapi)) {{ return false; }}""".
        format(
            field_name=x.field_name,
            field_name_l=x.field_name.lower(),
        ) 
        for x in params])

def generate_definition(filter_name, template, scope, *params):
    params = [x for x in params 
              if (not x.scope or scope in x.scope) and 
                 (not x.scope_exclude or scope not in x.scope_exclude)]
    format_params = {
        "filter_name": filter_name,
        "filter_name_u": filter_name.upper(),
        "avs_params": build_avs_params(params),
        "init_param_list": ', '.join([x.field_name for x in params]),
        "init_param_list_with_field_invoke": build_init_param_list_invoke(params, lambda x: x.has_field),
        "init_param_list_without_field_invoke": build_init_param_list_invoke(params, lambda x: not x.has_field),
        "init_param_list_with_field_func_def": build_init_param_list_func_def(params, lambda x: x.has_field),
        "class_field_def": build_class_field_def(params),
        "class_field_def_public": build_class_field_def(params, prefix=""),
        "class_field_init": build_class_field_init(params),
        "class_field_copy": build_class_field_copy(params),
        "params_set_defaults": build_params_set_defaults(params),
        "params_set_by_string": build_params_set_by_string(params),
        "f3kdb_params_from_avs": 
        build_f3kdb_params_from_avs(filter_name, params),
        "vapoursynth_params": build_vapoursynth_params(params),
        "f3kdb_params_from_vs": build_f3kdb_params_from_vs(params),
    }

    return OUTPUT_HEADER + template.format(**format_params)


if __name__ == "__main__":
    generate_output()

