include ../../config.mak

LIB = libtcomb.so

ifeq ($(HAVE_X86_64),yes)

VSASFLAGS = -Dprivate_prefix=tcomb -DARCH_X86_64=1 -DPIC -f elf64

%.o: %.asm
	$(YASM_silent)$(YASM) $(VSASFLAGS) $(ASMINC) $(ASFLAGS) -o $@ $<

endif

ifeq ($(HAVE_I686),yes)
NOASM = yes
endif

include ../../cc.inc

