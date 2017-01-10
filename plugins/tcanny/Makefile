include ../../config.mak

LIBNAME = tcanny
local_CXXFLAGS = -DVS_TARGET_CPU_X86

ifeq ($(HAVE_CXX14),1)
local_CXXFLAGS += -Isrc -Isrc/vectorclass
%AVX.o: VSCXXFLAGS+=-mavx
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2
else
CXXSRCS = r5/TCanny.cpp
endif

include ../../cxx.inc

clean-local:
	-rm -f r5/*.o src/*.o

