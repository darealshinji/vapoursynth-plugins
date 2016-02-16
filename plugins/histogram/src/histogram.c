#include <VapourSynth.h>

#include "classic.h"
#include "levels.h"
#include "color.h"
#include "color2.h"
#include "luma.h"



VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.histogram", "hist", "VapourSynth Histogram Plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("Classic", "clip:clip;", classicCreate, 0, plugin);
   registerFunc("Levels", "clip:clip;factor:float:opt;", levelsCreate, 0, plugin);
   registerFunc("Color", "clip:clip;", colorCreate, 0, plugin);
   registerFunc("Color2", "clip:clip;", color2Create, 0, plugin);
   registerFunc("Luma", "clip:clip;", lumaCreate, 0, plugin);
}
