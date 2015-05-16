
LIB = libtcomb.so
local_CFLAGS = -std=gnu99

ifeq ($(shell uname -p),x86_64)

ASMOBJS = src/asm/tcomb.o
$(LIB): $(ASMOBJS)
LIBADD = $(ASMOBJS)
CLEANFILES = $(ASMOBJS)

VSASFLAGS = -Dprivate_prefix=tcomb -DARCH_X86_64=1 -DPIC -f elf64 $(ASFLAGS)

%.o: %.asm
	yasm $(VSASFLAGS) -o $@ $<

endif

include ../cc.inc

