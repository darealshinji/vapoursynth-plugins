include ../config.mak

ifeq ($(HAVE_FFTW3F),yes)
LIB = libbm3d.so
local_CXXFLAGS = -Wno-unused-local-typedefs
LIBADD = -lfftw3f
endif

include ../cxx.inc

