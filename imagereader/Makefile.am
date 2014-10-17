include ../global.inc

AM_CFLAGS += -I./libjpeg-turbo

lib_LTLIBRARIES = libimagereader.la

libimagereader_la_SOURCES = \
	src/bmp.c \
	src/imagereader.c \
	src/jpeg.c \
	src/png.c \
	src/tga.c \
	src/writeframe.c

libimagereader_la_LDFLAGS = \
	$(XGLOBAL_LDFLAGS) \
	./libjpeg-turbo/.libs/libturbojpeg.a \
	-lpng -lz
