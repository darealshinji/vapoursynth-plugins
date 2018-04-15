include ../../config.mak

LIBNAME = tcanny
local_CXXFLAGS = -DVS_TARGET_CPU_X86 -Isrc -Isrc/vectorclass
%AVX.o: VSCXXFLAGS+=-mavx
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2

include ../../cxx.inc

