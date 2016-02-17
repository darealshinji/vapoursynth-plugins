include ../../config.mak

LIB = libfmtconv.so

ifeq ($(HAVE_X86_64),yes)
local_CXXFLAGS = -mcx16
endif

include ../../cxx.inc

%avx.o: VSCXXFLAGS+=-mavx
%avx2.o %Avx2.o: VSCXXFLAGS+=-mavx2

