
LIB = libfluxsmooth.so

ifeq ($(shell uname -p),x86_64)

ASMOBJS = src/asm/fluxsmooth.o
$(LIB): $(ASMOBJS)
LIBADD = $(ASMOBJS)
CLEANFILES = $(ASMOBJS)

VSASFLAGS = -DARCH_X86_64=1 -DPIC -f elf64 $(ASFLAGS)

%.o: %.asm
	yasm $(VSASFLAGS) -o $@ $<

endif

include ../cc.inc

