include ../../config.mak

MAKE = make
OPTFLAGS = -O2

ifeq ($(HAVE_X86_64),yes)
BITS = 64
else
BITS = 32
endif

ifeq ($(V), 1)
verbose = V=1
ffverbose = V=1
else
verbose = V=0
ffverbose = 
endif

LIB = liblsmashsource.so


all: $(LIB)

liblsmashsource.so: libs/lib/lsmash_stamp libs/lib/ffmpeg_stamp
	$(MAKE) -f lsmashsource.mk $(verbose)

libs/lib/lsmash_stamp: lsmash/config.mak
	$(MAKE) -C lsmash && \
	$(MAKE) -C lsmash install && \
	touch $@

libs/lib/ffmpeg_stamp: ffmpeg/config.mak
	$(MAKE) -C ffmpeg $(ffverbose) && \
	$(MAKE) -C ffmpeg install $(ffverbose) && \
	touch $@

lsmash/config.mak:
	cd lsmash && ./configure \
		--prefix="$(CURDIR)/libs" \
		--extra-cflags="-m$(BITS) $(OPTFLAGS) -Werror=format-security -fPIC -DPIC -fstack-protector-all -fno-strict-aliasing -mfpmath=sse -msse2 -D_FORTIFY_SOURCE=2" \
		--extra-ldflags="-m$(BITS)"

ffmpeg/config.mak:
	cd ffmpeg && CFLAGS="-m$(BITS) -Wno-deprecated-declarations" \
	CPPFLAGS="" LDFLAGS="-m$(BITS)" \
	./configure --prefix="$(CURDIR)/libs" \
		--enable-pic \
		--disable-debug \
		--toolchain=hardened \
		--optflags="$(OPTFLAGS)" \
		--enable-avresample \
		--disable-avdevice \
		--disable-avfilter \
		--disable-postproc \
		--disable-swresample \
		--disable-programs \
		--disable-doc \
		--disable-dxva2 \
		--disable-vaapi \
		--disable-vda \
		--disable-vdpau \
		--disable-encoders \
		--disable-muxers \
		--disable-iconv \
		--disable-bzlib \
		--disable-lzo \
		--disable-lzma \
		--disable-zlib \
		--disable-sdl \
		--disable-xlib

clean distclean:
	test ! -f lsmash/config.mak || $(MAKE) -C lsmash $@ || true
	test ! -f ffmpeg/config.mak || $(MAKE) -C ffmpeg $@ $(ffverbose)
	$(MAKE) -f lsmashsource.mk $@ $(verbose)
	rm -rf libs

