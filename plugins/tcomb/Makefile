include ../../config.mak

LIBNAME = tcomb

ifeq ($(HAVE_X86_64),yes)
VSASFLAGS = -Dprivate_prefix=tcomb -DARCH_X86_64=1 -DPIC -f elf64

%.o: %.asm
	$(asmcompile)
endif

ifeq ($(HAVE_I686),yes)
NOASM = yes
endif

include ../../cc.inc

