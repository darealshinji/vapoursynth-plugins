include ../config.mak

LIB = libfluxsmooth.so

ifeq ($(shell uname -p),x86_64)
ifeq ($(HAVE_YASM),yes)

VSASFLAGS = -DARCH_X86_64=1 -DPIC -f elf64

%.o: %.asm
	$(asmcompile)

endif #HAVE_YASM
endif #x86_64

include ../cc.inc

