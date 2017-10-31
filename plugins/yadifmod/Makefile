include ../../config.mak

ifeq ($(HAVE_CXX14),1)
LIBNAME = yadifmod
local_CXXFLAGS += -Isrc -Isrc/vectorclass
%AVX.o: VSCXXFLAGS+=-mavx
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2
endif

include ../../cxx.inc

