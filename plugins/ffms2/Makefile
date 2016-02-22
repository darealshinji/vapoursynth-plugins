include ../../config.mak

ifeq ($(V), 1)
verbose = V=1
else
verbose = V=0
Q = @ 
silent_config = @ echo 'CONFIGURE  ffms2';
silent_conf_out = >/dev/null
endif

ifeq ($(HAVE_ZLIB),yes)
LIBNAME = ffms2
LIB = $(LIBNAME).so
endif

local_CXXFLAGS  = -Wall -Werror=format-security -I../../../include/vapoursynth
local_CXXFLAGS += -fPIC -fno-strict-aliasing -mfpmath=sse -msse2 -D_FORTIFY_SOURCE=2
local_CXXFLAGS += $(CXXFLAGS) $(CPPFLAGS)
local_LDFLAGS   = -Wl,-Bsymbolic -Wl,-z,relro -Wl,-z,noexecstack -Wl,--as-needed $(LDFLAGS)


all: $(LIB)

ifeq ($(HAVE_ZLIB),yes)
$(LIB): src/src/core/.libs/libffms2.so
	$(Q)cp -f $< $@ && chmod 0644 $@

src/src/core/.libs/libffms2.so: src/Makefile
	$(MAKE) -C src src/core/libffms2.la $(verbose)

src/Makefile:
	$(silent_config)cd src && \
	CXXFLAGS="$(local_CXXFLAGS)" \
	LDFLAGS="$(local_LDFLAGS)" \
	./configure --enable-shared --disable-static $(silent_conf_out)
endif

clean distclean:
	rm -f $(LIB)
	[ ! -f src/Makefile ] || $(MAKE) -C src $@ $(verbose)

