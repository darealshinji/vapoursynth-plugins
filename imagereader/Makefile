include ../config.mak

ifeq ($(HAVE_NASM),yes)
ifeq ($(HAVE_YASM),yes)
ifeq ($(HAVE_JPEG),yes)
ifeq ($(HAVE_PNG),yes)
ifeq ($(HAVE_ZLIB),yes)
IMAGEREADER = yes
endif #HAVE_ZLIB
endif #HAVE_PNG
endif #HAVE_JPEG
endif #HAVE_YASM
endif #HAVE_NASM


ifeq ($(V), 1)
MAKE = make V=1
AUTORECONF = autoreconf -iv
else
MAKE = make V=0
AUTORECONF = autoreconf -i
endif

turbodir = libjpeg-turbo
ifeq ($(IMAGEREADER),yes)
LIB = libimagereader.so
endif


all: $(LIB)

ifeq ($(IMAGEREADER),yes)

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
	./configure --build=$(BUILD) --host=$(HOST) --disable-silent-rules \
		--enable-static --disable-shared --with-pic --with-jpeg8

$(turbodir)/configure:
	mkdir -p $(turbodir)/m4
	$(AUTORECONF) $(turbodir)

endif #IMAGEREADER

clean:
	$(MAKE) -C $(turbodir) clean || true
	$(MAKE) -f imagereader.mk clean

distclean:
	$(MAKE) -C $(turbodir) distclean || true
	$(MAKE) -f imagereader.mk distclean

