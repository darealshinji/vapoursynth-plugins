include ../config.mak

LIB = libnnedi3.so
LIBADD = -lm
local_CXXFLAGS = -Wno-unused-variable -DNNEDI3_X86 -DNNEDI3_DATADIR='"$(datarootdir)/nnedi3"'


ifeq ($(HAVE_YASM),yes)

ASMSRCS = src/asm/cpu-a.asm src/asm/nnedi3.asm
ASMOBJS = $(ASMSRCS:%.asm=%.o)
$(LIB): $(ASMOBJS)
LIBADD = $(ASMOBJS)
CLEANFILES = $(ASMOBJS)

VSASFLAGS = $(ASFLAGS) -f elf -Dprivate_prefix=nnedi3
ifeq ($(shell uname -p),x86_64)
VSASFLAGS += -DARCH_X86_64=1 -DPIC -m amd64
else
VSASFLAGS += -DARCH_X86_64=0
endif

%.o: %.asm
	$(YASM_silent)$(YASM) $(VSASFLAGS) -o $@ $<

endif #HAVE_YASM


include ../cxx.inc

