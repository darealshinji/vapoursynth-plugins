CXX = g++
CC = gcc

#VapourSynth_CFLAGS = $(shell pkg-config --cflags vapoursynth)
VapourSynth_CFLAGS = -I/usr/local/include/vapoursynth -I/usr/include/vapoursynth

commonflags = -c -O2 -fPIC -Wall -Wextra -Wno-unused-parameter -Wno-unused-result -Isrc

VSCFLAGS = $(commonflags) $(CFLAGS) $(local_CFLAGS) $(VapourSynth_CFLAGS) $(CPPFLAGS)
VSCXXFLAGS = $(commonflags) $(CXXFLAGS) $(local_CXXFLAGS) $(VapourSynth_CFLAGS) $(CPPFLAGS)
VSLDFLAGS = -shared $(LDFLAGS) -Wl,-soname,$(LIB) -Wl,-z,noexecstack -Wl,--as-needed

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

clobber: distclean
	@ if [ "x`grep '^clobber-local:' Makefile`" != "x" ]; then $(MAKE) clobber-local; fi

