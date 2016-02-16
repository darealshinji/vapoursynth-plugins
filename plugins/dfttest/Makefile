include ../config.mak

ifeq ($(HAVE_FFTW3F),yes)
LIB = libdfttest.so
LIBADD = -lfftw3f
local_CXXFLAGS = -Wno-parentheses
endif

include ../cxx.inc
