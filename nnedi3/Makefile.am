include ../global.inc
include ../config.mak

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libnnedi3.la

yasm_verbose = $(yasm_verbose_$(V))
yasm_verbose_ = $(yasm_verbose_$(AM_DEFAULT_VERBOSITY))
yasm_verbose_0 = @echo "  YASM    " $@;

.asm.lo:
	$(yasm_verbose)$(LIBTOOL) $(AM_V_lt) --mode=compile --tag=CC $(AS) $(ASFLAGS) -o $@ $< -prefer-non-pic

libnnedi3_la_SOURCES = src/nnedi3.cpp \
					   src/cpufeatures.cpp \
					   src/cpufeatures.h

if NNEDI3_X86
libnnedi3_la_SOURCES += src/asm/cpu-a.asm \
						src/asm/nnedi3.asm
endif

libnnedi3_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++11 -DNNEDI3_DATADIR='"$(prefix)/share/nnedi3"'
#-DNNEDI3_DATADIR='"$(pkgdatadir)"'

libnnedi3_la_LIBADD = -lm

