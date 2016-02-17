LIBNAME = waifu2x-w2xc
local_CXXFLAGS = -I./waifu2x/src -Wno-missing-field-initializers
LIBS = waifu2x/libw2xc.a $(shell pkg-config --libs opencv) -fopenmp -ldl

include ../../cxx.inc

