include ../../config.mak

ifeq ($(HAVE_FFTW3F),yes)
ifeq ($(HAVE_FFTW3F_NEWER),yes)
LIB = libbm3d.so
local_CXXFLAGS = -Wno-unused-local-typedefs
LIBADD = -lfftw3f
endif
endif

include ../../cxx.inc

