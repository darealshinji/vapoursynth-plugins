
LIB = libtcomb.so

ifeq ($(shell uname -p),x86_64)

ASMOBJS = src/asm/tcomb.o
$(LIB): $(ASMOBJS)
LIBADD = $(ASMOBJS)
CLEANFILES = $(ASMOBJS)

VSASFLAGS = -Dprivate_prefix=tcomb -DARCH_X86_64=1 -DPIC -f elf64

%.o: %.asm
	$(YASM_silent)$(YASM) $(VSASFLAGS) $(ASINC) $(ASFLAGS) -o $@ $<

endif

include ../cc.inc

