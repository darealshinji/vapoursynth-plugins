
lib_LTLIBRARIES = liblsmashsource.la

liblsmashsource_la_SOURCES = \
		src/libavsmash_source.c \
		src/lsmashsource.c \
		src/lwlibav_source.c \
		src/video_output.c

liblsmashsource_la_CFLAGS = $(AM_CFLAGS) -std=gnu99 -msse2

liblsmashsource_la_LIBADD = -llsmash -lavformat -lavcodec -lswscale -lavutil

