#include <VapourSynth.h>

void VS_CC classicCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
void VS_CC levelsCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
void VS_CC colorCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
void VS_CC color2Create(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
void VS_CC lumaCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.histogram", "hist", "VapourSynth Histogram Plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Classic", "clip:clip;", classicCreate, 0, plugin);
    registerFunc("Levels", "clip:clip;factor:float:opt;", levelsCreate, 0, plugin);
    registerFunc("Color", "clip:clip;", colorCreate, 0, plugin);
    registerFunc("Color2", "clip:clip;", color2Create, 0, plugin);
    registerFunc("Luma", "clip:clip;", lumaCreate, 0, plugin);
}
