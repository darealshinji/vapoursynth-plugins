
lib_LTLIBRARIES = libimagereader.la

libimagereader_la_SOURCES = \
			bmp.c \
			imagereader.c \
			jpeg.c \
			png.c \
			tga.c \
			writeframe.c

libimagereader_la_CFLAGS = $(AM_CFLAGS) -I../libjpeg-turbo

libimagereader_la_LIBADD = \
	../libjpeg-turbo/.libs/libturbojpeg.a \
	-ljpeg -lpng -lz
