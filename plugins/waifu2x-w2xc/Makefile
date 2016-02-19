include ../../config.mak

ifeq ($(V), 1)
verbose = V=1
endif

ifeq ($(HAVE_OPENCV),yes)
ifeq ($(HAVE_OPENCL),yes)
LIBNAME = waifu2x-w2xc
LIB = $(LIBNAME).so
endif
endif


all: $(LIB)

ifeq ($(HAVE_OPENCV),yes)
ifeq ($(HAVE_OPENCL),yes)
$(LIB): waifu2x/libw2xc.a
	$(MAKE) -f $(LIBNAME).mak $(verbose)

waifu2x/libw2xc.a:
	$(MAKE) -f libw2xc.mak
endif
endif

clean distclean:
	$(MAKE) -f libw2xc.mak $@ $(verbose)
	$(MAKE) -f $(LIBNAME).mak $@ $(verbose)
	rm -rf libs

