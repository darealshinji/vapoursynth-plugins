include ../../config.mak

ifeq ($(HAVE_AVCODEC),yes)
ifeq ($(HAVE_AVFORMAT),yes)
ifeq ($(HAVE_AVUTIL),yes)

LIBNAME = d2vsource
local_CXXFLAGS = -Wno-sign-compare
LIBADD = -lavcodec -lavformat -lavutil

endif
endif
endif

include ../../cxx.inc

