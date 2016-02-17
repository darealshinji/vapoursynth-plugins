include ../../config.mak

PACKAGE_VERSION = 7

LIB = libnnedi3.so
LIBADD = -lm
local_CXXFLAGS = -Wno-unused-variable -DPACKAGE_VERSION='"$(PACKAGE_VERSION)"' -DNNEDI3_X86 -DNNEDI3_DATADIR='"$(datarootdir)/nnedi3"'

ifeq ($(HAVE_YASM),yes)

VSASFLAGS = -f elf -Dprivate_prefix=nnedi3
ifeq ($(HAVE_X86_64),yes)
VSASFLAGS += -DARCH_X86_64=1 -DPIC -m amd64
else
VSASFLAGS += -DARCH_X86_64=0
endif

%.o: %.asm
	$(asmcompile)

endif #HAVE_YASM

include ../../cxx.inc

