include ../../config.mak

LIBNAME = ctmf

ifeq ($(HAVE_CXX14),1)
local_CXXFLAGS += -Isrc/vectorclass
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2
else
CXXSRCS = r2/CTMF.cpp
endif

include ../../cxx.inc

clean-local:
	-rm -f r5/*.o src/*.o

