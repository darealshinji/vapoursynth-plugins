include ../../config.mak

LIBNAME = tdeintmod

ifeq ($(HAVE_CXX14),1)
local_CXXFLAGS += -Isrc -Isrc/vectorclass
%AVX2.o: VSCXXFLAGS+=-mfma -mavx2
else
local_CXXFLAGS += -Wno-missing-field-initializers
CXXSRCS = r7/TDeintMod.cpp
endif

include ../../cxx.inc

clean-local:
	-rm -f r7/*.o src/*.o

