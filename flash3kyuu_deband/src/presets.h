#pragma once

static const char* const PRESETS[] = {
    "depth",
    "y=0/cb=0/cr=0/grainy=0/grainc=0",
    "low",
    "y=32/cb=32/cr=32/grainy=32/grainc=32",
    "medium",
    "y=48/cb=48/cr=48/grainy=48/grainc=48",
    "high",
    "y=64/cb=64/cr=64/grainy=64/grainc=64",
    "veryhigh",
    "y=80/cb=80/cr=80/grainy=80/grainc=80",
    "nograin",
    "grainy=0/grainc=0",
    "luma",
    "cb=0/cr=0/grainc=0",
    "chroma",
    "y=0/grainy=0",
    nullptr, nullptr
};

static_assert((sizeof(PRESETS) / sizeof(PRESETS[0])) % 2 == 0, "Incorrect preset definition");

