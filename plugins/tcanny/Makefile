include ../../config.mak

ifeq ($(HAVE_CXX14),1)
LIBNAME = tcanny

local_CXXFLAGS = -Isrc -Isrc/vectorclass -DVS_TARGET_CPU_X86

include ../../cxx.inc

%AVX.o: VSCXXFLAGS+=-mavx
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2
endif

