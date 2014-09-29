include config.mak

vpath %.c $(SRCDIR)
vpath %.h $(SRCDIR)

SRCS = adapt_motion.c combmask.c horizontal_dilation.c is_combed.c \
       merge_frames.c write_combmask.c

OBJS = $(SRCS:%.c=%.o)

.PHONY: all install clean distclean dep

all: $(LIBNAME)

$(LIBNAME): $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $^
	$(if $(STRIP), $(STRIP) $@)

%.o: %.c .depend
	$(CC) -c $(CFLAGS) -o $@ $<

install: all
	install -d $(libdir)
	install -m 755 $(LIBNAME) $(libdir)

clean:
	$(RM) $(OBJS) *.dll *.so *.dylib .depend

distclean: clean
	$(RM) config.*

dep: .depend

ifneq ($(wildcard .depend),)
include .depend
endif

.depend: config.mak
	@$(RM) .depend
	@$(foreach SRC, $(SRCS:%=$(SRCDIR)/%), $(CC) $(SRC) $(CFLAGS) -msse2 -MT $(SRC:$(SRCDIR)/%.c=%.o) -MM >> .depend;)

config.mak:
	./configure
