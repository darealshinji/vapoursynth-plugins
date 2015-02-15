#include <VapourSynth.h>


void readRegister(VSRegisterFunction registerFunc, VSPlugin *plugin);
void writeRegister(VSRegisterFunction registerFunc, VSPlugin *plugin);


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.damb", "damb", "Audio file reader and writer", VAPOURSYNTH_API_VERSION, 1, plugin);

    readRegister(registerFunc, plugin);
    writeRegister(registerFunc, plugin);
}
