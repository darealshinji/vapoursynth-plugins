include ../global.inc

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libfluxsmooth.la

if x86_64
BUILT_SOURCES = src/asm/fluxsmooth.o
CLEANFILES = $(BUILT_SOURCES)
endif

AM_V_YASM = $(am__v_YASM_$(V))
am__v_YASM_ = $(am__v_YASM_$(AM_DEFAULT_VERBOSITY))
am__v_YASM_0 = @echo "  YASM    " $@;
am__v_YASM_1 = 

.asm.o:
	$(AM_V_YASM)$(AS) $(ASFLAGS) -o $@ $<

libfluxsmooth_la_SOURCES = src/fluxsmooth.c

libfluxsmooth_la_LDFLAGS = $(XGLOBAL_LDFLAGS)

if x86_64
libfluxsmooth_la_SOURCES += src/asm/fluxsmooth.asm
libfluxsmooth_la_LDFLAGS += -Xlinker src/asm/fluxsmooth.o
libfluxsmooth_la_DEPENDENCIES = src/asm/fluxsmooth.o
endif
