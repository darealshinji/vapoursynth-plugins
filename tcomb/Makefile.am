include ../global.inc

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libtcomb.la

if x86_64
BUILT_SOURCES = src/asm/tcomb.o
CLEANFILES = $(BUILT_SOURCES)
endif

AM_V_YASM = $(am__v_YASM_$(V))
am__v_YASM_ = $(am__v_YASM_$(AM_DEFAULT_VERBOSITY))
am__v_YASM_0 = @echo "  YASM    " $@;
am__v_YASM_1 = 

.asm.o:
	$(AM_V_YASM)$(AS) $(ASFLAGS) -o $@ $<

libtcomb_la_SOURCES = src/tcomb.c

libtcomb_la_LDFLAGS = $(XGLOBAL_LDFLAGS)

if x86_64
libtcomb_la_SOURCES += src/asm/tcomb.asm
libtcomb_la_LDFLAGS += -Xlinker src/asm/tcomb.o
libtcomb_la_DEPENDENCIES = src/asm/tcomb.o
endif
