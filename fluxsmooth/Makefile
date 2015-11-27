include ../config.mak

LIB = libfluxsmooth.so
ASMOBJS = src/asm/fluxsmooth.o
CLEANFILES = $(ASMOBJS)

ifeq ($(shell uname -p),x86_64)
ifeq ($(HAVE_YASM),yes)

$(LIB): $(ASMOBJS)
LIBADD = $(ASMOBJS)
VSASFLAGS = -DARCH_X86_64=1 -DPIC -f elf64

%.o: %.asm
	$(YASM_silent)$(YASM) $(VSASFLAGS) $(ASINC) $(ASFLAGS) -o $@ $<

endif #HAVE_YASM
endif #x86_64

include ../cc.inc

