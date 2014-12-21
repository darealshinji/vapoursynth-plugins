include config.mak

plugins = $(libdir)/vapoursynth/

ifeq ($(V), 1)
MAKE = make -f Makefile V=1
AUTORECONF = autoreconf -iv
else
MAKE = make -f Makefile
AUTORECONF = autoreconf -i
endif

install = install -m644 -D
install_DIR = install -m755 -d

cleandirs = . fluxsmooth mvtools nnedi3 tcomb imagereader/libjpeg-turbo

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
	test -f Makefile  || ./configure
	$(MAKE)

all-am:
clean-am:
distclean-am:

dirs:
	$(foreach DIR,$(cleandirs),mkdir -p $(DIR)/m4 $(DIR)/build-aux ;)

install:
	$(install_DIR) $(DESTDIR)$(plugins)
	$(install_DIR) $(DESTDIR)$(docdir)
	$(foreach LIB,$(shell echo */.libs/*.so),$(install) $(LIB) $(DESTDIR)$(plugins) $(NL))
	$(foreach SCRIPT,$(python_SCRIPTS),$(install) $(SCRIPT) $(DESTDIR)$(plugins) $(NL))
	$(install) README.md $(DESTDIR)$(docdir)
	$(install) rawsource/format_list.txt $(DESTDIR)$(docdir)/rawsource_format_list
	install -m755 -D d2vsource/d2vscan.pl $(DESTDIR)$(plugins)
	$(install) d2vsource/d2vscan.txt $(DESTDIR)$(docdir)/d2vscan
	$(foreach FILE,$(shell echo */readme* */README*), \
		$(install) $(FILE) $(DESTDIR)$(docdir)/$(shell echo $$(dirname $(FILE))) $(NL))

clean:
	test -f Makefile && $(MAKE) clean || true

distclean:
	test -f Makefile && $(MAKE) distclean || true
	rm -f config.mak

cleanfiles = .dirstamp aclocal.m4 configure config.h.in config.h.in~ config.log \
	config.mak config.status jsimdcfg.inc libtool Makefile Makefile.in

clobber:
	test -f Makefile && $(MAKE) distclean || true
	$(foreach DIR,$(cleandirs),rm -rf $(DIR)/autom4te.cache $(DIR)/m4 $(DIR)/build-aux ;)
	$(foreach FILE,$(cleanfiles),find . -name $(FILE) -delete ;)
	rm -rf .deps $(shell find . -name .deps)

config.mak:
	./configure.sh

