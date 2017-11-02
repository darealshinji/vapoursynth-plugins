include ../../config.mak

ifeq ($(HAVE_FFTW3F),yes)
LIBNAME = bm3d
local_CXXFLAGS = -Wno-unused-local-typedefs
LIBADD = -lfftw3f
endif

include ../../cxx.inc

