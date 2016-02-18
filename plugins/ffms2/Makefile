include ../../config.mak

ifeq ($(HAVE_ZLIB),yes)

MAKE = make

ifeq ($(V), 1)
verbose = V=1
else
verbose = V=0
endif

LIBNAME = ffms2
LIB = $(LIBNAME).so


all: $(LIB)

$(LIB): ../../ffmpeg_libs/lib/ffmpeg_stamp
	$(MAKE) -f $(LIBNAME).mak $(verbose)

../../ffmpeg_libs/lib/ffmpeg_stamp:
	$(MAKE) -C ../.. -f ffmpeg.mak

clean distclean:
	$(MAKE) -f $(LIBNAME).mak $@ $(verbose)

endif

