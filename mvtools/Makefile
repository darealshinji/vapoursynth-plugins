include ../config.mak

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
CLEANFILES = $(ASMOBJS)


ifeq ($(HAVE_YASM),yes)
ifeq ($(HAVE_FFTW3F),yes)

LIB = libmvtools.so
$(LIB): $(ASMOBJS)
LIBADD = $(ASMOBJS) -lfftw3f

VSASFLAGS = $(ASFLAGS) -Dprivate_prefix=mvtools -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 -f elf
ifeq ($(shell uname -p),x86_64)
VSASFLAGS += -DARCH_X86_64=1 -DPIC -m amd64
else
VSASFLAGS += -DARCH_X86_64=0
endif

%.o: %.asm
	$(YASM_silent)$(YASM) $(VSASFLAGS) -o $@ $<

endif #HAVE_FFTW3F
endif #HAVE_YASM


include ../cxx.inc

