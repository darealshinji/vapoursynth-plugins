
include ../global.inc

$(LIB): $(COBJS)
	$(CCLD_silent)$(CC) $(VSLDFLAGS) -o $@ $(COBJS) $(LIBADD) $(LIBS)

%.o: %.c
	$(CC_silent)$(CC) $(VSCFLAGS) -o $@ $^

