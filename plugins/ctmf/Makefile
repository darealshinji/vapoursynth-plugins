include ../../config.mak

ifeq ($(HAVE_CXX14),1)
LIBNAME = ctmf
local_CXXFLAGS += -Isrc/vectorclass
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2
endif

include ../../cxx.inc

