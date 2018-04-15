include ../../config.mak

ifeq ($(HAVE_FFTW3F),yes)
LIBNAME = dfttest
LIBADD = -lfftw3f
local_CXXFLAGS = -DVS_TARGET_CPU_X86 -Isrc -Isrc/vectorclass
%AVX2.o: VSCXXFLAGS+=-mavx2 -mfma
endif

include ../../cxx.inc

