#ifndef LIBAVVERSION_H
#define LIBAVVERSION_H

#include "libavcodec/version.h"

#if defined(USE_OLD_FFAPI)
  #undef USE_OLD_FFAPI
#endif

#if LIBAVCODEC_VERSION_MAJOR < 55
  #define USE_OLD_FFAPI
#endif

#endif /* LIBAVVERSION_H */
