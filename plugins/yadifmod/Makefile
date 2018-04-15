include ../../config.mak

LIBNAME = yadifmod
local_CXXFLAGS += -Isrc -Isrc/vectorclass
%AVX.o: VSCXXFLAGS+=-mavx
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2

include ../../cxx.inc

