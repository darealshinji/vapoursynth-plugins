include config.mak

SUBDIRS = \
	addgrain \
	bifrost \
	bilateral \
	combmask \
	convo2d \
	ctmf \
	d2vsource \
	damb \
	dctfilter \
	deblock \
	delogo \
	dfttest \
	eedi2 \
	fieldhint \
	fillborders

ifeq ($(shell uname -p),x86_64)
SUBDIRS += flash3kyuu_deband
endif

SUBDIRS += \
	fluxsmooth \
	fmtconv \
	genericfilters \
	histogram \
	imagereader \
	it \
	msmoosh \
	mvtools \
	nnedi3 \
	rawsource \
	retinex \
	sangnommod \
	scenechange \
	scrawl \
	scxvid \
	ssiq \
	tc2cfr \
	tcanny \
	tcomb \
	tdeintmod \
	templinearapproximate \
	temporalsoften \
	temporalsoften2 \
	vaguedenoiser \
	vautodeint \
	videoscope \
	wwxd \
	yadifmod \
	zimg

plugins = $(libdir)/vapoursynth/

ifeq ($(V), 1)
MAKE = make V=1
else
MAKE = make
endif

GREP ?= grep
install = install -m644 -D
install_DIR = install -m755 -d

define NL


endef


all:
	$(foreach DIR,$(SUBDIRS),$(MAKE) -C $(DIR) $(NL))
	$(MAKE) -C ffms2 || true

install:
	$(install_DIR) $(DESTDIR)$(plugins)
	$(install_DIR) $(DESTDIR)$(docdir)
	$(install_DIR) $(DESTDIR)$(prefix)/share/nnedi3

	$(foreach LIB,$(shell ls */*.so),$(install) $(LIB) $(DESTDIR)$(plugins) $(NL))
	$(foreach SCRIPT,$(shell ls */*.py),$(install) $(SCRIPT) $(DESTDIR)$(plugins) $(NL))

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
	$(foreach FILE,$(shell ls */readme* */README*), \
		$(install) $(FILE) $(DESTDIR)$(docdir)/$(shell echo $$(dirname $(FILE))) $(NL))

clean:
	$(foreach DIR,$(SUBDIRS),$(MAKE) -C $(DIR) clean || true $(NL))
	$(MAKE) -C ffms2 clean || true

distclean: clean
	$(foreach DIR,$(SUBDIRS),$(MAKE) -C $(DIR) distclean || true $(NL))
	$(MAKE) -C ffms2 distclean || true
	rm -f config.mak

config.mak:
	./configure

