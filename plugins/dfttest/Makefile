include ../../config.mak

ifeq ($(HAVE_FFTW3F),yes)
LIBNAME = dfttest
LIBADD = -lfftw3f
local_CXXFLAGS = -Wno-parentheses
endif

include ../../cxx.inc

