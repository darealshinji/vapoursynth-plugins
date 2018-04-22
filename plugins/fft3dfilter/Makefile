include ../../config.mak

ifeq ($(HAVE_FFTW3F),yes)
LIBNAME = fft3dfilter
LIBADD = -lfftw3f_threads -lfftw3f
local_CXXFLAGS = -Wno-reorder
ifeq ($(HAVE_FFTW3_MAKE_PLANNER_THREAD_SAFE),yes)
local_CXXFLAGS += -DHAVE_FFTW3_MAKE_PLANNER_THREAD_SAFE
endif
endif

include ../../cxx.inc

