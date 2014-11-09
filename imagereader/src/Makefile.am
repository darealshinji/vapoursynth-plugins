include ../../global.inc

AM_CFLAGS += -I../libjpeg-turbo

lib_LTLIBRARIES = libimagereader.la

libimagereader_la_SOURCES = \
			bmp.c \
			imagereader.c \
			jpeg.c \
			png.c \
			tga.c \
			writeframe.c

libimagereader_la_LDFLAGS = $(XGLOBAL_LDFLAGS)

libimagereader_la_LIBADD = \
	../libjpeg-turbo/.libs/libturbojpeg.a \
	-ljpeg -lpng -lz
