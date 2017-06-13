/*
    Copyright (C) 2017  HolyWu

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mpls_parse.h"
#include <libbluray/bluray.h>

#include <algorithm>
#include <string>

#include <VapourSynth.h>
#include <VSHelper.h>

static void VS_CC readMplsCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    int err;

    const std::string source{ vsapi->propGetData(in, "source", 0, nullptr) };
    const int angle = int64ToIntS(vsapi->propGetInt(in, "angle", 0, &err));

    MPLS_PL * pl = bd_read_mpls(source.c_str());
    if (!pl)
        return vsapi->setError(out, ("ReadMpls: failed to open " + source).c_str());

    vsapi->propSetInt(out, "count", pl->list_count, paReplace);
    for (unsigned i = 0; i < pl->list_count; i++) {
        const MPLS_PI * pi = &pl->play_item[i];

        unsigned effectiveAngle = 0;
        if (pi->is_multi_angle) {
            if (angle < 0 || angle >= pi->angle_count)
                return vsapi->setError(out, ("ReadMpls: angle index out of range. There are only " + std::to_string(pi->angle_count) + " angles in the playlist").c_str());
            effectiveAngle = angle;
        }

        const std::string filename = std::string{ pi->clip[effectiveAngle].clip_id } + ".m2ts";
        vsapi->propSetData(out, "filename", filename.c_str(), -1, paAppend);
        vsapi->propSetData(out, "clip", (source.substr(0, source.find_last_of("/\\") - 8) + "STREAM/" + filename).c_str(), -1, paAppend);
    }
    if (pl->list_count == 1) {
        vsapi->propSetData(out, "filename", "", -1, paAppend);
        vsapi->propSetData(out, "clip", "", -1, paAppend);
    }

    bd_free_mpls(pl);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.holywu.readmpls", "mpls", "Reads a mpls file and returns a dictionary", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Read",
                 "source:data;"
                 "angle:int:opt;",
                 readMplsCreate, nullptr, plugin);
}
