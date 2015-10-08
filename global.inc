include ../config.mak

YASM := yasm

ifneq ($(V),1)
CXXLD_silent  = @ echo '  CXXLD    '$@;
CXX_silent    = @ echo '  CXX      '$@;
CCLD_silent   = @ echo '  CCLD     '$@;
CC_silent     = @ echo '  CC       '$@;
YASM_silent   = @ echo '  YASM     '$@;
AR_silent     = @ echo '  AR       '$@;
RANLIB_silent = @ echo '  RANLIB   '$@;
endif

commonflags  = -c -Wall -Wextra -Wno-unused-parameter -Wno-unused-result -Werror=format-security
commonflags += -fPIC -fstack-protector-all -fno-strict-aliasing -mfpmath=sse -msse2
commonflags += -DVS_TARGET_CPU_X86 -D_FORTIFY_SOURCE=2
commonflags += -Isrc -I../include -I../include/vapoursynth

VSCFLAGS = $(commonflags) $(local_CFLAGS) $(CFLAGS) -std=gnu99 $(CPPFLAGS)
VSCXXFLAGS = $(commonflags) $(local_CXXFLAGS) $(CXXFLAGS) $(CPPFLAGS)
VSLDFLAGS = -shared -Wl,-soname,$(LIB) -Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,-z,noexecstack -Wl,--as-needed $(local_LDFLAGS) $(LDFLAGS)

CXXSRCS = $(shell find src -name \*.cpp)
CXXOBJS = $(CXXSRCS:%.cpp=%.o)

CSRCS = $(shell find src -name \*.c)
COBJS = $(CSRCS:%.c=%.o)


all: $(LIB)

clean:
	rm -f $(LIB) $(CXXOBJS) $(COBJS) $(CLEANFILES)
	@ if [ "x`grep '^clean-local:' Makefile`" != "x" ]; then $(MAKE) clean-local; fi

distclean: clean
	@ if [ "x`grep '^distclean-local:' Makefile`" != "x" ]; then $(MAKE) distclean-local; fi

../config.mak:
	cd .. && ./configure
