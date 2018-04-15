include ../../config.mak

ifeq ($(HAVE_OPENCL),yes)
LIBNAME = knlmeanscl
local_CXXFLAGS = $(CL_CFLAGS) -Wno-deprecated-declarations -Wno-unused-variable
LIBADD = $(CL_LIBS)
endif

include ../../cxx.inc

