include config.mak

OPTFLAGS = -O2
MAKE = make

ifeq ($(HAVE_X86_64),yes)
BITS = 64
else
BITS = 32
endif

ifeq ($(V), 1)
ffverbose = V=1
else
silent_config = @ echo 'CONFIGURE  ffmpeg';
silent_conf_out = >/dev/null
endif

STAMP = ffmpeg_libs/lib/ffmpeg_stamp


all: $(STAMP)

clean distclean:
	test ! -f ffmpeg/config.mak || $(MAKE) -C ffmpeg $@ $(ffverbose)
	rm -rf ffmpeg_libs

$(STAMP): ffmpeg/config.mak
	$(MAKE) -C ffmpeg $(ffverbose) && \
	$(MAKE) -C ffmpeg install $(ffverbose) && \
	touch $@

ffmpeg/config.mak:
	$(silent_config)cd ffmpeg && CFLAGS="-m$(BITS) -Wno-deprecated-declarations" \
	CPPFLAGS="" LDFLAGS="-m$(BITS)" \
	./configure --prefix="$(CURDIR)/ffmpeg_libs" \
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
		--disable-hwaccels \
		--disable-muxers \
		--disable-iconv \
		--disable-bzlib \
		--disable-lzo \
		--disable-lzma \
		--disable-zlib \
		--disable-sdl \
		--disable-xlib \
		$(silent_conf_out)

