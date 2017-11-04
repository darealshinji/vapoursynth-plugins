include ../../config.mak

ifeq ($(HAVE_CXX14),1)
LIBNAME = eedi3
local_CXXFLAGS += -Wno-unused-variable -Isrc -Isrc/vectorclass
ifeq ($(HAVE_OPENCL),yes)
ifeq ($(HAVE_BOOST_SYSTEM),yes)
LIBADD = $(CL_LIBS)
#LIBADD += -lboost_system  # needs only headers
local_CXXFLAGS += -Wno-deprecated-declarations $(BOOST_CPPFLAGS) -DHAVE_OPENCL $(CL_CFLAGS)
endif
endif
%SSE4.o: VSCXXFLAGS+=-msse4.1
%AVX.o: VSCXXFLAGS+=-mavx
%AVX512.o: VSCXXFLAGS+=-mavx512f -mfma
endif

include ../../cxx.inc

