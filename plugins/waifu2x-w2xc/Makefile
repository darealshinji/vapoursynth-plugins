include ../../config.mak

ifeq ($(HAVE_OPENCV),yes)
LIB  = libwaifu2x-w2xc.so
W2XC = w2xc/libw2xc.a

$(LIB): $(W2XC)

local_CXXFLAGS = -I./w2xc/src
LIBS = $(W2XC) $(shell pkg-config --libs opencv) -fopenmp -ldl

# parallel building doesn't work
$(W2XC):
	$(MAKE) -C w2xc -j1
endif

clean-local:
	$(MAKE) -C w2xc clean

include ../../cxx.inc

