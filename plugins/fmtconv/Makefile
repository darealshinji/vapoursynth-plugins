include ../../config.mak

LIBNAME = fmtconv

ifeq ($(HAVE_X86_64),yes)
local_CXXFLAGS = -mcx16
endif

%avx.o: VSCXXFLAGS+=-mavx
%avx2.o %Avx2.o: VSCXXFLAGS+=-mavx2

include ../../cxx.inc

