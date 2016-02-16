include ../config.mak

ifeq ($(HAVE_AVCODEC),yes)
ifeq ($(HAVE_AVFORMAT),yes)
ifeq ($(HAVE_AVUTIL),yes)

LIB = libd2vsource.so
local_CXXFLAGS = -Wno-sign-compare
LIBADD = -lavcodec -lavformat -lavutil

endif
endif
endif

include ../cxx.inc

