include ../config.mak

LIB = libnnedi3.so

local_CXXFLAGS = -Wno-unused-variable -std=c++11 -mfpmath=sse -msse2 -DNNEDI3_DATADIR='"$(prefix)/share/nnedi3"'

ASMSRCS = src/asm/cpu-a.asm src/asm/nnedi3.asm
ASMOBJS = $(ASMSRCS:%.asm=%.o)
$(LIB): $(ASMOBJS)
LIBADD = $(ASMOBJS) -lm
CLEANFILES = $(ASMOBJS)

VSASFLAGS = $(ASFLAGS) -f elf -Dprivate_prefix=nnedi3
ifeq ($(shell uname -p),x86_64)
VSASFLAGS += -DARCH_X86_64=1 -DPIC -m amd64
else
VSASFLAGS += -DARCH_X86_64=0
endif

%.o: %.asm
	yasm $(VSASFLAGS) -o $@ $<

include ../cxx.inc


../config.mak:
	cd .. && ./configure.sh

