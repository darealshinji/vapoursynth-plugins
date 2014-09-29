AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libtcomb.la

if x86_64
BUILT_SOURCES = src/asm/tcomb.o
CLEANFILES = $(BUILT_SOURCES)
endif

.asm.o:
	$(AS) $(ASFLAGS) -o $@ $<

libtcomb_la_SOURCES = src/tcomb.c

libtcomb_la_LDFLAGS = -no-undefined -avoid-version

if x86_64
libtcomb_la_SOURCES += src/asm/tcomb.asm
libtcomb_la_LDFLAGS += -Xlinker src/asm/tcomb.o
libtcomb_la_DEPENDENCIES = src/asm/tcomb.o
endif
