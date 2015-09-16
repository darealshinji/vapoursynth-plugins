include ../config.mak

ifeq ($(HAVE_OPENCL),yes)
LIB = libknlmeanscl.so

local_CXXFLAGS = $(CL_CFLAGS)
LIBADD = $(CL_LIBS)
endif

include ../cxx.inc

