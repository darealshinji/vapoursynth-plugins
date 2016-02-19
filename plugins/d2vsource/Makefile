include ../../config.mak

ifeq ($(V), 1)
verbose = V=1
else
verbose = V=0
endif

LIBNAME = d2vsource
LIB = $(LIBNAME).so


all: $(LIB)

$(LIB): ../../ffmpeg_libs/lib/ffmpeg_stamp
	$(MAKE) -f $(LIBNAME).mak $(verbose)

../../ffmpeg_libs/lib/ffmpeg_stamp:
	$(MAKE) -C ../.. -f ffmpeg.mak

clean distclean:
	$(MAKE) -f $(LIBNAME).mak $@ $(verbose)

