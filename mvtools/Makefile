
LIB = libmvtools.so
local_CXXFLAGS = -msse2 -mfpmath=sse


ASMSRCS = \
	src/asm/const-a.asm \
	src/asm/cpu-a.asm \
	src/asm/pixel-a.asm \
	src/asm/sad-a.asm \
	src/asm/Interpolation.asm \
	src/asm/MVDegrains.asm \
	src/asm/Overlap.asm \
	src/asm/SAD.asm \
	src/asm/Variance.asm

ASMOBJS = $(ASMSRCS:%.asm=%.o)
$(LIB): $(ASMOBJS)
LIBADD = $(ASMOBJS) -lfftw3f
CLEANFILES = $(ASMOBJS)

VSASFLAGS = $(ASFLAGS) -Dprivate_prefix=mvtools -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 -f elf
ifeq ($(shell uname -p),x86_64)
VSASFLAGS += -DARCH_X86_64=1 -DPIC -m amd64
else
VSASFLAGS += -DARCH_X86_64=0
endif

%.o: %.asm
	$(YASM_silent)$(YASM) $(VSASFLAGS) -o $@ $<

include ../cxx.inc

