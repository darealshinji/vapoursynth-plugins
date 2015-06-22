ifeq ($(V), 1)
MAKE = make V=1
AUTORECONF = autoreconf -iv
else
MAKE = make V=0
AUTORECONF = autoreconf -i
endif

LIB = libfmtconv.so


all: $(LIB)

$(LIB): build/unix/.libs/$(LIB)
	cp $< .

build/unix/.libs/$(LIB): build/unix/Makefile
	$(MAKE) -C build/unix

# -fabi-version=6 is needed to enable debug builds
build/unix/Makefile: build/unix/configure
	cd build/unix && \
	CXXFLAGS="$(CXXFLAGS) -fabi-version=6 $(CPPFLAGS)" \
	LDFLAGS="-Wl,-z,defs -Wl,-z,noexecstack -Wl,--as-needed $(LDFLAGS)" \
	./configure

build/unix/configure:
	mkdir -p build/unix/m4
	$(AUTORECONF) build/unix

clean:
	make -C build/unix $@
	rm -f $(LIB)

distclean:
	make -C build/unix $@
	rm -f $(LIB)

