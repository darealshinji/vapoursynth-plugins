include ../../config.mak

ifeq ($(V), 1)
verbose = V=1
else
verbose = V=0
endif

ifeq ($(HAVE_ZLIB),yes)
LIBNAME = ffms2
LIB = $(LIBNAME).so
endif


all: $(LIB)

ifeq ($(HAVE_ZLIB),yes)
$(LIB): ../../ffmpeg_libs/lib/ffmpeg_stamp
	$(MAKE) -f $(LIBNAME).mak $(verbose)

../../ffmpeg_libs/lib/ffmpeg_stamp:
	$(MAKE) -C ../.. -f ffmpeg.mak
endif

clean distclean:
	$(MAKE) -f $(LIBNAME).mak $@ $(verbose)

