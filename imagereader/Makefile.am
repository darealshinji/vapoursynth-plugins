AUTORECONF   = $(AUTORECONF_$(V))
AUTORECONF_  = $(AUTORECONF_$(AM_DEFAULT_VERBOSITY))
AUTORECONF_0 = autoreconf -i
AUTORECONF_1 = autoreconf -iv

turbodir = libjpeg-turbo

all-local: clean-local
	test -f "$(turbodir)/configure" || $(AUTORECONF) $(turbodir)
	cd $(turbodir) && \
	./configure --enable-static --disable-shared --with-pic --with-jpeg8
	$(MAKE) -C $(turbodir)
	$(MAKE) -C src
	cp -r src/.libs .

clean-local:
	[ ! -f "$(turbodir)/Makefile" ] || $(MAKE) -C $(turbodir) clean
	rm -rf .libs

distclean-local: clean-local
	[ ! -f "$(turbodir)/Makefile" ] || $(MAKE) -C $(turbodir) cleanclean
	cd $(turbodir) && rm -f config.h.in~ config.h jconfig.h libjpeg.map \
	md5/md5cmp stamp-h1 stamp-h2 tjbench tjbenchtest tjunittest

