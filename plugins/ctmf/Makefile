include ../../config.mak

LIBNAME = ctmf
local_CXXFLAGS += -Isrc/vectorclass
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2

include ../../cxx.inc

