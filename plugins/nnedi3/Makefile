include ../../config.mak

LIBNAME = nnedi3
LIBADD = -lm
local_CXXFLAGS = -DPACKAGE_VERSION='"$(shell cat VERSION)"' -DNNEDI3_X86 -DNNEDI3_DATADIR='"$(datarootdir)/nnedi3"'

ifeq ($(HAVE_YASM),yes)
VSASFLAGS = -f elf -Dprivate_prefix=nnedi3
ifeq ($(HAVE_X86_64),yes)
VSASFLAGS += -DARCH_X86_64=1 -DPIC -m amd64
else
VSASFLAGS += -DARCH_X86_64=0
endif

%.o: %.asm
	$(asmcompile)
endif #HAVE_YASM

%_fma3.o: %_fma3.c
	$(CC_silent)$(CC) $(VSCFLAGS) -mfma -o $@ $^

%_fma4.o: %_fma4.c
	$(CC_silent)$(CC) $(VSCFLAGS) -mfma4 -o $@ $^

include ../../cxx_cc.inc

