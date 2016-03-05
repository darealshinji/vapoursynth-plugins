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

$(LIB): ../../ffmpeg_libs/lib/ffmpeg_stamp
	$(MAKE) -f $(LIBNAME).mk $(verbose)

../../ffmpeg_libs/lib/ffmpeg_stamp:
	$(MAKE) -C ../.. -f ffmpeg.mak

clean distclean:
	$(MAKE) -f $(LIBNAME).mk $@ $(verbose)
	rm -rf libs

