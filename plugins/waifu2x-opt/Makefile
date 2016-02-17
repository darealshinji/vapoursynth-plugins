
LIB  = libwaifu2x-opt.so
ZIMG = libzimg.a
W2X  = waifu2x-opt/libwaifu2x.a

$(LIB): $(W2X) $(ZIMG)

local_CXXFLAGS = -Wno-unused-local-typedefs -Wno-return-type -Iwaifu2x-opt/include -I../zimg/src/API
local_LDFLAGS  = -fopenmp
LIBS = $(W2X) $(ZIMG)

clean-local:
	$(MAKE) -C ../zimg clean
	$(MAKE) -C waifu2x-opt clean || true
	rm -rf $(W2X) $(ZIMG)

$(W2X):
	$(MAKE) -C waifu2x-opt CXXFLAGS='$(CXXFLAGS)' CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS)'

$(ZIMG):
	$(MAKE) -C ../zimg
	$(AR_silent)ar cru $@ ../zimg/src/*/*.o
	$(RANLIB_silent)ranlib $@

include ../../cxx.inc

