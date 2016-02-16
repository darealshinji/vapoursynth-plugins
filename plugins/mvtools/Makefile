include ../config.mak

ifeq ($(HAVE_YASM),yes)
ifeq ($(HAVE_FFTW3F),yes)

LIB = libmvtools.so
LIBADD = -lfftw3f

VSASFLAGS = -Dprivate_prefix=mvtools -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 -f elf
ifeq ($(HAVE_X86_64),yes)
VSASFLAGS += -DARCH_X86_64=1 -DPIC -m amd64
else
VSASFLAGS += -DARCH_X86_64=0
endif

#reminder: we keep our own copy of x86inc.asm because of compatibility issues

%.o: %.asm
	$(asmcompile)

endif #HAVE_FFTW3F
endif #HAVE_YASM

include ../cxx.inc

