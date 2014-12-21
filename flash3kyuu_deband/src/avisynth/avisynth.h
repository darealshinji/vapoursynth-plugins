#pragma once

#if USE_AVISYNTH_INTERFACE == 5
#include "include/interface_v5/avisynth.h"
#elif USE_AVISYNTH_INTERFACE == 3
#include "include/interface_legacy_26/avisynth.h"
#else
#error "USE_AVISYNTH_INTERFACE must be either 3 or 5"
#endif

