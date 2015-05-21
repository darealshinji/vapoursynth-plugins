CXX  = g++
CC   = gcc
YASM = yasm

ifneq ($(V),1)
CXXLD_silent = @ echo '  CXXLD    '$@;
CXX_silent   = @ echo '  CXX      '$@;
CCLD_silent  = @ echo '  CCLD     '$@;
CC_silent    = @ echo '  CC       '$@;
YASM_silent  = @ echo '  YASM     '$@;
endif

#VapourSynth_CFLAGS = $(shell pkg-config --cflags vapoursynth)
VapourSynth_CFLAGS = -I/usr/include/vapoursynth

commonflags = -c -O2 -fPIC -Wall -Wextra -Wno-unused-parameter -Wno-unused-result -Isrc

VSCFLAGS = $(commonflags) $(local_CFLAGS) $(VapourSynth_CFLAGS) $(CFLAGS) $(CPPFLAGS)
VSCXXFLAGS = $(commonflags) -std=c++11 $(local_CXXFLAGS) $(VapourSynth_CFLAGS) $(CXXFLAGS) $(CPPFLAGS)
VSLDFLAGS = -shared -Wl,-soname,$(LIB) -Wl,-z,defs -Wl,-z,noexecstack -Wl,--as-needed $(LDFLAGS)

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

