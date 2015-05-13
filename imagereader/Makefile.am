AUTORECONF   = $(AUTORECONF_$(V))
AUTORECONF_  = $(AUTORECONF_$(AM_DEFAULT_VERBOSITY))
AUTORECONF_0 = autoreconf -i
AUTORECONF_1 = autoreconf -iv

turbodir = libjpeg-turbo

all-local: clean-local src/.libs
	cp -r src/.libs .

src/.libs: $(turbodir)/libturbojpeg.la
	$(MAKE) -C src

$(turbodir)/libturbojpeg.la: $(turbodir)/libjpeg.la
	$(MAKE) -C $(turbodir) libturbojpeg.la

$(turbodir)/libjpeg.la: $(turbodir)/simd/libsimd.la
	$(MAKE) -C $(turbodir) libjpeg.la

$(turbodir)/simd/libsimd.la: $(turbodir)/Makefile
	$(MAKE) -C $(turbodir)/simd

$(turbodir)/Makefile: $(turbodir)/configure
	cd $(turbodir) && \
	CFLAGS="$(AM_CFLAGS) $(CFLAGS) -Wno-attributes -Wno-unused-but-set-variable" \
	CPPFLAGS="$(AM_CPPFLAGS) $(CPPFLAGS)" \
	./configure --enable-static --disable-shared --with-pic --with-jpeg8

$(turbodir)/configure:
	$(AUTORECONF) $(turbodir)

clean-local:
	[ ! -f "$(turbodir)/Makefile" ] || $(MAKE) -C $(turbodir) clean
	rm -rf .libs

distclean-local: clean-local
	[ ! -f "$(turbodir)/Makefile" ] || $(MAKE) -C $(turbodir) cleanclean
	cd $(turbodir) && rm -f config.h.in~ config.h jconfig.h libjpeg.map \
	md5/md5cmp stamp-h1 stamp-h2 tjbench tjbenchtest tjunittest

