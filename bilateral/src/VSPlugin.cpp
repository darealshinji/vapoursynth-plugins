#include "Bilateral.h"
#include "Gaussian.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin)
{
    configFunc("mawen1250.Bilateral", "bilateral",
        "Bilateral filter and Gaussian filter for VapourSynth.",
        VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("Bilateral", "input:clip;ref:clip:opt;sigmaS:float[]:opt;sigmaR:float[]:opt;planes:int[]:opt;algorithm:int[]:opt;PBFICnum:int[]:opt", BilateralCreate, nullptr, plugin);
    registerFunc("Gaussian", "input:clip;sigma:float[]:opt;sigmaV:float[]:opt", GaussianCreate, nullptr, plugin);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
