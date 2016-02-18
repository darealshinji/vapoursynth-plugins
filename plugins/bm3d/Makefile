include ../../config.mak

ifeq ($(HAVE_FFTW3F),yes)
ifeq ($(HAVE_FFTW3F_NEWER),yes)
LIBNAME = bm3d
local_CXXFLAGS = -Wno-unused-local-typedefs
LIBADD = -lfftw3f

include ../../cxx.inc
endif
endif
