include ../../config.mak

MAKE = make

ifeq ($(V), 1)
verbose = V=1
endif

LIBNAME = waifu2x-opt
LIB = $(LIBNAME).so


all: $(LIB)

$(LIB): libwaifu2x/libwaifu2x.a libzimg/libzimg.a
	$(MAKE) -f $(LIBNAME).mak $(verbose)

libwaifu2x/libwaifu2x.a:
	$(MAKE) -f libwaifu2x.mak

libzimg/libzimg.a:
	$(MAKE) -f libzimg.mak

clean distclean:
	$(MAKE) -f libwaifu2x.mak $@ $(verbose)
	$(MAKE) -f libzimg.mak $@ $(verbose)
	$(MAKE) -f $(LIBNAME).mak $@ $(verbose)

