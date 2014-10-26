include config.mak

plugins = $(libdir)/vapoursynth/
turbodir = imagereader/libjpeg-turbo

ifeq ($(V), 1)
MAKE = make -f Makefile V=1
AUTORECONF = autoreconf -iv
else
MAKE = make -f Makefile
AUTORECONF = autoreconf -i
endif

install = install -m644 -D
install_DIR = install -m755 -d

CLEANDIRS = . fluxsmooth mvtools nnedi3 tcomb $(turbodir)

execstack_LIBS = fluxsmooth nnedi3 tcomb

python_SCRIPTS = \
	finesharp/finesharp.py \
    havsfunc/havsfunc.py \
	templinearapproximate/MCDenoise.py \
	temporalsoften2/TemporalSoften2.py \
	vautodeint/VAutoDeint.py

define NL


endef


all: dirs
	test -f configure || $(AUTORECONF)
	test -f $(turbodir)/configure || (cd $(turbodir) && $(AUTORECONF))
	test -f $(turbodir)/Makefile  || (cd $(turbodir) && \
	./configure --enable-static=yes --enable-shared=no --with-pic)
	test -f Makefile  || ./configure
	cd $(turbodir) && $(MAKE)
	$(MAKE)

all-am:
clean-am:
distclean-am:

dirs:
	$(foreach DIR,$(CLEANDIRS),mkdir -p $(DIR)/m4 $(DIR)/build-aux ;)

install:
	$(install_DIR) $(DESTDIR)$(plugins)
	$(install_DIR) $(DESTDIR)$(docdir)
	$(foreach LIB,$(shell echo */.libs/*.so),$(install) $(LIB) $(DESTDIR)$(plugins) $(NL))
	$(foreach LIB,$(execstack_LIBS),execstack --clear-execstack $(DESTDIR)$(plugins)/lib$(LIB).so $(NL))
	$(foreach SCRIPT,$(python_SCRIPTS),$(install) $(SCRIPT) $(DESTDIR)$(plugins) $(NL))
	$(install) README $(DESTDIR)$(docdir)
	$(install) rawsource/format_list.txt $(DESTDIR)$(docdir)/rawsource_format_list
	$(foreach FILE,$(shell echo */readme* */README*), \
		$(install) $(FILE) $(DESTDIR)$(docdir)/$(shell echo $$(dirname $(FILE))) $(NL))

clean:
	$(MAKE) clean || true
	(cd $(turbodir) && $(MAKE) clean) || true

distclean:
	$(MAKE) distclean || true
	(cd $(turbodir) && $(MAKE) distclean) || true
	rm -f config.mak $(turbodir)/config.h.in~

clobber:
	$(MAKE) distclean || true
	(cd $(turbodir) && $(MAKE) distclean) || true
	rm -f config.mak libtool $(turbodir)/simd/jsimdcfg.inc $(turbodir)/config.h.in~
	$(foreach DIR,$(CLEANDIRS),\
	rm -rf configure $(DIR)/autom4te.cache $(DIR)/m4 $(DIR)/build-aux ; \
	rm -f $(DIR)/aclocal.m4 $(DIR)/config.h.in $(DIR)/configure $(DIR)/Makefile.in $(DIR)/*/Makefile.in ;)

config.mak:
	./configure.sh

