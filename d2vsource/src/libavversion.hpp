#ifndef LIBAVVERSION_H
#define LIBAVVERSION_H

#include <libavcodec/version.h>

#if LIBAVCODEC_VERSION_MAJOR <= 54
  #if LIBAVCODEC_VERSION_MINOR <= 35
    #define __OLD_AVCODEC_API
  #endif
#endif

#endif
