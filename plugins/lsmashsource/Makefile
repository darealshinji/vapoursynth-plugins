include ../../config.mak

ifeq ($(HAVE_X86_64),yes)
BITS = 64
else
BITS = 32
endif

ifeq ($(V), 1)
verbose = V=1
else
verbose = V=0
silent_config = @ echo 'CONFIGURE  lsmash';
silent_conf_out = >/dev/null
endif

LIBNAME = lsmashsource
LIB = $(LIBNAME).so


all: $(LIB)

$(LIB): libs/lib/lsmash_stamp ../../ffmpeg_libs/lib/ffmpeg_stamp
	$(MAKE) -f $(LIBNAME).mk $(verbose)

../../ffmpeg_libs/lib/ffmpeg_stamp:
	$(MAKE) -C ../.. -f ffmpeg.mak

libs/lib/lsmash_stamp: lsmash/config.mak
	$(MAKE) -C lsmash && \
	$(MAKE) -C lsmash install && \
	touch $@

lsmash/config.mak:
	$(silent_config)cd lsmash && \
	./configure --prefix="$(CURDIR)/libs" \
		--extra-cflags="-m$(BITS) $(OPTFLAGS) -Werror=format-security -fPIC -DPIC -fstack-protector-all -fno-strict-aliasing -mfpmath=sse -msse2 -D_FORTIFY_SOURCE=2" \
		--extra-ldflags="-m$(BITS)" \
		$(silent_conf_out)

clean distclean:
	test ! -f lsmash/config.mak || $(MAKE) -C lsmash $@ || true
	$(MAKE) -f $(LIBNAME).mk $@ $(verbose)
	rm -rf libs

