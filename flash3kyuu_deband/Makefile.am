waf_verbose   = $(waf_verbose_$(V))
waf_verbose_  = $(waf_verbose_$(AM_DEFAULT_VERBOSITY))
waf_verbose_1 = --verbose

all-local: clean-local
	./waf configure --mode=debug --shared --no-static --enable-vs --disable-avs $(waf_verbose)
	./waf build $(waf_verbose)
	mkdir -p .libs
	cp -f build/libf3kdb.so .libs

clean-local:
	./waf clean || true
	find . -type f -name *.pyc -delete
	rm -rf .libs

distclean-local: clean-local
	./waf distclean || true
	find . -type f -name *.pyc -delete

