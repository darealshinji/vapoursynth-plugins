LIBNAME = waifu2x-w2xc
local_CXXFLAGS = -Ilibw2xc/src -Wno-missing-field-initializers
LIBS = libw2xc/libw2xc.a $(shell pkg-config --libs opencv) -fopenmp -ldl

include ../../cxx.inc

