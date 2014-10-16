include config.mak

plugins = $(libdir)/vapoursynth/
turbodir = imagereader/libjpeg-turbo/build

ifeq ($(V), 1)
MAKE = make -f Makefile V=1
AUTORECONF = autoreconf -iv
else
MAKE = make -f Makefile
AUTORECONF = autoreconf -i
endif

install = install -m644 -D
install_DIR = install -m755 -d

CLEANFILES = \
	aclocal.m4 \
	compile \
	config.h.in* \
	config.log \
	config.guess \
	config.sub \
	config.status \
	configure \
	depcomp \
	install-sh \
	libtool \
	ltmain.sh \
	m4/libtool.m4 \
	m4/lt~obsolete.m4 \
	m4/ltoptions.m4 \
	m4/ltsugar.m4 \
	m4/ltversion.m4 \
	Makefile \
	Makefile.in \
	missing

execstack_LIBS = fluxsmooth nnedi3 tcomb

python_SCRIPTS = \
	finesharp/finesharp.py \
    havsfunc/havsfunc.py \
	templinearapproximate/MCDenoise.py \
	temporalsoften2/TemporalSoften2.py \
	vautodeint/VAutoDeint.py

define NL


endef


all: libturbojpeg.a
	test -f configure || $(AUTORECONF)
	test -f Makefile  || ./configure
	$(MAKE)

libturbojpeg.a:
	mkdir -p $(turbodir)
	test -f $(turbodir)/configure || (cd $(turbodir) && $(AUTORECONF))
	test -f $(turbodir)/Makefile  || (cd $(turbodir) && \
	../configure --enable-static=yes --enable-shared=no --with-pic)
	cd $(turbodir) && $(MAKE)

all-am:
clean-am:
distclean-am:

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
	cd $(turbodir) && $(MAKE) clean

distclean:
	$(MAKE) distclean || true
	rm -f config.mak
	rm -rf $(turbodir)

clobber:
	$(MAKE) distclean || true
	rm -f config.mak $(CLEANFILES)
	rm -rf autom4te.cache */autom4te.cache $(turbodir) imagereader/libjpeg-turbo/autom4te.cache
	$(foreach FILE,$(CLEANFILES),rm -f */$(FILE) imagereader/libjpeg-turbo/$(FILE) \
	imagereader/libjpeg-turbo/*/$(FILE) ;)

config.mak:
	./build.sh

