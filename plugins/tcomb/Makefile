include ../../config.mak

LIBNAME = tcomb

ifeq ($(HAVE_X86_64),yes)
VSASFLAGS = -Dprivate_prefix=tcomb -DARCH_X86_64=1 -DPIC -f elf64

#$(ASMINC)
%.o: %.asm
	$(YASM_silent)$(YASM) $(VSASFLAGS) $(ASFLAGS) -o $@ $<
endif

ifeq ($(HAVE_I686),yes)
NOASM = yes
endif

include ../../cc.inc

