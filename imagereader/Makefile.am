AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libimagereader.la

libimagereader_la_SOURCES = src/bmp.c src/imagereader.c src/jpeg.c src/png.c src/tga.c src/writeframe.c

libimagereader_la_LDFLAGS = -no-undefined -avoid-version

libimagereader_la_LIBS = -ljpeg -lpng -lz
