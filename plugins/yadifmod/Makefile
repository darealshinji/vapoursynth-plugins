include ../../config.mak

LIBNAME = yadifmod

ifeq ($(HAVE_CXX14),1)
local_CXXFLAGS += -Isrc -Isrc/vectorclass
%AVX.o: VSCXXFLAGS+=-mavx
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2
else
CXXSRCS = r9/Yadifmod.cpp
endif

include ../../cxx.inc

clean-local:
	-rm -f r9/*.o src/*.o
