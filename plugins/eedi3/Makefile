include ../../config.mak

LIBNAME = eedi3
local_CXXFLAGS += -Wno-unused-variable -Wno-ignored-qualifiers -Isrc -Isrc/vectorclass
ifeq ($(HAVE_OPENCL),yes)
ifeq ($(HAVE_BOOST_SYSTEM),yes)
LIBADD = $(CL_LIBS)
LIBADD += -lboost_system
local_CXXFLAGS += -Wno-deprecated-declarations $(BOOST_CPPFLAGS) -DHAVE_OPENCL $(CL_CFLAGS)
endif
endif
%SSE4.o: VSCXXFLAGS+=-msse4.1
%AVX.o: VSCXXFLAGS+=-mavx
%AVX512.o: VSCXXFLAGS+=-mavx512f -mfma

include ../../cxx.inc

