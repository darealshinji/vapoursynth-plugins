include config.mak

plugins = $(libdir)/vapoursynth/

ifeq ($(V), 1)
MAKE = make -f Makefile V=1
AUTORECONF = autoreconf -iv
else
MAKE = make -f Makefile
AUTORECONF = autoreconf -i
endif

GREP ?= grep
install = install -m644 -D
install_DIR = install -m755 -d

cleandirs = . ffms2 fluxsmooth mvtools nnedi3 tcomb imagereader/libjpeg-turbo

python_SCRIPTS = \
	finesharp/finesharp.py \
	havsfunc/havsfunc.py \
	maskdetail/MaskDetail.py \
	templinearapproximate/MCDenoise.py \
	temporalsoften2/TemporalSoften2.py \
	vapoursynth-modules/edgecleaner.py \
	vapoursynth-modules/rangeutils.py \
	vapoursynth-modules/scoll.py \
	vautodeint/VAutoDeint.py

define NL


endef


all: dirs
	test -f configure || ( $(AUTORECONF) && $(AUTORECONF) imagereader/libjpeg-turbo )
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
	$(install_DIR) $(DESTDIR)$(prefix)/share/nnedi3
	$(foreach LIB,$(shell echo */.libs/*.so),$(install) $(LIB) $(DESTDIR)$(plugins) $(NL))
	ffmslib=$$($(GREP) 'dlname' ffms2/src/core/libffms2.la | cut -d"'" -f2) ;\
		cp -f ffms2/src/core/.libs/$$ffmslib ffms2 ;\
		$(install) ffms2/$$ffmslib $(DESTDIR)$(plugins)
	$(foreach SCRIPT,$(python_SCRIPTS),$(install) $(SCRIPT) $(DESTDIR)$(plugins) $(NL))

	$(install) README.md $(DESTDIR)$(docdir)
	$(install) rawsource/format_list.txt $(DESTDIR)$(docdir)/rawsource_format_list
	$(install) flash3kyuu_deband/flash3kyuu_deband.txt $(DESTDIR)$(docdir)
	$(install) flash3kyuu_deband/flash3kyuu_deband.zh.txt $(DESTDIR)$(docdir)
	$(install) fmtconv/doc/fmtconv.html $(DESTDIR)$(docdir)
	$(install) fmtconv/doc/colorspace-subsampling.png $(DESTDIR)$(docdir)
	$(install) fmtconv/doc/vapourdoc.css $(DESTDIR)$(docdir)
	$(install) nnedi3/src/nnedi3_weights.bin $(DESTDIR)$(prefix)/share/nnedi3

	install -m755 -D d2vsource/d2vscan.pl $(DESTDIR)$(plugins)
	$(install) d2vsource/d2vscan.txt $(DESTDIR)$(docdir)/d2vscan
	$(foreach FILE,$(shell echo */readme* */README*), \
		$(install) $(FILE) $(DESTDIR)$(docdir)/$(shell echo $$(dirname $(FILE))) $(NL))

clean:
	test -f Makefile && $(MAKE) clean || true
	cd imagereader/src && rm -rf .libs *.la *.lo
	cd imagereader/libjpeg-turbo && rm -f md5/md5cmp stamp-h1 stamp-h2 tjbench tjbenchtest tjunittest
	rm -f ffms2/*.so.*

distclean: clean
	test -f Makefile && $(MAKE) distclean || true
	rm -f config.mak
	cd imagereader/libjpeg-turbo && rm -f config.h.in~ config.h jconfig.h libjpeg.map

cleanfiles = .dirstamp aclocal.m4 configure config.h.in config.h.in~ config.log \
	config.mak config.status jsimdcfg.inc libtool Makefile Makefile.in

clobber: distclean
	$(foreach DIR,$(cleandirs),rm -rf $(DIR)/autom4te.cache $(DIR)/m4 $(DIR)/build-aux ;)
	$(foreach FILE,$(cleanfiles),find . -name $(FILE) -delete ;)
	rm -rf .deps $(shell find . -name .deps)

config.mak:
	./configure.sh

