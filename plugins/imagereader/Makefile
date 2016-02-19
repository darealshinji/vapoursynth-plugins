include ../../config.mak

ifeq ($(HAVE_NASM),yes)
ifeq ($(HAVE_YASM),yes)
ifeq ($(HAVE_PNG),yes)
ifeq ($(HAVE_ZLIB),yes)
IMAGEREADER = yes
endif #HAVE_ZLIB
endif #HAVE_PNG
endif #HAVE_YASM
endif #HAVE_NASM


ifeq ($(V), 1)
verbose = V=1
AUTORECONF = autoreconf -iv
SILENTRULES = --disable-silent-rules
else
verbose = V=0
AUTORECONF = autoreconf -i
SILENTRULES = --enable-silent-rules
silent_config = @ echo 'CONFIGURE  turbo-jpeg';
silent_conf_out = >/dev/null
endif

turbodir = libjpeg-turbo
ifeq ($(IMAGEREADER),yes)
LIBNAME = imagereader
LIB = $(LIBNAME).so
endif


all: $(LIB)

ifeq ($(IMAGEREADER),yes)

$(LIB): $(turbodir)/libturbojpeg.la
	$(MAKE) -f imagereader.mk $(verbose)

$(turbodir)/libturbojpeg.la: $(turbodir)/libjpeg.la
	$(MAKE) -C $(turbodir) libturbojpeg.la $(verbose)

$(turbodir)/libjpeg.la: $(turbodir)/simd/libsimd.la
	$(MAKE) -C $(turbodir) libjpeg.la $(verbose)

$(turbodir)/simd/libsimd.la: $(turbodir)/Makefile
	$(MAKE) -C $(turbodir)/simd $(verbose)

$(turbodir)/Makefile: $(turbodir)/configure
	$(silent_config)cd $(turbodir) && \
	CFLAGS="$(CFLAGS) -Wno-attributes -Wno-unused-but-set-variable" \
	CPPFLAGS="$(CPPFLAGS)" \
	./configure --build=$(BUILD) \
		--host=$(HOST) \
		$(SILENTRULES) \
		--enable-static \
		--disable-shared \
		--with-pic \
		--with-jpeg8 \
		$(silent_conf_out)

$(turbodir)/configure:
	mkdir -p $(turbodir)/m4
	$(AUTORECONF) $(turbodir)

endif #IMAGEREADER

clean:
	$(MAKE) -C $(turbodir) clean $(verbose) || true
	$(MAKE) -f imagereader.mk clean $(verbose)

distclean:
	$(MAKE) -C $(turbodir) distclean $(verbose) || true
	$(MAKE) -f imagereader.mk distclean $(verbose)

