AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libfluxsmooth.la

if x86_64
BUILT_SOURCES = src/asm/fluxsmooth.o
CLEANFILES = $(BUILT_SOURCES)
endif

.asm.o:
	$(AS) $(ASFLAGS) -o $@ $<

libfluxsmooth_la_SOURCES = src/fluxsmooth.c

libfluxsmooth_la_LDFLAGS = -no-undefined -avoid-version

if x86_64
libfluxsmooth_la_SOURCES += src/asm/fluxsmooth.asm
libfluxsmooth_la_LDFLAGS += -Xlinker src/asm/fluxsmooth.o
libfluxsmooth_la_DEPENDENCIES = src/asm/fluxsmooth.o
endif
