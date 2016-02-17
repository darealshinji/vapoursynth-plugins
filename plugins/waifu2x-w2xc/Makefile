include ../../config.mak

MAKE = make

ifeq ($(V), 1)
verbose = V=1
endif

LIBNAME = waifu2x-w2xc
LIB = $(LIBNAME).so


ifeq ($(HAVE_OPENCV),yes)

all: $(LIB)

$(LIB): waifu2x/libw2xc.a
	$(MAKE) -f $(LIBNAME).mak $(verbose)

waifu2x/libw2xc.a:
	$(MAKE) -f libw2xc.mak

endif

clean distclean:
	$(MAKE) -f libw2xc.mak $@ $(verbose)
	$(MAKE) -f $(LIBNAME).mak $@ $(verbose)
	rm -rf libs

