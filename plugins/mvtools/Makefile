include ../../config.mak

ifeq ($(HAVE_YASM),yes)
ifeq ($(HAVE_FFTW3F),yes)

LIBNAME = mvtools
LIBADD = -lfftw3f
local_CPPFLAGS = -DMVTOOLS_X86 -DPACKAGE_VERSION='"$(shell cat VERSION)"'

VSASFLAGS = -Dprivate_prefix=mvtools -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 -f elf
ifeq ($(HAVE_X86_64),yes)
VSASFLAGS += -DARCH_X86_64=1 -DPIC -m amd64
else
VSASFLAGS += -DARCH_X86_64=0
endif

%.o: %.asm
	$(asmcompile)

%_AVX2.o: %_AVX2.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -mavx2 -o $@ $^

endif #HAVE_FFTW3F
endif #HAVE_YASM

include ../../cxx_cc.inc

