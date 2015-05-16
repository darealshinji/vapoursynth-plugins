
turbodir = libjpeg-turbo

all: libimagereader.so

libimagereader.so: $(turbodir)/libturbojpeg.la
	$(MAKE) -f imagereader.mk

$(turbodir)/libturbojpeg.la: $(turbodir)/libjpeg.la
	$(MAKE) -C $(turbodir) libturbojpeg.la

$(turbodir)/libjpeg.la: $(turbodir)/simd/libsimd.la
	$(MAKE) -C $(turbodir) libjpeg.la

$(turbodir)/simd/libsimd.la: $(turbodir)/Makefile
	$(MAKE) -C $(turbodir)/simd

$(turbodir)/Makefile: $(turbodir)/configure
	cd $(turbodir) && \
	CFLAGS="$(CFLAGS) -Wno-attributes -Wno-unused-but-set-variable" CPPFLAGS="$(CPPFLAGS)" \
	./configure --disable-silent-rules --enable-static --disable-shared --with-pic --with-jpeg8

$(turbodir)/configure:
	mkdir -p $(turbodir)/m4
	autoreconf -ivf $(turbodir)

clean:
	$(MAKE) -C $(turbodir) clean || true
	$(MAKE) -f imagereader.mk clean

distclean:
	$(MAKE) -C $(turbodir) distclean || true
	$(MAKE) -f imagereader.mk distclean

clobber:
	cd libjpeg-turbo && rm -rf Makefile.in aclocal.m4 autom4te.cache/ build-aux/ config.h.in \
	configure m4/ md5/Makefile.in simd/Makefile.in simd/jsimdcfg.inc

