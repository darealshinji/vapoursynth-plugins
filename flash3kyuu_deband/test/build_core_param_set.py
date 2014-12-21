#!/usr/bin/env python3

import os
import re
import glob
from itertools import product

HEADER = r"""
/*************************
 * Script generated code *
 *     Do not modify     *
 *************************/

#pragma once

#include "../include/f3kdb.h"

"""

NAME_PARSER = re.compile(
    r"^(?P<name>\w+)_(?P<width>\d+)x(?P<height>\d+)_"
    r"(?P<w_subsamp>\d)_(?P<h_subsamp>\d)_(?P<mode>\d)_(?P<depth>\d+)\.yuv$",
    re.I,
)


def process_file(name):
    base_name = os.path.basename(name)
    var_name = re.sub("[^A-Za-z0-9]", "_", os.path.splitext(base_name)[0])
    m = NAME_PARSER.match(base_name)
    print((r"f3kdb_video_info_t frame_{var_name}_vi = "
           r"{{ {width}, {height}, {w_subsamp}, {h_subsamp}, "
           r"(PIXEL_MODE){mode}, {depth}, 0 }};")
            .format(var_name=var_name, **m.groupdict()))

    with open(name, "rb") as f:
        print("const unsigned char frame_{}_data[] = {{ {} }};".format(
            var_name,
            ", ".join("0x{:x}".format(x) for x in f.read()),
        ))


    print(("const case_frame_t frame_{name} = "
           "{{ frame_{name}_vi, frame_{name}_data }};").format(name=var_name))

    return var_name


def convert_param_item(item):
    if isinstance(item, str):
        return item

    keys, value = item
    return "/".join("{}={}".format(x, value) for x in keys)


def generate_param_set():
    params = (
        list(product(
            (("y", "cb", "cr",),),
            (64, 32, 0, 96),
        )),
        list(product(
            (("grainy", "grainc",),),
            (64, 32, 0),
        )),
        (
            "range=15/keep_tv_range=true",
            "range=31/keep_tv_range=false",
        ),
        (
            "sample_mode=2/blur_first=true",
            "sample_mode=2/blur_first=false",
            "sample_mode=1/blur_first=true",
        ),
        (
            "output_depth=8/dither_algo=1",
            "output_depth=8/dither_algo=2",
            "output_depth=8/dither_algo=3",
            "output_depth=10/output_mode=1/dither_algo=1",
            "output_depth=10/output_mode=1/dither_algo=2",
            "output_depth=10/output_mode=1/dither_algo=3",
            "output_depth=10/output_mode=2/dither_algo=1",
            "output_depth=10/output_mode=2/dither_algo=2",
            "output_depth=10/output_mode=2/dither_algo=3",
            "output_depth=16/output_mode=1",
            "output_depth=16/output_mode=2",
        ),
    )
    print("const char* param_set[] = {")
    print(",\n".join([
        '"{}"'.format("/".join(convert_param_item(item) for item in group))
        for group in product(*params)
    ]))
    print("};")


def main():
    print(HEADER)
    frames = []
    for name in sorted(glob.iglob(os.path.join("case_frames", "*.yuv"))):
        frames.append(process_file(name))

    print("const case_frame_t* frames[] = {{ {} }};".format(", ".join(
        "&frame_" + x for x in frames
    )))
    generate_param_set()


if __name__ == '__main__':
    main()
