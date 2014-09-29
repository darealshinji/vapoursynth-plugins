include config.mak

SRCS = convo2d.c

OBJS = $(SRCS:%.c=%.o)

.PHONY: all clean distclean

all: $(LIBNAME)

$(LIBNAME): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	$(if $(STRIP), $(STRIP) -x $@)

%.o: %.c .depend
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	$(RM) *.o *.dll *.so

distclean: clean
	$(RM) config.mak .depend

ifneq ($(wildcard .depend),)
include .depend
endif

.depend: config.mak
	@$(RM) .depend
	@$(foreach SRC, $(SRCS), $(CC) $(SRC) $(CFLAGS) -g0 -MT $(SRC:%.c=%.o) -MM >> .depend;)

config.mak:
	./configure
