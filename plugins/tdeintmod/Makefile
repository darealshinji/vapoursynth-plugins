include ../../config.mak

LIBNAME = tdeintmod
local_CXXFLAGS += -Isrc -Isrc/vectorclass
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2

include ../../cxx.inc

