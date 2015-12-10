
include ../global.inc

ifeq ($(HAVE_YASM),yes)
ifneq ($(NOASM),yes)
_ASMOBJS = $(ASMOBJS)
endif
endif

$(LIB): $(COBJS) $(_ASMOBJS)
	$(CCLD_silent)$(CC) $(VSLDFLAGS) -o $@ $^ $(LIBADD) $(LIBS)

%.o: %.c
	$(CC_silent)$(CC) $(VSCFLAGS) -o $@ $^

