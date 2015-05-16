
include ../global.inc

$(LIB): $(COBJS)
	$(CC) $(VSLDFLAGS) -o $@ $(COBJS) $(LIBADD) $(LIBS)

%.o: %.c
	$(CC) $(VSCFLAGS) -o $@ $^

