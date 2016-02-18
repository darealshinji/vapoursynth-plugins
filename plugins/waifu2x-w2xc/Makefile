include ../../config.mak

ifeq ($(V), 1)
verbose = V=1
endif

LIBNAME = waifu2x-w2xc


ifeq ($(HAVE_OPENCV),yes)
ifeq ($(HAVE_OPENCL),yes)

LIB = $(LIBNAME).so


all: $(LIB)

$(LIB): waifu2x/libw2xc.a
	$(MAKE) -f $(LIBNAME).mak $(verbose)

waifu2x/libw2xc.a:
	$(MAKE) -f libw2xc.mak

endif #HAVE_OPENCV
endif #HAVE_OPENCL

clean distclean:
	$(MAKE) -f libw2xc.mak $@ $(verbose)
	$(MAKE) -f $(LIBNAME).mak $@ $(verbose)
	rm -rf libs

