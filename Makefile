include config.mak

SUBDIRS = \
	addgrain \
	bifrost \
	bilateral \
	bm3d \
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
	fillborders \
	flash3kyuu_deband \
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
	waifu2x \
	wwxd \
	yadifmod \
	zimg

plugins = $(libdir)/vapoursynth/
dist-packages := $(prefix)/lib/python3/dist-packages

ifeq ($(V), 1)
MAKE = make V=1
else
MAKE = make
endif

define NL


endef


all:
	$(foreach DIR,$(SUBDIRS),$(MAKE) -C $(DIR) $(NL))
	$(MAKE) -C ffms2 || true

install:
	$(INSTALL) -d $(DESTDIR)$(plugins)
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) -d $(DESTDIR)$(prefix)/share/nnedi3
	$(INSTALL) -d $(DESTDIR)$(dist-packages)

	$(foreach LIB,$(shell ls */*.so),$(INSTALL_DATA) $(LIB) $(DESTDIR)$(plugins) $(NL))
	$(foreach SCRIPT,$(shell ls */*.py),$(INSTALL_DATA) $(SCRIPT) $(DESTDIR)$(dist-packages) $(NL))

	$(INSTALL_DATA) README.md $(DESTDIR)$(docdir)
	$(INSTALL_DATA) rawsource/format_list.txt $(DESTDIR)$(docdir)/rawsource_format_list
	$(INSTALL_DATA) flash3kyuu_deband/flash3kyuu_deband.txt $(DESTDIR)$(docdir)
	$(INSTALL_DATA) flash3kyuu_deband/flash3kyuu_deband.zh.txt $(DESTDIR)$(docdir)
	$(INSTALL_DATA) fmtconv/doc/fmtconv.html $(DESTDIR)$(docdir)
	$(INSTALL_DATA) fmtconv/doc/colorspace-subsampling.png $(DESTDIR)$(docdir)
	$(INSTALL_DATA) fmtconv/doc/vapourdoc.css $(DESTDIR)$(docdir)
	$(INSTALL_DATA) nnedi3/src/nnedi3_weights.bin $(DESTDIR)$(prefix)/share/nnedi3
	$(INSTALL_DATA) waifu2x/models/noise1_model.json $(DESTDIR)$(plugins)
	$(INSTALL_DATA) waifu2x/models/noise2_model.json $(DESTDIR)$(plugins)
	$(INSTALL_DATA) waifu2x/models/scale2.0x_model.json $(DESTDIR)$(plugins)

	$(INSTALL) -m 755 d2vsource/d2vscan.pl $(DESTDIR)$(plugins)
	$(INSTALL_DATA) d2vsource/d2vscan.txt $(DESTDIR)$(docdir)/d2vscan
	$(foreach FILE,$(shell ls */readme* */README*), \
		$(INSTALL_DATA) $(FILE) $(DESTDIR)$(docdir)/$(shell echo $$(dirname $(FILE))) $(NL))

clean:
	$(foreach DIR,$(SUBDIRS),$(MAKE) -C $(DIR) clean || true $(NL))
	$(MAKE) -C ffms2 clean || true

distclean: clean
	$(foreach DIR,$(SUBDIRS),$(MAKE) -C $(DIR) distclean || true $(NL))
	$(MAKE) -C ffms2 distclean || true
	rm -f config.mak

config.mak:
	./configure

